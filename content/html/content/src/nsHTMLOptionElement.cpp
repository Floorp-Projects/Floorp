/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsIDOMText.h"

// Notify/query select frame for selected state
#include "nsIFormControlFrame.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIDOMHTMLSelectElement.h"


static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptGroupElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID); 

class nsHTMLOptionElement : public nsIDOMHTMLOptionElement,
                     public nsIScriptObjectOwner,
                     public nsIDOMEventReceiver,
                     public nsIHTMLContent
                     //public nsIFormControl
{
public:
  nsHTMLOptionElement(nsIAtom* aTag);
  virtual ~nsHTMLOptionElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLOptionElement
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);
  NS_IMETHOD GetDefaultSelected(PRBool* aDefaultSelected);
  NS_IMETHOD SetDefaultSelected(PRBool aDefaultSelected);
  NS_IMETHOD GetText(nsString& aText);
  NS_IMETHOD GetIndex(PRInt32* aIndex);
  NS_IMETHOD SetIndex(PRInt32 aIndex);
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetLabel(nsString& aLabel);
  NS_IMETHOD SetLabel(const nsString& aLabel);
  NS_IMETHOD GetSelected(PRBool* aSelected);
  NS_IMETHOD GetValue(nsString& aValue);
  NS_IMETHOD SetValue(const nsString& aValue);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
  nsIForm* mForm;

  // Return the primary frame associated with this content
  nsresult GetPrimaryFrame(nsIFormControlFrame *&aFormControlFrame);
};

nsresult
NS_NewHTMLOptionElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLOptionElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLOptionElement::nsHTMLOptionElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mForm = nsnull;
}

nsHTMLOptionElement::~nsHTMLOptionElement()
{
  if (mForm) {
    NS_RELEASE(mForm);
  }
}

// ISupports

NS_IMETHODIMP
nsHTMLOptionElement::AddRef(void)
{
  return ++mRefCnt; 
}

nsresult
nsHTMLOptionElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLOptionElementIID)) {
    *aInstancePtr = (void*)(nsIDOMHTMLOptionElement*) this;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

// the option has a ref to the form, but not vice versa. The form can get to the
// options via the select.
NS_IMETHODIMP_(nsrefcnt)
nsHTMLOptionElement::Release()
{
  --mRefCnt;
	if (mRefCnt <= 0) {
    delete this;                                       
    return 0;                                          
  } else {
    return mRefCnt;
  }
}

nsresult
nsHTMLOptionElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLOptionElement* it = new nsHTMLOptionElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLOptionElement::GetForm(nsIDOMHTMLFormElement** aForm)
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
nsHTMLOptionElement::GetSelected(PRBool* aValue)
{
  nsIFormControlFrame* formControlFrame = nsnull;
  nsresult rv = GetPrimaryFrame(formControlFrame);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 index;
    if (NS_OK == GetIndex(&index)) {
      nsString value;
      value.Append(index, 10); // Save the index in base 10
      formControlFrame->GetProperty(nsHTMLAtoms::selected, value);
      if (value == "1")
        *aValue = PR_TRUE;
      else
        *aValue = PR_FALSE;
    }
    NS_RELEASE(formControlFrame);
  }
  return rv;      
}

//NS_IMPL_BOOL_ATTR(nsHTMLOptionElement, DefaultSelected, defaultselected)
NS_IMPL_INT_ATTR(nsHTMLOptionElement, Index, index)
NS_IMPL_BOOL_ATTR(nsHTMLOptionElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Label, label)
NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Value, value)

NS_IMETHODIMP 
nsHTMLOptionElement::GetDefaultSelected(PRBool* aDefaultSelected)
{
  nsHTMLValue val;                                                 
  nsresult rv = mInner.GetHTMLAttribute(nsHTMLAtoms::selected, val);       
  *aDefaultSelected = (NS_CONTENT_ATTR_NOT_THERE != rv);                        
  return NS_OK;                                                     
}

NS_IMETHODIMP
nsHTMLOptionElement::SetDefaultSelected(PRBool aDefaultSelected)
{
  nsHTMLValue empty(eHTMLUnit_Empty);
  if (aDefaultSelected) {
    return mInner.SetHTMLAttribute(nsHTMLAtoms::selected, empty, PR_TRUE);
  } else {
    mInner.UnsetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::selected, PR_TRUE);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLOptionElement::StringToAttribute(nsIAtom* aAttribute,
                                const nsString& aValue,
                                nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::selected) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::disabled) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLOptionElement::AttributeToString(nsIAtom* aAttribute,
                                const nsHTMLValue& aValue,
                                nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLOptionElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLOptionElement::HandleDOMEvent(nsIPresContext& aPresContext,
                             nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent,
                             PRUint32 aFlags,
                             nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLOptionElement::GetText(nsString& aText)
{
  aText.SetLength(0);
  PRInt32 numNodes;
  nsresult rv = mInner.ChildCount(numNodes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  for (PRInt32 i = 0; i < numNodes; i++) {
    nsIContent* node;
    rv = ChildAt(i, node);
    if (NS_SUCCEEDED(rv)) {
      nsIDOMText* domText = nsnull;
      rv = node->QueryInterface(kIDOMTextIID, (void**)&domText);
      if (NS_SUCCEEDED(rv) && domText) {
        rv = domText->GetData(aText);
        aText.CompressWhitespace(PR_TRUE, PR_TRUE);
        NS_RELEASE(domText);
        NS_RELEASE(node);
        break;
      }
      NS_RELEASE(node);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}

// Options don't have frames - get the select content node
// then call nsGenericHTMLElement::GetPrimaryFrame()
nsresult nsHTMLOptionElement::GetPrimaryFrame(nsIFormControlFrame *&aIFormControlFrame)
{
  // Get the containing element (Either a select or an optGroup)
  nsresult res = NS_NOINTERFACE;
  nsIDOMNode* parentNode = nsnull;
  nsresult gotParent = this->GetParentNode(&parentNode);
  if (NS_OK == gotParent) {
    nsIDOMHTMLSelectElement* selectElement = nsnull;
    nsresult isSelect = parentNode->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&selectElement);

    // If we are in an OptGroup we need to GetParentNode again (at least once)
    if (NS_OK != isSelect) {
      nsIDOMHTMLOptGroupElement* optgroupElement = nsnull;
      while (1) { // Be ready for nested OptGroups
        nsresult isOptGroup = parentNode->QueryInterface(kIDOMHTMLOptGroupElementIID, (void**)&optgroupElement);
        if (NS_OK == isOptGroup) {     // We don't really need the optgroup,
          NS_RELEASE(optgroupElement); // just seeing if it IS one.
          nsIDOMNode* grandParentNode = nsnull;
          gotParent = parentNode->GetParentNode(&grandParentNode);
          if (NS_OK == gotParent) {
            NS_RELEASE(parentNode);
            parentNode = grandParentNode;
          } else {
            break; // Break out if we can't get our parent (we're screwed)
          }
        } else {
          break; // Break out if not a OptGroup (hopefully we have a select)
        }
      }
      isSelect = parentNode->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&selectElement);
    }

    // We have a select if we're gonna get one, so let go of the generic node
    NS_RELEASE(parentNode);

    if (NS_OK == isSelect) {
      nsIHTMLContent* selectContent = nsnull;
      nsresult gotContent = selectElement->QueryInterface(kIContentIID, (void**)&selectContent);
      NS_RELEASE(selectElement);

      if (NS_OK == gotContent) {
        res = nsGenericHTMLElement::GetPrimaryFrame(selectContent, aIFormControlFrame);
        NS_RELEASE(selectContent);
      }
    }
  }
  return res;
}
