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

#include "ipcMessageReader.h"
#include <string.h>

//*****************************************************************************
// ipcMessageReader
//*****************************************************************************   

PRUint8 ipcMessageReader::GetInt8()
{
  if (mBufPtr < mBufEnd)
    return *mBufPtr++;
  mError = PR_TRUE;
  return 0;
}

// GetInt16 and GetInt32 go to pains to avoid unaligned memory accesses that
// are larger than a byte. On some platforms, that causes a performance penalty.
// On other platforms, Tru64 for instance, it's an error.

PRUint16 ipcMessageReader::GetInt16()
{
  if (mBufPtr + sizeof(PRUint16) <= mBufEnd) {
    PRUint8 temp[2] = { mBufPtr[0], mBufPtr[1] };
    mBufPtr += sizeof(PRUint16);
    return *(PRUint16*)temp;
  }
  mError = PR_TRUE;
  return 0;
}

PRUint32 ipcMessageReader::GetInt32()
{
  if (mBufPtr + sizeof(PRUint32) <= mBufEnd) {
    PRUint8 temp[4] = { mBufPtr[0], mBufPtr[1], mBufPtr[2], mBufPtr[3] };
    mBufPtr += sizeof(PRUint32);
    return *(PRUint32*)temp;
  }
  mError = PR_TRUE;
  return 0;
}

PRInt32 ipcMessageReader::GetBytes(void* destBuffer, PRInt32 n)
{
  if (mBufPtr + n <= mBufEnd) {
    memcpy(destBuffer, mBufPtr, n);
    mBufPtr += n;
    return n;
  }
  mError = PR_TRUE;
  return 0;
}

PRBool ipcMessageReader::AdvancePtr(PRInt32 n)
{
  const PRUint8 *newPtr = mBufPtr + n;
  if (newPtr >= mBuf && newPtr <= mBufEnd) {
    mBufPtr = newPtr;
    return PR_TRUE;
  }
  mError = PR_TRUE;
  return PR_FALSE;
}
