/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_OUTERDOCACCESSIBLE_H_
#define MOZILLA_A11Y_OUTERDOCACCESSIBLE_H_

#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {
class DocAccessibleParent;

/**
 * Used for <browser>, <frame>, <iframe>, <page> or editor> elements.
 *
 * In these variable names, "outer" relates to the OuterDocAccessible as
 * opposed to the DocAccessibleWrap which is "inner". The outer node is
 * a something like tags listed above, whereas the inner node corresponds to
 * the inner document root.
 */

class OuterDocAccessible final : public AccessibleWrap
{
public:
  OuterDocAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(OuterDocAccessible, AccessibleWrap)

  DocAccessibleParent* RemoteChildDoc() const;

  // Accessible
  virtual void Shutdown() override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild) override;

  virtual bool InsertChildAt(uint32_t aIdx, Accessible* aChild) override;
  virtual bool RemoveChild(Accessible* aAccessible) override;
  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

#if defined(XP_WIN)
  virtual uint32_t ChildCount() const override;
  virtual Accessible* GetChildAt(uint32_t aIndex) const override;
#endif // defined(XP_WIN)

protected:
  virtual ~OuterDocAccessible() override;
};

inline OuterDocAccessible*
Accessible::AsOuterDoc()
{
  return IsOuterDoc() ? static_cast<OuterDocAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif
