/*
 * Jexl
 * Copyright (c) 2015 TechnologyAdvice
 */

var Evaluator = require('./evaluator/Evaluator'),
	Lexer = require('./Lexer'),
	Parser = require('./parser/Parser'),
	defaultGrammar = require('./grammar').elements;

/**
 * Jexl is the Javascript Expression Language, capable of parsing and
 * evaluating basic to complex expression strings, combined with advanced
 * xpath-like drilldown into native Javascript objects.
 * @constructor
 */
function Jexl() {
	this._customGrammar = null;
	this._lexer = null;
	this._transforms = {};
}

/**
 * Adds a binary operator to Jexl at the specified precedence. The higher the
 * precedence, the earlier the operator is applied in the order of operations.
 * For example, * has a higher precedence than +, because multiplication comes
 * before division.
 *
 * Please see grammar.js for a listing of all default operators and their
 * precedence values in order to choose the appropriate precedence for the
 * new operator.
 * @param {string} operator The operator string to be added
 * @param {number} precedence The operator's precedence
 * @param {function} fn A function to run to calculate the result. The function
 *      will be called with two arguments: left and right, denoting the values
 *      on either side of the operator. It should return either the resulting
 *      value, or a Promise that resolves with the resulting value.
 */
Jexl.prototype.addBinaryOp = function(operator, precedence, fn) {
	this._addGrammarElement(operator, {
		type: 'binaryOp',
		precedence: precedence,
		eval: fn
	});
};

/**
 * Adds a unary operator to Jexl. Unary operators are currently only supported
 * on the left side of the value on which it will operate.
 * @param {string} operator The operator string to be added
 * @param {function} fn A function to run to calculate the result. The function
 *      will be called with one argument: the literal value to the right of the
 *      operator. It should return either the resulting value, or a Promise
 *      that resolves with the resulting value.
 */
Jexl.prototype.addUnaryOp = function(operator, fn) {
	this._addGrammarElement(operator, {
		type: 'unaryOp',
		weight: Infinity,
		eval: fn
	});
};

/**
 * Adds or replaces a transform function in this Jexl instance.
 * @param {string} name The name of the transform function, as it will be used
 *      within Jexl expressions
 * @param {function} fn The function to be executed when this transform is
 *      invoked.  It will be provided with two arguments:
 *          - {*} value: The value to be transformed
 *          - {{}} args: The arguments for this transform
 *          - {function} cb: A callback function to be called with an error
 *            if the transform fails, or a null first argument and the
 *            transformed value as the second argument on success.
 */
Jexl.prototype.addTransform = function(name, fn) {
	this._transforms[name] = fn;
};

/**
 * Syntactic sugar for calling {@link #addTransform} repeatedly.  This function
 * accepts a map of one or more transform names to their transform function.
 * @param {{}} map A map of transform names to transform functions
 */
Jexl.prototype.addTransforms = function(map) {
	for (var key in map) {
		if (map.hasOwnProperty(key))
			this._transforms[key] = map[key];
	}
};

/**
 * Retrieves a previously set transform function.
 * @param {string} name The name of the transform function
 * @returns {function} The transform function
 */
Jexl.prototype.getTransform = function(name) {
	return this._transforms[name];
};

/**
 * Evaluates a Jexl string within an optional context.
 * @param {string} expression The Jexl expression to be evaluated
 * @param {Object} [context] A mapping of variables to values, which will be
 *      made accessible to the Jexl expression when evaluating it
 * @param {function} [cb] An optional callback function to be executed when
 *      evaluation is complete.  It will be supplied with two arguments:
 *          - {Error|null} err: Present if an error occurred
 *          - {*} result: The result of the evaluation
 * @returns {Promise<*>} resolves with the result of the evaluation.  Note that
 *      if a callback is supplied, the returned promise will already have
 *      a '.catch' attached to it in order to pass the error to the callback.
 */
Jexl.prototype.eval = function(expression, context, cb) {
	if (typeof context === 'function') {
		cb = context;
		context = {};
	}
	else if (!context)
		context = {};
	var valPromise = this._eval(expression, context);
	if (cb) {
		// setTimeout is used for the callback to break out of the Promise's
		// try/catch in case the callback throws.
		var called = false;
		return valPromise.then(function(val) {
			called = true;
			setTimeout(cb.bind(null, null, val), 0);
		}).catch(function(err) {
			if (!called)
				setTimeout(cb.bind(null, err), 0);
		});
	}
	return valPromise;
};

/**
 * Removes a binary or unary operator from the Jexl grammar.
 * @param {string} operator The operator string to be removed
 */
Jexl.prototype.removeOp = function(operator) {
	var grammar = this._getCustomGrammar();
	if (grammar[operator] && (grammar[operator].type == 'binaryOp' ||
			grammar[operator].type == 'unaryOp')) {
		delete grammar[operator];
		this._lexer = null;
	}
};

/**
 * Adds an element to the grammar map used by this Jexl instance, cloning
 * the default grammar first if necessary.
 * @param {string} str The key string to be added
 * @param {{type: <string>}} obj A map of configuration options for this
 *      grammar element
 * @private
 */
Jexl.prototype._addGrammarElement = function(str, obj) {
	var grammar = this._getCustomGrammar();
	grammar[str] = obj;
	this._lexer = null;
};

/**
 * Evaluates a Jexl string in the given context.
 * @param {string} exp The Jexl expression to be evaluated
 * @param {Object} [context] A mapping of variables to values, which will be
 *      made accessible to the Jexl expression when evaluating it
 * @returns {Promise<*>} resolves with the result of the evaluation.
 * @private
 */
Jexl.prototype._eval = function(exp, context) {
	var self = this,
		grammar = this._getGrammar(),
		parser = new Parser(grammar),
		evaluator = new Evaluator(grammar, this._transforms, context);
	return Promise.resolve().then(function() {
		parser.addTokens(self._getLexer().tokenize(exp));
		return evaluator.eval(parser.complete());
	});
};

/**
 * Gets the custom grammar object, creating it first if necessary. New custom
 * grammars are created by executing a shallow clone of the default grammar
 * map. The returned map is available to be changed.
 * @returns {{}} a customizable grammar map.
 * @private
 */
Jexl.prototype._getCustomGrammar = function() {
	if (!this._customGrammar) {
		this._customGrammar = {};
		for (var key in defaultGrammar) {
			if (defaultGrammar.hasOwnProperty(key))
				this._customGrammar[key] = defaultGrammar[key];
		}
	}
	return this._customGrammar;
};

/**
 * Gets the grammar map currently being used by Jexl; either the default map,
 * or a locally customized version. The returned map should never be changed
 * in any way.
 * @returns {{}} the grammar map currently in use.
 * @private
 */
Jexl.prototype._getGrammar = function() {
	return this._customGrammar || defaultGrammar;
};

/**
 * Gets a Lexer instance as a singleton in reference to this Jexl instance.
 * @returns {Lexer} an instance of Lexer, initialized with a grammar
 *      appropriate to this Jexl instance.
 * @private
 */
Jexl.prototype._getLexer = function() {
	if (!this._lexer)
		this._lexer = new Lexer(this._getGrammar());
	return this._lexer;
};

module.exports = new Jexl();
module.exports.Jexl = Jexl;
