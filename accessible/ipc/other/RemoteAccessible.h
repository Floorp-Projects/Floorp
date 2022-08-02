/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessible_h
#define mozilla_a11y_RemoteAccessible_h

#include "LocalAccessible.h"
#include "mozilla/a11y/RemoteAccessibleBase.h"
#include "mozilla/a11y/Role.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRect.h"

namespace mozilla {
namespace a11y {

class RemoteAccessible : public RemoteAccessibleBase<RemoteAccessible> {
 public:
  RemoteAccessible(uint64_t aID, RemoteAccessible* aParent,
                   DocAccessibleParent* aDoc, role aRole, AccType aType,
                   AccGenericType aGenericTypes, uint8_t aRoleMapEntryIndex)
      : RemoteAccessibleBase(aID, aParent, aDoc, aRole, aType, aGenericTypes,
                             aRoleMapEntryIndex) {
    MOZ_COUNT_CTOR(RemoteAccessible);
  }

  MOZ_COUNTED_DTOR(RemoteAccessible)

#include "mozilla/a11y/RemoteAccessibleShared.h"

  virtual uint32_t CharacterCount() const override;

  LayoutDeviceIntRect TextBounds(
      int32_t aStartOffset, int32_t aEndOffset,
      uint32_t aCoordType =
          nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE) override;

  LayoutDeviceIntRect CharBounds(int32_t aOffset, uint32_t aCoordType) override;

  virtual already_AddRefed<AccAttributes> TextAttributes(
      bool aIncludeDefAttrs, int32_t aOffset, int32_t* aStartOffset,
      int32_t* aEndOffset) override;

  virtual already_AddRefed<AccAttributes> DefaultTextAttributes() override;

  virtual uint32_t StartOffset() override;
  virtual uint32_t EndOffset() override;

  virtual int32_t LinkIndexAtOffset(uint32_t aOffset) override;

  virtual bool DoAction(uint8_t aIndex) const override;
  virtual uint8_t ActionCount() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual KeyBinding AccessKey() const override;

  virtual void SelectedItems(nsTArray<Accessible*>* aSelectedItems) override;
  virtual uint32_t SelectedItemCount() override;
  virtual Accessible* GetSelectedItem(uint32_t aIndex) override;
  virtual bool IsItemSelected(uint32_t aIndex) override;
  virtual bool AddItemToSelection(uint32_t aIndex) override;
  virtual bool RemoveItemFromSelection(uint32_t aIndex) override;
  virtual bool SelectAll() override;
  virtual bool UnselectAll() override;

  virtual nsAtom* LandmarkRole() const override;

  virtual int32_t SelectionCount() override;

  using RemoteAccessibleBase<RemoteAccessible>::SelectionBoundsAt;
  bool SelectionBoundsAt(int32_t aSelectionNum, nsString& aData,
                         int32_t* aStartOffset, int32_t* aEndOffset);

  virtual bool TableIsProbablyForLayout() override;

  /**
   * Get all relations for this accessible.
   */
  void Relations(nsTArray<RelationType>* aTypes,
                 nsTArray<nsTArray<RemoteAccessible*>>* aTargetSets) const;

 protected:
  explicit RemoteAccessible(DocAccessibleParent* aThisAsDoc)
      : RemoteAccessibleBase(aThisAsDoc) {
    MOZ_COUNT_CTOR(RemoteAccessible);
  }
};

////////////////////////////////////////////////////////////////////////////////
// RemoteAccessible downcasting method

inline RemoteAccessible* Accessible::AsRemote() {
  return IsRemote() ? static_cast<RemoteAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif
