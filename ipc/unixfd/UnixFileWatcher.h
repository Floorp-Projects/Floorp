/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_UnixFileWatcher_h
#define mozilla_ipc_UnixFileWatcher_h

#include "UnixFdWatcher.h"

namespace mozilla {
namespace ipc {

class UnixFileWatcher : public UnixFdWatcher
{
public:
  virtual ~UnixFileWatcher();

  nsresult Open(const char* aFilename, int aFlags, mode_t aMode = 0);

  // Callback method for successful open requests
  virtual void OnOpened() {};

protected:
  UnixFileWatcher(MessageLoop* aIOLoop);
  UnixFileWatcher(MessageLoop* aIOLoop, int aFd);
};

}
}

#endif
