/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef _rebuffer_h_
#define _rebuffer_h_

#include "prtypes.h"

//////////////////////////////////////////////////////////////
// A rebuffering class necessary for stream output buffering
//////////////////////////////////////////////////////////////

class MimeRebuffer {
public: 
    MimeRebuffer (void);
    virtual       ~MimeRebuffer (void);

    PRUint32      GetSize();
    PRUint32      IncreaseBuffer(const char *addBuf, PRUint32 size);
    PRUint32      ReduceBuffer(PRUint32 numBytes);
    char          *GetBuffer();

protected:
    PRUint32      mSize;
    char          *mBuf;
};

#endif /* _rebuffer_h_ */
