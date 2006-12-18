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
	using namespace MMgc;
	MethodInfo::MethodInfo()
		: AbstractFunction()
	{
		#ifdef DEBUGGER
		this->local_count = 0;
		this->max_scopes = 0;
		this->localNames = 0;
		this->firstSourceLine = 0;
		this->lastSourceLine = 0;
		this->offsetInAbc = 0;
		#endif
		this->impl32 = verifyEnter;
	}

	Atom MethodInfo::verifyEnter(MethodEnv* env, int argc, uint32 *ap)
	{
		MethodInfo* f = (MethodInfo*) env->method;

		f->verify(env->vtable->toplevel);

		#ifdef AVMPLUS_VERIFYALL
		f->flags |= VERIFIED;
		if (f->pool->core->verifyall && f->pool)
			f->pool->processVerifyQueue(env->toplevel());
		#endif

		env->impl32 = f->impl32;
		return f->impl32(env, argc, ap);
	}

	void MethodInfo::verify(Toplevel* toplevel)
	{
		AvmAssert(declaringTraits->linked);
		resolveSignature(toplevel);

		#ifdef DEBUGGER
		CallStackNode callStackNode(NULL, this, NULL, NULL, 0, NULL, NULL);
		#endif /* DEBUGGER */

		if (!body_pos)
		{
			// no body was supplied in abc
			toplevel->throwVerifyError(kNotImplementedError, toplevel->core()->toErrorString(this));
		}

		#ifdef AVMPLUS_MIR

		Verifier verifier(this, toplevel);

		AvmCore* core = this->core();
		if (core->turbo && !isFlagSet(AbstractFunction::SUGGEST_INTERP))
		{
			CodegenMIR mir(this);
			verifier.verify(&mir);	// pass 2 - data flow
			if (!mir.overflow)
				mir.emitMD(); // pass 3 - generate code

			// the MD buffer can overflow so we need to re-iterate
			// over the whole thing, since we aren't yet robust enough
			// to just rebuild the MD code.

			// mark it as interpreted and try to limp along
			if (mir.overflow)
			{
				#ifdef AVMPLUS_INTERP
				AvmCore* core = this->core();
				if (returnTraits() == NUMBER_TYPE)
					implN = Interpreter::interpN;
				else
					impl32 = Interpreter::interp32;
				#else
				toplevel()->throwError(kOutOfMemoryError);
				#endif //AVMPLUS_INTERP
			}
		}
		else
		{
			verifier.verify(NULL); // pass2 dataflow
		}
		#else
		Verifier verifier(this, toplevel);
		verifier.verify(NULL);
		#endif

        #ifdef DEBUGGER
		callStackNode.exit();
        #endif /* DEBUGGER */
	}
	
	#ifdef DEBUGGER

	// reg names
	Stringp MethodInfo::getLocalName(int index) const	{ return getRegName(index+param_count); }
	Stringp MethodInfo::getArgName(int index) const		{ return getRegName(index); }

	Stringp MethodInfo::getRegName(int slot) const 
	{
		AvmAssert(slot >= 0 && slot < local_count);
		Stringp name;
		if (localNames)
			name = localNames[slot];
		else
			name = core()->kundefined;
		return name;
	}

	void MethodInfo::setRegName(int slot, Stringp name)
	{
		//AvmAssert(slot >= 0 && slot < local_count);
		// @todo fix me.  This is a patch for bug #112405
		if (slot >= local_count)
			return;
		if (!localNames)
			initLocalNames();

		AvmCore* core = this->core();

		// [mmorearty 5/3/05] temporary workaround for bug 123237: if the register
		// already has a name, don't assign a new one
		if (getRegName(slot) != core->kundefined)
			return;

		//localNames[slot] = core->internString(name);
		WBRC(core->GetGC(), localNames, &localNames[slot], core->internString(name));
	}

	void MethodInfo::initLocalNames()
	{
		AvmCore* core = this->core();
		localNames = (Stringp*) core->GetGC()->Calloc(local_count, sizeof(Stringp), GC::kZero|GC::kContainsPointers);
		for(int i=0; i<local_count; i++)
		{
			//localNames[i] = core->kundefined;
			WBRC(core->GetGC(), localNames, &localNames[i], core->kundefined);
		}
	}

	/**
	 * convert ap[start]...ap[start+count-1] entries from their native types into
	 * Atoms.  The result is placed into out[to]...out[to+count-1].
	 * 
	 * The traitArr is used to determine the type of conversion that should take place.
	 * traitArr[start]...traitArr[start+count-1] are used.
	 *
	 * If the method is interpreted then we just copy the Atom, no conversion is needed.
	 */
	void MethodInfo::boxLocals(void* src, int srcPos, Traits** traitArr, Atom* dest, int destPos, int length)
	{
		int size = srcPos+length;
		int at = destPos;

		// if we are running mir then the types are native and we
		// need to box em.
		#ifdef AVMPLUS_INTERP
		if (isFlagSet(TURBO))
		#endif //AVMPLUS_INTERP
		{
			// each entry is a pointer into the function's stack frame
			void **in = (void**)src;			// WARNING this must match with MIR generator

			// now probe each type and do the atom conversion.
			AvmCore* core = this->core();
			for (int i=srcPos; i<size; i++)
			{
				Traits* t = traitArr[i];
				void *p = in[i];
				if (t == NUMBER_TYPE) 
				{
					dest[at] = core->doubleToAtom( *((double*)p) );
				}
				else if (t == INT_TYPE)
				{
					dest[at] = core->intToAtom( *((int*)p) );
				}
				else if (t == UINT_TYPE)
				{
					dest[at] = core->uintToAtom( *((uint32*)p) );
				}
				else if (t == BOOLEAN_TYPE)
				{
					dest[at] = *((int*)p) ? trueAtom : falseAtom;
				}
				else if (!t || t == OBJECT_TYPE || t == VOID_TYPE)
				{
					dest[at] = *((Atom*)p);
				}
				else
				{
					// it's a pointer type, either null or some specific atom tag.
					void* ptr = *((void**)p); // unknown pointer
					if (t == STRING_TYPE)
					{
						dest[at] = ((Stringp)ptr)->atom();
					}
					else if (t == NAMESPACE_TYPE)
					{
						dest[at] = ((Namespace*)ptr)->atom();
					}
					else 
					{
						dest[at] = ((ScriptObject*)ptr)->atom();
					}
				}
				at++;
			}
		}
		#ifdef AVMPLUS_INTERP
		else
		{
			// no MIR then we know they are Atoms and we just copy them
			Atom* in = (Atom*)src;
			for(int i=srcPos; i<size; i++)
				dest[at++] = in[i];
		}
		#endif //AVMPLUS_INTERP
	}

	/**
	 * convert in[0]...in[count-1] entries from Atoms to native types placing them 
	 * in ap[start]...out[start+count-1].
	 * 
	 * The traitArr is used to determine the type of conversion that should take place.
	 * traitArr[start]...traitArr[start+count-1] are used.
	 *
	 * If the method is interpreted then we just copy the Atom, no conversion is needed.
	 */
	void MethodInfo::unboxLocals(Atom* src, int srcPos, Traits** traitArr, void* dest, int destPos, int length)
	{
		#ifdef AVMPLUS_64BIT
		AvmDebugMsg (true, "are these ops right for 64-bit?  alignment of int/uint/bool?\n");
		#endif
		int size = destPos+length;
		int at = srcPos;

		// If the method has been jit'd then we need to box em, otherwise just
		// copy them 
		#ifdef AVMPLUS_INTERP
		if (isFlagSet(TURBO))
		#endif //AVMPLUS_srcTERP
		{
			// we allocated double sized entry for each local src CodegenMIR
			void** out = (void**)dest;		// WARNING this must match with MIR generator

			// now probe each type and conversion.
			AvmCore* core = this->core();
			for (int i=destPos; i<size; i++)
			{
				Traits* t = traitArr[i];
				void *p = out[i];
				if (t == NUMBER_TYPE) 
				{
					*((double*)p) = AvmCore::number_d(src[at++]);
				}
				else if (t == INT_TYPE)
				{
					*((int*)p) = AvmCore::integer_i(src[at++]);
				}
				else if (t == UINT_TYPE)
				{
					*((uint32*)p) = AvmCore::integer_u(src[at++]);
				}
				else if (t == BOOLEAN_TYPE)
				{
					*((int*)p) = src[at++]>>3;
				}
				else if (!t || t == OBJECT_TYPE || t == VOID_TYPE)
				{
					*((Atom*)p) = src[at++];
				}
				else
				{
					// ScriptObject, String, Namespace, or Null
					*((sintptr*)p) = (src[at++] & ~7);
				}
			}
		}
		#ifdef AVMPLUS_INTERP
		else
		{
			// no MIR then we know they are Atoms and we just copy them
			Atom* out = (Atom*)dest;
			for(int i=destPos; i<size; i++)
				out[i] = src[at++];
		}
		#endif //AVMPLUS_srcTERP
	}

	uint32 MethodInfo::size() const
	{
		uint32 size = AbstractFunction::size();
		size += (sizeof(MethodInfo) - sizeof(AbstractFunction));
		size += codeSize;
		return size;
	}
	#endif //DEBUGGER
}
