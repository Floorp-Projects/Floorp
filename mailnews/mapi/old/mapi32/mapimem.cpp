/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
//
//  mem.cpp
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//  This implements various memory management functions for use with
//  MAPI features of Communicator
//
#include <windows.h>     
#include <memory.h> 
#include <malloc.h>
#include "mapimem.h"
#include <nscpmapi.h>   // lives in communicator winfe
#include "nsstrseq.h"
#include "trace.h"
#include "mapiutl.h"
#include "xpapi.h"

LPSTR
CheckNullString(LPSTR inStr)
{
  static UCHAR  str[1];

  str[0] = '\0';
  if (inStr == NULL)
    return((LPSTR)str);
  else
    return(inStr);
}

void
FreeMAPIFile(lpMapiFileDesc pv)
{
  if (!pv)
    return;

  if (pv->lpszPathName != NULL)   
    free(pv->lpszPathName);

  if (pv->lpszFileName != NULL)   
    free(pv->lpszFileName);
}

void
FreeMAPIMessage(lpMapiMessage pv)
{
  ULONG i;

  if (!pv)
    return;

  if (pv->lpszSubject != NULL)
    free(pv->lpszSubject);

  if (pv->lpszNoteText)
      free(pv->lpszNoteText);
  
  if (pv->lpszMessageType)
    free(pv->lpszMessageType);
  
  if (pv->lpszDateReceived)
    free(pv->lpszDateReceived);
  
  if (pv->lpszConversationID)
    free(pv->lpszConversationID);
  
  if (pv->lpOriginator)
    FreeMAPIRecipient(pv->lpOriginator);
  
  for (i=0; i<pv->nRecipCount; i++)
  {
    if (&(pv->lpRecips[i]) != NULL)
    {
      FreeMAPIRecipient(&(pv->lpRecips[i]));
    }
  }

  if (pv->lpRecips != NULL)
  {
    free(pv->lpRecips);
  }

  for (i=0; i<pv->nFileCount; i++)
  {
    if (&(pv->lpFiles[i]) != NULL)
    {
      FreeMAPIFile(&(pv->lpFiles[i]));
    }
  }

  if (pv->lpFiles != NULL)
  {
    free(pv->lpFiles);
  }
  
  free(pv);
  pv = NULL;
}

void
FreeMAPIRecipient(lpMapiRecipDesc pv)
{
  if (!pv)
    return;

  if (pv->lpszName != NULL)   
    free(pv->lpszName);

  if (pv->lpszAddress != NULL)
    free(pv->lpszAddress);

  if (pv->lpEntryID != NULL)
    free(pv->lpEntryID);  
}

//
// This routine will take an lpMapiMessage structure and "flatten" it into
// one contiguous chunk of memory that can be easily passed around. After this
// is done, "extract" routines will be written to get complicated string routines
// out of the chunk of memory at the end.
// 
LPVOID
FlattenMAPIMessageStructure(lpMapiMessage msg, DWORD *totalSize)
{
  MAPISendMailType    *mailPtr;
  LPSTR               *strArray;
  DWORD               strCount = 0;
  DWORD               currentString = 0;
  DWORD               arrayBufSize = 0;
  DWORD               i;

  *totalSize = 0;
  if (!msg)
    return(NULL);

  //
  // Allocate the initial structure to hold all of the mail info.
  //
  *totalSize = sizeof(MAPISendMailType);
  mailPtr = (MAPISendMailType *) malloc(sizeof(MAPISendMailType));
  if (!mailPtr)
    return(NULL);
  memset(mailPtr, 0, sizeof(MAPISendMailType));

  //
  // First, assign all of the easy numeric values...
  //
  mailPtr->MSG_flFlags = msg->flFlags;          // unread,return receipt
  mailPtr->MSG_nRecipCount = msg->nRecipCount;  // Number of recipients
  mailPtr->MSG_nFileCount = msg->nFileCount;    // # of file attachments
  if (msg->lpOriginator != NULL)
  {
    mailPtr->MSG_ORIG_ulRecipClass = msg->lpOriginator->ulRecipClass; //  Recipient class - MAPI_TO, MAPI_CC, MAPI_BCC, MAPI_ORIG
  }

  //
  // Now, figure out how many string pointers we need...
  //
  strCount = 4;     // These are the 4 KNOWN strings up front for a message
  strCount += 2;    // This is for the originator name and address

  strCount += msg->nRecipCount * 3; // Name, address & class (cc, bcc) for each recipient
  strCount += msg->nFileCount  * 2; // filename and display name for each attachment

  // 
  // Now allocate a new string sequence...add one entry for NULL at the end
  //  
  arrayBufSize = sizeof(LPSTR) * (strCount + 1);

#ifdef WIN16    // Check for max mem allocation...
  if ((sizeof(MAPISendMailType) + arrayBufSize) > 64000)
  {
    free(mailPtr);
    return NULL;
  }
#endif

  //
  // Allocate a buffer for the string pointers and if this fails, 
  // cleanup and return.
  // 
  strArray = (LPSTR *)malloc( (size_t) arrayBufSize);
  if (!strArray)
  {
    free(mailPtr);
    return NULL;
  }

  memset(strArray, 0, (size_t) arrayBufSize);  // Set the array to NULL
  strArray[currentString++] = CheckNullString(msg->lpszSubject);        // Message Subject
  strArray[currentString++] = CheckNullString(msg->lpszNoteText);       // Message Text  
  strArray[currentString++] = CheckNullString(msg->lpszDateReceived);   // in YYYY/MM/DD HH:MM format
  strArray[currentString++] = CheckNullString(msg->lpszConversationID); // conversation thread ID

  if (msg->lpOriginator)
  {
    strArray[currentString++] = CheckNullString(msg->lpOriginator[0].lpszName);
    strArray[currentString++] = CheckNullString(msg->lpOriginator[0].lpszAddress);
  }
  else
  {
    strArray[currentString++] = CheckNullString(NULL);
    strArray[currentString++] = CheckNullString(NULL);
  }

  //
  // Assign pointers for the Name and address of each recipient
  //
  LPSTR toString  = "1";
  LPSTR ccString  = "2";
  LPSTR bccString = "3";

  for (i=0; i<msg->nRecipCount; i++)
  {
    // rhp - need message class
    if (msg->lpRecips[i].ulRecipClass == MAPI_BCC)
      strArray[currentString++] = CheckNullString(bccString);
    else if (msg->lpRecips[i].ulRecipClass == MAPI_CC)
      strArray[currentString++] = CheckNullString(ccString);
    else
      strArray[currentString++] = CheckNullString(toString);

    strArray[currentString++] = CheckNullString(msg->lpRecips[i].lpszName);
    strArray[currentString++] = CheckNullString(msg->lpRecips[i].lpszAddress);  
  }

  BYTE    szNewFileName[_MAX_PATH];

  for (i=0; i<msg->nFileCount; i++)
  {
    char *namePtr;
    // have to copy/create temp files here of office won't work...

    if ( 
          (msg->lpFiles[i].lpszFileName != NULL) &&
          (*msg->lpFiles[i].lpszFileName != '\0') 
       )
    {
      namePtr = (char *)msg->lpFiles[i].lpszFileName;
    }
    else
    {
      namePtr = (char *)msg->lpFiles[i].lpszPathName;
    }

    if (GetTempMailNameWithExtension((char *)szNewFileName, namePtr) == 0)
    {
      free(strArray);
      free(mailPtr);
      return NULL;
    }
    
    if (!XP_CopyFile((char *)msg->lpFiles[i].lpszPathName, (char *)szNewFileName, TRUE))
    {
      free(strArray);
      free(mailPtr);
      return NULL;
    }

    strArray[currentString++] = CheckNullString((char *)szNewFileName);
    strArray[currentString++] = CheckNullString(msg->lpFiles[i].lpszFileName);

    AddTempFile((LPSTR) szNewFileName);
//    strArray[currentString++] = CheckNullString(msg->lpFiles[i].lpszPathName);
//    strArray[currentString++] = CheckNullString(msg->lpFiles[i].lpszFileName);
  }

  if (currentString != strCount)
  {
    TRACE("MAPI PROBLEM!!!!!! FlattenMAPIMessageStructure() currentString != strCount\n");
  }

  strArray[strCount] = NULL;    // terminate at the end
  NSstringSeq strSeq = NSStrSeqNew(strArray);
  if (!strSeq)
  {
    free(strArray);
    free(mailPtr);
    return NULL;
  }
  
  //
  // Now we need to copy the structure into a big, contiguous chunk of memory
  //
  LONG totalArraySize = NSStrSeqSize(strSeq);
  LONG totalMemSize = sizeof(MAPISendMailType) + totalArraySize;

#ifdef WIN16
  if (totalMemSize > 64000)
  {
    free(strArray);
    NSStrSeqDelete(strSeq);
    free(mailPtr);
    return NULL;
  }
#endif

  MAPISendMailType *newMailPtr = (MAPISendMailType *)malloc((size_t)totalMemSize);
  if (!newMailPtr)
  {
    free(strArray);
    NSStrSeqDelete(strSeq);
    free(mailPtr);
    return NULL;
  }

  memset(newMailPtr, 0, (size_t) totalMemSize);
  //
  // Finally do the copy...
  //
  memcpy(newMailPtr, mailPtr, sizeof(MAPISendMailType));
  memcpy(newMailPtr->dataBuf, strSeq, (size_t) totalArraySize);
  *totalSize = totalMemSize;

  //
  // Cleanup and scram...
  //
  if (strArray)
    free(strArray);

  if (strSeq)
    NSStrSeqDelete(strSeq);

  if (mailPtr)
    free(mailPtr);

  return(newMailPtr);
}
