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
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMNSHTMLOptionElement.h"
#include "nsIOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
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

// Notify/query select frame for selected state
#include "nsIFormControlFrame.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "nsLayoutAtoms.h"
#include "nsIEventStateManager.h"
#include "nsXULAtoms.h"
#include "nsIDOMDocument.h"

/**
 * Implementation of &lt;option&gt;
 */
class nsHTMLOptionElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLOptionElement,
                            public nsIDOMNSHTMLOptionElement,
                            public nsIJSNativeInitializer,
                            public nsIOptionElement
{
public:
  nsHTMLOptionElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLOptionElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLOptionElement
  NS_DECL_NSIDOMHTMLOPTIONELEMENT

  // nsIDOMNSHTMLOptionElement
  NS_IMETHOD SetText(const nsAString & aText); 

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(JSContext* aContext, JSObject *aObj, 
                        PRUint32 argc, jsval *argv);

  NS_IMETHOD GetAttributeChangeHint(const nsIAtom* aAttribute,
                                    PRInt32 aModType,
                                    nsChangeHint& aHint) const;

  // nsIOptionElement
  NS_IMETHOD SetSelectedInternal(PRBool aValue, PRBool aNotify);

  // nsIContent
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify, PRBool aDeepSetDocument);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);

protected:
  /**
   * Get the primary frame associated with this content
   * @return the primary frame associated with this content
   */
  nsIFormControlFrame *GetSelectFrame() const;

  /**
   * Get the select content element that contains this option, this
   * intentionally does not return nsresult, all we care about is if
   * there's a select associated with this option or not.
   * @param aSelectElement the select element (out param)
   */
  void GetSelect(nsIDOMHTMLSelectElement **aSelectElement) const;

  /**
   * Notify the frame that the option text or value or label has changed.
   */
  void NotifyTextChanged();

  PRPackedBool mIsInitialized;
  PRPackedBool mIsSelected;
};

nsIHTMLContent*
NS_NewHTMLOptionElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
{
  /*
   * nsHTMLOptionElement's will be created without a nsINodeInfo passed in
   * if someone says "var opt = new Option();" in JavaScript, in a case like
   * that we request the nsINodeInfo from the document's nodeinfo list.
   */
  nsresult rv;
  nsCOMPtr<nsINodeInfo> nodeInfo(aNodeInfo);
  if (!nodeInfo) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    NS_ENSURE_TRUE(doc, nsnull);

    rv = doc->NodeInfoManager()->GetNodeInfo(nsHTMLAtoms::option, nsnull,
                                             kNameSpaceID_None,
                                             getter_AddRefs(nodeInfo));
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  return new nsHTMLOptionElement(nodeInfo);
}

nsHTMLOptionElement::nsHTMLOptionElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mIsInitialized(PR_FALSE),
    mIsSelected(PR_FALSE)
{
}

nsHTMLOptionElement::~nsHTMLOptionElement()
{
}

// ISupports


NS_IMPL_ADDREF_INHERITED(nsHTMLOptionElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLOptionElement, nsGenericElement)


// QueryInterface implementation for nsHTMLOptionElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLOptionElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLOptionElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLOptionElement)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIOptionElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLOptionElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Option)


NS_IMETHODIMP
nsHTMLOptionElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  NS_ENSURE_ARG_POINTER(aForm);
  *aForm = nsnull;

  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
  GetSelect(getter_AddRefs(selectElement));

  nsCOMPtr<nsIFormControl> selectControl(do_QueryInterface(selectElement));

  if (selectControl) {
    selectControl->GetForm(aForm);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetSelectedInternal(PRBool aValue, PRBool aNotify)
{
  mIsInitialized = PR_TRUE;
  mIsSelected = aValue;

  if (aNotify && mDocument) {
    mozAutoDocUpdate(mDocument, UPDATE_CONTENT_STATE, aNotify);
    mDocument->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_CHECKED);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetValue(const nsAString& aValue)
{
  SetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aValue, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::GetValue(nsAString& aValue)
{
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, aValue);
  // If the value attr is there, that is *exactly* what we use.  If it is
  // not, we compress whitespace .text.
  if (NS_CONTENT_ATTR_NOT_THERE == rv) {
    GetText(aValue);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLOptionElement::GetSelected(PRBool* aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = PR_FALSE;

  // If it's not initialized, initialize it.
  if (!mIsInitialized) {
    mIsInitialized = PR_TRUE;
    PRBool selected;
    GetDefaultSelected(&selected);
    // This does not need to be SetSelected (which sets selected in the select)
    // because we *will* be initialized when we are placed into a select.  Plus
    // it seems like that's just inviting an infinite loop.
    // We can pass |aNotify == PR_FALSE| since |GetSelected| is called
    // from |nsHTMLSelectElement::InsertOptionsIntoList|, which is
    // guaranteed to be called before frames are created for the
    // content.
    SetSelectedInternal(selected, PR_FALSE);
  }

  *aValue = mIsSelected;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetSelected(PRBool aValue)
{
  // Note: The select content obj maintains all the PresState
  // so defer to it to get the answer
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;
  GetSelect(getter_AddRefs(selectElement));
  nsCOMPtr<nsISelectElement> selectInt(do_QueryInterface(selectElement));
  if (selectInt) {
    PRInt32 index;
    GetIndex(&index);
    // This should end up calling SetSelectedInternal
    return selectInt->SetOptionsSelectedByIndex(index, index, aValue,
                                                PR_FALSE, PR_TRUE, PR_TRUE,
                                                nsnull);
  } else {
    return SetSelectedInternal(aValue, PR_TRUE);
  }

  return NS_OK;
}

NS_IMPL_BOOL_ATTR(nsHTMLOptionElement, DefaultSelected, selected)
NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Label, label)
//NS_IMPL_STRING_ATTR(nsHTMLOptionElement, Value, value)
NS_IMPL_BOOL_ATTR(nsHTMLOptionElement, Disabled, disabled)

NS_IMETHODIMP 
nsHTMLOptionElement::GetIndex(PRInt32* aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  *aIndex = -1; // -1 indicates the index was not found

  // Get our containing select content object.
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;

  GetSelect(getter_AddRefs(selectElement));

  if (selectElement) {
    // Get the options from the select object.
    nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
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
nsHTMLOptionElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            PRInt32 aModType,
                                            nsChangeHint& aHint) const
{
  nsresult rv =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType, aHint);

  if (aAttribute == nsHTMLAtoms::label ||
      aAttribute == nsHTMLAtoms::text) {
    NS_UpdateHint(aHint, NS_STYLE_HINT_REFLOW);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLOptionElement::GetText(nsAString& aText)
{
  PRUint32 i, numNodes = GetChildCount();

  aText.Truncate();

  nsAutoString text;
  for (i = 0; i < numNodes; i++) {
    nsCOMPtr<nsIDOMText> domText(do_QueryInterface(GetChildAt(i)));

    if (domText) {
      nsresult rv = domText->GetData(text);
      if (NS_FAILED(rv)) {
        aText.Truncate();
        return rv;
      }

      aText.Append(text);
    }
  }

  // XXX No CompressWhitespace for nsAString.  Sad.
  text = aText;
  text.CompressWhitespace(PR_TRUE, PR_TRUE);
  aText = text;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLOptionElement::SetText(const nsAString& aText)
{
  PRUint32 i, numNodes = GetChildCount();
  PRBool usedExistingTextNode = PR_FALSE;  // Do we need to create a text node?
  nsresult rv = NS_OK;

  for (i = 0; i < numNodes; i++) {
    nsCOMPtr<nsIDOMText> domText(do_QueryInterface(GetChildAt(i)));

    if (domText) {
      rv = domText->SetData(aText);

      if (NS_SUCCEEDED(rv)) {
        // If we used an existing node, the notification will not happen (the
        // notification typically happens in AppendChildTo).
        NotifyTextChanged();
        usedExistingTextNode = PR_TRUE;
      }

      break;
    }
  }

  if (!usedExistingTextNode) {
    nsCOMPtr<nsITextContent> text;
    rv = NS_NewTextNode(getter_AddRefs(text));
    NS_ENSURE_SUCCESS(rv, rv);

    text->SetText(aText, PR_TRUE);

    rv = AppendChildTo(text, PR_TRUE, PR_FALSE);
  }

  return rv;
}

void
nsHTMLOptionElement::NotifyTextChanged()
{
  // No need to flush here, if there's no frame yet we don't need to
  // force it to be created just to update the selection in it.
  nsIFormControlFrame* fcFrame = GetSelectFrame();

  if (fcFrame) {
    nsISelectControlFrame* selectFrame = nsnull;

    CallQueryInterface(fcFrame, &selectFrame);

    if (selectFrame) {
      selectFrame->OnOptionTextChanged(this);
    }
  }
}

nsresult
nsHTMLOptionElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  if (NS_SUCCEEDED(rv) && aNotify && aName == nsHTMLAtoms::label &&
      aNameSpaceID == kNameSpaceID_None) {
    // XXX Why does this only happen to the combobox?  and what about
    // when the text gets set and label is blank?
    NotifyTextChanged();
  }

  return rv;
}

//
// Override nsIContent children changing methods so we can detect when our text
// is changing
//
nsresult
nsHTMLOptionElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                   PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv = nsGenericHTMLElement::InsertChildAt(aKid, aIndex, aNotify,
                                                    aDeepSetDocument);
  NotifyTextChanged();
  return rv;
}

nsresult
nsHTMLOptionElement::AppendChildTo(nsIContent* aKid, PRBool aNotify, PRBool aDeepSetDocument)
{
  nsresult rv = nsGenericHTMLElement::AppendChildTo(aKid, aNotify,
                                                    aDeepSetDocument);
  NotifyTextChanged();
  return rv;
}

nsresult
nsHTMLOptionElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsresult rv = nsGenericHTMLElement::RemoveChildAt(aIndex, aNotify);
  NotifyTextChanged();
  return rv;
}


// Options don't have frames - get the select content node
// then call nsGenericHTMLElement::GetFormControlFrameFor()

nsIFormControlFrame *
nsHTMLOptionElement::GetSelectFrame() const
{
  if (!GetParent() || !mDocument) {
    return nsnull;
  }

  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement;

  GetSelect(getter_AddRefs(selectElement));

  nsCOMPtr<nsIContent> selectContent(do_QueryInterface(selectElement));

  if (!selectContent) {
    return nsnull;
  }

  return GetFormControlFrameFor(selectContent, mDocument, PR_FALSE);
}

// Get the select content element that contains this option
void
nsHTMLOptionElement::GetSelect(nsIDOMHTMLSelectElement **aSelectElement) const
{
  *aSelectElement = nsnull;

  for (nsIContent* parent = GetParent(); parent; parent = parent->GetParent()) {
    CallQueryInterface(parent, aSelectElement);
    if (*aSelectElement) {
      break;
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
      nsCOMPtr<nsITextContent> textContent;

      result = NS_NewTextNode(getter_AddRefs(textContent));
      if (NS_FAILED(result)) {
        return result;
      }

      textContent->SetText(NS_REINTERPRET_CAST(const PRUnichar*,
                                               JS_GetStringChars(jsstr)),
                           JS_GetStringLength(jsstr),
                           PR_FALSE);
      
      result = AppendChildTo(textContent, PR_FALSE, PR_FALSE);
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

        result = SetAttr(kNameSpaceID_None, nsHTMLAtoms::value, value,
                         PR_FALSE);
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
          result = SetAttr(kNameSpaceID_None, nsHTMLAtoms::selected,
                           EmptyString(), PR_FALSE);
          NS_ENSURE_SUCCESS(result, result);
        }

        // XXX This is *untested* behavior.  Should work though.
        if (argc > 3) {
          JSBool selected;
          if (JS_TRUE == JS_ValueToBoolean(aContext,
                                           argv[3],
                                           &selected)) {
            return SetSelected(selected);
          }
        }
      }
    }
  }

  return result;
}
