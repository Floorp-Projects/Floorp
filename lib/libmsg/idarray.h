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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
// idarray.h: header file
//

#ifndef _IDARRAY_H_
#define _IDARRAY_H_ 

/////////////////////////////////////////////////////////////////////////////
// IDArray - Array of ID's - Can contain up to uint - 1 ids. On Win16, 64K - 1
// On 32 bit platforms, more.
///////////////////////////////////////////////////////////////////////////// 
#include "dwordarr.h"

class IDArray : public XPDWordArray
{
// constructors
public:
	IDArray() : XPDWordArray() {}

// Public Operations on the MessageKey array - typesafe overrides
public:
	MessageKey operator[](uint nIndex) const {
	  return((MessageKey)XPDWordArray::operator[](nIndex));
	}
	MessageKey GetIdFromIndex(uint nIndex) {
	  return(operator[](nIndex));
	}
	MessageKey GetAt(uint nIndex) const {
	  return(operator[](nIndex));
	}
	void SetAt(uint nIndex, MessageKey id) {
	  XPDWordArray::SetAt(nIndex, (uint32)id);
	}
	void SetAtGrow(uint nIndex, MessageKey id) {
	  XPDWordArray::SetAtGrow(nIndex, (uint32)id);
	}
	void InsertAt(uint nIndex, MessageKey id, int nCount = 1) {
	  XPDWordArray::InsertAt(nIndex, (uint32)id, nCount);
	}
	void InsertAt(uint nIndex, const IDArray *idArray) {
	  XPDWordArray::InsertAt(nIndex, idArray);
	}
	uint Add(MessageKey id) {
	  return(XPDWordArray::Add((uint32)id));
	}
	uint Add(MessageKey *elementPtr, uint numElements) {
	  return XPDWordArray::Add((uint32 *) elementPtr, numElements);
	}
	void CopyArray(IDArray *oldA) { XPDWordArray::CopyArray((XPDWordArray*) oldA); }
	void CopyArray(IDArray &oldA) { XPDWordArray::CopyArray(oldA); }
// new operations
public:
	MSG_ViewIndex					FindIndex(MessageKey key); // returns -1 if not found
	
	// use these next two carefully
	MessageKey*				GetArray(void) {
	  return((MessageKey*)m_pData);		// only valid until another function
										// called on the array (like
										// GetBuffer() in CString)
	}
	void				SetArray(MessageKey* pData, int numElements,
								 int numAllocated);
};
///////////////////////////////////////////////////////
#endif // _IDARRAY_H_
