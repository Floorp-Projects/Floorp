/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPProcessParent_h
#define GMPProcessParent_h 1

#include "mozilla/Attributes.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/thread.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsIFile.h"

class nsIRunnable;

namespace mozilla::gmp {

class GMPProcessParent final : public mozilla::ipc::GeckoChildProcessHost {
 public:
  explicit GMPProcessParent(const std::string& aGMPPath);

  // Synchronously launch the plugin process. If the process fails to launch
  // after timeoutMs, this method will return false.
  bool Launch(int32_t aTimeoutMs);

  void Delete(nsCOMPtr<nsIRunnable> aCallback = nullptr);

  bool CanShutdown() override { return true; }
  const std::string& GetPluginFilePath() { return mGMPPath; }
  bool UseXPCOM() const { return mUseXpcom; }

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // Init static members on the main thread
  static void InitStaticMainThread();

  // Read prefs and environment variables to determine
  // when and if to start the Mac sandbox for the child
  // process. Starting the sandbox at launch is the new
  // preferred method. Code to support starting the sandbox
  // later at plugin start time should be removed once
  // starting at launch is stable and shipping.
  bool IsMacSandboxLaunchEnabled() override;

  // For process sandboxing purposes, set whether or not this
  // instance of the GMP process requires access to the macOS
  // window server. At present, Widevine requires window server
  // access, but OpenH264 decoding does not.
  void SetRequiresWindowServer(bool aRequiresWindowServer);

  // Return the sandbox type to be used with this process type.
  static MacSandboxType GetMacSandboxType() { return MacSandboxType_GMP; };
#endif

  using mozilla::ipc::GeckoChildProcessHost::GetChildProcessHandle;

 private:
  ~GMPProcessParent();

  void DoDelete();

  std::string mGMPPath;
  nsCOMPtr<nsIRunnable> mDeletedCallback;

  // Whether or not XPCOM is enabled in the GMP process.
  bool mUseXpcom;

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // Indicates whether we'll start the Mac GMP sandbox during
  // process launch (earlyinit) which is the new preferred method
  // or later in the process lifetime.
  static bool sLaunchWithMacSandbox;

  // Whether or not Mac sandbox violation logging is enabled.
  static bool sMacSandboxGMPLogging;

  // Override so we can set GMP-specific sandbox parameters
  bool FillMacSandboxInfo(MacSandboxInfo& aInfo) override;

  // Controls whether or not the sandbox will be configured with
  // window service access.
  bool mRequiresWindowServer;

#  if defined(DEBUG)
  // Used to assert InitStaticMainThread() is called before the constructor.
  static bool sIsMainThreadInitDone;
#  endif
#endif

  // For normalizing paths to be compatible with sandboxing.
  // We use normalized paths to generate the sandbox ruleset. Once
  // the sandbox has been started, resolving symlinks that point to
  // allowed directories could require reading paths not allowed by
  // the sandbox, so we should only attempt to load plugin libraries
  // using normalized paths.
  static nsresult NormalizePath(const char* aPath, PathString& aNormalizedPath);

  DISALLOW_COPY_AND_ASSIGN(GMPProcessParent);
};

}  // namespace mozilla::gmp

#endif  // ifndef GMPProcessParent_h
