/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
#include "nsAutoPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class MessagePortList MOZ_FINAL : public nsISupports
                                , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MessagePortList)

public:
  MessagePortList(nsISupports* aOwner, nsTArray<nsRefPtr<MessagePortBase>>& aPorts)
    : mOwner(aOwner)
    , mPorts(aPorts)
  {
    SetIsDOMBinding();
  }

  nsISupports*
  GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  uint32_t
  Length() const
  {
    return mPorts.Length();
  }

  MessagePortBase*
  Item(uint32_t aIndex)
  {
    return mPorts.SafeElementAt(aIndex);
  }

  MessagePortBase*
  IndexedGetter(uint32_t aIndex, bool &aFound)
  {
    aFound = aIndex < mPorts.Length();
    if (!aFound) {
      return nullptr;
    }
    return mPorts[aIndex];
  }

public:
  nsCOMPtr<nsISupports> mOwner;
  nsTArray<nsRefPtr<MessagePortBase>> mPorts;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePortList_h

