/*
	JSScope.h
	
	Scoping facility for JavaScript 2.0.
	
	by Patrick C. Beard.
 */

/**
 * Opaque reference to a JavaScript object.
 */
class JSObject;

/**
 * Opaque reference to a general JavaScript identifier key.
 * This can represent both UNICODE strings, and other more
 * efficient representations, such as hashcodes, etc.
 */
class JSIdentifier;

/**
 * Some fundamental constants.
 */
#define	JSNULL (0)
#define JSUNDEFINED = ((JSObject*)(-1))

/**
 * Basic abstraction to associate names with values in a chain of scopes.
 * Can represent any kind of scope, including lexical scopes (closures),
 * namespaces, and classes. Access control can be implemented
 */
class JSScope {
public:
    /**
	 * Gets the enclosing scope of this scope (if any).
	 * @return enclosing scope or JSNULL if none.
	 */
	virtual JSScope* getParent() = 0;

	/**
	 * Defines a new identifier in the current scope. In a completely static
	 * model, this may not be possible, and so this operation will be ignored.
	 * @param start the evaluation scope
	 * @param id identifier to define
	 * @param value optional value to associate with id
	 */
	virtual void defineValue(JSScope* start, JSIndentifier* id, JSObject* value = JSUNDEFINED) = 0;

	/**
	 * Looks up a value corresponding to the given identifier starting
	 * with the current scope, and proceeding up the scope chain. If
	 * the identifier's is not defined in any of the scopes, the value
	 * JSUNDEFINED is returned.
	 * @param start the evaluation scope
	 * @parm id the identifer to lookup
	 * @return result of lookup or JSUNDEFINED
	 */
	virtual JSObject* getValue(JSScope* start, JSIdentifier* id) = 0;
	
	/**
	 * Associates a value with an identifier in a given scope.
	 * @param start the evaluation scope
	 * @param id identifier to assign to
	 * @param value new value to give to id
	 * @return old value associated with identifier
	 */
	virtual JSObject* setValue(JSScope* start, JSIdentifier* id, JSObject* value) = 0;
};
