// ===============================================================================
//	Appearance_DebugHeaders.pch++	©1995-1998 Metrowerks Inc. All rights reserved.
// ===============================================================================

#ifndef __BROWSERHEADERS_PCH_H__
#define __BROWSERHEADERS_PCH_H__

//
//	Source for precompiled header for PowerPlant headers
//
//	This file #includes most header files for the PowerPlant library,
//	as well as most of the Toolbox headers used by the PowerPlant library
//	with all debugging symbols defined.

#if __option(precompile)
	#if __MWERKS__ >= 0x2100
		// slightly larger but much faster generation of pch files
		#pragma faster_pch_gen on
	#endif


		// Option for using PowerPlant namespace
		#define PP_Uses_PowerPlant_Namespace		0	// off, don't use PowerPlant namespace

		// establish some essential PowerPlant macros:
		#define PP_StdDialogs_Option	PP_StdDialogs_ClassicOnly	// use classic standard dialog implementation

        #include <ControlDefinitions.h>
        
		// include powerplant headers
		#if wantDebugging
			#include <PP_DebugHeaders.cp>
		#else
			#include <PP_ClassHeaders.cp>
		#endif


		#if wantDebugging
			#define DEBUG 1
		#else
			#undef DEBUG
		#endif
		
		#include "DefinesMac.h"
		#include "DefinesMozilla.h"
	
	
		//	Support for automatically naming the precompiled header file ...
		#if __POWERPC__
			#if wantDebugging
				#pragma precompile_target "BrowserHeadersDebug_pch"
			#else
				#pragma precompile_target "BrowserHeaders_pch"
			#endif
		#else
			#error "target currently unsupported"	
		#endif

#else

		// Load the precompiled header file
		#if __POWERPC__
			#if wantDebugging
				#include "BrowserHeadersDebug_pch"
			#else
				#include "BrowserHeaders_pch"		
			#endif
		#else
			#error "target currently unsupported"	
		#endif


#endif

#endif // __BROWSERHEADERS_PCH_H__
