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

#ifndef __avmplus_CallStackNode__
#define __avmplus_CallStackNode__

namespace avmplus
{
#ifdef DEBUGGER
	class StackTrace : public MMgc::GCObject
	{
	public:
		Stringp format(AvmCore* core);

		/**
		 * The dump and format methods display a human-readable
		 * version of the stack trace.  To avoid thousands of
		 * lines of output, such as would happen in an infinite
		 * recursion, no more than kMaxDisplayDepth levels will
		 * be displayed.
		 */
		const static int kMaxDisplayDepth = 64;

		int depth;
		DRCWB(Stringp) stringRep;

		struct Element
		{
			AbstractFunction *info;
			Stringp filename;			// in the form "C:\path\to\package\root;package/package;filename"
		    int linenum;
		};
		bool equals(StackTrace::Element *e, int depth);
		static uintptr hashCode(StackTrace::Element *e, int depth);

		Element elements[1];
	private:
		void dumpFilename(Stringp filename, PrintWriter& out) const;
	};
		
	/**
	 * The CallStackNode class tracks the call stack of executing
	 * ActionScript code.  It is used for debugger stack dumps
	 * and also to keep track of the security contexts of
	 * running code.  There is a master CallStackNode object on
	 * AvmCore, which is maintained by MethodInfo::enter as
	 * methods enter and exit.
	 *
	 * A single CallStackNode object represents a level of the
	 * call stack.  It tracks the currently executing method,
	 * and the source file and line number if debug info
	 * is available.
	 *
	 * CallStackNode objects get strung together into linked
	 * lists to represent a full call stack.  Entering a
	 * method adds a new element to core->stackTrace's
	 * linked list; exiting a method removes it.
	 *
	 * As OP_debugfile and OP_debugline opcodes are encountered,
	 * the file and line number information in the current
	 * CallStackNode object is updated.
	 *
	 * CallStackNode objects are "semi-immutable."  The file
	 * and line number information can be updated until someone
	 * wants to snapshot the CallStackNode.  To snapshot, the
	 * "freeze" method is called.  This freezes the entire
	 * linked list.  Once a CallStackNode is frozen, it cannot
	 * be directly modified... a clone must be made of it,
	 * and then the clone can be modified.
	 *
	 * The "semi-immutable" aspect is to support AVM+ heap
	 * profiling.  Every ScriptObject will point to the
	 * CallStackNode at time of allocation, so that the developer
	 * can figure out the call stack where an object was
	 * created.  The semi-immutability makes it possible to
	 * conserve memory, by having many ScriptObjects point to
	 * the same CallStackNode chain, or chains containing common
	 * CallStackNode elements.
	 */
	class CallStackNode
	{
	public:
		/**
		 * Constructs a new CallStackNode.  The method executing and
		 * the CallStackNode representing the previous call stack
		 * must be specified.  The source file and line number
		 * will be unset, initially.
		 * @param info the currently executing method
		 * @param next the CallStackNode representing the previous
		 *             call stack
		 */
		CallStackNode(MethodEnv *		env,
					  AbstractFunction *info,
					  Atom*				framep,
					  Traits**			frameTraits,
					  int				argc,
					  uint32 *			ap,
					  sintptr volatile *eip)
		{
			initialize(env, info, framep, frameTraits, argc, ap, eip);
		}

		// WARNING!!!! this method is called by CodegenMIR if you change the signature then change the call there.
		void initialize(MethodEnv *			env,
						AbstractFunction *	info,
						Atom*				framep,
						Traits**			frameTraits,
						int					argc,
						uint32 *			ap,
						sintptr volatile *	eip);

		void exit();

		MethodEnv *env;
		AbstractFunction* info;
		Stringp      filename;			// in the form "C:\path\to\package\root;package/package;filename"
		int         linenum;
		CallStackNode *next;

		int         depth;
		uint32 *    ap;
		int			argc;
		Atom*		framep;		// pointer to top of AS registers
		Traits**    traits;		// array of traits for AS registers
		sintptr volatile * eip; 	// ptr to where the current pc is stored

		void**		scopeBase(); // with MIR, array members are (ScriptObject*); with interpreter, they are (Atom).
		#ifdef AVMPLUS_INTERP
		int*		scopeDepth; // Only used by the interpreter! With MIR, look for NULL entires in the scopeBase array.
		#endif
	protected:
		// allow more flexibility to subclasses
		CallStackNode(){}
	};
	

	class FakeAbstractFunction : public AbstractFunction
	{
	public:
		void verify(Toplevel *) {}
	};

	class FakeCallStackNode : public CallStackNode
	{
	public:
		FakeCallStackNode(AvmCore *core, const char *name);
		~FakeCallStackNode();
		void sampleCheck() { if(core) core->sampleCheck(); }
	private:
		AvmCore *core;
	};

#define SAMPLE_FRAME(_strp, _core) avmplus::FakeCallStackNode __fcsn((avmplus::AvmCore*)_core, _strp)
#define SAMPLE_CHECK()  __fcsn.sampleCheck();

#else

#define SAMPLE_FRAME(_x, _s) 
#define SAMPLE_CHECK()

#endif /* DEBUGGER */
}

#endif /* __avmplus_CallStackNode__ */
