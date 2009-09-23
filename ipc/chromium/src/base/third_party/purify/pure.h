/*
 * Header file of Pure API function declarations.
 *
* (C) Copyright IBM Corporation. 2006, 2006. All Rights Reserved.
   *   You may recompile and redistribute these definitions as required.
 *
 * Version 1.0
 */

#if defined(PURIFY) || defined(QUANTIFY)

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

// Don't include this file directly, use purify.h instead.
// If you need something that's not there, add it.
#ifdef PURIFY_PRIVATE_INCLUDE

#define PURE_H_VERSION 1
#include <stddef.h>

//////////////////////////////
// API's Specific to Purify //
//////////////////////////////

// TRUE when Purify is running.
int __cdecl PurifyIsRunning(void)			;
//
// Print a string to the viewer.
//
int __cdecl PurePrintf(const char *fmt, ...)		;
int __cdecl PurifyPrintf(const char *fmt, ...)		;
//
// Purify functions for leak and memory-in-use functionalty.
//
size_t __cdecl PurifyNewInuse(void)			;
size_t __cdecl PurifyAllInuse(void) 			;
size_t __cdecl PurifyClearInuse(void)			;
size_t __cdecl PurifyNewLeaks(void)			;
size_t __cdecl PurifyAllLeaks(void)			;
size_t __cdecl PurifyClearLeaks(void)			;
//
// Purify functions for handle leakage.
//
size_t __cdecl PurifyAllHandlesInuse(void)			;
size_t __cdecl PurifyNewHandlesInuse(void)			;
//
// Functions that tell you about the state of memory.
//
size_t __cdecl PurifyDescribe(void *addr)			;
size_t __cdecl PurifyWhatColors(void *addr, size_t size) 	;
//
// Functions to test the state of memory.  If the memory is not
// accessable, an error is signaled just as if there were a memory
// reference and the function returns false.
//
int __cdecl PurifyAssertIsReadable(const void *addr, size_t size)	;	// size used to be an int, until IA64 came along
int __cdecl PurifyAssertIsWritable(const void *addr, size_t size)	;
//
// Functions to test the state of memory.  If the memory is not
// accessable, these functions return false.  No error is signaled.
//
int __cdecl PurifyIsReadable(const void *addr, size_t size)	;
int __cdecl PurifyIsWritable(const void *addr, size_t size)	;
int __cdecl PurifyIsInitialized(const void *addr, size_t size)	;
//
// Functions to set the state of memory.
//
void __cdecl PurifyMarkAsInitialized(void *addr, size_t size)	;
void __cdecl PurifyMarkAsUninitialized(void *addr, size_t size)	;
//
// Functions to do late detection of ABWs, FMWs, IPWs.
//
#define PURIFY_HEAP_CRT 					(HANDLE) ~(__int64) 1 /* 0xfffffffe */
#define PURIFY_HEAP_ALL 					(HANDLE) ~(__int64) 2 /* 0xfffffffd */
#define PURIFY_HEAP_BLOCKS_LIVE 			0x80000000
#define PURIFY_HEAP_BLOCKS_DEFERRED_FREE 	0x40000000
#define PURIFY_HEAP_BLOCKS_ALL 				(PURIFY_HEAP_BLOCKS_LIVE|PURIFY_HEAP_BLOCKS_DEFERRED_FREE)
int __cdecl PurifyHeapValidate(unsigned int hHeap, unsigned int dwFlags, const void *addr)	;
int __cdecl PurifySetLateDetectScanCounter(int counter);
int __cdecl PurifySetLateDetectScanInterval(int seconds);
//
// Functions to support pool allocators
//
void   __cdecl   PurifySetPoolId(const void *mem, int id);
int	   __cdecl   PurifyGetPoolId(const void *mem);
void   __cdecl   PurifySetUserData(const void *mem, void *data);
void * __cdecl   PurifyGetUserData(const void *mem);
void   __cdecl   PurifyMapPool(int id, void(*fn)());


////////////////////////////////
// API's Specific to Quantify //
////////////////////////////////

// TRUE when Quantify is running.
int __cdecl QuantifyIsRunning(void)			;

//
// Functions for controlling collection
//
int __cdecl QuantifyDisableRecordingData(void)		;
int __cdecl QuantifyStartRecordingData(void)		;
int __cdecl QuantifyStopRecordingData(void)		;
int __cdecl QuantifyClearData(void)			;
int __cdecl QuantifyIsRecordingData(void)		;

// Add a comment to the dataset
int __cdecl QuantifyAddAnnotation(char *)		;

// Save the current data, creating a "checkpoint" dataset
int __cdecl QuantifySaveData(void)			;

// Set the name of the current thread in the viewer
int __cdecl QuantifySetThreadName(char *)		;

////////////////////////////////
// API's Specific to Coverage //
////////////////////////////////

// TRUE when Coverage is running.
int __cdecl CoverageIsRunning(void)			;
//
// Functions for controlling collection
//
int __cdecl CoverageDisableRecordingData(void)		;
int __cdecl CoverageStartRecordingData(void)		;
int __cdecl CoverageStopRecordingData(void)		;
int __cdecl CoverageClearData(void)			;
int __cdecl CoverageIsRecordingData(void)		;
// Add a comment to the dataset
int __cdecl CoverageAddAnnotation(char *)		;

// Save the current data, creating a "checkpoint" dataset
int __cdecl CoverageSaveData(void)			;


#endif // PURIFY_PRIVATE_INCLUDE

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif // defined(PURIFY) || defined(QUANTIFY)