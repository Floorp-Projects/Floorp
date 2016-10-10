/*
 * Jexl
 * Copyright (c) 2015 TechnologyAdvice
 */

var h = require('./handlers');

/**
 * A mapping of all states in the finite state machine to a set of instructions
 * for handling or transitioning into other states. Each state can be handled
 * in one of two schemes: a tokenType map, or a subHandler.
 *
 * Standard expression elements are handled through the tokenType object. This
 * is an object map of all legal token types to encounter in this state (and
 * any unexpected token types will generate a thrown error) to an options
 * object that defines how they're handled.  The available options are:
 *
 *      {string} toState: The name of the state to which to transition
 *          immediately after handling this token
 *      {string} handler: The handler function to call when this token type is
 *          encountered in this state.  If omitted, the default handler
 *          matching the token's "type" property will be called. If the handler
 *          function does not exist, no call will be made and no error will be
 *          generated.  This is useful for tokens whose sole purpose is to
 *          transition to other states.
 *
 * States that consume a subexpression should define a subHandler, the
 * function to be called with an expression tree argument when the
 * subexpression is complete. Completeness is determined through the
 * endStates object, which maps tokens on which an expression should end to the
 * state to which to transition once the subHandler function has been called.
 *
 * Additionally, any state in which it is legal to mark the AST as completed
 * should have a 'completable' property set to boolean true.  Attempting to
 * call {@link Parser#complete} in any state without this property will result
 * in a thrown Error.
 *
 * @type {{}}
 */
exports.states = {
	expectOperand: {
		tokenTypes: {
			literal: {toState: 'expectBinOp'},
			identifier: {toState: 'identifier'},
			unaryOp: {},
			openParen: {toState: 'subExpression'},
			openCurl: {toState: 'expectObjKey', handler: h.objStart},
			dot: {toState: 'traverse'},
			openBracket: {toState: 'arrayVal', handler: h.arrayStart}
		}
	},
	expectBinOp: {
		tokenTypes: {
			binaryOp: {toState: 'expectOperand'},
			pipe: {toState: 'expectTransform'},
			dot: {toState: 'traverse'},
			question: {toState: 'ternaryMid', handler: h.ternaryStart}
		},
		completable: true
	},
	expectTransform: {
		tokenTypes: {
			identifier: {toState: 'postTransform', handler: h.transform}
		}
	},
	expectObjKey: {
		tokenTypes: {
			identifier: {toState: 'expectKeyValSep', handler: h.objKey},
			closeCurl: {toState: 'expectBinOp'}
		}
	},
	expectKeyValSep: {
		tokenTypes: {
			colon: {toState: 'objVal'}
		}
	},
	postTransform: {
		tokenTypes: {
			openParen: {toState: 'argVal'},
			binaryOp: {toState: 'expectOperand'},
			dot: {toState: 'traverse'},
			openBracket: {toState: 'filter'},
			pipe: {toState: 'expectTransform'}
		},
		completable: true
	},
	postTransformArgs: {
		tokenTypes: {
			binaryOp: {toState: 'expectOperand'},
			dot: {toState: 'traverse'},
			openBracket: {toState: 'filter'},
			pipe: {toState: 'expectTransform'}
		},
		completable: true
	},
	identifier: {
		tokenTypes: {
			binaryOp: {toState: 'expectOperand'},
			dot: {toState: 'traverse'},
			openBracket: {toState: 'filter'},
			pipe: {toState: 'expectTransform'},
			question: {toState: 'ternaryMid', handler: h.ternaryStart}
		},
		completable: true
	},
	traverse: {
		tokenTypes: {
			'identifier': {toState: 'identifier'}
		}
	},
	filter: {
		subHandler: h.filter,
		endStates: {
			closeBracket: 'identifier'
		}
	},
	subExpression: {
		subHandler: h.subExpression,
		endStates: {
			closeParen: 'expectBinOp'
		}
	},
	argVal: {
		subHandler: h.argVal,
		endStates: {
			comma: 'argVal',
			closeParen: 'postTransformArgs'
		}
	},
	objVal: {
		subHandler: h.objVal,
		endStates: {
			comma: 'expectObjKey',
			closeCurl: 'expectBinOp'
		}
	},
	arrayVal: {
		subHandler: h.arrayVal,
		endStates: {
			comma: 'arrayVal',
			closeBracket: 'expectBinOp'
		}
	},
	ternaryMid: {
		subHandler: h.ternaryMid,
		endStates: {
			colon: 'ternaryEnd'
		}
	},
	ternaryEnd: {
		subHandler: h.ternaryEnd,
		completable: true
	}
};
