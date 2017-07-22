(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define([], factory);
	else {
		var a = factory();
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(this, function() {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};

/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {

/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;

/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};

/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);

/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;

/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}


/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;

/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;

/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "/assets/build";

/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ({

/***/ 0:
/***/ function(module, exports, __webpack_require__) {

	module.exports = __webpack_require__(1123);


/***/ },

/***/ 6:
/***/ function(module, exports, __webpack_require__) {

	var Symbol = __webpack_require__(7),
	    getRawTag = __webpack_require__(10),
	    objectToString = __webpack_require__(11);

	/** `Object#toString` result references. */
	var nullTag = '[object Null]',
	    undefinedTag = '[object Undefined]';

	/** Built-in value references. */
	var symToStringTag = Symbol ? Symbol.toStringTag : undefined;

	/**
	 * The base implementation of `getTag` without fallbacks for buggy environments.
	 *
	 * @private
	 * @param {*} value The value to query.
	 * @returns {string} Returns the `toStringTag`.
	 */
	function baseGetTag(value) {
	  if (value == null) {
	    return value === undefined ? undefinedTag : nullTag;
	  }
	  return (symToStringTag && symToStringTag in Object(value))
	    ? getRawTag(value)
	    : objectToString(value);
	}

	module.exports = baseGetTag;


/***/ },

/***/ 7:
/***/ function(module, exports, __webpack_require__) {

	var root = __webpack_require__(8);

	/** Built-in value references. */
	var Symbol = root.Symbol;

	module.exports = Symbol;


/***/ },

/***/ 8:
/***/ function(module, exports, __webpack_require__) {

	var freeGlobal = __webpack_require__(9);

	/** Detect free variable `self`. */
	var freeSelf = typeof self == 'object' && self && self.Object === Object && self;

	/** Used as a reference to the global object. */
	var root = freeGlobal || freeSelf || Function('return this')();

	module.exports = root;


/***/ },

/***/ 9:
/***/ function(module, exports) {

	/* WEBPACK VAR INJECTION */(function(global) {/** Detect free variable `global` from Node.js. */
	var freeGlobal = typeof global == 'object' && global && global.Object === Object && global;

	module.exports = freeGlobal;

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ },

/***/ 10:
/***/ function(module, exports, __webpack_require__) {

	var Symbol = __webpack_require__(7);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Used to resolve the
	 * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
	 * of values.
	 */
	var nativeObjectToString = objectProto.toString;

	/** Built-in value references. */
	var symToStringTag = Symbol ? Symbol.toStringTag : undefined;

	/**
	 * A specialized version of `baseGetTag` which ignores `Symbol.toStringTag` values.
	 *
	 * @private
	 * @param {*} value The value to query.
	 * @returns {string} Returns the raw `toStringTag`.
	 */
	function getRawTag(value) {
	  var isOwn = hasOwnProperty.call(value, symToStringTag),
	      tag = value[symToStringTag];

	  try {
	    value[symToStringTag] = undefined;
	    var unmasked = true;
	  } catch (e) {}

	  var result = nativeObjectToString.call(value);
	  if (unmasked) {
	    if (isOwn) {
	      value[symToStringTag] = tag;
	    } else {
	      delete value[symToStringTag];
	    }
	  }
	  return result;
	}

	module.exports = getRawTag;


/***/ },

/***/ 11:
/***/ function(module, exports) {

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/**
	 * Used to resolve the
	 * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
	 * of values.
	 */
	var nativeObjectToString = objectProto.toString;

	/**
	 * Converts `value` to a string using `Object.prototype.toString`.
	 *
	 * @private
	 * @param {*} value The value to convert.
	 * @returns {string} Returns the converted string.
	 */
	function objectToString(value) {
	  return nativeObjectToString.call(value);
	}

	module.exports = objectToString;


/***/ },

/***/ 14:
/***/ function(module, exports) {

	/**
	 * Checks if `value` is object-like. A value is object-like if it's not `null`
	 * and has a `typeof` result of "object".
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is object-like, else `false`.
	 * @example
	 *
	 * _.isObjectLike({});
	 * // => true
	 *
	 * _.isObjectLike([1, 2, 3]);
	 * // => true
	 *
	 * _.isObjectLike(_.noop);
	 * // => false
	 *
	 * _.isObjectLike(null);
	 * // => false
	 */
	function isObjectLike(value) {
	  return value != null && typeof value == 'object';
	}

	module.exports = isObjectLike;


/***/ },

/***/ 70:
/***/ function(module, exports) {

	/**
	 * Checks if `value` is classified as an `Array` object.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an array, else `false`.
	 * @example
	 *
	 * _.isArray([1, 2, 3]);
	 * // => true
	 *
	 * _.isArray(document.body.children);
	 * // => false
	 *
	 * _.isArray('abc');
	 * // => false
	 *
	 * _.isArray(_.noop);
	 * // => false
	 */
	var isArray = Array.isArray;

	module.exports = isArray;


/***/ },

/***/ 72:
/***/ function(module, exports, __webpack_require__) {

	var baseGetTag = __webpack_require__(6),
	    isObjectLike = __webpack_require__(14);

	/** `Object#toString` result references. */
	var symbolTag = '[object Symbol]';

	/**
	 * Checks if `value` is classified as a `Symbol` primitive or object.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a symbol, else `false`.
	 * @example
	 *
	 * _.isSymbol(Symbol.iterator);
	 * // => true
	 *
	 * _.isSymbol('abc');
	 * // => false
	 */
	function isSymbol(value) {
	  return typeof value == 'symbol' ||
	    (isObjectLike(value) && baseGetTag(value) == symbolTag);
	}

	module.exports = isSymbol;


/***/ },

/***/ 108:
/***/ function(module, exports, __webpack_require__) {

	var baseToString = __webpack_require__(109);

	/**
	 * Converts `value` to a string. An empty string is returned for `null`
	 * and `undefined` values. The sign of `-0` is preserved.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to convert.
	 * @returns {string} Returns the converted string.
	 * @example
	 *
	 * _.toString(null);
	 * // => ''
	 *
	 * _.toString(-0);
	 * // => '-0'
	 *
	 * _.toString([1, 2, 3]);
	 * // => '1,2,3'
	 */
	function toString(value) {
	  return value == null ? '' : baseToString(value);
	}

	module.exports = toString;


/***/ },

/***/ 109:
/***/ function(module, exports, __webpack_require__) {

	var Symbol = __webpack_require__(7),
	    arrayMap = __webpack_require__(110),
	    isArray = __webpack_require__(70),
	    isSymbol = __webpack_require__(72);

	/** Used as references for various `Number` constants. */
	var INFINITY = 1 / 0;

	/** Used to convert symbols to primitives and strings. */
	var symbolProto = Symbol ? Symbol.prototype : undefined,
	    symbolToString = symbolProto ? symbolProto.toString : undefined;

	/**
	 * The base implementation of `_.toString` which doesn't convert nullish
	 * values to empty strings.
	 *
	 * @private
	 * @param {*} value The value to process.
	 * @returns {string} Returns the string.
	 */
	function baseToString(value) {
	  // Exit early for strings to avoid a performance hit in some environments.
	  if (typeof value == 'string') {
	    return value;
	  }
	  if (isArray(value)) {
	    // Recursively convert values (susceptible to call stack limits).
	    return arrayMap(value, baseToString) + '';
	  }
	  if (isSymbol(value)) {
	    return symbolToString ? symbolToString.call(value) : '';
	  }
	  var result = (value + '');
	  return (result == '0' && (1 / value) == -INFINITY) ? '-0' : result;
	}

	module.exports = baseToString;


/***/ },

/***/ 110:
/***/ function(module, exports) {

	/**
	 * A specialized version of `_.map` for arrays without support for iteratee
	 * shorthands.
	 *
	 * @private
	 * @param {Array} [array] The array to iterate over.
	 * @param {Function} iteratee The function invoked per iteration.
	 * @returns {Array} Returns the new mapped array.
	 */
	function arrayMap(array, iteratee) {
	  var index = -1,
	      length = array == null ? 0 : array.length,
	      result = Array(length);

	  while (++index < length) {
	    result[index] = iteratee(array[index], index, array);
	  }
	  return result;
	}

	module.exports = arrayMap;


/***/ },

/***/ 259:
/***/ function(module, exports, __webpack_require__) {

	var toString = __webpack_require__(108);

	/**
	 * Used to match `RegExp`
	 * [syntax characters](http://ecma-international.org/ecma-262/7.0/#sec-patterns).
	 */
	var reRegExpChar = /[\\^$.*+?()[\]{}|]/g,
	    reHasRegExpChar = RegExp(reRegExpChar.source);

	/**
	 * Escapes the `RegExp` special characters "^", "$", "\", ".", "*", "+",
	 * "?", "(", ")", "[", "]", "{", "}", and "|" in `string`.
	 *
	 * @static
	 * @memberOf _
	 * @since 3.0.0
	 * @category String
	 * @param {string} [string=''] The string to escape.
	 * @returns {string} Returns the escaped string.
	 * @example
	 *
	 * _.escapeRegExp('[lodash](https://lodash.com/)');
	 * // => '\[lodash\]\(https://lodash\.com/\)'
	 */
	function escapeRegExp(string) {
	  string = toString(string);
	  return (string && reHasRegExpChar.test(string))
	    ? string.replace(reRegExpChar, '\\$&')
	    : string;
	}

	module.exports = escapeRegExp;


/***/ },

/***/ 900:
/***/ function(module, exports, __webpack_require__) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	const networkRequest = __webpack_require__(901);
	const workerUtils = __webpack_require__(902);

	module.exports = {
	  networkRequest,
	  workerUtils
	};

/***/ },

/***/ 901:
/***/ function(module, exports) {

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	function networkRequest(url, opts) {
	  return new Promise((resolve, reject) => {
	    const req = new XMLHttpRequest();

	    req.addEventListener("readystatechange", () => {
	      if (req.readyState === XMLHttpRequest.DONE) {
	        if (req.status === 200) {
	          resolve({ content: req.responseText });
	        } else {
	          reject(req.statusText);
	        }
	      }
	    });

	    // Not working yet.
	    // if (!opts.loadFromCache) {
	    //   req.channel.loadFlags = (
	    //     Components.interfaces.nsIRequest.LOAD_BYPASS_CACHE |
	    //       Components.interfaces.nsIRequest.INHIBIT_CACHING |
	    //       Components.interfaces.nsIRequest.LOAD_ANONYMOUS
	    //   );
	    // }

	    req.open("GET", url);
	    req.send();
	  });
	}

	module.exports = networkRequest;

/***/ },

/***/ 902:
/***/ function(module, exports) {

	

	function WorkerDispatcher() {
	  this.msgId = 1;
	  this.worker = null;
	} /* This Source Code Form is subject to the terms of the Mozilla Public
	   * License, v. 2.0. If a copy of the MPL was not distributed with this
	   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	WorkerDispatcher.prototype = {
	  start(url) {
	    this.worker = new Worker(url);
	    this.worker.onerror = () => {
	      console.error(`Error in worker ${url}`);
	    };
	  },

	  stop() {
	    if (!this.worker) {
	      return;
	    }

	    this.worker.terminate();
	    this.worker = null;
	  },

	  task(method) {
	    return (...args) => {
	      return new Promise((resolve, reject) => {
	        const id = this.msgId++;
	        this.worker.postMessage({ id, method, args });

	        const listener = ({ data: result }) => {
	          if (result.id !== id) {
	            return;
	          }

	          this.worker.removeEventListener("message", listener);
	          if (result.error) {
	            reject(result.error);
	          } else {
	            resolve(result.response);
	          }
	        };

	        this.worker.addEventListener("message", listener);
	      });
	    };
	  }
	};

	function workerHandler(publicInterface) {
	  return function workerHandler(msg) {
	    const { id, method, args } = msg.data;
	    const response = publicInterface[method].apply(undefined, args);
	    if (response instanceof Promise) {
	      response.then(val => self.postMessage({ id, response: val }), err => self.postMessage({ id, error: err }));
	    } else {
	      self.postMessage({ id, response });
	    }
	  };
	}

	module.exports = {
	  WorkerDispatcher,
	  workerHandler
	};

/***/ },

/***/ 1123:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var _getMatches = __webpack_require__(1173);

	var _getMatches2 = _interopRequireDefault(_getMatches);

	var _projectSearch = __webpack_require__(1140);

	var _projectSearch2 = _interopRequireDefault(_projectSearch);

	var _devtoolsUtils = __webpack_require__(900);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

	var workerHandler = _devtoolsUtils.workerUtils.workerHandler;


	self.onmessage = workerHandler({ getMatches: _getMatches2.default, searchSources: _projectSearch2.default });

/***/ },

/***/ 1138:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	Object.defineProperty(exports, "__esModule", {
	  value: true
	});
	exports.default = buildQuery;

	var _escapeRegExp = __webpack_require__(259);

	var _escapeRegExp2 = _interopRequireDefault(_escapeRegExp);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

	/**
	 * Ignore doing outline matches for less than 3 whitespaces
	 *
	 * @memberof utils/source-search
	 * @static
	 */
	function ignoreWhiteSpace(str) {
	  return (/^\s{0,2}$/.test(str) ? "(?!\\s*.*)" : str
	  );
	}


	function wholeMatch(query, wholeWord) {
	  if (query == "" || !wholeWord) {
	    return query;
	  }

	  return `\\b${query}\\b`;
	}

	function buildFlags(caseSensitive, isGlobal) {
	  if (caseSensitive && isGlobal) {
	    return "g";
	  }

	  if (!caseSensitive && isGlobal) {
	    return "gi";
	  }

	  if (!caseSensitive && !isGlobal) {
	    return "i";
	  }

	  return;
	}

	function buildQuery(originalQuery, modifiers, _ref) {
	  var _ref$isGlobal = _ref.isGlobal,
	      isGlobal = _ref$isGlobal === undefined ? false : _ref$isGlobal,
	      _ref$ignoreSpaces = _ref.ignoreSpaces,
	      ignoreSpaces = _ref$ignoreSpaces === undefined ? false : _ref$ignoreSpaces;
	  var caseSensitive = modifiers.caseSensitive,
	      regexMatch = modifiers.regexMatch,
	      wholeWord = modifiers.wholeWord;


	  if (originalQuery == "") {
	    return new RegExp(originalQuery);
	  }

	  var query = originalQuery;
	  if (ignoreSpaces) {
	    query = ignoreWhiteSpace(query);
	  }

	  if (!regexMatch) {
	    query = (0, _escapeRegExp2.default)(query);
	  }

	  query = wholeMatch(query, wholeWord);
	  var flags = buildFlags(caseSensitive, isGlobal);

	  if (flags) {
	    return new RegExp(query, flags);
	  }

	  return new RegExp(query);
	}

/***/ },

/***/ 1140:
/***/ function(module, exports) {

	"use strict";

	Object.defineProperty(exports, "__esModule", {
	  value: true
	});
	exports.searchSource = searchSource;
	exports.default = searchSources;

	function _toConsumableArray(arr) { if (Array.isArray(arr)) { for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) { arr2[i] = arr[i]; } return arr2; } else { return Array.from(arr); } }

	// Maybe reuse file search's functions?
	function searchSource(source, queryText) {
	  var _ref;

	  var text = source.text,
	      loading = source.loading;

	  if (loading || !text || queryText == "") {
	    return [];
	  }

	  var lines = text.split("\n");
	  var result = undefined;
	  var query = new RegExp(queryText, "g");

	  var matches = lines.map((_text, line) => {
	    var indices = [];

	    while (result = query.exec(_text)) {
	      indices.push({
	        line: line + 1,
	        column: result.index,
	        match: result[0],
	        value: _text,
	        text: result.input
	      });
	    }
	    return indices;
	  }).filter(_matches => _matches.length > 0);

	  matches = (_ref = []).concat.apply(_ref, _toConsumableArray(matches));
	  return matches;
	}

	function searchSources(query, sources) {
	  var validSources = sources.valueSeq().filter(s => s.has("text")).toJS();
	  return validSources.map(source => ({
	    source,
	    filepath: source.url,
	    matches: searchSource(source, query)
	  }));
	}

/***/ },

/***/ 1173:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	Object.defineProperty(exports, "__esModule", {
	  value: true
	});
	exports.default = getMatches;

	var _buildQuery = __webpack_require__(1138);

	var _buildQuery2 = _interopRequireDefault(_buildQuery);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

	function getMatches(query, text, modifiers) {
	  var regexQuery = (0, _buildQuery2.default)(query, modifiers, {
	    isGlobal: true
	  });
	  var matchedLocations = [];
	  var lines = text.split("\n");
	  for (var i = 0; i < lines.length; i++) {
	    var singleMatch = void 0;
	    while ((singleMatch = regexQuery.exec(lines[i])) !== null) {
	      matchedLocations.push({ line: i, ch: singleMatch.index });
	    }
	  }
	  return matchedLocations;
	}

/***/ }

/******/ })
});
;