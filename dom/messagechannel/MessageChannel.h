/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageChannel_h
#define mozilla_dom_MessageChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsIGlobalObject;

namespace mozilla {
class ErrorResult;

namespace dom {

class MessagePort;

class MessageChannel final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MessageChannel)

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<MessageChannel> Constructor(
      const GlobalObject& aGlobal, ErrorResult& aRv);

  static already_AddRefed<MessageChannel> Constructor(nsIGlobalObject* aGlobal,
                                                      ErrorResult& aRv);

  MessagePort* Port1() const { return mPort1; }

  MessagePort* Port2() const { return mPort2; }

 private:
  explicit MessageChannel(nsIGlobalObject* aGlobal);
  ~MessageChannel();

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<MessagePort> mPort1;
  RefPtr<MessagePort> mPort2;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MessageChannel_h
