/*

  The contents of this file are subject to the Mozilla Public License Version
  1.0 //(the "MPL"); you may not use this file except in compliance with the
  MPL. You //may obtain a copy of the NPL at http://www.mozilla.org/MPL/

  Software distributed under the MPL is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL for the
  specific language governing rights and limitations under the MPL.

  The Initial Developer of this code under the MPL is Corporate Software &
  Technologies Int, Inc. Portions created by Corporate Software & Technologies
  Int, Inc are Copyright (C) 1998 Corporate Software & Technologies Int,
  Inc. All Rights Reserved.

 */


/* This is the vendor specific header file for the capi C API library.  

   This header file contains a few miscellaneous definitions, the function
   prototypes, datastructre and error code definitions, specific to CS&T.

*/


#ifndef _CAPI_HEADER_
#define _CAPI_HEADER_

/* Version information */

#define CAPI_VERSION_NAME "CAPI Version 0.4"
#define CAPI_VERSION_NUMBER "//CS&T//CAPI B.00.04 private beta//EN"
#define CAPI_VERSION_EXPIRY_DATE "19980918T040000Z"

/* #define FUNCTION_DEF in the following place. */

#ifndef FUNCTION_DEF
/* please #define FUNCTION_DEF(a)   here */
#endif

/* To make the header C++ friendly: */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Stuff for using the functions, a few definitions */


  /* If a callback function returns an error this error will be returned by the
   * function which used the callback.  To allow the unambiguous identification
   * of an error which was returned by the call back they will be returned cast
   * to type CAPIStatus with the bits in the following mask set.  A regular
   * CAPI error will never have these bits set.
   **/

#define CAPI_CALLBACK_ERR_MASK   ((CAPIStatus) 0x80000000)


  /* There is one additional function for this API. */


NS_CALENDAR(CAPIStatus) CAPI_GetLastStoredUIDs(
    CAPISession pSession,  /* i: The session to fetch UID from */
    char *** UIDs,          /* o: malloced array of strings containing UIDs */
    long * size,            /* o: size of array */
    long lFlags);           /* i: 0 at this time */




#ifdef __cplusplus
}
#endif /* __cplusplus */

/* 
 * The error codes returned by CAPI library functions follow.  Error codes are
 * often context-sensitive, so explanations of their meanings are included with
 * the function descriptions.  
 **/


#ifndef CAPI_ID_ERR
#define CAPI_ID_ERR              0x00019800
#endif

#define CAPI_ERR_OK                       ((CAPIStatus)               0x00)

  /* Critical errors */

#define CAPI_ERR_EXPIRED                  ((CAPIStatus) CAPI_ID_ERR + 0x01)
#define CAPI_ERR_FLAGS                    ((CAPIStatus) CAPI_ID_ERR + 0x02)
#define CAPI_ERR_NO_MEMORY                ((CAPIStatus) CAPI_ID_ERR + 0x03)
#define CAPI_ERR_NULL_PARAMETER           ((CAPIStatus) CAPI_ID_ERR + 0x04)
#define CAPI_ERR_INIFILE                  ((CAPIStatus) CAPI_ID_ERR + 0x05)
#define CAPI_ERR_CORRUPT_SESSION          ((CAPIStatus) CAPI_ID_ERR + 0x06)
#define CAPI_ERR_CORRUPT_HANDLE           ((CAPIStatus) CAPI_ID_ERR + 0x07)
#define CAPI_ERR_CORRUPT_STREAM           ((CAPIStatus) CAPI_ID_ERR + 0x08)
#define CAPI_ERR_IO                       ((CAPIStatus) CAPI_ID_ERR + 0x09)
#define CAPI_ERR_FILE                     ((CAPIStatus) CAPI_ID_ERR + 0x0A)
#define CAPI_ERR_CALLBACK                 ((CAPIStatus) CAPI_ID_ERR + 0x0B)
#define CAPI_ERR_SECURITY                 ((CAPIStatus) CAPI_ID_ERR + 0x0C)
#define CAPI_ERR_NOT_SUPPORTED            ((CAPIStatus) CAPI_ID_ERR + 0x0D)

  /* Data errors */

#define CAPI_ERR_NULL_HANDLE              ((CAPIStatus) CAPI_ID_ERR + 0x21)
#define CAPI_ERR_NULL_STREAM              ((CAPIStatus) CAPI_ID_ERR + 0x22)
#define CAPI_ERR_NULL_SESSION             ((CAPIStatus) CAPI_ID_ERR + 0x23)
#define CAPI_ERR_BAD_PARAMETER            ((CAPIStatus) CAPI_ID_ERR + 0x24)
#define CAPI_ERR_USER_X400                ((CAPIStatus) CAPI_ID_ERR + 0x25)
#define CAPI_ERR_USER_NONE                ((CAPIStatus) CAPI_ID_ERR + 0x26)
#define CAPI_ERR_USER_MANY                ((CAPIStatus) CAPI_ID_ERR + 0x27)
#define CAPI_ERR_PASS                     ((CAPIStatus) CAPI_ID_ERR + 0x28)
#define CAPI_ERR_HOST                     ((CAPIStatus) CAPI_ID_ERR + 0x29)
#define CAPI_ERR_SERVER                   ((CAPIStatus) CAPI_ID_ERR + 0x2A)
#define CAPI_ERR_NODE                     ((CAPIStatus) CAPI_ID_ERR + 0x2B)
#define CAPI_ERR_DATE_RANGE               ((CAPIStatus) CAPI_ID_ERR + 0x2C)
#define CAPI_ERR_DATE_STRING              ((CAPIStatus) CAPI_ID_ERR + 0x2D)
#define CAPI_ERR_UID                      ((CAPIStatus) CAPI_ID_ERR + 0x2E)
#define CAPI_ERR_AGENDA                   ((CAPIStatus) CAPI_ID_ERR + 0x2F)

  /* Parse errors  (Data errors for MIME and Icalendar parsing. */

#define CAPI_ERR_PARSE_ICAL               ((CAPIStatus) CAPI_ID_ERR + 0x31)
#define CAPI_ERR_PARSE_MIME               ((CAPIStatus) CAPI_ID_ERR + 0x32)

  /* An internal error occurred, either the result of an unexpected event,
     or a bug. */

#define CAPI_ERR_INTERNAL                 ((CAPIStatus) CAPI_ID_ERR + 0x40)

  /* non-fatal errors, indicating that the operation was successful, but under
     unusual circumstances, which could be a mistake.  These errors are not all
     implemented in the code yet. */

#define CAPI_NOHANDLES_NONFATAL_ERR       ((CAPIStatus) CAPI_ID_ERR + 0xC1)
#define CAPI_NOEVENTS_NONFATAL_ERR        ((CAPIStatus) CAPI_ID_ERR + 0xC2)
#define CAPI_CANTINVITE_NONFATAL_ERR      ((CAPIStatus) CAPI_ID_ERR + 0xC3)
#define CAPI_STORE_NONFATAL_ERR           ((CAPIStatus) CAPI_ID_ERR + 0xC4)
#define CAPI_BADINPUT_NONFATAL_ERR        ((CAPIStatus) CAPI_ID_ERR + 0xC5)
#define CAPI_ERR_NONFATAL_EVENT_IGNORED   ((CAPIStatus) CAPI_ID_ERR + 0xC6)
#define CAPI_ERR_NONFATAL_PROP_RESTRICT   ((CAPIStatus) CAPI_ID_ERR + 0xC7)

#endif /* _CAPI_HEADER_ */
