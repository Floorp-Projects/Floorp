/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Variant unions used internally be XPConnect - only include locally. */

#ifndef xpcvariant_h___
#define xpcvariant_h___

// this is used only for WrappedJS stub param repackaging
struct nsXPCMiniVariant
{
// No ctors or dtors so that we can use arrays of these on the stack
// with no penalty.
    union
    {
        int8    i8;
        int16   i16;
        int32   i32;
        int64   i64;
        uint8   u8;
        uint16  u16;
        uint32  u32;
        uint64  u64;
        float   f;
        double  d;
        PRBool  b;
        char    c;
        wchar_t wc;
        void*   p;
    } val;
};

struct nsXPCVariant : public nsXPCMiniVariant
{
// No ctors or dtors so that we can use arrays of these on the stack
// with no penalty.

    // inherits 'val' here
    void*     ptr;
    nsXPTType type;
    uint8     flags;

    enum
    {
        // these are bitflags!
        PTR_IS_DATA  = 0x1, // ptr points to 'real' data in val
        VAL_IS_OWNED = 0x2, // val.p holds alloced ptr that must be freed
        VAL_IS_IFACE = 0x4  // val.p holds interface ptr that must be released
    };

    JSBool IsPtrData()      const {return (JSBool) (flags & PTR_IS_DATA);}
    JSBool IsValOwned()     const {return (JSBool) (flags & VAL_IS_OWNED);}
    JSBool IsValInterface() const {return (JSBool) (flags & VAL_IS_IFACE);}
};

#endif /* xpcvariant_h___ */
