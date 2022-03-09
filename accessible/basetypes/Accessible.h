/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _Accessible_H_
#define _Accessible_H_

#include "mozilla/a11y/Role.h"
#include "mozilla/a11y/AccTypes.h"
#include "nsString.h"
#include "nsRect.h"
#include "Units.h"

class nsAtom;

struct nsRoleMapEntry;

namespace mozilla {
namespace a11y {

class AccAttributes;
class AccGroupInfo;
class HyperTextAccessibleBase;
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

/**
 * Group position (level, position in set and set size).
 */
struct GroupPos {
  GroupPos() : level(0), posInSet(0), setSize(0) {}
  GroupPos(int32_t aLevel, int32_t aPosInSet, int32_t aSetSize)
      : level(aLevel), posInSet(aPosInSet), setSize(aSetSize) {}

  int32_t level;
  int32_t posInSet;
  int32_t setSize;
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
   * Return true if this Accessible is before another Accessible in the tree.
   */
  bool IsBefore(const Accessible* aAcc) const;

  bool IsAncestorOf(const Accessible* aAcc) const {
    for (const Accessible* parent = aAcc->Parent(); parent;
         parent = parent->Parent()) {
      if (parent == this) {
        return true;
      }
    }
    return false;
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

  /**
   * Return group position (level, position in set and set size).
   */
  virtual GroupPos GroupPosition();

  /**
   * Return embedded accessible child at the given index.
   */
  virtual Accessible* EmbeddedChildAt(uint32_t aIndex) = 0;

  /**
   * Return index of the given embedded accessible child.
   */
  virtual int32_t IndexOfEmbeddedChild(Accessible* aChild) = 0;

  // Methods that potentially access a cache.

  /*
   * Get the name of this accessible.
   */
  virtual ENameValueFlag Name(nsString& aName) const = 0;

  /*
   * Get the description of this accessible.
   */
  virtual void Description(nsString& aDescription) const = 0;

  /**
   * Get the value of this accessible.
   */
  virtual void Value(nsString& aValue) const = 0;

  virtual double CurValue() const = 0;
  virtual double MinValue() const = 0;
  virtual double MaxValue() const = 0;
  virtual double Step() const = 0;

  /**
   * Return boundaries in screen coordinates in device pixels.
   */
  virtual LayoutDeviceIntRect Bounds() const = 0;

  /**
   * Returns text of accessible if accessible has text role otherwise empty
   * string.
   *
   * @param aText         [in] returned text of the accessible
   * @param aStartOffset  [in, optional] start offset inside of the accessible,
   *                        if missed entire text is appended
   * @param aLength       [in, optional] required length of text, if missed
   *                        then text from start offset till the end is appended
   */
  virtual void AppendTextTo(nsAString& aText, uint32_t aStartOffset = 0,
                            uint32_t aLength = UINT32_MAX) = 0;

  /**
   * Return all states of accessible (including ARIA states).
   */
  virtual uint64_t State() = 0;

  /**
   * Return the start offset of the embedded object within the parent
   * HyperTextAccessibleBase.
   */
  virtual uint32_t StartOffset();

  /**
   * Return the end offset of the link within the parent
   * HyperTextAccessibleBase.
   */
  virtual uint32_t EndOffset();

  /**
   * Return object attributes for the accessible.
   */
  virtual already_AddRefed<AccAttributes> Attributes() = 0;

  virtual already_AddRefed<nsAtom> DisplayStyle() const = 0;

  // Methods that interact with content.

  virtual void TakeFocus() const = 0;

  /**
   * Return tag name of associated DOM node.
   */
  virtual nsAtom* TagName() const = 0;

  /**
   * Return a landmark role if applied.
   */
  virtual nsAtom* LandmarkRole() const;

  //////////////////////////////////////////////////////////////////////////////
  // ActionAccessible

  /**
   * Return the number of actions that can be performed on this accessible.
   */
  virtual uint8_t ActionCount() const = 0;

  /**
   * Return action name at given index.
   */
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) = 0;

  /**
   * Default to localized action name.
   */
  void ActionDescriptionAt(uint8_t aIndex, nsAString& aDescription) {
    nsAutoString name;
    ActionNameAt(aIndex, name);
    TranslateString(name, aDescription);
  }

  /**
   * Invoke the accessible action.
   */
  virtual bool DoAction(uint8_t aIndex) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  // SelectAccessible

  /**
   * Return an array of selected items.
   */
  virtual void SelectedItems(nsTArray<Accessible*>* aItems) = 0;

  /**
   * Return the number of selected items.
   */
  virtual uint32_t SelectedItemCount() = 0;

  /**
   * Return selected item at the given index.
   */
  virtual Accessible* GetSelectedItem(uint32_t aIndex) = 0;

  /**
   * Determine if item at the given index is selected.
   */
  virtual bool IsItemSelected(uint32_t aIndex) = 0;

  /**
   * Add item at the given index the selection. Return true if success.
   */
  virtual bool AddItemToSelection(uint32_t aIndex) = 0;

  /**
   * Remove item at the given index from the selection. Return if success.
   */
  virtual bool RemoveItemFromSelection(uint32_t aIndex) = 0;

  /**
   * Select all items. Return true if success.
   */
  virtual bool SelectAll() = 0;

  /**
   * Unselect all items. Return true if success.
   */
  virtual bool UnselectAll() = 0;

  virtual void TakeSelection() = 0;

  virtual void SetSelected(bool aSelect) = 0;

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

  bool IsDateTimeField() const { return mType == eHTMLDateTimeFieldType; }

  virtual bool HasNumericValue() const = 0;

  // Remote/Local types

  virtual bool IsRemote() const = 0;
  RemoteAccessible* AsRemote();

  bool IsLocal() const { return !IsRemote(); }
  LocalAccessible* AsLocal();

  virtual HyperTextAccessibleBase* AsHyperTextBase() { return nullptr; }

  /**
   * Return the localized string for the given key.
   */
  static void TranslateString(const nsString& aKey, nsAString& aStringOut);

 protected:
  // Some abstracted group utility methods.

  /**
   * Get ARIA group attributes.
   */
  virtual void ARIAGroupPosition(int32_t* aLevel, int32_t* aSetSize,
                                 int32_t* aPosInSet) const = 0;

  /**
   * Return group info if there is an up-to-date version.
   */
  virtual AccGroupInfo* GetGroupInfo() const = 0;

  /**
   * Return group info or create and update.
   */
  virtual AccGroupInfo* GetOrCreateGroupInfo() = 0;

  /*
   * Return calculated group level based on accessible hierarchy.
   *
   * @param aFast  [in] Don't climb up tree. Calculate level from aria and
   *                    roles.
   */
  virtual int32_t GetLevel(bool aFast) const;

  /**
   * Calculate position in group and group size ('posinset' and 'setsize') based
   * on accessible hierarchy.
   *
   * @param  aPosInSet  [out] accessible position in the group
   * @param  aSetSize   [out] the group size
   */
  virtual void GetPositionAndSetSize(int32_t* aPosInSet, int32_t* aSetSize);

  /**
   * Return the nearest ancestor that has a primary action, or null.
   */
  const Accessible* ActionAncestor() const;

  /**
   * Return true if accessible has a primary action directly related to it, like
   * "click", "activate", "press", "jump", "open", "close", etc. A non-primary
   * action would be a complementary one like "showlongdesc".
   * If an accessible has an action that is associated with an ancestor, it is
   * not a primary action either.
   */
  virtual bool HasPrimaryAction() const = 0;

 private:
  static const uint8_t kTypeBits = 6;
  static const uint8_t kGenericTypesBits = 18;

  void StaticAsserts() const;

 protected:
  uint32_t mType : kTypeBits;
  uint32_t mGenericTypes : kGenericTypesBits;
  uint8_t mRoleMapEntryIndex;

  friend class DocAccessibleChildBase;
  friend class AccGroupInfo;
};

}  // namespace a11y
}  // namespace mozilla

#endif
