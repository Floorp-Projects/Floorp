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

//
// InternetConfig.h
//
// MacFE interface to its support for Apple Internet Config
//
// Created by Tim Craycroft, 2/11/96
//
//

#pragma once

#include "ICAPI.h"

// Always call.
//
// This will determine if InternetConfig is present
// and do the right thing.
//
// Everything else is taken care of behind the scenes.
//

// Note:
//------
// It reads "Everything else is taken care of behind the scenes" and that's right:
// the API below is ok. That's the "everything else" which is sub optimal.
// If you need to add a method in CInternetConfigInterface, I recommend you to
// have a look at how LaunchInternetConfigApplication() and GetInternetConfigString()
// are implemented. For a single method here, you need two methods "behind the scenes".

class CInternetConfigInterface
{
	public:

		static 	Boolean	CurrentlyUsingIC(void);
			// returns true iff IC is installed and we're listening to it

		static	ICError	LaunchInternetConfigApplication(ConstStr255Param key);
			// Lauches the app and opens one of the config panels (if specified).
			// The list of keys is in <ICKeys.h>.

		static	void	ConnectToInternetConfig();
			// gets the folder from CPrefs::GetFilePrototype(prefSpec(, MainFolder)

		static	void	DisconnectFromInternetConfig();
			// yeah, like this gets called

		static	void	GetInternetConfigString(ConstStr255Param icKey,
												Str255 s,
												long *port= nil);
			// If an error is encountered, s is set to "\p".
			// If port is not nil, then the following assumptions are made:
			//   * The string is a server name that may be have a ":portnumber"
			//     appended to it.
			//   * The initial value of *port is the default port number.
			// If port is not nil, then the returned string will have the ":number"
			// stripped and the *port value will be the port specified by the
			// user.

		static	ICError	GetInternetConfigFileMapping(	OSType fileType,
														OSType creator,
														ConstStr255Param filename,
														ICMapEntry *ent);
														
#ifndef MOZ_MAIL_NEWS
		static	ICError	SendInternetConfigURL(char *address);
#endif
			
		static	void	ResumeEvent();
			// somebody call me when I need to check the IC seed value
};
