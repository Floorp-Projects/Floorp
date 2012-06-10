/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccIterator.h"

#include "nsAccessibilityService.h"
#include "Accessible.h"

#include "mozilla/dom/Element.h"
#include "nsBindingManager.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// AccIterator
////////////////////////////////////////////////////////////////////////////////

AccIterator::AccIterator(Accessible* aAccessible,
                         filters::FilterFuncPtr aFilterFunc,
                         IterationType aIterationType) :
  mFilterFunc(aFilterFunc), mIsDeep(aIterationType != eFlatNav)
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
      IteratorState *tmp = mState;
      mState = mState->mParentState;
      delete tmp;

      continue;
    }

    bool isComplying = mFilterFunc(child);
    if (isComplying)
      return child;

    if (mIsDeep) {
      IteratorState *childState = new IteratorState(child, mState);
      mState = childState;
    }
  }

  return nsnull;
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
                     nsIAtom* aRelAttr) :
  mDocument(aDocument), mRelAttr(aRelAttr), mProviders(nsnull),
  mBindingParent(nsnull), mIndex(0)
{
  mBindingParent = aDependentContent->GetBindingParent();
  nsIAtom* IDAttr = mBindingParent ?
    nsGkAtoms::anonid : aDependentContent->GetIDAttributeName();

  nsAutoString id;
  if (aDependentContent->GetAttr(kNameSpaceID_None, IDAttr, id))
    mProviders = mDocument->mDependentIDsHash.Get(id);
}

Accessible*
RelatedAccIterator::Next()
{
  if (!mProviders)
    return nsnull;

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

  return nsnull;
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

Accessible*
HTMLLabelIterator::Next()
{
  // Get either <label for="[id]"> element which explicitly points to given
  // element, or <label> ancestor which implicitly point to it.
  Accessible* label = nsnull;
  while ((label = mRelIter.Next())) {
    if (label->GetContent()->Tag() == nsGkAtoms::label)
      return label;
  }

  // Ignore ancestor label on not widget accessible.
  if (mLabelFilter == eSkipAncestorLabel || !mAcc->IsWidget())
    return nsnull;

  // Go up tree to get a name of ancestor label if there is one (an ancestor
  // <label> implicitly points to us). Don't go up farther than form or
  // document.
  Accessible* walkUp = mAcc->Parent();
  while (walkUp && !walkUp->IsDoc()) {
    nsIContent* walkUpElm = walkUp->GetContent();
    if (walkUpElm->IsHTML()) {
      if (walkUpElm->Tag() == nsGkAtoms::label &&
          !walkUpElm->HasAttr(kNameSpaceID_None, nsGkAtoms::_for)) {
        mLabelFilter = eSkipAncestorLabel; // prevent infinite loop
        return walkUp;
      }

      if (walkUpElm->Tag() == nsGkAtoms::form)
        break;
    }

    walkUp = walkUp->Parent();
  }

  return nsnull;
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
  Accessible* output = nsnull;
  while ((output = mRelIter.Next())) {
    if (output->GetContent()->Tag() == nsGkAtoms::output)
      return output;
  }

  return nsnull;
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
  Accessible* label = nsnull;
  while ((label = mRelIter.Next())) {
    if (label->GetContent()->Tag() == nsGkAtoms::label)
      return label;
  }

  return nsnull;
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
  Accessible* descr = nsnull;
  while ((descr = mRelIter.Next())) {
    if (descr->GetContent()->Tag() == nsGkAtoms::description)
      return descr;
  }

  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// IDRefsIterator
////////////////////////////////////////////////////////////////////////////////

IDRefsIterator::
  IDRefsIterator(DocAccessible* aDoc, nsIContent* aContent,
                 nsIAtom* aIDRefsAttr) :
  mContent(aContent), mDoc(aDoc), mCurrIdx(0)
{
  if (mContent->IsInDoc())
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

  return nsnull;
}

nsIContent*
IDRefsIterator::GetElem(const nsDependentSubstring& aID)
{
  // Get elements in DOM tree by ID attribute if this is an explicit content.
  // In case of bound element check its anonymous subtree.
  if (!mContent->IsInAnonymousSubtree()) {
    dom::Element* refElm = mContent->OwnerDoc()->GetElementById(aID);
    if (refElm || !mContent->OwnerDoc()->BindingManager()->GetBinding(mContent))
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
  if (mContent->OwnerDoc()->BindingManager()->GetBinding(mContent)) {
    return mContent->OwnerDoc()->
      GetAnonymousElementByAttribute(mContent, nsGkAtoms::anonid, aID);
  }

  return nsnull;
}

Accessible*
IDRefsIterator::Next()
{
  nsIContent* nextElm = NextElem();
  return nextElm ? mDoc->GetAccessible(nextElm) : nsnull;
}

Accessible*
SingleAccIterator::Next()
{
  nsRefPtr<Accessible> nextAcc;
  mAcc.swap(nextAcc);
  return (nextAcc && !nextAcc->IsDefunct()) ? nextAcc : nsnull;
}

