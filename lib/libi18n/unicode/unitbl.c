/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "prlog.h"
#include "prtypes.h"
#include "libi18n.h"
#include "csid.h"

#define MAXUTABLENAME	16
typedef struct tblrsrcinfo {
	char	name[MAXUTABLENAME];
	uint16	refcount;
    HGLOBAL hTbl;
} tblrsrcinfo;

typedef struct utablename {
	uint16		csid;
	tblrsrcinfo	frominfo;
	tblrsrcinfo toinfo;

} utablename;

PR_PUBLIC_API(void*) 	UNICODE_LOADUCS2TABLE(uint16 csid, int from);
PR_PUBLIC_API(void)	UNICODE_UNLOADUCS2TABLE(uint16 csid, void *utblPtr, int from);

tblrsrcinfo* 	unicode_FindUTableName(uint16 csid, int from);

HINSTANCE _unicode_hInstance;

#ifdef _WIN32
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
  switch (dwReason)
  {
    case DLL_PROCESS_ATTACH:
      _unicode_hInstance = hDLL;
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:
      _unicode_hInstance = NULL;
      break;
  }

    return TRUE;
}

#else  /* ! _WIN32 */

int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg, 
                      WORD cbHeapSize, LPSTR lpszCmdLine )
{
    _unicode_hInstance = hInst;
    return TRUE;
}

BOOL CALLBACK _export _loadds WEP(BOOL fSystemExit)
{
    _unicode_hInstance = NULL;
    return TRUE;
}
#endif /* ! _WIN32 */


utablename utablenametbl[] =
{
	/* Special Note, for Windows, we use cp1252 for CS_LATIN1 */
	{CS_ASCII,		{"CP1252.UF",	0,	NULL},		{"CP1252.UT",	0,	NULL}},
	{CS_LATIN1,		{"CP1252.UF",	0,	NULL},		{"CP1252.UT",	0,	NULL}},
	{CS_SJIS,		{"SJIS.UF",		0,	NULL},		{"SJIS.UT",		0,	NULL}},
	{CS_BIG5,		{"BIG5.UF",		0,	NULL},		{"BIG5.UT",		0,	NULL}},
	{CS_GB_8BIT,	{"GB2312.UF",	0,	NULL},		{"GB2312.UT",	0,	NULL}},
	{CS_KSC_8BIT,	{"U20KSC.UF",	0,	NULL},		{"U20KSC.UT",	0,	NULL}},
	{CS_CP_1251,	{"CP1251.UF",	0,	NULL},		{"CP1251.UT",	0,	NULL}},
	{CS_CP_1250,		{"CP1250.UF",	0,	NULL},		{"CP1250.UT",	0,	NULL}},
	{CS_CP_1253,		{"CP1253.UF",	0,	NULL},		{"CP1253.UT",	0,	NULL}},
	{CS_8859_9,		{"CP1254.UF",	0,	NULL},		{"CP1254.UT",	0,	NULL}},
	{CS_SYMBOL,		{"MACSYMBO.UF",	0,	NULL},		{"MACSYMBO.UT",	0,	NULL}},
	{CS_DINGBATS,	{"MACDINGB.UF",	0,	NULL},		{"MACDINGB.UT",	0,	NULL}},
	{CS_DEFAULT,	{"",			0,	NULL},		{"",			0,	NULL}}
};
static tblrsrcinfo* unicode_FindUTableName(uint16 csid, int from)
{
	int i;
	for(i=0; utablenametbl[i].csid != CS_DEFAULT; i++)
	{
		if(utablenametbl[i].csid == csid)
			return from ? &(utablenametbl[i].frominfo) 
						: &(utablenametbl[i].toinfo);
	}
#ifdef _DEBUG
	OutputDebugString("unicode_FindUTableName: Cannot find table information");
#endif /* _DEBUG */

	return NULL;
}
PR_PUBLIC_API(void *) UNICODE_LOADUCS2TABLE(uint16 csid, int from)
{
    HRSRC   hrsrc;
    HGLOBAL hRes;
	void *table;
	tblrsrcinfo* tbl = unicode_FindUTableName(csid, from);
	/*	Cannot find this csid */
	if(tbl == NULL)
		return (NULL);
	/*  Find a loaded table */
	if(tbl->refcount > 0)
	{
		tbl->refcount++;
		return ((void*)LockResource(tbl->hTbl));
	}
	/*  Find a unloaded table */
	hrsrc = FindResource(_unicode_hInstance, tbl->name, RT_RCDATA);
	if(!hrsrc) 
	{
		/* cannot find that RCDATA resource */
#ifdef _DEBUG
		OutputDebugString("UNICODE_LoadUCS2Table cannot find table resource");
#endif
		return (NULL);
	}
	hRes = LoadResource(_unicode_hInstance,hrsrc);
	if(!hRes) 
	{
		/* cannot find that RCDATA resource */
#ifdef _DEBUG
		OutputDebugString("UNICODE_LoadUCS2Table cannot load table resource");
#endif
		return (NULL);
	}
	table = (void*)	LockResource(hRes);
	if(!table)
	{
		/* cannot find that RCDATA resource */
#ifdef _DEBUG
		OutputDebugString("UNICODE_LoadUCS2Table cannot lock table resource");
#endif
		return (NULL);
	}
	tbl->refcount++;
	tbl->hTbl = hRes;
	return(table);
}
PR_PUBLIC_API(void)	UNICODE_UNLOADUCS2TABLE(uint16 csid, void *utblPtr, int from)
{
	tblrsrcinfo* tbl = unicode_FindUTableName(csid, from);
	/*	Cannot find this csid */
	if(tbl == NULL)
	{
#ifdef _DEBUG
		OutputDebugString("unicode_UnloadUCS2Table don't know how to deal with this csid");
#endif
		return;
	}
	/*  Find a loaded table */
	if(tbl->refcount == 0)
	{
#ifdef _DEBUG
		OutputDebugString("unicode_UnloadUCS2Table try to unload an unloaded table");
#endif
		tbl->hTbl = NULL;
		return;
	}
#ifndef _WIN32
	/*  UnlockResource to decrease the internal reference count */
	UnlockResource(tbl->hTbl);
#endif
	tbl->refcount--;
	if(tbl->refcount <= 0)
	{
		FreeResource(tbl->hTbl);
		tbl->hTbl = NULL;
		tbl->refcount = 0;
	}
}
