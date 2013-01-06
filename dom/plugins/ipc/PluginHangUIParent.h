/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginHangUIParent_h
#define mozilla_plugins_PluginHangUIParent_h

#include "nsString.h"

#include "base/process.h"
#include "base/process_util.h"

#include "mozilla/plugins/PluginMessageUtils.h"

#include "MiniShmParent.h"

namespace mozilla {
namespace plugins {

class PluginModuleParent;

/**
 * This class is responsible for launching and communicating with the 
 * plugin-hang-ui process.
 *
 * NOTE: PluginHangUIParent is *not* an IPDL actor! In this case, "Parent" 
 *       is describing the fact that firefox is the parent process to the 
 *       plugin-hang-ui process, which is the PluginHangUIChild.
 *       PluginHangUIParent and PluginHangUIChild are a matched pair.
 * @see PluginHangUIChild
 */
class PluginHangUIParent : public MiniShmObserver
{
public:
  PluginHangUIParent(PluginModuleParent* aModule);
  virtual ~PluginHangUIParent();

  /**
   * Spawn the plugin-hang-ui.exe child process and terminate the given 
   * plugin container process if the user elects to stop the hung plugin.
   *
   * @param aPluginName Human-readable name of the affected plugin.
   * @return true if the plugin hang ui process was successfully launched,
   *         otherwise false.
   */
  bool
  Init(const nsString& aPluginName);

  /**
   * If the Plugin Hang UI is being shown, send a cancel notification to the 
   * Plugin Hang UI child process.
   *
   * @return true if the UI was shown and the cancel command was successfully
   *              sent to the child process, otherwise false.
   */
  bool
  Cancel();

  /**
   * Returns whether the Plugin Hang UI is currently being displayed.
   *
   * @return true if the Plugin Hang UI is showing, otherwise false.
   */
  bool
  IsShowing() const { return mIsShowing; }

  /**
   * Returns whether this Plugin Hang UI instance has been shown. Note 
   * that this does not necessarily mean that the UI is showing right now.
   *
   * @return true if the Plugin Hang UI has shown, otherwise false.
   */
  bool
  WasShown() const { return mIsShowing || mLastUserResponse != 0; }

  /**
   * Returns whether the user checked the "Don't ask me again" checkbox.
   *
   * @return true if the user does not want to see the Hang UI again.
   */
  bool
  DontShowAgain() const;

  /**
   * Returns whether the user clicked stop during the last time that the 
   * Plugin Hang UI was displayed, if applicable.
   *
   * @return true if the UI was shown and the user chose to stop the 
   *         plugin, otherwise false
   */
  bool
  WasLastHangStopped() const;

  /**
   * @return unsigned int containing the response bits from the last 
   * time the Plugin Hang UI ran.
   */
  unsigned int
  LastUserResponse() const { return mLastUserResponse; }

  /**
   * @return unsigned int containing the number of milliseconds that 
   * the Plugin Hang UI was displayed before the user responded.
   * Returns 0 if the Plugin Hang UI has not been shown or was cancelled.
   */
  unsigned int
  LastShowDurationMs() const;

  virtual void
  OnMiniShmEvent(MiniShmBase* aMiniShmObj) MOZ_OVERRIDE;

  virtual void
  OnMiniShmConnect(MiniShmBase* aMiniShmObj) MOZ_OVERRIDE;

private:
  nsresult
  GetHangUIOwnerWindowHandle(NativeWindowHandle& windowHandle);

  bool
  SendCancel();

  bool
  RecvUserResponse(const unsigned int& aResponse);

  static
  VOID CALLBACK SOnHangUIProcessExit(PVOID aContext, BOOLEAN aIsTimer);

private:
  PluginModuleParent* mModule;
  MessageLoop* mMainThreadMessageLoop;
  volatile bool mIsShowing;
  unsigned int mLastUserResponse;
  base::ProcessHandle mHangUIProcessHandle;
  NativeWindowHandle mMainWindowHandle;
  HANDLE mRegWait;
  HANDLE mShowEvent;
  DWORD mShowTicks;
  DWORD mResponseTicks;
  MiniShmParent mMiniShm;

  static const DWORD kTimeout;

  DISALLOW_COPY_AND_ASSIGN(PluginHangUIParent);
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginHangUIParent_h

