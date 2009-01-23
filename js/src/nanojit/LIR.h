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

/**
 * Fundamentally, the arguments to the various operands can be grouped along
 * two dimensions.  One dimension is size: can the arguments fit into a 32-bit
 * register, or not?  The other dimension is whether the argument is an integer
 * (including pointers) or a floating-point value.  In all comments below,
 * "integer" means integer of any size, including 64-bit, unless otherwise
 * specified.  All floating-point values are always 64-bit.  Below, "quad" is
 * used for a 64-bit value that might be either integer or floating-point.
 */
namespace nanojit
{
	#define is_trace_skip_tramp(op) ((op) <= LIR_tramp)
	
	enum LOpcode
#if defined(_MSC_VER) && _MSC_VER >= 1400
          : unsigned
#endif
	{
		// flags; upper bits reserved
		LIR64	= 0x40,			// result is double or quad
		
#define OPDEF(op, number, args) \
        LIR_##op = (number),
#define OPDEF64(op, number, args) \
        LIR_##op = ((number) | LIR64),
#include "LIRopcode.tbl"
        LIR_sentinel
#undef OPDEF
#undef OPDEF64
	};

	#if defined NANOJIT_64BIT
	#define LIR_ldp     LIR_ldq
	#define LIR_stp     LIR_stq
    #define LIR_piadd   LIR_qiadd
    #define LIR_piand   LIR_qiand
    #define LIR_pilsh   LIR_qilsh
	#define LIR_pcmov	LIR_qcmov
    #define LIR_pior    LIR_qior
	#else
	#define LIR_ldp     LIR_ld
	#define LIR_stp     LIR_st
    #define LIR_piadd   LIR_add
    #define LIR_piand   LIR_and
    #define LIR_pilsh   LIR_lsh
	#define LIR_pcmov	LIR_cmov
    #define LIR_pior    LIR_or
	#endif

	inline uint32_t argwords(uint32_t argc) {
		return (argc+3)>>2;
	}

	struct GuardRecord;
    struct SideExit;
    struct Page;

    enum AbiKind {
        ABI_FASTCALL,
        ABI_THISCALL,
		ABI_STDCALL,
        ABI_CDECL
    };

    enum ArgSize {
	    ARGSIZE_NONE = 0,
	    ARGSIZE_F = 1,
	    ARGSIZE_LO = 2,
	    ARGSIZE_Q = 3,
	    _ARGSIZE_MASK_INT = 2, 
        _ARGSIZE_MASK_ANY = 3
    };

    struct CallInfo
	{
		uintptr_t	_address;
        uint32_t	_argtypes:18;	// 9 2-bit fields indicating arg type, by ARGSIZE above (including ret type): a1 a2 a3 a4 a5 ret
        uint8_t		_cse:1;			// true if no side effects
        uint8_t		_fold:1;		// true if no side effects
        AbiKind     _abi:3;
		verbose_only ( const char* _name; )
		
		uint32_t FASTCALL _count_args(uint32_t mask) const;
        uint32_t get_sizes(ArgSize*) const;

        inline bool isInterface() const {
            return _address == 2 || _address == 3; /* hack! */
        }
        inline bool isIndirect() const {
            return _address < 256;
        }
		inline uint32_t FASTCALL count_args() const {
            return _count_args(_ARGSIZE_MASK_ANY) + isIndirect();
        }
		inline uint32_t FASTCALL count_iargs() const {
            return _count_args(_ARGSIZE_MASK_INT);
        }
		// fargs = args - iargs
	};

    inline bool isGuard(LOpcode op) {
        return op == LIR_x || op == LIR_xf || op == LIR_xt || op == LIR_loop || op == LIR_xbarrier;
    }

    inline bool isCall(LOpcode op) {
        op = LOpcode(op & ~LIR64);
        return op == LIR_call || op == LIR_calli;
    }

    inline bool isStore(LOpcode op) {
        op = LOpcode(op & ~LIR64);
        return op == LIR_st || op == LIR_sti;
    }

    inline bool isConst(LOpcode op) {
        NanoStaticAssert((LIR_short & 1) == 0);
        NanoStaticAssert(LIR_int == LIR_short + 1);
        return (op & ~1) == LIR_short;
    }

    inline bool isLoad(LOpcode op) {
        return op == LIR_ldq || op == LIR_ld || op == LIR_ldc || op == LIR_ldqc || op == LIR_ldcs;
    }

	// Sun Studio requires explicitly declaring signed int bit-field
	#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
	#define _sign_ signed 
	#else
	#define _sign_
	#endif

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
			_sign_ int32_t	disp:8;
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
            _sign_ int32_t  imm24:24;
        };

		// imm16 form
		struct i_type
		{
			LOpcode			code:8;
			uint32_t		resv:8;  // cobberred during assembly
			_sign_ int32_t  imm16:16;
		};

		// overlay used during code generation ( note that last byte is reserved for allocation )
		struct g_type
		{
			LOpcode			code:8;
			uint32_t		resv:8;   // cobberred during assembly
			uint32_t		unused:16;
		};

		#undef _sign_
		
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

		enum {
			callInfoWords = sizeof(LIns*)/sizeof(u_type)
		};

		uint32_t reference(LIns*) const;
		LIns* deref(int32_t off) const;

	public:
		LIns*		FASTCALL oprnd1() const;
		LIns*		FASTCALL oprnd2() const;
		LIns*		FASTCALL oprnd3() const;

		inline LOpcode	opcode() const	{ return u.code; }
		inline uint8_t	imm8()	 const	{ return c.imm8a; }
		inline uint8_t	imm8b()	 const	{ return c.imm8b; }
		inline int16_t	imm16()	 const	{ return i.imm16; }
		inline int32_t	imm24()	 const	{ return t.imm24; }
		LIns*	ref()	 const;
		int32_t	imm32()	 const;
		inline uint8_t	resv()	 const  { return g.resv; }
        void*	payload() const;
        inline Page*	page()			{ return (Page*) alignTo(this,NJ_PAGE_SIZE); }
        inline int32_t  size() const {
            NanoAssert(isop(LIR_alloc));
            return i.imm16<<2;
        }
        inline void setSize(int32_t bytes) {
            NanoAssert(isop(LIR_alloc) && (bytes&3)==0 && isU16(bytes>>2));
            i.imm16 = bytes>>2;
        }

		LIns* arg(uint32_t i);

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

		uint64_t constvalq() const;
		
		inline void* constvalp() const
		{
        #ifdef AVMPLUS_64BIT
		    return (void*)constvalq();
		#else
		    return (void*)constval();
        #endif      
		}
		
		double constvalf() const;
		bool isCse(const CallInfo *functions) const;
		bool isop(LOpcode o) const { return u.code == o; }
		bool isQuad() const;
		bool isCond() const;
		bool isCmp() const;
		bool isCall() const { return nanojit::isCall(u.code); }
        bool isStore() const { return nanojit::isStore(u.code); }
        bool isLoad() const { return nanojit::isLoad(u.code); }
		bool isGuard() const { return nanojit::isGuard(u.code); }
		// True if the instruction is a 32-bit or smaller constant integer.
		bool isconst() const { return nanojit::isConst(u.code); }
		// True if the instruction is a 32-bit or smaller constant integer and
		// has the value val when treated as a 32-bit signed integer.
		bool isconstval(int32_t val) const;
		// True if the instruction is a constant quad value.
		bool isconstq() const;
		// True if the instruction is a constant pointer value.
		bool isconstp() const;
        bool isTramp() {
            return isop(LIR_neartramp) || isop(LIR_tramp);
        }
		bool isBranch() const {
			return isop(LIR_jt) || isop(LIR_jf) || isop(LIR_j);
		}
		// Set the imm16 member.  Should only be used on instructions that use
		// that.  If you're not sure, you shouldn't be calling it.
		void setimm16(int32_t i);
		void setimm24(int32_t x);
		// Set the resv member.  Should only be used on instructions that use
		// that.  If you're not sure, you shouldn't be calling it.
		void setresv(uint32_t resv);
		// Set the opcode
		void initOpcode(LOpcode);
		// operand-setting methods
		void setOprnd1(LIns*);
		void setOprnd2(LIns*);
		void setOprnd3(LIns*);
        void setDisp(int8_t d);
		void target(LIns* t);
        LIns **targetAddr();
		LIns* getTarget();

        GuardRecord *record();

		inline uint32_t argc() const {
			NanoAssert(isCall());
			return c.imm8b;
		}
		size_t callInsWords() const;
		const CallInfo *callInfo() const;
	};
	typedef LIns*		LInsp;

	typedef struct { LIns* v; LIns i; } LirFarIns;
	typedef struct { int32_t v; LIns i; } LirImm32Ins;
	typedef struct { int32_t v[2]; LIns i; } LirImm64Ins;
	typedef struct { const CallInfo* ci; LIns i; } LirCallIns;
	
	static const uint32_t LIR_FAR_SLOTS	  = sizeof(LirFarIns)/sizeof(LIns); 
	static const uint32_t LIR_CALL_SLOTS = sizeof(LirCallIns)/sizeof(LIns); 
	static const uint32_t LIR_IMM32_SLOTS = sizeof(LirImm32Ins)/sizeof(LIns); 
	static const uint32_t LIR_IMM64_SLOTS = sizeof(LirImm64Ins)/sizeof(LIns); 
	
	bool FASTCALL isCse(LOpcode v);
	bool FASTCALL isCmp(LOpcode v);
	bool FASTCALL isCond(LOpcode v);
    inline bool isRet(LOpcode c) {
        return (c & ~LIR64) == LIR_ret;
    }
    bool FASTCALL isFloat(LOpcode v);
	LIns* FASTCALL callArgN(LInsp i, uint32_t n);
	extern const uint8_t operandCount[];

	class Fragmento;	// @todo remove this ; needed for minbuild for some reason?!?  Should not be compiling this code at all
	class LirFilter;

	// make it a GCObject so we can explicitly delete it early
	class LirWriter : public avmplus::GCObject
	{
	public:
		LirWriter *out;
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
		virtual LInsp insGuard(LOpcode v, LIns *c, LIns *x) {
			return out->insGuard(v, c, x);
		}
		virtual LInsp insBranch(LOpcode v, LInsp condition, LInsp to) {
			return out->insBranch(v, condition, to);
		}
        // arg: 0=first, 1=second, ...
        // kind: 0=arg 1=saved-reg
		virtual LInsp insParam(int32_t arg, int32_t kind) {
			return out->insParam(arg, kind);
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
		virtual LInsp insCall(const CallInfo *call, LInsp args[]) {
			return out->insCall(call, args);
		}
		virtual LInsp insAlloc(int32_t size) {
			return out->insAlloc(size);
		}

		// convenience
	    LIns*		insLoadi(LIns *base, int disp);
	    LIns*		insLoad(LOpcode op, LIns *base, int disp);
	    LIns*		store(LIns* value, LIns* base, int32_t d);
		// Inserts a conditional to execute and branches to execute if
		// the condition is true and false respectively.
	    LIns*		ins_choose(LIns* cond, LIns* iftrue, LIns* iffalse);
	    // Inserts an integer comparison to 0
	    LIns*		ins_eq0(LIns* oprnd1);
		// Inserts a binary operation where the second operand is an
		// integer immediate.
        LIns*       ins2i(LOpcode op, LIns *oprnd1, int32_t);
		LIns*		qjoin(LInsp lo, LInsp hi);
		LIns*		insImmPtr(const void *ptr);
		LIns*		insImmf(double f);
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
        avmplus::AvmCore *core;
        LabelMap(avmplus::AvmCore *, LabelMap* parent);
        ~LabelMap();
        void add(const void *p, size_t size, size_t align, const char *name);
		void add(const void *p, size_t size, size_t align, avmplus::String*);
		const char *dup(const char *);
		const char *format(const void *p);
		void promoteAll(const void *newbase);
    };

	class LirNameMap MMGC_SUBCLASS_DECL
	{
		template <class Key>
		class CountMap: public avmplus::SortedMap<Key, int, avmplus::LIST_NonGCObjects> {
		public:
			CountMap(avmplus::GC*gc) : avmplus::SortedMap<Key, int, avmplus::LIST_NonGCObjects>(gc) {}
			int add(Key k) {
				int c = 1;
				if (containsKey(k)) {
					c = 1+get(k);
				}
				put(k,c);
				return c;
			}
		};
		CountMap<int> lircounts;
		CountMap<const CallInfo *> funccounts;

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

		LirNameMap(avmplus::GC *gc, const CallInfo *_functions, LabelMap *r) 
			: lircounts(gc),
			funccounts(gc),
			names(gc),
			_functions(_functions),
			labels(r)
		{}
        ~LirNameMap();

		void addName(LInsp i, const char *s);
		bool addName(LInsp i, avmplus::String *s);
		void copyName(LInsp i, const char *s, int suffix);
        const char *formatRef(LIns *ref);
		const char *formatIns(LInsp i);
		void formatGuard(LInsp i, char *buf);
	};


	class VerboseWriter : public LirWriter
	{
		InsList code;
		DWB(LirNameMap*) names;
    public:
		VerboseWriter(avmplus::GC *gc, LirWriter *out, LirNameMap* names) 
			: LirWriter(out), code(gc), names(names)
		{}

		LInsp add(LInsp i) {
            if (i)
                code.add(i);
			return i;
		}

        LInsp add_flush(LInsp i) {
            if ((i = add(i)) != 0) 
                flush();
            return i;
        }

		void flush()
		{
            int n = code.size();
            if (n) {
			    for (int i=0; i < n; i++)
				    printf("    %s\n",names->formatIns(code[i]));
			    code.clear();
                if (n > 1)
        			printf("\n");
            }
		}

		LIns* insGuard(LOpcode op, LInsp cond, LIns *x) {
			return add_flush(out->insGuard(op,cond,x));
		}

		LIns* insBranch(LOpcode v, LInsp condition, LInsp to) {
			return add_flush(out->insBranch(v, condition, to));
		}

		LIns* ins0(LOpcode v) {
            if (v == LIR_label || v == LIR_start) {
                flush();
            }
			return add(out->ins0(v));
		}

		LIns* ins1(LOpcode v, LInsp a) {
            return isRet(v) ? add_flush(out->ins1(v, a)) : add(out->ins1(v, a));
		}
		LIns* ins2(LOpcode v, LInsp a, LInsp b) {
			return v == LIR_2 ? out->ins2(v,a,b) : add(out->ins2(v, a, b));
		}
		LIns* insCall(const CallInfo *call, LInsp args[]) {
			return add_flush(out->insCall(call, args));
		}
		LIns* insParam(int32_t i, int32_t kind) {
			return add(out->insParam(i, kind));
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
        LIns* insAlloc(int32_t size) {
            return add(out->insAlloc(size));
        }
        LIns* insImm(int32_t imm) {
            return add(out->insImm(imm));
        }
        LIns* insImmq(uint64_t imm) {
            return add(out->insImmq(imm));
        }
    };

#endif

	class ExprFilter: public LirWriter
	{
	public:
		ExprFilter(LirWriter *out) : LirWriter(out) {}
		LIns* ins1(LOpcode v, LIns* a);
	    LIns* ins2(LOpcode v, LIns* a, LIns* b);
		LIns* insGuard(LOpcode, LIns *cond, LIns *);
        LIns* insBranch(LOpcode, LIns *cond, LIns *target);
	};

	// @todo, this could be replaced by a generic HashMap or HashSet, if we had one
	class LInsHashSet
	{
		// must be a power of 2. 
		// don't start too small, or we'll waste time growing and rehashing.
		// don't start too large, will waste memory. 
		static const uint32_t kInitialCap = 64;	

		LInsp *m_list; // explicit WB's are used, no DWB needed.
		uint32_t m_used, m_cap;
		avmplus::GC* m_gc;

		static uint32_t FASTCALL hashcode(LInsp i);
		uint32_t FASTCALL find(LInsp name, uint32_t hash, const LInsp *list, uint32_t cap);
		static bool FASTCALL equals(LInsp a, LInsp b);
		void FASTCALL grow();

	public:

		LInsHashSet(avmplus::GC* gc);
		~LInsHashSet();
		LInsp find32(int32_t a, uint32_t &i);
		LInsp find64(uint64_t a, uint32_t &i);
		LInsp find1(LOpcode v, LInsp a, uint32_t &i);
		LInsp find2(LOpcode v, LInsp a, LInsp b, uint32_t &i);
		LInsp findcall(const CallInfo *call, uint32_t argc, LInsp args[], uint32_t &i);
		LInsp add(LInsp i, uint32_t k);
		void replace(LInsp i);
        void clear();

		static uint32_t FASTCALL hashimm(int32_t);
		static uint32_t FASTCALL hashimmq(uint64_t);
		static uint32_t FASTCALL hash1(LOpcode v, LInsp);
		static uint32_t FASTCALL hash2(LOpcode v, LInsp, LInsp);
		static uint32_t FASTCALL hashcall(const CallInfo *call, uint32_t argc, LInsp args[]);
	};

	class CseFilter: public LirWriter
	{
	public:
		LInsHashSet exprs;
		CseFilter(LirWriter *out, avmplus::GC *gc);
	    LIns* insImm(int32_t imm);
	    LIns* insImmq(uint64_t q);
	    LIns* ins0(LOpcode v);
		LIns* ins1(LOpcode v, LInsp);
		LIns* ins2(LOpcode v, LInsp, LInsp);
		LIns* insLoad(LOpcode v, LInsp b, LInsp d);
		LIns* insCall(const CallInfo *call, LInsp args[]);
		LIns* insGuard(LOpcode op, LInsp cond, LIns *x);
	};

	class LirBuffer : public avmplus::GCFinalizedObject
	{
		public:
			DWB(Fragmento*)		_frago;
			LirBuffer(Fragmento* frago, const CallInfo* functions);
			virtual ~LirBuffer();
			void        clear();
            void        rewind();
			LInsp		next();
			bool		outOMem() { return _noMem != 0; }
			
			debug_only (void validate() const;)
			verbose_only(DWB(LirNameMap*) names;)
			
			int32_t insCount();
			int32_t byteCount();

			// stats
			struct 
			{
				uint32_t lir;	// # instructions
			}
			_stats;

			const CallInfo* _functions;
            AbiKind abi;
            LInsp state,param1,sp,rp;
            LInsp savedRegs[NumSavedRegs];
            bool explicitSavedRegs;
			
		protected:
			friend class LirBufWriter;

			LInsp		commit(uint32_t count);
			Page*		pageAlloc();

			PageList	_pages;
			Page*		_nextPage; // allocated in preperation of a needing to growing the buffer
			LInsp		_unused;	// next unused instruction slot
			int			_noMem;		// set if ran out of memory when writing to buffer
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
			LInsp	insParam(int32_t i, int32_t kind);
			LInsp	insImm(int32_t imm);
			LInsp	insImmq(uint64_t imm);
		    LInsp	insCall(const CallInfo *call, LInsp args[]);
			LInsp	insGuard(LOpcode op, LInsp cond, LIns *x);
			LInsp	insBranch(LOpcode v, LInsp condition, LInsp to);
            LInsp   insAlloc(int32_t size);

			// buffer mgmt
			LInsp	skip(size_t);

		protected:
			LInsp	insFar(LOpcode op, LInsp target);
			void	ensureRoom(uint32_t count);
			bool	can8bReach(LInsp from, LInsp to) { return isU8(from-to-1); }
			bool	can24bReach(LInsp from, LInsp to){ return isS24(from-to); }
			void	prepFor(LInsp& i1, LInsp& i2, LInsp& i3);
			void	makeReachable(LInsp& o, LInsp from);
			
		private:
			LInsp	insLinkTo(LOpcode op, LInsp to);     // does NOT call ensureRoom() 
			LInsp	insLinkToFar(LOpcode op, LInsp to);  // does NOT call ensureRoom()
	};

	class LirFilter
	{
	public:
		LirFilter *in;
		LirFilter(LirFilter *in) : in(in) {}
        virtual ~LirFilter(){}

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
        void setpos(LIns *i) {
            _i = i;
        }
	};

    class Assembler;

    void compile(Assembler *assm, Fragment *frag);
	verbose_only(void live(avmplus::GC *gc, LirBuffer *lirbuf);)

	class StackFilter: public LirFilter
	{
	    avmplus::GC *gc;
		LirBuffer *lirbuf;
		LInsp sp;
		avmplus::BitSet stk;
        int top;
		int getTop(LInsp br);
	public:
	    StackFilter(LirFilter *in, avmplus::GC *gc, LirBuffer *lirbuf, LInsp sp); 
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

    // eliminate redundant loads by watching for stores & mutator calls
    class LoadFilter: public LirWriter
    {
    public:
        LInsp sp, rp;
        LInsHashSet exprs;
        void clear(LInsp p);
    public:
        LoadFilter(LirWriter *out, avmplus::GC *gc)
            : LirWriter(out), exprs(gc) { }

        LInsp ins0(LOpcode);
        LInsp insLoad(LOpcode, LInsp base, LInsp disp);
        LInsp insStore(LInsp v, LInsp b, LInsp d);
        LInsp insStorei(LInsp v, LInsp b, int32_t d);
        LInsp insCall(const CallInfo *call, LInsp args[]);
    };	
}
#endif // __nanojit_LIR__
