/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1995
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsUint8Array_H_
#define _nsUint8Array_H_

#include "nscore.h"
#include "nsCRT.h"
#include "prmem.h"
#include "msgCore.h"

class NS_MSG_BASE nsUint8Array 
{

public:

// Construction
	nsUint8Array();

// Attributes
	PRInt32 GetSize() const;
	PRInt32 GetUpperBound() const;
	void SetSize(PRInt32 nNewSize, PRInt32 nGrowBy = -1);
  void AllocateSpace(PRUint32 nNewSize) { if (nNewSize == 0) return; PRInt32 saveSize = m_nSize; SetSize(nNewSize); m_nSize = saveSize;};

// Operations
	// Clean up
	void FreeExtra();
	void RemoveAll();

	// Accessing elements
	PRUint8 GetAt(PRInt32 nIndex) const;
	void SetAt(PRInt32 nIndex, PRUint8 newElement);
	PRUint8& ElementAt(PRInt32 nIndex);

	// Potentially growing the array
	void SetAtGrow(PRInt32 nIndex, PRUint8 newElement);
	PRInt32 Add(PRUint8 newElement);

	// overloaded operator helpers
	PRUint8 operator[](PRInt32 nIndex) const;
	PRUint8& operator[](PRInt32 nIndex);

	// Operations that move elements around
	nsresult InsertAt(PRInt32 nIndex, PRUint8 newElement, PRInt32 nCount = 1);
	void RemoveAt(PRInt32 nIndex, PRInt32 nCount = 1);
	nsresult InsertAt(PRInt32 nStartIndex, nsUint8Array* pNewArray);
	void CopyArray(nsUint8Array &aSrcArray);

	// use carefully!
	PRUint8*				GetArray(void) {return((PRUint8*)m_pData);}		// only valid until another function called on the array (like GetBuffer() in CString)

// Implementation
protected:
	PRUint8* m_pData;   // the actual array of data
	PRInt32 m_nSize;     // # of elements (upperBound - 1)
	PRInt32 m_nMaxSize;  // max allocated
	PRInt32 m_nGrowBy;   // grow amount

public:
	~nsUint8Array();

#ifdef _DEBUG
	void AssertValid() const;
#endif

};

#endif
