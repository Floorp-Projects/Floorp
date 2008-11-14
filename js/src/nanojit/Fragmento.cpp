/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
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
		if (in < uint32_t(NJ_LOG2_PAGE_SIZE)) return NJ_LOG2_PAGE_SIZE;	// at least 1 page
		if (in > 30) return 30;	// 1GB should be enough for anyone
		return in;
	}

	/**
	 * This is the main control center for creating and managing fragments.
	 */
	Fragmento::Fragmento(AvmCore* core, uint32_t cacheSizeLog2) 
		: _frags(core->GetGC()),
           _freePages(core->GetGC(), 1024),
		  _allocList(core->GetGC()),
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

			/* Opcodes must be strictly increasing without holes. */
			uint32_t count = 0;
			#define OPDEF(op, number, operands) \
				NanoAssertMsg(LIR_##op == count++, "misnumbered opcode");
			#define OPDEF64(op, number, operands) OPDEF(op, number, operands)
			#include "LIRopcode.tbl"
			#undef OPDEF
			#undef OPDEF64
		}
#endif

#ifdef MEMORY_INFO
		_allocList.set_meminfo_name("Fragmento._allocList");
#endif
		NanoAssert(_max_pages > _pagesGrowth); // shrink growth if needed 
		_core = core;
		GC *gc = core->GetGC();
		_assm = NJ_NEW(gc, nanojit::Assembler)(this);
		verbose_only( enterCounts = NJ_NEW(gc, BlockHist)(gc); )
		verbose_only( mergeCounts = NJ_NEW(gc, BlockHist)(gc); )
	}

	Fragmento::~Fragmento()
	{
        AllocEntry *entry;

		clearFrags();
        _frags.clear();		
		_freePages.clear();
		while( _allocList.size() > 0 )
		{
			//fprintf(stderr,"dealloc %x\n", (intptr_t)_allocList.get(_allocList.size()-1));
#ifdef MEMORY_INFO
			ChangeSizeExplicit("NanoJitMem", -1, _gcHeap->Size(_allocList.last()));
#endif
            entry = _allocList.removeLast();
			_gcHeap->Free( entry->page, entry->allocSize );
            NJ_DELETE(entry);
		}
        NJ_DELETE(_assm);
#if defined(NJ_VERBOSE)
        NJ_DELETE(enterCounts);
        NJ_DELETE(mergeCounts);
#endif
	}

	void Fragmento::trackPages()
	{
		const uint32_t pageUse = _stats.pages - _freePages.size();
		if (_stats.maxPageUse < pageUse)
			_stats.maxPageUse = pageUse;
	}

	Page* Fragmento::pageAlloc()
	{
        NanoAssert(sizeof(Page) == NJ_PAGE_SIZE);
		if (!_freePages.size())
			pagesGrow(_pagesGrowth);	// try to get more mem
			            if ((_pagesGrowth << 1) < _max_pages)
							_pagesGrowth <<= 1;						

		trackPages();
		Page* page = 0;
		if (_freePages.size()) 
			page = _freePages.removeLast();
		return page;
		}
	
	void Fragmento::pagesRelease(PageList& l)
		{
		_freePages.add(l);
		l.clear();
		NanoAssert(_freePages.size() <= _stats.pages);
	}
	
	void Fragmento::pageFree(Page* page)
	{ 
		_freePages.add(page);
		NanoAssert(_freePages.size() <= _stats.pages);
	}

	void Fragmento::pagesGrow(int32_t count)
	{
		NanoAssert(!_freePages.size());
		MMGC_MEM_TYPE("NanojitFragmentoMem"); 
		Page* memory = 0;
        GC *gc = _core->GetGC();
		if (_stats.pages < _max_pages)
		{
            AllocEntry *entry;

            // make sure we don't grow beyond _max_pages
            if (_stats.pages + count > _max_pages)
                count = _max_pages - _stats.pages;
            if (count < 0)
                count = 0;
			// @todo nastiness that needs a fix'n
			_gcHeap = gc->GetGCHeap();
			NanoAssert(int32_t(NJ_PAGE_SIZE)<=_gcHeap->kNativePageSize);
			
			// convert _max_pages to gc page count 
			int32_t gcpages = (count*NJ_PAGE_SIZE) / _gcHeap->kNativePageSize;
			MMGC_MEM_TYPE("NanoJitMem"); 
			memory = (Page*)_gcHeap->Alloc(gcpages);
#ifdef MEMORY_INFO
			ChangeSizeExplicit("NanoJitMem", 1, _gcHeap->Size(memory));
#endif
			NanoAssert((int*)memory == pageTop(memory));
			//fprintf(stderr,"head alloc of %d at %x of %d pages using nj page size of %d\n", gcpages, (intptr_t)memory, (intptr_t)_gcHeap->kNativePageSize, NJ_PAGE_SIZE);

            entry = NJ_NEW(gc, AllocEntry);
            entry->page = memory;
            entry->allocSize = gcpages;
            _allocList.add(entry);

			_stats.pages += count;
			Page* page = memory;
			while(--count >= 0)
			{
				//fprintf(stderr,"Fragmento::pageGrow adding page %x ; %d\n", (unsigned)page, _freePages.size()+1);
				_freePages.add(page++);
			}
			trackPages();
		}
	}
	
	// Clear the fragment. This *does not* remove the fragment from the
	// map--the caller must take care of this.
	void Fragmento::clearFragment(Fragment* f)
	{
		Fragment *peer = f->peer;
		while (peer) {
			Fragment *next = peer->peer;
			peer->releaseTreeMem(this);
			NJ_DELETE(peer);
			peer = next;
		}
		f->releaseTreeMem(this);
		NJ_DELETE(f);
	}

	void Fragmento::clearFrag(const void* ip)
	{
		if (_frags.containsKey(ip)) {
			clearFragment(_frags.remove(ip));
		}
	}

	void Fragmento::clearFrags()
	{
		// reclaim any dangling native pages
		_assm->pageReset();

        while (!_frags.isEmpty()) {
            clearFragment(_frags.removeLast());
		}

		verbose_only( enterCounts->clear();)
		verbose_only( mergeCounts->clear();)
		verbose_only( _stats.flushes++ );
		verbose_only( _stats.compiles = 0 );
		//fprintf(stderr, "Fragmento.clearFrags %d free pages of %d\n", _stats.freePages, _stats.pages);
	}

	Assembler* Fragmento::assm()
	{
		return _assm;
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

	Fragment *Fragmento::getMerge(GuardRecord *lr, const void* ip)
    {
		Fragment *anchor = lr->exit->from->anchor;
		for (Fragment *f = anchor->branches; f != 0; f = f->nextbranch) {
			if (f->kind == MergeTrace && f->ip == ip /*&& f->calldepth == lr->calldepth*/) {
				// found existing shared branch on anchor
				return f;
			}
		}

		Fragment *f = newBranch(anchor, ip);
		f->root = f;
		f->kind = MergeTrace;
		verbose_only(
			int mergeid = 1;
			for (Fragment *g = anchor->branches; g != 0; g = g->nextbranch)
				if (g->kind == MergeTrace)
					mergeid++;
			addLabel(f, "M", mergeid); 
		)
        return f;
    }

	Fragment *Fragmento::createBranch(SideExit* exit, const void* ip)
    {
        Fragment *f = newBranch(exit->from, ip);
		f->kind = BranchTrace;
		f->treeBranches = f->root->treeBranches;
		f->root->treeBranches = f;
        return f;
    }

#ifdef NJ_VERBOSE
	struct fragstats {
		int size;
		uint64_t traceDur;
		uint64_t interpDur;
		int lir, lirbytes;
	};

	void Fragmento::dumpFragStats(Fragment *f, int level, fragstats &stat)
    {
        char buf[50];
        sprintf(buf, "%*c%s", 1+level, ' ', labels->format(f));

        int called = f->hits();
        if (called >= 0)
            called += f->_called;
        else
            called = -(1<<f->blacklistLevel) - called - 1;

        uint32_t main = f->_native - f->_exitNative;

        char cause[200];
        if (f->_token && strcmp(f->_token,"loop")==0)
            sprintf(cause,"%s %d", f->_token, f->xjumpCount);
		else if (f->_token) {
			if (f->eot_target) {
				sprintf(cause,"%s %s", f->_token, labels->format(f->eot_target));
			} else {
	            strcpy(cause, f->_token);
			}
		}
        else
            cause[0] = 0;
        
        _assm->outputf("%-10s %7d %6d %6d %6d %4d %9llu %9llu %-12s %s", buf,
            called, f->guardCount, main, f->_native, f->compileNbr, f->traceTicks/1000, f->interpTicks/1000,
			cause, labels->format(f->ip));
        
        stat.size += main;
		stat.traceDur += f->traceTicks;
		stat.interpDur += f->interpTicks;
		stat.lir += f->_lir;
		stat.lirbytes += f->_lirbytes;

		for (Fragment *x = f->branches; x != 0; x = x->nextbranch)
			if (x->kind != MergeTrace)
	            dumpFragStats(x,level+1,stat);
        for (Fragment *x = f->branches; x != 0; x = x->nextbranch)
			if (x->kind == MergeTrace)
	            dumpFragStats(x,level+1,stat);

        if (f->isAnchor() && f->branches != 0) {
            _assm->output("");
        }
    }

    class DurData { public:
        DurData(): frag(0), traceDur(0), interpDur(0), size(0) {}
        DurData(int): frag(0), traceDur(0), interpDur(0), size(0) {}
        DurData(Fragment* f, uint64_t td, uint64_t id, int32_t s)
			: frag(f), traceDur(td), interpDur(id), size(s) {}
        Fragment* frag;
        uint64_t traceDur;
        uint64_t interpDur;
		int32_t size;
    };

	void Fragmento::dumpRatio(const char *label, BlockHist *hist)
	{
		int total=0, unique=0;
		for (int i = 0, n=hist->size(); i < n; i++) {
			const void * id = hist->keyAt(i);
			int c = hist->get(id);
			if (c > 1) {
				//_assm->outputf("%d %X", c, id);
				unique += 1;
			}
			else if (c == 1) {
				unique += 1;
			}
			total += c;
		}
		_assm->outputf("%s total %d unique %d ratio %.1f%", label, total, unique, double(total)/unique);
	}

	void Fragmento::dumpStats()
	{
		bool vsave = _assm->_verbose;
		_assm->_verbose = true;

		_assm->output("");
		dumpRatio("inline", enterCounts);
		dumpRatio("merges", mergeCounts);
		_assm->outputf("abc %d il %d (%.1fx) abc+il %d (%.1fx)",
			_stats.abcsize, _stats.ilsize, (double)_stats.ilsize/_stats.abcsize,
			_stats.abcsize + _stats.ilsize,
			double(_stats.abcsize+_stats.ilsize)/_stats.abcsize);

		int32_t count = _frags.size();
		int32_t pages =  _stats.pages;
		int32_t maxPageUse =  _stats.maxPageUse;
		int32_t free = _freePages.size();
		int32_t flushes = _stats.flushes;
		if (!count)
		{
			_assm->outputf("No fragments in cache, %d flushes", flushes);
    		_assm->_verbose = vsave;
            return;
		}

        _assm->outputf("\nFragment statistics");
		_assm->outputf("  loop trees:     %d", count);
		_assm->outputf("  flushes:        %d", flushes);
		_assm->outputf("  compiles:       %d / %d", _stats.compiles, _stats.totalCompiles);
		_assm->outputf("  used:           %dk / %dk", (pages-free)<<(NJ_LOG2_PAGE_SIZE-10), pages<<(NJ_LOG2_PAGE_SIZE-10));
		_assm->outputf("  maxPageUse:     %dk", (maxPageUse)<<(NJ_LOG2_PAGE_SIZE-10));
		_assm->output("\ntrace         calls guards   main native  gen   T-trace  T-interp");

		avmplus::SortedMap<uint64_t, DurData, avmplus::LIST_NonGCObjects> durs(_core->gc);
		uint64_t totaldur=0;
		fragstats totalstat = { 0,0,0,0,0 };
        for (int32_t i=0; i<count; i++)
        {
            Fragment *f = _frags.at(i);
            while (true) {
                fragstats stat = { 0,0,0,0,0 };
                dumpFragStats(f, 0, stat);
                if (stat.lir) {
                    totalstat.lir += stat.lir;
                    totalstat.lirbytes += stat.lirbytes;
                }
                uint64_t bothDur = stat.traceDur + stat.interpDur;
                if (bothDur) {
                    totalstat.interpDur += stat.interpDur;
                    totalstat.traceDur += stat.traceDur;
                    totalstat.size += stat.size;
                    totaldur += bothDur;
                    while (durs.containsKey(bothDur)) bothDur++;
                    DurData d(f, stat.traceDur, stat.interpDur, stat.size);
                    durs.put(bothDur, d);
                }
                if (!f->peer)
                    break;
                f = f->peer;
            }
        }
		uint64_t totaltrace = totalstat.traceDur;
		int totalsize = totalstat.size;

		_assm->outputf("");
		_assm->outputf("lirbytes %d / lir %d = %.1f bytes/lir", totalstat.lirbytes,
            totalstat.lir, double(totalstat.lirbytes)/totalstat.lir);
		_assm->outputf("       trace         interp");
		_assm->outputf("%9lld (%2d%%)  %9lld (%2d%%)",
			totaltrace/1000, int(100.0*totaltrace/totaldur),
			(totaldur-totaltrace)/1000, int(100.0*(totaldur-totaltrace)/totaldur));
		_assm->outputf("");
		_assm->outputf("trace      ticks            trace           interp           size");
		for (int32_t i=durs.size()-1; i >= 0; i--) {
			uint64_t bothDur = durs.keyAt(i);
			DurData d = durs.get(bothDur);
			int size = d.size;
			_assm->outputf("%-4s %9lld (%2d%%)  %9lld (%2d%%)  %9lld (%2d%%)  %6d (%2d%%)  %s", 
				labels->format(d.frag),
				bothDur/1000, int(100.0*bothDur/totaldur),
				d.traceDur/1000, int(100.0*d.traceDur/totaldur),
				d.interpDur/1000, int(100.0*d.interpDur/totaldur),
				size, int(100.0*size/totalsize),
				labels->format(d.frag->ip));
		}

		_assm->_verbose = vsave;

	}

	void Fragmento::countBlock(BlockHist *hist, const void* ip)
	{
		int c = hist->count(ip);
		if (_assm->_verbose)
			_assm->outputf("++ %s %d", labels->format(ip), c);
	}

	void Fragmento::countIL(uint32_t il, uint32_t abc)
	{
		_stats.ilsize += il;
		_stats.abcsize += abc;
	}
	
#ifdef AVMPLUS_VERBOSE
	void Fragmento::drawTrees(char *fileName) {
		drawTraceTrees(this, this->_frags, this->_core, fileName);
	}
#endif
#endif // NJ_VERBOSE

	//
	// Fragment
	//
	Fragment::Fragment(const void* _ip) : ip(_ip)
	{
        // Fragment is a gc object which is zero'd by the GC, no need to clear fields
    }

	Fragment::~Fragment()
	{
        onDestroy();
		NanoAssert(_pages == 0);
    }

    void Fragment::resetHits()
    {
        blacklistLevel >>= 1;
        _hits = 0;
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
        f->recordAttempts = 0;
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

	void Fragmento::disconnectLoops()
	{
		for (int i = 0; i < _frags.size(); ++i) {
			Fragment* frag = _frags.at(i);
			if (frag->lastIns->isop(LIR_loop))
				_assm->disconnectLoop(frag->lastIns->record());
		}
	}

	void Fragmento::reconnectLoops()
	{
		for (int i = 0; i < _frags.size(); ++i) {
			Fragment* frag = _frags.at(i);
			if (frag->lastIns->isop(LIR_loop))
				_assm->reconnectLoop(frag->lastIns->record());
		}
	}

	void Fragment::releaseLirBuffer()
	{
		lastIns = 0;	
	}

	void Fragment::releaseCode(Fragmento* frago)
	{
		_code = 0;
		while(_pages)
		{
			Page* next = _pages->next;
			frago->pageFree(_pages);
			_pages = next;
		}
	}
	
	void Fragment::releaseTreeMem(Fragmento* frago)
	{
		releaseLirBuffer();
		releaseCode(frago);
			
		// now do it for all branches 
		Fragment* branch = branches;
		while(branch)
		{
			Fragment* next = branch->nextbranch;
			branch->releaseTreeMem(frago);  // @todo safer here to recurse in case we support nested trees
            NJ_DELETE(branch);
			branch = next;
		}
	}
	#endif /* FEATURE_NANOJIT */
}


