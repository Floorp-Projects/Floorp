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

#ifndef nsAOLCiter_h__
#define nsAOLCiter_h__

#include "nsString.h"
#include "nsICiter.h"


/** Mail citations using the AOL style >> This is a citation <<
  */
class nsAOLCiter  : public nsICiter
{
public:
  nsAOLCiter();
  virtual ~nsAOLCiter();
//Interfaces for addref and release and queryinterface
//NOTE: Use   NS_DECL_ISUPPORTS_INHERITED in any class inherited from nsEditor
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetCiteString(const nsAReadableString & aInString, nsAWritableString & aOutString);

  NS_IMETHOD StripCites(const nsAReadableString & aInString, nsAWritableString & aOutString);

  NS_IMETHOD Rewrap(const nsAReadableString & aInString,
                    PRUint32 aWrapCol, PRUint32 aFirstLineOffset,
                    PRBool aRespectNewlines,
                    nsAWritableString & aOutString);
};

#endif //nsAOLCiter_h__

