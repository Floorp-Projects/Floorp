/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

/**
 * Utilities to help manage image maps. These are needed in more than one
 * library.
 */

#ifndef nsImageMapUtils_h___
#define nsImageMapUtils_h___

class nsIDocument;
class nsIDOMHTMLMapElement;
#include "nsAString.h"

class nsImageMapUtils {
  // This class is not to be constructed, use the static member functions only
  nsImageMapUtils();
  ~nsImageMapUtils();

public:

  /**
   * FindImageMap tries to find a map element from a document.
   *
   * @param aDocument [in]  The document to be searched.
   * @param aUsemap   [in]  The value of the usemap attribute.
   * @param aMap      [out] The first found (if any) map element.
   * @return          XPCOM return values.
   */
  static nsresult FindImageMap(nsIDocument *aDocument, 
                               const nsAReadableString &aUsemap, 
                               nsIDOMHTMLMapElement **aMap);
};

#endif
