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
 * The Original Code is mozilla.org code.
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

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsImageMapUtils.h"

/*static*/
nsresult nsImageMapUtils::FindImageMap(nsIDocument *aDocument, 
                                       const nsAReadableString &aUsemap, 
                                       nsIDOMHTMLMapElement **aMap)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aMap);
  *aMap = nsnull;

  nsresult rv = NS_OK;

  nsAutoString usemap(aUsemap);

  // Strip out whitespace in the name for navigator compatibility
  // XXX NAV QUIRK
  usemap.StripWhitespace();

  if (!usemap.IsEmpty() && usemap.First() == '#') {
    usemap.Cut(0, 1);
  }

  if (usemap.IsEmpty())
    return rv;

  nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(aDocument));
  if (htmlDoc) {
    htmlDoc->GetImageMap(usemap,aMap);
  } else {
    // XHTML requires that where a name attribute was used in HTML 4.01,
    // the ID attribute must be used in XHTML. name is officially deprecated.
    // This simplifies our life becase we can simply get the map with
    // getElementById().
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(aDocument));
    if (domDoc) {
      nsCOMPtr<nsIDOMElement> element;
      domDoc->GetElementById(usemap,getter_AddRefs(element));
      if (element) {
        element->QueryInterface(NS_GET_IID(nsIDOMHTMLMapElement),(void**)aMap);
      }
    }
  }
  
  return rv;
}
