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
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"

#include "nsISelectElement.h"
#include "nsIDOMHTMLSelectElement.h"


class nsHTMLOptGroupElement : public nsGenericHTMLContainerElement,
                              public nsIDOMHTMLOptGroupElement
{
public:
  nsHTMLOptGroupElement();
  virtual ~nsHTMLOptGroupElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLOptGroupElement
  NS_DECL_NSIDOMHTMLOPTGROUPELEMENT

  // nsIContent
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument);
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:

  nsresult GetSelect(nsISelectElement **aSelectElement);
};

nsresult
NS_NewHTMLOptGroupElement(nsIHTMLContent** aInstancePtrResult,
                          nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLOptGroupElement* it = new nsHTMLOptGroupElement();

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


nsHTMLOptGroupElement::nsHTMLOptGroupElement()
{
}

nsHTMLOptGroupElement::~nsHTMLOptGroupElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLOptGroupElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLOptGroupElement, nsGenericElement);


// QueryInterface implementation for nsHTMLOptGroupElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLOptGroupElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLOptGroupElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLOptGroupElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLOptGroupElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLOptGroupElement* it = new nsHTMLOptGroupElement();

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


NS_IMPL_BOOL_ATTR(nsHTMLOptGroupElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLOptGroupElement, Label, label)


NS_IMETHODIMP
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
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    {
      return NS_OK;
    }
  }

  return nsGenericHTMLContainerElement::HandleDOMEvent(aPresContext, aEvent,
                                                       aDOMEvent, aFlags,
                                                       aEventStatus);
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLOptGroupElement::SizeOf(nsISizeOfHandler* aSizer,
                              PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif


// Get the select content element that contains this option
nsresult
nsHTMLOptGroupElement::GetSelect(nsISelectElement **aSelectElement)
{
  *aSelectElement = nsnull;
  // Get the containing element (Either a select or an optGroup)
  nsCOMPtr<nsIContent> parent;
  nsCOMPtr<nsIContent> prevParent;
  GetParent(*getter_AddRefs(parent));
  while (parent) {
    CallQueryInterface(parent, aSelectElement);
    if (*aSelectElement) {
      break;
    }
    prevParent = parent;
    prevParent->GetParent(*getter_AddRefs(parent));
  }

  return NS_OK;
}

// nsIContent
NS_IMETHODIMP
nsHTMLOptGroupElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                     PRBool aDeepSetDocument)
{
  // Since we're appending, the relevant option index to add after is found
  // *after* this optgroup.
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    PRInt32 count;
    ChildCount(count);
    sel->WillAddOptions(aKid, this, count);
  }

  // Actually perform the append
  return nsGenericHTMLContainerElement::AppendChildTo(aKid,
                                                      aNotify,
                                                      aDeepSetDocument);
}

NS_IMETHODIMP
nsHTMLOptGroupElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                                     PRBool aNotify, PRBool aDeepSetDocument)
{
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    sel->WillAddOptions(aKid, this, aIndex);
  }

  return nsGenericHTMLContainerElement::InsertChildAt(aKid,
                                                      aIndex,
                                                      aNotify,
                                                      aDeepSetDocument);
}

NS_IMETHODIMP
nsHTMLOptGroupElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                                      PRBool aNotify, PRBool aDeepSetDocument)
{
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    sel->WillRemoveOptions(this, aIndex);
    sel->WillAddOptions(aKid, this, aIndex);
  }

  return nsGenericHTMLContainerElement::ReplaceChildAt(aKid,
                                                       aIndex,
                                                       aNotify,
                                                       aDeepSetDocument);
}

NS_IMETHODIMP
nsHTMLOptGroupElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsCOMPtr<nsISelectElement> sel;
  GetSelect(getter_AddRefs(sel));
  if (sel) {
    sel->WillRemoveOptions(this, aIndex);
  }

  nsresult rv = nsGenericHTMLContainerElement::RemoveChildAt(aIndex,
                                                             aNotify);

  return rv;
}
