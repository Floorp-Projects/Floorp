/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "Fundamentals.h"

////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

class MallocMemoryOps {
public:
    static void* alloc(Uint32 size) {
        return ::calloc(1, size);
    }

    static void free(void* p) {
        ::free(p);
    }
};

////////////////////////////////////////////////////////////////////////////////
#if 0
#include "Pool.h"

class PoolMemoryOps {
public:
    PoolMemoryOps(Pool &pool) : pool(pool) {}

    static void* alloc(Uint32 size) {
        return pool.allocate(size);
    }

    static void free(void* p) {
        // no-op for pools
    }

protected:
    Pool &pool;
};

////////////////////////////////////////////////////////////////////////////////

#include "plarena.h"

class ArenaMemoryOps {
public:
    ArenaMemoryOps(PLArena &arena) : arena(arena) {}

    static void* alloc(Uint32 size) {
        void* p;
        PL_ARENA_ALLOCATE(p, &arena, size);
        return p;
    }

    static void free(void* p) {
        // no-op for arenas
    }

protected:
    PLArena &arena;
};
#endif // 0
////////////////////////////////////////////////////////////////////////////////

#include "smobj.h"

class SMMallocMemoryOps {
public:
    static void* alloc(Uint32 size) {
        return SM_Malloc(size);
    }

    static void free(void* p) {
        SM_Free(p);
    }
};

////////////////////////////////////////////////////////////////////////////////

class GCObjectArrayMemoryOps {
public:
    static void* alloc(Uint32 size) {
        return SM_AllocArray(&objectArrayClass, size / sizeof(void*));
    }

    static void free(void* p) {
        // no-op for gc
    }

    static void init() {
        SM_InitObjectClass(&genericObjectClass, NULL, sizeof(SMObjectClass),
                           0, 0, 0);
        SM_InitArrayClass(&objectArrayClass, NULL, (SMClass*)&genericObjectClass);
    }

protected:
    static SMObjectClass genericObjectClass;
    static SMArrayClass objectArrayClass;
};

////////////////////////////////////////////////////////////////////////////////

class GCByteArrayMemoryOps {
public:
    static void* alloc(Uint32 size) {
        return SM_AllocArray(&byteArrayClass, size);
    }

    static void free(void* p) {
        // no-op for gc
    }

    static void init() {
        SM_InitPrimClass(&byteClass, NULL, SMClassKind_ByteClass);
        SM_InitArrayClass(&byteArrayClass, NULL, (SMClass*)&byteClass);
    }

protected:
    static SMPrimClass byteClass;
    static SMArrayClass byteArrayClass;
};

////////////////////////////////////////////////////////////////////////////////

#endif // _MEMORY_H_
