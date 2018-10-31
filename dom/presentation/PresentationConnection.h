/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationConnection_h
#define mozilla_dom_PresentationConnection_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/PresentationConnectionBinding.h"
#include "mozilla/dom/PresentationConnectionCloseEventBinding.h"
#include "nsIPresentationListener.h"
#include "nsIRequest.h"
#include "nsIWeakReferenceUtils.h"

namespace mozilla {
namespace dom {

class Blob;
class PresentationConnectionList;

class PresentationConnection final : public DOMEventTargetHelper
                                   , public nsIPresentationSessionListener
                                   , public nsIRequest
                                   , public SupportsWeakPtr<PresentationConnection>
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationConnection,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER
  NS_DECL_NSIREQUEST
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(PresentationConnection)

  static already_AddRefed<PresentationConnection>
  Create(nsPIDOMWindowInner* aWindow,
         const nsAString& aId,
         const nsAString& aUrl,
         const uint8_t aRole,
         PresentationConnectionList* aList = nullptr);

  virtual void DisconnectFromOwner() override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  void GetId(nsAString& aId) const;

  void GetUrl(nsAString& aUrl) const;

  PresentationConnectionState State() const;

  PresentationConnectionBinaryType BinaryType() const;

  void SetBinaryType(PresentationConnectionBinaryType aType);

  void Send(const nsAString& aData,
            ErrorResult& aRv);

  void Send(Blob& aData,
            ErrorResult& aRv);

  void Send(const ArrayBuffer& aData,
            ErrorResult& aRv);

  void Send(const ArrayBufferView& aData,
            ErrorResult& aRv);

  void Close(ErrorResult& aRv);

  void Terminate(ErrorResult& aRv);

  bool
  Equals(uint64_t aWindowId, const nsAString& aId);

  IMPL_EVENT_HANDLER(connect);
  IMPL_EVENT_HANDLER(close);
  IMPL_EVENT_HANDLER(terminate);
  IMPL_EVENT_HANDLER(message);

private:
  PresentationConnection(nsPIDOMWindowInner* aWindow,
                         const nsAString& aId,
                         const nsAString& aUrl,
                         const uint8_t aRole,
                         PresentationConnectionList* aList);

  ~PresentationConnection();

  bool Init();

  void Shutdown();

  nsresult ProcessStateChanged(nsresult aReason);

  nsresult DispatchConnectionCloseEvent(PresentationConnectionClosedReason aReason,
                                         const nsAString& aMessage,
                                         bool aDispatchNow = false);

  nsresult DispatchMessageEvent(JS::Handle<JS::Value> aData);

  nsresult ProcessConnectionWentAway();

  nsresult AddIntoLoadGroup();

  nsresult RemoveFromLoadGroup();

  void AsyncCloseConnectionWithErrorMsg(const nsAString& aMessage);

  nsresult DoReceiveMessage(const nsACString& aData, bool aIsBinary);

  nsString mId;
  nsString mUrl;
  uint8_t mRole;
  PresentationConnectionState mState;
  RefPtr<PresentationConnectionList> mOwningConnectionList;
  nsWeakPtr mWeakLoadGroup;
  PresentationConnectionBinaryType mBinaryType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationConnection_h
