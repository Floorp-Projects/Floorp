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

#ifndef nsFindAndReplace_h__
#define nsFindAndReplace_h__

#include "nsCOMPtr.h"
#include "nsIFindAndReplace.h"
#include "nsITextServicesDocument.h"

class nsFindAndReplace : public nsIFindAndReplace
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFINDANDREPLACE

  nsFindAndReplace();
  virtual ~nsFindAndReplace();

protected:
  nsCOMPtr<nsITextServicesDocument> mTsDoc;
  PRPackedBool mFindBackwards;
  PRPackedBool mCaseSensitive;
  PRPackedBool mWrapFind;
  PRPackedBool mEntireWord;

  PRInt32 mStartBlockIndex;
  PRInt32 mStartSelOffset;
  PRInt32 mCurrentBlockIndex;
  PRInt32 mCurrentSelOffset;
  PRPackedBool mWrappedOnce;

  nsresult GetCurrentBlockIndex(nsITextServicesDocument *aDoc, PRInt32 *outBlockIndex);
  nsresult SetupDocForFind(nsITextServicesDocument *aDoc, PRInt32 *outBlockOffset);
  nsresult SetupDocForReplace(nsITextServicesDocument *aDoc, const nsString &aFindStr, PRInt32 *outBlockOffset);
  nsresult DoFind(nsITextServicesDocument *aTxtDoc, const nsString &aFindString, PRBool *aDidFind);
};

#endif // nsFindAndReplace_h__
