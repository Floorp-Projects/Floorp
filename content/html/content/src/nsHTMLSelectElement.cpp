/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSHTMLSelectElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsITextContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"
#include "nsIForm.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIOptionElement.h"
#include "nsIEventStateManager.h"
#include "nsGenericDOMHTMLCollection.h"
#include "nsISelectElement.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMNSHTMLOptionCollectn.h"
#include "nsGUIEvent.h"

// PresState
#include "nsVoidArray.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIPresState.h"
#include "nsIComponentManager.h"

// Notify/query select frame for selectedIndex
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsIFrame.h"

#include "nsRuleNode.h"


class nsHTMLSelectElement;


// nsHTMLOptionCollection
class nsHTMLOptionCollection: public nsIDOMNSHTMLOptionCollection,
                              public nsGenericDOMHTMLCollection
{
public:
  nsHTMLOptionCollection(nsHTMLSelectElement* aSelect);
  virtual ~nsHTMLOptionCollection();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNSHTMLOptionCollection interface, can't use the macro
  // NS_DECL_NSIDOMNSHTMLOPTIONLIST here since GetLength() is defined
  // in mode than one interface
  NS_IMETHOD SetLength(PRUint32 aLength);
  NS_IMETHOD GetSelectedIndex(PRInt32* aSelectedIndex);
  NS_IMETHOD SetSelectedIndex(PRInt32 aSelectedIndex);
  NS_IMETHOD SetOption(PRInt32 aIndex, nsIDOMHTMLOptionElement* aOption);

  // nsIDOMHTMLCollection interface
  NS_DECL_NSIDOMHTMLCOLLECTION

  nsresult InsertElementAt(nsIDOMNode* aOption, PRInt32 aIndex);
  nsresult RemoveElementAt(PRInt32 aIndex);
  nsresult GetOption(PRInt32 aIndex, nsIDOMHTMLOptionElement** aReturn);
  PRInt32 IndexOf(nsIContent* aOption);
  nsresult ItemAsOption(PRInt32 aIndex, nsIDOMHTMLOptionElement **aReturn);

  void DropReference();

private:
  nsCOMPtr<nsISupportsArray> mElements;
  nsHTMLSelectElement* mSelect;
};


class nsHTMLSelectElement : public nsGenericHTMLContainerFormElement,
                            public nsIDOMHTMLSelectElement,
                            public nsIDOMNSHTMLSelectElement,
                            public nsISelectElement
{
public:
  nsHTMLSelectElement();
  virtual ~nsHTMLSelectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLSelectElement
  NS_DECL_NSIDOMHTMLSELECTELEMENT

  // nsIDOMNSHTMLSelectElement
  NS_DECL_NSIDOMNSHTMLSELECTELEMENT

  // nsIContent
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify, 
                           PRBool aDeepSetDocument);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument);
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);   

  // Overriden nsIFormControl methods
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Reset();
  NS_IMETHOD IsSuccessful(nsIContent* aSubmitElement, PRBool *_retval);
  NS_IMETHOD GetMaxNumValues(PRInt32 *_retval);
  NS_IMETHOD GetNamesValues(PRInt32 aMaxNumValues,
                            PRInt32& aNumValues,
                            nsString* aValues,
                            nsString* aNames);

  // nsISelectElement
  NS_IMETHOD WillAddOptions(nsIContent* aOptions,
                            nsIContent* aParent,
                            PRInt32 aContentIndex);
  NS_IMETHOD WillRemoveOptions(nsIContent* aParent,
                               PRInt32 aContentIndex);
  NS_IMETHOD AddOption(nsIContent* aContent);
  NS_IMETHOD RemoveOption(nsIContent* aContent);
  NS_IMETHOD DoneAddingContent(PRBool aIsDone);
  NS_IMETHOD IsDoneAddingContent(PRBool * aIsDone);
  NS_IMETHOD IsOptionSelected(nsIDOMHTMLOptionElement* anOption,
                              PRBool * aIsSelected);
  NS_IMETHOD SetOptionSelected(nsIDOMHTMLOptionElement* anOption,
                               PRBool aIsSelected);
  NS_IMETHOD SetOptionsSelectedByIndex(PRInt32 aStartIndex,
                                       PRInt32 aEndIndex,
                                       PRBool aIsSelected,
                                       PRBool aClearAll,
                                       PRBool aSetDisabled,
                                       PRBool* aChangedSomething);
  NS_IMETHOD IsOptionDisabled(PRInt32 aIndex, PRBool* aIsDisabled);
  NS_IMETHOD OnOptionDisabled(nsIDOMHTMLOptionElement* anOption);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32 aModType, PRInt32& aHint) const;


protected:
  // Helper Methods
  nsresult GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                          PRInt32 * anIndex);
  nsresult IsOptionSelectedByIndex(PRInt32 index, PRBool* aIsSelected);
  nsresult FindSelectedIndex(PRInt32 aStartIndex);
  nsresult SelectSomething();
  nsresult CheckSelectSomething();
  nsresult OnOptionSelected(nsISelectControlFrame* aSelectFrame,
                            nsIPresContext* aPresContext,
                            PRInt32 aIndex,
                            PRBool aSelected);
  nsresult InitializeOption(nsIDOMHTMLOptionElement* aOption,
                            PRUint32* aNumOptions);
  nsresult RestoreStateTo(nsAString* aNewSelected);

#ifdef DEBUG_john
  // Don't remove these, por favor.  They're very useful in debugging
  nsresult PrintOptions(nsIContent* aOptions, PRInt32 tabs);
#endif

  // Adding options
  nsresult InsertOptionsIntoList(nsIContent* aOptions,
                                 PRInt32 aListIndex,
                                 PRInt32 aLevel);
  nsresult RemoveOptionsFromList(nsIContent* aOptions,
                                 PRInt32 aListIndex,
                                 PRInt32 aLevel);
  nsresult InsertOptionsIntoListRecurse(nsIContent* aOptions,
                                        PRInt32* aInsertIndex,
                                        PRInt32 aLevel);
  nsresult RemoveOptionsFromListRecurse(nsIContent* aOptions,
                                        PRInt32 aRemoveIndex,
                                        PRInt32* aNumRemoved,
                                        PRInt32 aLevel);
  nsresult GetContentLevel(nsIContent* aContent, PRInt32* aLevel);
  nsresult GetOptionAt(nsIContent* aOptions, PRInt32* aListIndex);
  nsresult GetOptionAfter(nsIContent* aOptions, PRInt32* aListIndex);
  nsresult GetFirstOptionIndex(nsIContent* aOptions, PRInt32* aListIndex);
  nsresult GetFirstChildOptionIndex(nsIContent* aOptions,
                                    PRInt32 aStartIndex,
                                    PRInt32 aEndIndex,
                                    PRInt32* aListIndex);

  nsHTMLOptionCollection* mOptions;
  PRBool    mIsDoneAddingContent;
  PRUint32  mArtifactsAtTopLevel;
  PRInt32   mSelectedIndex;
  nsString* mRestoreState;
};


//----------------------------------------------------------------------
//
// nsHTMLSelectElement
//

// construction, destruction

nsresult
NS_NewHTMLSelectElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLSelectElement* it = new nsHTMLSelectElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_STATIC_CAST(nsGenericHTMLElement *, it)->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLSelectElement::nsHTMLSelectElement()
{
  mIsDoneAddingContent = PR_TRUE;
  mArtifactsAtTopLevel = 0;

  mOptions = new nsHTMLOptionCollection(this);
  NS_IF_ADDREF(mOptions);

  mRestoreState = nsnull;
  mSelectedIndex = -1;
}

nsHTMLSelectElement::~nsHTMLSelectElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
  if (mOptions) {
    mOptions->DropReference();
    NS_RELEASE(mOptions);
  }
  if (mRestoreState) {
    delete mRestoreState;
    mRestoreState = nsnull;
  }
}

// ISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLSelectElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLSelectElement, nsGenericElement);


// QueryInterface implementation for nsHTMLSelectElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLSelectElement,
                                    nsGenericHTMLContainerFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLSelectElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLSelectElement)
  NS_INTERFACE_MAP_ENTRY(nsISelectElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLSelectElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLSelectElement

nsresult
nsHTMLSelectElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLSelectElement* it = new nsHTMLSelectElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = NS_STATIC_CAST(nsGenericHTMLElement *, it)->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLContainerFormElement::GetForm(aForm);
}


// nsIContent
NS_IMETHODIMP
nsHTMLSelectElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                   PRBool aDeepSetDocument) 
{
  PRInt32 count = 0;
  ChildCount(count);
  WillAddOptions(aKid, this, count);

  // Actually perform the append
  return nsGenericHTMLContainerFormElement::AppendChildTo(aKid,
                                                          aNotify,
                                                          aDeepSetDocument);
}

NS_IMETHODIMP
nsHTMLSelectElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                                   PRBool aNotify, PRBool aDeepSetDocument) 
{
  WillAddOptions(aKid, this, aIndex);

  return nsGenericHTMLContainerFormElement::InsertChildAt(aKid,
                                                          aIndex,
                                                          aNotify,
                                                          aDeepSetDocument);
}

NS_IMETHODIMP
nsHTMLSelectElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                                    PRBool aNotify, PRBool aDeepSetDocument) 
{
  WillRemoveOptions(this, aIndex);
  WillAddOptions(aKid, this, aIndex);

  return nsGenericHTMLContainerFormElement::ReplaceChildAt(aKid,
                                                           aIndex,
                                                           aNotify,
                                                           aDeepSetDocument);
}

NS_IMETHODIMP
nsHTMLSelectElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  WillRemoveOptions(this, aIndex);

  return nsGenericHTMLContainerFormElement::RemoveChildAt(aIndex,
                                                          aNotify);
}


// SelectElement methods

nsresult
nsHTMLSelectElement::InsertOptionsIntoList(nsIContent* aOptions,
                                           PRInt32 aListIndex,
                                           PRInt32 aLevel)
{
  PRInt32 insertIndex = aListIndex;
  InsertOptionsIntoListRecurse(aOptions, &insertIndex, aLevel);

  // Deal with the selected list
  if (insertIndex - aListIndex) {
    // Fix the currently selected index
    if (aListIndex <= mSelectedIndex) {
      mSelectedIndex += (insertIndex - aListIndex);
    }

    // Get the frame stuff for notification
    nsIFormControlFrame* fcFrame = nsnull;
    nsISelectControlFrame* selectFrame = nsnull;
    nsCOMPtr<nsIPresContext> presContext;
    GetPrimaryFrame(this, fcFrame, PR_FALSE, PR_FALSE);
    if (fcFrame) {
      CallQueryInterface(fcFrame, &selectFrame);
      if (selectFrame) {
        GetPresContext(this, getter_AddRefs(presContext));
      }
    }

    // Actually select the options if the added options warrant it
    nsCOMPtr<nsIDOMNode> optionNode;
    nsCOMPtr<nsIOptionElement> option;
    for (PRInt32 i=aListIndex;i<insertIndex;i++) {
      // Notify the frame that the option is added
      if (selectFrame) {
        selectFrame->AddOption(presContext, i);
      }

      Item(i, getter_AddRefs(optionNode));
      option = do_QueryInterface(optionNode);
      if (option) {
        PRBool selected;
        option->GetSelectedInternal(&selected);
        if (selected) {
          // Clear all other options
          PRBool isMultiple;
          GetMultiple(&isMultiple);
          if (!isMultiple) {
            SetOptionsSelectedByIndex(i, i, PR_TRUE, PR_TRUE, PR_TRUE, nsnull);
          }

          // This is sort of a hack ... we need to notify that the option was
          // set and change selectedIndex even though we didn't really change
          // its value.
          OnOptionSelected(selectFrame, presContext, i, PR_TRUE);
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
  PRInt32 numChildren;
  aOptions->ChildCount(numChildren);
  nsCOMPtr<nsIContent> child;
  for (PRInt32 i=0;i<numChildren;i++) {
    aOptions->ChildAt(i,*getter_AddRefs(child));
    PrintOptions(child, tabs+1);
  }

  return NS_OK;
}
#endif

nsresult
nsHTMLSelectElement::RemoveOptionsFromList(nsIContent* aOptions,
                                           PRInt32 aListIndex,
                                           PRInt32 aLevel)
{
  PRInt32 numRemoved = 0;
  RemoveOptionsFromListRecurse(aOptions, aListIndex, &numRemoved, aLevel);
  if (numRemoved) {
    // Tell the widget we removed the options
    nsIFormControlFrame* fcFrame = nsnull;
    GetPrimaryFrame(this, fcFrame, PR_FALSE, PR_FALSE);
    if (fcFrame) {
      nsISelectControlFrame* selectFrame = nsnull;
      CallQueryInterface(fcFrame, &selectFrame);
      if (selectFrame) {
        nsCOMPtr<nsIPresContext> presContext;
        GetPresContext(this, getter_AddRefs(presContext));
        for (int i=aListIndex;i<aListIndex+numRemoved;i++) {
          selectFrame->RemoveOption(presContext, i);
        }
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

// If the document is such that recursing over these options gets us
// deeper than four levels, there is something terribly wrong with the
// world.
nsresult
nsHTMLSelectElement::InsertOptionsIntoListRecurse(nsIContent* aOptions,
                                                  PRInt32* aInsertIndex,
                                                  PRInt32 aLevel)
{
  // We *assume* here that someone's brain has not gone horribly
  // wrong by putting <option> inside of <option>.  I'm sorry, I'm
  // just not going to look for an option inside of an option.
  // Sue me.

  nsCOMPtr<nsIDOMHTMLOptionElement> optElement(do_QueryInterface(aOptions));
  if (optElement) {
    nsCOMPtr<nsIDOMNode> optNode(do_QueryInterface(optElement));
    mOptions->InsertElementAt(optNode, *aInsertIndex);
    (*aInsertIndex)++;
    return NS_OK;
  }

  // If it's at the top level, then we just found out there are non-options
  // at the top level, which will throw off the insert count
  if (aLevel == 0) {
    mArtifactsAtTopLevel++;
  }

  // Recurse down into optgroups
  //
  // I *would* put a restriction in here to only search under
  // optgroups (and not, for example, <P></P>), but it really
  // doesn't *hurt* to search under other stuff and it's more
  // efficient in the normal only-optgroup-and-option case
  // (one less QueryInterface).
  PRInt32 numChildren;
  aOptions->ChildCount(numChildren);
  nsCOMPtr<nsIContent> child;
  for (PRInt32 i=0;i<numChildren;i++) {
    aOptions->ChildAt(i,*getter_AddRefs(child));
    InsertOptionsIntoListRecurse(child, aInsertIndex, aLevel+1);
  }

  return NS_OK;
}

// If the document is such that recursing over these options gets us deeper than
// four levels, there is something terribly wrong with the world.
nsresult
nsHTMLSelectElement::RemoveOptionsFromListRecurse(nsIContent* aOptions,
                                                  PRInt32 aRemoveIndex,
                                                  PRInt32* aNumRemoved,
                                                  PRInt32 aLevel) {
  // We *assume* here that someone's brain has not gone horribly
  // wrong by putting <option> inside of <option>.  I'm sorry, I'm
  // just not going to look for an option inside of an option.
  // Sue me.

  nsCOMPtr<nsIDOMHTMLOptionElement> optElement(do_QueryInterface(aOptions));
  if (optElement) {
    mOptions->RemoveElementAt(aRemoveIndex);
    (*aNumRemoved)++;
    return NS_OK;
  }

  // Yay, one less artifact at the top level.
  if (aLevel == 0) {
    mArtifactsAtTopLevel--;
  }

  // Recurse down deeper for options
  //
  // I *would* put a restriction in here to only search under
  // optgroups (and not, for example, <P></P>), but it really
  // doesn't *hurt* to search under other stuff and it's more
  // efficient in the normal only-optgroup-and-option case
  // (one less QueryInterface).
  PRInt32 numChildren;
  aOptions->ChildCount(numChildren);
  nsCOMPtr<nsIContent> child;
  for (PRInt32 i=0;i<numChildren;i++) {
    aOptions->ChildAt(i,*getter_AddRefs(child));
    RemoveOptionsFromListRecurse(child, aRemoveIndex, aNumRemoved, aLevel+1);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::WillAddOptions(nsIContent* aOptions,
                                    nsIContent* aParent,
                                    PRInt32 aContentIndex)
{
  PRInt32 level;
  GetContentLevel(aParent, &level);
  if (level == -1) {
    return NS_ERROR_FAILURE;
  }

  // Get the index where the options will be inserted
  PRInt32 ind = -1;
  PRInt32 children;
  aParent->ChildCount(children);
  if (aContentIndex >= children) {
    GetOptionAfter(aParent, &ind);
  } else {
    nsCOMPtr<nsIContent> currentKid;
    aParent->ChildAt(aContentIndex, *getter_AddRefs(currentKid));
    NS_ASSERTION(currentKid, "Child not found!");
    if (currentKid) {
      GetOptionAt(currentKid, &ind);
    }
  }

  InsertOptionsIntoList(aOptions, ind, level);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::WillRemoveOptions(nsIContent* aParent,
                                       PRInt32 aContentIndex)
{
  PRInt32 level;
  GetContentLevel(aParent, &level);
  if (level == -1) {
    return NS_ERROR_FAILURE;
  }

  // Get the index where the options will be removed
  nsCOMPtr<nsIContent> currentKid;
  aParent->ChildAt(aContentIndex, *getter_AddRefs(currentKid));
  if (currentKid) {
    PRInt32 ind = -1;
    GetFirstOptionIndex(currentKid, &ind);
    if (ind != -1) {
      RemoveOptionsFromList(currentKid, ind, level);
    }
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetContentLevel(nsIContent* aContent, PRInt32* aLevel)
{
  nsCOMPtr<nsIContent> content = aContent;
  nsCOMPtr<nsIContent> prevContent;

  *aLevel = 0;
  while (content != this) {
    (*aLevel)++;
    prevContent = content;
    prevContent->GetParent(*getter_AddRefs(content));
    if (!content) {
      *aLevel = -1;
      break;
    }
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetOptionAt(nsIContent* aOptions, PRInt32* aListIndex)
{
  // Search this node and below.
  // If not found, find the first one *after* this node.
  GetFirstOptionIndex(aOptions, aListIndex);
  if (*aListIndex == -1) {
    GetOptionAfter(aOptions, aListIndex);
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetOptionAfter(nsIContent* aOptions, PRInt32* aListIndex)
{
  // - If this is the select, the next option is the last.
  // - If not, search all the options after aOptions and up to the last option
  //   in the parent.
  // - If it's not there, search for the first option after the parent.
  if (aOptions == this) {
    PRUint32 len;
    GetLength(&len);
    *aListIndex = len;
  } else {
    nsCOMPtr<nsIContent> parent;
    aOptions->GetParent(*getter_AddRefs(parent));

    if (parent) {
      PRInt32 index;
      PRInt32 count;
      parent->IndexOf(aOptions, index);
      parent->ChildCount(count);

      GetFirstChildOptionIndex(parent, index+1, count, aListIndex);

      if ((*aListIndex) == -1) {
        GetOptionAfter(parent, aListIndex);
      }
    }
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetFirstOptionIndex(nsIContent* aOptions,
                                         PRInt32* aListIndex)
{
  nsCOMPtr<nsIDOMHTMLOptionElement> optElement(do_QueryInterface(aOptions));
  if (optElement) {
    GetOptionIndex(optElement, aListIndex);
    // If you nested stuff under the option, you're just plain
    // screwed.  *I'm* not going to aid and abet your evil deed.
    return NS_OK;
  }

  PRInt32 numChildren;
  aOptions->ChildCount(numChildren);
  GetFirstChildOptionIndex(aOptions, 0, numChildren, aListIndex);

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetFirstChildOptionIndex(nsIContent* aOptions,
                                              PRInt32 aStartIndex,
                                              PRInt32 aEndIndex,
                                              PRInt32* aListIndex)
{
  nsCOMPtr<nsIContent> child;
  for (PRInt32 i=aStartIndex;i<aEndIndex;i++) {
    aOptions->ChildAt(i,*getter_AddRefs(child));
    GetFirstOptionIndex(child, aListIndex);
    if ((*aListIndex) != -1) {
      return NS_OK;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLSelectElement::Add(nsIDOMHTMLElement* aElement,
                         nsIDOMHTMLElement* aBefore)
{
  nsresult rv;
  nsCOMPtr<nsIDOMNode> ret;

  if (nsnull == aBefore) {
    rv = AppendChild(aElement, getter_AddRefs(ret));
  }
  else {
    // Just in case we're not the parent, get the parent of the reference
    // element
    nsCOMPtr<nsIDOMNode> parent;
    
    rv = aBefore->GetParentNode(getter_AddRefs(parent));
    if (parent) {
      rv = parent->InsertBefore(aElement, aBefore, getter_AddRefs(ret));
    }
  }

  return rv;
}

NS_IMETHODIMP 
nsHTMLSelectElement::Remove(PRInt32 aIndex) 
{
  nsresult rv = NS_OK;
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

  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptions(nsIDOMHTMLCollection** aValue)
{
  *aValue = mOptions;
  NS_IF_ADDREF(*aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetType(nsAWritableString& aType)
{
  PRBool isMultiple;
  nsresult rv = NS_OK;

  rv = GetMultiple(&isMultiple);
  if (NS_OK == rv) {
    if (isMultiple) {
      aType.Assign(NS_LITERAL_STRING("select-multiple"));
    }
    else {
      aType.Assign(NS_LITERAL_STRING("select-one"));
    }
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
    nsCOMPtr<nsIHTMLContent> element;
    nsCOMPtr<nsINodeInfo> nodeInfo;

    mNodeInfo->NameChanged(nsHTMLAtoms::option, *getter_AddRefs(nodeInfo));

    rv = NS_NewHTMLOptionElement(getter_AddRefs(element), nodeInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> text;
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
                                   PR_FALSE, PR_TRUE, nsnull);
}

nsresult
nsHTMLSelectElement::GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                                    PRInt32 * anIndex)
{
  NS_ENSURE_ARG_POINTER(anIndex);

  PRUint32 numOptions;

  nsresult rv = GetLength(&numOptions);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMNode> node;

  for (PRUint32 i = 0; i < numOptions; i++) {
    rv = Item(i, getter_AddRefs(node));
    if (NS_SUCCEEDED(rv) && node) {
      nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));
      if (option && option.get() == aOption) {
        *anIndex = i;
        return NS_OK;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLSelectElement::IsOptionSelected(nsIDOMHTMLOptionElement* aOption,
                                      PRBool * aIsSelected)
{
  // start off by assuming it isn't in the list of index objects
  *aIsSelected = PR_FALSE;

  // first find the index of the incoming option
  PRInt32 index = -1;
  if (NS_FAILED(GetOptionIndex(aOption, &index))) {
    return NS_ERROR_FAILURE;
  }

  return IsOptionSelectedByIndex(index, aIsSelected);
}

nsresult
nsHTMLSelectElement::IsOptionSelectedByIndex(PRInt32 index,
                                             PRBool * aIsSelected)
{
  nsCOMPtr<nsIDOMHTMLOptionElement> option;
  mOptions->ItemAsOption(index, getter_AddRefs(option));
  if (option) {
    return option->GetSelected(aIsSelected);
  } else {
    return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLSelectElement::OnOptionDisabled(nsIDOMHTMLOptionElement* anOption)
{
  return NS_OK;
}

nsresult
nsHTMLSelectElement::OnOptionSelected(nsISelectControlFrame* aSelectFrame,
                                      nsIPresContext* aPresContext,
                                      PRInt32 aIndex,
                                      PRBool aSelected)
{
  //printf("OnOptionSelected(%d = '%c')\n", aIndex, (aSelected ? 'Y' : 'N'));
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
    optionElement->SetSelectedInternal(aSelected, PR_TRUE);
  }

  // Let the frame know too
  if (aSelectFrame) {
    aSelectFrame->OnOptionSelected(aPresContext, aIndex, aSelected);
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::FindSelectedIndex(PRInt32 aStartIndex)
{
  mSelectedIndex = -1;
  PRUint32 len;
  GetLength(&len);
  for (PRInt32 i=aStartIndex; i<(PRInt32)len; i++) {
    PRBool isSelected;
    IsOptionSelectedByIndex(i, &isSelected);
    if (isSelected) {
      mSelectedIndex = i;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetOptionSelected(nsIDOMHTMLOptionElement* anOption,
                                       PRBool aIsSelected)
{
  PRInt32 index;

  nsresult rv = GetOptionIndex(anOption, &index);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return SetOptionsSelectedByIndex(index, index, aIsSelected,
                                   PR_FALSE, PR_TRUE, nsnull);
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

  // To notify the frame if anything gets changed
  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_TRUE, PR_FALSE);

  nsCOMPtr<nsISelectControlFrame> selectFrame =
    do_QueryInterface(formControlFrame);

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

        nsCOMPtr<nsIDOMHTMLOptionElement> option;
        mOptions->ItemAsOption(optIndex, getter_AddRefs(option));
        if (option) {
          // If the index is already selected, ignore it.
          PRBool isSelected = PR_FALSE;
          option->GetSelected(&isSelected);
          if (!isSelected) {
            OnOptionSelected(selectFrame, presContext, optIndex, PR_TRUE);
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
          nsCOMPtr<nsIDOMHTMLOptionElement> option;
          mOptions->ItemAsOption(optIndex, getter_AddRefs(option));
          if (option) {
            // If the index is already selected, ignore it.
            PRBool isSelected = PR_FALSE;
            option->GetSelected(&isSelected);
            if (isSelected) {
              OnOptionSelected(selectFrame, presContext, optIndex, PR_FALSE);
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

      nsCOMPtr<nsIDOMHTMLOptionElement> option;
      mOptions->ItemAsOption(optIndex, getter_AddRefs(option));
      if (option) {
        // If the index is already selected, ignore it.
        PRBool isSelected = PR_FALSE;
        option->GetSelected(&isSelected);
        if (isSelected) {
          OnOptionSelected(selectFrame, presContext, optIndex, PR_FALSE);
          optionsDeselected = PR_TRUE;
        }
      }
    }
  }

  // Make sure something is selected
  if (optionsDeselected) {
    CheckSelectSomething();
  }

  // Let the caller know whether anything was changed
  if (aChangedSomething && (optionsSelected || optionsDeselected)) {
    *aChangedSomething = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::IsOptionDisabled(PRInt32 aIndex, PRBool* aIsDisabled)
{
  *aIsDisabled = PR_FALSE;
  nsCOMPtr<nsIDOMNode> optionNode;
  Item(aIndex, getter_AddRefs(optionNode));
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
  if (mArtifactsAtTopLevel) {
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
nsHTMLSelectElement::GetValue(nsAWritableString& aValue)
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
nsHTMLSelectElement::SetValue(const nsAReadableString& aValue)
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
  return SetElementFocus(PR_FALSE);
}

NS_IMETHODIMP
nsHTMLSelectElement::Focus()
{
  return SetElementFocus(PR_TRUE);
}

NS_IMETHODIMP
nsHTMLSelectElement::SetFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;

  if (NS_CONTENT_ATTR_HAS_VALUE ==
      nsGenericHTMLContainerFormElement::GetAttr(kNameSpaceID_HTML,
                                                 nsHTMLAtoms::disabled,
                                                 disabled)) {
    return NS_OK;
  }

  nsCOMPtr<nsIEventStateManager> esm;

  aPresContext->GetEventStateManager(getter_AddRefs(esm));

  if (esm) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
  }

  nsIFormControlFrame* formControlFrame = nsnull;

  GetPrimaryFrame(this, formControlFrame, PR_TRUE, PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    formControlFrame->ScrollIntoView(aPresContext);
    // Could call SelectAll(aPresContext) here to automatically
    // select text when we receive focus.
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::RemoveFocus(nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  // If we are disabled, we probably shouldn't have focus in the
  // first place, so allow it to be removed.
  nsresult rv = NS_OK;

  nsIFormControlFrame* formControlFrame = nsnull;

  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_FALSE, PR_FALSE);
  }

  nsCOMPtr<nsIEventStateManager> esm;
  aPresContext->GetEventStateManager(getter_AddRefs(esm));

  if (esm) {
    nsCOMPtr<nsIDocument> doc;

    GetDocument(*getter_AddRefs(doc));

    if (!doc) {
      return NS_ERROR_NULL_POINTER;
    }

    nsCOMPtr<nsIContent> rootContent;
    doc->GetRootContent(getter_AddRefs(rootContent));

    rv = esm->SetContentState(rootContent, NS_EVENT_STATE_FOCUS);
  }

  return rv;
}

NS_IMETHODIMP 
nsHTMLSelectElement::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  return mOptions->Item(aIndex, aReturn);
}

NS_IMETHODIMP 
nsHTMLSelectElement::NamedItem(const nsAReadableString& aName,
                               nsIDOMNode** aReturn)
{
  return mOptions->NamedItem(aName, aReturn);
}

nsresult
nsHTMLSelectElement::CheckSelectSomething()
{
  if (mIsDoneAddingContent) {
    PRInt32 size = 1;
    GetSize(&size);
    PRBool isMultiple;
    GetMultiple(&isMultiple);
    if (mSelectedIndex < 0 && !isMultiple && size <= 1) {
      SelectSomething();
    }
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::SelectSomething()
{
  // If we're not done building the select, don't play with this yet.
  if (!mIsDoneAddingContent) {
    return NS_OK;
  }

  // Don't select anything if we're disabled
  PRBool isDisabled = PR_FALSE;
  GetDisabled(&isDisabled);
  if (isDisabled) {
    return NS_OK;
  }

  PRUint32 count;
  GetLength(&count);
  for (PRUint32 i=0; i<count; i++) {
    nsCOMPtr<nsIDOMNode> optionNode;
    Item(i, getter_AddRefs(optionNode));
    nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(optionNode);
    if (option) {
      PRBool disabled;
      nsresult rv = option->GetDisabled(&disabled);

      if (NS_FAILED(rv) || !disabled) {
        SetSelectedIndex(i);
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::AddOption(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMHTMLElement> domElement(do_QueryInterface(aContent));
  return Add(domElement, nsnull);
}

NS_IMETHODIMP 
nsHTMLSelectElement::RemoveOption(nsIContent* aContent)
{
  // XXX We're *trusting* that this content is actually a child
  // of the select.  Bad things may happen if not.
  nsCOMPtr<nsIDOMNode> ret;
  nsCOMPtr<nsIDOMNode> toRemove(do_QueryInterface(aContent));
  return RemoveChild(toRemove, getter_AddRefs(ret));
}

NS_IMETHODIMP
nsHTMLSelectElement::IsDoneAddingContent(PRBool * aIsDone)
{
  *aIsDone = mIsDoneAddingContent;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::DoneAddingContent(PRBool aIsDone)
{
  mIsDoneAddingContent = aIsDone;

  nsIFormControlFrame* fcFrame = nsnull;
  GetPrimaryFrame(this, fcFrame, PR_FALSE, PR_FALSE);

  // If we foolishly tried to restore before we were done adding
  // content, restore the rest of the options proper-like
  if (mIsDoneAddingContent && mRestoreState) {
    RestoreStateTo(mRestoreState);
    delete mRestoreState;
    mRestoreState = nsnull;
  }

  if (fcFrame) {
    nsISelectControlFrame* selectFrame = nsnull;
    CallQueryInterface(fcFrame, &selectFrame);
    if (selectFrame) {
      selectFrame->DoneAddingContent(mIsDoneAddingContent);
    }
  }

  // Now that we're done, select something
  CheckSelectSomething();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAReadableString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::multiple) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

static void
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (!aData || !aAttributes)
    return;

  nsGenericHTMLElement::MapAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32 aModType,
                                              PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::multiple) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if ((aAttribute == nsHTMLAtoms::align) ||
           (aAttribute == nsHTMLAtoms::size)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;

  return NS_OK;
}


NS_IMETHODIMP
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

  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_FALSE, PR_FALSE);
  nsIFrame* formFrame = nsnull;

  if (formControlFrame &&
      NS_SUCCEEDED(formControlFrame->QueryInterface(NS_GET_IID(nsIFrame),
                                                    (void **)&formFrame)) &&
      formFrame)
  {
    const nsStyleUserInterface* uiStyle;
    formFrame->GetStyleData(eStyleStruct_UserInterface,
                            (const nsStyleStruct *&)uiStyle);

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
      !(aFlags & NS_EVENT_FLAG_CAPTURE) &&
      (aEvent->message == NS_BLUR_CONTENT) && formControlFrame) {
    formControlFrame->SetFocus(PR_FALSE, PR_TRUE);
  }

  return nsGenericHTMLContainerFormElement::HandleDOMEvent(aPresContext,
                                                           aEvent, aDOMEvent,
                                                           aFlags,
                                                           aEventStatus);
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLSelectElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = NS_FORM_SELECT;
    return NS_OK;
  }

  return NS_FORM_NOTOK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SaveState(nsIPresContext* aPresContext,
                               nsIPresState** aState)
{
  nsAutoString stateStr;

  PRUint32 len;
  GetLength(&len);

  for (PRUint32 optIndex = 0; optIndex < len; optIndex++) {
    nsCOMPtr<nsIDOMHTMLOptionElement> option;
    mOptions->ItemAsOption(optIndex, getter_AddRefs(option));
    if (option) {
      PRBool isSelected;
      option->GetSelected(&isSelected);
      if (isSelected) {
        if (!stateStr.IsEmpty()) {
          stateStr.Append(PRUnichar(','));
        }
        stateStr.AppendInt(optIndex);
      }
    }
  }

  nsresult rv = GetPrimaryPresState(this, aState);
  if (*aState) {
    rv = (*aState)->SetStateProperty(NS_LITERAL_STRING("selecteditems"),
                                     stateStr);
    NS_ASSERTION(NS_SUCCEEDED(rv), "selecteditems set failed!");
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::RestoreState(nsIPresContext* aPresContext,
                                  nsIPresState* aState)
{
  // XXX This works right now, but since this is only called from
  // RestoreState() in the frame, this will happen at the first frame
  // creation.  If JavaScript makes changes before then, and the page
  // is being reloaded, these changes will be lost.
  //
  // If RestoreState() is called a second time after SaveState() was
  // called, this will do nothing.

  // Get the presentation state object to retrieve our stuff out of.
  nsAutoString stateStr;
  nsresult rv = aState->GetStateProperty(NS_LITERAL_STRING("selecteditems"),
                                         stateStr);
  if (NS_SUCCEEDED(rv)) {
    RestoreStateTo(&stateStr);

    nsIFormControlFrame* formControlFrame = nsnull;
    GetPrimaryFrame(this, formControlFrame, PR_TRUE, PR_FALSE);
    if (formControlFrame) {
      formControlFrame->OnContentReset();
    }
  }

  return rv;
}

nsresult
nsHTMLSelectElement::RestoreStateTo(nsAString* aNewSelected)
{
  if (!mIsDoneAddingContent) {
    mRestoreState = new nsString;
    if (!mRestoreState) {
      return NS_OK;
    }
    *mRestoreState = *aNewSelected;
    return NS_OK;
  }

  PRUint32 len;
  GetLength(&len);

  // First clear all
  SetOptionsSelectedByIndex(-1, -1, PR_TRUE, PR_TRUE, PR_TRUE, nsnull);

  // Next set the proper ones
  PRUint32 currentInd = 0;
  while (currentInd < aNewSelected->Length()) {
    PRInt32 nextInd = aNewSelected->FindChar(',', currentInd);
    if (nextInd == -1) {
      nextInd = aNewSelected->Length();
    }
    nsDependentSubstring s = Substring(*aNewSelected,
                                       currentInd, (nextInd-currentInd));
    PRInt32 i = atoi(NS_ConvertUCS2toUTF8(s).get());
    SetOptionsSelectedByIndex(i, i, PR_TRUE, PR_FALSE, PR_TRUE, nsnull);
    currentInd = (PRUint32)nextInd+1;
  }

  //CheckSelectSomething();

  return NS_OK;
}

nsresult
nsHTMLSelectElement::Reset()
{
  PRBool isMultiple;
  nsresult rv = GetMultiple(&isMultiple);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 count = 0;

  PRUint32 numOptions;
  rv = GetLength(&numOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < numOptions; i++) {
    nsCOMPtr<nsIDOMNode> node;
    rv = Item(i, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));

    NS_ASSERTION(option, "option not an OptionElement");
    if (option) {
      InitializeOption(option, &count);
    }
  }

  PRInt32 size = 1;
  GetSize(&size);

  if (count == 0 && !isMultiple && size <= 1) {
    SelectSomething();
  }

  nsIFormControlFrame* formControlFrame = nsnull;
  GetPrimaryFrame(this, formControlFrame, PR_TRUE, PR_FALSE);
  if (formControlFrame) {
    formControlFrame->OnContentReset();
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::InitializeOption(nsIDOMHTMLOptionElement * aOption,
                                      PRUint32* aNumOptions)
{
  PRBool selected;
  nsresult rv = aOption->GetDefaultSelected(&selected);
  if (!NS_SUCCEEDED(rv)) {
    selected = PR_FALSE;
  }
  SetOptionSelected(aOption, selected);
  if (selected) {
    (*aNumOptions)++;
  }

  return NS_OK;
}

// Since this is multivalued, IsSuccessful here only really says whether
// we're *allowed* to submit options here.  There still may be no successful
// options and nothing will submit.
nsresult
nsHTMLSelectElement::IsSuccessful(nsIContent* aSubmitElement,
                                  PRBool *_retval)
{
  // if it's disabled, it won't submit
  PRBool disabled;
  nsresult rv = GetDisabled(&disabled);
  NS_ENSURE_SUCCESS(rv, rv);

  if (disabled) {
    *_retval = PR_FALSE;
    return NS_OK;
  }

  // If there is no name, it won't submit
  nsAutoString val;
  rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, val);
  *_retval = rv != NS_CONTENT_ATTR_NOT_THERE;

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetMaxNumValues(PRInt32 *_retval)
{
  PRUint32 length;
  nsresult rv = GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = length;

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetNamesValues(PRInt32 aMaxNumValues,
                                    PRInt32& aNumValues,
                                    nsString* aValues,
                                    nsString* aNames)
{
  nsAutoString name;
  nsresult rv = GetName(name);

  PRInt32 sentOptions = 0;

  PRUint32 len;
  GetLength(&len);

  // We loop through the cached list of selected items because it's
  // hella faster than looping through all options
  for (PRUint32 optIndex = 0; optIndex < len; optIndex++) {
    // Don't send disabled options
    PRBool disabled;
    rv = IsOptionDisabled(optIndex, &disabled);
    if (NS_FAILED(rv) || disabled) {
      continue;
    }

    nsCOMPtr<nsIDOMHTMLOptionElement> option;
    mOptions->ItemAsOption(optIndex, getter_AddRefs(option));
    if (option) {
      PRBool isSelected;
      rv = option->GetSelected(&isSelected);
      if (NS_FAILED(rv) || !isSelected) {
        continue;
      }

      nsCOMPtr<nsIOptionElement> optionElement = do_QueryInterface(option);
      if (optionElement) {
        nsAutoString value;
        rv = optionElement->GetValueOrText(value);
        NS_ENSURE_SUCCESS(rv, rv);

        // If the value is not there
        if (sentOptions < aMaxNumValues) {
          aNames[sentOptions] = name;
          aValues[sentOptions] = value;
          sentOptions++;
        }
      }
    }
  }

  aNumValues = sentOptions;

  return NS_OK;
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
  // Create the option array
  NS_NewISupportsArray(getter_AddRefs(mElements));
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
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLOptionCollection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNSHTMLOptionCollection)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLOptionCollection)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF_INHERITED(nsHTMLOptionCollection, nsGenericDOMHTMLCollection)
NS_IMPL_RELEASE_INHERITED(nsHTMLOptionCollection, nsGenericDOMHTMLCollection)


// nsIDOMNSHTMLOptionCollection interface

NS_IMETHODIMP    
nsHTMLOptionCollection::GetLength(PRUint32* aLength)
{
  return mElements->Count(aLength);
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetLength(PRUint32 aLength)
{
  nsresult rv = NS_ERROR_UNEXPECTED;

  if (mSelect) {
    rv = mSelect->SetLength(aLength);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetOption(PRInt32 aIndex,
                                  nsIDOMHTMLOptionElement *aOption)
{
  if (!mSelect) {
    return NS_OK;
  }

  PRUint32 length;
  nsresult rv = mElements->Count(&length);

  // If the indx is within range
  if ((aIndex >= 0) && (aIndex <= (PRInt32)length)) {
    // if the new option is null, remove this option
    if (!aOption) {
      mSelect->Remove(aIndex);

      // We're done.
      return NS_OK;
    }

    nsCOMPtr<nsIDOMNode> ret;
    if (aIndex == (PRInt32)length) {
      rv = mSelect->AppendChild(aOption, getter_AddRefs(ret));
    } else {
      // Find the option they're talking about and replace it
      nsCOMPtr<nsIDOMNode> refChild;
      rv = mElements->QueryElementAt(aIndex,
                                     NS_GET_IID(nsIDOMNode),
                                     getter_AddRefs(refChild));
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
  *aReturn = nsnull;

  PRUint32 length = 0;
  GetLength(&length);

  nsresult rv = NS_OK;
  if (aIndex < length) {
    rv = mElements->QueryElementAt(aIndex,
                                   NS_GET_IID(nsIDOMNode),
                                   (void**)aReturn);
  }

  return rv;
}

nsresult
nsHTMLOptionCollection::ItemAsOption(PRInt32 aIndex,
                                     nsIDOMHTMLOptionElement **aReturn)
{
  *aReturn = nsnull;

  PRUint32 length = 0;
  GetLength(&length);

  nsresult rv = NS_OK;
  if (aIndex < (PRInt32)length) {
    rv = mElements->QueryElementAt(aIndex,
                                   NS_GET_IID(nsIDOMHTMLOptionElement),
                                   (void**)aReturn);
  }

  return rv;
}

NS_IMETHODIMP 
nsHTMLOptionCollection::NamedItem(const nsAReadableString& aName,
                                  nsIDOMNode** aReturn)
{
  PRUint32 count;
  nsresult rv = mElements->Count(&count);

  *aReturn = nsnull;
  for (PRUint32 i = 0; i < count && !*aReturn; i++) {
    nsCOMPtr<nsIContent> content;
    rv = mElements->QueryElementAt(i,
                                   NS_GET_IID(nsIContent),
                                   getter_AddRefs(content));

    if (content) {
      nsAutoString name;

      if (((content->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::name,
                             name) == NS_CONTENT_ATTR_HAS_VALUE) &&
           (aName.Equals(name))) ||
          ((content->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::id,
                             name) == NS_CONTENT_ATTR_HAS_VALUE) &&
           (aName.Equals(name)))) {
        rv = CallQueryInterface(content, aReturn);
      }
    }
  }
  
  return rv;
}

nsresult
nsHTMLOptionCollection::InsertElementAt(nsIDOMNode* aOption, PRInt32 aIndex)
{
  return mElements->InsertElementAt(aOption, aIndex);
}

nsresult
nsHTMLOptionCollection::RemoveElementAt(PRInt32 aIndex)
{
  return mElements->RemoveElementAt(aIndex);
}

PRInt32
nsHTMLOptionCollection::IndexOf(nsIContent* aOption)
{
  // This (hopefully not incorrectly) ASSUMES that casting
  // nsIDOMHTMLOptionElement to nsIContent would return the same pointer
  // (comparable with ==).
  if (!aOption) {
    return -1;
  }

  return mElements->IndexOf(aOption);
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLSelectElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
