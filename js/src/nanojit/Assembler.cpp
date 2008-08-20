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

#ifdef AVMPLUS_PORTING_API
#include "portapi_nanojit.h"
#endif

#if defined(AVMPLUS_LINUX) && defined(AVMPLUS_ARM)
#include <asm/unistd.h>
#endif

namespace nanojit
{
	#ifdef FEATURE_NANOJIT


	class DeadCodeFilter: public LirFilter
	{
		Assembler *assm;
	public:
		DeadCodeFilter(LirFilter *in, Assembler *a) : LirFilter(in), assm(a) {}
		LInsp read() {
			for (;;) {
				LInsp i = in->read();
				if (!i || i->isGuard() 
					|| i->isCall() && !assm->_functions[i->fid()]._cse
					|| !assm->ignoreInstruction(i))
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
	public:
		VerboseBlockReader(LirFilter *in, Assembler *a, LirNameMap *n) 
			: LirFilter(in), assm(a), names(n), block(a->_gc) {}

		void flush() {
			assm->outputf("        %p:", assm->_nIns);
			assm->output("");
			for (int j=0,n=block.size(); j < n; j++)
				assm->outputf("    %s", names->formatIns(block[j]));
			assm->output("");
			block.clear();
		}

		LInsp read() {
			LInsp i = in->read();
			if (!i) {
				flush();
				return i;
			}
			if (i->isGuard()) {
				flush();
				block.add(i);
				if (i->oprnd1())
					block.add(i->oprnd1());
			}
			else {
				block.add(i);
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
        : _frago(frago)
        , _gc(frago->core()->gc)
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
		for(uint32_t i=0; i<NJ_MAX_PARAMETERS; i++)
			_activation.parameter[i] = 0;
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
		LIns* vic = findVictim(regs,allow,prefer);
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
		for(i=1; i<NJ_MAX_STACK_ENTRY; i++)
			_resvTable[i].arIndex = i-1;
		_resvFree= i-1;
	}

	Reservation* Assembler::reserveAlloc(LInsp i)
	{
		uint32_t item = _resvFree;
        Reservation *r = &_resvTable[item];
		_resvFree = r->arIndex;
		r->reg = UnknownReg;
		r->arIndex = 0;
		if (!item) 
			setError(ResvFull); 

        if (i->isconst() || i->isconstq())
            r->cost = 0;
        else if (i == _thisfrag->lirbuf->sp || i == _thisfrag->lirbuf->rp)
            r->cost = 2;
        else
            r->cost = 1;

        i->setresv(item);
		return r;
	}

	void Assembler::reserveFree(LInsp i)
	{
        Reservation *rs = getresv(i);
        NanoAssert(rs == &_resvTable[i->resv()]);
		rs->arIndex = _resvFree;
		_resvFree = i->resv();
        i->setresv(0);
	}

	void Assembler::internalReset()
	{
		// readies for a brand spanking new code generation pass.
		registerResetAll();
		reserveReset();
		arReset();
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
		}
		else
		{
			// return prior page (to allow overwrites) and mark out of mem 
			page = list;
			setError(OutOMem);
		}
		return &page->code[sizeof(page->code)/sizeof(NIns)]; // just past the end
	}
	
	void Assembler::pageReset()
	{
		pagesFree(_nativePages);		
		pagesFree(_nativeExitPages);
		
		_nIns = 0;
		_nExitIns = 0;

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

	const CallInfo* Assembler::callInfoFor(uint32_t fid)
	{	
		NanoAssert(fid < CI_Max);
		return &_functions[fid];
	}

	#ifdef _DEBUG
	
	void Assembler::resourceConsistencyCheck()
	{
		if (error()) return;

#ifdef NANOJIT_IA32
        NanoAssert(_allocator.active[FST0] && _fpuStkDepth == -1 ||
            !_allocator.active[FST0] && _fpuStkDepth == 0);
#endif
		
		// for tracking resv usage
		LIns* resv[NJ_MAX_STACK_ENTRY];
		for(int i=0; i<NJ_MAX_STACK_ENTRY; i++)
			resv[i]=0;
			
		// check AR entries
		NanoAssert(_activation.highwatermark < NJ_MAX_STACK_ENTRY);
		LIns* ins = 0;
		RegAlloc* regs = &_allocator;
		for(uint32_t i=_activation.lowwatermark; i<_activation.tos; i++)
		{
			ins = _activation.entry[i];
			if ( !ins )
				continue;
			Reservation *r = getresv(ins);
			int32_t idx = r - _resvTable;
			resv[idx]=ins;
			NanoAssertMsg(idx, "MUST have a resource for the instruction for it to have a stack location assigned to it");
			NanoAssertMsg( r->arIndex==0 || r->arIndex==i || (ins->isQuad()&&r->arIndex==i-(stack_direction(1))), "Stack record index mismatch");
			NanoAssertMsg( r->reg==UnknownReg || regs->isConsistent(r->reg,ins), "Register record mismatch");
		}
	
		registerConsistencyCheck(resv);
				
		// check resv table
		int32_t inuseCount = 0;
		int32_t notInuseCount = 0;
		for(uint32_t i=1; i<NJ_MAX_STACK_ENTRY; i++)
		{
			if (resv[i]==0)
			{
				notInuseCount++;
			}
			else
			{
				inuseCount++;
			}
		}

		int32_t freeCount = 0;
		uint32_t free = _resvFree;
		while(free)
		{
			free = _resvTable[free].arIndex;
			freeCount++;
		}
		NanoAssert( ( freeCount==notInuseCount && inuseCount+notInuseCount==(NJ_MAX_STACK_ENTRY-1) ) );
	}

	void Assembler::registerConsistencyCheck(LIns** resv)
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
					NanoAssert(v);
					int32_t idx = v - _resvTable;
					NanoAssert(idx >= 0 && idx < NJ_MAX_STACK_ENTRY);
					resv[idx]=ins;
					NanoAssertMsg(idx, "MUST have a resource for the instruction for it to have a register assigned to it");
					NanoAssertMsg( v->arIndex==0 || ins==_activation.entry[v->arIndex], "Stack record index mismatch");
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
			if (resvb && (rb = resvb->reg) != UnknownReg)
				allow &= ~rmask(rb);
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
			
	Register Assembler::findRegFor(LIns* i, RegisterMask allow)
	{
		Reservation* resv = getresv(i);
		Register r;

        if (resv && (r=resv->reg) != UnknownReg && (rmask(r) & allow)) {
			return r;
        }

		RegisterMask prefer = hint(i, allow);
		if (!resv) 	
			resv = reserveAlloc(i);

        if ((r=resv->reg) == UnknownReg)
		{
            if (resv->cost == 2 && (allow&SavedRegs))
                prefer = allow&SavedRegs;
			r = resv->reg = registerAlloc(prefer);
			_allocator.addActive(r, i);
			return r;
		}
		else 
		{
			// r not allowed
			resv->reg = UnknownReg;
			_allocator.retire(r);
            if (resv->cost == 2 && (allow&SavedRegs))
                prefer = allow&SavedRegs;
			Register s = resv->reg = registerAlloc(prefer);
			_allocator.addActive(s, i);
            if ((rmask(r) & GpRegs) && (rmask(s) & GpRegs)) {
    			MR(r, s);
            } 
			else
			{
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
		if (!resv->arIndex)
			resv->arIndex = arReserve(i);
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

	void Assembler::freeRsrcOf(LIns *i, bool pop)
	{
		Reservation* resv = getresv(i);
		int index = resv->arIndex;
		Register rr = resv->reg;

		if (rr != UnknownReg)
		{
			asm_spill(i, resv, pop);
			_allocator.retire(rr);	// free any register associated with entry
		}
		arFree(index);			// free any stack stack space associated with entry
		reserveFree(i);		// clear fields of entry and add it to free list
	}

	void Assembler::evict(Register r)
	{
		registerAlloc(rmask(r));
		_allocator.addFree(r);
	}

	void Assembler::asm_cmp(LIns *cond)
	{
        LOpcode condop = cond->opcode();
        
        // LIR_ov and LIR_cs recycle the flags set by arithmetic ops
        if ((condop == LIR_ov) || (condop == LIR_cs))
            return;
        
        LInsp lhs = cond->oprnd1();
		LInsp rhs = cond->oprnd2();
		Reservation *rA, *rB;

		// Not supported yet.
#if !defined NANOJIT_64BIT
		NanoAssert(!lhs->isQuad() && !rhs->isQuad());
#endif

		// ready to issue the compare
		if (rhs->isconst())
		{
			int c = rhs->constval();
			Register r = findRegFor(lhs, GpRegs);
			if (c == 0 && cond->isop(LIR_eq)) {
				if (rhs->isQuad() || lhs->isQuad()) {
#if defined NANOJIT_64BIT
					TESTQ(r, r);
#endif
				} else {
					TEST(r,r);
				}
#if defined NANOJIT_64BIT
			} else if (rhs->isQuad() || lhs->isQuad()) {
                findRegFor2(GpRegs, lhs, rA, rhs, rB);
                Register ra = rA->reg;
                Register rb = rB->reg;
                CMPQ(ra,rb);
#endif
            } else {
				CMPi(r, c);
            }
		}
		else
		{
			findRegFor2(GpRegs, lhs, rA, rhs, rB);
			Register ra = rA->reg;
			Register rb = rB->reg;
			if (rhs->isQuad() || lhs->isQuad()) {
#if defined NANOJIT_64BIT
				CMPQ(ra, rb);
#endif
			} else {
				CMP(ra, rb);
			}
		}
	}

    void Assembler::patch(GuardRecord *lr)
    {
        Fragment *frag = lr->target;
		NanoAssert(frag->fragEntry);
		NIns* was = asm_adjustBranch((NIns*)lr->jmp, frag->fragEntry);
		if (!lr->origTarget) lr->origTarget = was;
		verbose_only(verbose_outputf("patching jump at %p to target %p (was %p)\n",
			lr->jmp, frag->fragEntry, was);)
    }

    void Assembler::unpatch(GuardRecord *lr)
    {
		NIns* was = asm_adjustBranch((NIns*)lr->jmp, (NIns*)lr->origTarget);
		(void)was;
		verbose_only(verbose_outputf("unpatching jump at %p to original target %p (was %p)\n",
			lr->jmp, lr->origTarget, was);)
    }

    NIns* Assembler::asm_exit(LInsp guard)
    {
		SideExit *exit = guard->exit();
		NIns* at = 0;
		if (!_branchStateMap->get(exit))
		{
			at = asm_leave_trace(guard);
		}
		else
		{
			RegAlloc* captured = _branchStateMap->get(exit);
			mergeRegisterState(*captured);
			verbose_only(
				verbose_outputf("        merging trunk with %s",
					_frago->labels->format(exit->target));
				verbose_outputf("        %p:",_nIns);
			)			
			at = exit->target->fragEntry;
			NanoAssert(at);
			_branchStateMap->remove(exit);
		}
		return at;
	}
	
	NIns* Assembler::asm_leave_trace(LInsp guard)
	{
        verbose_only(bool priorVerbose = _verbose; )
		verbose_only( _verbose = verbose_enabled() && _frago->core()->config.verbose_exits; )
        verbose_only( int32_t nativeSave = _stats.native );
		verbose_only(verbose_outputf("--------------------------------------- end exit block SID %d", guard->exit()->sid);)

		RegAlloc capture = _allocator;

        // this point is unreachable.  so free all the registers.
		// if an instruction has a stack entry we will leave it alone,
		// otherwise we free it entirely.  mergeRegisterState will restore.
		releaseRegisters();
		
		swapptrs();
		_inExit = true;
		
		//verbose_only( verbose_outputf("         LIR_xend swapptrs, _nIns is now %08X(%08X), _nExitIns is now %08X(%08X)",_nIns, *_nIns,_nExitIns,*_nExitIns) );
		debug_only( _sv_fpuStkDepth = _fpuStkDepth; _fpuStkDepth = 0; )

		nFragExit(guard);

		// if/when we patch this exit to jump over to another fragment,
		// that fragment will need its parameters set up just like ours.
        LInsp stateins = _thisfrag->lirbuf->state;
		Register state = findSpecificRegFor(stateins, Register(stateins->imm8()));
		asm_bailout(guard, state);

		mergeRegisterState(capture);

		// this can be useful for breaking whenever an exit is taken
		//INT3();
		//NOP();

		// we are done producing the exit logic for the guard so demark where our exit block code begins
		NIns* jmpTarget = _nIns;	 // target in exit path for our mainline conditional jump 

		// swap back pointers, effectively storing the last location used in the exit path
		swapptrs();
		_inExit = false;
		
		//verbose_only( verbose_outputf("         LIR_xt/xf swapptrs, _nIns is now %08X(%08X), _nExitIns is now %08X(%08X)",_nIns, *_nIns,_nExitIns,*_nExitIns) );
		verbose_only( verbose_outputf("        %p:",jmpTarget);)
		verbose_only( verbose_outputf("--------------------------------------- exit block (LIR_xt|LIR_xf)") );

#ifdef NANOJIT_IA32
		NanoAssertMsgf(_fpuStkDepth == _sv_fpuStkDepth, ("LIR_xtf, _fpuStkDepth=%d, expect %d\n",_fpuStkDepth, _sv_fpuStkDepth));
		debug_only( _fpuStkDepth = _sv_fpuStkDepth; _sv_fpuStkDepth = 9999; )
#endif

        verbose_only( _verbose = priorVerbose; )
        verbose_only(_stats.exitnative += (_stats.native-nativeSave));

        return jmpTarget;
    }
	
	bool Assembler::ignoreInstruction(LInsp ins)
	{
        LOpcode op = ins->opcode();
        if (ins->isStore() || op == LIR_loop)
            return false;
	    return getresv(ins) == 0;
	}

	void Assembler::beginAssembly(Fragment* frag, RegAllocMap* branchStateMap)
	{
		_activation.lowwatermark = 1;
		_activation.tos = _activation.lowwatermark;
		_activation.highwatermark = _activation.tos;
        _thisfrag = frag;
		
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
			
        _epilogue = genEpilogue(SavedRegs);
		_branchStateMap = branchStateMap;
		
		verbose_only( verbose_outputf("        %p:",_nIns) );
		verbose_only( verbose_output("        epilogue:") );
	}
	
	void Assembler::assemble(Fragment* frag,  NInsList& loopJumps)
	{
		if (error()) return;	
		AvmCore *core = _frago->core();
		GC *gc = core->gc;
        _thisfrag = frag;

		// set up backwards pipeline: assembler -> StackFilter -> LirReader
		LirReader bufreader(frag->lastIns);
		StackFilter storefilter1(&bufreader, gc, frag, frag->lirbuf->sp);
		StackFilter storefilter2(&storefilter1, gc, frag, frag->lirbuf->rp);
		DeadCodeFilter deadfilter(&storefilter2, this);
		LirFilter* rdr = &deadfilter;
		verbose_only(
			VerboseBlockReader vbr(rdr, this, frag->lirbuf->names);
			if (verbose_enabled())
				rdr = &vbr;
		)

		verbose_only(_thisfrag->compileNbr++; )
		verbose_only(_frago->_stats.compiles++; )
		verbose_only(_frago->_stats.totalCompiles++; )
		_latestGuard = 0;
		_inExit = false;		
		gen(rdr, loopJumps);
		frag->fragEntry = _nIns;
		frag->outbound = core->config.tree_opt? _latestGuard : 0;
		//fprintf(stderr, "assemble frag %X entry %X\n", (int)frag, (int)frag->fragEntry);
	}

	void Assembler::endAssembly(Fragment* frag, NInsList& loopJumps)
	{
		while(!loopJumps.isEmpty())
		{
			NIns* loopJump = (NIns*)loopJumps.removeLast();
			nPatchBranch(loopJump, _nIns);
		}

		NIns* patchEntry = 0;
		if (!error())
		{
			patchEntry = genPrologue(SavedRegs);
			verbose_only( verbose_outputf("        %p:",_nIns); )
			verbose_only( verbose_output("        prologue"); )
		}
		
		// something bad happened?
		if (!error())
		{
			// check for resource leaks 
			debug_only( 
				for(uint32_t i=_activation.lowwatermark;i<_activation.highwatermark; i++) {
					NanoAssertMsgf(_activation.entry[i] == 0, ("frame entry %d wasn't freed\n",-4*i)); 
				}
			)

            frag->fragEntry = patchEntry;
			NIns* code = _nIns;
			
			// let the fragment manage the pages if we're using trees and there are branches
			Page* manage = (_frago->core()->config.tree_opt) ? handoverPages() : 0;
			frag->setCode(code, manage); // root of tree should manage all pages
			NanoAssert(!_frago->core()->config.tree_opt || frag == frag->anchor || frag->kind == MergeTrace);			
			//fprintf(stderr, "endAssembly frag %X entry %X\n", (int)frag, (int)frag->fragEntry);
		}
		
		AvmAssertMsg(error() || _fpuStkDepth == 0, ("_fpuStkDepth %d\n",_fpuStkDepth));

		internalReset();  // clear the reservation tables and regalloc
		NanoAssert(_branchStateMap->isEmpty());
		_branchStateMap = 0;
		
		#if defined(UNDER_CE)
		// If we've modified the code, we need to flush so we don't end up trying 
		// to execute junk
		FlushInstructionCache(GetCurrentProcess(), NULL, NULL);
		#elif defined(AVMPLUS_LINUX) && defined(AVMPLUS_ARM)
			// N A S T Y - obviously have to fix this
		// determine our page range

		Page *page=0, *first=0, *last=0;
		for (int i=2;i!=0;i--) {
			page = first = last = (i==2 ? _nativePages : _nativeExitPages);
			while (page)
			{
				if (page<first)
					first = page;
				if (page>last)
					last = page;
				page = page->next;
			}
	
			register unsigned long _beg __asm("a1") = (unsigned long)(first);
			register unsigned long _end __asm("a2") = (unsigned long)(last+NJ_PAGE_SIZE);
			register unsigned long _flg __asm("a3") = 0;
			register unsigned long _swi __asm("r7") = 0xF0002;
			__asm __volatile ("swi 0 	@ sys_cacheflush" : "=r" (_beg) : "0" (_beg), "r" (_end), "r" (_flg), "r" (_swi));
		}
		#endif
	#ifdef AVMPLUS_PORTING_API
		NanoJIT_PortAPI_FlushInstructionCache(_nIns, _endJit1Addr);
		NanoJIT_PortAPI_FlushInstructionCache(_nExitIns, _endJit2Addr);
	#endif
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
	
	void Assembler::gen(LirFilter* reader,  NInsList& loopJumps)
	{
		// trace must start with LIR_x or LIR_loop
		NanoAssert(reader->pos()->isop(LIR_x) || reader->pos()->isop(LIR_loop));
		 
		for (LInsp ins = reader->read(); ins != 0 && !error(); ins = reader->read())
		{
    		Reservation *rR = getresv(ins);
			LOpcode op = ins->opcode();			
			switch(op)
			{
				default:
					NanoAssertMsg(false, "unsupported LIR instruction");
					break;
					
				case LIR_short:
				case LIR_int:
				{
					Register rr = prepResultReg(ins, GpRegs);
					int32_t val;
					if (op == LIR_int)
						val = ins->imm32();
					else
						val = ins->imm16();
					if (val == 0)
						XOR(rr,rr);
					else
						LDi(rr, val);
					break;
				}
				case LIR_quad:
				{
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
					Register w = Register(ins->imm8());
                    NanoAssert(w != UnknownReg);
					// incoming arg in register
					prepResultReg(ins, rmask(w));
					break;
				}
				case LIR_qlo:
                {
					LIns *q = ins->oprnd1();

					if (!asm_qlo(ins, q))
					{
    					Register rr = prepResultReg(ins, GpRegs);
				        int d = findMemFor(q);
				        LD(rr, d, FP);
                    }
					break;
                }
				case LIR_qhi:
				{
					Register rr = prepResultReg(ins, GpRegs);
					LIns *q = ins->oprnd1();
					int d = findMemFor(q);
				    LD(rr, d+4, FP);
					break;
				}

				case LIR_qcmov:
				case LIR_cmov:
				{
					LIns* condval = ins->oprnd1();
					NanoAssert(condval->isCmp());

					LIns* values = ins->oprnd2();

					NanoAssert(values->opcode() == LIR_2);
					LIns* iftrue = values->oprnd1();
					LIns* iffalse = values->oprnd2();

					NanoAssert(op == LIR_qcmov || (!iftrue->isQuad() && !iffalse->isQuad()));
					
					const Register rr = prepResultReg(ins, GpRegs);

					// this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
					// (This is true on Intel, is it true on all architectures?)
					const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
					if (op == LIR_cmov) {
						switch (condval->opcode())
						{
							// note that these are all opposites...
							case LIR_eq:	MRNE(rr, iffalsereg);	break;
							case LIR_ov:    MRNO(rr, iffalsereg);   break;
							case LIR_cs:    MRNC(rr, iffalsereg);   break;
							case LIR_lt:	MRGE(rr, iffalsereg);	break;
							case LIR_le:	MRG(rr, iffalsereg);	break;
							case LIR_gt:	MRLE(rr, iffalsereg);	break;
							case LIR_ge:	MRL(rr, iffalsereg);	break;
							case LIR_ult:	MRAE(rr, iffalsereg);	break;
							case LIR_ule:	MRA(rr, iffalsereg);	break;
							case LIR_ugt:	MRBE(rr, iffalsereg);	break;
							case LIR_uge:	MRB(rr, iffalsereg);	break;
							debug_only( default: NanoAssert(0); break; )
						}
					} else if (op == LIR_qcmov) {
#if !defined NANOJIT_64BIT
						NanoAssert(0);
#else
						switch (condval->opcode())
						{
							// note that these are all opposites...
							case LIR_eq:	MRQNE(rr, iffalsereg);	break;
							case LIR_ov:    MRQNO(rr, iffalsereg);   break;
							case LIR_cs:    MRQNC(rr, iffalsereg);   break;
							case LIR_lt:	MRQGE(rr, iffalsereg);	break;
							case LIR_le:	MRQG(rr, iffalsereg);	break;
							case LIR_gt:	MRQLE(rr, iffalsereg);	break;
							case LIR_ge:	MRQL(rr, iffalsereg);	break;
							case LIR_ult:	MRQAE(rr, iffalsereg);	break;
							case LIR_ule:	MRQA(rr, iffalsereg);	break;
							case LIR_ugt:	MRQBE(rr, iffalsereg);	break;
							case LIR_uge:	MRQB(rr, iffalsereg);	break;
							debug_only( default: NanoAssert(0); break; )
						}
#endif
					}
					/*const Register iftruereg =*/ findSpecificRegFor(iftrue, rr);
					asm_cmp(condval);
					break;
				}

				case LIR_ld:
				case LIR_ldc:
				case LIR_ldcb:
				{
					LIns* base = ins->oprnd1();
					LIns* disp = ins->oprnd2();
					Register rr = prepResultReg(ins, GpRegs);
					Register ra = findRegFor(base, GpRegs);
					int d = disp->constval();
					if (op == LIR_ldcb)
						LD8Z(rr, d, ra);
					else
						LD(rr, d, ra); 
					break;
				}

				case LIR_ldq:
				{
					asm_load64(ins);
					break;
				}

				case LIR_neg:
				case LIR_not:
				{
					Register rr = prepResultReg(ins, GpRegs);

					LIns* lhs = ins->oprnd1();
					Reservation *rA = getresv(lhs);
					// if this is last use of lhs in reg, we can re-use result reg
					Register ra;
					if (rA == 0 || (ra=rA->reg) == UnknownReg)
						ra = findSpecificRegFor(lhs, rr);
					// else, rA already has a register assigned.

					if (op == LIR_not)
						NOT(rr); 
					else
						NEG(rr); 

					if ( rr != ra ) 
						MR(rr,ra); 
					break;
				}
				
				case LIR_qjoin:
				{
                    asm_qjoin(ins);
					break;
				}

#if defined NANOJIT_64BIT
                case LIR_qiadd:
                case LIR_qiand:
                case LIR_qilsh:
                {
                    asm_qbinop(ins);
                    break;
                }
#endif

				case LIR_add:
				case LIR_sub:
				case LIR_mul:
				case LIR_and:
				case LIR_or:
				case LIR_xor:
				case LIR_lsh:
				case LIR_rsh:
				case LIR_ush:
				{
                    LInsp lhs = ins->oprnd1();
                    LInsp rhs = ins->oprnd2();

					Register rb = UnknownReg;
					RegisterMask allow = GpRegs;
					if (lhs != rhs && (op == LIR_mul || !rhs->isconst()))
					{
						if ((rb = asm_binop_rhs_reg(ins)) == UnknownReg) {
							rb = findRegFor(rhs, allow);
						}
						allow &= ~rmask(rb);
					}

					Register rr = prepResultReg(ins, allow);
					Reservation* rA = getresv(lhs);
					Register ra;
					// if this is last use of lhs in reg, we can re-use result reg
					if (rA == 0 || (ra = rA->reg) == UnknownReg)
						ra = findSpecificRegFor(lhs, rr);
					// else, rA already has a register assigned.

					if (!rhs->isconst() || op == LIR_mul)
					{
						if (lhs == rhs)
							rb = ra;

						if (op == LIR_add)
							ADD(rr, rb);
						else if (op == LIR_sub)
							SUB(rr, rb);
						else if (op == LIR_mul)
							MUL(rr, rb);
						else if (op == LIR_and)
							AND(rr, rb);
						else if (op == LIR_or)
							OR(rr, rb);
						else if (op == LIR_xor)
							XOR(rr, rb);
						else if (op == LIR_lsh)
							SHL(rr, rb);
						else if (op == LIR_rsh)
							SAR(rr, rb);
						else if (op == LIR_ush)
							SHR(rr, rb);
						else
							NanoAssertMsg(0, "Unsupported");
					}
					else
					{
						int c = rhs->constval();
						if (op == LIR_add) {
#ifdef NANOJIT_IA32
							if (ra != rr) {
								LEA(rr, c, ra);
								ra = rr; // suppress mov
							} else
#endif
							{
								ADDi(rr, c); 
							}
						} else if (op == LIR_sub) {
#ifdef NANOJIT_IA32
							if (ra != rr) {
								LEA(rr, -c, ra);
								ra = rr;
							} else
#endif
							{
								SUBi(rr, c); 
							}
						} else if (op == LIR_and)
							ANDi(rr, c);
						else if (op == LIR_or)
							ORi(rr, c);
						else if (op == LIR_xor)
							XORi(rr, c);
						else if (op == LIR_lsh)
							SHLi(rr, c);
						else if (op == LIR_rsh)
							SARi(rr, c);
						else if (op == LIR_ush)
							SHRi(rr, c);
						else
							NanoAssertMsg(0, "Unsupported");
					}

					if ( rr != ra ) 
						MR(rr,ra);
					break;
				}
#ifndef NJ_SOFTFLOAT
				case LIR_fneg:
				{
					asm_fneg(ins);
					break;
				}
				case LIR_fadd:
				case LIR_fsub:
				case LIR_fmul:
				case LIR_fdiv:
				{
					asm_fop(ins);
                    break;
				}
				case LIR_i2f:
				{
					asm_i2f(ins);
					break;
				}
				case LIR_u2f:
				{
					asm_u2f(ins);
					break;
				}
#endif // NJ_SOFTFLOAT
				case LIR_st:
				case LIR_sti:
				{
                    asm_store32(ins->oprnd1(), ins->immdisp(), ins->oprnd2());
                    break;
				}
				case LIR_stq:
				case LIR_stqi:
				{
					LIns* value = ins->oprnd1();
					LIns* base = ins->oprnd2();
					int dr = ins->immdisp();
					if (value->isop(LIR_qjoin)) {
						// this is correct for little-endian only
						asm_store32(value->oprnd1(), dr, base);
						asm_store32(value->oprnd2(), dr+4, base);
					}
					else {
						asm_store64(value, dr, base);
					}
                    break;
				}
				case LIR_xt:
				case LIR_xf:
				{
                    NIns* exit = asm_exit(ins);
	
					// we only support cmp with guard right now, also assume it is 'close' and only emit the branch
					LIns* cond = ins->oprnd1();
					LOpcode condop = cond->opcode();
					NanoAssert(cond->isCond());
#ifndef NJ_SOFTFLOAT
                    if (condop >= LIR_feq && condop <= LIR_fge)
					{
						if (op == LIR_xf)
							JP(exit);
						else
							JNP(exit);
						asm_fcmp(cond);
                        break;
					}
#endif
					// produce the branch
					if (op == LIR_xf)
					{
						if (condop == LIR_eq)
							JNE(exit);
                        else if (condop == LIR_ov)
                            JNO(exit);
                        else if (condop == LIR_cs)
                            JNC(exit);
						else if (condop == LIR_lt)
							JNL(exit);
						else if (condop == LIR_le)
							JNLE(exit);
						else if (condop == LIR_gt)
							JNG(exit);
						else if (condop == LIR_ge)
							JNGE(exit);
						else if (condop == LIR_ult)
							JNB(exit);
						else if (condop == LIR_ule)
							JNBE(exit);
						else if (condop == LIR_ugt)
							JNA(exit);
						else //if (condop == LIR_uge)
							JNAE(exit);
					}
					else // op == LIR_xt
					{
						if (condop == LIR_eq)
							JE(exit);
                        else if (condop == LIR_ov)
                            JO(exit);
                        else if (condop == LIR_cs)
                            JC(exit);
						else if (condop == LIR_lt)
							JL(exit);
						else if (condop == LIR_le)
							JLE(exit);
						else if (condop == LIR_gt)
							JG(exit);
						else if (condop == LIR_ge)
							JGE(exit);
						else if (condop == LIR_ult)
							JB(exit);
						else if (condop == LIR_ule)
							JBE(exit);
						else if (condop == LIR_ugt)
							JA(exit);
						else //if (condop == LIR_uge)
							JAE(exit);
					}
					asm_cmp(cond);
					break;
				}
				case LIR_x:
				{
		            verbose_only(verbose_output(""));
					// generate the side exit branch on the main trace.
                    NIns *exit = asm_exit(ins);
					JMP( exit ); 
					break;
				}
				case LIR_loop:
				{
					JMP_long_placeholder(); // jump to SOT	
					verbose_only( if (_verbose && _outputCache) { _outputCache->removeLast(); outputf("         jmp   SOT"); } );
					
					loopJumps.add(_nIns);

                    #ifdef NJ_VERBOSE
                    // branching from this frag to ourself.
                    if (_frago->core()->config.show_stats)
					#if defined NANOJIT_64BIT
                        LDQi(argRegs[1], intptr_t((Fragment*)_thisfrag));
					#else
                        LDi(argRegs[1], int((Fragment*)_thisfrag));
                    #endif
                    #endif

					// restore first parameter, the only one we use
                    LInsp state = _thisfrag->lirbuf->state;
                    Register a0 = Register(state->imm8());
					findSpecificRegFor(state, a0); 
					break;
				}
#ifndef NJ_SOFTFLOAT
				case LIR_feq:
				case LIR_fle:
				case LIR_flt:
				case LIR_fgt:
				case LIR_fge:
				{
					// only want certain regs 
					Register r = prepResultReg(ins, AllowableFlagRegs);
					// SETcc only sets low 8 bits, so extend 
					MOVZX8(r,r);
					SETNP(r);
					asm_fcmp(ins);
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
					// only want certain regs 
					Register r = prepResultReg(ins, AllowableFlagRegs);
					// SETcc only sets low 8 bits, so extend 
					MOVZX8(r,r);
					if (op == LIR_eq)
						SETE(r);
                    else if (op == LIR_ov)
                        SETO(r);
                    else if (op == LIR_cs)
                        SETC(r);
					else if (op == LIR_lt)
						SETL(r);
					else if (op == LIR_le)
						SETLE(r);
					else if (op == LIR_gt)
						SETG(r);
					else if (op == LIR_ge)
						SETGE(r);
					else if (op == LIR_ult)
						SETB(r);
					else if (op == LIR_ule)
						SETBE(r);
					else if (op == LIR_ugt)
						SETA(r);
					else // if (op == LIR_uge)
						SETAE(r);
					asm_cmp(ins);
					break;
				}

#ifndef NJ_SOFTFLOAT
				case LIR_fcall:
#endif
#if defined NANOJIT_64BIT
				case LIR_callh:
#endif
				case LIR_call:
				{
                    Register rr = UnknownReg;
#ifndef NJ_SOFTFLOAT
                    if (op == LIR_fcall)
                    {
						rr = asm_prep_fcall(rR, ins);
                    }
                    else
#endif
                    {
						(void)rR;
                        rr = retRegs[0];
						prepResultReg(ins, rmask(rr));
                    }

					// do this after we've handled the call result, so we dont
					// force the call result to be spilled unnecessarily.
					restoreCallerSaved();

					asm_call(ins);
				}
			}

			// check that all is well (don't check in exit paths since its more complicated)
			debug_only( pageValidate(); )
			debug_only( resourceConsistencyCheck();  )
		}
	}

    void Assembler::asm_arg(ArgSize sz, LInsp p, Register r)
    {
        if (sz == ARGSIZE_Q) 
        {
			// ref arg - use lea
			if (r != UnknownReg)
			{
				// arg in specific reg
				int da = findMemFor(p);
				LEA(r, da, FP);
			}
			else
			{
				NanoAssert(0); // not supported
			}
		}
        else if (sz == ARGSIZE_LO)
		{
			if (r != UnknownReg)
			{
				// arg goes in specific register
				if (p->isconst())
					LDi(r, p->constval());
				else
					findSpecificRegFor(p, r);
			}
			else
			{
				asm_pusharg(p);
			}
		}
        else
		{
			asm_farg(p);
		}
    }

	uint32_t Assembler::arFree(uint32_t idx)
	{
		if (idx > 0 && _activation.entry[idx] == _activation.entry[idx+stack_direction(1)])
			_activation.entry[idx+stack_direction(1)] = 0;  // clear 2 slots for doubles 
		_activation.entry[idx] = 0;
		return 0;
	}

#ifdef NJ_VERBOSE
	void Assembler::printActivationState()
	{
		bool verbose_activation = false;
		if (!verbose_activation)
			return;
			
#ifdef NANOJIT_ARM
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
	
	uint32_t Assembler::arReserve(LIns* l)
	{
		NanoAssert(!l->isTramp());

		//verbose_only(printActivationState());
		const bool quad = l->isQuad();
		const int32_t n = _activation.tos;
		int32_t start = _activation.lowwatermark;
		int32_t i = 0;
		NanoAssert(start>0);
		if (n >= NJ_MAX_STACK_ENTRY-2)
		{	
			setError(StackFull);
			return start;
		}
		else if (quad)
		{
			if ( (start&1)==1 ) start++;  // even 
			for(i=start; i <= n; i+=2)
			{
				if ( (_activation.entry[i+stack_direction(1)] == 0) && (i==n || (_activation.entry[i] == 0)) )
					break;   //  for fp we need 2 adjacent aligned slots
			}
		}
		else
		{
			for(i=start; i < n; i++)
			{
				if (_activation.entry[i] == 0)
					break;   // not being used
			}
		}

		int32_t inc = ((i-n+1) < 0) ? 0 : (i-n+1);
		if (quad && stack_direction(1)>0) inc++;
		_activation.tos += inc;
		_activation.highwatermark += inc;

		// place the entry in the table and mark the instruction with it
		_activation.entry[i] = l;
		if (quad) _activation.entry[i+stack_direction(1)] = l;
		return i;
	}

	void Assembler::restoreCallerSaved()
	{
		// generate code to restore callee saved registers 
		// @todo speed this up
		RegisterMask scratch = ~SavedRegs;
		for (Register r = FirstReg; r <= LastReg; r = nextreg(r))
		{
			if ((rmask(r) & scratch) && _allocator.getActive(r))
            {
				evict(r);
            }
		}
	}
	
	/**
	 * Merge the current state of the registers with a previously stored version
	 */
	void Assembler::mergeRegisterState(RegAlloc& saved)
	{
		// evictions and pops first
		RegisterMask skip = 0;
		for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
		{
			LIns * curins = _allocator.getActive(r);
			LIns * savedins = saved.getActive(r);
			if (curins == savedins)
			{
				verbose_only( if (curins) 
					verbose_outputf("        skip %s", regNames[r]); )
				skip |= rmask(r);
			}
			else 
			{
				if (curins)
					evict(r);
				
    			#ifdef NANOJIT_IA32
				if (savedins && (rmask(r) & x87Regs))
					FSTP(r);
				#endif
			}
		}

		// now reassign mainline registers
		for (Register r=FirstReg; r <= LastReg; r = nextreg(r))
		{
			LIns *i = saved.getActive(r);
			if (i && !(skip&rmask(r)))
				findSpecificRegFor(i, r);
		}
		debug_only(saved.used = 0);  // marker that we are no longer in exit path
	}
	
	/**																 
	 * Guard records are laid out in the exit block buffer (_nInsExit),
	 * intersperced with the code.   Preceding the record are the native
	 * instructions associated with the record (i.e. the exit code).
	 * 
	 * The layout is as follows:
	 * 
	 * [ native code ] [ GuardRecord1 ]
	 * ...
	 * [ native code ] [ GuardRecordN ]
	 * 
	 * The guard record 'code' field should be used to locate 
	 * the start of the native code associated with the
	 * exit block. N.B the code may lie in a different page 
	 * than the guard record  
	 * 
	 * The last guard record is used for the unconditional jump
	 * at the end of the trace. 
	 * 
	 * NOTE:  It is also not guaranteed that the native code 
	 *        is contained on a single page.
	 */
	GuardRecord* Assembler::placeGuardRecord(LInsp guard)
	{
		// we align the guards to 4Byte boundary
		size_t size = GuardRecordSize(guard);
		SideExit *exit = guard->exit();
		NIns* ptr = (NIns*)alignTo(_nIns-size, 4);
		underrunProtect( (intptr_t)_nIns-(intptr_t)ptr );  // either got us a new page or there is enough space for us
		GuardRecord* rec = (GuardRecord*) alignTo(_nIns-size,4);
		rec->outgoing = _latestGuard;
		_latestGuard = rec;
		_nIns = (NIns*)rec;
		rec->next = 0;
		rec->origTarget = 0;		
		rec->target = exit->target;
		rec->from = _thisfrag;
		initGuardRecord(guard,rec);
		if (exit->target) 
			exit->target->addLink(rec);
		return rec;
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
		for (int i = 0; i < 5; ++i)
		{
			argt >>= 2;
			argc += (argt & mask) != 0;
		}
		return argc;
	}

    uint32_t CallInfo::get_sizes(ArgSize* sizes) const
    {
		uint32_t argt = _argtypes;
		uint32_t argc = 0;
		for (int32_t i = 0; i < 5; i++) {
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
        return argc;
    }
#endif
}
