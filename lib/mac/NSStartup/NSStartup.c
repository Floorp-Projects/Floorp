/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Patrick Beard <beard@netscape.com>
 */

/*
	NSStartup.c
 */

#include <CodeFragments.h>
#include <stdio.h>
#include <string.h>

#include "gc_fragments.h"

extern char __code_start__[];				/*	(defined by linker)	*/
extern char	__code_end__[];					/*	(defined by linker)	*/
extern char __data_start__[];				/*	(defined by linker)	*/
extern char __data_end__[];					/*	(defined by linker)	*/

/* standard __initialize/__terminate routines. */
extern pascal OSErr __initialize(const CFragInitBlock* initBlock);
extern pascal void __terminate(void);

pascal OSErr __NSInitialize(const CFragInitBlock* initBlock);
pascal void __NSTerminate(void);

pascal OSErr __NSInitialize(const CFragInitBlock* initBlock)
{
	//  let the GC know about this library.	
	GC_register_fragment(__data_start__, __data_end__ + sizeof(char*),
	                     __code_start__, __code_end__,
	                     initBlock->fragLocator.u.onDisk.fileSpec);

	return __initialize(initBlock);
}

pascal void __NSTerminate()
{
	__terminate();

	// remove this library's global roots.
	GC_unregister_fragment(__data_start__, __data_end__ + sizeof(char*),
	                       __code_start__, __code_end__);
}
