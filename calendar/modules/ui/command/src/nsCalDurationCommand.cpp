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
#include "nsCalDurationCommand.h"
#include "nsCalUICIID.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);

nsCalDurationCommand :: nsCalDurationCommand()
{
  NS_INIT_REFCNT();
  mDuration = nsnull;
  mPeriodFormat = nsCalPeriodFormat_kHour;
}

nsCalDurationCommand :: ~nsCalDurationCommand()  
{
  mDuration = nsnull;
}

NS_IMPL_ADDREF(nsCalDurationCommand)
NS_IMPL_RELEASE(nsCalDurationCommand)

nsresult nsCalDurationCommand::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kXPFCCommandIID);                         
  static NS_DEFINE_IID(kCalDurationCommandCID, NS_CAL_DURATION_COMMAND_CID);                         

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
  if (aIID.Equals(kCalDurationCommandCID)) {                                      
    *aInstancePtr = (void*)(nsCalDurationCommand *) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (NS_NOINTERFACE);

}

nsresult nsCalDurationCommand::Init(nsDuration * aDuration)
{
  mDuration = aDuration;
  return NS_OK;
}

nsresult nsCalDurationCommand::SetDuration(nsDuration * aDuration)
{
  mDuration = aDuration;
  return NS_OK;
}

nsDuration * nsCalDurationCommand::GetDuration()
{
  return (mDuration);
}

nsresult nsCalDurationCommand::SetPeriodFormat(nsCalPeriodFormat aPeriodFormat)
{
  mPeriodFormat = aPeriodFormat;
  return NS_OK;
}

nsCalPeriodFormat nsCalDurationCommand::GetPeriodFormat()
{
  return (mPeriodFormat);
}
