/*
 * Execution context.
 */

#ifndef Context_h
#define Context_h

#include <stack>
#include <vector>


namespace esc {
namespace v1 {

class Value;
class ObjectValue;

class Context {

	ObjectValue* global;
	std::stack<ObjectValue*> scopes;

public:
	Context() {
	}

	void pushScope(ObjectValue* scope) {
		if(scopes.empty()) {
			global = scope;
		}

		scopes.push(scope);
	}

	void popScope() {
		scopes.pop();
	}

	std::stack<ObjectValue*> getScopes() {
		return scopes;
	}

	ObjectValue* scope() {
		return scopes.top();
	}

	ObjectValue* globalScope() {
		return global;
	}

	std::vector<Value*> used_namespaces;
};

}
}

#endif // Context_h

/*
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
