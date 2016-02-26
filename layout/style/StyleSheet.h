/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheet_h
#define mozilla_StyleSheet_h

#include "mozilla/css/SheetParsingMode.h"

class nsINode;

namespace mozilla {

/**
 * Superclass for data common to CSSStyleSheet and ServoStyleSheet.
 */
class StyleSheet
{
public:
  StyleSheet();
  StyleSheet(const StyleSheet& aCopy,
             nsINode* aOwningNodeToUse);

  void SetOwningNode(nsINode* aOwningNode)
  {
    mOwningNode = aOwningNode;
  }

  void SetParsingMode(css::SheetParsingMode aParsingMode)
  {
    mParsingMode = aParsingMode;
  }

  nsINode* GetOwnerNode() const { return mOwningNode; }

protected:
  nsINode*              mOwningNode; // weak ref
  css::SheetParsingMode mParsingMode;
  bool                  mDisabled;
};

} // namespace mozilla

#endif // mozilla_StyleSheet_h
