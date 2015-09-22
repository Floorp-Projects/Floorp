function run_test() {
  for (var k in SOURCE_MAP_TEST_MODULE) {
    if (/^test/.test(k)) {
      SOURCE_MAP_TEST_MODULE[k](assert);
    }
  }
}


var SOURCE_MAP_TEST_MODULE =
/******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId])
/******/ 			return installedModules[moduleId].exports;
/******/
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			exports: {},
/******/ 			id: moduleId,
/******/ 			loaded: false
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
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
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var ArraySet = __webpack_require__(1).ArraySet;
	
	  function makeTestSet() {
	    var set = new ArraySet();
	    for (var i = 0; i < 100; i++) {
	      set.add(String(i));
	    }
	    return set;
	  }
	
	  exports['test .has() membership'] = function (assert) {
	    var set = makeTestSet();
	    for (var i = 0; i < 100; i++) {
	      assert.ok(set.has(String(i)));
	    }
	  };
	
	  exports['test .indexOf() elements'] = function (assert) {
	    var set = makeTestSet();
	    for (var i = 0; i < 100; i++) {
	      assert.strictEqual(set.indexOf(String(i)), i);
	    }
	  };
	
	  exports['test .at() indexing'] = function (assert) {
	    var set = makeTestSet();
	    for (var i = 0; i < 100; i++) {
	      assert.strictEqual(set.at(i), String(i));
	    }
	  };
	
	  exports['test creating from an array'] = function (assert) {
	    var set = ArraySet.fromArray(['foo', 'bar', 'baz', 'quux', 'hasOwnProperty']);
	
	    assert.ok(set.has('foo'));
	    assert.ok(set.has('bar'));
	    assert.ok(set.has('baz'));
	    assert.ok(set.has('quux'));
	    assert.ok(set.has('hasOwnProperty'));
	
	    assert.strictEqual(set.indexOf('foo'), 0);
	    assert.strictEqual(set.indexOf('bar'), 1);
	    assert.strictEqual(set.indexOf('baz'), 2);
	    assert.strictEqual(set.indexOf('quux'), 3);
	
	    assert.strictEqual(set.at(0), 'foo');
	    assert.strictEqual(set.at(1), 'bar');
	    assert.strictEqual(set.at(2), 'baz');
	    assert.strictEqual(set.at(3), 'quux');
	  };
	
	  exports['test that you can add __proto__; see github issue #30'] = function (assert) {
	    var set = new ArraySet();
	    set.add('__proto__');
	    assert.ok(set.has('__proto__'));
	    assert.strictEqual(set.at(0), '__proto__');
	    assert.strictEqual(set.indexOf('__proto__'), 0);
	  };
	
	  exports['test .fromArray() with duplicates'] = function (assert) {
	    var set = ArraySet.fromArray(['foo', 'foo']);
	    assert.ok(set.has('foo'));
	    assert.strictEqual(set.at(0), 'foo');
	    assert.strictEqual(set.indexOf('foo'), 0);
	    assert.strictEqual(set.toArray().length, 1);
	
	    set = ArraySet.fromArray(['foo', 'foo'], true);
	    assert.ok(set.has('foo'));
	    assert.strictEqual(set.at(0), 'foo');
	    assert.strictEqual(set.at(1), 'foo');
	    assert.strictEqual(set.indexOf('foo'), 0);
	    assert.strictEqual(set.toArray().length, 2);
	  };
	
	  exports['test .add() with duplicates'] = function (assert) {
	    var set = new ArraySet();
	    set.add('foo');
	
	    set.add('foo');
	    assert.ok(set.has('foo'));
	    assert.strictEqual(set.at(0), 'foo');
	    assert.strictEqual(set.indexOf('foo'), 0);
	    assert.strictEqual(set.toArray().length, 1);
	
	    set.add('foo', true);
	    assert.ok(set.has('foo'));
	    assert.strictEqual(set.at(0), 'foo');
	    assert.strictEqual(set.at(1), 'foo');
	    assert.strictEqual(set.indexOf('foo'), 0);
	    assert.strictEqual(set.toArray().length, 2);
	  };
	
	  exports['test .size()'] = function (assert) {
	    var set = new ArraySet();
	    set.add('foo');
	    set.add('bar');
	    set.add('baz');
	    assert.strictEqual(set.size(), 3);
	  };
	
	  exports['test .size() with disallowed duplicates'] = function (assert) {
	    var set = new ArraySet();
	
	    set.add('foo');
	    set.add('foo');
	
	    set.add('bar');
	    set.add('bar');
	
	    set.add('baz');
	    set.add('baz');
	
	    assert.strictEqual(set.size(), 3);
	  };
	
	  exports['test .size() with allowed duplicates'] = function (assert) {
	    var set = new ArraySet();
	
	    set.add('foo');
	    set.add('foo', true);
	
	    set.add('bar');
	    set.add('bar', true);
	
	    set.add('baz');
	    set.add('baz', true);
	
	    assert.strictEqual(set.size(), 3);
	  };
	}


/***/ },
/* 1 */
/***/ function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  var util = __webpack_require__(2);
	
	  /**
	   * A data structure which is a combination of an array and a set. Adding a new
	   * member is O(1), testing for membership is O(1), and finding the index of an
	   * element is O(1). Removing elements from the set is not supported. Only
	   * strings are supported for membership.
	   */
	  function ArraySet() {
	    this._array = [];
	    this._set = {};
	  }
	
	  /**
	   * Static method for creating ArraySet instances from an existing array.
	   */
	  ArraySet.fromArray = function ArraySet_fromArray(aArray, aAllowDuplicates) {
	    var set = new ArraySet();
	    for (var i = 0, len = aArray.length; i < len; i++) {
	      set.add(aArray[i], aAllowDuplicates);
	    }
	    return set;
	  };
	
	  /**
	   * Return how many unique items are in this ArraySet. If duplicates have been
	   * added, than those do not count towards the size.
	   *
	   * @returns Number
	   */
	  ArraySet.prototype.size = function ArraySet_size() {
	    return Object.getOwnPropertyNames(this._set).length;
	  };
	
	  /**
	   * Add the given string to this set.
	   *
	   * @param String aStr
	   */
	  ArraySet.prototype.add = function ArraySet_add(aStr, aAllowDuplicates) {
	    var sStr = util.toSetString(aStr);
	    var isDuplicate = this._set.hasOwnProperty(sStr);
	    var idx = this._array.length;
	    if (!isDuplicate || aAllowDuplicates) {
	      this._array.push(aStr);
	    }
	    if (!isDuplicate) {
	      this._set[sStr] = idx;
	    }
	  };
	
	  /**
	   * Is the given string a member of this set?
	   *
	   * @param String aStr
	   */
	  ArraySet.prototype.has = function ArraySet_has(aStr) {
	    var sStr = util.toSetString(aStr);
	    return this._set.hasOwnProperty(sStr);
	  };
	
	  /**
	   * What is the index of the given string in the array?
	   *
	   * @param String aStr
	   */
	  ArraySet.prototype.indexOf = function ArraySet_indexOf(aStr) {
	    var sStr = util.toSetString(aStr);
	    if (this._set.hasOwnProperty(sStr)) {
	      return this._set[sStr];
	    }
	    throw new Error('"' + aStr + '" is not in the set.');
	  };
	
	  /**
	   * What is the element at the given index?
	   *
	   * @param Number aIdx
	   */
	  ArraySet.prototype.at = function ArraySet_at(aIdx) {
	    if (aIdx >= 0 && aIdx < this._array.length) {
	      return this._array[aIdx];
	    }
	    throw new Error('No element indexed by ' + aIdx);
	  };
	
	  /**
	   * Returns the array representation of this set (which has the proper indices
	   * indicated by indexOf). Note that this is a copy of the internal array used
	   * for storing the members so that no one can mess with internal state.
	   */
	  ArraySet.prototype.toArray = function ArraySet_toArray() {
	    return this._array.slice();
	  };
	
	  exports.ArraySet = ArraySet;
	}


/***/ },
/* 2 */
/***/ function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	{
	  /**
	   * This is a helper function for getting values from parameter/options
	   * objects.
	   *
	   * @param args The object we are extracting values from
	   * @param name The name of the property we are getting.
	   * @param defaultValue An optional value to return if the property is missing
	   * from the object. If this is not specified and the property is missing, an
	   * error will be thrown.
	   */
	  function getArg(aArgs, aName, aDefaultValue) {
	    if (aName in aArgs) {
	      return aArgs[aName];
	    } else if (arguments.length === 3) {
	      return aDefaultValue;
	    } else {
	      throw new Error('"' + aName + '" is a required argument.');
	    }
	  }
	  exports.getArg = getArg;
	
	  var urlRegexp = /^(?:([\w+\-.]+):)?\/\/(?:(\w+:\w+)@)?([\w.]*)(?::(\d+))?(\S*)$/;
	  var dataUrlRegexp = /^data:.+\,.+$/;
	
	  function urlParse(aUrl) {
	    var match = aUrl.match(urlRegexp);
	    if (!match) {
	      return null;
	    }
	    return {
	      scheme: match[1],
	      auth: match[2],
	      host: match[3],
	      port: match[4],
	      path: match[5]
	    };
	  }
	  exports.urlParse = urlParse;
	
	  function urlGenerate(aParsedUrl) {
	    var url = '';
	    if (aParsedUrl.scheme) {
	      url += aParsedUrl.scheme + ':';
	    }
	    url += '//';
	    if (aParsedUrl.auth) {
	      url += aParsedUrl.auth + '@';
	    }
	    if (aParsedUrl.host) {
	      url += aParsedUrl.host;
	    }
	    if (aParsedUrl.port) {
	      url += ":" + aParsedUrl.port
	    }
	    if (aParsedUrl.path) {
	      url += aParsedUrl.path;
	    }
	    return url;
	  }
	  exports.urlGenerate = urlGenerate;
	
	  /**
	   * Normalizes a path, or the path portion of a URL:
	   *
	   * - Replaces consequtive slashes with one slash.
	   * - Removes unnecessary '.' parts.
	   * - Removes unnecessary '<dir>/..' parts.
	   *
	   * Based on code in the Node.js 'path' core module.
	   *
	   * @param aPath The path or url to normalize.
	   */
	  function normalize(aPath) {
	    var path = aPath;
	    var url = urlParse(aPath);
	    if (url) {
	      if (!url.path) {
	        return aPath;
	      }
	      path = url.path;
	    }
	    var isAbsolute = exports.isAbsolute(path);
	
	    var parts = path.split(/\/+/);
	    for (var part, up = 0, i = parts.length - 1; i >= 0; i--) {
	      part = parts[i];
	      if (part === '.') {
	        parts.splice(i, 1);
	      } else if (part === '..') {
	        up++;
	      } else if (up > 0) {
	        if (part === '') {
	          // The first part is blank if the path is absolute. Trying to go
	          // above the root is a no-op. Therefore we can remove all '..' parts
	          // directly after the root.
	          parts.splice(i + 1, up);
	          up = 0;
	        } else {
	          parts.splice(i, 2);
	          up--;
	        }
	      }
	    }
	    path = parts.join('/');
	
	    if (path === '') {
	      path = isAbsolute ? '/' : '.';
	    }
	
	    if (url) {
	      url.path = path;
	      return urlGenerate(url);
	    }
	    return path;
	  }
	  exports.normalize = normalize;
	
	  /**
	   * Joins two paths/URLs.
	   *
	   * @param aRoot The root path or URL.
	   * @param aPath The path or URL to be joined with the root.
	   *
	   * - If aPath is a URL or a data URI, aPath is returned, unless aPath is a
	   *   scheme-relative URL: Then the scheme of aRoot, if any, is prepended
	   *   first.
	   * - Otherwise aPath is a path. If aRoot is a URL, then its path portion
	   *   is updated with the result and aRoot is returned. Otherwise the result
	   *   is returned.
	   *   - If aPath is absolute, the result is aPath.
	   *   - Otherwise the two paths are joined with a slash.
	   * - Joining for example 'http://' and 'www.example.com' is also supported.
	   */
	  function join(aRoot, aPath) {
	    if (aRoot === "") {
	      aRoot = ".";
	    }
	    if (aPath === "") {
	      aPath = ".";
	    }
	    var aPathUrl = urlParse(aPath);
	    var aRootUrl = urlParse(aRoot);
	    if (aRootUrl) {
	      aRoot = aRootUrl.path || '/';
	    }
	
	    // `join(foo, '//www.example.org')`
	    if (aPathUrl && !aPathUrl.scheme) {
	      if (aRootUrl) {
	        aPathUrl.scheme = aRootUrl.scheme;
	      }
	      return urlGenerate(aPathUrl);
	    }
	
	    if (aPathUrl || aPath.match(dataUrlRegexp)) {
	      return aPath;
	    }
	
	    // `join('http://', 'www.example.com')`
	    if (aRootUrl && !aRootUrl.host && !aRootUrl.path) {
	      aRootUrl.host = aPath;
	      return urlGenerate(aRootUrl);
	    }
	
	    var joined = aPath.charAt(0) === '/'
	      ? aPath
	      : normalize(aRoot.replace(/\/+$/, '') + '/' + aPath);
	
	    if (aRootUrl) {
	      aRootUrl.path = joined;
	      return urlGenerate(aRootUrl);
	    }
	    return joined;
	  }
	  exports.join = join;
	
	  exports.isAbsolute = function (aPath) {
	    return aPath.charAt(0) === '/' || !!aPath.match(urlRegexp);
	  };
	
	  /**
	   * Make a path relative to a URL or another path.
	   *
	   * @param aRoot The root path or URL.
	   * @param aPath The path or URL to be made relative to aRoot.
	   */
	  function relative(aRoot, aPath) {
	    if (aRoot === "") {
	      aRoot = ".";
	    }
	
	    aRoot = aRoot.replace(/\/$/, '');
	
	    // It is possible for the path to be above the root. In this case, simply
	    // checking whether the root is a prefix of the path won't work. Instead, we
	    // need to remove components from the root one by one, until either we find
	    // a prefix that fits, or we run out of components to remove.
	    var level = 0;
	    while (aPath.indexOf(aRoot + '/') !== 0) {
	      var index = aRoot.lastIndexOf("/");
	      if (index < 0) {
	        return aPath;
	      }
	
	      // If the only part of the root that is left is the scheme (i.e. http://,
	      // file:///, etc.), one or more slashes (/), or simply nothing at all, we
	      // have exhausted all components, so the path is not relative to the root.
	      aRoot = aRoot.slice(0, index);
	      if (aRoot.match(/^([^\/]+:\/)?\/*$/)) {
	        return aPath;
	      }
	
	      ++level;
	    }
	
	    // Make sure we add a "../" for each component we removed from the root.
	    return Array(level + 1).join("../") + aPath.substr(aRoot.length + 1);
	  }
	  exports.relative = relative;
	
	  /**
	   * Because behavior goes wacky when you set `__proto__` on objects, we
	   * have to prefix all the strings in our set with an arbitrary character.
	   *
	   * See https://github.com/mozilla/source-map/pull/31 and
	   * https://github.com/mozilla/source-map/issues/30
	   *
	   * @param String aStr
	   */
	  function toSetString(aStr) {
	    return '$' + aStr;
	  }
	  exports.toSetString = toSetString;
	
	  function fromSetString(aStr) {
	    return aStr.substr(1);
	  }
	  exports.fromSetString = fromSetString;
	
	  /**
	   * Comparator between two mappings where the original positions are compared.
	   *
	   * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	   * mappings with the same original source/line/column, but different generated
	   * line and column the same. Useful when searching for a mapping with a
	   * stubbed out mapping.
	   */
	  function compareByOriginalPositions(mappingA, mappingB, onlyCompareOriginal) {
	    var cmp = mappingA.source - mappingB.source;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalLine - mappingB.originalLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalColumn - mappingB.originalColumn;
	    if (cmp !== 0 || onlyCompareOriginal) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedLine - mappingB.generatedLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    return mappingA.name - mappingB.name;
	  }
	  exports.compareByOriginalPositions = compareByOriginalPositions;
	
	  /**
	   * Comparator between two mappings with deflated source and name indices where
	   * the generated positions are compared.
	   *
	   * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	   * mappings with the same generated line and column, but different
	   * source/name/original line and column the same. Useful when searching for a
	   * mapping with a stubbed out mapping.
	   */
	  function compareByGeneratedPositionsDeflated(mappingA, mappingB, onlyCompareGenerated) {
	    var cmp = mappingA.generatedLine - mappingB.generatedLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	    if (cmp !== 0 || onlyCompareGenerated) {
	      return cmp;
	    }
	
	    cmp = mappingA.source - mappingB.source;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalLine - mappingB.originalLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalColumn - mappingB.originalColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    return mappingA.name - mappingB.name;
	  }
	  exports.compareByGeneratedPositionsDeflated = compareByGeneratedPositionsDeflated;
	
	  function strcmp(aStr1, aStr2) {
	    if (aStr1 === aStr2) {
	      return 0;
	    }
	
	    if (aStr1 > aStr2) {
	      return 1;
	    }
	
	    return -1;
	  }
	
	  /**
	   * Comparator between two mappings with inflated source and name strings where
	   * the generated positions are compared.
	   */
	  function compareByGeneratedPositionsInflated(mappingA, mappingB) {
	    var cmp = mappingA.generatedLine - mappingB.generatedLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.generatedColumn - mappingB.generatedColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = strcmp(mappingA.source, mappingB.source);
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalLine - mappingB.originalLine;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    cmp = mappingA.originalColumn - mappingB.originalColumn;
	    if (cmp !== 0) {
	      return cmp;
	    }
	
	    return strcmp(mappingA.name, mappingB.name);
	  }
	  exports.compareByGeneratedPositionsInflated = compareByGeneratedPositionsInflated;
	}


/***/ }
/******/ ]);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgZjFlMTE4YWJkNTdmZDlhZDAzNTIiLCJ3ZWJwYWNrOi8vLy4vdGVzdC90ZXN0LWFycmF5LXNldC5qcyIsIndlYnBhY2s6Ly8vLi9saWIvYXJyYXktc2V0LmpzIiwid2VicGFjazovLy8uL2xpYi91dGlsLmpzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiI7Ozs7Ozs7Ozs7O0FBQUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0EsdUJBQWU7QUFDZjtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7O0FBR0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7Ozs7OztBQ3RDQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSxvQkFBbUIsU0FBUztBQUM1QjtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0Esb0JBQW1CLFNBQVM7QUFDNUI7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSxvQkFBbUIsU0FBUztBQUM1QjtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBLG9CQUFtQixTQUFTO0FBQzVCO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQSw0Q0FBMkM7QUFDM0M7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7Ozs7Ozs7QUN4SUEsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EseUNBQXdDLFNBQVM7QUFDakQ7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7Ozs7Ozs7QUN2R0EsaUJBQWdCLG9CQUFvQjtBQUNwQztBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBLE1BQUs7QUFDTDtBQUNBLE1BQUs7QUFDTDtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBLGlEQUFnRCxRQUFRO0FBQ3hEO0FBQ0E7QUFDQTtBQUNBLFFBQU87QUFDUDtBQUNBLFFBQU87QUFDUDtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxVQUFTO0FBQ1Q7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQSIsImZpbGUiOiJ0ZXN0X2FycmF5X3NldC5qcyIsInNvdXJjZXNDb250ZW50IjpbIiBcdC8vIFRoZSBtb2R1bGUgY2FjaGVcbiBcdHZhciBpbnN0YWxsZWRNb2R1bGVzID0ge307XG5cbiBcdC8vIFRoZSByZXF1aXJlIGZ1bmN0aW9uXG4gXHRmdW5jdGlvbiBfX3dlYnBhY2tfcmVxdWlyZV9fKG1vZHVsZUlkKSB7XG5cbiBcdFx0Ly8gQ2hlY2sgaWYgbW9kdWxlIGlzIGluIGNhY2hlXG4gXHRcdGlmKGluc3RhbGxlZE1vZHVsZXNbbW9kdWxlSWRdKVxuIFx0XHRcdHJldHVybiBpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXS5leHBvcnRzO1xuXG4gXHRcdC8vIENyZWF0ZSBhIG5ldyBtb2R1bGUgKGFuZCBwdXQgaXQgaW50byB0aGUgY2FjaGUpXG4gXHRcdHZhciBtb2R1bGUgPSBpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXSA9IHtcbiBcdFx0XHRleHBvcnRzOiB7fSxcbiBcdFx0XHRpZDogbW9kdWxlSWQsXG4gXHRcdFx0bG9hZGVkOiBmYWxzZVxuIFx0XHR9O1xuXG4gXHRcdC8vIEV4ZWN1dGUgdGhlIG1vZHVsZSBmdW5jdGlvblxuIFx0XHRtb2R1bGVzW21vZHVsZUlkXS5jYWxsKG1vZHVsZS5leHBvcnRzLCBtb2R1bGUsIG1vZHVsZS5leHBvcnRzLCBfX3dlYnBhY2tfcmVxdWlyZV9fKTtcblxuIFx0XHQvLyBGbGFnIHRoZSBtb2R1bGUgYXMgbG9hZGVkXG4gXHRcdG1vZHVsZS5sb2FkZWQgPSB0cnVlO1xuXG4gXHRcdC8vIFJldHVybiB0aGUgZXhwb3J0cyBvZiB0aGUgbW9kdWxlXG4gXHRcdHJldHVybiBtb2R1bGUuZXhwb3J0cztcbiBcdH1cblxuXG4gXHQvLyBleHBvc2UgdGhlIG1vZHVsZXMgb2JqZWN0IChfX3dlYnBhY2tfbW9kdWxlc19fKVxuIFx0X193ZWJwYWNrX3JlcXVpcmVfXy5tID0gbW9kdWxlcztcblxuIFx0Ly8gZXhwb3NlIHRoZSBtb2R1bGUgY2FjaGVcbiBcdF9fd2VicGFja19yZXF1aXJlX18uYyA9IGluc3RhbGxlZE1vZHVsZXM7XG5cbiBcdC8vIF9fd2VicGFja19wdWJsaWNfcGF0aF9fXG4gXHRfX3dlYnBhY2tfcmVxdWlyZV9fLnAgPSBcIlwiO1xuXG4gXHQvLyBMb2FkIGVudHJ5IG1vZHVsZSBhbmQgcmV0dXJuIGV4cG9ydHNcbiBcdHJldHVybiBfX3dlYnBhY2tfcmVxdWlyZV9fKDApO1xuXG5cblxuLyoqIFdFQlBBQ0sgRk9PVEVSICoqXG4gKiogd2VicGFjay9ib290c3RyYXAgZjFlMTE4YWJkNTdmZDlhZDAzNTJcbiAqKi8iLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG57XG4gIHZhciBBcnJheVNldCA9IHJlcXVpcmUoJy4uL2xpYi9hcnJheS1zZXQnKS5BcnJheVNldDtcblxuICBmdW5jdGlvbiBtYWtlVGVzdFNldCgpIHtcbiAgICB2YXIgc2V0ID0gbmV3IEFycmF5U2V0KCk7XG4gICAgZm9yICh2YXIgaSA9IDA7IGkgPCAxMDA7IGkrKykge1xuICAgICAgc2V0LmFkZChTdHJpbmcoaSkpO1xuICAgIH1cbiAgICByZXR1cm4gc2V0O1xuICB9XG5cbiAgZXhwb3J0c1sndGVzdCAuaGFzKCkgbWVtYmVyc2hpcCddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBzZXQgPSBtYWtlVGVzdFNldCgpO1xuICAgIGZvciAodmFyIGkgPSAwOyBpIDwgMTAwOyBpKyspIHtcbiAgICAgIGFzc2VydC5vayhzZXQuaGFzKFN0cmluZyhpKSkpO1xuICAgIH1cbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IC5pbmRleE9mKCkgZWxlbWVudHMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgc2V0ID0gbWFrZVRlc3RTZXQoKTtcbiAgICBmb3IgKHZhciBpID0gMDsgaSA8IDEwMDsgaSsrKSB7XG4gICAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmluZGV4T2YoU3RyaW5nKGkpKSwgaSk7XG4gICAgfVxuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgLmF0KCkgaW5kZXhpbmcnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgc2V0ID0gbWFrZVRlc3RTZXQoKTtcbiAgICBmb3IgKHZhciBpID0gMDsgaSA8IDEwMDsgaSsrKSB7XG4gICAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmF0KGkpLCBTdHJpbmcoaSkpO1xuICAgIH1cbiAgfTtcblxuICBleHBvcnRzWyd0ZXN0IGNyZWF0aW5nIGZyb20gYW4gYXJyYXknXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgc2V0ID0gQXJyYXlTZXQuZnJvbUFycmF5KFsnZm9vJywgJ2JhcicsICdiYXonLCAncXV1eCcsICdoYXNPd25Qcm9wZXJ0eSddKTtcblxuICAgIGFzc2VydC5vayhzZXQuaGFzKCdmb28nKSk7XG4gICAgYXNzZXJ0Lm9rKHNldC5oYXMoJ2JhcicpKTtcbiAgICBhc3NlcnQub2soc2V0LmhhcygnYmF6JykpO1xuICAgIGFzc2VydC5vayhzZXQuaGFzKCdxdXV4JykpO1xuICAgIGFzc2VydC5vayhzZXQuaGFzKCdoYXNPd25Qcm9wZXJ0eScpKTtcblxuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuaW5kZXhPZignZm9vJyksIDApO1xuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuaW5kZXhPZignYmFyJyksIDEpO1xuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuaW5kZXhPZignYmF6JyksIDIpO1xuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuaW5kZXhPZigncXV1eCcpLCAzKTtcblxuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuYXQoMCksICdmb28nKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmF0KDEpLCAnYmFyJyk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC5hdCgyKSwgJ2JheicpO1xuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuYXQoMyksICdxdXV4Jyk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCB0aGF0IHlvdSBjYW4gYWRkIF9fcHJvdG9fXzsgc2VlIGdpdGh1YiBpc3N1ZSAjMzAnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgICB2YXIgc2V0ID0gbmV3IEFycmF5U2V0KCk7XG4gICAgc2V0LmFkZCgnX19wcm90b19fJyk7XG4gICAgYXNzZXJ0Lm9rKHNldC5oYXMoJ19fcHJvdG9fXycpKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmF0KDApLCAnX19wcm90b19fJyk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC5pbmRleE9mKCdfX3Byb3RvX18nKSwgMCk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCAuZnJvbUFycmF5KCkgd2l0aCBkdXBsaWNhdGVzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIHNldCA9IEFycmF5U2V0LmZyb21BcnJheShbJ2ZvbycsICdmb28nXSk7XG4gICAgYXNzZXJ0Lm9rKHNldC5oYXMoJ2ZvbycpKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmF0KDApLCAnZm9vJyk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC5pbmRleE9mKCdmb28nKSwgMCk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC50b0FycmF5KCkubGVuZ3RoLCAxKTtcblxuICAgIHNldCA9IEFycmF5U2V0LmZyb21BcnJheShbJ2ZvbycsICdmb28nXSwgdHJ1ZSk7XG4gICAgYXNzZXJ0Lm9rKHNldC5oYXMoJ2ZvbycpKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmF0KDApLCAnZm9vJyk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC5hdCgxKSwgJ2ZvbycpO1xuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuaW5kZXhPZignZm9vJyksIDApO1xuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQudG9BcnJheSgpLmxlbmd0aCwgMik7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCAuYWRkKCkgd2l0aCBkdXBsaWNhdGVzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIHNldCA9IG5ldyBBcnJheVNldCgpO1xuICAgIHNldC5hZGQoJ2ZvbycpO1xuXG4gICAgc2V0LmFkZCgnZm9vJyk7XG4gICAgYXNzZXJ0Lm9rKHNldC5oYXMoJ2ZvbycpKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmF0KDApLCAnZm9vJyk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC5pbmRleE9mKCdmb28nKSwgMCk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC50b0FycmF5KCkubGVuZ3RoLCAxKTtcblxuICAgIHNldC5hZGQoJ2ZvbycsIHRydWUpO1xuICAgIGFzc2VydC5vayhzZXQuaGFzKCdmb28nKSk7XG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC5hdCgwKSwgJ2ZvbycpO1xuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuYXQoMSksICdmb28nKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LmluZGV4T2YoJ2ZvbycpLCAwKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LnRvQXJyYXkoKS5sZW5ndGgsIDIpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgLnNpemUoKSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBzZXQgPSBuZXcgQXJyYXlTZXQoKTtcbiAgICBzZXQuYWRkKCdmb28nKTtcbiAgICBzZXQuYWRkKCdiYXInKTtcbiAgICBzZXQuYWRkKCdiYXonKTtcbiAgICBhc3NlcnQuc3RyaWN0RXF1YWwoc2V0LnNpemUoKSwgMyk7XG4gIH07XG5cbiAgZXhwb3J0c1sndGVzdCAuc2l6ZSgpIHdpdGggZGlzYWxsb3dlZCBkdXBsaWNhdGVzJ10gPSBmdW5jdGlvbiAoYXNzZXJ0KSB7XG4gICAgdmFyIHNldCA9IG5ldyBBcnJheVNldCgpO1xuXG4gICAgc2V0LmFkZCgnZm9vJyk7XG4gICAgc2V0LmFkZCgnZm9vJyk7XG5cbiAgICBzZXQuYWRkKCdiYXInKTtcbiAgICBzZXQuYWRkKCdiYXInKTtcblxuICAgIHNldC5hZGQoJ2JheicpO1xuICAgIHNldC5hZGQoJ2JheicpO1xuXG4gICAgYXNzZXJ0LnN0cmljdEVxdWFsKHNldC5zaXplKCksIDMpO1xuICB9O1xuXG4gIGV4cG9ydHNbJ3Rlc3QgLnNpemUoKSB3aXRoIGFsbG93ZWQgZHVwbGljYXRlcyddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAgIHZhciBzZXQgPSBuZXcgQXJyYXlTZXQoKTtcblxuICAgIHNldC5hZGQoJ2ZvbycpO1xuICAgIHNldC5hZGQoJ2ZvbycsIHRydWUpO1xuXG4gICAgc2V0LmFkZCgnYmFyJyk7XG4gICAgc2V0LmFkZCgnYmFyJywgdHJ1ZSk7XG5cbiAgICBzZXQuYWRkKCdiYXonKTtcbiAgICBzZXQuYWRkKCdiYXonLCB0cnVlKTtcblxuICAgIGFzc2VydC5zdHJpY3RFcXVhbChzZXQuc2l6ZSgpLCAzKTtcbiAgfTtcbn1cblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi90ZXN0L3Rlc3QtYXJyYXktc2V0LmpzXG4gKiogbW9kdWxlIGlkID0gMFxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICB2YXIgdXRpbCA9IHJlcXVpcmUoJy4vdXRpbCcpO1xuXG4gIC8qKlxuICAgKiBBIGRhdGEgc3RydWN0dXJlIHdoaWNoIGlzIGEgY29tYmluYXRpb24gb2YgYW4gYXJyYXkgYW5kIGEgc2V0LiBBZGRpbmcgYSBuZXdcbiAgICogbWVtYmVyIGlzIE8oMSksIHRlc3RpbmcgZm9yIG1lbWJlcnNoaXAgaXMgTygxKSwgYW5kIGZpbmRpbmcgdGhlIGluZGV4IG9mIGFuXG4gICAqIGVsZW1lbnQgaXMgTygxKS4gUmVtb3ZpbmcgZWxlbWVudHMgZnJvbSB0aGUgc2V0IGlzIG5vdCBzdXBwb3J0ZWQuIE9ubHlcbiAgICogc3RyaW5ncyBhcmUgc3VwcG9ydGVkIGZvciBtZW1iZXJzaGlwLlxuICAgKi9cbiAgZnVuY3Rpb24gQXJyYXlTZXQoKSB7XG4gICAgdGhpcy5fYXJyYXkgPSBbXTtcbiAgICB0aGlzLl9zZXQgPSB7fTtcbiAgfVxuXG4gIC8qKlxuICAgKiBTdGF0aWMgbWV0aG9kIGZvciBjcmVhdGluZyBBcnJheVNldCBpbnN0YW5jZXMgZnJvbSBhbiBleGlzdGluZyBhcnJheS5cbiAgICovXG4gIEFycmF5U2V0LmZyb21BcnJheSA9IGZ1bmN0aW9uIEFycmF5U2V0X2Zyb21BcnJheShhQXJyYXksIGFBbGxvd0R1cGxpY2F0ZXMpIHtcbiAgICB2YXIgc2V0ID0gbmV3IEFycmF5U2V0KCk7XG4gICAgZm9yICh2YXIgaSA9IDAsIGxlbiA9IGFBcnJheS5sZW5ndGg7IGkgPCBsZW47IGkrKykge1xuICAgICAgc2V0LmFkZChhQXJyYXlbaV0sIGFBbGxvd0R1cGxpY2F0ZXMpO1xuICAgIH1cbiAgICByZXR1cm4gc2V0O1xuICB9O1xuXG4gIC8qKlxuICAgKiBSZXR1cm4gaG93IG1hbnkgdW5pcXVlIGl0ZW1zIGFyZSBpbiB0aGlzIEFycmF5U2V0LiBJZiBkdXBsaWNhdGVzIGhhdmUgYmVlblxuICAgKiBhZGRlZCwgdGhhbiB0aG9zZSBkbyBub3QgY291bnQgdG93YXJkcyB0aGUgc2l6ZS5cbiAgICpcbiAgICogQHJldHVybnMgTnVtYmVyXG4gICAqL1xuICBBcnJheVNldC5wcm90b3R5cGUuc2l6ZSA9IGZ1bmN0aW9uIEFycmF5U2V0X3NpemUoKSB7XG4gICAgcmV0dXJuIE9iamVjdC5nZXRPd25Qcm9wZXJ0eU5hbWVzKHRoaXMuX3NldCkubGVuZ3RoO1xuICB9O1xuXG4gIC8qKlxuICAgKiBBZGQgdGhlIGdpdmVuIHN0cmluZyB0byB0aGlzIHNldC5cbiAgICpcbiAgICogQHBhcmFtIFN0cmluZyBhU3RyXG4gICAqL1xuICBBcnJheVNldC5wcm90b3R5cGUuYWRkID0gZnVuY3Rpb24gQXJyYXlTZXRfYWRkKGFTdHIsIGFBbGxvd0R1cGxpY2F0ZXMpIHtcbiAgICB2YXIgc1N0ciA9IHV0aWwudG9TZXRTdHJpbmcoYVN0cik7XG4gICAgdmFyIGlzRHVwbGljYXRlID0gdGhpcy5fc2V0Lmhhc093blByb3BlcnR5KHNTdHIpO1xuICAgIHZhciBpZHggPSB0aGlzLl9hcnJheS5sZW5ndGg7XG4gICAgaWYgKCFpc0R1cGxpY2F0ZSB8fCBhQWxsb3dEdXBsaWNhdGVzKSB7XG4gICAgICB0aGlzLl9hcnJheS5wdXNoKGFTdHIpO1xuICAgIH1cbiAgICBpZiAoIWlzRHVwbGljYXRlKSB7XG4gICAgICB0aGlzLl9zZXRbc1N0cl0gPSBpZHg7XG4gICAgfVxuICB9O1xuXG4gIC8qKlxuICAgKiBJcyB0aGUgZ2l2ZW4gc3RyaW5nIGEgbWVtYmVyIG9mIHRoaXMgc2V0P1xuICAgKlxuICAgKiBAcGFyYW0gU3RyaW5nIGFTdHJcbiAgICovXG4gIEFycmF5U2V0LnByb3RvdHlwZS5oYXMgPSBmdW5jdGlvbiBBcnJheVNldF9oYXMoYVN0cikge1xuICAgIHZhciBzU3RyID0gdXRpbC50b1NldFN0cmluZyhhU3RyKTtcbiAgICByZXR1cm4gdGhpcy5fc2V0Lmhhc093blByb3BlcnR5KHNTdHIpO1xuICB9O1xuXG4gIC8qKlxuICAgKiBXaGF0IGlzIHRoZSBpbmRleCBvZiB0aGUgZ2l2ZW4gc3RyaW5nIGluIHRoZSBhcnJheT9cbiAgICpcbiAgICogQHBhcmFtIFN0cmluZyBhU3RyXG4gICAqL1xuICBBcnJheVNldC5wcm90b3R5cGUuaW5kZXhPZiA9IGZ1bmN0aW9uIEFycmF5U2V0X2luZGV4T2YoYVN0cikge1xuICAgIHZhciBzU3RyID0gdXRpbC50b1NldFN0cmluZyhhU3RyKTtcbiAgICBpZiAodGhpcy5fc2V0Lmhhc093blByb3BlcnR5KHNTdHIpKSB7XG4gICAgICByZXR1cm4gdGhpcy5fc2V0W3NTdHJdO1xuICAgIH1cbiAgICB0aHJvdyBuZXcgRXJyb3IoJ1wiJyArIGFTdHIgKyAnXCIgaXMgbm90IGluIHRoZSBzZXQuJyk7XG4gIH07XG5cbiAgLyoqXG4gICAqIFdoYXQgaXMgdGhlIGVsZW1lbnQgYXQgdGhlIGdpdmVuIGluZGV4P1xuICAgKlxuICAgKiBAcGFyYW0gTnVtYmVyIGFJZHhcbiAgICovXG4gIEFycmF5U2V0LnByb3RvdHlwZS5hdCA9IGZ1bmN0aW9uIEFycmF5U2V0X2F0KGFJZHgpIHtcbiAgICBpZiAoYUlkeCA+PSAwICYmIGFJZHggPCB0aGlzLl9hcnJheS5sZW5ndGgpIHtcbiAgICAgIHJldHVybiB0aGlzLl9hcnJheVthSWR4XTtcbiAgICB9XG4gICAgdGhyb3cgbmV3IEVycm9yKCdObyBlbGVtZW50IGluZGV4ZWQgYnkgJyArIGFJZHgpO1xuICB9O1xuXG4gIC8qKlxuICAgKiBSZXR1cm5zIHRoZSBhcnJheSByZXByZXNlbnRhdGlvbiBvZiB0aGlzIHNldCAod2hpY2ggaGFzIHRoZSBwcm9wZXIgaW5kaWNlc1xuICAgKiBpbmRpY2F0ZWQgYnkgaW5kZXhPZikuIE5vdGUgdGhhdCB0aGlzIGlzIGEgY29weSBvZiB0aGUgaW50ZXJuYWwgYXJyYXkgdXNlZFxuICAgKiBmb3Igc3RvcmluZyB0aGUgbWVtYmVycyBzbyB0aGF0IG5vIG9uZSBjYW4gbWVzcyB3aXRoIGludGVybmFsIHN0YXRlLlxuICAgKi9cbiAgQXJyYXlTZXQucHJvdG90eXBlLnRvQXJyYXkgPSBmdW5jdGlvbiBBcnJheVNldF90b0FycmF5KCkge1xuICAgIHJldHVybiB0aGlzLl9hcnJheS5zbGljZSgpO1xuICB9O1xuXG4gIGV4cG9ydHMuQXJyYXlTZXQgPSBBcnJheVNldDtcbn1cblxuXG5cbi8qKioqKioqKioqKioqKioqKlxuICoqIFdFQlBBQ0sgRk9PVEVSXG4gKiogLi9saWIvYXJyYXktc2V0LmpzXG4gKiogbW9kdWxlIGlkID0gMVxuICoqIG1vZHVsZSBjaHVua3MgPSAwXG4gKiovIiwiLyogLSotIE1vZGU6IGpzOyBqcy1pbmRlbnQtbGV2ZWw6IDI7IC0qLSAqL1xuLypcbiAqIENvcHlyaWdodCAyMDExIE1vemlsbGEgRm91bmRhdGlvbiBhbmQgY29udHJpYnV0b3JzXG4gKiBMaWNlbnNlZCB1bmRlciB0aGUgTmV3IEJTRCBsaWNlbnNlLiBTZWUgTElDRU5TRSBvcjpcbiAqIGh0dHA6Ly9vcGVuc291cmNlLm9yZy9saWNlbnNlcy9CU0QtMy1DbGF1c2VcbiAqL1xue1xuICAvKipcbiAgICogVGhpcyBpcyBhIGhlbHBlciBmdW5jdGlvbiBmb3IgZ2V0dGluZyB2YWx1ZXMgZnJvbSBwYXJhbWV0ZXIvb3B0aW9uc1xuICAgKiBvYmplY3RzLlxuICAgKlxuICAgKiBAcGFyYW0gYXJncyBUaGUgb2JqZWN0IHdlIGFyZSBleHRyYWN0aW5nIHZhbHVlcyBmcm9tXG4gICAqIEBwYXJhbSBuYW1lIFRoZSBuYW1lIG9mIHRoZSBwcm9wZXJ0eSB3ZSBhcmUgZ2V0dGluZy5cbiAgICogQHBhcmFtIGRlZmF1bHRWYWx1ZSBBbiBvcHRpb25hbCB2YWx1ZSB0byByZXR1cm4gaWYgdGhlIHByb3BlcnR5IGlzIG1pc3NpbmdcbiAgICogZnJvbSB0aGUgb2JqZWN0LiBJZiB0aGlzIGlzIG5vdCBzcGVjaWZpZWQgYW5kIHRoZSBwcm9wZXJ0eSBpcyBtaXNzaW5nLCBhblxuICAgKiBlcnJvciB3aWxsIGJlIHRocm93bi5cbiAgICovXG4gIGZ1bmN0aW9uIGdldEFyZyhhQXJncywgYU5hbWUsIGFEZWZhdWx0VmFsdWUpIHtcbiAgICBpZiAoYU5hbWUgaW4gYUFyZ3MpIHtcbiAgICAgIHJldHVybiBhQXJnc1thTmFtZV07XG4gICAgfSBlbHNlIGlmIChhcmd1bWVudHMubGVuZ3RoID09PSAzKSB7XG4gICAgICByZXR1cm4gYURlZmF1bHRWYWx1ZTtcbiAgICB9IGVsc2Uge1xuICAgICAgdGhyb3cgbmV3IEVycm9yKCdcIicgKyBhTmFtZSArICdcIiBpcyBhIHJlcXVpcmVkIGFyZ3VtZW50LicpO1xuICAgIH1cbiAgfVxuICBleHBvcnRzLmdldEFyZyA9IGdldEFyZztcblxuICB2YXIgdXJsUmVnZXhwID0gL14oPzooW1xcdytcXC0uXSspOik/XFwvXFwvKD86KFxcdys6XFx3KylAKT8oW1xcdy5dKikoPzo6KFxcZCspKT8oXFxTKikkLztcbiAgdmFyIGRhdGFVcmxSZWdleHAgPSAvXmRhdGE6LitcXCwuKyQvO1xuXG4gIGZ1bmN0aW9uIHVybFBhcnNlKGFVcmwpIHtcbiAgICB2YXIgbWF0Y2ggPSBhVXJsLm1hdGNoKHVybFJlZ2V4cCk7XG4gICAgaWYgKCFtYXRjaCkge1xuICAgICAgcmV0dXJuIG51bGw7XG4gICAgfVxuICAgIHJldHVybiB7XG4gICAgICBzY2hlbWU6IG1hdGNoWzFdLFxuICAgICAgYXV0aDogbWF0Y2hbMl0sXG4gICAgICBob3N0OiBtYXRjaFszXSxcbiAgICAgIHBvcnQ6IG1hdGNoWzRdLFxuICAgICAgcGF0aDogbWF0Y2hbNV1cbiAgICB9O1xuICB9XG4gIGV4cG9ydHMudXJsUGFyc2UgPSB1cmxQYXJzZTtcblxuICBmdW5jdGlvbiB1cmxHZW5lcmF0ZShhUGFyc2VkVXJsKSB7XG4gICAgdmFyIHVybCA9ICcnO1xuICAgIGlmIChhUGFyc2VkVXJsLnNjaGVtZSkge1xuICAgICAgdXJsICs9IGFQYXJzZWRVcmwuc2NoZW1lICsgJzonO1xuICAgIH1cbiAgICB1cmwgKz0gJy8vJztcbiAgICBpZiAoYVBhcnNlZFVybC5hdXRoKSB7XG4gICAgICB1cmwgKz0gYVBhcnNlZFVybC5hdXRoICsgJ0AnO1xuICAgIH1cbiAgICBpZiAoYVBhcnNlZFVybC5ob3N0KSB7XG4gICAgICB1cmwgKz0gYVBhcnNlZFVybC5ob3N0O1xuICAgIH1cbiAgICBpZiAoYVBhcnNlZFVybC5wb3J0KSB7XG4gICAgICB1cmwgKz0gXCI6XCIgKyBhUGFyc2VkVXJsLnBvcnRcbiAgICB9XG4gICAgaWYgKGFQYXJzZWRVcmwucGF0aCkge1xuICAgICAgdXJsICs9IGFQYXJzZWRVcmwucGF0aDtcbiAgICB9XG4gICAgcmV0dXJuIHVybDtcbiAgfVxuICBleHBvcnRzLnVybEdlbmVyYXRlID0gdXJsR2VuZXJhdGU7XG5cbiAgLyoqXG4gICAqIE5vcm1hbGl6ZXMgYSBwYXRoLCBvciB0aGUgcGF0aCBwb3J0aW9uIG9mIGEgVVJMOlxuICAgKlxuICAgKiAtIFJlcGxhY2VzIGNvbnNlcXV0aXZlIHNsYXNoZXMgd2l0aCBvbmUgc2xhc2guXG4gICAqIC0gUmVtb3ZlcyB1bm5lY2Vzc2FyeSAnLicgcGFydHMuXG4gICAqIC0gUmVtb3ZlcyB1bm5lY2Vzc2FyeSAnPGRpcj4vLi4nIHBhcnRzLlxuICAgKlxuICAgKiBCYXNlZCBvbiBjb2RlIGluIHRoZSBOb2RlLmpzICdwYXRoJyBjb3JlIG1vZHVsZS5cbiAgICpcbiAgICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIHVybCB0byBub3JtYWxpemUuXG4gICAqL1xuICBmdW5jdGlvbiBub3JtYWxpemUoYVBhdGgpIHtcbiAgICB2YXIgcGF0aCA9IGFQYXRoO1xuICAgIHZhciB1cmwgPSB1cmxQYXJzZShhUGF0aCk7XG4gICAgaWYgKHVybCkge1xuICAgICAgaWYgKCF1cmwucGF0aCkge1xuICAgICAgICByZXR1cm4gYVBhdGg7XG4gICAgICB9XG4gICAgICBwYXRoID0gdXJsLnBhdGg7XG4gICAgfVxuICAgIHZhciBpc0Fic29sdXRlID0gZXhwb3J0cy5pc0Fic29sdXRlKHBhdGgpO1xuXG4gICAgdmFyIHBhcnRzID0gcGF0aC5zcGxpdCgvXFwvKy8pO1xuICAgIGZvciAodmFyIHBhcnQsIHVwID0gMCwgaSA9IHBhcnRzLmxlbmd0aCAtIDE7IGkgPj0gMDsgaS0tKSB7XG4gICAgICBwYXJ0ID0gcGFydHNbaV07XG4gICAgICBpZiAocGFydCA9PT0gJy4nKSB7XG4gICAgICAgIHBhcnRzLnNwbGljZShpLCAxKTtcbiAgICAgIH0gZWxzZSBpZiAocGFydCA9PT0gJy4uJykge1xuICAgICAgICB1cCsrO1xuICAgICAgfSBlbHNlIGlmICh1cCA+IDApIHtcbiAgICAgICAgaWYgKHBhcnQgPT09ICcnKSB7XG4gICAgICAgICAgLy8gVGhlIGZpcnN0IHBhcnQgaXMgYmxhbmsgaWYgdGhlIHBhdGggaXMgYWJzb2x1dGUuIFRyeWluZyB0byBnb1xuICAgICAgICAgIC8vIGFib3ZlIHRoZSByb290IGlzIGEgbm8tb3AuIFRoZXJlZm9yZSB3ZSBjYW4gcmVtb3ZlIGFsbCAnLi4nIHBhcnRzXG4gICAgICAgICAgLy8gZGlyZWN0bHkgYWZ0ZXIgdGhlIHJvb3QuXG4gICAgICAgICAgcGFydHMuc3BsaWNlKGkgKyAxLCB1cCk7XG4gICAgICAgICAgdXAgPSAwO1xuICAgICAgICB9IGVsc2Uge1xuICAgICAgICAgIHBhcnRzLnNwbGljZShpLCAyKTtcbiAgICAgICAgICB1cC0tO1xuICAgICAgICB9XG4gICAgICB9XG4gICAgfVxuICAgIHBhdGggPSBwYXJ0cy5qb2luKCcvJyk7XG5cbiAgICBpZiAocGF0aCA9PT0gJycpIHtcbiAgICAgIHBhdGggPSBpc0Fic29sdXRlID8gJy8nIDogJy4nO1xuICAgIH1cblxuICAgIGlmICh1cmwpIHtcbiAgICAgIHVybC5wYXRoID0gcGF0aDtcbiAgICAgIHJldHVybiB1cmxHZW5lcmF0ZSh1cmwpO1xuICAgIH1cbiAgICByZXR1cm4gcGF0aDtcbiAgfVxuICBleHBvcnRzLm5vcm1hbGl6ZSA9IG5vcm1hbGl6ZTtcblxuICAvKipcbiAgICogSm9pbnMgdHdvIHBhdGhzL1VSTHMuXG4gICAqXG4gICAqIEBwYXJhbSBhUm9vdCBUaGUgcm9vdCBwYXRoIG9yIFVSTC5cbiAgICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBqb2luZWQgd2l0aCB0aGUgcm9vdC5cbiAgICpcbiAgICogLSBJZiBhUGF0aCBpcyBhIFVSTCBvciBhIGRhdGEgVVJJLCBhUGF0aCBpcyByZXR1cm5lZCwgdW5sZXNzIGFQYXRoIGlzIGFcbiAgICogICBzY2hlbWUtcmVsYXRpdmUgVVJMOiBUaGVuIHRoZSBzY2hlbWUgb2YgYVJvb3QsIGlmIGFueSwgaXMgcHJlcGVuZGVkXG4gICAqICAgZmlyc3QuXG4gICAqIC0gT3RoZXJ3aXNlIGFQYXRoIGlzIGEgcGF0aC4gSWYgYVJvb3QgaXMgYSBVUkwsIHRoZW4gaXRzIHBhdGggcG9ydGlvblxuICAgKiAgIGlzIHVwZGF0ZWQgd2l0aCB0aGUgcmVzdWx0IGFuZCBhUm9vdCBpcyByZXR1cm5lZC4gT3RoZXJ3aXNlIHRoZSByZXN1bHRcbiAgICogICBpcyByZXR1cm5lZC5cbiAgICogICAtIElmIGFQYXRoIGlzIGFic29sdXRlLCB0aGUgcmVzdWx0IGlzIGFQYXRoLlxuICAgKiAgIC0gT3RoZXJ3aXNlIHRoZSB0d28gcGF0aHMgYXJlIGpvaW5lZCB3aXRoIGEgc2xhc2guXG4gICAqIC0gSm9pbmluZyBmb3IgZXhhbXBsZSAnaHR0cDovLycgYW5kICd3d3cuZXhhbXBsZS5jb20nIGlzIGFsc28gc3VwcG9ydGVkLlxuICAgKi9cbiAgZnVuY3Rpb24gam9pbihhUm9vdCwgYVBhdGgpIHtcbiAgICBpZiAoYVJvb3QgPT09IFwiXCIpIHtcbiAgICAgIGFSb290ID0gXCIuXCI7XG4gICAgfVxuICAgIGlmIChhUGF0aCA9PT0gXCJcIikge1xuICAgICAgYVBhdGggPSBcIi5cIjtcbiAgICB9XG4gICAgdmFyIGFQYXRoVXJsID0gdXJsUGFyc2UoYVBhdGgpO1xuICAgIHZhciBhUm9vdFVybCA9IHVybFBhcnNlKGFSb290KTtcbiAgICBpZiAoYVJvb3RVcmwpIHtcbiAgICAgIGFSb290ID0gYVJvb3RVcmwucGF0aCB8fCAnLyc7XG4gICAgfVxuXG4gICAgLy8gYGpvaW4oZm9vLCAnLy93d3cuZXhhbXBsZS5vcmcnKWBcbiAgICBpZiAoYVBhdGhVcmwgJiYgIWFQYXRoVXJsLnNjaGVtZSkge1xuICAgICAgaWYgKGFSb290VXJsKSB7XG4gICAgICAgIGFQYXRoVXJsLnNjaGVtZSA9IGFSb290VXJsLnNjaGVtZTtcbiAgICAgIH1cbiAgICAgIHJldHVybiB1cmxHZW5lcmF0ZShhUGF0aFVybCk7XG4gICAgfVxuXG4gICAgaWYgKGFQYXRoVXJsIHx8IGFQYXRoLm1hdGNoKGRhdGFVcmxSZWdleHApKSB7XG4gICAgICByZXR1cm4gYVBhdGg7XG4gICAgfVxuXG4gICAgLy8gYGpvaW4oJ2h0dHA6Ly8nLCAnd3d3LmV4YW1wbGUuY29tJylgXG4gICAgaWYgKGFSb290VXJsICYmICFhUm9vdFVybC5ob3N0ICYmICFhUm9vdFVybC5wYXRoKSB7XG4gICAgICBhUm9vdFVybC5ob3N0ID0gYVBhdGg7XG4gICAgICByZXR1cm4gdXJsR2VuZXJhdGUoYVJvb3RVcmwpO1xuICAgIH1cblxuICAgIHZhciBqb2luZWQgPSBhUGF0aC5jaGFyQXQoMCkgPT09ICcvJ1xuICAgICAgPyBhUGF0aFxuICAgICAgOiBub3JtYWxpemUoYVJvb3QucmVwbGFjZSgvXFwvKyQvLCAnJykgKyAnLycgKyBhUGF0aCk7XG5cbiAgICBpZiAoYVJvb3RVcmwpIHtcbiAgICAgIGFSb290VXJsLnBhdGggPSBqb2luZWQ7XG4gICAgICByZXR1cm4gdXJsR2VuZXJhdGUoYVJvb3RVcmwpO1xuICAgIH1cbiAgICByZXR1cm4gam9pbmVkO1xuICB9XG4gIGV4cG9ydHMuam9pbiA9IGpvaW47XG5cbiAgZXhwb3J0cy5pc0Fic29sdXRlID0gZnVuY3Rpb24gKGFQYXRoKSB7XG4gICAgcmV0dXJuIGFQYXRoLmNoYXJBdCgwKSA9PT0gJy8nIHx8ICEhYVBhdGgubWF0Y2godXJsUmVnZXhwKTtcbiAgfTtcblxuICAvKipcbiAgICogTWFrZSBhIHBhdGggcmVsYXRpdmUgdG8gYSBVUkwgb3IgYW5vdGhlciBwYXRoLlxuICAgKlxuICAgKiBAcGFyYW0gYVJvb3QgVGhlIHJvb3QgcGF0aCBvciBVUkwuXG4gICAqIEBwYXJhbSBhUGF0aCBUaGUgcGF0aCBvciBVUkwgdG8gYmUgbWFkZSByZWxhdGl2ZSB0byBhUm9vdC5cbiAgICovXG4gIGZ1bmN0aW9uIHJlbGF0aXZlKGFSb290LCBhUGF0aCkge1xuICAgIGlmIChhUm9vdCA9PT0gXCJcIikge1xuICAgICAgYVJvb3QgPSBcIi5cIjtcbiAgICB9XG5cbiAgICBhUm9vdCA9IGFSb290LnJlcGxhY2UoL1xcLyQvLCAnJyk7XG5cbiAgICAvLyBJdCBpcyBwb3NzaWJsZSBmb3IgdGhlIHBhdGggdG8gYmUgYWJvdmUgdGhlIHJvb3QuIEluIHRoaXMgY2FzZSwgc2ltcGx5XG4gICAgLy8gY2hlY2tpbmcgd2hldGhlciB0aGUgcm9vdCBpcyBhIHByZWZpeCBvZiB0aGUgcGF0aCB3b24ndCB3b3JrLiBJbnN0ZWFkLCB3ZVxuICAgIC8vIG5lZWQgdG8gcmVtb3ZlIGNvbXBvbmVudHMgZnJvbSB0aGUgcm9vdCBvbmUgYnkgb25lLCB1bnRpbCBlaXRoZXIgd2UgZmluZFxuICAgIC8vIGEgcHJlZml4IHRoYXQgZml0cywgb3Igd2UgcnVuIG91dCBvZiBjb21wb25lbnRzIHRvIHJlbW92ZS5cbiAgICB2YXIgbGV2ZWwgPSAwO1xuICAgIHdoaWxlIChhUGF0aC5pbmRleE9mKGFSb290ICsgJy8nKSAhPT0gMCkge1xuICAgICAgdmFyIGluZGV4ID0gYVJvb3QubGFzdEluZGV4T2YoXCIvXCIpO1xuICAgICAgaWYgKGluZGV4IDwgMCkge1xuICAgICAgICByZXR1cm4gYVBhdGg7XG4gICAgICB9XG5cbiAgICAgIC8vIElmIHRoZSBvbmx5IHBhcnQgb2YgdGhlIHJvb3QgdGhhdCBpcyBsZWZ0IGlzIHRoZSBzY2hlbWUgKGkuZS4gaHR0cDovLyxcbiAgICAgIC8vIGZpbGU6Ly8vLCBldGMuKSwgb25lIG9yIG1vcmUgc2xhc2hlcyAoLyksIG9yIHNpbXBseSBub3RoaW5nIGF0IGFsbCwgd2VcbiAgICAgIC8vIGhhdmUgZXhoYXVzdGVkIGFsbCBjb21wb25lbnRzLCBzbyB0aGUgcGF0aCBpcyBub3QgcmVsYXRpdmUgdG8gdGhlIHJvb3QuXG4gICAgICBhUm9vdCA9IGFSb290LnNsaWNlKDAsIGluZGV4KTtcbiAgICAgIGlmIChhUm9vdC5tYXRjaCgvXihbXlxcL10rOlxcLyk/XFwvKiQvKSkge1xuICAgICAgICByZXR1cm4gYVBhdGg7XG4gICAgICB9XG5cbiAgICAgICsrbGV2ZWw7XG4gICAgfVxuXG4gICAgLy8gTWFrZSBzdXJlIHdlIGFkZCBhIFwiLi4vXCIgZm9yIGVhY2ggY29tcG9uZW50IHdlIHJlbW92ZWQgZnJvbSB0aGUgcm9vdC5cbiAgICByZXR1cm4gQXJyYXkobGV2ZWwgKyAxKS5qb2luKFwiLi4vXCIpICsgYVBhdGguc3Vic3RyKGFSb290Lmxlbmd0aCArIDEpO1xuICB9XG4gIGV4cG9ydHMucmVsYXRpdmUgPSByZWxhdGl2ZTtcblxuICAvKipcbiAgICogQmVjYXVzZSBiZWhhdmlvciBnb2VzIHdhY2t5IHdoZW4geW91IHNldCBgX19wcm90b19fYCBvbiBvYmplY3RzLCB3ZVxuICAgKiBoYXZlIHRvIHByZWZpeCBhbGwgdGhlIHN0cmluZ3MgaW4gb3VyIHNldCB3aXRoIGFuIGFyYml0cmFyeSBjaGFyYWN0ZXIuXG4gICAqXG4gICAqIFNlZSBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL3B1bGwvMzEgYW5kXG4gICAqIGh0dHBzOi8vZ2l0aHViLmNvbS9tb3ppbGxhL3NvdXJjZS1tYXAvaXNzdWVzLzMwXG4gICAqXG4gICAqIEBwYXJhbSBTdHJpbmcgYVN0clxuICAgKi9cbiAgZnVuY3Rpb24gdG9TZXRTdHJpbmcoYVN0cikge1xuICAgIHJldHVybiAnJCcgKyBhU3RyO1xuICB9XG4gIGV4cG9ydHMudG9TZXRTdHJpbmcgPSB0b1NldFN0cmluZztcblxuICBmdW5jdGlvbiBmcm9tU2V0U3RyaW5nKGFTdHIpIHtcbiAgICByZXR1cm4gYVN0ci5zdWJzdHIoMSk7XG4gIH1cbiAgZXhwb3J0cy5mcm9tU2V0U3RyaW5nID0gZnJvbVNldFN0cmluZztcblxuICAvKipcbiAgICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aGVyZSB0aGUgb3JpZ2luYWwgcG9zaXRpb25zIGFyZSBjb21wYXJlZC5cbiAgICpcbiAgICogT3B0aW9uYWxseSBwYXNzIGluIGB0cnVlYCBhcyBgb25seUNvbXBhcmVHZW5lcmF0ZWRgIHRvIGNvbnNpZGVyIHR3b1xuICAgKiBtYXBwaW5ncyB3aXRoIHRoZSBzYW1lIG9yaWdpbmFsIHNvdXJjZS9saW5lL2NvbHVtbiwgYnV0IGRpZmZlcmVudCBnZW5lcmF0ZWRcbiAgICogbGluZSBhbmQgY29sdW1uIHRoZSBzYW1lLiBVc2VmdWwgd2hlbiBzZWFyY2hpbmcgZm9yIGEgbWFwcGluZyB3aXRoIGFcbiAgICogc3R1YmJlZCBvdXQgbWFwcGluZy5cbiAgICovXG4gIGZ1bmN0aW9uIGNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zKG1hcHBpbmdBLCBtYXBwaW5nQiwgb25seUNvbXBhcmVPcmlnaW5hbCkge1xuICAgIHZhciBjbXAgPSBtYXBwaW5nQS5zb3VyY2UgLSBtYXBwaW5nQi5zb3VyY2U7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbExpbmUgLSBtYXBwaW5nQi5vcmlnaW5hbExpbmU7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbENvbHVtbiAtIG1hcHBpbmdCLm9yaWdpbmFsQ29sdW1uO1xuICAgIGlmIChjbXAgIT09IDAgfHwgb25seUNvbXBhcmVPcmlnaW5hbCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5nZW5lcmF0ZWRDb2x1bW4gLSBtYXBwaW5nQi5nZW5lcmF0ZWRDb2x1bW47XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5nZW5lcmF0ZWRMaW5lIC0gbWFwcGluZ0IuZ2VuZXJhdGVkTGluZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIHJldHVybiBtYXBwaW5nQS5uYW1lIC0gbWFwcGluZ0IubmFtZTtcbiAgfVxuICBleHBvcnRzLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zID0gY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnM7XG5cbiAgLyoqXG4gICAqIENvbXBhcmF0b3IgYmV0d2VlbiB0d28gbWFwcGluZ3Mgd2l0aCBkZWZsYXRlZCBzb3VyY2UgYW5kIG5hbWUgaW5kaWNlcyB3aGVyZVxuICAgKiB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gICAqXG4gICAqIE9wdGlvbmFsbHkgcGFzcyBpbiBgdHJ1ZWAgYXMgYG9ubHlDb21wYXJlR2VuZXJhdGVkYCB0byBjb25zaWRlciB0d29cbiAgICogbWFwcGluZ3Mgd2l0aCB0aGUgc2FtZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uLCBidXQgZGlmZmVyZW50XG4gICAqIHNvdXJjZS9uYW1lL29yaWdpbmFsIGxpbmUgYW5kIGNvbHVtbiB0aGUgc2FtZS4gVXNlZnVsIHdoZW4gc2VhcmNoaW5nIGZvciBhXG4gICAqIG1hcHBpbmcgd2l0aCBhIHN0dWJiZWQgb3V0IG1hcHBpbmcuXG4gICAqL1xuICBmdW5jdGlvbiBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZChtYXBwaW5nQSwgbWFwcGluZ0IsIG9ubHlDb21wYXJlR2VuZXJhdGVkKSB7XG4gICAgdmFyIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZExpbmUgLSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICAgIGlmIChjbXAgIT09IDAgfHwgb25seUNvbXBhcmVHZW5lcmF0ZWQpIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0Euc291cmNlIC0gbWFwcGluZ0Iuc291cmNlO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxDb2x1bW4gLSBtYXBwaW5nQi5vcmlnaW5hbENvbHVtbjtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIHJldHVybiBtYXBwaW5nQS5uYW1lIC0gbWFwcGluZ0IubmFtZTtcbiAgfVxuICBleHBvcnRzLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0RlZmxhdGVkID0gY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQ7XG5cbiAgZnVuY3Rpb24gc3RyY21wKGFTdHIxLCBhU3RyMikge1xuICAgIGlmIChhU3RyMSA9PT0gYVN0cjIpIHtcbiAgICAgIHJldHVybiAwO1xuICAgIH1cblxuICAgIGlmIChhU3RyMSA+IGFTdHIyKSB7XG4gICAgICByZXR1cm4gMTtcbiAgICB9XG5cbiAgICByZXR1cm4gLTE7XG4gIH1cblxuICAvKipcbiAgICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aXRoIGluZmxhdGVkIHNvdXJjZSBhbmQgbmFtZSBzdHJpbmdzIHdoZXJlXG4gICAqIHRoZSBnZW5lcmF0ZWQgcG9zaXRpb25zIGFyZSBjb21wYXJlZC5cbiAgICovXG4gIGZ1bmN0aW9uIGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQikge1xuICAgIHZhciBjbXAgPSBtYXBwaW5nQS5nZW5lcmF0ZWRMaW5lIC0gbWFwcGluZ0IuZ2VuZXJhdGVkTGluZTtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZENvbHVtbiAtIG1hcHBpbmdCLmdlbmVyYXRlZENvbHVtbjtcbiAgICBpZiAoY21wICE9PSAwKSB7XG4gICAgICByZXR1cm4gY21wO1xuICAgIH1cblxuICAgIGNtcCA9IHN0cmNtcChtYXBwaW5nQS5zb3VyY2UsIG1hcHBpbmdCLnNvdXJjZSk7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbExpbmUgLSBtYXBwaW5nQi5vcmlnaW5hbExpbmU7XG4gICAgaWYgKGNtcCAhPT0gMCkge1xuICAgICAgcmV0dXJuIGNtcDtcbiAgICB9XG5cbiAgICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbENvbHVtbiAtIG1hcHBpbmdCLm9yaWdpbmFsQ29sdW1uO1xuICAgIGlmIChjbXAgIT09IDApIHtcbiAgICAgIHJldHVybiBjbXA7XG4gICAgfVxuXG4gICAgcmV0dXJuIHN0cmNtcChtYXBwaW5nQS5uYW1lLCBtYXBwaW5nQi5uYW1lKTtcbiAgfVxuICBleHBvcnRzLmNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkID0gY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zSW5mbGF0ZWQ7XG59XG5cblxuXG4vKioqKioqKioqKioqKioqKipcbiAqKiBXRUJQQUNLIEZPT1RFUlxuICoqIC4vbGliL3V0aWwuanNcbiAqKiBtb2R1bGUgaWQgPSAyXG4gKiogbW9kdWxlIGNodW5rcyA9IDBcbiAqKi8iXSwic291cmNlUm9vdCI6IiJ9