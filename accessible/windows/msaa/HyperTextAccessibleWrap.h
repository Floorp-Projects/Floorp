/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HyperTextAccessibleWrap_h__
#define mozilla_a11y_HyperTextAccessibleWrap_h__

#include "HyperTextAccessible.h"
#include "ia2AccessibleEditableText.h"
#include "ia2AccessibleHypertext.h"
#include "IUnknownImpl.h"

namespace mozilla {
template<class T> class StaticAutoPtr;
template<class T> class StaticRefPtr;

namespace a11y {

class HyperTextAccessibleWrap : public HyperTextAccessible,
                                public ia2AccessibleHypertext,
                                public ia2AccessibleEditableText
{
public:
  HyperTextAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
    HyperTextAccessible(aContent, aDoc) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual nsresult HandleAccEvent(AccEvent* aEvent);

protected:
  ~HyperTextAccessibleWrap() {}

  virtual nsresult GetModifiedText(bool aGetInsertedText, nsAString& aText,
                                   uint32_t *aStartOffset,
                                   uint32_t *aEndOffset);

  static StaticRefPtr<Accessible> sLastTextChangeAcc;
  static StaticAutoPtr<nsString> sLastTextChangeString;
  static bool sLastTextChangeWasInsert;
  static uint32_t sLastTextChangeStart;
  static uint32_t sLastTextChangeEnd;

  friend void PlatformInit();
};

} // namespace a11y
} // namespace mozilla

#endif
