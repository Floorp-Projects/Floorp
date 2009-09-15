/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *   Mozilla TraceMonkey Team
 *   Asko Tontti <atontti@cc.hut.fi>
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


#ifndef __nanojit_Fragmento__
#define __nanojit_Fragmento__

namespace nanojit
{
    struct GuardRecord;

    /**
     * Fragments are linear sequences of native code that have a single entry
     * point at the start of the fragment and may have one or more exit points
     *
     * It may turn out that that this arrangement causes too much traffic
     * between d and i-caches and that we need to carve up the structure differently.
     */
    class Fragment
    {
        public:
            Fragment(const void*
                     verbose_only(, uint32_t profFragID));

            NIns*           code()                          { return _code; }
            void            setCode(NIns* codee)            { _code = codee; }
            int32_t&        hits()                          { return _hits; }
            bool            isRoot() { return root == this; }

            Fragment*      root;
            LirBuffer*     lirbuf;
            LIns*          lastIns;

            const void* ip;
            uint32_t recordAttempts;
            NIns* fragEntry;
            void* vmprivate;

            // for fragment entry and exit profiling.  See detailed
            // how-to-use comment below.
            verbose_only( LIns*          loopLabel; ) // where's the loop top?
            verbose_only( uint32_t       profFragID; )
            verbose_only( uint32_t       profCount; )
            verbose_only( uint32_t       nStaticExits; )
            verbose_only( size_t         nCodeBytes; )
            verbose_only( size_t         nExitBytes; )
            verbose_only( uint32_t       guardNumberer; )
            verbose_only( GuardRecord*   guardsForFrag; )

        private:
            NIns*            _code;        // ptr to start of code
            int32_t          _hits;
    };
}

/*
 * How to use fragment profiling
 *
 * Fragprofiling adds code to count how many times each fragment is
 * entered, and how many times each guard (exit) is taken.  Using this
 * it's possible to easily find which fragments are hot, which ones
 * typically exit early, etc.  The fragprofiler also gathers some
 * simple static info: for each fragment, the number of code bytes,
 * number of exit-block bytes, and number of guards (exits).
 *
 * Fragments and guards are given unique IDs (FragID, GuardID) which
 * are shown in debug printouts, so as to facilitate navigating from
 * the accumulated statistics to the associated bits of code.
 * GuardIDs are issued automatically, but FragIDs you must supply when
 * calling Fragment::Fragment.  Supply values >= 1, and supply a
 * different value for each new fragment (doesn't matter what, they
 * just have to be unique and >= 1); else
 * js_FragProfiling_FragFinalizer will assert.
 *
 * How to use/embed:
 *
 * - use a debug build (one with NJ_VERBOSE).  Without it, none of
 *   this code is compiled in.
 *
 * - set LC_FragProfile in the lcbits of the LogControl* object handed
 *   to Nanojit
 *
 * When enabled, Fragment::profCount is incremented every time the
 * fragment is entered, and GuardRecord::profCount is incremented
 * every time that guard exits.  However, NJ has no way to know where
 * the fragment entry/loopback point is.  So you must set
 * Fragment::loopLabel before running the assembler, so as to indicate
 * where the fragment-entry counter increment should be placed.  If
 * the fragment does not naturally have a loop label then you will
 * need to artificially add one.
 *
 * It is the embedder's problem to fish out, collate and present the
 * accumulated stats at the end of the Fragment's lifetime.  A
 * Fragment contains stats indicating its entry count and static code
 * sizes.  It also has a ::guardsForFrag field, which is a linked list
 * of GuardRecords, and by traversing them you can get hold of the
 * exit counts.
 */

#endif // __nanojit_Fragmento__
