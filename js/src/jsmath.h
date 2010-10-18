/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
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

#ifndef jsmath_h___
#define jsmath_h___

namespace js {

typedef double (*UnaryFunType)(double);

class MathCache
{
    static const unsigned SizeLog2 = 12;
    static const unsigned Size = 1 << SizeLog2;
    struct Entry { double in; UnaryFunType f; double out; };
    Entry table[Size];

  public:
    MathCache();

    uintN hash(double x) {
        union { double d; struct { uint32 one, two; } s; } u = { x };
        uint32 hash32 = u.s.one ^ u.s.two;
        uint16 hash16 = (uint16)(hash32 ^ (hash32 >> 16));
        return (hash16 & (Size - 1)) ^ (hash16 >> (16 - SizeLog2));
    }

    /*
     * N.B. lookup uses double-equality. This is only safe if hash() maps +0
     * and -0 to different table entries, which is asserted in MathCache().
     */
    double lookup(UnaryFunType f, double x) {
        uintN index = hash(x);
        Entry &e = table[index];
        if (e.in == x && e.f == f)
            return e.out;
        e.in = x;
        e.f = f;
        return (e.out = f(x));
    }
};

} /* namespace js */

/*
 * JS math functions.
 */

extern js::Class js_MathClass;

extern JSObject *
js_InitMathClass(JSContext *cx, JSObject *obj);

extern void
js_InitRandom(JSContext *cx);

extern JSBool
js_math_ceil(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_math_floor(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_math_max(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_math_min(JSContext *cx, uintN argc, js::Value *vp);

extern JSBool
js_math_round(JSContext *cx, uintN argc, js::Value *vp);

extern jsdouble
js_math_ceil_impl(jsdouble x);

extern jsdouble
js_math_floor_impl(jsdouble x);

extern jsdouble
js_math_round_impl(jsdouble x);

#endif /* jsmath_h___ */
