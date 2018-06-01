/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIConsoleReportCollector_h
#define nsIConsoleReportCollector_h

#include "nsContentUtils.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"

class nsIDocument;

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
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
  // as variable nsString arguments (or really, any sort of const nsAString).
  // All other args the same as AddConsoleReport().
  template<typename... Params>
  void
  AddConsoleReport(uint32_t aErrorFlags, const nsACString& aCategory,
                   nsContentUtils::PropertiesFile aPropertiesFile,
                   const nsACString& aSourceFileURI, uint32_t aLineNumber,
                   uint32_t aColumnNumber, const nsACString& aMessageName,
                   Params&&... aParams)
  {
    nsTArray<nsString> params;
    mozilla::dom::StringArrayAppender::Append(params, sizeof...(Params),
                                              std::forward<Params>(aParams)...);
    AddConsoleReport(aErrorFlags, aCategory, aPropertiesFile, aSourceFileURI,
                     aLineNumber, aColumnNumber, aMessageName, params);
  }

  // An enum calss to indicate whether should free the pending reports or not.
  // Forget        Free the pending reports.
  // Save          Keep the pending reports.
  enum class ReportAction {
    Forget,
    Save
  };

  // Flush all pending reports to the console.  May be called from any thread.
  //
  // aInnerWindowID A inner window ID representing where to flush the reports.
  // aAction        An action to determine whether to reserve the pending
  //                reports. Defalut action is to forget the report.
  virtual void
  FlushReportsToConsole(uint64_t aInnerWindowID,
                        ReportAction aAction = ReportAction::Forget) = 0;

  virtual void
  FlushReportsToConsoleForServiceWorkerScope(const nsACString& aScope,
                                             ReportAction aAction = ReportAction::Forget) = 0;

  // Flush all pending reports to the console.  Main thread only.
  //
  // aDocument      An optional document representing where to flush the
  //                reports.  If provided, then the corresponding window's
  //                web console will get the reports.  Otherwise the reports
  //                go to the browser console.
  // aAction        An action to determine whether to reserve the pending
  //                reports. Defalut action is to forget the report.
  virtual void
  FlushConsoleReports(nsIDocument* aDocument,
                      ReportAction aAction = ReportAction::Forget) = 0;

  // Flush all pending reports to the console.  May be called from any thread.
  //
  // aLoadGroup     An optional loadGroup representing where to flush the
  //                reports.  If provided, then the corresponding window's
  //                web console will get the reports.  Otherwise the reports
  //                go to the browser console.
  // aAction        An action to determine whether to reserve the pending
  //                reports. Defalut action is to forget the report.
  virtual void
  FlushConsoleReports(nsILoadGroup* aLoadGroup,
                      ReportAction aAction = ReportAction::Forget) = 0;


  // Flush all pending reports to another collector.  May be called from any
  // thread.
  //
  // aCollector     A required collector object that will effectively take
  //                ownership of our currently console reports.
  virtual void
  FlushConsoleReports(nsIConsoleReportCollector* aCollector) = 0;

  // Clear all pending reports.
  virtual void
  ClearConsoleReports() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIConsoleReportCollector, NS_NSICONSOLEREPORTCOLLECTOR_IID)

#endif // nsIConsoleReportCollector_h
