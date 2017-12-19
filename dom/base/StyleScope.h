/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StyleScope_h__
#define mozilla_dom_StyleScope_h__

#include "nsTArray.h"

class nsINode;

namespace mozilla {
class StyleSheet;

namespace dom {

class StyleSheetList;

/**
 * A class meant to be shared by ShadowRoot and Document, that holds a list of
 * stylesheets.
 *
 * TODO(emilio, bug 1418159): In the future this should hold most of the
 * relevant style state, this should allow us to fix bug 548397.
 */
class StyleScope
{
public:
  virtual nsINode& AsNode() = 0;

  const nsINode& AsNode() const
  {
    return const_cast<StyleScope&>(*this).AsNode();
  }

  StyleSheet* SheetAt(size_t aIndex) const
  {
    return mStyleSheets.SafeElementAt(aIndex);
  }

  size_t SheetCount() const
  {
    return mStyleSheets.Length();
  }

  int32_t IndexOfSheet(const StyleSheet& aSheet) const
  {
    return mStyleSheets.IndexOf(&aSheet);
  }

  void InsertSheetAt(size_t aIndex, StyleSheet& aSheet)
  {
    mStyleSheets.InsertElementAt(aIndex, &aSheet);
  }

  void RemoveSheet(StyleSheet& aSheet)
  {
    mStyleSheets.RemoveElement(&aSheet);
  }

  void AppendStyleSheet(StyleSheet& aSheet)
  {
    mStyleSheets.AppendElement(&aSheet);
  }

  StyleSheetList& EnsureDOMStyleSheets();

  ~StyleScope();

protected:
  nsTArray<RefPtr<mozilla::StyleSheet>> mStyleSheets;
  RefPtr<mozilla::dom::StyleSheetList> mDOMStyleSheets;
};

}

}

#endif
