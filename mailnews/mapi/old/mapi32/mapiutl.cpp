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

//
// Find Communicator and return an HWND, if not, start Communicator,
// then find an HWND
//
HWND 
GetCommunicatorIPCWindow(void)
{
  HWND  hWnd = NULL; 
  DWORD timeCount = 0;
  BOOL  launchTry = FALSE;

  //
  // This will wait for 10 seconds before giving up and failing
  //
  while ((hWnd == NULL) && (timeCount < 20))
  {
    if ((hWnd = FindWindow("AfxFrameOrView", NULL)) && !FindWindow("aHiddenFrameClass", NULL))
      return(hWnd);
    else if ((hWnd = FindWindow("aHiddenFrameClass", NULL)))
      return(hWnd);

    if (!launchTry)
    {
    char    szPath[_MAX_PATH] = "";
		DWORD   nMAPIERROR; 

      if ((nMAPIERROR = XP_GetInstallLocation(szPath, _MAX_PATH)) != SUCCESS_SUCCESS)
      {
        return(NULL); 
      }
		
		  WORD nReturn = XP_CallProcess(szPath, " -MAPICLIENT");
      launchTry = TRUE;
    }

    //
    // Pause for 1/2 a second and try to connect again...
    //
#ifdef WIN32
    Sleep(500);
#else 
    Yield();
#endif        

    timeCount++;
  }

	return(hWnd);
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

#ifdef WIN16

void
GetWin16TempName(LPSTR realFileName, LPSTR tempPath, 
				 LPSTR szTempFileName, UINT uUnique)
{
  char	 *dotPtr = strrchr(realFileName, '.');
  if (dotPtr != NULL)
  {
    *dotPtr = '\0';
  }                
  
  int nameLen = lstrlen(realFileName); 
  if (dotPtr != NULL)
  {
    *dotPtr = '.';
  }
  
  if (nameLen <= 7)
  {
 	wsprintf(szTempFileName, "%s\\%d%s", tempPath, uUnique, realFileName);
  }
  else
  {
  	wsprintf(szTempFileName, "%s\\%d%s", tempPath, uUnique, (realFileName + 1));
  }
}

#endif

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
    if (uUnique < 10)
    {
      GetWin16TempName(realFileName, szTempPath, szTempFileName, uUnique);
    }
    else 
    { 
      res = ISGetTempFileName(szTempPath, "ns", uUnique++, szTempFileName);    
    }

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

  if ( (ValidateFile(szTempFileName) != 1) && (uUnique < MAXTRY) )
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
  if (!GetConfigInfoStr(szMapiSection, szTempFiles, pBuf, lenBuf, HKEY_ROOT))
  {
    *pBuf = 0;
  }
}

void WriteTempFiles(LPSTR pBuf)
{
  SetConfigInfoStr(szMapiSection, szTempFiles, pBuf, HKEY_ROOT);
}

void  AddTempFile(LPCSTR pFileName)
{
  if ( (!pFileName) || (pFileName[0] == '\0') )
    return;

  char *files = (char *)malloc(kMaxListLength);

  if (!files)
    return;

  GetTempFiles(files, kMaxListLength);
  if ((lstrlen(files) + lstrlen(pFileName) + 2) >= kMaxListLength)
  {
    free(files);
    return;
  }

  if (lstrlen(files) != 0)
  {
          lstrcat(files, ";");
  }

  lstrcat(files, pFileName);
  WriteTempFiles(files);
  free(files);
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
 
//#ifndef _DEBUG  
  _unlink(pFiles);
//#endif

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
  char *files = (char *)malloc(kMaxListLength);

  if (!files)
    return;

  GetTempFiles(files, kMaxListLength);

  while (*files)
  {
    DeleteFirstTempFile(files);
  }

  WriteTempFiles(files);
  free(files);
}

void CheckAgeTempFiles(void)
{
  char *files = (char *)malloc(kMaxListLength);

  if (!files)
    return;

  GetTempFiles(files, kMaxListLength);
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

  if (i >= 10)
  {
    DeleteFirstTempFile(files);
    WriteTempFiles(files);
  }

  free(files);
}

void 
CleanupMAPITempFiles(void)
{
  if (Is_16_OR_32_BIT_CommunitorRunning() == 0)
  {
    RemoveAllTempFiles();           // if Communicator not running, clean up all the temp files
  }
  else
  {
    CheckAgeTempFiles();
  }
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
