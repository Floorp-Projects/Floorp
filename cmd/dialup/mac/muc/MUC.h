/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#pragma once

enum MUCError
{
	errProfileNotFound = 1,
	errNeedToRunAccountSetup,
	errCannotSwitchDialSettings,
	errUserCancelledLaunch,
	errCantLoadNeededResources
};

enum MUCSelector
{
	kGetPluginVersion = 1,
	kSelectDialConfig,
	kAutoSelectDialConfig,
	kEditDialConfig,
	kGetDialConfig,
	kSetDialConfig,
	kNewProfileSelect,
	kClearProfileSelect,
	kInitListener,
	kCreateNewDialConfig,
	kModifyDialConfig,
	kDeleteDialConfig,
	kOpenConnection,
	kCloseConnection,
	kGetActivePPPConfig,
	kSetActivePPPConfig,
	kGetActiveOTConfig,
	kSetActiveOTConfig,
	kGetConnectionState,
	kGetAutoConnectState,
	kSetAutoConnectState,
	kGetModemsList,
	kGetActiveModemName,
	kGetReceivedIPPacketCount
 };

typedef struct
{
	Str255				mOTConfiguration;
	Str255				mOTPPPConfiguration;
	Str255				mOTModemConfiguration;
}
OTInfo, *OTInfoPtr;

typedef struct
{
	Str255				mProfileName;
	Str255				mAccountName;
	Str255				mModemName;
	Str255				mLocationName;
}
FreePPPInfo, *FreePPPInfoPtr;

typedef struct
{
	OTInfo				mOT;
	FreePPPInfo			mPPP;		
}
MUCInfo, *MUCInfoPtr;

typedef OSErr (*TraversePPPListFunc)( Str255** list );

#pragma export on
typedef long (*PE_PluginFuncType)(long selectorCode, void* pb, void* returnData );
#ifdef __cplusplus
extern "C" {
#endif
long PE_PluginFunc( long selectorCode, void* pb, void* returnData );
#ifdef __cplusplus
}
#endif
#pragma export off
