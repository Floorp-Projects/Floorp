/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PushNotifier_h
#define mozilla_dom_PushNotifier_h

#include "nsIPushNotifier.h"

#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

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
  virtual ~PushNotifier();

  nsresult NotifyPush(const nsACString& aScope, nsIPrincipal* aPrincipal,
                      const nsAString& aMessageId,
                      const Maybe<nsTArray<uint8_t>>& aData);
  nsresult DoNotifyObservers(nsISupports *aSubject, const char *aTopic,
                             const nsACString& aScope);
  bool ShouldNotifyWorkers(nsIPrincipal* aPrincipal);
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
  virtual ~PushData();

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
  virtual ~PushMessage();

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPushData> mData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PushNotifier_h
