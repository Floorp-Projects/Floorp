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
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSHTMLSelectElement.h"
#include "nsIDOMNSXBLFormControl.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsITextContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsMappedAttributes.h"
#include "nsIForm.h"
#include "nsIFormSubmission.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIOptionElement.h"
#include "nsIEventStateManager.h"
#include "nsGenericDOMHTMLCollection.h"
#include "nsISelectElement.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMHTMLOptionsCollection.h"
#include "nsIDOMNSHTMLOptionCollectn.h"
#include "nsGUIEvent.h"
#include "nsIBoxObject.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDocumentEvent.h"

// PresState
#include "nsXPCOM.h"
#include "nsIPresState.h"
#include "nsIComponentManager.h"
#include "nsCheapSets.h"

// Notify/query select frame for selectedIndex
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsIFrame.h"

#include "nsDOMError.h"
#include "nsRuleNode.h"


class nsHTMLSelectElement;

/**
 * The collection of options in the select (what you get back when you do
 * select.options in DOM)
 */
class nsHTMLOptionCollection: public nsIDOMHTMLOptionsCollection,
                              public nsIDOMNSHTMLOptionCollection,
                              public nsGenericDOMHTMLCollection
{
public:
  nsHTMLOptionCollection(nsHTMLSelectElement* aSelect);
  virtual ~nsHTMLOptionCollection();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLOptionsCollection interface
  NS_DECL_NSIDOMHTMLOPTIONSCOLLECTION

  // nsIDOMNSHTMLOptionCollection interface
  NS_DECL_NSIDOMNSHTMLOPTIONCOLLECTION

  // nsIDOMHTMLCollection interface, all its methods are defined in
  // nsIDOMHTMLOptionsCollection

  // Helpers for nsHTMLSelectElement
  /**
   * Insert an option
   * @param aOption the option to insert
   * @param aIndex the index to insert at
   */
  nsresult InsertElementAt(nsIDOMHTMLOptionElement* aOption, PRInt32 aIndex);
  /**
   * Remove an option
   * @param aIndex the index of the option to remove
   */
  nsresult RemoveElementAt(PRInt32 aIndex);
  /**
   * Get the option at the index
   * @param aIndex the index
   * @param aReturn the option returned [OUT]
   */
  nsIDOMHTMLOptionElement *ItemAsOption(PRInt32 aIndex);

  /**
   * Drop the reference to the select.  Called during select destruction.
   */
  void DropReference();

private:
  /** The list of options (holds strong references) */
  nsCOMArray<nsIDOMHTMLOptionElement> mElements;
  /** The select element that contains this array */
  nsHTMLSelectElement* mSelect;
};


/**
 * The restore state used by select
 */
class nsSelectState : public nsISupports {
public:
  nsSelectState()
  {
  }
  virtual ~nsSelectState()
  {
  }

  NS_DECL_ISUPPORTS

  void PutOption(PRInt32 aIndex, const nsAString& aValue)
  {
    // If the option is empty, store the index.  If not, store the value.
    if (aValue.IsEmpty()) {
      mIndices.Put(aIndex);
    } else {
      mValues.Put(aValue);
    }
  }

  PRBool ContainsOption(PRInt32 aIndex, const nsAString& aValue)
  {
    return mValues.Contains(aValue) || mIndices.Contains(aIndex);
  }

private:
  nsCheapStringSet mValues;
  nsCheapInt32Set mIndices;
};

NS_IMPL_ISUPPORTS0(nsSelectState)


/**
 * Implementation of &lt;select&gt;
 */
class nsHTMLSelectElement : public nsGenericHTMLFormElement,
                            public nsIDOMHTMLSelectElement,
                            public nsIDOMNSHTMLSelectElement,
                            public nsIDOMNSXBLFormControl,
                            public nsISelectElement
{
public:
  nsHTMLSelectElement(nsINodeInfo *aNodeInfo, PRBool aFromParser = PR_FALSE);
  virtual ~nsHTMLSelectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLSelectElement
  NS_DECL_NSIDOMHTMLSELECTELEMENT

  // nsIDOMNSHTMLSelectElement
  NS_DECL_NSIDOMNSHTMLSELECTELEMENT

  // nsIDOMNSXBLFormControl
  NS_DECL_NSIDOMNSXBLFORMCONTROL

  // nsIContent
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify, PRBool aDeepSetDocument);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);

  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);

  virtual void SetFocus(nsIPresContext* aPresContext);
  virtual void RemoveFocus(nsIPresContext* aPresContext);

  // Overriden nsIFormControl methods
  NS_IMETHOD_(PRInt32) GetType() { return NS_FORM_SELECT; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  NS_IMETHOD RestoreState(nsIPresState* aState);

  // nsISelectElement
  NS_DECL_NSISELECTELEMENT

  virtual void DoneAddingChildren();
  virtual PRBool IsDoneAddingChildren();

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                    PRInt32 aModType,
                                    nsChangeHint& aHint) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;


protected:
  // Helper Methods
  /**
   * Check whether the option specified by the index is selected
   * @param aIndex the index
   * @return whether the option at the index is selected
   */
  PRBool IsOptionSelectedByIndex(PRInt32 aIndex);
  /**
   * Starting with (and including) aStartIndex, find the first selected index
   * and set mSelectedIndex to it.
   * @param aStartIndex the index to start with
   */
  void FindSelectedIndex(PRInt32 aStartIndex);
  /**
   * Select some option if possible (generally the first non-disabled option).
   * @return true if something was selected, false otherwise
   */
  PRBool SelectSomething();
  /**
   * Call SelectSomething(), but only if nothing is selected
   * @see SelectSomething()
   * @return true if something was selected, false otherwise
   */
  PRBool CheckSelectSomething();
  /**
   * Called to trigger notifications of frames and fixing selected index
   *
   * @param aSelectFrame the frame for this content (could be null)
   * @param aPresContext the current pres context
   * @param aIndex the index that was selected or deselected
   * @param aSelected whether the index was selected or deselected
   * @param aNotify whether to notify the style system and such
   */
  void OnOptionSelected(nsISelectControlFrame* aSelectFrame,
                        nsIPresContext* aPresContext,
                        PRInt32 aIndex,
                        PRBool aSelected,
                        PRBool aNotify);
  /**
   * Restore state to a particular state string (representing the options)
   * @param aNewSelected the state string to restore to
   */
  void RestoreStateTo(nsSelectState* aNewSelected);

#ifdef DEBUG_john
  // Don't remove these, por favor.  They're very useful in debugging
  nsresult PrintOptions(nsIContent* aOptions, PRInt32 tabs);
#endif

  // Adding options
  /**
   * Insert option(s) into the options[] array and perform notifications
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start adding options into the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult InsertOptionsIntoList(nsIContent* aOptions,
                                 PRInt32 aListIndex,
                                 PRInt32 aDepth);
  /**
   * Remove option(s) from the options[] array
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start removing options from the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult RemoveOptionsFromList(nsIContent* aOptions,
                                 PRInt32 aListIndex,
                                 PRInt32 aDepth);
  /**
   * Insert option(s) into the options[] array (called by InsertOptionsIntoList)
   * @param aOptions the option or optgroup being added
   * @param aInsertIndex the index to start adding options into the list at
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult InsertOptionsIntoListRecurse(nsIContent* aOptions,
                                        PRInt32* aInsertIndex,
                                        PRInt32 aDepth);
  /**
   * Remove option(s) from the options[] array (called by RemoveOptionsFromList)
   * @param aOptions the option or optgroup being added
   * @param aListIndex the index to start removing options from the list at
   * @param aNumRemoved the number removed so far [OUT]
   * @param aDepth the depth of aOptions (1=direct child of select ...)
   */
  nsresult RemoveOptionsFromListRecurse(nsIContent* aOptions,
                                        PRInt32 aRemoveIndex,
                                        PRInt32* aNumRemoved,
                                        PRInt32 aDepth);
  /**
   * Find out how deep this content is from the select (1=direct child)
   * @param aContent the content to check
   * @return the depth
   */
  PRInt32 GetContentDepth(nsIContent* aContent);
  /**
   * Get the index of the first option at, under or following the content in
   * the select, or length of options[] if none are found
   * @param aOptions the content
   * @return the index of the first option
   */
  PRInt32 GetOptionIndexAt(nsIContent* aOptions);
  /**
   * Get the next option following the content in question (not at or under)
   * (this could include siblings of the current content or siblings of the
   * parent or children of siblings of the parent).
   * @param aOptions the content
   * @return the index of the next option after the content
   */
  PRInt32 GetOptionIndexAfter(nsIContent* aOptions);
  /**
   * Get the first option index at or under the content in question.
   * @param aOptions the content
   * @return the index of the first option at or under the content
   */
  PRInt32 GetFirstOptionIndex(nsIContent* aOptions);
  /**
   * Get the first option index under the content in question, within the
   * range specified.
   * @param aOptions the content
   * @param aStartIndex the first child to look at
   * @param aEndIndex the child *after* the last child to look at
   * @return the index of the first option at or under the content
   */
  PRInt32 GetFirstChildOptionIndex(nsIContent* aOptions,
                                   PRInt32 aStartIndex,
                                   PRInt32 aEndIndex);

  /**
   * Get the frame as an nsISelectControlFrame (MAY RETURN NULL)
   * @return the select frame, or null
   */
  nsISelectControlFrame *GetSelectFrame();

  /**
   * Helper method for dispatching custom DOM events to our anonymous subcontent
   * (for XBL form controls)
   * @param aName the name of the event to dispatch
   */
  void DispatchDOMEvent(const nsAString& aName);

  /** The options[] array */
  nsHTMLOptionCollection* mOptions;
  /** false if the parser is in the middle of adding children. */
  PRBool    mIsDoneAddingChildren;
  /** The number of non-options as children of the select */
  PRUint32  mNonOptionChildren;
  /** The number of optgroups anywhere under the select */
  PRUint32  mOptGroupCount;
  /**
   * The current selected index for selectedIndex (will be the first selected
   * index if multiple are selected)
   */
  PRInt32   mSelectedIndex;
  /**
   * The temporary restore state in case we try to restore before parser is
   * done adding options
   */
  nsSelectState* mRestoreState;
};


//----------------------------------------------------------------------
//
// nsHTMLSelectElement
//

// construction, destruction


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Select)

nsHTMLSelectElement::nsHTMLSelectElement(nsINodeInfo *aNodeInfo,
                                         PRBool aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mOptions(new nsHTMLOptionCollection(this)),
    mIsDoneAddingChildren(!aFromParser),
    mNonOptionChildren(0),
    mOptGroupCount(0),
    mSelectedIndex(-1),
    mRestoreState(nsnull)
{
  // DoneAddingChildren() will be called later if it's from the parser,
  // otherwise it is

  NS_IF_ADDREF(mOptions);
}

nsHTMLSelectElement::~nsHTMLSelectElement()
{
  if (mOptions) {
    mOptions->DropReference();
    NS_RELEASE(mOptions);
  }
  NS_IF_RELEASE(mRestoreState);
}

// ISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLSelectElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLSelectElement, nsGenericElement)


// QueryInterface implementation for nsHTMLSelectElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLSelectElement,
                                    nsGenericHTMLFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLSelectElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLSelectElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSXBLFormControl)
  NS_INTERFACE_MAP_ENTRY(nsISelectElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLSelectElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLSelectElement


NS_IMPL_HTML_DOM_CLONENODE(Select)


NS_IMETHODIMP
nsHTMLSelectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


// nsIContent
nsresult
nsHTMLSelectElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                   PRBool aDeepSetDocument)
{
  WillAddOptions(aKid, this, GetChildCount());

  // Actually perform the append
  return nsGenericHTMLFormElement::AppendChildTo(aKid, aNotify,
                                                 aDeepSetDocument);
}

nsresult
nsHTMLSelectElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                   PRBool aNotify, PRBool aDeepSetDocument)
{
  WillAddOptions(aKid, this, aIndex);

  return nsGenericHTMLFormElement::InsertChildAt(aKid, aIndex, aNotify,
                                                 aDeepSetDocument);
}

nsresult
nsHTMLSelectElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  WillRemoveOptions(this, aIndex);

  return nsGenericHTMLFormElement::RemoveChildAt(aIndex, aNotify);
}


// SelectElement methods

nsresult
nsHTMLSelectElement::InsertOptionsIntoList(nsIContent* aOptions,
                                           PRInt32 aListIndex,
                                           PRInt32 aDepth)
{
  PRInt32 insertIndex = aListIndex;
  nsresult rv = InsertOptionsIntoListRecurse(aOptions, &insertIndex, aDepth);
  NS_ENSURE_SUCCESS(rv, rv);

  // Deal with the selected list
  if (insertIndex - aListIndex) {
    // Fix the currently selected index
    if (aListIndex <= mSelectedIndex) {
      mSelectedIndex += (insertIndex - aListIndex);
    }

    // Get the frame stuff for notification. No need to flush here
    // since if there's no frame for the select yet the select will
    // get into the right state once it's created.
    nsISelectControlFrame* selectFrame = GetSelectFrame();

    nsCOMPtr<nsIPresContext> presContext;
    if (selectFrame) {
      GetPresContext(this, getter_AddRefs(presContext));
    }

    // Actually select the options if the added options warrant it
    nsCOMPtr<nsIDOMNode> optionNode;
    nsCOMPtr<nsIDOMHTMLOptionElement> option;
    for (PRInt32 i=aListIndex;i<insertIndex;i++) {
      // Notify the frame that the option is added
      if (selectFrame) {
        selectFrame->AddOption(presContext, i);
      }

      Item(i, getter_AddRefs(optionNode));
      option = do_QueryInterface(optionNode);
      if (option) {
        PRBool selected;
        option->GetSelected(&selected);
        if (selected) {
          // Clear all other options
          PRBool isMultiple;
          GetMultiple(&isMultiple);
          if (!isMultiple) {
            SetOptionsSelectedByIndex(i, i, PR_TRUE, PR_TRUE, PR_TRUE, PR_TRUE, nsnull);
          }

          // This is sort of a hack ... we need to notify that the option was
          // set and change selectedIndex even though we didn't really change
          // its value.
          OnOptionSelected(selectFrame, presContext, i, PR_TRUE, PR_FALSE);
        }
      }
    }

    CheckSelectSomething();
  }

  return NS_OK;
}

#ifdef DEBUG_john
nsresult
nsHTMLSelectElement::PrintOptions(nsIContent* aOptions, PRInt32 tabs)
{
  for (PRInt32 i=0;i<tabs;i++) {
    printf("  ");
  }

  nsCOMPtr<nsIDOMHTMLElement> elem(do_QueryInterface(aOptions));
  if (elem) {
    nsAutoString s;
    elem->GetTagName(s);
    printf("<%s>\n", NS_ConvertUCS2toUTF8(s).get());
  } else {
    printf(">>text\n");
  }

  // Recurse down into optgroups
  //
  // I *would* put a restriction in here to only search under
  // optgroups (and not, for example, <P></P>), but it really
  // doesn't *hurt* to search under other stuff and it's more
  // efficient in the normal only-optgroup-and-option case
  // (one less QueryInterface).
  PRUint32 numChildren = aOptions->GetChildCount();

  for (PRUint32 i = 0; i < numChildren; ++i) {
    PrintOptions(aOptions->GetChildAt(i), tabs + 1);
  }

  return NS_OK;
}
#endif

nsresult
nsHTMLSelectElement::RemoveOptionsFromList(nsIContent* aOptions,
                                           PRInt32 aListIndex,
                                           PRInt32 aDepth)
{
  PRInt32 numRemoved = 0;
  nsresult rv = RemoveOptionsFromListRecurse(aOptions, aListIndex, &numRemoved,
                                             aDepth);
  NS_ENSURE_SUCCESS(rv, rv);

  if (numRemoved) {
    // Tell the widget we removed the options
    nsISelectControlFrame* selectFrame = GetSelectFrame();
    if (selectFrame) {
      nsCOMPtr<nsIPresContext> presContext;
      GetPresContext(this, getter_AddRefs(presContext));
      for (int i = aListIndex; i < aListIndex + numRemoved; ++i) {
        selectFrame->RemoveOption(presContext, i);
      }
    }

    // Fix the selected index
    if (aListIndex <= mSelectedIndex) {
      if (mSelectedIndex < (aListIndex+numRemoved)) {
        // aListIndex <= mSelectedIndex < aListIndex+numRemoved
        // Find a new selected index if it was one of the ones removed.
        FindSelectedIndex(aListIndex);
      } else {
        // Shift the selected index if something in front of it was removed
        // aListIndex+numRemoved <= mSelectedIndex
        mSelectedIndex -= numRemoved;
      }
    }

    // Select something in case we removed the selected option on a
    // single select
    CheckSelectSomething();
  }

  return NS_OK;
}

static PRBool IsOptGroup(nsIContent *aContent)
{
  nsINodeInfo *ni = aContent->GetNodeInfo();

  return (ni && ni->Equals(nsHTMLAtoms::optgroup) &&
          aContent->IsContentOfType(nsIContent::eHTML));
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

  nsCOMPtr<nsIDOMHTMLOptionElement> optElement(do_QueryInterface(aOptions));
  if (optElement) {
    nsresult rv = mOptions->InsertElementAt(optElement, *aInsertIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aInsertIndex)++;
    return NS_OK;
  }

  // If it's at the top level, then we just found out there are non-options
  // at the top level, which will throw off the insert count
  if (aDepth == 0) {
    mNonOptionChildren++;
  }

  if (IsOptGroup(aOptions)) {
    mOptGroupCount++;
    DispatchDOMEvent(NS_LITERAL_STRING("selectHasGroups"));
  }

  // Recurse down into optgroups
  //
  // I *would* put a restriction in here to only search under
  // optgroups (and not, for example, <P></P>), but it really
  // doesn't *hurt* to search under other stuff and it's more
  // efficient in the normal only-optgroup-and-option case
  // (one less QueryInterface).
  PRUint32 numChildren = aOptions->GetChildCount();

  for (PRUint32 i = 0; i < numChildren; ++i) {
    nsresult rv = InsertOptionsIntoListRecurse(aOptions->GetChildAt(i),
                                               aInsertIndex, aDepth+1);
    NS_ENSURE_SUCCESS(rv, rv);
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
    nsresult rv = mOptions->RemoveElementAt(aRemoveIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aNumRemoved)++;
    return NS_OK;
  }

  // Yay, one less artifact at the top level.
  if (aDepth == 0) {
    mNonOptionChildren--;
  }

  if (mOptGroupCount) {
    if (IsOptGroup(aOptions)) {
      mOptGroupCount--;
      DispatchDOMEvent(NS_LITERAL_STRING("selectHasNoGroups"));
    }
  }

  // Recurse down deeper for options
  //
  // I *would* put a restriction in here to only search under
  // optgroups (and not, for example, <P></P>), but it really
  // doesn't *hurt* to search under other stuff and it's more
  // efficient in the normal only-optgroup-and-option case
  // (one less QueryInterface).
  PRUint32 numChildren = aOptions->GetChildCount();

  for (PRUint32 i = 0; i < numChildren; ++i) {
    nsresult rv = RemoveOptionsFromListRecurse(aOptions->GetChildAt(i),
                                               aRemoveIndex,
                                               aNumRemoved,
                                               aDepth + 1);
    NS_ENSURE_SUCCESS(rv, rv);
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
                                    PRInt32 aContentIndex)
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

  return InsertOptionsIntoList(aOptions, ind, level);
}

NS_IMETHODIMP
nsHTMLSelectElement::WillRemoveOptions(nsIContent* aParent,
                                       PRInt32 aContentIndex)
{
  nsresult rv = NS_OK;

  PRInt32 level = GetContentDepth(aParent);
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
      rv = RemoveOptionsFromList(currentKid, ind, level);
    }
  }

  return rv;
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
  nsCOMPtr<nsIDOMHTMLOptionElement> optElement(do_QueryInterface(aOptions));
  if (optElement) {
    GetOptionIndex(optElement, 0, PR_TRUE, &listIndex);
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
  nsIFormControlFrame* form_control_frame = GetFormControlFrame(PR_FALSE);

  nsISelectControlFrame *select_frame = nsnull;

  if (form_control_frame) {
    CallQueryInterface(form_control_frame, &select_frame);
  }

  return select_frame;
}

NS_IMETHODIMP
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
  while (ancestor != NS_STATIC_CAST(nsIDOMNode*, this)) {
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
  *aValue = mOptions;
  NS_IF_ADDREF(*aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetType(nsAString& aType)
{
  PRBool isMultiple;
  GetMultiple(&isMultiple);
  if (isMultiple) {
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

NS_IMETHODIMP
nsHTMLSelectElement::SetLength(PRUint32 aLength)
{
  nsresult rv=NS_OK;

  PRUint32 curlen;
  PRInt32 i;

  rv = GetLength(&curlen);
  if (NS_FAILED(rv)) {
    curlen = 0;
  }

  if (curlen && (curlen > aLength)) { // Remove extra options
    for (i = (curlen - 1); (i >= (PRInt32)aLength) && NS_SUCCEEDED(rv); i--) {
      rv = Remove(i);
    }
  } else if (aLength) {
    // This violates the W3C DOM but we do this for backwards compatibility
    nsCOMPtr<nsINodeInfo> nodeInfo;

    nsContentUtils::NameChanged(mNodeInfo, nsHTMLAtoms::option,
                                getter_AddRefs(nodeInfo));

    nsCOMPtr<nsIHTMLContent> element = NS_NewHTMLOptionElement(nodeInfo);
    if (!element) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsITextContent> text;
    rv = NS_NewTextNode(getter_AddRefs(text));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = element->AppendChildTo(text, PR_FALSE, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(element));

    for (i = curlen; i < (PRInt32)aLength; i++) {
      nsCOMPtr<nsIDOMNode> tmpNode;

      rv = AppendChild(node, getter_AddRefs(tmpNode));
      NS_ENSURE_SUCCESS(rv, rv);

      if (i < ((PRInt32)aLength - 1)) {
        nsCOMPtr<nsIDOMNode> newNode;

        rv = node->CloneNode(PR_TRUE, getter_AddRefs(newNode));
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

NS_IMETHODIMP
nsHTMLSelectElement::SetSelectedIndex(PRInt32 aIndex)
{
  return SetOptionsSelectedByIndex(aIndex, aIndex, PR_TRUE,
                                   PR_TRUE, PR_TRUE, PR_TRUE, nsnull);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                                    PRInt32 aStartIndex, PRBool aForward,
                                    PRInt32* aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  PRUint32 numOptions;

  nsresult rv = GetLength(&numOptions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aStartIndex = PR_MIN(aStartIndex, (PRInt32)numOptions - 1);
  aStartIndex = PR_MAX(0, aStartIndex);

  nsCOMPtr<nsIDOMNode> node;

  for (PRInt32 i = aStartIndex;
       (aForward ? i < (PRInt32)numOptions : i != -1);
       i += (aForward ? 1 : -1)) {
    rv = Item(i, getter_AddRefs(node));
    if (NS_SUCCEEDED(rv) && node) {
      nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));
      if (option && option.get() == aOption) {
        *aIndex = i;
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

PRBool
nsHTMLSelectElement::IsOptionSelectedByIndex(PRInt32 aIndex)
{
  nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(aIndex);
  PRBool isSelected = PR_FALSE;
  if (option) {
    option->GetSelected(&isSelected);
  }
  return isSelected;
}

void
nsHTMLSelectElement::OnOptionSelected(nsISelectControlFrame* aSelectFrame,
                                      nsIPresContext* aPresContext,
                                      PRInt32 aIndex,
                                      PRBool aSelected,
                                      PRBool aNotify)
{
  // Set the selected index
  if (aSelected && (aIndex < mSelectedIndex || mSelectedIndex < 0)) {
    mSelectedIndex = aIndex;
  } else if (!aSelected && aIndex == mSelectedIndex) {
    FindSelectedIndex(aIndex+1);
  }

  // Tell the option to get its bad self selected
  nsCOMPtr<nsIDOMNode> option;
  Item(aIndex, getter_AddRefs(option));
  if (option) {
    nsCOMPtr<nsIOptionElement> optionElement(do_QueryInterface(option));
    optionElement->SetSelectedInternal(aSelected, aNotify);
  }

  // Let the frame know too
  if (aSelectFrame) {
    aSelectFrame->OnOptionSelected(aPresContext, aIndex, aSelected);
  }
}

void
nsHTMLSelectElement::FindSelectedIndex(PRInt32 aStartIndex)
{
  mSelectedIndex = -1;
  PRUint32 len;
  GetLength(&len);
  for (PRInt32 i=aStartIndex; i<(PRInt32)len; i++) {
    if (IsOptionSelectedByIndex(i)) {
      mSelectedIndex = i;
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
NS_IMETHODIMP
nsHTMLSelectElement::SetOptionsSelectedByIndex(PRInt32 aStartIndex,
                                               PRInt32 aEndIndex,
                                               PRBool aIsSelected,
                                               PRBool aClearAll,
                                               PRBool aSetDisabled,
                                               PRBool aNotify,
                                               PRBool* aChangedSomething)
{
#if 0
  printf("SetOption(%d-%d, %c, ClearAll=%c)\n", aStartIndex, aEndIndex,
                                       (aIsSelected ? 'Y' : 'N'),
                                       (aClearAll ? 'Y' : 'N'));
#endif
  if (aChangedSomething) {
    *aChangedSomething = PR_FALSE;
  }

  nsresult rv;

  // Don't bother if the select is disabled
  if (!aSetDisabled) {
    PRBool selectIsDisabled = PR_FALSE;
    rv = GetDisabled(&selectIsDisabled);
    if (NS_SUCCEEDED(rv) && selectIsDisabled) {
      return NS_OK;
    }
  }

  // Don't bother if there are no options
  PRUint32 numItems = 0;
  GetLength(&numItems);
  if (numItems == 0) {
    return NS_OK;
  }

  // First, find out whether multiple items can be selected
  PRBool isMultiple;
  rv = GetMultiple(&isMultiple);
  if (NS_FAILED(rv)) {
    isMultiple = PR_FALSE;
  }

  // These variables tell us whether any options were selected
  // or deselected.
  PRBool optionsSelected = PR_FALSE;
  PRBool optionsDeselected = PR_FALSE;

  nsISelectControlFrame *selectFrame = nsnull;
  PRBool did_get_frame = PR_FALSE;

  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(this, getter_AddRefs(presContext));

  if (aIsSelected) {
    // Only select the first value if it's not multiple
    if (!isMultiple) {
      aEndIndex = aStartIndex;
    }

    // This variable tells whether or not all of the options we attempted to
    // select are disabled.  If ClearAll is passed in as true, and we do not
    // select anything because the options are disabled, we will not clear the
    // other options.  (This is to make the UI work the way one might expect.)
    PRBool allDisabled = !aSetDisabled;

    //
    // Save a little time when clearing other options
    //
    PRInt32 previousSelectedIndex = mSelectedIndex;

    //
    // Select the requested indices
    //
    // If index is -1, everything will be deselected (bug 28143)
    if (aStartIndex != -1) {
      // Verify that the indices are within bounds
      if (aStartIndex >= (PRInt32)numItems || aStartIndex < 0
         || aEndIndex >= (PRInt32)numItems || aEndIndex < 0) {
        return NS_ERROR_FAILURE;
      }

      // Loop through the options and select them (if they are not disabled and
      // if they are not already selected).
      for (PRInt32 optIndex = aStartIndex; optIndex <= aEndIndex; optIndex++) {

        // Ignore disabled options.
        if (!aSetDisabled) {
          PRBool isDisabled;
          IsOptionDisabled(optIndex, &isDisabled);

          if (isDisabled) {
            continue;
          } else {
            allDisabled = PR_FALSE;
          }
        }

        nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
        if (option) {
          // If the index is already selected, ignore it.
          PRBool isSelected = PR_FALSE;
          option->GetSelected(&isSelected);
          if (!isSelected) {
            // To notify the frame if anything gets changed. No need
            // to flush here, if there's no frame yet we don't need to
            // force it to be created just to notify it about a change
            // in the select.
            selectFrame = GetSelectFrame();

            did_get_frame = PR_TRUE;

            OnOptionSelected(selectFrame, presContext, optIndex, PR_TRUE, aNotify);
            optionsSelected = PR_TRUE;
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
           optIndex < (PRInt32)numItems;
           optIndex++) {
        if (optIndex < aStartIndex || optIndex > aEndIndex) {
          nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
          if (option) {
            // If the index is already selected, ignore it.
            PRBool isSelected = PR_FALSE;
            option->GetSelected(&isSelected);
            if (isSelected) {
              if (!did_get_frame) {
                // To notify the frame if anything gets changed, don't
                // flush, if the frame doesn't exist we don't need to
                // create it just to tell it about this change.
                selectFrame = GetSelectFrame();

                did_get_frame = PR_TRUE;
              }

              OnOptionSelected(selectFrame, presContext, optIndex, PR_FALSE, aNotify);
              optionsDeselected = PR_TRUE;

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
        PRBool isDisabled;
        IsOptionDisabled(optIndex, &isDisabled);
        if (isDisabled) {
          continue;
        }
      }

      nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
      if (option) {
        // If the index is already selected, ignore it.
        PRBool isSelected = PR_FALSE;
        option->GetSelected(&isSelected);
        if (isSelected) {
          if (!did_get_frame) {
            // To notify the frame if anything gets changed, don't
            // flush, if the frame doesn't exist we don't need to
            // create it just to tell it about this change.
            selectFrame = GetSelectFrame();

            did_get_frame = PR_TRUE;
          }

          OnOptionSelected(selectFrame, presContext, optIndex, PR_FALSE, aNotify);
          optionsDeselected = PR_TRUE;
        }
      }
    }
  }

  // Make sure something is selected unless we were set to -1 (none)
  if (optionsDeselected && aStartIndex != -1) {
    optionsSelected = CheckSelectSomething() || optionsSelected;
  }

  // Let the caller know whether anything was changed
  if (optionsSelected || optionsDeselected) {
    if (aChangedSomething)
      *aChangedSomething = PR_TRUE;

    // Dispatch an event to notify the subcontent that the selected item has changed
    DispatchDOMEvent(NS_LITERAL_STRING("selectedItemChanged"));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::IsOptionDisabled(PRInt32 aIndex, PRBool* aIsDisabled)
{
  *aIsDisabled = PR_FALSE;
  nsCOMPtr<nsIDOMNode> optionNode;
  Item(aIndex, getter_AddRefs(optionNode));
  NS_ENSURE_TRUE(optionNode, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(optionNode);
  if (option) {
    PRBool isDisabled;
    option->GetDisabled(&isDisabled);
    if (isDisabled) {
      *aIsDisabled = PR_TRUE;
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
        PRBool isDisabled;
        optGroupElement->GetDisabled(&isDisabled);

        if (isDisabled) {
          *aIsDisabled = PR_TRUE;
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

    if (NS_SUCCEEDED(rv) && node) {
      nsCOMPtr<nsIHTMLContent> option = do_QueryInterface(node);

      if (option) {
        nsHTMLValue value;
        // first check to see if value is there and has a value
        rv = option->GetHTMLAttribute(nsHTMLAtoms::value, value);

        if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
          if (eHTMLUnit_String == value.GetUnit()) {
            value.GetStringValue(aValue);
          } else {
            aValue.SetLength(0);
          }

          return NS_OK;
        }
#if 0 // temporary for bug 4050
        // first check to see if label is there and has a value
        rv = option->GetHTMLAttribute(nsHTMLAtoms::label, value);

        if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
          if (eHTMLUnit_String == value.GetUnit()) {
            value.GetStringValue(aValue);
          } else {
            aValue.SetLength(0);
          }

          return NS_OK;
        }
#endif
        nsCOMPtr<nsIDOMHTMLOptionElement> optElement(do_QueryInterface(node));
        if (optElement) {
          optElement->GetText(aValue);
        }

        return NS_OK;
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetValue(const nsAString& aValue)
{
  nsresult rv = NS_OK;

  PRUint32 length;
  rv = GetLength(&length);
  if (NS_SUCCEEDED(rv)) {
    PRUint32 i;
    for (i = 0; i < length; i++) {
      nsCOMPtr<nsIDOMNode> node;

      rv = Item(i, getter_AddRefs(node));

      if (NS_SUCCEEDED(rv) && node) {
        nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(node);

        if (option) {
          nsAutoString optionVal;

          option->GetValue(optionVal);

          if (optionVal.Equals(aValue)) {
            SetSelectedIndex((PRInt32)i);

            break;
          }
        }
      }
    }
  }

  return rv;
}


NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Disabled, disabled)
NS_IMPL_BOOL_ATTR(nsHTMLSelectElement, Multiple, multiple)
NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Name, name)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, Size, size)
NS_IMPL_INT_ATTR(nsHTMLSelectElement, TabIndex, tabindex)

NS_IMETHODIMP
nsHTMLSelectElement::Blur()
{
  SetElementFocus(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::Focus()
{
  SetElementFocus(PR_TRUE);

  return NS_OK;
}

void
nsHTMLSelectElement::SetFocus(nsIPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  // first see if we are disabled or not. If disabled then do nothing.
  if (HasAttr(kNameSpaceID_None, nsHTMLAtoms::disabled)) {
    return;
  }

  aPresContext->EventStateManager()->SetContentState(this,
                                                     NS_EVENT_STATE_FOCUS);

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus.
  }
}

void
nsHTMLSelectElement::RemoveFocus(nsIPresContext* aPresContext)
{
  if (!aPresContext)
    return;

  // If we are disabled, we probably shouldn't have focus in the
  // first place, so allow it to be removed.

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_FALSE, PR_FALSE);
  }

  if (mDocument) {
    aPresContext->EventStateManager()->SetContentState(nsnull,
                                                       NS_EVENT_STATE_FOCUS);
  }
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

PRBool
nsHTMLSelectElement::CheckSelectSomething()
{
  if (mIsDoneAddingChildren) {
    PRInt32 size = 1;
    GetSize(&size);
    PRBool isMultiple;
    GetMultiple(&isMultiple);
    if (mSelectedIndex < 0 && !isMultiple && size <= 1) {
      return SelectSomething();
    }
  }
  return PR_FALSE;
}

PRBool
nsHTMLSelectElement::SelectSomething()
{
  // If we're not done building the select, don't play with this yet.
  if (!mIsDoneAddingChildren) {
    return PR_FALSE;
  }

  PRUint32 count;
  GetLength(&count);
  for (PRUint32 i=0; i<count; i++) {
    PRBool disabled;
    nsresult rv = IsOptionDisabled(i, &disabled);

    if (NS_FAILED(rv) || !disabled) {
      rv = SetSelectedIndex(i);
      NS_ENSURE_SUCCESS(rv, PR_FALSE);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool
nsHTMLSelectElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

void
nsHTMLSelectElement::DoneAddingChildren()
{
  mIsDoneAddingChildren = PR_TRUE;

  nsISelectControlFrame* selectFrame = GetSelectFrame();

  // If we foolishly tried to restore before we were done adding
  // content, restore the rest of the options proper-like
  if (mRestoreState) {
    RestoreStateTo(mRestoreState);
    NS_RELEASE(mRestoreState);
  }

  // Notify the frame
  if (selectFrame) {
    selectFrame->DoneAddingChildren(PR_TRUE);
  }

  // Restore state
  RestoreFormControlState(this, this);

  // Now that we're done, select something (if it's a single select something
  // must be selected)
  CheckSelectSomething();
}

PRBool
nsHTMLSelectElement::ParseAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::size) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }
  if (aAttribute == nsHTMLAtoms::tabindex) {
    return aResult.ParseIntWithBounds(aValue, 0, 32767);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            PRInt32 aModType,
                                            nsChangeHint& aHint) const
{
  nsresult rv =
    nsGenericHTMLFormElement::GetAttributeChangeHint(aAttribute, aModType,
                                                     aHint);
  if (aAttribute == nsHTMLAtoms::multiple ||
      aAttribute == nsHTMLAtoms::size) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_FRAMECHANGE);
  }
  return rv;
}

NS_IMETHODIMP_(PRBool)
nsHTMLSelectElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageAlignAttributeMap
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

NS_IMETHODIMP
nsHTMLSelectElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;

  return NS_OK;
}


nsresult
nsHTMLSelectElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  // Do not process any DOM events if the element is disabled
  PRBool disabled;
  nsresult rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }

  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
  nsIFrame* formFrame = nsnull;

  if (formControlFrame &&
      NS_SUCCEEDED(CallQueryInterface(formControlFrame, &formFrame)) &&
      formFrame)
  {
    const nsStyleUserInterface* uiStyle = formFrame->GetStyleUserInterface();

    if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
      return NS_OK;
    }
  }

  // Must notify the frame that the blur event occurred
  // NOTE: At this point EventStateManager has not yet set the
  /// new content as having focus so this content is still considered
  // the focused element. So the ComboboxControlFrame tracks the focus
  // at a class level (Bug 32920)
  if ((nsEventStatus_eIgnore == *aEventStatus) &&
      !(aFlags & NS_EVENT_FLAG_CAPTURE) && !(aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) &&
      (aEvent->message == NS_BLUR_CONTENT) && formControlFrame) {
    formControlFrame->SetFocus(PR_FALSE, PR_TRUE);
  }

  return nsGenericHTMLFormElement::HandleDOMEvent(aPresContext, aEvent,
                                                  aDOMEvent, aFlags,
                                                  aEventStatus);
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLSelectElement::SaveState()
{
  nsSelectState* state = new nsSelectState();
  if (!state) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(state);

  PRUint32 len;
  GetLength(&len);

  for (PRUint32 optIndex = 0; optIndex < len; optIndex++) {
    nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
    if (option) {
      PRBool isSelected;
      option->GetSelected(&isSelected);
      if (isSelected) {
        nsAutoString value;
        option->GetValue(value);
        state->PutOption(optIndex, value);
      }
    }
  }

  nsCOMPtr<nsIPresState> presState;
  nsresult rv = GetPrimaryPresState(this, getter_AddRefs(presState));
  if (presState) {
    rv = presState->SetStatePropertyAsSupports(NS_LITERAL_STRING("selecteditems"),
                                           state);
    NS_ASSERTION(NS_SUCCEEDED(rv), "selecteditems set failed!");
  }

  NS_RELEASE(state);

  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::RestoreState(nsIPresState* aState)
{
  // Get the presentation state object to retrieve our stuff out of.
  nsCOMPtr<nsISupports> state;
  nsresult rv = aState->GetStatePropertyAsSupports(NS_LITERAL_STRING("selecteditems"),
                                                   getter_AddRefs(state));
  if (NS_SUCCEEDED(rv)) {
    RestoreStateTo((nsSelectState*)(nsISupports*)state);

    // Don't flush, if the frame doesn't exist yet it doesn't care if
    // we're reset or not.
    nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
    if (formControlFrame) {
      formControlFrame->OnContentReset();
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetBoxObject(nsIBoxObject** aResult)
{
  *aResult = nsnull;

  if (!mDocument)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(mDocument));
  return nsDoc->GetBoxObjectFor(NS_STATIC_CAST(nsIDOMElement*, this), aResult);
}

void
nsHTMLSelectElement::RestoreStateTo(nsSelectState* aNewSelected)
{
  if (!mIsDoneAddingChildren) {
    mRestoreState = aNewSelected;
    NS_ADDREF(mRestoreState);
    return;
  }

  PRUint32 len;
  GetLength(&len);

  // First clear all
  SetOptionsSelectedByIndex(-1, -1, PR_TRUE, PR_TRUE, PR_TRUE, PR_TRUE, nsnull);

  // Next set the proper ones
  for (PRInt32 i = 0; i < (PRInt32)len; i++) {
    nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(i);
    if (option) {
      nsAutoString value;
      option->GetValue(value);
      if (aNewSelected->ContainsOption(i, value)) {
        SetOptionsSelectedByIndex(i, i, PR_TRUE, PR_FALSE, PR_TRUE, PR_TRUE, nsnull);
      }
    }
  }

  //CheckSelectSomething();
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
      PRBool selected = PR_FALSE;
      option->GetDefaultSelected(&selected);
      SetOptionsSelectedByIndex(i, i, selected,
                                PR_FALSE, PR_TRUE, PR_TRUE, nsnull);
      if (selected) {
        numSelected++;
      }
    }
  }

  //
  // If nothing was selected and it's not multiple, select something
  //
  PRInt32 size = 1;
  GetSize(&size);

  PRBool isMultiple = PR_FALSE;
  GetMultiple(&isMultiple);

  if (numSelected == 0 && !isMultiple && size <= 1) {
    SelectSomething();
  }

  //
  // Let the frame know we were reset
  //
  // Don't flush, if there's no frame yet it won't care about us being
  // reset even if we forced it to be created now.
  //
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
  if (formControlFrame) {
    formControlFrame->OnContentReset();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                       nsIContent* aSubmitElement)
{
  nsresult rv = NS_OK;

  //
  // Disabled elements don't submit
  //
  PRBool disabled;
  rv = GetDisabled(&disabled);
  if (NS_FAILED(rv) || disabled) {
    return rv;
  }

  //
  // Get the name (if no name, no submit)
  //
  nsAutoString name;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, name);
  if (NS_FAILED(rv) || rv == NS_CONTENT_ATTR_NOT_THERE) {
    return rv;
  }

  //
  // Submit
  //
  PRUint32 len;
  GetLength(&len);

  for (PRUint32 optIndex = 0; optIndex < len; optIndex++) {
    // Don't send disabled options
    PRBool disabled;
    rv = IsOptionDisabled(optIndex, &disabled);
    if (NS_FAILED(rv) || disabled) {
      continue;
    }

    nsIDOMHTMLOptionElement *option = mOptions->ItemAsOption(optIndex);
    NS_ENSURE_TRUE(option, NS_ERROR_UNEXPECTED);

    PRBool isSelected;
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

    rv = aFormSubmission->AddNameValuePair(this, name, value);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetHasOptGroups(PRBool* aHasGroups)
{
  *aHasGroups = (mOptGroupCount > 0);
  return NS_OK;
}

void
nsHTMLSelectElement::DispatchDOMEvent(const nsAString& aName)
{
  nsCOMPtr<nsIDOMDocumentEvent> domDoc = do_QueryInterface(mDocument);
  if (domDoc) {
    nsCOMPtr<nsIDOMEvent> selectEvent;
    domDoc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(selectEvent));
    if (selectEvent) {
      selectEvent->InitEvent(aName, PR_TRUE, PR_TRUE);
      nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(NS_STATIC_CAST(nsIDOMNode*, this)));
      PRBool noDefault;
      target->DispatchEvent(selectEvent, &noDefault);
    }
  }
}

//----------------------------------------------------------------------
//
// nsHTMLOptionCollection implementation
//

nsHTMLOptionCollection::nsHTMLOptionCollection(nsHTMLSelectElement* aSelect)
{
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

// nsISupports

// QueryInterface implementation for nsHTMLOptionCollection
NS_INTERFACE_MAP_BEGIN(nsHTMLOptionCollection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLOptionsCollection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLOptionCollection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNSHTMLOptionCollection)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLOptionsCollection)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF_INHERITED(nsHTMLOptionCollection, nsGenericDOMHTMLCollection)
NS_IMPL_RELEASE_INHERITED(nsHTMLOptionCollection, nsGenericDOMHTMLCollection)


// nsIDOMNSHTMLOptionCollection interface

NS_IMETHODIMP
nsHTMLOptionCollection::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Count();

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
nsHTMLOptionCollection::SetOption(PRInt32 aIndex,
                                  nsIDOMHTMLOptionElement *aOption)
{
  nsresult rv = NS_OK;

  // If the indx is within range
  if (mSelect && aIndex >= 0 && aIndex <= mElements.Count()) {
    // if the new option is null, remove this option
    if (!aOption) {
      mSelect->Remove(aIndex);

      // We're done.
      return NS_OK;
    }

    nsCOMPtr<nsIDOMNode> ret;
    if (aIndex == mElements.Count()) {
      rv = mSelect->AppendChild(aOption, getter_AddRefs(ret));
    } else {
      // Find the option they're talking about and replace it
      nsIDOMHTMLOptionElement *refChild = mElements.ObjectAt(aIndex);
      NS_ENSURE_TRUE(refChild, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsIDOMNode> parent;
      refChild->GetParentNode(getter_AddRefs(parent));
      if (parent) {
        rv = parent->ReplaceChild(aOption, refChild, getter_AddRefs(ret));
      }
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
  nsIDOMHTMLOptionElement *option = mElements.SafeObjectAt(aIndex);

  NS_IF_ADDREF(*aReturn = option);

  return NS_OK;
}

nsIDOMHTMLOptionElement *
nsHTMLOptionCollection::ItemAsOption(PRInt32 aIndex)
{
  return mElements.SafeObjectAt(aIndex);
}

NS_IMETHODIMP
nsHTMLOptionCollection::NamedItem(const nsAString& aName,
                                  nsIDOMNode** aReturn)
{
  PRInt32 count = mElements.Count();
  nsresult rv = NS_OK;

  *aReturn = nsnull;

  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mElements.ObjectAt(i));

    if (content) {
      nsAutoString name;

      if (((content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name,
                             name) == NS_CONTENT_ATTR_HAS_VALUE) &&
           aName.Equals(name)) ||
          ((content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id,
                             name) == NS_CONTENT_ATTR_HAS_VALUE) &&
           aName.Equals(name))) {
        rv = CallQueryInterface(content, aReturn);

        break;
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLOptionCollection::GetSelect(nsIDOMHTMLSelectElement **aReturn)
{
  NS_IF_ADDREF(*aReturn = mSelect);
  return NS_OK;
}

nsresult
nsHTMLOptionCollection::InsertElementAt(nsIDOMHTMLOptionElement* aOption,
                                        PRInt32 aIndex)
{
  return mElements.InsertObjectAt(aOption, aIndex);
}

nsresult
nsHTMLOptionCollection::RemoveElementAt(PRInt32 aIndex)
{
  return mElements.RemoveObjectAt(aIndex);
}
