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
#include "nsCalStreamListener.h"
#include "nsIDTD.h"
#include "nsCalendarShell.h"
#include "nsIPref.h"
#include "nscalstrings.h"
#include "nsxpfcCIID.h"

//b770bd50-1aab-11d2-9246-00805f8a7ab6
#define NS_ICALSTREAMLISTENER_IID \
{ 0xb770bd50, 0x1aab, 0x11d2, \
{ 0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID,     NS_ICALSTREAMLISTENER_IID); 

nsresult nsCalStreamListener::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsCalStreamListener*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_CALENDAR nsresult NS_NewCalStreamListener(nsIStreamListener** aInstancePtrResult)
{
  nsCalStreamListener * it = new nsCalStreamListener();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsCalStreamListener)
NS_IMPL_RELEASE(nsCalStreamListener)

nsCalStreamListener::nsCalStreamListener()
{
  NS_INIT_REFCNT();
  mWidget = nsnull;
}

nsCalStreamListener::~nsCalStreamListener()
{
}

nsresult nsCalStreamListener::GetBindInfo(nsIURL * aURL)
{
  return NS_OK;
}

nsresult nsCalStreamListener::OnDataAvailable(nsIURL * aURL,
					      nsIInputStream *aIStream, 
                                              PRInt32 aLength)
{
  if (mWidget)
    return (mWidget->GetParserStreamInterface()->OnDataAvailable(aURL,
								 aIStream, 
								 aLength));

  return NS_OK;
}

nsresult nsCalStreamListener::OnStartBinding(nsIURL * aURL, 
					     const char *aContentType)
{
  if (mWidget)
    return (mWidget->GetParserStreamInterface()->OnStartBinding(aURL,aContentType));

  return NS_OK;
}

nsresult nsCalStreamListener::OnStopBinding(nsIURL * aURL, PRInt32 aStatus, const nsString &aMsg)
{
#ifndef XP_UNIX
  static PRUint32 done = 0 ;
  // XXX Hack til get StreamManager in place
  if (mWidget && done == 0)
  {
    nsresult res = mWidget->GetParserStreamInterface()->OnStopBinding(aURL, aStatus, aMsg);

    char pUI[1024];
    int i = 1024;
    ((nsCalendarShell *)mWidget->mCalendarShell)->mShellInstance->GetPreferences()->GetCharPref(CAL_STRING_PREF_JULIAN_UI_XML_CALENDAR,pUI,&i); 
    ((nsCalendarShell *)mWidget->mCalendarShell)->mDocumentContainer->LoadURL(pUI,nsnull);
    done = 1;
    return res;
  }
#endif
  if (mWidget)
    return (mWidget->GetParserStreamInterface()->OnStopBinding(aURL, aStatus, aMsg));
  
  return NS_OK;
}

nsresult nsCalStreamListener::OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

nsresult nsCalStreamListener::OnStatus(nsIURL* aURL, const nsString &aMsg)
{
  return NS_OK;
}


nsresult nsCalStreamListener::SetWidget(nsCalendarWidget * aWidget)
{
  mWidget = aWidget;
  return NS_OK;
}




