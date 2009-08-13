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

    static uint32_t calcSaneCacheSize(uint32_t in)
    {
        if (in < uint32_t(NJ_LOG2_PAGE_SIZE)) return NJ_LOG2_PAGE_SIZE;    // at least 1 page
        if (in > 32) return 32;    // 4GB should be enough for anyone
        return in;
    }

    /**
     * This is the main control center for creating and managing fragments.
     */
    Fragmento::Fragmento(AvmCore* core, LogControl* logc, uint32_t cacheSizeLog2, CodeAlloc* codeAlloc)
        :
#ifdef NJ_VERBOSE
          enterCounts(NULL),
          mergeCounts(NULL),
          labels(NULL),
#endif
          _core(core),
          _codeAlloc(codeAlloc),
          _frags(core->GetGC()),
          _max_pages(1 << (calcSaneCacheSize(cacheSizeLog2) - NJ_LOG2_PAGE_SIZE)),
          _pagesGrowth(1)
    {
#ifdef _DEBUG
        {
            // XXX These belong somewhere else, but I can't find the
            //     right location right now.
            NanoStaticAssert((LIR_lt ^ 3) == LIR_ge);
            NanoStaticAssert((LIR_le ^ 3) == LIR_gt);
            NanoStaticAssert((LIR_ult ^ 3) == LIR_uge);
            NanoStaticAssert((LIR_ule ^ 3) == LIR_ugt);
            NanoStaticAssert((LIR_flt ^ 3) == LIR_fge);
            NanoStaticAssert((LIR_fle ^ 3) == LIR_fgt);

            /* Opcodes must be strictly increasing without holes. */
            uint32_t count = 0;
#define OPDEF(op, number, operands, repkind) \
        NanoAssertMsg(LIR_##op == count++, "misnumbered opcode");
#define OPDEF64(op, number, operands, repkind) \
        OPDEF(op, number, operands, repkind)
#include "LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64
        }
#endif

#ifdef MEMORY_INFO
        _allocList.set_meminfo_name("Fragmento._allocList");
#endif
        NanoAssert(_max_pages > _pagesGrowth); // shrink growth if needed
        verbose_only( enterCounts = NJ_NEW(core->gc, BlockHist)(core->gc); )
        verbose_only( mergeCounts = NJ_NEW(core->gc, BlockHist)(core->gc); )

        memset(&_stats, 0, sizeof(_stats));
    }

    Fragmento::~Fragmento()
    {
        clearFrags();
#if defined(NJ_VERBOSE)
        NJ_DELETE(enterCounts);
        NJ_DELETE(mergeCounts);
#endif
    }


    // Clear the fragment. This *does not* remove the fragment from the
    // map--the caller must take care of this.
    void Fragmento::clearFragment(Fragment* f)
    {
        Fragment *peer = f->peer;
        while (peer) {
            Fragment *next = peer->peer;
            peer->releaseTreeMem(_codeAlloc);
            NJ_DELETE(peer);
            peer = next;
        }
        f->releaseTreeMem(_codeAlloc);
        NJ_DELETE(f);
    }

    void Fragmento::clearFrags()
    {
        while (!_frags.isEmpty()) {
            clearFragment(_frags.removeLast());
        }

        verbose_only( enterCounts->clear();)
        verbose_only( mergeCounts->clear();)
        verbose_only( _stats.flushes++ );
        verbose_only( _stats.compiles = 0 );
        //nj_dprintf("Fragmento.clearFrags %d free pages of %d\n", _stats.freePages, _stats.pages);
    }

    AvmCore* Fragmento::core()
    {
        return _core;
    }

    Fragment* Fragmento::getAnchor(const void* ip)
    {
        Fragment *f = newFrag(ip);
        Fragment *p = _frags.get(ip);
        if (p) {
            f->first = p;
            /* append at the end of the peer list */
            Fragment* next;
            while ((next = p->peer) != NULL)
                p = next;
            p->peer = f;
        } else {
            f->first = f;
            _frags.put(ip, f); /* this is the first fragment */
        }
        f->anchor = f;
        f->root = f;
        f->kind = LoopTrace;
        verbose_only( addLabel(f, "T", _frags.size()); )
        return f;
    }

    Fragment* Fragmento::getLoop(const void* ip)
    {
        return _frags.get(ip);
    }

#ifdef NJ_VERBOSE
    void Fragmento::addLabel(Fragment *f, const char *prefix, int id)
    {
        char fragname[20];
        sprintf(fragname,"%s%d", prefix, id);
        labels->add(f, sizeof(Fragment), 0, fragname);
    }
#endif

    Fragment *Fragmento::createBranch(SideExit* exit, const void* ip)
    {
        Fragment *f = newBranch(exit->from, ip);
        f->kind = BranchTrace;
        f->treeBranches = f->root->treeBranches;
        f->root->treeBranches = f;
        return f;
    }

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
          blacklistLevel(0),
          fragEntry(NULL),
          loopEntry(NULL),
          vmprivate(NULL),
          codeList(0),
          _code(NULL),
          _hits(0)
    {
    }

    Fragment::~Fragment()
    {
        onDestroy();
    }

    void Fragment::blacklist()
    {
        blacklistLevel++;
        _hits = -(1<<blacklistLevel);
    }

    Fragment *Fragmento::newFrag(const void* ip)
    {
        GC *gc = _core->gc;
        Fragment *f = NJ_NEW(gc, Fragment)(ip);
        f->blacklistLevel = 5;
        return f;
    }

    Fragment *Fragmento::newBranch(Fragment *from, const void* ip)
    {
        Fragment *f = newFrag(ip);
        f->anchor = from->anchor;
        f->root = from->root;
        f->xjumpCount = from->xjumpCount;
        /*// prepend
        f->nextbranch = from->branches;
        from->branches = f;*/
        // append
        if (!from->branches) {
            from->branches = f;
        } else {
            Fragment *p = from->branches;
            while (p->nextbranch != 0)
                p = p->nextbranch;
            p->nextbranch = f;
        }
        return f;
    }

    void Fragment::releaseLirBuffer()
    {
        lastIns = 0;
    }

    void Fragment::releaseCode(CodeAlloc *codeAlloc)
    {
        _code = 0;
        codeAlloc->freeAll(codeList);
    }

    void Fragment::releaseTreeMem(CodeAlloc *codeAlloc)
    {
        releaseLirBuffer();
        releaseCode(codeAlloc);

        // now do it for all branches
        Fragment* branch = branches;
        while(branch)
        {
            Fragment* next = branch->nextbranch;
            branch->releaseTreeMem(codeAlloc);  // @todo safer here to recurse in case we support nested trees
            NJ_DELETE(branch);
            branch = next;
        }
    }
    #endif /* FEATURE_NANOJIT */
}


