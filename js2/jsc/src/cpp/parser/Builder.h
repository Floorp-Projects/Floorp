/*
 * Activation interface. An activation provides slot storage.
 */

#ifndef Builder_h
#define Builder_h

namespace esc {
namespace v1 {

class ObjectValue;

class Builder {
public:
	virtual void build(Context& cx, ObjectValue* ob) = 0;
};

}
}

#endif // Builder_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
