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
#include "nsxpfcCIID.h"
#include "nsICalendarUser.h"

static NS_DEFINE_IID(kICalendarModelIID, NS_ICALENDAR_MODEL_IID);
static NS_DEFINE_IID(kIModelIID,         NS_IMODEL_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);

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

  return (NS_NOINTERFACE);

}


NS_IMPL_ADDREF(nsCalendarModel)
NS_IMPL_RELEASE(nsCalendarModel)

nsCalendarModel::~nsCalendarModel()
{
  mCalendarUser = nsnull; // Do Not Release
}

nsresult nsCalendarModel::Init()
{
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

