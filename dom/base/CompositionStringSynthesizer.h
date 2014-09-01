/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_compositionstringsynthesizer_h__
#define mozilla_dom_compositionstringsynthesizer_h__

#include "nsICompositionStringSynthesizer.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"
#include "mozilla/TextRange.h"

class nsIWidget;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class CompositionStringSynthesizer MOZ_FINAL :
  public nsICompositionStringSynthesizer
{
public:
  explicit CompositionStringSynthesizer(nsPIDOMWindow* aWindow);

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMPOSITIONSTRINGSYNTHESIZER

private:
  ~CompositionStringSynthesizer();

  nsWeakPtr mWindow; // refers an instance of nsPIDOMWindow
  nsString mString;
  nsRefPtr<TextRangeArray> mClauses;
  TextRange mCaret;

  nsIWidget* GetWidget();
  void ClearInternal();
};

} // namespace dom
} // namespace mozilla

#endif // #ifndef mozilla_dom_compositionstringsynthesizer_h__
