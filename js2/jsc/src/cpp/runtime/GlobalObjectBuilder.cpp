/*
 * Global object builder
 */

#pragma warning ( disable : 4786 )

#include <string>
#include <map>
#include <algorithm>

#include "Context.h"
#include "Value.h"
#include "Type.h"
#include "Slot.h"
#include "Builder.h"
#include "GlobalObjectBuilder.h"

#include "TypeValue.h"

namespace esc {
namespace v1 {


	void GlobalObjectBuilder::build(Context& cx, ObjectValue* ob) {

        Slot* slot;
		int slot_index;

		// Never type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Never",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// Null type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Null",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// Boolean type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Boolean",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// Number type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Number",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// String type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"String",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// Function type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Function",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// Array type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Boolean",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// Type type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Array",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// Unit type
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"Unit",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->objValue = new TypeValue();

		// public namespace
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"public",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->value = ObjectValue::publicNamespace;

		// print - (built-in) function
		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"print",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->value = new ObjectValue();  // function: print

		// version - (built-in) field 

		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"get version",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->intValue = 7; //value_count;		// function: get version

		slot_index = ob->addSlot(cx);
		slot_index = ob->define(cx,"set version",ObjectValue::publicNamespace,slot_index);
		slot = ob->getSlot(cx,slot_index);
		slot->intValue = 7; //value_count++;  // function: set version

	}

}
}

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
