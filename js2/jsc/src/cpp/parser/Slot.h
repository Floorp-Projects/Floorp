#ifndef Slot_h
#define Slot_h

/*
 * A slot.
 */

namespace esc {
namespace v1 {

class Value;
class ObjectValue;

struct Slot {
    int         attrs;
    TypeValue*      type;
	union {
		Value*       value;
		ObjectValue* objValue; // (ReferenceValue, ListValue, ObjectValue)
		int          intValue;
		char*        strValue;
    };

	Slot() : attrs(0), type(0), value(0) {
	}
	//Block block;
	//Store store;
};

}
}

#endif // Slot_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
