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
                                       const nsAString &aUsemap, 
                                       nsIDOMHTMLMapElement **aMap)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  NS_ENSURE_ARG_POINTER(aMap);
  *aMap = nsnull;

  /* we used to strip the whitespace from the usemap value as a Quirk, but it was too quirky and
     didn't really work correctly - see bug 87050 
   */

  if (aUsemap.IsEmpty())
    return NS_OK;

  PRInt32 hash = aUsemap.FindChar('#');
  nsAString::const_iterator start, end;
  if (hash > -1) {
    aUsemap.BeginReading(start);
    aUsemap.EndReading(end);
    start.advance(hash + 1);
    if (start == end)
      return NS_OK; // aUsemap == "#"
  }

  // At this point, if hash < 0, then there was no '#'
  // so aUsemap IS the ref. Otherwise the ref starts
  // from 'start'

  // NOTE!
  // I'd really like to do this, but it won't compile, and casting
  // Substring return to (const nsAString&) will cause a runtime crash.
  // Therefore, to avoid doing needless work, this pattern is replicated
  // below where we actually call functions with 'usemap' values.
  //const nsAString &usemap = (hash < 0) ? aUsemap : Substring(start, end);

  nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(aDocument));
  if (htmlDoc) {
    // See above NOTE
    if (hash < 0) {
      htmlDoc->GetImageMap(aUsemap, aMap);
    } else {
      htmlDoc->GetImageMap(Substring(start, end), aMap);
    }
  } else {
    // XHTML requires that where a name attribute was used in HTML 4.01,
    // the ID attribute must be used in XHTML. name is officially deprecated.
    // This simplifies our life becase we can simply get the map with
    // getElementById().
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(aDocument));
    if (domDoc) {
      nsCOMPtr<nsIDOMElement> element;
      // See above NOTE
      if (hash < 0) {
        domDoc->GetElementById(aUsemap, getter_AddRefs(element));
      } else {
        domDoc->GetElementById(Substring(start, end), getter_AddRefs(element));
      }
      if (element) {
        CallQueryInterface(element, aMap);
      }
    }
  }
  
  return NS_OK;
}
