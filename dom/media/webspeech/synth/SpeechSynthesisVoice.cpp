/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechSynthesis.h"
#include "nsSynthVoiceRegistry.h"
#include "mozilla/dom/SpeechSynthesisVoiceBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SpeechSynthesisVoice, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechSynthesisVoice)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechSynthesisVoice)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechSynthesisVoice)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechSynthesisVoice::SpeechSynthesisVoice(nsISupports* aParent,
                                           const nsAString& aUri)
  : mParent(aParent)
  , mUri(aUri)
{
}

SpeechSynthesisVoice::~SpeechSynthesisVoice()
{
}

JSObject*
SpeechSynthesisVoice::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SpeechSynthesisVoice_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
SpeechSynthesisVoice::GetParentObject() const
{
  return mParent;
}

void
SpeechSynthesisVoice::GetVoiceURI(nsString& aRetval) const
{
  aRetval = mUri;
}

void
SpeechSynthesisVoice::GetName(nsString& aRetval) const
{
  DebugOnly<nsresult> rv =
    nsSynthVoiceRegistry::GetInstance()->GetVoiceName(mUri, aRetval);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to get SpeechSynthesisVoice.name");
}

void
SpeechSynthesisVoice::GetLang(nsString& aRetval) const
{
  DebugOnly<nsresult> rv =
    nsSynthVoiceRegistry::GetInstance()->GetVoiceLang(mUri, aRetval);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to get SpeechSynthesisVoice.lang");
}

bool
SpeechSynthesisVoice::LocalService() const
{
  bool isLocal;
  DebugOnly<nsresult> rv =
    nsSynthVoiceRegistry::GetInstance()->IsLocalVoice(mUri, &isLocal);
  NS_WARNING_ASSERTION(
    NS_SUCCEEDED(rv), "Failed to get SpeechSynthesisVoice.localService");

  return isLocal;
}

bool
SpeechSynthesisVoice::Default() const
{
  bool isDefault;
  DebugOnly<nsresult> rv =
    nsSynthVoiceRegistry::GetInstance()->IsDefaultVoice(mUri, &isDefault);
  NS_WARNING_ASSERTION(
    NS_SUCCEEDED(rv), "Failed to get SpeechSynthesisVoice.default");

  return isDefault;
}

} // namespace dom
} // namespace mozilla
