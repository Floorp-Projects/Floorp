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

#ifndef __nsNNTPCategoryContainer_h
#define __nsNNTPCategoryContainer_h

#include "nsINNTPCategoryContainer.h"
#include "nsINNTPNewsgroup.h"

#include "nsISupports.h" /* interface nsISupports */

#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
//#include <stdio.h>

class nsNNTPCategoryContainer : public nsISupports {
 public: 
	nsNNTPCategoryContainer();
	virtual ~nsNNTPCategoryContainer();
	 
	NS_DECL_ISUPPORTS
	NS_IMETHOD GetRootCategory(nsINNTPNewsgroup * *aRootCategory);
	NS_IMETHOD SetRootCategory(nsINNTPNewsgroup * aRootCategory);
    NS_IMETHOD Initialize(nsINNTPNewsgroup * aRootCategory);
    
protected:
    nsINNTPNewsgroup * m_newsgroup;
};

#endif
