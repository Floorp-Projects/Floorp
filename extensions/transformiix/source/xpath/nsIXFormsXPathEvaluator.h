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

#ifndef nsIXFormsXPathEvaluator_h
#define nsIXFormsXPathEvaluator_h 


#include "nsISupports.h"

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif
class nsIDOMNode; /* forward declaration */
class nsIDOMXPathExpression; /* forward declaration */

/* starting interface:    nsIXFormsXPathEvaluator */
#define NS_XFORMS_XPATH_EVALUATOR_CONTRACTID "@mozilla.org/dom/xforms-xpath-evaluator;1"
/* a7e127c6-31ff-40b4-8780-15d6938b33d3 */
#define TRANSFORMIIX_XFORMS_XPATH_EVALUATOR_CID   \
{ 0xa7e127c6, 0x31ff, 0x40b4, { 0x87, 0x80, 0x15, 0xd6, 0x93, 0x8b, 0x33, 0xd3 } }

/* 60050a4a-4c99-4bad-8c47-3b9b96caf2b4 */
#define TRANSFORMIIX_XFORMS_XPATH_EVALUATOR_IID   \
{ 0x60050a4a, 0x4c99, 0x4bad, { 0x8c, 0x47, 0x3b, 0x9b, 0x96, 0xca, 0xf2, 0xb4 } }

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIXFORMXPATHEVALUATOR \
  NS_IMETHOD CreateExpression(const nsAString & aExpression, nsIDOMNode *aResolverNode,nsIDOMXPathExpression **aResult); \
  NS_IMETHOD Evaluate(const nsAString & aExpression, nsIDOMNode *aContextNode, nsIDOMNode *aResolverNode, PRUint16 aType, nsISupports *aInResult, nsISupports **aResult); 

/**
 * Private interface implemented by the nsXFormsXPathEvaluator in Transformiix
 *   and will move to the XForms extension when XPath is made extensible.  We
 *   are using this interface instead of nsIDOMXPathEvaluator since we can
 *   don't really need all of that overhead.  For example, this interface uses
 *   a resolver node from the xforms document rather than forcing XForms to
 *   create a namespace resolver node prior to creating the expression or
 *   running an evaluation.
 */
class NS_NO_VTABLE nsIXFormsXPathEvaluator : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(TRANSFORMIIX_XFORMS_XPATH_EVALUATOR_IID)

  /**
   * Function to create a nsIDOMXPathExpression from the provided expression
   * string.  aResolverNode is the xforms node that the expression is
   * associated with.
   */
  NS_IMETHOD CreateExpression(const nsAString & aExpression, nsIDOMNode *aResolverNode, nsIDOMXPathExpression **aResult) = 0;

  /**
   * Function to evaluate the given expression.  aResolverNode is the xforms
   * node that the expression is associated with.  The other parameters are as
   * required by DOM's XPathEvaluator.
   */
  NS_IMETHOD Evaluate(const nsAString & aExpression, nsIDOMNode *aContextNode, nsIDOMNode *aResolverNode, PRUint16 aType, nsISupports *aInResult, nsISupports **aResult) = 0;

};

#endif /* nsIXFormsXPathEvaluator_h  */
