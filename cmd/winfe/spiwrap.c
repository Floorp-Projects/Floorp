//--------------------------------------------------------------------
//  spiwrap.c
//
//  Created: 11/20/97
//  Author:  Matt Kendall
//
//  Copyright (C) 1997 Full Circle Software All Rights Reserved
//
//  Dynamically Load Spiral API
//
//--------------------------------------------------------------------
#include <windows.h>
#include <stdarg.h>

#include "fullsoft.h"

#define FC_WRAPPER_VERSION                             2
#define FC_WRAPPER_MINIMUM_ACCEPTABLE_LIBRARY_VERSION  1

#if !defined(_WINDOWS)
#error This is a Windows only file
#endif // _WINDOWS

#if !defined(_WIN32) && !defined(_WIN16)
#define _WIN16
#endif // ! _WIN32 && ! _WIN16

#if defined(_WIN32)
#define DEFINE_CALLNAME(s) const char sz##s[] = #s
#else // ! _WIN32
#define DEFINE_CALLNAME(s) const char sz##s[] = "_"#s
void FreeOrphanFCLoads() ;
#endif // ! _WIN32

void FCUnloadLibrary() ;
BOOL FCIsValidFullsoftLibrary(const char *) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCInitialize)( void ) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCCreateKey)(
	FC_KEY key,
	FC_DATA_TYPE type,
	FC_UINT32 first_count,
	FC_UINT32 last_count,
	FC_UINT32 max_element_size) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCCreatePersistentKey)(
	FC_KEY key,
	FC_DATA_TYPE type,
	FC_UINT32 first_count,
	FC_UINT32 last_count,
	FC_UINT32 max_element_size) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCAddDataToKey)(
	FC_KEY key,
	FC_PVOID buffer,
	FC_UINT32 data_length) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCAddIntToKey)(
	FC_KEY key,
	FC_UINT32 data) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCAddStringToKey)(
	FC_KEY key,
	FC_STRING string) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCRegisterMemory)(
	FC_KEY key,
	FC_DATA_TYPE type,
	FC_PVOID buffer,
	FC_UINT32 length,
	FC_UINT32 dereference_count,
	FC_CONTEXT context) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCUnregisterMemory)(
	FC_CONTEXT context) ;

typedef FC_ERROR (FCAPI __cdecl  *t_FCAddDateToKey)(
    FC_KEY key,
    FC_DATE date) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCSetCounter)(
    FC_KEY key,
    FC_UINT32 value) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCIncrementCounter)(
    FC_KEY key,
    FC_UINT32 value) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCTrigger)( FC_TRIGGER trigger ) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCTriggerInternal)(
    FC_TRIGGER szTrigger,
    FC_UINT32 eip,
    FC_UINT32 esp,
    FC_UINT32 ebp) ;

typedef void (FCAPI __cdecl *t_FCTraceInternal)(FC_STRING fmt,void FAR *) ;

typedef void (FCAPI __cdecl *t_FCAssertInternal)(unsigned long) ;

typedef FC_ERROR (FCAPI __cdecl *t_FCCleanup)( void ) ;

typedef int  (FCAPI __cdecl *t_FCOrphanLoadCount)() ;

typedef void (FCAPI __cdecl *t_FCAssertParamInternal)(FC_UINT32 pc,FC_UINT32 mask,FC_UINT32 level ) ;

typedef void (FCAPI __cdecl *t_FCTraceParamInternal)(FC_UINT32 mask,FC_UINT32 level,FC_STRING fmt,void FAR *args ) ;

typedef FC_UINT32 (FCAPI __cdecl *t_FCLibraryVersion)( FC_UINT32 wrap_version ) ;

typedef FC_ERROR  (FCAPI __cdecl *t_FCInitializeInternal)( FC_UINT32 wrap_version ) ;

DEFINE_CALLNAME( FCInitialize ) ;
DEFINE_CALLNAME( FCCreateKey ) ;
DEFINE_CALLNAME( FCCreatePersistentKey ) ;
DEFINE_CALLNAME( FCAddDataToKey ) ;
DEFINE_CALLNAME( FCAddIntToKey ) ;
DEFINE_CALLNAME( FCAddStringToKey ) ;
DEFINE_CALLNAME( FCRegisterMemory ) ;
DEFINE_CALLNAME( FCUnregisterMemory ) ;
DEFINE_CALLNAME( FCAddDateToKey ) ;
DEFINE_CALLNAME( FCSetCounter ) ;
DEFINE_CALLNAME( FCIncrementCounter ) ;
DEFINE_CALLNAME( FCTrigger ) ;
DEFINE_CALLNAME( FCTriggerInternal ) ;
DEFINE_CALLNAME( FCTraceInternal ) ;
DEFINE_CALLNAME( FCAssertInternal ) ;
DEFINE_CALLNAME( FCCleanup ) ;
DEFINE_CALLNAME( FCOrphanLoadCount ) ;
DEFINE_CALLNAME( FCAssertParamInternal ) ;
DEFINE_CALLNAME( FCTraceParamInternal ) ;
DEFINE_CALLNAME( FCLibraryVersion ) ;
DEFINE_CALLNAME( FCInitializeInternal ) ;

t_FCInitialize pfnFCInitialize ;
t_FCCreateKey pfnFCCreateKey ;
t_FCCreatePersistentKey pfnFCCreatePersistentKey ;
t_FCAddDataToKey pfnFCAddDataToKey ;
t_FCAddIntToKey pfnFCAddIntToKey ;
t_FCAddStringToKey pfnFCAddStringToKey ;
t_FCAddDateToKey pfnFCAddDateToKey ;
t_FCSetCounter pfnFCSetCounter ;
t_FCIncrementCounter pfnFCIncrementCounter ;
t_FCRegisterMemory pfnFCRegisterMemory ;
t_FCUnregisterMemory pfnFCUnregisterMemory ;
t_FCTrigger pfnFCTrigger ;

// undocumented dll functions
t_FCTraceInternal pfnFCTraceInternal ;
t_FCAssertInternal pfnFCAssertInternal ;
t_FCCleanup pfnFCCleanup ;
t_FCAssertParamInternal pfnFCAssertParamInternal ;
t_FCTraceParamInternal pfnFCTraceParamInternal ;
t_FCLibraryVersion pfnFCLibraryVersion ;
t_FCInitializeInternal pfnFCInitializeInternal ;
t_FCTriggerInternal pfnFCTriggerInternal ;

static HMODULE gMod = NULL;

void ExitFunction( void );
void ExitFunction( void )
{
	if( pfnFCCleanup!=NULL ) {
		pfnFCCleanup() ;
	}
	if( gMod!=NULL ) {
		FreeLibrary( gMod ) ;
	}
}

FC_ERROR FCAPI
FCInitialize() {
	FC_UINT32 dwLibraryVersion ;
	char szFullsoftLibrary[MAX_PATH] ;
	char *p,*pEnd,*pLastSlash ;
	HMODULE hMod;

#if defined(_WIN16)
	// If Spiral crashed in Win16 than "spiral.dll" will be left in memory with
	// a bogus reference count.  The following routine identifies this and
	// cleans up
	FreeOrphanFCLoads() ;
#endif // _WIN16

	if ( atexit(ExitFunction) != 0 ) {
		return FC_ERROR_CANT_INITIALIZE;
	}

	// Build up a fully qualified path to FULLSOFT.DLL
	GetModuleFileName( GetModuleHandle(NULL) , szFullsoftLibrary , MAX_PATH ) ;
	pEnd = szFullsoftLibrary+MAX_PATH ;
	pLastSlash = szFullsoftLibrary ;
	for( p = szFullsoftLibrary ; (p<pEnd) && *p ; p++ ) {
		if( *p=='\\' ) {
			pLastSlash = p ;
		}
	}
	if( *pLastSlash=='\\' ) {
		pLastSlash++ ;
	}
	strcpy(pLastSlash,"FULLSOFT.DLL") ;

	// Check for our copyright string in FULLSOFT.DLL
	if( FCIsValidFullsoftLibrary(szFullsoftLibrary)==FALSE ) {
		return FC_ERROR_CANT_INITIALIZE ;
	}

	// Load the library
	hMod = LoadLibrary(szFullsoftLibrary) ;
#if defined(_WIN32)
	if( hMod==NULL ) {
		return FC_ERROR_CANT_INITIALIZE ;
	}
#else // !_WIN32
	if( hMod<HINSTANCE_ERROR ) {
		hMod = NULL ;
		return FC_ERROR_CANT_INITIALIZE ;
	}
#endif // !_WIN32

	gMod = hMod;

	pfnFCInitialize = (t_FCInitialize)GetProcAddress(hMod,szFCInitialize) ;
	pfnFCCreateKey = (t_FCCreateKey)GetProcAddress(hMod, szFCCreateKey) ;
	pfnFCCreatePersistentKey = (t_FCCreatePersistentKey)GetProcAddress(hMod, szFCCreatePersistentKey) ;
	pfnFCAddDataToKey = (t_FCAddDataToKey)GetProcAddress(hMod, szFCAddDataToKey) ;
	pfnFCAddIntToKey = (t_FCAddIntToKey)GetProcAddress(hMod, szFCAddIntToKey) ;
	pfnFCAddStringToKey = (t_FCAddStringToKey)GetProcAddress(hMod, szFCAddStringToKey) ;
    pfnFCAddDateToKey = (t_FCAddDateToKey)GetProcAddress(hMod, szFCAddDateToKey) ;
    pfnFCSetCounter = (t_FCSetCounter)GetProcAddress(hMod, szFCSetCounter) ;
    pfnFCIncrementCounter = (t_FCIncrementCounter)GetProcAddress(hMod, szFCIncrementCounter) ;
	pfnFCRegisterMemory = (t_FCRegisterMemory)GetProcAddress(hMod, szFCRegisterMemory) ;
	pfnFCUnregisterMemory = (t_FCUnregisterMemory)GetProcAddress(hMod, szFCUnregisterMemory) ;
    pfnFCTrigger = (t_FCTrigger)GetProcAddress(hMod, szFCTrigger) ;
	
	pfnFCTraceInternal = (t_FCTraceInternal)GetProcAddress(hMod, szFCTraceInternal) ;
	pfnFCAssertInternal = (t_FCAssertInternal)GetProcAddress(hMod, szFCAssertInternal) ;
	pfnFCCleanup = (t_FCCleanup)GetProcAddress(hMod,szFCCleanup) ;
	pfnFCAssertParamInternal = (t_FCAssertParamInternal)GetProcAddress(hMod,szFCAssertParamInternal) ;
	pfnFCTraceParamInternal = (t_FCTraceParamInternal)GetProcAddress(hMod,szFCTraceParamInternal)  ;
	pfnFCLibraryVersion = (t_FCLibraryVersion)GetProcAddress(hMod,szFCLibraryVersion) ;
	pfnFCInitializeInternal = (t_FCInitializeInternal)GetProcAddress(hMod,szFCInitializeInternal) ;
	pfnFCTriggerInternal = (t_FCTriggerInternal)GetProcAddress(hMod, szFCTriggerInternal) ;

	// Get the library version and ensure it is compatible
	if( pfnFCLibraryVersion==NULL ) {
		FCUnloadLibrary() ;
		return FC_ERROR_CANT_INITIALIZE ;
	}
	dwLibraryVersion = pfnFCLibraryVersion( FC_WRAPPER_VERSION ) ;
	if( dwLibraryVersion<FC_WRAPPER_MINIMUM_ACCEPTABLE_LIBRARY_VERSION ) {
		FCUnloadLibrary() ;
		return FC_ERROR_CANT_INITIALIZE ;
	}

	// If we were able to load and locate the library's initialize routine
	// call it now
	if( pfnFCInitialize==NULL ) {
		FCUnloadLibrary() ;
		return FC_ERROR_CANT_INITIALIZE ;
	} else {
		return pfnFCInitializeInternal(FC_WRAPPER_VERSION) ;
	}
}

FC_ERROR FCAPI
FCCreateKey(
    FC_KEY key,
    FC_DATA_TYPE type,
    FC_UINT32 first_count,
    FC_UINT32 last_count,
    FC_UINT32 max_element_size) 
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCCreateKey!=NULL ) {
		err = pfnFCCreateKey(key,type,first_count,last_count,max_element_size) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCCreatePersistentKey(
    FC_KEY key,
    FC_DATA_TYPE type,
    FC_UINT32 first_count,
    FC_UINT32 last_count,
    FC_UINT32 max_element_size) 
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCCreateKey!=NULL ) {
		err = pfnFCCreatePersistentKey(key,type,first_count,last_count,max_element_size) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCAddDataToKey(
    FC_KEY key,
    FC_PVOID buffer,
    FC_UINT32 data_length) 
{
	if( pfnFCAddDataToKey!=NULL ) {
		pfnFCAddDataToKey(key,buffer,data_length) ;
	}
	return FC_ERROR_OK ;
}

FC_ERROR FCAPI
FCAddIntToKey(
    FC_KEY key,
    FC_UINT32 data)
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCAddIntToKey!=NULL ) {
		err = pfnFCAddIntToKey(key,data) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCAddStringToKey(
    FC_KEY key,
    FC_STRING string) 
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCAddStringToKey!=NULL ) {
		err = pfnFCAddStringToKey(key,string) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCRegisterMemory(
    FC_KEY key,
    FC_DATA_TYPE type,
    FC_PVOID buffer,
    FC_UINT32 length,
    FC_UINT32 dereference_count,
    FC_CONTEXT context) 
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCRegisterMemory!=NULL ) {
		pfnFCRegisterMemory(key,type,buffer,length,dereference_count,context) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCUnregisterMemory( FC_CONTEXT context ) {
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCUnregisterMemory!=NULL ) {
		pfnFCUnregisterMemory(context) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCAddDateToKey(
    FC_KEY key,
    FC_DATE date)
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCAddDateToKey!=NULL ) {
		err = pfnFCAddDateToKey(key,date) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCSetCounter(
    FC_KEY key,
    FC_UINT32 value)
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCSetCounter!=NULL ) {
		err = pfnFCSetCounter(key,value) ;
	}
	return err ;
}

FC_ERROR FCAPI
FCIncrementCounter(
    FC_KEY key,
    FC_UINT32 value)
{
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	if( pfnFCIncrementCounter!=NULL ) {
		err = pfnFCIncrementCounter(key,value) ;
	}
	return err ;
}

void FCAPI FCTrace(FC_STRING fmt,...) {
	if( pfnFCTraceInternal!=NULL ) {
		va_list args ;
		va_start(args,fmt) ;
		pfnFCTraceInternal(fmt,args) ;
		va_end(args) ;
	}
}

#if defined(_MSC_VER)
#pragma optimize("",off)
#else
#error  Only Microsoft compiler supported.  To fix this, enter the appropriate pragma
#error  here to ensure that a stack frame is generated by your compiler (or set your
#error  compiler command line switches) and remove this warning
#endif

#if defined(_WIN32)
void FCAPI FCAssert() {
	if( pfnFCAssertInternal!=NULL ) {
		FC_UINT32 pc ;
		_asm {
			mov eax,[ebp+4] ;
			mov pc,eax ;
		}
		pfnFCAssertInternal(pc) ;
	}
}

void FCAPI
FCAssertParam(
    FC_UINT32 mask,
    FC_UINT32 level )
{
	if( pfnFCAssertParamInternal!=NULL ) {
		FC_UINT32 pc ;
		_asm {
			push eax ;
			mov eax,[ebp+4] ;
			mov pc,eax ;
			pop eax ;
		}
		pfnFCAssertParamInternal(pc,mask,level) ;
	}
}

FC_ERROR FCAPI
FCTrigger(FC_TRIGGER trigger) {
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;
	FC_UINT32 dwEip,dwEsp,dwEbp ;
	
	// Get context for stack trace
	_asm {
		mov  eax, [ebp] ;
		mov  dwEbp, eax ;
		mov  eax,[ebp+4] ;
		mov  dwEip, eax ;
		mov  dwEsp, ebp ;
	}

	dwEsp += 8 ; // ebp points to return address
	
	if( pfnFCTriggerInternal!=NULL ) {
		pfnFCTriggerInternal(trigger,dwEip,dwEsp,dwEbp) ;
	}
	return err ;
}

#endif // _WIN32

#if defined(_WIN16)
void FCAPI FCAssert() {
	FC_UINT32 pc ;
	if( pfnFCAssertInternal!=NULL ) {
		_asm {
			mov  ax,[bp+4] ;
			mov  word ptr pc+2, ax ;
			mov  ax,[bp+2] ;
			mov  word ptr pc, ax ;
		}
		pfnFCAssertInternal(pc) ;
	}
}

void FCAPI
FCAssertParam(
    FC_UINT32 mask,
    FC_UINT32 level )
{
	if( pfnFCAssertParamInternal!=NULL ) {
		FC_UINT32 pc ;
		_asm {
			mov  ax,[bp+4] ;
			mov  word ptr pc+2, ax ;
			mov  ax,[bp+2] ;
			mov  word ptr pc, ax ;
		}
		pfnFCAssertParamInternal(pc,mask,level) ;
	}
}
#endif // _WIN16

#if defined(_MSC_VER)
#pragma optimize("",on)
#endif // _MSC_VER

void FCAPI
FCTraceParam(
    FC_UINT32 mask,
    FC_UINT32 level,
    FC_STRING fmt, ... )
{
	if( pfnFCTraceParamInternal!=NULL ) {
		va_list args ;
		va_start(args,fmt) ;
		pfnFCTraceParamInternal(mask,level,fmt,args) ;
		va_end(args) ;
	}
}

void FCUnloadLibrary() {
	if ( gMod != NULL ) {
		FreeLibrary( gMod );
		gMod = NULL;
	}
	
	pfnFCInitialize = NULL ;
	pfnFCCreateKey = NULL ;
	pfnFCAddDataToKey = NULL ;
	pfnFCAddIntToKey = NULL ;
	pfnFCAddStringToKey = NULL ;
	pfnFCAddDateToKey = NULL ;
	pfnFCSetCounter = NULL ;
	pfnFCIncrementCounter = NULL ;
	pfnFCRegisterMemory = NULL ;
	pfnFCTrigger = NULL ;

	pfnFCTraceInternal = NULL ;
	pfnFCAssertInternal = NULL ;
	pfnFCCleanup = NULL ;
	pfnFCAssertParamInternal = NULL ;
	pfnFCTraceParamInternal = NULL ;
	pfnFCLibraryVersion = NULL ;
	pfnFCInitializeInternal = NULL ;
}

#if defined(_WIN16)

FC_ERROR FCAPI
   FCTrigger(FC_TRIGGER trigger) {
	FC_ERROR err = FC_ERROR_NOT_INITIALIZED ;

	if( pfnFCTrigger!=NULL ) {
		pfnFCTrigger(trigger) ;
	}
	return err ;
}

void FreeOrphanFCLoads() {
	t_FCOrphanLoadCount FCOrphanLoadCount ;
	HMODULE hMod = GetModuleHandle("FULLSOFT") ;
	if( hMod!=NULL ) {
		FCOrphanLoadCount = (t_FCOrphanLoadCount)GetProcAddress(hMod,szFCOrphanLoadCount) ;
		if( FCOrphanLoadCount!=NULL ) {
			int nOrphans = FCOrphanLoadCount() ;
			for(;nOrphans;nOrphans--) {
				FreeLibrary(hMod) ;
			}
		}
	}
}
#endif // _WIN16

#if defined(_WIN32)

// 'Scrambled' version of the string we expect to find in the library.
const char FullsoftCopyright[]	=
    { '\x50', '\xcc', '\x49', '\xcf', '\x4e', '\xc4', '\x38', '\xad',
      '\x2e', '\x5b', '\x90', '\x00', '\x36', '\x63', '\xa1', '\xe7',
      '\x2d', '\x72', '\xab', '\xd8', '\x2b', '\xad', '\x26', '\x9f',
      '\xcc', '\x1c', '\x92', '\x11', '\x81', '\xfa', '\x6c', '\x99',
      '\xf9', '\x75', '\xe8', '\x69', '\xed', '\x5b', '\xda', '\x4c',
      '\x85', '\xb2', '\x08', '\x83', '\xf3', '\x2e', '\x3b' };

BOOL FCIsValidFullsoftLibrary(const char *szFile) {
	BOOL fRet = FALSE ;
	HANDLE hFile = INVALID_HANDLE_VALUE ;
	IMAGE_DOS_HEADER idh ;
	IMAGE_FILE_HEADER ifh ;
	DWORD dw ;
	char buffer[sizeof(FullsoftCopyright)] ;
	char		ch;
	char		output;
	char		last = 0;
	const char	*infoString ;
	const char	*expectString ;
	int         i ;

	hFile = CreateFile(szFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL) ;
	if( hFile==INVALID_HANDLE_VALUE ) {
		goto OnError ;
	}

	if( ReadFile(hFile,&idh,sizeof(idh),&dw,NULL)==FALSE ) {
		goto OnError ;
	}

	SetFilePointer(hFile,idh.e_lfanew+sizeof(DWORD),NULL,FILE_BEGIN) ;

	if( ReadFile(hFile,&ifh,sizeof(ifh),&dw,NULL)==FALSE ) {
		goto OnError ;
	}

	SetFilePointer(hFile,
				   ifh.SizeOfOptionalHeader + ifh.NumberOfSections * sizeof(IMAGE_SECTION_HEADER),
				   NULL,FILE_CURRENT) ;

	if( ReadFile(hFile,buffer,sizeof(FullsoftCopyright),&dw,NULL)==FALSE ) {
		goto OnError ;
	}

	infoString = buffer ;
	expectString = FullsoftCopyright ;
	
	// Compare against scrambled text.
	for( i=0 ; i<sizeof(FullsoftCopyright) ; i++ ) {
		ch = *expectString;
		output = ch - last - 13;
		last = ch;
		if( output!=*infoString ) {
			// Someone's different. Go get 'em, lawyers!
			break;
		}
		infoString++;
		expectString++;
	}
	if( i==sizeof(FullsoftCopyright) ) {
		fRet = TRUE ;
	}

OnError:
	if( hFile!=INVALID_HANDLE_VALUE) {
		CloseHandle( hFile ) ;
	}
	return fRet ;
}
#endif // _WIN32
