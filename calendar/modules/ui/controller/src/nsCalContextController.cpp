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

#include "nsCalContextController.h"
#include "nsIXPFCSubject.h"
#include "nsCalUICIID.h"
#include "nsCalUtilCIID.h"
#include "nsCalToolkit.h"
#include "nsIXPFCCommand.h"
#include "nscalstrings.h"
#include "nsxpfcCIID.h"
#include "nsIXPFCObserverManager.h"
#include "nsIServiceManager.h"
#include "nsIXPFCObserver.h"
#include "nsIXPFCSubject.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalContextControllerIID, NS_ICAL_CONTEXT_CONTROLLER_IID);
static NS_DEFINE_IID(kXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);
static NS_DEFINE_IID(kXPFCCommandCID, NS_XPFC_COMMAND_CID);
static NS_DEFINE_IID(kCXPFCObserverManagerCID, NS_XPFC_OBSERVERMANAGER_CID);
static NS_DEFINE_IID(kIXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);
static NS_DEFINE_IID(kCXPFCObserverIID,         NS_IXPFC_OBSERVER_IID);
static NS_DEFINE_IID(kCXPFCSubjectIID,          NS_IXPFC_SUBJECT_IID);

#define kNotFound -1

nsCalContextController :: nsCalContextController(nsISupports * aOuter) : nsXPFCCanvas(aOuter)
{
  NS_INIT_REFCNT();

  mOrientation = nsContextControllerOrientation_west;
  mDuration = nsnull;
  mPeriodFormat = nsCalPeriodFormat_kHour;
}

nsCalContextController :: ~nsCalContextController()
{
  nsIXPFCObserverManager* om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);
  nsIXPFCSubject * subject = (nsIXPFCSubject *)this;
  om->UnregisterSubject(subject);
  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  NS_IF_RELEASE(mDuration);
}

nsresult nsCalContextController::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalContextControllerIID);                         

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCSubjectIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCSubject *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (nsXPFCCanvas::QueryInterface(aIID, aInstancePtr));

}


NS_IMPL_ADDREF(nsCalContextController)
NS_IMPL_RELEASE(nsCalContextController)

void nsCalContextController :: SetOrientation(nsContextControllerOrientation eOrientation)
{
  mOrientation = eOrientation;
}

nsContextControllerOrientation nsCalContextController :: GetOrientation()
{
  return (mOrientation);
}

nsresult nsCalContextController :: Init()
{
  static NS_DEFINE_IID(kCalDurationCID, NS_DURATION_CID);
  static NS_DEFINE_IID(kCalDurationIID, NS_IDURATION_IID);

  nsresult res = nsRepository::CreateInstance(kCalDurationCID, 
                                              nsnull, 
                                              kCalDurationCID, 
                                              (void **)&mDuration);

  if (NS_OK != res)
    return res;

  mDuration->SetYear(0);
  mDuration->SetMonth(0);
  mDuration->SetDay(0);
  mDuration->SetHour(0);
  mDuration->SetMinute(0);
  mDuration->SetSecond(0);

  return (nsXPFCCanvas::Init());
}

nsresult nsCalContextController :: Attach(nsIXPFCObserver * aObserver)
{
  return NS_OK;
}

nsresult nsCalContextController :: Detach(nsIXPFCObserver * aObserver)
{
  return NS_OK;
}

nsresult nsCalContextController :: Notify(nsIXPFCCommand * aCommand)
{
  nsIXPFCSubject * subject;

  nsresult res = QueryInterface(kXPFCSubjectIID,(void **)&subject);

  if (res != NS_OK)
    return res;

  nsIXPFCObserverManager* om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);
  
  res = om->Notify(subject,aCommand);

  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  return(res);
}

nsresult nsCalContextController :: SetDuration(nsDuration * aDuration)
{
  mDuration = aDuration;
  return NS_OK;
}

nsDuration * nsCalContextController :: GetDuration()
{
  return (mDuration);
}

nsresult nsCalContextController::SetPeriodFormat(nsCalPeriodFormat aPeriodFormat)
{
  mPeriodFormat = aPeriodFormat;
  return NS_OK;
}

nsCalPeriodFormat nsCalContextController::GetPeriodFormat()
{
  return (mPeriodFormat);
}

nsresult nsCalContextController :: SetParameter(nsString& aKey, nsString& aValue)
{
  /*
   * Orientation
   */

  if (aKey.EqualsIgnoreCase(CAL_STRING_ORIENTATION)) {
    if (aValue.EqualsIgnoreCase(CAL_STRING_EAST))
      SetOrientation(nsContextControllerOrientation_east);
    else if (aValue.EqualsIgnoreCase(CAL_STRING_WEST))
      SetOrientation(nsContextControllerOrientation_west);
    else if (aValue.EqualsIgnoreCase(CAL_STRING_NORTH))
      SetOrientation(nsContextControllerOrientation_north);
    else if (aValue.EqualsIgnoreCase(CAL_STRING_SOUTH))
      SetOrientation(nsContextControllerOrientation_south);
    // XXX: TODO the actual orientation in this case depends on
    //      the layout objects orientation!
    else if (aValue.EqualsIgnoreCase(CAL_STRING_NORTHWEST))
      SetOrientation(nsContextControllerOrientation_north);
    else if (aValue.EqualsIgnoreCase(CAL_STRING_SOUTHEAST))
      SetOrientation(nsContextControllerOrientation_south);
  } 

  /*
   * Context Rule
   */
  
  else if (aKey.EqualsIgnoreCase(CAL_STRING_USECTXRULE)) {
    // XXX: Look at value "Day+1" to determine how to assign our
    //      datetime object and whether it is positive or negative

    PRInt32 i ;

    if (aValue.Find(CAL_STRING_SECOND) != kNotFound) {
      SetPeriodFormat(nsCalPeriodFormat_kSecond);
      nsString string = aValue.Cut(0,6);
      mDuration->SetSecond(string.ToInteger(&i));
    } else if (aValue.Find(CAL_STRING_MINUTE) != kNotFound) {
      SetPeriodFormat(nsCalPeriodFormat_kMinute);
      nsString string = aValue.Cut(0,6);
      mDuration->SetMinute(string.ToInteger(&i));
    } else if (aValue.Find(CAL_STRING_HOUR) != kNotFound) {
      SetPeriodFormat(nsCalPeriodFormat_kHour);
      nsString string = aValue.Cut(0,4);
      mDuration->SetHour(string.ToInteger(&i));
    } else if (aValue.Find(CAL_STRING_DAY) != kNotFound) {
      SetPeriodFormat(nsCalPeriodFormat_kDay);
      nsString string = aValue.Cut(0,3);
      mDuration->SetDay(string.ToInteger(&i));
    } else if (aValue.Find(CAL_STRING_MONTH) != kNotFound) {
      SetPeriodFormat(nsCalPeriodFormat_kMonth);
      nsString string = aValue.Cut(0,5);
      mDuration->SetMonth(string.ToInteger(&i));
    } else if (aValue.Find(CAL_STRING_YEAR) != kNotFound) {
      SetPeriodFormat(nsCalPeriodFormat_kMonth);
      nsString string = aValue.Cut(0,4);
      mDuration->SetYear(string.ToInteger(&i));
    }

  }


  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}
