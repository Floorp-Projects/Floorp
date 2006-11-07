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
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
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


#ifndef __avmplus_Debugger__
#define __avmplus_Debugger__

#ifdef DEBUGGER
namespace avmplus
{
	enum DIType
	{
		DI_BAD = 0,
		DI_LOCAL,
	};

	/**
	 * ----------------------------------------------------------
	 *   Interfaces supported by the AVM+ Debugger
	 * 
	 * ----------------------------------------------------------
	 */

	class SourceInfo : public MMgc::GCFinalizedObject
	{
	public:
		/**
		 * Dummy destructor
		 */
		virtual ~SourceInfo() {}
		
		/**
		 * Name of the source file
		 */
		virtual Stringp name() const = 0;

		/**
		 * Number of functions defined in this file.  
		 */
		virtual int functionCount() const = 0;

		/**
		 * Access to each function.  This is 
		 * accomplished via a call to locationFor
		 * which translates a source line number to a
		 * function index and an offset within the
		 * 
		 */
		virtual MethodInfo* functionAt(int index) const = 0;

		/**
		 * Sets a breakpoint at the specified line.
		 * @return true for success, false for failure (e.g. because
		 * there is no source code at that line).
		 */
		virtual bool setBreakpoint(int linenum) = 0;

		/**
		 * Removes a breakpoint.
		 */
		virtual bool clearBreakpoint(int linenum) = 0;

		/**
		 * Returns whether this source file has a breakpoint at
		 * the indicated line.
		 */
		virtual bool hasBreakpoint(int linenum) = 0;
	};

	class AbcInfo : public MMgc::GCFinalizedObject
	{
	public:
		/**
		 * Dummy destructor
		 */
		virtual ~AbcInfo() {}

		/**
		 * Information about the source files encountered within this abc file.
		 */
		virtual int sourceCount() const = 0;

		/**
		 * SourceInfo at index in array
		 */
		virtual SourceInfo* sourceAt(int index) const = 0;

		/**
		 * Number of bytes in the abc file. 
		 */
		virtual int size() const = 0;
	};

	class DebugFrame
	{
	public:
		// since we have virtual functions, we probably need a virtual dtor
		virtual ~DebugFrame() {}

		/**
		 * Identifies the source file and source line number
		 * corresponding to this frame 
		 */
		virtual bool sourceLocation(SourceInfo*& source, int& linenum) = 0;

		/**
		 * @returns a pointer to an array of count Atoms
		 */
		virtual bool arguments(Atom*& ar, int& count) = 0;

		/**
		 * @return a pointer to an object Atom whose members are
		 * the active locals for this frame.
		 */
		virtual bool locals(Atom*& ar, int& count) = 0;

		/**
		 * Set the value of a particular argument in the frame
		 */
		virtual bool setArgument(int which, Atom& val) = 0;

		/**
		 * Set the value of a particular local variable 
		 * in the frame
		 */
		virtual bool setLocal(int which, Atom& val) = 0;

		/**
		 * This pointer for the frame
		 */
		virtual bool dhis(Atom& a) = 0;
	};

	// forward refs
	class AbcFile;
	class SourceFile;
	class DebugStackFrame;

	/**
	 * Debugger support for the AVM+ virtual machine.
	 *
	 * Debugger is an abstract base class which must be subclassed.
	 * The Debugger base class will do the needed bookkeeping
	 * to track current file, line number, and has the logic
	 * for single-stepping through code and setting breakpoints.
	 * What it lacks is a user interface.
	 *
	 * Programs that embed the AVM+ virtual machine that want
	 * debugging support must subclass Debugger and override
	 * methods to actually put a face on the debugger.
	 *
	 * The class DebugCLI in the AVM+ command-line shell is an
	 * example of a Debugger subclass that provides a simple
	 * gdb-like interface.
	 */
	class Debugger : public MMgc::GCFinalizedObject
	{
	public:
		/**
		 * Constructor; must be invoked from subclass to
		 * initialize the Debugger's internal state.
		 */
		Debugger(AvmCore *core);
		virtual ~Debugger() {}

		/**
		 * enterDebugger must be overridden.
		 * It should block and present the actual debugger UI.
		 */
		virtual void enterDebugger() = 0;

		/**
		 * filterException must be overridden.
		 * This gives a debugger an opportunity to look at
		 * an exception at the instant before it is thrown.
		 * The debugger may choose to stop execution at this
		 * point and permit the developer to debug.
		 *
		 * @returns true if the debugger showed this exception
		 * to the developer (either by halting, or by doing a
		 * stack dump, or any other means); false if the debugger
		 * ignored the exception, in which case Flash might dump
		 * it to the console and/or display a message box.
		 */
		virtual bool filterException(Exception *exception) = 0;
		
		/**
		 * Called at the end of the method's prologue 
		 * to notify the debugger that a new method is about to be executed.
		 */
		virtual void debugMethod(MethodEnv* env);

		/**
		 * Non-virtual version of debugMethod, for calling
		 * from generated code
		 *
		 * WARNING:		 
		 * Do not make this virtual.  It is called from generated code.		 
		 */
		void _debugMethod(MethodEnv* env);
		
		/**
		 * debugLine is called from executing bytecode to
		 * report the current line number.  If linenum == -1,
		 * that means that the current line number has not changed
		 * -- in other words, we're currently in the middle of a
		 * line -- but that the debugger nonetheless needs to be
		 * given the opportunity to break in.
		 *
		 * WARNING:		 
		 * Do not make this virtual.  It is called from generated code.		 
		 */
		void debugLine(int linenum);

		/**
		 * debugFile is called from executing bytecode to
		 * report the file name that debugLine is reporting
		 * line numbers against.
		 *
		 * WARNING:		 
		 * Do not make this virtual.  It is called from generated code.
		 */
		void debugFile(Stringp file);

		/**
		 * Called when an abc file is first decoded.
		 * This method builds a list of source files
		 * and methods, etc from the given abc file.
		 */
		virtual void processAbc(PoolObject* pool, ScriptBuffer code);

		/**
		 * --------------------------------------------------
		 *    Execution related methods
		 * --------------------------------------------------
		 */

		/**
		 * Step to the next executable source line within the 
		 * program, will enter into functions.
		 *
		 * This method sets the halt state and returns;
		 * the debugger must return to actually resume execution.
		 */
		void stepInto();
		
		/**
		 * Step to the next executable source line within
		 * the program, will NOT enter into functions.
		 *
		 * This method sets the halt state and returns;
		 * the debugger must return to actually resume execution.
		 */
		void stepOver();

		/**
		 * Step out of the current method/function onto the 
		 * next executable source line.
		 *
		 * This method sets the halt state and returns;
		 * the debugger must return to actually resume execution.
		 */
		void stepOut();

		/**
		 * If this is called, that indicates that enterDebugger() was called, but
		 * for some reason the debugger on the other end of the socket connection told
		 * us that it did not want to stop -- it wanted to continue with whatever
		 * "step" command was already in progress.  The main case where this happens
		 * is when the user steps, and we hit a conditional breakpoint, but it turns
		 * out that the condition of the breakpoint has not been met.
		 */
		void stepContinue();

		/**
		 * --------------------------------------------------
		 *    Breakpoints
		 * --------------------------------------------------
		 */

		/**
		 * Set a breakpoint in a particular source flie
		 * NOTE: if the 'same' source file appears in 
		 * multiple abc files, it is up to the caller
		 * to ensure that this call is performed for each 
		 * SourceInfo object.
		 * 
		 * Setting the same breakpoint multiple times
		 * has no further effect
		 */
		bool breakpointSet(SourceInfo* src, int linenum);

		/**
		 * Clear a breakpoint on a particular source file
		 * Clearing a breakpoing that was not previously
		 * set has no effect and returns false.
		 */
		bool breakpointClear(SourceInfo* src, int linenum);

		/**
		 * --------------------------------------------------
		 *    Watchpoints
		 * --------------------------------------------------
		 */

		/**
		 * Set a watchpoint that halts debugger execution when 
		 * a read/write access occurs on the variable named memberName.
		 * @param context is either the parent variable on which the
		 * member exists or some other context (such as _global, _level0, 
		 * the locals array, the args array, etc) in which the 
		 * variable is found.
		 * @param memberName is the name of the member on which the 
		 * watch should be placed.
		 * @param k is the type of watch; one of read, write, readwrite.
		 */
		//bool watchpointSet(Atom context, Atom memberName, WatchKind k);

		/**
		 * Remove a previously set watchpoint from a variable member. 
		 * @param context is either the parent variable on which the
		 * member exists or some other context (such as _global, _level0, 
		 * the locals array, the args array, etc) in which the 
		 * variable is found.
		 * @param memberName is the name of the member on which the 
		 * watch should be placed.
		 * @param k is the type of watch; one of read, write, readwrite.
		 */
		//bool watchpointClear(Atom context, Atom memberName);

		/**
		 * Checks whether any variables that are being watched have
		 * changed.  Returns true if at least one has changed, or
		 * false if none of them have changed.
		 */
		virtual bool hitWatchpoint() = 0;

		/**
		 * --------------------------------------------------
		 *    Call stack (frame) related methods 
		 * --------------------------------------------------
		 */

		/**
		 * Returns the call stack depth (i.e. number of frames).
		 */
		int frameCount();
		
		/**
		 * Set frame to point to the specified frame number
		 * or null if frame number does not exist. 
		 */
		DebugFrame* frameAt(int frameNbr);

		/**
		 * --------------------------------------------------
		 *    Abc file information
		 * --------------------------------------------------
		 */

		/**
		 * # of abc files available
		 */
		int abcCount() const;

		/**
		 * Get information on each of the abc files that
		 * have been loaded. 
		 */
		AbcInfo* abcAt(int index) const;

		/**
		 * --------------------------------------------------
		 *    Trace facility for dumping out method entry 
		 * line number and file name information while executing
		 * --------------------------------------------------
		 */

		typedef enum _TraceLevel 
		{
			TRACE_OFF = 0,
			TRACE_METHODS = 1,						// method entry only 
			TRACE_METHODS_WITH_ARGS = 2,			// method entry and arguments
			TRACE_METHODS_AND_LINES = 3,			// method entry and line numbers
			TRACE_METHODS_AND_LINES_WITH_ARGS = 4,	// method entry, arguments and line numbers
		} TraceLevel;

		static TraceLevel astrace; 
		static uint64 astraceStartTime;

		void traceMethod(AbstractFunction* fnc, bool ignoreArgs=false);
		void traceLine(int linenum);

	protected:
		friend class DebugStackFrame;

		AvmCore *core;

		class StepState {
		public:
			StepState() { clear(); }
			void clear() { flag = false; depth = startingDepth = -1; }

			bool flag;
			int depth;
			int startingDepth;
		};

		StepState stepState;
		StepState oldStepState;

		// helper: find a StackTrace object for a given frame number
		CallStackNode* locateTrace(int frameNbr);

		// internal helper functions for parsing abcfiles
		void scanResources(AbcFile* file, PoolObject* pool);
		bool scanCode(AbcFile* file, PoolObject* pool, MethodInfo* m);

		// all abc files
		List<AbcInfo*, LIST_GCObjects>				abcList;
		MMgc::GCHashtable			pool2abcIndex;

	private:
		static int readS24(const byte *pc) { return AvmCore::readS24(pc); }
		static int readU16(const byte *pc) { return AvmCore::readU16(pc); }
		static int readU30(const byte *&pc) { return AvmCore::readU30(pc); }

	};

	/**
	 * ----------------------------------------------------------
	 *   Implementation classes for the interfaces defined above
	 * ----------------------------------------------------------
	 */

	class AbcFile : public AbcInfo
	{
	public:
		/**
		 * Information about the source files encountered within this abc file.
		 */
		int sourceCount() const;

		/**
		 * SourceInfo at index in array
		 */
		SourceInfo* sourceAt(int index) const;

		/**
		 * Number of bytes in the abc file. 
		 */
		int size() const;
		
		/**
		 * Contains all known debug information regarding a single 
		 * abc/swf file
		 */
		AbcFile(AvmCore* core, int size);

		/**
		 * Add source file to list; no check for uniqueness
		 */
		void sourceAdd(SourceFile* s);

		/*
		 * Find a source file in the list that has the same name 
		 * as that provided 
		 */
		SourceFile* sourceNamed(Stringp name);

	protected:
		AvmCore*			core;
		DWB(Hashtable*)		sourcemap;	// maps filename to that file's index in "sources"
		List<SourceFile*,LIST_GCObjects>	source;		// all source files used in this abc file
		int					byteCount;	// # bytes of bytecode 
	};

	class SourceFile : public SourceInfo
	{
	public:
		SourceFile(MMgc::GC* gc, Stringp name);

		/**
		 * name of source file 
		 */
		Stringp name() const;

		/**
		 * Number of functions defined in this file.  
		 */
		int functionCount() const;

		/**
		 * Access to each function 
		 */
		MethodInfo* functionAt(int index) const;

		/**
		 * A line - offset pair should be recorded 
		 */
		void addLine(AvmCore* core, int linenum, MethodInfo* function, int offset);

		bool setBreakpoint(int linenum);
		bool clearBreakpoint(int linenum);
		bool hasBreakpoint(int linenum);

	protected:
		Stringp							named;
		List<MethodInfo*, LIST_GCObjects>				functions;
		DWB(BitSet*)					sourceLines;	// lines that have source code on them
		DWB(BitSet*)					breakpoints;
	};

	class DebugStackFrame : public MMgc::GCObject, public DebugFrame
	{
	public:
		/**
		 * Identifies the source file and source line number
		 * corresponding to this frame 
		 */
		bool sourceLocation(SourceInfo*& source, int& linenum);

		/**
		 * @returns a pointer to an array of count Atoms
		 */
		bool arguments(Atom*& ar, int& count);

		/**
		 * @return a pointer to an array of Atoms whose 
		 * 'count' members are the active locals for this frame.
		 */
		bool locals(Atom*& ar, int& count);

		/**
		 * Set the value of a particular argument in the frame
		 */
		bool setArgument(int index, Atom& val);

		/**
		 * Set the value of a particular local variable 
		 * in the frame
		 */
		bool setLocal(int index, Atom& val);

		/**
		 * This pointer for the frame.  
		 */
		bool dhis(Atom& a);

		// constructor 
		DebugStackFrame(int nbr, CallStackNode* trace, Debugger* debug);

		// expose this for all interested
		CallStackNode* trace;

	protected:
		void argumentBounds(int* firstArgument, int* pastLastArgument);
		void localBounds(int* firstLocal, int* pastLastLocal);
		int indexOfFirstLocal();

		Debugger* debugger;
		int		  frameNbr;  // top of call stack == 0 
	};

}
#endif /* DEBUGGER */

#endif /* __avmplus_Debugger__ */
