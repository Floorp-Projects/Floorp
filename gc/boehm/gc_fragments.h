/*
	gc_fragments.h
 */

#pragma once

#ifndef __FILES__
#include <Files.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void GC_register_fragment(char* dataStart, char* dataEnd,
                          char* codeStart, char* codeEnd,
                          const FSSpec* fragmentSpec);

void GC_unregister_fragment(char* dataStart, char* dataEnd,
                            char* codeStart, char* codeEnd);

int GC_address_to_source(char* codeAddr, char symbolName[256], char fileName[256], UInt32* fileOffset);

#ifdef __cplusplus
} /* extern "C" */
#endif
