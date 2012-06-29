/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Pearl <rpearl@endofunctor.org>
 *   Michael Sullivan <sully@msully.net>
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

#ifndef jsion_range_analysis_h__
#define jsion_range_analysis_h__

#include "wtf/Platform.h"
#include "MIR.h"
#include "CompileInfo.h"

namespace js {
namespace ion {

class MBasicBlock;
class MIRGraph;

class RangeAnalysis
{
  protected:
	bool blockDominates(MBasicBlock *b, MBasicBlock *b2);
	void replaceDominatedUsesWith(MDefinition *orig, MDefinition *dom,
	                              MBasicBlock *block);

  protected:
    MIRGraph &graph_;

  public:
    RangeAnalysis(MIRGraph &graph);
    bool addBetaNobes();
    bool analyze();
    bool removeBetaNobes();
};

class Range {
    private:
        // :TODO: we should do symbolic range evaluation, where we have
        // information of the form v1 < v2 for arbitrary defs v1 and v2, not
        // just constants of type int32.
        // (Bug 766592)

        // We represent ranges where the endpoints can be in the set:
        // {-infty} U [INT_MIN, INT_MAX] U {infty}.  A bound of +/-
        // infty means that the value may have overflowed in that
        // direction. When computing the range of an integer
        // instruction, the ranges of the operands can be clamped to
        // [INT_MIN, INT_MAX], since if they had overflowed they would
        // no longer be integers. This is important for optimizations
        // and somewhat subtle.
        //
        // N.B.: All of the operations that compute new ranges based
        // on existing ranges will ignore the _infinite_ flags of the
        // input ranges; that is, they implicitly clamp the ranges of
        // the inputs to [INT_MIN, INT_MAX]. Therefore, while our range might
        // be infinite (and could overflow), when using this information to
        // propagate through other ranges, we disregard this fact; if that code
        // executes, then the overflow did not occur, so we may safely assume
        // that the range is [INT_MIN, INT_MAX] instead.
        //
        // To facilitate this trick, we maintain the invariants that:
        // 1) lower_infinite == true implies lower_ == JSVAL_INT_MIN
        // 2) upper_infinite == true implies upper_ == JSVAL_INT_MAX
        int32 lower_;
        bool lower_infinite_;
        int32 upper_;
        bool upper_infinite_;

    public:
        Range() :
            lower_(JSVAL_INT_MIN),
            lower_infinite_(true),
            upper_(JSVAL_INT_MAX),
            upper_infinite_(true)
        {}

        Range(int64_t l, int64_t h) {
            setLower(l);
            setUpper(h);
        }

        static int64_t abs64(int64_t x) {
#ifdef WTF_OS_WINDOWS
            return _abs64(x);
#else
            return llabs(x);
#endif
        }

        void printRange(FILE *fp);
        bool update(const Range *other);
        bool update(const Range &other) {
            return update(&other);
        }

        // Unlike the other operations, unionWith is an in-place
        // modification. This is to avoid a bunch of useless extra
        // copying when chaining together unions when handling Phi
        // nodes.
        void unionWith(const Range *other);

        static Range intersect(const Range *lhs, const Range *rhs);
        static Range add(const Range *lhs, const Range *rhs);
        static Range sub(const Range *lhs, const Range *rhs);
        static Range mul(const Range *lhs, const Range *rhs);

        static Range shl(const Range *lhs, int32 c);
        static Range shr(const Range *lhs, int32 c);

        inline void makeLowerInfinite() {
            lower_infinite_ = true;
            lower_ = JSVAL_INT_MIN;
        }
        inline void makeUpperInfinite() {
            upper_infinite_ = true;
            upper_ = JSVAL_INT_MAX;
        }
        inline void makeRangeInfinite() {
            makeLowerInfinite();
            makeUpperInfinite();
        }

        inline bool isLowerInfinite() const {
            return lower_infinite_;
        }
        inline bool isUpperInfinite() const {
            return upper_infinite_;
        }

        inline bool isFinite() const {
            return !isLowerInfinite() && !isUpperInfinite();
        }

        inline int32 lower() const {
            return lower_;
        }

        inline int32 upper() const {
            return upper_;
        }

        inline void setLower(int64_t x) {
            if (x > JSVAL_INT_MAX) { // c.c
                lower_ = JSVAL_INT_MAX;
            } else if (x < JSVAL_INT_MIN) {
                makeLowerInfinite();
            } else {
                lower_ = (int32)x;
                lower_infinite_ = false;
            }
        }
        inline void setUpper(int64_t x) {
            if (x > JSVAL_INT_MAX) {
                makeUpperInfinite();
            } else if (x < JSVAL_INT_MIN) { // c.c
                upper_ = JSVAL_INT_MIN;
            } else {
                upper_ = (int32)x;
                upper_infinite_ = false;
            }
        }
        void set(int64_t l, int64_t h) {
            setLower(l);
            setUpper(h);
        }
};

} // namespace ion
} // namespace js

#endif // jsion_range_analysis_h__

