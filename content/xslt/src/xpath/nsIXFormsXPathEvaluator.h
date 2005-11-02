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
class nsIDOMNSXPathExpression; /* forward declaration */

/* starting interface:    nsIXFormsXPathEvaluator */
#define NS_XFORMS_XPATH_EVALUATOR_CONTRACTID "@mozilla.org/dom/xforms-xpath-evaluator;1"
/* 4cdd884f-f949-4d82-bb78-b8edd9f1420c */
#define TRANSFORMIIX_XFORMS_XPATH_EVALUATOR_CID   \
{ 0x4cdd884f, 0xf949, 0x4d82, \
  {0xbb, 0x78, 0xb8, 0xed, 0xd9, 0xf1, 0x42, 0x0c} }

/* 61e5a446-73f7-432e-a2d6-d94d4a51aed8 */
#define TRANSFORMIIX_XFORMS_XPATH_EVALUATOR_IID   \
{ 0x61e5a446, 0x73f7, 0x432e, \
  {0xa2, 0xd6, 0xd9, 0x4d, 0x4a, 0x51, 0xae, 0xd8} }

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIXFORMXPATHEVALUATOR \
  NS_IMETHOD CreateExpression(const nsAString & aExpression, nsIDOMNode *aResolverNode, nsIDOMNSXPathExpression **aResult); \
  NS_IMETHOD Evaluate(const nsAString & aExpression, nsIDOMNode *aContextNode, PRUint32 aContextPosition, PRUint32 aContextSize, nsIDOMNode *aResolverNode, PRUint16 aType, nsISupports *aInResult, nsISupports **aResult); 

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
   * Function to create a nsIDOMNSXPathExpression from the provided expression
   * string.  aResolverNode is the xforms node that the expression is
   * associated with.
   */
  NS_IMETHOD CreateExpression(const nsAString & aExpression, nsIDOMNode *aResolverNode, nsIDOMNSXPathExpression **aResult) = 0;

  /**
   * Function to evaluate the given expression.  aResolverNode is the xforms
   * node that the expression is associated with.  The other parameters are as
   * required by DOM's XPathEvaluator.
   */
  NS_IMETHOD Evaluate(const nsAString & aExpression, nsIDOMNode *aContextNode, PRUint32 aContextPosition, PRUint32 aContextSize, nsIDOMNode *aResolverNode, PRUint16 aType, nsISupports *aInResult, nsISupports **aResult) = 0;

};

#endif /* nsIXFormsXPathEvaluator_h  */
