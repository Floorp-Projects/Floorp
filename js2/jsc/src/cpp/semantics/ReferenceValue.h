/*
 * ReferenceValue
 */

#ifndef ReferenceValue_h
#define ReferenceValue_h

#pragma warning( disable : 4786 )

#include <string>
#include <vector>

#include "Value.h"

namespace esc {
namespace v1 {

struct Slot;
class ObjectValue;
class Context;

class ReferenceValue : public Value {

	ObjectValue* base;
	Value* qualifier;
	std::vector<Value*>* used_namespaces;
	Slot* slot;
	int slot_index;
	int scope_index;


public:
	std::string  name;

	ReferenceValue(ObjectValue* base, std::string name, Value* qualifier);
	ReferenceValue(ObjectValue* base, std::string name, std::vector<Value*>* used_namespaces);
	bool lookup(Context& cx);
	bool lookupUnqualified(Context& cx);
	Slot* getSlot(Context& cx);
	Value* getValue(Context& cx);
	int getSlotIndex() { return slot_index; }
	int getScopeIndex() { return scope_index; }
	ReferenceValue* setBase(ObjectValue* base) { this->base = base; return this; }

	// main

    static int main(int argc, char* argv[]);
};

}
}

#endif // ReferenceValue_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
