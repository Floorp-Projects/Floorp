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

#ifndef __avmplus_Exception__
#define __avmplus_Exception__


namespace avmplus
{
	/**
	 * The Exception class is used to throw all exceptions in
	 * AVM+.  To throw an exception, an Exception object is
	 * instantiated and passed to AvmCore::throwException.
	 */
	class Exception : public MMgc::GCObject
	{
	public:
		Exception(Atom atom
#ifdef DEBUGGER
			, AvmCore* core
#endif /* DEBUGGER */
		);

		ATOM_WB atom;
		int flags;

		bool isValid();
		enum
		{
			/**
			 * An EXIT_EXCEPTION cannot be caught.  It indicates that
			 * the VM is shutting down.
			 */
			EXIT_EXCEPTION = 1

#ifdef DEBUGGER
			/**
			 * Indicates that this exception has already been passed to
			 * the debugger.
			 */
			, SEEN_BY_DEBUGGER = 2
#endif
		};

#ifdef DEBUGGER
		StackTrace* getStackTrace() const { return stackTrace; }
 		DWB(StackTrace*) stackTrace;
#endif /* DEBUGGER */
	};

	/**
	 * ExceptionHandler is a single entry in the exceptions
	 * table that accompanies a MethodInfo.  It describes:
	 *
	 * - the range of instructions that the exception handler
	 *   applies to
	 * - the location of the first instruction in the
	 *   exception handler.
	 * - the type of exceptions handled by this exception
	 *   handler
	 */
	class ExceptionHandler
	{
	public:
		/** Start of code range the exception applies to.  Inclusive. */
		int from;

		/** End of code range the exception applies to.  Exclusive. */
		int to;

		/** The target location to branch to when the exception occurs. */
		union {
			int target;
			CodegenMIR::OP* targetIns;
		};

		/** The type of exceptions handled by this exception handler. */
		Traits* traits;

		/** The exception scope traits. */
		Traits* scopeTraits;
	};

	/**
	 * ExceptionHandlerTable is a table of ExceptionHandler objects.
	 * The list of exception handlers in a MethodInfo entry in an
	 * ABC file is parsed into an ExceptionHandlerTable object.
	 */
	class ExceptionHandlerTable : public MMgc::GCObject
	{
	public:
		ExceptionHandlerTable(int exception_count);

		int exception_count;
		ExceptionHandler exceptions[1];
	};

#ifdef DEBUGGER
	/**
	 * CatchAction indicates what the CATCH block for a given ExceptionFrame will
	 * do if it gets invoked (that is, if an exception is raised from inside the
	 * TRY block).
	 *
	 * This information is needed by the debugger, in order to decide whether to
	 * suspend execution in the debugger, in order to let the user examine
	 * variables etc., before executing the CATCH block.
	 */
	enum CatchAction
	{
		// It is not known what the CATCH block will do.  This should almost never be used.
		kCatchAction_Unknown,

		// The CATCH block will silently consume any exception that occurs, and will not
		// treat it as an error, so exceptions should not be reported to the debugger.
		kCatchAction_Ignore,

		// The CATCH block will treat any exception that occurs as an error -- probably by
		// calling uncaughtException, but possibly by some other means.  So, exceptions
		// should be reported to the debugger.
		kCatchAction_ReportAsError,

		// The CATCH block will rethrow any exception that occurs; so, we will 'continue',
		// which will take us back to the 'for' loop to keep going up the exception stack,
		// until we find a frame with some other value.
		kCatchAction_Rethrow,

		// The CATCH block will walk up the stack of ActionScript exception frames, looking
		// for an ActionScript try/catch block which will catch it.
		kCatchAction_SearchForActionScriptExceptionHandler
	};
#endif

	/**
	 * ExceptionFrame class is used to track stack frames that contain
	 * exception handlers.
	 */
	class ExceptionFrame
	{
	public:
		ExceptionFrame()
		{
			core = NULL;
#ifdef DEBUGGER
			this->catchAction = kCatchAction_Unknown;
#endif
		}
		~ExceptionFrame() { endTry(); }
		void beginTry(AvmCore* core);
		void endTry();
		void beginCatch();
		void throwException(Exception *exception);

		AvmCore *core;
		ExceptionFrame *prevFrame;
		Namespace*const* dxnsAddr;
		jmp_buf jmpbuf;
		CodeContextAtom codeContextAtom;
#ifdef DEBUGGER
		CallStackNode *callStack;

		// Indicates what the CATCH block is going to do if it sees an exception.
		// (A CatchAction should only need 3 bits, but I have to give it 4 in
		// order to avoid problems with sign-extension.)
		CatchAction catchAction:4;
#endif /* DEBUGGER */
	};

	/**
	 * TRY, CATCH, and friends are macros for setting up exception try/catch
	 * blocks.  This is similar to the TRY, CATCH, etc. macros in MFC.
	 *
	 * AVM+ uses its own exception handling mechanism implemented using
	 * setjmp/longjmp.  Hosts of AVM+ can bridge these exceptions into
	 * regular C++ exceptions by catching and re-throwing. 
	 * 
	 * TRY_UNLESS is to support the optimization that if there are no exception handlers
	 * in this frame, we don't need to create the exception frame at all.  If expr
	 * is true, the exception frame is not created.
	 */
#ifdef DEBUGGER
	#define TRY(core, CATCH_ACTION) { \
		ExceptionFrame _ef; \
		_ef.beginTry(core); \
		_ef.catchAction = (CATCH_ACTION); \
		Exception* _ee; \
		if ((_ee=(Exception*)::setjmp(_ef.jmpbuf)) == NULL)
#else
	#define TRY(core, CATCH_ACTION) { \
		ExceptionFrame _ef; \
		_ef.beginTry(core); \
		Exception* _ee; \
		if ((_ee=(Exception*)::setjmp(_ef.jmpbuf)) == NULL)
#endif

#ifdef DEBUGGER
	#define TRY_UNLESS(core,expr,CATCH_ACTION) { \
		ExceptionFrame _ef; \
		Exception* _ee; \
		if ((expr) || (_ef.beginTry(core), _ef.catchAction=(CATCH_ACTION), (_ee=(Exception*)::setjmp(_ef.jmpbuf)) == NULL))
#else
	#define TRY_UNLESS(core,expr,CATCH_ACTION) { \
		ExceptionFrame _ef; \
		Exception* _ee; \
		if ((expr) || (_ef.beginTry(core), (_ee=(Exception*)::setjmp(_ef.jmpbuf)) == NULL))
#endif
    #define CATCH(x) else { _ef.beginCatch(); x = _ee;
    #define END_CATCH }
    #define END_TRY }
}

#endif /* __avmplus_Exception__ */
