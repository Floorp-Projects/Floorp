/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __PPHEADERS_PCH_H__
#define __PPHEADERS_PCH_H__

//--------------------------------------------------------------------------------
//
//	Source for precompiled header for mozilla PowerPlant projects
//
//--------------------------------------------------------------------------------

#if __option(precompile)
	#if __MWERKS__ >= 0x2100
		// slightly larger but much faster generation of pch files
		#pragma faster_pch_gen on
	#endif

        // include mozilla prefix file
		#if wantDebugging
		    #include "MacPrefix_debug.h"
		#else
		    #include "MacPrefix.h"
		#endif
        
        // PowerPlant definitions
		#define PP_Uses_PowerPlant_Namespace		0
		#define PP_Suppress_Notes_22                1
		#define PP_Uses_Aqua_MenuBar                1
        		
        #if !defined(TARGET_CARBON) || !TARGET_CARBON
            #define ACCESSOR_CALLS_ARE_FUNCTIONS 1		
        #endif

		#if TARGET_CARBON
		    #define PP_Target_Carbon 1
            #define PP_StdDialogs_Option	PP_StdDialogs_NavServicesOnly		
		#else
            #define PP_StdDialogs_Option	PP_StdDialogs_Conditional
		#endif
		
		
        
		// include powerplant headers
		#if wantDebugging
			#include <PP_DebugHeaders.cp>
		#else
			#include <PP_ClassHeaders.cp>
		#endif

		//	Support for automatically naming the precompiled header file ...
		#if __POWERPC__
			#if wantDebugging
				#pragma precompile_target "PPHeadersDebug_pch"
			#else
				#pragma precompile_target "PPHeaders_pch"
			#endif
		#else
			#error "target currently unsupported"	
		#endif

#else

		// Load the precompiled header file
		#if __POWERPC__
			#if wantDebugging
				#include "PPHeadersDebug_pch"
			#else
				#include "PPHeaders_pch"		
			#endif
		#else
			#error "target currently unsupported"	
		#endif


#endif

#endif // __PPHEADERS_PCH_H__
