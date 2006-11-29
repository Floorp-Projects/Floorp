/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#ifndef __avmplus_CodegenMIR__
#define __avmplus_CodegenMIR__


namespace avmplus
{
	// bit insert and extraction macros.  
	// usage   v - variable, h - high bit number. l - lower bit number, q qunatity to insert
	// E.g.  pull out bits 6..2 of a variable BIT_EXTRACT(v,6,2).... 0xxx xx00 
	// wanring bit insert does not mask off upper bits of q, so you must make sure that
	// the value of q is legal given h and l.
	// BIT_VALUE_FIT can be used to determine if a value can be represeted by the
	// specified number of bits.
	#define BIT_INSERT(v,h,l,q)    ( ( ( ((unsigned)v)&~(((1L<<((h+1)-(l))) - 1L)<<(l)) ) | ((q)<<(l)) ) )
	#define BIT_EXTRACT(v,h,l)     ( ( ((unsigned)v)&(((1L<<((h+1)-(l))) - 1L)<<(l)) ) >>(l) )
	#define BIT_VALUE_FITS(v,n)    ( BIT_EXTRACT(v,n-1,0) == (v) )

	// rounding v up to the given 2^ quantity
	#define BIT_ROUND_UP(v,q)      ( (((uintptr)v)+(q)-1) & ~((q)-1) )

	/**
	 * The CodegenMIR class is a dynamic code generator which translates
	 * AVM+ bytecodes into an architecture neutral intermediate representation
	 * suitable for common subexpression elimination, inlining, and register
	 * allocation.
	 */
	class CodegenMIR
	#ifdef AVMPLUS_ARM
		: public ArmAssembler
	#endif
	{
		AvmCore * const core;
		PoolObject * const pool;
		MethodInfo * const info;
		FrameState *state;

		#ifdef AVMPLUS_MAC_CARBON
		static int setjmpDummy(jmp_buf buf);
		static int setjmpAddress;
		static void setjmpInit();
		#endif

	public:

		/** set to true if the no more memory. */
		bool overflow;

		static const int MIR_float = 0x20;		// result is double
		static const int MIR_oper  = 0x40;		// eligible for cse
		enum MirOpcode 
		{
			MIR_bb      = 2,
			MIR_jmpt	= 3,				// offset, size
			MIR_cm		= 4,				// count, imm32  - call method
			MIR_cs		= 5,				// count, imm32  - call static
			MIR_ci		= 6,				// count, ptr	 - call indirect
			MIR_icmp 	= 7,				// ptr, imm32
			MIR_ucmp	= 8,				// ptr, imm32
			MIR_fcmp	= 9,				// ptr, imm32
			MIR_jeq		= 10,				// ptr, imm32
			MIR_jne		= 11,				// ptr, imm32
			MIR_ret		= 12,				// ptr
			MIR_jmp		= 13,				// ptr
			MIR_jmpi	= 14,				// ptr, disp
			MIR_st		= 15,				// ptr, disp, ptr
			MIR_arg     = 16,				// pos | reg - defines methods incoming arguments
			MIR_def		= 17,				// 
			MIR_use		= 18,				// 
			MIR_usea	= 19,				// imm32
			MIR_alloc   = 20,
			//          = 21,
			MIR_ld      = 22,				// non-optimizable load
			MIR_jlt     = 23,
			MIR_jle		= 24,
			MIR_jnlt	= 25,
			MIR_jnle	= 26,

			MIR_imm		= 1  | MIR_oper,	// 0,imm32
			MIR_imul	= 2  | MIR_oper,
			MIR_neg		= 3  | MIR_oper,	// ptr, ptr
			MIR_cmop    = 4  | MIR_oper,    // MIR_cm|oper, call method w/out side effects
			MIR_csop    = 5  | MIR_oper,    // MIR_cs|oper, call static method w/out side effects
			MIR_lsh		= 6  | MIR_oper,	// <<
			MIR_rsh		= 7  | MIR_oper,	// >>
			MIR_ush		= 8  | MIR_oper,	// >>>
			MIR_and		= 9  | MIR_oper,
			MIR_or		= 10 | MIR_oper,
			MIR_xor		= 11 | MIR_oper,
			MIR_add		= 12 | MIR_oper,
			MIR_sub		= 13 | MIR_oper,	// ptr, ptr
			MIR_eq		= 14 | MIR_oper,
			MIR_le		= 15 | MIR_oper,
			MIR_lt		= 16 | MIR_oper,
			MIR_ne		= 17 | MIR_oper,
			MIR_lea		= 18 | MIR_oper,	// ptr, disp

			MIR_ldop    = 22 | MIR_oper,    // ptr, disp (optimizable)

			// After this point are all instructions that return a double-sized
			// result.
			MIR_fcm		= 4  | MIR_float,	// count, imm32 - call method, float return
			MIR_fcs		= 5  | MIR_float,	// count, imm32 - call static, float return
			MIR_fci		= 6	 | MIR_float,	// count, addr - call indirect, float return

			MIR_fdef    = 17 | MIR_float,   // defines value of variable as floating point
			MIR_fuse    = 18 | MIR_float,   // 

			MIR_fld		= 22 | MIR_float,	// float load

			MIR_i2d		= 1  | MIR_float | MIR_oper,	// ptr
			MIR_fneg	= 2  | MIR_float | MIR_oper,	// ptr
			MIR_u2d		= 3  | MIR_float | MIR_oper,
			MIR_fcmop   = 4  | MIR_float | MIR_oper,
			MIR_fcsop   = 5  | MIR_float | MIR_oper,
			MIR_fadd	= 6  | MIR_float | MIR_oper,	// ptr, ptr
			MIR_fsub	= 7  | MIR_float | MIR_oper,	// ptr, ptr
			MIR_fmul	= 8  | MIR_float | MIR_oper,	// ptr, ptr
			MIR_fdiv	= 9  | MIR_float | MIR_oper,	// ptr, ptr
			
			MIR_fldop   = 22 | MIR_float | MIR_oper,	// ptr, disp (optimizable load)

			MIR_last	= 23 | MIR_float | MIR_oper // highest ordinal value possible
		};

		enum ArgNumber {
			_env = 0,
			_argc = 1, 
			_ap = 2
		};
		// ia32 offset = 4*ArgNumber+4

		/**
		 * Constructor.  Initializes the generator for building a method
		 */
		CodegenMIR(MethodInfo* info);

		/**
		 * Constructor.  Initializes the generator for native method thunks only.
		 */
		CodegenMIR(NativeMethod* m);

		/**
		 * Constructor for use with emitImtThunk
		 */
		CodegenMIR(PoolObject* pool);

		~CodegenMIR();

		/**
		 * Generates code for the method info.  The bytecode is translated
		 * into native machine code and the TURBO flag is set
		 * on the MethodInfo.  The original bytecode is retained for debugging.
		 * @param info method to compile
		 */
		void epilogue(FrameState* state);
		bool prologue(FrameState* state);
		void emitCall(FrameState* state, AbcOpcode opcode, int method_id, int argc, Traits* result);
		void emit(FrameState* state, AbcOpcode opcode, uintptr op1=0, uintptr op2=0, Traits* result=NULL);
		void emitIf(FrameState* state, AbcOpcode opcode, int target, int lhs, int rhs);
		void emitSwap(FrameState* state, int i, int j);
		void emitCopy(FrameState* state, int src, int dest);
		void emitGetscope(FrameState* state, int scope, int dest);
		void emitKill(FrameState* state, int i);
		void emitBlockStart(FrameState* state);
		void emitBlockEnd(FrameState* state);
		void emitIntConst(FrameState* state, int index, uintptr c);
		void emitDoubleConst(FrameState* state, int index, double* pd);
		void emitCoerce(FrameState* state, int index, Traits* type);
		void emitCheckNull(FrameState* state, int index);
		void emitSetContext(FrameState* state, AbstractFunction* f);
		void emitSetDxns(FrameState* state);
		void merge(const Value& current, Value& target);

#ifdef AVMPLUS_VERBOSE
		bool verbose();
#endif

#ifndef FEATURE_BUFFER_GUARD
		bool checkOverflow();
#endif

		/**
		 * Generates code for a native method thunk.
		 */
		void emitNativeThunk(NativeMethod* info);
		void* emitImtThunk(ImtBuilder::ImtEntry *e);

	private:

		void emitPrep(AbcOpcode opcode);

		const byte *abcStart;
		const byte *abcEnd;

		//
		// helpers called by generated code
		//
		static Atom coerce_o(Atom v);

		#ifdef AVMPLUS_ARM
		//
		// ARM needs helpers for floating point
		//
		static double fadd(double x, double y);
		static double fsub(double x, double y);
		static double fmul(double x, double y);
		static double fdiv(double x, double y);
		static int    fcmp(double x, double y);
		static double i2d(int i);
		static double u2d(uint32 i);
		#endif
	
	public:
		//
		// Register allocation stuff follows
		// Encoded in 7b quantity
		//

		#ifdef AVMPLUS_PPC

		#ifdef AVMPLUS_VERBOSE
		static const char *gpregNames[];
		static const char *fpregNames[];
		#endif
		
		typedef enum {
			CR0 = 0,
			CR1 = 1,
			CR2 = 2,
			CR3 = 3,			
			CR4 = 4,
			CR5 = 5,
			CR6 = 6,
			CR7 = 7
		} ConditionRegister;

		// These are used as register numbers in various parts of the code
		static const int MAX_REGISTERS = 32;
		enum
		{
			// general purpose 32bit regs
			R0  = 0, // scratch or the value 0
			SP  = 1, // stack pointer
			RTOC= 2, // toc pointer
			R3  = 3, // this, return value
			R4  = 4,
			R5  = 5,
			R6  = 6,
			R7  = 7,
			R8  = 8,
			R9  = 9,
			R10 = 10,						
			R11 = 11,						
			R12 = 12,						
			R13 = 13,						
			R14 = 14,						
			R15 = 15,						
			R16 = 16,						
			R17 = 17,						
			R18 = 18,						
			R19 = 19,						
			R20 = 20,						
			R21 = 21,						
			R22 = 22,						
			R23 = 23,						
			R24 = 24,						
			R25 = 25,						
			R26 = 26,						
			R27 = 27,						
			R28 = 28,						
			R29 = 29,						
			R30 = 30,
			R31 = 31,			

			// FP regs
			F0  = 0,
			F1  = 1,
			F2  = 2,
			F3  = 3,
			F4  = 4,
			F5  = 5,
			F6  = 6,
			F7  = 7,
			F8  = 8,
			F9  = 9,
			F10 = 10,						
			F11 = 11,						
			F12 = 12,						
			F13 = 13,						
			F14 = 14,						
			F15 = 15,						
			F16 = 16,						
			F17 = 17,						
			F18 = 18,						
			F19 = 19,						
			F20 = 20,						
			F21 = 21,						
			F22 = 22,						
			F23 = 23,						
			F24 = 24,						
			F25 = 25,						
			F26 = 26,						
			F27 = 27,						
			F28 = 28,						
			F29 = 29,						
			F30 = 30,
			F31 = 31,

			// special purpose registers (SPR)
			LR  = 8,
			CTR = 9,

			Unknown = 0x7f
		};
		typedef unsigned Register;
		#endif

		#ifdef AVMPLUS_ARM
		static const int MAX_REGISTERS = 16;
        #endif
		
		#ifdef AVMPLUS_IA32

		#ifdef AVMPLUS_VERBOSE
		static const char *gpregNames[];
		static const char *xmmregNames[];
		static const char *x87regNames[];
		#endif

		#ifdef _MAC
		byte *patch_esp_padding;
		#endif

		// These are used as register numbers in various parts of the code
		static const int MAX_REGISTERS = 8;

		typedef enum
		{
			// general purpose 32bit regs
			EAX = 0, // return value, scratch
			ECX = 1, // this, scratch
			EDX = 2, // scratch
			EBX = 3,
			ESP = 4, // stack pointer
			EBP = 5, // frame pointer
			ESI = 6,
			EDI = 7,

			SP = 4, // alias SP to ESP to match PPC name
 
			// SSE regs
			XMM0 = 0,
			XMM1 = 1,
			XMM2 = 2,
			XMM3 = 3,
			XMM4 = 4,
			XMM5 = 5,
			XMM6 = 6,
			XMM7 = 7,

			// X87 regs
			FST0 = 0,
			FST1 = 1,
			FST2 = 2,
			FST3 = 3,
			FST4 = 4,
			FST5 = 5,
			FST6 = 6,
			FST7 = 7,

			Unknown = -1
		} 
		Register;
		#endif

		#if defined (AVMPLUS_AMD64)

		#ifdef AVMPLUS_VERBOSE
		static const char *gpregNames[];
		static const char *xmmregNames[];
		static const char *x87regNames[];
		#endif

		#ifdef _MAC
		byte *patch_esp_padding;
		#endif

		// These are used as register numbers in various parts of the code
		static const int MAX_REGISTERS = 16;

		typedef enum
		{
			// 64bit - at this point, I'm not sure if we'll be using EAX, etc as registers
			// or whether this numbering scheme is correct.  For some generated ASM, usage
			// of RAX implies a REX prefix byte to define the 64-bit operand.  But the ModRM
			// byte stays the same (EAX=RAX in the encoding)
			
			// general purpose 32bit regs
			EAX = 0, // return value, scratch
			ECX = 1, // this, scratch
			EDX = 2, // scratch
			EBX = 3,
			ESP = 4, // stack pointer
			EBP = 5, // frame pointer
			ESI = 6,
			EDI = 7,
			
			RAX = 8, // return value, scratch
			RCX = 9, // this, scratch
			RDX = 10, // scratch
			RBX = 11,
			RSP = 12, // stack pointer
			RBP = 13, // frame pointer
			RSI = 14,
			RDI = 15,
			
			R8 = 16,
			R9 = 17,
			R10 = 18,
			R11 = 19,
			R12 = 20,
			R13 = 21,
			R14 = 22,
			R15 = 23,

			SP = 12, // alias SP to RSP to match PPC name
 
			// SSE regs
			XMM0 = 0,
			XMM1 = 1,
			XMM2 = 2,
			XMM3 = 3,
			XMM4 = 4,
			XMM5 = 5,
			XMM6 = 6,
			XMM7 = 7,

			// !!@ remove all float support?  SSE2 is always available
			// X87 regs
			FST0 = 0,
			FST1 = 1,
			FST2 = 2,
			FST3 = 3,
			FST4 = 4,
			FST5 = 5,
			FST6 = 6,
			FST7 = 7,

			Unknown = -1
		} 
		Register;
		#endif // AVMPLUS_AMD64
		
		#define rmask(r) (1 << (r))

		/**
		 * MIR instruction
		 * 
		 * This structure contains the definition of the instruction format.
		 * Generally instructions take 1 or 2 operands.  MIR_st is the
		 * the ONLY instruction that takes 3 operands.    The form
		 * of the operands (or type) is instruction dependent.  Each instruction 
		 * expects the operands to be in a specific form.  For example, MIR_add
		 * expects 2 operands which are pointers to other instructions.  Debug 
		 * builds will check operand types for conformance, but Release builds 
		 * will have no type checking; consider yourself warned!
		 * 
		 * During MIR generation we keep track of various details about the use
		 * of the instruction.  Such as whether its result is needed across a 
		 * function call and which instruction last uses the result of each
		 * instruction; lastUse.  The lastUse field of every instruction 
		 * is updated as instructions are generated.  In the case of MIR_st
		 * no result is generated and thus lastUse can be overlayed with oprnd3.
		 * 
		 * During machine dependent code generation oprnd2 is clobbered and
		 * the current register / stack position is maintained within this field.
		 */

		class OP
		{
		public:

			/** opcode */
			MirOpcode code:8;

			/** register assigned to this expr, or Unknown */
			Register reg:7;	

			/** flag indicating if result is used after call, so we prefer callee-saved regs */
			uint32 liveAcrossCall:1;

			/** link to a previous expr with the same opcode, for finding CSE's */
			uint32 prevcse:16;

			union
			{
				OP* oprnd1;			// 1st operand of instruction
				OP* base;			// base ptr for load/store/lea/jmpi
				uint32 argc;		// arg count, for calls
				int pos;			// position of spilled value, or position of label
			};

			union
			{
				OP* oprnd2;			// 2nd operand of instruction	
				int32 addr;			// call addr or pc addr
				int32 size;			// alloca size, table size
				int32 imm;			// imm value if const
				int disp;			// immediate signed displacement for load/store/lea/jmpi
				OP* target;			// branch target
				OP* join;			// def joined to
				uint32 *nextPatch;
			};

			union
			{
				OP* value;			// value to store in MIR_st
				OP* lastUse;		// OP that uses this expr furthest in the future (end of live range)
				OP* args[1];		// arg array, for call ops.  arg[1] is the
									// first arg, which will run over into the 
									// space of subsequent ops.
			};

			int isDouble() const {
				return code & MIR_float;
			}

			int spillSize() const {
				return code == MIR_alloc ? size : (code & MIR_float) ? 8 : 4;
			}

			int isDirty() const {
				return code != MIR_imm && pos == InvalidPos;
			}

		};

		class MirLabel
		{
		public:
			OP* bb;
			OP** nextPatchIns;
			MirLabel() {
				bb = 0;
				nextPatchIns = 0;
			}
		};

		class MdLabel
		{
		public:
			int value;
			uint32* nextPatch;	/* linked list of patch locations, where next address is an offset in the instruction */
			MdLabel() {
				value = 0;
				nextPatch = 0;
			}
		};

		#ifdef AVMPLUS_VERBOSE
		void buildFlowGraph();
		static void formatOpcode(PrintWriter& buffer, OP *ipStart, OP* op, PoolObject* pool, MMgc::GCHashtable* names);
	    static void formatInsOperand(PrintWriter& buffer, OP* oprnd, OP* ipStart);
	    static void formatOperand(PrintWriter& buffer, OP* oprnd, OP* ipStart);
		static MMgc::GCHashtable* initMethodNames(AvmCore* core);
		#endif //AVMPLUS_VERBOSE

		void emitMD();

		OP* exAtom;

	private:
		#define COREADDR(f) coreAddr((int (AvmCore::*)())(&f))
		#define GCADDR(f) gcAddr((int (MMgc::GC::*)())(&f))
		#define ENVADDR(f) envAddr((int (MethodEnv::*)())(&f))
		#define TOPLEVELADDR(f) toplevelAddr((int (Toplevel::*)())(&f))
		#define CALLSTACKADDR(f) callStackAddr((int (CallStackNode::*)())(&f))	
		#define SCRIPTADDR(f) scriptAddr((int (ScriptObject::*)())(&f))
		#define ARRAYADDR(f) arrayAddr((int (ArrayObject::*)())(&f))
		#define EFADDR(f)   efAddr((int (ExceptionFrame::*)())(&f))
		#define DEBUGGERADDR(f)   debuggerAddr((int (Debugger::*)())(&f))

		static sintptr coreAddr( int (AvmCore::*f)() );
		static sintptr gcAddr( int (MMgc::GC::*f)() );
		static sintptr envAddr( int (MethodEnv::*f)() );
		static sintptr toplevelAddr( int (Toplevel::*f)() );
	#ifdef DEBUGGER
		static sintptr callStackAddr( int (CallStackNode::*f)() );
		static sintptr debuggerAddr( int (Debugger::*f)() );
	#endif
		static sintptr efAddr( int (ExceptionFrame::*f)() );
		static sintptr scriptAddr( int (ScriptObject::*f)() );
		static sintptr arrayAddr( int (ArrayObject::*f)() );

		friend class Verifier;

		// MIR instruction buffer
		OP* ip;
		OP* ipStart;
		OP* ipEnd;
		uintptr mirBuffSize;
		int expansionFactor;
		GrowableBuffer* mirBuffer;

		byte*	 code;
		uint32*  casePtr;
		int		 case_count;

		#ifdef AVMPLUS_PPC
		typedef uint32 MDInstruction;
		#define PREV_MD_INS(m) (m-1)
		#endif

		#ifdef AVMPLUS_ARM
		#define PREV_MD_INS(m) (m-1)
		#endif
		
		#ifdef AVMPLUS_IA32
		typedef byte MDInstruction;
		#define PREV_MD_INS(m) (m-4)
		// for intel and our purposes previous instruction is 4 bytes prior to m; used for patching 32bit target addresses
		#endif

		// 64bit - need to verify
		#ifdef AVMPLUS_AMD64
		typedef byte MDInstruction;
		#define PREV_MD_INS(m) (m-4)
		// for intel and our purposes previous instruction is 4 bytes prior to m; used for patching 32bit target addresses
		#endif
		
		// machine specific instruction buffer
		#ifndef AVMPLUS_ARM
		MDInstruction* mip;
		MDInstruction* mipStart;
		#endif

		uint32 arg_index;

		void	mirLabel(MirLabel& l, OP* bb); 
		void	mirPatchPtr(OP** targetp, int pc);		/* patch the location 'where' with the 32b value of the label */
		void	mirPatchPtr(OP** targetp, MirLabel& l);
		void	mirPatch(OP* i, int pc);

		void	mdLabel(MdLabel* l, void* v);			/* machine specific version of position label (will trigger patching) */
		void	mdLabel(OP* l, void* v);			/* machine specific version of position label (will trigger patching) */
		void	mdPatch(void* where, MdLabel* l);		/* machine specific version for patch the location 'where' with the 32b value of the label */
		void	mdPatch(void* where, OP* l);		/* machine specific version for patch the location 'where' with the 32b value of the label */
		void	mdApplyPatch(uint32* where, int labelvalue); /* patch label->value into where */

		void    mdPatchPreviousOp(OP* ins) {
			mdPatch( PREV_MD_INS(mip), ins );
		}

		void    mdPatchPrevious(MdLabel* l) {
			mdPatch( PREV_MD_INS(mip), l );
		}

		class StackCheck
		{
		public:
			uint32 *patchStackSize;
			MdLabel  resumeLabel;
			MdLabel  overflowLabel;

			StackCheck() {
				patchStackSize = NULL;
			}
		};

		StackCheck stackCheck;
		
		MdLabel  patch_end;
		
		MirLabel npe_label;
		MirLabel interrupt_label;
		bool interruptable;
		
		#if defined (AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
		// stack probe for win32
		void emitAllocaProbe(int growthAmt, MdLabel* returnTo);
		#endif /* AVMPLUS_IA32 or AVMPLUS_AMD64 */

		#ifdef AVMPLUS_ARM
		uint32 *patch_stmfd;
		uint32 *patch_frame_size;

		/**
		 * Compute the size of the ARM callee area.  Must be done
		 * after the generation of MIR opcodes.
		 */
		int calleeAreaSize() const { return 8*maxArgCount; }

		int countBits(uint32);
        #endif
		
		#ifdef AVMPLUS_PPC
		uint32 *patch_stmw;
		uint32 *patch_stwu;

		/**
		 * Compute the size of the PPC callee area.  Must be done
		 * after the generation of MIR opcodes.
		 */
		int calleeAreaSize() const { return 8*maxArgCount; }
		
		// This helper exists on PPC only to minimize the code size
		// of the generated code for stack overflow checks
		static void stackOverflow(MethodEnv *env);
	    #endif
		
		uint32  maxArgCount;        // most number of arguments used in a call

		#ifdef AVMPLUS_PROFILE
		// register allocator stats
		int     fullyUsedCount;     // number of times all registers fully used
		int     longestSpan;        // most number of instructions that a register is used
		int		spills;			    // number of spills required
		int     steals;				// number of spills due to a register being stolen.
		int		remats;				// number of rematerializations 

		// profiler stats	 
		uint64  verifyStartTime;	// note the time we started verification
		uint64  mdStartTime;		// note the time we started MD generation

		#ifndef AVMPLUS_ARM
		int		mInstructionCount;  // number of machine instructions
		#define incInstructionCount() mInstructionCount++
		#endif

		#ifdef _DEBUG
		// buffer tuning information
		enum { SZ_ABC, SZ_MIR, SZ_MD, SZ_MIRWASTE, SZ_MDWASTE, SZ_MIREPI, SZ_MDEPI, SZ_MIRPRO, SZ_MDPRO, SZ_MIREXP, SZ_MDEXP, SZ_LAST };
		double sizingStats[SZ_LAST];
		#endif /* _DEBUG */

		#else

		#define incInstructionCount()
		
		#endif /* AVMPLUS_PROFILE */

		// pointer to list of argument definitions
		OP* methodArgs; 
		OP* calleeVars;
		Register framep;

		// stack space reserved by MIR_alloca instruction in prologue for locals, exception frame and Multiname
		OP*  _save_eip;
		OP*  _ef;
		OP*  dxns;
		OP*  dxnsAddrSave; // methods that set dxns need to save/restore

		#ifdef DEBUGGER
		OP*  localPtrs;		// array of local_count + max_scope (holds locals and scopes)
		OP*  localTraits;	// array of local_count (holds snapshot of Traits* per local)
		OP*  _callStackNode;
		#endif

		// track last function call issued
		OP* lastFunctionCall;

		// track last pc value we generated a store for
		int lastPcSave;

		// cse table which prevents duplicate instructions in the same bb
		OP* cseTable[MIR_last];
		OP* firstCse;
#ifdef AVMPLUS_PROFILE
		int cseHits;		// measure effectiveness of cse optimizer
		#define incCseHits() cseHits++
		int dceHits;		// measure effectiveness of dead code eliminator
		#define incDceHits() dceHits++
#else
		#define incCseHits()
		#define incDceHits()
#endif

#ifdef DEBUGGER
		void extendDefLifetime(OP* current);
#endif

		void saveState();

		OP* defIns(OP* v);
		OP* useIns(OP* def, int i);

		OP* undefConst;

		// frame state helpers
		OP*		localGet(uint32 i);
		void	localSet(uint32 i, OP* o);
		OP*		loadAtomRep(uint32 i);

		OP*	  InsAt(int nbr)  { return ipStart+nbr; }
		int	  InsNbr(OP* ins)	 { AvmAssert(ins >= ipStart); return (ins-ipStart); }
		OP*   InsConst(uintptr value) { return Ins(MIR_imm, value); }
		OP*   InsAlloc(size_t s) { return Ins(MIR_alloc, (int32)s); }
		void  InsDealloc(OP* alloc);
		OP*   ldargIns(ArgNumber arg) { return &methodArgs[arg]; }

		OP*   Ins(MirOpcode code, uintptr v=0);
		OP*   Ins(MirOpcode code, OP* a1, uintptr a2);
		OP*   Ins(MirOpcode code, OP* a1, OP* a2=0);
		OP*	  defineArgInsPos(int spOffset);
		OP*	  defineArgInsReg(Register r);
		OP*   binaryIns(MirOpcode code, OP* a1, OP* a2);
		
		OP*   loadIns(MirOpcode _code, int _disp, OP* _base)
		{
			AvmAssert((_code & ~MIR_float & ~MIR_oper) == MIR_ld);
			return Ins(_code, _base, (int32)_disp);
		}

		OP*   cmpOptimization (int lhs, int rhs);

		OP*   i2dIns(OP* v);
		OP*   u2dIns(OP* v);
		OP*   fcmpIns(OP* a1, OP* a2);
		OP*   binaryFcmpIns(OP* a1, OP* a2);

		OP*   cmpLt(int lhs, int rhs);
		OP*   cmpLe(int lhs, int rhs);
		OP*	  cmpEq(int funcaddr, int lhs, int rhs);

		void  storeIns(OP* v, uintptr disp, OP* base);

		OP*   leaIns(int disp, OP* base);
		OP*   callIns(int32 addr, uint32 argCount, MirOpcode code);
		OP*   callIndirect(MirOpcode code, OP* target, uint32 argCount, ...);
		OP*   callIns(MirOpcode code, int32 addr, uint32 argCount, ...);
		OP*   promoteNumberIns(Traits* t, int i);

		OP*   loadVTable(int base_index);
		OP*   loadToplevel(OP* env);

		// simple cse within BBs
		OP*   cseMatch(MirOpcode code, OP* a1, OP* a2=0);

		// dead code search
		void markDead(OP* ins);
		bool usedInState(OP* ins);

		void argIns(OP* a);

		OP*  storeAtomArgs(int count, int startAt);
		OP*  storeAtomArgs(OP* receiver, int count, int startAt);

		void updateUse(OP* currentIns, OP* op, Register hint=Unknown);

		void extendLastUse(OP* use, int targetpc);
		void extendLastUse(OP* ins, OP* use, OP* target);

		OP*  atomToNativeRep(Traits* t, OP* atom);
		OP*  atomToNativeRep(int i, OP* atom);
		bool isPointer(int i);
		bool isDouble(int i);
		OP*  ptrToNativeRep(Traits*, OP* ptr);

		OP*  initMultiname(Multiname* multiname, int& csp, bool isDelete = false);

#ifdef DEBUGGER
		void emitSampleCheck();
#endif

		//
		// -- MD Specific stuff
		//
		void generate();
		void generatePrologue();
		void generateEpilogue();
		void bindMethod(AbstractFunction* f);
#ifdef AVMPLUS_JIT_READONLY
		void makeCodeExecutable();
#endif /* AVMPLUS_JIT_READONLY */

		bool	ensureMDBufferCapacity(PoolObject* pool, size_t s);  // only if buffer guard is not used
		byte*	getMDBuffer(PoolObject* pool);	// 
		size_t	estimateMDBufferReservation(PoolObject* pool);  // 

		/**
		 * Information about the activation record for the method is built up 
		 * as we generate machine code.  As part of the prologue, we issue
		 * a stack adjustment instruction and then later patch the adjustment
		 * value.  Temporary values can be placed into the AR as method calls
		 * are issued.   Also MIR_alloca instructions will consume space.
		 */
		class AR
		{
		public:
			AR(MMgc::GC *gc) : temps(gc) {}
			List<OP*,LIST_NonGCObjects>		temps;				/* list of active temps */
			int				size;				/* current # of bytes consumed by the temps */
			int				highwatermark;		/* max size of temps */
			MDInstruction*	adjustmentIns;		/* AR sizing instruction to patch */
		};

		AR activation;

		static const int InvalidPos = -1;  /* invalid spill position  */

		void reserveStackSpace(OP* ins);

		#ifdef AVMPLUS_VERBOSE
		void displayStackTable();
		#endif //AVMPLUS_VERBOSE

    	#ifdef AVMPLUS_PPC
	    static const int kLinkageAreaSize = 24;
    	#endif

		// converts an instructions 'pos' field to a stack pointer relative displacement
		int stackPos(OP* ins);

		// structures for register allocator
		class RegInfo 
		{
		public:
			uint32 free;
			uint32 calleeSaved; // 1 = callee-saved, 0=caller-saved
#ifdef AVMPLUS_PPC
			unsigned LowerBound;
#endif
#ifdef AVMPLUS_ARM
			unsigned nonVolatileMask;
#endif
			OP* active[MAX_REGISTERS];  // active[r] = OP that defines r

			OP* findLastActive(int set);
			void flushCallerActives(uint32 flushmask);

			RegInfo() 
			{
				clear();
			}

			void clear()
			{
				free = 0;
				memset(active, 0, MAX_REGISTERS * sizeof(OP*));
			}

			uint32 isFree(Register r) 
			{
				return free & rmask(r);
			}

			/**
			* Add a register to the free list for the allocator.
			*
			*   IMPORTANT
			* 
			*  This is a necessary call when freeing a register.
			*/
			void addFree(Register r)
			{
				AvmAssert(!isFree(r));
				free |= rmask(r);
			}

			void removeFree(Register r)
			{
				AvmAssert(isFree(r));
				free &= ~rmask(r);
			}

			// if an instruction is no longer in use retire it
			void expire(OP* ins, OP* currentIns)
			{
				if (ins->reg != Unknown && ins->lastUse <= currentIns)
					retire(ins);
			}

			void expireAll(OP* currentIns)
			{
				for(int i=0; i<MAX_REGISTERS; i++)
				{
					OP* ins = active[i];
					if (ins && ins->lastUse <= currentIns)
						retire(ins);
				}
			}

			// instruction gives up the register it is currently bound to, adding it back to the free list,
			// removing it from the active list
			void retire(OP* ins)
			{
				AvmAssert(active[ins->reg] == ins);
				retire(ins->reg);
				ins->reg = Unknown;
			}

			void retire(Register r)
			{
				AvmAssert(r != Unknown);
				AvmAssert(active[r] != NULL);
				active[r] = NULL;
				free |= rmask(r);
			}
			/**
			 * add the register provided in v->reg to the active list
			 * IMPORTANT: this is a necesary call when allocating a register
			 */
			void addActive(OP* v)
			{
				//addActiveCount++;
				AvmAssert(active[v->reg] == NULL);
				active[v->reg] = v;
			}

			/**
			* Remove a register from the active list without
			* spilling its contents.
			* 
			* The caller MUST eventually call addFree()
			* or regs.addActive() in order to add the register
			* back to the freelist or back onto the active list.
			* 
			* It is the responsibility of the caller to update 
			* the reg field of the instruction.
			* 
			* @param r - register to remove from free list
			* @param forAlloctor - DO NOT USE, for allocator only
			* 
			* @return the instruction for which the register
			* was retaining a value.
			*/
			void removeActive(Register r)
			{
				//registerReleaseCount++;
				AvmAssert(r != Unknown);
				AvmAssert(active[r] != NULL);

				// remove the given register from the active list
				active[r] = NULL;
			}

			#ifdef _DEBUG
			int count;
			int countFree()
			{
				int _count = 0;
				for(int i=0;i<MAX_REGISTERS; i++)
					_count += ( (free & rmask(i)) == 0) ? 0 : 1;
				return _count;
			}
			int countActive()
			{
				int _count = 0;
				for(int i=0;i<MAX_REGISTERS; i++)
					_count += (active[i] == 0) ? 0 : 1;
				return _count;
			}

			void checkCount()
			{
				AvmAssert(count == (countActive() + countFree()));
			}
			void checkActives(OP* current);
			#endif //_DEBUG

		};

		// all registers that we use
		RegInfo gpregs;
		RegInfo fpregs;

		#ifdef AVMPLUS_VERBOSE
		void showRegisterStats(RegInfo& regs);
		#endif //AVMPLUS_VERBOSE

		// register active/freelist routines
		Register registerAllocSpecific(RegInfo& regs, Register r);
		Register registerAllocAny(RegInfo& regs, OP* ins);
		Register registerAllocFromSet(RegInfo& regs, int set);

		Register registerAlloc(RegInfo& regs, OP* ins, Register r)
		{
			return r == Unknown 
				? registerAllocAny(regs, ins)
				: registerAllocSpecific(regs, r);
		}

		uint32 spillCallerRegisters(OP* ins, RegInfo& regs, uint32 live);
		void spillTmps(OP* target);

		Register InsPrepResult(RegInfo& regs, OP* ins, int exclude=0);

		void setResultReg(RegInfo& regs, OP* ins, Register reg) 
		{
			ins->reg = reg;
			ins->pos = InvalidPos;
			regs.addActive(ins);
		}

		// MD instruction generators for spill/remat
		void spill(OP* ins);
		void rematerialize(OP* ins);
		void copyToStack(OP* ins, Register r);

		// spill and argument code for MD calls
		int prepCallArgsOntoStack(OP* ip, OP* postCall);

		void InsRegisterPrepA(OP* insRes, RegInfo& regsA, OP* insA, Register& reqdA);
		void InsRegisterPrepAB(OP* insRes, RegInfo& regsA, OP* insA, Register& reqdA, RegInfo& regsB, OP* insB, Register& reqdB);

		// prepare the registers in the best possible fashion for issuing the given instruction
		inline void InsPrep_A_IN_REQ_B_IN_WRONG(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);
		inline void InsPrep_A_IN_REQ_B_OUT_REQ(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);
		inline void InsPrep_A_IN_REQ_B_OUT_ANY(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);
#if 0
		inline void InsPrep_A_IN_WRONG_B_IN_WRONG(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);
#endif
		inline void InsPrep_A_IN_WRONG_B_IN_ANY(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);
		inline void InsPrep_A_IN_WRONG_B_OUT(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);
		inline void InsPrep_A_IN_ANY_B_OUT(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);
		inline void InsPrep_A_OUT_B_OUT(RegInfo& regs, OP* insA, Register& reqdA, OP* insB, Register& reqdB);

		void moveR2R(OP* ins, Register src, Register dst);

		// Returns true if immediate folding can be used
		bool canImmFold(OP *ins, OP *imm) const;

		#ifdef AVMPLUS_PPC
		//
		// PowerPC code generation
		//

		void GpBinary(int op, Register dst, Register src1, Register src2);
		void ADD(Register dst, Register src1, Register src2)	{ GpBinary(0x214, dst, src1, src2); }
		void MULLW(Register dst, Register src1, Register src2)	{ GpBinary(0x1D6, dst, src1, src2); }
		void AND(Register dst, Register src1, Register src2)	{ GpBinary(0x038, src1, dst, src2); }
		void SUB(Register dst, Register src1, Register src2)	{ GpBinary(0x050, dst, src2, src1); } // z = x - y is encoded as subf z,y,x 
		void SLW(Register dst, Register src, Register shift)	{ GpBinary(0x030, src, dst, shift); }

		void FpUnary(int op, Register dst, Register src);
		void FMR(Register dst, Register src)	{ FpUnary(0x90, dst, src); }
		void FNEG(Register dst, Register src)	{ FpUnary(0x50, dst, src); }

		void MR(Register dst, Register src, bool dot=false);
		void MR_dot(Register dst, Register src) { MR(dst,src,true); }

		void NEG(Register dst, Register src);
		void LWZ(Register dst, int disp, Register base);
		void LWZX(Register dst, Register rA, Register rB);
		void LFD(Register dst, int disp, Register base);
		void LFDX(Register dst, Register rA, Register rB);
		void STFD(Register src, int disp, Register dst);
		void STFDX(Register src, Register rA, Register rB);
		void LMW(Register dst, int disp, Register base);
		void STW(Register src, int disp, Register base);
		void STWX(Register src, Register rA, Register rB);
		void RLWINM(Register dst, Register src, int shift, int begin, int end);
		void SLWI(Register dst, Register src, int shift);
		void SRW(Register dst, Register src, Register shift);
		void SRWI(Register dst, Register src, int shift);
		void SRAW(Register dst, Register src, Register shift);
		void SRAWI(Register dst, Register src, int shift);
		void ADDI(Register dst, Register src, int imm16);
		void ADDIS(Register dst, Register src, int imm16);
		void SI(Register dst, Register src, int imm16);
		void CMP(ConditionRegister CR, Register regA, Register regB);
		void CMPI(ConditionRegister CR, Register src, int imm16);
		void LI(Register reg, int imm16);
		void LIS(Register reg, int imm16);
		void ORI(Register dst, Register src, int imm16);
		void ORIS(Register dst, Register src, int imm16);
		void ANDI_dot(Register dst, Register src, int imm16);
		void CMPL(ConditionRegister CR, Register regA, Register regB);
		void CMPLI(ConditionRegister CR, Register src, int imm16);
		void FCMPU(ConditionRegister CR, Register regA, Register regB);
		void MFCR(Register dst);
		void CROR(int crbD, int crbA, int crbB);
		void CRNOT(int crbD, int crbA);
		void XORI(Register dst, Register src, int imm16);
		void XORIS(Register dst, Register src, int imm16);
		void XOR(Register dst, Register src1, Register src2);
		void FADD(Register dst, Register src1, Register src2);
		void FSUB(Register dst, Register src1, Register src2);
		void FMUL(Register dst, Register src1, Register src2);
		void FDIV(Register dst, Register src1, Register src2);
		void OR(Register dst, Register src1, Register src2);
		void STMW(Register start, int disp, Register base);
		void STWU(Register dst, int disp, Register base);
		void STWUX(Register rS, Register rA, Register rB);

		void Movspr(int op, Register r, Register spr);
		void MTCTR(Register reg)	{ Movspr(0x7C0003A6, reg, CTR); }
		void MTLR(Register reg)		{ Movspr(0x7C0003A6, reg, LR); }
		void MFLR(Register reg)		{ Movspr(0x7C0002A6, reg, LR); }

		void BR(int op, int addr);
		void B(int offset)	{ BR(0, offset); }
		void BL(int offset) { BR(1, offset); }
		void BA(int addr)	{ BR(2, addr); }
		void BLA(int addr)  { BR(3, addr); }

		void Bcc(int op, ConditionRegister cr, int offset, bool hint=false);
		void BEQ(ConditionRegister cr, int offset, bool hint=false) { Bcc(0x4182, cr, offset, hint); }
		void BNE(ConditionRegister cr, int offset, bool hint=false) { Bcc(0x4082, cr, offset, hint); }
		void BLE(ConditionRegister cr, int offset, bool hint=false) { Bcc(0x4081, cr, offset, hint); }
		void BGT(ConditionRegister cr, int offset, bool hint=false) { Bcc(0x4181, cr, offset, hint); }
		void BLT(ConditionRegister cr, int offset, bool hint=false) { Bcc(0x4180, cr, offset, hint); }
		void BGE(ConditionRegister cr, int offset, bool hint=false) { Bcc(0x4080, cr, offset, hint); }
		void BDNZ(ConditionRegister cr, int offset, bool hint=false){ Bcc(0x4200, cr, offset, hint); }

		void Simple(int op);
		void BLR()		{ Simple(0x4E800020); }
		void BCTR()		{ Simple(0x4E800420); }
		void BCTRL()	{ Simple(0x4E800421); }
		void NOP()		{ Simple(0x60000000); }

		void patchRelativeBranch(uint32 *patch_ip, uint32 *target_ip)
		{
			int offset = (target_ip-patch_ip)*4;
			*patch_ip &= ~0xFFFC;
			*patch_ip |= offset;
		}
		
		// Load and store macros that take 32-bit offsets.
		// R0 will be used for the offset if needed.
		inline bool IsSmallDisplacement(int disp) const
		{
			return (uint32)(disp+0x8000) < 0x10000;
		}
		
		// Returns whether displacement fits in 26-bit
		// branch displacement (B, BL, BLA instructions)
		inline bool IsBranchDisplacement(int disp) const
		{
			return (uint32)(disp+0x2000000) < 0x4000000;
		}

		// Returns true if imm will fit in the SIMM field of
		// a PowerPC instruction (16-bit signed immediate)
		inline bool isSIMM(int imm) const
		{
			return !((imm + 0x8000) & 0xFFFF0000);
		}

		// Returns true if imm will fit in the UIMM field of
		// a PowerPC instruction (16-bit unsigned immediate)
		inline bool isUIMM(int imm) const
		{
			return !(imm & 0xFFFF0000);
		}

		void LWZ32(Register dst, int disp, Register base)
		{
			if (IsSmallDisplacement(disp))
			{
				LWZ(dst, disp, base);
			}
			else
			{
				LI32(R0, disp);
				LWZX(dst, base, R0);
			}
		}
		
		void STW32(Register src, int disp, Register dst)
		{
			if (IsSmallDisplacement(disp))
			{
				STW(src, disp, dst);
			}
			else
			{
				LI32(R0, disp);
				STWX(src, dst, R0);
			}
		}
		
		void LFD32(Register dst, int disp, Register base)
		{
			if (IsSmallDisplacement(disp))
			{
				LFD(dst, disp, base);
			}
			else
			{
				LI32(R0, disp);
				LFDX(dst, base, R0);
			}
		}
		
		void STFD32(Register src, int disp, Register dst)
		{
			if (IsSmallDisplacement(disp))
			{
				STFD(src, disp, dst);
			}
			else
			{
				LI32(R0, disp);
				STFDX(src, dst, R0);
			}
		}

		void ADDI32(Register dst, Register src, int imm32)
		{
			if (IsSmallDisplacement(imm32))
			{
				ADDI(dst, src, imm32);
			}
			else
			{
				LI32(R0, imm32);
				ADD(dst, src, R0);
			}
		}
		
		int HIWORD(int value) { return (value>>16)&0xFFFF; }
		int LOWORD(int value) { return value&0xFFFF; }

		// Macro to emit the right sequence for
		// loading an immediate 32-bit value
		void LI32(Register reg, int imm32)
		{
			if (imm32 & 0xFFFF0000) {
				LIS(reg, HIWORD(imm32));
				ORI(reg, reg, LOWORD(imm32));
			} else {
				if (imm32 & 0x8000) {
					// todo
					LI(reg, 0);
					ORI(reg, reg, imm32);
				} else {
					LI(reg, imm32);
				}
			}
		}
		
		//
		// End PowerPC code generation
		//
		// 64bit - enabled this for 64-bit to get compiling.  There
		// are certain differences between 32 and 64-bit bytecode
        #elif defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64)		
		//   ---------------------------------------------------
		// 
		//    IA32 specific stuff follows
		//
		//   ---------------------------------------------------

		bool x87Dirty;

		#ifdef AVMPLUS_VERBOSE
		unsigned x87Top:3;
		#endif

		bool is8bit(int i)
		{
			return ((signed char)i) == i;
		}

		void IMM32(int imm32) 
		{
			*(int*)mip = imm32;
			mip += 4;
		}

		void MODRM(Register reg, sintptr disp, Register base, int lshift, Register index);
		void MODRM(Register reg, sintptr disp, Register base);
		void MODRM(Register reg, Register operand);

		void ALU(int op);
		void RET()				{ ALU(0xc3); }
		void NOP()				{ ALU(0x90); }
		#ifdef AVMPLUS_64BIT
		void PUSH(Register r);
		void POP(Register r);
		#else
		void PUSH(Register r)	{ ALU(0x50|r); }
		void POP(Register r)	{ ALU(0x58|r); }
		#endif
		void LAHF()				{ ALU(0x9F); }

		void PUSH(sintptr imm);

		void MOV (Register dest, sintptr imm32);
		void MOV (sintptr disp, Register base, sintptr imm);
		
		// sse data transfer

		// sse math
		void SSE(int op, Register dest, Register src);
		void ADDSD(Register dest, Register src)		{ SSE(0xf20f58, dest, src); }
		void SUBSD(Register dest, Register src)		{ SSE(0xf20f5c, dest, src); }
		void MULSD(Register dest, Register src)		{ SSE(0xf20f59, dest, src); }
		void DIVSD(Register dest, Register src)		{ SSE(0xf20f5e, dest, src); }
		void CVTSI2SD(Register dest, Register src)	{ SSE(0xf20f2a, dest, src); }
		void UCOMISD(Register xmm1, Register xmm2)	{ SSE(0x660f2e, xmm1, xmm2); }
		void MOVAPD(Register dest, Register src)	{ SSE(0x660f28, dest, src); }

		void XORPD(Register dest, uintptr src);

		void SSE(int op, Register r, sintptr disp, Register base);
		void ADDSD(Register r, sintptr disp, Register base)		{ SSE(0xf20f58, r, disp, base); }
		void SUBSD(Register r, sintptr disp, Register base)		{ SSE(0xf20f5C, r, disp, base); }
		void MULSD(Register r, sintptr disp, Register base)		{ SSE(0xf20f59, r, disp, base); }
		void DIVSD(Register r, sintptr disp, Register base)		{ SSE(0xf20f5E, r, disp, base); }
		void MOVSD(Register r, sintptr disp, Register base)		{ SSE(0xf20f10, r, disp, base); }
		void MOVSD(sintptr disp, Register base, Register r)		{ SSE(0xf20f11, r, disp, base); }
		void CVTSI2SD(Register r, sintptr disp, Register base)	{ SSE(0xf20f2a, r, disp, base); }

		void ALU (byte op, Register reg, sintptr imm);
		void ADD(Register reg, sintptr imm) { ALU(0x05, reg, imm); }
		void SUB(Register reg, sintptr imm) { ALU(0x2d, reg, imm); }
		void AND(Register reg, sintptr imm) { ALU(0x25, reg, imm); }
		void XOR(Register reg, sintptr imm) { ALU(0x35, reg, imm); }
		void OR (Register reg, sintptr imm) { ALU(0x0d, reg, imm); }
		void CMP(Register reg, sintptr imm) { ALU(0x3d, reg, imm); }
		void IMUL(Register dst, sintptr imm);

		void ALU (int op, Register lhs_dest, Register rhs);
		void OR (Register lhs, Register rhs)	{ ALU(0x0b, lhs, rhs); }
		void AND(Register lhs, Register rhs)	{ ALU(0x23, lhs, rhs); }
		void XOR(Register lhs, Register rhs)	{ ALU(0x33, lhs, rhs); }
		void ADD(Register lhs, Register rhs)	{ ALU(0x03, lhs, rhs); }
		void CMP(Register lhs, Register rhs)	{ ALU(0x3b, lhs, rhs); }
		void SUB(Register lhs, Register rhs)	{ ALU(0x2b, lhs, rhs); }
		void TEST(Register lhs, Register rhs)	{ ALU(0x85, lhs, rhs); }
		void NEG(Register reg)					{ ALU(0xf7, (Register)3, reg); }
		void SHR(Register reg, Register /*amt*/)	{ ALU(0xd3, (Register)5, reg); } // unsigned >> ecx
		void SAR(Register reg, Register /*amt*/)	{ ALU(0xd3, (Register)7, reg); } // signed >> ecx
		void SHL(Register reg, Register /*amt*/)	{ ALU(0xd3, (Register)4, reg); } // unsigned << ecx
		void XCHG(Register rA, Register rB)		{ ALU(0x87, rA, rB); }
		void MOV (Register dest, Register src)	{ ALU(0x8b, dest, src); }

		void ALU2(int op, Register lhs_dest, Register rhs);
		void IMUL(Register lhs, Register rhs) { ALU2(0x0faf, lhs, rhs); }

		void SETB  (Register reg)	{ ALU2(0x0f92, reg, reg); }
		void SETNB (Register reg)	{ ALU2(0x0f93, reg, reg); }
		void SETE  (Register reg)	{ ALU2(0x0f94, reg, reg); }
		void SETNE (Register reg)	{ ALU2(0x0f95, reg, reg); }
		void SETBE (Register reg)	{ ALU2(0x0f96, reg, reg); }
		void SETNBE(Register reg)	{ ALU2(0x0f97, reg, reg); }
		void SETNP (Register reg)	{ ALU2(0x0f9b, reg, reg); }
		void SETP  (Register reg)	{ ALU2(0x0f9a, reg, reg); }
		void SETL  (Register reg)	{ ALU2(0x0f9C, reg, reg); }
		void SETLE (Register reg)	{ ALU2(0x0f9E, reg, reg); }
		void MOVZX_r8 (Register dest, Register src) { ALU2(0x0fb6, dest, src); }

		void ALU(int op, Register r, sintptr disp, Register base);
		void TEST(sintptr disp, Register base, Register r)	{ ALU(0x85, r, disp, base); }
		void LEA(Register r, sintptr disp, Register base)	{ ALU(0x8d, r, disp, base); }
		void CALL(sintptr disp, Register base)				{ ALU(0xff, (Register)2, disp, base); }
		void JMP(sintptr disp, Register base)				{ ALU(0xff, (Register)4, disp, base); }
		void PUSH(sintptr disp, Register base)				{ ALU(0xff, (Register)6, disp, base); }
		void MOV (sintptr disp, Register base, Register r)  { ALU(0x89, r, disp, base); }
		void MOV (Register r, sintptr disp, Register base)  { ALU(0x8b, r, disp, base); }

		void SHIFT(int op, Register reg, int imm8);
		void SAR(Register reg, int imm8) { SHIFT(7, reg, imm8); } // signed >> imm
		void SHR(Register reg, int imm8) { SHIFT(5, reg, imm8); } // unsigned >> imm
		void SHL(Register reg, int imm8) { SHIFT(4, reg, imm8); } // unsigned << imm
		void TEST_AH(uint8 imm8);

		void JCC (byte op, sintptr offset);
		void JB  (sintptr offset) { JCC(0x02, offset); }	
		void JNB (sintptr offset) { JCC(0x03, offset); }
		void JE  (sintptr offset) { JCC(0x04, offset); }
		void JNE (sintptr offset) { JCC(0x05, offset); }
		void JBE (sintptr offset) { JCC(0x06, offset); }	
		void JNBE(sintptr offset) { JCC(0x07, offset); }
		void JP  (sintptr offset) { JCC(0x0A, offset); }
		void JNP (sintptr offset) { JCC(0x0B, offset); }
		void JL  (sintptr offset) { JCC(0x0C, offset); }
		void JNL (sintptr offset) { JCC(0x0D, offset); }
		void JLE (sintptr offset) { JCC(0x0E, offset); }
		void JNLE(sintptr offset) { JCC(0x0F, offset); }
		void JMP (sintptr offset);
		void CALL(sintptr offset);

		void FPU(int op, sintptr disp, Register base);
		void FSTQ(sintptr disp, Register base)  { FPU(0xdd02, disp, base); }
		void FSTPQ(sintptr disp, Register base) { FPU(0xdd03, disp, base); }
		void FCOM(sintptr disp, Register base)	{ FPU(0xdc02, disp, base); } 
		void FLDQ(sintptr disp, Register base)  { x87Dirty=true; FPU(0xdd00, disp, base); }
		void FILDQ(sintptr disp, Register base)	{ x87Dirty=true; FPU(0xdf05, disp, base); }
		void FILD(sintptr disp, Register base)  { x87Dirty=true; FPU(0xdb00, disp, base); }
		void FADDQ(sintptr disp, Register base) { FPU(0xdc00, disp, base); }
		void FSUBQ(sintptr disp, Register base) { FPU(0xdc04, disp, base); }
		void FMULQ(sintptr disp, Register base) { FPU(0xdc01, disp, base); }
		void FDIVQ(sintptr disp, Register base) { FPU(0xdc06, disp, base); }

		void FPU(int op, Register r);
		void FFREE(Register r)	{ FPU(0xddc0, r); }
		void FSTP(Register r)	{ FPU(0xddd8, r); }
		void FADDQ(Register r)	{ FPU(0xd8c0, r); }
		void FSUBQ(Register r)	{ FPU(0xd8e0, r); }
		void FMULQ(Register r)	{ FPU(0xd8c8, r); }
		void FDIVQ(Register r)	{ FPU(0xd8f0, r); }

		void FPU(int op);
		void FUCOMP()		{ FPU(0xdde9); }
		void FCHS()			{ FPU(0xd9e0); }
		void FNSTSW_AX()	{ FPU(0xdfe0); }
		void EMMS()			{ FPU(0x0f77); x87Dirty=false;  }
		#endif // IA32 or AMD64

		#ifdef AVMPLUS_AMD64
		void IMM64(int64 imm64) 
		{
			*(int64*)mip = imm64;
			mip += 8;
		}
		void REX(Register a,  Register b=EAX, bool set64bit=true);
		
		bool is32bit(sintptr i)
		{
			return ((int32)i) == i;
		}
			
		#endif
		
		// macros

		// windows ia32 calling conventions
		// - args pushed right-to-left
		// - EAX, ECX, EDX are scratch registers
		// - result in EAX (32bit) or EAX:EDX (64bit)

		// cdecl calling conventions:
		//  - caller pops args

		// thiscall calling conventions:
		//  - this in ECX
		//  - callee pops args

		// stdcall calling conventions
		//  - callee pops args

		// fastcall calling conventions
		//  - first 2 args in ECX, EDX
		//  - callee pops args
		
		/** call a method using a relative offset */
		void thincall(sintptr addr) 
		{
			#ifdef AVMPLUS_PPC
			int disp = addr - (int)mip;
			if (IsBranchDisplacement(disp)) {
				// Use relative branch if possible
				BL(disp);
			} else if (IsBranchDisplacement(addr)) {
				// Use absolute branch if possible
				// Note: address will be sign-extended.
				BLA (addr);
			} else {
				// Otherwise, use register-based branched
				LI32(R12, addr);
				MTCTR(R12);
				BCTRL();
			}
            #endif

			#ifdef AVMPLUS_ARM
			int disp = (MDInstruction*)addr - mip - 2;
			if (!(disp & 0xFF000000))
			{
				// Branch displacement fits in 24-bits, use BL instruction.
				BL(addr - (int)mip);
			}
			else
			{
				// Branch displacement doesn't fit
				MOV_imm32(IP, addr);
				MOV(LR, PC);
				MOV(PC, IP);
			}
			#endif
			
			#ifdef AVMPLUS_IA32
			CALL (addr - (5+(int)mip));
			#endif
			
			#ifdef AVMPLUS_AMD64
			CALL (addr - (5+(uintptr)mip));
			#endif
		}

		bool isCodeContextChanged() const;
	};

	// machine dependent buffer sizing information try to use 4B aligned values
	#ifdef AVMPLUS_PPC
	static const int md_prologue_size		= 96;
	static const int md_epilogue_size		= 280;
	static const int md_native_thunk_size	= 1024;
	#endif

	#ifdef AVMPLUS_ARM
	static const int md_prologue_size		= 256;
	static const int md_epilogue_size		= 128;
	static const int md_native_thunk_size	= 1024;
    #endif /* AVMPLUS_ARM */
	
	#ifdef AVMPLUS_IA32
	static const int md_prologue_size		= 32;
	static const int md_epilogue_size		= 128;
	static const int md_native_thunk_size	= 256;
	#endif /* AVMPLUS_PPC */
	
	#ifdef AVMPLUS_AMD64
	//  64bit - may need adjustment
	static const int md_prologue_size		= 32;
	static const int md_epilogue_size		= 128;
	static const int md_native_thunk_size	= 256;
	#endif /* AVMPLUS_PPC */
	
}
#endif /* __avmplus_CodegenMIR__ */
