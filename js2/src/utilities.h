/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the JavaScript 2 Prototype.
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef utilities_h___
#define utilities_h___

#include "systemtypes.h"

namespace JavaScript
{

#ifndef _WIN32
 #define STATIC_CONST(type, expr) static const type expr
#else
 // Microsoft Visual C++ 6.0 bug: constants not supported
 #define STATIC_CONST(type, expr) enum {expr}
#endif


//
// Assertions
//

#ifdef DEBUG
    void Assert(const char *s, const char *file, int line);

 #define ASSERT(_expr) ((_expr) ? (void)0 : JavaScript::Assert(#_expr, __FILE__, __LINE__))
 #define NOT_REACHED(_reasonStr) JavaScript::Assert(_reasonStr, __FILE__, __LINE__)
 #define DEBUG_ONLY(_stmt) _stmt
#else
 #define ASSERT(expr) (void)0
 #define NOT_REACHED(reasonStr) (void)0
 #define DEBUG_ONLY(_stmt)
#endif


    // A checked_cast acts as a static_cast that is checked in DEBUG mode.
    // It can only be used to downcast a class hierarchy that has at least one virtual function.
#ifdef DEBUG
    template <class Target, class Source> inline Target checked_cast(Source *s)
    {
        Target t = dynamic_cast<Target>(s);
        ASSERT(t);
        return t;
    }
#else
 #define checked_cast static_cast
#endif


//
// Mathematics
//

    template<class N> inline N v_min(N v1, N v2) {return v1 <= v2 ? v1 : v2;}
    template<class N> inline N v_max(N v1, N v2) {return v1 >= v2 ? v1 : v2;}

    uint ceilingLog2(uint32 n);
    uint floorLog2(uint32 n);


//
// Flag Bitmaps
//

    template<class F> inline F setFlag(F flags, F flag) {return static_cast<F>(flags | flag);}
    template<class F> inline F clearFlag(F flags, F flag) {return static_cast<F>(flags & ~flag);}
    template<class F> inline bool testFlag(F flags, F flag) {return (flags & flag) != 0;}


//
// Signed/Unsigned Conversions
//

    // Use these instead of type casts to safely convert between signed and unsigned
    // integers of the same size.

    inline uint32 toUInt32(int32 x) {return static_cast<uint32>(x);}
    inline int32 toInt32(uint32 x) {return static_cast<int32>(x);}
    inline size_t toSize_t(ptrdiff_t x) {return static_cast<size_t>(x);}
    inline ptrdiff_t toPtrdiff_t(size_t x) {return static_cast<ptrdiff_t>(x);}

}
#endif /* utilities_h___ */
