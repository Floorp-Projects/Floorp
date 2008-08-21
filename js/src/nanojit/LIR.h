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

#ifndef __nanojit_LIR__
#define __nanojit_LIR__

namespace avmplus { class RegionTracker; }    

namespace nanojit
{
	#define is_trace_skip_tramp(op) ((op) <= LIR_tramp)
	
	enum LOpcode __msvc_only(: unsigned)	
	{
		// flags; upper bits reserved
		LIR64	= 0x40,			// result is double or quad
		
		// special operations (must be 0..N)
		LIR_trace = 2,	
		LIR_nearskip = 3, // must be LIR_skip-1 and lsb=1
		LIR_skip = 4,
        LIR_neartramp = 5, // must be LIR_tramp-1 and lsb=1
        LIR_tramp = 6,

		// non-pure operations
		LIR_param	= 10,
		LIR_st		= 11,
		LIR_ld		= 12,
        LIR_sti     = 14,
		LIR_call	= 18,
			
		// guards
		LIR_loop    = 19, // loop fragment
		LIR_x		= 20, // exit always

		// operators

		LIR_feq		= 26,
		LIR_flt		= 27,
		LIR_fgt		= 28,
		LIR_fle		= 29,
		LIR_fge		= 30,
        LIR_cmov    = 31, // conditional move (op1=cond, op2=cond(iftrue,iffalse))
		LIR_short   = 32,
		LIR_int		= 33,
		LIR_ldc     = 34, // non-volatile load
		LIR_2       = 35, // wraps a pair of refs
		LIR_neg		= 36,					// [ 1 integer input / integer output ]
		LIR_add		= 37,					// [ 2 operand integer intputs / integer output ]
		LIR_sub		= 38,
		LIR_mul		= 39,
        LIR_callh   = 40,
		LIR_and		= 41,
		LIR_or		= 42,
		LIR_xor		= 43,
		LIR_not		= 44,
		LIR_lsh		= 45,
		LIR_rsh		= 46,	// >>
		LIR_ush		= 47,	// >>>
        // conditional guards, op^1 to complement
		LIR_xt		= 48, // exit if true   0x30 0011 0000
		LIR_xf		= 49, // exit if false  0x31 0011 0001
		LIR_qlo		= 50,
		LIR_qhi		= 51,
		LIR_ldcb    = 52, // non-volatile 8-bit load

        LIR_ov      = 53,
        LIR_cs      = 54,
        LIR_eq      = 55,
        // relational operators.  op^1 to swap left/right, op^3 to complement.
		LIR_lt      = 56, // 0x38 0011 1000
		LIR_gt      = 57, // 0x39 0011 1001
		LIR_le		= 58, // 0x3A 0011 1010
		LIR_ge		= 59, // 0x3B 0011 1011
		LIR_ult		= 60, // 0x3C 0011 1100
		LIR_ugt		= 61, // 0x3D 0011 1101
		LIR_ule		= 62, // 0x3E 0011 1110
		LIR_uge		= 63, // 0x3F 0011 1111

		/**
		 * 64bit operations
		 */
		LIR_stq		= LIR_st | LIR64,
		LIR_stqi	= LIR_sti | LIR64,
		LIR_quad    = LIR_int | LIR64,
		LIR_ldq		= LIR_ld    | LIR64,
        LIR_qiand   = 24 | LIR64,
        LIR_qiadd   = 25 | LIR64,
        LIR_qilsh   = LIR_lsh | LIR64,

        LIR_fcall   = LIR_call  | LIR64,
		LIR_fneg	= LIR_neg  | LIR64,
		LIR_fadd	= LIR_add  | LIR64,
		LIR_fsub	= LIR_sub  | LIR64,
		LIR_fmul	= LIR_mul  | LIR64,
		LIR_fdiv	= 40        | LIR64,
		LIR_qcmov	= LIR_cmov | LIR64,

		LIR_qjoin	= 41 | LIR64,
		LIR_i2f		= 42 | LIR64,
		LIR_u2f		= 43 | LIR64
	};

	#if defined NANOJIT_64BIT
	#define LIR_ldp     LIR_ldq
    #define LIR_piadd   LIR_qiadd
    #define LIR_piand   LIR_qiand
    #define LIR_pilsh   LIR_qilsh
	#define LIR_pcmov	LIR_qcmov
	#else
	#define LIR_ldp     LIR_ld
    #define LIR_piadd   LIR_add
    #define LIR_piand   LIR_and
    #define LIR_pilsh   LIR_lsh
	#define LIR_pcmov	LIR_cmov
	#endif

	inline uint32_t argwords(uint32_t argc) {
		return (argc+3)>>2;
	}

    struct SideExit;
    struct Page;
    struct CallInfo;

	// Low-level Instruction 4B
	// had to lay it our as a union with duplicate code fields since msvc couldn't figure out how to compact it otherwise.
	class LIns
	{
        friend class LirBufWriter;
		// 3-operand form (backwards reach only)
		struct u_type
		{
			LOpcode			code:8;
			uint32_t		oprnd_3:8;	// only used for store, since this location gets clobbered during generation
			uint32_t		oprnd_1:8;  // 256 ins window and since they only point backwards this is sufficient.
			uint32_t		oprnd_2:8;  
		};

        struct sti_type
        {
			LOpcode			code:8;
			int32_t	    	disp:8;
			uint32_t		oprnd_1:8;  // 256 ins window and since they only point backwards this is sufficient.
			uint32_t		oprnd_2:8;  
        };

		// imm8 form 
		struct c_type
		{
			LOpcode			code:8;
			uint32_t		resv:8;  // cobberred during assembly
			uint32_t		imm8a:8;
			uint32_t		imm8b:8;  
		};

        // imm24 form for short tramp & skip
        struct t_type
        {
            LOpcode         code:8;
            int32_t         imm24:24;
        };

		// imm16 form
		struct i_type
		{
			LOpcode			code:8;
			uint32_t		resv:8;  // cobberred during assembly
			int32_t			imm16:16;
		};

		// overlay used during code generation ( note that last byte is reserved for allocation )
		struct g_type
		{
			LOpcode			code:8;
			uint32_t		resv:8;   // cobberred during assembly
			uint32_t		unused:16;
		};

		/**
		 * Various forms of the instruction.
		 * 
		 *    In general the oprnd_x entries contain an uint value 0-255 that identifies a previous 
		 *    instruction, where 0 means the previous instruction and 255 means the instruction two
		 *    hundred and fifty five prior to this one. 
		 *      
		 *    For pointing to instructions further than this range LIR_tramp is used.
		 */
		union 
		{
			u_type u;
			c_type c;
			i_type i;
            t_type t;
			g_type g;
            sti_type sti;
		};

		uint32_t reference(LIns*) const;
		LIns* deref(int32_t off) const;

	public:
		LIns*		FASTCALL oprnd1() const;
		LIns*		FASTCALL oprnd2() const;
		LIns*		FASTCALL oprnd3() const;

		inline LOpcode	opcode() const	{ return u.code; }
		inline uint8_t	imm8()	 const	{ return c.imm8a; }
		inline int16_t	imm16()	 const	{ return i.imm16; }
		inline LIns*	ref()	 const	{ 
            return (t.code & 1) ? (LIns*)this+t.imm24 : *(LIns**)(this-1);
        }
		inline int32_t	imm32()	 const	{ return *(int32_t*)(this-1); }
		inline uint8_t	resv()	 const  { return g.resv; }
        void*	payload() const;
        inline Page*	page()			{ return (Page*) alignTo(this,NJ_PAGE_SIZE); }

		// index args in r-l order.  arg(0) is rightmost arg
		inline LIns* arg(uint32_t i) {
			uint32_t c = argc();
			NanoAssert(i < c);
			uint8_t* offs = (uint8_t*) (this-argwords(c));
			return deref(offs[i]);
		}

        inline int32_t  immdisp()const 
		{
            return (u.code&~LIR64) == LIR_sti ? sti.disp : oprnd3()->constval();
        }
    
		inline static bool sameop(LIns* a, LIns* b)
		{
			// hacky but more efficient than opcode() == opcode() due to bit masking of 7-bit field
			union { 
				uint32_t x; 
				u_type u;
			} tmp;
			tmp.x = *(uint32_t*)a ^ *(uint32_t*)b;
			return tmp.u.code == 0;
		}

		inline int32_t constval() const
		{
			NanoAssert(isconst());
			return isop(LIR_short) ? imm16() : imm32();
		}

		inline uint64_t constvalq() const
		{
			NanoAssert(isconstq());
		#ifdef AVMPLUS_UNALIGNED_ACCESS
			return *(const uint64_t*)(this-2);
		#else
			union { uint64_t tmp; int32_t dst[2]; } u;
			const int32_t* src = (const int32_t*)(this-2);
			u.dst[0] = src[0];
			u.dst[1] = src[1];
			return u.tmp;
		#endif
		}
		
		inline void* constvalp() const
		{
        #ifdef AVMPLUS_64BIT
		    return (void*)constvalq();
		#else
		    return (void*)constval();
        #endif      
		}
		
		inline double constvalf() const
		{
			NanoAssert(isconstq());
		#ifdef AVMPLUS_UNALIGNED_ACCESS
			return *(const double*)(this-2);
		#else
			union { uint32_t dst[2]; double tmpf; } u;
			const int32_t* src = (const int32_t*)(this-2);
			u.dst[0] = src[0];
			u.dst[1] = src[1];
			return u.tmpf;
		#endif
		}

		bool isCse(const CallInfo *functions) const;
		bool isop(LOpcode o) const { return u.code == o; }
		bool isQuad() const;
		bool isCond() const;
		bool isCmp() const;
		bool isCall() const;
        bool isStore() const;
        bool isLoad() const;
		bool isGuard() const;
		bool isconst() const;
		bool isconstval(int32_t val) const;
		bool isconstq() const;
		bool isconstp() const;
        bool isTramp() {
            return isop(LIR_neartramp) || isop(LIR_tramp);
        }

		void setimm16(int32_t i);
		void setimm24(int32_t i);
		void setresv(uint32_t resv);
		void initOpcode(LOpcode);
		void setOprnd1(LIns*);
		void setOprnd2(LIns*);
		void setOprnd3(LIns*);
        void setDisp(int8_t d);

        SideExit *exit();

		inline uint32_t argc() {
			NanoAssert(isCall());
			return c.imm8b;
		}
        inline uint8_t  fid() const {
			NanoAssert(isCall());
			return c.imm8a;
        }
	};
	typedef LIns*		LInsp;

	bool FASTCALL isCse(LOpcode v);
	bool FASTCALL isCmp(LOpcode v);
	bool FASTCALL isCond(LOpcode v);
	LIns* FASTCALL callArgN(LInsp i, uint32_t n);
	extern const uint8_t operandCount[];

	class Fragmento;	// @todo remove this ; needed for minbuild for some reason?!?  Should not be compiling this code at all
	class LirFilter;
	struct CallInfo;

	// make it a GCObject so we can explicitly delete it early
	class LirWriter : public GCObject
	{
	public:
		LirWriter *out;
	public:
        const CallInfo *_functions;

		virtual ~LirWriter() {}
		LirWriter(LirWriter* out) 
			: out(out), _functions(out?out->_functions : 0) {}

		virtual LInsp ins0(LOpcode v) {
			return out->ins0(v);
		}
		virtual LInsp ins1(LOpcode v, LIns* a) {
			return out->ins1(v, a);
		}
		virtual LInsp ins2(LOpcode v, LIns* a, LIns* b) {
			return out->ins2(v, a, b);
		}
		virtual LInsp insGuard(LOpcode v, LIns *c, SideExit *x) {
			return out->insGuard(v, c, x);
		}
		virtual LInsp insParam(int32_t i) {
			return out->insParam(i);
		}
		virtual LInsp insImm(int32_t imm) {
			return out->insImm(imm);
		}
		virtual LInsp insImmq(uint64_t imm) {
			return out->insImmq(imm);
		}
		virtual LInsp insLoad(LOpcode op, LIns* base, LIns* d) {
			return out->insLoad(op, base, d);
		}
		virtual LInsp insStore(LIns* value, LIns* base, LIns* disp) {
			return out->insStore(value, base, disp);
		}
		virtual LInsp insStorei(LIns* value, LIns* base, int32_t d) {
			return isS8(d) ? out->insStorei(value, base, d)
				: out->insStore(value, base, insImm(d));
		}
		virtual LInsp insCall(uint32_t fid, LInsp args[]) {
			return out->insCall(fid, args);
		}

		// convenience
	    LIns*		insLoadi(LIns *base, int disp);
	    LIns*		insLoad(LOpcode op, LIns *base, int disp);
	    LIns*		ins_choose(LIns* cond, LIns* iftrue, LIns* iffalse, bool);
	    LIns*		ins_eq0(LIns* oprnd1);
        LIns*       ins2i(LOpcode op, LIns *oprnd1, int32_t);
		LIns*		qjoin(LInsp lo, LInsp hi);
		LIns*		insImmPtr(const void *ptr);
	};

#ifdef NJ_VERBOSE
	extern const char* lirNames[];

	/**
	 * map address ranges to meaningful names.
	 */
    class LabelMap MMGC_SUBCLASS_DECL
    {
		LabelMap* parent;
		class Entry MMGC_SUBCLASS_DECL
		{
		public:
			Entry(int) : name(0), size(0), align(0) {}
			Entry(avmplus::String *n, size_t s, size_t a) : name(n),size(s),align(a) {}
            ~Entry(); 
			DRCWB(avmplus::String*) name;
			size_t size:29, align:3;
		};
        avmplus::SortedMap<const void*, Entry*, avmplus::LIST_GCObjects> names;
		bool addrs, pad[3];
		char buf[1000], *end;
        void formatAddr(const void *p, char *buf);
    public:
		AvmCore *core;
        LabelMap(AvmCore *, LabelMap* parent);
        ~LabelMap();
        void add(const void *p, size_t size, size_t align, const char *name);
		void add(const void *p, size_t size, size_t align, avmplus::String*);
		const char *dup(const char *);
		const char *format(const void *p);
		void promoteAll(const void *newbase);
    };

	class LirNameMap MMGC_SUBCLASS_DECL
	{
		class CountMap: public avmplus::SortedMap<int, int, avmplus::LIST_NonGCObjects> {
		public:
			CountMap(GC*gc) : avmplus::SortedMap<int, int, avmplus::LIST_NonGCObjects>(gc) {};
			int add(int i) {
				int c = 1;
				if (containsKey(i)) {
					c = 1+get(i);
				}
				put(i,c);
				return c;
			}
		} lircounts, funccounts;
		class Entry MMGC_SUBCLASS_DECL 
		{
		public:
			Entry(int) : name(0) {}
			Entry(avmplus::String *n) : name(n) {}
            ~Entry();
			DRCWB(avmplus::String*) name;
		};
		avmplus::SortedMap<LInsp, Entry*, avmplus::LIST_GCObjects> names;
		const CallInfo *_functions;
		LabelMap *labels;
		void formatImm(int32_t c, char *buf);
	public:

		LirNameMap(GC *gc, const CallInfo *_functions, LabelMap *r) 
			: lircounts(gc),
			funccounts(gc),
			names(gc),
			_functions(_functions),
			labels(r)
		{}
        ~LirNameMap();

		void addName(LInsp i, const char *s);
		void addName(LInsp i, avmplus::String *s);
		void copyName(LInsp i, const char *s, int suffix);
        const char *formatRef(LIns *ref);
		const char *formatIns(LInsp i);
		void formatGuard(LInsp i, char *buf);
	};


	class VerboseWriter : public LirWriter
	{
		avmplus::List<LInsp, avmplus::LIST_NonGCObjects> code;
		LirNameMap *names;
    public:
		VerboseWriter(GC *gc, LirWriter *out, LirNameMap* names) 
			: LirWriter(out), code(gc), names(names) 
		{}

		LInsp add(LInsp i) {
			code.add(i);
			return i;
		}

		void flush()
		{
			for (int j=0, n=code.size(); j < n; j++)
				printf("    %s\n",names->formatIns(code[j]));
			code.clear();
			printf("\n");
		}

		LIns* insGuard(LOpcode op, LInsp cond, SideExit *x) {
			LInsp i = add(out->insGuard(op,cond,x));
			if (i)
				flush();
			return i;
		}

		LIns* ins0(LOpcode v) {
			LInsp i = add(out->ins0(v));
			if (i)
				flush();
			return i;
		}

		LIns* ins1(LOpcode v, LInsp a) {
			return add(out->ins1(v, a));
		}
		LIns* ins2(LOpcode v, LInsp a, LInsp b) {
			return v == LIR_2 ? out->ins2(v,a,b) : add(out->ins2(v, a, b));
		}
		LIns* insCall(uint32_t fid, LInsp args[]) {
			return add(out->insCall(fid, args));
		}
		LIns* insParam(int32_t i) {
			return add(out->insParam(i));
		}
		LIns* insLoad(LOpcode v, LInsp base, LInsp disp) {
			return add(out->insLoad(v, base, disp));
		}
		LIns* insStore(LInsp v, LInsp b, LInsp d) {
			return add(out->insStore(v, b, d));
		}
		LIns* insStorei(LInsp v, LInsp b, int32_t d) {
			return add(out->insStorei(v, b, d));
		}
    };

#endif

	class ExprFilter: public LirWriter
	{
	public:
		ExprFilter(LirWriter *out) : LirWriter(out) {}
		LIns* ins1(LOpcode v, LIns* a);
	    LIns* ins2(LOpcode v, LIns* a, LIns* b);
		LIns* insGuard(LOpcode v, LIns *c, SideExit *x);
	};

	// @todo, this could be replaced by a generic HashMap or HashSet, if we had one
	class LInsHashSet
	{
		// must be a power of 2. 
		// don't start too small, or we'll waste time growing and rehashing.
		// don't start too large, will waste memory. 
		static const uint32_t kInitialCap = 2048;	

		InsList m_list;
		uint32_t m_used;
		GC* m_gc;

		static uint32_t FASTCALL hashcode(LInsp i);
		uint32_t FASTCALL find(LInsp name, uint32_t hash, const InsList& list, uint32_t cap);
		static bool FASTCALL equals(LInsp a, LInsp b);
		void FASTCALL grow();

	public:

		LInsHashSet(GC* gc);
		LInsp find32(int32_t a, uint32_t &i);
		LInsp find64(uint64_t a, uint32_t &i);
		LInsp find1(LOpcode v, LInsp a, uint32_t &i);
		LInsp find2(LOpcode v, LInsp a, LInsp b, uint32_t &i);
		LInsp findcall(uint32_t fid, uint32_t argc, LInsp args[], uint32_t &i);
		LInsp add(LInsp i, uint32_t k);
		void replace(LInsp i);

		static uint32_t FASTCALL hashimm(int32_t);
		static uint32_t FASTCALL hashimmq(uint64_t);
		static uint32_t FASTCALL hash1(LOpcode v, LInsp);
		static uint32_t FASTCALL hash2(LOpcode v, LInsp, LInsp);
		static uint32_t FASTCALL hashcall(uint32_t fid, uint32_t argc, LInsp args[]);
	};

	class CseFilter: public LirWriter
	{
	public:
		LInsHashSet exprs;
		CseFilter(LirWriter *out, GC *gc);
	    LIns* insImm(int32_t imm);
	    LIns* insImmq(uint64_t q);
		LIns* ins1(LOpcode v, LInsp);
		LIns* ins2(LOpcode v, LInsp, LInsp);
		LIns* insLoad(LOpcode v, LInsp b, LInsp d);
		LIns* insCall(uint32_t fid, LInsp args[]);
		LIns* insGuard(LOpcode op, LInsp cond, SideExit *x);
	};

	struct Page;
	class LirBuffer : public GCFinalizedObject
	{
		public:
			DWB(Fragmento*)		_frago;
			LirBuffer(Fragmento* frago, const CallInfo* functions);
			virtual ~LirBuffer();
			void        clear();
			LInsp		next();
			LInsp		commit(uint32_t count);
			bool		addPage();
			bool		outOmem() { return _noMem != 0; }
			debug_only (void		validate() const;)
			verbose_only(DWB(LirNameMap*) names;)
			verbose_only(int insCount();)
			verbose_only(int byteCount();)

			// stats
			struct 
			{
				uint32_t lir;	// # instructions
				uint32_t pages;	// pages consumed
			}
			_stats;

			const CallInfo* _functions;
            LInsp state,param1,sp,rp;
			
		private:
			Page*		pageAlloc();

			Page*				_start;		// first page
			LInsp				_unused;	// next unused instruction slot
			int					_noMem;		// set if ran out of memory when writing to buffer
	};	

	class LirBufWriter : public LirWriter
	{
		DWB(LirBuffer*)	_buf;		// underlying buffer housing the instructions
        LInsp spref, rpref;

        public:
			LirBufWriter(LirBuffer* buf)
				: LirWriter(0), _buf(buf) {
				_functions = buf->_functions;
			}

			// LirWriter interface
			LInsp   insLoad(LOpcode op, LInsp base, LInsp off);
			LInsp	insStore(LInsp o1, LInsp o2, LInsp o3);
			LInsp	insStorei(LInsp o1, LInsp o2, int32_t imm);
			LInsp	ins0(LOpcode op);
			LInsp	ins1(LOpcode op, LInsp o1);
			LInsp	ins2(LOpcode op, LInsp o1, LInsp o2);
			LInsp	insParam(int32_t i);
			LInsp	insImm(int32_t imm);
			LInsp	insImmq(uint64_t imm);
		    LInsp	insCall(uint32_t fid, LInsp args[]);
			LInsp	insGuard(LOpcode op, LInsp cond, SideExit *x);

			// buffer mgmt
			LInsp	skip(size_t);
			LInsp	insFar(LOpcode op, LInsp target);
			LInsp	ensureReferenceable(LInsp i, int32_t addedDistance);
			bool	ensureRoom(uint32_t count);
			bool	canReference(LInsp from, LInsp to) {
				return isU8(from-to-1);
			}
	};

	class LirFilter
	{
	public:
		LirFilter *in;
		LirFilter(LirFilter *in) : in(in) {}
		virtual ~LirFilter() {}

		virtual LInsp read() {
			return in->read();
		}
		virtual LInsp pos() {
			return in->pos();
		}
	};

	// concrete
	class LirReader : public LirFilter
	{
		LInsp _i; // current instruction that this decoder is operating on.

	public:
		LirReader(LirBuffer* buf) : LirFilter(0), _i(buf->next()-1) { }
		LirReader(LInsp i) : LirFilter(0), _i(i) { }
		virtual ~LirReader() {}

		// LirReader i/f
		LInsp read(); // advance to the prior instruction
		LInsp pos() {
			return _i;
		}
	};

    class Assembler;

    void compile(Assembler *assm, Fragment *frag);
    verbose_only( void printTracker(const char* s, avmplus::RegionTracker& trk, Assembler* assm); )
	verbose_only(void live(GC *gc, Assembler *assm, Fragment *frag);)

	class StackFilter: public LirFilter
	{
		GC *gc;
		Fragment *frag;
		LInsp sp;
		avmplus::BitSet stk;
        int top;
		int getTop(LInsp guard);
	public:
		StackFilter(LirFilter *in, GC *gc, Fragment *frag, LInsp sp); 
		virtual ~StackFilter() {}
		LInsp read();
	};

	class CseReader: public LirFilter
	{
		LInsHashSet *exprs;
		const CallInfo *functions;
	public:
		CseReader(LirFilter *in, LInsHashSet *exprs, const CallInfo*);
		LInsp read();
	};
}
#endif // __nanojit_LIR__
