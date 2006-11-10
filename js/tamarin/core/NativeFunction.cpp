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



#include "avmplus.h"

namespace avmplus
{
	NativeMethod::NativeMethod(int flags, Handler handler)
		: AbstractFunction()
	{
		m_handler = handler;
		this->flags |= flags;
		this->impl32 = verifyEnter;
	}

	/**
		* invoke handler as handler(int cookie, ...) with native typed args
		*/
	NativeMethod::NativeMethod(int flags, Handler handler, int cookie)
		: AbstractFunction()
	{
		m_handler = handler;
		m_cookie  = cookie;
		this->flags |= (flags | NATIVE_COOKIE);
		this->impl32 = verifyEnter;
	}


	Atom NativeMethod::verifyEnter(MethodEnv* env, int argc, uint32 *ap)
	{
		NativeMethod* f = (NativeMethod*) env->method;

		f->verify(env->vtable->toplevel);

		#ifdef AVMPLUS_VERIFYALL
		f->flags |= VERIFIED;
		if (f->pool->core->verifyall && f->pool)
			f->pool->processVerifyQueue(env->toplevel());
		#endif

		env->impl32 = f->impl32;
		return f->impl32(env, argc, ap);
	}

	void NativeMethod::verify(Toplevel *toplevel)
	{
		AvmAssert(declaringTraits->linked);
		resolveSignature(toplevel);

		// generate the native method thunk
		CodegenMIR cgen(this);
		cgen.emitNativeThunk(this);
		if (cgen.overflow)
			toplevel->throwError(kOutOfMemoryError);
	}

	Atom NativeMethodV::verifyEnter(MethodEnv* env, int argc, uint32 *ap)
	{
		NativeMethodV* f = (NativeMethodV*) env->method;

		f->verify(env->vtable->toplevel);

		#ifdef AVMPLUS_VERIFYALL
		f->flags |= VERIFIED;
		if (f->pool->core->verifyall && f->pool)
			f->pool->processVerifyQueue(env->toplevel());
		#endif

		env->impl32 = f->impl32;
		return f->impl32(env, argc, ap);
	}

	Atom NativeMethodV::implv32(MethodEnv* env, 
				int argc, uint32 *ap)
	{
		// get the instance pointer before we unbox everything
		ScriptObject* instance = *(ScriptObject **)(ap);

#ifdef NATIVE_GLOBAL_FUNCTION_HACK
		// HACK for native toplevel functions
		instance = (ScriptObject*)(~7 & (int)instance);
#endif

		// convert the args in place
		Atom* atomv = (Atom*) ap;

		NativeMethodV* nmv = (NativeMethodV*) (AbstractFunction*) env->method;

		nmv->boxArgs(argc, ap, atomv);

		// skip the instance atom
		atomv++;

#ifdef DEBUGGER
		if (env->core()->debugger != 0)
			env->core()->debugger->traceMethod(nmv, true);  // tracing for native methods
#endif	/* DEBUGGER */

		if (nmv->m_haveCookie) {
			CookieHandler32 cookieHandler = (CookieHandler32)nmv->m_handler;
			return (instance->*cookieHandler)(nmv->m_cookie, atomv, argc);
		} else {
			Handler32 handler = (Handler32) nmv->m_handler;
			return (instance->*handler)(atomv, argc);
		}
	}

	double NativeMethodV::implvN(MethodEnv* env,
				int argc, uint32 * ap)
	{
		// get the instance pointer before we unbox everything
		ScriptObject* instance = *(ScriptObject **)(ap);

#ifdef NATIVE_GLOBAL_FUNCTION_HACK
		// HACK for native toplevel functions
		instance = (ScriptObject*)(~7 & (int)instance);
#endif

		// convert the args in place
		Atom* atomv = (Atom*) ap;
		
		NativeMethodV* nmv = (NativeMethodV*) (AbstractFunction*) env->method;

		nmv->boxArgs(argc, ap, atomv);

		// skip the instance atom
		atomv++;

#ifdef DEBUGGER
		if (env->core()->debugger != 0)
			env->core()->debugger->traceMethod(nmv, true);  // tracing for native methods
#endif	/* DEBUGGER */

		if (nmv->m_haveCookie) {
			CookieHandlerN cookieHandler = (CookieHandlerN)nmv->m_handler;
			return (instance->*cookieHandler)(nmv->m_cookie, atomv, argc);
		} else {
			HandlerN handler = (HandlerN) nmv->m_handler;
			return (instance->*handler)(atomv, argc);
		}
	}

}
