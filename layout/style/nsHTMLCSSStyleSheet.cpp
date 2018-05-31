/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing style attributes
 */

#include "nsHTMLCSSStyleSheet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/DeclarationBlock.h"
#include "nsPresContext.h"
#include "mozilla/dom/Element.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsCSSPseudoElements.h"
#include "mozilla/RestyleManager.h"

using namespace mozilla;
using namespace mozilla::dom;

nsHTMLCSSStyleSheet::nsHTMLCSSStyleSheet()
{
}

nsHTMLCSSStyleSheet::~nsHTMLCSSStyleSheet()
{
  // We may go away before all of our cached style attributes do,
  // so clean up any that are left.
  for (auto iter = mCachedStyleAttrs.Iter(); !iter.Done(); iter.Next()) {
    MiscContainer*& value = iter.Data();

    // Ideally we'd just call MiscContainer::Evict, but we can't do that since
    // we're iterating the hashtable.
    if (value->mType == nsAttrValue::eCSSDeclaration) {
      DeclarationBlock* declaration = value->mValue.mCSSDeclaration;
      declaration->SetHTMLCSSStyleSheet(nullptr);
    } else {
      MOZ_ASSERT_UNREACHABLE("unexpected cached nsAttrValue type");
    }

    value->mValue.mCached = 0;
    iter.Remove();
  }
}


void
nsHTMLCSSStyleSheet::CacheStyleAttr(const nsAString& aSerialized,
                                    MiscContainer* aValue)
{
  mCachedStyleAttrs.Put(aSerialized, aValue);
}

void
nsHTMLCSSStyleSheet::EvictStyleAttr(const nsAString& aSerialized,
                                    MiscContainer* aValue)
{
#ifdef DEBUG
  {
    NS_ASSERTION(aValue == mCachedStyleAttrs.Get(aSerialized),
                 "Cached value does not match?!");
  }
#endif
  mCachedStyleAttrs.Remove(aSerialized);
}

MiscContainer*
nsHTMLCSSStyleSheet::LookupStyleAttr(const nsAString& aSerialized)
{
  return mCachedStyleAttrs.Get(aSerialized);
}
