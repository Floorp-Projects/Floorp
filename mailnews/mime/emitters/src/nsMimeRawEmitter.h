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
#ifndef _nsMimeRawEmitter_h_
#define _nsMimeRawEmitter_h_

#include "prtypes.h"
#include "prio.h"
#include "nsMimeBaseEmitter.h"
#include "nsMimeRebuffer.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIURI.h"
#include "nsIPref.h"
#include "nsIChannel.h"

class nsMimeRawEmitter : public nsMimeBaseEmitter {
public: 
    nsMimeRawEmitter ();
    virtual       ~nsMimeRawEmitter (void);

    NS_IMETHOD    WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten);

protected:
};

/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewMimeRawEmitter(const nsIID& iid, void **result);

#endif /* _nsMimeRawEmitter_h_ */
