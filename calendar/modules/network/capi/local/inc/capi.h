/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

/*
 *  capi.h
 *
 *  sman
 *  30-Jun-98
 */

#ifndef __JULIAN_LOCAL_CAPI_H
#define __JULIAN_LOCAL_CAPI_H

#include "jdefines.h"
#include <unistring.h>
#include "nscore.h"
#include "nscalexport.h"

typedef void* CAPISession;
typedef void* CAPIHandle;
typedef void* CAPIStream;
typedef long CAPIStatus;

#define CAPI_THISINSTANCE       1
#define CAPI_THISANDPRIOR       2 
#define CAPI_THISANDFUTURE      3

#define CAPI_CALLBACK_CONTINUE  0
#define CAPI_CALLBACK_DONE     -1

#if 0
#define CAPI_ERR_OK 0

#define CAPI_ERR_CALLBACK 1
#define CAPI_ERR_COMP_NOT_FOUND 2
#define CAPI_ERR_CORRUPT_HANDLE 3
#define CAPI_ERR_CORRUPT_SESSION 4
#define CAPI_ERR_CORRUPT_STREAM 5
#define CAPI_ERR_DATE 6
#define CAPI_ERR_DATE_RANGE 7
#define CAPI_ERR_EXPIRED 8
#define CAPI_ERR_FLAGS 9
#define CAPI_ERR_HOST 10
#define CAPI_ERR_INTERNAL 11
#define CAPI_ERR_IO 12
#define CAPI_ERR_NO_MEMORY 13
#define CAPI_ERR_NOT_IMPLEMENTED 14
#define CAPI_ERR_NULL_PARAMETER 15
#define CAPI_ERR_PROPERTIES_BLOCKED 16
#define CAPI_ERR_REQUIRED_PROPERTY_MISSING 17
#define CAPI_ERR_SECURITY 18
#define CAPI_ERR_SERVER 19
#define CAPI_ERR_UID 20
#define CAPI_ERR_USERNAME_PASSWORD 21
#endif

typedef int (*CAPICallback)( 
    void* pData,                /* i: caller-defined data, the value  */
                                /*    supplied in CAPI_SetStreamCallbacks  */
    char* pBuf,                 /* i: buffer to read or write  */
    size_t iSize,               /* i: the number of characters in pBuf  */
    size_t* piTransferred);     /* o: the number of characters from pBuf that  */
 
 
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

NS_CALENDAR CAPIStatus CAPI_Capabilities( 
    const char** ppsVal,        /* o: a string describing the capabilities  */
    const char* psHost,         /* i: server host  */
    long lFlags );              /* i: bit flags (none at this time; set to 0)  */
                                /*    were processed.  */

NS_CALENDAR CAPIStatus CAPI_DeleteEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for delete  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* psUID,                /* i: UID of the event to delete  */
    char* dtRecurrenceID,       /* i: recurrence-id, NULL means ignore  */
    int iModifier);             /* i: one of CAPI_THISINSTANCE,  */
                                /*    CAPI_THISANDPRIOR, CAPI_THISANDFUTURE  */
                                /*    only valid if recurrence-id is non-NULL  */

NS_CALENDAR CAPIStatus CAPI_DestroyHandles( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pHList,         /* i: pointer to a list of handles to destroy  */
    int iHandleCount,           /* i: number of valid handles in pHList  */
    long lFlags);               /* i: bit flags (none at this time; set to 0)  */

NS_CALENDAR CAPIStatus CAPI_DestroyStreams( 
    CAPISession s,              /* i: login session handle  */
    CAPIStream* pS,             /* i: array of streams to destroy  */
    int iCount,                 /* i: number of valid handles in ppH  */
    long lFlags);               /* i: bit flags (none at this time; set to 0)  */

NS_CALENDAR CAPIStatus CAPI_FetchEventsByAlarmRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for Fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time, ex: "19980704T080000Z"  */
    char* dEnd,                 /* i: range end time, ex: "19980704T180000Z"  */
    char** ppsPropList,         /* i: list of properties to return in events  */
    int iPropCount,             /* i: number of properties in *ppsPropList  */
    CAPIStream stream);         /* i: stream to which solution set will be written  */

NS_CALENDAR CAPIStatus CAPI_FetchEventsByID( 
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

NS_CALENDAR CAPIStatus CAPI_FetchEventsByRange( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for fetch  */
    int iHandleCount,           /* i: number of valid handles in ppH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    char* dStart,               /* i: range start time  */
    char* dEnd,                 /* i: range end time  */
    char** ppsPropList,         /* i: list of properties returned in events  */
    int iPropCount,             /* i: number of properties in the list  */
    CAPIStream stream);         /* i: stream to which solution set will be written  */

NS_CALENDAR CAPIStatus CAPI_GetHandle( 
    CAPISession s,              /* i: login session handle  */
    char* u,                    /* i: user as defined in Login  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIHandle* pH);            /* o: handle  */

NS_CALENDAR CAPIStatus CAPI_Logoff( 
    CAPISession* s,             /* io: session from login  */
    long lFlags);               /* i: bit flags (none at this time; set to 0)  */

NS_CALENDAR CAPIStatus CAPI_Logon( 
    const char* psUser,         /* i: Calendar store (and ":extra" information )  */
    const char* psPassword,     /* i: password for sUser  */
    const char* psHost,         /* i: calendar server host (and :port)  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* pSession);     /* o: the session  */

NS_CALENDAR CAPIStatus CAPI_SetStreamCallbacks ( 
    CAPISession s,              /* i: login session handle  */
    CAPIStream* pStream,        /* io: The stream to modify  */
    CAPICallback pfnSndCallback,/* i: Snd iCalendar data    */
    void* userDataSnd,          /* i: a user supplied value */
    CAPICallback pfnRcvCallback,/* i: Rcv iCalendar data  */
    void* userDataRcv,          /* i: a user supplied value */
    long lFlags );              /* i: bit flags (none at this time; set to 0)  */

NS_CALENDAR CAPIStatus CAPI_StoreEvent( 
    CAPISession s,              /* i: login session handle  */
    CAPIHandle* pH,             /* i: list of CAPIHandles for store  */
    int iHandleCount,           /* i: number of valid handles in pH  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPIStream stream );        /* i: stream for reading data to store    */


/*
 *  NETSCAPE EXTENSIONS TO CAPI
 */
NS_CALENDAR CAPIStatus CAPI_LogonCurl( 
    const char* psCurl,         /* i: Calendar store (and ":extra" information )  */
    const char* psPassword,     /* i: password for sUser  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* pSession);     /* o: the session  */


#ifdef __cplusplus
}
#endif

/* The ok error */ 
#define CAPI_ERR_OK                   ((CAPIStatus) 0x00) 

/* Masks for checking for fields of error codes */ 
#define CAPI_ERRMASK_MODE_FIELD       ((CAPIStatus) 0xFF << 24) 
#define CAPI_ERRMASK_TYPE_FIELD       ((CAPIStatus) 0x1F << 19) 
#define CAPI_ERRMASK_ERR1_FIELD       ((CAPIStatus) 0x1F << 14) 
#define CAPI_ERRMASK_ERR2_FIELD       ((CAPIStatus) 0x3F << 8) 
#define CAPI_ERRMASK_VENDOR_FIELD     ((CAPIStatus) 0xFF) 

/* Masks for checking for errors at various levels */ 

#define CAPI_ERRMASK_TYPE    ((CAPIStatus) CAPI_ERRMASK_TYPE_FIELD) 
#define CAPI_ERRMASK_ERR1    ((CAPIStatus) CAPI_ERRMASK_TYPE + CAPI_ERRMASK_ERR1_FIELD) 
#define CAPI_ERRMASK_ERR2    ((CAPIStatus) CAPI_ERRMASK_ERR1 + CAPI_ERRMASK_ERR2_FIELD) 
#define CAPI_ERRMASK_VENDOR  ((CAPIStatus) CAPI_ERRMASK_ERR2 + CAPI_ERRMASK_VENDOR_FIELD) 

/* The non fatal error bit */ 

#define CAPI_ERRMODE_FATAL      ((CAPIStatus) 0x1 << 24)) 

/* The various error types (field 2) */ 
#define CAPI_ERRTYPE_DATA       ((CAPIStatus) 0x1 << 19) 
#define CAPI_ERRTYPE_SERVICE    ((CAPIStatus) 0x2 << 19) 
#define CAPI_ERRTYPE_API        ((CAPIStatus) 0x3 << 19) 
#define CAPI_ERRTYPE_SECURITY   ((CAPIStatus) 0x4 << 19) 
#define CAPI_ERRTYPE_LIBRARY    ((CAPIStatus) 0x5 << 19) 

/* Data errors  */ 

/* field 3 values */ 
#define CAPI_ERR1_ICAL    ((CAPIStatus) CAPI_ERRTYPE_DATA + (0x1 << 14 )) 
#define CAPI_ERR1_MIME    ((CAPIStatus) CAPI_ERRTYPE_DATA + (0x2 << 14 )) 
#define CAPI_ERR1_DATE    ((CAPIStatus) CAPI_ERRTYPE_DATA + (0x3 << 14 )) 
#define CAPI_ERR1_ID      ((CAPIStatus) CAPI_ERRTYPE_DATA + (0x4 << 14 )) 

/* field 4 values */ 
#define CAPI_ERR2_MIME_NONE    ((CAPIStatus) CAPI_ERR1_MIME + (0x1 << 8)) 
#define CAPI_ERR2_MIME_NOICAL  ((CAPIStatus) CAPI_ERR1_MIME + (0x2 << 8)) 

#define CAPI_ERR2_DATE_RANGE   ((CAPIStatus) CAPI_ERR1_DATE + (0x1 << 8)) 
#define CAPI_ERR2_DATE_FORMAT  ((CAPIStatus) CAPI_ERR1_DATE + (0x2 << 8)) 

#define CAPI_ERR2_ID_USERID    ((CAPIStatus) CAPI_ERR1_ID +   (0x1 << 8)) 
#define CAPI_ERR2_ID_HOSTNAME  ((CAPIStatus) CAPI_ERR1_ID +   (0x2 << 8)) 

/* field 5 values (Vendor specific errors) */ 

#define CAPI_ERR_IDUSERID_INIFILE ((CAPIStatus) CAPI_ERR2_ID_USERID + 0x01) 
#define CAPI_ERR_IDUSERID_FORMAT  ((CAPIStatus) CAPI_ERR2_ID_USERID + 0x02) 
#define CAPI_ERR_IDUSERID_NONE    ((CAPIStatus) CAPI_ERR2_ID_USERID + 0x03) 
#define CAPI_ERR_IDUSERID_MANY    ((CAPIStatus) CAPI_ERR2_ID_USERID + 0x04) 
#define CAPI_ERR_IDUSERID_NODE    ((CAPIStatus) CAPI_ERR2_ID_USERID + 0x05) 

/* 
 * Service errors 
 */ 

/* field 3 values */ 
#define CAPI_ERR1_MEMORY    ((CAPIStatus) CAPI_ERRTYPE_SERVICE + (0x1 << 14 )) 
#define CAPI_ERR1_FILE      ((CAPIStatus) CAPI_ERRTYPE_SERVICE + (0x2 << 14 )) 
#define CAPI_ERR1_NETWORK   ((CAPIStatus) CAPI_ERRTYPE_SERVICE + (0x3 << 14 )) 

/* field 4 values */ 
#define CAPI_ERR2_NETWORK_TIMEOUT ((CAPIStatus) CAPI_ERR1_NETWORK + (0x1 << 8)) 
  

/* 
 * API errors 
 */ 

/* field 3 values */ 
#define CAPI_ERR1_FLAGS     ((CAPIStatus) CAPI_ERRTYPE_API + (0x1 << 14)) 
#define CAPI_ERR1_NULLPARAM ((CAPIStatus) CAPI_ERRTYPE_API + (0x2 << 14)) 
#define CAPI_ERR1_CALLBACK  ((CAPIStatus) CAPI_ERRTYPE_API + (0x3 << 14)) 
#define CAPI_ERR1_HANDLE    ((CAPIStatus) CAPI_ERRTYPE_API + (0x4 << 14)) 
#define CAPI_ERR1_SESSION   ((CAPIStatus) CAPI_ERRTYPE_API + (0x5 << 14)) 
#define CAPI_ERR1_STREAM    ((CAPIStatus) CAPI_ERRTYPE_API + (0x6 << 14)) 

/* field 4 values */ 
#define CAPI_ERR2_HANDLE_NULL  ((CAPIStatus) CAPI_ERR1_HANDLE + (0x1 << 8)) 
#define CAPI_ERR2_HANDLE_BAD   ((CAPIStatus) CAPI_ERR1_HANDLE + (0x2 << 8)) 

#define CAPI_ERR2_SESSION_NULL ((CAPIStatus) CAPI_ERR1_SESSION + (0x1 << 8)) 
#define CAPI_ERR2_SESSION_BAD  ((CAPIStatus) CAPI_ERR1_SESSION + (0x2 << 8)) 

#define CAPI_ERR2_STREAM_NULL  ((CAPIStatus) CAPI_ERR1_STREAM + (0x1 << 8)) 
#define CAPI_ERR2_STREAM_BAD   ((CAPIStatus) CAPI_ERR1_STREAM + (0x2 << 8)) 

/* 
 * Security errors 
 */ 

/* field 3 values */ 
#define CAPI_ERR1_READ      ((CAPIStatus) CAPI_ERRTYPE_SECURITY + (0x1 << 14)) 
#define CAPI_ERR1_WRITE     ((CAPIStatus) CAPI_ERRTYPE_SECURITY + (0x2 << 14)) 

/* field 4 values */ 
#define CAPI_ERR2_READ_PROPS   ((CAPIStatus) CAPI_ERR1_READ + (0x1 << 8)) 

#define CAPI_ERR2_WRITE_AGENDA ((CAPIStatus) CAPI_ERR1_WRITE + (0x1 << 8)) 
#define CAPI_ERR2_WRITE_EVENT  ((CAPIStatus) CAPI_ERR1_WRITE + (0x2 << 8)) 
  

/* 
 * Library errors 
 */ 
/* field 3 values */ 
#define CAPI_ERR1_INTERNAL      ((CAPIStatus) CAPI_ERRTYPE_LIBRARY + (0x1 << 14)) 
#define CAPI_ERR1_IMPLENTATION  ((CAPIStatus) CAPI_ERRTYPE_LIBRARY + (0x2 << 14)) 

/* field 4 values */ 
#define CAPI_ERR2_INTERNALEXPIRY ((CAPIStatus) CAPI_ERR_INTERNAL + (0x1 << 8)) 

#endif  /* __JULIAN_LOCAL_CAPI_H */
