/* Insert copyright and license here 1995 */

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
  void AllocateSpace(PRUint32 nNewSize) { PRInt32 saveSize = m_nSize; SetSize(nNewSize); m_nSize = saveSize;};

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
