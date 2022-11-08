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
/******/ 	return __webpack_require__(__webpack_require__.s = 928);
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

/***/ 1084:
/***/ (function(module, exports, __webpack_require__) {

!function (t, n) {
   true ? module.exports = n() : "function" == typeof define && define.amd ? define([], n) : "object" == typeof exports ? exports.MD5 = n() : t.MD5 = n();
}(this, function () {
  return function (t) {
    function n(o) {
      if (r[o]) return r[o].exports;
      var e = r[o] = {
        i: o,
        l: !1,
        exports: {}
      };
      return t[o].call(e.exports, e, e.exports, n), e.l = !0, e.exports;
    }

    var r = {};
    return n.m = t, n.c = r, n.i = function (t) {
      return t;
    }, n.d = function (t, r, o) {
      n.o(t, r) || Object.defineProperty(t, r, {
        configurable: !1,
        enumerable: !0,
        get: o
      });
    }, n.n = function (t) {
      var r = t && t.__esModule ? function () {
        return t.default;
      } : function () {
        return t;
      };
      return n.d(r, "a", r), r;
    }, n.o = function (t, n) {
      return Object.prototype.hasOwnProperty.call(t, n);
    }, n.p = "", n(n.s = 4);
  }([function (t, n) {
    var r = {
      utf8: {
        stringToBytes: function (t) {
          return r.bin.stringToBytes(unescape(encodeURIComponent(t)));
        },
        bytesToString: function (t) {
          return decodeURIComponent(escape(r.bin.bytesToString(t)));
        }
      },
      bin: {
        stringToBytes: function (t) {
          for (var n = [], r = 0; r < t.length; r++) n.push(255 & t.charCodeAt(r));

          return n;
        },
        bytesToString: function (t) {
          for (var n = [], r = 0; r < t.length; r++) n.push(String.fromCharCode(t[r]));

          return n.join("");
        }
      }
    };
    t.exports = r;
  }, function (t, n, r) {
    !function () {
      var n = r(2),
          o = r(0).utf8,
          e = r(3),
          u = r(0).bin,
          i = function (t, r) {
        t.constructor == String ? t = r && "binary" === r.encoding ? u.stringToBytes(t) : o.stringToBytes(t) : e(t) ? t = Array.prototype.slice.call(t, 0) : Array.isArray(t) || t.constructor === Uint8Array || (t = t.toString());

        for (var f = n.bytesToWords(t), s = 8 * t.length, c = 1732584193, a = -271733879, p = -1732584194, l = 271733878, g = 0; g < f.length; g++) f[g] = 16711935 & (f[g] << 8 | f[g] >>> 24) | 4278255360 & (f[g] << 24 | f[g] >>> 8);

        f[s >>> 5] |= 128 << s % 32, f[14 + (s + 64 >>> 9 << 4)] = s;

        for (var h = i._ff, y = i._gg, d = i._hh, v = i._ii, g = 0; g < f.length; g += 16) {
          var b = c,
              x = a,
              T = p,
              B = l;
          c = h(c, a, p, l, f[g + 0], 7, -680876936), l = h(l, c, a, p, f[g + 1], 12, -389564586), p = h(p, l, c, a, f[g + 2], 17, 606105819), a = h(a, p, l, c, f[g + 3], 22, -1044525330), c = h(c, a, p, l, f[g + 4], 7, -176418897), l = h(l, c, a, p, f[g + 5], 12, 1200080426), p = h(p, l, c, a, f[g + 6], 17, -1473231341), a = h(a, p, l, c, f[g + 7], 22, -45705983), c = h(c, a, p, l, f[g + 8], 7, 1770035416), l = h(l, c, a, p, f[g + 9], 12, -1958414417), p = h(p, l, c, a, f[g + 10], 17, -42063), a = h(a, p, l, c, f[g + 11], 22, -1990404162), c = h(c, a, p, l, f[g + 12], 7, 1804603682), l = h(l, c, a, p, f[g + 13], 12, -40341101), p = h(p, l, c, a, f[g + 14], 17, -1502002290), a = h(a, p, l, c, f[g + 15], 22, 1236535329), c = y(c, a, p, l, f[g + 1], 5, -165796510), l = y(l, c, a, p, f[g + 6], 9, -1069501632), p = y(p, l, c, a, f[g + 11], 14, 643717713), a = y(a, p, l, c, f[g + 0], 20, -373897302), c = y(c, a, p, l, f[g + 5], 5, -701558691), l = y(l, c, a, p, f[g + 10], 9, 38016083), p = y(p, l, c, a, f[g + 15], 14, -660478335), a = y(a, p, l, c, f[g + 4], 20, -405537848), c = y(c, a, p, l, f[g + 9], 5, 568446438), l = y(l, c, a, p, f[g + 14], 9, -1019803690), p = y(p, l, c, a, f[g + 3], 14, -187363961), a = y(a, p, l, c, f[g + 8], 20, 1163531501), c = y(c, a, p, l, f[g + 13], 5, -1444681467), l = y(l, c, a, p, f[g + 2], 9, -51403784), p = y(p, l, c, a, f[g + 7], 14, 1735328473), a = y(a, p, l, c, f[g + 12], 20, -1926607734), c = d(c, a, p, l, f[g + 5], 4, -378558), l = d(l, c, a, p, f[g + 8], 11, -2022574463), p = d(p, l, c, a, f[g + 11], 16, 1839030562), a = d(a, p, l, c, f[g + 14], 23, -35309556), c = d(c, a, p, l, f[g + 1], 4, -1530992060), l = d(l, c, a, p, f[g + 4], 11, 1272893353), p = d(p, l, c, a, f[g + 7], 16, -155497632), a = d(a, p, l, c, f[g + 10], 23, -1094730640), c = d(c, a, p, l, f[g + 13], 4, 681279174), l = d(l, c, a, p, f[g + 0], 11, -358537222), p = d(p, l, c, a, f[g + 3], 16, -722521979), a = d(a, p, l, c, f[g + 6], 23, 76029189), c = d(c, a, p, l, f[g + 9], 4, -640364487), l = d(l, c, a, p, f[g + 12], 11, -421815835), p = d(p, l, c, a, f[g + 15], 16, 530742520), a = d(a, p, l, c, f[g + 2], 23, -995338651), c = v(c, a, p, l, f[g + 0], 6, -198630844), l = v(l, c, a, p, f[g + 7], 10, 1126891415), p = v(p, l, c, a, f[g + 14], 15, -1416354905), a = v(a, p, l, c, f[g + 5], 21, -57434055), c = v(c, a, p, l, f[g + 12], 6, 1700485571), l = v(l, c, a, p, f[g + 3], 10, -1894986606), p = v(p, l, c, a, f[g + 10], 15, -1051523), a = v(a, p, l, c, f[g + 1], 21, -2054922799), c = v(c, a, p, l, f[g + 8], 6, 1873313359), l = v(l, c, a, p, f[g + 15], 10, -30611744), p = v(p, l, c, a, f[g + 6], 15, -1560198380), a = v(a, p, l, c, f[g + 13], 21, 1309151649), c = v(c, a, p, l, f[g + 4], 6, -145523070), l = v(l, c, a, p, f[g + 11], 10, -1120210379), p = v(p, l, c, a, f[g + 2], 15, 718787259), a = v(a, p, l, c, f[g + 9], 21, -343485551), c = c + b >>> 0, a = a + x >>> 0, p = p + T >>> 0, l = l + B >>> 0;
        }

        return n.endian([c, a, p, l]);
      };

      i._ff = function (t, n, r, o, e, u, i) {
        var f = t + (n & r | ~n & o) + (e >>> 0) + i;
        return (f << u | f >>> 32 - u) + n;
      }, i._gg = function (t, n, r, o, e, u, i) {
        var f = t + (n & o | r & ~o) + (e >>> 0) + i;
        return (f << u | f >>> 32 - u) + n;
      }, i._hh = function (t, n, r, o, e, u, i) {
        var f = t + (n ^ r ^ o) + (e >>> 0) + i;
        return (f << u | f >>> 32 - u) + n;
      }, i._ii = function (t, n, r, o, e, u, i) {
        var f = t + (r ^ (n | ~o)) + (e >>> 0) + i;
        return (f << u | f >>> 32 - u) + n;
      }, i._blocksize = 16, i._digestsize = 16, t.exports = function (t, r) {
        if (void 0 === t || null === t) throw new Error("Illegal argument " + t);
        var o = n.wordsToBytes(i(t, r));
        return r && r.asBytes ? o : r && r.asString ? u.bytesToString(o) : n.bytesToHex(o);
      };
    }();
  }, function (t, n) {
    !function () {
      var n = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
          r = {
        rotl: function (t, n) {
          return t << n | t >>> 32 - n;
        },
        rotr: function (t, n) {
          return t << 32 - n | t >>> n;
        },
        endian: function (t) {
          if (t.constructor == Number) return 16711935 & r.rotl(t, 8) | 4278255360 & r.rotl(t, 24);

          for (var n = 0; n < t.length; n++) t[n] = r.endian(t[n]);

          return t;
        },
        randomBytes: function (t) {
          for (var n = []; t > 0; t--) n.push(Math.floor(256 * Math.random()));

          return n;
        },
        bytesToWords: function (t) {
          for (var n = [], r = 0, o = 0; r < t.length; r++, o += 8) n[o >>> 5] |= t[r] << 24 - o % 32;

          return n;
        },
        wordsToBytes: function (t) {
          for (var n = [], r = 0; r < 32 * t.length; r += 8) n.push(t[r >>> 5] >>> 24 - r % 32 & 255);

          return n;
        },
        bytesToHex: function (t) {
          for (var n = [], r = 0; r < t.length; r++) n.push((t[r] >>> 4).toString(16)), n.push((15 & t[r]).toString(16));

          return n.join("");
        },
        hexToBytes: function (t) {
          for (var n = [], r = 0; r < t.length; r += 2) n.push(parseInt(t.substr(r, 2), 16));

          return n;
        },
        bytesToBase64: function (t) {
          for (var r = [], o = 0; o < t.length; o += 3) for (var e = t[o] << 16 | t[o + 1] << 8 | t[o + 2], u = 0; u < 4; u++) 8 * o + 6 * u <= 8 * t.length ? r.push(n.charAt(e >>> 6 * (3 - u) & 63)) : r.push("=");

          return r.join("");
        },
        base64ToBytes: function (t) {
          t = t.replace(/[^A-Z0-9+\/]/gi, "");

          for (var r = [], o = 0, e = 0; o < t.length; e = ++o % 4) 0 != e && r.push((n.indexOf(t.charAt(o - 1)) & Math.pow(2, -2 * e + 8) - 1) << 2 * e | n.indexOf(t.charAt(o)) >>> 6 - 2 * e);

          return r;
        }
      };
      t.exports = r;
    }();
  }, function (t, n) {
    function r(t) {
      return !!t.constructor && "function" == typeof t.constructor.isBuffer && t.constructor.isBuffer(t);
    }

    function o(t) {
      return "function" == typeof t.readFloatLE && "function" == typeof t.slice && r(t.slice(0, 0));
    }
    /*!
    * Determine if an object is a Buffer
    *
    * @author   Feross Aboukhadijeh <https://feross.org>
    * @license  MIT
    */


    t.exports = function (t) {
      return null != t && (r(t) || o(t) || !!t._isBuffer);
    };
  }, function (t, n, r) {
    t.exports = r(1);
  }]);
});

/***/ }),

/***/ 584:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const md5 = __webpack_require__(1084);

function originalToGeneratedId(sourceId) {
  if (isGeneratedId(sourceId)) {
    return sourceId;
  }

  const lastIndex = sourceId.lastIndexOf("/originalSource");
  return lastIndex !== -1 ? sourceId.slice(0, lastIndex) : "";
}

const getMd5 = memoize(url => md5(url));

function generatedToOriginalId(generatedId, url) {
  return `${generatedId}/originalSource-${getMd5(url)}`;
}

function isOriginalId(id) {
  return id.includes("/originalSource");
}

function isGeneratedId(id) {
  return !isOriginalId(id);
}
/**
 * Trims the query part or reference identifier of a URL string, if necessary.
 */


function trimUrlQuery(url) {
  const length = url.length;

  for (let i = 0; i < length; ++i) {
    if (url[i] === "?" || url[i] === "&" || url[i] === "#") {
      return url.slice(0, i);
    }
  }

  return url;
} // Map suffix to content type.


const contentMap = {
  js: "text/javascript",
  jsm: "text/javascript",
  mjs: "text/javascript",
  ts: "text/typescript",
  tsx: "text/typescript-jsx",
  jsx: "text/jsx",
  vue: "text/vue",
  coffee: "text/coffeescript",
  elm: "text/elm",
  cljc: "text/x-clojure",
  cljs: "text/x-clojurescript"
};
/**
 * Returns the content type for the specified URL.  If no specific
 * content type can be determined, "text/plain" is returned.
 *
 * @return String
 *         The content type.
 */

function getContentType(url) {
  url = trimUrlQuery(url);
  const dot = url.lastIndexOf(".");

  if (dot >= 0) {
    const name = url.substring(dot + 1);

    if (name in contentMap) {
      return contentMap[name];
    }
  }

  return "text/plain";
}

function memoize(func) {
  const map = new Map();
  return arg => {
    if (map.has(arg)) {
      return map.get(arg);
    }

    const result = func(arg);
    map.set(arg, result);
    return result;
  };
}

module.exports = {
  originalToGeneratedId,
  generatedToOriginalId,
  isOriginalId,
  isGeneratedId,
  getContentType,
  contentMapForTesting: contentMap
};

/***/ }),

/***/ 708:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


Object.defineProperty(exports, "__esModule", {
  value: true
});
Object.defineProperty(exports, "originalToGeneratedId", {
  enumerable: true,
  get: function () {
    return _utils.originalToGeneratedId;
  }
});
Object.defineProperty(exports, "generatedToOriginalId", {
  enumerable: true,
  get: function () {
    return _utils.generatedToOriginalId;
  }
});
Object.defineProperty(exports, "isGeneratedId", {
  enumerable: true,
  get: function () {
    return _utils.isGeneratedId;
  }
});
Object.defineProperty(exports, "isOriginalId", {
  enumerable: true,
  get: function () {
    return _utils.isOriginalId;
  }
});
exports.default = exports.stopSourceMapWorker = exports.startSourceMapWorker = exports.getOriginalStackFrames = exports.clearSourceMaps = exports.applySourceMap = exports.getOriginalSourceText = exports.getFileGeneratedRange = exports.getGeneratedRangesForOriginal = exports.getOriginalLocations = exports.getOriginalLocation = exports.getAllGeneratedLocations = exports.getGeneratedLocation = exports.getGeneratedRanges = exports.getOriginalRanges = exports.hasOriginalURL = exports.getOriginalURLs = exports.setAssetRootURL = exports.dispatcher = void 0;

var _utils = __webpack_require__(584);

var self = _interopRequireWildcard(__webpack_require__(708));

function _getRequireWildcardCache(nodeInterop) { if (typeof WeakMap !== "function") return null; var cacheBabelInterop = new WeakMap(); var cacheNodeInterop = new WeakMap(); return (_getRequireWildcardCache = function (nodeInterop) { return nodeInterop ? cacheNodeInterop : cacheBabelInterop; })(nodeInterop); }

function _interopRequireWildcard(obj, nodeInterop) { if (!nodeInterop && obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(nodeInterop); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (key !== "default" && Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  WorkerDispatcher
} = __webpack_require__(1059);

const dispatcher = new WorkerDispatcher();
exports.dispatcher = dispatcher;

const _getGeneratedRanges = dispatcher.task("getGeneratedRanges", {
  queue: true
});

const _getGeneratedLocation = dispatcher.task("getGeneratedLocation", {
  queue: true
});

const _getAllGeneratedLocations = dispatcher.task("getAllGeneratedLocations", {
  queue: true
});

const _getOriginalLocation = dispatcher.task("getOriginalLocation", {
  queue: true
});

const setAssetRootURL = async assetRoot => dispatcher.invoke("setAssetRootURL", assetRoot);

exports.setAssetRootURL = setAssetRootURL;

const getOriginalURLs = async generatedSource => dispatcher.invoke("getOriginalURLs", generatedSource);

exports.getOriginalURLs = getOriginalURLs;

const hasOriginalURL = async url => dispatcher.invoke("hasOriginalURL", url);

exports.hasOriginalURL = hasOriginalURL;

const getOriginalRanges = async sourceId => dispatcher.invoke("getOriginalRanges", sourceId);

exports.getOriginalRanges = getOriginalRanges;

const getGeneratedRanges = async location => _getGeneratedRanges(location);

exports.getGeneratedRanges = getGeneratedRanges;

const getGeneratedLocation = async location => _getGeneratedLocation(location);

exports.getGeneratedLocation = getGeneratedLocation;

const getAllGeneratedLocations = async location => _getAllGeneratedLocations(location);

exports.getAllGeneratedLocations = getAllGeneratedLocations;

const getOriginalLocation = async (location, options = {}) => _getOriginalLocation(location, options);

exports.getOriginalLocation = getOriginalLocation;

const getOriginalLocations = async (locations, options = {}) => dispatcher.invoke("getOriginalLocations", locations, options);

exports.getOriginalLocations = getOriginalLocations;

const getGeneratedRangesForOriginal = async (sourceId, mergeUnmappedRegions) => dispatcher.invoke("getGeneratedRangesForOriginal", sourceId, mergeUnmappedRegions);

exports.getGeneratedRangesForOriginal = getGeneratedRangesForOriginal;

const getFileGeneratedRange = async originalSourceId => dispatcher.invoke("getFileGeneratedRange", originalSourceId);

exports.getFileGeneratedRange = getFileGeneratedRange;

const getOriginalSourceText = async originalSourceId => dispatcher.invoke("getOriginalSourceText", originalSourceId);

exports.getOriginalSourceText = getOriginalSourceText;

const applySourceMap = async (generatedId, url, code, mappings) => dispatcher.invoke("applySourceMap", generatedId, url, code, mappings);

exports.applySourceMap = applySourceMap;

const clearSourceMaps = async () => dispatcher.invoke("clearSourceMaps");

exports.clearSourceMaps = clearSourceMaps;

const getOriginalStackFrames = async generatedLocation => dispatcher.invoke("getOriginalStackFrames", generatedLocation);

exports.getOriginalStackFrames = getOriginalStackFrames;

const startSourceMapWorker = (url, assetRoot) => {
  dispatcher.start(url);
  setAssetRootURL(assetRoot);
};

exports.startSourceMapWorker = startSourceMapWorker;
const stopSourceMapWorker = dispatcher.stop.bind(dispatcher);
exports.stopSourceMapWorker = stopSourceMapWorker;
var _default = self;
exports.default = _default;

/***/ }),

/***/ 928:
/***/ (function(module, exports, __webpack_require__) {

module.exports = __webpack_require__(708);


/***/ })

/******/ });
});