/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSActor_h
#define mozilla_dom_JSActor_h

#include "js/TypeDecls.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsQueryActorChild;
class nsQueryActorParent;

namespace mozilla {
namespace dom {

class JSActorManager;
class JSActorMessageMeta;
class QueryPromiseHandler;

enum class JSActorMessageKind {
  Message,
  Query,
  QueryResolve,
  QueryReject,
  EndGuard_,
};

// Common base class for JSWindowActor{Parent,Child}.
class JSActor : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(JSActor)

  explicit JSActor(nsISupports* aGlobal = nullptr);

  const nsCString& Name() const { return mName; }

  void SendAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                        JS::Handle<JS::Value> aObj, ErrorResult& aRv);

  already_AddRefed<Promise> SendQuery(JSContext* aCx,
                                      const nsAString& aMessageName,
                                      JS::Handle<JS::Value> aObj,
                                      ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const { return mGlobal; };

 protected:
  // Send the message described by the structured clone data |aData|, and the
  // message metadata |aMetadata|. The underlying transport should call the
  // |ReceiveMessage| method on the other side asynchronously.
  virtual void SendRawMessage(const JSActorMessageMeta& aMetadata,
                              ipc::StructuredCloneData&& aData,
                              ipc::StructuredCloneData&& aStack,
                              ErrorResult& aRv) = 0;

  // Check if a message is so large that IPC will probably crash if we try to
  // send it. If it is too large, record telemetry about the message.
  static bool AllowMessage(const JSActorMessageMeta& aMetadata,
                           size_t aDataLength);

  // Helper method to send an in-process raw message.
  using OtherSideCallback = std::function<already_AddRefed<JSActorManager>()>;
  static void SendRawMessageInProcess(const JSActorMessageMeta& aMeta,
                                      ipc::StructuredCloneData&& aData,
                                      ipc::StructuredCloneData&& aStack,
                                      OtherSideCallback&& aGetOtherSide);

  virtual ~JSActor() = default;

  void SetName(const nsACString& aName);

  bool CanSend() const { return mCanSend; }

  void StartDestroy();
  void AfterDestroy();

  enum class CallbackFunction { WillDestroy, DidDestroy, ActorCreated };
  void InvokeCallback(CallbackFunction willDestroy);

  virtual void ClearManager() = 0;

 private:
  friend class JSActorManager;
  friend class ::nsQueryActorChild;   // for QueryInterfaceActor
  friend class ::nsQueryActorParent;  // for QueryInterfaceActor

  nsresult QueryInterfaceActor(const nsIID& aIID, void** aPtr);

  // Called by JSActorManager when they receive raw message data destined for
  // this actor.
  void ReceiveMessageOrQuery(JSContext* aCx,
                             const JSActorMessageMeta& aMetadata,
                             JS::Handle<JS::Value> aData, ErrorResult& aRv);
  void ReceiveQueryReply(JSContext* aCx, const JSActorMessageMeta& aMetadata,
                         JS::Handle<JS::Value> aData, ErrorResult& aRv);

  // Helper object used while processing query messages to send the final reply
  // message.
  class QueryHandler final : public PromiseNativeHandler {
   public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(QueryHandler)

    QueryHandler(JSActor* aActor, const JSActorMessageMeta& aMetadata,
                 Promise* aPromise);

    void RejectedCallback(JSContext* aCx,
                          JS::Handle<JS::Value> aValue) override;

    void ResolvedCallback(JSContext* aCx,
                          JS::Handle<JS::Value> aValue) override;

   private:
    ~QueryHandler() = default;

    void SendReply(JSContext* aCx, JSActorMessageKind aKind,
                   ipc::StructuredCloneData&& aData);

    RefPtr<JSActor> mActor;
    RefPtr<Promise> mPromise;
    nsString mMessageName;
    uint64_t mQueryId;
  };

  // A query which hasn't been resolved yet, along with metadata about what
  // query the promise is for.
  struct PendingQuery {
    RefPtr<Promise> mPromise;
    nsString mMessageName;
  };

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<nsISupports> mWrappedJS;
  nsCString mName;
  nsDataHashtable<nsUint64HashKey, PendingQuery> mPendingQueries;
  uint64_t mNextQueryId = 0;
  bool mCanSend = true;
};

}  // namespace dom
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::JSActorMessageKind>
    : public ContiguousEnumSerializer<
          mozilla::dom::JSActorMessageKind,
          mozilla::dom::JSActorMessageKind::Message,
          mozilla::dom::JSActorMessageKind::EndGuard_> {};

}  // namespace IPC

#endif  // !defined(mozilla_dom_JSActor_h)
