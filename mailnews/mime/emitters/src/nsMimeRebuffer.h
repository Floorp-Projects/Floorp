/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
