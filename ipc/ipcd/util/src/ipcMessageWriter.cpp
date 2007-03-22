/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
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

#include "ipcMessageWriter.h"
#include "prmem.h"
#include <string.h>

//*****************************************************************************
// ipcMessageWriter
//*****************************************************************************   

ipcMessageWriter::~ipcMessageWriter()
{
  if (mBuf)
    free(mBuf);
}
  
void ipcMessageWriter::PutInt8(PRUint8 val)
{
  if (EnsureCapacity(sizeof(PRUint8))) 
    *mBufPtr++ = val;
}

// PutInt16 and PutInt32 go to pains to avoid unaligned memory accesses that
// are larger than a byte. On some platforms, that causes a performance penalty.
// On other platforms, Tru64 for instance, it's an error.
  
void ipcMessageWriter::PutInt16(PRUint16 val)
{
  if (EnsureCapacity(sizeof(PRUint16))) {
    PRUint8 temp[2];
    *(PRUint16*)temp = val;
    *mBufPtr++ = temp[0];
    *mBufPtr++ = temp[1];
  }
}
  
void ipcMessageWriter::PutInt32(PRUint32 val)
{
  if (EnsureCapacity(sizeof(PRUint32))) {
    PRUint8 temp[4];
    *(PRUint32*)temp = val;
    *mBufPtr++ = temp[0];
    *mBufPtr++ = temp[1];
    *mBufPtr++ = temp[2];
    *mBufPtr++ = temp[3];    
  }
}
  
PRUint32 ipcMessageWriter::PutBytes(const void* src, PRUint32 n)
{
  if (EnsureCapacity(n)) {
    memcpy(mBufPtr, src, n);
    mBufPtr += n;
    return n;
  }
  return 0;
}
    
PRBool ipcMessageWriter::GrowCapacity(PRInt32 sizeNeeded)
{
  if (sizeNeeded < 0)
    return PR_FALSE;
  PRInt32 newCapacity = (mBufPtr - mBuf) + sizeNeeded;
  if (mCapacity == 0)
    mCapacity = newCapacity;
  else
  {
    while (newCapacity > mCapacity && (mCapacity << 1) > 0)
      mCapacity <<= 1;
    if (newCapacity > mCapacity) // if we broke out because of rollover
      return PR_FALSE;
  }
    
  PRInt32 curPos = mBufPtr - mBuf;
  mBuf = (PRUint8*)realloc(mBuf, mCapacity);
  if (!mBuf) {
    mError = PR_TRUE;
    return PR_FALSE;
  }
  mBufPtr = mBuf + curPos;
  mBufEnd = mBuf + mCapacity;
  return PR_TRUE;
}
