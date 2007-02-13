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

namespace avmplus
{
#undef DEBUG_EARLY_BINDING
	Atom MethodEnv::coerceEnter(int argc, Atom* atomv)
	{
		Toplevel* toplevel = vtable->toplevel;
		
		if (!method->argcOk(argc))
		{
			toplevel->argumentErrorClass()->throwError(kWrongArgumentCountError, core()->toErrorString((AbstractFunction*)method), core()->toErrorString(method->requiredParamCount()), core()->toErrorString(argc));
		}

		AbstractFunction* method = this->method;

		// just do enough to resolve signatures.  Don't do a full verify yet.
		method->resolveSignature(toplevel);

		// check receiver type first
		// caller will coerce instance if necessary,
		// so make sure it was done.
		AvmAssert(atomv[0] == toplevel->coerce(atomv[0], method->paramTraits(0)));

		// now unbox everything, including instance and rest args
		int extra = argc > method->param_count ? argc - method->param_count : 0;
		AvmAssert(method->restOffset > 0 && extra >= 0);
		uint32 *ap = (uint32 *) alloca(method->restOffset + sizeof(Atom)*extra);

		unboxCoerceArgs(argc, atomv, ap);

		// we know we have verified the method, so we can go right into it.
		Traits* returnType = method->returnTraits();
		AvmCore* core = this->core();
		if (returnType == NUMBER_TYPE)
		{
			AvmAssert(method->implN != NULL);
			double d = method->implN(this, argc, ap);
			return core->doubleToAtom(d);
		}
		else
		{
			AvmAssert(method->impl32 != NULL);
			Atom i = method->impl32(this, argc, ap);
			if (returnType == INT_TYPE)
				return core->intToAtom(i);
			else if (returnType == UINT_TYPE)
				return core->uintToAtom((uint32)i);
			else if (returnType == BOOLEAN_TYPE)
				return i ? trueAtom : falseAtom;
			else if (!returnType || returnType == OBJECT_TYPE || returnType == VOID_TYPE)
				return (Atom)i;
			else if (returnType == STRING_TYPE)
				return ((Stringp)i)->atom();
			else if (returnType == NAMESPACE_TYPE)
				return ((Namespace*)i)->atom();
			else
				return ((ScriptObject*)i)->atom();
		}
	}

	
	/**
	 * convert atoms to native args.  argc is the number of
	 * args, not counting the instance which is arg[0].  the
	 * layout is [instance][arg1..argN]
	 */
	void MethodEnv::unboxCoerceArgs(int argc, Atom* in, uint32 *argv)
	{
		AbstractFunction* f = this->method;
		AvmCore* core = this->core();
		Toplevel* toplevel = this->toplevel();

		Atom *args = (Atom *) argv;

		for (int i=0; i <= argc; i++)
		{
			if (i <= f->param_count)
			{
				Traits* t = f->paramTraits(i);
				AvmAssert(t != VOID_TYPE);

				if (t == NUMBER_TYPE) 
				{
					union {
						double d;
						uint32 l[2];
					};
					d = core->number(in[i]);
					#ifdef AVMPLUS_64BIT
					AvmAssert(sizeof(Atom) == sizeof(double));
					*(double *) args = d;
					args++;
					#else
					AvmAssert(sizeof(Atom) * 2 == sizeof(double));
					*args++ = l[0];
					*args++ = l[1];
					#endif
				}
				else if (t == INT_TYPE)
				{
					*args++ = core->integer(in[i]);
				}
				else if (t == UINT_TYPE)
				{
					*args++ = core->toUInt32(in[i]);
				}
				else if (t == BOOLEAN_TYPE)
				{
					*args++ = core->boolean(in[i]);
				}
				else if (t == OBJECT_TYPE)
				{
					*args++ = in[i] == undefinedAtom ? nullObjectAtom : in[i];
				}
				else if (!t)
				{
					*args++ = in[i];
				}
				else
				{
					// ScriptObject, String, or Namespace, or Null
					*args++ = toplevel->coerce(in[i],t) & ~7;
				}
			}
			else
			{
				*args++ = in[i];
			}
		}
	}


	Atom MethodEnv::delegateInvoke(MethodEnv* env, int argc, uint32 *ap)
	{
		env->impl32 = env->method->impl32;
		return env->impl32(env, argc, ap);
	}

	MethodEnv::MethodEnv(void *addr, VTable *vtable)
		: vtable(vtable), method(NULL)
	{
		typedef Atom (*AtomMethodProc)(MethodEnv*, int, uint32 *);
		impl32 = *(AtomMethodProc*) &addr;
	}

	MethodEnv::MethodEnv(AbstractFunction* method, VTable *vtable)
		: vtable(vtable), method(method)
	{
		// make the first call go to the method impl
		impl32 = delegateInvoke;

		AvmCore* core = vtable->traits->core;
		#ifdef AVMPLUS_VERBOSE
		if (method->declaringTraits != vtable->traits)
		{
			core->console << "ERROR " << method->name << " " << method->declaringTraits << " " << vtable->traits << "\n";
		}
		#endif
		if(method->declaringTraits != vtable->traits){
			AvmAssertMsg(0, "(method->declaringTraits != vtable->traits)");
			toplevel()->throwVerifyError(kCorruptABCError);
		}

		if (method->flags & AbstractFunction::NEED_ACTIVATION)
		{
			VTable *activation = core->newVTable(method->activationTraits, NULL, vtable->scope, vtable->abcEnv, toplevel());
			activation->resolveSignatures();
			setActivationOrMCTable(activation, kActivation);
		}

		// register this env in the callstatic method table
		int method_id = method->method_id;
		if (method_id != -1)
		{
			AbcEnv* abcEnv = vtable->abcEnv;
			AvmAssert(abcEnv->pool == (PoolObject *) method->pool);
			if (abcEnv->methods[method_id] == NULL)
			{
				abcEnv->setMethod(method_id, this);
			}
			#ifdef AVMPLUS_VERBOSE
			else if (method->pool->verbose)
			{
				core->console << "WARNING: tried to re-register global MethodEnv for " << method << "\n";
			}
			#endif
		}

	}
	
#ifdef DEBUGGER
	void MethodEnv::debugEnter(int argc, uint32 *ap, 
							   Traits**frameTraits, int localCount,
							   CallStackNode* callstack,
							   Atom* framep, volatile sintptr *eip)
	{
		AvmCore* core = this->core();

		// update profiler
		sendEnter(argc, ap);

		// dont reset the parameter traits since they are setup in the prologue
		int firstLocalAt = method->param_count+1;
		AvmAssert(!frameTraits || localCount >= firstLocalAt);
		if (frameTraits) memset(&frameTraits[firstLocalAt], 0, (localCount-firstLocalAt)*sizeof(Traits*));
		if (callstack) callstack->initialize(this, method, framep, frameTraits, argc, ap, eip);
		core->debugger->_debugMethod(this);

		core->sampleCheck();
	}

	void MethodEnv::debugExit(CallStackNode* callstack)
	{
		AvmAssert(this != 0);
		AvmCore* core = this->core();

		// update profiler 
		sendExit();

		core->callStack = callstack->next;

		// trigger a faked "line number changed" since we exited the function and are now back on the old line (well, almost)
		if (core->callStack && core->callStack->linenum > 0)
		{
			int line = core->callStack->linenum;
			core->callStack->linenum = -1;
			core->debugger->debugLine(line);
		}
	}

	void MethodEnv::sendEnter(int /*argc*/, uint32 * /*ap*/)
	{
		Profiler *profiler = core()->profiler;
		if (profiler->profilingDataWanted)
			profiler->sendFunctionEnter(method);
	}

	void MethodEnv::sendExit()
	{
		Profiler *profiler = core()->profiler;
		if (profiler->profilingDataWanted)
			profiler->sendFunctionExit();
	}
#endif

    void MethodEnv::nullcheck(Atom atom)
    {
		if (!AvmCore::isNullOrUndefined(atom))
			return;

		// TypeError in ECMA
		ErrorClass *error = toplevel()->typeErrorClass();
		if( error ){
			error->throwError(
					(atom == undefinedAtom) ? kConvertUndefinedToObjectError :
										kConvertNullToObjectError);
		} else {
			toplevel()->throwVerifyError(kCorruptABCError);
		}
    }

	void MethodEnv::npe()
	{
		toplevel()->throwTypeError(kConvertNullToObjectError);
	}

	void MethodEnv::interrupt()
	{
		vtable->traits->core->interrupt(this);
	}

	Traits* MethodEnv::toClassITraits(Atom atom)
	{
		switch (atom&7)
		{
		case kObjectType:
		{
			if( !AvmCore::isNull(atom) )
			{
				Traits* itraits = AvmCore::atomToScriptObject(atom)->traits()->itraits;
				if (itraits == NULL)
					toplevel()->throwTypeError(kIsTypeMustBeClassError);
				return itraits;
			}
			// else fall through and report an error
		}
		default:
            // TypeError in ECMA
			// ISSUE the error message should say "whatever" is not a class
			toplevel()->throwTypeError(
					   (atom == undefinedAtom) ? kConvertUndefinedToObjectError :
											kConvertNullToObjectError);
			return NULL;
		}
	}

	ArrayObject* MethodEnv::createRest(Atom* argv, int argc)
	{
		// create arguments Array using argv[param_count..argc]
		Atom* extra = argv + method->param_count + 1;
		int extra_count = argc > method->param_count ? argc - method->param_count : 0;
		return toplevel()->arrayClass->newarray(extra, extra_count);
	}

#ifdef AVMPLUS_MIR

	Atom MethodEnv::getpropertyHelper(Atom obj, Multiname *multi, VTable *vtable, Atom index)
	{
		if ((index&7) == kIntegerType)
		{
			return getpropertylate_i(obj, index>>3);
		}

		if ((index&7) == kDoubleType)
		{
			int i = AvmCore::integer_i(index);
			if ((double)i == AvmCore::atomToDouble(index))
			{
				return getpropertylate_i(obj, i);
			}
		}

		if (AvmCore::isObject(index))
		{
			ScriptObject* i = AvmCore::atomToScriptObject(index);
			if (i->traits() == core()->traits.qName_itraits)
			{
				QNameObject* qname = (QNameObject*) i;
				qname->getMultiname(*multi);
			}
			else if(!multi->isRtns() && core()->isDictionary(obj))
			{
				return AvmCore::atomToScriptObject(obj)->getAtomProperty(index);
			}
			else
			{
				multi->setName(core()->intern(index));
			}
		}
		else
		{
			multi->setName(core()->intern(index));
		}

		return toplevel()->getproperty(obj, multi, vtable);
	}

	void MethodEnv::initpropertyHelper(Atom obj, Multiname *multi, Atom value, VTable *vtable, Atom index)
	{
		if ((index&7) == kIntegerType)
		{
			setpropertylate_i(obj, index>>3, value);
			return;
		}

		if ((index&7) == kDoubleType)
		{
			int i = core()->integer(index);
			uint32 u = (uint32)(i);
			if ((double)u == AvmCore::atomToDouble(index))
			{
				setpropertylate_u(obj, u, value);
				return;
			}
			else if ((double)i == AvmCore::atomToDouble(index))
			{
				setpropertylate_i(obj, i, value);
				return;
			}
		}

		if (AvmCore::isObject(index))
		{
			ScriptObject* i = AvmCore::atomToScriptObject(index);
			if (i->traits() == core()->traits.qName_itraits)
			{
				QNameObject* qname = (QNameObject*) i;
				qname->getMultiname(*multi);
			}
			else
			{
				multi->setName(core()->intern(index));
			}
		}
		else
		{
			multi->setName(core()->intern(index));
		}

		initproperty(obj, multi, value, vtable);
	}

	void MethodEnv::setpropertyHelper(Atom obj, Multiname *multi, Atom value, VTable *vtable, Atom index)
	{
		if ((index&7) == kIntegerType)
		{
			setpropertylate_i(obj, index>>3, value);
			return;
		}

		if ((index&7) == kDoubleType)
		{
			int i = core()->integer(index);
			uint32 u = (uint32)(i);
			if ((double)u == AvmCore::atomToDouble(index))
			{
				setpropertylate_u(obj, u, value);
				return;
			}
			else if ((double)i == AvmCore::atomToDouble(index))
			{
				setpropertylate_i(obj, i, value);
				return;
			}
		}

		if (AvmCore::isObject(index))
		{
			ScriptObject* i = AvmCore::atomToScriptObject(index);
			if (i->traits() == core()->traits.qName_itraits)
			{
				QNameObject* qname = (QNameObject*) i;
				qname->getMultiname(*multi);
			}	
			else if(!multi->isRtns() && core()->isDictionary(obj))
			{
				AvmCore::atomToScriptObject(obj)->setAtomProperty(index, value);
				return;
			}
			else
			{
				multi->setName(core()->intern(index));
			}
		}
		else
		{
			multi->setName(core()->intern(index));
		}

		toplevel()->setproperty(obj, multi, value, vtable);
	}
	
	Atom MethodEnv::delpropertyHelper(Atom obj, Multiname *multi, Atom index)
	{
		AvmCore* core = this->core();

		if (AvmCore::isObject(obj) && AvmCore::isObject(index))
		{
            if( core->isXMLList(index) )
            {
                // Error according to E4X spec, section 11.3.1
                toplevel()->throwTypeError(kDeleteTypeError, core->toErrorString(toplevel()->toTraits(index)));
            }
			ScriptObject* i = AvmCore::atomToScriptObject(index);
			if (i->traits() == core->traits.qName_itraits)
			{
				QNameObject* qname = (QNameObject*) i;
				qname->getMultiname(*multi);
			}
			else if(!multi->isRtns() && core->isDictionary(obj))
			{
				bool res = AvmCore::atomToScriptObject(obj)->deleteAtomProperty(index);
				return res ? trueAtom : falseAtom;
			}
			else
			{
				multi->setName(core->intern(index));
			}
		}
		else
		{
			multi->setName(core->intern(index));
		}

		return delproperty(obj, multi);
	}

	void MethodEnv::initMultinameLateForDelete(Multiname& name, Atom index)
	{
		AvmCore *core = this->core();
		
		if (AvmCore::isObject(index))
		{
            if (core->isXMLList(index))
            {
                // Error according to E4X spec, section 11.3.1
                toplevel()->throwTypeError(kDeleteTypeError, core->toErrorString(toplevel()->toTraits(index)));
            }
			
			ScriptObject* i = AvmCore::atomToScriptObject(index);
			if (i->traits() == core->traits.qName_itraits)
			{
				QNameObject* qname = (QNameObject*) i;
				bool attr = name.isAttr();
				qname->getMultiname(name);
				if (attr)
					name.setAttr(attr);
				return;
			}
		}

		name.setName(core->intern(index));
	}		

	ScriptObject* MethodEnv::newcatch(Traits* traits)
	{
		AvmCore* core = this->core();
		Toplevel* toplevel = this->toplevel();
		if (traits == core->traits.object_itraits)
		{
			// don't need temporary vtable.  this is a scope for a finally clause
			// todo: asc shouldn't even call OP_newcatch in a finally clause
			return toplevel->objectClass->construct();
		}
		else
		{
			VTable *vt = core->newVTable(traits, NULL, vtable->scope, vtable->abcEnv, toplevel);
			vt->resolveSignatures();
			return core->newObject(vt, NULL);
		}
	}

	ArrayObject* MethodEnv::createArgumentsHelper(int argc, uint32 *ap)
	{
		// create arguments using argv[1..argc].
		// Even tho E3 says create an Object, E4 says create an Array so thats what we will do.
		AvmAssert(argc >= 0);
		Atom* atomv = (Atom*) alloca((argc+1) * sizeof(Atom));
		method->boxArgs(argc, ap, atomv);
		return createArguments(atomv, argc);
	}

	ArrayObject* MethodEnv::createRestHelper(int argc, uint32 *ap)
	{
		// create rest Array using argv[param_count..argc]
		Atom* extra = (Atom*) (method->restOffset + (char*)ap);
		int extra_count = argc > method->param_count ? argc - method->param_count : 0;
		return toplevel()->arrayClass->newarray(extra, extra_count);
	}

#endif // AVMPLUS_MIR

	Atom MethodEnv::getpropertylate_i(Atom obj, int index) const
	{
		// here we put the case for bind-none, since we know there are no bindings
		// with numeric names.
		if ((obj&7) == kObjectType)
		{
			if (index >= 0)
			{
				// try dynamic lookup on instance.  even if the traits are sealed,
				// we might need to search the prototype chain
				return AvmCore::atomToScriptObject(obj)->getUintProperty(index);
			}
			else
			{
				// negative - we must intern the integer
				return AvmCore::atomToScriptObject(obj)->getAtomProperty(method->core()->internInt(index)->atom());
			}
		}
		else
		{
			// primitive types are not dynamic, so we can go directly
			// to their __proto__ object
			AvmCore* core = method->core();
			Toplevel *toplevel = this->toplevel();
			ScriptObject *protoObject = toplevel->toPrototype(obj);
			return protoObject->ScriptObject::getStringPropertyFromProtoChain(core->internInt(index), protoObject, toplevel->toTraits(obj));			
		}
	}

	Atom MethodEnv::getpropertylate_u(Atom obj, uint32 index) const
	{
		// here we put the case for bind-none, since we know there are no bindings
		// with numeric names.
		if ((obj&7) == kObjectType)
		{
			// try dynamic lookup on instance.  even if the traits are sealed,
			// we might need to search the prototype chain
			return AvmCore::atomToScriptObject(obj)->getUintProperty(index);
		}
		else
		{
			// primitive types are not dynamic, so we can go directly
			// to their __proto__ object
			AvmCore* core = method->core();
			Toplevel *toplevel = this->toplevel();
			ScriptObject *protoObject = toplevel->toPrototype(obj);
			return protoObject->ScriptObject::getStringPropertyFromProtoChain(core->internUint32(index), protoObject, toplevel->toTraits(obj));			
		}
	}

	ScriptObject* MethodEnv::finddef(Multiname* multiname) const
	{
		Toplevel* toplevel = vtable->toplevel;

		ScriptEnv* script = getScriptEnv(multiname);
		if (script == (ScriptEnv*)BIND_AMBIGUOUS)
            toplevel->throwReferenceError(kAmbiguousBindingError, multiname);

		if (script == (ScriptEnv*)BIND_NONE)
            toplevel->throwReferenceError(kUndefinedVarError, multiname);

		ScriptObject* global = script->global;
		if (!global)
		{
			global = script->initGlobal();
			Atom argv[1] = { global->atom() };
			script->coerceEnter(0, argv);
		}
		return global;
	}

	ScriptEnv* MethodEnv::getScriptEnv(Multiname *multiname) const
	{
		ScriptEnv *se = (ScriptEnv*)abcEnv()->domainEnv->getScriptInit(multiname);
		if(!se)
		{	
			// check privates
			se = (ScriptEnv*)abcEnv()->privateScriptEnvs.getMulti(multiname);
		}
		return se;
	}

	ScriptObject* MethodEnv::finddefNsset(NamespaceSet* nsset, Stringp name) const
	{
		Multiname m(nsset);
		m.setName(name);
		return finddef(&m);
	}

	ScriptObject* MethodEnv::finddefNs(Namespace* ns, Stringp name) const
	{
		Multiname m(ns, name);
		return finddef(&m);
	}

	ScriptObject* ScriptEnv::initGlobal()
	{
		// object not defined yet.  define it by running the script that exports it
		Traits* traits = vtable->traits;
		vtable->resolveSignatures();
		NativeScriptInfo* nativeEntry = traits->getNativeScriptInfo();

		Toplevel* toplevel = this->toplevel();
		traits->resolveSignatures(toplevel);
		ScriptObject* delegate = toplevel->objectClass->prototype;

		if (nativeEntry != NULL)
		{
			// special script with native impl object
			return global = nativeEntry->handler(vtable, delegate);
		}
		else
		{
			// ordinary user script
			return global = (ScriptObject*) method->core()->newObject(vtable, delegate);
		}
	}

    ScriptObject* MethodEnv::op_newobject(Atom* sp, int argc) const
    {
		// pre-size the hashtable since we know how many vars are coming
		VTable* object_vtable = toplevel()->object_vtable;
		AvmCore* core = method->core();

		ScriptObject* o = new (core->GetGC(), object_vtable->getExtraSize()) 
			ScriptObject(object_vtable, toplevel()->objectClass->prototype,
					2*argc+1);

		for (; argc-- > 0; sp -= 2)
		{
			Atom name = sp[-1];
			//verifier should ensure names are String
			//todo have the verifier take care of interning too
			AvmAssert(AvmCore::isString(name));

			o->setAtomProperty(core->internString(name)->atom(), sp[0]);
		}
#ifdef DEBUGGER
		if( core->allocationTracking )
		{
			toplevel()->objectClass->addInstance(o->atom());
		}
#endif
		return o;
    }



	Atom MethodEnv::nextname(Atom objAtom, int index) const
	{
		if (index <= 0)
			return nullStringAtom;

		switch (objAtom&7)
		{
		case kObjectType:
			return AvmCore::atomToScriptObject(objAtom)->nextName(index);
		case kNamespaceType:
			return AvmCore::atomToNamespace(objAtom)->nextName(method->core(), index);
		default:
			ScriptObject* proto = toplevel()->toPrototype(objAtom);  // cn: types like Number are sealed, but their prototype could have dynamic properties
			return proto ? proto->nextName(index) : undefinedAtom;
		}
	}

	Atom MethodEnv::nextvalue(Atom objAtom, int index) const
	{
		if (index <= 0)
			return undefinedAtom;

		switch (objAtom&7)
		{
		case kObjectType:
			return AvmCore::atomToScriptObject(objAtom)->nextValue(index);
		case kNamespaceType:
			return AvmCore::atomToNamespace(objAtom)->nextValue(index);
		default:
			ScriptObject*  proto = toplevel()->toPrototype(objAtom);
			return (proto ? proto->nextValue(index) : undefinedAtom);
		}
	}

	int MethodEnv::hasnext(Atom objAtom, int index) const
	{
		if (index < 0)
			return 0;

		if (!AvmCore::isNullOrUndefined(objAtom))
		{
			switch (objAtom&7)
			{
			case kObjectType:
				return AvmCore::atomToScriptObject(objAtom)->nextNameIndex(index);
			case kNamespaceType:
				return AvmCore::atomToNamespace(objAtom)->nextNameIndex(index);
			default:
				ScriptObject* proto = toplevel()->toPrototype(objAtom);
				int nextIndex = ( proto ? proto->nextNameIndex(index) : 0);
				return nextIndex;
			}
		}
		else
		{
			return 0;
		}
	}

	int MethodEnv::hasnext2(Atom& objAtom, int& index) const
	{
		if (index < 0)
			return 0;

		ScriptObject *delegate = NULL;
		
		if (!AvmCore::isNullOrUndefined(objAtom))
		{
			switch (objAtom&7)
			{
			case kObjectType:
				{
					ScriptObject *object = AvmCore::atomToScriptObject(objAtom);
					delegate = object->getDelegate();
					index = object->nextNameIndex(index);
				}
				break;
			case kNamespaceType:
				{
					index = AvmCore::atomToNamespace(objAtom)->nextNameIndex(index);
					delegate = toplevel()->namespaceClass->prototype;
				}
				break;
			default:
				{
					ScriptObject* proto = toplevel()->toPrototype(objAtom);
					delegate = proto ? proto->getDelegate() : NULL;
					index = ( proto ? proto->nextNameIndex(index) : 0);
				}
			}
		}
		else
		{
			index = 0;
		}

		while (index == 0 && delegate != NULL)
		{
			// Advance to next object on prototype chain
			ScriptObject *object = delegate;
			objAtom = object->atom();
			delegate = object->getDelegate();
			index = object->nextNameIndex(index);
		}

		if (index == 0)
		{
			// If we're done, set object local to null
			objAtom = nullObjectAtom;
		}

		return index != 0;
	}
	
	Atom MethodEnv::in(Atom nameatom, Atom obj) const
	{
		AvmCore* core = method->core();
		Traits *t = toplevel()->toTraits(obj); // includes null check

		if(!core->isDictionaryLookup(nameatom, obj))
		{
			Stringp name = core->intern(nameatom);

			// ISSUE should we try this on each object on the proto chain or just the first?
			if (t->findBinding(name, core->publicNamespace) != BIND_NONE)
			{
				return trueAtom;
			}

			nameatom = name->atom();
		}

		ScriptObject* o = (obj&7)==kObjectType ? AvmCore::atomToScriptObject(obj) : 
				toplevel()->toPrototype(obj);
		do
		{
			if (o->hasAtomProperty(nameatom))
				return trueAtom;
		}
		while ((o = o->getDelegate()) != NULL);
		return falseAtom;
	}

	// see 13.2 creating function objects
    ClassClosure* MethodEnv::newfunction(AbstractFunction *function,
									 ScopeChain* outer,
									 Atom* scopes) const
    {
		AvmCore* core = this->core();
		AbcEnv* abcEnv = vtable->abcEnv;

		// TODO: if we have already created a function and the scope chain
		// is the same as last time, re-use the old closure?

		// declaringTraits must have been filled in by verifier.
		Traits* ftraits = function->declaringTraits;
		AvmAssert(ftraits != NULL);
		AvmAssert(ftraits->scope != NULL);

		ScopeChain* scope = ScopeChain::create(core->GetGC(), ftraits->scope, outer, *core->dxnsAddr);

		for (int i=outer->getSize(), n=scope->getSize(); i < n; i++)
		{
			scope->setScope(i, *scopes++);
		}

		FunctionClass* functionClass = toplevel()->functionClass;

		// the vtable for the new function object
		VTable* fvtable = core->newVTable(ftraits, functionClass->ivtable(), scope, abcEnv, toplevel());
		fvtable->resolveSignatures();
		FunctionEnv *fenv = new (core->GetGC()) FunctionEnv(function, fvtable);
		fvtable->call = fenv;
		fvtable->ivtable = toplevel()->object_vtable;

		ClassClosure* c = new (core->GetGC(), fvtable->getExtraSize()) ClassClosure(fvtable);
		c->setDelegate( functionClass->prototype );

		c->createVanillaPrototype();
		c->prototype->setStringProperty(core->kconstructor, c->atom());
		c->prototype->setStringPropertyIsEnumerable(core->kconstructor, false);

		fenv->closure = c;
		
        return c;
    }

    /**
     * given a classInfo, create a new ClassClosure object and return it on the stack.
     */

	ClassClosure* MethodEnv::newclass(AbstractFunction* cinit,
							ClassClosure *base,
							ScopeChain* outer,
							Atom* scopes) const
    {
		AvmCore* core = this->core();
		Toplevel* toplevel = this->toplevel();

		Traits* ctraits = cinit->declaringTraits;
		Traits* itraits = ctraits->itraits;

		// finish resolving the base class
		if (!base && itraits->base)
		{
			// class has a base but no base object was provided
			ErrorClass *error = toplevel->typeErrorClass();
			if( error )
				error->throwError(kConvertNullToObjectError);
			else
				toplevel->throwTypeError(kCorruptABCError);
		}

		// make sure the traits of the base vtable matches the base traits
		if (!(base == NULL && itraits->base == NULL || base != NULL && itraits->base == base->ivtable()->traits))
		{
			ErrorClass *error = toplevel->verifyErrorClass();
			if( error )
				error->throwError(kInvalidBaseClassError);
			else
				toplevel->throwTypeError(kCorruptABCError);
		}

		ctraits->resolveSignatures(toplevel);
		itraits->resolveSignatures(toplevel);

		// class scopechain = [..., class]
		ScopeChain* cscope = ScopeChain::create(core->GetGC(), ctraits->scope, outer, *core->dxnsAddr);

		int staticScopesCount = 0;

		int i = outer->getSize();
		for (int n=cscope->getSize()-staticScopesCount; i < n; i++)
		{
			cscope->setScope(i, *scopes++);
		}

		ScopeChain* iscope = ScopeChain::create(core->GetGC(), itraits->scope, cscope, *core->dxnsAddr);

		AbcEnv *abcEnv = vtable->abcEnv;
		VTable* cvtable = core->newVTable(ctraits, toplevel->class_vtable, cscope, abcEnv, toplevel);
		cvtable->resolveSignatures();
		VTable* ivtable = core->newVTable(itraits, base ? base->ivtable() : NULL, iscope, abcEnv, toplevel);
		ivtable->resolveSignatures();
		cvtable->ivtable = ivtable;

		if (itraits == core->traits.object_itraits) {
			// we just defined Object
			toplevel->object_vtable = ivtable;
		}
		else if (itraits == core->traits.class_itraits) {
			// we just defined Class
			toplevel->class_vtable = ivtable;
			cvtable->base = ivtable;
			toplevel->objectClass->vtable->base = ivtable;
		}

		NativeClassInfo* nativeEntry;
		ClassClosure *cc;
		if ((nativeEntry = cvtable->traits->getNativeClassInfo()) != NULL)
		{
			cc = nativeEntry->handler(cvtable);
		}
		else
		{
			cc = new (core->GetGC(), cvtable->getExtraSize()) ClassClosure(cvtable);
			AvmAssert(cc->prototype == NULL);
			cc->createVanillaPrototype();
		}

		if (cc->prototype)
		{
			// C.prototype.__proto__ = Base.prototype
			if (base != NULL) 
				cc->prototype->setDelegate( base->prototype );

			// C.prototype.constructor = C {DontEnum}
			cc->prototype->setStringProperty(core->kconstructor, cc->atom());
			cc->prototype->setStringPropertyIsEnumerable(core->kconstructor, false);
		}

		AvmAssert(i == iscope->getSize()-1);
		iscope->setScope(i, cc->atom());
		if (toplevel->classClass)
		{
			cc->setDelegate( toplevel->classClass->prototype );
		}

#ifdef DEBUGGER
		if(toplevel->classClass && core->allocationTracking)
		{
			toplevel->classClass->addInstance(cc->atom());
		}
#endif

		// Invoke the class init function.
		Atom argv[1] = { cc->atom() };
		cvtable->init->coerceEnter(0, argv);
		return cc;
    }

    void MethodEnv::initproperty(Atom obj, Multiname* multiname, Atom value, VTable* vtable) const
    {
		Toplevel* toplevel = this->toplevel();
		Binding b = toplevel->getBinding(vtable->traits, multiname);
		if ((b&7) == BIND_CONST)
		{
			if (vtable->init != this)
				toplevel->throwReferenceError(kConstWriteError, multiname, vtable->traits);
			b = (b & ~7) | BIND_VAR;
		}
		toplevel->setproperty_b(obj, multiname, value, vtable, b);
    }

	void MethodEnv::setpropertylate_i(Atom obj, int index, Atom value) const
	{
		if (AvmCore::isObject(obj))
		{
			ScriptObject* o = AvmCore::atomToScriptObject(obj);
			if (index >= 0)
			{
				o->setUintProperty(index, value);
			}
			else
			{
				// negative index - we must intern the integer
				o->setAtomProperty(method->core()->internInt(index)->atom(), value);
			}
		}
		else
		{
			// obj represents a primitive Number, Boolean, int, or String, and primitives
			// are sealed and final.  Cannot add dynamic vars to them.

			// throw a ReferenceError exception  Property not found and could not be created.
			Multiname tempname(core()->publicNamespace, core()->internInt(index));
			toplevel()->throwReferenceError(kWriteSealedError, &tempname, toplevel()->toTraits(obj));
		}
	}

	void MethodEnv::setpropertylate_u(Atom obj, uint32 index, Atom value) const
	{
		if (AvmCore::isObject(obj))
		{
			AvmCore::atomToScriptObject(obj)->setUintProperty(index, value);
		}
		else
		{
			// obj represents a primitive Number, Boolean, int, or String, and primitives
			// are sealed and final.  Cannot add dynamic vars to them.

			// throw a ReferenceError exception  Property not found and could not be created.
			Multiname tempname(core()->publicNamespace, core()->internUint32(index));
			toplevel()->throwReferenceError(kWriteSealedError, &tempname, toplevel()->toTraits(obj));
		}
	}

	Atom MethodEnv::callsuper(Multiname* multiname, int argc, Atom* atomv) const
	{
		VTable* base = vtable->base;
		Toplevel* toplevel = this->toplevel();
		Binding b = toplevel->getBinding(base->traits, multiname);
		switch (b&7)
		{
		default:
			toplevel->throwReferenceError(kCallNotFoundError, multiname, base->traits);

		case BIND_METHOD:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callsuper method " << base->traits << " " << multiname->name << "\n";
			#endif
			MethodEnv* superenv = base->methods[AvmCore::bindingToMethodId(b)];
			return superenv->coerceEnter(argc, atomv);
		}
		case BIND_VAR:
		case BIND_CONST:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callsuper slot " << base->traits << " " << multiname->name << "\n";
			#endif
			uint32 slot = AvmCore::bindingToSlotId(b);
			Atom method = AvmCore::atomToScriptObject(atomv[0])->getSlotAtom(slot);
			return toplevel->op_call(method, argc, atomv);
		}
		case BIND_SET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callsuper setter " << base->traits << " " << multiname->name << "\n";
			#endif
			// read on write-only property
			toplevel->throwReferenceError(kWriteOnlyError, multiname, base->traits);
		}
		case BIND_GET:
		case BIND_GETSET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callsuper getter " << base->traits << " " << multiname->name << "\n";
			#endif
			// Invoke the getter
			int m = AvmCore::bindingToGetterId(b);
			MethodEnv *f = base->methods[m];
			Atom atomv_out[1] = { atomv[0] };
			Atom method = f->coerceEnter(0, atomv_out);
			return toplevel->op_call(method, argc, atomv);
		}
		}
	}

	Atom MethodEnv::delproperty(Atom obj, Multiname* multiname) const
	{
		Toplevel* toplevel = this->toplevel();
		Traits* traits = toplevel->toTraits(obj); // includes null check
		if (AvmCore::isObject(obj))
		{
			Binding b = toplevel->getBinding(traits, multiname);
			if (b == BIND_NONE) 
			{
				bool b = AvmCore::atomToScriptObject(obj)->deleteMultinameProperty(multiname);
				return b ? trueAtom : falseAtom;
			}
			else if (AvmCore::isMethodBinding(b))
			{
				if (multiname->contains(core()->publicNamespace) && toplevel->isXmlBase(obj))
				{
					// dynamic props should hide declared methods on delete
					ScriptObject* so = AvmCore::atomToScriptObject(obj);
					bool b = so->deleteMultinameProperty(multiname);
					return b ? trueAtom : falseAtom;
				}
			}
		}
		else
		{
			toplevel->throwReferenceError(kDeleteSealedError, multiname, traits);
		}

		return falseAtom;
	}
	
    Atom MethodEnv::getsuper(Atom obj, Multiname* multiname) const
    {
		VTable* vtable = this->vtable->base;
		Toplevel* toplevel = this->toplevel();
		Binding b = toplevel->getBinding(vtable->traits, multiname);
        switch (b&7)
        {
		default:
			toplevel->throwReferenceError(kReadSealedError, multiname, vtable->traits);

		case BIND_METHOD: 
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getsuper method " << vtable->traits << " " << multiname->name << "\n";
			#endif
			// extracting a virtual method
			MethodEnv *m = vtable->methods[AvmCore::bindingToMethodId(b)];
			return toplevel->methodClosureClass->create(m, obj)->atom();
		}

        case BIND_VAR:
        case BIND_CONST:
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getsuper slot " << vtable->traits << " " << multiname->name << "\n";
			#endif
			return AvmCore::atomToScriptObject(obj)->getSlotAtom(AvmCore::bindingToSlotId(b));

		case BIND_SET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getsuper setter " << vtable->traits << " " << multiname->name << "\n";
			#endif
			// read on write-only property
			toplevel->throwReferenceError(kWriteOnlyError, multiname, vtable->traits);
		}
		case BIND_GET:
		case BIND_GETSET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getsuper getter " << vtable->traits << " " << multiname->name << "\n";
			#endif
			// Invoke the getter
			int m = AvmCore::bindingToGetterId(b);
			MethodEnv *f = vtable->methods[m];
			Atom atomv_out[1] = { obj };
			return f->coerceEnter(0, atomv_out);
		}
		}
    }

	
    void MethodEnv::setsuper(Atom obj, Multiname* multiname, Atom value) const
    {
		VTable* vtable = this->vtable->base;
		Toplevel* toplevel = this->toplevel();
		Binding b = toplevel->getBinding(vtable->traits, multiname);
        switch (b&7)
        {
		default:
			toplevel->throwReferenceError(kWriteSealedError, multiname, vtable->traits);

		case BIND_CONST:
			toplevel->throwReferenceError(kConstWriteError, multiname, vtable->traits);

		case BIND_METHOD: 
			toplevel->throwReferenceError(kCannotAssignToMethodError, multiname, vtable->traits);

		case BIND_GET: 
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setsuper getter " << vtable->traits << " " << multiname->name << "\n";
			#endif
			toplevel->throwReferenceError(kConstWriteError, multiname, vtable->traits);

		case BIND_VAR:
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setsuper slot " << vtable->traits << " " << multiname->name << "\n";
			#endif
			AvmCore::atomToScriptObject(obj)->setSlotAtom(AvmCore::bindingToSlotId(b), value);
            return;

		case BIND_SET: 
		case BIND_GETSET: 
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setsuper setter " << vtable->traits << " " << multiname->name << "\n";
			#endif
			// Invoke the setter
			uint32 m = AvmCore::bindingToSetterId(b);
			AvmAssert(m < vtable->traits->methodCount);
			MethodEnv* method = vtable->methods[m];
			Atom atomv_out[2] = { obj, value };
			// coerce value to proper type, then call enter
			method->coerceEnter(1, atomv_out);
			return;
		}
        }
    }

	Atom MethodEnv::findWithProperty(Atom atom, Multiname* multiname)
	{
		Toplevel* toplevel = this->toplevel();
		if ((atom&7)==kObjectType)
		{
			// usually scope objects are ScriptObject's

			ScriptObject* o = AvmCore::atomToScriptObject(atom);

			// search the delegate chain for a value.  we must look at
			// traits at each step along the way in case we have class
			// instances on the scope chain
			do
			{
				// ISSUE it is incorrect to return a reference to an object on the prototype
				// chain, we should only return the scopechain object; the next operation
				// could be a setproperty, and we don't want to mutate prototype objects this way.

				// look at the traits first, stop if found.
				Binding b = toplevel->getBinding(o->traits(), multiname);
				if (b != BIND_NONE)
					return atom;
				if (o->hasMultinameProperty(multiname))
					return atom;
			}
			while ((o = o->getDelegate()) != NULL);
		}
		else
		{
			// primitive value on scope chain!

			// first iteration looks at traits only since primitive values don't have
			// dynamic variables

			Binding b = toplevel->getBinding(toplevel->toTraits(atom), multiname);
			if (b != BIND_NONE)
				return atom;

			// then we continue starting at the prototype for this primitive type
			ScriptObject* o = toplevel->toPrototype(atom);
			do
			{
				Binding b = toplevel->getBinding(o->traits(), multiname);
				if (b != BIND_NONE)
					return atom;
				if (o->hasMultinameProperty(multiname))
					return atom;
			}
			while ((o = o->getDelegate()) != NULL);
		}

		return nullObjectAtom;
	}

	/**
	 * return the object on the scope chain that owns the given property.
	 * if no object has that property, return scope[0].  we search each
	 * delegate chain but only return objects that are on the scope chain.
	 * this way, find+get and find+set are both correct.  find+set should
	 * not mutate a prototype object.
	 */
	Atom MethodEnv::findproperty(ScopeChain* outer,
								 Atom* scopes,
								 int extraScopes,
								 Multiname* multiname,
								 bool strict,
								 Atom* withBase)
    {
		Toplevel* toplevel = this->toplevel();

		// look in each with frame in the current stack frame
		Atom* scopep = &scopes[extraScopes-1];
		if (withBase)
		{
			for (; scopep >= withBase; scopep--)
			{
				Atom result = findWithProperty(*scopep, multiname);
				if (!AvmCore::isNull(result))
				{
					return result;
				}
			}
		}

		// if we're in global$init (outer_depth==0), don't check "this" scope just yet.
		int outer_depth = outer->getSize();
		for (; scopep > scopes; scopep--)
		{
			Atom a = *scopep;
			Traits* t = toplevel->toTraits(a);
			Binding b = toplevel->getBinding(t, multiname);
			if (b != BIND_NONE)
				return a;
		}

		ScopeTypeChain* outerTraits = outer->scopeTraits;

		if (outer_depth > 0 && scopep >= scopes)
		{
			// consider "this" scope now, but constrain it to the declaringTraits of
			// the current method (verifier ensures this is safe)
			Atom a = *scopep;
			Traits *t;
			if (outerTraits->fullsize > outerTraits->size)
			{
				// scope traits has extra constraint for "this" scope, see OP_newclass in verifier
				t = outerTraits->scopes[outerTraits->size].traits;
			}
			else
			{
				// "this" scope type is the runtime type
				t = toplevel->toTraits(a);
			}

			Binding b = toplevel->getBinding(t, multiname);
			if (b != BIND_NONE)
				return a;
		}

		// now search outer scopes
		for (int i=outer_depth-1; i > 0; i--)
		{
			if (outerTraits->scopes[i].isWith)
			{
				Atom result = findWithProperty(outer->getScope(i), multiname);
				if (!AvmCore::isNull(result))
					return result;
			}
			else
			{
				// only look at the properties on the captured (verify time) type, not the actual type,
				// of the outer scope object.
				Atom a = outer->getScope(i);
				Traits* t = outerTraits->scopes[i].traits;
				Binding b = toplevel->getBinding(t, multiname);
				if (b != BIND_NONE)
					return a;
			}
		}

		// No imported definitions or global scope can have attributes.  (Using filter
		// operator with non existent attribute will get here. 
		if (multiname->isAttr())
		{
			if (strict)
				toplevel->throwReferenceError(kUndefinedVarError, multiname);
			return undefinedAtom;
		}

		// now we have searched all the scopes, except global
		
		// look for imported definition (similar logic to OP_finddef).  This will
		// find definitions in this script and in other scripts.
		ScriptEnv* script = getScriptEnv(multiname);
		if (script != (ScriptEnv*)BIND_NONE)
		{
			if (script == (ScriptEnv*)BIND_AMBIGUOUS)
				toplevel->throwReferenceError(kAmbiguousBindingError, multiname);

			ScriptObject* global = script->global;
			if (global == NULL)
			{
				global = script->initGlobal();
				Atom argv[1] = { script->global->atom() };
				script->coerceEnter(0, argv);
			}
			return global->atom();
		}

		// no imported definition found.  look for dynamic props
		// on the global object

		ScriptObject* global = AvmCore::atomToScriptObject(
			outer_depth > 0 ? outer->getScope(0) : *scopes
		);

		// search the delegate chain for a value.  The delegate
		// chain for the global object will only contain vanilla
		// objects (Object.prototype) so we can skip traits

		ScriptObject* o = global;
		do
		{
			if (o->hasMultinameProperty(multiname))
				return global->atom();
		}
		while ((o = o->getDelegate()) != NULL);

		// If a variable cannot be found
		// throw reference error if strict

		if (strict)
			toplevel->throwReferenceError(kUndefinedVarError, multiname);

		return global->atom();
    }

	Namespace* MethodEnv::internRtns(Atom nsAtom)
	{
		if ((nsAtom&7) != kNamespaceType)
			toplevel()->throwTypeError(kIllegalNamespaceError);
		return core()->internNamespace(AvmCore::atomToNamespace(nsAtom));
	}

	ArrayObject* MethodEnv::createArguments(Atom *atomv, int argc)
	{
		Toplevel* toplevel = this->toplevel();
		ArrayObject *arguments = toplevel->arrayClass->newarray(atomv+1,argc);
		ScriptObject *closure;
		if (method->flags & AbstractFunction::NEED_CLOSURE)
		{
			closure = toplevel->methodClosureClass->create(this, atomv[0]);
		}
		else
		{
			closure = ((FunctionEnv*)this)->closure;
		}
		arguments->setStringProperty(core()->kcallee, closure->atom());
		arguments->setStringPropertyIsEnumerable(core()->kcallee, false);
		return arguments;
	}

	Atom MethodEnv::getdescendants(Atom obj, Multiname* multiname)
	{
		if (AvmCore::isObject (obj))
		{
			return core()->atomToScriptObject(obj)->getDescendants (multiname);
		}
		else
		{
			// Rhino simply returns undefined for other Atom types
			// SpiderMonkey throws TypeError.  We're doing TypeError.
			toplevel()->throwTypeError(kDescendentsError, core()->toErrorString(toplevel()->toTraits(obj)));
			return undefinedAtom; // not reached
		}
	}

	Atom MethodEnv::getdescendantslate(Atom obj, Atom index, bool attr)
	{
		if (AvmCore::isObject(index))
		{
			ScriptObject* i = AvmCore::atomToScriptObject(index);
			if (i->traits() == core()->traits.qName_itraits)
			{
				QNameObject* qname = (QNameObject*) i;
				Multiname n2;
				qname->getMultiname(n2);
				if (attr)
					n2.setAttr(attr);
				return getdescendants(obj, &n2);
			}
		}

		// convert index to string

		AvmCore* core = this->core();
		Multiname name(core->publicNamespace, core->intern(index));
		if (attr)
			name.setAttr();
		return getdescendants(obj, &name);
	}

	void MethodEnv::checkfilter(Atom obj)
	{
		if ( !toplevel()->isXmlBase(obj) )
		{
			toplevel()->throwTypeError(kFilterError, core()->toErrorString(toplevel()->toTraits(obj)));
		}
	}

		
	/**
	 * implements ECMA implicit coersion.  returns the coerced value,
	 * or throws a TypeError if coersion is not possible.
	 */
    ScriptObject* MethodEnv::coerceAtom2SO(Atom atom, Traits* expected) const
    {
		AvmAssert(expected != NULL);
		AvmAssert(!expected->isMachineType);
		AvmAssert(expected != core()->traits.string_itraits);
		AvmAssert(expected != core()->traits.namespace_itraits);

		if (AvmCore::isNullOrUndefined(atom))
			return NULL;

		ScriptObject *so;
		if ((atom&7) == kObjectType &&
			(so=AvmCore::atomToScriptObject(atom))->traits()->containsInterface(expected))
		{
			return so;
		}
		else
		{
			// failed
#ifdef AVMPLUS_VERBOSE
			//core->console << "checktype failed " << expected << " <- " << atom << "\n";
#endif
			toplevel()->throwTypeError(kCheckTypeFailedError, core()->atomToErrorString(atom), core()->toErrorString(expected));
			return NULL;
		}
    }

	/**
	 * implements ECMA as operator.  Returns the same value, or null.
	 */
    Atom MethodEnv::astype(Atom atom, Traits* expected)
    {
		return core()->istype(atom, expected) ? atom : nullObjectAtom;
	}

	VTable *MethodEnv::getActivation()
	{
		int type = getType();
		switch(type)
		{
		case kActivation:
			return (VTable *)(activationOrMCTable&~3);
		case kActivationMethodTablePair:
			return getPair()->activation;
		default:
			return NULL;
		}
	}

	WeakKeyHashtable *MethodEnv::getMethodClosureTable()
	{
		int type = getType();
		if(!activationOrMCTable)
		{
			WeakKeyHashtable *wkh = new (core()->GetGC()) WeakKeyHashtable(core()->GetGC());			
			setActivationOrMCTable(wkh, kMethodTable);
			return wkh;
		}
		else if(type == kActivation)
		{
			WeakKeyHashtable *wkh = new (core()->GetGC()) WeakKeyHashtable(core()->GetGC());
			ActivationMethodTablePair *amtp = new (core()->GetGC()) ActivationMethodTablePair(getActivation(), wkh);					
			setActivationOrMCTable(amtp, kActivationMethodTablePair);
			return wkh;
		}
		else if(type == kActivationMethodTablePair)
		{
			return getPair()->methodTable;
		}
		return (WeakKeyHashtable*)(activationOrMCTable&~3);
	}
}
