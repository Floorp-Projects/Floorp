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
 */
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
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsDOMEventsIIDs.h"
#include "nsIDocument.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLAttributes.h"


class nsHTMLUnknownElement : public nsGenericHTMLContainerElement,
                             public nsIDOMHTMLElement
{
public:
  nsHTMLUnknownElement();
  virtual ~nsHTMLUnknownElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsAReadableString& aValue, PRBool aNotify);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
};

nsresult
NS_NewHTMLUnknownElement(nsIHTMLContent** aInstancePtrResult,
                         nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLUnknownElement* it = new nsHTMLUnknownElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLUnknownElement::nsHTMLUnknownElement()
{
}

nsHTMLUnknownElement::~nsHTMLUnknownElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLUnknownElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLUnknownElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI0(nsHTMLUnknownElement,
                        nsGenericHTMLContainerElement);


nsresult
nsHTMLUnknownElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLUnknownElement* it = new nsHTMLUnknownElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet *sheet;

  if (aDocument) {
    nsCOMPtr<nsIHTMLContentContainer> container(do_QueryInterface(aDocument));

    container->GetAttributeStyleSheet(&sheet);
  }

  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");

  return sheet;
}

static nsresult
EnsureWritableAttributes(nsIHTMLContent* aContent,
                         nsIHTMLAttributes*& aAttributes, PRBool aCreate)
{
  nsresult  result = NS_OK;

  if (!aAttributes) {
    if (PR_TRUE == aCreate) {
      result = NS_NewHTMLAttributes(&aAttributes);
    }
  }

  return result;
}

static void ReleaseAttributes(nsIHTMLAttributes*& aAttributes)
{
//  aAttributes->ReleaseContentRef();
  NS_RELEASE(aAttributes);
}


NS_IMETHODIMP
nsHTMLUnknownElement::SetAttribute(PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   const nsAReadableString& aValue,
                                   PRBool aNotify)
{
  nsresult  result = NS_OK;
  NS_ASSERTION((kNameSpaceID_HTML == aNameSpaceID) || 
               (kNameSpaceID_None == aNameSpaceID) || 
               (kNameSpaceID_Unknown == aNameSpaceID), 
               "html content only holds HTML attributes");

  if ((kNameSpaceID_HTML != aNameSpaceID) && 
      (kNameSpaceID_None != aNameSpaceID) &&
      (kNameSpaceID_Unknown != aNameSpaceID)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // Check for event handlers
  if ((nsLayoutAtoms::onclick == aAttribute) || 
      (nsLayoutAtoms::ondblclick == aAttribute) ||
      (nsLayoutAtoms::onmousedown == aAttribute) ||
      (nsLayoutAtoms::onmouseup == aAttribute) ||
      (nsLayoutAtoms::onmouseover == aAttribute) ||
      (nsLayoutAtoms::onmouseout == aAttribute)) {
    AddScriptEventListener(aAttribute, aValue, kIDOMMouseListenerIID); 
  } else if ((nsLayoutAtoms::onkeydown == aAttribute) ||
             (nsLayoutAtoms::onkeyup == aAttribute) ||
             (nsLayoutAtoms::onkeypress == aAttribute)) {
    AddScriptEventListener(aAttribute, aValue, kIDOMKeyListenerIID); 
  } else if (nsLayoutAtoms::onmousemove == aAttribute) {
    AddScriptEventListener(aAttribute, aValue, kIDOMMouseMotionListenerIID); 
  } else if (nsLayoutAtoms::onload == aAttribute) {
    AddScriptEventListener(nsLayoutAtoms::onload, aValue,
                           kIDOMLoadListenerIID);
  } else if ((nsLayoutAtoms::onunload == aAttribute) ||
             (nsLayoutAtoms::onabort == aAttribute) ||
             (nsLayoutAtoms::onerror == aAttribute)) {
    AddScriptEventListener(aAttribute, aValue, kIDOMLoadListenerIID); 
  } else if ((nsLayoutAtoms::onfocus == aAttribute) ||
             (nsLayoutAtoms::onblur == aAttribute)) {
    AddScriptEventListener(aAttribute, aValue, kIDOMFocusListenerIID); 
  } else if ((nsLayoutAtoms::onsubmit == aAttribute) ||
             (nsLayoutAtoms::onreset == aAttribute) ||
             (nsLayoutAtoms::onchange == aAttribute) ||
             (nsLayoutAtoms::onselect == aAttribute)) {
    AddScriptEventListener(aAttribute, aValue, kIDOMFormListenerIID); 
  } else if (nsLayoutAtoms::onpaint == aAttribute) {
    AddScriptEventListener(aAttribute, aValue, kIDOMPaintListenerIID); 
  } else if (nsLayoutAtoms::oninput == aAttribute) {
    AddScriptEventListener(aAttribute, aValue, kIDOMFormListenerIID);
  }
 
  nsHTMLValue val;
  
  if (NS_CONTENT_ATTR_NOT_THERE !=
      StringToAttribute(aAttribute, aValue, val)) {
    // string value was mapped to nsHTMLValue, set it that way
    result = SetHTMLAttribute(aAttribute, val, aNotify);

    return result;
  } else {
    if (ParseCommonAttribute(aAttribute, aValue, val)) {
      // string value was mapped to nsHTMLValue, set it that way
      result = SetHTMLAttribute(aAttribute, val, aNotify);
      return result;
    }

    if (aValue.IsEmpty()) { // if empty string
      val.SetEmptyValue();
      result = SetHTMLAttribute(aAttribute, val, aNotify);
      return result;
    }

    if (aNotify && (mDocument)) {
      mDocument->BeginUpdate();
    }

    // set as string value to avoid another string copy
    PRBool  impact = NS_STYLE_HINT_NONE;
    GetMappedAttributeImpact(aAttribute, impact);

    if (mDocument) {  // set attr via style sheet
      nsCOMPtr<nsIHTMLStyleSheet> sheet(dont_AddRef(GetAttrStyleSheet(mDocument)));

      if (sheet) {
        result = sheet->SetAttributeFor(aAttribute, aValue, 
                                        (NS_STYLE_HINT_CONTENT < impact), 
                                        this, mAttributes);
      }
    } else {  // manage this ourselves and re-sync when we connect to doc
      result = EnsureWritableAttributes(this, mAttributes, PR_TRUE);

      if (mAttributes) {
        PRInt32   count;
        result = mAttributes->SetAttributeFor(aAttribute, aValue, 
                                              (NS_STYLE_HINT_CONTENT < impact),
                                              this, nsnull, count);
        if (0 == count) {
          ReleaseAttributes(mAttributes);
        }
      }
    }
  }

  if (aNotify && (mDocument)) {
    result = mDocument->AttributeChanged(this, aNameSpaceID, aAttribute,
                                         NS_STYLE_HINT_UNKNOWN);
    mDocument->EndUpdate();
  }

  return result;
}

NS_IMETHODIMP
nsHTMLUnknownElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
