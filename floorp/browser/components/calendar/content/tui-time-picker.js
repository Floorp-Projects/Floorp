/*!
 * TOAST UI Time Picker
 * @version 2.1.6
 * @license MIT
 */
(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define([], factory);
	else if(typeof exports === 'object')
		exports["TimePicker"] = factory();
	else
		root["tui"] = root["tui"] || {}, root["tui"]["TimePicker"] = factory();
})(window, function() {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, { enumerable: true, get: getter });
/******/ 		}
/******/ 	};
/******/
/******/ 	// define __esModule on exports
/******/ 	__webpack_require__.r = function(exports) {
/******/ 		if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 			Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 		}
/******/ 		Object.defineProperty(exports, '__esModule', { value: true });
/******/ 	};
/******/
/******/ 	// create a fake namespace object
/******/ 	// mode & 1: value is a module id, require it
/******/ 	// mode & 2: merge all properties of value into the ns
/******/ 	// mode & 4: return value when already ns object
/******/ 	// mode & 8|1: behave like require
/******/ 	__webpack_require__.t = function(value, mode) {
/******/ 		if(mode & 1) value = __webpack_require__(value);
/******/ 		if(mode & 8) return value;
/******/ 		if((mode & 4) && typeof value === 'object' && value && value.__esModule) return value;
/******/ 		var ns = Object.create(null);
/******/ 		__webpack_require__.r(ns);
/******/ 		Object.defineProperty(ns, 'default', { enumerable: true, value: value });
/******/ 		if(mode & 2 && typeof value != 'string') for(var key in value) __webpack_require__.d(ns, key, function(key) { return value[key]; }.bind(null, key));
/******/ 		return ns;
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "dist";
/******/
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 20);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* eslint-disable complexity */
/**
 * @fileoverview Returns the first index at which a given element can be found in the array.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isArray = __webpack_require__(3);

/**
 * @module array
 */

/**
 * Returns the first index at which a given element can be found in the array
 * from start index(default 0), or -1 if it is not present.
 * It compares searchElement to elements of the Array using strict equality
 * (the same method used by the ===, or triple-equals, operator).
 * @param {*} searchElement Element to locate in the array
 * @param {Array} array Array that will be traversed.
 * @param {number} startIndex Start index in array for searching (default 0)
 * @returns {number} the First index at which a given element, or -1 if it is not present
 * @memberof module:array
 * @example
 * var inArray = require('tui-code-snippet/array/inArray'); // node, commonjs
 *
 * var arr = ['one', 'two', 'three', 'four'];
 * var idx1 = inArray('one', arr, 3); // -1
 * var idx2 = inArray('one', arr); // 0
 */
function inArray(searchElement, array, startIndex) {
  var i;
  var length;
  startIndex = startIndex || 0;

  if (!isArray(array)) {
    return -1;
  }

  if (Array.prototype.indexOf) {
    return Array.prototype.indexOf.call(array, searchElement, startIndex);
  }

  length = array.length;
  for (i = startIndex; startIndex >= 0 && i < length; i += 1) {
    if (array[i] === searchElement) {
      return i;
    }
  }

  return -1;
}

module.exports = inArray;


/***/ }),
/* 1 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Execute the provided callback once for each element present in the array(or Array-like object) in ascending order.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Execute the provided callback once for each element present
 * in the array(or Array-like object) in ascending order.
 * If the callback function returns false, the loop will be stopped.
 * Callback function(iteratee) is invoked with three arguments:
 *  1) The value of the element
 *  2) The index of the element
 *  3) The array(or Array-like object) being traversed
 * @param {Array|Arguments|NodeList} arr The array(or Array-like object) that will be traversed
 * @param {function} iteratee Callback function
 * @param {Object} [context] Context(this) of callback function
 * @memberof module:collection
 * @example
 * var forEachArray = require('tui-code-snippet/collection/forEachArray'); // node, commonjs
 *
 * var sum = 0;
 *
 * forEachArray([1,2,3], function(value){
 *     sum += value;
 * });
 * alert(sum); // 6
 */
function forEachArray(arr, iteratee, context) {
  var index = 0;
  var len = arr.length;

  context = context || null;

  for (; index < len; index += 1) {
    if (iteratee.call(context, arr[index], index, arr) === false) {
      break;
    }
  }
}

module.exports = forEachArray;


/***/ }),
/* 2 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Extend the target object from other objects.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * @module object
 */

/**
 * Extend the target object from other objects.
 * @param {object} target - Object that will be extended
 * @param {...object} objects - Objects as sources
 * @returns {object} Extended object
 * @memberof module:object
 */
function extend(target, objects) { // eslint-disable-line no-unused-vars
  var hasOwnProp = Object.prototype.hasOwnProperty;
  var source, prop, i, len;

  for (i = 1, len = arguments.length; i < len; i += 1) {
    source = arguments[i];
    for (prop in source) {
      if (hasOwnProp.call(source, prop)) {
        target[prop] = source[prop];
      }
    }
  }

  return target;
}

module.exports = extend;


/***/ }),
/* 3 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is an instance of Array or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is an instance of Array or not.
 * If the given variable is an instance of Array, return true.
 * @param {*} obj - Target for checking
 * @returns {boolean} Is array instance?
 * @memberof module:type
 */
function isArray(obj) {
  return obj instanceof Array;
}

module.exports = isArray;


/***/ }),
/* 4 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Execute the provided callback once for each property of object(or element of array) which actually exist.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isArray = __webpack_require__(3);
var forEachArray = __webpack_require__(1);
var forEachOwnProperties = __webpack_require__(16);

/**
 * @module collection
 */

/**
 * Execute the provided callback once for each property of object(or element of array) which actually exist.
 * If the object is Array-like object(ex-arguments object), It needs to transform to Array.(see 'ex2' of example).
 * If the callback function returns false, the loop will be stopped.
 * Callback function(iteratee) is invoked with three arguments:
 *  1) The value of the property(or The value of the element)
 *  2) The name of the property(or The index of the element)
 *  3) The object being traversed
 * @param {Object} obj The object that will be traversed
 * @param {function} iteratee Callback function
 * @param {Object} [context] Context(this) of callback function
 * @memberof module:collection
 * @example
 * var forEach = require('tui-code-snippet/collection/forEach'); // node, commonjs
 *
 * var sum = 0;
 *
 * forEach([1,2,3], function(value){
 *     sum += value;
 * });
 * alert(sum); // 6
 *
 * // In case of Array-like object
 * var array = Array.prototype.slice.call(arrayLike); // change to array
 * forEach(array, function(value){
 *     sum += value;
 * });
 */
function forEach(obj, iteratee, context) {
  if (isArray(obj)) {
    forEachArray(obj, iteratee, context);
  } else {
    forEachOwnProperties(obj, iteratee, context);
  }
}

module.exports = forEach;


/***/ }),
/* 5 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is undefined or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is undefined or not.
 * If the given variable is undefined, returns true.
 * @param {*} obj - Target for checking
 * @returns {boolean} Is undefined?
 * @memberof module:type
 */
function isUndefined(obj) {
  return obj === undefined; // eslint-disable-line no-undefined
}

module.exports = isUndefined;


/***/ }),
/* 6 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is a string or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is a string or not.
 * If the given variable is a string, return true.
 * @param {*} obj - Target for checking
 * @returns {boolean} Is string?
 * @memberof module:type
 */
function isString(obj) {
  return typeof obj === 'string' || obj instanceof String;
}

module.exports = isString;


/***/ }),
/* 7 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Convert text by binding expressions with context.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var inArray = __webpack_require__(0);
var forEach = __webpack_require__(4);
var isArray = __webpack_require__(3);
var isString = __webpack_require__(6);
var extend = __webpack_require__(2);

// IE8 does not support capture groups.
var EXPRESSION_REGEXP = /{{\s?|\s?}}/g;
var BRACKET_NOTATION_REGEXP = /^[a-zA-Z0-9_@]+\[[a-zA-Z0-9_@"']+\]$/;
var BRACKET_REGEXP = /\[\s?|\s?\]/;
var DOT_NOTATION_REGEXP = /^[a-zA-Z_]+\.[a-zA-Z_]+$/;
var DOT_REGEXP = /\./;
var STRING_NOTATION_REGEXP = /^["']\w+["']$/;
var STRING_REGEXP = /"|'/g;
var NUMBER_REGEXP = /^-?\d+\.?\d*$/;

var EXPRESSION_INTERVAL = 2;

var BLOCK_HELPERS = {
  'if': handleIf,
  'each': handleEach,
  'with': handleWith
};

var isValidSplit = 'a'.split(/a/).length === 3;

/**
 * Split by RegExp. (Polyfill for IE8)
 * @param {string} text - text to be splitted\
 * @param {RegExp} regexp - regular expression
 * @returns {Array.<string>}
 */
var splitByRegExp = (function() {
  if (isValidSplit) {
    return function(text, regexp) {
      return text.split(regexp);
    };
  }

  return function(text, regexp) {
    var result = [];
    var prevIndex = 0;
    var match, index;

    if (!regexp.global) {
      regexp = new RegExp(regexp, 'g');
    }

    match = regexp.exec(text);
    while (match !== null) {
      index = match.index;
      result.push(text.slice(prevIndex, index));

      prevIndex = index + match[0].length;
      match = regexp.exec(text);
    }
    result.push(text.slice(prevIndex));

    return result;
  };
})();

/**
 * Find value in the context by an expression.
 * @param {string} exp - an expression
 * @param {object} context - context
 * @returns {*}
 * @private
 */
// eslint-disable-next-line complexity
function getValueFromContext(exp, context) {
  var splitedExps;
  var value = context[exp];

  if (exp === 'true') {
    value = true;
  } else if (exp === 'false') {
    value = false;
  } else if (STRING_NOTATION_REGEXP.test(exp)) {
    value = exp.replace(STRING_REGEXP, '');
  } else if (BRACKET_NOTATION_REGEXP.test(exp)) {
    splitedExps = exp.split(BRACKET_REGEXP);
    value = getValueFromContext(splitedExps[0], context)[getValueFromContext(splitedExps[1], context)];
  } else if (DOT_NOTATION_REGEXP.test(exp)) {
    splitedExps = exp.split(DOT_REGEXP);
    value = getValueFromContext(splitedExps[0], context)[splitedExps[1]];
  } else if (NUMBER_REGEXP.test(exp)) {
    value = parseFloat(exp);
  }

  return value;
}

/**
 * Extract elseif and else expressions.
 * @param {Array.<string>} ifExps - args of if expression
 * @param {Array.<string>} sourcesInsideBlock - sources inside if block
 * @returns {object} - exps: expressions of if, elseif, and else / sourcesInsideIf: sources inside if, elseif, and else block.
 * @private
 */
function extractElseif(ifExps, sourcesInsideBlock) {
  var exps = [ifExps];
  var sourcesInsideIf = [];
  var otherIfCount = 0;
  var start = 0;

  // eslint-disable-next-line complexity
  forEach(sourcesInsideBlock, function(source, index) {
    if (source.indexOf('if') === 0) {
      otherIfCount += 1;
    } else if (source === '/if') {
      otherIfCount -= 1;
    } else if (!otherIfCount && (source.indexOf('elseif') === 0 || source === 'else')) {
      exps.push(source === 'else' ? ['true'] : source.split(' ').slice(1));
      sourcesInsideIf.push(sourcesInsideBlock.slice(start, index));
      start = index + 1;
    }
  });

  sourcesInsideIf.push(sourcesInsideBlock.slice(start));

  return {
    exps: exps,
    sourcesInsideIf: sourcesInsideIf
  };
}

/**
 * Helper function for "if". 
 * @param {Array.<string>} exps - array of expressions split by spaces
 * @param {Array.<string>} sourcesInsideBlock - array of sources inside the if block
 * @param {object} context - context
 * @returns {string}
 * @private
 */
function handleIf(exps, sourcesInsideBlock, context) {
  var analyzed = extractElseif(exps, sourcesInsideBlock);
  var result = false;
  var compiledSource = '';

  forEach(analyzed.exps, function(exp, index) {
    result = handleExpression(exp, context);
    if (result) {
      compiledSource = compile(analyzed.sourcesInsideIf[index], context);
    }

    return !result;
  });

  return compiledSource;
}

/**
 * Helper function for "each".
 * @param {Array.<string>} exps - array of expressions split by spaces
 * @param {Array.<string>} sourcesInsideBlock - array of sources inside the each block
 * @param {object} context - context
 * @returns {string}
 * @private
 */
function handleEach(exps, sourcesInsideBlock, context) {
  var collection = handleExpression(exps, context);
  var additionalKey = isArray(collection) ? '@index' : '@key';
  var additionalContext = {};
  var result = '';

  forEach(collection, function(item, key) {
    additionalContext[additionalKey] = key;
    additionalContext['@this'] = item;
    extend(context, additionalContext);

    result += compile(sourcesInsideBlock.slice(), context);
  });

  return result;
}

/**
 * Helper function for "with ... as"
 * @param {Array.<string>} exps - array of expressions split by spaces
 * @param {Array.<string>} sourcesInsideBlock - array of sources inside the with block
 * @param {object} context - context
 * @returns {string}
 * @private
 */
function handleWith(exps, sourcesInsideBlock, context) {
  var asIndex = inArray('as', exps);
  var alias = exps[asIndex + 1];
  var result = handleExpression(exps.slice(0, asIndex), context);

  var additionalContext = {};
  additionalContext[alias] = result;

  return compile(sourcesInsideBlock, extend(context, additionalContext)) || '';
}

/**
 * Extract sources inside block in place.
 * @param {Array.<string>} sources - array of sources
 * @param {number} start - index of start block
 * @param {number} end - index of end block
 * @returns {Array.<string>}
 * @private
 */
function extractSourcesInsideBlock(sources, start, end) {
  var sourcesInsideBlock = sources.splice(start + 1, end - start);
  sourcesInsideBlock.pop();

  return sourcesInsideBlock;
}

/**
 * Handle block helper function
 * @param {string} helperKeyword - helper keyword (ex. if, each, with)
 * @param {Array.<string>} sourcesToEnd - array of sources after the starting block
 * @param {object} context - context
 * @returns {Array.<string>}
 * @private
 */
function handleBlockHelper(helperKeyword, sourcesToEnd, context) {
  var executeBlockHelper = BLOCK_HELPERS[helperKeyword];
  var helperCount = 1;
  var startBlockIndex = 0;
  var endBlockIndex;
  var index = startBlockIndex + EXPRESSION_INTERVAL;
  var expression = sourcesToEnd[index];

  while (helperCount && isString(expression)) {
    if (expression.indexOf(helperKeyword) === 0) {
      helperCount += 1;
    } else if (expression.indexOf('/' + helperKeyword) === 0) {
      helperCount -= 1;
      endBlockIndex = index;
    }

    index += EXPRESSION_INTERVAL;
    expression = sourcesToEnd[index];
  }

  if (helperCount) {
    throw Error(helperKeyword + ' needs {{/' + helperKeyword + '}} expression.');
  }

  sourcesToEnd[startBlockIndex] = executeBlockHelper(
    sourcesToEnd[startBlockIndex].split(' ').slice(1),
    extractSourcesInsideBlock(sourcesToEnd, startBlockIndex, endBlockIndex),
    context
  );

  return sourcesToEnd;
}

/**
 * Helper function for "custom helper".
 * If helper is not a function, return helper itself.
 * @param {Array.<string>} exps - array of expressions split by spaces (first element: helper)
 * @param {object} context - context
 * @returns {string}
 * @private
 */
function handleExpression(exps, context) {
  var result = getValueFromContext(exps[0], context);

  if (result instanceof Function) {
    return executeFunction(result, exps.slice(1), context);
  }

  return result;
}

/**
 * Execute a helper function.
 * @param {Function} helper - helper function
 * @param {Array.<string>} argExps - expressions of arguments
 * @param {object} context - context
 * @returns {string} - result of executing the function with arguments
 * @private
 */
function executeFunction(helper, argExps, context) {
  var args = [];
  forEach(argExps, function(exp) {
    args.push(getValueFromContext(exp, context));
  });

  return helper.apply(null, args);
}

/**
 * Get a result of compiling an expression with the context.
 * @param {Array.<string>} sources - array of sources split by regexp of expression.
 * @param {object} context - context
 * @returns {Array.<string>} - array of sources that bind with its context
 * @private
 */
function compile(sources, context) {
  var index = 1;
  var expression = sources[index];
  var exps, firstExp, result;

  while (isString(expression)) {
    exps = expression.split(' ');
    firstExp = exps[0];

    if (BLOCK_HELPERS[firstExp]) {
      result = handleBlockHelper(firstExp, sources.splice(index, sources.length - index), context);
      sources = sources.concat(result);
    } else {
      sources[index] = handleExpression(exps, context);
    }

    index += EXPRESSION_INTERVAL;
    expression = sources[index];
  }

  return sources.join('');
}

/**
 * Convert text by binding expressions with context.
 * <br>
 * If expression exists in the context, it will be replaced.
 * ex) '{{title}}' with context {title: 'Hello!'} is converted to 'Hello!'.
 * An array or object can be accessed using bracket and dot notation.
 * ex) '{{odds\[2\]}}' with context {odds: \[1, 3, 5\]} is converted to '5'.
 * ex) '{{evens\[first\]}}' with context {evens: \[2, 4\], first: 0} is converted to '2'.
 * ex) '{{project\["name"\]}}' and '{{project.name}}' with context {project: {name: 'CodeSnippet'}} is converted to 'CodeSnippet'.
 * <br>
 * If replaced expression is a function, next expressions will be arguments of the function.
 * ex) '{{add 1 2}}' with context {add: function(a, b) {return a + b;}} is converted to '3'.
 * <br>
 * It has 3 predefined block helpers '{{helper ...}} ... {{/helper}}': 'if', 'each', 'with ... as ...'.
 * 1) 'if' evaluates conditional statements. It can use with 'elseif' and 'else'.
 * 2) 'each' iterates an array or object. It provides '@index'(array), '@key'(object), and '@this'(current element).
 * 3) 'with ... as ...' provides an alias.
 * @param {string} text - text with expressions
 * @param {object} context - context
 * @returns {string} - text that bind with its context
 * @memberof module:domUtil
 * @example
 * var template = require('tui-code-snippet/domUtil/template');
 * 
 * var source = 
 *     '<h1>'
 *   +   '{{if isValidNumber title}}'
 *   +     '{{title}}th'
 *   +   '{{elseif isValidDate title}}'
 *   +     'Date: {{title}}'
 *   +   '{{/if}}'
 *   + '</h1>'
 *   + '{{each list}}'
 *   +   '{{with addOne @index as idx}}'
 *   +     '<p>{{idx}}: {{@this}}</p>'
 *   +   '{{/with}}'
 *   + '{{/each}}';
 * 
 * var context = {
 *   isValidDate: function(text) {
 *     return /^\d{4}-(0|1)\d-(0|1|2|3)\d$/.test(text);
 *   },
 *   isValidNumber: function(text) {
 *     return /^\d+$/.test(text);
 *   }
 *   title: '2019-11-25',
 *   list: ['Clean the room', 'Wash the dishes'],
 *   addOne: function(num) {
 *     return num + 1;
 *   }
 * };
 * 
 * var result = template(source, context);
 * console.log(result); // <h1>Date: 2019-11-25</h1><p>1: Clean the room</p><p>2: Wash the dishes</p>
 */
function template(text, context) {
  return compile(splitByRegExp(text, EXPRESSION_REGEXP), context);
}

module.exports = template;


/***/ }),
/* 8 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview This module provides some functions for custom events. And it is implemented in the observer design pattern.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var extend = __webpack_require__(2);
var isExisty = __webpack_require__(23);
var isString = __webpack_require__(6);
var isObject = __webpack_require__(25);
var isArray = __webpack_require__(3);
var isFunction = __webpack_require__(26);
var forEach = __webpack_require__(4);

var R_EVENTNAME_SPLIT = /\s+/g;

/**
 * @class
 * @example
 * // node, commonjs
 * var CustomEvents = require('tui-code-snippet/customEvents/customEvents');
 */
function CustomEvents() {
  /**
     * @type {HandlerItem[]}
     */
  this.events = null;

  /**
     * only for checking specific context event was binded
     * @type {object[]}
     */
  this.contexts = null;
}

/**
 * Mixin custom events feature to specific constructor
 * @param {function} func - constructor
 * @example
 * var CustomEvents = require('tui-code-snippet/customEvents/customEvents'); // node, commonjs
 *
 * var model;
 * function Model() {
 *     this.name = '';
 * }
 * CustomEvents.mixin(Model);
 *
 * model = new Model();
 * model.on('change', function() { this.name = 'model'; }, this);
 * model.fire('change');
 * alert(model.name); // 'model';
 */
CustomEvents.mixin = function(func) {
  extend(func.prototype, CustomEvents.prototype);
};

/**
 * Get HandlerItem object
 * @param {function} handler - handler function
 * @param {object} [context] - context for handler
 * @returns {HandlerItem} HandlerItem object
 * @private
 */
CustomEvents.prototype._getHandlerItem = function(handler, context) {
  var item = {handler: handler};

  if (context) {
    item.context = context;
  }

  return item;
};

/**
 * Get event object safely
 * @param {string} [eventName] - create sub event map if not exist.
 * @returns {(object|array)} event object. if you supplied `eventName`
 *  parameter then make new array and return it
 * @private
 */
CustomEvents.prototype._safeEvent = function(eventName) {
  var events = this.events;
  var byName;

  if (!events) {
    events = this.events = {};
  }

  if (eventName) {
    byName = events[eventName];

    if (!byName) {
      byName = [];
      events[eventName] = byName;
    }

    events = byName;
  }

  return events;
};

/**
 * Get context array safely
 * @returns {array} context array
 * @private
 */
CustomEvents.prototype._safeContext = function() {
  var context = this.contexts;

  if (!context) {
    context = this.contexts = [];
  }

  return context;
};

/**
 * Get index of context
 * @param {object} ctx - context that used for bind custom event
 * @returns {number} index of context
 * @private
 */
CustomEvents.prototype._indexOfContext = function(ctx) {
  var context = this._safeContext();
  var index = 0;

  while (context[index]) {
    if (ctx === context[index][0]) {
      return index;
    }

    index += 1;
  }

  return -1;
};

/**
 * Memorize supplied context for recognize supplied object is context or
 *  name: handler pair object when off()
 * @param {object} ctx - context object to memorize
 * @private
 */
CustomEvents.prototype._memorizeContext = function(ctx) {
  var context, index;

  if (!isExisty(ctx)) {
    return;
  }

  context = this._safeContext();
  index = this._indexOfContext(ctx);

  if (index > -1) {
    context[index][1] += 1;
  } else {
    context.push([ctx, 1]);
  }
};

/**
 * Forget supplied context object
 * @param {object} ctx - context object to forget
 * @private
 */
CustomEvents.prototype._forgetContext = function(ctx) {
  var context, contextIndex;

  if (!isExisty(ctx)) {
    return;
  }

  context = this._safeContext();
  contextIndex = this._indexOfContext(ctx);

  if (contextIndex > -1) {
    context[contextIndex][1] -= 1;

    if (context[contextIndex][1] <= 0) {
      context.splice(contextIndex, 1);
    }
  }
};

/**
 * Bind event handler
 * @param {(string|{name:string, handler:function})} eventName - custom
 *  event name or an object {eventName: handler}
 * @param {(function|object)} [handler] - handler function or context
 * @param {object} [context] - context for binding
 * @private
 */
CustomEvents.prototype._bindEvent = function(eventName, handler, context) {
  var events = this._safeEvent(eventName);
  this._memorizeContext(context);
  events.push(this._getHandlerItem(handler, context));
};

/**
 * Bind event handlers
 * @param {(string|{name:string, handler:function})} eventName - custom
 *  event name or an object {eventName: handler}
 * @param {(function|object)} [handler] - handler function or context
 * @param {object} [context] - context for binding
 * //-- #1. Get Module --//
 * var CustomEvents = require('tui-code-snippet/customEvents/customEvents'); // node, commonjs
 *
 * //-- #2. Use method --//
 * // # 2.1 Basic Usage
 * CustomEvents.on('onload', handler);
 *
 * // # 2.2 With context
 * CustomEvents.on('onload', handler, myObj);
 *
 * // # 2.3 Bind by object that name, handler pairs
 * CustomEvents.on({
 *     'play': handler,
 *     'pause': handler2
 * });
 *
 * // # 2.4 Bind by object that name, handler pairs with context object
 * CustomEvents.on({
 *     'play': handler
 * }, myObj);
 */
CustomEvents.prototype.on = function(eventName, handler, context) {
  var self = this;

  if (isString(eventName)) {
    // [syntax 1, 2]
    eventName = eventName.split(R_EVENTNAME_SPLIT);
    forEach(eventName, function(name) {
      self._bindEvent(name, handler, context);
    });
  } else if (isObject(eventName)) {
    // [syntax 3, 4]
    context = handler;
    forEach(eventName, function(func, name) {
      self.on(name, func, context);
    });
  }
};

/**
 * Bind one-shot event handlers
 * @param {(string|{name:string,handler:function})} eventName - custom
 *  event name or an object {eventName: handler}
 * @param {function|object} [handler] - handler function or context
 * @param {object} [context] - context for binding
 */
CustomEvents.prototype.once = function(eventName, handler, context) {
  var self = this;

  if (isObject(eventName)) {
    context = handler;
    forEach(eventName, function(func, name) {
      self.once(name, func, context);
    });

    return;
  }

  function onceHandler() { // eslint-disable-line require-jsdoc
    handler.apply(context, arguments);
    self.off(eventName, onceHandler, context);
  }

  this.on(eventName, onceHandler, context);
};

/**
 * Splice supplied array by callback result
 * @param {array} arr - array to splice
 * @param {function} predicate - function return boolean
 * @private
 */
CustomEvents.prototype._spliceMatches = function(arr, predicate) {
  var i = 0;
  var len;

  if (!isArray(arr)) {
    return;
  }

  for (len = arr.length; i < len; i += 1) {
    if (predicate(arr[i]) === true) {
      arr.splice(i, 1);
      len -= 1;
      i -= 1;
    }
  }
};

/**
 * Get matcher for unbind specific handler events
 * @param {function} handler - handler function
 * @returns {function} handler matcher
 * @private
 */
CustomEvents.prototype._matchHandler = function(handler) {
  var self = this;

  return function(item) {
    var needRemove = handler === item.handler;

    if (needRemove) {
      self._forgetContext(item.context);
    }

    return needRemove;
  };
};

/**
 * Get matcher for unbind specific context events
 * @param {object} context - context
 * @returns {function} object matcher
 * @private
 */
CustomEvents.prototype._matchContext = function(context) {
  var self = this;

  return function(item) {
    var needRemove = context === item.context;

    if (needRemove) {
      self._forgetContext(item.context);
    }

    return needRemove;
  };
};

/**
 * Get matcher for unbind specific hander, context pair events
 * @param {function} handler - handler function
 * @param {object} context - context
 * @returns {function} handler, context matcher
 * @private
 */
CustomEvents.prototype._matchHandlerAndContext = function(handler, context) {
  var self = this;

  return function(item) {
    var matchHandler = (handler === item.handler);
    var matchContext = (context === item.context);
    var needRemove = (matchHandler && matchContext);

    if (needRemove) {
      self._forgetContext(item.context);
    }

    return needRemove;
  };
};

/**
 * Unbind event by event name
 * @param {string} eventName - custom event name to unbind
 * @param {function} [handler] - handler function
 * @private
 */
CustomEvents.prototype._offByEventName = function(eventName, handler) {
  var self = this;
  var andByHandler = isFunction(handler);
  var matchHandler = self._matchHandler(handler);

  eventName = eventName.split(R_EVENTNAME_SPLIT);

  forEach(eventName, function(name) {
    var handlerItems = self._safeEvent(name);

    if (andByHandler) {
      self._spliceMatches(handlerItems, matchHandler);
    } else {
      forEach(handlerItems, function(item) {
        self._forgetContext(item.context);
      });

      self.events[name] = [];
    }
  });
};

/**
 * Unbind event by handler function
 * @param {function} handler - handler function
 * @private
 */
CustomEvents.prototype._offByHandler = function(handler) {
  var self = this;
  var matchHandler = this._matchHandler(handler);

  forEach(this._safeEvent(), function(handlerItems) {
    self._spliceMatches(handlerItems, matchHandler);
  });
};

/**
 * Unbind event by object(name: handler pair object or context object)
 * @param {object} obj - context or {name: handler} pair object
 * @param {function} handler - handler function
 * @private
 */
CustomEvents.prototype._offByObject = function(obj, handler) {
  var self = this;
  var matchFunc;

  if (this._indexOfContext(obj) < 0) {
    forEach(obj, function(func, name) {
      self.off(name, func);
    });
  } else if (isString(handler)) {
    matchFunc = this._matchContext(obj);

    self._spliceMatches(this._safeEvent(handler), matchFunc);
  } else if (isFunction(handler)) {
    matchFunc = this._matchHandlerAndContext(handler, obj);

    forEach(this._safeEvent(), function(handlerItems) {
      self._spliceMatches(handlerItems, matchFunc);
    });
  } else {
    matchFunc = this._matchContext(obj);

    forEach(this._safeEvent(), function(handlerItems) {
      self._spliceMatches(handlerItems, matchFunc);
    });
  }
};

/**
 * Unbind custom events
 * @param {(string|object|function)} eventName - event name or context or
 *  {name: handler} pair object or handler function
 * @param {(function)} handler - handler function
 * @example
 * //-- #1. Get Module --//
 * var CustomEvents = require('tui-code-snippet/customEvents/customEvents'); // node, commonjs
 *
 * //-- #2. Use method --//
 * // # 2.1 off by event name
 * CustomEvents.off('onload');
 *
 * // # 2.2 off by event name and handler
 * CustomEvents.off('play', handler);
 *
 * // # 2.3 off by handler
 * CustomEvents.off(handler);
 *
 * // # 2.4 off by context
 * CustomEvents.off(myObj);
 *
 * // # 2.5 off by context and handler
 * CustomEvents.off(myObj, handler);
 *
 * // # 2.6 off by context and event name
 * CustomEvents.off(myObj, 'onload');
 *
 * // # 2.7 off by an Object.<string, function> that is {eventName: handler}
 * CustomEvents.off({
 *   'play': handler,
 *   'pause': handler2
 * });
 *
 * // # 2.8 off the all events
 * CustomEvents.off();
 */
CustomEvents.prototype.off = function(eventName, handler) {
  if (isString(eventName)) {
    // [syntax 1, 2]
    this._offByEventName(eventName, handler);
  } else if (!arguments.length) {
    // [syntax 8]
    this.events = {};
    this.contexts = [];
  } else if (isFunction(eventName)) {
    // [syntax 3]
    this._offByHandler(eventName);
  } else if (isObject(eventName)) {
    // [syntax 4, 5, 6]
    this._offByObject(eventName, handler);
  }
};

/**
 * Fire custom event
 * @param {string} eventName - name of custom event
 */
CustomEvents.prototype.fire = function(eventName) {  // eslint-disable-line
  this.invoke.apply(this, arguments);
};

/**
 * Fire a event and returns the result of operation 'boolean AND' with all
 *  listener's results.
 *
 * So, It is different from {@link CustomEvents#fire}.
 *
 * In service code, use this as a before event in component level usually
 *  for notifying that the event is cancelable.
 * @param {string} eventName - Custom event name
 * @param {...*} data - Data for event
 * @returns {boolean} The result of operation 'boolean AND'
 * @example
 * var map = new Map();
 * map.on({
 *     'beforeZoom': function() {
 *         // It should cancel the 'zoom' event by some conditions.
 *         if (that.disabled && this.getState()) {
 *             return false;
 *         }
 *         return true;
 *     }
 * });
 *
 * if (this.invoke('beforeZoom')) {    // check the result of 'beforeZoom'
 *     // if true,
 *     // doSomething
 * }
 */
CustomEvents.prototype.invoke = function(eventName) {
  var events, args, index, item;

  if (!this.hasListener(eventName)) {
    return true;
  }

  events = this._safeEvent(eventName);
  args = Array.prototype.slice.call(arguments, 1);
  index = 0;

  while (events[index]) {
    item = events[index];

    if (item.handler.apply(item.context, args) === false) {
      return false;
    }

    index += 1;
  }

  return true;
};

/**
 * Return whether at least one of the handlers is registered in the given
 *  event name.
 * @param {string} eventName - Custom event name
 * @returns {boolean} Is there at least one handler in event name?
 */
CustomEvents.prototype.hasListener = function(eventName) {
  return this.getListenerLength(eventName) > 0;
};

/**
 * Return a count of events registered.
 * @param {string} eventName - Custom event name
 * @returns {number} number of event
 */
CustomEvents.prototype.getListenerLength = function(eventName) {
  var events = this._safeEvent(eventName);

  return events.length;
};

module.exports = CustomEvents;


/***/ }),
/* 9 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview
 * This module provides a function to make a constructor
 * that can inherit from the other constructors like the CLASS easily.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var inherit = __webpack_require__(27);
var extend = __webpack_require__(2);

/**
 * @module defineClass
 */

/**
 * Help a constructor to be defined and to inherit from the other constructors
 * @param {*} [parent] Parent constructor
 * @param {Object} props Members of constructor
 *  @param {Function} props.init Initialization method
 *  @param {Object} [props.static] Static members of constructor
 * @returns {*} Constructor
 * @memberof module:defineClass
 * @example
 * var defineClass = require('tui-code-snippet/defineClass/defineClass'); // node, commonjs
 *
 * //-- #2. Use property --//
 * var Parent = defineClass({
 *     init: function() { // constuructor
 *         this.name = 'made by def';
 *     },
 *     method: function() {
 *         // ...
 *     },
 *     static: {
 *         staticMethod: function() {
 *              // ...
 *         }
 *     }
 * });
 *
 * var Child = defineClass(Parent, {
 *     childMethod: function() {}
 * });
 *
 * Parent.staticMethod();
 *
 * var parentInstance = new Parent();
 * console.log(parentInstance.name); //made by def
 * parentInstance.staticMethod(); // Error
 *
 * var childInstance = new Child();
 * childInstance.method();
 * childInstance.childMethod();
 */
function defineClass(parent, props) {
  var obj;

  if (!props) {
    props = parent;
    parent = null;
  }

  obj = props.init || function() {};

  if (parent) {
    inherit(obj, parent);
  }

  if (props.hasOwnProperty('static')) {
    extend(obj, props['static']);
    delete props['static'];
  }

  extend(obj.prototype, props);

  return obj;
}

module.exports = defineClass;


/***/ }),
/* 10 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Bind DOM events
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isString = __webpack_require__(6);
var forEach = __webpack_require__(4);

var safeEvent = __webpack_require__(17);

/**
 * Bind DOM events.
 * @param {HTMLElement} element - element to bind events
 * @param {(string|object)} types - Space splitted events names or eventName:handler object
 * @param {(function|object)} handler - handler function or context for handler method
 * @param {object} [context] context - context for handler method.
 * @memberof module:domEvent
 * @example
 * var div = document.querySelector('div');
 * 
 * // Bind one event to an element.
 * on(div, 'click', toggle);
 * 
 * // Bind multiple events with a same handler to multiple elements at once.
 * // Use event names splitted by a space.
 * on(div, 'mouseenter mouseleave', changeColor);
 * 
 * // Bind multiple events with different handlers to an element at once.
 * // Use an object which of key is an event name and value is a handler function.
 * on(div, {
 *   keydown: highlight,
 *   keyup: dehighlight
 * });
 * 
 * // Set a context for handler method.
 * var name = 'global';
 * var repository = {name: 'CodeSnippet'};
 * on(div, 'drag', function() {
 *  console.log(this.name);
 * }, repository);
 * // Result when you drag a div: "CodeSnippet"
 */
function on(element, types, handler, context) {
  if (isString(types)) {
    forEach(types.split(/\s+/g), function(type) {
      bindEvent(element, type, handler, context);
    });

    return;
  }

  forEach(types, function(func, type) {
    bindEvent(element, type, func, handler);
  });
}

/**
 * Bind DOM events
 * @param {HTMLElement} element - element to bind events
 * @param {string} type - events name
 * @param {function} handler - handler function or context for handler method
 * @param {object} [context] context - context for handler method.
 * @private
 */
function bindEvent(element, type, handler, context) {
  /**
     * Event handler
     * @param {Event} e - event object
     */
  function eventHandler(e) {
    handler.call(context || element, e || window.event);
  }

  if ('addEventListener' in element) {
    element.addEventListener(type, eventHandler);
  } else if ('attachEvent' in element) {
    element.attachEvent('on' + type, eventHandler);
  }
  memorizeHandler(element, type, handler, eventHandler);
}

/**
 * Memorize DOM event handler for unbinding.
 * @param {HTMLElement} element - element to bind events
 * @param {string} type - events name
 * @param {function} handler - handler function that user passed at on() use
 * @param {function} wrappedHandler - handler function that wrapped by domevent for implementing some features
 * @private
 */
function memorizeHandler(element, type, handler, wrappedHandler) {
  var events = safeEvent(element, type);
  var existInEvents = false;

  forEach(events, function(obj) {
    if (obj.handler === handler) {
      existInEvents = true;

      return false;
    }

    return true;
  });

  if (!existInEvents) {
    events.push({
      handler: handler,
      wrappedHandler: wrappedHandler
    });
  }
}

module.exports = on;


/***/ }),
/* 11 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Unbind DOM events
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isString = __webpack_require__(6);
var forEach = __webpack_require__(4);

var safeEvent = __webpack_require__(17);

/**
 * Unbind DOM events
 * If a handler function is not passed, remove all events of that type.
 * @param {HTMLElement} element - element to unbind events
 * @param {(string|object)} types - Space splitted events names or eventName:handler object
 * @param {function} [handler] - handler function
 * @memberof module:domEvent
 * @example
 * // Following the example of domEvent#on
 * 
 * // Unbind one event from an element.
 * off(div, 'click', toggle);
 * 
 * // Unbind multiple events with a same handler from multiple elements at once.
 * // Use event names splitted by a space.
 * off(element, 'mouseenter mouseleave', changeColor);
 * 
 * // Unbind multiple events with different handlers from an element at once.
 * // Use an object which of key is an event name and value is a handler function.
 * off(div, {
 *   keydown: highlight,
 *   keyup: dehighlight
 * });
 * 
 * // Unbind events without handlers.
 * off(div, 'drag');
 */
function off(element, types, handler) {
  if (isString(types)) {
    forEach(types.split(/\s+/g), function(type) {
      unbindEvent(element, type, handler);
    });

    return;
  }

  forEach(types, function(func, type) {
    unbindEvent(element, type, func);
  });
}

/**
 * Unbind DOM events
 * If a handler function is not passed, remove all events of that type.
 * @param {HTMLElement} element - element to unbind events
 * @param {string} type - events name
 * @param {function} [handler] - handler function
 * @private
 */
function unbindEvent(element, type, handler) {
  var events = safeEvent(element, type);
  var index;

  if (!handler) {
    forEach(events, function(item) {
      removeHandler(element, type, item.wrappedHandler);
    });
    events.splice(0, events.length);
  } else {
    forEach(events, function(item, idx) {
      if (handler === item.handler) {
        removeHandler(element, type, item.wrappedHandler);
        index = idx;

        return false;
      }

      return true;
    });
    events.splice(index, 1);
  }
}

/**
 * Remove an event handler
 * @param {HTMLElement} element - An element to remove an event
 * @param {string} type - event type
 * @param {function} handler - event handler
 * @private
 */
function removeHandler(element, type, handler) {
  if ('removeEventListener' in element) {
    element.removeEventListener(type, handler);
  } else if ('detachEvent' in element) {
    element.detachEvent('on' + type, handler);
  }
}

module.exports = off;


/***/ }),
/* 12 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Find parent element recursively
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var matches = __webpack_require__(30);

/**
 * Find parent element recursively
 * @param {HTMLElement} element - base element to start find
 * @param {string} selector - selector string for find
 * @returns {HTMLElement} - element finded or null
 * @memberof module:domUtil
 */
function closest(element, selector) {
  var parent = element.parentNode;

  if (matches(element, selector)) {
    return element;
  }

  while (parent && parent !== document) {
    if (matches(parent, selector)) {
      return parent;
    }

    parent = parent.parentNode;
  }

  return null;
}

module.exports = closest;


/***/ }),
/* 13 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Remove element from parent node.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Remove element from parent node.
 * @param {HTMLElement} element - element to remove.
 * @memberof module:domUtil
 */
function removeElement(element) {
  if (element && element.parentNode) {
    element.parentNode.removeChild(element);
  }
}

module.exports = removeElement;


/***/ }),
/* 14 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is a instance of HTMLNode or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is a instance of HTMLNode or not.
 * If the given variables is a instance of HTMLNode, return true.
 * @param {*} html - Target for checking
 * @returns {boolean} Is HTMLNode ?
 * @memberof module:type
 */
function isHTMLNode(html) {
  if (typeof HTMLElement === 'object') {
    return (html && (html instanceof HTMLElement || !!html.nodeType));
  }

  return !!(html && html.nodeType);
}

module.exports = isHTMLNode;


/***/ }),
/* 15 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var inArray = __webpack_require__(0);
var forEachArray = __webpack_require__(1);
var sendHostname = __webpack_require__(35);

var uniqueId = 0;

/**
 * Utils
 * @namespace util
 * @ignore
 */
var utils = {
  /**
   * Returns unique id
   * @returns {number}
   */
  getUniqueId: function() {
    uniqueId += 1;

    return uniqueId;
  },

  /**
   * Convert a value to meet the format
   * @param {number|string} value
   * @param {string} format - ex) hh, h, mm, m
   * @returns {string}
   */
  formatTime: function(value, format) {
    var PADDING_ZERO_TYPES = ['hh', 'mm'];
    value = String(value);

    return inArray(format, PADDING_ZERO_TYPES) >= 0 && value.length === 1 ? '0' + value : value;
  },

  /**
   * Get meridiem hour
   * @param {number} hour - Original hour
   * @returns {number} Converted meridiem hour
   */
  getMeridiemHour: function(hour) {
    hour %= 12;

    if (hour === 0) {
      hour = 12;
    }

    return hour;
  },

  /**
   * Returns range arr
   * @param {number} start - Start value
   * @param {number} end - End value
   * @param {number} [step] - Step value
   * @returns {Array}
   */
  getRangeArr: function(start, end, step) {
    var arr = [];
    var i;

    step = step || 1;

    if (start > end) {
      for (i = end; i >= start; i -= step) {
        arr.push(i);
      }
    } else {
      for (i = start; i <= end; i += step) {
        arr.push(i);
      }
    }

    return arr;
  },

  /**
   * Returns array filled with selected value
   * @param {number} start - Start index of array to fill
   * @param {number} end - End index of array to fill
   * @param {number} value - Value to be filled
   * @param {Array} [target] - Array to fill
   * @returns {Array}
   */
  fill: function(start, end, value, target) {
    var arr = target || [];
    var replaceEnd = Math.min(arr.length - 1, end);
    var i;

    for (i = start; i <= replaceEnd; i += 1) {
      arr[i] = value;
    }

    for (i = replaceEnd; i <= end; i += 1) {
      arr.push(value);
    }

    return arr;
  },

  /**
   * Get a target element
   * @param {Event} ev Event object
   * @returns {HTMLElement} An event target element
   */

  getTarget: function(ev) {
    return ev.target || ev.srcElement;
  },

  /**
   * send host name
   * @ignore
   */
  sendHostName: function() {
    sendHostname('time-picker', 'UA-129987462-1');
  },

  /**
   * Get disabled minute array
   * @param {Array} enableRanges array of object which contains range
   */
  getDisabledMinuteArr: function(enableRanges, minuteStep) {
    var arr = this.fill(0, Math.floor(60 / minuteStep) - 2, false);

    function setDisabled(enableRange) {
      var beginDisabledMinute = Math.ceil(enableRange.begin / minuteStep);
      var endDisabledMinute = Math.floor(enableRange.end / minuteStep);

      arr = this.fill(beginDisabledMinute, endDisabledMinute, true, arr);
    }

    forEachArray(enableRanges, setDisabled.bind(this));

    return arr;
  },

  /**
   * Set disabled on target element
   * @param {HTMLInputElement} el target element
   * @param {boolean} isDisabled target element
   */
  setDisabled: function(el, isDisabled) {
    el.disabled = isDisabled;
  }
};

module.exports = utils;


/***/ }),
/* 16 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Execute the provided callback once for each property of object which actually exist.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Execute the provided callback once for each property of object which actually exist.
 * If the callback function returns false, the loop will be stopped.
 * Callback function(iteratee) is invoked with three arguments:
 *  1) The value of the property
 *  2) The name of the property
 *  3) The object being traversed
 * @param {Object} obj The object that will be traversed
 * @param {function} iteratee  Callback function
 * @param {Object} [context] Context(this) of callback function
 * @memberof module:collection
 * @example
 * var forEachOwnProperties = require('tui-code-snippet/collection/forEachOwnProperties'); // node, commonjs
 *
 * var sum = 0;
 *
 * forEachOwnProperties({a:1,b:2,c:3}, function(value){
 *     sum += value;
 * });
 * alert(sum); // 6
 */
function forEachOwnProperties(obj, iteratee, context) {
  var key;

  context = context || null;

  for (key in obj) {
    if (obj.hasOwnProperty(key)) {
      if (iteratee.call(context, obj[key], key, obj) === false) {
        break;
      }
    }
  }
}

module.exports = forEachOwnProperties;


/***/ }),
/* 17 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Get event collection for specific HTML element
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var EVENT_KEY = '_feEventKey';

/**
 * Get event collection for specific HTML element
 * @param {HTMLElement} element - HTML element
 * @param {string} type - event type
 * @returns {array}
 * @private
 */
function safeEvent(element, type) {
  var events = element[EVENT_KEY];
  var handlers;

  if (!events) {
    events = element[EVENT_KEY] = {};
  }

  handlers = events[type];
  if (!handlers) {
    handlers = events[type] = [];
  }

  return handlers;
}

module.exports = safeEvent;


/***/ }),
/* 18 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Get HTML element's design classes.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isUndefined = __webpack_require__(5);

/**
 * Get HTML element's design classes.
 * @param {(HTMLElement|SVGElement)} element target element
 * @returns {string} element css class name
 * @memberof module:domUtil
 */
function getClass(element) {
  if (!element || !element.className) {
    return '';
  }

  if (isUndefined(element.className.baseVal)) {
    return element.className;
  }

  return element.className.baseVal;
}

module.exports = getClass;


/***/ }),
/* 19 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Set className value
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isArray = __webpack_require__(3);
var isUndefined = __webpack_require__(5);

/**
 * Set className value
 * @param {(HTMLElement|SVGElement)} element - target element
 * @param {(string|string[])} cssClass - class names
 * @private
 */
function setClassName(element, cssClass) {
  cssClass = isArray(cssClass) ? cssClass.join(' ') : cssClass;

  cssClass = cssClass.replace(/^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g, '');

  if (isUndefined(element.className.baseVal)) {
    element.className = cssClass;

    return;
  }

  element.className.baseVal = cssClass;
}

module.exports = setClassName;


/***/ }),
/* 20 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


__webpack_require__(21);

module.exports = __webpack_require__(22);


/***/ }),
/* 21 */
/***/ (function(module, exports, __webpack_require__) {

// extracted by mini-css-extract-plugin

/***/ }),
/* 22 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var inArray = __webpack_require__(0);
var forEachArray = __webpack_require__(1);
var CustomEvents = __webpack_require__(8);
var defineClass = __webpack_require__(9);
var extend = __webpack_require__(2);
var on = __webpack_require__(10);
var off = __webpack_require__(11);
var addClass = __webpack_require__(29);
var closest = __webpack_require__(12);
var removeElement = __webpack_require__(13);
var removeClass = __webpack_require__(32);
var isHTMLNode = __webpack_require__(14);
var isNumber = __webpack_require__(33);

var Spinbox = __webpack_require__(34);
var Selectbox = __webpack_require__(38);
var util = __webpack_require__(15);
var localeTexts = __webpack_require__(40);
var tmpl = __webpack_require__(41);
var meridiemTmpl = __webpack_require__(42);

var SELECTOR_HOUR_ELEMENT = '.tui-timepicker-hour';
var SELECTOR_MINUTE_ELEMENT = '.tui-timepicker-minute';
var SELECTOR_MERIDIEM_ELEMENT = '.tui-timepicker-meridiem';
var CLASS_NAME_LEFT_MERIDIEM = 'tui-has-left';
var CLASS_NAME_HIDDEN = 'tui-hidden';
var CLASS_NAME_CHECKED = 'tui-timepicker-meridiem-checked';
var INPUT_TYPE_SPINBOX = 'spinbox';
var INPUT_TYPE_SELECTBOX = 'selectbox';

var START_NUMBER_OF_TIME = 0;
var END_NUMBER_OF_MINUTE = 59;
var END_NUMBER_OF_HOUR = 23;
var END_NUMBER_OF_HOUR_WITH_MERIDIEM = 12;

/**
 * Merge default options
 * @ignore
 * @param {object} options - options
 * @returns {object} Merged options
 */
var mergeDefaultOptions = function(options) {
  return extend(
    {
      language: 'en',
      initialHour: 0,
      initialMinute: 0,
      showMeridiem: true,
      inputType: 'selectbox',
      hourStep: 1,
      minuteStep: 1,
      meridiemPosition: 'right',
      format: 'h:m',
      disabledHours: [],
      disabledMinutes: {},
      usageStatistics: true
    },
    options
  );
};

/**
 * @class
 * @param {string|HTMLElement} container - Container element or selector
 * @param {Object} [options] - Options for initialization
 * @param {number} [options.initialHour = 0] - Initial setting value of hour
 * @param {number} [options.initialMinute = 0] - Initial setting value of minute
 * @param {number} [options.hourStep = 1] - Step value of hour
 * @param {number} [options.minuteStep = 1] - Step value of minute
 * @param {string} [options.inputType = 'selectbox'] - 'selectbox' or 'spinbox'
 * @param {string} [options.format = 'h:m'] - hour, minute format for display
 * @param {boolean} [options.showMeridiem = true] - Show meridiem expression?
 * @param {Array} [options.disabledHours = []] - Registered Hours is disabled.
 * @param {Object} [options.disabledMinutes = {}] - Registered Minutes of selected hours is disabled.
 * @param {Object} [options.disabledMinutes.hour] - Key must be hour(number).
 *                 Value is array which contains only true or false and must be 60 of length
 * @param {string} [options.meridiemPosition = 'right'] - Set location of the meridiem element.
 *                 If this option set 'left', the meridiem element is created in front of the hour element.
 * @param {string} [options.language = 'en'] Set locale texts
 * @param {Boolean} [options.usageStatistics=true|false] send hostname to google analytics [default value is true]
 * @example
 * // ES6
 * import TimePicker from 'tui-time-picker';
 *
 * // CommonJS
 * const TimePicker = require('tui-time-picker');
 *
 * // Browser
 * const TimePicker = tui.TimePicker;
 *
 * const timepicker = new TimePicker('#timepicker-container', {
 *     initialHour: 15,
 *     initialMinute: 13,
 *     inputType: 'selectbox',
 *     showMeridiem: false
 * });
 */
var TimePicker = defineClass(
  /** @lends TimePicker.prototype */ {
    static: {
      /**
       * Locale text data
       * @type {object}
       * @memberof TimePicker
       * @static
       * @example
       * TimePicker.localeTexts['customKey'] = {
       *     am: 'a.m.',
       *     pm: 'p.m.'
       * };
       *
       * const instance = new TimePicker('#timepicker-container', {
       *     language: 'customKey',
       * });
       */
      localeTexts: localeTexts
    },
    init: function(container, options) {
      options = mergeDefaultOptions(options);

      /**
       * @type {number}
       * @private
       */
      this.id = util.getUniqueId();

      /**
       * @type {HTMLElement}
       * @private
       */
      this.container = isHTMLNode(container) ? container : document.querySelector(container);

      /**
       * @type {HTMLElement}
       * @private
       */
      this.element = null;

      /**
       * @type {HTMLElement}
       * @private
       */
      this.meridiemElement = null;

      /**
       * @type {HTMLElement}
       * @private
       */
      this.amEl = null;

      /**
       * @type {HTMLElement}
       * @private
       */
      this.pmEl = null;

      /**
       * @type {boolean}
       * @private
       */
      this.showMeridiem = options.showMeridiem;

      /**
       * Meridiem postion
       * @type {'left'|'right'}
       * @private
       */
      this.meridiemPosition = options.meridiemPosition;

      /**
       * @type {Spinbox|Selectbox}
       * @private
       */
      this.hourInput = null;

      /**
       * @type {Spinbox|Selectbox}
       * @private
       */
      this.minuteInput = null;

      /**
       * @type {number}
       * @private
       */
      this.hour = options.initialHour;

      /**
       * @type {number}
       * @private
       */
      this.minute = options.initialMinute;

      /**
       * @type {number}
       * @private
       */
      this.hourStep = options.hourStep;

      /**
       * @type {number}
       * @private
       */
      this.minuteStep = options.minuteStep;

      /**
       * @type {Array}
       * @private
       */
      this.disabledHours = options.disabledHours;

      /**
       * @type {Object}
       * @private
       */
      this.disabledMinutes = options.disabledMinutes;

      /**
       * TimePicker inputType
       * @type {'spinbox'|'selectbox'}
       * @private
       */
      this.inputType = options.inputType;

      /**
       * Locale text for meridiem
       * @type {string}
       * @private
       */
      this.localeText = localeTexts[options.language];

      /**
       * Time format for output
       * @type {string}
       * @private
       */
      this.format = this.getValidTimeFormat(options.format);

      this.render();
      this.setEvents();

      if (options.usageStatistics) {
        util.sendHostName();
      }
    },

    /**
     * Set event handlers to selectors, container
     * @private
     */
    setEvents: function() {
      this.hourInput.on('change', this.onChangeTimeInput, this);
      this.minuteInput.on('change', this.onChangeTimeInput, this);

      if (this.showMeridiem) {
        if (this.inputType === INPUT_TYPE_SELECTBOX) {
          on(this.meridiemElement.querySelector('select'), 'change', this.onChangeMeridiem, this);
        } else if (this.inputType === INPUT_TYPE_SPINBOX) {
          on(this.meridiemElement, 'click', this.onChangeMeridiem, this);
        }
      }
    },

    /**
     * Remove events
     * @private
     */
    removeEvents: function() {
      this.off();

      this.hourInput.destroy();
      this.minuteInput.destroy();

      if (this.showMeridiem) {
        if (this.inputType === INPUT_TYPE_SELECTBOX) {
          off(this.meridiemElement.querySelector('select'), 'change', this.onChangeMeridiem, this);
        } else if (this.inputType === INPUT_TYPE_SPINBOX) {
          off(this.meridiemElement, 'click', this.onChangeMeridiem, this);
        }
      }
    },

    /**
     * Render element
     * @private
     */
    render: function() {
      var context = {
        showMeridiem: this.showMeridiem,
        isSpinbox: this.inputType === 'spinbox'
      };

      if (this.showMeridiem) {
        extend(context, {
          meridiemElement: this.makeMeridiemHTML()
        });
      }

      if (this.element) {
        removeElement(this.element);
      }
      this.container.innerHTML = tmpl(context);
      this.element = this.container.firstChild;

      this.renderTimeInputs();

      if (this.showMeridiem) {
        this.setMeridiemElement();
      }
    },

    /**
     * Set meridiem element on timepicker
     * @private
     */
    setMeridiemElement: function() {
      if (this.meridiemPosition === 'left') {
        addClass(this.element, CLASS_NAME_LEFT_MERIDIEM);
      }
      this.meridiemElement = this.element.querySelector(SELECTOR_MERIDIEM_ELEMENT);
      this.amEl = this.meridiemElement.querySelector('[value="AM"]');
      this.pmEl = this.meridiemElement.querySelector('[value="PM"]');
      this.syncToMeridiemElements();
    },

    /**
     * Make html for meridiem element
     * @returns {HTMLElement} Meridiem element
     * @private
     */
    makeMeridiemHTML: function() {
      var localeText = this.localeText;

      return meridiemTmpl({
        am: localeText.am,
        pm: localeText.pm,
        radioId: this.id,
        isSpinbox: this.inputType === 'spinbox'
      });
    },

    /**
     * Render time selectors
     * @private
     */
    renderTimeInputs: function() {
      var hour = this.hour;
      var showMeridiem = this.showMeridiem;
      var hourElement = this.element.querySelector(SELECTOR_HOUR_ELEMENT);
      var minuteElement = this.element.querySelector(SELECTOR_MINUTE_ELEMENT);
      var BoxComponent = this.inputType.toLowerCase() === 'selectbox' ? Selectbox : Spinbox;
      var formatExplode = this.format.split(':');
      var hourItems = this.getHourItems();

      if (showMeridiem) {
        hour = util.getMeridiemHour(hour);
      }

      this.hourInput = new BoxComponent(hourElement, {
        initialValue: hour,
        items: hourItems,
        format: formatExplode[0],
        disabledItems: this.makeDisabledStatItems(hourItems)
      });

      this.minuteInput = new BoxComponent(minuteElement, {
        initialValue: this.minute,
        items: this.getMinuteItems(),
        format: formatExplode[1]
      });
    },

    makeDisabledStatItems: function(hourItems) {
      var result = [];
      var disabledHours = this.disabledHours.slice();

      if (this.showMeridiem) {
        disabledHours = this.meridiemableTime(disabledHours);
      }

      forEachArray(hourItems, function(hour) {
        result.push(inArray(hour, disabledHours) >= 0);
      });

      return result;
    },

    meridiemableTime: function(disabledHours) {
      var diffHour = 0;
      var startHour = 0;
      var endHour = 11;
      var result = [];

      if (this.hour >= 12) {
        diffHour = 12;
        startHour = 12;
        endHour = 23;
      }

      forEachArray(disabledHours, function(hour) {
        if (hour >= startHour && hour <= endHour) {
          result.push(hour - diffHour === 0 ? 12 : hour - diffHour);
        }
      });

      return result;
    },

    /**
     * Return formatted format.
     * @param {string} format - format option
     * @returns {string}
     * @private
     */
    getValidTimeFormat: function(format) {
      if (!format.match(/^[h]{1,2}:[m]{1,2}$/i)) {
        return 'h:m';
      }

      return format.toLowerCase();
    },

    /**
     * Initialize meridiem elements
     * @private
     */
    syncToMeridiemElements: function() {
      var selectedEl = this.hour >= 12 ? this.pmEl : this.amEl;
      var notSelectedEl = selectedEl === this.pmEl ? this.amEl : this.pmEl;

      selectedEl.setAttribute('selected', true);
      selectedEl.setAttribute('checked', true);
      addClass(selectedEl, CLASS_NAME_CHECKED);
      notSelectedEl.removeAttribute('selected');
      notSelectedEl.removeAttribute('checked');
      removeClass(notSelectedEl, CLASS_NAME_CHECKED);
    },

    /**
     * Set values in spinboxes from time
     * @private
     */
    syncToInputs: function() {
      var hour = this.hour;
      var minute = this.minute;

      if (this.showMeridiem) {
        hour = util.getMeridiemHour(hour);
      }

      this.hourInput.setValue(hour, true);
      this.minuteInput.setValue(minute, true);
    },

    /**
     * DOM event handler
     * @param {Event} ev - Change event on meridiem element
     * @private
     */
    onChangeMeridiem: function(ev) {
      var hour = this.hour;
      var target = util.getTarget(ev);

      if (target.value && closest(target, SELECTOR_MERIDIEM_ELEMENT)) {
        hour = this.to24Hour(target.value === 'PM', hour);
        this.setTime(hour, this.minute);
        this.setDisabledHours();
        this.setDisabledMinutes(hour);
      }
    },

    /**
     * Time change event handler
     * @private
     */
    onChangeTimeInput: function() {
      var hour = this.hourInput.getValue();
      var minute = this.minuteInput.getValue();
      var isPM = this.hour >= 12;

      if (this.showMeridiem) {
        hour = this.to24Hour(isPM, hour);
      }
      this.setTime(hour, minute);
      this.setDisabledMinutes(hour);
    },

    /**
     * 12Hour-expression to 24Hour-expression
     * @param {boolean} isPM - Is pm?
     * @param {number} hour - Hour
     * @returns {number}
     * @private
     */
    to24Hour: function(isPM, hour) {
      hour %= 12;
      if (isPM) {
        hour += 12;
      }

      return hour;
    },

    setDisabledHours: function() {
      var hourItems = this.getHourItems();
      var disabledItems = this.makeDisabledStatItems(hourItems);

      this.hourInput.setDisabledItems(disabledItems);
    },

    setDisabledMinutes: function(hour) {
      var disabledItems;
      disabledItems = this.disabledMinutes[hour] || [];

      this.minuteInput.setDisabledItems(disabledItems);
    },

    /**
     * Get items of hour
     * @returns {array} Hour item list
     * @private
     */
    getHourItems: function() {
      var step = this.hourStep;

      return this.showMeridiem ? util.getRangeArr(1, 12, step) : util.getRangeArr(0, 23, step);
    },

    /**
     * Get items of minute
     * @returns {array} Minute item list
     * @private
     */
    getMinuteItems: function() {
      return util.getRangeArr(0, 59, this.minuteStep);
    },

    /**
     * Whether the hour and minute are in valid items or not
     * @param {number} hour - Hour value
     * @param {number} minute - Minute value
     * @returns {boolean} State
     * @private
     */
    validItems: function(hour, minute) {
      if (!isNumber(hour) || !isNumber(minute)) {
        return false;
      }

      if (this.showMeridiem) {
        hour = util.getMeridiemHour(hour);
      }

      return inArray(hour, this.getHourItems()) > -1 && inArray(minute, this.getMinuteItems()) > -1;
    },

    /**
     * Set step of hour
     * @param {array} step - Step to create items of hour
     */
    setHourStep: function(step) {
      this.hourStep = step;
      this.hourInput.fire('changeItems', this.getHourItems());
    },

    /**
     * Get step of hour
     * @returns {number} Step of hour
     */
    getHourStep: function() {
      return this.hourStep;
    },

    /**
     * Set step of minute
     * @param {number} step - Step to create items of minute
     */
    setMinuteStep: function(step) {
      this.minuteStep = step;
      this.minuteInput.fire('changeItems', this.getMinuteItems());
    },

    /**
     * Get step of minute
     * @returns {number} Step of minute
     */
    getMinuteStep: function() {
      return this.minuteStep;
    },

    /**
     * Show time picker element
     */
    show: function() {
      removeClass(this.element, CLASS_NAME_HIDDEN);
    },

    /**
     * Hide time picker element
     */
    hide: function() {
      addClass(this.element, CLASS_NAME_HIDDEN);
    },

    /**
     * Set hour
     * @param {number} hour for time picker - (0~23)
     * @returns {boolean} result of set time
     */
    setHour: function(hour) {
      return this.setTime(hour, this.minute);
    },

    /**
     * Set minute
     * @param {number} minute for time picker
     * @returns {boolean} result of set time
     */
    setMinute: function(minute) {
      return this.setTime(this.hour, minute);
    },

    /**
     * Set time
     * @param {number} hour for time picker - (0~23)
     * @param {number} minute for time picker
     * @param {boolean} [silent] if it set true, 'change' event will not be fired.
     */
    setTime: function(hour, minute, silent) {
      if (!this.validItems(hour, minute)) {
        return;
      }

      this.hour = hour;
      this.minute = minute;

      this.syncToInputs();
      if (this.showMeridiem) {
        this.syncToMeridiemElements();
      }

      /**
       * Change event - TimePicker
       * @event TimePicker#change
       * @type {object} event - Event object
       * @property {number} hour - changed hour
       * @property {number} minute - changed minute
       * @example
       * timepicker.on('change', (e) => {
       *   console.log(e.hour, e.minute);
       * });
       */
      if (!silent) {
        this.fire('change', {
          hour: this.hour,
          minute: this.minute
        });
      }
    },

    /**
     * Set selectable range
     * @param {Object} begin - Contain begin hour and minute of range
     * @param {number} begin.hour - begin hour of range
     * @param {number} begin.minute - begin minute of range
     * @param {Object} [end] - Contain end hour and minute of range
     * @param {number} end.hour - end hour of range
     * @param {number} end.minute - end minute of range
     */
    setRange: function(begin, end) {
      var beginHour = begin.hour;
      var beginMin = begin.minute;
      var endHour, endMin;

      if (!this.isValidRange(begin, end)) {
        return;
      }

      if (end) {
        endHour = end.hour;
        endMin = end.minute;
      }

      this.setRangeHour(beginHour, endHour);
      this.setRangeMinute(beginHour, beginMin, endHour, endMin);

      this.applyRange(beginHour, beginMin, endHour);
    },

    /**
     * Set selectable range on hour
     * @param {number} beginHour - begin hour of range
     * @param {number} [endHour] - end hour of range
     * @private
     */
    setRangeHour: function(beginHour, endHour) {
      var disabledHours = util.getRangeArr(START_NUMBER_OF_TIME, beginHour - 1);

      if (endHour) {
        disabledHours = disabledHours.concat(util.getRangeArr(endHour + 1, END_NUMBER_OF_HOUR));
      }

      this.disabledHours = disabledHours.slice();
    },

    /**
     * Set selectable range on minute
     * @param {number} beginHour - begin hour of range
     * @param {number} beginMin - begin minute of range
     * @param {number} [endHour] - end hour of range
     * @param {number} [endMin] - end minute of range
     * @private
     */
    setRangeMinute: function(beginHour, beginMin, endHour, endMin) {
      var disabledMinRanges = [];

      if (!beginHour && !beginMin) {
        return;
      }

      disabledMinRanges.push({
        begin: START_NUMBER_OF_TIME,
        end: beginMin
      });

      if (endHour && endMin) {
        disabledMinRanges.push({
          begin: endMin,
          end: END_NUMBER_OF_MINUTE
        });

        if (beginHour === endHour) {
          this.disabledMinutes[beginHour] = util
            .getDisabledMinuteArr(disabledMinRanges, this.minuteStep)
            .slice();

          return;
        }

        this.disabledMinutes[endHour] = util
          .getDisabledMinuteArr([disabledMinRanges[1]], this.minuteStep)
          .slice();
      }

      this.disabledMinutes[beginHour] = util
        .getDisabledMinuteArr([disabledMinRanges[0]], this.minuteStep)
        .slice();
    },

    /**
     * Apply range
     * @param {number} beginHour - begin hour of range
     * @param {number} beginMin - begin minute of range
     * @param {number} [endHour] - end hour of range
     * @private
     */
    // eslint-disable-next-line complexity
    applyRange: function(beginHour, beginMin, endHour) {
      var targetMinuteIndex = Math.ceil(beginMin / this.minuteStep);
      var targetHour = beginHour;
      var targetMinute = targetMinuteIndex * this.minuteStep;
      var diffFromSelectableMinute;

      if (this.isLaterThanSetTime(beginHour, beginMin)) {
        if (this.disabledMinutes[targetHour][targetMinuteIndex]) {
          diffFromSelectableMinute =
            this.disabledMinutes[targetHour]
              .slice(targetMinuteIndex)
              .findIndex(function(isMinuteDisabled) {
                return !isMinuteDisabled;
              }) * this.minuteStep;

          targetMinute =
            diffFromSelectableMinute >= 0 ? targetMinute + diffFromSelectableMinute : 60;
        }

        if ((this.hourStep !== 1 && beginHour % this.hourStep !== 1) || targetMinute >= 60) {
          targetHour = beginHour + (beginHour % this.hourStep) + 1;
          targetMinute = 0;
        }

        this.setTime(targetHour, targetMinute);
      }

      this.setDisabledHours();
      this.setDisabledMinutes(this.hour);

      if (this.showMeridiem) {
        this.syncToMeridiemElements();

        util.setDisabled(this.amEl, beginHour >= END_NUMBER_OF_HOUR_WITH_MERIDIEM);
        util.setDisabled(this.pmEl, endHour < END_NUMBER_OF_HOUR_WITH_MERIDIEM);
      }
    },

    /**
     * Reset minute selectable range
     */
    resetMinuteRange: function() {
      var i;

      this.disabledMinutes = {};

      for (i = 0; i <= END_NUMBER_OF_HOUR; i += 1) {
        this.setDisabledMinutes(this.hour);
      }
    },

    /**
     * Whether the given range a valid range
     * @param {Object} begin - Contain begin hour and minute of range
     * @param {number} begin.hour - begin hour of range
     * @param {number} begin.minute - begin minute of range
     * @param {Object} [end] - Contain end hour and minute of range
     * @param {number} end.hour - end hour of range
     * @param {number} end.minute - end minute of range
     * @returns {boolean} result of range validation
     * @private
     */
    isValidRange: function(begin, end) {
      var beginHour = begin.hour;
      var beginMin = begin.minute;
      var endHour, endMin;

      if (!this.isValidTime(beginHour, beginMin)) {
        return false;
      }

      if (!end) {
        return true;
      }

      endHour = end.hour;
      endMin = end.minute;

      return this.isValidTime(endHour, endMin) && this.compareTimes(begin, end) > 0;
    },

    /**
     * Whether the given time a valid time
     * @param {number} hour - hour for validation
     * @param {number} minute - minute for validation
     * @returns {boolean} result of time validation
     * @private
     */
    isValidTime: function(hour, minute) {
      return (
        hour >= START_NUMBER_OF_TIME &&
        hour <= END_NUMBER_OF_HOUR &&
        minute >= START_NUMBER_OF_TIME &&
        minute <= END_NUMBER_OF_MINUTE
      );
    },

    /**
     * Compare given time with set time
     * @param {number} hour - given hour
     * @param {number} minute - given minute
     * @returns {boolean} result of compare
     * @private
     */
    isLaterThanSetTime: function(hour, minute) {
      return hour > this.hour || (hour === this.hour && minute > this.minute);
    },

    /**
     * Compare two times
     * it returns
     *  0: when begin equals end
     *  positive: when end later than begin
     *  negative: when begin later than end
     * @param {Object} begin - Contain begin hour and minute of range
     * @param {number} begin.hour - begin hour of range
     * @param {number} begin.minute - begin minute of range
     * @param {Object} end - Contain end hour and minute of range
     * @param {number} end.hour - end hour of range
     * @param {number} end.minute - end minute of range
     * @returns {boolean} result of range validation
     * @private
     */
    compareTimes: function(begin, end) {
      var first = new Date(0);
      var second = new Date(0);

      first.setHours(begin.hour, begin.minute);
      second.setHours(end.hour, end.minute);

      return second.getTime() - first.getTime();
    },

    /**
     * Get hour
     * @returns {number} hour - (0~23)
     */
    getHour: function() {
      return this.hour;
    },

    /**
     * Get minute
     * @returns {number} minute
     */
    getMinute: function() {
      return this.minute;
    },

    /**
     * Change locale text of meridiem by language code
     * @param {string} language - Language code
     */
    changeLanguage: function(language) {
      this.localeText = localeTexts[language];
      this.render();
    },

    /**
     * Destroy
     */
    destroy: function() {
      this.removeEvents();
      removeElement(this.element);

      // eslint-disable-next-line max-len
      this.container = this.showMeridiem = this.hourInput = this.minuteInput = this.hour = this.minute = this.inputType = this.element = this.meridiemElement = this.amEl = this.pmEl = null;
    }
  }
);

CustomEvents.mixin(TimePicker);
module.exports = TimePicker;


/***/ }),
/* 23 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is existing or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isUndefined = __webpack_require__(5);
var isNull = __webpack_require__(24);

/**
 * Check whether the given variable is existing or not.
 * If the given variable is not null and not undefined, returns true.
 * @param {*} param - Target for checking
 * @returns {boolean} Is existy?
 * @memberof module:type
 * @example
 * var isExisty = require('tui-code-snippet/type/isExisty'); // node, commonjs
 *
 * isExisty(''); //true
 * isExisty(0); //true
 * isExisty([]); //true
 * isExisty({}); //true
 * isExisty(null); //false
 * isExisty(undefined); //false
*/
function isExisty(param) {
  return !isUndefined(param) && !isNull(param);
}

module.exports = isExisty;


/***/ }),
/* 24 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is null or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is null or not.
 * If the given variable(arguments[0]) is null, returns true.
 * @param {*} obj - Target for checking
 * @returns {boolean} Is null?
 * @memberof module:type
 */
function isNull(obj) {
  return obj === null;
}

module.exports = isNull;


/***/ }),
/* 25 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is an object or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is an object or not.
 * If the given variable is an object, return true.
 * @param {*} obj - Target for checking
 * @returns {boolean} Is object?
 * @memberof module:type
 */
function isObject(obj) {
  return obj === Object(obj);
}

module.exports = isObject;


/***/ }),
/* 26 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is a function or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is a function or not.
 * If the given variable is a function, return true.
 * @param {*} obj - Target for checking
 * @returns {boolean} Is function?
 * @memberof module:type
 */
function isFunction(obj) {
  return obj instanceof Function;
}

module.exports = isFunction;


/***/ }),
/* 27 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Provide a simple inheritance in prototype-oriented.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var createObject = __webpack_require__(28);

/**
 * Provide a simple inheritance in prototype-oriented.
 * Caution :
 *  Don't overwrite the prototype of child constructor.
 *
 * @param {function} subType Child constructor
 * @param {function} superType Parent constructor
 * @memberof module:inheritance
 * @example
 * var inherit = require('tui-code-snippet/inheritance/inherit'); // node, commonjs
 *
 * // Parent constructor
 * function Animal(leg) {
 *     this.leg = leg;
 * }
 * Animal.prototype.growl = function() {
 *     // ...
 * };
 *
 * // Child constructor
 * function Person(name) {
 *     this.name = name;
 * }
 *
 * // Inheritance
 * inherit(Person, Animal);
 *
 * // After this inheritance, please use only the extending of property.
 * // Do not overwrite prototype.
 * Person.prototype.walk = function(direction) {
 *     // ...
 * };
 */
function inherit(subType, superType) {
  var prototype = createObject(superType.prototype);
  prototype.constructor = subType;
  subType.prototype = prototype;
}

module.exports = inherit;


/***/ }),
/* 28 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Create a new object with the specified prototype object and properties.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * @module inheritance
 */

/**
 * Create a new object with the specified prototype object and properties.
 * @param {Object} obj This object will be a prototype of the newly-created object.
 * @returns {Object}
 * @memberof module:inheritance
 */
function createObject(obj) {
  function F() {} // eslint-disable-line require-jsdoc
  F.prototype = obj;

  return new F();
}

module.exports = createObject;


/***/ }),
/* 29 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Add css class to element
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var forEach = __webpack_require__(4);
var inArray = __webpack_require__(0);
var getClass = __webpack_require__(18);
var setClassName = __webpack_require__(19);

/**
 * domUtil module
 * @module domUtil
 */

/**
 * Add css class to element
 * @param {(HTMLElement|SVGElement)} element - target element
 * @param {...string} cssClass - css classes to add
 * @memberof module:domUtil
 */
function addClass(element) {
  var cssClass = Array.prototype.slice.call(arguments, 1);
  var classList = element.classList;
  var newClass = [];
  var origin;

  if (classList) {
    forEach(cssClass, function(name) {
      element.classList.add(name);
    });

    return;
  }

  origin = getClass(element);

  if (origin) {
    cssClass = [].concat(origin.split(/\s+/), cssClass);
  }

  forEach(cssClass, function(cls) {
    if (inArray(cls, newClass) < 0) {
      newClass.push(cls);
    }
  });

  setClassName(element, newClass);
}

module.exports = addClass;


/***/ }),
/* 30 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check element match selector
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var inArray = __webpack_require__(0);
var toArray = __webpack_require__(31);

var elProto = Element.prototype;
var matchSelector = elProto.matches ||
    elProto.webkitMatchesSelector ||
    elProto.mozMatchesSelector ||
    elProto.msMatchesSelector ||
    function(selector) {
      var doc = this.document || this.ownerDocument;

      return inArray(this, toArray(doc.querySelectorAll(selector))) > -1;
    };

/**
 * Check element match selector
 * @param {HTMLElement} element - element to check
 * @param {string} selector - selector to check
 * @returns {boolean} is selector matched to element?
 * @memberof module:domUtil
 */
function matches(element, selector) {
  return matchSelector.call(element, selector);
}

module.exports = matches;


/***/ }),
/* 31 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Transform the Array-like object to Array.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var forEachArray = __webpack_require__(1);

/**
 * Transform the Array-like object to Array.
 * In low IE (below 8), Array.prototype.slice.call is not perfect. So, try-catch statement is used.
 * @param {*} arrayLike Array-like object
 * @returns {Array} Array
 * @memberof module:collection
 * @example
 * var toArray = require('tui-code-snippet/collection/toArray'); // node, commonjs
 *
 * var arrayLike = {
 *     0: 'one',
 *     1: 'two',
 *     2: 'three',
 *     3: 'four',
 *     length: 4
 * };
 * var result = toArray(arrayLike);
 *
 * alert(result instanceof Array); // true
 * alert(result); // one,two,three,four
 */
function toArray(arrayLike) {
  var arr;
  try {
    arr = Array.prototype.slice.call(arrayLike);
  } catch (e) {
    arr = [];
    forEachArray(arrayLike, function(value) {
      arr.push(value);
    });
  }

  return arr;
}

module.exports = toArray;


/***/ }),
/* 32 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Remove css class from element
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var forEachArray = __webpack_require__(1);
var inArray = __webpack_require__(0);
var getClass = __webpack_require__(18);
var setClassName = __webpack_require__(19);

/**
 * Remove css class from element
 * @param {(HTMLElement|SVGElement)} element - target element
 * @param {...string} cssClass - css classes to remove
 * @memberof module:domUtil
 */
function removeClass(element) {
  var cssClass = Array.prototype.slice.call(arguments, 1);
  var classList = element.classList;
  var origin, newClass;

  if (classList) {
    forEachArray(cssClass, function(name) {
      classList.remove(name);
    });

    return;
  }

  origin = getClass(element).split(/\s+/);
  newClass = [];
  forEachArray(origin, function(name) {
    if (inArray(name, cssClass) < 0) {
      newClass.push(name);
    }
  });

  setClassName(element, newClass);
}

module.exports = removeClass;


/***/ }),
/* 33 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Check whether the given variable is a number or not.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



/**
 * Check whether the given variable is a number or not.
 * If the given variable is a number, return true.
 * @param {*} obj - Target for checking
 * @returns {boolean} Is number?
 * @memberof module:type
 */
function isNumber(obj) {
  return typeof obj === 'number' || obj instanceof Number;
}

module.exports = isNumber;


/***/ }),
/* 34 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var inArray = __webpack_require__(0);
var forEachArray = __webpack_require__(1);
var CustomEvents = __webpack_require__(8);
var defineClass = __webpack_require__(9);
var extend = __webpack_require__(2);
var on = __webpack_require__(10);
var off = __webpack_require__(11);
var closest = __webpack_require__(12);
var removeElement = __webpack_require__(13);
var isHTMLNode = __webpack_require__(14);

var util = __webpack_require__(15);
var tmpl = __webpack_require__(37);

var SELECTOR_UP_BUTTON = '.tui-timepicker-btn-up';
var SELECTOR_DOWN_BUTTON = '.tui-timepicker-btn-down';

/**
 * @class
 * @ignore
 * @param {String|HTMLElement} container - Container of spinbox or selector
 * @param {Object} [options] - Options for initialization
 * @param {number} [options.initialValue] - initial setting value
 * @param {Array.<number>} items - Items
 */
var Spinbox = defineClass(
  /** @lends Spinbox.prototype */ {
    init: function(container, options) {
      options = extend(
        {
          items: []
        },
        options
      );

      /**
       * @type {HTMLElement}
       * @private
       */
      this._container = isHTMLNode(container) ? container : document.querySelector(container);

      /**
       * Spinbox element
       * @type {HTMLElement}
       * @private
       */
      this._element = null;

      /**
       * @type {HTMLElement}
       * @private
       */
      this._inputElement = null;

      /**
       * Spinbox value items
       * @type {Array.<number>}
       * @private
       */
      this._items = options.items;

      /**
       * Selectbox disabled items info
       * @type {Array.<number>}
       * @private
       */
      this._disabledItems = options.disabledItems || [];

      /**
       * @type {number}
       * @private
       */
      this._selectedIndex = Math.max(0, inArray(options.initialValue, this._items));

      /**
       * Time format for output
       * @type {string}
       * @private
       */
      this._format = options.format;

      this._render();
      this._setEvents();
    },

    /**
     * Render spinbox
     * @private
     */
    _render: function() {
      var index = inArray(this.getValue(), this._items);
      var context;

      if (this._disabledItems[index]) {
        this._selectedIndex = this._findEnabledIndex();
      }
      context = {
        maxLength: this._getMaxLength(),
        initialValue: this.getValue(),
        format: this._format,
        formatTime: util.formatTime
      };

      this._container.innerHTML = tmpl(context);
      this._element = this._container.firstChild;
      this._inputElement = this._element.querySelector('input');
    },

    /**
     * Find the index of the enabled item
     * @returns {number} - find selected index
     * @private
     */
    _findEnabledIndex: function() {
      return inArray(false, this._disabledItems);
    },

    /**
     * Returns maxlength of value
     * @returns {number}
     * @private
     */
    _getMaxLength: function() {
      var lengths = [];

      forEachArray(this._items, function(item) {
        lengths.push(String(item).length);
      });

      return Math.max.apply(null, lengths);
    },

    /**
     * Set disabledItems
     * @param {object} disabledItems - disabled status of items
     */
    setDisabledItems: function(disabledItems) {
      this._disabledItems = disabledItems;
      this._changeToInputValue();
    },

    /**
     * Assign default events to up/down button
     * @private
     */
    _setEvents: function() {
      on(this._container, 'click', this._onClickHandler, this);
      on(this._inputElement, 'keydown', this._onKeydownInputElement, this);
      on(this._inputElement, 'change', this._onChangeHandler, this);

      this.on(
        'changeItems',
        function(items) {
          this._items = items;
          this._render();
        },
        this
      );
    },

    /**
     * Remove events to up/down button
     * @private
     */
    _removeEvents: function() {
      this.off();

      off(this._container, 'click', this._onClickHandler, this);
      off(this._inputElement, 'keydown', this._onKeydownInputElement, this);
      off(this._inputElement, 'change', this._onChangeHandler, this);
    },

    /**
     * Click event handler
     * @param {Event} ev - Change event on up/down buttons.
     */
    _onClickHandler: function(ev) {
      var target = util.getTarget(ev);

      if (closest(target, SELECTOR_DOWN_BUTTON)) {
        this._setNextValue(true);
      } else if (closest(target, SELECTOR_UP_BUTTON)) {
        this._setNextValue(false);
      }
    },

    /**
     * Set input value
     * @param {boolean} isDown - From down-action?
     * @private
     */
    _setNextValue: function(isDown) {
      var index = this._selectedIndex;

      if (isDown) {
        index = index ? index - 1 : this._items.length - 1;
      } else {
        index = index < this._items.length - 1 ? index + 1 : 0;
      }

      if (this._disabledItems[index]) {
        this._selectedIndex = index;
        this._setNextValue(isDown);
      } else {
        this.setValue(this._items[index]);
      }
    },

    /**
     * DOM(Input element) Keydown Event handler
     * @param {Event} ev event-object
     * @private
     */
    _onKeydownInputElement: function(ev) {
      var keyCode = ev.which || ev.keyCode;
      var isDown;

      if (closest(util.getTarget(ev), 'input')) {
        switch (keyCode) {
          case 38:
            isDown = false;
            break;
          case 40:
            isDown = true;
            break;
          default:
            return;
        }

        this._setNextValue(isDown);
      }
    },

    /**
     * DOM(Input element) Change Event handler
     * @param {Event} ev Change event on an input element.
     * @private
     */
    _onChangeHandler: function(ev) {
      if (closest(util.getTarget(ev), 'input')) {
        this._changeToInputValue();
      }
    },

    /**
     * Change value to input-box if it is valid.
     * @private
     * @param {boolean} silent prevents firing 'change' event if it is true.
     */
    _changeToInputValue: function(silent) {
      var newValue = Number(this._inputElement.value);
      var newIndex = inArray(newValue, this._items);

      if (this._disabledItems[newIndex]) {
        newIndex = this._findEnabledIndex();
        newValue = this._items[newIndex];
      } else if (newIndex === this._selectedIndex) {
        return;
      }

      if (newIndex === -1) {
        this.setValue(this._items[this._selectedIndex], silent);
      } else {
        this._selectedIndex = newIndex;

        if (!silent) {
          this.fire('change', {
            value: newValue
          });
        }
      }
    },

    /**
     * Set value to input-box.
     * @param {number} value - Value
     * @param {boolean} silent - prevents firing 'change' event if it is true.
     */
    setValue: function(value, silent) {
      this._inputElement.value = util.formatTime(value, this._format);
      this._changeToInputValue(silent);
    },

    /**
     * Returns current value
     * @returns {number}
     */
    getValue: function() {
      return this._items[this._selectedIndex];
    },

    /**
     * Destory
     */
    destroy: function() {
      this._removeEvents();
      removeElement(this._element);
      this._container = this._element = this._inputElement = this._items = this._selectedIndex = null;
    }
  }
);

CustomEvents.mixin(Spinbox);
module.exports = Spinbox;


/***/ }),
/* 35 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Send hostname on DOMContentLoaded.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var isUndefined = __webpack_require__(5);
var imagePing = __webpack_require__(36);

var ms7days = 7 * 24 * 60 * 60 * 1000;

/**
 * Check if the date has passed 7 days
 * @param {number} date - milliseconds
 * @returns {boolean}
 * @private
 */
function isExpired(date) {
  var now = new Date().getTime();

  return now - date > ms7days;
}

/**
 * Send hostname on DOMContentLoaded.
 * To prevent hostname set tui.usageStatistics to false.
 * @param {string} appName - application name
 * @param {string} trackingId - GA tracking ID
 * @ignore
 */
function sendHostname(appName, trackingId) {
  var url = 'https://www.google-analytics.com/collect';
  var hostname = location.hostname;
  var hitType = 'event';
  var eventCategory = 'use';
  var applicationKeyForStorage = 'TOAST UI ' + appName + ' for ' + hostname + ': Statistics';
  var date = window.localStorage.getItem(applicationKeyForStorage);

  // skip if the flag is defined and is set to false explicitly
  if (!isUndefined(window.tui) && window.tui.usageStatistics === false) {
    return;
  }

  // skip if not pass seven days old
  if (date && !isExpired(date)) {
    return;
  }

  window.localStorage.setItem(applicationKeyForStorage, new Date().getTime());

  setTimeout(function() {
    if (document.readyState === 'interactive' || document.readyState === 'complete') {
      imagePing(url, {
        v: 1,
        t: hitType,
        tid: trackingId,
        cid: hostname,
        dp: hostname,
        dh: appName,
        el: appName,
        ec: eventCategory
      });
    }
  }, 1000);
}

module.exports = sendHostname;


/***/ }),
/* 36 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/**
 * @fileoverview Request image ping.
 * @author NHN FE Development Lab <dl_javascript@nhn.com>
 */



var forEachOwnProperties = __webpack_require__(16);

/**
 * @module request
 */

/**
 * Request image ping.
 * @param {String} url url for ping request
 * @param {Object} trackingInfo infos for make query string
 * @returns {HTMLElement}
 * @memberof module:request
 * @example
 * var imagePing = require('tui-code-snippet/request/imagePing'); // node, commonjs
 *
 * imagePing('https://www.google-analytics.com/collect', {
 *     v: 1,
 *     t: 'event',
 *     tid: 'trackingid',
 *     cid: 'cid',
 *     dp: 'dp',
 *     dh: 'dh'
 * });
 */
function imagePing(url, trackingInfo) {
  var trackingElement = document.createElement('img');
  var queryString = '';
  forEachOwnProperties(trackingInfo, function(value, key) {
    queryString += '&' + key + '=' + value;
  });
  queryString = queryString.substring(1);

  trackingElement.src = url + '?' + queryString;

  trackingElement.style.display = 'none';
  document.body.appendChild(trackingElement);
  document.body.removeChild(trackingElement);

  return trackingElement;
}

module.exports = imagePing;


/***/ }),
/* 37 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var template = __webpack_require__(7);

module.exports = function(context) {
  var source =
      '<div class="tui-timepicker-btn-area">'
    + '  <input type="text" class="tui-timepicker-spinbox-input"'
    + '        maxlength="{{maxLength}}"'
    + '        size="{{maxLength}}"'
    + '        value="{{formatTime initialValue format}}"'
    + '        aria-label="TimePicker spinbox value">'
    + '  <button type="button" class="tui-timepicker-btn tui-timepicker-btn-up">'
    + '    <span class="tui-ico-t-btn">Increase</span>'
    + '  </button>'
    + '  <button type="button" class="tui-timepicker-btn tui-timepicker-btn-down">'
    + '    <span class="tui-ico-t-btn">Decrease</span>'
    + '  </button>'
    + '</div>';

  return template(source, context);
};



/***/ }),
/* 38 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var inArray = __webpack_require__(0);
var CustomEvents = __webpack_require__(8);
var defineClass = __webpack_require__(9);
var extend = __webpack_require__(2);
var on = __webpack_require__(10);
var off = __webpack_require__(11);
var closest = __webpack_require__(12);
var removeElement = __webpack_require__(13);
var isHTMLNode = __webpack_require__(14);

var util = __webpack_require__(15);
var tmpl = __webpack_require__(39);

/**
 * @class
 * @ignore
 * @param {string|HTMLElement} container - Container element or selector
 * @param {object} options - Options
 * @param {Array.<number>} options.items - Items
 * @param {number} options.initialValue - Initial value
 */
var Selectbox = defineClass(
  /** @lends Selectbox.prototype */ {
    init: function(container, options) {
      options = extend(
        {
          items: []
        },
        options
      );

      /**
       * Container element
       * @type {HTMLElement}
       * @private
       */
      this._container = isHTMLNode(container) ? container : document.querySelector(container);

      /**
       * Selectbox items
       * @type {Array.<number>}
       * @private
       */
      this._items = options.items || [];

      /**
       * Selectbox disabled items info
       * @type {Array.<number>}
       * @private
       */
      this._disabledItems = options.disabledItems || [];

      /**
       * Selected index
       * @type {number}
       * @private
       */
      this._selectedIndex = Math.max(0, inArray(options.initialValue, this._items));

      /**
       * Time format for output
       * @type {string}
       * @private
       */
      this._format = options.format;

      /**
       * Select element
       * @type {HTMLElement}
       * @private
       */
      this._element = null;

      this._render();
      this._setEvents();
    },

    /**
     * Render selectbox
     * @private
     */
    _render: function() {
      var context;

      this._changeEnabledIndex();
      context = {
        items: this._items,
        format: this._format,
        initialValue: this.getValue(),
        disabledItems: this._disabledItems,
        formatTime: util.formatTime,
        equals: function(a, b) {
          return a === b;
        }
      };

      if (this._element) {
        this._removeElement();
      }

      this._container.innerHTML = tmpl(context);
      this._element = this._container.firstChild;
      on(this._element, 'change', this._onChangeHandler, this);
    },

    /**
     * Change the index of the enabled item
     * @private
     */
    _changeEnabledIndex: function() {
      var index = inArray(this.getValue(), this._items);
      if (this._disabledItems[index]) {
        this._selectedIndex = inArray(false, this._disabledItems);
      }
    },

    /**
     * Set disabledItems
     * @param {object} disabledItems - disabled status of items
     * @private
     */
    setDisabledItems: function(disabledItems) {
      this._disabledItems = disabledItems;
      this._render();
    },

    /**
     * Set events
     * @private
     */
    _setEvents: function() {
      this.on(
        'changeItems',
        function(items) {
          this._items = items;
          this._render();
        },
        this
      );
    },

    /**
     * Remove events
     * @private
     */
    _removeEvents: function() {
      this.off();
    },

    /**
     * Remove element
     * @private
     */
    _removeElement: function() {
      off(this._element, 'change', this._onChangeHandler, this);
      removeElement(this._element);
    },

    /**
     * Change event handler
     * @param {Event} ev Change event on a select element.
     * @private
     */
    _onChangeHandler: function(ev) {
      if (closest(util.getTarget(ev), 'select')) {
        this._setNewValue();
      }
    },

    /**
     * Set new value
     * @private
     * @param {boolean} silent prevents firing 'change' event if it is true.
     */
    _setNewValue: function(silent) {
      var newValue = Number(this._element.value);
      this._selectedIndex = inArray(newValue, this._items);
      if (!silent) {
        this.fire('change', {
          value: newValue
        });
      }
    },

    /**
     * Returns current value
     * @returns {number}
     */
    getValue: function() {
      return this._items[this._selectedIndex];
    },

    /**
     * Set value
     * @param {number} value - New value
     * @param {boolean} silent - prevents firing 'change' event if it is true.
     */
    setValue: function(value, silent) {
      var newIndex = inArray(value, this._items);

      if (newIndex > -1 && newIndex !== this._selectedIndex) {
        this._selectedIndex = newIndex;
        this._element.value = value;
        this._setNewValue(silent);
      }
    },

    /**
     * Destory
     */
    destroy: function() {
      this._removeEvents();
      this._removeElement();
      this._container = this._items = this._selectedIndex = this._element = null;
    }
  }
);

CustomEvents.mixin(Selectbox);
module.exports = Selectbox;


/***/ }),
/* 39 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var template = __webpack_require__(7);

module.exports = function(context) {
  var source =
      '<select class="tui-timepicker-select" aria-label="Time">'
    + '  {{each items}}'
    + '    {{if equals initialValue @this}}'
    + '      <option value="{{@this}}" selected {{if disabledItems[@index]}}disabled{{/if}}>{{formatTime @this format}}</option>'
    + '    {{else}}'
    + '      <option value="{{@this}}" {{if disabledItems[@index]}}disabled{{/if}}>{{formatTime @this format}}</option>'
    + '    {{/if}}'
    + '  {{/each}}'
    + '</select>';

  return template(source, context);
};



/***/ }),
/* 40 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


module.exports = {
  en: {
    am: 'AM',
    pm: 'PM'
  },
  ko: {
    am: '오전',
    pm: '오후'
  }
};


/***/ }),
/* 41 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var template = __webpack_require__(7);

module.exports = function(context) {
  var source =
      '<div class="tui-timepicker">'
    + '  <div class="tui-timepicker-body">'
    + '    <div class="tui-timepicker-row">'
    + '      {{if isSpinbox}}'
    + '        <div class="tui-timepicker-column tui-timepicker-spinbox tui-timepicker-hour"></div>'
    + '        <span class="tui-timepicker-column tui-timepicker-colon"><span class="tui-ico-colon">:</span></span>'
    + '        <div class="tui-timepicker-column tui-timepicker-spinbox tui-timepicker-minute"></div>'
    + '        {{if showMeridiem}}'
    + '          {{meridiemElement}}'
    + '        {{/if}}'
    + '      {{else}}'
    + '        <div class="tui-timepicker-column tui-timepicker-selectbox tui-timepicker-hour"></div>'
    + '        <span class="tui-timepicker-column tui-timepicker-colon"><span class="tui-ico-colon">:</span></span>'
    + '        <div class="tui-timepicker-column tui-timepicker-selectbox tui-timepicker-minute"></div>'
    + '        {{if showMeridiem}}'
    + '          {{meridiemElement}}'
    + '        {{/if}}'
    + '      {{/if}}'
    + '    </div>'
    + '  </div>'
    + '</div>';

  return template(source, context);
};



/***/ }),
/* 42 */
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var template = __webpack_require__(7);

module.exports = function(context) {
  var source =
      '{{if isSpinbox}}'
    + '  <div class="tui-timepicker-column tui-timepicker-checkbox tui-timepicker-meridiem">'
    + '    <div class="tui-timepicker-check-area">'
    + '      <ul class="tui-timepicker-check-lst">'
    + '        <li class="tui-timepicker-check">'
    + '          <div class="tui-timepicker-radio">'
    + '            <input type="radio"'
    + '                  name="optionsRadios-{{radioId}}"'
    + '                  value="AM"'
    + '                  class="tui-timepicker-radio-am"'
    + '                  id="tui-timepicker-radio-am-{{radioId}}">'
    + '            <label for="tui-timepicker-radio-am-{{radioId}}" class="tui-timepicker-radio-label">'
    + '              <span class="tui-timepicker-input-radio"></span>{{am}}'
    + '            </label>'
    + '          </div>'
    + '        </li>'
    + '        <li class="tui-timepicker-check">'
    + '          <div class="tui-timepicker-radio">'
    + '            <input type="radio"'
    + '                  name="optionsRadios-{{radioId}}"'
    + '                  value="PM"'
    + '                  class="tui-timepicker-radio-pm"'
    + '                  id="tui-timepicker-radio-pm-{{radioId}}">'
    + '            <label for="tui-timepicker-radio-pm-{{radioId}}" class="tui-timepicker-radio-label">'
    + '              <span class="tui-timepicker-input-radio"></span>{{pm}}'
    + '            </label>'
    + '          </div>'
    + '        </li>'
    + '      </ul>'
    + '    </div>'
    + '  </div>'
    + '{{else}}'
    + '  <div class="tui-timepicker-column tui-timepicker-selectbox tui-is-add-picker tui-timepicker-meridiem">'
    + '    <select class="tui-timepicker-select" aria-label="AM/PM">'
    + '      <option value="AM">{{am}}</option>'
    + '      <option value="PM">{{pm}}</option>'
    + '    </select>'
    + '  </div>'
    + '{{/if}}';

  return template(source, context);
};



/***/ })
/******/ ]);
});