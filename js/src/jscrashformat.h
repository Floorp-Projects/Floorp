/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
