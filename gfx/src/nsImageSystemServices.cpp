/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "ilISystemServices.h"
#include "nsITimer.h"
#include "prtypes.h"
#include "prmem.h"
#include "nsCRT.h"

class ImageSystemServicesImpl : public ilISystemServices {
public:
  static ImageSystemServicesImpl *sSS;

  ImageSystemServicesImpl();
  virtual ~ImageSystemServicesImpl();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

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
  NS_INIT_REFCNT();
}

ImageSystemServicesImpl::~ImageSystemServicesImpl()
{
}

NS_IMPL_ISUPPORTS1(ImageSystemServicesImpl, ilISystemServices)

static
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

    nsresult rv;
    rv = nsComponentManager::CreateInstance("@mozilla.org/timer;1",
                                            nsnull,
                                            NS_GET_IID(nsITimer),
                                            (void**)&timer);


    if (NS_FAILED(rv)) {
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

    return (void *)timer;
}

void 
ImageSystemServicesImpl::ClearTimeout(void *aTimerID)
{
    nsITimer* timer = (nsITimer *)aTimerID;
    void*     closure = timer->GetClosure();
    
    timer->Cancel();
    NS_RELEASE(timer);
    PR_DELETE(closure);
}

extern "C" NS_GFX_(nsresult)
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

  return ImageSystemServicesImpl::sSS->QueryInterface(NS_GET_IID(ilISystemServices), 
                                                      (void **) aInstancePtrResult);
}
