/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsStatusBarBiffManager_h__
#define nsStatusBarBiffManager_h__

#include "nsIFolderListener.h"

#include "msgCore.h"
#include "nsCOMPtr.h"


class nsStatusBarBiffManager : public nsIFolderListener
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIFOLDERLISTENER

	nsStatusBarBiffManager(); 
	virtual ~nsStatusBarBiffManager();

	nsresult Init();
	nsresult Shutdown();
	nsresult PerformStatusBarBiff(PRUint32 newBiffFlag);


private:
	PRBool   mInitialized;
	PRUint32 mCurrentBiffState;
    nsCString mDefaultSoundURL;

protected:
	  static nsIAtom* kBiffStateAtom;
};



#endif // nsStatusBarBiffManager_h__

