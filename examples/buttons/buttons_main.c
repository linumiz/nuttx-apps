/****************************************************************************
 * apps/examples/buttons/buttons_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/input/buttons.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_INPUT_BUTTONS
#  error "CONFIG_INPUT_BUTTONS is not defined in the configuration"
#endif

#ifndef CONFIG_INPUT_BUTTONS_NPOLLWAITERS
#  define CONFIG_INPUT_BUTTONS_NPOLLWAITERS 2
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_SIGNO
#  define CONFIG_EXAMPLES_BUTTONS_SIGNO 32
#endif

#ifndef CONFIG_INPUT_BUTTONS_POLL_DELAY
#  define CONFIG_INPUT_BUTTONS_POLL_DELAY 1000
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME0
#  define CONFIG_EXAMPLES_BUTTONS_NAME0 "BUTTON0"
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME1
#  define CONFIG_EXAMPLES_BUTTONS_NAME1 "BUTTON1"
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME2
#  define CONFIG_EXAMPLES_BUTTONS_NAME2 "BUTTON2"
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME3
#  define CONFIG_EXAMPLES_BUTTONS_NAME3 "BUTTON3"
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME4
#  define CONFIG_EXAMPLES_BUTTONS_NAME4 "BUTTON4"
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME5
#  define CONFIG_EXAMPLES_BUTTONS_NAME5 "BUTTON5"
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME6
#  define CONFIG_EXAMPLES_BUTTONS_NAME6 "BUTTON6"
#endif

#ifndef CONFIG_EXAMPLES_BUTTONS_NAME7
#  define CONFIG_EXAMPLES_BUTTONS_NAME7 "BUTTON7"
#endif

#define BUTTON_MAX 8

#ifndef CONFIG_EXAMPLES_BUTTONS_QTD
#  define CONFIG_EXAMPLES_BUTTONS_QTD BUTTON_MAX
#endif

#if CONFIG_EXAMPLES_BUTTONS_QTD > 8
#  error "CONFIG_EXAMPLES_BUTTONS_QTD > 8"
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_EXAMPLES_BUTTONS_NAMES
static const char button_name[CONFIG_EXAMPLES_BUTTONS_QTD][16] =
{
  CONFIG_EXAMPLES_BUTTONS_NAME0
#if CONFIG_EXAMPLES_BUTTONS_QTD > 1
  , CONFIG_EXAMPLES_BUTTONS_NAME1
#endif
#if CONFIG_EXAMPLES_BUTTONS_QTD > 2
  , CONFIG_EXAMPLES_BUTTONS_NAME2
#endif
#if CONFIG_EXAMPLES_BUTTONS_QTD > 3
  , CONFIG_EXAMPLES_BUTTONS_NAME3
#endif
#if CONFIG_EXAMPLES_BUTTONS_QTD > 4
  , CONFIG_EXAMPLES_BUTTONS_NAME4
#endif
#if CONFIG_EXAMPLES_BUTTONS_QTD > 5
  , CONFIG_EXAMPLES_BUTTONS_NAME5
#endif
#if CONFIG_EXAMPLES_BUTTONS_QTD > 6
  , CONFIG_EXAMPLES_BUTTONS_NAME6
#endif
#if CONFIG_EXAMPLES_BUTTONS_QTD > 7
  , CONFIG_EXAMPLES_BUTTONS_NAME7
#endif
};
#endif

static bool g_button_daemon_started;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: process_sample
 *
 * Description:
 *   Process button sample and detect press/release transitions.
 *
 * Input Parameters:
 *   sample    - Current button state bitmask
 *   oldsample - Previous button state bitmask
 *
 ****************************************************************************/

static void process_sample(btn_buttonset_t sample,
                           btn_buttonset_t *oldsample)
{
#ifdef CONFIG_EXAMPLES_BUTTONS_NAMES
  int i;

  /* Iterate through the configured number of buttons */

  for (i = 0; i < CONFIG_EXAMPLES_BUTTONS_QTD; i++)
    {
      if ((sample & (1 << i)) && !((*oldsample) & (1 << i)))
        {
          printf("%s was pressed\n", button_name[i]);
        }

      if (!(sample & (1 << i)) && ((*oldsample) & (1 << i)))
        {
          printf("%s was released\n", button_name[i]);
        }
    }
#else

  if (sample != *oldsample)
    {
      printf("Sample = %jd\n", (intmax_t)sample);
    }

#endif
  fflush(stdout);
  usleep(1000);
  *oldsample = sample;
}

/****************************************************************************
 * Name: button_daemon
 ****************************************************************************/

static int button_daemon(int argc, char *argv[])
{
#ifdef CONFIG_EXAMPLES_BUTTONS_POLL
  struct pollfd fds[1];
#endif

#ifdef CONFIG_EXAMPLES_BUTTONS_SIGNAL
  struct btn_notify_s btnevents;
#endif

  btn_buttonset_t supported;
  btn_buttonset_t sample = 0;

  btn_buttonset_t oldsample = 0;

  int ret;
  int fd;
  int i;

  UNUSED(i);

  /* Indicate that we are running */

  g_button_daemon_started = true;
  printf("button_daemon: Running\n");

  /* Open the BUTTON driver */

  printf("button_daemon: Opening %s\n", CONFIG_EXAMPLES_BUTTONS_DEVPATH);
  fd = open(CONFIG_EXAMPLES_BUTTONS_DEVPATH, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      int errcode = errno;
      printf("button_daemon: ERROR: Failed to open %s: %d\n",
             CONFIG_EXAMPLES_BUTTONS_DEVPATH, errcode);
      goto errout;
    }

  /* Get the set of BUTTONs supported */

  ret = ioctl(fd, BTNIOC_SUPPORTED,
              (unsigned long)((uintptr_t)&supported));
  if (ret < 0)
    {
      int errcode = errno;
      printf("button_daemon: ERROR: ioctl(BTNIOC_SUPPORTED) failed: %d\n",
             errcode);
      goto errout_with_fd;
    }

  printf("button_daemon: Supported BUTTONs 0x%02x\n",
         (unsigned int)supported);

#ifdef CONFIG_EXAMPLES_BUTTONS_SIGNAL
  /* Define the notifications events */

  btnevents.bn_press = supported;
  btnevents.bn_release = supported;

  btnevents.bn_event.sigev_notify = SIGEV_SIGNAL;
  btnevents.bn_event.sigev_signo = CONFIG_EXAMPLES_BUTTONS_SIGNO;

  /* Register to receive a signal when buttons are pressed/released */

  ret = ioctl(fd, BTNIOC_REGISTER,
              (unsigned long)((uintptr_t)&btnevents));
  if (ret < 0)
    {
      int errcode = errno;
      printf("button_daemon: ERROR: ioctl(BTNIOC_SUPPORTED) failed: %d\n",
             errcode);
      goto errout_with_fd;
    }

  /* Ignore the default signal action */

  signal(CONFIG_EXAMPLES_BUTTONS_SIGNO, SIG_IGN);

#endif

  /* Now loop forever, waiting BUTTONs events */

  for (; ; )
    {
#ifdef CONFIG_EXAMPLES_BUTTONS_SIGNAL
      struct siginfo value;
      sigset_t set;
#endif
#ifdef CONFIG_EXAMPLES_BUTTONS_POLL
      int nbytes;
#endif
#ifdef CONFIG_EXAMPLES_BUTTONS_SIGNAL
      /* Wait for a signal */

      sigemptyset(&set);
      sigaddset(&set, CONFIG_EXAMPLES_BUTTONS_SIGNO);
      ret = sigwaitinfo(&set, &value);
      if (ret < 0)
        {
          int errcode = errno;
          printf("button_daemon: ERROR: sigwaitinfo() failed: %d\n",
                 errcode);
          goto errout_with_fd;
        }

      sample = (btn_buttonset_t)value.si_value.sival_int;
      process_sample(sample, &oldsample);

#endif
#ifdef CONFIG_EXAMPLES_BUTTONS_POLL
      /* Prepare the File Descriptor for poll */

      memset(fds, 0, sizeof(fds));

      fds[0].fd      = fd;
      fds[0].events  = POLLIN;

      ret = poll(fds, 1, CONFIG_INPUT_BUTTONS_POLL_DELAY);

      printf("\nbutton_daemon: poll returned: %d\n", ret);
      if (ret < 0)
        {
          int errcode = errno;
          printf("button_daemon: ERROR poll failed: %d\n", errcode);
          continue;
        }
      else if (ret == 0)
        {
          printf("button_daemon: Timeout\n");
          continue;
        }
      else if (ret > CONFIG_INPUT_BUTTONS_NPOLLWAITERS)
        {
          printf("button_daemon: ERROR poll reported: %d\n", errno);
        }

      if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
        {
          printf("button_daemon: poll() error revents=0x%lx\n",
                 fds[0].revents);
          goto errout_with_fd;
        }

      if (!(fds[0].revents & POLLIN))
        {
          /* Spurious wakeup? */

          continue;
        }

      /* In any event, read until the pipe is empty */

      do
        {
          nbytes = read(fds[0].fd, (void *)&sample, sizeof(btn_buttonset_t));

          if (nbytes < 0)
            {
              if (errno == EAGAIN || errno == EINTR)
                {
                  break;
                }

              printf("button_daemon: read failed: %d\n", errno);
              goto errout_with_fd;
            }
          else if (nbytes == 0)
            {
              printf("button_daemon: No data read\n");
              break;
            }

          process_sample(sample, &oldsample);
        }
      while (1);

#endif
    }

errout_with_fd:
  close(fd);

errout:
  g_button_daemon_started = false;
  printf("button_daemon: Terminating\n");
  return EXIT_FAILURE;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * buttons_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;

  printf("buttons_main: Starting the button_daemon\n");
  if (g_button_daemon_started)
    {
      printf("buttons_main: button_daemon already running\n");
      return EXIT_SUCCESS;
    }

  ret = task_create("button_daemon", CONFIG_EXAMPLES_BUTTONS_PRIORITY,
                    CONFIG_EXAMPLES_BUTTONS_STACKSIZE, button_daemon,
                    NULL);

  if (ret < 0)
    {
      int errcode = errno;
      printf("buttons_main: ERROR: Failed to start button_daemon: %d\n",
             errcode);
      return EXIT_FAILURE;
    }

  printf("buttons_main: button_daemon started\n");
  return EXIT_SUCCESS;
}
