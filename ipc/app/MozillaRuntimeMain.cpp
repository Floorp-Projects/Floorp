/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../contentproc/plugin-container.cpp"

#include "mozilla/Bootstrap.h"
#include "mozilla/WindowsDllBlocklist.h"

using namespace mozilla;

int main(int argc, char* argv[]) {
#ifdef HAS_DLL_BLOCKLIST
  DllBlocklist_Initialize(eDllBlocklistInitFlagIsChildProcess);
#endif

  Bootstrap::UniquePtr bootstrap = GetBootstrap();
  if (!bootstrap) {
    return 2;
  }
  int ret = content_process_main(bootstrap.get(), argc, argv);
#if defined(DEBUG) && defined(HAS_DLL_BLOCKLIST)
  DllBlocklist_Shutdown();
#endif
  return ret;
}
