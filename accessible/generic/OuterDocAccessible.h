/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_OUTERDOCACCESSIBLE_H_
#define MOZILLA_A11Y_OUTERDOCACCESSIBLE_H_

#include "AccessibleWrap.h"

namespace mozilla {

namespace dom {
class BrowserBridgeChild;
}

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

class OuterDocAccessible final : public AccessibleWrap {
 public:
  OuterDocAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(OuterDocAccessible, AccessibleWrap)

  DocAccessibleParent* RemoteChildDoc() const;

  /**
   * For iframes in a content process which will be rendered in another content
   * process, tell the parent process about this OuterDocAccessible
   * so it can link the trees together when the embedded document is added.
   * Note that an OuterDocAccessible can be created before the
   * BrowserBridgeChild or vice versa. Therefore, this must be conditionally
   * called when either of these is created.
   */
  void SendEmbedderAccessible(dom::BrowserBridgeChild* aBridge);

  Maybe<nsMargin> GetCrossProcOffset() { return mCrossProcOffset; }

  void SetCrossProcOffset(nsMargin aMargin) {
    mCrossProcOffset = Some(aMargin);
  }

  // LocalAccessible
  virtual void Shutdown() override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual LocalAccessible* LocalChildAtPoint(
      int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) override;

  virtual bool InsertChildAt(uint32_t aIdx, LocalAccessible* aChild) override;
  virtual bool RemoveChild(LocalAccessible* aAccessible) override;
  virtual bool IsAcceptableChild(nsIContent* aEl) const override;

  virtual uint32_t ChildCount() const override;

  // Accessible
  virtual Accessible* ChildAt(uint32_t aIndex) const override;
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild) override;

 protected:
  virtual ~OuterDocAccessible() override;
  Maybe<nsMargin> mCrossProcOffset;
};

inline OuterDocAccessible* LocalAccessible::AsOuterDoc() {
  return IsOuterDoc() ? static_cast<OuterDocAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
