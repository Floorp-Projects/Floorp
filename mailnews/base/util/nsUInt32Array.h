/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#ifndef _nsUInt32Array_H_
#define _nsUInt32Array_H_

#include "nscore.h"
#include "nsCRT.h"
#include "prmem.h"
#include "msgCore.h"

class NS_MSG_BASE nsUInt32Array
{
public:
  // Construction/destruction
  nsUInt32Array();
  virtual ~nsUInt32Array();
  
  // State/attribute member functions
  PRUint32          GetSize() const;
  PRBool            SetSize(PRUint32 nNewSize, PRBool AdjustGrowth=PR_FALSE, PRUint32 nGrowBy = 0);
  PRBool            AllocateSpace(PRUint32 nNewSize) {
    if (nNewSize == 0) return PR_TRUE;
    PRUint32 saveSize = m_nSize;
    nsresult rv = SetSize(nNewSize);
    m_nSize = saveSize;
    return rv;
  };
  
  // Accessor member functions
  PRUint32            &ElementAt(PRUint32 nIndex);
  PRUint32             GetAt(PRUint32 nIndex) const;
  PRUint32            *GetData();
  void               SetAt(PRUint32 nIndex, PRUint32 newElement);
  PRUint32           IndexOf(PRUint32 element);
  PRUint32           IndexOfSorted(PRUint32 element);
  
  // Insertion/deletion member functions
  PRUint32  Add(PRUint32 newElement);
  PRUint32          Add(PRUint32 *elementPtr, PRUint32 numElements); 
  void	           InsertAt(PRUint32 nIndex, PRUint32 newElement, PRUint32 nCount = 1);
  void               InsertAt(PRUint32 nStartIndex, const nsUInt32Array *pNewArray);
  void               RemoveAll();
  void               RemoveAt(PRUint32 nIndex, PRUint32 nCount = 1);
  void               SetAtGrow(PRUint32 nIndex, PRUint32 newElement);
  PRBool             RemoveElement(PRUint32 element);
  // Sorting member functions
  void               QuickSort(int (* PR_CALLBACK compare) (const void *elem1, const void *elem2, void *) = NULL);
  
  // Overloaded operators
  PRUint32             operator[](PRUint32 nIndex) const { return GetAt(nIndex); }
  PRUint32            &operator[](PRUint32 nIndex) { return ElementAt(nIndex); }
  
  // Miscellaneous member functions
  PRUint32            *CloneData(); 
  void               CopyArray(nsUInt32Array *oldA);
  void               CopyArray(nsUInt32Array &oldA);
  
protected:
  // Member data
  PRUint32 m_nSize;
  PRUint32 m_nMaxSize;
  PRUint32 m_nGrowBy;
  PRUint32* m_pData;
};


#endif // _DWordArray_H_
