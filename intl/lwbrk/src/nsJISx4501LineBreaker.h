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
 */
#ifndef nsJISx4501LineBreaker_h__
#define nsJISx4501LineBreaker_h__


#include "nsILineBreaker.h"

class nsJISx4501LineBreaker : public nsILineBreaker
{
  NS_DECL_ISUPPORTS

public:
  nsJISx4501LineBreaker(const PRUnichar *aNoBegin, PRInt32 aNoBeginLen, 
                        const PRUnichar* aNoEnd, PRInt32 aNoEndLen);
  virtual ~nsJISx4501LineBreaker();

  NS_IMETHOD BreakInBetween(const PRUnichar* aText1 , PRUint32 aTextLen1,
                            const PRUnichar* aText2 , PRUint32 aTextLen2,
                            PRBool *oCanBreak);

  NS_IMETHOD Next( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
                   PRUint32* oNext, PRBool *oNeedMoreText);

  NS_IMETHOD Prev( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
                   PRUint32* oPrev, PRBool *oNeedMoreText);


protected:

  PRInt8   GetClass(PRUnichar u);
  PRInt8   ContextualAnalysis(PRUnichar prev, PRUnichar cur, PRUnichar next );
  PRBool   GetPair(PRInt8 c1, PRInt8 c2);

};

#endif  /* nsJISx4501LineBreaker_h__ */
