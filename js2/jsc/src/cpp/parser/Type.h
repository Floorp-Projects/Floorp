#ifndef Type_h
#define Type_h

/**
 * The interface for all types.
 */

#include <vector>

namespace esc {
namespace v1 {

class Value;
class Context;

class Type {
//	virtual std::vector<Value*> values() = 0;
//	virtual std::vector<Value*> converts() = 0;
//    virtual void   addSub(Type& type) = 0;
    virtual bool   includes(Context& cx, Value* value) = 0;
//    virtual void   setSuper(Type& type) = 0;
//    virtual Type*  getSuper() = 0;
//    virtual Value* convert(Context& cx, Value& value) = 0;
//    virtual Value* coerce(Context& cx, Value& value) = 0;
};

}
}

#endif // Type_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
