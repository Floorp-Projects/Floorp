/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a CSS style sheet returned from nsIStyleSheetService.preloadSheet */

#include "PreloadedStyleSheet.h"

#include "mozilla/css/Loader.h"
#include "mozilla/dom/Promise.h"
#include "nsICSSLoaderObserver.h"
#include "nsLayoutUtils.h"

namespace mozilla {

PreloadedStyleSheet::PreloadedStyleSheet(nsIURI* aURI,
                                         css::SheetParsingMode aParsingMode)
  : mLoaded(false)
  , mURI(aURI)
  , mParsingMode(aParsingMode)
{
}

/* static */ nsresult
PreloadedStyleSheet::Create(nsIURI* aURI,
                            css::SheetParsingMode aParsingMode,
                            PreloadedStyleSheet** aResult)
{
  *aResult = nullptr;

  RefPtr<PreloadedStyleSheet> preloadedSheet =
    new PreloadedStyleSheet(aURI, aParsingMode);

  preloadedSheet.forget(aResult);
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PreloadedStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIPreloadedStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PreloadedStyleSheet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PreloadedStyleSheet)

NS_IMPL_CYCLE_COLLECTION(PreloadedStyleSheet, mGecko, mServo)

nsresult
PreloadedStyleSheet::GetSheet(StyleBackendType aType, StyleSheet** aResult)
{
  *aResult = nullptr;

  MOZ_DIAGNOSTIC_ASSERT(mLoaded);

  RefPtr<StyleSheet>& sheet =
    aType == StyleBackendType::Gecko ? mGecko : mServo;

  if (!sheet) {
    RefPtr<css::Loader> loader = new css::Loader(aType, nullptr);
    nsresult rv = loader->LoadSheetSync(mURI, mParsingMode, true, &sheet);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(sheet);
  }

  *aResult = sheet;
  return NS_OK;
}

nsresult
PreloadedStyleSheet::Preload()
{
  MOZ_DIAGNOSTIC_ASSERT(!mLoaded);

  // The nsIStyleSheetService.preloadSheet API doesn't tell us which backend
  // the sheet will be used with, and it seems wasteful to eagerly create
  // both a CSSStyleSheet and a ServoStyleSheet.  So instead, we guess that
  // the sheet type we will want matches the current value of the stylo pref,
  // and preload a sheet of that type.
  //
  // If we guess wrong, we will re-load the sheet later with the requested type,
  // and we won't really have front loaded the loading time as the name
  // "preload" might suggest.  Also, in theory we could get different data from
  // fetching the URL again, but for the usage patterns of this API this is
  // unlikely, and it doesn't seem worth trying to store the contents of the URL
  // and duplicating a bunch of css::Loader's logic.
  auto type = nsLayoutUtils::StyloEnabled() ? StyleBackendType::Servo
                                            : StyleBackendType::Gecko;

  mLoaded = true;

  StyleSheet* sheet;
  return GetSheet(type, &sheet);
}

NS_IMPL_ISUPPORTS(PreloadedStyleSheet::StylesheetPreloadObserver,
                  nsICSSLoaderObserver)

NS_IMETHODIMP
PreloadedStyleSheet::StylesheetPreloadObserver::StyleSheetLoaded(
  StyleSheet* aSheet, bool aWasAlternate, nsresult aStatus)
{
  MOZ_DIAGNOSTIC_ASSERT(!mPreloadedSheet->mLoaded);
  mPreloadedSheet->mLoaded = true;

  if (NS_FAILED(aStatus)) {
    mPromise->MaybeReject(aStatus);
  } else {
    mPromise->MaybeResolve(mPreloadedSheet);
  }

  return NS_OK;
}

// Note: After calling this method, the preloaded sheet *must not* be used
// until the observer is notified that the sheet has finished loading.
nsresult
PreloadedStyleSheet::PreloadAsync(NotNull<dom::Promise*> aPromise)
{
  MOZ_DIAGNOSTIC_ASSERT(!mLoaded);

  // As with the Preload() method, we can't be sure that the sheet will only be
  // used with the backend that we're preloading it for now. If it's used with
  // a different backend later, it will be synchronously loaded for that
  // backend the first time it's used.
  auto type = nsLayoutUtils::StyloEnabled() ? StyleBackendType::Servo
                                            : StyleBackendType::Gecko;

  RefPtr<StyleSheet>& sheet =
    type == StyleBackendType::Gecko ? mGecko : mServo;

  RefPtr<css::Loader> loader = new css::Loader(type, nullptr);

  RefPtr<StylesheetPreloadObserver> obs =
    new StylesheetPreloadObserver(aPromise, this);

  return loader->LoadSheet(mURI, mParsingMode, false, obs, &sheet);
}

} // namespace mozilla
