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

// errmgr.cp, Mac front end ErrorManager

// Editorial comments:
//
//		Viewing some of this code may make you scream in panic
//		Don't try this at home
//		Use only under medical supervision

#include "uerrmgr.h"

#include <UDesktop.h>
#include "uapp.h"
#include "PascalString.h"
#include "xpassert.h"
#include "macutil.h"
#include "resgui.h"
#include "xp_error.h"
#include "UTextBox.h"
#include "resdef.h"
#include "macfeprefs.h"
// Apple
#include <Notification.h>
#include <Resources.h>
#include <AppleEvents.h>
#include <Dialogs.h>

#include <Icons.h>

#include "libi18n.h"

static void InitializeStrings();	// Need proto to use before declaration

OSType ErrorManager::sAlertApp = emSignature;

// Converts an OSErr to appropriate string.
// Just prints the number for now, might do something more
// useful in the future
const CStr255 ErrorManager::OSNumToStr(OSErr err)
{
	Str255 str;
	NumToString((long)err, str);
	CStr255 retStr(str);
	return retStr;
}

// PrepareToInteract makes sure that this is the frontmost application.
// If Netscape is not in the foreground, it blinks the small icon in
// the menu bar until the user brings us to front
void ErrorManager::PrepareToInteract()
{
	TryToInteract(10000000);
}

// TryToInteract tries to bring Netscape to foreground (like PrepareToInteract)
// If user does not bring Netscape to foreground in 900 seconds, 
// it returns FALSE;
Boolean ErrorManager::TryToInteract(long wait)
{
	ProcessSerialNumber netscapeProcess, frontProcess;
	OSErr err = ::GetCurrentProcess(&netscapeProcess);
	Boolean inFront = IsFrontApplication();
	if (!inFront)
	{
		unsigned long start, currentTime;
		::GetDateTime(&start);
		currentTime = start;
		NMRec nmRec;
		Handle iconSuite;
		err = ::GetIconSuite (&iconSuite, 128,svAllSmallData);
		nmRec.qLink = 0;
		nmRec.qType = nmType;
		nmRec.nmMark = true;
		nmRec.nmIcon = iconSuite;
		nmRec.nmStr = NULL;
		nmRec.nmSound = (Handle)-1;
		nmRec.nmResp = NULL;
		err = ::NMInstall(&nmRec);	// Remove the call automatically
		if (err == noErr)
		{
			while (!inFront && ((currentTime - start) < wait))
			{
				EventRecord event;
				::WaitNextEvent(0, &event, 30, NULL);
				::GetFrontProcess(&frontProcess);
				::GetDateTime(&currentTime);
				::SameProcess(&frontProcess, &netscapeProcess, &inFront);
			}
			::NMRemove(&nmRec);
		}
#if defined(DEBUG) && defined(ALEKS)
		XP_Abort();
#endif
		UDesktop::Resume();	// Because the resume event gets eaten up by dialog box
// Now, process any update events, so that our windows get refreshed properly
		EventRecord event;
		while (::WaitNextEvent(updateEvt, &event, 1, NULL))
			(CFrontApp::GetApplication())->DispatchEvent(event);
	}
	return inFront;
}

// Class that resizes the alert dialog

// Resizes 
void ShrinkToFitAlertDialog(cstring & temp);
void ShrinkToFitAlertDialog(cstring & temp)
{
	Handle ditl = ::GetResource('DITL', ALRT_PlainAlert);
	StHandleLocker lock(ditl);
	Handle alrt = ::GetResource('ALRT', ALRT_PlainAlert);
	StHandleLocker lock2(alrt);
	
// еее WARNING еее <ftang> 
// 	You need to consider the PAD byte after each string
#define IncludePadByte(a)		(((a) & 1) ? ((a) +1) : (a))

	// This ugliness happens because my pointer arithmetic was messed up otherwise
	UInt32 tmp = (UInt32)*ditl;
	Rect * okRect = (Rect*)(tmp + 0x6);
	// Offset + length of the OK string
	Rect * editRect = (Rect*)(tmp + 0x14 + IncludePadByte(*(uint8*)(tmp + 0xF)));
	StTextState textState;
	TextFont(0);
	TextSize(12);
	short height = UTextBox::TextBoxDialogHeight((char*)temp, temp.length(), editRect,
					teForceLeft, 0);
	if (height < 50)	// Need to make place for the icon
		height = 50;
	editRect->bottom = editRect->top + height;
	okRect->top = editRect->bottom + 8;
	okRect->bottom = okRect->top + 20;
// Get the window resource
// Strange thing here: after ALRT is called for the first time,
// altrRect gets altered. Its top/left are strange (>30000)
// So I reset them, and everything seems to work
// I bet that the toolbox guys are storing alert state in these
	Rect * alrtRect = (Rect*)*alrt;
	alrtRect->top = 100;
	alrtRect->bottom = alrtRect->top+ okRect->bottom + 8;
	alrtRect->left = 185;
	alrtRect->right = 577;
}

void ShrinkToFitYorNDialog(cstring & temp);
void ShrinkToFitYorNDialog(cstring & temp)
{
	Handle ditl = ::GetResource('DITL', ALRT_YorNAlert);
	StHandleLocker lock(ditl);
	Handle alrt = ::GetResource('ALRT', ALRT_YorNAlert);
	StHandleLocker lock2(alrt);
	// This ugliness happens because my pointer arithmetic was messed up otherwise
	UInt32 base = (UInt32)*ditl;

// еее WARNING еее <ftang> 
// 	You need to consider the PAD byte after each string

	Rect * yesRect = (Rect*)(base + 0x6);
	UInt8 * yesLen = (UInt8 *)(base + 0xF);
	
	Rect * noRect = (Rect*)(base + 0x14 + IncludePadByte(*yesLen));
	UInt8 * noLen = (UInt8 *)(base + 0x1D + IncludePadByte(*yesLen));
	Rect * editRect = (Rect*)(base + 0x22 + IncludePadByte(*yesLen) + IncludePadByte(*noLen));
	StTextState textState;
	TextFont(0);
	TextSize(12);
	short height = UTextBox::TextBoxDialogHeight((char*)temp, temp.length(), editRect,
					teForceLeft, 0);
	if (height < 50)	// Need to make place for the icon
		height = 50;
	editRect->bottom = editRect->top + height;
	yesRect->top = editRect->bottom + 8;
	yesRect->bottom = yesRect->top + 20;
	noRect->top = editRect->bottom + 8;
	noRect->bottom = noRect->top + 20;
// Get the window resource
// Strange thing here: after ALRT is called for the first time,
// altrRect gets altered. Its top/left are strange (>30000)
// So I reset them, and everything seems to work
// I bet that the toolbox guys are storing alert state in these
	Rect * alrtRect = (Rect*)*alrt;
	alrtRect->top = 185;
	alrtRect->bottom = alrtRect->top+ yesRect->bottom + 8;
	alrtRect->left = 185;
	alrtRect->right = 590;
}

	// Displays a vanilla alert. All strings inside the alert are 
	// supplied by caller
void ErrorManager::PlainAlert(const CStr255& s1, const char * s2, const char * s3, const char * s4)
{
	if (sAlertApp != emSignature)
		return;
	if (!TryToInteract(900))	// If we time out, go away
		return;
	CStr255 S1(s1);
	CStr255 S2(s2);
	CStr255 S3(s3);
	CStr255 S4(s4);
	ConvertCRtoLF(S1);
	ConvertCRtoLF(S2);
	ConvertCRtoLF(S3);
	ConvertCRtoLF(S4);
	ParamText(S1, S2, S3, S4);
	cstring temp;
	temp += s1;
	temp += s2;
	temp += s3;
	temp += s4;
	ShrinkToFitAlertDialog(temp);
	UDesktop::Deactivate();
	::CautionAlert(ALRT_PlainAlert, NULL);
	UDesktop::Activate();
}

void ErrorManager::PlainAlert( short resID )
{
	InitializeStrings();
	
	if (sAlertApp != emSignature)
		return;

	CStr255			cstr[ 4 ];
	StringHandle	strings[ 4 ];
		
	if ( !TryToInteract(900))
		return;
	
	for ( short i = 0; i <= 3; i++ )
	{
		strings[ i ] = GetString( resID + i );
		if ( strings[ i ] )
			cstr[ i ] = **( (CStr255**)strings[ i ] );
	}

	ParamText( cstr[ 0 ], cstr[ 1 ], cstr[ 2 ], cstr[ 3 ] );
	cstring temp;
	temp += cstr[ 0 ];
	temp += cstr[ 1 ];
	temp += cstr[ 2 ];
	temp += cstr[ 3 ];
	ShrinkToFitAlertDialog(temp);	// Size the dalog
	UDesktop::Deactivate();
	::CautionAlert( ALRT_PlainAlert, NULL );
	UDesktop::Activate();
}

// Displays a yes or no alert. All strings inside the alert are 
// supplied by caller
Boolean ErrorManager::PlainConfirm(const char * s1, const char * s2, const char * s3, const char * s4)
{
	if (sAlertApp != emSignature)
		return FALSE;

	if (!TryToInteract(900))	// If we time out, go away
		return false;
	CStr255 S1(s1);
	CStr255 S2(s2);
	CStr255 S3(s3);
	CStr255 S4(s4);
	ConvertCRtoLF(S1);
	ConvertCRtoLF(S2);
	ConvertCRtoLF(S3);
	ConvertCRtoLF(S4);
	
	// pkc (6/6/96) Hack to stuff s1 into CStr255's if it's the
	// only string passed in.
	if( S1.Length() < strlen(s1) && s2 == NULL )
	{
		int storedLength = S1.Length();
		S2 += (s1 + storedLength);
		ConvertCRtoLF(S2);
		storedLength += S2.Length();
		if( storedLength < strlen(s1) )
		{
			S3 += (s1 + storedLength);
			ConvertCRtoLF(S3);
			storedLength += S3.Length();
			if( storedLength < strlen(s1) )
			{
				S4 += (s1 + storedLength);
				ConvertCRtoLF(S4);
			}
		}
	}
	
	cstring temp;
	temp += s1;
	temp += s2;
	temp += s3;
	temp += s4;
	ParamText(S1, S2, S3, S4);
	ShrinkToFitYorNDialog(temp);
	UDesktop::Deactivate();
	int confirm = ::CautionAlert(ALRT_YorNAlert, NULL);
	UDesktop::Activate();
	return (confirm == 1);
}

void ErrorManager::ErrorNotify(OSErr err, const CStr255& message)
{
        if (sAlertApp != emSignature)
                return;
        CStr255 errNum = OSNumToStr(err);
        PlainAlert(message, errNum);
}

char * XP_GetString( int resID )
{
	XP_ASSERT( ( SHRT_MIN < resID ) && ( resID < SHRT_MAX ) );
	return GetCString( resID + RES_OFFSET);
}

static void XPStringsNotFoundAlert()
{
	Str255 stringsFileName;
	
	GetIndString(stringsFileName, 14000, 2); // From Bootstrap.rsrc.
	ParamText(stringsFileName, NULL, NULL, NULL);
	Alert (14003, NULL);
	ExitToShell();
}

//-----------------------------------
static void InitializeStrings()
{
	static Boolean initialized = false;
	if (initialized)
		return;

	initialized = true;
	
	//	Locate and load all the XP string resources
	//	in the "Netscape Resources" file
	//	inside the "Essential Files" folder.
	
	FSSpec stringsFolderSpec;
	
	if (FindGutsFolder(&stringsFolderSpec) != noErr)
		XPStringsNotFoundAlert();
	
	CInfoPBRec pb;
	
	pb.dirInfo.ioNamePtr = stringsFolderSpec.name;
	pb.dirInfo.ioVRefNum = stringsFolderSpec.vRefNum;
	pb.dirInfo.ioFDirIndex = 0;
	pb.dirInfo.ioDrDirID = stringsFolderSpec.parID;
	
	if (PBGetCatInfoSync(&pb) != noErr)
		XPStringsNotFoundAlert();

	Str255 stringFileName;
	GetIndString(stringFileName, 14000, 2);
	
	FSSpec stringsFileSpec;
	if (FSMakeFSSpec(pb.dirInfo.ioVRefNum, pb.dirInfo.ioDrDirID,
		stringFileName, &stringsFileSpec) != noErr)
		XPStringsNotFoundAlert();
	
	short currentFileRefNum = CurResFile();
	short stringsFileRefNum = FSpOpenResFile(&stringsFileSpec, fsCurPerm);
	
	if (ResError() != noErr)
		XPStringsNotFoundAlert();
		
	MoveResourceMapBelowApp();

	UseResFile(currentFileRefNum);
} // InitializeStrings

void MoveResourceMapBelowApp()
{
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#else
#error "There'll probably be a bug here."
#endif
	
	//	Partial definition of "private" Resource Manager structure.
	struct ResourceMap
	{
		char header[16];
		ResourceMap **next;
		short fileRefNum;
	};
	
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif
	
	short applicationRefNum = LMGetCurApRefNum();
	ResourceMap **stringsFileMap = (ResourceMap **)LMGetTopMapHndl();
	
	//	Find the application's resource map.
	for (ResourceMap **map = stringsFileMap; map; map = (*map)->next)
	{
		if ((*map)->fileRefNum == applicationRefNum)
		{
			//	Move the strings file below the application
			//	in the current resource chain.
			//	(Kids, don't do this at home!)
			ResourceMap **top = (*stringsFileMap)->next;
			LMSetTopMapHndl((Handle)top);
			LMSetCurMap((*top)->fileRefNum);
			(*stringsFileMap)->next = (*map)->next;
			(*map)->next = stringsFileMap;
			break;
		}
	}
} 

//-----------------------------------
char* GetCString( short resID )
{
	CStr255 pstring = GetPString(resID);
	return (char*)pstring;
} // GetCString

//-----------------------------------
CStr255 GetPString( ResIDT resID )
{
	InitializeStrings();
	StringHandle sh = GetString( resID );
	if (sh && *sh)
	{
		StHandleLocker aLock((Handle)sh); // probably not necessary
		return **(CStr255**)sh; // copy to caller's string.
	}
	CStr255 result; // initialized to empty by constructor.
	return result; // This copies the string to the caller's string.	
} // GetPString

char * INTL_ResourceCharSet(void)
{
    char *cbuf;
    cbuf = (char *)GetCString(CHARSET_RESID);
    if (cbuf[0] == '\0')
    	return "x-mac-roman";
    return cbuf;  
}
