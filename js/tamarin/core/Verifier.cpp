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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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


#include "avmplus.h"

namespace avmplus
{
#undef DEBUG_EARLY_BINDING

	Verifier::Verifier(MethodInfo* info, Toplevel* toplevel
#ifdef AVMPLUS_VERBOSE
		, bool secondTry
#endif
		)
	{
#ifdef AVMPLUS_VERBOSE
		this->secondTry = secondTry;
#endif
		this->info   = info;
		this->core   = info->core();
		this->pool   = info->pool;
		this->toplevel = toplevel;

		#ifdef FEATURE_BUFFER_GUARD
		#ifdef AVMPLUS_MIR
		this->growthGuard = 0;
		#endif //AVMPLUS_MIR
		#endif /* FEATURE_BUFFER_GUARD */

		#ifdef AVMPLUS_VERBOSE
		this->verbose = pool->verbose || (info->flags & AbstractFunction::VERBOSE_VERIFY);
		#endif

		#ifdef AVMPLUS_PROFILE
		if (core->dprof.dprofile)
			core->dprof.mark(OP_verifyop);
		#endif

		const byte* pos = info->body_pos;
		max_stack = AvmCore::readU30(pos);
		local_count = AvmCore::readU30(pos);
		int init_scope_depth = AvmCore::readU30(pos);
		int max_scope_depth = AvmCore::readU30(pos);
		max_scope = MethodInfo::maxScopeDepth(info, max_scope_depth - init_scope_depth);

		stackBase = scopeBase + max_scope;
		frameSize = stackBase + max_stack;

		if ((init_scope_depth < 0) || (max_scope_depth < 0) || (max_stack < 0) || 
			(max_scope < 0) || (local_count < 0) || (frameSize < 0) || (stackBase < 0))
			verifyFailed(kCorruptABCError);

		code_length = AvmCore::readU30(pos);

		code_pos = pos;

		pos += code_length;
		
		exceptions_pos = pos;

		blockStates = NULL;
		state       = NULL;
		labelCount = 0;

		if (info->declaringTraits == NULL)
		{
			// scope hasn't been captured yet.
			verifyFailed(kCannotVerifyUntilReferencedError);
		}
	}

	Verifier::~Verifier()
	{
		if (blockStates)
		{
			MMgc::GC* gc = core->GetGC();
			for(int i=0, n=blockStates->size(); i<n; i++)
			{
				FrameState* state = blockStates->at(i);
				if (state)
				{
					gc->Free(state);
				}
			}
			blockStates->clear();
			delete blockStates;
		}
	}

	/**
	 * (done) branches stay in code block
	 * (done) branches end on even instr boundaries
	 * (done) all local var operands stay inside [0..max_locals-1]
	 * (done) no illegal opcodes
	 * (done) cpool refs are inside [1..cpool_size-1]
	 * (done) converging paths have same stack depth
	 * (done) operand stack stays inside [0..max_stack-1]
	 * (done) locals defined before use
	 * (done) scope stack stays bounded
	 * (done) getScopeObject never exceeds [0..info->maxScopeDepth()-1]
	 * (done) global slots limits obeyed [0..var_count-1]
	 * (done) callstatic method limits obeyed [0..method_count-1]
	 * (done) cpool refs are correct type
	 * (done) make sure we don't fall off end of function
	 * (done) slot based ops are ok (slot must be legal)
 	 * (done) propref ops are ok: usage on actual type compatible with ref type.
	 * dynamic lookup ops are ok (type must not have that binding & must be dynamic)
     * dont access superclass state in ctor until super ctor called.
	 * @param pool
	 * @param info
	 */
    void Verifier::verify(CodegenMIR *mir)
	{		
		SAMPLE_FRAME("[verify]", core);

		#ifdef AVMPLUS_VERBOSE
		if (verbose)
            core->console << "verify " << info << '\n';
		secondTry = false;
		#endif

		this->mir = mir;
		if ( (state = newFrameState()) == 0 ){
			verifyFailed(kCorruptABCError);
		}
		for (int i=0, n=frameSize; i < n; i++)
			state->setType(i, NULL);

		if (info->flags & AbstractFunction::NEED_REST)
		{
			// param_count+1 <= max_reg
			checkLocal(info->param_count+1);

			state->setType(info->param_count+1, ARRAY_TYPE, true);

			#ifdef AVMPLUS_VERBOSE
			if (verbose && (info->flags & AbstractFunction::NEED_ARGUMENTS))
			{
				// warning, NEED_ARGUMENTS wins
				core->console << "WARNING, NEED_REST overrides NEED_ARGUMENTS when both are set\n";
			}
			#endif
		}
		else if (info->flags & AbstractFunction::NEED_ARGUMENTS)
		{
			// param_count+1 <= max_reg
			checkLocal(info->param_count+1);

			// E3 style arguments array is an Object, E4 says Array, E4 wins...
			state->setType(info->param_count+1, ARRAY_TYPE, true);
		}
		else
		{
			// param_count <= max_reg
			checkLocal(info->param_count);
		}

		// initialize method param types.
		// We already verified param_count is a legal register so
		// don't checkLocal(i) inside the loop.
		// AbstractFunction::verify takes care of resolving param&return type
		// names to Traits pointers, and resolving optional param default values.
		for (int i=0, n=info->param_count; i <= n; i++)
		{
			bool notNull = (i==0); // this is not null, but other params could be
			state->setType(i, info->paramTraits(i), notNull);
		}

		// initial scope chain types 
		int outer_depth = 0;

		ScopeTypeChain* scope = info->declaringTraits->scope;
		if (!scope && info->declaringTraits->init != info)
		{
			// this can occur when an activation scope inside a class instance method
			// contains a nested getter, setter, or method.  In that case the scope 
			// is not captured when the containing function is verified.  This isn't a
			// bug because such nested functions aren't suppported by AS3.  This
			// verify error is how we don't let those constructs run.
			verifyFailed(kNoScopeError, core->toErrorString(info));
		}

		state->scopeDepth = outer_depth;

		// resolve catch block types
		parseExceptionHandlers();

		// Save initial state
		FrameState* initialState = newFrameState();
		if( !initialState->init(state) ) 
			verifyFailed(kCorruptABCError);

		#ifdef AVMPLUS_PROFILE
		if (core->dprof.dprofile)
			core->dprof.mark(OP_verifypass);
		#endif

		int actualCount = 0;
		bool blockEnd = false;    // extended basic block ending

		// always load the entry state, no merge on second pass
		state->init(initialState);

#ifdef FEATURE_BUFFER_GUARD
		#ifdef AVMPLUS_MIR
		// allow the mir buffer to grow dynamically
		#ifdef AVMPLUS_LINUX
		// Linux requires the GrowthGuard to be allocated from the
		// heap, otherwise a segfault occurs when the GrowthGuard is
		// accessed outside of this function.
		this->growthGuard = new GrowthGuard(mir ? mir->mirBuffer : NULL);
		#else
		// Windows requires the GrowthGuard to be allocated from the stack.
		GrowthGuard guard( mir ? mir->mirBuffer : NULL);
		this->growthGuard = &guard;
		#endif //AVMPLUS_LINUX
		#endif //AVMPLUS_MIR
#endif /* FEATURE_BUFFER_GUARD */

		TRY(core, kCatchAction_Rethrow){

		#ifdef AVMPLUS_INTERP
		if (mir)
		#endif
		{
			#ifdef AVMPLUS_MIR
			if( !mir->prologue(state) ) 
			{
				if (!mir->overflow)
					verifyFailed(kCorruptABCError);

				// we're out of code memory so try to carry on with interpreter
				mir = 0;
				this->mir = 0;
			}
			#endif //AVMPLUS_MIR
		}

		int size;
		for (const byte* pc = code_pos, *code_end=code_pos+code_length; pc < code_end; pc += size)
		{
			SAMPLE_CHECK();
			#ifdef AVMPLUS_PROFILE
			if (core->dprof.dprofile)
				core->dprof.mark(OP_verifyop);
			#endif

			if (mir && mir->overflow)
			{
				mir = 0;
				this->mir = 0;
			}
			
			AbcOpcode opcode = (AbcOpcode) *pc;
			if (opOperandCount[opcode] == -1)
				verifyFailed(kIllegalOpcodeError, core->toErrorString(info), core->toErrorString(opcode), core->toErrorString(pc-code_pos));

			if (opcode == OP_label)
			{
				// insert a label here
				getFrameState(pc-code_pos)->targetOfBackwardsBranch = true;
			}

			bool unreachable = false;
			FrameState* blockState;
			if ( blockStates && (blockState = blockStates->get((uintptr)pc)) != 0 )
			{
				// send a bbend prior to the merge
				if (mir) mir->emitBlockEnd(state);

				if (!blockEnd || !blockState->initialized)
				{
					checkTarget(pc); 
				}

				#ifdef AVMPLUS_VERBOSE
				if (verbose)
					core->console << "B" << actualCount << ":\n";
				#endif

				// now load the merged state
				state->init(blockState);
				state->targetOfBackwardsBranch = blockState->targetOfBackwardsBranch;
				state->pc = pc - code_pos;

				// found the start of a new basic block
				if (mir) mir->emitBlockStart(state);

				state->targetOfBackwardsBranch = false;

				actualCount++;
				blockEnd = false;

				if (!blockState->targetOfBackwardsBranch)
				{
					blockStates->remove((uintptr)pc);
					core->GetGC()->Free(blockState);
				}
			}
			else
			{
				if (blockEnd)
				{
					// the next instruction is not reachable because it comes after
					// a jump, throw, or return, and there are no branch targets
					// inbetween the branch/jump/return and this instruction.
					// So, don't verify it.
					unreachable = true;
				}
			}

			state->pc = pc - code_pos;
			int sp = state->sp();

			if (info->exceptions)
			{
				bool mirSavedState = false;
				for (int i=0, n=info->exceptions->exception_count; i < n; i++)
				{
					ExceptionHandler* handler = &info->exceptions->exceptions[i];
					if (pc >= code_pos + handler->from && pc <= code_pos + handler->to)
					{
						// Set the insideTryBlock flag, so the codegen can
						// react appropriately.
						state->insideTryBlock = true;

						// If this instruction can throw exceptions, add an edge to
						// the catch handler.
						if (opCanThrow[opcode] || pc == code_pos + handler->from)
						{
							// TODO check stack is empty because catch handlers assume so.
							int saveStackDepth = state->stackDepth;
							int saveScopeDepth = state->scopeDepth;
							state->stackDepth = 0;
							state->scopeDepth = outer_depth;
							Value stackEntryZero = state->stackValue(0);

							#ifdef AVMPLUS_MIR
							if (mir && !mirSavedState) {
								mir->emitBlockEnd(state);
								mirSavedState = true;
							}
							#endif

							// add edge from try statement to catch block
							const byte* target = code_pos + handler->target;
							// atom received as *, will coerce to correct type in catch handler.
							state->push(NULL);

							#ifdef AVMPLUS_MIR
							if (mir)
								mir->localSet(stackBase, mir->exAtom);
							#endif

							checkTarget(target);
 							state->pop();

							state->stackDepth = saveStackDepth;
							state->scopeDepth = saveScopeDepth;
							state->stackValue(0) = stackEntryZero;

							// Search forward for OP_killreg in the try block
							// Set these registers to killed in the catch state
							if (pc == code_pos + handler->from)
							{
								FrameState *catchState = blockStates->get((uintptr)target);
								AvmAssert(catchState != 0);

                                for (const byte *temp = pc; temp <= code_pos + handler->to; )
								{
                                    if( *temp == OP_lookupswitch )
                                    {
                                        // Variable length instruction
										const byte *temp2 = temp+4;
                                        const int case_count = toplevel->readU30(temp2)+1;
										temp += AvmCore::calculateInstructionWidth(temp) + 3*case_count;
                                    }
									else
									{
										// If this instruction is OP_killreg, kill the register
										// in the catch block state too
										if (*temp == OP_kill)
										{
											const byte* temp2 = temp+1;
											int local = toplevel->readU30(temp2);
											if (local >= local_count)
												verifyFailed(kInvalidRegisterError, core->toErrorString(local));
											Value& v = catchState->value(local);
											v.notNull = false;
											v.traits = NULL;
											v.killed = true;
										}
										temp += AvmCore::calculateInstructionWidth(temp);
									}
								}
							}
						}							
					}
					
					if (pc == code_pos + handler->target)
					{
						emitCoerce(handler->traits, sp);
					}
				}
			}
			unsigned int imm30=0, imm30b=0;
			int imm8=0, imm24=0;

			const byte* nextpc = pc;
            AvmCore::readOperands(nextpc, imm30, imm24, imm30b, imm8);
			// make sure U30 operands are within bounds.
			if (opcode != OP_pushshort && ((imm30|imm30b) & 0xc0000000))
			{
				// don't report error if first operand of abs_jump (pc) is large.
				if (opcode != OP_abs_jump || (imm30b & 0xc0000000))
				{
					verifyFailed(kCorruptABCError);
				}
			}
			size = int(nextpc-pc);
			if (pc+size > code_end)
				verifyFailed(kLastInstExceedsCodeSizeError);

            #ifdef AVMPLUS_PROFILE
			if (core->sprof.sprofile)
				core->sprof.tally(opcode, size);
			#endif

			#ifdef AVMPLUS_VERBOSE
			if (verbose) 
			{
				showState(state, pc, unreachable);
			}
			#endif

			if (unreachable)
			{
				// Even if unreachable, we have to properly advance
				// past the variable-sized OP_lookupswitch
				if (opcode == OP_lookupswitch)
					size += 3*(imm30b+1);

				continue;
			}

			switch (opcode)
			{
			case OP_iflt:
			case OP_ifle:
			case OP_ifnlt:
			case OP_ifnle:
			case OP_ifgt:
			case OP_ifge:
			case OP_ifngt:
			case OP_ifnge:
			case OP_ifeq:
			case OP_ifstricteq:
			case OP_ifne:
			case OP_ifstrictne:
			{
				checkStack(2,0);
				int lhs = sp-1;
				state->pop();
				state->pop();
				if (mir)
					mir->emitIf(state, opcode, state->pc+size+imm24, lhs, lhs+1);
				checkTarget(nextpc+imm24);
				break;
			}

			case OP_iftrue:
			case OP_iffalse:
			{
				checkStack(1,0);
				int cond = sp;
				emitCoerce(BOOLEAN_TYPE, cond);
				state->pop();
				if (mir)
					mir->emitIf(state, opcode, state->pc+size+imm24, cond, 0);
				checkTarget(nextpc+imm24);
				break;
			}

			case OP_jump:
			{
				//checkStack(0,0)
				if (mir) mir->emit(state, opcode, state->pc+size+imm24);
				checkTarget(nextpc+imm24);	// target block;
				blockEnd = true;
				break;
			}

			case OP_lookupswitch: 
			{
				checkStack(1,0);
				peekType(INT_TYPE);
				const uint32 count = imm30b;
				if (mir) mir->emit(state, opcode, state->pc+imm24, count);

				state->pop();

				checkTarget(pc+imm24);
				uint32 case_count = 1 + count;
				if (pc + size + 3*case_count > code_end) 
					verifyFailed(kLastInstExceedsCodeSizeError);

				for (uint32 i=0; i < case_count; i++) {
					int off = AvmCore::readS24(pc+size);
					checkTarget(pc+off);
					size += 3;
				}
				blockEnd = true;
				break;
			}

			case OP_throw:
			{
				checkStack(1,0);
				// [ggrossman] it is legal to throw anything at all; don't check
				if (mir) mir->emit(state, opcode, sp);
				state->pop();
				blockEnd = true;
				break;
			}

			case OP_returnvalue:
			{
				checkStack(1,0);

				if (mir)
				{
					Traits* returnTraits = info->returnTraits();
					emitCoerce(returnTraits, sp);
					mir->emit(state, opcode, sp);
				}
				// make sure stack state is updated, since verifier scans
				// straight through to the next block.
				state->pop();
				blockEnd = true;
				break;
			}

			case OP_returnvoid: 
			{
				//checkStack(1,0)
				if (mir) mir->emit(state, opcode);
				blockEnd = true;
				break;
			}

			case OP_pushnull:
				checkStack(0,1);
				if (mir) mir->emitIntConst(state, sp+1, 0);
				state->push(NULL_TYPE);
				break;

			case OP_pushundefined:
				checkStack(0,1);
				if (mir) mir->emitIntConst(state, sp+1, undefinedAtom);
				state->push(VOID_TYPE);
				break;

			case OP_pushtrue:
				checkStack(0,1);
				if (mir) mir->emitIntConst(state, sp+1, 1);
				state->push(BOOLEAN_TYPE, true);
				break;

			case OP_pushfalse:
				checkStack(0,1);
				if (mir) mir->emitIntConst(state, sp+1, 0);
				state->push(BOOLEAN_TYPE, true);
				break;

			case OP_pushnan:
				checkStack(0,1);
				if (mir) mir->emitDoubleConst(state, sp+1, (double*)(core->kNaN & ~7));
				state->push(NUMBER_TYPE, true);
				break;

			case OP_pushshort:
				checkStack(0,1);
				if (mir) mir->emitIntConst(state, sp+1, (signed short)imm30);
				state->push(INT_TYPE, true);
				break;

			case OP_pushbyte:
				checkStack(0,1);
				if (mir) mir->emitIntConst(state, sp+1, (signed char)imm8);
				state->push(INT_TYPE, true);
				break;

			case OP_debugfile:
			{
				//checkStack(0,0)
				#ifdef DEBUGGER
				Atom filename = checkCpoolOperand(imm30, kStringType);
				if (mir) mir->emit(state, opcode, (uintptr)AvmCore::atomToString(filename));
				#endif
				break;
			}

			case OP_dxns:
			{
				//checkStack(0,0)
				if (!info->isFlagSet(AbstractFunction::SETS_DXNS))
					verifyFailed(kIllegalSetDxns, core->toErrorString(info));
				Atom uri = checkCpoolOperand(imm30, kStringType);
				if (mir) mir->emit(state, opcode, (uintptr)AvmCore::atomToString(uri));
				break;
			}

			case OP_dxnslate:
			{
				checkStack(1,0);
				if (!info->isFlagSet(AbstractFunction::SETS_DXNS))
					verifyFailed(kIllegalSetDxns, core->toErrorString(info));
				// codgeen will call intern on the input atom.
				if (mir) mir->emit(state, opcode, sp);
				state->pop();
				break;
			}

			case OP_pushstring: 
			{
				checkStack(0,1);
				uint32 index = imm30;
				if (index > 0 && index < pool->constantStringCount)
				{
					Stringp value = pool->cpool_string[index];
					if (mir)mir->emitIntConst(state, sp+1, (uintptr)value);
					state->push(STRING_TYPE, value != NULL);
				}
				else
				{
					verifyFailed(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantStringCount));
				}
				break;
			}

			case OP_pushint: 
			{
				checkStack(0,1);
				uint32 index = imm30;
				if (index > 0 && index < pool->constantIntCount)
				{
					if (mir)
					{
						int value = pool->cpool_int[index];
						mir->emitIntConst(state, sp+1, value);
					}
					state->push(INT_TYPE,true);
				}
				else
				{
					verifyFailed(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantIntCount));
				}
				break;
			}
			case OP_pushuint: 
			{
				checkStack(0,1);
				uint32 index = imm30;
				if (index > 0 && index < pool->constantUIntCount)
				{
					if (mir)
					{
						uint32 value = pool->cpool_uint[index];
						mir->emitIntConst(state, sp+1, value);
					}
					state->push(UINT_TYPE,true);
				}
				else
				{
					verifyFailed(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantUIntCount));
				}
				break;
			}
			case OP_pushdouble: 
			{
				checkStack(0,1);
				uint32 index = imm30;
				if (index > 0 && index < pool->constantDoubleCount)
				{
					if (mir)
					{
						double* p = pool->cpool_double[index];
						mir->emitDoubleConst(state, sp+1, p);
					}
					state->push(NUMBER_TYPE, true);
				}
				else
				{
					verifyFailed(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantDoubleCount));
				}
				break;
			}

			case OP_pushnamespace: 
			{
				checkStack(0,1);
				uint32 index = imm30;
				if (index > 0 && index < pool->constantNsCount)
				{
					Namespace* value = pool->cpool_ns[index];
					if (mir)mir->emitIntConst(state, sp+1, (uintptr)value);
					state->push(NAMESPACE_TYPE, value != NULL);
				}
				else
				{
					verifyFailed(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantNsCount));
				}
				break;
			}

			case OP_setlocal:
			{
				checkStack(1,0);
				checkLocal(imm30);
				if (mir) mir->emitCopy(state, sp, imm30);
				Value &v = state->stackTop();
				state->setType(imm30, v.traits, v.notNull);
				state->pop();
				break;
			}

			case OP_setlocal0:
			case OP_setlocal1:
			case OP_setlocal2:
			case OP_setlocal3:				
			{
				int localno = opcode-OP_setlocal0;
				checkStack(1,0);
				checkLocal(localno);
				if (mir) mir->emitCopy(state, sp, localno);
				Value &v = state->stackTop();
				state->setType(localno, v.traits, v.notNull);
				state->pop();
				break;
			}

			case OP_getlocal:
			{
				checkStack(0,1);
				Value& v = checkLocal(imm30);
				if (mir) mir->emitCopy(state, imm30, sp+1);
				state->push(v);
				break;
			}

			case OP_getlocal0:
			case OP_getlocal1:
			case OP_getlocal2:
			case OP_getlocal3:
			{
				int localno = opcode-OP_getlocal0;
				checkStack(0,1);
				Value& v = checkLocal(localno);
				if (mir) mir->emitCopy(state, localno, sp+1);
				state->push(v);
				break;
			}
			
			case OP_kill:
			{
				//checkStack(0,0)
				Value &v = checkLocal(imm30);
				if (mir) mir->emitKill(state, imm30);
				v.notNull = false;
				v.traits = NULL;
				break;
			}

			case OP_inclocal:
			case OP_declocal:
			{
				//checkStack(0,0);
				checkLocal(imm30);
				emitCoerce(NUMBER_TYPE, imm30);
				if (mir)
				{
					mir->emit(state, opcode, imm30, opcode==OP_inclocal ? 1 : -1, NUMBER_TYPE);
				}
				break;
			}

			case OP_inclocal_i:
			case OP_declocal_i:
			{
				//checkStack(0,0);
				checkLocal(imm30);
				emitCoerce(INT_TYPE, imm30);
				if (mir)
				{
					mir->emit(state, opcode, imm30, opcode==OP_inclocal_i ? 1 : -1, INT_TYPE);
				}
				break;
			}

			case OP_newfunction: 
			{
				checkStack(0,1);
				AbstractFunction* f = checkMethodInfo(imm30);

				f->setParamType(0, NULL);
				f->makeIntoPrototypeFunction(toplevel);
				Traits* ftraits = f->declaringTraits;
				// make sure the traits of the base vtable matches the base traits
				// This is also caught by an assert in VTable.cpp, however that turns
				// out to be too late and may cause crashes
				if( ftraits->base != toplevel->functionClass->ivtable()->traits ){
					AvmAssertMsg(0, "Verify failed:OP_newfunction");
 					if (toplevel->verifyErrorClass() != NULL)
 						verifyFailed(kInvalidBaseClassError);
				}
				int extraScopes = state->scopeDepth;
				ftraits->scope = ScopeTypeChain::create(core->GetGC(), scope, extraScopes);
				for (int i=0,j=scope->size, n=state->scopeDepth; i < n; i++, j++)
				{
					ftraits->scope->scopes[j].traits = state->scopeValue(i).traits;
					ftraits->scope->scopes[j].isWith = state->scopeValue(i).isWith;
				}

				if (f->activationTraits)
				{
					// ISSUE - if nested functions, need to capture scope, not make a copy
					f->activationTraits->scope = ftraits->scope;
				}

				#ifdef AVMPLUS_VERIFYALL
				if (core->verifyall)
					pool->enq(f);
				#endif

				if (mir) 
				{
					mir->emitSetDxns(state);
					mir->emit(state, opcode, imm30, sp+1, ftraits);
				}

				state->push(ftraits, true);
				break;
			}

			case OP_getlex:
			{
				if (state->scopeDepth + scope->size == 0)
				{
					verifyFailed(kFindVarWithNoScopeError);
				}
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(0, 1, &multiname);
				if (multiname.isRuntime())
					verifyFailed(kIllegalOpMultinameError, core->toErrorString(opcode), core->toErrorString(&multiname));
				emitFindProperty(OP_findpropstrict, multiname);
				emitGetProperty(multiname, 1);
				break;
			}

			case OP_findpropstrict:
			case OP_findproperty:
			{
				if (state->scopeDepth + scope->size == 0)
				{
					verifyFailed(kFindVarWithNoScopeError);
				}
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(0, 1, &multiname);
				emitFindProperty(opcode, multiname);
				break;
			}

			case OP_newclass: 
			{
				checkStack(1, 1);
				// must be a CONSTANT_Multiname
				Traits* ctraits = checkClassInfo(imm30);

				// the actual result type will be the static traits of the new class.
				
				// make sure the traits came from this pool.  they have to because
				// the class_index operand is resolved from the current pool.
				AvmAssert(ctraits->pool == pool);

				Traits *itraits = ctraits->itraits;

				int captureScopes = state->scopeDepth;

				ScopeTypeChain* cscope = ScopeTypeChain::create(core->GetGC(), scope, captureScopes, 1);
				int j=scope->size;
				for (int i=0, n=state->scopeDepth; i < n; i++, j++)
				{
					cscope->scopes[j].traits = state->scopeValue(i).traits;
					cscope->scopes[j].isWith = state->scopeValue(i).isWith;
				}

				// add a type constraint for the "this" scope of static methods
				cscope->scopes[state->scopeDepth].traits = ctraits;

				if (state->scopeDepth > 0)
				{
					// innermost scope must be the base class object or else createInstance()
					// will malfunction because it will use the wrong [base] class object to
					// construct the instance.  See ScriptObject::createInstance()
					Traits* baseCTraits = state->scopeValue(state->scopeDepth-1).traits;
					if (!baseCTraits || baseCTraits->itraits != itraits->base)
						verifyFailed(kCorruptABCError);
				}

				ScopeTypeChain* iscope = ScopeTypeChain::create(core->GetGC(), cscope, 1, 1);
				iscope->scopes[iscope->size-1].traits = ctraits;

				// add a type constraint for the "this" scope of instance methods
				iscope->scopes[iscope->size].traits = itraits;

				ctraits->scope = cscope;
				itraits->scope = iscope;

				ctraits->resolveSignatures(toplevel);
				itraits->resolveSignatures(toplevel);

				#ifdef AVMPLUS_VERIFYALL
				if (core->verifyall)
				{
					pool->enq(ctraits);
					pool->enq(itraits);
				}
				#endif

				// make sure base class is really a class
				if (mir)
				{
					mir->emitSetDxns(state);
					emitCoerce(CLASS_TYPE, state->sp());
					mir->emit(state, opcode, (uintptr)(void*)pool->cinits[imm30], sp, ctraits);
				}
				state->pop_push(1, ctraits, true);
				break;
			}

			case OP_finddef: 
			{
				// must be a CONSTANT_Multiname.
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(0, 1, &multiname);

				if (!multiname.isBinding())
				{
					// error, def name must be CT constant, regular name
					verifyFailed(kIllegalOpMultinameError, core->toErrorString(opcode), core->toErrorString(&multiname));
				}

				AbstractFunction* script = (AbstractFunction*)pool->getNamedScript(&multiname);
				if (script != (AbstractFunction*)BIND_NONE && script != (AbstractFunction*)BIND_AMBIGUOUS)
				{
					// found a single matching traits
					if (mir) mir->emit(state, opcode, (uintptr)&multiname, sp+1, script->declaringTraits);
					state->push(script->declaringTraits, true);
				}
				else
				{
					// no traits, or ambiguous reference.  use Object, anticipating
					// a runtime exception
					if (mir) mir->emit(state, opcode, (uintptr)&multiname, sp+1, OBJECT_TYPE);
					state->push(OBJECT_TYPE, true);
				}
				break;
			}

			case OP_setproperty:
			case OP_initproperty:
			{
				// stack in: object [ns] [name] value
				Multiname multiname;
				checkConstantMultiname(imm30, multiname); // CONSTANT_Multiname
				checkStackMulti(2, 0, &multiname);

				uint32 n=2;
				checkPropertyMultiname(n, multiname);
				Value& obj = state->peek(n);

				int ptrIndex = sp-(n-1);
				if (mir) emitCheckNull(ptrIndex);

				Binding b = toplevel->getBinding(obj.traits, &multiname);
				Traits* propTraits = readBinding(obj.traits, b);
				if (AvmCore::isSlotBinding(b) && mir &&
					// it's a var, or a const being set from the init function
					(!AvmCore::isConstBinding(b) || 
						obj.traits->init == info && opcode == OP_initproperty))
				{
					emitCoerce(propTraits, state->sp());
					mir->emit(state, OP_setslot, AvmCore::bindingToSlotId(b), ptrIndex, propTraits);
					state->pop(n);
					break;
				}
				// else: setting const from illegal context, fall through

				// If it's an accessor that we can early bind, do so.
				// Note that this cannot be done on String or Namespace,
				// since those are represented by non-ScriptObjects
				if (mir && AvmCore::hasSetterBinding(b))
				{
					// early bind to the setter
					int disp_id = AvmCore::bindingToSetterId(b);
					AbstractFunction *f = obj.traits->getMethod(disp_id);
					AvmAssert(f != NULL);
					emitCoerceArgs(f, 1);
					mir->emitSetContext(state, f);
					Traits* result = f->returnTraits();
					if (!obj.traits->isInterface)
						mir->emitCall(state, OP_callmethod, disp_id, 1, result);
					else
						mir->emitCall(state, OP_callinterface, f->iid(), 1, result);
					state->pop(n);
					break;
				}

				#ifdef DEBUG_EARLY_BINDING
				core->console << "verify setproperty " << obj.traits << " " << &multiname->getName() << " from within " << info << "\n";
				#endif

				// not a var binding or early bindable accessor
				if (mir)
				{
					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, (uintptr)&multiname);
				}
				state->pop(n);

				break;
			}

			case OP_getproperty:
			{
				// stack in: object [ns [name]]
				// stack out: value
				Multiname multiname;
				checkConstantMultiname(imm30, multiname); // CONSTANT_Multiname
				checkStackMulti(1, 1, &multiname);
				uint32 n=1;
				checkPropertyMultiname(n, multiname);
				emitGetProperty(multiname, n);
				break;
			}

			case OP_getdescendants:
			{
				// stack in: object [ns] [name]
				// stack out: value
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(1, 1, &multiname);
				uint32 n=1;
				checkPropertyMultiname(n, multiname);
				if (mir)
				{
					emitCheckNull(sp-(n-1));
					mir->emit(state, opcode, (uintptr)&multiname, 0, NULL);
				}
				state->pop_push(n, NULL);
				break;
			}

			case OP_checkfilter:
			{
				// stack in: object 
				// stack out: object
				checkStack(1, 1);
				if (mir)
				{
					emitCheckNull(state->sp());
					mir->emit(state, opcode, state->sp(), 0, NULL);
				}
				break;
			}

			case OP_deleteproperty:
			{
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(1, 1, &multiname);
				uint32 n=1;
				checkPropertyMultiname(n, multiname);
				if (mir) 
				{
					emitCheckNull(sp-(n-1));
					mir->emit(state, opcode, (uintptr)&multiname, 0, BOOLEAN_TYPE);
				}
				state->pop_push(n, BOOLEAN_TYPE);
				break;
			}

			case OP_astype:
			{
				checkStack(1, 1);
				// resolve operand into a traits, and push that type.
				Traits *t = checkTypeName(imm30);// CONSTANT_Multiname
				int index = sp;
				Traits* rhs = state->value(index).traits;
				if (!canAssign(t, rhs) || !Traits::isMachineCompatible(t, rhs))
				{
					Traits* resultType = t;
					// result is typed value or null, so if type can't hold null,
					// then result type is Object. 
					if (t && t->isMachineType)
						resultType = OBJECT_TYPE;
					if (mir)
						mir->emit(state, OP_astype, (uintptr)t, index, resultType);
					state->pop_push(1, t);
				}

				break;
			}

			case OP_astypelate:
			{
				checkStack(2,1);
				Value& classValue = state->peek(1); // rhs - class
				Traits* ct = classValue.traits;
				Traits* t = NULL;
				if (ct && (t=ct->itraits) != 0)
					if (t->isMachineType)
						t = OBJECT_TYPE;

				if (mir) mir->emit(state, opcode, 0, 0, t);
				state->pop_push(2, t);
				break;
			}

			case OP_coerce:
			{
				checkStack(1,1);
				// resolve operand into a traits, and push that type.
				emitCoerce(checkTypeName(imm30), sp);
				break;
			}

			case OP_convert_b:
			case OP_coerce_b:
			{
				checkStack(1,1);
				emitCoerce(BOOLEAN_TYPE, sp);
				break;
			}

			case OP_coerce_o:
			{
				checkStack(1,1);
				emitCoerce(OBJECT_TYPE, sp);
				break;
			}

			case OP_coerce_a:
			{
				checkStack(1,1);
				emitCoerce(NULL, sp);
				break;
			}

			case OP_convert_i:
			case OP_coerce_i:
			{
				checkStack(1,1);
				emitCoerce(INT_TYPE, sp);
				break;
			}

			case OP_convert_u:
			case OP_coerce_u:
			{
				checkStack(1,1);
				emitCoerce(UINT_TYPE, sp);
				break;
			}

			case OP_convert_d:
			case OP_coerce_d:
			{
				checkStack(1,1);
				emitCoerce(NUMBER_TYPE, sp);
				break;
			}

			case OP_coerce_s:
			{
				checkStack(1,1);
				emitCoerce(STRING_TYPE, sp);
				break;
			}

			case OP_istype: 
			{
				checkStack(1,1);
				// resolve operand into a traits, and test if value is that type
				Traits* itraits = checkTypeName(imm30); // CONSTANT_Multiname
				if (mir) mir->emit(state, opcode, (uintptr)itraits, sp, BOOLEAN_TYPE);
				state->pop();
				state->pop();
				state->push(OBJECT_TYPE);
				state->push(INT_TYPE);
				break;
			}

			case OP_istypelate: 
			{
				checkStack(2,1);
				if (mir) mir->emit(state, opcode, 0, 0, BOOLEAN_TYPE);
				// TODO if the only common base type of lhs,rhs is Object, then result is always false
				state->pop_push(2, BOOLEAN_TYPE);
				break;
			}

			case OP_convert_o:
			{	
				checkStack(1,1);
				// ISSUE should result be Object, laundering the type?
				// ToObject throws an exception on null and undefined, so after this runs we
				// know the value is safe to dereference.
				if (mir)
					emitCheckNull(sp);
				break;
			}

			case OP_convert_s: 
			case OP_esc_xelem: 
			case OP_esc_xattr:
			{
				checkStack(1,1);
				// this is the ECMA ToString and ToXMLString operators, so the result must not be null
				// (ToXMLString is split into two variants - escaping elements and attributes)
				emitToString(opcode, sp);
				break;
			}

			case OP_callstatic: 
			{
				AbstractFunction* m = checkMethodInfo(imm30);
				int method_id = m->method_id;
				const uint32 argc = imm30b;

				checkStack(argc+1, 1);

				if (!m->paramTraits(0))
				{
					verifyFailed(kDanglingFunctionError, core->toErrorString(m), core->toErrorString(info));
				}

				#ifdef AVMPLUS_VERIFYALL
				if (core->verifyall)
					pool->enq(m);
				#endif

				emitCoerceArgs(m, argc);
				
				Traits* resultType = m->returnTraits();
				if (mir)
				{
					emitCheckNull(sp-argc);
					mir->emitSetContext(state, m);
					mir->emitCall(state, OP_callstatic, method_id, argc, resultType);
				}
				state->pop_push(argc+1, resultType);
				break;
			}

			case OP_call: 
			{
				const uint32 argc = imm30;
				checkStack(argc+2, 1);
				// don't need null check, AvmCore::call() uses toFunction() for null check.
				
				/* 
					TODO optimizations
					    - if this is a class closure for a non-native type, call == coerce
						- if this is a function closure, try early binding using the traits->call sig
						- optimize simple cases of casts to builtin types
				*/
				if (mir) 
				{
					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, argc, 0, NULL);
				}
				state->pop_push(argc+2, NULL);
				break;
			}

			case OP_construct: 
			{
				// in: ctor arg1..N
				// out: obj
				const uint32 argc = imm30;
				checkStack(argc+1, 1);
				Traits* ctraits = state->peek(argc+1).traits;
				// don't need null check, AvmCore::construct() uses toFunction() for null check.
				Traits* itraits = ctraits ? ctraits->itraits : NULL;
				if (mir)
				{
					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, argc, 0, itraits);
				}
				state->pop_push(argc+1, itraits, true);
				break;
			}

			case OP_callmethod: 
			{
				const uint32 argc = imm30b;
				const int disp_id = imm30-1;
				checkStack(argc+1,1);
				if (disp_id >= 0)
				{
					Value& obj = state->peek(argc+1);
					if( !obj.traits ) verifyFailed(kCorruptABCError);
					checkEarlyMethodBinding(obj.traits);
					AbstractFunction* m = checkDispId(obj.traits, disp_id);
					Traits *resultType = m->returnTraits();
					if (mir)
					{
						emitCheckNull(sp-argc);
						emitCoerceArgs(m, argc);
						mir->emitSetContext(state, m);
						if (!obj.traits->isInterface)
							mir->emitCall(state, OP_callmethod, disp_id, argc, resultType);
						else
							mir->emitCall(state, OP_callinterface, m->iid(), argc, resultType);
					}
					state->pop_push(argc+1, resultType);
				}
				else
				{
					verifyFailed(kZeroDispIdError);
				}
				break;
			}

			case OP_callproperty: 
			case OP_callproplex: 
			case OP_callpropvoid:
			{
				// stack in: obj [ns [name]] args
				// stack out: result
				const uint32 argc = imm30b;
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(argc+1, 1, &multiname);

				checkCallMultiname(opcode, &multiname);

				uint32 n = argc+1; // index of receiver
				checkPropertyMultiname(n, multiname);

				Traits* t = state->peek(n).traits;

				if (mir) emitCheckNull(sp-(n-1));

				Binding b = toplevel->getBinding(t, &multiname);
				if (t)
					t->resolveSignatures(toplevel);

				if (AvmCore::isMethodBinding(b))
				{
					if (mir)
					{
						if (t == core->traits.math_ctraits)
						{
							b = findMathFunction(t, &multiname, b, argc);
						}
						else if (t == core->traits.string_itraits)
						{
							b = findStringFunction(t, &multiname, b, argc);
						}
					}

					int disp_id = AvmCore::bindingToMethodId(b);
					AbstractFunction* m = t->getMethod(disp_id);
					if (m->argcOk(argc))
					{
						Traits* resultType = m->returnTraits();
						if (mir)
						{
							emitCoerceArgs(m, argc);
							mir->emitSetContext(state, m);
							if (!t->isInterface)
								mir->emitCall(state, OP_callmethod, disp_id, argc, resultType);
							else
								mir->emitCall(state, OP_callinterface, m->iid(), argc, resultType);
						}
						state->pop_push(n, resultType);
						if (opcode == OP_callpropvoid)
							state->pop();
						break;
					}
				}
				else if (mir && AvmCore::isSlotBinding(b) && argc == 1)
				{
					// is it int, number, or uint?
					int slot_id = AvmCore::bindingToSlotId(b);
					Traits* slotType = t->getSlotTraits(slot_id);
					if (slotType == core->traits.int_ctraits)
					{
						emitCoerce(INT_TYPE, sp);
						Value v = state->stackTop();
						state->pop();
						state->stackTop() = v;
						if (opcode == OP_callpropvoid)
							state->pop();
						break;
					}
					else if (slotType == core->traits.uint_ctraits)
					{
						emitCoerce(UINT_TYPE, sp);
						Value v = state->stackTop();
						state->pop();
						state->stackTop() = v;
						if (opcode == OP_callpropvoid)
							state->pop();
						break;
					}
					else if (slotType == core->traits.number_ctraits)
					{
						emitCoerce(NUMBER_TYPE, sp);
						Value v = state->stackTop();
						state->pop();
						state->stackTop() = v;
						if (opcode == OP_callpropvoid)
							state->pop();
						break;
					}
					else if (slotType == core->traits.boolean_ctraits)
					{
						emitCoerce(BOOLEAN_TYPE, sp);
						Value v = state->stackTop();
						state->pop();
						state->stackTop() = v;
						if (opcode == OP_callpropvoid)
							state->pop();
						break;
					}
					else if (slotType == core->traits.string_ctraits)
					{
						emitToString(OP_convert_s, sp);
						Value v = state->stackTop();
						state->pop();
						state->stackTop() = v;
						if (opcode == OP_callpropvoid)
							state->pop();
						break;
					}

					// is this a user defined class?  A(1+ args) means coerce to A
					if (slotType && slotType->base == CLASS_TYPE && slotType->getNativeClassInfo() == NULL)
					{
						AvmAssert(slotType->itraits != NULL);
						emitCoerce(slotType->itraits, state->sp());
						Value v = state->stackTop();
						state->pop();
						state->stackTop() = v;
						if (opcode == OP_callpropvoid)
							state->pop();
						break;
					}
				}

				#ifdef DEBUG_EARLY_BINDING
				core->console << "verify callproperty " << t << " " << multiname->getName() << " from within " << info << "\n";
				#endif

				// don't know the binding now, resolve at runtime
				if (mir) 
				{
					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, (uintptr)&multiname, argc, NULL);
				}
				state->pop_push(n, NULL);
				if (opcode == OP_callpropvoid)
					state->pop();
				break;
			}

			case OP_constructprop: 
			{
				// stack in: obj [ns [name]] args
				const uint32 argc = imm30b;
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(argc+1, 1, &multiname);

				checkCallMultiname(opcode, &multiname);

				uint32 n = argc+1; // index of receiver
				checkPropertyMultiname(n, multiname);

				Value& obj = state->peek(n); // make sure object is there
				if (mir) emitCheckNull(sp-(n-1));

				#ifdef DEBUG_EARLY_BINDING
				//core->console << "verify constructprop " << t << " " << multiname->getName() << " from within " << info << "\n";
				#endif

				Binding b = toplevel->getBinding(obj.traits, &multiname);
				
				if (AvmCore::isSlotBinding(b))
				{
					int slot_id = AvmCore::bindingToSlotId(b);
					Traits* ctraits = readBinding(obj.traits, b);
					if (mir) mir->emit(state, OP_getslot, slot_id, sp-(n-1), ctraits);
					obj.notNull = false;
					obj.traits = ctraits;
					Traits* itraits = ctraits ? ctraits->itraits : NULL;
					if (mir)
					{
						mir->emitSetContext(state, NULL);
						mir->emit(state, OP_construct, argc, 0, itraits);
					}
					state->pop_push(argc+1, itraits, true);
					break;
				}

				// don't know the binding now, resolve at runtime
				if (mir) 
				{
					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, (uintptr)&multiname, argc, NULL);
				}
				state->pop_push(n, NULL);
				break;
			}

			case OP_callsuper: 
			case OP_callsupervoid:
			{
				// stack in: obj [ns [name]] args
				const uint32 argc = imm30b;
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(argc+1, 1, &multiname);

				if (multiname.isAttr())
					verifyFailed(kIllegalOpMultinameError, core->toErrorString(&multiname));
				
				uint32 n = argc+1; // index of receiver
				checkPropertyMultiname(n, multiname);

				if (mir)
					emitCheckNull(sp-(n-1));
				Traits* base = emitCoerceSuper(sp-(n-1));

				Binding b = toplevel->getBinding(base, &multiname);
				if (AvmCore::isMethodBinding(b))
				{
					int disp_id = AvmCore::bindingToMethodId(b);
					AbstractFunction* m = base->getMethod(disp_id);
					if( !m ) verifyFailed(kCorruptABCError);
					emitCoerceArgs(m, argc);
					Traits* resultType = m->returnTraits();
					if (mir) 
					{
						mir->emitSetContext(state, m);
						mir->emitCall(state, OP_callsuperid, disp_id, argc, resultType);
					}
					state->pop_push(n, resultType);
					if (opcode == OP_callsupervoid)
						state->pop();
					break;
				}

				#ifdef DEBUG_EARLY_BINDING
				core->console << "verify callsuper " << base << " " << multiname->getName() << " from within " << info << "\n";
				#endif

				// TODO optimize other cases
				if (mir)
				{
					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, (uintptr)&multiname, argc, NULL);
				}
				state->pop_push(n, NULL);
				if (opcode == OP_callsupervoid)
					state->pop();
				break;
			}

			case OP_getsuper:
			{
				// stack in: obj [ns [name]]
				// stack out: value
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(1, 1, &multiname);
				uint32 n=1;
				checkPropertyMultiname(n, multiname);

				if (multiname.isAttr())
					verifyFailed(kIllegalOpMultinameError, core->toErrorString(&multiname));

				int ptrIndex = sp-(n-1);
				Traits* base = emitCoerceSuper(ptrIndex);

				Binding b = toplevel->getBinding(base, &multiname);
				Traits* propType = readBinding(base, b);

				if (mir)
				{
					emitCheckNull(ptrIndex);

					if (AvmCore::isSlotBinding(b))
					{
						int slot_id = AvmCore::bindingToSlotId(b);
						if (mir) mir->emit(state, OP_getslot, slot_id, ptrIndex, propType);
						state->pop_push(n, propType);
						break;
					}

					if (AvmCore::hasGetterBinding(b))
					{
						// Invoke the getter
						int disp_id = AvmCore::bindingToGetterId(b);
						AbstractFunction *f = base->getMethod(disp_id);
						AvmAssert(f != NULL);
						emitCoerceArgs(f, 0);
						Traits* resultType = f->returnTraits();
						if (mir) 
						{
							mir->emitSetContext(state, f);
							mir->emitCall(state, OP_callsuperid, disp_id, 0, resultType);
						}
						state->pop_push(n, resultType);
						break;
					}
				}

				#ifdef DEBUG_EARLY_BINDING
				core->console << "verify getsuper " << base << " " << multiname.getName() << " from within " << info << "\n";
				#endif

				if (mir) 
				{
					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, (uintptr)&multiname, 0, propType);
				}
				state->pop_push(n, propType);
				break;
			}

			case OP_setsuper:
			{
				// stack in: obj [ns [name]] value
				Multiname multiname;
				checkConstantMultiname(imm30, multiname);
				checkStackMulti(2, 0, &multiname);
				uint32 n=2;
				checkPropertyMultiname(n, multiname);

				if (multiname.isAttr())
					verifyFailed(kIllegalOpMultinameError, core->toErrorString(&multiname));

				int ptrIndex = sp-(n-1);
				Traits* base = emitCoerceSuper(ptrIndex);

				if (mir)
				{
					emitCheckNull(ptrIndex);

					Binding b = toplevel->getBinding(base, &multiname);
					Traits* propType = readBinding(base, b);

					if (AvmCore::isSlotBinding(b))
					{
						if (AvmCore::isVarBinding(b))
						{
							int slot_id = AvmCore::bindingToSlotId(b);
							emitCoerce(propType, sp);
							mir->emit(state, OP_setslot, slot_id, ptrIndex);
						}
						// else, it's a readonly slot so ignore
						state->pop(n);
						break;
					}

					if (AvmCore::isAccessorBinding(b))
					{
						if (AvmCore::hasSetterBinding(b))
						{
							// Invoke the setter
							int disp_id = AvmCore::bindingToSetterId(b);
							AbstractFunction *f = base->getMethod(disp_id);
							AvmAssert(f != NULL);
							emitCoerceArgs(f, 1);
							mir->emitSetContext(state, f);
							mir->emitCall(state, OP_callsuperid, disp_id, 1, f->returnTraits());
						}
						// else, ignore write to readonly accessor
						state->pop(n);
						break;
					}

					#ifdef DEBUG_EARLY_BINDING
					core->console << "verify setsuper " << base << " " << multiname.getName() << " from within " << info << "\n";
					#endif

					mir->emitSetContext(state, NULL);
					mir->emit(state, opcode, (uintptr)&multiname);
				}

				state->pop(n);
				break;
			}

			case OP_constructsuper:
			{
				// stack in: obj, args ...
				const uint32 argc = imm30;
				checkStack(argc+1, 0);

				int ptrIndex = sp-argc;
				Traits* baseTraits = emitCoerceSuper(ptrIndex); // check receiver

				AbstractFunction *f = baseTraits->init;
				AvmAssert(f != NULL);

				emitCoerceArgs(f, argc);

				if (mir)
				{
					mir->emitSetContext(state, f);
					emitCheckNull(ptrIndex);
					mir->emitCall(state, opcode, 0, argc, VOID_TYPE);
				}

				state->pop(argc+1);
				// this is a true void call, no result to push.
				break;
			}

			case OP_newobject: 
			{
				uint32 argc = imm30;
				checkStack(2*argc, 1);
				int n=0;
				while (argc-- > 0)
				{
					n += 2;
					peekType(STRING_TYPE, n); // name; will call intern on it 
				}
				if (mir) mir->emit(state, opcode, imm30, 0, OBJECT_TYPE);
				state->pop_push(n, OBJECT_TYPE, true);
				break;
			}

			case OP_newarray: 
			{
				const uint32 argc = imm30;
				checkStack(argc, 1);
				if (mir) mir->emit(state, opcode, argc, 0, ARRAY_TYPE);
				state->pop_push(argc, ARRAY_TYPE, true);
				break;
			}

			case OP_pushscope: 
			{
				checkStack(1,0);
				Traits* scopeTraits = state->peek().traits;
				
				if (state->scopeDepth+1 <= max_scope) 
				{
					if (scope->fullsize > (scope->size+state->scopeDepth))
					{
						// extra constraints on type of pushscope allowed
						Traits* requiredType = scope->scopes[scope->size+state->scopeDepth].traits;
						if (!scopeTraits || !scopeTraits->containsInterface(requiredType))
						{
							verifyFailed(kIllegalOperandTypeError, core->toErrorString(scopeTraits), core->toErrorString(requiredType));
						}
					}
					if (mir)
					{
						emitCheckNull(sp);
						mir->emitCopy(state, sp, scopeBase+state->scopeDepth);
					}
					state->pop();
					state->setType(scopeBase+state->scopeDepth, scopeTraits, true, false);
					state->scopeDepth++;
				}
				else
				{
					verifyFailed(kScopeStackOverflowError);
				}
				break;
			}

			case OP_pushwith: 
			{
				checkStack(1,0);
				Traits* scopeTraits = state->peek().traits;
				
				if (state->scopeDepth+1 <= max_scope) 
				{
					if (mir)
					{
						emitCheckNull(sp);
						mir->emitCopy(state, sp, scopeBase+state->scopeDepth);
					}
					state->pop();
					state->setType(scopeBase+state->scopeDepth, scopeTraits, true, true);

					if (state->withBase == -1)
					{
						state->withBase = state->scopeDepth;
					}
						
					state->scopeDepth++;
				}
				else
				{
					verifyFailed(kScopeStackOverflowError);
				}
				break;
			}

			case OP_newactivation: 
			{
				if (!(info->flags & AbstractFunction::NEED_ACTIVATION))
				{
					verifyFailed(kInvalidNewActivationError);
				}
				//AvmAssert(!info->activationTraits->dynamic);
				// [ed] does the vm really care if an activation object is dynamic or not?
				checkStack(0, 1);
				if (mir) mir->emit(state, opcode, 0, 0, info->activationTraits);
				state->push(info->activationTraits, true);
				break;
			}

			case OP_newcatch: 
			{
				const int index = imm30;

				/*if (!(info->flags & AbstractFunction::NEED_ACTIVATION))
				{
					verifyFailed(kInvalidNewActivationError);
				}*/

				if (index < 0 || !info->exceptions || index >= info->exceptions->exception_count)
				{
					// todo better error msg
					verifyFailed(kInvalidNewActivationError);
				}
					
				ExceptionHandler* handler = &info->exceptions->exceptions[index];
				
				checkStack(0, 1);
				if (mir) mir->emit(state, opcode, 0, 0, handler->scopeTraits);
				state->push(handler->scopeTraits, true);
				break;
			}
			
			case OP_popscope:
			{
				//checkStack(0,0)
				// no code to emit
				if (state->scopeDepth-- <= outer_depth)
				{
					verifyFailed(kScopeStackUnderflowError);
				}
				#ifdef DEBUGGER
				if (mir) mir->emitKill(state, scopeBase + state->scopeDepth);
				#endif
				if (state->withBase >= state->scopeDepth)
				{
					state->withBase = -1;
				}
				break;
			}

			case OP_getscopeobject: 
			{
				checkStack(0,1);
				int scope_index = imm8;
				// local scope
				if (scope_index < state->scopeDepth)
				{
					if (mir) mir->emitCopy(state, scopeBase+scope_index, sp+1);
					// this will copy type and all attributes too
					state->push(state->scopeValue(scope_index));
				}
				else
				{
					verifyFailed(kGetScopeObjectBoundsError, core->toErrorString(imm8));
				}
				break;
			}

			case OP_getglobalscope: 
			{
				checkStack(0,1);
				emitGetGlobalScope();
				break;
			}
			
			case OP_getglobalslot: 
			{
				checkStack(0,1);
				emitGetGlobalScope();
				emitGetSlot(imm30-1);
				break;
			}

			case OP_setglobalslot: 
			{
				if (!state->scopeDepth && !scope->size)
				{
					verifyFailed(kNoGlobalScopeError);
				}
				Traits *globalTraits = scope->size > 0 ? scope->scopes[0].traits : state->scopeValue(0).traits;
				checkStack(1,0);
				checkEarlySlotBinding(globalTraits);
				Traits* slotTraits = checkSlot(globalTraits, imm30-1);
				if (mir)
				{
					emitCoerce(slotTraits, state->sp());
					mir->emit(state, opcode, imm30-1, sp, slotTraits);
				}
				state->pop();
				break;
			}
			
			case OP_getslot:
			{
				checkStack(1,1);
				emitGetSlot(imm30-1);
				break;
			}

			case OP_setslot: 
			{
				checkStack(2,0);
				emitSetSlot(imm30-1);
				break;
			}

			case OP_pop:
			{
				checkStack(1,0);
				state->pop();
				break;
			}

			case OP_dup: 
			{
				checkStack(1, 2);
				Value& v = state->peek();
				if (mir) mir->emitCopy(state, sp, sp+1);
				state->push(v);
				break;
			}

			case OP_swap: 
			{
				checkStack(2,2);
				emitSwap();
				break;
			}

			case OP_lessthan:
			case OP_greaterthan:
			case OP_lessequals:
			case OP_greaterequals:
			{
				checkStack(2,1);
				emitCompare(opcode);
				break;
			}

			case OP_equals:
			case OP_strictequals:
			case OP_instanceof:
			case OP_in:
			{
				checkStack(2,1);
				if (mir) mir->emit(state, opcode, 0, 0, BOOLEAN_TYPE);
				state->pop_push(2, BOOLEAN_TYPE);
				break;
			}

			case OP_not:
				checkStack(1,1);
				if (mir)
				{
					emitCoerce(BOOLEAN_TYPE, sp);
					mir->emit(state, opcode, sp);
				}
				state->pop_push(1, BOOLEAN_TYPE);
				break;

			case OP_add: 
			{
				checkStack(2,1);
				Value& rhs = state->peek(1);
				Value& lhs = state->peek(2);
				Traits* lhst = lhs.traits;
				Traits* rhst = rhs.traits;
				if (lhst == STRING_TYPE && lhs.notNull || rhst == STRING_TYPE && rhs.notNull)
				{
					if (mir)
					{
						emitToString(OP_convert_s, sp-1);
						emitToString(OP_convert_s, sp);
						mir->emit(state, OP_concat, 0, 0, STRING_TYPE);
					}
					state->pop_push(2, STRING_TYPE, true);
				}
				else if (lhst && lhst->isNumeric && rhst && rhst->isNumeric)
				{
					if (mir)
					{
						emitCoerce(NUMBER_TYPE, sp-1);
						emitCoerce(NUMBER_TYPE, sp);
						mir->emit(state, OP_add_d, 0, 0, NUMBER_TYPE);
					}
					state->pop_push(2, NUMBER_TYPE);
				}
				else
				{
					if (mir) mir->emit(state, opcode, 0, 0, OBJECT_TYPE);
					// dont know if it will return number or string, but neither will be null.
					state->pop_push(2,OBJECT_TYPE, true);
				}
				break;
			}

			case OP_modulo:
			case OP_subtract:
			case OP_divide:
			case OP_multiply:
				checkStack(2,1);
				if (mir)
				{
					emitCoerce(NUMBER_TYPE, sp-1); // convert LHS to number
					emitCoerce(NUMBER_TYPE, sp); // convert RHS to number
					mir->emit(state, opcode, 0, 0, NUMBER_TYPE);
				}
				state->pop_push(2, NUMBER_TYPE);
				break;

			case OP_negate:
				checkStack(1,1);
				emitCoerce(NUMBER_TYPE, sp);
				if (mir) mir->emit(state, opcode, sp, 0, NUMBER_TYPE);
				break;

			case OP_increment:
			case OP_decrement:
				checkStack(1,1);
				emitCoerce(NUMBER_TYPE, sp);
				if (mir) mir->emit(state, opcode, sp, opcode == OP_increment ? 1 : -1, NUMBER_TYPE);
				break;

			case OP_increment_i:
			case OP_decrement_i:
				checkStack(1,1);
				emitCoerce(INT_TYPE, sp);
				if (mir) mir->emit(state, opcode, state->sp(), opcode == OP_increment_i ? 1 : -1, INT_TYPE);
				break;

			case OP_add_i:
			case OP_subtract_i:
			case OP_multiply_i:
				checkStack(2,1);
				if (mir)
				{
					emitCoerce(INT_TYPE, sp-1);
					emitCoerce(INT_TYPE, sp);
					mir->emit(state, opcode, 0, 0, INT_TYPE);
				}
				state->pop_push(2, INT_TYPE);
				break;

			case OP_negate_i:
				checkStack(1,1);
				emitCoerce(INT_TYPE, sp);
				if (mir) mir->emit(state, opcode, sp, 0, INT_TYPE);
				break;

			case OP_bitand:
			case OP_bitor:
			case OP_bitxor:
				checkStack(2,1);
				if (mir)
				{
					emitCoerce(INT_TYPE, sp-1);
					emitCoerce(INT_TYPE, sp);
					mir->emit(state, opcode, 0, 0, INT_TYPE);
				}
				state->pop_push(2, INT_TYPE);
				break;

			// ISSUE do we care if shift amount is signed or not?  we mask 
			// the result so maybe it doesn't matter.
			// CN says see tests e11.7.2, 11.7.3, 9.6
			case OP_lshift:
			case OP_rshift:
				checkStack(2,1);
				if (mir)
				{
					emitCoerce(INT_TYPE, sp-1); // lhs
					emitCoerce(UINT_TYPE, sp); // rhs
					mir->emit(state, opcode, 0, 0, INT_TYPE);
				}
				state->pop_push(2, INT_TYPE);
				break;

			case OP_urshift:
				checkStack(2,1);
				if (mir)
				{
					emitCoerce(UINT_TYPE, sp-1); // lhs
					emitCoerce(UINT_TYPE, sp); // rhs
					mir->emit(state, opcode, 0, 0, UINT_TYPE);
				}
				state->pop_push(2, UINT_TYPE);
				break;

			case OP_bitnot:
				checkStack(1,1);
				emitCoerce(INT_TYPE, sp); // lhs
				if (mir) mir->emit(state, opcode, sp, 0, INT_TYPE);
				break;

			case OP_typeof:
			{
				checkStack(1,1);
				if (mir) mir->emit(state, opcode, sp, 0, STRING_TYPE);
				state->pop_push(1, STRING_TYPE, true);
				break;
			}

			case OP_bkpt:
			case OP_bkptline:
			case OP_nop:
			case OP_label:
			case OP_debug:
				break;

			case OP_debugline:
				#ifdef DEBUGGER
				// we actually do generate code for these, in debugger mode
				if (mir) mir->emit(state, opcode, imm30);
				#endif
				break;

			case OP_nextvalue:
			case OP_nextname:
			{
				checkStack(2,1);
				peekType(INT_TYPE,1);
				if (mir) mir->emit(state, opcode, 0, 0, NULL);
				state->pop_push(2, NULL);
				break;
			}

			case OP_hasnext:
			{
				checkStack(2,1);
				peekType(INT_TYPE,1);
				if (mir) mir->emit(state, opcode, 0, 0, INT_TYPE);
				state->pop_push(2, INT_TYPE);
				break;
			}

			case OP_hasnext2:
			{
				checkStack(0,1);
				checkLocal(imm30);
				Value& v = checkLocal(imm30b);
				if (imm30 == imm30b)
				{
					verifyFailed(kInvalidHasNextError);
				}
				if (v.traits != INT_TYPE)
				{
					verifyFailed(kIllegalOperandTypeError, core->toErrorString(v.traits), core->toErrorString(INT_TYPE));
				}
				if (mir) mir->emit(state, opcode, imm30, imm30b, BOOLEAN_TYPE);
				state->setType(imm30, NULL, false);
				state->push(BOOLEAN_TYPE);
				break;
			}

			case OP_abs_jump:
			{
				// first ensure the executing code isn't user code (only VM generated abc can use this op)
				if(pool->isCodePointer(pc))
				{
					verifyFailed(kIllegalOpcodeError, core->toErrorString(info), core->toErrorString(OP_abs_jump), core->toErrorString(pc-code_pos));
				}

				const byte* new_pc = (const byte*) imm30;
				#ifdef AVMPLUS_64BIT
				new_pc = (const byte *) (uintptr(new_pc) | (((uintptr) imm30b) << 32));
				const byte* new_code_end = new_pc + AvmCore::readU30 (nextpc);
				#else
				const byte* new_code_end = new_pc + imm30b;
				#endif

				// now ensure target points to within pool's script buffer
				if(!pool->isCodePointer(new_pc))
				{
					verifyFailed(kIllegalOpcodeError, core->toErrorString(info), core->toErrorString(OP_abs_jump), core->toErrorString(pc-code_pos));
				}

				// FIXME: what other verification steps should we do here?

				pc = code_pos = new_pc;
				code_end = new_code_end;
				size = 0;
				
				//set mir abcStart/End
				if(mir) {
					mir->abcStart = pc;
					mir->abcEnd = code_end;
					//core->GetGC()->Free((void*) info->body_pos);
					//info->body_pos = NULL;
				}

				exceptions_pos = code_end;
				code_length = int(code_end - pc);
				parseExceptionHandlers();

				break;
			}

			default:
				// size was nonzero, but no case handled the opcode.  someone asleep at the wheel!
				AvmAssert(false);
			}
		}
		
		if (!blockEnd)
		{
			verifyFailed(kCannotFallOffMethodError);
		}
		if (blockStates && actualCount != labelCount) 
		{
			verifyFailed(kInvalidBranchTargetError);
		}

		#ifdef AVMPLUS_INTERP
		if (!mir || mir->overflow) 
		{
			if (info->returnTraits() == NUMBER_TYPE)
				info->implN = Interpreter::interpN;
			else
				info->impl32 = Interpreter::interp32;
		}
		else
		#endif //AVMPLUS_INTERP
		{
			mir->epilogue(state);
		}

		#ifdef FEATURE_BUFFER_GUARD
		#ifdef AVMPLUS_MIR
		#ifdef AVMPLUS_LINUX
		delete growthGuard;
		#endif //AVMPLUS_LINUX
		growthGuard = NULL;
		#endif
		#endif

		} CATCH (Exception *exception) {

			// clean up growthGuard
#ifdef FEATURE_BUFFER_GUARD
#ifdef AVMPLUS_MIR
		    // Make sure the GrowthGuard is unregistered
			if (growthGuard)
			{
					growthGuard->~GrowthGuard();
//					growthGuard = NULL;
			}
#endif
#endif
			// re-throw exception
			core->throwException(exception);
		}
		END_CATCH
		END_TRY
	}

	void Verifier::checkPropertyMultiname(uint32 &depth, Multiname &multiname)
	{
		if (multiname.isRtname())
		{
			if (multiname.isQName())
			{
				// a.ns::@[name] or a.ns::[name]
				peekType(STRING_TYPE, depth++);
			}
			else
			{
				// a.@[name] or a[name]
				depth++;
			}
		}

		if (multiname.isRtns())
		{
			peekType(NAMESPACE_TYPE, depth++);
		}
	}

	void Verifier::emitCompare(AbcOpcode opcode)
	{
		// if either the LHS or RHS is a number type, then we know
		// it will be a numeric comparison.

		Value& rhs = state->peek(1);
		Value& lhs = state->peek(2);
		Traits *lhst = lhs.traits;
		Traits *rhst = rhs.traits;
		if (mir)
		{
			if (rhst && rhst->isNumeric && lhst && !lhst->isNumeric)
			{
				// convert lhs to Number
				emitCoerce(NUMBER_TYPE, state->sp()-1);
			}
			else if (lhst && lhst->isNumeric && rhst && !rhst->isNumeric)
			{
				// promote rhs to Number
				emitCoerce(NUMBER_TYPE, state->sp());
			}
			mir->emit(state, opcode, 0, 0, BOOLEAN_TYPE);
		}
		state->pop_push(2, BOOLEAN_TYPE);
	}

	void Verifier::emitFindProperty(AbcOpcode opcode, Multiname& multiname)
	{
		ScopeTypeChain* scope = info->declaringTraits->scope;
		bool early = false;
		if (multiname.isBinding())
		{
			int index = scopeBase + state->scopeDepth - 1;
			int base = scopeBase;
			if (scope->size == 0)
			{
				// if scope->size = 0, then global is a local
				// scope, and we dont want to early bind to global.
				base++;
			}
			for (; index >= base; index--)
			{
				Value& v = state->value(index);
				Binding b = toplevel->getBinding(v.traits, &multiname);
				if (b != BIND_NONE)
				{
					if (mir) mir->emitCopy(state, index, state->sp()+1);
					state->push(v);
					early = true;
					break;
				}
				else if (v.isWith)
				{
					// with scope could have dynamic property
					break;
				}
			}
			if (!early && index < base)
			{
				// look at captured scope types
				for (index = scope->size-1; index > 0; index--)
				{
					Traits* t = scope->scopes[index].traits;
					Binding b = toplevel->getBinding(t, &multiname);
					if (b != BIND_NONE)
					{
						if (mir) mir->emitGetscope(state, index, state->sp()+1);
						state->push(t, true);
						early = true;
						break;
					}
					else if (scope->scopes[index].isWith)
					{
						// with scope could have dynamic property
						break;
					}
				}
				if (!early && index <= 0)
				{
					// look at import table for a suitable script
					AbstractFunction* script = (AbstractFunction*)pool->getNamedScript(&multiname);
					if (script != (AbstractFunction*)BIND_NONE && script != (AbstractFunction*)BIND_AMBIGUOUS)
					{
						if (mir)
						{
							if (script == info)
							{
								// ISSUE what if there is an ambiguity at runtime? is VT too early to bind?
								// its defined here, use getscopeobject 0
								if (scope->size > 0)
								{
									mir->emitGetscope(state, 0, state->sp()+1);
								}
								else
								{
									mir->emitCopy(state, scopeBase, state->sp()+1);
								}
							}
							else
							{
								// found a single matching traits
								mir->emit(state, OP_finddef, (uintptr)&multiname, state->sp()+1, script->declaringTraits);
							}
						}
						state->push(script->declaringTraits, true);
						early = true;
						return;
					}
				}
			}
		}

		if (!early)
		{
			uint32 n=1;
			checkPropertyMultiname(n, multiname);
			if (mir) mir->emit(state, opcode, (uintptr)&multiname, 0, OBJECT_TYPE);
			state->pop_push(n-1, OBJECT_TYPE, true);
		}
	}

	void Verifier::emitGetProperty(Multiname &multiname, int n)
	{
		Value& obj = state->peek(n); // object

		if (mir) emitCheckNull(state->sp()-(n-1));

		Binding b = toplevel->getBinding(obj.traits, &multiname);
		Traits* propType = readBinding(obj.traits, b);

		if (AvmCore::isSlotBinding(b))
		{
			// early bind to slot
			if (mir) mir->emit(state, OP_getslot, AvmCore::bindingToSlotId(b), state->sp(), propType);
			state->pop_push(n, propType);
			return;
		}

		// Do early binding to accessors.
		if (AvmCore::hasGetterBinding(b))
		{
			// Invoke the getter
			int disp_id = AvmCore::bindingToGetterId(b);
			AbstractFunction *f = obj.traits->getMethod(disp_id);
			AvmAssert(f != NULL);
			if (mir)
			{
				emitCoerceArgs(f, 0);
				mir->emitSetContext(state, f);
				if (!obj.traits->isInterface)
					mir->emitCall(state, OP_callmethod, disp_id, 0, propType);
				else
					mir->emitCall(state, OP_callinterface, f->iid(), 0, propType);
			}
			AvmAssert(propType == f->returnTraits());
			state->pop_push(n, propType);
			return;
		}

		#ifdef DEBUG_EARLY_BINDING
		core->console << "verify getproperty " << obj.traits << " " << multiname->getName() << " from within " << info << "\n";
		#endif

		// default - do getproperty at runtime

		if (mir)
		{
			mir->emitSetContext(state, NULL);
			mir->emit(state, OP_getproperty, (uintptr)&multiname, 0, propType);
		}
		state->pop_push(n, propType);
	}

	void Verifier::emitGetGlobalScope()
	{
		ScopeTypeChain* scope = info->declaringTraits->scope;
		int captured_depth = scope->size;
		if (captured_depth > 0)
		{
			// enclosing scope
			if (mir) mir->emitGetscope(state, 0, state->sp()+1);
			state->push(scope->scopes[0].traits, true);
		}
		else
		{
			// local scope
			if (state->scopeDepth > 0)
			{
				if (mir) mir->emitCopy(state, scopeBase, state->sp()+1);
				// this will copy type and all attributes too
				state->push(state->scopeValue(0));
			}
			else
			{
				#ifdef _DEBUG
				if (pool->isBuiltin)
					core->console << "getglobalscope >= depth (0) "<< state->scopeDepth << "\n";
				#endif
				verifyFailed(kGetScopeObjectBoundsError, core->toErrorString(0));
			}
		}
	}

	void Verifier::emitGetSlot(int slot)
	{
		Value& obj = state->peek();
		checkEarlySlotBinding(obj.traits);
		Traits* slotTraits = checkSlot(obj.traits, slot);
		if (mir)
		{
			emitCheckNull(state->sp());
			mir->emit(state, OP_getslot, slot, state->sp(), slotTraits);
		}
		state->pop_push(1, slotTraits);
	}

	void Verifier::emitSetSlot(int slot)
	{
		Value& obj = state->peek(2); // object
		// if code isn't in pool, its our generated init function which we always
		// allow early binding on
		if(pool->isCodePointer(info->body_pos))
			checkEarlySlotBinding(obj.traits);
		Traits* slotTraits = checkSlot(obj.traits, slot);
		if (mir)
		{
			emitCoerce(slotTraits, state->sp());
			emitCheckNull(state->sp()-1);
			mir->emit(state, OP_setslot, slot, state->sp()-1, slotTraits);
		}
		state->pop(2);
	}

	void Verifier::emitSwap()
	{
		if (mir) mir->emitSwap(state, state->sp(), state->sp()-1);
		Value v1 = state->peek(1);
		Value v2 = state->peek(2);
		state->pop(2);
		state->push(v1);
		state->push(v2);
	}

	FrameState *Verifier::getFrameState(sintptr targetpc)
	{
		const byte *target = code_pos + targetpc;
		FrameState *targetState;
		// get state for target address or create a new one if none exists
		if (!blockStates)
		{
			blockStates = new (core->GetGC()) SortedIntMap<FrameState*>(core->GetGC(), 128);
		}
		if ( (targetState = blockStates->get((uintptr)target)) == 0 ) 
		{
			targetState = newFrameState();
			targetState->pc = target - code_pos;
			blockStates->put((uintptr)target, targetState);
			labelCount++;
		}
		return targetState;
	}
	
	void Verifier::emitToString(AbcOpcode opcode, int i)
	{
		Traits *st = STRING_TYPE;
		Value& value = state->value(i);
		Traits *in = value.traits;
		if (in != st || !value.notNull || opcode != OP_convert_s)
		{
			if (mir)
			{
				if (opcode == OP_convert_s && in && 
					(value.notNull || in->isNumeric || in == BOOLEAN_TYPE))
				{
					mir->emitCoerce(state, i, st);
				}
				else
				{
					mir->emit(state, opcode, i, 0, st);
				}
			}
			value.traits = st;
			value.notNull = true;
		}
	}

	void Verifier::emitCheckNull(int i)
	{
		Value& value = state->value(i);
		if (!value.notNull)
		{
			mir->emitCheckNull(state, i);
			for (int j=0, n = frameSize; j < n; j++) 
			{
				// also mark all copies of value.ins as non-null
				Value &v2 = state->value(j);
				if (v2.ins == value.ins)
					v2.notNull = true;
			}
		}
	}

	void Verifier::checkCallMultiname(AbcOpcode /*opcode*/, Multiname* name) const
	{
		if (name->isAttr())
		{
			verifyFailed(kIllegalOpMultinameError, core->toErrorString(name));
		}
	}

	Traits* Verifier::emitCoerceSuper(int index)
	{
		Traits* base = info->declaringTraits->base;
		if (base != NULL)
		{
			if (mir)
				emitCoerce(base, index);
		}
		else
		{
			verifyFailed(kIllegalSuperCallError, core->toErrorString(info));
		}
		return base;
	}

	void Verifier::emitCoerce(Traits* target, int index)
	{
		Value &v = state->value(index);
		Traits* rhs = v.traits;
		if (mir && (!canAssign(target, rhs) || !Traits::isMachineCompatible(target,rhs)))
			mir->emitCoerce(state, index, target);
		state->setType(index, target, v.notNull);
	}

	Traits* Verifier::peekType(Traits* requiredType, int n)
	{
		Traits* t = state->peek(n).traits;
		if (t != requiredType)
		{
			verifyFailed(kIllegalOperandTypeError, core->toErrorString(t), core->toErrorString(requiredType));
		}
		return t;
	}

	void Verifier::checkEarlySlotBinding(Traits* t)
	{
		bool slotAllow;
		pool->allowEarlyBinding(t, slotAllow);
		if (!slotAllow)
		{
			// illegal disp_id or slot_id access since dispatch table layout and instance layout
			// are not known at compile time
			verifyFailed(kIllegalEarlyBindingError, core->toErrorString(t));
		}
	}

	void Verifier::checkEarlyMethodBinding(Traits* t)
	{
		bool slotAllow;
		pool->allowEarlyBinding(t, slotAllow);
		if (true)
		{
			// illegal disp_id or slot_id access since dispatch table layout and instance layout
			// are not known at compile time
			verifyFailed(kIllegalEarlyBindingError, core->toErrorString(t));
		}
	}

	void Verifier::emitCoerceArgs(AbstractFunction* m, int argc)
	{
		if (!m->argcOk(argc))
		{
			verifyFailed(kWrongArgumentCountError, core->toErrorString(m), core->toErrorString(m->requiredParamCount()), core->toErrorString(argc));
		}

		m->resolveSignature(toplevel);

		// coerce parameter types
		int n=1;
		while (argc > 0) 
		{
			if (mir)
			{
				Traits* target = (argc <= m->param_count) ? m->paramTraits(argc) : NULL;
				emitCoerce(target, state->sp()-(n-1));
			}
			argc--;
			n++;
		}

		// coerce receiver type
		if (mir)
			emitCoerce(m->paramTraits(0), state->sp()-(n-1));
	}

	bool Verifier::canAssign(Traits* lhs, Traits* rhs) const
	{
		if (!Traits::isMachineCompatible(lhs,rhs))
		{
			// no machine type is compatible with any other 
			return false;
		}

		if (!lhs)
			return true;

		// type on right must be same class or subclass of type on left.
		Traits* t = rhs;
		while (t != lhs && t != NULL)
			t = t->base;
		return t != NULL;
	}

	void Verifier::checkStack(uint32 pop, uint32 push)
	{
		if (uint32(state->stackDepth) < pop)
			verifyFailed(kStackUnderflowError);
		if (state->stackDepth-pop+push > uint32(max_stack))
			verifyFailed(kStackOverflowError);
	}

	void Verifier::checkStackMulti(uint32 pop, uint32 push, Multiname* m)
	{
		if (m->isRtname()) pop++;
		if (m->isRtns()) pop++;
		checkStack(pop,push);
	}

	Value& Verifier::checkLocal(int local)
	{
		if (local < 0 || local >= local_count)
			verifyFailed(kInvalidRegisterError, core->toErrorString(local));
		return state->value(local);
	}
	
	Traits* Verifier::checkSlot(Traits *traits, int imm30)
	{
        uint32 slot = imm30;
        if (!traits || slot >= traits->slotCount)
		{
			verifyFailed(kSlotExceedsCountError, core->toErrorString(slot+1), core->toErrorString(traits?traits->slotCount:0), core->toErrorString(traits));
		}
		traits->resolveSignatures(toplevel);

		return traits->getSlotTraits(slot);
	}

	Traits* Verifier::readBinding(Traits* traits, Binding b)
	{
		if (traits)
			traits->resolveSignatures(toplevel);
		switch (b&7)
		{
		default:
			AvmAssert(false); // internal error - illegal binding type
		case BIND_GET:
		case BIND_GETSET:
		{
			int m = AvmCore::bindingToGetterId(b);
			AbstractFunction *f = traits->getMethod(m);
			return f->returnTraits();
		}
		case BIND_SET:
			// TODO lookup type here. get/set must have same type.
		case BIND_NONE:
			// dont know what this is
			// fall through
		case BIND_METHOD:
			// extracted method or dynamic data, don't know which
			return NULL;
		case BIND_VAR:
		case BIND_CONST:
			return traits->getSlotTraits(AvmCore::bindingToSlotId(b));
		}
	}

	AbstractFunction* Verifier::checkMethodInfo(uint32 id)
	{
		if (id >= pool->methodCount)
		{
            verifyFailed(kMethodInfoExceedsCountError, core->toErrorString(id), core->toErrorString(pool->methodCount));
			return NULL;
		}
		else
		{
			return pool->methods[id];
		}
	}

	Traits* Verifier::checkClassInfo(uint32 id)
	{
		if (id >= pool->classCount)
		{
            verifyFailed(kClassInfoExceedsCountError, core->toErrorString(id), core->toErrorString(pool->classCount));
			return NULL;
		}
		else
		{
			return pool->cinits[id]->declaringTraits;
		}
	}

	Traits* Verifier::checkTypeName(uint32 index)
	{
		Multiname name;
		checkConstantMultiname(index, name); // CONSTANT_Multiname
		Traits *t = pool->getTraits(&name, toplevel);
		if (t == NULL)
			verifyFailed(kClassNotFoundError, core->toErrorString(&name));
		return t;
	}

    AbstractFunction* Verifier::checkDispId(Traits* traits, uint32 disp_id)
    {
        if (disp_id > traits->methodCount)
		{
            verifyFailed(kDispIdExceedsCountError, core->toErrorString(disp_id), core->toErrorString(traits->methodCount), core->toErrorString(traits));
			return NULL;
		}
		else
		{
			if (!traits->getMethod(disp_id)) 
			{
				verifyFailed(kDispIdUndefinedError, core->toErrorString(disp_id), core->toErrorString(traits));
			}
			return traits->getMethod(disp_id);
		}
    }

    void Verifier::verifyFailed(int errorID, Stringp arg1, Stringp arg2, Stringp arg3) const
    {
#ifdef FEATURE_BUFFER_GUARD
#ifdef AVMPLUS_MIR
		// Make sure the GrowthGuard is unregistered
		if (growthGuard)
		{
			growthGuard->~GrowthGuard();
		}
#endif
#endif

#ifdef AVMPLUS_VERBOSE
		if (!secondTry && !verbose)
		{
			// capture the verify trace even if verbose is false.
			Verifier v2(info, toplevel, true);
			v2.verbose = true;
			v2.verify(NULL);
		}
#endif
		core->throwErrorV(toplevel->verifyErrorClass(), errorID, arg1, arg2, arg3);

		// This function throws, and should never return.
		AvmAssert(false);
    }

    void Verifier::checkTarget(const byte* target)
    {
		FrameState *targetState = getFrameState(target-code_pos);
		AvmAssert(targetState != 0);
		if (!targetState->initialized)
		{
			//if (verbose)
			//	core->console << "merge first target=" << targetState->pc << "\n";
            // first time visiting target block
			targetState->init(state);
			targetState->initialized = true;

			// if this label is a loop header then clear the notNull flag for
			// any state entry that might become null in the loop body.  this
			// prevents us from needing to re-verify the loop, at a cost of a
			// few more null pointer checks. 
			if (targetState->targetOfBackwardsBranch)
			{
				// null check on all locals
				for (int i=0, n=local_count; i < n; i++)
					targetState->value(i).notNull = false;

				// and all stack entries
				for (int i=stackBase, n=i+state->stackDepth; i < n; i++)
					targetState->value(i).notNull = false;

				// we don't have to clear notNull on scope stack entries because we
				// check for null in op_pushscope/pushwith
			}
			
			//if (verbose)
			//	showState(targetState, targetState->pc+code_pos, false);
        }
        else
        {
			/*if (verbose)
			{
				core->console << "merge current=" << state->pc << "\n";
				showState(state, code_pos+state->pc, false);
				core->console << "merge target=" << targetState->pc << "\n";
				showState(targetState, code_pos+targetState->pc, false);
			}*/

			// check matching stack depth
            if (state->stackDepth != targetState->stackDepth) 
			{
				verifyFailed(kStackDepthUnbalancedError, core->toErrorString((int)state->stackDepth), core->toErrorString((int)targetState->stackDepth));
			}

			// check matching scope chain depth
            if (state->scopeDepth != targetState->scopeDepth)
			{
				verifyFailed(kScopeDepthUnbalancedError, core->toErrorString(state->scopeDepth), core->toErrorString(targetState->scopeDepth));
			}

			// merge types of locals, scopes, and operands
			// ISSUE merge should preserve common interfaces even when
			// common supertype does not:
			//    class A implements I {}
			//    class B implements I {}
			//    var i:I = b ? new A : new B

			const int scopeTop  = scopeBase + targetState->scopeDepth;
			const int stackTop  = stackBase + targetState->stackDepth;
			Value* targetValues = &targetState->value(0);
			Value* curValues = &state->value(0);
			for (int i=0, n=stackTop; i < n; i++)
			{
				if (i >= scopeTop && i < stackBase) 
				{
					// invalid location, ignore it.
					continue;
				}

				Value& curValue = curValues[i];
				Value& targetValue = targetValues[i];
				if (curValue.killed || targetValue.killed) 
				{
					// this reg has been killed in one or both states;
					// ignore it.
					continue;
				}

				Traits* t1 = targetValue.traits;
				Traits* t2 = curValue.traits;
				bool isWith = curValue.isWith;

				if (isWith != targetValue.isWith) 
				{
					// failure: pushwith on one edge, pushscope on other edge, cannot merge.
					verifyFailed(kCannotMergeTypesError, core->toErrorString(t1), core->toErrorString(t2));
				}

				Traits* t3 = (t1 == t2) ? t1 : findCommonBase(t1, t2);
				
				#ifdef AVMPLUS_MIR
				if (mir)
					mir->merge(curValue, targetValue);
				#endif // AVMPLUS_MIR

				bool notNull = targetValue.notNull && curValue.notNull;
				if (targetState->pc < state->pc && 
					(t3 != t1 || t1 && !t1->isNumeric && notNull != targetValue.notNull))
				{
					// failure: merge on back-edge
					verifyFailed(kCannotMergeTypesError, core->toErrorString(t1), core->toErrorString(t3));
				}

				// if we're targeting a label we can't propagate notNull since we don't yet know 
				// the state of all the other possible branches.  Another possible fix would be to 
				// enforce a null check at each branch to the target.
				if (targetState->targetOfBackwardsBranch)
					notNull = false;

				targetState->setType(i, t3, notNull, isWith);
			}

			/*if (verbose) {
				core->console << "after merge\n";
				showState(targetState, code_pos+targetState->pc, false);
			}*/
        }
    }

	/**
	 * find common base class of these two types
	 */
	Traits* Verifier::findCommonBase(Traits* t1, Traits* t2)
	{
		AvmAssert(t1 != t2);

		if (t1 == NULL) {
			// assume t1 is always non-null
			Traits *temp = t1;
			t1 = t2;
			t2 = temp;
		}

		if (!Traits::isMachineCompatible(t1,t2))
		{
			// these two types are incompatible machine types that require
			// coersions before the join node.
			verifyFailed(kCannotMergeTypesError, core->toErrorString(t1), core->toErrorString(t2));
		}

		if (t1 == NULL_TYPE && t2 && !t2->isMachineType)
		{
			// okay to merge null with pointer type
			return t2;
		}
		if (t2 == NULL_TYPE && t1 && !t1->isMachineType)
		{
			// okay to merge null with pointer type
			return t1;
		}

		// all commonBase flags start out false.  set the cb bits on 
		// t1 and its ancestors.
		Traits* t = t1;
		do t->commonBase = true;
		while ((t = t->base) != NULL);

		// now search t2 and its ancestors looking for the first cb=true
		t = t2;
		while (t != NULL && !t->commonBase) 
			t = t->base;

		Traits* common = t;

		// finally reset the cb bits to false for next time
		t = t1;
		do t->commonBase = false;
		while ((t = t->base) != NULL);

		// found common base, possibly *
		if (!Traits::isMachineCompatible(t1,common) || !Traits::isMachineCompatible(t2,common))
		{
			// these two types are incompatible types that require
			// coersions before the join node.
			verifyFailed(kCannotMergeTypesError, core->toErrorString(t1), core->toErrorString(t2));
		}
		return common;
	}

    Atom Verifier::checkCpoolOperand(uint32 index, int requiredAtomType)
    {
		switch( requiredAtomType )
		{
		case kStringType:
			if( !index || index >= pool->constantStringCount )
			{
				verifyFailed(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantStringCount));
				return undefinedAtom;
			}
			return pool->cpool_string[index]->atom();

		case kObjectType:
			if( !index || index >= pool->constantMnCount )
			{
				verifyFailed(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantMnCount));
				return undefinedAtom;
			}
			return pool->cpool_mn[index];

		default:
			verifyFailed(kCpoolEntryWrongTypeError, core->toErrorString(index));
			return undefinedAtom;
		}
    }

	void Verifier::checkConstantMultiname(uint32 index, Multiname& m)
	{
		checkCpoolOperand(index, kObjectType);
		pool->parseMultiname(m, index);
	}

	Binding Verifier::findMathFunction(Traits* math, Multiname* multiname, Binding b, int argc)
	{
		Stringp newname = core->internString(core->concatStrings(core->constantString("_"), multiname->getName()));
		Binding newb = math->getName(newname);
		if (AvmCore::isMethodBinding(newb))
		{
			int disp_id = AvmCore::bindingToMethodId(newb);
			AbstractFunction* newf = math->getMethod(disp_id);
			if (argc == newf->param_count)
			{
				for (int i=state->stackDepth-argc, n=state->stackDepth; i < n; i++)
				{
					Traits* t = state->stackValue(i).traits;
					if (t != NUMBER_TYPE && t != INT_TYPE && t != UINT_TYPE)
						return b;
				}
				b = newb;
			}
		}
		return b;
	}

	Binding Verifier::findStringFunction(Traits* str, Multiname* multiname, Binding b, int argc)
	{
		Stringp newname = core->internString(core->concatStrings(core->constantString("_"), multiname->getName()));
		Binding newb = str->getName(newname);
		if (AvmCore::isMethodBinding(newb))
		{
			int disp_id = AvmCore::bindingToMethodId(newb);
			AbstractFunction* newf = str->getMethod(disp_id);
			// We have all required parameters but not more than required.
			if ((argc >= (newf->param_count - newf->optional_count)) && (argc <= newf->param_count))
			{
				for (int i=state->stackDepth-argc, k = 1, n=state->stackDepth; i < n; i++, k++)
				{
					Traits* t = state->stackValue(i).traits;
					if (t != newf->paramTraits(k))
						return b;
				}
				b = newb;
			}
		}
		return b;
	}

	void Verifier::parseExceptionHandlers()
	{
		const byte* pos = exceptions_pos;
		int exception_count = toplevel->readU30(pos);

		if (exception_count != 0) 
		{
			size_t extra = sizeof(ExceptionHandler)*(exception_count-1);
			ExceptionHandlerTable* table = new (core->GetGC(), extra) ExceptionHandlerTable(exception_count);
			ExceptionHandler *handler = table->exceptions;
			for (int i=0; i<exception_count; i++) 
			{
				handler->from = toplevel->readU30(pos);
				handler->to = toplevel->readU30(pos);
				handler->target = toplevel->readU30(pos);

				int type_index = toplevel->readU30(pos);
				Traits* t = type_index ? checkTypeName(type_index) : NULL;

				Multiname qn;
				int name_index = (pool->version != (46<<16|15)) ? toplevel->readU30(pos) : 0;
				if (name_index != 0)
				{
					pool->parseMultiname(qn, name_index);
				}

				#ifdef AVMPLUS_VERBOSE
				if (verbose)
				{
					core->console << "            exception["<<i<<"] from="<< handler->from
						<< " to=" << handler->to
						<< " target=" << handler->target 
						<< " type=" << t
						<< " name=";
					if (name_index != 0)
					    core->console << &qn;
					else
						core->console << "(none)";
					core->console << "\n";
				}
				#endif

				if (handler->from < 0 ||
					handler->to < handler->from ||
					handler->target < handler->to || 
					handler->target > code_length)
				{
					// illegal range in handler record
					verifyFailed(kIllegalExceptionHandlerError);
				}

				// handler->traits = t
				WB(core->GetGC(), table, &handler->traits, t);

				Traits* scopeTraits;
				
				if (name_index == 0)
				{
					scopeTraits = OBJECT_TYPE;
				}
				else
				{
					scopeTraits = core->newTraits(NULL, 1, 0, sizeof(ScriptObject));
					scopeTraits->pool = pool;
					scopeTraits->final = true;
					scopeTraits->defineSlot(qn.getName(), qn.getNamespace(), 0, BIND_VAR);
					scopeTraits->slotCount = 1;
					scopeTraits->initTables();
					AbcGen gen(core->GetGC());
					scopeTraits->setSlotInfo(0, 0, toplevel, t, scopeTraits->sizeofInstance, CPoolKind(0), gen);
					scopeTraits->setTotalSize(scopeTraits->sizeofInstance + 16);
					scopeTraits->linked = true;
				}
				
				// handler->scopeTraits = scopeTraits
				WB(core->GetGC(), table, &handler->scopeTraits, scopeTraits);

				
				getFrameState(handler->target)->targetOfBackwardsBranch = true;

				handler++;
			}

			//info->exceptions = table;
			WB(core->GetGC(), info, &info->exceptions, table);
		}
		else
		{
			info->exceptions = NULL;
		}
	}

#ifdef AVMPLUS_VERBOSE
	/**
     * display contents of current stack frame only.
     */
    void Verifier::showState(FrameState *state, const byte* pc, bool unreachable)
    {
		// stack
		core->console << "                        stack:";
		for (int i=0, n=state->stackDepth; i < n; i++) {
			core->console << " ";
			printValue(state->stackValue(i));
		}
		core->console << '\n';

        // scope chain
		core->console << "                        scope: ";
		if (info->declaringTraits->scope && info->declaringTraits->scope->size > 0)
		{
			core->console << "[";
			for (int i=0, n=info->declaringTraits->scope->size; i < n; i++)
			{
				Value v;
				v.traits = info->declaringTraits->scope->scopes[i].traits;
				v.isWith = info->declaringTraits->scope->scopes[i].isWith;
				v.killed = false;
				v.notNull = true;
				#ifdef AVMPLUS_MIR
				v.ins = 0;
				v.stored = false;
				#endif
				printValue(v);
				if (i+1 < n)
					core->console << " ";
			}
			core->console << "] ";
		}
		for (int i=0, n=state->scopeDepth; i < n; i++) 
		{
            printValue(state->scopeValue(i));
			core->console << " ";
        }
		core->console << '\n';

        // locals
		core->console << "                         locals: ";
		for (int i=0, n=scopeBase; i < n; i++) {
            printValue(state->value(i));
			core->console << " ";
        }
		core->console << '\n';

		// opcode
		if (unreachable)
			core->console << "- ";
		else
			core->console << "  ";
		core->console << state->pc << ':';
        core->formatOpcode(core->console, pc, (AbcOpcode)*pc, state->pc, pool);
		core->console << '\n';
    }

	void Verifier::printValue(Value& v)
	{
		Traits* t = v.traits;
		if (!t)
		{
			core->console << "*";
		}
		else
		{
			core->console << t->format(core);
			if (!t->isNumeric && !v.notNull && t != BOOLEAN_TYPE && t != NULL_TYPE)
				core->console << "?";
		}
#ifdef AVMPLUS_MIR
		if (mir && v.ins)
			mir->formatOperand(core->console, v.ins, mir->ipStart);
#endif
	}

#endif /* AVMPLUS_VERBOSE */

	FrameState* Verifier::newFrameState()
	{
		size_t extra = (frameSize-1)*sizeof(Value);
		return new (core->GetGC(), extra) FrameState(this);
	}
}
