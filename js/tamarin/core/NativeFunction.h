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


#ifndef __avmplus_NativeFunction__
#define __avmplus_NativeFunction__

namespace avmplus
{

	/**
	 * The NativeMethod class is a wrapper to bind a native C++ function
	 * to a class method surfaced into the ActionScript world.
	 * The class method must be defined with the "native" attribute.
	 *
	 * NativeMethod uses C++ calling conventions when calling the
	 * native C++ code.  Incoming parameters are coerced to the
	 * proper type before the call as follows:
	 *
	 * AS type         C++ type
	 * -------         --------
	 * Void            Atom, if parameter, void if return type
	 * Object          Atom
	 * Boolean         bool
	 * Number          double
	 * String          Stringp (String *)
	 * Class           ClassClosure*
	 * MovieClip       MovieClipObject*   (similar for any other class)
	 */
	class NativeMethod : public AbstractFunction
	{
	public:
		typedef void (ScriptObject::*Handler)();
		typedef Atom (ScriptObject::*GetHandler)();
		typedef void (ScriptObject::*SetHandler)(Atom);

		/**
		 * invoke handler as handler(...) with native typed args
		 */
		NativeMethod(int flags, Handler handler);

		/**
		 * invoke handler as handler(int cookie, ...) with native typed args
		 */
		NativeMethod(int flags, Handler handler, int cookie);
		
		// we have virtual functions, so we probably need a virtual dtor
		virtual ~NativeMethod() {}

		static int verifyEnter(MethodEnv* env, int argc, va_list ap);

		virtual void verify(Toplevel* toplevel);

		int m_cookie;
		union {
			Handler m_handler;
			int m_handler_addr;
		};
		
	};

	/**
	 * NativeMethodV is similar to NativeMethod but does not coerce
	 * arguments automatically to appropriate C++ types.
	 * Instead, the C++ code always receives arguments in the
	 * form: Atom method(Atom *argv, int argc);
	 *
	 * NativeMethodV is deprecated and will eventually be removed.
	 */
	class NativeMethodV : public AbstractFunction
	{
	public:
		typedef int (ScriptObject::*Handler32)(Atom *argv, int argc);
		typedef int (ScriptObject::*CookieHandler32)(int cookie, Atom *argv, int argc);		
		typedef double (ScriptObject::*HandlerN)(Atom *argv, int argc);
		typedef double (ScriptObject::*CookieHandlerN)(int cookie, Atom *argv, int argc);		

		/**
		 * invoke handler as handler(...) with native typed args
		 */
		NativeMethodV(Handler32 handler, int flgs)
			: AbstractFunction()
		{
			m_handler = handler;
			this->flags |= (flgs | NEED_REST);
			this->impl32 = verifyEnter;
		}

		/**
		 * invoke handler as handler(int cookie, ...) with native typed args
		 */
		NativeMethodV(Handler32 handler, int cookie, int flgs)
			: AbstractFunction()
		{
			m_handler = handler;
			m_cookie  = cookie;
			m_haveCookie = true;
			this->impl32 = verifyEnter;
			this->flags = flgs;
		}

		void verify(Toplevel* toplevel)
		{
			AvmAssert(declaringTraits->linked);
			resolveSignature(toplevel);
			AvmCore* core = this->core();
			if (returnTraits() == NUMBER_TYPE)
				impl32 = implv32;
			else
				implN = implvN;
		}

		static int verifyEnter(MethodEnv* env, int argc, va_list ap);

		static int implv32(MethodEnv* env, 
				   int argc, va_list ap);
		static double implvN(MethodEnv* env,
				   int argc, va_list ap);
		
	private:
		bool m_haveCookie;
		int m_cookie;
		Handler32 m_handler;
	};


	/**
	 * NativeTableEntry is an internal structure used for native
	 * method tables.  It is wrapped by the NATIVE_METHOD() macros
	 * below.
	 */
	struct NativeTableEntry
	{
		typedef void (ScriptObject::*Handler)();
		int method_id;
		enum {
			kNativePrefix,
			kNativeMethod,
			kNativeMethod1,
			kNativeMethodRest,
			kNativeMethodV,
			kNativeMethodV1,
		} type;
		Handler handler;
		int cookie;
		int flags;
	};

	/**
	 * Macros for declaring native methods
	 */
#define BEGIN_NATIVE_MAP(_Class) NativeTableEntry _Class::natives[] = {
		
#define DECLARE_NATIVE_MAP(_Class) static ClassClosure* createClassClosure(VTable* cvtable) \
{ return new (cvtable->gc(), cvtable->getExtraSize()) _Class(cvtable); } \
static NativeTableEntry natives[];

#define DECLARE_NATIVE_SCRIPT(_Script) static ScriptObject* createGlobalObject(VTable* vtable, ScriptObject* delegate) \
{ return new (vtable->gc(), vtable->getExtraSize()) _Script(vtable, delegate); } \
static NativeTableEntry natives[];

#define NATIVE_METHOD(method_id, handler) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethod, (NativeTableEntry::Handler)&handler, 0, AbstractFunction::NEEDS_CODECONTEXT | AbstractFunction::NEEDS_DXNS },
#define NATIVE_METHOD_FLAGS(method_id, handler, fl) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethod, (NativeTableEntry::Handler)&handler, 0, fl },

#define NATIVE_METHOD2(method_id, handler) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethod, (NativeTableEntry::Handler)(handler), 0, AbstractFunction::NEEDS_CODECONTEXT | AbstractFunction::NEEDS_DXNS },
#define NATIVE_METHOD2_FLAGS(method_id, handler,fl) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethod, (NativeTableEntry::Handler)(handler), 0, fl },

#define NATIVE_METHOD1(method_id, handler, cookie) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethod1, (NativeTableEntry::Handler)&handler, cookie, AbstractFunction::NEEDS_CODECONTEXT | AbstractFunction::NEEDS_DXNS },
#define NATIVE_METHOD1_FLAGS(method_id, handler, cookie, fl) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethod1, (NativeTableEntry::Handler)&handler, cookie, fl },

#define NATIVE_METHODV(method_id, handler) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethodV, (NativeTableEntry::Handler)&handler, 0, AbstractFunction::NEEDS_CODECONTEXT | AbstractFunction::NEEDS_DXNS },
#define NATIVE_METHODV_FLAGS(method_id, handler, fl) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethodV, (NativeTableEntry::Handler)&handler, 0, fl },
		
#define NATIVE_METHODV1(method_id, handler, cookie) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethodV1, (NativeTableEntry::Handler)&handler, cookie, AbstractFunction::NEEDS_CODECONTEXT | AbstractFunction::NEEDS_DXNS },
#define NATIVE_METHODV1_FLAGS(method_id, handler, cookie, fl) { avmplus::NativeID::method_id, NativeTableEntry::kNativeMethodV1, (NativeTableEntry::Handler)&handler, cookie, fl },


#define END_NATIVE_MAP() { -1, NativeTableEntry::kNativeMethod, NULL, 0 } };

    /**
	 * NativeScriptInfo is an internal structure used for
	 * native script tables.  It is wrapped by the
	 * NATIVE_SCRIPT() macro.
	 */
    struct NativeScriptInfo
	{
		typedef ScriptObject* (*Handler)(VTable*, ScriptObject*);

		int script_id;
		Handler handler;
		NativeTableEntry *nativeMap;
		int sizeofInstance;
	};

	/**
	 * NativeClassInfo is an internal structure used for native
	 * method tables.  It is wrapped by the NATIVE_CLASS() macro.
	 */
	struct NativeClassInfo
	{
		typedef ClassClosure* (*Handler)(VTable*);

		int class_id;
		Handler handler;
		NativeTableEntry *nativeMap;
		int sizeofClass;
		int sizeofInstance;
	};
}	

#endif /* __avmplus_NativeFunction__ */
