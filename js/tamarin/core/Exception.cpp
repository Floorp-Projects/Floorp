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


#include "avmplus.h"

namespace avmplus
{
	//
	// Exception
	//

	Exception::Exception(Atom atom
#ifdef DEBUGGER
			, AvmCore* core
#endif /* DEBUGGER */
		)
	{
		this->atom = atom;
		this->flags = 0;

        #ifdef DEBUGGER
		// If the exception atom is an Error object, copy its stack trace.
		// Otherwise, generate a new stack trace.
		if (core->istype(atom, core->traits.error_itraits))
		{
			stackTrace = ((ErrorObject*)AvmCore::atomToScriptObject(atom))->getStackTrace();
		}
		else
		{
			stackTrace = core->newStackTrace();
		}
		#endif
	}
	
	bool Exception::isValid()
	{
		return (atom&7)==kObjectType;
	}
	
	//
	// ExceptionHandlerTable
	//
	
	ExceptionHandlerTable::ExceptionHandlerTable(int exception_count)
	{
		this->exception_count = exception_count;
		memset(exceptions, 0, sizeof(ExceptionHandler)*exception_count);
	}
	
	//
	// ExceptionFrame
	//
	void ExceptionFrame::beginTry(AvmCore* core)
	{
		this->core = core;

		prevFrame = core->exceptionFrame;

		if (!prevFrame) {
			// Do special setup for first frame
			core->setStackBase();
		}

		core->exceptionFrame = this;

#ifdef DEBUGGER
		callStack = core->callStack;

		// beginTry() is called from both the TRY macro and from JIT'd code.  The TRY
		// macro will immediately change the value of catchAction right after the
		// call to beginTry(); but the JIT'd code does not change catchAction.  So,
		// we initialize catchAction to the value that it needs when we're called
		// from JIT'd code, that is, kCatchAction_SearchForActionScriptExceptionHandler.
		catchAction = kCatchAction_SearchForActionScriptExceptionHandler;
#endif /* DEBUGGER */

		codeContextAtom = core->codeContextAtom;
		dxnsAddr = core->dxnsAddr;
	}

	void ExceptionFrame::endTry()
	{
		if (core) {
			// ISSUE do we need to check core if it is set in constructor?
			core->exceptionFrame = prevFrame;

			// Restore the code context to what it was before the try
			core->codeContextAtom = codeContextAtom;
		}
	}
	
	void ExceptionFrame::throwException(Exception *exception)
	{
		longjmp(jmpbuf, (intptr)exception);
	}

	void ExceptionFrame::beginCatch()
	{
		core->exceptionFrame = prevFrame;

#ifdef DEBUGGER
		//AvmAssert(callStack && callStack->env);
		if (core->profiler->profilingDataWanted && callStack && callStack->env)
			core->profiler->sendCatch(callStack->env->method);

		core->callStack = callStack;
#endif // DEBUGGER

		core->codeContextAtom = codeContextAtom;
		core->dxnsAddr = dxnsAddr;
	}
}
