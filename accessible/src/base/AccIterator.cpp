/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "AccIterator.h"

#include "nsAccessibilityService.h"
#include "nsAccessible.h"

#include "mozilla/dom/Element.h"
#include "nsBindingManager.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////////////
// AccIterator
////////////////////////////////////////////////////////////////////////////////

AccIterator::AccIterator(nsAccessible *aAccessible,
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

nsAccessible*
AccIterator::Next()
{
  while (mState) {
    nsAccessible *child = mState->mParent->GetChildAt(mState->mIndex++);
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

AccIterator::IteratorState::IteratorState(nsAccessible *aParent,
                                          IteratorState *mParentState) :
  mParent(aParent), mIndex(0), mParentState(mParentState)
{
}


////////////////////////////////////////////////////////////////////////////////
// RelatedAccIterator
////////////////////////////////////////////////////////////////////////////////

RelatedAccIterator::
  RelatedAccIterator(nsDocAccessible* aDocument, nsIContent* aDependentContent,
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

nsAccessible*
RelatedAccIterator::Next()
{
  if (!mProviders)
    return nsnull;

  while (mIndex < mProviders->Length()) {
    nsDocAccessible::AttrRelProvider* provider = (*mProviders)[mIndex++];

    // Return related accessible for the given attribute and if the provider
    // content is in the same binding in the case of XBL usage.
    if (provider->mRelAttr == mRelAttr) {
      nsIContent* bindingParent = provider->mContent->GetBindingParent();
      bool inScope = mBindingParent == bindingParent ||
        mBindingParent == provider->mContent;

      if (inScope) {
        nsAccessible* related = mDocument->GetAccessible(provider->mContent);
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
  HTMLLabelIterator(nsDocAccessible* aDocument, const nsAccessible* aAccessible,
                    LabelFilter aFilter) :
  mRelIter(aDocument, aAccessible->GetContent(), nsGkAtoms::_for),
  mAcc(aAccessible), mLabelFilter(aFilter)
{
}

nsAccessible*
HTMLLabelIterator::Next()
{
  // Get either <label for="[id]"> element which explicitly points to given
  // element, or <label> ancestor which implicitly point to it.
  nsAccessible* label = nsnull;
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
  nsAccessible* walkUp = mAcc->Parent();
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
HTMLOutputIterator(nsDocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsGkAtoms::_for)
{
}

nsAccessible*
HTMLOutputIterator::Next()
{
  nsAccessible* output = nsnull;
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
  XULLabelIterator(nsDocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsGkAtoms::control)
{
}

nsAccessible*
XULLabelIterator::Next()
{
  nsAccessible* label = nsnull;
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
  XULDescriptionIterator(nsDocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsGkAtoms::control)
{
}

nsAccessible*
XULDescriptionIterator::Next()
{
  nsAccessible* descr = nsnull;
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
  IDRefsIterator(nsDocAccessible* aDoc, nsIContent* aContent,
                 nsIAtom* aIDRefsAttr) :
  mCurrIdx(0), mContent(aContent), mDoc(aDoc)
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
  nsCOMPtr<nsIDOMElement> refDOMElm;
  nsCOMPtr<nsIDOMDocumentXBL> xblDocument =
    do_QueryInterface(mContent->OwnerDoc());

  // Check inside the binding the element is contained in.
  nsIContent* bindingParent = mContent->GetBindingParent();
  if (bindingParent) {
    nsCOMPtr<nsIDOMElement> bindingParentElm = do_QueryInterface(bindingParent);
    xblDocument->GetAnonymousElementByAttribute(bindingParentElm,
                                                NS_LITERAL_STRING("anonid"),
                                                aID,
                                                getter_AddRefs(refDOMElm));
    nsCOMPtr<dom::Element> refElm = do_QueryInterface(refDOMElm);
    if (refElm)
      return refElm;
  }

  // Check inside the binding of the element.
  if (mContent->OwnerDoc()->BindingManager()->GetBinding(mContent)) {
    nsCOMPtr<nsIDOMElement> elm = do_QueryInterface(mContent);
    xblDocument->GetAnonymousElementByAttribute(elm,
                                                NS_LITERAL_STRING("anonid"),
                                                aID,
                                                getter_AddRefs(refDOMElm));
    nsCOMPtr<dom::Element> refElm = do_QueryInterface(refDOMElm);
    return refElm;
  }

  return nsnull;
}

nsAccessible*
IDRefsIterator::Next()
{
  nsIContent* nextElm = NextElem();
  return nextElm ? mDoc->GetAccessible(nextElm) : nsnull;
}

nsAccessible*
SingleAccIterator::Next()
{
  nsRefPtr<nsAccessible> nextAcc;
  mAcc.swap(nextAcc);
  return (nextAcc && !nextAcc->IsDefunct()) ? nextAcc : nsnull;
}

