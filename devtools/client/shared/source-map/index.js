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

/***/ 584:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const md5 = __webpack_require__(644);

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

/***/ 585:
/***/ (function(module, exports) {

var charenc = {
  // UTF-8 encoding
  utf8: {
    // Convert a string to a byte array
    stringToBytes: function(str) {
      return charenc.bin.stringToBytes(unescape(encodeURIComponent(str)));
    },

    // Convert a byte array to a string
    bytesToString: function(bytes) {
      return decodeURIComponent(escape(charenc.bin.bytesToString(bytes)));
    }
  },

  // Binary encoding
  bin: {
    // Convert a string to a byte array
    stringToBytes: function(str) {
      for (var bytes = [], i = 0; i < str.length; i++)
        bytes.push(str.charCodeAt(i) & 0xFF);
      return bytes;
    },

    // Convert a byte array to a string
    bytesToString: function(bytes) {
      for (var str = [], i = 0; i < bytes.length; i++)
        str.push(String.fromCharCode(bytes[i]));
      return str.join('');
    }
  }
};

module.exports = charenc;


/***/ }),

/***/ 644:
/***/ (function(module, exports, __webpack_require__) {

(function(){
  var crypt = __webpack_require__(645),
      utf8 = __webpack_require__(585).utf8,
      isBuffer = __webpack_require__(646),
      bin = __webpack_require__(585).bin,

  // The core
  md5 = function (message, options) {
    // Convert to byte array
    if (message.constructor == String)
      if (options && options.encoding === 'binary')
        message = bin.stringToBytes(message);
      else
        message = utf8.stringToBytes(message);
    else if (isBuffer(message))
      message = Array.prototype.slice.call(message, 0);
    else if (!Array.isArray(message))
      message = message.toString();
    // else, assume byte array already

    var m = crypt.bytesToWords(message),
        l = message.length * 8,
        a =  1732584193,
        b = -271733879,
        c = -1732584194,
        d =  271733878;

    // Swap endian
    for (var i = 0; i < m.length; i++) {
      m[i] = ((m[i] <<  8) | (m[i] >>> 24)) & 0x00FF00FF |
             ((m[i] << 24) | (m[i] >>>  8)) & 0xFF00FF00;
    }

    // Padding
    m[l >>> 5] |= 0x80 << (l % 32);
    m[(((l + 64) >>> 9) << 4) + 14] = l;

    // Method shortcuts
    var FF = md5._ff,
        GG = md5._gg,
        HH = md5._hh,
        II = md5._ii;

    for (var i = 0; i < m.length; i += 16) {

      var aa = a,
          bb = b,
          cc = c,
          dd = d;

      a = FF(a, b, c, d, m[i+ 0],  7, -680876936);
      d = FF(d, a, b, c, m[i+ 1], 12, -389564586);
      c = FF(c, d, a, b, m[i+ 2], 17,  606105819);
      b = FF(b, c, d, a, m[i+ 3], 22, -1044525330);
      a = FF(a, b, c, d, m[i+ 4],  7, -176418897);
      d = FF(d, a, b, c, m[i+ 5], 12,  1200080426);
      c = FF(c, d, a, b, m[i+ 6], 17, -1473231341);
      b = FF(b, c, d, a, m[i+ 7], 22, -45705983);
      a = FF(a, b, c, d, m[i+ 8],  7,  1770035416);
      d = FF(d, a, b, c, m[i+ 9], 12, -1958414417);
      c = FF(c, d, a, b, m[i+10], 17, -42063);
      b = FF(b, c, d, a, m[i+11], 22, -1990404162);
      a = FF(a, b, c, d, m[i+12],  7,  1804603682);
      d = FF(d, a, b, c, m[i+13], 12, -40341101);
      c = FF(c, d, a, b, m[i+14], 17, -1502002290);
      b = FF(b, c, d, a, m[i+15], 22,  1236535329);

      a = GG(a, b, c, d, m[i+ 1],  5, -165796510);
      d = GG(d, a, b, c, m[i+ 6],  9, -1069501632);
      c = GG(c, d, a, b, m[i+11], 14,  643717713);
      b = GG(b, c, d, a, m[i+ 0], 20, -373897302);
      a = GG(a, b, c, d, m[i+ 5],  5, -701558691);
      d = GG(d, a, b, c, m[i+10],  9,  38016083);
      c = GG(c, d, a, b, m[i+15], 14, -660478335);
      b = GG(b, c, d, a, m[i+ 4], 20, -405537848);
      a = GG(a, b, c, d, m[i+ 9],  5,  568446438);
      d = GG(d, a, b, c, m[i+14],  9, -1019803690);
      c = GG(c, d, a, b, m[i+ 3], 14, -187363961);
      b = GG(b, c, d, a, m[i+ 8], 20,  1163531501);
      a = GG(a, b, c, d, m[i+13],  5, -1444681467);
      d = GG(d, a, b, c, m[i+ 2],  9, -51403784);
      c = GG(c, d, a, b, m[i+ 7], 14,  1735328473);
      b = GG(b, c, d, a, m[i+12], 20, -1926607734);

      a = HH(a, b, c, d, m[i+ 5],  4, -378558);
      d = HH(d, a, b, c, m[i+ 8], 11, -2022574463);
      c = HH(c, d, a, b, m[i+11], 16,  1839030562);
      b = HH(b, c, d, a, m[i+14], 23, -35309556);
      a = HH(a, b, c, d, m[i+ 1],  4, -1530992060);
      d = HH(d, a, b, c, m[i+ 4], 11,  1272893353);
      c = HH(c, d, a, b, m[i+ 7], 16, -155497632);
      b = HH(b, c, d, a, m[i+10], 23, -1094730640);
      a = HH(a, b, c, d, m[i+13],  4,  681279174);
      d = HH(d, a, b, c, m[i+ 0], 11, -358537222);
      c = HH(c, d, a, b, m[i+ 3], 16, -722521979);
      b = HH(b, c, d, a, m[i+ 6], 23,  76029189);
      a = HH(a, b, c, d, m[i+ 9],  4, -640364487);
      d = HH(d, a, b, c, m[i+12], 11, -421815835);
      c = HH(c, d, a, b, m[i+15], 16,  530742520);
      b = HH(b, c, d, a, m[i+ 2], 23, -995338651);

      a = II(a, b, c, d, m[i+ 0],  6, -198630844);
      d = II(d, a, b, c, m[i+ 7], 10,  1126891415);
      c = II(c, d, a, b, m[i+14], 15, -1416354905);
      b = II(b, c, d, a, m[i+ 5], 21, -57434055);
      a = II(a, b, c, d, m[i+12],  6,  1700485571);
      d = II(d, a, b, c, m[i+ 3], 10, -1894986606);
      c = II(c, d, a, b, m[i+10], 15, -1051523);
      b = II(b, c, d, a, m[i+ 1], 21, -2054922799);
      a = II(a, b, c, d, m[i+ 8],  6,  1873313359);
      d = II(d, a, b, c, m[i+15], 10, -30611744);
      c = II(c, d, a, b, m[i+ 6], 15, -1560198380);
      b = II(b, c, d, a, m[i+13], 21,  1309151649);
      a = II(a, b, c, d, m[i+ 4],  6, -145523070);
      d = II(d, a, b, c, m[i+11], 10, -1120210379);
      c = II(c, d, a, b, m[i+ 2], 15,  718787259);
      b = II(b, c, d, a, m[i+ 9], 21, -343485551);

      a = (a + aa) >>> 0;
      b = (b + bb) >>> 0;
      c = (c + cc) >>> 0;
      d = (d + dd) >>> 0;
    }

    return crypt.endian([a, b, c, d]);
  };

  // Auxiliary functions
  md5._ff  = function (a, b, c, d, x, s, t) {
    var n = a + (b & c | ~b & d) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };
  md5._gg  = function (a, b, c, d, x, s, t) {
    var n = a + (b & d | c & ~d) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };
  md5._hh  = function (a, b, c, d, x, s, t) {
    var n = a + (b ^ c ^ d) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };
  md5._ii  = function (a, b, c, d, x, s, t) {
    var n = a + (c ^ (b | ~d)) + (x >>> 0) + t;
    return ((n << s) | (n >>> (32 - s))) + b;
  };

  // Package private blocksize
  md5._blocksize = 16;
  md5._digestsize = 16;

  module.exports = function (message, options) {
    if (message === undefined || message === null)
      throw new Error('Illegal argument ' + message);

    var digestbytes = crypt.wordsToBytes(md5(message, options));
    return options && options.asBytes ? digestbytes :
        options && options.asString ? bin.bytesToString(digestbytes) :
        crypt.bytesToHex(digestbytes);
  };

})();


/***/ }),

/***/ 645:
/***/ (function(module, exports) {

(function() {
  var base64map
      = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/',

  crypt = {
    // Bit-wise rotation left
    rotl: function(n, b) {
      return (n << b) | (n >>> (32 - b));
    },

    // Bit-wise rotation right
    rotr: function(n, b) {
      return (n << (32 - b)) | (n >>> b);
    },

    // Swap big-endian to little-endian and vice versa
    endian: function(n) {
      // If number given, swap endian
      if (n.constructor == Number) {
        return crypt.rotl(n, 8) & 0x00FF00FF | crypt.rotl(n, 24) & 0xFF00FF00;
      }

      // Else, assume array and swap all items
      for (var i = 0; i < n.length; i++)
        n[i] = crypt.endian(n[i]);
      return n;
    },

    // Generate an array of any length of random bytes
    randomBytes: function(n) {
      for (var bytes = []; n > 0; n--)
        bytes.push(Math.floor(Math.random() * 256));
      return bytes;
    },

    // Convert a byte array to big-endian 32-bit words
    bytesToWords: function(bytes) {
      for (var words = [], i = 0, b = 0; i < bytes.length; i++, b += 8)
        words[b >>> 5] |= bytes[i] << (24 - b % 32);
      return words;
    },

    // Convert big-endian 32-bit words to a byte array
    wordsToBytes: function(words) {
      for (var bytes = [], b = 0; b < words.length * 32; b += 8)
        bytes.push((words[b >>> 5] >>> (24 - b % 32)) & 0xFF);
      return bytes;
    },

    // Convert a byte array to a hex string
    bytesToHex: function(bytes) {
      for (var hex = [], i = 0; i < bytes.length; i++) {
        hex.push((bytes[i] >>> 4).toString(16));
        hex.push((bytes[i] & 0xF).toString(16));
      }
      return hex.join('');
    },

    // Convert a hex string to a byte array
    hexToBytes: function(hex) {
      for (var bytes = [], c = 0; c < hex.length; c += 2)
        bytes.push(parseInt(hex.substr(c, 2), 16));
      return bytes;
    },

    // Convert a byte array to a base-64 string
    bytesToBase64: function(bytes) {
      for (var base64 = [], i = 0; i < bytes.length; i += 3) {
        var triplet = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
        for (var j = 0; j < 4; j++)
          if (i * 8 + j * 6 <= bytes.length * 8)
            base64.push(base64map.charAt((triplet >>> 6 * (3 - j)) & 0x3F));
          else
            base64.push('=');
      }
      return base64.join('');
    },

    // Convert a base-64 string to a byte array
    base64ToBytes: function(base64) {
      // Remove non-base-64 characters
      base64 = base64.replace(/[^A-Z0-9+\/]/ig, '');

      for (var bytes = [], i = 0, imod4 = 0; i < base64.length;
          imod4 = ++i % 4) {
        if (imod4 == 0) continue;
        bytes.push(((base64map.indexOf(base64.charAt(i - 1))
            & (Math.pow(2, -2 * imod4 + 8) - 1)) << (imod4 * 2))
            | (base64map.indexOf(base64.charAt(i)) >>> (6 - imod4 * 2)));
      }
      return bytes;
    }
  };

  module.exports = crypt;
})();


/***/ }),

/***/ 646:
/***/ (function(module, exports) {

/*!
 * Determine if an object is a Buffer
 *
 * @author   Feross Aboukhadijeh <https://feross.org>
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