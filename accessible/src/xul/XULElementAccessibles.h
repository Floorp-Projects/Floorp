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
  virtual void Shutdown();
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual Relation RelationByType(RelationType aType) MOZ_OVERRIDE;

  void UpdateLabelValue(const nsString& aValue);

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;
  virtual void CacheChildren() MOZ_OVERRIDE;

private:
  nsRefPtr<XULLabelTextLeafAccessible> mValueTextLeaf;
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
class XULLabelTextLeafAccessible MOZ_FINAL : public TextLeafAccessibleWrap
{
public:
  XULLabelTextLeafAccessible(nsIContent* aContent, DocAccessible* aDoc) :
    TextLeafAccessibleWrap(aContent, aDoc)
  { mStateFlags |= eSharedNode; }

  virtual ~XULLabelTextLeafAccessible() { }

  // Accessible
  virtual a11y::role NativeRole() MOZ_OVERRIDE;
  virtual uint64_t NativeState() MOZ_OVERRIDE;
};


/**
 * Used for XUL tooltip element.
 */
class XULTooltipAccessible : public LeafAccessible
{

public:
  XULTooltipAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // Accessible
  virtual a11y::role NativeRole();
  virtual uint64_t NativeState();
};

class XULLinkAccessible : public XULLabelAccessible
{

public:
  XULLinkAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessible
  NS_IMETHOD GetActionName(uint8_t aIndex, nsAString& aName);
  NS_IMETHOD DoAction(uint8_t aIndex);

  // Accessible
  virtual void Value(nsString& aValue);
  virtual a11y::role NativeRole();
  virtual uint64_t NativeLinkState() const;

  // ActionAccessible
  virtual uint8_t ActionCount();

  // HyperLinkAccessible
  virtual bool IsLink();
  virtual uint32_t StartOffset();
  virtual uint32_t EndOffset();
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex);

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;

  enum { eAction_Jump = 0 };

};

} // namespace a11y
} // namespace mozilla

#endif
