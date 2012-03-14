/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mozilla/Util.h"

#include "nsHTMLSelectElement.h"

#include "nsHTMLOptionElement.h"
#include "nsIDOMEventTarget.h"
#include "nsContentCreatorFunctions.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsLayoutUtils.h"
#include "nsMappedAttributes.h"
#include "nsIForm.h"
#include "nsFormSubmission.h"
#include "nsIFormProcessor.h"
#include "nsContentCreatorFunctions.h"

#include "nsIDOMHTMLOptGroupElement.h"
#include "nsEventStates.h"
#include "nsGUIEvent.h"
#include "nsIPrivateDOMEvent.h"

// Notify/query select frame for selectedIndex
#include "nsIDocument.h"
#include "nsIFormControlFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIListControlFrame.h"
#include "nsIFrame.h"

#include "nsDOMError.h"
#include "nsServiceManagerUtils.h"
#include "nsRuleData.h"
#include "nsEventDispatcher.h"
#include "mozilla/dom/Element.h"
#include "mozAutoDocUpdate.h"
#include "dombindings.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS1(nsSelectState, nsSelectState)
NS_DEFINE_STATIC_IID_ACCESSOR(nsSelectState, NS_SELECT_STATE_IID)

//----------------------------------------------------------------------
//
// nsSafeOptionListMutation
//

nsSafeOptionListMutation::nsSafeOptionListMutation(nsIContent* aSelect,
                                                   nsIContent* aParent,
                                                   nsIContent* aKid,
                                                   PRUint32 aIndex,
                                                   bool aNotify)
  : mSelect(nsHTMLSelectElement::FromContent(aSelect))
  , mTopLevelMutation(false)
  , mNeedsRebuild(false)
{
  if (mSelect) {
    mTopLevelMutation = !mSelect->mMutating;
    if (mTopLevelMutation) {
      mSelect->mMutating = true;
    } else {
      // This is very unfortunate, but to handle mutation events properly,
      // option list must be up-to-date before inserting or removing options.
      // Fortunately this is called only if mutation event listener
      // adds or removes options.
      mSelect->RebuildOptionsArray(aNotify);
    }
    nsresult rv;
    if (aKid) {
      rv = mSelect->WillAddOptions(aKid, aParent, aIndex, aNotify);
    } else {
      rv = mSelect->WillRemoveOptions(aParent, aIndex, aNotify);
    }
    mNeedsRebuild = NS_FAILED(rv);
  }
}

nsSafeOptionListMutation::~nsSafeOptionListMutation()
{
  if (mSelect) {
    if (mNeedsRebuild || (mTopLevelMutation && mGuard.Mutated(1))) {
      mSelect->RebuildOptionsArray(true);
    }
    if (mTopLevelMutation) {
      mSelect->mMutating = false;
    }
#ifdef DEBUG
    mSelect->VerifyOptionsArray();
#endif
  }
}

//----------------------------------------------------------------------
//
// nsHTMLSelectElement
//

// construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Select)

nsHTMLSelectElement::nsHTMLSelectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                         FromParser aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mOptions(new nsHTMLOptionCollection(this)),
    mIsDoneAddingChildren(!aFromParser),
    mDisabledChanged(false),
    mMutating(false),
    mInhibitStateRestoration(!!(aFromParser & FROM_PARSER_FRAGMENT)),
    mSelectionHasChanged(false),
    mDefaultSelectionSet(false),
    mCanShowInvalidUI(true),
    mCanShowValidUI(true),
    mNonOptionChildren(0),
    mOptGroupCount(0),
    mSelectedIndex(-1)
{
  // DoneAddingChildren() will be called later if it's from the parser,
  // otherwise it is

  // Set up our default state: enabled, optional, and valid.
  AddStatesSilently(NS_EVENT_STATE_ENABLED |
                    NS_EVENT_STATE_OPTIONAL |
                    NS_EVENT_STATE_VALID);
}

nsHTMLSelectElement::~nsHTMLSelectElement()
{
  mOptions->DropReference();
}

// ISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLSelectElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLSelectElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mOptions,
                                                       nsIDOMHTMLCollection)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLSelectElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSelectElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLSelectElement, nsHTMLSelectElement)

// QueryInterface implementation for nsHTMLSelectElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLSelectElement)
  NS_HTML_CONTENT_INTERFACE_TABLE2(nsHTMLSelectElement,
                                   nsIDOMHTMLSelectElement,
                                   nsIConstraintValidation)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLSelectElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLSelectElement)


// nsIDOMHTMLSelectElement


NS_IMPL_ELEMENT_CLONE(nsHTMLSelectElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(nsHTMLSelectElement)

NS_IMETHODIMP
nsHTMLSelectElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

nsresult
nsHTMLSelectElement::InsertChildAt(nsIContent* aKid,
                                   PRUint32 aIndex,
                                   bool aNotify)
{
  nsSafeOptionListMutation safeMutation(this, this, aKid, aIndex, aNotify);
  nsresult rv = nsGenericHTMLFormElement::InsertChildAt(aKid, aIndex, aNotify);
  if (NS_FAILED(rv)) {
    safeMutation.MutationFailed();
  }
  return rv;
}

nsresult
nsHTMLSelectElement::RemoveChildAt(PRUint32 aIndex, bool aNotify)
{
  nsSafeOptionListMutation safeMutation(this, this, nsnull, aIndex, aNotify);
  nsresult rv = nsGenericHTMLFormElement::RemoveChildAt(aIndex, aNotify);
  if (NS_FAILED(rv)) {
    safeMutation.MutationFailed();
  }
  return rv;
}


// SelectElement methods

nsresult
nsHTMLSelectElement::InsertOptionsIntoList(nsIContent* aOptions,
                                           PRInt32 aListIndex,
                                           PRInt32 aDepth,
                                           bool aNotify)
{
  PRInt32 insertIndex = aListIndex;
  nsresult rv = InsertOptionsIntoListRecurse(aOptions, &insertIndex, aDepth);
  NS_ENSURE_SUCCESS(rv, rv);

  // Deal with the selected list
  if (insertIndex - aListIndex) {
    // Fix the currently selected index
    if (aListIndex <= mSelectedIndex) {
      mSelectedIndex += (insertIndex - aListIndex);
      SetSelectionChanged(true, aNotify);
    }

    // Get the frame stuff for notification. No need to flush here
    // since if there's no frame for the select yet the select will
    // get into the right state once it's created.
    nsISelectControlFrame* selectFrame = nsnull;
    nsWeakFrame weakSelectFrame;
    bool didGetFrame = false;

    // Actually select the options if the added options warrant it
    nsCOMPtr<nsIDOMNode> optionNode;
    nsCOMPtr<nsIDOMHTMLOptionElement> option;
    for (PRInt32 i = aListIndex; i < insertIndex; i++) {
      // Notify the frame that the option is added
      if (!didGetFrame || (selectFrame && !weakSelectFrame.IsAlive())) {
        selectFrame = GetSelectFrame();
        weakSelectFrame = do_QueryFrame(selectFrame);
        didGetFrame = true;
      }

      if (selectFrame) {
        selectFrame->AddOption(i);
      }

      Item(i, getter_AddRefs(optionNode));
      option = do_QueryInterface(optionNode);
      if (option) {
        bool selected;
        option->GetSelected(&selected);
        if (selected) {
          // Clear all other options
          if (!HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
            SetOptionsSelectedByIndex(i, i, true, true, true, true, nsnull);
          }

          // This is sort of a hack ... we need to notify that the option was
          // set and change selectedIndex even though we didn't really change
          // its value.
          OnOptionSelected(selectFrame, i, true, false, false);
        }
      }
    }

    CheckSelectSomething(aNotify);
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::RemoveOptionsFromList(nsIContent* aOptions,
                                           PRInt32 aListIndex,
                                           PRInt32 aDepth,
                                           bool aNotify)
{
  PRInt32 numRemoved = 0;
  nsresult rv = RemoveOptionsFromListRecurse(aOptions, aListIndex, &numRemoved,
                                             aDepth);
  NS_ENSURE_SUCCESS(rv, rv);

  if (numRemoved) {
    // Tell the widget we removed the options
    nsISelectControlFrame* selectFrame = GetSelectFrame();
    if (selectFrame) {
      nsAutoScriptBlocker scriptBlocker;
      for (PRInt32 i = aListIndex; i < aListIndex + numRemoved; ++i) {
        selectFrame->RemoveOption(i);
      }
    }

    // Fix the selected index
    if (aListIndex <= mSelectedIndex) {
      if (mSelectedIndex < (aListIndex+numRemoved)) {
        // aListIndex <= mSelectedIndex < aListIndex+numRemoved
        // Find a new selected index if it was one of the ones removed.
        FindSelectedIndex(aListIndex, aNotify);
      } else {
        // Shift the selected index if something in front of it was removed
        // aListIndex+numRemoved <= mSelectedIndex
        mSelectedIndex -= numRemoved;
        SetSelectionChanged(true, aNotify);
      }
    }

    // Select something in case we removed the selected option on a
    // single select
    if (!CheckSelectSomething(aNotify) && mSelectedIndex == -1) {
      // Update the validity state in case of we've just removed the last
      // option.
      UpdateValueMissingValidityState();

      UpdateState(aNotify);
    }
  }

  return NS_OK;
}

// If the document is such that recursing over these options gets us
// deeper than four levels, there is something terribly wrong with the
// world.
nsresult
nsHTMLSelectElement::InsertOptionsIntoListRecurse(nsIContent* aOptions,
                                                  PRInt32* aInsertIndex,
                                                  PRInt32 aDepth)
{
  // We *assume* here that someone's brain has not gone horribly
  // wrong by putting <option> inside of <option>.  I'm sorry, I'm
  // just not going to look for an option inside of an option.
  // Sue me.

  nsHTMLOptionElement *optElement = nsHTMLOptionElement::FromContent(aOptions);
  if (optElement) {
    nsresult rv = mOptions->InsertOptionAt(optElement, *aInsertIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aInsertIndex)++;
    return NS_OK;
  }

  // If it's at the top level, then we just found out there are non-options
  // at the top level, which will throw off the insert count
  if (aDepth == 0) {
    mNonOptionChildren++;
  }

  // Recurse down into optgroups
  if (aOptions->IsHTML(nsGkAtoms::optgroup)) {
    mOptGroupCount++;

    for (nsIContent* child = aOptions->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      nsresult rv = InsertOptionsIntoListRecurse(child,
                                                 aInsertIndex, aDepth + 1);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// If the document is such that recursing over these options gets us deeper than
// four levels, there is something terribly wrong with the world.
nsresult
nsHTMLSelectElement::RemoveOptionsFromListRecurse(nsIContent* aOptions,
                                                  PRInt32 aRemoveIndex,
                                                  PRInt32* aNumRemoved,
                                                  PRInt32 aDepth)
{
  // We *assume* here that someone's brain has not gone horribly
  // wrong by putting <option> inside of <option>.  I'm sorry, I'm
  // just not going to look for an option inside of an option.
  // Sue me.

  nsCOMPtr<nsIDOMHTMLOptionElement> optElement(do_QueryInterface(aOptions));
  if (optElement) {
    if (mOptions->ItemAsOption(aRemoveIndex) != optElement) {
      NS_ERROR("wrong option at index");
      return NS_ERROR_UNEXPECTED;
    }
    mOptions->RemoveOptionAt(aRemoveIndex);
    (*aNumRemoved)++;
    return NS_OK;
  }

  // Yay, one less artifact at the top level.
  if (aDepth == 0) {
    mNonOptionChildren--;
  }

  // Recurse down deeper for options
  if (mOptGroupCount && aOptions->IsHTML(nsGkAtoms::optgroup)) {
    mOptGroupCount--;

    for (nsIContent* child = aOptions->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      nsresult rv = RemoveOptionsFromListRecurse(child,
                                                 aRemoveIndex,
                                                 aNumRemoved,
                                                 aDepth + 1);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// XXXldb Doing the processing before the content nodes have been added
// to the document (as the name of this function seems to require, and
// as the callers do), is highly unusual.  Passing around unparented
// content to other parts of the app can make those things think the
// options are the root content node.
NS_IMETHODIMP
nsHTMLSelectElement::WillAddOptions(nsIContent* aOptions,
                                    nsIContent* aParent,
                                    PRInt32 aContentIndex,
                                    bool aNotify)
{
  PRInt32 level = GetContentDepth(aParent);
  if (level == -1) {
    return NS_ERROR_FAILURE;
  }

  // Get the index where the options will be inserted
  PRInt32 ind = -1;
  if (!mNonOptionChildren) {
    // If there are no artifacts, aContentIndex == ind
    ind = aContentIndex;
  } else {
    // If there are artifacts, we have to get the index of the option the
    // hard way
    PRInt32 children = aParent->GetChildCount();

    if (aContentIndex >= children) {
      // If the content insert is after the end of the parent, then we want to get
      // the next index *after* the parent and insert there.
      ind = GetOptionIndexAfter(aParent);
    } else {
      // If the content insert is somewhere in the middle of the container, then
      // we want to get the option currently at the index and insert in front of
      // that.
      nsIContent *currentKid = aParent->GetChildAt(aContentIndex);
      NS_ASSERTION(currentKid, "Child not found!");
      if (currentKid) {
        ind = GetOptionIndexAt(currentKid);
      } else {
        ind = -1;
      }
    }
  }

  return InsertOptionsIntoList(aOptions, ind, level, aNotify);
}

NS_IMETHODIMP
nsHTMLSelectElement::WillRemoveOptions(nsIContent* aParent,
                                       PRInt32 aContentIndex,
                                       bool aNotify)
{
  PRInt32 level = GetContentDepth(aParent);
  NS_ASSERTION(level >= 0, "getting notified by unexpected content");
  if (level == -1) {
    return NS_ERROR_FAILURE;
  }

  // Get the index where the options will be removed
  nsIContent *currentKid = aParent->GetChildAt(aContentIndex);
  if (currentKid) {
    PRInt32 ind;
    if (!mNonOptionChildren) {
      // If there are no artifacts, aContentIndex == ind
      ind = aContentIndex;
    } else {
      // If there are artifacts, we have to get the index of the option the
      // hard way
      ind = GetFirstOptionIndex(currentKid);
    }
    if (ind != -1) {
      nsresult rv = RemoveOptionsFromList(currentKid, ind, level, aNotify);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

PRInt32
nsHTMLSelectElement::GetContentDepth(nsIContent* aContent)
{
  nsIContent* content = aContent;

  PRInt32 retval = 0;
  while (content != this) {
    retval++;
    content = content->GetParent();
    if (!content) {
      retval = -1;
      break;
    }
  }

  return retval;
}

PRInt32
nsHTMLSelectElement::GetOptionIndexAt(nsIContent* aOptions)
{
  // Search this node and below.
  // If not found, find the first one *after* this node.
  PRInt32 retval = GetFirstOptionIndex(aOptions);
  if (retval == -1) {
    retval = GetOptionIndexAfter(aOptions);
  }

  return retval;
}

PRInt32
nsHTMLSelectElement::GetOptionIndexAfter(nsIContent* aOptions)
{
  // - If this is the select, the next option is the last.
  // - If not, search all the options after aOptions and up to the last option
  //   in the parent.
  // - If it's not there, search for the first option after the parent.
  if (aOptions == this) {
    PRUint32 len;
    GetLength(&len);
    return len;
  }

  PRInt32 retval = -1;

  nsCOMPtr<nsIContent> parent = aOptions->GetParent();

  if (parent) {
    PRInt32 index = parent->IndexOf(aOptions);
    PRInt32 count = parent->GetChildCount();

    retval = GetFirstChildOptionIndex(parent, index+1, count);

    if (retval == -1) {
      retval = GetOptionIndexAfter(parent);
    }
  }

  return retval;
}

PRInt32
nsHTMLSelectElement::GetFirstOptionIndex(nsIContent* aOptions)
{
  PRInt32 listIndex = -1;
  nsHTMLOptionElement *optElement = nsHTMLOptionElement::FromContent(aOptions);
  if (optElement) {
    GetOptionIndex(optElement, 0, true, &listIndex);
    // If you nested stuff under the option, you're just plain
    // screwed.  *I'm* not going to aid and abet your evil deed.
    return listIndex;
  }

  listIndex = GetFirstChildOptionIndex(aOptions, 0, aOptions->GetChildCount());

  return listIndex;
}

PRInt32
nsHTMLSelectElement::GetFirstChildOptionIndex(nsIContent* aOptions,
                                              PRInt32 aStartIndex,
                                              PRInt32 aEndIndex)
{
  PRInt32 retval = -1;

  for (PRInt32 i = aStartIndex; i < aEndIndex; ++i) {
    retval = GetFirstOptionIndex(aOptions->GetChildAt(i));
    if (retval != -1) {
      break;
    }
  }

  return retval;
}

nsISelectControlFrame *
nsHTMLSelectElement::GetSelectFrame()
{
  nsIFormControlFrame* form_control_frame = GetFormControlFrame(false);

  nsISelectControlFrame *select_frame = nsnull;

  if (form_control_frame) {
    select_frame = do_QueryFrame(form_control_frame);
  }

  return select_frame;
}

nsresult
nsHTMLSelectElement::Add(nsIDOMHTMLElement* aElement,
                         nsIDOMHTMLElement* aBefore)
{
  nsCOMPtr<nsIDOMNode> added;
  if (!aBefore) {
    return AppendChild(aElement, getter_AddRefs(added));
  }

  // Just in case we're not the parent, get the parent of the reference
  // element
  nsCOMPtr<nsIDOMNode> parent;
  aBefore->GetParentNode(getter_AddRefs(parent));
  if (!parent) {
    // NOT_FOUND_ERR: Raised if before is not a descendant of the SELECT
    // element.
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsCOMPtr<nsIDOMNode> ancestor(parent);
  nsCOMPtr<nsIDOMNode> temp;
  while (ancestor != static_cast<nsIDOMNode*>(this)) {
    ancestor->GetParentNode(getter_AddRefs(temp));
    if (!temp) {
      // NOT_FOUND_ERR: Raised if before is not a descendant of the SELECT
      // element.
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }
    temp.swap(ancestor);
  }

  // If the before parameter is not null, we are equivalent to the
  // insertBefore method on the parent of before.
  return parent->InsertBefore(aElement, aBefore, getter_AddRefs(added));
}

NS_IMETHODIMP
nsHTMLSelectElement::Add(nsIDOMHTMLElement* aElement,
                         nsIVariant* aBefore)
{
  PRUint16 dataType;
  nsresult rv = aBefore->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  // aBefore is omitted, undefined or null
  if (dataType == nsIDataType::VTYPE_EMPTY ||
      dataType == nsIDataType::VTYPE_VOID) {
    return Add(aElement);
  }

  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIDOMHTMLElement> beforeElement;

  // whether aBefore is nsIDOMHTMLElement...
  if (NS_SUCCEEDED(aBefore->GetAsISupports(getter_AddRefs(supports)))) {
    beforeElement = do_QueryInterface(supports);

    NS_ENSURE_TRUE(beforeElement, NS_ERROR_DOM_SYNTAX_ERR);
    return Add(aElement, beforeElement);
  }

  // otherwise, whether aBefore is long
  PRInt32 index;
  NS_ENSURE_SUCCESS(aBefore->GetAsInt32(&index), NS_ERROR_DOM_SYNTAX_ERR);

  // If item index is out of range, insert to last.
  // (since beforeElement becomes null, it is inserted to last)
  nsCOMPtr<nsIDOMNode> beforeNode;
  if (NS_SUCCEEDED(Item(index, getter_AddRefs(beforeNode)))) {
    beforeElement = do_QueryInterface(beforeNode);
  }

  return Add(aElement, beforeElement);
}

NS_IMETHODIMP
nsHTMLSelectElement::Remove(PRInt32 aIndex)
{
  nsCOMPtr<nsIDOMNode> option;
  Item(aIndex, getter_AddRefs(option));

  if (option) {
    nsCOMPtr<nsIDOMNode> parent;

    option->GetParentNode(getter_AddRefs(parent));
    if (parent) {
      nsCOMPtr<nsIDOMNode> ret;
      parent->RemoveChild(option, getter_AddRefs(ret));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptions(nsIDOMHTMLOptionsCollection** aValue)
{
  NS_IF_ADDREF(*aValue = GetOptions());

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetType(nsAString& aType)
{
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
    aType.AssignLiteral("select-multiple");
  }
  else {
    aType.AssignLiteral("select-one");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetLength(PRUint32* aLength)
{
  return mOptions->GetLength(aLength);
}

#define MAX_DYNAMIC_SELECT_LENGTH 10000

NS_IMETHODIMP
nsHTMLSelectElement::SetLength(PRUint32 aLength)
{
  PRUint32 curlen;
  nsresult rv = GetLength(&curlen);
  if (NS_FAILED(rv)) {
    curlen = 0;
  }

  if (curlen > aLength) { // Remove extra options
    for (PRUint32 i = curlen; i > aLength && NS_SUCCEEDED(rv); --i) {
      rv = Remove(i - 1);
    }
  } else if (aLength > curlen) {
    if (aLength > MAX_DYNAMIC_SELECT_LENGTH) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
    
    // This violates the W3C DOM but we do this for backwards compatibility
    nsCOMPtr<nsINodeInfo> nodeInfo;

    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::option,
                                getter_AddRefs(nodeInfo));

    nsCOMPtr<nsIContent> element = NS_NewHTMLOptionElement(nodeInfo.forget());
    if (!element) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIContent> text;
    rv = NS_NewTextNode(getter_AddRefs(text), mNodeInfo->NodeInfoManager());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = element->AppendChildTo(text, false);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(element));

    for (PRUint32 i = curlen; i < aLength; i++) {
      nsCOMPtr<nsIDOMNode> tmpNode;

      rv = AppendChild(node, getter_AddRefs(tmpNode));
      NS_ENSURE_SUCCESS(rv, rv);

      if (i + 1 < aLength) {
        nsCOMPtr<nsIDOMNode> newNode;

        rv = node->CloneNode(true, 1, getter_AddRefs(newNode));
        NS_ENSURE_SUCCESS(rv, rv);

        node = newNode;
      }
    }
  }

  return NS_OK;
}

//NS_IMPL_INT_ATTR(nsHTMLSelectElement, SelectedIndex, selectedindex)

NS_IMETHODIMP
nsHTMLSelectElement::GetSelectedIndex(PRInt32* aValue)
{
  *aValue = mSelectedIndex;

  return NS_OK;
}

nsresult
nsHTMLSelectElement::SetSelectedIndexInternal(PRInt32 aIndex, bool aNotify)
{
  PRInt32 oldSelectedIndex = mSelectedIndex;

  nsresult rv = SetOptionsSelectedByIndex(aIndex, aIndex, true,
                                          true, true, aNotify, nsnull);

  if (NS_SUCCEEDED(rv)) {
    nsISelectControlFrame* selectFrame = GetSelectFrame();
    if (selectFrame) {
      rv = selectFrame->OnSetSelectedIndex(oldSelectedIndex, mSelectedIndex);
    }
  }

  SetSelectionChanged(true, aNotify);

  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetSelectedIndex(PRInt32 aIndex)
{
  return SetSelectedIndexInternal(aIndex, true);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                                    PRInt32 aStartIndex, bool aForward,
                                    PRInt32* aIndex)
{
  nsCOMPtr<nsINode> option = do_QueryInterface(aOption);
  return mOptions->GetOptionIndex(option->AsElement(), aStartIndex, aForward, aIndex);
}

bool
nsHTMLSelectElement::IsOptionSelectedByIndex(PRInt32 aIndex)
{
  nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(aIndex);
  bool isSelected = false;
  if (option) {
    option->GetSelected(&isSelected);
  }
  return isSelected;
}

void
nsHTMLSelectElement::OnOptionSelected(nsISelectControlFrame* aSelectFrame,
                                      PRInt32 aIndex,
                                      bool aSelected,
                                      bool aChangeOptionState,
                                      bool aNotify)
{
  // Set the selected index
  if (aSelected && (aIndex < mSelectedIndex || mSelectedIndex < 0)) {
    mSelectedIndex = aIndex;
    SetSelectionChanged(true, aNotify);
  } else if (!aSelected && aIndex == mSelectedIndex) {
    FindSelectedIndex(aIndex + 1, aNotify);
  }

  if (aChangeOptionState) {
    // Tell the option to get its bad self selected
    nsCOMPtr<nsIDOMNode> option;
    Item(aIndex, getter_AddRefs(option));
    if (option) {
      nsRefPtr<nsHTMLOptionElement> optionElement = 
        static_cast<nsHTMLOptionElement*>(option.get());
      optionElement->SetSelectedInternal(aSelected, aNotify);
    }
  }

  // Let the frame know too
  if (aSelectFrame) {
    aSelectFrame->OnOptionSelected(aIndex, aSelected);
  }

  UpdateValueMissingValidityState();
  UpdateState(aNotify);
}

void
nsHTMLSelectElement::FindSelectedIndex(PRInt32 aStartIndex, bool aNotify)
{
  mSelectedIndex = -1;
  SetSelectionChanged(true, aNotify);
  PRUint32 len;
  GetLength(&len);
  for (PRInt32 i = aStartIndex; i < PRInt32(len); i++) {
    if (IsOptionSelectedByIndex(i)) {
      mSelectedIndex = i;
      SetSelectionChanged(true, aNotify);
      break;
    }
  }
}

// XXX Consider splitting this into two functions for ease of reading:
// SelectOptionsByIndex(startIndex, endIndex, clearAll, checkDisabled)
//   startIndex, endIndex - the range of options to turn on
//                          (-1, -1) will clear all indices no matter what.
//   clearAll - will clear all other options unless checkDisabled is on
//              and all the options attempted to be set are disabled
//              (note that if it is not multiple, and an option is selected,
//              everything else will be cleared regardless).
//   checkDisabled - if this is TRUE, and an option is disabled, it will not be
//                   changed regardless of whether it is selected or not.
//                   Generally the UI passes TRUE and JS passes FALSE.
//                   (setDisabled currently is the opposite)
// DeselectOptionsByIndex(startIndex, endIndex, checkDisabled)
//   startIndex, endIndex - the range of options to turn on
//                          (-1, -1) will clear all indices no matter what.
//   checkDisabled - if this is TRUE, and an option is disabled, it will not be
//                   changed regardless of whether it is selected or not.
//                   Generally the UI passes TRUE and JS passes FALSE.
//                   (setDisabled currently is the opposite)
//
// XXXbz the above comment is pretty confusing.  Maybe we should actually
// document the args to this function too, in addition to documenting what
// things might end up looking like?  In particular, pay attention to the
// setDisabled vs checkDisabled business.
NS_IMETHODIMP
nsHTMLSelectElement::SetOptionsSelectedByIndex(PRInt32 aStartIndex,
                                               PRInt32 aEndIndex,
                                               bool aIsSelected,
                                               bool aClearAll,
                                               bool aSetDisabled,
                                               bool aNotify,
                                               bool* aChangedSomething)
{
#if 0
  printf("SetOption(%d-%d, %c, ClearAll=%c)\n", aStartIndex, aEndIndex,
                                       (aIsSelected ? 'Y' : 'N'),
                                       (aClearAll ? 'Y' : 'N'));
#endif
  if (aChangedSomething) {
    *aChangedSomething = false;
  }

  // Don't bother if the select is disabled
  if (!aSetDisabled && IsDisabled()) {
    return NS_OK;
  }

  // Don't bother if there are no options
  PRUint32 numItems = 0;
  GetLength(&numItems);
  if (numItems == 0) {
    return NS_OK;
  }

  // First, find out whether multiple items can be selected
  bool isMultiple = HasAttr(kNameSpaceID_None, nsGkAtoms::multiple);

  // These variables tell us whether any options were selected
  // or deselected.
  bool optionsSelected = false;
  bool optionsDeselected = false;

  nsISelectControlFrame *selectFrame = nsnull;
  bool didGetFrame = false;
  nsWeakFrame weakSelectFrame;

  if (aIsSelected) {
    // Setting selectedIndex to an out-of-bounds index means -1. (HTML5)
    if (aStartIndex >= (PRInt32)numItems || aStartIndex < 0 ||
        aEndIndex >= (PRInt32)numItems || aEndIndex < 0) {
      aStartIndex = -1;
      aEndIndex = -1;
    }

    // Only select the first value if it's not multiple
    if (!isMultiple) {
      aEndIndex = aStartIndex;
    }

    // This variable tells whether or not all of the options we attempted to
    // select are disabled.  If ClearAll is passed in as true, and we do not
    // select anything because the options are disabled, we will not clear the
    // other options.  (This is to make the UI work the way one might expect.)
    bool allDisabled = !aSetDisabled;

    //
    // Save a little time when clearing other options
    //
    PRInt32 previousSelectedIndex = mSelectedIndex;

    //
    // Select the requested indices
    //
    // If index is -1, everything will be deselected (bug 28143)
    if (aStartIndex != -1) {
      // Loop through the options and select them (if they are not disabled and
      // if they are not already selected).
      for (PRInt32 optIndex = aStartIndex; optIndex <= aEndIndex; optIndex++) {

        // Ignore disabled options.
        if (!aSetDisabled) {
          bool isDisabled;
          IsOptionDisabled(optIndex, &isDisabled);

          if (isDisabled) {
            continue;
          } else {
            allDisabled = false;
          }
        }

        nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
        if (option) {
          // If the index is already selected, ignore it.
          bool isSelected = false;
          option->GetSelected(&isSelected);
          if (!isSelected) {
            // To notify the frame if anything gets changed. No need
            // to flush here, if there's no frame yet we don't need to
            // force it to be created just to notify it about a change
            // in the select.
            selectFrame = GetSelectFrame();
            weakSelectFrame = do_QueryFrame(selectFrame);
            didGetFrame = true;

            OnOptionSelected(selectFrame, optIndex, true, true, aNotify);
            optionsSelected = true;
          }
        }
      }
    }

    // Next remove all other options if single select or all is clear
    // If index is -1, everything will be deselected (bug 28143)
    if (((!isMultiple && optionsSelected)
       || (aClearAll && !allDisabled)
       || aStartIndex == -1)
       && previousSelectedIndex != -1) {
      for (PRInt32 optIndex = previousSelectedIndex;
           optIndex < PRInt32(numItems);
           optIndex++) {
        if (optIndex < aStartIndex || optIndex > aEndIndex) {
          nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
          if (option) {
            // If the index is already selected, ignore it.
            bool isSelected = false;
            option->GetSelected(&isSelected);
            if (isSelected) {
              if (!didGetFrame || (selectFrame && !weakSelectFrame.IsAlive())) {
                // To notify the frame if anything gets changed, don't
                // flush, if the frame doesn't exist we don't need to
                // create it just to tell it about this change.
                selectFrame = GetSelectFrame();
                weakSelectFrame = do_QueryFrame(selectFrame);

                didGetFrame = true;
              }

              OnOptionSelected(selectFrame, optIndex, false, true,
                               aNotify);
              optionsDeselected = true;

              // Only need to deselect one option if not multiple
              if (!isMultiple) {
                break;
              }
            }
          }
        }
      }
    }

  } else {

    // If we're deselecting, loop through all selected items and deselect
    // any that are in the specified range.
    for (PRInt32 optIndex = aStartIndex; optIndex <= aEndIndex; optIndex++) {
      if (!aSetDisabled) {
        bool isDisabled;
        IsOptionDisabled(optIndex, &isDisabled);
        if (isDisabled) {
          continue;
        }
      }

      nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
      if (option) {
        // If the index is already selected, ignore it.
        bool isSelected = false;
        option->GetSelected(&isSelected);
        if (isSelected) {
          if (!didGetFrame || (selectFrame && !weakSelectFrame.IsAlive())) {
            // To notify the frame if anything gets changed, don't
            // flush, if the frame doesn't exist we don't need to
            // create it just to tell it about this change.
            selectFrame = GetSelectFrame();
            weakSelectFrame = do_QueryFrame(selectFrame);

            didGetFrame = true;
          }

          OnOptionSelected(selectFrame, optIndex, false, true, aNotify);
          optionsDeselected = true;
        }
      }
    }
  }

  // Make sure something is selected unless we were set to -1 (none)
  if (optionsDeselected && aStartIndex != -1) {
    optionsSelected = CheckSelectSomething(aNotify) || optionsSelected;
  }

  // Let the caller know whether anything was changed
  if (optionsSelected || optionsDeselected) {
    if (aChangedSomething)
      *aChangedSomething = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::IsOptionDisabled(PRInt32 aIndex, bool* aIsDisabled)
{
  *aIsDisabled = false;
  nsCOMPtr<nsIDOMNode> optionNode;
  Item(aIndex, getter_AddRefs(optionNode));
  NS_ENSURE_TRUE(optionNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(optionNode);
  if (option) {
    bool isDisabled;
    option->GetDisabled(&isDisabled);
    if (isDisabled) {
      *aIsDisabled = true;
      return NS_OK;
    }
  }

  // Check for disabled optgroups
  // If there are no artifacts, there are no optgroups
  if (mNonOptionChildren) {
    nsCOMPtr<nsIDOMNode> parent;
    while (1) {
      optionNode->GetParentNode(getter_AddRefs(parent));

      // If we reached the top of the doc (scary), we're done
      if (!parent) {
        break;
      }

      // If we reached the select element, we're done
      nsCOMPtr<nsIDOMHTMLSelectElement> selectElement =
        do_QueryInterface(parent);
      if (selectElement) {
        break;
      }

      nsCOMPtr<nsIDOMHTMLOptGroupElement> optGroupElement =
        do_QueryInterface(parent);

      if (optGroupElement) {
        bool isDisabled;
        optGroupElement->GetDisabled(&isDisabled);

        if (isDisabled) {
          *aIsDisabled = true;
          return NS_OK;
        }
      } else {
        // If you put something else between you and the optgroup, you're a
        // moron and you deserve not to have optgroup disabling work.
        break;
      }

      optionNode = parent;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetValue(nsAString& aValue)
{
  PRInt32 selectedIndex;

  nsresult rv = GetSelectedIndex(&selectedIndex);

  if (NS_SUCCEEDED(rv) && selectedIndex > -1) {
    nsCOMPtr<nsIDOMNode> node;

    rv = Item(selectedIndex, getter_AddRefs(node));

    nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(node);
    if (NS_SUCCEEDED(rv) && option) {
      return option->GetValue(aValue);
    }
  }

  aValue.Truncate();
  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetValue(const nsAString& aValue)
{
  PRUint32 length;
  nsresult rv = GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = Item(i, getter_AddRefs(node));
    if (NS_FAILED(rv) || !node) {
      continue;
    }

    nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(node);
    if (!option) {
      continue;
    }
    nsAutoString optionVal;
    option->GetValue(optionVal);
    if (optionVal.Equals(aValue)) {
      SetSelectedIndexInternal(PRInt32(i), true);
      break;
    }
  }
  return rv;
}


NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Autofocus, autofocus)
NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Disabled, disabled)
NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Multiple, multiple)
NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Required, required)
NS_IMPL_NON_NEGATIVE_INT_ATTR_DEFAULT_VALUE(nsHTMLSelectElement, Size, size, 0)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, TabIndex, tabindex)

bool
nsHTMLSelectElement::IsHTMLFocusable(bool aWithMouse,
                                     bool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLFormElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  *aIsFocusable = !IsDisabled();

  return false;
}

NS_IMETHODIMP
nsHTMLSelectElement::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  return mOptions->Item(aIndex, aReturn);
}

NS_IMETHODIMP
nsHTMLSelectElement::NamedItem(const nsAString& aName,
                               nsIDOMNode** aReturn)
{
  return mOptions->NamedItem(aName, aReturn);
}

bool
nsHTMLSelectElement::CheckSelectSomething(bool aNotify)
{
  if (mIsDoneAddingChildren) {
    if (mSelectedIndex < 0 && IsCombobox()) {
      return SelectSomething(aNotify);
    }
  }
  return false;
}

bool
nsHTMLSelectElement::SelectSomething(bool aNotify)
{
  // If we're not done building the select, don't play with this yet.
  if (!mIsDoneAddingChildren) {
    return false;
  }

  PRUint32 count;
  GetLength(&count);
  for (PRUint32 i = 0; i < count; i++) {
    bool disabled;
    nsresult rv = IsOptionDisabled(i, &disabled);

    if (NS_FAILED(rv) || !disabled) {
      rv = SetSelectedIndexInternal(i, aNotify);
      NS_ENSURE_SUCCESS(rv, false);

      UpdateValueMissingValidityState();
      UpdateState(aNotify);

      return true;
    }
  }

  return false;
}

nsresult
nsHTMLSelectElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // If there is a disabled fieldset in the parent chain, the element is now
  // barred from constraint validation.
  // XXXbz is this still needed now that fieldset changes always call
  // FieldSetDisabledChanged?
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);

  return rv;
}

void
nsHTMLSelectElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);

  // We might be no longer disabled because our parent chain changed.
  // XXXbz is this still needed now that fieldset changes always call
  // FieldSetDisabledChanged?
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

nsresult
nsHTMLSelectElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                   const nsAttrValueOrString* aValue,
                                   bool aNotify)
{
  if (aNotify && aName == nsGkAtoms::disabled &&
      aNameSpaceID == kNameSpaceID_None) {
    mDisabledChanged = true;
  }

  return nsGenericHTMLFormElement::BeforeSetAttr(aNameSpaceID, aName,
                                                 aValue, aNotify);
}

nsresult
nsHTMLSelectElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::disabled) {
      UpdateBarredFromConstraintValidation();
    } else if (aName == nsGkAtoms::required) {
      UpdateValueMissingValidityState();
    }

    UpdateState(aNotify);
  }

  return nsGenericHTMLFormElement::AfterSetAttr(aNameSpaceID, aName,
                                                aValue, aNotify);
}

nsresult
nsHTMLSelectElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                               bool aNotify)
{
  if (aNotify && aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::multiple) {
    // We're changing from being a multi-select to a single-select.
    // Make sure we only have one option selected before we do that.
    // Note that this needs to come before we really unset the attr,
    // since SetOptionsSelectedByIndex does some bail-out type
    // optimization for cases when the select is not multiple that
    // would lead to only a single option getting deselected.
    if (mSelectedIndex >= 0) {
      SetSelectedIndexInternal(mSelectedIndex, aNotify);
    }
  }

  nsresult rv = nsGenericHTMLFormElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                    aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNotify && aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::multiple) {
    // We might have become a combobox; make sure _something_ gets
    // selected in that case
    CheckSelectSomething(aNotify);
  }

  return rv;
}

void
nsHTMLSelectElement::DoneAddingChildren(bool aHaveNotified)
{
  mIsDoneAddingChildren = true;

  nsISelectControlFrame* selectFrame = GetSelectFrame();

  // If we foolishly tried to restore before we were done adding
  // content, restore the rest of the options proper-like
  if (mRestoreState) {
    RestoreStateTo(mRestoreState);
    mRestoreState = nsnull;
  }

  // Notify the frame
  if (selectFrame) {
    selectFrame->DoneAddingChildren(true);
  }

  // Restore state
  if (!mInhibitStateRestoration) {
    RestoreFormControlState(this, this);
  }

  // Now that we're done, select something (if it's a single select something
  // must be selected)
  if (!CheckSelectSomething(false)) {
    // If an option has @selected set, it will be selected during parsing but
    // with an empty value. We have to make sure the select element updates it's
    // validity state to take this into account.
    UpdateValueMissingValidityState();

    // And now make sure we update our content state too
    UpdateState(aHaveNotified);
  }

  mDefaultSelectionSet = true;
}

bool
nsHTMLSelectElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::size && kNameSpaceID_None == aNamespaceID) {
    return aResult.ParsePositiveIntValue(aValue);
  }
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
nsHTMLSelectElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            PRInt32 aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLFormElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::multiple ||
      aAttribute == nsGkAtoms::size) {
    NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
  }
  return retval;
}

NS_IMETHODIMP_(bool)
nsHTMLSelectElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageAlignAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
nsHTMLSelectElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}


nsresult
nsHTMLSelectElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = nsnull;
  if (formControlFrame) {
    formFrame = do_QueryFrame(formControlFrame);
  }

  aVisitor.mCanHandle = false;
  if (IsElementDisabledForEvents(aVisitor.mEvent->message, formFrame)) {
    return NS_OK;
  }

  return nsGenericHTMLFormElement::PreHandleEvent(aVisitor);
}

nsresult
nsHTMLSelectElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->message == NS_FOCUS_CONTENT) {
    // If the invalid UI is shown, we should show it while focused and
    // update the invalid/valid UI.
    mCanShowInvalidUI = !IsValid() && ShouldShowValidityUI();

    // If neither invalid UI nor valid UI is shown, we shouldn't show the valid
    // UI while focused.
    mCanShowValidUI = ShouldShowValidityUI();

    // We don't have to update NS_EVENT_STATE_MOZ_UI_INVALID nor
    // NS_EVENT_STATE_MOZ_UI_VALID given that the states should not change.
  } else if (aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    mCanShowInvalidUI = true;
    mCanShowValidUI = true;

    UpdateState(true);
  }

  return nsGenericHTMLFormElement::PostHandleEvent(aVisitor);
}

nsEventStates
nsHTMLSelectElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLFormElement::IntrinsicState();

  if (IsCandidateForConstraintValidation()) {
    if (IsValid()) {
      state |= NS_EVENT_STATE_VALID;
    } else {
      state |= NS_EVENT_STATE_INVALID;

      if ((!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) &&
          (GetValidityState(VALIDITY_STATE_CUSTOM_ERROR) ||
           (mCanShowInvalidUI && ShouldShowValidityUI()))) {
        state |= NS_EVENT_STATE_MOZ_UI_INVALID;
      }
    }

    // :-moz-ui-valid applies if all the following are true:
    // 1. The element is not focused, or had either :-moz-ui-valid or
    //    :-moz-ui-invalid applying before it was focused ;
    // 2. The element is either valid or isn't allowed to have
    //    :-moz-ui-invalid applying ;
    // 3. The element has no form owner or its form owner doesn't have the
    //    novalidate attribute set ;
    // 4. The element has already been modified or the user tried to submit the
    //    form owner while invalid.
    if ((!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) &&
        (mCanShowValidUI && ShouldShowValidityUI() &&
         (IsValid() || (state.HasState(NS_EVENT_STATE_MOZ_UI_INVALID) &&
                        !mCanShowInvalidUI)))) {
      state |= NS_EVENT_STATE_MOZ_UI_VALID;
    }
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    state |= NS_EVENT_STATE_REQUIRED;
  } else {
    state |= NS_EVENT_STATE_OPTIONAL;
  }

  return state;
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLSelectElement::SaveState()
{
  nsRefPtr<nsSelectState> state = new nsSelectState();

  PRUint32 len;
  GetLength(&len);

  for (PRUint32 optIndex = 0; optIndex < len; optIndex++) {
    nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
    if (option) {
      bool isSelected;
      option->GetSelected(&isSelected);
      if (isSelected) {
        nsAutoString value;
        option->GetValue(value);
        state->PutOption(optIndex, value);
      }
    }
  }

  nsPresState *presState = nsnull;
  nsresult rv = GetPrimaryPresState(this, &presState);
  if (presState) {
    presState->SetStateProperty(state);

    if (mDisabledChanged) {
      // We do not want to save the real disabled state but the disabled
      // attribute.
      presState->SetDisabled(HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
    }
  }

  return rv;
}

bool
nsHTMLSelectElement::RestoreState(nsPresState* aState)
{
  // Get the presentation state object to retrieve our stuff out of.
  nsCOMPtr<nsSelectState> state(
    do_QueryInterface(aState->GetStateProperty()));

  if (state) {
    RestoreStateTo(state);

    // Don't flush, if the frame doesn't exist yet it doesn't care if
    // we're reset or not.
    DispatchContentReset();
  }

  if (aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
  }

  return false;
}

void
nsHTMLSelectElement::RestoreStateTo(nsSelectState* aNewSelected)
{
  if (!mIsDoneAddingChildren) {
    mRestoreState = aNewSelected;
    return;
  }

  PRUint32 len;
  GetLength(&len);

  // First clear all
  SetOptionsSelectedByIndex(-1, -1, true, true, true, true, nsnull);

  // Next set the proper ones
  for (PRInt32 i = 0; i < PRInt32(len); i++) {
    nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(i);
    if (option) {
      nsAutoString value;
      nsresult rv = option->GetValue(value);
      if (NS_SUCCEEDED(rv) && aNewSelected->ContainsOption(i, value)) {
        SetOptionsSelectedByIndex(i, i, true, false, true, true, nsnull);
      }
    }
  }
}

NS_IMETHODIMP
nsHTMLSelectElement::Reset()
{
  PRUint32 numSelected = 0;

  //
  // Cycle through the options array and reset the options
  //
  PRUint32 numOptions;
  nsresult rv = GetLength(&numOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < numOptions; i++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = Item(i, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));

    NS_ASSERTION(option, "option not an OptionElement");
    if (option) {
      //
      // Reset the option to its default value
      //
      bool selected = false;
      option->GetDefaultSelected(&selected);
      SetOptionsSelectedByIndex(i, i, selected,
                                false, true, true, nsnull);
      if (selected) {
        numSelected++;
      }
    }
  }

  //
  // If nothing was selected and it's not multiple, select something
  //
  if (numSelected == 0 && IsCombobox()) {
    SelectSomething(true);
  }

  SetSelectionChanged(false, true);

  //
  // Let the frame know we were reset
  //
  // Don't flush, if there's no frame yet it won't care about us being
  // reset even if we forced it to be created now.
  //
  DispatchContentReset();

  return NS_OK;
}

static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID);

NS_IMETHODIMP
nsHTMLSelectElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  // Disabled elements don't submit
  if (IsDisabled()) {
    return NS_OK;
  }

  //
  // Get the name (if no name, no submit)
  //
  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
  if (name.IsEmpty()) {
    return NS_OK;
  }

  //
  // Submit
  //
  PRUint32 len;
  GetLength(&len);

  nsAutoString mozType;
  nsCOMPtr<nsIFormProcessor> keyGenProcessor;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::_moz_type, mozType) &&
      mozType.EqualsLiteral("-mozilla-keygen")) {
    keyGenProcessor = do_GetService(kFormProcessorCID);
  }

  for (PRUint32 optIndex = 0; optIndex < len; optIndex++) {
    // Don't send disabled options
    bool disabled;
    nsresult rv = IsOptionDisabled(optIndex, &disabled);
    if (NS_FAILED(rv) || disabled) {
      continue;
    }

    nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
    NS_ENSURE_TRUE(option, NS_ERROR_UNEXPECTED);

    bool isSelected;
    rv = option->GetSelected(&isSelected);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!isSelected) {
      continue;
    }

    nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = do_QueryInterface(option);
    NS_ENSURE_TRUE(optionElement, NS_ERROR_UNEXPECTED);

    nsAutoString value;
    rv = optionElement->GetValue(value);
    NS_ENSURE_SUCCESS(rv, rv);

    if (keyGenProcessor) {
      nsAutoString tmp(value);
      rv = keyGenProcessor->ProcessValue(this, name, tmp);
      if (NS_SUCCEEDED(rv)) {
        value = tmp;
      }
    }

    rv = aFormSubmission->AddNameValuePair(name, value);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetHasOptGroups(bool* aHasGroups)
{
  *aHasGroups = (mOptGroupCount > 0);
  return NS_OK;
}

void
nsHTMLSelectElement::DispatchContentReset()
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  if (formControlFrame) {
    // Only dispatch content reset notification if this is a list control
    // frame or combo box control frame.
    if (IsCombobox()) {
      nsIComboboxControlFrame* comboFrame = do_QueryFrame(formControlFrame);
      if (comboFrame) {
        comboFrame->OnContentReset();
      }
    } else {
      nsIListControlFrame* listFrame = do_QueryFrame(formControlFrame);
      if (listFrame) {
        listFrame->OnContentReset();
      }
    }
  }
}

static void
AddOptionsRecurse(nsIContent* aRoot, nsHTMLOptionCollection* aArray)
{
  for (nsIContent* cur = aRoot->GetFirstChild();
       cur;
       cur = cur->GetNextSibling()) {
    nsHTMLOptionElement* opt = nsHTMLOptionElement::FromContent(cur);
    if (opt) {
      // If we fail here, then at least we've tried our best
      aArray->AppendOption(opt);
    } else if (cur->IsHTML(nsGkAtoms::optgroup)) {
      AddOptionsRecurse(cur, aArray);
    }
  }
}

void
nsHTMLSelectElement::RebuildOptionsArray(bool aNotify)
{
  mOptions->Clear();
  AddOptionsRecurse(this, mOptions);
  FindSelectedIndex(0, aNotify);
}

bool
nsHTMLSelectElement::IsValueMissing()
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    return false;
  }

  PRUint32 length;
  mOptions->GetLength(&length);

  for (PRUint32 i = 0; i < length; ++i) {
    nsIDOMHTMLOptionElement* option = mOptions->ItemAsOption(i);
    bool selected;
    NS_ENSURE_SUCCESS(option->GetSelected(&selected), false);

    if (!selected) {
      continue;
    }

    bool disabled;
    IsOptionDisabled(i, &disabled);
    if (disabled) {
      continue;
    }

    nsAutoString value;
    NS_ENSURE_SUCCESS(option->GetValue(value), false);
    if (!value.IsEmpty()) {
      return false;
    }
  }

  return true;
}

void
nsHTMLSelectElement::UpdateValueMissingValidityState()
{
  SetValidityState(VALIDITY_STATE_VALUE_MISSING, IsValueMissing());
}

nsresult
nsHTMLSelectElement::GetValidationMessage(nsAString& aValidationMessage,
                                          ValidityStateType aType)
{
  switch (aType) {
    case VALIDITY_STATE_VALUE_MISSING: {
      nsXPIDLString message;
      nsresult rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                       "FormValidationSelectMissing",
                                                       message);
      aValidationMessage = message;
      return rv;
    }
    default: {
      return nsIConstraintValidation::GetValidationMessage(aValidationMessage, aType);
    }
  }
}

#ifdef DEBUG

static void
VerifyOptionsRecurse(nsIContent* aRoot, PRInt32& aIndex,
                     nsHTMLOptionCollection* aArray)
{
  for (nsIContent* cur = aRoot->GetFirstChild();
       cur;
       cur = cur->GetNextSibling()) {
    nsCOMPtr<nsIDOMHTMLOptionElement> opt = do_QueryInterface(cur);
    if (opt) {
      NS_ASSERTION(opt == aArray->ItemAsOption(aIndex++),
                   "Options collection broken");
    } else if (cur->IsHTML(nsGkAtoms::optgroup)) {
      VerifyOptionsRecurse(cur, aIndex, aArray);
    }
  }
}

void
nsHTMLSelectElement::VerifyOptionsArray()
{
  PRInt32 aIndex = 0;
  VerifyOptionsRecurse(this, aIndex, mOptions);
}

#endif

//----------------------------------------------------------------------
//
// nsHTMLOptionCollection implementation
//

nsHTMLOptionCollection::nsHTMLOptionCollection(nsHTMLSelectElement* aSelect)
{
  SetIsProxy();

  // Do not maintain a reference counted reference. When
  // the select goes away, it will let us know.
  mSelect = aSelect;
}

nsHTMLOptionCollection::~nsHTMLOptionCollection()
{
  DropReference();
}

void
nsHTMLOptionCollection::DropReference()
{
  // Drop our (non ref-counted) reference
  mSelect = nsnull;
}

nsresult
nsHTMLOptionCollection::GetOptionIndex(mozilla::dom::Element* aOption,
                                       PRInt32 aStartIndex,
                                       bool aForward,
                                       PRInt32* aIndex)
{
  // NOTE: aIndex shouldn't be set if the returned value isn't NS_OK.

  PRInt32 index;

  // Make the common case fast
  if (aStartIndex == 0 && aForward) {
    index = mElements.IndexOf(aOption);
    if (index == -1) {
      return NS_ERROR_FAILURE;
    }
    
    *aIndex = index;
    return NS_OK;
  }

  PRInt32 high = mElements.Length();
  PRInt32 step = aForward ? 1 : -1;

  for (index = aStartIndex; index < high && index > -1; index += step) {
    if (mElements[index] == aOption) {
      *aIndex = index;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLOptionCollection)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsHTMLOptionCollection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsHTMLOptionCollection)
  {
    PRUint32 i;
    for (i = 0; i < tmp->mElements.Length(); ++i) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mElements[i]");
      cb.NoteXPCOMChild(static_cast<Element*>(tmp->mElements[i]));
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsHTMLOptionCollection)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// nsISupports

DOMCI_DATA(HTMLOptionsCollection, nsHTMLOptionCollection)

// QueryInterface implementation for nsHTMLOptionCollection
NS_INTERFACE_TABLE_HEAD(nsHTMLOptionCollection)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE3(nsHTMLOptionCollection,
                      nsIHTMLCollection,
                      nsIDOMHTMLOptionsCollection,
                      nsIDOMHTMLCollection)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsHTMLOptionCollection)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(HTMLOptionsCollection)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHTMLOptionCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHTMLOptionCollection)


JSObject*
nsHTMLOptionCollection::WrapObject(JSContext *cx, JSObject *scope,
                                   bool *triedToWrap)
{
  return mozilla::dom::binding::HTMLOptionsCollection::create(cx, scope, this,
                                                              triedToWrap);
}

NS_IMETHODIMP
nsHTMLOptionCollection::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetLength(PRUint32 aLength)
{
  if (!mSelect) {
    return NS_ERROR_UNEXPECTED;
  }

  return mSelect->SetLength(aLength);
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetOption(PRUint32 aIndex,
                                  nsIDOMHTMLOptionElement *aOption)
{
  if (!mSelect) {
    return NS_OK;
  }

  // if the new option is null, just remove this option.  Note that it's safe
  // to pass a too-large aIndex in here.
  if (!aOption) {
    mSelect->Remove(aIndex);

    // We're done.
    return NS_OK;
  }

  nsresult rv = NS_OK;

  PRUint32 index = PRUint32(aIndex);

  // Now we're going to be setting an option in our collection
  if (index > mElements.Length()) {
    // Fill our array with blank options up to (but not including, since we're
    // about to change it) aIndex, for compat with other browsers.
    rv = SetLength(index);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(index <= mElements.Length(), "SetLength lied");
  
  nsCOMPtr<nsIDOMNode> ret;
  if (index == mElements.Length()) {
    rv = mSelect->AppendChild(aOption, getter_AddRefs(ret));
  } else {
    // Find the option they're talking about and replace it
    // hold a strong reference to follow COM rules.
    nsCOMPtr<nsIDOMHTMLOptionElement> refChild = ItemAsOption(index);
    NS_ENSURE_TRUE(refChild, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIDOMNode> parent;
    refChild->GetParentNode(getter_AddRefs(parent));
    if (parent) {
      rv = parent->ReplaceChild(aOption, refChild, getter_AddRefs(ret));
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLOptionCollection::GetSelectedIndex(PRInt32 *aSelectedIndex)
{
  NS_ENSURE_TRUE(mSelect, NS_ERROR_UNEXPECTED);

  return mSelect->GetSelectedIndex(aSelectedIndex);
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetSelectedIndex(PRInt32 aSelectedIndex)
{
  NS_ENSURE_TRUE(mSelect, NS_ERROR_UNEXPECTED);

  return mSelect->SetSelectedIndex(aSelectedIndex);
}

NS_IMETHODIMP
nsHTMLOptionCollection::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsISupports* item = GetNodeAt(aIndex);
  if (!item) {
    *aReturn = nsnull;

    return NS_OK;
  }

  return CallQueryInterface(item, aReturn);
}

nsIContent*
nsHTMLOptionCollection::GetNodeAt(PRUint32 aIndex)
{
  return static_cast<nsIContent*>(ItemAsOption(aIndex));
}

static nsHTMLOptionElement*
GetNamedItemHelper(nsTArray<nsRefPtr<nsHTMLOptionElement> > &aElements,
                   const nsAString& aName)
{
  PRUint32 count = aElements.Length();
  for (PRUint32 i = 0; i < count; i++) {
    nsHTMLOptionElement *content = aElements.ElementAt(i);
    if (content &&
        (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, aName,
                              eCaseMatters) ||
         content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::id, aName,
                              eCaseMatters))) {
      return content;
    }
  }

  return nsnull;
}

nsISupports*
nsHTMLOptionCollection::GetNamedItem(const nsAString& aName,
                                     nsWrapperCache **aCache)
{
  nsINode *item = GetNamedItemHelper(mElements, aName);
  *aCache = item;
  return item;
}

nsINode*
nsHTMLOptionCollection::GetParentObject()
{
    return mSelect;
}

NS_IMETHODIMP
nsHTMLOptionCollection::NamedItem(const nsAString& aName,
                                  nsIDOMNode** aReturn)
{
  NS_IF_ADDREF(*aReturn = GetNamedItemHelper(mElements, aName));

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionCollection::GetSelect(nsIDOMHTMLSelectElement **aReturn)
{
  NS_IF_ADDREF(*aReturn = mSelect);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionCollection::Add(nsIDOMHTMLOptionElement *aOption,
                            nsIVariant *aBefore)
{
  if (!aOption) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!mSelect) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  return mSelect->Add(aOption, aBefore);
}

NS_IMETHODIMP
nsHTMLOptionCollection::Remove(PRInt32 aIndex)
{
  NS_ENSURE_TRUE(mSelect, NS_ERROR_UNEXPECTED);

  PRUint32 len = 0;
  mSelect->GetLength(&len);
  if (aIndex < 0 || (PRUint32)aIndex >= len)
    aIndex = 0;

  return mSelect->Remove(aIndex);
}

void
nsHTMLSelectElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(IsDisabled());
}

void
nsHTMLSelectElement::FieldSetDisabledChanged(bool aNotify)
{
  UpdateBarredFromConstraintValidation();

  nsGenericHTMLFormElement::FieldSetDisabledChanged(aNotify);
}

void
nsHTMLSelectElement::SetSelectionChanged(bool aValue, bool aNotify)
{
  if (!mDefaultSelectionSet) {
    return;
  }

  bool previousSelectionChangedValue = mSelectionHasChanged;
  mSelectionHasChanged = aValue;

  if (mSelectionHasChanged != previousSelectionChangedValue) {
    UpdateState(aNotify);
  }
}
