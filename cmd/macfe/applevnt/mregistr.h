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

// mregistr.h
// Registry for AppleEvent notifiers
// Pretty clumsy right now, but separating this functionality out of uapp seems
// to be the right thing.
// It is just a collection of routines

#pragma once

#include <LArray.h>

class CNotifierRegistry	{
public:
// ¥¥ÊAppleEvent handling
	static void			HandleAppleEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	static void			ReadProtocolHandlers();	// Saving to prefs interface
	static void			WriteProtocolHandlers();

private:
	static void 		HandleRegisterProtocol(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	static void 		HandleUnregisterProtocol(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
// ¥¥ url echo
	static void 		HandleRegisterURLEcho(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	static void 		HandleUnregisterURLEcho(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
};