/*
 * ReferenceValue
 */

#pragma warning ( disable : 4786 )

#include <stack>

#include "ReferenceValue.h"
#include "TypeValue.h"
#include "ObjectValue.h"
#include "Context.h"

namespace esc {
namespace v1 {

	ReferenceValue::ReferenceValue(ObjectValue* base, std::string name, 
		Value* qualifier)
		: base(base), name(name), qualifier(qualifier), 
		  used_namespaces((std::vector<Value*>*)0), slot((Slot*)0), 
		  slot_index(0),scope_index(0) {
	}

	ReferenceValue::ReferenceValue(ObjectValue* base, std::string name, 
		std::vector<Value*>* used_namespaces)
		: base(base), name(name), qualifier((Value*)0), 
		  used_namespaces(used_namespaces), slot((Slot*)0), 
		  slot_index(0),scope_index(0) {
	}

	Value* ReferenceValue::getValue(Context& cx) {
		
		
		Slot* s = this->getSlot(cx);

		if( s ) {
			return s->value;
		} else {
			return (Value*)0;
		}

	}

	Slot* ReferenceValue::getSlot(Context& cx) {
		
		if( slot ) {
			return slot;
		}

		if( lookup(cx) ) {
			// lookup has the side-effect of binding
			// this reference to its defining slot. So
			// now just return it.
			return this->slot;
		} else {
			return (Slot*)0;
		}
	}

	/*
	 * Lookup qualified name in the given context.
	 */

	bool ReferenceValue::lookup(Context& cx) {

		ObjectValue* obj;

		if(!base) {
			
			// If there is no base reference, search for the
			// binding from the inside to the outside of the 
			// scope chain. Fix up this reference's base member
			// and recurse.

			std::stack<ObjectValue*> scopes = cx.getScopes();
			int scope_index = 0;
			while( scopes.size() ) {
				this->base = scopes.top(); // Set the base value
				if( this->lookup(cx) ) {
					// Found one. Clear the temporary base value 
					// and set the scope index.
					this->base = 0;
					this->scope_index = scope_index;
					break;
				}
				scopes.pop();
				++scope_index;
			}

		} else {
			
			// If there is a base reference, search for the 
			// binding in this object and then up the proto
			// chain.

			if(!qualifier) {
				return this->lookupUnqualified(cx);
			}

			obj = base;

			while(obj && !obj->hasName(cx,name,qualifier)) {
				obj = obj->proto(cx);
			}

			if( !obj ) {
				return false;
			}

			// Bind slot

			this->slot_index = obj->getSlotIndex(cx,name,qualifier);
			this->slot       = obj->getSlot(cx,this->slot_index);
		}

		return true;
	}
	
	/*
	 * Lookup a name that has one of the used namespaces.
	 *
	 * 1 Traverse to root of proto chain, pushing all objects
	 *   onto a temporary stack.
	 * 2 In lifo order, see if one or more of the qualified
	 *   names represented by the name and the used qualfiers
	 *   is in the top object on the stack. Repeat for all
	 *   objects on the stack until one object with at least
	 *   one match is found, or all objects have been searched.
	 * 3 Create a new set of namespaces that inclues all
	 *   the qualifiers of all the names that matched in the
	 *   previous step.
	 * 4 Starting with the most derived object, search to 
	 *   proto chain for one or more of the qualified names
	 *   represented by the name and the new set of used
	 *   qualifiers.
	 * 5 If the found set represents more than one slot, throw
	 *   and error exception. Otherwise, return true.
	 * 6 If no matching names are found, return false.
	 */
	
	bool ReferenceValue::lookupUnqualified(Context& cx) {

		// Search this object and up the inheritance chain
		// to find the qualified name.

		ObjectValue* obj = base;

		// Push the protos onto a stack.

		std::stack<ObjectValue*> protos;

		while(obj) {
			protos.push(obj);
			obj = obj->proto(cx);
		}

		// Search the protos top-down for matching names.

		Namespaces*  namespaces = 0;
		while(!protos.empty()) {
			ObjectValue* obj = protos.top();
			namespaces = obj->hasNames(cx,name,&cx.used_namespaces);
			if( namespaces ) {
				break;
			}
			protos.pop();
		}

		// If no namespaces, then no matches. Return false.

		if( !namespaces ) {
			return false;
		}

		// Search for the first object in the proto chain that
		// matches the name and namespaces.

		obj = base?base:cx.scope();

		while(obj && !(obj->hasNames(cx,name,namespaces))) {
			obj = obj->proto(cx);
		}

		if( !obj ) {
			return false;
		}

		// Verify that all matched names point to a single slot.

        int last_index = 0;
		for( int i = 0; i < namespaces->size(); i++ ) {
			Value* qualifier = namespaces->at(i);
			int index = obj->getSlotIndex(cx,name,qualifier);
			if( last_index!=0 && index!=last_index ) {
				throw;
				// error();
			}
			last_index = index;
		}

		// Bind slot

		this->slot_index = last_index;
		this->slot  = obj->getSlot(cx,this->slot_index);

		return true;
	}
}
}

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
