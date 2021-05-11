/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "core/TelemetryEvent.h"
#include "gtest/gtest.h"
#include "js/Array.h"  // JS::GetArrayLength
#include "js/TypeDecls.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

#include <string.h>
#include <stdlib.h>
#include "nsContentSecurityManager.h"
#include "nsContentSecurityUtils.h"
#include "nsContentUtils.h"
#include "nsIContentPolicy.h"
#include "nsILoadInfo.h"
#include "nsNetUtil.h"
#include "nsStringFwd.h"
#include "mozilla/nsRedirectHistoryEntry.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;

TEST_F(TelemetryTestFixture, UnexpectedPrivilegedLoadsTelemetryTest) {
  struct testResults {
    nsCString fileinfo;
    nsCString extraValueContenttype;
    nsCString extraValueRemotetype;
    nsCString extraValueFiledetails;
    nsCString extraValueRedirects;
  };

  struct testCasesAndResults {
    nsCString urlstring;
    nsContentPolicyType contentType;
    nsCString remoteType;
    testResults expected;
  };

  AutoJSContextWithGlobal cx(mCleanGlobal);
  // Make sure we don't look at events from other tests.
  Unused << mTelemetry->ClearEvents();

  // required for telemetry lookups
  constexpr auto category = "security"_ns;
  constexpr auto method = "unexpectedload"_ns;
  constexpr auto object = "systemprincipal"_ns;
  constexpr auto extraKeyContenttype = "contenttype"_ns;
  constexpr auto extraKeyRemotetype = "remotetype"_ns;
  constexpr auto extraKeyFiledetails = "filedetails"_ns;
  constexpr auto extraKeyRedirects = "redirects"_ns;

  // some cases from TestFilenameEvalParser
  // no need to replicate all scenarios?!
  testCasesAndResults myTestCases[] = {
      {"chrome://firegestures/content/browser.js"_ns,
       nsContentPolicyType::TYPE_SCRIPT,
       "web"_ns,
       {"chromeuri"_ns, "TYPE_SCRIPT"_ns, "web"_ns,
        "chrome://firegestures/content/browser.js"_ns, ""_ns}},
      {"resource://firegestures/content/browser.js"_ns,
       nsContentPolicyType::TYPE_SCRIPT,
       "web"_ns,
       {"resourceuri"_ns, "TYPE_SCRIPT"_ns, "web"_ns,
        "resource://firegestures/content/browser.js"_ns, ""_ns}},
      {// test that we don't report blob details
       // ..and test that we strip of URLs from remoteTypes
       "blob://000-000"_ns,
       nsContentPolicyType::TYPE_SCRIPT,
       "webIsolated=https://blob.example/"_ns,
       {"bloburi"_ns, "TYPE_SCRIPT"_ns, "webIsolated"_ns, "unknown"_ns, ""_ns}},
      {// test for cases where finalURI is null, due to a broken nested URI
       // .. like malformed moz-icon URLs
       "moz-icon:blahblah"_ns,
       nsContentPolicyType::TYPE_STYLESHEET,
       "web"_ns,
       {"other"_ns, "TYPE_STYLESHEET"_ns, "web"_ns, "unknown"_ns, ""_ns}},
      {// we dont report data urls
       // ..and test that we strip of URLs from remoteTypes
       "data://blahblahblah"_ns,
       nsContentPolicyType::TYPE_SCRIPT,
       "webCOOP+COEP=https://data.example"_ns,
       {"dataurl"_ns, "TYPE_SCRIPT"_ns, "webCOOP+COEP"_ns, "unknown"_ns,
        ""_ns}},
      {// handle data URLs for webextension content scripts differently
       // .. by noticing their annotation
       "data:text/css;extension=style;charset=utf-8,/* some css here */"_ns,
       nsContentPolicyType::TYPE_STYLESHEET,
       "web"_ns,
       {"dataurl-extension-contentstyle"_ns, "TYPE_STYLESHEET"_ns, "web"_ns,
        "unknown"_ns, ""_ns}},
      {// we only report file URLs on windows, where we can easily sanitize
       "file://c/users/tom/file.txt"_ns,
       nsContentPolicyType::TYPE_SCRIPT,
       "web"_ns,
       {
#if defined(XP_WIN)
           "sanitizedWindowsURL"_ns, "TYPE_SCRIPT"_ns, "web"_ns,
           "file://.../file.txt"_ns, ""_ns

#else
           "other"_ns, "TYPE_SCRIPT"_ns, "web"_ns, "unknown"_ns, ""_ns
#endif
       }},
      {// test for one redirect
       "moz-extension://abcdefab-1234-4321-0000-abcdefabcdef/js/assets.js"_ns,
       nsContentPolicyType::TYPE_SCRIPT,
       "web"_ns,
       {
#if defined(XP_WIN)
           "sanitizedWindowsURL"_ns, "TYPE_SCRIPT"_ns, "web"_ns,
           // the extension-id is made-up, so the extension will report failure
           "moz-extension://[failed finding addon by host]/js/assets.js"_ns,
           "https"_ns
#else
           "other"_ns, "TYPE_SCRIPT"_ns, "web"_ns, "unknown"_ns, "https"_ns
#endif
       }},
      {// test for cases where finalURI is empty
       ""_ns,
       nsContentPolicyType::TYPE_STYLESHEET,
       "web"_ns,
       {"other"_ns, "TYPE_STYLESHEET"_ns, "web"_ns, "unknown"_ns, ""_ns}},
      {// test for cases where finalURI is null, due to the struct layout, we'll
       // override the URL with nullptr in loop below.
       "URLWillResultInNullPtr"_ns,
       nsContentPolicyType::TYPE_SCRIPT,
       "web"_ns,
       {"other"_ns, "TYPE_SCRIPT"_ns, "web"_ns, "unknown"_ns, ""_ns}},
  };

  int i = 0;
  for (auto const& currentTest : myTestCases) {
    nsresult rv;
    nsCOMPtr<nsIURI> uri;

    // special-casing for a case where the uri is null
    if (!currentTest.urlstring.Equals("URLWillResultInNullPtr")) {
      NS_NewURI(getter_AddRefs(uri), currentTest.urlstring);
    }

    // We can't create channels for chrome: URLs unless they are in a chrome
    // registry that maps them into the actual destination URL (usually
    // file://). It seems that gtest don't have chrome manifest registered, so
    // we'll use a mockChannel with a mockUri.
    nsCOMPtr<nsIURI> mockUri;
    rv = NS_NewURI(getter_AddRefs(mockUri), "http://example.com"_ns);
    ASSERT_EQ(rv, NS_OK) << "Could not create mockUri";
    nsCOMPtr<nsIChannel> mockChannel;
    nsCOMPtr<nsIIOService> service = do_GetIOService();
    if (!service) {
      ASSERT_TRUE(false)
      << "Couldn't initialize IOService";
    }
    rv = service->NewChannelFromURI(
        mockUri, nullptr, nsContentUtils::GetSystemPrincipal(),
        nsContentUtils::GetSystemPrincipal(), 0, currentTest.contentType,
        getter_AddRefs(mockChannel));
    ASSERT_EQ(rv, NS_OK) << "Could not create a mock channel";
    nsCOMPtr<nsILoadInfo> mockLoadInfo = mockChannel->LoadInfo();

    // We're adding a redirect entry for one specific test
    if (currentTest.urlstring.EqualsASCII(
            "moz-extension://abcdefab-1234-4321-0000-abcdefabcdef/js/"
            "assets.js")) {
      nsCOMPtr<nsIURI> redirUri;
      NS_NewURI(getter_AddRefs(redirUri),
                "https://www.analytics.example/analytics.js"_ns);
      nsCOMPtr<nsIPrincipal> redirPrincipal =
          BasePrincipal::CreateContentPrincipal(redirUri, OriginAttributes());
      nsCOMPtr<nsIRedirectHistoryEntry> entry =
          new net::nsRedirectHistoryEntry(redirPrincipal, nullptr, ""_ns);

      mockLoadInfo->AppendRedirectHistoryEntry(entry, false);
    }

    // this will record the event
    nsContentSecurityManager::MeasureUnexpectedPrivilegedLoads(
        mockLoadInfo, uri, currentTest.remoteType);

    // let's inspect the recorded events

    JS::RootedValue eventsSnapshot(cx.GetJSContext());
    GetEventSnapshot(cx.GetJSContext(), &eventsSnapshot);

    ASSERT_TRUE(EventPresent(cx.GetJSContext(), eventsSnapshot, category,
                             method, object))
    << "Test event with value and extra must be present.";

    // Convert eventsSnapshot into array/object
    JSContext* aCx = cx.GetJSContext();
    JS::RootedObject arrayObj(aCx, &eventsSnapshot.toObject());

    JS::Rooted<JS::Value> eventRecord(aCx);
    ASSERT_TRUE(JS_GetElement(aCx, arrayObj, i++, &eventRecord))
    << "Must be able to get record.";  // record is already undefined :-/

    ASSERT_TRUE(!eventRecord.isUndefined())
    << "eventRecord should not be undefined";

    JS::RootedObject recordArray(aCx, &eventRecord.toObject());
    uint32_t recordLength;
    ASSERT_TRUE(JS::GetArrayLength(aCx, recordArray, &recordLength))
    << "Event record array must have length.";
    ASSERT_TRUE(recordLength == 6)
    << "Event record must have 6 elements.";

    JS::Rooted<JS::Value> str(aCx);
    nsAutoJSString jsStr;
    // The fileinfo string is at index 4
    ASSERT_TRUE(JS_GetElement(aCx, recordArray, 4, &str))
    << "Must be able to get value.";
    ASSERT_TRUE(jsStr.init(aCx, str))
    << "Value must be able to be init'd to a jsstring.";

    ASSERT_STREQ(NS_ConvertUTF16toUTF8(jsStr).get(),
                 currentTest.expected.fileinfo.get())
        << "Reported fileinfo '" << NS_ConvertUTF16toUTF8(jsStr).get()
        << " 'equals expected value: " << currentTest.expected.fileinfo.get();

    // Extra is at index 5
    JS::Rooted<JS::Value> obj(aCx);
    ASSERT_TRUE(JS_GetElement(aCx, recordArray, 5, &obj))
    << "Must be able to get extra data";
    JS::RootedObject extraObj(aCx, &obj.toObject());
    // looking at remotetype extra for content type
    JS::Rooted<JS::Value> extraValC(aCx);
    ASSERT_TRUE(
        JS_GetProperty(aCx, extraObj, extraKeyContenttype.get(), &extraValC))
    << "Must be able to get the extra key's value for contenttype";
    ASSERT_TRUE(jsStr.init(aCx, extraValC))
    << "Extra value contenttype must be able to be init'd to a jsstring.";
    ASSERT_STREQ(NS_ConvertUTF16toUTF8(jsStr).get(),
                 currentTest.expected.extraValueContenttype.get())
        << "Reported value for extra contenttype '"
        << NS_ConvertUTF16toUTF8(jsStr).get()
        << "' should equals supplied value"
        << currentTest.expected.extraValueContenttype.get();
    // and again for remote type
    JS::Rooted<JS::Value> extraValP(aCx);
    ASSERT_TRUE(
        JS_GetProperty(aCx, extraObj, extraKeyRemotetype.get(), &extraValP))
    << "Must be able to get the extra key's value for remotetype";
    ASSERT_TRUE(jsStr.init(aCx, extraValP))
    << "Extra value remotetype must be able to be init'd to a jsstring.";
    ASSERT_STREQ(NS_ConvertUTF16toUTF8(jsStr).get(),
                 currentTest.expected.extraValueRemotetype.get())
        << "Reported value for extra remotetype '"
        << NS_ConvertUTF16toUTF8(jsStr).get()
        << "' should equals supplied value: "
        << currentTest.expected.extraValueRemotetype.get();
    // repeating the same for filedetails extra
    JS::Rooted<JS::Value> extraValF(aCx);
    ASSERT_TRUE(
        JS_GetProperty(aCx, extraObj, extraKeyFiledetails.get(), &extraValF))
    << "Must be able to get the extra key's value for filedetails";
    ASSERT_TRUE(jsStr.init(aCx, extraValF))
    << "Extra value filedetails must be able to be init'd to a jsstring.";
    ASSERT_STREQ(NS_ConvertUTF16toUTF8(jsStr).get(),
                 currentTest.expected.extraValueFiledetails.get())
        << "Reported value for extra filedetails '"
        << NS_ConvertUTF16toUTF8(jsStr).get() << "'should equals supplied value"
        << currentTest.expected.extraValueFiledetails.get();
    // checking the extraKeyRedirects match
    JS::Rooted<JS::Value> extraValRedirects(aCx);
    ASSERT_TRUE(JS_GetProperty(aCx, extraObj, extraKeyRedirects.get(),
                               &extraValRedirects))
    << "Must be able to get the extra value for redirects";
    ASSERT_TRUE(jsStr.init(aCx, extraValRedirects))
    << "Extra value redirects must be able to be init'd to a jsstring";
    ASSERT_STREQ(NS_ConvertUTF16toUTF8(jsStr).get(),
                 currentTest.expected.extraValueRedirects.get())
        << "Reported value for extra redirect '"
        << NS_ConvertUTF16toUTF8(jsStr).get()
        << "' should equals supplied value: "
        << currentTest.expected.extraValueRedirects.get();
  }
}
