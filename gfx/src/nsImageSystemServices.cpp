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

#include "ilISystemServices.h"
#include "nsITimer.h"
#include "prtypes.h"
#include "prmem.h"
#include "nsCRT.h"

static NS_DEFINE_IID(kISystemServicesIID, IL_ISYSTEMSERVICES_IID);

class ImageSystemServicesImpl : public ilISystemServices {
public:
  static ImageSystemServicesImpl *sSS;

  ImageSystemServicesImpl();
  ~ImageSystemServicesImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual void * SetTimeout(ilTimeoutCallbackFunction aFunc, 
			  void * aClosure, PRUint32 aMsecs);

  virtual void ClearTimeout(void *aTimerID);
};

typedef struct {
    ilTimeoutCallbackFunction mFunc;
    void *mClosure;
} TimerClosure;

ImageSystemServicesImpl* ImageSystemServicesImpl::sSS = nsnull;

ImageSystemServicesImpl::ImageSystemServicesImpl()
{
}

ImageSystemServicesImpl::~ImageSystemServicesImpl()
{
}

NS_IMPL_ISUPPORTS(ImageSystemServicesImpl, kISystemServicesIID)

void
timer_callback (nsITimer *aTimer, void *aClosure)
{
    TimerClosure *tc = (TimerClosure *)aClosure;

    (*tc->mFunc)(tc->mClosure);

    NS_RELEASE(aTimer);
    PR_DELETE(tc);
}

void * 
ImageSystemServicesImpl::SetTimeout(ilTimeoutCallbackFunction aFunc, 
				    void * aClosure, PRUint32 aMsecs)
{
    nsITimer *timer;
    TimerClosure *tc;

    if (NS_NewTimer(&timer) != NS_OK) {
        return nsnull;
    }

    tc = (TimerClosure *)PR_NEWZAP(TimerClosure);
    if (tc == nsnull) {
        NS_RELEASE(timer);
        return nsnull;
    }

    tc->mFunc = aFunc;
    tc->mClosure = aClosure;

    if (timer->Init(timer_callback, (void *)tc, aMsecs) != NS_OK) {
        NS_RELEASE(timer);
        PR_DELETE(tc);
        return nsnull;
    }

    NS_ADDREF(timer);

    return (void *)timer;
}

void 
ImageSystemServicesImpl::ClearTimeout(void *aTimerID)
{
    nsITimer *timer = (nsITimer *)aTimerID;
    
    timer->Cancel();
    NS_RELEASE(timer);
}

nsresult
NS_NewImageSystemServices(ilISystemServices **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (ImageSystemServicesImpl::sSS == nsnull) {
    ImageSystemServicesImpl::sSS = new ImageSystemServicesImpl();
  }

  if (ImageSystemServicesImpl::sSS == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
  }

  return ImageSystemServicesImpl::sSS->QueryInterface(kISystemServicesIID, 
                                                (void **) aInstancePtrResult);
}
