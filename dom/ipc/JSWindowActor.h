/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActor_h
#define mozilla_dom_JSWindowActor_h

#include "js/TypeDecls.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsRefPtrHashtable.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

enum class JSWindowActorMessageKind {
  Message,
  Query,
  QueryResolve,
  QueryReject,
  EndGuard_,
};

class JSWindowActorMessageMeta;
class QueryPromiseHandler;

// Common base class for JSWindowActor{Parent,Child}.
class JSWindowActor : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(JSWindowActor)

  JSWindowActor();

  enum class Type { Parent, Child };
  enum class CallbackFunction { WillDestroy, DidDestroy, ActorCreated };

  const nsString& Name() const { return mName; }

  void SendAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                        JS::Handle<JS::Value> aObj,
                        JS::Handle<JS::Value> aTransfers, ErrorResult& aRv);

  already_AddRefed<Promise> SendQuery(JSContext* aCx,
                                      const nsAString& aMessageName,
                                      JS::Handle<JS::Value> aObj,
                                      JS::Handle<JS::Value> aTransfers,
                                      ErrorResult& aRv);

  void ReceiveRawMessage(const JSWindowActorMessageMeta& aMetadata,
                         ipc::StructuredCloneData&& aData);

  virtual nsIGlobalObject* GetParentObject() const = 0;

  void RejectPendingQueries();

 protected:
  // Send the message described by the structured clone data |aData|, and the
  // message metadata |aMetadata|. The underlying transport should call the
  // |ReceiveMessage| method on the other side asynchronously.
  virtual void SendRawMessage(const JSWindowActorMessageMeta& aMetadata,
                              ipc::StructuredCloneData&& aData,
                              ErrorResult& aRv) = 0;

  virtual ~JSWindowActor() = default;

  void SetName(const nsAString& aName);

  void StartDestroy();

  void AfterDestroy();

  void InvokeCallback(CallbackFunction willDestroy);

 private:
  void ReceiveMessageOrQuery(JSContext* aCx,
                             const JSWindowActorMessageMeta& aMetadata,
                             JS::Handle<JS::Value> aData, ErrorResult& aRv);

  void ReceiveQueryReply(JSContext* aCx,
                         const JSWindowActorMessageMeta& aMetadata,
                         JS::Handle<JS::Value> aData, ErrorResult& aRv);

  // Helper object used while processing query messages to send the final reply
  // message.
  class QueryHandler final : public PromiseNativeHandler {
   public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(QueryHandler)

    QueryHandler(JSWindowActor* aActor,
                 const JSWindowActorMessageMeta& aMetadata);

    void RejectedCallback(JSContext* aCx,
                          JS::Handle<JS::Value> aValue) override;

    void ResolvedCallback(JSContext* aCx,
                          JS::Handle<JS::Value> aValue) override;

   private:
    ~QueryHandler() = default;

    void SendReply(JSContext* aCx, JSWindowActorMessageKind aKind,
                   ipc::StructuredCloneData&& aData);

    RefPtr<JSWindowActor> mActor;
    nsString mMessageName;
    uint64_t mQueryId;
  };

  nsString mName;
  nsRefPtrHashtable<nsUint64HashKey, Promise> mPendingQueries;
  uint64_t mNextQueryId;
};

}  // namespace dom
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::JSWindowActorMessageKind>
    : public ContiguousEnumSerializer<
          mozilla::dom::JSWindowActorMessageKind,
          mozilla::dom::JSWindowActorMessageKind::Message,
          mozilla::dom::JSWindowActorMessageKind::EndGuard_> {};

}  // namespace IPC

#endif  // !defined(mozilla_dom_JSWindowActor_h)
