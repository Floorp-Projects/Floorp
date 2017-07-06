(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory(require("Services"), require("devtools/shared/flags"));
	else if(typeof define === 'function' && define.amd)
		define(["Services", "devtools/shared/flags"], factory);
	else {
		var a = typeof exports === 'object' ? factory(require("Services"), require("devtools/shared/flags")) : factory(root["Services"], root["devtools/shared/flags"]);
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(this, function(__WEBPACK_EXTERNAL_MODULE_29__, __WEBPACK_EXTERNAL_MODULE_121__) {
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

	module.exports = __webpack_require__(757);


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

/***/ 13:
/***/ function(module, exports) {

	/**
	 * Creates a unary function that invokes `func` with its argument transformed.
	 *
	 * @private
	 * @param {Function} func The function to wrap.
	 * @param {Function} transform The argument transform.
	 * @returns {Function} Returns the new function.
	 */
	function overArg(func, transform) {
	  return function(arg) {
	    return func(transform(arg));
	  };
	}

	module.exports = overArg;


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

/***/ 29:
/***/ function(module, exports) {

	module.exports = __WEBPACK_EXTERNAL_MODULE_29__;

/***/ },

/***/ 51:
/***/ function(module, exports) {

	module.exports = function(module) {
		if(!module.webpackPolyfill) {
			module.deprecate = function() {};
			module.paths = [];
			// module.parent = undefined by default
			module.children = [];
			module.webpackPolyfill = 1;
		}
		return module;
	}


/***/ },

/***/ 67:
/***/ function(module, exports, __webpack_require__) {

	var baseGet = __webpack_require__(68);

	/**
	 * Gets the value at `path` of `object`. If the resolved value is
	 * `undefined`, the `defaultValue` is returned in its place.
	 *
	 * @static
	 * @memberOf _
	 * @since 3.7.0
	 * @category Object
	 * @param {Object} object The object to query.
	 * @param {Array|string} path The path of the property to get.
	 * @param {*} [defaultValue] The value returned for `undefined` resolved values.
	 * @returns {*} Returns the resolved value.
	 * @example
	 *
	 * var object = { 'a': [{ 'b': { 'c': 3 } }] };
	 *
	 * _.get(object, 'a[0].b.c');
	 * // => 3
	 *
	 * _.get(object, ['a', '0', 'b', 'c']);
	 * // => 3
	 *
	 * _.get(object, 'a.b.c', 'default');
	 * // => 'default'
	 */
	function get(object, path, defaultValue) {
	  var result = object == null ? undefined : baseGet(object, path);
	  return result === undefined ? defaultValue : result;
	}

	module.exports = get;


/***/ },

/***/ 68:
/***/ function(module, exports, __webpack_require__) {

	var castPath = __webpack_require__(69),
	    toKey = __webpack_require__(111);

	/**
	 * The base implementation of `_.get` without support for default values.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {Array|string} path The path of the property to get.
	 * @returns {*} Returns the resolved value.
	 */
	function baseGet(object, path) {
	  path = castPath(path, object);

	  var index = 0,
	      length = path.length;

	  while (object != null && index < length) {
	    object = object[toKey(path[index++])];
	  }
	  return (index && index == length) ? object : undefined;
	}

	module.exports = baseGet;


/***/ },

/***/ 69:
/***/ function(module, exports, __webpack_require__) {

	var isArray = __webpack_require__(70),
	    isKey = __webpack_require__(71),
	    stringToPath = __webpack_require__(73),
	    toString = __webpack_require__(108);

	/**
	 * Casts `value` to a path array if it's not one.
	 *
	 * @private
	 * @param {*} value The value to inspect.
	 * @param {Object} [object] The object to query keys on.
	 * @returns {Array} Returns the cast property path array.
	 */
	function castPath(value, object) {
	  if (isArray(value)) {
	    return value;
	  }
	  return isKey(value, object) ? [value] : stringToPath(toString(value));
	}

	module.exports = castPath;


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

/***/ 71:
/***/ function(module, exports, __webpack_require__) {

	var isArray = __webpack_require__(70),
	    isSymbol = __webpack_require__(72);

	/** Used to match property names within property paths. */
	var reIsDeepProp = /\.|\[(?:[^[\]]*|(["'])(?:(?!\1)[^\\]|\\.)*?\1)\]/,
	    reIsPlainProp = /^\w*$/;

	/**
	 * Checks if `value` is a property name and not a property path.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @param {Object} [object] The object to query keys on.
	 * @returns {boolean} Returns `true` if `value` is a property name, else `false`.
	 */
	function isKey(value, object) {
	  if (isArray(value)) {
	    return false;
	  }
	  var type = typeof value;
	  if (type == 'number' || type == 'symbol' || type == 'boolean' ||
	      value == null || isSymbol(value)) {
	    return true;
	  }
	  return reIsPlainProp.test(value) || !reIsDeepProp.test(value) ||
	    (object != null && value in Object(object));
	}

	module.exports = isKey;


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

/***/ 73:
/***/ function(module, exports, __webpack_require__) {

	var memoizeCapped = __webpack_require__(74);

	/** Used to match property names within property paths. */
	var reLeadingDot = /^\./,
	    rePropName = /[^.[\]]+|\[(?:(-?\d+(?:\.\d+)?)|(["'])((?:(?!\2)[^\\]|\\.)*?)\2)\]|(?=(?:\.|\[\])(?:\.|\[\]|$))/g;

	/** Used to match backslashes in property paths. */
	var reEscapeChar = /\\(\\)?/g;

	/**
	 * Converts `string` to a property path array.
	 *
	 * @private
	 * @param {string} string The string to convert.
	 * @returns {Array} Returns the property path array.
	 */
	var stringToPath = memoizeCapped(function(string) {
	  var result = [];
	  if (reLeadingDot.test(string)) {
	    result.push('');
	  }
	  string.replace(rePropName, function(match, number, quote, string) {
	    result.push(quote ? string.replace(reEscapeChar, '$1') : (number || match));
	  });
	  return result;
	});

	module.exports = stringToPath;


/***/ },

/***/ 74:
/***/ function(module, exports, __webpack_require__) {

	var memoize = __webpack_require__(75);

	/** Used as the maximum memoize cache size. */
	var MAX_MEMOIZE_SIZE = 500;

	/**
	 * A specialized version of `_.memoize` which clears the memoized function's
	 * cache when it exceeds `MAX_MEMOIZE_SIZE`.
	 *
	 * @private
	 * @param {Function} func The function to have its output memoized.
	 * @returns {Function} Returns the new memoized function.
	 */
	function memoizeCapped(func) {
	  var result = memoize(func, function(key) {
	    if (cache.size === MAX_MEMOIZE_SIZE) {
	      cache.clear();
	    }
	    return key;
	  });

	  var cache = result.cache;
	  return result;
	}

	module.exports = memoizeCapped;


/***/ },

/***/ 75:
/***/ function(module, exports, __webpack_require__) {

	var MapCache = __webpack_require__(76);

	/** Error message constants. */
	var FUNC_ERROR_TEXT = 'Expected a function';

	/**
	 * Creates a function that memoizes the result of `func`. If `resolver` is
	 * provided, it determines the cache key for storing the result based on the
	 * arguments provided to the memoized function. By default, the first argument
	 * provided to the memoized function is used as the map cache key. The `func`
	 * is invoked with the `this` binding of the memoized function.
	 *
	 * **Note:** The cache is exposed as the `cache` property on the memoized
	 * function. Its creation may be customized by replacing the `_.memoize.Cache`
	 * constructor with one whose instances implement the
	 * [`Map`](http://ecma-international.org/ecma-262/7.0/#sec-properties-of-the-map-prototype-object)
	 * method interface of `clear`, `delete`, `get`, `has`, and `set`.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Function
	 * @param {Function} func The function to have its output memoized.
	 * @param {Function} [resolver] The function to resolve the cache key.
	 * @returns {Function} Returns the new memoized function.
	 * @example
	 *
	 * var object = { 'a': 1, 'b': 2 };
	 * var other = { 'c': 3, 'd': 4 };
	 *
	 * var values = _.memoize(_.values);
	 * values(object);
	 * // => [1, 2]
	 *
	 * values(other);
	 * // => [3, 4]
	 *
	 * object.a = 2;
	 * values(object);
	 * // => [1, 2]
	 *
	 * // Modify the result cache.
	 * values.cache.set(object, ['a', 'b']);
	 * values(object);
	 * // => ['a', 'b']
	 *
	 * // Replace `_.memoize.Cache`.
	 * _.memoize.Cache = WeakMap;
	 */
	function memoize(func, resolver) {
	  if (typeof func != 'function' || (resolver != null && typeof resolver != 'function')) {
	    throw new TypeError(FUNC_ERROR_TEXT);
	  }
	  var memoized = function() {
	    var args = arguments,
	        key = resolver ? resolver.apply(this, args) : args[0],
	        cache = memoized.cache;

	    if (cache.has(key)) {
	      return cache.get(key);
	    }
	    var result = func.apply(this, args);
	    memoized.cache = cache.set(key, result) || cache;
	    return result;
	  };
	  memoized.cache = new (memoize.Cache || MapCache);
	  return memoized;
	}

	// Expose `MapCache`.
	memoize.Cache = MapCache;

	module.exports = memoize;


/***/ },

/***/ 76:
/***/ function(module, exports, __webpack_require__) {

	var mapCacheClear = __webpack_require__(77),
	    mapCacheDelete = __webpack_require__(102),
	    mapCacheGet = __webpack_require__(105),
	    mapCacheHas = __webpack_require__(106),
	    mapCacheSet = __webpack_require__(107);

	/**
	 * Creates a map cache object to store key-value pairs.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [entries] The key-value pairs to cache.
	 */
	function MapCache(entries) {
	  var index = -1,
	      length = entries == null ? 0 : entries.length;

	  this.clear();
	  while (++index < length) {
	    var entry = entries[index];
	    this.set(entry[0], entry[1]);
	  }
	}

	// Add methods to `MapCache`.
	MapCache.prototype.clear = mapCacheClear;
	MapCache.prototype['delete'] = mapCacheDelete;
	MapCache.prototype.get = mapCacheGet;
	MapCache.prototype.has = mapCacheHas;
	MapCache.prototype.set = mapCacheSet;

	module.exports = MapCache;


/***/ },

/***/ 77:
/***/ function(module, exports, __webpack_require__) {

	var Hash = __webpack_require__(78),
	    ListCache = __webpack_require__(93),
	    Map = __webpack_require__(101);

	/**
	 * Removes all key-value entries from the map.
	 *
	 * @private
	 * @name clear
	 * @memberOf MapCache
	 */
	function mapCacheClear() {
	  this.size = 0;
	  this.__data__ = {
	    'hash': new Hash,
	    'map': new (Map || ListCache),
	    'string': new Hash
	  };
	}

	module.exports = mapCacheClear;


/***/ },

/***/ 78:
/***/ function(module, exports, __webpack_require__) {

	var hashClear = __webpack_require__(79),
	    hashDelete = __webpack_require__(89),
	    hashGet = __webpack_require__(90),
	    hashHas = __webpack_require__(91),
	    hashSet = __webpack_require__(92);

	/**
	 * Creates a hash object.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [entries] The key-value pairs to cache.
	 */
	function Hash(entries) {
	  var index = -1,
	      length = entries == null ? 0 : entries.length;

	  this.clear();
	  while (++index < length) {
	    var entry = entries[index];
	    this.set(entry[0], entry[1]);
	  }
	}

	// Add methods to `Hash`.
	Hash.prototype.clear = hashClear;
	Hash.prototype['delete'] = hashDelete;
	Hash.prototype.get = hashGet;
	Hash.prototype.has = hashHas;
	Hash.prototype.set = hashSet;

	module.exports = Hash;


/***/ },

/***/ 79:
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(80);

	/**
	 * Removes all key-value entries from the hash.
	 *
	 * @private
	 * @name clear
	 * @memberOf Hash
	 */
	function hashClear() {
	  this.__data__ = nativeCreate ? nativeCreate(null) : {};
	  this.size = 0;
	}

	module.exports = hashClear;


/***/ },

/***/ 80:
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(81);

	/* Built-in method references that are verified to be native. */
	var nativeCreate = getNative(Object, 'create');

	module.exports = nativeCreate;


/***/ },

/***/ 81:
/***/ function(module, exports, __webpack_require__) {

	var baseIsNative = __webpack_require__(82),
	    getValue = __webpack_require__(88);

	/**
	 * Gets the native function at `key` of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {string} key The key of the method to get.
	 * @returns {*} Returns the function if it's native, else `undefined`.
	 */
	function getNative(object, key) {
	  var value = getValue(object, key);
	  return baseIsNative(value) ? value : undefined;
	}

	module.exports = getNative;


/***/ },

/***/ 82:
/***/ function(module, exports, __webpack_require__) {

	var isFunction = __webpack_require__(83),
	    isMasked = __webpack_require__(85),
	    isObject = __webpack_require__(84),
	    toSource = __webpack_require__(87);

	/**
	 * Used to match `RegExp`
	 * [syntax characters](http://ecma-international.org/ecma-262/7.0/#sec-patterns).
	 */
	var reRegExpChar = /[\\^$.*+?()[\]{}|]/g;

	/** Used to detect host constructors (Safari). */
	var reIsHostCtor = /^\[object .+?Constructor\]$/;

	/** Used for built-in method references. */
	var funcProto = Function.prototype,
	    objectProto = Object.prototype;

	/** Used to resolve the decompiled source of functions. */
	var funcToString = funcProto.toString;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/** Used to detect if a method is native. */
	var reIsNative = RegExp('^' +
	  funcToString.call(hasOwnProperty).replace(reRegExpChar, '\\$&')
	  .replace(/hasOwnProperty|(function).*?(?=\\\()| for .+?(?=\\\])/g, '$1.*?') + '$'
	);

	/**
	 * The base implementation of `_.isNative` without bad shim checks.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a native function,
	 *  else `false`.
	 */
	function baseIsNative(value) {
	  if (!isObject(value) || isMasked(value)) {
	    return false;
	  }
	  var pattern = isFunction(value) ? reIsNative : reIsHostCtor;
	  return pattern.test(toSource(value));
	}

	module.exports = baseIsNative;


/***/ },

/***/ 83:
/***/ function(module, exports, __webpack_require__) {

	var baseGetTag = __webpack_require__(6),
	    isObject = __webpack_require__(84);

	/** `Object#toString` result references. */
	var asyncTag = '[object AsyncFunction]',
	    funcTag = '[object Function]',
	    genTag = '[object GeneratorFunction]',
	    proxyTag = '[object Proxy]';

	/**
	 * Checks if `value` is classified as a `Function` object.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a function, else `false`.
	 * @example
	 *
	 * _.isFunction(_);
	 * // => true
	 *
	 * _.isFunction(/abc/);
	 * // => false
	 */
	function isFunction(value) {
	  if (!isObject(value)) {
	    return false;
	  }
	  // The use of `Object#toString` avoids issues with the `typeof` operator
	  // in Safari 9 which returns 'object' for typed arrays and other constructors.
	  var tag = baseGetTag(value);
	  return tag == funcTag || tag == genTag || tag == asyncTag || tag == proxyTag;
	}

	module.exports = isFunction;


/***/ },

/***/ 84:
/***/ function(module, exports) {

	/**
	 * Checks if `value` is the
	 * [language type](http://www.ecma-international.org/ecma-262/7.0/#sec-ecmascript-language-types)
	 * of `Object`. (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(_.noop);
	 * // => true
	 *
	 * _.isObject(null);
	 * // => false
	 */
	function isObject(value) {
	  var type = typeof value;
	  return value != null && (type == 'object' || type == 'function');
	}

	module.exports = isObject;


/***/ },

/***/ 85:
/***/ function(module, exports, __webpack_require__) {

	var coreJsData = __webpack_require__(86);

	/** Used to detect methods masquerading as native. */
	var maskSrcKey = (function() {
	  var uid = /[^.]+$/.exec(coreJsData && coreJsData.keys && coreJsData.keys.IE_PROTO || '');
	  return uid ? ('Symbol(src)_1.' + uid) : '';
	}());

	/**
	 * Checks if `func` has its source masked.
	 *
	 * @private
	 * @param {Function} func The function to check.
	 * @returns {boolean} Returns `true` if `func` is masked, else `false`.
	 */
	function isMasked(func) {
	  return !!maskSrcKey && (maskSrcKey in func);
	}

	module.exports = isMasked;


/***/ },

/***/ 86:
/***/ function(module, exports, __webpack_require__) {

	var root = __webpack_require__(8);

	/** Used to detect overreaching core-js shims. */
	var coreJsData = root['__core-js_shared__'];

	module.exports = coreJsData;


/***/ },

/***/ 87:
/***/ function(module, exports) {

	/** Used for built-in method references. */
	var funcProto = Function.prototype;

	/** Used to resolve the decompiled source of functions. */
	var funcToString = funcProto.toString;

	/**
	 * Converts `func` to its source code.
	 *
	 * @private
	 * @param {Function} func The function to convert.
	 * @returns {string} Returns the source code.
	 */
	function toSource(func) {
	  if (func != null) {
	    try {
	      return funcToString.call(func);
	    } catch (e) {}
	    try {
	      return (func + '');
	    } catch (e) {}
	  }
	  return '';
	}

	module.exports = toSource;


/***/ },

/***/ 88:
/***/ function(module, exports) {

	/**
	 * Gets the value at `key` of `object`.
	 *
	 * @private
	 * @param {Object} [object] The object to query.
	 * @param {string} key The key of the property to get.
	 * @returns {*} Returns the property value.
	 */
	function getValue(object, key) {
	  return object == null ? undefined : object[key];
	}

	module.exports = getValue;


/***/ },

/***/ 89:
/***/ function(module, exports) {

	/**
	 * Removes `key` and its value from the hash.
	 *
	 * @private
	 * @name delete
	 * @memberOf Hash
	 * @param {Object} hash The hash to modify.
	 * @param {string} key The key of the value to remove.
	 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
	 */
	function hashDelete(key) {
	  var result = this.has(key) && delete this.__data__[key];
	  this.size -= result ? 1 : 0;
	  return result;
	}

	module.exports = hashDelete;


/***/ },

/***/ 90:
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(80);

	/** Used to stand-in for `undefined` hash values. */
	var HASH_UNDEFINED = '__lodash_hash_undefined__';

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Gets the hash value for `key`.
	 *
	 * @private
	 * @name get
	 * @memberOf Hash
	 * @param {string} key The key of the value to get.
	 * @returns {*} Returns the entry value.
	 */
	function hashGet(key) {
	  var data = this.__data__;
	  if (nativeCreate) {
	    var result = data[key];
	    return result === HASH_UNDEFINED ? undefined : result;
	  }
	  return hasOwnProperty.call(data, key) ? data[key] : undefined;
	}

	module.exports = hashGet;


/***/ },

/***/ 91:
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(80);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Checks if a hash value for `key` exists.
	 *
	 * @private
	 * @name has
	 * @memberOf Hash
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function hashHas(key) {
	  var data = this.__data__;
	  return nativeCreate ? (data[key] !== undefined) : hasOwnProperty.call(data, key);
	}

	module.exports = hashHas;


/***/ },

/***/ 92:
/***/ function(module, exports, __webpack_require__) {

	var nativeCreate = __webpack_require__(80);

	/** Used to stand-in for `undefined` hash values. */
	var HASH_UNDEFINED = '__lodash_hash_undefined__';

	/**
	 * Sets the hash `key` to `value`.
	 *
	 * @private
	 * @name set
	 * @memberOf Hash
	 * @param {string} key The key of the value to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns the hash instance.
	 */
	function hashSet(key, value) {
	  var data = this.__data__;
	  this.size += this.has(key) ? 0 : 1;
	  data[key] = (nativeCreate && value === undefined) ? HASH_UNDEFINED : value;
	  return this;
	}

	module.exports = hashSet;


/***/ },

/***/ 93:
/***/ function(module, exports, __webpack_require__) {

	var listCacheClear = __webpack_require__(94),
	    listCacheDelete = __webpack_require__(95),
	    listCacheGet = __webpack_require__(98),
	    listCacheHas = __webpack_require__(99),
	    listCacheSet = __webpack_require__(100);

	/**
	 * Creates an list cache object.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [entries] The key-value pairs to cache.
	 */
	function ListCache(entries) {
	  var index = -1,
	      length = entries == null ? 0 : entries.length;

	  this.clear();
	  while (++index < length) {
	    var entry = entries[index];
	    this.set(entry[0], entry[1]);
	  }
	}

	// Add methods to `ListCache`.
	ListCache.prototype.clear = listCacheClear;
	ListCache.prototype['delete'] = listCacheDelete;
	ListCache.prototype.get = listCacheGet;
	ListCache.prototype.has = listCacheHas;
	ListCache.prototype.set = listCacheSet;

	module.exports = ListCache;


/***/ },

/***/ 94:
/***/ function(module, exports) {

	/**
	 * Removes all key-value entries from the list cache.
	 *
	 * @private
	 * @name clear
	 * @memberOf ListCache
	 */
	function listCacheClear() {
	  this.__data__ = [];
	  this.size = 0;
	}

	module.exports = listCacheClear;


/***/ },

/***/ 95:
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(96);

	/** Used for built-in method references. */
	var arrayProto = Array.prototype;

	/** Built-in value references. */
	var splice = arrayProto.splice;

	/**
	 * Removes `key` and its value from the list cache.
	 *
	 * @private
	 * @name delete
	 * @memberOf ListCache
	 * @param {string} key The key of the value to remove.
	 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
	 */
	function listCacheDelete(key) {
	  var data = this.__data__,
	      index = assocIndexOf(data, key);

	  if (index < 0) {
	    return false;
	  }
	  var lastIndex = data.length - 1;
	  if (index == lastIndex) {
	    data.pop();
	  } else {
	    splice.call(data, index, 1);
	  }
	  --this.size;
	  return true;
	}

	module.exports = listCacheDelete;


/***/ },

/***/ 96:
/***/ function(module, exports, __webpack_require__) {

	var eq = __webpack_require__(97);

	/**
	 * Gets the index at which the `key` is found in `array` of key-value pairs.
	 *
	 * @private
	 * @param {Array} array The array to inspect.
	 * @param {*} key The key to search for.
	 * @returns {number} Returns the index of the matched value, else `-1`.
	 */
	function assocIndexOf(array, key) {
	  var length = array.length;
	  while (length--) {
	    if (eq(array[length][0], key)) {
	      return length;
	    }
	  }
	  return -1;
	}

	module.exports = assocIndexOf;


/***/ },

/***/ 97:
/***/ function(module, exports) {

	/**
	 * Performs a
	 * [`SameValueZero`](http://ecma-international.org/ecma-262/7.0/#sec-samevaluezero)
	 * comparison between two values to determine if they are equivalent.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to compare.
	 * @param {*} other The other value to compare.
	 * @returns {boolean} Returns `true` if the values are equivalent, else `false`.
	 * @example
	 *
	 * var object = { 'a': 1 };
	 * var other = { 'a': 1 };
	 *
	 * _.eq(object, object);
	 * // => true
	 *
	 * _.eq(object, other);
	 * // => false
	 *
	 * _.eq('a', 'a');
	 * // => true
	 *
	 * _.eq('a', Object('a'));
	 * // => false
	 *
	 * _.eq(NaN, NaN);
	 * // => true
	 */
	function eq(value, other) {
	  return value === other || (value !== value && other !== other);
	}

	module.exports = eq;


/***/ },

/***/ 98:
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(96);

	/**
	 * Gets the list cache value for `key`.
	 *
	 * @private
	 * @name get
	 * @memberOf ListCache
	 * @param {string} key The key of the value to get.
	 * @returns {*} Returns the entry value.
	 */
	function listCacheGet(key) {
	  var data = this.__data__,
	      index = assocIndexOf(data, key);

	  return index < 0 ? undefined : data[index][1];
	}

	module.exports = listCacheGet;


/***/ },

/***/ 99:
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(96);

	/**
	 * Checks if a list cache value for `key` exists.
	 *
	 * @private
	 * @name has
	 * @memberOf ListCache
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function listCacheHas(key) {
	  return assocIndexOf(this.__data__, key) > -1;
	}

	module.exports = listCacheHas;


/***/ },

/***/ 100:
/***/ function(module, exports, __webpack_require__) {

	var assocIndexOf = __webpack_require__(96);

	/**
	 * Sets the list cache `key` to `value`.
	 *
	 * @private
	 * @name set
	 * @memberOf ListCache
	 * @param {string} key The key of the value to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns the list cache instance.
	 */
	function listCacheSet(key, value) {
	  var data = this.__data__,
	      index = assocIndexOf(data, key);

	  if (index < 0) {
	    ++this.size;
	    data.push([key, value]);
	  } else {
	    data[index][1] = value;
	  }
	  return this;
	}

	module.exports = listCacheSet;


/***/ },

/***/ 101:
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(81),
	    root = __webpack_require__(8);

	/* Built-in method references that are verified to be native. */
	var Map = getNative(root, 'Map');

	module.exports = Map;


/***/ },

/***/ 102:
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(103);

	/**
	 * Removes `key` and its value from the map.
	 *
	 * @private
	 * @name delete
	 * @memberOf MapCache
	 * @param {string} key The key of the value to remove.
	 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
	 */
	function mapCacheDelete(key) {
	  var result = getMapData(this, key)['delete'](key);
	  this.size -= result ? 1 : 0;
	  return result;
	}

	module.exports = mapCacheDelete;


/***/ },

/***/ 103:
/***/ function(module, exports, __webpack_require__) {

	var isKeyable = __webpack_require__(104);

	/**
	 * Gets the data for `map`.
	 *
	 * @private
	 * @param {Object} map The map to query.
	 * @param {string} key The reference key.
	 * @returns {*} Returns the map data.
	 */
	function getMapData(map, key) {
	  var data = map.__data__;
	  return isKeyable(key)
	    ? data[typeof key == 'string' ? 'string' : 'hash']
	    : data.map;
	}

	module.exports = getMapData;


/***/ },

/***/ 104:
/***/ function(module, exports) {

	/**
	 * Checks if `value` is suitable for use as unique object key.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is suitable, else `false`.
	 */
	function isKeyable(value) {
	  var type = typeof value;
	  return (type == 'string' || type == 'number' || type == 'symbol' || type == 'boolean')
	    ? (value !== '__proto__')
	    : (value === null);
	}

	module.exports = isKeyable;


/***/ },

/***/ 105:
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(103);

	/**
	 * Gets the map value for `key`.
	 *
	 * @private
	 * @name get
	 * @memberOf MapCache
	 * @param {string} key The key of the value to get.
	 * @returns {*} Returns the entry value.
	 */
	function mapCacheGet(key) {
	  return getMapData(this, key).get(key);
	}

	module.exports = mapCacheGet;


/***/ },

/***/ 106:
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(103);

	/**
	 * Checks if a map value for `key` exists.
	 *
	 * @private
	 * @name has
	 * @memberOf MapCache
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function mapCacheHas(key) {
	  return getMapData(this, key).has(key);
	}

	module.exports = mapCacheHas;


/***/ },

/***/ 107:
/***/ function(module, exports, __webpack_require__) {

	var getMapData = __webpack_require__(103);

	/**
	 * Sets the map `key` to `value`.
	 *
	 * @private
	 * @name set
	 * @memberOf MapCache
	 * @param {string} key The key of the value to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns the map cache instance.
	 */
	function mapCacheSet(key, value) {
	  var data = getMapData(this, key),
	      size = data.size;

	  data.set(key, value);
	  this.size += data.size == size ? 0 : 1;
	  return this;
	}

	module.exports = mapCacheSet;


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

/***/ 111:
/***/ function(module, exports, __webpack_require__) {

	var isSymbol = __webpack_require__(72);

	/** Used as references for various `Number` constants. */
	var INFINITY = 1 / 0;

	/**
	 * Converts `value` to a string key if it's not a string or symbol.
	 *
	 * @private
	 * @param {*} value The value to inspect.
	 * @returns {string|symbol} Returns the key.
	 */
	function toKey(value) {
	  if (typeof value == 'string' || isSymbol(value)) {
	    return value;
	  }
	  var result = (value + '');
	  return (result == '0' && (1 / value) == -INFINITY) ? '-0' : result;
	}

	module.exports = toKey;


/***/ },

/***/ 112:
/***/ function(module, exports, __webpack_require__) {

	var baseSet = __webpack_require__(113);

	/**
	 * Sets the value at `path` of `object`. If a portion of `path` doesn't exist,
	 * it's created. Arrays are created for missing index properties while objects
	 * are created for all other missing properties. Use `_.setWith` to customize
	 * `path` creation.
	 *
	 * **Note:** This method mutates `object`.
	 *
	 * @static
	 * @memberOf _
	 * @since 3.7.0
	 * @category Object
	 * @param {Object} object The object to modify.
	 * @param {Array|string} path The path of the property to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns `object`.
	 * @example
	 *
	 * var object = { 'a': [{ 'b': { 'c': 3 } }] };
	 *
	 * _.set(object, 'a[0].b.c', 4);
	 * console.log(object.a[0].b.c);
	 * // => 4
	 *
	 * _.set(object, ['x', '0', 'y', 'z'], 5);
	 * console.log(object.x[0].y.z);
	 * // => 5
	 */
	function set(object, path, value) {
	  return object == null ? object : baseSet(object, path, value);
	}

	module.exports = set;


/***/ },

/***/ 113:
/***/ function(module, exports, __webpack_require__) {

	var assignValue = __webpack_require__(114),
	    castPath = __webpack_require__(69),
	    isIndex = __webpack_require__(117),
	    isObject = __webpack_require__(84),
	    toKey = __webpack_require__(111);

	/**
	 * The base implementation of `_.set`.
	 *
	 * @private
	 * @param {Object} object The object to modify.
	 * @param {Array|string} path The path of the property to set.
	 * @param {*} value The value to set.
	 * @param {Function} [customizer] The function to customize path creation.
	 * @returns {Object} Returns `object`.
	 */
	function baseSet(object, path, value, customizer) {
	  if (!isObject(object)) {
	    return object;
	  }
	  path = castPath(path, object);

	  var index = -1,
	      length = path.length,
	      lastIndex = length - 1,
	      nested = object;

	  while (nested != null && ++index < length) {
	    var key = toKey(path[index]),
	        newValue = value;

	    if (index != lastIndex) {
	      var objValue = nested[key];
	      newValue = customizer ? customizer(objValue, key, nested) : undefined;
	      if (newValue === undefined) {
	        newValue = isObject(objValue)
	          ? objValue
	          : (isIndex(path[index + 1]) ? [] : {});
	      }
	    }
	    assignValue(nested, key, newValue);
	    nested = nested[key];
	  }
	  return object;
	}

	module.exports = baseSet;


/***/ },

/***/ 114:
/***/ function(module, exports, __webpack_require__) {

	var baseAssignValue = __webpack_require__(115),
	    eq = __webpack_require__(97);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Assigns `value` to `key` of `object` if the existing value is not equivalent
	 * using [`SameValueZero`](http://ecma-international.org/ecma-262/7.0/#sec-samevaluezero)
	 * for equality comparisons.
	 *
	 * @private
	 * @param {Object} object The object to modify.
	 * @param {string} key The key of the property to assign.
	 * @param {*} value The value to assign.
	 */
	function assignValue(object, key, value) {
	  var objValue = object[key];
	  if (!(hasOwnProperty.call(object, key) && eq(objValue, value)) ||
	      (value === undefined && !(key in object))) {
	    baseAssignValue(object, key, value);
	  }
	}

	module.exports = assignValue;


/***/ },

/***/ 115:
/***/ function(module, exports, __webpack_require__) {

	var defineProperty = __webpack_require__(116);

	/**
	 * The base implementation of `assignValue` and `assignMergeValue` without
	 * value checks.
	 *
	 * @private
	 * @param {Object} object The object to modify.
	 * @param {string} key The key of the property to assign.
	 * @param {*} value The value to assign.
	 */
	function baseAssignValue(object, key, value) {
	  if (key == '__proto__' && defineProperty) {
	    defineProperty(object, key, {
	      'configurable': true,
	      'enumerable': true,
	      'value': value,
	      'writable': true
	    });
	  } else {
	    object[key] = value;
	  }
	}

	module.exports = baseAssignValue;


/***/ },

/***/ 116:
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(81);

	var defineProperty = (function() {
	  try {
	    var func = getNative(Object, 'defineProperty');
	    func({}, '', {});
	    return func;
	  } catch (e) {}
	}());

	module.exports = defineProperty;


/***/ },

/***/ 117:
/***/ function(module, exports) {

	/** Used as references for various `Number` constants. */
	var MAX_SAFE_INTEGER = 9007199254740991;

	/** Used to detect unsigned integer values. */
	var reIsUint = /^(?:0|[1-9]\d*)$/;

	/**
	 * Checks if `value` is a valid array-like index.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @param {number} [length=MAX_SAFE_INTEGER] The upper bounds of a valid index.
	 * @returns {boolean} Returns `true` if `value` is a valid index, else `false`.
	 */
	function isIndex(value, length) {
	  length = length == null ? MAX_SAFE_INTEGER : length;
	  return !!length &&
	    (typeof value == 'number' || reIsUint.test(value)) &&
	    (value > -1 && value % 1 == 0 && value < length);
	}

	module.exports = isIndex;


/***/ },

/***/ 118:
/***/ function(module, exports) {

	

/***/ },

/***/ 119:
/***/ function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(process) {// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	// resolves . and .. elements in a path array with directory names there
	// must be no slashes, empty elements, or device names (c:\) in the array
	// (so also no leading and trailing slashes - it does not distinguish
	// relative and absolute paths)
	function normalizeArray(parts, allowAboveRoot) {
	  // if the path tries to go above the root, `up` ends up > 0
	  var up = 0;
	  for (var i = parts.length - 1; i >= 0; i--) {
	    var last = parts[i];
	    if (last === '.') {
	      parts.splice(i, 1);
	    } else if (last === '..') {
	      parts.splice(i, 1);
	      up++;
	    } else if (up) {
	      parts.splice(i, 1);
	      up--;
	    }
	  }

	  // if the path is allowed to go above the root, restore leading ..s
	  if (allowAboveRoot) {
	    for (; up--; up) {
	      parts.unshift('..');
	    }
	  }

	  return parts;
	}

	// Split a filename into [root, dir, basename, ext], unix version
	// 'root' is just a slash, or nothing.
	var splitPathRe =
	    /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
	var splitPath = function(filename) {
	  return splitPathRe.exec(filename).slice(1);
	};

	// path.resolve([from ...], to)
	// posix version
	exports.resolve = function() {
	  var resolvedPath = '',
	      resolvedAbsolute = false;

	  for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
	    var path = (i >= 0) ? arguments[i] : process.cwd();

	    // Skip empty and invalid entries
	    if (typeof path !== 'string') {
	      throw new TypeError('Arguments to path.resolve must be strings');
	    } else if (!path) {
	      continue;
	    }

	    resolvedPath = path + '/' + resolvedPath;
	    resolvedAbsolute = path.charAt(0) === '/';
	  }

	  // At this point the path should be resolved to a full absolute path, but
	  // handle relative paths to be safe (might happen when process.cwd() fails)

	  // Normalize the path
	  resolvedPath = normalizeArray(filter(resolvedPath.split('/'), function(p) {
	    return !!p;
	  }), !resolvedAbsolute).join('/');

	  return ((resolvedAbsolute ? '/' : '') + resolvedPath) || '.';
	};

	// path.normalize(path)
	// posix version
	exports.normalize = function(path) {
	  var isAbsolute = exports.isAbsolute(path),
	      trailingSlash = substr(path, -1) === '/';

	  // Normalize the path
	  path = normalizeArray(filter(path.split('/'), function(p) {
	    return !!p;
	  }), !isAbsolute).join('/');

	  if (!path && !isAbsolute) {
	    path = '.';
	  }
	  if (path && trailingSlash) {
	    path += '/';
	  }

	  return (isAbsolute ? '/' : '') + path;
	};

	// posix version
	exports.isAbsolute = function(path) {
	  return path.charAt(0) === '/';
	};

	// posix version
	exports.join = function() {
	  var paths = Array.prototype.slice.call(arguments, 0);
	  return exports.normalize(filter(paths, function(p, index) {
	    if (typeof p !== 'string') {
	      throw new TypeError('Arguments to path.join must be strings');
	    }
	    return p;
	  }).join('/'));
	};


	// path.relative(from, to)
	// posix version
	exports.relative = function(from, to) {
	  from = exports.resolve(from).substr(1);
	  to = exports.resolve(to).substr(1);

	  function trim(arr) {
	    var start = 0;
	    for (; start < arr.length; start++) {
	      if (arr[start] !== '') break;
	    }

	    var end = arr.length - 1;
	    for (; end >= 0; end--) {
	      if (arr[end] !== '') break;
	    }

	    if (start > end) return [];
	    return arr.slice(start, end - start + 1);
	  }

	  var fromParts = trim(from.split('/'));
	  var toParts = trim(to.split('/'));

	  var length = Math.min(fromParts.length, toParts.length);
	  var samePartsLength = length;
	  for (var i = 0; i < length; i++) {
	    if (fromParts[i] !== toParts[i]) {
	      samePartsLength = i;
	      break;
	    }
	  }

	  var outputParts = [];
	  for (var i = samePartsLength; i < fromParts.length; i++) {
	    outputParts.push('..');
	  }

	  outputParts = outputParts.concat(toParts.slice(samePartsLength));

	  return outputParts.join('/');
	};

	exports.sep = '/';
	exports.delimiter = ':';

	exports.dirname = function(path) {
	  var result = splitPath(path),
	      root = result[0],
	      dir = result[1];

	  if (!root && !dir) {
	    // No dirname whatsoever
	    return '.';
	  }

	  if (dir) {
	    // It has a dirname, strip trailing slash
	    dir = dir.substr(0, dir.length - 1);
	  }

	  return root + dir;
	};


	exports.basename = function(path, ext) {
	  var f = splitPath(path)[2];
	  // TODO: make this comparison case-insensitive on windows?
	  if (ext && f.substr(-1 * ext.length) === ext) {
	    f = f.substr(0, f.length - ext.length);
	  }
	  return f;
	};


	exports.extname = function(path) {
	  return splitPath(path)[3];
	};

	function filter (xs, f) {
	    if (xs.filter) return xs.filter(f);
	    var res = [];
	    for (var i = 0; i < xs.length; i++) {
	        if (f(xs[i], i, xs)) res.push(xs[i]);
	    }
	    return res;
	}

	// String.prototype.substr - negative index don't work in IE8
	var substr = 'ab'.substr(-1) === 'b'
	    ? function (str, start, len) { return str.substr(start, len) }
	    : function (str, start, len) {
	        if (start < 0) start = str.length + start;
	        return str.substr(start, len);
	    }
	;

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(120)))

/***/ },

/***/ 120:
/***/ function(module, exports) {

	// shim for using process in browser
	var process = module.exports = {};

	// cached from whatever global is present so that test runners that stub it
	// don't break things.  But we need to wrap it in a try catch in case it is
	// wrapped in strict mode code which doesn't define any globals.  It's inside a
	// function because try/catches deoptimize in certain engines.

	var cachedSetTimeout;
	var cachedClearTimeout;

	function defaultSetTimout() {
	    throw new Error('setTimeout has not been defined');
	}
	function defaultClearTimeout () {
	    throw new Error('clearTimeout has not been defined');
	}
	(function () {
	    try {
	        if (typeof setTimeout === 'function') {
	            cachedSetTimeout = setTimeout;
	        } else {
	            cachedSetTimeout = defaultSetTimout;
	        }
	    } catch (e) {
	        cachedSetTimeout = defaultSetTimout;
	    }
	    try {
	        if (typeof clearTimeout === 'function') {
	            cachedClearTimeout = clearTimeout;
	        } else {
	            cachedClearTimeout = defaultClearTimeout;
	        }
	    } catch (e) {
	        cachedClearTimeout = defaultClearTimeout;
	    }
	} ())
	function runTimeout(fun) {
	    if (cachedSetTimeout === setTimeout) {
	        //normal enviroments in sane situations
	        return setTimeout(fun, 0);
	    }
	    // if setTimeout wasn't available but was latter defined
	    if ((cachedSetTimeout === defaultSetTimout || !cachedSetTimeout) && setTimeout) {
	        cachedSetTimeout = setTimeout;
	        return setTimeout(fun, 0);
	    }
	    try {
	        // when when somebody has screwed with setTimeout but no I.E. maddness
	        return cachedSetTimeout(fun, 0);
	    } catch(e){
	        try {
	            // When we are in I.E. but the script has been evaled so I.E. doesn't trust the global object when called normally
	            return cachedSetTimeout.call(null, fun, 0);
	        } catch(e){
	            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error
	            return cachedSetTimeout.call(this, fun, 0);
	        }
	    }


	}
	function runClearTimeout(marker) {
	    if (cachedClearTimeout === clearTimeout) {
	        //normal enviroments in sane situations
	        return clearTimeout(marker);
	    }
	    // if clearTimeout wasn't available but was latter defined
	    if ((cachedClearTimeout === defaultClearTimeout || !cachedClearTimeout) && clearTimeout) {
	        cachedClearTimeout = clearTimeout;
	        return clearTimeout(marker);
	    }
	    try {
	        // when when somebody has screwed with setTimeout but no I.E. maddness
	        return cachedClearTimeout(marker);
	    } catch (e){
	        try {
	            // When we are in I.E. but the script has been evaled so I.E. doesn't  trust the global object when called normally
	            return cachedClearTimeout.call(null, marker);
	        } catch (e){
	            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error.
	            // Some versions of I.E. have different rules for clearTimeout vs setTimeout
	            return cachedClearTimeout.call(this, marker);
	        }
	    }



	}
	var queue = [];
	var draining = false;
	var currentQueue;
	var queueIndex = -1;

	function cleanUpNextTick() {
	    if (!draining || !currentQueue) {
	        return;
	    }
	    draining = false;
	    if (currentQueue.length) {
	        queue = currentQueue.concat(queue);
	    } else {
	        queueIndex = -1;
	    }
	    if (queue.length) {
	        drainQueue();
	    }
	}

	function drainQueue() {
	    if (draining) {
	        return;
	    }
	    var timeout = runTimeout(cleanUpNextTick);
	    draining = true;

	    var len = queue.length;
	    while(len) {
	        currentQueue = queue;
	        queue = [];
	        while (++queueIndex < len) {
	            if (currentQueue) {
	                currentQueue[queueIndex].run();
	            }
	        }
	        queueIndex = -1;
	        len = queue.length;
	    }
	    currentQueue = null;
	    draining = false;
	    runClearTimeout(timeout);
	}

	process.nextTick = function (fun) {
	    var args = new Array(arguments.length - 1);
	    if (arguments.length > 1) {
	        for (var i = 1; i < arguments.length; i++) {
	            args[i - 1] = arguments[i];
	        }
	    }
	    queue.push(new Item(fun, args));
	    if (queue.length === 1 && !draining) {
	        runTimeout(drainQueue);
	    }
	};

	// v8 likes predictible objects
	function Item(fun, array) {
	    this.fun = fun;
	    this.array = array;
	}
	Item.prototype.run = function () {
	    this.fun.apply(null, this.array);
	};
	process.title = 'browser';
	process.browser = true;
	process.env = {};
	process.argv = [];
	process.version = ''; // empty string to avoid regexp issues
	process.versions = {};

	function noop() {}

	process.on = noop;
	process.addListener = noop;
	process.once = noop;
	process.off = noop;
	process.removeListener = noop;
	process.removeAllListeners = noop;
	process.emit = noop;
	process.prependListener = noop;
	process.prependOnceListener = noop;

	process.listeners = function (name) { return [] }

	process.binding = function (name) {
	    throw new Error('process.binding is not supported');
	};

	process.cwd = function () { return '/' };
	process.chdir = function (dir) {
	    throw new Error('process.chdir is not supported');
	};
	process.umask = function() { return 0; };


/***/ },

/***/ 121:
/***/ function(module, exports) {

	module.exports = __WEBPACK_EXTERNAL_MODULE_121__;

/***/ },

/***/ 198:
/***/ function(module, exports, __webpack_require__) {

	var DataView = __webpack_require__(199),
	    Map = __webpack_require__(101),
	    Promise = __webpack_require__(200),
	    Set = __webpack_require__(201),
	    WeakMap = __webpack_require__(202),
	    baseGetTag = __webpack_require__(6),
	    toSource = __webpack_require__(87);

	/** `Object#toString` result references. */
	var mapTag = '[object Map]',
	    objectTag = '[object Object]',
	    promiseTag = '[object Promise]',
	    setTag = '[object Set]',
	    weakMapTag = '[object WeakMap]';

	var dataViewTag = '[object DataView]';

	/** Used to detect maps, sets, and weakmaps. */
	var dataViewCtorString = toSource(DataView),
	    mapCtorString = toSource(Map),
	    promiseCtorString = toSource(Promise),
	    setCtorString = toSource(Set),
	    weakMapCtorString = toSource(WeakMap);

	/**
	 * Gets the `toStringTag` of `value`.
	 *
	 * @private
	 * @param {*} value The value to query.
	 * @returns {string} Returns the `toStringTag`.
	 */
	var getTag = baseGetTag;

	// Fallback for data views, maps, sets, and weak maps in IE 11 and promises in Node.js < 6.
	if ((DataView && getTag(new DataView(new ArrayBuffer(1))) != dataViewTag) ||
	    (Map && getTag(new Map) != mapTag) ||
	    (Promise && getTag(Promise.resolve()) != promiseTag) ||
	    (Set && getTag(new Set) != setTag) ||
	    (WeakMap && getTag(new WeakMap) != weakMapTag)) {
	  getTag = function(value) {
	    var result = baseGetTag(value),
	        Ctor = result == objectTag ? value.constructor : undefined,
	        ctorString = Ctor ? toSource(Ctor) : '';

	    if (ctorString) {
	      switch (ctorString) {
	        case dataViewCtorString: return dataViewTag;
	        case mapCtorString: return mapTag;
	        case promiseCtorString: return promiseTag;
	        case setCtorString: return setTag;
	        case weakMapCtorString: return weakMapTag;
	      }
	    }
	    return result;
	  };
	}

	module.exports = getTag;


/***/ },

/***/ 199:
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(81),
	    root = __webpack_require__(8);

	/* Built-in method references that are verified to be native. */
	var DataView = getNative(root, 'DataView');

	module.exports = DataView;


/***/ },

/***/ 200:
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(81),
	    root = __webpack_require__(8);

	/* Built-in method references that are verified to be native. */
	var Promise = getNative(root, 'Promise');

	module.exports = Promise;


/***/ },

/***/ 201:
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(81),
	    root = __webpack_require__(8);

	/* Built-in method references that are verified to be native. */
	var Set = getNative(root, 'Set');

	module.exports = Set;


/***/ },

/***/ 202:
/***/ function(module, exports, __webpack_require__) {

	var getNative = __webpack_require__(81),
	    root = __webpack_require__(8);

	/* Built-in method references that are verified to be native. */
	var WeakMap = getNative(root, 'WeakMap');

	module.exports = WeakMap;


/***/ },

/***/ 203:
/***/ function(module, exports) {

	/**
	 * Converts `map` to its key-value pairs.
	 *
	 * @private
	 * @param {Object} map The map to convert.
	 * @returns {Array} Returns the key-value pairs.
	 */
	function mapToArray(map) {
	  var index = -1,
	      result = Array(map.size);

	  map.forEach(function(value, key) {
	    result[++index] = [key, value];
	  });
	  return result;
	}

	module.exports = mapToArray;


/***/ },

/***/ 205:
/***/ function(module, exports, __webpack_require__) {

	var arrayLikeKeys = __webpack_require__(206),
	    baseKeys = __webpack_require__(217),
	    isArrayLike = __webpack_require__(220);

	/**
	 * Creates an array of the own enumerable property names of `object`.
	 *
	 * **Note:** Non-object values are coerced to objects. See the
	 * [ES spec](http://ecma-international.org/ecma-262/7.0/#sec-object.keys)
	 * for more details.
	 *
	 * @static
	 * @since 0.1.0
	 * @memberOf _
	 * @category Object
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the array of property names.
	 * @example
	 *
	 * function Foo() {
	 *   this.a = 1;
	 *   this.b = 2;
	 * }
	 *
	 * Foo.prototype.c = 3;
	 *
	 * _.keys(new Foo);
	 * // => ['a', 'b'] (iteration order is not guaranteed)
	 *
	 * _.keys('hi');
	 * // => ['0', '1']
	 */
	function keys(object) {
	  return isArrayLike(object) ? arrayLikeKeys(object) : baseKeys(object);
	}

	module.exports = keys;


/***/ },

/***/ 206:
/***/ function(module, exports, __webpack_require__) {

	var baseTimes = __webpack_require__(207),
	    isArguments = __webpack_require__(208),
	    isArray = __webpack_require__(70),
	    isBuffer = __webpack_require__(210),
	    isIndex = __webpack_require__(117),
	    isTypedArray = __webpack_require__(212);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Creates an array of the enumerable property names of the array-like `value`.
	 *
	 * @private
	 * @param {*} value The value to query.
	 * @param {boolean} inherited Specify returning inherited property names.
	 * @returns {Array} Returns the array of property names.
	 */
	function arrayLikeKeys(value, inherited) {
	  var isArr = isArray(value),
	      isArg = !isArr && isArguments(value),
	      isBuff = !isArr && !isArg && isBuffer(value),
	      isType = !isArr && !isArg && !isBuff && isTypedArray(value),
	      skipIndexes = isArr || isArg || isBuff || isType,
	      result = skipIndexes ? baseTimes(value.length, String) : [],
	      length = result.length;

	  for (var key in value) {
	    if ((inherited || hasOwnProperty.call(value, key)) &&
	        !(skipIndexes && (
	           // Safari 9 has enumerable `arguments.length` in strict mode.
	           key == 'length' ||
	           // Node.js 0.10 has enumerable non-index properties on buffers.
	           (isBuff && (key == 'offset' || key == 'parent')) ||
	           // PhantomJS 2 has enumerable non-index properties on typed arrays.
	           (isType && (key == 'buffer' || key == 'byteLength' || key == 'byteOffset')) ||
	           // Skip index properties.
	           isIndex(key, length)
	        ))) {
	      result.push(key);
	    }
	  }
	  return result;
	}

	module.exports = arrayLikeKeys;


/***/ },

/***/ 207:
/***/ function(module, exports) {

	/**
	 * The base implementation of `_.times` without support for iteratee shorthands
	 * or max array length checks.
	 *
	 * @private
	 * @param {number} n The number of times to invoke `iteratee`.
	 * @param {Function} iteratee The function invoked per iteration.
	 * @returns {Array} Returns the array of results.
	 */
	function baseTimes(n, iteratee) {
	  var index = -1,
	      result = Array(n);

	  while (++index < n) {
	    result[index] = iteratee(index);
	  }
	  return result;
	}

	module.exports = baseTimes;


/***/ },

/***/ 208:
/***/ function(module, exports, __webpack_require__) {

	var baseIsArguments = __webpack_require__(209),
	    isObjectLike = __webpack_require__(14);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/** Built-in value references. */
	var propertyIsEnumerable = objectProto.propertyIsEnumerable;

	/**
	 * Checks if `value` is likely an `arguments` object.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an `arguments` object,
	 *  else `false`.
	 * @example
	 *
	 * _.isArguments(function() { return arguments; }());
	 * // => true
	 *
	 * _.isArguments([1, 2, 3]);
	 * // => false
	 */
	var isArguments = baseIsArguments(function() { return arguments; }()) ? baseIsArguments : function(value) {
	  return isObjectLike(value) && hasOwnProperty.call(value, 'callee') &&
	    !propertyIsEnumerable.call(value, 'callee');
	};

	module.exports = isArguments;


/***/ },

/***/ 209:
/***/ function(module, exports, __webpack_require__) {

	var baseGetTag = __webpack_require__(6),
	    isObjectLike = __webpack_require__(14);

	/** `Object#toString` result references. */
	var argsTag = '[object Arguments]';

	/**
	 * The base implementation of `_.isArguments`.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an `arguments` object,
	 */
	function baseIsArguments(value) {
	  return isObjectLike(value) && baseGetTag(value) == argsTag;
	}

	module.exports = baseIsArguments;


/***/ },

/***/ 210:
/***/ function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(module) {var root = __webpack_require__(8),
	    stubFalse = __webpack_require__(211);

	/** Detect free variable `exports`. */
	var freeExports = typeof exports == 'object' && exports && !exports.nodeType && exports;

	/** Detect free variable `module`. */
	var freeModule = freeExports && typeof module == 'object' && module && !module.nodeType && module;

	/** Detect the popular CommonJS extension `module.exports`. */
	var moduleExports = freeModule && freeModule.exports === freeExports;

	/** Built-in value references. */
	var Buffer = moduleExports ? root.Buffer : undefined;

	/* Built-in method references for those with the same name as other `lodash` methods. */
	var nativeIsBuffer = Buffer ? Buffer.isBuffer : undefined;

	/**
	 * Checks if `value` is a buffer.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.3.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a buffer, else `false`.
	 * @example
	 *
	 * _.isBuffer(new Buffer(2));
	 * // => true
	 *
	 * _.isBuffer(new Uint8Array(2));
	 * // => false
	 */
	var isBuffer = nativeIsBuffer || stubFalse;

	module.exports = isBuffer;

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(51)(module)))

/***/ },

/***/ 211:
/***/ function(module, exports) {

	/**
	 * This method returns `false`.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.13.0
	 * @category Util
	 * @returns {boolean} Returns `false`.
	 * @example
	 *
	 * _.times(2, _.stubFalse);
	 * // => [false, false]
	 */
	function stubFalse() {
	  return false;
	}

	module.exports = stubFalse;


/***/ },

/***/ 212:
/***/ function(module, exports, __webpack_require__) {

	var baseIsTypedArray = __webpack_require__(213),
	    baseUnary = __webpack_require__(215),
	    nodeUtil = __webpack_require__(216);

	/* Node.js helper references. */
	var nodeIsTypedArray = nodeUtil && nodeUtil.isTypedArray;

	/**
	 * Checks if `value` is classified as a typed array.
	 *
	 * @static
	 * @memberOf _
	 * @since 3.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a typed array, else `false`.
	 * @example
	 *
	 * _.isTypedArray(new Uint8Array);
	 * // => true
	 *
	 * _.isTypedArray([]);
	 * // => false
	 */
	var isTypedArray = nodeIsTypedArray ? baseUnary(nodeIsTypedArray) : baseIsTypedArray;

	module.exports = isTypedArray;


/***/ },

/***/ 213:
/***/ function(module, exports, __webpack_require__) {

	var baseGetTag = __webpack_require__(6),
	    isLength = __webpack_require__(214),
	    isObjectLike = __webpack_require__(14);

	/** `Object#toString` result references. */
	var argsTag = '[object Arguments]',
	    arrayTag = '[object Array]',
	    boolTag = '[object Boolean]',
	    dateTag = '[object Date]',
	    errorTag = '[object Error]',
	    funcTag = '[object Function]',
	    mapTag = '[object Map]',
	    numberTag = '[object Number]',
	    objectTag = '[object Object]',
	    regexpTag = '[object RegExp]',
	    setTag = '[object Set]',
	    stringTag = '[object String]',
	    weakMapTag = '[object WeakMap]';

	var arrayBufferTag = '[object ArrayBuffer]',
	    dataViewTag = '[object DataView]',
	    float32Tag = '[object Float32Array]',
	    float64Tag = '[object Float64Array]',
	    int8Tag = '[object Int8Array]',
	    int16Tag = '[object Int16Array]',
	    int32Tag = '[object Int32Array]',
	    uint8Tag = '[object Uint8Array]',
	    uint8ClampedTag = '[object Uint8ClampedArray]',
	    uint16Tag = '[object Uint16Array]',
	    uint32Tag = '[object Uint32Array]';

	/** Used to identify `toStringTag` values of typed arrays. */
	var typedArrayTags = {};
	typedArrayTags[float32Tag] = typedArrayTags[float64Tag] =
	typedArrayTags[int8Tag] = typedArrayTags[int16Tag] =
	typedArrayTags[int32Tag] = typedArrayTags[uint8Tag] =
	typedArrayTags[uint8ClampedTag] = typedArrayTags[uint16Tag] =
	typedArrayTags[uint32Tag] = true;
	typedArrayTags[argsTag] = typedArrayTags[arrayTag] =
	typedArrayTags[arrayBufferTag] = typedArrayTags[boolTag] =
	typedArrayTags[dataViewTag] = typedArrayTags[dateTag] =
	typedArrayTags[errorTag] = typedArrayTags[funcTag] =
	typedArrayTags[mapTag] = typedArrayTags[numberTag] =
	typedArrayTags[objectTag] = typedArrayTags[regexpTag] =
	typedArrayTags[setTag] = typedArrayTags[stringTag] =
	typedArrayTags[weakMapTag] = false;

	/**
	 * The base implementation of `_.isTypedArray` without Node.js optimizations.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a typed array, else `false`.
	 */
	function baseIsTypedArray(value) {
	  return isObjectLike(value) &&
	    isLength(value.length) && !!typedArrayTags[baseGetTag(value)];
	}

	module.exports = baseIsTypedArray;


/***/ },

/***/ 214:
/***/ function(module, exports) {

	/** Used as references for various `Number` constants. */
	var MAX_SAFE_INTEGER = 9007199254740991;

	/**
	 * Checks if `value` is a valid array-like length.
	 *
	 * **Note:** This method is loosely based on
	 * [`ToLength`](http://ecma-international.org/ecma-262/7.0/#sec-tolength).
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a valid length, else `false`.
	 * @example
	 *
	 * _.isLength(3);
	 * // => true
	 *
	 * _.isLength(Number.MIN_VALUE);
	 * // => false
	 *
	 * _.isLength(Infinity);
	 * // => false
	 *
	 * _.isLength('3');
	 * // => false
	 */
	function isLength(value) {
	  return typeof value == 'number' &&
	    value > -1 && value % 1 == 0 && value <= MAX_SAFE_INTEGER;
	}

	module.exports = isLength;


/***/ },

/***/ 215:
/***/ function(module, exports) {

	/**
	 * The base implementation of `_.unary` without support for storing metadata.
	 *
	 * @private
	 * @param {Function} func The function to cap arguments for.
	 * @returns {Function} Returns the new capped function.
	 */
	function baseUnary(func) {
	  return function(value) {
	    return func(value);
	  };
	}

	module.exports = baseUnary;


/***/ },

/***/ 216:
/***/ function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(module) {var freeGlobal = __webpack_require__(9);

	/** Detect free variable `exports`. */
	var freeExports = typeof exports == 'object' && exports && !exports.nodeType && exports;

	/** Detect free variable `module`. */
	var freeModule = freeExports && typeof module == 'object' && module && !module.nodeType && module;

	/** Detect the popular CommonJS extension `module.exports`. */
	var moduleExports = freeModule && freeModule.exports === freeExports;

	/** Detect free variable `process` from Node.js. */
	var freeProcess = moduleExports && freeGlobal.process;

	/** Used to access faster Node.js helpers. */
	var nodeUtil = (function() {
	  try {
	    return freeProcess && freeProcess.binding && freeProcess.binding('util');
	  } catch (e) {}
	}());

	module.exports = nodeUtil;

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(51)(module)))

/***/ },

/***/ 217:
/***/ function(module, exports, __webpack_require__) {

	var isPrototype = __webpack_require__(218),
	    nativeKeys = __webpack_require__(219);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * The base implementation of `_.keys` which doesn't treat sparse arrays as dense.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the array of property names.
	 */
	function baseKeys(object) {
	  if (!isPrototype(object)) {
	    return nativeKeys(object);
	  }
	  var result = [];
	  for (var key in Object(object)) {
	    if (hasOwnProperty.call(object, key) && key != 'constructor') {
	      result.push(key);
	    }
	  }
	  return result;
	}

	module.exports = baseKeys;


/***/ },

/***/ 218:
/***/ function(module, exports) {

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/**
	 * Checks if `value` is likely a prototype object.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a prototype, else `false`.
	 */
	function isPrototype(value) {
	  var Ctor = value && value.constructor,
	      proto = (typeof Ctor == 'function' && Ctor.prototype) || objectProto;

	  return value === proto;
	}

	module.exports = isPrototype;


/***/ },

/***/ 219:
/***/ function(module, exports, __webpack_require__) {

	var overArg = __webpack_require__(13);

	/* Built-in method references for those with the same name as other `lodash` methods. */
	var nativeKeys = overArg(Object.keys, Object);

	module.exports = nativeKeys;


/***/ },

/***/ 220:
/***/ function(module, exports, __webpack_require__) {

	var isFunction = __webpack_require__(83),
	    isLength = __webpack_require__(214);

	/**
	 * Checks if `value` is array-like. A value is considered array-like if it's
	 * not a function and has a `value.length` that's an integer greater than or
	 * equal to `0` and less than or equal to `Number.MAX_SAFE_INTEGER`.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is array-like, else `false`.
	 * @example
	 *
	 * _.isArrayLike([1, 2, 3]);
	 * // => true
	 *
	 * _.isArrayLike(document.body.children);
	 * // => true
	 *
	 * _.isArrayLike('abc');
	 * // => true
	 *
	 * _.isArrayLike(_.noop);
	 * // => false
	 */
	function isArrayLike(value) {
	  return value != null && isLength(value.length) && !isFunction(value);
	}

	module.exports = isArrayLike;


/***/ },

/***/ 226:
/***/ function(module, exports, __webpack_require__) {

	// @flow

	const { isDevelopment } = __webpack_require__(828);
	const { Services, PrefsHelper } = __webpack_require__(830);

	const prefsSchemaVersion = "1.0.1";

	const pref = Services.pref;

	if (isDevelopment()) {
	  pref("devtools.debugger.client-source-maps-enabled", true);
	  pref("devtools.debugger.pause-on-exceptions", false);
	  pref("devtools.debugger.ignore-caught-exceptions", false);
	  pref("devtools.debugger.call-stack-visible", false);
	  pref("devtools.debugger.scopes-visible", false);
	  pref("devtools.debugger.start-panel-collapsed", false);
	  pref("devtools.debugger.end-panel-collapsed", false);
	  pref("devtools.debugger.tabs", "[]");
	  pref("devtools.debugger.ui.framework-grouping-on", true);
	  pref("devtools.debugger.pending-selected-location", "{}");
	  pref("devtools.debugger.pending-breakpoints", "{}");
	  pref("devtools.debugger.expressions", "[]");
	  pref("devtools.debugger.file-search-case-sensitive", false);
	  pref("devtools.debugger.file-search-whole-word", false);
	  pref("devtools.debugger.file-search-regex-match", false);
	  pref("devtools.debugger.prefs-schema-version", "1.0.0");
	}

	const prefs = new PrefsHelper("devtools", {
	  clientSourceMapsEnabled: ["Bool", "debugger.client-source-maps-enabled"],
	  pauseOnExceptions: ["Bool", "debugger.pause-on-exceptions"],
	  ignoreCaughtExceptions: ["Bool", "debugger.ignore-caught-exceptions"],
	  callStackVisible: ["Bool", "debugger.call-stack-visible"],
	  scopesVisible: ["Bool", "debugger.scopes-visible"],
	  startPanelCollapsed: ["Bool", "debugger.start-panel-collapsed"],
	  endPanelCollapsed: ["Bool", "debugger.end-panel-collapsed"],
	  frameworkGroupingOn: ["Bool", "debugger.ui.framework-grouping-on"],
	  tabs: ["Json", "debugger.tabs"],
	  pendingSelectedLocation: ["Json", "debugger.pending-selected-location"],
	  pendingBreakpoints: ["Json", "debugger.pending-breakpoints"],
	  expressions: ["Json", "debugger.expressions"],
	  fileSearchCaseSensitive: ["Bool", "debugger.file-search-case-sensitive"],
	  fileSearchWholeWord: ["Bool", "debugger.file-search-whole-word"],
	  fileSearchRegexMatch: ["Bool", "debugger.file-search-regex-match"],
	  debuggerPrefsSchemaVersion: ["Char", "debugger.prefs-schema-version"]
	});

	if (prefs.debuggerPrefsSchemaVersion !== prefsSchemaVersion) {
	  // clear pending Breakpoints
	  prefs.pendingBreakpoints = {};
	  prefs.debuggerPrefsSchemaVersion = prefsSchemaVersion;
	}

	module.exports = { prefs };


/***/ },

/***/ 264:
/***/ function(module, exports, __webpack_require__) {

	var baseMatches = __webpack_require__(265),
	    baseMatchesProperty = __webpack_require__(294),
	    identity = __webpack_require__(298),
	    isArray = __webpack_require__(70),
	    property = __webpack_require__(299);

	/**
	 * The base implementation of `_.iteratee`.
	 *
	 * @private
	 * @param {*} [value=_.identity] The value to convert to an iteratee.
	 * @returns {Function} Returns the iteratee.
	 */
	function baseIteratee(value) {
	  // Don't store the `typeof` result in a variable to avoid a JIT bug in Safari 9.
	  // See https://bugs.webkit.org/show_bug.cgi?id=156034 for more details.
	  if (typeof value == 'function') {
	    return value;
	  }
	  if (value == null) {
	    return identity;
	  }
	  if (typeof value == 'object') {
	    return isArray(value)
	      ? baseMatchesProperty(value[0], value[1])
	      : baseMatches(value);
	  }
	  return property(value);
	}

	module.exports = baseIteratee;


/***/ },

/***/ 265:
/***/ function(module, exports, __webpack_require__) {

	var baseIsMatch = __webpack_require__(266),
	    getMatchData = __webpack_require__(291),
	    matchesStrictComparable = __webpack_require__(293);

	/**
	 * The base implementation of `_.matches` which doesn't clone `source`.
	 *
	 * @private
	 * @param {Object} source The object of property values to match.
	 * @returns {Function} Returns the new spec function.
	 */
	function baseMatches(source) {
	  var matchData = getMatchData(source);
	  if (matchData.length == 1 && matchData[0][2]) {
	    return matchesStrictComparable(matchData[0][0], matchData[0][1]);
	  }
	  return function(object) {
	    return object === source || baseIsMatch(object, source, matchData);
	  };
	}

	module.exports = baseMatches;


/***/ },

/***/ 266:
/***/ function(module, exports, __webpack_require__) {

	var Stack = __webpack_require__(267),
	    baseIsEqual = __webpack_require__(273);

	/** Used to compose bitmasks for value comparisons. */
	var COMPARE_PARTIAL_FLAG = 1,
	    COMPARE_UNORDERED_FLAG = 2;

	/**
	 * The base implementation of `_.isMatch` without support for iteratee shorthands.
	 *
	 * @private
	 * @param {Object} object The object to inspect.
	 * @param {Object} source The object of property values to match.
	 * @param {Array} matchData The property names, values, and compare flags to match.
	 * @param {Function} [customizer] The function to customize comparisons.
	 * @returns {boolean} Returns `true` if `object` is a match, else `false`.
	 */
	function baseIsMatch(object, source, matchData, customizer) {
	  var index = matchData.length,
	      length = index,
	      noCustomizer = !customizer;

	  if (object == null) {
	    return !length;
	  }
	  object = Object(object);
	  while (index--) {
	    var data = matchData[index];
	    if ((noCustomizer && data[2])
	          ? data[1] !== object[data[0]]
	          : !(data[0] in object)
	        ) {
	      return false;
	    }
	  }
	  while (++index < length) {
	    data = matchData[index];
	    var key = data[0],
	        objValue = object[key],
	        srcValue = data[1];

	    if (noCustomizer && data[2]) {
	      if (objValue === undefined && !(key in object)) {
	        return false;
	      }
	    } else {
	      var stack = new Stack;
	      if (customizer) {
	        var result = customizer(objValue, srcValue, key, object, source, stack);
	      }
	      if (!(result === undefined
	            ? baseIsEqual(srcValue, objValue, COMPARE_PARTIAL_FLAG | COMPARE_UNORDERED_FLAG, customizer, stack)
	            : result
	          )) {
	        return false;
	      }
	    }
	  }
	  return true;
	}

	module.exports = baseIsMatch;


/***/ },

/***/ 267:
/***/ function(module, exports, __webpack_require__) {

	var ListCache = __webpack_require__(93),
	    stackClear = __webpack_require__(268),
	    stackDelete = __webpack_require__(269),
	    stackGet = __webpack_require__(270),
	    stackHas = __webpack_require__(271),
	    stackSet = __webpack_require__(272);

	/**
	 * Creates a stack cache object to store key-value pairs.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [entries] The key-value pairs to cache.
	 */
	function Stack(entries) {
	  var data = this.__data__ = new ListCache(entries);
	  this.size = data.size;
	}

	// Add methods to `Stack`.
	Stack.prototype.clear = stackClear;
	Stack.prototype['delete'] = stackDelete;
	Stack.prototype.get = stackGet;
	Stack.prototype.has = stackHas;
	Stack.prototype.set = stackSet;

	module.exports = Stack;


/***/ },

/***/ 268:
/***/ function(module, exports, __webpack_require__) {

	var ListCache = __webpack_require__(93);

	/**
	 * Removes all key-value entries from the stack.
	 *
	 * @private
	 * @name clear
	 * @memberOf Stack
	 */
	function stackClear() {
	  this.__data__ = new ListCache;
	  this.size = 0;
	}

	module.exports = stackClear;


/***/ },

/***/ 269:
/***/ function(module, exports) {

	/**
	 * Removes `key` and its value from the stack.
	 *
	 * @private
	 * @name delete
	 * @memberOf Stack
	 * @param {string} key The key of the value to remove.
	 * @returns {boolean} Returns `true` if the entry was removed, else `false`.
	 */
	function stackDelete(key) {
	  var data = this.__data__,
	      result = data['delete'](key);

	  this.size = data.size;
	  return result;
	}

	module.exports = stackDelete;


/***/ },

/***/ 270:
/***/ function(module, exports) {

	/**
	 * Gets the stack value for `key`.
	 *
	 * @private
	 * @name get
	 * @memberOf Stack
	 * @param {string} key The key of the value to get.
	 * @returns {*} Returns the entry value.
	 */
	function stackGet(key) {
	  return this.__data__.get(key);
	}

	module.exports = stackGet;


/***/ },

/***/ 271:
/***/ function(module, exports) {

	/**
	 * Checks if a stack value for `key` exists.
	 *
	 * @private
	 * @name has
	 * @memberOf Stack
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function stackHas(key) {
	  return this.__data__.has(key);
	}

	module.exports = stackHas;


/***/ },

/***/ 272:
/***/ function(module, exports, __webpack_require__) {

	var ListCache = __webpack_require__(93),
	    Map = __webpack_require__(101),
	    MapCache = __webpack_require__(76);

	/** Used as the size to enable large array optimizations. */
	var LARGE_ARRAY_SIZE = 200;

	/**
	 * Sets the stack `key` to `value`.
	 *
	 * @private
	 * @name set
	 * @memberOf Stack
	 * @param {string} key The key of the value to set.
	 * @param {*} value The value to set.
	 * @returns {Object} Returns the stack cache instance.
	 */
	function stackSet(key, value) {
	  var data = this.__data__;
	  if (data instanceof ListCache) {
	    var pairs = data.__data__;
	    if (!Map || (pairs.length < LARGE_ARRAY_SIZE - 1)) {
	      pairs.push([key, value]);
	      this.size = ++data.size;
	      return this;
	    }
	    data = this.__data__ = new MapCache(pairs);
	  }
	  data.set(key, value);
	  this.size = data.size;
	  return this;
	}

	module.exports = stackSet;


/***/ },

/***/ 273:
/***/ function(module, exports, __webpack_require__) {

	var baseIsEqualDeep = __webpack_require__(274),
	    isObjectLike = __webpack_require__(14);

	/**
	 * The base implementation of `_.isEqual` which supports partial comparisons
	 * and tracks traversed objects.
	 *
	 * @private
	 * @param {*} value The value to compare.
	 * @param {*} other The other value to compare.
	 * @param {boolean} bitmask The bitmask flags.
	 *  1 - Unordered comparison
	 *  2 - Partial comparison
	 * @param {Function} [customizer] The function to customize comparisons.
	 * @param {Object} [stack] Tracks traversed `value` and `other` objects.
	 * @returns {boolean} Returns `true` if the values are equivalent, else `false`.
	 */
	function baseIsEqual(value, other, bitmask, customizer, stack) {
	  if (value === other) {
	    return true;
	  }
	  if (value == null || other == null || (!isObjectLike(value) && !isObjectLike(other))) {
	    return value !== value && other !== other;
	  }
	  return baseIsEqualDeep(value, other, bitmask, customizer, baseIsEqual, stack);
	}

	module.exports = baseIsEqual;


/***/ },

/***/ 274:
/***/ function(module, exports, __webpack_require__) {

	var Stack = __webpack_require__(267),
	    equalArrays = __webpack_require__(275),
	    equalByTag = __webpack_require__(281),
	    equalObjects = __webpack_require__(284),
	    getTag = __webpack_require__(198),
	    isArray = __webpack_require__(70),
	    isBuffer = __webpack_require__(210),
	    isTypedArray = __webpack_require__(212);

	/** Used to compose bitmasks for value comparisons. */
	var COMPARE_PARTIAL_FLAG = 1;

	/** `Object#toString` result references. */
	var argsTag = '[object Arguments]',
	    arrayTag = '[object Array]',
	    objectTag = '[object Object]';

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * A specialized version of `baseIsEqual` for arrays and objects which performs
	 * deep comparisons and tracks traversed objects enabling objects with circular
	 * references to be compared.
	 *
	 * @private
	 * @param {Object} object The object to compare.
	 * @param {Object} other The other object to compare.
	 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
	 * @param {Function} customizer The function to customize comparisons.
	 * @param {Function} equalFunc The function to determine equivalents of values.
	 * @param {Object} [stack] Tracks traversed `object` and `other` objects.
	 * @returns {boolean} Returns `true` if the objects are equivalent, else `false`.
	 */
	function baseIsEqualDeep(object, other, bitmask, customizer, equalFunc, stack) {
	  var objIsArr = isArray(object),
	      othIsArr = isArray(other),
	      objTag = objIsArr ? arrayTag : getTag(object),
	      othTag = othIsArr ? arrayTag : getTag(other);

	  objTag = objTag == argsTag ? objectTag : objTag;
	  othTag = othTag == argsTag ? objectTag : othTag;

	  var objIsObj = objTag == objectTag,
	      othIsObj = othTag == objectTag,
	      isSameTag = objTag == othTag;

	  if (isSameTag && isBuffer(object)) {
	    if (!isBuffer(other)) {
	      return false;
	    }
	    objIsArr = true;
	    objIsObj = false;
	  }
	  if (isSameTag && !objIsObj) {
	    stack || (stack = new Stack);
	    return (objIsArr || isTypedArray(object))
	      ? equalArrays(object, other, bitmask, customizer, equalFunc, stack)
	      : equalByTag(object, other, objTag, bitmask, customizer, equalFunc, stack);
	  }
	  if (!(bitmask & COMPARE_PARTIAL_FLAG)) {
	    var objIsWrapped = objIsObj && hasOwnProperty.call(object, '__wrapped__'),
	        othIsWrapped = othIsObj && hasOwnProperty.call(other, '__wrapped__');

	    if (objIsWrapped || othIsWrapped) {
	      var objUnwrapped = objIsWrapped ? object.value() : object,
	          othUnwrapped = othIsWrapped ? other.value() : other;

	      stack || (stack = new Stack);
	      return equalFunc(objUnwrapped, othUnwrapped, bitmask, customizer, stack);
	    }
	  }
	  if (!isSameTag) {
	    return false;
	  }
	  stack || (stack = new Stack);
	  return equalObjects(object, other, bitmask, customizer, equalFunc, stack);
	}

	module.exports = baseIsEqualDeep;


/***/ },

/***/ 275:
/***/ function(module, exports, __webpack_require__) {

	var SetCache = __webpack_require__(276),
	    arraySome = __webpack_require__(279),
	    cacheHas = __webpack_require__(280);

	/** Used to compose bitmasks for value comparisons. */
	var COMPARE_PARTIAL_FLAG = 1,
	    COMPARE_UNORDERED_FLAG = 2;

	/**
	 * A specialized version of `baseIsEqualDeep` for arrays with support for
	 * partial deep comparisons.
	 *
	 * @private
	 * @param {Array} array The array to compare.
	 * @param {Array} other The other array to compare.
	 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
	 * @param {Function} customizer The function to customize comparisons.
	 * @param {Function} equalFunc The function to determine equivalents of values.
	 * @param {Object} stack Tracks traversed `array` and `other` objects.
	 * @returns {boolean} Returns `true` if the arrays are equivalent, else `false`.
	 */
	function equalArrays(array, other, bitmask, customizer, equalFunc, stack) {
	  var isPartial = bitmask & COMPARE_PARTIAL_FLAG,
	      arrLength = array.length,
	      othLength = other.length;

	  if (arrLength != othLength && !(isPartial && othLength > arrLength)) {
	    return false;
	  }
	  // Assume cyclic values are equal.
	  var stacked = stack.get(array);
	  if (stacked && stack.get(other)) {
	    return stacked == other;
	  }
	  var index = -1,
	      result = true,
	      seen = (bitmask & COMPARE_UNORDERED_FLAG) ? new SetCache : undefined;

	  stack.set(array, other);
	  stack.set(other, array);

	  // Ignore non-index properties.
	  while (++index < arrLength) {
	    var arrValue = array[index],
	        othValue = other[index];

	    if (customizer) {
	      var compared = isPartial
	        ? customizer(othValue, arrValue, index, other, array, stack)
	        : customizer(arrValue, othValue, index, array, other, stack);
	    }
	    if (compared !== undefined) {
	      if (compared) {
	        continue;
	      }
	      result = false;
	      break;
	    }
	    // Recursively compare arrays (susceptible to call stack limits).
	    if (seen) {
	      if (!arraySome(other, function(othValue, othIndex) {
	            if (!cacheHas(seen, othIndex) &&
	                (arrValue === othValue || equalFunc(arrValue, othValue, bitmask, customizer, stack))) {
	              return seen.push(othIndex);
	            }
	          })) {
	        result = false;
	        break;
	      }
	    } else if (!(
	          arrValue === othValue ||
	            equalFunc(arrValue, othValue, bitmask, customizer, stack)
	        )) {
	      result = false;
	      break;
	    }
	  }
	  stack['delete'](array);
	  stack['delete'](other);
	  return result;
	}

	module.exports = equalArrays;


/***/ },

/***/ 276:
/***/ function(module, exports, __webpack_require__) {

	var MapCache = __webpack_require__(76),
	    setCacheAdd = __webpack_require__(277),
	    setCacheHas = __webpack_require__(278);

	/**
	 *
	 * Creates an array cache object to store unique values.
	 *
	 * @private
	 * @constructor
	 * @param {Array} [values] The values to cache.
	 */
	function SetCache(values) {
	  var index = -1,
	      length = values == null ? 0 : values.length;

	  this.__data__ = new MapCache;
	  while (++index < length) {
	    this.add(values[index]);
	  }
	}

	// Add methods to `SetCache`.
	SetCache.prototype.add = SetCache.prototype.push = setCacheAdd;
	SetCache.prototype.has = setCacheHas;

	module.exports = SetCache;


/***/ },

/***/ 277:
/***/ function(module, exports) {

	/** Used to stand-in for `undefined` hash values. */
	var HASH_UNDEFINED = '__lodash_hash_undefined__';

	/**
	 * Adds `value` to the array cache.
	 *
	 * @private
	 * @name add
	 * @memberOf SetCache
	 * @alias push
	 * @param {*} value The value to cache.
	 * @returns {Object} Returns the cache instance.
	 */
	function setCacheAdd(value) {
	  this.__data__.set(value, HASH_UNDEFINED);
	  return this;
	}

	module.exports = setCacheAdd;


/***/ },

/***/ 278:
/***/ function(module, exports) {

	/**
	 * Checks if `value` is in the array cache.
	 *
	 * @private
	 * @name has
	 * @memberOf SetCache
	 * @param {*} value The value to search for.
	 * @returns {number} Returns `true` if `value` is found, else `false`.
	 */
	function setCacheHas(value) {
	  return this.__data__.has(value);
	}

	module.exports = setCacheHas;


/***/ },

/***/ 279:
/***/ function(module, exports) {

	/**
	 * A specialized version of `_.some` for arrays without support for iteratee
	 * shorthands.
	 *
	 * @private
	 * @param {Array} [array] The array to iterate over.
	 * @param {Function} predicate The function invoked per iteration.
	 * @returns {boolean} Returns `true` if any element passes the predicate check,
	 *  else `false`.
	 */
	function arraySome(array, predicate) {
	  var index = -1,
	      length = array == null ? 0 : array.length;

	  while (++index < length) {
	    if (predicate(array[index], index, array)) {
	      return true;
	    }
	  }
	  return false;
	}

	module.exports = arraySome;


/***/ },

/***/ 280:
/***/ function(module, exports) {

	/**
	 * Checks if a `cache` value for `key` exists.
	 *
	 * @private
	 * @param {Object} cache The cache to query.
	 * @param {string} key The key of the entry to check.
	 * @returns {boolean} Returns `true` if an entry for `key` exists, else `false`.
	 */
	function cacheHas(cache, key) {
	  return cache.has(key);
	}

	module.exports = cacheHas;


/***/ },

/***/ 281:
/***/ function(module, exports, __webpack_require__) {

	var Symbol = __webpack_require__(7),
	    Uint8Array = __webpack_require__(282),
	    eq = __webpack_require__(97),
	    equalArrays = __webpack_require__(275),
	    mapToArray = __webpack_require__(203),
	    setToArray = __webpack_require__(283);

	/** Used to compose bitmasks for value comparisons. */
	var COMPARE_PARTIAL_FLAG = 1,
	    COMPARE_UNORDERED_FLAG = 2;

	/** `Object#toString` result references. */
	var boolTag = '[object Boolean]',
	    dateTag = '[object Date]',
	    errorTag = '[object Error]',
	    mapTag = '[object Map]',
	    numberTag = '[object Number]',
	    regexpTag = '[object RegExp]',
	    setTag = '[object Set]',
	    stringTag = '[object String]',
	    symbolTag = '[object Symbol]';

	var arrayBufferTag = '[object ArrayBuffer]',
	    dataViewTag = '[object DataView]';

	/** Used to convert symbols to primitives and strings. */
	var symbolProto = Symbol ? Symbol.prototype : undefined,
	    symbolValueOf = symbolProto ? symbolProto.valueOf : undefined;

	/**
	 * A specialized version of `baseIsEqualDeep` for comparing objects of
	 * the same `toStringTag`.
	 *
	 * **Note:** This function only supports comparing values with tags of
	 * `Boolean`, `Date`, `Error`, `Number`, `RegExp`, or `String`.
	 *
	 * @private
	 * @param {Object} object The object to compare.
	 * @param {Object} other The other object to compare.
	 * @param {string} tag The `toStringTag` of the objects to compare.
	 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
	 * @param {Function} customizer The function to customize comparisons.
	 * @param {Function} equalFunc The function to determine equivalents of values.
	 * @param {Object} stack Tracks traversed `object` and `other` objects.
	 * @returns {boolean} Returns `true` if the objects are equivalent, else `false`.
	 */
	function equalByTag(object, other, tag, bitmask, customizer, equalFunc, stack) {
	  switch (tag) {
	    case dataViewTag:
	      if ((object.byteLength != other.byteLength) ||
	          (object.byteOffset != other.byteOffset)) {
	        return false;
	      }
	      object = object.buffer;
	      other = other.buffer;

	    case arrayBufferTag:
	      if ((object.byteLength != other.byteLength) ||
	          !equalFunc(new Uint8Array(object), new Uint8Array(other))) {
	        return false;
	      }
	      return true;

	    case boolTag:
	    case dateTag:
	    case numberTag:
	      // Coerce booleans to `1` or `0` and dates to milliseconds.
	      // Invalid dates are coerced to `NaN`.
	      return eq(+object, +other);

	    case errorTag:
	      return object.name == other.name && object.message == other.message;

	    case regexpTag:
	    case stringTag:
	      // Coerce regexes to strings and treat strings, primitives and objects,
	      // as equal. See http://www.ecma-international.org/ecma-262/7.0/#sec-regexp.prototype.tostring
	      // for more details.
	      return object == (other + '');

	    case mapTag:
	      var convert = mapToArray;

	    case setTag:
	      var isPartial = bitmask & COMPARE_PARTIAL_FLAG;
	      convert || (convert = setToArray);

	      if (object.size != other.size && !isPartial) {
	        return false;
	      }
	      // Assume cyclic values are equal.
	      var stacked = stack.get(object);
	      if (stacked) {
	        return stacked == other;
	      }
	      bitmask |= COMPARE_UNORDERED_FLAG;

	      // Recursively compare objects (susceptible to call stack limits).
	      stack.set(object, other);
	      var result = equalArrays(convert(object), convert(other), bitmask, customizer, equalFunc, stack);
	      stack['delete'](object);
	      return result;

	    case symbolTag:
	      if (symbolValueOf) {
	        return symbolValueOf.call(object) == symbolValueOf.call(other);
	      }
	  }
	  return false;
	}

	module.exports = equalByTag;


/***/ },

/***/ 282:
/***/ function(module, exports, __webpack_require__) {

	var root = __webpack_require__(8);

	/** Built-in value references. */
	var Uint8Array = root.Uint8Array;

	module.exports = Uint8Array;


/***/ },

/***/ 283:
/***/ function(module, exports) {

	/**
	 * Converts `set` to an array of its values.
	 *
	 * @private
	 * @param {Object} set The set to convert.
	 * @returns {Array} Returns the values.
	 */
	function setToArray(set) {
	  var index = -1,
	      result = Array(set.size);

	  set.forEach(function(value) {
	    result[++index] = value;
	  });
	  return result;
	}

	module.exports = setToArray;


/***/ },

/***/ 284:
/***/ function(module, exports, __webpack_require__) {

	var getAllKeys = __webpack_require__(285);

	/** Used to compose bitmasks for value comparisons. */
	var COMPARE_PARTIAL_FLAG = 1;

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * A specialized version of `baseIsEqualDeep` for objects with support for
	 * partial deep comparisons.
	 *
	 * @private
	 * @param {Object} object The object to compare.
	 * @param {Object} other The other object to compare.
	 * @param {number} bitmask The bitmask flags. See `baseIsEqual` for more details.
	 * @param {Function} customizer The function to customize comparisons.
	 * @param {Function} equalFunc The function to determine equivalents of values.
	 * @param {Object} stack Tracks traversed `object` and `other` objects.
	 * @returns {boolean} Returns `true` if the objects are equivalent, else `false`.
	 */
	function equalObjects(object, other, bitmask, customizer, equalFunc, stack) {
	  var isPartial = bitmask & COMPARE_PARTIAL_FLAG,
	      objProps = getAllKeys(object),
	      objLength = objProps.length,
	      othProps = getAllKeys(other),
	      othLength = othProps.length;

	  if (objLength != othLength && !isPartial) {
	    return false;
	  }
	  var index = objLength;
	  while (index--) {
	    var key = objProps[index];
	    if (!(isPartial ? key in other : hasOwnProperty.call(other, key))) {
	      return false;
	    }
	  }
	  // Assume cyclic values are equal.
	  var stacked = stack.get(object);
	  if (stacked && stack.get(other)) {
	    return stacked == other;
	  }
	  var result = true;
	  stack.set(object, other);
	  stack.set(other, object);

	  var skipCtor = isPartial;
	  while (++index < objLength) {
	    key = objProps[index];
	    var objValue = object[key],
	        othValue = other[key];

	    if (customizer) {
	      var compared = isPartial
	        ? customizer(othValue, objValue, key, other, object, stack)
	        : customizer(objValue, othValue, key, object, other, stack);
	    }
	    // Recursively compare objects (susceptible to call stack limits).
	    if (!(compared === undefined
	          ? (objValue === othValue || equalFunc(objValue, othValue, bitmask, customizer, stack))
	          : compared
	        )) {
	      result = false;
	      break;
	    }
	    skipCtor || (skipCtor = key == 'constructor');
	  }
	  if (result && !skipCtor) {
	    var objCtor = object.constructor,
	        othCtor = other.constructor;

	    // Non `Object` object instances with different constructors are not equal.
	    if (objCtor != othCtor &&
	        ('constructor' in object && 'constructor' in other) &&
	        !(typeof objCtor == 'function' && objCtor instanceof objCtor &&
	          typeof othCtor == 'function' && othCtor instanceof othCtor)) {
	      result = false;
	    }
	  }
	  stack['delete'](object);
	  stack['delete'](other);
	  return result;
	}

	module.exports = equalObjects;


/***/ },

/***/ 285:
/***/ function(module, exports, __webpack_require__) {

	var baseGetAllKeys = __webpack_require__(286),
	    getSymbols = __webpack_require__(288),
	    keys = __webpack_require__(205);

	/**
	 * Creates an array of own enumerable property names and symbols of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the array of property names and symbols.
	 */
	function getAllKeys(object) {
	  return baseGetAllKeys(object, keys, getSymbols);
	}

	module.exports = getAllKeys;


/***/ },

/***/ 286:
/***/ function(module, exports, __webpack_require__) {

	var arrayPush = __webpack_require__(287),
	    isArray = __webpack_require__(70);

	/**
	 * The base implementation of `getAllKeys` and `getAllKeysIn` which uses
	 * `keysFunc` and `symbolsFunc` to get the enumerable property names and
	 * symbols of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {Function} keysFunc The function to get the keys of `object`.
	 * @param {Function} symbolsFunc The function to get the symbols of `object`.
	 * @returns {Array} Returns the array of property names and symbols.
	 */
	function baseGetAllKeys(object, keysFunc, symbolsFunc) {
	  var result = keysFunc(object);
	  return isArray(object) ? result : arrayPush(result, symbolsFunc(object));
	}

	module.exports = baseGetAllKeys;


/***/ },

/***/ 287:
/***/ function(module, exports) {

	/**
	 * Appends the elements of `values` to `array`.
	 *
	 * @private
	 * @param {Array} array The array to modify.
	 * @param {Array} values The values to append.
	 * @returns {Array} Returns `array`.
	 */
	function arrayPush(array, values) {
	  var index = -1,
	      length = values.length,
	      offset = array.length;

	  while (++index < length) {
	    array[offset + index] = values[index];
	  }
	  return array;
	}

	module.exports = arrayPush;


/***/ },

/***/ 288:
/***/ function(module, exports, __webpack_require__) {

	var arrayFilter = __webpack_require__(289),
	    stubArray = __webpack_require__(290);

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Built-in value references. */
	var propertyIsEnumerable = objectProto.propertyIsEnumerable;

	/* Built-in method references for those with the same name as other `lodash` methods. */
	var nativeGetSymbols = Object.getOwnPropertySymbols;

	/**
	 * Creates an array of the own enumerable symbols of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the array of symbols.
	 */
	var getSymbols = !nativeGetSymbols ? stubArray : function(object) {
	  if (object == null) {
	    return [];
	  }
	  object = Object(object);
	  return arrayFilter(nativeGetSymbols(object), function(symbol) {
	    return propertyIsEnumerable.call(object, symbol);
	  });
	};

	module.exports = getSymbols;


/***/ },

/***/ 289:
/***/ function(module, exports) {

	/**
	 * A specialized version of `_.filter` for arrays without support for
	 * iteratee shorthands.
	 *
	 * @private
	 * @param {Array} [array] The array to iterate over.
	 * @param {Function} predicate The function invoked per iteration.
	 * @returns {Array} Returns the new filtered array.
	 */
	function arrayFilter(array, predicate) {
	  var index = -1,
	      length = array == null ? 0 : array.length,
	      resIndex = 0,
	      result = [];

	  while (++index < length) {
	    var value = array[index];
	    if (predicate(value, index, array)) {
	      result[resIndex++] = value;
	    }
	  }
	  return result;
	}

	module.exports = arrayFilter;


/***/ },

/***/ 290:
/***/ function(module, exports) {

	/**
	 * This method returns a new empty array.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.13.0
	 * @category Util
	 * @returns {Array} Returns the new empty array.
	 * @example
	 *
	 * var arrays = _.times(2, _.stubArray);
	 *
	 * console.log(arrays);
	 * // => [[], []]
	 *
	 * console.log(arrays[0] === arrays[1]);
	 * // => false
	 */
	function stubArray() {
	  return [];
	}

	module.exports = stubArray;


/***/ },

/***/ 291:
/***/ function(module, exports, __webpack_require__) {

	var isStrictComparable = __webpack_require__(292),
	    keys = __webpack_require__(205);

	/**
	 * Gets the property names, values, and compare flags of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the match data of `object`.
	 */
	function getMatchData(object) {
	  var result = keys(object),
	      length = result.length;

	  while (length--) {
	    var key = result[length],
	        value = object[key];

	    result[length] = [key, value, isStrictComparable(value)];
	  }
	  return result;
	}

	module.exports = getMatchData;


/***/ },

/***/ 292:
/***/ function(module, exports, __webpack_require__) {

	var isObject = __webpack_require__(84);

	/**
	 * Checks if `value` is suitable for strict equality comparisons, i.e. `===`.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` if suitable for strict
	 *  equality comparisons, else `false`.
	 */
	function isStrictComparable(value) {
	  return value === value && !isObject(value);
	}

	module.exports = isStrictComparable;


/***/ },

/***/ 293:
/***/ function(module, exports) {

	/**
	 * A specialized version of `matchesProperty` for source values suitable
	 * for strict equality comparisons, i.e. `===`.
	 *
	 * @private
	 * @param {string} key The key of the property to get.
	 * @param {*} srcValue The value to match.
	 * @returns {Function} Returns the new spec function.
	 */
	function matchesStrictComparable(key, srcValue) {
	  return function(object) {
	    if (object == null) {
	      return false;
	    }
	    return object[key] === srcValue &&
	      (srcValue !== undefined || (key in Object(object)));
	  };
	}

	module.exports = matchesStrictComparable;


/***/ },

/***/ 294:
/***/ function(module, exports, __webpack_require__) {

	var baseIsEqual = __webpack_require__(273),
	    get = __webpack_require__(67),
	    hasIn = __webpack_require__(295),
	    isKey = __webpack_require__(71),
	    isStrictComparable = __webpack_require__(292),
	    matchesStrictComparable = __webpack_require__(293),
	    toKey = __webpack_require__(111);

	/** Used to compose bitmasks for value comparisons. */
	var COMPARE_PARTIAL_FLAG = 1,
	    COMPARE_UNORDERED_FLAG = 2;

	/**
	 * The base implementation of `_.matchesProperty` which doesn't clone `srcValue`.
	 *
	 * @private
	 * @param {string} path The path of the property to get.
	 * @param {*} srcValue The value to match.
	 * @returns {Function} Returns the new spec function.
	 */
	function baseMatchesProperty(path, srcValue) {
	  if (isKey(path) && isStrictComparable(srcValue)) {
	    return matchesStrictComparable(toKey(path), srcValue);
	  }
	  return function(object) {
	    var objValue = get(object, path);
	    return (objValue === undefined && objValue === srcValue)
	      ? hasIn(object, path)
	      : baseIsEqual(srcValue, objValue, COMPARE_PARTIAL_FLAG | COMPARE_UNORDERED_FLAG);
	  };
	}

	module.exports = baseMatchesProperty;


/***/ },

/***/ 295:
/***/ function(module, exports, __webpack_require__) {

	var baseHasIn = __webpack_require__(296),
	    hasPath = __webpack_require__(297);

	/**
	 * Checks if `path` is a direct or inherited property of `object`.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Object
	 * @param {Object} object The object to query.
	 * @param {Array|string} path The path to check.
	 * @returns {boolean} Returns `true` if `path` exists, else `false`.
	 * @example
	 *
	 * var object = _.create({ 'a': _.create({ 'b': 2 }) });
	 *
	 * _.hasIn(object, 'a');
	 * // => true
	 *
	 * _.hasIn(object, 'a.b');
	 * // => true
	 *
	 * _.hasIn(object, ['a', 'b']);
	 * // => true
	 *
	 * _.hasIn(object, 'b');
	 * // => false
	 */
	function hasIn(object, path) {
	  return object != null && hasPath(object, path, baseHasIn);
	}

	module.exports = hasIn;


/***/ },

/***/ 296:
/***/ function(module, exports) {

	/**
	 * The base implementation of `_.hasIn` without support for deep paths.
	 *
	 * @private
	 * @param {Object} [object] The object to query.
	 * @param {Array|string} key The key to check.
	 * @returns {boolean} Returns `true` if `key` exists, else `false`.
	 */
	function baseHasIn(object, key) {
	  return object != null && key in Object(object);
	}

	module.exports = baseHasIn;


/***/ },

/***/ 297:
/***/ function(module, exports, __webpack_require__) {

	var castPath = __webpack_require__(69),
	    isArguments = __webpack_require__(208),
	    isArray = __webpack_require__(70),
	    isIndex = __webpack_require__(117),
	    isLength = __webpack_require__(214),
	    toKey = __webpack_require__(111);

	/**
	 * Checks if `path` exists on `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {Array|string} path The path to check.
	 * @param {Function} hasFunc The function to check properties.
	 * @returns {boolean} Returns `true` if `path` exists, else `false`.
	 */
	function hasPath(object, path, hasFunc) {
	  path = castPath(path, object);

	  var index = -1,
	      length = path.length,
	      result = false;

	  while (++index < length) {
	    var key = toKey(path[index]);
	    if (!(result = object != null && hasFunc(object, key))) {
	      break;
	    }
	    object = object[key];
	  }
	  if (result || ++index != length) {
	    return result;
	  }
	  length = object == null ? 0 : object.length;
	  return !!length && isLength(length) && isIndex(key, length) &&
	    (isArray(object) || isArguments(object));
	}

	module.exports = hasPath;


/***/ },

/***/ 298:
/***/ function(module, exports) {

	/**
	 * This method returns the first argument it receives.
	 *
	 * @static
	 * @since 0.1.0
	 * @memberOf _
	 * @category Util
	 * @param {*} value Any value.
	 * @returns {*} Returns `value`.
	 * @example
	 *
	 * var object = { 'a': 1 };
	 *
	 * console.log(_.identity(object) === object);
	 * // => true
	 */
	function identity(value) {
	  return value;
	}

	module.exports = identity;


/***/ },

/***/ 299:
/***/ function(module, exports, __webpack_require__) {

	var baseProperty = __webpack_require__(300),
	    basePropertyDeep = __webpack_require__(301),
	    isKey = __webpack_require__(71),
	    toKey = __webpack_require__(111);

	/**
	 * Creates a function that returns the value at `path` of a given object.
	 *
	 * @static
	 * @memberOf _
	 * @since 2.4.0
	 * @category Util
	 * @param {Array|string} path The path of the property to get.
	 * @returns {Function} Returns the new accessor function.
	 * @example
	 *
	 * var objects = [
	 *   { 'a': { 'b': 2 } },
	 *   { 'a': { 'b': 1 } }
	 * ];
	 *
	 * _.map(objects, _.property('a.b'));
	 * // => [2, 1]
	 *
	 * _.map(_.sortBy(objects, _.property(['a', 'b'])), 'a.b');
	 * // => [1, 2]
	 */
	function property(path) {
	  return isKey(path) ? baseProperty(toKey(path)) : basePropertyDeep(path);
	}

	module.exports = property;


/***/ },

/***/ 300:
/***/ function(module, exports) {

	/**
	 * The base implementation of `_.property` without support for deep paths.
	 *
	 * @private
	 * @param {string} key The key of the property to get.
	 * @returns {Function} Returns the new accessor function.
	 */
	function baseProperty(key) {
	  return function(object) {
	    return object == null ? undefined : object[key];
	  };
	}

	module.exports = baseProperty;


/***/ },

/***/ 301:
/***/ function(module, exports, __webpack_require__) {

	var baseGet = __webpack_require__(68);

	/**
	 * A specialized version of `baseProperty` which supports deep paths.
	 *
	 * @private
	 * @param {Array|string} path The path of the property to get.
	 * @returns {Function} Returns the new accessor function.
	 */
	function basePropertyDeep(path) {
	  return function(object) {
	    return baseGet(object, path);
	  };
	}

	module.exports = basePropertyDeep;


/***/ },

/***/ 395:
/***/ function(module, exports, __webpack_require__) {

	var createBaseFor = __webpack_require__(396);

	/**
	 * The base implementation of `baseForOwn` which iterates over `object`
	 * properties returned by `keysFunc` and invokes `iteratee` for each property.
	 * Iteratee functions may exit iteration early by explicitly returning `false`.
	 *
	 * @private
	 * @param {Object} object The object to iterate over.
	 * @param {Function} iteratee The function invoked per iteration.
	 * @param {Function} keysFunc The function to get the keys of `object`.
	 * @returns {Object} Returns `object`.
	 */
	var baseFor = createBaseFor();

	module.exports = baseFor;


/***/ },

/***/ 396:
/***/ function(module, exports) {

	/**
	 * Creates a base function for methods like `_.forIn` and `_.forOwn`.
	 *
	 * @private
	 * @param {boolean} [fromRight] Specify iterating from right to left.
	 * @returns {Function} Returns the new base function.
	 */
	function createBaseFor(fromRight) {
	  return function(object, iteratee, keysFunc) {
	    var index = -1,
	        iterable = Object(object),
	        props = keysFunc(object),
	        length = props.length;

	    while (length--) {
	      var key = props[fromRight ? length : ++index];
	      if (iteratee(iterable[key], key, iterable) === false) {
	        break;
	      }
	    }
	    return object;
	  };
	}

	module.exports = createBaseFor;


/***/ },

/***/ 757:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var tests = __webpack_require__(758);

	var _require = __webpack_require__(766),
	    setupTestRunner = _require.setupTestRunner;

	var utils = __webpack_require__(760);

	var _require2 = __webpack_require__(828),
	    isDevelopment = _require2.isDevelopment;

	if (isDevelopment()) {
	  __webpack_require__(792);
	}

	module.exports = Object.assign({}, tests, {
	  setupTestRunner,
	  utils
	});

/***/ },

/***/ 758:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	module.exports = {
	  asm: __webpack_require__(759),
	  breaking: __webpack_require__(769),
	  breakpoints: __webpack_require__(770),
	  breakpointsCond: __webpack_require__(771),
	  callStack: __webpack_require__(772),
	  expressions: __webpack_require__(773),
	  debuggerButtons: __webpack_require__(774),
	  editorSelect: __webpack_require__(775),
	  editorPreview: __webpack_require__(1020),
	  editorGutter: __webpack_require__(776),
	  editorHighlight: __webpack_require__(777),
	  keyboardNavigation: __webpack_require__(778),
	  keyboardShortcuts: __webpack_require__(779),
	  iframes: __webpack_require__(780),
	  navigation: __webpack_require__(781),
	  pauseOnExceptions: __webpack_require__(782),
	  prettyPrint: __webpack_require__(783),
	  prettyPrintPaused: __webpack_require__(784),
	  returnvalues: __webpack_require__(785),
	  scopes: __webpack_require__(786),
	  scopesMutations: __webpack_require__(1021),
	  searching: __webpack_require__(787),
	  sources: __webpack_require__(788),
	  sourceMaps: __webpack_require__(789),
	  sourceMapsReloading: __webpack_require__(1129),
	  sourceMaps2: __webpack_require__(790),
	  sourceMapsBogus: __webpack_require__(791),
	  tabs: __webpack_require__(1022)
	};

/***/ },

/***/ 759:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var asm = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var is = ctx.is;

	    var dbg = yield initDebugger("doc-asm.html");

	    var getSelectedSource = dbg.selectors.getSelectedSource,
	        getState = dbg.getState;


	    yield reload(dbg);

	    // After reload() we are getting getSources notifiction for old sources,
	    // using the debugger statement to really stop are reloaded page.
	    yield waitForPaused(dbg);
	    yield resume(dbg);

	    yield waitForSources(dbg, "doc-asm.html", "asm.js");

	    // Expand nodes and make sure more sources appear.
	    is(findAllElements(dbg, "sourceNodes").length, 2);

	    clickElement(dbg, "sourceArrow", 2);
	    is(findAllElements(dbg, "sourceNodes").length, 4);

	    selectSource(dbg, "asm.js");

	    yield addBreakpoint(dbg, "asm.js", 7);
	    invokeInTab(dbg, "runAsm");

	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "asm.js", 7);
	  });

	  return function asm(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    invokeInTab = _require.invokeInTab,
	    assertPausedLocation = _require.assertPausedLocation,
	    clickElement = _require.clickElement,
	    findAllElements = _require.findAllElements,
	    addBreakpoint = _require.addBreakpoint,
	    reload = _require.reload,
	    waitForPaused = _require.waitForPaused,
	    resume = _require.resume,
	    selectSource = _require.selectSource,
	    waitForSources = _require.waitForSources;

	module.exports = asm;

/***/ },

/***/ 760:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var wait = __webpack_require__(761);
	var assert = __webpack_require__(765);
	var mocha = __webpack_require__(766);
	var commands = __webpack_require__(767);
	var shared = __webpack_require__(762);

	module.exports = Object.assign({}, wait, assert, mocha, commands, shared);

/***/ },

/***/ 761:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	/**
	 * Waits for `predicate(state)` to be true. `state` is the redux app state.
	 *
	 * @memberof mochitest/waits
	 * @param {Object} dbg
	 * @param {Function} predicate
	 * @return {Promise}
	 * @static
	 */
	var waitForState = (() => {
	  var _ref = _asyncToGenerator(function* (dbg, predicate) {
	    return new Promise(function (resolve) {
	      var unsubscribe = dbg.store.subscribe(function () {
	        if (predicate(dbg.store.getState())) {
	          unsubscribe();
	          resolve();
	        }
	      });
	    });
	  });

	  return function waitForState(_x, _x2) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var waitForPaused = (() => {
	  var _ref2 = _asyncToGenerator(function* (dbg) {
	    // We want to make sure that we get both a real paused event and
	    // that the state is fully populated. The client may do some more
	    // work (call other client methods) before populating the state.
	    yield waitForThreadEvents(dbg, "paused"), yield waitForState(dbg, function (state) {
	      var pause = dbg.selectors.getPause(state);
	      // Make sure we have the paused state.
	      if (!pause) {
	        return false;
	      }
	      // Make sure the source text is completely loaded for the
	      // source we are paused in.
	      var sourceId = get(pause, "frame.location.sourceId");
	      var source = dbg.selectors.getSourc(dbg.getState(), sourceId);
	      return source && !source.get("loading");
	    });
	  });

	  return function waitForPaused(_x3) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	/**
	 * Wait for a specific action type to be dispatch.
	 * If an async action, will wait for it to be done.
	 *
	 * @memberof mochitest/waits
	 * @param {Object} dbg
	 * @param {String} type
	 * @param {Number} eventRepeat
	 * @return {Promise}
	 * @static
	 */


	var waitForDispatch = (() => {
	  var _ref3 = _asyncToGenerator(function* (dbg, type) {
	    var _ref4 = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {},
	        _ref4$useLaunchpad = _ref4.useLaunchpad,
	        useLaunchpad = _ref4$useLaunchpad === undefined ? false : _ref4$useLaunchpad,
	        _ref4$eventRepeat = _ref4.eventRepeat,
	        eventRepeat = _ref4$eventRepeat === undefined ? 1 : _ref4$eventRepeat;

	    var store = useLaunchpad ? dbg.launchpadStore : dbg.store;
	    var count = 0;

	    info("Waiting for " + type + " to dispatch " + eventRepeat + " time(s)");
	    while (count < eventRepeat) {
	      yield _afterDispatchDone(store, type);
	      count++;
	      info(type + " dispatched " + count + " time(s)");
	    }
	  });

	  return function waitForDispatch(_x4, _x5) {
	    return _ref3.apply(this, arguments);
	  };
	})();

	// Wait until an action of `type` is dispatched. If it's part of an
	// async operation, wait until the `status` field is "done" or "error"


	var _afterDispatchDone = (() => {
	  var _ref5 = _asyncToGenerator(function* (store, type) {
	    return new Promise(function (resolve) {
	      store.dispatch({
	        // Normally we would use `services.WAIT_UNTIL`, but use the
	        // internal name here so tests aren't forced to always pass it
	        // in
	        type: "@@service/waitUntil",
	        predicate: function (action) {
	          if (action.type === type) {
	            return action.status ? action.status === "done" || action.status === "error" : true;
	          }
	        },
	        run: function (dispatch, getState, action) {
	          resolve(action);
	        }
	      });
	    });
	  });

	  return function _afterDispatchDone(_x7, _x8) {
	    return _ref5.apply(this, arguments);
	  };
	})();

	// Wait until an action of `type` is dispatched. This is different
	// than `_afterDispatchDone` because it doesn't wait for async actions
	// to be done/errored. Use this if you want to listen for the "start"
	// action of an async operation (somewhat rare).


	var waitForNextDispatch = (() => {
	  var _ref6 = _asyncToGenerator(function* (store, type) {
	    return new Promise(function (resolve) {
	      store.dispatch({
	        // Normally we would use `services.WAIT_UNTIL`, but use the
	        // internal name here so tests aren't forced to always pass it
	        // in
	        type: "@@service/waitUntil",
	        predicate: function (action) {
	          return action.type === type;
	        },
	        run: function (dispatch, getState, action) {
	          resolve(action);
	        }
	      });
	    });
	  });

	  return function waitForNextDispatch(_x9, _x10) {
	    return _ref6.apply(this, arguments);
	  };
	})();

	var waitForTime = (() => {
	  var _ref7 = _asyncToGenerator(function* (time) {
	    return new Promise(function (resolve, reject) {
	      setTimeout(resolve, time);
	    });
	  });

	  return function waitForTime(_x11) {
	    return _ref7.apply(this, arguments);
	  };
	})();

	var waitForSources = (() => {
	  var _ref8 = _asyncToGenerator(function* (dbg) {
	    for (var _len = arguments.length, sources = Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
	      sources[_key - 1] = arguments[_key];
	    }

	    if (sources.length === 0) {
	      return Promise.resolve();
	    }

	    function sourceExists(state, url) {
	      return getSources(state).some(s => {
	        var sourceUrl = s.get("url");
	        return sourceUrl && sourceUrl.includes(url);
	      });
	    }

	    info("Waiting on sources: " + sources.join(", "));
	    var getSources = dbg.selectors.getSources,
	        store = dbg.store;

	    return Promise.all(sources.map(function (url) {
	      if (!sourceExists(store.getState(), url)) {
	        return waitForState(dbg, function () {
	          return sourceExists(store.getState(), url);
	        });
	      }
	    }));
	  });

	  return function waitForSources(_x12) {
	    return _ref8.apply(this, arguments);
	  };
	})();

	var waitForElement = (() => {
	  var _ref9 = _asyncToGenerator(function* (dbg, selector) {
	    return waitUntil(function () {
	      return findElementWithSelector(dbg, selector);
	    });
	  });

	  return function waitForElement(_x13, _x14) {
	    return _ref9.apply(this, arguments);
	  };
	})();

	var waitUntil = (() => {
	  var _ref10 = _asyncToGenerator(function* (predicate) {
	    var interval = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 20;

	    return new Promise(function (resolve) {
	      var timer = setInterval(function () {
	        if (predicate()) {
	          clearInterval(timer);
	          resolve();
	        }
	      }, interval);
	    });
	  });

	  return function waitUntil(_x15) {
	    return _ref10.apply(this, arguments);
	  };
	})();

	/**
	 * Waits for specific thread events.
	 *
	 * @memberof mochitest/waits
	 * @param {Object} dbg
	 * @param {String} eventName
	 * @return {Promise}
	 * @static
	 */


	var waitForThreadEvents = (() => {
	  var _ref11 = _asyncToGenerator(function* (dbg, eventName) {
	    info("Waiting for thread event '" + eventName + "' to fire.");
	    var thread = dbg.threadClient;

	    return new Promise(function (resolve, reject) {
	      thread.addListener(eventName, function onEvent(eventName) {
	        info("Thread event '" + eventName + "' fired.");
	        thread.removeListener(eventName, onEvent);
	        resolve.apply(resolve);
	      });
	    });
	  });

	  return function waitForThreadEvents(_x17, _x18) {
	    return _ref11.apply(this, arguments);
	  };
	})();

	var waitForTargetEvent = (() => {
	  var _ref12 = _asyncToGenerator(function* (dbg, eventName) {
	    info("Waiting for target event '" + eventName + "' to fire.");
	    var tabTarget = dbg.tabTarget;

	    return new Promise(function (resolve, reject) {
	      tabTarget.on(eventName, function onEvent(eventName) {
	        info("Thread event '" + eventName + "' fired.");
	        tabTarget.off(eventName, onEvent);
	        resolve.apply(resolve);
	      });
	    });
	  });

	  return function waitForTargetEvent(_x19, _x20) {
	    return _ref12.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var get = __webpack_require__(67);

	var _require = __webpack_require__(762),
	    findElementWithSelector = _require.findElementWithSelector,
	    info = _require.info;

	module.exports = {
	  waitForPaused,
	  waitForDispatch,
	  waitForTime,
	  waitForSources,
	  waitForElement,
	  waitForTargetEvent,
	  waitForThreadEvents,
	  waitUntil
	};

/***/ },

/***/ 762:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var mapValues = __webpack_require__(763);

	var selectors = {
	  callStackHeader: ".call-stack-pane ._header",
	  callStackBody: ".call-stack-pane .pane",
	  scopesHeader: ".scopes-pane ._header",
	  breakpointItem: i => `.breakpoints-list .breakpoint:nth-child(${i})`,
	  scopeNode: i => `.scopes-list .tree-node:nth-child(${i}) .object-label`,
	  scopeValue: i => `.scopes-list .tree-node:nth-child(${i}) .object-value`,
	  expressionNode: i => `.expressions-list .tree-node:nth-child(${i}) .object-label`,
	  expressionValue: i => `.expressions-list .tree-node:nth-child(${i}) .object-value`,
	  expressionClose: i => `.expressions-list .expression-container:nth-child(${i}) .close-btn`,
	  expressionNodes: ".expressions-list .tree-node",
	  frame: i => `.frames ul li:nth-child(${i})`,
	  frames: ".frames ul li",
	  gutter: i => `.CodeMirror-code *:nth-child(${i}) .CodeMirror-linenumber`,
	  menuitem: i => `menupopup menuitem:nth-child(${i})`,
	  pauseOnExceptions: ".pause-exceptions",
	  breakpoint: ".CodeMirror-code > .new-breakpoint",
	  highlightLine: ".CodeMirror-code > .highlight-line",
	  codeMirror: ".CodeMirror",
	  resume: ".resume.active",
	  stepOver: ".stepOver.active",
	  stepOut: ".stepOut.active",
	  stepIn: ".stepIn.active",
	  toggleBreakpoints: ".breakpoints-toggle",
	  prettyPrintButton: ".prettyPrint",
	  sourcesFooter: ".sources-panel .source-footer",
	  editorFooter: ".editor-pane .source-footer",
	  sourceNode: i => `.sources-list .tree-node:nth-child(${i}) .node`,
	  sourceNodes: ".sources-list .tree-node",
	  sourceArrow: i => `.sources-list .tree-node:nth-child(${i}) .arrow`,
	  sourceTabs: `.source-tabs`
	};

	function findElement(dbg, elementName) {
	  for (var _len = arguments.length, args = Array(_len > 2 ? _len - 2 : 0), _key = 2; _key < _len; _key++) {
	    args[_key - 2] = arguments[_key];
	  }

	  var selector = getSelector.apply(undefined, [elementName].concat(args));
	  return findElementWithSelector(dbg, selector);
	}

	function findElementWithSelector(dbg, selector) {
	  return dbg.win.document.querySelector(selector);
	}

	function findAllElements(dbg, elementName) {
	  for (var _len2 = arguments.length, args = Array(_len2 > 2 ? _len2 - 2 : 0), _key2 = 2; _key2 < _len2; _key2++) {
	    args[_key2 - 2] = arguments[_key2];
	  }

	  var selector = getSelector.apply(undefined, [elementName].concat(args));
	  return dbg.win.document.querySelectorAll(selector);
	}

	function getSelector(elementName) {
	  var selector = selectors[elementName];
	  if (!selector) {
	    throw new Error(`The selector ${elementName} is not defined`);
	  }

	  if (typeof selector == "function") {
	    for (var _len3 = arguments.length, args = Array(_len3 > 1 ? _len3 - 1 : 0), _key3 = 1; _key3 < _len3; _key3++) {
	      args[_key3 - 1] = arguments[_key3];
	    }

	    selector = selector.apply(undefined, args);
	  }

	  return selector;
	}

	function findSource(dbg, url) {
	  if (typeof url !== "string") {
	    // Support passing in a source object itelf all APIs that use this
	    // function support both styles
	    return url;
	  }

	  var sources = dbg.selectors.getSources(dbg.getState());
	  var source = sources.find(s => {
	    var sourceUrl = s.get("url");
	    return sourceUrl && sourceUrl.includes(url);
	  });

	  if (!source) {
	    throw new Error("Unable to find source: " + url);
	  }

	  return source.toJS();
	}

	function isPaused(dbg) {
	  var getPause = dbg.selectors.getPause,
	      getState = dbg.getState;

	  return !!getPause(getState());
	}

	function isVisibleWithin(outerEl, innerEl) {
	  var innerRect = innerEl.getBoundingClientRect();
	  var outerRect = outerEl.getBoundingClientRect();
	  return innerRect.top > outerRect.top && innerRect.bottom < outerRect.bottom;
	}

	function info(msg) {
	  var message = `INFO: ${msg}\n`;
	  if (typeof dump == "function") {
	    dump(message);
	  }

	  console.log(message);
	}

	module.exports = {
	  findElement,
	  findElementWithSelector,
	  findAllElements,
	  findSource,
	  selectors,
	  getSelector,
	  isPaused,
	  isVisibleWithin,
	  info
	};

/***/ },

/***/ 763:
/***/ function(module, exports, __webpack_require__) {

	var baseAssignValue = __webpack_require__(115),
	    baseForOwn = __webpack_require__(764),
	    baseIteratee = __webpack_require__(264);

	/**
	 * Creates an object with the same keys as `object` and values generated
	 * by running each own enumerable string keyed property of `object` thru
	 * `iteratee`. The iteratee is invoked with three arguments:
	 * (value, key, object).
	 *
	 * @static
	 * @memberOf _
	 * @since 2.4.0
	 * @category Object
	 * @param {Object} object The object to iterate over.
	 * @param {Function} [iteratee=_.identity] The function invoked per iteration.
	 * @returns {Object} Returns the new mapped object.
	 * @see _.mapKeys
	 * @example
	 *
	 * var users = {
	 *   'fred':    { 'user': 'fred',    'age': 40 },
	 *   'pebbles': { 'user': 'pebbles', 'age': 1 }
	 * };
	 *
	 * _.mapValues(users, function(o) { return o.age; });
	 * // => { 'fred': 40, 'pebbles': 1 } (iteration order is not guaranteed)
	 *
	 * // The `_.property` iteratee shorthand.
	 * _.mapValues(users, 'age');
	 * // => { 'fred': 40, 'pebbles': 1 } (iteration order is not guaranteed)
	 */
	function mapValues(object, iteratee) {
	  var result = {};
	  iteratee = baseIteratee(iteratee, 3);

	  baseForOwn(object, function(value, key, object) {
	    baseAssignValue(result, key, iteratee(value, key, object));
	  });
	  return result;
	}

	module.exports = mapValues;


/***/ },

/***/ 764:
/***/ function(module, exports, __webpack_require__) {

	var baseFor = __webpack_require__(395),
	    keys = __webpack_require__(205);

	/**
	 * The base implementation of `_.forOwn` without support for iteratee shorthands.
	 *
	 * @private
	 * @param {Object} object The object to iterate over.
	 * @param {Function} iteratee The function invoked per iteration.
	 * @returns {Object} Returns `object`.
	 */
	function baseForOwn(object, iteratee) {
	  return object && baseFor(object, iteratee, keys);
	}

	module.exports = baseForOwn;


/***/ },

/***/ 765:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var get = __webpack_require__(67);

	var _require = __webpack_require__(762),
	    findSource = _require.findSource,
	    findElement = _require.findElement,
	    isVisibleWithin = _require.isVisibleWithin,
	    isPaused = _require.isPaused;

	function assertPausedLocation(dbg, ctx, source, line) {
	  var _dbg$selectors = dbg.selectors,
	      getSelectedSource = _dbg$selectors.getSelectedSource,
	      getPause = _dbg$selectors.getPause,
	      getState = dbg.getState;

	  source = findSource(dbg, source);

	  var is = ctx.is,
	      ok = ctx.ok;

	  // Check the selected source

	  is(getSelectedSource(getState()).get("id"), source.id);

	  // Check the pause location
	  var location = get(getPause(getState()), "frame.location");
	  is(location.sourceId, source.id);
	  is(location.line, line);

	  // Check the debug line
	  ok(dbg.win.cm.lineInfo(line - 1).wrapClass.includes("debug-line"), "Line is highlighted as paused");
	}

	function assertNotPaused(dbg, ctx) {
	  var ok = ctx.ok;

	  ok(!isPaused(dbg), "not paused");
	}

	function assertHighlightLocation(dbg, ctx, source, line) {
	  var _dbg$selectors2 = dbg.selectors,
	      getSelectedSource = _dbg$selectors2.getSelectedSource,
	      getPause = _dbg$selectors2.getPause,
	      getState = dbg.getState;
	  var is = ctx.is,
	      ok = ctx.ok;

	  source = findSource(dbg, source);

	  // Check the selected source
	  is(getSelectedSource(getState()).get("url"), source.url);

	  // Check the highlight line
	  var lineEl = findElement(dbg, "highlightLine");
	  ok(lineEl, "Line is highlighted");
	  ok(isVisibleWithin(findElement(dbg, "codeMirror"), lineEl), "Highlighted line is visible");
	  ok(dbg.win.cm.lineInfo(line - 1).wrapClass.includes("highlight-line"), "Line is highlighted");
	}

	module.exports = {
	  assertPausedLocation,
	  assertNotPaused,
	  assertHighlightLocation
	};

/***/ },

/***/ 766:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var initDebugger = (() => {
	  var _ref = _asyncToGenerator(function* (url) {
	    Services.prefs.clearUserPref("devtools.debugger.tabs");
	    Services.prefs.clearUserPref("devtools.debugger.pending-selected-location");
	    url = url.startsWith("data:") ? url : EXAMPLE_URL + url;
	    var toolbox = yield openNewTabAndToolbox(url, "jsdebugger");
	    return createDebuggerContext(toolbox);
	  });

	  return function initDebugger(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(762),
	    info = _require.info;

	var ContentTask, gBrowser, isLinux, isMac, cmdOrCtrl, keyMappings, openNewTabAndToolbox, Services, EXAMPLE_URL, EventUtils;

	function setKeyboardMapping(isLinux, cmdOrCtrl) {
	  // On Mac, going to beginning/end only works with meta+left/right.  On
	  // Windows, it only works with home/end.  On Linux, apparently, either
	  // ctrl+left/right or home/end work.
	  var endKey = isMac ? { code: "VK_RIGHT", modifiers: cmdOrCtrl } : { code: "VK_END" };
	  var startKey = isMac ? { code: "VK_LEFT", modifiers: cmdOrCtrl } : { code: "VK_HOME" };
	  return {
	    sourceSearch: { code: "p", modifiers: cmdOrCtrl },
	    fileSearch: { code: "f", modifiers: cmdOrCtrl },
	    Enter: { code: "VK_RETURN" },
	    Up: { code: "VK_UP" },
	    Down: { code: "VK_DOWN" },
	    Right: { code: "VK_RIGHT" },
	    Left: { code: "VK_LEFT" },
	    End: endKey,
	    Start: startKey,
	    Tab: { code: "VK_TAB" },
	    Escape: { code: "VK_ESCAPE" },
	    pauseKey: { code: "VK_F8" },
	    resumeKey: { code: "VK_F8" },
	    stepOverKey: { code: "VK_F10" },
	    stepInKey: { code: "VK_F11", modifiers: { ctrlKey: isLinux } },
	    stepOutKey: {
	      code: "VK_F11",
	      modifiers: { ctrlKey: isLinux, shiftKey: true }
	    }
	  };
	}

	function setupTestRunner(context) {
	  ContentTask = context.ContentTask;
	  gBrowser = context.gBrowser;
	  openNewTabAndToolbox = context.openNewTabAndToolbox;
	  Services = context.Services;
	  EXAMPLE_URL = context.EXAMPLE_URL;
	  EventUtils = context.EventUtils;
	  isLinux = Services.appinfo.OS === "Linux";
	  isMac = Services.appinfo.OS === "Darwin";
	  cmdOrCtrl = isLinux ? { ctrlKey: true } : { metaKey: true };
	  keyMappings = setKeyboardMapping(isLinux, cmdOrCtrl);
	}

	function invokeInTab(dbg, fnc) {
	  info(`Invoking function ${fnc} in tab`);
	  return ContentTask.spawn(gBrowser.selectedBrowser, fnc, function* (fnc) {
	    content.wrappedJSObject[fnc](); // eslint-disable-line mozilla/no-cpows-in-tests, max-len
	  });
	}

	function evalInTab(dbg, script) {
	  info(`evaling script ${script}`);
	  return ContentTask.spawn(gBrowser.selectedBrowser, script, function (script) {
	    content.eval(script);
	  });
	}

	function selectMenuItem(dbg, index) {
	  // the context menu is in the toolbox window
	  var doc = dbg.toolbox.win.document;

	  // there are several context menus, we want the one with the menu-api
	  var popup = doc.querySelector('menupopup[menu-api="true"]');

	  var item = popup.querySelector(`menuitem:nth-child(${index})`);
	  return EventUtils.synthesizeMouseAtCenter(item, {}, dbg.toolbox.win);
	}

	/**
	 * Simulates a key press in the debugger window.
	 *
	 * @memberof mochitest/helpers
	 * @param {Object} dbg
	 * @param {String} keyName
	 * @return {Promise}
	 * @static
	 */
	function pressKey(dbg, keyName) {
	  var keyEvent = keyMappings[keyName];
	  var code = keyEvent.code,
	      modifiers = keyEvent.modifiers;

	  return EventUtils.synthesizeKey(code, modifiers || {}, dbg.win);
	}

	function type(dbg, string) {
	  string.split("").forEach(char => {
	    EventUtils.synthesizeKey(char, {}, dbg.win);
	  });
	}

	function countSources(dbg) {
	  var sources = dbg.selectors.getSources(dbg.getState());
	  return sources.size;
	}

	function createDebuggerContext(toolbox) {
	  var win = toolbox.getPanel("jsdebugger").panelWin;
	  var store = win.Debugger.store;

	  return {
	    actions: win.Debugger.actions,
	    selectors: win.Debugger.selectors,
	    getState: store.getState,
	    store: store,
	    client: win.Debugger.client,
	    threadClient: toolbox.threadClient,
	    toolbox: toolbox,
	    win: win
	  };
	}

	module.exports = {
	  invokeInTab,
	  evalInTab,
	  selectMenuItem,
	  pressKey,
	  type,
	  countSources,
	  setupTestRunner,
	  info,
	  initDebugger,
	  environment: "mochitest"
	};

/***/ },

/***/ 767:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	/**
	 * Selects the source.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @param {String} url
	 * @param {Number} line
	 * @return {Promise}
	 * @static
	 */
	var selectSource = (() => {
	  var _ref = _asyncToGenerator(function* (dbg, url, line) {
	    info("Selecting source: " + url);
	    var source = findSource(dbg, url);
	    var hasSource = !!dbg.selectors.getSource(dbg.getState(), source.id);
	    dbg.actions.selectSource(source.id, { line });
	    if (!hasSource) {
	      return waitForDispatch(dbg, "SELECT_SOURCE");
	    }
	  });

	  return function selectSource(_x, _x2, _x3) {
	    return _ref.apply(this, arguments);
	  };
	})();

	/**
	 * Steps over.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @return {Promise}
	 * @static
	 */


	var stepOver = (() => {
	  var _ref2 = _asyncToGenerator(function* (dbg) {
	    info("Stepping over");
	    dbg.actions.stepOver();
	    return waitForPaused(dbg);
	  });

	  return function stepOver(_x4) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	/**
	 * Steps in.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @return {Promise}
	 * @static
	 */


	var stepIn = (() => {
	  var _ref3 = _asyncToGenerator(function* (dbg) {
	    info("Stepping in");
	    dbg.actions.stepIn();
	    return waitForPaused(dbg);
	  });

	  return function stepIn(_x5) {
	    return _ref3.apply(this, arguments);
	  };
	})();

	/**
	 * Steps out.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @return {Promise}
	 * @static
	 */


	var stepOut = (() => {
	  var _ref4 = _asyncToGenerator(function* (dbg) {
	    info("Stepping out");
	    dbg.actions.stepOut();
	    return waitForPaused(dbg);
	  });

	  return function stepOut(_x6) {
	    return _ref4.apply(this, arguments);
	  };
	})();

	/**
	 * Resumes.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @return {Promise}
	 * @static
	 */


	var resume = (() => {
	  var _ref5 = _asyncToGenerator(function* (dbg) {
	    info("Resuming");
	    dbg.actions.resume();
	    return waitForThreadEvents(dbg, "resumed");
	  });

	  return function resume(_x7) {
	    return _ref5.apply(this, arguments);
	  };
	})();

	/**
	 * Reloads the debuggee.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @param {Array} sources
	 * @return {Promise}
	 * @static
	 */


	var reload = (() => {
	  var _ref6 = _asyncToGenerator(function* (dbg) {
	    yield dbg.client.reload();
	    yield waitForDispatch(dbg, "NAVIGATE");

	    for (var _len = arguments.length, sources = Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
	      sources[_key - 1] = arguments[_key];
	    }

	    return waitForSources.apply(undefined, [dbg].concat(sources));
	  });

	  return function reload(_x8) {
	    return _ref6.apply(this, arguments);
	  };
	})();

	/**
	 * Navigates the debuggee to another url.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @param {String} url
	 * @param {Array} sources
	 * @return {Promise}
	 * @static
	 */


	var navigate = (() => {
	  var _ref7 = _asyncToGenerator(function* (dbg, url) {
	    dbg.client.navigate(url);

	    for (var _len2 = arguments.length, sources = Array(_len2 > 2 ? _len2 - 2 : 0), _key2 = 2; _key2 < _len2; _key2++) {
	      sources[_key2 - 2] = arguments[_key2];
	    }

	    return waitForSources.apply(undefined, [dbg].concat(sources));
	  });

	  return function navigate(_x9, _x10) {
	    return _ref7.apply(this, arguments);
	  };
	})();

	/**
	 * Adds a breakpoint to a source at line/col.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @param {String} source
	 * @param {Number} line
	 * @param {Number} col
	 * @return {Promise}
	 * @static
	 */


	var addBreakpoint = (() => {
	  var _ref8 = _asyncToGenerator(function* (dbg, source, line, col) {
	    source = findSource(dbg, source);
	    var sourceId = source.id;
	    dbg.actions.addBreakpoint({ sourceId, line, col });
	    return waitForDispatch(dbg, "ADD_BREAKPOINT");
	  });

	  return function addBreakpoint(_x11, _x12, _x13, _x14) {
	    return _ref8.apply(this, arguments);
	  };
	})();

	/**
	 * Removes a breakpoint from a source at line/col.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @param {String} source
	 * @param {Number} line
	 * @param {Number} col
	 * @return {Promise}
	 * @static
	 */


	var removeBreakpoint = (() => {
	  var _ref9 = _asyncToGenerator(function* (dbg, sourceId, line, col) {
	    return dbg.actions.removeBreakpoint({ sourceId, line, col });
	  });

	  return function removeBreakpoint(_x15, _x16, _x17, _x18) {
	    return _ref9.apply(this, arguments);
	  };
	})();

	var disableBreakpoint = (() => {
	  var _ref10 = _asyncToGenerator(function* (dbg, source, line) {
	    return dbg.actions.disableBreakpoint({
	      sourceId: source.id,
	      line,
	      column: undefined
	    });
	  });

	  return function disableBreakpoint(_x19, _x20, _x21) {
	    return _ref10.apply(this, arguments);
	  };
	})();

	/**
	 * Toggles the Pause on exceptions feature in the debugger.
	 *
	 * @memberof mochitest/actions
	 * @param {Object} dbg
	 * @param {Boolean} pauseOnExceptions
	 * @param {Boolean} ignoreCaughtExceptions
	 * @return {Promise}
	 * @static
	 */


	var togglePauseOnExceptions = (() => {
	  var _ref11 = _asyncToGenerator(function* (dbg, pauseOnExceptions, ignoreCaughtExceptions) {
	    var command = dbg.actions.pauseOnExceptions(pauseOnExceptions, ignoreCaughtExceptions);

	    if (!isPaused(dbg)) {
	      return waitForThreadEvents(dbg, "resumed");
	    }

	    return command;
	  });

	  return function togglePauseOnExceptions(_x22, _x23, _x24) {
	    return _ref11.apply(this, arguments);
	  };
	})();

	var clickElement = (() => {
	  var _ref12 = _asyncToGenerator(function* (dbg, elementName) {
	    for (var _len3 = arguments.length, args = Array(_len3 > 2 ? _len3 - 2 : 0), _key3 = 2; _key3 < _len3; _key3++) {
	      args[_key3 - 2] = arguments[_key3];
	    }

	    var selector = getSelector.apply(undefined, [elementName].concat(args));
	    var el = dbg.win.document.querySelector(selector);
	    clickEl(dbg.win, el);
	  });

	  return function clickElement(_x25, _x26) {
	    return _ref12.apply(this, arguments);
	  };
	})();

	var dblClickElement = (() => {
	  var _ref13 = _asyncToGenerator(function* (dbg, elementName) {
	    for (var _len4 = arguments.length, args = Array(_len4 > 2 ? _len4 - 2 : 0), _key4 = 2; _key4 < _len4; _key4++) {
	      args[_key4 - 2] = arguments[_key4];
	    }

	    var selector = getSelector.apply(undefined, [elementName].concat(args));
	    var el = dbg.win.document.querySelector(selector);
	    dblClickEl(dbg.win, el);
	  });

	  return function dblClickElement(_x27, _x28) {
	    return _ref13.apply(this, arguments);
	  };
	})();

	var rightClickElement = (() => {
	  var _ref14 = _asyncToGenerator(function* (dbg, elementName) {
	    for (var _len5 = arguments.length, args = Array(_len5 > 2 ? _len5 - 2 : 0), _key5 = 2; _key5 < _len5; _key5++) {
	      args[_key5 - 2] = arguments[_key5];
	    }

	    var selector = getSelector.apply(undefined, [elementName].concat(args));
	    var el = dbg.win.document.querySelector(selector);
	    info("right click on the gutter", el);
	    rightClickEl(dbg.win, el);
	  });

	  return function rightClickElement(_x29, _x30) {
	    return _ref14.apply(this, arguments);
	  };
	})();

	// NOTE: we should fix this for mochitests. It's likely that `this` would work.


	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(768),
	    clickEl = _require.clickEl,
	    rightClickEl = _require.rightClickEl,
	    dblClickEl = _require.dblClickEl,
	    mouseOverEl = _require.mouseOverEl;

	function info(msg) {
	  console.log(`info: ${msg}\n`);
	}

	var _require2 = __webpack_require__(766),
	    evalInTab = _require2.evalInTab,
	    invokeInTab = _require2.invokeInTab,
	    selectMenuItem = _require2.selectMenuItem,
	    pressKey = _require2.pressKey,
	    type = _require2.type;

	var _require3 = __webpack_require__(762),
	    selectors = _require3.selectors,
	    findSource = _require3.findSource,
	    getSelector = _require3.getSelector,
	    info = _require3.info,
	    isPaused = _require3.isPaused;

	var _require4 = __webpack_require__(761),
	    waitForSources = _require4.waitForSources,
	    waitForDispatch = _require4.waitForDispatch,
	    waitForPaused = _require4.waitForPaused,
	    waitForThreadEvents = _require4.waitForThreadEvents;

	/**
	* Closes a tab
	*
	* @memberof mochitest/actions
	* @param {Object} dbg
	* @param {String} url
	* @return {Promise}
	* @static
	*/


	function closeTab(dbg, url) {
	  info("Closing tab: " + url);
	  var source = findSource(dbg, url);

	  dbg.actions.closeTab(source.url);
	}var winObj = typeof window == "Object" ? window : {};
	winObj.resumeTest = undefined;

	/**
	 * Pause the test and let you interact with the debugger.
	 * The test can be resumed by invoking `resumeTest` in the console.
	 *
	 * @memberof mochitest
	 * @static
	 */
	function pauseTest() {
	  info("Test paused. Invoke resumeTest to continue.");
	  return new Promise(resolve => resumeTest = resolve);
	}

	module.exports = {
	  closeTab,
	  selectSource,
	  stepOver,
	  stepIn,
	  stepOut,
	  resume,
	  reload,
	  navigate,
	  addBreakpoint,
	  removeBreakpoint,
	  disableBreakpoint,
	  togglePauseOnExceptions,
	  clickElement,
	  mouseOverEl,
	  navigate,
	  invokeInTab,
	  evalInTab,
	  rightClickElement,
	  dblClickElement,
	  selectMenuItem,
	  type,
	  pressKey,
	  pauseTest
	};

/***/ },

/***/ 768:
/***/ function(module, exports) {

	"use strict";

	function triggerMouseEvent(_ref) {
	  var type = _ref.type,
	      _ref$props = _ref.props,
	      props = _ref$props === undefined ? {} : _ref$props,
	      win = _ref.win,
	      el = _ref.el;

	  var event = new win.Event(type, {
	    view: win,
	    bubbles: true,
	    cancelable: true
	  });

	  var rect = el.getBoundingClientRect();
	  event = Object.assign(event, props, {
	    clientX: (rect.left + rect.right) / 2,
	    clientY: (rect.top + rect.bottom) / 2
	  });

	  el.dispatchEvent(event);
	}

	function clickEl(win, el) {
	  triggerMouseEvent({
	    type: "mousedown",
	    win: win,
	    el
	  });

	  triggerMouseEvent({
	    type: "click",
	    win: win,
	    el
	  });

	  return triggerMouseEvent({
	    type: "mouseup",
	    win: win,
	    el
	  });
	}

	function rightClickEl(win, el) {
	  return triggerMouseEvent({
	    type: "contextmenu",
	    props: { which: 3 },
	    win,
	    el
	  });
	}

	function dblClickEl(win, el) {
	  return triggerMouseEvent({
	    type: "dblclick",
	    win,
	    el
	  });
	}

	function mouseOverEl(win, el) {
	  triggerMouseEvent({
	    type: "mouseover",
	    win: win,
	    el
	  });
	}

	module.exports = {
	  clickEl,
	  rightClickEl,
	  dblClickEl,
	  mouseOverEl
	};

/***/ },

/***/ 769:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    findElement = _require.findElement,
	    selectSource = _require.selectSource,
	    clickElement = _require.clickElement,
	    addBreakpoint = _require.addBreakpoint,
	    reload = _require.reload,
	    stepOver = _require.stepOver,
	    invokeInTab = _require.invokeInTab,
	    resume = _require.resume,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch,
	    setupTestRunner = _require.setupTestRunner,
	    initDebugger = _require.initDebugger;

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is;

	    var dbg = yield initDebugger("doc-scripts.html", "scripts.html");
	    var getSelectedSource = dbg.selectors.getSelectedSource,
	        getState = dbg.getState;

	    // Make sure we can set a top-level breakpoint and it will be hit on
	    // reload.

	    yield addBreakpoint(dbg, "scripts.html", 18);
	    reload(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "scripts.html", 18);
	    yield resume(dbg);

	    var paused = waitForPaused(dbg);

	    // Create an eval script that pauses itself.
	    invokeInTab(dbg, "doEval");

	    yield paused;
	    yield resume(dbg);
	    var source = getSelectedSource(getState()).toJS();
	    ok(!source.url, "It is an eval source");

	    yield addBreakpoint(dbg, source, 5);
	    invokeInTab(dbg, "evaledFunc");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, source, 5);
	  });

	  function breaking(_x) {
	    return _ref.apply(this, arguments);
	  }

	  return breaking;
	})();

/***/ },

/***/ 770:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var removeBreakpoint = (() => {
	  var _ref = _asyncToGenerator(function* (dbg, index) {
	    var bp = findElement(dbg, "breakpointItem", index);
	    bp.querySelector(".close-btn").click();
	    yield waitForDispatch(dbg, "REMOVE_BREAKPOINT");
	  });

	  return function removeBreakpoint(_x, _x2) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var disableBreakpoint = (() => {
	  var _ref2 = _asyncToGenerator(function* (dbg, index) {
	    toggleBreakpoint(dbg, index);
	    yield waitForDispatch(dbg, "DISABLE_BREAKPOINT");
	  });

	  return function disableBreakpoint(_x3, _x4) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	var enableBreakpoint = (() => {
	  var _ref3 = _asyncToGenerator(function* (dbg, index) {
	    toggleBreakpoint(dbg, index);
	    yield waitForDispatch(dbg, "ENABLE_BREAKPOINT");
	  });

	  return function enableBreakpoint(_x5, _x6) {
	    return _ref3.apply(this, arguments);
	  };
	})();

	var toggleBreakpoints = (() => {
	  var _ref4 = _asyncToGenerator(function* (dbg) {
	    clickElement(dbg, "toggleBreakpoints");
	    yield waitForDispatch(dbg, "TOGGLE_BREAKPOINTS");
	  });

	  return function toggleBreakpoints(_x7) {
	    return _ref4.apply(this, arguments);
	  };
	})();

	var toggle = (() => {
	  var _ref5 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    var dbg = yield initDebugger("doc-scripts.html", "simple2");

	    // Create two breakpoints
	    yield selectSource(dbg, "simple2");
	    yield addBreakpoint(dbg, "simple2", 3);
	    yield addBreakpoint(dbg, "simple2", 5);

	    // Disable the first one
	    yield disableBreakpoint(dbg, 1);
	    var bp1 = findBreakpoint(dbg, "simple2", 3);
	    var bp2 = findBreakpoint(dbg, "simple2", 5);
	    is(bp1.disabled, true, "first breakpoint is disabled");
	    is(bp2.disabled, false, "second breakpoint is enabled");

	    // Disable and Re-Enable the second one
	    yield disableBreakpoint(dbg, 2);
	    yield enableBreakpoint(dbg, 2);
	    bp2 = findBreakpoint(dbg, "simple2", 5);
	    is(bp2.disabled, false, "second breakpoint is enabled");
	  });

	  return function toggle(_x8) {
	    return _ref5.apply(this, arguments);
	  };
	})();

	// toggle all


	var toggleAll = (() => {
	  var _ref6 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    var dbg = yield initDebugger("doc-scripts.html");

	    // Create two breakpoints
	    yield selectSource(dbg, "simple2");
	    yield addBreakpoint(dbg, "simple2", 3);
	    yield addBreakpoint(dbg, "simple2", 5);

	    // Disable all of the breakpoints
	    yield toggleBreakpoints(dbg);
	    var bp1 = findBreakpoint(dbg, "simple2", 3);
	    var bp2 = findBreakpoint(dbg, "simple2", 5);
	    is(bp1.disabled, true, "first breakpoint is disabled");
	    is(bp2.disabled, true, "second breakpoint is disabled");

	    // Enable all of the breakpoints
	    yield toggleBreakpoints(dbg);
	    bp1 = findBreakpoint(dbg, "simple2", 3);
	    bp2 = findBreakpoint(dbg, "simple2", 5);
	    is(bp1.disabled, false, "first breakpoint is enabled");
	    is(bp2.disabled, false, "second breakpoint is enabled");

	    // Remove the breakpoints
	    yield removeBreakpoint(dbg, 1);
	    yield removeBreakpoint(dbg, 1);
	    var bps = findBreakpoints(dbg);
	    is(bps.size, 0, "breakpoints are removed");
	  });

	  return function toggleAll(_x9) {
	    return _ref6.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    findElement = _require.findElement,
	    findSource = _require.findSource,
	    selectSource = _require.selectSource,
	    clickElement = _require.clickElement,
	    addBreakpoint = _require.addBreakpoint,
	    waitForDispatch = _require.waitForDispatch;

	function toggleBreakpoint(dbg, index) {
	  var bp = findElement(dbg, "breakpointItem", index);
	  var input = bp.querySelector("input");
	  input.click();
	}

	function findBreakpoint(dbg, url, line) {
	  var getBreakpoint = dbg.selectors.getBreakpoint,
	      getState = dbg.getState;

	  var source = findSource(dbg, url);
	  return getBreakpoint(getState(), { sourceId: source.id, line });
	}

	function findBreakpoints(dbg) {
	  var getBreakpoints = dbg.selectors.getBreakpoints,
	      getState = dbg.getState;

	  return getBreakpoints(getState());
	}

	module.exports = {
	  toggle,
	  toggleAll
	};

/***/ },

/***/ 771:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var setConditionalBreakpoint = (() => {
	  var _ref = _asyncToGenerator(function* (dbg, _ref2, index, condition) {
	    var info = _ref2.info;

	    info("right click on the gutter");
	    rightClickElement(dbg, "gutter", index);
	    selectMenuItem(dbg, 2);
	    yield waitForElement(dbg, cbInput);
	    var el = findElementWithSelector(dbg, cbInput);

	    type(dbg, condition);
	    pressKey(dbg, "Enter");
	  });

	  return function setConditionalBreakpoint(_x, _x2, _x3, _x4) {
	    return _ref.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    waitForPaused = _require.waitForPaused,
	    waitForElement = _require.waitForElement,
	    waitForDispatch = _require.waitForDispatch,
	    findSource = _require.findSource,
	    findElement = _require.findElement,
	    findElementWithSelector = _require.findElementWithSelector,
	    selectSource = _require.selectSource,
	    clickElement = _require.clickElement,
	    rightClickElement = _require.rightClickElement,
	    selectMenuItem = _require.selectMenuItem,
	    type = _require.type,
	    pressKey = _require.pressKey,
	    initDebugger = _require.initDebugger;

	var cbInput = ".conditional-breakpoint-panel input";

	function findBreakpoint(dbg, url, line) {
	  var getBreakpoint = dbg.selectors.getBreakpoint,
	      getState = dbg.getState;

	  var source = findSource(dbg, url);
	  return getBreakpoint(getState(), { sourceId: source.id, line });
	}

	module.exports = (() => {
	  var _ref3 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-scripts.html", "simple2");
	    yield selectSource(dbg, "simple2");

	    info("Adding a conditional Breakpoint");
	    yield setConditionalBreakpoint(dbg, ctx, 5, "1");
	    yield waitForDispatch(dbg, "ADD_BREAKPOINT");
	    var bp = findBreakpoint(dbg, "simple2", 5);
	    is(bp.condition, "1", "breakpoint is created with the condition");

	    info("Editing a conditional Breakpoint");
	    yield setConditionalBreakpoint(dbg, ctx, 5, "2");
	    yield waitForDispatch(dbg, "SET_BREAKPOINT_CONDITION");
	    bp = findBreakpoint(dbg, "simple2", 5);
	    is(bp.condition, "12", "breakpoint is created with the condition");

	    info("Removing a conditional breakpoint");
	    clickElement(dbg, "gutter", 5);
	    yield waitForDispatch(dbg, "REMOVE_BREAKPOINT");
	    bp = findBreakpoint(dbg, "simple2", 5);
	    is(bp, undefined, "breakpoint was removed");

	    info("Adding a condition to a breakpoint");
	    clickElement(dbg, "gutter", 5);
	    yield waitForDispatch(dbg, "ADD_BREAKPOINT");
	    yield setConditionalBreakpoint(dbg, ctx, 5, "1");
	    bp = findBreakpoint(dbg, "simple2", 5);
	    is(bp.condition, "1", "breakpoint is created with the condition");
	  });

	  function breakpointsCond(_x5) {
	    return _ref3.apply(this, arguments);
	  }

	  return breakpointsCond;
	})();

/***/ },

/***/ 772:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var test1 = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-script-switching.html");

	    toggleCallStack(dbg);

	    var notPaused = findElement(dbg, "callStackBody").innerText.trim();
	    is(notPaused, "Not Paused", "Not paused message is shown");

	    invokeInTab(dbg, "firstCall");
	    yield waitForPaused(dbg);

	    ok(isFrameSelected(dbg, 1, "secondCall"), "the first frame is selected");

	    clickElement(dbg, "frame", 2);
	    ok(isFrameSelected(dbg, 2, "firstCall"), "the second frame is selected");

	    var button = toggleButton(dbg);
	    ok(!button, "toggle button shouldn't be there");
	  });

	  return function test1(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var test2 = (() => {
	  var _ref2 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-frames.html", "frames");

	    // NOTE: The web test runner pauses on the inline debugger statement where the mochitest skips it.
	    if (isPaused(dbg)) {
	      yield resume(dbg);
	    }

	    toggleCallStack(dbg);

	    invokeInTab(dbg, "startRecursion");
	    yield waitForPaused(dbg);

	    ok(isFrameSelected(dbg, 1, "recurseA"), "the first frame is selected");

	    // check to make sure that the toggle button isn't there
	    var button = toggleButton(dbg);
	    var frames = findAllElements(dbg, "frames");
	    is(button.innerText, "Expand Rows", "toggle button should be expand");
	    is(frames.length, 7, "There should be at most seven frames");

	    button.click();

	    button = toggleButton(dbg);
	    frames = findAllElements(dbg, "frames");
	    is(button.innerText, "Collapse Rows", "toggle button should be collapse");

	    // the web runner uses an console eval to call the function, which adds an extra frame
	    var frameCount = environment == "mocha" ? 23 : 22;
	    is(frames.length, frameCount, "All of the frames should be shown");
	  });

	  return function test2(_x2) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    environment = _require.environment,
	    assertPausedLocation = _require.assertPausedLocation,
	    waitForPaused = _require.waitForPaused,
	    invokeInTab = _require.invokeInTab,
	    clickElement = _require.clickElement,
	    findElement = _require.findElement,
	    findAllElements = _require.findAllElements,
	    isPaused = _require.isPaused,
	    resume = _require.resume,
	    reload = _require.reload;

	// checks to see if the frame is selected and the title is correct


	function isFrameSelected(dbg, index, title) {
	  var $frame = findElement(dbg, "frame", index);
	  var frame = dbg.selectors.getSelectedFrame(dbg.getState());

	  var elSelected = $frame.classList.contains("selected");
	  var titleSelected = frame.displayName == title;

	  return elSelected && titleSelected;
	}

	function toggleButton(dbg) {
	  var callStackBody = findElement(dbg, "callStackBody");
	  return callStackBody.querySelector(".show-more");
	}

	function toggleCallStack(dbg) {
	  return findElement(dbg, "callStackHeader").click();
	}

	module.exports = {
	  test1,
	  test2
	};

/***/ },

/***/ 773:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var addExpression = (() => {
	  var _ref = _asyncToGenerator(function* (dbg, input) {
	    info("Adding an expression");
	    findElementWithSelector(dbg, selectors.input).focus();
	    type(dbg, input);
	    pressKey(dbg, "Enter");

	    yield waitForDispatch(dbg, "EVALUATE_EXPRESSION");
	  });

	  return function addExpression(_x, _x2) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var editExpression = (() => {
	  var _ref2 = _asyncToGenerator(function* (dbg, input) {
	    info("updating the expression");
	    dblClickElement(dbg, "expressionNode", 1);
	    type(dbg, input);
	    pressKey(dbg, "Enter");
	    yield waitForDispatch(dbg, "EVALUATE_EXPRESSION");
	  });

	  return function editExpression(_x3, _x4) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	var deleteExpression = (() => {
	  var _ref3 = _asyncToGenerator(function* (dbg, index) {
	    info("Deleting the expression");
	    var deleteExpression = waitForDispatch(dbg, "DELETE_EXPRESSION");
	    clickElement(dbg, "expressionClose", index);
	    yield deleteExpression;
	  });

	  return function deleteExpression(_x5, _x6) {
	    return _ref3.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    addBreakpoint = _require.addBreakpoint,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch,
	    type = _require.type,
	    pressKey = _require.pressKey,
	    findElementWithSelector = _require.findElementWithSelector,
	    findElement = _require.findElement,
	    findAllElements = _require.findAllElements,
	    invokeInTab = _require.invokeInTab,
	    clickElement = _require.clickElement,
	    dblClickElement = _require.dblClickElement;

	/**
	 * tests the watch expressions component
	 * 1. add watch expressions
	 * 2. edit watch expressions
	 * 3. delete watch expressions
	 */

	var selectors = {
	  input: "input.input-expression"
	};

	function getLabel(dbg, index) {
	  return findElement(dbg, "expressionNode", index).innerText;
	}

	function getValue(dbg, index) {
	  return findElement(dbg, "expressionValue", index).innerText;
	}

	module.exports = (() => {
	  var _ref4 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        requestLongerTimeout = ctx.requestLongerTimeout;

	    var dbg = yield initDebugger("doc-script-switching.html");

	    invokeInTab(dbg, "firstCall");
	    yield waitForPaused(dbg);

	    yield addExpression(dbg, "f");
	    is(getLabel(dbg, 1), "f");
	    is(getValue(dbg, 1), "(unavailable)");

	    yield editExpression(dbg, "oo");
	    is(getLabel(dbg, 1), "foo()");
	    is(getValue(dbg, 1), "");

	    yield deleteExpression(dbg, 1);
	    is(findAllElements(dbg, "expressionNodes").length, 0);
	  });

	  return function (_x7) {
	    return _ref4.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 774:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    waitForPaused = _require.waitForPaused,
	    invokeInTab = _require.invokeInTab,
	    clickElement = _require.clickElement,
	    findElement = _require.findElement,
	    reload = _require.reload;

	function clickStepOver(dbg) {
	  clickElement(dbg, "stepOver");
	  return waitForPaused(dbg);
	}

	function clickStepIn(dbg) {
	  clickElement(dbg, "stepIn");
	  return waitForPaused(dbg);
	}

	function clickStepOut(dbg) {
	  clickElement(dbg, "stepOut");
	  return waitForPaused(dbg);
	}

	/**
	 * Test debugger buttons
	 *  1. resume
	 *  2. stepOver
	 *  3. stepIn
	 *  4. stepOver to the end of a function
	 *  5. stepUp at the end of a function
	 */
	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-debugger-statements.html");

	    yield reload(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 8);

	    // resume
	    clickElement(dbg, "resume");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 12);

	    // step over
	    yield clickStepOver(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 13);

	    // step into
	    yield clickStepIn(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 18);

	    // step over
	    yield clickStepOver(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 20);

	    // step out
	    yield clickStepOut(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 20);
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 775:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    addBreakpoint = _require.addBreakpoint,
	    waitForPaused = _require.waitForPaused,
	    invokeInTab = _require.invokeInTab,
	    clickElement = _require.clickElement,
	    findElement = _require.findElement,
	    stepIn = _require.stepIn,
	    stepOut = _require.stepOut,
	    resume = _require.resume,
	    isVisibleWithin = _require.isVisibleWithin;

	// Tests that the editor highlights the correct location when the
	// debugger pauses

	// checks to see if the first breakpoint is visible


	function isElementVisible(dbg, elementName) {
	  var bpLine = findElement(dbg, elementName);
	  var cm = findElement(dbg, "codeMirror");
	  return bpLine && isVisibleWithin(cm, bpLine);
	}

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        requestLongerTimeout = ctx.requestLongerTimeout;

	    // This test runs too slowly on linux debug. I'd like to figure out
	    // which is the slowest part of this and make it run faster, but to
	    // fix a frequent failure allow a longer timeout.

	    requestLongerTimeout(2);

	    var dbg = yield initDebugger("doc-scripts.html");
	    var getSelectedSource = dbg.selectors.getSelectedSource,
	        getState = dbg.getState;

	    var simple1 = findSource(dbg, "simple1.js");
	    var simple2 = findSource(dbg, "simple2.js");

	    // Set the initial breakpoint.
	    yield addBreakpoint(dbg, simple1, 4);
	    ok(!getSelectedSource(getState()), "No selected source");

	    // Call the function that we set a breakpoint in.
	    invokeInTab(dbg, "main");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, simple1, 4);

	    // Step through to another file and make sure it's paused in the
	    // right place.
	    yield stepIn(dbg);
	    assertPausedLocation(dbg, ctx, simple2, 2);

	    // Step back out to the initial file.
	    yield stepOut(dbg);
	    yield stepOut(dbg);
	    assertPausedLocation(dbg, ctx, simple1, 5);
	    yield resume(dbg);

	    // Make sure that we can set a breakpoint on a line out of the
	    // viewport, and that pausing there scrolls the editor to it.
	    var longSrc = findSource(dbg, "long.js");
	    yield addBreakpoint(dbg, longSrc, 66);

	    invokeInTab(dbg, "testModel");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, longSrc, 66);
	    ok(isElementVisible(dbg, "breakpoint"), "Breakpoint is visible");
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 776:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    clickElement = _require.clickElement,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    selectSource = _require.selectSource,
	    addBreakpoint = _require.addBreakpoint,
	    assertHighlightLocation = _require.assertHighlightLocation,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch;

	// Tests the breakpoint gutter and making sure breakpoint icons exist
	// correctly

	// Utilities for interacting with the editor


	function clickGutter(dbg, line) {
	  clickElement(dbg, "gutter", line);
	}

	function getLineEl(dbg, line) {
	  var lines = dbg.win.document.querySelectorAll(".CodeMirror-code > div");
	  return lines[line - 1];
	}

	function assertEditorBreakpoint(dbg, ctx, line, shouldExist) {
	  var ok = ctx.ok;

	  var exists = !!getLineEl(dbg, line).querySelector(".new-breakpoint");
	  ok(exists === shouldExist, "Breakpoint " + (shouldExist ? "exists" : "does not exist") + " on line " + line);
	}

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-scripts.html");
	    var _dbg$selectors = dbg.selectors,
	        getBreakpoints = _dbg$selectors.getBreakpoints,
	        getBreakpoint = _dbg$selectors.getBreakpoint,
	        getState = dbg.getState;

	    var source = findSource(dbg, "simple1.js");

	    yield selectSource(dbg, source.url);

	    // Make sure that clicking the gutter creates a breakpoint icon.
	    clickGutter(dbg, 4);
	    yield waitForDispatch(dbg, "ADD_BREAKPOINT");
	    is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
	    assertEditorBreakpoint(dbg, ctx, 4, true);

	    // Make sure clicking at the same place removes the icon.
	    clickGutter(dbg, 4);
	    yield waitForDispatch(dbg, "REMOVE_BREAKPOINT");
	    is(getBreakpoints(getState()).size, 0, "No breakpoints exist");
	    assertEditorBreakpoint(dbg, ctx, 4, false);

	    // Test that a breakpoint icon slides down to the correct line.
	    clickGutter(dbg, 2);
	    yield waitForDispatch(dbg, "ADD_BREAKPOINT");
	    is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
	    ok(getBreakpoint(getState(), { sourceId: source.id, line: 4 }), "Breakpoint has correct line");
	    assertEditorBreakpoint(dbg, ctx, 2, false);
	    assertEditorBreakpoint(dbg, ctx, 4, true);

	    // Do the same sliding and make sure it works if there's already a
	    // breakpoint.
	    clickGutter(dbg, 2);
	    yield waitForDispatch(dbg, "ADD_BREAKPOINT");
	    is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
	    assertEditorBreakpoint(dbg, ctx, 2, false);
	    assertEditorBreakpoint(dbg, ctx, 4, true);

	    clickGutter(dbg, 4);
	    yield waitForDispatch(dbg, "REMOVE_BREAKPOINT");
	    is(getBreakpoints(getState()).size, 0, "No breakpoints exist");
	    assertEditorBreakpoint(dbg, ctx, 4, false);
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 777:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    addBreakpoint = _require.addBreakpoint,
	    assertHighlightLocation = _require.assertHighlightLocation,
	    selectSource = _require.selectSource,
	    findAllElements = _require.findAllElements,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch;

	// Tests that the editor will always highight the right line, no
	// matter if the source text doesn't exist yet or even if the source
	// doesn't exist.

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        EXAMPLE_URL = ctx.EXAMPLE_URL;


	    var dbg = yield initDebugger("doc-scripts.html");
	    var getSourceText = dbg.selectors.getSourceText,
	        getState = dbg.getState;

	    var sourceUrl = EXAMPLE_URL + "long.js";

	    // The source itself doesn't even exist yet, and using
	    // `selectSourceURL` will set a pending request to load this source
	    // and highlight a specific line.
	    dbg.actions.selectSourceURL(sourceUrl, { line: 66 });

	    // Wait for the source text to load and make sure we're in the right
	    // place.
	    yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");

	    // Jump to line 16 and make sure the editor scrolled.
	    yield selectSource(dbg, "long.js", 16);
	    assertHighlightLocation(dbg, ctx, "long.js", 16);

	    // Make sure only one line is ever highlighted and the flash
	    // animation is cancelled on old lines.
	    yield selectSource(dbg, "long.js", 17);
	    yield selectSource(dbg, "long.js", 18);
	    assertHighlightLocation(dbg, ctx, "long.js", 18);
	    is(findAllElements(dbg, "highlightLine").length, 1, "Only 1 line is highlighted");

	    // Test jumping to a line in a source that exists but hasn't been
	    // loaded yet.
	    selectSource(dbg, "simple1.js", 6);

	    // Make sure the source is in the loading state, wait for it to be
	    // fully loaded, and check the highlighted line.
	    var simple1 = findSource(dbg, "simple1.js");
	    ok(getSourceText(getState(), simple1.id).get("loading"));
	    yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
	    ok(getSourceText(getState(), simple1.id).get("text"));
	    assertHighlightLocation(dbg, ctx, "simple1.js", 6);
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 778:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var keyboardNavigation = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-scripts.html");
	    var doc = dbg.win.document;

	    yield selectSource(dbg, "simple2");

	    yield waitForElement(dbg, ".CodeMirror");
	    var editor = findElementWithSelector(dbg, ".CodeMirror");
	    var textarea = findElementWithSelector(dbg, "textarea");
	    editor.focus();

	    info("Enter code editor");
	    pressKey(dbg, "Enter");
	    is(textarea, doc.activeElement, "Editor is enabled");

	    info("Exit code editor and focus on container");
	    pressKey(dbg, "Escape");
	    is(editor, doc.activeElement, "Focused on container");

	    info("Enter code editor");
	    pressKey(dbg, "Tab");
	    is(textarea, doc.activeElement, "Editor is enabled");
	  });

	  return function keyboardNavigation(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    waitForElement = _require.waitForElement,
	    findElement = _require.findElement,
	    findElementWithSelector = _require.findElementWithSelector,
	    selectSource = _require.selectSource,
	    pressKey = _require.pressKey;

	module.exports = keyboardNavigation;

/***/ },

/***/ 779:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var keyboardShortcuts = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var info = ctx.info;

	    var dbg = yield initDebugger("doc-debugger-statements.html");

	    yield reload(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 8);

	    yield pressResume(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 12);

	    yield pressStepIn(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 13);

	    yield pressStepOut(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 14);

	    yield pressStepOver(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 9);
	  });

	  return function keyboardShortcuts(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    waitForElement = _require.waitForElement,
	    waitForPaused = _require.waitForPaused,
	    assertPausedLocation = _require.assertPausedLocation,
	    pressKey = _require.pressKey,
	    reload = _require.reload;

	function pressResume(dbg) {
	  pressKey(dbg, "resumeKey");
	  return waitForPaused(dbg);
	}

	function pressStepOver(dbg) {
	  pressKey(dbg, "stepOverKey");
	  return waitForPaused(dbg);
	}

	function pressStepIn(dbg) {
	  pressKey(dbg, "stepInKey");
	  return waitForPaused(dbg);
	}

	function pressStepOut(dbg) {
	  pressKey(dbg, "stepOutKey");
	  return waitForPaused(dbg);
	}

	module.exports = keyboardShortcuts;

/***/ },

/***/ 780:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    reload = _require.reload,
	    resume = _require.resume,
	    waitForPaused = _require.waitForPaused;

	/**
	 * Test debugging a page with iframes
	 *  1. pause in the main thread
	 *  2. pause in the iframe
	 */


	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-iframes.html");

	    // test pausing in the main thread
	    yield reload(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "iframes.html", 8);

	    // test pausing in the iframe
	    yield resume(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 8);

	    // test pausing in the iframe
	    yield resume(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "debugger-statements.html", 12);
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 781:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    invokeInTab = _require.invokeInTab,
	    waitForElement = _require.waitForElement,
	    navigate = _require.navigate,
	    addBreakpoint = _require.addBreakpoint,
	    selectSource = _require.selectSource,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch,
	    assertPausedLocation = _require.assertPausedLocation,
	    reload = _require.reload;

	function countSources(dbg) {
	  var sources = dbg.selectors.getSources(dbg.getState());
	  return sources.size;
	}

	/**
	 * Test navigating
	 * navigating while paused will reset the pause state and sources
	 */
	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-script-switching.html");
	    var _dbg$selectors = dbg.selectors,
	        getSelectedSource = _dbg$selectors.getSelectedSource,
	        getPause = _dbg$selectors.getPause,
	        getState = dbg.getState;


	    invokeInTab(dbg, "firstCall");
	    yield waitForPaused(dbg);

	    yield navigate(dbg, "doc-scripts.html", "simple1.js");
	    yield addBreakpoint(dbg, "simple1.js", 4);
	    invokeInTab(dbg, "main");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "simple1.js", 4);
	    is(countSources(dbg), 4, "4 sources are loaded.");

	    yield navigate(dbg, "about:blank");
	    yield waitForDispatch(dbg, "NAVIGATE");
	    is(countSources(dbg), 0, "0 sources are loaded.");
	    ok(!getPause(getState()), "No pause state exists");

	    yield navigate(dbg, "doc-scripts.html", "simple1.js", "simple2.js", "long.js", "scripts.html");

	    is(countSources(dbg), 4, "4 sources are loaded.");

	    // Test that the current select source persists across reloads
	    yield selectSource(dbg, "long.js");
	    yield reload(dbg, "long.js");
	    ok(getSelectedSource(getState()).get("url").includes("long.js"), "Selected source is long.js");
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 782:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	/*
	  Tests Pausing on exception
	  1. skip an uncaught exception
	  2. pause on an uncaught exception
	  3. pause on a caught error
	  4. skip a caught error
	*/
	var testButton = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    var dbg = yield initDebugger("doc-exceptions.html");

	    // test skipping an uncaught exception
	    yield togglePauseOnExceptions(dbg, false, false);
	    yield uncaughtException(dbg);
	    ok(!isPaused(dbg));

	    // Test pausing on an uncaught exception
	    yield togglePauseOnExceptions(dbg, true, false);
	    uncaughtException(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "exceptions.js", 2);
	    yield resume(dbg);

	    // Test pausing on a caught Error
	    yield togglePauseOnExceptions(dbg, true, false);
	    caughtException(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "exceptions.js", 15);
	    yield resume(dbg);

	    // Test skipping a caught error
	    yield togglePauseOnExceptions(dbg, true, true);
	    caughtException(dbg);
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "exceptions.js", 17);
	    yield resume(dbg);
	  });

	  return function testButton(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var testReloading = (() => {
	  var _ref2 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-exceptions.html");

	    yield togglePauseOnExceptions(dbg, true, false);
	    dbg = yield initDebugger("doc-exceptions.html");
	    assertPOEState(dbg, ctx, true, false);
	  });

	  return function testReloading(_x2) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    invokeInTab = _require.invokeInTab,
	    togglePauseOnExceptions = _require.togglePauseOnExceptions,
	    waitForPaused = _require.waitForPaused,
	    isPaused = _require.isPaused,
	    resume = _require.resume,
	    reload = _require.reload;

	function uncaughtException(dbg) {
	  return invokeInTab(dbg, "uncaughtException").catch(() => {});
	}

	function caughtException(dbg) {
	  return invokeInTab(dbg, "caughtException");
	}

	function assertPOEState(dbg, ctx, pause, ignore) {
	  var is = ctx.is;
	  var getState = dbg.getState,
	      _dbg$selectors = dbg.selectors,
	      getShouldPauseOnExceptions = _dbg$selectors.getShouldPauseOnExceptions,
	      getShouldIgnoreCaughtExceptions = _dbg$selectors.getShouldIgnoreCaughtExceptions;


	  is(getShouldPauseOnExceptions(getState()), pause);
	  is(getShouldIgnoreCaughtExceptions(getState()), ignore);
	}

	module.exports = {
	  testButton,
	  testReloading
	};

/***/ },

/***/ 783:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var prettyPrint = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is;

	    var dbg = yield initDebugger("doc-minified.html", "math.min");

	    yield selectSource(dbg, "math.min.js");

	    clickElement(dbg, "prettyPrintButton");
	    yield waitForDispatch(dbg, "TOGGLE_PRETTY_PRINT");

	    var ppSrc = findSource(dbg, "math.min.js:formatted");
	    ok(ppSrc, "Pretty-printed source exists");

	    yield addBreakpoint(dbg, ppSrc, 18);

	    invokeInTab(dbg, "arithmetic");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, ppSrc, 18);

	    yield stepOver(dbg);
	    assertPausedLocation(dbg, ctx, ppSrc, 27);
	    yield resume(dbg);

	    // The pretty-print button should go away in the pretty-printed
	    // source.
	    ok(!findElement(dbg, "editorFooter"), "Footer is hidden");

	    yield selectSource(dbg, "math.min.js");
	    ok(findElement(dbg, "editorFooter"), "Footer is hidden");
	  });

	  return function prettyPrint(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch,
	    findSource = _require.findSource,
	    findElement = _require.findElement,
	    selectSource = _require.selectSource,
	    clickElement = _require.clickElement,
	    addBreakpoint = _require.addBreakpoint,
	    stepOver = _require.stepOver,
	    invokeInTab = _require.invokeInTab,
	    resume = _require.resume;

	module.exports = prettyPrint;

/***/ },

/***/ 784:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    selectSource = _require.selectSource,
	    addBreakpoint = _require.addBreakpoint,
	    invokeInTab = _require.invokeInTab,
	    resume = _require.resume,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch,
	    clickElement = _require.clickElement;

	// Tests pretty-printing a source that is currently paused.

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-minified.html");

	    yield selectSource(dbg, "math.min.js");
	    yield addBreakpoint(dbg, "math.min.js", 2);

	    invokeInTab(dbg, "arithmetic");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "math.min.js", 2);

	    clickElement(dbg, "prettyPrintButton");
	    yield waitForDispatch(dbg, "TOGGLE_PRETTY_PRINT");

	    assertPausedLocation(dbg, ctx, "math.min.js:formatted", 18);

	    yield resume(dbg);
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 785:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var testReturnValue = (() => {
	  var _ref = _asyncToGenerator(function* (dbg, ctx, val) {
	    var is = ctx.is,
	        ok = ctx.ok;


	    evalInTab(dbg, `return_something(${val})`);
	    yield waitForPaused(dbg);

	    // "Step in" 3 times to get to the point where the debugger can
	    // see the return value.
	    yield stepIn(dbg);
	    yield stepIn(dbg);
	    yield stepIn(dbg);
	    is(getLabel(dbg, 1), "return_something", "check for return_something");

	    // We don't show "undefined" but we do show other falsy values.
	    var label = getLabel(dbg, 2);
	    if (val === "undefined") {
	      ok(label !== "<return>", "do not show <return> for undefined");
	    } else {
	      is(label, "<return>", "check for <return>");
	      is(getValue(dbg, 2), val, `check value is ${val}`);
	    }

	    yield resume(dbg);
	    assertNotPaused(dbg, ctx);
	  });

	  return function testReturnValue(_x, _x2, _x3) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var testThrowValue = (() => {
	  var _ref2 = _asyncToGenerator(function* (dbg, ctx, val) {
	    var is = ctx.is;

	    evalInTab(dbg, `throw_something(${val})`).catch(function () {});
	    yield waitForPaused(dbg);

	    // "Step in" to get to the exception.
	    yield stepIn(dbg);

	    is(getLabel(dbg, 1), "callee", "check for callee");
	    is(getLabel(dbg, 2), "<exception>", "check for <exception>");
	    is(getValue(dbg, 2), val, `check exception value is ${val}`);

	    yield resume(dbg);
	    yield waitForPaused(dbg);
	    yield resume(dbg);
	    assertNotPaused(dbg, ctx);
	  });

	  return function testThrowValue(_x4, _x5, _x6) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    evalInTab = _require.evalInTab,
	    assertNotPaused = _require.assertNotPaused,
	    findElement = _require.findElement,
	    initDebugger = _require.initDebugger,
	    togglePauseOnExceptions = _require.togglePauseOnExceptions,
	    resume = _require.resume,
	    stepIn = _require.stepIn,
	    waitForPaused = _require.waitForPaused;

	function getLabel(dbg, index) {
	  return findElement(dbg, "scopeNode", index).innerText;
	}

	function getValue(dbg, index) {
	  return findElement(dbg, "scopeValue", index).innerText;
	}

	function toggleScopes(dbg) {
	  return findElement(dbg, "scopesHeader").click();
	}

	module.exports = (() => {
	  var _ref3 = _asyncToGenerator(function* (ctx) {
	    var info = ctx.info;

	    var dbg = yield initDebugger("doc-return-values.html");
	    toggleScopes(dbg);
	    yield togglePauseOnExceptions(dbg, true, false);

	    yield testReturnValue(dbg, ctx, "57");
	    yield testThrowValue(dbg, ctx, "57");
	  });

	  return function (_x7) {
	    return _ref3.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 786:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var expandingProperties = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    var dbg = yield initDebugger("doc-script-switching.html", "script-switching-01", "script-switching-02");

	    toggleScopes(dbg);
	    invokeInTab(dbg, "firstCall");

	    yield waitForPaused(dbg);

	    is(getLabel(dbg, 1), "secondCall");
	    is(getLabel(dbg, 2), "<this>");
	    is(getLabel(dbg, 4), "foo()");

	    toggleNode(dbg, 4);
	    yield waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
	    is(getLabel(dbg, 5), "length");

	    yield stepOver(dbg);
	    is(getLabel(dbg, 4), "foo()");
	    is(getLabel(dbg, 5), "Window");
	  });

	  return function expandingProperties(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var changingScopes = (() => {
	  var _ref2 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    var dbg = yield initDebugger("doc-script-switching.html", "script-switching-01", "script-switching-02");

	    toggleScopes(dbg);
	    invokeInTab(dbg, "firstCall");

	    yield waitForPaused(dbg);

	    clickElement(dbg, "frame", 2);
	    is(getLabel(dbg, 1), "firstCall");
	    is(getLabel(dbg, 2), "<this>");

	    toggleNode(dbg, 2);
	    yield waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
	    is(getLabel(dbg, 5), "CSS2Properties()");
	  });

	  return function changingScopes(_x2) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    waitForPaused = _require.waitForPaused,
	    invokeInTab = _require.invokeInTab,
	    evalInTab = _require.evalInTab,
	    clickElement = _require.clickElement,
	    findElement = _require.findElement,
	    waitForTime = _require.waitForTime,
	    waitForDispatch = _require.waitForDispatch,
	    stepOver = _require.stepOver;

	function toggleNode(dbg, index) {
	  clickElement(dbg, "scopeNode", index);
	}

	function getLabel(dbg, index) {
	  return findElement(dbg, "scopeNode", index).innerText;
	}

	function toggleScopes(dbg) {
	  return findElement(dbg, "scopesHeader").click();
	}

	module.exports = {
	  expandingProperties,
	  changingScopes
	};

/***/ },

/***/ 787:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    waitForPaused = _require.waitForPaused,
	    pressKey = _require.pressKey,
	    findElementWithSelector = _require.findElementWithSelector,
	    type = _require.type,
	    waitForElement = _require.waitForElement,
	    invokeInTab = _require.invokeInTab,
	    clickElement = _require.clickElement,
	    findElement = _require.findElement,
	    waitForDispatch = _require.waitForDispatch;

	// Testing source search


	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    var dbg = yield initDebugger("doc-script-switching.html");

	    pressKey(dbg, "sourceSearch");
	    yield waitForElement(dbg, "input");
	    findElementWithSelector(dbg, "input").focus();
	    type(dbg, "sw");
	    pressKey(dbg, "Enter");

	    yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
	    var source = dbg.selectors.getSelectedSource(dbg.getState());
	    ok(source.get("url").match(/switching-01/), "first source is selected");

	    // 2. arrow keys and check to see if source is selected
	    pressKey(dbg, "sourceSearch");
	    findElementWithSelector(dbg, "input").focus();
	    type(dbg, "sw");
	    pressKey(dbg, "Down");
	    pressKey(dbg, "Enter");

	    yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
	    source = dbg.selectors.getSelectedSource(dbg.getState());
	    ok(source.get("url").match(/switching-02/), "second source is selected");
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 788:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var waitForSourceCount = (() => {
	  var _ref = _asyncToGenerator(function* (dbg, i) {
	    // We are forced to wait until the DOM nodes appear because the
	    // source tree batches its rendering.
	    yield waitUntil(function () {
	      return countSources(dbg) === i;
	    });
	  });

	  return function waitForSourceCount(_x, _x2) {
	    return _ref.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    environment = _require.environment,
	    clickElement = _require.clickElement,
	    invokeInTab = _require.invokeInTab,
	    pressKey = _require.pressKey,
	    selectSource = _require.selectSource,
	    evalInTab = _require.evalInTab,
	    findElement = _require.findElement,
	    findAllElements = _require.findAllElements,
	    findElementWithSelector = _require.findElementWithSelector,
	    waitForSources = _require.waitForSources,
	    waitForDispatch = _require.waitForDispatch,
	    waitUntil = _require.waitUntil,
	    waitForTime = _require.waitForTime;

	// Tests that the source tree works.

	function countSources(dbg) {
	  return findAllElements(dbg, "sourceNodes").length;
	}

	function toggleDirectory(dbg, index) {
	  return clickElement(dbg, "sourceArrow", index);
	}

	function clickNode(dbg, index) {
	  return clickElement(dbg, "sourceNode", index);
	}

	function getSourceNode(dbg, index) {
	  return findElement(dbg, "sourceNode", index).textContent;
	}

	function getFocusedNode(dbg) {
	  return findElementWithSelector(dbg, ".sources-list .focused");
	}

	module.exports = (() => {
	  var _ref2 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-sources.html");
	    var getSelectedSource = dbg.selectors.getSelectedSource,
	        getState = dbg.getState;


	    yield waitForSources(dbg, "simple1", "simple2", "nested-source", "long.js", "doc-sources.html");

	    // wait for source render to debounce
	    yield waitForTime(200);

	    // Expand nodes and make sure more sources appear.
	    is(countSources(dbg), 2);

	    toggleDirectory(dbg, 2);
	    is(countSources(dbg), 7);

	    toggleDirectory(dbg, 3);

	    is(countSources(dbg), 8);

	    // Select a source.
	    ok(!getFocusedNode(dbg), "Source is not focused");

	    var selected = waitForDispatch(dbg, "SELECT_SOURCE");
	    clickNode(dbg, 4);
	    yield selected;

	    ok(getFocusedNode(dbg), "Source is focused");

	    ok(getSelectedSource(getState()).get("url").includes("nested-source.js"), "The right source is selected");

	    // Make sure new sources appear in the list.
	    invokeInTab(dbg, "loadScript");
	    yield waitForSourceCount(dbg, 9);

	    is(getSourceNode(dbg, 7), "math.min.js", "The dynamic script exists");

	    if (environment == "mochitest") {
	      // Make sure named eval sources appear in the list.
	      evalInTab(dbg, "window.evaledFunc = function() {} //# sourceURL=evaled.js");

	      yield waitForSourceCount(dbg, 11);
	      is(findElement(dbg, "sourceNode", 2).textContent, "evaled.js", "The eval script exists");
	    }
	  });

	  return function (_x3) {
	    return _ref2.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 789:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    waitForSources = _require.waitForSources,
	    selectSource = _require.selectSource,
	    addBreakpoint = _require.addBreakpoint,
	    stepIn = _require.stepIn,
	    stepOut = _require.stepOut,
	    stepOver = _require.stepOver,
	    invokeInTab = _require.invokeInTab,
	    waitForPaused = _require.waitForPaused;

	// Tests loading sourcemapped sources, setting breakpoints, and
	// stepping in them.

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-sourcemaps.html");
	    var _dbg$selectors = dbg.selectors,
	        getBreakpoint = _dbg$selectors.getBreakpoint,
	        getBreakpoints = _dbg$selectors.getBreakpoints,
	        getState = dbg.getState;


	    yield waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
	    ok(true, "Original sources exist");
	    var entrySrc = findSource(dbg, "entry.js");

	    yield selectSource(dbg, entrySrc);
	    ok(dbg.win.cm.getValue().includes("window.keepMeAlive"), "Original source text loaded correctly");

	    // Test that breakpoint sliding is not attempted. The breakpoint
	    // should not move anywhere.
	    yield addBreakpoint(dbg, entrySrc, 13);
	    is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
	    ok(getBreakpoint(getState(), { sourceId: entrySrc.id, line: 13 }), "Breakpoint has correct line");

	    // Test breaking on a breakpoint
	    yield addBreakpoint(dbg, "entry.js", 15);
	    is(getBreakpoints(getState()).size, 2, "Two breakpoints exist");
	    ok(getBreakpoint(getState(), { sourceId: entrySrc.id, line: 15 }), "Breakpoint has correct line");

	    invokeInTab(dbg, "keepMeAlive");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, entrySrc, 15);

	    yield stepIn(dbg);
	    assertPausedLocation(dbg, ctx, "times2.js", 2);
	    yield stepOver(dbg);
	    assertPausedLocation(dbg, ctx, "times2.js", 3);

	    yield stepOut(dbg);
	    yield stepOut(dbg);
	    assertPausedLocation(dbg, ctx, "entry.js", 16);
	  });

	  function sourceMaps(_x) {
	    return _ref.apply(this, arguments);
	  }

	  return sourceMaps;
	})();

/***/ },

/***/ 790:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    waitForSources = _require.waitForSources,
	    selectSource = _require.selectSource,
	    addBreakpoint = _require.addBreakpoint,
	    invokeInTab = _require.invokeInTab,
	    waitForPaused = _require.waitForPaused;

	// Tests loading sourcemapped sources, setting breakpoints, and
	// stepping in them.

	// This source map does not have source contents, so it's fetched separately

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    var dbg = yield initDebugger("doc-sourcemaps2.html");
	    var _dbg$selectors = dbg.selectors,
	        getBreakpoint = _dbg$selectors.getBreakpoint,
	        getBreakpoints = _dbg$selectors.getBreakpoints,
	        getState = dbg.getState;


	    yield waitForSources(dbg, "main.js", "main.min.js");

	    ok(true, "Original sources exist");
	    var mainSrc = findSource(dbg, "main.js");

	    yield selectSource(dbg, mainSrc);

	    // Test that breakpoint is not off by a line.
	    yield addBreakpoint(dbg, mainSrc, 4);
	    is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
	    ok(getBreakpoint(getState(), { sourceId: mainSrc.id, line: 4 }), "Breakpoint has correct line");

	    invokeInTab(dbg, "logMessage");

	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "main.js", 4);
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 791:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    countSources = _require.countSources,
	    assertPausedLocation = _require.assertPausedLocation,
	    selectSource = _require.selectSource,
	    addBreakpoint = _require.addBreakpoint,
	    invokeInTab = _require.invokeInTab,
	    waitForPaused = _require.waitForPaused;

	// Test that an error while loading a sourcemap does not break
	// debugging.

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;


	    var dbg = yield initDebugger("doc-sourcemap-bogus.html");
	    var getSources = dbg.selectors.getSources,
	        getState = dbg.getState;


	    yield selectSource(dbg, "bogus-map.js");

	    // We should still be able to set breakpoints and pause in the
	    // generated source.
	    yield addBreakpoint(dbg, "bogus-map.js", 4);
	    invokeInTab(dbg, "runCode");
	    yield waitForPaused(dbg);
	    assertPausedLocation(dbg, ctx, "bogus-map.js", 4);

	    // Make sure that only the single generated source exists. The
	    // sourcemap failed to download.
	    is(countSources(dbg), 1, "Only 1 source exists");
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 792:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var _expect = __webpack_require__(800);

	var _expect2 = _interopRequireDefault(_expect);

	var _index = __webpack_require__(758);

	var _index2 = _interopRequireDefault(_index);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	__webpack_require__(793);

	var _require = __webpack_require__(226),
	    prefs = _require.prefs;

	Object.assign(window, { prefs }, _index2.default);

	window.ok = function ok(expected) {
	  (0, _expect2.default)(expected).to.be.truthy;
	};

	window.is = function is(expected, actual) {
	  (0, _expect2.default)(expected).to.equal(actual);
	};

	window.info = function info(msg) {
	  console.log(`info: ${msg}\n`);
	};

	window.requestLongerTimeout = function () {};

	var ctx = { ok, is, info, requestLongerTimeout };

	mocha.setup({ timeout: 10000, ui: "bdd" });

	describe("Tests", () => {
	  beforeEach(function () {
	    console.log("TEST START", this.currentTest.title);
	    prefs.pauseOnExceptions = false;
	    prefs.ignoreCaughtExceptions = false;
	    prefs.pendingSelectedLocation = {};
	    prefs.expressions = [];
	    prefs.pendingBreakpoints = [];
	    prefs.tabs = [];
	  });

	  afterEach(function () {
	    prefs.pauseOnExceptions = false;
	    prefs.ignoreCaughtExceptions = false;
	    prefs.pendingSelectedLocation = {};
	    prefs.expressions = [];
	    prefs.pendingBreakpoints = [];
	    prefs.tabs = [];

	    var err = this.currentTest.err;
	    var msg = err ? "FAILURE" : "SUCCESS";
	    console.log(`TEST ${msg}`, this.currentTest.title);
	    if (err) {
	      console.log(err.message);
	      console.log(err.stack);
	    }
	  });

	  it("asm", _asyncToGenerator(function* () {
	    return yield asm(ctx);
	  }));

	  describe("breakpoints", () => {
	    it("breakpoints - toggle", _asyncToGenerator(function* () {
	      return yield breakpoints.toggle(ctx);
	    }));

	    it("breakpoints - toggleAll", _asyncToGenerator(function* () {
	      return yield breakpoints.toggleAll(ctx);
	    }));

	    it("breaking", _asyncToGenerator(function* () {
	      return yield breaking(ctx);
	    }));

	    it("conditional breakpoints", _asyncToGenerator(function* () {
	      return yield breakpointsCond(ctx);
	    }));
	  });

	  it("expressions", _asyncToGenerator(function* () {
	    return yield expressions(ctx);
	  }));

	  describe("editor", () => {
	    it("editor select", _asyncToGenerator(function* () {
	      return yield editorSelect(ctx);
	    }));

	    it("editor gutter", _asyncToGenerator(function* () {
	      return yield editorGutter(ctx);
	    }));

	    xit("editor highlight", _asyncToGenerator(function* () {
	      return yield editorHighlight(ctx);
	    }));

	    xit("editor preview", _asyncToGenerator(function* () {
	      return yield editorPreview(ctx);
	    }));
	  });

	  xit("keyboard navigation", _asyncToGenerator(function* () {
	    return yield keyboardNavigation(ctx);
	  }));

	  xit("keyboard shortcuts", _asyncToGenerator(function* () {
	    return yield keyboardShortcuts(ctx);
	  }));

	  xit("navigation", _asyncToGenerator(function* () {
	    return yield navigation(ctx);
	  }));

	  describe("call stack", () => {
	    it("test 1", _asyncToGenerator(function* () {
	      return yield callStack.test1(ctx);
	    }));

	    it("test 2", _asyncToGenerator(function* () {
	      return yield callStack.test2(ctx);
	    }));
	  });

	  it("debugger buttons", _asyncToGenerator(function* () {
	    return yield debuggerButtons(ctx);
	  }));

	  it("iframes", _asyncToGenerator(function* () {
	    return yield iframes(ctx);
	  }));

	  it("pause on exceptions - button", _asyncToGenerator(function* () {
	    return yield pauseOnExceptions.testButton(ctx);
	  }));

	  it("pause on exceptions - reloading", _asyncToGenerator(function* () {
	    return yield pauseOnExceptions.testReloading(ctx);
	  }));

	  it("pretty print", _asyncToGenerator(function* () {
	    return yield prettyPrint(ctx);
	  }));

	  it("pretty print paused", _asyncToGenerator(function* () {
	    return yield prettyPrintPaused(ctx);
	  }));

	  it("returnvalues", _asyncToGenerator(function* () {
	    return yield returnvalues(ctx);
	  }));

	  xit("searching", _asyncToGenerator(function* () {
	    return yield searching(ctx);
	  }));

	  it("scopes - expanding properties", _asyncToGenerator(function* () {
	    return yield scopes.expandingProperties(ctx);
	  }));
	  it("scopes - changing scopes", _asyncToGenerator(function* () {
	    return yield scopes.changingScopes(ctx);
	  }));

	  it("render the expected scopes when variable mutates while stepping", _asyncToGenerator(function* () {
	    return yield scopesMutations(ctx);
	  }));

	  it("sources", _asyncToGenerator(function* () {
	    return yield sources(ctx);
	  }));

	  describe("source maps", () => {
	    it("stepping", _asyncToGenerator(function* () {
	      return yield sourceMaps(ctx);
	    }));
	    it("reloading", _asyncToGenerator(function* () {
	      return yield sourceMapsReloading(ctx);
	    }));
	    it("source maps 2", _asyncToGenerator(function* () {
	      return yield sourceMaps2(ctx);
	    }));
	    it("source maps bogus", _asyncToGenerator(function* () {
	      return yield sourceMapsBogus(ctx);
	    }));
	  });

	  describe("tabs", () => {
	    // expected 2 to equal 1
	    it("add tabs", _asyncToGenerator(function* () {
	      return yield tabs.addTabs(ctx);
	    }));
	    it("reload with tabs", _asyncToGenerator(function* () {
	      return yield tabs.reloadWithTabs(ctx);
	    }));
	    it("reload with no tabs", _asyncToGenerator(function* () {
	      return yield tabs.reloadWithNoTabs(ctx);
	    }));
	  });
	});

	mocha.run(failures => {
	  console.log("WERE DON", failures);
	});

/***/ },

/***/ 793:
/***/ function(module, exports) {

	(function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
	(function (process,global){
	'use strict';

	/* eslint no-unused-vars: off */
	/* eslint-env commonjs */

	/**
	 * Shim process.stdout.
	 */

	process.stdout = require('browser-stdout')();

	var Mocha = require('./lib/mocha');

	/**
	 * Create a Mocha instance.
	 *
	 * @return {undefined}
	 */

	var mocha = new Mocha({ reporter: 'html' });

	/**
	 * Save timer references to avoid Sinon interfering (see GH-237).
	 */

	var Date = global.Date;
	var setTimeout = global.setTimeout;
	var setInterval = global.setInterval;
	var clearTimeout = global.clearTimeout;
	var clearInterval = global.clearInterval;

	var uncaughtExceptionHandlers = [];

	var originalOnerrorHandler = global.onerror;

	/**
	 * Remove uncaughtException listener.
	 * Revert to original onerror handler if previously defined.
	 */

	process.removeListener = function (e, fn) {
	  if (e === 'uncaughtException') {
	    if (originalOnerrorHandler) {
	      global.onerror = originalOnerrorHandler;
	    } else {
	      global.onerror = function () {};
	    }
	    var i = Mocha.utils.indexOf(uncaughtExceptionHandlers, fn);
	    if (i !== -1) {
	      uncaughtExceptionHandlers.splice(i, 1);
	    }
	  }
	};

	/**
	 * Implements uncaughtException listener.
	 */

	process.on = function (e, fn) {
	  if (e === 'uncaughtException') {
	    global.onerror = function (err, url, line) {
	      fn(new Error(err + ' (' + url + ':' + line + ')'));
	      return !mocha.allowUncaught;
	    };
	    uncaughtExceptionHandlers.push(fn);
	  }
	};

	// The BDD UI is registered by default, but no UI will be functional in the
	// browser without an explicit call to the overridden `mocha.ui` (see below).
	// Ensure that this default UI does not expose its methods to the global scope.
	mocha.suite.removeAllListeners('pre-require');

	var immediateQueue = [];
	var immediateTimeout;

	function timeslice () {
	  var immediateStart = new Date().getTime();
	  while (immediateQueue.length && (new Date().getTime() - immediateStart) < 100) {
	    immediateQueue.shift()();
	  }
	  if (immediateQueue.length) {
	    immediateTimeout = setTimeout(timeslice, 0);
	  } else {
	    immediateTimeout = null;
	  }
	}

	/**
	 * High-performance override of Runner.immediately.
	 */

	Mocha.Runner.immediately = function (callback) {
	  immediateQueue.push(callback);
	  if (!immediateTimeout) {
	    immediateTimeout = setTimeout(timeslice, 0);
	  }
	};

	/**
	 * Function to allow assertion libraries to throw errors directly into mocha.
	 * This is useful when running tests in a browser because window.onerror will
	 * only receive the 'message' attribute of the Error.
	 */
	mocha.throwError = function (err) {
	  Mocha.utils.forEach(uncaughtExceptionHandlers, function (fn) {
	    fn(err);
	  });
	  throw err;
	};

	/**
	 * Override ui to ensure that the ui functions are initialized.
	 * Normally this would happen in Mocha.prototype.loadFiles.
	 */

	mocha.ui = function (ui) {
	  Mocha.prototype.ui.call(this, ui);
	  this.suite.emit('pre-require', global, null, this);
	  return this;
	};

	/**
	 * Setup mocha with the given setting options.
	 */

	mocha.setup = function (opts) {
	  if (typeof opts === 'string') {
	    opts = { ui: opts };
	  }
	  for (var opt in opts) {
	    if (opts.hasOwnProperty(opt)) {
	      this[opt](opts[opt]);
	    }
	  }
	  return this;
	};

	/**
	 * Run mocha, returning the Runner.
	 */

	mocha.run = function (fn) {
	  var options = mocha.options;
	  mocha.globals('location');

	  var query = Mocha.utils.parseQuery(global.location.search || '');
	  if (query.grep) {
	    mocha.grep(query.grep);
	  }
	  if (query.fgrep) {
	    mocha.fgrep(query.fgrep);
	  }
	  if (query.invert) {
	    mocha.invert();
	  }

	  return Mocha.prototype.run.call(mocha, function (err) {
	    // The DOM Document is not available in Web Workers.
	    var document = global.document;
	    if (document && document.getElementById('mocha') && options.noHighlighting !== true) {
	      Mocha.utils.highlightTags('code');
	    }
	    if (fn) {
	      fn(err);
	    }
	  });
	};

	/**
	 * Expose the process shim.
	 * https://github.com/mochajs/mocha/pull/916
	 */

	Mocha.process = process;

	/**
	 * Expose mocha.
	 */

	global.Mocha = Mocha;
	global.mocha = mocha;

	// this allows test/acceptance/required-tokens.js to pass; thus,
	// you can now do `const describe = require('mocha').describe` in a
	// browser context (assuming browserification).  should fix #880
	module.exports = global;

	}).call(this,require('_process'),typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"./lib/mocha":14,"_process":82,"browser-stdout":41}],2:[function(require,module,exports){
	'use strict';

	function noop () {}

	module.exports = function () {
	  return noop;
	};

	},{}],3:[function(require,module,exports){
	'use strict';

	/**
	 * Module exports.
	 */

	exports.EventEmitter = EventEmitter;

	/**
	 * Object#toString reference.
	 */
	var objToString = Object.prototype.toString;

	/**
	 * Check if a value is an array.
	 *
	 * @api private
	 * @param {*} val The value to test.
	 * @return {boolean} true if the value is an array, otherwise false.
	 */
	function isArray (val) {
	  return objToString.call(val) === '[object Array]';
	}

	/**
	 * Event emitter constructor.
	 *
	 * @api public
	 */
	function EventEmitter () {}

	/**
	 * Add a listener.
	 *
	 * @api public
	 * @param {string} name Event name.
	 * @param {Function} fn Event handler.
	 * @return {EventEmitter} Emitter instance.
	 */
	EventEmitter.prototype.on = function (name, fn) {
	  if (!this.$events) {
	    this.$events = {};
	  }

	  if (!this.$events[name]) {
	    this.$events[name] = fn;
	  } else if (isArray(this.$events[name])) {
	    this.$events[name].push(fn);
	  } else {
	    this.$events[name] = [this.$events[name], fn];
	  }

	  return this;
	};

	EventEmitter.prototype.addListener = EventEmitter.prototype.on;

	/**
	 * Adds a volatile listener.
	 *
	 * @api public
	 * @param {string} name Event name.
	 * @param {Function} fn Event handler.
	 * @return {EventEmitter} Emitter instance.
	 */
	EventEmitter.prototype.once = function (name, fn) {
	  var self = this;

	  function on () {
	    self.removeListener(name, on);
	    fn.apply(this, arguments);
	  }

	  on.listener = fn;
	  this.on(name, on);

	  return this;
	};

	/**
	 * Remove a listener.
	 *
	 * @api public
	 * @param {string} name Event name.
	 * @param {Function} fn Event handler.
	 * @return {EventEmitter} Emitter instance.
	 */
	EventEmitter.prototype.removeListener = function (name, fn) {
	  if (this.$events && this.$events[name]) {
	    var list = this.$events[name];

	    if (isArray(list)) {
	      var pos = -1;

	      for (var i = 0, l = list.length; i < l; i++) {
	        if (list[i] === fn || (list[i].listener && list[i].listener === fn)) {
	          pos = i;
	          break;
	        }
	      }

	      if (pos < 0) {
	        return this;
	      }

	      list.splice(pos, 1);

	      if (!list.length) {
	        delete this.$events[name];
	      }
	    } else if (list === fn || (list.listener && list.listener === fn)) {
	      delete this.$events[name];
	    }
	  }

	  return this;
	};

	/**
	 * Remove all listeners for an event.
	 *
	 * @api public
	 * @param {string} name Event name.
	 * @return {EventEmitter} Emitter instance.
	 */
	EventEmitter.prototype.removeAllListeners = function (name) {
	  if (name === undefined) {
	    this.$events = {};
	    return this;
	  }

	  if (this.$events && this.$events[name]) {
	    this.$events[name] = null;
	  }

	  return this;
	};

	/**
	 * Get all listeners for a given event.
	 *
	 * @api public
	 * @param {string} name Event name.
	 * @return {EventEmitter} Emitter instance.
	 */
	EventEmitter.prototype.listeners = function (name) {
	  if (!this.$events) {
	    this.$events = {};
	  }

	  if (!this.$events[name]) {
	    this.$events[name] = [];
	  }

	  if (!isArray(this.$events[name])) {
	    this.$events[name] = [this.$events[name]];
	  }

	  return this.$events[name];
	};

	/**
	 * Emit an event.
	 *
	 * @api public
	 * @param {string} name Event name.
	 * @return {boolean} true if at least one handler was invoked, else false.
	 */
	EventEmitter.prototype.emit = function (name) {
	  if (!this.$events) {
	    return false;
	  }

	  var handler = this.$events[name];

	  if (!handler) {
	    return false;
	  }

	  var args = Array.prototype.slice.call(arguments, 1);

	  if (typeof handler === 'function') {
	    handler.apply(this, args);
	  } else if (isArray(handler)) {
	    var listeners = handler.slice();

	    for (var i = 0, l = listeners.length; i < l; i++) {
	      listeners[i].apply(this, args);
	    }
	  } else {
	    return false;
	  }

	  return true;
	};

	},{}],4:[function(require,module,exports){
	'use strict';

	/**
	 * Expose `Progress`.
	 */

	module.exports = Progress;

	/**
	 * Initialize a new `Progress` indicator.
	 */
	function Progress () {
	  this.percent = 0;
	  this.size(0);
	  this.fontSize(11);
	  this.font('helvetica, arial, sans-serif');
	}

	/**
	 * Set progress size to `size`.
	 *
	 * @api public
	 * @param {number} size
	 * @return {Progress} Progress instance.
	 */
	Progress.prototype.size = function (size) {
	  this._size = size;
	  return this;
	};

	/**
	 * Set text to `text`.
	 *
	 * @api public
	 * @param {string} text
	 * @return {Progress} Progress instance.
	 */
	Progress.prototype.text = function (text) {
	  this._text = text;
	  return this;
	};

	/**
	 * Set font size to `size`.
	 *
	 * @api public
	 * @param {number} size
	 * @return {Progress} Progress instance.
	 */
	Progress.prototype.fontSize = function (size) {
	  this._fontSize = size;
	  return this;
	};

	/**
	 * Set font to `family`.
	 *
	 * @param {string} family
	 * @return {Progress} Progress instance.
	 */
	Progress.prototype.font = function (family) {
	  this._font = family;
	  return this;
	};

	/**
	 * Update percentage to `n`.
	 *
	 * @param {number} n
	 * @return {Progress} Progress instance.
	 */
	Progress.prototype.update = function (n) {
	  this.percent = n;
	  return this;
	};

	/**
	 * Draw on `ctx`.
	 *
	 * @param {CanvasRenderingContext2d} ctx
	 * @return {Progress} Progress instance.
	 */
	Progress.prototype.draw = function (ctx) {
	  try {
	    var percent = Math.min(this.percent, 100);
	    var size = this._size;
	    var half = size / 2;
	    var x = half;
	    var y = half;
	    var rad = half - 1;
	    var fontSize = this._fontSize;

	    ctx.font = fontSize + 'px ' + this._font;

	    var angle = Math.PI * 2 * (percent / 100);
	    ctx.clearRect(0, 0, size, size);

	    // outer circle
	    ctx.strokeStyle = '#9f9f9f';
	    ctx.beginPath();
	    ctx.arc(x, y, rad, 0, angle, false);
	    ctx.stroke();

	    // inner circle
	    ctx.strokeStyle = '#eee';
	    ctx.beginPath();
	    ctx.arc(x, y, rad - 1, 0, angle, true);
	    ctx.stroke();

	    // text
	    var text = this._text || (percent | 0) + '%';
	    var w = ctx.measureText(text).width;

	    ctx.fillText(text, x - w / 2 + 1, y + fontSize / 2 - 1);
	  } catch (err) {
	    // don't fail if we can't render progress
	  }
	  return this;
	};

	},{}],5:[function(require,module,exports){
	(function (global){
	'use strict';

	exports.isatty = function isatty () {
	  return true;
	};

	exports.getWindowSize = function getWindowSize () {
	  if ('innerHeight' in global) {
	    return [global.innerHeight, global.innerWidth];
	  }
	  // In a Web Worker, the DOM Window is not available.
	  return [640, 480];
	};

	}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{}],6:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var JSON = require('json3');

	/**
	 * Expose `Context`.
	 */

	module.exports = Context;

	/**
	 * Initialize a new `Context`.
	 *
	 * @api private
	 */
	function Context () {}

	/**
	 * Set or get the context `Runnable` to `runnable`.
	 *
	 * @api private
	 * @param {Runnable} runnable
	 * @return {Context}
	 */
	Context.prototype.runnable = function (runnable) {
	  if (!arguments.length) {
	    return this._runnable;
	  }
	  this.test = this._runnable = runnable;
	  return this;
	};

	/**
	 * Set test timeout `ms`.
	 *
	 * @api private
	 * @param {number} ms
	 * @return {Context} self
	 */
	Context.prototype.timeout = function (ms) {
	  if (!arguments.length) {
	    return this.runnable().timeout();
	  }
	  this.runnable().timeout(ms);
	  return this;
	};

	/**
	 * Set test timeout `enabled`.
	 *
	 * @api private
	 * @param {boolean} enabled
	 * @return {Context} self
	 */
	Context.prototype.enableTimeouts = function (enabled) {
	  this.runnable().enableTimeouts(enabled);
	  return this;
	};

	/**
	 * Set test slowness threshold `ms`.
	 *
	 * @api private
	 * @param {number} ms
	 * @return {Context} self
	 */
	Context.prototype.slow = function (ms) {
	  this.runnable().slow(ms);
	  return this;
	};

	/**
	 * Mark a test as skipped.
	 *
	 * @api private
	 * @return {Context} self
	 */
	Context.prototype.skip = function () {
	  this.runnable().skip();
	  return this;
	};

	/**
	 * Allow a number of retries on failed tests
	 *
	 * @api private
	 * @param {number} n
	 * @return {Context} self
	 */
	Context.prototype.retries = function (n) {
	  if (!arguments.length) {
	    return this.runnable().retries();
	  }
	  this.runnable().retries(n);
	  return this;
	};

	/**
	 * Inspect the context void of `._runnable`.
	 *
	 * @api private
	 * @return {string}
	 */
	Context.prototype.inspect = function () {
	  return JSON.stringify(this, function (key, val) {
	    return key === 'runnable' || key === 'test' ? undefined : val;
	  }, 2);
	};

	},{"json3":69}],7:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Runnable = require('./runnable');
	var inherits = require('./utils').inherits;

	/**
	 * Expose `Hook`.
	 */

	module.exports = Hook;

	/**
	 * Initialize a new `Hook` with the given `title` and callback `fn`.
	 *
	 * @param {String} title
	 * @param {Function} fn
	 * @api private
	 */
	function Hook (title, fn) {
	  Runnable.call(this, title, fn);
	  this.type = 'hook';
	}

	/**
	 * Inherit from `Runnable.prototype`.
	 */
	inherits(Hook, Runnable);

	/**
	 * Get or set the test `err`.
	 *
	 * @param {Error} err
	 * @return {Error}
	 * @api public
	 */
	Hook.prototype.error = function (err) {
	  if (!arguments.length) {
	    err = this._error;
	    this._error = null;
	    return err;
	  }

	  this._error = err;
	};

	},{"./runnable":33,"./utils":38}],8:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Test = require('../test');

	/**
	 * BDD-style interface:
	 *
	 *      describe('Array', function() {
	 *        describe('#indexOf()', function() {
	 *          it('should return -1 when not present', function() {
	 *            // ...
	 *          });
	 *
	 *          it('should return the index when present', function() {
	 *            // ...
	 *          });
	 *        });
	 *      });
	 *
	 * @param {Suite} suite Root suite.
	 */
	module.exports = function (suite) {
	  var suites = [suite];

	  suite.on('pre-require', function (context, file, mocha) {
	    var common = require('./common')(suites, context, mocha);

	    context.before = common.before;
	    context.after = common.after;
	    context.beforeEach = common.beforeEach;
	    context.afterEach = common.afterEach;
	    context.run = mocha.options.delay && common.runWithSuite(suite);
	    /**
	     * Describe a "suite" with the given `title`
	     * and callback `fn` containing nested suites
	     * and/or tests.
	     */

	    context.describe = context.context = function (title, fn) {
	      return common.suite.create({
	        title: title,
	        file: file,
	        fn: fn
	      });
	    };

	    /**
	     * Pending describe.
	     */

	    context.xdescribe = context.xcontext = context.describe.skip = function (title, fn) {
	      return common.suite.skip({
	        title: title,
	        file: file,
	        fn: fn
	      });
	    };

	    /**
	     * Exclusive suite.
	     */

	    context.describe.only = function (title, fn) {
	      return common.suite.only({
	        title: title,
	        file: file,
	        fn: fn
	      });
	    };

	    /**
	     * Describe a specification or test-case
	     * with the given `title` and callback `fn`
	     * acting as a thunk.
	     */

	    context.it = context.specify = function (title, fn) {
	      var suite = suites[0];
	      if (suite.isPending()) {
	        fn = null;
	      }
	      var test = new Test(title, fn);
	      test.file = file;
	      suite.addTest(test);
	      return test;
	    };

	    /**
	     * Exclusive test-case.
	     */

	    context.it.only = function (title, fn) {
	      return common.test.only(mocha, context.it(title, fn));
	    };

	    /**
	     * Pending test case.
	     */

	    context.xit = context.xspecify = context.it.skip = function (title) {
	      context.it(title);
	    };

	    /**
	     * Number of attempts to retry.
	     */
	    context.it.retries = function (n) {
	      context.retries(n);
	    };
	  });
	};

	},{"../test":36,"./common":9}],9:[function(require,module,exports){
	'use strict';

	var Suite = require('../suite');

	/**
	 * Functions common to more than one interface.
	 *
	 * @param {Suite[]} suites
	 * @param {Context} context
	 * @param {Mocha} mocha
	 * @return {Object} An object containing common functions.
	 */
	module.exports = function (suites, context, mocha) {
	  return {
	    /**
	     * This is only present if flag --delay is passed into Mocha. It triggers
	     * root suite execution.
	     *
	     * @param {Suite} suite The root suite.
	     * @return {Function} A function which runs the root suite
	     */
	    runWithSuite: function runWithSuite (suite) {
	      return function run () {
	        suite.run();
	      };
	    },

	    /**
	     * Execute before running tests.
	     *
	     * @param {string} name
	     * @param {Function} fn
	     */
	    before: function (name, fn) {
	      suites[0].beforeAll(name, fn);
	    },

	    /**
	     * Execute after running tests.
	     *
	     * @param {string} name
	     * @param {Function} fn
	     */
	    after: function (name, fn) {
	      suites[0].afterAll(name, fn);
	    },

	    /**
	     * Execute before each test case.
	     *
	     * @param {string} name
	     * @param {Function} fn
	     */
	    beforeEach: function (name, fn) {
	      suites[0].beforeEach(name, fn);
	    },

	    /**
	     * Execute after each test case.
	     *
	     * @param {string} name
	     * @param {Function} fn
	     */
	    afterEach: function (name, fn) {
	      suites[0].afterEach(name, fn);
	    },

	    suite: {
	      /**
	       * Create an exclusive Suite; convenience function
	       * See docstring for create() below.
	       *
	       * @param {Object} opts
	       * @returns {Suite}
	       */
	      only: function only (opts) {
	        mocha.options.hasOnly = true;
	        opts.isOnly = true;
	        return this.create(opts);
	      },

	      /**
	       * Create a Suite, but skip it; convenience function
	       * See docstring for create() below.
	       *
	       * @param {Object} opts
	       * @returns {Suite}
	       */
	      skip: function skip (opts) {
	        opts.pending = true;
	        return this.create(opts);
	      },

	      /**
	       * Creates a suite.
	       * @param {Object} opts Options
	       * @param {string} opts.title Title of Suite
	       * @param {Function} [opts.fn] Suite Function (not always applicable)
	       * @param {boolean} [opts.pending] Is Suite pending?
	       * @param {string} [opts.file] Filepath where this Suite resides
	       * @param {boolean} [opts.isOnly] Is Suite exclusive?
	       * @returns {Suite}
	       */
	      create: function create (opts) {
	        var suite = Suite.create(suites[0], opts.title);
	        suite.pending = Boolean(opts.pending);
	        suite.file = opts.file;
	        suites.unshift(suite);
	        if (opts.isOnly) {
	          suite.parent._onlySuites = suite.parent._onlySuites.concat(suite);
	          mocha.options.hasOnly = true;
	        }
	        if (typeof opts.fn === 'function') {
	          opts.fn.call(suite);
	          suites.shift();
	        } else if (typeof opts.fn === 'undefined' && !suite.pending) {
	          throw new Error('Suite "' + suite.fullTitle() + '" was defined but no callback was supplied. Supply a callback or explicitly skip the suite.');
	        }

	        return suite;
	      }
	    },

	    test: {

	      /**
	       * Exclusive test-case.
	       *
	       * @param {Object} mocha
	       * @param {Function} test
	       * @returns {*}
	       */
	      only: function (mocha, test) {
	        test.parent._onlyTests = test.parent._onlyTests.concat(test);
	        mocha.options.hasOnly = true;
	        return test;
	      },

	      /**
	       * Pending test case.
	       *
	       * @param {string} title
	       */
	      skip: function (title) {
	        context.test(title);
	      },

	      /**
	       * Number of retry attempts
	       *
	       * @param {number} n
	       */
	      retries: function (n) {
	        context.retries(n);
	      }
	    }
	  };
	};

	},{"../suite":35}],10:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Suite = require('../suite');
	var Test = require('../test');

	/**
	 * Exports-style (as Node.js module) interface:
	 *
	 *     exports.Array = {
	 *       '#indexOf()': {
	 *         'should return -1 when the value is not present': function() {
	 *
	 *         },
	 *
	 *         'should return the correct index when the value is present': function() {
	 *
	 *         }
	 *       }
	 *     };
	 *
	 * @param {Suite} suite Root suite.
	 */
	module.exports = function (suite) {
	  var suites = [suite];

	  suite.on('require', visit);

	  function visit (obj, file) {
	    var suite;
	    for (var key in obj) {
	      if (typeof obj[key] === 'function') {
	        var fn = obj[key];
	        switch (key) {
	          case 'before':
	            suites[0].beforeAll(fn);
	            break;
	          case 'after':
	            suites[0].afterAll(fn);
	            break;
	          case 'beforeEach':
	            suites[0].beforeEach(fn);
	            break;
	          case 'afterEach':
	            suites[0].afterEach(fn);
	            break;
	          default:
	            var test = new Test(key, fn);
	            test.file = file;
	            suites[0].addTest(test);
	        }
	      } else {
	        suite = Suite.create(suites[0], key);
	        suites.unshift(suite);
	        visit(obj[key], file);
	        suites.shift();
	      }
	    }
	  }
	};

	},{"../suite":35,"../test":36}],11:[function(require,module,exports){
	'use strict';

	exports.bdd = require('./bdd');
	exports.tdd = require('./tdd');
	exports.qunit = require('./qunit');
	exports.exports = require('./exports');

	},{"./bdd":8,"./exports":10,"./qunit":12,"./tdd":13}],12:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Test = require('../test');

	/**
	 * QUnit-style interface:
	 *
	 *     suite('Array');
	 *
	 *     test('#length', function() {
	 *       var arr = [1,2,3];
	 *       ok(arr.length == 3);
	 *     });
	 *
	 *     test('#indexOf()', function() {
	 *       var arr = [1,2,3];
	 *       ok(arr.indexOf(1) == 0);
	 *       ok(arr.indexOf(2) == 1);
	 *       ok(arr.indexOf(3) == 2);
	 *     });
	 *
	 *     suite('String');
	 *
	 *     test('#length', function() {
	 *       ok('foo'.length == 3);
	 *     });
	 *
	 * @param {Suite} suite Root suite.
	 */
	module.exports = function (suite) {
	  var suites = [suite];

	  suite.on('pre-require', function (context, file, mocha) {
	    var common = require('./common')(suites, context, mocha);

	    context.before = common.before;
	    context.after = common.after;
	    context.beforeEach = common.beforeEach;
	    context.afterEach = common.afterEach;
	    context.run = mocha.options.delay && common.runWithSuite(suite);
	    /**
	     * Describe a "suite" with the given `title`.
	     */

	    context.suite = function (title) {
	      if (suites.length > 1) {
	        suites.shift();
	      }
	      return common.suite.create({
	        title: title,
	        file: file,
	        fn: false
	      });
	    };

	    /**
	     * Exclusive Suite.
	     */

	    context.suite.only = function (title) {
	      if (suites.length > 1) {
	        suites.shift();
	      }
	      return common.suite.only({
	        title: title,
	        file: file,
	        fn: false
	      });
	    };

	    /**
	     * Describe a specification or test-case
	     * with the given `title` and callback `fn`
	     * acting as a thunk.
	     */

	    context.test = function (title, fn) {
	      var test = new Test(title, fn);
	      test.file = file;
	      suites[0].addTest(test);
	      return test;
	    };

	    /**
	     * Exclusive test-case.
	     */

	    context.test.only = function (title, fn) {
	      return common.test.only(mocha, context.test(title, fn));
	    };

	    context.test.skip = common.test.skip;
	    context.test.retries = common.test.retries;
	  });
	};

	},{"../test":36,"./common":9}],13:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Test = require('../test');

	/**
	 * TDD-style interface:
	 *
	 *      suite('Array', function() {
	 *        suite('#indexOf()', function() {
	 *          suiteSetup(function() {
	 *
	 *          });
	 *
	 *          test('should return -1 when not present', function() {
	 *
	 *          });
	 *
	 *          test('should return the index when present', function() {
	 *
	 *          });
	 *
	 *          suiteTeardown(function() {
	 *
	 *          });
	 *        });
	 *      });
	 *
	 * @param {Suite} suite Root suite.
	 */
	module.exports = function (suite) {
	  var suites = [suite];

	  suite.on('pre-require', function (context, file, mocha) {
	    var common = require('./common')(suites, context, mocha);

	    context.setup = common.beforeEach;
	    context.teardown = common.afterEach;
	    context.suiteSetup = common.before;
	    context.suiteTeardown = common.after;
	    context.run = mocha.options.delay && common.runWithSuite(suite);

	    /**
	     * Describe a "suite" with the given `title` and callback `fn` containing
	     * nested suites and/or tests.
	     */
	    context.suite = function (title, fn) {
	      return common.suite.create({
	        title: title,
	        file: file,
	        fn: fn
	      });
	    };

	    /**
	     * Pending suite.
	     */
	    context.suite.skip = function (title, fn) {
	      return common.suite.skip({
	        title: title,
	        file: file,
	        fn: fn
	      });
	    };

	    /**
	     * Exclusive test-case.
	     */
	    context.suite.only = function (title, fn) {
	      return common.suite.only({
	        title: title,
	        file: file,
	        fn: fn
	      });
	    };

	    /**
	     * Describe a specification or test-case with the given `title` and
	     * callback `fn` acting as a thunk.
	     */
	    context.test = function (title, fn) {
	      var suite = suites[0];
	      if (suite.isPending()) {
	        fn = null;
	      }
	      var test = new Test(title, fn);
	      test.file = file;
	      suite.addTest(test);
	      return test;
	    };

	    /**
	     * Exclusive test-case.
	     */

	    context.test.only = function (title, fn) {
	      return common.test.only(mocha, context.test(title, fn));
	    };

	    context.test.skip = common.test.skip;
	    context.test.retries = common.test.retries;
	  });
	};

	},{"../test":36,"./common":9}],14:[function(require,module,exports){
	(function (process,global,__dirname){
	'use strict';

	/*!
	 * mocha
	 * Copyright(c) 2011 TJ Holowaychuk <tj@vision-media.ca>
	 * MIT Licensed
	 */

	/**
	 * Module dependencies.
	 */

	var escapeRe = require('escape-string-regexp');
	var path = require('path');
	var reporters = require('./reporters');
	var utils = require('./utils');

	/**
	 * Expose `Mocha`.
	 */

	exports = module.exports = Mocha;

	/**
	 * To require local UIs and reporters when running in node.
	 */

	if (!process.browser) {
	  var cwd = process.cwd();
	  module.paths.push(cwd, path.join(cwd, 'node_modules'));
	}

	/**
	 * Expose internals.
	 */

	exports.utils = utils;
	exports.interfaces = require('./interfaces');
	exports.reporters = reporters;
	exports.Runnable = require('./runnable');
	exports.Context = require('./context');
	exports.Runner = require('./runner');
	exports.Suite = require('./suite');
	exports.Hook = require('./hook');
	exports.Test = require('./test');

	/**
	 * Return image `name` path.
	 *
	 * @api private
	 * @param {string} name
	 * @return {string}
	 */
	function image (name) {
	  return path.join(__dirname, '../images', name + '.png');
	}

	/**
	 * Set up mocha with `options`.
	 *
	 * Options:
	 *
	 *   - `ui` name "bdd", "tdd", "exports" etc
	 *   - `reporter` reporter instance, defaults to `mocha.reporters.spec`
	 *   - `globals` array of accepted globals
	 *   - `timeout` timeout in milliseconds
	 *   - `retries` number of times to retry failed tests
	 *   - `bail` bail on the first test failure
	 *   - `slow` milliseconds to wait before considering a test slow
	 *   - `ignoreLeaks` ignore global leaks
	 *   - `fullTrace` display the full stack-trace on failing
	 *   - `grep` string or regexp to filter tests with
	 *
	 * @param {Object} options
	 * @api public
	 */
	function Mocha (options) {
	  options = options || {};
	  this.files = [];
	  this.options = options;
	  if (options.grep) {
	    this.grep(new RegExp(options.grep));
	  }
	  if (options.fgrep) {
	    this.fgrep(options.fgrep);
	  }
	  this.suite = new exports.Suite('', new exports.Context());
	  this.ui(options.ui);
	  this.bail(options.bail);
	  this.reporter(options.reporter, options.reporterOptions);
	  if (typeof options.timeout !== 'undefined' && options.timeout !== null) {
	    this.timeout(options.timeout);
	  }
	  if (typeof options.retries !== 'undefined' && options.retries !== null) {
	    this.retries(options.retries);
	  }
	  this.useColors(options.useColors);
	  if (options.enableTimeouts !== null) {
	    this.enableTimeouts(options.enableTimeouts);
	  }
	  if (options.slow) {
	    this.slow(options.slow);
	  }
	}

	/**
	 * Enable or disable bailing on the first failure.
	 *
	 * @api public
	 * @param {boolean} [bail]
	 */
	Mocha.prototype.bail = function (bail) {
	  if (!arguments.length) {
	    bail = true;
	  }
	  this.suite.bail(bail);
	  return this;
	};

	/**
	 * Add test `file`.
	 *
	 * @api public
	 * @param {string} file
	 */
	Mocha.prototype.addFile = function (file) {
	  this.files.push(file);
	  return this;
	};

	/**
	 * Set reporter to `reporter`, defaults to "spec".
	 *
	 * @param {String|Function} reporter name or constructor
	 * @param {Object} reporterOptions optional options
	 * @api public
	 * @param {string|Function} reporter name or constructor
	 * @param {Object} reporterOptions optional options
	 */
	Mocha.prototype.reporter = function (reporter, reporterOptions) {
	  if (typeof reporter === 'function') {
	    this._reporter = reporter;
	  } else {
	    reporter = reporter || 'spec';
	    var _reporter;
	    // Try to load a built-in reporter.
	    if (reporters[reporter]) {
	      _reporter = reporters[reporter];
	    }
	    // Try to load reporters from process.cwd() and node_modules
	    if (!_reporter) {
	      try {
	        _reporter = require(reporter);
	      } catch (err) {
	        if (err.message.indexOf('Cannot find module') !== -1) {
	          // Try to load reporters from a path (absolute or relative)
	          try {
	            _reporter = require(path.resolve(process.cwd(), reporter));
	          } catch (_err) {
	            err.message.indexOf('Cannot find module') !== -1 ? console.warn('"' + reporter + '" reporter not found')
	              : console.warn('"' + reporter + '" reporter blew up with error:\n' + err.stack);
	          }
	        } else {
	          console.warn('"' + reporter + '" reporter blew up with error:\n' + err.stack);
	        }
	      }
	    }
	    if (!_reporter && reporter === 'teamcity') {
	      console.warn('The Teamcity reporter was moved to a package named ' +
	        'mocha-teamcity-reporter ' +
	        '(https://npmjs.org/package/mocha-teamcity-reporter).');
	    }
	    if (!_reporter) {
	      throw new Error('invalid reporter "' + reporter + '"');
	    }
	    this._reporter = _reporter;
	  }
	  this.options.reporterOptions = reporterOptions;
	  return this;
	};

	/**
	 * Set test UI `name`, defaults to "bdd".
	 *
	 * @api public
	 * @param {string} bdd
	 */
	Mocha.prototype.ui = function (name) {
	  name = name || 'bdd';
	  this._ui = exports.interfaces[name];
	  if (!this._ui) {
	    try {
	      this._ui = require(name);
	    } catch (err) {
	      throw new Error('invalid interface "' + name + '"');
	    }
	  }
	  this._ui = this._ui(this.suite);

	  this.suite.on('pre-require', function (context) {
	    exports.afterEach = context.afterEach || context.teardown;
	    exports.after = context.after || context.suiteTeardown;
	    exports.beforeEach = context.beforeEach || context.setup;
	    exports.before = context.before || context.suiteSetup;
	    exports.describe = context.describe || context.suite;
	    exports.it = context.it || context.test;
	    exports.setup = context.setup || context.beforeEach;
	    exports.suiteSetup = context.suiteSetup || context.before;
	    exports.suiteTeardown = context.suiteTeardown || context.after;
	    exports.suite = context.suite || context.describe;
	    exports.teardown = context.teardown || context.afterEach;
	    exports.test = context.test || context.it;
	    exports.run = context.run;
	  });

	  return this;
	};

	/**
	 * Load registered files.
	 *
	 * @api private
	 */
	Mocha.prototype.loadFiles = function (fn) {
	  var self = this;
	  var suite = this.suite;
	  this.files.forEach(function (file) {
	    file = path.resolve(file);
	    suite.emit('pre-require', global, file, self);
	    suite.emit('require', require(file), file, self);
	    suite.emit('post-require', global, file, self);
	  });
	  fn && fn();
	};

	/**
	 * Enable growl support.
	 *
	 * @api private
	 */
	Mocha.prototype._growl = function (runner, reporter) {
	  var notify = require('growl');

	  runner.on('end', function () {
	    var stats = reporter.stats;
	    if (stats.failures) {
	      var msg = stats.failures + ' of ' + runner.total + ' tests failed';
	      notify(msg, { name: 'mocha', title: 'Failed', image: image('error') });
	    } else {
	      notify(stats.passes + ' tests passed in ' + stats.duration + 'ms', {
	        name: 'mocha',
	        title: 'Passed',
	        image: image('ok')
	      });
	    }
	  });
	};

	/**
	 * Escape string and add it to grep as a regexp.
	 *
	 * @api public
	 * @param str
	 * @returns {Mocha}
	 */
	Mocha.prototype.fgrep = function (str) {
	  return this.grep(new RegExp(escapeRe(str)));
	};

	/**
	 * Add regexp to grep, if `re` is a string it is escaped.
	 *
	 * @param {RegExp|String} re
	 * @return {Mocha}
	 * @api public
	 * @param {RegExp|string} re
	 * @return {Mocha}
	 */
	Mocha.prototype.grep = function (re) {
	  if (utils.isString(re)) {
	    // extract args if it's regex-like, i.e: [string, pattern, flag]
	    var arg = re.match(/^\/(.*)\/(g|i|)$|.*/);
	    this.options.grep = new RegExp(arg[1] || arg[0], arg[2]);
	  } else {
	    this.options.grep = re;
	  }
	  return this;
	};
	/**
	 * Invert `.grep()` matches.
	 *
	 * @return {Mocha}
	 * @api public
	 */
	Mocha.prototype.invert = function () {
	  this.options.invert = true;
	  return this;
	};

	/**
	 * Ignore global leaks.
	 *
	 * @param {Boolean} ignore
	 * @return {Mocha}
	 * @api public
	 * @param {boolean} ignore
	 * @return {Mocha}
	 */
	Mocha.prototype.ignoreLeaks = function (ignore) {
	  this.options.ignoreLeaks = Boolean(ignore);
	  return this;
	};

	/**
	 * Enable global leak checking.
	 *
	 * @return {Mocha}
	 * @api public
	 */
	Mocha.prototype.checkLeaks = function () {
	  this.options.ignoreLeaks = false;
	  return this;
	};

	/**
	 * Display long stack-trace on failing
	 *
	 * @return {Mocha}
	 * @api public
	 */
	Mocha.prototype.fullTrace = function () {
	  this.options.fullStackTrace = true;
	  return this;
	};

	/**
	 * Enable growl support.
	 *
	 * @return {Mocha}
	 * @api public
	 */
	Mocha.prototype.growl = function () {
	  this.options.growl = true;
	  return this;
	};

	/**
	 * Ignore `globals` array or string.
	 *
	 * @param {Array|String} globals
	 * @return {Mocha}
	 * @api public
	 * @param {Array|string} globals
	 * @return {Mocha}
	 */
	Mocha.prototype.globals = function (globals) {
	  this.options.globals = (this.options.globals || []).concat(globals);
	  return this;
	};

	/**
	 * Emit color output.
	 *
	 * @param {Boolean} colors
	 * @return {Mocha}
	 * @api public
	 * @param {boolean} colors
	 * @return {Mocha}
	 */
	Mocha.prototype.useColors = function (colors) {
	  if (colors !== undefined) {
	    this.options.useColors = colors;
	  }
	  return this;
	};

	/**
	 * Use inline diffs rather than +/-.
	 *
	 * @param {Boolean} inlineDiffs
	 * @return {Mocha}
	 * @api public
	 * @param {boolean} inlineDiffs
	 * @return {Mocha}
	 */
	Mocha.prototype.useInlineDiffs = function (inlineDiffs) {
	  this.options.useInlineDiffs = inlineDiffs !== undefined && inlineDiffs;
	  return this;
	};

	/**
	 * Set the timeout in milliseconds.
	 *
	 * @param {Number} timeout
	 * @return {Mocha}
	 * @api public
	 * @param {number} timeout
	 * @return {Mocha}
	 */
	Mocha.prototype.timeout = function (timeout) {
	  this.suite.timeout(timeout);
	  return this;
	};

	/**
	 * Set the number of times to retry failed tests.
	 *
	 * @param {Number} retry times
	 * @return {Mocha}
	 * @api public
	 */
	Mocha.prototype.retries = function (n) {
	  this.suite.retries(n);
	  return this;
	};

	/**
	 * Set slowness threshold in milliseconds.
	 *
	 * @param {Number} slow
	 * @return {Mocha}
	 * @api public
	 * @param {number} slow
	 * @return {Mocha}
	 */
	Mocha.prototype.slow = function (slow) {
	  this.suite.slow(slow);
	  return this;
	};

	/**
	 * Enable timeouts.
	 *
	 * @param {Boolean} enabled
	 * @return {Mocha}
	 * @api public
	 * @param {boolean} enabled
	 * @return {Mocha}
	 */
	Mocha.prototype.enableTimeouts = function (enabled) {
	  this.suite.enableTimeouts(arguments.length && enabled !== undefined ? enabled : true);
	  return this;
	};

	/**
	 * Makes all tests async (accepting a callback)
	 *
	 * @return {Mocha}
	 * @api public
	 */
	Mocha.prototype.asyncOnly = function () {
	  this.options.asyncOnly = true;
	  return this;
	};

	/**
	 * Disable syntax highlighting (in browser).
	 *
	 * @api public
	 */
	Mocha.prototype.noHighlighting = function () {
	  this.options.noHighlighting = true;
	  return this;
	};

	/**
	 * Enable uncaught errors to propagate (in browser).
	 *
	 * @return {Mocha}
	 * @api public
	 */
	Mocha.prototype.allowUncaught = function () {
	  this.options.allowUncaught = true;
	  return this;
	};

	/**
	 * Delay root suite execution.
	 * @returns {Mocha}
	 */
	Mocha.prototype.delay = function delay () {
	  this.options.delay = true;
	  return this;
	};

	/**
	 * Run tests and invoke `fn()` when complete.
	 *
	 * @api public
	 * @param {Function} fn
	 * @return {Runner}
	 */
	Mocha.prototype.run = function (fn) {
	  if (this.files.length) {
	    this.loadFiles();
	  }
	  var suite = this.suite;
	  var options = this.options;
	  options.files = this.files;
	  var runner = new exports.Runner(suite, options.delay);
	  var reporter = new this._reporter(runner, options);
	  runner.ignoreLeaks = options.ignoreLeaks !== false;
	  runner.fullStackTrace = options.fullStackTrace;
	  runner.hasOnly = options.hasOnly;
	  runner.asyncOnly = options.asyncOnly;
	  runner.allowUncaught = options.allowUncaught;
	  if (options.grep) {
	    runner.grep(options.grep, options.invert);
	  }
	  if (options.globals) {
	    runner.globals(options.globals);
	  }
	  if (options.growl) {
	    this._growl(runner, reporter);
	  }
	  if (options.useColors !== undefined) {
	    exports.reporters.Base.useColors = options.useColors;
	  }
	  exports.reporters.Base.inlineDiffs = options.useInlineDiffs;

	  function done (failures) {
	    if (reporter.done) {
	      reporter.done(failures, fn);
	    } else {
	      fn && fn(failures);
	    }
	  }

	  return runner.run(done);
	};

	}).call(this,require('_process'),typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {},"/lib")
	},{"./context":6,"./hook":7,"./interfaces":11,"./reporters":21,"./runnable":33,"./runner":34,"./suite":35,"./test":36,"./utils":38,"_process":82,"escape-string-regexp":62,"growl":64,"path":42}],15:[function(require,module,exports){
	'use strict';

	/**
	 * Helpers.
	 */

	var s = 1000;
	var m = s * 60;
	var h = m * 60;
	var d = h * 24;
	var y = d * 365.25;

	/**
	 * Parse or format the given `val`.
	 *
	 * Options:
	 *
	 *  - `long` verbose formatting [false]
	 *
	 * @api public
	 * @param {string|number} val
	 * @param {Object} options
	 * @return {string|number}
	 */
	module.exports = function (val, options) {
	  options = options || {};
	  if (typeof val === 'string') {
	    return parse(val);
	  }
	  // https://github.com/mochajs/mocha/pull/1035
	  return options['long'] ? longFormat(val) : shortFormat(val);
	};

	/**
	 * Parse the given `str` and return milliseconds.
	 *
	 * @api private
	 * @param {string} str
	 * @return {number}
	 */
	function parse (str) {
	  var match = (/^((?:\d+)?\.?\d+) *(ms|seconds?|s|minutes?|m|hours?|h|days?|d|years?|y)?$/i).exec(str);
	  if (!match) {
	    return;
	  }
	  var n = parseFloat(match[1]);
	  var type = (match[2] || 'ms').toLowerCase();
	  switch (type) {
	    case 'years':
	    case 'year':
	    case 'y':
	      return n * y;
	    case 'days':
	    case 'day':
	    case 'd':
	      return n * d;
	    case 'hours':
	    case 'hour':
	    case 'h':
	      return n * h;
	    case 'minutes':
	    case 'minute':
	    case 'm':
	      return n * m;
	    case 'seconds':
	    case 'second':
	    case 's':
	      return n * s;
	    case 'ms':
	      return n;
	    default:
	      // No default case
	  }
	}

	/**
	 * Short format for `ms`.
	 *
	 * @api private
	 * @param {number} ms
	 * @return {string}
	 */
	function shortFormat (ms) {
	  if (ms >= d) {
	    return Math.round(ms / d) + 'd';
	  }
	  if (ms >= h) {
	    return Math.round(ms / h) + 'h';
	  }
	  if (ms >= m) {
	    return Math.round(ms / m) + 'm';
	  }
	  if (ms >= s) {
	    return Math.round(ms / s) + 's';
	  }
	  return ms + 'ms';
	}

	/**
	 * Long format for `ms`.
	 *
	 * @api private
	 * @param {number} ms
	 * @return {string}
	 */
	function longFormat (ms) {
	  return plural(ms, d, 'day') ||
	    plural(ms, h, 'hour') ||
	    plural(ms, m, 'minute') ||
	    plural(ms, s, 'second') ||
	    ms + ' ms';
	}

	/**
	 * Pluralization helper.
	 *
	 * @api private
	 * @param {number} ms
	 * @param {number} n
	 * @param {string} name
	 */
	function plural (ms, n, name) {
	  if (ms < n) {
	    return;
	  }
	  if (ms < n * 1.5) {
	    return Math.floor(ms / n) + ' ' + name;
	  }
	  return Math.ceil(ms / n) + ' ' + name + 's';
	}

	},{}],16:[function(require,module,exports){
	'use strict';

	/**
	 * Expose `Pending`.
	 */

	module.exports = Pending;

	/**
	 * Initialize a new `Pending` error with the given message.
	 *
	 * @param {string} message
	 */
	function Pending (message) {
	  this.message = message;
	}

	},{}],17:[function(require,module,exports){
	(function (process,global){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var tty = require('tty');
	var diff = require('diff');
	var ms = require('../ms');
	var utils = require('../utils');
	var supportsColor = process.browser ? null : require('supports-color');

	/**
	 * Expose `Base`.
	 */

	exports = module.exports = Base;

	/**
	 * Save timer references to avoid Sinon interfering.
	 * See: https://github.com/mochajs/mocha/issues/237
	 */

	/* eslint-disable no-unused-vars, no-native-reassign */
	var Date = global.Date;
	var setTimeout = global.setTimeout;
	var setInterval = global.setInterval;
	var clearTimeout = global.clearTimeout;
	var clearInterval = global.clearInterval;
	/* eslint-enable no-unused-vars, no-native-reassign */

	/**
	 * Check if both stdio streams are associated with a tty.
	 */

	var isatty = tty.isatty(1) && tty.isatty(2);

	/**
	 * Enable coloring by default, except in the browser interface.
	 */

	exports.useColors = !process.browser && (supportsColor || (process.env.MOCHA_COLORS !== undefined));

	/**
	 * Inline diffs instead of +/-
	 */

	exports.inlineDiffs = false;

	/**
	 * Default color map.
	 */

	exports.colors = {
	  pass: 90,
	  fail: 31,
	  'bright pass': 92,
	  'bright fail': 91,
	  'bright yellow': 93,
	  pending: 36,
	  suite: 0,
	  'error title': 0,
	  'error message': 31,
	  'error stack': 90,
	  checkmark: 32,
	  fast: 90,
	  medium: 33,
	  slow: 31,
	  green: 32,
	  light: 90,
	  'diff gutter': 90,
	  'diff added': 32,
	  'diff removed': 31
	};

	/**
	 * Default symbol map.
	 */

	exports.symbols = {
	  ok: '✓',
	  err: '✖',
	  dot: '․',
	  comma: ',',
	  bang: '!'
	};

	// With node.js on Windows: use symbols available in terminal default fonts
	if (process.platform === 'win32') {
	  exports.symbols.ok = '\u221A';
	  exports.symbols.err = '\u00D7';
	  exports.symbols.dot = '.';
	}

	/**
	 * Color `str` with the given `type`,
	 * allowing colors to be disabled,
	 * as well as user-defined color
	 * schemes.
	 *
	 * @param {string} type
	 * @param {string} str
	 * @return {string}
	 * @api private
	 */
	var color = exports.color = function (type, str) {
	  if (!exports.useColors) {
	    return String(str);
	  }
	  return '\u001b[' + exports.colors[type] + 'm' + str + '\u001b[0m';
	};

	/**
	 * Expose term window size, with some defaults for when stderr is not a tty.
	 */

	exports.window = {
	  width: 75
	};

	if (isatty) {
	  exports.window.width = process.stdout.getWindowSize
	      ? process.stdout.getWindowSize(1)[0]
	      : tty.getWindowSize()[1];
	}

	/**
	 * Expose some basic cursor interactions that are common among reporters.
	 */

	exports.cursor = {
	  hide: function () {
	    isatty && process.stdout.write('\u001b[?25l');
	  },

	  show: function () {
	    isatty && process.stdout.write('\u001b[?25h');
	  },

	  deleteLine: function () {
	    isatty && process.stdout.write('\u001b[2K');
	  },

	  beginningOfLine: function () {
	    isatty && process.stdout.write('\u001b[0G');
	  },

	  CR: function () {
	    if (isatty) {
	      exports.cursor.deleteLine();
	      exports.cursor.beginningOfLine();
	    } else {
	      process.stdout.write('\r');
	    }
	  }
	};

	/**
	 * Outut the given `failures` as a list.
	 *
	 * @param {Array} failures
	 * @api public
	 */

	exports.list = function (failures) {
	  console.log();
	  failures.forEach(function (test, i) {
	    // format
	    var fmt = color('error title', '  %s) %s:\n') +
	      color('error message', '     %s') +
	      color('error stack', '\n%s\n');

	    // msg
	    var msg;
	    var err = test.err;
	    var message;
	    if (err.message && typeof err.message.toString === 'function') {
	      message = err.message + '';
	    } else if (typeof err.inspect === 'function') {
	      message = err.inspect() + '';
	    } else {
	      message = '';
	    }
	    var stack = err.stack || message;
	    var index = message ? stack.indexOf(message) : -1;
	    var actual = err.actual;
	    var expected = err.expected;
	    var escape = true;

	    if (index === -1) {
	      msg = message;
	    } else {
	      index += message.length;
	      msg = stack.slice(0, index);
	      // remove msg from stack
	      stack = stack.slice(index + 1);
	    }

	    // uncaught
	    if (err.uncaught) {
	      msg = 'Uncaught ' + msg;
	    }
	    // explicitly show diff
	    if (err.showDiff !== false && sameType(actual, expected) && expected !== undefined) {
	      escape = false;
	      if (!(utils.isString(actual) && utils.isString(expected))) {
	        err.actual = actual = utils.stringify(actual);
	        err.expected = expected = utils.stringify(expected);
	      }

	      fmt = color('error title', '  %s) %s:\n%s') + color('error stack', '\n%s\n');
	      var match = message.match(/^([^:]+): expected/);
	      msg = '\n      ' + color('error message', match ? match[1] : msg);

	      if (exports.inlineDiffs) {
	        msg += inlineDiff(err, escape);
	      } else {
	        msg += unifiedDiff(err, escape);
	      }
	    }

	    // indent stack trace
	    stack = stack.replace(/^/gm, '  ');

	    console.log(fmt, (i + 1), test.fullTitle(), msg, stack);
	  });
	};

	/**
	 * Initialize a new `Base` reporter.
	 *
	 * All other reporters generally
	 * inherit from this reporter, providing
	 * stats such as test duration, number
	 * of tests passed / failed etc.
	 *
	 * @param {Runner} runner
	 * @api public
	 */

	function Base (runner) {
	  var stats = this.stats = { suites: 0, tests: 0, passes: 0, pending: 0, failures: 0 };
	  var failures = this.failures = [];

	  if (!runner) {
	    return;
	  }
	  this.runner = runner;

	  runner.stats = stats;

	  runner.on('start', function () {
	    stats.start = new Date();
	  });

	  runner.on('suite', function (suite) {
	    stats.suites = stats.suites || 0;
	    suite.root || stats.suites++;
	  });

	  runner.on('test end', function () {
	    stats.tests = stats.tests || 0;
	    stats.tests++;
	  });

	  runner.on('pass', function (test) {
	    stats.passes = stats.passes || 0;

	    if (test.duration > test.slow()) {
	      test.speed = 'slow';
	    } else if (test.duration > test.slow() / 2) {
	      test.speed = 'medium';
	    } else {
	      test.speed = 'fast';
	    }

	    stats.passes++;
	  });

	  runner.on('fail', function (test, err) {
	    stats.failures = stats.failures || 0;
	    stats.failures++;
	    test.err = err;
	    failures.push(test);
	  });

	  runner.on('end', function () {
	    stats.end = new Date();
	    stats.duration = new Date() - stats.start;
	  });

	  runner.on('pending', function () {
	    stats.pending++;
	  });
	}

	/**
	 * Output common epilogue used by many of
	 * the bundled reporters.
	 *
	 * @api public
	 */
	Base.prototype.epilogue = function () {
	  var stats = this.stats;
	  var fmt;

	  console.log();

	  // passes
	  fmt = color('bright pass', ' ') +
	    color('green', ' %d passing') +
	    color('light', ' (%s)');

	  console.log(fmt,
	    stats.passes || 0,
	    ms(stats.duration));

	  // pending
	  if (stats.pending) {
	    fmt = color('pending', ' ') +
	      color('pending', ' %d pending');

	    console.log(fmt, stats.pending);
	  }

	  // failures
	  if (stats.failures) {
	    fmt = color('fail', '  %d failing');

	    console.log(fmt, stats.failures);

	    Base.list(this.failures);
	    console.log();
	  }

	  console.log();
	};

	/**
	 * Pad the given `str` to `len`.
	 *
	 * @api private
	 * @param {string} str
	 * @param {string} len
	 * @return {string}
	 */
	function pad (str, len) {
	  str = String(str);
	  return Array(len - str.length + 1).join(' ') + str;
	}

	/**
	 * Returns an inline diff between 2 strings with coloured ANSI output
	 *
	 * @api private
	 * @param {Error} err with actual/expected
	 * @param {boolean} escape
	 * @return {string} Diff
	 */
	function inlineDiff (err, escape) {
	  var msg = errorDiff(err, 'WordsWithSpace', escape);

	  // linenos
	  var lines = msg.split('\n');
	  if (lines.length > 4) {
	    var width = String(lines.length).length;
	    msg = lines.map(function (str, i) {
	      return pad(++i, width) + ' |' + ' ' + str;
	    }).join('\n');
	  }

	  // legend
	  msg = '\n' +
	    color('diff removed', 'actual') +
	    ' ' +
	    color('diff added', 'expected') +
	    '\n\n' +
	    msg +
	    '\n';

	  // indent
	  msg = msg.replace(/^/gm, '      ');
	  return msg;
	}

	/**
	 * Returns a unified diff between two strings.
	 *
	 * @api private
	 * @param {Error} err with actual/expected
	 * @param {boolean} escape
	 * @return {string} The diff.
	 */
	function unifiedDiff (err, escape) {
	  var indent = '      ';
	  function cleanUp (line) {
	    if (escape) {
	      line = escapeInvisibles(line);
	    }
	    if (line[0] === '+') {
	      return indent + colorLines('diff added', line);
	    }
	    if (line[0] === '-') {
	      return indent + colorLines('diff removed', line);
	    }
	    if (line.match(/@@/)) {
	      return null;
	    }
	    if (line.match(/\\ No newline/)) {
	      return null;
	    }
	    return indent + line;
	  }
	  function notBlank (line) {
	    return typeof line !== 'undefined' && line !== null;
	  }
	  var msg = diff.createPatch('string', err.actual, err.expected);
	  var lines = msg.split('\n').splice(4);
	  return '\n      ' +
	    colorLines('diff added', '+ expected') + ' ' +
	    colorLines('diff removed', '- actual') +
	    '\n\n' +
	    lines.map(cleanUp).filter(notBlank).join('\n');
	}

	/**
	 * Return a character diff for `err`.
	 *
	 * @api private
	 * @param {Error} err
	 * @param {string} type
	 * @param {boolean} escape
	 * @return {string}
	 */
	function errorDiff (err, type, escape) {
	  var actual = escape ? escapeInvisibles(err.actual) : err.actual;
	  var expected = escape ? escapeInvisibles(err.expected) : err.expected;
	  return diff['diff' + type](actual, expected).map(function (str) {
	    if (str.added) {
	      return colorLines('diff added', str.value);
	    }
	    if (str.removed) {
	      return colorLines('diff removed', str.value);
	    }
	    return str.value;
	  }).join('');
	}

	/**
	 * Returns a string with all invisible characters in plain text
	 *
	 * @api private
	 * @param {string} line
	 * @return {string}
	 */
	function escapeInvisibles (line) {
	  return line.replace(/\t/g, '<tab>')
	    .replace(/\r/g, '<CR>')
	    .replace(/\n/g, '<LF>\n');
	}

	/**
	 * Color lines for `str`, using the color `name`.
	 *
	 * @api private
	 * @param {string} name
	 * @param {string} str
	 * @return {string}
	 */
	function colorLines (name, str) {
	  return str.split('\n').map(function (str) {
	    return color(name, str);
	  }).join('\n');
	}

	/**
	 * Object#toString reference.
	 */
	var objToString = Object.prototype.toString;

	/**
	 * Check that a / b have the same type.
	 *
	 * @api private
	 * @param {Object} a
	 * @param {Object} b
	 * @return {boolean}
	 */
	function sameType (a, b) {
	  return objToString.call(a) === objToString.call(b);
	}

	}).call(this,require('_process'),typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"../ms":15,"../utils":38,"_process":82,"diff":56,"supports-color":42,"tty":5}],18:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var utils = require('../utils');

	/**
	 * Expose `Doc`.
	 */

	exports = module.exports = Doc;

	/**
	 * Initialize a new `Doc` reporter.
	 *
	 * @param {Runner} runner
	 * @api public
	 */
	function Doc (runner) {
	  Base.call(this, runner);

	  var indents = 2;

	  function indent () {
	    return Array(indents).join('  ');
	  }

	  runner.on('suite', function (suite) {
	    if (suite.root) {
	      return;
	    }
	    ++indents;
	    console.log('%s<section class="suite">', indent());
	    ++indents;
	    console.log('%s<h1>%s</h1>', indent(), utils.escape(suite.title));
	    console.log('%s<dl>', indent());
	  });

	  runner.on('suite end', function (suite) {
	    if (suite.root) {
	      return;
	    }
	    console.log('%s</dl>', indent());
	    --indents;
	    console.log('%s</section>', indent());
	    --indents;
	  });

	  runner.on('pass', function (test) {
	    console.log('%s  <dt>%s</dt>', indent(), utils.escape(test.title));
	    var code = utils.escape(utils.clean(test.body));
	    console.log('%s  <dd><pre><code>%s</code></pre></dd>', indent(), code);
	  });

	  runner.on('fail', function (test, err) {
	    console.log('%s  <dt class="error">%s</dt>', indent(), utils.escape(test.title));
	    var code = utils.escape(utils.clean(test.body));
	    console.log('%s  <dd class="error"><pre><code>%s</code></pre></dd>', indent(), code);
	    console.log('%s  <dd class="error">%s</dd>', indent(), utils.escape(err));
	  });
	}

	},{"../utils":38,"./base":17}],19:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var inherits = require('../utils').inherits;
	var color = Base.color;

	/**
	 * Expose `Dot`.
	 */

	exports = module.exports = Dot;

	/**
	 * Initialize a new `Dot` matrix test reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function Dot (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var width = Base.window.width * 0.75 | 0;
	  var n = -1;

	  runner.on('start', function () {
	    process.stdout.write('\n');
	  });

	  runner.on('pending', function () {
	    if (++n % width === 0) {
	      process.stdout.write('\n  ');
	    }
	    process.stdout.write(color('pending', Base.symbols.comma));
	  });

	  runner.on('pass', function (test) {
	    if (++n % width === 0) {
	      process.stdout.write('\n  ');
	    }
	    if (test.speed === 'slow') {
	      process.stdout.write(color('bright yellow', Base.symbols.dot));
	    } else {
	      process.stdout.write(color(test.speed, Base.symbols.dot));
	    }
	  });

	  runner.on('fail', function () {
	    if (++n % width === 0) {
	      process.stdout.write('\n  ');
	    }
	    process.stdout.write(color('fail', Base.symbols.bang));
	  });

	  runner.on('end', function () {
	    console.log();
	    self.epilogue();
	  });
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(Dot, Base);

	}).call(this,require('_process'))
	},{"../utils":38,"./base":17,"_process":82}],20:[function(require,module,exports){
	(function (global){
	'use strict';

	/* eslint-env browser */

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var utils = require('../utils');
	var Progress = require('../browser/progress');
	var escapeRe = require('escape-string-regexp');
	var escape = utils.escape;

	/**
	 * Save timer references to avoid Sinon interfering (see GH-237).
	 */

	/* eslint-disable no-unused-vars, no-native-reassign */
	var Date = global.Date;
	var setTimeout = global.setTimeout;
	var setInterval = global.setInterval;
	var clearTimeout = global.clearTimeout;
	var clearInterval = global.clearInterval;
	/* eslint-enable no-unused-vars, no-native-reassign */

	/**
	 * Expose `HTML`.
	 */

	exports = module.exports = HTML;

	/**
	 * Stats template.
	 */

	var statsTemplate = '<ul id="mocha-stats">' +
	  '<li class="progress"><canvas width="40" height="40"></canvas></li>' +
	  '<li class="passes"><a href="javascript:void(0);">passes:</a> <em>0</em></li>' +
	  '<li class="failures"><a href="javascript:void(0);">failures:</a> <em>0</em></li>' +
	  '<li class="duration">duration: <em>0</em>s</li>' +
	  '</ul>';

	var playIcon = '&#x2023;';

	/**
	 * Initialize a new `HTML` reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function HTML (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var stats = this.stats;
	  var stat = fragment(statsTemplate);
	  var items = stat.getElementsByTagName('li');
	  var passes = items[1].getElementsByTagName('em')[0];
	  var passesLink = items[1].getElementsByTagName('a')[0];
	  var failures = items[2].getElementsByTagName('em')[0];
	  var failuresLink = items[2].getElementsByTagName('a')[0];
	  var duration = items[3].getElementsByTagName('em')[0];
	  var canvas = stat.getElementsByTagName('canvas')[0];
	  var report = fragment('<ul id="mocha-report"></ul>');
	  var stack = [report];
	  var progress;
	  var ctx;
	  var root = document.getElementById('mocha');

	  if (canvas.getContext) {
	    var ratio = window.devicePixelRatio || 1;
	    canvas.style.width = canvas.width;
	    canvas.style.height = canvas.height;
	    canvas.width *= ratio;
	    canvas.height *= ratio;
	    ctx = canvas.getContext('2d');
	    ctx.scale(ratio, ratio);
	    progress = new Progress();
	  }

	  if (!root) {
	    return error('#mocha div missing, add it to your document');
	  }

	  // pass toggle
	  on(passesLink, 'click', function (evt) {
	    evt.preventDefault();
	    unhide();
	    var name = (/pass/).test(report.className) ? '' : ' pass';
	    report.className = report.className.replace(/fail|pass/g, '') + name;
	    if (report.className.trim()) {
	      hideSuitesWithout('test pass');
	    }
	  });

	  // failure toggle
	  on(failuresLink, 'click', function (evt) {
	    evt.preventDefault();
	    unhide();
	    var name = (/fail/).test(report.className) ? '' : ' fail';
	    report.className = report.className.replace(/fail|pass/g, '') + name;
	    if (report.className.trim()) {
	      hideSuitesWithout('test fail');
	    }
	  });

	  root.appendChild(stat);
	  root.appendChild(report);

	  if (progress) {
	    progress.size(40);
	  }

	  runner.on('suite', function (suite) {
	    if (suite.root) {
	      return;
	    }

	    // suite
	    var url = self.suiteURL(suite);
	    var el = fragment('<li class="suite"><h1><a href="%s">%s</a></h1></li>', url, escape(suite.title));

	    // container
	    stack[0].appendChild(el);
	    stack.unshift(document.createElement('ul'));
	    el.appendChild(stack[0]);
	  });

	  runner.on('suite end', function (suite) {
	    if (suite.root) {
	      updateStats();
	      return;
	    }
	    stack.shift();
	  });

	  runner.on('pass', function (test) {
	    var url = self.testURL(test);
	    var markup = '<li class="test pass %e"><h2>%e<span class="duration">%ems</span> ' +
	      '<a href="%s" class="replay">' + playIcon + '</a></h2></li>';
	    var el = fragment(markup, test.speed, test.title, test.duration, url);
	    self.addCodeToggle(el, test.body);
	    appendToStack(el);
	    updateStats();
	  });

	  runner.on('fail', function (test) {
	    var el = fragment('<li class="test fail"><h2>%e <a href="%e" class="replay">' + playIcon + '</a></h2></li>',
	      test.title, self.testURL(test));
	    var stackString; // Note: Includes leading newline
	    var message = test.err.toString();

	    // <=IE7 stringifies to [Object Error]. Since it can be overloaded, we
	    // check for the result of the stringifying.
	    if (message === '[object Error]') {
	      message = test.err.message;
	    }

	    if (test.err.stack) {
	      var indexOfMessage = test.err.stack.indexOf(test.err.message);
	      if (indexOfMessage === -1) {
	        stackString = test.err.stack;
	      } else {
	        stackString = test.err.stack.substr(test.err.message.length + indexOfMessage);
	      }
	    } else if (test.err.sourceURL && test.err.line !== undefined) {
	      // Safari doesn't give you a stack. Let's at least provide a source line.
	      stackString = '\n(' + test.err.sourceURL + ':' + test.err.line + ')';
	    }

	    stackString = stackString || '';

	    if (test.err.htmlMessage && stackString) {
	      el.appendChild(fragment('<div class="html-error">%s\n<pre class="error">%e</pre></div>',
	        test.err.htmlMessage, stackString));
	    } else if (test.err.htmlMessage) {
	      el.appendChild(fragment('<div class="html-error">%s</div>', test.err.htmlMessage));
	    } else {
	      el.appendChild(fragment('<pre class="error">%e%e</pre>', message, stackString));
	    }

	    self.addCodeToggle(el, test.body);
	    appendToStack(el);
	    updateStats();
	  });

	  runner.on('pending', function (test) {
	    var el = fragment('<li class="test pass pending"><h2>%e</h2></li>', test.title);
	    appendToStack(el);
	    updateStats();
	  });

	  function appendToStack (el) {
	    // Don't call .appendChild if #mocha-report was already .shift()'ed off the stack.
	    if (stack[0]) {
	      stack[0].appendChild(el);
	    }
	  }

	  function updateStats () {
	    // TODO: add to stats
	    var percent = stats.tests / runner.total * 100 | 0;
	    if (progress) {
	      progress.update(percent).draw(ctx);
	    }

	    // update stats
	    var ms = new Date() - stats.start;
	    text(passes, stats.passes);
	    text(failures, stats.failures);
	    text(duration, (ms / 1000).toFixed(2));
	  }
	}

	/**
	 * Makes a URL, preserving querystring ("search") parameters.
	 *
	 * @param {string} s
	 * @return {string} A new URL.
	 */
	function makeUrl (s) {
	  var search = window.location.search;

	  // Remove previous grep query parameter if present
	  if (search) {
	    search = search.replace(/[?&]grep=[^&\s]*/g, '').replace(/^&/, '?');
	  }

	  return window.location.pathname + (search ? search + '&' : '?') + 'grep=' + encodeURIComponent(escapeRe(s));
	}

	/**
	 * Provide suite URL.
	 *
	 * @param {Object} [suite]
	 */
	HTML.prototype.suiteURL = function (suite) {
	  return makeUrl(suite.fullTitle());
	};

	/**
	 * Provide test URL.
	 *
	 * @param {Object} [test]
	 */
	HTML.prototype.testURL = function (test) {
	  return makeUrl(test.fullTitle());
	};

	/**
	 * Adds code toggle functionality for the provided test's list element.
	 *
	 * @param {HTMLLIElement} el
	 * @param {string} contents
	 */
	HTML.prototype.addCodeToggle = function (el, contents) {
	  var h2 = el.getElementsByTagName('h2')[0];

	  on(h2, 'click', function () {
	    pre.style.display = pre.style.display === 'none' ? 'block' : 'none';
	  });

	  var pre = fragment('<pre><code>%e</code></pre>', utils.clean(contents));
	  el.appendChild(pre);
	  pre.style.display = 'none';
	};

	/**
	 * Display error `msg`.
	 *
	 * @param {string} msg
	 */
	function error (msg) {
	  document.body.appendChild(fragment('<div id="mocha-error">%s</div>', msg));
	}

	/**
	 * Return a DOM fragment from `html`.
	 *
	 * @param {string} html
	 */
	function fragment (html) {
	  var args = arguments;
	  var div = document.createElement('div');
	  var i = 1;

	  div.innerHTML = html.replace(/%([se])/g, function (_, type) {
	    switch (type) {
	      case 's': return String(args[i++]);
	      case 'e': return escape(args[i++]);
	      // no default
	    }
	  });

	  return div.firstChild;
	}

	/**
	 * Check for suites that do not have elements
	 * with `classname`, and hide them.
	 *
	 * @param {text} classname
	 */
	function hideSuitesWithout (classname) {
	  var suites = document.getElementsByClassName('suite');
	  for (var i = 0; i < suites.length; i++) {
	    var els = suites[i].getElementsByClassName(classname);
	    if (!els.length) {
	      suites[i].className += ' hidden';
	    }
	  }
	}

	/**
	 * Unhide .hidden suites.
	 */
	function unhide () {
	  var els = document.getElementsByClassName('suite hidden');
	  for (var i = 0; i < els.length; ++i) {
	    els[i].className = els[i].className.replace('suite hidden', 'suite');
	  }
	}

	/**
	 * Set an element's text contents.
	 *
	 * @param {HTMLElement} el
	 * @param {string} contents
	 */
	function text (el, contents) {
	  if (el.textContent) {
	    el.textContent = contents;
	  } else {
	    el.innerText = contents;
	  }
	}

	/**
	 * Listen on `event` with callback `fn`.
	 */
	function on (el, event, fn) {
	  if (el.addEventListener) {
	    el.addEventListener(event, fn, false);
	  } else {
	    el.attachEvent('on' + event, fn);
	  }
	}

	}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"../browser/progress":4,"../utils":38,"./base":17,"escape-string-regexp":62}],21:[function(require,module,exports){
	'use strict';

	// Alias exports to a their normalized format Mocha#reporter to prevent a need
	// for dynamic (try/catch) requires, which Browserify doesn't handle.
	exports.Base = exports.base = require('./base');
	exports.Dot = exports.dot = require('./dot');
	exports.Doc = exports.doc = require('./doc');
	exports.TAP = exports.tap = require('./tap');
	exports.JSON = exports.json = require('./json');
	exports.HTML = exports.html = require('./html');
	exports.List = exports.list = require('./list');
	exports.Min = exports.min = require('./min');
	exports.Spec = exports.spec = require('./spec');
	exports.Nyan = exports.nyan = require('./nyan');
	exports.XUnit = exports.xunit = require('./xunit');
	exports.Markdown = exports.markdown = require('./markdown');
	exports.Progress = exports.progress = require('./progress');
	exports.Landing = exports.landing = require('./landing');
	exports.JSONStream = exports['json-stream'] = require('./json-stream');

	},{"./base":17,"./doc":18,"./dot":19,"./html":20,"./json":23,"./json-stream":22,"./landing":24,"./list":25,"./markdown":26,"./min":27,"./nyan":28,"./progress":29,"./spec":30,"./tap":31,"./xunit":32}],22:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var JSON = require('json3');

	/**
	 * Expose `List`.
	 */

	exports = module.exports = List;

	/**
	 * Initialize a new `List` test reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function List (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var total = runner.total;

	  runner.on('start', function () {
	    console.log(JSON.stringify(['start', { total: total }]));
	  });

	  runner.on('pass', function (test) {
	    console.log(JSON.stringify(['pass', clean(test)]));
	  });

	  runner.on('fail', function (test, err) {
	    test = clean(test);
	    test.err = err.message;
	    test.stack = err.stack || null;
	    console.log(JSON.stringify(['fail', test]));
	  });

	  runner.on('end', function () {
	    process.stdout.write(JSON.stringify(['end', self.stats]));
	  });
	}

	/**
	 * Return a plain-object representation of `test`
	 * free of cyclic properties etc.
	 *
	 * @api private
	 * @param {Object} test
	 * @return {Object}
	 */
	function clean (test) {
	  return {
	    title: test.title,
	    fullTitle: test.fullTitle(),
	    duration: test.duration,
	    currentRetry: test.currentRetry()
	  };
	}

	}).call(this,require('_process'))
	},{"./base":17,"_process":82,"json3":69}],23:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');

	/**
	 * Expose `JSON`.
	 */

	exports = module.exports = JSONReporter;

	/**
	 * Initialize a new `JSON` reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function JSONReporter (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var tests = [];
	  var pending = [];
	  var failures = [];
	  var passes = [];

	  runner.on('test end', function (test) {
	    tests.push(test);
	  });

	  runner.on('pass', function (test) {
	    passes.push(test);
	  });

	  runner.on('fail', function (test) {
	    failures.push(test);
	  });

	  runner.on('pending', function (test) {
	    pending.push(test);
	  });

	  runner.on('end', function () {
	    var obj = {
	      stats: self.stats,
	      tests: tests.map(clean),
	      pending: pending.map(clean),
	      failures: failures.map(clean),
	      passes: passes.map(clean)
	    };

	    runner.testResults = obj;

	    process.stdout.write(JSON.stringify(obj, null, 2));
	  });
	}

	/**
	 * Return a plain-object representation of `test`
	 * free of cyclic properties etc.
	 *
	 * @api private
	 * @param {Object} test
	 * @return {Object}
	 */
	function clean (test) {
	  return {
	    title: test.title,
	    fullTitle: test.fullTitle(),
	    duration: test.duration,
	    currentRetry: test.currentRetry(),
	    err: errorJSON(test.err || {})
	  };
	}

	/**
	 * Transform `error` into a JSON object.
	 *
	 * @api private
	 * @param {Error} err
	 * @return {Object}
	 */
	function errorJSON (err) {
	  var res = {};
	  Object.getOwnPropertyNames(err).forEach(function (key) {
	    res[key] = err[key];
	  }, err);
	  return res;
	}

	}).call(this,require('_process'))
	},{"./base":17,"_process":82}],24:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var inherits = require('../utils').inherits;
	var cursor = Base.cursor;
	var color = Base.color;

	/**
	 * Expose `Landing`.
	 */

	exports = module.exports = Landing;

	/**
	 * Airplane color.
	 */

	Base.colors.plane = 0;

	/**
	 * Airplane crash color.
	 */

	Base.colors['plane crash'] = 31;

	/**
	 * Runway color.
	 */

	Base.colors.runway = 90;

	/**
	 * Initialize a new `Landing` reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function Landing (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var width = Base.window.width * 0.75 | 0;
	  var total = runner.total;
	  var stream = process.stdout;
	  var plane = color('plane', '✈');
	  var crashed = -1;
	  var n = 0;

	  function runway () {
	    var buf = Array(width).join('-');
	    return '  ' + color('runway', buf);
	  }

	  runner.on('start', function () {
	    stream.write('\n\n\n  ');
	    cursor.hide();
	  });

	  runner.on('test end', function (test) {
	    // check if the plane crashed
	    var col = crashed === -1 ? width * ++n / total | 0 : crashed;

	    // show the crash
	    if (test.state === 'failed') {
	      plane = color('plane crash', '✈');
	      crashed = col;
	    }

	    // render landing strip
	    stream.write('\u001b[' + (width + 1) + 'D\u001b[2A');
	    stream.write(runway());
	    stream.write('\n  ');
	    stream.write(color('runway', Array(col).join('⋅')));
	    stream.write(plane);
	    stream.write(color('runway', Array(width - col).join('⋅') + '\n'));
	    stream.write(runway());
	    stream.write('\u001b[0m');
	  });

	  runner.on('end', function () {
	    cursor.show();
	    console.log();
	    self.epilogue();
	  });
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(Landing, Base);

	}).call(this,require('_process'))
	},{"../utils":38,"./base":17,"_process":82}],25:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var inherits = require('../utils').inherits;
	var color = Base.color;
	var cursor = Base.cursor;

	/**
	 * Expose `List`.
	 */

	exports = module.exports = List;

	/**
	 * Initialize a new `List` test reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function List (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var n = 0;

	  runner.on('start', function () {
	    console.log();
	  });

	  runner.on('test', function (test) {
	    process.stdout.write(color('pass', '    ' + test.fullTitle() + ': '));
	  });

	  runner.on('pending', function (test) {
	    var fmt = color('checkmark', '  -') +
	      color('pending', ' %s');
	    console.log(fmt, test.fullTitle());
	  });

	  runner.on('pass', function (test) {
	    var fmt = color('checkmark', '  ' + Base.symbols.ok) +
	      color('pass', ' %s: ') +
	      color(test.speed, '%dms');
	    cursor.CR();
	    console.log(fmt, test.fullTitle(), test.duration);
	  });

	  runner.on('fail', function (test) {
	    cursor.CR();
	    console.log(color('fail', '  %d) %s'), ++n, test.fullTitle());
	  });

	  runner.on('end', self.epilogue.bind(self));
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(List, Base);

	}).call(this,require('_process'))
	},{"../utils":38,"./base":17,"_process":82}],26:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var utils = require('../utils');

	/**
	 * Constants
	 */

	var SUITE_PREFIX = '$';

	/**
	 * Expose `Markdown`.
	 */

	exports = module.exports = Markdown;

	/**
	 * Initialize a new `Markdown` reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function Markdown (runner) {
	  Base.call(this, runner);

	  var level = 0;
	  var buf = '';

	  function title (str) {
	    return Array(level).join('#') + ' ' + str;
	  }

	  function mapTOC (suite, obj) {
	    var ret = obj;
	    var key = SUITE_PREFIX + suite.title;

	    obj = obj[key] = obj[key] || { suite: suite };
	    suite.suites.forEach(function (suite) {
	      mapTOC(suite, obj);
	    });

	    return ret;
	  }

	  function stringifyTOC (obj, level) {
	    ++level;
	    var buf = '';
	    var link;
	    for (var key in obj) {
	      if (key === 'suite') {
	        continue;
	      }
	      if (key !== SUITE_PREFIX) {
	        link = ' - [' + key.substring(1) + ']';
	        link += '(#' + utils.slug(obj[key].suite.fullTitle()) + ')\n';
	        buf += Array(level).join('  ') + link;
	      }
	      buf += stringifyTOC(obj[key], level);
	    }
	    return buf;
	  }

	  function generateTOC (suite) {
	    var obj = mapTOC(suite, {});
	    return stringifyTOC(obj, 0);
	  }

	  generateTOC(runner.suite);

	  runner.on('suite', function (suite) {
	    ++level;
	    var slug = utils.slug(suite.fullTitle());
	    buf += '<a name="' + slug + '"></a>' + '\n';
	    buf += title(suite.title) + '\n';
	  });

	  runner.on('suite end', function () {
	    --level;
	  });

	  runner.on('pass', function (test) {
	    var code = utils.clean(test.body);
	    buf += test.title + '.\n';
	    buf += '\n```js\n';
	    buf += code + '\n';
	    buf += '```\n\n';
	  });

	  runner.on('end', function () {
	    process.stdout.write('# TOC\n');
	    process.stdout.write(generateTOC(runner.suite));
	    process.stdout.write(buf);
	  });
	}

	}).call(this,require('_process'))
	},{"../utils":38,"./base":17,"_process":82}],27:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var inherits = require('../utils').inherits;

	/**
	 * Expose `Min`.
	 */

	exports = module.exports = Min;

	/**
	 * Initialize a new `Min` minimal test reporter (best used with --watch).
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function Min (runner) {
	  Base.call(this, runner);

	  runner.on('start', function () {
	    // clear screen
	    process.stdout.write('\u001b[2J');
	    // set cursor position
	    process.stdout.write('\u001b[1;3H');
	  });

	  runner.on('end', this.epilogue.bind(this));
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(Min, Base);

	}).call(this,require('_process'))
	},{"../utils":38,"./base":17,"_process":82}],28:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var inherits = require('../utils').inherits;

	/**
	 * Expose `Dot`.
	 */

	exports = module.exports = NyanCat;

	/**
	 * Initialize a new `Dot` matrix test reporter.
	 *
	 * @param {Runner} runner
	 * @api public
	 */

	function NyanCat (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var width = Base.window.width * 0.75 | 0;
	  var nyanCatWidth = this.nyanCatWidth = 11;

	  this.colorIndex = 0;
	  this.numberOfLines = 4;
	  this.rainbowColors = self.generateColors();
	  this.scoreboardWidth = 5;
	  this.tick = 0;
	  this.trajectories = [[], [], [], []];
	  this.trajectoryWidthMax = (width - nyanCatWidth);

	  runner.on('start', function () {
	    Base.cursor.hide();
	    self.draw();
	  });

	  runner.on('pending', function () {
	    self.draw();
	  });

	  runner.on('pass', function () {
	    self.draw();
	  });

	  runner.on('fail', function () {
	    self.draw();
	  });

	  runner.on('end', function () {
	    Base.cursor.show();
	    for (var i = 0; i < self.numberOfLines; i++) {
	      write('\n');
	    }
	    self.epilogue();
	  });
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(NyanCat, Base);

	/**
	 * Draw the nyan cat
	 *
	 * @api private
	 */

	NyanCat.prototype.draw = function () {
	  this.appendRainbow();
	  this.drawScoreboard();
	  this.drawRainbow();
	  this.drawNyanCat();
	  this.tick = !this.tick;
	};

	/**
	 * Draw the "scoreboard" showing the number
	 * of passes, failures and pending tests.
	 *
	 * @api private
	 */

	NyanCat.prototype.drawScoreboard = function () {
	  var stats = this.stats;

	  function draw (type, n) {
	    write(' ');
	    write(Base.color(type, n));
	    write('\n');
	  }

	  draw('green', stats.passes);
	  draw('fail', stats.failures);
	  draw('pending', stats.pending);
	  write('\n');

	  this.cursorUp(this.numberOfLines);
	};

	/**
	 * Append the rainbow.
	 *
	 * @api private
	 */

	NyanCat.prototype.appendRainbow = function () {
	  var segment = this.tick ? '_' : '-';
	  var rainbowified = this.rainbowify(segment);

	  for (var index = 0; index < this.numberOfLines; index++) {
	    var trajectory = this.trajectories[index];
	    if (trajectory.length >= this.trajectoryWidthMax) {
	      trajectory.shift();
	    }
	    trajectory.push(rainbowified);
	  }
	};

	/**
	 * Draw the rainbow.
	 *
	 * @api private
	 */

	NyanCat.prototype.drawRainbow = function () {
	  var self = this;

	  this.trajectories.forEach(function (line) {
	    write('\u001b[' + self.scoreboardWidth + 'C');
	    write(line.join(''));
	    write('\n');
	  });

	  this.cursorUp(this.numberOfLines);
	};

	/**
	 * Draw the nyan cat
	 *
	 * @api private
	 */
	NyanCat.prototype.drawNyanCat = function () {
	  var self = this;
	  var startWidth = this.scoreboardWidth + this.trajectories[0].length;
	  var dist = '\u001b[' + startWidth + 'C';
	  var padding = '';

	  write(dist);
	  write('_,------,');
	  write('\n');

	  write(dist);
	  padding = self.tick ? '  ' : '   ';
	  write('_|' + padding + '/\\_/\\ ');
	  write('\n');

	  write(dist);
	  padding = self.tick ? '_' : '__';
	  var tail = self.tick ? '~' : '^';
	  write(tail + '|' + padding + this.face() + ' ');
	  write('\n');

	  write(dist);
	  padding = self.tick ? ' ' : '  ';
	  write(padding + '""  "" ');
	  write('\n');

	  this.cursorUp(this.numberOfLines);
	};

	/**
	 * Draw nyan cat face.
	 *
	 * @api private
	 * @return {string}
	 */

	NyanCat.prototype.face = function () {
	  var stats = this.stats;
	  if (stats.failures) {
	    return '( x .x)';
	  } else if (stats.pending) {
	    return '( o .o)';
	  } else if (stats.passes) {
	    return '( ^ .^)';
	  }
	  return '( - .-)';
	};

	/**
	 * Move cursor up `n`.
	 *
	 * @api private
	 * @param {number} n
	 */

	NyanCat.prototype.cursorUp = function (n) {
	  write('\u001b[' + n + 'A');
	};

	/**
	 * Move cursor down `n`.
	 *
	 * @api private
	 * @param {number} n
	 */

	NyanCat.prototype.cursorDown = function (n) {
	  write('\u001b[' + n + 'B');
	};

	/**
	 * Generate rainbow colors.
	 *
	 * @api private
	 * @return {Array}
	 */
	NyanCat.prototype.generateColors = function () {
	  var colors = [];

	  for (var i = 0; i < (6 * 7); i++) {
	    var pi3 = Math.floor(Math.PI / 3);
	    var n = (i * (1.0 / 6));
	    var r = Math.floor(3 * Math.sin(n) + 3);
	    var g = Math.floor(3 * Math.sin(n + 2 * pi3) + 3);
	    var b = Math.floor(3 * Math.sin(n + 4 * pi3) + 3);
	    colors.push(36 * r + 6 * g + b + 16);
	  }

	  return colors;
	};

	/**
	 * Apply rainbow to the given `str`.
	 *
	 * @api private
	 * @param {string} str
	 * @return {string}
	 */
	NyanCat.prototype.rainbowify = function (str) {
	  if (!Base.useColors) {
	    return str;
	  }
	  var color = this.rainbowColors[this.colorIndex % this.rainbowColors.length];
	  this.colorIndex += 1;
	  return '\u001b[38;5;' + color + 'm' + str + '\u001b[0m';
	};

	/**
	 * Stdout helper.
	 *
	 * @param {string} string A message to write to stdout.
	 */
	function write (string) {
	  process.stdout.write(string);
	}

	}).call(this,require('_process'))
	},{"../utils":38,"./base":17,"_process":82}],29:[function(require,module,exports){
	(function (process){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var inherits = require('../utils').inherits;
	var color = Base.color;
	var cursor = Base.cursor;

	/**
	 * Expose `Progress`.
	 */

	exports = module.exports = Progress;

	/**
	 * General progress bar color.
	 */

	Base.colors.progress = 90;

	/**
	 * Initialize a new `Progress` bar test reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 * @param {Object} options
	 */
	function Progress (runner, options) {
	  Base.call(this, runner);

	  var self = this;
	  var width = Base.window.width * 0.50 | 0;
	  var total = runner.total;
	  var complete = 0;
	  var lastN = -1;

	  // default chars
	  options = options || {};
	  options.open = options.open || '[';
	  options.complete = options.complete || '▬';
	  options.incomplete = options.incomplete || Base.symbols.dot;
	  options.close = options.close || ']';
	  options.verbose = false;

	  // tests started
	  runner.on('start', function () {
	    console.log();
	    cursor.hide();
	  });

	  // tests complete
	  runner.on('test end', function () {
	    complete++;

	    var percent = complete / total;
	    var n = width * percent | 0;
	    var i = width - n;

	    if (n === lastN && !options.verbose) {
	      // Don't re-render the line if it hasn't changed
	      return;
	    }
	    lastN = n;

	    cursor.CR();
	    process.stdout.write('\u001b[J');
	    process.stdout.write(color('progress', '  ' + options.open));
	    process.stdout.write(Array(n).join(options.complete));
	    process.stdout.write(Array(i).join(options.incomplete));
	    process.stdout.write(color('progress', options.close));
	    if (options.verbose) {
	      process.stdout.write(color('progress', ' ' + complete + ' of ' + total));
	    }
	  });

	  // tests are complete, output some stats
	  // and the failures if any
	  runner.on('end', function () {
	    cursor.show();
	    console.log();
	    self.epilogue();
	  });
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(Progress, Base);

	}).call(this,require('_process'))
	},{"../utils":38,"./base":17,"_process":82}],30:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var inherits = require('../utils').inherits;
	var color = Base.color;

	/**
	 * Expose `Spec`.
	 */

	exports = module.exports = Spec;

	/**
	 * Initialize a new `Spec` test reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function Spec (runner) {
	  Base.call(this, runner);

	  var self = this;
	  var indents = 0;
	  var n = 0;

	  function indent () {
	    return Array(indents).join('  ');
	  }

	  runner.on('start', function () {
	    console.log();
	  });

	  runner.on('suite', function (suite) {
	    ++indents;
	    console.log(color('suite', '%s%s'), indent(), suite.title);
	  });

	  runner.on('suite end', function () {
	    --indents;
	    if (indents === 1) {
	      console.log();
	    }
	  });

	  runner.on('pending', function (test) {
	    var fmt = indent() + color('pending', '  - %s');
	    console.log(fmt, test.title);
	  });

	  runner.on('pass', function (test) {
	    var fmt;
	    if (test.speed === 'fast') {
	      fmt = indent() +
	        color('checkmark', '  ' + Base.symbols.ok) +
	        color('pass', ' %s');
	      console.log(fmt, test.title);
	    } else {
	      fmt = indent() +
	        color('checkmark', '  ' + Base.symbols.ok) +
	        color('pass', ' %s') +
	        color(test.speed, ' (%dms)');
	      console.log(fmt, test.title, test.duration);
	    }
	  });

	  runner.on('fail', function (test) {
	    console.log(indent() + color('fail', '  %d) %s'), ++n, test.title);
	  });

	  runner.on('end', self.epilogue.bind(self));
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(Spec, Base);

	},{"../utils":38,"./base":17}],31:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');

	/**
	 * Expose `TAP`.
	 */

	exports = module.exports = TAP;

	/**
	 * Initialize a new `TAP` reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function TAP (runner) {
	  Base.call(this, runner);

	  var n = 1;
	  var passes = 0;
	  var failures = 0;

	  runner.on('start', function () {
	    var total = runner.grepTotal(runner.suite);
	    console.log('%d..%d', 1, total);
	  });

	  runner.on('test end', function () {
	    ++n;
	  });

	  runner.on('pending', function (test) {
	    console.log('ok %d %s # SKIP -', n, title(test));
	  });

	  runner.on('pass', function (test) {
	    passes++;
	    console.log('ok %d %s', n, title(test));
	  });

	  runner.on('fail', function (test, err) {
	    failures++;
	    console.log('not ok %d %s', n, title(test));
	    if (err.stack) {
	      console.log(err.stack.replace(/^/gm, '  '));
	    }
	  });

	  runner.on('end', function () {
	    console.log('# tests ' + (passes + failures));
	    console.log('# pass ' + passes);
	    console.log('# fail ' + failures);
	  });
	}

	/**
	 * Return a TAP-safe title of `test`
	 *
	 * @api private
	 * @param {Object} test
	 * @return {String}
	 */
	function title (test) {
	  return test.fullTitle().replace(/#/g, '');
	}

	},{"./base":17}],32:[function(require,module,exports){
	(function (process,global){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Base = require('./base');
	var utils = require('../utils');
	var inherits = utils.inherits;
	var fs = require('fs');
	var escape = utils.escape;
	var mkdirp = require('mkdirp');
	var path = require('path');

	/**
	 * Save timer references to avoid Sinon interfering (see GH-237).
	 */

	/* eslint-disable no-unused-vars, no-native-reassign */
	var Date = global.Date;
	var setTimeout = global.setTimeout;
	var setInterval = global.setInterval;
	var clearTimeout = global.clearTimeout;
	var clearInterval = global.clearInterval;
	/* eslint-enable no-unused-vars, no-native-reassign */

	/**
	 * Expose `XUnit`.
	 */

	exports = module.exports = XUnit;

	/**
	 * Initialize a new `XUnit` reporter.
	 *
	 * @api public
	 * @param {Runner} runner
	 */
	function XUnit (runner, options) {
	  Base.call(this, runner);

	  var stats = this.stats;
	  var tests = [];
	  var self = this;

	  if (options && options.reporterOptions && options.reporterOptions.output) {
	    if (!fs.createWriteStream) {
	      throw new Error('file output not supported in browser');
	    }
	    mkdirp.sync(path.dirname(options.reporterOptions.output));
	    self.fileStream = fs.createWriteStream(options.reporterOptions.output);
	  }

	  runner.on('pending', function (test) {
	    tests.push(test);
	  });

	  runner.on('pass', function (test) {
	    tests.push(test);
	  });

	  runner.on('fail', function (test) {
	    tests.push(test);
	  });

	  runner.on('end', function () {
	    self.write(tag('testsuite', {
	      name: 'Mocha Tests',
	      tests: stats.tests,
	      failures: stats.failures,
	      errors: stats.failures,
	      skipped: stats.tests - stats.failures - stats.passes,
	      timestamp: (new Date()).toUTCString(),
	      time: (stats.duration / 1000) || 0
	    }, false));

	    tests.forEach(function (t) {
	      self.test(t);
	    });

	    self.write('</testsuite>');
	  });
	}

	/**
	 * Inherit from `Base.prototype`.
	 */
	inherits(XUnit, Base);

	/**
	 * Override done to close the stream (if it's a file).
	 *
	 * @param failures
	 * @param {Function} fn
	 */
	XUnit.prototype.done = function (failures, fn) {
	  if (this.fileStream) {
	    this.fileStream.end(function () {
	      fn(failures);
	    });
	  } else {
	    fn(failures);
	  }
	};

	/**
	 * Write out the given line.
	 *
	 * @param {string} line
	 */
	XUnit.prototype.write = function (line) {
	  if (this.fileStream) {
	    this.fileStream.write(line + '\n');
	  } else if (typeof process === 'object' && process.stdout) {
	    process.stdout.write(line + '\n');
	  } else {
	    console.log(line);
	  }
	};

	/**
	 * Output tag for the given `test.`
	 *
	 * @param {Test} test
	 */
	XUnit.prototype.test = function (test) {
	  var attrs = {
	    classname: test.parent.fullTitle(),
	    name: test.title,
	    time: (test.duration / 1000) || 0
	  };

	  if (test.state === 'failed') {
	    var err = test.err;
	    this.write(tag('testcase', attrs, false, tag('failure', {}, false, escape(err.message) + '\n' + escape(err.stack))));
	  } else if (test.isPending()) {
	    this.write(tag('testcase', attrs, false, tag('skipped', {}, true)));
	  } else {
	    this.write(tag('testcase', attrs, true));
	  }
	};

	/**
	 * HTML tag helper.
	 *
	 * @param name
	 * @param attrs
	 * @param close
	 * @param content
	 * @return {string}
	 */
	function tag (name, attrs, close, content) {
	  var end = close ? '/>' : '>';
	  var pairs = [];
	  var tag;

	  for (var key in attrs) {
	    if (Object.prototype.hasOwnProperty.call(attrs, key)) {
	      pairs.push(key + '="' + escape(attrs[key]) + '"');
	    }
	  }

	  tag = '<' + name + (pairs.length ? ' ' + pairs.join(' ') : '') + end;
	  if (content) {
	    tag += content + '</' + name + end;
	  }
	  return tag;
	}

	}).call(this,require('_process'),typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"../utils":38,"./base":17,"_process":82,"fs":42,"mkdirp":79,"path":42}],33:[function(require,module,exports){
	(function (global){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var EventEmitter = require('events').EventEmitter;
	var JSON = require('json3');
	var Pending = require('./pending');
	var debug = require('debug')('mocha:runnable');
	var milliseconds = require('./ms');
	var utils = require('./utils');
	var create = require('lodash.create');

	/**
	 * Save timer references to avoid Sinon interfering (see GH-237).
	 */

	/* eslint-disable no-unused-vars, no-native-reassign */
	var Date = global.Date;
	var setTimeout = global.setTimeout;
	var setInterval = global.setInterval;
	var clearTimeout = global.clearTimeout;
	var clearInterval = global.clearInterval;
	/* eslint-enable no-unused-vars, no-native-reassign */

	/**
	 * Object#toString().
	 */

	var toString = Object.prototype.toString;

	/**
	 * Expose `Runnable`.
	 */

	module.exports = Runnable;

	/**
	 * Initialize a new `Runnable` with the given `title` and callback `fn`.
	 *
	 * @param {String} title
	 * @param {Function} fn
	 * @api private
	 * @param {string} title
	 * @param {Function} fn
	 */
	function Runnable (title, fn) {
	  this.title = title;
	  this.fn = fn;
	  this.body = (fn || '').toString();
	  this.async = fn && fn.length;
	  this.sync = !this.async;
	  this._timeout = 2000;
	  this._slow = 75;
	  this._enableTimeouts = true;
	  this.timedOut = false;
	  this._trace = new Error('done() called multiple times');
	  this._retries = -1;
	  this._currentRetry = 0;
	  this.pending = false;
	}

	/**
	 * Inherit from `EventEmitter.prototype`.
	 */
	Runnable.prototype = create(EventEmitter.prototype, {
	  constructor: Runnable
	});

	/**
	 * Set & get timeout `ms`.
	 *
	 * @api private
	 * @param {number|string} ms
	 * @return {Runnable|number} ms or Runnable instance.
	 */
	Runnable.prototype.timeout = function (ms) {
	  if (!arguments.length) {
	    return this._timeout;
	  }
	  // see #1652 for reasoning
	  if (ms === 0 || ms > Math.pow(2, 31)) {
	    this._enableTimeouts = false;
	  }
	  if (typeof ms === 'string') {
	    ms = milliseconds(ms);
	  }
	  debug('timeout %d', ms);
	  this._timeout = ms;
	  if (this.timer) {
	    this.resetTimeout();
	  }
	  return this;
	};

	/**
	 * Set & get slow `ms`.
	 *
	 * @api private
	 * @param {number|string} ms
	 * @return {Runnable|number} ms or Runnable instance.
	 */
	Runnable.prototype.slow = function (ms) {
	  if (typeof ms === 'undefined') {
	    return this._slow;
	  }
	  if (typeof ms === 'string') {
	    ms = milliseconds(ms);
	  }
	  debug('timeout %d', ms);
	  this._slow = ms;
	  return this;
	};

	/**
	 * Set and get whether timeout is `enabled`.
	 *
	 * @api private
	 * @param {boolean} enabled
	 * @return {Runnable|boolean} enabled or Runnable instance.
	 */
	Runnable.prototype.enableTimeouts = function (enabled) {
	  if (!arguments.length) {
	    return this._enableTimeouts;
	  }
	  debug('enableTimeouts %s', enabled);
	  this._enableTimeouts = enabled;
	  return this;
	};

	/**
	 * Halt and mark as pending.
	 *
	 * @api public
	 */
	Runnable.prototype.skip = function () {
	  throw new Pending('sync skip');
	};

	/**
	 * Check if this runnable or its parent suite is marked as pending.
	 *
	 * @api private
	 */
	Runnable.prototype.isPending = function () {
	  return this.pending || (this.parent && this.parent.isPending());
	};

	/**
	 * Set number of retries.
	 *
	 * @api private
	 */
	Runnable.prototype.retries = function (n) {
	  if (!arguments.length) {
	    return this._retries;
	  }
	  this._retries = n;
	};

	/**
	 * Get current retry
	 *
	 * @api private
	 */
	Runnable.prototype.currentRetry = function (n) {
	  if (!arguments.length) {
	    return this._currentRetry;
	  }
	  this._currentRetry = n;
	};

	/**
	 * Return the full title generated by recursively concatenating the parent's
	 * full title.
	 *
	 * @api public
	 * @return {string}
	 */
	Runnable.prototype.fullTitle = function () {
	  return this.parent.fullTitle() + ' ' + this.title;
	};

	/**
	 * Clear the timeout.
	 *
	 * @api private
	 */
	Runnable.prototype.clearTimeout = function () {
	  clearTimeout(this.timer);
	};

	/**
	 * Inspect the runnable void of private properties.
	 *
	 * @api private
	 * @return {string}
	 */
	Runnable.prototype.inspect = function () {
	  return JSON.stringify(this, function (key, val) {
	    if (key[0] === '_') {
	      return;
	    }
	    if (key === 'parent') {
	      return '#<Suite>';
	    }
	    if (key === 'ctx') {
	      return '#<Context>';
	    }
	    return val;
	  }, 2);
	};

	/**
	 * Reset the timeout.
	 *
	 * @api private
	 */
	Runnable.prototype.resetTimeout = function () {
	  var self = this;
	  var ms = this.timeout() || 1e9;

	  if (!this._enableTimeouts) {
	    return;
	  }
	  this.clearTimeout();
	  this.timer = setTimeout(function () {
	    if (!self._enableTimeouts) {
	      return;
	    }
	    self.callback(new Error('Timeout of ' + ms +
	      'ms exceeded. For async tests and hooks, ensure "done()" is called; if returning a Promise, ensure it resolves.'));
	    self.timedOut = true;
	  }, ms);
	};

	/**
	 * Whitelist a list of globals for this test run.
	 *
	 * @api private
	 * @param {string[]} globals
	 */
	Runnable.prototype.globals = function (globals) {
	  if (!arguments.length) {
	    return this._allowedGlobals;
	  }
	  this._allowedGlobals = globals;
	};

	/**
	 * Run the test and invoke `fn(err)`.
	 *
	 * @param {Function} fn
	 * @api private
	 */
	Runnable.prototype.run = function (fn) {
	  var self = this;
	  var start = new Date();
	  var ctx = this.ctx;
	  var finished;
	  var emitted;

	  // Sometimes the ctx exists, but it is not runnable
	  if (ctx && ctx.runnable) {
	    ctx.runnable(this);
	  }

	  // called multiple times
	  function multiple (err) {
	    if (emitted) {
	      return;
	    }
	    emitted = true;
	    self.emit('error', err || new Error('done() called multiple times; stacktrace may be inaccurate'));
	  }

	  // finished
	  function done (err) {
	    var ms = self.timeout();
	    if (self.timedOut) {
	      return;
	    }
	    if (finished) {
	      return multiple(err || self._trace);
	    }

	    self.clearTimeout();
	    self.duration = new Date() - start;
	    finished = true;
	    if (!err && self.duration > ms && self._enableTimeouts) {
	      err = new Error('Timeout of ' + ms +
	      'ms exceeded. For async tests and hooks, ensure "done()" is called; if returning a Promise, ensure it resolves.');
	    }
	    fn(err);
	  }

	  // for .resetTimeout()
	  this.callback = done;

	  // explicit async with `done` argument
	  if (this.async) {
	    this.resetTimeout();

	    // allows skip() to be used in an explicit async context
	    this.skip = function asyncSkip () {
	      done(new Pending('async skip call'));
	      // halt execution.  the Runnable will be marked pending
	      // by the previous call, and the uncaught handler will ignore
	      // the failure.
	      throw new Pending('async skip; aborting execution');
	    };

	    if (this.allowUncaught) {
	      return callFnAsync(this.fn);
	    }
	    try {
	      callFnAsync(this.fn);
	    } catch (err) {
	      emitted = true;
	      done(utils.getError(err));
	    }
	    return;
	  }

	  if (this.allowUncaught) {
	    if (this.isPending()) {
	      done();
	    } else {
	      callFn(this.fn);
	    }
	    return;
	  }

	  // sync or promise-returning
	  try {
	    if (this.isPending()) {
	      done();
	    } else {
	      callFn(this.fn);
	    }
	  } catch (err) {
	    emitted = true;
	    done(utils.getError(err));
	  }

	  function callFn (fn) {
	    var result = fn.call(ctx);
	    if (result && typeof result.then === 'function') {
	      self.resetTimeout();
	      result
	        .then(function () {
	          done();
	          // Return null so libraries like bluebird do not warn about
	          // subsequently constructed Promises.
	          return null;
	        },
	        function (reason) {
	          done(reason || new Error('Promise rejected with no or falsy reason'));
	        });
	    } else {
	      if (self.asyncOnly) {
	        return done(new Error('--async-only option in use without declaring `done()` or returning a promise'));
	      }

	      done();
	    }
	  }

	  function callFnAsync (fn) {
	    var result = fn.call(ctx, function (err) {
	      if (err instanceof Error || toString.call(err) === '[object Error]') {
	        return done(err);
	      }
	      if (err) {
	        if (Object.prototype.toString.call(err) === '[object Object]') {
	          return done(new Error('done() invoked with non-Error: ' +
	            JSON.stringify(err)));
	        }
	        return done(new Error('done() invoked with non-Error: ' + err));
	      }
	      if (result && utils.isPromise(result)) {
	        return done(new Error('Resolution method is overspecified. Specify a callback *or* return a Promise; not both.'));
	      }

	      done();
	    });
	  }
	};

	}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"./ms":15,"./pending":16,"./utils":38,"debug":2,"events":3,"json3":69,"lodash.create":75}],34:[function(require,module,exports){
	(function (process,global){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var EventEmitter = require('events').EventEmitter;
	var Pending = require('./pending');
	var utils = require('./utils');
	var inherits = utils.inherits;
	var debug = require('debug')('mocha:runner');
	var Runnable = require('./runnable');
	var filter = utils.filter;
	var indexOf = utils.indexOf;
	var some = utils.some;
	var keys = utils.keys;
	var stackFilter = utils.stackTraceFilter();
	var stringify = utils.stringify;
	var type = utils.type;
	var undefinedError = utils.undefinedError;
	var isArray = utils.isArray;

	/**
	 * Non-enumerable globals.
	 */

	var globals = [
	  'setTimeout',
	  'clearTimeout',
	  'setInterval',
	  'clearInterval',
	  'XMLHttpRequest',
	  'Date',
	  'setImmediate',
	  'clearImmediate'
	];

	/**
	 * Expose `Runner`.
	 */

	module.exports = Runner;

	/**
	 * Initialize a `Runner` for the given `suite`.
	 *
	 * Events:
	 *
	 *   - `start`  execution started
	 *   - `end`  execution complete
	 *   - `suite`  (suite) test suite execution started
	 *   - `suite end`  (suite) all tests (and sub-suites) have finished
	 *   - `test`  (test) test execution started
	 *   - `test end`  (test) test completed
	 *   - `hook`  (hook) hook execution started
	 *   - `hook end`  (hook) hook complete
	 *   - `pass`  (test) test passed
	 *   - `fail`  (test, err) test failed
	 *   - `pending`  (test) test pending
	 *
	 * @api public
	 * @param {Suite} suite Root suite
	 * @param {boolean} [delay] Whether or not to delay execution of root suite
	 * until ready.
	 */
	function Runner (suite, delay) {
	  var self = this;
	  this._globals = [];
	  this._abort = false;
	  this._delay = delay;
	  this.suite = suite;
	  this.started = false;
	  this.total = suite.total();
	  this.failures = 0;
	  this.on('test end', function (test) {
	    self.checkGlobals(test);
	  });
	  this.on('hook end', function (hook) {
	    self.checkGlobals(hook);
	  });
	  this._defaultGrep = /.*/;
	  this.grep(this._defaultGrep);
	  this.globals(this.globalProps().concat(extraGlobals()));
	}

	/**
	 * Wrapper for setImmediate, process.nextTick, or browser polyfill.
	 *
	 * @param {Function} fn
	 * @api private
	 */
	Runner.immediately = global.setImmediate || process.nextTick;

	/**
	 * Inherit from `EventEmitter.prototype`.
	 */
	inherits(Runner, EventEmitter);

	/**
	 * Run tests with full titles matching `re`. Updates runner.total
	 * with number of tests matched.
	 *
	 * @param {RegExp} re
	 * @param {Boolean} invert
	 * @return {Runner} for chaining
	 * @api public
	 * @param {RegExp} re
	 * @param {boolean} invert
	 * @return {Runner} Runner instance.
	 */
	Runner.prototype.grep = function (re, invert) {
	  debug('grep %s', re);
	  this._grep = re;
	  this._invert = invert;
	  this.total = this.grepTotal(this.suite);
	  return this;
	};

	/**
	 * Returns the number of tests matching the grep search for the
	 * given suite.
	 *
	 * @param {Suite} suite
	 * @return {Number}
	 * @api public
	 * @param {Suite} suite
	 * @return {number}
	 */
	Runner.prototype.grepTotal = function (suite) {
	  var self = this;
	  var total = 0;

	  suite.eachTest(function (test) {
	    var match = self._grep.test(test.fullTitle());
	    if (self._invert) {
	      match = !match;
	    }
	    if (match) {
	      total++;
	    }
	  });

	  return total;
	};

	/**
	 * Return a list of global properties.
	 *
	 * @return {Array}
	 * @api private
	 */
	Runner.prototype.globalProps = function () {
	  var props = keys(global);

	  // non-enumerables
	  for (var i = 0; i < globals.length; ++i) {
	    if (~indexOf(props, globals[i])) {
	      continue;
	    }
	    props.push(globals[i]);
	  }

	  return props;
	};

	/**
	 * Allow the given `arr` of globals.
	 *
	 * @param {Array} arr
	 * @return {Runner} for chaining
	 * @api public
	 * @param {Array} arr
	 * @return {Runner} Runner instance.
	 */
	Runner.prototype.globals = function (arr) {
	  if (!arguments.length) {
	    return this._globals;
	  }
	  debug('globals %j', arr);
	  this._globals = this._globals.concat(arr);
	  return this;
	};

	/**
	 * Check for global variable leaks.
	 *
	 * @api private
	 */
	Runner.prototype.checkGlobals = function (test) {
	  if (this.ignoreLeaks) {
	    return;
	  }
	  var ok = this._globals;

	  var globals = this.globalProps();
	  var leaks;

	  if (test) {
	    ok = ok.concat(test._allowedGlobals || []);
	  }

	  if (this.prevGlobalsLength === globals.length) {
	    return;
	  }
	  this.prevGlobalsLength = globals.length;

	  leaks = filterLeaks(ok, globals);
	  this._globals = this._globals.concat(leaks);

	  if (leaks.length > 1) {
	    this.fail(test, new Error('global leaks detected: ' + leaks.join(', ') + ''));
	  } else if (leaks.length) {
	    this.fail(test, new Error('global leak detected: ' + leaks[0]));
	  }
	};

	/**
	 * Fail the given `test`.
	 *
	 * @api private
	 * @param {Test} test
	 * @param {Error} err
	 */
	Runner.prototype.fail = function (test, err) {
	  if (test.isPending()) {
	    return;
	  }

	  ++this.failures;
	  test.state = 'failed';

	  if (!(err instanceof Error || err && typeof err.message === 'string')) {
	    err = new Error('the ' + type(err) + ' ' + stringify(err) + ' was thrown, throw an Error :)');
	  }

	  try {
	    err.stack = (this.fullStackTrace || !err.stack)
	      ? err.stack
	      : stackFilter(err.stack);
	  } catch (ignored) {
	    // some environments do not take kindly to monkeying with the stack
	  }

	  this.emit('fail', test, err);
	};

	/**
	 * Fail the given `hook` with `err`.
	 *
	 * Hook failures work in the following pattern:
	 * - If bail, then exit
	 * - Failed `before` hook skips all tests in a suite and subsuites,
	 *   but jumps to corresponding `after` hook
	 * - Failed `before each` hook skips remaining tests in a
	 *   suite and jumps to corresponding `after each` hook,
	 *   which is run only once
	 * - Failed `after` hook does not alter
	 *   execution order
	 * - Failed `after each` hook skips remaining tests in a
	 *   suite and subsuites, but executes other `after each`
	 *   hooks
	 *
	 * @api private
	 * @param {Hook} hook
	 * @param {Error} err
	 */
	Runner.prototype.failHook = function (hook, err) {
	  if (hook.ctx && hook.ctx.currentTest) {
	    hook.originalTitle = hook.originalTitle || hook.title;
	    hook.title = hook.originalTitle + ' for "' + hook.ctx.currentTest.title + '"';
	  }

	  this.fail(hook, err);
	  if (this.suite.bail()) {
	    this.emit('end');
	  }
	};

	/**
	 * Run hook `name` callbacks and then invoke `fn()`.
	 *
	 * @api private
	 * @param {string} name
	 * @param {Function} fn
	 */

	Runner.prototype.hook = function (name, fn) {
	  var suite = this.suite;
	  var hooks = suite['_' + name];
	  var self = this;

	  function next (i) {
	    var hook = hooks[i];
	    if (!hook) {
	      return fn();
	    }
	    self.currentRunnable = hook;

	    hook.ctx.currentTest = self.test;

	    self.emit('hook', hook);

	    if (!hook.listeners('error').length) {
	      hook.on('error', function (err) {
	        self.failHook(hook, err);
	      });
	    }

	    hook.run(function (err) {
	      var testError = hook.error();
	      if (testError) {
	        self.fail(self.test, testError);
	      }
	      if (err) {
	        if (err instanceof Pending) {
	          if (name === 'beforeEach' || name === 'afterEach') {
	            self.test.pending = true;
	          } else {
	            utils.forEach(suite.tests, function (test) {
	              test.pending = true;
	            });
	            // a pending hook won't be executed twice.
	            hook.pending = true;
	          }
	        } else {
	          self.failHook(hook, err);

	          // stop executing hooks, notify callee of hook err
	          return fn(err);
	        }
	      }
	      self.emit('hook end', hook);
	      delete hook.ctx.currentTest;
	      next(++i);
	    });
	  }

	  Runner.immediately(function () {
	    next(0);
	  });
	};

	/**
	 * Run hook `name` for the given array of `suites`
	 * in order, and callback `fn(err, errSuite)`.
	 *
	 * @api private
	 * @param {string} name
	 * @param {Array} suites
	 * @param {Function} fn
	 */
	Runner.prototype.hooks = function (name, suites, fn) {
	  var self = this;
	  var orig = this.suite;

	  function next (suite) {
	    self.suite = suite;

	    if (!suite) {
	      self.suite = orig;
	      return fn();
	    }

	    self.hook(name, function (err) {
	      if (err) {
	        var errSuite = self.suite;
	        self.suite = orig;
	        return fn(err, errSuite);
	      }

	      next(suites.pop());
	    });
	  }

	  next(suites.pop());
	};

	/**
	 * Run hooks from the top level down.
	 *
	 * @param {String} name
	 * @param {Function} fn
	 * @api private
	 */
	Runner.prototype.hookUp = function (name, fn) {
	  var suites = [this.suite].concat(this.parents()).reverse();
	  this.hooks(name, suites, fn);
	};

	/**
	 * Run hooks from the bottom up.
	 *
	 * @param {String} name
	 * @param {Function} fn
	 * @api private
	 */
	Runner.prototype.hookDown = function (name, fn) {
	  var suites = [this.suite].concat(this.parents());
	  this.hooks(name, suites, fn);
	};

	/**
	 * Return an array of parent Suites from
	 * closest to furthest.
	 *
	 * @return {Array}
	 * @api private
	 */
	Runner.prototype.parents = function () {
	  var suite = this.suite;
	  var suites = [];
	  while (suite.parent) {
	    suite = suite.parent;
	    suites.push(suite);
	  }
	  return suites;
	};

	/**
	 * Run the current test and callback `fn(err)`.
	 *
	 * @param {Function} fn
	 * @api private
	 */
	Runner.prototype.runTest = function (fn) {
	  var self = this;
	  var test = this.test;

	  if (!test) {
	    return;
	  }
	  if (this.asyncOnly) {
	    test.asyncOnly = true;
	  }
	  test.on('error', function (err) {
	    self.fail(test, err);
	  });
	  if (this.allowUncaught) {
	    test.allowUncaught = true;
	    return test.run(fn);
	  }
	  try {
	    test.run(fn);
	  } catch (err) {
	    fn(err);
	  }
	};

	/**
	 * Run tests in the given `suite` and invoke the callback `fn()` when complete.
	 *
	 * @api private
	 * @param {Suite} suite
	 * @param {Function} fn
	 */
	Runner.prototype.runTests = function (suite, fn) {
	  var self = this;
	  var tests = suite.tests.slice();
	  var test;

	  function hookErr (_, errSuite, after) {
	    // before/after Each hook for errSuite failed:
	    var orig = self.suite;

	    // for failed 'after each' hook start from errSuite parent,
	    // otherwise start from errSuite itself
	    self.suite = after ? errSuite.parent : errSuite;

	    if (self.suite) {
	      // call hookUp afterEach
	      self.hookUp('afterEach', function (err2, errSuite2) {
	        self.suite = orig;
	        // some hooks may fail even now
	        if (err2) {
	          return hookErr(err2, errSuite2, true);
	        }
	        // report error suite
	        fn(errSuite);
	      });
	    } else {
	      // there is no need calling other 'after each' hooks
	      self.suite = orig;
	      fn(errSuite);
	    }
	  }

	  function next (err, errSuite) {
	    // if we bail after first err
	    if (self.failures && suite._bail) {
	      return fn();
	    }

	    if (self._abort) {
	      return fn();
	    }

	    if (err) {
	      return hookErr(err, errSuite, true);
	    }

	    // next test
	    test = tests.shift();

	    // all done
	    if (!test) {
	      return fn();
	    }

	    // grep
	    var match = self._grep.test(test.fullTitle());
	    if (self._invert) {
	      match = !match;
	    }
	    if (!match) {
	      // Run immediately only if we have defined a grep. When we
	      // define a grep — It can cause maximum callstack error if
	      // the grep is doing a large recursive loop by neglecting
	      // all tests. The run immediately function also comes with
	      // a performance cost. So we don't want to run immediately
	      // if we run the whole test suite, because running the whole
	      // test suite don't do any immediate recursive loops. Thus,
	      // allowing a JS runtime to breathe.
	      if (self._grep !== self._defaultGrep) {
	        Runner.immediately(next);
	      } else {
	        next();
	      }
	      return;
	    }

	    if (test.isPending()) {
	      self.emit('pending', test);
	      self.emit('test end', test);
	      return next();
	    }

	    // execute test and hook(s)
	    self.emit('test', self.test = test);
	    self.hookDown('beforeEach', function (err, errSuite) {
	      if (test.isPending()) {
	        self.emit('pending', test);
	        self.emit('test end', test);
	        return next();
	      }
	      if (err) {
	        return hookErr(err, errSuite, false);
	      }
	      self.currentRunnable = self.test;
	      self.runTest(function (err) {
	        test = self.test;
	        if (err) {
	          var retry = test.currentRetry();
	          if (err instanceof Pending) {
	            test.pending = true;
	            self.emit('pending', test);
	          } else if (retry < test.retries()) {
	            var clonedTest = test.clone();
	            clonedTest.currentRetry(retry + 1);
	            tests.unshift(clonedTest);

	            // Early return + hook trigger so that it doesn't
	            // increment the count wrong
	            return self.hookUp('afterEach', next);
	          } else {
	            self.fail(test, err);
	          }
	          self.emit('test end', test);

	          if (err instanceof Pending) {
	            return next();
	          }

	          return self.hookUp('afterEach', next);
	        }

	        test.state = 'passed';
	        self.emit('pass', test);
	        self.emit('test end', test);
	        self.hookUp('afterEach', next);
	      });
	    });
	  }

	  this.next = next;
	  this.hookErr = hookErr;
	  next();
	};

	/**
	 * Run the given `suite` and invoke the callback `fn()` when complete.
	 *
	 * @api private
	 * @param {Suite} suite
	 * @param {Function} fn
	 */
	Runner.prototype.runSuite = function (suite, fn) {
	  var i = 0;
	  var self = this;
	  var total = this.grepTotal(suite);
	  var afterAllHookCalled = false;

	  debug('run suite %s', suite.fullTitle());

	  if (!total || (self.failures && suite._bail)) {
	    return fn();
	  }

	  this.emit('suite', this.suite = suite);

	  function next (errSuite) {
	    if (errSuite) {
	      // current suite failed on a hook from errSuite
	      if (errSuite === suite) {
	        // if errSuite is current suite
	        // continue to the next sibling suite
	        return done();
	      }
	      // errSuite is among the parents of current suite
	      // stop execution of errSuite and all sub-suites
	      return done(errSuite);
	    }

	    if (self._abort) {
	      return done();
	    }

	    var curr = suite.suites[i++];
	    if (!curr) {
	      return done();
	    }

	    // Avoid grep neglecting large number of tests causing a
	    // huge recursive loop and thus a maximum call stack error.
	    // See comment in `this.runTests()` for more information.
	    if (self._grep !== self._defaultGrep) {
	      Runner.immediately(function () {
	        self.runSuite(curr, next);
	      });
	    } else {
	      self.runSuite(curr, next);
	    }
	  }

	  function done (errSuite) {
	    self.suite = suite;
	    self.nextSuite = next;

	    if (afterAllHookCalled) {
	      fn(errSuite);
	    } else {
	      // mark that the afterAll block has been called once
	      // and so can be skipped if there is an error in it.
	      afterAllHookCalled = true;

	      // remove reference to test
	      delete self.test;

	      self.hook('afterAll', function () {
	        self.emit('suite end', suite);
	        fn(errSuite);
	      });
	    }
	  }

	  this.nextSuite = next;

	  this.hook('beforeAll', function (err) {
	    if (err) {
	      return done();
	    }
	    self.runTests(suite, next);
	  });
	};

	/**
	 * Handle uncaught exceptions.
	 *
	 * @param {Error} err
	 * @api private
	 */
	Runner.prototype.uncaught = function (err) {
	  if (err) {
	    debug('uncaught exception %s', err === (function () {
	      return this;
	    }.call(err)) ? (err.message || err) : err);
	  } else {
	    debug('uncaught undefined exception');
	    err = undefinedError();
	  }
	  err.uncaught = true;

	  var runnable = this.currentRunnable;

	  if (!runnable) {
	    runnable = new Runnable('Uncaught error outside test suite');
	    runnable.parent = this.suite;

	    if (this.started) {
	      this.fail(runnable, err);
	    } else {
	      // Can't recover from this failure
	      this.emit('start');
	      this.fail(runnable, err);
	      this.emit('end');
	    }

	    return;
	  }

	  runnable.clearTimeout();

	  // Ignore errors if complete or pending
	  if (runnable.state || runnable.isPending()) {
	    return;
	  }
	  this.fail(runnable, err);

	  // recover from test
	  if (runnable.type === 'test') {
	    this.emit('test end', runnable);
	    this.hookUp('afterEach', this.next);
	    return;
	  }

	 // recover from hooks
	  if (runnable.type === 'hook') {
	    var errSuite = this.suite;
	    // if hook failure is in afterEach block
	    if (runnable.fullTitle().indexOf('after each') > -1) {
	      return this.hookErr(err, errSuite, true);
	    }
	    // if hook failure is in beforeEach block
	    if (runnable.fullTitle().indexOf('before each') > -1) {
	      return this.hookErr(err, errSuite, false);
	    }
	    // if hook failure is in after or before blocks
	    return this.nextSuite(errSuite);
	  }

	  // bail
	  this.emit('end');
	};

	/**
	 * Cleans up the references to all the deferred functions
	 * (before/after/beforeEach/afterEach) and tests of a Suite.
	 * These must be deleted otherwise a memory leak can happen,
	 * as those functions may reference variables from closures,
	 * thus those variables can never be garbage collected as long
	 * as the deferred functions exist.
	 *
	 * @param {Suite} suite
	 */
	function cleanSuiteReferences (suite) {
	  function cleanArrReferences (arr) {
	    for (var i = 0; i < arr.length; i++) {
	      delete arr[i].fn;
	    }
	  }

	  if (isArray(suite._beforeAll)) {
	    cleanArrReferences(suite._beforeAll);
	  }

	  if (isArray(suite._beforeEach)) {
	    cleanArrReferences(suite._beforeEach);
	  }

	  if (isArray(suite._afterAll)) {
	    cleanArrReferences(suite._afterAll);
	  }

	  if (isArray(suite._afterEach)) {
	    cleanArrReferences(suite._afterEach);
	  }

	  for (var i = 0; i < suite.tests.length; i++) {
	    delete suite.tests[i].fn;
	  }
	}

	/**
	 * Run the root suite and invoke `fn(failures)`
	 * on completion.
	 *
	 * @param {Function} fn
	 * @return {Runner} for chaining
	 * @api public
	 * @param {Function} fn
	 * @return {Runner} Runner instance.
	 */
	Runner.prototype.run = function (fn) {
	  var self = this;
	  var rootSuite = this.suite;

	  // If there is an `only` filter
	  if (this.hasOnly) {
	    filterOnly(rootSuite);
	  }

	  fn = fn || function () {};

	  function uncaught (err) {
	    self.uncaught(err);
	  }

	  function start () {
	    self.started = true;
	    self.emit('start');
	    self.runSuite(rootSuite, function () {
	      debug('finished running');
	      self.emit('end');
	    });
	  }

	  debug('start');

	  // references cleanup to avoid memory leaks
	  this.on('suite end', cleanSuiteReferences);

	  // callback
	  this.on('end', function () {
	    debug('end');
	    process.removeListener('uncaughtException', uncaught);
	    fn(self.failures);
	  });

	  // uncaught exception
	  process.on('uncaughtException', uncaught);

	  if (this._delay) {
	    // for reporters, I guess.
	    // might be nice to debounce some dots while we wait.
	    this.emit('waiting', rootSuite);
	    rootSuite.once('run', start);
	  } else {
	    start();
	  }

	  return this;
	};

	/**
	 * Cleanly abort execution.
	 *
	 * @api public
	 * @return {Runner} Runner instance.
	 */
	Runner.prototype.abort = function () {
	  debug('aborting');
	  this._abort = true;

	  return this;
	};

	/**
	 * Filter suites based on `isOnly` logic.
	 *
	 * @param {Array} suite
	 * @returns {Boolean}
	 * @api private
	 */
	function filterOnly (suite) {
	  if (suite._onlyTests.length) {
	    // If the suite contains `only` tests, run those and ignore any nested suites.
	    suite.tests = suite._onlyTests;
	    suite.suites = [];
	  } else {
	    // Otherwise, do not run any of the tests in this suite.
	    suite.tests = [];
	    utils.forEach(suite._onlySuites, function (onlySuite) {
	      // If there are other `only` tests/suites nested in the current `only` suite, then filter that `only` suite.
	      // Otherwise, all of the tests on this `only` suite should be run, so don't filter it.
	      if (hasOnly(onlySuite)) {
	        filterOnly(onlySuite);
	      }
	    });
	    // Run the `only` suites, as well as any other suites that have `only` tests/suites as descendants.
	    suite.suites = filter(suite.suites, function (childSuite) {
	      return indexOf(suite._onlySuites, childSuite) !== -1 || filterOnly(childSuite);
	    });
	  }
	  // Keep the suite only if there is something to run
	  return suite.tests.length || suite.suites.length;
	}

	/**
	 * Determines whether a suite has an `only` test or suite as a descendant.
	 *
	 * @param {Array} suite
	 * @returns {Boolean}
	 * @api private
	 */
	function hasOnly (suite) {
	  return suite._onlyTests.length || suite._onlySuites.length || some(suite.suites, hasOnly);
	}

	/**
	 * Filter leaks with the given globals flagged as `ok`.
	 *
	 * @api private
	 * @param {Array} ok
	 * @param {Array} globals
	 * @return {Array}
	 */
	function filterLeaks (ok, globals) {
	  return filter(globals, function (key) {
	    // Firefox and Chrome exposes iframes as index inside the window object
	    if (/^\d+/.test(key)) {
	      return false;
	    }

	    // in firefox
	    // if runner runs in an iframe, this iframe's window.getInterface method
	    // not init at first it is assigned in some seconds
	    if (global.navigator && (/^getInterface/).test(key)) {
	      return false;
	    }

	    // an iframe could be approached by window[iframeIndex]
	    // in ie6,7,8 and opera, iframeIndex is enumerable, this could cause leak
	    if (global.navigator && (/^\d+/).test(key)) {
	      return false;
	    }

	    // Opera and IE expose global variables for HTML element IDs (issue #243)
	    if (/^mocha-/.test(key)) {
	      return false;
	    }

	    var matched = filter(ok, function (ok) {
	      if (~ok.indexOf('*')) {
	        return key.indexOf(ok.split('*')[0]) === 0;
	      }
	      return key === ok;
	    });
	    return !matched.length && (!global.navigator || key !== 'onerror');
	  });
	}

	/**
	 * Array of globals dependent on the environment.
	 *
	 * @return {Array}
	 * @api private
	 */
	function extraGlobals () {
	  if (typeof process === 'object' && typeof process.version === 'string') {
	    var parts = process.version.split('.');
	    var nodeVersion = utils.reduce(parts, function (a, v) {
	      return a << 8 | v;
	    });

	    // 'errno' was renamed to process._errno in v0.9.11.

	    if (nodeVersion < 0x00090B) {
	      return ['errno'];
	    }
	  }

	  return [];
	}

	}).call(this,require('_process'),typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"./pending":16,"./runnable":33,"./utils":38,"_process":82,"debug":2,"events":3}],35:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var EventEmitter = require('events').EventEmitter;
	var Hook = require('./hook');
	var utils = require('./utils');
	var inherits = utils.inherits;
	var debug = require('debug')('mocha:suite');
	var milliseconds = require('./ms');

	/**
	 * Expose `Suite`.
	 */

	exports = module.exports = Suite;

	/**
	 * Create a new `Suite` with the given `title` and parent `Suite`. When a suite
	 * with the same title is already present, that suite is returned to provide
	 * nicer reporter and more flexible meta-testing.
	 *
	 * @api public
	 * @param {Suite} parent
	 * @param {string} title
	 * @return {Suite}
	 */
	exports.create = function (parent, title) {
	  var suite = new Suite(title, parent.ctx);
	  suite.parent = parent;
	  title = suite.fullTitle();
	  parent.addSuite(suite);
	  return suite;
	};

	/**
	 * Initialize a new `Suite` with the given `title` and `ctx`.
	 *
	 * @api private
	 * @param {string} title
	 * @param {Context} parentContext
	 */
	function Suite (title, parentContext) {
	  if (!utils.isString(title)) {
	    throw new Error('Suite `title` should be a "string" but "' + typeof title + '" was given instead.');
	  }
	  this.title = title;
	  function Context () {}
	  Context.prototype = parentContext;
	  this.ctx = new Context();
	  this.suites = [];
	  this.tests = [];
	  this.pending = false;
	  this._beforeEach = [];
	  this._beforeAll = [];
	  this._afterEach = [];
	  this._afterAll = [];
	  this.root = !title;
	  this._timeout = 2000;
	  this._enableTimeouts = true;
	  this._slow = 75;
	  this._bail = false;
	  this._retries = -1;
	  this._onlyTests = [];
	  this._onlySuites = [];
	  this.delayed = false;
	}

	/**
	 * Inherit from `EventEmitter.prototype`.
	 */
	inherits(Suite, EventEmitter);

	/**
	 * Return a clone of this `Suite`.
	 *
	 * @api private
	 * @return {Suite}
	 */
	Suite.prototype.clone = function () {
	  var suite = new Suite(this.title);
	  debug('clone');
	  suite.ctx = this.ctx;
	  suite.timeout(this.timeout());
	  suite.retries(this.retries());
	  suite.enableTimeouts(this.enableTimeouts());
	  suite.slow(this.slow());
	  suite.bail(this.bail());
	  return suite;
	};

	/**
	 * Set timeout `ms` or short-hand such as "2s".
	 *
	 * @api private
	 * @param {number|string} ms
	 * @return {Suite|number} for chaining
	 */
	Suite.prototype.timeout = function (ms) {
	  if (!arguments.length) {
	    return this._timeout;
	  }
	  if (ms.toString() === '0') {
	    this._enableTimeouts = false;
	  }
	  if (typeof ms === 'string') {
	    ms = milliseconds(ms);
	  }
	  debug('timeout %d', ms);
	  this._timeout = parseInt(ms, 10);
	  return this;
	};

	/**
	 * Set number of times to retry a failed test.
	 *
	 * @api private
	 * @param {number|string} n
	 * @return {Suite|number} for chaining
	 */
	Suite.prototype.retries = function (n) {
	  if (!arguments.length) {
	    return this._retries;
	  }
	  debug('retries %d', n);
	  this._retries = parseInt(n, 10) || 0;
	  return this;
	};

	/**
	  * Set timeout to `enabled`.
	  *
	  * @api private
	  * @param {boolean} enabled
	  * @return {Suite|boolean} self or enabled
	  */
	Suite.prototype.enableTimeouts = function (enabled) {
	  if (!arguments.length) {
	    return this._enableTimeouts;
	  }
	  debug('enableTimeouts %s', enabled);
	  this._enableTimeouts = enabled;
	  return this;
	};

	/**
	 * Set slow `ms` or short-hand such as "2s".
	 *
	 * @api private
	 * @param {number|string} ms
	 * @return {Suite|number} for chaining
	 */
	Suite.prototype.slow = function (ms) {
	  if (!arguments.length) {
	    return this._slow;
	  }
	  if (typeof ms === 'string') {
	    ms = milliseconds(ms);
	  }
	  debug('slow %d', ms);
	  this._slow = ms;
	  return this;
	};

	/**
	 * Sets whether to bail after first error.
	 *
	 * @api private
	 * @param {boolean} bail
	 * @return {Suite|number} for chaining
	 */
	Suite.prototype.bail = function (bail) {
	  if (!arguments.length) {
	    return this._bail;
	  }
	  debug('bail %s', bail);
	  this._bail = bail;
	  return this;
	};

	/**
	 * Check if this suite or its parent suite is marked as pending.
	 *
	 * @api private
	 */
	Suite.prototype.isPending = function () {
	  return this.pending || (this.parent && this.parent.isPending());
	};

	/**
	 * Run `fn(test[, done])` before running tests.
	 *
	 * @api private
	 * @param {string} title
	 * @param {Function} fn
	 * @return {Suite} for chaining
	 */
	Suite.prototype.beforeAll = function (title, fn) {
	  if (this.isPending()) {
	    return this;
	  }
	  if (typeof title === 'function') {
	    fn = title;
	    title = fn.name;
	  }
	  title = '"before all" hook' + (title ? ': ' + title : '');

	  var hook = new Hook(title, fn);
	  hook.parent = this;
	  hook.timeout(this.timeout());
	  hook.retries(this.retries());
	  hook.enableTimeouts(this.enableTimeouts());
	  hook.slow(this.slow());
	  hook.ctx = this.ctx;
	  this._beforeAll.push(hook);
	  this.emit('beforeAll', hook);
	  return this;
	};

	/**
	 * Run `fn(test[, done])` after running tests.
	 *
	 * @api private
	 * @param {string} title
	 * @param {Function} fn
	 * @return {Suite} for chaining
	 */
	Suite.prototype.afterAll = function (title, fn) {
	  if (this.isPending()) {
	    return this;
	  }
	  if (typeof title === 'function') {
	    fn = title;
	    title = fn.name;
	  }
	  title = '"after all" hook' + (title ? ': ' + title : '');

	  var hook = new Hook(title, fn);
	  hook.parent = this;
	  hook.timeout(this.timeout());
	  hook.retries(this.retries());
	  hook.enableTimeouts(this.enableTimeouts());
	  hook.slow(this.slow());
	  hook.ctx = this.ctx;
	  this._afterAll.push(hook);
	  this.emit('afterAll', hook);
	  return this;
	};

	/**
	 * Run `fn(test[, done])` before each test case.
	 *
	 * @api private
	 * @param {string} title
	 * @param {Function} fn
	 * @return {Suite} for chaining
	 */
	Suite.prototype.beforeEach = function (title, fn) {
	  if (this.isPending()) {
	    return this;
	  }
	  if (typeof title === 'function') {
	    fn = title;
	    title = fn.name;
	  }
	  title = '"before each" hook' + (title ? ': ' + title : '');

	  var hook = new Hook(title, fn);
	  hook.parent = this;
	  hook.timeout(this.timeout());
	  hook.retries(this.retries());
	  hook.enableTimeouts(this.enableTimeouts());
	  hook.slow(this.slow());
	  hook.ctx = this.ctx;
	  this._beforeEach.push(hook);
	  this.emit('beforeEach', hook);
	  return this;
	};

	/**
	 * Run `fn(test[, done])` after each test case.
	 *
	 * @api private
	 * @param {string} title
	 * @param {Function} fn
	 * @return {Suite} for chaining
	 */
	Suite.prototype.afterEach = function (title, fn) {
	  if (this.isPending()) {
	    return this;
	  }
	  if (typeof title === 'function') {
	    fn = title;
	    title = fn.name;
	  }
	  title = '"after each" hook' + (title ? ': ' + title : '');

	  var hook = new Hook(title, fn);
	  hook.parent = this;
	  hook.timeout(this.timeout());
	  hook.retries(this.retries());
	  hook.enableTimeouts(this.enableTimeouts());
	  hook.slow(this.slow());
	  hook.ctx = this.ctx;
	  this._afterEach.push(hook);
	  this.emit('afterEach', hook);
	  return this;
	};

	/**
	 * Add a test `suite`.
	 *
	 * @api private
	 * @param {Suite} suite
	 * @return {Suite} for chaining
	 */
	Suite.prototype.addSuite = function (suite) {
	  suite.parent = this;
	  suite.timeout(this.timeout());
	  suite.retries(this.retries());
	  suite.enableTimeouts(this.enableTimeouts());
	  suite.slow(this.slow());
	  suite.bail(this.bail());
	  this.suites.push(suite);
	  this.emit('suite', suite);
	  return this;
	};

	/**
	 * Add a `test` to this suite.
	 *
	 * @api private
	 * @param {Test} test
	 * @return {Suite} for chaining
	 */
	Suite.prototype.addTest = function (test) {
	  test.parent = this;
	  test.timeout(this.timeout());
	  test.retries(this.retries());
	  test.enableTimeouts(this.enableTimeouts());
	  test.slow(this.slow());
	  test.ctx = this.ctx;
	  this.tests.push(test);
	  this.emit('test', test);
	  return this;
	};

	/**
	 * Return the full title generated by recursively concatenating the parent's
	 * full title.
	 *
	 * @api public
	 * @return {string}
	 */
	Suite.prototype.fullTitle = function () {
	  if (this.parent) {
	    var full = this.parent.fullTitle();
	    if (full) {
	      return full + ' ' + this.title;
	    }
	  }
	  return this.title;
	};

	/**
	 * Return the total number of tests.
	 *
	 * @api public
	 * @return {number}
	 */
	Suite.prototype.total = function () {
	  return utils.reduce(this.suites, function (sum, suite) {
	    return sum + suite.total();
	  }, 0) + this.tests.length;
	};

	/**
	 * Iterates through each suite recursively to find all tests. Applies a
	 * function in the format `fn(test)`.
	 *
	 * @api private
	 * @param {Function} fn
	 * @return {Suite}
	 */
	Suite.prototype.eachTest = function (fn) {
	  utils.forEach(this.tests, fn);
	  utils.forEach(this.suites, function (suite) {
	    suite.eachTest(fn);
	  });
	  return this;
	};

	/**
	 * This will run the root suite if we happen to be running in delayed mode.
	 */
	Suite.prototype.run = function run () {
	  if (this.root) {
	    this.emit('run');
	  }
	};

	},{"./hook":7,"./ms":15,"./utils":38,"debug":2,"events":3}],36:[function(require,module,exports){
	'use strict';

	/**
	 * Module dependencies.
	 */

	var Runnable = require('./runnable');
	var create = require('lodash.create');
	var isString = require('./utils').isString;

	/**
	 * Expose `Test`.
	 */

	module.exports = Test;

	/**
	 * Initialize a new `Test` with the given `title` and callback `fn`.
	 *
	 * @api private
	 * @param {String} title
	 * @param {Function} fn
	 */
	function Test (title, fn) {
	  if (!isString(title)) {
	    throw new Error('Test `title` should be a "string" but "' + typeof title + '" was given instead.');
	  }
	  Runnable.call(this, title, fn);
	  this.pending = !fn;
	  this.type = 'test';
	}

	/**
	 * Inherit from `Runnable.prototype`.
	 */
	Test.prototype = create(Runnable.prototype, {
	  constructor: Test
	});

	Test.prototype.clone = function () {
	  var test = new Test(this.title, this.fn);
	  test.timeout(this.timeout());
	  test.slow(this.slow());
	  test.enableTimeouts(this.enableTimeouts());
	  test.retries(this.retries());
	  test.currentRetry(this.currentRetry());
	  test.globals(this.globals());
	  test.parent = this.parent;
	  test.file = this.file;
	  test.ctx = this.ctx;
	  return test;
	};

	},{"./runnable":33,"./utils":38,"lodash.create":75}],37:[function(require,module,exports){
	'use strict';

	/**
	 * Pad a `number` with a ten's place zero.
	 *
	 * @param {number} number
	 * @return {string}
	 */
	function pad(number) {
	  var n = number.toString();
	  return n.length === 1 ? '0' + n : n;
	}

	/**
	 * Turn a `date` into an ISO string.
	 *
	 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toISOString
	 *
	 * @param {Date} date
	 * @return {string}
	 */
	function toISOString(date) {
	  return date.getUTCFullYear()
	    + '-' + pad(date.getUTCMonth() + 1)
	    + '-' + pad(date.getUTCDate())
	    + 'T' + pad(date.getUTCHours())
	    + ':' + pad(date.getUTCMinutes())
	    + ':' + pad(date.getUTCSeconds())
	    + '.' + String((date.getUTCMilliseconds()/1000).toFixed(3)).slice(2, 5)
	    + 'Z';
	}

	/*
	 * Exports.
	 */

	module.exports = toISOString;

	},{}],38:[function(require,module,exports){
	(function (process,Buffer){
	'use strict';

	/* eslint-env browser */

	/**
	 * Module dependencies.
	 */

	var JSON = require('json3');
	var basename = require('path').basename;
	var debug = require('debug')('mocha:watch');
	var exists = require('fs').existsSync || require('path').existsSync;
	var glob = require('glob');
	var path = require('path');
	var join = path.join;
	var readdirSync = require('fs').readdirSync;
	var statSync = require('fs').statSync;
	var watchFile = require('fs').watchFile;
	var lstatSync = require('fs').lstatSync;
	var toISOString = require('./to-iso-string');

	/**
	 * Ignored directories.
	 */

	var ignore = ['node_modules', '.git'];

	exports.inherits = require('util').inherits;

	/**
	 * Escape special characters in the given string of html.
	 *
	 * @api private
	 * @param  {string} html
	 * @return {string}
	 */
	exports.escape = function (html) {
	  return String(html)
	    .replace(/&/g, '&amp;')
	    .replace(/"/g, '&quot;')
	    .replace(/</g, '&lt;')
	    .replace(/>/g, '&gt;');
	};

	/**
	 * Array#forEach (<=IE8)
	 *
	 * @api private
	 * @param {Array} arr
	 * @param {Function} fn
	 * @param {Object} scope
	 */
	exports.forEach = function (arr, fn, scope) {
	  for (var i = 0, l = arr.length; i < l; i++) {
	    fn.call(scope, arr[i], i);
	  }
	};

	/**
	 * Test if the given obj is type of string.
	 *
	 * @api private
	 * @param {Object} obj
	 * @return {boolean}
	 */
	exports.isString = function (obj) {
	  return typeof obj === 'string';
	};

	/**
	 * Array#map (<=IE8)
	 *
	 * @api private
	 * @param {Array} arr
	 * @param {Function} fn
	 * @param {Object} scope
	 * @return {Array}
	 */
	exports.map = function (arr, fn, scope) {
	  var result = [];
	  for (var i = 0, l = arr.length; i < l; i++) {
	    result.push(fn.call(scope, arr[i], i, arr));
	  }
	  return result;
	};

	/**
	 * Array#indexOf (<=IE8)
	 *
	 * @api private
	 * @param {Array} arr
	 * @param {Object} obj to find index of
	 * @param {number} start
	 * @return {number}
	 */
	var indexOf = exports.indexOf = function (arr, obj, start) {
	  for (var i = start || 0, l = arr.length; i < l; i++) {
	    if (arr[i] === obj) {
	      return i;
	    }
	  }
	  return -1;
	};

	/**
	 * Array#reduce (<=IE8)
	 *
	 * @api private
	 * @param {Array} arr
	 * @param {Function} fn
	 * @param {Object} val Initial value.
	 * @return {*}
	 */
	var reduce = exports.reduce = function (arr, fn, val) {
	  var rval = val;

	  for (var i = 0, l = arr.length; i < l; i++) {
	    rval = fn(rval, arr[i], i, arr);
	  }

	  return rval;
	};

	/**
	 * Array#filter (<=IE8)
	 *
	 * @api private
	 * @param {Array} arr
	 * @param {Function} fn
	 * @return {Array}
	 */
	exports.filter = function (arr, fn) {
	  var ret = [];

	  for (var i = 0, l = arr.length; i < l; i++) {
	    var val = arr[i];
	    if (fn(val, i, arr)) {
	      ret.push(val);
	    }
	  }

	  return ret;
	};

	/**
	 * Array#some (<=IE8)
	 *
	 * @api private
	 * @param {Array} arr
	 * @param {Function} fn
	 * @return {Array}
	 */
	exports.some = function (arr, fn) {
	  for (var i = 0, l = arr.length; i < l; i++) {
	    if (fn(arr[i])) {
	      return true;
	    }
	  }
	  return false;
	};

	/**
	 * Object.keys (<=IE8)
	 *
	 * @api private
	 * @param {Object} obj
	 * @return {Array} keys
	 */
	exports.keys = typeof Object.keys === 'function' ? Object.keys : function (obj) {
	  var keys = [];
	  var has = Object.prototype.hasOwnProperty; // for `window` on <=IE8

	  for (var key in obj) {
	    if (has.call(obj, key)) {
	      keys.push(key);
	    }
	  }

	  return keys;
	};

	/**
	 * Watch the given `files` for changes
	 * and invoke `fn(file)` on modification.
	 *
	 * @api private
	 * @param {Array} files
	 * @param {Function} fn
	 */
	exports.watch = function (files, fn) {
	  var options = { interval: 100 };
	  files.forEach(function (file) {
	    debug('file %s', file);
	    watchFile(file, options, function (curr, prev) {
	      if (prev.mtime < curr.mtime) {
	        fn(file);
	      }
	    });
	  });
	};

	/**
	 * Array.isArray (<=IE8)
	 *
	 * @api private
	 * @param {Object} obj
	 * @return {Boolean}
	 */
	var isArray = typeof Array.isArray === 'function' ? Array.isArray : function (obj) {
	  return Object.prototype.toString.call(obj) === '[object Array]';
	};

	exports.isArray = isArray;

	/**
	 * Buffer.prototype.toJSON polyfill.
	 *
	 * @type {Function}
	 */
	if (typeof Buffer !== 'undefined' && Buffer.prototype) {
	  Buffer.prototype.toJSON = Buffer.prototype.toJSON || function () {
	    return Array.prototype.slice.call(this, 0);
	  };
	}

	/**
	 * Ignored files.
	 *
	 * @api private
	 * @param {string} path
	 * @return {boolean}
	 */
	function ignored (path) {
	  return !~ignore.indexOf(path);
	}

	/**
	 * Lookup files in the given `dir`.
	 *
	 * @api private
	 * @param {string} dir
	 * @param {string[]} [ext=['.js']]
	 * @param {Array} [ret=[]]
	 * @return {Array}
	 */
	exports.files = function (dir, ext, ret) {
	  ret = ret || [];
	  ext = ext || ['js'];

	  var re = new RegExp('\\.(' + ext.join('|') + ')$');

	  readdirSync(dir)
	    .filter(ignored)
	    .forEach(function (path) {
	      path = join(dir, path);
	      if (lstatSync(path).isDirectory()) {
	        exports.files(path, ext, ret);
	      } else if (path.match(re)) {
	        ret.push(path);
	      }
	    });

	  return ret;
	};

	/**
	 * Compute a slug from the given `str`.
	 *
	 * @api private
	 * @param {string} str
	 * @return {string}
	 */
	exports.slug = function (str) {
	  return str
	    .toLowerCase()
	    .replace(/ +/g, '-')
	    .replace(/[^-\w]/g, '');
	};

	/**
	 * Strip the function definition from `str`, and re-indent for pre whitespace.
	 *
	 * @param {string} str
	 * @return {string}
	 */
	exports.clean = function (str) {
	  str = str
	    .replace(/\r\n?|[\n\u2028\u2029]/g, '\n').replace(/^\uFEFF/, '')
	    // (traditional)->  space/name     parameters    body     (lambda)-> parameters       body   multi-statement/single          keep body content
	    .replace(/^function(?:\s*|\s+[^(]*)\([^)]*\)\s*\{((?:.|\n)*?)\s*\}$|^\([^)]*\)\s*=>\s*(?:\{((?:.|\n)*?)\s*\}|((?:.|\n)*))$/, '$1$2$3');

	  var spaces = str.match(/^\n?( *)/)[1].length;
	  var tabs = str.match(/^\n?(\t*)/)[1].length;
	  var re = new RegExp('^\n?' + (tabs ? '\t' : ' ') + '{' + (tabs || spaces) + '}', 'gm');

	  str = str.replace(re, '');

	  return exports.trim(str);
	};

	/**
	 * Trim the given `str`.
	 *
	 * @api private
	 * @param {string} str
	 * @return {string}
	 */
	exports.trim = function (str) {
	  return str.replace(/^\s+|\s+$/g, '');
	};

	/**
	 * Parse the given `qs`.
	 *
	 * @api private
	 * @param {string} qs
	 * @return {Object}
	 */
	exports.parseQuery = function (qs) {
	  return reduce(qs.replace('?', '').split('&'), function (obj, pair) {
	    var i = pair.indexOf('=');
	    var key = pair.slice(0, i);
	    var val = pair.slice(++i);

	    // Due to how the URLSearchParams API treats spaces
	    obj[key] = decodeURIComponent(val.replace(/\+/g, '%20'));

	    return obj;
	  }, {});
	};

	/**
	 * Highlight the given string of `js`.
	 *
	 * @api private
	 * @param {string} js
	 * @return {string}
	 */
	function highlight (js) {
	  return js
	    .replace(/</g, '&lt;')
	    .replace(/>/g, '&gt;')
	    .replace(/\/\/(.*)/gm, '<span class="comment">//$1</span>')
	    .replace(/('.*?')/gm, '<span class="string">$1</span>')
	    .replace(/(\d+\.\d+)/gm, '<span class="number">$1</span>')
	    .replace(/(\d+)/gm, '<span class="number">$1</span>')
	    .replace(/\bnew[ \t]+(\w+)/gm, '<span class="keyword">new</span> <span class="init">$1</span>')
	    .replace(/\b(function|new|throw|return|var|if|else)\b/gm, '<span class="keyword">$1</span>');
	}

	/**
	 * Highlight the contents of tag `name`.
	 *
	 * @api private
	 * @param {string} name
	 */
	exports.highlightTags = function (name) {
	  var code = document.getElementById('mocha').getElementsByTagName(name);
	  for (var i = 0, len = code.length; i < len; ++i) {
	    code[i].innerHTML = highlight(code[i].innerHTML);
	  }
	};

	/**
	 * If a value could have properties, and has none, this function is called,
	 * which returns a string representation of the empty value.
	 *
	 * Functions w/ no properties return `'[Function]'`
	 * Arrays w/ length === 0 return `'[]'`
	 * Objects w/ no properties return `'{}'`
	 * All else: return result of `value.toString()`
	 *
	 * @api private
	 * @param {*} value The value to inspect.
	 * @param {string} typeHint The type of the value
	 * @returns {string}
	 */
	function emptyRepresentation (value, typeHint) {
	  switch (typeHint) {
	    case 'function':
	      return '[Function]';
	    case 'object':
	      return '{}';
	    case 'array':
	      return '[]';
	    default:
	      return value.toString();
	  }
	}

	/**
	 * Takes some variable and asks `Object.prototype.toString()` what it thinks it
	 * is.
	 *
	 * @api private
	 * @see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/toString
	 * @param {*} value The value to test.
	 * @returns {string} Computed type
	 * @example
	 * type({}) // 'object'
	 * type([]) // 'array'
	 * type(1) // 'number'
	 * type(false) // 'boolean'
	 * type(Infinity) // 'number'
	 * type(null) // 'null'
	 * type(new Date()) // 'date'
	 * type(/foo/) // 'regexp'
	 * type('type') // 'string'
	 * type(global) // 'global'
	 * type(new String('foo') // 'object'
	 */
	var type = exports.type = function type (value) {
	  if (value === undefined) {
	    return 'undefined';
	  } else if (value === null) {
	    return 'null';
	  } else if (typeof Buffer !== 'undefined' && Buffer.isBuffer(value)) {
	    return 'buffer';
	  }
	  return Object.prototype.toString.call(value)
	    .replace(/^\[.+\s(.+?)]$/, '$1')
	    .toLowerCase();
	};

	/**
	 * Stringify `value`. Different behavior depending on type of value:
	 *
	 * - If `value` is undefined or null, return `'[undefined]'` or `'[null]'`, respectively.
	 * - If `value` is not an object, function or array, return result of `value.toString()` wrapped in double-quotes.
	 * - If `value` is an *empty* object, function, or array, return result of function
	 *   {@link emptyRepresentation}.
	 * - If `value` has properties, call {@link exports.canonicalize} on it, then return result of
	 *   JSON.stringify().
	 *
	 * @api private
	 * @see exports.type
	 * @param {*} value
	 * @return {string}
	 */
	exports.stringify = function (value) {
	  var typeHint = type(value);

	  if (!~indexOf(['object', 'array', 'function'], typeHint)) {
	    if (typeHint === 'buffer') {
	      var json = value.toJSON();
	      // Based on the toJSON result
	      return jsonStringify(json.data && json.type ? json.data : json, 2)
	        .replace(/,(\n|$)/g, '$1');
	    }

	    // IE7/IE8 has a bizarre String constructor; needs to be coerced
	    // into an array and back to obj.
	    if (typeHint === 'string' && typeof value === 'object') {
	      value = reduce(value.split(''), function (acc, char, idx) {
	        acc[idx] = char;
	        return acc;
	      }, {});
	      typeHint = 'object';
	    } else {
	      return jsonStringify(value);
	    }
	  }

	  for (var prop in value) {
	    if (Object.prototype.hasOwnProperty.call(value, prop)) {
	      return jsonStringify(exports.canonicalize(value, null, typeHint), 2).replace(/,(\n|$)/g, '$1');
	    }
	  }

	  return emptyRepresentation(value, typeHint);
	};

	/**
	 * like JSON.stringify but more sense.
	 *
	 * @api private
	 * @param {Object}  object
	 * @param {number=} spaces
	 * @param {number=} depth
	 * @returns {*}
	 */
	function jsonStringify (object, spaces, depth) {
	  if (typeof spaces === 'undefined') {
	    // primitive types
	    return _stringify(object);
	  }

	  depth = depth || 1;
	  var space = spaces * depth;
	  var str = isArray(object) ? '[' : '{';
	  var end = isArray(object) ? ']' : '}';
	  var length = typeof object.length === 'number' ? object.length : exports.keys(object).length;
	  // `.repeat()` polyfill
	  function repeat (s, n) {
	    return new Array(n).join(s);
	  }

	  function _stringify (val) {
	    switch (type(val)) {
	      case 'null':
	      case 'undefined':
	        val = '[' + val + ']';
	        break;
	      case 'array':
	      case 'object':
	        val = jsonStringify(val, spaces, depth + 1);
	        break;
	      case 'boolean':
	      case 'regexp':
	      case 'symbol':
	      case 'number':
	        val = val === 0 && (1 / val) === -Infinity // `-0`
	          ? '-0'
	          : val.toString();
	        break;
	      case 'date':
	        var sDate;
	        if (isNaN(val.getTime())) { // Invalid date
	          sDate = val.toString();
	        } else {
	          sDate = val.toISOString ? val.toISOString() : toISOString(val);
	        }
	        val = '[Date: ' + sDate + ']';
	        break;
	      case 'buffer':
	        var json = val.toJSON();
	        // Based on the toJSON result
	        json = json.data && json.type ? json.data : json;
	        val = '[Buffer: ' + jsonStringify(json, 2, depth + 1) + ']';
	        break;
	      default:
	        val = (val === '[Function]' || val === '[Circular]')
	          ? val
	          : JSON.stringify(val); // string
	    }
	    return val;
	  }

	  for (var i in object) {
	    if (!Object.prototype.hasOwnProperty.call(object, i)) {
	      continue; // not my business
	    }
	    --length;
	    str += '\n ' + repeat(' ', space) +
	      (isArray(object) ? '' : '"' + i + '": ') + // key
	      _stringify(object[i]) +                    // value
	      (length ? ',' : '');                       // comma
	  }

	  return str +
	    // [], {}
	    (str.length !== 1 ? '\n' + repeat(' ', --space) + end : end);
	}

	/**
	 * Test if a value is a buffer.
	 *
	 * @api private
	 * @param {*} value The value to test.
	 * @return {boolean} True if `value` is a buffer, otherwise false
	 */
	exports.isBuffer = function (value) {
	  return typeof Buffer !== 'undefined' && Buffer.isBuffer(value);
	};

	/**
	 * Return a new Thing that has the keys in sorted order. Recursive.
	 *
	 * If the Thing...
	 * - has already been seen, return string `'[Circular]'`
	 * - is `undefined`, return string `'[undefined]'`
	 * - is `null`, return value `null`
	 * - is some other primitive, return the value
	 * - is not a primitive or an `Array`, `Object`, or `Function`, return the value of the Thing's `toString()` method
	 * - is a non-empty `Array`, `Object`, or `Function`, return the result of calling this function again.
	 * - is an empty `Array`, `Object`, or `Function`, return the result of calling `emptyRepresentation()`
	 *
	 * @api private
	 * @see {@link exports.stringify}
	 * @param {*} value Thing to inspect.  May or may not have properties.
	 * @param {Array} [stack=[]] Stack of seen values
	 * @param {string} [typeHint] Type hint
	 * @return {(Object|Array|Function|string|undefined)}
	 */
	exports.canonicalize = function canonicalize (value, stack, typeHint) {
	  var canonicalizedObj;
	  /* eslint-disable no-unused-vars */
	  var prop;
	  /* eslint-enable no-unused-vars */
	  typeHint = typeHint || type(value);
	  function withStack (value, fn) {
	    stack.push(value);
	    fn();
	    stack.pop();
	  }

	  stack = stack || [];

	  if (indexOf(stack, value) !== -1) {
	    return '[Circular]';
	  }

	  switch (typeHint) {
	    case 'undefined':
	    case 'buffer':
	    case 'null':
	      canonicalizedObj = value;
	      break;
	    case 'array':
	      withStack(value, function () {
	        canonicalizedObj = exports.map(value, function (item) {
	          return exports.canonicalize(item, stack);
	        });
	      });
	      break;
	    case 'function':
	      /* eslint-disable guard-for-in */
	      for (prop in value) {
	        canonicalizedObj = {};
	        break;
	      }
	      /* eslint-enable guard-for-in */
	      if (!canonicalizedObj) {
	        canonicalizedObj = emptyRepresentation(value, typeHint);
	        break;
	      }
	    /* falls through */
	    case 'object':
	      canonicalizedObj = canonicalizedObj || {};
	      withStack(value, function () {
	        exports.forEach(exports.keys(value).sort(), function (key) {
	          canonicalizedObj[key] = exports.canonicalize(value[key], stack);
	        });
	      });
	      break;
	    case 'date':
	    case 'number':
	    case 'regexp':
	    case 'boolean':
	    case 'symbol':
	      canonicalizedObj = value;
	      break;
	    default:
	      canonicalizedObj = value + '';
	  }

	  return canonicalizedObj;
	};

	/**
	 * Lookup file names at the given `path`.
	 *
	 * @api public
	 * @param {string} path Base path to start searching from.
	 * @param {string[]} extensions File extensions to look for.
	 * @param {boolean} recursive Whether or not to recurse into subdirectories.
	 * @return {string[]} An array of paths.
	 */
	exports.lookupFiles = function lookupFiles (path, extensions, recursive) {
	  var files = [];
	  var re = new RegExp('\\.(' + extensions.join('|') + ')$');

	  if (!exists(path)) {
	    if (exists(path + '.js')) {
	      path += '.js';
	    } else {
	      files = glob.sync(path);
	      if (!files.length) {
	        throw new Error("cannot resolve path (or pattern) '" + path + "'");
	      }
	      return files;
	    }
	  }

	  try {
	    var stat = statSync(path);
	    if (stat.isFile()) {
	      return path;
	    }
	  } catch (err) {
	    // ignore error
	    return;
	  }

	  readdirSync(path).forEach(function (file) {
	    file = join(path, file);
	    try {
	      var stat = statSync(file);
	      if (stat.isDirectory()) {
	        if (recursive) {
	          files = files.concat(lookupFiles(file, extensions, recursive));
	        }
	        return;
	      }
	    } catch (err) {
	      // ignore error
	      return;
	    }
	    if (!stat.isFile() || !re.test(file) || basename(file)[0] === '.') {
	      return;
	    }
	    files.push(file);
	  });

	  return files;
	};

	/**
	 * Generate an undefined error with a message warning the user.
	 *
	 * @return {Error}
	 */

	exports.undefinedError = function () {
	  return new Error('Caught undefined error, did you throw without specifying what?');
	};

	/**
	 * Generate an undefined error if `err` is not defined.
	 *
	 * @param {Error} err
	 * @return {Error}
	 */

	exports.getError = function (err) {
	  return err || exports.undefinedError();
	};

	/**
	 * @summary
	 * This Filter based on `mocha-clean` module.(see: `github.com/rstacruz/mocha-clean`)
	 * @description
	 * When invoking this function you get a filter function that get the Error.stack as an input,
	 * and return a prettify output.
	 * (i.e: strip Mocha and internal node functions from stack trace).
	 * @returns {Function}
	 */
	exports.stackTraceFilter = function () {
	  // TODO: Replace with `process.browser`
	  var is = typeof document === 'undefined' ? { node: true } : { browser: true };
	  var slash = path.sep;
	  var cwd;
	  if (is.node) {
	    cwd = process.cwd() + slash;
	  } else {
	    cwd = (typeof location === 'undefined'
	      ? window.location
	      : location).href.replace(/\/[^/]*$/, '/');
	    slash = '/';
	  }

	  function isMochaInternal (line) {
	    return (~line.indexOf('node_modules' + slash + 'mocha' + slash)) ||
	      (~line.indexOf('node_modules' + slash + 'mocha.js')) ||
	      (~line.indexOf('bower_components' + slash + 'mocha.js')) ||
	      (~line.indexOf(slash + 'mocha.js'));
	  }

	  function isNodeInternal (line) {
	    return (~line.indexOf('(timers.js:')) ||
	      (~line.indexOf('(events.js:')) ||
	      (~line.indexOf('(node.js:')) ||
	      (~line.indexOf('(module.js:')) ||
	      (~line.indexOf('GeneratorFunctionPrototype.next (native)')) ||
	      false;
	  }

	  return function (stack) {
	    stack = stack.split('\n');

	    stack = reduce(stack, function (list, line) {
	      if (isMochaInternal(line)) {
	        return list;
	      }

	      if (is.node && isNodeInternal(line)) {
	        return list;
	      }

	      // Clean up cwd(absolute)
	      if (/\(?.+:\d+:\d+\)?$/.test(line)) {
	        line = line.replace(cwd, '');
	      }

	      list.push(line);
	      return list;
	    }, []);

	    return stack.join('\n');
	  };
	};

	/**
	 * Crude, but effective.
	 * @api
	 * @param {*} value
	 * @returns {boolean} Whether or not `value` is a Promise
	 */
	exports.isPromise = function isPromise (value) {
	  return typeof value === 'object' && typeof value.then === 'function';
	};

	/**
	 * It's a noop.
	 * @api
	 */
	exports.noop = function () {};

	}).call(this,require('_process'),require("buffer").Buffer)
	},{"./to-iso-string":37,"_process":82,"buffer":44,"debug":2,"fs":42,"glob":42,"json3":69,"path":42,"util":99}],39:[function(require,module,exports){
	'use strict'

	exports.byteLength = byteLength
	exports.toByteArray = toByteArray
	exports.fromByteArray = fromByteArray

	var lookup = []
	var revLookup = []
	var Arr = typeof Uint8Array !== 'undefined' ? Uint8Array : Array

	var code = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
	for (var i = 0, len = code.length; i < len; ++i) {
	  lookup[i] = code[i]
	  revLookup[code.charCodeAt(i)] = i
	}

	revLookup['-'.charCodeAt(0)] = 62
	revLookup['_'.charCodeAt(0)] = 63

	function placeHoldersCount (b64) {
	  var len = b64.length
	  if (len % 4 > 0) {
	    throw new Error('Invalid string. Length must be a multiple of 4')
	  }

	  // the number of equal signs (place holders)
	  // if there are two placeholders, than the two characters before it
	  // represent one byte
	  // if there is only one, then the three characters before it represent 2 bytes
	  // this is just a cheap hack to not do indexOf twice
	  return b64[len - 2] === '=' ? 2 : b64[len - 1] === '=' ? 1 : 0
	}

	function byteLength (b64) {
	  // base64 is 4/3 + up to two characters of the original data
	  return b64.length * 3 / 4 - placeHoldersCount(b64)
	}

	function toByteArray (b64) {
	  var i, j, l, tmp, placeHolders, arr
	  var len = b64.length
	  placeHolders = placeHoldersCount(b64)

	  arr = new Arr(len * 3 / 4 - placeHolders)

	  // if there are placeholders, only get up to the last complete 4 chars
	  l = placeHolders > 0 ? len - 4 : len

	  var L = 0

	  for (i = 0, j = 0; i < l; i += 4, j += 3) {
	    tmp = (revLookup[b64.charCodeAt(i)] << 18) | (revLookup[b64.charCodeAt(i + 1)] << 12) | (revLookup[b64.charCodeAt(i + 2)] << 6) | revLookup[b64.charCodeAt(i + 3)]
	    arr[L++] = (tmp >> 16) & 0xFF
	    arr[L++] = (tmp >> 8) & 0xFF
	    arr[L++] = tmp & 0xFF
	  }

	  if (placeHolders === 2) {
	    tmp = (revLookup[b64.charCodeAt(i)] << 2) | (revLookup[b64.charCodeAt(i + 1)] >> 4)
	    arr[L++] = tmp & 0xFF
	  } else if (placeHolders === 1) {
	    tmp = (revLookup[b64.charCodeAt(i)] << 10) | (revLookup[b64.charCodeAt(i + 1)] << 4) | (revLookup[b64.charCodeAt(i + 2)] >> 2)
	    arr[L++] = (tmp >> 8) & 0xFF
	    arr[L++] = tmp & 0xFF
	  }

	  return arr
	}

	function tripletToBase64 (num) {
	  return lookup[num >> 18 & 0x3F] + lookup[num >> 12 & 0x3F] + lookup[num >> 6 & 0x3F] + lookup[num & 0x3F]
	}

	function encodeChunk (uint8, start, end) {
	  var tmp
	  var output = []
	  for (var i = start; i < end; i += 3) {
	    tmp = (uint8[i] << 16) + (uint8[i + 1] << 8) + (uint8[i + 2])
	    output.push(tripletToBase64(tmp))
	  }
	  return output.join('')
	}

	function fromByteArray (uint8) {
	  var tmp
	  var len = uint8.length
	  var extraBytes = len % 3 // if we have 1 byte left, pad 2 bytes
	  var output = ''
	  var parts = []
	  var maxChunkLength = 16383 // must be multiple of 3

	  // go through the array every three bytes, we'll deal with trailing stuff later
	  for (var i = 0, len2 = len - extraBytes; i < len2; i += maxChunkLength) {
	    parts.push(encodeChunk(uint8, i, (i + maxChunkLength) > len2 ? len2 : (i + maxChunkLength)))
	  }

	  // pad the end with zeros, but make sure to not forget the extra bytes
	  if (extraBytes === 1) {
	    tmp = uint8[len - 1]
	    output += lookup[tmp >> 2]
	    output += lookup[(tmp << 4) & 0x3F]
	    output += '=='
	  } else if (extraBytes === 2) {
	    tmp = (uint8[len - 2] << 8) + (uint8[len - 1])
	    output += lookup[tmp >> 10]
	    output += lookup[(tmp >> 4) & 0x3F]
	    output += lookup[(tmp << 2) & 0x3F]
	    output += '='
	  }

	  parts.push(output)

	  return parts.join('')
	}

	},{}],40:[function(require,module,exports){

	},{}],41:[function(require,module,exports){
	(function (process){
	var WritableStream = require('stream').Writable
	var inherits = require('util').inherits

	module.exports = BrowserStdout


	inherits(BrowserStdout, WritableStream)

	function BrowserStdout(opts) {
	  if (!(this instanceof BrowserStdout)) return new BrowserStdout(opts)

	  opts = opts || {}
	  WritableStream.call(this, opts)
	  this.label = (opts.label !== undefined) ? opts.label : 'stdout'
	}

	BrowserStdout.prototype._write = function(chunks, encoding, cb) {
	  var output = chunks.toString ? chunks.toString() : chunks
	  if (this.label === false) {
	    console.log(output)
	  } else {
	    console.log(this.label+':', output)
	  }
	  process.nextTick(cb)
	}

	}).call(this,require('_process'))
	},{"_process":82,"stream":94,"util":99}],42:[function(require,module,exports){
	arguments[4][40][0].apply(exports,arguments)
	},{"dup":40}],43:[function(require,module,exports){
	(function (global){
	'use strict';

	var buffer = require('buffer');
	var Buffer = buffer.Buffer;
	var SlowBuffer = buffer.SlowBuffer;
	var MAX_LEN = buffer.kMaxLength || 2147483647;
	exports.alloc = function alloc(size, fill, encoding) {
	  if (typeof Buffer.alloc === 'function') {
	    return Buffer.alloc(size, fill, encoding);
	  }
	  if (typeof encoding === 'number') {
	    throw new TypeError('encoding must not be number');
	  }
	  if (typeof size !== 'number') {
	    throw new TypeError('size must be a number');
	  }
	  if (size > MAX_LEN) {
	    throw new RangeError('size is too large');
	  }
	  var enc = encoding;
	  var _fill = fill;
	  if (_fill === undefined) {
	    enc = undefined;
	    _fill = 0;
	  }
	  var buf = new Buffer(size);
	  if (typeof _fill === 'string') {
	    var fillBuf = new Buffer(_fill, enc);
	    var flen = fillBuf.length;
	    var i = -1;
	    while (++i < size) {
	      buf[i] = fillBuf[i % flen];
	    }
	  } else {
	    buf.fill(_fill);
	  }
	  return buf;
	}
	exports.allocUnsafe = function allocUnsafe(size) {
	  if (typeof Buffer.allocUnsafe === 'function') {
	    return Buffer.allocUnsafe(size);
	  }
	  if (typeof size !== 'number') {
	    throw new TypeError('size must be a number');
	  }
	  if (size > MAX_LEN) {
	    throw new RangeError('size is too large');
	  }
	  return new Buffer(size);
	}
	exports.from = function from(value, encodingOrOffset, length) {
	  if (typeof Buffer.from === 'function' && (!global.Uint8Array || Uint8Array.from !== Buffer.from)) {
	    return Buffer.from(value, encodingOrOffset, length);
	  }
	  if (typeof value === 'number') {
	    throw new TypeError('"value" argument must not be a number');
	  }
	  if (typeof value === 'string') {
	    return new Buffer(value, encodingOrOffset);
	  }
	  if (typeof ArrayBuffer !== 'undefined' && value instanceof ArrayBuffer) {
	    var offset = encodingOrOffset;
	    if (arguments.length === 1) {
	      return new Buffer(value);
	    }
	    if (typeof offset === 'undefined') {
	      offset = 0;
	    }
	    var len = length;
	    if (typeof len === 'undefined') {
	      len = value.byteLength - offset;
	    }
	    if (offset >= value.byteLength) {
	      throw new RangeError('\'offset\' is out of bounds');
	    }
	    if (len > value.byteLength - offset) {
	      throw new RangeError('\'length\' is out of bounds');
	    }
	    return new Buffer(value.slice(offset, offset + len));
	  }
	  if (Buffer.isBuffer(value)) {
	    var out = new Buffer(value.length);
	    value.copy(out, 0, 0, value.length);
	    return out;
	  }
	  if (value) {
	    if (Array.isArray(value) || (typeof ArrayBuffer !== 'undefined' && value.buffer instanceof ArrayBuffer) || 'length' in value) {
	      return new Buffer(value);
	    }
	    if (value.type === 'Buffer' && Array.isArray(value.data)) {
	      return new Buffer(value.data);
	    }
	  }

	  throw new TypeError('First argument must be a string, Buffer, ' + 'ArrayBuffer, Array, or array-like object.');
	}
	exports.allocUnsafeSlow = function allocUnsafeSlow(size) {
	  if (typeof Buffer.allocUnsafeSlow === 'function') {
	    return Buffer.allocUnsafeSlow(size);
	  }
	  if (typeof size !== 'number') {
	    throw new TypeError('size must be a number');
	  }
	  if (size >= MAX_LEN) {
	    throw new RangeError('size is too large');
	  }
	  return new SlowBuffer(size);
	}

	}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"buffer":44}],44:[function(require,module,exports){
	(function (global){
	/*!
	 * The buffer module from node.js, for the browser.
	 *
	 * @author   Feross Aboukhadijeh <feross@feross.org> <http://feross.org>
	 * @license  MIT
	 */
	/* eslint-disable no-proto */

	'use strict'

	var base64 = require('base64-js')
	var ieee754 = require('ieee754')
	var isArray = require('isarray')

	exports.Buffer = Buffer
	exports.SlowBuffer = SlowBuffer
	exports.INSPECT_MAX_BYTES = 50

	/**
	 * If `Buffer.TYPED_ARRAY_SUPPORT`:
	 *   === true    Use Uint8Array implementation (fastest)
	 *   === false   Use Object implementation (most compatible, even IE6)
	 *
	 * Browsers that support typed arrays are IE 10+, Firefox 4+, Chrome 7+, Safari 5.1+,
	 * Opera 11.6+, iOS 4.2+.
	 *
	 * Due to various browser bugs, sometimes the Object implementation will be used even
	 * when the browser supports typed arrays.
	 *
	 * Note:
	 *
	 *   - Firefox 4-29 lacks support for adding new properties to `Uint8Array` instances,
	 *     See: https://bugzilla.mozilla.org/show_bug.cgi?id=695438.
	 *
	 *   - Chrome 9-10 is missing the `TypedArray.prototype.subarray` function.
	 *
	 *   - IE10 has a broken `TypedArray.prototype.subarray` function which returns arrays of
	 *     incorrect length in some situations.

	 * We detect these buggy browsers and set `Buffer.TYPED_ARRAY_SUPPORT` to `false` so they
	 * get the Object implementation, which is slower but behaves correctly.
	 */
	Buffer.TYPED_ARRAY_SUPPORT = global.TYPED_ARRAY_SUPPORT !== undefined
	  ? global.TYPED_ARRAY_SUPPORT
	  : typedArraySupport()

	/*
	 * Export kMaxLength after typed array support is determined.
	 */
	exports.kMaxLength = kMaxLength()

	function typedArraySupport () {
	  try {
	    var arr = new Uint8Array(1)
	    arr.__proto__ = {__proto__: Uint8Array.prototype, foo: function () { return 42 }}
	    return arr.foo() === 42 && // typed array instances can be augmented
	        typeof arr.subarray === 'function' && // chrome 9-10 lack `subarray`
	        arr.subarray(1, 1).byteLength === 0 // ie10 has broken `subarray`
	  } catch (e) {
	    return false
	  }
	}

	function kMaxLength () {
	  return Buffer.TYPED_ARRAY_SUPPORT
	    ? 0x7fffffff
	    : 0x3fffffff
	}

	function createBuffer (that, length) {
	  if (kMaxLength() < length) {
	    throw new RangeError('Invalid typed array length')
	  }
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    // Return an augmented `Uint8Array` instance, for best performance
	    that = new Uint8Array(length)
	    that.__proto__ = Buffer.prototype
	  } else {
	    // Fallback: Return an object instance of the Buffer class
	    if (that === null) {
	      that = new Buffer(length)
	    }
	    that.length = length
	  }

	  return that
	}

	/**
	 * The Buffer constructor returns instances of `Uint8Array` that have their
	 * prototype changed to `Buffer.prototype`. Furthermore, `Buffer` is a subclass of
	 * `Uint8Array`, so the returned instances will have all the node `Buffer` methods
	 * and the `Uint8Array` methods. Square bracket notation works as expected -- it
	 * returns a single octet.
	 *
	 * The `Uint8Array` prototype remains unmodified.
	 */

	function Buffer (arg, encodingOrOffset, length) {
	  if (!Buffer.TYPED_ARRAY_SUPPORT && !(this instanceof Buffer)) {
	    return new Buffer(arg, encodingOrOffset, length)
	  }

	  // Common case.
	  if (typeof arg === 'number') {
	    if (typeof encodingOrOffset === 'string') {
	      throw new Error(
	        'If encoding is specified then the first argument must be a string'
	      )
	    }
	    return allocUnsafe(this, arg)
	  }
	  return from(this, arg, encodingOrOffset, length)
	}

	Buffer.poolSize = 8192 // not used by this implementation

	// TODO: Legacy, not needed anymore. Remove in next major version.
	Buffer._augment = function (arr) {
	  arr.__proto__ = Buffer.prototype
	  return arr
	}

	function from (that, value, encodingOrOffset, length) {
	  if (typeof value === 'number') {
	    throw new TypeError('"value" argument must not be a number')
	  }

	  if (typeof ArrayBuffer !== 'undefined' && value instanceof ArrayBuffer) {
	    return fromArrayBuffer(that, value, encodingOrOffset, length)
	  }

	  if (typeof value === 'string') {
	    return fromString(that, value, encodingOrOffset)
	  }

	  return fromObject(that, value)
	}

	/**
	 * Functionally equivalent to Buffer(arg, encoding) but throws a TypeError
	 * if value is a number.
	 * Buffer.from(str[, encoding])
	 * Buffer.from(array)
	 * Buffer.from(buffer)
	 * Buffer.from(arrayBuffer[, byteOffset[, length]])
	 **/
	Buffer.from = function (value, encodingOrOffset, length) {
	  return from(null, value, encodingOrOffset, length)
	}

	if (Buffer.TYPED_ARRAY_SUPPORT) {
	  Buffer.prototype.__proto__ = Uint8Array.prototype
	  Buffer.__proto__ = Uint8Array
	  if (typeof Symbol !== 'undefined' && Symbol.species &&
	      Buffer[Symbol.species] === Buffer) {
	    // Fix subarray() in ES2016. See: https://github.com/feross/buffer/pull/97
	    Object.defineProperty(Buffer, Symbol.species, {
	      value: null,
	      configurable: true
	    })
	  }
	}

	function assertSize (size) {
	  if (typeof size !== 'number') {
	    throw new TypeError('"size" argument must be a number')
	  } else if (size < 0) {
	    throw new RangeError('"size" argument must not be negative')
	  }
	}

	function alloc (that, size, fill, encoding) {
	  assertSize(size)
	  if (size <= 0) {
	    return createBuffer(that, size)
	  }
	  if (fill !== undefined) {
	    // Only pay attention to encoding if it's a string. This
	    // prevents accidentally sending in a number that would
	    // be interpretted as a start offset.
	    return typeof encoding === 'string'
	      ? createBuffer(that, size).fill(fill, encoding)
	      : createBuffer(that, size).fill(fill)
	  }
	  return createBuffer(that, size)
	}

	/**
	 * Creates a new filled Buffer instance.
	 * alloc(size[, fill[, encoding]])
	 **/
	Buffer.alloc = function (size, fill, encoding) {
	  return alloc(null, size, fill, encoding)
	}

	function allocUnsafe (that, size) {
	  assertSize(size)
	  that = createBuffer(that, size < 0 ? 0 : checked(size) | 0)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) {
	    for (var i = 0; i < size; ++i) {
	      that[i] = 0
	    }
	  }
	  return that
	}

	/**
	 * Equivalent to Buffer(num), by default creates a non-zero-filled Buffer instance.
	 * */
	Buffer.allocUnsafe = function (size) {
	  return allocUnsafe(null, size)
	}
	/**
	 * Equivalent to SlowBuffer(num), by default creates a non-zero-filled Buffer instance.
	 */
	Buffer.allocUnsafeSlow = function (size) {
	  return allocUnsafe(null, size)
	}

	function fromString (that, string, encoding) {
	  if (typeof encoding !== 'string' || encoding === '') {
	    encoding = 'utf8'
	  }

	  if (!Buffer.isEncoding(encoding)) {
	    throw new TypeError('"encoding" must be a valid string encoding')
	  }

	  var length = byteLength(string, encoding) | 0
	  that = createBuffer(that, length)

	  var actual = that.write(string, encoding)

	  if (actual !== length) {
	    // Writing a hex string, for example, that contains invalid characters will
	    // cause everything after the first invalid character to be ignored. (e.g.
	    // 'abxxcd' will be treated as 'ab')
	    that = that.slice(0, actual)
	  }

	  return that
	}

	function fromArrayLike (that, array) {
	  var length = array.length < 0 ? 0 : checked(array.length) | 0
	  that = createBuffer(that, length)
	  for (var i = 0; i < length; i += 1) {
	    that[i] = array[i] & 255
	  }
	  return that
	}

	function fromArrayBuffer (that, array, byteOffset, length) {
	  array.byteLength // this throws if `array` is not a valid ArrayBuffer

	  if (byteOffset < 0 || array.byteLength < byteOffset) {
	    throw new RangeError('\'offset\' is out of bounds')
	  }

	  if (array.byteLength < byteOffset + (length || 0)) {
	    throw new RangeError('\'length\' is out of bounds')
	  }

	  if (byteOffset === undefined && length === undefined) {
	    array = new Uint8Array(array)
	  } else if (length === undefined) {
	    array = new Uint8Array(array, byteOffset)
	  } else {
	    array = new Uint8Array(array, byteOffset, length)
	  }

	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    // Return an augmented `Uint8Array` instance, for best performance
	    that = array
	    that.__proto__ = Buffer.prototype
	  } else {
	    // Fallback: Return an object instance of the Buffer class
	    that = fromArrayLike(that, array)
	  }
	  return that
	}

	function fromObject (that, obj) {
	  if (Buffer.isBuffer(obj)) {
	    var len = checked(obj.length) | 0
	    that = createBuffer(that, len)

	    if (that.length === 0) {
	      return that
	    }

	    obj.copy(that, 0, 0, len)
	    return that
	  }

	  if (obj) {
	    if ((typeof ArrayBuffer !== 'undefined' &&
	        obj.buffer instanceof ArrayBuffer) || 'length' in obj) {
	      if (typeof obj.length !== 'number' || isnan(obj.length)) {
	        return createBuffer(that, 0)
	      }
	      return fromArrayLike(that, obj)
	    }

	    if (obj.type === 'Buffer' && isArray(obj.data)) {
	      return fromArrayLike(that, obj.data)
	    }
	  }

	  throw new TypeError('First argument must be a string, Buffer, ArrayBuffer, Array, or array-like object.')
	}

	function checked (length) {
	  // Note: cannot use `length < kMaxLength()` here because that fails when
	  // length is NaN (which is otherwise coerced to zero.)
	  if (length >= kMaxLength()) {
	    throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
	                         'size: 0x' + kMaxLength().toString(16) + ' bytes')
	  }
	  return length | 0
	}

	function SlowBuffer (length) {
	  if (+length != length) { // eslint-disable-line eqeqeq
	    length = 0
	  }
	  return Buffer.alloc(+length)
	}

	Buffer.isBuffer = function isBuffer (b) {
	  return !!(b != null && b._isBuffer)
	}

	Buffer.compare = function compare (a, b) {
	  if (!Buffer.isBuffer(a) || !Buffer.isBuffer(b)) {
	    throw new TypeError('Arguments must be Buffers')
	  }

	  if (a === b) return 0

	  var x = a.length
	  var y = b.length

	  for (var i = 0, len = Math.min(x, y); i < len; ++i) {
	    if (a[i] !== b[i]) {
	      x = a[i]
	      y = b[i]
	      break
	    }
	  }

	  if (x < y) return -1
	  if (y < x) return 1
	  return 0
	}

	Buffer.isEncoding = function isEncoding (encoding) {
	  switch (String(encoding).toLowerCase()) {
	    case 'hex':
	    case 'utf8':
	    case 'utf-8':
	    case 'ascii':
	    case 'latin1':
	    case 'binary':
	    case 'base64':
	    case 'ucs2':
	    case 'ucs-2':
	    case 'utf16le':
	    case 'utf-16le':
	      return true
	    default:
	      return false
	  }
	}

	Buffer.concat = function concat (list, length) {
	  if (!isArray(list)) {
	    throw new TypeError('"list" argument must be an Array of Buffers')
	  }

	  if (list.length === 0) {
	    return Buffer.alloc(0)
	  }

	  var i
	  if (length === undefined) {
	    length = 0
	    for (i = 0; i < list.length; ++i) {
	      length += list[i].length
	    }
	  }

	  var buffer = Buffer.allocUnsafe(length)
	  var pos = 0
	  for (i = 0; i < list.length; ++i) {
	    var buf = list[i]
	    if (!Buffer.isBuffer(buf)) {
	      throw new TypeError('"list" argument must be an Array of Buffers')
	    }
	    buf.copy(buffer, pos)
	    pos += buf.length
	  }
	  return buffer
	}

	function byteLength (string, encoding) {
	  if (Buffer.isBuffer(string)) {
	    return string.length
	  }
	  if (typeof ArrayBuffer !== 'undefined' && typeof ArrayBuffer.isView === 'function' &&
	      (ArrayBuffer.isView(string) || string instanceof ArrayBuffer)) {
	    return string.byteLength
	  }
	  if (typeof string !== 'string') {
	    string = '' + string
	  }

	  var len = string.length
	  if (len === 0) return 0

	  // Use a for loop to avoid recursion
	  var loweredCase = false
	  for (;;) {
	    switch (encoding) {
	      case 'ascii':
	      case 'latin1':
	      case 'binary':
	        return len
	      case 'utf8':
	      case 'utf-8':
	      case undefined:
	        return utf8ToBytes(string).length
	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return len * 2
	      case 'hex':
	        return len >>> 1
	      case 'base64':
	        return base64ToBytes(string).length
	      default:
	        if (loweredCase) return utf8ToBytes(string).length // assume utf8
	        encoding = ('' + encoding).toLowerCase()
	        loweredCase = true
	    }
	  }
	}
	Buffer.byteLength = byteLength

	function slowToString (encoding, start, end) {
	  var loweredCase = false

	  // No need to verify that "this.length <= MAX_UINT32" since it's a read-only
	  // property of a typed array.

	  // This behaves neither like String nor Uint8Array in that we set start/end
	  // to their upper/lower bounds if the value passed is out of range.
	  // undefined is handled specially as per ECMA-262 6th Edition,
	  // Section 13.3.3.7 Runtime Semantics: KeyedBindingInitialization.
	  if (start === undefined || start < 0) {
	    start = 0
	  }
	  // Return early if start > this.length. Done here to prevent potential uint32
	  // coercion fail below.
	  if (start > this.length) {
	    return ''
	  }

	  if (end === undefined || end > this.length) {
	    end = this.length
	  }

	  if (end <= 0) {
	    return ''
	  }

	  // Force coersion to uint32. This will also coerce falsey/NaN values to 0.
	  end >>>= 0
	  start >>>= 0

	  if (end <= start) {
	    return ''
	  }

	  if (!encoding) encoding = 'utf8'

	  while (true) {
	    switch (encoding) {
	      case 'hex':
	        return hexSlice(this, start, end)

	      case 'utf8':
	      case 'utf-8':
	        return utf8Slice(this, start, end)

	      case 'ascii':
	        return asciiSlice(this, start, end)

	      case 'latin1':
	      case 'binary':
	        return latin1Slice(this, start, end)

	      case 'base64':
	        return base64Slice(this, start, end)

	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return utf16leSlice(this, start, end)

	      default:
	        if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
	        encoding = (encoding + '').toLowerCase()
	        loweredCase = true
	    }
	  }
	}

	// The property is used by `Buffer.isBuffer` and `is-buffer` (in Safari 5-7) to detect
	// Buffer instances.
	Buffer.prototype._isBuffer = true

	function swap (b, n, m) {
	  var i = b[n]
	  b[n] = b[m]
	  b[m] = i
	}

	Buffer.prototype.swap16 = function swap16 () {
	  var len = this.length
	  if (len % 2 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 16-bits')
	  }
	  for (var i = 0; i < len; i += 2) {
	    swap(this, i, i + 1)
	  }
	  return this
	}

	Buffer.prototype.swap32 = function swap32 () {
	  var len = this.length
	  if (len % 4 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 32-bits')
	  }
	  for (var i = 0; i < len; i += 4) {
	    swap(this, i, i + 3)
	    swap(this, i + 1, i + 2)
	  }
	  return this
	}

	Buffer.prototype.swap64 = function swap64 () {
	  var len = this.length
	  if (len % 8 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 64-bits')
	  }
	  for (var i = 0; i < len; i += 8) {
	    swap(this, i, i + 7)
	    swap(this, i + 1, i + 6)
	    swap(this, i + 2, i + 5)
	    swap(this, i + 3, i + 4)
	  }
	  return this
	}

	Buffer.prototype.toString = function toString () {
	  var length = this.length | 0
	  if (length === 0) return ''
	  if (arguments.length === 0) return utf8Slice(this, 0, length)
	  return slowToString.apply(this, arguments)
	}

	Buffer.prototype.equals = function equals (b) {
	  if (!Buffer.isBuffer(b)) throw new TypeError('Argument must be a Buffer')
	  if (this === b) return true
	  return Buffer.compare(this, b) === 0
	}

	Buffer.prototype.inspect = function inspect () {
	  var str = ''
	  var max = exports.INSPECT_MAX_BYTES
	  if (this.length > 0) {
	    str = this.toString('hex', 0, max).match(/.{2}/g).join(' ')
	    if (this.length > max) str += ' ... '
	  }
	  return '<Buffer ' + str + '>'
	}

	Buffer.prototype.compare = function compare (target, start, end, thisStart, thisEnd) {
	  if (!Buffer.isBuffer(target)) {
	    throw new TypeError('Argument must be a Buffer')
	  }

	  if (start === undefined) {
	    start = 0
	  }
	  if (end === undefined) {
	    end = target ? target.length : 0
	  }
	  if (thisStart === undefined) {
	    thisStart = 0
	  }
	  if (thisEnd === undefined) {
	    thisEnd = this.length
	  }

	  if (start < 0 || end > target.length || thisStart < 0 || thisEnd > this.length) {
	    throw new RangeError('out of range index')
	  }

	  if (thisStart >= thisEnd && start >= end) {
	    return 0
	  }
	  if (thisStart >= thisEnd) {
	    return -1
	  }
	  if (start >= end) {
	    return 1
	  }

	  start >>>= 0
	  end >>>= 0
	  thisStart >>>= 0
	  thisEnd >>>= 0

	  if (this === target) return 0

	  var x = thisEnd - thisStart
	  var y = end - start
	  var len = Math.min(x, y)

	  var thisCopy = this.slice(thisStart, thisEnd)
	  var targetCopy = target.slice(start, end)

	  for (var i = 0; i < len; ++i) {
	    if (thisCopy[i] !== targetCopy[i]) {
	      x = thisCopy[i]
	      y = targetCopy[i]
	      break
	    }
	  }

	  if (x < y) return -1
	  if (y < x) return 1
	  return 0
	}

	// Finds either the first index of `val` in `buffer` at offset >= `byteOffset`,
	// OR the last index of `val` in `buffer` at offset <= `byteOffset`.
	//
	// Arguments:
	// - buffer - a Buffer to search
	// - val - a string, Buffer, or number
	// - byteOffset - an index into `buffer`; will be clamped to an int32
	// - encoding - an optional encoding, relevant is val is a string
	// - dir - true for indexOf, false for lastIndexOf
	function bidirectionalIndexOf (buffer, val, byteOffset, encoding, dir) {
	  // Empty buffer means no match
	  if (buffer.length === 0) return -1

	  // Normalize byteOffset
	  if (typeof byteOffset === 'string') {
	    encoding = byteOffset
	    byteOffset = 0
	  } else if (byteOffset > 0x7fffffff) {
	    byteOffset = 0x7fffffff
	  } else if (byteOffset < -0x80000000) {
	    byteOffset = -0x80000000
	  }
	  byteOffset = +byteOffset  // Coerce to Number.
	  if (isNaN(byteOffset)) {
	    // byteOffset: it it's undefined, null, NaN, "foo", etc, search whole buffer
	    byteOffset = dir ? 0 : (buffer.length - 1)
	  }

	  // Normalize byteOffset: negative offsets start from the end of the buffer
	  if (byteOffset < 0) byteOffset = buffer.length + byteOffset
	  if (byteOffset >= buffer.length) {
	    if (dir) return -1
	    else byteOffset = buffer.length - 1
	  } else if (byteOffset < 0) {
	    if (dir) byteOffset = 0
	    else return -1
	  }

	  // Normalize val
	  if (typeof val === 'string') {
	    val = Buffer.from(val, encoding)
	  }

	  // Finally, search either indexOf (if dir is true) or lastIndexOf
	  if (Buffer.isBuffer(val)) {
	    // Special case: looking for empty string/buffer always fails
	    if (val.length === 0) {
	      return -1
	    }
	    return arrayIndexOf(buffer, val, byteOffset, encoding, dir)
	  } else if (typeof val === 'number') {
	    val = val & 0xFF // Search for a byte value [0-255]
	    if (Buffer.TYPED_ARRAY_SUPPORT &&
	        typeof Uint8Array.prototype.indexOf === 'function') {
	      if (dir) {
	        return Uint8Array.prototype.indexOf.call(buffer, val, byteOffset)
	      } else {
	        return Uint8Array.prototype.lastIndexOf.call(buffer, val, byteOffset)
	      }
	    }
	    return arrayIndexOf(buffer, [ val ], byteOffset, encoding, dir)
	  }

	  throw new TypeError('val must be string, number or Buffer')
	}

	function arrayIndexOf (arr, val, byteOffset, encoding, dir) {
	  var indexSize = 1
	  var arrLength = arr.length
	  var valLength = val.length

	  if (encoding !== undefined) {
	    encoding = String(encoding).toLowerCase()
	    if (encoding === 'ucs2' || encoding === 'ucs-2' ||
	        encoding === 'utf16le' || encoding === 'utf-16le') {
	      if (arr.length < 2 || val.length < 2) {
	        return -1
	      }
	      indexSize = 2
	      arrLength /= 2
	      valLength /= 2
	      byteOffset /= 2
	    }
	  }

	  function read (buf, i) {
	    if (indexSize === 1) {
	      return buf[i]
	    } else {
	      return buf.readUInt16BE(i * indexSize)
	    }
	  }

	  var i
	  if (dir) {
	    var foundIndex = -1
	    for (i = byteOffset; i < arrLength; i++) {
	      if (read(arr, i) === read(val, foundIndex === -1 ? 0 : i - foundIndex)) {
	        if (foundIndex === -1) foundIndex = i
	        if (i - foundIndex + 1 === valLength) return foundIndex * indexSize
	      } else {
	        if (foundIndex !== -1) i -= i - foundIndex
	        foundIndex = -1
	      }
	    }
	  } else {
	    if (byteOffset + valLength > arrLength) byteOffset = arrLength - valLength
	    for (i = byteOffset; i >= 0; i--) {
	      var found = true
	      for (var j = 0; j < valLength; j++) {
	        if (read(arr, i + j) !== read(val, j)) {
	          found = false
	          break
	        }
	      }
	      if (found) return i
	    }
	  }

	  return -1
	}

	Buffer.prototype.includes = function includes (val, byteOffset, encoding) {
	  return this.indexOf(val, byteOffset, encoding) !== -1
	}

	Buffer.prototype.indexOf = function indexOf (val, byteOffset, encoding) {
	  return bidirectionalIndexOf(this, val, byteOffset, encoding, true)
	}

	Buffer.prototype.lastIndexOf = function lastIndexOf (val, byteOffset, encoding) {
	  return bidirectionalIndexOf(this, val, byteOffset, encoding, false)
	}

	function hexWrite (buf, string, offset, length) {
	  offset = Number(offset) || 0
	  var remaining = buf.length - offset
	  if (!length) {
	    length = remaining
	  } else {
	    length = Number(length)
	    if (length > remaining) {
	      length = remaining
	    }
	  }

	  // must be an even number of digits
	  var strLen = string.length
	  if (strLen % 2 !== 0) throw new TypeError('Invalid hex string')

	  if (length > strLen / 2) {
	    length = strLen / 2
	  }
	  for (var i = 0; i < length; ++i) {
	    var parsed = parseInt(string.substr(i * 2, 2), 16)
	    if (isNaN(parsed)) return i
	    buf[offset + i] = parsed
	  }
	  return i
	}

	function utf8Write (buf, string, offset, length) {
	  return blitBuffer(utf8ToBytes(string, buf.length - offset), buf, offset, length)
	}

	function asciiWrite (buf, string, offset, length) {
	  return blitBuffer(asciiToBytes(string), buf, offset, length)
	}

	function latin1Write (buf, string, offset, length) {
	  return asciiWrite(buf, string, offset, length)
	}

	function base64Write (buf, string, offset, length) {
	  return blitBuffer(base64ToBytes(string), buf, offset, length)
	}

	function ucs2Write (buf, string, offset, length) {
	  return blitBuffer(utf16leToBytes(string, buf.length - offset), buf, offset, length)
	}

	Buffer.prototype.write = function write (string, offset, length, encoding) {
	  // Buffer#write(string)
	  if (offset === undefined) {
	    encoding = 'utf8'
	    length = this.length
	    offset = 0
	  // Buffer#write(string, encoding)
	  } else if (length === undefined && typeof offset === 'string') {
	    encoding = offset
	    length = this.length
	    offset = 0
	  // Buffer#write(string, offset[, length][, encoding])
	  } else if (isFinite(offset)) {
	    offset = offset | 0
	    if (isFinite(length)) {
	      length = length | 0
	      if (encoding === undefined) encoding = 'utf8'
	    } else {
	      encoding = length
	      length = undefined
	    }
	  // legacy write(string, encoding, offset, length) - remove in v0.13
	  } else {
	    throw new Error(
	      'Buffer.write(string, encoding, offset[, length]) is no longer supported'
	    )
	  }

	  var remaining = this.length - offset
	  if (length === undefined || length > remaining) length = remaining

	  if ((string.length > 0 && (length < 0 || offset < 0)) || offset > this.length) {
	    throw new RangeError('Attempt to write outside buffer bounds')
	  }

	  if (!encoding) encoding = 'utf8'

	  var loweredCase = false
	  for (;;) {
	    switch (encoding) {
	      case 'hex':
	        return hexWrite(this, string, offset, length)

	      case 'utf8':
	      case 'utf-8':
	        return utf8Write(this, string, offset, length)

	      case 'ascii':
	        return asciiWrite(this, string, offset, length)

	      case 'latin1':
	      case 'binary':
	        return latin1Write(this, string, offset, length)

	      case 'base64':
	        // Warning: maxLength not taken into account in base64Write
	        return base64Write(this, string, offset, length)

	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return ucs2Write(this, string, offset, length)

	      default:
	        if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
	        encoding = ('' + encoding).toLowerCase()
	        loweredCase = true
	    }
	  }
	}

	Buffer.prototype.toJSON = function toJSON () {
	  return {
	    type: 'Buffer',
	    data: Array.prototype.slice.call(this._arr || this, 0)
	  }
	}

	function base64Slice (buf, start, end) {
	  if (start === 0 && end === buf.length) {
	    return base64.fromByteArray(buf)
	  } else {
	    return base64.fromByteArray(buf.slice(start, end))
	  }
	}

	function utf8Slice (buf, start, end) {
	  end = Math.min(buf.length, end)
	  var res = []

	  var i = start
	  while (i < end) {
	    var firstByte = buf[i]
	    var codePoint = null
	    var bytesPerSequence = (firstByte > 0xEF) ? 4
	      : (firstByte > 0xDF) ? 3
	      : (firstByte > 0xBF) ? 2
	      : 1

	    if (i + bytesPerSequence <= end) {
	      var secondByte, thirdByte, fourthByte, tempCodePoint

	      switch (bytesPerSequence) {
	        case 1:
	          if (firstByte < 0x80) {
	            codePoint = firstByte
	          }
	          break
	        case 2:
	          secondByte = buf[i + 1]
	          if ((secondByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0x1F) << 0x6 | (secondByte & 0x3F)
	            if (tempCodePoint > 0x7F) {
	              codePoint = tempCodePoint
	            }
	          }
	          break
	        case 3:
	          secondByte = buf[i + 1]
	          thirdByte = buf[i + 2]
	          if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0xF) << 0xC | (secondByte & 0x3F) << 0x6 | (thirdByte & 0x3F)
	            if (tempCodePoint > 0x7FF && (tempCodePoint < 0xD800 || tempCodePoint > 0xDFFF)) {
	              codePoint = tempCodePoint
	            }
	          }
	          break
	        case 4:
	          secondByte = buf[i + 1]
	          thirdByte = buf[i + 2]
	          fourthByte = buf[i + 3]
	          if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80 && (fourthByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0xF) << 0x12 | (secondByte & 0x3F) << 0xC | (thirdByte & 0x3F) << 0x6 | (fourthByte & 0x3F)
	            if (tempCodePoint > 0xFFFF && tempCodePoint < 0x110000) {
	              codePoint = tempCodePoint
	            }
	          }
	      }
	    }

	    if (codePoint === null) {
	      // we did not generate a valid codePoint so insert a
	      // replacement char (U+FFFD) and advance only 1 byte
	      codePoint = 0xFFFD
	      bytesPerSequence = 1
	    } else if (codePoint > 0xFFFF) {
	      // encode to utf16 (surrogate pair dance)
	      codePoint -= 0x10000
	      res.push(codePoint >>> 10 & 0x3FF | 0xD800)
	      codePoint = 0xDC00 | codePoint & 0x3FF
	    }

	    res.push(codePoint)
	    i += bytesPerSequence
	  }

	  return decodeCodePointsArray(res)
	}

	// Based on http://stackoverflow.com/a/22747272/680742, the browser with
	// the lowest limit is Chrome, with 0x10000 args.
	// We go 1 magnitude less, for safety
	var MAX_ARGUMENTS_LENGTH = 0x1000

	function decodeCodePointsArray (codePoints) {
	  var len = codePoints.length
	  if (len <= MAX_ARGUMENTS_LENGTH) {
	    return String.fromCharCode.apply(String, codePoints) // avoid extra slice()
	  }

	  // Decode in chunks to avoid "call stack size exceeded".
	  var res = ''
	  var i = 0
	  while (i < len) {
	    res += String.fromCharCode.apply(
	      String,
	      codePoints.slice(i, i += MAX_ARGUMENTS_LENGTH)
	    )
	  }
	  return res
	}

	function asciiSlice (buf, start, end) {
	  var ret = ''
	  end = Math.min(buf.length, end)

	  for (var i = start; i < end; ++i) {
	    ret += String.fromCharCode(buf[i] & 0x7F)
	  }
	  return ret
	}

	function latin1Slice (buf, start, end) {
	  var ret = ''
	  end = Math.min(buf.length, end)

	  for (var i = start; i < end; ++i) {
	    ret += String.fromCharCode(buf[i])
	  }
	  return ret
	}

	function hexSlice (buf, start, end) {
	  var len = buf.length

	  if (!start || start < 0) start = 0
	  if (!end || end < 0 || end > len) end = len

	  var out = ''
	  for (var i = start; i < end; ++i) {
	    out += toHex(buf[i])
	  }
	  return out
	}

	function utf16leSlice (buf, start, end) {
	  var bytes = buf.slice(start, end)
	  var res = ''
	  for (var i = 0; i < bytes.length; i += 2) {
	    res += String.fromCharCode(bytes[i] + bytes[i + 1] * 256)
	  }
	  return res
	}

	Buffer.prototype.slice = function slice (start, end) {
	  var len = this.length
	  start = ~~start
	  end = end === undefined ? len : ~~end

	  if (start < 0) {
	    start += len
	    if (start < 0) start = 0
	  } else if (start > len) {
	    start = len
	  }

	  if (end < 0) {
	    end += len
	    if (end < 0) end = 0
	  } else if (end > len) {
	    end = len
	  }

	  if (end < start) end = start

	  var newBuf
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    newBuf = this.subarray(start, end)
	    newBuf.__proto__ = Buffer.prototype
	  } else {
	    var sliceLen = end - start
	    newBuf = new Buffer(sliceLen, undefined)
	    for (var i = 0; i < sliceLen; ++i) {
	      newBuf[i] = this[i + start]
	    }
	  }

	  return newBuf
	}

	/*
	 * Need to make sure that buffer isn't trying to write out of bounds.
	 */
	function checkOffset (offset, ext, length) {
	  if ((offset % 1) !== 0 || offset < 0) throw new RangeError('offset is not uint')
	  if (offset + ext > length) throw new RangeError('Trying to access beyond buffer length')
	}

	Buffer.prototype.readUIntLE = function readUIntLE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var val = this[offset]
	  var mul = 1
	  var i = 0
	  while (++i < byteLength && (mul *= 0x100)) {
	    val += this[offset + i] * mul
	  }

	  return val
	}

	Buffer.prototype.readUIntBE = function readUIntBE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    checkOffset(offset, byteLength, this.length)
	  }

	  var val = this[offset + --byteLength]
	  var mul = 1
	  while (byteLength > 0 && (mul *= 0x100)) {
	    val += this[offset + --byteLength] * mul
	  }

	  return val
	}

	Buffer.prototype.readUInt8 = function readUInt8 (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 1, this.length)
	  return this[offset]
	}

	Buffer.prototype.readUInt16LE = function readUInt16LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  return this[offset] | (this[offset + 1] << 8)
	}

	Buffer.prototype.readUInt16BE = function readUInt16BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  return (this[offset] << 8) | this[offset + 1]
	}

	Buffer.prototype.readUInt32LE = function readUInt32LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return ((this[offset]) |
	      (this[offset + 1] << 8) |
	      (this[offset + 2] << 16)) +
	      (this[offset + 3] * 0x1000000)
	}

	Buffer.prototype.readUInt32BE = function readUInt32BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset] * 0x1000000) +
	    ((this[offset + 1] << 16) |
	    (this[offset + 2] << 8) |
	    this[offset + 3])
	}

	Buffer.prototype.readIntLE = function readIntLE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var val = this[offset]
	  var mul = 1
	  var i = 0
	  while (++i < byteLength && (mul *= 0x100)) {
	    val += this[offset + i] * mul
	  }
	  mul *= 0x80

	  if (val >= mul) val -= Math.pow(2, 8 * byteLength)

	  return val
	}

	Buffer.prototype.readIntBE = function readIntBE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var i = byteLength
	  var mul = 1
	  var val = this[offset + --i]
	  while (i > 0 && (mul *= 0x100)) {
	    val += this[offset + --i] * mul
	  }
	  mul *= 0x80

	  if (val >= mul) val -= Math.pow(2, 8 * byteLength)

	  return val
	}

	Buffer.prototype.readInt8 = function readInt8 (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 1, this.length)
	  if (!(this[offset] & 0x80)) return (this[offset])
	  return ((0xff - this[offset] + 1) * -1)
	}

	Buffer.prototype.readInt16LE = function readInt16LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  var val = this[offset] | (this[offset + 1] << 8)
	  return (val & 0x8000) ? val | 0xFFFF0000 : val
	}

	Buffer.prototype.readInt16BE = function readInt16BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  var val = this[offset + 1] | (this[offset] << 8)
	  return (val & 0x8000) ? val | 0xFFFF0000 : val
	}

	Buffer.prototype.readInt32LE = function readInt32LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset]) |
	    (this[offset + 1] << 8) |
	    (this[offset + 2] << 16) |
	    (this[offset + 3] << 24)
	}

	Buffer.prototype.readInt32BE = function readInt32BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset] << 24) |
	    (this[offset + 1] << 16) |
	    (this[offset + 2] << 8) |
	    (this[offset + 3])
	}

	Buffer.prototype.readFloatLE = function readFloatLE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)
	  return ieee754.read(this, offset, true, 23, 4)
	}

	Buffer.prototype.readFloatBE = function readFloatBE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)
	  return ieee754.read(this, offset, false, 23, 4)
	}

	Buffer.prototype.readDoubleLE = function readDoubleLE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 8, this.length)
	  return ieee754.read(this, offset, true, 52, 8)
	}

	Buffer.prototype.readDoubleBE = function readDoubleBE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 8, this.length)
	  return ieee754.read(this, offset, false, 52, 8)
	}

	function checkInt (buf, value, offset, ext, max, min) {
	  if (!Buffer.isBuffer(buf)) throw new TypeError('"buffer" argument must be a Buffer instance')
	  if (value > max || value < min) throw new RangeError('"value" argument is out of bounds')
	  if (offset + ext > buf.length) throw new RangeError('Index out of range')
	}

	Buffer.prototype.writeUIntLE = function writeUIntLE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    var maxBytes = Math.pow(2, 8 * byteLength) - 1
	    checkInt(this, value, offset, byteLength, maxBytes, 0)
	  }

	  var mul = 1
	  var i = 0
	  this[offset] = value & 0xFF
	  while (++i < byteLength && (mul *= 0x100)) {
	    this[offset + i] = (value / mul) & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeUIntBE = function writeUIntBE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    var maxBytes = Math.pow(2, 8 * byteLength) - 1
	    checkInt(this, value, offset, byteLength, maxBytes, 0)
	  }

	  var i = byteLength - 1
	  var mul = 1
	  this[offset + i] = value & 0xFF
	  while (--i >= 0 && (mul *= 0x100)) {
	    this[offset + i] = (value / mul) & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeUInt8 = function writeUInt8 (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 1, 0xff, 0)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) value = Math.floor(value)
	  this[offset] = (value & 0xff)
	  return offset + 1
	}

	function objectWriteUInt16 (buf, value, offset, littleEndian) {
	  if (value < 0) value = 0xffff + value + 1
	  for (var i = 0, j = Math.min(buf.length - offset, 2); i < j; ++i) {
	    buf[offset + i] = (value & (0xff << (8 * (littleEndian ? i : 1 - i)))) >>>
	      (littleEndian ? i : 1 - i) * 8
	  }
	}

	Buffer.prototype.writeUInt16LE = function writeUInt16LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	  } else {
	    objectWriteUInt16(this, value, offset, true)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeUInt16BE = function writeUInt16BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 8)
	    this[offset + 1] = (value & 0xff)
	  } else {
	    objectWriteUInt16(this, value, offset, false)
	  }
	  return offset + 2
	}

	function objectWriteUInt32 (buf, value, offset, littleEndian) {
	  if (value < 0) value = 0xffffffff + value + 1
	  for (var i = 0, j = Math.min(buf.length - offset, 4); i < j; ++i) {
	    buf[offset + i] = (value >>> (littleEndian ? i : 3 - i) * 8) & 0xff
	  }
	}

	Buffer.prototype.writeUInt32LE = function writeUInt32LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset + 3] = (value >>> 24)
	    this[offset + 2] = (value >>> 16)
	    this[offset + 1] = (value >>> 8)
	    this[offset] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, true)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeUInt32BE = function writeUInt32BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 24)
	    this[offset + 1] = (value >>> 16)
	    this[offset + 2] = (value >>> 8)
	    this[offset + 3] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, false)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeIntLE = function writeIntLE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) {
	    var limit = Math.pow(2, 8 * byteLength - 1)

	    checkInt(this, value, offset, byteLength, limit - 1, -limit)
	  }

	  var i = 0
	  var mul = 1
	  var sub = 0
	  this[offset] = value & 0xFF
	  while (++i < byteLength && (mul *= 0x100)) {
	    if (value < 0 && sub === 0 && this[offset + i - 1] !== 0) {
	      sub = 1
	    }
	    this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeIntBE = function writeIntBE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) {
	    var limit = Math.pow(2, 8 * byteLength - 1)

	    checkInt(this, value, offset, byteLength, limit - 1, -limit)
	  }

	  var i = byteLength - 1
	  var mul = 1
	  var sub = 0
	  this[offset + i] = value & 0xFF
	  while (--i >= 0 && (mul *= 0x100)) {
	    if (value < 0 && sub === 0 && this[offset + i + 1] !== 0) {
	      sub = 1
	    }
	    this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeInt8 = function writeInt8 (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 1, 0x7f, -0x80)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) value = Math.floor(value)
	  if (value < 0) value = 0xff + value + 1
	  this[offset] = (value & 0xff)
	  return offset + 1
	}

	Buffer.prototype.writeInt16LE = function writeInt16LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	  } else {
	    objectWriteUInt16(this, value, offset, true)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeInt16BE = function writeInt16BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 8)
	    this[offset + 1] = (value & 0xff)
	  } else {
	    objectWriteUInt16(this, value, offset, false)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeInt32LE = function writeInt32LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	    this[offset + 2] = (value >>> 16)
	    this[offset + 3] = (value >>> 24)
	  } else {
	    objectWriteUInt32(this, value, offset, true)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeInt32BE = function writeInt32BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
	  if (value < 0) value = 0xffffffff + value + 1
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 24)
	    this[offset + 1] = (value >>> 16)
	    this[offset + 2] = (value >>> 8)
	    this[offset + 3] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, false)
	  }
	  return offset + 4
	}

	function checkIEEE754 (buf, value, offset, ext, max, min) {
	  if (offset + ext > buf.length) throw new RangeError('Index out of range')
	  if (offset < 0) throw new RangeError('Index out of range')
	}

	function writeFloat (buf, value, offset, littleEndian, noAssert) {
	  if (!noAssert) {
	    checkIEEE754(buf, value, offset, 4, 3.4028234663852886e+38, -3.4028234663852886e+38)
	  }
	  ieee754.write(buf, value, offset, littleEndian, 23, 4)
	  return offset + 4
	}

	Buffer.prototype.writeFloatLE = function writeFloatLE (value, offset, noAssert) {
	  return writeFloat(this, value, offset, true, noAssert)
	}

	Buffer.prototype.writeFloatBE = function writeFloatBE (value, offset, noAssert) {
	  return writeFloat(this, value, offset, false, noAssert)
	}

	function writeDouble (buf, value, offset, littleEndian, noAssert) {
	  if (!noAssert) {
	    checkIEEE754(buf, value, offset, 8, 1.7976931348623157E+308, -1.7976931348623157E+308)
	  }
	  ieee754.write(buf, value, offset, littleEndian, 52, 8)
	  return offset + 8
	}

	Buffer.prototype.writeDoubleLE = function writeDoubleLE (value, offset, noAssert) {
	  return writeDouble(this, value, offset, true, noAssert)
	}

	Buffer.prototype.writeDoubleBE = function writeDoubleBE (value, offset, noAssert) {
	  return writeDouble(this, value, offset, false, noAssert)
	}

	// copy(targetBuffer, targetStart=0, sourceStart=0, sourceEnd=buffer.length)
	Buffer.prototype.copy = function copy (target, targetStart, start, end) {
	  if (!start) start = 0
	  if (!end && end !== 0) end = this.length
	  if (targetStart >= target.length) targetStart = target.length
	  if (!targetStart) targetStart = 0
	  if (end > 0 && end < start) end = start

	  // Copy 0 bytes; we're done
	  if (end === start) return 0
	  if (target.length === 0 || this.length === 0) return 0

	  // Fatal error conditions
	  if (targetStart < 0) {
	    throw new RangeError('targetStart out of bounds')
	  }
	  if (start < 0 || start >= this.length) throw new RangeError('sourceStart out of bounds')
	  if (end < 0) throw new RangeError('sourceEnd out of bounds')

	  // Are we oob?
	  if (end > this.length) end = this.length
	  if (target.length - targetStart < end - start) {
	    end = target.length - targetStart + start
	  }

	  var len = end - start
	  var i

	  if (this === target && start < targetStart && targetStart < end) {
	    // descending copy from end
	    for (i = len - 1; i >= 0; --i) {
	      target[i + targetStart] = this[i + start]
	    }
	  } else if (len < 1000 || !Buffer.TYPED_ARRAY_SUPPORT) {
	    // ascending copy from start
	    for (i = 0; i < len; ++i) {
	      target[i + targetStart] = this[i + start]
	    }
	  } else {
	    Uint8Array.prototype.set.call(
	      target,
	      this.subarray(start, start + len),
	      targetStart
	    )
	  }

	  return len
	}

	// Usage:
	//    buffer.fill(number[, offset[, end]])
	//    buffer.fill(buffer[, offset[, end]])
	//    buffer.fill(string[, offset[, end]][, encoding])
	Buffer.prototype.fill = function fill (val, start, end, encoding) {
	  // Handle string cases:
	  if (typeof val === 'string') {
	    if (typeof start === 'string') {
	      encoding = start
	      start = 0
	      end = this.length
	    } else if (typeof end === 'string') {
	      encoding = end
	      end = this.length
	    }
	    if (val.length === 1) {
	      var code = val.charCodeAt(0)
	      if (code < 256) {
	        val = code
	      }
	    }
	    if (encoding !== undefined && typeof encoding !== 'string') {
	      throw new TypeError('encoding must be a string')
	    }
	    if (typeof encoding === 'string' && !Buffer.isEncoding(encoding)) {
	      throw new TypeError('Unknown encoding: ' + encoding)
	    }
	  } else if (typeof val === 'number') {
	    val = val & 255
	  }

	  // Invalid ranges are not set to a default, so can range check early.
	  if (start < 0 || this.length < start || this.length < end) {
	    throw new RangeError('Out of range index')
	  }

	  if (end <= start) {
	    return this
	  }

	  start = start >>> 0
	  end = end === undefined ? this.length : end >>> 0

	  if (!val) val = 0

	  var i
	  if (typeof val === 'number') {
	    for (i = start; i < end; ++i) {
	      this[i] = val
	    }
	  } else {
	    var bytes = Buffer.isBuffer(val)
	      ? val
	      : utf8ToBytes(new Buffer(val, encoding).toString())
	    var len = bytes.length
	    for (i = 0; i < end - start; ++i) {
	      this[i + start] = bytes[i % len]
	    }
	  }

	  return this
	}

	// HELPER FUNCTIONS
	// ================

	var INVALID_BASE64_RE = /[^+\/0-9A-Za-z-_]/g

	function base64clean (str) {
	  // Node strips out invalid characters like \n and \t from the string, base64-js does not
	  str = stringtrim(str).replace(INVALID_BASE64_RE, '')
	  // Node converts strings with length < 2 to ''
	  if (str.length < 2) return ''
	  // Node allows for non-padded base64 strings (missing trailing ===), base64-js does not
	  while (str.length % 4 !== 0) {
	    str = str + '='
	  }
	  return str
	}

	function stringtrim (str) {
	  if (str.trim) return str.trim()
	  return str.replace(/^\s+|\s+$/g, '')
	}

	function toHex (n) {
	  if (n < 16) return '0' + n.toString(16)
	  return n.toString(16)
	}

	function utf8ToBytes (string, units) {
	  units = units || Infinity
	  var codePoint
	  var length = string.length
	  var leadSurrogate = null
	  var bytes = []

	  for (var i = 0; i < length; ++i) {
	    codePoint = string.charCodeAt(i)

	    // is surrogate component
	    if (codePoint > 0xD7FF && codePoint < 0xE000) {
	      // last char was a lead
	      if (!leadSurrogate) {
	        // no lead yet
	        if (codePoint > 0xDBFF) {
	          // unexpected trail
	          if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	          continue
	        } else if (i + 1 === length) {
	          // unpaired lead
	          if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	          continue
	        }

	        // valid lead
	        leadSurrogate = codePoint

	        continue
	      }

	      // 2 leads in a row
	      if (codePoint < 0xDC00) {
	        if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	        leadSurrogate = codePoint
	        continue
	      }

	      // valid surrogate pair
	      codePoint = (leadSurrogate - 0xD800 << 10 | codePoint - 0xDC00) + 0x10000
	    } else if (leadSurrogate) {
	      // valid bmp char, but last char was a lead
	      if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	    }

	    leadSurrogate = null

	    // encode utf8
	    if (codePoint < 0x80) {
	      if ((units -= 1) < 0) break
	      bytes.push(codePoint)
	    } else if (codePoint < 0x800) {
	      if ((units -= 2) < 0) break
	      bytes.push(
	        codePoint >> 0x6 | 0xC0,
	        codePoint & 0x3F | 0x80
	      )
	    } else if (codePoint < 0x10000) {
	      if ((units -= 3) < 0) break
	      bytes.push(
	        codePoint >> 0xC | 0xE0,
	        codePoint >> 0x6 & 0x3F | 0x80,
	        codePoint & 0x3F | 0x80
	      )
	    } else if (codePoint < 0x110000) {
	      if ((units -= 4) < 0) break
	      bytes.push(
	        codePoint >> 0x12 | 0xF0,
	        codePoint >> 0xC & 0x3F | 0x80,
	        codePoint >> 0x6 & 0x3F | 0x80,
	        codePoint & 0x3F | 0x80
	      )
	    } else {
	      throw new Error('Invalid code point')
	    }
	  }

	  return bytes
	}

	function asciiToBytes (str) {
	  var byteArray = []
	  for (var i = 0; i < str.length; ++i) {
	    // Node's code seems to be doing this and not & 0x7F..
	    byteArray.push(str.charCodeAt(i) & 0xFF)
	  }
	  return byteArray
	}

	function utf16leToBytes (str, units) {
	  var c, hi, lo
	  var byteArray = []
	  for (var i = 0; i < str.length; ++i) {
	    if ((units -= 2) < 0) break

	    c = str.charCodeAt(i)
	    hi = c >> 8
	    lo = c % 256
	    byteArray.push(lo)
	    byteArray.push(hi)
	  }

	  return byteArray
	}

	function base64ToBytes (str) {
	  return base64.toByteArray(base64clean(str))
	}

	function blitBuffer (src, dst, offset, length) {
	  for (var i = 0; i < length; ++i) {
	    if ((i + offset >= dst.length) || (i >= src.length)) break
	    dst[i + offset] = src[i]
	  }
	  return i
	}

	function isnan (val) {
	  return val !== val // eslint-disable-line no-self-compare
	}

	}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"base64-js":39,"ieee754":65,"isarray":68}],45:[function(require,module,exports){
	(function (Buffer){
	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	// NOTE: These type checking functions intentionally don't use `instanceof`
	// because it is fragile and can be easily faked with `Object.create()`.

	function isArray(arg) {
	  if (Array.isArray) {
	    return Array.isArray(arg);
	  }
	  return objectToString(arg) === '[object Array]';
	}
	exports.isArray = isArray;

	function isBoolean(arg) {
	  return typeof arg === 'boolean';
	}
	exports.isBoolean = isBoolean;

	function isNull(arg) {
	  return arg === null;
	}
	exports.isNull = isNull;

	function isNullOrUndefined(arg) {
	  return arg == null;
	}
	exports.isNullOrUndefined = isNullOrUndefined;

	function isNumber(arg) {
	  return typeof arg === 'number';
	}
	exports.isNumber = isNumber;

	function isString(arg) {
	  return typeof arg === 'string';
	}
	exports.isString = isString;

	function isSymbol(arg) {
	  return typeof arg === 'symbol';
	}
	exports.isSymbol = isSymbol;

	function isUndefined(arg) {
	  return arg === void 0;
	}
	exports.isUndefined = isUndefined;

	function isRegExp(re) {
	  return objectToString(re) === '[object RegExp]';
	}
	exports.isRegExp = isRegExp;

	function isObject(arg) {
	  return typeof arg === 'object' && arg !== null;
	}
	exports.isObject = isObject;

	function isDate(d) {
	  return objectToString(d) === '[object Date]';
	}
	exports.isDate = isDate;

	function isError(e) {
	  return (objectToString(e) === '[object Error]' || e instanceof Error);
	}
	exports.isError = isError;

	function isFunction(arg) {
	  return typeof arg === 'function';
	}
	exports.isFunction = isFunction;

	function isPrimitive(arg) {
	  return arg === null ||
	         typeof arg === 'boolean' ||
	         typeof arg === 'number' ||
	         typeof arg === 'string' ||
	         typeof arg === 'symbol' ||  // ES6 symbol
	         typeof arg === 'undefined';
	}
	exports.isPrimitive = isPrimitive;

	exports.isBuffer = Buffer.isBuffer;

	function objectToString(o) {
	  return Object.prototype.toString.call(o);
	}

	}).call(this,{"isBuffer":require("../../is-buffer/index.js")})
	},{"../../is-buffer/index.js":67}],46:[function(require,module,exports){
	/*istanbul ignore start*/"use strict";

	exports.__esModule = true;
	exports. /*istanbul ignore end*/convertChangesToDMP = convertChangesToDMP;
	// See: http://code.google.com/p/google-diff-match-patch/wiki/API
	function convertChangesToDMP(changes) {
	  var ret = [],
	      change = /*istanbul ignore start*/void 0 /*istanbul ignore end*/,
	      operation = /*istanbul ignore start*/void 0 /*istanbul ignore end*/;
	  for (var i = 0; i < changes.length; i++) {
	    change = changes[i];
	    if (change.added) {
	      operation = 1;
	    } else if (change.removed) {
	      operation = -1;
	    } else {
	      operation = 0;
	    }

	    ret.push([operation, change.value]);
	  }
	  return ret;
	}


	},{}],47:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports. /*istanbul ignore end*/convertChangesToXML = convertChangesToXML;
	function convertChangesToXML(changes) {
	  var ret = [];
	  for (var i = 0; i < changes.length; i++) {
	    var change = changes[i];
	    if (change.added) {
	      ret.push('<ins>');
	    } else if (change.removed) {
	      ret.push('<del>');
	    }

	    ret.push(escapeHTML(change.value));

	    if (change.added) {
	      ret.push('</ins>');
	    } else if (change.removed) {
	      ret.push('</del>');
	    }
	  }
	  return ret.join('');
	}

	function escapeHTML(s) {
	  var n = s;
	  n = n.replace(/&/g, '&amp;');
	  n = n.replace(/</g, '&lt;');
	  n = n.replace(/>/g, '&gt;');
	  n = n.replace(/"/g, '&quot;');

	  return n;
	}


	},{}],48:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.arrayDiff = undefined;
	exports. /*istanbul ignore end*/diffArrays = diffArrays;

	var /*istanbul ignore start*/_base = require('./base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/var arrayDiff = /*istanbul ignore start*/exports. /*istanbul ignore end*/arrayDiff = new /*istanbul ignore start*/_base2['default']() /*istanbul ignore end*/;
	arrayDiff.tokenize = arrayDiff.join = function (value) {
	  return value.slice();
	};

	function diffArrays(oldArr, newArr, callback) {
	  return arrayDiff.diff(oldArr, newArr, callback);
	}


	},{"./base":49}],49:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports['default'] = /*istanbul ignore end*/Diff;
	function Diff() {}

	Diff.prototype = { /*istanbul ignore start*/
	  /*istanbul ignore end*/diff: function diff(oldString, newString) {
	    /*istanbul ignore start*/var /*istanbul ignore end*/options = arguments.length <= 2 || arguments[2] === undefined ? {} : arguments[2];

	    var callback = options.callback;
	    if (typeof options === 'function') {
	      callback = options;
	      options = {};
	    }
	    this.options = options;

	    var self = this;

	    function done(value) {
	      if (callback) {
	        setTimeout(function () {
	          callback(undefined, value);
	        }, 0);
	        return true;
	      } else {
	        return value;
	      }
	    }

	    // Allow subclasses to massage the input prior to running
	    oldString = this.castInput(oldString);
	    newString = this.castInput(newString);

	    oldString = this.removeEmpty(this.tokenize(oldString));
	    newString = this.removeEmpty(this.tokenize(newString));

	    var newLen = newString.length,
	        oldLen = oldString.length;
	    var editLength = 1;
	    var maxEditLength = newLen + oldLen;
	    var bestPath = [{ newPos: -1, components: [] }];

	    // Seed editLength = 0, i.e. the content starts with the same values
	    var oldPos = this.extractCommon(bestPath[0], newString, oldString, 0);
	    if (bestPath[0].newPos + 1 >= newLen && oldPos + 1 >= oldLen) {
	      // Identity per the equality and tokenizer
	      return done([{ value: this.join(newString), count: newString.length }]);
	    }

	    // Main worker method. checks all permutations of a given edit length for acceptance.
	    function execEditLength() {
	      for (var diagonalPath = -1 * editLength; diagonalPath <= editLength; diagonalPath += 2) {
	        var basePath = /*istanbul ignore start*/void 0 /*istanbul ignore end*/;
	        var addPath = bestPath[diagonalPath - 1],
	            removePath = bestPath[diagonalPath + 1],
	            _oldPos = (removePath ? removePath.newPos : 0) - diagonalPath;
	        if (addPath) {
	          // No one else is going to attempt to use this value, clear it
	          bestPath[diagonalPath - 1] = undefined;
	        }

	        var canAdd = addPath && addPath.newPos + 1 < newLen,
	            canRemove = removePath && 0 <= _oldPos && _oldPos < oldLen;
	        if (!canAdd && !canRemove) {
	          // If this path is a terminal then prune
	          bestPath[diagonalPath] = undefined;
	          continue;
	        }

	        // Select the diagonal that we want to branch from. We select the prior
	        // path whose position in the new string is the farthest from the origin
	        // and does not pass the bounds of the diff graph
	        if (!canAdd || canRemove && addPath.newPos < removePath.newPos) {
	          basePath = clonePath(removePath);
	          self.pushComponent(basePath.components, undefined, true);
	        } else {
	          basePath = addPath; // No need to clone, we've pulled it from the list
	          basePath.newPos++;
	          self.pushComponent(basePath.components, true, undefined);
	        }

	        _oldPos = self.extractCommon(basePath, newString, oldString, diagonalPath);

	        // If we have hit the end of both strings, then we are done
	        if (basePath.newPos + 1 >= newLen && _oldPos + 1 >= oldLen) {
	          return done(buildValues(self, basePath.components, newString, oldString, self.useLongestToken));
	        } else {
	          // Otherwise track this path as a potential candidate and continue.
	          bestPath[diagonalPath] = basePath;
	        }
	      }

	      editLength++;
	    }

	    // Performs the length of edit iteration. Is a bit fugly as this has to support the
	    // sync and async mode which is never fun. Loops over execEditLength until a value
	    // is produced.
	    if (callback) {
	      (function exec() {
	        setTimeout(function () {
	          // This should not happen, but we want to be safe.
	          /* istanbul ignore next */
	          if (editLength > maxEditLength) {
	            return callback();
	          }

	          if (!execEditLength()) {
	            exec();
	          }
	        }, 0);
	      })();
	    } else {
	      while (editLength <= maxEditLength) {
	        var ret = execEditLength();
	        if (ret) {
	          return ret;
	        }
	      }
	    }
	  },
	  /*istanbul ignore start*/ /*istanbul ignore end*/pushComponent: function pushComponent(components, added, removed) {
	    var last = components[components.length - 1];
	    if (last && last.added === added && last.removed === removed) {
	      // We need to clone here as the component clone operation is just
	      // as shallow array clone
	      components[components.length - 1] = { count: last.count + 1, added: added, removed: removed };
	    } else {
	      components.push({ count: 1, added: added, removed: removed });
	    }
	  },
	  /*istanbul ignore start*/ /*istanbul ignore end*/extractCommon: function extractCommon(basePath, newString, oldString, diagonalPath) {
	    var newLen = newString.length,
	        oldLen = oldString.length,
	        newPos = basePath.newPos,
	        oldPos = newPos - diagonalPath,
	        commonCount = 0;
	    while (newPos + 1 < newLen && oldPos + 1 < oldLen && this.equals(newString[newPos + 1], oldString[oldPos + 1])) {
	      newPos++;
	      oldPos++;
	      commonCount++;
	    }

	    if (commonCount) {
	      basePath.components.push({ count: commonCount });
	    }

	    basePath.newPos = newPos;
	    return oldPos;
	  },
	  /*istanbul ignore start*/ /*istanbul ignore end*/equals: function equals(left, right) {
	    return left === right;
	  },
	  /*istanbul ignore start*/ /*istanbul ignore end*/removeEmpty: function removeEmpty(array) {
	    var ret = [];
	    for (var i = 0; i < array.length; i++) {
	      if (array[i]) {
	        ret.push(array[i]);
	      }
	    }
	    return ret;
	  },
	  /*istanbul ignore start*/ /*istanbul ignore end*/castInput: function castInput(value) {
	    return value;
	  },
	  /*istanbul ignore start*/ /*istanbul ignore end*/tokenize: function tokenize(value) {
	    return value.split('');
	  },
	  /*istanbul ignore start*/ /*istanbul ignore end*/join: function join(chars) {
	    return chars.join('');
	  }
	};

	function buildValues(diff, components, newString, oldString, useLongestToken) {
	  var componentPos = 0,
	      componentLen = components.length,
	      newPos = 0,
	      oldPos = 0;

	  for (; componentPos < componentLen; componentPos++) {
	    var component = components[componentPos];
	    if (!component.removed) {
	      if (!component.added && useLongestToken) {
	        var value = newString.slice(newPos, newPos + component.count);
	        value = value.map(function (value, i) {
	          var oldValue = oldString[oldPos + i];
	          return oldValue.length > value.length ? oldValue : value;
	        });

	        component.value = diff.join(value);
	      } else {
	        component.value = diff.join(newString.slice(newPos, newPos + component.count));
	      }
	      newPos += component.count;

	      // Common case
	      if (!component.added) {
	        oldPos += component.count;
	      }
	    } else {
	      component.value = diff.join(oldString.slice(oldPos, oldPos + component.count));
	      oldPos += component.count;

	      // Reverse add and remove so removes are output first to match common convention
	      // The diffing algorithm is tied to add then remove output and this is the simplest
	      // route to get the desired output with minimal overhead.
	      if (componentPos && components[componentPos - 1].added) {
	        var tmp = components[componentPos - 1];
	        components[componentPos - 1] = components[componentPos];
	        components[componentPos] = tmp;
	      }
	    }
	  }

	  // Special case handle for when one terminal is ignored. For this case we merge the
	  // terminal into the prior string and drop the change.
	  var lastComponent = components[componentLen - 1];
	  if (componentLen > 1 && (lastComponent.added || lastComponent.removed) && diff.equals('', lastComponent.value)) {
	    components[componentLen - 2].value += lastComponent.value;
	    components.pop();
	  }

	  return components;
	}

	function clonePath(path) {
	  return { newPos: path.newPos, components: path.components.slice(0) };
	}


	},{}],50:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.characterDiff = undefined;
	exports. /*istanbul ignore end*/diffChars = diffChars;

	var /*istanbul ignore start*/_base = require('./base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/var characterDiff = /*istanbul ignore start*/exports. /*istanbul ignore end*/characterDiff = new /*istanbul ignore start*/_base2['default']() /*istanbul ignore end*/;
	function diffChars(oldStr, newStr, callback) {
	  return characterDiff.diff(oldStr, newStr, callback);
	}


	},{"./base":49}],51:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.cssDiff = undefined;
	exports. /*istanbul ignore end*/diffCss = diffCss;

	var /*istanbul ignore start*/_base = require('./base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/var cssDiff = /*istanbul ignore start*/exports. /*istanbul ignore end*/cssDiff = new /*istanbul ignore start*/_base2['default']() /*istanbul ignore end*/;
	cssDiff.tokenize = function (value) {
	  return value.split(/([{}:;,]|\s+)/);
	};

	function diffCss(oldStr, newStr, callback) {
	  return cssDiff.diff(oldStr, newStr, callback);
	}


	},{"./base":49}],52:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.jsonDiff = undefined;

	var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) { return typeof obj; } : function (obj) { return obj && typeof Symbol === "function" && obj.constructor === Symbol ? "symbol" : typeof obj; };

	exports. /*istanbul ignore end*/diffJson = diffJson;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/canonicalize = canonicalize;

	var /*istanbul ignore start*/_base = require('./base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	/*istanbul ignore end*/
	var /*istanbul ignore start*/_line = require('./line') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/

	var objectPrototypeToString = Object.prototype.toString;

	var jsonDiff = /*istanbul ignore start*/exports. /*istanbul ignore end*/jsonDiff = new /*istanbul ignore start*/_base2['default']() /*istanbul ignore end*/;
	// Discriminate between two lines of pretty-printed, serialized JSON where one of them has a
	// dangling comma and the other doesn't. Turns out including the dangling comma yields the nicest output:
	jsonDiff.useLongestToken = true;

	jsonDiff.tokenize = /*istanbul ignore start*/_line.lineDiff. /*istanbul ignore end*/tokenize;
	jsonDiff.castInput = function (value) {
	  /*istanbul ignore start*/var /*istanbul ignore end*/undefinedReplacement = this.options.undefinedReplacement;


	  return typeof value === 'string' ? value : JSON.stringify(canonicalize(value), function (k, v) {
	    if (typeof v === 'undefined') {
	      return undefinedReplacement;
	    }

	    return v;
	  }, '  ');
	};
	jsonDiff.equals = function (left, right) {
	  return (/*istanbul ignore start*/_base2['default']. /*istanbul ignore end*/prototype.equals(left.replace(/,([\r\n])/g, '$1'), right.replace(/,([\r\n])/g, '$1'))
	  );
	};

	function diffJson(oldObj, newObj, options) {
	  return jsonDiff.diff(oldObj, newObj, options);
	}

	// This function handles the presence of circular references by bailing out when encountering an
	// object that is already on the "stack" of items being processed.
	function canonicalize(obj, stack, replacementStack) {
	  stack = stack || [];
	  replacementStack = replacementStack || [];

	  var i = /*istanbul ignore start*/void 0 /*istanbul ignore end*/;

	  for (i = 0; i < stack.length; i += 1) {
	    if (stack[i] === obj) {
	      return replacementStack[i];
	    }
	  }

	  var canonicalizedObj = /*istanbul ignore start*/void 0 /*istanbul ignore end*/;

	  if ('[object Array]' === objectPrototypeToString.call(obj)) {
	    stack.push(obj);
	    canonicalizedObj = new Array(obj.length);
	    replacementStack.push(canonicalizedObj);
	    for (i = 0; i < obj.length; i += 1) {
	      canonicalizedObj[i] = canonicalize(obj[i], stack, replacementStack);
	    }
	    stack.pop();
	    replacementStack.pop();
	    return canonicalizedObj;
	  }

	  if (obj && obj.toJSON) {
	    obj = obj.toJSON();
	  }

	  if ( /*istanbul ignore start*/(typeof /*istanbul ignore end*/obj === 'undefined' ? 'undefined' : _typeof(obj)) === 'object' && obj !== null) {
	    stack.push(obj);
	    canonicalizedObj = {};
	    replacementStack.push(canonicalizedObj);
	    var sortedKeys = [],
	        key = /*istanbul ignore start*/void 0 /*istanbul ignore end*/;
	    for (key in obj) {
	      /* istanbul ignore else */
	      if (obj.hasOwnProperty(key)) {
	        sortedKeys.push(key);
	      }
	    }
	    sortedKeys.sort();
	    for (i = 0; i < sortedKeys.length; i += 1) {
	      key = sortedKeys[i];
	      canonicalizedObj[key] = canonicalize(obj[key], stack, replacementStack);
	    }
	    stack.pop();
	    replacementStack.pop();
	  } else {
	    canonicalizedObj = obj;
	  }
	  return canonicalizedObj;
	}


	},{"./base":49,"./line":53}],53:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.lineDiff = undefined;
	exports. /*istanbul ignore end*/diffLines = diffLines;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffTrimmedLines = diffTrimmedLines;

	var /*istanbul ignore start*/_base = require('./base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	/*istanbul ignore end*/
	var /*istanbul ignore start*/_params = require('../util/params') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/var lineDiff = /*istanbul ignore start*/exports. /*istanbul ignore end*/lineDiff = new /*istanbul ignore start*/_base2['default']() /*istanbul ignore end*/;
	lineDiff.tokenize = function (value) {
	  var retLines = [],
	      linesAndNewlines = value.split(/(\n|\r\n)/);

	  // Ignore the final empty token that occurs if the string ends with a new line
	  if (!linesAndNewlines[linesAndNewlines.length - 1]) {
	    linesAndNewlines.pop();
	  }

	  // Merge the content and line separators into single tokens
	  for (var i = 0; i < linesAndNewlines.length; i++) {
	    var line = linesAndNewlines[i];

	    if (i % 2 && !this.options.newlineIsToken) {
	      retLines[retLines.length - 1] += line;
	    } else {
	      if (this.options.ignoreWhitespace) {
	        line = line.trim();
	      }
	      retLines.push(line);
	    }
	  }

	  return retLines;
	};

	function diffLines(oldStr, newStr, callback) {
	  return lineDiff.diff(oldStr, newStr, callback);
	}
	function diffTrimmedLines(oldStr, newStr, callback) {
	  var options = /*istanbul ignore start*/(0, _params.generateOptions) /*istanbul ignore end*/(callback, { ignoreWhitespace: true });
	  return lineDiff.diff(oldStr, newStr, options);
	}


	},{"../util/params":61,"./base":49}],54:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.sentenceDiff = undefined;
	exports. /*istanbul ignore end*/diffSentences = diffSentences;

	var /*istanbul ignore start*/_base = require('./base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/var sentenceDiff = /*istanbul ignore start*/exports. /*istanbul ignore end*/sentenceDiff = new /*istanbul ignore start*/_base2['default']() /*istanbul ignore end*/;
	sentenceDiff.tokenize = function (value) {
	  return value.split(/(\S.+?[.!?])(?=\s+|$)/);
	};

	function diffSentences(oldStr, newStr, callback) {
	  return sentenceDiff.diff(oldStr, newStr, callback);
	}


	},{"./base":49}],55:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.wordDiff = undefined;
	exports. /*istanbul ignore end*/diffWords = diffWords;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffWordsWithSpace = diffWordsWithSpace;

	var /*istanbul ignore start*/_base = require('./base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	/*istanbul ignore end*/
	var /*istanbul ignore start*/_params = require('../util/params') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/

	// Based on https://en.wikipedia.org/wiki/Latin_script_in_Unicode
	//
	// Ranges and exceptions:
	// Latin-1 Supplement, 0080–00FF
	//  - U+00D7  × Multiplication sign
	//  - U+00F7  ÷ Division sign
	// Latin Extended-A, 0100–017F
	// Latin Extended-B, 0180–024F
	// IPA Extensions, 0250–02AF
	// Spacing Modifier Letters, 02B0–02FF
	//  - U+02C7  ˇ &#711;  Caron
	//  - U+02D8  ˘ &#728;  Breve
	//  - U+02D9  ˙ &#729;  Dot Above
	//  - U+02DA  ˚ &#730;  Ring Above
	//  - U+02DB  ˛ &#731;  Ogonek
	//  - U+02DC  ˜ &#732;  Small Tilde
	//  - U+02DD  ˝ &#733;  Double Acute Accent
	// Latin Extended Additional, 1E00–1EFF
	var extendedWordChars = /^[A-Za-z\xC0-\u02C6\u02C8-\u02D7\u02DE-\u02FF\u1E00-\u1EFF]+$/;

	var reWhitespace = /\S/;

	var wordDiff = /*istanbul ignore start*/exports. /*istanbul ignore end*/wordDiff = new /*istanbul ignore start*/_base2['default']() /*istanbul ignore end*/;
	wordDiff.equals = function (left, right) {
	  return left === right || this.options.ignoreWhitespace && !reWhitespace.test(left) && !reWhitespace.test(right);
	};
	wordDiff.tokenize = function (value) {
	  var tokens = value.split(/(\s+|\b)/);

	  // Join the boundary splits that we do not consider to be boundaries. This is primarily the extended Latin character set.
	  for (var i = 0; i < tokens.length - 1; i++) {
	    // If we have an empty string in the next field and we have only word chars before and after, merge
	    if (!tokens[i + 1] && tokens[i + 2] && extendedWordChars.test(tokens[i]) && extendedWordChars.test(tokens[i + 2])) {
	      tokens[i] += tokens[i + 2];
	      tokens.splice(i + 1, 2);
	      i--;
	    }
	  }

	  return tokens;
	};

	function diffWords(oldStr, newStr, callback) {
	  var options = /*istanbul ignore start*/(0, _params.generateOptions) /*istanbul ignore end*/(callback, { ignoreWhitespace: true });
	  return wordDiff.diff(oldStr, newStr, options);
	}
	function diffWordsWithSpace(oldStr, newStr, callback) {
	  return wordDiff.diff(oldStr, newStr, callback);
	}


	},{"../util/params":61,"./base":49}],56:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports.canonicalize = exports.convertChangesToXML = exports.convertChangesToDMP = exports.parsePatch = exports.applyPatches = exports.applyPatch = exports.createPatch = exports.createTwoFilesPatch = exports.structuredPatch = exports.diffArrays = exports.diffJson = exports.diffCss = exports.diffSentences = exports.diffTrimmedLines = exports.diffLines = exports.diffWordsWithSpace = exports.diffWords = exports.diffChars = exports.Diff = undefined;
	/*istanbul ignore end*/
	var /*istanbul ignore start*/_base = require('./diff/base') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _base2 = _interopRequireDefault(_base);

	/*istanbul ignore end*/
	var /*istanbul ignore start*/_character = require('./diff/character') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_word = require('./diff/word') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_line = require('./diff/line') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_sentence = require('./diff/sentence') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_css = require('./diff/css') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_json = require('./diff/json') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_array = require('./diff/array') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_apply = require('./patch/apply') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_parse = require('./patch/parse') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_create = require('./patch/create') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_dmp = require('./convert/dmp') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_xml = require('./convert/xml') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	exports. /*istanbul ignore end*/Diff = _base2['default'];
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffChars = _character.diffChars;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffWords = _word.diffWords;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffWordsWithSpace = _word.diffWordsWithSpace;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffLines = _line.diffLines;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffTrimmedLines = _line.diffTrimmedLines;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffSentences = _sentence.diffSentences;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffCss = _css.diffCss;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffJson = _json.diffJson;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/diffArrays = _array.diffArrays;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/structuredPatch = _create.structuredPatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/createTwoFilesPatch = _create.createTwoFilesPatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/createPatch = _create.createPatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/applyPatch = _apply.applyPatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/applyPatches = _apply.applyPatches;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/parsePatch = _parse.parsePatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/convertChangesToDMP = _dmp.convertChangesToDMP;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/convertChangesToXML = _xml.convertChangesToXML;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/canonicalize = _json.canonicalize; /* See LICENSE file for terms of use */

	/*
	 * Text diff implementation.
	 *
	 * This library supports the following APIS:
	 * JsDiff.diffChars: Character by character diff
	 * JsDiff.diffWords: Word (as defined by \b regex) diff which ignores whitespace
	 * JsDiff.diffLines: Line based diff
	 *
	 * JsDiff.diffCss: Diff targeted at CSS content
	 *
	 * These methods are based on the implementation proposed in
	 * "An O(ND) Difference Algorithm and its Variations" (Myers, 1986).
	 * http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.4.6927
	 */


	},{"./convert/dmp":46,"./convert/xml":47,"./diff/array":48,"./diff/base":49,"./diff/character":50,"./diff/css":51,"./diff/json":52,"./diff/line":53,"./diff/sentence":54,"./diff/word":55,"./patch/apply":57,"./patch/create":58,"./patch/parse":59}],57:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports. /*istanbul ignore end*/applyPatch = applyPatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/applyPatches = applyPatches;

	var /*istanbul ignore start*/_parse = require('./parse') /*istanbul ignore end*/;

	var /*istanbul ignore start*/_distanceIterator = require('../util/distance-iterator') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	var _distanceIterator2 = _interopRequireDefault(_distanceIterator);

	function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { 'default': obj }; }

	/*istanbul ignore end*/function applyPatch(source, uniDiff) {
	  /*istanbul ignore start*/var /*istanbul ignore end*/options = arguments.length <= 2 || arguments[2] === undefined ? {} : arguments[2];

	  if (typeof uniDiff === 'string') {
	    uniDiff = /*istanbul ignore start*/(0, _parse.parsePatch) /*istanbul ignore end*/(uniDiff);
	  }

	  if (Array.isArray(uniDiff)) {
	    if (uniDiff.length > 1) {
	      throw new Error('applyPatch only works with a single input.');
	    }

	    uniDiff = uniDiff[0];
	  }

	  // Apply the diff to the input
	  var lines = source.split(/\r\n|[\n\v\f\r\x85]/),
	      delimiters = source.match(/\r\n|[\n\v\f\r\x85]/g) || [],
	      hunks = uniDiff.hunks,
	      compareLine = options.compareLine || function (lineNumber, line, operation, patchContent) /*istanbul ignore start*/{
	    return (/*istanbul ignore end*/line === patchContent
	    );
	  },
	      errorCount = 0,
	      fuzzFactor = options.fuzzFactor || 0,
	      minLine = 0,
	      offset = 0,
	      removeEOFNL = /*istanbul ignore start*/void 0 /*istanbul ignore end*/,
	      addEOFNL = /*istanbul ignore start*/void 0 /*istanbul ignore end*/;

	  /**
	   * Checks if the hunk exactly fits on the provided location
	   */
	  function hunkFits(hunk, toPos) {
	    for (var j = 0; j < hunk.lines.length; j++) {
	      var line = hunk.lines[j],
	          operation = line[0],
	          content = line.substr(1);

	      if (operation === ' ' || operation === '-') {
	        // Context sanity check
	        if (!compareLine(toPos + 1, lines[toPos], operation, content)) {
	          errorCount++;

	          if (errorCount > fuzzFactor) {
	            return false;
	          }
	        }
	        toPos++;
	      }
	    }

	    return true;
	  }

	  // Search best fit offsets for each hunk based on the previous ones
	  for (var i = 0; i < hunks.length; i++) {
	    var hunk = hunks[i],
	        maxLine = lines.length - hunk.oldLines,
	        localOffset = 0,
	        toPos = offset + hunk.oldStart - 1;

	    var iterator = /*istanbul ignore start*/(0, _distanceIterator2['default']) /*istanbul ignore end*/(toPos, minLine, maxLine);

	    for (; localOffset !== undefined; localOffset = iterator()) {
	      if (hunkFits(hunk, toPos + localOffset)) {
	        hunk.offset = offset += localOffset;
	        break;
	      }
	    }

	    if (localOffset === undefined) {
	      return false;
	    }

	    // Set lower text limit to end of the current hunk, so next ones don't try
	    // to fit over already patched text
	    minLine = hunk.offset + hunk.oldStart + hunk.oldLines;
	  }

	  // Apply patch hunks
	  for (var _i = 0; _i < hunks.length; _i++) {
	    var _hunk = hunks[_i],
	        _toPos = _hunk.offset + _hunk.newStart - 1;
	    if (_hunk.newLines == 0) {
	      _toPos++;
	    }

	    for (var j = 0; j < _hunk.lines.length; j++) {
	      var line = _hunk.lines[j],
	          operation = line[0],
	          content = line.substr(1),
	          delimiter = _hunk.linedelimiters[j];

	      if (operation === ' ') {
	        _toPos++;
	      } else if (operation === '-') {
	        lines.splice(_toPos, 1);
	        delimiters.splice(_toPos, 1);
	        /* istanbul ignore else */
	      } else if (operation === '+') {
	          lines.splice(_toPos, 0, content);
	          delimiters.splice(_toPos, 0, delimiter);
	          _toPos++;
	        } else if (operation === '\\') {
	          var previousOperation = _hunk.lines[j - 1] ? _hunk.lines[j - 1][0] : null;
	          if (previousOperation === '+') {
	            removeEOFNL = true;
	          } else if (previousOperation === '-') {
	            addEOFNL = true;
	          }
	        }
	    }
	  }

	  // Handle EOFNL insertion/removal
	  if (removeEOFNL) {
	    while (!lines[lines.length - 1]) {
	      lines.pop();
	      delimiters.pop();
	    }
	  } else if (addEOFNL) {
	    lines.push('');
	    delimiters.push('\n');
	  }
	  for (var _k = 0; _k < lines.length - 1; _k++) {
	    lines[_k] = lines[_k] + delimiters[_k];
	  }
	  return lines.join('');
	}

	// Wrapper that supports multiple file patches via callbacks.
	function applyPatches(uniDiff, options) {
	  if (typeof uniDiff === 'string') {
	    uniDiff = /*istanbul ignore start*/(0, _parse.parsePatch) /*istanbul ignore end*/(uniDiff);
	  }

	  var currentIndex = 0;
	  function processIndex() {
	    var index = uniDiff[currentIndex++];
	    if (!index) {
	      return options.complete();
	    }

	    options.loadFile(index, function (err, data) {
	      if (err) {
	        return options.complete(err);
	      }

	      var updatedContent = applyPatch(data, index, options);
	      options.patched(index, updatedContent, function (err) {
	        if (err) {
	          return options.complete(err);
	        }

	        processIndex();
	      });
	    });
	  }
	  processIndex();
	}


	},{"../util/distance-iterator":60,"./parse":59}],58:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports. /*istanbul ignore end*/structuredPatch = structuredPatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/createTwoFilesPatch = createTwoFilesPatch;
	/*istanbul ignore start*/exports. /*istanbul ignore end*/createPatch = createPatch;

	var /*istanbul ignore start*/_line = require('../diff/line') /*istanbul ignore end*/;

	/*istanbul ignore start*/
	function _toConsumableArray(arr) { if (Array.isArray(arr)) { for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) { arr2[i] = arr[i]; } return arr2; } else { return Array.from(arr); } }

	/*istanbul ignore end*/function structuredPatch(oldFileName, newFileName, oldStr, newStr, oldHeader, newHeader, options) {
	  if (!options) {
	    options = {};
	  }
	  if (typeof options.context === 'undefined') {
	    options.context = 4;
	  }

	  var diff = /*istanbul ignore start*/(0, _line.diffLines) /*istanbul ignore end*/(oldStr, newStr, options);
	  diff.push({ value: '', lines: [] }); // Append an empty value to make cleanup easier

	  function contextLines(lines) {
	    return lines.map(function (entry) {
	      return ' ' + entry;
	    });
	  }

	  var hunks = [];
	  var oldRangeStart = 0,
	      newRangeStart = 0,
	      curRange = [],
	      oldLine = 1,
	      newLine = 1;
	  /*istanbul ignore start*/
	  var _loop = function _loop( /*istanbul ignore end*/i) {
	    var current = diff[i],
	        lines = current.lines || current.value.replace(/\n$/, '').split('\n');
	    current.lines = lines;

	    if (current.added || current.removed) {
	      /*istanbul ignore start*/
	      var _curRange;

	      /*istanbul ignore end*/
	      // If we have previous context, start with that
	      if (!oldRangeStart) {
	        var prev = diff[i - 1];
	        oldRangeStart = oldLine;
	        newRangeStart = newLine;

	        if (prev) {
	          curRange = options.context > 0 ? contextLines(prev.lines.slice(-options.context)) : [];
	          oldRangeStart -= curRange.length;
	          newRangeStart -= curRange.length;
	        }
	      }

	      // Output our changes
	      /*istanbul ignore start*/(_curRange = /*istanbul ignore end*/curRange).push. /*istanbul ignore start*/apply /*istanbul ignore end*/( /*istanbul ignore start*/_curRange /*istanbul ignore end*/, /*istanbul ignore start*/_toConsumableArray( /*istanbul ignore end*/lines.map(function (entry) {
	        return (current.added ? '+' : '-') + entry;
	      })));

	      // Track the updated file position
	      if (current.added) {
	        newLine += lines.length;
	      } else {
	        oldLine += lines.length;
	      }
	    } else {
	      // Identical context lines. Track line changes
	      if (oldRangeStart) {
	        // Close out any changes that have been output (or join overlapping)
	        if (lines.length <= options.context * 2 && i < diff.length - 2) {
	          /*istanbul ignore start*/
	          var _curRange2;

	          /*istanbul ignore end*/
	          // Overlapping
	          /*istanbul ignore start*/(_curRange2 = /*istanbul ignore end*/curRange).push. /*istanbul ignore start*/apply /*istanbul ignore end*/( /*istanbul ignore start*/_curRange2 /*istanbul ignore end*/, /*istanbul ignore start*/_toConsumableArray( /*istanbul ignore end*/contextLines(lines)));
	        } else {
	          /*istanbul ignore start*/
	          var _curRange3;

	          /*istanbul ignore end*/
	          // end the range and output
	          var contextSize = Math.min(lines.length, options.context);
	          /*istanbul ignore start*/(_curRange3 = /*istanbul ignore end*/curRange).push. /*istanbul ignore start*/apply /*istanbul ignore end*/( /*istanbul ignore start*/_curRange3 /*istanbul ignore end*/, /*istanbul ignore start*/_toConsumableArray( /*istanbul ignore end*/contextLines(lines.slice(0, contextSize))));

	          var hunk = {
	            oldStart: oldRangeStart,
	            oldLines: oldLine - oldRangeStart + contextSize,
	            newStart: newRangeStart,
	            newLines: newLine - newRangeStart + contextSize,
	            lines: curRange
	          };
	          if (i >= diff.length - 2 && lines.length <= options.context) {
	            // EOF is inside this hunk
	            var oldEOFNewline = /\n$/.test(oldStr);
	            var newEOFNewline = /\n$/.test(newStr);
	            if (lines.length == 0 && !oldEOFNewline) {
	              // special case: old has no eol and no trailing context; no-nl can end up before adds
	              curRange.splice(hunk.oldLines, 0, '\\ No newline at end of file');
	            } else if (!oldEOFNewline || !newEOFNewline) {
	              curRange.push('\\ No newline at end of file');
	            }
	          }
	          hunks.push(hunk);

	          oldRangeStart = 0;
	          newRangeStart = 0;
	          curRange = [];
	        }
	      }
	      oldLine += lines.length;
	      newLine += lines.length;
	    }
	  };

	  for (var i = 0; i < diff.length; i++) {
	    /*istanbul ignore start*/
	    _loop( /*istanbul ignore end*/i);
	  }

	  return {
	    oldFileName: oldFileName, newFileName: newFileName,
	    oldHeader: oldHeader, newHeader: newHeader,
	    hunks: hunks
	  };
	}

	function createTwoFilesPatch(oldFileName, newFileName, oldStr, newStr, oldHeader, newHeader, options) {
	  var diff = structuredPatch(oldFileName, newFileName, oldStr, newStr, oldHeader, newHeader, options);

	  var ret = [];
	  if (oldFileName == newFileName) {
	    ret.push('Index: ' + oldFileName);
	  }
	  ret.push('===================================================================');
	  ret.push('--- ' + diff.oldFileName + (typeof diff.oldHeader === 'undefined' ? '' : '\t' + diff.oldHeader));
	  ret.push('+++ ' + diff.newFileName + (typeof diff.newHeader === 'undefined' ? '' : '\t' + diff.newHeader));

	  for (var i = 0; i < diff.hunks.length; i++) {
	    var hunk = diff.hunks[i];
	    ret.push('@@ -' + hunk.oldStart + ',' + hunk.oldLines + ' +' + hunk.newStart + ',' + hunk.newLines + ' @@');
	    ret.push.apply(ret, hunk.lines);
	  }

	  return ret.join('\n') + '\n';
	}

	function createPatch(fileName, oldStr, newStr, oldHeader, newHeader, options) {
	  return createTwoFilesPatch(fileName, fileName, oldStr, newStr, oldHeader, newHeader, options);
	}


	},{"../diff/line":53}],59:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports. /*istanbul ignore end*/parsePatch = parsePatch;
	function parsePatch(uniDiff) {
	  /*istanbul ignore start*/var /*istanbul ignore end*/options = arguments.length <= 1 || arguments[1] === undefined ? {} : arguments[1];

	  var diffstr = uniDiff.split(/\r\n|[\n\v\f\r\x85]/),
	      delimiters = uniDiff.match(/\r\n|[\n\v\f\r\x85]/g) || [],
	      list = [],
	      i = 0;

	  function parseIndex() {
	    var index = {};
	    list.push(index);

	    // Parse diff metadata
	    while (i < diffstr.length) {
	      var line = diffstr[i];

	      // File header found, end parsing diff metadata
	      if (/^(\-\-\-|\+\+\+|@@)\s/.test(line)) {
	        break;
	      }

	      // Diff index
	      var header = /^(?:Index:|diff(?: -r \w+)+)\s+(.+?)\s*$/.exec(line);
	      if (header) {
	        index.index = header[1];
	      }

	      i++;
	    }

	    // Parse file headers if they are defined. Unified diff requires them, but
	    // there's no technical issues to have an isolated hunk without file header
	    parseFileHeader(index);
	    parseFileHeader(index);

	    // Parse hunks
	    index.hunks = [];

	    while (i < diffstr.length) {
	      var _line = diffstr[i];

	      if (/^(Index:|diff|\-\-\-|\+\+\+)\s/.test(_line)) {
	        break;
	      } else if (/^@@/.test(_line)) {
	        index.hunks.push(parseHunk());
	      } else if (_line && options.strict) {
	        // Ignore unexpected content unless in strict mode
	        throw new Error('Unknown line ' + (i + 1) + ' ' + JSON.stringify(_line));
	      } else {
	        i++;
	      }
	    }
	  }

	  // Parses the --- and +++ headers, if none are found, no lines
	  // are consumed.
	  function parseFileHeader(index) {
	    var headerPattern = /^(---|\+\+\+)\s+([\S ]*)(?:\t(.*?)\s*)?$/;
	    var fileHeader = headerPattern.exec(diffstr[i]);
	    if (fileHeader) {
	      var keyPrefix = fileHeader[1] === '---' ? 'old' : 'new';
	      index[keyPrefix + 'FileName'] = fileHeader[2];
	      index[keyPrefix + 'Header'] = fileHeader[3];

	      i++;
	    }
	  }

	  // Parses a hunk
	  // This assumes that we are at the start of a hunk.
	  function parseHunk() {
	    var chunkHeaderIndex = i,
	        chunkHeaderLine = diffstr[i++],
	        chunkHeader = chunkHeaderLine.split(/@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/);

	    var hunk = {
	      oldStart: +chunkHeader[1],
	      oldLines: +chunkHeader[2] || 1,
	      newStart: +chunkHeader[3],
	      newLines: +chunkHeader[4] || 1,
	      lines: [],
	      linedelimiters: []
	    };

	    var addCount = 0,
	        removeCount = 0;
	    for (; i < diffstr.length; i++) {
	      // Lines starting with '---' could be mistaken for the "remove line" operation
	      // But they could be the header for the next file. Therefore prune such cases out.
	      if (diffstr[i].indexOf('--- ') === 0 && i + 2 < diffstr.length && diffstr[i + 1].indexOf('+++ ') === 0 && diffstr[i + 2].indexOf('@@') === 0) {
	        break;
	      }
	      var operation = diffstr[i][0];

	      if (operation === '+' || operation === '-' || operation === ' ' || operation === '\\') {
	        hunk.lines.push(diffstr[i]);
	        hunk.linedelimiters.push(delimiters[i] || '\n');

	        if (operation === '+') {
	          addCount++;
	        } else if (operation === '-') {
	          removeCount++;
	        } else if (operation === ' ') {
	          addCount++;
	          removeCount++;
	        }
	      } else {
	        break;
	      }
	    }

	    // Handle the empty block count case
	    if (!addCount && hunk.newLines === 1) {
	      hunk.newLines = 0;
	    }
	    if (!removeCount && hunk.oldLines === 1) {
	      hunk.oldLines = 0;
	    }

	    // Perform optional sanity checking
	    if (options.strict) {
	      if (addCount !== hunk.newLines) {
	        throw new Error('Added line count did not match for hunk at line ' + (chunkHeaderIndex + 1));
	      }
	      if (removeCount !== hunk.oldLines) {
	        throw new Error('Removed line count did not match for hunk at line ' + (chunkHeaderIndex + 1));
	      }
	    }

	    return hunk;
	  }

	  while (i < diffstr.length) {
	    parseIndex();
	  }

	  return list;
	}


	},{}],60:[function(require,module,exports){
	/*istanbul ignore start*/"use strict";

	exports.__esModule = true;

	exports["default"] = /*istanbul ignore end*/function (start, minLine, maxLine) {
	  var wantForward = true,
	      backwardExhausted = false,
	      forwardExhausted = false,
	      localOffset = 1;

	  return function iterator() {
	    if (wantForward && !forwardExhausted) {
	      if (backwardExhausted) {
	        localOffset++;
	      } else {
	        wantForward = false;
	      }

	      // Check if trying to fit beyond text length, and if not, check it fits
	      // after offset location (or desired location on first iteration)
	      if (start + localOffset <= maxLine) {
	        return localOffset;
	      }

	      forwardExhausted = true;
	    }

	    if (!backwardExhausted) {
	      if (!forwardExhausted) {
	        wantForward = true;
	      }

	      // Check if trying to fit before text beginning, and if not, check it fits
	      // before offset location
	      if (minLine <= start - localOffset) {
	        return -localOffset++;
	      }

	      backwardExhausted = true;
	      return iterator();
	    }

	    // We tried to fit hunk before text beginning and beyond text lenght, then
	    // hunk can't fit on the text. Return undefined
	  };
	};


	},{}],61:[function(require,module,exports){
	/*istanbul ignore start*/'use strict';

	exports.__esModule = true;
	exports. /*istanbul ignore end*/generateOptions = generateOptions;
	function generateOptions(options, defaults) {
	  if (typeof options === 'function') {
	    defaults.callback = options;
	  } else if (options) {
	    for (var name in options) {
	      /* istanbul ignore else */
	      if (options.hasOwnProperty(name)) {
	        defaults[name] = options[name];
	      }
	    }
	  }
	  return defaults;
	}


	},{}],62:[function(require,module,exports){
	'use strict';

	var matchOperatorsRe = /[|\\{}()[\]^$+*?.]/g;

	module.exports = function (str) {
		if (typeof str !== 'string') {
			throw new TypeError('Expected a string');
		}

		return str.replace(matchOperatorsRe, '\\$&');
	};

	},{}],63:[function(require,module,exports){
	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	function EventEmitter() {
	  this._events = this._events || {};
	  this._maxListeners = this._maxListeners || undefined;
	}
	module.exports = EventEmitter;

	// Backwards-compat with node 0.10.x
	EventEmitter.EventEmitter = EventEmitter;

	EventEmitter.prototype._events = undefined;
	EventEmitter.prototype._maxListeners = undefined;

	// By default EventEmitters will print a warning if more than 10 listeners are
	// added to it. This is a useful default which helps finding memory leaks.
	EventEmitter.defaultMaxListeners = 10;

	// Obviously not all Emitters should be limited to 10. This function allows
	// that to be increased. Set to zero for unlimited.
	EventEmitter.prototype.setMaxListeners = function(n) {
	  if (!isNumber(n) || n < 0 || isNaN(n))
	    throw TypeError('n must be a positive number');
	  this._maxListeners = n;
	  return this;
	};

	EventEmitter.prototype.emit = function(type) {
	  var er, handler, len, args, i, listeners;

	  if (!this._events)
	    this._events = {};

	  // If there is no 'error' event listener then throw.
	  if (type === 'error') {
	    if (!this._events.error ||
	        (isObject(this._events.error) && !this._events.error.length)) {
	      er = arguments[1];
	      if (er instanceof Error) {
	        throw er; // Unhandled 'error' event
	      } else {
	        // At least give some kind of context to the user
	        var err = new Error('Uncaught, unspecified "error" event. (' + er + ')');
	        err.context = er;
	        throw err;
	      }
	    }
	  }

	  handler = this._events[type];

	  if (isUndefined(handler))
	    return false;

	  if (isFunction(handler)) {
	    switch (arguments.length) {
	      // fast cases
	      case 1:
	        handler.call(this);
	        break;
	      case 2:
	        handler.call(this, arguments[1]);
	        break;
	      case 3:
	        handler.call(this, arguments[1], arguments[2]);
	        break;
	      // slower
	      default:
	        args = Array.prototype.slice.call(arguments, 1);
	        handler.apply(this, args);
	    }
	  } else if (isObject(handler)) {
	    args = Array.prototype.slice.call(arguments, 1);
	    listeners = handler.slice();
	    len = listeners.length;
	    for (i = 0; i < len; i++)
	      listeners[i].apply(this, args);
	  }

	  return true;
	};

	EventEmitter.prototype.addListener = function(type, listener) {
	  var m;

	  if (!isFunction(listener))
	    throw TypeError('listener must be a function');

	  if (!this._events)
	    this._events = {};

	  // To avoid recursion in the case that type === "newListener"! Before
	  // adding it to the listeners, first emit "newListener".
	  if (this._events.newListener)
	    this.emit('newListener', type,
	              isFunction(listener.listener) ?
	              listener.listener : listener);

	  if (!this._events[type])
	    // Optimize the case of one listener. Don't need the extra array object.
	    this._events[type] = listener;
	  else if (isObject(this._events[type]))
	    // If we've already got an array, just append.
	    this._events[type].push(listener);
	  else
	    // Adding the second element, need to change to array.
	    this._events[type] = [this._events[type], listener];

	  // Check for listener leak
	  if (isObject(this._events[type]) && !this._events[type].warned) {
	    if (!isUndefined(this._maxListeners)) {
	      m = this._maxListeners;
	    } else {
	      m = EventEmitter.defaultMaxListeners;
	    }

	    if (m && m > 0 && this._events[type].length > m) {
	      this._events[type].warned = true;
	      console.error('(node) warning: possible EventEmitter memory ' +
	                    'leak detected. %d listeners added. ' +
	                    'Use emitter.setMaxListeners() to increase limit.',
	                    this._events[type].length);
	      if (typeof console.trace === 'function') {
	        // not supported in IE 10
	        console.trace();
	      }
	    }
	  }

	  return this;
	};

	EventEmitter.prototype.on = EventEmitter.prototype.addListener;

	EventEmitter.prototype.once = function(type, listener) {
	  if (!isFunction(listener))
	    throw TypeError('listener must be a function');

	  var fired = false;

	  function g() {
	    this.removeListener(type, g);

	    if (!fired) {
	      fired = true;
	      listener.apply(this, arguments);
	    }
	  }

	  g.listener = listener;
	  this.on(type, g);

	  return this;
	};

	// emits a 'removeListener' event iff the listener was removed
	EventEmitter.prototype.removeListener = function(type, listener) {
	  var list, position, length, i;

	  if (!isFunction(listener))
	    throw TypeError('listener must be a function');

	  if (!this._events || !this._events[type])
	    return this;

	  list = this._events[type];
	  length = list.length;
	  position = -1;

	  if (list === listener ||
	      (isFunction(list.listener) && list.listener === listener)) {
	    delete this._events[type];
	    if (this._events.removeListener)
	      this.emit('removeListener', type, listener);

	  } else if (isObject(list)) {
	    for (i = length; i-- > 0;) {
	      if (list[i] === listener ||
	          (list[i].listener && list[i].listener === listener)) {
	        position = i;
	        break;
	      }
	    }

	    if (position < 0)
	      return this;

	    if (list.length === 1) {
	      list.length = 0;
	      delete this._events[type];
	    } else {
	      list.splice(position, 1);
	    }

	    if (this._events.removeListener)
	      this.emit('removeListener', type, listener);
	  }

	  return this;
	};

	EventEmitter.prototype.removeAllListeners = function(type) {
	  var key, listeners;

	  if (!this._events)
	    return this;

	  // not listening for removeListener, no need to emit
	  if (!this._events.removeListener) {
	    if (arguments.length === 0)
	      this._events = {};
	    else if (this._events[type])
	      delete this._events[type];
	    return this;
	  }

	  // emit removeListener for all listeners on all events
	  if (arguments.length === 0) {
	    for (key in this._events) {
	      if (key === 'removeListener') continue;
	      this.removeAllListeners(key);
	    }
	    this.removeAllListeners('removeListener');
	    this._events = {};
	    return this;
	  }

	  listeners = this._events[type];

	  if (isFunction(listeners)) {
	    this.removeListener(type, listeners);
	  } else if (listeners) {
	    // LIFO order
	    while (listeners.length)
	      this.removeListener(type, listeners[listeners.length - 1]);
	  }
	  delete this._events[type];

	  return this;
	};

	EventEmitter.prototype.listeners = function(type) {
	  var ret;
	  if (!this._events || !this._events[type])
	    ret = [];
	  else if (isFunction(this._events[type]))
	    ret = [this._events[type]];
	  else
	    ret = this._events[type].slice();
	  return ret;
	};

	EventEmitter.prototype.listenerCount = function(type) {
	  if (this._events) {
	    var evlistener = this._events[type];

	    if (isFunction(evlistener))
	      return 1;
	    else if (evlistener)
	      return evlistener.length;
	  }
	  return 0;
	};

	EventEmitter.listenerCount = function(emitter, type) {
	  return emitter.listenerCount(type);
	};

	function isFunction(arg) {
	  return typeof arg === 'function';
	}

	function isNumber(arg) {
	  return typeof arg === 'number';
	}

	function isObject(arg) {
	  return typeof arg === 'object' && arg !== null;
	}

	function isUndefined(arg) {
	  return arg === void 0;
	}

	},{}],64:[function(require,module,exports){
	(function (process){
	// Growl - Copyright TJ Holowaychuk <tj@vision-media.ca> (MIT Licensed)

	/**
	 * Module dependencies.
	 */

	var exec = require('child_process').exec
	  , fs = require('fs')
	  , path = require('path')
	  , exists = fs.existsSync || path.existsSync
	  , os = require('os')
	  , quote = JSON.stringify
	  , cmd;

	function which(name) {
	  var paths = process.env.PATH.split(':');
	  var loc;

	  for (var i = 0, len = paths.length; i < len; ++i) {
	    loc = path.join(paths[i], name);
	    if (exists(loc)) return loc;
	  }
	}

	switch(os.type()) {
	  case 'Darwin':
	    if (which('terminal-notifier')) {
	      cmd = {
	          type: "Darwin-NotificationCenter"
	        , pkg: "terminal-notifier"
	        , msg: '-message'
	        , title: '-title'
	        , subtitle: '-subtitle'
	        , icon: '-appIcon'
	        , sound:  '-sound'
	        , url: '-open'
	        , priority: {
	              cmd: '-execute'
	            , range: []
	          }
	      };
	    } else {
	      cmd = {
	          type: "Darwin-Growl"
	        , pkg: "growlnotify"
	        , msg: '-m'
	        , sticky: '--sticky'
	        , priority: {
	              cmd: '--priority'
	            , range: [
	                -2
	              , -1
	              , 0
	              , 1
	              , 2
	              , "Very Low"
	              , "Moderate"
	              , "Normal"
	              , "High"
	              , "Emergency"
	            ]
	          }
	      };
	    }
	    break;
	  case 'Linux':
	    if (which('growl')) {
	      cmd = {
	          type: "Linux-Growl"
	        , pkg: "growl"
	        , msg: '-m'
	        , title: '-title'
	        , subtitle: '-subtitle'
	        , host: {
	            cmd: '-H'
	          , hostname: '192.168.33.1'
	        }
	      };
	    } else {
	      cmd = {
	          type: "Linux"
	        , pkg: "notify-send"
	        , msg: ''
	        , sticky: '-t 0'
	        , icon: '-i'
	        , priority: {
	            cmd: '-u'
	          , range: [
	              "low"
	            , "normal"
	            , "critical"
	          ]
	        }
	      };
	    }
	    break;
	  case 'Windows_NT':
	    cmd = {
	        type: "Windows"
	      , pkg: "growlnotify"
	      , msg: ''
	      , sticky: '/s:true'
	      , title: '/t:'
	      , icon: '/i:'
	      , url: '/cu:'
	      , priority: {
	            cmd: '/p:'
	          , range: [
	              -2
	            , -1
	            , 0
	            , 1
	            , 2
	          ]
	        }
	    };
	    break;
	}

	/**
	 * Expose `growl`.
	 */

	exports = module.exports = growl;

	/**
	 * Node-growl version.
	 */

	exports.version = '1.4.1'

	/**
	 * Send growl notification _msg_ with _options_.
	 *
	 * Options:
	 *
	 *  - title   Notification title
	 *  - sticky  Make the notification stick (defaults to false)
	 *  - priority  Specify an int or named key (default is 0)
	 *  - name    Application name (defaults to growlnotify)
	 *  - sound   Sound efect ( in OSx defined in preferences -> sound -> effects) * works only in OSX > 10.8x
	 *  - image
	 *    - path to an icon sets --iconpath
	 *    - path to an image sets --image
	 *    - capitalized word sets --appIcon
	 *    - filename uses extname as --icon
	 *    - otherwise treated as --icon
	 *
	 * Examples:
	 *
	 *   growl('New email')
	 *   growl('5 new emails', { title: 'Thunderbird' })
	 *   growl('5 new emails', { title: 'Thunderbird', sound: 'Purr' })
	 *   growl('Email sent', function(){
	 *     // ... notification sent
	 *   })
	 *
	 * @param {string} msg
	 * @param {object} options
	 * @param {function} fn
	 * @api public
	 */

	function growl(msg, options, fn) {
	  var image
	    , args
	    , options = options || {}
	    , fn = fn || function(){};

	  if (options.exec) {
	    cmd = {
	        type: "Custom"
	      , pkg: options.exec
	      , range: []
	    };
	  }

	  // noop
	  if (!cmd) return fn(new Error('growl not supported on this platform'));
	  args = [cmd.pkg];

	  // image
	  if (image = options.image) {
	    switch(cmd.type) {
	      case 'Darwin-Growl':
	        var flag, ext = path.extname(image).substr(1)
	        flag = flag || ext == 'icns' && 'iconpath'
	        flag = flag || /^[A-Z]/.test(image) && 'appIcon'
	        flag = flag || /^png|gif|jpe?g$/.test(ext) && 'image'
	        flag = flag || ext && (image = ext) && 'icon'
	        flag = flag || 'icon'
	        args.push('--' + flag, quote(image))
	        break;
	      case 'Darwin-NotificationCenter':
	        args.push(cmd.icon, quote(image));
	        break;
	      case 'Linux':
	        args.push(cmd.icon, quote(image));
	        // libnotify defaults to sticky, set a hint for transient notifications
	        if (!options.sticky) args.push('--hint=int:transient:1');
	        break;
	      case 'Windows':
	        args.push(cmd.icon + quote(image));
	        break;
	    }
	  }

	  // sticky
	  if (options.sticky) args.push(cmd.sticky);

	  // priority
	  if (options.priority) {
	    var priority = options.priority + '';
	    var checkindexOf = cmd.priority.range.indexOf(priority);
	    if (~cmd.priority.range.indexOf(priority)) {
	      args.push(cmd.priority, options.priority);
	    }
	  }

	  //sound
	  if(options.sound && cmd.type === 'Darwin-NotificationCenter'){
	    args.push(cmd.sound, options.sound)
	  }

	  // name
	  if (options.name && cmd.type === "Darwin-Growl") {
	    args.push('--name', options.name);
	  }

	  switch(cmd.type) {
	    case 'Darwin-Growl':
	      args.push(cmd.msg);
	      args.push(quote(msg).replace(/\\n/g, '\n'));
	      if (options.title) args.push(quote(options.title));
	      break;
	    case 'Darwin-NotificationCenter':
	      args.push(cmd.msg);
	      var stringifiedMsg = quote(msg);
	      var escapedMsg = stringifiedMsg.replace(/\\n/g, '\n');
	      args.push(escapedMsg);
	      if (options.title) {
	        args.push(cmd.title);
	        args.push(quote(options.title));
	      }
	      if (options.subtitle) {
	        args.push(cmd.subtitle);
	        args.push(quote(options.subtitle));
	      }
	      if (options.url) {
	        args.push(cmd.url);
	        args.push(quote(options.url));
	      }
	      break;
	    case 'Linux-Growl':
	      args.push(cmd.msg);
	      args.push(quote(msg).replace(/\\n/g, '\n'));
	      if (options.title) args.push(quote(options.title));
	      if (cmd.host) {
	        args.push(cmd.host.cmd, cmd.host.hostname)
	      }
	      break;
	    case 'Linux':
	      if (options.title) {
	        args.push(quote(options.title));
	        args.push(cmd.msg);
	        args.push(quote(msg).replace(/\\n/g, '\n'));
	      } else {
	        args.push(quote(msg).replace(/\\n/g, '\n'));
	      }
	      break;
	    case 'Windows':
	      args.push(quote(msg).replace(/\\n/g, '\n'));
	      if (options.title) args.push(cmd.title + quote(options.title));
	      if (options.url) args.push(cmd.url + quote(options.url));
	      break;
	    case 'Custom':
	      args[0] = (function(origCommand) {
	        var message = options.title
	          ? options.title + ': ' + msg
	          : msg;
	        var command = origCommand.replace(/(^|[^%])%s/g, '$1' + quote(message));
	        if (command === origCommand) args.push(quote(message));
	        return command;
	      })(args[0]);
	      break;
	  }

	  // execute
	  exec(args.join(' '), fn);
	};

	}).call(this,require('_process'))
	},{"_process":82,"child_process":42,"fs":42,"os":80,"path":42}],65:[function(require,module,exports){
	exports.read = function (buffer, offset, isLE, mLen, nBytes) {
	  var e, m
	  var eLen = nBytes * 8 - mLen - 1
	  var eMax = (1 << eLen) - 1
	  var eBias = eMax >> 1
	  var nBits = -7
	  var i = isLE ? (nBytes - 1) : 0
	  var d = isLE ? -1 : 1
	  var s = buffer[offset + i]

	  i += d

	  e = s & ((1 << (-nBits)) - 1)
	  s >>= (-nBits)
	  nBits += eLen
	  for (; nBits > 0; e = e * 256 + buffer[offset + i], i += d, nBits -= 8) {}

	  m = e & ((1 << (-nBits)) - 1)
	  e >>= (-nBits)
	  nBits += mLen
	  for (; nBits > 0; m = m * 256 + buffer[offset + i], i += d, nBits -= 8) {}

	  if (e === 0) {
	    e = 1 - eBias
	  } else if (e === eMax) {
	    return m ? NaN : ((s ? -1 : 1) * Infinity)
	  } else {
	    m = m + Math.pow(2, mLen)
	    e = e - eBias
	  }
	  return (s ? -1 : 1) * m * Math.pow(2, e - mLen)
	}

	exports.write = function (buffer, value, offset, isLE, mLen, nBytes) {
	  var e, m, c
	  var eLen = nBytes * 8 - mLen - 1
	  var eMax = (1 << eLen) - 1
	  var eBias = eMax >> 1
	  var rt = (mLen === 23 ? Math.pow(2, -24) - Math.pow(2, -77) : 0)
	  var i = isLE ? 0 : (nBytes - 1)
	  var d = isLE ? 1 : -1
	  var s = value < 0 || (value === 0 && 1 / value < 0) ? 1 : 0

	  value = Math.abs(value)

	  if (isNaN(value) || value === Infinity) {
	    m = isNaN(value) ? 1 : 0
	    e = eMax
	  } else {
	    e = Math.floor(Math.log(value) / Math.LN2)
	    if (value * (c = Math.pow(2, -e)) < 1) {
	      e--
	      c *= 2
	    }
	    if (e + eBias >= 1) {
	      value += rt / c
	    } else {
	      value += rt * Math.pow(2, 1 - eBias)
	    }
	    if (value * c >= 2) {
	      e++
	      c /= 2
	    }

	    if (e + eBias >= eMax) {
	      m = 0
	      e = eMax
	    } else if (e + eBias >= 1) {
	      m = (value * c - 1) * Math.pow(2, mLen)
	      e = e + eBias
	    } else {
	      m = value * Math.pow(2, eBias - 1) * Math.pow(2, mLen)
	      e = 0
	    }
	  }

	  for (; mLen >= 8; buffer[offset + i] = m & 0xff, i += d, m /= 256, mLen -= 8) {}

	  e = (e << mLen) | m
	  eLen += mLen
	  for (; eLen > 0; buffer[offset + i] = e & 0xff, i += d, e /= 256, eLen -= 8) {}

	  buffer[offset + i - d] |= s * 128
	}

	},{}],66:[function(require,module,exports){
	if (typeof Object.create === 'function') {
	  // implementation from standard node.js 'util' module
	  module.exports = function inherits(ctor, superCtor) {
	    ctor.super_ = superCtor
	    ctor.prototype = Object.create(superCtor.prototype, {
	      constructor: {
	        value: ctor,
	        enumerable: false,
	        writable: true,
	        configurable: true
	      }
	    });
	  };
	} else {
	  // old school shim for old browsers
	  module.exports = function inherits(ctor, superCtor) {
	    ctor.super_ = superCtor
	    var TempCtor = function () {}
	    TempCtor.prototype = superCtor.prototype
	    ctor.prototype = new TempCtor()
	    ctor.prototype.constructor = ctor
	  }
	}

	},{}],67:[function(require,module,exports){
	/*!
	 * Determine if an object is a Buffer
	 *
	 * @author   Feross Aboukhadijeh <feross@feross.org> <http://feross.org>
	 * @license  MIT
	 */

	// The _isBuffer check is for Safari 5-7 support, because it's missing
	// Object.prototype.constructor. Remove this eventually
	module.exports = function (obj) {
	  return obj != null && (isBuffer(obj) || isSlowBuffer(obj) || !!obj._isBuffer)
	}

	function isBuffer (obj) {
	  return !!obj.constructor && typeof obj.constructor.isBuffer === 'function' && obj.constructor.isBuffer(obj)
	}

	// For Node v0.10 support. Remove this eventually.
	function isSlowBuffer (obj) {
	  return typeof obj.readFloatLE === 'function' && typeof obj.slice === 'function' && isBuffer(obj.slice(0, 0))
	}

	},{}],68:[function(require,module,exports){
	var toString = {}.toString;

	module.exports = Array.isArray || function (arr) {
	  return toString.call(arr) == '[object Array]';
	};

	},{}],69:[function(require,module,exports){
	(function (global){
	/*! JSON v3.3.2 | http://bestiejs.github.io/json3 | Copyright 2012-2014, Kit Cambridge | http://kit.mit-license.org */
	;(function () {
	  // Detect the `define` function exposed by asynchronous module loaders. The
	  // strict `define` check is necessary for compatibility with `r.js`.
	  var isLoader = false;

	  // A set of types used to distinguish objects from primitives.
	  var objectTypes = {
	    "function": true,
	    "object": true
	  };

	  // Detect the `exports` object exposed by CommonJS implementations.
	  var freeExports = objectTypes[typeof exports] && exports && !exports.nodeType && exports;

	  // Use the `global` object exposed by Node (including Browserify via
	  // `insert-module-globals`), Narwhal, and Ringo as the default context,
	  // and the `window` object in browsers. Rhino exports a `global` function
	  // instead.
	  var root = objectTypes[typeof window] && window || this,
	      freeGlobal = freeExports && objectTypes[typeof module] && module && !module.nodeType && typeof global == "object" && global;

	  if (freeGlobal && (freeGlobal["global"] === freeGlobal || freeGlobal["window"] === freeGlobal || freeGlobal["self"] === freeGlobal)) {
	    root = freeGlobal;
	  }

	  // Public: Initializes JSON 3 using the given `context` object, attaching the
	  // `stringify` and `parse` functions to the specified `exports` object.
	  function runInContext(context, exports) {
	    context || (context = root["Object"]());
	    exports || (exports = root["Object"]());

	    // Native constructor aliases.
	    var Number = context["Number"] || root["Number"],
	        String = context["String"] || root["String"],
	        Object = context["Object"] || root["Object"],
	        Date = context["Date"] || root["Date"],
	        SyntaxError = context["SyntaxError"] || root["SyntaxError"],
	        TypeError = context["TypeError"] || root["TypeError"],
	        Math = context["Math"] || root["Math"],
	        nativeJSON = context["JSON"] || root["JSON"];

	    // Delegate to the native `stringify` and `parse` implementations.
	    if (typeof nativeJSON == "object" && nativeJSON) {
	      exports.stringify = nativeJSON.stringify;
	      exports.parse = nativeJSON.parse;
	    }

	    // Convenience aliases.
	    var objectProto = Object.prototype,
	        getClass = objectProto.toString,
	        isProperty, forEach, undef;

	    // Test the `Date#getUTC*` methods. Based on work by @Yaffle.
	    var isExtended = new Date(-3509827334573292);
	    try {
	      // The `getUTCFullYear`, `Month`, and `Date` methods return nonsensical
	      // results for certain dates in Opera >= 10.53.
	      isExtended = isExtended.getUTCFullYear() == -109252 && isExtended.getUTCMonth() === 0 && isExtended.getUTCDate() === 1 &&
	        // Safari < 2.0.2 stores the internal millisecond time value correctly,
	        // but clips the values returned by the date methods to the range of
	        // signed 32-bit integers ([-2 ** 31, 2 ** 31 - 1]).
	        isExtended.getUTCHours() == 10 && isExtended.getUTCMinutes() == 37 && isExtended.getUTCSeconds() == 6 && isExtended.getUTCMilliseconds() == 708;
	    } catch (exception) {}

	    // Internal: Determines whether the native `JSON.stringify` and `parse`
	    // implementations are spec-compliant. Based on work by Ken Snyder.
	    function has(name) {
	      if (has[name] !== undef) {
	        // Return cached feature test result.
	        return has[name];
	      }
	      var isSupported;
	      if (name == "bug-string-char-index") {
	        // IE <= 7 doesn't support accessing string characters using square
	        // bracket notation. IE 8 only supports this for primitives.
	        isSupported = "a"[0] != "a";
	      } else if (name == "json") {
	        // Indicates whether both `JSON.stringify` and `JSON.parse` are
	        // supported.
	        isSupported = has("json-stringify") && has("json-parse");
	      } else {
	        var value, serialized = '{"a":[1,true,false,null,"\\u0000\\b\\n\\f\\r\\t"]}';
	        // Test `JSON.stringify`.
	        if (name == "json-stringify") {
	          var stringify = exports.stringify, stringifySupported = typeof stringify == "function" && isExtended;
	          if (stringifySupported) {
	            // A test function object with a custom `toJSON` method.
	            (value = function () {
	              return 1;
	            }).toJSON = value;
	            try {
	              stringifySupported =
	                // Firefox 3.1b1 and b2 serialize string, number, and boolean
	                // primitives as object literals.
	                stringify(0) === "0" &&
	                // FF 3.1b1, b2, and JSON 2 serialize wrapped primitives as object
	                // literals.
	                stringify(new Number()) === "0" &&
	                stringify(new String()) == '""' &&
	                // FF 3.1b1, 2 throw an error if the value is `null`, `undefined`, or
	                // does not define a canonical JSON representation (this applies to
	                // objects with `toJSON` properties as well, *unless* they are nested
	                // within an object or array).
	                stringify(getClass) === undef &&
	                // IE 8 serializes `undefined` as `"undefined"`. Safari <= 5.1.7 and
	                // FF 3.1b3 pass this test.
	                stringify(undef) === undef &&
	                // Safari <= 5.1.7 and FF 3.1b3 throw `Error`s and `TypeError`s,
	                // respectively, if the value is omitted entirely.
	                stringify() === undef &&
	                // FF 3.1b1, 2 throw an error if the given value is not a number,
	                // string, array, object, Boolean, or `null` literal. This applies to
	                // objects with custom `toJSON` methods as well, unless they are nested
	                // inside object or array literals. YUI 3.0.0b1 ignores custom `toJSON`
	                // methods entirely.
	                stringify(value) === "1" &&
	                stringify([value]) == "[1]" &&
	                // Prototype <= 1.6.1 serializes `[undefined]` as `"[]"` instead of
	                // `"[null]"`.
	                stringify([undef]) == "[null]" &&
	                // YUI 3.0.0b1 fails to serialize `null` literals.
	                stringify(null) == "null" &&
	                // FF 3.1b1, 2 halts serialization if an array contains a function:
	                // `[1, true, getClass, 1]` serializes as "[1,true,],". FF 3.1b3
	                // elides non-JSON values from objects and arrays, unless they
	                // define custom `toJSON` methods.
	                stringify([undef, getClass, null]) == "[null,null,null]" &&
	                // Simple serialization test. FF 3.1b1 uses Unicode escape sequences
	                // where character escape codes are expected (e.g., `\b` => `\u0008`).
	                stringify({ "a": [value, true, false, null, "\x00\b\n\f\r\t"] }) == serialized &&
	                // FF 3.1b1 and b2 ignore the `filter` and `width` arguments.
	                stringify(null, value) === "1" &&
	                stringify([1, 2], null, 1) == "[\n 1,\n 2\n]" &&
	                // JSON 2, Prototype <= 1.7, and older WebKit builds incorrectly
	                // serialize extended years.
	                stringify(new Date(-8.64e15)) == '"-271821-04-20T00:00:00.000Z"' &&
	                // The milliseconds are optional in ES 5, but required in 5.1.
	                stringify(new Date(8.64e15)) == '"+275760-09-13T00:00:00.000Z"' &&
	                // Firefox <= 11.0 incorrectly serializes years prior to 0 as negative
	                // four-digit years instead of six-digit years. Credits: @Yaffle.
	                stringify(new Date(-621987552e5)) == '"-000001-01-01T00:00:00.000Z"' &&
	                // Safari <= 5.1.5 and Opera >= 10.53 incorrectly serialize millisecond
	                // values less than 1000. Credits: @Yaffle.
	                stringify(new Date(-1)) == '"1969-12-31T23:59:59.999Z"';
	            } catch (exception) {
	              stringifySupported = false;
	            }
	          }
	          isSupported = stringifySupported;
	        }
	        // Test `JSON.parse`.
	        if (name == "json-parse") {
	          var parse = exports.parse;
	          if (typeof parse == "function") {
	            try {
	              // FF 3.1b1, b2 will throw an exception if a bare literal is provided.
	              // Conforming implementations should also coerce the initial argument to
	              // a string prior to parsing.
	              if (parse("0") === 0 && !parse(false)) {
	                // Simple parsing test.
	                value = parse(serialized);
	                var parseSupported = value["a"].length == 5 && value["a"][0] === 1;
	                if (parseSupported) {
	                  try {
	                    // Safari <= 5.1.2 and FF 3.1b1 allow unescaped tabs in strings.
	                    parseSupported = !parse('"\t"');
	                  } catch (exception) {}
	                  if (parseSupported) {
	                    try {
	                      // FF 4.0 and 4.0.1 allow leading `+` signs and leading
	                      // decimal points. FF 4.0, 4.0.1, and IE 9-10 also allow
	                      // certain octal literals.
	                      parseSupported = parse("01") !== 1;
	                    } catch (exception) {}
	                  }
	                  if (parseSupported) {
	                    try {
	                      // FF 4.0, 4.0.1, and Rhino 1.7R3-R4 allow trailing decimal
	                      // points. These environments, along with FF 3.1b1 and 2,
	                      // also allow trailing commas in JSON objects and arrays.
	                      parseSupported = parse("1.") !== 1;
	                    } catch (exception) {}
	                  }
	                }
	              }
	            } catch (exception) {
	              parseSupported = false;
	            }
	          }
	          isSupported = parseSupported;
	        }
	      }
	      return has[name] = !!isSupported;
	    }

	    if (!has("json")) {
	      // Common `[[Class]]` name aliases.
	      var functionClass = "[object Function]",
	          dateClass = "[object Date]",
	          numberClass = "[object Number]",
	          stringClass = "[object String]",
	          arrayClass = "[object Array]",
	          booleanClass = "[object Boolean]";

	      // Detect incomplete support for accessing string characters by index.
	      var charIndexBuggy = has("bug-string-char-index");

	      // Define additional utility methods if the `Date` methods are buggy.
	      if (!isExtended) {
	        var floor = Math.floor;
	        // A mapping between the months of the year and the number of days between
	        // January 1st and the first of the respective month.
	        var Months = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];
	        // Internal: Calculates the number of days between the Unix epoch and the
	        // first day of the given month.
	        var getDay = function (year, month) {
	          return Months[month] + 365 * (year - 1970) + floor((year - 1969 + (month = +(month > 1))) / 4) - floor((year - 1901 + month) / 100) + floor((year - 1601 + month) / 400);
	        };
	      }

	      // Internal: Determines if a property is a direct property of the given
	      // object. Delegates to the native `Object#hasOwnProperty` method.
	      if (!(isProperty = objectProto.hasOwnProperty)) {
	        isProperty = function (property) {
	          var members = {}, constructor;
	          if ((members.__proto__ = null, members.__proto__ = {
	            // The *proto* property cannot be set multiple times in recent
	            // versions of Firefox and SeaMonkey.
	            "toString": 1
	          }, members).toString != getClass) {
	            // Safari <= 2.0.3 doesn't implement `Object#hasOwnProperty`, but
	            // supports the mutable *proto* property.
	            isProperty = function (property) {
	              // Capture and break the object's prototype chain (see section 8.6.2
	              // of the ES 5.1 spec). The parenthesized expression prevents an
	              // unsafe transformation by the Closure Compiler.
	              var original = this.__proto__, result = property in (this.__proto__ = null, this);
	              // Restore the original prototype chain.
	              this.__proto__ = original;
	              return result;
	            };
	          } else {
	            // Capture a reference to the top-level `Object` constructor.
	            constructor = members.constructor;
	            // Use the `constructor` property to simulate `Object#hasOwnProperty` in
	            // other environments.
	            isProperty = function (property) {
	              var parent = (this.constructor || constructor).prototype;
	              return property in this && !(property in parent && this[property] === parent[property]);
	            };
	          }
	          members = null;
	          return isProperty.call(this, property);
	        };
	      }

	      // Internal: Normalizes the `for...in` iteration algorithm across
	      // environments. Each enumerated key is yielded to a `callback` function.
	      forEach = function (object, callback) {
	        var size = 0, Properties, members, property;

	        // Tests for bugs in the current environment's `for...in` algorithm. The
	        // `valueOf` property inherits the non-enumerable flag from
	        // `Object.prototype` in older versions of IE, Netscape, and Mozilla.
	        (Properties = function () {
	          this.valueOf = 0;
	        }).prototype.valueOf = 0;

	        // Iterate over a new instance of the `Properties` class.
	        members = new Properties();
	        for (property in members) {
	          // Ignore all properties inherited from `Object.prototype`.
	          if (isProperty.call(members, property)) {
	            size++;
	          }
	        }
	        Properties = members = null;

	        // Normalize the iteration algorithm.
	        if (!size) {
	          // A list of non-enumerable properties inherited from `Object.prototype`.
	          members = ["valueOf", "toString", "toLocaleString", "propertyIsEnumerable", "isPrototypeOf", "hasOwnProperty", "constructor"];
	          // IE <= 8, Mozilla 1.0, and Netscape 6.2 ignore shadowed non-enumerable
	          // properties.
	          forEach = function (object, callback) {
	            var isFunction = getClass.call(object) == functionClass, property, length;
	            var hasProperty = !isFunction && typeof object.constructor != "function" && objectTypes[typeof object.hasOwnProperty] && object.hasOwnProperty || isProperty;
	            for (property in object) {
	              // Gecko <= 1.0 enumerates the `prototype` property of functions under
	              // certain conditions; IE does not.
	              if (!(isFunction && property == "prototype") && hasProperty.call(object, property)) {
	                callback(property);
	              }
	            }
	            // Manually invoke the callback for each non-enumerable property.
	            for (length = members.length; property = members[--length]; hasProperty.call(object, property) && callback(property));
	          };
	        } else if (size == 2) {
	          // Safari <= 2.0.4 enumerates shadowed properties twice.
	          forEach = function (object, callback) {
	            // Create a set of iterated properties.
	            var members = {}, isFunction = getClass.call(object) == functionClass, property;
	            for (property in object) {
	              // Store each property name to prevent double enumeration. The
	              // `prototype` property of functions is not enumerated due to cross-
	              // environment inconsistencies.
	              if (!(isFunction && property == "prototype") && !isProperty.call(members, property) && (members[property] = 1) && isProperty.call(object, property)) {
	                callback(property);
	              }
	            }
	          };
	        } else {
	          // No bugs detected; use the standard `for...in` algorithm.
	          forEach = function (object, callback) {
	            var isFunction = getClass.call(object) == functionClass, property, isConstructor;
	            for (property in object) {
	              if (!(isFunction && property == "prototype") && isProperty.call(object, property) && !(isConstructor = property === "constructor")) {
	                callback(property);
	              }
	            }
	            // Manually invoke the callback for the `constructor` property due to
	            // cross-environment inconsistencies.
	            if (isConstructor || isProperty.call(object, (property = "constructor"))) {
	              callback(property);
	            }
	          };
	        }
	        return forEach(object, callback);
	      };

	      // Public: Serializes a JavaScript `value` as a JSON string. The optional
	      // `filter` argument may specify either a function that alters how object and
	      // array members are serialized, or an array of strings and numbers that
	      // indicates which properties should be serialized. The optional `width`
	      // argument may be either a string or number that specifies the indentation
	      // level of the output.
	      if (!has("json-stringify")) {
	        // Internal: A map of control characters and their escaped equivalents.
	        var Escapes = {
	          92: "\\\\",
	          34: '\\"',
	          8: "\\b",
	          12: "\\f",
	          10: "\\n",
	          13: "\\r",
	          9: "\\t"
	        };

	        // Internal: Converts `value` into a zero-padded string such that its
	        // length is at least equal to `width`. The `width` must be <= 6.
	        var leadingZeroes = "000000";
	        var toPaddedString = function (width, value) {
	          // The `|| 0` expression is necessary to work around a bug in
	          // Opera <= 7.54u2 where `0 == -0`, but `String(-0) !== "0"`.
	          return (leadingZeroes + (value || 0)).slice(-width);
	        };

	        // Internal: Double-quotes a string `value`, replacing all ASCII control
	        // characters (characters with code unit values between 0 and 31) with
	        // their escaped equivalents. This is an implementation of the
	        // `Quote(value)` operation defined in ES 5.1 section 15.12.3.
	        var unicodePrefix = "\\u00";
	        var quote = function (value) {
	          var result = '"', index = 0, length = value.length, useCharIndex = !charIndexBuggy || length > 10;
	          var symbols = useCharIndex && (charIndexBuggy ? value.split("") : value);
	          for (; index < length; index++) {
	            var charCode = value.charCodeAt(index);
	            // If the character is a control character, append its Unicode or
	            // shorthand escape sequence; otherwise, append the character as-is.
	            switch (charCode) {
	              case 8: case 9: case 10: case 12: case 13: case 34: case 92:
	                result += Escapes[charCode];
	                break;
	              default:
	                if (charCode < 32) {
	                  result += unicodePrefix + toPaddedString(2, charCode.toString(16));
	                  break;
	                }
	                result += useCharIndex ? symbols[index] : value.charAt(index);
	            }
	          }
	          return result + '"';
	        };

	        // Internal: Recursively serializes an object. Implements the
	        // `Str(key, holder)`, `JO(value)`, and `JA(value)` operations.
	        var serialize = function (property, object, callback, properties, whitespace, indentation, stack) {
	          var value, className, year, month, date, time, hours, minutes, seconds, milliseconds, results, element, index, length, prefix, result;
	          try {
	            // Necessary for host object support.
	            value = object[property];
	          } catch (exception) {}
	          if (typeof value == "object" && value) {
	            className = getClass.call(value);
	            if (className == dateClass && !isProperty.call(value, "toJSON")) {
	              if (value > -1 / 0 && value < 1 / 0) {
	                // Dates are serialized according to the `Date#toJSON` method
	                // specified in ES 5.1 section 15.9.5.44. See section 15.9.1.15
	                // for the ISO 8601 date time string format.
	                if (getDay) {
	                  // Manually compute the year, month, date, hours, minutes,
	                  // seconds, and milliseconds if the `getUTC*` methods are
	                  // buggy. Adapted from @Yaffle's `date-shim` project.
	                  date = floor(value / 864e5);
	                  for (year = floor(date / 365.2425) + 1970 - 1; getDay(year + 1, 0) <= date; year++);
	                  for (month = floor((date - getDay(year, 0)) / 30.42); getDay(year, month + 1) <= date; month++);
	                  date = 1 + date - getDay(year, month);
	                  // The `time` value specifies the time within the day (see ES
	                  // 5.1 section 15.9.1.2). The formula `(A % B + B) % B` is used
	                  // to compute `A modulo B`, as the `%` operator does not
	                  // correspond to the `modulo` operation for negative numbers.
	                  time = (value % 864e5 + 864e5) % 864e5;
	                  // The hours, minutes, seconds, and milliseconds are obtained by
	                  // decomposing the time within the day. See section 15.9.1.10.
	                  hours = floor(time / 36e5) % 24;
	                  minutes = floor(time / 6e4) % 60;
	                  seconds = floor(time / 1e3) % 60;
	                  milliseconds = time % 1e3;
	                } else {
	                  year = value.getUTCFullYear();
	                  month = value.getUTCMonth();
	                  date = value.getUTCDate();
	                  hours = value.getUTCHours();
	                  minutes = value.getUTCMinutes();
	                  seconds = value.getUTCSeconds();
	                  milliseconds = value.getUTCMilliseconds();
	                }
	                // Serialize extended years correctly.
	                value = (year <= 0 || year >= 1e4 ? (year < 0 ? "-" : "+") + toPaddedString(6, year < 0 ? -year : year) : toPaddedString(4, year)) +
	                  "-" + toPaddedString(2, month + 1) + "-" + toPaddedString(2, date) +
	                  // Months, dates, hours, minutes, and seconds should have two
	                  // digits; milliseconds should have three.
	                  "T" + toPaddedString(2, hours) + ":" + toPaddedString(2, minutes) + ":" + toPaddedString(2, seconds) +
	                  // Milliseconds are optional in ES 5.0, but required in 5.1.
	                  "." + toPaddedString(3, milliseconds) + "Z";
	              } else {
	                value = null;
	              }
	            } else if (typeof value.toJSON == "function" && ((className != numberClass && className != stringClass && className != arrayClass) || isProperty.call(value, "toJSON"))) {
	              // Prototype <= 1.6.1 adds non-standard `toJSON` methods to the
	              // `Number`, `String`, `Date`, and `Array` prototypes. JSON 3
	              // ignores all `toJSON` methods on these objects unless they are
	              // defined directly on an instance.
	              value = value.toJSON(property);
	            }
	          }
	          if (callback) {
	            // If a replacement function was provided, call it to obtain the value
	            // for serialization.
	            value = callback.call(object, property, value);
	          }
	          if (value === null) {
	            return "null";
	          }
	          className = getClass.call(value);
	          if (className == booleanClass) {
	            // Booleans are represented literally.
	            return "" + value;
	          } else if (className == numberClass) {
	            // JSON numbers must be finite. `Infinity` and `NaN` are serialized as
	            // `"null"`.
	            return value > -1 / 0 && value < 1 / 0 ? "" + value : "null";
	          } else if (className == stringClass) {
	            // Strings are double-quoted and escaped.
	            return quote("" + value);
	          }
	          // Recursively serialize objects and arrays.
	          if (typeof value == "object") {
	            // Check for cyclic structures. This is a linear search; performance
	            // is inversely proportional to the number of unique nested objects.
	            for (length = stack.length; length--;) {
	              if (stack[length] === value) {
	                // Cyclic structures cannot be serialized by `JSON.stringify`.
	                throw TypeError();
	              }
	            }
	            // Add the object to the stack of traversed objects.
	            stack.push(value);
	            results = [];
	            // Save the current indentation level and indent one additional level.
	            prefix = indentation;
	            indentation += whitespace;
	            if (className == arrayClass) {
	              // Recursively serialize array elements.
	              for (index = 0, length = value.length; index < length; index++) {
	                element = serialize(index, value, callback, properties, whitespace, indentation, stack);
	                results.push(element === undef ? "null" : element);
	              }
	              result = results.length ? (whitespace ? "[\n" + indentation + results.join(",\n" + indentation) + "\n" + prefix + "]" : ("[" + results.join(",") + "]")) : "[]";
	            } else {
	              // Recursively serialize object members. Members are selected from
	              // either a user-specified list of property names, or the object
	              // itself.
	              forEach(properties || value, function (property) {
	                var element = serialize(property, value, callback, properties, whitespace, indentation, stack);
	                if (element !== undef) {
	                  // According to ES 5.1 section 15.12.3: "If `gap` {whitespace}
	                  // is not the empty string, let `member` {quote(property) + ":"}
	                  // be the concatenation of `member` and the `space` character."
	                  // The "`space` character" refers to the literal space
	                  // character, not the `space` {width} argument provided to
	                  // `JSON.stringify`.
	                  results.push(quote(property) + ":" + (whitespace ? " " : "") + element);
	                }
	              });
	              result = results.length ? (whitespace ? "{\n" + indentation + results.join(",\n" + indentation) + "\n" + prefix + "}" : ("{" + results.join(",") + "}")) : "{}";
	            }
	            // Remove the object from the traversed object stack.
	            stack.pop();
	            return result;
	          }
	        };

	        // Public: `JSON.stringify`. See ES 5.1 section 15.12.3.
	        exports.stringify = function (source, filter, width) {
	          var whitespace, callback, properties, className;
	          if (objectTypes[typeof filter] && filter) {
	            if ((className = getClass.call(filter)) == functionClass) {
	              callback = filter;
	            } else if (className == arrayClass) {
	              // Convert the property names array into a makeshift set.
	              properties = {};
	              for (var index = 0, length = filter.length, value; index < length; value = filter[index++], ((className = getClass.call(value)), className == stringClass || className == numberClass) && (properties[value] = 1));
	            }
	          }
	          if (width) {
	            if ((className = getClass.call(width)) == numberClass) {
	              // Convert the `width` to an integer and create a string containing
	              // `width` number of space characters.
	              if ((width -= width % 1) > 0) {
	                for (whitespace = "", width > 10 && (width = 10); whitespace.length < width; whitespace += " ");
	              }
	            } else if (className == stringClass) {
	              whitespace = width.length <= 10 ? width : width.slice(0, 10);
	            }
	          }
	          // Opera <= 7.54u2 discards the values associated with empty string keys
	          // (`""`) only if they are used directly within an object member list
	          // (e.g., `!("" in { "": 1})`).
	          return serialize("", (value = {}, value[""] = source, value), callback, properties, whitespace, "", []);
	        };
	      }

	      // Public: Parses a JSON source string.
	      if (!has("json-parse")) {
	        var fromCharCode = String.fromCharCode;

	        // Internal: A map of escaped control characters and their unescaped
	        // equivalents.
	        var Unescapes = {
	          92: "\\",
	          34: '"',
	          47: "/",
	          98: "\b",
	          116: "\t",
	          110: "\n",
	          102: "\f",
	          114: "\r"
	        };

	        // Internal: Stores the parser state.
	        var Index, Source;

	        // Internal: Resets the parser state and throws a `SyntaxError`.
	        var abort = function () {
	          Index = Source = null;
	          throw SyntaxError();
	        };

	        // Internal: Returns the next token, or `"$"` if the parser has reached
	        // the end of the source string. A token may be a string, number, `null`
	        // literal, or Boolean literal.
	        var lex = function () {
	          var source = Source, length = source.length, value, begin, position, isSigned, charCode;
	          while (Index < length) {
	            charCode = source.charCodeAt(Index);
	            switch (charCode) {
	              case 9: case 10: case 13: case 32:
	                // Skip whitespace tokens, including tabs, carriage returns, line
	                // feeds, and space characters.
	                Index++;
	                break;
	              case 123: case 125: case 91: case 93: case 58: case 44:
	                // Parse a punctuator token (`{`, `}`, `[`, `]`, `:`, or `,`) at
	                // the current position.
	                value = charIndexBuggy ? source.charAt(Index) : source[Index];
	                Index++;
	                return value;
	              case 34:
	                // `"` delimits a JSON string; advance to the next character and
	                // begin parsing the string. String tokens are prefixed with the
	                // sentinel `@` character to distinguish them from punctuators and
	                // end-of-string tokens.
	                for (value = "@", Index++; Index < length;) {
	                  charCode = source.charCodeAt(Index);
	                  if (charCode < 32) {
	                    // Unescaped ASCII control characters (those with a code unit
	                    // less than the space character) are not permitted.
	                    abort();
	                  } else if (charCode == 92) {
	                    // A reverse solidus (`\`) marks the beginning of an escaped
	                    // control character (including `"`, `\`, and `/`) or Unicode
	                    // escape sequence.
	                    charCode = source.charCodeAt(++Index);
	                    switch (charCode) {
	                      case 92: case 34: case 47: case 98: case 116: case 110: case 102: case 114:
	                        // Revive escaped control characters.
	                        value += Unescapes[charCode];
	                        Index++;
	                        break;
	                      case 117:
	                        // `\u` marks the beginning of a Unicode escape sequence.
	                        // Advance to the first character and validate the
	                        // four-digit code point.
	                        begin = ++Index;
	                        for (position = Index + 4; Index < position; Index++) {
	                          charCode = source.charCodeAt(Index);
	                          // A valid sequence comprises four hexdigits (case-
	                          // insensitive) that form a single hexadecimal value.
	                          if (!(charCode >= 48 && charCode <= 57 || charCode >= 97 && charCode <= 102 || charCode >= 65 && charCode <= 70)) {
	                            // Invalid Unicode escape sequence.
	                            abort();
	                          }
	                        }
	                        // Revive the escaped character.
	                        value += fromCharCode("0x" + source.slice(begin, Index));
	                        break;
	                      default:
	                        // Invalid escape sequence.
	                        abort();
	                    }
	                  } else {
	                    if (charCode == 34) {
	                      // An unescaped double-quote character marks the end of the
	                      // string.
	                      break;
	                    }
	                    charCode = source.charCodeAt(Index);
	                    begin = Index;
	                    // Optimize for the common case where a string is valid.
	                    while (charCode >= 32 && charCode != 92 && charCode != 34) {
	                      charCode = source.charCodeAt(++Index);
	                    }
	                    // Append the string as-is.
	                    value += source.slice(begin, Index);
	                  }
	                }
	                if (source.charCodeAt(Index) == 34) {
	                  // Advance to the next character and return the revived string.
	                  Index++;
	                  return value;
	                }
	                // Unterminated string.
	                abort();
	              default:
	                // Parse numbers and literals.
	                begin = Index;
	                // Advance past the negative sign, if one is specified.
	                if (charCode == 45) {
	                  isSigned = true;
	                  charCode = source.charCodeAt(++Index);
	                }
	                // Parse an integer or floating-point value.
	                if (charCode >= 48 && charCode <= 57) {
	                  // Leading zeroes are interpreted as octal literals.
	                  if (charCode == 48 && ((charCode = source.charCodeAt(Index + 1)), charCode >= 48 && charCode <= 57)) {
	                    // Illegal octal literal.
	                    abort();
	                  }
	                  isSigned = false;
	                  // Parse the integer component.
	                  for (; Index < length && ((charCode = source.charCodeAt(Index)), charCode >= 48 && charCode <= 57); Index++);
	                  // Floats cannot contain a leading decimal point; however, this
	                  // case is already accounted for by the parser.
	                  if (source.charCodeAt(Index) == 46) {
	                    position = ++Index;
	                    // Parse the decimal component.
	                    for (; position < length && ((charCode = source.charCodeAt(position)), charCode >= 48 && charCode <= 57); position++);
	                    if (position == Index) {
	                      // Illegal trailing decimal.
	                      abort();
	                    }
	                    Index = position;
	                  }
	                  // Parse exponents. The `e` denoting the exponent is
	                  // case-insensitive.
	                  charCode = source.charCodeAt(Index);
	                  if (charCode == 101 || charCode == 69) {
	                    charCode = source.charCodeAt(++Index);
	                    // Skip past the sign following the exponent, if one is
	                    // specified.
	                    if (charCode == 43 || charCode == 45) {
	                      Index++;
	                    }
	                    // Parse the exponential component.
	                    for (position = Index; position < length && ((charCode = source.charCodeAt(position)), charCode >= 48 && charCode <= 57); position++);
	                    if (position == Index) {
	                      // Illegal empty exponent.
	                      abort();
	                    }
	                    Index = position;
	                  }
	                  // Coerce the parsed value to a JavaScript number.
	                  return +source.slice(begin, Index);
	                }
	                // A negative sign may only precede numbers.
	                if (isSigned) {
	                  abort();
	                }
	                // `true`, `false`, and `null` literals.
	                if (source.slice(Index, Index + 4) == "true") {
	                  Index += 4;
	                  return true;
	                } else if (source.slice(Index, Index + 5) == "false") {
	                  Index += 5;
	                  return false;
	                } else if (source.slice(Index, Index + 4) == "null") {
	                  Index += 4;
	                  return null;
	                }
	                // Unrecognized token.
	                abort();
	            }
	          }
	          // Return the sentinel `$` character if the parser has reached the end
	          // of the source string.
	          return "$";
	        };

	        // Internal: Parses a JSON `value` token.
	        var get = function (value) {
	          var results, hasMembers;
	          if (value == "$") {
	            // Unexpected end of input.
	            abort();
	          }
	          if (typeof value == "string") {
	            if ((charIndexBuggy ? value.charAt(0) : value[0]) == "@") {
	              // Remove the sentinel `@` character.
	              return value.slice(1);
	            }
	            // Parse object and array literals.
	            if (value == "[") {
	              // Parses a JSON array, returning a new JavaScript array.
	              results = [];
	              for (;; hasMembers || (hasMembers = true)) {
	                value = lex();
	                // A closing square bracket marks the end of the array literal.
	                if (value == "]") {
	                  break;
	                }
	                // If the array literal contains elements, the current token
	                // should be a comma separating the previous element from the
	                // next.
	                if (hasMembers) {
	                  if (value == ",") {
	                    value = lex();
	                    if (value == "]") {
	                      // Unexpected trailing `,` in array literal.
	                      abort();
	                    }
	                  } else {
	                    // A `,` must separate each array element.
	                    abort();
	                  }
	                }
	                // Elisions and leading commas are not permitted.
	                if (value == ",") {
	                  abort();
	                }
	                results.push(get(value));
	              }
	              return results;
	            } else if (value == "{") {
	              // Parses a JSON object, returning a new JavaScript object.
	              results = {};
	              for (;; hasMembers || (hasMembers = true)) {
	                value = lex();
	                // A closing curly brace marks the end of the object literal.
	                if (value == "}") {
	                  break;
	                }
	                // If the object literal contains members, the current token
	                // should be a comma separator.
	                if (hasMembers) {
	                  if (value == ",") {
	                    value = lex();
	                    if (value == "}") {
	                      // Unexpected trailing `,` in object literal.
	                      abort();
	                    }
	                  } else {
	                    // A `,` must separate each object member.
	                    abort();
	                  }
	                }
	                // Leading commas are not permitted, object property names must be
	                // double-quoted strings, and a `:` must separate each property
	                // name and value.
	                if (value == "," || typeof value != "string" || (charIndexBuggy ? value.charAt(0) : value[0]) != "@" || lex() != ":") {
	                  abort();
	                }
	                results[value.slice(1)] = get(lex());
	              }
	              return results;
	            }
	            // Unexpected token encountered.
	            abort();
	          }
	          return value;
	        };

	        // Internal: Updates a traversed object member.
	        var update = function (source, property, callback) {
	          var element = walk(source, property, callback);
	          if (element === undef) {
	            delete source[property];
	          } else {
	            source[property] = element;
	          }
	        };

	        // Internal: Recursively traverses a parsed JSON object, invoking the
	        // `callback` function for each value. This is an implementation of the
	        // `Walk(holder, name)` operation defined in ES 5.1 section 15.12.2.
	        var walk = function (source, property, callback) {
	          var value = source[property], length;
	          if (typeof value == "object" && value) {
	            // `forEach` can't be used to traverse an array in Opera <= 8.54
	            // because its `Object#hasOwnProperty` implementation returns `false`
	            // for array indices (e.g., `![1, 2, 3].hasOwnProperty("0")`).
	            if (getClass.call(value) == arrayClass) {
	              for (length = value.length; length--;) {
	                update(value, length, callback);
	              }
	            } else {
	              forEach(value, function (property) {
	                update(value, property, callback);
	              });
	            }
	          }
	          return callback.call(source, property, value);
	        };

	        // Public: `JSON.parse`. See ES 5.1 section 15.12.2.
	        exports.parse = function (source, callback) {
	          var result, value;
	          Index = 0;
	          Source = "" + source;
	          result = get(lex());
	          // If a JSON string contains multiple tokens, it is invalid.
	          if (lex() != "$") {
	            abort();
	          }
	          // Reset the parser state.
	          Index = Source = null;
	          return callback && getClass.call(callback) == functionClass ? walk((value = {}, value[""] = result, value), "", callback) : result;
	        };
	      }
	    }

	    exports["runInContext"] = runInContext;
	    return exports;
	  }

	  if (freeExports && !isLoader) {
	    // Export for CommonJS environments.
	    runInContext(root, freeExports);
	  } else {
	    // Export for web browsers and JavaScript engines.
	    var nativeJSON = root.JSON,
	        previousJSON = root["JSON3"],
	        isRestored = false;

	    var JSON3 = runInContext(root, (root["JSON3"] = {
	      // Public: Restores the original value of the global `JSON` object and
	      // returns a reference to the `JSON3` object.
	      "noConflict": function () {
	        if (!isRestored) {
	          isRestored = true;
	          root.JSON = nativeJSON;
	          root["JSON3"] = previousJSON;
	          nativeJSON = previousJSON = null;
	        }
	        return JSON3;
	      }
	    }));

	    root.JSON = {
	      "parse": JSON3.parse,
	      "stringify": JSON3.stringify
	    };
	  }

	  // Export for asynchronous module loaders.
	  if (isLoader) {
	    define(function () {
	      return JSON3;
	    });
	  }
	}).call(this);

	}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{}],70:[function(require,module,exports){
	/**
	 * lodash 3.2.0 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */
	var baseCopy = require('lodash._basecopy'),
	    keys = require('lodash.keys');

	/**
	 * The base implementation of `_.assign` without support for argument juggling,
	 * multiple sources, and `customizer` functions.
	 *
	 * @private
	 * @param {Object} object The destination object.
	 * @param {Object} source The source object.
	 * @returns {Object} Returns `object`.
	 */
	function baseAssign(object, source) {
	  return source == null
	    ? object
	    : baseCopy(source, keys(source), object);
	}

	module.exports = baseAssign;

	},{"lodash._basecopy":71,"lodash.keys":78}],71:[function(require,module,exports){
	/**
	 * lodash 3.0.1 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */

	/**
	 * Copies properties of `source` to `object`.
	 *
	 * @private
	 * @param {Object} source The object to copy properties from.
	 * @param {Array} props The property names to copy.
	 * @param {Object} [object={}] The object to copy properties to.
	 * @returns {Object} Returns `object`.
	 */
	function baseCopy(source, props, object) {
	  object || (object = {});

	  var index = -1,
	      length = props.length;

	  while (++index < length) {
	    var key = props[index];
	    object[key] = source[key];
	  }
	  return object;
	}

	module.exports = baseCopy;

	},{}],72:[function(require,module,exports){
	/**
	 * lodash 3.0.3 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */

	/**
	 * The base implementation of `_.create` without support for assigning
	 * properties to the created object.
	 *
	 * @private
	 * @param {Object} prototype The object to inherit from.
	 * @returns {Object} Returns the new object.
	 */
	var baseCreate = (function() {
	  function object() {}
	  return function(prototype) {
	    if (isObject(prototype)) {
	      object.prototype = prototype;
	      var result = new object;
	      object.prototype = undefined;
	    }
	    return result || {};
	  };
	}());

	/**
	 * Checks if `value` is the [language type](https://es5.github.io/#x8) of `Object`.
	 * (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(1);
	 * // => false
	 */
	function isObject(value) {
	  // Avoid a V8 JIT bug in Chrome 19-20.
	  // See https://code.google.com/p/v8/issues/detail?id=2291 for more details.
	  var type = typeof value;
	  return !!value && (type == 'object' || type == 'function');
	}

	module.exports = baseCreate;

	},{}],73:[function(require,module,exports){
	/**
	 * lodash 3.9.1 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */

	/** `Object#toString` result references. */
	var funcTag = '[object Function]';

	/** Used to detect host constructors (Safari > 5). */
	var reIsHostCtor = /^\[object .+?Constructor\]$/;

	/**
	 * Checks if `value` is object-like.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is object-like, else `false`.
	 */
	function isObjectLike(value) {
	  return !!value && typeof value == 'object';
	}

	/** Used for native method references. */
	var objectProto = Object.prototype;

	/** Used to resolve the decompiled source of functions. */
	var fnToString = Function.prototype.toString;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Used to resolve the [`toStringTag`](http://ecma-international.org/ecma-262/6.0/#sec-object.prototype.tostring)
	 * of values.
	 */
	var objToString = objectProto.toString;

	/** Used to detect if a method is native. */
	var reIsNative = RegExp('^' +
	  fnToString.call(hasOwnProperty).replace(/[\\^$.*+?()[\]{}|]/g, '\\$&')
	  .replace(/hasOwnProperty|(function).*?(?=\\\()| for .+?(?=\\\])/g, '$1.*?') + '$'
	);

	/**
	 * Gets the native function at `key` of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {string} key The key of the method to get.
	 * @returns {*} Returns the function if it's native, else `undefined`.
	 */
	function getNative(object, key) {
	  var value = object == null ? undefined : object[key];
	  return isNative(value) ? value : undefined;
	}

	/**
	 * Checks if `value` is classified as a `Function` object.
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is correctly classified, else `false`.
	 * @example
	 *
	 * _.isFunction(_);
	 * // => true
	 *
	 * _.isFunction(/abc/);
	 * // => false
	 */
	function isFunction(value) {
	  // The use of `Object#toString` avoids issues with the `typeof` operator
	  // in older versions of Chrome and Safari which return 'function' for regexes
	  // and Safari 8 equivalents which return 'object' for typed array constructors.
	  return isObject(value) && objToString.call(value) == funcTag;
	}

	/**
	 * Checks if `value` is the [language type](https://es5.github.io/#x8) of `Object`.
	 * (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(1);
	 * // => false
	 */
	function isObject(value) {
	  // Avoid a V8 JIT bug in Chrome 19-20.
	  // See https://code.google.com/p/v8/issues/detail?id=2291 for more details.
	  var type = typeof value;
	  return !!value && (type == 'object' || type == 'function');
	}

	/**
	 * Checks if `value` is a native function.
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a native function, else `false`.
	 * @example
	 *
	 * _.isNative(Array.prototype.push);
	 * // => true
	 *
	 * _.isNative(_);
	 * // => false
	 */
	function isNative(value) {
	  if (value == null) {
	    return false;
	  }
	  if (isFunction(value)) {
	    return reIsNative.test(fnToString.call(value));
	  }
	  return isObjectLike(value) && reIsHostCtor.test(value);
	}

	module.exports = getNative;

	},{}],74:[function(require,module,exports){
	/**
	 * lodash 3.0.9 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */

	/** Used to detect unsigned integer values. */
	var reIsUint = /^\d+$/;

	/**
	 * Used as the [maximum length](https://people.mozilla.org/~jorendorff/es6-draft.html#sec-number.max_safe_integer)
	 * of an array-like value.
	 */
	var MAX_SAFE_INTEGER = 9007199254740991;

	/**
	 * The base implementation of `_.property` without support for deep paths.
	 *
	 * @private
	 * @param {string} key The key of the property to get.
	 * @returns {Function} Returns the new function.
	 */
	function baseProperty(key) {
	  return function(object) {
	    return object == null ? undefined : object[key];
	  };
	}

	/**
	 * Gets the "length" property value of `object`.
	 *
	 * **Note:** This function is used to avoid a [JIT bug](https://bugs.webkit.org/show_bug.cgi?id=142792)
	 * that affects Safari on at least iOS 8.1-8.3 ARM64.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @returns {*} Returns the "length" value.
	 */
	var getLength = baseProperty('length');

	/**
	 * Checks if `value` is array-like.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is array-like, else `false`.
	 */
	function isArrayLike(value) {
	  return value != null && isLength(getLength(value));
	}

	/**
	 * Checks if `value` is a valid array-like index.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @param {number} [length=MAX_SAFE_INTEGER] The upper bounds of a valid index.
	 * @returns {boolean} Returns `true` if `value` is a valid index, else `false`.
	 */
	function isIndex(value, length) {
	  value = (typeof value == 'number' || reIsUint.test(value)) ? +value : -1;
	  length = length == null ? MAX_SAFE_INTEGER : length;
	  return value > -1 && value % 1 == 0 && value < length;
	}

	/**
	 * Checks if the provided arguments are from an iteratee call.
	 *
	 * @private
	 * @param {*} value The potential iteratee value argument.
	 * @param {*} index The potential iteratee index or key argument.
	 * @param {*} object The potential iteratee object argument.
	 * @returns {boolean} Returns `true` if the arguments are from an iteratee call, else `false`.
	 */
	function isIterateeCall(value, index, object) {
	  if (!isObject(object)) {
	    return false;
	  }
	  var type = typeof index;
	  if (type == 'number'
	      ? (isArrayLike(object) && isIndex(index, object.length))
	      : (type == 'string' && index in object)) {
	    var other = object[index];
	    return value === value ? (value === other) : (other !== other);
	  }
	  return false;
	}

	/**
	 * Checks if `value` is a valid array-like length.
	 *
	 * **Note:** This function is based on [`ToLength`](https://people.mozilla.org/~jorendorff/es6-draft.html#sec-tolength).
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a valid length, else `false`.
	 */
	function isLength(value) {
	  return typeof value == 'number' && value > -1 && value % 1 == 0 && value <= MAX_SAFE_INTEGER;
	}

	/**
	 * Checks if `value` is the [language type](https://es5.github.io/#x8) of `Object`.
	 * (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(1);
	 * // => false
	 */
	function isObject(value) {
	  // Avoid a V8 JIT bug in Chrome 19-20.
	  // See https://code.google.com/p/v8/issues/detail?id=2291 for more details.
	  var type = typeof value;
	  return !!value && (type == 'object' || type == 'function');
	}

	module.exports = isIterateeCall;

	},{}],75:[function(require,module,exports){
	/**
	 * lodash 3.1.1 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */
	var baseAssign = require('lodash._baseassign'),
	    baseCreate = require('lodash._basecreate'),
	    isIterateeCall = require('lodash._isiterateecall');

	/**
	 * Creates an object that inherits from the given `prototype` object. If a
	 * `properties` object is provided its own enumerable properties are assigned
	 * to the created object.
	 *
	 * @static
	 * @memberOf _
	 * @category Object
	 * @param {Object} prototype The object to inherit from.
	 * @param {Object} [properties] The properties to assign to the object.
	 * @param- {Object} [guard] Enables use as a callback for functions like `_.map`.
	 * @returns {Object} Returns the new object.
	 * @example
	 *
	 * function Shape() {
	 *   this.x = 0;
	 *   this.y = 0;
	 * }
	 *
	 * function Circle() {
	 *   Shape.call(this);
	 * }
	 *
	 * Circle.prototype = _.create(Shape.prototype, {
	 *   'constructor': Circle
	 * });
	 *
	 * var circle = new Circle;
	 * circle instanceof Circle;
	 * // => true
	 *
	 * circle instanceof Shape;
	 * // => true
	 */
	function create(prototype, properties, guard) {
	  var result = baseCreate(prototype);
	  if (guard && isIterateeCall(prototype, properties, guard)) {
	    properties = undefined;
	  }
	  return properties ? baseAssign(result, properties) : result;
	}

	module.exports = create;

	},{"lodash._baseassign":70,"lodash._basecreate":72,"lodash._isiterateecall":74}],76:[function(require,module,exports){
	/**
	 * lodash (Custom Build) <https://lodash.com/>
	 * Build: `lodash modularize exports="npm" -o ./`
	 * Copyright jQuery Foundation and other contributors <https://jquery.org/>
	 * Released under MIT license <https://lodash.com/license>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 */

	/** Used as references for various `Number` constants. */
	var MAX_SAFE_INTEGER = 9007199254740991;

	/** `Object#toString` result references. */
	var argsTag = '[object Arguments]',
	    funcTag = '[object Function]',
	    genTag = '[object GeneratorFunction]';

	/** Used for built-in method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Used to resolve the
	 * [`toStringTag`](http://ecma-international.org/ecma-262/7.0/#sec-object.prototype.tostring)
	 * of values.
	 */
	var objectToString = objectProto.toString;

	/** Built-in value references. */
	var propertyIsEnumerable = objectProto.propertyIsEnumerable;

	/**
	 * Checks if `value` is likely an `arguments` object.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an `arguments` object,
	 *  else `false`.
	 * @example
	 *
	 * _.isArguments(function() { return arguments; }());
	 * // => true
	 *
	 * _.isArguments([1, 2, 3]);
	 * // => false
	 */
	function isArguments(value) {
	  // Safari 8.1 makes `arguments.callee` enumerable in strict mode.
	  return isArrayLikeObject(value) && hasOwnProperty.call(value, 'callee') &&
	    (!propertyIsEnumerable.call(value, 'callee') || objectToString.call(value) == argsTag);
	}

	/**
	 * Checks if `value` is array-like. A value is considered array-like if it's
	 * not a function and has a `value.length` that's an integer greater than or
	 * equal to `0` and less than or equal to `Number.MAX_SAFE_INTEGER`.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is array-like, else `false`.
	 * @example
	 *
	 * _.isArrayLike([1, 2, 3]);
	 * // => true
	 *
	 * _.isArrayLike(document.body.children);
	 * // => true
	 *
	 * _.isArrayLike('abc');
	 * // => true
	 *
	 * _.isArrayLike(_.noop);
	 * // => false
	 */
	function isArrayLike(value) {
	  return value != null && isLength(value.length) && !isFunction(value);
	}

	/**
	 * This method is like `_.isArrayLike` except that it also checks if `value`
	 * is an object.
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an array-like object,
	 *  else `false`.
	 * @example
	 *
	 * _.isArrayLikeObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isArrayLikeObject(document.body.children);
	 * // => true
	 *
	 * _.isArrayLikeObject('abc');
	 * // => false
	 *
	 * _.isArrayLikeObject(_.noop);
	 * // => false
	 */
	function isArrayLikeObject(value) {
	  return isObjectLike(value) && isArrayLike(value);
	}

	/**
	 * Checks if `value` is classified as a `Function` object.
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a function, else `false`.
	 * @example
	 *
	 * _.isFunction(_);
	 * // => true
	 *
	 * _.isFunction(/abc/);
	 * // => false
	 */
	function isFunction(value) {
	  // The use of `Object#toString` avoids issues with the `typeof` operator
	  // in Safari 8-9 which returns 'object' for typed array and other constructors.
	  var tag = isObject(value) ? objectToString.call(value) : '';
	  return tag == funcTag || tag == genTag;
	}

	/**
	 * Checks if `value` is a valid array-like length.
	 *
	 * **Note:** This method is loosely based on
	 * [`ToLength`](http://ecma-international.org/ecma-262/7.0/#sec-tolength).
	 *
	 * @static
	 * @memberOf _
	 * @since 4.0.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a valid length, else `false`.
	 * @example
	 *
	 * _.isLength(3);
	 * // => true
	 *
	 * _.isLength(Number.MIN_VALUE);
	 * // => false
	 *
	 * _.isLength(Infinity);
	 * // => false
	 *
	 * _.isLength('3');
	 * // => false
	 */
	function isLength(value) {
	  return typeof value == 'number' &&
	    value > -1 && value % 1 == 0 && value <= MAX_SAFE_INTEGER;
	}

	/**
	 * Checks if `value` is the
	 * [language type](http://www.ecma-international.org/ecma-262/7.0/#sec-ecmascript-language-types)
	 * of `Object`. (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @since 0.1.0
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(_.noop);
	 * // => true
	 *
	 * _.isObject(null);
	 * // => false
	 */
	function isObject(value) {
	  var type = typeof value;
	  return !!value && (type == 'object' || type == 'function');
	}

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
	  return !!value && typeof value == 'object';
	}

	module.exports = isArguments;

	},{}],77:[function(require,module,exports){
	/**
	 * lodash 3.0.4 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */

	/** `Object#toString` result references. */
	var arrayTag = '[object Array]',
	    funcTag = '[object Function]';

	/** Used to detect host constructors (Safari > 5). */
	var reIsHostCtor = /^\[object .+?Constructor\]$/;

	/**
	 * Checks if `value` is object-like.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is object-like, else `false`.
	 */
	function isObjectLike(value) {
	  return !!value && typeof value == 'object';
	}

	/** Used for native method references. */
	var objectProto = Object.prototype;

	/** Used to resolve the decompiled source of functions. */
	var fnToString = Function.prototype.toString;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/**
	 * Used to resolve the [`toStringTag`](http://ecma-international.org/ecma-262/6.0/#sec-object.prototype.tostring)
	 * of values.
	 */
	var objToString = objectProto.toString;

	/** Used to detect if a method is native. */
	var reIsNative = RegExp('^' +
	  fnToString.call(hasOwnProperty).replace(/[\\^$.*+?()[\]{}|]/g, '\\$&')
	  .replace(/hasOwnProperty|(function).*?(?=\\\()| for .+?(?=\\\])/g, '$1.*?') + '$'
	);

	/* Native method references for those with the same name as other `lodash` methods. */
	var nativeIsArray = getNative(Array, 'isArray');

	/**
	 * Used as the [maximum length](http://ecma-international.org/ecma-262/6.0/#sec-number.max_safe_integer)
	 * of an array-like value.
	 */
	var MAX_SAFE_INTEGER = 9007199254740991;

	/**
	 * Gets the native function at `key` of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @param {string} key The key of the method to get.
	 * @returns {*} Returns the function if it's native, else `undefined`.
	 */
	function getNative(object, key) {
	  var value = object == null ? undefined : object[key];
	  return isNative(value) ? value : undefined;
	}

	/**
	 * Checks if `value` is a valid array-like length.
	 *
	 * **Note:** This function is based on [`ToLength`](http://ecma-international.org/ecma-262/6.0/#sec-tolength).
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a valid length, else `false`.
	 */
	function isLength(value) {
	  return typeof value == 'number' && value > -1 && value % 1 == 0 && value <= MAX_SAFE_INTEGER;
	}

	/**
	 * Checks if `value` is classified as an `Array` object.
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is correctly classified, else `false`.
	 * @example
	 *
	 * _.isArray([1, 2, 3]);
	 * // => true
	 *
	 * _.isArray(function() { return arguments; }());
	 * // => false
	 */
	var isArray = nativeIsArray || function(value) {
	  return isObjectLike(value) && isLength(value.length) && objToString.call(value) == arrayTag;
	};

	/**
	 * Checks if `value` is classified as a `Function` object.
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is correctly classified, else `false`.
	 * @example
	 *
	 * _.isFunction(_);
	 * // => true
	 *
	 * _.isFunction(/abc/);
	 * // => false
	 */
	function isFunction(value) {
	  // The use of `Object#toString` avoids issues with the `typeof` operator
	  // in older versions of Chrome and Safari which return 'function' for regexes
	  // and Safari 8 equivalents which return 'object' for typed array constructors.
	  return isObject(value) && objToString.call(value) == funcTag;
	}

	/**
	 * Checks if `value` is the [language type](https://es5.github.io/#x8) of `Object`.
	 * (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(1);
	 * // => false
	 */
	function isObject(value) {
	  // Avoid a V8 JIT bug in Chrome 19-20.
	  // See https://code.google.com/p/v8/issues/detail?id=2291 for more details.
	  var type = typeof value;
	  return !!value && (type == 'object' || type == 'function');
	}

	/**
	 * Checks if `value` is a native function.
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a native function, else `false`.
	 * @example
	 *
	 * _.isNative(Array.prototype.push);
	 * // => true
	 *
	 * _.isNative(_);
	 * // => false
	 */
	function isNative(value) {
	  if (value == null) {
	    return false;
	  }
	  if (isFunction(value)) {
	    return reIsNative.test(fnToString.call(value));
	  }
	  return isObjectLike(value) && reIsHostCtor.test(value);
	}

	module.exports = isArray;

	},{}],78:[function(require,module,exports){
	/**
	 * lodash 3.1.2 (Custom Build) <https://lodash.com/>
	 * Build: `lodash modern modularize exports="npm" -o ./`
	 * Copyright 2012-2015 The Dojo Foundation <http://dojofoundation.org/>
	 * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
	 * Copyright 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
	 * Available under MIT license <https://lodash.com/license>
	 */
	var getNative = require('lodash._getnative'),
	    isArguments = require('lodash.isarguments'),
	    isArray = require('lodash.isarray');

	/** Used to detect unsigned integer values. */
	var reIsUint = /^\d+$/;

	/** Used for native method references. */
	var objectProto = Object.prototype;

	/** Used to check objects for own properties. */
	var hasOwnProperty = objectProto.hasOwnProperty;

	/* Native method references for those with the same name as other `lodash` methods. */
	var nativeKeys = getNative(Object, 'keys');

	/**
	 * Used as the [maximum length](http://ecma-international.org/ecma-262/6.0/#sec-number.max_safe_integer)
	 * of an array-like value.
	 */
	var MAX_SAFE_INTEGER = 9007199254740991;

	/**
	 * The base implementation of `_.property` without support for deep paths.
	 *
	 * @private
	 * @param {string} key The key of the property to get.
	 * @returns {Function} Returns the new function.
	 */
	function baseProperty(key) {
	  return function(object) {
	    return object == null ? undefined : object[key];
	  };
	}

	/**
	 * Gets the "length" property value of `object`.
	 *
	 * **Note:** This function is used to avoid a [JIT bug](https://bugs.webkit.org/show_bug.cgi?id=142792)
	 * that affects Safari on at least iOS 8.1-8.3 ARM64.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @returns {*} Returns the "length" value.
	 */
	var getLength = baseProperty('length');

	/**
	 * Checks if `value` is array-like.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is array-like, else `false`.
	 */
	function isArrayLike(value) {
	  return value != null && isLength(getLength(value));
	}

	/**
	 * Checks if `value` is a valid array-like index.
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @param {number} [length=MAX_SAFE_INTEGER] The upper bounds of a valid index.
	 * @returns {boolean} Returns `true` if `value` is a valid index, else `false`.
	 */
	function isIndex(value, length) {
	  value = (typeof value == 'number' || reIsUint.test(value)) ? +value : -1;
	  length = length == null ? MAX_SAFE_INTEGER : length;
	  return value > -1 && value % 1 == 0 && value < length;
	}

	/**
	 * Checks if `value` is a valid array-like length.
	 *
	 * **Note:** This function is based on [`ToLength`](http://ecma-international.org/ecma-262/6.0/#sec-tolength).
	 *
	 * @private
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is a valid length, else `false`.
	 */
	function isLength(value) {
	  return typeof value == 'number' && value > -1 && value % 1 == 0 && value <= MAX_SAFE_INTEGER;
	}

	/**
	 * A fallback implementation of `Object.keys` which creates an array of the
	 * own enumerable property names of `object`.
	 *
	 * @private
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the array of property names.
	 */
	function shimKeys(object) {
	  var props = keysIn(object),
	      propsLength = props.length,
	      length = propsLength && object.length;

	  var allowIndexes = !!length && isLength(length) &&
	    (isArray(object) || isArguments(object));

	  var index = -1,
	      result = [];

	  while (++index < propsLength) {
	    var key = props[index];
	    if ((allowIndexes && isIndex(key, length)) || hasOwnProperty.call(object, key)) {
	      result.push(key);
	    }
	  }
	  return result;
	}

	/**
	 * Checks if `value` is the [language type](https://es5.github.io/#x8) of `Object`.
	 * (e.g. arrays, functions, objects, regexes, `new Number(0)`, and `new String('')`)
	 *
	 * @static
	 * @memberOf _
	 * @category Lang
	 * @param {*} value The value to check.
	 * @returns {boolean} Returns `true` if `value` is an object, else `false`.
	 * @example
	 *
	 * _.isObject({});
	 * // => true
	 *
	 * _.isObject([1, 2, 3]);
	 * // => true
	 *
	 * _.isObject(1);
	 * // => false
	 */
	function isObject(value) {
	  // Avoid a V8 JIT bug in Chrome 19-20.
	  // See https://code.google.com/p/v8/issues/detail?id=2291 for more details.
	  var type = typeof value;
	  return !!value && (type == 'object' || type == 'function');
	}

	/**
	 * Creates an array of the own enumerable property names of `object`.
	 *
	 * **Note:** Non-object values are coerced to objects. See the
	 * [ES spec](http://ecma-international.org/ecma-262/6.0/#sec-object.keys)
	 * for more details.
	 *
	 * @static
	 * @memberOf _
	 * @category Object
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the array of property names.
	 * @example
	 *
	 * function Foo() {
	 *   this.a = 1;
	 *   this.b = 2;
	 * }
	 *
	 * Foo.prototype.c = 3;
	 *
	 * _.keys(new Foo);
	 * // => ['a', 'b'] (iteration order is not guaranteed)
	 *
	 * _.keys('hi');
	 * // => ['0', '1']
	 */
	var keys = !nativeKeys ? shimKeys : function(object) {
	  var Ctor = object == null ? undefined : object.constructor;
	  if ((typeof Ctor == 'function' && Ctor.prototype === object) ||
	      (typeof object != 'function' && isArrayLike(object))) {
	    return shimKeys(object);
	  }
	  return isObject(object) ? nativeKeys(object) : [];
	};

	/**
	 * Creates an array of the own and inherited enumerable property names of `object`.
	 *
	 * **Note:** Non-object values are coerced to objects.
	 *
	 * @static
	 * @memberOf _
	 * @category Object
	 * @param {Object} object The object to query.
	 * @returns {Array} Returns the array of property names.
	 * @example
	 *
	 * function Foo() {
	 *   this.a = 1;
	 *   this.b = 2;
	 * }
	 *
	 * Foo.prototype.c = 3;
	 *
	 * _.keysIn(new Foo);
	 * // => ['a', 'b', 'c'] (iteration order is not guaranteed)
	 */
	function keysIn(object) {
	  if (object == null) {
	    return [];
	  }
	  if (!isObject(object)) {
	    object = Object(object);
	  }
	  var length = object.length;
	  length = (length && isLength(length) &&
	    (isArray(object) || isArguments(object)) && length) || 0;

	  var Ctor = object.constructor,
	      index = -1,
	      isProto = typeof Ctor == 'function' && Ctor.prototype === object,
	      result = Array(length),
	      skipIndexes = length > 0;

	  while (++index < length) {
	    result[index] = (index + '');
	  }
	  for (var key in object) {
	    if (!(skipIndexes && isIndex(key, length)) &&
	        !(key == 'constructor' && (isProto || !hasOwnProperty.call(object, key)))) {
	      result.push(key);
	    }
	  }
	  return result;
	}

	module.exports = keys;

	},{"lodash._getnative":73,"lodash.isarguments":76,"lodash.isarray":77}],79:[function(require,module,exports){
	(function (process){
	var path = require('path');
	var fs = require('fs');
	var _0777 = parseInt('0777', 8);

	module.exports = mkdirP.mkdirp = mkdirP.mkdirP = mkdirP;

	function mkdirP (p, opts, f, made) {
	    if (typeof opts === 'function') {
	        f = opts;
	        opts = {};
	    }
	    else if (!opts || typeof opts !== 'object') {
	        opts = { mode: opts };
	    }
	    
	    var mode = opts.mode;
	    var xfs = opts.fs || fs;
	    
	    if (mode === undefined) {
	        mode = _0777 & (~process.umask());
	    }
	    if (!made) made = null;
	    
	    var cb = f || function () {};
	    p = path.resolve(p);
	    
	    xfs.mkdir(p, mode, function (er) {
	        if (!er) {
	            made = made || p;
	            return cb(null, made);
	        }
	        switch (er.code) {
	            case 'ENOENT':
	                mkdirP(path.dirname(p), opts, function (er, made) {
	                    if (er) cb(er, made);
	                    else mkdirP(p, opts, cb, made);
	                });
	                break;

	            // In the case of any other error, just see if there's a dir
	            // there already.  If so, then hooray!  If not, then something
	            // is borked.
	            default:
	                xfs.stat(p, function (er2, stat) {
	                    // if the stat fails, then that's super weird.
	                    // let the original error be the failure reason.
	                    if (er2 || !stat.isDirectory()) cb(er, made)
	                    else cb(null, made);
	                });
	                break;
	        }
	    });
	}

	mkdirP.sync = function sync (p, opts, made) {
	    if (!opts || typeof opts !== 'object') {
	        opts = { mode: opts };
	    }
	    
	    var mode = opts.mode;
	    var xfs = opts.fs || fs;
	    
	    if (mode === undefined) {
	        mode = _0777 & (~process.umask());
	    }
	    if (!made) made = null;

	    p = path.resolve(p);

	    try {
	        xfs.mkdirSync(p, mode);
	        made = made || p;
	    }
	    catch (err0) {
	        switch (err0.code) {
	            case 'ENOENT' :
	                made = sync(path.dirname(p), opts, made);
	                sync(p, opts, made);
	                break;

	            // In the case of any other error, just see if there's a dir
	            // there already.  If so, then hooray!  If not, then something
	            // is borked.
	            default:
	                var stat;
	                try {
	                    stat = xfs.statSync(p);
	                }
	                catch (err1) {
	                    throw err0;
	                }
	                if (!stat.isDirectory()) throw err0;
	                break;
	        }
	    }

	    return made;
	};

	}).call(this,require('_process'))
	},{"_process":82,"fs":42,"path":42}],80:[function(require,module,exports){
	exports.endianness = function () { return 'LE' };

	exports.hostname = function () {
	    if (typeof location !== 'undefined') {
	        return location.hostname
	    }
	    else return '';
	};

	exports.loadavg = function () { return [] };

	exports.uptime = function () { return 0 };

	exports.freemem = function () {
	    return Number.MAX_VALUE;
	};

	exports.totalmem = function () {
	    return Number.MAX_VALUE;
	};

	exports.cpus = function () { return [] };

	exports.type = function () { return 'Browser' };

	exports.release = function () {
	    if (typeof navigator !== 'undefined') {
	        return navigator.appVersion;
	    }
	    return '';
	};

	exports.networkInterfaces
	= exports.getNetworkInterfaces
	= function () { return {} };

	exports.arch = function () { return 'javascript' };

	exports.platform = function () { return 'browser' };

	exports.tmpdir = exports.tmpDir = function () {
	    return '/tmp';
	};

	exports.EOL = '\n';

	},{}],81:[function(require,module,exports){
	(function (process){
	'use strict';

	if (!process.version ||
	    process.version.indexOf('v0.') === 0 ||
	    process.version.indexOf('v1.') === 0 && process.version.indexOf('v1.8.') !== 0) {
	  module.exports = nextTick;
	} else {
	  module.exports = process.nextTick;
	}

	function nextTick(fn, arg1, arg2, arg3) {
	  if (typeof fn !== 'function') {
	    throw new TypeError('"callback" argument must be a function');
	  }
	  var len = arguments.length;
	  var args, i;
	  switch (len) {
	  case 0:
	  case 1:
	    return process.nextTick(fn);
	  case 2:
	    return process.nextTick(function afterTickOne() {
	      fn.call(null, arg1);
	    });
	  case 3:
	    return process.nextTick(function afterTickTwo() {
	      fn.call(null, arg1, arg2);
	    });
	  case 4:
	    return process.nextTick(function afterTickThree() {
	      fn.call(null, arg1, arg2, arg3);
	    });
	  default:
	    args = new Array(len - 1);
	    i = 0;
	    while (i < args.length) {
	      args[i++] = arguments[i];
	    }
	    return process.nextTick(function afterTick() {
	      fn.apply(null, args);
	    });
	  }
	}

	}).call(this,require('_process'))
	},{"_process":82}],82:[function(require,module,exports){
	// shim for using process in browser
	var process = module.exports = {};

	// cached from whatever global is present so that test runners that stub it
	// don't break things.  But we need to wrap it in a try catch in case it is
	// wrapped in strict mode code which doesn't define any globals.  It's inside a
	// function because try/catches deoptimize in certain engines.

	var cachedSetTimeout;
	var cachedClearTimeout;

	function defaultSetTimout() {
	    throw new Error('setTimeout has not been defined');
	}
	function defaultClearTimeout () {
	    throw new Error('clearTimeout has not been defined');
	}
	(function () {
	    try {
	        if (typeof setTimeout === 'function') {
	            cachedSetTimeout = setTimeout;
	        } else {
	            cachedSetTimeout = defaultSetTimout;
	        }
	    } catch (e) {
	        cachedSetTimeout = defaultSetTimout;
	    }
	    try {
	        if (typeof clearTimeout === 'function') {
	            cachedClearTimeout = clearTimeout;
	        } else {
	            cachedClearTimeout = defaultClearTimeout;
	        }
	    } catch (e) {
	        cachedClearTimeout = defaultClearTimeout;
	    }
	} ())
	function runTimeout(fun) {
	    if (cachedSetTimeout === setTimeout) {
	        //normal enviroments in sane situations
	        return setTimeout(fun, 0);
	    }
	    // if setTimeout wasn't available but was latter defined
	    if ((cachedSetTimeout === defaultSetTimout || !cachedSetTimeout) && setTimeout) {
	        cachedSetTimeout = setTimeout;
	        return setTimeout(fun, 0);
	    }
	    try {
	        // when when somebody has screwed with setTimeout but no I.E. maddness
	        return cachedSetTimeout(fun, 0);
	    } catch(e){
	        try {
	            // When we are in I.E. but the script has been evaled so I.E. doesn't trust the global object when called normally
	            return cachedSetTimeout.call(null, fun, 0);
	        } catch(e){
	            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error
	            return cachedSetTimeout.call(this, fun, 0);
	        }
	    }


	}
	function runClearTimeout(marker) {
	    if (cachedClearTimeout === clearTimeout) {
	        //normal enviroments in sane situations
	        return clearTimeout(marker);
	    }
	    // if clearTimeout wasn't available but was latter defined
	    if ((cachedClearTimeout === defaultClearTimeout || !cachedClearTimeout) && clearTimeout) {
	        cachedClearTimeout = clearTimeout;
	        return clearTimeout(marker);
	    }
	    try {
	        // when when somebody has screwed with setTimeout but no I.E. maddness
	        return cachedClearTimeout(marker);
	    } catch (e){
	        try {
	            // When we are in I.E. but the script has been evaled so I.E. doesn't  trust the global object when called normally
	            return cachedClearTimeout.call(null, marker);
	        } catch (e){
	            // same as above but when it's a version of I.E. that must have the global object for 'this', hopfully our context correct otherwise it will throw a global error.
	            // Some versions of I.E. have different rules for clearTimeout vs setTimeout
	            return cachedClearTimeout.call(this, marker);
	        }
	    }



	}
	var queue = [];
	var draining = false;
	var currentQueue;
	var queueIndex = -1;

	function cleanUpNextTick() {
	    if (!draining || !currentQueue) {
	        return;
	    }
	    draining = false;
	    if (currentQueue.length) {
	        queue = currentQueue.concat(queue);
	    } else {
	        queueIndex = -1;
	    }
	    if (queue.length) {
	        drainQueue();
	    }
	}

	function drainQueue() {
	    if (draining) {
	        return;
	    }
	    var timeout = runTimeout(cleanUpNextTick);
	    draining = true;

	    var len = queue.length;
	    while(len) {
	        currentQueue = queue;
	        queue = [];
	        while (++queueIndex < len) {
	            if (currentQueue) {
	                currentQueue[queueIndex].run();
	            }
	        }
	        queueIndex = -1;
	        len = queue.length;
	    }
	    currentQueue = null;
	    draining = false;
	    runClearTimeout(timeout);
	}

	process.nextTick = function (fun) {
	    var args = new Array(arguments.length - 1);
	    if (arguments.length > 1) {
	        for (var i = 1; i < arguments.length; i++) {
	            args[i - 1] = arguments[i];
	        }
	    }
	    queue.push(new Item(fun, args));
	    if (queue.length === 1 && !draining) {
	        runTimeout(drainQueue);
	    }
	};

	// v8 likes predictible objects
	function Item(fun, array) {
	    this.fun = fun;
	    this.array = array;
	}
	Item.prototype.run = function () {
	    this.fun.apply(null, this.array);
	};
	process.title = 'browser';
	process.browser = true;
	process.env = {};
	process.argv = [];
	process.version = ''; // empty string to avoid regexp issues
	process.versions = {};

	function noop() {}

	process.on = noop;
	process.addListener = noop;
	process.once = noop;
	process.off = noop;
	process.removeListener = noop;
	process.removeAllListeners = noop;
	process.emit = noop;

	process.binding = function (name) {
	    throw new Error('process.binding is not supported');
	};

	process.cwd = function () { return '/' };
	process.chdir = function (dir) {
	    throw new Error('process.chdir is not supported');
	};
	process.umask = function() { return 0; };

	},{}],83:[function(require,module,exports){
	module.exports = require("./lib/_stream_duplex.js")

	},{"./lib/_stream_duplex.js":84}],84:[function(require,module,exports){
	// a duplex stream is just a stream that is both readable and writable.
	// Since JS doesn't have multiple prototypal inheritance, this class
	// prototypally inherits from Readable, and then parasitically from
	// Writable.

	'use strict';

	/*<replacement>*/

	var objectKeys = Object.keys || function (obj) {
	  var keys = [];
	  for (var key in obj) {
	    keys.push(key);
	  }return keys;
	};
	/*</replacement>*/

	module.exports = Duplex;

	/*<replacement>*/
	var processNextTick = require('process-nextick-args');
	/*</replacement>*/

	/*<replacement>*/
	var util = require('core-util-is');
	util.inherits = require('inherits');
	/*</replacement>*/

	var Readable = require('./_stream_readable');
	var Writable = require('./_stream_writable');

	util.inherits(Duplex, Readable);

	var keys = objectKeys(Writable.prototype);
	for (var v = 0; v < keys.length; v++) {
	  var method = keys[v];
	  if (!Duplex.prototype[method]) Duplex.prototype[method] = Writable.prototype[method];
	}

	function Duplex(options) {
	  if (!(this instanceof Duplex)) return new Duplex(options);

	  Readable.call(this, options);
	  Writable.call(this, options);

	  if (options && options.readable === false) this.readable = false;

	  if (options && options.writable === false) this.writable = false;

	  this.allowHalfOpen = true;
	  if (options && options.allowHalfOpen === false) this.allowHalfOpen = false;

	  this.once('end', onend);
	}

	// the no-half-open enforcer
	function onend() {
	  // if we allow half-open state, or if the writable side ended,
	  // then we're ok.
	  if (this.allowHalfOpen || this._writableState.ended) return;

	  // no more data can be written.
	  // But allow more writes to happen in this tick.
	  processNextTick(onEndNT, this);
	}

	function onEndNT(self) {
	  self.end();
	}

	function forEach(xs, f) {
	  for (var i = 0, l = xs.length; i < l; i++) {
	    f(xs[i], i);
	  }
	}
	},{"./_stream_readable":86,"./_stream_writable":88,"core-util-is":45,"inherits":66,"process-nextick-args":81}],85:[function(require,module,exports){
	// a passthrough stream.
	// basically just the most minimal sort of Transform stream.
	// Every written chunk gets output as-is.

	'use strict';

	module.exports = PassThrough;

	var Transform = require('./_stream_transform');

	/*<replacement>*/
	var util = require('core-util-is');
	util.inherits = require('inherits');
	/*</replacement>*/

	util.inherits(PassThrough, Transform);

	function PassThrough(options) {
	  if (!(this instanceof PassThrough)) return new PassThrough(options);

	  Transform.call(this, options);
	}

	PassThrough.prototype._transform = function (chunk, encoding, cb) {
	  cb(null, chunk);
	};
	},{"./_stream_transform":87,"core-util-is":45,"inherits":66}],86:[function(require,module,exports){
	(function (process){
	'use strict';

	module.exports = Readable;

	/*<replacement>*/
	var processNextTick = require('process-nextick-args');
	/*</replacement>*/

	/*<replacement>*/
	var isArray = require('isarray');
	/*</replacement>*/

	Readable.ReadableState = ReadableState;

	/*<replacement>*/
	var EE = require('events').EventEmitter;

	var EElistenerCount = function (emitter, type) {
	  return emitter.listeners(type).length;
	};
	/*</replacement>*/

	/*<replacement>*/
	var Stream;
	(function () {
	  try {
	    Stream = require('st' + 'ream');
	  } catch (_) {} finally {
	    if (!Stream) Stream = require('events').EventEmitter;
	  }
	})();
	/*</replacement>*/

	var Buffer = require('buffer').Buffer;
	/*<replacement>*/
	var bufferShim = require('buffer-shims');
	/*</replacement>*/

	/*<replacement>*/
	var util = require('core-util-is');
	util.inherits = require('inherits');
	/*</replacement>*/

	/*<replacement>*/
	var debugUtil = require('util');
	var debug = void 0;
	if (debugUtil && debugUtil.debuglog) {
	  debug = debugUtil.debuglog('stream');
	} else {
	  debug = function () {};
	}
	/*</replacement>*/

	var BufferList = require('./internal/streams/BufferList');
	var StringDecoder;

	util.inherits(Readable, Stream);

	function prependListener(emitter, event, fn) {
	  if (typeof emitter.prependListener === 'function') {
	    return emitter.prependListener(event, fn);
	  } else {
	    // This is a hack to make sure that our error handler is attached before any
	    // userland ones.  NEVER DO THIS. This is here only because this code needs
	    // to continue to work with older versions of Node.js that do not include
	    // the prependListener() method. The goal is to eventually remove this hack.
	    if (!emitter._events || !emitter._events[event]) emitter.on(event, fn);else if (isArray(emitter._events[event])) emitter._events[event].unshift(fn);else emitter._events[event] = [fn, emitter._events[event]];
	  }
	}

	var Duplex;
	function ReadableState(options, stream) {
	  Duplex = Duplex || require('./_stream_duplex');

	  options = options || {};

	  // object stream flag. Used to make read(n) ignore n and to
	  // make all the buffer merging and length checks go away
	  this.objectMode = !!options.objectMode;

	  if (stream instanceof Duplex) this.objectMode = this.objectMode || !!options.readableObjectMode;

	  // the point at which it stops calling _read() to fill the buffer
	  // Note: 0 is a valid value, means "don't call _read preemptively ever"
	  var hwm = options.highWaterMark;
	  var defaultHwm = this.objectMode ? 16 : 16 * 1024;
	  this.highWaterMark = hwm || hwm === 0 ? hwm : defaultHwm;

	  // cast to ints.
	  this.highWaterMark = ~ ~this.highWaterMark;

	  // A linked list is used to store data chunks instead of an array because the
	  // linked list can remove elements from the beginning faster than
	  // array.shift()
	  this.buffer = new BufferList();
	  this.length = 0;
	  this.pipes = null;
	  this.pipesCount = 0;
	  this.flowing = null;
	  this.ended = false;
	  this.endEmitted = false;
	  this.reading = false;

	  // a flag to be able to tell if the onwrite cb is called immediately,
	  // or on a later tick.  We set this to true at first, because any
	  // actions that shouldn't happen until "later" should generally also
	  // not happen before the first write call.
	  this.sync = true;

	  // whenever we return null, then we set a flag to say
	  // that we're awaiting a 'readable' event emission.
	  this.needReadable = false;
	  this.emittedReadable = false;
	  this.readableListening = false;
	  this.resumeScheduled = false;

	  // Crypto is kind of old and crusty.  Historically, its default string
	  // encoding is 'binary' so we have to make this configurable.
	  // Everything else in the universe uses 'utf8', though.
	  this.defaultEncoding = options.defaultEncoding || 'utf8';

	  // when piping, we only care about 'readable' events that happen
	  // after read()ing all the bytes and not getting any pushback.
	  this.ranOut = false;

	  // the number of writers that are awaiting a drain event in .pipe()s
	  this.awaitDrain = 0;

	  // if true, a maybeReadMore has been scheduled
	  this.readingMore = false;

	  this.decoder = null;
	  this.encoding = null;
	  if (options.encoding) {
	    if (!StringDecoder) StringDecoder = require('string_decoder/').StringDecoder;
	    this.decoder = new StringDecoder(options.encoding);
	    this.encoding = options.encoding;
	  }
	}

	var Duplex;
	function Readable(options) {
	  Duplex = Duplex || require('./_stream_duplex');

	  if (!(this instanceof Readable)) return new Readable(options);

	  this._readableState = new ReadableState(options, this);

	  // legacy
	  this.readable = true;

	  if (options && typeof options.read === 'function') this._read = options.read;

	  Stream.call(this);
	}

	// Manually shove something into the read() buffer.
	// This returns true if the highWaterMark has not been hit yet,
	// similar to how Writable.write() returns true if you should
	// write() some more.
	Readable.prototype.push = function (chunk, encoding) {
	  var state = this._readableState;

	  if (!state.objectMode && typeof chunk === 'string') {
	    encoding = encoding || state.defaultEncoding;
	    if (encoding !== state.encoding) {
	      chunk = bufferShim.from(chunk, encoding);
	      encoding = '';
	    }
	  }

	  return readableAddChunk(this, state, chunk, encoding, false);
	};

	// Unshift should *always* be something directly out of read()
	Readable.prototype.unshift = function (chunk) {
	  var state = this._readableState;
	  return readableAddChunk(this, state, chunk, '', true);
	};

	Readable.prototype.isPaused = function () {
	  return this._readableState.flowing === false;
	};

	function readableAddChunk(stream, state, chunk, encoding, addToFront) {
	  var er = chunkInvalid(state, chunk);
	  if (er) {
	    stream.emit('error', er);
	  } else if (chunk === null) {
	    state.reading = false;
	    onEofChunk(stream, state);
	  } else if (state.objectMode || chunk && chunk.length > 0) {
	    if (state.ended && !addToFront) {
	      var e = new Error('stream.push() after EOF');
	      stream.emit('error', e);
	    } else if (state.endEmitted && addToFront) {
	      var _e = new Error('stream.unshift() after end event');
	      stream.emit('error', _e);
	    } else {
	      var skipAdd;
	      if (state.decoder && !addToFront && !encoding) {
	        chunk = state.decoder.write(chunk);
	        skipAdd = !state.objectMode && chunk.length === 0;
	      }

	      if (!addToFront) state.reading = false;

	      // Don't add to the buffer if we've decoded to an empty string chunk and
	      // we're not in object mode
	      if (!skipAdd) {
	        // if we want the data now, just emit it.
	        if (state.flowing && state.length === 0 && !state.sync) {
	          stream.emit('data', chunk);
	          stream.read(0);
	        } else {
	          // update the buffer info.
	          state.length += state.objectMode ? 1 : chunk.length;
	          if (addToFront) state.buffer.unshift(chunk);else state.buffer.push(chunk);

	          if (state.needReadable) emitReadable(stream);
	        }
	      }

	      maybeReadMore(stream, state);
	    }
	  } else if (!addToFront) {
	    state.reading = false;
	  }

	  return needMoreData(state);
	}

	// if it's past the high water mark, we can push in some more.
	// Also, if we have no data yet, we can stand some
	// more bytes.  This is to work around cases where hwm=0,
	// such as the repl.  Also, if the push() triggered a
	// readable event, and the user called read(largeNumber) such that
	// needReadable was set, then we ought to push more, so that another
	// 'readable' event will be triggered.
	function needMoreData(state) {
	  return !state.ended && (state.needReadable || state.length < state.highWaterMark || state.length === 0);
	}

	// backwards compatibility.
	Readable.prototype.setEncoding = function (enc) {
	  if (!StringDecoder) StringDecoder = require('string_decoder/').StringDecoder;
	  this._readableState.decoder = new StringDecoder(enc);
	  this._readableState.encoding = enc;
	  return this;
	};

	// Don't raise the hwm > 8MB
	var MAX_HWM = 0x800000;
	function computeNewHighWaterMark(n) {
	  if (n >= MAX_HWM) {
	    n = MAX_HWM;
	  } else {
	    // Get the next highest power of 2 to prevent increasing hwm excessively in
	    // tiny amounts
	    n--;
	    n |= n >>> 1;
	    n |= n >>> 2;
	    n |= n >>> 4;
	    n |= n >>> 8;
	    n |= n >>> 16;
	    n++;
	  }
	  return n;
	}

	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function howMuchToRead(n, state) {
	  if (n <= 0 || state.length === 0 && state.ended) return 0;
	  if (state.objectMode) return 1;
	  if (n !== n) {
	    // Only flow one buffer at a time
	    if (state.flowing && state.length) return state.buffer.head.data.length;else return state.length;
	  }
	  // If we're asking for more than the current hwm, then raise the hwm.
	  if (n > state.highWaterMark) state.highWaterMark = computeNewHighWaterMark(n);
	  if (n <= state.length) return n;
	  // Don't have enough
	  if (!state.ended) {
	    state.needReadable = true;
	    return 0;
	  }
	  return state.length;
	}

	// you can override either this method, or the async _read(n) below.
	Readable.prototype.read = function (n) {
	  debug('read', n);
	  n = parseInt(n, 10);
	  var state = this._readableState;
	  var nOrig = n;

	  if (n !== 0) state.emittedReadable = false;

	  // if we're doing read(0) to trigger a readable event, but we
	  // already have a bunch of data in the buffer, then just trigger
	  // the 'readable' event and move on.
	  if (n === 0 && state.needReadable && (state.length >= state.highWaterMark || state.ended)) {
	    debug('read: emitReadable', state.length, state.ended);
	    if (state.length === 0 && state.ended) endReadable(this);else emitReadable(this);
	    return null;
	  }

	  n = howMuchToRead(n, state);

	  // if we've ended, and we're now clear, then finish it up.
	  if (n === 0 && state.ended) {
	    if (state.length === 0) endReadable(this);
	    return null;
	  }

	  // All the actual chunk generation logic needs to be
	  // *below* the call to _read.  The reason is that in certain
	  // synthetic stream cases, such as passthrough streams, _read
	  // may be a completely synchronous operation which may change
	  // the state of the read buffer, providing enough data when
	  // before there was *not* enough.
	  //
	  // So, the steps are:
	  // 1. Figure out what the state of things will be after we do
	  // a read from the buffer.
	  //
	  // 2. If that resulting state will trigger a _read, then call _read.
	  // Note that this may be asynchronous, or synchronous.  Yes, it is
	  // deeply ugly to write APIs this way, but that still doesn't mean
	  // that the Readable class should behave improperly, as streams are
	  // designed to be sync/async agnostic.
	  // Take note if the _read call is sync or async (ie, if the read call
	  // has returned yet), so that we know whether or not it's safe to emit
	  // 'readable' etc.
	  //
	  // 3. Actually pull the requested chunks out of the buffer and return.

	  // if we need a readable event, then we need to do some reading.
	  var doRead = state.needReadable;
	  debug('need readable', doRead);

	  // if we currently have less than the highWaterMark, then also read some
	  if (state.length === 0 || state.length - n < state.highWaterMark) {
	    doRead = true;
	    debug('length less than watermark', doRead);
	  }

	  // however, if we've ended, then there's no point, and if we're already
	  // reading, then it's unnecessary.
	  if (state.ended || state.reading) {
	    doRead = false;
	    debug('reading or ended', doRead);
	  } else if (doRead) {
	    debug('do read');
	    state.reading = true;
	    state.sync = true;
	    // if the length is currently zero, then we *need* a readable event.
	    if (state.length === 0) state.needReadable = true;
	    // call internal read method
	    this._read(state.highWaterMark);
	    state.sync = false;
	    // If _read pushed data synchronously, then `reading` will be false,
	    // and we need to re-evaluate how much data we can return to the user.
	    if (!state.reading) n = howMuchToRead(nOrig, state);
	  }

	  var ret;
	  if (n > 0) ret = fromList(n, state);else ret = null;

	  if (ret === null) {
	    state.needReadable = true;
	    n = 0;
	  } else {
	    state.length -= n;
	  }

	  if (state.length === 0) {
	    // If we have nothing in the buffer, then we want to know
	    // as soon as we *do* get something into the buffer.
	    if (!state.ended) state.needReadable = true;

	    // If we tried to read() past the EOF, then emit end on the next tick.
	    if (nOrig !== n && state.ended) endReadable(this);
	  }

	  if (ret !== null) this.emit('data', ret);

	  return ret;
	};

	function chunkInvalid(state, chunk) {
	  var er = null;
	  if (!Buffer.isBuffer(chunk) && typeof chunk !== 'string' && chunk !== null && chunk !== undefined && !state.objectMode) {
	    er = new TypeError('Invalid non-string/buffer chunk');
	  }
	  return er;
	}

	function onEofChunk(stream, state) {
	  if (state.ended) return;
	  if (state.decoder) {
	    var chunk = state.decoder.end();
	    if (chunk && chunk.length) {
	      state.buffer.push(chunk);
	      state.length += state.objectMode ? 1 : chunk.length;
	    }
	  }
	  state.ended = true;

	  // emit 'readable' now to make sure it gets picked up.
	  emitReadable(stream);
	}

	// Don't emit readable right away in sync mode, because this can trigger
	// another read() call => stack overflow.  This way, it might trigger
	// a nextTick recursion warning, but that's not so bad.
	function emitReadable(stream) {
	  var state = stream._readableState;
	  state.needReadable = false;
	  if (!state.emittedReadable) {
	    debug('emitReadable', state.flowing);
	    state.emittedReadable = true;
	    if (state.sync) processNextTick(emitReadable_, stream);else emitReadable_(stream);
	  }
	}

	function emitReadable_(stream) {
	  debug('emit readable');
	  stream.emit('readable');
	  flow(stream);
	}

	// at this point, the user has presumably seen the 'readable' event,
	// and called read() to consume some data.  that may have triggered
	// in turn another _read(n) call, in which case reading = true if
	// it's in progress.
	// However, if we're not ended, or reading, and the length < hwm,
	// then go ahead and try to read some more preemptively.
	function maybeReadMore(stream, state) {
	  if (!state.readingMore) {
	    state.readingMore = true;
	    processNextTick(maybeReadMore_, stream, state);
	  }
	}

	function maybeReadMore_(stream, state) {
	  var len = state.length;
	  while (!state.reading && !state.flowing && !state.ended && state.length < state.highWaterMark) {
	    debug('maybeReadMore read 0');
	    stream.read(0);
	    if (len === state.length)
	      // didn't get any data, stop spinning.
	      break;else len = state.length;
	  }
	  state.readingMore = false;
	}

	// abstract method.  to be overridden in specific implementation classes.
	// call cb(er, data) where data is <= n in length.
	// for virtual (non-string, non-buffer) streams, "length" is somewhat
	// arbitrary, and perhaps not very meaningful.
	Readable.prototype._read = function (n) {
	  this.emit('error', new Error('not implemented'));
	};

	Readable.prototype.pipe = function (dest, pipeOpts) {
	  var src = this;
	  var state = this._readableState;

	  switch (state.pipesCount) {
	    case 0:
	      state.pipes = dest;
	      break;
	    case 1:
	      state.pipes = [state.pipes, dest];
	      break;
	    default:
	      state.pipes.push(dest);
	      break;
	  }
	  state.pipesCount += 1;
	  debug('pipe count=%d opts=%j', state.pipesCount, pipeOpts);

	  var doEnd = (!pipeOpts || pipeOpts.end !== false) && dest !== process.stdout && dest !== process.stderr;

	  var endFn = doEnd ? onend : cleanup;
	  if (state.endEmitted) processNextTick(endFn);else src.once('end', endFn);

	  dest.on('unpipe', onunpipe);
	  function onunpipe(readable) {
	    debug('onunpipe');
	    if (readable === src) {
	      cleanup();
	    }
	  }

	  function onend() {
	    debug('onend');
	    dest.end();
	  }

	  // when the dest drains, it reduces the awaitDrain counter
	  // on the source.  This would be more elegant with a .once()
	  // handler in flow(), but adding and removing repeatedly is
	  // too slow.
	  var ondrain = pipeOnDrain(src);
	  dest.on('drain', ondrain);

	  var cleanedUp = false;
	  function cleanup() {
	    debug('cleanup');
	    // cleanup event handlers once the pipe is broken
	    dest.removeListener('close', onclose);
	    dest.removeListener('finish', onfinish);
	    dest.removeListener('drain', ondrain);
	    dest.removeListener('error', onerror);
	    dest.removeListener('unpipe', onunpipe);
	    src.removeListener('end', onend);
	    src.removeListener('end', cleanup);
	    src.removeListener('data', ondata);

	    cleanedUp = true;

	    // if the reader is waiting for a drain event from this
	    // specific writer, then it would cause it to never start
	    // flowing again.
	    // So, if this is awaiting a drain, then we just call it now.
	    // If we don't know, then assume that we are waiting for one.
	    if (state.awaitDrain && (!dest._writableState || dest._writableState.needDrain)) ondrain();
	  }

	  // If the user pushes more data while we're writing to dest then we'll end up
	  // in ondata again. However, we only want to increase awaitDrain once because
	  // dest will only emit one 'drain' event for the multiple writes.
	  // => Introduce a guard on increasing awaitDrain.
	  var increasedAwaitDrain = false;
	  src.on('data', ondata);
	  function ondata(chunk) {
	    debug('ondata');
	    increasedAwaitDrain = false;
	    var ret = dest.write(chunk);
	    if (false === ret && !increasedAwaitDrain) {
	      // If the user unpiped during `dest.write()`, it is possible
	      // to get stuck in a permanently paused state if that write
	      // also returned false.
	      // => Check whether `dest` is still a piping destination.
	      if ((state.pipesCount === 1 && state.pipes === dest || state.pipesCount > 1 && indexOf(state.pipes, dest) !== -1) && !cleanedUp) {
	        debug('false write response, pause', src._readableState.awaitDrain);
	        src._readableState.awaitDrain++;
	        increasedAwaitDrain = true;
	      }
	      src.pause();
	    }
	  }

	  // if the dest has an error, then stop piping into it.
	  // however, don't suppress the throwing behavior for this.
	  function onerror(er) {
	    debug('onerror', er);
	    unpipe();
	    dest.removeListener('error', onerror);
	    if (EElistenerCount(dest, 'error') === 0) dest.emit('error', er);
	  }

	  // Make sure our error handler is attached before userland ones.
	  prependListener(dest, 'error', onerror);

	  // Both close and finish should trigger unpipe, but only once.
	  function onclose() {
	    dest.removeListener('finish', onfinish);
	    unpipe();
	  }
	  dest.once('close', onclose);
	  function onfinish() {
	    debug('onfinish');
	    dest.removeListener('close', onclose);
	    unpipe();
	  }
	  dest.once('finish', onfinish);

	  function unpipe() {
	    debug('unpipe');
	    src.unpipe(dest);
	  }

	  // tell the dest that it's being piped to
	  dest.emit('pipe', src);

	  // start the flow if it hasn't been started already.
	  if (!state.flowing) {
	    debug('pipe resume');
	    src.resume();
	  }

	  return dest;
	};

	function pipeOnDrain(src) {
	  return function () {
	    var state = src._readableState;
	    debug('pipeOnDrain', state.awaitDrain);
	    if (state.awaitDrain) state.awaitDrain--;
	    if (state.awaitDrain === 0 && EElistenerCount(src, 'data')) {
	      state.flowing = true;
	      flow(src);
	    }
	  };
	}

	Readable.prototype.unpipe = function (dest) {
	  var state = this._readableState;

	  // if we're not piping anywhere, then do nothing.
	  if (state.pipesCount === 0) return this;

	  // just one destination.  most common case.
	  if (state.pipesCount === 1) {
	    // passed in one, but it's not the right one.
	    if (dest && dest !== state.pipes) return this;

	    if (!dest) dest = state.pipes;

	    // got a match.
	    state.pipes = null;
	    state.pipesCount = 0;
	    state.flowing = false;
	    if (dest) dest.emit('unpipe', this);
	    return this;
	  }

	  // slow case. multiple pipe destinations.

	  if (!dest) {
	    // remove all.
	    var dests = state.pipes;
	    var len = state.pipesCount;
	    state.pipes = null;
	    state.pipesCount = 0;
	    state.flowing = false;

	    for (var _i = 0; _i < len; _i++) {
	      dests[_i].emit('unpipe', this);
	    }return this;
	  }

	  // try to find the right one.
	  var i = indexOf(state.pipes, dest);
	  if (i === -1) return this;

	  state.pipes.splice(i, 1);
	  state.pipesCount -= 1;
	  if (state.pipesCount === 1) state.pipes = state.pipes[0];

	  dest.emit('unpipe', this);

	  return this;
	};

	// set up data events if they are asked for
	// Ensure readable listeners eventually get something
	Readable.prototype.on = function (ev, fn) {
	  var res = Stream.prototype.on.call(this, ev, fn);

	  if (ev === 'data') {
	    // Start flowing on next tick if stream isn't explicitly paused
	    if (this._readableState.flowing !== false) this.resume();
	  } else if (ev === 'readable') {
	    var state = this._readableState;
	    if (!state.endEmitted && !state.readableListening) {
	      state.readableListening = state.needReadable = true;
	      state.emittedReadable = false;
	      if (!state.reading) {
	        processNextTick(nReadingNextTick, this);
	      } else if (state.length) {
	        emitReadable(this, state);
	      }
	    }
	  }

	  return res;
	};
	Readable.prototype.addListener = Readable.prototype.on;

	function nReadingNextTick(self) {
	  debug('readable nexttick read 0');
	  self.read(0);
	}

	// pause() and resume() are remnants of the legacy readable stream API
	// If the user uses them, then switch into old mode.
	Readable.prototype.resume = function () {
	  var state = this._readableState;
	  if (!state.flowing) {
	    debug('resume');
	    state.flowing = true;
	    resume(this, state);
	  }
	  return this;
	};

	function resume(stream, state) {
	  if (!state.resumeScheduled) {
	    state.resumeScheduled = true;
	    processNextTick(resume_, stream, state);
	  }
	}

	function resume_(stream, state) {
	  if (!state.reading) {
	    debug('resume read 0');
	    stream.read(0);
	  }

	  state.resumeScheduled = false;
	  state.awaitDrain = 0;
	  stream.emit('resume');
	  flow(stream);
	  if (state.flowing && !state.reading) stream.read(0);
	}

	Readable.prototype.pause = function () {
	  debug('call pause flowing=%j', this._readableState.flowing);
	  if (false !== this._readableState.flowing) {
	    debug('pause');
	    this._readableState.flowing = false;
	    this.emit('pause');
	  }
	  return this;
	};

	function flow(stream) {
	  var state = stream._readableState;
	  debug('flow', state.flowing);
	  while (state.flowing && stream.read() !== null) {}
	}

	// wrap an old-style stream as the async data source.
	// This is *not* part of the readable stream interface.
	// It is an ugly unfortunate mess of history.
	Readable.prototype.wrap = function (stream) {
	  var state = this._readableState;
	  var paused = false;

	  var self = this;
	  stream.on('end', function () {
	    debug('wrapped end');
	    if (state.decoder && !state.ended) {
	      var chunk = state.decoder.end();
	      if (chunk && chunk.length) self.push(chunk);
	    }

	    self.push(null);
	  });

	  stream.on('data', function (chunk) {
	    debug('wrapped data');
	    if (state.decoder) chunk = state.decoder.write(chunk);

	    // don't skip over falsy values in objectMode
	    if (state.objectMode && (chunk === null || chunk === undefined)) return;else if (!state.objectMode && (!chunk || !chunk.length)) return;

	    var ret = self.push(chunk);
	    if (!ret) {
	      paused = true;
	      stream.pause();
	    }
	  });

	  // proxy all the other methods.
	  // important when wrapping filters and duplexes.
	  for (var i in stream) {
	    if (this[i] === undefined && typeof stream[i] === 'function') {
	      this[i] = function (method) {
	        return function () {
	          return stream[method].apply(stream, arguments);
	        };
	      }(i);
	    }
	  }

	  // proxy certain important events.
	  var events = ['error', 'close', 'destroy', 'pause', 'resume'];
	  forEach(events, function (ev) {
	    stream.on(ev, self.emit.bind(self, ev));
	  });

	  // when we try to consume some more bytes, simply unpause the
	  // underlying stream.
	  self._read = function (n) {
	    debug('wrapped _read', n);
	    if (paused) {
	      paused = false;
	      stream.resume();
	    }
	  };

	  return self;
	};

	// exposed for testing purposes only.
	Readable._fromList = fromList;

	// Pluck off n bytes from an array of buffers.
	// Length is the combined lengths of all the buffers in the list.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function fromList(n, state) {
	  // nothing buffered
	  if (state.length === 0) return null;

	  var ret;
	  if (state.objectMode) ret = state.buffer.shift();else if (!n || n >= state.length) {
	    // read it all, truncate the list
	    if (state.decoder) ret = state.buffer.join('');else if (state.buffer.length === 1) ret = state.buffer.head.data;else ret = state.buffer.concat(state.length);
	    state.buffer.clear();
	  } else {
	    // read part of list
	    ret = fromListPartial(n, state.buffer, state.decoder);
	  }

	  return ret;
	}

	// Extracts only enough buffered data to satisfy the amount requested.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function fromListPartial(n, list, hasStrings) {
	  var ret;
	  if (n < list.head.data.length) {
	    // slice is the same for buffers and strings
	    ret = list.head.data.slice(0, n);
	    list.head.data = list.head.data.slice(n);
	  } else if (n === list.head.data.length) {
	    // first chunk is a perfect match
	    ret = list.shift();
	  } else {
	    // result spans more than one buffer
	    ret = hasStrings ? copyFromBufferString(n, list) : copyFromBuffer(n, list);
	  }
	  return ret;
	}

	// Copies a specified amount of characters from the list of buffered data
	// chunks.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function copyFromBufferString(n, list) {
	  var p = list.head;
	  var c = 1;
	  var ret = p.data;
	  n -= ret.length;
	  while (p = p.next) {
	    var str = p.data;
	    var nb = n > str.length ? str.length : n;
	    if (nb === str.length) ret += str;else ret += str.slice(0, n);
	    n -= nb;
	    if (n === 0) {
	      if (nb === str.length) {
	        ++c;
	        if (p.next) list.head = p.next;else list.head = list.tail = null;
	      } else {
	        list.head = p;
	        p.data = str.slice(nb);
	      }
	      break;
	    }
	    ++c;
	  }
	  list.length -= c;
	  return ret;
	}

	// Copies a specified amount of bytes from the list of buffered data chunks.
	// This function is designed to be inlinable, so please take care when making
	// changes to the function body.
	function copyFromBuffer(n, list) {
	  var ret = bufferShim.allocUnsafe(n);
	  var p = list.head;
	  var c = 1;
	  p.data.copy(ret);
	  n -= p.data.length;
	  while (p = p.next) {
	    var buf = p.data;
	    var nb = n > buf.length ? buf.length : n;
	    buf.copy(ret, ret.length - n, 0, nb);
	    n -= nb;
	    if (n === 0) {
	      if (nb === buf.length) {
	        ++c;
	        if (p.next) list.head = p.next;else list.head = list.tail = null;
	      } else {
	        list.head = p;
	        p.data = buf.slice(nb);
	      }
	      break;
	    }
	    ++c;
	  }
	  list.length -= c;
	  return ret;
	}

	function endReadable(stream) {
	  var state = stream._readableState;

	  // If we get here before consuming all the bytes, then that is a
	  // bug in node.  Should never happen.
	  if (state.length > 0) throw new Error('"endReadable()" called on non-empty stream');

	  if (!state.endEmitted) {
	    state.ended = true;
	    processNextTick(endReadableNT, state, stream);
	  }
	}

	function endReadableNT(state, stream) {
	  // Check that we didn't get one last unshift.
	  if (!state.endEmitted && state.length === 0) {
	    state.endEmitted = true;
	    stream.readable = false;
	    stream.emit('end');
	  }
	}

	function forEach(xs, f) {
	  for (var i = 0, l = xs.length; i < l; i++) {
	    f(xs[i], i);
	  }
	}

	function indexOf(xs, x) {
	  for (var i = 0, l = xs.length; i < l; i++) {
	    if (xs[i] === x) return i;
	  }
	  return -1;
	}
	}).call(this,require('_process'))
	},{"./_stream_duplex":84,"./internal/streams/BufferList":89,"_process":82,"buffer":44,"buffer-shims":43,"core-util-is":45,"events":63,"inherits":66,"isarray":68,"process-nextick-args":81,"string_decoder/":95,"util":40}],87:[function(require,module,exports){
	// a transform stream is a readable/writable stream where you do
	// something with the data.  Sometimes it's called a "filter",
	// but that's not a great name for it, since that implies a thing where
	// some bits pass through, and others are simply ignored.  (That would
	// be a valid example of a transform, of course.)
	//
	// While the output is causally related to the input, it's not a
	// necessarily symmetric or synchronous transformation.  For example,
	// a zlib stream might take multiple plain-text writes(), and then
	// emit a single compressed chunk some time in the future.
	//
	// Here's how this works:
	//
	// The Transform stream has all the aspects of the readable and writable
	// stream classes.  When you write(chunk), that calls _write(chunk,cb)
	// internally, and returns false if there's a lot of pending writes
	// buffered up.  When you call read(), that calls _read(n) until
	// there's enough pending readable data buffered up.
	//
	// In a transform stream, the written data is placed in a buffer.  When
	// _read(n) is called, it transforms the queued up data, calling the
	// buffered _write cb's as it consumes chunks.  If consuming a single
	// written chunk would result in multiple output chunks, then the first
	// outputted bit calls the readcb, and subsequent chunks just go into
	// the read buffer, and will cause it to emit 'readable' if necessary.
	//
	// This way, back-pressure is actually determined by the reading side,
	// since _read has to be called to start processing a new chunk.  However,
	// a pathological inflate type of transform can cause excessive buffering
	// here.  For example, imagine a stream where every byte of input is
	// interpreted as an integer from 0-255, and then results in that many
	// bytes of output.  Writing the 4 bytes {ff,ff,ff,ff} would result in
	// 1kb of data being output.  In this case, you could write a very small
	// amount of input, and end up with a very large amount of output.  In
	// such a pathological inflating mechanism, there'd be no way to tell
	// the system to stop doing the transform.  A single 4MB write could
	// cause the system to run out of memory.
	//
	// However, even in such a pathological case, only a single written chunk
	// would be consumed, and then the rest would wait (un-transformed) until
	// the results of the previous transformed chunk were consumed.

	'use strict';

	module.exports = Transform;

	var Duplex = require('./_stream_duplex');

	/*<replacement>*/
	var util = require('core-util-is');
	util.inherits = require('inherits');
	/*</replacement>*/

	util.inherits(Transform, Duplex);

	function TransformState(stream) {
	  this.afterTransform = function (er, data) {
	    return afterTransform(stream, er, data);
	  };

	  this.needTransform = false;
	  this.transforming = false;
	  this.writecb = null;
	  this.writechunk = null;
	  this.writeencoding = null;
	}

	function afterTransform(stream, er, data) {
	  var ts = stream._transformState;
	  ts.transforming = false;

	  var cb = ts.writecb;

	  if (!cb) return stream.emit('error', new Error('no writecb in Transform class'));

	  ts.writechunk = null;
	  ts.writecb = null;

	  if (data !== null && data !== undefined) stream.push(data);

	  cb(er);

	  var rs = stream._readableState;
	  rs.reading = false;
	  if (rs.needReadable || rs.length < rs.highWaterMark) {
	    stream._read(rs.highWaterMark);
	  }
	}

	function Transform(options) {
	  if (!(this instanceof Transform)) return new Transform(options);

	  Duplex.call(this, options);

	  this._transformState = new TransformState(this);

	  // when the writable side finishes, then flush out anything remaining.
	  var stream = this;

	  // start out asking for a readable event once data is transformed.
	  this._readableState.needReadable = true;

	  // we have implemented the _read method, and done the other things
	  // that Readable wants before the first _read call, so unset the
	  // sync guard flag.
	  this._readableState.sync = false;

	  if (options) {
	    if (typeof options.transform === 'function') this._transform = options.transform;

	    if (typeof options.flush === 'function') this._flush = options.flush;
	  }

	  this.once('prefinish', function () {
	    if (typeof this._flush === 'function') this._flush(function (er) {
	      done(stream, er);
	    });else done(stream);
	  });
	}

	Transform.prototype.push = function (chunk, encoding) {
	  this._transformState.needTransform = false;
	  return Duplex.prototype.push.call(this, chunk, encoding);
	};

	// This is the part where you do stuff!
	// override this function in implementation classes.
	// 'chunk' is an input chunk.
	//
	// Call `push(newChunk)` to pass along transformed output
	// to the readable side.  You may call 'push' zero or more times.
	//
	// Call `cb(err)` when you are done with this chunk.  If you pass
	// an error, then that'll put the hurt on the whole operation.  If you
	// never call cb(), then you'll never get another chunk.
	Transform.prototype._transform = function (chunk, encoding, cb) {
	  throw new Error('Not implemented');
	};

	Transform.prototype._write = function (chunk, encoding, cb) {
	  var ts = this._transformState;
	  ts.writecb = cb;
	  ts.writechunk = chunk;
	  ts.writeencoding = encoding;
	  if (!ts.transforming) {
	    var rs = this._readableState;
	    if (ts.needTransform || rs.needReadable || rs.length < rs.highWaterMark) this._read(rs.highWaterMark);
	  }
	};

	// Doesn't matter what the args are here.
	// _transform does all the work.
	// That we got here means that the readable side wants more data.
	Transform.prototype._read = function (n) {
	  var ts = this._transformState;

	  if (ts.writechunk !== null && ts.writecb && !ts.transforming) {
	    ts.transforming = true;
	    this._transform(ts.writechunk, ts.writeencoding, ts.afterTransform);
	  } else {
	    // mark that we need a transform, so that any data that comes in
	    // will get processed, now that we've asked for it.
	    ts.needTransform = true;
	  }
	};

	function done(stream, er) {
	  if (er) return stream.emit('error', er);

	  // if there's nothing in the write buffer, then that means
	  // that nothing more will ever be provided
	  var ws = stream._writableState;
	  var ts = stream._transformState;

	  if (ws.length) throw new Error('Calling transform done when ws.length != 0');

	  if (ts.transforming) throw new Error('Calling transform done when still transforming');

	  return stream.push(null);
	}
	},{"./_stream_duplex":84,"core-util-is":45,"inherits":66}],88:[function(require,module,exports){
	(function (process){
	// A bit simpler than readable streams.
	// Implement an async ._write(chunk, encoding, cb), and it'll handle all
	// the drain event emission and buffering.

	'use strict';

	module.exports = Writable;

	/*<replacement>*/
	var processNextTick = require('process-nextick-args');
	/*</replacement>*/

	/*<replacement>*/
	var asyncWrite = !process.browser && ['v0.10', 'v0.9.'].indexOf(process.version.slice(0, 5)) > -1 ? setImmediate : processNextTick;
	/*</replacement>*/

	Writable.WritableState = WritableState;

	/*<replacement>*/
	var util = require('core-util-is');
	util.inherits = require('inherits');
	/*</replacement>*/

	/*<replacement>*/
	var internalUtil = {
	  deprecate: require('util-deprecate')
	};
	/*</replacement>*/

	/*<replacement>*/
	var Stream;
	(function () {
	  try {
	    Stream = require('st' + 'ream');
	  } catch (_) {} finally {
	    if (!Stream) Stream = require('events').EventEmitter;
	  }
	})();
	/*</replacement>*/

	var Buffer = require('buffer').Buffer;
	/*<replacement>*/
	var bufferShim = require('buffer-shims');
	/*</replacement>*/

	util.inherits(Writable, Stream);

	function nop() {}

	function WriteReq(chunk, encoding, cb) {
	  this.chunk = chunk;
	  this.encoding = encoding;
	  this.callback = cb;
	  this.next = null;
	}

	var Duplex;
	function WritableState(options, stream) {
	  Duplex = Duplex || require('./_stream_duplex');

	  options = options || {};

	  // object stream flag to indicate whether or not this stream
	  // contains buffers or objects.
	  this.objectMode = !!options.objectMode;

	  if (stream instanceof Duplex) this.objectMode = this.objectMode || !!options.writableObjectMode;

	  // the point at which write() starts returning false
	  // Note: 0 is a valid value, means that we always return false if
	  // the entire buffer is not flushed immediately on write()
	  var hwm = options.highWaterMark;
	  var defaultHwm = this.objectMode ? 16 : 16 * 1024;
	  this.highWaterMark = hwm || hwm === 0 ? hwm : defaultHwm;

	  // cast to ints.
	  this.highWaterMark = ~ ~this.highWaterMark;

	  this.needDrain = false;
	  // at the start of calling end()
	  this.ending = false;
	  // when end() has been called, and returned
	  this.ended = false;
	  // when 'finish' is emitted
	  this.finished = false;

	  // should we decode strings into buffers before passing to _write?
	  // this is here so that some node-core streams can optimize string
	  // handling at a lower level.
	  var noDecode = options.decodeStrings === false;
	  this.decodeStrings = !noDecode;

	  // Crypto is kind of old and crusty.  Historically, its default string
	  // encoding is 'binary' so we have to make this configurable.
	  // Everything else in the universe uses 'utf8', though.
	  this.defaultEncoding = options.defaultEncoding || 'utf8';

	  // not an actual buffer we keep track of, but a measurement
	  // of how much we're waiting to get pushed to some underlying
	  // socket or file.
	  this.length = 0;

	  // a flag to see when we're in the middle of a write.
	  this.writing = false;

	  // when true all writes will be buffered until .uncork() call
	  this.corked = 0;

	  // a flag to be able to tell if the onwrite cb is called immediately,
	  // or on a later tick.  We set this to true at first, because any
	  // actions that shouldn't happen until "later" should generally also
	  // not happen before the first write call.
	  this.sync = true;

	  // a flag to know if we're processing previously buffered items, which
	  // may call the _write() callback in the same tick, so that we don't
	  // end up in an overlapped onwrite situation.
	  this.bufferProcessing = false;

	  // the callback that's passed to _write(chunk,cb)
	  this.onwrite = function (er) {
	    onwrite(stream, er);
	  };

	  // the callback that the user supplies to write(chunk,encoding,cb)
	  this.writecb = null;

	  // the amount that is being written when _write is called.
	  this.writelen = 0;

	  this.bufferedRequest = null;
	  this.lastBufferedRequest = null;

	  // number of pending user-supplied write callbacks
	  // this must be 0 before 'finish' can be emitted
	  this.pendingcb = 0;

	  // emit prefinish if the only thing we're waiting for is _write cbs
	  // This is relevant for synchronous Transform streams
	  this.prefinished = false;

	  // True if the error was already emitted and should not be thrown again
	  this.errorEmitted = false;

	  // count buffered requests
	  this.bufferedRequestCount = 0;

	  // allocate the first CorkedRequest, there is always
	  // one allocated and free to use, and we maintain at most two
	  this.corkedRequestsFree = new CorkedRequest(this);
	}

	WritableState.prototype.getBuffer = function writableStateGetBuffer() {
	  var current = this.bufferedRequest;
	  var out = [];
	  while (current) {
	    out.push(current);
	    current = current.next;
	  }
	  return out;
	};

	(function () {
	  try {
	    Object.defineProperty(WritableState.prototype, 'buffer', {
	      get: internalUtil.deprecate(function () {
	        return this.getBuffer();
	      }, '_writableState.buffer is deprecated. Use _writableState.getBuffer ' + 'instead.')
	    });
	  } catch (_) {}
	})();

	var Duplex;
	function Writable(options) {
	  Duplex = Duplex || require('./_stream_duplex');

	  // Writable ctor is applied to Duplexes, though they're not
	  // instanceof Writable, they're instanceof Readable.
	  if (!(this instanceof Writable) && !(this instanceof Duplex)) return new Writable(options);

	  this._writableState = new WritableState(options, this);

	  // legacy.
	  this.writable = true;

	  if (options) {
	    if (typeof options.write === 'function') this._write = options.write;

	    if (typeof options.writev === 'function') this._writev = options.writev;
	  }

	  Stream.call(this);
	}

	// Otherwise people can pipe Writable streams, which is just wrong.
	Writable.prototype.pipe = function () {
	  this.emit('error', new Error('Cannot pipe, not readable'));
	};

	function writeAfterEnd(stream, cb) {
	  var er = new Error('write after end');
	  // TODO: defer error events consistently everywhere, not just the cb
	  stream.emit('error', er);
	  processNextTick(cb, er);
	}

	// If we get something that is not a buffer, string, null, or undefined,
	// and we're not in objectMode, then that's an error.
	// Otherwise stream chunks are all considered to be of length=1, and the
	// watermarks determine how many objects to keep in the buffer, rather than
	// how many bytes or characters.
	function validChunk(stream, state, chunk, cb) {
	  var valid = true;
	  var er = false;
	  // Always throw error if a null is written
	  // if we are not in object mode then throw
	  // if it is not a buffer, string, or undefined.
	  if (chunk === null) {
	    er = new TypeError('May not write null values to stream');
	  } else if (!Buffer.isBuffer(chunk) && typeof chunk !== 'string' && chunk !== undefined && !state.objectMode) {
	    er = new TypeError('Invalid non-string/buffer chunk');
	  }
	  if (er) {
	    stream.emit('error', er);
	    processNextTick(cb, er);
	    valid = false;
	  }
	  return valid;
	}

	Writable.prototype.write = function (chunk, encoding, cb) {
	  var state = this._writableState;
	  var ret = false;

	  if (typeof encoding === 'function') {
	    cb = encoding;
	    encoding = null;
	  }

	  if (Buffer.isBuffer(chunk)) encoding = 'buffer';else if (!encoding) encoding = state.defaultEncoding;

	  if (typeof cb !== 'function') cb = nop;

	  if (state.ended) writeAfterEnd(this, cb);else if (validChunk(this, state, chunk, cb)) {
	    state.pendingcb++;
	    ret = writeOrBuffer(this, state, chunk, encoding, cb);
	  }

	  return ret;
	};

	Writable.prototype.cork = function () {
	  var state = this._writableState;

	  state.corked++;
	};

	Writable.prototype.uncork = function () {
	  var state = this._writableState;

	  if (state.corked) {
	    state.corked--;

	    if (!state.writing && !state.corked && !state.finished && !state.bufferProcessing && state.bufferedRequest) clearBuffer(this, state);
	  }
	};

	Writable.prototype.setDefaultEncoding = function setDefaultEncoding(encoding) {
	  // node::ParseEncoding() requires lower case.
	  if (typeof encoding === 'string') encoding = encoding.toLowerCase();
	  if (!(['hex', 'utf8', 'utf-8', 'ascii', 'binary', 'base64', 'ucs2', 'ucs-2', 'utf16le', 'utf-16le', 'raw'].indexOf((encoding + '').toLowerCase()) > -1)) throw new TypeError('Unknown encoding: ' + encoding);
	  this._writableState.defaultEncoding = encoding;
	  return this;
	};

	function decodeChunk(state, chunk, encoding) {
	  if (!state.objectMode && state.decodeStrings !== false && typeof chunk === 'string') {
	    chunk = bufferShim.from(chunk, encoding);
	  }
	  return chunk;
	}

	// if we're already writing something, then just put this
	// in the queue, and wait our turn.  Otherwise, call _write
	// If we return false, then we need a drain event, so set that flag.
	function writeOrBuffer(stream, state, chunk, encoding, cb) {
	  chunk = decodeChunk(state, chunk, encoding);

	  if (Buffer.isBuffer(chunk)) encoding = 'buffer';
	  var len = state.objectMode ? 1 : chunk.length;

	  state.length += len;

	  var ret = state.length < state.highWaterMark;
	  // we must ensure that previous needDrain will not be reset to false.
	  if (!ret) state.needDrain = true;

	  if (state.writing || state.corked) {
	    var last = state.lastBufferedRequest;
	    state.lastBufferedRequest = new WriteReq(chunk, encoding, cb);
	    if (last) {
	      last.next = state.lastBufferedRequest;
	    } else {
	      state.bufferedRequest = state.lastBufferedRequest;
	    }
	    state.bufferedRequestCount += 1;
	  } else {
	    doWrite(stream, state, false, len, chunk, encoding, cb);
	  }

	  return ret;
	}

	function doWrite(stream, state, writev, len, chunk, encoding, cb) {
	  state.writelen = len;
	  state.writecb = cb;
	  state.writing = true;
	  state.sync = true;
	  if (writev) stream._writev(chunk, state.onwrite);else stream._write(chunk, encoding, state.onwrite);
	  state.sync = false;
	}

	function onwriteError(stream, state, sync, er, cb) {
	  --state.pendingcb;
	  if (sync) processNextTick(cb, er);else cb(er);

	  stream._writableState.errorEmitted = true;
	  stream.emit('error', er);
	}

	function onwriteStateUpdate(state) {
	  state.writing = false;
	  state.writecb = null;
	  state.length -= state.writelen;
	  state.writelen = 0;
	}

	function onwrite(stream, er) {
	  var state = stream._writableState;
	  var sync = state.sync;
	  var cb = state.writecb;

	  onwriteStateUpdate(state);

	  if (er) onwriteError(stream, state, sync, er, cb);else {
	    // Check if we're actually ready to finish, but don't emit yet
	    var finished = needFinish(state);

	    if (!finished && !state.corked && !state.bufferProcessing && state.bufferedRequest) {
	      clearBuffer(stream, state);
	    }

	    if (sync) {
	      /*<replacement>*/
	      asyncWrite(afterWrite, stream, state, finished, cb);
	      /*</replacement>*/
	    } else {
	        afterWrite(stream, state, finished, cb);
	      }
	  }
	}

	function afterWrite(stream, state, finished, cb) {
	  if (!finished) onwriteDrain(stream, state);
	  state.pendingcb--;
	  cb();
	  finishMaybe(stream, state);
	}

	// Must force callback to be called on nextTick, so that we don't
	// emit 'drain' before the write() consumer gets the 'false' return
	// value, and has a chance to attach a 'drain' listener.
	function onwriteDrain(stream, state) {
	  if (state.length === 0 && state.needDrain) {
	    state.needDrain = false;
	    stream.emit('drain');
	  }
	}

	// if there's something in the buffer waiting, then process it
	function clearBuffer(stream, state) {
	  state.bufferProcessing = true;
	  var entry = state.bufferedRequest;

	  if (stream._writev && entry && entry.next) {
	    // Fast case, write everything using _writev()
	    var l = state.bufferedRequestCount;
	    var buffer = new Array(l);
	    var holder = state.corkedRequestsFree;
	    holder.entry = entry;

	    var count = 0;
	    while (entry) {
	      buffer[count] = entry;
	      entry = entry.next;
	      count += 1;
	    }

	    doWrite(stream, state, true, state.length, buffer, '', holder.finish);

	    // doWrite is almost always async, defer these to save a bit of time
	    // as the hot path ends with doWrite
	    state.pendingcb++;
	    state.lastBufferedRequest = null;
	    if (holder.next) {
	      state.corkedRequestsFree = holder.next;
	      holder.next = null;
	    } else {
	      state.corkedRequestsFree = new CorkedRequest(state);
	    }
	  } else {
	    // Slow case, write chunks one-by-one
	    while (entry) {
	      var chunk = entry.chunk;
	      var encoding = entry.encoding;
	      var cb = entry.callback;
	      var len = state.objectMode ? 1 : chunk.length;

	      doWrite(stream, state, false, len, chunk, encoding, cb);
	      entry = entry.next;
	      // if we didn't call the onwrite immediately, then
	      // it means that we need to wait until it does.
	      // also, that means that the chunk and cb are currently
	      // being processed, so move the buffer counter past them.
	      if (state.writing) {
	        break;
	      }
	    }

	    if (entry === null) state.lastBufferedRequest = null;
	  }

	  state.bufferedRequestCount = 0;
	  state.bufferedRequest = entry;
	  state.bufferProcessing = false;
	}

	Writable.prototype._write = function (chunk, encoding, cb) {
	  cb(new Error('not implemented'));
	};

	Writable.prototype._writev = null;

	Writable.prototype.end = function (chunk, encoding, cb) {
	  var state = this._writableState;

	  if (typeof chunk === 'function') {
	    cb = chunk;
	    chunk = null;
	    encoding = null;
	  } else if (typeof encoding === 'function') {
	    cb = encoding;
	    encoding = null;
	  }

	  if (chunk !== null && chunk !== undefined) this.write(chunk, encoding);

	  // .end() fully uncorks
	  if (state.corked) {
	    state.corked = 1;
	    this.uncork();
	  }

	  // ignore unnecessary end() calls.
	  if (!state.ending && !state.finished) endWritable(this, state, cb);
	};

	function needFinish(state) {
	  return state.ending && state.length === 0 && state.bufferedRequest === null && !state.finished && !state.writing;
	}

	function prefinish(stream, state) {
	  if (!state.prefinished) {
	    state.prefinished = true;
	    stream.emit('prefinish');
	  }
	}

	function finishMaybe(stream, state) {
	  var need = needFinish(state);
	  if (need) {
	    if (state.pendingcb === 0) {
	      prefinish(stream, state);
	      state.finished = true;
	      stream.emit('finish');
	    } else {
	      prefinish(stream, state);
	    }
	  }
	  return need;
	}

	function endWritable(stream, state, cb) {
	  state.ending = true;
	  finishMaybe(stream, state);
	  if (cb) {
	    if (state.finished) processNextTick(cb);else stream.once('finish', cb);
	  }
	  state.ended = true;
	  stream.writable = false;
	}

	// It seems a linked list but it is not
	// there will be only 2 of these for each stream
	function CorkedRequest(state) {
	  var _this = this;

	  this.next = null;
	  this.entry = null;

	  this.finish = function (err) {
	    var entry = _this.entry;
	    _this.entry = null;
	    while (entry) {
	      var cb = entry.callback;
	      state.pendingcb--;
	      cb(err);
	      entry = entry.next;
	    }
	    if (state.corkedRequestsFree) {
	      state.corkedRequestsFree.next = _this;
	    } else {
	      state.corkedRequestsFree = _this;
	    }
	  };
	}
	}).call(this,require('_process'))
	},{"./_stream_duplex":84,"_process":82,"buffer":44,"buffer-shims":43,"core-util-is":45,"events":63,"inherits":66,"process-nextick-args":81,"util-deprecate":96}],89:[function(require,module,exports){
	'use strict';

	var Buffer = require('buffer').Buffer;
	/*<replacement>*/
	var bufferShim = require('buffer-shims');
	/*</replacement>*/

	module.exports = BufferList;

	function BufferList() {
	  this.head = null;
	  this.tail = null;
	  this.length = 0;
	}

	BufferList.prototype.push = function (v) {
	  var entry = { data: v, next: null };
	  if (this.length > 0) this.tail.next = entry;else this.head = entry;
	  this.tail = entry;
	  ++this.length;
	};

	BufferList.prototype.unshift = function (v) {
	  var entry = { data: v, next: this.head };
	  if (this.length === 0) this.tail = entry;
	  this.head = entry;
	  ++this.length;
	};

	BufferList.prototype.shift = function () {
	  if (this.length === 0) return;
	  var ret = this.head.data;
	  if (this.length === 1) this.head = this.tail = null;else this.head = this.head.next;
	  --this.length;
	  return ret;
	};

	BufferList.prototype.clear = function () {
	  this.head = this.tail = null;
	  this.length = 0;
	};

	BufferList.prototype.join = function (s) {
	  if (this.length === 0) return '';
	  var p = this.head;
	  var ret = '' + p.data;
	  while (p = p.next) {
	    ret += s + p.data;
	  }return ret;
	};

	BufferList.prototype.concat = function (n) {
	  if (this.length === 0) return bufferShim.alloc(0);
	  if (this.length === 1) return this.head.data;
	  var ret = bufferShim.allocUnsafe(n >>> 0);
	  var p = this.head;
	  var i = 0;
	  while (p) {
	    p.data.copy(ret, i);
	    i += p.data.length;
	    p = p.next;
	  }
	  return ret;
	};
	},{"buffer":44,"buffer-shims":43}],90:[function(require,module,exports){
	module.exports = require("./lib/_stream_passthrough.js")

	},{"./lib/_stream_passthrough.js":85}],91:[function(require,module,exports){
	(function (process){
	var Stream = (function (){
	  try {
	    return require('st' + 'ream'); // hack to fix a circular dependency issue when used with browserify
	  } catch(_){}
	}());
	exports = module.exports = require('./lib/_stream_readable.js');
	exports.Stream = Stream || exports;
	exports.Readable = exports;
	exports.Writable = require('./lib/_stream_writable.js');
	exports.Duplex = require('./lib/_stream_duplex.js');
	exports.Transform = require('./lib/_stream_transform.js');
	exports.PassThrough = require('./lib/_stream_passthrough.js');

	if (!process.browser && process.env.READABLE_STREAM === 'disable' && Stream) {
	  module.exports = Stream;
	}

	}).call(this,require('_process'))
	},{"./lib/_stream_duplex.js":84,"./lib/_stream_passthrough.js":85,"./lib/_stream_readable.js":86,"./lib/_stream_transform.js":87,"./lib/_stream_writable.js":88,"_process":82}],92:[function(require,module,exports){
	module.exports = require("./lib/_stream_transform.js")

	},{"./lib/_stream_transform.js":87}],93:[function(require,module,exports){
	module.exports = require("./lib/_stream_writable.js")

	},{"./lib/_stream_writable.js":88}],94:[function(require,module,exports){
	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	module.exports = Stream;

	var EE = require('events').EventEmitter;
	var inherits = require('inherits');

	inherits(Stream, EE);
	Stream.Readable = require('readable-stream/readable.js');
	Stream.Writable = require('readable-stream/writable.js');
	Stream.Duplex = require('readable-stream/duplex.js');
	Stream.Transform = require('readable-stream/transform.js');
	Stream.PassThrough = require('readable-stream/passthrough.js');

	// Backwards-compat with node 0.4.x
	Stream.Stream = Stream;



	// old-style streams.  Note that the pipe method (the only relevant
	// part of this class) is overridden in the Readable class.

	function Stream() {
	  EE.call(this);
	}

	Stream.prototype.pipe = function(dest, options) {
	  var source = this;

	  function ondata(chunk) {
	    if (dest.writable) {
	      if (false === dest.write(chunk) && source.pause) {
	        source.pause();
	      }
	    }
	  }

	  source.on('data', ondata);

	  function ondrain() {
	    if (source.readable && source.resume) {
	      source.resume();
	    }
	  }

	  dest.on('drain', ondrain);

	  // If the 'end' option is not supplied, dest.end() will be called when
	  // source gets the 'end' or 'close' events.  Only dest.end() once.
	  if (!dest._isStdio && (!options || options.end !== false)) {
	    source.on('end', onend);
	    source.on('close', onclose);
	  }

	  var didOnEnd = false;
	  function onend() {
	    if (didOnEnd) return;
	    didOnEnd = true;

	    dest.end();
	  }


	  function onclose() {
	    if (didOnEnd) return;
	    didOnEnd = true;

	    if (typeof dest.destroy === 'function') dest.destroy();
	  }

	  // don't leave dangling pipes when there are errors.
	  function onerror(er) {
	    cleanup();
	    if (EE.listenerCount(this, 'error') === 0) {
	      throw er; // Unhandled stream error in pipe.
	    }
	  }

	  source.on('error', onerror);
	  dest.on('error', onerror);

	  // remove all the event listeners that were added.
	  function cleanup() {
	    source.removeListener('data', ondata);
	    dest.removeListener('drain', ondrain);

	    source.removeListener('end', onend);
	    source.removeListener('close', onclose);

	    source.removeListener('error', onerror);
	    dest.removeListener('error', onerror);

	    source.removeListener('end', cleanup);
	    source.removeListener('close', cleanup);

	    dest.removeListener('close', cleanup);
	  }

	  source.on('end', cleanup);
	  source.on('close', cleanup);

	  dest.on('close', cleanup);

	  dest.emit('pipe', source);

	  // Allow for unix-like usage: A.pipe(B).pipe(C)
	  return dest;
	};

	},{"events":63,"inherits":66,"readable-stream/duplex.js":83,"readable-stream/passthrough.js":90,"readable-stream/readable.js":91,"readable-stream/transform.js":92,"readable-stream/writable.js":93}],95:[function(require,module,exports){
	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	var Buffer = require('buffer').Buffer;

	var isBufferEncoding = Buffer.isEncoding
	  || function(encoding) {
	       switch (encoding && encoding.toLowerCase()) {
	         case 'hex': case 'utf8': case 'utf-8': case 'ascii': case 'binary': case 'base64': case 'ucs2': case 'ucs-2': case 'utf16le': case 'utf-16le': case 'raw': return true;
	         default: return false;
	       }
	     }


	function assertEncoding(encoding) {
	  if (encoding && !isBufferEncoding(encoding)) {
	    throw new Error('Unknown encoding: ' + encoding);
	  }
	}

	// StringDecoder provides an interface for efficiently splitting a series of
	// buffers into a series of JS strings without breaking apart multi-byte
	// characters. CESU-8 is handled as part of the UTF-8 encoding.
	//
	// @TODO Handling all encodings inside a single object makes it very difficult
	// to reason about this code, so it should be split up in the future.
	// @TODO There should be a utf8-strict encoding that rejects invalid UTF-8 code
	// points as used by CESU-8.
	var StringDecoder = exports.StringDecoder = function(encoding) {
	  this.encoding = (encoding || 'utf8').toLowerCase().replace(/[-_]/, '');
	  assertEncoding(encoding);
	  switch (this.encoding) {
	    case 'utf8':
	      // CESU-8 represents each of Surrogate Pair by 3-bytes
	      this.surrogateSize = 3;
	      break;
	    case 'ucs2':
	    case 'utf16le':
	      // UTF-16 represents each of Surrogate Pair by 2-bytes
	      this.surrogateSize = 2;
	      this.detectIncompleteChar = utf16DetectIncompleteChar;
	      break;
	    case 'base64':
	      // Base-64 stores 3 bytes in 4 chars, and pads the remainder.
	      this.surrogateSize = 3;
	      this.detectIncompleteChar = base64DetectIncompleteChar;
	      break;
	    default:
	      this.write = passThroughWrite;
	      return;
	  }

	  // Enough space to store all bytes of a single character. UTF-8 needs 4
	  // bytes, but CESU-8 may require up to 6 (3 bytes per surrogate).
	  this.charBuffer = new Buffer(6);
	  // Number of bytes received for the current incomplete multi-byte character.
	  this.charReceived = 0;
	  // Number of bytes expected for the current incomplete multi-byte character.
	  this.charLength = 0;
	};


	// write decodes the given buffer and returns it as JS string that is
	// guaranteed to not contain any partial multi-byte characters. Any partial
	// character found at the end of the buffer is buffered up, and will be
	// returned when calling write again with the remaining bytes.
	//
	// Note: Converting a Buffer containing an orphan surrogate to a String
	// currently works, but converting a String to a Buffer (via `new Buffer`, or
	// Buffer#write) will replace incomplete surrogates with the unicode
	// replacement character. See https://codereview.chromium.org/121173009/ .
	StringDecoder.prototype.write = function(buffer) {
	  var charStr = '';
	  // if our last write ended with an incomplete multibyte character
	  while (this.charLength) {
	    // determine how many remaining bytes this buffer has to offer for this char
	    var available = (buffer.length >= this.charLength - this.charReceived) ?
	        this.charLength - this.charReceived :
	        buffer.length;

	    // add the new bytes to the char buffer
	    buffer.copy(this.charBuffer, this.charReceived, 0, available);
	    this.charReceived += available;

	    if (this.charReceived < this.charLength) {
	      // still not enough chars in this buffer? wait for more ...
	      return '';
	    }

	    // remove bytes belonging to the current character from the buffer
	    buffer = buffer.slice(available, buffer.length);

	    // get the character that was split
	    charStr = this.charBuffer.slice(0, this.charLength).toString(this.encoding);

	    // CESU-8: lead surrogate (D800-DBFF) is also the incomplete character
	    var charCode = charStr.charCodeAt(charStr.length - 1);
	    if (charCode >= 0xD800 && charCode <= 0xDBFF) {
	      this.charLength += this.surrogateSize;
	      charStr = '';
	      continue;
	    }
	    this.charReceived = this.charLength = 0;

	    // if there are no more bytes in this buffer, just emit our char
	    if (buffer.length === 0) {
	      return charStr;
	    }
	    break;
	  }

	  // determine and set charLength / charReceived
	  this.detectIncompleteChar(buffer);

	  var end = buffer.length;
	  if (this.charLength) {
	    // buffer the incomplete character bytes we got
	    buffer.copy(this.charBuffer, 0, buffer.length - this.charReceived, end);
	    end -= this.charReceived;
	  }

	  charStr += buffer.toString(this.encoding, 0, end);

	  var end = charStr.length - 1;
	  var charCode = charStr.charCodeAt(end);
	  // CESU-8: lead surrogate (D800-DBFF) is also the incomplete character
	  if (charCode >= 0xD800 && charCode <= 0xDBFF) {
	    var size = this.surrogateSize;
	    this.charLength += size;
	    this.charReceived += size;
	    this.charBuffer.copy(this.charBuffer, size, 0, size);
	    buffer.copy(this.charBuffer, 0, 0, size);
	    return charStr.substring(0, end);
	  }

	  // or just emit the charStr
	  return charStr;
	};

	// detectIncompleteChar determines if there is an incomplete UTF-8 character at
	// the end of the given buffer. If so, it sets this.charLength to the byte
	// length that character, and sets this.charReceived to the number of bytes
	// that are available for this character.
	StringDecoder.prototype.detectIncompleteChar = function(buffer) {
	  // determine how many bytes we have to check at the end of this buffer
	  var i = (buffer.length >= 3) ? 3 : buffer.length;

	  // Figure out if one of the last i bytes of our buffer announces an
	  // incomplete char.
	  for (; i > 0; i--) {
	    var c = buffer[buffer.length - i];

	    // See http://en.wikipedia.org/wiki/UTF-8#Description

	    // 110XXXXX
	    if (i == 1 && c >> 5 == 0x06) {
	      this.charLength = 2;
	      break;
	    }

	    // 1110XXXX
	    if (i <= 2 && c >> 4 == 0x0E) {
	      this.charLength = 3;
	      break;
	    }

	    // 11110XXX
	    if (i <= 3 && c >> 3 == 0x1E) {
	      this.charLength = 4;
	      break;
	    }
	  }
	  this.charReceived = i;
	};

	StringDecoder.prototype.end = function(buffer) {
	  var res = '';
	  if (buffer && buffer.length)
	    res = this.write(buffer);

	  if (this.charReceived) {
	    var cr = this.charReceived;
	    var buf = this.charBuffer;
	    var enc = this.encoding;
	    res += buf.slice(0, cr).toString(enc);
	  }

	  return res;
	};

	function passThroughWrite(buffer) {
	  return buffer.toString(this.encoding);
	}

	function utf16DetectIncompleteChar(buffer) {
	  this.charReceived = buffer.length % 2;
	  this.charLength = this.charReceived ? 2 : 0;
	}

	function base64DetectIncompleteChar(buffer) {
	  this.charReceived = buffer.length % 3;
	  this.charLength = this.charReceived ? 3 : 0;
	}

	},{"buffer":44}],96:[function(require,module,exports){
	(function (global){

	/**
	 * Module exports.
	 */

	module.exports = deprecate;

	/**
	 * Mark that a method should not be used.
	 * Returns a modified function which warns once by default.
	 *
	 * If `localStorage.noDeprecation = true` is set, then it is a no-op.
	 *
	 * If `localStorage.throwDeprecation = true` is set, then deprecated functions
	 * will throw an Error when invoked.
	 *
	 * If `localStorage.traceDeprecation = true` is set, then deprecated functions
	 * will invoke `console.trace()` instead of `console.error()`.
	 *
	 * @param {Function} fn - the function to deprecate
	 * @param {String} msg - the string to print to the console when `fn` is invoked
	 * @returns {Function} a new "deprecated" version of `fn`
	 * @api public
	 */

	function deprecate (fn, msg) {
	  if (config('noDeprecation')) {
	    return fn;
	  }

	  var warned = false;
	  function deprecated() {
	    if (!warned) {
	      if (config('throwDeprecation')) {
	        throw new Error(msg);
	      } else if (config('traceDeprecation')) {
	        console.trace(msg);
	      } else {
	        console.warn(msg);
	      }
	      warned = true;
	    }
	    return fn.apply(this, arguments);
	  }

	  return deprecated;
	}

	/**
	 * Checks `localStorage` for boolean values for the given `name`.
	 *
	 * @param {String} name
	 * @returns {Boolean}
	 * @api private
	 */

	function config (name) {
	  // accessing global.localStorage can trigger a DOMException in sandboxed iframes
	  try {
	    if (!global.localStorage) return false;
	  } catch (_) {
	    return false;
	  }
	  var val = global.localStorage[name];
	  if (null == val) return false;
	  return String(val).toLowerCase() === 'true';
	}

	}).call(this,typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{}],97:[function(require,module,exports){
	arguments[4][66][0].apply(exports,arguments)
	},{"dup":66}],98:[function(require,module,exports){
	module.exports = function isBuffer(arg) {
	  return arg && typeof arg === 'object'
	    && typeof arg.copy === 'function'
	    && typeof arg.fill === 'function'
	    && typeof arg.readUInt8 === 'function';
	}
	},{}],99:[function(require,module,exports){
	(function (process,global){
	// Copyright Joyent, Inc. and other Node contributors.
	//
	// Permission is hereby granted, free of charge, to any person obtaining a
	// copy of this software and associated documentation files (the
	// "Software"), to deal in the Software without restriction, including
	// without limitation the rights to use, copy, modify, merge, publish,
	// distribute, sublicense, and/or sell copies of the Software, and to permit
	// persons to whom the Software is furnished to do so, subject to the
	// following conditions:
	//
	// The above copyright notice and this permission notice shall be included
	// in all copies or substantial portions of the Software.
	//
	// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
	// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
	// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
	// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
	// USE OR OTHER DEALINGS IN THE SOFTWARE.

	var formatRegExp = /%[sdj%]/g;
	exports.format = function(f) {
	  if (!isString(f)) {
	    var objects = [];
	    for (var i = 0; i < arguments.length; i++) {
	      objects.push(inspect(arguments[i]));
	    }
	    return objects.join(' ');
	  }

	  var i = 1;
	  var args = arguments;
	  var len = args.length;
	  var str = String(f).replace(formatRegExp, function(x) {
	    if (x === '%%') return '%';
	    if (i >= len) return x;
	    switch (x) {
	      case '%s': return String(args[i++]);
	      case '%d': return Number(args[i++]);
	      case '%j':
	        try {
	          return JSON.stringify(args[i++]);
	        } catch (_) {
	          return '[Circular]';
	        }
	      default:
	        return x;
	    }
	  });
	  for (var x = args[i]; i < len; x = args[++i]) {
	    if (isNull(x) || !isObject(x)) {
	      str += ' ' + x;
	    } else {
	      str += ' ' + inspect(x);
	    }
	  }
	  return str;
	};


	// Mark that a method should not be used.
	// Returns a modified function which warns once by default.
	// If --no-deprecation is set, then it is a no-op.
	exports.deprecate = function(fn, msg) {
	  // Allow for deprecating things in the process of starting up.
	  if (isUndefined(global.process)) {
	    return function() {
	      return exports.deprecate(fn, msg).apply(this, arguments);
	    };
	  }

	  if (process.noDeprecation === true) {
	    return fn;
	  }

	  var warned = false;
	  function deprecated() {
	    if (!warned) {
	      if (process.throwDeprecation) {
	        throw new Error(msg);
	      } else if (process.traceDeprecation) {
	        console.trace(msg);
	      } else {
	        console.error(msg);
	      }
	      warned = true;
	    }
	    return fn.apply(this, arguments);
	  }

	  return deprecated;
	};


	var debugs = {};
	var debugEnviron;
	exports.debuglog = function(set) {
	  if (isUndefined(debugEnviron))
	    debugEnviron = process.env.NODE_DEBUG || '';
	  set = set.toUpperCase();
	  if (!debugs[set]) {
	    if (new RegExp('\\b' + set + '\\b', 'i').test(debugEnviron)) {
	      var pid = process.pid;
	      debugs[set] = function() {
	        var msg = exports.format.apply(exports, arguments);
	        console.error('%s %d: %s', set, pid, msg);
	      };
	    } else {
	      debugs[set] = function() {};
	    }
	  }
	  return debugs[set];
	};


	/**
	 * Echos the value of a value. Trys to print the value out
	 * in the best way possible given the different types.
	 *
	 * @param {Object} obj The object to print out.
	 * @param {Object} opts Optional options object that alters the output.
	 */
	/* legacy: obj, showHidden, depth, colors*/
	function inspect(obj, opts) {
	  // default options
	  var ctx = {
	    seen: [],
	    stylize: stylizeNoColor
	  };
	  // legacy...
	  if (arguments.length >= 3) ctx.depth = arguments[2];
	  if (arguments.length >= 4) ctx.colors = arguments[3];
	  if (isBoolean(opts)) {
	    // legacy...
	    ctx.showHidden = opts;
	  } else if (opts) {
	    // got an "options" object
	    exports._extend(ctx, opts);
	  }
	  // set default options
	  if (isUndefined(ctx.showHidden)) ctx.showHidden = false;
	  if (isUndefined(ctx.depth)) ctx.depth = 2;
	  if (isUndefined(ctx.colors)) ctx.colors = false;
	  if (isUndefined(ctx.customInspect)) ctx.customInspect = true;
	  if (ctx.colors) ctx.stylize = stylizeWithColor;
	  return formatValue(ctx, obj, ctx.depth);
	}
	exports.inspect = inspect;


	// http://en.wikipedia.org/wiki/ANSI_escape_code#graphics
	inspect.colors = {
	  'bold' : [1, 22],
	  'italic' : [3, 23],
	  'underline' : [4, 24],
	  'inverse' : [7, 27],
	  'white' : [37, 39],
	  'grey' : [90, 39],
	  'black' : [30, 39],
	  'blue' : [34, 39],
	  'cyan' : [36, 39],
	  'green' : [32, 39],
	  'magenta' : [35, 39],
	  'red' : [31, 39],
	  'yellow' : [33, 39]
	};

	// Don't use 'blue' not visible on cmd.exe
	inspect.styles = {
	  'special': 'cyan',
	  'number': 'yellow',
	  'boolean': 'yellow',
	  'undefined': 'grey',
	  'null': 'bold',
	  'string': 'green',
	  'date': 'magenta',
	  // "name": intentionally not styling
	  'regexp': 'red'
	};


	function stylizeWithColor(str, styleType) {
	  var style = inspect.styles[styleType];

	  if (style) {
	    return '\u001b[' + inspect.colors[style][0] + 'm' + str +
	           '\u001b[' + inspect.colors[style][1] + 'm';
	  } else {
	    return str;
	  }
	}


	function stylizeNoColor(str, styleType) {
	  return str;
	}


	function arrayToHash(array) {
	  var hash = {};

	  array.forEach(function(val, idx) {
	    hash[val] = true;
	  });

	  return hash;
	}


	function formatValue(ctx, value, recurseTimes) {
	  // Provide a hook for user-specified inspect functions.
	  // Check that value is an object with an inspect function on it
	  if (ctx.customInspect &&
	      value &&
	      isFunction(value.inspect) &&
	      // Filter out the util module, it's inspect function is special
	      value.inspect !== exports.inspect &&
	      // Also filter out any prototype objects using the circular check.
	      !(value.constructor && value.constructor.prototype === value)) {
	    var ret = value.inspect(recurseTimes, ctx);
	    if (!isString(ret)) {
	      ret = formatValue(ctx, ret, recurseTimes);
	    }
	    return ret;
	  }

	  // Primitive types cannot have properties
	  var primitive = formatPrimitive(ctx, value);
	  if (primitive) {
	    return primitive;
	  }

	  // Look up the keys of the object.
	  var keys = Object.keys(value);
	  var visibleKeys = arrayToHash(keys);

	  if (ctx.showHidden) {
	    keys = Object.getOwnPropertyNames(value);
	  }

	  // IE doesn't make error fields non-enumerable
	  // http://msdn.microsoft.com/en-us/library/ie/dww52sbt(v=vs.94).aspx
	  if (isError(value)
	      && (keys.indexOf('message') >= 0 || keys.indexOf('description') >= 0)) {
	    return formatError(value);
	  }

	  // Some type of object without properties can be shortcutted.
	  if (keys.length === 0) {
	    if (isFunction(value)) {
	      var name = value.name ? ': ' + value.name : '';
	      return ctx.stylize('[Function' + name + ']', 'special');
	    }
	    if (isRegExp(value)) {
	      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
	    }
	    if (isDate(value)) {
	      return ctx.stylize(Date.prototype.toString.call(value), 'date');
	    }
	    if (isError(value)) {
	      return formatError(value);
	    }
	  }

	  var base = '', array = false, braces = ['{', '}'];

	  // Make Array say that they are Array
	  if (isArray(value)) {
	    array = true;
	    braces = ['[', ']'];
	  }

	  // Make functions say that they are functions
	  if (isFunction(value)) {
	    var n = value.name ? ': ' + value.name : '';
	    base = ' [Function' + n + ']';
	  }

	  // Make RegExps say that they are RegExps
	  if (isRegExp(value)) {
	    base = ' ' + RegExp.prototype.toString.call(value);
	  }

	  // Make dates with properties first say the date
	  if (isDate(value)) {
	    base = ' ' + Date.prototype.toUTCString.call(value);
	  }

	  // Make error with message first say the error
	  if (isError(value)) {
	    base = ' ' + formatError(value);
	  }

	  if (keys.length === 0 && (!array || value.length == 0)) {
	    return braces[0] + base + braces[1];
	  }

	  if (recurseTimes < 0) {
	    if (isRegExp(value)) {
	      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
	    } else {
	      return ctx.stylize('[Object]', 'special');
	    }
	  }

	  ctx.seen.push(value);

	  var output;
	  if (array) {
	    output = formatArray(ctx, value, recurseTimes, visibleKeys, keys);
	  } else {
	    output = keys.map(function(key) {
	      return formatProperty(ctx, value, recurseTimes, visibleKeys, key, array);
	    });
	  }

	  ctx.seen.pop();

	  return reduceToSingleString(output, base, braces);
	}


	function formatPrimitive(ctx, value) {
	  if (isUndefined(value))
	    return ctx.stylize('undefined', 'undefined');
	  if (isString(value)) {
	    var simple = '\'' + JSON.stringify(value).replace(/^"|"$/g, '')
	                                             .replace(/'/g, "\\'")
	                                             .replace(/\\"/g, '"') + '\'';
	    return ctx.stylize(simple, 'string');
	  }
	  if (isNumber(value))
	    return ctx.stylize('' + value, 'number');
	  if (isBoolean(value))
	    return ctx.stylize('' + value, 'boolean');
	  // For some reason typeof null is "object", so special case here.
	  if (isNull(value))
	    return ctx.stylize('null', 'null');
	}


	function formatError(value) {
	  return '[' + Error.prototype.toString.call(value) + ']';
	}


	function formatArray(ctx, value, recurseTimes, visibleKeys, keys) {
	  var output = [];
	  for (var i = 0, l = value.length; i < l; ++i) {
	    if (hasOwnProperty(value, String(i))) {
	      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
	          String(i), true));
	    } else {
	      output.push('');
	    }
	  }
	  keys.forEach(function(key) {
	    if (!key.match(/^\d+$/)) {
	      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
	          key, true));
	    }
	  });
	  return output;
	}


	function formatProperty(ctx, value, recurseTimes, visibleKeys, key, array) {
	  var name, str, desc;
	  desc = Object.getOwnPropertyDescriptor(value, key) || { value: value[key] };
	  if (desc.get) {
	    if (desc.set) {
	      str = ctx.stylize('[Getter/Setter]', 'special');
	    } else {
	      str = ctx.stylize('[Getter]', 'special');
	    }
	  } else {
	    if (desc.set) {
	      str = ctx.stylize('[Setter]', 'special');
	    }
	  }
	  if (!hasOwnProperty(visibleKeys, key)) {
	    name = '[' + key + ']';
	  }
	  if (!str) {
	    if (ctx.seen.indexOf(desc.value) < 0) {
	      if (isNull(recurseTimes)) {
	        str = formatValue(ctx, desc.value, null);
	      } else {
	        str = formatValue(ctx, desc.value, recurseTimes - 1);
	      }
	      if (str.indexOf('\n') > -1) {
	        if (array) {
	          str = str.split('\n').map(function(line) {
	            return '  ' + line;
	          }).join('\n').substr(2);
	        } else {
	          str = '\n' + str.split('\n').map(function(line) {
	            return '   ' + line;
	          }).join('\n');
	        }
	      }
	    } else {
	      str = ctx.stylize('[Circular]', 'special');
	    }
	  }
	  if (isUndefined(name)) {
	    if (array && key.match(/^\d+$/)) {
	      return str;
	    }
	    name = JSON.stringify('' + key);
	    if (name.match(/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/)) {
	      name = name.substr(1, name.length - 2);
	      name = ctx.stylize(name, 'name');
	    } else {
	      name = name.replace(/'/g, "\\'")
	                 .replace(/\\"/g, '"')
	                 .replace(/(^"|"$)/g, "'");
	      name = ctx.stylize(name, 'string');
	    }
	  }

	  return name + ': ' + str;
	}


	function reduceToSingleString(output, base, braces) {
	  var numLinesEst = 0;
	  var length = output.reduce(function(prev, cur) {
	    numLinesEst++;
	    if (cur.indexOf('\n') >= 0) numLinesEst++;
	    return prev + cur.replace(/\u001b\[\d\d?m/g, '').length + 1;
	  }, 0);

	  if (length > 60) {
	    return braces[0] +
	           (base === '' ? '' : base + '\n ') +
	           ' ' +
	           output.join(',\n  ') +
	           ' ' +
	           braces[1];
	  }

	  return braces[0] + base + ' ' + output.join(', ') + ' ' + braces[1];
	}


	// NOTE: These type checking functions intentionally don't use `instanceof`
	// because it is fragile and can be easily faked with `Object.create()`.
	function isArray(ar) {
	  return Array.isArray(ar);
	}
	exports.isArray = isArray;

	function isBoolean(arg) {
	  return typeof arg === 'boolean';
	}
	exports.isBoolean = isBoolean;

	function isNull(arg) {
	  return arg === null;
	}
	exports.isNull = isNull;

	function isNullOrUndefined(arg) {
	  return arg == null;
	}
	exports.isNullOrUndefined = isNullOrUndefined;

	function isNumber(arg) {
	  return typeof arg === 'number';
	}
	exports.isNumber = isNumber;

	function isString(arg) {
	  return typeof arg === 'string';
	}
	exports.isString = isString;

	function isSymbol(arg) {
	  return typeof arg === 'symbol';
	}
	exports.isSymbol = isSymbol;

	function isUndefined(arg) {
	  return arg === void 0;
	}
	exports.isUndefined = isUndefined;

	function isRegExp(re) {
	  return isObject(re) && objectToString(re) === '[object RegExp]';
	}
	exports.isRegExp = isRegExp;

	function isObject(arg) {
	  return typeof arg === 'object' && arg !== null;
	}
	exports.isObject = isObject;

	function isDate(d) {
	  return isObject(d) && objectToString(d) === '[object Date]';
	}
	exports.isDate = isDate;

	function isError(e) {
	  return isObject(e) &&
	      (objectToString(e) === '[object Error]' || e instanceof Error);
	}
	exports.isError = isError;

	function isFunction(arg) {
	  return typeof arg === 'function';
	}
	exports.isFunction = isFunction;

	function isPrimitive(arg) {
	  return arg === null ||
	         typeof arg === 'boolean' ||
	         typeof arg === 'number' ||
	         typeof arg === 'string' ||
	         typeof arg === 'symbol' ||  // ES6 symbol
	         typeof arg === 'undefined';
	}
	exports.isPrimitive = isPrimitive;

	exports.isBuffer = require('./support/isBuffer');

	function objectToString(o) {
	  return Object.prototype.toString.call(o);
	}


	function pad(n) {
	  return n < 10 ? '0' + n.toString(10) : n.toString(10);
	}


	var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep',
	              'Oct', 'Nov', 'Dec'];

	// 26 Feb 16:19:34
	function timestamp() {
	  var d = new Date();
	  var time = [pad(d.getHours()),
	              pad(d.getMinutes()),
	              pad(d.getSeconds())].join(':');
	  return [d.getDate(), months[d.getMonth()], time].join(' ');
	}


	// log is just a thin wrapper to console.log that prepends a timestamp
	exports.log = function() {
	  console.log('%s - %s', timestamp(), exports.format.apply(exports, arguments));
	};


	/**
	 * Inherit the prototype methods from one constructor into another.
	 *
	 * The Function.prototype.inherits from lang.js rewritten as a standalone
	 * function (not on Function.prototype). NOTE: If this file is to be loaded
	 * during bootstrapping this function needs to be rewritten using some native
	 * functions as prototype setup using normal JavaScript does not work as
	 * expected during bootstrapping (see mirror.js in r114903).
	 *
	 * @param {function} ctor Constructor function which needs to inherit the
	 *     prototype.
	 * @param {function} superCtor Constructor function to inherit prototype from.
	 */
	exports.inherits = require('inherits');

	exports._extend = function(origin, add) {
	  // Don't do anything if add isn't an object
	  if (!add || !isObject(add)) return origin;

	  var keys = Object.keys(add);
	  var i = keys.length;
	  while (i--) {
	    origin[keys[i]] = add[keys[i]];
	  }
	  return origin;
	};

	function hasOwnProperty(obj, prop) {
	  return Object.prototype.hasOwnProperty.call(obj, prop);
	}

	}).call(this,require('_process'),typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
	},{"./support/isBuffer":98,"_process":82,"inherits":97}]},{},[1]);


/***/ },

/***/ 794:
/***/ function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(global) {/*!
	 * The buffer module from node.js, for the browser.
	 *
	 * @author   Feross Aboukhadijeh <feross@feross.org> <http://feross.org>
	 * @license  MIT
	 */
	/* eslint-disable no-proto */

	'use strict'

	var base64 = __webpack_require__(795)
	var ieee754 = __webpack_require__(796)
	var isArray = __webpack_require__(797)

	exports.Buffer = Buffer
	exports.SlowBuffer = SlowBuffer
	exports.INSPECT_MAX_BYTES = 50

	/**
	 * If `Buffer.TYPED_ARRAY_SUPPORT`:
	 *   === true    Use Uint8Array implementation (fastest)
	 *   === false   Use Object implementation (most compatible, even IE6)
	 *
	 * Browsers that support typed arrays are IE 10+, Firefox 4+, Chrome 7+, Safari 5.1+,
	 * Opera 11.6+, iOS 4.2+.
	 *
	 * Due to various browser bugs, sometimes the Object implementation will be used even
	 * when the browser supports typed arrays.
	 *
	 * Note:
	 *
	 *   - Firefox 4-29 lacks support for adding new properties to `Uint8Array` instances,
	 *     See: https://bugzilla.mozilla.org/show_bug.cgi?id=695438.
	 *
	 *   - Chrome 9-10 is missing the `TypedArray.prototype.subarray` function.
	 *
	 *   - IE10 has a broken `TypedArray.prototype.subarray` function which returns arrays of
	 *     incorrect length in some situations.

	 * We detect these buggy browsers and set `Buffer.TYPED_ARRAY_SUPPORT` to `false` so they
	 * get the Object implementation, which is slower but behaves correctly.
	 */
	Buffer.TYPED_ARRAY_SUPPORT = global.TYPED_ARRAY_SUPPORT !== undefined
	  ? global.TYPED_ARRAY_SUPPORT
	  : typedArraySupport()

	/*
	 * Export kMaxLength after typed array support is determined.
	 */
	exports.kMaxLength = kMaxLength()

	function typedArraySupport () {
	  try {
	    var arr = new Uint8Array(1)
	    arr.__proto__ = {__proto__: Uint8Array.prototype, foo: function () { return 42 }}
	    return arr.foo() === 42 && // typed array instances can be augmented
	        typeof arr.subarray === 'function' && // chrome 9-10 lack `subarray`
	        arr.subarray(1, 1).byteLength === 0 // ie10 has broken `subarray`
	  } catch (e) {
	    return false
	  }
	}

	function kMaxLength () {
	  return Buffer.TYPED_ARRAY_SUPPORT
	    ? 0x7fffffff
	    : 0x3fffffff
	}

	function createBuffer (that, length) {
	  if (kMaxLength() < length) {
	    throw new RangeError('Invalid typed array length')
	  }
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    // Return an augmented `Uint8Array` instance, for best performance
	    that = new Uint8Array(length)
	    that.__proto__ = Buffer.prototype
	  } else {
	    // Fallback: Return an object instance of the Buffer class
	    if (that === null) {
	      that = new Buffer(length)
	    }
	    that.length = length
	  }

	  return that
	}

	/**
	 * The Buffer constructor returns instances of `Uint8Array` that have their
	 * prototype changed to `Buffer.prototype`. Furthermore, `Buffer` is a subclass of
	 * `Uint8Array`, so the returned instances will have all the node `Buffer` methods
	 * and the `Uint8Array` methods. Square bracket notation works as expected -- it
	 * returns a single octet.
	 *
	 * The `Uint8Array` prototype remains unmodified.
	 */

	function Buffer (arg, encodingOrOffset, length) {
	  if (!Buffer.TYPED_ARRAY_SUPPORT && !(this instanceof Buffer)) {
	    return new Buffer(arg, encodingOrOffset, length)
	  }

	  // Common case.
	  if (typeof arg === 'number') {
	    if (typeof encodingOrOffset === 'string') {
	      throw new Error(
	        'If encoding is specified then the first argument must be a string'
	      )
	    }
	    return allocUnsafe(this, arg)
	  }
	  return from(this, arg, encodingOrOffset, length)
	}

	Buffer.poolSize = 8192 // not used by this implementation

	// TODO: Legacy, not needed anymore. Remove in next major version.
	Buffer._augment = function (arr) {
	  arr.__proto__ = Buffer.prototype
	  return arr
	}

	function from (that, value, encodingOrOffset, length) {
	  if (typeof value === 'number') {
	    throw new TypeError('"value" argument must not be a number')
	  }

	  if (typeof ArrayBuffer !== 'undefined' && value instanceof ArrayBuffer) {
	    return fromArrayBuffer(that, value, encodingOrOffset, length)
	  }

	  if (typeof value === 'string') {
	    return fromString(that, value, encodingOrOffset)
	  }

	  return fromObject(that, value)
	}

	/**
	 * Functionally equivalent to Buffer(arg, encoding) but throws a TypeError
	 * if value is a number.
	 * Buffer.from(str[, encoding])
	 * Buffer.from(array)
	 * Buffer.from(buffer)
	 * Buffer.from(arrayBuffer[, byteOffset[, length]])
	 **/
	Buffer.from = function (value, encodingOrOffset, length) {
	  return from(null, value, encodingOrOffset, length)
	}

	if (Buffer.TYPED_ARRAY_SUPPORT) {
	  Buffer.prototype.__proto__ = Uint8Array.prototype
	  Buffer.__proto__ = Uint8Array
	  if (typeof Symbol !== 'undefined' && Symbol.species &&
	      Buffer[Symbol.species] === Buffer) {
	    // Fix subarray() in ES2016. See: https://github.com/feross/buffer/pull/97
	    Object.defineProperty(Buffer, Symbol.species, {
	      value: null,
	      configurable: true
	    })
	  }
	}

	function assertSize (size) {
	  if (typeof size !== 'number') {
	    throw new TypeError('"size" argument must be a number')
	  } else if (size < 0) {
	    throw new RangeError('"size" argument must not be negative')
	  }
	}

	function alloc (that, size, fill, encoding) {
	  assertSize(size)
	  if (size <= 0) {
	    return createBuffer(that, size)
	  }
	  if (fill !== undefined) {
	    // Only pay attention to encoding if it's a string. This
	    // prevents accidentally sending in a number that would
	    // be interpretted as a start offset.
	    return typeof encoding === 'string'
	      ? createBuffer(that, size).fill(fill, encoding)
	      : createBuffer(that, size).fill(fill)
	  }
	  return createBuffer(that, size)
	}

	/**
	 * Creates a new filled Buffer instance.
	 * alloc(size[, fill[, encoding]])
	 **/
	Buffer.alloc = function (size, fill, encoding) {
	  return alloc(null, size, fill, encoding)
	}

	function allocUnsafe (that, size) {
	  assertSize(size)
	  that = createBuffer(that, size < 0 ? 0 : checked(size) | 0)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) {
	    for (var i = 0; i < size; ++i) {
	      that[i] = 0
	    }
	  }
	  return that
	}

	/**
	 * Equivalent to Buffer(num), by default creates a non-zero-filled Buffer instance.
	 * */
	Buffer.allocUnsafe = function (size) {
	  return allocUnsafe(null, size)
	}
	/**
	 * Equivalent to SlowBuffer(num), by default creates a non-zero-filled Buffer instance.
	 */
	Buffer.allocUnsafeSlow = function (size) {
	  return allocUnsafe(null, size)
	}

	function fromString (that, string, encoding) {
	  if (typeof encoding !== 'string' || encoding === '') {
	    encoding = 'utf8'
	  }

	  if (!Buffer.isEncoding(encoding)) {
	    throw new TypeError('"encoding" must be a valid string encoding')
	  }

	  var length = byteLength(string, encoding) | 0
	  that = createBuffer(that, length)

	  var actual = that.write(string, encoding)

	  if (actual !== length) {
	    // Writing a hex string, for example, that contains invalid characters will
	    // cause everything after the first invalid character to be ignored. (e.g.
	    // 'abxxcd' will be treated as 'ab')
	    that = that.slice(0, actual)
	  }

	  return that
	}

	function fromArrayLike (that, array) {
	  var length = array.length < 0 ? 0 : checked(array.length) | 0
	  that = createBuffer(that, length)
	  for (var i = 0; i < length; i += 1) {
	    that[i] = array[i] & 255
	  }
	  return that
	}

	function fromArrayBuffer (that, array, byteOffset, length) {
	  array.byteLength // this throws if `array` is not a valid ArrayBuffer

	  if (byteOffset < 0 || array.byteLength < byteOffset) {
	    throw new RangeError('\'offset\' is out of bounds')
	  }

	  if (array.byteLength < byteOffset + (length || 0)) {
	    throw new RangeError('\'length\' is out of bounds')
	  }

	  if (byteOffset === undefined && length === undefined) {
	    array = new Uint8Array(array)
	  } else if (length === undefined) {
	    array = new Uint8Array(array, byteOffset)
	  } else {
	    array = new Uint8Array(array, byteOffset, length)
	  }

	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    // Return an augmented `Uint8Array` instance, for best performance
	    that = array
	    that.__proto__ = Buffer.prototype
	  } else {
	    // Fallback: Return an object instance of the Buffer class
	    that = fromArrayLike(that, array)
	  }
	  return that
	}

	function fromObject (that, obj) {
	  if (Buffer.isBuffer(obj)) {
	    var len = checked(obj.length) | 0
	    that = createBuffer(that, len)

	    if (that.length === 0) {
	      return that
	    }

	    obj.copy(that, 0, 0, len)
	    return that
	  }

	  if (obj) {
	    if ((typeof ArrayBuffer !== 'undefined' &&
	        obj.buffer instanceof ArrayBuffer) || 'length' in obj) {
	      if (typeof obj.length !== 'number' || isnan(obj.length)) {
	        return createBuffer(that, 0)
	      }
	      return fromArrayLike(that, obj)
	    }

	    if (obj.type === 'Buffer' && isArray(obj.data)) {
	      return fromArrayLike(that, obj.data)
	    }
	  }

	  throw new TypeError('First argument must be a string, Buffer, ArrayBuffer, Array, or array-like object.')
	}

	function checked (length) {
	  // Note: cannot use `length < kMaxLength()` here because that fails when
	  // length is NaN (which is otherwise coerced to zero.)
	  if (length >= kMaxLength()) {
	    throw new RangeError('Attempt to allocate Buffer larger than maximum ' +
	                         'size: 0x' + kMaxLength().toString(16) + ' bytes')
	  }
	  return length | 0
	}

	function SlowBuffer (length) {
	  if (+length != length) { // eslint-disable-line eqeqeq
	    length = 0
	  }
	  return Buffer.alloc(+length)
	}

	Buffer.isBuffer = function isBuffer (b) {
	  return !!(b != null && b._isBuffer)
	}

	Buffer.compare = function compare (a, b) {
	  if (!Buffer.isBuffer(a) || !Buffer.isBuffer(b)) {
	    throw new TypeError('Arguments must be Buffers')
	  }

	  if (a === b) return 0

	  var x = a.length
	  var y = b.length

	  for (var i = 0, len = Math.min(x, y); i < len; ++i) {
	    if (a[i] !== b[i]) {
	      x = a[i]
	      y = b[i]
	      break
	    }
	  }

	  if (x < y) return -1
	  if (y < x) return 1
	  return 0
	}

	Buffer.isEncoding = function isEncoding (encoding) {
	  switch (String(encoding).toLowerCase()) {
	    case 'hex':
	    case 'utf8':
	    case 'utf-8':
	    case 'ascii':
	    case 'latin1':
	    case 'binary':
	    case 'base64':
	    case 'ucs2':
	    case 'ucs-2':
	    case 'utf16le':
	    case 'utf-16le':
	      return true
	    default:
	      return false
	  }
	}

	Buffer.concat = function concat (list, length) {
	  if (!isArray(list)) {
	    throw new TypeError('"list" argument must be an Array of Buffers')
	  }

	  if (list.length === 0) {
	    return Buffer.alloc(0)
	  }

	  var i
	  if (length === undefined) {
	    length = 0
	    for (i = 0; i < list.length; ++i) {
	      length += list[i].length
	    }
	  }

	  var buffer = Buffer.allocUnsafe(length)
	  var pos = 0
	  for (i = 0; i < list.length; ++i) {
	    var buf = list[i]
	    if (!Buffer.isBuffer(buf)) {
	      throw new TypeError('"list" argument must be an Array of Buffers')
	    }
	    buf.copy(buffer, pos)
	    pos += buf.length
	  }
	  return buffer
	}

	function byteLength (string, encoding) {
	  if (Buffer.isBuffer(string)) {
	    return string.length
	  }
	  if (typeof ArrayBuffer !== 'undefined' && typeof ArrayBuffer.isView === 'function' &&
	      (ArrayBuffer.isView(string) || string instanceof ArrayBuffer)) {
	    return string.byteLength
	  }
	  if (typeof string !== 'string') {
	    string = '' + string
	  }

	  var len = string.length
	  if (len === 0) return 0

	  // Use a for loop to avoid recursion
	  var loweredCase = false
	  for (;;) {
	    switch (encoding) {
	      case 'ascii':
	      case 'latin1':
	      case 'binary':
	        return len
	      case 'utf8':
	      case 'utf-8':
	      case undefined:
	        return utf8ToBytes(string).length
	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return len * 2
	      case 'hex':
	        return len >>> 1
	      case 'base64':
	        return base64ToBytes(string).length
	      default:
	        if (loweredCase) return utf8ToBytes(string).length // assume utf8
	        encoding = ('' + encoding).toLowerCase()
	        loweredCase = true
	    }
	  }
	}
	Buffer.byteLength = byteLength

	function slowToString (encoding, start, end) {
	  var loweredCase = false

	  // No need to verify that "this.length <= MAX_UINT32" since it's a read-only
	  // property of a typed array.

	  // This behaves neither like String nor Uint8Array in that we set start/end
	  // to their upper/lower bounds if the value passed is out of range.
	  // undefined is handled specially as per ECMA-262 6th Edition,
	  // Section 13.3.3.7 Runtime Semantics: KeyedBindingInitialization.
	  if (start === undefined || start < 0) {
	    start = 0
	  }
	  // Return early if start > this.length. Done here to prevent potential uint32
	  // coercion fail below.
	  if (start > this.length) {
	    return ''
	  }

	  if (end === undefined || end > this.length) {
	    end = this.length
	  }

	  if (end <= 0) {
	    return ''
	  }

	  // Force coersion to uint32. This will also coerce falsey/NaN values to 0.
	  end >>>= 0
	  start >>>= 0

	  if (end <= start) {
	    return ''
	  }

	  if (!encoding) encoding = 'utf8'

	  while (true) {
	    switch (encoding) {
	      case 'hex':
	        return hexSlice(this, start, end)

	      case 'utf8':
	      case 'utf-8':
	        return utf8Slice(this, start, end)

	      case 'ascii':
	        return asciiSlice(this, start, end)

	      case 'latin1':
	      case 'binary':
	        return latin1Slice(this, start, end)

	      case 'base64':
	        return base64Slice(this, start, end)

	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return utf16leSlice(this, start, end)

	      default:
	        if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
	        encoding = (encoding + '').toLowerCase()
	        loweredCase = true
	    }
	  }
	}

	// The property is used by `Buffer.isBuffer` and `is-buffer` (in Safari 5-7) to detect
	// Buffer instances.
	Buffer.prototype._isBuffer = true

	function swap (b, n, m) {
	  var i = b[n]
	  b[n] = b[m]
	  b[m] = i
	}

	Buffer.prototype.swap16 = function swap16 () {
	  var len = this.length
	  if (len % 2 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 16-bits')
	  }
	  for (var i = 0; i < len; i += 2) {
	    swap(this, i, i + 1)
	  }
	  return this
	}

	Buffer.prototype.swap32 = function swap32 () {
	  var len = this.length
	  if (len % 4 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 32-bits')
	  }
	  for (var i = 0; i < len; i += 4) {
	    swap(this, i, i + 3)
	    swap(this, i + 1, i + 2)
	  }
	  return this
	}

	Buffer.prototype.swap64 = function swap64 () {
	  var len = this.length
	  if (len % 8 !== 0) {
	    throw new RangeError('Buffer size must be a multiple of 64-bits')
	  }
	  for (var i = 0; i < len; i += 8) {
	    swap(this, i, i + 7)
	    swap(this, i + 1, i + 6)
	    swap(this, i + 2, i + 5)
	    swap(this, i + 3, i + 4)
	  }
	  return this
	}

	Buffer.prototype.toString = function toString () {
	  var length = this.length | 0
	  if (length === 0) return ''
	  if (arguments.length === 0) return utf8Slice(this, 0, length)
	  return slowToString.apply(this, arguments)
	}

	Buffer.prototype.equals = function equals (b) {
	  if (!Buffer.isBuffer(b)) throw new TypeError('Argument must be a Buffer')
	  if (this === b) return true
	  return Buffer.compare(this, b) === 0
	}

	Buffer.prototype.inspect = function inspect () {
	  var str = ''
	  var max = exports.INSPECT_MAX_BYTES
	  if (this.length > 0) {
	    str = this.toString('hex', 0, max).match(/.{2}/g).join(' ')
	    if (this.length > max) str += ' ... '
	  }
	  return '<Buffer ' + str + '>'
	}

	Buffer.prototype.compare = function compare (target, start, end, thisStart, thisEnd) {
	  if (!Buffer.isBuffer(target)) {
	    throw new TypeError('Argument must be a Buffer')
	  }

	  if (start === undefined) {
	    start = 0
	  }
	  if (end === undefined) {
	    end = target ? target.length : 0
	  }
	  if (thisStart === undefined) {
	    thisStart = 0
	  }
	  if (thisEnd === undefined) {
	    thisEnd = this.length
	  }

	  if (start < 0 || end > target.length || thisStart < 0 || thisEnd > this.length) {
	    throw new RangeError('out of range index')
	  }

	  if (thisStart >= thisEnd && start >= end) {
	    return 0
	  }
	  if (thisStart >= thisEnd) {
	    return -1
	  }
	  if (start >= end) {
	    return 1
	  }

	  start >>>= 0
	  end >>>= 0
	  thisStart >>>= 0
	  thisEnd >>>= 0

	  if (this === target) return 0

	  var x = thisEnd - thisStart
	  var y = end - start
	  var len = Math.min(x, y)

	  var thisCopy = this.slice(thisStart, thisEnd)
	  var targetCopy = target.slice(start, end)

	  for (var i = 0; i < len; ++i) {
	    if (thisCopy[i] !== targetCopy[i]) {
	      x = thisCopy[i]
	      y = targetCopy[i]
	      break
	    }
	  }

	  if (x < y) return -1
	  if (y < x) return 1
	  return 0
	}

	// Finds either the first index of `val` in `buffer` at offset >= `byteOffset`,
	// OR the last index of `val` in `buffer` at offset <= `byteOffset`.
	//
	// Arguments:
	// - buffer - a Buffer to search
	// - val - a string, Buffer, or number
	// - byteOffset - an index into `buffer`; will be clamped to an int32
	// - encoding - an optional encoding, relevant is val is a string
	// - dir - true for indexOf, false for lastIndexOf
	function bidirectionalIndexOf (buffer, val, byteOffset, encoding, dir) {
	  // Empty buffer means no match
	  if (buffer.length === 0) return -1

	  // Normalize byteOffset
	  if (typeof byteOffset === 'string') {
	    encoding = byteOffset
	    byteOffset = 0
	  } else if (byteOffset > 0x7fffffff) {
	    byteOffset = 0x7fffffff
	  } else if (byteOffset < -0x80000000) {
	    byteOffset = -0x80000000
	  }
	  byteOffset = +byteOffset  // Coerce to Number.
	  if (isNaN(byteOffset)) {
	    // byteOffset: it it's undefined, null, NaN, "foo", etc, search whole buffer
	    byteOffset = dir ? 0 : (buffer.length - 1)
	  }

	  // Normalize byteOffset: negative offsets start from the end of the buffer
	  if (byteOffset < 0) byteOffset = buffer.length + byteOffset
	  if (byteOffset >= buffer.length) {
	    if (dir) return -1
	    else byteOffset = buffer.length - 1
	  } else if (byteOffset < 0) {
	    if (dir) byteOffset = 0
	    else return -1
	  }

	  // Normalize val
	  if (typeof val === 'string') {
	    val = Buffer.from(val, encoding)
	  }

	  // Finally, search either indexOf (if dir is true) or lastIndexOf
	  if (Buffer.isBuffer(val)) {
	    // Special case: looking for empty string/buffer always fails
	    if (val.length === 0) {
	      return -1
	    }
	    return arrayIndexOf(buffer, val, byteOffset, encoding, dir)
	  } else if (typeof val === 'number') {
	    val = val & 0xFF // Search for a byte value [0-255]
	    if (Buffer.TYPED_ARRAY_SUPPORT &&
	        typeof Uint8Array.prototype.indexOf === 'function') {
	      if (dir) {
	        return Uint8Array.prototype.indexOf.call(buffer, val, byteOffset)
	      } else {
	        return Uint8Array.prototype.lastIndexOf.call(buffer, val, byteOffset)
	      }
	    }
	    return arrayIndexOf(buffer, [ val ], byteOffset, encoding, dir)
	  }

	  throw new TypeError('val must be string, number or Buffer')
	}

	function arrayIndexOf (arr, val, byteOffset, encoding, dir) {
	  var indexSize = 1
	  var arrLength = arr.length
	  var valLength = val.length

	  if (encoding !== undefined) {
	    encoding = String(encoding).toLowerCase()
	    if (encoding === 'ucs2' || encoding === 'ucs-2' ||
	        encoding === 'utf16le' || encoding === 'utf-16le') {
	      if (arr.length < 2 || val.length < 2) {
	        return -1
	      }
	      indexSize = 2
	      arrLength /= 2
	      valLength /= 2
	      byteOffset /= 2
	    }
	  }

	  function read (buf, i) {
	    if (indexSize === 1) {
	      return buf[i]
	    } else {
	      return buf.readUInt16BE(i * indexSize)
	    }
	  }

	  var i
	  if (dir) {
	    var foundIndex = -1
	    for (i = byteOffset; i < arrLength; i++) {
	      if (read(arr, i) === read(val, foundIndex === -1 ? 0 : i - foundIndex)) {
	        if (foundIndex === -1) foundIndex = i
	        if (i - foundIndex + 1 === valLength) return foundIndex * indexSize
	      } else {
	        if (foundIndex !== -1) i -= i - foundIndex
	        foundIndex = -1
	      }
	    }
	  } else {
	    if (byteOffset + valLength > arrLength) byteOffset = arrLength - valLength
	    for (i = byteOffset; i >= 0; i--) {
	      var found = true
	      for (var j = 0; j < valLength; j++) {
	        if (read(arr, i + j) !== read(val, j)) {
	          found = false
	          break
	        }
	      }
	      if (found) return i
	    }
	  }

	  return -1
	}

	Buffer.prototype.includes = function includes (val, byteOffset, encoding) {
	  return this.indexOf(val, byteOffset, encoding) !== -1
	}

	Buffer.prototype.indexOf = function indexOf (val, byteOffset, encoding) {
	  return bidirectionalIndexOf(this, val, byteOffset, encoding, true)
	}

	Buffer.prototype.lastIndexOf = function lastIndexOf (val, byteOffset, encoding) {
	  return bidirectionalIndexOf(this, val, byteOffset, encoding, false)
	}

	function hexWrite (buf, string, offset, length) {
	  offset = Number(offset) || 0
	  var remaining = buf.length - offset
	  if (!length) {
	    length = remaining
	  } else {
	    length = Number(length)
	    if (length > remaining) {
	      length = remaining
	    }
	  }

	  // must be an even number of digits
	  var strLen = string.length
	  if (strLen % 2 !== 0) throw new TypeError('Invalid hex string')

	  if (length > strLen / 2) {
	    length = strLen / 2
	  }
	  for (var i = 0; i < length; ++i) {
	    var parsed = parseInt(string.substr(i * 2, 2), 16)
	    if (isNaN(parsed)) return i
	    buf[offset + i] = parsed
	  }
	  return i
	}

	function utf8Write (buf, string, offset, length) {
	  return blitBuffer(utf8ToBytes(string, buf.length - offset), buf, offset, length)
	}

	function asciiWrite (buf, string, offset, length) {
	  return blitBuffer(asciiToBytes(string), buf, offset, length)
	}

	function latin1Write (buf, string, offset, length) {
	  return asciiWrite(buf, string, offset, length)
	}

	function base64Write (buf, string, offset, length) {
	  return blitBuffer(base64ToBytes(string), buf, offset, length)
	}

	function ucs2Write (buf, string, offset, length) {
	  return blitBuffer(utf16leToBytes(string, buf.length - offset), buf, offset, length)
	}

	Buffer.prototype.write = function write (string, offset, length, encoding) {
	  // Buffer#write(string)
	  if (offset === undefined) {
	    encoding = 'utf8'
	    length = this.length
	    offset = 0
	  // Buffer#write(string, encoding)
	  } else if (length === undefined && typeof offset === 'string') {
	    encoding = offset
	    length = this.length
	    offset = 0
	  // Buffer#write(string, offset[, length][, encoding])
	  } else if (isFinite(offset)) {
	    offset = offset | 0
	    if (isFinite(length)) {
	      length = length | 0
	      if (encoding === undefined) encoding = 'utf8'
	    } else {
	      encoding = length
	      length = undefined
	    }
	  // legacy write(string, encoding, offset, length) - remove in v0.13
	  } else {
	    throw new Error(
	      'Buffer.write(string, encoding, offset[, length]) is no longer supported'
	    )
	  }

	  var remaining = this.length - offset
	  if (length === undefined || length > remaining) length = remaining

	  if ((string.length > 0 && (length < 0 || offset < 0)) || offset > this.length) {
	    throw new RangeError('Attempt to write outside buffer bounds')
	  }

	  if (!encoding) encoding = 'utf8'

	  var loweredCase = false
	  for (;;) {
	    switch (encoding) {
	      case 'hex':
	        return hexWrite(this, string, offset, length)

	      case 'utf8':
	      case 'utf-8':
	        return utf8Write(this, string, offset, length)

	      case 'ascii':
	        return asciiWrite(this, string, offset, length)

	      case 'latin1':
	      case 'binary':
	        return latin1Write(this, string, offset, length)

	      case 'base64':
	        // Warning: maxLength not taken into account in base64Write
	        return base64Write(this, string, offset, length)

	      case 'ucs2':
	      case 'ucs-2':
	      case 'utf16le':
	      case 'utf-16le':
	        return ucs2Write(this, string, offset, length)

	      default:
	        if (loweredCase) throw new TypeError('Unknown encoding: ' + encoding)
	        encoding = ('' + encoding).toLowerCase()
	        loweredCase = true
	    }
	  }
	}

	Buffer.prototype.toJSON = function toJSON () {
	  return {
	    type: 'Buffer',
	    data: Array.prototype.slice.call(this._arr || this, 0)
	  }
	}

	function base64Slice (buf, start, end) {
	  if (start === 0 && end === buf.length) {
	    return base64.fromByteArray(buf)
	  } else {
	    return base64.fromByteArray(buf.slice(start, end))
	  }
	}

	function utf8Slice (buf, start, end) {
	  end = Math.min(buf.length, end)
	  var res = []

	  var i = start
	  while (i < end) {
	    var firstByte = buf[i]
	    var codePoint = null
	    var bytesPerSequence = (firstByte > 0xEF) ? 4
	      : (firstByte > 0xDF) ? 3
	      : (firstByte > 0xBF) ? 2
	      : 1

	    if (i + bytesPerSequence <= end) {
	      var secondByte, thirdByte, fourthByte, tempCodePoint

	      switch (bytesPerSequence) {
	        case 1:
	          if (firstByte < 0x80) {
	            codePoint = firstByte
	          }
	          break
	        case 2:
	          secondByte = buf[i + 1]
	          if ((secondByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0x1F) << 0x6 | (secondByte & 0x3F)
	            if (tempCodePoint > 0x7F) {
	              codePoint = tempCodePoint
	            }
	          }
	          break
	        case 3:
	          secondByte = buf[i + 1]
	          thirdByte = buf[i + 2]
	          if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0xF) << 0xC | (secondByte & 0x3F) << 0x6 | (thirdByte & 0x3F)
	            if (tempCodePoint > 0x7FF && (tempCodePoint < 0xD800 || tempCodePoint > 0xDFFF)) {
	              codePoint = tempCodePoint
	            }
	          }
	          break
	        case 4:
	          secondByte = buf[i + 1]
	          thirdByte = buf[i + 2]
	          fourthByte = buf[i + 3]
	          if ((secondByte & 0xC0) === 0x80 && (thirdByte & 0xC0) === 0x80 && (fourthByte & 0xC0) === 0x80) {
	            tempCodePoint = (firstByte & 0xF) << 0x12 | (secondByte & 0x3F) << 0xC | (thirdByte & 0x3F) << 0x6 | (fourthByte & 0x3F)
	            if (tempCodePoint > 0xFFFF && tempCodePoint < 0x110000) {
	              codePoint = tempCodePoint
	            }
	          }
	      }
	    }

	    if (codePoint === null) {
	      // we did not generate a valid codePoint so insert a
	      // replacement char (U+FFFD) and advance only 1 byte
	      codePoint = 0xFFFD
	      bytesPerSequence = 1
	    } else if (codePoint > 0xFFFF) {
	      // encode to utf16 (surrogate pair dance)
	      codePoint -= 0x10000
	      res.push(codePoint >>> 10 & 0x3FF | 0xD800)
	      codePoint = 0xDC00 | codePoint & 0x3FF
	    }

	    res.push(codePoint)
	    i += bytesPerSequence
	  }

	  return decodeCodePointsArray(res)
	}

	// Based on http://stackoverflow.com/a/22747272/680742, the browser with
	// the lowest limit is Chrome, with 0x10000 args.
	// We go 1 magnitude less, for safety
	var MAX_ARGUMENTS_LENGTH = 0x1000

	function decodeCodePointsArray (codePoints) {
	  var len = codePoints.length
	  if (len <= MAX_ARGUMENTS_LENGTH) {
	    return String.fromCharCode.apply(String, codePoints) // avoid extra slice()
	  }

	  // Decode in chunks to avoid "call stack size exceeded".
	  var res = ''
	  var i = 0
	  while (i < len) {
	    res += String.fromCharCode.apply(
	      String,
	      codePoints.slice(i, i += MAX_ARGUMENTS_LENGTH)
	    )
	  }
	  return res
	}

	function asciiSlice (buf, start, end) {
	  var ret = ''
	  end = Math.min(buf.length, end)

	  for (var i = start; i < end; ++i) {
	    ret += String.fromCharCode(buf[i] & 0x7F)
	  }
	  return ret
	}

	function latin1Slice (buf, start, end) {
	  var ret = ''
	  end = Math.min(buf.length, end)

	  for (var i = start; i < end; ++i) {
	    ret += String.fromCharCode(buf[i])
	  }
	  return ret
	}

	function hexSlice (buf, start, end) {
	  var len = buf.length

	  if (!start || start < 0) start = 0
	  if (!end || end < 0 || end > len) end = len

	  var out = ''
	  for (var i = start; i < end; ++i) {
	    out += toHex(buf[i])
	  }
	  return out
	}

	function utf16leSlice (buf, start, end) {
	  var bytes = buf.slice(start, end)
	  var res = ''
	  for (var i = 0; i < bytes.length; i += 2) {
	    res += String.fromCharCode(bytes[i] + bytes[i + 1] * 256)
	  }
	  return res
	}

	Buffer.prototype.slice = function slice (start, end) {
	  var len = this.length
	  start = ~~start
	  end = end === undefined ? len : ~~end

	  if (start < 0) {
	    start += len
	    if (start < 0) start = 0
	  } else if (start > len) {
	    start = len
	  }

	  if (end < 0) {
	    end += len
	    if (end < 0) end = 0
	  } else if (end > len) {
	    end = len
	  }

	  if (end < start) end = start

	  var newBuf
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    newBuf = this.subarray(start, end)
	    newBuf.__proto__ = Buffer.prototype
	  } else {
	    var sliceLen = end - start
	    newBuf = new Buffer(sliceLen, undefined)
	    for (var i = 0; i < sliceLen; ++i) {
	      newBuf[i] = this[i + start]
	    }
	  }

	  return newBuf
	}

	/*
	 * Need to make sure that buffer isn't trying to write out of bounds.
	 */
	function checkOffset (offset, ext, length) {
	  if ((offset % 1) !== 0 || offset < 0) throw new RangeError('offset is not uint')
	  if (offset + ext > length) throw new RangeError('Trying to access beyond buffer length')
	}

	Buffer.prototype.readUIntLE = function readUIntLE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var val = this[offset]
	  var mul = 1
	  var i = 0
	  while (++i < byteLength && (mul *= 0x100)) {
	    val += this[offset + i] * mul
	  }

	  return val
	}

	Buffer.prototype.readUIntBE = function readUIntBE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    checkOffset(offset, byteLength, this.length)
	  }

	  var val = this[offset + --byteLength]
	  var mul = 1
	  while (byteLength > 0 && (mul *= 0x100)) {
	    val += this[offset + --byteLength] * mul
	  }

	  return val
	}

	Buffer.prototype.readUInt8 = function readUInt8 (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 1, this.length)
	  return this[offset]
	}

	Buffer.prototype.readUInt16LE = function readUInt16LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  return this[offset] | (this[offset + 1] << 8)
	}

	Buffer.prototype.readUInt16BE = function readUInt16BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  return (this[offset] << 8) | this[offset + 1]
	}

	Buffer.prototype.readUInt32LE = function readUInt32LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return ((this[offset]) |
	      (this[offset + 1] << 8) |
	      (this[offset + 2] << 16)) +
	      (this[offset + 3] * 0x1000000)
	}

	Buffer.prototype.readUInt32BE = function readUInt32BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset] * 0x1000000) +
	    ((this[offset + 1] << 16) |
	    (this[offset + 2] << 8) |
	    this[offset + 3])
	}

	Buffer.prototype.readIntLE = function readIntLE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var val = this[offset]
	  var mul = 1
	  var i = 0
	  while (++i < byteLength && (mul *= 0x100)) {
	    val += this[offset + i] * mul
	  }
	  mul *= 0x80

	  if (val >= mul) val -= Math.pow(2, 8 * byteLength)

	  return val
	}

	Buffer.prototype.readIntBE = function readIntBE (offset, byteLength, noAssert) {
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) checkOffset(offset, byteLength, this.length)

	  var i = byteLength
	  var mul = 1
	  var val = this[offset + --i]
	  while (i > 0 && (mul *= 0x100)) {
	    val += this[offset + --i] * mul
	  }
	  mul *= 0x80

	  if (val >= mul) val -= Math.pow(2, 8 * byteLength)

	  return val
	}

	Buffer.prototype.readInt8 = function readInt8 (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 1, this.length)
	  if (!(this[offset] & 0x80)) return (this[offset])
	  return ((0xff - this[offset] + 1) * -1)
	}

	Buffer.prototype.readInt16LE = function readInt16LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  var val = this[offset] | (this[offset + 1] << 8)
	  return (val & 0x8000) ? val | 0xFFFF0000 : val
	}

	Buffer.prototype.readInt16BE = function readInt16BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 2, this.length)
	  var val = this[offset + 1] | (this[offset] << 8)
	  return (val & 0x8000) ? val | 0xFFFF0000 : val
	}

	Buffer.prototype.readInt32LE = function readInt32LE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset]) |
	    (this[offset + 1] << 8) |
	    (this[offset + 2] << 16) |
	    (this[offset + 3] << 24)
	}

	Buffer.prototype.readInt32BE = function readInt32BE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)

	  return (this[offset] << 24) |
	    (this[offset + 1] << 16) |
	    (this[offset + 2] << 8) |
	    (this[offset + 3])
	}

	Buffer.prototype.readFloatLE = function readFloatLE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)
	  return ieee754.read(this, offset, true, 23, 4)
	}

	Buffer.prototype.readFloatBE = function readFloatBE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 4, this.length)
	  return ieee754.read(this, offset, false, 23, 4)
	}

	Buffer.prototype.readDoubleLE = function readDoubleLE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 8, this.length)
	  return ieee754.read(this, offset, true, 52, 8)
	}

	Buffer.prototype.readDoubleBE = function readDoubleBE (offset, noAssert) {
	  if (!noAssert) checkOffset(offset, 8, this.length)
	  return ieee754.read(this, offset, false, 52, 8)
	}

	function checkInt (buf, value, offset, ext, max, min) {
	  if (!Buffer.isBuffer(buf)) throw new TypeError('"buffer" argument must be a Buffer instance')
	  if (value > max || value < min) throw new RangeError('"value" argument is out of bounds')
	  if (offset + ext > buf.length) throw new RangeError('Index out of range')
	}

	Buffer.prototype.writeUIntLE = function writeUIntLE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    var maxBytes = Math.pow(2, 8 * byteLength) - 1
	    checkInt(this, value, offset, byteLength, maxBytes, 0)
	  }

	  var mul = 1
	  var i = 0
	  this[offset] = value & 0xFF
	  while (++i < byteLength && (mul *= 0x100)) {
	    this[offset + i] = (value / mul) & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeUIntBE = function writeUIntBE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  byteLength = byteLength | 0
	  if (!noAssert) {
	    var maxBytes = Math.pow(2, 8 * byteLength) - 1
	    checkInt(this, value, offset, byteLength, maxBytes, 0)
	  }

	  var i = byteLength - 1
	  var mul = 1
	  this[offset + i] = value & 0xFF
	  while (--i >= 0 && (mul *= 0x100)) {
	    this[offset + i] = (value / mul) & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeUInt8 = function writeUInt8 (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 1, 0xff, 0)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) value = Math.floor(value)
	  this[offset] = (value & 0xff)
	  return offset + 1
	}

	function objectWriteUInt16 (buf, value, offset, littleEndian) {
	  if (value < 0) value = 0xffff + value + 1
	  for (var i = 0, j = Math.min(buf.length - offset, 2); i < j; ++i) {
	    buf[offset + i] = (value & (0xff << (8 * (littleEndian ? i : 1 - i)))) >>>
	      (littleEndian ? i : 1 - i) * 8
	  }
	}

	Buffer.prototype.writeUInt16LE = function writeUInt16LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	  } else {
	    objectWriteUInt16(this, value, offset, true)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeUInt16BE = function writeUInt16BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0xffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 8)
	    this[offset + 1] = (value & 0xff)
	  } else {
	    objectWriteUInt16(this, value, offset, false)
	  }
	  return offset + 2
	}

	function objectWriteUInt32 (buf, value, offset, littleEndian) {
	  if (value < 0) value = 0xffffffff + value + 1
	  for (var i = 0, j = Math.min(buf.length - offset, 4); i < j; ++i) {
	    buf[offset + i] = (value >>> (littleEndian ? i : 3 - i) * 8) & 0xff
	  }
	}

	Buffer.prototype.writeUInt32LE = function writeUInt32LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset + 3] = (value >>> 24)
	    this[offset + 2] = (value >>> 16)
	    this[offset + 1] = (value >>> 8)
	    this[offset] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, true)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeUInt32BE = function writeUInt32BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0xffffffff, 0)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 24)
	    this[offset + 1] = (value >>> 16)
	    this[offset + 2] = (value >>> 8)
	    this[offset + 3] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, false)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeIntLE = function writeIntLE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) {
	    var limit = Math.pow(2, 8 * byteLength - 1)

	    checkInt(this, value, offset, byteLength, limit - 1, -limit)
	  }

	  var i = 0
	  var mul = 1
	  var sub = 0
	  this[offset] = value & 0xFF
	  while (++i < byteLength && (mul *= 0x100)) {
	    if (value < 0 && sub === 0 && this[offset + i - 1] !== 0) {
	      sub = 1
	    }
	    this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeIntBE = function writeIntBE (value, offset, byteLength, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) {
	    var limit = Math.pow(2, 8 * byteLength - 1)

	    checkInt(this, value, offset, byteLength, limit - 1, -limit)
	  }

	  var i = byteLength - 1
	  var mul = 1
	  var sub = 0
	  this[offset + i] = value & 0xFF
	  while (--i >= 0 && (mul *= 0x100)) {
	    if (value < 0 && sub === 0 && this[offset + i + 1] !== 0) {
	      sub = 1
	    }
	    this[offset + i] = ((value / mul) >> 0) - sub & 0xFF
	  }

	  return offset + byteLength
	}

	Buffer.prototype.writeInt8 = function writeInt8 (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 1, 0x7f, -0x80)
	  if (!Buffer.TYPED_ARRAY_SUPPORT) value = Math.floor(value)
	  if (value < 0) value = 0xff + value + 1
	  this[offset] = (value & 0xff)
	  return offset + 1
	}

	Buffer.prototype.writeInt16LE = function writeInt16LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	  } else {
	    objectWriteUInt16(this, value, offset, true)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeInt16BE = function writeInt16BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 2, 0x7fff, -0x8000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 8)
	    this[offset + 1] = (value & 0xff)
	  } else {
	    objectWriteUInt16(this, value, offset, false)
	  }
	  return offset + 2
	}

	Buffer.prototype.writeInt32LE = function writeInt32LE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value & 0xff)
	    this[offset + 1] = (value >>> 8)
	    this[offset + 2] = (value >>> 16)
	    this[offset + 3] = (value >>> 24)
	  } else {
	    objectWriteUInt32(this, value, offset, true)
	  }
	  return offset + 4
	}

	Buffer.prototype.writeInt32BE = function writeInt32BE (value, offset, noAssert) {
	  value = +value
	  offset = offset | 0
	  if (!noAssert) checkInt(this, value, offset, 4, 0x7fffffff, -0x80000000)
	  if (value < 0) value = 0xffffffff + value + 1
	  if (Buffer.TYPED_ARRAY_SUPPORT) {
	    this[offset] = (value >>> 24)
	    this[offset + 1] = (value >>> 16)
	    this[offset + 2] = (value >>> 8)
	    this[offset + 3] = (value & 0xff)
	  } else {
	    objectWriteUInt32(this, value, offset, false)
	  }
	  return offset + 4
	}

	function checkIEEE754 (buf, value, offset, ext, max, min) {
	  if (offset + ext > buf.length) throw new RangeError('Index out of range')
	  if (offset < 0) throw new RangeError('Index out of range')
	}

	function writeFloat (buf, value, offset, littleEndian, noAssert) {
	  if (!noAssert) {
	    checkIEEE754(buf, value, offset, 4, 3.4028234663852886e+38, -3.4028234663852886e+38)
	  }
	  ieee754.write(buf, value, offset, littleEndian, 23, 4)
	  return offset + 4
	}

	Buffer.prototype.writeFloatLE = function writeFloatLE (value, offset, noAssert) {
	  return writeFloat(this, value, offset, true, noAssert)
	}

	Buffer.prototype.writeFloatBE = function writeFloatBE (value, offset, noAssert) {
	  return writeFloat(this, value, offset, false, noAssert)
	}

	function writeDouble (buf, value, offset, littleEndian, noAssert) {
	  if (!noAssert) {
	    checkIEEE754(buf, value, offset, 8, 1.7976931348623157E+308, -1.7976931348623157E+308)
	  }
	  ieee754.write(buf, value, offset, littleEndian, 52, 8)
	  return offset + 8
	}

	Buffer.prototype.writeDoubleLE = function writeDoubleLE (value, offset, noAssert) {
	  return writeDouble(this, value, offset, true, noAssert)
	}

	Buffer.prototype.writeDoubleBE = function writeDoubleBE (value, offset, noAssert) {
	  return writeDouble(this, value, offset, false, noAssert)
	}

	// copy(targetBuffer, targetStart=0, sourceStart=0, sourceEnd=buffer.length)
	Buffer.prototype.copy = function copy (target, targetStart, start, end) {
	  if (!start) start = 0
	  if (!end && end !== 0) end = this.length
	  if (targetStart >= target.length) targetStart = target.length
	  if (!targetStart) targetStart = 0
	  if (end > 0 && end < start) end = start

	  // Copy 0 bytes; we're done
	  if (end === start) return 0
	  if (target.length === 0 || this.length === 0) return 0

	  // Fatal error conditions
	  if (targetStart < 0) {
	    throw new RangeError('targetStart out of bounds')
	  }
	  if (start < 0 || start >= this.length) throw new RangeError('sourceStart out of bounds')
	  if (end < 0) throw new RangeError('sourceEnd out of bounds')

	  // Are we oob?
	  if (end > this.length) end = this.length
	  if (target.length - targetStart < end - start) {
	    end = target.length - targetStart + start
	  }

	  var len = end - start
	  var i

	  if (this === target && start < targetStart && targetStart < end) {
	    // descending copy from end
	    for (i = len - 1; i >= 0; --i) {
	      target[i + targetStart] = this[i + start]
	    }
	  } else if (len < 1000 || !Buffer.TYPED_ARRAY_SUPPORT) {
	    // ascending copy from start
	    for (i = 0; i < len; ++i) {
	      target[i + targetStart] = this[i + start]
	    }
	  } else {
	    Uint8Array.prototype.set.call(
	      target,
	      this.subarray(start, start + len),
	      targetStart
	    )
	  }

	  return len
	}

	// Usage:
	//    buffer.fill(number[, offset[, end]])
	//    buffer.fill(buffer[, offset[, end]])
	//    buffer.fill(string[, offset[, end]][, encoding])
	Buffer.prototype.fill = function fill (val, start, end, encoding) {
	  // Handle string cases:
	  if (typeof val === 'string') {
	    if (typeof start === 'string') {
	      encoding = start
	      start = 0
	      end = this.length
	    } else if (typeof end === 'string') {
	      encoding = end
	      end = this.length
	    }
	    if (val.length === 1) {
	      var code = val.charCodeAt(0)
	      if (code < 256) {
	        val = code
	      }
	    }
	    if (encoding !== undefined && typeof encoding !== 'string') {
	      throw new TypeError('encoding must be a string')
	    }
	    if (typeof encoding === 'string' && !Buffer.isEncoding(encoding)) {
	      throw new TypeError('Unknown encoding: ' + encoding)
	    }
	  } else if (typeof val === 'number') {
	    val = val & 255
	  }

	  // Invalid ranges are not set to a default, so can range check early.
	  if (start < 0 || this.length < start || this.length < end) {
	    throw new RangeError('Out of range index')
	  }

	  if (end <= start) {
	    return this
	  }

	  start = start >>> 0
	  end = end === undefined ? this.length : end >>> 0

	  if (!val) val = 0

	  var i
	  if (typeof val === 'number') {
	    for (i = start; i < end; ++i) {
	      this[i] = val
	    }
	  } else {
	    var bytes = Buffer.isBuffer(val)
	      ? val
	      : utf8ToBytes(new Buffer(val, encoding).toString())
	    var len = bytes.length
	    for (i = 0; i < end - start; ++i) {
	      this[i + start] = bytes[i % len]
	    }
	  }

	  return this
	}

	// HELPER FUNCTIONS
	// ================

	var INVALID_BASE64_RE = /[^+\/0-9A-Za-z-_]/g

	function base64clean (str) {
	  // Node strips out invalid characters like \n and \t from the string, base64-js does not
	  str = stringtrim(str).replace(INVALID_BASE64_RE, '')
	  // Node converts strings with length < 2 to ''
	  if (str.length < 2) return ''
	  // Node allows for non-padded base64 strings (missing trailing ===), base64-js does not
	  while (str.length % 4 !== 0) {
	    str = str + '='
	  }
	  return str
	}

	function stringtrim (str) {
	  if (str.trim) return str.trim()
	  return str.replace(/^\s+|\s+$/g, '')
	}

	function toHex (n) {
	  if (n < 16) return '0' + n.toString(16)
	  return n.toString(16)
	}

	function utf8ToBytes (string, units) {
	  units = units || Infinity
	  var codePoint
	  var length = string.length
	  var leadSurrogate = null
	  var bytes = []

	  for (var i = 0; i < length; ++i) {
	    codePoint = string.charCodeAt(i)

	    // is surrogate component
	    if (codePoint > 0xD7FF && codePoint < 0xE000) {
	      // last char was a lead
	      if (!leadSurrogate) {
	        // no lead yet
	        if (codePoint > 0xDBFF) {
	          // unexpected trail
	          if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	          continue
	        } else if (i + 1 === length) {
	          // unpaired lead
	          if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	          continue
	        }

	        // valid lead
	        leadSurrogate = codePoint

	        continue
	      }

	      // 2 leads in a row
	      if (codePoint < 0xDC00) {
	        if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	        leadSurrogate = codePoint
	        continue
	      }

	      // valid surrogate pair
	      codePoint = (leadSurrogate - 0xD800 << 10 | codePoint - 0xDC00) + 0x10000
	    } else if (leadSurrogate) {
	      // valid bmp char, but last char was a lead
	      if ((units -= 3) > -1) bytes.push(0xEF, 0xBF, 0xBD)
	    }

	    leadSurrogate = null

	    // encode utf8
	    if (codePoint < 0x80) {
	      if ((units -= 1) < 0) break
	      bytes.push(codePoint)
	    } else if (codePoint < 0x800) {
	      if ((units -= 2) < 0) break
	      bytes.push(
	        codePoint >> 0x6 | 0xC0,
	        codePoint & 0x3F | 0x80
	      )
	    } else if (codePoint < 0x10000) {
	      if ((units -= 3) < 0) break
	      bytes.push(
	        codePoint >> 0xC | 0xE0,
	        codePoint >> 0x6 & 0x3F | 0x80,
	        codePoint & 0x3F | 0x80
	      )
	    } else if (codePoint < 0x110000) {
	      if ((units -= 4) < 0) break
	      bytes.push(
	        codePoint >> 0x12 | 0xF0,
	        codePoint >> 0xC & 0x3F | 0x80,
	        codePoint >> 0x6 & 0x3F | 0x80,
	        codePoint & 0x3F | 0x80
	      )
	    } else {
	      throw new Error('Invalid code point')
	    }
	  }

	  return bytes
	}

	function asciiToBytes (str) {
	  var byteArray = []
	  for (var i = 0; i < str.length; ++i) {
	    // Node's code seems to be doing this and not & 0x7F..
	    byteArray.push(str.charCodeAt(i) & 0xFF)
	  }
	  return byteArray
	}

	function utf16leToBytes (str, units) {
	  var c, hi, lo
	  var byteArray = []
	  for (var i = 0; i < str.length; ++i) {
	    if ((units -= 2) < 0) break

	    c = str.charCodeAt(i)
	    hi = c >> 8
	    lo = c % 256
	    byteArray.push(lo)
	    byteArray.push(hi)
	  }

	  return byteArray
	}

	function base64ToBytes (str) {
	  return base64.toByteArray(base64clean(str))
	}

	function blitBuffer (src, dst, offset, length) {
	  for (var i = 0; i < length; ++i) {
	    if ((i + offset >= dst.length) || (i >= src.length)) break
	    dst[i + offset] = src[i]
	  }
	  return i
	}

	function isnan (val) {
	  return val !== val // eslint-disable-line no-self-compare
	}

	/* WEBPACK VAR INJECTION */}.call(exports, (function() { return this; }())))

/***/ },

/***/ 795:
/***/ function(module, exports) {

	'use strict'

	exports.byteLength = byteLength
	exports.toByteArray = toByteArray
	exports.fromByteArray = fromByteArray

	var lookup = []
	var revLookup = []
	var Arr = typeof Uint8Array !== 'undefined' ? Uint8Array : Array

	var code = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
	for (var i = 0, len = code.length; i < len; ++i) {
	  lookup[i] = code[i]
	  revLookup[code.charCodeAt(i)] = i
	}

	revLookup['-'.charCodeAt(0)] = 62
	revLookup['_'.charCodeAt(0)] = 63

	function placeHoldersCount (b64) {
	  var len = b64.length
	  if (len % 4 > 0) {
	    throw new Error('Invalid string. Length must be a multiple of 4')
	  }

	  // the number of equal signs (place holders)
	  // if there are two placeholders, than the two characters before it
	  // represent one byte
	  // if there is only one, then the three characters before it represent 2 bytes
	  // this is just a cheap hack to not do indexOf twice
	  return b64[len - 2] === '=' ? 2 : b64[len - 1] === '=' ? 1 : 0
	}

	function byteLength (b64) {
	  // base64 is 4/3 + up to two characters of the original data
	  return b64.length * 3 / 4 - placeHoldersCount(b64)
	}

	function toByteArray (b64) {
	  var i, j, l, tmp, placeHolders, arr
	  var len = b64.length
	  placeHolders = placeHoldersCount(b64)

	  arr = new Arr(len * 3 / 4 - placeHolders)

	  // if there are placeholders, only get up to the last complete 4 chars
	  l = placeHolders > 0 ? len - 4 : len

	  var L = 0

	  for (i = 0, j = 0; i < l; i += 4, j += 3) {
	    tmp = (revLookup[b64.charCodeAt(i)] << 18) | (revLookup[b64.charCodeAt(i + 1)] << 12) | (revLookup[b64.charCodeAt(i + 2)] << 6) | revLookup[b64.charCodeAt(i + 3)]
	    arr[L++] = (tmp >> 16) & 0xFF
	    arr[L++] = (tmp >> 8) & 0xFF
	    arr[L++] = tmp & 0xFF
	  }

	  if (placeHolders === 2) {
	    tmp = (revLookup[b64.charCodeAt(i)] << 2) | (revLookup[b64.charCodeAt(i + 1)] >> 4)
	    arr[L++] = tmp & 0xFF
	  } else if (placeHolders === 1) {
	    tmp = (revLookup[b64.charCodeAt(i)] << 10) | (revLookup[b64.charCodeAt(i + 1)] << 4) | (revLookup[b64.charCodeAt(i + 2)] >> 2)
	    arr[L++] = (tmp >> 8) & 0xFF
	    arr[L++] = tmp & 0xFF
	  }

	  return arr
	}

	function tripletToBase64 (num) {
	  return lookup[num >> 18 & 0x3F] + lookup[num >> 12 & 0x3F] + lookup[num >> 6 & 0x3F] + lookup[num & 0x3F]
	}

	function encodeChunk (uint8, start, end) {
	  var tmp
	  var output = []
	  for (var i = start; i < end; i += 3) {
	    tmp = (uint8[i] << 16) + (uint8[i + 1] << 8) + (uint8[i + 2])
	    output.push(tripletToBase64(tmp))
	  }
	  return output.join('')
	}

	function fromByteArray (uint8) {
	  var tmp
	  var len = uint8.length
	  var extraBytes = len % 3 // if we have 1 byte left, pad 2 bytes
	  var output = ''
	  var parts = []
	  var maxChunkLength = 16383 // must be multiple of 3

	  // go through the array every three bytes, we'll deal with trailing stuff later
	  for (var i = 0, len2 = len - extraBytes; i < len2; i += maxChunkLength) {
	    parts.push(encodeChunk(uint8, i, (i + maxChunkLength) > len2 ? len2 : (i + maxChunkLength)))
	  }

	  // pad the end with zeros, but make sure to not forget the extra bytes
	  if (extraBytes === 1) {
	    tmp = uint8[len - 1]
	    output += lookup[tmp >> 2]
	    output += lookup[(tmp << 4) & 0x3F]
	    output += '=='
	  } else if (extraBytes === 2) {
	    tmp = (uint8[len - 2] << 8) + (uint8[len - 1])
	    output += lookup[tmp >> 10]
	    output += lookup[(tmp >> 4) & 0x3F]
	    output += lookup[(tmp << 2) & 0x3F]
	    output += '='
	  }

	  parts.push(output)

	  return parts.join('')
	}


/***/ },

/***/ 796:
/***/ function(module, exports) {

	exports.read = function (buffer, offset, isLE, mLen, nBytes) {
	  var e, m
	  var eLen = nBytes * 8 - mLen - 1
	  var eMax = (1 << eLen) - 1
	  var eBias = eMax >> 1
	  var nBits = -7
	  var i = isLE ? (nBytes - 1) : 0
	  var d = isLE ? -1 : 1
	  var s = buffer[offset + i]

	  i += d

	  e = s & ((1 << (-nBits)) - 1)
	  s >>= (-nBits)
	  nBits += eLen
	  for (; nBits > 0; e = e * 256 + buffer[offset + i], i += d, nBits -= 8) {}

	  m = e & ((1 << (-nBits)) - 1)
	  e >>= (-nBits)
	  nBits += mLen
	  for (; nBits > 0; m = m * 256 + buffer[offset + i], i += d, nBits -= 8) {}

	  if (e === 0) {
	    e = 1 - eBias
	  } else if (e === eMax) {
	    return m ? NaN : ((s ? -1 : 1) * Infinity)
	  } else {
	    m = m + Math.pow(2, mLen)
	    e = e - eBias
	  }
	  return (s ? -1 : 1) * m * Math.pow(2, e - mLen)
	}

	exports.write = function (buffer, value, offset, isLE, mLen, nBytes) {
	  var e, m, c
	  var eLen = nBytes * 8 - mLen - 1
	  var eMax = (1 << eLen) - 1
	  var eBias = eMax >> 1
	  var rt = (mLen === 23 ? Math.pow(2, -24) - Math.pow(2, -77) : 0)
	  var i = isLE ? 0 : (nBytes - 1)
	  var d = isLE ? 1 : -1
	  var s = value < 0 || (value === 0 && 1 / value < 0) ? 1 : 0

	  value = Math.abs(value)

	  if (isNaN(value) || value === Infinity) {
	    m = isNaN(value) ? 1 : 0
	    e = eMax
	  } else {
	    e = Math.floor(Math.log(value) / Math.LN2)
	    if (value * (c = Math.pow(2, -e)) < 1) {
	      e--
	      c *= 2
	    }
	    if (e + eBias >= 1) {
	      value += rt / c
	    } else {
	      value += rt * Math.pow(2, 1 - eBias)
	    }
	    if (value * c >= 2) {
	      e++
	      c /= 2
	    }

	    if (e + eBias >= eMax) {
	      m = 0
	      e = eMax
	    } else if (e + eBias >= 1) {
	      m = (value * c - 1) * Math.pow(2, mLen)
	      e = e + eBias
	    } else {
	      m = value * Math.pow(2, eBias - 1) * Math.pow(2, mLen)
	      e = 0
	    }
	  }

	  for (; mLen >= 8; buffer[offset + i] = m & 0xff, i += d, m /= 256, mLen -= 8) {}

	  e = (e << mLen) | m
	  eLen += mLen
	  for (; eLen > 0; buffer[offset + i] = e & 0xff, i += d, e /= 256, eLen -= 8) {}

	  buffer[offset + i - d] |= s * 128
	}


/***/ },

/***/ 797:
/***/ function(module, exports) {

	var toString = {}.toString;

	module.exports = Array.isArray || function (arr) {
	  return toString.call(arr) == '[object Array]';
	};


/***/ },

/***/ 800:
/***/ function(module, exports, __webpack_require__) {

	/* WEBPACK VAR INJECTION */(function(module, Buffer) {(function (global, module) {

	  var exports = module.exports;

	  /**
	   * Exports.
	   */

	  module.exports = expect;
	  expect.Assertion = Assertion;

	  /**
	   * Exports version.
	   */

	  expect.version = '0.3.1';

	  /**
	   * Possible assertion flags.
	   */

	  var flags = {
	      not: ['to', 'be', 'have', 'include', 'only']
	    , to: ['be', 'have', 'include', 'only', 'not']
	    , only: ['have']
	    , have: ['own']
	    , be: ['an']
	  };

	  function expect (obj) {
	    return new Assertion(obj);
	  }

	  /**
	   * Constructor
	   *
	   * @api private
	   */

	  function Assertion (obj, flag, parent) {
	    this.obj = obj;
	    this.flags = {};

	    if (undefined != parent) {
	      this.flags[flag] = true;

	      for (var i in parent.flags) {
	        if (parent.flags.hasOwnProperty(i)) {
	          this.flags[i] = true;
	        }
	      }
	    }

	    var $flags = flag ? flags[flag] : keys(flags)
	      , self = this;

	    if ($flags) {
	      for (var i = 0, l = $flags.length; i < l; i++) {
	        // avoid recursion
	        if (this.flags[$flags[i]]) continue;

	        var name = $flags[i]
	          , assertion = new Assertion(this.obj, name, this)

	        if ('function' == typeof Assertion.prototype[name]) {
	          // clone the function, make sure we dont touch the prot reference
	          var old = this[name];
	          this[name] = function () {
	            return old.apply(self, arguments);
	          };

	          for (var fn in Assertion.prototype) {
	            if (Assertion.prototype.hasOwnProperty(fn) && fn != name) {
	              this[name][fn] = bind(assertion[fn], assertion);
	            }
	          }
	        } else {
	          this[name] = assertion;
	        }
	      }
	    }
	  }

	  /**
	   * Performs an assertion
	   *
	   * @api private
	   */

	  Assertion.prototype.assert = function (truth, msg, error, expected) {
	    var msg = this.flags.not ? error : msg
	      , ok = this.flags.not ? !truth : truth
	      , err;

	    if (!ok) {
	      err = new Error(msg.call(this));
	      if (arguments.length > 3) {
	        err.actual = this.obj;
	        err.expected = expected;
	        err.showDiff = true;
	      }
	      throw err;
	    }

	    this.and = new Assertion(this.obj);
	  };

	  /**
	   * Check if the value is truthy
	   *
	   * @api public
	   */

	  Assertion.prototype.ok = function () {
	    this.assert(
	        !!this.obj
	      , function(){ return 'expected ' + i(this.obj) + ' to be truthy' }
	      , function(){ return 'expected ' + i(this.obj) + ' to be falsy' });
	  };

	  /**
	   * Creates an anonymous function which calls fn with arguments.
	   *
	   * @api public
	   */

	  Assertion.prototype.withArgs = function() {
	    expect(this.obj).to.be.a('function');
	    var fn = this.obj;
	    var args = Array.prototype.slice.call(arguments);
	    return expect(function() { fn.apply(null, args); });
	  };

	  /**
	   * Assert that the function throws.
	   *
	   * @param {Function|RegExp} callback, or regexp to match error string against
	   * @api public
	   */

	  Assertion.prototype.throwError =
	  Assertion.prototype.throwException = function (fn) {
	    expect(this.obj).to.be.a('function');

	    var thrown = false
	      , not = this.flags.not;

	    try {
	      this.obj();
	    } catch (e) {
	      if (isRegExp(fn)) {
	        var subject = 'string' == typeof e ? e : e.message;
	        if (not) {
	          expect(subject).to.not.match(fn);
	        } else {
	          expect(subject).to.match(fn);
	        }
	      } else if ('function' == typeof fn) {
	        fn(e);
	      }
	      thrown = true;
	    }

	    if (isRegExp(fn) && not) {
	      // in the presence of a matcher, ensure the `not` only applies to
	      // the matching.
	      this.flags.not = false;
	    }

	    var name = this.obj.name || 'fn';
	    this.assert(
	        thrown
	      , function(){ return 'expected ' + name + ' to throw an exception' }
	      , function(){ return 'expected ' + name + ' not to throw an exception' });
	  };

	  /**
	   * Checks if the array is empty.
	   *
	   * @api public
	   */

	  Assertion.prototype.empty = function () {
	    var expectation;

	    if ('object' == typeof this.obj && null !== this.obj && !isArray(this.obj)) {
	      if ('number' == typeof this.obj.length) {
	        expectation = !this.obj.length;
	      } else {
	        expectation = !keys(this.obj).length;
	      }
	    } else {
	      if ('string' != typeof this.obj) {
	        expect(this.obj).to.be.an('object');
	      }

	      expect(this.obj).to.have.property('length');
	      expectation = !this.obj.length;
	    }

	    this.assert(
	        expectation
	      , function(){ return 'expected ' + i(this.obj) + ' to be empty' }
	      , function(){ return 'expected ' + i(this.obj) + ' to not be empty' });
	    return this;
	  };

	  /**
	   * Checks if the obj exactly equals another.
	   *
	   * @api public
	   */

	  Assertion.prototype.be =
	  Assertion.prototype.equal = function (obj) {
	    this.assert(
	        obj === this.obj
	      , function(){ return 'expected ' + i(this.obj) + ' to equal ' + i(obj) }
	      , function(){ return 'expected ' + i(this.obj) + ' to not equal ' + i(obj) });
	    return this;
	  };

	  /**
	   * Checks if the obj sortof equals another.
	   *
	   * @api public
	   */

	  Assertion.prototype.eql = function (obj) {
	    this.assert(
	        expect.eql(this.obj, obj)
	      , function(){ return 'expected ' + i(this.obj) + ' to sort of equal ' + i(obj) }
	      , function(){ return 'expected ' + i(this.obj) + ' to sort of not equal ' + i(obj) }
	      , obj);
	    return this;
	  };

	  /**
	   * Assert within start to finish (inclusive).
	   *
	   * @param {Number} start
	   * @param {Number} finish
	   * @api public
	   */

	  Assertion.prototype.within = function (start, finish) {
	    var range = start + '..' + finish;
	    this.assert(
	        this.obj >= start && this.obj <= finish
	      , function(){ return 'expected ' + i(this.obj) + ' to be within ' + range }
	      , function(){ return 'expected ' + i(this.obj) + ' to not be within ' + range });
	    return this;
	  };

	  /**
	   * Assert typeof / instance of
	   *
	   * @api public
	   */

	  Assertion.prototype.a =
	  Assertion.prototype.an = function (type) {
	    if ('string' == typeof type) {
	      // proper english in error msg
	      var n = /^[aeiou]/.test(type) ? 'n' : '';

	      // typeof with support for 'array'
	      this.assert(
	          'array' == type ? isArray(this.obj) :
	            'regexp' == type ? isRegExp(this.obj) :
	              'object' == type
	                ? 'object' == typeof this.obj && null !== this.obj
	                : type == typeof this.obj
	        , function(){ return 'expected ' + i(this.obj) + ' to be a' + n + ' ' + type }
	        , function(){ return 'expected ' + i(this.obj) + ' not to be a' + n + ' ' + type });
	    } else {
	      // instanceof
	      var name = type.name || 'supplied constructor';
	      this.assert(
	          this.obj instanceof type
	        , function(){ return 'expected ' + i(this.obj) + ' to be an instance of ' + name }
	        , function(){ return 'expected ' + i(this.obj) + ' not to be an instance of ' + name });
	    }

	    return this;
	  };

	  /**
	   * Assert numeric value above _n_.
	   *
	   * @param {Number} n
	   * @api public
	   */

	  Assertion.prototype.greaterThan =
	  Assertion.prototype.above = function (n) {
	    this.assert(
	        this.obj > n
	      , function(){ return 'expected ' + i(this.obj) + ' to be above ' + n }
	      , function(){ return 'expected ' + i(this.obj) + ' to be below ' + n });
	    return this;
	  };

	  /**
	   * Assert numeric value below _n_.
	   *
	   * @param {Number} n
	   * @api public
	   */

	  Assertion.prototype.lessThan =
	  Assertion.prototype.below = function (n) {
	    this.assert(
	        this.obj < n
	      , function(){ return 'expected ' + i(this.obj) + ' to be below ' + n }
	      , function(){ return 'expected ' + i(this.obj) + ' to be above ' + n });
	    return this;
	  };

	  /**
	   * Assert string value matches _regexp_.
	   *
	   * @param {RegExp} regexp
	   * @api public
	   */

	  Assertion.prototype.match = function (regexp) {
	    this.assert(
	        regexp.exec(this.obj)
	      , function(){ return 'expected ' + i(this.obj) + ' to match ' + regexp }
	      , function(){ return 'expected ' + i(this.obj) + ' not to match ' + regexp });
	    return this;
	  };

	  /**
	   * Assert property "length" exists and has value of _n_.
	   *
	   * @param {Number} n
	   * @api public
	   */

	  Assertion.prototype.length = function (n) {
	    expect(this.obj).to.have.property('length');
	    var len = this.obj.length;
	    this.assert(
	        n == len
	      , function(){ return 'expected ' + i(this.obj) + ' to have a length of ' + n + ' but got ' + len }
	      , function(){ return 'expected ' + i(this.obj) + ' to not have a length of ' + len });
	    return this;
	  };

	  /**
	   * Assert property _name_ exists, with optional _val_.
	   *
	   * @param {String} name
	   * @param {Mixed} val
	   * @api public
	   */

	  Assertion.prototype.property = function (name, val) {
	    if (this.flags.own) {
	      this.assert(
	          Object.prototype.hasOwnProperty.call(this.obj, name)
	        , function(){ return 'expected ' + i(this.obj) + ' to have own property ' + i(name) }
	        , function(){ return 'expected ' + i(this.obj) + ' to not have own property ' + i(name) });
	      return this;
	    }

	    if (this.flags.not && undefined !== val) {
	      if (undefined === this.obj[name]) {
	        throw new Error(i(this.obj) + ' has no property ' + i(name));
	      }
	    } else {
	      var hasProp;
	      try {
	        hasProp = name in this.obj
	      } catch (e) {
	        hasProp = undefined !== this.obj[name]
	      }

	      this.assert(
	          hasProp
	        , function(){ return 'expected ' + i(this.obj) + ' to have a property ' + i(name) }
	        , function(){ return 'expected ' + i(this.obj) + ' to not have a property ' + i(name) });
	    }

	    if (undefined !== val) {
	      this.assert(
	          val === this.obj[name]
	        , function(){ return 'expected ' + i(this.obj) + ' to have a property ' + i(name)
	          + ' of ' + i(val) + ', but got ' + i(this.obj[name]) }
	        , function(){ return 'expected ' + i(this.obj) + ' to not have a property ' + i(name)
	          + ' of ' + i(val) });
	    }

	    this.obj = this.obj[name];
	    return this;
	  };

	  /**
	   * Assert that the array contains _obj_ or string contains _obj_.
	   *
	   * @param {Mixed} obj|string
	   * @api public
	   */

	  Assertion.prototype.string =
	  Assertion.prototype.contain = function (obj) {
	    if ('string' == typeof this.obj) {
	      this.assert(
	          ~this.obj.indexOf(obj)
	        , function(){ return 'expected ' + i(this.obj) + ' to contain ' + i(obj) }
	        , function(){ return 'expected ' + i(this.obj) + ' to not contain ' + i(obj) });
	    } else {
	      this.assert(
	          ~indexOf(this.obj, obj)
	        , function(){ return 'expected ' + i(this.obj) + ' to contain ' + i(obj) }
	        , function(){ return 'expected ' + i(this.obj) + ' to not contain ' + i(obj) });
	    }
	    return this;
	  };

	  /**
	   * Assert exact keys or inclusion of keys by using
	   * the `.own` modifier.
	   *
	   * @param {Array|String ...} keys
	   * @api public
	   */

	  Assertion.prototype.key =
	  Assertion.prototype.keys = function ($keys) {
	    var str
	      , ok = true;

	    $keys = isArray($keys)
	      ? $keys
	      : Array.prototype.slice.call(arguments);

	    if (!$keys.length) throw new Error('keys required');

	    var actual = keys(this.obj)
	      , len = $keys.length;

	    // Inclusion
	    ok = every($keys, function (key) {
	      return ~indexOf(actual, key);
	    });

	    // Strict
	    if (!this.flags.not && this.flags.only) {
	      ok = ok && $keys.length == actual.length;
	    }

	    // Key string
	    if (len > 1) {
	      $keys = map($keys, function (key) {
	        return i(key);
	      });
	      var last = $keys.pop();
	      str = $keys.join(', ') + ', and ' + last;
	    } else {
	      str = i($keys[0]);
	    }

	    // Form
	    str = (len > 1 ? 'keys ' : 'key ') + str;

	    // Have / include
	    str = (!this.flags.only ? 'include ' : 'only have ') + str;

	    // Assertion
	    this.assert(
	        ok
	      , function(){ return 'expected ' + i(this.obj) + ' to ' + str }
	      , function(){ return 'expected ' + i(this.obj) + ' to not ' + str });

	    return this;
	  };

	  /**
	   * Assert a failure.
	   *
	   * @param {String ...} custom message
	   * @api public
	   */
	  Assertion.prototype.fail = function (msg) {
	    var error = function() { return msg || "explicit failure"; }
	    this.assert(false, error, error);
	    return this;
	  };

	  /**
	   * Function bind implementation.
	   */

	  function bind (fn, scope) {
	    return function () {
	      return fn.apply(scope, arguments);
	    }
	  }

	  /**
	   * Array every compatibility
	   *
	   * @see bit.ly/5Fq1N2
	   * @api public
	   */

	  function every (arr, fn, thisObj) {
	    var scope = thisObj || global;
	    for (var i = 0, j = arr.length; i < j; ++i) {
	      if (!fn.call(scope, arr[i], i, arr)) {
	        return false;
	      }
	    }
	    return true;
	  }

	  /**
	   * Array indexOf compatibility.
	   *
	   * @see bit.ly/a5Dxa2
	   * @api public
	   */

	  function indexOf (arr, o, i) {
	    if (Array.prototype.indexOf) {
	      return Array.prototype.indexOf.call(arr, o, i);
	    }

	    if (arr.length === undefined) {
	      return -1;
	    }

	    for (var j = arr.length, i = i < 0 ? i + j < 0 ? 0 : i + j : i || 0
	        ; i < j && arr[i] !== o; i++);

	    return j <= i ? -1 : i;
	  }

	  // https://gist.github.com/1044128/
	  var getOuterHTML = function(element) {
	    if ('outerHTML' in element) return element.outerHTML;
	    var ns = "http://www.w3.org/1999/xhtml";
	    var container = document.createElementNS(ns, '_');
	    var xmlSerializer = new XMLSerializer();
	    var html;
	    if (document.xmlVersion) {
	      return xmlSerializer.serializeToString(element);
	    } else {
	      container.appendChild(element.cloneNode(false));
	      html = container.innerHTML.replace('><', '>' + element.innerHTML + '<');
	      container.innerHTML = '';
	      return html;
	    }
	  };

	  // Returns true if object is a DOM element.
	  var isDOMElement = function (object) {
	    if (typeof HTMLElement === 'object') {
	      return object instanceof HTMLElement;
	    } else {
	      return object &&
	        typeof object === 'object' &&
	        object.nodeType === 1 &&
	        typeof object.nodeName === 'string';
	    }
	  };

	  /**
	   * Inspects an object.
	   *
	   * @see taken from node.js `util` module (copyright Joyent, MIT license)
	   * @api private
	   */

	  function i (obj, showHidden, depth) {
	    var seen = [];

	    function stylize (str) {
	      return str;
	    }

	    function format (value, recurseTimes) {
	      // Provide a hook for user-specified inspect functions.
	      // Check that value is an object with an inspect function on it
	      if (value && typeof value.inspect === 'function' &&
	          // Filter out the util module, it's inspect function is special
	          value !== exports &&
	          // Also filter out any prototype objects using the circular check.
	          !(value.constructor && value.constructor.prototype === value)) {
	        return value.inspect(recurseTimes);
	      }

	      // Primitive types cannot have properties
	      switch (typeof value) {
	        case 'undefined':
	          return stylize('undefined', 'undefined');

	        case 'string':
	          var simple = '\'' + json.stringify(value).replace(/^"|"$/g, '')
	                                                   .replace(/'/g, "\\'")
	                                                   .replace(/\\"/g, '"') + '\'';
	          return stylize(simple, 'string');

	        case 'number':
	          return stylize('' + value, 'number');

	        case 'boolean':
	          return stylize('' + value, 'boolean');
	      }
	      // For some reason typeof null is "object", so special case here.
	      if (value === null) {
	        return stylize('null', 'null');
	      }

	      if (isDOMElement(value)) {
	        return getOuterHTML(value);
	      }

	      // Look up the keys of the object.
	      var visible_keys = keys(value);
	      var $keys = showHidden ? Object.getOwnPropertyNames(value) : visible_keys;

	      // Functions without properties can be shortcutted.
	      if (typeof value === 'function' && $keys.length === 0) {
	        if (isRegExp(value)) {
	          return stylize('' + value, 'regexp');
	        } else {
	          var name = value.name ? ': ' + value.name : '';
	          return stylize('[Function' + name + ']', 'special');
	        }
	      }

	      // Dates without properties can be shortcutted
	      if (isDate(value) && $keys.length === 0) {
	        return stylize(value.toUTCString(), 'date');
	      }
	      
	      // Error objects can be shortcutted
	      if (value instanceof Error) {
	        return stylize("["+value.toString()+"]", 'Error');
	      }

	      var base, type, braces;
	      // Determine the object type
	      if (isArray(value)) {
	        type = 'Array';
	        braces = ['[', ']'];
	      } else {
	        type = 'Object';
	        braces = ['{', '}'];
	      }

	      // Make functions say that they are functions
	      if (typeof value === 'function') {
	        var n = value.name ? ': ' + value.name : '';
	        base = (isRegExp(value)) ? ' ' + value : ' [Function' + n + ']';
	      } else {
	        base = '';
	      }

	      // Make dates with properties first say the date
	      if (isDate(value)) {
	        base = ' ' + value.toUTCString();
	      }

	      if ($keys.length === 0) {
	        return braces[0] + base + braces[1];
	      }

	      if (recurseTimes < 0) {
	        if (isRegExp(value)) {
	          return stylize('' + value, 'regexp');
	        } else {
	          return stylize('[Object]', 'special');
	        }
	      }

	      seen.push(value);

	      var output = map($keys, function (key) {
	        var name, str;
	        if (value.__lookupGetter__) {
	          if (value.__lookupGetter__(key)) {
	            if (value.__lookupSetter__(key)) {
	              str = stylize('[Getter/Setter]', 'special');
	            } else {
	              str = stylize('[Getter]', 'special');
	            }
	          } else {
	            if (value.__lookupSetter__(key)) {
	              str = stylize('[Setter]', 'special');
	            }
	          }
	        }
	        if (indexOf(visible_keys, key) < 0) {
	          name = '[' + key + ']';
	        }
	        if (!str) {
	          if (indexOf(seen, value[key]) < 0) {
	            if (recurseTimes === null) {
	              str = format(value[key]);
	            } else {
	              str = format(value[key], recurseTimes - 1);
	            }
	            if (str.indexOf('\n') > -1) {
	              if (isArray(value)) {
	                str = map(str.split('\n'), function (line) {
	                  return '  ' + line;
	                }).join('\n').substr(2);
	              } else {
	                str = '\n' + map(str.split('\n'), function (line) {
	                  return '   ' + line;
	                }).join('\n');
	              }
	            }
	          } else {
	            str = stylize('[Circular]', 'special');
	          }
	        }
	        if (typeof name === 'undefined') {
	          if (type === 'Array' && key.match(/^\d+$/)) {
	            return str;
	          }
	          name = json.stringify('' + key);
	          if (name.match(/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/)) {
	            name = name.substr(1, name.length - 2);
	            name = stylize(name, 'name');
	          } else {
	            name = name.replace(/'/g, "\\'")
	                       .replace(/\\"/g, '"')
	                       .replace(/(^"|"$)/g, "'");
	            name = stylize(name, 'string');
	          }
	        }

	        return name + ': ' + str;
	      });

	      seen.pop();

	      var numLinesEst = 0;
	      var length = reduce(output, function (prev, cur) {
	        numLinesEst++;
	        if (indexOf(cur, '\n') >= 0) numLinesEst++;
	        return prev + cur.length + 1;
	      }, 0);

	      if (length > 50) {
	        output = braces[0] +
	                 (base === '' ? '' : base + '\n ') +
	                 ' ' +
	                 output.join(',\n  ') +
	                 ' ' +
	                 braces[1];

	      } else {
	        output = braces[0] + base + ' ' + output.join(', ') + ' ' + braces[1];
	      }

	      return output;
	    }
	    return format(obj, (typeof depth === 'undefined' ? 2 : depth));
	  }

	  expect.stringify = i;

	  function isArray (ar) {
	    return Object.prototype.toString.call(ar) === '[object Array]';
	  }

	  function isRegExp(re) {
	    var s;
	    try {
	      s = '' + re;
	    } catch (e) {
	      return false;
	    }

	    return re instanceof RegExp || // easy case
	           // duck-type for context-switching evalcx case
	           typeof(re) === 'function' &&
	           re.constructor.name === 'RegExp' &&
	           re.compile &&
	           re.test &&
	           re.exec &&
	           s.match(/^\/.*\/[gim]{0,3}$/);
	  }

	  function isDate(d) {
	    return d instanceof Date;
	  }

	  function keys (obj) {
	    if (Object.keys) {
	      return Object.keys(obj);
	    }

	    var keys = [];

	    for (var i in obj) {
	      if (Object.prototype.hasOwnProperty.call(obj, i)) {
	        keys.push(i);
	      }
	    }

	    return keys;
	  }

	  function map (arr, mapper, that) {
	    if (Array.prototype.map) {
	      return Array.prototype.map.call(arr, mapper, that);
	    }

	    var other= new Array(arr.length);

	    for (var i= 0, n = arr.length; i<n; i++)
	      if (i in arr)
	        other[i] = mapper.call(that, arr[i], i, arr);

	    return other;
	  }

	  function reduce (arr, fun) {
	    if (Array.prototype.reduce) {
	      return Array.prototype.reduce.apply(
	          arr
	        , Array.prototype.slice.call(arguments, 1)
	      );
	    }

	    var len = +this.length;

	    if (typeof fun !== "function")
	      throw new TypeError();

	    // no value to return if no initial value and an empty array
	    if (len === 0 && arguments.length === 1)
	      throw new TypeError();

	    var i = 0;
	    if (arguments.length >= 2) {
	      var rv = arguments[1];
	    } else {
	      do {
	        if (i in this) {
	          rv = this[i++];
	          break;
	        }

	        // if array contains no values, no initial value to return
	        if (++i >= len)
	          throw new TypeError();
	      } while (true);
	    }

	    for (; i < len; i++) {
	      if (i in this)
	        rv = fun.call(null, rv, this[i], i, this);
	    }

	    return rv;
	  }

	  /**
	   * Asserts deep equality
	   *
	   * @see taken from node.js `assert` module (copyright Joyent, MIT license)
	   * @api private
	   */

	  expect.eql = function eql(actual, expected) {
	    // 7.1. All identical values are equivalent, as determined by ===.
	    if (actual === expected) {
	      return true;
	    } else if ('undefined' != typeof Buffer
	      && Buffer.isBuffer(actual) && Buffer.isBuffer(expected)) {
	      if (actual.length != expected.length) return false;

	      for (var i = 0; i < actual.length; i++) {
	        if (actual[i] !== expected[i]) return false;
	      }

	      return true;

	      // 7.2. If the expected value is a Date object, the actual value is
	      // equivalent if it is also a Date object that refers to the same time.
	    } else if (actual instanceof Date && expected instanceof Date) {
	      return actual.getTime() === expected.getTime();

	      // 7.3. Other pairs that do not both pass typeof value == "object",
	      // equivalence is determined by ==.
	    } else if (typeof actual != 'object' && typeof expected != 'object') {
	      return actual == expected;
	    // If both are regular expression use the special `regExpEquiv` method
	    // to determine equivalence.
	    } else if (isRegExp(actual) && isRegExp(expected)) {
	      return regExpEquiv(actual, expected);
	    // 7.4. For all other Object pairs, including Array objects, equivalence is
	    // determined by having the same number of owned properties (as verified
	    // with Object.prototype.hasOwnProperty.call), the same set of keys
	    // (although not necessarily the same order), equivalent values for every
	    // corresponding key, and an identical "prototype" property. Note: this
	    // accounts for both named and indexed properties on Arrays.
	    } else {
	      return objEquiv(actual, expected);
	    }
	  };

	  function isUndefinedOrNull (value) {
	    return value === null || value === undefined;
	  }

	  function isArguments (object) {
	    return Object.prototype.toString.call(object) == '[object Arguments]';
	  }

	  function regExpEquiv (a, b) {
	    return a.source === b.source && a.global === b.global &&
	           a.ignoreCase === b.ignoreCase && a.multiline === b.multiline;
	  }

	  function objEquiv (a, b) {
	    if (isUndefinedOrNull(a) || isUndefinedOrNull(b))
	      return false;
	    // an identical "prototype" property.
	    if (a.prototype !== b.prototype) return false;
	    //~~~I've managed to break Object.keys through screwy arguments passing.
	    //   Converting to array solves the problem.
	    if (isArguments(a)) {
	      if (!isArguments(b)) {
	        return false;
	      }
	      a = pSlice.call(a);
	      b = pSlice.call(b);
	      return expect.eql(a, b);
	    }
	    try{
	      var ka = keys(a),
	        kb = keys(b),
	        key, i;
	    } catch (e) {//happens when one is a string literal and the other isn't
	      return false;
	    }
	    // having the same number of owned properties (keys incorporates hasOwnProperty)
	    if (ka.length != kb.length)
	      return false;
	    //the same set of keys (although not necessarily the same order),
	    ka.sort();
	    kb.sort();
	    //~~~cheap key test
	    for (i = ka.length - 1; i >= 0; i--) {
	      if (ka[i] != kb[i])
	        return false;
	    }
	    //equivalent values for every corresponding key, and
	    //~~~possibly expensive deep test
	    for (i = ka.length - 1; i >= 0; i--) {
	      key = ka[i];
	      if (!expect.eql(a[key], b[key]))
	         return false;
	    }
	    return true;
	  }

	  var json = (function () {
	    "use strict";

	    if ('object' == typeof JSON && JSON.parse && JSON.stringify) {
	      return {
	          parse: nativeJSON.parse
	        , stringify: nativeJSON.stringify
	      }
	    }

	    var JSON = {};

	    function f(n) {
	        // Format integers to have at least two digits.
	        return n < 10 ? '0' + n : n;
	    }

	    function date(d, key) {
	      return isFinite(d.valueOf()) ?
	          d.getUTCFullYear()     + '-' +
	          f(d.getUTCMonth() + 1) + '-' +
	          f(d.getUTCDate())      + 'T' +
	          f(d.getUTCHours())     + ':' +
	          f(d.getUTCMinutes())   + ':' +
	          f(d.getUTCSeconds())   + 'Z' : null;
	    }

	    var cx = /[\u0000\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g,
	        escapable = /[\\\"\x00-\x1f\x7f-\x9f\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g,
	        gap,
	        indent,
	        meta = {    // table of character substitutions
	            '\b': '\\b',
	            '\t': '\\t',
	            '\n': '\\n',
	            '\f': '\\f',
	            '\r': '\\r',
	            '"' : '\\"',
	            '\\': '\\\\'
	        },
	        rep;


	    function quote(string) {

	  // If the string contains no control characters, no quote characters, and no
	  // backslash characters, then we can safely slap some quotes around it.
	  // Otherwise we must also replace the offending characters with safe escape
	  // sequences.

	        escapable.lastIndex = 0;
	        return escapable.test(string) ? '"' + string.replace(escapable, function (a) {
	            var c = meta[a];
	            return typeof c === 'string' ? c :
	                '\\u' + ('0000' + a.charCodeAt(0).toString(16)).slice(-4);
	        }) + '"' : '"' + string + '"';
	    }


	    function str(key, holder) {

	  // Produce a string from holder[key].

	        var i,          // The loop counter.
	            k,          // The member key.
	            v,          // The member value.
	            length,
	            mind = gap,
	            partial,
	            value = holder[key];

	  // If the value has a toJSON method, call it to obtain a replacement value.

	        if (value instanceof Date) {
	            value = date(key);
	        }

	  // If we were called with a replacer function, then call the replacer to
	  // obtain a replacement value.

	        if (typeof rep === 'function') {
	            value = rep.call(holder, key, value);
	        }

	  // What happens next depends on the value's type.

	        switch (typeof value) {
	        case 'string':
	            return quote(value);

	        case 'number':

	  // JSON numbers must be finite. Encode non-finite numbers as null.

	            return isFinite(value) ? String(value) : 'null';

	        case 'boolean':
	        case 'null':

	  // If the value is a boolean or null, convert it to a string. Note:
	  // typeof null does not produce 'null'. The case is included here in
	  // the remote chance that this gets fixed someday.

	            return String(value);

	  // If the type is 'object', we might be dealing with an object or an array or
	  // null.

	        case 'object':

	  // Due to a specification blunder in ECMAScript, typeof null is 'object',
	  // so watch out for that case.

	            if (!value) {
	                return 'null';
	            }

	  // Make an array to hold the partial results of stringifying this object value.

	            gap += indent;
	            partial = [];

	  // Is the value an array?

	            if (Object.prototype.toString.apply(value) === '[object Array]') {

	  // The value is an array. Stringify every element. Use null as a placeholder
	  // for non-JSON values.

	                length = value.length;
	                for (i = 0; i < length; i += 1) {
	                    partial[i] = str(i, value) || 'null';
	                }

	  // Join all of the elements together, separated with commas, and wrap them in
	  // brackets.

	                v = partial.length === 0 ? '[]' : gap ?
	                    '[\n' + gap + partial.join(',\n' + gap) + '\n' + mind + ']' :
	                    '[' + partial.join(',') + ']';
	                gap = mind;
	                return v;
	            }

	  // If the replacer is an array, use it to select the members to be stringified.

	            if (rep && typeof rep === 'object') {
	                length = rep.length;
	                for (i = 0; i < length; i += 1) {
	                    if (typeof rep[i] === 'string') {
	                        k = rep[i];
	                        v = str(k, value);
	                        if (v) {
	                            partial.push(quote(k) + (gap ? ': ' : ':') + v);
	                        }
	                    }
	                }
	            } else {

	  // Otherwise, iterate through all of the keys in the object.

	                for (k in value) {
	                    if (Object.prototype.hasOwnProperty.call(value, k)) {
	                        v = str(k, value);
	                        if (v) {
	                            partial.push(quote(k) + (gap ? ': ' : ':') + v);
	                        }
	                    }
	                }
	            }

	  // Join all of the member texts together, separated with commas,
	  // and wrap them in braces.

	            v = partial.length === 0 ? '{}' : gap ?
	                '{\n' + gap + partial.join(',\n' + gap) + '\n' + mind + '}' :
	                '{' + partial.join(',') + '}';
	            gap = mind;
	            return v;
	        }
	    }

	  // If the JSON object does not yet have a stringify method, give it one.

	    JSON.stringify = function (value, replacer, space) {

	  // The stringify method takes a value and an optional replacer, and an optional
	  // space parameter, and returns a JSON text. The replacer can be a function
	  // that can replace values, or an array of strings that will select the keys.
	  // A default replacer method can be provided. Use of the space parameter can
	  // produce text that is more easily readable.

	        var i;
	        gap = '';
	        indent = '';

	  // If the space parameter is a number, make an indent string containing that
	  // many spaces.

	        if (typeof space === 'number') {
	            for (i = 0; i < space; i += 1) {
	                indent += ' ';
	            }

	  // If the space parameter is a string, it will be used as the indent string.

	        } else if (typeof space === 'string') {
	            indent = space;
	        }

	  // If there is a replacer, it must be a function or an array.
	  // Otherwise, throw an error.

	        rep = replacer;
	        if (replacer && typeof replacer !== 'function' &&
	                (typeof replacer !== 'object' ||
	                typeof replacer.length !== 'number')) {
	            throw new Error('JSON.stringify');
	        }

	  // Make a fake root object containing our value under the key of ''.
	  // Return the result of stringifying the value.

	        return str('', {'': value});
	    };

	  // If the JSON object does not yet have a parse method, give it one.

	    JSON.parse = function (text, reviver) {
	    // The parse method takes a text and an optional reviver function, and returns
	    // a JavaScript value if the text is a valid JSON text.

	        var j;

	        function walk(holder, key) {

	    // The walk method is used to recursively walk the resulting structure so
	    // that modifications can be made.

	            var k, v, value = holder[key];
	            if (value && typeof value === 'object') {
	                for (k in value) {
	                    if (Object.prototype.hasOwnProperty.call(value, k)) {
	                        v = walk(value, k);
	                        if (v !== undefined) {
	                            value[k] = v;
	                        } else {
	                            delete value[k];
	                        }
	                    }
	                }
	            }
	            return reviver.call(holder, key, value);
	        }


	    // Parsing happens in four stages. In the first stage, we replace certain
	    // Unicode characters with escape sequences. JavaScript handles many characters
	    // incorrectly, either silently deleting them, or treating them as line endings.

	        text = String(text);
	        cx.lastIndex = 0;
	        if (cx.test(text)) {
	            text = text.replace(cx, function (a) {
	                return '\\u' +
	                    ('0000' + a.charCodeAt(0).toString(16)).slice(-4);
	            });
	        }

	    // In the second stage, we run the text against regular expressions that look
	    // for non-JSON patterns. We are especially concerned with '()' and 'new'
	    // because they can cause invocation, and '=' because it can cause mutation.
	    // But just to be safe, we want to reject all unexpected forms.

	    // We split the second stage into 4 regexp operations in order to work around
	    // crippling inefficiencies in IE's and Safari's regexp engines. First we
	    // replace the JSON backslash pairs with '@' (a non-JSON character). Second, we
	    // replace all simple value tokens with ']' characters. Third, we delete all
	    // open brackets that follow a colon or comma or that begin the text. Finally,
	    // we look to see that the remaining characters are only whitespace or ']' or
	    // ',' or ':' or '{' or '}'. If that is so, then the text is safe for eval.

	        if (/^[\],:{}\s]*$/
	                .test(text.replace(/\\(?:["\\\/bfnrt]|u[0-9a-fA-F]{4})/g, '@')
	                    .replace(/"[^"\\\n\r]*"|true|false|null|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?/g, ']')
	                    .replace(/(?:^|:|,)(?:\s*\[)+/g, ''))) {

	    // In the third stage we use the eval function to compile the text into a
	    // JavaScript structure. The '{' operator is subject to a syntactic ambiguity
	    // in JavaScript: it can begin a block or an object literal. We wrap the text
	    // in parens to eliminate the ambiguity.

	            j = eval('(' + text + ')');

	    // In the optional fourth stage, we recursively walk the new structure, passing
	    // each name/value pair to a reviver function for possible transformation.

	            return typeof reviver === 'function' ?
	                walk({'': j}, '') : j;
	        }

	    // If the text is not JSON parseable, then a SyntaxError is thrown.

	        throw new SyntaxError('JSON.parse');
	    };

	    return JSON;
	  })();

	  if ('undefined' != typeof window) {
	    window.expect = module.exports;
	  }

	})(
	    this
	  ,  true ? module : {exports: {}}
	);

	/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(51)(module), __webpack_require__(794).Buffer))

/***/ },

/***/ 828:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var feature = __webpack_require__(829);

	module.exports = feature;

/***/ },

/***/ 829:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var pick = __webpack_require__(67);
	var put = __webpack_require__(112);
	var fs = __webpack_require__(118);
	var path = __webpack_require__(119);

	var config = void 0;

	var flag = __webpack_require__(121);

	function isBrowser() {
	  return typeof window == "object" && typeof module == "undefined";
	}

	/**
	 * Gets a config value for a given key
	 * e.g "chrome.webSocketPort"
	 */
	function getValue(key) {
	  return pick(config, key);
	}

	function setValue(key, value) {
	  return put(config, key, value);
	}

	function isEnabled(key) {
	  return config.features && typeof config.features[key] == "object" ? config.features[key].enabled : config.features[key];
	}

	function isDevelopment() {
	  if (isBrowser()) {
	    if (true) {
	      return false;
	    }
	    var href = window.location ? window.location.href : "";
	    return href.match(/^file:/) || href.match(/localhost:/);
	  }

	  if (isFirefoxPanel()) {
	    // Default to production if compiling for the Firefox panel
	    return ("production") === "development";
	  }
	  return ("production") !== "production";
	}

	function isTesting() {
	  return flag.testing;
	}

	function isFirefoxPanel() {
	  return ("firefox-panel") == "firefox-panel";
	}

	function isApplication() {
	  return ("firefox-panel") == "application";
	}

	function isFirefox() {
	  return (/firefox/i.test(navigator.userAgent)
	  );
	}

	function setConfig(value) {
	  config = value;
	}

	function getConfig() {
	  return config;
	}

	function updateLocalConfig(relativePath) {
	  var localConfigPath = path.resolve(relativePath, "../configs/local.json");
	  var output = JSON.stringify(config, null, 2);
	  fs.writeFileSync(localConfigPath, output, { flag: "w" });
	  return output;
	}

	module.exports = {
	  isEnabled,
	  getValue,
	  setValue,
	  isDevelopment,
	  isTesting,
	  isFirefoxPanel,
	  isApplication,
	  isFirefox,
	  getConfig,
	  setConfig,
	  updateLocalConfig
	};

/***/ },

/***/ 830:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	var Menu = __webpack_require__(967);
	var MenuItem = __webpack_require__(970);

	var _require = __webpack_require__(971),
	    PrefsHelper = _require.PrefsHelper;

	var Services = __webpack_require__(29);

	var _require2 = __webpack_require__(972),
	    KeyShortcuts = _require2.KeyShortcuts;

	module.exports = {
	  KeyShortcuts,
	  Menu,
	  MenuItem,
	  PrefsHelper,
	  Services
	};

/***/ },

/***/ 967:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	var EventEmitter = __webpack_require__(968);

	function inToolbox() {
	  return window.parent.document.documentURI == "about:devtools-toolbox";
	}

	/**
	 * A partial implementation of the Menu API provided by electron:
	 * https://github.com/electron/electron/blob/master/docs/api/menu.md.
	 *
	 * Extra features:
	 *  - Emits an 'open' and 'close' event when the menu is opened/closed

	 * @param String id (non standard)
	 *        Needed so tests can confirm the XUL implementation is working
	 */
	function Menu() {
	  var _ref = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : {},
	      _ref$id = _ref.id,
	      id = _ref$id === undefined ? null : _ref$id;

	  this.menuitems = [];
	  this.id = id;

	  Object.defineProperty(this, "items", {
	    get() {
	      return this.menuitems;
	    }
	  });

	  EventEmitter.decorate(this);
	}

	/**
	 * Add an item to the end of the Menu
	 *
	 * @param {MenuItem} menuItem
	 */
	Menu.prototype.append = function (menuItem) {
	  this.menuitems.push(menuItem);
	};

	/**
	 * Add an item to a specified position in the menu
	 *
	 * @param {int} pos
	 * @param {MenuItem} menuItem
	 */
	Menu.prototype.insert = function (pos, menuItem) {
	  throw Error("Not implemented");
	};

	/**
	 * Show the Menu at a specified location on the screen
	 *
	 * Missing features:
	 *   - browserWindow - BrowserWindow (optional) - Default is null.
	 *   - positioningItem Number - (optional) OS X
	 *
	 * @param {int} screenX
	 * @param {int} screenY
	 * @param Toolbox toolbox (non standard)
	 *        Needed so we in which window to inject XUL
	 */
	Menu.prototype.popup = function (screenX, screenY, toolbox) {
	  var doc = toolbox.doc;
	  var popupset = doc.querySelector("popupset");
	  // See bug 1285229, on Windows, opening the same popup multiple times in a
	  // row ends up duplicating the popup. The newly inserted popup doesn't
	  // dismiss the old one. So remove any previously displayed popup before
	  // opening a new one.
	  var popup = popupset.querySelector("menupopup[menu-api=\"true\"]");
	  if (popup) {
	    popup.hidePopup();
	  }

	  popup = this.createPopup(doc);
	  popup.setAttribute("menu-api", "true");

	  if (this.id) {
	    popup.id = this.id;
	  }
	  this._createMenuItems(popup);

	  // Remove the menu from the DOM once it's hidden.
	  popup.addEventListener("popuphidden", e => {
	    if (e.target === popup) {
	      popup.remove();
	      this.emit("close", popup);
	    }
	  });

	  popup.addEventListener("popupshown", e => {
	    if (e.target === popup) {
	      this.emit("open", popup);
	    }
	  });

	  popupset.appendChild(popup);
	  popup.openPopupAtScreen(screenX, screenY, true);
	};

	Menu.prototype.createPopup = function (doc) {
	  return doc.createElement("menupopup");
	};

	Menu.prototype._createMenuItems = function (parent) {
	  var doc = parent.ownerDocument;
	  this.menuitems.forEach(item => {
	    if (!item.visible) {
	      return;
	    }

	    if (item.submenu) {
	      var menupopup = doc.createElement("menupopup");
	      item.submenu._createMenuItems(menupopup);

	      var menuitem = doc.createElement("menuitem");
	      menuitem.setAttribute("label", item.label);
	      if (!inToolbox()) {
	        menuitem.textContent = item.label;
	      }

	      var menu = doc.createElement("menu");
	      menu.appendChild(menuitem);
	      menu.appendChild(menupopup);
	      if (item.disabled) {
	        menu.setAttribute("disabled", "true");
	      }
	      if (item.accesskey) {
	        menu.setAttribute("accesskey", item.accesskey);
	      }
	      if (item.id) {
	        menu.id = item.id;
	      }
	      parent.appendChild(menu);
	    } else if (item.type === "separator") {
	      var menusep = doc.createElement("menuseparator");
	      parent.appendChild(menusep);
	    } else {
	      var _menuitem = doc.createElement("menuitem");
	      _menuitem.setAttribute("label", item.label);

	      if (!inToolbox()) {
	        _menuitem.textContent = item.label;
	      }

	      _menuitem.addEventListener("command", () => item.click());

	      if (item.type === "checkbox") {
	        _menuitem.setAttribute("type", "checkbox");
	      }
	      if (item.type === "radio") {
	        _menuitem.setAttribute("type", "radio");
	      }
	      if (item.disabled) {
	        _menuitem.setAttribute("disabled", "true");
	      }
	      if (item.checked) {
	        _menuitem.setAttribute("checked", "true");
	      }
	      if (item.accesskey) {
	        _menuitem.setAttribute("accesskey", item.accesskey);
	      }
	      if (item.id) {
	        _menuitem.id = item.id;
	      }

	      parent.appendChild(_menuitem);
	    }
	  });
	};

	Menu.setApplicationMenu = () => {
	  throw Error("Not implemented");
	};

	Menu.sendActionToFirstResponder = () => {
	  throw Error("Not implemented");
	};

	Menu.buildFromTemplate = () => {
	  throw Error("Not implemented");
	};

	module.exports = Menu;

/***/ },

/***/ 968:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	var EventEmitter = function EventEmitter() {};
	module.exports = EventEmitter;

	var promise = __webpack_require__(969);

	/**
	 * Decorate an object with event emitter functionality.
	 *
	 * @param Object aObjectToDecorate
	 *        Bind all public methods of EventEmitter to
	 *        the aObjectToDecorate object.
	 */
	EventEmitter.decorate = function EventEmitter_decorate(aObjectToDecorate) {
	  var emitter = new EventEmitter();
	  aObjectToDecorate.on = emitter.on.bind(emitter);
	  aObjectToDecorate.off = emitter.off.bind(emitter);
	  aObjectToDecorate.once = emitter.once.bind(emitter);
	  aObjectToDecorate.emit = emitter.emit.bind(emitter);
	};

	EventEmitter.prototype = {
	  /**
	   * Connect a listener.
	   *
	   * @param string aEvent
	   *        The event name to which we're connecting.
	   * @param function aListener
	   *        Called when the event is fired.
	   */
	  on: function EventEmitter_on(aEvent, aListener) {
	    if (!this._eventEmitterListeners) this._eventEmitterListeners = new Map();
	    if (!this._eventEmitterListeners.has(aEvent)) {
	      this._eventEmitterListeners.set(aEvent, []);
	    }
	    this._eventEmitterListeners.get(aEvent).push(aListener);
	  },

	  /**
	   * Listen for the next time an event is fired.
	   *
	   * @param string aEvent
	   *        The event name to which we're connecting.
	   * @param function aListener
	   *        (Optional) Called when the event is fired. Will be called at most
	   *        one time.
	   * @return promise
	   *        A promise which is resolved when the event next happens. The
	   *        resolution value of the promise is the first event argument. If
	   *        you need access to second or subsequent event arguments (it's rare
	   *        that this is needed) then use aListener
	   */
	  once: function EventEmitter_once(aEvent, aListener) {
	    var _this = this;

	    var deferred = promise.defer();

	    var handler = function (aEvent, aFirstArg) {
	      for (var _len = arguments.length, aRest = Array(_len > 2 ? _len - 2 : 0), _key = 2; _key < _len; _key++) {
	        aRest[_key - 2] = arguments[_key];
	      }

	      _this.off(aEvent, handler);
	      if (aListener) {
	        aListener.apply(null, [aEvent, aFirstArg].concat(aRest));
	      }
	      deferred.resolve(aFirstArg);
	    };

	    handler._originalListener = aListener;
	    this.on(aEvent, handler);

	    return deferred.promise;
	  },

	  /**
	   * Remove a previously-registered event listener.  Works for events
	   * registered with either on or once.
	   *
	   * @param string aEvent
	   *        The event name whose listener we're disconnecting.
	   * @param function aListener
	   *        The listener to remove.
	   */
	  off: function EventEmitter_off(aEvent, aListener) {
	    if (!this._eventEmitterListeners) return;
	    var listeners = this._eventEmitterListeners.get(aEvent);
	    if (listeners) {
	      this._eventEmitterListeners.set(aEvent, listeners.filter(l => {
	        return l !== aListener && l._originalListener !== aListener;
	      }));
	    }
	  },

	  /**
	   * Emit an event.  All arguments to this method will
	   * be sent to listener functions.
	   */
	  emit: function EventEmitter_emit(aEvent) {
	    var _this2 = this,
	        _arguments = arguments;

	    if (!this._eventEmitterListeners || !this._eventEmitterListeners.has(aEvent)) {
	      return;
	    }

	    var originalListeners = this._eventEmitterListeners.get(aEvent);

	    var _loop = function (listener) {
	      // If the object was destroyed during event emission, stop
	      // emitting.
	      if (!_this2._eventEmitterListeners) {
	        return "break";
	      }

	      // If listeners were removed during emission, make sure the
	      // event handler we're going to fire wasn't removed.
	      if (originalListeners === _this2._eventEmitterListeners.get(aEvent) || _this2._eventEmitterListeners.get(aEvent).some(l => l === listener)) {
	        try {
	          listener.apply(null, _arguments);
	        } catch (ex) {
	          // Prevent a bad listener from interfering with the others.
	          var msg = ex + ": " + ex.stack;
	          //console.error(msg);
	          console.log(msg);
	        }
	      }
	    };

	    for (var listener of this._eventEmitterListeners.get(aEvent)) {
	      var _ret = _loop(listener);

	      if (_ret === "break") break;
	    }
	  }
	};

/***/ },

/***/ 969:
/***/ function(module, exports) {

	"use strict";

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	/*
	 * A sham for https://dxr.mozilla.org/mozilla-central/source/toolkit/modules/Promise.jsm
	 */

	/**
	 * Promise.jsm is mostly the Promise web API with a `defer` method. Just drop this in here,
	 * and use the native web API (although building with webpack/babel, it may replace this
	 * with it's own version if we want to target environments that do not have `Promise`.
	 */

	var p = typeof window != "undefined" ? window.Promise : Promise;
	p.defer = function defer() {
	  var resolve, reject;
	  var promise = new Promise(function () {
	    resolve = arguments[0];
	    reject = arguments[1];
	  });
	  return {
	    resolve: resolve,
	    reject: reject,
	    promise: promise
	  };
	};

	module.exports = p;

/***/ },

/***/ 970:
/***/ function(module, exports) {

	"use strict";

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	/**
	 * A partial implementation of the MenuItem API provided by electron:
	 * https://github.com/electron/electron/blob/master/docs/api/menu-item.md.
	 *
	 * Missing features:
	 *   - id String - Unique within a single menu. If defined then it can be used
	 *                 as a reference to this item by the position attribute.
	 *   - role String - Define the action of the menu item; when specified the
	 *                   click property will be ignored
	 *   - sublabel String
	 *   - accelerator Accelerator
	 *   - icon NativeImage
	 *   - position String - This field allows fine-grained definition of the
	 *                       specific location within a given menu.
	 *
	 * Implemented features:
	 *  @param Object options
	 *    Function click
	 *      Will be called with click(menuItem, browserWindow) when the menu item
	 *       is clicked
	 *    String type
	 *      Can be normal, separator, submenu, checkbox or radio
	 *    String label
	 *    Boolean enabled
	 *      If false, the menu item will be greyed out and unclickable.
	 *    Boolean checked
	 *      Should only be specified for checkbox or radio type menu items.
	 *    Menu submenu
	 *      Should be specified for submenu type menu items. If submenu is specified,
	 *      the type: 'submenu' can be omitted. If the value is not a Menu then it
	 *      will be automatically converted to one using Menu.buildFromTemplate.
	 *    Boolean visible
	 *      If false, the menu item will be entirely hidden.
	 */
	function MenuItem() {
	  var _ref = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : {},
	      _ref$accesskey = _ref.accesskey,
	      accesskey = _ref$accesskey === undefined ? null : _ref$accesskey,
	      _ref$checked = _ref.checked,
	      checked = _ref$checked === undefined ? false : _ref$checked,
	      _ref$click = _ref.click,
	      click = _ref$click === undefined ? () => {} : _ref$click,
	      _ref$disabled = _ref.disabled,
	      disabled = _ref$disabled === undefined ? false : _ref$disabled,
	      _ref$label = _ref.label,
	      label = _ref$label === undefined ? "" : _ref$label,
	      _ref$id = _ref.id,
	      id = _ref$id === undefined ? null : _ref$id,
	      _ref$submenu = _ref.submenu,
	      submenu = _ref$submenu === undefined ? null : _ref$submenu,
	      _ref$type = _ref.type,
	      type = _ref$type === undefined ? "normal" : _ref$type,
	      _ref$visible = _ref.visible,
	      visible = _ref$visible === undefined ? true : _ref$visible;

	  this.accesskey = accesskey;
	  this.checked = checked;
	  this.click = click;
	  this.disabled = disabled;
	  this.id = id;
	  this.label = label;
	  this.submenu = submenu;
	  this.type = type;
	  this.visible = visible;
	}

	module.exports = MenuItem;

/***/ },

/***/ 971:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var _slicedToArray = function () { function sliceIterator(arr, i) { var _arr = []; var _n = true; var _d = false; var _e = undefined; try { for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) { _arr.push(_s.value); if (i && _arr.length === i) break; } } catch (err) { _d = true; _e = err; } finally { try { if (!_n && _i["return"]) _i["return"](); } finally { if (_d) throw _e; } } return _arr; } return function (arr, i) { if (Array.isArray(arr)) { return arr; } else if (Symbol.iterator in Object(arr)) { return sliceIterator(arr, i); } else { throw new TypeError("Invalid attempt to destructure non-iterable instance"); } }; }();

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	var Services = __webpack_require__(29);
	var EventEmitter = __webpack_require__(968);

	/**
	 * Shortcuts for lazily accessing and setting various preferences.
	 * Usage:
	 *   let prefs = new Prefs("root.path.to.branch", {
	 *     myIntPref: ["Int", "leaf.path.to.my-int-pref"],
	 *     myCharPref: ["Char", "leaf.path.to.my-char-pref"],
	 *     myJsonPref: ["Json", "leaf.path.to.my-json-pref"],
	 *     myFloatPref: ["Float", "leaf.path.to.my-float-pref"]
	 *     ...
	 *   });
	 *
	 * Get/set:
	 *   prefs.myCharPref = "foo";
	 *   let aux = prefs.myCharPref;
	 *
	 * Observe:
	 *   prefs.registerObserver();
	 *   prefs.on("pref-changed", (prefName, prefValue) => {
	 *     ...
	 *   });
	 *
	 * @param string prefsRoot
	 *        The root path to the required preferences branch.
	 * @param object prefsBlueprint
	 *        An object containing { accessorName: [prefType, prefName] } keys.
	 */
	function PrefsHelper() {
	  var prefsRoot = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : "";
	  var prefsBlueprint = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};

	  EventEmitter.decorate(this);

	  var cache = new Map();

	  for (var accessorName in prefsBlueprint) {
	    var _prefsBlueprint$acces = _slicedToArray(prefsBlueprint[accessorName], 2),
	        prefType = _prefsBlueprint$acces[0],
	        prefName = _prefsBlueprint$acces[1];

	    map(this, cache, accessorName, prefType, prefsRoot, prefName);
	  }

	  var observer = makeObserver(this, cache, prefsRoot, prefsBlueprint);
	  this.registerObserver = () => observer.register();
	  this.unregisterObserver = () => observer.unregister();
	}

	/**
	 * Helper method for getting a pref value.
	 *
	 * @param Map cache
	 * @param string prefType
	 * @param string prefsRoot
	 * @param string prefName
	 * @return any
	 */
	function get(cache, prefType, prefsRoot, prefName) {
	  var cachedPref = cache.get(prefName);
	  if (cachedPref !== undefined) {
	    return cachedPref;
	  }
	  var value = Services.prefs["get" + prefType + "Pref"]([prefsRoot, prefName].join("."));
	  cache.set(prefName, value);
	  return value;
	}

	/**
	 * Helper method for setting a pref value.
	 *
	 * @param Map cache
	 * @param string prefType
	 * @param string prefsRoot
	 * @param string prefName
	 * @param any value
	 */
	function set(cache, prefType, prefsRoot, prefName, value) {
	  Services.prefs["set" + prefType + "Pref"]([prefsRoot, prefName].join("."), value);
	  cache.set(prefName, value);
	}

	/**
	 * Maps a property name to a pref, defining lazy getters and setters.
	 * Supported types are "Bool", "Char", "Int", "Float" (sugar around "Char"
	 * type and casting), and "Json" (which is basically just sugar for "Char"
	 * using the standard JSON serializer).
	 *
	 * @param PrefsHelper self
	 * @param Map cache
	 * @param string accessorName
	 * @param string prefType
	 * @param string prefsRoot
	 * @param string prefName
	 * @param array serializer [optional]
	 */
	function map(self, cache, accessorName, prefType, prefsRoot, prefName) {
	  var serializer = arguments.length > 6 && arguments[6] !== undefined ? arguments[6] : { in: e => e, out: e => e };

	  if (prefName in self) {
	    throw new Error(`Can't use ${prefName} because it overrides a property` + "on the instance.");
	  }
	  if (prefType == "Json") {
	    map(self, cache, accessorName, "Char", prefsRoot, prefName, {
	      in: JSON.parse,
	      out: JSON.stringify
	    });
	    return;
	  }
	  if (prefType == "Float") {
	    map(self, cache, accessorName, "Char", prefsRoot, prefName, {
	      in: Number.parseFloat,
	      out: n => n + ""
	    });
	    return;
	  }

	  Object.defineProperty(self, accessorName, {
	    get: () => serializer.in(get(cache, prefType, prefsRoot, prefName)),
	    set: e => set(cache, prefType, prefsRoot, prefName, serializer.out(e))
	  });
	}

	/**
	 * Finds the accessor for the provided pref, based on the blueprint object
	 * used in the constructor.
	 *
	 * @param PrefsHelper self
	 * @param object prefsBlueprint
	 * @return string
	 */
	function accessorNameForPref(somePrefName, prefsBlueprint) {
	  for (var accessorName in prefsBlueprint) {
	    var _prefsBlueprint$acces2 = _slicedToArray(prefsBlueprint[accessorName], 2),
	        prefName = _prefsBlueprint$acces2[1];

	    if (somePrefName == prefName) {
	      return accessorName;
	    }
	  }
	  return "";
	}

	/**
	 * Creates a pref observer for `self`.
	 *
	 * @param PrefsHelper self
	 * @param Map cache
	 * @param string prefsRoot
	 * @param object prefsBlueprint
	 * @return object
	 */
	function makeObserver(self, cache, prefsRoot, prefsBlueprint) {
	  return {
	    register: function () {
	      this._branch = Services.prefs.getBranch(prefsRoot + ".");
	      this._branch.addObserver("", this);
	    },
	    unregister: function () {
	      this._branch.removeObserver("", this);
	    },
	    observe: function (subject, topic, prefName) {
	      // If this particular pref isn't handled by the blueprint object,
	      // even though it's in the specified branch, ignore it.
	      var accessorName = accessorNameForPref(prefName, prefsBlueprint);
	      if (!(accessorName in self)) {
	        return;
	      }
	      cache.delete(prefName);
	      self.emit("pref-changed", accessorName, self[accessorName]);
	    }
	  };
	}

	exports.PrefsHelper = PrefsHelper;

/***/ },

/***/ 972:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	var _slicedToArray = function () { function sliceIterator(arr, i) { var _arr = []; var _n = true; var _d = false; var _e = undefined; try { for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) { _arr.push(_s.value); if (i && _arr.length === i) break; } } catch (err) { _d = true; _e = err; } finally { try { if (!_n && _i["return"]) _i["return"](); } finally { if (_d) throw _e; } } return _arr; } return function (arr, i) { if (Array.isArray(arr)) { return arr; } else if (Symbol.iterator in Object(arr)) { return sliceIterator(arr, i); } else { throw new TypeError("Invalid attempt to destructure non-iterable instance"); } }; }();

	/* This Source Code Form is subject to the terms of the Mozilla Public
	 * License, v. 2.0. If a copy of the MPL was not distributed with this
	 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

	var _require = __webpack_require__(29),
	    appinfo = _require.appinfo;

	var EventEmitter = __webpack_require__(968);
	var isOSX = appinfo.OS === "Darwin";

	// List of electron keys mapped to DOM API (DOM_VK_*) key code
	var ElectronKeysMapping = {
	  "F1": "DOM_VK_F1",
	  "F2": "DOM_VK_F2",
	  "F3": "DOM_VK_F3",
	  "F4": "DOM_VK_F4",
	  "F5": "DOM_VK_F5",
	  "F6": "DOM_VK_F6",
	  "F7": "DOM_VK_F7",
	  "F8": "DOM_VK_F8",
	  "F9": "DOM_VK_F9",
	  "F10": "DOM_VK_F10",
	  "F11": "DOM_VK_F11",
	  "F12": "DOM_VK_F12",
	  "F13": "DOM_VK_F13",
	  "F14": "DOM_VK_F14",
	  "F15": "DOM_VK_F15",
	  "F16": "DOM_VK_F16",
	  "F17": "DOM_VK_F17",
	  "F18": "DOM_VK_F18",
	  "F19": "DOM_VK_F19",
	  "F20": "DOM_VK_F20",
	  "F21": "DOM_VK_F21",
	  "F22": "DOM_VK_F22",
	  "F23": "DOM_VK_F23",
	  "F24": "DOM_VK_F24",
	  "Space": "DOM_VK_SPACE",
	  "Backspace": "DOM_VK_BACK_SPACE",
	  "Delete": "DOM_VK_DELETE",
	  "Insert": "DOM_VK_INSERT",
	  "Return": "DOM_VK_RETURN",
	  "Enter": "DOM_VK_RETURN",
	  "Up": "DOM_VK_UP",
	  "Down": "DOM_VK_DOWN",
	  "Left": "DOM_VK_LEFT",
	  "Right": "DOM_VK_RIGHT",
	  "Home": "DOM_VK_HOME",
	  "End": "DOM_VK_END",
	  "PageUp": "DOM_VK_PAGE_UP",
	  "PageDown": "DOM_VK_PAGE_DOWN",
	  "Escape": "DOM_VK_ESCAPE",
	  "Esc": "DOM_VK_ESCAPE",
	  "Tab": "DOM_VK_TAB",
	  "VolumeUp": "DOM_VK_VOLUME_UP",
	  "VolumeDown": "DOM_VK_VOLUME_DOWN",
	  "VolumeMute": "DOM_VK_VOLUME_MUTE",
	  "PrintScreen": "DOM_VK_PRINTSCREEN"
	};

	/**
	 * Helper to listen for keyboard events decribed in .properties file.
	 *
	 * let shortcuts = new KeyShortcuts({
	 *   window
	 * });
	 * shortcuts.on("Ctrl+F", event => {
	 *   // `event` is the KeyboardEvent which relates to the key shortcuts
	 * });
	 *
	 * @param DOMWindow window
	 *        The window object of the document to listen events from.
	 * @param DOMElement target
	 *        Optional DOM Element on which we should listen events from.
	 *        If omitted, we listen for all events fired on `window`.
	 */
	function KeyShortcuts(_ref) {
	  var window = _ref.window,
	      target = _ref.target;

	  this.window = window;
	  this.target = target || window;
	  this.keys = new Map();
	  this.eventEmitter = new EventEmitter();
	  this.target.addEventListener("keydown", this);
	}

	/*
	 * Parse an electron-like key string and return a normalized object which
	 * allow efficient match on DOM key event. The normalized object matches DOM
	 * API.
	 *
	 * @param DOMWindow window
	 *        Any DOM Window object, just to fetch its `KeyboardEvent` object
	 * @param String str
	 *        The shortcut string to parse, following this document:
	 *        https://github.com/electron/electron/blob/master/docs/api/accelerator.md
	 */
	KeyShortcuts.parseElectronKey = function (window, str) {
	  var modifiers = str.split("+");
	  var key = modifiers.pop();

	  var shortcut = {
	    ctrl: false,
	    meta: false,
	    alt: false,
	    shift: false,
	    // Set for character keys
	    key: undefined,
	    // Set for non-character keys
	    keyCode: undefined
	  };
	  for (var mod of modifiers) {
	    if (mod === "Alt") {
	      shortcut.alt = true;
	    } else if (["Command", "Cmd"].includes(mod)) {
	      shortcut.meta = true;
	    } else if (["CommandOrControl", "CmdOrCtrl"].includes(mod)) {
	      if (isOSX) {
	        shortcut.meta = true;
	      } else {
	        shortcut.ctrl = true;
	      }
	    } else if (["Control", "Ctrl"].includes(mod)) {
	      shortcut.ctrl = true;
	    } else if (mod === "Shift") {
	      shortcut.shift = true;
	    } else {
	      console.error("Unsupported modifier:", mod, "from key:", str);
	      return null;
	    }
	  }

	  // Plus is a special case. It's a character key and shouldn't be matched
	  // against a keycode as it is only accessible via Shift/Capslock
	  if (key === "Plus") {
	    key = "+";
	  }

	  if (typeof key === "string" && key.length === 1) {
	    // Match any single character
	    shortcut.key = key.toLowerCase();
	  } else if (key in ElectronKeysMapping) {
	    // Maps the others manually to DOM API DOM_VK_*
	    key = ElectronKeysMapping[key];
	    shortcut.keyCode = window.KeyboardEvent[key];
	    // Used only to stringify the shortcut
	    shortcut.keyCodeString = key;
	    shortcut.key = key;
	  } else {
	    console.error("Unsupported key:", key);
	    return null;
	  }

	  return shortcut;
	};

	KeyShortcuts.stringify = function (shortcut) {
	  var list = [];
	  if (shortcut.alt) {
	    list.push("Alt");
	  }
	  if (shortcut.ctrl) {
	    list.push("Ctrl");
	  }
	  if (shortcut.meta) {
	    list.push("Cmd");
	  }
	  if (shortcut.shift) {
	    list.push("Shift");
	  }
	  var key = void 0;
	  if (shortcut.key) {
	    key = shortcut.key.toUpperCase();
	  } else {
	    key = shortcut.keyCodeString;
	  }
	  list.push(key);
	  return list.join("+");
	};

	KeyShortcuts.prototype = {
	  destroy() {
	    this.target.removeEventListener("keydown", this);
	    this.keys.clear();
	  },

	  doesEventMatchShortcut(event, shortcut) {
	    if (shortcut.meta != event.metaKey) {
	      return false;
	    }
	    if (shortcut.ctrl != event.ctrlKey) {
	      return false;
	    }
	    if (shortcut.alt != event.altKey) {
	      return false;
	    }
	    // Shift is a special modifier, it may implicitely be required if the
	    // expected key is a special character accessible via shift.
	    if (shortcut.shift != event.shiftKey && event.key && event.key.match(/[a-zA-Z]/)) {
	      return false;
	    }
	    if (shortcut.keyCode) {
	      return event.keyCode == shortcut.keyCode;
	    } else if (event.key in ElectronKeysMapping) {
	      return ElectronKeysMapping[event.key] === shortcut.key;
	    }

	    // get the key from the keyCode if key is not provided.
	    var key = event.key || String.fromCharCode(event.keyCode);

	    // For character keys, we match if the final character is the expected one.
	    // But for digits we also accept indirect match to please azerty keyboard,
	    // which requires Shift to be pressed to get digits.
	    return key.toLowerCase() == shortcut.key || shortcut.key.match(/^[0-9]$/) && event.keyCode == shortcut.key.charCodeAt(0);
	  },

	  handleEvent(event) {
	    for (var _ref2 of this.keys) {
	      var _ref3 = _slicedToArray(_ref2, 2);

	      var key = _ref3[0];
	      var shortcut = _ref3[1];

	      if (this.doesEventMatchShortcut(event, shortcut)) {
	        this.eventEmitter.emit(key, event);
	      }
	    }
	  },

	  on(key, listener) {
	    if (typeof listener !== "function") {
	      throw new Error("KeyShortcuts.on() expects a function as " + "second argument");
	    }
	    if (!this.keys.has(key)) {
	      var shortcut = KeyShortcuts.parseElectronKey(this.window, key);
	      // The key string is wrong and we were unable to compute the key shortcut
	      if (!shortcut) {
	        return;
	      }
	      this.keys.set(key, shortcut);
	    }
	    this.eventEmitter.on(key, listener);
	  },

	  off(key, listener) {
	    this.eventEmitter.off(key, listener);
	  }
	};
	exports.KeyShortcuts = KeyShortcuts;

/***/ },

/***/ 1020:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    addBreakpoint = _require.addBreakpoint,
	    assertHighlightLocation = _require.assertHighlightLocation,
	    selectSource = _require.selectSource,
	    invokeInTab = _require.invokeInTab,
	    mouseOverEl = _require.mouseOverEl,
	    findAllElements = _require.findAllElements,
	    waitForPaused = _require.waitForPaused,
	    waitForDispatch = _require.waitForDispatch;

	// Tests that the editor will always highight the right line, no
	// matter if the source text doesn't exist yet or even if the source
	// doesn't exist.

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        EXAMPLE_URL = ctx.EXAMPLE_URL;


	    var dbg = yield initDebugger("doc-scripts.html", "long.js");
	    yield addBreakpoint(dbg, "long.js", 82);

	    invokeInTab(dbg, "toggleModel");
	    yield waitForPaused(dbg);
	    var el = dbg.win.cm.getWrapperElement().querySelectorAll(".CodeMirror-line ")[50].querySelector(".cm-variable-2");

	    mouseOverEl(dbg.win, el);
	    assertPausedLocation(dbg, ctx, "long.js", 82);
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 1021:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    clickElement = _require.clickElement,
	    evalInTab = _require.evalInTab,
	    findElement = _require.findElement,
	    initDebugger = _require.initDebugger,
	    invokeInTab = _require.invokeInTab,
	    resume = _require.resume,
	    stepOver = _require.stepOver,
	    waitForDispatch = _require.waitForDispatch,
	    waitForPaused = _require.waitForPaused,
	    waitForTime = _require.waitForTime;

	function getScopeNodeLabel(dbg, index) {
	  return findElement(dbg, "scopeNode", index).innerText;
	}

	function getScopeNodeValue(dbg, index) {
	  return findElement(dbg, "scopeValue", index).innerText;
	}

	function expandNode(dbg, index) {
	  var onLoadProperties = onLoadObjectProperties(dbg);
	  clickElement(dbg, "scopeNode", index);
	  return onLoadProperties;
	}

	function toggleScopes(dbg) {
	  return findElement(dbg, "scopesHeader").click();
	}

	function onLoadObjectProperties(dbg) {
	  return waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
	}

	module.exports = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info;

	    console.log(">>> starting");
	    var dbg = yield initDebugger("doc-script-mutate.html", "script-mutate");

	    toggleScopes(dbg);

	    var onPaused = waitForPaused(dbg);
	    invokeInTab(dbg, "mutate");
	    yield onPaused;

	    is(getScopeNodeLabel(dbg, 2), "<this>", 'The second element in the scope panel is "<this>"');
	    is(getScopeNodeLabel(dbg, 3), "phonebook", 'The third element in the scope panel is "phonebook"');

	    // Expand `phonebook`
	    yield expandNode(dbg, 3);
	    is(getScopeNodeLabel(dbg, 4), "S", 'The fourth element in the scope panel is "S"');

	    // Expand `S`
	    yield expandNode(dbg, 4);
	    is(getScopeNodeLabel(dbg, 5), "sarah", 'The fifth element in the scope panel is "sarah"');
	    is(getScopeNodeLabel(dbg, 6), "serena", 'The sixth element in the scope panel is "serena"');

	    // Expand `sarah`
	    yield expandNode(dbg, 5);
	    is(getScopeNodeLabel(dbg, 6), "lastName", 'The sixth element in the scope panel is now "lastName"');
	    is(getScopeNodeValue(dbg, 6), '"Doe"', 'The "lastName" element has the expected "Doe" value');

	    info("Resuming");
	    onPaused = waitForPaused(dbg);
	    yield resume(dbg);
	    yield onPaused;
	    is(getScopeNodeLabel(dbg, 2), "<this>", 'The second element in the scope panel is "<this>"');
	    is(getScopeNodeLabel(dbg, 3), "phonebook", 'The third element in the scope panel is "phonebook"');
	  });

	  return function (_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

/***/ },

/***/ 1022:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	Object.defineProperty(exports, "__esModule", {
	  value: true
	});

	var addTabs = exports.addTabs = (() => {
	  var _ref = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        requestLongerTimeout = ctx.requestLongerTimeout;


	    var dbg = yield initDebugger("doc-scripts.html", "simple1", "simple2");

	    yield selectSource(dbg, "simple1");
	    yield selectSource(dbg, "simple2");
	    expect(countTabs(dbg)).to.equal(2);

	    // Test reloading the debugger
	    dbg = yield initDebugger("doc-scripts.html", "simple1", "simple2");
	    expect(countTabs(dbg)).to.equal(2);
	    yield selectSource(dbg, "simple2");

	    // Test reloading the debuggee
	    yield reload(dbg, "simple1", "simple2");
	    yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");
	    expect(countTabs(dbg)).to.equal(2);
	  });

	  return function addTabs(_x) {
	    return _ref.apply(this, arguments);
	  };
	})();

	var reloadWithTabs = exports.reloadWithTabs = (() => {
	  var _ref2 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        requestLongerTimeout = ctx.requestLongerTimeout;


	    var dbg = yield initDebugger("doc-scripts.html", "simple1", "simple2");

	    // debugger
	    yield selectSource(dbg, "simple1");
	    yield selectSource(dbg, "simple2");
	    expect(countTabs(dbg)).to.equal(2);

	    // Test reloading the debugger
	    yield reload(dbg, "simple1", "simple2");
	    yield waitForDispatch(dbg, "SELECT_SOURCE");
	    expect(countTabs(dbg)).to.equal(2);
	  });

	  return function reloadWithTabs(_x2) {
	    return _ref2.apply(this, arguments);
	  };
	})();

	var reloadWithNoTabs = exports.reloadWithNoTabs = (() => {
	  var _ref3 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        requestLongerTimeout = ctx.requestLongerTimeout;


	    var dbg = yield initDebugger("doc-scripts.html", "simple1", "simple2");

	    yield selectSource(dbg, "simple1");
	    yield selectSource(dbg, "simple2");
	    closeTab(dbg, "simple1");
	    closeTab(dbg, "simple2");

	    // Test reloading the debugger
	    dbg = yield initDebugger("doc-scripts.html", "simple1", "simple2");
	    expect(countTabs(dbg)).to.equal(0);
	  });

	  return function reloadWithNoTabs(_x3) {
	    return _ref3.apply(this, arguments);
	  };
	})();

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    reload = _require.reload,
	    selectSource = _require.selectSource,
	    findElement = _require.findElement,
	    waitForDispatch = _require.waitForDispatch,
	    closeTab = _require.closeTab;

	function countTabs(dbg) {
	  return findElement(dbg, "sourceTabs").children.length;
	}

/***/ },

/***/ 1129:
/***/ function(module, exports, __webpack_require__) {

	"use strict";

	function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

	var _require = __webpack_require__(760),
	    initDebugger = _require.initDebugger,
	    reload = _require.reload,
	    selectSource = _require.selectSource,
	    findElement = _require.findElement,
	    waitForDispatch = _require.waitForDispatch,
	    closeTab = _require.closeTab,
	    assertPausedLocation = _require.assertPausedLocation,
	    findSource = _require.findSource,
	    waitForSources = _require.waitForSources,
	    disableBreakpoint = _require.disableBreakpoint,
	    addBreakpoint = _require.addBreakpoint,
	    waitForPaused = _require.waitForPaused;

	function assertBP(dbg, sourceId, _ref) {
	  var line = _ref.line,
	      _ref$disabled = _ref.disabled,
	      disabled = _ref$disabled === undefined ? false : _ref$disabled;
	  var _dbg$selectors = dbg.selectors,
	      getBreakpoint = _dbg$selectors.getBreakpoint,
	      getBreakpoints = _dbg$selectors.getBreakpoints,
	      getState = dbg.getState;

	  var bp = getBreakpoint(getState(), { sourceId, line });

	  is(bp.location.line, line, "Breakpoint has correct line");

	  is(bp.disabled, disabled);
	}

	module.exports = (() => {
	  var _ref2 = _asyncToGenerator(function* (ctx) {
	    var ok = ctx.ok,
	        is = ctx.is,
	        info = ctx.info,
	        requestLongerTimeout = ctx.requestLongerTimeout;


	    var dbg = yield initDebugger("doc-sourcemaps.html");
	    var _dbg$selectors2 = dbg.selectors,
	        getBreakpoint = _dbg$selectors2.getBreakpoint,
	        getBreakpoints = _dbg$selectors2.getBreakpoints,
	        getState = dbg.getState;


	    yield waitForSources(dbg, "entry.js", "output.js", "times2.js", "opts.js");
	    var entrySrc = findSource(dbg, "entry.js");
	    yield selectSource(dbg, entrySrc);

	    yield addBreakpoint(dbg, entrySrc, 13);
	    is(getBreakpoints(getState()).size, 1, "One breakpoint exists");
	    assertBP(dbg, entrySrc.id, { line: 13 });

	    yield addBreakpoint(dbg, entrySrc, 15);
	    yield disableBreakpoint(dbg, entrySrc, 15);

	    // Test reloading the debugger
	    yield reload(dbg, "times2.js", "opts.js");
	    yield waitForDispatch(dbg, "LOAD_SOURCE_TEXT");

	    is(getBreakpoints(getState()).size, 2, "One breakpoint exists");
	    assertBP(dbg, entrySrc.id, { line: 13 });
	    assertBP(dbg, entrySrc.id, { line: 15, disabled: true });
	  });

	  function sourceMapsReloading(_x) {
	    return _ref2.apply(this, arguments);
	  }

	  return sourceMapsReloading;
	})();

/***/ }

/******/ })
});
;