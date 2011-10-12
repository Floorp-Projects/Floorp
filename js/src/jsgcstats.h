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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 30, 2010
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
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

#ifndef jsgcstats_h___
#define jsgcstats_h___

#if !defined JS_DUMP_CONSERVATIVE_GC_ROOTS && defined DEBUG
# define JS_DUMP_CONSERVATIVE_GC_ROOTS 1
#endif

namespace js {
namespace gc {
/*
 * The conservative GC test for a word shows that it is either a valid GC
 * thing or is not for one of the following reasons.
 */
enum ConservativeGCTest
{
    CGCT_VALID,
    CGCT_LOWBITSET, /* excluded because one of the low bits was set */
    CGCT_NOTARENA,  /* not within arena range in a chunk */
    CGCT_OTHERCOMPARTMENT,  /* in another compartment */
    CGCT_NOTCHUNK,  /* not within a valid chunk */
    CGCT_FREEARENA, /* within arena containing only free things */
    CGCT_NOTLIVE,   /* gcthing is not allocated */
    CGCT_END
};

struct ConservativeGCStats
{
    uint32  counter[gc::CGCT_END];  /* ConservativeGCTest classification
                                       counters */
    uint32  unaligned;              /* number of valid but not aligned on
                                       thing start pointers */ 

    void add(const ConservativeGCStats &another) {
        for (size_t i = 0; i != JS_ARRAY_LENGTH(counter); ++i)
            counter[i] += another.counter[i];
    }

    void dump(FILE *fp);
};

} //gc

} //js

#endif /* jsgcstats_h__ */
