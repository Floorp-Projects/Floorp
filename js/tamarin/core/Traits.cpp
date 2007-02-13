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
	using namespace MMgc;

	/**
     * traits with base traits (inheritance)
     */
    Traits::Traits(AvmCore *core, 
				   Traits *base,
				   int nameCount,
				   int interfaceCount,
				   int interfaceCapacity,
				   size_t sizeofInstance)
		: core(core),
		MultinameHashtable(0)
    {
		// delay initialization to when vtable is setup so memory can be reported appropriately
		Init(1+5*nameCount/4);

		this->needsHashtable = false;
		this->sizeofInstance = sizeofInstance;
		this->isInterface    = false;
		this->hasInterfaces  = false;

		this->interfaceCount = interfaceCount;
		this->interfaceCapacity = interfaceCapacity;

		this->hashTableOffset = -1;
		
		this->metadata_pos = NULL;

		this->slot_metadata_pos = NULL;

		// We are being sneaky and storing the slots immediately
		// at the end of this object.  The methods table follows
		// the slots table.

		// replaced with getInterfaces()
		//interfaces = (Traitsp*)((char*)this+sizeof(Traits));

		if (base != NULL)
		{
			this->base			 = base;

			// clear and copy interfaces
			addInterface(base);
			
			// don't copy down the symbol table, we'll walk up the tree
			// when we need to.  This saves a lot of memory for negligable slowdown.
		}
    }

	Traits::~Traits()
	{
	}

	void Traits::addInterface(Traits* ifc)
	{
		AvmAssert(ifc != this && ifc != NULL);
		//*(findInterface(ifc)) = ifc;
		WB(core->GetGC(), this, findInterface(ifc), ifc);

		if(ifc->isInterface)
			hasInterfaces = true;

		for (int i=0, n=ifc->interfaceCapacity; i < n; i++)
		{
			Traits* t = ifc->getInterface(i);
			if (t != NULL)
			{
				AvmAssert(ifc != this);
				if(t->isInterface)
					hasInterfaces = true;
				//*(findInterface(t)) = t;
				WB(core->GetGC(), this, findInterface(t), t);
			}			
		}
	}

	Binding Traits::findBinding(Stringp name, Namespace* ns) const
	{
		const Traits* t = this;
		Binding b;
		do
		{
			b = t->get(name, ns);
		}
		while (b == BIND_NONE && (t=t->base) != NULL);
		return b;
	}

	Binding Traits::findBinding(Stringp name, NamespaceSet* nsset) const
	{
		const Traits* t = this;
		Binding b;
		do
		{
			b = t->get(name, nsset);
		}
		while (b == BIND_NONE && (t=t->base) != NULL);
		return b;
	}

	void Traits::initTables()
	{
		// copy down info from base class
		AvmAssert(!linked);

		MMgc::GC* gc = core->GetGC();

		if (slotCount > 0 || methodCount > 0 || hasInterfaces)
		{
			MMGC_MEM_TYPE(this);

			size_t size = slotCount*sizeof(Traits*) + // slotTypes
						slotCount*sizeof(int) + // slot offsets
						methodCount*sizeof(AbstractFunction*);

			if(hasInterfaces)
				size += IMT_SIZE * sizeof(Binding);

			void *idata = gc->Alloc(size, GC::kZero | GC::kContainsPointers);
			
			WB(gc, this, &instanceData, idata);
		}

		AbstractFunction **methods = getMethods();
		int *offsets = getOffsets();
		Traits **slotTypes = getSlotTypes();
		if (base)
		{
			AvmAssert(base->linked);

			//
			// process the slots: types, offsets, default values
			//
			int firstSlot;
			if ((firstSlot=base->slotCount) > 0)
			{
				// copy base offsets and adjust for sizeofInstance
				AvmAssert(sizeofInstance >= base->sizeofInstance);
				int delta = sizeofInstance - base->sizeofInstance;

				for (int i=0; i < firstSlot; i++)
				{
					WB(gc, instanceData, &slotTypes[i], base->getSlotTypes()[i]);
					offsets[i] = base->getOffsets()[i] + delta;
				}
			}

			int firstMethod;
			if ((firstMethod = base->methodCount) > 0)
			{
				// copy base methods
				for (int i=0; i < firstMethod; i++)
				{
					WB(gc, slotTypes, &methods[i], base->getMethods()[i]);
				}
			}
		}
	}

	void Traits::initMetadataTable() 
	{
		if( !slot_metadata_pos )
		{	
			MMgc::GC* gc = core->GetGC();
			// Lots of things won't have any metadata, so only alloc the space if we really need it
			MMGC_MEM_TYPE(this);
			const byte** idata = (const byte**)gc->Calloc(slotCount+methodCount, sizeof(const byte*), GC::kZero | GC::kContainsPointers);
			WB(gc, this, &slot_metadata_pos, idata);
		}
	}

	void Traits::setIndexedMetadataPos(uint32 i, const byte* pos)
	{
		AvmAssert(i < (slotCount+methodCount) );
		if (!slot_metadata_pos)
			initMetadataTable();
		slot_metadata_pos[i] = pos;
	}

	const byte* Traits::getSlotMetadataPos(uint32 i, PoolObject*& residingPool) const
	{
		AvmAssert( i < slotCount );
		Traits* t = (Traits*)this;
		const byte* pos = 0;
		while(t && !pos && i<t->slotCount)
		{
			if (t->slot_metadata_pos)
			{
				pos = t->slot_metadata_pos[i];
				residingPool = t->pool;
			}
			t = t->base;
		}
		return pos;
	}

	const byte* Traits::getMethodMetadataPos(uint32 i, PoolObject*& residingPool) const
	{
		AvmAssert( i < methodCount );
		Traits* t = (Traits*)this;
		const byte* pos = 0;
		while(t && !pos && i<t->methodCount)
		{
			if (t->slot_metadata_pos)
			{
				pos = t->slot_metadata_pos[t->slotCount+i];
				residingPool = t->pool;
			}
			t = t->base;
		}
		return pos;
	}

	/**
	 * This must be called before any method is verified or any
	 * instances are created.  It is not done eagerly in AbcParser
	 * because doing so would prevent circular type references between
	 * slots of cooperating classes.
     *
     * Resolve the type and position/width of each slot.
	 */
	void Traits::resolveSignatures(const Toplevel* toplevel)
	{
		if (!linked)
		{
			int firstSlot = 0;
			int firstMethod = 0;

			Traitsp* interfaces = getInterfaces();
			// make sure base class and all interfaces are resolved
			for (int i=0, n=interfaceCapacity; i < n; i++)
			{
				Traits* ifc = interfaces[i];
				if (ifc && !ifc->linked)
				{
					ifc->resolveSignatures(toplevel);
				}
			}

			if (base)
			{
				firstSlot = base->slotCount;
				firstMethod = base->methodCount;
			}

			initTables();

			pool->resolveTraits(this, firstSlot, toplevel);

			// make sure all the methods have resolved types
			for (int i=0, n=methodCount; i < n; i++)
			{
				AbstractFunction *f = getMethod(i);
				if (f != NULL)
					f->resolveSignature(toplevel);

				// else, sparse method array, why?
			}

			bool legal = true;
			if (base && base->methodCount > 0)
			{
				// check concrete overrides
                AbstractFunction *virt=NULL;
                AbstractFunction *over=NULL;

				for (int i=0, n=base->methodCount; i < n; i++)
				{
					virt = base->getMethod(i);
					over = getMethod(i);
					if (virt != NULL && virt != over)
						legal &= checkOverride(virt, over);
				}
			}

			if (hasInterfaces && legal && !this->isInterface)
			{
				ImtBuilder imtBuilder(core->GetGC());

				// make sure every interface method is implemented
				for (int i=0, n=interfaceCapacity; i < n; i++)
				{
					Traits* ifc = interfaces[i];
					if (ifc && ifc->isInterface)
					{
						AbstractFunction *virt=NULL;
						AbstractFunction *over=NULL;

						for (int j=ifc->next(0); j != 0; j = ifc->next(j))
						{
							Stringp name = ifc->keyAt(j);
							Namespace* ns = ifc->nsAt(j);
							Binding iBinding = ifc->valueAt(j);

							if (AvmCore::isMethodBinding(iBinding))
							{
								virt = ifc->getMethod(AvmCore::bindingToMethodId(iBinding));

								Binding cBinding = findBinding(name, ns);

								if (!AvmCore::isMethodBinding(cBinding))
								{
									// Try again with public namespace
									Binding pBinding = findBinding(name, core->publicNamespace);

									// If that worked, add a trait.
									if (AvmCore::isMethodBinding(pBinding))
									{
										add(name, ns, pBinding);
										cBinding = pBinding;
									}
								}

								if (!AvmCore::isMethodBinding(cBinding))
								{
									#ifdef AVMPLUS_VERBOSE
									core->console << "\n";
									core->console << "method not implemented " << virt << "\n";
									core->console << "   over-binding " << (int)cBinding << " in " << this << "\n";
									#endif
									over = NULL;
									legal = false;
								}
								else
								{
									int disp_id = AvmCore::bindingToMethodId(cBinding);
									over = getMethod(disp_id);
									if (over != virt)
										legal &= checkOverride(virt, over);
									imtBuilder.addEntry(virt, disp_id);
								}
							}
							else if (AvmCore::isAccessorBinding(iBinding))
							{
								//virtAcc = bindingToAccessor(iBinding)
								Binding cBinding = findBinding(name, ns);

								if (!AvmCore::isAccessorBinding(cBinding))
								{
									// Try again with public namespace
									Binding pBinding = findBinding(name, core->publicNamespace);

									// If that worked, add a trait.
									if (AvmCore::isAccessorBinding(pBinding))
									{
										add(name, ns, pBinding);
										cBinding = pBinding;
									}
								}

								if (!AvmCore::isAccessorBinding(cBinding))
								{
									#ifdef AVMPLUS_VERBOSE
									core->console << "\n";
									core->console << "accessor not implemented " << ns << "::" << name << "\n";
									core->console << "   over-binding " << (int)cBinding << " in " << this << "\n";
									#endif
									over = NULL;
									legal = false;
								}
								else
								{
									// check getter & setter overrides
									if (AvmCore::hasGetterBinding(iBinding))
									{
										virt = ifc->getMethod(AvmCore::bindingToGetterId(iBinding));
										if (!AvmCore::hasGetterBinding(cBinding))
										{
											// missing getter
											#ifdef AVMPLUS_VERBOSE
											core->console << "\n";
											core->console << "getter not implemented " << ns << "::" << name << "\n";
											core->console << "   over-binding " << (int)cBinding << " in " << this << "\n";
											#endif
											over = NULL;
											legal = false;
										}
										else
										{
											int disp_id = AvmCore::bindingToGetterId(cBinding);
											over = getMethod(disp_id);
											if (over != virt)
												legal &= checkOverride(virt,over);
											imtBuilder.addEntry(virt, disp_id);
										}
									}

									if (AvmCore::hasSetterBinding(iBinding))
									{
										virt = ifc->getMethod(AvmCore::bindingToSetterId(iBinding));
										if (!AvmCore::hasSetterBinding(cBinding))
										{
											// missing setter
											#ifdef AVMPLUS_VERBOSE
											core->console << "\n";
											core->console << "setter not implemented " << ns << "::" << name << "\n";
											core->console << "   over-binding " << (int)cBinding << " in " << this << "\n";
											#endif
											over = NULL;
											legal = false;
										}
										else
										{
											int disp_id = AvmCore::bindingToSetterId(cBinding);
											over = getMethod(disp_id);
											if (over != virt)
												legal &= checkOverride(virt,over);
											imtBuilder.addEntry(virt, disp_id);
										}
									}
								}
							}
						}
					}
				}
				imtBuilder.finish(getIMT(), pool);
			}
            if (!legal)
            {
                Multiname qname(ns, name);
                toplevel->throwVerifyError(kIllegalOverrideError, core->toErrorString(&qname), core->toErrorString(this));
            }
			linked = true;
		}
	}

    bool Traits::checkOverride(AbstractFunction* virt, AbstractFunction* over) const
    {
        Traits* overTraits = over->returnTraits();
        Traits* virtTraits = virt->returnTraits();

        if (overTraits != virtTraits)
        {
#ifdef AVMPLUS_VERBOSE
            core->console << "\n";
            core->console << "return types dont match\n";
            core->console << "   virt " << virtTraits << " " << virt << "\n";
            core->console << "   over " << overTraits << " " << over << "\n";
#endif
            return false;
        }

        if (over->param_count != virt->param_count ||
            over->optional_count != virt->optional_count)
        {
#ifdef AVMPLUS_VERBOSE
            core->console << "\n";
            core->console << "param count mismatch\n";
            core->console << "   virt params=" << virt->param_count << " optional=" << virt->optional_count << " " << virt << "\n";
            core->console << "   over params=" << over->param_count << " optional=" << over->optional_count << " " << virt << "\n";
#endif
            return false;
        }

		// allow subclass param 0 to implement or extend base param 0
		virtTraits = virt->paramTraits(0);
		if (!containsInterface(virtTraits) ||
			!isMachineCompatible(this,virtTraits))
		{
			if (!this->isMachineType && virtTraits == core->traits.object_itraits)
			{
				over->flags |= AbstractFunction::UNBOX_THIS;
			}
			else
			{
				#ifdef AVMPLUS_VERBOSE
				core->console << "\n";
				core->console << "param 0 incompatible\n";
				core->console << "   virt " << virtTraits << " " << virt << "\n";
				core->console << "   over " << this << " " << over << "\n";
				#endif
				return false;
			}
		}

        for (int k=1, p=over->param_count; k <= p; k++)
        {
            overTraits = over->paramTraits(k);
            virtTraits = virt->paramTraits(k);
            if (overTraits != virtTraits)
            {
				#ifdef AVMPLUS_VERBOSE
				core->console << "\n";
				core->console << "param " << k << " incompatible\n";
				core->console << "   virt " << virtTraits << " " << virt << "\n";
				core->console << "   over " << overTraits << " " << over << "\n";
				#endif
				return false;
            }
        }

		if (virt->flags & AbstractFunction::UNBOX_THIS)
		{
			// the UNBOX_THIS flag is sticky, all the way down the inheritance tree
			over->flags |= AbstractFunction::UNBOX_THIS;
		}

        return true;
    }

	// static
	bool Traits::isMachineCompatible(const Traits* a, const Traits* b)
	{
		return a == b ||
			// *, Object, and Void are each represented as Atom
			(!a || a == a->core->traits.object_itraits || a == a->core->traits.void_itraits) &&
			(!b || b == b->core->traits.object_itraits || b == b->core->traits.void_itraits) ||
			// all other non-pointer types have unique representations
			(a && b && !a->isMachineType && !b->isMachineType);
	}

#ifdef AVMPLUS_VERBOSE
	Stringp Traits::format(AvmCore* core) const
	{
		if (name != NULL)
			return Multiname::format(core, ns, name);
		else
			return core->concatStrings(core->newString("Traits@"),
									   core->formatAtomPtr((uintptr)this));
	}
#endif

	//
	// implemented traits, organized as a hash-set
	//

	Traitsp* Traits::findInterface(Traits* t) const
	{
		Traitsp* set = this->getInterfaces();
        // this is a quadratic probe
        int n = 7;
		unsigned bitMask = this->interfaceCapacity - 1;

		// Note: Mask off MSB to avoid negative indices.  Mask off bottom
		// 3 bits because it doesn't contribute to hash.

		// [ed] math assumes t is 8-aligned!
		AvmAssert((((uintptr)t) & 7) == 0);

		unsigned i = (((uintptr)t)>>3) & bitMask;
        Traitsp k;
        while ((k=set[i]) != t && k != NULL)
		{
			i = (i + (n++)) & bitMask;				// quadratic probe
		}
        return &set[i];
	}

	void Traits::setSlotInfo(int index, uint32 slot_id, const Toplevel* toplevel, Traits* t, int offset, CPoolKind kind, AbcGen& gen) const
	{
		AvmAssert(t != VOID_TYPE);
		AvmAssert(slot_id < slotCount);
		Traits **slotTypes = getSlotTypes();
		int *offsets = getOffsets();

		// slotTypes[slot_id] = t;
		WB(core->GetGC(), instanceData, &slotTypes[slot_id], t);
		offsets[slot_id] = offset;

		if (!t)
		{
			Atom value = !index ? undefinedAtom : pool->getDefaultValue(toplevel, index, kind, t);

			if(value != 0)
			{
				gen.getlocalN(0);
				if(value == undefinedAtom)
					gen.pushundefined();
				else if (AvmCore::isNull(value))
					gen.pushnull();
				else
					gen.pushconstant(kind, index);
				gen.setslot(slot_id);
			}
		}
		else if (t == OBJECT_TYPE)
		{
			Atom value = !index ? nullObjectAtom : pool->getDefaultValue(toplevel, index, kind, t);
			if (value == undefinedAtom)
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			if(value != 0)
			{
				gen.getlocalN(0);
				if(AvmCore::isNull(value))
					gen.pushnull();
				else
					gen.pushconstant(kind, index);
				gen.setslot(slot_id);
			}
		}
		else if (t == NUMBER_TYPE)
		{
			Atom value = !index ? core->kNaN : pool->getDefaultValue(toplevel, index, kind, t);
			if (!(AvmCore::isInteger(value)||AvmCore::isDouble(value)))
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			if(AvmCore::number_d(value) != 0)
			{
				gen.getlocalN(0);
				if(value == core->kNaN)
					gen.pushnan();
				else
					gen.pushconstant(kind, index);
				gen.setslot(slot_id);
			}
		}
		else if (t == BOOLEAN_TYPE)
		{
			Atom value = !index ? falseAtom : pool->getDefaultValue(toplevel, index, kind, t);
			if (!AvmCore::isBoolean(value))
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			AvmAssert(urshift(falseAtom,3) == 0);
			if(value != falseAtom)
			{
				gen.getlocalN(0);
				gen.pushtrue();
				gen.setslot(slot_id);
			}
		}
		else if (t == UINT_TYPE)
		{
			Atom value = !index ? (0|kIntegerType) : pool->getDefaultValue(toplevel, index, kind, t);
			if (!AvmCore::isInteger(value) && !AvmCore::isDouble(value)) 
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			double d = AvmCore::number_d(value);
			if (d != (uint32)d) 
			{								
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			if(value != (0|kIntegerType))
			{
				gen.getlocalN(0);
				gen.pushconstant(kind, index);
				gen.setslot(slot_id);				
			}
		}
		else if (t == INT_TYPE)
		{
			Atom value = !index ? (0|kIntegerType) : pool->getDefaultValue(toplevel, index, kind, t);
			if (!AvmCore::isInteger(value) && !AvmCore::isDouble(value)) 
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			double d = AvmCore::number_d(value);
			if (d != (int)d)
			{								
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}
		
			if(value != (0|kIntegerType))
			{
				gen.getlocalN(0);
				gen.pushconstant(kind, index);
				gen.setslot(slot_id);				
			}
		}
		else if (t == STRING_TYPE)
		{
			Atom value = !index ? nullStringAtom : pool->getDefaultValue(toplevel, index, kind, t);
			if (!(AvmCore::isNull(value) || AvmCore::isString(value)))
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			if(!AvmCore::isNull(value))
			{
				gen.getlocalN(0);
				gen.pushconstant(kind, index);
				gen.setslot(slot_id);				
			}
		}
		else if (t == NAMESPACE_TYPE)
		{
			Atom value = !index ? nullNsAtom : pool->getDefaultValue(toplevel, index, kind, t);
			if (!(AvmCore::isNull(value) || AvmCore::isNamespace(value)))
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}

			if(!AvmCore::isNull(value))
			{
				gen.getlocalN(0);
				gen.pushconstant(kind, index);
				gen.setslot(slot_id);				
			}
		}
		else
		{
			// any other type: only allow null default value
			Atom value = !index ? nullObjectAtom : pool->getDefaultValue(toplevel, index, kind, t);
			if (!AvmCore::isNull(value))
			{
				Multiname qname(t->ns, t->name);
				toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
			}
		}
	}

#ifdef _DEBUG
	int Traits::interfaceSlop()
	{
		Traitsp *interfaces = getInterfaces();
		int count=0;
		for(int i=0, n=interfaceCapacity; i<n; i++)
		{
			if(interfaces[i] != NULL)
				count++;
		}
		return interfaceCount - count;		
	}
#endif

#ifdef MMGC_DRC
	void Traits::destroyInstance(ScriptObject *obj)
	{
		//TODO: code gen this function

		Hashtable *ht = NULL;
		if(needsHashtable) {
			ht = obj->getTable();
		}
		
		// memset native space to zero except baseclasses
		memset((char*)obj+sizeof(AvmPlusScriptableObject), 0, sizeofInstance-sizeof(AvmPlusScriptableObject));

		// NOTE: this code can't use type macros or core b/c it can run
		// after AvmCore has been destructed
		int *offsets = getOffsets();
		Traitsp *traits = getSlotTypes();
		for(int i=0, n=slotCount; i<n; i++) {
			Traits *t = traits[i];
			// offset is pointed off the end of our object
			AvmAssert(offsets[i] < int(MMgc::GC::Size (obj)));
  			Atom *slot = (Atom*)((char*)obj+offsets[i]);
			if(Traits::canContainPointer(t)) {
				RCObject *rc = NULL;
				if(!t || t->isObjectType) { // atom
					Atom a = *slot;
					int type = a&7;
					//if(type == kObjectType || type == kStringType || type == kNamespaceType)
					AvmAssert(kObjectType == 1 && kStringType == 2 && kNamespaceType == 3);
					if(type <= kNamespaceType)
						rc = (RCObject*)(a&~7);
				} else { // raw pointer
					rc = *(RCObject**)slot;
				}
				if(rc)
					rc->DecrementRef();
				*slot = 0;
			} else { // machineType
				AvmAssert(t->getTotalSize() == 1 || t->getTotalSize() == 4 || t->getTotalSize() == 8);
				if (t->getTotalSize() == 8)
				{
					#ifdef AVMPLUS_64BIT
					*slot = 0;
					#else
					*slot = 0;
					*(slot + 1) = 0;
					#endif
				}
				else // 4 byte slot
				{
					// The Boolean size is 1 but takes up 4 bytes.  (Important on Mac with reverse endian)
					*(uint32 *)slot = 0;
				}
			}
		}

		// do this last, idea is to zero the object in order
		if(ht) {
			ht->destroy();
		}
	}
#endif

	ImtBuilder::ImtBuilder(MMgc::GC *gc)
	{
		this->gc = gc;
		memset(entries, 0, sizeof(ImtEntry*)*Traits::IMT_SIZE);
	}

	void ImtBuilder::addEntry(AbstractFunction* virt, int disp_id)
	{
		int i = virt->iid() % Traits::IMT_SIZE;
#ifdef AVMPLUS_VERBOSE
		if (entries[i] && virt->pool->verbose)
			virt->core()->console << "conflict " << virt->iid() << " " << i << "\n";
#endif
		entries[i] = new (gc) ImtEntry(virt, entries[i], disp_id);
	}

	void ImtBuilder::finish(Binding imt[], PoolObject* pool)
	{
		for (int i=0; i < Traits::IMT_SIZE; i++)
		{
			ImtEntry *e = entries[i];
			if (e == NULL)
			{
				imt[i] = BIND_NONE;
			}
			else if (e->next == NULL)
			{
				// single entry, no conflict
				imt[i] = e->disp_id<<3 | BIND_METHOD;
				gc->Free(e);
			}
			else
			{
				// build conflict stub
				CodegenMIR mir(pool);
				imt[i] = BIND_ITRAMP | (uintptr)mir.emitImtThunk(e);
				AvmAssert((imt[i]&7)==BIND_ITRAMP); // addr must be 8-aligned
			}
		}
	}
}
