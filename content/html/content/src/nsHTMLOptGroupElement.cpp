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
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"

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

protected:

  /**
   * Get the select content element that contains this option
   * @param aSelectElement the select element [OUT]
   */
  void GetSelect(nsISelectElement **aSelectElement);
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


NS_IMPL_HTML_DOM_CLONENODE(OptGroup)


NS_IMPL_BOOL_ATTR(nsHTMLOptGroupElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLOptGroupElement, Label, label)


nsresult
nsHTMLOptGroupElement::HandleDOMEvent(nsIPresContext* aPresContext,
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
  if (formControlFrame) {
    CallQueryInterface(formControlFrame, &formFrame);
  }

  if (formFrame) {
    const nsStyleUserInterface* uiStyle = formFrame->GetStyleUserInterface();
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

// nsIContent
nsresult
nsHTMLOptGroupElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                     PRBool aDeepSetDocument)
{
  // Since we're appending, the relevant option index to add after is found
  // *after* this optgroup.
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    sel->WillAddOptions(aKid, this, GetChildCount());
  }

  // Actually perform the append
  return nsGenericHTMLElement::AppendChildTo(aKid, aNotify, aDeepSetDocument);
}

nsresult
nsHTMLOptGroupElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                     PRBool aNotify, PRBool aDeepSetDocument)
{
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    sel->WillAddOptions(aKid, this, aIndex);
  }

  return nsGenericHTMLElement::InsertChildAt(aKid, aIndex, aNotify,
                                             aDeepSetDocument);
}

nsresult
nsHTMLOptGroupElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    sel->WillRemoveOptions(this, aIndex);
  }

  return nsGenericHTMLElement::RemoveChildAt(aIndex, aNotify);
}
