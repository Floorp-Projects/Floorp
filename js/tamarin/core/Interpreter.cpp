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

#ifdef AVMPLUS_MAC
#ifndef __GNUC__
// inline_max_total_size() defaults to 10000.
// This module includes so many inline functions that we
// exceed this limit and we start getting compile warnings,
// so bump up the limit for this file. 
#pragma inline_max_total_size(26576)
#endif
#endif

#ifdef AVMPLUS_INTERP
namespace avmplus
{	
	Atom Interpreter::interp32(MethodEnv* env, int argc, uint32 *ap)
	{
		Atom a = interp(env, argc, ap);
		Traits* t = env->method->returnTraits();
		AvmCore* core = env->core();
		if (!t || t == OBJECT_TYPE || t == VOID_TYPE)
			return a;
		if (t == INT_TYPE)
			return AvmCore::integer_i(a);
		if (t == UINT_TYPE)
			return AvmCore::integer_u(a);
		if (t == BOOLEAN_TYPE)
			return a>>3;
		return a & ~7; // possibly null pointer
	}

	double Interpreter::interpN(MethodEnv* env, int argc, uint32 * ap)
	{
		Atom a = interp(env, argc, ap);
		return AvmCore::number_d(a);
	}

    /**
     * Interpret the AVM+ instruction set.
     * @return
     */
    Atom Interpreter::interp(MethodEnv *env, int argc, uint32 *ap)
    {

		MethodInfo* info = (MethodInfo*)(AbstractFunction*) env->method;
		AvmCore *core = info->core();

		if (core->minstack)
		{
			// Take the address of a local variable to get
			// stack pointer
			uintptr sp = (uintptr)&core;
			if (sp < core->minstack)
			{
				env->vtable->traits->core->stackOverflow(env);
			}
		}

		#ifdef AVMPLUS_VERBOSE
		if (info->pool->verbose)
			core->console << "interp " << info << '\n';
		#endif

		const byte* pos = info->body_pos;
		int max_stack = AvmCore::readU30(pos);
		int local_count = AvmCore::readU30(pos);
		int init_scope_depth = AvmCore::readU30(pos);
		int max_scope_depth = AvmCore::readU30(pos);
		int max_scope = MethodInfo::maxScopeDepth(info, max_scope_depth - init_scope_depth);
		AvmCore::readU30(pos); // code_length
		const byte * volatile code_start = pos;
		
		// these should have been checked in AbcParser
		AvmAssert(local_count+max_scope+max_stack > 0);
		Atom* framep = (Atom*)alloca(sizeof(Atom)*(local_count + max_scope + max_stack));
		Atom* scopeBase = framep + local_count;
		Atom* withBase = NULL;

		#ifdef DEBUGGER
		CallStackNode callStackNode(env, info, framep, 0, argc, ap, 0 /* later changed to 'pc' */);
		// don't allow entry into the debugger until we have setup the frame
		#endif

		CodeContextAtom savedCodeContext = core->codeContextAtom;
		if (info->pool->domain->base != NULL) {
			core->codeContextAtom = (CodeContextAtom)env | CONTEXT_ENV;
		}

		Atom* atomv = (Atom*)ap;
		info->boxArgs(argc, ap, atomv);

		// 1. copy instance and args to local frame
		for (int i=0, n = argc < info->param_count ? argc : info->param_count; i <= n; i++)
		{
			framep[i] = atomv[i];
		}

		// Store original value of argc for createRest and createArguments.
		// argc may be changed by the optional parameter check below.
		int arguments_argc = argc;

		// set optional param values.  these not aliased to arguments[] since arguments[]
		// only present with traditional prototype functions (no optional args)
		if (info->flags & AbstractFunction::HAS_OPTIONAL)
		{
			if (argc < info->param_count)
			{
				// initialize default values
				for (int i=argc+1, o=argc + info->optional_count - info->param_count, n=info->param_count; i <= n; i++, o++)
				{
					framep[i] = info->getDefaultValue(o);
				}
				argc = info->param_count;
			}
		}

		// 4. set remaining locals to undefined.  Don't have to init scope or stack because
		// our conservative GC scan knows how to ignore garbage.
		for (Atom *p = framep + 1 + info->param_count; p < scopeBase; p++)
		{
			*p = undefinedAtom;
		}

		Toplevel *const toplevel = env->toplevel();

		// 2. capture arguments or rest array.
		if (info->flags & AbstractFunction::NEED_REST)
		{
			framep[info->param_count+1] = env->createRest(atomv,arguments_argc)->atom();
		}
		else if (info->flags & AbstractFunction::NEED_ARGUMENTS)
		{
			// create arguments using atomv[1..argc].
			// Even tho E3 says create an Object, E4 says create an Array so thats what we will do.
			framep[info->param_count+1] = env->createArguments(atomv, arguments_argc)->atom();
		}
		
		// 3. create the activation object, if necessary

		// init the scope chain by copying it from the captured scope
		ScopeChain* scope = env->vtable->scope;
		Namespace *dxns = scope->defaultXmlNamespace;
		Namespace **dxnsAddr;
		Namespace *const*dxnsAddrSave = NULL;

		if(info->setsDxns()) {
			dxnsAddrSave = core->dxnsAddr;
			dxnsAddr = &dxns;
		} else {
			dxnsAddr = scope->getDefaultNamespaceAddr();
		}

		int outer_depth = scope->getSize();
		int scopeDepth = 0;
	
		// make sure scope chain depth is right before entering.
		volatile int initialScopeDepth = scopeDepth;

		Atom tempAtom;

		PoolObject *pool = info->pool;
		const List<Stringp, LIST_RCObjects>& cpool_string = pool->cpool_string;
        const List<int,LIST_NonGCObjects>& cpool_int = pool->cpool_int;
        const List<uint32,LIST_NonGCObjects>& cpool_uint = pool->cpool_uint;
        const List<double*, LIST_GCObjects>& cpool_double = pool->cpool_double;
		const List<Namespace*, LIST_RCObjects>& cpool_ns = pool->cpool_ns;

		Atom *sp = scopeBase + max_scope - 1;

		#ifdef DEBUGGER
		Debugger* debugger = core->debugger;
		if (core->callStack)
			core->callStack->framep = framep;

		// notify the debugger that we are entering a new frame.
		env->debugEnter(argc, ap, NULL, local_count, NULL, framep, 0);  // call it but make sure that callStackNode is not re-init'd
		#endif

		const byte* pc = code_start;
		sintptr volatile expc;

		#ifdef DEBUGGER
		callStackNode.eip = &expc;
		callStackNode.scopeDepth = &scopeDepth;
		#endif

		// Mask that can be XOR'd to flip a boolean atom
		const int booleanNotMask  = trueAtom^falseAtom;

		// whether this sequence is interruptable or not.
		bool interruptable = (info->flags & AbstractFunction::NON_INTERRUPTABLE) ? false : true;

		core->branchCheck(env, interruptable, -1);

	  MainLoop:
		TRY_UNLESS(core, !info->exceptions, kCatchAction_SearchForActionScriptExceptionHandler) {
		
		
		// the verifier ensures we don't fall off the end of a method.  so
		// we dont have to check the end pointer here.
        for (;;)
        {
			// restore this every time since we might have called out to
			// code that changes it
			core->dxnsAddr = dxnsAddr;

			expc = pc-code_start;
			AbcOpcode opcode = (AbcOpcode) *pc++;

			#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
            {
                showState(info, opcode, pc - 1 - code_start, framep, sp-framep,
					scopeBase+scopeDepth-1-framep, scopeBase-framep, scopeBase+max_scope-framep,
					code_start);
            }
			#endif

			#ifdef AVMPLUS_PROFILE
			if (core->dprof.dprofile)
				core->dprof.mark(opcode);
			#endif

            switch (opcode)
            {
            case OP_returnvoid:
            case OP_returnvalue:
				#ifdef DEBUGGER
				env->debugExit(&callStackNode);
				#endif				
				core->codeContextAtom = savedCodeContext;

				tempAtom = toplevel->coerce((opcode==OP_returnvoid) ? undefinedAtom : *sp,
					info->returnTraits());

				#ifdef AVMPLUS_VERBOSE
				if (info->pool->verbose)
					core->console << "exit " << info << '\n';
				#endif

				return tempAtom;

            case OP_nop:
            case OP_label:
			case OP_timestamp:
                continue;

			case OP_bkpt:
				#ifdef DEBUGGER
				if (debugger)
				{
					debugger->enterDebugger();
				}
				#endif
				continue;

			case OP_debugline:
			{
			    #ifdef DEBUGGER
				int line = readU30(pc);
				if (debugger)
				{
					debugger->debugLine(line);
				}
				#else
				readU30(pc);
				#endif
				continue;
			}

			case OP_bkptline:
			{
			    #ifdef DEBUGGER
				int line = readU30(pc);
				if (debugger)
				{
					debugger->debugLine(line);
					debugger->enterDebugger();
				}
				#else
				readU30(pc);
				#endif
				continue;
			}

			case OP_debug:
				// tbd
				pc += AvmCore::calculateInstructionWidth(pc-1) - 1;
				continue;

			case OP_debugfile:
			{
				#ifdef DEBUGGER
				int index = readU30(pc);
				if (debugger)
				{
					debugger->debugFile(pool->getString(index));
				}
				#else
				readU30(pc);
				#endif
				continue;
			}

			case OP_jump:
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += 3+j;
				}
                continue;

            case OP_pushnull:
				sp++;
                sp[0] = nullObjectAtom;
                continue;
            case OP_pushundefined:
				sp++;
                sp[0] = undefinedAtom;
                continue;
            case OP_pushstring:
				sp++;
                sp[0] = cpool_string[readU30(pc)]->atom();
                continue;
            case OP_pushint:
				sp++;
                sp[0] = core->intToAtom(cpool_int[readU30(pc)]);
                continue;
            case OP_pushuint:
				sp++;
                sp[0] = core->uintToAtom(cpool_uint[readU30(pc)]);
                continue;
            case OP_pushdouble:
				sp++;
                sp[0] = kDoubleType|(uintptr)cpool_double[readU30(pc)];
                continue;
            case OP_pushnamespace:
                sp++;
                sp[0] = cpool_ns[readU30(pc)]->atom();
                continue;
            case OP_getlocal:
                sp++;
				sp[0] = framep[readU30(pc)];
				continue;
            case OP_getlocal0:
			case OP_getlocal1:
			case OP_getlocal2:
			case OP_getlocal3:
                sp++;
				sp[0] = framep[opcode-OP_getlocal0];
				continue;
            case OP_pushtrue:
                sp++;
				sp[0] = trueAtom;
                continue;
            case OP_pushfalse:
				sp++;
                sp[0] = falseAtom;
                continue;
			case OP_pushnan:
				sp++;
				sp[0] = core->kNaN;
				continue;

            case OP_pop:
                sp--;
                continue;

            case OP_dup:
				sp++;
				sp[0] = sp[-1];
                continue;

            case OP_swap:
                tempAtom = sp[0];
                sp[0] = sp[-1];
                sp[-1] = tempAtom;
                continue;

            case OP_convert_s:
                sp[0] = core->string(sp[0])->atom();
                continue;

			case OP_esc_xelem: // ToXMLString will call EscapeElementValue
				sp[0] = core->ToXMLString(sp[0])->atom();
				continue;

			case OP_esc_xattr:
				sp[0] = core->EscapeAttributeValue(sp[0])->atom();
				continue;

            case OP_coerce_d:
            case OP_convert_d:
                sp[0] = core->numberAtom(sp[0]);
                continue;

            case OP_convert_b:
            case OP_coerce_b:
                sp[0] = core->booleanAtom(sp[0]);
                continue;

			// if sp[0] is null or undefined, throw TypeError.  otherwise return same value.
            case OP_convert_o:
                env->nullcheck(sp[0]);
                continue;

            case OP_negate:
                sp[0] = core->doubleToAtom(-core->number(sp[0]));
                continue;

			case OP_negate_i:
                sp[0] = core->intToAtom(-core->integer(sp[0]));
                continue;

            case OP_increment:
				*sp = core->numberAtom(*sp);
				core->increment_d(sp, 1);
                continue;

            case OP_increment_i:
				core->increment_i(sp, 1);
                continue;

			case OP_inclocal:
			{
				Atom* rp = framep+readU30(pc);
				*rp = core->numberAtom(*rp);
				core->increment_d(rp, 1);
				continue;
			}

			case OP_kill:
			{
				framep[readU30(pc)] = undefinedAtom;
				continue;
			}

            case OP_inclocal_i:
				core->increment_i(framep+readU30(pc), 1);
				continue;

            case OP_decrement:
				*sp = core->numberAtom(*sp);
				core->increment_d(sp, -1);
                continue;

            case OP_decrement_i:
				core->increment_i(sp, -1);
                continue;

			case OP_declocal:
			{
				Atom* rp = framep+readU30(pc);
				*rp = core->numberAtom(*rp);
				core->increment_d(rp, -1);
				continue;
			}

			case OP_declocal_i:
				core->increment_i(framep+readU30(pc), -1);
                continue;

            case OP_typeof:
				*sp = core->_typeof(*sp)->atom();
                continue;

            case OP_not:
                *sp = core->booleanAtom(*sp) ^ booleanNotMask;
                continue;

			case OP_bitnot:
				*sp = core->intToAtom(~core->integer(*sp));
                continue;

            case OP_setlocal:
                framep[readU30(pc)] = *(sp--);
                continue;
            case OP_setlocal0:
            case OP_setlocal1:
            case OP_setlocal2:
            case OP_setlocal3:
                framep[opcode-OP_setlocal0] = *(sp--);
                continue;

            case OP_add:
                sp[-1] = toplevel->add2(sp[-1], sp[0]);
                sp--;
                continue;

			case OP_add_i:
				sp[-1] = core->intToAtom(core->integer(sp[-1]) + core->integer(sp[0]));
                sp--;
                continue;

            case OP_subtract:
                sp[-1] = core->doubleToAtom(core->number(sp[-1]) - core->number(sp[0]));
                sp--;
                continue;

            case OP_subtract_i:
				sp[-1] = core->intToAtom(core->integer(sp[-1]) - core->integer(sp[0]));
                sp--;
                continue;

            case OP_multiply:
                sp[-1] = core->doubleToAtom(core->number(sp[-1]) * core->number(sp[0]));
                sp--;
                continue;

			case OP_multiply_i:
                sp[-1] = core->intToAtom(core->integer(sp[-1]) * core->integer(sp[0]));
                sp--;
                continue;

            case OP_divide:
				sp[-1] = core->doubleToAtom(core->number(sp[-1]) / core->number(sp[0]));
                sp--;
                continue;

            case OP_modulo:
				sp[-1] = core->doubleToAtom(MathUtils::mod(core->number(sp[-1]), core->number(sp[0])));
				sp--;
				continue;

            case OP_lshift:
				sp[-1] = core->intToAtom( core->integer(sp[-1]) << (core->toUInt32(sp[0])&0x1F) );
                sp--;
                continue;

            case OP_rshift:
				sp[-1] = core->intToAtom( core->integer(sp[-1]) >> (core->toUInt32(sp[0])&0x1F) );
                sp--;
                continue;

            case OP_urshift:
                sp[-1] = core->uintToAtom( core->toUInt32(sp[-1]) >> (core->toUInt32(sp[0])&0x1F) );
                sp--;
                continue;

            case OP_bitand:
				sp[-1] = core->intToAtom(core->integer(sp[-1]) & core->integer(sp[0]));
                sp--;
				continue;

            case OP_bitor:
                sp[-1] = core->intToAtom(core->integer(sp[-1]) | core->integer(sp[0]));
                sp--;
                continue;

            case OP_bitxor:
				sp[-1] = core->intToAtom(core->integer(sp[-1]) ^ core->integer(sp[0]));
                sp--;
                continue;

            case OP_equals:
				sp[-1] = core->eq(sp[-1], sp[0]);
                sp--;
                continue;

            case OP_strictequals:
                sp[-1] = core->stricteq(sp[-1], sp[0]);
                sp--;
                continue;
				
			case OP_lookupswitch: 
			{
				const byte* base = pc-1;
				// safe to assume int since verifier checks for int
				uint32 index = AvmCore::integer_u(*(sp--));
				const byte* switch_pc = pc+3;
				uint32 case_count = readU30(switch_pc) + 1;
                pc = base+readS24( index < case_count ? (switch_pc + 3*index) : pc );

				continue;
			}

            case OP_iftrue:
				if (core->booleanAtom(*(sp--)) & booleanNotMask)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += 3+j;
				}
				else
                    pc += 3;
                continue;

            case OP_iffalse:
				if (!(core->booleanAtom(*(sp--)) & booleanNotMask))
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += 3+j;
				}
				else
                    pc += 3;
				continue;

			case OP_ifeq:
				sp -= 2;
				if (core->eq(sp[1], sp[2]) == trueAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifne:
				sp -= 2;
                if (core->eq(sp[1], sp[2]) == falseAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifstricteq:
				sp -= 2;
                if (core->stricteq(sp[1], sp[2]) == trueAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

            case OP_ifstrictne:
				sp -= 2;
                if (core->stricteq(sp[1], sp[2]) == falseAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_iflt:
				sp -= 2;
                if (core->compare(sp[1], sp[2]) == trueAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifnlt:
				sp -= 2;
                if (core->compare(sp[1], sp[2]) != trueAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifle:
				sp -= 2;
                if (core->compare(sp[2], sp[1]) == falseAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifnle:
				sp -= 2;
                if (core->compare(sp[2], sp[1]) != falseAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifgt:
				sp -= 2;
                if (core->compare(sp[2], sp[1]) == trueAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifngt:
				sp -= 2;
                if (core->compare(sp[2], sp[1]) != trueAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;
			
			case OP_ifge:
				sp -= 2;
                if (core->compare(sp[1], sp[2]) == falseAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

			case OP_ifnge:
				sp -= 2;
                if (core->compare(sp[1], sp[2]) != falseAtom)
				{
					int j = readS24(pc);
					core->branchCheck(env, interruptable, j);
                    pc += j;
				}
				pc += 3;
                continue;

            case OP_lessthan:
                sp--;
				sp[0] = core->compare(sp[0],sp[1]) == trueAtom ? trueAtom : falseAtom;
                continue;

            case OP_lessequals:
				sp--;
				sp[0] = core->compare(sp[1],sp[0]) == falseAtom ? trueAtom : falseAtom;
                continue;

            case OP_greaterthan:
				sp--;
				sp[0] = core->compare(sp[1],sp[0]) == trueAtom ? trueAtom : falseAtom;
                continue;

            case OP_greaterequals:
				sp--;
				sp[0] = core->compare(sp[0],sp[1]) == falseAtom ? trueAtom : falseAtom;
                continue;

            case OP_newobject:
                argc = readU30(pc);
                tempAtom = env->op_newobject(sp, argc)->atom();
                *(sp -= 2*argc-1) = tempAtom;
                continue;

            case OP_newarray:
                argc = readU30(pc);
                tempAtom = toplevel->arrayClass->newarray(sp-argc+1, argc)->atom();
                *(sp -= argc-1) = tempAtom;
                continue;

			case OP_getlex:
			{
				// findpropstrict + getproperty
				// stack in:  -
				// stack out: value
				Multiname name;
				pool->parseMultiname(name, readU30(pc));

				// only non-runtime names are allowed.  but this still includes
				// wildcard and attribute names.
				Atom obj = env->findproperty(scope, scopeBase, scopeDepth, &name, true, withBase);
				*(++sp) = toplevel->getproperty(obj, &name, toplevel->toVTable(obj));
				continue;
			}	

			// get a property using a multiname ref
            case OP_getproperty:
			{
				Multiname multiname;
				pool->parseMultiname(multiname, readU30(pc));
				if (!multiname.isRuntime())
				{
					sp[0] = toplevel->getproperty(sp[0], &multiname, toplevel->toVTable(sp[0]));
				}
				else
				{
					if(multiname.isRtns() || !core->isDictionaryLookup(*sp, *(sp-1))) {
						sp = initMultiname(env, multiname, sp);
						*sp = toplevel->getproperty(*sp, &multiname, toplevel->toVTable(*sp));
					} else {
						Atom key = *(sp--);
						sp[0] = AvmCore::atomToScriptObject(sp[0])->getAtomProperty(key);
					}
				}

				continue;
			}

			// set a property using a multiname ref
			case OP_setproperty: 
			{
				Multiname multiname; 
				pool->parseMultiname(multiname, readU30(pc));
				Atom value = *(sp--);
				if (!multiname.isRuntime())
				{
					Atom obj = *(sp--);
					toplevel->setproperty(obj, &multiname, value, toplevel->toVTable(obj));
				}
				else
				{
					if(multiname.isRtns() || !core->isDictionaryLookup(*sp, *(sp-1))) {
						sp = initMultiname(env, multiname, sp);
						Atom obj = *(sp--);
						toplevel->setproperty(obj, &multiname, value, toplevel->toVTable(obj));
					} else {
						Atom key = *(sp--);
						Atom obj = *(sp--);
						AvmCore::atomToScriptObject(obj)->setAtomProperty(key, value);
					}
				}
                continue;
			}

			case OP_initproperty: 
			{
				Multiname multiname; 
				pool->parseMultiname(multiname, readU30(pc));
				Atom value = *(sp--);
				if (!multiname.isRuntime())
				{
					Atom obj = *(sp--);
					env->initproperty(obj, &multiname, value, toplevel->toVTable(obj));
				}
				else
				{
					sp = initMultiname(env, multiname, sp);
					Atom obj = *(sp--);
					env->initproperty(obj, &multiname, value, toplevel->toVTable(obj));
				}
                continue;
			}

			case OP_getdescendants:
			{
				Multiname name;
				pool->parseMultiname(name, readU30(pc));
				if (!name.isRuntime())
				{
					sp[0] = env->getdescendants(sp[0], &name);
				}
				else
				{
					sp = initMultiname(env, name, sp);
					sp[0] = env->getdescendants(sp[0], &name);
				}
				continue;
			}

			case OP_checkfilter:
			{
				env->checkfilter(sp[0]);
				continue;
			}

			// search the scope chain for a given property and return the object
			// that contains it.  the next instruction will usually be getpropname
			// or setpropname.
            case OP_findpropstrict:
            case OP_findproperty:
			{
				// stack in:  [ns [name]]
				// stack out: obj
				Multiname multiname;
				pool->parseMultiname(multiname, readU30(pc));
				if (!multiname.isRuntime())
				{
					*(++sp) = env->findproperty(scope, scopeBase, scopeDepth, &multiname, opcode == OP_findpropstrict, withBase);
				}
				else
				{
					sp = initMultiname(env, multiname, sp);
					*(++sp) = env->findproperty(scope, scopeBase, scopeDepth, &multiname, opcode == OP_findpropstrict, withBase);
				}
				continue;
			}

			case OP_finddef:
			{
				// stack in: 
				// stack out: obj
				Multiname multiname;
				pool->parseMultiname(multiname, readU30(pc));
				*(++sp) = env->finddef(&multiname)->atom();
				continue;
			}

			case OP_nextname:
				sp--;
				// verifier checks for int
				sp[0] = env->nextname(sp[0], AvmCore::integer_i(sp[1]));
				continue;

			case OP_nextvalue:
				sp--;
				// verifier checks for int
				sp[0] = env->nextvalue(sp[0], AvmCore::integer_i(sp[1]));
				continue;
				
			case OP_hasnext:
				sp--;
				// verifier checks for int
				sp[0] = core->intToAtom(env->hasnext(sp[0], AvmCore::integer_i(sp[1])));
				continue;

			case OP_hasnext2:
				{
					int objectReg = readU30(pc);
					int indexReg  = readU30(pc);
					Atom objAtom = framep[objectReg];
					int index = core->integer(framep[indexReg]);
					*(++sp) = env->hasnext2(objAtom, index) ? trueAtom : falseAtom;
					framep[objectReg] = objAtom;
					framep[indexReg] = core->intToAtom(index);
				}
				break;

			// delete property using multiname
			case OP_deleteproperty: 
			{
				Multiname multiname;
				pool->parseMultiname(multiname, readU30(pc));
				if (!multiname.isRuntime())
				{
					sp[0] = env->delproperty(sp[0], &multiname);
				}
				else
				{
					if(multiname.isRtns() || !core->isDictionaryLookup(*sp, *(sp-1))) {
						sp = initMultiname(env, multiname, sp, true);
						sp[0] = env->delproperty(sp[0], &multiname);
					} else {
						Atom key = *(sp--);
						bool res = AvmCore::atomToScriptObject(sp[0])->deleteAtomProperty(key);
						sp[0] = res ? trueAtom : falseAtom;
					}
				}
				continue;
			}

            case OP_setslot:
			{
				sp -= 2;
				env->nullcheck(sp[1]);
				int slot_id = readU30(pc)-1;
				ScriptObject* o = AvmCore::atomToScriptObject(sp[1]);
				o->setSlotAtom(slot_id,
					toplevel->coerce(sp[2], o->traits()->getSlotTraits(slot_id)));
				continue;
			}

			case OP_getslot:
				env->nullcheck(sp[0]);
				sp[0] = AvmCore::atomToScriptObject(sp[0])->getSlotAtom(readU30(pc)-1);
				continue;

			case OP_setglobalslot:
			{
				// find the global activation scope (object at depth 0 on scope chain)
				ScriptObject *global;
				if (outer_depth == 0)
				{
					global = AvmCore::atomToScriptObject(scopeBase[0]);
				}
				else
				{
					global = AvmCore::atomToScriptObject(scope->getScope(0));
				}

				int slot_id = readU30(pc)-1;
				sp--;
				global->setSlotAtom(slot_id, 
					toplevel->coerce(sp[1], global->traits()->getSlotTraits(slot_id)));
				continue;
			}

			case OP_getglobalslot:
			{
				// find the global activation scope (object at depth 0 on scope chain)
				ScriptObject *global;
				if (outer_depth == 0)
				{
					global = AvmCore::atomToScriptObject(scopeBase[0]);
				}
				else
				{
					global = AvmCore::atomToScriptObject(scope->getScope(0));
				}

				sp++;
				sp[0] = global->getSlotAtom(readU30(pc)-1);
				continue;
			}

			case OP_call: 
			{
                argc = readU30(pc);
                // stack in: function, receiver, arg1, ... argN
                // stack out: result
                tempAtom = toplevel->op_call(sp[-argc-1]/*function*/, argc, sp-argc);
                *(sp = sp-argc-1) = tempAtom;
                continue;
			}

			case OP_construct: 
			{
                argc = readU30(pc);
                // stack in: function, arg1, ..., argN
                // stack out: new instance
                tempAtom = toplevel->op_construct(sp[-argc]/*function*/, argc, sp-argc);				
                *(sp = sp-argc) = tempAtom;
                continue;
			}

            case OP_newfunction: 
			{
				sp++;
				AbstractFunction *body = pool->getMethodInfo(readU30(pc));
				sp[0] = env->newfunction(body, scope, scopeBase)->atom();
                continue;
            }

            case OP_newclass:
				{
					int class_index = readU30(pc);
					AbstractFunction *cinit = pool->cinits[class_index];
					ClassClosure* base = (ClassClosure*)(~7&toplevel->coerce(sp[0], CLASS_TYPE));
					sp[0] = env->newclass(cinit, base, scope, scopeBase)->atom();
				}
                continue;
				
            case OP_callstatic:
				{
					// stack in: receiver, arg1..N
					// stack out: result
					int method_id = readU30(pc);
					argc = readU30(pc);
					env->nullcheck(sp[-argc]);
					// ISSUE if arg types were checked in verifier, this coerces again.
					MethodEnv* f = env->vtable->abcEnv->methods[method_id];
					tempAtom = f->coerceEnter(argc, sp-argc);
					*(sp -= argc) = tempAtom;
				}
				continue;

            case OP_callmethod:
				{
					// stack in: receiver, arg1..N
					// stack out: result
					uint32 disp_id = readU30(pc)-1;
					argc = readU30(pc);
					// null check included in env->callmethod
					//tempAtom = env->callmethod(disp_id, argc, sp-argc);
					Atom* atomv = sp-argc;

					// must be a real class instance for this to be used.  primitives that have
					// methods will only have final bindings and no dispatch table.
					VTable* vtable = toplevel->toVTable(atomv[0]); // includes null check
					AvmAssert(disp_id < vtable->traits->methodCount);
					MethodEnv *f = vtable->methods[disp_id];
					// ISSUE if arg types were checked in verifier, this coerces again.
					tempAtom = f->coerceEnter(argc, atomv);

					*(sp -= argc) = tempAtom;
				}
				continue;

			case OP_callproperty:
			case OP_callpropvoid:
			case OP_callproplex:
				{
					// stack in: obj [ns [name]] arg1..N
					// stack out: result
					Multiname multiname;
					pool->parseMultiname(multiname, readU30(pc));
					argc = readU30(pc);
					if (!multiname.isRuntime())
					{
						// np check in toVTable
						Atom base = sp[-argc];
						if (opcode == OP_callproplex)
							sp[-argc] = nullObjectAtom;
						tempAtom = toplevel->callproperty(base, &multiname, argc, sp-argc, toplevel->toVTable(base));
						*(sp -= argc) = tempAtom;
					}
					else
					{
						Atom* atomv = sp-argc;
						sp = initMultiname(env, multiname, sp-argc);
						Atom base = *sp;
						atomv[0] = opcode == OP_callproplex ? nullObjectAtom : base;
						*sp = toplevel->callproperty(base, &multiname, argc, atomv, toplevel->toVTable(base));
					}
					if (opcode == OP_callpropvoid)
					{
						sp--;
					}
				}
				continue;

			case OP_constructprop:
				{
					// stack in: obj [ns [name]] arg1..N
					// stack out: result
					Multiname name;
					pool->parseMultiname(name, readU30(pc));
					argc = readU30(pc);
					if (!name.isRuntime())
					{
						// np check in toVTable
						tempAtom = toplevel->constructprop(&name, argc, sp-argc, toplevel->toVTable(sp[-argc]));
						*(sp -= argc) = tempAtom;
					}
					else
					{
						Atom* atomv = sp-argc;
						sp = initMultiname(env, name, sp-argc);
						atomv[0] = *sp;
						*sp = toplevel->constructprop(&name, argc, atomv, toplevel->toVTable(atomv[0]));
					}
				}
				continue;

			case OP_callsuper:
			case OP_callsupervoid:
				{
					// stack in: obj [ns [name]] arg1..N
					Multiname name;
					pool->parseMultiname(name, readU30(pc));
					argc = readU30(pc);

					if (!name.isRuntime())
					{
						env->nullcheck(sp[-argc]); // null check
						tempAtom = env->callsuper(&name, argc, sp-argc);
						*(sp -= argc) = tempAtom;
					}
					else
					{
						Atom* atomv = sp-argc;
						sp = initMultiname(env, name, sp-argc);
						atomv[0] = *sp;
						env->nullcheck(atomv[0]);
						*sp = env->callsuper(&name, argc, atomv);
					}
					if (opcode == OP_callsupervoid)
					{
						sp--;
					}
				}
				continue;

			case OP_getsuper:
			{
				Multiname name;
				pool->parseMultiname(name, readU30(pc));
				if (!name.isRuntime())
				{
					Atom objAtom = *sp;
					env->nullcheck(objAtom);//null check
					*sp = env->getsuper(objAtom, &name);
				}
				else
				{
					sp = initMultiname(env, name, sp);
					Atom objAtom = *sp;
					env->nullcheck(objAtom);//null check
					*sp = env->getsuper(objAtom, &name);
				}
				continue;
			}

			case OP_setsuper:
			{
				int index = readU30(pc);
				Multiname name;
				pool->parseMultiname(name, index);
				Atom valueAtom = *(sp--);
				if (!name.isRuntime())
				{
					Atom objAtom = *(sp--);
					env->nullcheck(objAtom);
					env->setsuper(objAtom, &name, valueAtom);
				}
				else
				{
					sp = initMultiname(env, name, sp);
					Atom objAtom = *(sp--);
					env->nullcheck(objAtom);
					env->setsuper(objAtom, &name, valueAtom);
				}
				continue;
			}

			// obj arg1 arg2
		    //           sp
			case OP_constructsuper:
			{
				// stack in:  obj arg1..N
				// stack out: 
				argc = readU30(pc);
				env->nullcheck(sp[-argc]);
				env->vtable->base->init->coerceEnter(argc, sp-argc);
				sp -= argc+1;
				continue;
			}
				
            case OP_pushshort:
                // this just pushes an integer since we dont have short atoms
                *(++sp) = ((signed short)readU30(pc))<<3|kIntegerType;
				continue;

			case OP_astype:
			{
				Multiname multiname;
				pool->parseMultiname(multiname, readU30(pc));
				sp[0] = env->astype(sp[0], pool->getTraits(&multiname, toplevel));
				break;
			}

			case OP_astypelate: 
			{
				sp--;
				sp[0] = env->astype(sp[0], env->toClassITraits(sp[1]));
                continue;
			}

            case OP_coerce:
			{
                // expects a CONSTANT_Multiname cpool index
				// this is the ES4 implicit coersion
				Multiname multiname;
				pool->parseMultiname(multiname, readU30(pc));
				sp[0] = toplevel->coerce(sp[0], pool->getTraits(&multiname, toplevel));
                continue;
			}

			case OP_coerce_a:
				// no-op since interpreter only uses atoms
				continue;

			case OP_coerce_o:
				if (sp[0] == undefinedAtom)
					sp[0] = nullObjectAtom;
				continue;

			case OP_coerce_s:
				sp[0] = AvmCore::isNullOrUndefined(sp[0]) ? nullStringAtom : core->string(sp[0])->atom();
				continue;

			case OP_istype: 
			{
                // expects a CONSTANT_Multiname cpool index
				// used when operator "is" RHS is a compile-time type constant
				Multiname multiname;
				pool->parseMultiname(multiname, readU30(pc));
				Traits* itraits = pool->getTraits(&multiname, toplevel);
				sp[0] = core->istypeAtom(sp[0], itraits);
                continue;
			}

			case OP_istypelate: 
			{
				sp--;
				sp[0] = core->istypeAtom(sp[0], env->toClassITraits(sp[1]));
                continue;
			}

            case OP_pushbyte:
				sp++;
                sp[0] = ((sint8)*pc++)<<3|kIntegerType;
                continue;

            case OP_getscopeobject:
			{
				int scope_index = *pc++;
				sp++;
				sp[0] = scopeBase[scope_index];
				continue;
			}

            case OP_getglobalscope:
			{
				sp++;
				if (outer_depth > 0)
				{
					sp[0] = scope->getScope(0);
				}
				else
				{
					sp[0] = scopeBase[0];
				}
				continue;
			}

            case OP_pushscope:
			{
				sp--;
				Atom s = sp[1];
				env->nullcheck(s);
				scopeBase[scopeDepth++] = s;
				continue;
			}
			
            case OP_pushwith:
			{
				sp--;
				Atom s = sp[1];
				env->nullcheck(s);
				if (!withBase)
				{
					withBase = scopeBase+scopeDepth;
				}
				scopeBase[scopeDepth++] = s;
				continue;
			}

            case OP_newactivation:
			{
				sp++;
				sp[0] = core->newActivation(env->getActivation(), NULL)->atom();
				continue;
			}

            case OP_newcatch:
			{
				int catch_index = readU30(pc);
				Traits *t = info->exceptions->exceptions[catch_index].scopeTraits;
				sp++;
				sp[0] = env->newcatch(t)->atom();
				continue;
			}

            case OP_popscope:
				scopeDepth--;
				if (withBase >= scopeBase+scopeDepth)
				{
					withBase = NULL;
				}
				continue;

			case OP_coerce_i:
            case OP_convert_i:
                sp[0] = core->intAtom(sp[0]);
                continue;

			case OP_coerce_u:
			case OP_convert_u:
                sp[0] = core->uintAtom(sp[0]);
                continue;

			case OP_throw:
				core->throwAtom(*sp--);
				continue;
				
            case OP_instanceof:
				sp--;
				sp[0] = toplevel->instanceof(sp[0], sp[1]);
				continue;

            case OP_in:
				sp--;
				sp[0] = env->in(sp[0], sp[1]);
				continue;

			case OP_dxns:
				dxns = core->newPublicNamespace(cpool_string[readU30(pc)]);
				continue;

			case OP_dxnslate:
				dxns = core->newPublicNamespace(core->intern(*sp));
				sp--;
				break;

			case OP_abs_jump:
			{
				if (interruptable && core->interrupted)
					env->interrupt();
				#ifdef AVMPLUS_64BIT
				const byte *target = (const byte *) (AvmCore::readU30(pc) | (uintptr(AvmCore::readU30(pc)) << 32));
				#else
				const byte *target = (const byte *) AvmCore::readU30(pc);
				#endif
				code_start = pc = (const byte*) target;
				break;
			}
			default:
				AvmAssert(false);
            }
			
			if(info->setsDxns()) {
				core->dxnsAddr = dxnsAddrSave;
			}

		}

		
		}

		CATCH (Exception *exception)
		{
			if(info->setsDxns()) {
				core->dxnsAddr = dxnsAddrSave;
			}
			// find handler; rethrow if no handler.
			ExceptionHandler *handler = core->findExceptionHandler(info, expc, exception);
			// handler found in current method
			pc = code_start + handler->target;
			scopeDepth = initialScopeDepth; // ISSUE with() { try {} }
			sp = scopeBase + max_scope - 1;
			*(++sp) = exception->atom;
			goto MainLoop;
		}
		END_CATCH
		END_TRY 

		//
		// we never get here. verifier doesn't allow code to fall off end.
		//
    }

	Atom* Interpreter::initMultiname(MethodEnv* env, Multiname &name, Atom* sp, bool isDelete/*=false*/)
	{
		if (name.isRtname())
		{
			Atom index = *(sp--);
			AvmCore* core = env->core();

			if (isDelete)
			{
				if (core->isXMLList(index))
				{
					// Error according to E4X spec, section 11.3.1
					env->toplevel()->throwTypeError(kDeleteTypeError, core->toErrorString(env->toplevel()->toTraits(index)));
				}
			}
			
			// is it a qname?
			if (AvmCore::isObject(index))
			{
				ScriptObject* i = AvmCore::atomToScriptObject(index);
				if (i->traits() == core->traits.qName_itraits)
				{
					QNameObject* qname = (QNameObject*) i;
					bool attr = name.isAttr();
					qname->getMultiname(name);
					if (attr)
						name.setAttr(attr);

					// Discard runtime namespace if present
					if (name.isRtns())
						sp--;

					return sp;
				}
			}
					
			name.setName(core->intern(index));
		}

		if (name.isRtns())
			name.setNamespace(env->internRtns(*(sp--)));

		return sp;
	}

#ifdef AVMPLUS_VERBOSE
	    /**
     * display contents of current stack frame only.
     */
	void Interpreter::showState(MethodInfo* info, AbcOpcode opcode, int off,
							Atom* framep, int sp, int scopep, int scopeBase, int stackBase,
							const byte *code_start)
    {
		PoolObject* pool = info->pool;
		AvmCore* core = pool->core;
		const byte* pc = code_start + off;

		// stack
		core->console << "                        stack:";
		for (int i=stackBase; i <= sp; i++) {
			core->console << " " << core->format(framep[i]);
		}
		core->console << '\n';

        // scope chain
		core->console << "                        scope: ";
		for (int i=scopeBase; i <= scopep; i++) {
            core->console << core->format(framep[i]) << " ";
        }
		core->console << '\n';

        // locals
		core->console << "                         locals: ";
		for (int i=0; i < scopeBase; i++) {
            core->console << core->format(framep[i]) << " ";
        }
		core->console << '\n';

		// opcode
		core->console << "  ";
#ifdef DEBUGGER
		if (core->debugger && core->callStack && core->callStack->filename)
		{
			core->console << '[' << core->callStack->filename << ':' << (uint32)core->callStack->linenum << "] ";
		}
#endif
		core->console << off << ':';
		core->formatOpcode(core->console, pc, opcode, off, pool);
		core->console << '\n';
    }
#endif
}
#endif /* AVMPLUS_INTERP */
