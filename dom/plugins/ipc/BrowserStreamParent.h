/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_BrowserStreamParent_h
#define mozilla_plugins_BrowserStreamParent_h

#include "mozilla/plugins/PBrowserStreamParent.h"
#include "mozilla/plugins/AStream.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsPluginStreamListenerPeer.h"

namespace mozilla {
namespace plugins {

class PluginInstanceParent;

class BrowserStreamParent : public PBrowserStreamParent, public AStream
{
  friend class PluginModuleParent;
  friend class PluginInstanceParent;

public:
  BrowserStreamParent(PluginInstanceParent* npp,
                      NPStream* stream);
  virtual ~BrowserStreamParent();

  virtual bool IsBrowserStream() override { return true; }

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult AnswerNPN_RequestRead(const IPCByteRanges& ranges,
                                                        NPError* result) override;

  virtual mozilla::ipc::IPCResult RecvNPN_DestroyStream(const NPReason& reason) override;

  virtual mozilla::ipc::IPCResult RecvStreamDestroyed() override;

  int32_t WriteReady();
  int32_t Write(int32_t offset, int32_t len, void* buffer);
  void StreamAsFile(const char* fname);

  void NPP_DestroyStream(NPReason reason);

  void SetAlive()
  {
    if (mState == INITIALIZING) {
      mState = ALIVE;
    }
  }

private:
  using PBrowserStreamParent::SendNPP_DestroyStream;

  PluginInstanceParent* mNPP;
  NPStream* mStream;
  nsCOMPtr<nsISupports> mStreamPeer;
  RefPtr<nsNPAPIPluginStreamListener> mStreamListener;

  enum {
    INITIALIZING,
    DEFERRING_DESTROY,
    ALIVE,
    DYING,
    DELETING
  } mState;
};

} // namespace plugins
} // namespace mozilla

#endif
