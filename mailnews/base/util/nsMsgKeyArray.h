/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
// nsMsgKeyArray.h: header file
//

#ifndef _nsMsgKeyArray_H_
#define _nsMsgKeyArray_H_ 

/////////////////////////////////////////////////////////////////////////////
// nsMsgKeyArray - Growable Array of nsMsgKeys's - Can contain up to uint - 1 ids. 
///////////////////////////////////////////////////////////////////////////// 
#include "nsUInt32Array.h"
#include "MailNewsTypes.h"

class NS_MSG_BASE nsMsgKeyArray : public nsUInt32Array
{
// constructors
public:
	nsMsgKeyArray() : nsUInt32Array() {}

// Public Operations on the nsMsgKey array - typesafe overrides
public:
	nsMsgKey operator[](PRUint32 nIndex) const {
	  return((nsMsgKey)nsUInt32Array::operator[](nIndex));
	}
	nsMsgKey GetKeyFromIndex(PRUint32 nIndex) {
	  return(operator[](nIndex));
	}
	nsMsgKey GetAt(PRUint32 nIndex) const {
	  return(operator[](nIndex));
	}
	void SetAt(PRUint32 nIndex, nsMsgKey key) {
	  nsUInt32Array::SetAt(nIndex, (PRUint32)key);
	}
	void SetAtGrow(PRUint32 nIndex, nsMsgKey key) {
	  nsUInt32Array::SetAtGrow(nIndex, (uint32)key);
	}
	void InsertAt(PRUint32 nIndex, nsMsgKey key, int nCount = 1) {
	  nsUInt32Array::InsertAt(nIndex, (PRUint32)key, nCount);
	}
	void InsertAt(PRUint32 nIndex, const nsMsgKeyArray *idArray) {
	  nsUInt32Array::InsertAt(nIndex, idArray);
	}
	PRUint32 Add(nsMsgKey id) {
	  return(nsUInt32Array::Add((uint32)id));
	}
	PRUint32 Add(nsMsgKey *elementPtr, PRUint32 numElements) {
	  return nsUInt32Array::Add((PRUint32 *) elementPtr, numElements);
	}
	void CopyArray(nsMsgKeyArray *oldA) { nsUInt32Array::CopyArray((nsUInt32Array*) oldA); }
	void CopyArray(nsMsgKeyArray &oldA) { nsUInt32Array::CopyArray(oldA); }
// new operations
public:
	nsMsgViewIndex					FindIndex(nsMsgKey key, PRUint32 startIndex = 0); // returns -1 if not found
	
	// use these next two carefully
	nsMsgKey*				GetArray(void) {
	  return((nsMsgKey*)m_pData);		// only valid until another function
										// called on the array (like
										// GetBuffer() in CString)
	}
	void				SetArray(nsMsgKey* pData, int numElements,
								 int numAllocated);
};
///////////////////////////////////////////////////////
#endif // _IDARRAY_H_
