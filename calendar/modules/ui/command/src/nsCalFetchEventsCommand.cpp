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
#include "nsCalFetchEventsCommand.h"
#include "nsCalUICIID.h"
#include "nsCalUtilCIID.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);

nsCalFetchEventsCommand :: nsCalFetchEventsCommand()
{
  NS_INIT_REFCNT();
  mStartDate = nsnull;
  mEndDate = nsnull;
}

nsCalFetchEventsCommand :: ~nsCalFetchEventsCommand()  
{
}

NS_IMPL_ADDREF(nsCalFetchEventsCommand)
NS_IMPL_RELEASE(nsCalFetchEventsCommand)

nsresult nsCalFetchEventsCommand::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kXPFCCommandIID);                         
  static NS_DEFINE_IID(kCalFetchEventsCommandCID, NS_CAL_FETCHEVENTS_COMMAND_CID);                         

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
  if (aIID.Equals(kCalFetchEventsCommandCID)) {                                      
    *aInstancePtr = (void*)(nsCalFetchEventsCommand *) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (NS_NOINTERFACE);

}

nsresult nsCalFetchEventsCommand::Init()
{
  return NS_OK ;
}

