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

#ifndef __avmplus_AbstractFunction__
#define __avmplus_AbstractFunction__


#ifdef verify
#undef verify
#endif

namespace avmplus
{
	/**
	 * AbstractFunction is the base class for all functions that
	 * can be executed by the VM: Actionscript functions,
	 * native functions, etc.
	 */
	class AbstractFunction : public MMgc::GCObject
	{
	public:
		/** @name flags from .abc - limited to a BYTE */
		/*@{*/
		/** need arguments[0..argc] */
		static const int NEED_ARGUMENTS		= 0x00000001;

		/** need activation object */
		static const int NEED_ACTIVATION	= 0x00000002;

		/** need arguments[param_count+1..argc] */		
		static const int NEED_REST          = 0x00000004;

		/** has optional parameters */
		static const int HAS_OPTIONAL       = 0x00000008;

		/** allow extra args, but dont capture them */
		static const int IGNORE_REST        = 0x00000010;

		/** method is native */
		static const int NATIVE				= 0x00000020;

		/** method sets default namespace */
		static const int SETS_DXNS			= 0x00000040;

		/** method has table for parameter names */
		static const int HAS_PARAM_NAMES	= 0x00000080;

		/*@}*/

		/** @name internal flags - upper 3 BYTES available */
		/*@{*/
		static const int OVERRIDE           = 0x00010000;

		static const int NON_INTERRUPTABLE	= 0x00020000;

		static const int UNBOX_THIS         = 0x00040000;

		static const int NEEDS_CODECONTEXT  = 0x00080000;

		static const int HAS_EXCEPTIONS		= 0x00100000;

#ifdef AVMPLUS_VERBOSE
		static const int VERBOSE_VERIFY		= 0x00200000;
#endif

		static const int NEEDS_DXNS			= 0x00400000;

		static const int VERIFIED			= 0x00800000;
#ifdef AVMPLUS_VERIFYALL
		static const int VERIFY_PENDING		= 0x01000000;
#endif

		/** indicates method is final, no overrides allowed */
		static const int FINAL				= 0x02000000;

		/** indicates the function is a method, that pushes the
		    receiver object onto the scope chain at method entry */
		static const int NEED_CLOSURE       = 0x04000000;

		/** set to indicate that a function has no bytecode body. */
		static const int ABSTRACT_METHOD	= 0x08000000;

		#ifdef AVMPLUS_INTERP
		/**
		 * set to indicate that a function has been compiled
		 * to native code.  In release mode we always compile
		 * so we don't need the flag.
		 */
		static const int TURBO				= 0x80000000;

		/**
		 * set to indictate that a function has been 
		 * recommended to be interpreted. 
		 */
		static const int SUGGEST_INTERP		= 0x40000000;
		#endif /* AVMPLUS_INTERP */

		/**
		 * set once the signature types have been resolved and
		 * override signatures have been checked.
		 */
		static const int LINKED             = 0x20000000;

		/*@}*/

		/**
		 * @name NativeMethod flags
		 * These are used in the NativeMethod subclass
		 */
		/*@{*/
		/** cookie int passed into C++ code */
		static const int NATIVE_COOKIE      = 0x10000000;
		/*@}*/

		DWB(Traits*) declaringTraits;
		DWB(Traits*) activationTraits;
		DWB(PoolObject*) pool;
		
		AvmCore* core() const
		{
			return pool->core;
		}

		uintptr iid() const
		{
			return ((uintptr)this)>>3;
		}

		bool usesCallerContext() const
		{
			return pool->isBuiltin && (!(flags & NATIVE) || (flags & NEEDS_CODECONTEXT));
		}

		// Builtin + non-native functions always need the dxns code emitted 
		// Builtin + native functions have flags to specify if they need the dxns code
		bool usesDefaultXmlNamespace() const
		{
			return pool->isBuiltin && (!(flags & NATIVE) || (flags & NEEDS_DXNS));
		}

		/** number of declared parameters including optionals */
		int param_count;

		/** last optional_count params are optional */		
		int optional_count;

		// offset to first rest arg,
		// including the instance parameter.  
		// this is sum(sizeof(paramType(0..N)))
		int restOffset; 

		/** see bitmask defs above */
		int flags; 
		int method_id;

		/** pointer to abc MethodInfo record */
		const byte* info_pos;

		void initParamTypes(int count);
		void initDefaultValues(int count);

		void resolveSignature(const Toplevel* toplevel);

		bool argcOk(int argc)
		{
			return argc >= param_count-optional_count && 
				(argc <= param_count || allowExtraArgs());
		}

		/** 
		 * invoke this method. args are already coerced.  argc
		 * is the number of arguments AFTER the instance, which
		 * is arg 0.  ap will always have at least the instance.
		 */
		union {
			Atom (*impl32)(MethodEnv*, int, uint32 *);
			double (*implN)(MethodEnv*, int, uint32 *);
		};

    protected:
		AbstractFunction();

	public:
		
		void setParamType(int index, Traits* t);
		void setDefaultValue(int index, Atom defaultValue);
		void makeIntoPrototypeFunction(const Toplevel* toplevel);

		Traits* paramTraits(int index) const {
			AvmAssert(index >= 0 && index <= param_count);
			return m_types[index];
		}

		const Atom* getDefaultValues() const {
			return m_values;
		}

		Atom getDefaultValue(int i) const {
			return m_values[i];
		}

		void setReturnType(Traits* t) {
			m_returnType = t;
		}

		Traits* returnTraits() const {
			return m_returnType;
		}

		int requiredParamCount() const {
			return param_count-optional_count;
		}

		int allowExtraArgs() const {
			return isFlagSet(NEED_REST|NEED_ARGUMENTS|IGNORE_REST);
		}

		int hasMethodBody() const {
			return !isFlagSet(ABSTRACT_METHOD);
		}

		int isFlagSet(int f) const {
			return (flags & f);
		}

		int hasExceptions() const {
			return flags & HAS_EXCEPTIONS;
		}

		int setsDxns() const {
			return flags & SETS_DXNS;
		}

		bool makeMethodOf(Traits* type);

#ifdef AVMPLUS_VERIFYALL
		int isVerified() const {
			return flags & VERIFIED;
		}
		virtual void verify(Toplevel* toplevel) = 0;
#endif

		void boxArgs(int argc, uint32 *ap, Atom* out);

	protected:
		DWB(Traits*) m_returnType;
		DWB(Traits**) m_types; // actual length will be 1+param_count
		DWB(Atom*) m_values; // default values for any optional params. size = optional_count

#ifdef AVMPLUS_VERBOSE

		/** Dummy destructor to avoid warnings */
		virtual ~AbstractFunction() {}
	public:
		virtual Stringp format(const AvmCore* core) const;
#endif

#if defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
	public:
		DRCWB(Stringp) name;
#endif
#ifdef DEBUGGER
		virtual uint32 size() const;
#endif
	};
}

#endif /* __avmplus_AbstractFunction__ */
