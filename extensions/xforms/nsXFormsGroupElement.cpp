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
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
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
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIDOM3Node.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMHTMLDivElement.h"

#include "nsIXTFXMLVisual.h"
#include "nsIXTFXMLVisualWrapper.h"

#include "nsXFormsControlStub.h"
#include "nsIModelElementPrivate.h"
#include "nsXFormsUtils.h"
#include "nsXFormsDelegateStub.h"

#ifdef DEBUG
// #define DEBUG_XF_GROUP
#endif

/**
 * Implementation of the XForms \<group\> control.
 * 
 * @see http://www.w3.org/TR/xforms/slice9.html#id2631290
 *
 * @todo XXX: If a \<label\> is the first element child for \<group\> it is the
 * label for the entire group
 *
 * @todo XXX: With some small adjustments we could let nsXFormsContextContainer
 * implement group, and get rid of this class (XXX).
 */

class nsXFormsGroupElement : public nsXFormsDelegateStub
{
protected:
  /** Tries to focus a child form control.*/
  PRBool TryFocusChildControl(nsIDOMNode *aParent);

public:
  // nsIXFormsControl
  NS_IMETHOD TryFocus(PRBool *aOK);

#ifdef DEBUG_smaug
  virtual const char* Name() { return "group"; }
#endif

};

PRBool
nsXFormsGroupElement::TryFocusChildControl(nsIDOMNode* aParent)
{
  if (!aParent)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = aParent->GetChildNodes(getter_AddRefs(children));
  if (NS_FAILED(rv))
    return PR_FALSE;

  PRUint32 childCount = 0;
  children->GetLength(&childCount);
  nsCOMPtr<nsIDOMNode> child;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIXFormsControl> control(do_QueryInterface(child));
    PRBool focus = PR_FALSE;
    if (control) {
      control->TryFocus(&focus);
      if (focus)
        return PR_TRUE;
    }
    focus = TryFocusChildControl(child);
    if (focus)
      return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsXFormsGroupElement::TryFocus(PRBool* aOK)
{
  *aOK = PR_FALSE;
  if (GetRelevantState()) {
    *aOK = TryFocusChildControl(mElement);
  }
  return NS_OK;
}

// Factory
NS_HIDDEN_(nsresult)
NS_NewXFormsGroupElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsGroupElement();
  if (!*aResult)
  return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
