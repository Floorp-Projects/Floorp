/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/SetProcessTitle.h"

#include "nsString.h"

#ifdef XP_LINUX

#  include "base/set_process_title_linux.h"
#  define HAVE_SETPROCTITLE
#  define HAVE_SETPROCTITLE_INIT

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
    defined(__DragonFly__)

#  include <sys/types.h>
#  include <unistd.h>
#  define HAVE_SETPROCTITLE

#endif

namespace mozilla {

void SetProcessTitle(const std::vector<std::string>& aNewArgv) {
#ifdef HAVE_SETPROCTITLE
  nsAutoCStringN<1024> buf;

  bool firstArg = true;
  for (const std::string& arg : aNewArgv) {
    if (firstArg) {
      firstArg = false;
    } else {
      buf.Append(' ');
    }
    buf.Append(arg.c_str());
  }

  setproctitle("-%s", buf.get());
#endif
}

void SetProcessTitleInit(char** aOrigArgv) {
#ifdef HAVE_SETPROCTITLE_INIT
  setproctitle_init(aOrigArgv);
#endif
}

}  // namespace mozilla
