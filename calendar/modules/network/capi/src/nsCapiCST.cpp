/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL") you may not use this file except in
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

#include "nsCapiCST.h"
#include "nsCapiCIID.h"
#include "nspr.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCCapiCSTCID,   NS_CAPI_CST_CID);
static NS_DEFINE_IID(kICapiIID,      NS_ICAPI_IID);

#ifdef XP_PC
  #define CAPI_LOCAL_NAME "capi.dll"
#else
  #define CAPI_LOCAL_NAME "bogus.dll"
#endif


nsCapiCST :: nsCapiCST(nsISupports* outer)
{
  NS_INIT_REFCNT();
  mLibrary = nsnull;
}

nsCapiCST :: ~nsCapiCST()
{
  if (nsnull != mLibrary)
    PR_UnloadLibrary(mLibrary);
}

nsresult nsCapiCST::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kCCapiCSTCID);
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsCapiCST *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kICapiIID)) {                                          
    *aInstancePtr = (void*) (nsICapi *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (NS_NOINTERFACE);
}

NS_IMPL_ADDREF(nsCapiCST)
NS_IMPL_RELEASE(nsCapiCST)

nsresult nsCapiCST :: Init()
{
  mLibrary = PR_LoadLibrary(CAPI_LOCAL_NAME);

  mFunctions.capabilities                 = (Capabilities) PR_FindSymbol(mLibrary, "CAPI_Capabilities");
  mFunctions.delete_event                 = (DeleteEvent) PR_FindSymbol(mLibrary, "CAPI_DeleteEvent");
  mFunctions.destroy_handles              = (DestroyHandles) PR_FindSymbol(mLibrary, "CAPI_DestroyHandles");
  mFunctions.destroy_streams              = (DestroyStreams) PR_FindSymbol(mLibrary, "CAPI_DestroyStreams");
  mFunctions.fetch_events_by_alarm_range  = (FetchEventsByAlarmRange) PR_FindSymbol(mLibrary, "CAPI_FetchEventsByAlarmRange");
  mFunctions.fetch_events_by_id           = (FetchEventsByID) PR_FindSymbol(mLibrary, "CAPI_FetchEventsByID");
  mFunctions.fetch_events_by_range        = (FetchEventsByRange) PR_FindSymbol(mLibrary, "CAPI_FetchEventsByRange");
  mFunctions.get_handle                   = (GetHandle) PR_FindSymbol(mLibrary, "CAPI_GetHandle");
  mFunctions.logoff                       = (Logoff) PR_FindSymbol(mLibrary, "CAPI_Logoff");
  mFunctions.logon                        = (Logon) PR_FindSymbol(mLibrary, "CAPI_Logon");
  mFunctions.set_stream_callbacks         = (SetStreamCallbacks) PR_FindSymbol(mLibrary, "CAPI_SetStreamCallbacks");
  mFunctions.store_event                  = (StoreEvent) PR_FindSymbol(mLibrary, "CAPI_StoreEvent");

  return NS_OK;
}


CAPIStatus nsCapiCST :: CAPI_Capabilities(const char** ppsVal,
                                            const char* psHost,
                                            long lFlags)
{
  return (mFunctions.capabilities(ppsVal, psHost, lFlags));
}

CAPIStatus nsCapiCST :: CAPI_DeleteEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for delete  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* psUID,                /* i: UID of the event to delete  */
    char* dtRecurrenceID,       /* i: recurrence-id, NULL means ignore  */
    int iModifier)             /* i: one of CAPI_THISINSTANCE,  */
                                /*    CAPI_THISANDPRIOR, CAPI_THISANDFUTURE  */
                                /*    only valid if recurrence-id is non-NULL  */
{
  return (mFunctions.delete_event(s,pH,iHandleCount,lFlags,psUID,dtRecurrenceID,iModifier));
}


CAPIStatus nsCapiCST :: CAPI_DestroyHandles( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pHList,         /* i: pointer to a list of handles to destroy  */
    int iHandleCount,           /* i: number of valid handles in pHList  */
    long lFlags)
{
  return (mFunctions.destroy_handles(s,pHList,iHandleCount,lFlags));
}


CAPIStatus nsCapiCST :: CAPI_DestroyStreams( 
    CAPISession s,              /* i: login session handle  */
    CAPIStream* pS,             /* i: array of streams to destroy  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags)                /* i: bit flags (none at this time; set to 0)  */
{
    return (mFunctions.destroy_streams(s,pS,iHandleCount,lFlags));
}

CAPIStatus nsCapiCST :: CAPI_FetchEventsByAlarmRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for Fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time, ex: "19980704T080000Z"  */
    char* dEnd,                 /* i: range end time, ex: "19980704T180000Z"  */
    char** ppsPropList,         /* i: list of properties to return in events  */
    int iPropCount,             /* i: number of properties in *ppsPropList  */
    CAPIStream stream)         /* i: stream to which solution set will be written  */
{
    return (mFunctions.fetch_events_by_alarm_range(s,pH,iHandleCount,lFlags,dStart,dEnd,ppsPropList,iPropCount,stream));
}

CAPIStatus nsCapiCST :: CAPI_FetchEventsByID( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle h,               /* i: calendar from which to fetch events  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* psUID,                /* i: UID of the event to fetch  */
    char* dtRecurrenceID,       /* i: recurrence-id, NULL means ignore  */
    int iModifier,              /* i: one of CAPI_THISINSTANCE,  */
                                /*    CAPI_THISANDPRIOR, CAPI_THISANDFUTURE  */
                                /*    only valid if recurrence-id is non-NULL  */
    char** ppsPropList,         /* i: list of properties returned in events   */
    int iPropCount,             /* i: number of properties in the list  */
    CAPIStream stream)         /* i: stream to which solution set will be written  */
{
    return (mFunctions.fetch_events_by_id(s,h,lFlags,psUID,dtRecurrenceID,iModifier,ppsPropList,iPropCount,stream));
}

CAPIStatus nsCapiCST :: CAPI_FetchEventsByRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time  */
    char* dEnd,                 /* i: range end time  */
    char** ppsPropList,         /* i: list of properties returned in events  */
    int iPropCount,             /* i: number of properties in the list  */
    CAPIStream stream)         /* i: stream to which solution set will be written  */
{
    return (mFunctions.fetch_events_by_range(s,pH,iHandleCount,lFlags,dStart,dEnd,ppsPropList,iPropCount,stream));
}

CAPIStatus nsCapiCST :: CAPI_GetHandle( 
    CAPISession s,              /* i: login session handle  */
    char* u,                    /* i: user as defined in Login  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIHandle* pH)            /* o: handle  */
{
    return (mFunctions.get_handle(s,u,lFlags,pH));
}

CAPIStatus nsCapiCST :: CAPI_Logoff( 
    CAPISession* s,             /* io: session from login  */
    long lFlags)               /* i: bit flags (none at this time; set to 0)  */
{
    return (mFunctions.logoff(s,lFlags));
}

CAPIStatus nsCapiCST :: CAPI_Logon( 
    const char* psUser,         /* i: Calendar store (and ":extra" information )  */
    const char* psPassword,     /* i: password for sUser  */
    const char* psHost,         /* i: calendar server host (and :port)  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* pSession)     /* o: the session  */
{
    return (mFunctions.logon(psUser,psPassword,psHost,lFlags,pSession));
}

CAPIStatus nsCapiCST :: CAPI_SetStreamCallbacks ( 
    CAPISession s,
    CAPIStream* pStream,        /* io: The stream to modify  */
    CAPICallback pfnSndCallback,/* i: Snd iCalendar data    */
    void* userDataSnd,          /* i: a user supplied value */
    CAPICallback pfnRcvCallback,/* i: Rcv iCalendar data  */
    void* userDataRcv,          /* i: a user supplied value */
    long lFlags )              /* i: bit flags (none at this time; set to 0)  */
{
    return (mFunctions.set_stream_callbacks(s,pStream,pfnSndCallback,userDataSnd,pfnRcvCallback,userDataRcv,lFlags));
}

CAPIStatus nsCapiCST :: CAPI_StoreEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for store  */
    int iHandleCount,           /* i: number of valid handles in pH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIStream stream )        /* i: stream for reading data to store    */
{
    return (mFunctions.store_event(s,pH,iHandleCount,lFlags,stream));
}

