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
#include "nsCycleCollectionParticipant.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsQueryActorChild;
class nsQueryActorParent;

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

  const nsCString& Name() const { return mName; }

  void SendAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                        JS::Handle<JS::Value> aObj, ErrorResult& aRv);

  already_AddRefed<Promise> SendQuery(JSContext* aCx,
                                      const nsAString& aMessageName,
                                      JS::Handle<JS::Value> aObj,
                                      ErrorResult& aRv);

  void ReceiveRawMessage(const JSWindowActorMessageMeta& aMetadata,
                         ipc::StructuredCloneData&& aData,
                         ipc::StructuredCloneData&& aStack);

  virtual nsIGlobalObject* GetParentObject() const = 0;

  void RejectPendingQueries();

 protected:
  // Send the message described by the structured clone data |aData|, and the
  // message metadata |aMetadata|. The underlying transport should call the
  // |ReceiveMessage| method on the other side asynchronously.
  virtual void SendRawMessage(const JSWindowActorMessageMeta& aMetadata,
                              ipc::StructuredCloneData&& aData,
                              ipc::StructuredCloneData&& aStack,
                              ErrorResult& aRv) = 0;

  // Check if a message is so large that IPC will probably crash if we try to
  // send it. If it is too large, record telemetry about the message.
  static bool AllowMessage(const JSWindowActorMessageMeta& aMetadata,
                           size_t aDataLength);

  virtual ~JSWindowActor() = default;

  void SetName(const nsACString& aName);

  void StartDestroy();

  void AfterDestroy();

  void InvokeCallback(CallbackFunction willDestroy);

 private:
  friend class ::nsQueryActorChild;   // for QueryInterfaceActor
  friend class ::nsQueryActorParent;  // for QueryInterfaceActor

  nsresult QueryInterfaceActor(const nsIID& aIID, void** aPtr);

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
                 const JSWindowActorMessageMeta& aMetadata, Promise* aPromise);

    void RejectedCallback(JSContext* aCx,
                          JS::Handle<JS::Value> aValue) override;

    void ResolvedCallback(JSContext* aCx,
                          JS::Handle<JS::Value> aValue) override;

   private:
    ~QueryHandler() = default;

    void SendReply(JSContext* aCx, JSWindowActorMessageKind aKind,
                   ipc::StructuredCloneData&& aData);

    RefPtr<JSWindowActor> mActor;
    RefPtr<Promise> mPromise;
    nsString mMessageName;
    uint64_t mQueryId;
  };

  nsCOMPtr<nsISupports> mWrappedJS;
  nsCString mName;
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
