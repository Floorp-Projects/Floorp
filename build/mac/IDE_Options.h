/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*

	This file overrides all option settings in the IDE.  It is an attempt to allow all builds
	to have the same options.

	Note: We can't use ConditionalMacros.h in this file because it will conflict with 
	the PowerPlant precompiled headers.

*/

	/* warning pragmas */
#pragma warn_hidevirtual 		on
#pragma warn_emptydecl 			on
#pragma warn_unusedvar 			on
#pragma warn_extracomma 		on
#pragma warn_illpragma 			on
#pragma warn_possunwant 		on
#pragma warn_unusedarg 			off		/* turned off to reduce warnings */

#pragma check_header_flags 		on

	/* Language features that must be the same across libraries... */
#pragma enumsalwaysint			on
#pragma unsigned_char			off
#pragma exceptions				on
#pragma bool 					on
#pragma wchar_type              on
#pragma RTTI                    on


	/* Save as much space as possible with strings... */
#pragma pool_strings				on
#pragma dont_reuse_strings			off

#pragma options align=native
#pragma sym 						on				/* Takes no memory.  OK in non-debug. */



#ifdef powerc /* ...generating PowerPC */
	#pragma toc_data 				on
	#pragma fp_contract 			on
	#pragma readonly_strings 		on

	#ifdef DEBUG
		#pragma profile 		off							/* Turn this on to profile the application. */
																				/* Look for more details about profiling in nsMacMessagePump.cpp.  */
		#pragma traceback 			on
		#pragma global_optimizer 	off
		#pragma scheduling 			off
		#pragma peephole 			off
		#pragma optimize_for_size 	off
	#else
		#pragma traceback 			on					/* leave on until the final release, so MacsBug logs are interpretable */
		#pragma global_optimizer 	on
		#pragma optimization_level 	4
		#pragma scheduling 			603
		#pragma peephole 			on
		#pragma optimize_for_size	on

    #pragma opt_strength_reduction on
    #pragma opt_propagation on
    #pragma opt_loop_invariants on
    #pragma opt_lifetimes on
    #pragma opt_dead_code on
    #pragma opt_dead_assignments on
    #pragma opt_common_subs on
	#endif

#else /* ...generating 68k */
	#pragma code68020 			on
	#pragma code68881 			off

		/* Far everything... */
	#pragma far_code
	#pragma far_data 			on
	#pragma far_strings 		on
	#pragma far_vtables 		on

	#pragma fourbyteints		on	/* 4-byte ints */
	#pragma IEEEdoubles			on	/* 8-byte doubles (as required by Java and NSPR) */

	#ifdef DEBUG
		#pragma macsbug 		on
		#pragma oldstyle_symbols off
	#else
		#pragma macsbug 		off
	#endif
#endif
