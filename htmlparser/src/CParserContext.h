/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef __CParserContext
#define __CParserContext

#include "nsIParser.h"
#include "nsDeque.h"
#include "nsParserTypes.h"
#include "nsIURL.h"
#include "nsIDTD.h"
#include "nsScanner.h"
#include "nsIStreamListener.h"
#include "nsString.h"

/**
 * Note that the parser is given FULL access to all
 * data in a parsercontext. Hey, that what it's for!
 */

class CParserContext {

public:

    enum {eTransferBufferSize=4096};

    CParserContext( CScanner* aScanner,
                    CParserContext* aPreviousContext=0,
                    nsIStreamObserver* aListener=0);


    ~CParserContext();
                          

    PRInt32             mMajorIteration;
    PRInt32             mMinorIteration;

    nsString            mSourceType;
    eAutoDetectResult   mAutoDetectStatus;

    nsDequeIterator*    mCurrentPos;
    nsDequeIterator*    mMarkPos;
    nsDeque             mTokenDeque;
    CScanner*           mScanner;
    nsIDTD*             mDTD;

    eParseMode          mParseMode;
    char*               mTransferBuffer;
    nsIStreamObserver*  mListener;

    CParserContext*     mPrevContext;

};



#endif


