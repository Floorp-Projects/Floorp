/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jscrashformat_h___
#define jscrashformat_h___

#include <string.h>

namespace js {
namespace crash {

const static int crash_cookie_len = 16;
const static char crash_cookie[crash_cookie_len] = "*J*S*CRASHDATA*";

/* These values are used for CrashHeader::id. */
enum {
    JS_CRASH_STACK_GC = 0x400,
    JS_CRASH_STACK_ERROR = 0x401,
    JS_CRASH_RING = 0x800
};

/*
 * All the data here will be stored directly in the minidump, so we use
 * platform-independent types. We also ensure that the size of every field is a
 * multiple of 8 bytes, to guarantee that they won't be padded.
 */

struct CrashHeader
{
    char cookie[crash_cookie_len];

    /* id of the crash data, chosen from the enum above. */
    uint64_t id;

    CrashHeader(uint64_t id) : id(id) { memcpy(cookie, crash_cookie, crash_cookie_len); }
};

struct CrashRegisters
{
    uint64_t ip, sp, bp;
};

const static int crash_buffer_size = 32 * 1024;

struct CrashStack
{
    CrashStack(uint64_t id) : header(id) {}

    CrashHeader header;
    uint64_t snaptime;    /* Unix time when the stack was snapshotted. */
    CrashRegisters regs;  /* Register contents for the snapshot. */
    uint64_t stack_base;  /* Base address of stack at the time of snapshot. */
    uint64_t stack_len;   /* Extent of the stack. */
    char stack[crash_buffer_size]; /* Contents of the stack. */
};

struct CrashRing
{
    CrashRing(uint64_t id) : header(id), offset(0) { memset(buffer, 0, sizeof(buffer)); }

    CrashHeader header;
    uint64_t offset; /* Next byte to be written in the buffer. */
    char buffer[crash_buffer_size];
};

/* These are the tag values for each entry in the CrashRing. */
enum {
    JS_CRASH_TAG_GC = 0x200
};

} /* namespace crash */
} /* namespace js */

#endif
