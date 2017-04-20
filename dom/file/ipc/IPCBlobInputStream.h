/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_IPCBlobInputStream_h
#define mozilla_dom_ipc_IPCBlobInputStream_h

#include "nsIInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"

namespace mozilla {
namespace dom {

class IPCBlobInputStreamChild;

class IPCBlobInputStream final : public nsIInputStream
                               , public nsICloneableInputStream
                               , public nsIIPCSerializableInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

  explicit IPCBlobInputStream(IPCBlobInputStreamChild* aActor);

private:
  ~IPCBlobInputStream();

  RefPtr<IPCBlobInputStreamChild> mActor;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ipc_IPCBlobInputStream_h
