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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#ifndef nsXFormsUtils_h_
#define nsXFormsUtils_h_

/**
 * This class has static helper methods that don't fit into a specific place
 * in the class hierarchy.
 */

#include "prtypes.h"
#include "nsCOMPtr.h"
#include "nsIDOMXPathResult.h"

class nsIDOMNode;
class nsIDOMElement;
class nsIXFormsModelElement;
class nsString;

#define NS_NAMESPACE_XFORMS              "http://www.w3.org/2002/xforms"
#define NS_NAMESPACE_XHTML               "http://www.w3.org/1999/xhtml"
#define NS_NAMESPACE_XML_SCHEMA          "http://www.w3.org/2001/XMLSchema"
#define NS_NAMESPACE_XML_SCHEMA_INSTANCE "http://www.w3.org/2001/XMLSchema-instance"

class nsXFormsUtils
{
public:

  /**
   * Possible values for any |aElementFlags| parameters.  These may be
   * bitwise OR'd together.
   */
  enum {
    /**
     * The element has a "model" attribute.
     */
    ELEMENT_WITH_MODEL_ATTR = 1 << 0
  };

  /**
   * Locate the model that is a parent of |aElement|.  This method walks up the
   * content tree looking for the containing model.
   */
  static NS_HIDDEN_(nsIDOMNode*)
    GetParentModel(nsIDOMElement *aElement);

  /**
   * Locate the model that |aElement| is bound to, and if applicable, the
   * <bind> element that it uses.  The model is returned and the
   * bind element is returned (addrefed) in |aBindElement|.
   */
  static NS_HIDDEN_(nsIDOMNode*)
    GetModelAndBind(nsIDOMElement  *aElement,
                    PRUint32        aElementFlags,
                    nsIDOMElement **aBindElement);

  /**
   * Evaluate a 'bind' or 'ref' attribute on |aElement|.  |aResultType| is
   * used as the desired result type for the XPath evaluation.  The model and
   * bind elements (if applicable) are located as part of this evaluation,
   * and are returned (addrefed) in |aModel| and |aBind|.
   *
   * The return value is an XPathResult as returned from
   * nsIDOMXPathEvaluator::Evaluate().
   */
  static NS_HIDDEN_(already_AddRefed<nsIDOMXPathResult>)
    EvaluateNodeBinding(nsIDOMElement  *aElement,
                        PRUint32        aElementFlags,
                        const nsString &aDefaultRef,
                        PRUint16        aResultType,
                        nsIDOMNode    **aModel,
                        nsIDOMElement **aBind);


  /**
   * Given a bind element |aElement|, return the nodeset that it applies to.
   * |aResultType| is used as the desired result type for the XPath
   * evaluation.
   */
  static NS_HIDDEN_(already_AddRefed<nsIDOMXPathResult>)
    EvaluateNodeset(nsIDOMElement *aElement, PRUint16 aResultType);

  /**
   * Given a bind element |aElement|, find the context node to be used
   * for evaluating its nodeset.
   */
  static NS_HIDDEN_(already_AddRefed<nsIDOMNode>)
    FindBindContext(nsIDOMElement         *aBindElement,
                    nsIXFormsModelElement *aModelElement);

  /**
   * Convenience method for doing XPath evaluations.  This gets a
   * nsIDOMXPathEvaluator from |aContextNode|'s ownerDocument, and calls
   * nsIDOMXPathEvaluator::Evalute using the given expression, context node,
   * namespace resolver, and result type.
   */
  static NS_HIDDEN_(already_AddRefed<nsIDOMXPathResult>)
    EvaluateXPath(const nsAString &aExpression,
                  nsIDOMNode      *aContextNode,
                  nsIDOMNode      *aResolverNode,
                  PRUint16         aResultType);

  /**
   * Clone the set of IIDs in |aIIDList| into |aOutArray|.
   * |aOutCount| is set to |aIIDCount|.
   */
  static NS_HIDDEN_(nsresult) CloneScriptingInterfaces(const nsIID *aIIDList,
                                                       unsigned int aIIDCount,
                                                       PRUint32 *aOutCount,
                                                       nsIID ***aOutArray);
};

#endif
