/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_MiniShmChild_h
#define mozilla_plugins_MiniShmChild_h

#include "MiniShmBase.h"

#include <string>

namespace mozilla {
namespace plugins {

/**
 * This class provides a lightweight shared memory interface for a child 
 * process in Win32.
 * This code assumes that there is a parent-child relationship between 
 * processes, as it inherits handles from the parent process.
 * Note that this class is *not* an IPDL actor.
 *
 * @see MiniShmParent
 */
class MiniShmChild : public MiniShmBase
{
public:
  MiniShmChild();
  virtual ~MiniShmChild();

  /**
   * Initialize shared memory on the child side.
   *
   * @param aObserver A MiniShmObserver object to receive event notifications.
   * @param aCookie Cookie obtained from MiniShmParent::GetCookie
   * @param aTimeout Timeout in milliseconds.
   * @return nsresult error code
   */
  nsresult
  Init(MiniShmObserver* aObserver, const std::wstring& aCookie,
       const DWORD aTimeout);

  virtual nsresult
  Send() override;

protected:
  void
  OnEvent() override;

private:
  HANDLE mParentEvent;
  HANDLE mParentGuard;
  HANDLE mChildEvent;
  HANDLE mChildGuard;
  HANDLE mFileMapping;
  HANDLE mRegWait;
  LPVOID mView;
  DWORD  mTimeout;

  DISALLOW_COPY_AND_ASSIGN(MiniShmChild);
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_MiniShmChild_h

