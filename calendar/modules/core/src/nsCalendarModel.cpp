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

#include <stdio.h>
#include "nscore.h"
#include "nsCalendarModel.h"
#include "nsCoreCIID.h"
#include "nsCalUICIID.h"
#include "nsxpfcCIID.h"
#include "nsICalendarUser.h"
#include "nsxpfcCIID.h"
#include "nsIXPFCCommand.h"
#include "nsIXPFCObserverManager.h"
#include "nsIServiceManager.h"
#include "nsXPFCNotificationStateCommand.h"
#include "nsCalFetchEventsCommand.h"
#include "nsXPFCModelUpdateCommand.h"
#include "nsIXPFCObserver.h"
#include "nsIXPFCSubject.h"

static NS_DEFINE_IID(kCXPFCSubjectIID,          NS_IXPFC_SUBJECT_IID);
static NS_DEFINE_IID(kICalendarModelIID, NS_ICALENDAR_MODEL_IID);
static NS_DEFINE_IID(kIModelIID,         NS_IMODEL_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);
static NS_DEFINE_IID(kXPFCCommandCID, NS_XPFC_COMMAND_CID);
static NS_DEFINE_IID(kCXPFCObserverManagerCID, NS_XPFC_OBSERVERMANAGER_CID);
static NS_DEFINE_IID(kIXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);
static NS_DEFINE_IID(kXPFCObserverIID, NS_IXPFC_OBSERVER_IID);
static NS_DEFINE_IID(kXPFCCommandReceiverIID, NS_IXPFC_COMMANDRECEIVER_IID);
static NS_DEFINE_IID(kCXPFCObserverIID,         NS_IXPFC_OBSERVER_IID);

nsCalendarModel::nsCalendarModel(nsISupports* outer)
{
  NS_INIT_REFCNT();
  mCalendarUser = nsnull;
}

nsresult nsCalendarModel::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kClassIID, kICalendarModelIID);                         

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIModelIID)) {                                      
    *aInstancePtr = (void*)(nsIModel*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCObserverIID)) {                                          
    *aInstancePtr = (void*)(nsIXPFCObserver *) this;   
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCSubjectIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCSubject *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCCommandReceiverIID)) {                                          
    *aInstancePtr = (void*)(nsIXPFCCommandReceiver *) this;   
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (NS_NOINTERFACE);

}


NS_IMPL_ADDREF(nsCalendarModel)
NS_IMPL_RELEASE(nsCalendarModel)

nsCalendarModel::~nsCalendarModel()
{
  nsIXPFCObserverManager* om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);
  nsIXPFCObserver * observer = (nsIXPFCObserver *) this;
  nsIXPFCSubject * subject = (nsIXPFCSubject *) this;
  om->UnregisterSubject(subject);
  om->UnregisterObserver(observer);
  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  mCalendarUser = nsnull; // Do Not Release
}

nsresult nsCalendarModel::Init()
{
  /*
   * We care about Command State Notifications, so register
   * ourselves for them
   */
  
  nsIXPFCObserverManager* om;
  nsIXPFCObserver * ob ;

  nsresult res = QueryInterface(kXPFCObserverIID, (void**)&ob);

  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);

  res = om->RegisterForCommandState(ob,nsCommandState_eComplete);

  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  NS_RELEASE(ob);

  return NS_OK;
}


NS_IMETHODIMP nsCalendarModel :: GetCalendarUser(nsICalendarUser *& aCalendarUser)
{
  aCalendarUser = mCalendarUser;
  return NS_OK;
}

NS_IMETHODIMP nsCalendarModel :: SetCalendarUser(nsICalendarUser* aCalendarUser)
{
  mCalendarUser = aCalendarUser;
  return NS_OK;
}


nsEventStatus nsCalendarModel::Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand)
{

  nsEventStatus status;

  /*
   * Update our internal structure based on this update
   */

  status = Action(aCommand);

  /*
   * Tell others we are updated
   *
   *      IDEA:  Pass this event off to the canvas, who will update it's view
   *             based on the command, but wait with hourglass until the final
   *             ModelUpdated command comes in for rendering the contents of the
   *             view ... maybe this should be configurable
   *
   */

  Notify(aCommand);

  return (nsEventStatus_eIgnore);
}

nsEventStatus nsCalendarModel::Action(nsIXPFCCommand * aCommand)
{
  /*
   * We can receive 2 general classes of events:
   *
   * 1) Events telling us a LayerCollection needs data of a specific range
   * 2) Events telling us that a series of Commands have completed
   *
   */

  nsEventStatus status = nsEventStatus_eIgnore;
  nsXPFCNotificationStateCommand * notification_command  = nsnull;
  nsresult res;

  static NS_DEFINE_IID(kCXPFCNotificationStateCommandCID, NS_XPFC_NOTIFICATIONSTATE_COMMAND_CID);
  
  res = aCommand->QueryInterface(kCXPFCNotificationStateCommandCID,(void**)&notification_command);

  if (NS_OK == res)
  {
    HandleNotificationCommand(notification_command);

    NS_RELEASE(notification_command);

    status = nsEventStatus_eConsumeNoDefault;
  }

  nsCalFetchEventsCommand * fetch_command  = nsnull;

  static NS_DEFINE_IID(kCalFetchEventsCommandCID, NS_CAL_FETCHEVENTS_COMMAND_CID);                 
  
  res = aCommand->QueryInterface(kCalFetchEventsCommandCID,(void**)&fetch_command);

  if (NS_OK == res)
  {
    HandleFetchCommand(fetch_command);

    NS_RELEASE(fetch_command);

    status = nsEventStatus_eConsumeNoDefault;
  }

  return status;
}

nsresult nsCalendarModel :: HandleNotificationCommand(nsXPFCNotificationStateCommand * aCommand)
{
  /*
   * Steve, here is where you want to do the Flush() on CAPI
   * to get the list of events.
   *
   * Be sure to check aCommand->mCommandState to ensure it has
   * value nsCommandState_eComplete first.
   *
   * For Now, I've just passed the ModelUpdate Command directly to
   * the Observers, but you should do this when data comes back, with
   * the correct status in the model update (ie more data to come, data
   * complete, etc...)
   *
   * .... Greg
   */

  SendModelUpdateCommand();


  return NS_OK;
}

nsresult nsCalendarModel :: HandleFetchCommand(nsCalFetchEventsCommand * aCommand)
{
  /*
   * Steve, here is where you want to do deal with a specific request
   * for a range of data
   *
   * .... Greg
   */

  return NS_OK;
}

nsresult nsCalendarModel :: Attach(nsIXPFCObserver * aObserver)
{
  return NS_OK;
}

nsresult nsCalendarModel :: Detach(nsIXPFCObserver * aObserver)
{
  return NS_OK;
}

nsresult nsCalendarModel :: SendModelUpdateCommand()
{
  nsXPFCModelUpdateCommand * command  = nsnull;
  nsresult res;

  static NS_DEFINE_IID(kModelUpdateCommandCID, NS_XPFC_MODELUPDATE_COMMAND_CID);                 

  res = nsRepository::CreateInstance(kModelUpdateCommandCID, 
                                     nsnull, 
                                     kXPFCCommandIID, 
                                     (void **)&command);

  if (NS_OK == res)
  {
    nsIModel * model;
    
    QueryInterface(kIModelIID, (void**)&model);

    command->Init();

    command->mModel = model;

    Notify(command);

    NS_IF_RELEASE(command);
    NS_RELEASE(model);
  }

  return NS_OK;

}

nsresult nsCalendarModel :: Notify(nsIXPFCCommand * aCommand)
{
  nsresult res = NS_OK;

  nsIXPFCSubject * subject;

  res = QueryInterface(kXPFCSubjectIID,(void **)&subject);

  if (res != NS_OK)
    return res;

  nsIXPFCObserverManager* om;

  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);

  res = om->Notify(subject,aCommand);

  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  NS_RELEASE(subject);

  return(res);
}
