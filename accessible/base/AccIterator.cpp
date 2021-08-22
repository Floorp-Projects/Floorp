/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccIterator.h"

#include "AccGroupInfo.h"
#include "DocAccessible-inl.h"
#ifdef MOZ_XUL
#  include "XULTreeAccessible.h"
#endif

#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/dom/HTMLLabelElement.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// AccIterator
////////////////////////////////////////////////////////////////////////////////

AccIterator::AccIterator(const LocalAccessible* aAccessible,
                         filters::FilterFuncPtr aFilterFunc)
    : mFilterFunc(aFilterFunc) {
  mState = new IteratorState(aAccessible);
}

AccIterator::~AccIterator() {
  while (mState) {
    IteratorState* tmp = mState;
    mState = tmp->mParentState;
    delete tmp;
  }
}

LocalAccessible* AccIterator::Next() {
  while (mState) {
    LocalAccessible* child = mState->mParent->LocalChildAt(mState->mIndex++);
    if (!child) {
      IteratorState* tmp = mState;
      mState = mState->mParentState;
      delete tmp;

      continue;
    }

    uint32_t result = mFilterFunc(child);
    if (result & filters::eMatch) return child;

    if (!(result & filters::eSkipSubtree)) {
      IteratorState* childState = new IteratorState(child, mState);
      mState = childState;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccIterator::IteratorState

AccIterator::IteratorState::IteratorState(const LocalAccessible* aParent,
                                          IteratorState* mParentState)
    : mParent(aParent), mIndex(0), mParentState(mParentState) {}

////////////////////////////////////////////////////////////////////////////////
// RelatedAccIterator
////////////////////////////////////////////////////////////////////////////////

RelatedAccIterator::RelatedAccIterator(DocAccessible* aDocument,
                                       nsIContent* aDependentContent,
                                       nsAtom* aRelAttr)
    : mDocument(aDocument), mRelAttr(aRelAttr), mProviders(nullptr), mIndex(0) {
  nsAutoString id;
  if (aDependentContent->IsElement() &&
      aDependentContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::id,
                                              id)) {
    mProviders = mDocument->GetRelProviders(aDependentContent->AsElement(), id);
  }
}

LocalAccessible* RelatedAccIterator::Next() {
  if (!mProviders) return nullptr;

  while (mIndex < mProviders->Length()) {
    const auto& provider = (*mProviders)[mIndex++];

    // Return related accessible for the given attribute.
    if (provider->mRelAttr == mRelAttr) {
      LocalAccessible* related = mDocument->GetAccessible(provider->mContent);
      if (related) {
        return related;
      }

      // If the document content is pointed by relation then return the
      // document itself.
      if (provider->mContent == mDocument->GetContent()) {
        return mDocument;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLabelIterator
////////////////////////////////////////////////////////////////////////////////

HTMLLabelIterator::HTMLLabelIterator(DocAccessible* aDocument,
                                     const LocalAccessible* aAccessible,
                                     LabelFilter aFilter)
    : mRelIter(aDocument, aAccessible->GetContent(), nsGkAtoms::_for),
      mAcc(aAccessible),
      mLabelFilter(aFilter) {}

bool HTMLLabelIterator::IsLabel(LocalAccessible* aLabel) {
  dom::HTMLLabelElement* labelEl =
      dom::HTMLLabelElement::FromNode(aLabel->GetContent());
  return labelEl && labelEl->GetControl() == mAcc->GetContent();
}

LocalAccessible* HTMLLabelIterator::Next() {
  // Get either <label for="[id]"> element which explicitly points to given
  // element, or <label> ancestor which implicitly point to it.
  LocalAccessible* label = nullptr;
  while ((label = mRelIter.Next())) {
    if (IsLabel(label)) {
      return label;
    }
  }

  // Ignore ancestor label on not widget accessible.
  if (mLabelFilter == eSkipAncestorLabel || !mAcc->IsWidget()) return nullptr;

  // Go up tree to get a name of ancestor label if there is one (an ancestor
  // <label> implicitly points to us). Don't go up farther than form or
  // document.
  LocalAccessible* walkUp = mAcc->LocalParent();
  while (walkUp && !walkUp->IsDoc()) {
    nsIContent* walkUpEl = walkUp->GetContent();
    if (IsLabel(walkUp) &&
        !walkUpEl->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::_for)) {
      mLabelFilter = eSkipAncestorLabel;  // prevent infinite loop
      return walkUp;
    }

    if (walkUpEl->IsHTMLElement(nsGkAtoms::form)) break;

    walkUp = walkUp->LocalParent();
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLOutputIterator
////////////////////////////////////////////////////////////////////////////////

HTMLOutputIterator::HTMLOutputIterator(DocAccessible* aDocument,
                                       nsIContent* aElement)
    : mRelIter(aDocument, aElement, nsGkAtoms::_for) {}

LocalAccessible* HTMLOutputIterator::Next() {
  LocalAccessible* output = nullptr;
  while ((output = mRelIter.Next())) {
    if (output->GetContent()->IsHTMLElement(nsGkAtoms::output)) return output;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULLabelIterator
////////////////////////////////////////////////////////////////////////////////

XULLabelIterator::XULLabelIterator(DocAccessible* aDocument,
                                   nsIContent* aElement)
    : mRelIter(aDocument, aElement, nsGkAtoms::control) {}

LocalAccessible* XULLabelIterator::Next() {
  LocalAccessible* label = nullptr;
  while ((label = mRelIter.Next())) {
    if (label->GetContent()->IsXULElement(nsGkAtoms::label)) return label;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULDescriptionIterator
////////////////////////////////////////////////////////////////////////////////

XULDescriptionIterator::XULDescriptionIterator(DocAccessible* aDocument,
                                               nsIContent* aElement)
    : mRelIter(aDocument, aElement, nsGkAtoms::control) {}

LocalAccessible* XULDescriptionIterator::Next() {
  LocalAccessible* descr = nullptr;
  while ((descr = mRelIter.Next())) {
    if (descr->GetContent()->IsXULElement(nsGkAtoms::description)) return descr;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// IDRefsIterator
////////////////////////////////////////////////////////////////////////////////

IDRefsIterator::IDRefsIterator(DocAccessible* aDoc, nsIContent* aContent,
                               nsAtom* aIDRefsAttr)
    : mContent(aContent), mDoc(aDoc), mCurrIdx(0) {
  if (mContent->IsElement()) {
    mContent->AsElement()->GetAttr(kNameSpaceID_None, aIDRefsAttr, mIDs);
  }
}

const nsDependentSubstring IDRefsIterator::NextID() {
  for (; mCurrIdx < mIDs.Length(); mCurrIdx++) {
    if (!NS_IsAsciiWhitespace(mIDs[mCurrIdx])) break;
  }

  if (mCurrIdx >= mIDs.Length()) return nsDependentSubstring();

  nsAString::index_type idStartIdx = mCurrIdx;
  while (++mCurrIdx < mIDs.Length()) {
    if (NS_IsAsciiWhitespace(mIDs[mCurrIdx])) break;
  }

  return Substring(mIDs, idStartIdx, mCurrIdx++ - idStartIdx);
}

nsIContent* IDRefsIterator::NextElem() {
  while (true) {
    const nsDependentSubstring id = NextID();
    if (id.IsEmpty()) break;

    nsIContent* refContent = GetElem(id);
    if (refContent) return refContent;
  }

  return nullptr;
}

dom::Element* IDRefsIterator::GetElem(nsIContent* aContent,
                                      const nsAString& aID) {
  // Get elements in DOM tree by ID attribute if this is an explicit content.
  // In case of bound element check its anonymous subtree.
  if (!aContent->IsInNativeAnonymousSubtree()) {
    dom::DocumentOrShadowRoot* docOrShadowRoot =
        aContent->GetUncomposedDocOrConnectedShadowRoot();
    if (docOrShadowRoot) {
      dom::Element* refElm = docOrShadowRoot->GetElementById(aID);
      if (refElm) {
        return refElm;
      }
    }
  }
  return nullptr;
}

dom::Element* IDRefsIterator::GetElem(const nsDependentSubstring& aID) {
  return GetElem(mContent, aID);
}

LocalAccessible* IDRefsIterator::Next() {
  nsIContent* nextEl = nullptr;
  while ((nextEl = NextElem())) {
    LocalAccessible* acc = mDoc->GetAccessible(nextEl);
    if (acc) {
      return acc;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// SingleAccIterator
////////////////////////////////////////////////////////////////////////////////

LocalAccessible* SingleAccIterator::Next() {
  RefPtr<LocalAccessible> nextAcc;
  mAcc.swap(nextAcc);
  if (!nextAcc || nextAcc->IsDefunct()) {
    return nullptr;
  }
  return nextAcc;
}

////////////////////////////////////////////////////////////////////////////////
// ItemIterator
////////////////////////////////////////////////////////////////////////////////

LocalAccessible* ItemIterator::Next() {
  if (mContainer) {
    mAnchor = AccGroupInfo::FirstItemOf(mContainer);
    mContainer = nullptr;
    return mAnchor;
  }

  return mAnchor ? (mAnchor = AccGroupInfo::NextItemTo(mAnchor)) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeItemIterator
////////////////////////////////////////////////////////////////////////////////

XULTreeItemIterator::XULTreeItemIterator(const XULTreeAccessible* aXULTree,
                                         nsITreeView* aTreeView,
                                         int32_t aRowIdx)
    : mXULTree(aXULTree),
      mTreeView(aTreeView),
      mRowCount(-1),
      mContainerLevel(-1),
      mCurrRowIdx(aRowIdx + 1) {
  mTreeView->GetRowCount(&mRowCount);
  if (aRowIdx != -1) mTreeView->GetLevel(aRowIdx, &mContainerLevel);
}

LocalAccessible* XULTreeItemIterator::Next() {
  while (mCurrRowIdx < mRowCount) {
    int32_t level = 0;
    mTreeView->GetLevel(mCurrRowIdx, &level);

    if (level == mContainerLevel + 1) {
      return mXULTree->GetTreeItemAccessible(mCurrRowIdx++);
    }

    if (level <= mContainerLevel) {  // got level up
      mCurrRowIdx = mRowCount;
      break;
    }

    mCurrRowIdx++;
  }

  return nullptr;
}
