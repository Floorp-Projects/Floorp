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

// earlmgr.h, Mac front end

#ifndef _EarlMgr_
#define _EarlMgr_
#pragma once

	// client
class CMimeMapper;
	// project
#include "xp_core.h"
#include "ntypes.h"
#include "client.h"
	// utilities
struct CStr255;
	// libraries
class LFileStream;
	// system

/*****************************************************************************
 *	EarlManager
 *	Dispatcher of the load commands	
 ******************************************************************************/

class EarlManager: public LPeriodical {
public:
					EarlManager ();
	
	// ее URL loading interface
	
	// StartLoadURL tells network library to start loading the URL
	static int		StartLoadURL (URL_Struct * request, MWContext *context,
						FO_Present_Types output_format);

	// Cancels all loads for a given context
	static void 	DoCancelLoad (MWContext *context);

	// Called by netlib when URL loading is done. This routine is passed to NET_LoadURL
	static void 	DispatchFinishLoadURL (URL_Struct *url, int status, MWContext *context);

	// ее Event handling
	// Instead of calling NET_InterruptWindow at unsafe time (possible reentrancy)
	// call this function. It will interrupt the window after netlib returns
	void			InterruptThis(MWContext * context);
	void			SpendTime (const EventRecord &inMacEvent);
	void			SpendYieldedTime();
private:
	void 			FinishLoadURL (URL_Struct *url, int status, MWContext *context);
	MWContext * 	fInterruptContext;		// Just used by InterruptThis
};

extern EarlManager TheEarlManager;


#endif // _EarlMgr_


