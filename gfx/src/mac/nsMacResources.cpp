/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsMacResources.h"
#include <Resources.h>
#include <MacWindows.h>


short nsMacResources::mRefNum				= kResFileNotOpened;
short nsMacResources::mSaveResFile	= 0;

pascal OSErr __NSInitialize(const CFragInitBlock *theInitBlock);
pascal OSErr __initializeResources(const CFragInitBlock *theInitBlock);

pascal void __NSTerminate(void);
pascal void __terminateResources(void);

//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------
pascal OSErr __initializeResources(const CFragInitBlock *theInitBlock)
{
    OSErr err = __NSInitialize(theInitBlock);
    if (err)
    	return err;

	short saveResFile = ::CurResFile();

	short refNum = FSpOpenResFile(theInitBlock->fragLocator.u.onDisk.fileSpec, fsRdPerm);
	nsMacResources::SetLocalResourceFile(refNum);

	::UseResFile(saveResFile);

	return ::ResError();
}


//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------
pascal void __terminateResources(void)
{
	::CloseResFile(nsMacResources::GetLocalResourceFile());
    __NSTerminate();
}

//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------

nsresult nsMacResources::OpenLocalResourceFile()
{
	if (mRefNum == kResFileNotOpened)
		return NS_ERROR_NOT_INITIALIZED;

	mSaveResFile = ::CurResFile();
	::UseResFile(mRefNum);

	return (::ResError() == noErr ? NS_OK : NS_ERROR_FAILURE);
}


//----------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------

nsresult nsMacResources::CloseLocalResourceFile()
{
	if (mRefNum == kResFileNotOpened)
		return NS_ERROR_NOT_INITIALIZED;

	::UseResFile(mSaveResFile);

	return (::ResError() == noErr ? NS_OK : NS_ERROR_FAILURE);
}

