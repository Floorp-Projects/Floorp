/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupportsPrimitives.h"
#include "nsSpeechTask.h"
#include "mozilla/Logging.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Element.h"

#include "mozilla/dom/SpeechSynthesisBinding.h"
#include "SpeechSynthesis.h"
#include "nsSynthVoiceRegistry.h"
#include "nsIDocument.h"

#undef LOG
mozilla::LogModule*
GetSpeechSynthLog()
{
  static mozilla::LazyLogModule sLog("SpeechSynthesis");

  return sLog;
}
#define LOG(type, msg) MOZ_LOG(GetSpeechSynthLog(), type, msg)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(SpeechSynthesis)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SpeechSynthesis)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCurrentTask)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSpeechQueue)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mVoiceCache.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SpeechSynthesis)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentTask)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSpeechQueue)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  for (auto iter = tmp->mVoiceCache.Iter(); !iter.Done(); iter.Next()) {
    SpeechSynthesisVoice* voice = iter.UserData();
    cb.NoteXPCOMChild(voice);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(SpeechSynthesis)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechSynthesis)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechSynthesis)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechSynthesis)

SpeechSynthesis::SpeechSynthesis(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
  , mHoldQueue(false)
  , mInnerID(aParent->WindowID())
{
  MOZ_ASSERT(aParent->IsInnerWindow());
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "inner-window-destroyed", true);
  }

}

SpeechSynthesis::~SpeechSynthesis()
{
}

JSObject*
SpeechSynthesis::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SpeechSynthesisBinding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner*
SpeechSynthesis::GetParentObject() const
{
  return mParent;
}

bool
SpeechSynthesis::Pending() const
{
  switch (mSpeechQueue.Length()) {
  case 0:
    return false;

  case 1:
    return mSpeechQueue.ElementAt(0)->GetState() == SpeechSynthesisUtterance::STATE_PENDING;

  default:
    return true;
  }
}

bool
SpeechSynthesis::Speaking() const
{
  if (!mSpeechQueue.IsEmpty() &&
      mSpeechQueue.ElementAt(0)->GetState() == SpeechSynthesisUtterance::STATE_SPEAKING) {
    return true;
  }

  // Returns global speaking state if global queue is enabled. Or false.
  return nsSynthVoiceRegistry::GetInstance()->IsSpeaking();
}

bool
SpeechSynthesis::Paused() const
{
  return mHoldQueue || (mCurrentTask && mCurrentTask->IsPrePaused()) ||
         (!mSpeechQueue.IsEmpty() && mSpeechQueue.ElementAt(0)->IsPaused());
}

bool
SpeechSynthesis::HasEmptyQueue() const
{
  return mSpeechQueue.Length() == 0;
}

void
SpeechSynthesis::Speak(SpeechSynthesisUtterance& aUtterance)
{
  if (aUtterance.mState != SpeechSynthesisUtterance::STATE_NONE) {
    // XXX: Should probably raise an error
    return;
  }

  mSpeechQueue.AppendElement(&aUtterance);
  aUtterance.mState = SpeechSynthesisUtterance::STATE_PENDING;

  if (mSpeechQueue.Length() == 1 && !mCurrentTask && !mHoldQueue) {
    AdvanceQueue();
  }
}

void
SpeechSynthesis::AdvanceQueue()
{
  LOG(LogLevel::Debug,
      ("SpeechSynthesis::AdvanceQueue length=%d", mSpeechQueue.Length()));

  if (mSpeechQueue.IsEmpty()) {
    return;
  }

  RefPtr<SpeechSynthesisUtterance> utterance = mSpeechQueue.ElementAt(0);

  nsAutoString docLang;
  nsIDocument* doc = mParent->GetExtantDoc();

  if (doc) {
    Element* elm = doc->GetHtmlElement();

    if (elm) {
      elm->GetLang(docLang);
    }
  }

  mCurrentTask =
    nsSynthVoiceRegistry::GetInstance()->SpeakUtterance(*utterance, docLang);

  if (mCurrentTask) {
    mCurrentTask->SetSpeechSynthesis(this);
  }

  return;
}

void
SpeechSynthesis::Cancel()
{
  if (!mSpeechQueue.IsEmpty() &&
      mSpeechQueue.ElementAt(0)->GetState() == SpeechSynthesisUtterance::STATE_SPEAKING) {
    // Remove all queued utterances except for current one, we will remove it
    // in OnEnd
    mSpeechQueue.RemoveElementsAt(1, mSpeechQueue.Length() - 1);
  } else {
    mSpeechQueue.Clear();
  }

  if (mCurrentTask) {
    mCurrentTask->Cancel();
  }
}

void
SpeechSynthesis::Pause()
{
  if (Paused()) {
    return;
  }

  if (mCurrentTask && !mSpeechQueue.IsEmpty() &&
      mSpeechQueue.ElementAt(0)->GetState() != SpeechSynthesisUtterance::STATE_ENDED) {
    mCurrentTask->Pause();
  } else {
    mHoldQueue = true;
  }
}

void
SpeechSynthesis::Resume()
{
  if (!Paused()) {
    return;
  }

  if (mCurrentTask) {
    mCurrentTask->Resume();
  } else {
    mHoldQueue = false;
    AdvanceQueue();
  }
}

void
SpeechSynthesis::OnEnd(const nsSpeechTask* aTask)
{
  MOZ_ASSERT(mCurrentTask == aTask);

  if (!mSpeechQueue.IsEmpty()) {
    mSpeechQueue.RemoveElementAt(0);
  }

  mCurrentTask = nullptr;
  AdvanceQueue();
}

void
SpeechSynthesis::GetVoices(nsTArray< RefPtr<SpeechSynthesisVoice> >& aResult)
{
  aResult.Clear();
  uint32_t voiceCount = 0;

  nsresult rv = nsSynthVoiceRegistry::GetInstance()->GetVoiceCount(&voiceCount);
  if(NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsISupports* voiceParent = NS_ISUPPORTS_CAST(nsIObserver*, this);

  for (uint32_t i = 0; i < voiceCount; i++) {
    nsAutoString uri;
    rv = nsSynthVoiceRegistry::GetInstance()->GetVoice(i, uri);

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to retrieve voice from registry");
      continue;
    }

    SpeechSynthesisVoice* voice = mVoiceCache.GetWeak(uri);

    if (!voice) {
      voice = new SpeechSynthesisVoice(voiceParent, uri);
    }

    aResult.AppendElement(voice);
  }

  mVoiceCache.Clear();

  for (uint32_t i = 0; i < aResult.Length(); i++) {
    SpeechSynthesisVoice* voice = aResult[i];
    mVoiceCache.Put(voice->mUri, voice);
  }
}

// For testing purposes, allows us to cancel the current task that is
// misbehaving, and flush the queue.
void
SpeechSynthesis::ForceEnd()
{
  if (mCurrentTask) {
    mCurrentTask->ForceEnd();
  }
}

NS_IMETHODIMP
SpeechSynthesis::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "inner-window-destroyed")) {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

  uint64_t innerID;
  nsresult rv = wrapper->GetData(&innerID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (innerID == mInnerID) {
    if (mCurrentTask) {
      mCurrentTask->Cancel();
    }

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "inner-window-destroyed");
    }
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
