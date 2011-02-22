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
AccIterator::GetNext()
{
  while (mState) {
    nsAccessible *child = mState->mParent->GetChildAt(mState->mIndex++);
    if (!child) {
      IteratorState *tmp = mState;
      mState = mState->mParentState;
      delete tmp;

      continue;
    }

    PRBool isComplying = mFilterFunc(child);
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
    nsAccessibilityAtoms::anonid : aDependentContent->GetIDAttributeName();

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
    if (provider->mRelAttr == mRelAttr &&
        (!mBindingParent ||
         mBindingParent == provider->mContent->GetBindingParent())) {
      nsAccessible* related = mDocument->GetAccessible(provider->mContent);
      if (related)
        return related;

      // If the document content is pointed by relation then return the document
      // itself.
      if (provider->mContent == mDocument->GetContent())
        return mDocument;
    }
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLLabelIterator
////////////////////////////////////////////////////////////////////////////////

HTMLLabelIterator::
  HTMLLabelIterator(nsDocAccessible* aDocument, nsIContent* aElement,
                    LabelFilter aFilter) :
  mRelIter(aDocument, aElement, nsAccessibilityAtoms::_for),
  mElement(aElement), mLabelFilter(aFilter)
{
}

nsAccessible*
HTMLLabelIterator::Next()
{
  // Get either <label for="[id]"> element which explicitly points to given
  // element, or <label> ancestor which implicitly point to it.
  nsAccessible* label = nsnull;
  while ((label = mRelIter.Next())) {
    if (label->GetContent()->Tag() == nsAccessibilityAtoms::label)
      return label;
  }

  if (mLabelFilter == eSkipAncestorLabel)
    return nsnull;

  // Go up tree get name of ancestor label if there is one (an ancestor <label>
  // implicitly points to us). Don't go up farther than form or body element.
  nsIContent* walkUpContent = mElement;
  while ((walkUpContent = walkUpContent->GetParent()) &&
         walkUpContent->Tag() != nsAccessibilityAtoms::form &&
         walkUpContent->Tag() != nsAccessibilityAtoms::body) {
    if (walkUpContent->Tag() == nsAccessibilityAtoms::label) {
      // Prevent infinite loop.
      mLabelFilter = eSkipAncestorLabel;
      return GetAccService()->GetAccessible(walkUpContent);
    }
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// HTMLOutputIterator
////////////////////////////////////////////////////////////////////////////////

HTMLOutputIterator::
HTMLOutputIterator(nsDocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsAccessibilityAtoms::_for)
{
}

nsAccessible*
HTMLOutputIterator::Next()
{
  nsAccessible* output = nsnull;
  while ((output = mRelIter.Next())) {
    if (output->GetContent()->Tag() == nsAccessibilityAtoms::output)
      return output;
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// XULLabelIterator
////////////////////////////////////////////////////////////////////////////////

XULLabelIterator::
  XULLabelIterator(nsDocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsAccessibilityAtoms::control)
{
}

nsAccessible*
XULLabelIterator::Next()
{
  nsAccessible* label = nsnull;
  while ((label = mRelIter.Next())) {
    if (label->GetContent()->Tag() == nsAccessibilityAtoms::label)
      return label;
  }

  return nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// XULDescriptionIterator
////////////////////////////////////////////////////////////////////////////////

XULDescriptionIterator::
  XULDescriptionIterator(nsDocAccessible* aDocument, nsIContent* aElement) :
  mRelIter(aDocument, aElement, nsAccessibilityAtoms::control)
{
}

nsAccessible*
XULDescriptionIterator::Next()
{
  nsAccessible* descr = nsnull;
  while ((descr = mRelIter.Next())) {
    if (descr->GetContent()->Tag() == nsAccessibilityAtoms::description)
      return descr;
  }

  return nsnull;
}
