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

#ifndef nsICapi_h___
#define nsICapi_h___

#include "nsISupports.h"

#include "capi.h"

//88e55e80-41fa-11d2-924a-00805f8a7ab6
#define NS_ICAPI_IID   \
{ 0x88e55e80, 0x41fa, 0x11d2,    \
{ 0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsICapi : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;
  
  NS_IMETHOD_(CAPIStatus) CAPI_Capabilities( 
    const char** ppsVal,        /* o: a string describing the capabilities  */
    const char* psHost,         /* i: server host  */
    long lFlags ) = 0;              /* i: bit flags (none at this time; set to 0)  */
                                /*    were processed.  */

NS_IMETHOD_(CAPIStatus) CAPI_DeleteEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for delete  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* psUID,                /* i: UID of the event to delete  */
    char* dtRecurrenceID,       /* i: recurrence-id, NULL means ignore  */
    int iModifier) = 0;             /* i: one of CAPI_THISINSTANCE,  */
                                /*    CAPI_THISANDPRIOR, CAPI_THISANDFUTURE  */
                                /*    only valid if recurrence-id is non-NULL  */

NS_IMETHOD_(CAPIStatus) CAPI_DestroyHandles( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pHList,         /* i: pointer to a list of handles to destroy  */
    int iHandleCount,           /* i: number of valid handles in pHList  */
    long lFlags) = 0;               /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_DestroyStreams( 
    CAPISession s,              /* i: login session handle  */
    CAPIStream* pS,             /* i: array of streams to destroy  */
    int iHandlCount,            /* i: number of valid handles in ppH  */
    long lFlags) = 0;               /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_FetchEventsByAlarmRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for Fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time, ex: "19980704T080000Z"  */
    char* dEnd,                 /* i: range end time, ex: "19980704T180000Z"  */
    char** ppsPropList,         /* i: list of properties to return in events  */
    int iPropCount,             /* i: number of properties in *ppsPropList  */
    CAPIStream stream) = 0;         /* i: stream to which solution set will be written  */

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
    CAPIStream stream) = 0;         /* i: stream to which solution set will be written  */

NS_IMETHOD_(CAPIStatus) CAPI_FetchEventsByRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time  */
    char* dEnd,                 /* i: range end time  */
    char** ppsPropList,         /* i: list of properties returned in events  */
    int iPropCount,             /* i: number of properties in the list  */
    CAPIStream stream) = 0;         /* i: stream to which solution set will be written  */

NS_IMETHOD_(CAPIStatus) CAPI_GetHandle( 
    CAPISession s,              /* i: login session handle  */
    char* u,                    /* i: user as defined in Login  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIHandle* pH) = 0;            /* o: handle  */

NS_IMETHOD_(CAPIStatus) CAPI_Logoff( 
    CAPISession* s,             /* io: session from login  */
    long lFlags) = 0;               /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_Logon( 
    const char* psUser,         /* i: Calendar store (and ":extra" information )  */
    const char* psPassword,     /* i: password for sUser  */
    const char* psHost,         /* i: calendar server host (and :port)  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* pSession) = 0;     /* o: the session  */

NS_IMETHOD_(CAPIStatus) CAPI_SetStreamCallbacks ( 
    CAPISession s,
    CAPIStream* pStream,        /* io: The stream to modify  */
    CAPICallback pfnSndCallback,/* i: Snd iCalendar data    */
    void* userDataSnd,          /* i: a user supplied value */
    CAPICallback pfnRcvCallback,/* i: Rcv iCalendar data  */
    void* userDataRcv,          /* i: a user supplied value */
    long lFlags ) = 0;              /* i: bit flags (none at this time; set to 0)  */

NS_IMETHOD_(CAPIStatus) CAPI_StoreEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for store  */
    int iHandleCount,           /* i: number of valid handles in pH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIStream stream ) = 0;        /* i: stream for reading data to store    */

};

#endif /* nsICapi_h___ */
