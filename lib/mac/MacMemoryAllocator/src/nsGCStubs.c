/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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

int GC_address_to_source(char* codeAddr, char symbolName[256], char fileName[256], UInt32* fileOffset)
{
	return 0;
}

GC_PTR GC_malloc_atomic(size_t size_in_bytes) { return NULL; }

void MWUnmangle(const char *mangled_name, char *unmangled_name, size_t buffersize)
{
	strncpy(unmangled_name, mangled_name, buffersize);
}

void GC_trace_object(void* object);
void GC_trace_object(void* object) {}

#endif
