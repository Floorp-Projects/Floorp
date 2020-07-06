/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TrialInlining_h
#define jit_TrialInlining_h

#include "jit/CacheIR.h"
#include "vm/BytecodeLocation.h"

/*
 * [SMDOC] Trial Inlining
 *
 * WarpBuilder relies on transpiling CacheIR. When inlining scripted
 * functions in WarpBuilder, we want our ICs to be as monomorphic as
 * possible. Functions with multiple callers complicate this. An IC in
 * such a function might be monomorphic for any given caller, but
 * polymorphic overall. This make the input to WarpBuilder less precise.
 *
 * To solve this problem, we do trial inlining. During baseline
 * execution, we identify call sites for which it would be useful to
 * have more precise inlining data. For each such call site, we
 * allocate a fresh ICScript and replace the existing call IC with a
 * new specialized IC that invokes the callee using the new
 * ICScript. Other callers of the callee will continue using the
 * default ICScript. When we eventually Warp-compile the script, we
 * can generate code for the callee using the IC information in our
 * private ICScript, which is specialized for its caller.
 *
 * The same approach can be used to inline recursively.
 */

namespace js {
namespace jit {

bool DoTrialInlining(JSContext* cx, BaselineFrame* frame);

}  // namespace jit
}  // namespace js

#endif /* jit_TrialInlining_h */
