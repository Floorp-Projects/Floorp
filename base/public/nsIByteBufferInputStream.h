/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef nsIByteBufferInputStream_h___
#define nsIByteBufferInputStream_h___

#include "nsIInputStream.h"

#define NS_IBYTEBUFFERINPUTSTREAM_IID                \
{ /* 40767100-eb9c-11d2-931c-00104ba0fd40 */         \
    0x40767100,                                      \
    0xeb9c,                                          \
    0x11d2,                                          \
    {0x93, 0x1c, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIByteBufferInputStream : public nsIInputStream {
public:

    static const nsIID& GetIID() { static nsIID iid = NS_IBYTEBUFFERINPUTSTREAM_IID; return iid; }

    NS_IMETHOD Fill(nsIInputStream* stream, PRUint32 *filledAmount) = 0;
};

// Makes an byte buffer input stream:
extern NS_BASE nsresult
NS_NewByteBufferInputStream(PRUint32 size, nsIByteBufferInputStream* *result);

#endif /* nsByteBufferInputStream_h___ */
