/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BASE_PROCESS_TITLE_LINUX_H_
#define BASE_PROCESS_TITLE_LINUX_H_

void setproctitle(const char* fmt, ...);
void setproctitle_init(char** main_argv);

#endif  // BASE_PROCESS_TITLE_LINUX_H_
