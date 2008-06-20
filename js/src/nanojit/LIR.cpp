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
#include <stdio.h>

namespace nanojit
{
    using namespace avmplus;
	#ifdef FEATURE_NANOJIT

	// @todo -- a lookup table would be better here
	uint32_t FASTCALL operandCount(LOpcode op)
	{
		switch(op)
		{
			case LIR_trace:
			case LIR_skip:
			case LIR_tramp:
			case LIR_loop:
			case LIR_x:
			case LIR_short:
			case LIR_int:
			case LIR_quad:
			case LIR_call:
			case LIR_fcall:
            case LIR_param:
                return 0;

            case LIR_callh:
			case LIR_arg:
			case LIR_ref:
			case LIR_farg:
			case LIR_not:
			case LIR_xt:
			case LIR_xf:
			case LIR_qlo:
			case LIR_qhi:
			case LIR_neg:
			case LIR_fneg:
			case LIR_i2f:
			case LIR_u2f:
                return 1;
				
			default:
                return 2;
		}
	}				

	// LIR verbose specific
	#ifdef NJ_VERBOSE

	void Lir::initEngine()
	{
		debug_only( { LIns l; l.initOpcode(LIR_last); NanoAssert(l.opcode()>0); } );
		NanoAssert( LIR_last < (1<<8)-1 );  // only 8 bits or the opcode 
		verbose_only( initVerboseStructures() );
	}

		const char* Lir::_lirNames[LIR_last+1];

		void Lir::initVerboseStructures()
		{
			memset(_lirNames, 0, sizeof(_lirNames));

			_lirNames[LIR_short] = "short";
			_lirNames[LIR_int] =  "int";
			_lirNames[LIR_quad]  =  "quad";
			_lirNames[LIR_trace] =  "trace";
			_lirNames[LIR_skip]  =  "skip";
			_lirNames[LIR_tramp] =  "tramp";
			_lirNames[LIR_loop] =	"loop";
			_lirNames[LIR_x]	=	"x";
			_lirNames[LIR_xt]	=	"xt";
			_lirNames[LIR_xf]	=	"xf";
			_lirNames[LIR_eq]   =   "eq";
			_lirNames[LIR_lt]   =   "lt";
			_lirNames[LIR_le]   =   "le";
			_lirNames[LIR_gt]   =   "gt";
			_lirNames[LIR_ge]   =   "ge";
			_lirNames[LIR_ult]   =  "ult";
			_lirNames[LIR_ule]   =  "ule";
			_lirNames[LIR_ugt]   =  "ugt";
			_lirNames[LIR_uge]   =  "uge";
			_lirNames[LIR_neg] =    "neg";
			_lirNames[LIR_add] =	"add";
			_lirNames[LIR_sub] =	"sub";
			_lirNames[LIR_mul] =	"mul";
			_lirNames[LIR_and] =	"and";
			_lirNames[LIR_or] =		"or";
			_lirNames[LIR_xor] =	"xor";
			_lirNames[LIR_not] =	"not";
			_lirNames[LIR_lsh] =	"lsh";
			_lirNames[LIR_rsh] =	"rsh";
			_lirNames[LIR_ush] =	"ush";
			_lirNames[LIR_fneg] =   "fneg";
			_lirNames[LIR_fadd] =	"fadd";
			_lirNames[LIR_fsub] =	"fsub";
			_lirNames[LIR_fmul] =	"fmul";
			_lirNames[LIR_fdiv] =	"fdiv";
			_lirNames[LIR_i2f] =	"i2f";
			_lirNames[LIR_u2f] =	"u2f";
			_lirNames[LIR_ld] =		"ld";
			_lirNames[LIR_ldc] =	"ldc";
			_lirNames[LIR_ldcb] =	"ldcb";
			_lirNames[LIR_cmov] =	"cmov";
            _lirNames[LIR_2] =      "";
			_lirNames[LIR_ldq] =	"ldq";
			_lirNames[LIR_st] =		"st";
			_lirNames[LIR_sti] =	"sti";
			_lirNames[LIR_arg] =	"arg";
			_lirNames[LIR_param] =	"param";
			_lirNames[LIR_call] =	"call";
			_lirNames[LIR_callh] =	"callh";
			_lirNames[LIR_qjoin] =	"qjoin";
			_lirNames[LIR_qlo] =	"qlo";
			_lirNames[LIR_qhi] =	"qhi";
			_lirNames[LIR_ref] =	"ref";
			_lirNames[LIR_last]=	"???";
			_lirNames[LIR_farg] =	"farg";
			_lirNames[LIR_fcall] =	"fcall";
		}
	#endif /* NANOJIT_VEBROSE */
	
	// implementation

#ifdef NJ_PROFILE
	// @todo fixup move to nanojit.h
	#undef counter_value
	#define counter_value(x)		x
#endif /* NJ_PROFILE */

	//static int32_t buffer_count = 0;
	
	// LCompressedBuffer
	LirBuffer::LirBuffer(Fragmento* frago, const CallInfo* functions)
		: _frago(frago), _functions(functions)
	{
		_start = 0;
		clear();
		_start = pageAlloc();
		if (_start)
		{
			verbose_only(_start->seq = 0;)
			_unused = &_start->lir[0];
		}
		//buffer_count++;
		//fprintf(stderr, "LirBuffer %x start %x count %d\n", (int)this, (int)_start, buffer_count);
	}

	LirBuffer::~LirBuffer()
	{
		//buffer_count--;
		//fprintf(stderr, "~LirBuffer %x count %d\n", (int)this, buffer_count);
		clear();
		_frago = 0;
	}
	
	void LirBuffer::clear()
	{
		// free all the memory and clear the stats
		debug_only( if (_start) validate();)
		while( _start )
		{
			Page *next = _start->next;
			_frago->pageFree( _start );
			_start = next;
			_stats.pages--;
		}
		NanoAssert(_stats.pages == 0);
		_unused = 0;
		_stats.lir = 0;
		_noMem = 0;
	}

	#ifdef _DEBUG
	void LirBuffer::validate() const
	{
		uint32_t count = 0;
		Page *last = 0;
		Page *page = _start;
		while(page)
		{
			last = page;
			page = page->next;
			count++;
		}
		NanoAssert(count == _stats.pages);
		NanoAssert(_noMem || _unused->page()->next == 0);
		NanoAssert(_noMem || samepage(last,_unused));
	}
	#endif 
	
	Page* LirBuffer::pageAlloc()
	{
		Page* page = _frago->pageAlloc();
		if (page)
		{
			page->next = 0;	// end of list marker for new page
			_stats.pages++;
		}
		else
		{
			_noMem = 1;
		}
		return page;
	}
	
	uint32_t LirBuffer::size()
	{
		debug_only( validate(); )
		return _stats.lir;
	}
	
	LInsp LirBuffer::next()
	{
		debug_only( validate(); )
		return _unused;
	}

	bool LirBuffer::addPage()
	{
		LInsp last = _unused;
		// we need to pull in a new page and stamp the old one with a link to it
        Page *lastPage = last->page();
		Page *page = pageAlloc();
		if (page)
		{
			lastPage->next = page;  // forward link to next page 
			_unused = &page->lir[0];
            verbose_only(page->seq = lastPage->seq+1;)
			//fprintf(stderr, "Fragmento::ensureRoom stamping %x with %x; start %x unused %x\n", (int)pageBottom(last), (int)page, (int)_start, (int)_unused);
			debug_only( validate(); )
			return true;
		} 
		else {
			// mem failure, rewind pointer to top of page so that subsequent instruction works
			verbose_only(if (_frago->assm()->_verbose) _frago->assm()->outputf("page alloc failed");)
			_unused = &lastPage->lir[0];
		}
		return false;
	}
	
	bool LirBufWriter::ensureRoom(uint32_t count)
	{
		LInsp last = _buf->next();
		if (!samepage(last,last+count)
			&& _buf->addPage()) 
		{
			// link LIR stream back to prior instruction (careful insFar relies on _unused...)
			LInsp next = _buf->next();
			insFar(LIR_skip, last-1-next);
		}
		return !_buf->outOmem();
	}

	LInsp LirBuffer::commit(uint32_t count)
	{
		debug_only(validate();)
		NanoAssertMsg( samepage(_unused, _unused+count), "You need to call ensureRoom first!" );
		return _unused += count;
	}
	
	uint32_t LIns::reference(LIns *r)
	{
		int delta = this-r-1;
		NanoAssert(isU8(delta));
		return delta;
	}

	LInsp LirBufWriter::ensureReferenceable(LInsp i, int32_t addedDistance)
	{
		if (!i) return 0;
		NanoAssert(!i->isop(LIR_tramp));
		LInsp next = _buf->next();
		LInsp from = next + addedDistance;
		if ( canReference(from,i) )
			return i;

		// need a trampoline to get to i
		LInsp tramp = insFar(LIR_tramp, i-next);
		NanoAssert( tramp+tramp->imm24() == i );
		return tramp;
	}
	
	LInsp LirBufWriter::insStore(LInsp o1, LInsp o2, LInsp o3)
	{
		LOpcode op = LIR_st;
		NanoAssert(o1 && o2 && o3);
		ensureRoom(4);
		LInsp r1 = ensureReferenceable(o1,3);
		LInsp r2 = ensureReferenceable(o2,2);
		LInsp r3 = ensureReferenceable(o3,1);

		LInsp l = _buf->next();
		l->initOpcode(op);
		l->setOprnd1(r1);
		l->setOprnd2(r2);
		l->setOprnd3(r3);

		_buf->commit(1);
		_buf->_stats.lir++;
		return l;
	}
	
	LInsp LirBufWriter::insStorei(LInsp o1, LInsp o2, int32_t d)
	{
		LOpcode op = LIR_sti;
		NanoAssert(o1 && o2 && isS8(d));
		ensureRoom(3);
		LInsp r1 = ensureReferenceable(o1,2);
		LInsp r2 = ensureReferenceable(o2,1);

		LInsp l = _buf->next();
		l->initOpcode(op);
		l->setOprnd1(r1);
		l->setOprnd2(r2);
		l->setDisp(int8_t(d));

		_buf->commit(1);
		_buf->_stats.lir++;
		return l;
	}

	LInsp LirBufWriter::ins0(LOpcode op)
	{
		if (!ensureRoom(1)) return 0;
		LInsp l = _buf->next();
		l->initOpcode(op);
		_buf->commit(1);
		_buf->_stats.lir++;
		return l;
	}
	
	LInsp LirBufWriter::ins1(LOpcode op, LInsp o1)
	{
		ensureRoom(2);
		LInsp r1 = ensureReferenceable(o1,1);

		LInsp l = _buf->next();
		l->initOpcode(op);
		if (r1)
			l->setOprnd1(r1);

		_buf->commit(1);
		_buf->_stats.lir++;
		return l;
	}
	
	LInsp LirBufWriter::ins2(LOpcode op, LInsp o1, LInsp o2)
	{
		ensureRoom(3);
		LInsp r1 = ensureReferenceable(o1,2);
		LInsp r2 = ensureReferenceable(o2,1);

		LInsp l = _buf->next();
		l->initOpcode(op);
		if (r1)
			l->setOprnd1(r1);
		if (r2)
			l->setOprnd2(r2);

		_buf->commit(1);
		_buf->_stats.lir++;
		return l;
	}

	LInsp LirBufWriter::insLoad(LOpcode op, LInsp base, LInsp d)
	{
		return ins2(op,base,d);
	}

	LInsp LirBufWriter::insGuard(LOpcode op, LInsp c, SideExit *x)
	{
		LInsp data = skip(sizeof(SideExit));
		*((SideExit*)data->payload()) = *x;
		return ins2(op, c, data);
	}

	LInsp LirBufWriter::insImm8(LOpcode op, int32_t a, int32_t b)
	{
		ensureRoom(1);
		LInsp l = _buf->next();
		l->initOpcode(op);
		l->setimm8(a,b);

		_buf->commit(1);
		_buf->_stats.lir++;
		return l;
	}
	
	LInsp LirBufWriter::insFar(LOpcode op, int32_t imm)
	{
		ensureRoom(1);

		LInsp l = _buf->next();
		l->initOpcode(op);
		l->setimm24(imm);

		_buf->commit(1);
		return l;
	}
	
	LInsp LirBufWriter::insImm(int32_t imm)
	{
		if (isS16(imm)) {
			ensureRoom(1);
			LInsp l = _buf->next();
			l->initOpcode(LIR_short);
			l->setimm16(imm);
			_buf->commit(1);
			_buf->_stats.lir++;
			return l;
		} else {
			ensureRoom(2);
			int32_t* l = (int32_t*)_buf->next();
			*l = imm;
			_buf->commit(1);
			return ins0(LIR_int);
		}
	}
	
	LInsp LirBufWriter::insImmq(uint64_t imm)
	{
		ensureRoom(3);
		int32_t* l = (int32_t*)_buf->next();
		l[0] = int32_t(imm);
		l[1] = int32_t(imm>>32);
		_buf->commit(2);	
		return ins0(LIR_quad);
	}

	LInsp LirBufWriter::skip(size_t size)
	{
        const uint32_t n = (size+sizeof(LIns)-1)/sizeof(LIns);
		ensureRoom(n+1);
		LInsp i = _buf->next();
		_buf->commit(n);
		return insFar(LIR_skip, i-1-_buf->next());
	}

	LInsp LirReader::read()	
	{
		LInsp cur = _i;
		if (!cur)
			return 0;
		LIns* i = cur;
		LOpcode iop = i->opcode();
		do
		{
			switch (iop)
			{					
				default:
					i--;
					break;

				case LIR_skip:
					NanoAssert(i->imm24() != 0);
					i += i->imm24();
					break;
		
				case LIR_int:
					NanoAssert(samepage(i, i-2));
					i -= 2;
					break;

				case LIR_quad:
					NanoAssert(samepage(i,i-3));
					i -= 3;
					break;

				case LIR_trace:
					_i = 0;  // start of trace
					return cur;
			}
			iop = i->opcode();
		}
		while (is_trace_skip_tramp(iop)||iop==LIR_2);
		_i = i;
		return cur;
	}

	bool FASTCALL isCmp(LOpcode c) {
		return c >= LIR_eq && c <= LIR_uge;
	}

	bool LIns::isCmp() const {
		return nanojit::isCmp(u.code);
	}

	bool LIns::isCall() const
	{
		return (u.code&~LIR64) == LIR_call;
	}

	bool LIns::isGuard() const
	{
		return u.code==LIR_x || u.code==LIR_xf || u.code==LIR_xt;
	}

    bool LIns::isStore() const
    {
        return u.code == LIR_st || u.code == LIR_sti;
    }

    bool LIns::isLoad() const
    {
        return u.code == LIR_ldq || u.code == LIR_ld || u.code == LIR_ldc;
    }

	bool LIns::isconst() const
	{
		return (opcode()&~1) == LIR_short;
	}

	bool LIns::isconstval(int32_t val) const
	{
		return isconst() && constval()==val;
	}

	bool LIns::isconstq() const
	{	
		return isop(LIR_quad);
	}

	bool FASTCALL isCse(LOpcode op) {
		op = LOpcode(op & ~LIR64);
		return op >= LIR_cmov && op <= LIR_uge;
	}

    bool LIns::isCse(const CallInfo *functions) const
    { 
		return nanojit::isCse(u.code) || isCall() && functions[imm8()]._cse;
    }

	void LIns::setimm8(int32_t a, int32_t b)
	{
		NanoAssert(isS8(a) && isS8(b));
		c.imm8a = int8_t(a);
		c.imm8b = int8_t(b);
	}

	void LIns::setimm16(int32_t x)
	{
		NanoAssert(isS16(x));
		i.imm16 = int16_t(x);
	}

	void LIns::setimm24(int32_t x)
	{
		t.imm24 = x;
	}

	void LIns::setresv(uint32_t resv)
	{
		NanoAssert(isU8(resv));
		g.resv = resv;
	}

	void LIns::initOpcode(LOpcode op)
	{
		t.code = op;
		t.imm24 = 0;
	}

	void LIns::setOprnd1(LInsp r)
	{
		u.oprnd_1 = reference(r);
	}

	void LIns::setOprnd2(LInsp r)
	{
		u.oprnd_2 = reference(r);
	}

	void LIns::setOprnd3(LInsp r)
	{
		u.oprnd_3 = reference(r);
	}

    void LIns::setDisp(int8_t d)
    {
        sti.disp = d;
    }

	LInsp	LIns::oprnd1() const	
	{ 
		LInsp i = (LInsp) this - u.oprnd_1 - 1;
		if (i->isop(LIR_tramp)) 
		{
			i += i->imm24();
			if (i->isop(LIR_tramp))
				i += i->imm24();
		}
		return i;
	}
	
	LInsp	LIns::oprnd2() const
	{ 
		LInsp i = (LInsp) this - u.oprnd_2 - 1;
		if (i->isop(LIR_tramp)) 
		{
			i += i->imm24();
			if (i->isop(LIR_tramp))
				i += i->imm24();
		}
		return i;
	}

	LInsp	LIns::oprnd3() const
	{ 
		LInsp i = (LInsp) this - u.oprnd_3 - 1;
		if (i->isop(LIR_tramp)) 
		{
			i += i->imm24();
			if (i->isop(LIR_tramp))
				i += i->imm24();
		}
		return i;
	}

    void *LIns::payload() const
    {
        NanoAssert(opcode() == LIR_skip);
        return (void*) (this+imm24()+1);
    }

    LIns* LirWriter::ins2i(LOpcode v, LIns* oprnd1, int32_t imm)
    {
        return ins2(v, oprnd1, insImm(imm));
    }

    bool insIsS16(LInsp i)
    {
        if (i->isconst()) {
            int c = i->constval();
            return isS16(c);
        }
        if (i->isop(LIR_cmov)) {
            LInsp vals = i->oprnd2();
            return insIsS16(vals->oprnd1()) && insIsS16(vals->oprnd2());
        }
        if (i->isCmp())
            return true;
        // many other possibilities too.
        return false;
    }

	LIns* ExprFilter::ins1(LOpcode v, LIns* i)
	{
		if (v == LIR_qlo) {
			if (i->isconstq())
				return insImm(int32_t(i->constvalq()));
			if (i->isop(LIR_qjoin))
				return i->oprnd1();
		}
		else if (v == LIR_qhi) {
			if (i->isconstq())
				return insImm(int32_t(i->constvalq()>>32));
			if (i->isop(LIR_qjoin))
				return i->oprnd2();
		}
		else if (v == i->opcode() && (v == LIR_not || v == LIR_neg || v == LIR_fneg)) {
			return i->oprnd1();
		}

		// todo
		// -(a-b) = b-a

		return out->ins1(v, i);
	}

	LIns* ExprFilter::ins2(LOpcode v, LIns* oprnd1, LIns* oprnd2)
	{
		NanoAssert(oprnd1 && oprnd2);
		if (v == LIR_cmov) {
			if (oprnd2->oprnd1() == oprnd2->oprnd2()) {
				// c ? a : a => a
				return oprnd2->oprnd1();
			}
		}
		if (oprnd1 == oprnd2)
		{
			if (v == LIR_xor || v == LIR_sub ||
				!oprnd1->isQuad() && (v == LIR_ult || v == LIR_ugt || v == LIR_gt || v == LIR_lt))
				return insImm(0);
			if (v == LIR_or || v == LIR_and)
				return oprnd1;
			if (!oprnd1->isQuad() && (v == LIR_le || v == LIR_ule || v == LIR_ge || v == LIR_uge)) {
				// x <= x == 1; x >= x == 1
				return insImm(1);
			}
		}
		if (oprnd1->isconst() && oprnd2->isconst())
		{
			int c1 = oprnd1->constval();
			int c2 = oprnd2->constval();
			if (v == LIR_qjoin) {
				uint64_t q = c1 | uint64_t(c2)<<32;
				return insImmq(q);
			}
			if (v == LIR_eq)
				return insImm(c1 == c2);
			if (v == LIR_lt)
				return insImm(c1 < c2);
			if (v == LIR_gt)
				return insImm(c1 > c2);
			if (v == LIR_le)
				return insImm(c1 <= c2);
			if (v == LIR_ge)
				return insImm(c1 >= c2);
			if (v == LIR_ult)
				return insImm(uint32_t(c1) < uint32_t(c2));
			if (v == LIR_ugt)
				return insImm(uint32_t(c1) > uint32_t(c2));
			if (v == LIR_ule)
				return insImm(uint32_t(c1) <= uint32_t(c2));
			if (v == LIR_uge)
				return insImm(uint32_t(c1) >= uint32_t(c2));
			if (v == LIR_rsh)
				return insImm(int32_t(c1) >> int32_t(c2));
			if (v == LIR_lsh)
				return insImm(int32_t(c1) << int32_t(c2));
			if (v == LIR_ush)
				return insImm(uint32_t(c1) >> int32_t(c2));
		}
		else if (oprnd1->isconstq() && oprnd2->isconstq())
		{
			double c1 = oprnd1->constvalf();
			double c2 = oprnd1->constvalf();
			if (v == LIR_eq)
				return insImm(c1 == c2);
			if (v == LIR_lt)
				return insImm(c1 < c2);
			if (v == LIR_gt)
				return insImm(c1 > c2);
			if (v == LIR_le)
				return insImm(c1 <= c2);
			if (v == LIR_ge)
				return insImm(c1 >= c2);
		}
		else if (oprnd1->isconst() && !oprnd2->isconst())
		{
			if (v == LIR_add || v == LIR_mul ||
				v == LIR_fadd || v == LIR_fmul ||
				v == LIR_xor || v == LIR_or || v == LIR_and ||
				v == LIR_eq) {
				// move const to rhs
				LIns* t = oprnd2;
				oprnd2 = oprnd1;
				oprnd1 = t;
			}
			else if (v >= LIR_lt && v <= LIR_uge && !oprnd2->isQuad()) {
				// move const to rhs, swap the operator
				LIns *t = oprnd2;
				oprnd2 = oprnd1;
				oprnd1 = t;
				v = LOpcode(v^1);
			}
			else if (v == LIR_cmov) {
				// const ? x : y => return x or y depending on const
				return oprnd1->constval() ? oprnd2->oprnd1() : oprnd2->oprnd2();
			}
		}

		if (oprnd2->isconst())
		{
			int c = oprnd2->constval();
			if (v == LIR_add && oprnd1->isop(LIR_add) && oprnd1->oprnd2()->isconst()) {
				// add(add(x,c1),c2) => add(x,c1+c2)
				c += oprnd1->oprnd2()->constval();
				oprnd2 = insImm(c);
				oprnd1 = oprnd1->oprnd1();
			}
			else if (v == LIR_sub && oprnd1->isop(LIR_add) && oprnd1->oprnd2()->isconst()) {
				// sub(add(x,c1),c2) => add(x,c1-c2)
				c = oprnd1->oprnd2()->constval() - c;
				oprnd2 = insImm(c);
				oprnd1 = oprnd1->oprnd1();
				v = LIR_add;
			}
			else if (v == LIR_rsh && c == 16 && oprnd1->isop(LIR_lsh) &&
					 oprnd1->oprnd2()->isconstval(16)) {
				if (insIsS16(oprnd1->oprnd1())) {
					// rsh(lhs(x,16),16) == x, if x is S16
					return oprnd1->oprnd1();
				}
			}
			else if (v == LIR_ult) {
				if (oprnd1->isop(LIR_cmov)) {
					LInsp a = oprnd1->oprnd2()->oprnd1();
					LInsp b = oprnd1->oprnd2()->oprnd2();
					if (a->isconst() && b->isconst()) {
						bool a_lt = uint32_t(a->constval()) < uint32_t(oprnd2->constval());
						bool b_lt = uint32_t(b->constval()) < uint32_t(oprnd2->constval());
						if (a_lt == b_lt)
							return insImm(a_lt);
					}
				}
			}

			if (c == 0)
			{
				if (v == LIR_add || v == LIR_or || v == LIR_xor ||
					v == LIR_sub || v == LIR_lsh || v == LIR_rsh || v == LIR_ush)
					return oprnd1;
				else if (v == LIR_and || v == LIR_mul)
					return oprnd2;
				else if (v == LIR_eq && oprnd1->isop(LIR_or) && 
					oprnd1->oprnd2()->isconst() &&
					oprnd1->oprnd2()->constval() != 0) {
					// (x or c) != 0 if c != 0
					return insImm(0);
				}
			}
			else if (c == -1 || c == 1 && oprnd1->isCmp()) {
				if (v == LIR_or) {
					// x | -1 = -1, cmp | 1 = 1
					return oprnd2;
				}
				else if (v == LIR_and) {
					// x & -1 = x, cmp & 1 = cmp
					return oprnd1;
				}
			}
		}

		LInsp i;
		if (v == LIR_qjoin && oprnd1->isop(LIR_qlo) && oprnd2->isop(LIR_qhi) 
			&& (i = oprnd1->oprnd1()) == oprnd1->oprnd1()) {
			// qjoin(qlo(x),qhi(x)) == x
			return i;
		}

		return out->ins2(v, oprnd1, oprnd2);
	}

	LIns* ExprFilter::insGuard(LOpcode v, LInsp c, SideExit *x)
	{
		if (v != LIR_x) {
			if (c->isconst()) {
				if (v == LIR_xt && !c->constval() || v == LIR_xf && c->constval()) {
					return 0; // no guard needed
				}
				else {
					// need a way to EOT now, since this is trace end.
					return out->insGuard(LIR_x, 0, x);
				}
			}
			else {
				while (c->isop(LIR_eq) && c->oprnd1()->isCmp() && 
					c->oprnd2()->isconstval(0)) {
				    // xt(eq(cmp,0)) => xf(cmp)   or   xf(eq(cmp,0)) => xt(cmp)
				    v = LOpcode(v^1);
				    c = c->oprnd1();
				}
			}
		}
		return out->insGuard(v, c, x);
	}

    LIns* LirWriter::insLoadi(LIns *base, int disp) 
    { 
        return insLoad(LIR_ld,base,disp);
    }

	LIns* LirWriter::insLoad(LOpcode op, LIns *base, int disp)
	{
		return insLoad(op, base, insImm(disp));
	}

	LIns* LirWriter::ins_eq0(LIns* oprnd1)
	{
		return ins2i(LIR_eq, oprnd1, 0);
	}

	LIns* LirWriter::qjoin(LInsp lo, LInsp hi)
	{
		return ins2(LIR_qjoin, lo, hi);
	}

	LIns* LirWriter::ins_choose(LIns* cond, LIns* iftrue, LIns* iffalse, bool hasConditionalMove)
	{
		// if not a conditional, make it implicitly an ==0 test (then flop results)
		if (!cond->isCmp())
		{
			cond = ins_eq0(cond);
			LInsp tmp = iftrue;
			iftrue = iffalse;
			iffalse = tmp;
		}

		if (hasConditionalMove)
		{
			return ins2(LIR_cmov, cond, ins2(LIR_2, iftrue, iffalse));
		}

		// @todo -- it might be better to use a short conditional branch rather than
		// the bit-twiddling on systems that don't provide a conditional move instruction.
		LInsp ncond = ins1(LIR_neg, cond); // cond ? -1 : 0
		return ins2(LIR_or, 
					ins2(LIR_and, iftrue, ncond), 
					ins2(LIR_and, iffalse, ins1(LIR_not, ncond)));
	}

    LIns* LirBufWriter::insCall(int32_t fid, LInsp args[])
	{
		static const LOpcode k_argmap[] = { LIR_farg, LIR_arg, LIR_ref };
		static const LOpcode k_callmap[] = { LIR_call, LIR_fcall, LIR_call, LIR_callh };

		const CallInfo& ci = _functions[fid];
		uint32_t argt = ci._argtypes;
		int32_t argc = ci.count_args();
		const uint32_t ret = argt & 3;
		LOpcode op = k_callmap[ret];
		//printf("   ret is type %d %s\n", ret, Lir::_lirNames[op]);

#ifdef NJ_SOFTFLOAT
		if (op == LIR_fcall)
			op = LIR_callh;
		LInsp args2[5*2]; // arm could require 2 args per double
		int32_t j = 0;
		uint32_t argt2 = argt&3; // copy of return type
		for (int32_t i = 0; i < argc; i++) {
			argt >>= 2;
			uint32_t a = argt&3;
			if (a == ARGSIZE_F) {
				LInsp q = args[i];
				args2[j++] = ins1(LIR_qhi, q);
				argt2 |= ARGSIZE_LO << (j*2);
				args2[j++] = ins1(LIR_qlo, q);
				argt2 |= ARGSIZE_LO << (j*2);
			} else {
				args2[j++] = args[i];
				argt2 |= a << (j*2);
			}
		}
		args = args2;
		argt = argt2;
		argc = j;
#endif

		for (int32_t i = 0; i < argc; i++) {
			argt >>= 2;
			AvmAssert((argt&3)!=0);
			ins1(k_argmap[(argt&3)-1], args[i]);
		}

		return insImm8(op==LIR_callh ? LIR_call : op, fid, argc);
	}

	/*
#ifdef AVMPLUS_VERBOSE
    void printTracker(const char* s, RegionTracker& trk, Assembler* assm)
    {
        assm->outputf("%s tracker width %d starting %X zeroth %X indexOf(starting) %d", s, trk.width, trk.starting, trk.zeroth, trk.indexOf(trk.starting));
        assm->output_ins("   location ", trk.location);
        for(int k=0;k<trk.length;k++)
        {
            if (trk.element[k])
            {
                assm->outputf(" [%d]", k+1);
                assm->output_ins(" val ", trk.element[k]);
            }
        }
    }
#endif

	LInsp adjustTracker(RegionTracker& trk, int32_t adj, LInsp i)
	{
		int32_t d = i->immdisp();
		LInsp unaligned = 0;
		if ( d&((1<<trk.width)-1) )
		{
			unaligned = i;
		}
		else
		{
			// put() if there is nothing at this slot
			void* at = (void*)(d+adj);
			if (!trk.get(at))
			{
				LInsp v = i->oprnd1();
				trk.set(at, v);
			}
		}
		return unaligned;
	}

    void trackersAtExit(SideExit* exit, RegionTracker& rtrk, RegionTracker& strk, Assembler *assm)
    {
        (void)assm;
        int32_t s_adj=(int32_t)strk.starting, r_adj=(int32_t)rtrk.starting;
		Fragment* frag = exit->from;
		LInsp param0=frag->param0, sp=frag->sp, rp=frag->rp;
        LirReader *r = frag->lirbuf->reader();
        AvmCore *core = frag->lirbuf->_frago->core();
		InsList live(core->gc);

		rtrk.location->setresv(0);
		strk.location->setresv(0);
		
        verbose_only(if (assm->_verbose) assm->output_ins("Reconstituting region trackers, starting from ", exit->ins);)

		LInsp i = 0;
		bool checkLive = true;
#if 0 
		// @todo needed for partial tree compile 
		bool checkLive = true;
		
		// build a list of known live-valued instructions at the exit
		verbose_only(if (assm->_verbose) assm->output("   compile-time live values at exit");)
		LInsp i = r->setPos(exit->arAtExit);
        while(i)
        {
			if (i->isop(LIR_2))
			{
				LInsp t = i->oprnd1();
				if (live.indexOf(t)<0)
				{
					verbose_only(if (assm->_verbose) assm->output_ins("  ", t);)
					live.add(t);
				}
			}
			i = r->previous();
		}
#endif
		
        // traverse backward starting from the exit instruction
		i = r->setPos(exit->ins);
        while(i)
        {
            if (i->isStore())
            {
                LInsp base = i->oprnd2();
                if (base == param0) 
                {
                    // update stop/rstop
                    int32_t d = i->immdisp();
                    if (d == offsetof(InterpState,sp)) 
					{
                        s_adj += i->oprnd1()->oprnd2()->constval();
                    }
                    else if (d == offsetof(InterpState,rp))
					{
                        r_adj += i->oprnd1()->oprnd2()->constval();
					}
                }
                else if (base == sp) 
                {
					LInsp what = i->oprnd1();
					bool imm = what->isconst() || what->isconstq();
					if (!checkLive || (imm || live.indexOf(what)>=0))
					{
						verbose_only(if (assm->_verbose) assm->output_ins("  strk-adding ", i);)
						adjustTracker(strk, s_adj, i);
					}
					else
					{
						verbose_only(if (assm->_verbose) assm->output_ins("  strk-ignoring ", i);)
					}
                }
                else if (base == rp) 
                {
					LInsp what = i->oprnd1();
					bool imm = what->isconst() || what->isconstq();
					if (!checkLive || imm || live.indexOf(what))
					{
						verbose_only(if (assm->_verbose) assm->output_ins("  rtrk-adding ", i);)
						adjustTracker(rtrk, r_adj, i);
					}
					else
					{
						verbose_only(if (assm->_verbose) assm->output_ins("  rtrk-adding ", i);)
					}
                }
            }
            i = r->previous();
        }

        verbose_only(if (assm->_verbose) { printTracker("rtrk", rtrk,assm); } )
        verbose_only(if (assm->_verbose) { printTracker("strk", strk,assm); } )
    }
	*/

    using namespace avmplus;

	StoreFilter::StoreFilter(LirFilter *in, GC *gc, Assembler *assm, LInsp p0, LInsp sp, LInsp rp) 
		: LirFilter(in), gc(gc), assm(assm), param0(p0), sp(sp), rp(rp), stop(0), rtop(0)
	{}

	LInsp StoreFilter::read() 
	{
		for (;;) 
		{
			LInsp i = in->read();
			if (!i)
				return i;
			bool remove = false;
			if (i->isStore())
			{
				LInsp base = i->oprnd2();
				if (base == param0) 
				{
					// update stop/rstop
					int d = i->immdisp();
					if (d == offsetof(InterpState,sp)) {
						stop = i->oprnd1()->oprnd2()->constval() >> 2;
						NanoAssert(!(stop&1));
					}
					else if (d == offsetof(InterpState,rp))
						rtop = i->oprnd1()->oprnd2()->constval() >> 2;
				}
				else if (base == sp) 
				{
					LInsp v = i->oprnd1();
					int d = i->immdisp() >> 2;
					int top = stop+2;
					if (d >= top) {
						remove = true;
					} else {
						d = top - d;
						if (v->isQuad()) {
							// storing 8 bytes
							if (stk.get(d) && stk.get(d-1)) {
								remove = true;
							} else {
								stk.set(gc, d);
								stk.set(gc, d-1);
							}
						}
						else {
							// storing 4 bytes
							if (stk.get(d))
								remove = true;
							else
								stk.set(gc, d);
						}
					}
				}
				else if (base == rp) 
				{
					int d = i->immdisp() >> 2;
					if (d >= rtop) {
						remove = true;
					} else {
						d = rtop - d;
						if (rstk.get(d))
							remove = true;
						else
							rstk.set(gc, d);
					}
				}
			}
			else if (i->isGuard())
			{
				rstk.reset();
				stk.reset();
				SideExit *exit = i->exit();
				stop = exit->sp_adj >> 2;
				rtop = exit->rp_adj >> 2;
				NanoAssert(!(stop&1));
			}
			if (!remove)
				return i;
		}
	}

	//
	// inlined/separated version of SuperFastHash
	// This content is copyrighted by Paul Hsieh, For reference see : http://www.azillionmonkeys.com/qed/hash.html
	//
	inline uint32_t _hash8(uint32_t hash, const uint8_t data)
	{
		hash += data;
		hash ^= hash << 10;
		hash += hash >> 1;
		return hash;
	}

	inline uint32_t _hash32(uint32_t hash, const uint32_t data)
	{
		const uint32_t dlo = data & 0xffff;
		const uint32_t dhi = data >> 16;
		hash += dlo;
		const uint32_t tmp = (dhi << 11) ^ hash;
		hash = (hash << 16) ^ tmp;
		hash += hash >> 11;
		return hash;
	}
	
	inline uint32_t _hashptr(uint32_t hash, const void* data)
	{
#ifdef AVMPLUS_64BIT
		hash = _hash32(hash, uint32_t(uintptr_t(data) >> 32));
		hash = _hash32(hash, uint32_t(uintptr_t(data)));
		return hash;
#else
		return _hash32(hash, uint32_t(data));
#endif
	}

	inline uint32_t _hashfinish(uint32_t hash)
	{
		/* Force "avalanching" of final 127 bits */
		hash ^= hash << 3;
		hash += hash >> 5;
		hash ^= hash << 4;
		hash += hash >> 17;
		hash ^= hash << 25;
		hash += hash >> 6;
		return hash;
	}

	LInsHashSet::LInsHashSet(GC* gc) : 
			m_list(gc, kInitialCap), m_used(0), m_gc(gc)
	{
		m_list.set(kInitialCap-1, 0);
	}

	/*static*/ uint32_t FASTCALL LInsHashSet::hashcode(LInsp i)
	{
		const LOpcode op = i->opcode();
		switch (op)
		{
			case LIR_short:
				return hashimm(i->imm16());
			case LIR_int:
				return hashimm(i->imm32());
			case LIR_quad:
				return hashimmq(i->constvalq());
			case LIR_call:
			case LIR_fcall:
			{
				LInsp args[10];
				int32_t argc = i->imm8b();
				NanoAssert(argc < 10);
				LirReader ri(i);
				for (int32_t j=argc; j > 0; )
					args[--j] = ri.previous()->oprnd1();
				return hashcall(i->imm8(), argc, args);
			} 
			default:
				if (operandCount(op) == 2)
					return hash2(op, i->oprnd1(), i->oprnd2());
				else
					return hash1(op, i->oprnd1());
		}
	}

	/*static*/ bool FASTCALL LInsHashSet::equals(LInsp a, LInsp b) 
	{
		if (a==b)
			return true;
		AvmAssert(a->opcode() == b->opcode());
		const LOpcode op = a->opcode();
		switch (op)
		{
			case LIR_short:
			{
				return a->imm16() == b->imm16();
			} 
			case LIR_int:
			{
				return a->imm32() == b->imm32();
			} 
			case LIR_quad:
			{
				return a->constvalq() == b->constvalq();
			}
			case LIR_call:
			case LIR_fcall:
			{
				uint32_t argc;
				if (a->imm8() != b->imm8()) return false;
				if ((argc=a->imm8b()) != b->imm8b()) return false;
				LirReader ra(a), rb(b);
				while (argc-- > 0)
					if (ra.previous()->oprnd1() != rb.previous()->oprnd1())
						return false;
				return true;
			} 
			default:
			{
				const uint32_t count = operandCount(op);
				if ((count >= 1 && a->oprnd1() != b->oprnd1()) ||
					(count >= 2 && a->oprnd2() != b->oprnd2()))
					return false;
				return true;
			}
		}
	}

	void FASTCALL LInsHashSet::grow()
	{
		const uint32_t newcap = m_list.size() << 1;
		InsList newlist(m_gc, newcap);
		newlist.set(newcap-1, 0);
		for (uint32_t i=0, n=m_list.size(); i < n; i++)
		{
			LInsp name = m_list.get(i);
			if (!name) continue;
			uint32_t j = find(name, hashcode(name), newlist, newcap);
			newlist.set(j, name);
		}
		m_list.become(newlist);
	}

	uint32_t FASTCALL LInsHashSet::find(LInsp name, uint32_t hash, const InsList& list, uint32_t cap)
	{
		const uint32_t bitmask = (cap - 1) & ~0x1;

		uint32_t n = 7 << 1;
		hash &= bitmask;  
		LInsp k;
		while ((k = list.get(hash)) != NULL &&
			(!LIns::sameop(k,name) || !equals(k, name)))
		{
			hash = (hash + (n += 2)) & bitmask;		// quadratic probe
		}
		return hash;
	}

	LInsp LInsHashSet::add(LInsp name, uint32_t k)
	{
		// this is relatively short-lived so let's try a more aggressive load factor
		// in the interest of improving performance
		if (((m_used+1)<<1) >= m_list.size()) // 0.50
		{
			grow();
			k = find(name, hashcode(name), m_list, m_list.size());
		}
		NanoAssert(!m_list.get(k));
		m_used++;
		m_list.set(k, name);
		return name;
	}

	uint32_t LInsHashSet::hashimm(int32_t a) {
		return _hashfinish(_hash32(0,a));
	}

	uint32_t LInsHashSet::hashimmq(uint64_t a) {
		uint32_t hash = _hash32(0, uint32_t(a >> 32));
		return _hashfinish(_hash32(hash, uint32_t(a)));
	}

	uint32_t LInsHashSet::hash1(LOpcode op, LInsp a) {
		uint32_t hash = _hash8(0,uint8_t(op));
		return _hashfinish(_hashptr(hash, a));
	}

	uint32_t LInsHashSet::hash2(LOpcode op, LInsp a, LInsp b) {
		uint32_t hash = _hash8(0,uint8_t(op));
		hash = _hashptr(hash, a);
		return _hashfinish(_hashptr(hash, b));
	}

	uint32_t LInsHashSet::hashcall(int32_t fid, uint32_t argc, LInsp args[]) {
		uint32_t hash = _hash32(0,fid);
		for (int32_t j=argc-1; j >= 0; j--)
			hash = _hashptr(hash,args[j]);
		return _hashfinish(hash);
	}

	LInsp LInsHashSet::find32(int32_t a, uint32_t &i)
	{
		uint32_t cap = m_list.size();
		const InsList& list = m_list;
		const uint32_t bitmask = (cap - 1) & ~0x1;
		uint32_t hash = hashimm(a) & bitmask;
		uint32_t n = 7 << 1;
		LInsp k;
		while ((k = list.get(hash)) != NULL && 
			(!k->isconst() || k->constval() != a))
		{
			hash = (hash + (n += 2)) & bitmask;		// quadratic probe
		}
		i = hash;
		return k;
	}

	LInsp LInsHashSet::find64(uint64_t a, uint32_t &i)
	{
		uint32_t cap = m_list.size();
		const InsList& list = m_list;
		const uint32_t bitmask = (cap - 1) & ~0x1;
		uint32_t hash = hashimmq(a) & bitmask;  
		uint32_t n = 7 << 1;
		LInsp k;
		while ((k = list.get(hash)) != NULL && 
			(!k->isconstq() || k->constvalq() != a))
		{
			hash = (hash + (n += 2)) & bitmask;		// quadratic probe
		}
		i = hash;
		return k;
	}

	LInsp LInsHashSet::find1(LOpcode op, LInsp a, uint32_t &i)
	{
		uint32_t cap = m_list.size();
		const InsList& list = m_list;
		const uint32_t bitmask = (cap - 1) & ~0x1;
		uint32_t hash = hash1(op,a) & bitmask;  
		uint32_t n = 7 << 1;
		LInsp k;
		while ((k = list.get(hash)) != NULL && 
			(k->opcode() != op || k->oprnd1() != a))
		{
			hash = (hash + (n += 2)) & bitmask;		// quadratic probe
		}
		i = hash;
		return k;
	}

	LInsp LInsHashSet::find2(LOpcode op, LInsp a, LInsp b, uint32_t &i)
	{
		uint32_t cap = m_list.size();
		const InsList& list = m_list;
		const uint32_t bitmask = (cap - 1) & ~0x1;
		uint32_t hash = hash2(op,a,b) & bitmask;  
		uint32_t n = 7 << 1;
		LInsp k;
		while ((k = list.get(hash)) != NULL && 
			(k->opcode() != op || k->oprnd1() != a || k->oprnd2() != b))
		{
			hash = (hash + (n += 2)) & bitmask;		// quadratic probe
		}
		i = hash;
		return k;
	}

	bool argsmatch(LInsp i, uint32_t argc, LInsp args[])
	{
		// we don't have callinfo here so we cannot use argiterator
		LirReader r(i);
		for (LInsp a = r.previous(); a->isArg(); a=r.previous())
			if (a->oprnd1() != args[--argc])
				return false;
		return true;
	}

	LInsp LInsHashSet::findcall(int32_t fid, uint32_t argc, LInsp args[], uint32_t &i)
	{
		uint32_t cap = m_list.size();
		const InsList& list = m_list;
		const uint32_t bitmask = (cap - 1) & ~0x1;
		uint32_t hash = hashcall(fid, argc, args) & bitmask;  
		uint32_t n = 7 << 1;
		LInsp k;
		while ((k = list.get(hash)) != NULL &&
			(!k->isCall() || k->imm8() != fid || !argsmatch(k, argc, args)))
		{
			hash = (hash + (n += 2)) & bitmask;		// quadratic probe
		}
		i = hash;
		return k;
	}

    SideExit *LIns::exit()
    {
        NanoAssert(isGuard());
        return (SideExit*)oprnd2()->payload();
    }

#ifdef NJ_VERBOSE
    class RetiredEntry: public GCObject
    {
    public:
        List<LInsp, LIST_NonGCObjects> live;
        LInsp i;
        RetiredEntry(GC *gc): live(gc) {}
    };
	class LiveTable 
	{
	public:
		SortedMap<LInsp,LInsp,LIST_NonGCObjects> live;
        List<RetiredEntry*, LIST_GCObjects> retired;
		int maxlive;
		LiveTable(GC *gc) : live(gc), retired(gc), maxlive(0) {}
		void add(LInsp i, LInsp use) {
            if (!i->isconst() && !i->isconstq() && !live.containsKey(i)) {
                live.put(i,use);
            }
		}
        void retire(LInsp i, GC *gc) {
            RetiredEntry *e = new (gc) RetiredEntry(gc);
            e->i = i;
            for (int j=0, n=live.size(); j < n; j++) {
                LInsp l = live.keyAt(j);
                if (!l->isStore() && !l->isGuard() && !l->isArg() && !l->isop(LIR_loop))
                    e->live.add(l);
            }
            int size=0;
		    if ((size = e->live.size()) > maxlive)
			    maxlive = size;

            live.remove(i);
            retired.add(e);
		}
		bool contains(LInsp i) {
			return live.containsKey(i);
		}
	};

    void live(GC *gc, Assembler *assm, Fragment *frag)
	{
		// traverse backwards to find live exprs and a few other stats.

		LInsp sp = frag->sp;
		LInsp rp = frag->rp;
		LiveTable live(gc);
		uint32_t exits = 0;
		LirBuffer *lirbuf = frag->lirbuf;
        LirReader br(lirbuf);
		StoreFilter r(&br, gc, 0, frag->param0, sp, rp);
        bool skipargs = false;
        int total = 0;
        live.add(frag->param0, r.pos());
		for (LInsp i = r.read(); i != 0; i = r.read())
		{
            total++;

            if (i->isArg()) {
                if (!skipargs)
                    live.add(i->oprnd1(),0);
            } else {
                skipargs = false;
            }

            // first handle side-effect instructions
			if (i->isStore() || i->isGuard() || i->isop(LIR_loop) ||
				i->isCall() && !assm->callInfoFor(i->imm8())->_cse)
			{
				live.add(i,0);
                if (i->isGuard())
                    exits++;
			}

			// now propagate liveness
			if (live.contains(i))
			{
				live.retire(i,gc);
				if (i->isStore()) {
					live.add(i->oprnd2(),i); // base
					live.add(i->oprnd1(),i); // val
				}
                else if (i->isop(LIR_cmov)) {
                    live.add(i->oprnd1(),i);
                    live.add(i->oprnd2()->oprnd1(),i);
                    live.add(i->oprnd2()->oprnd2(),i);
                }
				else if (operandCount(i->opcode()) == 1) {
				    live.add(i->oprnd1(),i);
				}
				else if (operandCount(i->opcode()) == 2) {
					live.add(i->oprnd1(),i);
					live.add(i->oprnd2(),i);
				}
			}
			else
			{
                skipargs = i->isCall();
			}
		}
 
		assm->outputf("live instruction count %ld, total %ld, max pressure %d",
			live.retired.size(), total, live.maxlive);
        assm->outputf("side exits %ld", exits);

		// print live exprs, going forwards
		LirNameMap *names = frag->lirbuf->names;
		for (int j=live.retired.size()-1; j >= 0; j--) 
        {
            RetiredEntry *e = live.retired[j];
            char livebuf[1000], *s=livebuf;
            *s = 0;
            for (int k=0,n=e->live.size(); k < n; k++) {
				strcpy(s, names->formatRef(e->live[k]));
				s += strlen(s);
				*s++ = ' '; *s = 0;
				NanoAssert(s < livebuf+sizeof(livebuf));
            }
			printf("%-60s %s\n", livebuf, names->formatIns(e->i));
			if (e->i->isGuard())
				printf("\n");
		}
	}

	void LirNameMap::addName(LInsp i, Stringp name) {
		Entry *e = new (labels->core->gc) Entry(name);
		names.put(i, e);
	}
	void LirNameMap::addName(LInsp i, const char *name) {
		addName(i, labels->core->newString(name));
	}

	void LirNameMap::copyName(LInsp i, const char *s, int suffix) {
		char s2[200];
		sprintf(s2,"%s%d", s,suffix);
		addName(i, labels->core->newString(s2));
	}

	void LirNameMap::formatImm(int32_t c, char *buf) {
		if (c >= 10000 || c <= -10000)
			sprintf(buf,"#%s",labels->format((void*)c));
        else
            sprintf(buf,"%d", c);
	}

	const char* LirNameMap::formatRef(LIns *ref)
	{
		char buffer[200], *buf=buffer;
		buf[0]=0;
		GC *gc = labels->core->gc;
		if (names.containsKey(ref)) {
			StringNullTerminatedUTF8 cname(gc, names.get(ref)->name);
			strcat(buf, cname.c_str());
		}
		else if (ref->isconstq()) {
			formatImm(uint32_t(ref->constvalq()>>32), buf);
			buf += strlen(buf);
			*buf++ = ':';
			formatImm(uint32_t(ref->constvalq()), buf);
		}
		else if (ref->isconst()) {
			formatImm(ref->constval(), buf);
		}
		else {
			if (ref->isCall()) {
				copyName(ref, _functions[ref->imm8()]._name, funccounts.add(ref->imm8()));
			} else {
				copyName(ref, nameof(ref), lircounts.add(ref->opcode()));
			}
			StringNullTerminatedUTF8 cname(gc, names.get(ref)->name);
			strcat(buf, cname.c_str());
		}
		return labels->dup(buffer);
	}

	const char* LirNameMap::formatIns(LIns* i)
	{
		char sbuf[200];
		char *s = sbuf;
		if (!i->isStore() && !i->isGuard() && !i->isop(LIR_trace) && !i->isop(LIR_loop)) {
			sprintf(s, "%s = ", formatRef(i));
			s += strlen(s);
		}

		switch(i->opcode())
		{
			case LIR_short:
			case LIR_int:
			{
                sprintf(s, "%s", formatRef(i));
				break;
			}

			case LIR_quad:
			{
				int32_t *p = (int32_t*) (i-2);
				sprintf(s, "#%X:%X", p[1], p[0]);
				break;
			}

			case LIR_loop:
			case LIR_trace:
				sprintf(s, "%s", nameof(i));
				break;

			case LIR_fcall:
			case LIR_call: {
				sprintf(s, "%s ( ", _functions[i->imm8()]._name);
				LirReader r(i);
				for (LInsp a = r.previous(); a->isArg(); a = r.previous()) {
					s += strlen(s);
					sprintf(s, "%s ",formatRef(a->oprnd1()));
				}
				s += strlen(s);
				sprintf(s, ")");
				break;
			}

			case LIR_param:
                sprintf(s, "%s %s", nameof(i), gpn(i->imm8()));
				break;

			case LIR_x: {
                SideExit *x = (SideExit*) i->oprnd2()->payload();
				uint32_t ip = uint32_t(x->from->frid) + x->ip_adj;
				sprintf(s, "%s -> %s sp%+d rp%+d f%+d", nameof(i), 
					labels->format((void*)ip),
					x->sp_adj, x->rp_adj, x->f_adj);
                break;
			}

            case LIR_callh:
			case LIR_neg:
			case LIR_fneg:
			case LIR_arg:
			case LIR_farg:
			case LIR_i2f:
			case LIR_u2f:
			case LIR_qlo:
			case LIR_qhi:
			case LIR_ref:
				sprintf(s, "%s %s", nameof(i), formatRef(i->oprnd1()));
				break;

			case LIR_xt:
			case LIR_xf: {
                SideExit *x = (SideExit*) i->oprnd2()->payload();
				uint32_t ip = int32_t(x->from->frid) + x->ip_adj;
				sprintf(s, "%s %s -> %s sp%+d rp%+d f%+d", nameof(i),
					formatRef(i->oprnd1()),
					labels->format((void*)ip),
					x->sp_adj, x->rp_adj, x->f_adj);
				break;
            }
			case LIR_add:
			case LIR_sub: 
		 	case LIR_mul: 
			case LIR_fadd:
			case LIR_fsub: 
		 	case LIR_fmul: 
			case LIR_fdiv: 
			case LIR_and: 
			case LIR_or: 
			case LIR_not: 
			case LIR_xor: 
			case LIR_lsh: 
			case LIR_rsh:
			case LIR_ush:
			case LIR_eq:
			case LIR_lt:
			case LIR_le:
			case LIR_gt:
			case LIR_ge:
			case LIR_ult:
			case LIR_ule:
			case LIR_ugt:
			case LIR_uge:
			case LIR_qjoin:
				sprintf(s, "%s %s, %s", nameof(i), 
					formatRef(i->oprnd1()), 
					formatRef(i->oprnd2()));
				break;

			case LIR_cmov:
                sprintf(s, "%s ? %s : %s", 
					formatRef(i->oprnd1()), 
					formatRef(i->oprnd2()->oprnd1()), 
					formatRef(i->oprnd2()->oprnd2()));
				break;

			case LIR_ld: 
			case LIR_ldc: 
			case LIR_ldq: 
			case LIR_ldcb: 
				sprintf(s, "%s %s[%s]", nameof(i), 
					formatRef(i->oprnd1()), 
					formatRef(i->oprnd2()));
				break;

			case LIR_st: 
            case LIR_sti:
				sprintf(s, "%s[%d] = %s", 
					formatRef(i->oprnd2()), 
					i->immdisp(), 
					formatRef(i->oprnd1()));
				break;

			default:
				sprintf(s, "?");
				break;
		}
		return labels->dup(sbuf);
	}


#endif

	CseFilter::CseFilter(LirWriter *out, GC *gc)
		: LirWriter(out), exprs(gc) {}

	LIns* CseFilter::insImm(int32_t imm)
	{
		uint32_t k;
		LInsp found = exprs.find32(imm, k);
		if (found)
			return found;
		return exprs.add(out->insImm(imm), k);
	}

	LIns* CseFilter::insImmq(uint64_t q)
	{
		uint32_t k;
		LInsp found = exprs.find64(q, k);
		if (found)
			return found;
		return exprs.add(out->insImmq(q), k);
	}

	LIns* CseFilter::ins1(LOpcode v, LInsp a)
	{
		if (isCse(v)) {
			NanoAssert(operandCount(v)==1);
			uint32_t k;
			LInsp found = exprs.find1(v, a, k);
			if (found)
				return found;
			return exprs.add(out->ins1(v,a), k);
		}
		return out->ins1(v,a);
	}

	LIns* CseFilter::ins2(LOpcode v, LInsp a, LInsp b)
	{
		if (isCse(v)) {
			NanoAssert(operandCount(v)==2);
			uint32_t k;
			LInsp found = exprs.find2(v, a, b, k);
			if (found)
				return found;
			return exprs.add(out->ins2(v,a,b), k);
		}
		return out->ins2(v,a,b);
	}

	LIns* CseFilter::insLoad(LOpcode v, LInsp base, LInsp disp)
	{
		if (isCse(v)) {
			NanoAssert(operandCount(v)==2);
			uint32_t k;
			LInsp found = exprs.find2(v, base, disp, k);
			if (found)
				return found;
			return exprs.add(out->insLoad(v,base,disp), k);
		}
		return out->insLoad(v,base,disp);
	}

	LInsp CseFilter::insGuard(LOpcode v, LInsp c, SideExit *x)
	{
		if (isCse(v)) {
			// conditional guard
			NanoAssert(operandCount(v)==1);
			uint32_t k;
			LInsp found = exprs.find1(v, c, k);
			if (found)
				return 0;
			return exprs.add(out->insGuard(v,c,x), k);
		}
		return out->insGuard(v, c, x);
	}

	LInsp CseFilter::insCall(int32_t fid, LInsp args[])
	{
		const CallInfo *c = &_functions[fid];
		if (c->_cse) {
			uint32_t k;
			LInsp found = exprs.findcall(fid, c->count_args(), args, k);
			if (found)
				return found;
			return exprs.add(out->insCall(fid, args), k);
		}
		return out->insCall(fid, args);
	}

    LIns* FASTCALL callArgN(LIns* i, uint32_t n)
	{
		// @todo clean up; shouldn't have to create a reader                                               
		LirReader rdr(i);
		do
			i = rdr.read();
		while (n-- > 0);
		return i;
	}

    void compile(Assembler* assm, Fragment* triggerFrag)
    {
        AvmCore *core = triggerFrag->lirbuf->_frago->core();
        GC *gc = core->gc;

		verbose_only( StringList asmOutput(gc); )
		verbose_only( assm->_outputCache = &asmOutput; )

		verbose_only(if (assm->_verbose && core->config.verbose_live)
			live(gc, assm, triggerFrag);)

		bool treeCompile = core->config.tree_opt && (triggerFrag->kind == BranchTrace);
		RegAllocMap regMap(gc);
		NInsList loopJumps(gc);
		assm->beginAssembly(triggerFrag, &regMap);

		//fprintf(stderr, "recompile trigger %X kind %d\n", (int)triggerFrag, triggerFrag->kind);
		Fragment* root = triggerFrag;
		if (treeCompile)
		{
			// recompile the entire tree
			root = triggerFrag->anchor;
			root->removeIntraLinks();
			root->unlink(assm);			// unlink all incoming jumps ; since the compile() can fail
			root->unlinkBranches(assm); // no one jumps into a branch (except from within the tree) so safe to clear the links table
			root->fragEntry = 0;
			
			// do the tree branches
			Fragment* frag = root->treeBranches;
			while(frag)
			{
				// compile til no more frags
				if (frag->lastIns)
				{
					NIns* loopJump = assm->assemble(frag);
					verbose_only(if (assm->_verbose) assm->outputf("compiling branch %X that exits from SID %d",frag->frid,frag->spawnedFrom->sid);)
					if (loopJump) loopJumps.add((intptr_t)loopJump);
					
					NanoAssert(frag->kind == BranchTrace);
					RegAlloc* regs = new (gc) RegAlloc();
					assm->copyRegisters(regs);
					assm->releaseRegisters();
					SideExit* exit = frag->spawnedFrom;
					regMap.put(exit, regs);
				}
				frag = frag->treeBranches;
			}
		}
		
		// now the the main trunk

		NIns* loopJump = assm->assemble(root);
		verbose_only(if (assm->_verbose) assm->output("compiling trunk");)
		if (loopJump) loopJumps.add((intptr_t)loopJump);
		assm->endAssembly(root, loopJumps);

		// remove the map enties
		while(!regMap.isEmpty())
			gc->Free(regMap.removeLast());
			
		// reverse output so that assembly is displayed low-to-high
		verbose_only( assm->_outputCache = 0; )
		verbose_only(for(int i=asmOutput.size()-1; i>=0; --i) { assm->outputf("%s",asmOutput.get(i)); } );

		if (assm->error())
		{
			root->fragEntry = 0;
		}
		else
		{
			root->link(assm);
			if (treeCompile) root->linkBranches(assm);
		}
    }

	#endif /* FEATURE_NANOJIT */

#if defined(NJ_VERBOSE)
    LabelMap::LabelMap(AvmCore *core, LabelMap* parent)
        : parent(parent), names(core->gc), addrs(core->config.verbose_addrs), end(buf), core(core)
	{}

    void LabelMap::add(const void *p, size_t size, size_t align, const char *name)
	{
		if (!this) return;
		add(p, size, align, core->newString(name));
	}

    void LabelMap::add(const void *p, size_t size, size_t align, Stringp name)
    {
		if (!this) return;
		Entry *e = new (core->gc) Entry(name, size<<align, align);
		names.put(p, e);
    }

    const char *LabelMap::format(const void *p)
    {
		char b[200];
		int i = names.findNear(p);
		if (i >= 0) {
			const void *start = names.keyAt(i);
			Entry *e = names.at(i);
			const void *end = (const char*)start + e->size;
			avmplus::StringNullTerminatedUTF8 cname(core->gc, e->name);
			const char *name = cname.c_str();
			if (p == start) {
				if (addrs)
					sprintf(b,"%p %s",p,name);
				else
					strcpy(b, name);
				return dup(b);
			}
			else if (p > start && p < end) {
				int d = (int(p)-int(start)) >> e->align;
				if (addrs)
					sprintf(b, "%p %s+%d", p, name, d);
				else
					sprintf(b,"%s+%d", name, d);
				return dup(b);
			}
		}
		if (parent)
			return parent->format(p);

		sprintf(b, "%p", p);
		return dup(b);
    }

	const char *LabelMap::dup(const char *b)
	{
		int need = strlen(b)+1;
		char *s = end;
		end += need;
		if (end > buf+sizeof(buf)) {
			s = buf;
			end = s+need;
		}
		strcpy(s, b);
		return s;
	}
#endif // NJ_VERBOSE
}
	
