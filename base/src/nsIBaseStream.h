/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIBaseStream_h___
#define nsIBaseStream_h___

#include "nscore.h"
#include "nsISupports.h"


/* 6ccb17a0-e95e-11d1-beae-00805f8a66dc */
#define NS_IBASESTREAM_IID   \
{ 0x6ccb17a0, 0xe95e, 0x11d1, \
  {0xbe, 0xae, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }

/** Abstract stream */
class nsIBaseStream : public nsISupports {
public:

  /** Return the number of bytes in the stream */
  virtual PRInt32 GetLength() = 0;

  /** Close the stream. */
  virtual void Close() = 0;
};


#endif /* nsInputStream_h___ */
