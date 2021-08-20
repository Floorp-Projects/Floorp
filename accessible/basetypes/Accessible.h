/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _Accessible_H_
#define _Accessible_H_

#include "mozilla/a11y/Role.h"
#include "mozilla/a11y/AccTypes.h"
#include "nsString.h"

class nsAtom;

struct nsRoleMapEntry;

namespace mozilla {
namespace a11y {

class LocalAccessible;
class RemoteAccessible;

/**
 * Name type flags.
 */
enum ENameValueFlag {
  /**
   * Name either
   *  a) present (not empty): !name.IsEmpty()
   *  b) no name (was missed): name.IsVoid()
   */
  eNameOK,

  /**
   * Name was left empty by the author on purpose:
   * name.IsEmpty() && !name.IsVoid().
   */
  eNoNameOnPurpose,

  /**
   * Name was computed from the subtree.
   */
  eNameFromSubtree,

  /**
   * Tooltip was used as a name.
   */
  eNameFromTooltip
};

class Accessible {
 protected:
  Accessible();

  Accessible(AccType aType, AccGenericType aGenericTypes,
             uint8_t aRoleMapEntryIndex);

 public:
  virtual Accessible* Parent() const = 0;

  virtual role Role() const = 0;

  /**
   * Return child accessible at the given index.
   */
  virtual Accessible* ChildAt(uint32_t aIndex) const = 0;

  virtual Accessible* NextSibling() const = 0;
  virtual Accessible* PrevSibling() const = 0;

  virtual uint32_t ChildCount() const = 0;

  virtual int32_t IndexInParent() const = 0;

  bool HasChildren() const { return !!FirstChild(); }

  inline Accessible* FirstChild() const {
    return ChildCount() ? ChildAt(0) : nullptr;
  }

  inline Accessible* LastChild() const {
    uint32_t childCount = ChildCount();
    return childCount ? ChildAt(childCount - 1) : nullptr;
  }

  /**
   * Used by ChildAtPoint() method to get direct or deepest child at point.
   */
  enum class EWhichChildAtPoint { DirectChild, DeepestChild };

  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild) = 0;

  /**
   * Return ARIA role map if any.
   */
  const nsRoleMapEntry* ARIARoleMap() const;

  /**
   * Return true if ARIA role is specified on the element.
   */
  bool HasARIARole() const;
  bool IsARIARole(nsAtom* aARIARole) const;
  bool HasStrongARIARole() const;

  /**
   * Return true if the accessible belongs to the given accessible type.
   */
  bool HasGenericType(AccGenericType aType) const;

  // Methods that potentially access a cache.

  /*
   * Get the name of this accessible.
   */
  virtual ENameValueFlag Name(nsString& aName) const = 0;

  // Type "is" methods

  bool IsDoc() const { return HasGenericType(eDocument); }

  bool IsTableRow() const { return HasGenericType(eTableRow); }

  /**
   * Note: The eTable* types defined in the ARIA map are used in
   * nsAccessibilityService::CreateAccessible to determine which ARIAGrid*
   * classes to use for accessible object creation. However, an invalid table
   * structure might cause these classes not to be used after all.
   *
   * To make sure we're really dealing with a table cell, only check the
   * generic type defined by the class, not the type defined in the ARIA map.
   */
  bool IsTableCell() const { return mGenericTypes & eTableCell; }

  bool IsTable() const { return HasGenericType(eTable); }

  bool IsHyperText() const { return HasGenericType(eHyperText); }

  bool IsSelect() const { return HasGenericType(eSelect); }

  bool IsActionable() const { return HasGenericType(eActionable); }

  bool IsText() const { return mGenericTypes & eText; }

  bool IsImage() const { return mType == eImageType; }

  bool IsApplication() const { return mType == eApplicationType; }

  bool IsAlert() const { return HasGenericType(eAlert); }

  bool IsAutoComplete() const { return HasGenericType(eAutoComplete); }

  bool IsButton() const { return HasGenericType(eButton); }

  bool IsCombobox() const { return HasGenericType(eCombobox); }

  virtual bool IsLink() const = 0;

  /**
   * Return true if the used ARIA role (if any) allows the hypertext accessible
   * to expose text interfaces.
   */
  bool IsTextRole();

  bool IsGenericHyperText() const { return mType == eHyperTextType; }

  bool IsHTMLBr() const { return mType == eHTMLBRType; }
  bool IsHTMLCaption() const { return mType == eHTMLCaptionType; }
  bool IsHTMLCombobox() const { return mType == eHTMLComboboxType; }
  bool IsHTMLFileInput() const { return mType == eHTMLFileInputType; }

  bool IsHTMLListItem() const { return mType == eHTMLLiType; }

  bool IsHTMLLink() const { return mType == eHTMLLinkType; }

  bool IsHTMLOptGroup() const { return mType == eHTMLOptGroupType; }

  bool IsHTMLTable() const { return mType == eHTMLTableType; }
  bool IsHTMLTableRow() const { return mType == eHTMLTableRowType; }

  bool IsImageMap() const { return mType == eImageMapType; }

  bool IsList() const { return HasGenericType(eList); }

  bool IsListControl() const { return HasGenericType(eListControl); }

  bool IsMenuButton() const { return HasGenericType(eMenuButton); }

  bool IsMenuPopup() const { return mType == eMenuPopupType; }

  bool IsProxy() const { return mType == eProxyType; }

  bool IsOuterDoc() const { return mType == eOuterDocType; }

  bool IsProgress() const { return mType == eProgressType; }

  bool IsRoot() const { return mType == eRootType; }

  bool IsPassword() const { return mType == eHTMLTextPasswordFieldType; }

  bool IsTextLeaf() const { return mType == eTextLeafType; }

  bool IsXULLabel() const { return mType == eXULLabelType; }

  bool IsXULListItem() const { return mType == eXULListItemType; }

  bool IsXULTabpanels() const { return mType == eXULTabpanelsType; }

  bool IsXULTooltip() const { return mType == eXULTooltipType; }

  bool IsXULTree() const { return mType == eXULTreeType; }

  bool IsAutoCompletePopup() const {
    return HasGenericType(eAutoCompletePopup);
  }

  bool IsTextField() const {
    return mType == eHTMLTextFieldType || mType == eHTMLTextPasswordFieldType;
  }

  virtual bool HasNumericValue() const = 0;

  // Remote/Local types

  virtual bool IsRemote() const = 0;
  RemoteAccessible* AsRemote();

  bool IsLocal() { return !IsRemote(); }
  LocalAccessible* AsLocal();

 private:
  static const uint8_t kTypeBits = 6;
  static const uint8_t kGenericTypesBits = 18;

  void StaticAsserts() const;

 protected:
  uint32_t mType : kTypeBits;
  uint32_t mGenericTypes : kGenericTypesBits;
  uint8_t mRoleMapEntryIndex;

  friend class DocAccessibleChildBase;
};

}  // namespace a11y
}  // namespace mozilla

#endif
