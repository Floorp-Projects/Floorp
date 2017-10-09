/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccIterator.h"

#include "AccGroupInfo.h"
#ifdef MOZ_XUL
#include "XULTreeAccessible.h"
#endif

#include "mozilla/dom/HTMLLabelElement.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// AccIterator
////////////////////////////////////////////////////////////////////////////////

AccIterator::AccIterator(Accessible* aAccessible,
                         filters::FilterFuncPtr aFilterFunc) :
  mFilterFunc(aFilterFunc)
{
  mState = new IteratorState(aAccessible);
}

AccIterator::~AccIterator()
{
  while (mState) {
    IteratorState *tmp = mState;
    mState = tmp->mParentState;
    delete tmp;
  }
}

Accessible*
AccIterator::Next()
{
  while (mState) {
    Accessible* child = mState->mParent->GetChildAt(mState->mIndex++);
    if (!child) {
      IteratorState* tmp = mState;
      mState = mState->mParentState;
      delete tmp;

      continue;
    }

    uint32_t result = mFilterFunc(child);
    if (result & filters::eMatch)
      return child;

    if (!(result & filters::eSkipSubtree)) {
      IteratorState* childState = new IteratorState(child, mState);
      mState = childState;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccIterator::IteratorState

AccIterator::IteratorState::IteratorState(Accessible* aParent,
                                          IteratorState *mParentState) :
  mParent(aParent), mIndex(0), mParentState(mParentState)
{
}


////////////////////////////////////////////////////////////////////////////////
// RelatedAccIterator
////////////////////////////////////////////////////////////////////////////////

RelatedAccIterator::
  RelatedAccIterator(DocAccessible* aDocument, nsIContent* aDependentContent,
                     nsAtom* aRelAttr) :
  mDocument(aDocument), mRelAttr(aRelAttr), mProviders(nullptr),
  mBindingParent(nullptr), mIndex(0)
{
  mBindingParent = aDependentContent->GetBindingParent();
  nsAtom* IDAttr = mBindingParent ?
    nsGkAtoms::anonid : nsGkAtoms::id;

  nsAutoString id;
  if (aDependentContent->GetAttr(kNameSpaceID_None, IDAttr, id))
    mProviders = mDocument->mDependentIDsHash.Get(id);
}

Accessible*
RelatedAccIterator::Next()
{
  if (!mProviders)
    return nullptr;

  while (mIndex < mProviders->Length()) {
    DocAccessible::AttrRelProvider* provider = (*mProviders)[mIndex++];

    // Return related accessible for the given attribute and if the provider
    // content is in the same binding in the case of XBL usage.
    if (provider->mRelAttr == mRelAttr) {
      nsIContent* bindingParent = provider->mContent->GetBindingParent();
      bool inScope = mBindingParent == bindingParent ||
        mBindingParent == provider->mContent;

      if (inScope) {
        Accessible* related = mDocument->GetAccessible(provider->mContent);
        if (related)
          return related;

        // If the document content is pointed by relation then return the document
        // itself.
        if (provider->mContent == mDocument->GetContent())
          return mDocument;
      }
    }
  }

  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLLabelIterator
////////////////////////////////////////////////////////////////////////////////

HTMLLabelIterator::
  HTMLLabelIterator(DocAccessible* aDocument, const Accessible* aAccessible,
                    LabelFilter aFilter) :
  mRelIter(aDocument, aAccessible->GetContent(), nsGkAtoms::_for),
  mAcc(aAccessible), mLabelFilter(aFilter)
{
}

bool
HTMLLabelIterator::IsLabel(Accessible* aLabel)
{
  dom::HTMLLabelElement* labelEl =
    dom::HTMLLabelElement::FromContent(aLabel->GetContent());
  return labelEl && labelEl->GetControl() == mAcc->GetContent();
}

Accessible*
HTMLLabelIterator::Next()
{
  // Get either <label for="[id]"> element which explicitly points to given
  // element, or <label> ancestor which implicitly point to it.
  Accessible* label = nullptr;
  while ((label = mRelIter.Next())) {
    if (IsLabel(label)) {
      return label;
    }
  }

  // Ignore ancestor label on not widget accessible.
  if (mLabelFilter == eSkipAncestorLabel || !mAcc->IsWidget())
    return nullptr;

  // Go up tree to get a name of ancestor label if there is one (an ancestor
  // <label> implicitly points to us). Don't go up farther than form or
  // document.
  Accessible* walkUp = mAcc->Parent();
  while (walkUp && !walkUp->IsDoc()) {
    nsIContent* walkUpEl = walkUp->GetContent();
    if (IsLabel(walkUp) &&
        !walkUpEl->HasAttr(kNameSpaceID_None, nsGkAtoms::_for)) {
      mLabelFilter = eSkipAncestorLabel; // prevent infinite loop
      return walkUp;
    }

    if (walkUpEl->IsHTMLElement(nsGkAtoms::form))
      break;

    walkUp = walkUp->Parent();
  }

  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLOutputIterator
////////////////////////////////////////////////////////////////////////////////

HTMLOutputIterator::
HTMLOutputIterator(DocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsGkAtoms::_for)
{
}

Accessible*
HTMLOutputIterator::Next()
{
  Accessible* output = nullptr;
  while ((output = mRelIter.Next())) {
    if (output->GetContent()->IsHTMLElement(nsGkAtoms::output))
      return output;
  }

  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// XULLabelIterator
////////////////////////////////////////////////////////////////////////////////

XULLabelIterator::
  XULLabelIterator(DocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsGkAtoms::control)
{
}

Accessible*
XULLabelIterator::Next()
{
  Accessible* label = nullptr;
  while ((label = mRelIter.Next())) {
    if (label->GetContent()->IsXULElement(nsGkAtoms::label))
      return label;
  }

  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// XULDescriptionIterator
////////////////////////////////////////////////////////////////////////////////

XULDescriptionIterator::
  XULDescriptionIterator(DocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsGkAtoms::control)
{
}

Accessible*
XULDescriptionIterator::Next()
{
  Accessible* descr = nullptr;
  while ((descr = mRelIter.Next())) {
    if (descr->GetContent()->IsXULElement(nsGkAtoms::description))
      return descr;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// IDRefsIterator
////////////////////////////////////////////////////////////////////////////////

IDRefsIterator::
  IDRefsIterator(DocAccessible* aDoc, nsIContent* aContent,
                 nsAtom* aIDRefsAttr) :
  mContent(aContent), mDoc(aDoc), mCurrIdx(0)
{
  if (mContent->IsInUncomposedDoc())
    mContent->GetAttr(kNameSpaceID_None, aIDRefsAttr, mIDs);
}

const nsDependentSubstring
IDRefsIterator::NextID()
{
  for (; mCurrIdx < mIDs.Length(); mCurrIdx++) {
    if (!NS_IsAsciiWhitespace(mIDs[mCurrIdx]))
      break;
  }

  if (mCurrIdx >= mIDs.Length())
    return nsDependentSubstring();

  nsAString::index_type idStartIdx = mCurrIdx;
  while (++mCurrIdx < mIDs.Length()) {
    if (NS_IsAsciiWhitespace(mIDs[mCurrIdx]))
      break;
  }

  return Substring(mIDs, idStartIdx, mCurrIdx++ - idStartIdx);
}

nsIContent*
IDRefsIterator::NextElem()
{
  while (true) {
    const nsDependentSubstring id = NextID();
    if (id.IsEmpty())
      break;

    nsIContent* refContent = GetElem(id);
    if (refContent)
      return refContent;
  }

  return nullptr;
}

nsIContent*
IDRefsIterator::GetElem(const nsDependentSubstring& aID)
{
  // Get elements in DOM tree by ID attribute if this is an explicit content.
  // In case of bound element check its anonymous subtree.
  if (!mContent->IsInAnonymousSubtree()) {
    dom::Element* refElm = mContent->OwnerDoc()->GetElementById(aID);
    if (refElm || !mContent->GetXBLBinding())
      return refElm;
  }

  // If content is in anonymous subtree or an element having anonymous subtree
  // then use "anonid" attribute to get elements in anonymous subtree.

  // Check inside the binding the element is contained in.
  nsIContent* bindingParent = mContent->GetBindingParent();
  if (bindingParent) {
    nsIContent* refElm = bindingParent->OwnerDoc()->
      GetAnonymousElementByAttribute(bindingParent, nsGkAtoms::anonid, aID);

    if (refElm)
      return refElm;
  }

  // Check inside the binding of the element.
  if (mContent->GetXBLBinding()) {
    return mContent->OwnerDoc()->
      GetAnonymousElementByAttribute(mContent, nsGkAtoms::anonid, aID);
  }

  return nullptr;
}

Accessible*
IDRefsIterator::Next()
{
  nsIContent* nextEl = nullptr;
  while ((nextEl = NextElem())) {
    Accessible* acc = mDoc->GetAccessible(nextEl);
    if (acc) {
      return acc;
    }
  }
  return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// SingleAccIterator
////////////////////////////////////////////////////////////////////////////////

Accessible*
SingleAccIterator::Next()
{
  RefPtr<Accessible> nextAcc;
  mAcc.swap(nextAcc);
  if (!nextAcc || nextAcc->IsDefunct()) {
    return nullptr;
  }
  return nextAcc;
}


////////////////////////////////////////////////////////////////////////////////
// ItemIterator
////////////////////////////////////////////////////////////////////////////////

Accessible*
ItemIterator::Next()
{
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

XULTreeItemIterator::XULTreeItemIterator(XULTreeAccessible* aXULTree,
                                         nsITreeView* aTreeView,
                                         int32_t aRowIdx) :
  mXULTree(aXULTree), mTreeView(aTreeView), mRowCount(-1),
  mContainerLevel(-1), mCurrRowIdx(aRowIdx + 1)
{
  mTreeView->GetRowCount(&mRowCount);
  if (aRowIdx != -1)
    mTreeView->GetLevel(aRowIdx, &mContainerLevel);
}

Accessible*
XULTreeItemIterator::Next()
{
  while (mCurrRowIdx < mRowCount) {
    int32_t level = 0;
    mTreeView->GetLevel(mCurrRowIdx, &level);

    if (level == mContainerLevel + 1)
      return mXULTree->GetTreeItemAccessible(mCurrRowIdx++);

    if (level <= mContainerLevel) { // got level up
      mCurrRowIdx = mRowCount;
      break;
    }

    mCurrRowIdx++;
  }

  return nullptr;
}
