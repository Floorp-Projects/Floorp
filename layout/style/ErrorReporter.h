/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* diagnostic reporting for CSS style sheet parser */

#ifndef mozilla_css_ErrorReporter_h_
#define mozilla_css_ErrorReporter_h_

#include "nsStringFwd.h"

struct nsCSSToken;
class nsIDocument;
class nsIURI;

namespace mozilla {
class StyleSheet;

namespace css {

class Loader;

// FIXME(emilio): Probably better to call this ErrorBuilder or something?
class MOZ_STACK_CLASS ErrorReporter final
{
public:
  ErrorReporter(const StyleSheet* aSheet,
                const Loader* aLoader,
                nsIURI* aURI);

  ~ErrorReporter();

  static void ReleaseGlobals();
  static void EnsureGlobalsInitialized()
  {
    if (MOZ_UNLIKELY(!sInitialized)) {
      InitGlobals();
    }
  }

  static bool ShouldReportErrors(const nsIDocument&);
  static bool ShouldReportErrors(const StyleSheet* aSheet,
                                 const Loader* aLoader);

  void OutputError(uint32_t aLineNumber,
                   uint32_t aLineOffset,
                   const nsACString& aSource);
  void ClearError();

  // In all overloads of ReportUnexpected, aMessage is a stringbundle
  // name, which will be processed as a format string with the
  // indicated number of parameters.

  // no parameters
  void ReportUnexpected(const char *aMessage);
  // one parameter which has already been escaped appropriately
  void ReportUnexpectedUnescaped(const char* aMessage,
                                 const nsAutoString& aParam);

private:
  void OutputError();
  void AddToError(const nsString &aErrorText);
  static void InitGlobals();

  static bool sInitialized;
  static bool sReportErrors;

  nsString mError;
  nsString mErrorLine;
  nsString mFileName;
  const StyleSheet* mSheet;
  const Loader* mLoader;
  nsIURI* mURI;
  uint32_t mErrorLineNumber;
  uint32_t mPrevErrorLineNumber;
  uint32_t mErrorColNumber;
};

} // namespace css
} // namespace mozilla

#endif // mozilla_css_ErrorReporter_h_
