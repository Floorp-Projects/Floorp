// ===========================================================================
//	MetroNubUserInterface.h		©1996-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================

#ifndef __MetroNubUserInterface__
#define __MetroNubUserInterface__
#pragma once

#include <CodeFragments.h>

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

#ifdef __cplusplus
	extern "C" {
#endif


// ---------------------------------------------------------------------------
//		¥ Constants
// ---------------------------------------------------------------------------

const short kMetroNubUserAPIVersion = 1;		// current User API version
const OSType kMetroNubUserSignature = 'MnUI';


// ---------------------------------------------------------------------------
//		¥ IsDebuggerRunning
// ---------------------------------------------------------------------------

// pascal Boolean IsDebuggerRunning ();

typedef pascal Boolean (*IsDebuggerRunningProcPtr)();

#if GENERATINGCFM
typedef UniversalProcPtr IsDebuggerRunningUPP;
#else
typedef IsDebuggerRunningProcPtr IsDebuggerRunningUPP;
#endif

enum {
	uppIsDebuggerRunningProcInfo = kPascalStackBased
		 | RESULT_SIZE(SIZE_CODE(sizeof(Boolean)))
};

#if GENERATINGCFM
#define NewIsDebuggerRunningProc(userRoutine)		\
		(IsDebuggerRunningUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppIsDebuggerRunningProcInfo, GetCurrentArchitecture())
#else
#define NewIsDebuggerRunningProc(userRoutine)		\
		((IsDebuggerRunningUPP) (userRoutine))
#endif

#if GENERATINGCFM
#define CallIsDebuggerRunningProc(userRoutine)		\
		CallUniversalProc((UniversalProcPtr)(userRoutine), uppIsDebuggerRunningProcInfo)
#else
#define CallIsDebuggerRunningProc(userRoutine)		\
			(*(userRoutine))()
#endif


// ---------------------------------------------------------------------------
//		¥ AmIBeingDebugged
// ---------------------------------------------------------------------------

// pascal Boolean AmIBeingDebugged ();

typedef pascal Boolean (*AmIBeingDebuggedProcPtr)();

#if GENERATINGCFM
typedef UniversalProcPtr AmIBeingDebuggedUPP;
#else
typedef AmIBeingDebuggedProcPtr AmIBeingDebuggedUPP;
#endif

enum {
	uppAmIBeingDebuggedProcInfo = kPascalStackBased
		 | RESULT_SIZE(SIZE_CODE(sizeof(Boolean)))
};

#if GENERATINGCFM
#define NewAmIBeingDebuggedProc(userRoutine)		\
		(AmIBeingDebuggedUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppAmIBeingDebuggedProcInfo, GetCurrentArchitecture())
#else
#define NewAmIBeingDebuggedProc(userRoutine)		\
		((AmIBeingDebuggedUPP) (userRoutine))
#endif

#if GENERATINGCFM
#define CallAmIBeingDebuggedProc(userRoutine)		\
		CallUniversalProc((UniversalProcPtr)(userRoutine), uppAmIBeingDebuggedProcInfo)
#else
#define CallAmIBeingDebuggedProc(userRoutine)		\
			(*(userRoutine))()
#endif


// ---------------------------------------------------------------------------
//		¥ MetroNubUserEntryBlock
// ---------------------------------------------------------------------------

struct MetroNubUserEntryBlock
{
	long					blockLength;		// length of this block
	short					apiLowVersion;		// lowest supported version of the Nub API
	short					apiHiVersion;		// highest supported version of the Nub API
	Str31					nubVersion;			// short version string from 'vers' 1 resource
		
	IsDebuggerRunningUPP	isDebuggerRunning;
	AmIBeingDebuggedUPP		amIBeingDebugged;
};
typedef struct MetroNubUserEntryBlock MetroNubUserEntryBlock;

#ifdef __cplusplus
	}
#endif

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

#endif