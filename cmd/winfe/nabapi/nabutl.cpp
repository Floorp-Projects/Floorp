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
//  Various utils needed for the Address book functions
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <windows.h> 
#include <time.h> 
#include <sys/stat.h>
#include <io.h>
#include "xpapi.h"
#include "trace.h"
#include "nabipc.h"
#include "nabutl.h"

#define     ONE_SEC       20
#define     STARTUP       "Start"
#define     STARTUP_TIME  20
#define     EXIT          "Exit"
#define     EXIT_TIME     10

//
// Global variables
//
BOOL	  gLoggingEnabled = FALSE;

void
SetLoggingEnabled(BOOL val)
{
  gLoggingEnabled = val;
}

// Log File
void 
LogString(LPCSTR pStr1)
{
  // Off of the declaration line...
  LPCSTR pStr2 = NULL;
  BOOL useStr1 = TRUE;

	if (gLoggingEnabled)
	{
		char tempPath[_MAX_PATH] = "";
    if (getenv("TEMP"))
    {
      lstrcpy((LPSTR) tempPath, getenv("TEMP"));  // environmental variable
    } 

		int len = lstrlen(tempPath);
		if ((len > 1) && tempPath[len - 1] != '\\')
		{
			lstrcat(tempPath, "\\");
		}
		lstrcat(tempPath, szNABLog);
		HFILE hFile = _lopen(tempPath, OF_WRITE);
		if (hFile == HFILE_ERROR)
		{
			hFile = _lcreat(tempPath, 0);
		}
		if (hFile != HFILE_ERROR)
		{
			_llseek(hFile, 0, SEEK_END);		// seek to the end of the file
			LPCSTR pTemp = useStr1 ? pStr1 : pStr2;
			_lwrite(hFile, pTemp, lstrlen(pTemp));
			_lclose(hFile);
		}
	}
}

void
BuildMemName(LPSTR name, ULONG winSeed)
{
  static DWORD    id = 0;

  if (id == 0)
  {
    // Seed the random-number generator with current time so that
    // the numbers will be different every time we run.
    srand( (unsigned)time( NULL ) );
    id = rand();
  }

  wsprintf(name, "NAB_IPC_SMEM-%d", (winSeed + id++));
  TRACE("Shared Memory Name = [%s]\n", name);
}

DWORD 
ValidateFile(LPCSTR szFile) 
{
  	struct _stat buf;
    int result;

    result = _stat( szFile, &buf );
    if (result != 0)
      return(1);

    if (!(buf.st_mode & S_IREAD))
      return(2);

    return(0);
}

DWORD 
GetFileCount(LPSTR pFiles, LPSTR delimChar)
{
  DWORD   count = 1;
	if ((!pFiles) || (!*pFiles))
		return(0);

  for (DWORD i=0; i<strlen(pFiles); i++)
  {
    if (pFiles[i] == delimChar[0])
    {
      ++count;
    }
  }

  return(count);
}

ULONG
GetFileSize(LPSTR fName)
{
  struct _stat buf;
  int result;
  
  result = _stat( fName, &buf );
  if (result != 0)
    return(0);
  
  return(buf.st_size);
}
 
LPVOID
LoadBlobToMemory(LPSTR fName)
{
  UCHAR     *ptr = NULL;
  ULONG     bufSize = GetFileSize(fName);

  if (bufSize == 0)
  {
    _unlink(fName);
    return(NULL);
  }

  ptr = (UCHAR *)malloc( (size_t) bufSize);
  if (!ptr)
  {
    _unlink(fName);
    return(NULL);
  }

  HFILE hFile = _lopen(fName, OF_READ);
  if (hFile == HFILE_ERROR)
  {
    _unlink(fName);
    free(ptr);
    return(NULL);
  }

  UINT numRead = _lread(hFile, ptr, (size_t) bufSize);
  _lclose(hFile);

  if (numRead != bufSize)
  {
    _unlink(fName);
    free(ptr);
    return(NULL);
  }

  _unlink(fName);
	return(ptr);	
}

LONG
WriteMemoryBufferToDisk(LPSTR fName, LONG bufSize, LPSTR buf)
{
  if (!buf)
  {
    return(-1);
  }

  HFILE hFile = _lcreat(fName, 0);
  if (hFile == HFILE_ERROR)
  {
    return(-1);
  }

  LONG writeCount = _lwrite(hFile, buf, (size_t) bufSize);
  _lclose(hFile);

  if (writeCount != bufSize)
  {
    _unlink(fName);
    return(-1);
  }

  return(0);
}

LPSTR
GetTheTempDirectoryOnTheSystem(void)
{
 static UCHAR retPath[_MAX_PATH];
  
  if (getenv("TEMP"))
  {
    lstrcpy((LPSTR) retPath, getenv("TEMP"));  // environmental variable
  } 
  else if (getenv("TMP"))
  {
    lstrcpy((LPSTR) retPath, getenv("TMP"));  // How about this environmental variable?
  }
  else
  {
    GetWindowsDirectory((LPSTR) retPath, sizeof(retPath));
  }

  return((LPSTR) &(retPath[0]));
}

#ifdef WIN16
int WINAPI EXPORT ISGetTempFileName(LPCSTR a_pDummyPath,  LPCSTR a_pPrefix, UINT a_uUnique, LPSTR a_pResultName)
{
#ifdef GetTempFileName	// we need the real thing comming up next...
#undef GetTempFileName
#endif
	return GetTempFileName(0, a_pPrefix, a_uUnique, a_pResultName);
}
#endif

LONG
GetTempAttachmentName(LPSTR fName) 
{
  UINT        res;
  static UINT uUnique = 1;

  if (!fName)
    return(-1);

  LPSTR szTempPath = GetTheTempDirectoryOnTheSystem();

TRYAGAIN:
#ifdef WIN32
  res = GetTempFileName(szTempPath, "NAB", uUnique++, fName);
#else
  res = ISGetTempFileName(szTempPath, "NAB", uUnique++, fName);
#endif

  if (ValidateFile(fName) != 1) 
  {
    if (uUnique < 32000)
    {
      goto TRYAGAIN;
    }
    else
    {
      return(-1);
    }
  }

  return 0;
}

void *
CleanMalloc(size_t mallocSize)
{
  void *ptr = malloc(mallocSize);
  if (!ptr)
    return(NULL);

  memset(ptr, 0, mallocSize);
  return(ptr);
}

void
SafeFree(void *ptr)
{
  if (!ptr)
    return;

  free(ptr);
  ptr = NULL;
}

void
FlushIt(void)
{
  /* get the next message, if any */
  MSG   msg;
  BOOL  ret;
  
  /* if we got one, process it */
  while ((ret = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) == TRUE)
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

static char szNABSection[] = "Software\\Netscape\\Netscape Navigator\\NABAPI";

DWORD 
GetPauseTime(LPSTR attrib)
{  
  DWORD val = 0;

  if (!GetConfigInfoNum(szNABSection, attrib, &val, HKEY_LOCAL_MACHINE))
  {
    if (lstrcmp(attrib, STARTUP) == 0)
      val = STARTUP_TIME;
    else if (lstrcmp(attrib, EXIT) == 0)
      val = EXIT_TIME;
    else
      val = STARTUP_TIME;
  }

  return val;
}

#define EXITING -10  //multi-instance code return values
#define RUNNING  10

const UINT NEAR msg_IPCStatus = RegisterWindowMessage("NetscapeIPCMsg");
const UINT NEAR msg_ExitStatus = RegisterWindowMessage("ExitingNetscape");

HWND
GetTheValidWindow(void)
{
  HWND  hWnd = NULL;

  while ((hWnd = FindWindow("aHiddenFrameClass", NULL)) != NULL)
  {
    if (SendMessage(hWnd, msg_IPCStatus, 0, 0) == 19)
    {
      if (SendMessage(hWnd, msg_ExitStatus, 0, 0) == EXITING)
        return NULL;
      else
        break;
    }

    FlushIt();
    Sleep(200);
  }

  return hWnd;
}

//
// Find Communicator and return an HWND, if not, start Communicator,
// then find an HWND
//
HWND 
GetCommunicatorIPCWindow(BOOL justBail)
{
  HWND  hWnd = NULL;
  DWORD timeCount = 0;

  hWnd = GetTheValidWindow();
  if (hWnd != NULL)
    return hWnd;
    
  if (justBail)
    return NULL;

  char    szPath[_MAX_PATH] = "";
  DWORD   nMAPIERROR; 
    
  if ((nMAPIERROR = XP_GetInstallLocation(szPath, _MAX_PATH)) != NAB_SUCCESS)
    return(NULL); 

  TRACE("STARTING COMMUNICATOR\n");  
  if (!XP_CallProcess(szPath, " -NABAPI"))
  { 
    TRACE("FAILED TO COMMUNICATOR\n");  
    return(NULL);
  }

  DWORD waitTime = GetPauseTime(STARTUP);

  TRACE("WAIT FOR COMMUNICATOR TO START UP!\n");  
  TRACE("WAITING %d SECONDS FOR COMMUNICATOR\n", waitTime);
  while ( (Is_16_OR_32_BIT_CommunitorRunning() == 0) && 
                        (timeCount < (ONE_SEC * waitTime) ) )
  {
#ifdef WIN32
    FlushIt();
    Sleep(50);
#else 
    Win16Sleep(50);
#endif    
    timeCount++;
  }

  return(GetTheValidWindow());
}
