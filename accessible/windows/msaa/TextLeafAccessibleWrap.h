/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextLeafAccessibleWrap_h__
#define mozilla_a11y_TextLeafAccessibleWrap_h__

#include "TextLeafAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Wrap TextLeafAccessible to expose ISimpleDOMText as a native interface with
 * a tear off.
 */
class TextLeafAccessibleWrap : public TextLeafAccessible
{
public:
  TextLeafAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    TextLeafAccessible(aContent, aDoc) { }
  virtual ~TextLeafAccessibleWrap() {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED
};

} // namespace a11y
} // namespace mozilla

#endif
