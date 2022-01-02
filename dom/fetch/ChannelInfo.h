/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChannelInfo_h
#define mozilla_dom_ChannelInfo_h

#include "nsString.h"
#include "nsCOMPtr.h"

class nsIChannel;
class nsIGlobalObject;
class nsIURI;

namespace mozilla {
namespace ipc {
class IPCChannelInfo;
}  // namespace ipc

namespace dom {

class Document;

// This class represents the information related to a Response that we
// retrieve from the corresponding channel that is used to perform the fetch.
//
// When adding new members to this object, the following code needs to be
// updated:
// * IPCChannelInfo
// * InitFromChannel and InitFromIPCChannelInfo members
// * ResurrectInfoOnChannel member
// * AsIPCChannelInfo member
// * constructors and assignment operators for this class.
// * DOM Cache schema code (in dom/cache/DBSchema.cpp) to ensure that the newly
//   added member is saved into the DB and loaded from it properly.
//
// Care must be taken when initializing this object, or when calling
// ResurrectInfoOnChannel().  This object cannot be initialized twice, and
// ResurrectInfoOnChannel() cannot be called on it before it has been
// initialized.  There are assertions ensuring these invariants.
class ChannelInfo final {
 public:
  typedef mozilla::ipc::IPCChannelInfo IPCChannelInfo;

  ChannelInfo() : mInited(false) {}

  ChannelInfo(const ChannelInfo& aRHS) = default;

  ChannelInfo& operator=(const ChannelInfo& aRHS) = default;

  void InitFromDocument(Document* aDoc);
  void InitFromChannel(nsIChannel* aChannel);
  void InitFromChromeGlobal(nsIGlobalObject* aGlobal);
  void InitFromIPCChannelInfo(const IPCChannelInfo& aChannelInfo);

  // This restores every possible information stored from a previous channel
  // object on a new one.
  nsresult ResurrectInfoOnChannel(nsIChannel* aChannel);

  bool IsInitialized() const { return mInited; }

  IPCChannelInfo AsIPCChannelInfo() const;

 private:
  void SetSecurityInfo(nsISupports* aSecurityInfo);

 private:
  nsCString mSecurityInfo;
  bool mInited;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ChannelInfo_h
