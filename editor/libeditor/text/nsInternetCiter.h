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

#ifndef nsInternetCiter_h__
#define nsInternetCiter_h__

#include "nsICiter.h"

#include "nsString.h"

/** Mail citations using standard Internet style.
  */
class nsInternetCiter  : public nsICiter
{
public:
  nsInternetCiter();
  virtual ~nsInternetCiter();
//Interfaces for addref and release and queryinterface
//NOTE: Use   NS_DECL_ISUPPORTS_INHERITED in any class inherited from nsEditor
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetCiteString(const nsAReadableString & aInString, nsAWritableString & aOutString);

  NS_IMETHOD StripCites(const nsAReadableString & aInString, nsAWritableString & aOutString);

  NS_IMETHOD Rewrap(const nsAReadableString & aInString,
                    PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                    PRBool aRespectNewlines,
                    nsAWritableString & aOutString);

protected:
  nsresult StripCitesAndLinebreaks(const nsAReadableString& aInString, nsAWritableString& aOutString,
                                   PRBool aLinebreaksToo, PRInt32* aCiteLevel);
};

#endif //nsInternetCiter_h__

