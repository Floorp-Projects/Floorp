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
// nsMsgKeyArray.cpp : implementation file
//

#include "msgCore.h"    // precompiled header...

#include "nsMsgKeyArray.h"


nsMsgViewIndex nsMsgKeyArray::FindIndex(nsMsgKey key, PRUint32 startIndex)
{
  for (PRUint32 i = startIndex; i < GetSize(); i++)
  {
    if ((nsMsgKey)(m_pData[i]) == key)
    {
      return i;
    }
  }
  return nsMsgViewIndex_None;
}

void nsMsgKeyArray::SetArray(nsMsgKey* pData, int numElements, int numAllocated)
{
  NS_ASSERTION(pData != NULL, "storage is NULL");
  NS_ASSERTION(numElements >= 0, "negative number of elements");
  NS_ASSERTION(numAllocated >= numElements, "num elements more than array size");
  
  delete [] m_pData;			// delete previous array
  m_pData = pData;			// set new array
  m_nMaxSize = numAllocated;	// set size
  m_nSize = numElements;		// set allocated length
}
