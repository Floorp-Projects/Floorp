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
 */

#ifndef nsULE_H
#define nsULE_H

#include "nscore.h"
#include "prtypes.h"

#include "nsCtlCIID.h"
#include "nsILE.h"

#include "pango-types.h"
#include "pango-glyph.h"

struct textRun {
  PRInt32         length; /* Length of a chunk */  
  PRBool          isOther; /* Outside the range */
  const PRUnichar *start; /* Address of start offset */
  struct textRun  *next;
};

struct textRunList {
  struct textRun *head;
  struct textRun *cur;
  PRInt32        numRuns;
};

/* Class nsULE : Declaration */
class nsULE : public nsILE {  
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ULE_IID)
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_ULE_CID)  
  NS_DECL_ISUPPORTS

  nsULE(void);
  
  // Destructor
  virtual ~nsULE(void);
  
  // Public Methods of nsULE - Used to handle CTL operations including
  // A> API used to generate Presentation Forms based on supported fonts
  // B> API used by common text operations such as cursor positioning 
  //    and selection

  NS_IMETHOD GetPresentationForm(const PRUnichar *aString,
                                 PRUint32        aLength,
                                 const char      *fontCharset,
                                 char            *aGlyphs,
                                 PRSize          *aOutLength);

  NS_IMETHOD PrevCluster(const PRUnichar         *aString,
                         PRUint32                aLength,
                         const PRInt32           aIndex,
                         PRInt32                 *prevOffset);

  NS_IMETHOD NextCluster(const PRUnichar         *aString,
                         PRUint32                aLength,
                         const PRInt32           aIndex,
                         PRInt32                 *nextOffset);

  NS_IMETHOD GetRangeOfCluster(const PRUnichar *aString,
                               PRUint32        aLength,
                               const PRInt32   aIndex,
                               PRInt32         *aStart,
                               PRInt32         *aEnd);

 private:

  // Housekeeping Members
  void Init(void);
  void CleanUp(void);

  PangoEngineShape* GetShaper(const PRUnichar *, PRUint32, const char *);
  
  PRInt32 GetRawCtlData(const PRUnichar*, PRUint32, PangoGlyphString*);

  PRInt32 SeparateScript(const PRUnichar*, PRInt32, textRunList*);
};
#endif /* nsULE_H */
