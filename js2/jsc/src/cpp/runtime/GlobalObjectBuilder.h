#ifndef GlobalObjectConstructor_h
#define GlobalObjectConstructor_h

/*
 * Constructs global objects
 */

#include <string>
#include <vector>

#include "Builder.h"
#include "ObjectValue.h"

namespace esc {
namespace v1 {

class GlobalObjectBuilder : public Builder {
public:
	GlobalObjectBuilder() {
	}

	virtual void build(Context& cx, ObjectValue* ob);
};

}
}


#endif // GlobalObjectConstructor_h

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved
 */
