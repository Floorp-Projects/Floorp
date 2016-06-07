/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortList_h
#define mozilla_dom_MessagePortList_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/MessagePort.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class MessagePortList final : public nsISupports
                            , public nsWrapperCache
{
  ~MessagePortList() {}

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MessagePortList)

public:
  MessagePortList(nsISupports* aOwner,
                  const nsTArray<RefPtr<MessagePort>>& aPorts)
    : mOwner(aOwner)
    , mPorts(aPorts)
  {
  }

  nsISupports*
  GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t
  Length() const
  {
    return mPorts.Length();
  }

  MessagePort*
  Item(uint32_t aIndex)
  {
    return mPorts.SafeElementAt(aIndex);
  }

  MessagePort*
  IndexedGetter(uint32_t aIndex, bool &aFound)
  {
    aFound = aIndex < mPorts.Length();
    if (!aFound) {
      return nullptr;
    }
    return mPorts[aIndex];
  }

private:
  nsCOMPtr<nsISupports> mOwner;
  nsTArray<RefPtr<MessagePort>> mPorts;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePortList_h

