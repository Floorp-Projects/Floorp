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


#include "avmplus.h"

namespace avmplus
{
	PoolObject::PoolObject(AvmCore* core, ScriptBuffer& sb, const byte* startPos) :
		core(core),
		cpool_int(0),
		cpool_uint(0),
		cpool_double(core->GetGC(), 0),
		cpool_string(core->GetGC(), 0),
		cpool_ns(core->GetGC(), 0),
		cpool_ns_set(core->GetGC(), 0),
		cpool_mn(0),
		methods(core->GetGC(), 0),
		metadata_infos(0),
		cinits(core->GetGC(), 0),
		scripts(core->GetGC(), 0),
		abcStart(startPos)
#ifdef AVMPLUS_VERIFYALL
		,verifyQueue(core->GetGC(), 0)
#endif
	{
		namedTraits = new(core->GetGC()) MultinameHashtable();
		m_code = sb.getImpl();
#ifdef AVMPLUS_MIR
		codeBuffer = new (core->GetGC()) GrowableBuffer(core->GetGC()->GetGCHeap());
#endif
		version = AvmCore::readU16(&code()[0]) | AvmCore::readU16(&code()[2])<<16;
	}

	PoolObject::~PoolObject()
	{
		#ifdef AVMPLUS_MIR
		delete codeBuffer;
		#endif
	}
	
    AbstractFunction* PoolObject::getMethodInfo(uint32 index)
    {
		AvmAssert(index < methodCount);
		return methods[index];
    }

    const byte* PoolObject::getMetadataInfoPos(uint32 index)
    {
		AvmAssert (index < metadataCount);
		return metadata_infos[index];
    }

	Traits* PoolObject::getBuiltinTraits(Stringp name) const
	{
		AvmAssert(BIND_NONE == 0);
		return (Traits*) namedTraits->getName(name);
	}

	Traits* PoolObject::getTraits(Stringp name, Namespace* ns, bool recursive/*=true*/) const
	{
		// look for class in VM-wide type table
		Traits* t = domain->getNamedTraits(name, ns, recursive);

		// look for class in current ABC file
		if (t == NULL)
			t = (Traits*) namedTraits->get(name, ns);
		return t;
	}

	Traits* PoolObject::getTraits(Stringp name, bool recursive/*=true*/) const
	{
		return getTraits(name, core->publicNamespace, recursive);
	}

	Traits* PoolObject::getTraits(Multiname* mname, const Toplevel* toplevel, bool recursive/*=true*/) const
	{
		// do full lookup of multiname, error if more than 1 match
		// return Traits if 1 match, NULL if 0 match, throw ambiguity error if >1 match
		Traits* match = NULL;
		if (mname->isBinding())
		{
			// multiname must not be an attr name, have wildcards, or have runtime parts.
			for (int i=0, n=mname->namespaceCount(); i < n; i++)
			{
				Traits* t = getTraits(mname->getName(), mname->getNamespace(i), recursive);
				if (t != NULL)
				{
					if (match == NULL)
					{
						match = t;
					}
					else if (match != t)
					{
						// ambiguity
						toplevel->throwReferenceError(kAmbiguousBindingError, mname);
					}
				}
			}
		}
		return match;
	}

	void PoolObject::addNamedTraits(Stringp name, Namespace* ns, Traits* traits)
	{
		namedTraits->add(name, ns, (Binding)traits);
	}

#ifdef AVMPLUS_VERIFYALL
	void PoolObject::enq(AbstractFunction* f)
	{
		if (!f->isVerified() && !(f->flags & AbstractFunction::VERIFY_PENDING))
		{
			f->flags |= AbstractFunction::VERIFY_PENDING;
			verifyQueue.add(f);
		}
	}

	void PoolObject::enq(Traits* t)
	{
		for (int i=0, n=t->methodCount; i < n; i++)
		{
			AbstractFunction* f = t->getMethod(i);
			if (f)
				enq(f);
		}
	}

	void PoolObject::processVerifyQueue(Toplevel* toplevel)
	{
		while (!verifyQueue.isEmpty())
		{
			AbstractFunction* f = verifyQueue.removeLast();
			AvmAssert(!f->isVerified());
			f->verify(toplevel);
		}
	}
#endif

	Namespace* PoolObject::getNamespace(int index) const
	{
		return cpool_ns[index];  
	}

	NamespaceSet* PoolObject::getNamespaceSet(int index) const
	{
		return cpool_ns_set[index];  
	}

	Stringp PoolObject::getString(int index) const
	{
		return cpool_string[index];  
	}

	Atom PoolObject::getDefaultValue(const Toplevel* toplevel, uint32 index, CPoolKind kind, Traits* t) const
	{
		AvmAssert(index != 0);
		// Look in the cpool specified by kind
		switch(kind)
		{
		case CONSTANT_Int:
			if( index >= constantIntCount )
				toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(constantIntCount));
			return core->intToAtom(cpool_int[index]);

		case CONSTANT_UInt:
			if( index >= constantUIntCount )
				toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(constantUIntCount));
			return core->uintToAtom(cpool_uint[index]);

		case CONSTANT_Double:
			if( index >= constantDoubleCount )
				toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(constantDoubleCount));
			return kDoubleType|(uintptr)cpool_double[index];

		case CONSTANT_Utf8:
			if( index >= constantStringCount )
				toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(constantStringCount));
			return cpool_string[index]->atom();

		case CONSTANT_True:
			return trueAtom;

		case CONSTANT_False:
			return falseAtom;

		case CONSTANT_Namespace:
        case CONSTANT_PackageNamespace:
        case CONSTANT_PackageInternalNs:
        case CONSTANT_ProtectedNamespace:
        case CONSTANT_ExplicitNamespace:
        case CONSTANT_StaticProtectedNs:
		case CONSTANT_PrivateNs:
			if( index >= constantNsCount )
				toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(constantNsCount));
			return cpool_ns[index]->atom();

		case CONSTANT_Null:
			return nullObjectAtom;

		default:
			{
			// Multinames & NamespaceSets are invalid default values.
			if (t)
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}
			else
			{
				toplevel->throwVerifyError(kCorruptABCError);
			}
			return undefinedAtom; // not reached
			}
		}
	}

	void PoolObject::parseMultiname(const byte *pos, Multiname& m) const
	{
		// the multiname has already been validated so we don't do
		// any checking here, we just fill in the Multiname object
		// with the information we have parsed.

		int index;
		CPoolKind kind = (CPoolKind) *(pos++);
        switch (kind)
        {
		case CONSTANT_Qname: 
		case CONSTANT_QnameA:
		{
			// U16 namespace_index
            // U16 name_index
			// parse a multiname with one namespace (aka qname)

			index = AvmCore::readU30(pos);
			if (!index)
				m.setAnyNamespace();
			else
				m.setNamespace(getNamespace(index));

			index = AvmCore::readU30(pos);
			if (!index)
				m.setAnyName();
			else
				m.setName(getString(index));

			m.setQName();
			m.setAttr(kind==CONSTANT_QnameA);
			break;
		}

		case CONSTANT_RTQname:
		case CONSTANT_RTQnameA: 
		{
			// U16 name_index
			// parse a multiname with just a name; ns fetched at runtime

			index = AvmCore::readU30(pos);
			if (!index)
				m.setAnyName();
			else
				m.setName(getString(index));

			m.setQName();
			m.setRtns();
			m.setAttr(kind==CONSTANT_RTQnameA);
			break;
		}

		case CONSTANT_RTQnameL:
		case CONSTANT_RTQnameLA:
		{
			m.setQName();
			m.setRtns();
			m.setRtname();
			m.setAttr(kind==CONSTANT_RTQnameLA);
			break;
		}

		case CONSTANT_Multiname:
		case CONSTANT_MultinameA:
		{
			index = AvmCore::readU30(pos);
			if (!index)
				m.setAnyName();
			else
				m.setName(getString(index));

			index = AvmCore::readU30(pos);
			AvmAssert(index != 0);
			m.setNsset(getNamespaceSet(index));

			m.setAttr(kind==CONSTANT_MultinameA);
			break;
		}

		case CONSTANT_MultinameL:
		case CONSTANT_MultinameLA:
		{
			m.setRtname();

			index = AvmCore::readU30(pos);
			AvmAssert(index != 0);
			m.setNsset(getNamespaceSet(index));

			m.setAttr(kind==CONSTANT_MultinameLA);
			break;
		}
		
		default:
			AvmAssert(false);
		}

		return;
	}


	Atom PoolObject::resolveQName(const byte* &p, Multiname &m, const Toplevel* toplevel) const
	{
		uint32 index = AvmCore::readU30(p);

		if (index == 0 || index >= constantMnCount)
			toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(constantMnCount));

		Atom a = cpool_mn[index];

		parseMultiname(a, m);
		if (!m.isQName())
			toplevel->throwVerifyError(kCpoolEntryWrongTypeError, core->toErrorString(index));

		return a;
	}

	
	Traits* PoolObject::resolveTypeName(const byte* &pc, const Toplevel* toplevel, bool allowVoid/*=false*/) const
	{
		// only save the type name for now.  verifier will resolve to traits
		uint32 index = AvmCore::readU30(pc);
		if (index == 0)
		{
			return NULL;
		}

		// check contents is a multiname.  in the cpool, and type system, kObjectType means multiname.
		if (index >= constantMnCount)
			toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(constantMnCount));

		Atom a = cpool_mn[index];

		Multiname m;
		parseMultiname(a, m);

		Traits* t = getTraits(&m, toplevel);
		if (!t)
		{
			#ifdef AVMPLUS_VERBOSE
			if (!toplevel || !toplevel->verifyErrorClass())
				core->console << "class not found: " << &m << "\n";
			#endif
			toplevel->throwVerifyError(kClassNotFoundError, core->toErrorString(&m));
		}

		if (!allowVoid && t == VOID_TYPE)
			toplevel->throwVerifyError(kIllegalVoidError);

		return t;
	}

	void PoolObject::allowEarlyBinding(Traits* t, bool& slot) const
	{
		// the compiler can early bind to a type's slots when it's defined
		// in the same abc file (ensuring it came from the same compiler)
		// or when the base class came from another abc file and has zero slots
		// this ensures you cant use the early opcodes to access an external type's
		// private members.
		slot = true;
		while (t != NULL && t->slotCount > 0 &&	slot)
		{
			if (t->pool != this)
			{
				if(t->slotCount > 0)
					slot = false;
			}
			t = t->base;
		}
	}

    void PoolObject::resolveTraits(Traits *traits, int firstSlot, const Toplevel* toplevel)
    {
		int offset = traits->sizeofInstance;
		int padoffset = -1;
		if (traits->base && traits->base->base)
		{
			AvmAssert(traits->base->getTotalSize() != 0);
			offset += traits->base->getTotalSize() - traits->base->sizeofInstance;
		}

		const byte* pos = traits->getTraitsPos();
		AvmAssert(pos != NULL);

		int nameCount = AvmCore::readU30(pos);

		int slot_id = firstSlot;
		
		AbcGen gen(core->GetGC(), traits->slotCount * 7);

		bool earlySlotBinding;
		allowEarlyBinding(traits, earlySlotBinding);

        for (int i=0; i < nameCount; i++)
        {
			Multiname qn;
			resolveQName(pos, qn, toplevel);
			Stringp name = qn.getName();
			Namespace* ns = qn.getNamespace();
            int tag = (TraitKind)pos[0];
			TraitKind kind = (TraitKind) (tag & 0x0f); //Get rid of the flags
			pos += 1;

			switch (kind)
			{
			case TRAIT_Slot:
			case TRAIT_Const:
			{
				// compute the slot
				uint32 useSlotId = AvmCore::readU30(pos);
                if(!earlySlotBinding ) useSlotId = 0;
				if (!useSlotId)
					useSlotId = slot_id++;
				else
					useSlotId--;

				// compute the type
				Traits* slotTraits = resolveTypeName(pos, toplevel);

				// default value
				int value_index = AvmCore::readU30(pos);

				CPoolKind value_kind = (CPoolKind)0;
				if (value_index)
				{
					value_kind = (CPoolKind)*(pos++);
				}

				// default value for this slot.
				int slotOffset;
				if ((slotTraits == NUMBER_TYPE)
					#ifdef AVMPLUS_64BIT
					|| ((slotTraits != INT_TYPE) && (slotTraits != UINT_TYPE) && (slotTraits != BOOLEAN_TYPE))
					#endif			
				)
				{
					// 8-aligned, 8-byte field
					if (offset&7)
					{
						padoffset = offset;
						offset += 4;
					}
					slotOffset = offset;
					offset += 8;
				}
				else
				{
					// 4-aligned, 4-byte field
					if (padoffset != -1)
					{
						slotOffset = padoffset;
						padoffset = -1;
					}
					else
					{
						slotOffset = offset;
						offset += 4;
					}
				}

				traits->setSlotInfo(value_index, useSlotId, toplevel, slotTraits, slotOffset, value_kind, gen);
				if( tag & ATTR_metadata )
				{
					traits->setSlotMetadataPos(useSlotId, pos);
				}
				break;
			}

			case TRAIT_Class: 
			{
				// compute the slot
				uint32 useSlotId = AvmCore::readU30(pos);
                if( !earlySlotBinding ) useSlotId = 0;
				if (!useSlotId)
					useSlotId = slot_id++;
				else
					useSlotId--;

				// get the class type
				uint32 class_info = AvmCore::readU30(pos);
				if (class_info >= classCount)
					toplevel->throwVerifyError(kClassInfoExceedsCountError, core->toErrorString(class_info), core->toErrorString(classCount));

				AbstractFunction* cinit = cinits[class_info];
				if (!cinit) 
					toplevel->throwVerifyError(kClassInfoOrderError, core->toErrorString(class_info));

				int slotOffset;
				#ifdef AVMPLUS_64BIT
				// 8-aligned, 8-byte field
				if (offset&7)
				{
					padoffset = offset;
					offset += 4;
				}
				slotOffset = offset;
				offset += 8;
				#else
				// 4-aligned, 4-byte field
				if (padoffset != -1)
				{
					slotOffset = padoffset;
					padoffset = -1;
				}
				else
				{
					slotOffset = offset;
					offset += 4;
				}
				#endif

				traits->setSlotInfo(0, useSlotId, toplevel, cinit->declaringTraits, slotOffset, CPoolKind(0), gen);
				break;
			}

			case TRAIT_Method:
			{
				int earlyDispId = AvmCore::readU30( pos );
                (void)earlyDispId;

				uint32 method_info = AvmCore::readU30(pos);

				// method_info range already checked in AbcParser
				AvmAssert(method_info < methodCount);
				AbstractFunction *f = getMethodInfo(method_info);
				AvmAssert(f != NULL);

				// disp_id assigned by abcParser, this binding must exist already.
				Binding b = traits->get(name, ns);

				AvmAssert(AvmCore::isMethodBinding(b));
				int disp_id = AvmCore::bindingToMethodId(b);

				// !!@ Ed says there may be an earlier place in AbcParser to catch this
				if (traits->getMethod(disp_id) && traits->getOverride(ns,name,tag,toplevel) == BIND_NONE)
				{
					toplevel->throwVerifyError(kDuplicateDispIdError, core->toErrorString(traits->getMethod(disp_id)), core->toErrorString(disp_id));
				}
				traits->setMethod(disp_id, f);
				if( tag & ATTR_metadata )
				{
					traits->setMethodMetadataPos(disp_id, pos);
				}
				break;
			}

			case TRAIT_Getter:
			case TRAIT_Setter:
			{
				int earlyDispId = AvmCore::readU30(pos);
                (void)earlyDispId;

				uint32 method_info = AvmCore::readU30(pos);
				
				// method_info already checked in AbcParser
				AvmAssert(method_info < methodCount);
				AbstractFunction* f = getMethodInfo(method_info);
				AvmAssert(f != NULL);

				Binding b = traits->get(name, ns);

				AvmAssert(b==BIND_NONE || AvmCore::isAccessorBinding(b));
				uint32 disp_id = kind == TRAIT_Getter ? AvmCore::bindingToGetterId(b) : AvmCore::bindingToSetterId(b);
				AvmAssert(disp_id < traits->methodCount);
				// !!@ Ed says there may be an earlier place in AbcParser to catch this
				if (traits->getMethod(disp_id))
				{
					Binding baseBinding = traits->getOverride(ns,name,tag,toplevel);
					if (kind == TRAIT_Getter && !AvmCore::hasGetterBinding(baseBinding) ||
						kind == TRAIT_Setter && !AvmCore::hasSetterBinding(baseBinding))
					{
						toplevel->throwVerifyError(kDuplicateDispIdError, core->toErrorString(traits->getMethod(disp_id)), core->toErrorString(disp_id));
					}
				}
				traits->setMethod(disp_id, f);
				if( tag & ATTR_metadata )
				{
					traits->setMethodMetadataPos(disp_id, pos);
				}

				break;
			}

			case TRAIT_Function:
			{
				// compute the slot
				int useSlotId = AvmCore::readU30(pos);
                if( !earlySlotBinding ) useSlotId = 0;
				if (!useSlotId)
					useSlotId = slot_id++;
				else
					useSlotId--;

				// compute the type
				uint32 method_info = AvmCore::readU30(pos);

				AbstractFunction *f = getMethodInfo(method_info);

				int slotOffset;
				#ifdef AVMPLUS_64BIT
				// 8-aligned, 8-byte field
				if (offset&7)
				{
					padoffset = offset;
					offset += 4;
				}
				slotOffset = offset;
				offset += 8;
				#else
				// 4-aligned, 4-byte field
				if (padoffset != -1)
				{
					slotOffset = padoffset;
					padoffset = -1;
				}
				else
				{
					slotOffset = offset;
					offset += 4;
				}
				#endif

				traits->setSlotInfo(0, useSlotId, toplevel, f->declaringTraits, slotOffset, CPoolKind(0), gen);
				if( tag & ATTR_metadata )
				{
					traits->setMethodMetadataPos(useSlotId, pos);
				}
				break;
			}
			}

			// skip metadata
            if( tag & ATTR_metadata )
            {
				int metaCount = AvmCore::readU30(pos);
				for( int metadata = 0; metadata < metaCount; ++metadata )
				{
					AvmCore::readU30(pos);
				}
            }
		}

		int *offsets = traits->getOffsets();
		for (uint32 i=0, n=traits->slotCount; i < n; i++)
		{
			if (offsets[i] == 0)
			{
				// sparse slot table.  make types default to *
				#ifdef AVMPLUS_VERBOSE
				if (verbose)
				{
					core->console << "WARNING: slot " << i+1 << " on " << traits << " not defined by compiler.  Using *\n";
				}
				#endif

				AvmAssert(0);
				int slotOffset;
				// 4-aligned, 4-byte field
				if (padoffset != -1)
				{
					slotOffset = padoffset;
					padoffset = -1;
				}
				else
				{
					slotOffset = offset;
					offset += 4;
				}
				
				traits->setSlotInfo(0, i, toplevel, NULL, slotOffset, CPoolKind(0), gen);
			}
		}		

		// if initialization code gen is required, create a new method body and write it to traits->init->body_pos
		if(gen.size() > 0)
		{
			AbcGen newMethodBody(core->GetGC(), 16+gen.size());

			// insert body preamble
			MethodInfo *init;
			
			if(traits->init) {
				init = (MethodInfo*)(AbstractFunction*)traits->init;
				const byte *pos = init->body_pos;
				if( !init->body_pos ) toplevel->throwVerifyError(kCorruptABCError);

				int maxStack = AvmCore::readU30(pos);
				// the code we're generating needs at least 2
				maxStack = maxStack > 1 ? maxStack : 2;
				newMethodBody.writeInt(maxStack); // max_stack
				newMethodBody.writeInt(AvmCore::readU30(pos)); //local_count
				newMethodBody.writeInt(AvmCore::readU30(pos)); //init_scope_depth
				newMethodBody.writeInt(AvmCore::readU30(pos)); //max_scope_depth

				// skip real code length
				int code_length = AvmCore::readU30(pos);
				
				// if first instruction is OP_constructsuper keep it as first instruction
				if(*pos == OP_constructsuper)
				{
					gen.getBytes().insert(0, OP_constructsuper);
					// don't invoke it again later
					pos++;
					code_length--;
				}

				gen.abs_jump(pos, code_length);	

			} else {
				// make one
				init = new (core->GetGC()) MethodInfo();
				init->declaringTraits = traits;
				init->pool = this;
				init->param_count = 0;
				init->restOffset = 4; // sizeof(this)
				init->initParamTypes(1);
				init->setParamType(0, traits);	
				init->setReturnType(VOID_TYPE);
				traits->init = init;
				init->flags |= AbstractFunction::LINKED;

				newMethodBody.writeInt(2); // max_stack
				newMethodBody.writeInt(1); //local_count
				newMethodBody.writeInt(1); //init_scope_depth
				newMethodBody.writeInt(1); //max_scope_depth

				gen.returnvoid();
			}

			newMethodBody.writeInt(gen.size()); // code length
			newMethodBody.writeBytes(gen.getBytes());

			// no exceptions, when we jump to the real code, we'll read the exceptions for that code
			newMethodBody.writeInt(0);

			// the verifier and interpreter don't read the activation traits so stop here
			byte *newBytes = (byte*) core->GetGC()->Alloc(newMethodBody.size());
			memcpy(newBytes, newMethodBody.getBytes().getData(), newMethodBody.size());
			//init->body_pos = newBytes;
			WB(core->GetGC(), init, &init->body_pos, newBytes);
		}

		if (traits->base && traits->base->base && traits->base->hashTableOffset != -1)
		{
			traits->hashTableOffset = traits->base->hashTableOffset + traits->sizeofInstance - traits->base->sizeofInstance;
		}
		else if (traits->needsHashtable)
		{
			traits->hashTableOffset = offset;
			offset += sizeof(Hashtable);
		}

		traits->setTotalSize((size_t) offset);
    }

	Binding Traits::getOverride(Namespace* ns, Stringp name, int tag, const Toplevel *toplevel) const
	{
		int kind = tag & 0x0f;
		Binding baseBinding = BIND_NONE;
		if (base)
		{
			if (protectedNamespace == ns && base->protectedNamespace)
			{
				baseBinding = base->findBinding(name, base->protectedNamespace);
			}
			else
			{
				baseBinding = base->findBinding(name, ns);
			}
		}
		if (baseBinding == BIND_NONE)
		{
			if (tag & ATTR_override)
			{
				// error if override attr set, and nothing is actually overridden
				Multiname qname(ns,name);
				toplevel->throwVerifyError(kIllegalOverrideError, toplevel->core()->toErrorString(&qname), toplevel->core()->toErrorString((Traits*)this));
			}
			return BIND_NONE;
		}

		switch (kind)
		{
		case TRAIT_Method:
		{
			if (AvmCore::isMethodBinding(baseBinding))
			{
				if (!(tag & ATTR_override))
				{
					// error if override flag not set, and something is overridden
					break;
				}
				return baseBinding;
			}
			// error, method can only override a virtual method
			break;
		}
		case TRAIT_Getter:
		{
			if (AvmCore::hasGetterBinding(baseBinding))
			{
				if (!(tag & ATTR_override))
				{
					// error if override flag not set, and something is overridden
					break;
				}
				return baseBinding;
			}
			if ((baseBinding&7) == BIND_SET)
			{
				if (tag & ATTR_override)
				{
					// error if override attr set, and nothing is actually overridden
					break;
				}
				return baseBinding;
			}
			// error, method can only override a virtual method
			break;
		}
		case TRAIT_Setter:
		{
			if (AvmCore::hasSetterBinding(baseBinding))
			{
				if (!(tag & ATTR_override))
				{
					// error if override flag not set, and something is overridden
					break;
				}
				return baseBinding;
			}
			if ((baseBinding&7) == BIND_GET)
			{
				if (tag & ATTR_override)
				{
					// error if override attr set, and nothing is actually overridden
					break;
				}
				return baseBinding;
			}
			// error, method can only override a virtual method
			break;
		}
		default:
			// internal error to call getOverride on a non-method trait
			AvmAssert(false);
			break;
		}
		Multiname qname(ns,name);
#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
			core->console << "illegal override in "<< this << ": " << &qname <<"\n";
#endif
		toplevel->throwVerifyError(kIllegalOverrideError, toplevel->core()->toErrorString(&qname), toplevel->core()->toErrorString((Traits*)this));
		return BIND_NONE;
	}

	void PoolObject::addPrivateNamedScript(Stringp name, Namespace* ns, AbstractFunction *script)
	{
		privateNamedScripts.add(name, ns, (Atom)script);
	}

	AbstractFunction* PoolObject::getNamedScript(Multiname* multiname) const
	{
		AbstractFunction *f = domain->getNamedScript(multiname);
		if(!f)
		{
			f = (AbstractFunction*)privateNamedScripts.getMulti(multiname);
		}
		return f;
	}
}
