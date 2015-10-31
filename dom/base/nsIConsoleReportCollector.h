/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIConsoleReportCollector_h
#define nsIConsoleReportCollector_h

#include "nsContentUtils.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"

class nsACString;
class nsIDocument;
class nsString;

#define NS_NSICONSOLEREPORTCOLLECTOR_IID \
  {0xdd98a481, 0xd2c4, 0x4203, {0x8d, 0xfa, 0x85, 0xbf, 0xd7, 0xdc, 0xd7, 0x05}}

// An interface for saving reports until we can flush them to the correct
// window at a later time.
class NS_NO_VTABLE nsIConsoleReportCollector : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NSICONSOLEREPORTCOLLECTOR_IID)

  // Add a pending report to be later displayed on the console.  This may be
  // called from any thread.
  //
  // aErrorFlags      A nsIScriptError flags value.
  // aCategory        Name of module reporting error.
  // aPropertiesFile  Properties file containing localized message.
  // aSourceFileURI   The URI of the script generating the error. Must be a URI
  //                  spec.
  // aLineNumber      The line number where the error was generated. May be 0 if
  //                  the line number is not known.
  // aColumnNumber    The column number where the error was generated. May be 0
  //                  if the line number is not known.
  // aMessageName     The name of the localized message contained in the
  //                  properties file.
  // aStringParams    An array of nsString parameters to use when localizing the
  //                  message.
  virtual void
  AddConsoleReport(uint32_t aErrorFlags, const nsACString& aCategory,
                   nsContentUtils::PropertiesFile aPropertiesFile,
                   const nsACString& aSourceFileURI, uint32_t aLineNumber,
                   uint32_t aColumnNumber, const nsACString& aMessageName,
                   const nsTArray<nsString>& aStringParams) = 0;

  // A version of AddConsoleReport() that accepts the message parameters
  // as variable nsString arguments.  Note, the parameters must be exactly
  // nsString and not another string class.  All other args the same as
  // AddConsoleReport().
  template<typename... Params>
  void
  AddConsoleReport(uint32_t aErrorFlags, const nsACString& aCategory,
                   nsContentUtils::PropertiesFile aPropertiesFile,
                   const nsACString& aSourceFileURI, uint32_t aLineNumber,
                   uint32_t aColumnNumber, const nsACString& aMessageName,
                   Params... aParams)
  {
    nsTArray<nsString> params;
    mozilla::dom::StringArrayAppender::Append(params, sizeof...(Params), aParams...);
    AddConsoleReport(aErrorFlags, aCategory, aPropertiesFile, aSourceFileURI,
                     aLineNumber, aColumnNumber, aMessageName, params);
  }

  // Flush all pending reports to the console.
  //
  // aDocument      An optional document representing where to flush the
  //                reports.  If provided, then the corresponding window's
  //                web console will get the reports.  Otherwise the reports
  //                go to the browser console.
  virtual void
  FlushConsoleReports(nsIDocument* aDocument) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIConsoleReportCollector, NS_NSICONSOLEREPORTCOLLECTOR_IID)

#endif // nsIConsoleReportCollector_h
