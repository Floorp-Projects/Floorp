/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a CSS style sheet returned from nsIStyleSheetService.preloadSheet */

#ifndef mozilla_PreloadedStyleSheet_h
#define mozilla_PreloadedStyleSheet_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/NotNull.h"
#include "mozilla/Result.h"
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

class PreloadedStyleSheet : public nsIPreloadedStyleSheet {
 public:
  PreloadedStyleSheet(nsIURI*, css::SheetParsingMode);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PreloadedStyleSheet)

  // *aResult is not addrefed, since the PreloadedStyleSheet holds a strong
  // reference to the sheet.
  nsresult GetSheet(StyleSheet** aResult);

  nsresult Preload();
  nsresult PreloadAsync(NotNull<dom::Promise*> aPromise);

 protected:
  virtual ~PreloadedStyleSheet() {}

 private:
  class StylesheetPreloadObserver final : public nsICSSLoaderObserver {
   public:
    NS_DECL_ISUPPORTS

    explicit StylesheetPreloadObserver(NotNull<dom::Promise*> aPromise,
                                       PreloadedStyleSheet* aSheet)
        : mPromise(aPromise), mPreloadedSheet(aSheet) {}

    NS_IMETHOD StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                                nsresult aStatus) override;

   protected:
    virtual ~StylesheetPreloadObserver() {}

   private:
    RefPtr<dom::Promise> mPromise;
    RefPtr<PreloadedStyleSheet> mPreloadedSheet;
  };

  RefPtr<StyleSheet> mSheet;

  bool mLoaded;
  nsCOMPtr<nsIURI> mURI;
  css::SheetParsingMode mParsingMode;
};

}  // namespace mozilla

#endif  // mozilla_PreloadedStyleSheet_h
