// ===========================================================================
//	MetroNubUtils.c				©1996-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================

#ifndef __MetroNubUtils__
#include "MetroNubUtils.h"
#endif

#ifndef __MetroNubUserInterface__
#include "MetroNubUserInterface.h"
#endif

#ifndef __GESTALT__
#include <Gestalt.h>
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif


static MetroNubUserEntryBlock*	gMetroNubEntry = NULL;



Boolean	IsMetroNubInstalled()
{
	static Boolean lookedForMetroNub = false;
	
	if (! lookedForMetroNub)
	{
		long	result;
		
		// look for MetroNub's Gestalt selector
		if (Gestalt(kMetroNubUserSignature, &result) == noErr)
		{
			MetroNubUserEntryBlock* block = (MetroNubUserEntryBlock *)result;
			
			// make sure the version of the API is compatible
			if (block->apiLowVersion <= kMetroNubUserAPIVersion &&
				kMetroNubUserAPIVersion <= block->apiHiVersion)
				gMetroNubEntry = block;		// success!
		}
		
		lookedForMetroNub = true;
	}

	return (gMetroNubEntry != NULL);
}


Boolean IsMWDebuggerRunning()
{
	if (IsMetroNubInstalled())
		return CallIsDebuggerRunningProc(gMetroNubEntry->isDebuggerRunning);
	else
		return false;
}



Boolean AmIBeingMWDebugged()
{
	if (IsMetroNubInstalled())
		return CallAmIBeingDebuggedProc(gMetroNubEntry->amIBeingDebugged);
	else
		return false;
}
