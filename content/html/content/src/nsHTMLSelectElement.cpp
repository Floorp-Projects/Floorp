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
#include "nsIEventStateManager.h"
#include "nsGenericDOMHTMLCollection.h"
#include "nsISelectElement.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMNSHTMLOptionCollectn.h"
#include "nsGUIEvent.h"

// PresState
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
  NS_IMETHOD GetSelectedIndex(PRInt32 *aSelectedIndex);
  NS_IMETHOD SetSelectedIndex(PRInt32 aSelectedIndex);
  NS_IMETHOD SetOption(PRInt32 aIndex, nsIDOMHTMLOptionElement *aOption);

  // nsIDOMHTMLCollection interface
  NS_DECL_NSIDOMHTMLCOLLECTION

  void AddOption(nsIContent* aOption);
  void AddOptionIndexed(nsIContent* aOption, PRInt32 aIndex);
  void RemoveOption(nsIContent* aOption);
  PRInt32 IndexOf(nsIContent* aOption);

  void Clear();
  void DropReference();

  void GetOptions();

private:
  nsVoidArray mElements;
  PRPackedBool mDirty;
  PRPackedBool mFlat;
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
  NS_IMETHOD Init();

  // nsISelectElement
  NS_IMETHOD AddOption(nsIContent* aContent);
  NS_IMETHOD RemoveOption(nsIContent* aContent);
  NS_IMETHOD DoneAddingContent(PRBool aIsDone);
  NS_IMETHOD IsDoneAddingContent(PRBool * aIsDone);
  NS_IMETHOD IsOptionSelected(nsIDOMHTMLOptionElement* anOption,
                              PRBool * aIsSelected);
  NS_IMETHOD SetOptionSelected(nsIDOMHTMLOptionElement* anOption,
                               PRBool aIsSelected);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;

  NS_IMETHOD AddOptionIndexed(nsIContent* aContent, PRInt32 aIndex);

protected:
  // Helper Methods
  nsresult GetPresState(nsIPresState** aPresState,
                        nsISupportsArray** aValueArray);
  nsresult GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                          PRUint32 * anIndex);
  nsresult SetOptionSelectedByIndex(PRInt32 aIndex, PRBool aIsSelected);

  nsHTMLOptionCollection* mOptions;
  PRBool        mIsDoneAddingContent;
};


// nsHTMLSelectElement

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
  mOptions = nsnull;
  mIsDoneAddingContent = PR_TRUE;
}

nsHTMLSelectElement::~nsHTMLSelectElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
  if (mOptions) {
    mOptions->Clear();
    mOptions->DropReference();
    NS_RELEASE(mOptions);
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
  nsresult res = nsGenericHTMLContainerFormElement::AppendChildTo(aKid,
                                                                  aNotify,
                                                                  aDeepSetDocument);
  if (NS_SUCCEEDED(res)) {
    PRInt32 numChildren;
    ChildCount(numChildren);

    // this is effectively where we added it.  May not be the same as
    // mOptions->GetLength if there are <optgroup>s
    res = AddOptionIndexed(aKid, numChildren-1);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLSelectElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                                   PRBool aNotify, PRBool aDeepSetDocument) 
{
  nsresult res = nsGenericHTMLContainerFormElement::InsertChildAt(aKid, aIndex,
                                                                  aNotify,
                                                                  aDeepSetDocument);
  if (NS_SUCCEEDED(res)) {
    res = AddOptionIndexed(aKid, aIndex); 
  }

  return res;
}

NS_IMETHODIMP
nsHTMLSelectElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                                    PRBool aNotify, PRBool aDeepSetDocument) 
{
  nsCOMPtr<nsIContent> content;
  if (NS_SUCCEEDED(ChildAt(aIndex, *getter_AddRefs(content)))) {
    RemoveOption(content);
  }

  nsresult res = nsGenericHTMLContainerFormElement::ReplaceChildAt(aKid,
                                                                   aIndex,
                                                                   aNotify,
                                                                   aDeepSetDocument);

  if (NS_SUCCEEDED(res)) {
    res = AddOptionIndexed(aKid, aIndex);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLSelectElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify) 
{
  nsCOMPtr<nsIContent> content;
  ChildAt(aIndex, *getter_AddRefs(content));

  nsresult res = nsGenericHTMLContainerFormElement::RemoveChildAt(aIndex,
                                                                  aNotify);

  if (NS_SUCCEEDED(res) && content) {
    RemoveOption(content);
  }

  return res;
}

NS_IMETHODIMP
nsHTMLSelectElement::Add(nsIDOMHTMLElement* aElement,
                         nsIDOMHTMLElement* aBefore)
{
  nsresult result;
  nsCOMPtr<nsIDOMNode> ret;

  if (nsnull == aBefore) {
    result = AppendChild(aElement, getter_AddRefs(ret));
  }
  else {
    // Just in case we're not the parent, get the parent of the reference
    // element
    nsCOMPtr<nsIDOMNode> parent;
    
    result = aBefore->GetParentNode(getter_AddRefs(parent));
    if (parent) {
      result = parent->InsertBefore(aElement, aBefore, getter_AddRefs(ret));
    }
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLSelectElement::Remove(PRInt32 aIndex) 
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> option;

  if (!mOptions) {
    Init();
  }

  mOptions->Item(aIndex, getter_AddRefs(option));

  if (option) {
    nsCOMPtr<nsIDOMNode> parent;

    option->GetParentNode(getter_AddRefs(parent));
    if (parent) {
      nsCOMPtr<nsIDOMNode> ret;
      parent->RemoveChild(option, getter_AddRefs(ret));
    }
  }

  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptions(nsIDOMHTMLCollection** aValue)
{
  if (!mOptions) {
    Init();
  }

  *aValue = mOptions;
  NS_IF_ADDREF(*aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetType(nsAWritableString& aType)
{
  PRBool isMultiple;
  nsresult result = NS_OK;

  result = GetMultiple(&isMultiple);
  if (NS_OK == result) {
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
  if (!mOptions) {
    Init();
  }

  if (mOptions) {
    return mOptions->GetLength(aLength);
  }

  *aLength = 0;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetLength(PRUint32 aLength)
{
  if (!mOptions) {
    Init();
  }

  nsresult rv=NS_OK;

  PRUint32 curlen;
  PRInt32 i;

  GetLength(&curlen);

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
  nsIFormControlFrame* formControlFrame = nsnull;

  nsresult rv = GetPrimaryFrame(this, formControlFrame, PR_FALSE);

  if (NS_SUCCEEDED(rv)) {
    nsAutoString value;
    rv = formControlFrame->GetProperty(nsHTMLAtoms::selectedindex, value);

    if (NS_SUCCEEDED(rv) && (value.Length() > 0)) {
      PRInt32 retval = 0;
      PRInt32 error = 0;

      retval = value.ToInteger(&error, 10); // Convert to integer, base 10

      if (!error) {
        *aValue = retval;
      } else {
        rv = NS_ERROR_UNEXPECTED;
      }
    }
  } else { // The frame hasn't been created yet.  Use the options array
    *aValue = -1;
    // Call the helper method to get the PresState and 
    // the SupportsArray that contains the value
    //
    nsCOMPtr<nsIPresState> presState;
    nsCOMPtr<nsISupportsArray> value;

    nsresult res = GetPresState(getter_AddRefs(presState),
                                getter_AddRefs(value));

    if (NS_SUCCEEDED(res) && presState) {
      nsCOMPtr<nsISupports> supp;
      presState->GetStatePropertyAsSupports(NS_LITERAL_STRING("selecteditems"),
                                            getter_AddRefs(supp));

      res = NS_ERROR_NULL_POINTER;

      if (supp) {
        nsCOMPtr<nsISupportsArray> svalue = do_QueryInterface(supp);

        if (svalue) {
          PRUint32 count = 0;
          svalue->Count(&count);

          nsCOMPtr<nsISupportsPRInt32> thisVal;
          for (PRUint32 i=0; i<count; i++) {
            nsCOMPtr<nsISupports> suppval = getter_AddRefs(svalue->ElementAt(i));
            thisVal = do_QueryInterface(suppval);

            if (thisVal) {
              res = thisVal->GetData(aValue);
              if (NS_SUCCEEDED(res)) {
                // if we find the value the return NS_OK
                return NS_OK;
              }
            } else {
              res = NS_ERROR_UNEXPECTED;
            }

            if (NS_FAILED(res))
              break;
          }
        }
      }

      // At this point we have no frame
      // and the PresState didn't have the value we were looking for
      // so now go get the default value.
      if (NS_FAILED(res)) {
        // If we are a combo box, our default selectedIndex is 0, not -1;
        // XXX The logic here is duplicated in
        // nsCSSFrameConstructor::ConstructSelectFrame and
        // nsSelectControlFrame::GetDesiredSize
        PRBool isMultiple;
        rv = GetMultiple(&isMultiple);         // Must not be multiple
        if (NS_SUCCEEDED(rv) && !isMultiple) {
          PRInt32 size = 1;
          rv = GetSize(&size);                 // Size 1 or not set
          if (NS_SUCCEEDED(rv) &&
              ((1 >= size) || (NS_CONTENT_ATTR_NOT_THERE == size))) {
            *aValue = 0;
          }
        }

        nsCOMPtr<nsIDOMHTMLCollection> options;
        rv = GetOptions(getter_AddRefs(options));

        if (NS_SUCCEEDED(rv) && options) {
          PRUint32 numOptions;
          rv = options->GetLength(&numOptions);

          if (NS_SUCCEEDED(rv)) {
            for (PRUint32 i = 0; i < numOptions; i++) {
              nsCOMPtr<nsIDOMNode> node;
              rv = options->Item(i, getter_AddRefs(node));

              if (node) {
                nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));

                if (option) {
                  PRBool selected;
                  // DefaultSelected == HTML Selected
	                rv = option->GetDefaultSelected(&selected);
                  if (NS_SUCCEEDED(rv) && selected) {
                    *aValue = i;
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetSelectedIndex(PRInt32 aIndex)
{
  nsIFormControlFrame* formControlFrame = nsnull;

  GetPrimaryFrame(this, formControlFrame, PR_FALSE);

  if (formControlFrame) {
    nsAutoString value;
    value.AppendInt(aIndex, 10);

    nsCOMPtr<nsIPresContext> presContext;

    GetPresContext(this, getter_AddRefs(presContext));

    formControlFrame->SetProperty(presContext, nsHTMLAtoms::selectedindex,
                                  value);
  } else {
    SetOptionSelectedByIndex(aIndex, PR_TRUE);
  }

  return NS_OK;
}

nsresult
nsHTMLSelectElement::GetOptionIndex(nsIDOMHTMLOptionElement* aOption,
                                    PRUint32 * anIndex)
{
  NS_ENSURE_ARG_POINTER(anIndex);
  *anIndex = 0;

  // first find index of option
  nsCOMPtr<nsIDOMHTMLCollection> options;

  nsresult rv = GetOptions(getter_AddRefs(options));

  if (options) {
    PRUint32 numOptions;

    rv = options->GetLength(&numOptions);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIDOMNode> node;

    for (PRUint32 i = 0; i < numOptions; i++) {
      rv = options->Item(i, getter_AddRefs(node));
      if (NS_SUCCEEDED(rv) && node) {
        nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(node));
        if (option && option.get() == aOption) {
          *anIndex = i;
          return NS_OK;
        }
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
  PRUint32 index    = 0;
  if (NS_FAILED(GetOptionIndex(aOption, &index))) {
    return NS_ERROR_FAILURE;
  }

  // The PresState hold a nsISupportsArray that contain
  // nsISupportsPRInt32 objects that hold the index of the option that
  // is selected, for example, there are 10 options and option 2 and
  // oiption 8 are selected then the array hold to objects, one that
  // contains 2 and one that contains 8

  nsCOMPtr<nsIPresState> presState;
  nsCOMPtr<nsISupportsArray> value;

  nsresult res = GetPresState(getter_AddRefs(presState),
                              getter_AddRefs(value));

  if (NS_SUCCEEDED(res) && presState) {
    // get the property by name. I am not sure why this is done, versus
    // just using the value array above
    nsCOMPtr<nsISupports> supp;
    presState->GetStatePropertyAsSupports(NS_LITERAL_STRING("selecteditems"),
                                          getter_AddRefs(supp));

    res = NS_ERROR_NULL_POINTER;

    if (supp) {
      // Query for nsISupportArray from nsISupports
      nsCOMPtr<nsISupportsArray> svalue = do_QueryInterface(supp);

      if (svalue) {
        PRUint32 count = 0, i; // num items in array
        svalue->Count(&count);

        // loop through the array look for "index" object and then we
        // compare it against the option's index to see if there is a
        // match
        nsCOMPtr<nsISupportsPRInt32> thisVal;

        for (i=0; i < count; i++) {
          // get index obj
          nsCOMPtr<nsISupports> suppval = getter_AddRefs(svalue->ElementAt(i));
          thisVal = do_QueryInterface(suppval);

          if (thisVal) {
            // get out the index
            PRInt32 optIndex;
            res = thisVal->GetData(&optIndex);
            if (NS_SUCCEEDED(res)) {
              // ccompare them then bail if they match
              if (PRUint32(optIndex) == index) {
                *aIsSelected = PR_TRUE;
                return NS_OK;
              }
            }
          } else {
            res = NS_ERROR_UNEXPECTED;
          }

          if (NS_FAILED(res))
            break;
        }
      }
    }
  }

  return NS_OK;

}

NS_IMETHODIMP
nsHTMLSelectElement::SetOptionSelected(nsIDOMHTMLOptionElement* anOption,
                                       PRBool aIsSelected)
{
  PRUint32 index = 0;

  if (NS_FAILED(GetOptionIndex(anOption, &index))) {
    return NS_ERROR_FAILURE;
  }

  return SetOptionSelectedByIndex(index, aIsSelected);
}

nsresult
nsHTMLSelectElement::SetOptionSelectedByIndex(PRInt32 aIndex,
                                              PRBool aIsSelected)
{
  // Since there are no frames, then we must adjust which options
  // are selcted in the content and the current select state is kept in
  // the PreState
  //
  // First, get whether this is a select with 
  // multiple items that can be slected
  PRBool isMultiple;
  nsresult res = GetMultiple(&isMultiple);         // Must not be multiple
  if (NS_FAILED(res)) {
    isMultiple = PR_FALSE;
  }

  // Call the helper method to get the PresState and 
  // the SupportsArray that contains the value
  //
  nsCOMPtr<nsIPresState> presState;
  nsCOMPtr<nsISupportsArray> value;
  res = GetPresState(getter_AddRefs(presState), getter_AddRefs(value));
  if (NS_SUCCEEDED(res) && !presState) {
    return NS_OK;
  }

  // If the nsISupportsArray doesn't exist then we must creat one
  // cache the values
  PRBool doesExist = PR_FALSE;
  if (!value) {
    // if we are selecting an options continue on
    // but if the array doesn't exist and we are unselecting an option bail out
    // because we don't need to do anything
    if (aIsSelected) {
      res = NS_NewISupportsArray(getter_AddRefs(value));
    } else {
      return NS_OK;
    }
  } else {
    doesExist = PR_TRUE;
  }

  if (NS_SUCCEEDED(res) && value) {
    // The nsISupportArray will hold a list of indexes 
    // of the items that are selected
    // get the count of selected items
    PRUint32 count = 0;
    value->Count(&count);

    // If there are no items and we are unselecting an option
    // then we don't have to do anything.
    if (count == 0 && !aIsSelected) {
      return NS_OK;
    }

    // if not multiple then only one item can be selected
    // so we clear the list of selcted items
    if (!isMultiple || (-1 == aIndex)) { // -1 -> deselect all (bug 28143)
      value->Clear();
      count = 0;
    }

    // Set the index as selected and add it to the array
    if (-1 != aIndex) {
      if (aIsSelected) {
        nsCOMPtr<nsISupportsPRInt32> thisVal;

        thisVal = do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID);

        if (thisVal) {
          res = thisVal->SetData(aIndex);
          if (NS_SUCCEEDED(res)) {
            PRBool okay = value->InsertElementAt((nsISupports *)thisVal, count);
            if (!okay) res = NS_ERROR_OUT_OF_MEMORY; // Most likely cause;
          }
        }
      } else {
        // deselecting the option -> remove it from the list
        nsCOMPtr<nsISupportsPRInt32> thisVal;
        for (PRUint32 i = 0; i < count; i++) {
          nsCOMPtr<nsISupports> suppval = getter_AddRefs(value->ElementAt(i));
          thisVal = do_QueryInterface(suppval);
          if (thisVal) {
            PRInt32 optIndex;
            res = thisVal->GetData(&optIndex);
            if (NS_SUCCEEDED(res)) {
              if (optIndex == aIndex) {
                value->RemoveElementAt(i);
                return NS_OK;
              }
            }
          } else {
            res = NS_ERROR_UNEXPECTED;
          }
          if (NS_FAILED(res))
            break;
        }
      }
    }

    // If it is a new nsISupportsArray then 
    // set it into the PresState
    if (!doesExist) {
      presState->SetStatePropertyAsSupports(NS_LITERAL_STRING("selecteditems"),
                                            value);
    }
  } // if

  return NS_OK;
}

//NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Value, value)
NS_IMETHODIMP 
nsHTMLSelectElement::GetValue(nsAWritableString& aValue)
{
  nsresult result = NS_OK;
  PRInt32 selectedIndex;

  result = GetSelectedIndex(&selectedIndex);
  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIDOMHTMLCollection> options;

    result = GetOptions(getter_AddRefs(options));
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDOMNode> node;

      if (selectedIndex == -1) {
        selectedIndex = 0;
      }

      result = options->Item(selectedIndex, getter_AddRefs(node));

      if (NS_SUCCEEDED(result) && node) {
        nsCOMPtr<nsIHTMLContent> option = do_QueryInterface(node);

        if (option) {
          nsHTMLValue value;
          // first check to see if value is there and has a value
          nsresult rv = option->GetHTMLAttribute(nsHTMLAtoms::value, value);

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
  }

  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetValue(const nsAReadableString& aValue)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMHTMLCollection> options;
  
  result = GetOptions(getter_AddRefs(options));

  if (NS_SUCCEEDED(result)) {
    PRUint32 i, length;

    options->GetLength(&length);

    for(i = 0; i < length; i++) {
      nsCOMPtr<nsIDOMNode> node;

      result = options->Item(i, getter_AddRefs(node));

      if (NS_SUCCEEDED(result) && node) {
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
    
  return result;
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

  GetPrimaryFrame(this, formControlFrame);

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

  GetPrimaryFrame(this, formControlFrame);

  if (formControlFrame) {
    formControlFrame->SetFocus(PR_FALSE, PR_FALSE);
  }

  nsCOMPtr<nsIEventStateManager> esm;
  aPresContext->GetEventStateManager(getter_AddRefs(esm));

  if (esm) {
    nsCOMPtr<nsIDocument> doc;

    GetDocument(*getter_AddRefs(doc));

    if (!doc)
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIContent> rootContent;
    doc->GetRootContent(getter_AddRefs(rootContent));

    rv = esm->SetContentState(rootContent, NS_EVENT_STATE_FOCUS);
  }

  return rv;
}

NS_IMETHODIMP 
nsHTMLSelectElement::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  if (!mOptions) {
    Init();
  }

  if (mOptions) {
    return mOptions->Item(aIndex, aReturn);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsHTMLSelectElement::NamedItem(const nsAReadableString& aName,
                               nsIDOMNode** aReturn)
{
  if (!mOptions) {
    Init();
  }

  if (mOptions) {
    return mOptions->NamedItem(aName, aReturn);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTMLSelectElement::AddOptionIndexed(nsIContent* aContent, PRInt32 aIndex)
{
  // When first populating the select, this will be null but that's ok
  // as we will manually update the widget at frame construction time.
  if (!mOptions)
    return NS_OK;

  // Add the option to the option list.
  mOptions->AddOptionIndexed(aContent, aIndex);

  // Update the widget
  nsIFormControlFrame* fcFrame = nsnull;

  nsresult result = GetPrimaryFrame(this, fcFrame, PR_FALSE);

  if (fcFrame) {
    nsISelectControlFrame* selectFrame = nsnull;

    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),
                                     (void **) &selectFrame);

    if (NS_SUCCEEDED(result) && selectFrame) {
      nsCOMPtr<nsIPresContext> presContext;

      GetPresContext(this, getter_AddRefs(presContext));

      result = selectFrame->AddOption(presContext, aIndex);
    }
  }
  
  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::AddOption(nsIContent* aContent)
{
  // When first populating the select, this will be null but that's ok
  // as we will manually update the widget at frame construction time.
  if (!mOptions)
    return NS_OK;

  // Add the option to the option list.
  mOptions->AddOption(aContent);

  // Update the widget
  nsIFormControlFrame* fcFrame = nsnull;

  nsresult result = GetPrimaryFrame(this, fcFrame, PR_FALSE);

  if (fcFrame) {
    nsISelectControlFrame* selectFrame = nsnull;

    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),
                                     (void **) &selectFrame);

    if (NS_SUCCEEDED(result) && selectFrame) {
      nsCOMPtr<nsIPresContext> presContext;

      GetPresContext(this, getter_AddRefs(presContext));

      result = selectFrame->AddOption(presContext,
                                      mOptions->IndexOf(aContent));
    }
  }
  
  return result;
}

//-----------------------------------------------------------------
// Returns NS_ERROR_FAILURE, if it can't get the PresState
// Returns NS_OK, if it can
//
// aValueArray will be null if it didn't exist and non-null if it did
// always return NS_OK whether aValueArray was nsnull or not
//-----------------------------------------------------------------
nsresult 
nsHTMLSelectElement::GetPresState(nsIPresState** aPresState,
                                  nsISupportsArray** aValueArray)
{
  *aValueArray = nsnull;
  *aPresState  = nsnull;

  // Retrieve the presentation state instead.
  nsCOMPtr<nsIPresState> presState;
  nsresult rv = GetPrimaryPresState(this, getter_AddRefs(presState));

  // Obtain the value property from the presentation state.
  if (NS_FAILED(rv) || !presState) {
    return rv;
  }

  // check to see if there is already a supports
  nsCOMPtr<nsISupports> supp;

  nsresult res = presState->GetStatePropertyAsSupports(NS_LITERAL_STRING("selecteditems"), getter_AddRefs(supp));

  if (NS_SUCCEEDED(res) && supp) {
    if (NS_FAILED(supp->QueryInterface(NS_GET_IID(nsISupportsArray),
                                       (void**)aValueArray))) {
      // Be paranoid - make sure it is zeroed out
      *aValueArray = nsnull;
    }
  }

  *aPresState = presState;
  NS_ADDREF(*aPresState);

  return rv;
}

NS_IMETHODIMP 
nsHTMLSelectElement::RemoveOption(nsIContent* aContent)
{
  // When first populating the select, this will be null but that's ok
  // as we will manually update the widget at frame construction time.
  if (!mOptions)
    return NS_OK;

  PRInt32 oldIndex = mOptions->IndexOf(aContent);

  // Remove the option from the options list
  mOptions->RemoveOption(aContent);

  // Update the widget
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = GetPrimaryFrame(this, fcFrame, PR_FALSE);

  if (NS_SUCCEEDED(result) && fcFrame) {
    nsISelectControlFrame* selectFrame = nsnull;

    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),
                                     (void **)&selectFrame);

    if (NS_SUCCEEDED(result) && selectFrame) {
      // We can't get our index if we've already been replaced in the
      // OptionCollection. If we couldn't get our index, pass -1, remove
      // all options and recreate Coincidentally, IndexOf returns -1
      // if the option isn't found in the list

      nsCOMPtr<nsIPresContext> presContext;

      GetPresContext(this, getter_AddRefs(presContext));

      result = selectFrame->RemoveOption(presContext, oldIndex);
    }
  } else {
    // Since there are no frames, then we must adjust which options
    // are selcted in the content and the current select state is kept in
    // the PreState
    //
    // Call the helper method to get the PresState and 
    // the SupportsArray that contains the value
    //
    nsCOMPtr<nsIPresState> presState;
    nsCOMPtr<nsISupportsArray> value;
    nsresult res = GetPresState(getter_AddRefs(presState),
                                getter_AddRefs(value));

    if (NS_SUCCEEDED(res) && value) {
      // The nsISupportArray is a list of selected options
      // Get the number of "selected" options in the select
      PRUint32 count = 0;

      value->Count(&count);

      // look through the list to see if the option being removed is selected
      // then remove it from the nsISupportsArray
      PRInt32 j=0;
      PRUint32 i;

      for (i = 0; i < count; i++) {
        nsCOMPtr<nsISupports> suppval = getter_AddRefs(value->ElementAt(i));

        if (suppval) {
          nsCOMPtr<nsISupportsPRInt32> thisVal = do_QueryInterface(suppval);

          if (thisVal) {
            res = thisVal->GetData(&j);
            // the the index being removed match the indexed that is cached?

            if (NS_SUCCEEDED(res) && j == oldIndex) {
              res = value->RemoveElementAt(i);

              break;
            }
          } else {
            res = NS_ERROR_UNEXPECTED;
          }

          break;
        }
      } // for
    }
  }

  return result;
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
  nsresult result = GetPrimaryFrame(this, fcFrame, PR_FALSE);

  if (NS_SUCCEEDED(result) && fcFrame) {
    nsISelectControlFrame* selectFrame = nsnull;

    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),
                                     (void **)&selectFrame);

    if (NS_SUCCEEDED(result) && selectFrame) {
      result = selectFrame->DoneAddingContent(mIsDoneAddingContent);
    }
  }

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
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (!aData || !aAttributes)
    return;

  nsGenericHTMLElement::MapAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
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
  rv = GetPrimaryFrame(this, formControlFrame);
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
nsHTMLSelectElement::Init()
{
  if (!mOptions) {
    mOptions = new nsHTMLOptionCollection(this);
    NS_ADDREF(mOptions);
  } else {
    mOptions->Clear();
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// nsHTMLOptionCollection implementation
// XXX this was modified form nsHTMLFormElement.cpp. We need a base
// class implementation

static PRBool
GetOptionsRecurse(nsIContent* aContent, nsVoidArray& aOptions)
{
  PRBool result = PR_TRUE;
  PRInt32 numChildren;
  aContent->ChildCount(numChildren);

  nsCOMPtr<nsIContent> child;

  for (int i = 0; i < numChildren; i++) {
    aContent->ChildAt(i, *getter_AddRefs(child));

    if (child) {
      nsIDOMHTMLOptionElement* option = nsnull;

      child->QueryInterface(NS_GET_IID(nsIDOMHTMLOptionElement),
                            (void**)&option);

      if (option) {
        aOptions.AppendElement(option); // keep the ref count
      } else {
        // there must be an <optgroup> element; disable flat options speedup
        result = PR_FALSE;
        // ignore return from lower levels.
        GetOptionsRecurse(child, aOptions);
      }
    }
  }
  return result;
}

void 
nsHTMLOptionCollection::GetOptions()
{
  Clear();
  mFlat = GetOptionsRecurse(mSelect, mElements);
  mDirty = PR_FALSE;
}


nsHTMLOptionCollection::nsHTMLOptionCollection(nsHTMLSelectElement* aSelect) 
{
  mDirty = PR_TRUE;
  mFlat = PR_TRUE;
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
  if (mDirty && mSelect) {
    GetOptions();
  }

  *aLength = (PRUint32)mElements.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetLength(PRUint32 aLength)
{
  nsresult result = NS_ERROR_UNEXPECTED;

  if (mSelect) {
    if (mDirty) {
      GetOptions();
    }  

    result = mSelect->SetLength(aLength);
  }

  return result;  
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetOption(PRInt32 aIndex,
                                  nsIDOMHTMLOptionElement *aOption)
{
  if (!mSelect) {
    return NS_OK;
  }

  nsresult rv = NS_ERROR_FAILURE;

  // Update the options list
  if (mDirty) {
    GetOptions();
  }

  PRInt32 length = mElements.Count();

  // If the indx is within range
  if ((aIndex >= 0) && (aIndex <= length)) {
    // if the new option is null, remove this option
    if (!aOption) {
      mSelect->Remove(aIndex);

      // We're done.

      return NS_OK;
    }

    nsCOMPtr<nsIDOMNode> ret;

    if (aIndex == length) {
      rv = mSelect->AppendChild(aOption, getter_AddRefs(ret));
    } else {
      nsIDOMNode *refChild = (nsIDOMNode*)mElements.ElementAt(aIndex);
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

  if (mDirty) {
    GetOptions();
  }  

  return mSelect->GetSelectedIndex(aSelectedIndex);
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetSelectedIndex(PRInt32 aSelectedIndex)
{
  NS_ENSURE_TRUE(mSelect, NS_ERROR_UNEXPECTED);

  if (mDirty) {
    GetOptions();
  }

  return mSelect->SetSelectedIndex(aSelectedIndex);
}

NS_IMETHODIMP
nsHTMLOptionCollection::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  if (mDirty && mSelect) {
    GetOptions();
  }

  PRUint32 length = 0;
  GetLength(&length);

  if (aIndex >= length) {
    *aReturn = nsnull;
  } else {
    *aReturn = (nsIDOMNode*)mElements.ElementAt(aIndex);
    NS_ADDREF(*aReturn);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLOptionCollection::NamedItem(const nsAReadableString& aName,
                                  nsIDOMNode** aReturn)
{
  if (mDirty && mSelect) {
    GetOptions();
  }

  PRUint32 count = mElements.Count();
  nsresult result = NS_OK;

  *aReturn = nsnull;
  for (PRUint32 i = 0; i < count && !*aReturn; i++) {
    nsIDOMHTMLOptionElement *option;
    option = (nsIDOMHTMLOptionElement*)mElements.ElementAt(i);

    if (nsnull != option) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(option));

      if (content) {
        nsAutoString name;
        // XXX Should it be an EqualsIgnoreCase?

        if (((content->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::name,
                               name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name))) ||
            ((content->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::id,
                               name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name)))) {
          result = option->QueryInterface(NS_GET_IID(nsIDOMNode),
                                          (void **)aReturn);
        }
      }
    }
  }
  
  return result;
}

void 
nsHTMLOptionCollection::AddOption(nsIContent* aOption)
{
  // Just mark ourselves as dirty. The next time someone
  // makes a call that requires us to look at the elements
  // list, we'll recompute it.
  mDirty = PR_TRUE;
}

void 
nsHTMLOptionCollection::AddOptionIndexed(nsIContent* aOption, PRInt32 aIndex)
{
  if (!mDirty) {
    PRBool insertAsFlat = mFlat;

    if (!insertAsFlat) {
      // try to find a way to avoid rebuilding the list
      if (aIndex > 0) {
        PRInt32 numChildren;
        mSelect->ChildCount(numChildren);

        // Note: we're called _after_ adding the child, so compare to
        // numchildren-1
        if (aIndex == mElements.Count() ||
            aIndex == numChildren-1) {
          // insert at end
          insertAsFlat = PR_TRUE;
        }
        nsCOMPtr<nsIContent> content;
        mSelect->ChildAt(aIndex-1, *getter_AddRefs(content));
        aIndex = mElements.IndexOf(content);
        if (aIndex != -1) {
          // the previous element is in the list, so we know we insert
          // right after it.
          aIndex += 1;
          insertAsFlat = PR_TRUE;
        }
      } else {
        insertAsFlat = PR_TRUE;
      }
    }
    if (insertAsFlat) {
      nsIDOMHTMLOptionElement *option = nsnull;
      
      aOption->QueryInterface(NS_GET_IID(nsIDOMHTMLOptionElement),
                              (void**)&option);
      // option was addrefed for us
      if (option)
        mElements.InsertElementAt(option, aIndex);
    } else {
      mDirty = PR_TRUE;
    }
  }
}

void 
nsHTMLOptionCollection::RemoveOption(nsIContent* aOption)
{
  nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(aOption));

  if (option) {
    mElements.RemoveElement(option);
  }
}

PRInt32
nsHTMLOptionCollection::IndexOf(nsIContent* aOption)
{
  nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(aOption));

  if (mDirty && mSelect) {
    GetOptions();
  }

  if (!option) {
    return -1;
  }

  return mElements.IndexOf(option);
}

void
nsHTMLOptionCollection::Clear()
{
  PRUint32 numOptions = mElements.Count(), i;
  nsIDOMHTMLOptionElement* option = nsnull;

  for (i = 0; i < numOptions; i++) {
    option = (nsIDOMHTMLOptionElement*)mElements.ElementAt(i);
    NS_ASSERTION(option,"option already released");
    NS_IF_RELEASE(option);
  }

  mElements.Clear();
  mFlat = PR_TRUE;
}


#ifdef DEBUG
NS_IMETHODIMP
nsHTMLSelectElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
