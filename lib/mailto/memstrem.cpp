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
/*lame but i need it, mjudge*/
#include "xp_core.h"
#include "xp_mem.h"
#include "xp_mcom.h" /*for memcopy*/

#include "memstrem.h"



memstream::memstream()
{
    mem = NULL;
    sizealloc = 0;
    frozen = FALSE;
    memloc = 0;
    buffer = DEFAULT_BUFFER_AMT;
}

memstream::memstream(int32 size, int32 bufferdelta /*= DEF*/)
{
    mem = (char *)XP_ALLOC(size);
    sizealloc = size;
    frozen = FALSE;
    memloc = 0;
    buffer = bufferdelta;
}

int
memstream::write(const char *t_input,int bytesize)
{
    if (frozen)
        return 0; //frozen, cant write
    if ((bytesize + memloc) > sizealloc)
    {
        sizealloc = (bytesize + memloc) + buffer;
        mem = (char *)XP_REALLOC(mem, sizealloc);
        if (!mem)
            return 0;
    }
    XP_MEMCPY(mem + memloc,t_input , bytesize);
    memloc +=bytesize;
    return bytesize;
}



memstream::~memstream()
{
    if (!frozen)
        clear();
}



void
memstream::clear()
{
    XP_FREEIF(mem);
    sizealloc = 0;
    memloc = 0;
    frozen = FALSE;
}
