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

#ifndef ipcMessageWriter_h__
#define ipcMessageWriter_h__

#include "prtypes.h"

//*****************************************************************************
// ipcMessageWriter
//
// Creates a block of memory and resizes it in order to hold writes. The
// block will be freed when this object is destroyed. Bytes are written in
// native endianess, as ipcMessageReader reads them in native endianess.
//*****************************************************************************   

class ipcMessageWriter
{
public:
                  ipcMessageWriter(PRUint32 initialCapacity) :
                    mBuf(NULL),
                    mBufPtr(NULL), mBufEnd(NULL),
                    mCapacity(0),
                    mError(PR_FALSE)
                  {
                    EnsureCapacity(initialCapacity);
                  }
                  
                  ~ipcMessageWriter();

                  // Returns PR_TRUE if an error has occured at any point
                  // during the lifetime of this object, due to the buffer
                  // not being able to be grown to the required size.
  PRBool          HasError()
                  { return mError; }

  void            PutInt8(PRUint8 val);
  void            PutInt16(PRUint16 val);
  void            PutInt32(PRUint32 val);  
  PRUint32        PutBytes(const void* src, PRUint32 n);

                  // Returns the beginning of the buffer. Do not free this.
  PRUint8*        GetBuffer()
                  { return mBuf; }

  PRInt32         GetSize()
                  { return mBufPtr - mBuf; }

private:
  PRBool          EnsureCapacity(PRInt32 sizeNeeded)
                  {
                    return (mBuf && ((mBufPtr + sizeNeeded) <= mBufEnd)) ?
                      PR_TRUE : GrowCapacity(sizeNeeded);
                  }
  PRBool          GrowCapacity(PRInt32 sizeNeeded);

private:
  PRUint8         *mBuf;
  PRUint8         *mBufPtr, *mBufEnd;
  PRInt32         mCapacity;
  PRBool          mError;
};

#endif // ipcMessageWriter_h__
