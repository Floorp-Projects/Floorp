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
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsISizeOfHandler.h"


class nsHTMLFieldSetElement : public nsGenericHTMLContainerFormElement,
                              public nsIDOMHTMLFieldSetElement
{
public:
  nsHTMLFieldSetElement();
  virtual ~nsHTMLFieldSetElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerFormElement::)

  // nsIDOMHTMLFieldSetElement
  NS_IMETHOD GetForm(nsIDOMHTMLFormElement** aForm);

  // nsIFormControl
  NS_IMETHOD GetType(PRInt32* aType);
  NS_IMETHOD Reset();
  NS_IMETHOD IsSuccessful(nsIContent* aSubmitElement, PRBool *_retval);
  NS_IMETHOD GetMaxNumValues(PRInt32 *_retval);
  NS_IMETHOD GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                            nsString* aValues, nsString* aNames);

#ifdef DEBUG
  // nsIContent
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
};

// construction, destruction

nsresult
NS_NewHTMLFieldSetElement(nsIHTMLContent** aInstancePtrResult,
                          nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLFieldSetElement* it = new nsHTMLFieldSetElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_STATIC_CAST(nsGenericElement *, it)->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLFieldSetElement::nsHTMLFieldSetElement()
{
}

nsHTMLFieldSetElement::~nsHTMLFieldSetElement()
{
  // Null out form's pointer to us - no ref counting here!
  SetForm(nsnull);
}

// nsISupports

NS_IMPL_ADDREF_INHERITED(nsHTMLFieldSetElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLFieldSetElement, nsGenericElement);


// QueryInterface implementation for nsHTMLFieldSetElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLFieldSetElement,
                                    nsGenericHTMLContainerFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFieldSetElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFieldSetElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


// nsIDOMHTMLFieldSetElement

nsresult
nsHTMLFieldSetElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLFieldSetElement* it = new nsHTMLFieldSetElement();

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


// nsIContent

NS_IMETHODIMP
nsHTMLFieldSetElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLContainerFormElement::GetForm(aForm);
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLFieldSetElement::GetType(PRInt32* aType)
{
  if (aType) {
    *aType = NS_FORM_FIELDSET;
    return NS_OK;
  } else {
    return NS_FORM_NOTOK;
  }
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLFieldSetElement::SizeOf(nsISizeOfHandler* aSizer,
                              PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif

nsresult
nsHTMLFieldSetElement::Reset()
{
  return NS_OK;
}

nsresult
nsHTMLFieldSetElement::IsSuccessful(nsIContent* aSubmitElement,
                                    PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

nsresult
nsHTMLFieldSetElement::GetMaxNumValues(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

nsresult
nsHTMLFieldSetElement::GetNamesValues(PRInt32 aMaxNumValues,
                                      PRInt32& aNumValues,
                                      nsString* aValues,
                                      nsString* aNames)
{
  aNumValues = 0;
  return NS_OK;
}
