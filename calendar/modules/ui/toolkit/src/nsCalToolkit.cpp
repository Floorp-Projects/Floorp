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

#include "nsCalToolkit.h"
#include "nsCalUICIID.h"
#include "nsxpfcCIID.h"
#include "nsCalendarShell.h"
#include "nsXPFCCanvasManager.h"
#include "nsICalendarWidget.h"
#include "nsCalShellCIID.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalToolkitIID, NS_ICAL_TOOLKIT_IID);

nsCalToolkit * gCalToolkit = nsnull;

nsCalToolkit :: nsCalToolkit() : nsXPFCToolkit()
{
  NS_INIT_REFCNT();
  mCalendarShell = nsnull;

  if (gCalToolkit == nsnull)
    gCalToolkit = (nsCalToolkit *) this;

}

nsCalToolkit :: ~nsCalToolkit()  
{

  nsIApplicationShell * shell = (nsIApplicationShell *) mCalendarShell;

  NS_IF_RELEASE(shell);
}

NS_IMPL_ADDREF(nsCalToolkit)
NS_IMPL_RELEASE(nsCalToolkit)

nsresult nsCalToolkit::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalToolkitIID);                         
  static NS_DEFINE_IID(kCalToolkitCID, NS_CAL_TOOLKIT_CID);                         

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
  if (aIID.Equals(kCalToolkitCID)) {                                      
    *aInstancePtr = (void*)(nsCalToolkit *) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (NS_NOINTERFACE);

}


nsresult nsCalToolkit::Init(nsIApplicationShell * aApplicationShell)
{
  static NS_DEFINE_IID(kCCalendarShellCID, NS_CAL_SHELL_CID);

  aApplicationShell->QueryInterface(kCCalendarShellCID,(void **)&mCalendarShell);

  return (nsXPFCToolkit::Init(aApplicationShell));
}

CAPISession nsCalToolkit::GetCAPISession()
{
  return (mCalendarShell->GetCAPISession());
}

CAPIHandle nsCalToolkit::GetCAPIHandle()
{
  return (mCalendarShell->GetCAPIHandle());
}

char * nsCalToolkit::GetCAPIPassword()
{
  return (mCalendarShell->GetCAPIPassword());
}

NSCalendar * nsCalToolkit::GetNSCalendar()
{
  return (mCalendarShell->GetNSCalendar());
}
