/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesisVoice_h
#define mozilla_dom_SpeechSynthesisVoice_h

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"

namespace mozilla::dom {

class nsSynthVoiceRegistry;
class SpeechSynthesis;

class SpeechSynthesisVoice final : public nsISupports, public nsWrapperCache {
  friend class nsSynthVoiceRegistry;
  friend class SpeechSynthesis;

 public:
  SpeechSynthesisVoice(nsISupports* aParent, const nsAString& aUri);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SpeechSynthesisVoice)

  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetVoiceURI(nsString& aRetval) const;

  void GetName(nsString& aRetval) const;

  void GetLang(nsString& aRetval) const;

  bool LocalService() const;

  bool Default() const;

 private:
  virtual ~SpeechSynthesisVoice();

  nsCOMPtr<nsISupports> mParent;

  nsString mUri;
};

}  // namespace mozilla::dom

#endif
