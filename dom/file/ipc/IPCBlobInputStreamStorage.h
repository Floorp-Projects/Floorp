/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_IPCBlobInputStreamStorage_h
#define mozilla_dom_ipc_IPCBlobInputStreamStorage_h

#include "mozilla/RefPtr.h"
#include "nsClassHashtable.h"
#include "nsISupportsImpl.h"

class nsIInputStream;
struct nsID;

namespace mozilla {
namespace dom {

class IPCBlobInputStreamParentCallback;

class IPCBlobInputStreamStorage final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IPCBlobInputStreamStorage);

  // This initializes the singleton and it must be called on the main-thread.
  static void
  Initialize();

  static IPCBlobInputStreamStorage*
  Get();

  void
  AddStream(nsIInputStream* aInputStream, const nsID& aID);

  void
  ForgetStream(const nsID& aID);

  void
  GetStream(const nsID& aID, nsIInputStream** aInputStream);

  void
  StoreCallback(const nsID& aID, IPCBlobInputStreamParentCallback* aCallback);

  already_AddRefed<IPCBlobInputStreamParentCallback>
  TakeCallback(const nsID& aID);

private:
  IPCBlobInputStreamStorage();
  ~IPCBlobInputStreamStorage();

  struct StreamData
  {
    nsCOMPtr<nsIInputStream> mInputStream;
    RefPtr<IPCBlobInputStreamParentCallback> mCallback;
  };

  nsClassHashtable<nsIDHashKey, StreamData> mStorage;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_IPCBlobInputStreamStorage_h
