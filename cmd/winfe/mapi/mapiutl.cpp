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
//  Various utils needed for the MAPI functions
//  Written by: Rich Pizzarro (rhp@netscape.com)
//  November 1997
//
#include <windows.h> 
#include <time.h> 
#include <sys/stat.h>
#include <io.h>
#include "xpapi.h"
#include "trace.h"
#include "mapiipc.h"
#include "mapiutl.h"
#include "resource.h"

#define     ONE_SEC       20
#define     STARTUP       "Start"
#define     STARTUP_TIME  20
#define     EXIT          "Exit"
#define     EXIT_TIME     10

//
// Global variables
//
BOOL	  gLoggingEnabled = FALSE;

DWORD 
GetPauseTime(LPSTR attrib)
{  
  DWORD val = 0;

  if (!GetConfigInfoNum(szMapiSection, attrib, &val, HKEY_LOCAL_MACHINE))
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

#ifdef WIN16

void
Win16Sleep(DWORD sleepTime)
{
  MSG   msg;
  BOOL  ret; 
  DWORD startTick = GetTickCount();

  while ( (GetTickCount() - startTick) < sleepTime )
  {
    ret = (BOOL)PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
    if (ret)
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else
    {
      Yield();
    }
  }
}

#endif

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
		lstrcat(tempPath, szMapiLog);
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

  wsprintf(name, "MAPI_IPC_SMEM-%d", (winSeed + id++));
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

//
// return of zero is ok 
// 1 = MAPI_E_ATTACHMENT_NOT_FOUND
// 2 = MAPI_E_ATTACHMENT_OPEN_FAILURE
//
DWORD
SanityCheckAttachmentFiles(lpMapiMessage lpMessage)
{
  ULONG i;
  DWORD rc;

  for (i=0; i<lpMessage->nFileCount; i++)
  {
    if ((rc = ValidateFile(lpMessage->lpFiles[i].lpszPathName)) != 0)
    {
      return(rc);
    }
  }

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

//
// Extract a filename from a string
// Return TRUE if file found, else FALSE
//
BOOL
ExtractFile(LPSTR pFiles, LPSTR delimChar, DWORD fIndex, LPSTR fName)
{
  LPSTR   ptr = pFiles;
  DWORD   loc;
  DWORD   count = 0;

	if ((!pFiles) || (!*pFiles))
		return(0);

  // Get to the fIndex'th entry
  for (loc=0; loc<strlen(pFiles); loc++)
  {
    if (count == fIndex)
      break;
    if (pFiles[loc] == delimChar[0])
      count++;
  }

  if (loc >= strlen(pFiles))    // Got to the end of string!
    return(FALSE);

  lstrcpy(fName, (LPSTR)pFiles + loc);
  //
  // Truncate at 2nd delimiter
  //
  for (DWORD i=0; i<strlen(fName); i++)
  {
    if (fName[i] == delimChar[0])
    {
      fName[i] = '\0';
      break;
    }
  }

  return(TRUE);
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
  res = GetTempFileName(szTempPath, "MAPI", uUnique++, fName);
#else
  res = ISGetTempFileName(szTempPath, "MAPI", uUnique++, fName);
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

// RICHIE - strip all of the HTML stuff out of the message...
int
CheckForInlineHTML(char *noteBody, DWORD len, DWORD *curPos, char *newBody, DWORD *realLen)
{
  LPSTR   tags[] = {"&nbsp;", "&lt;", "&amp;", NULL};
  UCHAR   tagsSubst[] = {' ', '<', '&', NULL};
  int     x = 0;

  while (tags[x])
  {
    // should we check for first tag
    if ( (*curPos+strlen(tags[x])) < len)
    {
      if (strncmp(tags[x], noteBody, strlen(tags[x])) == 0)
      {
        *curPos += strlen(tags[x]) - 1;
        newBody[*realLen] = tagsSubst[x];
        *realLen += 1;
        return(-1);
      }
    }

    ++x;
  }

  return(0);
}

//
// RICHIE - This is also temporary fix for now...
//
LPSTR
StripSignedMessage(LPSTR noteText, DWORD totalCR) 
{
char    *newBuf;
LPSTR   startTag = "<HTML>";
LPSTR   endTag = "/HTML>";
DWORD   i;
DWORD   realLen = 0;
DWORD   startPos = 0;
DWORD   len = strlen(noteText);;

  // create a new buffer...
  newBuf = (char *) malloc((size_t)(len + totalCR));
  if (!newBuf)
    return(noteText);

  newBuf[0] = '\0';

  // First, find the start of the HTML for the message...
  for (i=0; i<len; i++)
  {
    // should we check for first tag
    if ( (i+strlen(startTag)) < len)
    {
      if (strncmp(startTag, (noteText + i), strlen(startTag)) == 0)
      {
        startPos = i + strlen(startTag);
        break;
      }
    }
  }

  // Didn't find any HTML start tag
  if (i == len)
    return(noteText);

  BOOL    inHTML = FALSE;
  BOOL    firstChar = FALSE;

  for (i=startPos; i<len; i++)
  {
    char  *ptr = (noteText + i);

    if ( ((*ptr == 0x0D) || (*ptr == 0x20)) && (!firstChar) )
      continue;
    else
      firstChar = TRUE;

    // First, check for the end /HTML> tag
    if ( (i+strlen(endTag)) < len)
    {
      if (strncmp(endTag, ptr, strlen(endTag)) == 0)
      {
        break;
      }
    }

    // If we are in HTML, check for a ">"...
    if (inHTML)
    {
      if (*ptr == '>')
      {
        inHTML = FALSE;
      }
      continue;
    }

    // Check for NEW HTML...
    if (*ptr == '<')
    {
      inHTML = TRUE;
      continue;
    }

    if (CheckForInlineHTML(ptr, len, &i, newBuf, &realLen))
      continue;

    newBuf[realLen++] = *ptr;
    // Tack on a line feed if we hit a CR...
    if ( *ptr == 0x0D )
    {
      newBuf[realLen++] = 0x0A;
    }
  }

  // terminate the buffer - reallocate and move on...
  newBuf[realLen++] = '\0';
  newBuf = (LPSTR) realloc(newBuf, (size_t) realLen);

  // check if the realloc worked and if so, free old memory and
  // return...if not, just return the original buffer
  if (!newBuf)
  {
    return(noteText);
  }
  else
  {
    free(noteText);
    return(newBuf);
  }
}

//
// RICHIE - this is a temporary fix for now to get rid of
// html stuff within the text of a message - if there was a
// valid noteText buffer coming into this call, we need to 
// free it on the way out.
//
LPSTR
StripHTML(LPSTR noteText)
{
  char    *newBuf;
  LPSTR   signTag = "This is a cryptographically signed message in MIME format.";
  LPSTR   mimeTag = "This is a multi-part message in MIME format.";

  DWORD   i;
  DWORD   realLen = 0;
  DWORD   totalCR = 0;

  // do sanity checking...
  if ((!noteText) || (!(*noteText)))
    return(noteText);

  // more sanity checking...
  DWORD len = strlen(noteText) + 1;
  if (len <= 0) 
    return(noteText);

  // Get the number of CR's in this message and add room for
  // the LF's
  for (i=0; i<len; i++)
  {
    if ( (*(noteText + i)) == 0x0D )
      ++totalCR;
  }

  // This is a check for a signed message in the start of a message
  // check for sign line...    
  if ( strlen(signTag) < len)
  {
    if ( 
        (strncmp(signTag, noteText, strlen(signTag)) == 0) ||
        (strncmp(mimeTag, noteText, strlen(mimeTag)) == 0) 
       )
    {
      return( StripSignedMessage(noteText, totalCR) );
    }
  }

  // create a new buffer...
  newBuf = (char *) malloc((size_t)(len + totalCR));
  if (!newBuf)
    return(noteText);

  newBuf[0] = '\0';

  BOOL    firstChar = FALSE;

  // Now do the translation for the body of the note...
  for (i=0; i<len; i++)
  {
    char  *ptr = (noteText + i);
    
    if ( ((*ptr == 0x0D) || (*ptr == 0x20)) && (!firstChar) )
      continue;
    else
      firstChar = TRUE;

    if (CheckForInlineHTML(ptr, len, &i, newBuf, &realLen))
      continue;

    newBuf[realLen++] = *ptr;
    if ( *ptr == 0x0D )
    {
      newBuf[realLen++] = 0x0A;
    }
  }

  // terminate the buffer - reallocate and move on...
  newBuf[realLen++] = '\0';
  newBuf = (LPSTR) realloc(newBuf, (size_t) realLen);

  // check if the realloc worked and if so, free old memory and
  // return...if not, just return the original buffer
  if (!newBuf)
  {
    return(noteText);
  }
  else
  {
    free(noteText);
    return(newBuf);
  }
}

UINT
GetTempMailNameWithExtensionORIGINAL(LPSTR szTempFileName, 
                             LPSTR origName)
{
  UINT        res;
  static UINT uUnique = 1;

  char *szTempPath = GetTheTempDirectoryOnTheSystem();

TRYAGAIN:

#ifdef WIN32
  res = GetTempFileName(szTempPath, "nsm", uUnique++, szTempFileName);
#else
  res = ISGetTempFileName(szTempPath, "nsm", uUnique++, szTempFileName);
#endif

  // Now add the correct extension...
  char *origExt = strrchr(origName, '.');
  if (origExt != NULL)
  {
    char *tmpExt = strrchr(szTempFileName, '.');
    if (tmpExt != NULL)
    {
      origExt++;
      tmpExt++;

      while ( ((tmpExt) && (origExt)) && (*origExt != '\0') )
      {
        *tmpExt = *origExt;
        tmpExt++;
        origExt++;
      }

      *tmpExt = '\0';
    }
  }

  if ((!ValidateFile(szTempFileName)) && (uUnique < 32000))
  {
    goto TRYAGAIN;
  }

  return res;
}

#define MAXTRY  9999  // How many times do we try..

UINT
GetTempMailNameWithExtension(LPSTR szTempFileName, 
                             LPSTR origName)
{
  UINT        res = 1;
  UINT        uUnique = 0;
  char        *szTempPath = GetTheTempDirectoryOnTheSystem();
  char        *tmpPtr;
  char        *realFileName = NULL;

  if ( (origName != NULL) && (*origName != '\0') )
  {
    tmpPtr = origName;
  }
  else
  {
    tmpPtr = szTempFileName;
  }

  realFileName = strrchr(tmpPtr, '\\');
  if (!realFileName)
    realFileName = tmpPtr;
  else
    realFileName++;

TRYAGAIN:

#ifdef WIN32
  if (uUnique == 0)
  {
    wsprintf(szTempFileName, "%s\\%s", szTempPath, realFileName);
  }
  else
  {
    wsprintf(szTempFileName, "%s\\%d_%s", 
                szTempPath, uUnique, realFileName);
  }
#else // WIN16
  if ( (uUnique == 0) && (strlen(realFileName) <= 12) )
  {
    wsprintf(szTempFileName, "%s\\%s", szTempPath, realFileName);
  }
  else
  {
    res = ISGetTempFileName(szTempPath, "ns", uUnique++, szTempFileName);    

    // Now add the correct extension...
    char *origExt = strrchr(realFileName, '.');
    if (origExt != NULL)
    {
      char *tmpExt = strrchr(szTempFileName, '.');
      if (tmpExt != NULL)
      {
        origExt++;
        tmpExt++;

        while ( ((tmpExt) && (origExt)) && (*origExt != '\0') )
        {
          *tmpExt = *origExt;
          tmpExt++;
          origExt++;
        }

        *tmpExt = '\0';
      }
    }
  }
#endif

  if ((!ValidateFile(szTempFileName)) && (uUnique < MAXTRY))
  {
    uUnique++;
    if (uUnique >= MAXTRY)
      return(1);
    goto TRYAGAIN;
  }

  return res;
}

#define kMaxTempFiles   10
#define kMaxListLength  (10 * _MAX_PATH)

void GetTempFiles(LPSTR pBuf, int lenBuf)
{
  if (!GetConfigInfoStr(szMapiSection, szTempFiles, pBuf, lenBuf, HKEY_CURRENT_USER))
  {
    *pBuf = 0;
  }
}

void WriteTempFiles(LPSTR pBuf)
{
  SetConfigInfoStr(szMapiSection, szTempFiles, pBuf, HKEY_CURRENT_USER);
  /* HKEY_ROOT);  - permission problem */
}

void  AddTempFile(LPCSTR pFileName)
{
  char files[kMaxListLength];

  GetTempFiles(files, sizeof(files));
  if ((lstrlen(files) + lstrlen(pFileName) + 2) >= sizeof(files))
  {
          return;
  }
  if (lstrlen(files) != 0)
  {
          lstrcat(files, ";");
  }
  lstrcat(files, pFileName);
  WriteTempFiles(files);
}

void DeleteFirstTempFile(LPSTR pFiles)
{
  if (!*pFiles)
    return;
  LPSTR pTemp = strchr(pFiles, ';');
  if (pTemp)
  {
    *pTemp = 0;
  }
  
  _unlink(pFiles);
  if (pTemp)
  {
    memmove(pFiles, pTemp + 1, lstrlen(pTemp + 1) + 1);
  }
  else
  {
    *pFiles = 0;
  }
}

void 
RemoveAllTempFiles(void)
{
  char files[kMaxListLength];
  GetTempFiles(files, sizeof(files));

  while (*files)
  {
          DeleteFirstTempFile(files);
  }
  WriteTempFiles(files);
}

void CheckAgeTempFiles(void)
{
  char files[kMaxListLength];
  GetTempFiles(files, sizeof(files));
  int i = 0;
  LPSTR pTemp = files;
  while (TRUE)
  {
          pTemp = strchr(pTemp, ';');
          if (!pTemp)
                  break;
          ++pTemp;
          ++i;
  }
  if (i >= 4)
  {
          DeleteFirstTempFile(files);
          WriteTempFiles(files);
  }
}

// Need this to make sure that we can kill all temp files...
// Enum all windows and check title for "Composition"...
BOOL    foundItRunning = FALSE;

BOOL CALLBACK 
EnumWindowsProc(HWND   hwnd,	// handle to parent window
                LPARAM lParam) 	// application-defined value
{
  UINT          msg = RegisterWindowMessage(MAPI_CUSTOM_COMPOSE_MSG);

  // Need to do this to avoid SendMessage() causing recursion
  DWORD otherID;
  DWORD myID = GetCurrentProcessId();
  (void) GetWindowThreadProcessId(hwnd, &otherID);

  if (myID == otherID)
    return TRUE;
  
  LONG  rc = SendMessage(hwnd, msg, 0, 0);
  
  if (rc == MAPI_CUSTOM_RET_CODE)
  {
    foundItRunning = TRUE;
    return FALSE;
  }
  
  return TRUE;
}

BOOL
CompositionWindowRunning(void)
{
  foundItRunning = FALSE;
  if (Is_16_OR_32_BIT_CommunitorRunning() == 0)
    return FALSE;

  (void) EnumWindows(EnumWindowsProc, NULL);
  return (foundItRunning);
}

void 
CleanupMAPITempFiles(void)
{
  if (!CompositionWindowRunning())
  {
    RemoveAllTempFiles();           // if Communicator not running, clean up all the temp files
  }
  else
  {
    CheckAgeTempFiles();
  }
}

#define EXITING -10  //multi-instance code return values
#define RUNNING  10

const UINT NEAR msg_IPCStatus = RegisterWindowMessage("NetscapeIPCMsg");
const UINT NEAR msg_ExitStatus = RegisterWindowMessage("ExitingNetscape");

HWND
GetTheValidWindow(void)
{
  HWND  hWnd = NULL;
  DWORD waitTime = GetPauseTime(STARTUP);
  DWORD timeCount = 0;

  while ( ((hWnd = FindWindow("aHiddenFrameClass", NULL)) != NULL) &&
                        (timeCount < ( (ONE_SEC * waitTime) / 2) ) )
  {
    if (SendMessage(hWnd, msg_IPCStatus, 0, 0) == 19)
    {
      if (SendMessage(hWnd, msg_ExitStatus, 0, 0) == EXITING)
        return NULL;
      else
      {
        return hWnd;
      }
    }
    else  
    {
      timeCount++;
    }

    FlushIt();
    Sleep(50);
  }

  return NULL;
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
    
  if ((nMAPIERROR = XP_GetInstallLocation(szPath, _MAX_PATH)) != SUCCESS_SUCCESS)
    return(NULL); 

  TRACE("STARTING COMMUNICATOR\n");  
  if (!XP_CallProcess(szPath, " -MAPICLIENT"))
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
