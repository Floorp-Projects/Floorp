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

#include "stdafx.h"  
#include "mucproc.h"

#define new DEBUG_NEW
//********************************************************************************
//
// CMucProc constructor
//
//********************************************************************************
CMucProc::CMucProc()
{
	acct_flag = FALSE;
	modem_flag = FALSE;   
	m_lpfnPEPluginFunc = NULL;
}

//********************************************************************************
//
// LoadMuc
//
//********************************************************************************
BOOL CMucProc::LoadMuc() 
{
	long m_version;

	if(m_lpfnPEPluginFunc != NULL) 
		return TRUE;   
	m_lpfnPEPluginFunc = (FARPEFUNC)::GetProcAddress(theApp.m_hPEInst,
	    "PEPluginFunc");
    
	if(m_lpfnPEPluginFunc == NULL)
	{
		FreeLibrary(theApp.m_hPEInst);
		return FALSE;
	}

	// check the MUC version number
	(*m_lpfnPEPluginFunc)(kGetPluginVersion, NULL, &m_version);
	if(m_version != 0x00010000)
	{
		FreeLibrary(theApp.m_hPEInst);  
		m_lpfnPEPluginFunc = NULL;
		return FALSE;
	}
	return TRUE;
}
// account information
//********************************************************************************
//
// GetAcctNameList
//
//********************************************************************************
BOOL CMucProc::GetAcctArray(CStringArray *acctList)
{
	if(!acct_flag)
		GetAccountNames();
    
	int size = acctNames.GetSize();
    
	if(size !=0)
	{   
#ifdef XP_WIN32 
		acctList->Copy(acctNames);       
#else
		for( int i=0; i<size; i++)
			acctList->SetAtGrow(i, acctNames[i]);
#endif                  
		return TRUE;
	}
	return FALSE;
}

//********************************************************************************
//
// IsAccountValid
//
//********************************************************************************
BOOL CMucProc::IsAcctValid(char *acctStr)
{
	int             i = 0;

	if(!acct_flag)
		GetAccountNames();

	int num = acctNames.GetSize();

	while(i<num)
	{
		if(_stricmp(acctStr, (const char*)acctNames[i]) == 0)
			return TRUE;
		else
			i++;
	}
	return FALSE;
}

// modem information
//********************************************************************************
//
// GetModemList
//
//********************************************************************************
BOOL CMucProc::GetModemArray(CStringArray *modemList)
{
	if(!modem_flag)
		GetModemNames();

	
	int num = modemNames.GetSize();
	if(num !=0)
	{ 
#ifdef WIN32
		modemList->Copy(modemNames);      
#else
		for(int i=0; i<num; i++)
			modemList->SetAtGrow(i, modemNames.GetAt(i));
#endif
		return TRUE;
	}
	return FALSE;
}

//********************************************************************************
//
// IsModemValid
//
//********************************************************************************
BOOL CMucProc::IsModemValid(char* modemStr)
{
	int             i = 0;

	if(!modem_flag)
		GetModemNames();
	
	int num = modemNames.GetSize();

	while(i<num)
	{
		if(strcmp(modemStr, (const char*)modemNames.GetAt(i)) == 0)
			return TRUE;
		else
			i++;
	}
	return FALSE;
}

// GetModemList

//********************************************************************************
//
// GetModemNames
//
//********************************************************************************

BOOL CMucProc::GetModemNames()
{
	char    modemResults[MAX_PATH*10];
	int             numDevices;
	int             i;
	char    temp[MAX_PATH];

	memset(modemResults, 0x00, MAX_PATH*10);

	if(0 == (*m_lpfnPEPluginFunc)(kSelectModemConfig, &numDevices, modemResults))
	{
		if(modemResults != NULL && numDevices != 0)
		{
			// decoding: the muc string is deliminated by "()"                                
			CString         key = "()"; 
			CString         str = modemResults;
			int             pos;
	    
	    i=0;
			while(i<numDevices)
			{
				pos = str.Find(key);  
				modemNames.Add(str.Left(pos));
				str = str.Mid(pos+2);
				i++;
			}
			modem_flag = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}



//********************************************************************************
//
// GetAccountNames
//
//********************************************************************************
BOOL CMucProc::GetAccountNames()
{
	int             numNames=0;
	int             i=0;
	char    acctResults[MAX_PATH*10];
	char    temp[MAX_PATH];

	memset(acctResults, 0x00, MAX_PATH*10); 

      if(0==(*m_lpfnPEPluginFunc)(kSelectAcctConfig, &numNames, acctResults))
	{
		if(acctResults != NULL && numNames != 0)
		{ 
			// decoding: the muc string is deliminated by "()"                                
			CString         key = "()"; 
			CString         str = acctResults;
			int             pos;    
			
			i=0;
			while(i<numNames)
			{
				pos = str.Find(key);  
				acctNames.Add(str.Left(pos));
				str = str.Mid(pos+2);
				i++;
			}
			acct_flag = TRUE;
			return TRUE;
		}
	}
	return FALSE;
}

//********************************************************************************
//
// SetDialOnDemand
//
//********************************************************************************
void CMucProc::SetDialOnDemand(CString acctStr, BOOL flag)
{
	if(!LoadMuc())  return;

	(*m_lpfnPEPluginFunc)(kSelectDialOnDemand, (void*)((const char*)acctStr), &flag);
}
