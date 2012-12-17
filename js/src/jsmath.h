/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    unsigned hash(double x) {
        union { double d; struct { uint32_t one, two; } s; } u = { x };
        uint32_t hash32 = u.s.one ^ u.s.two;
        uint16_t hash16 = uint16_t(hash32 ^ (hash32 >> 16));
        return (hash16 & (Size - 1)) ^ (hash16 >> (16 - SizeLog2));
    }

    /*
     * N.B. lookup uses double-equality. This is only safe if hash() maps +0
     * and -0 to different table entries, which is asserted in MathCache().
     */
    double lookup(UnaryFunType f, double x) {
        unsigned index = hash(x);
        Entry &e = table[index];
        if (e.in == x && e.f == f)
            return e.out;
        e.in = x;
        e.f = f;
        return (e.out = f(x));
    }

    size_t sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf);
};

} /* namespace js */

/*
 * JS math functions.
 */

extern JSObject *
js_InitMathClass(JSContext *cx, js::HandleObject obj);

extern void
js_InitRandom(JSContext *cx);

extern double
math_random_no_outparam(JSContext *cx);

extern JSBool
js_math_random(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_abs(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_ceil(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_floor(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_max(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_min(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_round(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_sqrt(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_math_pow(JSContext *cx, unsigned argc, js::Value *vp);

extern double
js_math_ceil_impl(double x);

extern double
js_math_floor_impl(double x);

namespace js {

extern JSBool
math_imul(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
math_log(JSContext *cx, unsigned argc, js::Value *vp);

extern double
math_log_impl(MathCache *cache, double x);

extern JSBool
math_sin(JSContext *cx, unsigned argc, js::Value *vp);

extern double
math_sin_impl(MathCache *cache, double x);

extern JSBool
math_cos(JSContext *cx, unsigned argc, js::Value *vp);

extern double
math_cos_impl(MathCache *cache, double x);

extern JSBool
math_tan(JSContext *cx, unsigned argc, js::Value *vp);

extern double
math_tan_impl(MathCache *cache, double x);

extern double
powi(double x, int y);

extern double
ecmaPow(double x, double y);

} /* namespace js */

#endif /* jsmath_h___ */
