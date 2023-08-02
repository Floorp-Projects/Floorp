/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLTableAccessible_h__
#define mozilla_a11y_HTMLTableAccessible_h__

#include "HyperTextAccessible.h"

class nsITableCellLayout;
class nsTableCellFrame;
class nsTableWrapperFrame;

namespace mozilla {

namespace a11y {

class HTMLTableAccessible;

/**
 * HTML table cell accessible (html:td).
 */
class HTMLTableCellAccessible : public HyperTextAccessible {
 public:
  HTMLTableCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // nsISupports
  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLTableCellAccessible,
                                       HyperTextAccessible)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;

 protected:
  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;
  // HTMLTableCellAccessible
 public:
  HTMLTableAccessible* Table() const;
  uint32_t ColExtent() const;
  uint32_t RowExtent() const;

  static HTMLTableCellAccessible* GetFrom(LocalAccessible* aAcc) {
    if (aAcc->IsHTMLTableCell()) {
      return static_cast<HTMLTableCellAccessible*>(aAcc);
    }
    return nullptr;
  }

 protected:
  virtual ~HTMLTableCellAccessible() {}
};

/**
 * HTML table row/column header accessible (html:th or html:td@scope).
 */
class HTMLTableHeaderCellAccessible : public HTMLTableCellAccessible {
 public:
  HTMLTableHeaderCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
};

/**
 * HTML table row accessible (html:tr).
 */
class HTMLTableRowAccessible : public HyperTextAccessible {
 public:
  HTMLTableRowAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessible(aContent, aDoc) {
    mType = eHTMLTableRowType;
    mGenericTypes |= eTableRow;
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLTableRowAccessible,
                                       HyperTextAccessible)

 protected:
  virtual ~HTMLTableRowAccessible() {}

  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

/**
 * HTML table accessible (html:table).
 */

// To turn on table debugging descriptions define SHOW_LAYOUT_HEURISTIC
// This allow release trunk builds to be used by testers to refine the
// data vs. layout heuristic
// #define SHOW_LAYOUT_HEURISTIC

class HTMLTableAccessible : public HyperTextAccessible {
 public:
  HTMLTableAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessible(aContent, aDoc) {
    mType = eHTMLTableType;
    mGenericTypes |= eTable;
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLTableAccessible, HyperTextAccessible)

  // HTMLTableAccessible
  LocalAccessible* Caption() const;
  uint32_t ColCount() const;
  uint32_t RowCount();
  bool IsProbablyLayoutTable();

  static HTMLTableAccessible* GetFrom(LocalAccessible* aAcc) {
    if (aAcc->IsHTMLTable()) {
      return static_cast<HTMLTableAccessible*>(aAcc);
    }
    return nullptr;
  }

  // LocalAccessible
  virtual void Description(nsString& aDescription) const override;
  virtual uint64_t NativeState() const override;
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;
  virtual Relation RelationByType(RelationType aRelationType) const override;

  virtual bool InsertChildAt(uint32_t aIndex, LocalAccessible* aChild) override;

 protected:
  virtual ~HTMLTableAccessible() {}

  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;

  // HTMLTableAccessible

#ifdef SHOW_LAYOUT_HEURISTIC
  nsString mLayoutHeuristic;
#endif

 private:
  /**
   * Get table wrapper frame, or return null if there is no inner table.
   */
  nsTableWrapperFrame* GetTableWrapperFrame() const;
};

/**
 * HTML caption accessible (html:caption).
 */
class HTMLCaptionAccessible : public HyperTextAccessible {
 public:
  HTMLCaptionAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessible(aContent, aDoc) {
    mType = eHTMLCaptionType;
  }

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual Relation RelationByType(RelationType aRelationType) const override;

 protected:
  virtual ~HTMLCaptionAccessible() {}
};

}  // namespace a11y
}  // namespace mozilla

#endif
