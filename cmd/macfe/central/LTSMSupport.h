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

#include <TextServices.h>
#ifndef __LTSMSUPPORT__
#define __LTSMSUPPORT__

#include <TSMTE.h>

//	ftang:
//	LTSMSupport handle the following TSM stuff:
//		InitTSMAwareApplication()	- in Initalize() => BeginTextServices()
//		CloseTSMAwareApplication()  - in DoQuit() => EndTextServices()
//		TSMEvent() 					- in TSMEvent()
//		SetTSMCursor() 				- in SetTSMCursor()
//
//	LTSMSupport handle the following TSM stuff:
//		NewTSMDocument()
//		DeleteTSMDocument()
//		ActiveTSMDocument()
//		DeactivateTSMDocument()
//		FixTSMDocument()
//
//	The following TSM stuff will not be handle:
//		TSMMenuSelect() : We only deal with input method. Therefore, ignore it
//							See IM-Text 7-22

class	LTSMSupport {
public:
	static void				Initialize();
	static void				DoQuit(Int32	inSaveOption);
	static Boolean			TSMEvent(const EventRecord &inMacEvent);		
	static Boolean			SetTSMCursor(Point mousePos);
	
	static Boolean			HasTextServices();
	static Boolean			HasTSMTE();
	
	static TSMTEPreUpdateUPP	GetDefaultTSMTEPreUpdateUPP();
	static TSMTEPostUpdateUPP	GetDefaultTSMTEPostUpdateUPP();
	
	static void 			StartFontScriptLimit();
 	static void 			StartFontScriptLimit( ScriptCode scriptcode );
	
	static void				EndFontScriptLimit();
	
protected:
	static void				CheckForTextServices(void);	
	static void				BeginTextServices(void);
	static void 			TSMTENewUPP();

};

#endif
