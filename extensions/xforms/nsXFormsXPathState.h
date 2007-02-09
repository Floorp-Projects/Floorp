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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include "nsIXFormsXPathState.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

class nsXFormsXPathState : public nsIXFormsXPathState
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXFORMSXPATHSTATE

  /**
   * Constructors.
   *
   * @param aXFormsNode     The XForms control upon which the expression
   *                        appears.  Used to allow our XForms XPath functions
   *                        access to the XForms document.
   * @param aContextNode    The original context node used to build and
   *                        evaluate the XPath expression.  This can be useful
   *                        to remember since this is the context node used
   *                        for the parse context part of XPath.  But the
   *                        context that our XFormsXPath functions has access
   *                        to is the evaluation context.  The context node
   *                        for the evaluation context can chnage as XPath
   *                        works its way through the expression.
   */

  nsXFormsXPathState(nsIDOMNode *aXFormsNode,
                     nsIDOMNode *aContextNode)
    : mXFormsNode(aXFormsNode),
      mOriginalContextNode(aContextNode) {};

private:

  nsCOMPtr<nsIDOMNode> mXFormsNode;
  nsCOMPtr<nsIDOMNode> mOriginalContextNode;
};
