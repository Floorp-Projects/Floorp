/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*
   wallet.cpp
*/

#include "wallet.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIURL.h"

#include "xp_list.h"
#include "prefapi.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

#include "htmldlgs.h"
#include "prtypes.h"
#include "prlong.h"
#include "prinrval.h"
#include "allxpstr.h"

/***************************************************/
/* The following declarations define the data base */
/***************************************************/

enum PlacementType {DUP_IGNORE, DUP_OVERWRITE, DUP_BEFORE, DUP_AFTER, AT_END};

typedef struct _wallet_MapElement {
  nsAutoString * item1;
  nsAutoString * item2;
  XP_List * itemList;
} wallet_MapElement;

typedef struct _wallet_Sublist {
  nsAutoString * item;
} wallet_Sublist;

PRIVATE XP_List * wallet_URLFieldToSchema_list=0;
PRIVATE XP_List * wallet_specificURLFieldToSchema_list=0;
PRIVATE XP_List * wallet_FieldToSchema_list=0;
PRIVATE XP_List * wallet_SchemaToValue_list=0;
PRIVATE XP_List * wallet_SchemaConcat_list=0;

typedef struct _wallet_PrefillElement {
  nsIDOMHTMLInputElement* inputElement;
  nsIDOMHTMLSelectElement* selectElement;
  nsAutoString * schema;
  nsAutoString * value;
  PRInt32 selectIndex;
  PRUint32 count;
  XP_List * resume;
} wallet_PrefillElement;

/***********************************************************/
/* The following routines are for diagnostic purposes only */
/***********************************************************/

#ifdef DEBUG

void
wallet_Pause(){
  fprintf(stdout,"%cpress y to continue\n", '\007');
  char c;
  for (;;) {
    c = getchar();
    if (tolower(c) == 'y') {
      fprintf(stdout,"OK\n");
      break;
    }
  }
  while (c != '\n') {
    c = getchar();
  }
}

void
wallet_DumpAutoString(nsAutoString as){
  char s[100];
  as.ToCString(s, 100);
  fprintf(stdout, "%s\n", s);
}

void
wallet_Dump(XP_List * list) {
  XP_List * list_ptr;
  wallet_MapElement * ptr;

  char item1[100];
  char item2[100];
  char item[100];
  list_ptr = list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    ptr->item1->ToCString(item1, 100);
    ptr->item2->ToCString(item2, 100);
    fprintf(stdout, "%s %s \n", item1, item2);
    XP_List * list_ptr1;
    wallet_Sublist * ptr1;
    list_ptr1 = ptr->itemList;
    while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
      ptr1->item->ToCString(item, 100);
      fprintf(stdout, "     %s \n", item);
    }
  }
  wallet_Pause();
}

/******************************************************************/
/* The following diagnostic routines are for timing purposes only */
/******************************************************************/

const PRInt32 timing_max = 1000;
PRInt64 timings [timing_max];
char timingID [timing_max];
PRInt32 timing_index = 0;

PRInt64 stopwatch = LL_Zero();
PRInt64 stopwatchBase;
PRBool stopwatchRunning = FALSE;

void
wallet_ClearTiming() {
  timing_index  = 0;
}

void
wallet_DumpTiming() {
  PRInt32 i;
  for (i=1; i<timing_index; i++) {
#ifndef	XP_MAC
    fprintf(stdout, "time %c = %ld\n", timingID[i], (timings[i] - timings[i-1])/100);
#endif
    if (i%20 == 0) {
      wallet_Pause();
    }
  }
  wallet_Pause();
}

void
wallet_AddTiming(char c) {
  if (timing_index<timing_max) {
    timingID[timing_index] = c;
#ifndef	XP_MAC
    timings[timing_index++] = PR_IntervalNow();	// note: PR_IntervalNow returns a 32 bit value!
#endif
  }
}

void
wallet_ClearStopwatch() {
  stopwatch = LL_Zero();
  stopwatchRunning = FALSE;
}

void
wallet_ResumeStopwatch() {
  if (!stopwatchRunning) {
#ifndef	XP_MAC
    stopwatchBase = PR_IntervalNow();	// note: PR_IntervalNow returns a 32 bit value!
#endif
    stopwatchRunning = TRUE;
  }
}

void
wallet_PauseStopwatch() {
  if (stopwatchRunning) {
#ifndef	XP_MAC
    stopwatch += (PR_IntervalNow() - stopwatchBase);	// note: PR_IntervalNow returns a 32 bit value!
#endif
    stopwatchRunning = FALSE;
  }
}

void
wallet_DumpStopwatch() {
  if (stopwatchRunning) {
#ifndef	XP_MAC
    stopwatch += (PR_IntervalNow() - stopwatchBase);	// note: PR_IntervalNow returns a 32 bit value!
    stopwatchBase = PR_IntervalNow();
#endif
  }
#ifndef	XP_MAC
  fprintf(stdout, "stopwatch = %ld\n", stopwatch/100);  
#endif
}
#endif /* DEBUG */

/********************************************************/
/* The following data and procedures are for preference */
/********************************************************/

static const char *pref_captureForms =
    "wallet.captureForms";
PRIVATE Bool wallet_captureForms = FALSE;

PRIVATE void
wallet_SetFormsCapturingPref(Bool x)
{
    /* do nothing if new value of pref is same as current value */
    if (x == wallet_captureForms) {
        return;
    }

    /* change the pref */
    wallet_captureForms = x;
}

MODULE_PRIVATE int PR_CALLBACK
wallet_FormsCapturingPrefChanged(const char * newpref, void * data)
{
    PRBool x;
    PREF_GetBoolPref(pref_captureForms, &x);
    wallet_SetFormsCapturingPref(x);
    return PREF_NOERROR;
}

void
wallet_RegisterCapturePrefCallbacks(void)
{
    PRBool x = PR_TRUE; /* initialize to default value in case PREF_GetBoolPref fails */
    static Bool first_time = TRUE;

    if(first_time)
    {
        first_time = FALSE;
        PREF_GetBoolPref(pref_captureForms, &x);
        wallet_SetFormsCapturingPref(x);
        PREF_RegisterCallback(pref_captureForms, wallet_FormsCapturingPrefChanged, NULL);
    }
}

PRIVATE Bool
wallet_GetFormsCapturingPref(void)
{
    wallet_RegisterCapturePrefCallbacks();
    return wallet_captureForms;
}

static const char *pref_useDialogs =
    "wallet.useDialogs";
PRIVATE Bool wallet_useDialogs = FALSE;

PRIVATE void
wallet_SetUsingDialogsPref(Bool x)
{
    /* do nothing if new value of pref is same as current value */
    if (x == wallet_useDialogs) {
        return;
    }

    /* change the pref */
    wallet_useDialogs = x;
}

MODULE_PRIVATE int PR_CALLBACK
wallet_UsingDialogsPrefChanged(const char * newpref, void * data)
{
    PRBool x;
    PREF_GetBoolPref(pref_useDialogs, &x);
    wallet_SetUsingDialogsPref(x);
    return PREF_NOERROR;
}

void
wallet_RegisterUsingDialogsPrefCallbacks(void)
{
    PRBool x = PR_FALSE; /* initialize to default value in case PREF_GetBoolPref fails */
    static Bool first_time = TRUE;

    if(first_time)
    {
        first_time = FALSE;
        PREF_GetBoolPref(pref_useDialogs, &x);
        wallet_SetUsingDialogsPref(x);
        PREF_RegisterCallback(pref_useDialogs, wallet_UsingDialogsPrefChanged, NULL);
    }
}

PRIVATE Bool
wallet_GetUsingDialogsPref(void)
{
    wallet_RegisterUsingDialogsPrefCallbacks();
    return wallet_useDialogs;
}

/*********************************************/
/* Temporary until we have a real dialog box */
/*********************************************/

PRBool FE_Confirm(char * szMessage) {
  if (!wallet_GetUsingDialogsPref()) {
    return JS_TRUE;
  }
  fprintf(stdout, "%c%s  (y/n)?  ", '\007', szMessage); /* \007 is BELL */
  PRBool result;
  char c;
  for (;;) {
    c = getchar();
    if (tolower(c) == 'y') {
      result = PR_TRUE;
      break;
    }
    if (tolower(c) == 'n') {
      result = PR_FALSE;
      break;
    }
  }
  while (c != '\n') {
    c = getchar();
  }
  return result;
}

/**********************************************************************************/
/* The following routines are for locking the data base.  They are not being used */
/**********************************************************************************/

#ifdef junk
//#include "prpriv.h" /* for NewNamedMonitor */

static PRMonitor * wallet_lock_monitor = NULL;
static PRThread  * wallet_lock_owner = NULL;
static int wallet_lock_count = 0;

PRIVATE void
wallet_lock(void) {
  if(!wallet_lock_monitor) {
//        wallet_lock_monitor =
//            PR_NewNamedMonitor("wallet-lock");
  }

  PR_EnterMonitor(wallet_lock_monitor);

  while(TRUE) {

    /* no current owner or owned by this thread */
    PRThread * t = PR_CurrentThread();
    if(wallet_lock_owner == NULL || wallet_lock_owner == t) {
      wallet_lock_owner = t;
      wallet_lock_count++;

      PR_ExitMonitor(wallet_lock_monitor);
      return;
    }

    /* owned by someone else -- wait till we can get it */
    PR_Wait(wallet_lock_monitor, PR_INTERVAL_NO_TIMEOUT);
  }
}

PRIVATE void
wallet_unlock(void) {
  PR_EnterMonitor(wallet_lock_monitor);

#ifdef DEBUG
  /* make sure someone doesn't try to free a lock they don't own */
  PR_ASSERT(wallet_lock_owner == PR_CurrentThread());
#endif

  wallet_lock_count--;

  if(wallet_lock_count == 0) {
    wallet_lock_owner = NULL;
    PR_Notify(wallet_lock_monitor);
  }
  PR_ExitMonitor(wallet_lock_monitor);
}

#endif

/**********************************************************/
/* The following routines are for accessing the data base */
/**********************************************************/

/*
 * clear out the designated list
 */
void
wallet_Clear(XP_List ** list) {
  XP_List * list_ptr;
  wallet_MapElement * ptr;

  list_ptr = *list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    delete ptr->item1;
    delete ptr->item2;

    XP_List * list_ptr1;
    wallet_Sublist * ptr1;
    list_ptr1 = ptr->itemList;
    while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
      delete ptr1->item;
    }
    delete ptr->itemList;
    XP_ListRemoveObject(*list, ptr);
    list_ptr = *list;
    delete ptr;
  }
  *list = 0;
}

/*
 * add an entry to the designated list
 */
void
wallet_WriteToList(
    nsAutoString& item1,
    nsAutoString& item2,
    XP_List* itemList,
    XP_List*& list,
    PlacementType placement = DUP_BEFORE) {

  XP_List * list_ptr;
  wallet_MapElement * ptr;
  PRBool added_to_list = FALSE;

  wallet_MapElement * mapElement;
  mapElement = XP_NEW(wallet_MapElement);

  mapElement->item1 = &item1;
  mapElement->item2 = &item2;
  mapElement->itemList = itemList;

  /* make sure the list exists */
  if(!list) {
      list = XP_ListNew();
      if(!list) {
          return;
      }
  }

  /*
   * Add new entry to the list in alphabetical order by item1.
   * If identical value of item1 exists, use "placement" parameter to 
   * determine what to do
   */
  list_ptr = list;
  item1.ToLowerCase();
  if (AT_END==placement) {
    XP_ListAddObjectToEnd (list, mapElement);
    return;
  }
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    if((ptr->item1->Compare(item1))==0) {
      if (DUP_OVERWRITE==placement) {
        delete ptr->item1;
        delete ptr->item2;
        delete mapElement;
        ptr->item1 = &item1;
        ptr->item2 = &item2;
      } else if (DUP_BEFORE==placement) {
        XP_ListInsertObject(list, ptr, mapElement);
      }
      if (DUP_AFTER!=placement) {
        added_to_list = TRUE;
        break;
      }
    } else if((ptr->item1->Compare(item1))>=0) {
      XP_ListInsertObject(list, ptr, mapElement);
      added_to_list = TRUE;
      break;
    }
  }
  if (!added_to_list) {
    XP_ListAddObjectToEnd (list, mapElement);
  }
}

/*
 * fetch an entry from the designated list
 */
PRInt32
wallet_ReadFromList(
  nsAutoString item1,
  nsAutoString& item2,
  XP_List*& itemList,
  XP_List*& list_ptr)
{
  wallet_MapElement * ptr;

  wallet_MapElement * mapElement;
  mapElement = XP_NEW(wallet_MapElement);

  /* make sure the list exists */
  if(!list_ptr) {
    return -1;
  }

  /* find item1 in the list */
  item1.ToLowerCase();
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    if((ptr->item1->Compare(item1))==0) {
      item2 = nsAutoString(*ptr->item2);
      itemList = ptr->itemList;
      return 0;
    }
  }
  return -1;
}

/*
 * given a sublist element, advance to the next sublist element and get its value
 */
PRInt32
wallet_ReadFromSublist(nsAutoString& value, XP_List*& resume)
{
  wallet_Sublist * ptr;
  if((ptr=(wallet_Sublist *) XP_ListNextObject(resume))!=0) {
    value = *ptr->item;
    return 0;
  }
  return -1;
}

/************************************************************/
/* The following routines are for unlocking the stored data */
/************************************************************/

#define maxKeySize 100
char key[maxKeySize+1];
PRUint32 keyPosition = 0;
PRBool keyFailure = FALSE;

PUBLIC PRBool
wallet_BadKey() {
  return keyFailure;
}

PUBLIC void
wallet_SetKey() {
  keyFailure = FALSE;
  keyPosition = 0;

  if (!wallet_GetUsingDialogsPref()) {
    key[keyPosition++] = '~';
    return;
  }
  fprintf(stdout, "%cpassword=", '\007');
  char c;
  for (;;) {
    c = getchar();
    if (c == '\n') {
      key[keyPosition] = '\0';

      break;
    }
    if (keyPosition < maxKeySize) {
      key[keyPosition++] = c;
    }
  }
  keyPosition = 0;
}

PUBLIC char
wallet_GetKey() {
  if (keyPosition >= PL_strlen(key)) {
    keyPosition = 0;
  }
  return key[keyPosition++];
}

PUBLIC void
wallet_RestartKey() {
  keyPosition = 0;
}

PUBLIC void
wallet_WriteKey(nsOutputFileStream strm) {
  /* If we store the key obscured by the key itself, then the result will be zero
   * for all keys (since we are using XOR to obscure).  So instead we store
   * key[1..n],key[0] obscured by the actual key.
   */
  char* p = key+1;
  while (*p) {
    strm.put(*(p++)^wallet_GetKey());
  }
  strm.put((*key)^wallet_GetKey());  
}

void
wallet_ReadKey(nsInputFileStream strm) {
  char* p = key+1;
  while (*p) {
    if (strm.get() != (*(p++)^wallet_GetKey())) {
      keyFailure = TRUE;
      *key = '\0';
      return;
    }
  }
  if (strm.get() != ((*key)^wallet_GetKey())) {
    keyFailure = TRUE;
    *key = '\0';
  }
}

/******************************************************/
/* The following routines are for accessing the files */
/******************************************************/

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRInt32
wallet_GetLine(nsInputFileStream strm, nsAutoString*& aLine, PRBool obscure) {

  /* read the line */
  aLine = new nsAutoString("");   
  char c;
  for (;;) {
    if (strm.eof()) {
      return -1;
    }
    c = strm.get()^(obscure ? wallet_GetKey() : (char)0);
    if (c == '\n') {
      break;
    }
    if (c != '\r') {
      *aLine += c;
    }
  }

  return 0;
}

/*
 * Write a line to a file
 * return -1 if an error occurs
 */
PRInt32
wallet_PutLine(nsOutputFileStream strm, const nsString& aLine, PRBool obscure)
{
  /* allocate a buffer from the heap */
  char * cp = new char[aLine.Length() + 1];
  if (! cp) {
    return -1;
  }

  aLine.ToCString(cp, aLine.Length() + 1);

  /* output each character */
  char* p = cp;
  while (*p) {
    strm.put(*(p++)^(obscure ? wallet_GetKey() : (char)0));
  }
  strm.put('\n'^(obscure ? wallet_GetKey() : (char)0));

  delete[] cp;
  return 0;
}

/*
 * write contents of designated list into designated file
 */
void
wallet_WriteToFile(char* filename, XP_List* list, PRBool obscure) {
  XP_List * list_ptr;
  wallet_MapElement * ptr;

  if (obscure && wallet_BadKey()) {
    return;
  }

  /* open output stream */
  nsSpecialSystemDirectory walletFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
  walletFile += "res";
//  walletFile += "wallet";
  walletFile += filename;
  nsOutputFileStream strm(walletFile);
  if (!strm.is_open()) {
    NS_ERROR("unable to open file");
    return;
  }
  if (obscure) {
    wallet_RestartKey();
    wallet_WriteKey(strm);
  }

  /* make sure the list exists */
  if(!list) {
    return;
  }

  /* traverse the list */
  list_ptr = list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    wallet_PutLine(strm, *ptr->item1, obscure);
    if (*ptr->item2 != "") {
      wallet_PutLine(strm, *ptr->item2, obscure);
    } else {
      XP_List * list_ptr1;
      wallet_Sublist * ptr1;
      list_ptr1 = ptr->itemList;
      while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
        wallet_PutLine(strm, *ptr->item1, obscure);
      }
    }
    wallet_PutLine(strm, "", obscure);
 }

  /* close the stream */
  strm.flush();
  strm.close();
}

/*
 * Read contents of designated file into designated list
 */
void
wallet_ReadFromFile
    (char* filename, XP_List*& list, PRBool obscure, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsSpecialSystemDirectory walletFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
  walletFile += "res";
//  walletFile += "wallet";
  walletFile += filename;
  nsInputFileStream strm(walletFile);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }
  if (obscure) {
    wallet_RestartKey();
    wallet_ReadKey(strm);
    if (wallet_BadKey()) {
      FE_Confirm("Key failure -- value file will not be opened");
      strm.close();
      return;
    }
  }

  for (;;) {
    nsAutoString * aItem1;
    if (wallet_GetLine(strm, aItem1, obscure) == -1) {
      /* end of file reached */
      strm.close();
      return;
    }

    nsAutoString * aItem2;
    if (wallet_GetLine(strm, aItem2, obscure) == -1) {
      /* unexpected end of file reached */
      delete aItem1;
      strm.close();
      return;
    }

    nsAutoString * aItem3;
    if (wallet_GetLine(strm, aItem3, obscure) == -1) {
      /* end of file reached */
      XP_List* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, list, placement);
      strm.close();
      return;
    }

    if (aItem3->Length()==0) {
      /* just a pair of values, no need for a sublist */
      XP_List* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, list, placement);
    } else {
      /* need to create a sublist and put item2 and item3 onto it */
      XP_List * itemList = XP_ListNew();
      wallet_Sublist * sublist;
      sublist = XP_NEW(wallet_Sublist);
      sublist->item = new nsAutoString (*aItem2);
      XP_ListAddObjectToEnd (itemList, sublist);
      delete aItem2;
      sublist = XP_NEW(wallet_Sublist);
      sublist->item = new nsAutoString (*aItem3);
      XP_ListAddObjectToEnd (itemList, sublist);
      delete aItem3;
      /* add any following items to sublist up to next blank line */
      nsAutoString * dummy2 = new nsAutoString("");
      for (;;) {
        /* get next item for sublist */
        if (wallet_GetLine(strm, aItem3, obscure) == -1) {
          /* end of file reached */
          wallet_WriteToList(*aItem1, *dummy2, itemList, list, placement);
          strm.close();
          return;
        }
        if (aItem3->Length()==0) {
          /* blank line reached indicating end of sublist */
          wallet_WriteToList(*aItem1, *dummy2, itemList, list, placement);
          break;
        }
        /* add item to sublist */
        sublist = XP_NEW(wallet_Sublist);
        sublist->item = new nsAutoString (*aItem3);
        XP_ListAddObjectToEnd (itemList, sublist);
        delete aItem3;
      }
    }
  }
}

/*
 * Read contents of designated URLFieldToSchema file into designated list
 */
void
wallet_ReadFromURLFieldToSchemaFile
    (char* filename, XP_List*& list, PlacementType placement = DUP_AFTER) {

  /* open input stream */
  nsSpecialSystemDirectory walletFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
  walletFile += "res";
  walletFile += "wallet";
  walletFile += filename;
  nsInputFileStream strm(walletFile);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }
  /* wallet_RestartKey();  not needed since file is not encoded */

  /* make sure the list exists */
  if(!list) {
    list = XP_ListNew();
    if(!list) {
      strm.close();
      return;
    }
  }

  for (;;) {

    nsAutoString * aItem;
    if (wallet_GetLine(strm, aItem, FALSE) == -1) {
      /* end of file reached */
      strm.close();
      return;
    }

    XP_List * itemList = XP_ListNew();
    nsAutoString * dummy = new nsAutoString("");
    wallet_WriteToList(*aItem, *dummy, itemList, list, placement);

    for (;;) {
      nsAutoString * aItem1;
      if (wallet_GetLine(strm, aItem1, FALSE) == -1) {
        /* end of file reached */
        strm.close();
        return;
      }

      if (aItem1->Length()==0) {
        /* end of url reached */
        break;
      }

      nsAutoString * aItem2;
      if (wallet_GetLine(strm, aItem2, FALSE) == -1) {
        /* unexpected end of file reached */
        delete aItem1;
        strm.close();
        return;
      }

      XP_List* dummy = NULL;
      wallet_WriteToList(*aItem1, *aItem2, dummy, itemList, placement);

      nsAutoString * aItem3;
      if (wallet_GetLine(strm, aItem3, FALSE) == -1) {
        /* end of file reached */
        strm.close();
        return;
      }

      if (aItem3->Length()!=0) {
        /* invalid file format */
        strm.close();
        delete aItem3;
        return;
      }
      delete aItem3;
    }
  }
}

/***************************************************************/
/* The following routines are for fetching data from NetCenter */
/***************************************************************/

void
wallet_FetchFromNetCenter(char* from, char* to) {
  nsINetService *inet = nsnull;
  nsIURL * url;
  if (!NS_FAILED(NS_NewURL(&url, from))) {
    nsresult rv = nsServiceManager::GetService(kNetServiceCID,
                                               kINetServiceIID,
                                               (nsISupports **)&inet);
    if (NS_SUCCEEDED(rv)) {

      /* open network stream */
      nsIInputStream* newStream;
      nsIInputStream* *aNewStream = &newStream;
      rv = inet->OpenBlockingStream(url, nsnull, aNewStream);
      if (NS_SUCCEEDED(rv)) {

        /* open output file */
        nsSpecialSystemDirectory walletFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
        walletFile += "res";
//        walletFile += "wallet";
        walletFile += to;
        nsOutputFileStream strm(walletFile);
        if (!strm.is_open()) {
          NS_ERROR("unable to open file");
        } else {

          /* place contents of network stream in output file */
          char buff[1001];
          PRUint32 count;
          while (NS_OK == (*aNewStream)->Read(buff,1000,&count)) {
            buff[count] = '\0';
            strm.write(buff, count);
          }
          strm.flush();
          strm.close();
        }
      }
    }
    nsServiceManager::ReleaseService(kNetServiceCID, inet);
  }
}

/*
 * fetch URL-specific field/schema mapping from netcenter and put into local copy of file
 * at URLFieldSchema.tbl
 */
void
wallet_FetchURLFieldSchemaFromNetCenter() {
  wallet_FetchFromNetCenter
    ("http://people.netscape.com/morse/wallet/URLFieldSchema.tbl","URLFieldSchema.tbl");
}

/*
 * fetch generic field/schema mapping from netcenter and put into
 * local copy of file at FieldSchema.tbl
 */
void
wallet_FetchFieldSchemaFromNetCenter() {
  wallet_FetchFromNetCenter
    ("http://people.netscape.com/morse/wallet/FieldSchema.tbl","FieldSchema.tbl");
}

/*
 * fetch generic schema-concatenation rules from netcenter and put into
 * local copy of file at SchemaConcat.tbl
 */
void
wallet_FetchSchemaConcatFromNetCenter() {
  wallet_FetchFromNetCenter
    ("http://people.netscape.com/morse/wallet/SchemaConcat.tbl","SchemaConcat.tbl");
}

/*********************************************************************/
/* The following are utility routines for the main wallet processing */
/*********************************************************************/
 
/*
 * given a field name, get the value
 */
PRInt32 FieldToValue(
    nsAutoString field,
    nsAutoString& schema,
    nsAutoString& value,
    XP_List*& itemList,
    XP_List*& resume)
{
  /* return if no SchemaToValue list exists */
  if (!wallet_SchemaToValue_list) {
    return -1;
  }

  /* fetch schema name from field/schema tables */
  XP_List* FieldToSchema_list = wallet_FieldToSchema_list;
  XP_List* URLFieldToSchema_list = wallet_specificURLFieldToSchema_list;
  XP_List* SchemaToValue_list;
  XP_List* dummy;
  if (nsnull == resume) {
    resume = wallet_SchemaToValue_list;
  }
  if ((wallet_ReadFromList(field, schema, dummy, URLFieldToSchema_list) != -1) ||
       (wallet_ReadFromList(field, schema, dummy, FieldToSchema_list) != -1)) {
    /* schema name found, now fetch value from schema/value table */ 
    SchemaToValue_list = resume;
    if (wallet_ReadFromList(schema, value, itemList, SchemaToValue_list) != -1) {
      /* value found, prefill it into form */
      resume = SchemaToValue_list;
      return 0;
    } else {
      /* value not found, see if concatenation rule exists */
      XP_List * itemList2;

      XP_List * SchemaConcat_list = wallet_SchemaConcat_list;
      nsAutoString dummy2;
      if (wallet_ReadFromList(schema, dummy2, itemList2, SchemaConcat_list) != -1) {
        /* concatenation rules exist, generate value as a concatenation */
        XP_List * list_ptr1;
        wallet_Sublist * ptr1;
        list_ptr1 = itemList2;
        value = nsAutoString("");
        nsAutoString value2;
        while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
          SchemaToValue_list = wallet_SchemaToValue_list;
          if (wallet_ReadFromList(*(ptr1->item), value2, dummy, SchemaToValue_list) != -1) {
            if (value.Length()>0) {
              value += " ";
            }
            value += value2;
          }
        }
        resume = nsnull;
        itemList = nsnull;
        return 0;
      }
    }
  } else {
    /* schema name not found, use field name as schema name and fetch value */
    SchemaToValue_list = resume;
    if (wallet_ReadFromList(field, value, itemList, SchemaToValue_list) != -1) {
      /* value found, prefill it into form */
      resume = SchemaToValue_list;
      return 0;
    }
  }
  resume = nsnull;
  return -1;
}

PRInt32
wallet_GetSelectIndex(
  nsIDOMHTMLSelectElement* selectElement,
  nsAutoString value,
  PRInt32& index)
{
  nsresult result;
  PRUint32 length;
  selectElement->GetLength(&length);
  nsIDOMHTMLCollection * options;
  result = selectElement->GetOptions(&options);
  if ((NS_SUCCEEDED(result)) && (nsnull != options)) {
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    for (PRUint32 optionX = 0; optionX < numOptions; optionX++) {
      nsIDOMNode* optionNode = nsnull;
      options->Item(optionX, &optionNode);
      if (nsnull != optionNode) {
        nsIDOMHTMLOptionElement* optionElement = nsnull;
        result = optionNode->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&optionElement);
        if ((NS_SUCCEEDED(result)) && (nsnull != optionElement)) {
          nsAutoString optionValue;
          nsAutoString optionText;
          optionElement->GetValue(optionValue);
          optionElement->GetText(optionText);
          if (value==optionValue || value==optionText) {
            index = optionX;
            return 0;
          }
          NS_RELEASE(optionElement);
        }
        NS_RELEASE(optionNode);
      }
    }
    NS_RELEASE(options);
  }
  return -1;
}

PRInt32
wallet_GetPrefills(
  nsIDOMNode* elementNode,
  nsIDOMHTMLInputElement*& inputElement,  
  nsIDOMHTMLSelectElement*& selectElement,
  nsAutoString*& schemaPtr,
  nsAutoString*& valuePtr,
  PRInt32& selectIndex,
  XP_List*& resume)
{
  nsresult result;

  /* get prefills for input element */
  result = elementNode->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement);
  if ((NS_SUCCEEDED(result)) && (nsnull != inputElement)) {
    nsAutoString type;
    result = inputElement->GetType(type);
    if ((NS_SUCCEEDED(result)) && ((type =="") || (type.Compare("text", PR_TRUE) == 0))) {
      nsAutoString field;
      result = inputElement->GetName(field);
      if (NS_SUCCEEDED(result)) {
        nsAutoString schema;
        nsAutoString value;
        XP_List* itemList;
        if (FieldToValue(field, schema, value, itemList, resume) == 0) {
          if (value == "" && nsnull != itemList) {
            /* pick first of a set of synonymous values */
            wallet_ReadFromSublist(value, itemList);
          }
          valuePtr = new nsAutoString(value);
          schemaPtr = new nsAutoString(schema);
          selectElement = nsnull;
          selectIndex = -1;
          return 0;
        }
      }
    }
    NS_RELEASE(inputElement);
    return -1;
  }

  /* get prefills for dropdown list */
  result = elementNode->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&selectElement);
  if ((NS_SUCCEEDED(result)) && (nsnull != selectElement)) {
    nsAutoString field;
    result = selectElement->GetName(field);
    if (NS_SUCCEEDED(result)) {
      nsAutoString schema;
      nsAutoString value;
      XP_List* itemList;
      if (FieldToValue(field, schema, value, itemList, resume) == 0) {
        if (value != "") {
          /* no synonym list, just one value to try */
          result = wallet_GetSelectIndex(selectElement, value, selectIndex);
          if (NS_SUCCEEDED(result)) {
            /* value matched one of the values in the drop-down list */
            valuePtr = new nsAutoString(value);
            schemaPtr = new nsAutoString(schema);
            inputElement = nsnull;
            return 0;
          }
        } else {
          /* synonym list exists, try each value */
          while (wallet_ReadFromSublist(value, itemList) == 0) {
            result = wallet_GetSelectIndex(selectElement, value, selectIndex);
            if (NS_SUCCEEDED(result)) {
              /* value matched one of the values in the drop-down list */
              valuePtr = new nsAutoString(value);
              schemaPtr = new nsAutoString(schema);
              inputElement = nsnull;
              return 0;
            }
          }
        }
      }
    }
    NS_RELEASE(selectElement);
  }
  return -1;
}

/*
 * initialization for wallet session (done only once)
 */
void
wallet_Initialize() {
  static PRBool wallet_initialized = FALSE;
  if (!wallet_initialized) {
    wallet_FetchFieldSchemaFromNetCenter();
    wallet_FetchURLFieldSchemaFromNetCenter();
    wallet_FetchSchemaConcatFromNetCenter();

    wallet_ReadFromFile("FieldSchema.tbl", wallet_FieldToSchema_list, FALSE);
    wallet_ReadFromURLFieldToSchemaFile("URLFieldSchema.tbl", wallet_URLFieldToSchema_list);
    wallet_ReadFromFile("SchemaConcat.tbl", wallet_SchemaConcat_list, FALSE);

    wallet_SetKey();
    wallet_ReadFromFile("SchemaValue.tbl", wallet_SchemaToValue_list, TRUE);

#if DEBUG
//    fprintf(stdout,"Field to Schema table \n");
//    wallet_Dump(wallet_FieldToSchema_list);

//    fprintf(stdout,"Schema to Value table \n");
//    wallet_Dump(wallet_SchemaToValue_list);

//    fprintf(stdout,"SchemaConcat table \n");
//    wallet_Dump(wallet_SchemaConcat_list);

//    fprintf(stdout,"URL Field to Schema table \n");
//    XP_List * list_ptr;
//    char item1[100];
//    wallet_MapElement * ptr;
//    list_ptr = wallet_URLFieldToSchema_list;
//    while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
//      ptr->item1->ToCString(item1, 100);
//      fprintf(stdout, item1);
//      fprintf(stdout,"\n");
//      wallet_Dump(ptr->itemList);
//    }
#endif
    wallet_initialized = TRUE;
  } else if (wallet_BadKey()) {
    wallet_SetKey();
    wallet_ReadFromFile("SchemaValue.tbl", wallet_SchemaToValue_list, TRUE);
  }
}

/*
 * initialization for current URL
 */
void
wallet_InitializeCurrentURL(nsIDocument * doc) {
  static nsIURL * lastUrl = NULL;

  /* get url */
  nsIURL* url;
  url = doc->GetDocumentURL();
  if (lastUrl == url) {
    NS_RELEASE(url);
    return;
  } else {
    if (lastUrl) {
//??      NS_RELEASE(lastUrl);
    }
    lastUrl = url;
  }

  /* get host+file */
  const char* host;
  url->GetHost(&host);
  nsAutoString urlName = nsAutoString(host);
  const char* file;
  url->GetFile(&file);
  urlName = urlName + file;
  NS_RELEASE(url);

  /* get field/schema mapping specific to current url */
  XP_List * list_ptr;
  wallet_MapElement * ptr;
  list_ptr = wallet_URLFieldToSchema_list;
  while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
    if (*(ptr->item1) == urlName) {
      wallet_specificURLFieldToSchema_list = ptr->itemList;
      break;
    }
  }
#ifdef DEBUG
//  fprintf(stdout,"specific URL Field to Schema table \n");
//  wallet_Dump(wallet_specificURLFieldToSchema_list);
#endif
}

#define SEPARATOR "#*%&"

nsAutoString *
wallet_GetNextInString(char*& ptr) {
  nsAutoString * result;
  char * endptr;
  endptr = PL_strstr(ptr, SEPARATOR);
  if (!endptr) {
    return NULL;
  }
  *endptr = '\0';
  result = new nsAutoString(ptr);
  *endptr = SEPARATOR[0];
  ptr = endptr + PL_strlen(SEPARATOR);
  return result;
}

void
wallet_ReleasePrefillElementList(XP_List * wallet_PrefillElement_list) {
  if (wallet_PrefillElement_list) {
    wallet_PrefillElement * ptr;
    XP_List * list_ptr = wallet_PrefillElement_list;
    while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
      if (ptr->inputElement) {
        NS_RELEASE(ptr->inputElement);
      } else {
        NS_RELEASE(ptr->selectElement);
      }
      delete ptr->schema;
      delete ptr->value;
      XP_ListRemoveObject(wallet_PrefillElement_list, ptr);
      list_ptr = wallet_PrefillElement_list;
      delete ptr;
    }
  }
}

PR_STATIC_CALLBACK(PRBool)
wallet_RequestToPrefillDone(XPDialogState* state, char** argv, int argc,
	unsigned int button) {
  if (button == XP_DIALOG_OK_BUTTON) {
    char* listAsAscii;
    char* fillins;
    nsAutoString * next;

    /* get values that are in environment variables */
    fillins = XP_FindValueInArgs("fillins", argv, argc);
    listAsAscii = XP_FindValueInArgs("list", argv, argc);
    XP_List * list;
    sscanf(listAsAscii, "%ld", &list);

    /* process the list, doing the fillins */

    /*
     * note: there are two lists involved here and we are stepping through both of them.

     * One is the pre-fill list that was generated when we walked through the html content.
     * For each pre-fillable item, it contains n entries, one for each possible value that
     * can be prefilled for that field.  The first entry for each field can be identified
     * because it has a non-zero count field (in fact, the count is the number of entries
     * for that field), all subsequent entries for the same field have a zero count field.

     * The other is the fillin list which was generated by the html dialog that just
     * finished.  It contains one entry for each pre-fillable item specificying that
     * particular value that should be prefilled for that item.
     */

    XP_List * list_ptr = list;
    wallet_PrefillElement * ptr;
    char * ptr2;
    ptr2 = fillins;
    /* step through pre-fill list */
    PRBool first = TRUE;
    while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
      /* advance in fillins list each time a new schema name in pre-fill list is encountered */
      if (ptr->count != 0) {
        /* count != 0 indicates a new schema name */
        if (!first) {
          delete next;
          first = FALSE;
        }
        next = wallet_GetNextInString(ptr2);
        if (nsnull == next) {
          return PR_FALSE;
        }
        if (*next != *ptr->schema) {
          delete next;
          return PR_FALSE; /* something's wrong so stop prefilling */
        }
        delete next;
        next = wallet_GetNextInString(ptr2);
      }

      if (*next == *ptr->value) {
        /*
         * Remove entry from wallet_SchemaToValue_list and then reinsert.  This will
         * keep multiple values in that list for the same field ordered with
         * most-recently-used first.  That's useful since the first such entry
         * is the default value used for pre-filling.
         */

        /*
         * Test for ptr->count being zero is an optimization that avoids us from doing a
         * reordering if the current entry already was first
         */
        if (ptr->resume && (ptr->count == 0)) {
          wallet_MapElement * mapElement = (wallet_MapElement *) (ptr->resume->object);
          XP_ListRemoveObject(wallet_SchemaToValue_list, mapElement);
          wallet_WriteToList(
            *(mapElement->item1),
            *(mapElement->item2),
            mapElement->itemList, 
            wallet_SchemaToValue_list);
        }
      }

      /* Change the value */

      if ((*next == *ptr->value) || ((ptr->count>0) && (*next == ""))) {
        if (((*next == *ptr->value) || (*next == "")) && ptr->inputElement) {
          ptr->inputElement->SetValue(*next);
        } else {
          nsresult result;
          result = wallet_GetSelectIndex(ptr->selectElement, *next, ptr->selectIndex);
          if (NS_SUCCEEDED(result)) {
            ptr->selectElement->SetSelectedIndex(ptr->selectIndex);
          } else {
            ptr->selectElement->SetSelectedIndex(0);
          }
        }
      }
    }
    delete next;

    /* Release the prefill list that was generated when we walked thru the html content */
    wallet_ReleasePrefillElementList(list);
  }

  return PR_FALSE;
}

static XPDialogInfo dialogInfo = {
  0,
  wallet_RequestToPrefillDone,
  1000,
  1200
};

void
wallet_RequestToPrefill(XP_List * list) {
  char *buffer = (char*)PR_Malloc(BUFLEN);
  char *buffer2 = 0;
  PRInt32 g = 0;

  XPDialogStrings* strings;
  strings = XP_GetDialogStrings(XP_CERT_PAGE_STRINGS); /* why doesn't this link? */
  if (!strings) {
    return;
  }

  LocalStrAllocCopy(buffer2, "");

  /* generate initial section of html file */
  g += PR_snprintf(buffer+g, BUFLEN-g,
"<HTML>\n"
"<HEAD>\n"
"  <TITLE>Pre-Filling</TITLE>\n"
"  <SCRIPT>\n"
"    index_frame = 0;\n"
"    title_frame = 1;\n"
"    list_frame = 2;\n"
"    button_frame = 3;\n"
"\n"
    );
  FLUSH_BUFFER

  /* start generating list of fillins */
//  StrAllocCopy (heading, XP_GetString(???); !!!HOW DO WE DO I18N IN RAPTOR???
  g += PR_snprintf(buffer+g, BUFLEN-g,
"    function loadFillins(){\n"
"      top.frames[title_frame].document.open();\n"
"      top.frames[title_frame].document.write\n"
"        (\"&nbsp;%s\");\n"
"      top.frames[title_frame].document.close();\n"
"\n"
"      top.frames[list_frame].document.open();\n"
"      top.frames[list_frame].document.write(\n"
"        \"<FORM name=fSelectFillin>\" +\n"
"          \"<BR>\" +\n"
"          \"<TABLE BORDER=0>\" +\n"
"            \"<TR>\" +\n"
"              \"<TD>\" +\n"
"                \"<BR>\" +\n",
    "Following items can be pre-filled for you."); //!!!NEED I18N!!!
  FLUSH_BUFFER
//  PR_FREEIF(heading); !!! NEED TO I18N THIS !!!

  /* generate the html for the list of fillins */
  wallet_PrefillElement * ptr;
  XP_List * list_ptr = list;
  PRUint32 count;
  while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
    char * schema;
    schema = ptr->schema->ToNewCString();
    char * value;
    value = ptr->value->ToNewCString();
    if (ptr->count) {
      count = ptr->count;
      g += PR_snprintf(buffer+g, BUFLEN-g,
"                \"<TR>\" +\n"
"                  \"<TD>%s:  </TD>\" +\n"
"                  \"<TD>\" +\n"
"                    \"<SELECT>\" +\n"
"                      \"<OPTION VALUE=\\\"%s\\\">%s</OPTION>\" +\n",
        schema, schema, value);
    } else {
      g += PR_snprintf(buffer+g, BUFLEN-g,
"                      \"<OPTION VALUE=\\\"%s\\\">%s</OPTION>\" +\n",
        schema, value);
    }
    count--;
    if (count == 0) {
    g += PR_snprintf(buffer+g, BUFLEN-g,
"                      \"<OPTION VALUE=\\\"%s\\\"></OPTION>\" +\n"
"                    \"</SELECT><BR>\" +\n"
"                  \"</TD>\" +\n"
"                \"</TR>\" +\n",
      schema);
    }
    FLUSH_BUFFER
    delete []schema;
    delete []value;
  }    

  /* finish generating list of fillins */
  g += PR_snprintf(buffer+g, BUFLEN-g,
"              \"</TD>\" +\n"
"            \"</TR>\" +\n"
"          \"</TABLE>\" +\n"
"        \"</FORM>\"\n"
"      );\n"
"      top.frames[list_frame].document.close();\n"
"    };\n"
"\n"
    );
  FLUSH_BUFFER
//  PR_FREEIF(heading); !!! NEED TO I18N THIS !!!

/* generate rest of html */
  g += PR_snprintf(buffer+g, BUFLEN-g,
"    function loadButtons(){\n"
"      top.frames[button_frame].document.open();\n"
"      top.frames[button_frame].document.write(\n"
"        \"<FORM name=buttons action=internal-walletPrefill-handler method=post>\" +\n"
"          \"<BR>\" +\n"
"          \"<DIV align=center>\" +\n"
"            \"<INPUT type=BUTTON value=OK width=80 onclick=parent.clicker(this,window.parent)>\" +\n"
"            \" &nbsp;&nbsp;\" +\n"
"            \"<INPUT type=BUTTON value=Cancel width=80 onclick=parent.clicker(this,window.parent)>\" +\n"
"          \"</DIV>\" +\n"
"          \"<INPUT type=HIDDEN name=xxxbuttonxxx>\" +\n"
"          \"<INPUT type=HIDDEN name=handle value="
    );
  FLUSH_BUFFER

  /* generate remainder of html, it will go into strings->arg[2] */
  g += PR_snprintf(buffer+g, BUFLEN-g,
">\" +\n"
"          \"<INPUT TYPE=HIDDEN NAME=fillins VALUE=\\\" \\\" SIZE=-1>\" +\n"
"          \"<INPUT TYPE=HIDDEN NAME=list VALUE=\\\" \\\" SIZE=-1>\" +\n"
"        \"</FORM>\"\n"
"      );\n"
"      top.frames[button_frame].document.close();\n"
"    }\n"
"\n"
"    function loadFrames(){\n"
"      loadFillins();\n"
"      loadButtons();\n"
"    }\n"
"\n"
"    function clicker(but,win){\n"
"      selname = top.frames[list_frame].document.fSelectFillin;\n"
"      var list = top.frames[button_frame].document.buttons.list;\n"
"      list.value = %ld;\n"
"      var fillins = top.frames[button_frame].document.buttons.fillins;\n"
"      fillins.value = \"\";\n"
"      for (i=0; i<selname.length; i++) {\n"
"        fillins.value = fillins.value +\n"
"          selname.elements[i].options[selname.elements[i].selectedIndex].value + \"%s\" +\n"
"          selname.elements[i].options[selname.elements[i].selectedIndex].text + \"%s\";\n"
"      }\n"
#ifndef HTMLDialogs
"      var expires = new Date();\n"
"      expires.setTime(expires.getTime() + 1000*60*60*24*365);\n"
"      document.cookie = \"htmldlgs=|\" + but.value +\n"
"        '|list|' + list.value + '|fillins|' + fillins.value + '|' +\n"
"        \"; expires=\" + expires.toGMTString();\n"
#endif
"      top.frames[button_frame].document.buttons.xxxbuttonxxx.value = but.value;\n"
"      top.frames[button_frame].document.buttons.xxxbuttonxxx.name = 'button';\n"
"      top.frames[button_frame].document.buttons.submit();\n"
#ifndef HTMLDialogs 
//"      top.frames[list_frame].document.open();\n"
//"      top.frames[list_frame].document.close();\n"
//"      top.frames[button_frame].document.open();\n"
//"      top.frames[button_frame].document.close();\n"
#endif
"    }\n"
"\n"
"  </SCRIPT>\n"
"</HEAD>\n"
"<FRAMESET ROWS = 25,25,*,75\n"
"         BORDER=0\n"
"         FRAMESPACING=0\n"
"         onLoad=loadFrames()>\n"
"  <FRAME SRC=about:blank\n"
"        NAME=index_frame\n"
"        SCROLLING=NO\n"
"        MARGINWIDTH=1\n"
"        MARGINHEIGHT=1\n"
"        NORESIZE>\n"
"  <FRAME SRC=about:blank\n"
"        NAME=title_frame\n"
"        SCROLLING=NO\n"
"        MARGINWIDTH=1\n"
"        MARGINHEIGHT=1\n"
"        NORESIZE>\n"
"    <FRAME SRC=about:blank\n"
"          NAME=list_frame\n"
"          SCROLLING=AUTO\n"
"          MARGINWIDTH=0\n"
"          MARGINHEIGHT=0\n"
"          NORESIZE>\n"
"  <FRAME SRC=about:blank\n"
"        NAME=button_frame\n"
"        SCROLLING=NO\n"
"        MARGINWIDTH=1\n"
"        MARGINHEIGHT=1\n"
"        NORESIZE>\n"
"</FRAMESET>\n"
"\n"
"<NOFRAMES>\n"
"  <BODY> <BR> </BODY>\n"
"</NOFRAMES>\n"
"</HTML>\n",
    list, SEPARATOR, SEPARATOR);
  FLUSH_BUFFER

  /* free buffer since it is no longer needed */
  PR_FREEIF(buffer);

  /* put html just generated into strings->arg[0] and invoke HTML dialog */
  if (buffer2) {
    XP_CopyDialogString(strings, 0, buffer2);
    PR_Free(buffer2);
    buffer2 = NULL;
  }
  XP_MakeHTMLDialog(NULL, &dialogInfo, 0, strings, NULL, PR_FALSE);
  return;
}

#define WALLET_EDITOR_URL "http://people.netscape.com/morse/wallet/walleted.html"
//#define WALLET_EDITOR_URL "http://peoplestage/morse/wallet/walleted.html"
//#define WALLET_EDITOR_URL "resource:/res/samples/walleted.html"
// bad!!! should pass the above URL as parameter to wallet_PostEdit
#define BREAK '\001'

void
wallet_PostEdit() {
  if (wallet_BadKey()) {
    return;
  }

  nsAutoString * nsCookie = new nsAutoString("");
  nsIURL* url;
  char* separator;

  nsINetService *netservice;
  nsresult res;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_SUCCEEDED(res)) && (nsnull != netservice)) {
    const nsAutoString walletEditor = nsAutoString(WALLET_EDITOR_URL);
    if (!NS_FAILED(NS_NewURL(&url, walletEditor))) {
      res = netservice->GetCookieString(url, *nsCookie);
    }
    NS_RELEASE(netservice);

    /* convert cookie to a C string */
    char *cookies = nsCookie->ToNewCString();
    char *cookie = PL_strstr(cookies, "SchemaToValue="); /* get to SchemaToValue= */
    cookie = cookie + PL_strlen("SchemaToValue="); /* get passed htmldlgs=| */

    /* return if OK button was not pressed */
    separator = strchr(cookie, BREAK);
    *separator = '\0';
    if (strcmp(cookie, "OK")) {
      *separator = BREAK;
      delete []cookies;
      return;
    }
    cookie = separator+1;
    *separator = BREAK;

    /* open SchemaValue file */
    nsSpecialSystemDirectory walletFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    walletFile += "res";
    walletFile += "wallet";
    walletFile += "SchemaValue.tbl";
    nsOutputFileStream strm(walletFile);
    if (!strm.is_open()) {
      NS_ERROR("unable to open file");
      delete []cookies;
      return;
    }
    wallet_RestartKey();
    wallet_WriteKey(strm);

    /* write the values in the cookie to the file */
    for (int i=0; ((*cookie != '\0') && (*cookie != ';')); i++) {
      separator = strchr(cookie, BREAK);
      *separator = '\0';
      wallet_PutLine(strm, cookie,TRUE);
      cookie = separator+1;
      *separator = BREAK;
    }

    /* close the file and read it back into the SchemaToValue list */
    strm.close();
    wallet_Clear(&wallet_SchemaToValue_list);
    wallet_ReadFromFile("SchemaValue.tbl", wallet_SchemaToValue_list, TRUE);
    delete []cookies;
  }
}

/***************************************************************/
/* The following are the interface routines seen by other dlls */
/***************************************************************/

/*
 * edit the users data
 */
PUBLIC void
WLLT_PreEdit(nsIURL* url) {

  if (nsnull == url) {
    wallet_PostEdit();
    return;
  }

  nsINetService *netservice;
  nsresult res;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_SUCCEEDED(res)) && (nsnull != netservice)) {
    wallet_Initialize();
    nsAutoString * cookie = new nsAutoString("SchemaToValue=");
    *cookie += BREAK;
    XP_List * list_ptr;
    wallet_MapElement * ptr;
    list_ptr = wallet_SchemaToValue_list;

    while((ptr = (wallet_MapElement *) XP_ListNextObject(list_ptr))!=0) {
      *cookie += *(ptr->item1) + BREAK;
      if (*ptr->item2 != "") {
        *cookie += *(ptr->item2) + BREAK;
      } else {
        XP_List * list_ptr1;
        wallet_Sublist * ptr1;
        list_ptr1 = ptr->itemList;
        while((ptr1=(wallet_Sublist *) XP_ListNextObject(list_ptr1))!=0) {
          *cookie += *(ptr1->item) + BREAK;
        }
      }
      *cookie += BREAK;
    }
    res = netservice->SetCookieString(url, *cookie);
    delete cookie;
    NS_RELEASE(netservice);
  }
}

/*
 * get the form elements on the current page and prefill them if possible
 */
PUBLIC void
WLLT_Prefill(nsIPresShell* shell, PRBool quick) {

#ifndef HTMLDialogs 
  if (!shell) {
    XP_MakeHTMLDialog2(&dialogInfo);
    return;
  }
#endif

  /* create list of elements that can be prefilled */
  XP_List *wallet_PrefillElement_list=XP_ListNew();
  if (!wallet_PrefillElement_list) {
    return;
  }

  /* starting with the present shell, get each form element and put them on a list */
  if (nsnull != shell) {
    nsIDocument* doc = nsnull;
    if (shell->GetDocument(&doc) == NS_OK) {
      wallet_Initialize();
      wallet_InitializeCurrentURL(doc);
      nsIDOMHTMLDocument* htmldoc = nsnull;
      nsresult result = doc->QueryInterface(kIDOMHTMLDocumentIID, (void**)&htmldoc);
      if ((NS_SUCCEEDED(result)) && (nsnull != htmldoc)) {
        nsIDOMHTMLCollection* forms = nsnull;
        htmldoc->GetForms(&forms);
        if (nsnull != forms) {
          PRUint32 numForms;
          forms->GetLength(&numForms);
          for (PRUint32 formX = 0; formX < numForms; formX++) {
            nsIDOMNode* formNode = nsnull;
            forms->Item(formX, &formNode);
            if (nsnull != formNode) {
              nsIDOMHTMLFormElement* formElement = nsnull;
              result = formNode->QueryInterface(kIDOMHTMLFormElementIID, (void**)&formElement);
              if ((NS_SUCCEEDED(result)) && (nsnull != formElement)) {
                nsIDOMHTMLCollection* elements = nsnull;
                result = formElement->GetElements(&elements);
                if ((NS_SUCCEEDED(result)) && (nsnull != elements)) {
                  /* got to the form elements at long last */
                  PRUint32 numElements;
                  elements->GetLength(&numElements);
                  for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
                    nsIDOMNode* elementNode = nsnull;
                    elements->Item(elementX, &elementNode);
                    if (nsnull != elementNode) {
                      wallet_PrefillElement * prefillElement;
                      XP_List * resume = nsnull;
                      wallet_PrefillElement * firstElement = nsnull;
                      PRUint32 numberOfElements = 0;
                      for (;;) {
                        /* loop to allow for multiple values */
                        /* first element in multiple-value group will have its count
                         * field set to the number of elements in group.  All other
                         * elements in group will have count field set to 0
                         */
                        prefillElement = XP_NEW(wallet_PrefillElement);
                        if (wallet_GetPrefills
                            (elementNode,
                            prefillElement->inputElement,
                            prefillElement->selectElement,
                            prefillElement->schema,
                            prefillElement->value,
                            prefillElement->selectIndex,
                            resume) != -1) {
                          /* another value found */
                          prefillElement->resume = resume;
                          if (nsnull == firstElement) {
                            firstElement = prefillElement;
                          }
                          numberOfElements++;
                          prefillElement->count = 0;
                          XP_ListAddObjectToEnd(wallet_PrefillElement_list, prefillElement);
                          if (nsnull == resume) {
                            /* value was found from concat rules, can't resume from here */
                            break;
                          }
                        } else {
                          /* value not found, stop looking for more values */
                          delete prefillElement;
                          break;
                        }
                      }
                      if (numberOfElements>0) {
                        firstElement->count = numberOfElements;
                      }
                      NS_RELEASE(elementNode);
                    }
                  }
                  NS_RELEASE(elements);
                }
                NS_RELEASE(formElement);
              }
              NS_RELEASE(formNode);
            }
          }
          NS_RELEASE(forms);
        }
        NS_RELEASE(htmldoc);
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(shell);
  }

  /* return if no elements were put into the list */
  if (!XP_ListCount(wallet_PrefillElement_list)) {
    return;
  }

  /* prefill each element using the list */
  if (wallet_PrefillElement_list) {
    if (quick) {
      /* prefill each element without any user verification */
      XP_List * list_ptr = wallet_PrefillElement_list;
      wallet_PrefillElement * ptr;
      while((ptr = (wallet_PrefillElement *) XP_ListNextObject(list_ptr))!=0) {
        if (ptr->count) {
          if (ptr->inputElement) {
            ptr->inputElement->SetValue(*(ptr->value));
          } else {
            ptr->selectElement->SetSelectedIndex(ptr->selectIndex);
          }
        }
      }
      /* go thru list just generated and release everything */
      wallet_ReleasePrefillElementList(wallet_PrefillElement_list);
    } else {
      /* let user verify the prefills first */
      wallet_RequestToPrefill(wallet_PrefillElement_list);
    }
  }
#ifdef DEBUG
wallet_DumpStopwatch();
wallet_ClearStopwatch();
//wallet_DumpTiming();
//wallet_ClearTiming();
#endif
}

/*
 * see if user wants to capture data on current page
 */

PUBLIC void
WLLT_OKToCapture(PRBool * result, PRInt32 count, char* URLName) {
  *result =
    (strcmp(URLName, WALLET_EDITOR_URL)) && wallet_GetFormsCapturingPref() &&
    (count>=3) && FE_Confirm("Do you want to put the values on this form into your wallet?");
}

/*
 * capture the value of a form element
 */
PUBLIC void
WLLT_Capture(nsIDocument* doc, nsString field, nsString value) {

  /* do nothing if there is no value */
  if (!value.Length()) {
    return;
  }

  /* read in the mappings if they are not already present */
  wallet_Initialize();
  wallet_InitializeCurrentURL(doc);

  nsAutoString oldValue;

  /* is there a mapping from this field name to a schema name */
  nsAutoString schema;
  XP_List* FieldToSchema_list = wallet_FieldToSchema_list;
  XP_List* URLFieldToSchema_list = wallet_specificURLFieldToSchema_list;
  XP_List* SchemaToValue_list = wallet_SchemaToValue_list;
  XP_List* dummy;

  if ((wallet_ReadFromList(field, schema, dummy, URLFieldToSchema_list) != -1) ||
      (wallet_ReadFromList(field, schema, dummy, FieldToSchema_list) != -1)) {

    /* field to schema mapping already exists */

    /* is this a new value for the schema */
    if ((wallet_ReadFromList(schema, oldValue, dummy, SchemaToValue_list)==-1) || 
        (oldValue != value)) {

      /* this is a new value so store it */
      nsAutoString * aValue = new nsAutoString(value);
      nsAutoString * aSchema = new nsAutoString(schema);
      dummy = 0;
      wallet_WriteToList(*aSchema, *aValue, dummy, wallet_SchemaToValue_list);
      wallet_WriteToFile("SchemaValue.tbl", wallet_SchemaToValue_list, TRUE);
    }
  } else {

    /* no field to schema mapping so assume schema name is same as field name */

    /* is this a new value for the schema */
    if ((wallet_ReadFromList(field, oldValue, dummy, SchemaToValue_list)==-1) ||
        (oldValue != value)) {

      /* this is a new value so store it */
      nsAutoString * aField = new nsAutoString(field);
      nsAutoString * aValue = new nsAutoString(value);
      dummy = 0;
      wallet_WriteToList(*aField, *aValue, dummy, wallet_SchemaToValue_list);
      wallet_WriteToFile("SchemaValue.tbl", wallet_SchemaToValue_list, TRUE);
    }
  }
}
