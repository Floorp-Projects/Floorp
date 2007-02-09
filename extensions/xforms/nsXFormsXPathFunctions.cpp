/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Aaron Reed <aaronr@us.ibm.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXFormsXPathFunctions.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsString.h"
#include "nsXFormsUtils.h"
#include "prprf.h"
#include "txDouble.h"
#include "txIFunctionEvaluationContext.h"
#include "txINodeSet.h"
#include "nsIClassInfoImpl.h"

#define NS_NAMESPACE_XFORMS "http://www.w3.org/2002/xforms"

static const PRUint32 nanMask[2] = TX_DOUBLE_NaN;
#define kNaN *((double*)nanMask)

NS_IMPL_ISUPPORTS1_CI(nsXFormsXPathFunctions, nsIXFormsXPathFunctions)

NS_IMETHODIMP
nsXFormsXPathFunctions::Avg(txINodeSet *aNodeSet, double *aResult)
{
    PRUint32 length;
    nsresult rv = aNodeSet->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    double total = 0;
    PRUint32 i;
    for (i = 0; i < length; ++i) {
        double item;
        rv = aNodeSet->ItemAsNumber(i, &item);
        NS_ENSURE_SUCCESS(rv, rv);

        if (TX_DOUBLE_IS_NaN(item)) {
            // This will make aResult be set to kNaN below.
            i = 0;
            break;
        }

        total += item;
    }

    *aResult = (i > 0) ? (total / i) : kNaN;

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::BooleanFromString(const nsAString & aString,
                                          PRBool *aResult)
{
    *aResult = aString.EqualsLiteral("1") ||
               aString.LowerCaseEqualsLiteral("true");

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::CountNonEmpty(txINodeSet *aNodeSet, double *aResult)
{
    PRUint32 length;
    nsresult rv = aNodeSet->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    double result = 0;
    PRUint32 i;
    for (i = 0; i < length; ++i) {
        nsAutoString item;
        rv = aNodeSet->ItemAsString(i, item);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!item.IsEmpty()) {
            ++result;
        }
    }

    *aResult = result;

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::DaysFromDate(const nsAString &aDateTime,
                                     double *aResult)
{
    PRInt32 result = 0;
    nsresult rv = nsXFormsUtils::GetDaysFromDateTime(aDateTime, &result);
    if (rv == NS_ERROR_ILLEGAL_VALUE) {
        *aResult = kNaN;
        rv = NS_OK;
    }
    else {
        *aResult = result;
    }

    return rv;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::If(PRBool aValue, const nsAString &aIfString,
                           const nsAString &aElseString, nsAString &aResult)
{
    // XXX Avoid evaluating aIfString and aElseString until after checking
    //     aValue. Probably needs vararg support.
    aResult = aValue ? aIfString : aElseString;

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Index(txIFunctionEvaluationContext *aContext,
                              const nsAString &aID, double *aResult)
{
    // Given an element's id as the parameter, need to query the element and
    //   make sure that it is a xforms:repeat node.  Given that, must query
    //   its index.

    nsCOMPtr<nsIXFormsXPathState> state;
    aContext->GetState(getter_AddRefs(state));
    nsCOMPtr<nsIDOMNode> resolverNode;
    state->GetXformsNode(getter_AddRefs(resolverNode));
    NS_ENSURE_TRUE(resolverNode, NS_ERROR_FAILURE);

    // here document is the XForms document
    nsCOMPtr<nsIDOMDocument> document;
    resolverNode->GetOwnerDocument(getter_AddRefs(document));
    NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

    // aID should be the id of a nsIXFormsRepeatElement
    nsCOMPtr<nsIDOMElement> repeatEle;
    nsCOMPtr<nsIDOMElement> resolverEle(do_QueryInterface(resolverNode));
    nsresult rv =
      nsXFormsUtils::GetElementByContextId(resolverEle, aID,
                                           getter_AddRefs(repeatEle));
    NS_ENSURE_SUCCESS(rv, rv);

    // now get the index value from the xforms:repeat.
    PRInt32 index;
    rv = nsXFormsUtils::GetRepeatIndex(repeatEle, &index);
    NS_ENSURE_SUCCESS(rv, rv);

    // repeat's index is 1-based.  If it is 0, then that is still ok since
    // repeat's index can be 0 if uninitialized or if the nodeset that it
    // is bound to is empty (either initially or due to delete remove all
    // of the instance nodes).  If index == -1, then repeatEle isn't an
    // XForms repeat element, so we need to return NaN per spec.
    *aResult = index >= 0 ? index : kNaN;

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Instance(txIFunctionEvaluationContext *aContext,
                                 const nsAString &aInstanceId,
                                 txINodeSet **aResult)
{
    *aResult = nsnull;

    // The state is the node in the XForms document that contained
    //   the expression we are evaluating.  We'll use this to get the
    //   document.  If this isn't here, then something is wrong. Bail.
    nsCOMPtr<nsIXFormsXPathState> state;
    aContext->GetState(getter_AddRefs(state));
    nsCOMPtr<nsIDOMNode> resolverNode;
    state->GetXformsNode(getter_AddRefs(resolverNode));
    NS_ENSURE_TRUE(resolverNode, NS_ERROR_FAILURE);

    // here document is the XForms document
    nsCOMPtr<nsIDOMDocument> document;
    resolverNode->GetOwnerDocument(getter_AddRefs(document));
    NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMElement> instEle;
    nsresult rv = document->GetElementById(aInstanceId,
                                           getter_AddRefs(instEle));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool foundInstance = PR_FALSE;
    if (instEle) {
        nsAutoString localname, namespaceURI;
        instEle->GetLocalName(localname);
        instEle->GetNamespaceURI(namespaceURI);

        foundInstance = localname.EqualsLiteral("instance") &&
                        namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS);
    }

    nsCOMPtr<txINodeSet> result =
        do_CreateInstance("@mozilla.org/transformiix-nodeset;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!foundInstance) {
        // We didn't find an instance element with the given id.  Return the
        //   empty result set.
        result.swap(*aResult);

        return NS_OK;
    }

    // Make sure that this element is contained in the same
    //   model as the context node of the expression as per
    //   the XForms 1.0 spec.

    // first step is to get the contextNode passed in to
    //   the evaluation

    nsCOMPtr<nsIDOMNode> xfContextNode;
    rv = aContext->GetContextNode(getter_AddRefs(xfContextNode));
    NS_ENSURE_SUCCESS(rv, rv);

    // now see if the node we found (instEle) and the
    //   context node for the evaluation (xfContextNode) link
    //   back to the same model.
    nsCOMPtr<nsIDOMNode> instNode, modelInstance;
    instNode = do_QueryInterface(instEle);
    rv = nsXFormsUtils::GetModelFromNode(instNode,
                                         getter_AddRefs(modelInstance));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool modelContainsNode =
        nsXFormsUtils::IsNodeAssocWithModel(xfContextNode, modelInstance);

    if (modelContainsNode) {
        // ok, we've found an instance node with the proper id
        //   that fulfills the requirement of being from the
        //   same model as the context node.  Now we need to
        //   return a 'node-set containing just the root
        //   element node of the referenced instance data'.
        //   Wonderful.

        nsCOMPtr<nsIDOMNode> root;
        rv = nsXFormsUtils::GetInstanceDocumentRoot(aInstanceId,
                                                    modelInstance,
                                                    getter_AddRefs(root));
        NS_ENSURE_SUCCESS(rv, rv);

        if (root) {
            result->Add(root);
        }

        result.swap(*aResult);

        return NS_OK;
    }

    // XXX where we need to do the work

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Max(txINodeSet *aNodeSet, double *aResult)
{
    PRUint32 length;
    nsresult rv = aNodeSet->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    double result = kNaN;
    PRUint32 i;
    for (i = 0; i < length; ++i) {
        double item;
        rv = aNodeSet->ItemAsNumber(i, &item);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!TX_DOUBLE_COMPARE(item, <=, result)) {
            result = item;
        }

        if (TX_DOUBLE_IS_NaN(result)) {
            break;
        }
    }

    *aResult = result;

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Min(txINodeSet *aNodeSet, double *aResult)
{
    PRUint32 length;
    nsresult rv = aNodeSet->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    double result = kNaN;
    PRUint32 i;
    for (i = 0; i < length; ++i) {
        double item;
        rv = aNodeSet->ItemAsNumber(i, &item);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!TX_DOUBLE_COMPARE(item, >=, result)) {
            result = item;
        }

        if (TX_DOUBLE_IS_NaN(result)) {
            break;
        }
    }

    *aResult = result;

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Months(const nsAString & aDuration, double *aResult)
{
    PRInt32 result = 0;
    nsresult rv = nsXFormsUtils::GetMonths(aDuration, &result);
    if (rv == NS_ERROR_ILLEGAL_VALUE) {
        *aResult = kNaN;

        return NS_OK;
    }

    *aResult = result;

    return rv;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Now(nsAString & aResult)
{
    PRExplodedTime time;
    char ctime[60];

    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &time);
    int gmtoffsethour = time.tm_params.tp_gmt_offset < 0 ?
                        -1*time.tm_params.tp_gmt_offset / 3600 :
                        time.tm_params.tp_gmt_offset / 3600;
    int remainder = time.tm_params.tp_gmt_offset%3600;
    int gmtoffsetminute = remainder ? remainder/60 : 00;

    char zone_location[40];
    const int zoneBufSize = sizeof(zone_location);
    PR_snprintf(zone_location, zoneBufSize, "%c%02d:%02d\0",
                time.tm_params.tp_gmt_offset < 0 ? '-' : '+',
                gmtoffsethour, gmtoffsetminute);

    PR_FormatTime(ctime, sizeof(ctime), "%Y-%m-%dT%H:%M:%S\0", &time);

    aResult.AssignASCII(ctime);
    AppendASCIItoUTF16(zone_location, aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Property(const nsAString &aProperty,
                                 nsAString &aResult)
{
    // This function can handle "version" and "conformance-level"
    //   which is all that the XForms 1.0 spec is worried about
    if (aProperty.EqualsLiteral("version")) {
        aResult.AssignLiteral("1.0");
    }
    else if (aProperty.EqualsLiteral("conformance-level")) {
        aResult.AssignLiteral("basic");
    }
    else {
        aResult.Truncate();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Seconds(const nsAString &aDuration, double *aResult)
{
    nsresult rv = nsXFormsUtils::GetSeconds(aDuration, aResult);
    if (rv == NS_ERROR_ILLEGAL_VALUE) {
        *aResult = kNaN;
        rv = NS_OK;
    }

    return rv;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::SecondsFromDateTime(const nsAString &aDateTime,
                                            double *aResult)
{
    nsresult rv = nsXFormsUtils::GetSecondsFromDateTime(aDateTime, aResult);
    if (rv == NS_ERROR_ILLEGAL_VALUE) {
        *aResult = kNaN;
        rv = NS_OK;
    }

    return rv;
}

NS_IMETHODIMP
nsXFormsXPathFunctions::Current(txIFunctionEvaluationContext *aContext,
                                txINodeSet **aResult)
{
  *aResult = nsnull;

  if (!nsXFormsUtils::ExperimentalFeaturesEnabled()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // now get the contextNode passed in to the evaluation
  nsCOMPtr<nsIXFormsXPathState> state;
  aContext->GetState(getter_AddRefs(state));
  nsCOMPtr<nsIDOMNode> origContextNode;
  state->GetOriginalContextNode(getter_AddRefs(origContextNode));
  NS_ENSURE_STATE(origContextNode);
  
  nsresult rv;
  nsCOMPtr<txINodeSet> result =
    do_CreateInstance("@mozilla.org/transformiix-nodeset;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  result->Add(origContextNode);
  result.swap(*aResult);

  return NS_OK;
}
