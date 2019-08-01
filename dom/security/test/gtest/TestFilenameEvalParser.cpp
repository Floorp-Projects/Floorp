/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <string.h>
#include <stdlib.h>

#include "nsContentSecurityManager.h"
#include "nsStringFwd.h"

static NS_NAMED_LITERAL_CSTRING(kChromeURI, "chromeuri");
static NS_NAMED_LITERAL_CSTRING(kResourceURI, "resourceuri");
static NS_NAMED_LITERAL_CSTRING(kSingleString, "singlestring");
static NS_NAMED_LITERAL_CSTRING(kMozillaExtension, "mozillaextension");
static NS_NAMED_LITERAL_CSTRING(kOtherExtension, "otherextension");
static NS_NAMED_LITERAL_CSTRING(kSuspectedUserChromeJS,
                                "suspectedUserChromeJS");
static NS_NAMED_LITERAL_CSTRING(kOther, "other");

TEST(FilenameEvalParser, ResourceChrome)
{
  {
    NS_NAMED_LITERAL_STRING(str, "chrome://firegestures/content/browser.js");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kChromeURI && ret.second().isSome() &&
                ret.second().value() == str);
  }
  {
    NS_NAMED_LITERAL_STRING(str, "resource://firegestures/content/browser.js");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kResourceURI && ret.second().isSome() &&
                ret.second().value() == str);
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
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kMozillaExtension &&
                ret.second().value() ==
                    NS_LITERAL_STRING(
                        "federated-learning@s!/experiments/study/api.js"));
  }
  {  // Test mozilla.org replacing
    NS_NAMED_LITERAL_STRING(
        str,
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/api.js");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(
        ret.first() == kMozillaExtension &&
        ret.second().value() ==
            NS_LITERAL_STRING(
                "federated-learning@shigeld.m!/experiments/study/api.js"));
  }
  {  // Test truncating
    NS_NAMED_LITERAL_STRING(
        str,
        "jar:file:///c:/users/bob/appdata/roaming/mozilla/firefox/profiles/foo/"
        "extensions/federated-learning@shigeld.mozilla.org.xpi!/experiments/"
        "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii.js");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(
        ret.first() == kMozillaExtension &&
        ret.second().value() ==
            NS_LITERAL_STRING("federated-learning@shigeld.m!/experiments/"
                              "study/apiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"));
  }
}

TEST(FilenameEvalParser, UserChromeJS)
{
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures/content/browser.uc.js");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kSuspectedUserChromeJS &&
                !ret.second().isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures/content/browser.uc.js?");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kSuspectedUserChromeJS &&
                !ret.second().isSome());
  }
  {
    nsLiteralString str =
        NS_LITERAL_STRING("firegestures/content/browser.uc.js?243244224");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kSuspectedUserChromeJS &&
                !ret.second().isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(
        str,
        "file:///b:/fxprofiles/mark/chrome/"
        "addbookmarkherewithmiddleclick.uc.js?1558444389291");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kSuspectedUserChromeJS &&
                !ret.second().isSome());
  }
}

TEST(FilenameEvalParser, SingleFile)
{
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures/content");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() != kSingleString && !ret.second().isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures\\content");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() != kSingleString && !ret.second().isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "browser.uc.js?2456");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kSingleString && ret.second().isSome() &&
                ret.second().value() == str);
  }
  {
    NS_NAMED_LITERAL_STRING(str, "debugger");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kSingleString && ret.second().isSome() &&
                ret.second().value() == str);
  }
}

TEST(FilenameEvalParser, Other)
{
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures/content");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kOther && !ret.second().isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "firegestures\\content");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kOther && !ret.second().isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "/home/tom/files/thing");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kOther && !ret.second().isSome());
  }
  {
    NS_NAMED_LITERAL_STRING(str, "file://c/uers/tom/file.txt");
    FilenameType ret = nsContentSecurityManager::FilenameToEvalType(str);
    ASSERT_TRUE(ret.first() == kOther && !ret.second().isSome());
  }
}
