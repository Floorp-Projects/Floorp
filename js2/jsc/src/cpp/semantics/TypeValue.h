#ifndef TypeValue_h
#define TypeValue_h

/**
 * The interface for all types.
 */

#include <vector>

#include "Builder.h"
#include "ObjectValue.h"

namespace esc {
namespace v1 {

class TypeValue : public ObjectValue, public Type, public Builder {
	static bool isInitialized;
public:

	ObjectValue* prototype;

    virtual bool includes(Context& cx, Value* value);
    virtual void build(Context& cx, ObjectValue* ob);
	TypeValue* super(Context& cx);

	TypeValue() {
		this->prototype = new ObjectValue();
	}

	static TypeValue* booleanType;
	static TypeValue* namespaceType;
	static TypeValue* objectType;
	static TypeValue* typeType;

#if 0
	virtual std::vector<Value*> values() {};
	virtual std::vector<Value*> converts() {};
    virtual void   addSub(Type& type) {};
    virtual void   setSuper(Type& type) {};
    virtual Type*  getSuper(){};
    virtual Value* convert(Context& cx, Value& value) {};
    virtual Value* coerce(Context& cx, Value& value) {};
#endif

	static void init();
	static void fini();

};

}
}

#endif // TypeValue_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
