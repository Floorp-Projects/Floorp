/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL. You may obtain a copy of the NPL at  
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation. Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights    
 * Reserved. */

#include <Files.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "gc.h"
#include "gc_fragments.h"
#include "generic_threads.h"

#ifndef GC_LEAK_DETECTOR

void MWUnmangle(const char *mangled_name, char *unmangled_name, size_t buffersize);

// stub implementations, when GC leak detection isn't on. these are needed so that
// NSStdLib has something to export for these functions, even when the GC isn't used.
void GC_register_fragment(char* dataStart, char* dataEnd,
                          char* codeStart, char* codeEnd,
                          const FSSpec* fragmentSpec) {}
void GC_unregister_fragment(char* dataStart, char* dataEnd,
                            char* codeStart, char* codeEnd) {}
void GC_clear_roots() {}
void GC_generic_init_threads() {}
void GC_gcollect() {}

FILE* GC_stdout = NULL;
FILE* GC_stderr = NULL;

int GC_address_to_source(char* codeAddr, char fileName[256], UInt32* fileOffset)
{
	return 0;
}

void MWUnmangle(const char *mangled_name, char *unmangled_name, size_t buffersize)
{
	strncpy(unmangled_name, mangled_name, buffersize);
}

#endif
