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

#ifndef _HELPER_APP_
#define _HELPER_APP_

class CHelperApp : public CObject
{
private:
    friend class CSpawnList;
	friend class COwnedAndLostList;
	friend class COwnedLostItem;

    POSITION m_rIndex;
public:
    CHelperApp()    {
        m_rIndex = m_cplHelpers.AddTail((void *)this);
		how_handle = HANDLE_UNKNOWN;
    }
    ~CHelperApp()   {
        m_cplHelpers.RemoveAt(m_rIndex);
    }

	int 	iPos;
	NET_cdataStruct *cd_item;
	int     how_handle;
	CString csCmd;
	BOOL	bChanged;
	BOOL	bNewType;
	BOOL    bChangedExts; 
	CString	strFileClass;  // Windows registry file type class

	CString csMimePrefPrefix; //Prefix of the mime type - if this helper is associated with a MIME type specd. thru' prefs...CRN_MIME

	static CPtrList m_cplHelpers;
};


//
// INI file strings to tell how to handle mime types
//
#define MIME_INTERNALLY "browser-handle-internal"
#define MIME_PROMPT     "browser-handle-promptuser"
#define MIME_SAVE       "browser-handle-internal-savetodisk"
#define MIME_SHELLEXECUTE      "browser-handle-shellexecute"
#define MIME_OLE      "browser-handle-oleserver"

// Helper functions
extern NET_cdataStruct *
fe_NewFileType(LPCSTR lpszDescription, LPCSTR lpszExtension, LPCSTR lpszMimeType, LPCSTR lpszOpenCmd);

extern BOOL fe_RemoveFileType(NET_cdataStruct *);

extern BOOL fe_ChangeFileType(NET_cdataStruct *, LPCSTR lpszMimeType, int nHowToHandle, LPCSTR lpszOpenCmd);

extern BOOL fe_CanHandleByOLE(char** exts, short numOfExt);
extern BOOL fe_SetHandleByOLE(char* mimeType, CHelperApp* app,BOOL handleByOLE);
extern BOOL fe_IsHandleByOLE(char* mimeType);
extern BOOL  CopyRegKeys(HKEY  hKeyOldName, HKEY  hKeyNewName, DWORD subkeys, DWORD maxSubKeyLen, DWORD maxClassLen,				   DWORD values,
				   DWORD maxValueNameLen,DWORD maxValueLen,const char *OldPath,const char *NewPath);
extern void SetShellOpenCommand(LPCSTR lpszFileClass, LPCSTR lpszCmdString);
#endif /* _HELPER_APP_ */
