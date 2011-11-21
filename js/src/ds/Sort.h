/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#ifndef Sort_h__
#define Sort_h__

#include "jstypes.h"

namespace js {

namespace detail {

template<typename T>
JS_ALWAYS_INLINE void
CopyNonEmptyArray(T *dst, const T *src, size_t nelems)
{
    JS_ASSERT(nelems != 0);
    const T *end = src + nelems;
    do {
        *dst++ = *src++;
    } while (src != end);
}

/* Helper function for MergeSort. */
template<typename T, typename Comparator>
JS_ALWAYS_INLINE bool
MergeArrayRuns(T *dst, const T *src, size_t run1, size_t run2, Comparator c)
{
    JS_ASSERT(run1 >= 1);
    JS_ASSERT(run2 >= 1);

    /* Copy runs already in sorted order. */
    const T *b = src + run1;
    bool lessOrEqual;
    if (!c(b[-1],  b[0], &lessOrEqual))
        return false;

    if (!lessOrEqual) {
        /* Runs are not already sorted, merge them. */
        for (const T *a = src;;) {
            if (!c(*a, *b, &lessOrEqual))
                return false;
            if (lessOrEqual) {
                *dst++ = *a++;
                if (!--run1) {
                    src = b;
                    break;
                }
            } else {
                *dst++ = *b++;
                if (!--run2) {
                    src = a;
                    break;
                }
            }
        }
    }
    CopyNonEmptyArray(dst, src, run1 + run2);
    return true;
}

} /* namespace detail */

/*
 * Sort the array using the merge sort algorithm. The scratch should point to
 * a temporary storage that can hold nelems elements.
 *
 * The comparator must provide the () operator with the following signature:
 *
 *     bool operator()(const T& a, const T& a, bool *lessOrEqualp);
 *
 * It should return true on success and sets lessOrEqualp to the result of
 * a <= b operation. If it returns false, the sort terminates immediately with
 * the false result. In this case the content of the array and scratch is
 * arbitrary.
 */
template<typename T, typename Comparator>
bool
MergeSort(T *array, size_t nelems, T *scratch, Comparator c)
{
    const size_t INS_SORT_LIMIT = 3;

    if (nelems <= 1)
        return true;

    /*
     * Apply insertion sort to small chunks to reduce the number of merge
     * passes needed.
     */
    for (size_t lo = 0; lo < nelems; lo += INS_SORT_LIMIT) {
        size_t hi = lo + INS_SORT_LIMIT;
        if (hi >= nelems)
            hi = nelems;
        for (size_t i = lo + 1; i != hi; i++) {
            for (size_t j = i; ;) {
                bool lessOrEqual;
                if (!c(array[j - 1], array[j], &lessOrEqual))
                    return false;
                if (lessOrEqual)
                    break;
                T tmp = array[j - 1];
                array[j - 1] = array[j];
                array[j] = tmp;
                if (--j == lo)
                    break;
            }
        }
    }

    T *vec1 = array;
    T *vec2 = scratch;
    for (size_t run = INS_SORT_LIMIT; run < nelems; run *= 2) {
        for (size_t lo = 0; lo < nelems; lo += 2 * run) {
            size_t hi = lo + run;
            if (hi >= nelems) {
                detail::CopyNonEmptyArray(vec2 + lo, vec1 + lo, nelems - lo);
                break;
            }
            size_t run2 = (run <= nelems - hi) ? run : nelems - hi;
            if (!detail::MergeArrayRuns(vec2 + lo, vec1 + lo, run, run2, c))
                return false;
        }
        T *swap = vec1;
        vec1 = vec2;
        vec2 = swap;
    }
    if (vec1 == scratch)
        detail::CopyNonEmptyArray(array, scratch, nelems);
    return true;
}

} /* namespace js */

#endif
