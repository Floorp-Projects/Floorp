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
///////////////////////////////////////////////////////////////////////////////
//
// Fileio.cpp
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
// 02/05/97    xxxxxxxxxxxxxx    Define Native API for win3.x
//             xxxxxxxxxxxxxx    Define Native API for win95 & winNT
///////////////////////////////////////////////////////////////////////////////

#include <npapi.h>
#include "plugin.h"

// resource include
#ifdef WIN32 // **************************** WIN32 *****************************
#include "resource.h"
#else        // **************************** WIN16 *****************************
#include "asw16res.h"
#endif // !WIN32

#ifdef WIN32  
// ********************* Win32 includes **************************
#include <winbase.h>
#else  // win16
// ********************* Win16 includes **************************
#include <windows.h>
#include <tchar.h>
#include <dos.h>
#include <errno.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>  			// for _S_IWRITE etc..
#include <stdio.h>
#include "helper16.h"
// ********************* Win16 constants ************************** 
#define MAX_FILELENGTH		16000		// max filesize when reading in entire file content
#define MAX_PATH			255
#define LPCTSTR				LPCSTR
#define LPTSTR				LPSTR
#define DEF_ACCINFOFILENAME	"acctinfo.txt"
// ******* Win16 equivalent of error constants for WinExec() ******
#define ERROR_FILE_NOT_FOUND    2
#define ERROR_PATH_NOT_FOUND    3
#define ERROR_BAD_FORMAT        11
// ********************* Win16 vars *******************************
#endif

// ======================== general includes ======================
#include <string.h>
#include <commdlg.h>    // includes common dialog functionality
#include <dlgs.h>       // includes common dialog template defines
#include <cderr.h>      // includes the common dialog error codes
#include "errmsg.h"

// ========================= java include  ========================
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"

// ========================= constants ============================
#define INI_BUFFER_SIZE 	255		// INI key value buffer size
#define REGSERV_SR			"RegServ.SR" 

JRIGlobalRef	g_globalRefFileList = NULL;

//********************************************************************************
//
// GetStringPlatformChars
//
// converts java string to c string depends on platform OS chars
//********************************************************************************
const char *GetStringPlatformChars(JRIEnv *env,
								   struct java_lang_String *JSstring) 
{
	const char *newString = NULL;

	if (JSstring != NULL)
		newString = JRI_GetStringPlatformChars(env, JSstring, NULL, 0);

//	JRI_ExceptionDescribe(env);
	JRI_ExceptionClear(env);

	return (const char *)newString;
}

//********************************************************************************
// native method:
//
// GetNameValuePair
//
// retrieves a string from the specified section in an initialization file
//********************************************************************************
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetNameValuePair(JRIEnv *env,
												struct netscape_npasw_SetupPlugin* ThisPlugin,
												struct java_lang_String *JSfile,
												struct java_lang_String *JSsection,
												struct java_lang_String *JSname)

{
	int err = 0;
	struct java_lang_String *value;
	char *buf=NULL;     //allocate 1k, in case we return the entire file
	short len = 0;
	unsigned long ret = 0;

	// converts plugin strings to c strings
	const char *file = NULL, *section = NULL, *name = NULL;

	// converts plugin strings to c strings
	if (JSfile != NULL)
		file = GetStringPlatformChars(env, JSfile);
	if (JSsection != NULL)
		section = GetStringPlatformChars(env, JSsection);
	if (JSname != NULL)
		name = GetStringPlatformChars(env, JSname);

	if ((file) && (!section) && (!name)) {
		
		// both section and name are NULL, means return the entire file contents!
		// the file may not be in INI format
#ifdef WIN32 // ********************* Win32 **************************
		HANDLE hFile=NULL;

		// opens the file for READ
		hFile = CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile != INVALID_HANDLE_VALUE) {  // openned file is valid
			
			OVERLAPPED overlap;
			DWORD bytesRead;
			
			memset(&overlap, 0, sizeof(overlap));
			
			// gets file size
			DWORD fileSize = GetFileSize(hFile, NULL);

#else        // ********************* Win16 **************************
		
		unsigned int bytesRead = BUFSIZ;
		int hFile = _open(file, _O_RDONLY|_O_BINARY, _S_IREAD);

		if(hFile != -1)  
		{
			long fileSize = _filelength(hFile);
			if (fileSize == -1L || 
				fileSize >= MAX_FILELENGTH)	// abort if file is longer then threshold
				return NULL;				// to avoid hitting 16K limit in JRI_NewStringPlatform() 
#endif
			len = (short) fileSize;

			// allocate enough for the file buffer
			buf = (char *)malloc((size_t)(sizeof(char) * (fileSize+1)));
			if (!buf)
				return NULL;

			// read the file from beginning
#ifdef WIN32 // ********************* Win32 **************************
			if (ReadFile(hFile, buf, fileSize, &bytesRead, NULL) == FALSE)
				err = GetLastError();
#else        // ********************* Win16 **************************
			if ((bytesRead = _read(hFile, buf, (unsigned int) fileSize)) <= 0)
				bytesRead = 0;
			
			assert(bytesRead <= fileSize);
#endif
			buf[bytesRead] = '\0';

			// close file
#ifdef WIN32 // ********************* Win32 **************************
			CloseHandle(hFile);
#else        // ********************* Win16 **************************
			int nResult = _close(hFile);
			assert(nResult == 0);
#endif
		} else {		// invalid file!

			      
#ifdef WIN32 // ********************* Win32 **************************
			err = GetLastError();
#endif
			return NULL;
		}
	} else {		// read an INI entry

		buf = (char *)malloc(sizeof(char) * (INI_BUFFER_SIZE + 1));
		assert(buf);
		if (!buf) 
			return NULL;

		ret = GetPrivateProfileString(section, name, "", buf, INI_BUFFER_SIZE, file);

		// len = (short)ret + 1; // This appears to be incorrect, causes null char in string
		len = (short)ret;
	}

	value = JRI_NewStringPlatform(env, buf, len, NULL, 0);
	free(buf);
		
	return (struct java_lang_String *)value;
}


//********************************************************************************
// native method:
//
// SetNameValuePair
//
// sets the value of a key string in the specified section in a *.SR or *.INI file
//********************************************************************************
JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fSetNameValuePair(JRIEnv* env,
												struct netscape_npasw_SetupPlugin *ThisPlugin,
												struct java_lang_String *JSfile,
												struct java_lang_String *JSsection,
												struct java_lang_String *JSname,
												struct java_lang_String *JSvalue) 
{
	const char *file = NULL, *section = NULL, *name = NULL, *value = NULL;

	// convert JavaScript string to c strings
	if (JSfile != NULL)
		file = GetStringPlatformChars(env, JSfile);
	if (JSsection != NULL)
		section = GetStringPlatformChars(env, JSsection);
	if (JSname != NULL)
		name = GetStringPlatformChars(env, JSname);
	if (JSvalue != NULL)
		value = GetStringPlatformChars(env, JSvalue);

	// if file doesn't exist, create the file
#ifdef WIN32 // ********************* Win32 **************************
	
	WIN32_FIND_DATA *lpFileInfo = new WIN32_FIND_DATA;
	HANDLE hfile = FindFirstFile(file, lpFileInfo);

	if (hfile == INVALID_HANDLE_VALUE) {

		SECURITY_ATTRIBUTES secAttrib;
		memset(&secAttrib, 0, sizeof(SECURITY_ATTRIBUTES));
		secAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
		secAttrib.lpSecurityDescriptor = NULL;
		secAttrib.bInheritHandle = FALSE;

		HANDLE hfile = CreateFile (file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
								   &secAttrib, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hfile)
			CloseHandle(hfile);

	} else {

		FindClose(hfile);

	}

	delete lpFileInfo;

	// writes the value out to file
	BOOL bResult = WritePrivateProfileString(section, name, value, file);
	assert(bResult);

#else	// ************************************ WIN 16 ********************************
   
	if (file && *file)		// write if file name is valid
	{
		// writes the value out to file, if file does not exist, it'll be created
		WritePrivateProfileString(section, name, value, file);
	}
#endif  // !WIN16
}


//********************************************************************************
// native method:
//
// GetFolderContents
//
// returns a list of file names in the specified directory
//********************************************************************************
JRI_PUBLIC_API(jref)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetFolderContents(JRIEnv* env,
												 struct netscape_npasw_SetupPlugin* ThisPlugin,
												 struct java_lang_String *JSpath,
												 struct java_lang_String *JSsuffix)
{
	char szfilepath[_MAX_PATH];
	char **ppFileArray = NULL;
	short iFileCount = 0;
	struct java_lang_String *value;

	const char *path=NULL, *suffix=NULL;

	// gets the file directory
	if (JSpath != NULL)
		path = GetStringPlatformChars(env, JSpath);
	else
		return NULL;

	// gets the file extention
	if(JSsuffix != NULL)
		suffix = GetStringPlatformChars(env, JSsuffix);
	else
		return NULL;


	// constructs the file path we're looking for
	strcpy(szfilepath, path);
	strcat(szfilepath, "*");
	strcat(szfilepath, suffix);

	// finds firt file in the directory

#ifdef WIN32 // ********************* Win32 **************************
	
	WIN32_FIND_DATA fileInfo;
	HANDLE hFindFile = FindFirstFile(szfilepath, &fileInfo);

	if (hFindFile != INVALID_HANDLE_VALUE) {

		ppFileArray = (char **)malloc(sizeof(char *) * (iFileCount + 1));
		ppFileArray[iFileCount] = strdup(fileInfo.cFileName);
		iFileCount++;

	    // finds the next file
		while (FindNextFile(hFindFile, &fileInfo)) {

			if (strcmp(fileInfo.cFileName, REGSERV_SR) != 0)  {

				ppFileArray = (char **)realloc(ppFileArray, sizeof(char *) * (iFileCount + 1));
				ppFileArray[iFileCount] = strdup(fileInfo.cFileName);

#else       // ********************* Win16 **************************

	struct _find_t  c_file;
	unsigned uResult = _dos_findfirst(szfilepath, _A_NORMAL, &c_file); 
	if (0 == uResult)
	{
		ppFileArray = (char **)malloc(sizeof(char *) * (iFileCount + 1));
		ppFileArray[iFileCount] = strdup(c_file.name);
		iFileCount++;

		// finds the next file
		while(0 == _dos_findnext(&c_file))
		{
			// don't add "RegServ.SR" file
			if (0 != strcmp(c_file.name, REGSERV_SR)) 
			{
				ppFileArray = (char **)realloc(ppFileArray, sizeof(char *) * (iFileCount + 1));
				ppFileArray[iFileCount] = strdup(c_file.name);
#endif

				iFileCount++;
			}
		}

		// dumps everything in the JRI array
		void *resultFileArray = JRI_NewObjectArray(env, iFileCount, class_java_lang_String(env), NULL);
		if (resultFileArray == NULL)
			return NULL;

		// lock the JRI array reference, dispose old reference if necessary
		if (g_globalRefFileList)
			JRI_DisposeGlobalRef(env, g_globalRefFileList); 
		g_globalRefFileList = JRI_NewGlobalRef(env, resultFileArray); 

		if (resultFileArray) {

			for (short i=0; i<iFileCount; i++) {

				value = JRI_NewStringPlatform(env, ppFileArray[i], strlen(ppFileArray[i]), NULL, 0);
				JRI_SetObjectArrayElement(env, resultFileArray, i, value);

				free(ppFileArray[i]);
			}
		}

		free(ppFileArray);
		return (jref)resultFileArray;

	} else {

		return NULL;
	}
}



//********************************************************************************
// native method:
//
// GetExternalEditor
//
// allow Account Setup to pick an external editor to use
//********************************************************************************
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetExternalEditor(JRIEnv* env,
												 struct netscape_npasw_SetupPlugin* ThisPlugin)
{
	struct java_lang_String *editor;
	char *fileName = NULL;
	char *fileFilter = NULL;
	OPENFILENAME OpenFileName;
	TCHAR szFile[_MAX_PATH]  = "\0";

	// construct file filter for *.exe
	fileFilter = (char *)malloc(sizeof(char) * MAX_PATH);
	int iCounter = 0;
	char *pCat = "Executable Files (*.exe)";
	memcpy((void *)(fileFilter + iCounter), (void *)pCat, strlen(pCat));
	iCounter += strlen(pCat);

	pCat = "";
	memcpy((void *)(fileFilter + iCounter), (void *)pCat, 1);

	iCounter += 1;
	pCat = "*.exe";
	memcpy((void *)(fileFilter + iCounter), (void *)pCat, strlen(pCat));
	iCounter += strlen(pCat);

	pCat = "\0";
	memcpy((void *)(fileFilter + iCounter), (void *)pCat, 2);
	iCounter += 2;


	strcpy(szFile, "");

	// Fill in the OPENFILENAME structure for FileOpen dialog
	OpenFileName.lStructSize       = sizeof(OPENFILENAME);
	OpenFileName.hwndOwner         = GetActiveWindow(); 
	OpenFileName.hInstance         = (HINSTANCE) GetModuleHandle("netscape.exe");

#ifdef WIN32 // ********************* Win32 **************************
    OpenFileName.lpstrFilter       = (LPCTSTR) fileFilter; // "Executible Files(*.exe)";
#else        // ********************* Win16 **************************
    OpenFileName.lpstrFilter       = (LPCSTR) fileFilter; // "Executible Files(*.exe)";
#endif       
    OpenFileName.lpstrCustomFilter = NULL;
    OpenFileName.nMaxCustFilter    = 0;
    OpenFileName.nFilterIndex      = 0;
    OpenFileName.lpstrFile         = szFile;
    OpenFileName.nMaxFile          = sizeof(szFile);
    OpenFileName.lpstrFileTitle    = NULL;
    OpenFileName.nMaxFileTitle     = 0;
    OpenFileName.lpstrInitialDir   = NULL;
    OpenFileName.lpstrTitle        = "Choose Editor";
    OpenFileName.nFileOffset       = 0;
    OpenFileName.nFileExtension    = 0;
    OpenFileName.lpstrDefExt       = NULL;;
    OpenFileName.lCustData         = NULL;
    OpenFileName.lpfnHook         = NULL;
    OpenFileName.lpTemplateName    = NULL;
    OpenFileName.Flags             = OFN_FILEMUSTEXIST | OFN_EXTENSIONDIFFERENT | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN;
   
	BOOL bResult = GetOpenFileName(&OpenFileName);
	if (bResult) {
		fileName = (char *)OpenFileName.lpstrFile;
		short len = strlen(fileName);
		editor = JRI_NewStringPlatform(env, fileName, len, NULL, 0);
	}

	free(fileFilter);
	
	return bResult ? (java_lang_String *) editor : NULL;
}





//********************************************************************************
// native method:
//
// OpenFileWithEditor
//
// Opens an html file with an editor other than Navigator Editor
//********************************************************************************
JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fOpenFileWithEditor(JRIEnv* env,
												  struct netscape_npasw_SetupPlugin* self,
												  struct java_lang_String *JSapp,
												  struct java_lang_String *JSfile)
{
	const char *app=NULL, *file=NULL;

	if (JSapp != NULL)
		app = GetStringPlatformChars(env, JSapp);

	if (JSfile != NULL)
		file = GetStringPlatformChars(env, JSfile);

	//2, 1 for null char at the end, 1 for space char btw app & file
	int iCmdLength = 2;

	if (app)
		iCmdLength += strlen(app);

	if (file)
		iCmdLength += strlen(file);

	char *appCmd = (char *)malloc(sizeof(char) * iCmdLength);

	if (appCmd) {
		strcpy(appCmd, app);
		appCmd[strlen(appCmd)+1] = '\0';
		appCmd[strlen(appCmd)] = (char)32;
		//strcat(appCmd, " ");
		strcat(appCmd, file);
		appCmd[iCmdLength-1] = '\0';

		// starts up the external editor

#ifdef WIN32
		PROCESS_INFORMATION pi;
		BOOL                fRet;
		STARTUPINFO         sti;
		UINT                err = ERROR_SUCCESS;

		memset(&sti,0,sizeof(sti));
		sti.cb = sizeof(STARTUPINFO);

		fRet = CreateProcess(NULL, appCmd, NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi);

		if (!fRet) {
			DWORD ERR=GetLastError();
			char str[255];
			char szBuf[255];
			if (getMsgString(str, IDS_ERR_EDITOR)) {
				wsprintf(szBuf, str, app, file);
				DisplayErrMsg(szBuf, MB_OK);
			}

		}


#if 0
		char str[255];
		char szBuf[255];
		DWORD ERR;
		if (!fRet) {
			ERR=GetLastError();
		} else {
			ERR=0;
		}

		if (getMsgString(str, IDS_ERR_EDITOR2)) {
			wsprintf(szBuf, str, appCmd, ERR, iCmdLength, strlen(app), (int)app[strlen(app)-1], strlen(file), (int)file[0]);
			DisplayErrMsg(szBuf, MB_OK);
		}

#endif

#else
		int ret;

		ret = WinExec(appCmd, SW_SHOWNORMAL);
		
		// err handling if WinExec fails (returns greater than 31 if succeeds)
		if (ret < 31) {
			char buf[255];
			switch (ret) {
				case 0:           
						if (getMsgString(buf, IDS_OUT_OF_MEMORY))
							DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);
						break;
				case ERROR_BAD_FORMAT:
						if (getMsgString(buf, IDS_INVALID_EXE))
							DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);
						break;
				case ERROR_FILE_NOT_FOUND:
						if (getMsgString(buf, IDS_INVALID_FILE))
							DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);
						break;
				case ERROR_PATH_NOT_FOUND:
						if (getMsgString(buf, IDS_INVALID_PATH))
							DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);
						break;
				default:
						if (getMsgString(buf, IDS_NO_EDITOR))
							DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);
						break;
			}
		}
#endif

		// free memory
		free(appCmd);

	}
}



//********************************************************************************
// native method:
//
// SaveTextToFile
//
// Saves the info from Regi to a text file
//********************************************************************************
JRI_PUBLIC_API(jbool)
native_netscape_npasw_SetupPlugin_SECURE_0005fSaveTextToFile(JRIEnv* env,
											  struct netscape_npasw_SetupPlugin* self,
											  struct java_lang_String *JSsuggestFilename,
											  struct java_lang_String *JSdata,
											  jbool JSprompt)

{
	char *fileFilter = NULL;
	OPENFILENAME OpenFileName;
	jbool isSaved = FALSE;
	TCHAR szFile[_MAX_PATH]  = "\0";

	const char *fileName=NULL, *DataBuf=NULL;

	if (JSsuggestFilename != NULL) 
		fileName = GetStringPlatformChars(env, JSsuggestFilename);
	if (JSdata != NULL)
		DataBuf = GetStringPlatformChars(env, JSdata);


#ifdef WIN32 // ********************* Win32 **************************
	HANDLE hFile;
#else        // ********************* Win16 **************************
	HFILE hFile;
#endif

	// check for NULL buffer, string ops on null ptrs can be problematic
	assert(NULL != DataBuf);

	if (!JSprompt) {
		
		// save(create/overwrite) the file without display SAVE dialog
#ifdef WIN32 // ********************* Win32 **************************
		hFile = CreateFile (fileName, GENERIC_READ|GENERIC_WRITE, 
							FILE_SHARE_READ|FILE_SHARE_WRITE, FALSE,
							CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile != INVALID_HANDLE_VALUE) {
			DWORD NumberOfBytesWritten;
			if (WriteFile(hFile, DataBuf, strlen(DataBuf), &NumberOfBytesWritten, NULL) == TRUE) {
				isSaved = TRUE;
			}

			CloseHandle(hFile);
		}
#else        // ********************* Win16 **************************
		hFile = _lcreat(fileName, 0);

		if (HFILE_ERROR != hFile)
		{
			UINT uResult = _lwrite(hFile, DataBuf, strlen(DataBuf));
			HFILE hResult = _lclose(hFile);
		}
#endif // WIN32

	} else { // display SAVE dialog to user...

		// construct file filter for *.txt
		fileFilter = (char *)malloc(sizeof(char) * MAX_PATH);
		int iCounter = 0;
		char *pCat = "Text Documents (*.txt)";
		memcpy((void *)(fileFilter + iCounter), (void *)pCat, strlen(pCat));
		iCounter += strlen(pCat);
		pCat = "";
		memcpy((void *)(fileFilter + iCounter), (void *)pCat, 1);
		iCounter += 1;
		pCat = "*.txt";
		memcpy((void *)(fileFilter + iCounter), (void *)pCat, strlen(pCat));
		iCounter += strlen(pCat);
		pCat = "\0";
		memcpy((void *)(fileFilter + iCounter), (void *)pCat, 2);
		iCounter += 2;

		strcpy(szFile, fileName);
#ifndef WIN32 // ********************* Win16 *************************
		// construct valid win16 filename 8.3 format
		strcat(szFile, ".txt");
		ParseWin16BadChar(szFile);
#endif
		// Fill in the OPENFILENAME structure for FileOpen dialog
		memset(&OpenFileName, 0, sizeof(OPENFILENAME));
		OpenFileName.lStructSize       = sizeof(OPENFILENAME);
		OpenFileName.hwndOwner         = GetActiveWindow(); 
		OpenFileName.hInstance         = (HINSTANCE) GetModuleHandle("netscape.exe");
		OpenFileName.lpstrFilter       = (LPCTSTR) fileFilter; // "Executible Files(*.exe)";
		OpenFileName.lpstrCustomFilter = NULL;
		OpenFileName.nMaxCustFilter    = 0;
		OpenFileName.nFilterIndex      = 0;
		OpenFileName.lpstrFile         = szFile;
		OpenFileName.nMaxFile          = sizeof(szFile);
		OpenFileName.lpstrFileTitle    = NULL;
		OpenFileName.nMaxFileTitle     = 0;
		OpenFileName.lpstrInitialDir   = NULL;
		OpenFileName.lpstrTitle        = "Save New Account Information";
		OpenFileName.nFileOffset       = 0;
		OpenFileName.nFileExtension    = 0;
		OpenFileName.lpstrDefExt       = "txt";
		OpenFileName.lCustData         = NULL;
		OpenFileName.lpfnHook          = NULL;
		OpenFileName.lpTemplateName    = NULL;
		OpenFileName.Flags             = OFN_CREATEPROMPT | OFN_EXTENSIONDIFFERENT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;


		isSaved = FALSE;
		BOOL bGetSaveFileName = GetSaveFileName(&OpenFileName);
		
#ifndef WIN32	// ********************* Win16 **************************
		// Save with default filename if filename constructed is invalid
		if (!bGetSaveFileName && FNERR_INVALIDFILENAME == CommDlgExtendedError())
		{
			strcpy(szFile, DEF_ACCINFOFILENAME);
			bGetSaveFileName = GetSaveFileName(&OpenFileName);
		}
#endif
		
		if (bGetSaveFileName) {
			char fileName[_MAX_PATH];
			strcpy(fileName, OpenFileName.lpstrFile);

			// saves the file here

#ifdef WIN32 // ********************* Win32 **************************
			hFile = CreateFile(fileName, GENERIC_READ|GENERIC_WRITE, 
								FILE_SHARE_READ|FILE_SHARE_WRITE, FALSE,
								CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			
			int ret = GetLastError();
			if (hFile != INVALID_HANDLE_VALUE) {
				DWORD NumberOfBytesWritten;
				if (WriteFile(hFile, DataBuf, strlen(DataBuf), &NumberOfBytesWritten, NULL) == TRUE) {
					isSaved = TRUE;
				}
				CloseHandle(hFile);
			}
		} 
#else       // ********************* Win16 **************************
			hFile = _lcreat(fileName, 0);

			if (HFILE_ERROR != hFile)
			{
				UINT uResult = _lwrite(hFile, DataBuf, strlen(DataBuf));
				_lclose(hFile);
			}
		}
#endif // WIN32

	   free(fileFilter);

   }

	return (jbool) isSaved;
}



//********************************************************************************
// native method:
//
// ReadFile
//
// Reads Binary File
//********************************************************************************
extern JRI_PUBLIC_API(struct java_lang_Object *)
native_netscape_npasw_SetupPlugin_SECURE_0005fReadFile(JRIEnv* env,
										struct netscape_npasw_SetupPlugin* self,
										struct java_lang_String *JSfile)
{
	java_lang_Object *retVal=NULL;
	const char *theFile = NULL;

	if (JSfile != NULL) 
		theFile = GetStringPlatformChars(env, JSfile);

	if (!theFile)
		return NULL;

	// gets a handle to open the file for reading...
#ifdef WIN32
	HANDLE hFile = CreateFile (theFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	// if we failed to open file, return
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;
#else
	HFILE hFile = _lopen(theFile, OF_READ);

	// if we failed to open file, return
	if (hFile == HFILE_ERROR)
		return NULL;
#endif

	DWORD FileSize;
	void *buffer;
#ifdef WIN32
	// finds the file size
	DWORD FileSizeHigh;
	DWORD dwError;
	FileSize = GetFileSize(hFile, &FileSizeHigh);

	// if we failed to get the size, return
	if (FileSize == 0xFFFFFFFF && (dwError = GetLastError()) != NO_ERROR) {
		CloseHandle(hFile);
		return NULL;
	}

	// read the file data
	buffer = (void *)malloc(sizeof(char) * FileSize);
#endif

	if (!buffer) {
#ifdef WIN32
		CloseHandle(hFile);
#else
		_lclose(hFile);
#endif
		return NULL;
	}

	DWORD BytesToRead = FileSize;
	DWORD BytesRead;
	
#ifdef WIN32
	BOOL rtnRead = ReadFile(hFile, buffer, BytesToRead, &BytesRead, NULL);
	int err = GetLastError();
	// make sure there is no err in reading
	if ((rtnRead != TRUE) || (BytesRead == 0)) {
		CloseHandle(hFile);
		return NULL;
	}
#else
//   int nBytesRead = _lread(hFile, buffer, BytesToRead);
#endif






/*
char *fileName="c:\\tmp\\stuff.ico";
HANDLE hFile2;
hFile2 = CreateFile(fileName, GENERIC_READ|GENERIC_WRITE, 
					FILE_SHARE_READ|FILE_SHARE_WRITE, FALSE,
					CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

if (hFile2 != INVALID_HANDLE_VALUE) {
	DWORD NumberOfBytesWritten;
	int y = strlen((char *)buffer);
	if (WriteFile(hFile2, buffer, BytesRead, &NumberOfBytesWritten, NULL) == TRUE) {
		int x = 1;
	}
	CloseHandle(hFile2);
}
*/


#ifdef WIN32
	CloseHandle(hFile);
#else
	_lclose(hFile);
#endif

	// now construct java byte object
	retVal = (java_lang_Object *)JRI_NewByteArray(env, BytesRead, buffer);



	return (java_lang_Object *)retVal;

}



//********************************************************************************
// native method:
//
// WriteFile
//
// Writes Binary File
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fWriteFile(JRIEnv* env,
										 struct netscape_npasw_SetupPlugin* self,
										 struct java_lang_String *JSfile,
										 struct java_lang_Object *JSdata)
{
	const char *fileName=NULL, *fileData=NULL;
	long dataLen=0;

	if ((JSfile == NULL) || (JSdata == NULL))
		return;

	fileName = GetStringPlatformChars(env, JSfile);
	dataLen = JRI_GetByteArrayLength(env, JSdata);

	if (!dataLen)
		return;

	// gets the javaObject data
	fileData = JRI_GetByteArrayElements(env, JSdata);
	if (!fileData)
		return;

#ifdef WIN32
	// now we have the filename, data & data length, we can write/create the file
	HANDLE hFile;
	hFile = CreateFile(fileName, GENERIC_READ|GENERIC_WRITE, 
						FILE_SHARE_READ|FILE_SHARE_WRITE, FALSE,
						CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {
		DWORD NumberOfBytesWritten;
		WriteFile(hFile, fileData, dataLen, &NumberOfBytesWritten, NULL);
		CloseHandle(hFile);
	}
#else
	int hFile = _open(fileName, _O_RDWR, _S_IWRITE);
	if (-1 != hFile)  
	{
		_write(hFile, (void *) fileData, (unsigned int) dataLen);
		_close(hFile);
	}
#endif

	return;
}

