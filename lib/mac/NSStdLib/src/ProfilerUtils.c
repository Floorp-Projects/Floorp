/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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


#ifdef DEBUG

#include <profiler.h>
#include <Events.h>
#include "ProfilerUtils.h"

//---------------------------------------------

static Boolean sProfileInProgress = false;
void ProfileStart()
{
	if (! sProfileInProgress)
	{
		sProfileInProgress = true;
		if (ProfilerInit(collectDetailed, microsecondsTimeBase, 5000, 500))
			return;
		ProfilerSetStatus(true);
	}
}

//---------------------------------------------
void ProfileStop()
{
	if (sProfileInProgress)
	{
		ProfilerDump("\pMozilla Profile");
		ProfilerTerm();
		sProfileInProgress = false;
	}
}

//---------------------------------------------
void ProfileSuspend()
{
	if (sProfileInProgress)
		ProfilerSetStatus(false);
}

//---------------------------------------------
void ProfileResume()
{
	if (sProfileInProgress)
		ProfilerSetStatus(true);
}

//---------------------------------------------
Boolean ProfileInProgress()
{
	return sProfileInProgress;
}

#endif //DEBUG
