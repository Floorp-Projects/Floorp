#ifndef Value_h
#define Value_h

/*
 * The value class from which all other values derive. Immediate
 * children are ObjectValue, ReferenceValue, ListValue, and
 * CompletionValue.
 */

namespace esc {
namespace v1 {

class Context;
class TypeValue;

class Value {
public:
	virtual Value* getValue(Context& context) { return this; }
    virtual TypeValue* getType(Context& context) { return (TypeValue*)0; }
};

}
}

#endif // Value_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
