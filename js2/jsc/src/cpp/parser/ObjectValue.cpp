/*
 * The value class from which all other values derive.
 */

#pragma warning ( disable : 4786 )

#include <string>
#include <map>
#include <algorithm>

#include "Context.h"
#include "Value.h"
#include "Type.h"
#include "Slot.h"

#include "ObjectValue.h"
#include "ReferenceValue.h"
#include "TypeValue.h"

namespace esc {
namespace v1 {

	ObjectValue* ObjectValue::undefinedValue;
    ObjectValue* ObjectValue::nullValue;
	ObjectValue* ObjectValue::trueValue;
	ObjectValue* ObjectValue::falseValue;
	ObjectValue* ObjectValue::publicNamespace;
	ObjectValue* ObjectValue::objectPrototype;

	Slot default_slot;

	void ObjectValue::init() {
		ObjectValue::undefinedValue  = new ObjectValue();
		ObjectValue::nullValue       = new ObjectValue();
		ObjectValue::trueValue       = new ObjectValue();
		ObjectValue::falseValue      = new ObjectValue();
		ObjectValue::publicNamespace = new ObjectValue(TypeValue::namespaceType);
		ObjectValue::objectPrototype = new ObjectValue();
	}
	
	void ObjectValue::fini() {
	}
	
	ObjectValue::ObjectValue() 
		: names(*new std::map<std::string,Qualifiers>()),
		  slots(*new std::vector<Slot>(3,default_slot)),
		  dyna_slots(*new std::vector<Slot>),
		  frozen_flag(false),
		  first_index(0) {

		initInstance(0,0);
	}

	ObjectValue::ObjectValue(Builder& builder) 
		: names(*new std::map<std::string,Qualifiers>()),
		  slots(*new std::vector<Slot>(3,default_slot)),
		  dyna_slots(*new std::vector<Slot>),
		  frozen_flag(false),
		  first_index(0) {

		initInstance(0,TypeValue::objectType);

		Context cx;
		builder.build(cx,this);
	}

	ObjectValue::ObjectValue(bool value) 
		: names(*new std::map<std::string,Qualifiers>()),
		  slots(*new std::vector<Slot>(3,default_slot)),
		  dyna_slots(*new std::vector<Slot>),
		  frozen_flag(false),
		  first_index(0) {

		initInstance(TypeValue::booleanType->prototype,TypeValue::booleanType);

		Context& cx = *new Context();
		Slot* slot     = getSlot(cx,first_index+SLOT_VALUE);
		slot->intValue = value;
		slot->type     = TypeValue::booleanType;
		slot->attrs    = 0;
	}

	// Construct an object with a given prototype object.

	ObjectValue::ObjectValue(ObjectValue* protoObject) 
		: names(*new std::map<std::string,Qualifiers>()),
		  slots(*new std::vector<Slot>(3,default_slot)),
		  dyna_slots(*new std::vector<Slot>),
		  frozen_flag(false),
		  first_index(protoObject->getLastSlotIndex()) {

		initInstance(protoObject,TypeValue::objectType);
	}

	// Construct an object with a class defined.

	ObjectValue::ObjectValue(TypeValue* classObject) 
		: names(*new std::map<std::string,Qualifiers>()),
		  slots(*new std::vector<Slot>(3,default_slot)),
		  dyna_slots(*new std::vector<Slot>),
		  frozen_flag(false),
		  first_index(classObject->prototype->getLastSlotIndex()) {

		initInstance(classObject->prototype,classObject);
	}

	void ObjectValue::initInstance(ObjectValue* protoObject,TypeValue* classObject) {
		
		Context& cx = *new Context();
		
		Slot* slot;
		slot = getSlot(cx,first_index+SLOT_PROTO);
		slot->value = protoObject;
		slot->type  = TypeValue::objectType;
		slot->attrs = 0;
		slot = getSlot(cx,first_index+SLOT_CLASS);
		slot->value = classObject;
		slot->type  = TypeValue::typeType;
		slot->attrs = 0;

		// Call the class object's builder method to initialize
		// this instance.

		if( classObject ) {
			classObject->build(cx,this);
		}
	}

	/*
	 * Scope methods
	 */

    /*
	 * Look for a property name in the object.
	 */

	bool ObjectValue::hasName(Context& cx, std::string name, Value* qualifier) {

		Names::iterator nameit = names.find(name);
        
		if( nameit == names.end() ) {
			return (bool)0;
		}

		Qualifiers& qual = (*nameit).second;
		
		return qual.find(qualifier) != qual.end();
	}

    Namespaces* ObjectValue::hasNames(Context& cx, std::string name, Namespaces* namespaces) {

		Namespaces* hasNamespaces = new Namespaces();
		Names::iterator nameit = names.find(name);
        
		if( nameit == names.end() ) {
			return (Namespaces*)0;
		}

		Qualifiers* quals = &(*nameit).second;

		// For each member of namespaces, see if there
		// is a matching qualifier.
		
		for(int i = 0; i < namespaces->size(); i++ ) {
			Value* qual = namespaces->at(i);
			if( quals->find(qual) != quals->end() ) {
				hasNamespaces->push_back(qual);
			}
		}
		return hasNamespaces;
	}

    /*
	 * Get the slot for a property.
	 *
	 * WARNING:
	 * Before calling this method you must call hasName to ensure that
	 * the requested property exists.
	 */

	Slot* ObjectValue::get(Context& cx, std::string name, Value* qualifier) {

		std::map<Value*,int>& prop = names[name];
		int index = prop[qualifier];
		return getSlot(cx,index);
	}

    /*
	 * Set the value of a property.
	 */

	int ObjectValue::define(Context& cx, std::string name, Value* qualifier, int index) {

		// Get the property for name

		std::map<std::string,Qualifiers>::iterator nameit = names.find(name);
	
		// If there is one, then get the qualifier map for that name.
		// Otherwise, create a qualifier map and a new property.

		if( nameit == names.end() ) {
			Qualifiers qualifiers;
			names[name] = qualifiers;
		}

		// Add the qualifier to the qualifiers map, and set its value to index.
		
		std::map<Value*,int>& prop = names[name];
		prop[qualifier] = index;
		return index;
	}


	/*
	 * Add a slot to this object. This object's fixed slots
	 * (that is, all slots added before freezeSlots is called)
	 * begin where the [[Prototype]] indexes end.
	 *
	 * If [[Prototype]] object has not been frozen or this
	 * object has been frozen, then addSlot allocates the
	 * slot in the dynamic slot space.
	 */
	
	int ObjectValue::addSlot(Context& cx) {
		Slot slot;
		int index;
		if( proto(cx) && proto(cx)->isFixed() && !this->isFixed() ) {
			index = first_index + slots.size();
			slots.push_back(slot);
		} else if( !this->isFixed() ) {
			index = slots.size();
			slots.push_back(slot);
		} else {
			index = -dyna_slots.size();
			dyna_slots.push_back(slot);
		}
		return index;
	}

	/*
	 * Get the slot for an index. If it is a fixed slot
	 * it will have a positive value. dynamic slots have
	 * negative values.
	 */

	Slot* ObjectValue::getSlot(Context& cx, int index) {
		Slot* slot;
		if( ((unsigned)index) < first_index ) {
			slot = proto(cx)->getSlot(cx,index);
		} else if (index < 0) {
			slot = &dyna_slots[-index];
		} else {
     		slot = &slots[index-first_index];
		}
		return slot;
	}

	/*
	 * Get the slot index of a named property.
	 *
	 * Before calling this method you must call hasName to ensure that
	 * the requested property exists.
	 */
	
	int ObjectValue::getSlotIndex(Context& cx, std::string name, Value* qualifier) {
		std::map<Value*,int>& prop = names[name];
		int size = prop.size();
		int index = prop[qualifier];
		return index;
	}

	bool ObjectValue::isFixed() { 
		return frozen_flag; 
	}

	int ObjectValue::getFirstSlotIndex() { 
		return first_index; 
	}

	int ObjectValue::getLastSlotIndex() { 
		frozen_flag = true; 
		return first_index+slots.size()-1; 
	}

	ObjectValue* ObjectValue::proto(Context& cx) {
		return this->getSlot(cx,first_index+SLOT_PROTO)->objValue;
	}
	
	TypeValue* ObjectValue::getType(Context& cx) {
		return reinterpret_cast<TypeValue*>(this->getSlot(cx,first_index+SLOT_CLASS)->objValue);
	}
	
	/*
	 * Test driver
	 */

    int ObjectValue::main(int argc, char* argv[]) {
		Context& cx = *new Context();
		ObjectValue ob;
		ObjectValue q1,q2,q3;

		int i1,i2,i3;
		i1 = ob.addSlot(cx);				// add a slot
		i1 = ob.define(cx,"a",&q1,i1);	// define a property
		i2 = ob.addSlot(cx);
		i2 = ob.define(cx,"a",&q2,i2);
		i3 = ob.addSlot(cx);
		i3 = ob.define(cx,"a",&q3,i2);


		//ObjectValue v1,v2,v3;

		ob.get(cx,"a",&q1)->intValue = 10;
		ob.get(cx,"a",&q2)->intValue = 20;
		int v3 = ob.get(cx,"a",&q3)->intValue;

		Slot* s1 = ob.getSlot(cx,i1);
		Slot* s2 = ob.getSlot(cx,i2);
		Slot* s3 = ob.getSlot(cx,i3);

		ObjectValue b((bool)1);

		delete &cx;

		testClass(argc,argv);

		return 0;
	}

	// Test the case:
	// class B { static var xg = 1; var xp = 2; }
	// var b = new B;
	// b.xi = 3;
	// b.class.xg;
	// b.xp;
	// b.xi;

    int ObjectValue::testClass(int argc, char* argv[]) {

		Context cx;;

		// Allocate the parts
		
		ObjectValue global;

		ObjectValue default_ns;

		// Wire them together

		int slot_index;
		Slot* slot;

		// Init the proto object
		
		ObjectValue b_proto;
		slot_index = b_proto.addSlot(cx);				// add a slot
		slot_index = b_proto.define(cx,"xp",&default_ns,slot_index); // define a property
		slot       = b_proto.getSlot(cx,slot_index);
		slot->intValue = 10;

		// Init the class object

		TypeValue b_class;
		slot_index = b_class.addSlot(cx);				// add a slot
		slot_index = b_class.define(cx,"xg",&default_ns,slot_index); // define a property
		slot = b_class.getSlot(cx,slot_index);
		slot->intValue = 30;

		// Init the instance object
		
		ObjectValue b_instance(b_proto);
		int first_index = b_instance.getFirstSlotIndex();
		slot        = b_instance.getSlot(cx,first_index+SLOT_PROTO);
		slot->value = &b_proto;
		slot->type  = TypeValue::objectType;
		slot->attrs = 0;

		slot_index  = b_instance.define(cx,"class",&default_ns,first_index+SLOT_CLASS); // define a property
		slot        = b_instance.getSlot(cx,first_index+SLOT_CLASS);
		slot->value = &b_class;
		slot->type  = TypeValue::typeType;
		slot->attrs = 0;

		slot_index = b_instance.addSlot(cx);				// add a slot
		slot_index = b_instance.define(cx,"xi",&default_ns,slot_index); // define a property
		slot = b_instance.getSlot(cx,slot_index);
		slot->intValue = 20;

		slot_index = global.addSlot(cx);				// add a slot
		slot_index = global.define(cx,"b",&default_ns,slot_index); // define a property
		slot = global.getSlot(cx,slot_index);
		slot->objValue = &b_instance;

		// Make and evaluate some references

		
		// b.xp

		ReferenceValue r1(&b_instance,"xp",&default_ns);
		Slot* sp;
		sp = r1.getSlot(cx);
		int sp_index = r1.getSlotIndex();
		sp = b_instance.getSlot(cx,sp_index);

		// b.xi

		ReferenceValue r2(&b_instance,"xi",&default_ns);
		Slot*  si = r2.getSlot(cx);

		// b.class.xg

		ReferenceValue r3(&b_instance,"class",&default_ns);
		ObjectValue& vB = *((ObjectValue*)r3.getValue(cx));
		ReferenceValue r4(&vB,"xg",&default_ns);
		Slot*  sg = r4.getSlot(cx);

		return 0;
	}
}
}

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
