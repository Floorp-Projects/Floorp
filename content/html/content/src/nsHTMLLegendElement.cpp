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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIEventStateManager.h"
#include "nsIFocusController.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"


class nsHTMLLegendElement : public nsGenericHTMLFormElement,
                            public nsIDOMHTMLLegendElement
{
public:
  nsHTMLLegendElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLLegendElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLLegendElement
  NS_DECL_NSIDOMHTMLLEGENDELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRInt32) GetType() const { return NS_FORM_LEGEND; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual void SetFocus(nsPresContext* aPresContext);
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  PRPackedBool mInSetFocus;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Legend)


nsHTMLLegendElement::nsHTMLLegendElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFormElement(aNodeInfo)
  , mInSetFocus(PR_FALSE)
{
}

nsHTMLLegendElement::~nsHTMLLegendElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLLegendElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLLegendElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLLegendElement
NS_INTERFACE_TABLE_HEAD(nsHTMLLegendElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLLegendElement, nsIDOMHTMLLegendElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLLegendElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLLegendElement)


// nsIDOMHTMLLegendElement


NS_IMPL_ELEMENT_CLONE(nsHTMLLegendElement)


NS_IMETHODIMP
nsHTMLLegendElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}


NS_IMPL_STRING_ATTR(nsHTMLLegendElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLLegendElement, Align, align)

// this contains center, because IE4 does
static const nsAttrValue::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { "bottom", NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { "top", NS_STYLE_VERTICAL_ALIGN_TOP },
  { 0 }
};

PRBool
nsHTMLLegendElement::ParseAttribute(PRInt32 aNamespaceID,
                                    nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsGkAtoms::align && aNamespaceID == kNameSpaceID_None) {
    return aResult.ParseEnumValue(aValue, kAlignTable);
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

nsChangeHint
nsHTMLLegendElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                            PRInt32 aModType) const
{
  nsChangeHint retval =
      nsGenericHTMLFormElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::align) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
}

nsresult
nsHTMLLegendElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  PRBool accesskey = (aAttribute == nsGkAtoms::accesskey &&
                      aNameSpaceID == kNameSpaceID_None);
  if (accesskey) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsresult rv = nsGenericHTMLFormElement::SetAttr(aNameSpaceID, aAttribute,
                                                  aPrefix, aValue, aNotify);

  if (accesskey && !aValue.IsEmpty()) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

nsresult
nsHTMLLegendElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                               PRBool aNotify)
{
  if (aAttribute == nsGkAtoms::accesskey &&
      aNameSpaceID == kNameSpaceID_None) {
    RegUnRegAccessKey(PR_FALSE);
  }

  return nsGenericHTMLFormElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

nsresult
nsHTMLLegendElement::Reset()
{
  return NS_OK;
}

nsresult
nsHTMLLegendElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    RegUnRegAccessKey(PR_TRUE);
  }

  return rv;
}

void
nsHTMLLegendElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  if (IsInDoc()) {
    RegUnRegAccessKey(PR_FALSE);
  }

  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);
}

void
nsHTMLLegendElement::SetFocus(nsPresContext* aPresContext)
{
  nsIDocument *document = GetCurrentDoc();
  if (!aPresContext || !document || mInSetFocus) {
    return;
  }

  mInSetFocus = PR_TRUE;
  if (IsFocusable()) {
    nsGenericHTMLFormElement::SetFocus(aPresContext);
  } else {
    // If the legend isn't focusable (no tabindex) we focus whatever is
    // focusable following the legend instead, bug 81481.
    nsCOMPtr<nsPIDOMWindow> ourWindow = document->GetWindow();
    if (ourWindow) {
      nsIFocusController* focusController =
        ourWindow->GetRootFocusController();
      nsCOMPtr<nsIDOMElement> domElement =
        do_QueryInterface(static_cast<nsIContent *>(this));
      if (focusController && domElement) {
        focusController->MoveFocus(PR_TRUE, domElement);
      }
    }
  }
  mInSetFocus = PR_FALSE;
}

NS_IMETHODIMP
nsHTMLLegendElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                       nsIContent* aSubmitElement)
{
  return NS_OK;
}
