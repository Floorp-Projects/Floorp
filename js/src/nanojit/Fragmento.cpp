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

#include "nanojit.h"
#undef MEMORY_INFO

namespace nanojit
{
    #ifdef FEATURE_NANOJIT

    using namespace avmplus;

    //
    // Fragment
    //
    Fragment::Fragment(const void* _ip)
        :
#ifdef NJ_VERBOSE
          _called(0),
          _native(0),
          _exitNative(0),
          _lir(0),
          _lirbytes(0),
          _token(NULL),
          traceTicks(0),
          interpTicks(0),
          eot_target(NULL),
          sid(0),
          compileNbr(0),
#endif
          treeBranches(NULL),
          branches(NULL),
          nextbranch(NULL),
          anchor(NULL),
          root(NULL),
          parent(NULL),
          first(NULL),
          peer(NULL),
          lirbuf(NULL),
          lastIns(NULL),
          spawnedFrom(NULL),
          kind(LoopTrace),
          ip(_ip),
          guardCount(0),
          xjumpCount(0),
          recordAttempts(0),
          fragEntry(NULL),
          loopEntry(NULL),
          vmprivate(NULL),
          _code(NULL),
          _hits(0)
    {
    }
    #endif /* FEATURE_NANOJIT */
}


