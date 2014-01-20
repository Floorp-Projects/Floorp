/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=2 et : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_StreamNotifyChild_h
#define mozilla_plugins_StreamNotifyChild_h

#include "mozilla/plugins/PStreamNotifyChild.h"

namespace mozilla {
namespace plugins {

class BrowserStreamChild;

class StreamNotifyChild : public PStreamNotifyChild
{
  friend class PluginInstanceChild;
  friend class BrowserStreamChild;

public:
  StreamNotifyChild(const nsCString& aURL)
    : mURL(aURL)
    , mClosure(nullptr)
    , mBrowserStream(nullptr)
  { }

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  void SetValid(void* aClosure) {
    mClosure = aClosure;
  }

  void NPP_URLNotify(NPReason reason);

private:
  virtual bool Recv__delete__(const NPReason& reason) MOZ_OVERRIDE;

  bool RecvRedirectNotify(const nsCString& url, const int32_t& status) MOZ_OVERRIDE;

  /**
   * If a stream is created for this this URLNotify, we associate the objects
   * so that the NPP_URLNotify call is not fired before the stream data is
   * completely delivered. The BrowserStreamChild takes responsibility for
   * calling NPP_URLNotify and deleting this object.
   */
  void SetAssociatedStream(BrowserStreamChild* bs);

  nsCString mURL;
  void* mClosure;

  /**
   * If mBrowserStream is true, it is responsible for deleting this C++ object
   * and DeallocPStreamNotify is not, so that the delayed delivery of
   * NPP_URLNotify is possible.
   */
  BrowserStreamChild* mBrowserStream;
};

} // namespace plugins
} // namespace mozilla

#endif
