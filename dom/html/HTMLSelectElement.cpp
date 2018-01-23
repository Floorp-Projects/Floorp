/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLSelectElement.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/HTMLOptGroupElement.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLSelectElementBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentList.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDocument.h"
#include "nsIFormControlFrame.h"
#include "nsIForm.h"
#include "nsIFormProcessor.h"
#include "nsIFrame.h"
#include "nsIListControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsLayoutUtils.h"
#include "nsMappedAttributes.h"
#include "nsPresState.h"
#include "nsServiceManagerUtils.h"
#include "nsStyleConsts.h"
#include "nsTextNode.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Select)

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(SelectState, SelectState)

//----------------------------------------------------------------------
//
// SafeOptionListMutation
//

SafeOptionListMutation::SafeOptionListMutation(nsIContent* aSelect,
                                               nsIContent* aParent,
                                               nsIContent* aKid,
                                               uint32_t aIndex,
                                               bool aNotify)
  : mSelect(HTMLSelectElement::FromContentOrNull(aSelect))
  , mTopLevelMutation(false)
  , mNeedsRebuild(false)
  , mNotify(aNotify)
  , mInitialSelectedIndex(-1)
{
  if (mSelect) {
    mInitialSelectedIndex = mSelect->SelectedIndex();
    mTopLevelMutation = !mSelect->mMutating;
    if (mTopLevelMutation) {
      mSelect->mMutating = true;
    } else {
      // This is very unfortunate, but to handle mutation events properly,
      // option list must be up-to-date before inserting or removing options.
      // Fortunately this is called only if mutation event listener
      // adds or removes options.
      mSelect->RebuildOptionsArray(mNotify);
    }
    nsresult rv;
    if (aKid) {
      rv = mSelect->WillAddOptions(aKid, aParent, aIndex, mNotify);
    } else {
      rv = mSelect->WillRemoveOptions(aParent, aIndex, mNotify);
    }
    mNeedsRebuild = NS_FAILED(rv);
  }
}

SafeOptionListMutation::~SafeOptionListMutation()
{
  if (mSelect) {
    if (mNeedsRebuild || (mTopLevelMutation && mGuard.Mutated(1))) {
      mSelect->RebuildOptionsArray(true);
    }
    if (mTopLevelMutation) {
      mSelect->mMutating = false;
    }
    if (mSelect->SelectedIndex() != mInitialSelectedIndex) {
      // We must have triggered the SelectSomething() codepath, which can cause
      // our validity to change.  Unfortunately, our attempt to update validity
      // in that case may not have worked correctly, because we actually call it
      // before we have inserted the new <option>s into the DOM!  Go ahead and
      // update validity here as needed, because by now we know our <option>s
      // are where they should be.
      mSelect->UpdateValueMissingValidityState();
      mSelect->UpdateState(mNotify);
    }
#ifdef DEBUG
    mSelect->VerifyOptionsArray();
#endif
  }
}

//----------------------------------------------------------------------
//
// HTMLSelectElement
//

// construction, destruction


HTMLSelectElement::HTMLSelectElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFormElementWithState(aNodeInfo, NS_FORM_SELECT),
    mOptions(new HTMLOptionsCollection(this)),
    mAutocompleteAttrState(nsContentUtils::eAutocompleteAttrState_Unknown),
    mAutocompleteInfoState(nsContentUtils::eAutocompleteAttrState_Unknown),
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
  SetHasWeirdParserInsertionMode();

  // DoneAddingChildren() will be called later if it's from the parser,
  // otherwise it is

  // Set up our default state: enabled, optional, and valid.
  AddStatesSilently(NS_EVENT_STATE_ENABLED |
                    NS_EVENT_STATE_OPTIONAL |
                    NS_EVENT_STATE_VALID);
}

HTMLSelectElement::~HTMLSelectElement()
{
  mOptions->DropReference();
}

// ISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLSelectElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLSelectElement,
                                                  nsGenericHTMLFormElementWithState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOptions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelectedOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLSelectElement,
                                                nsGenericHTMLFormElementWithState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelectedOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLSelectElement,
                                             nsGenericHTMLFormElementWithState,
                                             nsIConstraintValidation)


// nsIDOMHTMLSelectElement


NS_IMPL_ELEMENT_CLONE(HTMLSelectElement)

void
HTMLSelectElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);
}

void
HTMLSelectElement::GetAutocomplete(DOMString& aValue)
{
  const nsAttrValue* attributeVal = GetParsedAttr(nsGkAtoms::autocomplete);

  mAutocompleteAttrState =
    nsContentUtils::SerializeAutocompleteAttribute(attributeVal, aValue,
                                                   mAutocompleteAttrState);
}

void
HTMLSelectElement::GetAutocompleteInfo(AutocompleteInfo& aInfo)
{
  const nsAttrValue* attributeVal = GetParsedAttr(nsGkAtoms::autocomplete);
  mAutocompleteInfoState =
    nsContentUtils::SerializeAutocompleteAttribute(attributeVal, aInfo,
                                                   mAutocompleteInfoState,
                                                   true);
}

nsresult
HTMLSelectElement::InsertChildAt(nsIContent* aKid,
                                 uint32_t aIndex,
                                 bool aNotify)
{
  SafeOptionListMutation safeMutation(this, this, aKid, aIndex, aNotify);
  nsresult rv = nsGenericHTMLFormElementWithState::InsertChildAt(aKid, aIndex,
                                                                 aNotify);
  if (NS_FAILED(rv)) {
    safeMutation.MutationFailed();
  }
  return rv;
}

void
HTMLSelectElement::RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify)
{
  SafeOptionListMutation safeMutation(this, this, nullptr, aIndex, aNotify);
  nsGenericHTMLFormElementWithState::RemoveChildAt_Deprecated(aIndex, aNotify);
}

void
HTMLSelectElement::RemoveChildNode(nsIContent* aKid, bool aNotify)
{
  SafeOptionListMutation safeMutation(this, this, nullptr,
                                      ComputeIndexOf(aKid), aNotify);
  nsGenericHTMLFormElementWithState::RemoveChildNode(aKid, aNotify);
}

void
HTMLSelectElement::InsertOptionsIntoList(nsIContent* aOptions,
                                         int32_t aListIndex,
                                         int32_t aDepth,
                                         bool aNotify)
{
  MOZ_ASSERT(aDepth == 0 || aDepth == 1);
  int32_t insertIndex = aListIndex;

  HTMLOptionElement* optElement = HTMLOptionElement::FromContent(aOptions);
  if (optElement) {
    mOptions->InsertOptionAt(optElement, insertIndex);
    insertIndex++;
  } else if (aDepth == 0) {
    // If it's at the top level, then we just found out there are non-options
    // at the top level, which will throw off the insert count
    mNonOptionChildren++;

    // Deal with optgroups
    if (aOptions->IsHTMLElement(nsGkAtoms::optgroup)) {
      mOptGroupCount++;

      for (nsIContent* child = aOptions->GetFirstChild();
           child;
           child = child->GetNextSibling()) {
        optElement = HTMLOptionElement::FromContent(child);
        if (optElement) {
          mOptions->InsertOptionAt(optElement, insertIndex);
          insertIndex++;
        }
      }
    }
  } // else ignore even if optgroup; we want to ignore nested optgroups.

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
    nsISelectControlFrame* selectFrame = nullptr;
    AutoWeakFrame weakSelectFrame;
    bool didGetFrame = false;

    // Actually select the options if the added options warrant it
    for (int32_t i = aListIndex; i < insertIndex; i++) {
      // Notify the frame that the option is added
      if (!didGetFrame || (selectFrame && !weakSelectFrame.IsAlive())) {
        selectFrame = GetSelectFrame();
        weakSelectFrame = do_QueryFrame(selectFrame);
        didGetFrame = true;
      }

      if (selectFrame) {
        selectFrame->AddOption(i);
      }

      RefPtr<HTMLOptionElement> option = Item(i);
      if (option && option->Selected()) {
        // Clear all other options
        if (!HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
          uint32_t mask = IS_SELECTED | CLEAR_ALL | SET_DISABLED | NOTIFY;
          SetOptionsSelectedByIndex(i, i, mask);
        }

        // This is sort of a hack ... we need to notify that the option was
        // set and change selectedIndex even though we didn't really change
        // its value.
        OnOptionSelected(selectFrame, i, true, false, false);
      }
    }

    CheckSelectSomething(aNotify);
  }
}

nsresult
HTMLSelectElement::RemoveOptionsFromList(nsIContent* aOptions,
                                         int32_t aListIndex,
                                         int32_t aDepth,
                                         bool aNotify)
{
  MOZ_ASSERT(aDepth == 0 || aDepth == 1);
  int32_t numRemoved = 0;

  HTMLOptionElement* optElement = HTMLOptionElement::FromContent(aOptions);
  if (optElement) {
    if (mOptions->ItemAsOption(aListIndex) != optElement) {
      NS_ERROR("wrong option at index");
      return NS_ERROR_UNEXPECTED;
    }
    mOptions->RemoveOptionAt(aListIndex);
    numRemoved++;
  } else if (aDepth == 0) {
    // Yay, one less artifact at the top level.
    mNonOptionChildren--;

    // Recurse down deeper for options
    if (mOptGroupCount && aOptions->IsHTMLElement(nsGkAtoms::optgroup)) {
      mOptGroupCount--;

      for (nsIContent* child = aOptions->GetFirstChild();
          child;
          child = child->GetNextSibling()) {
        optElement = HTMLOptionElement::FromContent(child);
        if (optElement) {
          if (mOptions->ItemAsOption(aListIndex) != optElement) {
            NS_ERROR("wrong option at index");
            return NS_ERROR_UNEXPECTED;
          }
          mOptions->RemoveOptionAt(aListIndex);
          numRemoved++;
        }
      }
    }
  } // else don't check for an optgroup; we want to ignore nested optgroups

  if (numRemoved) {
    // Tell the widget we removed the options
    nsISelectControlFrame* selectFrame = GetSelectFrame();
    if (selectFrame) {
      nsAutoScriptBlocker scriptBlocker;
      for (int32_t i = aListIndex; i < aListIndex + numRemoved; ++i) {
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

// XXXldb Doing the processing before the content nodes have been added
// to the document (as the name of this function seems to require, and
// as the callers do), is highly unusual.  Passing around unparented
// content to other parts of the app can make those things think the
// options are the root content node.
NS_IMETHODIMP
HTMLSelectElement::WillAddOptions(nsIContent* aOptions,
                                  nsIContent* aParent,
                                  int32_t aContentIndex,
                                  bool aNotify)
{
  if (this != aParent && this != aParent->GetParent()) {
    return NS_OK;
  }
  int32_t level = aParent == this ? 0 : 1;

  // Get the index where the options will be inserted
  int32_t ind = -1;
  if (!mNonOptionChildren) {
    // If there are no artifacts, aContentIndex == ind
    ind = aContentIndex;
  } else {
    // If there are artifacts, we have to get the index of the option the
    // hard way
    int32_t children = aParent->GetChildCount();

    if (aContentIndex >= children) {
      // If the content insert is after the end of the parent, then we want to get
      // the next index *after* the parent and insert there.
      ind = GetOptionIndexAfter(aParent);
    } else {
      // If the content insert is somewhere in the middle of the container, then
      // we want to get the option currently at the index and insert in front of
      // that.
      nsIContent* currentKid = aParent->GetChildAt_Deprecated(aContentIndex);
      NS_ASSERTION(currentKid, "Child not found!");
      if (currentKid) {
        ind = GetOptionIndexAt(currentKid);
      } else {
        ind = -1;
      }
    }
  }

  InsertOptionsIntoList(aOptions, ind, level, aNotify);
  return NS_OK;
}

NS_IMETHODIMP
HTMLSelectElement::WillRemoveOptions(nsIContent* aParent,
                                     int32_t aContentIndex,
                                     bool aNotify)
{
  if (this != aParent && this != aParent->GetParent()) {
    return NS_OK;
  }
  int32_t level = this == aParent ? 0 : 1;

  // Get the index where the options will be removed
  nsIContent* currentKid = aParent->GetChildAt_Deprecated(aContentIndex);
  if (currentKid) {
    int32_t ind;
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

int32_t
HTMLSelectElement::GetOptionIndexAt(nsIContent* aOptions)
{
  // Search this node and below.
  // If not found, find the first one *after* this node.
  int32_t retval = GetFirstOptionIndex(aOptions);
  if (retval == -1) {
    retval = GetOptionIndexAfter(aOptions);
  }

  return retval;
}

int32_t
HTMLSelectElement::GetOptionIndexAfter(nsIContent* aOptions)
{
  // - If this is the select, the next option is the last.
  // - If not, search all the options after aOptions and up to the last option
  //   in the parent.
  // - If it's not there, search for the first option after the parent.
  if (aOptions == this) {
    return Length();
  }

  int32_t retval = -1;

  nsCOMPtr<nsIContent> parent = aOptions->GetParent();

  if (parent) {
    int32_t index = parent->ComputeIndexOf(aOptions);
    int32_t count = parent->GetChildCount();

    retval = GetFirstChildOptionIndex(parent, index+1, count);

    if (retval == -1) {
      retval = GetOptionIndexAfter(parent);
    }
  }

  return retval;
}

int32_t
HTMLSelectElement::GetFirstOptionIndex(nsIContent* aOptions)
{
  int32_t listIndex = -1;
  HTMLOptionElement* optElement = HTMLOptionElement::FromContent(aOptions);
  if (optElement) {
    mOptions->GetOptionIndex(optElement->AsElement(), 0, true, &listIndex);
    return listIndex;
  }

  listIndex = GetFirstChildOptionIndex(aOptions, 0, aOptions->GetChildCount());

  return listIndex;
}

int32_t
HTMLSelectElement::GetFirstChildOptionIndex(nsIContent* aOptions,
                                            int32_t aStartIndex,
                                            int32_t aEndIndex)
{
  int32_t retval = -1;

  for (int32_t i = aStartIndex; i < aEndIndex; ++i) {
    retval = GetFirstOptionIndex(aOptions->GetChildAt_Deprecated(i));
    if (retval != -1) {
      break;
    }
  }

  return retval;
}

nsISelectControlFrame*
HTMLSelectElement::GetSelectFrame()
{
  nsIFormControlFrame* form_control_frame = GetFormControlFrame(false);

  nsISelectControlFrame* select_frame = nullptr;

  if (form_control_frame) {
    select_frame = do_QueryFrame(form_control_frame);
  }

  return select_frame;
}

void
HTMLSelectElement::Add(const HTMLOptionElementOrHTMLOptGroupElement& aElement,
                       const Nullable<HTMLElementOrLong>& aBefore,
                       ErrorResult& aRv)
{
  nsGenericHTMLElement& element =
    aElement.IsHTMLOptionElement() ?
    static_cast<nsGenericHTMLElement&>(aElement.GetAsHTMLOptionElement()) :
    static_cast<nsGenericHTMLElement&>(aElement.GetAsHTMLOptGroupElement());

  if (aBefore.IsNull()) {
    Add(element, static_cast<nsGenericHTMLElement*>(nullptr), aRv);
  } else if (aBefore.Value().IsHTMLElement()) {
    Add(element, &aBefore.Value().GetAsHTMLElement(), aRv);
  } else {
    Add(element, aBefore.Value().GetAsLong(), aRv);
  }
}

void
HTMLSelectElement::Add(nsGenericHTMLElement& aElement,
                       nsGenericHTMLElement* aBefore,
                       ErrorResult& aError)
{
  if (!aBefore) {
    Element::AppendChild(aElement, aError);
    return;
  }

  // Just in case we're not the parent, get the parent of the reference
  // element
  nsCOMPtr<nsINode> parent = aBefore->Element::GetParentNode();
  if (!parent || !nsContentUtils::ContentIsDescendantOf(parent, this)) {
    // NOT_FOUND_ERR: Raised if before is not a descendant of the SELECT
    // element.
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  // If the before parameter is not null, we are equivalent to the
  // insertBefore method on the parent of before.
  nsCOMPtr<nsINode> refNode = aBefore;
  parent->InsertBefore(aElement, refNode, aError);
}

void
HTMLSelectElement::Remove(int32_t aIndex)
{
  nsCOMPtr<nsINode> option = Item(static_cast<uint32_t>(aIndex));
  if (!option) {
    return;
  }

  option->Remove();
}

void
HTMLSelectElement::GetType(nsAString& aType)
{
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
    aType.AssignLiteral("select-multiple");
  }
  else {
    aType.AssignLiteral("select-one");
  }
}

#define MAX_DYNAMIC_SELECT_LENGTH 10000

void
HTMLSelectElement::SetLength(uint32_t aLength, ErrorResult& aRv)
{
  uint32_t curlen = Length();

  if (curlen > aLength) { // Remove extra options
    for (uint32_t i = curlen; i > aLength; --i) {
      Remove(i - 1);
    }
  } else if (aLength > curlen) {
    if (aLength > MAX_DYNAMIC_SELECT_LENGTH) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }

    RefPtr<mozilla::dom::NodeInfo> nodeInfo;

    nsContentUtils::QNameChanged(mNodeInfo, nsGkAtoms::option,
                                 getter_AddRefs(nodeInfo));

    nsCOMPtr<nsINode> node = NS_NewHTMLOptionElement(nodeInfo.forget());

    RefPtr<nsTextNode> text = new nsTextNode(mNodeInfo->NodeInfoManager());

    aRv = node->AppendChildTo(text, false);
    if (aRv.Failed()) {
      return;
    }

    for (uint32_t i = curlen; i < aLength; i++) {
      nsINode::AppendChild(*node, aRv);
      if (aRv.Failed()) {
        return;
      }

      if (i + 1 < aLength) {
        node = node->CloneNode(true, aRv);
        if (aRv.Failed()) {
          return;
        }
        MOZ_ASSERT(node);
      }
    }
  }
}

/* static */
bool
HTMLSelectElement::MatchSelectedOptions(Element* aElement,
                                        int32_t /* unused */,
                                        nsAtom* /* unused */,
                                        void* /* unused*/)
{
  HTMLOptionElement* option = HTMLOptionElement::FromContent(aElement);
  return option && option->Selected();
}

nsIHTMLCollection*
HTMLSelectElement::SelectedOptions()
{
  if (!mSelectedOptions) {
    mSelectedOptions = new nsContentList(this, MatchSelectedOptions, nullptr,
                                         nullptr, /* deep */ true);
  }
  return mSelectedOptions;
}

nsresult
HTMLSelectElement::SetSelectedIndexInternal(int32_t aIndex, bool aNotify)
{
  int32_t oldSelectedIndex = mSelectedIndex;
  uint32_t mask = IS_SELECTED | CLEAR_ALL | SET_DISABLED;
  if (aNotify) {
    mask |= NOTIFY;
  }

  SetOptionsSelectedByIndex(aIndex, aIndex, mask);

  nsresult rv = NS_OK;
  nsISelectControlFrame* selectFrame = GetSelectFrame();
  if (selectFrame) {
    rv = selectFrame->OnSetSelectedIndex(oldSelectedIndex, mSelectedIndex);
  }

  SetSelectionChanged(true, aNotify);

  return rv;
}

bool
HTMLSelectElement::IsOptionSelectedByIndex(int32_t aIndex)
{
  HTMLOptionElement* option = Item(static_cast<uint32_t>(aIndex));
  return option && option->Selected();
}

void
HTMLSelectElement::OnOptionSelected(nsISelectControlFrame* aSelectFrame,
                                    int32_t aIndex,
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
    RefPtr<HTMLOptionElement> option = Item(static_cast<uint32_t>(aIndex));
    if (option) {
      option->SetSelectedInternal(aSelected, aNotify);
    }
  }

  // Let the frame know too
  if (aSelectFrame) {
    aSelectFrame->OnOptionSelected(aIndex, aSelected);
  }

  UpdateSelectedOptions();
  UpdateValueMissingValidityState();
  UpdateState(aNotify);
}

void
HTMLSelectElement::FindSelectedIndex(int32_t aStartIndex, bool aNotify)
{
  mSelectedIndex = -1;
  SetSelectionChanged(true, aNotify);
  uint32_t len = Length();
  for (int32_t i = aStartIndex; i < int32_t(len); i++) {
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
bool
HTMLSelectElement::SetOptionsSelectedByIndex(int32_t aStartIndex,
                                             int32_t aEndIndex,
                                             uint32_t aOptionsMask)
{
#if 0
  printf("SetOption(%d-%d, %c, ClearAll=%c)\n", aStartIndex, aEndIndex,
                                      (aOptionsMask & IS_SELECTED ? 'Y' : 'N'),
                                      (aOptionsMask & CLEAR_ALL ? 'Y' : 'N'));
#endif
  // Don't bother if the select is disabled
  if (!(aOptionsMask & SET_DISABLED) && IsDisabled()) {
    return false;
  }

  // Don't bother if there are no options
  uint32_t numItems = Length();
  if (numItems == 0) {
    return false;
  }

  // First, find out whether multiple items can be selected
  bool isMultiple = Multiple();

  // These variables tell us whether any options were selected
  // or deselected.
  bool optionsSelected = false;
  bool optionsDeselected = false;

  nsISelectControlFrame* selectFrame = nullptr;
  bool didGetFrame = false;
  AutoWeakFrame weakSelectFrame;

  if (aOptionsMask & IS_SELECTED) {
    // Setting selectedIndex to an out-of-bounds index means -1. (HTML5)
    if (aStartIndex < 0 || AssertedCast<uint32_t>(aStartIndex) >= numItems ||
        aEndIndex < 0 || AssertedCast<uint32_t>(aEndIndex) >= numItems) {
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
    bool allDisabled = !(aOptionsMask & SET_DISABLED);

    //
    // Save a little time when clearing other options
    //
    int32_t previousSelectedIndex = mSelectedIndex;

    //
    // Select the requested indices
    //
    // If index is -1, everything will be deselected (bug 28143)
    if (aStartIndex != -1) {
      MOZ_ASSERT(aStartIndex >= 0);
      MOZ_ASSERT(aEndIndex >= 0);
      // Loop through the options and select them (if they are not disabled and
      // if they are not already selected).
      for (uint32_t optIndex = AssertedCast<uint32_t>(aStartIndex);
           optIndex <= AssertedCast<uint32_t>(aEndIndex);
           optIndex++) {
        RefPtr<HTMLOptionElement> option = Item(optIndex);

        // Ignore disabled options.
        if (!(aOptionsMask & SET_DISABLED)) {
          if (option && IsOptionDisabled(option)) {
            continue;
          }
          allDisabled = false;
        }

        // If the index is already selected, ignore it.
        if (option && !option->Selected()) {
          // To notify the frame if anything gets changed. No need
          // to flush here, if there's no frame yet we don't need to
          // force it to be created just to notify it about a change
          // in the select.
          selectFrame = GetSelectFrame();
          weakSelectFrame = do_QueryFrame(selectFrame);
          didGetFrame = true;

          OnOptionSelected(selectFrame, optIndex, true, true,
                           aOptionsMask & NOTIFY);
          optionsSelected = true;
        }
      }
    }

    // Next remove all other options if single select or all is clear
    // If index is -1, everything will be deselected (bug 28143)
    if (((!isMultiple && optionsSelected)
       || ((aOptionsMask & CLEAR_ALL) && !allDisabled)
       || aStartIndex == -1)
       && previousSelectedIndex != -1) {
      for (uint32_t optIndex = AssertedCast<uint32_t>(previousSelectedIndex);
           optIndex < numItems;
           optIndex++) {
        if (static_cast<int32_t>(optIndex) < aStartIndex ||
            static_cast<int32_t>(optIndex) > aEndIndex) {
          HTMLOptionElement* option = Item(optIndex);
          // If the index is already selected, ignore it.
          if (option && option->Selected()) {
            if (!didGetFrame || (selectFrame && !weakSelectFrame.IsAlive())) {
              // To notify the frame if anything gets changed, don't
              // flush, if the frame doesn't exist we don't need to
              // create it just to tell it about this change.
              selectFrame = GetSelectFrame();
              weakSelectFrame = do_QueryFrame(selectFrame);

              didGetFrame = true;
            }

            OnOptionSelected(selectFrame, optIndex, false, true,
                             aOptionsMask & NOTIFY);
            optionsDeselected = true;

            // Only need to deselect one option if not multiple
            if (!isMultiple) {
              break;
            }
          }
        }
      }
    }
  } else {
    // If we're deselecting, loop through all selected items and deselect
    // any that are in the specified range.
    for (int32_t optIndex = aStartIndex; optIndex <= aEndIndex; optIndex++) {
      HTMLOptionElement* option = Item(optIndex);
      if (!(aOptionsMask & SET_DISABLED) && IsOptionDisabled(option)) {
        continue;
      }

      // If the index is already selected, ignore it.
      if (option && option->Selected()) {
        if (!didGetFrame || (selectFrame && !weakSelectFrame.IsAlive())) {
          // To notify the frame if anything gets changed, don't
          // flush, if the frame doesn't exist we don't need to
          // create it just to tell it about this change.
          selectFrame = GetSelectFrame();
          weakSelectFrame = do_QueryFrame(selectFrame);

          didGetFrame = true;
        }

        OnOptionSelected(selectFrame, optIndex, false, true,
                         aOptionsMask & NOTIFY);
        optionsDeselected = true;
      }
    }
  }

  // Make sure something is selected unless we were set to -1 (none)
  if (optionsDeselected && aStartIndex != -1 && !(aOptionsMask & NO_RESELECT)) {
    optionsSelected =
      CheckSelectSomething(aOptionsMask & NOTIFY) || optionsSelected;
  }

  // Let the caller know whether anything was changed
  return optionsSelected || optionsDeselected;
}

NS_IMETHODIMP
HTMLSelectElement::IsOptionDisabled(int32_t aIndex, bool* aIsDisabled)
{
  *aIsDisabled = false;
  RefPtr<HTMLOptionElement> option = Item(aIndex);
  NS_ENSURE_TRUE(option, NS_ERROR_FAILURE);

  *aIsDisabled = IsOptionDisabled(option);
  return NS_OK;
}

bool
HTMLSelectElement::IsOptionDisabled(HTMLOptionElement* aOption) const
{
  MOZ_ASSERT(aOption);
  if (aOption->Disabled()) {
    return true;
  }

  // Check for disabled optgroups
  // If there are no artifacts, there are no optgroups
  if (mNonOptionChildren) {
    for (nsCOMPtr<Element> node = static_cast<nsINode*>(aOption)->GetParentElement();
         node;
         node = node->GetParentElement()) {
      // If we reached the select element, we're done
      if (node->IsHTMLElement(nsGkAtoms::select)) {
        return false;
      }

      RefPtr<HTMLOptGroupElement> optGroupElement =
        HTMLOptGroupElement::FromContent(node);

      if (!optGroupElement) {
        // If you put something else between you and the optgroup, you're a
        // moron and you deserve not to have optgroup disabling work.
        return false;
      }

      if (optGroupElement->Disabled()) {
        return true;
      }
    }
  }

  return false;
}

void
HTMLSelectElement::GetValue(DOMString& aValue)
{
  int32_t selectedIndex = SelectedIndex();
  if (selectedIndex < 0) {
    return;
  }

  RefPtr<HTMLOptionElement> option =
    Item(static_cast<uint32_t>(selectedIndex));

  if (!option) {
    return;
  }

  option->GetValue(aValue);
}

void
HTMLSelectElement::SetValue(const nsAString& aValue)
{
  uint32_t length = Length();

  for (uint32_t i = 0; i < length; i++) {
    RefPtr<HTMLOptionElement> option = Item(i);
    if (!option) {
      continue;
    }

    nsAutoString optionVal;
    option->GetValue(optionVal);
    if (optionVal.Equals(aValue)) {
      SetSelectedIndexInternal(int32_t(i), true);
      return;
    }
  }
  // No matching option was found.
  SetSelectedIndexInternal(-1, true);
}

int32_t
HTMLSelectElement::TabIndexDefault()
{
  return 0;
}

bool
HTMLSelectElement::IsHTMLFocusable(bool aWithMouse,
                                   bool* aIsFocusable, int32_t* aTabIndex)
{
  if (nsGenericHTMLFormElementWithState::IsHTMLFocusable(aWithMouse, aIsFocusable,
      aTabIndex))
  {
    return true;
  }

  *aIsFocusable = !IsDisabled();

  return false;
}

bool
HTMLSelectElement::CheckSelectSomething(bool aNotify)
{
  if (mIsDoneAddingChildren) {
    if (mSelectedIndex < 0 && IsCombobox()) {
      return SelectSomething(aNotify);
    }
  }
  return false;
}

bool
HTMLSelectElement::SelectSomething(bool aNotify)
{
  // If we're not done building the select, don't play with this yet.
  if (!mIsDoneAddingChildren) {
    return false;
  }

  uint32_t count = Length();
  for (uint32_t i = 0; i < count; i++) {
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
HTMLSelectElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElementWithState::BindToTree(aDocument, aParent,
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
HTMLSelectElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsGenericHTMLFormElementWithState::UnbindFromTree(aDeep, aNullParent);

  // We might be no longer disabled because our parent chain changed.
  // XXXbz is this still needed now that fieldset changes always call
  // FieldSetDisabledChanged?
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

nsresult
HTMLSelectElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::disabled) {
      if (aNotify) {
        mDisabledChanged = true;
      }
    } else if (aName == nsGkAtoms::multiple) {
      if (!aValue && aNotify && mSelectedIndex >= 0) {
        // We're changing from being a multi-select to a single-select.
        // Make sure we only have one option selected before we do that.
        // Note that this needs to come before we really unset the attr,
        // since SetOptionsSelectedByIndex does some bail-out type
        // optimization for cases when the select is not multiple that
        // would lead to only a single option getting deselected.
        SetSelectedIndexInternal(mSelectedIndex, aNotify);
      }
    }
  }

  return nsGenericHTMLFormElementWithState::BeforeSetAttr(aNameSpaceID, aName,
                                                          aValue, aNotify);
}

nsresult
HTMLSelectElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::disabled) {
      // This *has* to be called *before* validity state check because
      // UpdateBarredFromConstraintValidation and
      // UpdateValueMissingValidityState depend on our disabled state.
      UpdateDisabledState(aNotify);

      UpdateValueMissingValidityState();
      UpdateBarredFromConstraintValidation();
    } else if (aName == nsGkAtoms::required) {
      // This *has* to be called *before* UpdateValueMissingValidityState
      // because UpdateValueMissingValidityState depends on our required
      // state.
      UpdateRequiredState(!!aValue, aNotify);

      UpdateValueMissingValidityState();
    } else if (aName == nsGkAtoms::autocomplete) {
      // Clear the cached @autocomplete attribute and autocompleteInfo state.
      mAutocompleteAttrState = nsContentUtils::eAutocompleteAttrState_Unknown;
      mAutocompleteInfoState = nsContentUtils::eAutocompleteAttrState_Unknown;
    } else if (aName == nsGkAtoms::multiple) {
      if (!aValue && aNotify) {
        // We might have become a combobox; make sure _something_ gets
        // selected in that case
        CheckSelectSomething(aNotify);
      }
    }
  }

  return nsGenericHTMLFormElementWithState::AfterSetAttr(aNameSpaceID, aName,
                                                         aValue, aOldValue,
                                                         aSubjectPrincipal,
                                                         aNotify);
}

void
HTMLSelectElement::DoneAddingChildren(bool aHaveNotified)
{
  mIsDoneAddingChildren = true;

  nsISelectControlFrame* selectFrame = GetSelectFrame();

  // If we foolishly tried to restore before we were done adding
  // content, restore the rest of the options proper-like
  if (mRestoreState) {
    RestoreStateTo(mRestoreState);
    mRestoreState = nullptr;
  }

  // Notify the frame
  if (selectFrame) {
    selectFrame->DoneAddingChildren(true);
  }

  if (!mInhibitStateRestoration) {
    nsresult rv = GenerateStateKey();
    if (NS_SUCCEEDED(rv)) {
      RestoreFormControlState();
    }
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
HTMLSelectElement::ParseAttribute(int32_t aNamespaceID,
                                  nsAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsIPrincipal* aMaybeScriptedPrincipal,
                                  nsAttrValue& aResult)
{
  if (kNameSpaceID_None == aNamespaceID) {
    if (aAttribute == nsGkAtoms::size) {
      return aResult.ParsePositiveIntValue(aValue);
    } else if (aAttribute == nsGkAtoms::autocomplete) {
      aResult.ParseAtomArray(aValue);
      return true;
    }
  }
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void
HTMLSelectElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                         GenericSpecifiedValues* aData)
{
  nsGenericHTMLFormElementWithState::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElementWithState::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
HTMLSelectElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                          int32_t aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLFormElementWithState::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::multiple ||
      aAttribute == nsGkAtoms::size) {
    retval |= nsChangeHint_ReconstructFrame;
  }
  return retval;
}

NS_IMETHODIMP_(bool)
HTMLSelectElement::IsAttributeMapped(const nsAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageAlignAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
HTMLSelectElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

bool
HTMLSelectElement::IsDisabledForEvents(EventMessage aMessage)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIFrame* formFrame = nullptr;
  if (formControlFrame) {
    formFrame = do_QueryFrame(formControlFrame);
  }
  return IsElementDisabledForEvents(aMessage, formFrame);
}

nsresult
HTMLSelectElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent->mMessage)) {
    return NS_OK;
  }

  return nsGenericHTMLFormElementWithState::GetEventTargetParent(aVisitor);
}

nsresult
HTMLSelectElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  if (aVisitor.mEvent->mMessage == eFocus) {
    // If the invalid UI is shown, we should show it while focused and
    // update the invalid/valid UI.
    mCanShowInvalidUI = !IsValid() && ShouldShowValidityUI();

    // If neither invalid UI nor valid UI is shown, we shouldn't show the valid
    // UI while focused.
    mCanShowValidUI = ShouldShowValidityUI();

    // We don't have to update NS_EVENT_STATE_MOZ_UI_INVALID nor
    // NS_EVENT_STATE_MOZ_UI_VALID given that the states should not change.
  } else if (aVisitor.mEvent->mMessage == eBlur) {
    mCanShowInvalidUI = true;
    mCanShowValidUI = true;

    UpdateState(true);
  }

  return nsGenericHTMLFormElementWithState::PostHandleEvent(aVisitor);
}

EventStates
HTMLSelectElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLFormElementWithState::IntrinsicState();

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

  return state;
}

// nsIFormControl

NS_IMETHODIMP
HTMLSelectElement::SaveState()
{
  nsPresState* presState = GetPrimaryPresState();
  if (!presState) {
    return NS_OK;
  }

  RefPtr<SelectState> state = new SelectState();

  uint32_t len = Length();

  for (uint32_t optIndex = 0; optIndex < len; optIndex++) {
    HTMLOptionElement* option = Item(optIndex);
    if (option && option->Selected()) {
      nsAutoString value;
      option->GetValue(value);
      state->PutOption(optIndex, value);
    }
  }

  presState->SetStateProperty(state);

  if (mDisabledChanged) {
    // We do not want to save the real disabled state but the disabled
    // attribute.
    presState->SetDisabled(HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
  }

  return NS_OK;
}

bool
HTMLSelectElement::RestoreState(nsPresState* aState)
{
  // Get the presentation state object to retrieve our stuff out of.
  nsCOMPtr<SelectState> state(
    do_QueryInterface(aState->GetStateProperty()));

  if (state) {
    RestoreStateTo(state);

    // Don't flush, if the frame doesn't exist yet it doesn't care if
    // we're reset or not.
    DispatchContentReset();
  }

  if (aState->IsDisabledSet() && !aState->GetDisabled()) {
    IgnoredErrorResult rv;
    SetDisabled(false, rv);
  }

  return false;
}

void
HTMLSelectElement::RestoreStateTo(SelectState* aNewSelected)
{
  if (!mIsDoneAddingChildren) {
    mRestoreState = aNewSelected;
    return;
  }

  uint32_t len = Length();
  uint32_t mask = IS_SELECTED | CLEAR_ALL | SET_DISABLED | NOTIFY;

  // First clear all
  SetOptionsSelectedByIndex(-1, -1, mask);

  // Next set the proper ones
  for (uint32_t i = 0; i < len; i++) {
    HTMLOptionElement* option = Item(i);
    if (option) {
      nsAutoString value;
      option->GetValue(value);
      if (aNewSelected->ContainsOption(i, value)) {
        SetOptionsSelectedByIndex(i, i, IS_SELECTED | SET_DISABLED | NOTIFY);
      }
    }
  }
}

NS_IMETHODIMP
HTMLSelectElement::Reset()
{
  uint32_t numSelected = 0;

  //
  // Cycle through the options array and reset the options
  //
  uint32_t numOptions = Length();

  for (uint32_t i = 0; i < numOptions; i++) {
    RefPtr<HTMLOptionElement> option = Item(i);
    if (option) {
      //
      // Reset the option to its default value
      //

      uint32_t mask = SET_DISABLED | NOTIFY | NO_RESELECT;
      if (option->DefaultSelected()) {
        mask |= IS_SELECTED;
        numSelected++;
      }

      SetOptionsSelectedByIndex(i, i, mask);
      option->SetSelectedChanged(false);
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
HTMLSelectElement::SubmitNamesValues(HTMLFormSubmission* aFormSubmission)
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
  uint32_t len = Length();

  nsAutoString mozType;
  nsCOMPtr<nsIFormProcessor> keyGenProcessor;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::moztype, mozType) &&
      mozType.EqualsLiteral("-mozilla-keygen")) {
    keyGenProcessor = do_GetService(kFormProcessorCID);
  }

  for (uint32_t optIndex = 0; optIndex < len; optIndex++) {
    HTMLOptionElement* option = Item(optIndex);

    // Don't send disabled options
    if (!option || IsOptionDisabled(option)) {
      continue;
    }

    if (!option->Selected()) {
      continue;
    }

    nsString value;
    option->GetValue(value);

    if (keyGenProcessor) {
      nsString tmp(value);
      if (NS_SUCCEEDED(keyGenProcessor->ProcessValue(this, name, tmp))) {
        value = tmp;
      }
    }

    aFormSubmission->AddNameValuePair(name, value);
  }

  return NS_OK;
}

void
HTMLSelectElement::DispatchContentReset()
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
AddOptions(nsIContent* aRoot, HTMLOptionsCollection* aArray)
{
  for (nsIContent* child = aRoot->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    HTMLOptionElement* opt = HTMLOptionElement::FromContent(child);
    if (opt) {
      aArray->AppendOption(opt);
    } else if (child->IsHTMLElement(nsGkAtoms::optgroup)) {
      for (nsIContent* grandchild = child->GetFirstChild();
           grandchild;
           grandchild = grandchild->GetNextSibling()) {
        opt = HTMLOptionElement::FromContent(grandchild);
        if (opt) {
          aArray->AppendOption(opt);
        }
      }
    }
  }
}

void
HTMLSelectElement::RebuildOptionsArray(bool aNotify)
{
  mOptions->Clear();
  AddOptions(this, mOptions);
  FindSelectedIndex(0, aNotify);
}

bool
HTMLSelectElement::IsValueMissing() const
{
  if (!Required()) {
    return false;
  }

  uint32_t length = Length();

  for (uint32_t i = 0; i < length; ++i) {
    RefPtr<HTMLOptionElement> option = Item(i);
    // Check for a placeholder label option, don't count it as a valid value.
    if (i == 0 && !Multiple() && Size() <= 1 && option->GetParent() == this) {
      nsAutoString value;
      option->GetValue(value);
      if (value.IsEmpty()) {
        continue;
      }
    }

    if (!option->Selected()) {
      continue;
    }

    if (IsOptionDisabled(option)) {
      continue;
    }

    return false;
  }

  return true;
}

void
HTMLSelectElement::UpdateValueMissingValidityState()
{
  SetValidityState(VALIDITY_STATE_VALUE_MISSING, IsValueMissing());
}

nsresult
HTMLSelectElement::GetValidationMessage(nsAString& aValidationMessage,
                                        ValidityStateType aType)
{
  switch (aType) {
    case VALIDITY_STATE_VALUE_MISSING: {
      nsAutoString message;
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

void
HTMLSelectElement::VerifyOptionsArray()
{
  int32_t index = 0;
  for (nsIContent* child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    HTMLOptionElement* opt = HTMLOptionElement::FromContent(child);
    if (opt) {
      NS_ASSERTION(opt == mOptions->ItemAsOption(index++),
                   "Options collection broken");
    } else if (child->IsHTMLElement(nsGkAtoms::optgroup)) {
      for (nsIContent* grandchild = child->GetFirstChild();
           grandchild;
           grandchild = grandchild->GetNextSibling()) {
        opt = HTMLOptionElement::FromContent(grandchild);
        if (opt) {
          NS_ASSERTION(opt == mOptions->ItemAsOption(index++),
                       "Options collection broken");
        }
      }
    }
  }
}

#endif

void
HTMLSelectElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(IsDisabled());
}

void
HTMLSelectElement::FieldSetDisabledChanged(bool aNotify)
{
  // This *has* to be called before UpdateBarredFromConstraintValidation and
  // UpdateValueMissingValidityState because these two functions depend on our
  // disabled state.
  nsGenericHTMLFormElementWithState::FieldSetDisabledChanged(aNotify);

  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();
  UpdateState(aNotify);
}

void
HTMLSelectElement::SetSelectionChanged(bool aValue, bool aNotify)
{
  if (!mDefaultSelectionSet) {
    return;
  }

  UpdateSelectedOptions();

  bool previousSelectionChangedValue = mSelectionHasChanged;
  mSelectionHasChanged = aValue;

  if (mSelectionHasChanged != previousSelectionChangedValue) {
    UpdateState(aNotify);
  }
}

void
HTMLSelectElement::UpdateSelectedOptions()
{
  if (mSelectedOptions) {
    mSelectedOptions->SetDirty();
  }
}

bool
HTMLSelectElement::OpenInParentProcess()
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIComboboxControlFrame* comboFrame = do_QueryFrame(formControlFrame);
  if (comboFrame) {
    return comboFrame->IsOpenInParentProcess();
  }

  return false;
}

void
HTMLSelectElement::SetOpenInParentProcess(bool aVal)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIComboboxControlFrame* comboFrame = do_QueryFrame(formControlFrame);
  if (comboFrame) {
    comboFrame->SetOpenInParentProcess(aVal);
  }
}

void
HTMLSelectElement::SetPreviewValue(const nsAString& aValue)
{
  mPreviewValue = aValue;
  nsContentUtils::RemoveNewlines(mPreviewValue);
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  nsIComboboxControlFrame* comboFrame = do_QueryFrame(formControlFrame);
  if (comboFrame) {
    comboFrame->RedisplaySelectedText();
  }
}

JSObject*
HTMLSelectElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLSelectElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
