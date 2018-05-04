/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULElementAccessibles_h__
#define mozilla_a11y_XULElementAccessibles_h__

#include "HyperTextAccessibleWrap.h"
#include "TextLeafAccessibleWrap.h"

namespace mozilla {
namespace a11y {

class XULLabelTextLeafAccessible;

/**
 * Used for XUL description and label elements.
 */
class XULLabelAccessible : public HyperTextAccessibleWrap
{
public:
  XULLabelAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Shutdown() override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
  virtual Relation RelationByType(RelationType aType) override;

  void UpdateLabelValue(const nsString& aValue);

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;

private:
  RefPtr<XULLabelTextLeafAccessible> mValueTextLeaf;
};

inline XULLabelAccessible*
Accessible::AsXULLabel()
{
  return IsXULLabel() ? static_cast<XULLabelAccessible*>(this) : nullptr;
}


/**
 * Used to implement text interface on XUL label accessible in case when text
 * is provided by @value attribute (no underlying text frame).
 */
class XULLabelTextLeafAccessible final : public TextLeafAccessibleWrap
{
public:
  XULLabelTextLeafAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    TextLeafAccessibleWrap(aContent, aDoc)
  { mStateFlags |= eSharedNode; }

  virtual ~XULLabelTextLeafAccessible() { }

  // Accessible
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
};


/**
 * Used for XUL tooltip element.
 */
class XULTooltipAccessible : public LeafAccessible
{

public:
  XULTooltipAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeState() override;
};

class XULLinkAccessible : public XULLabelAccessible
{

public:
  XULLinkAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual void Value(nsString& aValue) override;
  virtual a11y::role NativeRole() override;
  virtual uint64_t NativeLinkState() const override;

  // ActionAccessible
  virtual uint8_t ActionCount() override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool DoAction(uint8_t aIndex) override;

  // HyperLinkAccessible
  virtual bool IsLink() const override;
  virtual uint32_t StartOffset() override;
  virtual uint32_t EndOffset() override;
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex) override;

protected:
  virtual ~XULLinkAccessible();

  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) override;

  enum { eAction_Jump = 0 };

};

} // namespace a11y
} // namespace mozilla

#endif
