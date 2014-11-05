/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonAnalysis.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/RangeAnalysis.h"

#include "jsapi-tests/testJitMinimalFunc.h"
#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::jit;

BEGIN_TEST(testJitRangeAnalysis_MathSign)
{
    MinimalFunc func;
    MBasicBlock *block = func.createEntryBlock();

    MParameter *p = func.createParameter();
    block->add(p);

    MConstant *nan = MConstant::New(func.alloc, DoubleValue(JS::GenericNaN()));
    block->add(nan);
    MConstant *neginf = MConstant::New(func.alloc, DoubleValue(mozilla::NegativeInfinity<double>()));
    block->add(neginf);
    MConstant *negthreehalves = MConstant::New(func.alloc, DoubleValue(-1.5));
    block->add(negthreehalves);
    MConstant *negone = MConstant::New(func.alloc, DoubleValue(-1.0));
    block->add(negone);
    MConstant *neghalf = MConstant::New(func.alloc, DoubleValue(-0.5));
    block->add(neghalf);
    MConstant *negzero = MConstant::New(func.alloc, DoubleValue(-0.0));
    block->add(negzero);
    MConstant *zero = MConstant::New(func.alloc, DoubleValue(0.0));
    block->add(zero);
    MConstant *half = MConstant::New(func.alloc, DoubleValue(0.5));
    block->add(half);
    MConstant *one = MConstant::New(func.alloc, DoubleValue(1.0));
    block->add(one);
    MConstant *threehalves = MConstant::New(func.alloc, DoubleValue(1.5));
    block->add(threehalves);
    MConstant *inf = MConstant::New(func.alloc, DoubleValue(mozilla::PositiveInfinity<double>()));
    block->add(inf);

    CHECK(mozilla::IsNaN(nan->value().toNumber()));
    CHECK(mozilla::IsInfinite(neginf->value().toNumber()));
    CHECK(mozilla::IsNegative(neginf->value().toNumber()));
    CHECK(mozilla::IsFinite(negthreehalves->value().toNumber()));
    CHECK(mozilla::IsNegative(negthreehalves->value().toNumber()));
    CHECK(mozilla::IsFinite(negone->value().toNumber()));
    CHECK(mozilla::IsNegative(negone->value().toNumber()));
    CHECK(mozilla::IsFinite(neghalf->value().toNumber()));
    CHECK(mozilla::IsNegative(neghalf->value().toNumber()));
    CHECK(mozilla::IsFinite(negzero->value().toNumber()));
    CHECK(mozilla::IsNegative(negzero->value().toNumber()));
    CHECK(mozilla::IsNegativeZero(negzero->value().toNumber()));
    CHECK(mozilla::IsFinite(negzero->value().toNumber()));
    CHECK(!mozilla::IsNegative(zero->value().toNumber()));
    CHECK(!mozilla::IsNegativeZero(zero->value().toNumber()));
    CHECK(mozilla::IsFinite(half->value().toNumber()));
    CHECK(!mozilla::IsNegative(half->value().toNumber()));
    CHECK(mozilla::IsFinite(one->value().toNumber()));
    CHECK(!mozilla::IsNegative(one->value().toNumber()));
    CHECK(mozilla::IsFinite(threehalves->value().toNumber()));
    CHECK(!mozilla::IsNegative(threehalves->value().toNumber()));
    CHECK(mozilla::IsInfinite(inf->value().toNumber()));
    CHECK(!mozilla::IsNegative(inf->value().toNumber()));

    MathCache cache;

    MMathFunction *nanSign = MMathFunction::New(func.alloc, nan, MMathFunction::Sign, &cache);
    block->add(nanSign);
    MMathFunction *neginfSign = MMathFunction::New(func.alloc, neginf, MMathFunction::Sign, &cache);
    block->add(neginfSign);
    MMathFunction *negthreehalvesSign = MMathFunction::New(func.alloc, negthreehalves,
                                                           MMathFunction::Sign, &cache);
    block->add(negthreehalvesSign);
    MMathFunction *negoneSign = MMathFunction::New(func.alloc, negone, MMathFunction::Sign, &cache);
    block->add(negoneSign);
    MMathFunction *neghalfSign = MMathFunction::New(func.alloc, neghalf, MMathFunction::Sign, &cache);
    block->add(neghalfSign);
    MMathFunction *negzeroSign = MMathFunction::New(func.alloc, negzero, MMathFunction::Sign, &cache);
    block->add(negzeroSign);
    MMathFunction *zeroSign = MMathFunction::New(func.alloc, zero, MMathFunction::Sign, &cache);
    block->add(zeroSign);
    MMathFunction *halfSign = MMathFunction::New(func.alloc, half, MMathFunction::Sign, &cache);
    block->add(halfSign);
    MMathFunction *oneSign = MMathFunction::New(func.alloc, one, MMathFunction::Sign, &cache);
    block->add(oneSign);
    MMathFunction *threehalvesSign = MMathFunction::New(func.alloc, threehalves,
                                                        MMathFunction::Sign, &cache);
    block->add(threehalvesSign);
    MMathFunction *infSign = MMathFunction::New(func.alloc, inf, MMathFunction::Sign, &cache);
    block->add(infSign);

    MReturn *ret = MReturn::New(func.alloc, p);
    block->end(ret);

    if (!func.runRangeAnalysis())
        return false;

    CHECK(!nanSign->range());
    CHECK(neginfSign->range()->isInt32());
    CHECK(neginfSign->range()->lower() == -1);
    CHECK(neginfSign->range()->upper() == -1);
    CHECK(negthreehalvesSign->range()->isInt32());
    CHECK(negthreehalvesSign->range()->lower() == -1);
    CHECK(negthreehalvesSign->range()->upper() == -1);
    CHECK(negoneSign->range()->isInt32());
    CHECK(negoneSign->range()->lower() == -1);
    CHECK(negoneSign->range()->upper() == -1);
    CHECK(neghalfSign->range()->isInt32());
    CHECK(neghalfSign->range()->lower() == -1);

    // This should ideally be -1, but range analysis can't represent the
    // specific fractional range of the constant.
    CHECK(neghalfSign->range()->upper() == 0);

    CHECK(!negzeroSign->range()->canHaveFractionalPart());
    CHECK(negzeroSign->range()->canBeNegativeZero());
    CHECK(negzeroSign->range()->lower() == 0);
    CHECK(negzeroSign->range()->upper() == 0);
    CHECK(!zeroSign->range()->canHaveFractionalPart());
    CHECK(!zeroSign->range()->canBeNegativeZero());
    CHECK(zeroSign->range()->lower() == 0);
    CHECK(zeroSign->range()->upper() == 0);
    CHECK(halfSign->range()->isInt32());

    // This should ideally be 1, but range analysis can't represent the
    // specific fractional range of the constant.
    CHECK(halfSign->range()->lower() == 0);

    CHECK(halfSign->range()->upper() == 1);
    CHECK(oneSign->range()->isInt32());
    CHECK(oneSign->range()->lower() == 1);
    CHECK(oneSign->range()->upper() == 1);
    CHECK(threehalvesSign->range()->isInt32());
    CHECK(threehalvesSign->range()->lower() == 1);
    CHECK(threehalvesSign->range()->upper() == 1);
    CHECK(infSign->range()->isInt32());
    CHECK(infSign->range()->lower() == 1);
    CHECK(infSign->range()->upper() == 1);

    return true;
}
END_TEST(testJitRangeAnalysis_MathSign)
