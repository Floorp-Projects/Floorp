/* -*- Mode: C++;    tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

// TODO:  move these to gc.h.
void GC_mark_object(GC_PTR object, GC_word mark);
void GC_trace_object(GC_PTR object, int verbose);

void GC_mark_object(GC_PTR object, GC_word mark) {}
void GC_trace_object(GC_PTR object, int verbose) {}

#endif
