/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
//
//  Address Book API Utility Functions
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  March 1998

#include "stdafx.h"
#include <stdio.h>    
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "wfemsg.h"     // for WFE_MSGGetMaster()
#include "nsstrseq.h"
#include "abapi.h"
#include "abhook.h"
#include "nabapi.h"
#include "mapismem.h"
#include "hiddenfr.h"
#include "msgcom.h"
#include "abutils.h"
#include "abcom.h"

extern CNetscapeApp theApp;

typedef struct {
  BOOL          foundInSearch;
  int           attribType;
  AB_AttribID   attribID;
  char          *ldifName;
} attribPairsType;

// I18N: Please don't touch these strings!
#define   ATTRIB_COUNT      19
attribPairsType   attribArray[] = { 
      // FALSE, dn: cn=first last,mail=email
      FALSE, CHAR_VALUE, AB_attribDisplayName, /*AB_attribFullName,*/ "cn: ",
      FALSE, CHAR_VALUE, AB_attribFamilyName,    "sn: ",
      FALSE, CHAR_VALUE, AB_attribGivenName,     "givenname: ",
      //  FALSE, CHAR_VALUE, "top",    "objectclass: ",
      //  FALSE, CHAR_VALUE, "person", "objectclass: ",
      FALSE, CHAR_VALUE, AB_attribInfo,          "description: ",
      FALSE, CHAR_VALUE, AB_attribLocality,      "locality: ",
      FALSE, CHAR_VALUE, AB_attribRegion,        "st: ",
      FALSE, CHAR_VALUE, AB_attribEmailAddress,  "mail: ",
      FALSE, CHAR_VALUE, AB_attribTitle,         "title: ",
      FALSE, CHAR_VALUE, AB_attribPOAddress,     "postOfficeBox: ",
      FALSE, CHAR_VALUE, AB_attribStreetAddress, "streetaddress: ",
      FALSE, CHAR_VALUE, AB_attribZipCode,       "postalcode: ",
      FALSE, CHAR_VALUE, AB_attribCountry,       "countryname: ",
      FALSE, CHAR_VALUE, AB_attribWorkPhone,     "telephonenumber: ",
      FALSE, CHAR_VALUE, AB_attribFaxPhone,      "facsimiletelephonenumber: ", 
      FALSE, CHAR_VALUE, AB_attribHomePhone,     "homephone: ",
      FALSE, CHAR_VALUE, AB_attribCompanyName,   "o: ",
      FALSE, CHAR_VALUE, AB_attribNickName,      "xmozillanickname: ",
      FALSE, BOOL_VALUE, AB_attribHTMLMail,      "xmozillausehtmlmail: ",
      FALSE, INT_VALUE,  AB_attribUseServer,     "xmozillauseconferenceserver: "};

BOOL
TackOnAttributeValuePair(MSG_Pane *abPane, LONG userIndex, 
                         LPSTR outString, AB_AttribID attribID, LPSTR stringKey, 
                         DWORD typeOfValue, LPSTR concatString)
{
  int               result;
  AB_AttributeValue *value;

  if (!outString)
    return FALSE;

  result = AB_GetEntryAttributeForPane(abPane, userIndex, attribID, &value);
  if (result != 0)
    return FALSE;


  if (typeOfValue == CHAR_VALUE)
  {
    // Do the key name first...
    if ( (value->u.string != NULL) && (value->u.string[0] != '\0') )
    {
      lstrcat(outString, stringKey);
      lstrcat(outString, (char *)value->u.string);

      if (concatString)
        lstrcat(outString, concatString);
    }
  }
  else   if (typeOfValue == BOOL_VALUE)
  {
    // Do the key name first...
    lstrcat(outString, stringKey);

    if (value->u.boolValue)
      lstrcat(outString, "TRUE");
    else
      lstrcat(outString, "FALSE");

    if (concatString)
      lstrcat(outString, concatString);
  }
  else   if (typeOfValue == INT_VALUE)
  {
    char  tval[16];

    // Do the key name first...
    lstrcat(outString, stringKey);

    wsprintf(tval, "%d", value->u.shortValue);
    lstrcat(outString, tval);
    if (concatString)
      lstrcat(outString, concatString);
  }

  AB_FreeEntryAttributeValue(value);
  return(TRUE);
}

BOOL
GetLDIFLineForUser(MSG_Pane *abPane, LONG userIndex, LPSTR outString, 
                   NABUserID *userID, NABUpdateTime *updtTime)
{
  ABID        id;
  int         i;
  BOOL        rc;

  if (!outString)
    return FALSE;

  outString[0] = '\0';
  lstrcpy(outString, "dn: ");

  rc = TackOnAttributeValuePair(abPane, userIndex, outString, AB_attribFullName, 
                                "cn: ", CHAR_VALUE, ",");
  if (!rc)
    return FALSE;
  rc = TackOnAttributeValuePair(abPane, userIndex, outString, AB_attribEmailAddress, "mail: ",
                                CHAR_VALUE, NAB_CRLF);
  if (!rc)
    return FALSE;

  for (i=0; i<ATTRIB_COUNT; i++)
  {
    (void) TackOnAttributeValuePair(abPane, userIndex, outString, 
                                attribArray[i].attribID, 
                                attribArray[i].ldifName, 
                                attribArray[i].attribType, NAB_CRLF);
  }

  // Get the ABID...
  int result = AB_GetABIDForIndex(abPane, userIndex, &id);
  if (result != 0)
    return FALSE;

  *userID = id;
  *updtTime = 0;
  return TRUE;
}

static HFILE    hHTMLFile = NULL;

BOOL    
OpenNABAPIHTMLFile(LPSTR fName, LPSTR title)
{
  hHTMLFile = _lcreat(fName, 0);
  if (hHTMLFile == HFILE_ERROR)
    return FALSE;

  if ( _lwrite(hHTMLFile, HTML_HEAD1, lstrlen(HTML_HEAD1)) == HFILE_ERROR)
  {
    _lclose(hHTMLFile);
    return FALSE;
  }

  if ( _lwrite(hHTMLFile, title, lstrlen(title)) == HFILE_ERROR)
  {
    _lclose(hHTMLFile);
    return FALSE;
  }

  if ( _lwrite(hHTMLFile, HMTL_HEAD2, lstrlen(HMTL_HEAD2)) == HFILE_ERROR)
  {
    _lclose(hHTMLFile);
    return FALSE;
  }

  return TRUE;
}

BOOL    
CloseNABAPIHTMLFile(void)
{
  if ( _lwrite(hHTMLFile, HTML_TAIL, lstrlen(HTML_TAIL)) == HFILE_ERROR)
  {
    _lclose(hHTMLFile);
    return FALSE;
  }

  _lclose(hHTMLFile);
  hHTMLFile = NULL;
  return TRUE;
}

#define HTMLSTART   1
#define HTMLEND     2
#define HTMLBREAK   3
#define HTMLSPACE   4
#define HTMLCOMMA   5
#define HTMLNEWROW  6
#define HTMLENDROW  7
 
BOOL
DumpHTMLForTable(BOOL hType)
{
  LPSTR cstart = "<TD>";
  LPSTR cend = "</TD>\n";
  LPSTR cbreak = "<BR>";
  LPSTR cspace = " ";
  LPSTR ccomma = ", ";
  LPSTR cnewrow = "<TR>\n";
  LPSTR cendrow = "</TR>\n";
  LPSTR ptr;

  if (hType == HTMLSTART)
    ptr = cstart;
  else if (hType == HTMLEND)
    ptr = cend;
  else if (hType == HTMLSPACE)
    ptr = cspace;
  else if (hType == HTMLCOMMA)
    ptr = ccomma;
  else if (hType == HTMLNEWROW)
    ptr = cnewrow;
  else if (hType == HTMLENDROW)
    ptr = cendrow;
  else
    ptr = cbreak;

  if ( _lwrite(hHTMLFile, ptr, lstrlen(ptr)) == HFILE_ERROR)
    return FALSE;
  else
    return TRUE;
}

BOOL
DumpHTMLValue(MSG_Pane *abPane, LONG userIndex, AB_AttribID attribID)
{
  int               result;
  AB_AttributeValue *value;

  result = AB_GetEntryAttributeForPane(abPane, userIndex, attribID, &value);
  if (result != 0)
    return FALSE;

  if ( (value->u.string != NULL) && (value->u.string[0] != '\0') )
  {
    if ( _lwrite(hHTMLFile, value->u.string, lstrlen(value->u.string)) == HFILE_ERROR)
      return FALSE;
    else
      return TRUE;
  }
  else
    return FALSE;
}

BOOL
DumpHTMLTableLineForUser(MSG_Pane *abPane, LONG userIndex)
{
  DumpHTMLForTable(HTMLNEWROW);

  DumpHTMLForTable(HTMLSTART);
  DumpHTMLValue(abPane, userIndex, AB_attribFamilyName);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLSTART);
  DumpHTMLValue(abPane, userIndex, AB_attribGivenName);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLSTART);
  DumpHTMLValue(abPane, userIndex, AB_attribEmailAddress);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLSTART);
  if (DumpHTMLValue(abPane, userIndex, AB_attribCompanyName))
    DumpHTMLForTable(HTMLBREAK);
  DumpHTMLValue(abPane, userIndex, AB_attribTitle);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLSTART);
  DumpHTMLValue(abPane, userIndex, AB_attribWorkPhone);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLSTART);
  DumpHTMLValue(abPane, userIndex, AB_attribFaxPhone);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLSTART);
  DumpHTMLValue(abPane, userIndex, AB_attribHomePhone);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLSTART);
  if (DumpHTMLValue(abPane, userIndex, AB_attribPOAddress))
    DumpHTMLForTable(HTMLBREAK);
  if (DumpHTMLValue(abPane, userIndex, AB_attribStreetAddress))
    DumpHTMLForTable(HTMLBREAK);

  if (DumpHTMLValue(abPane, userIndex, AB_attribLocality))
    DumpHTMLForTable(HTMLCOMMA);
  DumpHTMLValue(abPane, userIndex, AB_attribRegion);
  if (DumpHTMLValue(abPane, userIndex, AB_attribZipCode))
    DumpHTMLForTable(HTMLBREAK);
  DumpHTMLValue(abPane, userIndex, AB_attribCountry);
  DumpHTMLForTable(HTMLEND);

  DumpHTMLForTable(HTMLENDROW);
  return TRUE;
}

BOOL
FindAttributeInLine(LPSTR attribName, LPSTR addLine)
{
  DWORD totlen, complen, i;

  if ( (!attribName) || !(*attribName) )
    return FALSE;

  if ( (!addLine) || !(*addLine) )
    return FALSE;

  i = 0;
  totlen = strlen(addLine);
  complen = strlen(attribName);

  while ( (i + complen) <= totlen )
  {
    if (strncmp( attribName, addLine+i, complen ) == 0)
    {
      return TRUE;
    }

    i++;
  }

  return FALSE;
}

LPSTR
ExtractAttribValue(LPSTR attribName, LPSTR searchLine)
{
  DWORD totlen, complen, i;

  if ( (!attribName) || !(*searchLine) )
    return NULL;

  if ( (!searchLine) || !(*searchLine) )
    return NULL;

  i = 0;
  totlen = strlen(searchLine);
  complen = strlen(attribName);

  while ( (i + complen) <= totlen )
  {
    if (strncmp( attribName, searchLine+i, complen ) == 0)
    {
      if (i+complen == totlen)    // Check if we are at the end...
        return NULL;

      LPSTR  newPtr;
      LPSTR  startPtr;
      LPSTR  endPtr;
      DWORD  totalSize;

      // Have to add this hack for the fact we have an attribute that
      // is a subset of another (i.e. "mail: " and "xmozillausehtmlmail: ")
      if ( 
           (strcmp(attribName, "mail: ") == 0) && (i > 0) && 
           (*(searchLine+i-1) == 'l' )
         )
      {
        i++;
        continue;
      }
      // end of hack

      startPtr = searchLine + (i + complen);
      endPtr = startPtr;

      while ( ((*endPtr) != '\0') && ((*endPtr) != '\r') )
      {
        endPtr = (endPtr + 1);
      }

      totalSize = (endPtr - startPtr) + 1;
      newPtr = (LPSTR) malloc(totalSize);
      if (!newPtr)
        return NULL;

      memset(newPtr, 0, totalSize);
      strncpy(newPtr, startPtr, (totalSize - 1));
      return newPtr;
    }

    i++;
  }

  return NULL;
}

BOOL
ThisIsAStringAttrib(AB_AttribID attrib)
{
  int i;

  for (i=0; i<ATTRIB_COUNT; i++)
  {
    if (attribArray[i].attribID == attrib)
    {
      if (attribArray[i].attribType == CHAR_VALUE)
        return TRUE;
      else
        return FALSE;
    }
  }

  return FALSE;
}

// Inserting Address Book Entires...
NABError
InsertEntryToAB(AB_ContainerInfo *abContainer, LPSTR newLine, 
                BOOL updateOnly, ABID *updateID)
{
ULONG             i;
DWORD             numItems = 0;
ABID              tempEntryID;
AB_AttributeValue *values;
BOOL              found;
USHORT            currentAttribLoc;

  // Build the entries to add for the address book...
  for (i=0; i<ATTRIB_COUNT; i++)
  {
    attribArray[i].foundInSearch = FindAttributeInLine(attribArray[i].ldifName, newLine);
    if (found)
      numItems++;
  }

  if (numItems == 0)
    return(NAB_FAILURE);

  ++numItems;   // add one for the type...

  // Now that we have this information, malloc the memory needed and
  // move on...
  values = (AB_AttributeValue *) malloc(sizeof(AB_AttributeValue) * numItems);
  if (!values)
    return(NAB_MEMORY_FAILURE);

  memset(values, 0, (sizeof(AB_AttributeValue) * numItems));

  // First should be the type of entry...
  values[0].attrib      = AB_attribEntryType;
  values[0].u.entryType = AB_Person;

  LPSTR   ptr;
  currentAttribLoc = 1;
  for (i=0; i<ATTRIB_COUNT; i++)
  {
    if (attribArray[i].foundInSearch)
    {
      if ( (ptr = ExtractAttribValue(attribArray[i].ldifName, newLine)) == NULL)
        continue;

       // Here?...then we found it!
      values[currentAttribLoc].attrib = attribArray[i].attribID;
      if (attribArray[i].attribType == CHAR_VALUE)
      {
        values[currentAttribLoc].u.string = ptr;        
      }
      else if (attribArray[i].attribType == BOOL_VALUE)
      {
        if (strcmp(ptr, "TRUE") == 0)
          values[currentAttribLoc].u.boolValue = TRUE;
        else
          values[currentAttribLoc].u.boolValue = FALSE;
        free(ptr);
      }
      else if (attribArray[i].attribType == INT_VALUE)
      {
        values[currentAttribLoc].u.shortValue = atoi(ptr);
        free(ptr);
      }

      currentAttribLoc++;
    }
  }

  int result;
  if (updateOnly)
  {
    result = AB_SetEntryAttributes(abContainer, *updateID, values, currentAttribLoc);
  }
  else
  {
    result = AB_AddUserAB2(abContainer, values, currentAttribLoc, &tempEntryID);
    *updateID = tempEntryID;
  }

  // Now free all allocated memory...
  for (i=1; i<currentAttribLoc; i++)  // start from 1 - 0 is AB_attribEntryType
  {
    if (ThisIsAStringAttrib(values[i].attrib))
    {
      if (values[i].u.string != NULL)
        free(values[i].u.string);
    }
  }

  free(values);

  if (result) 
    return NAB_FAILURE;
  else
    return NAB_SUCCESS;
}

BOOL
GetIDSearchField(LPSTR ldifInfo, DWORD *id, LPSTR *searchValue)
{
  LPSTR   ptr;
  DWORD   i;

  for (i=0; i<ATTRIB_COUNT; i++)
  {
    if (attribArray[i].attribType != CHAR_VALUE)
      continue;

    ptr = ExtractAttribValue(attribArray[i].ldifName, ldifInfo);
    if (ptr != NULL)
    {
      *id = attribArray[i].attribID;
      break;
    }
  }

  if (!ptr)
    return FALSE;
  else
  {
    *searchValue = ptr;
    return TRUE;
  }
}

BOOL
FindValueForIDInLine(MSG_Pane  *addressBookPane, LONG userIndex, 
                     AB_AttribID id, LPSTR searchValue)
{
  int               result;
  AB_AttributeValue *value;
  BOOL              found = FALSE;

  if ((!searchValue) || (!*searchValue))
    return FALSE;

  result = AB_GetEntryAttributeForPane(addressBookPane, userIndex, id, &value);
  if (result != 0)
    return FALSE;

  // Do the key name first...
  if ( (value->u.string != NULL) && (value->u.string[0] != '\0') )
  {
    if ( strlen(value->u.string) >= strlen(searchValue) ) 
    {
      if (_strnicmp(value->u.string, searchValue, strlen(searchValue)) == 0)
      {
        found = TRUE;
      }
    }
  }

  AB_FreeEntryAttributeValue(value);
  return(found);
}

BOOL
SearchABForAttrib(AB_ContainerInfo  *abContainer,
                  LPSTR             searchAttrib,
                  LPSTR             ldifInfo,
                  NABUserID         *userID, 
                  NABUpdateTime     *updtTime)
{
  int       result;
  MSG_Pane  *addressBookPane;    // Container info for a particular addr book

  result = AB_CreateABPane(&addressBookPane, theApp.m_pAddressContext, WFE_MSGGetMaster());
  if (result)
  {
    return FALSE;    
  }

  result = AB_InitializeABPane(addressBookPane, abContainer);
  if (result)
  {
    AB_ClosePane(addressBookPane);
		return(FALSE);
  }

  DWORD         id;
  int           currentLocation = 0;
  LONG          lineCount = MSG_GetNumLines(addressBookPane);
  LPSTR         searchValue = NULL;
  BOOL          found = FALSE;

  //
  // Support lookups by ABID's...
  if ((searchAttrib == NULL) || (searchAttrib[0] == '\0'))
  {
    while ( currentLocation < lineCount )    
    {
      ABID    id;

      // Get the ABID...
      int result = AB_GetABIDForIndex(addressBookPane, currentLocation, &id);
      if (result != 0)
      {
        ++currentLocation;
        continue;
      }

      if (id == *userID)
      {
        found = TRUE;
        if (!GetLDIFLineForUser(addressBookPane, currentLocation, ldifInfo, userID, updtTime))
        {
          found = FALSE;
          currentLocation++;
          continue;
        }

        break;
      }
      
      // Increment for next call...
      currentLocation++;
    }
    
    AB_ClosePane(addressBookPane);
    return found;
  }
  // Support lookups by ABID's...
  //

  if (!GetIDSearchField(searchAttrib, &id, &searchValue))
  {
    AB_ClosePane(addressBookPane);
		return(FALSE);
  }
 
  while ( currentLocation < lineCount )    
  {
    found = FindValueForIDInLine(addressBookPane, currentLocation, 
                                 (AB_AttribID)id, searchValue);
    if (found)
    {
      if (!GetLDIFLineForUser(addressBookPane, currentLocation, ldifInfo, userID, updtTime))
      {
        found = FALSE;
        currentLocation++;
        continue;
      }
      break;
    } 

    // Increment for next call...
    currentLocation++;
  }

  if (searchValue != NULL)
    free(searchValue);

  AB_ClosePane(addressBookPane);
  return found;
}

