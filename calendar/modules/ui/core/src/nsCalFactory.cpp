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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsCalUICIID.h"

#include "nsCalTimeContext.h"
#include "nsCalComponent.h"
#include "nsCalDurationCommand.h"
#include "nsCalDayListCommand.h"
#include "nsCalFetchEventsCommand.h"
#include "nsCalNewModelCommand.h"
#include "nsCalContextController.h"
#include "nsCalTimebarContextController.h"
#include "nsCalMonthContextController.h"

#include "nsCalTimebarCanvas.h"
#include "nsCalStatusCanvas.h"
#include "nsCalCommandCanvas.h"
#include "nsCalTimebarComponentCanvas.h"
#include "nsCalTimebarHeading.h"
#include "nsCalTimebarUserHeading.h"
#include "nsCalTimebarTimeHeading.h"
#include "nsCalTimebarScale.h"
#include "nsCalMultiDayViewCanvas.h"
#include "nsCalMultiUserViewCanvas.h"
#include "nsCalDayViewCanvas.h"
#include "nsCalMonthViewCanvas.h"
#include "nsCalTodoComponentCanvas.h"

#include "nsCalToolkit.h"

static NS_DEFINE_IID(kCCalTimeContext, NS_CAL_TIME_CONTEXT_CID);
static NS_DEFINE_IID(kCCalComponent, NS_CAL_COMPONENT_CID);
static NS_DEFINE_IID(kCCalDurationCommand, NS_CAL_DURATION_COMMAND_CID);
static NS_DEFINE_IID(kCCalDayListCommand, NS_CAL_DAYLIST_COMMAND_CID);
static NS_DEFINE_IID(kCCalFetchEventsCommand, NS_CAL_FETCHEVENTS_COMMAND_CID);
static NS_DEFINE_IID(kCCalNewModelCommand, NS_CAL_NEWMODEL_COMMAND_CID);
static NS_DEFINE_IID(kCCalContextController, NS_CAL_CONTEXT_CONTROLLER_CID);
static NS_DEFINE_IID(kCCalTimebarContextController, NS_CAL_TIMEBAR_CONTEXT_CONTROLLER_CID);
static NS_DEFINE_IID(kCCalMonthContextController, NS_CAL_MONTH_CONTEXT_CONTROLLER_CID);

static NS_DEFINE_IID(kICalTimeContext, NS_ICAL_TIME_CONTEXT_IID);
static NS_DEFINE_IID(kICalContextController, NS_ICAL_CONTEXT_CONTROLLER_IID);

static NS_DEFINE_IID(kCCalTimebarCanvas, NS_CAL_TIMEBARCANVAS_CID);
static NS_DEFINE_IID(kCCalCanvas, NS_CAL_CANVAS_CID);
static NS_DEFINE_IID(kCCalStatusCanvas, NS_CAL_STATUSCANVAS_CID);
static NS_DEFINE_IID(kCCalCommandCanvas, NS_CAL_COMMANDCANVAS_CID);
static NS_DEFINE_IID(kCCalTimebarComponentCanvas, NS_CAL_TIMEBARCOMPONENTCANVAS_CID);
static NS_DEFINE_IID(kCCalMultiDayViewCanvas, NS_CAL_MULTIDAYVIEWCANVAS_CID);
static NS_DEFINE_IID(kCCalMultiUserViewCanvas, NS_CAL_MULTIUSERVIEWCANVAS_CID);
static NS_DEFINE_IID(kCCalDayViewCanvas, NS_CAL_DAYVIEWCANVAS_CID);
static NS_DEFINE_IID(kCCalMonthViewCanvas, NS_CAL_MONTHVIEWCANVAS_CID);
static NS_DEFINE_IID(kCCalTodoComponentCanvas, NS_CAL_TODOCOMPONENTCANVAS_CID);
static NS_DEFINE_IID(kCCalTimebarHeading, NS_CAL_TIMEBARHEADING_CID);
static NS_DEFINE_IID(kCCalTimebarUserHeading, NS_CAL_TIMEBARUSERHEADING_CID);
static NS_DEFINE_IID(kCCalTimebarTimeHeading, NS_CAL_TIMEBARTIMEHEADING_CID);
static NS_DEFINE_IID(kCCalTimebarScale, NS_CAL_TIMEBARSCALE_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

static NS_DEFINE_IID(kCCalToolkit,  NS_CAL_TOOLKIT_CID);

class nsCalFactory : public nsIFactory
{   
  public:   
    // nsISupports methods   
    NS_IMETHOD QueryInterface(const nsIID &aIID,    
                              void **aResult);   
    NS_IMETHOD_(nsrefcnt) AddRef(void);   
    NS_IMETHOD_(nsrefcnt) Release(void);   

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsCalFactory(const nsCID &aClass);   
    ~nsCalFactory();   

  private:   
    nsrefcnt  mRefCnt;   
    nsCID     mClassID;
};   

nsCalFactory::nsCalFactory(const nsCID &aClass)   
{   
  mRefCnt = 0;
  mClassID = aClass;
}   

nsCalFactory::~nsCalFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsCalFactory::QueryInterface(const nsIID &aIID,   
                                      void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

nsrefcnt nsCalFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt nsCalFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

nsresult nsCalFactory::CreateInstance(nsISupports *aOuter,  
                                      const nsIID &aIID,  
                                      void **aResult)  
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCCalTimeContext)) {
    inst = (nsISupports *)(nsICalTimeContext *)new nsCalTimeContext();
#if 0
    nsICalTimeContext * cc = (nsICalTimeContext *)new nsCalTimeContext();
    cc->QueryInterface(kISupportsIID,(void **)&inst);
#endif
  }
  else if (mClassID.Equals(kCCalComponent)) {
    inst = (nsISupports *)new nsCalComponent();
  }
  else if (mClassID.Equals(kCCalDurationCommand)) {
    inst = (nsISupports *)new nsCalDurationCommand();
  }
  else if (mClassID.Equals(kCCalDayListCommand)) {
    inst = (nsISupports *)new nsCalDayListCommand();
  }
  else if (mClassID.Equals(kCCalFetchEventsCommand)) {
    inst = (nsISupports *)new nsCalFetchEventsCommand();
  }
  else if (mClassID.Equals(kCCalNewModelCommand)) {
    inst = (nsISupports *)new nsCalNewModelCommand();
  }
  else if (mClassID.Equals(kCCalContextController)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalContextController(aOuter);
  }
  else if (mClassID.Equals(kCCalTimebarContextController)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTimebarContextController(aOuter);
  }
  else if (mClassID.Equals(kCCalMonthContextController)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalMonthContextController(aOuter);
  }
  else if (mClassID.Equals(kCCalTimebarCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTimebarCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalStatusCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalStatusCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalCommandCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalCommandCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalTimebarComponentCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTimebarComponentCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalTodoComponentCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTodoComponentCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalTimebarHeading)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTimebarHeading(aOuter);
  }
  else if (mClassID.Equals(kCCalTimebarUserHeading)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTimebarUserHeading(aOuter);
  }
  else if (mClassID.Equals(kCCalTimebarTimeHeading)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTimebarTimeHeading(aOuter);
  }
  else if (mClassID.Equals(kCCalTimebarScale)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalTimebarScale(aOuter);
  }
  else if (mClassID.Equals(kCCalMultiDayViewCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalMultiDayViewCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalMultiUserViewCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalMultiUserViewCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalDayViewCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalDayViewCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalMonthViewCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsCalMonthViewCanvas(aOuter);
  }
  else if (mClassID.Equals(kCCalToolkit)) {
    inst = (nsISupports *)(nsICalToolkit *)new nsCalToolkit();
  }

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

/* For Agg in Canvas?
        if (nsnull != aOuter) {
            result = &it->fAggregated;
        } else {
            result = (nsIProtocolConnection *)it;
        }

*/
  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult nsCalFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsCalFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}

