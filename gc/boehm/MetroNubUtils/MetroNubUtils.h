// ===========================================================================
//	MetroNubUtils.h				©1996-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================

#ifndef __MetroNubUtils__
#define __MetroNubUtils__
#pragma once

#include <Types.h>

#ifdef __cplusplus
	extern "C" {
#endif


Boolean	IsMetroNubInstalled();		// returns true if a compatible version
									// of MetroNub is installed

Boolean IsMWDebuggerRunning();		// returns true if the Metrowerks debugger
									// is running

Boolean AmIBeingMWDebugged();		// returns true if this process is being
									// debugged by the Metrowerks debugger

#ifdef __cplusplus
	}
#endif

#endif