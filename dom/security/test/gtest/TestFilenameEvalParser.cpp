/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <string.h>
#include <stdlib.h>

#include "nsContentSecurityUtils.h"
#include "nsStringFwd.h"

#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/extensions/WebExtensionPolicy.h"

static NS_NAMED_LITERAL_CSTRING(kChromeURI, "chromeuri");
static NS_NAMED_LITERAL_CSTRING(kResourceURI, "resourceuri");
static NS_NAMED_LITERAL_CSTRING(kBlobUri, "bloburi");
static NS_NAMED_LITERAL_CSTRING(kDataUri, "dataurl");
static NS_NAMED_LITERAL_CSTRING(kSingleString, "singlestring");
static NS_NAMED_LITERAL_CSTRING(kMozillaExtension, "mozillaextension");
static NS_NAMED_LITERAL_CSTRING(kOtherExtension, "otherextension");
static NS_NAMED_LITERAL_CSTRING(kSuspectedUserChromeJS,
                                "suspectedUserChromeJS");
static NS_NAMED_LITERAL_CSTRING(kSanitizedWindowsURL, "sanitizedWindowsURL");
static NS_NAMED_LITERAL_CSTRING(kSanitizedWindowsPath, "sanitizedWindowsPath");
static NS_NAMED_LITERAL_CSTRING(kOther, "other");

#define ASSERT_AND_PRINT(first, second, condition)                      \
  fprintf(stderr, "First: %s\n", first.get());                          \
  fprintf(stderr, "Second: %s\n", NS_ConvertUTF16toUTF8(second).get()); \
  ASSERT_TRUE((condition));
// Usage: ASSERT_AND_PRINT(ret.first, ret.second.value(), ...

TEST(FilenameEvalParser, ResourceChrome)
{
  {
    NS_NAMED_LITERAL_STRING(str, "chrome://firegestures/content/browser.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kChromeURI && ret.second.isSome() &&
                ret.second.value() == str);
  }
  {
    NS_NAMED_LITERAL_STRING(str, "resource://firegestures/content/browser.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kResourceURI && ret.second.isSome() &&
                ret.second.value() == str);
  }
}

TEST(FilenameEvalParser, BlobData)
{
  {
    NS_NAMED_LITERAL_STRING(str, "blob://000-000");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kBlobUri && !ret.second.isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "blob:000-000");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kBlobUri && !ret.second.isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "data://blahblahblah");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kDataUri && !ret.second.isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "data:blahblahblah");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kDataUri && !ret.second.isSome());
  }
}

TEST(FilenameEvalParser, MozExtension)
{
  {  // Test shield.mozilla.org replacing
    NS_NAMED_LITERAL_STRING(
        str,
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/foo/"
        "extensions/federated-learning@shield.mozilla.org.xpi!/experiments/"
        "study/api.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kMozillaExtension &&
                ret.second.value() ==
                    NS_LITERAL_STRING(
                        "federated-learning@s!/experiments/study/api.js"));
  }
  {  // Test mozilla.org replacing
    NS_NAMED_LITERAL_STRING(
        str,
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/api.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(
        ret.first == kMozillaExtension &&
        ret.second.value() ==
            NS_LITERAL_STRING(
                "federated-learning@shigeld.m!/experiments/study/api.js"));
  }
  {  // Test truncating
    NS_NAMED_LITERAL_STRING(
        str,
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(
        ret.first == kMozillaExtension &&
        ret.second.value() ==
            NS_LITERAL_STRING("federated-learning@shigeld.m!/experiments/"
                              "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"));
  }
}

TEST(FilenameEvalParser, UserChromeJS)
{
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures/content/browser.uc.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures/content/browser.uc.js?");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
  {
    nsLiteralString str =
        NS_LITERAL_STRING("firegestures/content/browser.uc.js?243244224");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(
        str,
        "file:///b:/fxprofiles/mark/chrome/"
        "addbookmarkherewithmiddleclick.uc.js?1558444389291");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
}

TEST(FilenameEvalParser, SingleFile)
{
  {
    NS_NAMED_LITERAL_STRING(str, "browser.uc.js?2456");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSingleString && ret.second.isSome() &&
                ret.second.value() == str);
  }
  {
    NS_NAMED_LITERAL_STRING(str, "debugger");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSingleString && ret.second.isSome() &&
                ret.second.value() == str);
  }
}

TEST(FilenameEvalParser, Other)
{
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures--content");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "gallop://thing/fire");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == NS_LITERAL_STRING("gallop"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "gallop://fire");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == NS_LITERAL_STRING("gallop"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures/content");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == NS_LITERAL_STRING("content"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures\\content");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == NS_LITERAL_STRING("content"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "/home/tom/files/thing");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == NS_LITERAL_STRING("thing"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "file://c/uers/tom/file.txt");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == NS_LITERAL_STRING("file://.../file.txt"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "c:/uers/tom/file.txt");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == NS_LITERAL_STRING("file.txt"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "http://example.com/");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == NS_LITERAL_STRING("http"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    NS_NAMED_LITERAL_STRING(str, "http://example.com/thing.html");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == NS_LITERAL_STRING("http"));
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
}

#if defined(XP_WIN)
TEST(FilenameEvalParser, WebExtensionPathParser)
{
  {
    // Set up an Extension and register it so we can test against it.
    mozilla::dom::AutoJSAPI jsAPI;
    ASSERT_TRUE(jsAPI.Init(xpc::PrivilegedJunkScope()));
    JSContext* cx = jsAPI.cx();

    mozilla::dom::GlobalObject go(cx, xpc::PrivilegedJunkScope());
    auto wEI = new mozilla::extensions::WebExtensionInit();

    JS::Rooted<JSObject*> func(
        cx, (JSObject*)JS_NewFunction(cx, (JSNative)1, 0, 0, "customMethodA"));
    JS::Rooted<JSObject*> tempGlobalRoot(cx, JS::CurrentGlobalOrNull(cx));
    wEI->mLocalizeCallback = new mozilla::dom::WebExtensionLocalizeCallback(
        cx, func, tempGlobalRoot, NULL);

    wEI->mAllowedOrigins =
        mozilla::dom::OwningMatchPatternSetOrStringSequence();
    nsString* slotPtr =
        wEI->mAllowedOrigins.SetAsStringSequence().AppendElement(
            mozilla::fallible);
    nsString& slot = *slotPtr;
    slot.Truncate();
    slot = NS_LITERAL_STRING("http://example.com");

    wEI->mName = NS_LITERAL_STRING("gtest Test Extension");
    wEI->mId = NS_LITERAL_STRING("gtesttestextension@mozilla.org");
    wEI->mBaseURL = NS_LITERAL_STRING("file://foo");
    wEI->mMozExtensionHostname =
        NS_LITERAL_CSTRING("e37c3c08-beac-a04b-8032-c4f699a1a856");

    mozilla::ErrorResult eR;
    RefPtr<mozilla::WebExtensionPolicy> w =
        mozilla::extensions::WebExtensionPolicy::Constructor(go, *wEI, eR);
    w->SetActive(true, eR);

    NS_NAMED_LITERAL_STRING(
        str,
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/file.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);

    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() ==
                    NS_LITERAL_STRING(
                        "moz-extension://[gtesttestextension@mozilla.org: "
                        "gtest Test Extension]/path/to/file.js"));

    w->SetActive(false, eR);
  }
  {
    NS_NAMED_LITERAL_STRING(
        str,
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/file.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == NS_LITERAL_STRING("moz-extension"));
  }
  {
    NS_NAMED_LITERAL_STRING(
        str, "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/file.js");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);
    ASSERT_TRUE(
        ret.first == kSanitizedWindowsURL &&
        ret.second.value() ==
            NS_LITERAL_STRING(
                "moz-extension://[failed finding addon by host]/file.js"));
  }
  {
    NS_NAMED_LITERAL_STRING(
        str,
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/"
        "file.js?querystringx=6");
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() ==
                    NS_LITERAL_STRING("moz-extension://[failed finding addon "
                                      "by host]/path/to/file.js"));
  }
}
#endif
