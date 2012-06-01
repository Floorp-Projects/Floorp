/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextLeafAccessible_h__
#define mozilla_a11y_TextLeafAccessible_h__

#include "nsBaseWidgetAccessible.h"

namespace mozilla {
namespace a11y {
 
/**
 * Generic class used for text nodes.
 */
class TextLeafAccessible : public nsLinkableAccessible
{
public:
  TextLeafAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~TextLeafAccessible();

  // Accessible
  virtual mozilla::a11y::role NativeRole();
  virtual void AppendTextTo(nsAString& aText, PRUint32 aStartOffset = 0,
                            PRUint32 aLength = PR_UINT32_MAX);
  virtual ENameValueFlag Name(nsString& aName);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties* aAttributes);

  // TextLeafAccessible
  void SetText(const nsAString& aText) { mText = aText; }
  const nsString& Text() const { return mText; }

protected:
  // Accessible
  virtual void CacheChildren();

protected:
  nsString mText;
};

} // namespace a11y
} // namespace mozilla

////////////////////////////////////////////////////////////////////////////////
// Accessible downcast method

inline mozilla::a11y::TextLeafAccessible*
Accessible::AsTextLeaf()
{
  return mFlags & eTextLeafAccessible ?
    static_cast<mozilla::a11y::TextLeafAccessible*>(this) : nsnull;
}

#endif

