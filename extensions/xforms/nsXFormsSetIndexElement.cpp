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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
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

#include <math.h>

#include "nsIDOMDocument.h"
#include "nsIDOMXPathResult.h"

#include "nsIXFormsRepeatElement.h"
#include "nsXFormsActionModuleBase.h"
#include "nsXFormsActionElement.h"

#ifdef DEBUG
//#define DEBUG_XF_SETINDEX
#endif

/**
 * Implementation of the XForms \<setindex\> element.
 *
 * To set the index for a \<repeat\>, we find the (DOM) \<repeat\> with the
 * given @id, and call nsIXFormsRepeatElement::SetIndex() on it.
 *
 * @see http://www.w3.org/TR/xforms/slice9.html#action-setRepeatCursor
 */
class nsXFormsSetIndexElement : public nsXFormsActionModuleBase
{
public:
  NS_DECL_NSIXFORMSACTIONMODULEELEMENT
};

NS_IMETHODIMP
nsXFormsSetIndexElement::HandleAction(nsIDOMEvent            *aEvent,
                                      nsIXFormsActionElement *aParentAction)
{
  if (!mElement)
    return NS_OK;

  
  // Get @repeat and @index
  nsAutoString id, index;
  NS_NAMED_LITERAL_STRING(repeatStr, "repeat");
  mElement->GetAttribute(repeatStr, id);
  NS_NAMED_LITERAL_STRING(indexStr, "index");
  mElement->GetAttribute(indexStr, index);
  if (id.IsEmpty() || index.IsEmpty()) {
    /// @todo Should we dispatch an exception, or just define that this does
    /// not happen as it is a non-validating form as both are required
    /// attributes, and index should be an integer? (XXX)
    return NS_ERROR_ABORT;
  }


  // Find index (XPath) value
  nsresult rv;
  nsCOMPtr<nsIModelElementPrivate> model;
  nsCOMPtr<nsIDOMXPathResult> result;
  // EvaluateNodeBinding uses @bind if @index is not present, but we have
  // checked that @index is present above
  rv = nsXFormsUtils::EvaluateNodeBinding(mElement,
                                          0,
                                          indexStr,
                                          EmptyString(),
                                          nsIDOMXPathResult::NUMBER_TYPE,
                                          getter_AddRefs(model),
                                          getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!result) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("indexEvalError"),
                               mElement);
    return NS_OK;
  }
  double indexDoub;
  rv = result->GetNumberValue(&indexDoub);
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 indexInt = indexDoub < 1 ? 0 : (PRUint32) floor(indexDoub);

  // Find the \<repeat\> with @id == |id|
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_XF_SETINDEX
  printf("<setindex>: Setting index '%s' to '%d'\n",
         NS_ConvertUCS2toUTF8(id).get(),
         indexInt);
#endif  
  nsCOMPtr<nsIDOMElement> repeatElem;
  rv = domDoc->GetElementById(id, getter_AddRefs(repeatElem));
  nsCOMPtr<nsIXFormsRepeatElement> repeat = do_QueryInterface(repeatElem);
  if (!repeat) {
    const PRUnichar *strings[] = { id.get(), repeatStr.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("idRefError"),
                               strings, 2, mElement, mElement);
    return NS_OK;
  }

  // Set the index = |indexInt|
  return repeat->SetIndex(&indexInt, PR_FALSE);
}

NS_HIDDEN_(nsresult)
NS_NewXFormsSetIndexElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsSetIndexElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

