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

#ifndef __avmplus_Traits__
#define __avmplus_Traits__


namespace avmplus
{
	/**
	 * Traits objects describe the layout of objects.  Traits are
	 * used to describe a variety of things in the VM, such as
	 * classes, global objects, and activation objects.
	 *
	 * A Traits object contains a hash table with the names of
	 * all properties and methods.  Each name maps to a binding.
	 *
	 * There are several types of binding: binding to a slot,
	 * binding to an accessor, binding to a virtual function,
	 * binding to a final function.
	 */
	class Traits : public MultinameHashtable
	{
		friend class VTable;
	public:
		AvmCore* const core;

		/**
		 * Pointer to the base traits; that is, the traits
		 * of the base class.
		 */
		DWB(Traits*) base;
 
		/** The name of the class described by this traits object. */
		DRCWB(Namespace*) ns;
		DRCWB(Stringp) name;

		/**
		  * If true, the class needs a hash table.  Typically true for dynamic
		  * classes, but will be false for XML since they store dynamic props
		  * differently
		  */
		bool needsHashtable:1;
		
		/** set once signature types have been resolved */
		bool linked:1;

		/** set when the class cannot be extended */
		bool final:1;

		/** true for types whose values are not pointers */
		bool isMachineType:1;

		/** true for types that are interfaces */
		bool isInterface:1;

		/** used for Verify::findCommonBase */
		bool commonBase:1;

		/** used in verify/mir, true for int, uint, Number */
		bool isNumeric:1;

		/** used to tell us whether we need to allocate space for the IMT */
		bool hasInterfaces:1;

		/** true if this is NOT a derived object class. */
		/** Also true if this is an XML or XMLList object. */
		bool notDerivedObjectOrXML:1;

		/** for this == OBJECT_TYPE where we can't use OBJECT_TYPE */
		bool isObjectType:1;

		/** how we implement dictionary or strict style lookups */
		bool isDictionary:1;

		/**
		* if this type is a factory, itraits is non-null and points
		* to the type created by this factory.
		*/
		Traits* itraits;

		// choose a number that is relatively prime to sizeof(AbstractFunction)/8
		// since we use the AbstractFunction pointer as the interface method id
		// smaller = dense table, few large conflict stubs
		// larger  = sparse table, many small conflict stubs 

#ifdef _DEBUG
		static const int IMT_SIZE = 3;  // good for testing all code paths
#else
		static const int IMT_SIZE = 7;  // good for performance
#endif

		const byte* getTraitsPos() const {
			AvmAssert(!linked);
			return traits_pos;
		}

		void setTraitsPos(const byte* pos) {
			AvmAssert(!linked);
			traits_pos = pos;
		}

		size_t getTotalSize() const { 
			AvmAssert(linked);
			return totalSize;
		}

		void setTotalSize(size_t _size) { 
			totalSize = _size;
		}

		void setMetadataPos(const byte* pos) {
			metadata_pos = pos;
		}

		const byte* getMetadataPos() const {
			return metadata_pos;
		}

		void setSlotMetadataPos(uint32 i, const byte* pos)								
		{ AvmAssert(i < slotCount);   setIndexedMetadataPos(i, pos); }
		void setMethodMetadataPos(uint32 i, const byte* pos)							
		{ AvmAssert(i < methodCount); setIndexedMetadataPos(i+slotCount, pos); }
		const byte* getSlotMetadataPos(uint32 i, PoolObject*& residingPool) const;
		const byte* getMethodMetadataPos(uint32 i, PoolObject*& residingPool) const;

		static bool canContainPointer(Traits *t) {
			return !t || !t->isMachineType || t->isObjectType;
		}

		// not a call/init union b/c smart pointers and union's don't mix
		DWB(AbstractFunction*) init;		

	private:

		void initMetadataTable();
		void setIndexedMetadataPos(uint32 i, const byte* pos);

		// in resolveTraits we read traits_pos for the last time and at the end set totalSize
		union {
			const byte* traits_pos;
			/** total size of instance including native size and slots size */
			size_t totalSize;
		};


		/** chunk of memory allocated after traits resolution holding slotTypes, offsets and methods */
		void *instanceData;

		const byte* metadata_pos;

		const byte** slot_metadata_pos;

	public:

		// table of interface dispatch stubs.
		// BIND_NONE   = no entry
		// BIND_METHOD+disp_id = no conflict, dispatches to concrete method
		// BIND_ITRAMP+addr    = conflict, dispatch to conflict resolution stub
		// IMT table (if we have one, comes after the interfaces)
		Binding *getIMT() const {
			AvmAssert(hasInterfaces);
			return (Binding*) (getMethods() + methodCount);
		}

		AbstractFunction **getMethods() const { return instanceData ? (AbstractFunction**) (getOffsets() + slotCount) : NULL; }
		Traits **getSlotTypes() const { return (Traits**) instanceData; }
		int *getOffsets() const { return instanceData ? (int*) ((Traits **) instanceData + slotCount) : NULL; }

		/** # of methods in the methods array */
		uint32 methodCount;
		
		/** # of slots in the slots array */
		uint32 slotCount;         
		
		/** # of traits (interfaces and base classes) in the interfaces array */
		uint32 interfaceCount;

		/** size of interfaces array */
		uint32 interfaceCapacity;

		/** offset of hash table for dynamic objects, -1 if invalid */
		int hashTableOffset;

		/**
		 * sizeof implementation class,
		 * e.g. ScriptObject, etc.  Not counting extra room for slots
		 */
		size_t sizeofInstance;

		/**
		 * The constant pool owning this definition. NULL for internal types.
		 */
		DWB(PoolObject*) pool;

		/** scope chain types */
		DWB(ScopeTypeChain*) scope;

		/** protected namespace, if any */
		DRCWB(Namespace*) protectedNamespace;
		
		/**
		 * Constructor for traits.  base != NULL means inheritance.
		 */
		Traits(AvmCore *core,
			   Traits *base,
			   int nameCount,
			   int interfaceCount,
			   int interfaceCapacity,
			   size_t sizeofInstance);
		
		virtual ~Traits();

		/**
		 * Defines a slot on this traits object.
		 */
		void defineSlot(Stringp _name, Namespace* _ns, int _slot, Binding _kind)
		{
			add(_name, _ns, _slot<<3|_kind);
		}
		
		Binding findBinding(Stringp name, Namespace* ns) const;
		Binding findBinding(Stringp name, NamespaceSet* ns) const;
		Binding getOverride(Namespace* ns, Stringp name, int tag, const Toplevel *toplevel) const;

		void resolveSignatures(const Toplevel* toplevel);
		void initTables();
		bool checkOverride(AbstractFunction* virt, AbstractFunction* over) const;

		/**
		 * Adds the traits object t to the list of interfaces
		 * implemented by objects described by this traits object.
		 */
		void addInterface(Traits* t);

		/**
		 * Returns whether this traits object implements the
		 * interface described by traits t.
		 */
		int containsInterface(Traits* t) const
		{
			return this == t || *(findInterface(t)) != NULL;
		}

		/**
		 * Returns the type of the slot identified by slot.
		 * @param slot slot to retrieve type of, zero-indexed
		 */
		Traits* getSlotTraits(uint32 i) const 
		{
			//AvmAssert(linked);
			AvmAssert(i < slotCount);
			return getSlotTypes()[i];
		}
		

		void setSlotInfo(int index, uint32 slot_id, const Toplevel* toplevel,
			Traits* slotType, int offset, CPoolKind kind, AbcGen& gen) const;


		static bool isMachineCompatible(const Traits* a, const Traits* b);

		void setNativeClassInfo(NativeClassInfo* entry) { this->nativeClassInfo = entry; }
		void setNativeScriptInfo(NativeScriptInfo* entry) { this->nativeScriptInfo = entry; }

		NativeClassInfo* getNativeClassInfo() const { return nativeClassInfo; }
		NativeScriptInfo* getNativeScriptInfo() const { return nativeScriptInfo; }

		AbstractFunction* getMethod(uint32 i) const
		{
			AvmAssert(i < methodCount);
			AbstractFunction **_methods = getMethods();
			return _methods ? _methods[i] : NULL;
		}

		void setMethod(uint32 i, AbstractFunction *method)
		{
			AvmAssert(i < methodCount && getMethods() != NULL);
			//methods[i] = method;
			WB(core->GetGC(), instanceData, &getMethods()[i], method);
		}


		/** Interfaces supported by objects described by this traits. */
		Traitsp* getInterfaces() const { return (Traitsp*)((char*)this+sizeof(Traits)); } 

#ifdef _DEBUG
		int interfaceSlop();
#endif

#ifdef MMGC_DRC
		void destroyInstance(ScriptObject *obj);
#endif

	private:
		union {
			NativeClassInfo* nativeClassInfo;
			NativeScriptInfo* nativeScriptInfo;
		};

		Traitsp* findInterface(Traits* t) const;


#ifdef AVMPLUS_VERBOSE
	public:
		virtual Stringp format(AvmCore* core) const;
#endif

		Traits* getInterface(uint32 i) const
		{
			AvmAssert(i < interfaceCapacity);
			return getInterfaces()[i];
		}

	};

	class ImtBuilder
	{
	public:
		class ImtEntry: public MMgc::GCObject
		{
		public:
			ImtEntry(AbstractFunction* v, ImtEntry* n, int d)
				: virt(v), next(n), disp_id(d) {}
			AbstractFunction * const virt;
			ImtEntry * const next;
			const int disp_id;
		};

		ImtBuilder(MMgc::GC *gc);
		void addEntry(AbstractFunction* virt, int disp_id);
		void finish(Binding imt[], PoolObject* pool);

	private:
		MMgc::GC *gc;
		ImtEntry *entries[Traits::IMT_SIZE];
	};
}

#endif /* __avmplus_Traits__ */
