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

#ifndef __avmplus_ErrorClass__
#define __avmplus_ErrorClass__


namespace avmplus
{
	/**
	 * class Error
	 */
	class ErrorClass : public ClassClosure
	{
	public:
		DECLARE_NATIVE_MAP(ErrorClass)
		ErrorClass(VTable* cvtable);

		Atom call(int argc, Atom* argv)
		{
			return construct(argc, argv);
		}

		ScriptObject *createInstance(VTable *ivtable, ScriptObject *delegate);


		/**
		 * throwError is a convenience function for throwing
		 * an exception with a formatted error message,
		 * printf-style
		 */
		void throwError(int errorID, Stringp arg1=0, Stringp arg2=0, Stringp arg3=0);

		/** @name static methods */
		/*@{*/
		Stringp getErrorMessage(int errorID) const;
#ifdef DEBUGGER
		Atom getStackTrace(Atom thisAtom,
						   Atom *argv,
						   int argc);
#endif /* DEBUGGER */
	};

	/**
	 * The ErrorObject class is the C++ implementation of the
	 * Error class in the ECMA-262 Specification.
	 *
	 * The main difference between an ErrorObject and a regular
	 * ScriptObject is the presence of the stack trace, which
	 * can be retrieved with the getStackTrace method.
	 */
	class ErrorObject : public ScriptObject
	{
	public:
		ErrorObject(VTable *vtable, ScriptObject *delegate);
		~ErrorObject() {
#ifdef DEBUGGER
			stackTrace = 0; 
#endif
		}

		Stringp stackTraceToString() const;
#ifdef DEBUGGER
		StackTrace *getStackTrace() const { return stackTrace; }

	private:
		StackTrace *stackTrace;
#endif /* DEBUGGER */
	};

	/**
	 * NativeErrorClass
	 *
	 * This class exists primarily to override the [[Call]] entry point
	 * to the class closure, so that it constructs an error object
	 * instead of attempting a coercion.
	 */
	class NativeErrorClass : public ClassClosure
	{
	public:
		DECLARE_NATIVE_MAP(NativeErrorClass)

		NativeErrorClass(VTable* cvtable);

		Atom call(int argc, Atom* argv)
		{
			return construct(argc, argv);
		}
	};
}

#endif /* __avmplus_ErrorClass__ */
