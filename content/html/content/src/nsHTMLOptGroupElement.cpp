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
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"

#include "nsISelectElement.h"
#include "nsIDOMHTMLSelectElement.h"


/**
 * The implementation of &lt;optgroup&gt;
 */
class nsHTMLOptGroupElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLOptGroupElement
{
public:
  nsHTMLOptGroupElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLOptGroupElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLOptGroupElement
  NS_DECL_NSIDOMHTMLOPTGROUPELEMENT

  // nsGenericElement
  virtual nsresult WillAddOrRemoveChild(nsIContent* aKid,
                                        PRUint32 aIndex,
                                        PRBool aRemove);

  // nsIContent
  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);

  virtual PRInt32 IntrinsicState() const;
 
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
  {
    nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                                aValue, aNotify);
      
    AfterSetAttr(aNameSpaceID, aName, &aValue, aNotify);
    return rv;
  }
 
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify)
  {
    nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                  aNotify);
      
    AfterSetAttr(aNameSpaceID, aAttribute, nsnull, aNotify);
    return rv;
  }

protected:

  /**
   * Get the select content element that contains this option
   * @param aSelectElement the select element [OUT]
   */
  void GetSelect(nsISelectElement **aSelectElement);
 
  /**
   * Called when an attribute has just been changed
   */
  void AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                    const nsAString* aValue, PRBool aNotify);
};


NS_IMPL_NS_NEW_HTML_ELEMENT(OptGroup)


nsHTMLOptGroupElement::nsHTMLOptGroupElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLOptGroupElement::~nsHTMLOptGroupElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLOptGroupElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLOptGroupElement, nsGenericElement)


// QueryInterface implementation for nsHTMLOptGroupElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLOptGroupElement,
                                    nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLOptGroupElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLOptGroupElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_DOM_CLONENODE(nsHTMLOptGroupElement)


NS_IMPL_BOOL_ATTR(nsHTMLOptGroupElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLOptGroupElement, Label, label)


nsresult
nsHTMLOptGroupElement::HandleDOMEvent(nsPresContext* aPresContext,
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

  nsIFrame* frame = GetPrimaryFrame(PR_FALSE);
  if (frame) {
    const nsStyleUserInterface* uiStyle = frame->GetStyleUserInterface();
    if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
      return NS_OK;
    }
  }

  return nsGenericHTMLElement::HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                              aFlags, aEventStatus);
}

void
nsHTMLOptGroupElement::GetSelect(nsISelectElement **aSelectElement)
{
  *aSelectElement = nsnull;
  for (nsIContent* parent = GetParent(); parent; parent = parent->GetParent()) {
    CallQueryInterface(parent, aSelectElement);
    if (*aSelectElement) {
      break;
    }
  }
}

void
nsHTMLOptGroupElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                    const nsAString* aValue, PRBool aNotify)
{
  if (aNotify && aNameSpaceID == kNameSpaceID_None &&
      aName == nsHTMLAtoms::disabled) {
    nsIDocument* document = GetCurrentDoc();
    if (document) {
      mozAutoDocUpdate(document, UPDATE_CONTENT_STATE, PR_TRUE);
      document->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_DISABLED |
                                     NS_EVENT_STATE_ENABLED);
    }
  }
}
 
nsresult
nsHTMLOptGroupElement::WillAddOrRemoveChild(nsIContent* aKid,
                                            PRUint32 aIndex,
                                            PRBool aRemove)
{
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    if (aRemove) {
      sel->WillRemoveOptions(this, aIndex);
    } else {
      sel->WillAddOptions(aKid, this, aIndex);
    }
  }

  return nsGenericHTMLElement::WillAddOrRemoveChild(aKid, aIndex, aRemove);
}

PRInt32
nsHTMLOptGroupElement::IntrinsicState() const
{
  PRInt32 state = nsGenericHTMLElement::IntrinsicState();
  PRBool disabled;
  GetBoolAttr(nsHTMLAtoms::disabled, &disabled);
  if (disabled) {
    state |= NS_EVENT_STATE_DISABLED;
    state &= ~NS_EVENT_STATE_ENABLED;
  } else {
    state &= ~NS_EVENT_STATE_DISABLED;
    state |= NS_EVENT_STATE_ENABLED;
  }

  return state;
}
