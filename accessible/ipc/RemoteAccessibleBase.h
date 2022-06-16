/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessibleBase_h
#define mozilla_a11y_RemoteAccessibleBase_h

#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/CacheConstants.h"
#include "mozilla/a11y/HyperTextAccessibleBase.h"
#include "mozilla/a11y/Role.h"
#include "AccAttributes.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "nsTArray.h"
#include "nsRect.h"
#include "LocalAccessible.h"

namespace mozilla {
namespace a11y {

class Attribute;
class DocAccessibleParent;
class RemoteAccessible;
enum class RelationType;

template <class Derived>
class RemoteAccessibleBase : public Accessible, public HyperTextAccessibleBase {
 public:
  virtual ~RemoteAccessibleBase() { MOZ_ASSERT(!mWrapper); }

  virtual bool IsRemote() const override { return true; }

  void AddChildAt(uint32_t aIdx, Derived* aChild) {
    mChildren.InsertElementAt(aIdx, aChild);
    if (IsHyperText()) {
      InvalidateCachedHyperTextOffsets();
    }
  }

  virtual uint32_t ChildCount() const override { return mChildren.Length(); }
  Derived* RemoteChildAt(uint32_t aIdx) const {
    return mChildren.SafeElementAt(aIdx);
  }
  Derived* RemoteFirstChild() const {
    return mChildren.Length() ? mChildren[0] : nullptr;
  }
  Derived* RemoteLastChild() const {
    return mChildren.Length() ? mChildren[mChildren.Length() - 1] : nullptr;
  }
  Derived* RemotePrevSibling() const {
    if (IsDoc()) {
      // The normal code path doesn't work for documents because the parent
      // might be a local OuterDoc, but IndexInParent() will return 1.
      // A document is always a single child of an OuterDoc anyway.
      return nullptr;
    }
    int32_t idx = IndexInParent();
    if (idx == -1) {
      return nullptr;  // No parent.
    }
    return idx > 0 ? RemoteParent()->mChildren[idx - 1] : nullptr;
  }
  Derived* RemoteNextSibling() const {
    if (IsDoc()) {
      // The normal code path doesn't work for documents because the parent
      // might be a local OuterDoc, but IndexInParent() will return 1.
      // A document is always a single child of an OuterDoc anyway.
      return nullptr;
    }
    int32_t idx = IndexInParent();
    if (idx == -1) {
      return nullptr;  // No parent.
    }
    MOZ_ASSERT(idx >= 0);
    size_t newIdx = idx + 1;
    return newIdx < RemoteParent()->mChildren.Length()
               ? RemoteParent()->mChildren[newIdx]
               : nullptr;
  }

  // Accessible hierarchy method overrides

  virtual Accessible* Parent() const override { return RemoteParent(); }

  virtual Accessible* ChildAt(uint32_t aIndex) const override {
    return RemoteChildAt(aIndex);
  }

  virtual Accessible* NextSibling() const override {
    return RemoteNextSibling();
  }

  virtual Accessible* PrevSibling() const override {
    return RemotePrevSibling();
  }

  // XXX evaluate if this is fast enough.
  virtual int32_t IndexInParent() const override {
    Derived* parent = RemoteParent();
    if (!parent) {
      return -1;
    }
    return parent->mChildren.IndexOf(static_cast<const Derived*>(this));
  }
  virtual uint32_t EmbeddedChildCount() override;
  virtual int32_t IndexOfEmbeddedChild(Accessible* aChild) override;
  virtual Accessible* EmbeddedChildAt(uint32_t aChildIdx) override;

  void Shutdown();

  void SetChildDoc(DocAccessibleParent* aChildDoc);
  void ClearChildDoc(DocAccessibleParent* aChildDoc);

  /**
   * Remove The given child.
   */
  void RemoveChild(Derived* aChild) {
    mChildren.RemoveElement(aChild);
    if (IsHyperText()) {
      InvalidateCachedHyperTextOffsets();
    }
  }

  /**
   * Return the proxy for the parent of the wrapped accessible.
   */
  Derived* RemoteParent() const;

  LocalAccessible* OuterDocOfRemoteBrowser() const;

  /**
   * Get the role of the accessible we're proxying.
   */
  virtual role Role() const override { return mRole; }

  /**
   * Return true if this is an embedded object.
   */
  bool IsEmbeddedObject() const { return !IsText(); }

  virtual bool IsLink() const override {
    if (IsHTMLLink()) {
      // XXX: HTML links always return true for IsLink.
      return true;
    }

    if (IsText()) {
      return false;
    }

    if (Accessible* parent = Parent()) {
      return parent->IsHyperText();
    }

    return false;
  }

  virtual bool HasNumericValue() const override {
    // XXX: We combine the aria and native "has numeric value" field
    // when we serialize the local accessible into eNumericValue.
    return HasGenericType(eNumericValue);
  }

  // Methods that potentially access a cache.

  virtual ENameValueFlag Name(nsString& aName) const override;
  virtual void Description(nsString& aDescription) const override;
  virtual void Value(nsString& aValue) const override;

  virtual double CurValue() const override;
  virtual double MinValue() const override;
  virtual double MaxValue() const override;
  virtual double Step() const override;

  virtual Accessible* ChildAtPoint(
      int32_t aX, int32_t aY,
      LocalAccessible::EWhichChildAtPoint aWhichChild) override;

  virtual LayoutDeviceIntRect Bounds() const override;

  virtual nsRect BoundsInAppUnits() const override;

  virtual uint64_t State() override;

  virtual already_AddRefed<AccAttributes> Attributes() override;

  virtual nsAtom* TagName() const override;

  virtual already_AddRefed<nsAtom> DisplayStyle() const override;

  virtual Maybe<float> Opacity() const override;

  virtual uint8_t ActionCount() const override;

  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;

  virtual bool DoAction(uint8_t aIndex) const override;

  virtual void SelectionRanges(nsTArray<TextRange>* aRanges) const override;

  //////////////////////////////////////////////////////////////////////////////
  // SelectAccessible

  virtual void SelectedItems(nsTArray<Accessible*>* aItems) override;

  virtual uint32_t SelectedItemCount() override;

  virtual Accessible* GetSelectedItem(uint32_t aIndex) override;

  virtual bool IsItemSelected(uint32_t aIndex) override;

  virtual bool AddItemToSelection(uint32_t aIndex) override;

  virtual bool RemoveItemFromSelection(uint32_t aIndex) override;

  virtual bool SelectAll() override;

  virtual bool UnselectAll() override;

  virtual void TakeSelection() override;

  virtual void SetSelected(bool aSelect) override;

  // Methods that interact with content.

  virtual void TakeFocus() const override;
  virtual void ScrollTo(uint32_t aHow) const override;
  virtual void SetCaretOffset(int32_t aOffset) override;

  /**
   * Allow the platform to store a pointers worth of data on us.
   */
  uintptr_t GetWrapper() const { return mWrapper; }
  void SetWrapper(uintptr_t aWrapper) { mWrapper = aWrapper; }

  virtual uint64_t ID() const override { return mID; }

  /**
   * Return the document containing this proxy, or the proxy itself if it is a
   * document.
   */
  DocAccessibleParent* Document() const { return mDoc; }

  DocAccessibleParent* AsDoc() const { return IsDoc() ? mDoc : nullptr; }

  void ApplyCache(CacheUpdateType aUpdateType, AccAttributes* aFields) {
    if (aUpdateType == CacheUpdateType::Initial) {
      mCachedFields = aFields;
    } else {
      if (!mCachedFields) {
        // The fields cache can be uninitialized if there were no cache-worthy
        // fields in the initial cache push.
        // We don't do a simple assign because we don't want to store the
        // DeleteEntry entries.
        mCachedFields = new AccAttributes();
      }
      mCachedFields->Update(aFields);
      if (IsTextLeaf()) {
        Derived* parent = RemoteParent();
        if (parent && parent->IsHyperText()) {
          parent->InvalidateCachedHyperTextOffsets();
        }
      }
    }
  }

  void UpdateStateCache(uint64_t aState, bool aEnabled) {
    if (aState & kRemoteCalculatedStates) {
      return;
    }
    uint64_t state = 0;
    if (mCachedFields) {
      if (auto oldState =
              mCachedFields->GetAttribute<uint64_t>(nsGkAtoms::state)) {
        state = *oldState;
      }
    } else {
      mCachedFields = new AccAttributes();
    }
    if (aEnabled) {
      state |= aState;
    } else {
      state &= ~aState;
    }
    mCachedFields->SetAttribute(nsGkAtoms::state, state);
  }

  void InvalidateGroupInfo();

  virtual void AppendTextTo(nsAString& aText, uint32_t aStartOffset = 0,
                            uint32_t aLength = UINT32_MAX) override;

  virtual bool TableIsProbablyForLayout();

  uint32_t GetCachedTextLength();
  Maybe<const nsTArray<int32_t>&> GetCachedTextLines();
  Maybe<nsTArray<nsRect>> GetCachedCharData();
  RefPtr<const AccAttributes> GetCachedTextAttributes();

  virtual HyperTextAccessibleBase* AsHyperTextBase() override {
    return IsHyperText() ? static_cast<HyperTextAccessibleBase*>(this)
                         : nullptr;
  }

  virtual TableAccessibleBase* AsTableBase() override;
  virtual TableCellAccessibleBase* AsTableCellBase() override;

  virtual void DOMNodeID(nsString& aID) const override;

  // HyperTextAccessibleBase
  virtual already_AddRefed<AccAttributes> DefaultTextAttributes() override;

  virtual void InvalidateCachedHyperTextOffsets() override {
    if (mCachedFields) {
      mCachedFields->Remove(nsGkAtoms::offset);
    }
  }

 protected:
  RemoteAccessibleBase(uint64_t aID, Derived* aParent,
                       DocAccessibleParent* aDoc, role aRole, AccType aType,
                       AccGenericType aGenericTypes, uint8_t aRoleMapEntryIndex)
      : Accessible(aType, aGenericTypes, aRoleMapEntryIndex),
        mParent(aParent->ID()),
        mDoc(aDoc),
        mWrapper(0),
        mID(aID),
        mCachedFields(nullptr),
        mRole(aRole) {}

  explicit RemoteAccessibleBase(DocAccessibleParent* aThisAsDoc)
      : Accessible(),
        mParent(kNoParent),
        mDoc(aThisAsDoc),
        mWrapper(0),
        mID(0),
        mCachedFields(nullptr),
        mRole(roles::DOCUMENT) {
    mGenericTypes = eDocument | eHyperText;
  }

 protected:
  void SetParent(Derived* aParent);
  Maybe<nsRect> RetrieveCachedBounds() const;
  bool ApplyTransform(nsRect& aBounds) const;
  void ApplyScrollOffset(nsRect& aBounds) const;
  LayoutDeviceIntRect BoundsWithOffset(Maybe<nsRect> aOffset) const;

  virtual void ARIAGroupPosition(int32_t* aLevel, int32_t* aSetSize,
                                 int32_t* aPosInSet) const override;

  virtual AccGroupInfo* GetGroupInfo() const override;

  virtual AccGroupInfo* GetOrCreateGroupInfo() override;

  virtual bool HasPrimaryAction() const override;

  nsAtom* GetPrimaryAction() const;

  virtual const nsTArray<int32_t>& GetCachedHyperTextOffsets() const override;

 private:
  uintptr_t mParent;
  static const uintptr_t kNoParent = UINTPTR_MAX;

  friend Derived;
  friend DocAccessibleParent;
  friend TextLeafPoint;
  friend HyperTextAccessibleBase;
  friend class xpcAccessible;
  friend class CachedTableCellAccessible;

  nsTArray<Derived*> mChildren;
  DocAccessibleParent* mDoc;
  uintptr_t mWrapper;
  uint64_t mID;

 protected:
  virtual const Accessible* Acc() const override { return this; }

  RefPtr<AccAttributes> mCachedFields;

  // XXX DocAccessibleParent gets to change this to change the role of
  // documents.
  role mRole : 27;
};

extern template class RemoteAccessibleBase<RemoteAccessible>;

}  // namespace a11y
}  // namespace mozilla

#endif
