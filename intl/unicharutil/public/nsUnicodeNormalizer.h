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
 * The Original Code is Unicode case conversion helpers.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsUnicodeNormalizer_h__
#define nsUnicodeNormalizer_h__

#include "nscore.h"
#include "nsISupports.h"

#include "nsIUnicodeNormalizer.h"

nsresult NS_NewUnicodeNormalizer(nsISupports** oResult);


class nsUnicodeNormalizer : public nsIUnicodeNormalizer {
public:
   nsUnicodeNormalizer();
   virtual ~nsUnicodeNormalizer();

   NS_DECL_ISUPPORTS 

   NS_IMETHOD NormalizeUnicodeNFD( const nsAString& aSrc, nsAString& aDest);
   NS_IMETHOD NormalizeUnicodeNFC( const nsAString& aSrc, nsAString& aDest);
   NS_IMETHOD NormalizeUnicodeNFKD( const nsAString& aSrc, nsAString& aDest);
   NS_IMETHOD NormalizeUnicodeNFKC( const nsAString& aSrc, nsAString& aDest);

   // low-level access to the composition data needed for HarfBuzz callbacks
   static bool Compose(PRUint32 a, PRUint32 b, PRUint32 *ab);
   static bool DecomposeNonRecursively(PRUint32 comp, PRUint32 *c1, PRUint32 *c2);
};

#endif //nsUnicodeNormalizer_h__

