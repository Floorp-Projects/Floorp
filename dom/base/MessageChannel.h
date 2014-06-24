
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageChannel_h
#define mozilla_dom_MessageChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class MessagePort;

class MessageChannel MOZ_FINAL : public nsISupports
                               , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MessageChannel)

  static bool Enabled(JSContext* aCx, JSObject* aGlobal);

public:
  MessageChannel(nsPIDOMWindow* aWindow);

  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<MessageChannel>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  MessagePort*
  Port1() const
  {
    return mPort1;
  }

  MessagePort*
  Port2() const
  {
    return mPort2;
  }

private:
  ~MessageChannel();

  nsCOMPtr<nsPIDOMWindow> mWindow;

  nsRefPtr<MessagePort> mPort1;
  nsRefPtr<MessagePort> mPort2;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessageChannel_h
