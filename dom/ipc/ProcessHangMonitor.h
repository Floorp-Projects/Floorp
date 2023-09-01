/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ProcessHangMonitor_h
#define mozilla_ProcessHangMonitor_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Atomics.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIRemoteTab.h"
#include "nsIThread.h"
#include "nsStringFwd.h"

class nsIRunnable;
class nsIBrowserChild;
class nsIThread;

namespace mozilla {

namespace dom {
class ContentParent;
class BrowserParent;
struct CancelContentJSOptions;
}  // namespace dom

class PProcessHangMonitorParent;

class ProcessHangMonitor final : public nsIObserver {
 private:
  ProcessHangMonitor();
  virtual ~ProcessHangMonitor();

 public:
  static ProcessHangMonitor* Get() { return sInstance; }
  static ProcessHangMonitor* GetOrCreate();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<PProcessHangMonitorParent> AddProcess(
      dom::ContentParent* aContentParent);
  static void RemoveProcess(PProcessHangMonitorParent* aParent);

  static void ClearHang();

  static void PaintWhileInterruptingJS(PProcessHangMonitorParent* aParent,
                                       dom::BrowserParent* aTab);

  static void UnloadLayersWhileInterruptingJS(
      PProcessHangMonitorParent* aParent, dom::BrowserParent* aTab);

  static void ClearPaintWhileInterruptingJS();
  static void MaybeStartPaintWhileInterruptingJS();

  static void CancelContentJSExecutionIfRunning(
      PProcessHangMonitorParent* aParent, dom::BrowserParent* aTab,
      nsIRemoteTab::NavigationType aNavigationType,
      const dom::CancelContentJSOptions& aCancelContentJSOptions);

  static void SetMainThreadQoSPriority(PProcessHangMonitorParent* aParent,
                                       nsIThread::QoSPriority aQoSPriority);

  enum SlowScriptAction {
    Continue,
    Terminate,
    StartDebugger,
  };
  SlowScriptAction NotifySlowScript(nsIBrowserChild* aBrowserChild,
                                    const char* aFileName,
                                    const nsString& aAddonId,
                                    const double aDuration);

  void NotifyPluginHang(uint32_t aPluginId);

  bool IsDebuggerStartupComplete();

  void InitiateCPOWTimeout();
  bool ShouldTimeOutCPOWs();

  nsresult Dispatch(already_AddRefed<nsIRunnable> aRunnable);
  bool IsOnThread();

 private:
  static ProcessHangMonitor* sInstance;

  Atomic<bool> mCPOWTimeout;

  nsCOMPtr<nsIThread> mThread;
};

}  // namespace mozilla

#endif  // mozilla_ProcessHangMonitor_h
