/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static content security utilities. */

#ifndef nsContentSecurityUtils_h___
#define nsContentSecurityUtils_h___

class nsIChannel;
class nsIHttpChannel;

namespace mozilla {
namespace dom {
class Document;
}  // namespace dom
}  // namespace mozilla

typedef std::pair<nsCString, mozilla::Maybe<nsString>> FilenameTypeAndDetails;

class nsContentSecurityUtils {
 public:
  static FilenameTypeAndDetails FilenameToFilenameType(
      const nsString& fileName, bool collectAdditionalExtensionData);
  static bool IsEvalAllowed(JSContext* cx, bool aIsSystemPrincipal,
                            const nsAString& aScript);
  static void NotifyEvalUsage(bool aIsSystemPrincipal,
                              NS_ConvertUTF8toUTF16& aFileNameA,
                              uint64_t aWindowID, uint32_t aLineNumber,
                              uint32_t aColumnNumber);

  // Helper function to query the HTTP Channel of a potential
  // multi-part channel. Mostly used for querying response headers
  static nsresult GetHttpChannelFromPotentialMultiPart(
      nsIChannel* aChannel, nsIHttpChannel** aHttpChannel);

  // Helper function which performs the following framing checks
  // * CSP frame-ancestors
  // * x-frame-options
  // If any of the two disallows framing, the channel will be cancelled.
  static void PerformCSPFrameAncestorAndXFOCheck(nsIChannel* aChannel);

#if defined(DEBUG)
  static void AssertAboutPageHasCSP(mozilla::dom::Document* aDocument);
#endif

  static bool ValidateScriptFilename(const char* aFilename,
                                     bool aIsSystemRealm);
};

#endif /* nsContentSecurityUtils_h___ */
