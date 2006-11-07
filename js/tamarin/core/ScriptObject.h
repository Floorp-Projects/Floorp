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

#ifndef __avmplus_ScriptObject__
#define __avmplus_ScriptObject__


namespace avmplus
{
#ifdef DEBUGGER
	class GCHashtableScriptObject;
#endif
	/**
	 * one actionscript object.  might or might not be a function.
	 * Base class for all objects visible to script code.
	 */
	class ScriptObject : public AvmPlusScriptableObject
	{
	private:
		#ifdef AVMPLUS_MIR
		friend class CodegenMIR;
		#endif

	public:
		VTable* const vtable;
		
		ScriptObject(VTable* vtable, ScriptObject* delegate,
					 int capacity = Hashtable::kDefaultCapacity);
		~ScriptObject();

		ScriptObject* getDelegate() const { return delegate; }
		void setDelegate(ScriptObject *d) { WBRC(MMgc::GC::GetGC(this), this, &delegate, d); }

		Atom atom() const {
			return kObjectType|(int)this; 
		}

		virtual Atom toAtom() const {
			return atom();
		}

		Traits* traits() const {
			return vtable->traits;
		}

		AvmCore* core() const {
			return vtable->traits->core;
		}

		MMgc::GC* gc() const {
			return core()->GetGC();
		}

		Toplevel* toplevel() const {
			return vtable->toplevel;
		}

		DomainEnv* domainEnv() const {
			return vtable->abcEnv->domainEnv;
		}

		virtual Hashtable* getTable() const {
			AvmAssert(vtable->traits->hashTableOffset != -1);
			return (Hashtable*)((byte*)this+vtable->traits->hashTableOffset);
		}

		bool isValidDynamicName(Multiname* m) const {
			return m->contains(core()->publicNamespace) && !m->isAnyName() && !m->isAttr();
		}
		
		Atom getSlotAtom(int slot);
		void setSlotAtom(int slot, Atom atom);

		virtual Atom getProperty(Multiname* name) const;
		virtual void setProperty(Multiname* name, Atom value);
		virtual bool deleteProperty(Multiname* name);
		virtual bool hasProperty(Multiname* name) const;

		virtual Atom getDescendants(Multiname* name) const;

		// argv[0] = receiver
		virtual Atom callProperty(Multiname* name, int argc, Atom* argv);
		virtual Atom constructProperty(Multiname* name, int argc, Atom* argv);

		virtual Atom getProperty(Atom name) const;
		virtual Atom getProperty(Stringp name) const {
			return getProperty(name->atom());
		}

		virtual bool hasProperty(Atom name) const;
		virtual bool hasProperty(Stringp name) const 
		{
			AvmAssert(name != NULL && name->isInterned());
			return hasProperty(name->atom());
		}

		virtual bool propertyIsEnumerable(Atom name) const;
		virtual bool propertyIsEnumerable(Stringp name) const
		{
			AvmAssert(name != NULL && name->isInterned());
			return propertyIsEnumerable(name->atom());
		}

		virtual void setPropertyIsEnumerable(Atom name, bool enumerable);
		virtual void setPropertyIsEnumerable(Stringp name, bool enumerable)
		{
			setPropertyIsEnumerable(name->atom(), enumerable);
		}
		
		virtual bool deleteProperty(Atom name);
		virtual bool deleteProperty(Stringp name) {
			return deleteProperty(name->atom());
		}

		virtual void setProperty(Atom name, Atom value);
		virtual void setProperty(Stringp name, Atom value) {
			setProperty(name->atom(), value);
		}

		Atom getPropertyFromProtoChain(Atom name, ScriptObject* protochain, Traits *origObjTraits) const;
		Atom getPropertyFromProtoChain(Stringp name, ScriptObject* protochain, Traits *origObjTraits) const
		{
			return getPropertyFromProtoChain(name->atom(), protochain, origObjTraits);
		}

		virtual Atom getUintProperty(uint32 i) const;
		virtual void setUintProperty(uint32 i, Atom value);
		virtual bool delUintProperty(uint32 i);
		virtual bool hasUintProperty(uint32 i) const;

		virtual Atom defaultValue();		// ECMA [[DefaultValue]]
		virtual Atom toString();
		
		// argv[0] = receiver
		virtual Atom call(int argc, Atom* argv);

		// argv[0] = receiver(ignored)
		virtual Atom construct(int argc, Atom* argv);

		// TODO make this const
		virtual Atom nextName(int index);

		// TODO make this const
		virtual Atom nextValue(int index);

		// TODO make this const
		virtual int nextNameIndex(int index);

		// HACK; we need this so we can compare MethodClosures that
		// have the same instance and method.
		virtual bool isMethodClosure() { return false; }

		virtual ScriptObject* createInstance(VTable* ivtable, ScriptObject* prototype);

		Atom function_call(Atom thisAtom, Atom *argv, int argc);
		Atom function_apply(Atom thisAtom, Atom argArray);
		
		// The maximum integer key we can use with our ScriptObject
		// HashTable must fit within 28 bits.  Any integer larger
		// than 28 bits will use a string key.
		static const int MAX_INTEGER_MASK = 0xF0000000;

#ifdef AVMPLUS_VERBOSE
	public:
		virtual Stringp format(AvmCore* core) const;
#endif
	private:
        ScriptObject* delegate;     // __proto__ in AS2, archetype in semantics
		
#ifdef DEBUGGER
		DRCWB(GCHashtableScriptObject*) instances;
	public:
		void addInstance(Atom a);
		void removeInstance(Atom a);
		ScriptObject *getInstances();
		ScriptObject *getSlotIterator();
		uint32 size() const;
#endif
	};
}

#endif /* __avmplus_ScriptObject__ */
