/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozHunspellFileMgrGlue_h
#define mozHunspellFileMgrGlue_h

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint32_t(hunspell_create_filemgr_t)(const char* aFilename,
                                            const char* aKey);
typedef bool(hunspell_get_line_t)(uint32_t aFd, char** aLinePtr);
typedef int(hunspell_get_line_num_t)(uint32_t aFd);
typedef void(hunspell_destruct_filemgr_t)(uint32_t aFd);

void RegisterHunspellCallbacks(
    hunspell_create_filemgr_t* aHunspellCreateFilemgr,
    hunspell_get_line_t* aHunspellGetLine,
    hunspell_get_line_num_t* aHunspellGetLine_num,
    hunspell_destruct_filemgr_t* aHunspellDestructFilemgr);

extern hunspell_create_filemgr_t* moz_glue_hunspell_create_filemgr;
extern hunspell_get_line_t* moz_glue_hunspell_get_line;
extern hunspell_get_line_num_t* moz_glue_hunspell_get_line_num;
extern hunspell_destruct_filemgr_t* moz_glue_hunspell_destruct_filemgr;

#if defined(__cplusplus)
}
#endif

#endif
