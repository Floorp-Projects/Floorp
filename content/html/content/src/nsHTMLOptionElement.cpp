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
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLFormElement.h"
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
#include "nsITextContent.h"
#include "nsIDOMNode.h"
#include "nsGenericElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIJSNativeInitializer.h"
#include "nsISelectElement.h"
#include "nsISelectControlFrame.h"
#include "nsIComboboxControlFrame.h"

// Notify/query select frame for selected state
#include "nsIFormControlFrame.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"


class nsHTMLOptionElement : public nsGenericHTMLContainerElement,
                            public nsIDOMHTMLOptionElement,
                            public nsIJSNativeInitializer
{
public:
  nsHTMLOptionElement();
  virtual ~nsHTMLOptionElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLOptionElement
  NS_DECL_NSIDOMHTMLOPTIONELEMENT

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(JSContext* aContext, JSObject *aObj, 
                        PRUint32 argc, jsval *argv);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  // Get the primary frame associated with this content
  nsresult GetPrimaryFrame(nsIFormControlFrame *&aFormControlFrame,
                           PRBool aFlushNotifications = PR_TRUE);

  // Get the select content element that contains this option, this
  // intentionally does not return nsresult, all we care about is if
  // there's a select associated with this option or not.
  void GetSelect(nsIDOMHTMLSelectElement *&aSelectElement);
};

nsresult
NS_NewHTMLOptionElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  /*
   * nsHTMLOptionElement's will be created without a nsINodeInfo passed in
   * if someone creates new option elements in JavaScript, in a case like
   * that we request the nsINodeInfo from the anonymous nodeinfo list.
   */
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
    nsresult rv;
    rv = nsNodeInfoManager::GetAnonymousManager(*getter_AddRefs(nodeInfoManager));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nodeInfoManager->GetNodeInfo(nsHTMLAtoms::option, nsnull,
                                      kNameSpaceID_None,
                                      *getter_AddRefs(nodeInfo));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsIHTMLContent* it = new nsHTMLOptionElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_STATIC_CAST(nsGenericElement *, it)->Init(nodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLOptionElement::nsHTMLOptionElement()
{
}

nsHTMLOptionElement::~nsHTMLOptionElement()
{
}

// ISupports


NS_IMPL_ADDREF_INHERITED(nsHTMLOptionElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLOptionElement, nsGenericElement);


// QueryInterface implementation for nsHTMLOptionElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLOptionElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLOptionElement)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLOptionElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLOptionElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLOptionElement* it = new nsHTMLOptionElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = NS_STATIC_CAST(nsGenericElement *, it)->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
  GetSelect(*getter_AddRefs(selectElement));

  nsCOMPtr<nsIFormControl> selectControl(do_QueryInterface(selectElement));

  if (selectControl) {
    selectControl->GetForm(aForm);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLOptionElement::GetSelected(PRBool* aValue) 
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = PR_FALSE;

  nsIFormControlFrame* formControlFrame = nsnull;

  // DO NOT flush pending reflows here
  GetPrimaryFrame(formControlFrame, PR_FALSE);

  if (formControlFrame) {
    PRInt32 indx;

    GetIndex(&indx);

    if (indx >= 0) {
      nsAutoString value;

      value.AppendInt(indx, 10); // Save the index in base 10

      formControlFrame->GetProperty(nsHTMLAtoms::selected, value);

      *aValue = value.EqualsWithConversion("1");
    }
  } else {
    // Note: The select content obj maintains all the PresState
    // so defer to it to get the answer

    nsCOMPtr<nsIDOMNode> parentNode;
    GetParentNode(getter_AddRefs(parentNode));

    nsCOMPtr<nsISelectElement> selectElement(do_QueryInterface(parentNode));

    if (selectElement) {
      return selectElement->IsOptionSelected(this, aValue);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLOptionElement::SetSelected(PRBool aValue)
{
  nsIFormControlFrame* fcFrame = nsnull;

  // DO NOT flush pending reflows here
  nsresult result = GetPrimaryFrame(fcFrame, PR_FALSE);

  if (NS_SUCCEEDED(result) && fcFrame) {
    nsISelectControlFrame* selectFrame = nsnull;
    result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),
                                     (void **) &selectFrame);

    if (NS_SUCCEEDED(result) && (selectFrame)) {
      PRInt32 indx;

      GetIndex(&indx);

      if (indx >= 0) {
        // this will flush pending reflows
        return selectFrame->SetOptionSelected(indx, aValue);
      }
    }
  } else {
    // Note: The select content obj maintains all the PresState
    // so defer to it to get the answer
    nsCOMPtr<nsIDOMNode> parentNode;
    result = NS_OK;

    GetParentNode(getter_AddRefs(parentNode));

    nsCOMPtr<nsISelectElement> selectElement(do_QueryInterface(parentNode));

    if (selectElement) {
      return selectElement->SetOptionSelected(this, aValue);
    }
  }

  return result;
}

//NS_IMPL_BOOL_ATTR(nsHTMLOptionElement, DefaultSelected, defaultselected)
//NS_IMPL_INT_ATTR(nsHTMLOptionElement, Index, index)
//NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Label, label)
NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Value, value)

NS_IMETHODIMP                                                      
nsHTMLOptionElement::GetDisabled(PRBool* aDisabled)                             
{                                                                  
  nsHTMLValue val;                                                 
  nsresult rv = GetHTMLAttribute(nsHTMLAtoms::disabled, val);
  *aDisabled = (NS_CONTENT_ATTR_NOT_THERE != rv);
  return NS_OK;
}         
                                                         
NS_IMETHODIMP                                                      
nsHTMLOptionElement::SetDisabled(PRBool aDisabled)                       
{                                                                  
  nsresult rv = NS_OK;
  nsHTMLValue empty(eHTMLUnit_Empty);

  if (aDisabled) {
    rv = SetHTMLAttribute(nsHTMLAtoms::disabled, empty, PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      nsIFormControlFrame* fcFrame = nsnull;
      nsresult result = GetPrimaryFrame(fcFrame);
      if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
        nsISelectControlFrame* selectFrame = nsnull;

        result = fcFrame->QueryInterface(NS_GET_IID(nsISelectControlFrame),
                                         (void **)&selectFrame);

        if (NS_SUCCEEDED(result) && (nsnull != selectFrame)) {
          selectFrame->OptionDisabled(this);
        }
      }
    }
  } else {
    rv = UnsetAttr(kNameSpaceID_HTML, nsHTMLAtoms::selected, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP                                                      
nsHTMLOptionElement::GetLabel(nsAWritableString& aValue)
{                                                                  
  nsGenericHTMLContainerElement::GetAttr(kNameSpaceID_HTML,
                                         nsHTMLAtoms::label, aValue);
  return NS_OK;
}         
                                                         
NS_IMETHODIMP                                                      
nsHTMLOptionElement::SetLabel(const nsAReadableString& aValue)
{                                                                  
  nsresult result;

  result = nsGenericHTMLContainerElement::SetAttr(kNameSpaceID_HTML,
                                                  nsHTMLAtoms::label,
                                                  aValue, PR_TRUE); 
  if (NS_SUCCEEDED(result)) {
    nsIFormControlFrame* fcFrame = nsnull;

    result = GetPrimaryFrame(fcFrame);

    if (NS_SUCCEEDED(result) && (nsnull != fcFrame)) {
      nsIComboboxControlFrame* selectFrame = nsnull;

      result = fcFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame),
                                       (void **) &selectFrame);

      if (NS_SUCCEEDED(result) && selectFrame) {
        selectFrame->UpdateSelection(PR_FALSE, PR_TRUE, 0);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLOptionElement::GetDefaultSelected(PRBool* aDefaultSelected)
{
  nsHTMLValue val;                                                 

  nsresult rv = GetHTMLAttribute(nsHTMLAtoms::selected, val);
  *aDefaultSelected = (NS_CONTENT_ATTR_NOT_THERE != rv);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetDefaultSelected(PRBool aDefaultSelected)
{
  nsHTMLValue empty(eHTMLUnit_Empty);
  nsresult rv = NS_OK;

  if (aDefaultSelected) {
    rv = SetHTMLAttribute(nsHTMLAtoms::selected, empty, PR_TRUE);
  } else {
    rv = UnsetAttr(kNameSpaceID_HTML, nsHTMLAtoms::selected, PR_TRUE);
  }

  if (NS_SUCCEEDED(rv)) {
    // When setting DefaultSelected, we must also reset Selected (DOM Errata)
    rv = SetSelected(aDefaultSelected);
  }

  return rv;
}

NS_IMETHODIMP 
nsHTMLOptionElement::GetIndex(PRInt32* aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  *aIndex = -1; // -1 indicates the index was not found

  // Get our containing select content object.
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;

  GetSelect(*getter_AddRefs(selectElement));

  if (selectElement) {
    // Get the options from the select object.
    nsCOMPtr<nsIDOMHTMLCollection> options;

    selectElement->GetOptions(getter_AddRefs(options));

    if (options) {
      // Walk the options to find out where we are in the list (ick, O(n))
      PRUint32 length = 0;

      options->GetLength(&length);

      nsCOMPtr<nsIDOMNode> thisOption;

      for (PRUint32 i = 0; i < length; i++) {
        options->Item(i, getter_AddRefs(thisOption));

        if (thisOption.get() == NS_STATIC_CAST(nsIDOMNode *, this)) {
          *aIndex = i;

          break;
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::StringToAttribute(nsIAtom* aAttribute,
                                const nsAReadableString& aValue,
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
nsHTMLOptionElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                              PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::label) {
    aHint = NS_STYLE_HINT_REFLOW; 
  } else if (aAttribute == nsHTMLAtoms::text) {
    aHint = NS_STYLE_HINT_REFLOW; 
  } else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::GetText(nsAWritableString& aText)
{
  PRInt32 numNodes, i;

  aText.SetLength(0);

  nsresult rv = ChildCount(numNodes);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (i = 0; i < numNodes; i++) {
    nsCOMPtr<nsIContent> node;

    ChildAt(i, *getter_AddRefs(node));

    if (node) {
      nsCOMPtr<nsIDOMText> domText(do_QueryInterface(node));

      if (domText) {
        rv = domText->GetData(aText);

        nsAutoString text(aText);

        // the option could be all spaces, so compress the white space
        // then make sure it's not empty, if the option is all
        // whitespace then we return the whitespace

        text.CompressWhitespace(PR_TRUE, PR_TRUE);

        if (!text.IsEmpty()) {
          aText.Assign(text);
          break;
        }
      }

    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetText(const nsAReadableString& aText)
{
  PRInt32 numNodes, i;
  PRBool usedExistingTextNode = PR_FALSE;  // Do we need to create a text node?

  nsresult result = ChildCount(numNodes);

  if (NS_FAILED(result)) {
    return result;
  }

  for (i = 0; i < numNodes; i++) {
    nsCOMPtr<nsIContent> node;

    ChildAt(i, *getter_AddRefs(node));

    if (node) {
      nsCOMPtr<nsIDOMText> domText(do_QueryInterface(node));

      if (domText) {
        result = domText->SetData(aText);

        if (NS_SUCCEEDED(result)) {
          usedExistingTextNode = PR_TRUE;
        }

        break;
      }
    }
  }

  if (!usedExistingTextNode) {
    nsCOMPtr<nsIContent> text;
    result = NS_NewTextNode(getter_AddRefs(text));
    if (NS_OK == result) {
      nsCOMPtr<nsIDOMText> domtext(do_QueryInterface(text));

      if (domtext) {
        result = domtext->SetData(aText);

	    if (NS_SUCCEEDED(result)) {
          result = AppendChildTo(text, PR_TRUE, PR_FALSE);

          if (NS_SUCCEEDED(result)) {
            nsCOMPtr<nsIDocument> doc;

            result = GetDocument(*getter_AddRefs(doc));

            if (NS_SUCCEEDED(result)) {
              text->SetDocument(doc, PR_FALSE, PR_TRUE);
            }
          }
        }
      }
    }
  }

  if (NS_SUCCEEDED(result)) {
    nsIFormControlFrame* fcFrame = nsnull;
    result = GetPrimaryFrame(fcFrame);

    if (NS_SUCCEEDED(result) && fcFrame) {
      nsIComboboxControlFrame* selectFrame = nsnull;

      result = fcFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame),
                                       (void **)&selectFrame);

      if (NS_SUCCEEDED(result) && selectFrame) {
        selectFrame->UpdateSelection(PR_FALSE, PR_TRUE, 0);
      }
    }
  }

  return NS_OK;
}


// Options don't have frames - get the select content node
// then call nsGenericHTMLElement::GetPrimaryFrame()

nsresult
nsHTMLOptionElement::GetPrimaryFrame(nsIFormControlFrame *&aIFormControlFrame,
                                     PRBool aFlushNotifications)
{
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;

  nsresult res = NS_ERROR_FAILURE; // This should be NS_OK;

  GetSelect(*getter_AddRefs(selectElement));

  if (selectElement) {
    nsCOMPtr<nsIHTMLContent> selectContent(do_QueryInterface(selectElement));

    if (selectContent) {
      res = nsGenericHTMLElement::GetPrimaryFrame(selectContent,
                                                  aIFormControlFrame,
                                                  aFlushNotifications);
    }
  }

  return res;
}

// Get the select content element that contains this option
void
nsHTMLOptionElement::GetSelect(nsIDOMHTMLSelectElement *&aSelectElement)
{
  aSelectElement = nsnull;

  // Get the containing element (Either a select or an optGroup)
  nsCOMPtr<nsIDOMNode> parentNode;

  GetParentNode(getter_AddRefs(parentNode));

  if (parentNode) {
    nsresult res;
    res = parentNode->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                     (void**)&aSelectElement);

    // If we are in an OptGroup we need to GetParentNode again (at least once)
    if (NS_FAILED(res)) {
      nsCOMPtr<nsIDOMHTMLOptGroupElement> optgroupElement;

      while (parentNode) { // Be ready for nested OptGroups
        // Don't need the optgroupElement, just seeing if it IS one.
        optgroupElement = do_QueryInterface(parentNode);

        if (optgroupElement) {
          nsIDOMNode* tmpNode = parentNode.get();

          tmpNode->GetParentNode(getter_AddRefs(parentNode));
        } else {
          break; // Break out if not a OptGroup (hopefully we have a select)
        }
      }

      if (parentNode) {
        parentNode->QueryInterface(NS_GET_IID(nsIDOMHTMLSelectElement),
                                   (void**)&aSelectElement);
      }
    }
  }
}

NS_IMETHODIMP    
nsHTMLOptionElement::Initialize(JSContext* aContext, 
                                JSObject *aObj,
                                PRUint32 argc, 
                                jsval *argv)
{
  nsresult result = NS_OK;

  if (argc > 0) {
    // The first (optional) parameter is the text of the option
    JSString* jsstr = JS_ValueToString(aContext, argv[0]);
    if (jsstr) {
      // Create a new text node and append it to the option
      nsCOMPtr<nsIContent> content;

      result = NS_NewTextNode(getter_AddRefs(content));
      if (NS_FAILED(result)) {
        return result;
      }

      nsCOMPtr<nsITextContent> textContent(do_QueryInterface(content));

      if (!textContent) {
        return NS_ERROR_FAILURE;
      }

      result = textContent->SetText(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(jsstr)),
                                JS_GetStringLength(jsstr),
                                PR_FALSE);

      if (NS_FAILED(result)) {
        return result;
      }
      
      // this addrefs textNode:
      result = AppendChildTo(content, PR_FALSE, PR_FALSE);
      if (NS_FAILED(result)) {
        return result;
      }
    }

    if (argc > 1) {
      // The second (optional) parameter is the value of the option
      jsstr = JS_ValueToString(aContext, argv[1]);
      if (nsnull != jsstr) {
        // Set the value attribute for this element
        nsAutoString value(NS_REINTERPRET_CAST(const PRUnichar*,
                                               JS_GetStringChars(jsstr)));

        result = nsGenericHTMLContainerElement::SetAttr(kNameSpaceID_HTML,
                                                        nsHTMLAtoms::value,
                                                        value, PR_FALSE);
        if (NS_FAILED(result)) {
          return result;
        }
      }

      if (argc > 2) {
        // The third (optional) parameter is the defaultSelected value
        JSBool defaultSelected;
        if ((JS_TRUE == JS_ValueToBoolean(aContext,
                                         argv[2],
                                         &defaultSelected)) &&
            (JS_TRUE == defaultSelected)) {
          nsHTMLValue empty(eHTMLUnit_Empty);

          result = SetHTMLAttribute(nsHTMLAtoms::selected, empty, PR_FALSE);

          if (NS_FAILED(result)) {
            return result;
          }          
        }

        // XXX Since we don't store the selected state, we can't deal
        // with the fourth (optional) parameter that is meant to specify
        // whether the option element should be currently selected or
        // not. Does anyone depend on this behavior?
      }
    }
  }

  return result;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLOptionElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
