/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define([], factory);
	else {
		var a = factory();
		for(var i in a) (typeof exports === 'object' ? exports : root)[i] = a[i];
	}
})(typeof self !== 'undefined' ? self : this, function() {
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
/******/ 			Object.defineProperty(exports, name, {
/******/ 				configurable: false,
/******/ 				enumerable: true,
/******/ 				get: getter
/******/ 			});
/******/ 		}
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
/******/ 	__webpack_require__.p = "/assets/build";
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 904);
/******/ })
/************************************************************************/
/******/ ({

/***/ 1059:
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */


function WorkerDispatcher() {
  this.msgId = 1;
  this.worker = null; // Map of message ids -> promise resolution functions, for dispatching worker responses

  this.pendingCalls = new Map();
  this._onMessage = this._onMessage.bind(this);
}

WorkerDispatcher.prototype = {
  start(url, win = window) {
    this.worker = new win.Worker(url);

    this.worker.onerror = err => {
      console.error(`Error in worker ${url}`, err.message);
    };

    this.worker.addEventListener("message", this._onMessage);
  },

  stop() {
    if (!this.worker) {
      return;
    }

    this.worker.removeEventListener("message", this._onMessage);
    this.worker.terminate();
    this.worker = null;
    this.pendingCalls.clear();
  },

  task(method, {
    queue = false
  } = {}) {
    const calls = [];

    const push = args => {
      return new Promise((resolve, reject) => {
        if (queue && calls.length === 0) {
          Promise.resolve().then(flush);
        }

        calls.push({
          args,
          resolve,
          reject
        });

        if (!queue) {
          flush();
        }
      });
    };

    const flush = () => {
      const items = calls.slice();
      calls.length = 0;

      if (!this.worker) {
        return;
      }

      const id = this.msgId++;
      this.worker.postMessage({
        id,
        method,
        calls: items.map(item => item.args)
      });
      this.pendingCalls.set(id, items);
    };

    return (...args) => push(args);
  },

  invoke(method, ...args) {
    return this.task(method)(...args);
  },

  _onMessage({
    data: result
  }) {
    const items = this.pendingCalls.get(result.id);
    this.pendingCalls.delete(result.id);

    if (!items) {
      return;
    }

    if (!this.worker) {
      return;
    }

    result.results.forEach((resultData, i) => {
      const {
        resolve,
        reject
      } = items[i];

      if (resultData.error) {
        const err = new Error(resultData.message);
        err.metadata = resultData.metadata;
        reject(err);
      } else {
        resolve(resultData.response);
      }
    });
  }

};

function workerHandler(publicInterface) {
  return function (msg) {
    const {
      id,
      method,
      calls
    } = msg.data;
    Promise.all(calls.map(args => {
      try {
        const response = publicInterface[method].apply(undefined, args);

        if (response instanceof Promise) {
          return response.then(val => ({
            response: val
          }), err => asErrorMessage(err));
        }

        return {
          response
        };
      } catch (error) {
        return asErrorMessage(error);
      }
    })).then(results => {
      globalThis.postMessage({
        id,
        results
      });
    });
  };
}

function asErrorMessage(error) {
  if (typeof error === "object" && error && "message" in error) {
    // Error can't be sent via postMessage, so be sure to convert to
    // string.
    return {
      error: true,
      message: error.message,
      metadata: error.metadata
    };
  }

  return {
    error: true,
    message: error == null ? error : error.toString(),
    metadata: undefined
  };
} // Might be loaded within a worker thread where `module` isn't available.


if (true) {
  module.exports = {
    WorkerDispatcher,
    workerHandler
  };
}

/***/ }),

/***/ 607:
/***/ (function(module, exports) {

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


/***/ }),

/***/ 701:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = getMatches;

var _assert = _interopRequireDefault(__webpack_require__(906));

var _buildQuery = _interopRequireDefault(__webpack_require__(907));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getMatches(query, text, modifiers) {
  if (!query || !text || !modifiers) {
    return [];
  }

  const regexQuery = (0, _buildQuery.default)(query, modifiers, {
    isGlobal: true
  });
  const matchedLocations = [];
  const lines = text.split("\n");

  for (let i = 0; i < lines.length; i++) {
    let singleMatch;
    const line = lines[i];

    while ((singleMatch = regexQuery.exec(line)) !== null) {
      // Flow doesn't understand the test above.
      if (!singleMatch) {
        throw new Error("no singleMatch");
      }

      matchedLocations.push({
        line: i,
        ch: singleMatch.index
      }); // When the match is an empty string the regexQuery.lastIndex will not
      // change resulting in an infinite loop so we need to check for this and
      // increment it manually in that case.  See issue #7023

      if (singleMatch[0] === "") {
        (0, _assert.default)(!regexQuery.unicode, "lastIndex++ can cause issues in unicode mode");
        regexQuery.lastIndex++;
      }
    }
  }

  return matchedLocations;
}

/***/ }),

/***/ 904:
/***/ (function(module, exports, __webpack_require__) {

module.exports = __webpack_require__(905);


/***/ }),

/***/ 905:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


var _getMatches = _interopRequireDefault(__webpack_require__(701));

var _projectSearch = __webpack_require__(909);

var _workerUtils = __webpack_require__(1059);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
self.onmessage = (0, _workerUtils.workerHandler)({
  getMatches: _getMatches.default,
  findSourceMatches: _projectSearch.findSourceMatches
});

/***/ }),

/***/ 906:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = assert;

var _environment = __webpack_require__(968);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function assert(condition, message) {
  if ((0, _environment.isNodeTest)() && !condition) {
    throw new Error(`Assertion failure: ${message}`);
  }
}

/***/ }),

/***/ 907:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = buildQuery;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function escapeRegExp(str) {
  const reRegExpChar = /[\\^$.*+?()[\]{}|]/g;
  return str.replace(reRegExpChar, "\\$&");
}
/**
 * Ignore doing outline matches for less than 3 whitespaces
 *
 * @memberof utils/source-search
 * @static
 */


function ignoreWhiteSpace(str) {
  return /^\s{0,2}$/.test(str) ? "(?!\\s*.*)" : str;
}

function wholeMatch(query, wholeWord) {
  if (query === "" || !wholeWord) {
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

  return null;
}

function buildQuery(originalQuery, modifiers, {
  isGlobal = false,
  ignoreSpaces = false
}) {
  const {
    caseSensitive,
    regexMatch,
    wholeWord
  } = modifiers;

  if (originalQuery === "") {
    return new RegExp(originalQuery);
  }

  let query = originalQuery; // If we don't want to do a regexMatch, we need to escape all regex related characters
  // so they would actually match.

  if (!regexMatch) {
    query = escapeRegExp(query);
  } // ignoreWhiteSpace might return a negative lookbehind, and in such case, we want it
  // to be consumed as a RegExp part by the callsite, so this needs to be called after
  // the regexp is escaped.


  if (ignoreSpaces) {
    query = ignoreWhiteSpace(query);
  }

  query = wholeMatch(query, wholeWord);
  const flags = buildFlags(caseSensitive, isGlobal);

  if (flags) {
    return new RegExp(query, flags);
  }

  return new RegExp(query);
}

/***/ }),

/***/ 909:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.findSourceMatches = findSourceMatches;

var _getMatches = _interopRequireDefault(__webpack_require__(701));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// Maybe reuse file search's functions?
function findSourceMatches(sourceId, content, queryText) {
  if (queryText == "") {
    return [];
  }

  const modifiers = {
    caseSensitive: false,
    regexMatch: false,
    wholeWord: false
  };
  const text = content.value;
  const lines = text.split("\n");
  return (0, _getMatches.default)(queryText, text, modifiers).map(({
    line,
    ch
  }) => {
    const {
      value,
      matchIndex
    } = truncateLine(lines[line], ch);
    return {
      sourceId,
      line: line + 1,
      column: ch,
      matchIndex,
      match: queryText,
      value
    };
  });
} // This is used to find start of a word, so that cropped string look nice


const startRegex = /([ !@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?])/g; // Similarly, find

const endRegex = new RegExp(["([ !@#$%^&*()_+-=[]{};':\"\\|,.<>/?])", '[^ !@#$%^&*()_+-=[]{};\':"\\|,.<>/?]*$"/'].join(""));

function truncateLine(text, column) {
  if (text.length < 100) {
    return {
      matchIndex: column,
      value: text
    };
  } // Initially take 40 chars left to the match


  const offset = Math.max(column - 40, 0); // 400 characters should be enough to figure out the context of the match

  const truncStr = text.slice(offset, column + 400);
  let start = truncStr.search(startRegex);
  let end = truncStr.search(endRegex);

  if (start > column) {
    // No word separator found before the match, so we take all characters
    // before the match
    start = -1;
  }

  if (end < column) {
    end = truncStr.length;
  }

  const value = truncStr.slice(start + 1, end);
  return {
    matchIndex: column - start - offset - 1,
    value
  };
}

/***/ }),

/***/ 968:
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* WEBPACK VAR INJECTION */(function(process) {

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isNode = isNode;
exports.isNodeTest = isNodeTest;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function isNode() {
  try {
    return process.release.name == "node";
  } catch (e) {
    return false;
  }
}

function isNodeTest() {
  return isNode() && "production" != "production";
}
/* WEBPACK VAR INJECTION */}.call(exports, __webpack_require__(607)))

/***/ })

/******/ });
});