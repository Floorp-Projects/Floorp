/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Sanitizer_h
#define mozilla_dom_Sanitizer_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/SanitizerBinding.h"
#include "nsString.h"
#include "nsIParserUtils.h"
#include "nsTreeSanitizer.h"

// XXX(Bug 1673929) This is not really needed here, but the generated
// SanitizerBinding.cpp needs it and does not include it.
#include "mozilla/dom/Document.h"

class nsISupports;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

class Sanitizer final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Sanitizer)

  explicit Sanitizer(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
    MOZ_ASSERT(aGlobal);
    // FIXME(freddyb): Waiting for wicg-draft to evolve. Bug 1658564.
    mSanitizationFlags = nsIParserUtils::SanitizerAllowStyle |
                         nsIParserUtils::SanitizerAllowComments;
  }

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  /**
   * Sanitizer() WebIDL constructor
   * @return a new Sanitizer object, with methods as below
   */
  static already_AddRefed<Sanitizer> Constructor(
      const GlobalObject& aGlobal, const SanitizerOptions& aOptions,
      ErrorResult& aRv);

  /**
   * sanitize WebIDL method.
   * @param aInput       "bad" HTML that needs to be sanitized
   * @return DocumentFragment of the sanitized HTML
   */
  already_AddRefed<DocumentFragment> Sanitize(
      const Optional<mozilla::dom::StringOrDocumentFragmentOrDocument>& aInput,
      ErrorResult& aRv);

  /**
   * sanitizeToString WebIDL method.
   * @param aInput       "bad" HTML that needs to be sanitized
   * @param outSanitized out-param for the string of sanitized HTML
   */
  void SanitizeToString(
      const Optional<StringOrDocumentFragmentOrDocument>& aInput,
      nsAString& outSanitized, ErrorResult& aRv);

  /**
   * Logs localized message to either content console or browser console
   * @param aName              Localization key
   * @param aParams            Localization parameters
   * @param aFlags             Logging Flag (see nsIScriptError)
   */
  void LogLocalizedString(const char* aName, const nsTArray<nsString>& aParams,
                          uint32_t aFlags);

 private:
  ~Sanitizer() = default;
  already_AddRefed<DocumentFragment> InputToNewFragment(
      const Optional<mozilla::dom::StringOrDocumentFragmentOrDocument>& aInput,
      ErrorResult& aRv);
  /**
   * Logs localized message to either content console or browser console
   * @param aMessage           Message to log
   * @param aFlags             Logging Flag (see nsIScriptError)
   * @param aInnerWindowID     Inner Window ID (Logged on browser console if 0)
   * @param aFromPrivateWindow If from private window
   */
  static void LogMessage(const nsAString& aMessage, uint32_t aFlags,
                         uint64_t aInnerWindowID, bool aFromPrivateWindow);

  SanitizerOptions mOptions;
  uint32_t mSanitizationFlags;
  nsCOMPtr<nsIGlobalObject> mGlobal;
};
}  // namespace dom
}  // namespace mozilla

#endif  // ifndef mozilla_dom_Sanitizer_h
