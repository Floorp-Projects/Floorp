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
 * Portions created by the Initial Developer are Copyright (C) 1993-2006
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

#ifdef DEBUGGER

namespace avmplus
{
	using namespace MMgc;

	Debugger::TraceLevel Debugger::astrace = Debugger::TRACE_OFF;
	uint64 Debugger::astraceStartTime = OSDep::currentTimeMillis();

	Debugger::Debugger(AvmCore *core)
		: core(core)
		, abcList(core->GetGC())
		, pool2abcIndex()
	{
	}

	void Debugger::stepInto()
	{
		stepState.flag = true;
		stepState.depth = -1;

		// Check that core and core->callStack are non-null before dereferencing
		// them, so that it's possible to call stepInto() even before there is
		// a stackframe (so execution will stop at the first executed line of code).
		stepState.startingDepth = (core && core->callStack) ? core->callStack->depth : 0;
	}

	void Debugger::stepOver()
	{
		stepState.flag = true;
		stepState.depth = core->callStack->depth;
		stepState.startingDepth = core->callStack->depth;
	}

	void Debugger::stepOut()
	{
		stepState.flag = true;
		stepState.depth = core->callStack->depth - 1;
		stepState.startingDepth = core->callStack->depth;
	}

	void Debugger::stepContinue()
	{
		// Restore the previous stepping state
		stepState = oldStepState;
	}

	/**
	 * Set a breakpoint in a particular source flie
 	 * NOTE: if the 'same' source file appears in 
	 * multiple abc files, it is up to the caller
	 * to ensure that this call is performed for each 
	 * SourceInfo object.
	 */
	bool Debugger::breakpointSet(SourceInfo* source, int linenum)
	{
		return source->setBreakpoint(linenum);
	}

	/**
	 * Clear a breakpoint on a particular source file
	 */
	bool Debugger::breakpointClear(SourceInfo* source, int linenum)
	{
		return source->clearBreakpoint(linenum);
	}

	void Debugger::debugLine(int linenum)
	{
		AvmAssert( core->callStack !=0 );
		if (!core->callStack)
			return;

		AvmAssert(linenum > 0);

		int prev = core->callStack->linenum;
		core->callStack->linenum = linenum;

		int line = linenum;

		// line number has changed
		bool changed = (prev == line) ? false : true;
		bool exited =  (prev == -1) ? true : false; // are we being called as a result of function exit?
		if (!changed && !exited) 
			return;  // still on the same line in the same function?

		if (core->profiler->profilingDataWanted && core->profiler->profileSwitch)
		{
			core->profiler->sendLineTimestamp(line);
		}

		// tracing information
		traceLine(line);

		// check if we should stop due to breakpoint or step
		bool stop = false;
		if (stepState.flag)
		{
			if (stepState.startingDepth != -1 && core->callStack->depth < stepState.startingDepth)
			{
				// We stepped out of whatever function was executing when the
				// stepInto/stepOver/stepOut command was executed.  We may be
				// in the middle of a line of code, but we still want to stop
				// immediately.  See bug 126633.
				stop = true;
			}
			else if (!exited && (stepState.depth == -1 || core->callStack->depth <= stepState.depth) ) 
			{
				// We reached the beginning of a new line of code.
				stop = true;
			}
		}

		// we didn't decide to stop due to a step, but check if we hit a breakpoint
		if (!stop && !exited)
		{
			AbstractFunction* f = core->callStack->info;
			if ( (f->flags & AbstractFunction::ABSTRACT_METHOD) == 0) 
			{
				MethodInfo* m = (MethodInfo*)f;
				AbcFile* abc = m->getFile();
				if (abc)
				{
					SourceFile* source = abc->sourceNamed( core->callStack->filename );
					if (source && source->hasBreakpoint(line))
					{
						stop = true;
					}
				}
			}
		}

		// we still haven't decided to stop; check our watchpoints
		if (!stop && !exited)
		{
			if (hitWatchpoint())
				stop = true;
		}

		if (stop)
		{
			// Terminate whatever step operation may have been happening.  But first,
			// save the state of the step, so that if someone calls stepContinue(),
			// then we can restore it.
			StepState oldOldStepState = oldStepState; // save oldStepState in case of reentrancy
			oldStepState = stepState; // save stepState so that stepContinue() can find it
			stepState.clear(); // turn off stepping

			enterDebugger();

			oldStepState = oldOldStepState; // restore oldStepState
		}
	}

	void Debugger::debugFile(Stringp filename)
	{
		AvmAssert( core->callStack != 0 );
		if (!core->callStack)
			return;

		AvmAssert(filename != 0);

		Stringp prev = core->callStack->filename;
		core->callStack->filename = filename;

		// filename changed
		if (prev != filename) 
		{
			if (core->profiler->profilingDataWanted)
			{
				UTF8String* sourceFile = filename->toUTF8String();
				core->profiler->sendDebugFileUrl(sourceFile);
			}
		}
	}

	void Debugger::debugMethod(MethodEnv* /*env*/)
	{
		// nop
	}

	void Debugger::_debugMethod(MethodEnv* env)
	{
		traceMethod(env->method);

		// can't debug native methods
		if ( !(env->method->flags & MethodInfo::NATIVE) )
			debugMethod(env);
	}

	void Debugger::traceMethod(AbstractFunction* fnc, bool ignoreArgs)
	{
		if (astrace > TRACE_OFF)
		{
			if (fnc)
			{
				// disable trace temporarily so associated allocs don't appear
				TraceLevel lvl = astrace;
				astrace = TRACE_OFF;

				// WARNING: don't change the format of output since outside utils depend on it
				uint64 delta = OSDep::currentTimeMillis() - astraceStartTime;
				core->console << (uint32)(delta) << " AVMINF: MTHD ";
				if (fnc->name && (fnc->name->length() > 0) )
					core->console << fnc->name;
				else
					core->console << "<unknown>";
				
				core->console << " (";
				if (!ignoreArgs && core->callStack && (lvl == TRACE_METHODS_WITH_ARGS || lvl == TRACE_METHODS_AND_LINES_WITH_ARGS))
				{
					DebugStackFrame* frame = (DebugStackFrame*)frameAt(0);
					int count;
					Atom* arr;
					if (frame && frame->arguments(arr, count))
					{
						for(int i=0; i<count; i++)
						{
							core->console << core->format (arr[i]);
							if (i+1 < count)
								core->console << ", ";
						}
					}
				}
				core->console << ")";
				if (!fnc->isFlagSet(AbstractFunction::SUGGEST_INTERP))
				{
					core->console << " @ 0x";			
					core->console.writeHexAddr( (uintptr)fnc->impl32);
				}
				core->console << "\n";		
				astrace = lvl;
			}
		}
	}

	void Debugger::traceLine(int line)
	{
		if (astrace >= TRACE_METHODS_AND_LINES)
		{
			Stringp file = core->callStack->filename;

			TraceLevel lvl = astrace;
			astrace = TRACE_OFF;

			// WARNING: don't change the format of output since outside utils depend on it
			uint64 delta = OSDep::currentTimeMillis() - astraceStartTime;
			core->console << (uint32)(delta) << " AVMINF: LINE ";
			if (file)
				core->console << "   " << line << "\t\t " << file << "\n";
			else
				core->console << "   " << line << "\t\t ??? \n"; 			
			astrace = lvl;
		}
	}

	/**
	 * Called when an abc file is first decoded.
	 * This method builds a list of source files
	 * and methods, etc from the given abc file.
	 */
	void Debugger::processAbc(PoolObject* pool, ScriptBuffer code)
	{
		// first off we build an AbcInfo object 
		AbcFile* abc = new (core->GetGC()) AbcFile(core, code.getSize());
		
		// now let's scan the abc resources pulling out what we need
		scanResources(abc, pool);

		// build a bridging table from pools to abcs
		int index = abcList.size();
		pool2abcIndex.add(pool, (const void*)index);

		// at this point our abc object has been populated with
		// source file objects and should ready to go. 
		// so we add it to the list and we are done
		abcList.add(abc);
	}

	/**
	 * Scans the pool object and pulls out information about the abc file
	 * placing it in the AbcFile
	 */
	void Debugger::scanResources(AbcFile* file, PoolObject* pool)
	{
		// walk all methods 
		int mCount = pool->methodCount;
		for(int i=0; i<mCount; i++)
		{
			AbstractFunction* f = pool->methods[i];
			if (f->hasMethodBody())
			{
				// yes there is code for this method
				MethodInfo* m = (MethodInfo*)f;
				if (m->body_pos)
				{
					// if body_pos is null we havent got the body yet or
					// this is an interface method
					scanCode(file, pool, m);
				}
			}
		}
	}

	/**
	 * Scans the bytecode and adds source level information to 
	 * the abc info object
	 */
	bool Debugger::scanCode(AbcFile* file, PoolObject* pool, MethodInfo* m)
	{
		const byte *abc_start = &m->pool->code()[0];

		const byte *pos = m->body_pos;

		m->setFile(file);

		AvmCore::readU30(pos); // max_stack
		AvmCore::readU30(pos); // local_count;
		AvmCore::readU30(pos); // init_stack_depth;
		AvmCore::readU30(pos); // max_stack_depth;
		int code_len = AvmCore::readU30(pos);

		const byte *start = pos;
		const byte *end = pos + code_len;

		int size = 0;
		int op_count;
		SourceFile* active = NULL; // current source file
        for (const byte* pc=start; pc < end; pc += size)
        {
            op_count = opOperandCount[*pc];
			if (op_count == -1 && *pc != OP_lookupswitch)
				return false; // ohh very bad, verifier will catch this

			size = AvmCore::calculateInstructionWidth(pc);

			if (pc+size > end)
				return false; // also bad, let the verifier will handle it

            switch (*pc)
            {
				case OP_lookupswitch:
				{
					// variable length instruction
					const byte *pc2 = pc+4;
					int case_count = 1 + readU30(pc2);
                    size += case_count*3;
					break;
				}

				case OP_debug:
				{
					// form is 8bit type followed by pool entry
					// then 4Byte extra info
					int type = (uint8)*(pc+1);

					switch(type)
					{
						case DI_LOCAL:
						{
							// in this case last word contains
							// register and line number 
							const byte* pc2 = pc+2;
							int index = readU30(pc2);
							int slot = (uint8)*(pc2);
							//int line = readS24(pc+5);

							
							//Atom str = pool->cpool[index];
							Stringp s = pool->getString(index);
							m->setRegName(slot, s);
						}
					}

					break;
				}

				case OP_debugline:
				{
					// this means that we have a new source line for the given offset
					const byte* pc2 = pc+1;
					int line = readU30(pc2);
					if (active == NULL)
						AvmAssert(0 == 1); // means OP_debugline appeared before OP_debugfile which is WRONG!  Fix compiler
					else
						active->addLine(core, line, m, pc - abc_start);
 					break;
				}

				case OP_debugfile:
				{
					// new or existing source file
					const byte* pc2 = pc+1;
					Stringp name = pool->getString(readU30(pc2));
					active = file->sourceNamed(name);
					if (active == NULL)
					{
						active = new (core->GetGC()) SourceFile(core->GetGC(), name);
						file->sourceAdd(active);
					}
					break;
				}	
			}
		}
		return true;
	}

	/**
	 * Returns the call stack depth (i.e. number of frames).
	 */
	int Debugger::frameCount()
	{
		const int MAX_FRAMES = 500; // we need a max, for perf. reasons (bug 175526)
		CallStackNode* trace = core->callStack;
		int count = 1;
		while( (trace = trace->next) != 0 && count < MAX_FRAMES )
			count++;
		return count;
	}

	/**
	 * Set frame to point to the specified frame number
 	 * or null if frame number does not exist. 
	 */
	DebugFrame* Debugger::frameAt(int frameNbr)
	{
		DebugFrame* frame = NULL;

		if (frameNbr >= 0)
		{
			CallStackNode* trace = locateTrace(frameNbr);
			if (trace)
				frame = new (core->GetGC()) DebugStackFrame(frameNbr, trace, this);
		}
		return frame;
	}

	/**
	 * Stack frames are labelled 0 to stackTrace->depth
	 * With zero being the topmost or most recent frame.
	 */
	CallStackNode* Debugger::locateTrace(int frameNbr)
	{
		int count = 0;
		CallStackNode* trace = core->callStack;
		while(count++ < frameNbr && trace != NULL)
			trace = trace->next;

		return trace;
	}

	/**
	 * # of abc files available
	 */
	int Debugger::abcCount() const
	{
		return abcList.size();
	}

	/**
	 * Get information on each of the abc files that
	 * have been loaded. 
	 */
	AbcInfo* Debugger::abcAt(int index) const
	{
		return abcList.get(index);
	}

	/**
	 * Contains all known debug information regarding a single 
	 * abc/swf file
	 */
	AbcFile::AbcFile(AvmCore* core, int size)
		: core(core),
		  source(core->GetGC()),
		  byteCount(size)
	{
		sourcemap = new (core->GetGC()) Hashtable(core->GetGC());
	}

	int AbcFile::sourceCount() const
	{
		return source.size(); 
	}

	SourceInfo* AbcFile::sourceAt(int index) const
	{
		return source.get(index);
	}

	int AbcFile::size() const 
	{ 
		return byteCount; 
	}

	/**
	 * Find a source file with the given name
	 */
	SourceFile* AbcFile::sourceNamed(Stringp name)
	{
		Atom atom = sourcemap->get(name->atom());
		if (AvmCore::isUndefined(atom))
			return NULL;
		uint32 index = AvmCore::integer_u(atom);
		return source.get(index);
	}

	/**
	 * Add source file to list; no check for uniqueness
	 * of name
	 */
	void AbcFile::sourceAdd(SourceFile* s)
	{
		uint32 index = source.add(s);
		sourcemap->add(s->name()->atom(), core->uintToAtom(index));
	}

	/**
	 * new source info
	 */
	SourceFile::SourceFile(MMgc::GC* gc, Stringp name)
		: named(name)
		, functions(gc)
	{
	}

	Stringp SourceFile::name() const 
	{ 
		return named; 
	}

	/**
	 * A line - offset pair should be recorded 
	 */
	void SourceFile::addLine(AvmCore* core, int linenum, MethodInfo* func, int offset)
	{
		// Add the function to our list if it doesn't exist.  Use lastIndexOf() instead of
		// indexOf(), because this will be faster in the very common case where the function
		// already exists at the end of the list.
		int index = functions.lastIndexOf(func);
		if (index < 0)
			index = functions.add(func);

		// line numbers for a given function don't always come in sequential
		// order -- for example, I've seen them come out of order if a function
		// contains an inner anonymous function -- so, compare the new line
		// against firstSourceLine every time we get called
		if (func->firstSourceLine == 0 || linenum < func->firstSourceLine)
			func->firstSourceLine = linenum;

		if (func->offsetInAbc == 0 || offset < func->offsetInAbc)
			func->offsetInAbc = offset;

		if (func->lastSourceLine == 0 || linenum > func->lastSourceLine)
			func->lastSourceLine = linenum;

		if (sourceLines == NULL)
			sourceLines = new (core->GetGC()) BitSet();
		sourceLines->set(linenum);
	}

	int SourceFile::functionCount() const 
	{ 
		return functions.size(); 
	}

	MethodInfo* SourceFile::functionAt(int index) const 
	{
		return functions.get(index); 
	}

	bool SourceFile::setBreakpoint(int linenum)
	{
		if (sourceLines == NULL || !sourceLines->get(linenum))
			return false;

		if (breakpoints == NULL)
			breakpoints = new (GC::GetGC(this)) BitSet();
		breakpoints->set(linenum);
		return true;
	}

	bool SourceFile::clearBreakpoint(int linenum)
	{
		if (breakpoints == NULL || !breakpoints->get(linenum))
			return false;
		breakpoints->clear(linenum);
		return true;
	}

	bool SourceFile::hasBreakpoint(int linenum)
	{
		return (breakpoints != NULL && breakpoints->get(linenum));
	}

	DebugStackFrame::DebugStackFrame(int nbr, CallStackNode* tr, Debugger* debug)
		: trace(tr)
		, debugger(debug)
		, frameNbr(nbr)
	{
	}

	/**
	 * Identifies the source file and source line number
	 * corresponding to this frame 
	 */
	bool DebugStackFrame::sourceLocation(SourceInfo*& source, int& linenum)
	{
		// use the method info to locate the abcfile / source 
		AbstractFunction* m = trace->info;
		if (trace->filename)
		{
			uintptr index = (uintptr)debugger->pool2abcIndex.get(Atom((PoolObject*)m->pool));

			AbcFile* abc = (AbcFile*)debugger->abcAt(index);
			source = abc->sourceNamed(trace->filename);
		}
		linenum = trace->linenum;

		// valid info?
		return (source != NULL && linenum > 0);
	}

	/**
 	 * This pointer for the frame
	 */
	bool DebugStackFrame::dhis(Atom& a)
	{
		bool worked = false;
		if (trace->framep)
		{
			MethodInfo* info = (MethodInfo*)trace->info;
			info->boxLocals(trace->framep, 0, trace->traits, &a, 0, 1); // pull framep[0] = [this] 
			worked = true;
		}
		else
		{
			a = undefinedAtom;
		}
		return worked;
	}

	/**
 	 * @return a pointer to an object Atom whose members are
 	 * the arguments passed into a function for this frame
 	 */
	bool DebugStackFrame::arguments(Atom*& ar, int& count)
	{
		bool worked = true;
		if (trace->framep)
		{
			int firstArgument, pastLastArgument;
			argumentBounds(&firstArgument, &pastLastArgument);
			count = pastLastArgument - firstArgument;
			if (count > 0)
			{
				// pull the args into an array -- skip [0] which is [this]
				ar = (Atom*) debugger->core->GetGC()->Calloc(count, sizeof(Atom), GC::kContainsPointers|GC::kZero);
				MethodInfo* info = (MethodInfo*)trace->info;
				info->boxLocals(trace->framep, firstArgument, trace->traits, ar, 0, count);
			}
		}
		else
		{
			worked = false;
			count = 0;
		}
		return worked;
	}

	bool DebugStackFrame::setArgument(int which, Atom& val)
	{
		bool worked = false;
		if (trace->framep)
		{
			int firstArgument, pastLastArgument;
			argumentBounds(&firstArgument, &pastLastArgument);
			int count = pastLastArgument - firstArgument;
			if (count > 0 && which < count)
			{
				// copy the single arg over
				MethodInfo* info = (MethodInfo*)trace->info;
				info->unboxLocals(&val, 0, trace->traits, trace->framep, firstArgument+which, 1);
				worked = true;
			}
		}
		return worked;
	}

	/**
	 * @return a pointer to an object Atom whose members are
	 * the active locals for this frame.
	 */
	bool DebugStackFrame::locals(Atom*& ar, int& count)
	{
		bool worked = true;
		if (trace->framep)
		{
			int firstLocal, pastLastLocal;
			localBounds(&firstLocal, &pastLastLocal);
			count = pastLastLocal - firstLocal;
			AvmAssert(count >= 0);
			if (count > 0)
			{
				// frame looks like [this][param0...paramN][local0...localN]
				ar = (Atom*) debugger->core->GetGC()->Calloc(count, sizeof(Atom), GC::kContainsPointers|GC::kZero);
				MethodInfo* info = (MethodInfo*)trace->info;
				info->boxLocals(trace->framep, firstLocal, trace->traits, ar, 0, count);

				// If NEED_REST or NEED_ARGUMENTS is set, and the MIR is being used, then the first
				// local is actually not an atom at all -- it is an ArrayObject*.  So, we need to
				// convert it to an atom.  (If the interpreter is being used instead of the MIR, then
				// it is stored as an atom.)
				if (info->flags & (AbstractFunction::NEED_REST | AbstractFunction::NEED_ARGUMENTS))
				{
					int atomType = ar[0] & 7;
					if (atomType == 0) // 0 is not a legal atom type, so ar[0] is not an atom
					{
						ScriptObject* obj = (ScriptObject*)ar[0];
						ar[0] = obj->atom();
					}
				}
			}
		}
		else
		{
			worked = false;
			count = 0;
		}
		return worked;
	}

	bool DebugStackFrame::setLocal(int which, Atom& val)
	{
		bool worked = false;
		if (trace->framep)
		{
			int firstLocal, pastLastLocal;
			localBounds(&firstLocal, &pastLastLocal);
			int count = pastLastLocal - firstLocal;
			if (count > 0 && which < count)
			{
				MethodInfo* info = (MethodInfo*)trace->info;
				if (which == 0 && (info->flags & (AbstractFunction::NEED_REST | AbstractFunction::NEED_ARGUMENTS)))
				{
					// They are trying to modify the first local, but that is actually the special
					// array for "...rest" or for "arguments".  That is too complicated to allow
					// right now.  We're just going to fail the request.
				}
				else
				{
					// copy the single arg over
					info->unboxLocals(&val, 0, trace->traits, trace->framep, firstLocal+which, 1);
					worked = true;
				}
			}
		}
		return worked;
	}

	// Returns the indices of the arguments: [firstArgument, pastLastArgument)
	void DebugStackFrame::argumentBounds(int* firstArgument, int* pastLastArgument)
	{
		*firstArgument = 1; // because [0] is 'this'
		*pastLastArgument = indexOfFirstLocal();
	}

	// Returns the indices of the locals: [firstLocal, pastLastLocal)
	void DebugStackFrame::localBounds(int* firstLocal, int* pastLastLocal)
	{
		*firstLocal = indexOfFirstLocal();
		if (trace->framep)
		{
			MethodInfo* info = (MethodInfo*) trace->info;
			*pastLastLocal = info->local_count;
		}
		else
		{
			*pastLastLocal = *firstLocal;
		}
	}

	int DebugStackFrame::indexOfFirstLocal()
	{
		// 'trace->argc' is the number of arguments that were actually passed in
		// to this function, but that is not what we want -- we want
		// 'info->param_count', because that is the number of arguments we were
		// *expecting* to get.  There are two reasons we want 'info->param_count':
		//
		// (1) if the caller passed in too many args, we want to ignore the
		//     trailing ones; and
		// (2) if the caller passed in too few args to a function that has some
		//     default parameters, we want to display the args with their default
		//     values.
		return 1 + trace->info->param_count;
	}

}

#endif /* DEBUGGER */
