/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#ifndef _nsMsgThreadedDBView_H_
#define _nsMsgThreadedDBView_H_

#include "nsMsgDBView.h"

// this class should probably inherit from the class that
// implements the outliner. Since I don't know what that is yet,
// I'll just make it inherit from nsMsgDBView for now.
class nsMsgThreadedDBView : public nsMsgDBView
{
public:
  nsMsgThreadedDBView();
  virtual ~nsMsgThreadedDBView();

  NS_IMETHOD Open(nsIMsgDatabase *msgDB, nsMsgViewSortType *viewType, PRInt32 *count);
  NS_IMETHOD Close();
  NS_IMETHOD Init(PRInt32 *pCount);
  NS_IMETHOD AddKeys(nsMsgKey *pKeys, PRInt32 *pFlags, const char *pLevels, nsMsgViewSortType *sortType, PRInt32 numKeysToAdd);
  NS_IMETHOD Sort(nsMsgViewSortType *sortType, nsMsgViewSortOrder *sortOrder);

protected:
  nsresult ListThreadIds(nsMsgKey *startMsg, PRBool unreadOnly, nsMsgKey *pOutput, PRInt32 *pFlags, char *pLevels, 
									 PRInt32 numToList, PRInt32 *pNumListed, PRInt32 *pTotalHeaders);
  // these are used to save off the previous view so that bopping back and forth
  // between two views is quick (e.g., threaded and flat sorted by date).
	PRBool		m_havePrevView;
	nsMsgKeyArray		m_prevIdArray;
	nsUInt32Array m_prevFlags;
	nsByteArray m_prevLevels;
  nsCOMPtr <nsISimpleEnumerator> m_threadEnumerator;
};

#endif
