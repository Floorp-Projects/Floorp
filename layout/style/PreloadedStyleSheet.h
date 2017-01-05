/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=2:expandtab:shiftwidth=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a CSS style sheet returned from nsIStyleSheetService.preloadSheet */

#ifndef mozilla_PreloadedStyleSheet_h
#define mozilla_PreloadedStyleSheet_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/Result.h"
#include "nsIPreloadedStyleSheet.h"

namespace mozilla {

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

protected:
  virtual ~PreloadedStyleSheet() {}

private:
  PreloadedStyleSheet(nsIURI* aURI, css::SheetParsingMode aParsingMode);

  RefPtr<StyleSheet> mGecko;
  RefPtr<StyleSheet> mServo;

  nsCOMPtr<nsIURI> mURI;
  css::SheetParsingMode mParsingMode;
};

} // namespace mozilla

#endif // mozilla_PreloadedStyleSheet_h
