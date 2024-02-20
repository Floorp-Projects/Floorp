/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginParser.h"

#include <regex>

#include "mozilla/OriginAttributes.h"
#include "mozilla/dom/quota/Constants.h"
#include "mozilla/dom/quota/QuotaCommon.h"

namespace mozilla::dom::quota {

// static
auto OriginParser::ParseOrigin(const nsACString& aOrigin, nsCString& aSpec,
                               OriginAttributes* aAttrs,
                               nsCString& aOriginalSuffix) -> ResultType {
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(aAttrs);

  nsCString origin(aOrigin);
  int32_t pos = origin.RFindChar('^');

  if (pos == kNotFound) {
    aOriginalSuffix.Truncate();
  } else {
    aOriginalSuffix = Substring(origin, pos);
  }

  OriginAttributes originAttributes;

  nsCString originNoSuffix;
  bool ok = originAttributes.PopulateFromOrigin(aOrigin, originNoSuffix);
  if (!ok) {
    return InvalidOrigin;
  }

  OriginParser parser(originNoSuffix);

  *aAttrs = originAttributes;
  return parser.Parse(aSpec);
}

auto OriginParser::Parse(nsACString& aSpec) -> ResultType {
  while (mTokenizer.hasMoreTokens()) {
    const nsDependentCSubstring& token = mTokenizer.nextToken();

    HandleToken(token);

    if (mError) {
      break;
    }

    if (!mHandledTokens.IsEmpty()) {
      mHandledTokens.AppendLiteral(", ");
    }
    mHandledTokens.Append('\'');
    mHandledTokens.Append(token);
    mHandledTokens.Append('\'');
  }

  if (!mError && mTokenizer.separatorAfterCurrentToken()) {
    HandleTrailingSeparator();
  }

  if (mError) {
    QM_WARNING("Origin '%s' failed to parse, handled tokens: %s", mOrigin.get(),
               mHandledTokens.get());

    return (mSchemeType == eChrome || mSchemeType == eAbout) ? ObsoleteOrigin
                                                             : InvalidOrigin;
  }

  MOZ_ASSERT(mState == eComplete || mState == eHandledTrailingSeparator);

  // For IPv6 URL, it should at least have three groups.
  MOZ_ASSERT_IF(mIPGroup > 0, mIPGroup >= 3);

  nsAutoCString spec(mScheme);

  if (mSchemeType == eFile) {
    spec.AppendLiteral("://");

    if (mUniversalFileOrigin) {
      MOZ_ASSERT(mPathnameComponents.Length() == 1);

      spec.Append(mPathnameComponents[0]);
    } else {
      for (uint32_t count = mPathnameComponents.Length(), index = 0;
           index < count; index++) {
        spec.Append('/');
        spec.Append(mPathnameComponents[index]);
      }
    }

    aSpec = spec;

    return ValidOrigin;
  }

  if (mSchemeType == eAbout) {
    if (mMaybeObsolete) {
      // The "moz-safe-about+++home" was acciedntally created by a buggy nightly
      // and can be safely removed.
      return mHost.EqualsLiteral("home") ? ObsoleteOrigin : InvalidOrigin;
    }
    spec.Append(':');
  } else if (mSchemeType != eChrome) {
    spec.AppendLiteral("://");
  }

  spec.Append(mHost);

  if (!mPort.IsNull()) {
    spec.Append(':');
    spec.AppendInt(mPort.Value());
  }

  aSpec = spec;

  return mScheme.EqualsLiteral("app") ? ObsoleteOrigin : ValidOrigin;
}

void OriginParser::HandleScheme(const nsDependentCSubstring& aToken) {
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingAppIdOrScheme || mState == eExpectingScheme);

  bool isAbout = false;
  bool isMozSafeAbout = false;
  bool isFile = false;
  bool isChrome = false;
  if (aToken.EqualsLiteral("http") || aToken.EqualsLiteral("https") ||
      (isAbout = aToken.EqualsLiteral("about") ||
                 (isMozSafeAbout = aToken.EqualsLiteral("moz-safe-about"))) ||
      aToken.EqualsLiteral("indexeddb") ||
      (isFile = aToken.EqualsLiteral("file")) || aToken.EqualsLiteral("app") ||
      aToken.EqualsLiteral("resource") ||
      aToken.EqualsLiteral("moz-extension") ||
      (isChrome = aToken.EqualsLiteral(kChromeOrigin)) ||
      aToken.EqualsLiteral("uuid")) {
    mScheme = aToken;

    if (isAbout) {
      mSchemeType = eAbout;
      mState = isMozSafeAbout ? eExpectingEmptyToken1OrHost : eExpectingHost;
    } else if (isChrome) {
      mSchemeType = eChrome;
      if (mTokenizer.hasMoreTokens()) {
        mError = true;
      }
      mState = eComplete;
    } else {
      if (isFile) {
        mSchemeType = eFile;
      }
      mState = eExpectingEmptyToken1;
    }

    return;
  }

  QM_WARNING("'%s' is not a valid scheme!", nsCString(aToken).get());

  mError = true;
}

void OriginParser::HandlePathnameComponent(
    const nsDependentCSubstring& aToken) {
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingEmptyTokenOrDriveLetterOrPathnameComponent ||
             mState == eExpectingEmptyTokenOrPathnameComponent);
  MOZ_ASSERT(mSchemeType == eFile);

  mPathnameComponents.AppendElement(aToken);

  mState = mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                      : eComplete;
}

void OriginParser::HandleToken(const nsDependentCSubstring& aToken) {
  switch (mState) {
    case eExpectingAppIdOrScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected an app id or scheme (not an empty string)!");

        mError = true;
        return;
      }

      if (IsAsciiDigit(aToken.First())) {
        // nsDependentCSubstring doesn't provice ToInteger()
        nsCString token(aToken);

        nsresult rv;
        Unused << token.ToInteger(&rv);
        if (NS_SUCCEEDED(rv)) {
          mState = eExpectingInMozBrowser;
          return;
        }
      }

      HandleScheme(aToken);

      return;
    }

    case eExpectingInMozBrowser: {
      if (aToken.Length() != 1) {
        QM_WARNING("'%zu' is not a valid length for the inMozBrowser flag!",
                   aToken.Length());

        mError = true;
        return;
      }

      if (aToken.First() == 't') {
        mInIsolatedMozBrowser = true;
      } else if (aToken.First() == 'f') {
        mInIsolatedMozBrowser = false;
      } else {
        QM_WARNING("'%s' is not a valid value for the inMozBrowser flag!",
                   nsCString(aToken).get());

        mError = true;
        return;
      }

      mState = eExpectingScheme;

      return;
    }

    case eExpectingScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a scheme (not an empty string)!");

        mError = true;
        return;
      }

      HandleScheme(aToken);

      return;
    }

    case eExpectingEmptyToken1: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the first empty token!");

        mError = true;
        return;
      }

      mState = eExpectingEmptyToken2;

      return;
    }

    case eExpectingEmptyToken2: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the second empty token!");

        mError = true;
        return;
      }

      if (mSchemeType == eFile) {
        mState = eExpectingEmptyTokenOrUniversalFileOrigin;
      } else {
        if (mSchemeType == eAbout) {
          mMaybeObsolete = true;
        }
        mState = eExpectingHost;
      }

      return;
    }

    case eExpectingEmptyTokenOrUniversalFileOrigin: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrDriveLetterOrPathnameComponent
                     : eComplete;

        return;
      }

      if (aToken.EqualsLiteral("UNIVERSAL_FILE_URI_ORIGIN")) {
        mUniversalFileOrigin = true;

        mPathnameComponents.AppendElement(aToken);

        mState = eComplete;

        return;
      }

      QM_WARNING(
          "Expected the third empty token or "
          "UNIVERSAL_FILE_URI_ORIGIN!");

      mError = true;
      return;
    }

    case eExpectingHost: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a host (not an empty string)!");

        mError = true;
        return;
      }

      mHost = aToken;

      if (aToken.First() == '[') {
        MOZ_ASSERT(mIPGroup == 0);

        ++mIPGroup;
        mState = eExpectingIPV6Token;

        MOZ_ASSERT(mTokenizer.hasMoreTokens());
        return;
      }

      if (mTokenizer.hasMoreTokens()) {
        if (mSchemeType == eAbout) {
          QM_WARNING("Expected an empty string after host!");

          mError = true;
          return;
        }

        mState = eExpectingPort;

        return;
      }

      mState = eComplete;

      return;
    }

    case eExpectingPort: {
      MOZ_ASSERT(mSchemeType == eNone);

      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a port (not an empty string)!");

        mError = true;
        return;
      }

      // nsDependentCSubstring doesn't provice ToInteger()
      nsCString token(aToken);

      nsresult rv;
      uint32_t port = token.ToInteger(&rv);
      if (NS_SUCCEEDED(rv)) {
        mPort.SetValue() = port;
      } else {
        QM_WARNING("'%s' is not a valid port number!", token.get());

        mError = true;
        return;
      }

      mState = eComplete;

      return;
    }

    case eExpectingEmptyTokenOrDriveLetterOrPathnameComponent: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        mPathnameComponents.AppendElement(""_ns);

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      if (aToken.Length() == 1 && IsAsciiAlpha(aToken.First())) {
        mMaybeDriveLetter = true;

        mPathnameComponents.AppendElement(aToken);

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingEmptyTokenOrPathnameComponent: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        if (mMaybeDriveLetter) {
          MOZ_ASSERT(mPathnameComponents.Length() == 1);

          nsCString& pathnameComponent = mPathnameComponents[0];
          pathnameComponent.Append(':');

          mMaybeDriveLetter = false;
        } else {
          mPathnameComponents.AppendElement(""_ns);
        }

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingEmptyToken1OrHost: {
      MOZ_ASSERT(mSchemeType == eAbout &&
                 mScheme.EqualsLiteral("moz-safe-about"));

      if (aToken.IsEmpty()) {
        mState = eExpectingEmptyToken2;
      } else {
        mHost = aToken;
        mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;
      }

      return;
    }

    case eExpectingIPV6Token: {
      // A safe check for preventing infinity recursion.
      if (++mIPGroup > 8) {
        mError = true;
        return;
      }

      mHost.AppendLiteral(":");
      mHost.Append(aToken);
      if (!aToken.IsEmpty() && aToken.Last() == ']') {
        mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;
      }

      return;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }
}

void OriginParser::HandleTrailingSeparator() {
  MOZ_ASSERT(mState == eComplete);
  MOZ_ASSERT(mSchemeType == eFile);

  mPathnameComponents.AppendElement(""_ns);

  mState = eHandledTrailingSeparator;
}

bool IsUUIDOrigin(const nsCString& aOrigin) {
  if (!StringBeginsWith(aOrigin, kUUIDOriginScheme)) {
    return false;
  }

  static const std::regex pattern(
      "^uuid://[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-4[0-9A-Fa-f]{3}-[89ABab"
      "][0-9A-Fa-f]{3}-[0-9A-Fa-f]{12}$");

  return regex_match(aOrigin.get(), pattern);
}

}  // namespace mozilla::dom::quota
