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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "mozilla/dom/Element.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsImageMapUtils.h"

using namespace mozilla::dom;

/*static*/
already_AddRefed<nsIDOMHTMLMapElement>
nsImageMapUtils::FindImageMap(nsIDocument *aDocument, 
                              const nsAString &aUsemap)
{
  if (!aDocument)
    return nsnull;

  // We used to strip the whitespace from the usemap value as a Quirk,
  // but it was too quirky and didn't really work correctly - see bug
  // 87050

  if (aUsemap.IsEmpty())
    return nsnull;

  nsAString::const_iterator start, end;
  aUsemap.BeginReading(start);
  aUsemap.EndReading(end);

  PRInt32 hash = aUsemap.FindChar('#');
  if (hash > -1) {
    // aUsemap contains a '#', set start to point right after the '#'
    start.advance(hash + 1);

    if (start == end) {
      return nsnull; // aUsemap == "#"
    }
  } else {
    return nsnull;
  }

  const nsAString& usemap = Substring(start, end);

  nsCOMPtr<nsIHTMLDocument> htmlDoc(do_QueryInterface(aDocument));
  if (htmlDoc) {
    nsCOMPtr<nsIDOMHTMLMapElement> map =
      do_QueryInterface(htmlDoc->GetImageMap(usemap));
    return map.forget();
  } else {
    // For XHTML elements embedded in non-XHTML documents we get the
    // map by id since XHTML requires that where a "name" attribute
    // was used in HTML 4.01, the "id" attribute must be used in
    // XHTML. The attribute "name" is officially deprecated.  This
    // simplifies our life becase we can simply get the map with
    // getElementById().
    Element* element = aDocument->GetElementById(usemap);

    if (element) {
      nsIDOMHTMLMapElement* map;
      CallQueryInterface(element, &map);
      return map;
    }
  }
  
  return nsnull;
}
