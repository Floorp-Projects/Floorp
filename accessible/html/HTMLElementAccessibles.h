/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HTMLElementAccessibles_h__
#define mozilla_a11y_HTMLElementAccessibles_h__

#include "BaseAccessibles.h"

namespace mozilla {
namespace a11y {

/**
 * Used for HTML hr element.
 */
class HTMLHRAccessible : public LeafAccessible {
 public:
  HTMLHRAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : LeafAccessible(aContent, aDoc) {}

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
};

/**
 * Used for HTML br element.
 */
class HTMLBRAccessible : public LeafAccessible {
 public:
  HTMLBRAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : LeafAccessible(aContent, aDoc) {
    mType = eHTMLBRType;
    mGenericTypes |= eText;
  }

  // LocalAccessible
  virtual a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

 protected:
  // LocalAccessible
  virtual ENameValueFlag NativeName(nsString& aName) const override;
};

/**
 * Used for HTML label element.
 */
class HTMLLabelAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLLabelAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessibleWrap(aContent, aDoc) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLLabelAccessible,
                                       HyperTextAccessibleWrap)

  // LocalAccessible
  virtual Relation RelationByType(RelationType aType) const override;

  // ActionAccessible
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool HasPrimaryAction() const override;

 protected:
  virtual ~HTMLLabelAccessible() {}
  virtual ENameValueFlag NativeName(nsString& aName) const override;
  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;
};

/**
 * Used for HTML output element.
 */
class HTMLOutputAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLOutputAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessibleWrap(aContent, aDoc) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLOutputAccessible,
                                       HyperTextAccessibleWrap)

  // LocalAccessible
  virtual Relation RelationByType(RelationType aType) const override;

  virtual void DOMAttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType,
                                   const nsAttrValue* aOldValue,
                                   uint64_t aOldState) override;

 protected:
  virtual ~HTMLOutputAccessible() {}
};

/**
 * Accessible for the HTML summary element.
 */
class HTMLSummaryAccessible : public HyperTextAccessibleWrap {
 public:
  enum { eAction_Click = 0 };

  HTMLSummaryAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Check that the given LocalAccessible belongs to a details frame.
  // If so, find and return the accessible for the detail frame's
  // main summary.
  static HTMLSummaryAccessible* FromDetails(LocalAccessible* aDetails);

  // LocalAccessible
  virtual uint64_t NativeState() const override;

  // ActionAccessible
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool HasPrimaryAction() const override;

  // Widgets
  virtual bool IsWidget() const override;
};

/**
 * Used for HTML header and footer elements.
 */
class HTMLHeaderOrFooterAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLHeaderOrFooterAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessibleWrap(aContent, aDoc) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLHeaderOrFooterAccessible,
                                       HyperTextAccessibleWrap)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;

 protected:
  virtual ~HTMLHeaderOrFooterAccessible() {}
};

/**
 * Used for HTML section element.
 */
class HTMLSectionAccessible : public HyperTextAccessibleWrap {
 public:
  HTMLSectionAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : HyperTextAccessibleWrap(aContent, aDoc) {}

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HTMLSectionAccessible,
                                       HyperTextAccessibleWrap)

  // LocalAccessible
  virtual a11y::role NativeRole() const override;

 protected:
  virtual ~HTMLSectionAccessible() = default;
};

}  // namespace a11y
}  // namespace mozilla

#endif
