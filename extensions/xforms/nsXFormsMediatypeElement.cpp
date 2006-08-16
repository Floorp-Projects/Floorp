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
 * Alexander Surkov.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

/**
 * Implementation of the XForms \<mediatype\> element.
 */

#include "nsXFormsDelegateStub.h"
#include "nsIDOM3Node.h"

class nsXFormsMediatypeElement : public nsXFormsDelegateStub
{
public:
  // nsIXFormsDelegate
  NS_IMETHOD GetValue(nsAString& aValue);

  nsXFormsMediatypeElement();

#ifdef DEBUG_smaug
  virtual const char* Name() { return "mediatype"; }
#endif
};

nsXFormsMediatypeElement::nsXFormsMediatypeElement():
  nsXFormsDelegateStub(NS_LITERAL_STRING("mediatype"))
{
}

NS_IMETHODIMP
nsXFormsMediatypeElement::GetValue(nsAString& aValue)
{
  // The order of precedence for determining the mediatype is:
  //   single node binding, inline text

  nsXFormsDelegateStub::GetValue(aValue);
  if (aValue.IsVoid()) {
    NS_ENSURE_STATE(mElement);

    nsCOMPtr<nsIDOM3Node> inner(do_QueryInterface(mElement));
    if (inner) {
      inner->GetTextContent(aValue);
    }
  }

  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsMediatypeElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsMediatypeElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

