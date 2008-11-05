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

#ifdef FEATURE_NANOJIT

#ifdef AVMPLUS_PORTING_API
#include "portapi_nanojit.h"
#endif

#if defined(AVMPLUS_UNIX) && defined(AVMPLUS_ARM)
#include <asm/unistd.h>
extern "C" void __clear_cache(char *BEG, char *END);
#endif

namespace nanojit
{


	class DeadCodeFilter: public LirFilter
	{
		const CallInfo *functions;

	    bool ignoreInstruction(LInsp ins)
	    {
            LOpcode op = ins->opcode();
            if (ins->isStore() ||
                op == LIR_loop ||
                op == LIR_label ||
                op == LIR_live ||
                isRet(op)) {
                return false;
            }
	        return ins->resv() == 0;
	    }

	public:		
		DeadCodeFilter(LirFilter *in, const CallInfo *f) : LirFilter(in), functions(f) {}
		LInsp read() {
			for (;;) {
				LInsp i = in->read();
				if (!i || i->isGuard() || i->isBranch()
					|| i->isCall() && !i->isCse(functions)
					|| !ignoreInstruction(i))
					return i;
			}
		}
	};

#ifdef NJ_VERBOSE
	class VerboseBlockReader: public LirFilter
	{
		Assembler *assm;
		LirNameMap *names;
		avmplus::List<LInsp, avmplus::LIST_NonGCObjects> block;
        bool flushnext;
	public:
		VerboseBlockReader(LirFilter *in, Assembler *a, LirNameMap *n) 
			: LirFilter(in), assm(a), names(n), block(a->_gc), flushnext(false)
        {}

		void flush() {
            flushnext = false;
            if (!block.isEmpty()) {
			    for (int j=0,n=block.size(); j < n; j++) {
					LIns *i = block[j];
				    assm->outputf("    %s", names->formatIns(block[j]));
					if (i->isop(LIR_label)) {
						assm->outputf("        %p:", assm->_nIns);
						assm->output("");
					}
				}
			    block.clear();
            }
		}

        void flush_add(LInsp i) {
            flush();
            block.add(i);
        }

		LInsp read() {
			LInsp i = in->read();
			if (!i) {
				flush();
				return i;
			}
            if (i->isGuard()) {
				flush_add(i);
				if (i->oprnd1())
					block.add(i->oprnd1());
            }
            else if (isRet(i->opcode()) || i->isBranch()) {
                flush_add(i);
            }
			else {
                if (flushnext)
                    flush();
				flush_add(i);//block.add(i);
                if (i->isop(LIR_label))
                    flushnext = true;
			}
			return i;
		}
	};
#endif
	
	/**
	 * Need the following:
	 *
	 *	- merging paths ( build a graph? ), possibly use external rep to drive codegen
	 */
    Assembler::Assembler(Fragmento* frago)
        : hasLoop(0)
		, _frago(frago)
        , _gc(frago->core()->gc)
        , _labels(_gc)
        , _patches(_gc)
        , pending_lives(_gc)
	{
        AvmCore *core = frago->core();
		nInit(core);
		verbose_only( _verbose = !core->quiet_opt() && core->verbose() );
		verbose_only( _outputCache = 0);
		
		internalReset();
		pageReset();
	}

    void Assembler::arReset()
	{
		_activation.highwatermark = 0;
		_activation.lowwatermark = 0;
		_activation.tos = 0;

		for(uint32_t i=0; i<NJ_MAX_STACK_ENTRY; i++)
			_activation.entry[i] = 0;
	}

 	void Assembler::registerResetAll()
	{
		nRegisterResetAll(_allocator);

		// keep a tally of the registers to check that our allocator works correctly
		debug_only(_allocator.count = _allocator.countFree(); )
		debug_only(_allocator.checkCount(); )
		debug_only(_fpuStkDepth = 0; )
	}

	Register Assembler::registerAlloc(RegisterMask allow)
	{
		RegAlloc &regs = _allocator;
//		RegisterMask prefer = livePastCall(_ins) ? saved : scratch;
		RegisterMask prefer = SavedRegs & allow;
		RegisterMask free = regs.free & allow;

		RegisterMask set = prefer;
		if (set == 0) set = allow;

        if (free)
        {
    		// at least one is free
		    set &= free;

		    // ok we have at least 1 free register so let's try to pick 
		    // the best one given the profile of the instruction 
		    if (!set)
		    {
			    // desired register class is not free so pick first of any class
			    set = free;
		    }
		    NanoAssert((set & allow) != 0);
		    Register r = nRegisterAllocFromSet(set);
		    regs.used |= rmask(r);
		    return r;
        }
		counter_increment(steals);

		// nothing free, steal one 
		// LSRA says pick the one with the furthest use
		LIns* vic = findVictim(regs, allow);
		NanoAssert(vic != NULL);

	    Reservation* resv = getresv(vic);

		// restore vic
	    Register r = resv->reg;
        regs.removeActive(r);
        resv->reg = UnknownReg;

		asm_restore(vic, resv, r);
		return r;
	}

	void Assembler::reserveReset()
	{
		_resvTable[0].arIndex = 0;
		int i;
        for(i=1; i<NJ_MAX_STACK_ENTRY; i++) {
			_resvTable[i].arIndex = i-1;
            _resvTable[i].used = 0;
        }
		_resvFree= i-1;
	}

    /**
     * these instructions don't have to be saved & reloaded to spill,
     * they can just be recalculated w/out any inputs.
     */
    bool Assembler::canRemat(LIns *i) {
        return i->isconst() || i->isconstq() || i->isop(LIR_alloc);
    }

	Reservation* Assembler::reserveAlloc(LInsp i)
	{
		uint32_t item = _resvFree;
        Reservation *r = &_resvTable[item];
		_resvFree = r->arIndex;
		r->reg = UnknownReg;
		r->arIndex = 0;
        r->used = 1;
		if (!item) 
			setError(ResvFull); 
        i->setresv(item);
		return r;
	}

	void Assembler::reserveFree(LInsp i)
	{
        Reservation *rs = getresv(i);
        NanoAssert(rs == &_resvTable[i->resv()]);
		rs->arIndex = _resvFree;
        rs->used = 0;
		_resvFree = i->resv();
        i->setresv(0);
	}

	void Assembler::internalReset()
	{
		// readies for a brand spanking new code generation pass.
		registerResetAll();
		reserveReset();
		arReset();
        pending_lives.clear();
	}

	NIns* Assembler::pageAlloc(bool exitPage)
	{
		Page*& list = (exitPage) ? _nativeExitPages : _nativePages;
		Page* page = _frago->pageAlloc();
		if (page)
		{
			page->next = list;
			list = page;
			nMarkExecute(page);
			_stats.pages++;
		}
		else
		{
			// return prior page (to allow overwrites) and mark out of mem 
			page = list;
			setError(OutOMem);
			if (!list)
				return NULL;
		}
		return &page->code[sizeof(page->code)/sizeof(NIns)]; // just past the end
	}
	
	void Assembler::pageReset()
	{
		pagesFree(_nativePages);		
		pagesFree(_nativeExitPages);
		
		_nIns = 0;
		_nExitIns = 0;
		_stats.pages = 0;

		nativePageReset();
	}
	
	void Assembler::pagesFree(Page*& page)
	{
		while(page)
		{
			Page *next = page->next;  // pull next ptr prior to free
			_frago->pageFree(page);
			page = next;
		}
	}

	#define bytesFromTop(x)		( (size_t)(x) - (size_t)pageTop(x) )
	#define bytesToBottom(x)	( (size_t)pageBottom(x) - (size_t)(x) )
	#define bytesBetween(x,y)	( (size_t)(x) - (size_t)(y) )
	
	int32_t Assembler::codeBytes()
	{
		// start and end on same page?
		size_t exit = 0;
		int32_t pages = _stats.pages;
		if (_nExitIns-1 == _stats.codeExitStart)
			;
		else if (samepage(_nExitIns,_stats.codeExitStart))
			exit = bytesBetween(_stats.codeExitStart, _nExitIns);
		else
		{
			pages--;
			exit = ((intptr_t)_stats.codeExitStart & (NJ_PAGE_SIZE-1)) ? bytesFromTop(_stats.codeExitStart)+1 : 0;
			exit += bytesToBottom(_nExitIns)+1;
		}

		size_t main = 0;
		if (_nIns-1 == _stats.codeStart)
			;
		else if (samepage(_nIns,_stats.codeStart))
			main = bytesBetween(_stats.codeStart, _nIns);
		else
		{
			pages--;
			main = ((intptr_t)_stats.codeStart & (NJ_PAGE_SIZE-1)) ? bytesFromTop(_stats.codeStart)+1 : 0;
			main += bytesToBottom(_nIns)+1;
		}
		//fprintf(stderr,"size %d, exit is %d, main is %d, page count %d, sizeof %d\n", (int)((pages) * NJ_PAGE_SIZE + main + exit),(int)exit, (int)main, (int)_stats.pages, (int)sizeof(Page));
		return (pages) * NJ_PAGE_SIZE + main + exit;		
	}

	#undef bytesFromTop
	#undef bytesToBottom
	#undef byteBetween
	
	Page* Assembler::handoverPages(bool exitPages)
	{
		Page*& list = (exitPages) ? _nativeExitPages : _nativePages;
		NIns*& ins =  (exitPages) ? _nExitIns : _nIns;
		Page* start = list;
		list = 0;
		ins = 0;
		return start;
	}

	#ifdef _DEBUG
	bool Assembler::onPage(NIns* where, bool exitPages)
	{
		Page* page = (exitPages) ? _nativeExitPages : _nativePages;
		bool on = false;
		while(page)
		{
			if (samepage(where-1,page))
				on = true;
			page = page->next;
		}
		return on;
	}
	
	void Assembler::pageValidate()
	{
		if (error()) return;
		// _nIns and _nExitIns need to be at least on
		// one of these pages
		NanoAssertMsg( onPage(_nIns)&& onPage(_nExitIns,true), "Native instruction pointer overstep paging bounds; check overrideProtect for last instruction");
	}
	#endif

	#ifdef _DEBUG
	
	void Assembler::resourceConsistencyCheck()
	{
		if (error()) return;

#ifdef NANOJIT_IA32
        NanoAssert(_allocator.active[FST0] && _fpuStkDepth == -1 ||
            !_allocator.active[FST0] && _fpuStkDepth == 0);
#endif
		
        AR &ar = _activation;
		// check AR entries
		NanoAssert(ar.highwatermark < NJ_MAX_STACK_ENTRY);
		LIns* ins = 0;
		RegAlloc* regs = &_allocator;
		for(uint32_t i = ar.lowwatermark; i < ar.tos; i++)
		{
			ins = ar.entry[i];
			if ( !ins )
				continue;
			Reservation *r = getresv(ins);
            NanoAssert(r != 0);
			int32_t idx = r - _resvTable;
			NanoAssertMsg(idx, "MUST have a resource for the instruction for it to have a stack location assigned to it");
            if (r->arIndex) {
                if (ins->isop(LIR_alloc)) {
                    int j=i+1;
                    for (int n = i + (ins->size()>>2); j < n; j++) {
                        NanoAssert(ar.entry[j]==ins);
                    }
		        	NanoAssert(r->arIndex == (uint32_t)j-1);
                    i = j-1;
                }
                else if (ins->isQuad()) {
                    NanoAssert(ar.entry[i - stack_direction(1)]==ins);
                    i += 1; // skip high word
                }
                else {
        			NanoAssertMsg(r->arIndex == i, "Stack record index mismatch");
                }
            }
			NanoAssertMsg( r->reg==UnknownReg || regs->isConsistent(r->reg,ins), "Register record mismatch");
		}
	
		registerConsistencyCheck();
				
		// check resv table
		int32_t inuseCount = 0;
		int32_t notInuseCount = 0;
        for(uint32_t i=1; i < sizeof(_resvTable)/sizeof(_resvTable[0]); i++) {
            _resvTable[i].used ? inuseCount++ : notInuseCount++;
        }

		int32_t freeCount = 0;
		uint32_t free = _resvFree;
        while(free) {
			free = _resvTable[free].arIndex;
			freeCount++;
		}
		NanoAssert( ( freeCount==notInuseCount && inuseCount+notInuseCount==(NJ_MAX_STACK_ENTRY-1) ) );
	}

	void Assembler::registerConsistencyCheck()
	{	
		// check registers
		RegAlloc *regs = &_allocator;
		uint32_t managed = regs->managed;
		Register r = FirstReg;
		while(managed)
		{
			if (managed&1)
			{
				if (regs->isFree(r))
				{
					NanoAssert(regs->getActive(r)==0);
				}
				else
				{
					LIns* ins = regs->getActive(r);
					// @todo we should be able to check across RegAlloc's somehow (to include savedGP...)
					Reservation *v = getresv(ins);
					NanoAssert(v != 0);
					int32_t idx = v - _resvTable;
					NanoAssert(idx >= 0 && idx < NJ_MAX_STACK_ENTRY);
					NanoAssertMsg(idx, "MUST have a resource for the instruction for it to have a register assigned to it");
					NanoAssertMsg( regs->getActive(v->reg)==ins, "Register record mismatch");
				}			
			}
			
			// next register in bitfield
			r = nextreg(r);
			managed >>= 1;
		}
	}
	#endif /* _DEBUG */

	void Assembler::findRegFor2(RegisterMask allow, LIns* ia, Reservation* &resva, LIns* ib, Reservation* &resvb)
	{
		if (ia == ib) 
		{
			findRegFor(ia, allow);
			resva = resvb = getresv(ia);
		}
		else
		{
			Register rb = UnknownReg;
            resvb = getresv(ib);
            if (resvb && (rb = resvb->reg) != UnknownReg) {
                if (allow & rmask(rb)) {
                    // ib already assigned to an allowable reg, keep that one
                    allow &= ~rmask(rb);
                } else {
                    // ib assigned to unusable reg, pick a different one below.
                    rb = UnknownReg;
                }
            }
            Register ra = findRegFor(ia, allow);
            resva = getresv(ia);
            NanoAssert(error() || (resva != 0 && ra != UnknownReg));
            if (rb == UnknownReg)
            {
                allow &= ~rmask(ra);
                findRegFor(ib, allow);
                resvb = getresv(ib);
            }
		}
	}

	Register Assembler::findSpecificRegFor(LIns* i, Register w)
	{
		return findRegFor(i, rmask(w));
	}

    Register Assembler::getBaseReg(LIns *i, int &d, RegisterMask allow)
    {
        if (i->isop(LIR_alloc)) {
            d += findMemFor(i);
            return FP;
        } else {
            return findRegFor(i, allow);
        }
    }
			
	Register Assembler::findRegFor(LIns* i, RegisterMask allow)
	{
        if (i->isop(LIR_alloc)) {
            // never allocate a reg for this w/out stack space too
            findMemFor(i);
        }

        Reservation* resv = getresv(i);
		Register r;

		// if we have an existing reservation and it has a non-unknown
		// register allocated, and that register is in our allowed mask,
		// return it.
        if (resv && (r=resv->reg) != UnknownReg && (rmask(r) & allow)) {
            _allocator.useActive(r);
			return r;
        }

		// figure out what registers are preferred for this instruction
		RegisterMask prefer = hint(i, allow);

		// if we didn't have a reservation, allocate one now
        if (!resv)
			resv = reserveAlloc(i);

        r = resv->reg;

#ifdef AVMPLUS_IA32
        if (r != UnknownReg && 
            ((rmask(r)&XmmRegs) && !(allow&XmmRegs) ||
                 (rmask(r)&x87Regs) && !(allow&x87Regs)))
        {
            // x87 <-> xmm copy required
            //_nvprof("fpu-evict",1);
            evict(r);
            r = UnknownReg;
        }
#endif

        if (r == UnknownReg)
		{
			r = resv->reg = registerAlloc(prefer);
			_allocator.addActive(r, i);
			return r;
		}
		else
		{
			// the already-allocated register isn't in the allowed mask;
			// we need to grab a new one and then copy over the old
			// contents to the new.
			resv->reg = UnknownReg;
			_allocator.retire(r);
			Register s = resv->reg = registerAlloc(prefer);
			_allocator.addActive(s, i);
            if ((rmask(r) & GpRegs) && (rmask(s) & GpRegs)) {
    			MR(r, s);
            } 
            else {
				asm_nongp_copy(r, s);
			}
			return s;
		}
	}

	int Assembler::findMemFor(LIns *i)
	{
		Reservation* resv = getresv(i);
		if (!resv)
			resv = reserveAlloc(i);
        if (!resv->arIndex) {
			resv->arIndex = arReserve(i);
            NanoAssert(resv->arIndex <= _activation.highwatermark);
        }
		return disp(resv);
	}

	Register Assembler::prepResultReg(LIns *i, RegisterMask allow)
	{
		Reservation* resv = getresv(i);
		const bool pop = !resv || resv->reg == UnknownReg;
		Register rr = findRegFor(i, allow);
		freeRsrcOf(i, pop);
		return rr;
	}

	void Assembler::asm_spilli(LInsp i, Reservation *resv, bool pop)
	{
		int d = disp(resv);
		Register rr = resv->reg;
		bool quad = i->opcode() == LIR_param || i->isQuad();
		asm_spill(rr, d, pop, quad);
		if (d) 
		{
			verbose_only(if (_verbose) {
				outputf("        spill %s",_thisfrag->lirbuf->names->formatRef(i));
			})
		}
	}

	void Assembler::freeRsrcOf(LIns *i, bool pop)
	{
		Reservation* resv = getresv(i);
		int index = resv->arIndex;
		Register rr = resv->reg;

		if (rr != UnknownReg)
		{
			asm_spilli(i, resv, pop);
			_allocator.retire(rr);	// free any register associated with entry
		}
		if (index)
            arFree(index);			// free any stack stack space associated with entry
		reserveFree(i);		// clear fields of entry and add it to free list
	}

	void Assembler::evict(Register r)
	{
		registerAlloc(rmask(r));
		_allocator.addFree(r);
	}

    void Assembler::patch(GuardRecord *lr)
    {
        Fragment *frag = lr->exit->target;
		NanoAssert(frag->fragEntry != 0);
		NIns* was = nPatchBranch((NIns*)lr->jmpToTarget, frag->fragEntry);
		verbose_only(verbose_outputf("patching jump at %p to target %p (was %p)\n",
			lr->jmpToTarget, frag->fragEntry, was);)
		(void)was;
    }

    void Assembler::patch(SideExit *exit)
    {
        GuardRecord *rec = exit->guards;
        AvmAssert(rec);
        while (rec) {
            patch(rec);
            rec = rec->next;
        }
    }

	void Assembler::disconnectLoop(GuardRecord *lr)
	{
        NanoAssert(lr->stubEntry);
        NIns* was = nPatchBranch((NIns*)lr->jmpToStub, (NIns*)lr->stubEntry);
        verbose_only(verbose_outputf("disconnected loop-jump at %p: exiting to %p (was looping to %p)\n",
                                     lr->jmpToStub, lr->stubEntry, was);)
        NanoAssert(lr->exit->from->loopEntry == was);
	}

	void Assembler::reconnectLoop(GuardRecord *lr)
	{
        NanoAssert(lr->exit->from->loopEntry);
        NIns* was = nPatchBranch((NIns*)lr->jmpToStub, lr->exit->from->loopEntry);
        verbose_only(verbose_outputf("reconnected loop-jump at %p: looping to %p (was exiting to %p)\n",
                                     lr->jmpToStub, lr->exit->from->loopEntry, was);)
        NanoAssert(lr->stubEntry == was);
	}
    
    NIns* Assembler::asm_exit(LInsp guard)
    {
		SideExit *exit = guard->record()->exit;
		NIns* at = 0;
		if (!_branchStateMap->get(exit))
		{
			at = asm_leave_trace(guard);
		}
		else
		{
			RegAlloc* captured = _branchStateMap->get(exit);
			intersectRegisterState(*captured);
			verbose_only(
				verbose_outputf("        merging trunk with %s",
					_frago->labels->format(exit->target));
				verbose_outputf("        %p:",_nIns);
			)			
			at = exit->target->fragEntry;
			NanoAssert(at != 0);
			_branchStateMap->remove(exit);
		}
		return at;
	}
	
	NIns* Assembler::asm_leave_trace(LInsp guard)
	{
        verbose_only(bool priorVerbose = _verbose; )
		verbose_only( _verbose = verbose_enabled() && _frago->core()->config.verbose_exits; )
        verbose_only( int32_t nativeSave = _stats.native );
		verbose_only(verbose_outputf("--------------------------------------- end exit block %p", guard);)

		RegAlloc capture = _allocator;

        // this point is unreachable.  so free all the registers.
		// if an instruction has a stack entry we will leave it alone,
		// otherwise we free it entirely.  intersectRegisterState will restore.
		releaseRegisters();
		
		swapptrs();
		_inExit = true;
		
		//verbose_only( verbose_outputf("         LIR_xend swapptrs, _nIns is now %08X(%08X), _nExitIns is now %08X(%08X)",_nIns, *_nIns,_nExitIns,*_nExitIns) );
		debug_only( _sv_fpuStkDepth = _fpuStkDepth; _fpuStkDepth = 0; )

		nFragExit(guard);

		// restore the callee-saved register and parameters
		assignSavedRegs();
		assignParamRegs();

		intersectRegisterState(capture);

		// this can be useful for breaking whenever an exit is taken
		//INT3();
		//NOP();

		// we are done producing the exit logic for the guard so demark where our exit block code begins
		guard->record()->stubEntry = _nIns;	 // target in exit path for our mainline conditional jump 

		// swap back pointers, effectively storing the last location used in the exit path
		swapptrs();
		_inExit = false;
		
		//verbose_only( verbose_outputf("         LIR_xt/xf swapptrs, _nIns is now %08X(%08X), _nExitIns is now %08X(%08X)",_nIns, *_nIns,_nExitIns,*_nExitIns) );
		verbose_only( verbose_outputf("        %p:",guard->record()->stubEntry);)
		verbose_only( verbose_outputf("--------------------------------------- exit block (LIR_xt|LIR_xf)") );

#ifdef NANOJIT_IA32
		NanoAssertMsgf(_fpuStkDepth == _sv_fpuStkDepth, "LIR_xtf, _fpuStkDepth=%d, expect %d\n",_fpuStkDepth, _sv_fpuStkDepth);
		debug_only( _fpuStkDepth = _sv_fpuStkDepth; _sv_fpuStkDepth = 9999; )
#endif

        verbose_only( _verbose = priorVerbose; )
        verbose_only(_stats.exitnative += (_stats.native-nativeSave));

        return (NIns*) guard->record()->stubEntry;
    }
	
	void Assembler::beginAssembly(Fragment *frag, RegAllocMap* branchStateMap)
	{
        _thisfrag = frag;
		_activation.lowwatermark = 1;
		_activation.tos = _activation.lowwatermark;
		_activation.highwatermark = _activation.tos;
		
		counter_reset(native);
		counter_reset(exitnative);
		counter_reset(steals);
		counter_reset(spills);
		counter_reset(remats);

		setError(None);

		// native code gen buffer setup
		nativePageSetup();
		
	#ifdef AVMPLUS_PORTING_API
		_endJit1Addr = _nIns;
		_endJit2Addr = _nExitIns;
	#endif

		// make sure we got memory at least one page
		if (error()) return;
			
#ifdef PERFM
		_stats.pages = 0;
		_stats.codeStart = _nIns-1;
		_stats.codeExitStart = _nExitIns-1;		
		//fprintf(stderr,"pageReset %d start %x exit start %x\n", _stats.pages, (int)_stats.codeStart, (int)_stats.codeExitStart);
#endif /* PERFM */

        _epilogue = genEpilogue();
		_branchStateMap = branchStateMap;
        _labels.clear();
        _patches.clear();

		verbose_only( verbose_outputf("        %p:",_nIns) );
		verbose_only( verbose_output("        epilogue:") );
	}
	
	void Assembler::assemble(Fragment* frag,  NInsList& loopJumps)
	{
		if (error()) return;	
		AvmCore *core = _frago->core();
        _thisfrag = frag;

		// set up backwards pipeline: assembler -> StackFilter -> LirReader
		LirReader bufreader(frag->lastIns);
		GC *gc = core->gc;
		StackFilter storefilter1(&bufreader, gc, frag->lirbuf, frag->lirbuf->sp);
		StackFilter storefilter2(&storefilter1, gc, frag->lirbuf, frag->lirbuf->rp);
		DeadCodeFilter deadfilter(&storefilter2, frag->lirbuf->_functions);
		LirFilter* rdr = &deadfilter;
		verbose_only(
			VerboseBlockReader vbr(rdr, this, frag->lirbuf->names);
			if (verbose_enabled())
				rdr = &vbr;
		)

		verbose_only(_thisfrag->compileNbr++; )
		verbose_only(_frago->_stats.compiles++; )
		verbose_only(_frago->_stats.totalCompiles++; )
		_inExit = false;	
        gen(rdr, loopJumps);
		frag->loopEntry = _nIns;
		//frag->outbound = core->config.tree_opt? _latestGuard : 0;
		//fprintf(stderr, "assemble frag %X entry %X\n", (int)frag, (int)frag->fragEntry);

        if (!error()) {
		    // patch all branches
		    while(!_patches.isEmpty())
		    {
			    NIns* where = _patches.lastKey();
			    LInsp targ = _patches.removeLast();
                LabelState *label = _labels.get(targ);
			    NIns* ntarg = label->addr;
                if (ntarg) {
				    nPatchBranch(where,ntarg);
			    }
                else {
				    _err = UnknownBranch;
				    break;
			    }
		    }
        }
	}

	void Assembler::endAssembly(Fragment* frag, NInsList& loopJumps)
	{
	    NIns* SOT = 0;
	    if (frag->isRoot()) {
	        SOT = frag->loopEntry;
            verbose_only( verbose_outputf("        %p:",_nIns); )
	    } else {
	        SOT = frag->root->fragEntry;
	    }
        AvmAssert(SOT);
		while(!loopJumps.isEmpty())
		{
			NIns* loopJump = (NIns*)loopJumps.removeLast();
            verbose_only( verbose_outputf("patching %p to %p", loopJump, SOT); )
			nPatchBranch(loopJump, SOT);
		}

		NIns* fragEntry = 0;
		
		if (!error())
		{
			fragEntry = genPrologue();
			verbose_only( verbose_outputf("        %p:",_nIns); )
			verbose_only( verbose_output("        prologue"); )
		}
		
		// something bad happened?
		if (!error())
		{
			// check for resource leaks 
			debug_only( 
				for(uint32_t i=_activation.lowwatermark;i<_activation.highwatermark; i++) {
					NanoAssertMsgf(_activation.entry[i] == 0, "frame entry %d wasn't freed\n",-4*i);
				}
			)

            frag->fragEntry = fragEntry;
			NIns* code = _nIns;
#ifdef PERFM
			_nvprof("code", codeBytes());  // requires that all pages are released between begin/endAssembly()otherwise we double count
#endif
			// let the fragment manage the pages if we're using trees and there are branches
			Page* manage = (_frago->core()->config.tree_opt) ? handoverPages() : 0;			
			frag->setCode(code, manage); // root of tree should manage all pages
			//fprintf(stderr, "endAssembly frag %X entry %X\n", (int)frag, (int)frag->fragEntry);
		}
		
		NanoAssertMsgf(error() || _fpuStkDepth == 0,"_fpuStkDepth %d\n",_fpuStkDepth);

		internalReset();  // clear the reservation tables and regalloc
		NanoAssert(!_branchStateMap || _branchStateMap->isEmpty());
		_branchStateMap = 0;

#ifdef AVMPLUS_ARM
		// If we've modified the code, we need to flush so we don't end up trying 
		// to execute junk
# if defined(UNDER_CE)
		FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
# elif defined(AVMPLUS_UNIX)
		for (int i = 0; i < 2; i++) {
			Page *p = (i == 0) ? _nativePages : _nativeExitPages;

			Page *first = p;
			while (p) {
				if (!p->next || p->next != p+1) {
					__clear_cache((char*)first, (char*)(p+1));
					first = p->next;
				}
				p = p->next;
			}
		}
# endif
#endif

# ifdef AVMPLUS_PORTING_API
		NanoJIT_PortAPI_FlushInstructionCache(_nIns, _endJit1Addr);
		NanoJIT_PortAPI_FlushInstructionCache(_nExitIns, _endJit2Addr);
# endif
	}
	
	void Assembler::copyRegisters(RegAlloc* copyTo)
	{
		*copyTo = _allocator;
	}

	void Assembler::releaseRegisters()
	{
		for (Register r = FirstReg; r <= LastReg; r = nextreg(r))
		{
			LIns *i = _allocator.getActive(r);
			if (i)
			{
				// clear reg allocation, preserve stack allocation.
				Reservation* resv = getresv(i);
				NanoAssert(resv != 0);
				_allocator.retire(r);
				if (r == resv->reg)
					resv->reg = UnknownReg;

				if (!resv->arIndex && resv->reg == UnknownReg)
				{
					reserveFree(i);
				}
			}
		}
	}

#ifdef PERFM
#define countlir_live() _nvprof("lir-live",1)
#define countlir_ret() _nvprof("lir-ret",1)
#define countlir_alloc() _nvprof("lir-alloc",1)
#define countlir_var() _nvprof("lir-var",1)
#define countlir_use() _nvprof("lir-use",1)
#define countlir_def() _nvprof("lir-def",1)
#define countlir_imm() _nvprof("lir-imm",1)
#define countlir_param() _nvprof("lir-param",1)
#define countlir_cmov() _nvprof("lir-cmov",1)
#define countlir_ld() _nvprof("lir-ld",1)
#define countlir_ldq() _nvprof("lir-ldq",1)
#define countlir_alu() _nvprof("lir-alu",1)
#define countlir_qjoin() _nvprof("lir-qjoin",1)
#define countlir_qlo() _nvprof("lir-qlo",1)
#define countlir_qhi() _nvprof("lir-qhi",1)
#define countlir_fpu() _nvprof("lir-fpu",1)
#define countlir_st() _nvprof("lir-st",1)
#define countlir_stq() _nvprof("lir-stq",1)
#define countlir_jmp() _nvprof("lir-jmp",1)
#define countlir_jcc() _nvprof("lir-jcc",1)
#define countlir_label() _nvprof("lir-label",1)
#define countlir_xcc() _nvprof("lir-xcc",1)
#define countlir_x() _nvprof("lir-x",1)
#define countlir_loop() _nvprof("lir-loop",1)
#define countlir_call() _nvprof("lir-call",1)
#else
#define countlir_live()
#define countlir_ret()
#define countlir_alloc()
#define countlir_var()
#define countlir_use()
#define countlir_def()
#define countlir_imm()
#define countlir_param()
#define countlir_cmov()
#define countlir_ld()
#define countlir_ldq()
#define countlir_alu()
#define countlir_qjoin()
#define countlir_qlo()
#define countlir_qhi()
#define countlir_fpu()
#define countlir_st()
#define countlir_stq()
#define countlir_jmp()
#define countlir_jcc()
#define countlir_label()
#define countlir_xcc()
#define countlir_x()
#define countlir_loop()
#define countlir_call()
#endif

	void Assembler::gen(LirFilter* reader,  NInsList& loopJumps)
	{
		// trace must start with LIR_x or LIR_loop
		NanoAssert(reader->pos()->isop(LIR_x) || reader->pos()->isop(LIR_loop));
		 
		for (LInsp ins = reader->read(); ins != 0 && !error(); ins = reader->read())
		{
			LOpcode op = ins->opcode();			
			switch(op)
			{
				default:
					NanoAssertMsgf(false, "unsupported LIR instruction: %d (~0x40: %d)\n", op, op&~LIR64);
					break;
					
                case LIR_live: {
                    countlir_live();
                    pending_lives.add(ins->oprnd1());
                    break;
                }

                case LIR_ret:  {
                    countlir_ret();
                    if (_nIns != _epilogue) {
                        JMP(_epilogue);
                    }
                    assignSavedRegs();
#ifdef NANOJIT_ARM
                    // the epilogue moves R2 to R0; we may want to do this
                    // after assignSavedRegs
                    findSpecificRegFor(ins->oprnd1(), R2);
#else
                    findSpecificRegFor(ins->oprnd1(), retRegs[0]);
#endif
                    break;
                }

                case LIR_fret: {
                    countlir_ret();
                    if (_nIns != _epilogue) {
                        JMP(_epilogue);
                    }
                    assignSavedRegs();
#ifdef NANOJIT_IA32
                    findSpecificRegFor(ins->oprnd1(), FST0);
#else
                    NanoAssert(false);
#endif
                    fpu_pop();
                    break;
                }

                // allocate some stack space.  the value of this instruction
                // is the address of the stack space.
                case LIR_alloc: {
                    countlir_alloc();
                    Reservation *resv = getresv(ins);
                    NanoAssert(resv->arIndex != 0);
                    Register r = resv->reg;
                    if (r != UnknownReg) {
    			        _allocator.retire(r);
                        resv->reg = UnknownReg;
                        asm_restore(ins, resv, r);
                    }
                    freeRsrcOf(ins, 0);
                    break;
                }
				case LIR_short:
				{
                    countlir_imm();
					asm_short(ins);
					break;
				}
				case LIR_int:
				{
                    countlir_imm();
					asm_int(ins);
					break;
				}
				case LIR_quad:
				{
                    countlir_imm();
					asm_quad(ins);
					break;
				}
#if !defined NANOJIT_64BIT
				case LIR_callh:
				{
					// return result of quad-call in register
					prepResultReg(ins, rmask(retRegs[1]));
                    // if hi half was used, we must use the call to ensure it happens
                    findRegFor(ins->oprnd1(), rmask(retRegs[0]));
					break;
				}
#endif
				case LIR_param:
				{
                    countlir_param();
					asm_param(ins);
					break;
				}
				case LIR_qlo:
                {
                    countlir_qlo();
					asm_qlo(ins);
					break;
				}
				case LIR_qhi:
				{
                    countlir_qhi();
					asm_qhi(ins);
					break;
				}
				case LIR_qcmov:
				case LIR_cmov:
				{
                    countlir_cmov();
					asm_cmov(ins);
					break;
				}				
				case LIR_ld:
				case LIR_ldc:
				case LIR_ldcb:
				case LIR_ldcs:
				{
                    countlir_ld();
					asm_ld(ins);
					break;
				}
				case LIR_ldq:
				case LIR_ldqc:
				{
                    countlir_ldq();
					asm_load64(ins);
					break;
				}
				case LIR_neg:
				case LIR_not:
				{
                    countlir_alu();
					asm_neg_not(ins);
					break;
				}
				case LIR_qjoin:
				{
                    countlir_qjoin();
                    asm_qjoin(ins);
					break;
				}

#if defined NANOJIT_64BIT
                case LIR_qiadd:
                case LIR_qiand:
                case LIR_qilsh:
                case LIR_qior:
                {
                    asm_qbinop(ins);
                    break;
                }
#endif

				case LIR_add:
				case LIR_addp:
				case LIR_sub:
				case LIR_mul:
				case LIR_and:
				case LIR_or:
				case LIR_xor:
				case LIR_lsh:
				case LIR_rsh:
				case LIR_ush:
				{
                    countlir_alu();
					asm_arith(ins);
					break;
				}
#ifndef NJ_SOFTFLOAT
				case LIR_fneg:
				{
                    countlir_fpu();
					asm_fneg(ins);
					break;
				}
				case LIR_fadd:
				case LIR_fsub:
				case LIR_fmul:
				case LIR_fdiv:
				{
                    countlir_fpu();
					asm_fop(ins);
                    break;
				}
				case LIR_i2f:
				{
                    countlir_fpu();
					asm_i2f(ins);
					break;
				}
				case LIR_u2f:
				{
                    countlir_fpu();
					asm_u2f(ins);
					break;
				}
#endif // NJ_SOFTFLOAT
				case LIR_st:
				case LIR_sti:
				{
                    countlir_st();
                    asm_store32(ins->oprnd1(), ins->immdisp(), ins->oprnd2());
                    break;
				}
				case LIR_stq:
				case LIR_stqi:
				{
                    countlir_stq();
					LIns* value = ins->oprnd1();
					LIns* base = ins->oprnd2();
					int dr = ins->immdisp();
					if (value->isop(LIR_qjoin)) 
					{
						// this is correct for little-endian only
						asm_store32(value->oprnd1(), dr, base);
						asm_store32(value->oprnd2(), dr+4, base);
					}
					else 
					{
						asm_store64(value, dr, base);
					}
                    break;
				}

				case LIR_j:
				{
                    countlir_jmp();
					LInsp to = ins->getTarget();
                    LabelState *label = _labels.get(to);
                    // the jump is always taken so whatever register state we
                    // have from downstream code, is irrelevant to code before
                    // this jump.  so clear it out.  we will pick up register
                    // state from the jump target, if we have seen that label.
                    releaseRegisters();
                    if (label && label->addr) {
                        // forward jump - pick up register state from target.
                        unionRegisterState(label->regs);
                        JMP(label->addr);
                    }
                    else {
                        // backwards jump
                        hasLoop = true;
                        handleLoopCarriedExprs();
                        if (!label) {
                            // save empty register state at loop header
                            _labels.add(to, 0, _allocator);
                        }
                        else {
                            intersectRegisterState(label->regs);
                        }
                        JMP(0);
    					_patches.put(_nIns, to);
                        verbose_only(
                            verbose_outputf("        Loop %s -> %s", 
                                lirNames[ins->opcode()], 
                                _thisfrag->lirbuf->names->formatRef(to));
                        )
                    }
					break;
				}

				case LIR_jt:
				case LIR_jf:
				{
                    countlir_jcc();
					LInsp to = ins->getTarget();
					LIns* cond = ins->oprnd1();
                    LabelState *label = _labels.get(to);
                    if (label && label->addr) {
                        // forward jump to known label.  need to merge with label's register state.
                        unionRegisterState(label->regs);
    					asm_branch(op == LIR_jf, cond, label->addr, false);
                    }
                    else {
                        // back edge.
                        hasLoop = true;
                        handleLoopCarriedExprs();
                        if (!label) {
                            // evict all registers, most conservative approach.
                            evictRegs(~_allocator.free);
                            _labels.add(to, 0, _allocator);
                        } 
                        else {
                            // evict all registers, most conservative approach.
                            intersectRegisterState(label->regs);
                        }
                        NIns *branch = asm_branch(op == LIR_jf, cond, 0, false);
			            _patches.put(branch,to);
                        verbose_only(
                            verbose_outputf("Loop %s -> %s", 
                                lirNames[ins->opcode()], 
                                _thisfrag->lirbuf->names->formatRef(to));
                        )
                    }
					break;
				}					
				case LIR_label:
				{
                    countlir_label();
                    LabelState *label = _labels.get(ins);
                    if (!label) {
                        // label seen first, normal target of forward jump, save addr & allocator
    					_labels.add(ins, _nIns, _allocator);
                    }
                    else {
                        // we're at the top of a loop
                        hasLoop = true;
                        NanoAssert(label->addr == 0 && label->regs.isValid());
                        //evictRegs(~_allocator.free);
                        intersectRegisterState(label->regs);
                        //asm_align_code();
                        label->addr = _nIns;
                        verbose_only(
                            verbose_outputf("Loop %s", _thisfrag->lirbuf->names->formatRef(ins));
                        )
                    }
					break;
				}

                case LIR_xt:
				case LIR_xf:
				{
                    countlir_xcc();
					// we only support cmp with guard right now, also assume it is 'close' and only emit the branch
                    NIns* exit = asm_exit(ins); // does intersectRegisterState()
					LIns* cond = ins->oprnd1();
					asm_branch(op == LIR_xf, cond, exit, false);
					break;
				}
				case LIR_x:
				{
                    countlir_x();
		            verbose_only(verbose_output(""));
					// generate the side exit branch on the main trace.
                    NIns *exit = asm_exit(ins);
					JMP( exit ); 
					break;
				}
				case LIR_loop:
				{
                    countlir_loop();
					asm_loop(ins, loopJumps);
			        assignSavedRegs();
			        assignParamRegs();
					break;
				}

#ifndef NJ_SOFTFLOAT
				case LIR_feq:
				case LIR_fle:
				case LIR_flt:
				case LIR_fgt:
				case LIR_fge:
				{
                    countlir_fpu();
					asm_fcond(ins);
					break;
				}
#endif
				case LIR_eq:
                case LIR_ov:
                case LIR_cs:
				case LIR_le:
				case LIR_lt:
				case LIR_gt:
				case LIR_ge:
				case LIR_ult:
				case LIR_ule:
				case LIR_ugt:
				case LIR_uge:
				{
                    countlir_alu();
					asm_cond(ins);
					break;
				}
				
#ifndef NJ_SOFTFLOAT
				case LIR_fcall:
				case LIR_fcalli:
#endif
#if defined NANOJIT_64BIT
				case LIR_callh:
#endif
				case LIR_call:
				case LIR_calli:
				{
                    countlir_call();
                    Register rr = UnknownReg;
#ifndef NJ_SOFTFLOAT
                    if ((op&LIR64))
                    {
                        // fcall or fcalli
						Reservation* rR = getresv(ins);
						rr = asm_prep_fcall(rR, ins);
                    }
                    else
#endif
                    {
                        rr = retRegs[0];
						prepResultReg(ins, rmask(rr));
                    }

					// do this after we've handled the call result, so we dont
					// force the call result to be spilled unnecessarily.

					evictScratchRegs();

					asm_call(ins);
				}
			}

			if (error())
				return;

			// check that all is well (don't check in exit paths since its more complicated)
			debug_only( pageValidate(); )
			debug_only( resourceConsistencyCheck();  )
		}
	}

    void Assembler::assignSavedRegs()
    {
        // restore saved regs
		releaseRegisters();
        LirBuffer *b = _thisfrag->lirbuf;
        for (int i=0, n = NumSavedRegs; i < n; i++) {
            LIns *p = b->savedRegs[i];
            if (p)
                findSpecificRegFor(p, savedRegs[p->imm8()]);
        }
    }

    void Assembler::reserveSavedRegs()
    {
        LirBuffer *b = _thisfrag->lirbuf;
        for (int i=0, n = NumSavedRegs; i < n; i++) {
            LIns *p = b->savedRegs[i];
            if (p)
                findMemFor(p);
        }
    }

    // restore parameter registers
    void Assembler::assignParamRegs()
    {
        LInsp state = _thisfrag->lirbuf->state;
        if (state)
            findSpecificRegFor(state, argRegs[state->imm8()]); 
        LInsp param1 = _thisfrag->lirbuf->param1;
        if (param1)
            findSpecificRegFor(param1, argRegs[param1->imm8()]);
    }
    
    void Assembler::handleLoopCarriedExprs()
    {
        // ensure that exprs spanning the loop are marked live at the end of the loop
        reserveSavedRegs();
        for (int i=0, n=pending_lives.size(); i < n; i++) {
            findMemFor(pending_lives[i]);
        }
    }

	void Assembler::arFree(uint32_t idx)
	{
        AR &ar = _activation;
        LIns *i = ar.entry[idx];
        NanoAssert(i != 0);
        do {
            ar.entry[idx] = 0;
            idx--;
        } while (ar.entry[idx] == i);
	}

#ifdef NJ_VERBOSE
	void Assembler::printActivationState()
	{
		bool verbose_activation = false;
		if (!verbose_activation)
			return;
			
#ifdef NANOJIT_ARM
		// @todo Why is there here?!?  This routine should be indep. of platform
		verbose_only(
			if (_verbose) {
				char* s = &outline[0];
				memset(s, ' ', 51);  s[51] = '\0';
				s += strlen(s);
				sprintf(s, " SP ");
				s += strlen(s);
				for(uint32_t i=_activation.lowwatermark; i<_activation.tos;i++) {
					LInsp ins = _activation.entry[i];
					if (ins && ins !=_activation.entry[i+1]) {
						sprintf(s, "%d(%s) ", 4*i, _thisfrag->lirbuf->names->formatRef(ins));
						s += strlen(s);
					}
				}
				output(&outline[0]);
			}
		)
#else
		verbose_only(
			char* s = &outline[0];
			if (_verbose) {
				memset(s, ' ', 51);  s[51] = '\0';
				s += strlen(s);
				sprintf(s, " ebp ");
				s += strlen(s);

				for(uint32_t i=_activation.lowwatermark; i<_activation.tos;i++) {
					LInsp ins = _activation.entry[i];
					if (ins /* && _activation.entry[i]!=_activation.entry[i+1]*/) {
						sprintf(s, "%d(%s) ", -4*i,_thisfrag->lirbuf->names->formatRef(ins));
						s += strlen(s);
					}
				}
				output(&outline[0]);
			}
		)
#endif
	}
#endif

    bool canfit(int32_t size, int32_t loc, AR &ar) {
        for (int i=0; i < size; i++) {
            if (ar.entry[loc+stack_direction(i)])
                return false;
        }
        return true;
    }
	
	uint32_t Assembler::arReserve(LIns* l)
	{
		NanoAssert(!l->isTramp());

		//verbose_only(printActivationState());
        int32_t size = l->isop(LIR_alloc) ? (l->size()>>2) : l->isQuad() ? 2 : sizeof(intptr_t)>>2;
        AR &ar = _activation;
		const int32_t tos = ar.tos;
		int32_t start = ar.lowwatermark;
		int32_t i = 0;
		NanoAssert(start>0);

        if (size == 1) {
            // easy most common case -- find a hole, or make the frame bigger
            for (i=start; i < NJ_MAX_STACK_ENTRY; i++) {
                if (ar.entry[i] == 0) {
                    // found a hole
                    ar.entry[i] = l;
                    break;
                }
            }
        }
        else if (size == 2) {
			if ( (start&1)==1 ) start++;  // even 8 boundary
			for (i=start; i < NJ_MAX_STACK_ENTRY; i+=2) {
                if ( (ar.entry[i+stack_direction(1)] == 0) && (i==tos || (ar.entry[i] == 0)) ) {
                    // found 2 adjacent aligned slots
                    NanoAssert(_activation.entry[i] == 0);
                    NanoAssert(_activation.entry[i+stack_direction(1)] == 0);
                    ar.entry[i] = l;
                    ar.entry[i+stack_direction(1)] = l;
                    break;   
                }
			}
		}
        else {
            // alloc larger block on 8byte boundary.
            if (start < size) start = size;
            if ((start&1)==1) start++;
            for (i=start; i < NJ_MAX_STACK_ENTRY; i+=2) {
                if (canfit(size, i, ar)) {
		            // place the entry in the table and mark the instruction with it
                    for (int32_t j=0; j < size; j++) {
                        NanoAssert(_activation.entry[i+stack_direction(j)] == 0);
                        _activation.entry[i+stack_direction(j)] = l;
                    }
                    break;
                }
            }
		}
        if (i >= (int32_t)ar.tos) {
            ar.tos = ar.highwatermark = i+1;
        }
		if (tos+size >= NJ_MAX_STACK_ENTRY) {	
			setError(StackFull);
		}
        return i;
	}

    /**
     * move regs around so the SavedRegs contains the highest priority regs.
     */
    void Assembler::evictScratchRegs()
    {
		// find the top GpRegs that are candidates to put in SavedRegs

        // tosave is a binary heap stored in an array.  the root is tosave[0],
        // left child is at i+1, right child is at i+2.  

        Register tosave[LastReg-FirstReg+1];
        int len=0;
        RegAlloc *regs = &_allocator;
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
			if (rmask(r) & GpRegs) {
				LIns *i = regs->getActive(r);
				if (i) {
					if (canRemat(i)) {
						evict(r);
					}
					else {
						int32_t pri = regs->getPriority(r);
                        // add to heap by adding to end and bubbling up
                        int j = len++;
                        while (j > 0 && pri > regs->getPriority(tosave[j/2])) {
                            tosave[j] = tosave[j/2];
                            j /= 2;
                        }
                        NanoAssert(size_t(j) < sizeof(tosave)/sizeof(tosave[0]));
                        tosave[j] = r;
					}
				}
            }
        }

        // now primap has the live exprs in priority order.  
		// allocate each of the top priority exprs to a SavedReg

        RegisterMask allow = SavedRegs;
        while (allow && len > 0) {
            // get the highest priority var
            Register hi = tosave[0];
            LIns *i = regs->getActive(hi);
            Register r = findRegFor(i, allow);
			allow &= ~rmask(r);

            // remove from heap by replacing root with end element and bubbling down.
            if (allow && --len > 0) {
                Register last = tosave[len];
                int j = 0;
                while (j+1 < len) {
                    int child = j+1;
                    if (j+2 < len && regs->getPriority(tosave[j+2]) > regs->getPriority(tosave[j+1]))
                        child++;
                    if (regs->getPriority(last) > regs->getPriority(tosave[child]))
                        break;
                    tosave[j] = tosave[child];
                    j = child;
                }
                tosave[j] = last;
            }
        }

		// now evict everything else.
		evictRegs(~SavedRegs);
    }

	void Assembler::evictRegs(RegisterMask regs)
	{
		// generate code to restore callee saved registers 
		// @todo speed this up
        for (Register r = FirstReg; r <= LastReg; r = nextreg(r)) {
            if ((rmask(r) & regs) && _allocator.getActive(r)) {
				evict(r);
            }
		}
	}
	
	/**
	 * Merge the current state of the registers with a previously stored version
     * current == saved    skip
     * current & saved     evict current, keep saved
     * current & !saved    evict current  (unionRegisterState would keep)
     * !current & saved    keep saved
	 */
	void Assembler::intersectRegisterState(RegAlloc& saved)
	{
		// evictions and pops first
		RegisterMask skip = 0;
		for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
		{
			LIns * curins = _allocator.getActive(r);
			LIns * savedins = saved.getActive(r);
			if (curins == savedins)
			{
				verbose_only( if (curins) verbose_outputf("        skip %s", regNames[r]); )
				skip |= rmask(r);
			}
			else 
			{
                if (curins) {
                    //_nvprof("intersect-evict",1);
					evict(r);
                }
				
    			#ifdef NANOJIT_IA32
				if (savedins && (rmask(r) & x87Regs))
					FSTP(r);
				#endif
			}
		}
        assignSaved(saved, skip);
	}

	/**
	 * Merge the current state of the registers with a previously stored version.
     * 
     * current == saved    skip
     * current & saved     evict current, keep saved
     * current & !saved    keep current (intersectRegisterState would evict)
     * !current & saved    keep saved
	 */
	void Assembler::unionRegisterState(RegAlloc& saved)
	{
		// evictions and pops first
		RegisterMask skip = 0;
		for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
		{
			LIns * curins = _allocator.getActive(r);
			LIns * savedins = saved.getActive(r);
			if (curins == savedins)
			{
				verbose_only( if (curins) verbose_outputf("        skip %s", regNames[r]); )
				skip |= rmask(r);
			}
			else 
			{
                if (curins && savedins) {
                    //_nvprof("union-evict",1);
					evict(r);
                }
				
    			#ifdef NANOJIT_IA32
				if (rmask(r) & x87Regs) {
					if (savedins) {
						FSTP(r);
					}
					else {
						// saved state did not have fpu reg allocated,
						// so we must evict here to keep x87 stack balanced.
						evict(r);
					}
				}
				#endif
			}
		}
        assignSaved(saved, skip);
    }

    void Assembler::assignSaved(RegAlloc &saved, RegisterMask skip)
    {
		// now reassign mainline registers
		for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
		{
			LIns *i = saved.getActive(r);
			if (i && !(skip&rmask(r)))
				findSpecificRegFor(i, r);
		}
		debug_only(saved.used = 0);  // marker that we are no longer in exit path
	}
	
	void Assembler::setCallTable(const CallInfo* functions)
	{
		_functions = functions;
	}

	#ifdef NJ_VERBOSE
		char Assembler::outline[8192];

		void Assembler::outputf(const char* format, ...)
		{
			va_list args;
			va_start(args, format);
			outline[0] = '\0';
			vsprintf(outline, format, args);
			output(outline);
		}

		void Assembler::output(const char* s)
		{
			if (_outputCache)
			{
				char* str = (char*)_gc->Alloc(strlen(s)+1);
				strcpy(str, s);
				_outputCache->add(str);
			}
			else
			{
				_frago->core()->console << s << "\n";
			}
		}

		void Assembler::output_asm(const char* s)
		{
			if (!verbose_enabled())
				return;
			if (*s != '^')
				output(s);
		}

		char* Assembler::outputAlign(char *s, int col) 
		{
			int len = strlen(s);
			int add = ((col-len)>0) ? col-len : 1;
			memset(&s[len], ' ', add);
			s[col] = '\0';
			return &s[col];
		}
	#endif // verbose

	#endif /* FEATURE_NANOJIT */

#if defined(FEATURE_NANOJIT) || defined(NJ_VERBOSE)
	uint32_t CallInfo::_count_args(uint32_t mask) const
	{
		uint32_t argc = 0;
		uint32_t argt = _argtypes;
		for (uint32_t i = 0; i < MAXARGS; ++i) {
			argt >>= 2;
			argc += (argt & mask) != 0;
		}
		return argc;
	}

    uint32_t CallInfo::get_sizes(ArgSize* sizes) const
    {
		uint32_t argt = _argtypes;
		uint32_t argc = 0;
		for (uint32_t i = 0; i < MAXARGS; i++) {
			argt >>= 2;
			ArgSize a = ArgSize(argt&3);
#ifdef NJ_SOFTFLOAT
			if (a == ARGSIZE_F) {
                sizes[argc++] = ARGSIZE_LO;
                sizes[argc++] = ARGSIZE_LO;
                continue;
            }
#endif
            if (a != ARGSIZE_NONE) {
                sizes[argc++] = a;
            }
		}
        if (isIndirect()) {
            // add one more arg for indirect call address
            argc++;
        }
        return argc;
    }

    void LabelStateMap::add(LIns *label, NIns *addr, RegAlloc &regs) {
        LabelState *st = new (gc) LabelState(addr, regs);
        labels.put(label, st);
    }

    LabelStateMap::~LabelStateMap() {
        clear();
    }

    void LabelStateMap::clear() {
        LabelState *st;

        while (!labels.isEmpty()) {
            st = labels.removeLast();
            delete st;
        }
    }

    LabelState* LabelStateMap::get(LIns *label) {
        return labels.get(label);
    }
}
#endif // FEATURE_NANOJIT
