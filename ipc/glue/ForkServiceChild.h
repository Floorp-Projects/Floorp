/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __FORKSERVICE_CHILD_H_
#define __FORKSERVICE_CHILD_H_

#include "base/process_util.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "mozilla/ipc/MiniTransceiver.h"
#include "mozilla/ipc/LaunchError.h"
#include "mozilla/Result.h"

#include <sys/types.h>
#include <poll.h>

namespace mozilla {
namespace ipc {

class GeckoChildProcessHost;

/**
 * This is the interface to the fork server.
 *
 * When the chrome process calls |ForkServiceChild| to create a new
 * process, this class send a message to the fork server through a
 * pipe and get the PID of the new process from the reply.
 */
class ForkServiceChild {
 public:
  ForkServiceChild(int aFd, GeckoChildProcessHost* aProcess);
  virtual ~ForkServiceChild();

  struct Args {
#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
    int mForkFlags = 0;
    bool mChroot = false;
#endif
    nsTArray<nsCString> mArgv;
    nsTArray<EnvVar> mEnv;
    nsTArray<FdMapping> mFdsRemap;
  };

  /**
   * Ask the fork server to create a new process with given parameters.
   *
   * The fork server uses |base::LaunchApp()| to create a new
   * content process with the following parameters.
   *
   * \param aArgv assigns |argv| of the content process.
   * \param aEnvMap sets |LaunchOptions::env_map|.
   * \param aFdsRemap sets |LaunchOptions::fd_to_remap|.
   * \param aPid returns the PID of the content process created.
   * \return true if success.
   */
  Result<Ok, LaunchError> SendForkNewSubprocess(const Args& aArgs, pid_t* aPid);

  /**
   * Create a fork server process and the singleton of this class.
   *
   * This function uses |GeckoChildProcessHost| to launch the fork
   * server, getting the fd of a pipe/socket to the fork server from
   * it's |IPC::Channel|.
   */
  static void StartForkServer();
  static void StopForkServer();
  /**
   * Return the singleton.
   */
  static ForkServiceChild* Get() {
    auto child = sForkServiceChild.get();
    return child == nullptr || child->mFailed ? nullptr : child;
  }

 private:
  // Called when a message is received.
  void OnMessageReceived(UniquePtr<IPC::Message> message);
  void OnError();

  UniquePtr<MiniTransceiver> mTcver;
  static UniquePtr<ForkServiceChild> sForkServiceChild;
  pid_t mRecvPid;
  bool mFailed;  // The forkserver has crashed or disconnected.
  GeckoChildProcessHost* mProcess;
};

/**
 * Start a fork server at |xpcom-startup| from the chrome process.
 */
class ForkServerLauncher : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  ForkServerLauncher();
  static already_AddRefed<ForkServerLauncher> Create();

 private:
  friend class ForkServiceChild;
  virtual ~ForkServerLauncher();

  static void RestartForkServer();

  static bool mHaveStartedClient;
  static StaticRefPtr<ForkServerLauncher> mSingleton;
};

}  // namespace ipc
}  // namespace mozilla

#endif /* __FORKSERVICE_CHILD_H_ */
