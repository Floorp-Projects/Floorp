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

#include "LTSMSupport.h"
static Boolean 				mHasTextServices = false;
static Boolean				mHasTSMTE = false;
static ScriptCode			mSysScript = smRoman;
static Boolean				mTSMTEisVersion1 = false;
static TSMTEPreUpdateUPP 	mTSMTEPreUpdateUPP = NULL;
static TSMTEPostUpdateUPP 	mTSMTEPostUpdateUPP = NULL;
		

static Boolean TrapAvailable(short theTrap);
static pascal void DefaultTSMTEPreUpdateProc(TEHandle textH, long refCon);

// ---------------------------------------------------------------------------
//	Copy from InlineInputSample.c
// this TSMTEPreUpdateProc only works around a bug in TSMTE 1.0, which has
// been fixed in 1.1. For other possible uses, see technote TE 27.
// ---------------------------------------------------------------------------

static pascal void DefaultTSMTEPreUpdateProc(TEHandle textH, long refCon)
{
	#pragma unused(refCon)

	
	if (mTSMTEisVersion1)		// Modified here for performance 
	{
		ScriptCode keyboardScript;
		short mode;
		TextStyle theStyle;
		
		keyboardScript = ::GetScriptManagerVariable(smKeyScript);
		mode = doFont;
		if (!(::TEContinuousStyle(&mode, &theStyle, textH) &&
				::FontToScript(theStyle.tsFont) == keyboardScript))
		{
			theStyle.tsFont = ::GetScriptVariable(keyboardScript, smScriptAppFond);
			::TESetStyle(doFont, &theStyle, false, textH);
		};
	};
}

// ---------------------------------------------------------------------------
//		¥ Initialization
// ---------------------------------------------------------------------------
//	Default constructor
void LTSMSupport::Initialize()
{
	mSysScript = ::GetScriptManagerVariable(smSysScript); 
	CheckForTextServices();
	BeginTextServices();
	TSMTENewUPP();
}
// ---------------------------------------------------------------------------
//		¥ CheckForTextServices
//		Call by constructor
//		From TE27 Page 4/14
// ---------------------------------------------------------------------------
void LTSMSupport::CheckForTextServices(void)
{
        long gestaltResponse;
        
        if (::TrapAvailable(_Gestalt))
        {
                if ((::Gestalt(gestaltTSMgrVersion, &gestaltResponse) == noErr) &&
                         (gestaltResponse >= 1))
                {
                		mTSMTEisVersion1 = (gestaltResponse == gestaltTSMTE1);
                        mHasTextServices = true;
                        if (::Gestalt(gestaltTSMTEAttr, &gestaltResponse) == noErr)
                                mHasTSMTE = ((gestaltResponse >> gestaltTSMTEPresent) & 1);
                };
        };
}

// ---------------------------------------------------------------------------
//		¥ TSMTENewUPP
//		Modified from InlineInputSample.c
// ---------------------------------------------------------------------------
void LTSMSupport::TSMTENewUPP()
{
	if(mHasTSMTE) {
		if(mTSMTEPreUpdateUPP == NULL )
			mTSMTEPreUpdateUPP = NewTSMTEPreUpdateProc(DefaultTSMTEPreUpdateProc);
	}
}
// ---------------------------------------------------------------------------
//		¥ BeginTextServices
//		Call by constructor
//		From TE27 Page 4/14
// ---------------------------------------------------------------------------
void LTSMSupport::BeginTextServices()
{
	if (!(mHasTSMTE && ::InitTSMAwareApplication() == noErr))
	{
	        // if this happens, just move on without text services
	        mHasTextServices = false;
	        mHasTSMTE = false;
	};
}
// ---------------------------------------------------------------------------
//		¥ DoQuit
//		Called by DoQuit()
//		From TE27 Page 4/14
// ---------------------------------------------------------------------------
void LTSMSupport::DoQuit(Int32	/* inSaveOption */)
{
	if (mHasTextServices)
        (void) ::CloseTSMAwareApplication();
}
// ---------------------------------------------------------------------------
//		¥ IntlTSMEvent
//		From TE27 Page 11/14
// ---------------------------------------------------------------------------
Boolean
LTSMSupport::TSMEvent(const EventRecord &inMacEvent)
{
        if(mHasTextServices)
        {
	        // make sure we have a port and it's not the Window Manager port
//	        if (qd.thePort != nil && ::FrontWindow() != nil)
//	        {
//	                oldFont = qd.thePort->txFont;
//	                keyboardScript = ::GetScriptManagerVariable(smKeyScript);
//	                if (::FontToScript(oldFont) != keyboardScript)
//	                        ::TextFont(GetScriptVariable(keyboardScript, smScriptAppFond));
//	        };
	        return ::TSMEvent((EventRecord *)&inMacEvent);
	    }
	    else
	    {
	    	return false;
	    }
}
// ---------------------------------------------------------------------------
//		¥ IntlTSMEvent
//		From TE27 Page 11/14
// ---------------------------------------------------------------------------
Boolean	LTSMSupport::SetTSMCursor(Point mousePos)
{
	if(mHasTextServices)
		return ::SetTSMCursor(mousePos);
	else
		return false;
}
// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
Boolean	LTSMSupport::HasTextServices()
{
	return mHasTextServices;
}
// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
Boolean	LTSMSupport::HasTSMTE()
{
	return mHasTSMTE;
}
// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------

TSMTEPreUpdateUPP	LTSMSupport::GetDefaultTSMTEPreUpdateUPP()
{
	return mTSMTEPreUpdateUPP;
}
// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
TSMTEPostUpdateUPP	LTSMSupport::GetDefaultTSMTEPostUpdateUPP()
{
	return mTSMTEPostUpdateUPP;
}
// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
void LTSMSupport::StartFontScriptLimit()
{
	//short theFontScript = ::FontScript();
	//StartFontScriptLimit(theFontScript);
}

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
void LTSMSupport::StartFontScriptLimit(  ScriptCode /* scriptcode */)
{
	//	We want to disable all the script except: Roman and the Font script
	//  1. we have set the system script to the font script
	//	2. disable all the script except Roman and System script by calling 
	//			KeyScript(smDisablKybds);
	//  3. Should we also switch input method to the font script ?
	//	4. restore the system script.
	//if(mSysScript != scriptcode)
	//{
	//	::SetScriptManagerVariable(smSysScript, scriptcode);		
	//}
	//if(scriptcode != ::GetScriptManagerVariable(smKeyScript))
	//{
	//	::KeyScript(scriptcode);
	//}
//	::KeyScript(smKeyDisableKybds);
//	::SetScriptManagerVariable(smSysScript, mSysScript);		
}
// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------
void LTSMSupport::EndFontScriptLimit()
{
	// Re-enable all the script
	//::KeyScript(smKeyEnableKybds);
	//::SetScriptManagerVariable(smSysScript, mSysScript);		
}
// ---------------------------------------------------------------------------
// check to see if a given trap is implemented. We follow IM VI-3-8.
// ---------------------------------------------------------------------------
static 
Boolean TrapAvailable(short theTrap)
{
	TrapType theTrapType;
	short numToolboxTraps;
	
	if ((theTrap & 0x0800) > 0)
		theTrapType = ToolTrap;
	else
		theTrapType = OSTrap;

	if (theTrapType == ToolTrap)
	{
		theTrap = theTrap & 0x07ff;
		if (NGetTrapAddress(_InitGraf, ToolTrap) == NGetTrapAddress(0xaa6e, ToolTrap))
			numToolboxTraps = 0x0200;
		else
			numToolboxTraps = 0x0400;
		if (theTrap >= numToolboxTraps)
			theTrap = _Unimplemented;
	};

	return (NGetTrapAddress(theTrap, theTrapType) != NGetTrapAddress(_Unimplemented, ToolTrap));
}
