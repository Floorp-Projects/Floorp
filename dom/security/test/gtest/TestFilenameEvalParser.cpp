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

static constexpr auto kChromeURI = "chromeuri"_ns;
static constexpr auto kResourceURI = "resourceuri"_ns;
static constexpr auto kBlobUri = "bloburi"_ns;
static constexpr auto kDataUri = "dataurl"_ns;
static constexpr auto kSingleString = "singlestring"_ns;
static constexpr auto kMozillaExtension = "mozillaextension"_ns;
static constexpr auto kOtherExtension = "otherextension"_ns;
static constexpr auto kSuspectedUserChromeJS = "suspectedUserChromeJS"_ns;
static constexpr auto kSanitizedWindowsURL = "sanitizedWindowsURL"_ns;
static constexpr auto kSanitizedWindowsPath = "sanitizedWindowsPath"_ns;
static constexpr auto kOther = "other"_ns;

#define ASSERT_AND_PRINT(first, second, condition)                      \
  fprintf(stderr, "First: %s\n", first.get());                          \
  fprintf(stderr, "Second: %s\n", NS_ConvertUTF16toUTF8(second).get()); \
  ASSERT_TRUE((condition));
// Usage: ASSERT_AND_PRINT(ret.first, ret.second.value(), ...

TEST(FilenameEvalParser, ResourceChrome)
{
  {
    constexpr auto str = u"chrome://firegestures/content/browser.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kChromeURI && ret.second.isSome() &&
                ret.second.value() == str);
  }
  {
    constexpr auto str = u"resource://firegestures/content/browser.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kResourceURI && ret.second.isSome() &&
                ret.second.value() == str);
  }
}

TEST(FilenameEvalParser, BlobData)
{
  {
    constexpr auto str = u"blob://000-000"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kBlobUri && !ret.second.isSome());
  }
  {
    constexpr auto str = u"blob:000-000"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kBlobUri && !ret.second.isSome());
  }
  {
    constexpr auto str = u"data://blahblahblah"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kDataUri && !ret.second.isSome());
  }
  {
    constexpr auto str = u"data:blahblahblah"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kDataUri && !ret.second.isSome());
  }
}

TEST(FilenameEvalParser, MozExtension)
{
  {  // Test shield.mozilla.org replacing
    constexpr auto str =
        u"jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/"
        u"foo/"
        "extensions/federated-learning@shield.mozilla.org.xpi!/experiments/"
        "study/api.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kMozillaExtension &&
                ret.second.value() ==
                    u"federated-learning@s!/experiments/study/api.js"_ns);
  }
  {  // Test mozilla.org replacing
    constexpr auto str =
        u"jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/"
        u"foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/api.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(
        ret.first == kMozillaExtension &&
        ret.second.value() ==
            nsLiteralString(
                u"federated-learning@shigeld.m!/experiments/study/api.js"));
  }
  {  // Test truncating
    constexpr auto str =
        u"jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/"
        u"foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kMozillaExtension &&
                ret.second.value() ==
                    u"federated-learning@shigeld.m!/experiments/"
                    "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"_ns);
  }
}

TEST(FilenameEvalParser, UserChromeJS)
{
  {
    constexpr auto str = u"firegestures/content/browser.uc.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
  {
    constexpr auto str = u"firegestures/content/browser.uc.js?"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
  {
    constexpr auto str = u"firegestures/content/browser.uc.js?243244224"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
  {
    constexpr auto str =
        u"file:///b:/fxprofiles/mark/chrome/"
        "addbookmarkherewithmiddleclick.uc.js?1558444389291"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSuspectedUserChromeJS && !ret.second.isSome());
  }
}

TEST(FilenameEvalParser, SingleFile)
{
  {
    constexpr auto str = u"browser.uc.js?2456"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSingleString && ret.second.isSome() &&
                ret.second.value() == str);
  }
  {
    constexpr auto str = u"debugger"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSingleString && ret.second.isSome() &&
                ret.second.value() == str);
  }
}

TEST(FilenameEvalParser, Other)
{
  {
    constexpr auto str = u"firegestures--content"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
  }
  {
    constexpr auto str = u"gallop://thing/fire"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"gallop"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"gallop://fire"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"gallop"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"firegestures/content"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == u"content"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"firegestures\\content"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == u"content"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"/home/tom/files/thing"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == u"thing"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"file://c/uers/tom/file.txt"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"file://.../file.txt"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"c:/uers/tom/file.txt"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsPath &&
                ret.second.value() == u"file.txt"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"http://example.com/"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"http"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"http://example.com/thing.html"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"http"_ns);
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
    slot = u"http://example.com"_ns;

    wEI->mName = u"gtest Test Extension"_ns;
    wEI->mId = u"gtesttestextension@mozilla.org"_ns;
    wEI->mBaseURL = u"file://foo"_ns;
    wEI->mMozExtensionHostname = "e37c3c08-beac-a04b-8032-c4f699a1a856"_ns;

    mozilla::ErrorResult eR;
    RefPtr<mozilla::WebExtensionPolicy> w =
        mozilla::extensions::WebExtensionPolicy::Constructor(go, *wEI, eR);
    w->SetActive(true, eR);

    constexpr auto str =
        u"moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/file.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);

    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() ==
                    u"moz-extension://[gtesttestextension@mozilla.org: "
                    "gtest Test Extension]/path/to/file.js"_ns);

    w->SetActive(false, eR);
  }
  {
    constexpr auto str =
        u"moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/file.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"moz-extension"_ns);
  }
  {
    constexpr auto str =
        u"moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/file.js"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);
    ASSERT_TRUE(
        ret.first == kSanitizedWindowsURL &&
        ret.second.value() ==
            nsLiteralString(
                u"moz-extension://[failed finding addon by host]/file.js"));
  }
  {
    constexpr auto str =
        u"moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856/path/to/"
        "file.js?querystringx=6"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, true);
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() ==
                    u"moz-extension://[failed finding addon "
                    "by host]/path/to/file.js"_ns);
  }
}
#endif

TEST(FilenameEvalParser, AboutPageParser)
{
  {
    constexpr auto str = u"about:about"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"about:about"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"about:about?hello"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"about:about"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"about:about#mom"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"about:about"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
  {
    constexpr auto str = u"about:about?hello=there#mom"_ns;
    FilenameTypeAndDetails ret =
        nsContentSecurityUtils::FilenameToFilenameType(str, false);
#if defined(XP_WIN)
    ASSERT_TRUE(ret.first == kSanitizedWindowsURL &&
                ret.second.value() == u"about:about"_ns);
#else
    ASSERT_TRUE(ret.first == kOther && !ret.second.isSome());
#endif
  }
}
