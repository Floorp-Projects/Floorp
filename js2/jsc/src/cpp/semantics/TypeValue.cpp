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
#include "Builder.h"

#include "TypeValue.h"

namespace esc {
namespace v1 {


	bool       TypeValue::isInitialized = false;
	TypeValue* TypeValue::booleanType;
	TypeValue* TypeValue::namespaceType;
	TypeValue* TypeValue::objectType;
	TypeValue* TypeValue::typeType;

	void TypeValue::init() {

		if( isInitialized ) {
			return;
		}

		isInitialized  = true;
		booleanType    = new TypeValue();
		namespaceType  = new TypeValue();
		objectType     = new TypeValue();
		typeType       = new TypeValue();
	}
	
	void TypeValue::fini() {

		if( isInitialized ) {
			delete typeType;
			delete objectType;
			delete namespaceType;
			delete booleanType;
			isInitialized = false;
		}
	}
	
    bool TypeValue::includes(Context& cx, Value* value) {

		TypeValue* type = value->getType(cx);

		while( type ) {
			if( type == this ) {
				return true;
			}
			type = type->super(cx);
		}

		return false;
	}

    void TypeValue::build(Context& cx, ObjectValue* ob) {
	}

	TypeValue* TypeValue::super(Context& cx) {
		return reinterpret_cast<TypeValue*>(this->proto(cx));
	}
	

}
}

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
