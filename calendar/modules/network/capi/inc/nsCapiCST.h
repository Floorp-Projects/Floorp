/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsCapiCST_h___
#define nsCapiCST_h___

#include "nsICapi.h"
#include "nspr.h"
#include "capi.h"

typedef CAPIStatus (*Capabilities)(const char** ppsVal,const char* psHost,long lFlags);
typedef CAPIStatus (*DeleteEvent)(CAPISession s,CAPIHandle* pH,int iHandleCount,long lFlags,char* psUID,char* dtRecurrenceID,int iModifier);
typedef CAPIStatus (*DestroyHandles)(CAPISession s,CAPIHandle* pHList,int iHandleCount,long lFlags);
typedef CAPIStatus (*DestroyStreams)(CAPISession s,CAPIStream* pS,int iHandleCount,long lFlags);
typedef CAPIStatus (*FetchEventsByAlarmRange)(CAPISession s,CAPIHandle* pH,int iHandleCount,long lFlags,char* dStart,char* dEnd,char** ppsPropList,int iPropCount,CAPIStream stream);
typedef CAPIStatus (*FetchEventsByID)(CAPISession s,CAPIHandle h,long lFlags,char* psUID,char* dtRecurrenceID,int iModifier,char** ppsPropList,int iPropCount,CAPIStream stream);
typedef CAPIStatus (*FetchEventsByRange)(CAPISession s,CAPIHandle* pH,int iHandleCount,long lFlags,char* dStart,char* dEnd,char** ppsPropList,int iPropCount,CAPIStream stream) ;
typedef CAPIStatus (*GetHandle)(CAPISession s,char* u,long lFlags,CAPIHandle* pH);
typedef CAPIStatus (*Logoff)(CAPISession* s,long lFlags);
typedef CAPIStatus (*Logon)(const char* psUser,const char* psPassword,const char* psHost,long lFlags,CAPISession* pSession);
typedef CAPIStatus (*SetStreamCallbacks)(CAPISession s,CAPIStream* pStream,CAPICallback pfnSndCallback,void* userDataSnd,CAPICallback pfnRcvCallback,void* userDataRcv,long lFlags );
typedef CAPIStatus (*StoreEvent)(CAPISession s,CAPIHandle* pH,int iHandleCount,long lFlags,CAPIStream stream );
typedef CAPIStatus (*LogonCurl)(const char* psCurl,const char* psPassword,long lFlags,CAPISession* pSession);

typedef struct {
  Capabilities            capabilities;
  DeleteEvent             delete_event;
  DestroyHandles          destroy_handles;
  DestroyStreams          destroy_streams;
  FetchEventsByAlarmRange fetch_events_by_alarm_range;
  FetchEventsByID         fetch_events_by_id;
  FetchEventsByRange      fetch_events_by_range;
  GetHandle               get_handle;
  Logoff                  logoff;
  Logon                   logon;
  SetStreamCallbacks      set_stream_callbacks;
  StoreEvent              store_event;
} capi_cst_funcs ;


class nsCapiCST : public nsICapi
{
public:
  nsCapiCST(nsISupports* outer);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();


  NS_IMETHOD_(CAPIStatus) CAPI_Capabilities( 
    const char** ppsVal,        /* o: a string describing the capabilities  */
    const char* psHost,         /* i: server host  */
    long lFlags );              /* i: bit flags (none at this time; set to 0)  */
                                /*    were processed.  */

NS_IMETHOD_(CAPIStatus) CAPI_DeleteEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for delete  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* psUID,                /* i: UID of the event to delete  */
    char* dtRecurrenceID,       /* i: recurrence-id, NULL means ignore  */
    int iModifier);             /* i: one of CAPI_THISINSTANCE,  */
                                /*    CAPI_THISANDPRIOR, CAPI_THISANDFUTURE  */
                                /*    only valid if recurrence-id is non-NULL  */

NS_IMETHOD_(CAPIStatus) CAPI_DestroyHandles( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pHList,         /* i: pointer to a list of handles to destroy  */
    int iHandleCount,           /* i: number of valid handles in pHList  */
    long lFlags);               /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_DestroyStreams( 
    CAPISession s,              /* i: login session handle  */
    CAPIStream* pS,             /* i: array of streams to destroy  */
    int iHandlCount,            /* i: number of valid handles in ppH  */
    long lFlags);               /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_FetchEventsByAlarmRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for Fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time, ex: "19980704T080000Z"  */
    char* dEnd,                 /* i: range end time, ex: "19980704T180000Z"  */
    char** ppsPropList,         /* i: list of properties to return in events  */
    int iPropCount,             /* i: number of properties in *ppsPropList  */
    CAPIStream stream);         /* i: stream to which solution set will be written  */

NS_IMETHOD_(CAPIStatus) CAPI_FetchEventsByID( 
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
    CAPIStream stream);         /* i: stream to which solution set will be written  */

NS_IMETHOD_(CAPIStatus) CAPI_FetchEventsByRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time  */
    char* dEnd,                 /* i: range end time  */
    char** ppsPropList,         /* i: list of properties returned in events  */
    int iPropCount,             /* i: number of properties in the list  */
    CAPIStream stream);         /* i: stream to which solution set will be written  */

NS_IMETHOD_(CAPIStatus) CAPI_GetHandle( 
    CAPISession s,              /* i: login session handle  */
    char* u,                    /* i: user as defined in Login  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIHandle* pH);            /* o: handle  */

NS_IMETHOD_(CAPIStatus) CAPI_Logoff( 
    CAPISession* s,             /* io: session from login  */
    long lFlags);               /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_Logon( 
    const char* psUser,         /* i: Calendar store (and ":extra" information )  */
    const char* psPassword,     /* i: password for sUser  */
    const char* psHost,         /* i: calendar server host (and :port)  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* pSession);     /* o: the session  */

NS_IMETHOD_(CAPIStatus) CAPI_SetStreamCallbacks ( 
    CAPISession s,
    CAPIStream* pStream,        /* io: The stream to modify  */
    CAPICallback pfnSndCallback,/* i: Snd iCalendar data    */
    void* userDataSnd,          /* i: a user supplied value */
    CAPICallback pfnRcvCallback,/* i: Rcv iCalendar data  */
    void* userDataRcv,          /* i: a user supplied value */
    long lFlags );              /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_StoreEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for store  */
    int iHandleCount,           /* i: number of valid handles in pH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIStream stream );        /* i: stream for reading data to store    */

protected:
  ~nsCapiCST();

private:
  PRLibrary * mLibrary;
  capi_cst_funcs  mFunctions;

};

#endif /* nsCapiCST_h___ */
