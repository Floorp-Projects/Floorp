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

#include "res\appicon.h"
#include "dialog.h"
#include "mainfrm.h"
#include "custom.h"
#include "shcut.h"
#include "edt.h"
#include "prmon.h"
#include "fegui.h"
#include "prefapi.h"
#include <io.h>
#include "secrng.h"
#include "mailmisc.h"
#include "ipframe.h"
#include "mnprefs.h"
#include "secnav.h"
#include "edprops.h"

//xpstrsw header is for resource switcher that will hold the proper instance handle for each stringloaded
#include "xpstrsw.h"

#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif
#ifdef __BORLANDC__
	#define _lseek lseek
#endif
       
MODULE_PRIVATE char * XP_CacheFileName();

static HPALETTE ghPalette = NULL;
static CMapWordToPtr gBitmapMap;
static HFONT ghFont = NULL;

// Load a string, stored as RCDATA, from the resource file. The
// caller must free the pointer that is returned
char *
wfe_LoadResourceString (LPCSTR lpszName)
{
	HRSRC	hrsrc;
	HGLOBAL	hRes;
	LPSTR	lpszString;

	hrsrc = ::FindResource(AfxGetResourceHandle(), lpszName, RT_RCDATA);

	if (!hrsrc) {
		TRACE1("wfe_LoadResourceString cannot find resource: %s\n", lpszName);
		return NULL;
	}

	hRes = ::LoadResource(AfxGetResourceHandle(), hrsrc);

	if (!hRes) {
		TRACE1("wfe_LoadResourceString cannot load resource: %s\n", lpszName);
		return NULL;		
	}

	lpszString = (LPSTR)::LockResource(hRes);

	if (!lpszString) {
		TRACE1("wfe_LoadResourceString cannot lock resource: %s\n", lpszName);
		::FreeResource(hRes);
		return NULL;
	}

	// Return a copy of the string
	lpszString = XP_STRDUP(lpszString);

#ifndef XP_WIN32	
	::UnlockResource(hRes);
#endif
	::FreeResource(hRes);
	return lpszString;
}

#define MAXSTRINGLEN 8192 
// Load a string from resource table
//Added pSwitcher to allow us to dictate which HINSTANCE to load the string from.  if NULL, we use AfxGetResourceHandle
//mjudge 10-30-97
char * szLoadString( UINT iID , ResourceSwitcher *pSwitcher/*=NULL*/)
{
    // These strings MUST be static so they don't live on the stack.  Since we
    // will be returning a pointer to one of these strings, we want the info
    // to be there once we exit this function
    // This implements a circular buffer: its size depends on how many times the
    // szLoadString function will be called in a single function call.  For
    // example, if you do this:
    //
    //   MessageBox ( NULL, 
    //                szLoadString(IDS_ERROR1), 
    //                szLoadString(IDS_MSGBOXTITLE), 
    //                MB_OK );
    //
    // We need at least 2 strings here.  We should be safe with four.

    static int iWhichString = 0;
    static char *szLoadedStrings[4] = { NULL, NULL, NULL, NULL };

	if ( !szLoadedStrings[iWhichString] ) {
		szLoadedStrings[iWhichString] = new char[MAXSTRINGLEN];
	}

    // This implements a circular buffer using the four strings. This means that
    // the pointer you get back from szLoadString will have the correct string in
    // it until szLoadString has been called four more times.

	char *szLoadedString = szLoadedStrings[iWhichString];
    iWhichString = (iWhichString + 1) % 4;

    szLoadedString[0] = 0;  // Init the string to emptyville.

	// This allows loading of strings greater than 255 characters. If the
	// first character of the string is an exclamation point, this means to
	// tack on the string with a value of ID+1.  This continues until
	// the next string does not exist or the first character is not an
	// exclamation point

	// These are just normal local variables used for looping and return values
	// and string magic
	char szLoadBuffer[256];

	BOOL bKeepLoading;
	HINSTANCE hResourceHandle;
	if (NULL== pSwitcher)
		hResourceHandle=AfxGetResourceHandle();
	else
		hResourceHandle=pSwitcher->GetResourceHandle();
	do {
		bKeepLoading = FALSE;
		if (LoadString ( hResourceHandle, iID, szLoadBuffer, sizeof(szLoadBuffer))) {


			if ('!' == *szLoadBuffer) {
				// Continuation char found. Keep the loop going
				bKeepLoading = TRUE;
				// Assume next entry in string table is in numerical order
				iID++;
				strcat ( szLoadedString, szLoadBuffer + 1 );
			} else {
				strcat ( szLoadedString, szLoadBuffer );
			}
		}
	} while (bKeepLoading);

	return szLoadedString;
}

#ifdef XP_WIN16
#define INVALID_HANDLE_VALUE NULL 

//  16 Bit FindFirstFile, second parameter void *b must be of type struct
//   _find_t *.  Return value if successful will just be this same
//   structure, otherwise INVALID_HANDLE_VALUE.

PRIVATE char *cp_filesystems = "\\*.*";

//  See _dos_findfirst on how to read the structure _find_t members.
void * 
FindFirstFile(const char * a, void * b) 
{
  //  First off, our void * coming in is actually a dirStruct.
  auto dirStruct *dSp = (dirStruct *)b;
  
  //  See if the dir we're looking at is a root of a file system; string indicates this.
  //  If so, we have to hack a way to provide drive letters and such in the listing.
  if(XP_STRCMP(a, cp_filesystems) == 0) {
    dSp->c_checkdrive = 'A';
  }
  else  {
    //  +2 because +1 means to return false in find next.
    dSp->c_checkdrive = 'Z' + 2;
  }
  
  //  If we're going after the root, then we have to loop through all the drives until we find one....  Ugh.
  //  The way to tell if we're currently looking for file systems, is if c_checkdrive is a valid drive letter.
  if(dSp->c_checkdrive == 'A')  {
    //  Save the drive we're on currently.
    auto int i_current = _getdrive();
    
    while(dSp->c_checkdrive <= 'Z') {
      //  If we can change to a drive, then it is valid.
      //  I would think that there is a better way to do this, but all the examples that I've seen do this
      //    exact thing.
      if(_chdrive(dSp->c_checkdrive - 'A' + 1) == 0)  {
        break;
      }
      
      //  Drive we checked wasn't valid.  Go on to the next one.
      dSp->c_checkdrive++;
    }
    
    //  Restore the original drive.
    _chdrive(i_current);
    
    //  If we found a valid drive, we need to dup it's name in the findfirst stuff to pass it back in the name field.
    //  Also, increment on to the next drive.
    //  Members of the _find_t data structure aren't set up correctly....  They don't seem to be referenced anywhere,
    //    so I'm not going to bother.
    if(dSp->c_checkdrive <= 'Z')  {
      dSp->file_data.name[0] = dSp->c_checkdrive;
      dSp->file_data.name[1] = '|';
      dSp->file_data.name[2] = '\0';
      
      dSp->c_checkdrive++;
      
      //  Just return b, as it is the void dSp, and we will soon be seeing it again in findnext....
      return(b);
    }
    
    //  If we found no valid drives, then we need to return an invalid state.
    return(INVALID_HANDLE_VALUE);
  }

  //  Specify what type of files you'd like to find.  See _dos_findfirst.
  auto unsigned u_finding = _A_NORMAL | _A_RDONLY | _A_ARCH | _A_SUBDIR;
  
  if(_dos_findfirst(a, u_finding, &(dSp->file_data)) == 0)  {
    //  Success, just return the structure containing the info.
    //  b should not be altered!  Only read information from the
    //  structure.

    return(b);
  }
  
  return(INVALID_HANDLE_VALUE);
}

int 
FindNextFile(void * a, void * b)
{
  //  a and b are actually the same.
  //  Check to see if we are still searching drives from findfirst.
  auto dirStruct *dSp = (dirStruct *)a;
  
  if(dSp->c_checkdrive <= 'Z')  {
    //  Save the drive we're on currently.
    auto int i_current = _getdrive();
    
    while(dSp->c_checkdrive <= 'Z') {
      //  If we can change to a drive, then it is valid.
      if(_chdrive(dSp->c_checkdrive - 'A' + 1) == 0)  {
        break;
      }
      
      //  Drive we checked wasn't valid.  Go on to the next one.
      dSp->c_checkdrive++;
    }
    
    //  Restore the original drive.
    _chdrive(i_current);
    
    //  If we found a valid drive, we need to dup it's name in the findfirst stuff to pass it back in the name field.
    //  Also, increment on to the next drive.
    //  Members of the _find_t data structure aren't set up correctly....  They don't seem to be referenced anywhere,
    //    so I'm not going to bother.
    if(dSp->c_checkdrive <= 'Z')  {
      dSp->file_data.name[0] = dSp->c_checkdrive;
      dSp->file_data.name[1] = '|';
      dSp->file_data.name[2] = '\0';
      
      dSp->c_checkdrive++;
      
      //  Just return as findnext
      return(TRUE);
    }
  }
  
  if(dSp->c_checkdrive == 'Z' + 1)  {
    //  Special case where we will stop looking for file systems to list.  Return as though we failed.
    return(FALSE);
  }

  //  Just call _dos_findnext with a, since a and b are actually the
  //  same.  Return true if successful.
  if(_dos_findnext(&(dSp->file_data)) == 0) {
    return(TRUE);
  }
  return(FALSE);
} 

void 
FindClose(void * a)
{
  //  Do nothing.  It's up to the caller to get rid of *a since we
  //  created nothing.
}

#endif

//
// Return a string the same as the In string but with all of the \n's replaced
//  with \r\n's 
//
MODULE_PRIVATE char * 
FE_Windowsify(const char * In)
{
    char *Out;
    char *o, *i; 
    int32 len;

    if(!In)
        return NULL;

    // if every character is a \n then new string is twice as long
    len = (int32) XP_STRLEN(In) * 2 + 1;
    Out = (char *) XP_ALLOC(len);
    if(!Out)
        return NULL;

  // Move the characters over one by one.  If the current character is
  //  a \n replace add a \r before moving the \n over
    for(i = (char *) In, o = Out; i && *i; *o++ = *i++)
       if(*i == '\n')
            *o++ = '\r';

    // Make sure our new string is NULL terminated
    *o = '\0';
    return(Out);
}

// middle cuts string into a string of iNum length.  If bModifyString is
// FALSE (the default), a copy is made which the caller must free
PUBLIC char * fe_MiddleCutString(char *pString ,int iNum, BOOL bModifyString)
{
	ASSERT(pString);
	if (!pString)
		return NULL;

	// If the caller wants us to allocate a new string, just dup the string
	// that was passed in since the result can't be any longer
	if (!bModifyString) {
		pString = strdup(pString);

		if (!pString) {
			TRACE0("fe_MiddleCutString: strdup failed!\n");
			return NULL;
		}
	}

	// Make sure the length to cut to is something sensible
	ASSERT(iNum > 3);
	if (iNum <= 3)
		return pString;  // don't bother...

	// We need to call i18n function for multibyte charset (e.g. Japanese SJIS)
	//
	int16 csid = CIntlWin::GetSystemLocaleCsid();
	if (csid & MULTIBYTE) {
		INTL_MidTruncateString (csid, pString, pString, iNum);
		return pString;
	}

	// See if there is any work to do
	int	nLen = strlen(pString);

	if (nLen > iNum) {
		int	nLeftChars = (iNum - 3) / 2;
		int	nRightChars = iNum - 3 - nLeftChars;

		// Insert the ellipsis
		pString[nLeftChars] = '.';
		pString[nLeftChars + 1] = '.';
		pString[nLeftChars + 2] = '.';

		// Use the right "nRightChars" characters
		strcpy(pString + nLeftChars + 3, pString + nLen - nRightChars);
	}

	return pString;
}

/* puts up a FE security dialog
 *
 * Should return TRUE if the url should continue to
 * be loaded
 * Should return FALSE if the url should be aborted
 */
PUBLIC Bool  
FE_SecurityDialog(MWContext * context, int message, XP_Bool *prefs_toggle)
{

    if (theApp.m_bKioskMode)
        return TRUE;

    if(context) {
		CDialogSecurity box(message, prefs_toggle, ABSTRACTCX(context)->GetDialogOwner());
	return(box.DoModal());
    } else {
        CDialogSecurity box(message, prefs_toggle, NULL);
	return(box.DoModal());
    }
}

// Save the cipher preference to the preferences file
// XXX - jsw - remove me
extern "C" void
FE_SetCipherPrefs(MWContext *context, char *cipher)
{
}

// Retrieve the cipger preferences. The caller is expected to free
// the returned string
// XXX - jsw - remove me
extern "C" char *
FE_GetCipherPrefs(void)
{
	return NULL;
}

//  Return some spanked out full path on the mac.
//  For windows, we just return what was passed in.
//  We must provide it in a seperate buffer, otherwise they might change
//      the original and change also what they believe to be saved.
extern "C"  char *WH_FilePlatformName(const char *pName)    {

    if(pName)   {
        return XP_STRDUP(pName);
    }
    
    return NULL;
}

/* char ch;				// Byte, not Character, to be located
 * const char* cpStr;	// Null-terminated string
 * uint16 iCSID;		// file system id or win_csid
 * jliu@09/08/97
 */
char* intl_strrchr( char* cpStr, char ch, uint16 iCSID )
{
	if( cpStr == NULL || *cpStr == NULL )
		return NULL;

	if( ( iCSID & MULTIBYTE ) == 0  || ( ch & 0x80 ) != 0 )
		return strrchr( cpStr, ch );

	char* cpLastChar = NULL;

	while( *cpStr != NULL ){
		if( *cpStr == ch )
			cpLastChar = cpStr++;
		else if( ( *cpStr & 0x80 ) == 0 )
			cpStr++;
		else
			cpStr = INTL_NextChar( iCSID, cpStr );
	}

	return cpLastChar;
}

/* const char* cpStr;	// Null-terminated string
 * uint16 iCSID;		// file system id or win_csid
 * jliu@09/08/97
 */
BOOL intl_IsEndWithBS( char* cpStr, uint16 iCSID )
{
	if( cpStr == NULL || *cpStr == NULL )
		return FALSE;

	int len = strlen( cpStr );	// how many bytes in cpStr
	if( cpStr[len-1] != '\\' )
		return FALSE;

	if( ( iCSID & MULTIBYTE ) == 0 )
		return TRUE;

	char* cpLastChar = NULL;
	while( *cpStr != NULL ){
		cpLastChar = cpStr;
		cpStr = INTL_NextChar( iCSID, cpStr );
	}

	return *cpLastChar == '\\';
}

extern "C" char *
FE_GetProgramDirectory(char *buffer, int length)
{
	::GetModuleFileName(theApp.m_hInstance, buffer, length);

	//	Find the trailing slash.
	//  char *pSlash = ::strrchr(buffer, '\\');
	char* pSlash = intl_strrchr( buffer, '\\', CIntlWin::GetSystemLocaleCsid() );
	if(pSlash)	{
		*(pSlash+1) = '\0';
	}
	else	{
		buffer[0] = '\0';
	}
    return (buffer);
}


char*
XP_TempDirName(void)
{
    char *tmp = theApp.m_pTempDir;
    if (!tmp)
        return XP_STRDUP(".");
    return XP_STRDUP(tmp);
}

// Windows _tempnam() lets the TMP environment variable override things sent in
//  so it look like we're going to have to make a temp name by hand
//
// The user should *NOT* free the returned string.  It is stored in static space
//  and so is not valid across multiple calls to this function
//
// The names generated look like
//   c:\netscape\cache\m0.moz
//   c:\netscape\cache\m1.moz
// up to...
//   c:\netscape\cache\m9999999.moz
// after that if fails
//
PUBLIC char *
xp_TempFileName(int type, const char * request_prefix, const char * extension,
				char* file_buf)
{
  const char * directory = NULL; 
  char * ext = NULL;           // file extension if any
  char * prefix = NULL;        // file prefix if any
  
  
  XP_StatStruct statinfo;
  int status;     

  //
  // based on the type of request determine what directory we should be
  //   looking into
  //  
  switch(type) {
    case xpCache:
        directory = theApp.m_pCacheDir;
        ext = ".MOZ";
        prefix = CACHE_PREFIX;
        break;
#ifdef MOZ_MAIL_NEWS        
	case xpSNewsRC:
	case xpNewsRC:
    case xpNewsgroups:
    case xpSNewsgroups:
    case xpTemporaryNewsRC:
        directory = g_MsgPrefs.m_csNewsDir;
        ext = (char *)extension;
        prefix = (char *)request_prefix;
        break;
	case xpMailFolderSummary:
	case xpMailFolder:
		directory = g_MsgPrefs.m_csMailDir;
        ext = (char *)extension;
        prefix = (char *)request_prefix;
		break;
	case xpAddrBook:
		//changed jonm-- to support multi-profile
		//directory = theApp.m_pInstallDir->GetCharValue();
		directory = (const char *)theApp.m_UserDirectory;
		if ((request_prefix == 0) || (XP_STRLEN (request_prefix) == 0))
			prefix = "abook";
		ext = ".nab";
		break;
#endif // MOZ_MAIL_NEWS
	case xpCacheFAT:
		directory = theApp.m_pCacheDir;
		prefix = "fat";
		ext = "db";
		break;
	case xpJPEGFile:
        directory = theApp.m_pTempDir;
		ext = ".jpg";
        prefix = (char *)request_prefix;
		break;
	case xpPKCS12File:
		directory = theApp.m_pTempDir;
		ext = ".p12";
		prefix = (char *)request_prefix;
		break;
    case xpURL:
      {
         if (request_prefix)
         {
            if ( XP_STRRCHR(request_prefix, '/') )
            {
               XP_StatStruct st;

               directory = (char *)request_prefix;
               if (XP_Stat (directory, &st, xpURL))
                  XP_MakeDirectoryR (directory, xpURL);
               ext = (char *)extension;
               prefix = (char *)"su";
               break;
            }
        }
        // otherwise, fall through
      }
    case xpTemporary:
    default:
        directory = theApp.m_pTempDir;
        ext = (char *)extension;
        prefix = (char *)request_prefix;
        break;
    }

  if(!directory)
    return(NULL);

  if(!prefix)
    prefix = "X";

  if(!ext)
    ext = ".TMP";

  //  We need to base our temporary file names on time, and not on sequential
  //    addition because of the cache not being updated when the user
  //    crashes and files that have been deleted are over written with
  //    other files; bad data.
  //  The 52 valid DOS file name characters are
  //    0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_^$~!#%&-{}@`'()
  //  We will only be using the first 32 of the choices.
  //
  //  Time name format will be M+++++++.MOZ
  //    Where M is the single letter prefix (can be longer....)
  //    Where +++++++ is the 7 character time representation (a full 8.3
  //      file name will be made).
  //    Where .MOZ is the file extension to be used.
  //
  //  In the event that the time requested is the same time as the last call
  //    to this function, then the current time is incremented by one,
  //    as is the last time called to facilitate unique file names.
  //  In the event that the invented file name already exists (can't
  //    really happen statistically unless the clock is messed up or we
  //    manually incremented the time), then the times are incremented
  //    until an open name can be found.
  //
  //  time_t (the time) has 32 bits, or 4,294,967,296 combinations.
  //  We will map the 32 bits into the 7 possible characters as follows:
  //    Starting with the lsb, sets of 5 bits (32 values) will be mapped
  //      over to the appropriate file name character, and then
  //      incremented to an approprate file name character.
  //    The left over 2 bits will be mapped into the seventh file name
  //      character.
  //
  
  int i_letter, i_timechars, i_numtries = 0;
  char ca_time[8];
  time_t this_call = (time_t)0;
  
  //  We have to base the length of our time string on the length
  //    of the incoming prefix....
  //
  i_timechars = 8 - strlen(prefix);
  
  //  Infinite loop until the conditions are satisfied.
  //  There is no danger, unless every possible file name is used.
  //
  while(1)  {
    //    We used to use the time to generate this.
    //    Now, we use some crypto to avoid bug #47027
    RNG_GenerateGlobalRandomBytes((void *)&this_call, sizeof(this_call));

    //  Convert the time into a 7 character string.
    //  Strip only the signifigant 5 bits.
    //  We'll want to reverse the string to make it look coherent
    //    in a directory of file names.
    //
    for(i_letter = 0; i_letter < i_timechars; i_letter++) {
      ca_time[i_letter] = (char)((this_call >> (i_letter * 5)) & 0x1F);
      
      //  Convert any numbers to their equivalent ascii code
      //
      if(ca_time[i_letter] <= 9)  {
        ca_time[i_letter] += '0';
      }
      //  Convert the character to it's equivalent ascii code
      //
      else  {
        ca_time[i_letter] += 'A' - 10;
      }
    }
    
    //  End the created time string.
    //
    ca_time[i_letter] = '\0';
    
    //  Reverse the time string.
    //
    _strrev(ca_time);
    
    //  Create the fully qualified path and file name.
    //
    sprintf(file_buf, "%s\\%s%s%s", directory, prefix, ca_time, ext);

    //  Determine if the file exists, and mark that we've tried yet
    //    another file name (mark to be used later).
    //  
	//  Use the system call instead of XP_Stat since we already
	//  know the name and we don't want recursion
	//
	status = _stat(file_buf, &statinfo);
    i_numtries++;
    
    //  If it does not exists, we are successful, return the name.
    //
    if(status == -1)  {
      /* don't generate a directory as part of the 
       * cache temp names.  When the cache file name
       * is passed into the other XP_File functions
       * we will append the cache directory to the name
       * to get the complete path.
       * This will allow the cache to be moved around
       * and for netscape to be used to generate external
       * cache FAT's.  :lou
       */
      if(type == xpCache )
          sprintf(file_buf, "%s%s%s", prefix, ca_time, ext);

//      TRACE("Temp file name is %s\n", file_buf);
      return(file_buf);
    }
    
    //  If there is no room for additional characters in the time,
    //    we'll have to return NULL here, or we go infinite.
    //    This is a one case scenario where the requested prefix is
    //    actually 8 letters long.
    //  Infinite loops could occur with a 7, 6, 5, etc character prefixes
    //    if available files are all eaten up (rare to impossible), in
    //    which case, we should check at some arbitrary frequency of
    //    tries before we give up instead of attempting to Vulcanize
    //    this code.  Live long and prosper.
    //
    if(i_timechars == 0)  {
      break;
    }
    else if(i_numtries == 0x00FF) {
      break;
    }
  }
  
  //  Requested name is thought to be impossible to generate.
  //
  TRACE("No more temp file names....\n");
  return(NULL);
  
}

PUBLIC char *
WH_TempFileName(int type, const char * request_prefix, const char * extension)
{
	static char file_buf[_MAX_PATH];		/* protected by _pr_TempName_lock */
	char* result;
#ifdef NSPR
	XP_ASSERT(_pr_TempName_lock);
	PR_EnterMonitor(_pr_TempName_lock);
#endif
	result = XP_STRDUP(xp_TempFileName(type, request_prefix, extension, file_buf));
#ifdef NSPR
	PR_ExitMonitor(_pr_TempName_lock);
#endif
	return result;
}

//
// Return a string that is equal to the NetName string but with the
//  cross-platform characters changed back into DOS characters
// The caller is responsible for XP_FREE()ing the return string
//
MODULE_PRIVATE char * 
XP_NetToDosFileName(const char * NetName)
{
    char *p, *newName;

    if(!NetName)
        return NULL;
        
    //  If the name is only '/' or begins '//' keep the
	//    whole name else strip the leading '/'
	BOOL bChopSlash = FALSE;

    if(NetName[0] == '/')
        bChopSlash = TRUE;

	// save just / as a path
	if(NetName[0] == '/' && NetName[1] == '\0')
		bChopSlash = FALSE;

	// spanky Win9X path name
	if(NetName[0] == '/' && NetName[1] == '/')
		bChopSlash = FALSE;

    if(bChopSlash)
        newName = XP_STRDUP(&(NetName[1]));
    else
        newName = XP_STRDUP(NetName);

    if(!newName)
        return NULL;

	if( newName[1] == '|' )
		newName[1] = ':';

    for(p = newName; *p; p++){
		if( *p == '/' )
			*p = '\\';
	}

    return(newName);

}

/* implement an lseek using _lseek that
 * writes zero's when extending a file
 * beyond the end.
 * This function attemps to override
 * the standard lseek.
 */
extern "C" long wfe_lseek(int fd, long offset, int origin)
{
 	long cur_pos;
	long end_pos;
	long seek_pos;

	if(origin == SEEK_CUR)
      {	
      	if(offset < 1)							  
	    	return(_lseek(fd, offset, SEEK_CUR));

		cur_pos = _lseek(fd, 0, SEEK_CUR);

		if(cur_pos < 0)
			return(cur_pos);
	  }
										 
	end_pos = _lseek(fd, 0, SEEK_END);
	if(end_pos < 0)
		return(end_pos);

	if(origin == SEEK_SET)
		seek_pos = offset;
	else if(origin == SEEK_CUR)
		seek_pos = cur_pos + offset;
	else if(origin == SEEK_END)
		seek_pos = end_pos + offset;
 	else
	  {
	  	assert(0);
		return(-1);
	  }

 	/* the seek position desired is before the
	 * end of the file.  We don't need
	 * to do anything special except the seek.
	 */
 	if(seek_pos <= end_pos)
 		return(_lseek(fd, seek_pos, SEEK_SET));
 		
 	/* the seek position is beyond the end of the
 	 * file.  Write zero's to the end.
 	 *
	 * we are already at the end of the file so
	 * we just need to "write()" zeros for the
	 * difference between seek_pos-end_pos and
	 * then seek to the position to finish
	 * the call
 	 */
 	{ 
 	 	char buffer[1024];
	   	long len = seek_pos-end_pos;
	   	memset(&buffer, 0, 1024);
	   	while(len > 0)
	      {
	        write(fd, &buffer, CASTINT((1024 > len ? len : 1024)));
		    len -= 1024L;
		  }
		return(_lseek(fd, seek_pos, SEEK_SET));
  	}		

 }
//
// Returns the absolute name of a file 
// 
// The result of this function can be used with the standard
// open/fopen ansi file functions
// The caller is responsible for XP_FREE()ing the string -*myName
//
PUBLIC char * 
xp_FileName(const char * name, XP_FileType type, char* *myName)
{
	uint16 iFileSystemCSID = CIntlWin::GetSystemLocaleCsid();	// this file system csid, not win_csid
	char * newName = NULL;
    char * netLibName = NULL;
    BOOL   bNeedToRegister = FALSE;   // == do we need to register a new 
                                      //   newshost->file name mapping
	char * prefStr = NULL;
    CString csHostName;
    CString csHost;
    struct _stat sInfo;
    int iDot;
    int iColon;

	CString fileName;
	char* tempName = NULL;

	switch(type) {
	case xpCacheFAT:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\fat.db", (const char *)theApp.m_pCacheDir);
		break;
	case xpExtCacheIndex:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\extcache.fat", (const char *)theApp.m_pCacheDir);
		break;

    // larubbio
    case xpSARCacheIndex:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        sprintf(newName, "%s\\archive.fat", theApp.m_pSARCacheDir);
        break;

	case xpHTTPCookie:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		//sprintf(newName, "%s\\cookies.txt", theApp.m_pInstallDir->GetCharValue());
		// changed -- jonm to support multi-profile
		sprintf(newName, "%s\\cookies.txt", (const char *)theApp.m_UserDirectory);
		break;

#if defined(CookieManagement)
	case xpHTTPCookiePermission:
		newName = (char *) XP_ALLOC(_MAX_PATH);
                //sprintf(newName, "%s\\cookperm.txt", theApp.m_pInstallDir->GetCharValue());
		// changed -- jonm to support multi-profile
                sprintf(newName, "%s\\cookperm.txt", (const char *)theApp.m_UserDirectory);
		break;
#endif

#if defined(SingleSignon)
	case xpHTTPSingleSignon:
		newName = (char *) XP_ALLOC(_MAX_PATH);
                //sprintf(newName, "%s\\signons.txt", theApp.m_pInstallDir->GetCharValue());
		// changed -- jonm to support multi-profile
                sprintf(newName, "%s\\signons.txt", (const char *)theApp.m_UserDirectory);
		break;
#endif

#ifdef MOZ_MAIL_NEWS      
	case xpSNewsRC:
	case xpNewsRC:
        // see if we are asking about the default news host
        // else look up in netlib
        if ( !name || !strlen(name) )
            name = g_MsgPrefs.m_csNewsHost;

        netLibName = NET_MapNewsrcHostToFilename((char *)name, 
                                                 (type == xpSNewsRC),
                                                 FALSE);
        
        // if we found something in our map just skip the rest of this stuff
        if(netLibName && *netLibName) {
            newName = XP_STRDUP(netLibName);
            break;
        }

        // whatever name we eventually end up with we will need to register it
        //   before we leave the function
        bNeedToRegister = TRUE;

        // If we are on the default host see if there is a newsrc file in the
        //   news directory.  If so, that is what we want
        if(!stricmp(name, g_MsgPrefs.m_csNewsHost)) {
            csHostName = g_MsgPrefs.m_csNewsDir;
            csHostName += "\\newsrc";
            if(_stat((const char *) csHostName, &sInfo) == 0) {
                newName = XP_STRDUP((const char *) csHostName);
                break;
            }
        }
                        
        // See if we are going to be able to build a file name based 
        //   on the hostname
        csHostName = g_MsgPrefs.m_csNewsDir;
        csHostName += '\\';

        // build up '<hostname>.rc' so we can tell how long its going to be
        //   we will use that as the default name to try
		if(type == xpSNewsRC)
			csHost += 's';
        csHost += name;

        // if we have a news host news.foo.com we just want to use the "news"
        //   part
        iDot = csHost.Find('.');
        if(iDot != -1)
            csHost = csHost.Left(iDot);

#ifdef XP_WIN16
        if(csHost.GetLength() > 8)
            csHost = csHost.Left(8);
#endif

		iColon = csHost.Find(':');
		if (iColon != -1) {
			// Windows file system seems to do horrid things if you have
			// a filename with a colon in it.
			csHost = csHost.Left(iColon);
		}

        csHost += ".rc";

        // csHost is now of the form <hostname>.rc and is in 8.3 format
        //   if we are on a Win16 box

        csHostName += csHost;

        // looks like a file with that name already exists -- panic
        if(_stat((const char *) csHostName, &sInfo) != -1) {
            
            char host[5];

            // else generate a new file in news directory
            strncpy(host, name, 4);
            host[4] = '\0';

            newName = WH_TempFileName(type, host, ".rc");
            if(!newName)
                return(NULL);

        } else {

            newName = XP_STRDUP((const char *) csHostName);

        }

 		break;
	case xpNewsrcFileMap:
        // return name of FAT file in news directory
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\fat", (const char *)g_MsgPrefs.m_csNewsDir);
		break;
	case xpNewsgroups:
	case xpSNewsgroups:
        // look up in netlib
        if ( !name || !strlen(name) )
            name = g_MsgPrefs.m_csNewsHost;

        netLibName = NET_MapNewsrcHostToFilename((char *)name, 
                                                 (type == xpSNewsgroups),
                                                 TRUE);

        if(!netLibName) {

            csHostName = g_MsgPrefs.m_csNewsDir;
            csHostName += '\\';

			if(type == xpSNewsgroups)
				csHost += 's';
            csHost += name;

            // see if we can just use "<hostname>.rcg"
            // it might be news.foo.com so just take "news"
            int iDot = csHost.Find('.');
            if(iDot != -1)
                csHost = csHost.Left(iDot);

#ifdef XP_WIN16
            if(csHost.GetLength() > 8)
                csHost = csHost.Left(8);
#endif

			iColon = csHost.Find(':');
			if (iColon != -1) {
				// Windows file system seems to do horrid things if you have
				// a filename with a colon in it.
				csHost = csHost.Left(iColon);
			}

            csHost += ".rcg";

            // csHost is now of the form <hostname>.rcg

            csHostName += csHost;

            // looks like a file with that name already exists -- panic
            if(_stat((const char *) csHostName, &sInfo) != -1) {
                
                char host[5];

                // else generate a new file in news directory
                strncpy(host, name, 4);
                host[4] = '\0';

                newName = WH_TempFileName(type, host, ".rcg");
                if(!newName)
                    return(NULL);

            } else {

                newName = XP_STRDUP((const char *) csHostName);

            }

            if ( !name || !strlen(name))
                NET_RegisterNewsrcFile(newName,(char *)(const char *)g_MsgPrefs.m_csNewsHost,
                    (type == xpSNewsgroups), TRUE );
            else
                NET_RegisterNewsrcFile(newName,(char*)name,(type == xpSNewsgroups), TRUE );

        } else {

            newName = XP_STRDUP(netLibName);

        }
        break;
	case xpMimeTypes:
		name = NULL;
		break;
#endif // MOZ_MAIL_NEWS
	case xpGlobalHistory:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		// changed -- jonm to support multi-profile
		sprintf(newName, "%s\\%s.hst", (const char *)theApp.m_UserDirectory, XP_AppName);
		break;
    case xpGlobalHistoryList:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        sprintf( newName, "%s\\ns_hstry.htm" );
        break;
	case xpKeyChain:
		name = NULL;
		break;

      /* larubbio */
      case xpSARCache:
        if(!name) {
                      return NULL;
              }
              newName = (char *) XP_ALLOC(_MAX_PATH);
              sprintf(newName, "%s\\%s", theApp.m_pSARCacheDir, name);
              break;

	case xpCache:
        if(!name) {
            tempName = WH_TempFileName(xpCache, NULL, NULL);
			if (!tempName) return NULL;
			name = tempName;
		}
        newName = (char *) XP_ALLOC(_MAX_PATH);
        if ((strchr(name,'|')  || strchr(name,':')))  { // Local File URL if find a |
            if(name[0] == '/')
            	strcpy(newName,name+1); // skip past extra slash
			else
				strcpy(newName,name); // absolute path is valid
        } else {
        	sprintf(newName, "%s\\%s", (const char *)theApp.m_pCacheDir, name);
		}
        break;
    case xpBookmarks:
	case xpHotlist: 
		if (!name || !strlen(name)) 
			name = theApp.m_pBookmarkFile;
		break;
	case xpSocksConfig:
		prefStr = NULL;
		PREF_CopyCharPref("browser.socksfile_location",&prefStr);
		name = prefStr;
		break;		
	case xpCertDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        if ( name ) {
			sprintf(newName, "%s\\cert%s.db", (const char *)theApp.m_UserDirectory, name);
        } else {
			sprintf(newName, "%s\\cert.db", (const char *)theApp.m_UserDirectory);
        }
		break;
	case xpCertDBNameIDX:
        newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\certni.db", (const char *)theApp.m_UserDirectory);
		break;
	case xpKeyDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
	if ( name ) {
	  sprintf(newName, "%s\\key%s.db", (const char *)theApp.m_UserDirectory, name);
	} else {
	  sprintf(newName, "%s\\key.db", (const char *)theApp.m_UserDirectory);
	}
		break;
	case xpSecModuleDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\secmod.db", (const char *)theApp.m_UserDirectory);
		break;
	case xpSignedAppletDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
	if ( name ) {
	  sprintf(newName, "%s\\signed%s.db", (const char *)theApp.m_UserDirectory, name);
	} else {
	  sprintf(newName, "%s\\signed.db", (const char *)theApp.m_UserDirectory);
	}
		break;
#ifdef MOZ_MAIL_NEWS      
	case xpAddrBook:
		{
#ifdef XP_WIN16
        if(!name || !strlen(name) )
            newName = WH_TempName(type, NULL);
#else
            newName = (char *) XP_ALLOC(_MAX_PATH);
            strcpy(newName, name);

            // strip off the extension
            char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
            if(!pEnd)
				pEnd = newName;

            pEnd = strchr(pEnd, '.');
			if(pEnd)
				*pEnd = '\0';
            strcat(newName, ".nab");
#endif
		}		
        break;
		case xpAddrBookNew:
		{
			newName = (char *) XP_ALLOC(_MAX_PATH);
			sprintf(newName, "%s\\%s", (const char *)theApp.m_UserDirectory, name);
			break;
		}		
        break;
		case xpVCardFile:
		{
            newName = (char *) XP_ALLOC(_MAX_PATH);
            strcpy(newName, name);

            // strip off the extension
            char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
            if(!pEnd)
				pEnd = newName;

            pEnd = strchr(pEnd, '.');
			if(pEnd)
				*pEnd = '\0';
            strcat(newName, ".vcf");
		}		
        break;
	case xpLDIFFile:
		{
            newName = (char *) XP_ALLOC(_MAX_PATH);
            strcpy(newName, name);

            // strip off the extension
            char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
            if(!pEnd)
				pEnd = newName;

            pEnd = strchr(pEnd, '.');
			if(pEnd)
				*pEnd = '\0';
#ifdef XP_WIN16
			strcat(newName, ".ldi");
#else
            strcat(newName, ".ldif");
#endif
		}		
        break;
	case xpTemporaryNewsRC:
        {
            CString csHostName = g_MsgPrefs.m_csNewsDir;
            csHostName += "\\news.tmp";
            newName = XP_STRDUP((const char *) csHostName);
        }
        break;
#endif // MOZ_MAIL_NEWS        
    case xpPKCS12File:
		{
            newName = (char *) XP_ALLOC(_MAX_PATH);
            strcpy(newName, name);

            // strip off the extension
            char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
            if(!pEnd)
				pEnd = newName;

            pEnd = strchr(pEnd, '.');
			if(pEnd)
				*pEnd = '\0';
            strcat(newName, ".p12");
		}		
		break;	
    case xpTemporary:
        if(!name || !strlen(name) )
            newName = WH_TempName(type, NULL);
        break;
#ifdef MOZ_MAIL_NEWS        
    case xpMailFolder:
		if(!name)
		    name = g_MsgPrefs.m_csMailDir;
        break;
	case xpMailFolderSummary:
        {
            newName = (char *) XP_ALLOC(_MAX_PATH);
            strcpy(newName, name);

            // strip off the extension
            char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
            if(!pEnd)
                pEnd = newName;

#ifdef XP_WIN16	// backend won't allow '.' in win16 folder names, but just to be safe.
            pEnd = strchr(pEnd, '.');
            if(pEnd)
                *pEnd = '\0';
#endif
            strcat(newName, ".snm");
        }
		break;
	case xpMailSort:
        newName = (char *) XP_ALLOC(_MAX_PATH);
		// if name is not null or empty, it's probably the host name for the imap server.
		// This will need to be extended to support per publc folder filters
		if (name && strlen(name) > 0)
			sprintf(newName, "%s\\ImapMail\\%s\\rules.dat", (const char *)theApp.m_UserDirectory, name);
		else
			sprintf(newName, "%s\\rules.dat", (const char *)g_MsgPrefs.m_csMailDir);
		break;
	case xpNewsSort:
		newName = (char *) XP_ALLOC(_MAX_PATH);
	    sprintf(newName, "%s\\%s", (const char *)g_MsgPrefs.m_csNewsDir, name);
		break;
	case xpMailFilterLog:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\mailfilt.log", (const char *)g_MsgPrefs.m_csMailDir);
		break;
	case xpNewsFilterLog:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\newsfilt.log", (const char *)g_MsgPrefs.m_csNewsDir);
		break;
	case xpMailPopState:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        sprintf(newName, "%s\\popstate.dat", (const char *)g_MsgPrefs.m_csMailDir);
		break;
	case xpMailSubdirectory:
        {
            newName = (char *) XP_ALLOC(_MAX_PATH);
            strcpy(newName, name);

            // strip off the trailing slash if any
            char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
            if(!pEnd)
                pEnd = newName;

            strcat(newName, ".sbd");
        }
		break;
#endif // MOZ_MAIL_NEWS      
    // name of global cross-platform registry
    case xpRegistry:
        // eventually need to support arbitrary names; this is the default
        newName = (char *) XP_ALLOC(_MAX_PATH);
        if ( newName != NULL ) {
            GetWindowsDirectory(newName, _MAX_PATH);
            int namelen = strlen(newName);
            //if ( newName[namelen-1] == '\\' )
            if( intl_IsEndWithBS( newName, iFileSystemCSID ) )
                namelen--;
            strcpy(newName+namelen, "\\nsreg.dat");
        }
        break;
	// name of news group database 
#ifdef MOZ_MAIL_NEWS   
	case xpXoverCache:
		newName = (char *) XP_ALLOC(_MAX_PATH);
	    sprintf(newName, "%s\\%s", (const char *)g_MsgPrefs.m_csNewsDir, name);
	    break;
#endif // MOZ_MAIL_NEWS
	case xpProxyConfig:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        //sprintf(newName, "%s\\proxy.cfg", theApp.m_pInstallDir->GetCharValue());
		sprintf(newName, "%s\\proxy.cfg", (const char *)theApp.m_UserDirectory);
		break;

    // add any cases where no modification is necessary here 	
    // The name is fine all by itself, no need to modify it 
	case xpFileToPost:
	case xpExtCache:
    case xpURL:
        // name is OK as it is
		break;
#ifdef MOZ_MAIL_NEWS      
	case xpNewsHostDatabase:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\news.db", (const char *)g_MsgPrefs.m_csNewsDir);
		break;

	case xpImapRootDirectory:
		newName = PR_smprintf ("%s\\ImapMail", (const char *)theApp.m_UserDirectory);
		break;
	case xpImapServerDirectory:
		{
			int len = 0;
			char *tempImapServerDir = XP_STRDUP(name);
			char *imapServerDir = tempImapServerDir;
#ifdef XP_WIN16
			// first, truncate it to 8 characters
			if ((len = XP_STRLEN(imapServerDir)) > 8) {
				imapServerDir = imapServerDir + len - 8;
			}

			// Now, replace all illegal characters with '_'
			const char *illegalChars = "\"/\\[]:;=,|?<>*$. ";
			for (char *possibleillegal = imapServerDir; *possibleillegal; )
				if (XP_STRCHR(illegalChars, *possibleillegal) || ((unsigned char)*possibleillegal < 31) )
					*possibleillegal++ = '_';
				else
					possibleillegal = INTL_NextChar( iFileSystemCSID, possibleillegal );
#endif
			newName = PR_smprintf ("%s\\ImapMail\\%s", (const char *)theApp.m_UserDirectory, imapServerDir);
			if (tempImapServerDir) XP_FREE(tempImapServerDir);
		}
	break;

	case xpJSMailFilters:
		newName = PR_smprintf("%s\\filters.js", (const char *)g_MsgPrefs.m_csMailDir);
		break;
#endif // MOZ_MAIL_NEWS
	case xpJSHTMLFilters:
		newName = PR_smprintf("%s\\hook.js", (const char *)theApp.m_UserDirectory);
		break;
	case xpFolderCache:
		newName = PR_smprintf ("%s\\summary.dat", (const char *)theApp.m_UserDirectory);
		break;

	case xpCryptoPolicy:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		FE_GetProgramDirectory(newName, _MAX_PATH);
		strcat(newName, "moz40p3");
		break;
	
	case xpJSCookieFilters:
		newName = PR_smprintf("%s\\cookies.js", (const char *)theApp.m_UserDirectory);
		break;


	case xpLIClientDB:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\locindep.dat", (const char *)theApp.m_UserDirectory);
		break;

	case xpLIPrefs:
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\liprefs.js", (const char *)theApp.m_UserDirectory);
		break;

	default:
		ASSERT(0);  /* all types should be covered */
        break;
	}

#ifdef MOZ_MAIL_NEWS    
    // make sure we have the correct newsrc file registered for next time
	if((type == xpSNewsRC || type == xpNewsRC) && bNeedToRegister)
        NET_RegisterNewsrcFile(newName, (char *)name, (type == xpSNewsRC), FALSE );
#endif

    // determine what file we are supposed to load and make sure it looks
    //   like a DOS pathname and not some unixy punk name
	if(newName) {
		*myName = XP_NetToDosFileName((const char*)newName);
		XP_FREE(newName); 
	} else {
		*myName = XP_NetToDosFileName((const char*)name);
	}

	if (tempName) XP_FREE(tempName);

	if (prefStr) XP_FREE(prefStr);        
	
	// whee, we're done
	return(*myName);
}   

//
// Open a file with the given name
// If a special file type is provided we might need to get the name
//  out of the preferences list
//
PUBLIC XP_File 
XP_FileOpen(const char * name, XP_FileType type, const XP_FilePerm perm)
{
	XP_File fp;
	char *filename = WH_FileName(name, type);

	if(!filename || !(*filename))
		return(NULL);
		
     //  THIS MUST BE DONE FIRST TO PLUG WIN OS SECURITY HOLE.
     //  Do not allow UNC pathnames if the command line stated so.
     if(xpURL == type && FALSE == theApp.m_bUNC) {
         //  Make sure we have enough buffer to check.
         if(filename && filename[0] && filename[1]) {
             if(
                 ('\\' == filename[0] && '\\' == filename[1]) ||
                 ('/' == filename[0] && '/' == filename[1]) ||
                 ('\\' == filename[0] && '/' == filename[1]) ||
                 ('/' == filename[0] && '\\' == filename[1])
             ) {
                 //  Presumed UNC format (or likely to not open in the first place).
                 //  Denied.
                 XP_FREE(filename);
                 return(NULL);
             }
         }
     }

#ifdef DEBUG_nobody
	TRACE("Opening a file type (%d) permissions: %s (%s)\n", type, perm, filename);
#endif

#ifdef XP_WIN32
	if (type == xpURL) {
		HANDLE	hFile;
		DWORD	dwType;

		// Check if we're trying to open a device. We don't allow this
		hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL);

		if (hFile != INVALID_HANDLE_VALUE) {
			dwType = GetFileType(hFile);
			CloseHandle(hFile);
	
			if (dwType != FILE_TYPE_DISK) {
				XP_FREE(filename);
				return NULL;
			}
		}
	}
#endif

#ifdef XP_WIN16
	// Windows uses ANSI codepage, DOS uses OEM codepage, fopen takes OEM codepage
	// That's why we need to do conversion here.
	CString oembuff = filename;
	oembuff.AnsiToOem();
	fp = fopen(oembuff, (char *) perm);

	if (fp && type == xpURL) {
		union _REGS	inregs, outregs;

		// Check if we opened a device. Execute an Interrupt 21h to invoke
		// MS-DOS system call 44h
		inregs.x.ax = 0x4400;	 // MS-DOS function to get device information
		inregs.x.bx = _fileno(fp);
		_int86(0x21, &inregs, &outregs);

		if (outregs.x.dx & 0x80) {
			// It's a device. Don't allow any reading/writing
			fclose(fp);
			XP_FREE(filename);
			return NULL;
		}
	}
#else	
	fp = fopen(filename, (char *) perm);
#endif
	XP_FREE(filename);
	return(fp);
}


//
// Rename tmpname to fname 
//
int XP_FileRename(const char *tmpname, XP_FileType tmptype, const char *fname, XP_FileType ftype)	
{
    char * pOldName = WH_FileName(tmpname, tmptype);
    char * pNewName = WH_FileName(fname, ftype);

    if(!pOldName || !pNewName)
        return(-1);

#ifdef XP_WIN32
	// try using win32 system call. If that fails, do what the old code did.
	if (MoveFileEx(pOldName, pNewName, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
	{
		XP_FREE(pOldName);
		XP_FREE(pNewName);
		return 0;
	}
#endif
    // lets try ANSI first
    int status = rename(pOldName, pNewName);

    // if rename failed try to just copy
    if(status == -1)
    {
        if(WFE_CopyFile(pOldName, pNewName))    
        {
            // we were at least able to copy the file so return the correct
            //   status and get rid of the old file
            status = 0;
            remove(pOldName);
        }
    }

    // clean up
    XP_FREE(pOldName);
    XP_FREE(pNewName);

	return(status);
}

//
// Make a directory with the given name (may need WH_FileName munging first)
//
int XP_MakeDirectory(const char* name, XP_FileType type)	
{
	char * pName = WH_FileName(name, type);
	if(pName) {
	        int dir;
		dir = mkdir(pName);
		XP_FREE(pName);
		return(dir);
	}
	else
		return(-1);
}

//
// Remove a directory with the given name
//
int XP_RemoveDirectory (const char *name, XP_FileType type)
{
	int ret = -1;
	char *pName = WH_FileName (name, type);
	if (pName)	{
		ret = rmdir (pName);
		XP_FREE(pName);
	}
	return ret;
}

/****************************************************************************
*
*	XP_FileTruncate
*
*	PARAMETERS:
*		name	- filename to be truncated
*		length	- desired size (in bytes) for the truncated file
*
*	RETURNS:
*		Non-negative if successful, -1 if an exception occurs
*
*	DESCRIPTION:
*		This function is called to truncate (or extend) a given file.
*
****************************************************************************/

int XP_FileTruncate(const char * name, XP_FileType type, int32 length)
{
	int nRtn = 0;
	
	char * szFileName = WH_FileName(name, type);
	if (szFileName != NULL)
	{
		TRY
		{
			CFile file(szFileName, CFile::modeReadWrite);
			file.SetLength((DWORD)length);
			file.Close();
		}
		CATCH(CFileException, e)
		{
			nRtn = -1;
			
			#ifdef _DEBUG
				afxDump << "File exception: " << e->m_cause << "\n";
			#endif
		}
		END_CATCH
		XP_FREE(szFileName);
	}  /* end if */
	else
	{
		nRtn = -1;
	}  /* end else */

	return(nRtn);
	
} // END OF	FUNCTION XP_FileTruncate()

//
// Mimic unix stat call
// Return -1 on error
//
PUBLIC int 
XP_Stat(const char * name, XP_StatStruct * info, XP_FileType type)
{
    char * filename = WH_FileName(name, type);
    int res;

    if(!info || !filename)
        return(-1);

    //  THIS MUST BE DONE FIRST TO PLUG WIN OS SECURITY HOLE.	
    //  Do not allow UNC pathnames if the command line stated so.
    if(xpURL == type && FALSE == theApp.m_bUNC) {
        //  Make sure we have enough buffer to check.
        if(filename && filename[0] && filename[1]) {
            if(
                ('\\' == filename[0] && '\\' == filename[1]) ||
                ('/' == filename[0] && '/' == filename[1]) ||
                ('\\' == filename[0] && '/' == filename[1]) ||
                ('/' == filename[0] && '\\' == filename[1])
            ) {
                //  Presumed UNC format (or likely to not open in the first place).
                //  Denied.
                XP_FREE(filename);
                return(-1);
            }
        }
    }	
    
    // Strip off final slash on directory names 
    // BUT we will need to make sure we have c:\ NOT c:   
    int len = XP_STRLEN(filename) - 1;
    if(len > 1 && filename[len] == '\\' && filename[len -1] != ':'){
		uint16 iFileSystemCSID = CIntlWin::GetSystemLocaleCsid();	// this file system csid, not win_csid
		if( intl_IsEndWithBS( filename, iFileSystemCSID ) )
			filename[len] = '\0';
	}

#ifdef XP_WIN16        
    //  Also, if we are looking at something like "c:", we won't call stat at 
    //      all, as we never know if they have a diskette in drive A or such 
    //      and don't want to stall waiting.
    //  Be very strict on what we let into this statement, though.
    if(len == 1 && filename[len] == ':')  {
      //  Set the stat info ourselves.  The numbers were generic, taken 
      //    from a _stat call to "c:\"
      //  Is this a hack, or what?
      info->st_atime =
      info->st_ctime =
      info->st_mtime = 315561600L;
      info->st_uid = 0;
      info->st_gid = 0;
      info->st_size = (_off_t) 0;
      info->st_rdev =
      info->st_dev = filename[len - 1] - 'A';
      info->st_mode = 16895;
      info->st_nlink = 1;
      res = 0;
    }
    else  {
	  // Windows uses ANSI codepage, DOS uses OEM codepage, _stat takes OEM codepage
	  // That's why we need to do conversion here.
	  CString oembuff = filename;
	  oembuff.AnsiToOem();
      res = _stat(oembuff, info);
	}
#else // XP_WIN16
      //  Normal file or directory.
      res = _stat(filename, info);
#endif
    XP_FREE(filename);
    return(res);
}
                             
                             
PUBLIC XP_Dir 
XP_OpenDir(const char * name, XP_FileType type)
{
    XP_Dir dir;
    char * filename = WH_FileName(name, type);
	uint16 iFileSystemCSID = CIntlWin::GetSystemLocaleCsid();	// this file system csid, not win_csid
    CString foo;
											  
    if(!filename)
        return NULL;

    //  THIS MUST BE DONE FIRST TO PLUG WIN OS SECURITY HOLE.	
    //  Do not allow UNC pathnames if the command line stated so.
    if(xpURL == type && FALSE == theApp.m_bUNC) {
        //  Make sure we have enough buffer to check.
        if(filename && filename[0] && filename[1]) {
            if(
                ('\\' == filename[0] && '\\' == filename[1]) ||
                ('/' == filename[0] && '/' == filename[1]) ||
                ('\\' == filename[0] && '/' == filename[1]) ||
                ('/' == filename[0] && '\\' == filename[1])
            ) {
                //  Presumed UNC format (or likely to not open in the first place).
                //  Denied.
                XP_FREE(filename);
                return(NULL);
            }
        }
    }	
    
	dir = (XP_Dir) new DIR;

    // For directory names we need \*.* at the end, if and only if there is an actual name specified.
    foo += filename;
    //if(filename[XP_STRLEN(filename) - 1] != '\\')
	if( !intl_IsEndWithBS( filename, iFileSystemCSID ) )
        foo += '\\';
    foo += "*.*";

    dir->directoryPtr = FindFirstFile((const char *) foo, &(dir->data));
    XP_FREE(filename);
    if(dir->directoryPtr == INVALID_HANDLE_VALUE) {
        delete dir;
        return(NULL);
    } else {
        return(dir);
    }
}


//
// HACK:- use a static location cuz unix does it that way even
//  though it makes for non-reentrant code
//
PUBLIC XP_DirEntryStruct * 
XP_ReadDir(XP_Dir dir)
{                                         
    static XP_DirEntryStruct dirEntry;

    if(dir && dir->directoryPtr) {
#ifdef XP_WIN16
        XP_STRCPY(dirEntry.d_name, dir->data.file_data.name);
#else
        XP_STRCPY(dirEntry.d_name, dir->data.cFileName);
#endif
        if(FindNextFile(dir->directoryPtr, &(dir->data)) == FALSE) {
            FindClose(dir->directoryPtr);
            dir->directoryPtr = NULL;
        }
        return(&dirEntry);
    } else {
        return(NULL);
    }
}


//
// Close the directory
//
PUBLIC void 
XP_CloseDir(XP_Dir dir)
{
	if (dir)
	{
		if (dir->directoryPtr)
			FindClose (dir->directoryPtr);
		delete dir;
	}
} 

//
// Return 0 on success, -1 on failure.  Silly unix weenies
//
PUBLIC int 
XP_FileRemove(const char * name, XP_FileType type)
{		   
	char *filename = WH_FileName(name, type);

	if(!filename)
		return(-1);

#ifdef MOZ_MAIL_NEWS	
	if(type == xpNewsRC || type == xpNewsgroups)
        NET_UnregisterNewsHost( name, FALSE);
    else if(type == xpSNewsRC || type == xpSNewsgroups)
        NET_UnregisterNewsHost(name, TRUE);
#endif // MOZ_MAIL_NEWS

	int status = remove(filename);
	XP_FREE(filename);
	return(status);
}

BOOL XP_RemoveDirectoryRecursive(const char *name, XP_FileType type)
{
	XP_DirEntryStruct *entry;
	XP_StatStruct     statbuf;
	BOOL		  ret = TRUE;
	CString 	  dot    = ".";
	CString 	  dotdot = "..";
	int 	 	  status;

	XP_Dir dir = XP_OpenDir(name, type);  
	if (!dir) return FALSE;

	// Go through the directory entries and delete appropriately
	while ((entry = XP_ReadDir(dir)))
	{
		CString child;
		child = name;
		child += "\\";
		child += entry->d_name;
		if (!(status = XP_Stat(child, &statbuf, type)))
			if (entry->d_name == dot || entry->d_name == dotdot)
			{
				// Do nothing, rmdir will clean these up
			}
			else if ((statbuf.st_mode & _S_IFMT) == _S_IFDIR) // _S_ISDIR() isn't on Win16
			{
				// Recursive call to clean out subdirectory 
				if (!XP_RemoveDirectoryRecursive(child, type))
					ret = FALSE;
			}
			else
			{
				// Everything that's not a directory is a file!
				if (XP_FileRemove(child, type) != 0)
					ret = FALSE;
			}
	}

	// OK, remove the top-level directory if we can
	if (XP_RemoveDirectory(name, type) != 0)
		ret = FALSE;
	
	XP_CloseDir(dir);

	return ret;
}


//  Should be self-explanitory --- return TRUE on success FALSE on failure
BOOL 
WFE_MoveFile(const char *cpSrc, const char *cpTarg)  
{
#ifdef XP_WIN32
	return MoveFile(cpSrc, cpTarg);  // overwrite existing target file
#else
    // lets try ANSI first
    int status = rename(cpSrc, cpTarg);

    // if rename failed try to just copy
    if(status !=0)
    {
        if(WFE_CopyFile(cpSrc, cpTarg))    
        {
            // we were at least able to copy the file so return the correct
            //   status and get rid of the old file
            status = 0;
            remove(cpSrc);
			return TRUE;
        }
		else return FALSE;
    }
	else return TRUE;
#endif
}

//  Should be self-explanitory --- return TRUE on success FALSE on failure
BOOL 
WFE_CopyFile(const char *cpSrc, const char *cpTarg)  
{
#ifdef XP_WIN32
	return CopyFile(cpSrc, cpTarg, FALSE);  // overwrite existing target file
#else
  CFile cfSource;
  CFile cfTarget;
  
  TRY {
	Bool fail;
	/* Open returns 0 on failure */
    fail = cfSource.Open(cpSrc, CFile::modeRead | CFile::shareExclusive);
	if (fail == 0)
		return (FALSE);
    fail = cfTarget.Open(cpTarg, CFile::modeWrite | CFile::modeCreate | CFile::shareExclusive);
	if (fail == 0)
		return (FALSE);
  }
  CATCH(CFileException, e)  {
    //  Couldn't do it, just return.
    return(FALSE);
  }
  END_CATCH
  
  //  Copy the thing.
  char *pBuffer = new char[8192];
  UINT uAmount;
  
  do  {
    uAmount = 8192;
    TRY {
      uAmount = cfSource.Read((void *)pBuffer, uAmount);
      cfTarget.Write((void *)pBuffer, uAmount);
    }
    CATCH(CFileException, e)  {
      //  don't know why we failed, but assume we're done.
      //  perhaps end of file.
      break;
    }
    END_CATCH
  }
  while(uAmount != 0);
  
  //  Clean up the buffer.
  //  Files close up automagically.
  delete []pBuffer;
  return(TRUE);
#endif
}

/* copy the file specified by the first argument
 * over the newsrc file.  The second argument specifies
 * the hostname for this newsrc or "" if it is the default
 * host.
 *
 * The first filename argument was generated by a call to WH_TempName
 * with an enum of "xpTemporaryNewsRC" 
 *
 * Should return TRUE on success or FALSE on failure.
 */
extern "C" Bool XP_File_CopyOverNewsRC(const char *newfile, 
                                       const char *news_hostname,
                                       Bool        is_secure)  
{
#ifdef MOZ_MAIL_NEWS
    char *pRCName = NET_MapNewsrcHostToFilename((char *)news_hostname, 
                                                is_secure,
                                                FALSE);

    //  Is the current rc valid?
    if(pRCName == NULL) {
        return(FALSE);
    }

    //  Are we getting a valid filename?
    if(newfile == NULL) {
        return(FALSE);
    }

    //  Does the new one exist?
    CFileStatus rStatus;
    if(CFile::GetStatus(newfile, rStatus) == 0) {
        return(FALSE);
    }

    //  Does the old one exist?
    //  If so, we should remove it.
    if(CFile::GetStatus(pRCName, rStatus) != 0) {
        //  Attempt to remove.
        TRY {
            CFile::Remove(pRCName);
        }
        CATCH(CFileException, e)  {
            return(FALSE);
        }
        END_CATCH     
    }

    TRY {
        //  Attempt a speedy rename.
        CFile::Rename(newfile, pRCName);
    }
    CATCH(CFileException, e)  {
        //  Rename didn't work, attempt to copy, as fast as possible.
        if(FALSE == WFE_CopyFile(newfile, pRCName)) {
            //  Uh, like we lost the newsrc file permanently here!!!!!!!!!!
            ASSERT(0);
            return(FALSE);
        }

        //  Should remove the temp file now, as it is expected to go away.
        TRY {
            CFile::Remove(newfile);
        }
        CATCH(CFileException, e)  {
            //  Leave it lying around, not much we can do.
            return(TRUE);
        }
        END_CATCH
    }
    END_CATCH
#endif // MOZ_MAIL_NEWS
    return(TRUE);
}

extern "C" XP_Bool XP_FileNameContainsBadChars (const char *name)
{
#ifdef XP_WIN16
	char *badChars = "\\/:*?\"<>| ,+";
#else
	char *badChars = "\\/:*?\"<>|";
#endif
	XP_ASSERT( name != NULL );
	uint len = XP_STRLEN(badChars);
	if( len < 1 )
		return FALSE;

	uint16 iFileSystemCSID = CIntlWin::GetSystemLocaleCsid();	// this file system csid, not win_csid
	while( *name != NULL ){
		for (uint j = 0; j < len; j++)
			if (*name == badChars[j])
				return TRUE;
		name = INTL_NextChar( iFileSystemCSID, (char*)name );
	}
	return FALSE;
}

// Print a trace message to the console
//
#ifdef DEBUG
PUBLIC void 
FE_Trace(const char * msg)
{
    /* This used to be just TRACE(msg), but that's a mistake -- 
     * if msg happens to have a '%' in it, TRACE will
     * interpret the '%' as the beginning of an argument to be
     * printf'd and look up the stack and try to
     * interpret random data as the argument. At a minimum this
     * leads to corrupt trace info. Usually it causes bus errors.
     */
    TRACE("%s", msg);
}
#endif

PUBLIC void 
FE_ConnectToRemoteHost(MWContext * context, int url_type, char *
    hostname, char * port, char * username) 
{
    LPSTR   lpszCommand;
    UINT    iRet;

    ASSERT(url_type == FE_TN3270_URL_TYPE ||
           url_type == FE_TELNET_URL_TYPE ||
           url_type == FE_RLOGIN_URL_TYPE);

#ifdef _WIN32
	BOOL checkRegistry = TRUE;
	CInternetShortcut InternetShortcut;
    if (InternetShortcut.ShellSupport() && !theApp.m_bParseTelnetURLs)
	{
      SHELLEXECUTEINFO    sei;
	  
      // Build the command string
      if (port)
        lpszCommand = PR_smprintf("%s://%s:%s", url_type == FE_TN3270_URL_TYPE ?
            "tn3270" : "telnet", hostname, port);
      else
        lpszCommand = PR_smprintf("%s://%s", url_type == FE_TN3270_URL_TYPE ?
            "tn3270" : "telnet", hostname);

      // Use ShellExecuteEx to launch
      sei.cbSize = sizeof(sei);
      sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
      sei.hwnd = NULL;
      sei.lpVerb = NULL;  // default to Open
      sei.lpFile = lpszCommand;
      sei.lpParameters = NULL;
      sei.lpDirectory = NULL;
      sei.nShow = SW_SHOW;
      ShellExecuteEx(&sei);
      iRet = (UINT)sei.hInstApp;
	  
      XP_FREE(lpszCommand);

	  checkRegistry = iRet < 32;
	}
	
	if (checkRegistry)
	{
#endif // _WIN32
    char    szBuf[_MAX_PATH];
	LONG	lResult;
	LONG	cbData;
	HKEY	hKey;

	// Build the shell\open key for the file type class
	PR_snprintf(szBuf, sizeof(szBuf), "%s\\shell\\open",
        url_type == FE_TN3270_URL_TYPE ? "tn3270" : "telnet");
	
    // Open the key
    lResult = RegOpenKey(HKEY_CLASSES_ROOT, szBuf, &hKey);
	if (lResult != ERROR_SUCCESS) {
        // Nothing registered for the file type class
        FE_Alert(context, szLoadString(IDS_NO_VIEWER_CONFIGD));
        return;
    }

    // Get the Open command string
    cbData = sizeof(szBuf);
	lResult = RegQueryValue(hKey, "command", szBuf, &cbData);
	RegCloseKey(hKey);

    if (lResult == ERROR_SUCCESS) {
        LPSTR   lpszApp = szBuf;
        LPSTR   lpszArg;

        // Strip off any command line options
        lpszArg = strstr(lpszApp, "%1");
        if (lpszArg)
            *lpszArg = '\0';  // null terminate the string here

        // Build the WinExec command line
        if (port)
            lpszCommand = PR_smprintf("%s %s %s", lpszApp, hostname, port);
        else
            lpszCommand = PR_smprintf("%s %s", lpszApp, hostname);
    
        // Run it
        iRet = (UINT)WinExec(lpszCommand, SW_SHOW);
        XP_FREE(lpszCommand);
    
        // If the error is file not found or path not found report it here; otherwise
        // report it below
        if (iRet == 2 || iRet == 3) {
            LPSTR   lpszMsg;
    
            lpszMsg = PR_smprintf(szLoadString(IDS_UNABLE_FIND_APP), lpszApp);
            if (lpszMsg) {
                FE_Alert(context, lpszMsg);
                XP_FREE(lpszMsg);
            }
    
            return;
        }
    }
#ifdef _WIN32
	}
#endif // _WIN32

    // See if it worked
    if (iRet < 32) {
        LPSTR   lpszMsg;

        lpszMsg = PR_smprintf(szLoadString(IDS_UNABLE_TO_LAUNCH_APP), iRet);
        if (lpszMsg) {
            FE_Alert(context, lpszMsg);
            XP_FREE(lpszMsg);
        }

    } else {
        FE_Progress(context, szLoadString(IDS_SPAWNING_APP));

        if (username) {
            LPSTR   lpszMsg;

            lpszMsg = PR_smprintf(szLoadString(IDS_LOGIN_AS), username);
            if (lpszMsg) {
                FE_Alert(context, lpszMsg);
                XP_FREE(lpszMsg);
            }
        }
    }
}


PUBLIC void *FE_AboutData (const char *which, char **data_ret, int32 *length_ret, char **content_type_ret)
{
  char *loc = (char*)strchr (which, '?');
  char *which2;
  char *tmp;
  
  if (loc)
    *loc++ = 0;

    BOOL bHandled = FALSE;
    if(FALSE == bHandled) {
        char *pCompare = strdup(which);
        if(pCompare) {
            char *pReplace = strchr(pCompare, '.');
            if(pReplace) {
                *pReplace = '_';
            }
            HINSTANCE hInst = AfxGetResourceHandle();
            if(hInst) {
                HRSRC hFound = FindResource(hInst, pCompare, RT_RCDATA);
                free(pCompare);
                pCompare = NULL;
                if(hFound) {
                    HGLOBAL hRes = LoadResource(hInst, hFound);
                    hFound = NULL;
                    if(hRes) {
                        char *pBytes = (char *)LockResource(hRes);
                        if(pBytes) {
                            //  Check for identifying bin2rc string.
                            if(!strcmp(pBytes, "bin2rc generated resource")) {
                                pBytes += 26; // length + 1

                                //  Next is a content-type.
                                //  The mime type should be something we recognize.
                                //  Assign in a constant string only.
                                if(!stricmp(pBytes, IMAGE_GIF))   {
                                    *content_type_ret = IMAGE_GIF;
                                }
                                else if(!stricmp(pBytes, IMAGE_JPG))    {
                                    *content_type_ret = IMAGE_JPG;
                                }
                                pBytes += strlen(pBytes) + 1;  // length + 1

                                //  Next is the size of the data if we are willing.
                                if(*content_type_ret)   {
                                    *length_ret =  strtol(pBytes, NULL, 10);
                                }
                                pBytes += strlen(pBytes) + 1; // length + 1

                                //  Next is the data if any.
                                if(*length_ret)  {
                                    *data_ret = (char *)malloc(*length_ret);
                                    if(*data_ret) {
                                        memcpy(*data_ret, pBytes, *length_ret);
                                        bHandled = TRUE;
                                    }
                                    else    {
                                        //  Unable to alloc.
                                        //  Clear out.
                                        *content_type_ret = NULL;
                                        *length_ret = 0;
                                    }
                                }
                            }
                            pBytes = NULL;
                        }
                        UnlockResource(hRes);
                        FreeResource(hRes);
                        hRes = NULL;
                    }
                }
            }
        }
    }

    if(FALSE == bHandled)    {
      which2 = strdup (which);
      for (tmp = which2; *tmp; tmp++)
	      *tmp += 69;

        char *a = NULL;
        if (!strcmp (which2, ""))   { 				// about:
            CString csVersion, csBuildNumber;
		    char *tmpversionBuf;

			csVersion = XP_AppVersion;
			csVersion = csVersion.Left(csVersion.Find("]") + 1);
		    tmpversionBuf = (char *)XP_ALLOC(15);
		    //Loading from netscape.exe instead of resdll.dll since the string has been moved #109455
		    LoadString(::AfxGetInstanceHandle(), IDS_APP_BUILD_NUMBER, tmpversionBuf, 15);
			csBuildNumber = tmpversionBuf;
		    if (tmpversionBuf) XP_FREE(tmpversionBuf);
			csVersion+=csBuildNumber;

            char *pSSLVersion = NULL;
			char *pSSLString = NULL;
			char *pSSLCapability = SECNAV_SSLCapabilities();
            pSSLVersion = strdup(SECNAV_SecurityVersion(PR_TRUE));
            /* NULL pointer check. strlen fires a page fault if passed a NULL pointer */
            if (pSSLVersion && pSSLCapability) 
			{
				pSSLString = (char *)malloc(strlen(pSSLVersion) + strlen(pSSLCapability) + 48*2);
                sprintf(pSSLString, szLoadString(IDS_ABOUT_SECURITY), pSSLVersion, pSSLCapability);
            }
			if (pSSLCapability) free(pSSLCapability);

            char *pSp = szLoadString(IDS_ABOUT_0);
            /* Null pointer check */
            if (pSp && pSSLString ) 
			{
				a = (char *)malloc(strlen(pSp) + strlen(pSSLString) + strlen(csVersion) * 2 + 48);
                sprintf(a, pSp, (const char *)csVersion, (const char *)csVersion, pSSLString);
            }
        
            /* Free string space if allocated */
			if (pSSLString) free(pSSLString);
			if (pSSLVersion) free(pSSLVersion);
        }
        else if (!strcmp (which, "mailintro"))
          a = strdup(szLoadString(IDS_MAIL_0)); 
        else if (!strcmp (which, "license"))
	      a = wfe_LoadResourceString("license");      
        else if (!strcmp (which, "mozilla"))
          a = strdup (szLoadString(IDS_MOZILLA_0));
        else if (!strcmp (which2, "\250\264\265\276\267\256\254\255\271"))
          a = strdup(szLoadString(IDS_COPYRIGHT_0));
        else if (!strcmp (which2, "\247\261\246\263\260"))
          a = strdup ("");                                           
#ifdef EDITOR
        else if (!strcmp (which, "editfilenew"))
          a = strdup (EDT_GetEmptyDocumentString());
#endif // EDITOR
	    else if (!strcmp(which, "plugins"))
	      a = wfe_LoadResourceString("aboutplg");      
        else
          a = strdup(which);

    
        if (a)
        {
        //          for (tmp = a; *tmp; tmp++) *tmp -= 69;
          *data_ret = a;
          *length_ret = strlen (*data_ret);
          *content_type_ret = TEXT_MDL; 
      
          if(!strcmp (which, "license"))
            *content_type_ret = TEXT_PLAIN;

#ifdef DEBUG
          //    Produce some hard to debug situations here manually.
          if(!stricmp(which, "WM_ENDSESSION"))   {
              //    Shutdown of the system case.
              FEU_FrameBroadcast(TRUE, WM_QUERYENDSESSION, 1, 0);
              FEU_FrameBroadcast(FALSE, WM_ENDSESSION, 1, 0);
          }
          else if(!stricmp(which, "debug")) {
            DebugBreak();
          }
          else if(!stricmp(which, "PostQuitMessage"))   {
            PostQuitMessage(0);
          }
#endif
        }
        else
        {
          *data_ret = 0;
          *length_ret = 0;
          *content_type_ret = 0;
        }
        free (which2);
    }

    return(*data_ret);
}

/*****/

PUBLIC void FE_FreeAboutData(void *data, const char *which2)
{
    if(data)   {
        free(data);
        data = NULL;
    }
}

char *XP_PlatformPartialPathToXPPartialPath (const char *platformName)
{
	return XP_STRDUP(platformName);
}


char *XP_PlatformFileToURL (const char *platformName)
{
	CString csXPName;
	WFE_ConvertFile2Url (csXPName, platformName);
	return XP_STRDUP(csXPName);
}
void WFE_ConvertFile2Url(CString& csUrl, const char *pLocalFileName)
{
	uint16 iFileSystemCSID = CIntlWin::GetSystemLocaleCsid();	// this file system csid, not win_csid
	//	First empty the returning url, in case it has something in it.
	csUrl.Empty();

	//	If there's nothing to convert, return now.
	if(pLocalFileName == NULL || *pLocalFileName == '\0')	{
		return;
	}

	//	A CString we can muck with.
	CString csLocal = pLocalFileName;

    if (csLocal.GetLength() > 4) {        

        if ( !strnicmp(&pLocalFileName[strlen(pLocalFileName)-4],".lnk",4)) {
            char szName[_MAX_PATH];
            HRESULT hRes = ::ResolveShortCut(NULL, pLocalFileName, szName);
            if (SUCCEEDED(hRes)) {
                csLocal = szName;
            }
        }
        else if (!strnicmp(&pLocalFileName[strlen(pLocalFileName)-4],".url",4)) {
            CInternetShortcut InternetShortcut;
  		    if (InternetShortcut.ShellSupport()) {
	    	    // got a valid .URL internet shortcut file
                char szNew[_MAX_PATH];
			    CInternetShortcut Shortcut (pLocalFileName);
			    Shortcut.GetURL(szNew,sizeof(szNew));
                csUrl = szNew;
                return;
    	    }
        }
    }


	//	Convert $ROOT to the directory of the executable.
	if(csLocal.Left(5) == "$ROOT")	{
		char aPath[_MAX_PATH];
		::GetModuleFileName(theApp.m_hInstance, aPath, _MAX_PATH);

		//	Find the trailing slash.
		char *pSlash = intl_strrchr(aPath, '\\', iFileSystemCSID);
		if(pSlash)	{
			*pSlash = '\0';
		}
		else	{
			aPath[0] = '\0';
		}

		csLocal = aPath + csLocal.Right(csLocal.GetLength() - 5);
	}
	
	//	Convert local file paths which have a drive letter to an appropriate URL.
	//	This means we must always have a fully qualified file path in order to convert
	//		these correctly.
	const char* src = csLocal.GetBuffer( csLocal.GetLength() ) + 2;
	const char* end = src + csLocal.GetLength() - 2;
	if(csLocal.GetLength() >= 2 && csLocal[1] == ':')	{
		csUrl += "file:///";
		csUrl += csLocal[0];
		csUrl += '|';
#ifdef BUG93660_4_05_FIX
		for(int iTraverse = 2; iTraverse < csLocal.GetLength();
			iTraverse++)	
		{
			if(csLocal[iTraverse] == '\\')
			{
				// bug 93660 -- double byte file name gets truncated
				// We don't want to replace the backslash with
				// forwardslash in case of the double byte file name
				// iTraverse starts with 2; iTraverse - 1 will not go
				// negative 
				if (CIntlWin::GetSystemLocaleCsid() & MULTIBYTE &&
					INTL_IsLeadByte(CIntlWin::GetSystemLocaleCsid(), 
									csLocal[iTraverse-1]))
				{
					csUrl += csLocal[iTraverse];
				}
				else
				{
					csUrl += '/';
				}
			}
			else
			{
				csUrl += csLocal[iTraverse];
			}
		}
#endif
	}
	//	Convert microsoft network paths.
	else if(csLocal.Left(2) == "\\\\" || csLocal.Left(2) == "//")	{
		csUrl += "file:////";
	}
	else	{
		//	This was a normal URL.
		//	Don't corrupt it.
		csUrl = pLocalFileName;
		src = end = NULL;
	}

	if( src != NULL ){
		while( src < end ){
  			if( *src == '\\'){
  				csUrl += '/';
				src++;

  			} else {
				const char* next = INTL_NextChar( iFileSystemCSID, (char*)src );
				ASSERT( next <= end );
				while( src < next )
					csUrl += *src++;
  			}
		}
 		csLocal.ReleaseBuffer();
	}




    // This seems to be missing cases such as "..\foo"
    // Replace all "\" with "/" Note: only do this for the file: protocol or if there is no
	// specified protocol type; this way we won't munge things like mocha: and mailbox:
	int	nUrlType = NET_URL_Type(csUrl);

	if (nUrlType == FILE_TYPE_URL || nUrlType == 0) {
 		char* src = csUrl.GetBuffer( csUrl.GetLength() );
 		while( *src != NULL ){
 			if( *src == '\\' )
 				*src++ = '/';
 			else
 				src = INTL_NextChar( iFileSystemCSID, src );
		}
 		csUrl.ReleaseBuffer();
	}

    //	Check for a file url with an extra slash.
	//	This is special case code to make sure an arbitrary number
	//		of slashes sends the file URL as a microsoft network
	//		path.
	//	Go recursive, minus one slash.
	if(csUrl.Left(10) == "file://///")	{
		CString csLeft = csUrl.Left(9);
		CString csRight = csUrl.Right(csUrl.GetLength() - 10);
		WFE_ConvertFile2Url(csUrl, (const char *)(csLeft + csRight));
	}
}

extern CString WFE_EscapeURL(const char* url)
{
	CString urlStr(url);
	CString encoded;
	int l = urlStr.GetLength();
	for (int i = 0; i < l; i++)
	{
		char c = urlStr[i];
		if (c == ' ')
			encoded += "%20";
		else encoded += c;
	}

	return encoded;
}

extern void FE_SetRefreshURLTimer(MWContext *pContext, uint32 ulSeconds, char *pRefreshUrl) {
//	Purpose:    Set a delayed load timer for a window for a particular URL.
//	Arguments:  pContext    The context that we're in.  We are only interested on wether or not there appears to be a Frame/hence document.
//              ulSeconds   The number of seconds that will pass before we attempt the reload.
//	Returns:    void
//	Comments:
//	Revision History:
//      02-17-95    created GAB
//		07-22-95	modified to use new context.
//      05-03-96    modified to use new API outside of idle loop.
//

    //  Only do this if it is a window context from the back end.
    if(ABSTRACTCX(pContext) && ABSTRACTCX(pContext)->IsWindowContext()) {
        FEU_ClientPull(pContext, ulSeconds * 1000, NET_CreateURLStruct(pRefreshUrl, NET_NORMAL_RELOAD), FO_CACHE_AND_PRESENT, FALSE);
    }
}

#if defined XP_WIN16 && defined DEBUG
void AfxDebugBreak()	{
	_asm { int 3 };
}
#endif

extern "C" void XP_Assert(int stuff)	{
    ASSERT(stuff);
}

extern "C" void fe__assert(void *, void *, unsigned)	{
#ifdef DEBUG
	TRACE("lowest level Assertion failure!\n");
	AfxDebugBreak(); 
#endif
}
extern "C" void fe_abort(void)	{
#ifdef DEBUG
	TRACE("lowest level Abortion called!\n");
	AfxDebugBreak();
#endif
}

extern "C" void XP_AssertAtLine( char *pFileName, int iLine ){
#ifdef DEBUG
#ifdef XP_WIN32
    if( AfxAssertFailedLine( pFileName, iLine) )
#endif
    {
        AfxDebugBreak();
    }
#endif
}

// Draws a 3D button exactly as Windows95 style
void WFE_DrawWindowsButtonRect(HDC hDC, LPRECT lpRect, BOOL bPushed)
{
    
    HPEN penHighlight = ::CreatePen(PS_SOLID, 1, bPushed ? sysInfo.m_clrBtnShadow : sysInfo.m_clrBtnHilite);
    HPEN penShadow = ::CreatePen(PS_SOLID, 1, bPushed ? sysInfo.m_clrBtnHilite : sysInfo.m_clrBtnShadow);
    HPEN penBlack = ::CreatePen(PS_SOLID, 1, RGB(0,0,0));

    // Top and left
    HPEN penOld = (HPEN)::SelectObject(hDC, bPushed ? penBlack : penHighlight);
    ::MoveToEx(hDC, lpRect->left, lpRect->bottom, NULL);
    ::LineTo(hDC, lpRect->left, lpRect->top);
    ::LineTo(hDC, lpRect->right+1, lpRect->top);

    // Bottom and right
    ::SelectObject(hDC, bPushed ? penHighlight : penBlack);
    ::MoveToEx(hDC, lpRect->left+1, lpRect->bottom, NULL);
    ::LineTo(hDC, lpRect->right, lpRect->bottom);
    ::LineTo(hDC, lpRect->right, lpRect->top);

    // Shadow color line inside to black line
    ::SelectObject(hDC, penShadow);
   ::MoveToEx(hDC, lpRect->left+1, lpRect->bottom-1, NULL);
    if( bPushed )
    {
        ::LineTo(hDC, lpRect->left+1, lpRect->top-1);
        ::LineTo(hDC, lpRect->right-1, lpRect->top-1);
    } else {
        ::LineTo(hDC, lpRect->right-1, lpRect->bottom-1);
        ::LineTo(hDC, lpRect->right-1, lpRect->top+1);
    }

    // Cleanup
    ::SelectObject(hDC, penOld);
    ::DeleteObject(penShadow);
    ::DeleteObject(penHighlight);
    ::DeleteObject(penBlack);

}

//============================================================= DrawHighlight
// Draw the highlight around the area
 void WFE_DrawHighlight( HDC hDC, LPRECT lpRect, COLORREF clrTopLeft, COLORREF clrBottomRight )
{
	HPEN hpenTopLeft = ::CreatePen( PS_SOLID, 0, clrTopLeft );
	HPEN hpenBottomRight = ::CreatePen( PS_SOLID, 0, clrBottomRight );
	HPEN hpenOld = (HPEN) ::SelectObject( hDC, hpenTopLeft );

	::MoveToEx( hDC, lpRect->left, lpRect->bottom, NULL );
	::LineTo( hDC, lpRect->left, lpRect->top );
	::LineTo( hDC, lpRect->right - 1, lpRect->top );

	::SelectObject( hDC, hpenBottomRight );
	::LineTo( hDC, lpRect->right - 1, lpRect->bottom - 1);
	::LineTo( hDC, lpRect->left, lpRect->bottom - 1);

	::SelectObject( hDC, hpenOld );

	VERIFY(::DeleteObject( hpenTopLeft ));
	VERIFY(::DeleteObject( hpenBottomRight ));
}


//============================================================ DrawRaisedRect
void WFE_DrawRaisedRect( HDC hDC, LPRECT lpRect )
{
	RECT rcTmp = *lpRect;
	::InflateRect( &rcTmp, -1, -1 );
#ifdef _WIN32
	WFE_DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_3DLIGHT ), 
				   GetSysColor( COLOR_3DSHADOW ) );
	WFE_DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_3DHILIGHT ), 
				   GetSysColor( COLOR_3DDKSHADOW ) );
#else
	WFE_DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_BTNHIGHLIGHT ), 
				   GetSysColor( COLOR_BTNSHADOW ) );
	WFE_DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_BTNHIGHLIGHT ), 
				   GetSysColor( COLOR_BTNSHADOW ) );
#endif
}


//=========================================================== DrawLoweredRect
void WFE_DrawLoweredRect( HDC hDC, LPRECT lpRect )
{
	RECT rcTmp = *lpRect;
	::InflateRect( &rcTmp, -1, -1 );
#ifdef _WIN32
	WFE_DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_3DSHADOW ), 
				   GetSysColor( COLOR_3DLIGHT ) );
	WFE_DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_3DDKSHADOW ), 
				   GetSysColor( COLOR_3DHILIGHT) );
#else
	WFE_DrawHighlight( hDC, &rcTmp, 
				   GetSysColor( COLOR_BTNSHADOW ), 
				   GetSysColor( COLOR_BTNHIGHLIGHT ) );
	WFE_DrawHighlight( hDC, lpRect, 
				   GetSysColor( COLOR_BTNSHADOW ), 
				   GetSysColor( COLOR_BTNHIGHLIGHT ) );
#endif
}

void WFE_Draw3DButtonRect( HDC hDC, LPRECT lpRect, BOOL bPushed )
{
	if ( bPushed ) {
		WFE_DrawLoweredRect( hDC, lpRect );
	} else {
		WFE_DrawRaisedRect( hDC, lpRect );
	}
}

void WFE_MakeTransparentBitmap( HDC hDC, HBITMAP hBitmap )
{
	BITMAP bm;
	COLORREF cColor;
	HBITMAP bmAndBack, bmAndObject, bmAndMem;
	HGDIOBJ bmBackOld, bmObjectOld, bmMemOld;
	HDC hdcTemp, hdcMem, hdcBack, hdcObject;

	VERIFY( ::GetObject( hBitmap, sizeof(BITMAP), (LPSTR)&bm ) );

	// Create some DCs to hold temporary data.
	hdcTemp   = CreateCompatibleDC(hDC);
	hdcBack   = CreateCompatibleDC(hDC);
	hdcObject = CreateCompatibleDC(hDC);
	hdcMem    = CreateCompatibleDC(hDC);

	SelectObject( hdcTemp, hBitmap );

	// Monochrome DC
	bmAndBack   = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, NULL);

	// Monochrome DC
	bmAndObject = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, NULL);

	bmAndMem    = CreateCompatibleBitmap(hDC, bm.bmWidth, bm.bmHeight);

	// Each DC must select a bitmap object to store pixel data.
	bmBackOld   = ::SelectObject(hdcBack, bmAndBack);
	bmObjectOld = ::SelectObject(hdcObject, bmAndObject);
	bmMemOld    = ::SelectObject(hdcMem, bmAndMem);

	// Set the background color of the source DC to the color
	// contained in the parts of the bitmap that should be transparent
	cColor = SetBkColor(hdcTemp, RGB_TRANSPARENT );

	// Create the object mask for the bitmap by performing a BitBlt
	// from the source bitmap to a monochrome bitmap.
	BitBlt(hdcObject, 0, 0, bm.bmWidth, bm.bmHeight, hdcTemp, 0, 0, SRCCOPY);

	// Set the background color of the source DC back to the original
	// color.
	SetBkColor(hdcTemp, cColor);

	// Create the inverse of the object mask.
	BitBlt(hdcBack, 0, 0, bm.bmWidth, bm.bmHeight, hdcObject, 0, 0, NOTSRCCOPY);

	// Fill the background of the mem DC with the system background color.
	FillRect( hdcMem, CRect( 0, 0, bm.bmWidth, bm.bmHeight ), CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) ) );

	// Mask out the places where the bitmap will be placed.
	BitBlt(hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, hdcObject, 0, 0, SRCAND);

	// Mask out the transparent colored pixels on the bitmap.
	BitBlt(hdcTemp, 0, 0, bm.bmWidth, bm.bmHeight, hdcBack, 0, 0, SRCAND);

	// XOR the bitmap with the background on the destination DC.
	BitBlt(hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, hdcTemp, 0, 0, SRCPAINT);

	// Copy the results back to the source bitmap.
	BitBlt(hdcTemp, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

	// Delete the memory bitmaps.
	VERIFY(DeleteObject(SelectObject(hdcBack, bmBackOld)));
	VERIFY(DeleteObject(SelectObject(hdcObject, bmObjectOld)));
	VERIFY(DeleteObject(SelectObject(hdcMem, bmMemOld)));

	// Delete the memory DCs.
	VERIFY(DeleteDC(hdcMem));
	VERIFY(DeleteDC(hdcBack));
	VERIFY(DeleteDC(hdcObject));
	VERIFY(DeleteDC(hdcTemp));
}

#define RGB_TO_RGBQUAD(r,g,b)   (RGB(b,g,r))
#define CLR_TO_RGBQUAD(clr)     (RGB(GetBValue(clr), GetGValue(clr), GetRValue(clr)))

HBITMAP WFE_LoadSysColorBitmap( HINSTANCE hInst, LPCSTR lpszResourceName )
{
	struct COLORMAP
	{
		// use DWORD instead of RGBQUAD so we can compare two RGBQUADs easily
		DWORD rgbqFrom;
		int iSysColorTo;
	};
	static const COLORMAP sysColorMap[] =
	{
		// mapping from color in DIB to system color
		{ RGB_TO_RGBQUAD(0x00, 0x00, 0x00),  COLOR_BTNTEXT },       // black
		{ RGB_TO_RGBQUAD(0x80, 0x80, 0x80),  COLOR_BTNSHADOW },     // dark grey
		{ RGB_TO_RGBQUAD(0xC0, 0xC0, 0xC0),  COLOR_BTNFACE },       // bright grey
		{ RGB_TO_RGBQUAD(0xFF, 0xFF, 0xFF),  COLOR_BTNHIGHLIGHT },   // white
		{ RGB_TO_RGBQUAD(0xFF, 0x00, 0xFF),  COLOR_BTNFACE }   // hot pink
	};
	const int nMaps = 5;

	HRSRC hRsrc = ::FindResource(hInst, lpszResourceName, RT_BITMAP);

	HGLOBAL hglb;
	if ((hglb = LoadResource(hInst, hRsrc)) == NULL)
		return NULL;

	LPBITMAPINFOHEADER lpBitmap = (LPBITMAPINFOHEADER)LockResource(hglb);
	if (lpBitmap == NULL)
		return NULL;

	// make copy of BITMAPINFOHEADER so we can modify the color table
	int nColorTableSize = 1 << lpBitmap->biBitCount;
	UINT nSize = CASTUINT(lpBitmap->biSize + nColorTableSize * sizeof(RGBQUAD));
	LPBITMAPINFOHEADER lpBitmapInfo = (LPBITMAPINFOHEADER)::malloc(nSize);
	if (lpBitmapInfo == NULL)
		return NULL;
	memcpy(lpBitmapInfo, lpBitmap, nSize);

	// color table is in RGBQUAD DIB format
	DWORD* pColorTable =
		(DWORD*)(((LPBYTE)lpBitmapInfo) + (UINT)lpBitmapInfo->biSize);

	for (int iColor = 0; iColor < nColorTableSize; iColor++)
	{
		// look for matching RGBQUAD color in original
		for (int i = 0; i < nMaps; i++)
		{
			if (pColorTable[iColor] == sysColorMap[i].rgbqFrom)
			{
				pColorTable[iColor] =
					CLR_TO_RGBQUAD(::GetSysColor(sysColorMap[i].iSysColorTo));
				break;
			}
		}
	}

	int nWidth = (int)lpBitmapInfo->biWidth;
	int nHeight = (int)lpBitmapInfo->biHeight;
	HDC hDCScreen = ::GetDC(NULL);
	HBITMAP hbm = ::CreateCompatibleBitmap(hDCScreen, nWidth, nHeight);

	if (hbm != NULL)
	{
		HDC hDCGlyphs = ::CreateCompatibleDC(hDCScreen);
		HBITMAP hbmOld = (HBITMAP)::SelectObject(hDCGlyphs, hbm);

		LPBYTE lpBits;
		lpBits = (LPBYTE)(lpBitmap + 1);
		lpBits += (1 << (lpBitmapInfo->biBitCount)) * sizeof(RGBQUAD);

		StretchDIBits(hDCGlyphs, 0, 0, nWidth, nHeight, 0, 0, nWidth, nHeight,
			lpBits, (LPBITMAPINFO)lpBitmapInfo, DIB_RGB_COLORS, SRCCOPY);
		SelectObject(hDCGlyphs, hbmOld);

		::DeleteDC(hDCGlyphs);
	}
	::ReleaseDC(NULL, hDCScreen);

	// free copy of bitmap info struct and resource itself
	::free(lpBitmapInfo);
	::FreeResource(hglb);

	return hbm;
}

/////////////////////////////////////////////////////////////////////////////
// equivalent to LoadBitmap(), except uses pDC for
// correct color mapping of BMP into pDC's palette.
HBITMAP wfe_LoadBitmap(HINSTANCE hInst, HDC hDC, LPCSTR pszBmName ) 
{
	int iFrames;

	if ((pszBmName == MAKEINTRESOURCE(IDB_ANIM_0)) && (CUST_IsCustomAnimation(&iFrames))) {
		char *pFile=NULL,*pUrl=NULL;
		HBITMAP hCust = NULL;
		CString csProfileFile;

		PREF_CopyConfigString("toolbar.logo.win_large_file",&pFile);

		// we need to first check the profile for the file (for PE) and then resort to the default of the EXE dir
		csProfileFile = theApp.m_UserDirectory;
		csProfileFile += "\\";
		csProfileFile += pFile;
		hCust = WFE_LoadBitmapFromFile(csProfileFile);
		
		if (!hCust)  // try the default place
			hCust = WFE_LoadBitmapFromFile(pFile);	
		if (pFile) XP_FREE(pFile);
		if (hCust) return hCust;
	}

	if ((pszBmName == MAKEINTRESOURCE(IDB_ANIMSMALL_0)) && (CUST_IsCustomAnimation(&iFrames))) {
		char *pFile=NULL,*pUrl=NULL;
		HBITMAP hCust = NULL;
		CString csProfileFile;

		PREF_CopyConfigString("toolbar.logo.win_small_file",&pFile);

		// we need to first check the profile for the file (for PE) and then resort to the default of the EXE dir
		csProfileFile = theApp.m_UserDirectory;
		csProfileFile += "\\";
		csProfileFile += pFile;
		hCust = WFE_LoadBitmapFromFile(csProfileFile);
		
		if (!hCust)  // try the default place
			hCust = WFE_LoadBitmapFromFile(pFile);
		if (pFile) XP_FREE(pFile);
		if (hCust) return hCust;
	}

    // get the resource from the file
    HBITMAP hBmp = (HBITMAP)FindResource(hInst, pszBmName, RT_BITMAP);
    ASSERT(hBmp != NULL);
    if(!hBmp)
        return NULL;
    HGLOBAL hRes = LoadResource(hInst, (HRSRC)hBmp);
    ASSERT(hRes != NULL);
    if(!hRes)
        return NULL;
    LPBITMAPINFO pPackedDib = (LPBITMAPINFO)(LockResource(hRes));
    ASSERT(pPackedDib != NULL);
    if(!pPackedDib)
        return NULL;
        
    // build a DDB header for  pDC

    BITMAPINFOHEADER bmihDDB;
    memcpy(&bmihDDB, pPackedDib, sizeof(BITMAPINFOHEADER));
    bmihDDB.biBitCount = GetDeviceCaps( hDC, BITSPIXEL);
    bmihDDB.biClrUsed = 0;
    bmihDDB.biClrImportant = 0;

    
    // create the DDB
    int iColorTableSize =
        (1 << pPackedDib->bmiHeader.biBitCount) * sizeof(RGBQUAD);
    void* pBits =
        ((char*)pPackedDib) + pPackedDib->bmiHeader.biSize + iColorTableSize;
    HBITMAP hDDB = CreateDIBitmap(hDC,
                                &bmihDDB,
                                CBM_INIT,
                                pBits,
                                pPackedDib,
                                DIB_RGB_COLORS);
    
    // done, clean up
	UnlockResource(hRes);
    BOOL bResult = FreeResource(hRes);
    
    return hDDB;
}

static int FindColorIndex(LPRGBQUAD pColors, int nNumColors, COLORREF color)
{
	for(int i = 0; i < nNumColors; i++)
	{
		if(pColors[i].rgbRed == GetRValue(color) &&
		   pColors[i].rgbGreen == GetGValue(color) &&
		   pColors[i].rgbBlue == GetBValue(color)) {
				return i;
		}
	}
	return -1;
}

// Loads a bitmap and returns the handle. Retrieves the color of the
// first pixel in the image and replaces that entry in the color table
// with clrBackground
HBITMAP WFE_LoadTransparentBitmap(HINSTANCE hInstance, HDC hDC, COLORREF clrBackground, COLORREF clrTransparent,
								  HPALETTE hPalette, UINT nResID)
{
	// Find the resource
	HRSRC	hrsrc = FindResource(hInstance, MAKEINTRESOURCE(nResID), RT_BITMAP);

	assert(hrsrc);
	if (!hrsrc)
		return NULL;

	// Get a handle for the resource
	HGLOBAL	hglb = LoadResource(hInstance, hrsrc);
	if (!hglb)
		return NULL;

	// Get a pointer to the BITMAPINFOHEADER
	LPBITMAPINFOHEADER	lpbi = (LPBITMAPINFOHEADER)LockResource(hglb);

	// We expect a 4-bpp or 8-bpp image only
	assert(lpbi->biBitCount == 4 || lpbi->biBitCount == 8);
	if (lpbi->biBitCount != 4 && lpbi->biBitCount != 8) {
		UnlockResource(hglb);
		FreeResource(hglb);
		return NULL;
	}

#ifdef XP_WIN32
	// make copy of BITMAPINFOHEADER so we can modify the color table
	int nColorTableSize = 1 << lpbi->biBitCount;
	UINT nSize = CASTUINT(lpbi->biSize + nColorTableSize * sizeof(RGBQUAD));

	LPBITMAPINFOHEADER lpBitmapInfo = (LPBITMAPINFOHEADER)::malloc(nSize);
	if (lpBitmapInfo == NULL)
		return NULL;
	memcpy(lpBitmapInfo, lpbi, nSize);

	// color table is in RGBQUAD DIB format
	LPRGBQUAD pColors =
		(LPRGBQUAD)((LPSTR)lpBitmapInfo + lpBitmapInfo->biSize);

	LPRGBQUAD pOldColors =
		(LPRGBQUAD)((LPSTR)lpbi + lpbi->biSize);

	UINT	nClrUsed = lpbi->biClrUsed == 0 ? 1 << lpbi->biBitCount : (UINT)lpbi->biClrUsed;
	LPBYTE	lpBits = (LPBYTE)(pOldColors + nClrUsed);


#else
	LPRGBQUAD	pColors = (LPRGBQUAD)((LPSTR)lpbi + (WORD)lpbi->biSize);
	UINT	nClrUsed = lpbi->biClrUsed == 0 ? 1 << lpbi->biBitCount : (UINT)lpbi->biClrUsed;
	LPBYTE	lpBits = (LPBYTE)(pColors + nClrUsed);
#endif


	// Munge the color table entry
//	int	nIndex = lpbi->biBitCount == 8 ? *lpBits : *lpBits & 0xF;
	int nIndex = FindColorIndex(pColors, nClrUsed, clrTransparent);

	pColors[nIndex].rgbGreen = GetGValue(clrBackground);
	pColors[nIndex].rgbRed = GetRValue(clrBackground);
	pColors[nIndex].rgbBlue = GetBValue(clrBackground);

	// Create the DDB
	HBITMAP		hBitmap;
	HPALETTE hOldPalette = SelectPalette(hDC, hPalette, FALSE);


#ifdef XP_WIN32
	hBitmap = CreateDIBitmap(hDC, lpBitmapInfo, CBM_INIT, lpBits,
		(LPBITMAPINFO)lpBitmapInfo, DIB_RGB_COLORS);

	::free(lpBitmapInfo);
#else
	hBitmap = CreateDIBitmap(hDC, lpbi, CBM_INIT, lpBits,
		(LPBITMAPINFO)lpbi, DIB_RGB_COLORS);

	//Restore the bitmap in case we want to go through this process again
	pColors[nIndex].rgbRed = GetRValue(clrTransparent);
	pColors[nIndex].rgbGreen = GetGValue(clrTransparent);
	pColors[nIndex].rgbBlue = GetBValue(clrTransparent);
#endif
	// The parent should unselect the palette when they got destroy.
		SelectPalette(hDC, hOldPalette, TRUE);
	// Release the resource
	UnlockResource(hglb);
	FreeResource(hglb);

	return hBitmap;
}

void WFE_InitializeUIPalette(HDC hDC)
{
	IL_IRGB transparentColor;
	transparentColor.index = 0;
	transparentColor.red = 255;
	transparentColor.green = 0;
	transparentColor.blue = 255;
	if(ghPalette == NULL) {
		ghPalette = CDCCX::CreateColorPalette(hDC, transparentColor, 8);

	}
}

BOOL WFE_IsGlobalPalette(HPALETTE hPal)
{

	return (hPal == ghPalette);
}

HPALETTE WFE_GetUIPalette(CWnd* parent)
{
	// for normal case.
	if (!parent)
		return ghPalette;
	else if (parent->IsKindOf(RUNTIME_CLASS(CGenericFrame))) {
		CWinCX* pcxWin = ((CGenericFrame*)parent)->GetMainWinContext();
		if (pcxWin) {
			HPALETTE hPalette = pcxWin->GetPalette();
			if (hPalette) return hPalette;
			else return	ghPalette;
		}
		else return ghPalette;
	}
	else if (parent->IsKindOf(RUNTIME_CLASS(CInPlaceFrame))) {	 // for OLE
		// XXX write me.
		CWinCX *pWinCX = (CWinCX*)FEU_GetLastActiveFrameContext();
		if (pWinCX) {
			HPALETTE hPalette = pWinCX->GetPalette();
			if (hPalette) return hPalette;
			else return	ghPalette;
		}
		else return	ghPalette;

	}
	else // for other case. 
		return ghPalette;
}

void WFE_DestroyUIPalette(void)
{
	if(ghPalette)
		DeleteObject(ghPalette);
}

typedef struct {
	COLORREF clrBackground;
	HBITMAP bitmap;
} BitmapMapEntry;

// if bTransparent is FALSE then clrBackground and clrTransparent are ignored
HBITMAP WFE_LookupLoadAndEnterBitmap(HDC hDC, UINT nID, BOOL bTransparent, HPALETTE hPalette, 
									 COLORREF clrBackground, COLORREF clrTransparent)
{

	HINSTANCE hInst = AfxGetResourceHandle();
	HBITMAP hBitmap;
	BitmapMapEntry *pEntry = NULL;

	if(!gBitmapMap.Lookup(nID, (void*&)pEntry) || pEntry->clrBackground != clrBackground)
	{
		HPALETTE hOldPalette = SelectPalette(hDC, hPalette, FALSE);

		if(bTransparent)
			hBitmap = WFE_LoadTransparentBitmap(hInst, hDC, clrBackground, clrTransparent,
												hPalette, nID);
		else
		{
			hBitmap = wfe_LoadBitmap(hInst, hDC, MAKEINTRESOURCE(nID));
		}
		if(!pEntry)
			pEntry = new BitmapMapEntry;
		pEntry->clrBackground = clrBackground;
		pEntry->bitmap = hBitmap;
		gBitmapMap.SetAt(nID, (void*&)pEntry);
		::SelectPalette(hDC, hOldPalette, TRUE);
	}

	return pEntry->bitmap;
}

void WFE_RemoveBitmapFromMap(UINT nID)
{
	BitmapMapEntry *pEntry = NULL;

	if(gBitmapMap.Lookup(nID, (void*&)pEntry))
	{
		::DeleteObject(pEntry->bitmap);
		delete pEntry;
		gBitmapMap.RemoveKey(nID);
	}
}

void WFE_DestroyBitmapMap(void)
{
	POSITION pos;
	WORD dwKey;
	BitmapMapEntry *pEntry = NULL;

	// also destroy bitmaps;
	pos = gBitmapMap.GetStartPosition();

	while(pos)
	{
		gBitmapMap.GetNextAssoc(pos, dwKey,(void *&)pEntry);
		DeleteObject(pEntry->bitmap);
		delete pEntry;
	}

	gBitmapMap.RemoveAll();


}

HFONT WFE_GetUIFont(HDC hDC)
{
#ifdef XP_WIN32
		if (sysInfo.m_bWin4 == TRUE)
			ghFont = (HFONT) GetStockObject(DEFAULT_GUI_FONT);
		else 
#endif
		if (sysInfo.m_bDBCS == FALSE)
			ghFont = (HFONT) GetStockObject(ANSI_VAR_FONT);
		else {
			if (ghFont == NULL) {
				LOGFONT lf;
				XP_MEMSET(&lf,0,sizeof(LOGFONT));
				lf.lfPitchAndFamily = FF_SWISS;
				lf.lfCharSet = IntlGetLfCharset(0);
   				lf.lfHeight = -MulDiv(8, GetDeviceCaps(hDC, LOGPIXELSY), 72);
				strcpy(lf.lfFaceName, IntlGetUIPropFaceName(0));
				lf.lfWeight = 400;
				ghFont = theApp.CreateAppFont( lf );
			}
		}
	return ghFont;
}

void WFE_DestroyUIFont(void)
{
}

CFrameWnd *WFE_GetOwnerFrame(CWnd *pWnd)
{
	return pWnd->GetTopLevelFrame();
}

int WFE_FindMenu(CMenu *pMenu, CString& menuItemName)
{
	if (!pMenu)
	    return -1;

	int nCount = pMenu->GetMenuItemCount();
	
	for(int i = 0; i < nCount; i++)
	{
		char lpszMenuString[64];

		pMenu->GetMenuString(i, lpszMenuString, 64, MF_BYPOSITION);

		if(strcmp( lpszMenuString, menuItemName) == 0)
			return i;

	}

	return -1;

}

int WFE_FindMenuItem(CMenu *pMenu, UINT nCommand)
{
	if (!pMenu)
	    return -1;

	int nCount = pMenu->GetMenuItemCount();
	
	for(int i = 0; i < nCount; i++)
	{

		UINT nID = pMenu->GetMenuItemID(i);

		if(nID == nCommand)
			return i;

	}

	return -1;
}

#ifdef FEATURE_LOAD_BITMAP_FILES
#include "loadbmps.i00"
#endif

HBITMAP WFE_LoadBitmapFromFile (const char* szFileName)
{
    HBITMAP hRetval = NULL;
    
#ifdef FEATURE_LOAD_BITMAP_FILES
    hRetval = wfe_loadbitmapfromfile(szFileName);
#else
#ifdef _WIN32
    //  This only works on >= 95 and NT4.
    hRetval = (HBITMAP)LoadImage(NULL, szFileName, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE);
#endif
#endif
    
    return(hRetval);
}

void WFE_FillSolidRect(CDC *pDC, CRect crRect, COLORREF rgbFill)	{
#ifdef XP_WIN32
	pDC->FillSolidRect(crRect, rgbFill);
#else
	::SetBkColor(pDC->m_hDC, rgbFill);
	::ExtTextOut(pDC->m_hDC, 0, 0, ETO_OPAQUE, crRect, NULL, 0, NULL);
#endif
}

#define STRING_ELLIPSES _T("...")

int WFE_DrawTextEx( int iCSID, HDC hdc, LPTSTR lpchText, int cchText, LPRECT lprc, UINT dwDTFormat,
					UINT dwMoreFormat )
{
	if (cchText < 0)
		cchText = _tcslen(lpchText);

#ifdef XP_WIN32
	if ((dwMoreFormat == WFE_DT_CROPRIGHT) && sysInfo.m_bWin4) {
		dwDTFormat |= DT_END_ELLIPSIS;
		return CIntlWin::DrawTextEx(iCSID, hdc, (LPTSTR) lpchText, cchText, lprc,
									dwDTFormat, NULL);
	}
#endif

	LPTSTR pString = NULL;
    CSize cs = CIntlWin::GetTextExtent( iCSID, hdc, lpchText, cchText);

    if (cs.cx > (lprc->right - lprc->left) ) {
		int iNumToRemove = 0;
	    int iEllipsesLength = _tcslen(STRING_ELLIPSES);

		pString = new _TCHAR[cchText + iEllipsesLength + 2];
		if (!pString) {
			ASSERT(0);
			return 0;
		}
		_tcscpy(pString,lpchText);
		while ( cs.cx > (lprc->right - lprc->left) ) {
			if (++iNumToRemove >= cchText) {
				delete [] pString;
				return cs.cy;
			}
			switch (dwMoreFormat) {
				case WFE_DT_CROPRIGHT:
					_tcscpy(&pString[cchText - iNumToRemove], STRING_ELLIPSES);
					break;
				case WFE_DT_CROPCENTER:
					{
						int iLeft = (cchText - iNumToRemove) / 2;
						int iRight = iLeft + ((cchText - iNumToRemove)%2);
						_tcscpy(&pString[iLeft], STRING_ELLIPSES);
						_tcscpy(&pString[iLeft+iEllipsesLength],
								&lpchText[cchText-iRight]);
					}
					break;
				case WFE_DT_CROPLEFT:
						_tcscpy(pString, STRING_ELLIPSES);
						_tcscpy(&pString[iEllipsesLength],
								&lpchText[iNumToRemove]);
					break;
			}
			cs = CIntlWin::GetTextExtent( iCSID, hdc, pString, strlen(pString));
		}
		lpchText = pString;
    }

    int res = CIntlWin::DrawText( iCSID, hdc, (LPTSTR) lpchText, -1,  lprc, dwDTFormat );

	if (pString)
    	delete [] pString;
	
	return res;
	
}

#ifdef __BORLANDC__
	#define	_S_IFREG	S_IFREG
#endif

// If pszLocalName is not NULL, we return the full pathname
//   in local platform syntax, even if file is not found.
//   Caller must free this string.
// Returns TRUE if file already exists
//
MODULE_PRIVATE Bool
XP_ConvertUrlToLocalFile( const char * szURL, char **pszLocalName)
{
    BOOL bFileFound = FALSE;
    // Default assumes no file found - no local filename
    if ( pszLocalName ) {
        *pszLocalName = NULL;
    }

    // Must have "file:" URL type and at least 1 character after "///"
    if ( szURL == NULL || !NET_IsLocalFileURL((char*)szURL) || XP_STRLEN(szURL) <= 8 ) {
        return FALSE;
    }

    // Extract file path from URL: e.g. "/c|/foo/file.html"
    char *szPath = NET_ParseURL( szURL, GET_PATH_PART);
    if ( szPath == NULL ) {
        return FALSE;
    }

  	// Get filename - skip over initial "/"
    char *szFileName;
    if (szPath[0] == '/' && szPath[1] == '/') {
      // bug 35222, if url is a shared Windows (NT or Win95) directory using UNC name,
      // don't skip over the first slash, WH_FileName will do the right thing.
      // I have the suspicion that we should never be skipping the first slash, that
      // WH_FileName knows what to do, but I don't want to make a risky change now.
      szFileName = WH_FileName(szPath, xpURL);
    }
    else {
      szFileName = WH_FileName(szPath+1, xpURL);
    }
    XP_FREE(szPath);

    // Test if file exists
    XP_StatStruct statinfo; 
    if ( -1 != XP_Stat(szFileName, &statinfo, xpURL) &&
        statinfo.st_mode & _S_IFREG ) {
        bFileFound = TRUE;
    }

    if ( pszLocalName ) {
        // Pass string to caller
        // (we didn't change it, so there's no need to XP_STRDUP)
        *pszLocalName = szFileName;
    }
    return bFileFound;
}


// Create a backup filename for renaming current file before saving data
//    Input should be be URL file type "file:///..."
//    Caller must free the string with XP_FREE
//
MODULE_PRIVATE char *
XP_BackupFileName( const char * szURL )
{
    char * szFileName;
    // Must have "file:" URL type and at least 1 character after "///"
    if ( szURL == NULL || !NET_IsLocalFileURL((char*)szURL) || XP_STRLEN(szURL) <= 8 ) {
        return NULL;
    }
    // Add extra space for 8+3 temp filename, but subtract for "file:///"
    szFileName = (char *) XP_ALLOC((XP_STRLEN(szURL)+5)*sizeof(char));
    if ( szFileName == NULL ) {
        return NULL;
    }
	// Get filename but ignore "file:///"
    {
	char* filename;
  if (szURL[8] == '/') {
    // bug 35222, if url is a shared Windows (NT or Win95) directory using UNC name,
    // don't skip over the first slash, WH_FileName will do the right thing.
    // I have the suspicion that we should never be skipping the first slash, that
    // WH_FileName knows what to do, but I don't want to make a risky change now.
    filename = WH_FileName(szURL+7, xpURL);
  }
  else {
    filename = WH_FileName(szURL+8, xpURL);
  }
	if (!filename) return NULL;
	XP_STRCPY(szFileName,filename);
	XP_FREE(filename);
    }

#ifdef XP_WIN16
    char * pEnd = szFileName + XP_STRLEN( szFileName );
    while ( *pEnd != '\\' && pEnd != szFileName ) {
        if ( *pEnd == '.' ) {
            // Truncate at current extension
            *pEnd = '\0';
        }
        pEnd--;
    }
#endif
    // Add extension to the filename
    XP_STRCAT( szFileName, ".BAK" );
    return szFileName;
}

// Copy file routine for Editor - ask user permission
//  to overwrite existing file.
// Params may be local file format or "file://" URL format,
//   and assumes full pathnames
BOOL FE_CopyFile(const char *cpSrc, const char *cpTarg)
{
    char * szSrc = NULL;
    char * szTarg = NULL;
    BOOL bFreeSrc = FALSE;
    BOOL bFreeTarg = FALSE;
    BOOL bSrcExists = TRUE;
    BOOL bTargExists = FALSE;
    XP_StatStruct statinfo; 
    CString csMsg;
    BOOL bResult = FALSE;

    if ( NET_IsLocalFileURL((char*)cpSrc) ){
        bSrcExists = XP_ConvertUrlToLocalFile(cpSrc, &szSrc);        
        bFreeSrc = (szSrc != NULL);
    } else {
        // Use supplied name - assume its a file
        szSrc = (char*)cpSrc;
        bSrcExists = (-1 != XP_Stat(cpSrc, &statinfo, xpURL)) &&
                     (statinfo.st_mode & _S_IFREG);
    }

    if ( bSrcExists ){
        if ( NET_IsLocalFileURL((char*)cpTarg) ){
            bTargExists = XP_ConvertUrlToLocalFile(cpTarg, &szTarg);
            bFreeTarg = (szTarg != NULL);
        } else {
            bTargExists = (-1 != XP_Stat(cpTarg, &statinfo, xpURL)) &&
                          (statinfo.st_mode & _S_IFREG);
            // Use supplied name - assume its a file
            szTarg = (char*)cpTarg;
        }
        if ( bTargExists ) {
            AfxFormatString1(csMsg, IDS_WARN_REPLACE_FILE, szTarg);  
            if ( IDYES == MessageBox( NULL, csMsg, szLoadString(IDS_COPY_FILE), 
                                      MB_YESNO | MB_ICONQUESTION) ) {
                bResult = WFE_CopyFile(szSrc, szTarg);
            }
        } else {
            bResult = WFE_CopyFile(szSrc, szTarg);
        }
        if ( !bResult ) {
            AfxFormatString1(csMsg, IDS_ERR_COPY_FILE, szSrc);  
            MessageBox( NULL, csMsg, szLoadString(IDS_COPY_FILE), MB_OK | MB_ICONSTOP);
        }
    }
    else {
        // Can't copy if no source!
        AfxFormatString1(csMsg, IDS_ERR_SRC_NOT_FOUND, szSrc);  
        MessageBox( NULL, csMsg, szLoadString(IDS_COPY_FILE), MB_OK | MB_ICONSTOP);
    }

    if ( bFreeSrc ) {
        XP_FREE(szSrc);
    }
    if ( bFreeTarg ) {
        XP_FREE(szTarg);
    }
    return bResult;
}

// CLM: Some simple functions to aid in COLORREF to LO_Color conversions

// Fill a LoColor structure
void WFE_SetLO_Color( COLORREF crColor, LO_Color *pLoColor )
{
    if ( pLoColor ) {
    	pLoColor->red = GetRValue(crColor);
        pLoColor->green = GetGValue(crColor);
        pLoColor->blue = GetBValue(crColor);
    }
}

// This allocates a new LoColor structure if it
//   does not exist
void WFE_SetLO_ColorPtr( COLORREF crColor, LO_Color **ppLoColor )
{
	if ( ppLoColor == NULL ) {
        return;
    }
	if ( *ppLoColor == NULL ) {
        *ppLoColor = XP_NEW( LO_Color );
    }
	(*ppLoColor)->red = GetRValue(crColor);
    (*ppLoColor)->green = GetGValue(crColor);
    (*ppLoColor)->blue = GetBValue(crColor);
}

// Convert a LO_Color structure or get the default
//  default document colors
COLORREF WFE_LO2COLORREF( LO_Color * pLoColor, int iColorIndex )
{
    COLORREF crColor = 0;
    if ( pLoColor ) {
        crColor = RGB( pLoColor->red, pLoColor->green, pLoColor->blue);
    } 
    else {
        crColor = RGB(lo_master_colors[iColorIndex].red,
                      lo_master_colors[iColorIndex].green,
                      lo_master_colors[iColorIndex].blue);
    }
    return crColor;
}

// Parse a "#FFEEAA" style color string into colors make a COLORREF
BOOL WFE_ParseColor(char *pRGB, COLORREF * pCRef )
{
    uint8 red, green, blue;
    BOOL bColorsFound = LO_ParseRGB(pRGB, &red, &green, &blue);
    if ( bColorsFound ) {
        *pCRef = RGB( red, green, blue);
    }
    return bColorsFound;
}

// Helper to clear data
void WFE_FreeLO_Color(LO_Color_struct **ppLo)
{
    if ( *ppLo ) {
        XP_FREE( *ppLo );
        *ppLo = NULL;
    }
}

#ifdef EDITOR
// Get current color at caret or within a selection
COLORREF WFE_GetCurrentFontColor(MWContext * pMWContext)
{
    COLORREF crColor = DEFAULT_COLORREF;

    EDT_CharacterData *pCharData = EDT_GetCharacterData(pMWContext);

    if( !pMWContext ){
        return crColor;
    }

    if( pCharData && 0 != (pCharData->mask & TF_FONT_COLOR) ){
        // We are sure about the color
        if( pCharData->pColor ){
            // If we have a LO_Color struct, then use its color
            crColor = WFE_LO2COLORREF( pCharData->pColor, LO_COLOR_FG );
        }
    } else {
        // We are not sure of color
        crColor = MIXED_COLORREF;
    }
    if(pCharData) EDT_FreeCharacterData(pCharData);
    return crColor;
}
#endif // EDITOR

/******************************************************************************/
/* Thread-safe entry points: */

extern PRMonitor* _pr_TempName_lock;

char *
WH_TempName(XP_FileType type, const char * prefix)
{
	static char buf[_MAX_PATH];	/* protected by _pr_TempName_lock */
	char* result;
#ifdef NSPR
	XP_ASSERT(_pr_TempName_lock);
	PR_EnterMonitor(_pr_TempName_lock);
#endif
	result = XP_STRDUP(xp_TempFileName(type, prefix, NULL, buf));
#ifdef NSPR
	PR_ExitMonitor(_pr_TempName_lock);
#endif
	return result;
}

// The caller is responsible for XP_FREE()ing the return string
PUBLIC char *
WH_FileName (const char *name, XP_FileType type)
{
	char* myName;
	char* result;
	/*
	** I'm not sure this lock is really needed by windows, but just
	** to be safe:
	*/
#ifdef NSPR
	//	XP_ASSERT(_pr_TempName_lock);
	//	PR_EnterMonitor(_pr_TempName_lock);
#endif
	result = xp_FileName(name, type, &myName);
#ifdef NSPR
	//	PR_ExitMonitor(_pr_TempName_lock);
#endif
	return myName;
}

int32 FE_LPtoDPoint(MWContext *pContext, int32 logicalPoint)    
{
      int32 iRetval = logicalPoint;

      CAbstractCX *pAbstract = ABSTRACTCX(pContext);
      if(pAbstract && pAbstract->IsDCContext())       {
              CDCCX *pCx = CXDC(pContext);
              HDC hDC = pCx->GetContextDC();
              if(hDC) {
                      POINT tempPoint;
                      tempPoint.x = 1;
                      tempPoint.y = logicalPoint;
                      // it is printing, convert them from device point to logical point for printers
                      if( TRUE == ::LPtoDP(hDC, &tempPoint, 1) )
                              iRetval = tempPoint.y;
              }
      }

      return(iRetval);
}

/******************************************************************************/

#ifdef XP_WIN16
static UINT PASCAL DirOpenHookProc(
        HWND hDlg,                /* window handle of the dialog box */
        UINT message,             /* type of message                 */
        UINT wParam,            /* message-specific information    */
        LONG lParam)
#else
static BOOL APIENTRY DirOpenHookProc(
        HWND hDlg,                /* window handle of the dialog box */
        UINT message,             /* type of message                 */
        UINT wParam,            /* message-specific information    */
        LONG lParam)
#endif
{
    switch (message)
    {
        case WM_INITDIALOG:
        //We must put something in this field, even though it is hidden.  This is
        //because if this field is empty, or has something like "*.txt" in it,
        //and the user hits OK, the dlg will NOT close.  We'll jam something in
        //there (like "k5bg") so when the user hits OK, the dlg terminates.
        //Note that we'll deal with the "k5bg" during return processing (see below)
            SetDlgItemText(hDlg, edt1, "k5bg");
        break;
    }
    return (FALSE);
}

/////////////////////////////////////////////////////////////////////////////
// CDirDialog		   
CDirDialog::CDirDialog(CWnd* pParent, LPCTSTR pInitDir)
	: CFileDialog(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, NULL, pParent)
{
	m_szIniFile[0] = '\0';

	m_ofn.lStructSize		= sizeof(OPENFILENAME);
	m_ofn.hwndOwner			= pParent->GetSafeHwnd();
	m_ofn.hInstance			= AfxGetResourceHandle();
	m_ofn.lpstrFile			= m_szIniFile;
	m_ofn.nMaxFile			= 132;
	m_ofn.lpstrFilter		= NULL; 
	m_ofn.lpstrCustomFilter = NULL;
	m_ofn.lpstrInitialDir	= pInitDir; 
	m_ofn.lpstrFileTitle	= NULL;
	m_ofn.nMaxFileTitle		= 132;
	m_ofn.lpstrDefExt		= (LPSTR)NULL;
	m_ofn.lpstrDefExt		= "";
#ifdef XP_WIN32
	m_ofn.Flags = OFN_NOCHANGEDIR | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK |OFN_NONETWORKBUTTON |OFN_LONGNAMES; 
	m_ofn.lpfnHook = (LPOFNHOOKPROC)DirOpenHookProc;
#else
	m_ofn.Flags = OFN_NOCHANGEDIR | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK; 
	m_ofn.lpfnHook = &DirOpenHookProc;
#endif
	m_ofn.lpTemplateName = (LPTSTR)MAKEINTRESOURCE(FILEOPENORD);

}

void CDirDialog::DoDataExchange(CDataExchange* pDX)
{
	CFileDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDirDialog, CFileDialog)
END_MESSAGE_MAP()

void FE_PokeLibSecOnIdle()
{
    long	lVal;
    DWORD	dwVal;
    HWND	hWnd;
	BYTE	buf[20];
	int		nBytes;

	nBytes = RNG_GetNoise(buf, sizeof(buf));
    RNG_RandomUpdate(buf, nBytes);

    dwVal = GetMessagePos();			// mouse position based
    RNG_RandomUpdate(&dwVal, sizeof(dwVal));

    lVal = GetMessageTime();			// time from startup to last message
    RNG_RandomUpdate(&lVal, sizeof(lVal));

    lVal = GetTickCount();			// we are about to go idle so this is probably
    RNG_RandomUpdate(&lVal, sizeof(lVal));	//   different from the one above

    hWnd = GetClipboardOwner(); 		// 2 or 4 bytes
    RNG_RandomUpdate((void *)&hWnd, sizeof(HWND));
}


void WFE_DrawDragRect(CDC* pdc, LPCRECT lpRect, SIZE size,
	LPCRECT lpRectLast, SIZE sizeLast, CBrush* pBrush, CBrush* pBrushLast)
{
#ifdef XP_WIN32
	pdc->DrawDragRect(lpRect, size, lpRectLast, sizeLast, pBrush, pBrushLast);
#endif
}

void FE_GetDocAndWindowPosition(MWContext * context, int32 *pX, int32 *pY, 
    int32 *pWidth, int32 *pHeight )
{
	if(context && ABSTRACTCX(context) &&
        ABSTRACTCX(context)->IsWindowContext() == TRUE)	{
		CPaneCX *pPaneCX = VOID2CX(context->fe.cx, CPaneCX);

        *pX = pPaneCX->GetOriginX(); 
        *pY = pPaneCX->GetOriginY();                             

    	//	When we report the size to layout, we must always take care to subtract
    	//		for the size of the scrollbars if we have dynamic or always on
    	//		scrollers.
    	if(pPaneCX->DynamicScrollBars() == FALSE 
    	        && pPaneCX->IsHScrollBarOn() == FALSE 
    	        && pPaneCX->IsVScrollBarOn() == FALSE)	{
    	    *pWidth = pPaneCX->GetWidth();
    	    *pHeight = pPaneCX->GetHeight();
    	}
    	else	{
    		*pWidth = pPaneCX->GetWidth() - sysInfo.m_iScrollWidth;
    		*pHeight = pPaneCX->GetHeight() - sysInfo.m_iScrollHeight;
    	}
    }
	else	{
		//	No window here....
		ASSERT(0);
		*pX = *pY = *pHeight = *pWidth = 0;
	}
}

/* Convert an HTML SIZE param value (1-7) into POINT-SIZE value */
int16 FE_CalcFontPointSize(MWContext * pMWContext, intn iSize, XP_Bool bFixedWidth)
{
    if( pMWContext /*&& wfe_iFontSizeMode == ED_FONTSIZE_ADVANCED*/ )
    {
	    EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(pMWContext);
        int iBaseSize = bFixedWidth ? pEncoding->iFixSize : pEncoding->iPropSize;

        CDCCX *pDC = VOID2CX(pMWContext->fe.cx, CDCCX);

        if( pDC && iSize >= MIN_FONT_SIZE && iSize <= MAX_FONT_SIZE )
            return (int16)(pDC->CalcFontPointSize(iSize, iBaseSize));
    }
    return 0;    
}
