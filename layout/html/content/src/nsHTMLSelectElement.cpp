/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMNSHTMLSelectElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIFocusableContent.h"
#include "nsIEventStateManager.h"
#include "nsGenericDOMHTMLCollection.h"
#include "nsIJSScriptObject.h"
#include "nsISelectElement.h"
#include "nsISelectControlFrame.h"
#include "nsISizeOfHandler.h"
#include "nsIDOMNSHTMLOptionCollection.h"

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

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMNSHTMLSelectElementIID, NS_IDOMNSHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMNSHTMLOptionCollectionIID, NS_IDOMNSHTMLOPTIONCOLLECTION_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kISelectElementIID, NS_ISELECTELEMENT_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID); 
static NS_DEFINE_IID(kIFocusableContentIID, NS_IFOCUSABLECONTENT_IID);

class nsHTMLSelectElement;

// nsHTMLOptionCollection
class nsHTMLOptionCollection: public nsIDOMNSHTMLOptionCollection,
                              public nsGenericDOMHTMLCollection,
                              public nsIJSScriptObject
{
public:
  nsHTMLOptionCollection(nsHTMLSelectElement* aSelect);
  virtual ~nsHTMLOptionCollection();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNSHTMLOptionCollection interface
  NS_IMETHOD    SetLength(PRUint32 aLength);
  NS_IMETHOD    GetSelectedIndex(PRInt32 *aSelectedIndex);
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex);

  // nsIDOMHTMLCollection interface
  NS_DECL_IDOMHTMLCOLLECTION

  // nsIJSScriptObject interface
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);
  PRBool    AddProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    GetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    SetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj);
  PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID);
  PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
  void      Finalize(JSContext *aContext, JSObject *aObj);

  void AddOption(nsIContent* aOption);
  void RemoveOption(nsIContent* aOption);
  PRInt32 IndexOf(nsIContent* aOption);

  void Clear();
  void DropReference();

  void GetOptions();

private:
  nsVoidArray mElements;
  PRBool mDirty;
  nsHTMLSelectElement* mSelect;
};

class nsHTMLSelectElement : public nsIDOMHTMLSelectElement,
                            public nsIDOMNSHTMLSelectElement,
                            public nsIJSScriptObject,
                            public nsIHTMLContent,
                            public nsIFormControl,
                            public nsIFocusableContent,
                            public nsISelectElement
{
public:
  nsHTMLSelectElement(nsIAtom* aTag);
  virtual ~nsHTMLSelectElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLSelectElement
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD GetSelectedIndex(PRInt32* aSelectedIndex);
  NS_IMETHOD SetSelectedIndex(PRInt32 aSelectedIndex);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);
  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD SetLength(PRUint32 aLength);
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD GetOptions(nsIDOMNSHTMLOptionCollection** aOptions);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetMultiple(PRBool* aMultiple);
  NS_IMETHOD SetMultiple(PRBool aMultiple);
  NS_IMETHOD GetName(nsString& aName);
  NS_IMETHOD SetName(const nsString& aName);
  NS_IMETHOD GetSize(PRInt32* aSize);
  NS_IMETHOD SetSize(PRInt32 aSize);
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore);
  NS_IMETHOD Remove(PRInt32 aIndex);
  NS_IMETHOD Blur();
  NS_IMETHOD Focus();

  // nsIDOMNSHTMLSelectElement
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD NamedItem(const nsString& aName, nsIDOMNode** aReturn);

  // nsIContent
  NS_IMPL_ICONTENT_NO_SETPARENT_NO_SETDOCUMENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIFormControl
  NS_IMETHOD SetForm(nsIDOMHTMLFormElement* aForm);
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Init();

  NS_IMETHOD SetFocus(nsIPresContext* aPresContext);
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext);

  // nsISelectElement
  NS_IMETHOD AddOption(nsIContent* aContent);
  NS_IMETHOD RemoveOption(nsIContent* aContent);
  NS_IMETHOD DoneAddingContent(PRBool aIsDone);
  NS_IMETHOD IsDoneAddingContent(PRBool * aIsDone);

  // nsIJSScriptObject
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj,
                                jsval aID, jsval *aVp);
  virtual PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj,
                                   jsval aID, jsval *aVp);
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj,
                                jsval aID, jsval *aVp);
  // Implement this to enable setting option via frm.select[x]
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj,
                                jsval aID, jsval *aVp);
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj);
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual void      Finalize(JSContext *aContext, JSObject *aObj);

protected:
  NS_IMETHOD GetPresState(nsIPresState** aPresState, nsISupportsArray** aValueArray);

  nsGenericHTMLContainerElement mInner;
  nsIForm*      mForm;
  nsHTMLOptionCollection* mOptions;
  PRBool        mIsDoneAddingContent;
};


// nsHTMLSelectElement

// construction, destruction

nsresult
NS_NewHTMLSelectElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLSelectElement* it = new nsHTMLSelectElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLSelectElement::nsHTMLSelectElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mOptions = nsnull;
  mForm = nsnull;
  mIsDoneAddingContent = PR_TRUE;
}

nsHTMLSelectElement::~nsHTMLSelectElement()
{
  if (nsnull != mForm) {
    // prevent mForm from decrementing its ref count on us
    mForm->RemoveElement(this, PR_FALSE); 
    NS_RELEASE(mForm);
  }
  if (nsnull != mOptions) {
    mOptions->Clear();
    mOptions->DropReference();
    NS_RELEASE(mOptions);
  }
}

// ISupports

NS_IMPL_ADDREF(nsHTMLSelectElement)

nsresult
nsHTMLSelectElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  // NS_IMPL_HTML_CONTENT_QUERY_INTERFACE returns mInner as the script object
  // We need to be our own so we can implement setprop to set select[i]
  if (aIID.Equals(kIJSScriptObjectIID)) {
    *aInstancePtr = (void*)(nsIJSScriptObject*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLSelectElementIID)) {
    *aInstancePtr = (void*)(nsIDOMHTMLSelectElement*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kIDOMNSHTMLSelectElementIID)) {
    *aInstancePtr = (void*)(nsIDOMNSHTMLSelectElement*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kIFormControlIID)) {
    *aInstancePtr = (void*)(nsIFormControl*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kIFocusableContentIID)) {
    *aInstancePtr = (void*)(nsIFocusableContent*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kISelectElementIID)) {
    *aInstancePtr = (void*)(nsISelectElement*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP_(nsrefcnt)
nsHTMLSelectElement::Release()
{
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "nsHTMLSelectElement");
  if (mRefCnt <= 0) {
    delete this;                                       
    return 0;                                          
  } else if ((1 == mRefCnt) && mForm) { 
    mRefCnt = 0;
    delete this;
    return 0;
  } else {
    return mRefCnt;
  }
}

// nsIDOMHTMLSelectElement

nsresult
nsHTMLSelectElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLSelectElement* it = new nsHTMLSelectElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // Stabilize the refcount before copying the inner element.
  NS_ADDREF(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  nsresult rv = it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
  NS_RELEASE(it);
  return rv;
}

// nsIContent

NS_IMETHODIMP
nsHTMLSelectElement::SetParent(nsIContent* aParent)
{
  return mInner.SetParentForFormControls(aParent, this, mForm);
}

NS_IMETHODIMP
nsHTMLSelectElement::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
  return mInner.SetDocumentForFormControls(aDocument, aDeep, this, mForm);
}

NS_IMETHODIMP
nsHTMLSelectElement::Add(nsIDOMHTMLElement* aElement, nsIDOMHTMLElement* aBefore)
{
  nsresult result;
  nsIDOMNode* ret;

  if (nsnull == aBefore) {
    result = mInner.AppendChild(aElement, &ret);
    NS_IF_RELEASE(ret);
  }
  else {
    // Just in case we're not the parent, get the parent of the reference
    // element
    nsIDOMNode* parent;
    
    result = aBefore->GetParentNode(&parent);
    if (NS_SUCCEEDED(result) && (nsnull != parent)) {
      result = parent->InsertBefore(aElement, aBefore, &ret);
      NS_IF_RELEASE(ret);
      NS_RELEASE(parent);
    }
  }

  return result;
}

NS_IMETHODIMP 
nsHTMLSelectElement::Remove(PRInt32 aIndex) 
{
  nsresult result = NS_OK;
  nsIDOMNode* option;

  if (nsnull == mOptions) {
    Init();
  }

  result = mOptions->Item(aIndex, &option);
  if (NS_SUCCEEDED(result) && (nsnull != option)) {
    nsIDOMNode* parent;

    result = option->GetParentNode(&parent);
    if (NS_SUCCEEDED(result) && (nsnull != parent)) {
      nsIDOMNode* ret;
      parent->RemoveChild(option, &ret);
      NS_IF_RELEASE(ret);
      NS_RELEASE(parent);
    }

    NS_RELEASE(option);
  }

  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  nsresult result = NS_OK;
  *aForm = nsnull;
  if (nsnull != mForm) {
    nsIDOMHTMLFormElement* formElem = nsnull;
    result = mForm->QueryInterface(kIDOMHTMLFormElementIID, (void**)&formElem);
    if (NS_OK == result) {
      *aForm = formElem;
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetOptions(nsIDOMNSHTMLOptionCollection** aValue)
{
  if (nsnull == mOptions) {
    Init();
  }
  NS_ADDREF(mOptions);
  *aValue = mOptions;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetType(nsString& aType)
{
  PRBool isMultiple;
  nsresult result = NS_OK;

  result = GetMultiple(&isMultiple);
  if (NS_OK == result) {
    if (isMultiple) {
      aType.Assign("select-multiple");
    }
    else {
      aType.Assign("select-one");
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetLength(PRUint32* aLength)
{
  if (nsnull == mOptions) {
    Init();
  }
  if (nsnull != mOptions) {
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

  PRUint32 curlen;
  nsresult result;
  GetLength(&curlen);
  if (curlen && (curlen > aLength)) { // Remove extra options
    for (PRInt32 i = (curlen - 1); (i>=(PRInt32)aLength) && NS_SUCCEEDED(result); i--) {
      result = Remove(i);
    }
  } else { // Add options?
  }

  return NS_OK;
}

//NS_IMPL_INT_ATTR(nsHTMLSelectElement, SelectedIndex, selectedindex)
NS_IMETHODIMP
nsHTMLSelectElement::GetSelectedIndex(PRInt32* aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    nsString value;
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
    nsresult res = GetPresState(getter_AddRefs(presState), getter_AddRefs(value));
    if (NS_SUCCEEDED(res) && presState) {
      nsCOMPtr<nsISupports> supp;
      presState->GetStatePropertyAsSupports("selecteditems", getter_AddRefs(supp));

      nsresult res = NS_ERROR_NULL_POINTER;
      if (!supp)
        return NS_ERROR_FAILURE;

      nsCOMPtr<nsISupportsArray> value = do_QueryInterface(supp);
      if (!value)
        return NS_ERROR_FAILURE;

      PRUint32 count = 0;
      value->Count(&count);

      nsCOMPtr<nsISupportsPRInt32> thisVal;
      PRInt32 j=0;
      for (PRUint32 i=0; i<count; i++) {
        nsCOMPtr<nsISupports> suppval = getter_AddRefs(value->ElementAt(i));
        thisVal = do_QueryInterface(suppval);
        if (thisVal) {
          res = thisVal->GetData(aValue);
          if (NS_SUCCEEDED(res)) {
            return NS_OK;
          }
        } else {
          res = NS_ERROR_UNEXPECTED;
        }
        if (!NS_SUCCEEDED(res)) break;
      }
    } else {
      // If we are a combo box, our default selectedIndex is 0, not -1;
      // XXX The logic here is duplicated in
      // nsCSSFrameConstructor::ConstructSelectFrame and
      // nsSelectControlFrame::GetDesiredSize
      PRBool isMultiple;
      rv = GetMultiple(&isMultiple);         // Must not be multiple
      if (NS_SUCCEEDED(rv) && !isMultiple) {
        PRInt32 size = 1;
        rv = GetSize(&size);                 // Size 1 or not set
        if (NS_SUCCEEDED(rv) && ((1 >= size) || (NS_CONTENT_ATTR_NOT_THERE == size))) {
          *aValue = 0;
        }
      }
      nsCOMPtr<nsIDOMNSHTMLOptionCollection> options;
      rv = GetOptions(getter_AddRefs(options));
      if (NS_SUCCEEDED(rv) && options) {
        PRUint32 numOptions;
        rv = options->GetLength(&numOptions);
        if (NS_SUCCEEDED(rv)) {
          for (PRUint32 i = 0; i < numOptions; i++) {
            nsCOMPtr<nsIDOMNode> node;
            rv = options->Item(i, getter_AddRefs(node));
            if (NS_SUCCEEDED(rv) && node) {
              nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(node, &rv);
              if (NS_SUCCEEDED(rv) && option) {
                PRBool selected;
	              rv = option->GetDefaultSelected(&selected); // DefaultSelected == HTML Selected
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
  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetSelectedIndex(PRInt32 aIndex)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  if (NS_OK == nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame)) {
    nsString value;
    value.Append(aIndex, 10);
    nsIPresContext* presContext;
    nsGenericHTMLElement::GetPresContext(this, &presContext);
    formControlFrame->SetProperty(presContext, nsHTMLAtoms::selectedindex, value);
    NS_IF_RELEASE(presContext);
  } else {
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
      res = NS_NewISupportsArray(getter_AddRefs(value));
    } else {
      doesExist = PR_TRUE;
    }

    if (NS_SUCCEEDED(res) && value) {
      // The nsISupportArray will hold a list of indexes 
      // of the items that are selected
      // get the count of selected items
      PRUint32 count = 0;
      value->Count(&count);

      // if not multiple then only one item can be selected
      // so we clear the list of selcted items
      if (!isMultiple) {
        value->Clear();
        count = 0;
      }

      // Set the index as selected and add it to the array
      nsCOMPtr<nsISupportsPRInt32> thisVal;
      res = nsComponentManager::CreateInstance(NS_SUPPORTS_PRINT32_PROGID,
	                     nsnull, NS_GET_IID(nsISupportsPRInt32), (void**)getter_AddRefs(thisVal));
      if (NS_SUCCEEDED(res) && thisVal) {
        res = thisVal->SetData(aIndex);
	      if (NS_SUCCEEDED(res)) {
          PRBool okay = value->InsertElementAt((nsISupports *)thisVal, count);
	        if (!okay) res = NS_ERROR_OUT_OF_MEMORY; // Most likely cause;
        }
	    }

      // If it is a new nsISupportsArray then 
      // set it into the PresState
      if (!doesExist) {
        presState->SetStatePropertyAsSupports("selecteditems", value);
      }
    } // if
  }
  return NS_OK;
}

//NS_IMPL_STRING_ATTR(nsHTMLSelectElement, Value, value)
NS_IMETHODIMP 
nsHTMLSelectElement::GetValue(nsString& aValue)
{
  nsresult result = NS_OK;
  PRInt32 selectedIndex;

  result = GetSelectedIndex(&selectedIndex);
  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIDOMNSHTMLOptionCollection> options;

    result = GetOptions(getter_AddRefs(options));
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIDOMNode> node;
      if (selectedIndex == -1) {
        selectedIndex = 0;
      }
      result = options->Item(selectedIndex, getter_AddRefs(node));
      if (NS_SUCCEEDED(result) && node) {
        nsCOMPtr<nsIDOMHTMLOptionElement> option = do_QueryInterface(node);
        if (option) {
          option->GetValue(aValue);
          if (0 == aValue.Length()) {
            option->GetLabel(aValue);
            if (0 == aValue.Length()) {
              option->GetText(aValue);
            }
          }
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::SetValue(const nsString& aValue)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNSHTMLOptionCollection> options;
  
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
nsHTMLSelectElement::Blur() // XXX not tested
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
     // Ask the frame to Deselect focus (i.e Blur).
    formControlFrame->SetFocus(PR_FALSE, PR_TRUE);
    return NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::Focus()
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    formControlFrame->SetFocus(PR_TRUE, PR_TRUE);
    return NS_OK;
  }
  return rv;

}

NS_IMETHODIMP
nsHTMLSelectElement::SetFocus(nsIPresContext* aPresContext)
{
  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
    NS_RELEASE(esm);
  }

  // XXX Should focus only this presContext
  Focus();
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = nsGenericHTMLElement::GetPrimaryFrame(this, formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    formControlFrame->ScrollIntoView(aPresContext);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLSelectElement::RemoveFocus(nsIPresContext* aPresContext)
{
  // XXX Should focus only this presContext
  Blur();
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLSelectElement::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  if (!mOptions) {
    Init();
  }
  if (mOptions) {
    return  mOptions->Item(aIndex, aReturn);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsHTMLSelectElement::NamedItem(const nsString& aName, nsIDOMNode** aReturn)
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
nsHTMLSelectElement::AddOption(nsIContent* aContent)
{
  // When first populating the select, this will be null but that's ok
  // as we will manually update the widget at frame construction time.
  if (!mOptions) return NS_OK;

  // Add the option to the option list.
  mOptions->AddOption(aContent);

  // Update the widget
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = nsGenericHTMLElement::GetPrimaryFrame(this, fcFrame, PR_FALSE);
  if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
    nsISelectControlFrame* selectFrame = nsnull;
    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),(void **) &selectFrame);
    if (NS_SUCCEEDED(result) && (nsnull != selectFrame)) {
      nsIPresContext* presContext;
      nsGenericHTMLElement::GetPresContext(this, &presContext);
      result = selectFrame->AddOption(presContext, mOptions->IndexOf(aContent));
      NS_IF_RELEASE(presContext);
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
NS_IMETHODIMP 
nsHTMLSelectElement::GetPresState(nsIPresState** aPresState, nsISupportsArray** aValueArray)
{
  *aValueArray = nsnull;
  *aPresState  = nsnull;

  // Retrieve the presentation state instead.
  nsIPresState* presState;
  PRInt32 type;
  GetType(&type);
  nsresult rv = nsGenericHTMLElement::GetPrimaryPresState(this, nsIStatefulFrame::eSelectType, 
                                                          &presState);

  // Obtain the value property from the presentation state.
  if (NS_FAILED(rv) || presState == nsnull) {
    return rv;
  }

  // check to see if there is already a supports
  nsISupports * supp;
  nsresult res = presState->GetStatePropertyAsSupports("selecteditems", &supp);
  if (NS_SUCCEEDED(res) && supp != nsnull) {
    if (NS_FAILED(supp->QueryInterface(NS_GET_IID(nsISupportsArray), (void**)aValueArray))) {
      // Be paranoid - make sure it is zeroed out
      *aValueArray = nsnull;
    }
    NS_RELEASE(supp);
  }

  *aPresState = presState;

  return rv;
}

NS_IMETHODIMP 
nsHTMLSelectElement::RemoveOption(nsIContent* aContent)
{
  // When first populating the select, this will be null but that's ok
  // as we will manually update the widget at frame construction time.
  if (!mOptions) return NS_OK;

  PRInt32 oldIndex = mOptions->IndexOf(aContent);

  // Remove the option from the options list
  mOptions->RemoveOption(aContent);

  // Update the widget
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = nsGenericHTMLElement::GetPrimaryFrame(this, fcFrame, PR_FALSE);
  if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
    nsISelectControlFrame* selectFrame = nsnull;
    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),(void **) &selectFrame);
    if (NS_SUCCEEDED(result) && (nsnull != selectFrame)) {
      // We can't get our index if we've already been replaced in the OptionList.
      // If we couldn't get our index, pass -1, remove all options and recreate
      // Coincidentally, IndexOf returns -1 if the option isn't found in the list
      nsIPresContext* presContext;
      nsGenericHTMLElement::GetPresContext(this, &presContext);
      result = selectFrame->RemoveOption(presContext, oldIndex);
      NS_IF_RELEASE(presContext);
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
    nsresult res = GetPresState(getter_AddRefs(presState), getter_AddRefs(value));

    if (NS_SUCCEEDED(res) && value) {
      // The nsISupportArray is a list of selected options
      // Get the number of "selected" options in the select
      PRUint32 count = 0;
      value->Count(&count);

      // look through the list to see if the option being removed is selected
      // then remove it from the nsISupportsArray
      PRInt32 j=0;
      for (PRUint32 i=0; i<count; i++) {
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
          if (!NS_SUCCEEDED(res)) break;
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
  nsresult result = nsGenericHTMLElement::GetPrimaryFrame(this, fcFrame,PR_FALSE);
  if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
    nsISelectControlFrame* selectFrame = nsnull;
    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),(void **) &selectFrame);
    if (NS_SUCCEEDED(result) && (nsnull != selectFrame)) {
      result = selectFrame->DoneAddingContent(mIsDoneAddingContent);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
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
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLSelectElement::AttributeToString(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  aAttributes->GetAttribute(nsHTMLAtoms::align, value);
  if (eHTMLUnit_Enumerated == value.GetUnit()) {
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetMutableStyleData(eStyleStruct_Display);
    nsStyleText* text = (nsStyleText*)
      aContext->GetMutableStyleData(eStyleStruct_Text);
    switch (value.GetIntValue()) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      display->mFloats = NS_STYLE_FLOAT_LEFT;
      break;
    case NS_STYLE_TEXT_ALIGN_RIGHT:
      display->mFloats = NS_STYLE_FLOAT_RIGHT;
      break;
    default:
      text->mVerticalAlign.SetIntValue(value.GetIntValue(), eStyleUnit_Enumerated);
      break;
    }
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLSelectElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::multiple) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if ((aAttribute == nsHTMLAtoms::align) ||
           (aAttribute == nsHTMLAtoms::size)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
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

  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLSelectElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = NS_FORM_SELECT;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}


// An important assumption is that if aForm is null, the previous mForm will not be released
// This allows nsHTMLFormElement to deal with circular references.
NS_IMETHODIMP
nsHTMLSelectElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  nsresult result;
  nsIFormControl *formControl;

  result = QueryInterface(kIFormControlIID, (void**)&formControl);
  if (NS_FAILED(result))
    formControl = nsnull;

  if (mForm && formControl)
    mForm->RemoveElement(formControl, PR_TRUE);

  if (nsnull == aForm)
    mForm = nsnull;
  else {
    NS_IF_RELEASE(mForm);
    if (formControl) {
      result = aForm->QueryInterface(kIFormIID, (void**)&mForm); // keep the ref
      if ((NS_OK == result) && mForm) {
        mForm->AddElement(formControl);
      }
    }
  }
  NS_IF_RELEASE(formControl);
  return result;
}

NS_IMETHODIMP
nsHTMLSelectElement::Init()
{
  if (nsnull == mOptions) {
    mOptions = new nsHTMLOptionCollection(this);
    NS_ADDREF(mOptions);
  }

  return NS_OK;
}


// nsIJSScriptObject interface

PRBool    
nsHTMLSelectElement::AddProperty(JSContext *aContext, 
                          JSObject *aObj, 
                          jsval aID, 
                          jsval *aVp)
{
  return PR_TRUE;
}
 
PRBool    
nsHTMLSelectElement::DeleteProperty(JSContext *aContext, 
                             JSObject *aObj, 
                             jsval aID, 
                             jsval *aVp)
{
  return PR_TRUE;
}
 
PRBool    
nsHTMLSelectElement::GetProperty(JSContext *aContext, 
                          JSObject *aObj, 
                          jsval aID, 
                          jsval *aVp)
{
  return PR_TRUE;
}

PRBool    
nsHTMLSelectElement::SetProperty(JSContext *aContext, 
                          JSObject *aObj, 
                          jsval aID, 
                          jsval *aVp)
{
  nsresult res = NS_OK;
  // Set options in the options list by indexing into select
  if (JSVAL_IS_INT(aID) && mOptions) {
    nsIJSScriptObject* optList = nsnull;
    res = mOptions->QueryInterface(kIJSScriptObjectIID, (void **)&optList);
    if (NS_SUCCEEDED(res) && optList) {
      res = optList->SetProperty(aContext, aObj, aID, aVp);
    }
  } else {
    res = mInner.SetProperty(aContext, aObj, aID, aVp);
  }
  return res;
}

PRBool    
nsHTMLSelectElement::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return PR_TRUE;
}

PRBool    
nsHTMLSelectElement::Resolve(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return PR_TRUE;
}

PRBool    
nsHTMLSelectElement::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return PR_TRUE;
}

void      
nsHTMLSelectElement::Finalize(JSContext *aContext, JSObject *aObj)
{
}

//----------------------------------------------------------------------

// nsHTMLOptionCollection implementation
// XXX this was modified form nsHTMLFormElement.cpp. We need a base class implementation

static
void GetOptionsRecurse(nsIContent* aContent, nsVoidArray& aOptions)
{
  PRInt32 numChildren;
  aContent->ChildCount(numChildren);
  nsIContent* child = nsnull;
  nsIDOMHTMLOptionElement* option = nsnull;
  for (int i = 0; i < numChildren; i++) {
    aContent->ChildAt(i, child);
    if (child) {
      nsresult result = child->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
      if ((NS_OK == result) && option) {
        aOptions.AppendElement(option); // keep the ref count
      } else {
        GetOptionsRecurse(child, aOptions);
      }
      NS_RELEASE(child);
    }
  }
}

void 
nsHTMLOptionCollection::GetOptions()
{
  Clear();
  GetOptionsRecurse(mSelect, mElements);
  mDirty = PR_FALSE;
}


nsHTMLOptionCollection::nsHTMLOptionCollection(nsHTMLSelectElement* aSelect) 
{
  mDirty = PR_TRUE;
  // Do not maintain a reference counted reference. When
  // the select goes away, it will let us know.
  mSelect = aSelect;
  mScriptObject = nsnull;
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

// ISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLOptionCollection, nsGenericDOMHTMLCollection)
NS_IMPL_RELEASE_INHERITED(nsHTMLOptionCollection, nsGenericDOMHTMLCollection)

nsresult
nsHTMLOptionCollection::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
  if (aIID.Equals(kIJSScriptObjectIID)) {
    *aInstancePtr = (void*)(nsIJSScriptObject*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNSHTMLOptionCollectionIID)) {
    *aInstancePtr = (void*)(nsIDOMNSHTMLOptionCollection*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return nsGenericDOMHTMLCollection::QueryInterface(aIID, aInstancePtr);
}

// nsIDOMNSHTMLOptionCollection interface

NS_IMETHODIMP    
nsHTMLOptionCollection::GetLength(PRUint32* aLength)
{
  if (mDirty && (nsnull != mSelect)) {
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
nsHTMLOptionCollection::GetSelectedIndex(PRInt32 *aSelectedIndex)
{
  nsresult result = NS_ERROR_UNEXPECTED;
  if (mSelect) {
    if (mDirty) {
      GetOptions();
    }  
    result = mSelect->GetSelectedIndex(aSelectedIndex);
  }
  return result;  
}

NS_IMETHODIMP
nsHTMLOptionCollection::SetSelectedIndex(PRInt32 aSelectedIndex)
{
  nsresult result = NS_ERROR_UNEXPECTED;
  if (mSelect) {
    if (mDirty) {
      GetOptions();
    }
    result = mSelect->SetSelectedIndex(aSelectedIndex);
  }
  return result;
}

NS_IMETHODIMP
nsHTMLOptionCollection::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  if (mDirty && (nsnull != mSelect)) {
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
nsHTMLOptionCollection::NamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  if (mDirty && (nsnull != mSelect)) {
    GetOptions();
  }
  PRUint32 count = mElements.Count();
  nsresult result = NS_OK;

  *aReturn = nsnull;
  for (PRUint32 i = 0; i < count && *aReturn == nsnull; i++) {
    nsIDOMHTMLOptionElement *option;
    option = (nsIDOMHTMLOptionElement*)mElements.ElementAt(i);
    if (nsnull != option) {
      nsIContent *content;
      
      result = option->QueryInterface(kIContentIID, (void **)&content);
      if (NS_OK == result) {
        nsAutoString name;
        // XXX Should it be an EqualsIgnoreCase?
        if (((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name))) ||
            ((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::id, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name)))) {
          result = option->QueryInterface(kIDOMNodeIID, (void **)aReturn);
        }
        NS_RELEASE(content);
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
nsHTMLOptionCollection::RemoveOption(nsIContent* aOption)
{
  nsIDOMHTMLOptionElement* option;

  if ((nsnull != aOption) &&
      NS_SUCCEEDED(aOption->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option))) {
    if (mElements.RemoveElement(option)) {
      nsresult result;
      NS_RELEASE2(option, result);
    }
    NS_RELEASE(option);
  }
}

PRInt32
nsHTMLOptionCollection::IndexOf(nsIContent* aOption)
{
  nsIDOMHTMLOptionElement* option;

  if (mDirty && (nsnull != mSelect)) {
    GetOptions();
  }
  if ((nsnull != aOption) &&
    NS_SUCCEEDED(aOption->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option))) {
    return mElements.IndexOf(option);
  }
  return -1;
}

// nsIScriptObjectOwner interface

NS_IMETHODIMP
nsHTMLOptionCollection::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptNSHTMLOptionCollection(aContext, (nsISupports *)(nsIDOMNSHTMLOptionCollection *)this, nsnull, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsHTMLOptionCollection::SetScriptObject(void* aScriptObject)
{
  return nsGenericDOMHTMLCollection::SetScriptObject(aScriptObject);
}

// nsIJSScriptObject interface

PRBool    
nsHTMLOptionCollection::AddProperty(JSContext *aContext, 
                          JSObject *aObj, 
                          jsval aID, 
                          jsval *aVp)
{
  return PR_TRUE;
}
 
PRBool    
nsHTMLOptionCollection::DeleteProperty(JSContext *aContext, 
                             JSObject *aObj, 
                             jsval aID, 
                             jsval *aVp)
{
  return PR_TRUE;
}
 
PRBool    
nsHTMLOptionCollection::GetProperty(JSContext *aContext, 
                          JSObject *aObj, 
                          jsval aID, 
                          jsval *aVp)
{
  return PR_TRUE;
}

PRBool    
nsHTMLOptionCollection::SetProperty(JSContext *aContext, 
                          JSObject *aObj, 
                          jsval aID, 
                          jsval *aVp)
{
  // XXX How about some error reporting and error
  // propogation in this method???

  if (JSVAL_IS_INT(aID) && (nsnull != mSelect)) {
    PRInt32 indx = JSVAL_TO_INT(aID);
    nsresult result;

    // Update the options list
    if (mDirty) {
      GetOptions();
    }
    
    PRInt32 length = mElements.Count();

    // If the indx is within range
    if ((indx >= 0) && (indx <= length)) {

      // if the value is null, remove this option
      if (JSVAL_IS_NULL(*aVp)) {
        mSelect->Remove(indx);
      }
      else {
        JSObject* jsobj = JSVAL_TO_OBJECT(*aVp); 
        JSClass* jsclass = JS_GetClass(aContext, jsobj);
        if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) {
          nsISupports *supports = (nsISupports *)JS_GetPrivate(aContext, jsobj);
          nsIDOMNode* option; 
          nsIDOMNode* parent;
          nsIDOMNode* refChild;
          nsIDOMNode* ret;

          if (NS_OK == supports->QueryInterface(kIDOMNodeIID, (void **)&option)) {
            if (indx == length) {
              result = mSelect->AppendChild(option, &ret);
              NS_IF_RELEASE(ret);
            }
            else {
              refChild = (nsIDOMNode*)mElements.ElementAt(indx);
              if (nsnull != refChild) {
                result = refChild->GetParentNode(&parent);
                if (NS_SUCCEEDED(result) && (nsnull != parent)) {
                  
                  result = parent->ReplaceChild(option, refChild, &ret);
                  NS_IF_RELEASE(ret);
                  NS_RELEASE(parent);
                }
              }            
            }
            NS_RELEASE(option);
          }
        }
      }
    }
  }

  return PR_TRUE;
}

PRBool    
nsHTMLOptionCollection::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return PR_TRUE;
}

PRBool    
nsHTMLOptionCollection::Resolve(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return PR_TRUE;
}

PRBool    
nsHTMLOptionCollection::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return PR_TRUE;
}

void      
nsHTMLOptionCollection::Finalize(JSContext *aContext, JSObject *aObj)
{
}

void
nsHTMLOptionCollection::Clear()
{
  PRUint32 numOptions = mElements.Count();
  for (PRUint32 i = 0; i < numOptions; i++) {
    nsIDOMHTMLOptionElement* option = (nsIDOMHTMLOptionElement*)mElements.ElementAt(i);
    NS_ASSERTION(option,"option already released");
    NS_RELEASE(option);
  }
  mElements.Clear();
}


NS_IMETHODIMP
nsHTMLSelectElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  mInner.SizeOf(aSizer, aResult, sizeof(*this));
  if (mForm) {
    PRBool recorded;
    aSizer->RecordObject(mForm, &recorded);
    if (!recorded) {
      PRUint32 formSize;
      mForm->SizeOf(aSizer, &formSize);
      aSizer->AddSize(nsHTMLAtoms::iform, formSize);
    }
  }
#endif
  return NS_OK;
}
