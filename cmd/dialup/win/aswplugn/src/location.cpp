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

#include <npapi.h>
#include "plugin.h"
#include <tapi.h>

// resource include
#ifdef WIN32
#include "resource.h"
#endif

#define TAPI_VERSION                0x00010004

//
// The format of the entry is
//  LocationX=Y,"name","outside-line-local","outside-line-long-D","area-code",1,0,0,1,"",tone=0,"call-waiting-string"
//
typedef struct tapiLineStruct {
    int         nIndex;
    char		csLocationName[60];
    char		csOutsideLocal[20];
    char		csOutsideLongDistance[20];
    char		csAreaCode[20];
    int         nCountryCode;
    int         nCreditCard;
    int         nDunnoB;
    int         nDunnoC;
    char		csDialAsLongDistance[10];
    int         nPulseDialing;
    char		csCallWaiting[20];
} tapiLine;


extern BOOL getMsgString(char *buf, UINT uID);
extern int DisplayErrMsg(char *text, int style);
extern void FAR PASCAL lineCallbackFuncNT(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);


//********************************************************************************
// GetInt
//
// Pull an int off the front of the string and return the new string ptr
//********************************************************************************
char * GetInt(char * pInString, int * pInt)
{
    char * pStr;
    char buf[32];

    if(!pInString) {
        *pInt = 0;
        return(NULL);
    }

    // copy the string over.  This strchr should be smarter    
    pStr = strchr(pInString, ',');
    if(!pStr) {
        *pInt = 0;
        return(NULL);
    }

    // convert the string            
    strncpy(buf, pInString, pStr - pInString);
    buf[pStr - pInString] = '\0';
    *pInt = atoi(buf);

    // return the part after the int
    return(pStr + 1);

}




//********************************************************************************
// GetString
//
// Pull a string from the front of the incoming string and return
//   the first character after the string
//********************************************************************************
char * GetString(char * pInString, char *csString)
{
	csString[0] = '\0';
	int i=0,x=0;
	BOOL copy = FALSE;
	char newpInString[MAX_PATH];

    if(!pInString) {
        csString = "";
        return(NULL);
    }

    // skip over first quote
    if(pInString[i] == '\"')
        i++;

    // copy over stuff by hand line a moron
    while(pInString[i] != '\"') {
		//strcat(csString, (char *)pInString[i]);
		csString[x] = pInString[i];
		i++;
		x++;
		copy = TRUE;
	}
	
	if (copy)
		csString[x] = '\0';


    // skip over the closing quote
    if(pInString[i] == '\"')
        i++;

    // skip over the comma at the end
    if(pInString[i] == ',')
        i++;

	newpInString[0]='\0';
	x=0;
	for (unsigned short j=i; j<strlen(pInString); j++) {
		//strcat(newpInString, (char *)pInString[j]);
		newpInString[x] = pInString[j];
		x++;
	}
	
	newpInString[x]='\0';

	strcpy(pInString, newpInString);

    return(pInString);

}




//********************************************************************************
// ReadLine
//
// Read a line out of the telephon.ini file and fill the stuff in
//********************************************************************************
void ReadLine(char * lpszFile, int nLineNumber, tapiLine * line) 
{
    
    char buf[256];
    char pLocation[128];
    sprintf(pLocation, "Location%d", nLineNumber);
    ::GetPrivateProfileString("Locations", pLocation, "", buf, sizeof(buf), lpszFile);

    char * pString = buf;
    pString = GetInt(pString, &line->nIndex);
    pString = GetString(pString, (char *)&line->csLocationName);
    pString = GetString(pString, (char *)&line->csOutsideLocal);
    pString = GetString(pString, (char *)&line->csOutsideLongDistance);
    pString = GetString(pString, (char *)&line->csAreaCode);
    pString = GetInt(pString, &line->nCountryCode);
    pString = GetInt(pString, &line->nCreditCard);
    pString = GetInt(pString, &line->nDunnoB);
    pString = GetInt(pString, &line->nDunnoC);
    pString = GetString(pString, (char *)&line->csDialAsLongDistance);
    pString = GetInt(pString, &line->nPulseDialing);
    pString = GetString(pString, (char *)&line->csCallWaiting);

}



//********************************************************************************
// WriteLine
//
// Given a tapiLine structure write it out to telephon.ini
//
//********************************************************************************
void WriteLine(char * lpszFile, int nLineNumber, tapiLine * line) 
{

    char buffer[256];
    sprintf(buffer, "%d,\"%s\",\"%s\",\"%s\",\"%s\",%d,%d,%d,%d,\"%s\",%d,\"%s\"",
        line->nIndex,
        (const char *) line->csLocationName,
        (const char *) line->csOutsideLocal,
        (const char *) line->csOutsideLongDistance,
        (const char *) line->csAreaCode,
        line->nCountryCode,
        line->nCreditCard,
        line->nDunnoB,
        line->nDunnoC,
        (const char *) line->csDialAsLongDistance,
        line->nPulseDialing,
        (const char *) line->csCallWaiting);

    char pLocation[32];
    sprintf(pLocation, "Location%d", nLineNumber);
    ::WritePrivateProfileString("Locations", pLocation, buffer, lpszFile);

}



//********************************************************************************
// SetLocationInfo
//
// sets the location info for win95 dialers
//
// The format of the entry is
//  LocationX=Y,"name","outside-line-local","outside-line-long-D","area-code",1,0,0,1,"",tone=0,"call-waiting-string"
//
//********************************************************************************
BOOL SetLocationInfo(ACCOUNTPARAMS account, LOCATIONPARAMS location)
{

	// first read information from telephon.ini
	char buf[256];

    // get windows directory
    char lpszDir[MAX_PATH];
    if(GetWindowsDirectory(lpszDir, MAX_PATH) == 0)
        return -1;

    strcat(lpszDir, "\\telephon.ini");

	// now we build new line information based on the old one and some info
    // see if there were any locations to begin with
    ::GetPrivateProfileString("Locations", "CurrentLocation", "", buf, sizeof(buf), lpszDir);
    if(buf[0] == '\0') {

        // build the string
        tapiLine line;
        line.nIndex = 0;
		strcpy(line.csLocationName, "Default Location");
		strcpy(line.csOutsideLocal, location.OutsideLineAccess);
		strcpy(line.csOutsideLongDistance, location.OutsideLineAccess);
		strcpy(line.csAreaCode, location.UserAreaCode);
        line.nCountryCode = location.UserCountryCode;
        line.nCreditCard = 0;
        line.nDunnoB = 0;
        line.nDunnoC = 1;

		if (location.DialAsLongDistance == TRUE) {
			strcpy(line.csDialAsLongDistance, "528");
		} else {
			strcpy(line.csDialAsLongDistance, "");
		}
        line.nPulseDialing = (location.DialType == 0 ? 1 : 0);
        if(location.DisableCallWaiting)
			strcpy(line.csCallWaiting, location.DisableCallWaitingCode);
        else
            strcpy(line.csCallWaiting, "");

        WriteLine(lpszDir, 0, &line);

        // need to create a whole location section
        ::WritePrivateProfileString("Locations", "CurrentLocation", "0,0", lpszDir);
        ::WritePrivateProfileString("Locations", "Locations", "1,1", lpszDir);
        ::WritePrivateProfileString("Locations", "Inited", "1", lpszDir);

    }
    else {

        int nId, nCount;
        sscanf(buf, "%d,%d", &nId, &nCount);

        // read the line
        tapiLine line;                                
        ReadLine(lpszDir, nId, &line);

		strcpy(line.csOutsideLocal, location.OutsideLineAccess);
		strcpy(line.csOutsideLongDistance, location.OutsideLineAccess);
        if(location.DisableCallWaiting)
			strcpy(line.csCallWaiting, location.DisableCallWaitingCode);
        else
            strcpy(line.csCallWaiting, "");

        line.nPulseDialing = (location.DialType == 0 ? 1 : 0);

        if(strcmp(location.UserAreaCode, "") != 0)
            strcpy(line.csAreaCode, location.UserAreaCode);

		if (location.DialAsLongDistance == TRUE) {
			strcpy(line.csDialAsLongDistance, "528");
		} else {
			strcpy(line.csDialAsLongDistance, "");
		}

        // write the line back out
        WriteLine(lpszDir, nId, &line);

    }

	return TRUE;
}


//********************************************************************************
// SetLocationInfo
//
// sets the location info for winNT dialers
//********************************************************************************
BOOL SetLocationInfoNT(ACCOUNTPARAMS account, LOCATIONPARAMS location)
{
	
	LINEINITIALIZEEXPARAMS  m_LineInitExParams;
	HLINEAPP                m_LineApp;
	DWORD                   dwNumDevs;
	LINETRANSLATECAPS       m_LineTranslateCaps;

	DWORD dwApiVersion = 0x00020000;


	// Initialize TAPI. in order to get current location ID
	m_LineInitExParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
	m_LineInitExParams.dwTotalSize = sizeof(LINEINITIALIZEEXPARAMS);
	m_LineInitExParams.dwNeededSize = sizeof(LINEINITIALIZEEXPARAMS);


	if (lineInitialize(&m_LineApp, DLLinstance, lineCallbackFuncNT, NULL, &dwNumDevs) != 0) {
		char buf[255];
		if (getMsgString(buf, IDS_NO_TAPI))
		   DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);

		return FALSE;
	}

	m_LineTranslateCaps.dwTotalSize = sizeof(LINETRANSLATECAPS);
	m_LineTranslateCaps.dwNeededSize = sizeof(LINETRANSLATECAPS);
	if (lineGetTranslateCaps(m_LineApp, dwApiVersion, &m_LineTranslateCaps) != 0)
		return FALSE;


	//m_LineTranslateCaps.dwCurrentLocationID

	// gets the location info from registry
	HKEY hKey;
	char *keyPath = (char *)malloc(sizeof(char) * 512);

	assert(keyPath);
	if (!keyPath)
		return NULL;
	strcpy(keyPath, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\locations");

	// finds the user profile location in registry
	if(ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, NULL, KEY_ALL_ACCESS, &hKey))
		return -1;

	DWORD subKeys;
	DWORD maxSubKeyLen;
	DWORD maxClassLen;
	DWORD values;
	DWORD maxValueNameLen;
	DWORD maxValueLen;
	DWORD securityDescriptor;
	FILETIME lastWriteTime;

	// get some information about this reg key 
	if (ERROR_SUCCESS != RegQueryInfoKey(hKey, NULL, NULL, NULL, &subKeys, &maxSubKeyLen, &maxClassLen, &values, &maxValueNameLen, &maxValueLen, &securityDescriptor, &lastWriteTime))
		return -2;


	// now loop through the location keys to find the one that matches current location ID
	if (subKeys > 0) {

		DWORD subkeyNameSize = maxSubKeyLen + 1;
		char subkeyName[20];

		for (DWORD index=0; index<subKeys; index++) {

			// gets a location key name
			if (ERROR_SUCCESS != RegEnumKey(hKey, index, subkeyName, subkeyNameSize)) 
				return -3;

			//try open location key
			char NewSubkeyPath[260];
			HKEY hkeyNewSubkey;

			strcpy((char *)NewSubkeyPath, keyPath);
			strcat((char *)NewSubkeyPath, "\\");
			strcat((char *)NewSubkeyPath, subkeyName);

			if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, (char *)NewSubkeyPath, NULL, KEY_ALL_ACCESS, &hkeyNewSubkey))
				return -4;

			DWORD valbuf;
			DWORD type = REG_SZ;
			DWORD bufsize = 20;

			// get location key's ID value
			if (ERROR_SUCCESS != RegQueryValueEx(hkeyNewSubkey, "ID", NULL, &type, (LPBYTE)&valbuf, &bufsize))
				return -5;

			// if it matches the default location ID
			if (valbuf == m_LineTranslateCaps.dwCurrentLocationID) {

				// we got the location we want, now change the pulse/tone flag
				DWORD FlagsVal;
				if (location.DialType == 0)
					if (location.DisableCallWaiting)
						FlagsVal = 4;
					else
						FlagsVal = 0;
				else
					if (location.DisableCallWaiting)
						FlagsVal = 5;
					else
						FlagsVal = 1;
					
				if (ERROR_SUCCESS != RegSetValueEx(hkeyNewSubkey, "Flags", NULL, type, (LPBYTE)&FlagsVal, bufsize))
					return -6;

				// sets the OutsideAccess
				if (ERROR_SUCCESS != RegSetValueEx(hkeyNewSubkey, "OutsideAccess", NULL, REG_SZ, (LPBYTE)&location.OutsideLineAccess, strlen(location.OutsideLineAccess)+1))
					return -7;

				if (ERROR_SUCCESS != RegSetValueEx(hkeyNewSubkey, "LongDistanceAccess", NULL, REG_SZ, (LPBYTE)&location.OutsideLineAccess, strlen(location.OutsideLineAccess)+1))
					return -8;

				// sets call waiting
				char *callwaiting;
				
				if(location.DisableCallWaiting)
					callwaiting = location.DisableCallWaitingCode;
				else
					callwaiting ="";

				if (ERROR_SUCCESS != RegSetValueEx(hkeyNewSubkey, "DisableCallWaiting", NULL, REG_SZ, (LPBYTE)callwaiting, strlen(callwaiting)+1))
					return -9;

				
				// sets user's area code
		        if(strcmp(location.UserAreaCode, "") != 0) {
					if (ERROR_SUCCESS != RegSetValueEx(hkeyNewSubkey, "AreaCode", NULL, REG_SZ, (LPBYTE)location.UserAreaCode, strlen(location.UserAreaCode)+1))
						return -10;
				} else {
					// check if we're international, and force a default area code, so that we don't get an error creating a dialer
					if (account.IntlMode) {
						char *code="415";
						if (ERROR_SUCCESS != RegSetValueEx(hkeyNewSubkey, "AreaCode", NULL, REG_SZ, (LPBYTE)code, strlen(code)+1))
							return -11;
					}
				}

				RegCloseKey(hkeyNewSubkey);
				break;
			}
	
			RegCloseKey(hkeyNewSubkey);
		}
	}

	RegCloseKey(hKey);

	return TRUE;
}

