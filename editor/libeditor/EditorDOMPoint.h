/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorDOMPoint_h
#define mozilla_EditorDOMPoint_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Text.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsINode.h"

namespace mozilla {

template <typename ParentType, typename ChildType>
class EditorDOMPointBase;

/**
 * EditorDOMPoint and EditorRawDOMPoint are simple classes which refers
 * a point in the DOM tree at creating the instance or initializing the
 * instance with calling Set().
 *
 * EditorDOMPoint refers container node (and child node if it's already set)
 * with nsCOMPtr.  EditorRawDOMPoint refers them with raw pointer.
 * So, EditorRawDOMPoint is useful when you access the nodes only before
 * changing DOM tree since increasing refcount may appear in micro benchmark
 * if it's in a hot path.  On the other hand, if you need to refer them even
 * after changing DOM tree, you must use EditorDOMPoint.
 *
 * When initializing an instance only with child node or offset,  the instance
 * starts to refer the child node or offset in the container.  In this case,
 * the other information hasn't been initialized due to performance reason.
 * When you retrieve the other information with calling Offset() or
 * GetChild(), the other information is computed with the current DOM tree.
 * Therefore, e.g., in the following case, the other information may be
 * different:
 *
 * EditorDOMPoint pointA(container1, childNode1);
 * EditorDOMPoint pointB(container1, childNode1);
 * Unused << pointA.Offset(); // The offset is computed now.
 * container1->RemoveChild(childNode1->GetPreviousSibling());
 * Unused << pointB.Offset(); // Now, pointB.Offset() equals pointA.Offset() - 1
 *
 * similarly:
 *
 * EditorDOMPoint pointA(container1, 5);
 * EditorDOMPoint pointB(container1, 5);
 * Unused << pointA.GetChild(); // The child is computed now.
 * container1->RemoveChild(childNode1->GetFirstChild());
 * Unused << pointB.GetChild(); // Now, pointB.GetChild() equals
 *                              // pointA.GetChild()->GetPreviousSibling().
 *
 * So, when you initialize an instance only with one information, you need to
 * be careful when you access the other information after changing the DOM tree.
 * When you need to lock the child node or offset and recompute the other
 * information with new DOM tree, you can use
 * AutoEditorDOMPointOffsetInvalidator and AutoEditorDOMPointChildInvalidator.
 */

typedef EditorDOMPointBase<nsCOMPtr<nsINode>, nsCOMPtr<nsIContent>>
    EditorDOMPoint;
typedef EditorDOMPointBase<nsINode*, nsIContent*> EditorRawDOMPoint;
typedef EditorDOMPointBase<RefPtr<dom::Text>, nsIContent*> EditorDOMPointInText;

template <typename ParentType, typename ChildType>
class EditorDOMPointBase final {
  typedef EditorDOMPointBase<ParentType, ChildType> SelfType;

 public:
  EditorDOMPointBase()
      : mParent(nullptr), mChild(nullptr), mIsChildInitialized(false) {}

  template <typename ContainerType>
  EditorDOMPointBase(ContainerType* aContainer, int32_t aOffset)
      : mParent(aContainer),
        mChild(nullptr),
        mOffset(mozilla::Some(aOffset)),
        mIsChildInitialized(false) {
    NS_WARNING_ASSERTION(
        !mParent || mOffset.value() <= mParent->Length(),
        "The offset is larger than the length of aContainer or negative");
    if (!mParent) {
      mOffset.reset();
    }
  }

  template <typename ContainerType, template <typename> typename StrongPtr>
  EditorDOMPointBase(const StrongPtr<ContainerType>& aContainer,
                     int32_t aOffset)
      : EditorDOMPointBase(aContainer.get(), aOffset) {}

  /**
   * Different from RangeBoundary, aPointedNode should be a child node
   * which you want to refer.
   */
  explicit EditorDOMPointBase(nsINode* aPointedNode)
      : mParent(aPointedNode && aPointedNode->IsContent()
                    ? aPointedNode->GetParentNode()
                    : nullptr),
        mChild(aPointedNode && aPointedNode->IsContent()
                   ? aPointedNode->AsContent()
                   : nullptr),
        mIsChildInitialized(false) {
    mIsChildInitialized = aPointedNode && mChild;
    NS_WARNING_ASSERTION(IsSet(),
                         "The child is nullptr or doesn't have its parent");
    NS_WARNING_ASSERTION(mChild && mChild->GetParentNode() == mParent,
                         "Initializing RangeBoundary with invalid value");
  }

  EditorDOMPointBase(nsINode* aContainer, nsIContent* aPointedNode,
                     int32_t aOffset)
      : mParent(aContainer),
        mChild(aPointedNode),
        mOffset(mozilla::Some(aOffset)),
        mIsChildInitialized(true) {
    MOZ_DIAGNOSTIC_ASSERT(
        aContainer, "This constructor shouldn't be used when pointing nowhere");
    MOZ_ASSERT(mOffset.value() <= mParent->Length());
    MOZ_ASSERT(mChild || mParent->Length() == mOffset.value() ||
               !mParent->IsContainerNode());
    MOZ_ASSERT(!mChild || mParent == mChild->GetParentNode());
    MOZ_ASSERT(mParent->GetChildAt_Deprecated(mOffset.value()) == mChild);
  }

  template <typename PT, typename CT>
  explicit EditorDOMPointBase(const RangeBoundaryBase<PT, CT>& aOther)
      : mParent(aOther.mParent),
        mChild(aOther.mRef ? aOther.mRef->GetNextSibling()
                           : (aOther.mParent ? aOther.mParent->GetFirstChild()
                                             : nullptr)),
        mOffset(aOther.mOffset),
        mIsChildInitialized(aOther.mRef || (aOther.mOffset.isSome() &&
                                            !aOther.mOffset.value())) {}

  template <typename PT, typename CT>
  MOZ_IMPLICIT EditorDOMPointBase(const EditorDOMPointBase<PT, CT>& aOther)
      : mParent(aOther.mParent),
        mChild(aOther.mChild),
        mOffset(aOther.mOffset),
        mIsChildInitialized(aOther.mIsChildInitialized) {}

  /**
   * GetContainer() returns the container node at the point.
   * GetContainerAs*() returns the container node as specific type.
   */
  nsINode* GetContainer() const { return mParent; }

  nsIContent* GetContainerAsContent() const {
    return nsIContent::FromNodeOrNull(mParent);
  }

  MOZ_NEVER_INLINE_DEBUG nsIContent* ContainerAsContent() const {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mParent->IsContent());
    return mParent->AsContent();
  }

  dom::Element* GetContainerAsElement() const {
    return dom::Element::FromNodeOrNull(mParent);
  }

  MOZ_NEVER_INLINE_DEBUG dom::Element* ContainerAsElement() const {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mParent->IsElement());
    return mParent->AsElement();
  }

  dom::Text* GetContainerAsText() const {
    return dom::Text::FromNodeOrNull(mParent);
  }

  MOZ_NEVER_INLINE_DEBUG dom::Text* ContainerAsText() const {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(IsInTextNode());
    return mParent->AsText();
  }

  /**
   * GetContainerParent() returns parent of the container node at the point.
   */
  nsINode* GetContainerParent() const {
    return mParent ? mParent->GetParent() : nullptr;
  }

  nsIContent* GetContainerParentAsContent() const {
    return nsIContent::FromNodeOrNull(GetContainerParent());
  }

  dom::Element* GetContainerParentAsElement() const {
    return dom::Element::FromNodeOrNull(GetContainerParent());
  }

  /**
   * CanContainerHaveChildren() returns true if the container node can have
   * child nodes.  Otherwise, e.g., when the container is a text node, returns
   * false.
   */
  bool CanContainerHaveChildren() const {
    return mParent && mParent->IsContainerNode();
  }

  /**
   * IsContainerEmpty() returns true if it has no children or its text is empty.
   */
  bool IsContainerEmpty() const { return mParent && !mParent->Length(); }

  /**
   * IsInDataNode() returns true if the container node is a data node including
   * text node.
   */
  bool IsInDataNode() const { return mParent && mParent->IsCharacterData(); }

  /**
   * IsInTextNode() returns true if the container node is a text node.
   */
  bool IsInTextNode() const { return mParent && mParent->IsText(); }

  /**
   * IsInNativeAnonymousSubtree() returns true if the container is in
   * native anonymous subtree.
   */
  bool IsInNativeAnonymousSubtree() const {
    return mParent && mParent->IsInNativeAnonymousSubtree();
  }

  /**
   * IsContainerHTMLElement() returns true if the container node is an HTML
   * element node and its node name is aTag.
   */
  bool IsContainerHTMLElement(nsAtom* aTag) const {
    return mParent && mParent->IsHTMLElement(aTag);
  }

  /**
   * IsContainerAnyOfHTMLElements() returns true if the container node is an
   * HTML element node and its node name is one of the arguments.
   */
  template <typename First, typename... Args>
  bool IsContainerAnyOfHTMLElements(First aFirst, Args... aArgs) const {
    return mParent && mParent->IsAnyOfHTMLElements(aFirst, aArgs...);
  }

  /**
   * GetChild() returns a child node which is pointed by the instance.
   * If mChild hasn't been initialized yet, this computes the child node
   * from mParent and mOffset with *current* DOM tree.
   */
  nsIContent* GetChild() const {
    if (!mParent || !mParent->IsContainerNode()) {
      return nullptr;
    }
    if (mIsChildInitialized) {
      return mChild;
    }
    // Fix child node now.
    const_cast<SelfType*>(this)->EnsureChild();
    return mChild;
  }

  /**
   * GetNextSiblingOfChild() returns next sibling of the child node.
   * If this refers after the last child or the container cannot have children,
   * this returns nullptr with warning.
   * If mChild hasn't been initialized yet, this computes the child node
   * from mParent and mOffset with *current* DOM tree.
   */
  nsIContent* GetNextSiblingOfChild() const {
    if (NS_WARN_IF(!mParent) || NS_WARN_IF(!mParent->IsContainerNode())) {
      return nullptr;
    }
    if (mIsChildInitialized) {
      return mChild ? mChild->GetNextSibling() : nullptr;
    }
    MOZ_ASSERT(mOffset.isSome());
    if (NS_WARN_IF(mOffset.value() > mParent->Length())) {
      // If this has been set only offset and now the offset is invalid,
      // let's just return nullptr.
      return nullptr;
    }
    // Fix child node now.
    const_cast<SelfType*>(this)->EnsureChild();
    return mChild ? mChild->GetNextSibling() : nullptr;
  }

  /**
   * GetPreviousSiblingOfChild() returns previous sibling of a child
   * at offset.  If this refers the first child or the container cannot have
   * children, this returns nullptr with warning.
   * If mChild hasn't been initialized yet, this computes the child node
   * from mParent and mOffset with *current* DOM tree.
   */
  nsIContent* GetPreviousSiblingOfChild() const {
    if (NS_WARN_IF(!mParent) || NS_WARN_IF(!mParent->IsContainerNode())) {
      return nullptr;
    }
    if (mIsChildInitialized) {
      return mChild ? mChild->GetPreviousSibling() : mParent->GetLastChild();
    }
    MOZ_ASSERT(mOffset.isSome());
    if (NS_WARN_IF(mOffset.value() > mParent->Length())) {
      // If this has been set only offset and now the offset is invalid,
      // let's just return nullptr.
      return nullptr;
    }
    // Fix child node now.
    const_cast<SelfType*>(this)->EnsureChild();
    return mChild ? mChild->GetPreviousSibling() : mParent->GetLastChild();
  }

  /**
   * Simple accessors of the character in dom::Text so that when you call
   * these methods, you need to guarantee that the container is a dom::Text.
   */
  MOZ_NEVER_INLINE_DEBUG char16_t Char() const {
    MOZ_ASSERT(IsSetAndValid());
    MOZ_ASSERT(!IsEndOfContainer());
    return ContainerAsText()->TextFragment().CharAt(mOffset.value());
  }
  MOZ_NEVER_INLINE_DEBUG bool IsCharASCIISpace() const {
    return nsCRT::IsAsciiSpace(Char());
  }
  MOZ_NEVER_INLINE_DEBUG bool IsCharNBSP() const { return Char() == 0x00A0; }

  MOZ_NEVER_INLINE_DEBUG char16_t PreviousChar() const {
    MOZ_ASSERT(IsSetAndValid());
    MOZ_ASSERT(!IsStartOfContainer());
    return ContainerAsText()->TextFragment().CharAt(mOffset.value() - 1);
  }
  MOZ_NEVER_INLINE_DEBUG bool IsPreviousCharASCIISpace() const {
    return nsCRT::IsAsciiSpace(PreviousChar());
  }
  MOZ_NEVER_INLINE_DEBUG bool IsPreviousCharNBSP() const {
    return PreviousChar() == 0x00A0;
  }

  MOZ_NEVER_INLINE_DEBUG char16_t NextChar() const {
    MOZ_ASSERT(IsSetAndValid());
    MOZ_ASSERT(!IsAtLastContent() && !IsEndOfContainer());
    return ContainerAsText()->TextFragment().CharAt(mOffset.value() + 1);
  }
  MOZ_NEVER_INLINE_DEBUG bool IsNextCharASCIISpace() const {
    return nsCRT::IsAsciiSpace(NextChar());
  }
  MOZ_NEVER_INLINE_DEBUG bool IsNextCharNBSP() const {
    return NextChar() == 0x00A0;
  }

  uint32_t Offset() const {
    if (mOffset.isSome()) {
      MOZ_ASSERT(mOffset.isSome());
      return mOffset.value();
    }
    if (!mParent) {
      MOZ_ASSERT(!mChild);
      return 0;
    }
    MOZ_ASSERT(mParent->IsContainerNode(),
               "If the container cannot have children, mOffset.isSome() should "
               "be true");
    if (!mChild) {
      // We're referring after the last child.  Fix offset now.
      const_cast<SelfType*>(this)->mOffset = mozilla::Some(mParent->Length());
      return mOffset.value();
    }
    MOZ_ASSERT(mChild->GetParentNode() == mParent);
    // Fix offset now.
    if (mChild == mParent->GetFirstChild()) {
      const_cast<SelfType*>(this)->mOffset = mozilla::Some(0);
    } else {
      const_cast<SelfType*>(this)->mOffset =
          mozilla::Some(mParent->ComputeIndexOf(mChild));
    }
    return mOffset.value();
  }

  /**
   * Set() sets a point to aOffset or aChild.
   * If it's set with aOffset, mChild is invalidated.  If it's set with aChild,
   * mOffset may be invalidated.
   */
  template <typename ContainerType>
  void Set(ContainerType* aContainer, int32_t aOffset) {
    mParent = aContainer;
    mChild = nullptr;
    mOffset = mozilla::Some(aOffset);
    mIsChildInitialized = false;
    NS_ASSERTION(!mParent || mOffset.value() <= mParent->Length(),
                 "The offset is out of bounds");
  }
  template <typename ContainerType, template <typename> typename StrongPtr>
  void Set(const StrongPtr<ContainerType>& aContainer, int32_t aOffset) {
    Set(aContainer.get(), aOffset);
  }
  void Set(const nsINode* aChild) {
    MOZ_ASSERT(aChild);
    if (NS_WARN_IF(!aChild->IsContent())) {
      Clear();
      return;
    }
    mParent = aChild->GetParentNode();
    mChild = const_cast<nsIContent*>(aChild->AsContent());
    mOffset.reset();
    mIsChildInitialized = true;
  }

  /**
   * SetToEndOf() sets this to the end of aContainer.  Then, mChild is always
   * nullptr but marked as initialized and mOffset is always set.
   */
  template <typename ContainerType>
  MOZ_NEVER_INLINE_DEBUG void SetToEndOf(const ContainerType* aContainer) {
    MOZ_ASSERT(aContainer);
    mParent = const_cast<ContainerType*>(aContainer);
    mChild = nullptr;
    mOffset = mozilla::Some(mParent->Length());
    mIsChildInitialized = true;
  }
  template <typename ContainerType, template <typename> typename StrongPtr>
  MOZ_NEVER_INLINE_DEBUG void SetToEndOf(
      const StrongPtr<ContainerType>& aContainer) {
    SetToEndOf(aContainer.get());
  }
  template <typename ContainerType>
  MOZ_NEVER_INLINE_DEBUG static SelfType AtEndOf(
      const ContainerType& aContainer) {
    SelfType point;
    point.SetToEndOf(&aContainer);
    return point;
  }
  template <typename ContainerType, template <typename> typename StrongPtr>
  MOZ_NEVER_INLINE_DEBUG static SelfType AtEndOf(
      const StrongPtr<ContainerType>& aContainer) {
    MOZ_ASSERT(aContainer.get());
    return AtEndOf(*aContainer.get());
  }

  /**
   * SetAfter() sets mChild to next sibling of aChild.
   */
  void SetAfter(const nsINode* aChild) {
    MOZ_ASSERT(aChild);
    nsIContent* nextSibling = aChild->GetNextSibling();
    if (nextSibling) {
      Set(nextSibling);
      return;
    }
    nsINode* parentNode = aChild->GetParentNode();
    if (NS_WARN_IF(!parentNode)) {
      Clear();
      return;
    }
    SetToEndOf(parentNode);
  }
  template <typename ContainerType>
  static SelfType After(const ContainerType& aContainer) {
    SelfType point;
    point.SetAfter(&aContainer);
    return point;
  }
  template <typename ContainerType, template <typename> typename StrongPtr>
  MOZ_NEVER_INLINE_DEBUG static SelfType After(
      const StrongPtr<ContainerType>& aContainer) {
    MOZ_ASSERT(aContainer.get());
    return After(*aContainer.get());
  }
  template <typename PT, typename CT>
  MOZ_NEVER_INLINE_DEBUG static SelfType After(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    MOZ_ASSERT(aPoint.IsSet());
    if (aPoint.mChild) {
      return After(*aPoint.mChild);
    }
    if (NS_WARN_IF(aPoint.IsEndOfContainer())) {
      return SelfType();
    }
    SelfType point(aPoint);
    MOZ_ALWAYS_TRUE(point.AdvanceOffset());
    return point;
  }

  /**
   * NextPoint() and PreviousPoint() returns next/previous DOM point in
   * the container.
   */
  MOZ_NEVER_INLINE_DEBUG SelfType NextPoint() const {
    NS_ASSERTION(!IsEndOfContainer(), "Should not be at end of the container");
    SelfType result(*this);
    result.AdvanceOffset();
    return result;
  }
  MOZ_NEVER_INLINE_DEBUG SelfType PreviousPoint() const {
    NS_ASSERTION(!IsStartOfContainer(),
                 "Should not be at start of the container");
    SelfType result(*this);
    result.RewindOffset();
    return result;
  }

  /**
   * Clear() makes the instance not point anywhere.
   */
  void Clear() {
    mParent = nullptr;
    mChild = nullptr;
    mOffset.reset();
    mIsChildInitialized = false;
  }

  /**
   * AdvanceOffset() tries to refer next sibling of mChild and/of next offset.
   * If the container can have children and there is no next sibling or the
   * offset reached the length of the container, this outputs warning and does
   * nothing.  So, callers need to check if there is next sibling which you
   * need to refer.
   *
   * @return            true if there is a next DOM point to refer.
   */
  bool AdvanceOffset() {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    // If only mOffset is available, just compute the offset.
    if ((mOffset.isSome() && !mIsChildInitialized) ||
        !mParent->IsContainerNode()) {
      MOZ_ASSERT(mOffset.isSome());
      MOZ_ASSERT(!mChild);
      if (NS_WARN_IF(mOffset.value() >= mParent->Length())) {
        // We're already referring the start of the container.
        return false;
      }
      mOffset = mozilla::Some(mOffset.value() + 1);
      return true;
    }

    MOZ_ASSERT(mIsChildInitialized);
    MOZ_ASSERT(!mOffset.isSome() || mOffset.isSome());
    if (NS_WARN_IF(!mParent->HasChildren()) || NS_WARN_IF(!mChild) ||
        NS_WARN_IF(mOffset.isSome() && mOffset.value() >= mParent->Length())) {
      // We're already referring the end of the container (or outside).
      return false;
    }

    if (mOffset.isSome()) {
      MOZ_ASSERT(mOffset.isSome());
      mOffset = mozilla::Some(mOffset.value() + 1);
    }
    mChild = mChild->GetNextSibling();
    return true;
  }

  /**
   * RewindOffset() tries to refer previous sibling of mChild and/or previous
   * offset.  If the container can have children and there is no next previous
   * or the offset is 0, this outputs warning and does nothing.  So, callers
   * need to check if there is previous sibling which you need to refer.
   *
   * @return            true if there is a previous DOM point to refer.
   */
  bool RewindOffset() {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    // If only mOffset is available, just compute the offset.
    if ((mOffset.isSome() && !mIsChildInitialized) ||
        !mParent->IsContainerNode()) {
      MOZ_ASSERT(mOffset.isSome());
      MOZ_ASSERT(!mChild);
      if (NS_WARN_IF(!mOffset.value()) ||
          NS_WARN_IF(mOffset.value() > mParent->Length())) {
        // We're already referring the start of the container or
        // the offset is invalid since perhaps, the offset was set before
        // the last DOM tree change.
        return false;
      }
      mOffset = mozilla::Some(mOffset.value() - 1);
      return true;
    }

    MOZ_ASSERT(mIsChildInitialized);
    MOZ_ASSERT(!mOffset.isSome() || mOffset.isSome());
    if (NS_WARN_IF(!mParent->HasChildren()) ||
        NS_WARN_IF(mChild && !mChild->GetPreviousSibling()) ||
        NS_WARN_IF(mOffset.isSome() && !mOffset.value())) {
      // We're already referring the start of the container (or the child has
      // been moved from the container?).
      return false;
    }

    nsIContent* previousSibling =
        mChild ? mChild->GetPreviousSibling() : mParent->GetLastChild();
    if (NS_WARN_IF(!previousSibling)) {
      // We're already referring the first child of the container.
      return false;
    }

    if (mOffset.isSome()) {
      mOffset = mozilla::Some(mOffset.value() - 1);
    }
    mChild = previousSibling;
    return true;
  }

  /**
   * GetNonAnonymousSubtreePoint() returns a DOM point which is NOT in
   * native-anonymous subtree.  If the instance isn't in native-anonymous
   * subtree, this returns same point.  Otherwise, climbs up until finding
   * non-native-anonymous parent and returns the point of it.  I.e.,
   * container is parent of the found non-anonymous-native node.
   */
  EditorRawDOMPoint GetNonAnonymousSubtreePoint() const {
    if (NS_WARN_IF(!IsSet())) {
      return EditorRawDOMPoint();
    }
    if (!IsInNativeAnonymousSubtree()) {
      return EditorRawDOMPoint(*this);
    }
    nsINode* parent;
    for (parent = mParent->GetParentNode();
         parent && parent->IsInNativeAnonymousSubtree();
         parent = parent->GetParentNode()) {
    }
    if (!parent) {
      return EditorRawDOMPoint();
    }
    return EditorRawDOMPoint(parent);
  }

  bool IsSet() const {
    return mParent && (mIsChildInitialized || mOffset.isSome());
  }

  bool IsSetAndValid() const {
    if (!IsSet()) {
      return false;
    }

    if (mChild && mChild->GetParentNode() != mParent) {
      return false;
    }
    if (mOffset.isSome() && mOffset.value() > mParent->Length()) {
      return false;
    }
    return true;
  }

  bool HasChildMovedFromContainer() const {
    return mChild && mChild->GetParentNode() != mParent;
  }

  bool IsStartOfContainer() const {
    // If we're referring the first point in the container:
    //   If mParent is not a container like a text node, mOffset is 0.
    //   If mChild is initialized and it's first child of mParent.
    //   If mChild isn't initialized and the offset is 0.
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    if (!mParent->IsContainerNode()) {
      return !mOffset.value();
    }
    if (mIsChildInitialized) {
      if (mParent->GetFirstChild() == mChild) {
        NS_WARNING_ASSERTION(!mOffset.isSome() || !mOffset.value(),
                             "If mOffset was initialized, it should be 0");
        return true;
      }
      NS_WARNING_ASSERTION(!mOffset.isSome() || mParent->GetChildAt_Deprecated(
                                                    mOffset.value()) == mChild,
                           "mOffset and mChild are mismatched");
      return false;
    }
    MOZ_ASSERT(mOffset.isSome());
    return !mOffset.value();
  }

  bool IsEndOfContainer() const {
    // If we're referring after the last point of the container:
    //   If mParent is not a container like text node, mOffset is same as the
    //   length of the container.
    //   If mChild is initialized and it's nullptr.
    //   If mChild isn't initialized and mOffset is same as the length of the
    //   container.
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    if (!mParent->IsContainerNode()) {
      return mOffset.value() == mParent->Length();
    }
    if (mIsChildInitialized) {
      if (!mChild) {
        NS_WARNING_ASSERTION(
            !mOffset.isSome() || mOffset.value() == mParent->Length(),
            "If mOffset was initialized, it should be length of the container");
        return true;
      }
      NS_WARNING_ASSERTION(!mOffset.isSome() || mParent->GetChildAt_Deprecated(
                                                    mOffset.value()) == mChild,
                           "mOffset and mChild are mismatched");
      return false;
    }
    MOZ_ASSERT(mOffset.isSome());
    return mOffset.value() == mParent->Length();
  }

  /**
   * IsAtLastContent() returns true when it refers last child of the container
   * or last character offset of text node.
   */
  bool IsAtLastContent() const {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    if (mParent->IsContainerNode() && mOffset.isSome()) {
      return mOffset.value() == mParent->Length() - 1;
    }
    if (mIsChildInitialized) {
      if (mChild && mChild == mParent->GetLastChild()) {
        NS_WARNING_ASSERTION(
            !mOffset.isSome() || mOffset.value() == mParent->Length() - 1,
            "If mOffset was initialized, it should be length - 1 of the "
            "container");
        return true;
      }
      NS_WARNING_ASSERTION(!mOffset.isSome() || mParent->GetChildAt_Deprecated(
                                                    mOffset.value()) == mChild,
                           "mOffset and mChild are mismatched");
      return false;
    }
    MOZ_ASSERT(mOffset.isSome());
    return mOffset.value() == mParent->Length() - 1;
  }

  bool IsBRElementAtEndOfContainer() const {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    if (!mParent->IsContainerNode()) {
      return false;
    }
    const_cast<SelfType*>(this)->EnsureChild();
    if (!mChild || mChild->GetNextSibling()) {
      return false;
    }
    return mChild->IsHTMLElement(nsGkAtoms::br);
  }

  template <typename A, typename B>
  EditorDOMPointBase& operator=(const RangeBoundaryBase<A, B>& aOther) {
    mParent = aOther.mParent;
    mChild = aOther.mRef ? aOther.mRef->GetNextSibling()
                         : (aOther.mParent && aOther.mParent->IsContainerNode()
                                ? aOther.mParent->GetFirstChild()
                                : nullptr);
    mOffset = aOther.mOffset;
    mIsChildInitialized =
        aOther.mRef || (aOther.mParent && !aOther.mParent->IsContainerNode()) ||
        (aOther.mOffset.isSome() && !aOther.mOffset.value());
    return *this;
  }

  template <typename A, typename B>
  EditorDOMPointBase& operator=(const EditorDOMPointBase<A, B>& aOther) {
    mParent = aOther.mParent;
    mChild = aOther.mChild;
    mOffset = aOther.mOffset;
    mIsChildInitialized = aOther.mIsChildInitialized;
    return *this;
  }

  template <typename A, typename B>
  bool operator==(const EditorDOMPointBase<A, B>& aOther) const {
    if (mParent != aOther.mParent) {
      return false;
    }

    if (mOffset.isSome() && aOther.mOffset.isSome()) {
      // If both mOffset are set, we need to compare both mRef too because
      // the relation of mRef and mOffset have already broken by DOM tree
      // changes.
      if (mOffset != aOther.mOffset) {
        return false;
      }
      if (mChild == aOther.mChild) {
        return true;
      }
      if (NS_WARN_IF(mIsChildInitialized && aOther.mIsChildInitialized)) {
        // In this case, relation between mChild and mOffset of one of or both
        // of them doesn't match with current DOM tree since the DOM tree might
        // have been changed after computing mChild or mOffset.
        return false;
      }
      // If one of mChild hasn't been computed yet, we should compare them only
      // with mOffset.  Perhaps, we shouldn't copy mChild from non-nullptr one
      // to the other since if we copy it here, it may be unexpected behavior
      // for some callers.
      return true;
    }

    MOZ_ASSERT(mIsChildInitialized || aOther.mIsChildInitialized);

    if (mOffset.isSome() && !mIsChildInitialized && !aOther.mOffset.isSome() &&
        aOther.mIsChildInitialized) {
      // If this has only mOffset and the other has only mChild, this needs to
      // compute mChild now.
      const_cast<SelfType*>(this)->EnsureChild();
      return mChild == aOther.mChild;
    }

    if (!mOffset.isSome() && mIsChildInitialized && aOther.mOffset.isSome() &&
        !aOther.mIsChildInitialized) {
      // If this has only mChild and the other has only mOffset, the other needs
      // to compute mChild now.
      const_cast<EditorDOMPointBase<A, B>&>(aOther).EnsureChild();
      return mChild == aOther.mChild;
    }

    // If mOffset of one of them hasn't been computed from mChild yet, we should
    // compare only with mChild.  Perhaps, we shouldn't copy mOffset from being
    // some one to not being some one since if we copy it here, it may be
    // unexpected behavior for some callers.
    return mChild == aOther.mChild;
  }

  template <typename A, typename B>
  bool operator==(const RangeBoundaryBase<A, B>& aOther) const {
    // TODO: Optimize this with directly comparing with RangeBoundaryBase
    //       members.
    return *this == SelfType(aOther);
  }

  template <typename A, typename B>
  bool operator!=(const EditorDOMPointBase<A, B>& aOther) const {
    return !(*this == aOther);
  }

  template <typename A, typename B>
  bool operator!=(const RangeBoundaryBase<A, B>& aOther) const {
    return !(*this == aOther);
  }

  /**
   * This operator should be used if API of other modules take RawRangeBoundary,
   * e.g., methods of Selection and nsRange.
   */
  operator const RawRangeBoundary() const { return ToRawRangeBoundary(); }
  const RawRangeBoundary ToRawRangeBoundary() const {
    if (!IsSet() || NS_WARN_IF(!mIsChildInitialized && !mOffset.isSome())) {
      return RawRangeBoundary();
    }
    if (!mParent->IsContainerNode()) {
      MOZ_ASSERT(mOffset.value() <= mParent->Length());
      // If the container is a data node like a text node, we need to create
      // RangeBoundaryBase instance only with mOffset because mChild is always
      // nullptr.
      return RawRangeBoundary(mParent, mOffset.value());
    }
    if (mIsChildInitialized && mOffset.isSome()) {
      // If we've already set both child and offset, we should create
      // RangeBoundary with offset after validation.
#ifdef DEBUG
      if (mChild) {
        MOZ_ASSERT(mParent == mChild->GetParentNode());
        MOZ_ASSERT(mParent->GetChildAt_Deprecated(mOffset.value()) == mChild);
      } else {
        MOZ_ASSERT(mParent->Length() == mOffset.value());
      }
#endif  // #ifdef DEBUG
      return RawRangeBoundary(mParent, mOffset.value());
    }
    // Otherwise, we should create RangeBoundaryBase only with available
    // information.
    if (mOffset.isSome()) {
      return RawRangeBoundary(mParent, mOffset.value());
    }
    if (mChild) {
      return RawRangeBoundary(mParent, mChild->GetPreviousSibling());
    }
    return RawRangeBoundary(mParent, mParent->GetLastChild());
  }

 private:
  void EnsureChild() {
    if (mIsChildInitialized) {
      return;
    }
    if (!mParent) {
      MOZ_ASSERT(!mOffset.isSome());
      return;
    }
    MOZ_ASSERT(mOffset.isSome());
    MOZ_ASSERT(mOffset.value() <= mParent->Length());
    mIsChildInitialized = true;
    if (!mParent->IsContainerNode()) {
      return;
    }
    mChild = mParent->GetChildAt_Deprecated(mOffset.value());
    MOZ_ASSERT(mChild || mOffset.value() == mParent->Length());
  }

  ParentType mParent;
  ChildType mChild;

  mozilla::Maybe<uint32_t> mOffset;

  bool mIsChildInitialized;

  template <typename PT, typename CT>
  friend class EditorDOMPointBase;

  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          EditorDOMPoint&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(EditorDOMPoint&);
};

inline void ImplCycleCollectionUnlink(EditorDOMPoint& aField) {
  ImplCycleCollectionUnlink(aField.mParent);
  ImplCycleCollectionUnlink(aField.mChild);
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, EditorDOMPoint& aField,
    const char* aName, uint32_t aFlags) {
  ImplCycleCollectionTraverse(aCallback, aField.mParent, "mParent", 0);
  ImplCycleCollectionTraverse(aCallback, aField.mChild, "mChild", 0);
}

/**
 * AutoEditorDOMPointOffsetInvalidator is useful if DOM tree will be changed
 * when EditorDOMPoint instance is available and keeps referring same child
 * node.
 *
 * This class automatically guarantees that given EditorDOMPoint instance
 * stores the child node and invalidates its offset when the instance is
 * destroyed.  Additionally, users of this class can invalidate the offset
 * manually when they need.
 */
class MOZ_STACK_CLASS AutoEditorDOMPointOffsetInvalidator final {
 public:
  explicit AutoEditorDOMPointOffsetInvalidator(EditorDOMPoint& aPoint)
      : mPoint(aPoint), mCanceled(false) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    MOZ_ASSERT(mPoint.CanContainerHaveChildren());
    mChild = mPoint.GetChild();
  }

  ~AutoEditorDOMPointOffsetInvalidator() {
    if (!mCanceled) {
      InvalidateOffset();
    }
  }

  /**
   * Manually, invalidate offset of the given point.
   */
  void InvalidateOffset() {
    if (mChild) {
      mPoint.Set(mChild);
    } else {
      // If the point referred after the last child, let's keep referring
      // after current last node of the old container.
      mPoint.SetToEndOf(mPoint.GetContainer());
    }
  }

  /**
   * After calling Cancel(), mPoint won't be modified by the destructor.
   */
  void Cancel() { mCanceled = true; }

 private:
  EditorDOMPoint& mPoint;
  // Needs to store child node by ourselves because EditorDOMPoint stores
  // child node with mRef which is previous sibling of current child node.
  // Therefore, we cannot keep referring it if it's first child.
  nsCOMPtr<nsIContent> mChild;

  bool mCanceled;

  AutoEditorDOMPointOffsetInvalidator() = delete;
  AutoEditorDOMPointOffsetInvalidator(
      const AutoEditorDOMPointOffsetInvalidator& aOther) = delete;
  const AutoEditorDOMPointOffsetInvalidator& operator=(
      const AutoEditorDOMPointOffsetInvalidator& aOther) = delete;
};

/**
 * AutoEditorDOMPointChildInvalidator is useful if DOM tree will be changed
 * when EditorDOMPoint instance is available and keeps referring same container
 * and offset in it.
 *
 * This class automatically guarantees that given EditorDOMPoint instance
 * stores offset and invalidates its child node when the instance is destroyed.
 * Additionally, users of this class can invalidate the child manually when
 * they need.
 */
class MOZ_STACK_CLASS AutoEditorDOMPointChildInvalidator final {
 public:
  explicit AutoEditorDOMPointChildInvalidator(EditorDOMPoint& aPoint)
      : mPoint(aPoint), mCanceled(false) {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    Unused << mPoint.Offset();
  }

  ~AutoEditorDOMPointChildInvalidator() {
    if (!mCanceled) {
      InvalidateChild();
    }
  }

  /**
   * Manually, invalidate child of the given point.
   */
  void InvalidateChild() { mPoint.Set(mPoint.GetContainer(), mPoint.Offset()); }

  /**
   * After calling Cancel(), mPoint won't be modified by the destructor.
   */
  void Cancel() { mCanceled = true; }

 private:
  EditorDOMPoint& mPoint;

  bool mCanceled;

  AutoEditorDOMPointChildInvalidator() = delete;
  AutoEditorDOMPointChildInvalidator(
      const AutoEditorDOMPointChildInvalidator& aOther) = delete;
  const AutoEditorDOMPointChildInvalidator& operator=(
      const AutoEditorDOMPointChildInvalidator& aOther) = delete;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_EditorDOMPoint_h
