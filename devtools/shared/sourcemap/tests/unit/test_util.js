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
/***/ (function(module, exports, __webpack_require__) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2014 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
	var libUtil = __webpack_require__(1);
	
	exports['test urls'] = function (assert) {
	  var assertUrl = function (url) {
	    assert.equal(url, libUtil.urlGenerate(libUtil.urlParse(url)));
	  };
	  assertUrl('http://');
	  assertUrl('http://www.example.com');
	  assertUrl('http://user:pass@www.example.com');
	  assertUrl('http://www.example.com:80');
	  assertUrl('http://www.example.com/');
	  assertUrl('http://www.example.com/foo/bar');
	  assertUrl('http://www.example.com/foo/bar/');
	  assertUrl('http://user:pass@www.example.com:80/foo/bar/');
	
	  assertUrl('//');
	  assertUrl('//www.example.com');
	  assertUrl('file:///www.example.com');
	
	  assert.equal(libUtil.urlParse(''), null);
	  assert.equal(libUtil.urlParse('.'), null);
	  assert.equal(libUtil.urlParse('..'), null);
	  assert.equal(libUtil.urlParse('a'), null);
	  assert.equal(libUtil.urlParse('a/b'), null);
	  assert.equal(libUtil.urlParse('a//b'), null);
	  assert.equal(libUtil.urlParse('/a'), null);
	  assert.equal(libUtil.urlParse('data:foo,bar'), null);
	
	  var parsed = libUtil.urlParse('http://x-y.com/bar');
	  assert.equal(parsed.scheme, 'http');
	  assert.equal(parsed.host, 'x-y.com');
	  assert.equal(parsed.path, '/bar');
	
	  var webpackURL = 'webpack:///webpack/bootstrap 67e184f9679733298d44'
	  parsed = libUtil.urlParse(webpackURL);
	  assert.equal(parsed.scheme, 'webpack');
	  assert.equal(parsed.host, '');
	  assert.equal(parsed.path, '/webpack/bootstrap 67e184f9679733298d44');
	  assert.equal(webpackURL, libUtil.urlGenerate(parsed));
	};
	
	exports['test normalize()'] = function (assert) {
	  assert.equal(libUtil.normalize('/..'), '/');
	  assert.equal(libUtil.normalize('/../'), '/');
	  assert.equal(libUtil.normalize('/../../../..'), '/');
	  assert.equal(libUtil.normalize('/../../../../a/b/c'), '/a/b/c');
	  assert.equal(libUtil.normalize('/a/b/c/../../../d/../../e'), '/e');
	
	  assert.equal(libUtil.normalize('..'), '..');
	  assert.equal(libUtil.normalize('../'), '../');
	  assert.equal(libUtil.normalize('../../a/'), '../../a/');
	  assert.equal(libUtil.normalize('a/..'), '.');
	  assert.equal(libUtil.normalize('a/../../..'), '../..');
	
	  assert.equal(libUtil.normalize('/.'), '/');
	  assert.equal(libUtil.normalize('/./'), '/');
	  assert.equal(libUtil.normalize('/./././.'), '/');
	  assert.equal(libUtil.normalize('/././././a/b/c'), '/a/b/c');
	  assert.equal(libUtil.normalize('/a/b/c/./././d/././e'), '/a/b/c/d/e');
	
	  assert.equal(libUtil.normalize(''), '.');
	  assert.equal(libUtil.normalize('.'), '.');
	  assert.equal(libUtil.normalize('./'), '.');
	  assert.equal(libUtil.normalize('././a'), 'a');
	  assert.equal(libUtil.normalize('a/./'), 'a/');
	  assert.equal(libUtil.normalize('a/././.'), 'a');
	
	  assert.equal(libUtil.normalize('/a/b//c////d/////'), '/a/b/c/d/');
	  assert.equal(libUtil.normalize('///a/b//c////d/////'), '///a/b/c/d/');
	  assert.equal(libUtil.normalize('a/b//c////d'), 'a/b/c/d');
	
	  assert.equal(libUtil.normalize('.///.././../a/b//./..'), '../../a')
	
	  assert.equal(libUtil.normalize('http://www.example.com'), 'http://www.example.com');
	  assert.equal(libUtil.normalize('http://www.example.com/'), 'http://www.example.com/');
	  assert.equal(libUtil.normalize('http://www.example.com/./..//a/b/c/.././d//'), 'http://www.example.com/a/b/d/');
	};
	
	exports['test join()'] = function (assert) {
	  assert.equal(libUtil.join('a', 'b'), 'a/b');
	  assert.equal(libUtil.join('a/', 'b'), 'a/b');
	  assert.equal(libUtil.join('a//', 'b'), 'a/b');
	  assert.equal(libUtil.join('a', 'b/'), 'a/b/');
	  assert.equal(libUtil.join('a', 'b//'), 'a/b/');
	  assert.equal(libUtil.join('a/', '/b'), '/b');
	  assert.equal(libUtil.join('a//', '//b'), '//b');
	
	  assert.equal(libUtil.join('a', '..'), '.');
	  assert.equal(libUtil.join('a', '../b'), 'b');
	  assert.equal(libUtil.join('a/b', '../c'), 'a/c');
	
	  assert.equal(libUtil.join('a', '.'), 'a');
	  assert.equal(libUtil.join('a', './b'), 'a/b');
	  assert.equal(libUtil.join('a/b', './c'), 'a/b/c');
	
	  assert.equal(libUtil.join('a', 'http://www.example.com'), 'http://www.example.com');
	  assert.equal(libUtil.join('a', 'data:foo,bar'), 'data:foo,bar');
	
	
	  assert.equal(libUtil.join('', 'b'), 'b');
	  assert.equal(libUtil.join('.', 'b'), 'b');
	  assert.equal(libUtil.join('', 'b/'), 'b/');
	  assert.equal(libUtil.join('.', 'b/'), 'b/');
	  assert.equal(libUtil.join('', 'b//'), 'b/');
	  assert.equal(libUtil.join('.', 'b//'), 'b/');
	
	  assert.equal(libUtil.join('', '..'), '..');
	  assert.equal(libUtil.join('.', '..'), '..');
	  assert.equal(libUtil.join('', '../b'), '../b');
	  assert.equal(libUtil.join('.', '../b'), '../b');
	
	  assert.equal(libUtil.join('', '.'), '.');
	  assert.equal(libUtil.join('.', '.'), '.');
	  assert.equal(libUtil.join('', './b'), 'b');
	  assert.equal(libUtil.join('.', './b'), 'b');
	
	  assert.equal(libUtil.join('', 'http://www.example.com'), 'http://www.example.com');
	  assert.equal(libUtil.join('.', 'http://www.example.com'), 'http://www.example.com');
	  assert.equal(libUtil.join('', 'data:foo,bar'), 'data:foo,bar');
	  assert.equal(libUtil.join('.', 'data:foo,bar'), 'data:foo,bar');
	
	
	  assert.equal(libUtil.join('..', 'b'), '../b');
	  assert.equal(libUtil.join('..', 'b/'), '../b/');
	  assert.equal(libUtil.join('..', 'b//'), '../b/');
	
	  assert.equal(libUtil.join('..', '..'), '../..');
	  assert.equal(libUtil.join('..', '../b'), '../../b');
	
	  assert.equal(libUtil.join('..', '.'), '..');
	  assert.equal(libUtil.join('..', './b'), '../b');
	
	  assert.equal(libUtil.join('..', 'http://www.example.com'), 'http://www.example.com');
	  assert.equal(libUtil.join('..', 'data:foo,bar'), 'data:foo,bar');
	
	
	  assert.equal(libUtil.join('a', ''), 'a');
	  assert.equal(libUtil.join('a', '.'), 'a');
	  assert.equal(libUtil.join('a/', ''), 'a');
	  assert.equal(libUtil.join('a/', '.'), 'a');
	  assert.equal(libUtil.join('a//', ''), 'a');
	  assert.equal(libUtil.join('a//', '.'), 'a');
	  assert.equal(libUtil.join('/a', ''), '/a');
	  assert.equal(libUtil.join('/a', '.'), '/a');
	  assert.equal(libUtil.join('', ''), '.');
	  assert.equal(libUtil.join('.', ''), '.');
	  assert.equal(libUtil.join('.', ''), '.');
	  assert.equal(libUtil.join('.', '.'), '.');
	  assert.equal(libUtil.join('..', ''), '..');
	  assert.equal(libUtil.join('..', '.'), '..');
	  assert.equal(libUtil.join('http://foo.org/a', ''), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/a', '.'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/a/', ''), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/a/', '.'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/a//', ''), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/a//', '.'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org', ''), 'http://foo.org/');
	  assert.equal(libUtil.join('http://foo.org', '.'), 'http://foo.org/');
	  assert.equal(libUtil.join('http://foo.org/', ''), 'http://foo.org/');
	  assert.equal(libUtil.join('http://foo.org/', '.'), 'http://foo.org/');
	  assert.equal(libUtil.join('http://foo.org//', ''), 'http://foo.org/');
	  assert.equal(libUtil.join('http://foo.org//', '.'), 'http://foo.org/');
	  assert.equal(libUtil.join('//www.example.com', ''), '//www.example.com/');
	  assert.equal(libUtil.join('//www.example.com', '.'), '//www.example.com/');
	
	
	  assert.equal(libUtil.join('http://foo.org/a', 'b'), 'http://foo.org/a/b');
	  assert.equal(libUtil.join('http://foo.org/a/', 'b'), 'http://foo.org/a/b');
	  assert.equal(libUtil.join('http://foo.org/a//', 'b'), 'http://foo.org/a/b');
	  assert.equal(libUtil.join('http://foo.org/a', 'b/'), 'http://foo.org/a/b/');
	  assert.equal(libUtil.join('http://foo.org/a', 'b//'), 'http://foo.org/a/b/');
	  assert.equal(libUtil.join('http://foo.org/a/', '/b'), 'http://foo.org/b');
	  assert.equal(libUtil.join('http://foo.org/a//', '//b'), 'http://b');
	
	  assert.equal(libUtil.join('http://foo.org/a', '..'), 'http://foo.org/');
	  assert.equal(libUtil.join('http://foo.org/a', '../b'), 'http://foo.org/b');
	  assert.equal(libUtil.join('http://foo.org/a/b', '../c'), 'http://foo.org/a/c');
	
	  assert.equal(libUtil.join('http://foo.org/a', '.'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/a', './b'), 'http://foo.org/a/b');
	  assert.equal(libUtil.join('http://foo.org/a/b', './c'), 'http://foo.org/a/b/c');
	
	  assert.equal(libUtil.join('http://foo.org/a', 'http://www.example.com'), 'http://www.example.com');
	  assert.equal(libUtil.join('http://foo.org/a', 'data:foo,bar'), 'data:foo,bar');
	
	
	  assert.equal(libUtil.join('http://foo.org', 'a'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/', 'a'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org//', 'a'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org', '/a'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org/', '/a'), 'http://foo.org/a');
	  assert.equal(libUtil.join('http://foo.org//', '/a'), 'http://foo.org/a');
	
	
	  assert.equal(libUtil.join('http://', 'www.example.com'), 'http://www.example.com');
	  assert.equal(libUtil.join('file:///', 'www.example.com'), 'file:///www.example.com');
	  assert.equal(libUtil.join('http://', 'ftp://example.com'), 'ftp://example.com');
	
	  assert.equal(libUtil.join('http://www.example.com', '//foo.org/bar'), 'http://foo.org/bar');
	  assert.equal(libUtil.join('//www.example.com', '//foo.org/bar'), '//foo.org/bar');
	};
	
	// TODO Issue #128: Define and test this function properly.
	exports['test relative()'] = function (assert) {
	  assert.equal(libUtil.relative('/the/root', '/the/root/one.js'), 'one.js');
	  assert.equal(libUtil.relative('http://the/root', 'http://the/root/one.js'), 'one.js');
	  assert.equal(libUtil.relative('/the/root', '/the/rootone.js'), '../rootone.js');
	  assert.equal(libUtil.relative('http://the/root', 'http://the/rootone.js'), '../rootone.js');
	  assert.equal(libUtil.relative('/the/root', '/therootone.js'), '/therootone.js');
	  assert.equal(libUtil.relative('http://the/root', '/therootone.js'), '/therootone.js');
	
	  assert.equal(libUtil.relative('', '/the/root/one.js'), '/the/root/one.js');
	  assert.equal(libUtil.relative('.', '/the/root/one.js'), '/the/root/one.js');
	  assert.equal(libUtil.relative('', 'the/root/one.js'), 'the/root/one.js');
	  assert.equal(libUtil.relative('.', 'the/root/one.js'), 'the/root/one.js');
	
	  assert.equal(libUtil.relative('/', '/the/root/one.js'), 'the/root/one.js');
	  assert.equal(libUtil.relative('/', 'the/root/one.js'), 'the/root/one.js');
	};
	
	exports['test computeSourceURL'] = function (assert) {
	  // Tests with sourceMapURL.
	  assert.equal(libUtil.computeSourceURL('', 'src/test.js', 'http://example.com'),
	               'http://example.com/src/test.js');
	  assert.equal(libUtil.computeSourceURL(undefined, 'src/test.js', 'http://example.com'),
	               'http://example.com/src/test.js');
	  assert.equal(libUtil.computeSourceURL('src', 'test.js', 'http://example.com'),
	               'http://example.com/src/test.js');
	  assert.equal(libUtil.computeSourceURL('src/', 'test.js', 'http://example.com'),
	               'http://example.com/src/test.js');
	  assert.equal(libUtil.computeSourceURL('src', '/test.js', 'http://example.com'),
	               'http://example.com/src/test.js');
	  assert.equal(libUtil.computeSourceURL('http://mozilla.com', 'src/test.js', 'http://example.com'),
	               'http://mozilla.com/src/test.js');
	  assert.equal(libUtil.computeSourceURL('', 'test.js', 'http://example.com/src/test.js.map'),
	               'http://example.com/src/test.js');
	
	  // Legacy code won't pass in the sourceMapURL.
	  assert.equal(libUtil.computeSourceURL('', 'src/test.js'), 'src/test.js');
	  assert.equal(libUtil.computeSourceURL(undefined, 'src/test.js'), 'src/test.js');
	  assert.equal(libUtil.computeSourceURL('src', 'test.js'), 'src/test.js');
	  assert.equal(libUtil.computeSourceURL('src/', 'test.js'), 'src/test.js');
	  assert.equal(libUtil.computeSourceURL('src', '/test.js'), 'src/test.js');
	  assert.equal(libUtil.computeSourceURL('src', '../test.js'), 'test.js');
	  assert.equal(libUtil.computeSourceURL('src/dir', '../././../test.js'), 'test.js');
	
	  // This gives different results with the old algorithm and the new
	  // spec-compliant algorithm.
	  assert.equal(libUtil.computeSourceURL('http://example.com/dir', '/test.js'),
	               'http://example.com/dir/test.js');
	};


/***/ }),
/* 1 */
/***/ (function(module, exports) {

	/* -*- Mode: js; js-indent-level: 2; -*- */
	/*
	 * Copyright 2011 Mozilla Foundation and contributors
	 * Licensed under the New BSD license. See LICENSE or:
	 * http://opensource.org/licenses/BSD-3-Clause
	 */
	
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
	
	var urlRegexp = /^(?:([\w+\-.]+):)?\/\/(?:(\w+:\w+)@)?([\w.-]*)(?::(\d+))?(.*)$/;
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
	 * - Replaces consecutive slashes with one slash.
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
	  return aPath.charAt(0) === '/' || urlRegexp.test(aPath);
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
	
	var supportsNullProto = (function () {
	  var obj = Object.create(null);
	  return !('__proto__' in obj);
	}());
	
	function identity (s) {
	  return s;
	}
	
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
	  if (isProtoString(aStr)) {
	    return '$' + aStr;
	  }
	
	  return aStr;
	}
	exports.toSetString = supportsNullProto ? identity : toSetString;
	
	function fromSetString(aStr) {
	  if (isProtoString(aStr)) {
	    return aStr.slice(1);
	  }
	
	  return aStr;
	}
	exports.fromSetString = supportsNullProto ? identity : fromSetString;
	
	function isProtoString(s) {
	  if (!s) {
	    return false;
	  }
	
	  var length = s.length;
	
	  if (length < 9 /* "__proto__".length */) {
	    return false;
	  }
	
	  if (s.charCodeAt(length - 1) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 2) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 3) !== 111 /* 'o' */ ||
	      s.charCodeAt(length - 4) !== 116 /* 't' */ ||
	      s.charCodeAt(length - 5) !== 111 /* 'o' */ ||
	      s.charCodeAt(length - 6) !== 114 /* 'r' */ ||
	      s.charCodeAt(length - 7) !== 112 /* 'p' */ ||
	      s.charCodeAt(length - 8) !== 95  /* '_' */ ||
	      s.charCodeAt(length - 9) !== 95  /* '_' */) {
	    return false;
	  }
	
	  for (var i = length - 10; i >= 0; i--) {
	    if (s.charCodeAt(i) !== 36 /* '$' */) {
	      return false;
	    }
	  }
	
	  return true;
	}
	
	/**
	 * Comparator between two mappings where the original positions are compared.
	 *
	 * Optionally pass in `true` as `onlyCompareGenerated` to consider two
	 * mappings with the same original source/line/column, but different generated
	 * line and column the same. Useful when searching for a mapping with a
	 * stubbed out mapping.
	 */
	function compareByOriginalPositions(mappingA, mappingB, onlyCompareOriginal) {
	  var cmp = strcmp(mappingA.source, mappingB.source);
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
	
	  return strcmp(mappingA.name, mappingB.name);
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
	exports.compareByGeneratedPositionsDeflated = compareByGeneratedPositionsDeflated;
	
	function strcmp(aStr1, aStr2) {
	  if (aStr1 === aStr2) {
	    return 0;
	  }
	
	  if (aStr1 === null) {
	    return 1; // aStr2 !== null
	  }
	
	  if (aStr2 === null) {
	    return -1; // aStr1 !== null
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
	
	/**
	 * Strip any JSON XSSI avoidance prefix from the string (as documented
	 * in the source maps specification), and then parse the string as
	 * JSON.
	 */
	function parseSourceMapInput(str) {
	  return JSON.parse(str.replace(/^\)]}'[^\n]*\n/, ''));
	}
	exports.parseSourceMapInput = parseSourceMapInput;
	
	/**
	 * Compute the URL of a source given the the source root, the source's
	 * URL, and the source map's URL.
	 */
	function computeSourceURL(sourceRoot, sourceURL, sourceMapURL) {
	  sourceURL = sourceURL || '';
	
	  if (sourceRoot) {
	    // This follows what Chrome does.
	    if (sourceRoot[sourceRoot.length - 1] !== '/' && sourceURL[0] !== '/') {
	      sourceRoot += '/';
	    }
	    // The spec says:
	    //   Line 4: An optional source root, useful for relocating source
	    //   files on a server or removing repeated values in the
	    //   “sources” entry.  This value is prepended to the individual
	    //   entries in the “source” field.
	    sourceURL = sourceRoot + sourceURL;
	  }
	
	  // Historically, SourceMapConsumer did not take the sourceMapURL as
	  // a parameter.  This mode is still somewhat supported, which is why
	  // this code block is conditional.  However, it's preferable to pass
	  // the source map URL to SourceMapConsumer, so that this function
	  // can implement the source URL resolution algorithm as outlined in
	  // the spec.  This block is basically the equivalent of:
	  //    new URL(sourceURL, sourceMapURL).toString()
	  // ... except it avoids using URL, which wasn't available in the
	  // older releases of node still supported by this library.
	  //
	  // The spec says:
	  //   If the sources are not absolute URLs after prepending of the
	  //   “sourceRoot”, the sources are resolved relative to the
	  //   SourceMap (like resolving script src in a html document).
	  if (sourceMapURL) {
	    var parsed = urlParse(sourceMapURL);
	    if (!parsed) {
	      throw new Error("sourceMapURL could not be parsed");
	    }
	    if (parsed.path) {
	      // Strip the last path component, but keep the "/".
	      var index = parsed.path.lastIndexOf('/');
	      if (index >= 0) {
	        parsed.path = parsed.path.substring(0, index + 1);
	      }
	    }
	    sourceURL = join(urlGenerate(parsed), sourceURL);
	  }
	
	  return normalize(sourceURL);
	}
	exports.computeSourceURL = computeSourceURL;


/***/ })
/******/ ]);
//# sourceMappingURL=data:application/json;charset=utf-8;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgYjQ4ZDVkYjU2ZTA1NDMwM2M4Y2YiLCJ3ZWJwYWNrOi8vLy4vdGVzdC90ZXN0LXV0aWwuanMiLCJ3ZWJwYWNrOi8vLy4vbGliL3V0aWwuanMiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6Ijs7Ozs7Ozs7Ozs7QUFBQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSx1QkFBZTtBQUNmO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOzs7QUFHQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBOzs7Ozs7O0FDdENBLGlCQUFnQixvQkFBb0I7QUFDcEM7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOzs7QUFHQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7OztBQUdBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTs7O0FBR0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7OztBQUdBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7O0FBR0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOzs7QUFHQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7Ozs7OztBQ2pRQSxpQkFBZ0Isb0JBQW9CO0FBQ3BDO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSxJQUFHO0FBQ0g7QUFDQSxJQUFHO0FBQ0g7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSwrQ0FBOEMsUUFBUTtBQUN0RDtBQUNBO0FBQ0E7QUFDQSxNQUFLO0FBQ0w7QUFDQSxNQUFLO0FBQ0w7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0EsUUFBTztBQUNQO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBLEVBQUM7O0FBRUQ7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUEsNEJBQTJCLFFBQVE7QUFDbkM7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQSxjQUFhO0FBQ2I7O0FBRUE7QUFDQSxlQUFjO0FBQ2Q7O0FBRUE7QUFDQTtBQUNBOztBQUVBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQSx1Q0FBc0M7QUFDdEM7QUFDQTs7QUFFQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7O0FBRUE7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBO0FBQ0E7QUFDQTtBQUNBOztBQUVBO0FBQ0E7QUFDQSIsImZpbGUiOiJ0ZXN0X3V0aWwuanMiLCJzb3VyY2VzQ29udGVudCI6WyIgXHQvLyBUaGUgbW9kdWxlIGNhY2hlXG4gXHR2YXIgaW5zdGFsbGVkTW9kdWxlcyA9IHt9O1xuXG4gXHQvLyBUaGUgcmVxdWlyZSBmdW5jdGlvblxuIFx0ZnVuY3Rpb24gX193ZWJwYWNrX3JlcXVpcmVfXyhtb2R1bGVJZCkge1xuXG4gXHRcdC8vIENoZWNrIGlmIG1vZHVsZSBpcyBpbiBjYWNoZVxuIFx0XHRpZihpbnN0YWxsZWRNb2R1bGVzW21vZHVsZUlkXSlcbiBcdFx0XHRyZXR1cm4gaW5zdGFsbGVkTW9kdWxlc1ttb2R1bGVJZF0uZXhwb3J0cztcblxuIFx0XHQvLyBDcmVhdGUgYSBuZXcgbW9kdWxlIChhbmQgcHV0IGl0IGludG8gdGhlIGNhY2hlKVxuIFx0XHR2YXIgbW9kdWxlID0gaW5zdGFsbGVkTW9kdWxlc1ttb2R1bGVJZF0gPSB7XG4gXHRcdFx0ZXhwb3J0czoge30sXG4gXHRcdFx0aWQ6IG1vZHVsZUlkLFxuIFx0XHRcdGxvYWRlZDogZmFsc2VcbiBcdFx0fTtcblxuIFx0XHQvLyBFeGVjdXRlIHRoZSBtb2R1bGUgZnVuY3Rpb25cbiBcdFx0bW9kdWxlc1ttb2R1bGVJZF0uY2FsbChtb2R1bGUuZXhwb3J0cywgbW9kdWxlLCBtb2R1bGUuZXhwb3J0cywgX193ZWJwYWNrX3JlcXVpcmVfXyk7XG5cbiBcdFx0Ly8gRmxhZyB0aGUgbW9kdWxlIGFzIGxvYWRlZFxuIFx0XHRtb2R1bGUubG9hZGVkID0gdHJ1ZTtcblxuIFx0XHQvLyBSZXR1cm4gdGhlIGV4cG9ydHMgb2YgdGhlIG1vZHVsZVxuIFx0XHRyZXR1cm4gbW9kdWxlLmV4cG9ydHM7XG4gXHR9XG5cblxuIFx0Ly8gZXhwb3NlIHRoZSBtb2R1bGVzIG9iamVjdCAoX193ZWJwYWNrX21vZHVsZXNfXylcbiBcdF9fd2VicGFja19yZXF1aXJlX18ubSA9IG1vZHVsZXM7XG5cbiBcdC8vIGV4cG9zZSB0aGUgbW9kdWxlIGNhY2hlXG4gXHRfX3dlYnBhY2tfcmVxdWlyZV9fLmMgPSBpbnN0YWxsZWRNb2R1bGVzO1xuXG4gXHQvLyBfX3dlYnBhY2tfcHVibGljX3BhdGhfX1xuIFx0X193ZWJwYWNrX3JlcXVpcmVfXy5wID0gXCJcIjtcblxuIFx0Ly8gTG9hZCBlbnRyeSBtb2R1bGUgYW5kIHJldHVybiBleHBvcnRzXG4gXHRyZXR1cm4gX193ZWJwYWNrX3JlcXVpcmVfXygwKTtcblxuXG5cbi8vIFdFQlBBQ0sgRk9PVEVSIC8vXG4vLyB3ZWJwYWNrL2Jvb3RzdHJhcCBiNDhkNWRiNTZlMDU0MzAzYzhjZiIsIi8qIC0qLSBNb2RlOiBqczsganMtaW5kZW50LWxldmVsOiAyOyAtKi0gKi9cbi8qXG4gKiBDb3B5cmlnaHQgMjAxNCBNb3ppbGxhIEZvdW5kYXRpb24gYW5kIGNvbnRyaWJ1dG9yc1xuICogTGljZW5zZWQgdW5kZXIgdGhlIE5ldyBCU0QgbGljZW5zZS4gU2VlIExJQ0VOU0Ugb3I6XG4gKiBodHRwOi8vb3BlbnNvdXJjZS5vcmcvbGljZW5zZXMvQlNELTMtQ2xhdXNlXG4gKi9cblxudmFyIGxpYlV0aWwgPSByZXF1aXJlKCcuLi9saWIvdXRpbCcpO1xuXG5leHBvcnRzWyd0ZXN0IHVybHMnXSA9IGZ1bmN0aW9uIChhc3NlcnQpIHtcbiAgdmFyIGFzc2VydFVybCA9IGZ1bmN0aW9uICh1cmwpIHtcbiAgICBhc3NlcnQuZXF1YWwodXJsLCBsaWJVdGlsLnVybEdlbmVyYXRlKGxpYlV0aWwudXJsUGFyc2UodXJsKSkpO1xuICB9O1xuICBhc3NlcnRVcmwoJ2h0dHA6Ly8nKTtcbiAgYXNzZXJ0VXJsKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyk7XG4gIGFzc2VydFVybCgnaHR0cDovL3VzZXI6cGFzc0B3d3cuZXhhbXBsZS5jb20nKTtcbiAgYXNzZXJ0VXJsKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tOjgwJyk7XG4gIGFzc2VydFVybCgnaHR0cDovL3d3dy5leGFtcGxlLmNvbS8nKTtcbiAgYXNzZXJ0VXJsKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tL2Zvby9iYXInKTtcbiAgYXNzZXJ0VXJsKCdodHRwOi8vd3d3LmV4YW1wbGUuY29tL2Zvby9iYXIvJyk7XG4gIGFzc2VydFVybCgnaHR0cDovL3VzZXI6cGFzc0B3d3cuZXhhbXBsZS5jb206ODAvZm9vL2Jhci8nKTtcblxuICBhc3NlcnRVcmwoJy8vJyk7XG4gIGFzc2VydFVybCgnLy93d3cuZXhhbXBsZS5jb20nKTtcbiAgYXNzZXJ0VXJsKCdmaWxlOi8vL3d3dy5leGFtcGxlLmNvbScpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLnVybFBhcnNlKCcnKSwgbnVsbCk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLnVybFBhcnNlKCcuJyksIG51bGwpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnLi4nKSwgbnVsbCk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLnVybFBhcnNlKCdhJyksIG51bGwpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnYS9iJyksIG51bGwpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnYS8vYicpLCBudWxsKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwudXJsUGFyc2UoJy9hJyksIG51bGwpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC51cmxQYXJzZSgnZGF0YTpmb28sYmFyJyksIG51bGwpO1xuXG4gIHZhciBwYXJzZWQgPSBsaWJVdGlsLnVybFBhcnNlKCdodHRwOi8veC15LmNvbS9iYXInKTtcbiAgYXNzZXJ0LmVxdWFsKHBhcnNlZC5zY2hlbWUsICdodHRwJyk7XG4gIGFzc2VydC5lcXVhbChwYXJzZWQuaG9zdCwgJ3gteS5jb20nKTtcbiAgYXNzZXJ0LmVxdWFsKHBhcnNlZC5wYXRoLCAnL2JhcicpO1xuXG4gIHZhciB3ZWJwYWNrVVJMID0gJ3dlYnBhY2s6Ly8vd2VicGFjay9ib290c3RyYXAgNjdlMTg0Zjk2Nzk3MzMyOThkNDQnXG4gIHBhcnNlZCA9IGxpYlV0aWwudXJsUGFyc2Uod2VicGFja1VSTCk7XG4gIGFzc2VydC5lcXVhbChwYXJzZWQuc2NoZW1lLCAnd2VicGFjaycpO1xuICBhc3NlcnQuZXF1YWwocGFyc2VkLmhvc3QsICcnKTtcbiAgYXNzZXJ0LmVxdWFsKHBhcnNlZC5wYXRoLCAnL3dlYnBhY2svYm9vdHN0cmFwIDY3ZTE4NGY5Njc5NzMzMjk4ZDQ0Jyk7XG4gIGFzc2VydC5lcXVhbCh3ZWJwYWNrVVJMLCBsaWJVdGlsLnVybEdlbmVyYXRlKHBhcnNlZCkpO1xufTtcblxuZXhwb3J0c1sndGVzdCBub3JtYWxpemUoKSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy8uLicpLCAnLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy8uLi8nKSwgJy8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLi4vLi4vLi4vLi4nKSwgJy8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLi4vLi4vLi4vLi4vYS9iL2MnKSwgJy9hL2IvYycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy9hL2IvYy8uLi8uLi8uLi9kLy4uLy4uL2UnKSwgJy9lJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcuLicpLCAnLi4nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcuLi8nKSwgJy4uLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy4uLy4uL2EvJyksICcuLi8uLi9hLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJ2EvLi4nKSwgJy4nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdhLy4uLy4uLy4uJyksICcuLi8uLicpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLy4nKSwgJy8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLi8nKSwgJy8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLi8uLy4vLicpLCAnLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy8uLy4vLi8uL2EvYi9jJyksICcvYS9iL2MnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvYS9iL2MvLi8uLy4vZC8uLy4vZScpLCAnL2EvYi9jL2QvZScpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnJyksICcuJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLicpLCAnLicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy4vJyksICcuJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnLi8uL2EnKSwgJ2EnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCdhLy4vJyksICdhLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJ2EvLi8uLy4nKSwgJ2EnKTtcblxuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJy9hL2IvL2MvLy8vZC8vLy8vJyksICcvYS9iL2MvZC8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcvLy9hL2IvL2MvLy8vZC8vLy8vJyksICcvLy9hL2IvYy9kLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJ2EvYi8vYy8vLy9kJyksICdhL2IvYy9kJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwubm9ybWFsaXplKCcuLy8vLi4vLi8uLi9hL2IvLy4vLi4nKSwgJy4uLy4uL2EnKVxuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLm5vcm1hbGl6ZSgnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20vJyksICdodHRwOi8vd3d3LmV4YW1wbGUuY29tLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5ub3JtYWxpemUoJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20vLi8uLi8vYS9iL2MvLi4vLi9kLy8nKSwgJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20vYS9iL2QvJyk7XG59O1xuXG5leHBvcnRzWyd0ZXN0IGpvaW4oKSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJ2InKSwgJ2EvYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhLycsICdiJyksICdhL2InKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYS8vJywgJ2InKSwgJ2EvYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJ2IvJyksICdhL2IvJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EnLCAnYi8vJyksICdhL2IvJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EvJywgJy9iJyksICcvYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhLy8nLCAnLy9iJyksICcvL2InKTtcblxuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJy4uJyksICcuJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EnLCAnLi4vYicpLCAnYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhL2InLCAnLi4vYycpLCAnYS9jJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYScsICcuJyksICdhJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EnLCAnLi9iJyksICdhL2InKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYS9iJywgJy4vYycpLCAnYS9iL2MnKTtcblxuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhJywgJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20nKSwgJ2h0dHA6Ly93d3cuZXhhbXBsZS5jb20nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYScsICdkYXRhOmZvbyxiYXInKSwgJ2RhdGE6Zm9vLGJhcicpO1xuXG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJ2InKSwgJ2InKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLicsICdiJyksICdiJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJycsICdiLycpLCAnYi8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLicsICdiLycpLCAnYi8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJ2IvLycpLCAnYi8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLicsICdiLy8nKSwgJ2IvJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJy4uJyksICcuLicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuJywgJy4uJyksICcuLicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcnLCAnLi4vYicpLCAnLi4vYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuJywgJy4uL2InKSwgJy4uL2InKTtcblxuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcnLCAnLicpLCAnLicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuJywgJy4nKSwgJy4nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignJywgJy4vYicpLCAnYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuJywgJy4vYicpLCAnYicpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJycsICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyksICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcnLCAnZGF0YTpmb28sYmFyJyksICdkYXRhOmZvbyxiYXInKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLicsICdkYXRhOmZvbyxiYXInKSwgJ2RhdGE6Zm9vLGJhcicpO1xuXG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnYicpLCAnLi4vYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuLicsICdiLycpLCAnLi4vYi8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnYi8vJyksICcuLi9iLycpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4uJywgJy4uJyksICcuLi8uLicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuLicsICcuLi9iJyksICcuLi8uLi9iJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnLicpLCAnLi4nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnLi9iJyksICcuLi9iJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLi4nLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuLicsICdkYXRhOmZvbyxiYXInKSwgJ2RhdGE6Zm9vLGJhcicpO1xuXG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYScsICcnKSwgJ2EnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYScsICcuJyksICdhJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EvJywgJycpLCAnYScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdhLycsICcuJyksICdhJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2EvLycsICcnKSwgJ2EnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignYS8vJywgJy4nKSwgJ2EnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignL2EnLCAnJyksICcvYScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcvYScsICcuJyksICcvYScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcnLCAnJyksICcuJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnJyksICcuJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnJyksICcuJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4nLCAnLicpLCAnLicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCcuLicsICcnKSwgJy4uJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy4uJywgJy4nKSwgJy4uJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnLicpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hLycsICcnKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYS8nLCAnLicpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hLy8nLCAnJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvLycsICcuJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnJywgJycpLCAnaHR0cDovL2Zvby5vcmcvJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnJywgJy4nKSwgJ2h0dHA6Ly9mb28ub3JnLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy8nLCAnJyksICdodHRwOi8vZm9vLm9yZy8nKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvJywgJy4nKSwgJ2h0dHA6Ly9mb28ub3JnLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy8vJywgJycpLCAnaHR0cDovL2Zvby5vcmcvJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnLy8nLCAnLicpLCAnaHR0cDovL2Zvby5vcmcvJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy8vd3d3LmV4YW1wbGUuY29tJywgJycpLCAnLy93d3cuZXhhbXBsZS5jb20vJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJy8vd3d3LmV4YW1wbGUuY29tJywgJy4nKSwgJy8vd3d3LmV4YW1wbGUuY29tLycpO1xuXG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYScsICdiJyksICdodHRwOi8vZm9vLm9yZy9hL2InKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYS8nLCAnYicpLCAnaHR0cDovL2Zvby5vcmcvYS9iJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvLycsICdiJyksICdodHRwOi8vZm9vLm9yZy9hL2InKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYScsICdiLycpLCAnaHR0cDovL2Zvby5vcmcvYS9iLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJ2IvLycpLCAnaHR0cDovL2Zvby5vcmcvYS9iLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hLycsICcvYicpLCAnaHR0cDovL2Zvby5vcmcvYicpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hLy8nLCAnLy9iJyksICdodHRwOi8vYicpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnLi4nKSwgJ2h0dHA6Ly9mb28ub3JnLycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJy4uL2InKSwgJ2h0dHA6Ly9mb28ub3JnL2InKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYS9iJywgJy4uL2MnKSwgJ2h0dHA6Ly9mb28ub3JnL2EvYycpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnLicpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy9hJywgJy4vYicpLCAnaHR0cDovL2Zvby5vcmcvYS9iJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EvYicsICcuL2MnKSwgJ2h0dHA6Ly9mb28ub3JnL2EvYi9jJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvYScsICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyksICdodHRwOi8vd3d3LmV4YW1wbGUuY29tJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnL2EnLCAnZGF0YTpmb28sYmFyJyksICdkYXRhOmZvbyxiYXInKTtcblxuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnJywgJ2EnKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvJywgJ2EnKSwgJ2h0dHA6Ly9mb28ub3JnL2EnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL2Zvby5vcmcvLycsICdhJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnJywgJy9hJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly9mb28ub3JnLycsICcvYScpLCAnaHR0cDovL2Zvby5vcmcvYScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vZm9vLm9yZy8vJywgJy9hJyksICdodHRwOi8vZm9vLm9yZy9hJyk7XG5cblxuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdodHRwOi8vJywgJ3d3dy5leGFtcGxlLmNvbScpLCAnaHR0cDovL3d3dy5leGFtcGxlLmNvbScpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5qb2luKCdmaWxlOi8vLycsICd3d3cuZXhhbXBsZS5jb20nKSwgJ2ZpbGU6Ly8vd3d3LmV4YW1wbGUuY29tJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmpvaW4oJ2h0dHA6Ly8nLCAnZnRwOi8vZXhhbXBsZS5jb20nKSwgJ2Z0cDovL2V4YW1wbGUuY29tJyk7XG5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignaHR0cDovL3d3dy5leGFtcGxlLmNvbScsICcvL2Zvby5vcmcvYmFyJyksICdodHRwOi8vZm9vLm9yZy9iYXInKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuam9pbignLy93d3cuZXhhbXBsZS5jb20nLCAnLy9mb28ub3JnL2JhcicpLCAnLy9mb28ub3JnL2JhcicpO1xufTtcblxuLy8gVE9ETyBJc3N1ZSAjMTI4OiBEZWZpbmUgYW5kIHRlc3QgdGhpcyBmdW5jdGlvbiBwcm9wZXJseS5cbmV4cG9ydHNbJ3Rlc3QgcmVsYXRpdmUoKSddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnL3RoZS9yb290JywgJy90aGUvcm9vdC9vbmUuanMnKSwgJ29uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnaHR0cDovL3RoZS9yb290JywgJ2h0dHA6Ly90aGUvcm9vdC9vbmUuanMnKSwgJ29uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnL3RoZS9yb290JywgJy90aGUvcm9vdG9uZS5qcycpLCAnLi4vcm9vdG9uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnaHR0cDovL3RoZS9yb290JywgJ2h0dHA6Ly90aGUvcm9vdG9uZS5qcycpLCAnLi4vcm9vdG9uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnL3RoZS9yb290JywgJy90aGVyb290b25lLmpzJyksICcvdGhlcm9vdG9uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnaHR0cDovL3RoZS9yb290JywgJy90aGVyb290b25lLmpzJyksICcvdGhlcm9vdG9uZS5qcycpO1xuXG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCcnLCAnL3RoZS9yb290L29uZS5qcycpLCAnL3RoZS9yb290L29uZS5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnLicsICcvdGhlL3Jvb3Qvb25lLmpzJyksICcvdGhlL3Jvb3Qvb25lLmpzJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLnJlbGF0aXZlKCcnLCAndGhlL3Jvb3Qvb25lLmpzJyksICd0aGUvcm9vdC9vbmUuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwucmVsYXRpdmUoJy4nLCAndGhlL3Jvb3Qvb25lLmpzJyksICd0aGUvcm9vdC9vbmUuanMnKTtcblxuICBhc3NlcnQuZXF1YWwobGliVXRpbC5yZWxhdGl2ZSgnLycsICcvdGhlL3Jvb3Qvb25lLmpzJyksICd0aGUvcm9vdC9vbmUuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwucmVsYXRpdmUoJy8nLCAndGhlL3Jvb3Qvb25lLmpzJyksICd0aGUvcm9vdC9vbmUuanMnKTtcbn07XG5cbmV4cG9ydHNbJ3Rlc3QgY29tcHV0ZVNvdXJjZVVSTCddID0gZnVuY3Rpb24gKGFzc2VydCkge1xuICAvLyBUZXN0cyB3aXRoIHNvdXJjZU1hcFVSTC5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuY29tcHV0ZVNvdXJjZVVSTCgnJywgJ3NyYy90ZXN0LmpzJywgJ2h0dHA6Ly9leGFtcGxlLmNvbScpLFxuICAgICAgICAgICAgICAgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zcmMvdGVzdC5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5jb21wdXRlU291cmNlVVJMKHVuZGVmaW5lZCwgJ3NyYy90ZXN0LmpzJywgJ2h0dHA6Ly9leGFtcGxlLmNvbScpLFxuICAgICAgICAgICAgICAgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zcmMvdGVzdC5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5jb21wdXRlU291cmNlVVJMKCdzcmMnLCAndGVzdC5qcycsICdodHRwOi8vZXhhbXBsZS5jb20nKSxcbiAgICAgICAgICAgICAgICdodHRwOi8vZXhhbXBsZS5jb20vc3JjL3Rlc3QuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuY29tcHV0ZVNvdXJjZVVSTCgnc3JjLycsICd0ZXN0LmpzJywgJ2h0dHA6Ly9leGFtcGxlLmNvbScpLFxuICAgICAgICAgICAgICAgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zcmMvdGVzdC5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5jb21wdXRlU291cmNlVVJMKCdzcmMnLCAnL3Rlc3QuanMnLCAnaHR0cDovL2V4YW1wbGUuY29tJyksXG4gICAgICAgICAgICAgICAnaHR0cDovL2V4YW1wbGUuY29tL3NyYy90ZXN0LmpzJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmNvbXB1dGVTb3VyY2VVUkwoJ2h0dHA6Ly9tb3ppbGxhLmNvbScsICdzcmMvdGVzdC5qcycsICdodHRwOi8vZXhhbXBsZS5jb20nKSxcbiAgICAgICAgICAgICAgICdodHRwOi8vbW96aWxsYS5jb20vc3JjL3Rlc3QuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuY29tcHV0ZVNvdXJjZVVSTCgnJywgJ3Rlc3QuanMnLCAnaHR0cDovL2V4YW1wbGUuY29tL3NyYy90ZXN0LmpzLm1hcCcpLFxuICAgICAgICAgICAgICAgJ2h0dHA6Ly9leGFtcGxlLmNvbS9zcmMvdGVzdC5qcycpO1xuXG4gIC8vIExlZ2FjeSBjb2RlIHdvbid0IHBhc3MgaW4gdGhlIHNvdXJjZU1hcFVSTC5cbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuY29tcHV0ZVNvdXJjZVVSTCgnJywgJ3NyYy90ZXN0LmpzJyksICdzcmMvdGVzdC5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5jb21wdXRlU291cmNlVVJMKHVuZGVmaW5lZCwgJ3NyYy90ZXN0LmpzJyksICdzcmMvdGVzdC5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5jb21wdXRlU291cmNlVVJMKCdzcmMnLCAndGVzdC5qcycpLCAnc3JjL3Rlc3QuanMnKTtcbiAgYXNzZXJ0LmVxdWFsKGxpYlV0aWwuY29tcHV0ZVNvdXJjZVVSTCgnc3JjLycsICd0ZXN0LmpzJyksICdzcmMvdGVzdC5qcycpO1xuICBhc3NlcnQuZXF1YWwobGliVXRpbC5jb21wdXRlU291cmNlVVJMKCdzcmMnLCAnL3Rlc3QuanMnKSwgJ3NyYy90ZXN0LmpzJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmNvbXB1dGVTb3VyY2VVUkwoJ3NyYycsICcuLi90ZXN0LmpzJyksICd0ZXN0LmpzJyk7XG4gIGFzc2VydC5lcXVhbChsaWJVdGlsLmNvbXB1dGVTb3VyY2VVUkwoJ3NyYy9kaXInLCAnLi4vLi8uLy4uL3Rlc3QuanMnKSwgJ3Rlc3QuanMnKTtcblxuICAvLyBUaGlzIGdpdmVzIGRpZmZlcmVudCByZXN1bHRzIHdpdGggdGhlIG9sZCBhbGdvcml0aG0gYW5kIHRoZSBuZXdcbiAgLy8gc3BlYy1jb21wbGlhbnQgYWxnb3JpdGhtLlxuICBhc3NlcnQuZXF1YWwobGliVXRpbC5jb21wdXRlU291cmNlVVJMKCdodHRwOi8vZXhhbXBsZS5jb20vZGlyJywgJy90ZXN0LmpzJyksXG4gICAgICAgICAgICAgICAnaHR0cDovL2V4YW1wbGUuY29tL2Rpci90ZXN0LmpzJyk7XG59O1xuXG5cblxuLy8vLy8vLy8vLy8vLy8vLy8vXG4vLyBXRUJQQUNLIEZPT1RFUlxuLy8gLi90ZXN0L3Rlc3QtdXRpbC5qc1xuLy8gbW9kdWxlIGlkID0gMFxuLy8gbW9kdWxlIGNodW5rcyA9IDAiLCIvKiAtKi0gTW9kZToganM7IGpzLWluZGVudC1sZXZlbDogMjsgLSotICovXG4vKlxuICogQ29weXJpZ2h0IDIwMTEgTW96aWxsYSBGb3VuZGF0aW9uIGFuZCBjb250cmlidXRvcnNcbiAqIExpY2Vuc2VkIHVuZGVyIHRoZSBOZXcgQlNEIGxpY2Vuc2UuIFNlZSBMSUNFTlNFIG9yOlxuICogaHR0cDovL29wZW5zb3VyY2Uub3JnL2xpY2Vuc2VzL0JTRC0zLUNsYXVzZVxuICovXG5cbi8qKlxuICogVGhpcyBpcyBhIGhlbHBlciBmdW5jdGlvbiBmb3IgZ2V0dGluZyB2YWx1ZXMgZnJvbSBwYXJhbWV0ZXIvb3B0aW9uc1xuICogb2JqZWN0cy5cbiAqXG4gKiBAcGFyYW0gYXJncyBUaGUgb2JqZWN0IHdlIGFyZSBleHRyYWN0aW5nIHZhbHVlcyBmcm9tXG4gKiBAcGFyYW0gbmFtZSBUaGUgbmFtZSBvZiB0aGUgcHJvcGVydHkgd2UgYXJlIGdldHRpbmcuXG4gKiBAcGFyYW0gZGVmYXVsdFZhbHVlIEFuIG9wdGlvbmFsIHZhbHVlIHRvIHJldHVybiBpZiB0aGUgcHJvcGVydHkgaXMgbWlzc2luZ1xuICogZnJvbSB0aGUgb2JqZWN0LiBJZiB0aGlzIGlzIG5vdCBzcGVjaWZpZWQgYW5kIHRoZSBwcm9wZXJ0eSBpcyBtaXNzaW5nLCBhblxuICogZXJyb3Igd2lsbCBiZSB0aHJvd24uXG4gKi9cbmZ1bmN0aW9uIGdldEFyZyhhQXJncywgYU5hbWUsIGFEZWZhdWx0VmFsdWUpIHtcbiAgaWYgKGFOYW1lIGluIGFBcmdzKSB7XG4gICAgcmV0dXJuIGFBcmdzW2FOYW1lXTtcbiAgfSBlbHNlIGlmIChhcmd1bWVudHMubGVuZ3RoID09PSAzKSB7XG4gICAgcmV0dXJuIGFEZWZhdWx0VmFsdWU7XG4gIH0gZWxzZSB7XG4gICAgdGhyb3cgbmV3IEVycm9yKCdcIicgKyBhTmFtZSArICdcIiBpcyBhIHJlcXVpcmVkIGFyZ3VtZW50LicpO1xuICB9XG59XG5leHBvcnRzLmdldEFyZyA9IGdldEFyZztcblxudmFyIHVybFJlZ2V4cCA9IC9eKD86KFtcXHcrXFwtLl0rKTopP1xcL1xcLyg/OihcXHcrOlxcdyspQCk/KFtcXHcuLV0qKSg/OjooXFxkKykpPyguKikkLztcbnZhciBkYXRhVXJsUmVnZXhwID0gL15kYXRhOi4rXFwsLiskLztcblxuZnVuY3Rpb24gdXJsUGFyc2UoYVVybCkge1xuICB2YXIgbWF0Y2ggPSBhVXJsLm1hdGNoKHVybFJlZ2V4cCk7XG4gIGlmICghbWF0Y2gpIHtcbiAgICByZXR1cm4gbnVsbDtcbiAgfVxuICByZXR1cm4ge1xuICAgIHNjaGVtZTogbWF0Y2hbMV0sXG4gICAgYXV0aDogbWF0Y2hbMl0sXG4gICAgaG9zdDogbWF0Y2hbM10sXG4gICAgcG9ydDogbWF0Y2hbNF0sXG4gICAgcGF0aDogbWF0Y2hbNV1cbiAgfTtcbn1cbmV4cG9ydHMudXJsUGFyc2UgPSB1cmxQYXJzZTtcblxuZnVuY3Rpb24gdXJsR2VuZXJhdGUoYVBhcnNlZFVybCkge1xuICB2YXIgdXJsID0gJyc7XG4gIGlmIChhUGFyc2VkVXJsLnNjaGVtZSkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLnNjaGVtZSArICc6JztcbiAgfVxuICB1cmwgKz0gJy8vJztcbiAgaWYgKGFQYXJzZWRVcmwuYXV0aCkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLmF1dGggKyAnQCc7XG4gIH1cbiAgaWYgKGFQYXJzZWRVcmwuaG9zdCkge1xuICAgIHVybCArPSBhUGFyc2VkVXJsLmhvc3Q7XG4gIH1cbiAgaWYgKGFQYXJzZWRVcmwucG9ydCkge1xuICAgIHVybCArPSBcIjpcIiArIGFQYXJzZWRVcmwucG9ydFxuICB9XG4gIGlmIChhUGFyc2VkVXJsLnBhdGgpIHtcbiAgICB1cmwgKz0gYVBhcnNlZFVybC5wYXRoO1xuICB9XG4gIHJldHVybiB1cmw7XG59XG5leHBvcnRzLnVybEdlbmVyYXRlID0gdXJsR2VuZXJhdGU7XG5cbi8qKlxuICogTm9ybWFsaXplcyBhIHBhdGgsIG9yIHRoZSBwYXRoIHBvcnRpb24gb2YgYSBVUkw6XG4gKlxuICogLSBSZXBsYWNlcyBjb25zZWN1dGl2ZSBzbGFzaGVzIHdpdGggb25lIHNsYXNoLlxuICogLSBSZW1vdmVzIHVubmVjZXNzYXJ5ICcuJyBwYXJ0cy5cbiAqIC0gUmVtb3ZlcyB1bm5lY2Vzc2FyeSAnPGRpcj4vLi4nIHBhcnRzLlxuICpcbiAqIEJhc2VkIG9uIGNvZGUgaW4gdGhlIE5vZGUuanMgJ3BhdGgnIGNvcmUgbW9kdWxlLlxuICpcbiAqIEBwYXJhbSBhUGF0aCBUaGUgcGF0aCBvciB1cmwgdG8gbm9ybWFsaXplLlxuICovXG5mdW5jdGlvbiBub3JtYWxpemUoYVBhdGgpIHtcbiAgdmFyIHBhdGggPSBhUGF0aDtcbiAgdmFyIHVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgaWYgKHVybCkge1xuICAgIGlmICghdXJsLnBhdGgpIHtcbiAgICAgIHJldHVybiBhUGF0aDtcbiAgICB9XG4gICAgcGF0aCA9IHVybC5wYXRoO1xuICB9XG4gIHZhciBpc0Fic29sdXRlID0gZXhwb3J0cy5pc0Fic29sdXRlKHBhdGgpO1xuXG4gIHZhciBwYXJ0cyA9IHBhdGguc3BsaXQoL1xcLysvKTtcbiAgZm9yICh2YXIgcGFydCwgdXAgPSAwLCBpID0gcGFydHMubGVuZ3RoIC0gMTsgaSA+PSAwOyBpLS0pIHtcbiAgICBwYXJ0ID0gcGFydHNbaV07XG4gICAgaWYgKHBhcnQgPT09ICcuJykge1xuICAgICAgcGFydHMuc3BsaWNlKGksIDEpO1xuICAgIH0gZWxzZSBpZiAocGFydCA9PT0gJy4uJykge1xuICAgICAgdXArKztcbiAgICB9IGVsc2UgaWYgKHVwID4gMCkge1xuICAgICAgaWYgKHBhcnQgPT09ICcnKSB7XG4gICAgICAgIC8vIFRoZSBmaXJzdCBwYXJ0IGlzIGJsYW5rIGlmIHRoZSBwYXRoIGlzIGFic29sdXRlLiBUcnlpbmcgdG8gZ29cbiAgICAgICAgLy8gYWJvdmUgdGhlIHJvb3QgaXMgYSBuby1vcC4gVGhlcmVmb3JlIHdlIGNhbiByZW1vdmUgYWxsICcuLicgcGFydHNcbiAgICAgICAgLy8gZGlyZWN0bHkgYWZ0ZXIgdGhlIHJvb3QuXG4gICAgICAgIHBhcnRzLnNwbGljZShpICsgMSwgdXApO1xuICAgICAgICB1cCA9IDA7XG4gICAgICB9IGVsc2Uge1xuICAgICAgICBwYXJ0cy5zcGxpY2UoaSwgMik7XG4gICAgICAgIHVwLS07XG4gICAgICB9XG4gICAgfVxuICB9XG4gIHBhdGggPSBwYXJ0cy5qb2luKCcvJyk7XG5cbiAgaWYgKHBhdGggPT09ICcnKSB7XG4gICAgcGF0aCA9IGlzQWJzb2x1dGUgPyAnLycgOiAnLic7XG4gIH1cblxuICBpZiAodXJsKSB7XG4gICAgdXJsLnBhdGggPSBwYXRoO1xuICAgIHJldHVybiB1cmxHZW5lcmF0ZSh1cmwpO1xuICB9XG4gIHJldHVybiBwYXRoO1xufVxuZXhwb3J0cy5ub3JtYWxpemUgPSBub3JtYWxpemU7XG5cbi8qKlxuICogSm9pbnMgdHdvIHBhdGhzL1VSTHMuXG4gKlxuICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBqb2luZWQgd2l0aCB0aGUgcm9vdC5cbiAqXG4gKiAtIElmIGFQYXRoIGlzIGEgVVJMIG9yIGEgZGF0YSBVUkksIGFQYXRoIGlzIHJldHVybmVkLCB1bmxlc3MgYVBhdGggaXMgYVxuICogICBzY2hlbWUtcmVsYXRpdmUgVVJMOiBUaGVuIHRoZSBzY2hlbWUgb2YgYVJvb3QsIGlmIGFueSwgaXMgcHJlcGVuZGVkXG4gKiAgIGZpcnN0LlxuICogLSBPdGhlcndpc2UgYVBhdGggaXMgYSBwYXRoLiBJZiBhUm9vdCBpcyBhIFVSTCwgdGhlbiBpdHMgcGF0aCBwb3J0aW9uXG4gKiAgIGlzIHVwZGF0ZWQgd2l0aCB0aGUgcmVzdWx0IGFuZCBhUm9vdCBpcyByZXR1cm5lZC4gT3RoZXJ3aXNlIHRoZSByZXN1bHRcbiAqICAgaXMgcmV0dXJuZWQuXG4gKiAgIC0gSWYgYVBhdGggaXMgYWJzb2x1dGUsIHRoZSByZXN1bHQgaXMgYVBhdGguXG4gKiAgIC0gT3RoZXJ3aXNlIHRoZSB0d28gcGF0aHMgYXJlIGpvaW5lZCB3aXRoIGEgc2xhc2guXG4gKiAtIEpvaW5pbmcgZm9yIGV4YW1wbGUgJ2h0dHA6Ly8nIGFuZCAnd3d3LmV4YW1wbGUuY29tJyBpcyBhbHNvIHN1cHBvcnRlZC5cbiAqL1xuZnVuY3Rpb24gam9pbihhUm9vdCwgYVBhdGgpIHtcbiAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgYVJvb3QgPSBcIi5cIjtcbiAgfVxuICBpZiAoYVBhdGggPT09IFwiXCIpIHtcbiAgICBhUGF0aCA9IFwiLlwiO1xuICB9XG4gIHZhciBhUGF0aFVybCA9IHVybFBhcnNlKGFQYXRoKTtcbiAgdmFyIGFSb290VXJsID0gdXJsUGFyc2UoYVJvb3QpO1xuICBpZiAoYVJvb3RVcmwpIHtcbiAgICBhUm9vdCA9IGFSb290VXJsLnBhdGggfHwgJy8nO1xuICB9XG5cbiAgLy8gYGpvaW4oZm9vLCAnLy93d3cuZXhhbXBsZS5vcmcnKWBcbiAgaWYgKGFQYXRoVXJsICYmICFhUGF0aFVybC5zY2hlbWUpIHtcbiAgICBpZiAoYVJvb3RVcmwpIHtcbiAgICAgIGFQYXRoVXJsLnNjaGVtZSA9IGFSb290VXJsLnNjaGVtZTtcbiAgICB9XG4gICAgcmV0dXJuIHVybEdlbmVyYXRlKGFQYXRoVXJsKTtcbiAgfVxuXG4gIGlmIChhUGF0aFVybCB8fCBhUGF0aC5tYXRjaChkYXRhVXJsUmVnZXhwKSkge1xuICAgIHJldHVybiBhUGF0aDtcbiAgfVxuXG4gIC8vIGBqb2luKCdodHRwOi8vJywgJ3d3dy5leGFtcGxlLmNvbScpYFxuICBpZiAoYVJvb3RVcmwgJiYgIWFSb290VXJsLmhvc3QgJiYgIWFSb290VXJsLnBhdGgpIHtcbiAgICBhUm9vdFVybC5ob3N0ID0gYVBhdGg7XG4gICAgcmV0dXJuIHVybEdlbmVyYXRlKGFSb290VXJsKTtcbiAgfVxuXG4gIHZhciBqb2luZWQgPSBhUGF0aC5jaGFyQXQoMCkgPT09ICcvJ1xuICAgID8gYVBhdGhcbiAgICA6IG5vcm1hbGl6ZShhUm9vdC5yZXBsYWNlKC9cXC8rJC8sICcnKSArICcvJyArIGFQYXRoKTtcblxuICBpZiAoYVJvb3RVcmwpIHtcbiAgICBhUm9vdFVybC5wYXRoID0gam9pbmVkO1xuICAgIHJldHVybiB1cmxHZW5lcmF0ZShhUm9vdFVybCk7XG4gIH1cbiAgcmV0dXJuIGpvaW5lZDtcbn1cbmV4cG9ydHMuam9pbiA9IGpvaW47XG5cbmV4cG9ydHMuaXNBYnNvbHV0ZSA9IGZ1bmN0aW9uIChhUGF0aCkge1xuICByZXR1cm4gYVBhdGguY2hhckF0KDApID09PSAnLycgfHwgdXJsUmVnZXhwLnRlc3QoYVBhdGgpO1xufTtcblxuLyoqXG4gKiBNYWtlIGEgcGF0aCByZWxhdGl2ZSB0byBhIFVSTCBvciBhbm90aGVyIHBhdGguXG4gKlxuICogQHBhcmFtIGFSb290IFRoZSByb290IHBhdGggb3IgVVJMLlxuICogQHBhcmFtIGFQYXRoIFRoZSBwYXRoIG9yIFVSTCB0byBiZSBtYWRlIHJlbGF0aXZlIHRvIGFSb290LlxuICovXG5mdW5jdGlvbiByZWxhdGl2ZShhUm9vdCwgYVBhdGgpIHtcbiAgaWYgKGFSb290ID09PSBcIlwiKSB7XG4gICAgYVJvb3QgPSBcIi5cIjtcbiAgfVxuXG4gIGFSb290ID0gYVJvb3QucmVwbGFjZSgvXFwvJC8sICcnKTtcblxuICAvLyBJdCBpcyBwb3NzaWJsZSBmb3IgdGhlIHBhdGggdG8gYmUgYWJvdmUgdGhlIHJvb3QuIEluIHRoaXMgY2FzZSwgc2ltcGx5XG4gIC8vIGNoZWNraW5nIHdoZXRoZXIgdGhlIHJvb3QgaXMgYSBwcmVmaXggb2YgdGhlIHBhdGggd29uJ3Qgd29yay4gSW5zdGVhZCwgd2VcbiAgLy8gbmVlZCB0byByZW1vdmUgY29tcG9uZW50cyBmcm9tIHRoZSByb290IG9uZSBieSBvbmUsIHVudGlsIGVpdGhlciB3ZSBmaW5kXG4gIC8vIGEgcHJlZml4IHRoYXQgZml0cywgb3Igd2UgcnVuIG91dCBvZiBjb21wb25lbnRzIHRvIHJlbW92ZS5cbiAgdmFyIGxldmVsID0gMDtcbiAgd2hpbGUgKGFQYXRoLmluZGV4T2YoYVJvb3QgKyAnLycpICE9PSAwKSB7XG4gICAgdmFyIGluZGV4ID0gYVJvb3QubGFzdEluZGV4T2YoXCIvXCIpO1xuICAgIGlmIChpbmRleCA8IDApIHtcbiAgICAgIHJldHVybiBhUGF0aDtcbiAgICB9XG5cbiAgICAvLyBJZiB0aGUgb25seSBwYXJ0IG9mIHRoZSByb290IHRoYXQgaXMgbGVmdCBpcyB0aGUgc2NoZW1lIChpLmUuIGh0dHA6Ly8sXG4gICAgLy8gZmlsZTovLy8sIGV0Yy4pLCBvbmUgb3IgbW9yZSBzbGFzaGVzICgvKSwgb3Igc2ltcGx5IG5vdGhpbmcgYXQgYWxsLCB3ZVxuICAgIC8vIGhhdmUgZXhoYXVzdGVkIGFsbCBjb21wb25lbnRzLCBzbyB0aGUgcGF0aCBpcyBub3QgcmVsYXRpdmUgdG8gdGhlIHJvb3QuXG4gICAgYVJvb3QgPSBhUm9vdC5zbGljZSgwLCBpbmRleCk7XG4gICAgaWYgKGFSb290Lm1hdGNoKC9eKFteXFwvXSs6XFwvKT9cXC8qJC8pKSB7XG4gICAgICByZXR1cm4gYVBhdGg7XG4gICAgfVxuXG4gICAgKytsZXZlbDtcbiAgfVxuXG4gIC8vIE1ha2Ugc3VyZSB3ZSBhZGQgYSBcIi4uL1wiIGZvciBlYWNoIGNvbXBvbmVudCB3ZSByZW1vdmVkIGZyb20gdGhlIHJvb3QuXG4gIHJldHVybiBBcnJheShsZXZlbCArIDEpLmpvaW4oXCIuLi9cIikgKyBhUGF0aC5zdWJzdHIoYVJvb3QubGVuZ3RoICsgMSk7XG59XG5leHBvcnRzLnJlbGF0aXZlID0gcmVsYXRpdmU7XG5cbnZhciBzdXBwb3J0c051bGxQcm90byA9IChmdW5jdGlvbiAoKSB7XG4gIHZhciBvYmogPSBPYmplY3QuY3JlYXRlKG51bGwpO1xuICByZXR1cm4gISgnX19wcm90b19fJyBpbiBvYmopO1xufSgpKTtcblxuZnVuY3Rpb24gaWRlbnRpdHkgKHMpIHtcbiAgcmV0dXJuIHM7XG59XG5cbi8qKlxuICogQmVjYXVzZSBiZWhhdmlvciBnb2VzIHdhY2t5IHdoZW4geW91IHNldCBgX19wcm90b19fYCBvbiBvYmplY3RzLCB3ZVxuICogaGF2ZSB0byBwcmVmaXggYWxsIHRoZSBzdHJpbmdzIGluIG91ciBzZXQgd2l0aCBhbiBhcmJpdHJhcnkgY2hhcmFjdGVyLlxuICpcbiAqIFNlZSBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL3B1bGwvMzEgYW5kXG4gKiBodHRwczovL2dpdGh1Yi5jb20vbW96aWxsYS9zb3VyY2UtbWFwL2lzc3Vlcy8zMFxuICpcbiAqIEBwYXJhbSBTdHJpbmcgYVN0clxuICovXG5mdW5jdGlvbiB0b1NldFN0cmluZyhhU3RyKSB7XG4gIGlmIChpc1Byb3RvU3RyaW5nKGFTdHIpKSB7XG4gICAgcmV0dXJuICckJyArIGFTdHI7XG4gIH1cblxuICByZXR1cm4gYVN0cjtcbn1cbmV4cG9ydHMudG9TZXRTdHJpbmcgPSBzdXBwb3J0c051bGxQcm90byA/IGlkZW50aXR5IDogdG9TZXRTdHJpbmc7XG5cbmZ1bmN0aW9uIGZyb21TZXRTdHJpbmcoYVN0cikge1xuICBpZiAoaXNQcm90b1N0cmluZyhhU3RyKSkge1xuICAgIHJldHVybiBhU3RyLnNsaWNlKDEpO1xuICB9XG5cbiAgcmV0dXJuIGFTdHI7XG59XG5leHBvcnRzLmZyb21TZXRTdHJpbmcgPSBzdXBwb3J0c051bGxQcm90byA/IGlkZW50aXR5IDogZnJvbVNldFN0cmluZztcblxuZnVuY3Rpb24gaXNQcm90b1N0cmluZyhzKSB7XG4gIGlmICghcykge1xuICAgIHJldHVybiBmYWxzZTtcbiAgfVxuXG4gIHZhciBsZW5ndGggPSBzLmxlbmd0aDtcblxuICBpZiAobGVuZ3RoIDwgOSAvKiBcIl9fcHJvdG9fX1wiLmxlbmd0aCAqLykge1xuICAgIHJldHVybiBmYWxzZTtcbiAgfVxuXG4gIGlmIChzLmNoYXJDb2RlQXQobGVuZ3RoIC0gMSkgIT09IDk1ICAvKiAnXycgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSAyKSAhPT0gOTUgIC8qICdfJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDMpICE9PSAxMTEgLyogJ28nICovIHx8XG4gICAgICBzLmNoYXJDb2RlQXQobGVuZ3RoIC0gNCkgIT09IDExNiAvKiAndCcgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSA1KSAhPT0gMTExIC8qICdvJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDYpICE9PSAxMTQgLyogJ3InICovIHx8XG4gICAgICBzLmNoYXJDb2RlQXQobGVuZ3RoIC0gNykgIT09IDExMiAvKiAncCcgKi8gfHxcbiAgICAgIHMuY2hhckNvZGVBdChsZW5ndGggLSA4KSAhPT0gOTUgIC8qICdfJyAqLyB8fFxuICAgICAgcy5jaGFyQ29kZUF0KGxlbmd0aCAtIDkpICE9PSA5NSAgLyogJ18nICovKSB7XG4gICAgcmV0dXJuIGZhbHNlO1xuICB9XG5cbiAgZm9yICh2YXIgaSA9IGxlbmd0aCAtIDEwOyBpID49IDA7IGktLSkge1xuICAgIGlmIChzLmNoYXJDb2RlQXQoaSkgIT09IDM2IC8qICckJyAqLykge1xuICAgICAgcmV0dXJuIGZhbHNlO1xuICAgIH1cbiAgfVxuXG4gIHJldHVybiB0cnVlO1xufVxuXG4vKipcbiAqIENvbXBhcmF0b3IgYmV0d2VlbiB0d28gbWFwcGluZ3Mgd2hlcmUgdGhlIG9yaWdpbmFsIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKlxuICogT3B0aW9uYWxseSBwYXNzIGluIGB0cnVlYCBhcyBgb25seUNvbXBhcmVHZW5lcmF0ZWRgIHRvIGNvbnNpZGVyIHR3b1xuICogbWFwcGluZ3Mgd2l0aCB0aGUgc2FtZSBvcmlnaW5hbCBzb3VyY2UvbGluZS9jb2x1bW4sIGJ1dCBkaWZmZXJlbnQgZ2VuZXJhdGVkXG4gKiBsaW5lIGFuZCBjb2x1bW4gdGhlIHNhbWUuIFVzZWZ1bCB3aGVuIHNlYXJjaGluZyBmb3IgYSBtYXBwaW5nIHdpdGggYVxuICogc3R1YmJlZCBvdXQgbWFwcGluZy5cbiAqL1xuZnVuY3Rpb24gY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnMobWFwcGluZ0EsIG1hcHBpbmdCLCBvbmx5Q29tcGFyZU9yaWdpbmFsKSB7XG4gIHZhciBjbXAgPSBzdHJjbXAobWFwcGluZ0Euc291cmNlLCBtYXBwaW5nQi5zb3VyY2UpO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsTGluZSAtIG1hcHBpbmdCLm9yaWdpbmFsTGluZTtcbiAgaWYgKGNtcCAhPT0gMCkge1xuICAgIHJldHVybiBjbXA7XG4gIH1cblxuICBjbXAgPSBtYXBwaW5nQS5vcmlnaW5hbENvbHVtbiAtIG1hcHBpbmdCLm9yaWdpbmFsQ29sdW1uO1xuICBpZiAoY21wICE9PSAwIHx8IG9ubHlDb21wYXJlT3JpZ2luYWwpIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLmdlbmVyYXRlZExpbmUgLSBtYXBwaW5nQi5nZW5lcmF0ZWRMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIHJldHVybiBzdHJjbXAobWFwcGluZ0EubmFtZSwgbWFwcGluZ0IubmFtZSk7XG59XG5leHBvcnRzLmNvbXBhcmVCeU9yaWdpbmFsUG9zaXRpb25zID0gY29tcGFyZUJ5T3JpZ2luYWxQb3NpdGlvbnM7XG5cbi8qKlxuICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aXRoIGRlZmxhdGVkIHNvdXJjZSBhbmQgbmFtZSBpbmRpY2VzIHdoZXJlXG4gKiB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKlxuICogT3B0aW9uYWxseSBwYXNzIGluIGB0cnVlYCBhcyBgb25seUNvbXBhcmVHZW5lcmF0ZWRgIHRvIGNvbnNpZGVyIHR3b1xuICogbWFwcGluZ3Mgd2l0aCB0aGUgc2FtZSBnZW5lcmF0ZWQgbGluZSBhbmQgY29sdW1uLCBidXQgZGlmZmVyZW50XG4gKiBzb3VyY2UvbmFtZS9vcmlnaW5hbCBsaW5lIGFuZCBjb2x1bW4gdGhlIHNhbWUuIFVzZWZ1bCB3aGVuIHNlYXJjaGluZyBmb3IgYVxuICogbWFwcGluZyB3aXRoIGEgc3R1YmJlZCBvdXQgbWFwcGluZy5cbiAqL1xuZnVuY3Rpb24gY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQobWFwcGluZ0EsIG1hcHBpbmdCLCBvbmx5Q29tcGFyZUdlbmVyYXRlZCkge1xuICB2YXIgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICBpZiAoY21wICE9PSAwIHx8IG9ubHlDb21wYXJlR2VuZXJhdGVkKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IHN0cmNtcChtYXBwaW5nQS5zb3VyY2UsIG1hcHBpbmdCLnNvdXJjZSk7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgcmV0dXJuIHN0cmNtcChtYXBwaW5nQS5uYW1lLCBtYXBwaW5nQi5uYW1lKTtcbn1cbmV4cG9ydHMuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zRGVmbGF0ZWQgPSBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNEZWZsYXRlZDtcblxuZnVuY3Rpb24gc3RyY21wKGFTdHIxLCBhU3RyMikge1xuICBpZiAoYVN0cjEgPT09IGFTdHIyKSB7XG4gICAgcmV0dXJuIDA7XG4gIH1cblxuICBpZiAoYVN0cjEgPT09IG51bGwpIHtcbiAgICByZXR1cm4gMTsgLy8gYVN0cjIgIT09IG51bGxcbiAgfVxuXG4gIGlmIChhU3RyMiA9PT0gbnVsbCkge1xuICAgIHJldHVybiAtMTsgLy8gYVN0cjEgIT09IG51bGxcbiAgfVxuXG4gIGlmIChhU3RyMSA+IGFTdHIyKSB7XG4gICAgcmV0dXJuIDE7XG4gIH1cblxuICByZXR1cm4gLTE7XG59XG5cbi8qKlxuICogQ29tcGFyYXRvciBiZXR3ZWVuIHR3byBtYXBwaW5ncyB3aXRoIGluZmxhdGVkIHNvdXJjZSBhbmQgbmFtZSBzdHJpbmdzIHdoZXJlXG4gKiB0aGUgZ2VuZXJhdGVkIHBvc2l0aW9ucyBhcmUgY29tcGFyZWQuXG4gKi9cbmZ1bmN0aW9uIGNvbXBhcmVCeUdlbmVyYXRlZFBvc2l0aW9uc0luZmxhdGVkKG1hcHBpbmdBLCBtYXBwaW5nQikge1xuICB2YXIgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkTGluZSAtIG1hcHBpbmdCLmdlbmVyYXRlZExpbmU7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0EuZ2VuZXJhdGVkQ29sdW1uIC0gbWFwcGluZ0IuZ2VuZXJhdGVkQ29sdW1uO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IHN0cmNtcChtYXBwaW5nQS5zb3VyY2UsIG1hcHBpbmdCLnNvdXJjZSk7XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgY21wID0gbWFwcGluZ0Eub3JpZ2luYWxMaW5lIC0gbWFwcGluZ0Iub3JpZ2luYWxMaW5lO1xuICBpZiAoY21wICE9PSAwKSB7XG4gICAgcmV0dXJuIGNtcDtcbiAgfVxuXG4gIGNtcCA9IG1hcHBpbmdBLm9yaWdpbmFsQ29sdW1uIC0gbWFwcGluZ0Iub3JpZ2luYWxDb2x1bW47XG4gIGlmIChjbXAgIT09IDApIHtcbiAgICByZXR1cm4gY21wO1xuICB9XG5cbiAgcmV0dXJuIHN0cmNtcChtYXBwaW5nQS5uYW1lLCBtYXBwaW5nQi5uYW1lKTtcbn1cbmV4cG9ydHMuY29tcGFyZUJ5R2VuZXJhdGVkUG9zaXRpb25zSW5mbGF0ZWQgPSBjb21wYXJlQnlHZW5lcmF0ZWRQb3NpdGlvbnNJbmZsYXRlZDtcblxuLyoqXG4gKiBTdHJpcCBhbnkgSlNPTiBYU1NJIGF2b2lkYW5jZSBwcmVmaXggZnJvbSB0aGUgc3RyaW5nIChhcyBkb2N1bWVudGVkXG4gKiBpbiB0aGUgc291cmNlIG1hcHMgc3BlY2lmaWNhdGlvbiksIGFuZCB0aGVuIHBhcnNlIHRoZSBzdHJpbmcgYXNcbiAqIEpTT04uXG4gKi9cbmZ1bmN0aW9uIHBhcnNlU291cmNlTWFwSW5wdXQoc3RyKSB7XG4gIHJldHVybiBKU09OLnBhcnNlKHN0ci5yZXBsYWNlKC9eXFwpXX0nW15cXG5dKlxcbi8sICcnKSk7XG59XG5leHBvcnRzLnBhcnNlU291cmNlTWFwSW5wdXQgPSBwYXJzZVNvdXJjZU1hcElucHV0O1xuXG4vKipcbiAqIENvbXB1dGUgdGhlIFVSTCBvZiBhIHNvdXJjZSBnaXZlbiB0aGUgdGhlIHNvdXJjZSByb290LCB0aGUgc291cmNlJ3NcbiAqIFVSTCwgYW5kIHRoZSBzb3VyY2UgbWFwJ3MgVVJMLlxuICovXG5mdW5jdGlvbiBjb21wdXRlU291cmNlVVJMKHNvdXJjZVJvb3QsIHNvdXJjZVVSTCwgc291cmNlTWFwVVJMKSB7XG4gIHNvdXJjZVVSTCA9IHNvdXJjZVVSTCB8fCAnJztcblxuICBpZiAoc291cmNlUm9vdCkge1xuICAgIC8vIFRoaXMgZm9sbG93cyB3aGF0IENocm9tZSBkb2VzLlxuICAgIGlmIChzb3VyY2VSb290W3NvdXJjZVJvb3QubGVuZ3RoIC0gMV0gIT09ICcvJyAmJiBzb3VyY2VVUkxbMF0gIT09ICcvJykge1xuICAgICAgc291cmNlUm9vdCArPSAnLyc7XG4gICAgfVxuICAgIC8vIFRoZSBzcGVjIHNheXM6XG4gICAgLy8gICBMaW5lIDQ6IEFuIG9wdGlvbmFsIHNvdXJjZSByb290LCB1c2VmdWwgZm9yIHJlbG9jYXRpbmcgc291cmNlXG4gICAgLy8gICBmaWxlcyBvbiBhIHNlcnZlciBvciByZW1vdmluZyByZXBlYXRlZCB2YWx1ZXMgaW4gdGhlXG4gICAgLy8gICDigJxzb3VyY2Vz4oCdIGVudHJ5LiAgVGhpcyB2YWx1ZSBpcyBwcmVwZW5kZWQgdG8gdGhlIGluZGl2aWR1YWxcbiAgICAvLyAgIGVudHJpZXMgaW4gdGhlIOKAnHNvdXJjZeKAnSBmaWVsZC5cbiAgICBzb3VyY2VVUkwgPSBzb3VyY2VSb290ICsgc291cmNlVVJMO1xuICB9XG5cbiAgLy8gSGlzdG9yaWNhbGx5LCBTb3VyY2VNYXBDb25zdW1lciBkaWQgbm90IHRha2UgdGhlIHNvdXJjZU1hcFVSTCBhc1xuICAvLyBhIHBhcmFtZXRlci4gIFRoaXMgbW9kZSBpcyBzdGlsbCBzb21ld2hhdCBzdXBwb3J0ZWQsIHdoaWNoIGlzIHdoeVxuICAvLyB0aGlzIGNvZGUgYmxvY2sgaXMgY29uZGl0aW9uYWwuICBIb3dldmVyLCBpdCdzIHByZWZlcmFibGUgdG8gcGFzc1xuICAvLyB0aGUgc291cmNlIG1hcCBVUkwgdG8gU291cmNlTWFwQ29uc3VtZXIsIHNvIHRoYXQgdGhpcyBmdW5jdGlvblxuICAvLyBjYW4gaW1wbGVtZW50IHRoZSBzb3VyY2UgVVJMIHJlc29sdXRpb24gYWxnb3JpdGhtIGFzIG91dGxpbmVkIGluXG4gIC8vIHRoZSBzcGVjLiAgVGhpcyBibG9jayBpcyBiYXNpY2FsbHkgdGhlIGVxdWl2YWxlbnQgb2Y6XG4gIC8vICAgIG5ldyBVUkwoc291cmNlVVJMLCBzb3VyY2VNYXBVUkwpLnRvU3RyaW5nKClcbiAgLy8gLi4uIGV4Y2VwdCBpdCBhdm9pZHMgdXNpbmcgVVJMLCB3aGljaCB3YXNuJ3QgYXZhaWxhYmxlIGluIHRoZVxuICAvLyBvbGRlciByZWxlYXNlcyBvZiBub2RlIHN0aWxsIHN1cHBvcnRlZCBieSB0aGlzIGxpYnJhcnkuXG4gIC8vXG4gIC8vIFRoZSBzcGVjIHNheXM6XG4gIC8vICAgSWYgdGhlIHNvdXJjZXMgYXJlIG5vdCBhYnNvbHV0ZSBVUkxzIGFmdGVyIHByZXBlbmRpbmcgb2YgdGhlXG4gIC8vICAg4oCcc291cmNlUm9vdOKAnSwgdGhlIHNvdXJjZXMgYXJlIHJlc29sdmVkIHJlbGF0aXZlIHRvIHRoZVxuICAvLyAgIFNvdXJjZU1hcCAobGlrZSByZXNvbHZpbmcgc2NyaXB0IHNyYyBpbiBhIGh0bWwgZG9jdW1lbnQpLlxuICBpZiAoc291cmNlTWFwVVJMKSB7XG4gICAgdmFyIHBhcnNlZCA9IHVybFBhcnNlKHNvdXJjZU1hcFVSTCk7XG4gICAgaWYgKCFwYXJzZWQpIHtcbiAgICAgIHRocm93IG5ldyBFcnJvcihcInNvdXJjZU1hcFVSTCBjb3VsZCBub3QgYmUgcGFyc2VkXCIpO1xuICAgIH1cbiAgICBpZiAocGFyc2VkLnBhdGgpIHtcbiAgICAgIC8vIFN0cmlwIHRoZSBsYXN0IHBhdGggY29tcG9uZW50LCBidXQga2VlcCB0aGUgXCIvXCIuXG4gICAgICB2YXIgaW5kZXggPSBwYXJzZWQucGF0aC5sYXN0SW5kZXhPZignLycpO1xuICAgICAgaWYgKGluZGV4ID49IDApIHtcbiAgICAgICAgcGFyc2VkLnBhdGggPSBwYXJzZWQucGF0aC5zdWJzdHJpbmcoMCwgaW5kZXggKyAxKTtcbiAgICAgIH1cbiAgICB9XG4gICAgc291cmNlVVJMID0gam9pbih1cmxHZW5lcmF0ZShwYXJzZWQpLCBzb3VyY2VVUkwpO1xuICB9XG5cbiAgcmV0dXJuIG5vcm1hbGl6ZShzb3VyY2VVUkwpO1xufVxuZXhwb3J0cy5jb21wdXRlU291cmNlVVJMID0gY29tcHV0ZVNvdXJjZVVSTDtcblxuXG5cbi8vLy8vLy8vLy8vLy8vLy8vL1xuLy8gV0VCUEFDSyBGT09URVJcbi8vIC4vbGliL3V0aWwuanNcbi8vIG1vZHVsZSBpZCA9IDFcbi8vIG1vZHVsZSBjaHVua3MgPSAwIl0sInNvdXJjZVJvb3QiOiIifQ==