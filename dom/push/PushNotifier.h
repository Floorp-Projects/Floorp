/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PushNotifier_h
#define mozilla_dom_PushNotifier_h

#include "nsIPushNotifier.h"

#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsString.h"

#include "mozilla/Maybe.h"

namespace mozilla {
namespace dom {

class ContentChild;
class ContentParent;

/**
 * `PushDispatcher` is a base class used to forward observer notifications and
 * service worker events to the correct process.
 */
class MOZ_STACK_CLASS PushDispatcher
{
public:
  // Fires an XPCOM observer notification. This method may be called from both
  // processes.
  virtual nsresult NotifyObservers() = 0;

  // Fires a service worker event. This method is called from the content
  // process if e10s is enabled, or the parent otherwise.
  virtual nsresult NotifyWorkers() = 0;

  // A convenience method that calls `NotifyObservers` and `NotifyWorkers`.
  nsresult NotifyObserversAndWorkers();

  // Sends an IPDL message to fire an observer notification in the parent
  // process. This method is only called from the content process, and only
  // if e10s is enabled.
  virtual bool SendToParent(ContentChild* aParentActor) = 0;

  // Sends an IPDL message to fire an observer notification and a service worker
  // event in the content process. This method is only called from the parent,
  // and only if e10s is enabled.
  virtual bool SendToChild(ContentParent* aContentActor) = 0;

  // An optional method, called from the parent if e10s is enabled and there
  // are no active content processes. The default behavior is a no-op.
  virtual nsresult HandleNoChildProcesses();

protected:
  PushDispatcher(const nsACString& aScope,
                 nsIPrincipal* aPrincipal);

  virtual ~PushDispatcher();

  bool ShouldNotifyWorkers();
  nsresult DoNotifyObservers(nsISupports *aSubject, const char *aTopic,
                             const nsACString& aScope);

  const nsCString mScope;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

/**
 * `PushNotifier` implements the `nsIPushNotifier` interface. This service
 * broadcasts XPCOM observer notifications for incoming push messages, then
 * forwards incoming push messages to service workers.
 *
 * All scriptable methods on this interface may be called from the parent or
 * content process. Observer notifications are broadcasted to both processes.
 */
class PushNotifier final : public nsIPushNotifier
{
public:
  PushNotifier();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PushNotifier, nsIPushNotifier)
  NS_DECL_NSIPUSHNOTIFIER

private:
  ~PushNotifier();

  nsresult Dispatch(PushDispatcher& aDispatcher);
};

/**
 * `PushData` provides methods for retrieving push message data in different
 * formats. This class is similar to the `PushMessageData` WebIDL interface.
 */
class PushData final : public nsIPushData
{
public:
  explicit PushData(const nsTArray<uint8_t>& aData);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PushData, nsIPushData)
  NS_DECL_NSIPUSHDATA

private:
  ~PushData();

  nsresult EnsureDecodedText();

  nsTArray<uint8_t> mData;
  nsString mDecodedText;
};

/**
 * `PushMessage` exposes the subscription principal and data for a push
 * message. Each `push-message` observer receives an instance of this class
 * as the subject.
 */
class PushMessage final : public nsIPushMessage
{
public:
  PushMessage(nsIPrincipal* aPrincipal, nsIPushData* aData);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(PushMessage, nsIPushMessage)
  NS_DECL_NSIPUSHMESSAGE

private:
  ~PushMessage();

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPushData> mData;
};

class PushMessageDispatcher final : public PushDispatcher
{
public:
  PushMessageDispatcher(const nsACString& aScope,
               nsIPrincipal* aPrincipal,
               const nsAString& aMessageId,
               const Maybe<nsTArray<uint8_t>>& aData);
  ~PushMessageDispatcher();

  nsresult NotifyObservers() override;
  nsresult NotifyWorkers() override;
  bool SendToParent(ContentChild* aParentActor) override;
  bool SendToChild(ContentParent* aContentActor) override;

private:
  const nsString mMessageId;
  const Maybe<nsTArray<uint8_t>> mData;
};

class PushSubscriptionChangeDispatcher final : public PushDispatcher
{
public:
  PushSubscriptionChangeDispatcher(const nsACString& aScope,
                                 nsIPrincipal* aPrincipal);
  ~PushSubscriptionChangeDispatcher();

  nsresult NotifyObservers() override;
  nsresult NotifyWorkers() override;
  bool SendToParent(ContentChild* aParentActor) override;
  bool SendToChild(ContentParent* aContentActor) override;
};

class PushSubscriptionModifiedDispatcher : public PushDispatcher
{
public:
  PushSubscriptionModifiedDispatcher(const nsACString& aScope,
                                     nsIPrincipal* aPrincipal);
  ~PushSubscriptionModifiedDispatcher();

  nsresult NotifyObservers() override;
  nsresult NotifyWorkers() override;
  bool SendToParent(ContentChild* aParentActor) override;
  bool SendToChild(ContentParent* aContentActor) override;
};

class PushErrorDispatcher final : public PushDispatcher
{
public:
  PushErrorDispatcher(const nsACString& aScope,
                      nsIPrincipal* aPrincipal,
                      const nsAString& aMessage,
                      uint32_t aFlags);
  ~PushErrorDispatcher();

  nsresult NotifyObservers() override;
  nsresult NotifyWorkers() override;
  bool SendToParent(ContentChild* aParentActor) override;
  bool SendToChild(ContentParent* aContentActor) override;

private:
  nsresult HandleNoChildProcesses() override;

  const nsString mMessage;
  uint32_t mFlags;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PushNotifier_h
