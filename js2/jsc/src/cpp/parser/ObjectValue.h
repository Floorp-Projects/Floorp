/*
 * This class is the building block for all ECMA values. Object values 
 * are a sequence of instances linked through the [[Prototype]] slot. 
 * Classes are a sequence of instances of the sub-class TypeValue, also
 * linked through the [[Prototype]] slot. Lookup of class and instance 
 * properties uses the same algorithm to find a name. Names are bound
 * to slots. Slots contain values that are methods for accessing or 
 * computing program values (i.e running code.)
 *
 * An object consists of a table of names, and a vector of slots. The
 * slots hold methods for accessing and computing values. An object 
 * also has a private data accessible to the methods and accessors of
 * the object.
 *
 * All symbolic references to the properties of an object are bound
 * to the method of a slot. Constant references to fixed, final prop-
 * erties can be compiled to direct access of the data value. This
 * is the case with access to private, final or global variables.
 * 
 * The instance and class hierarchies built at compile-time can be
 * compressed into a single instance prototype and class object for 
 * faster lookup and dispatch.
 */

#ifndef ObjectValue_h
#define ObjectValue_h

#pragma warning( disable : 4786 )

#include <map>
#include <vector>
#include <string>

#include "Value.h"
#include "Type.h"
#include "Slot.h"

namespace esc {
namespace v1 {

class Context;
class TypeValue;
class Builder;

	enum { 
		SLOT_PROTO = 0,
		SLOT_CLASS,
		SLOT_VALUE,
	};

typedef std::map<Value*,int>             Qualifiers;
typedef std::map<std::string,Qualifiers> Names;
typedef std::vector<Slot>                Slots;
typedef std::vector<Value*>              Namespaces;

class ObjectValue : public Value {
private:

	Names names;  // Names table
	Slots slots;  // Slots table
	Slots dyna_slots;  // Slots table
	bool  frozen_flag;
	int   first_index;

public:

	static ObjectValue* undefinedValue;
	static ObjectValue* nullValue;
	static ObjectValue* trueValue;
	static ObjectValue* falseValue;
	static ObjectValue* publicNamespace;
	static ObjectValue* objectPrototype;

	ObjectValue(); 
	ObjectValue(ObjectValue* protoObject);
	ObjectValue(TypeValue* classObject);
	ObjectValue(Builder& builder);
	ObjectValue(bool value);

	void ObjectValue::initInstance(ObjectValue* protoObject,TypeValue* classObject);

	virtual ~ObjectValue() {
		//names.clear();
		//slots.clear();
		//dyna_slots.clear();
	}

	/* Initialize and finalize the class.
	 */
	static void init();
	static void fini();

	/* Define a property and associate it with slot_index
	 */
    int define(Context& cx, std::string name, Value* qualifier, int slot_index);

	/* Get the slot index of a property name
	 */
    int getSlotIndex(Context& cx, std::string name, Value* qualifier);

	/* Get the slot of a property name
	 */
    Slot* get(Context& cx, std::string name, Value* qualifier);

	/* Check for a property
	 */
    bool  hasName(Context& cx, std::string name, Value* qualifier);

	/* Check for names that consist of an id and one of multiple
	 * namespaces. Return a vector of namespaces that match. This
	 * method is used for lookup of unqualified names.
	 */
    Namespaces* hasNames(Context& cx, std::string name, Namespaces* namespaces);

	/* Add a slot to this object.
	 */
	int   addSlot(Context& cx);

	/* Get a slot for a slot index
	 */
	Slot* getSlot(Context& cx, int index);

	/* Check if the object is fixed yet.
	 */
	bool isFixed();

    /* Get the start index for slots in this object. The indicies of
	 * all the objects in a prototype chain are contiguous.
	 */
	int getFirstSlotIndex();

	/* Get the last slot index allocated for this part of the
	 * instance. Calling this method has the side-effect of
	 * freezing the slot table. Thereafter, new slots are
	 * allocated in the dynamic slots table.
	 */
	int getLastSlotIndex();

	/* Get the [[Prototype]] object
	 */
	ObjectValue* proto(Context& cx);
    
	/* Get the [[Type]] object
	 */
	virtual TypeValue* getType(Context& context);

	// main

    static int main(int argc, char* argv[]);
    static int testClass(int argc, char* argv[]);

};

}
}

#endif // ObjectValue_h

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
