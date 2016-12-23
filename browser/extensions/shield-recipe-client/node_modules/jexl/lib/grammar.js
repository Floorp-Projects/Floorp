/*
 * Jexl
 * Copyright (c) 2015 TechnologyAdvice
 */

/**
 * A map of all expression elements to their properties. Note that changes
 * here may require changes in the Lexer or Parser.
 * @type {{}}
 */
exports.elements = {
	'.': {type: 'dot'},
	'[': {type: 'openBracket'},
	']': {type: 'closeBracket'},
	'|': {type: 'pipe'},
	'{': {type: 'openCurl'},
	'}': {type: 'closeCurl'},
	':': {type: 'colon'},
	',': {type: 'comma'},
	'(': {type: 'openParen'},
	')': {type: 'closeParen'},
	'?': {type: 'question'},
	'+': {type: 'binaryOp', precedence: 30,
		eval: function(left, right) { return left + right; }},
	'-': {type: 'binaryOp', precedence: 30,
		eval: function(left, right) { return left - right; }},
	'*': {type: 'binaryOp', precedence: 40,
		eval: function(left, right) { return left * right; }},
	'/': {type: 'binaryOp', precedence: 40,
		eval: function(left, right) { return left / right; }},
	'//': {type: 'binaryOp', precedence: 40,
		eval: function(left, right) { return Math.floor(left / right); }},
	'%': {type: 'binaryOp', precedence: 50,
		eval: function(left, right) { return left % right; }},
	'^': {type: 'binaryOp', precedence: 50,
		eval: function(left, right) { return Math.pow(left, right); }},
	'==': {type: 'binaryOp', precedence: 20,
		eval: function(left, right) { return left == right; }},
	'!=': {type: 'binaryOp', precedence: 20,
		eval: function(left, right) { return left != right; }},
	'>': {type: 'binaryOp', precedence: 20,
		eval: function(left, right) { return left > right; }},
	'>=': {type: 'binaryOp', precedence: 20,
		eval: function(left, right) { return left >= right; }},
	'<': {type: 'binaryOp', precedence: 20,
		eval: function(left, right) { return left < right; }},
	'<=': {type: 'binaryOp', precedence: 20,
		eval: function(left, right) { return left <= right; }},
	'&&': {type: 'binaryOp', precedence: 10,
		eval: function(left, right) { return left && right; }},
	'||': {type: 'binaryOp', precedence: 10,
		eval: function(left, right) { return left || right; }},
	'in': {type: 'binaryOp', precedence: 20,
		eval: function(left, right) {
			if (typeof right === 'string')
				return right.indexOf(left) !== -1;
			if (Array.isArray(right)) {
				return right.some(function(elem) {
					return elem == left;
				});
			}
			return false;
		}},
	'!': {type: 'unaryOp', precedence: Infinity,
		eval: function(right) { return !right; }}
};
