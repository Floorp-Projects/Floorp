/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a CSS style sheet returned from nsIStyleSheetService.preloadSheet */

#ifndef mozilla_PreloadedStyleSheet_h
#define mozilla_PreloadedStyleSheet_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/NotNull.h"
#include "mozilla/Result.h"
#include "mozilla/StyleBackendType.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICSSLoaderObserver.h"
#include "nsIPreloadedStyleSheet.h"

class nsIURI;

namespace mozilla {
namespace dom {
  class Promise;
}

class StyleSheet;

class PreloadedStyleSheet : public nsIPreloadedStyleSheet
{
public:
  // *aResult is addrefed.
  static nsresult Create(nsIURI* aURI, css::SheetParsingMode aParsingMode,
                         PreloadedStyleSheet** aResult);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PreloadedStyleSheet)

  // *aResult is not addrefed, since the PreloadedStyleSheet holds a strong
  // reference to the sheet.
  nsresult GetSheet(StyleBackendType aType, StyleSheet** aResult);

  nsresult Preload();
  nsresult PreloadAsync(NotNull<dom::Promise*> aPromise);

protected:
  virtual ~PreloadedStyleSheet() {}

private:
  PreloadedStyleSheet(nsIURI* aURI, css::SheetParsingMode aParsingMode);

  class StylesheetPreloadObserver final : public nsICSSLoaderObserver
  {
  public:
    NS_DECL_ISUPPORTS

    explicit StylesheetPreloadObserver(NotNull<dom::Promise*> aPromise,
                                       PreloadedStyleSheet* aSheet)
      : mPromise(aPromise)
      , mPreloadedSheet(aSheet)
    {}

    NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet,
                                bool aWasAlternate,
                                nsresult aStatus) override;

  protected:
    virtual ~StylesheetPreloadObserver() {}

  private:
    RefPtr<dom::Promise> mPromise;
    RefPtr<PreloadedStyleSheet> mPreloadedSheet;
  };

  RefPtr<StyleSheet> mGecko;
  RefPtr<StyleSheet> mServo;

  bool mLoaded;
  nsCOMPtr<nsIURI> mURI;
  css::SheetParsingMode mParsingMode;
};

} // namespace mozilla

#endif // mozilla_PreloadedStyleSheet_h
