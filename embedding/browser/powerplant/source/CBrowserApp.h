/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#pragma once

#include <PP_Prefix.h>
#include <LApplication.h>

#ifndef nsError_h
#include "nsError.h"
#endif

 
class	CBrowserApp : public PP_PowerPlant::LApplication
{

public:
					        CBrowserApp();	// constructor registers PPobs
	virtual 			    ~CBrowserApp();	// stub destructor


    virtual void            ProcessNextEvent();

    virtual void            HandleAppleEvent(const AppleEvent&	inAppleEvent,
                                             AppleEvent&			outAEReply,
                                             AEDesc&				outResult,
                                             long				    inAENumber);

	// this overriding method handles application commands
	virtual Boolean		    ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam);	


	// this overriding method returns the status of menu items
	virtual void            FindCommandStatus(PP_PowerPlant::CommandT inCommand,
            								  Boolean &outEnabled, Boolean &outUsesMark,
            								  PP_PowerPlant::Char16 &outMark, Str255 outName);
								
   virtual Boolean          AttemptQuitSelf(SInt32 inSaveOption);

									
protected:
		
	virtual void            StartUp();			// override startup functions
	virtual void		    MakeMenuBar();

	virtual nsresult        InitializePrefs();
    static nsresult         InitCachePrefs();

    virtual Boolean         SelectFileObject(PP_PowerPlant::CommandT	inCommand,
                                             FSSpec& outSpec);
};
