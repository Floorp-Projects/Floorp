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
	#ifdef DEBUGGER
	// helper object to store class instances
	class GCHashtableScriptObject : public ScriptObject
	{
	public:
		GCHashtableScriptObject(VTable *vtable) : ScriptObject(vtable, vtable->toplevel->objectClass->prototype) {}	
		
		int nextNameIndex(int index) 
		{ 
			return ht.nextIndex(index); 
		}
		
		Atom nextValue(int index) 
		{ 
			Atom a = (Atom)ht.keyAt(index-1);
			return a;
		}
		
		// so we don't count these
		ClassClosure* cc() const { return NULL; }

		MMgc::GCHashtable ht;
	};

	class SlotIterator : public ScriptObject
	{
	public:
		SlotIterator(ScriptObject *obj, VTable *vtable)
			: obj(obj),
			ScriptObject(vtable, vtable->toplevel->objectClass->prototype) {}

		int nextNameIndex(int index)
		{
			if(index == 0)
				currTraits = obj->traits();

			while (currTraits != NULL)
			{
				while ((index = currTraits->next(index)) != 0)
				{
					if (AvmCore::isSlotBinding(currTraits->valueAt(index)))
						return index;
				}

				currTraits = currTraits->base;
			}

			return 0;
		}

		Atom nextValue(int index)
		{
			Multiname mn(currTraits->nsAt(index), currTraits->keyAt(index), true);
			QNameObject *qname = new (gc(), toplevel()->qnameClass()->ivtable()->getExtraSize())
									QNameObject(toplevel()->qnameClass(), mn);

			return qname->atom();
		}

	private:
		DRCWB(ScriptObject *) obj;
		DWB(Traits *) currTraits;
	};
	#endif

	ScriptObject::ScriptObject(VTable *vtable,
							   ScriptObject *delegate,
	                           int capacity /*=Hashtable::kDefaultCapacity*/)
		: vtable(vtable)
	{
		// initialize slots in this object to initial values from traits.
		Traits* traits = vtable->traits;
		AvmAssert(traits->linked);

		// Ensure that our object is large enough to hold its extra traits data.
		AvmAssert(MMgc::GC::Size(this) >= traits->getTotalSize());

 		setDelegate(delegate);

		if (traits->needsHashtable)
		{
			MMGC_MEM_TYPE(this);
			getTable()->initialize(this->gc(), capacity);
			getTable()->setDontEnumSupport(true);
		}

#if 0
		if (vtable->toplevel && vtable->toplevel->scriptObjectTable)
		{
			vtable->toplevel->scriptObjectTable->addScriptObject(this);
		}
#endif
	}

	ScriptObject::~ScriptObject()
	{
#ifdef MMGC_DRC
		setDelegate(NULL);
		vtable->traits->destroyInstance(this);
#endif
	}
	
    /**
     * traverse the delegate chain looking for a value.
     * [ed] it's okay to look only at the HT's in the delegate chain because
     * delegate values may only be instances of Object.  They cannot be objects
     * with slots.  We don't need to look at traits at each step.
     * todo - enforce this rule
     * @param name
     * @return
     */
	Atom ScriptObject::getAtomProperty(Atom name) const
	{
		if (!traits()->needsHashtable)
		{
			return getAtomPropertyFromProtoChain(name, delegate, traits());
		}
		else
		{
			Stringp s = core()->atomToString(name);
			AvmAssert(s->isInterned());
			Atom ival = s->getIntAtom();
			if (ival)
			{
				name = ival;
			}

			// dynamic lookup on this object
			const ScriptObject *o = this;
			do
			{
				Hashtable *table = o->getTable();
				const Atom *atoms = table->getAtoms();
				int i = table->find(name, atoms, table->getNumAtoms());
				if (atoms[i] != Hashtable::EMPTY)
					return atoms[i+1];
			}
			while ((o = o->delegate) != NULL);
			return undefinedAtom;
		}			
	}
	
	Atom ScriptObject::getAtomPropertyFromProtoChain(Atom name, ScriptObject* o, Traits *origObjTraits) const
	{
        // todo will delegate always be non-null here?
		if (o != NULL)
		{
			Atom searchname = name;
			Stringp s = core()->atomToString(name);
			AvmAssert(s->isInterned());
			Atom ival = s->getIntAtom();
			if (ival)
			{
				searchname = ival;
			}
			do
			{
				Atom *atoms = o->getTable()->getAtoms();
				int i = o->getTable()->find(searchname, atoms, o->getTable()->getNumAtoms());
				if (atoms[i] != Hashtable::EMPTY)
					return atoms[i+1];
			}
			while ((o = o->delegate) != NULL);
		}
		Multiname multiname(core()->publicNamespace,AvmCore::atomToString(name));
		toplevel()->throwReferenceError(kReadSealedError, &multiname, origObjTraits);
		// unreached
		return undefinedAtom;
	}

	bool ScriptObject::hasMultinameProperty(Multiname* multiname) const
	{
		if (traits()->needsHashtable)
		{
			if (isValidDynamicName(multiname))
			{
				return hasAtomProperty(multiname->getName()->atom());
			}
			else
			{
				return false;
			}
		}
		else
		{
			// ISSUE should this walk the proto chain?
			return false;
		}
	}

	bool ScriptObject::hasAtomProperty(Atom name) const
	{
		if (traits()->needsHashtable)
		{
			Stringp s = core()->atomToString(name);
			AvmAssert(s->isInterned());
			Atom ival = s->getIntAtom();
			if (ival)
			{
				name = ival;
			}

			return getTable()->contains(name);
		}
		else
		{
			// ISSUE should this walk the proto chain?
			return false;
		}
	}

    void ScriptObject::setAtomProperty(Atom name, Atom value)
    {
		if (traits()->needsHashtable)
		{
			Stringp s = core()->atomToString(name);
			AvmAssert(s->isInterned());
			Atom ival = s->getIntAtom();
			if (ival)
			{
				name = ival;
			}

			MMGC_MEM_TYPE(this);
			getTable()->add (name, value);
			MMGC_MEM_TYPE(NULL);
		}
		else
		{
			Multiname multiname(core()->publicNamespace, AvmCore::atomToString(name));
			// cannot create properties on a sealed object.
			toplevel()->throwReferenceError(kWriteSealedError, &multiname, traits());
		}
    }

	void ScriptObject::setMultinameProperty(Multiname* name, Atom value)
	{
		if (traits()->needsHashtable && isValidDynamicName(name))
		{
			setStringProperty(name->getName(), value);
		}
		else
		{
			// cannot create properties on a sealed object.
			toplevel()->throwReferenceError(kWriteSealedError, name, traits());
		}
	}

	bool ScriptObject::getAtomPropertyIsEnumerable(Atom name) const
	{
		if (traits()->needsHashtable)
		{
			Stringp s = core()->atomToString(name);
			AvmAssert(s->isInterned());
			Atom ival = s->getIntAtom();
			if (ival)
			{
				name = ival;
			}

			return getTable()->getAtomPropertyIsEnumerable(name);
		}
		else
		{
			// ISSUE should this walk the proto chain?
			return false;
		}
	}

	void ScriptObject::setAtomPropertyIsEnumerable(Atom name, bool enumerable)
	{
		if (traits()->needsHashtable)
		{
			Stringp s = core()->atomToString(name);
			AvmAssert(s->isInterned());
			Atom ival = s->getIntAtom();
			if (ival)
			{
				name = ival;
			}

			getTable()->setAtomPropertyIsEnumerable(name, enumerable);
		}
		else
		{
			// cannot create properties on a sealed object.
			Multiname multiname(core()->publicNamespace, AvmCore::atomToString(name));
			toplevel()->throwReferenceError(kWriteSealedError, &multiname, traits());
		}
	}
	
	bool ScriptObject::deleteAtomProperty(Atom name)
	{
		if (traits()->needsHashtable)
		{
			Stringp s = core()->atomToString(name);
			AvmAssert(s->isInterned());
			Atom ival = s->getIntAtom();
			if (ival)
			{
				name = ival;
			}

			getTable()->remove(name);
			return true;
		}
		else
		{
			return false;
		}
	}
	
	bool ScriptObject::deleteMultinameProperty(Multiname* name)
	{
		if (traits()->needsHashtable && isValidDynamicName(name))
		{
			return deleteStringProperty(name->getName());
		}
		else
		{
			return false;
		}
	}

	Atom ScriptObject::getUintProperty(uint32 i) const
	{
		AvmCore* core = this->core();

		if (!(i&MAX_INTEGER_MASK))
		{
			if (!traits()->needsHashtable)
			{
				Atom name = core->internUint32(i)->atom();
				return getAtomPropertyFromProtoChain(name, delegate, traits());
			}
			else
			{
				// dynamic lookup on this object
				Atom name = core->uintToAtom (i);
				const ScriptObject *o = this;
				do
				{
					Hashtable *table = o->getTable();
					const Atom *atoms = table->getAtoms();
					int i = table->find(name, atoms, table->getNumAtoms());
					if (atoms[i] != Hashtable::EMPTY)
						return atoms[i+1];
				}
				while ((o = o->delegate) != NULL);
				return undefinedAtom;
			}			
		}
		else
		{
			return getAtomProperty(core->internUint32(i)->atom());
		}		
	}

	void ScriptObject::setUintProperty(uint32 i, Atom value)
	{
		AvmCore* core = this->core();
		if (!(i&MAX_INTEGER_MASK)) 
		{
			Atom name = core->uintToAtom (i);
			if (traits()->needsHashtable)
			{
				MMGC_MEM_TYPE(this);
				getTable()->add(name, value);
				MMGC_MEM_TYPE(NULL);
			}
			else
			{
				Multiname multiname(core->publicNamespace, core->internUint32(i));
				// cannot create properties on a sealed object.
				toplevel()->throwReferenceError(kWriteSealedError, &multiname, traits());
			}
		}
		else
		{
			setAtomProperty(core->internUint32(i)->atom(), value);
		}
	}

	bool ScriptObject::delUintProperty(uint32 i)
	{
		AvmCore* core = this->core();
		if (!(i&MAX_INTEGER_MASK)) 
		{
			Atom name = core->uintToAtom (i);
			if (traits()->needsHashtable)
			{
				getTable()->remove(name);
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return deleteAtomProperty(core->internUint32(i)->atom());
		}
	}

	bool ScriptObject::hasUintProperty(uint32 i) const
	{
		AvmCore* core = this->core();
		if (!(i&MAX_INTEGER_MASK)) 
		{
			Atom name = core->uintToAtom (i);
			if (traits()->needsHashtable)
			{
				return getTable()->contains(name);
			}
			else
			{
				// ISSUE should this walk the proto chain?
				return false;
			}
		}
		else
		{
			return hasAtomProperty(core->internUint32(i)->atom());
		}
	}

	Atom ScriptObject::getMultinameProperty(Multiname* multiname) const
	{
		if (isValidDynamicName(multiname))
		{
			return getStringProperty(multiname->getName());
		}
		else
		{
			Toplevel* toplevel = this->toplevel();

			if (multiname->isNsset())
				toplevel->throwReferenceError(kReadSealedErrorNs, multiname, traits());
			else
				toplevel->throwReferenceError(kReadSealedError, multiname, traits());
			return undefinedAtom;
		}
	}

	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom ScriptObject::callProperty(Multiname* multiname, int argc, Atom* argv)
	{
		Toplevel* toplevel = this->toplevel();
		Atom method = getMultinameProperty(multiname);
		if (!AvmCore::isObject(method))
			toplevel->throwTypeError(kCallOfNonFunctionError, core()->toErrorString(multiname));
		argv[0] = atom(); // replace receiver
		return toplevel->op_call(method, argc, argv);
	}

	Atom ScriptObject::constructProperty(Multiname* multiname, int argc, Atom* argv)
	{
		Atom ctor = getMultinameProperty(multiname);
		argv[0] = atom(); // replace receiver
		return toplevel()->op_construct(ctor, argc, argv);
	}
	
	Atom ScriptObject::getDescendants(Multiname* /*name*/) const
	{
		toplevel()->throwTypeError(kDescendentsError, core()->toErrorString(traits()));
		return undefinedAtom;// not reached
	}

#ifdef AVMPLUS_VERBOSE
	Stringp ScriptObject::format(AvmCore* core) const
	{
		if (traits()->name != NULL) {
			return core->concatStrings(traits()->format(core),
									   core->concatStrings(core->newString("@"),
														   core->formatAtomPtr(atom())));
		} else {
			return core->concatStrings(core->newString("{}@"),
									   core->formatAtomPtr(atom()));
		}
	}
#endif

	Atom ScriptObject::defaultValue()
	{
		AvmCore *core = this->core();
		Toplevel* toplevel = this->toplevel();

		Atom atomv_out[1];

		// call this.valueOf()
		Multiname tempname(core->publicNamespace, core->kvalueOf);
		atomv_out[0] = atom();
		Atom result = toplevel->callproperty(atom(), &tempname, 0, atomv_out, vtable);

		// if result is primitive, return it
		if ((result&7) != kObjectType)
			return result;

		// otherwise call this.toString()
		tempname.setName(core->ktoString);
		atomv_out[0] = atom();
		result = toplevel->callproperty(atom(), &tempname, 0, atomv_out, vtable);

		// if result is primitive, return it
		if ((result&7) != kObjectType)
			return result;

		// could not convert to primitive.
		toplevel->throwTypeError(kConvertToPrimitiveError, core->toErrorString(traits()));
		return undefinedAtom;
	}

	// Execute the ToString algorithm as described in ECMA-262 Section 9.8.
	// This is ToString(ToPrimitive(input argument, hint String))
	// ToPrimitive(input argument, hint String) calls [[DefaultValue]]
	// described in ECMA-262 8.6.2.6.  The [[DefaultValue]] algorithm
	// with hint String is inlined here.
	Atom ScriptObject::toString()
	{
		AvmCore *core = this->core();
		Toplevel* toplevel = this->toplevel();

		Atom atomv_out[1];

		// call this.toString()
		Multiname tempname(core->publicNamespace, core->ktoString);
		atomv_out[0] = atom();
		Atom result = toplevel->callproperty(atom(), &tempname, 0, atomv_out, vtable);

		// if result is primitive, return its ToString
		if ((result&7) != kObjectType)
			return core->string(result)->atom();

		// otherwise call this.valueOf()
		tempname.setName(core->kvalueOf);
		atomv_out[0] = atom();
		result = toplevel->callproperty(atom(), &tempname, 0, atomv_out, vtable);

		// if result is primitive, return it
		if ((result&7) != kObjectType)
			return core->string(result)->atom();

		// could not convert to primitive.
		toplevel->throwTypeError(kConvertToPrimitiveError, core->toErrorString(traits()));
		return undefinedAtom;
	}

	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom ScriptObject::call(int /*argc*/, Atom* /*argv*/)
	{
		// TypeError in ECMA to execute a non-function
		Multiname name(core()->publicNamespace, core()->constantString("value"));
		toplevel()->throwTypeError(kCallOfNonFunctionError, core()->toErrorString(&name));
		return undefinedAtom;
	}

	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom ScriptObject::construct(int /*argc*/, Atom* /*argv*/)
	{
		// TypeError in ECMA to execute a non-function
		toplevel()->throwTypeError(kConstructOfNonFunctionError);
		return undefinedAtom;
	}

	Atom ScriptObject::getSlotAtom(int slot)
	{
		Traits* traits = this->traits();
		AvmCore* core = traits->core;
		Traits* t = traits->getSlotTraits(slot);
		AvmAssert(t != VOID_TYPE);
		void* p = ((char*)this) + traits->getOffsets()[slot];
		if (!t || t == OBJECT_TYPE)
		{
			return *((Atom*)p);
		}
		else if (t == NUMBER_TYPE)
		{
			return core->doubleToAtom(*((double*)p));
		}
		else if (t == INT_TYPE)
		{
			return core->intToAtom(*((int*)p));
		}
		else if (t == UINT_TYPE)
		{
			return core->uintToAtom(*((uint32*)p));
		}
		else if (t == BOOLEAN_TYPE)
		{
			return (*((int*)p)<<3)|kBooleanType;
		}
		else if (t == STRING_TYPE)
		{
			Stringp s = *((Stringp*)p);
			return s->atom(); // may be null|kStringType, that's ok
		}
		else if (t == NAMESPACE_TYPE)
		{
			Namespace* ns = *((Namespace**)p);
			return ns->atom(); // may be null|kNamespaceType, no problemo
		}
		else
		{
			ScriptObject* o = *((ScriptObject**)p);
			return o->atom(); // may be null|kObjectType, copasthetic
		}
	}

	void ScriptObject::setSlotAtom(int slot, Atom value)
	{
		Traits* traits = this->traits();
		AvmCore* core = traits->core;
		Traits* t = traits->getSlotTraits(slot);
		AvmAssert(t != VOID_TYPE);
		Atom *p = (Atom*)(((char*)this) + traits->getOffsets()[slot]);
		if (!t || t == OBJECT_TYPE)
		{
			//*((Atom*)p) = value;
			WBATOM(core->GetGC(), this, p, value);
		}
		else if (t == NUMBER_TYPE)
		{
			*((double*)p) = AvmCore::number_d(value);
		}
		else if (t == INT_TYPE)
		{
			*((int*)p) = AvmCore::integer_i(value);
		}
		else if (t == UINT_TYPE)
		{
			*((uint32*)p) = AvmCore::integer_u(value);
		}
		else if (t == BOOLEAN_TYPE)
		{
			*((int*)p) = urshift(value,3);
		}
		else
		{
			//*((int32*)p) = value & ~7;
			WBRC(core->GetGC(), this, p, value & ~7);
		}
	}

	Atom ScriptObject::nextName(int index)
	{
		AvmAssert(traits()->needsHashtable);
		AvmAssert(index > 0);

		Hashtable *ht = getTable();
		if (index-1 >= ht->getNumAtoms()/2)
			return nullStringAtom;
		Atom m = ht->getAtoms()[(index-1)<<1] & ~ht->dontEnumMask();
		if (AvmCore::isNullOrUndefined(m))
			return nullStringAtom;
		return m;
	}

	Atom ScriptObject::nextValue(int index)
	{
		AvmAssert(traits()->needsHashtable);
		AvmAssert(index > 0);

		Hashtable *ht = getTable();
		if (index-1 >= ht->getNumAtoms()/2)
			return undefinedAtom;
		Atom m = ht->getAtoms()[(index-1)<<1] & ~ht->dontEnumMask();
		if (AvmCore::isNullOrUndefined(m))
			return nullStringAtom;
		return getTable()->getAtoms()[((index-1)<<1)+1];
	}

	int ScriptObject::nextNameIndex(int index)
	{
		AvmAssert(index >= 0);

		if (!traits()->needsHashtable)
			return 0;

		// todo clean this up.
		if (index != 0) {
			index = index<<1;
		}
		// Advance to first non-empty slot.
		Hashtable* table = getTable();
		int numAtoms = table->getNumAtoms();
		int dontEnumMask = table->dontEnumMask();
		while (index < numAtoms) {
			Atom m = table->getAtoms()[index];
			if (m != 0 && m != undefinedAtom && (m & dontEnumMask) != Hashtable::kDontEnumBit)
				return (index>>1)+1;
			index += 2;
		}
		return 0;
	}

	/**
	 * the default implementation calls the delegate (should be the base class objects) 
	 * so that derived classes delegate object allocation to the base class.  If you 
	 * are overriding createInstance and your base class is Object, unconditionally allocate
	 * the instance object using the given vtable and prototype.
	 */
	ScriptObject* ScriptObject::createInstance(VTable* ivtable, ScriptObject* prototype)
	{
		if (ivtable->base)
		{
			ScopeChain *scope = vtable->scope;
			if (scope->getSize())
			{
				Atom baseAtom = scope->getScope(scope->getSize()-1);
				if (!AvmCore::isObject(baseAtom))
					toplevel()->throwVerifyError(kCorruptABCError);

				ScriptObject *base = AvmCore::atomToScriptObject(baseAtom);
				// make sure scope object is base type's class object
				AvmAssert(base->traits()->itraits == this->traits()->itraits->base);
				return base->createInstance(ivtable, prototype);
			}
		}

		return core()->newObject(ivtable, prototype);
	}

	
	/**
     * Function.prototype.call()
     */
	Atom ScriptObject::function_call(Atom thisArg,
							 Atom *argv,
							 int argc)
	{
		if (argc > 0) 
		{
			// argc = # of AS3 arguments
			AvmAssert(argc >= 0);
			Atom *newargs = (Atom *) alloca(sizeof(Atom)*(argc+1));
			newargs[0] = thisArg;
			memcpy(&newargs[1], argv, argc*sizeof(Atom));
			return call(argc, newargs);
		}
		else
		{
			// AS3 caller didn't supply any args
			Atom newargs[1] = { thisArg };
			return call(0, newargs);
		}
	}

	/**
     * Function.prototype.apply()
     */
	Atom ScriptObject::function_apply(Atom thisArg, Atom argArray)
	{
		// when argArray == undefined or null, same as not being there at all
		// see Function/e15_3_4_3_1.as 
	
		if (!AvmCore::isNullOrUndefined(argArray))
		{
			AvmCore* core = this->core();

			if (!core->istype(argArray, ARRAY_TYPE))
				toplevel()->throwTypeError(kApplyError);

			ArrayObject *a = (ArrayObject*)AvmCore::atomToScriptObject(argArray);
			
			int count = a->getLength();
			Atom* newargs = (Atom*) alloca(sizeof(Atom)*(1+count));
			newargs[0] = thisArg;
			for (int i=0; i<count; i++) {
				newargs[i+1] = a->getUintProperty(i);
			}
			return call(count, newargs);
		}
		else
		{
			Atom newargs[1] = { thisArg };
			return call(0, newargs);
		}
	}
 
#ifdef DEBUGGER
	void ScriptObject::addInstance(Atom a)
	{
		if(!instances) {			
			if(!toplevel()->objectClass->ivtable())
				return;
			VTable *vt = toplevel()->objectClass->ivtable();
			instances = new (gc(), vt->getExtraSize()) GCHashtableScriptObject(vt);
		}
		if(AvmCore::isPointer(a) && !AvmCore::isNumber(a))
		{
			instances->ht.add(a, NULL);
			// tell the thing about its atom rep so it can properly remove itself later
			AvmPlusScriptableObject *o = (AvmPlusScriptableObject*)(a&~7);
			o->setType(a&7);
			o->setCreator(this);
		}

	}

	void ScriptObject::removeInstance(Atom a)
	{
		if(instances && instances->ht.count() > 0)
		{
			instances->ht.remove((const void*)a);
		}
	}

	ScriptObject* ScriptObject::getInstances()
	{
		return instances;
	}

	ScriptObject* ScriptObject::getSlotIterator()
	{
		VTable *vt = toplevel()->objectClass->ivtable();
		return new (gc(), vt->getExtraSize()) SlotIterator(this, vt);
	}
	
	uint32 ScriptObject::size() const
	{
		uint32 size = traits()->getTotalSize();
		if(traits()->needsHashtable)
		{
			Hashtable *ht = getTable();
			size += ht->getSize() * 2 * sizeof(Atom);
		}
		size -= sizeof(AvmPlusScriptableObject);
		return size;
	}
#endif
}
