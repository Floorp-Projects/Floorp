/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * XPCTL : nsULE.h
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by SUN are Copyright (C) 2000 SUN
 * Microsystems, Inc. All Rights Reserved.
 *
 * This module 'XPCTL Interface' is based on Pango (www.pango.org) 
 * by Red Hat Software. Portions created by Redhat are Copyright (C) 
 * 1999 Red Hat Software.
 * 
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */
#ifndef nsULE_H
#define nsULE_H

#include "nscore.h"
#include "prtypes.h"

#include "nsCtlCIID.h"
#include "nsILE.h"

#include "pango-types.h"
#include "pango-glyph.h"

/* Class nsULE : Declaration */
class nsULE : public nsILE {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ULE_IID)
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_ULE_CID)
  NS_DECL_ISUPPORTS

  nsULE(void);
  virtual ~nsULE(void);

  // Public Methods of nsULE - Used to handle CTL operations including
  // A> API used to generate Presentation Forms based on supported fonts
  // B> API used by common text operations such as cursor positioning
  //    and selection

  NS_IMETHOD NeedsCTLFix(const PRUnichar         *aString,
                         const PRInt32           aBeg,
                         const PRInt32           aEnd,
                         PRBool                  *aCTLNeeded);

  NS_IMETHOD GetPresentationForm(const PRUnichar *aString,
                                 PRUint32        aLength,
                                 const char      *aFontCharset,
                                 char            *aGlyphs,
                                 PRSize          *aOutLength,
                                 PRBool          aIsWide = PR_FALSE);

  NS_IMETHOD PrevCluster(const PRUnichar         *aString,
                         PRUint32                aLength,
                         const PRInt32           aIndex,
                         PRInt32                 *aPrevOffset);

  NS_IMETHOD NextCluster(const PRUnichar         *aString,
                         PRUint32                aLength,
                         const PRInt32           aIndex,
                         PRInt32                 *aNextOffset);

  NS_IMETHOD GetRangeOfCluster(const PRUnichar   *aString,
                               PRUint32          aLength,
                               const PRInt32     aIndex,
                               PRInt32           *aStart,
                               PRInt32           *aEnd);

 private:
  const char* GetDefaultFont(const PRUnichar);
  PRInt32     GetGlyphInfo(const PRUnichar*, PRInt32,
                           PangoliteGlyphString*,
                           const char* = (const char*)NULL);
};
#endif /* !nsULE_H */
