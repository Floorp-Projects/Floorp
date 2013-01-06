/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_MiniShmParent_h
#define mozilla_plugins_MiniShmParent_h

#include "MiniShmBase.h"

#include <string>

namespace mozilla {
namespace plugins {

/**
 * This class provides a lightweight shared memory interface for a parent 
 * process in Win32.
 * This code assumes that there is a parent-child relationship between 
 * processes, as it creates inheritable handles.
 * Note that this class is *not* an IPDL actor.
 *
 * @see MiniShmChild
 */
class MiniShmParent : public MiniShmBase
{
public:
  MiniShmParent();
  virtual ~MiniShmParent();

  static const unsigned int kDefaultMiniShmSectionSize;

  /**
   * Initialize shared memory on the parent side.
   *
   * @param aObserver A MiniShmObserver object to receive event notifications.
   * @param aTimeout Timeout in milliseconds.
   * @param aSectionSize Desired size of the shared memory section. This is 
   *                     expected to be a multiple of 0x1000 (4KiB).
   * @return nsresult error code
   */
  nsresult
  Init(MiniShmObserver* aObserver, const DWORD aTimeout,
       const unsigned int aSectionSize = kDefaultMiniShmSectionSize);

  /**
   * Destroys the shared memory section. Useful to explicitly release 
   * resources if it is known that they won't be needed again.
   */
  void
  CleanUp();

  /**
   * Provides a cookie string that should be passed to MiniShmChild
   * during its initialization.
   *
   * @param aCookie A std::wstring variable to receive the cookie.
   * @return nsresult error code
   */
  nsresult
  GetCookie(std::wstring& aCookie);

  virtual nsresult
  Send() MOZ_OVERRIDE;

  bool
  IsConnected() const;

protected:
  void
  OnEvent() MOZ_OVERRIDE;

private:
  void
  FinalizeConnection();

  unsigned int mSectionSize;
  HANDLE mParentEvent;
  HANDLE mParentGuard;
  HANDLE mChildEvent;
  HANDLE mChildGuard;
  HANDLE mRegWait;
  HANDLE mFileMapping;
  LPVOID mView;
  bool   mIsConnected;
  DWORD  mTimeout;
  
  DISALLOW_COPY_AND_ASSIGN(MiniShmParent);
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_MiniShmParent_h

