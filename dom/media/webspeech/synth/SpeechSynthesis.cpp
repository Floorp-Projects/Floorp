/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupportsPrimitives.h"
#include "nsSpeechTask.h"
#include "mozilla/Logging.h"

#include "mozilla/dom/Element.h"

#include "mozilla/dom/SpeechSynthesisBinding.h"
#include "SpeechSynthesis.h"
#include "nsContentUtils.h"
#include "nsSynthVoiceRegistry.h"
#include "mozilla/dom/Document.h"
#include "nsIDocShell.h"

#undef LOG
mozilla::LogModule* GetSpeechSynthLog() {
  static mozilla::LazyLogModule sLog("SpeechSynthesis");

  return sLog;
}
#define LOG(type, msg) MOZ_LOG(GetSpeechSynthLog(), type, msg)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(SpeechSynthesis)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SpeechSynthesis,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCurrentTask)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSpeechQueue)
  tmp->mVoiceCache.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SpeechSynthesis,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentTask)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSpeechQueue)
  for (auto iter = tmp->mVoiceCache.Iter(); !iter.Done(); iter.Next()) {
    SpeechSynthesisVoice* voice = iter.UserData();
    cb.NoteXPCOMChild(voice);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechSynthesis)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SpeechSynthesis, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeechSynthesis, DOMEventTargetHelper)

SpeechSynthesis::SpeechSynthesis(nsPIDOMWindowInner* aParent)
    : DOMEventTargetHelper(aParent),
      mHoldQueue(false),
      mInnerID(aParent->WindowID()) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "inner-window-destroyed", true);
    obs->AddObserver(this, "synth-voices-changed", true);
  }
}

SpeechSynthesis::~SpeechSynthesis() {}

JSObject* SpeechSynthesis::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SpeechSynthesis_Binding::Wrap(aCx, this, aGivenProto);
}

bool SpeechSynthesis::Pending() const {
  // If we don't have any task, nothing is pending. If we have only one task,
  // check if that task is currently pending. If we have more than one task,
  // then the tasks after the first one are definitely pending.
  return mSpeechQueue.Length() > 1 ||
         (mSpeechQueue.Length() == 1 &&
          (!mCurrentTask || mCurrentTask->IsPending()));
}

bool SpeechSynthesis::Speaking() const {
  // Check global speaking state if there is no active speaking task.
  return (!mSpeechQueue.IsEmpty() && HasSpeakingTask()) ||
         nsSynthVoiceRegistry::GetInstance()->IsSpeaking();
}

bool SpeechSynthesis::Paused() const {
  return mHoldQueue || (mCurrentTask && mCurrentTask->IsPrePaused()) ||
         (!mSpeechQueue.IsEmpty() && mSpeechQueue.ElementAt(0)->IsPaused());
}

bool SpeechSynthesis::HasEmptyQueue() const {
  return mSpeechQueue.Length() == 0;
}

bool SpeechSynthesis::HasVoices() const {
  uint32_t voiceCount = mVoiceCache.Count();
  if (voiceCount == 0) {
    nsresult rv =
        nsSynthVoiceRegistry::GetInstance()->GetVoiceCount(&voiceCount);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
  }

  return voiceCount != 0;
}

void SpeechSynthesis::Speak(SpeechSynthesisUtterance& aUtterance) {
  if (!mInnerID) {
    return;
  }

  mSpeechQueue.AppendElement(&aUtterance);

  // If we only have one item in the queue, we aren't pre-paused, and
  // we have voices available, speak it.
  if (mSpeechQueue.Length() == 1 && !mCurrentTask && !mHoldQueue &&
      HasVoices()) {
    AdvanceQueue();
  }
}

void SpeechSynthesis::AdvanceQueue() {
  LOG(LogLevel::Debug,
      ("SpeechSynthesis::AdvanceQueue length=%zu", mSpeechQueue.Length()));

  if (mSpeechQueue.IsEmpty()) {
    return;
  }

  RefPtr<SpeechSynthesisUtterance> utterance = mSpeechQueue.ElementAt(0);

  nsAutoString docLang;
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  Document* doc = window ? window->GetExtantDoc() : nullptr;

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
}

void SpeechSynthesis::Cancel() {
  if (!mSpeechQueue.IsEmpty() && HasSpeakingTask()) {
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

void SpeechSynthesis::Pause() {
  if (Paused()) {
    return;
  }

  if (!mSpeechQueue.IsEmpty() && HasSpeakingTask()) {
    mCurrentTask->Pause();
  } else {
    mHoldQueue = true;
  }
}

void SpeechSynthesis::Resume() {
  if (!Paused()) {
    return;
  }

  mHoldQueue = false;

  if (mCurrentTask) {
    mCurrentTask->Resume();
  } else {
    AdvanceQueue();
  }
}

void SpeechSynthesis::OnEnd(const nsSpeechTask* aTask) {
  MOZ_ASSERT(mCurrentTask == aTask);

  if (!mSpeechQueue.IsEmpty()) {
    mSpeechQueue.RemoveElementAt(0);
  }

  mCurrentTask = nullptr;
  AdvanceQueue();
}

void SpeechSynthesis::GetVoices(
    nsTArray<RefPtr<SpeechSynthesisVoice> >& aResult) {
  aResult.Clear();
  uint32_t voiceCount = 0;
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  nsCOMPtr<nsIDocShell> docShell = window ? window->GetDocShell() : nullptr;

  if (nsContentUtils::ShouldResistFingerprinting(docShell)) {
    return;
  }

  nsresult rv = nsSynthVoiceRegistry::GetInstance()->GetVoiceCount(&voiceCount);
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
void SpeechSynthesis::ForceEnd() {
  if (mCurrentTask) {
    mCurrentTask->ForceEnd();
  }
}

NS_IMETHODIMP
SpeechSynthesis::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "inner-window-destroyed") == 0) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t innerID;
    nsresult rv = wrapper->GetData(&innerID);
    NS_ENSURE_SUCCESS(rv, rv);

    if (innerID == mInnerID) {
      mInnerID = 0;
      Cancel();

      nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService();
      if (obs) {
        obs->RemoveObserver(this, "inner-window-destroyed");
      }
    }
  } else if (strcmp(aTopic, "synth-voices-changed") == 0) {
    LOG(LogLevel::Debug, ("SpeechSynthesis::onvoiceschanged"));
    nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
    nsCOMPtr<nsIDocShell> docShell = window ? window->GetDocShell() : nullptr;

    if (!nsContentUtils::ShouldResistFingerprinting(docShell)) {
      DispatchTrustedEvent(NS_LITERAL_STRING("voiceschanged"));
      // If we have a pending item, and voices become available, speak it.
      if (!mCurrentTask && !mHoldQueue && HasVoices()) {
        AdvanceQueue();
      }
    }
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
