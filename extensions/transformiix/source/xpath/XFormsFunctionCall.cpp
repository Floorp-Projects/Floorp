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

/*
 * XFormsFunctionCall
 * A representation of the XPath NodeSet funtions
 */

#include "FunctionLib.h"
#include "nsAutoPtr.h"
#include "txNodeSet.h"
#include "txAtoms.h"
#include "txIXPathContext.h"
#include "txTokenizer.h"
#include "XFormsFunctions.h"
#include <math.h>
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"
#include "nsIXFormsUtilityService.h"
#include "nsServiceManagerUtils.h"  // needed for do_GetService?
#include "prprf.h"

/*
 * Creates a XFormsFunctionCall of the given type
 */
XFormsFunctionCall::XFormsFunctionCall(XFormsFunctions aType, nsIDOMNode *aResolverNode)
    : mType(aType)
    , mResolverNode(aResolverNode)
{
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 */
nsresult
XFormsFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
  *aResult = nsnull;
  nsresult rv = NS_OK;
  txListIterator iter(&params);

  switch (mType) {
    case AVG:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

      nsRefPtr<txNodeSet> nodes;
      nsresult rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                      getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);
   
      double res = 0;
      PRInt32 i;
      for (i = 0; i < nodes->size(); ++i) {
        nsAutoString resultStr;
        txXPathNodeUtils::appendNodeValue(nodes->get(i), resultStr);
        res += Double::toDouble(resultStr);
      }
   
      if (i > 0) {
        res = (res/i);
      }
      else {
        res = Double::NaN;
      }
      return aContext->recycler()->getNumberResult(res, aResult);
    }
    case BOOLEANFROMSTRING:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

      PRInt32 retvalue = -1;
      nsAutoString booleanValue;
      evaluateToString((Expr*)iter.next(), aContext, booleanValue);

      aContext->recycler()->getBoolResult(
                                  booleanValue.EqualsLiteral("1") ||
                                  booleanValue.LowerCaseEqualsLiteral("true"), 
                                  aResult);

      return NS_OK;
    }
    case COUNTNONEMPTY:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

      nsRefPtr<txNodeSet> nodes;
      nsresult rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                      getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);
   
      double res = 0, test = 0;
      PRInt32 i, count=0;
      for (i = 0; i < nodes->size(); ++i) {
        nsAutoString resultStr;
        txXPathNodeUtils::appendNodeValue(nodes->get(i), resultStr);
        if (!resultStr.IsEmpty()) {
          count++;
        }
      }
   
      return aContext->recycler()->getNumberResult(count, aResult);
    }
    case DAYSFROMDATE:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    case IF:
    {
      if (!requireParams(3, 3, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      PRBool test;
      nsAutoString valueToReturn;
      test = evaluateToBoolean((Expr*)iter.next(), aContext);

      // grab 'true' value to return
      Expr *getvalue = (Expr*)iter.next();
   
      if (!test) {
        // grab 'false' value to return
        getvalue = (Expr*)iter.next();
      }
      evaluateToString(getvalue, aContext, valueToReturn);
   
      return aContext->recycler()->getStringResult(valueToReturn, aResult);
    }
    case INDEX:
    {
      // Given an element's id as the parameter, need to query the element and 
      //   make sure that it is a xforms:repeat node.  Given that, must query 
      //   its index.
      if (!requireParams(1, 1, aContext))
          return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    case INSTANCE:
    {
      nsresult rv;
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      nsRefPtr<txNodeSet> resultSet;
      rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
      NS_ENSURE_SUCCESS(rv, rv);
   
      nsAutoString instanceId;
      evaluateToString((Expr*)iter.next(), aContext, instanceId);
   
      // here document is the XForms document
      nsCOMPtr<nsIDOMDocument> document;
      rv = mResolverNode->GetOwnerDocument(getter_AddRefs(document)); 
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(document, NS_ERROR_NULL_POINTER);
 
      nsCOMPtr<nsIDOMElement> instEle;
      rv = document->GetElementById(instanceId, getter_AddRefs(instEle)); 
 
      PRBool foundInstance = PR_FALSE;
      nsAutoString localname, namespaceURI;
      if (instEle) {
        instEle->GetLocalName(localname);
        instEle->GetNamespaceURI(namespaceURI);
        if (localname.EqualsLiteral("instance") && 
            namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
          foundInstance = PR_TRUE;
        }
      }
 
      if (!foundInstance) {
        // We didn't find an instance element with the given id.  Return the
        //   empty result set.
        *aResult = resultSet;
        NS_ADDREF(*aResult);
    
        return NS_OK;
      }
 
      // Make sure that this element is contained in the same
      //   model as the context node of the expression as per
      //   the XForms 1.0 spec.
 
      // first step is to get the contextNode passed in to
      //   the evaluation
 
      nsCOMPtr<nsIDOMNode> xfContextNode;
      rv = txXPathNativeNode::getNode(aContext->getContextNode(), 
                                      getter_AddRefs(xfContextNode)); 
      NS_ENSURE_SUCCESS(rv, rv);
 
      // now see if the node we found (instEle) and the 
      //   context node for the evaluation (xfContextNode) link
      //   back to the same model. 
      nsCOMPtr<nsIXFormsUtilityService>xformsService = 
            do_GetService("@mozilla.org/xforms-utility-service;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);
 
      nsCOMPtr<nsIDOMNode> instNode, modelInstance;
      instNode = do_QueryInterface(instEle);
      rv = xformsService->GetModelFromNode(instNode, 
                                           getter_AddRefs(modelInstance)); 
                                 
      NS_ENSURE_SUCCESS(rv, rv);
 
      PRBool modelContainsNode = PR_FALSE;
      rv = xformsService->IsNodeAssocWithModel(xfContextNode, 
                                               modelInstance, 
                                               &modelContainsNode);
      NS_ENSURE_SUCCESS(rv, rv);
 
      if (modelContainsNode) {
        // ok, we've found an instance node with the proper id
        //   that fulfills the requirement of being from the
        //   same model as the context node.  Now we need to
        //   return a 'node-set containing just the root
        //   element node of the referenced instance data'.
        //   Wonderful.
 
        nsCOMPtr<nsIDOMNode> instanceRoot;
        rv = xformsService->GetInstanceDocumentRoot(
                              instanceId,
                              modelInstance,
                              getter_AddRefs(instanceRoot));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(instanceRoot, NS_ERROR_NULL_POINTER);
 
        nsAutoPtr<txXPathNode> txNode(txXPathNativeNode::createXPathNode(instanceRoot));
        if (txNode) {
          resultSet->add(*txNode);
        }
      }
 
 
        // XXX where we need to do the work
       // if (walker.moveToElementById(instanceId)) {
       //     resultSet->add(walker.getCurrentPosition());
       // }
   
      *aResult = resultSet;
      NS_ADDREF(*aResult);
   
      return NS_OK;
    }
    case MAX:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

      nsRefPtr<txNodeSet> nodes;
      nsresult rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                      getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);
   
      double res = Double::NaN;
      PRInt32 i;
      for (i = 0; i < nodes->size(); ++i) {
        double test;
        nsAutoString resultStr;
        txXPathNodeUtils::appendNodeValue(nodes->get(i), resultStr);
        test = Double::toDouble(resultStr);
        if (Double::isNaN(test)) {
          res = Double::NaN;
          break;
        }
        if (test > res || i == 0) {
          res = test;
        }
      }
   
      return aContext->recycler()->getNumberResult(res, aResult);
    }
    case MIN:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

      nsRefPtr<txNodeSet> nodes;
      nsresult rv = evaluateToNodeSet((Expr*)iter.next(), aContext,
                                      getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);
   
      double res = Double::NaN;
      PRInt32 i;
      for (i = 0; i < nodes->size(); ++i) {
        double test;
        nsAutoString resultStr;
        txXPathNodeUtils::appendNodeValue(nodes->get(i), resultStr);
        test = Double::toDouble(resultStr);
        if (Double::isNaN(test)) {
          res = Double::NaN;
          break;
        }
        if ((test < res) || (i==0)) {
          res = test;
        }
      }
   
      return aContext->recycler()->getNumberResult(res, aResult);
    }
    case MONTHS:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    case NOW:
    {
      if (!requireParams(0, 0, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

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
      nsString sTime = NS_ConvertASCIItoUTF16(ctime) + NS_ConvertASCIItoUTF16(zone_location);
   
      return aContext->recycler()->getStringResult(sTime, aResult);
    }
    case PROPERTY:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      nsAutoString property;
      evaluateToString((Expr*)iter.next(), aContext, property);
 
      // This function can handle "version" and "conformance-level"
      //   which is all that the XForms 1.0 spec is worried about
      if (property.Equals(NS_LITERAL_STRING("version")))
        property.Assign(NS_LITERAL_STRING("1.0"));
      else if (property.Equals(NS_LITERAL_STRING("conformance-level")))
        property.Assign(NS_LITERAL_STRING("basic"));
   
      return aContext->recycler()->getStringResult(property, aResult);
    }
    case SECONDS:
    {
      double dbl=0;
      if (!requireParams(1, 1, aContext))
          return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      return NS_ERROR_NOT_IMPLEMENTED;
   
    }
    case SECONDSFROMDATETIME:
    {
      if (!requireParams(1, 1, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
   
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  } /* switch() */

  aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                         NS_ERROR_UNEXPECTED);
  return NS_ERROR_UNEXPECTED;
}

#ifdef TX_TO_STRING
nsresult
XFormsFunctionCall::getNameAtom(nsIAtom** aAtom)
{
  switch (mType) {
    case AVG:
    {
      *aAtom = txXPathAtoms::avg;
      break;
    }
    case BOOLEANFROMSTRING:
    {
      *aAtom = txXPathAtoms::booleanFromString;
      break;
    }
    case COUNTNONEMPTY:
    {
      *aAtom = txXPathAtoms::countNonEmpty;
      break;
    }
    case DAYSFROMDATE:
    {
      *aAtom = txXPathAtoms::daysFromDate;
      break;
    }
    case IF:
    {
      *aAtom = txXPathAtoms::ifFunc;
      break;
    }
    case INDEX:
    {
      *aAtom = txXPathAtoms::index;
      break;
    }
    case INSTANCE:
    {
      *aAtom = txXPathAtoms::instance;
      break;
    }
    case MAX:
    {
      *aAtom = txXPathAtoms::max;
      break;
    }
    case MIN:
    {
      *aAtom = txXPathAtoms::min;
      break;
    }
    case MONTHS:
    {
      *aAtom = txXPathAtoms::months;
      break;
    }
    case NOW:
    {
      *aAtom = txXPathAtoms::now;
      break;
    }
    case PROPERTY:
    {
      *aAtom = txXPathAtoms::property;
      break;
    }
    case SECONDS:
    {
      *aAtom = txXPathAtoms::seconds;
      break;
    }
    case SECONDSFROMDATETIME:
    {
      *aAtom = txXPathAtoms::secondsFromDateTime;
      break;
    }
    default:
    {
      *aAtom = 0;
      return NS_ERROR_FAILURE;
    }
  }
  NS_ADDREF(*aAtom);
  return NS_OK;
}
#endif
