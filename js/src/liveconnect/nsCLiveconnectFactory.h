/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the class definition to implement nsIFactory XP-COM interface.
 *
 */

#ifndef nsCLiveconnectFactory_h___
#define nsCLiveconnectFactory_h___

#include "nsISupports.h"
#include "nsIFactory.h"

class nsCLiveconnectFactory : public nsIFactory {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports and AggregatedQueryInterface:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIFactory:

    NS_IMETHOD
    CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    NS_IMETHOD
    LockFactory(PRBool aLock);


    ////////////////////////////////////////////////////////////////////////////
    // from nsCLiveconnectFactory:

    nsCLiveconnectFactory(void);
    virtual ~nsCLiveconnectFactory(void);
};

#endif // nsCLiveconnectFactory_h___
