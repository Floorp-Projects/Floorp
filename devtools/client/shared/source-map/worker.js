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
/******/ 	return __webpack_require__(__webpack_require__.s = 389);
/******/ })
/************************************************************************/
/******/ ({

/***/ 104:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const sourceMapRequests = new Map();

function clearSourceMaps() {
  sourceMapRequests.clear();
}

function getSourceMap(generatedSourceId) {
  return sourceMapRequests.get(generatedSourceId);
}

function setSourceMap(generatedId, request) {
  sourceMapRequests.set(generatedId, request);
}

module.exports = {
  clearSourceMaps,
  getSourceMap,
  setSourceMap
};

/***/ }),

/***/ 105:
/***/ (function(module, exports, __webpack_require__) {

(function(){
  var crypt = __webpack_require__(106),
      utf8 = __webpack_require__(36).utf8,
      isBuffer = __webpack_require__(107),
      bin = __webpack_require__(36).bin,

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

/***/ 106:
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

/***/ 107:
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

/***/ 13:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function networkRequest(url, opts) {
  return fetch(url, {
    cache: opts.loadFromCache ? "default" : "no-cache"
  }).then(res => {
    if (res.status >= 200 && res.status < 300) {
      if (res.headers.get("Content-Type") === "application/wasm") {
        return res.arrayBuffer().then(buffer => ({
          content: buffer,
          isDwarf: true
        }));
      }

      return res.text().then(text => ({
        content: text
      }));
    }

    return Promise.reject(`request failed with status ${res.status}`);
  });
}

module.exports = networkRequest;

/***/ }),

/***/ 14:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function WorkerDispatcher() {
  this.msgId = 1;
  this.worker = null;
}

WorkerDispatcher.prototype = {
  start(url, win = window) {
    this.worker = new win.Worker(url);

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

  task(method, {
    queue = false
  } = {}) {
    const calls = [];

    const push = args => {
      return new Promise((resolve, reject) => {
        if (queue && calls.length === 0) {
          Promise.resolve().then(flush);
        }

        calls.push([args, resolve, reject]);

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
        calls: items.map(item => item[0])
      });

      const listener = ({
        data: result
      }) => {
        if (result.id !== id) {
          return;
        }

        if (!this.worker) {
          return;
        }

        this.worker.removeEventListener("message", listener);
        result.results.forEach((resultData, i) => {
          const [, resolve, reject] = items[i];

          if (resultData.error) {
            reject(resultData.error);
          } else {
            resolve(resultData.response);
          }
        });
      };

      this.worker.addEventListener("message", listener);
    };

    return (...args) => push(args);
  },

  invoke(method, ...args) {
    return this.task(method)(...args);
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
          }), // Error can't be sent via postMessage, so be sure to
          // convert to string.
          err => ({
            error: err.toString()
          }));
        }

        return {
          response
        };
      } catch (error) {
        // Error can't be sent via postMessage, so be sure to convert to
        // string.
        return {
          error: error.toString()
        };
      }
    })).then(results => {
      self.postMessage({
        id,
        results
      });
    });
  };
}

module.exports = {
  WorkerDispatcher,
  workerHandler
};

/***/ }),

/***/ 171:
/***/ (function(module, exports, __webpack_require__) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

const base64VLQ = __webpack_require__(172);
const util = __webpack_require__(61);
const ArraySet = __webpack_require__(177).ArraySet;
const MappingList = __webpack_require__(402).MappingList;

/**
 * An instance of the SourceMapGenerator represents a source map which is
 * being built incrementally. You may pass an object with the following
 * properties:
 *
 *   - file: The filename of the generated source.
 *   - sourceRoot: A root for all relative URLs in this source map.
 */
class SourceMapGenerator {
  constructor(aArgs) {
    if (!aArgs) {
      aArgs = {};
    }
    this._file = util.getArg(aArgs, "file", null);
    this._sourceRoot = util.getArg(aArgs, "sourceRoot", null);
    this._skipValidation = util.getArg(aArgs, "skipValidation", false);
    this._sources = new ArraySet();
    this._names = new ArraySet();
    this._mappings = new MappingList();
    this._sourcesContents = null;
  }

  /**
   * Creates a new SourceMapGenerator based on a SourceMapConsumer
   *
   * @param aSourceMapConsumer The SourceMap.
   */
  static fromSourceMap(aSourceMapConsumer) {
    const sourceRoot = aSourceMapConsumer.sourceRoot;
    const generator = new SourceMapGenerator({
      file: aSourceMapConsumer.file,
      sourceRoot
    });
    aSourceMapConsumer.eachMapping(function(mapping) {
      const newMapping = {
        generated: {
          line: mapping.generatedLine,
          column: mapping.generatedColumn
        }
      };

      if (mapping.source != null) {
        newMapping.source = mapping.source;
        if (sourceRoot != null) {
          newMapping.source = util.relative(sourceRoot, newMapping.source);
        }

        newMapping.original = {
          line: mapping.originalLine,
          column: mapping.originalColumn
        };

        if (mapping.name != null) {
          newMapping.name = mapping.name;
        }
      }

      generator.addMapping(newMapping);
    });
    aSourceMapConsumer.sources.forEach(function(sourceFile) {
      let sourceRelative = sourceFile;
      if (sourceRoot !== null) {
        sourceRelative = util.relative(sourceRoot, sourceFile);
      }

      if (!generator._sources.has(sourceRelative)) {
        generator._sources.add(sourceRelative);
      }

      const content = aSourceMapConsumer.sourceContentFor(sourceFile);
      if (content != null) {
        generator.setSourceContent(sourceFile, content);
      }
    });
    return generator;
  }

  /**
   * Add a single mapping from original source line and column to the generated
   * source's line and column for this source map being created. The mapping
   * object should have the following properties:
   *
   *   - generated: An object with the generated line and column positions.
   *   - original: An object with the original line and column positions.
   *   - source: The original source file (relative to the sourceRoot).
   *   - name: An optional original token name for this mapping.
   */
  addMapping(aArgs) {
    const generated = util.getArg(aArgs, "generated");
    const original = util.getArg(aArgs, "original", null);
    let source = util.getArg(aArgs, "source", null);
    let name = util.getArg(aArgs, "name", null);

    if (!this._skipValidation) {
      this._validateMapping(generated, original, source, name);
    }

    if (source != null) {
      source = String(source);
      if (!this._sources.has(source)) {
        this._sources.add(source);
      }
    }

    if (name != null) {
      name = String(name);
      if (!this._names.has(name)) {
        this._names.add(name);
      }
    }

    this._mappings.add({
      generatedLine: generated.line,
      generatedColumn: generated.column,
      originalLine: original != null && original.line,
      originalColumn: original != null && original.column,
      source,
      name
    });
  }

  /**
   * Set the source content for a source file.
   */
  setSourceContent(aSourceFile, aSourceContent) {
    let source = aSourceFile;
    if (this._sourceRoot != null) {
      source = util.relative(this._sourceRoot, source);
    }

    if (aSourceContent != null) {
      // Add the source content to the _sourcesContents map.
      // Create a new _sourcesContents map if the property is null.
      if (!this._sourcesContents) {
        this._sourcesContents = Object.create(null);
      }
      this._sourcesContents[util.toSetString(source)] = aSourceContent;
    } else if (this._sourcesContents) {
      // Remove the source file from the _sourcesContents map.
      // If the _sourcesContents map is empty, set the property to null.
      delete this._sourcesContents[util.toSetString(source)];
      if (Object.keys(this._sourcesContents).length === 0) {
        this._sourcesContents = null;
      }
    }
  }

  /**
   * Applies the mappings of a sub-source-map for a specific source file to the
   * source map being generated. Each mapping to the supplied source file is
   * rewritten using the supplied source map. Note: The resolution for the
   * resulting mappings is the minimium of this map and the supplied map.
   *
   * @param aSourceMapConsumer The source map to be applied.
   * @param aSourceFile Optional. The filename of the source file.
   *        If omitted, SourceMapConsumer's file property will be used.
   * @param aSourceMapPath Optional. The dirname of the path to the source map
   *        to be applied. If relative, it is relative to the SourceMapConsumer.
   *        This parameter is needed when the two source maps aren't in the same
   *        directory, and the source map to be applied contains relative source
   *        paths. If so, those relative source paths need to be rewritten
   *        relative to the SourceMapGenerator.
   */
  applySourceMap(aSourceMapConsumer, aSourceFile, aSourceMapPath) {
    let sourceFile = aSourceFile;
    // If aSourceFile is omitted, we will use the file property of the SourceMap
    if (aSourceFile == null) {
      if (aSourceMapConsumer.file == null) {
        throw new Error(
          "SourceMapGenerator.prototype.applySourceMap requires either an explicit source file, " +
          'or the source map\'s "file" property. Both were omitted.'
        );
      }
      sourceFile = aSourceMapConsumer.file;
    }
    const sourceRoot = this._sourceRoot;
    // Make "sourceFile" relative if an absolute Url is passed.
    if (sourceRoot != null) {
      sourceFile = util.relative(sourceRoot, sourceFile);
    }
    // Applying the SourceMap can add and remove items from the sources and
    // the names array.
    const newSources = this._mappings.toArray().length > 0
      ? new ArraySet()
      : this._sources;
    const newNames = new ArraySet();

    // Find mappings for the "sourceFile"
    this._mappings.unsortedForEach(function(mapping) {
      if (mapping.source === sourceFile && mapping.originalLine != null) {
        // Check if it can be mapped by the source map, then update the mapping.
        const original = aSourceMapConsumer.originalPositionFor({
          line: mapping.originalLine,
          column: mapping.originalColumn
        });
        if (original.source != null) {
          // Copy mapping
          mapping.source = original.source;
          if (aSourceMapPath != null) {
            mapping.source = util.join(aSourceMapPath, mapping.source);
          }
          if (sourceRoot != null) {
            mapping.source = util.relative(sourceRoot, mapping.source);
          }
          mapping.originalLine = original.line;
          mapping.originalColumn = original.column;
          if (original.name != null) {
            mapping.name = original.name;
          }
        }
      }

      const source = mapping.source;
      if (source != null && !newSources.has(source)) {
        newSources.add(source);
      }

      const name = mapping.name;
      if (name != null && !newNames.has(name)) {
        newNames.add(name);
      }

    }, this);
    this._sources = newSources;
    this._names = newNames;

    // Copy sourcesContents of applied map.
    aSourceMapConsumer.sources.forEach(function(srcFile) {
      const content = aSourceMapConsumer.sourceContentFor(srcFile);
      if (content != null) {
        if (aSourceMapPath != null) {
          srcFile = util.join(aSourceMapPath, srcFile);
        }
        if (sourceRoot != null) {
          srcFile = util.relative(sourceRoot, srcFile);
        }
        this.setSourceContent(srcFile, content);
      }
    }, this);
  }

  /**
   * A mapping can have one of the three levels of data:
   *
   *   1. Just the generated position.
   *   2. The Generated position, original position, and original source.
   *   3. Generated and original position, original source, as well as a name
   *      token.
   *
   * To maintain consistency, we validate that any new mapping being added falls
   * in to one of these categories.
   */
  _validateMapping(aGenerated, aOriginal, aSource, aName) {
    // When aOriginal is truthy but has empty values for .line and .column,
    // it is most likely a programmer error. In this case we throw a very
    // specific error message to try to guide them the right way.
    // For example: https://github.com/Polymer/polymer-bundler/pull/519
    if (aOriginal && typeof aOriginal.line !== "number" && typeof aOriginal.column !== "number") {
        throw new Error(
            "original.line and original.column are not numbers -- you probably meant to omit " +
            "the original mapping entirely and only map the generated position. If so, pass " +
            "null for the original mapping instead of an object with empty or null values."
        );
    }

    if (aGenerated && "line" in aGenerated && "column" in aGenerated
        && aGenerated.line > 0 && aGenerated.column >= 0
        && !aOriginal && !aSource && !aName) {
      // Case 1.

    } else if (aGenerated && "line" in aGenerated && "column" in aGenerated
             && aOriginal && "line" in aOriginal && "column" in aOriginal
             && aGenerated.line > 0 && aGenerated.column >= 0
             && aOriginal.line > 0 && aOriginal.column >= 0
             && aSource) {
      // Cases 2 and 3.

    } else {
      throw new Error("Invalid mapping: " + JSON.stringify({
        generated: aGenerated,
        source: aSource,
        original: aOriginal,
        name: aName
      }));
    }
  }

  /**
   * Serialize the accumulated mappings in to the stream of base 64 VLQs
   * specified by the source map format.
   */
  _serializeMappings() {
    let previousGeneratedColumn = 0;
    let previousGeneratedLine = 1;
    let previousOriginalColumn = 0;
    let previousOriginalLine = 0;
    let previousName = 0;
    let previousSource = 0;
    let result = "";
    let next;
    let mapping;
    let nameIdx;
    let sourceIdx;

    const mappings = this._mappings.toArray();
    for (let i = 0, len = mappings.length; i < len; i++) {
      mapping = mappings[i];
      next = "";

      if (mapping.generatedLine !== previousGeneratedLine) {
        previousGeneratedColumn = 0;
        while (mapping.generatedLine !== previousGeneratedLine) {
          next += ";";
          previousGeneratedLine++;
        }
      } else if (i > 0) {
        if (!util.compareByGeneratedPositionsInflated(mapping, mappings[i - 1])) {
          continue;
        }
        next += ",";
      }

      next += base64VLQ.encode(mapping.generatedColumn
                                 - previousGeneratedColumn);
      previousGeneratedColumn = mapping.generatedColumn;

      if (mapping.source != null) {
        sourceIdx = this._sources.indexOf(mapping.source);
        next += base64VLQ.encode(sourceIdx - previousSource);
        previousSource = sourceIdx;

        // lines are stored 0-based in SourceMap spec version 3
        next += base64VLQ.encode(mapping.originalLine - 1
                                   - previousOriginalLine);
        previousOriginalLine = mapping.originalLine - 1;

        next += base64VLQ.encode(mapping.originalColumn
                                   - previousOriginalColumn);
        previousOriginalColumn = mapping.originalColumn;

        if (mapping.name != null) {
          nameIdx = this._names.indexOf(mapping.name);
          next += base64VLQ.encode(nameIdx - previousName);
          previousName = nameIdx;
        }
      }

      result += next;
    }

    return result;
  }

  _generateSourcesContent(aSources, aSourceRoot) {
    return aSources.map(function(source) {
      if (!this._sourcesContents) {
        return null;
      }
      if (aSourceRoot != null) {
        source = util.relative(aSourceRoot, source);
      }
      const key = util.toSetString(source);
      return Object.prototype.hasOwnProperty.call(this._sourcesContents, key)
        ? this._sourcesContents[key]
        : null;
    }, this);
  }

  /**
   * Externalize the source map.
   */
  toJSON() {
    const map = {
      version: this._version,
      sources: this._sources.toArray(),
      names: this._names.toArray(),
      mappings: this._serializeMappings()
    };
    if (this._file != null) {
      map.file = this._file;
    }
    if (this._sourceRoot != null) {
      map.sourceRoot = this._sourceRoot;
    }
    if (this._sourcesContents) {
      map.sourcesContent = this._generateSourcesContent(map.sources, map.sourceRoot);
    }

    return map;
  }

  /**
   * Render the source map being generated to a string.
   */
  toString() {
    return JSON.stringify(this.toJSON());
  }
}

SourceMapGenerator.prototype._version = 3;
exports.SourceMapGenerator = SourceMapGenerator;


/***/ }),

/***/ 172:
/***/ (function(module, exports, __webpack_require__) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * Based on the Base 64 VLQ implementation in Closure Compiler:
 * https://code.google.com/p/closure-compiler/source/browse/trunk/src/com/google/debugging/sourcemap/Base64VLQ.java
 *
 * Copyright 2011 The Closure Compiler Authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of Google Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const base64 = __webpack_require__(392);

// A single base 64 digit can contain 6 bits of data. For the base 64 variable
// length quantities we use in the source map spec, the first bit is the sign,
// the next four bits are the actual value, and the 6th bit is the
// continuation bit. The continuation bit tells us whether there are more
// digits in this value following this digit.
//
//   Continuation
//   |    Sign
//   |    |
//   V    V
//   101011

const VLQ_BASE_SHIFT = 5;

// binary: 100000
const VLQ_BASE = 1 << VLQ_BASE_SHIFT;

// binary: 011111
const VLQ_BASE_MASK = VLQ_BASE - 1;

// binary: 100000
const VLQ_CONTINUATION_BIT = VLQ_BASE;

/**
 * Converts from a two-complement value to a value where the sign bit is
 * placed in the least significant bit.  For example, as decimals:
 *   1 becomes 2 (10 binary), -1 becomes 3 (11 binary)
 *   2 becomes 4 (100 binary), -2 becomes 5 (101 binary)
 */
function toVLQSigned(aValue) {
  return aValue < 0
    ? ((-aValue) << 1) + 1
    : (aValue << 1) + 0;
}

/**
 * Converts to a two-complement value from a value where the sign bit is
 * placed in the least significant bit.  For example, as decimals:
 *   2 (10 binary) becomes 1, 3 (11 binary) becomes -1
 *   4 (100 binary) becomes 2, 5 (101 binary) becomes -2
 */
// eslint-disable-next-line no-unused-vars
function fromVLQSigned(aValue) {
  const isNegative = (aValue & 1) === 1;
  const shifted = aValue >> 1;
  return isNegative
    ? -shifted
    : shifted;
}

/**
 * Returns the base 64 VLQ encoded value.
 */
exports.encode = function base64VLQ_encode(aValue) {
  let encoded = "";
  let digit;

  let vlq = toVLQSigned(aValue);

  do {
    digit = vlq & VLQ_BASE_MASK;
    vlq >>>= VLQ_BASE_SHIFT;
    if (vlq > 0) {
      // There are still more digits in this value, so we must make sure the
      // continuation bit is marked.
      digit |= VLQ_CONTINUATION_BIT;
    }
    encoded += base64.encode(digit);
  } while (vlq > 0);

  return encoded;
};


/***/ }),

/***/ 177:
/***/ (function(module, exports) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

/**
 * A data structure which is a combination of an array and a set. Adding a new
 * member is O(1), testing for membership is O(1), and finding the index of an
 * element is O(1). Removing elements from the set is not supported. Only
 * strings are supported for membership.
 */
class ArraySet {
  constructor() {
    this._array = [];
    this._set = new Map();
  }

  /**
   * Static method for creating ArraySet instances from an existing array.
   */
  static fromArray(aArray, aAllowDuplicates) {
    const set = new ArraySet();
    for (let i = 0, len = aArray.length; i < len; i++) {
      set.add(aArray[i], aAllowDuplicates);
    }
    return set;
  }

  /**
   * Return how many unique items are in this ArraySet. If duplicates have been
   * added, than those do not count towards the size.
   *
   * @returns Number
   */
  size() {
    return this._set.size;
  }

  /**
   * Add the given string to this set.
   *
   * @param String aStr
   */
  add(aStr, aAllowDuplicates) {
    const isDuplicate = this.has(aStr);
    const idx = this._array.length;
    if (!isDuplicate || aAllowDuplicates) {
      this._array.push(aStr);
    }
    if (!isDuplicate) {
      this._set.set(aStr, idx);
    }
  }

  /**
   * Is the given string a member of this set?
   *
   * @param String aStr
   */
  has(aStr) {
      return this._set.has(aStr);
  }

  /**
   * What is the index of the given string in the array?
   *
   * @param String aStr
   */
  indexOf(aStr) {
    const idx = this._set.get(aStr);
    if (idx >= 0) {
        return idx;
    }
    throw new Error('"' + aStr + '" is not in the set.');
  }

  /**
   * What is the element at the given index?
   *
   * @param Number aIdx
   */
  at(aIdx) {
    if (aIdx >= 0 && aIdx < this._array.length) {
      return this._array[aIdx];
    }
    throw new Error("No element indexed by " + aIdx);
  }

  /**
   * Returns the array representation of this set (which has the proper indices
   * indicated by indexOf). Note that this is a copy of the internal array used
   * for storing the members so that no one can mess with internal state.
   */
  toArray() {
    return this._array.slice();
  }
}
exports.ArraySet = ArraySet;


/***/ }),

/***/ 178:
/***/ (function(module, exports, __webpack_require__) {

"use strict";


let mappingsWasm = null;

module.exports = function readWasm() {
  if (typeof mappingsWasm === "string") {
    return fetch(mappingsWasm)
      .then(response => response.arrayBuffer());
  }
  if (mappingsWasm instanceof ArrayBuffer) {
    return Promise.resolve(mappingsWasm);
  }

  throw new Error("You must provide the string URL or ArrayBuffer contents " +
                  "of lib/mappings.wasm by calling " +
                  "SourceMapConsumer.initialize({ 'lib/mappings.wasm': ... }) " +
                  "before using SourceMapConsumer");
};

module.exports.initialize = input => {
  mappingsWasm = input;
};


/***/ }),

/***/ 179:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  SourceMapConsumer
} = __webpack_require__(60);

async function createConsumer(map, sourceMapUrl) {
  return new SourceMapConsumer(map, sourceMapUrl);
}

module.exports = {
  createConsumer
};

/***/ }),

/***/ 36:
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

/***/ 389:
/***/ (function(module, exports, __webpack_require__) {

module.exports = __webpack_require__(390);


/***/ }),

/***/ 390:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  getOriginalURLs,
  hasOriginalURL,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getAllGeneratedLocations,
  getOriginalLocation,
  getOriginalLocations,
  getOriginalSourceText,
  getGeneratedRangesForOriginal,
  getFileGeneratedRange,
  hasMappedSource,
  clearSourceMaps,
  applySourceMap
} = __webpack_require__(391);

const {
  getOriginalStackFrames
} = __webpack_require__(411);

const {
  setAssetRootURL
} = __webpack_require__(515);

const {
  workerUtils: {
    workerHandler
  }
} = __webpack_require__(7); // The interface is implemented in source-map to be
// easier to unit test.


self.onmessage = workerHandler({
  setAssetRootURL,
  getOriginalURLs,
  hasOriginalURL,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getAllGeneratedLocations,
  getOriginalLocation,
  getOriginalLocations,
  getOriginalSourceText,
  getOriginalStackFrames,
  getGeneratedRangesForOriginal,
  getFileGeneratedRange,
  hasMappedSource,
  applySourceMap,
  clearSourceMaps
});

/***/ }),

/***/ 391:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Source Map Worker
 * @module utils/source-map-worker
 */
const {
  networkRequest
} = __webpack_require__(7);

const {
  SourceMapConsumer,
  SourceMapGenerator
} = __webpack_require__(60);

const {
  createConsumer
} = __webpack_require__(179);

const assert = __webpack_require__(407);

const {
  fetchSourceMap,
  hasOriginalURL,
  clearOriginalURLs
} = __webpack_require__(408);

const {
  getSourceMap,
  setSourceMap,
  clearSourceMaps: clearSourceMapsRequests
} = __webpack_require__(104);

const {
  originalToGeneratedId,
  generatedToOriginalId,
  isGeneratedId,
  isOriginalId,
  getContentType
} = __webpack_require__(64);

const {
  clearWasmXScopes
} = __webpack_require__(510);

async function getOriginalURLs(generatedSource) {
  const map = await fetchSourceMap(generatedSource);
  return map && map.sources;
}

const COMPUTED_SPANS = new WeakSet();
const SOURCE_MAPPINGS = new WeakMap();

async function getOriginalRanges(sourceId, url) {
  if (!isOriginalId(sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(sourceId);
  const map = await getSourceMap(generatedSourceId);

  if (!map) {
    return [];
  }

  let mappings = SOURCE_MAPPINGS.get(map);

  if (!mappings) {
    mappings = new Map();
    SOURCE_MAPPINGS.set(map, mappings);
  }

  let fileMappings = mappings.get(url);

  if (!fileMappings) {
    fileMappings = [];
    mappings.set(url, fileMappings);
    const originalMappings = fileMappings;
    map.eachMapping(mapping => {
      if (mapping.source !== url) {
        return;
      }

      const last = originalMappings[originalMappings.length - 1];

      if (last && last.line === mapping.originalLine) {
        if (last.columnStart < mapping.originalColumn) {
          last.columnEnd = mapping.originalColumn;
        } else {
          // Skip this duplicate original location,
          return;
        }
      }

      originalMappings.push({
        line: mapping.originalLine,
        columnStart: mapping.originalColumn,
        columnEnd: Infinity
      });
    }, null, SourceMapConsumer.ORIGINAL_ORDER);
  }

  return fileMappings;
}
/**
 * Given an original location, find the ranges on the generated file that
 * are mapped from the original range containing the location.
 */


async function getGeneratedRanges(location, originalSource) {
  if (!isOriginalId(location.sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const map = await getSourceMap(generatedSourceId);

  if (!map) {
    return [];
  }

  if (!COMPUTED_SPANS.has(map)) {
    COMPUTED_SPANS.add(map);
    map.computeColumnSpans();
  } // We want to use 'allGeneratedPositionsFor' to get the _first_ generated
  // location, but it hard-codes SourceMapConsumer.LEAST_UPPER_BOUND as the
  // bias, making it search in the wrong direction for this usecase.
  // To work around this, we use 'generatedPositionFor' and then look up the
  // exact original location, making any bias value unnecessary, and then
  // use that location for the call to 'allGeneratedPositionsFor'.


  const genPos = map.generatedPositionFor({
    source: originalSource.url,
    line: location.line,
    column: location.column == null ? 0 : location.column,
    bias: SourceMapConsumer.GREATEST_LOWER_BOUND
  });

  if (genPos.line === null) {
    return [];
  }

  const positions = map.allGeneratedPositionsFor(map.originalPositionFor({
    line: genPos.line,
    column: genPos.column
  }));
  return positions.map(mapping => ({
    line: mapping.line,
    columnStart: mapping.column,
    columnEnd: mapping.lastColumn
  })).sort((a, b) => {
    const line = a.line - b.line;
    return line === 0 ? a.column - b.column : line;
  });
}

async function getGeneratedLocation(location, originalSource) {
  if (!isOriginalId(location.sourceId)) {
    return location;
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const map = await getSourceMap(generatedSourceId);

  if (!map) {
    return location;
  }

  const positions = map.allGeneratedPositionsFor({
    source: originalSource.url,
    line: location.line,
    column: location.column == null ? 0 : location.column
  }); // Prior to source-map 0.7, the source-map module returned the earliest
  // generated location in the file when there were multiple generated
  // locations. The current comparison fn in 0.7 does not appear to take
  // generated location into account properly.

  let match;

  for (const pos of positions) {
    if (!match || pos.line < match.line || pos.column < match.column) {
      match = pos;
    }
  }

  if (!match) {
    match = map.generatedPositionFor({
      source: originalSource.url,
      line: location.line,
      column: location.column == null ? 0 : location.column,
      bias: SourceMapConsumer.LEAST_UPPER_BOUND
    });
  }

  return {
    sourceId: generatedSourceId,
    line: match.line,
    column: match.column
  };
}

async function getAllGeneratedLocations(location, originalSource) {
  if (!isOriginalId(location.sourceId)) {
    return [];
  }

  const generatedSourceId = originalToGeneratedId(location.sourceId);
  const map = await getSourceMap(generatedSourceId);

  if (!map) {
    return [];
  }

  const positions = map.allGeneratedPositionsFor({
    source: originalSource.url,
    line: location.line,
    column: location.column == null ? 0 : location.column
  });
  return positions.map(({
    line,
    column
  }) => ({
    sourceId: generatedSourceId,
    line,
    column
  }));
}

async function getOriginalLocations(sourceId, locations, options = {}) {
  if (locations.some(location => location.sourceId != sourceId)) {
    throw new Error("Generated locations must belong to the same source");
  }

  const map = await getSourceMap(sourceId);

  if (!map) {
    return locations;
  }

  return locations.map(location => getOriginalLocationSync(map, location, options));
}

function getOriginalLocationSync(map, location, {
  search
} = {}) {
  // First check for an exact match
  let match = map.originalPositionFor({
    line: location.line,
    column: location.column == null ? 0 : location.column
  }); // If there is not an exact match, look for a match with a bias at the
  // current location and then on subsequent lines

  if (search) {
    let line = location.line;
    let column = location.column == null ? 0 : location.column;

    while (match.source === null) {
      match = map.originalPositionFor({
        line,
        column,
        bias: SourceMapConsumer[search]
      });
      line += search == "LEAST_UPPER_BOUND" ? 1 : -1;
      column = search == "LEAST_UPPER_BOUND" ? 0 : Infinity;
    }
  }

  const {
    source: sourceUrl,
    line,
    column
  } = match;

  if (sourceUrl == null) {
    // No url means the location didn't map.
    return location;
  }

  return {
    sourceId: generatedToOriginalId(location.sourceId, sourceUrl),
    sourceUrl,
    line,
    column
  };
}

async function getOriginalLocation(location, options = {}) {
  if (!isGeneratedId(location.sourceId)) {
    return location;
  }

  const map = await getSourceMap(location.sourceId);

  if (!map) {
    return location;
  }

  return getOriginalLocationSync(map, location, options);
}

async function getOriginalSourceText(originalSource) {
  assert(isOriginalId(originalSource.id), "Source is not an original source");
  const generatedSourceId = originalToGeneratedId(originalSource.id);
  const map = await getSourceMap(generatedSourceId);

  if (!map) {
    return null;
  }

  let text = map.sourceContentFor(originalSource.url);

  if (!text) {
    text = (await networkRequest(originalSource.url, {
      loadFromCache: false
    })).content;
  }

  return {
    text,
    contentType: getContentType(originalSource.url || "")
  };
}
/**
 * Find the set of ranges on the generated file that map from the original
 * file's locations.
 *
 * @param sourceId - The original ID of the file we are processing.
 * @param url - The original URL of the file we are processing.
 * @param mergeUnmappedRegions - If unmapped regions are encountered between
 *   two mappings for the given original file, allow the two mappings to be
 *   merged anyway. This is useful if you are more interested in the general
 *   contiguous ranges associated with a file, rather than the specifics of
 *   the ranges provided by the sourcemap.
 */


const GENERATED_MAPPINGS = new WeakMap();

async function getGeneratedRangesForOriginal(sourceId, url, mergeUnmappedRegions = false) {
  assert(isOriginalId(sourceId), "Source is not an original source");
  const map = await getSourceMap(originalToGeneratedId(sourceId)); // NOTE: this is only needed for Flow

  if (!map) {
    return [];
  }

  if (!COMPUTED_SPANS.has(map)) {
    COMPUTED_SPANS.add(map);
    map.computeColumnSpans();
  }

  if (!GENERATED_MAPPINGS.has(map)) {
    GENERATED_MAPPINGS.set(map, new Map());
  }

  const generatedRangesMap = GENERATED_MAPPINGS.get(map);

  if (!generatedRangesMap) {
    return [];
  }

  if (generatedRangesMap.has(sourceId)) {
    // NOTE we need to coerce the result to an array for Flow
    return generatedRangesMap.get(sourceId) || [];
  } // Gather groups of mappings on the generated file, with new groups created
  // if we cross a mapping for a different file.


  let currentGroup = [];
  const originalGroups = [currentGroup];
  map.eachMapping(mapping => {
    if (mapping.source === url) {
      currentGroup.push({
        start: {
          line: mapping.generatedLine,
          column: mapping.generatedColumn
        },
        end: {
          line: mapping.generatedLine,
          // The lastGeneratedColumn value is an inclusive value so we add
          // one to it to get the exclusive end position.
          column: mapping.lastGeneratedColumn + 1
        }
      });
    } else if (typeof mapping.source === "string" && currentGroup.length > 0) {
      // If there is a URL, but it is for a _different_ file, we create a
      // new group of mappings so that we can tell
      currentGroup = [];
      originalGroups.push(currentGroup);
    }
  }, null, SourceMapConsumer.GENERATED_ORDER);
  const generatedMappingsForOriginal = [];

  if (mergeUnmappedRegions) {
    // If we don't care about excluding unmapped regions, then we just need to
    // create a range that is the fully encompasses each group, ignoring the
    // empty space between each individual range.
    for (const group of originalGroups) {
      if (group.length > 0) {
        generatedMappingsForOriginal.push({
          start: group[0].start,
          end: group[group.length - 1].end
        });
      }
    }
  } else {
    let lastEntry;

    for (const group of originalGroups) {
      lastEntry = null;

      for (const {
        start,
        end
      } of group) {
        const lastEnd = lastEntry ? wrappedMappingPosition(lastEntry.end) : null; // If this entry comes immediately after the previous one, extend the
        // range of the previous entry instead of adding a new one.

        if (lastEntry && lastEnd && lastEnd.line === start.line && lastEnd.column === start.column) {
          lastEntry.end = end;
        } else {
          const newEntry = {
            start,
            end
          };
          generatedMappingsForOriginal.push(newEntry);
          lastEntry = newEntry;
        }
      }
    }
  }

  generatedRangesMap.set(sourceId, generatedMappingsForOriginal);
  return generatedMappingsForOriginal;
}

function wrappedMappingPosition(pos) {
  if (pos.column !== Infinity) {
    return pos;
  } // If the end of the entry consumes the whole line, treat it as wrapping to
  // the next line.


  return {
    line: pos.line + 1,
    column: 0
  };
}

async function getFileGeneratedRange(originalSource) {
  assert(isOriginalId(originalSource.id), "Source is not an original source");
  const map = await getSourceMap(originalToGeneratedId(originalSource.id));

  if (!map) {
    return;
  }

  const start = map.generatedPositionFor({
    source: originalSource.url,
    line: 1,
    column: 0,
    bias: SourceMapConsumer.LEAST_UPPER_BOUND
  });
  const end = map.generatedPositionFor({
    source: originalSource.url,
    line: Number.MAX_SAFE_INTEGER,
    column: Number.MAX_SAFE_INTEGER,
    bias: SourceMapConsumer.GREATEST_LOWER_BOUND
  });
  return {
    start,
    end
  };
}

async function hasMappedSource(location) {
  if (isOriginalId(location.sourceId)) {
    return true;
  }

  const loc = await getOriginalLocation(location);
  return loc.sourceId !== location.sourceId;
}

function applySourceMap(generatedId, url, code, mappings) {
  const generator = new SourceMapGenerator({
    file: url
  });
  mappings.forEach(mapping => generator.addMapping(mapping));
  generator.setSourceContent(url, code);
  const map = createConsumer(generator.toJSON());
  setSourceMap(generatedId, Promise.resolve(map));
}

function clearSourceMaps() {
  clearSourceMapsRequests();
  clearWasmXScopes();
  clearOriginalURLs();
}

module.exports = {
  getOriginalURLs,
  hasOriginalURL,
  getOriginalRanges,
  getGeneratedRanges,
  getGeneratedLocation,
  getAllGeneratedLocations,
  getOriginalLocation,
  getOriginalLocations,
  getOriginalSourceText,
  getGeneratedRangesForOriginal,
  getFileGeneratedRange,
  applySourceMap,
  clearSourceMaps,
  hasMappedSource
};

/***/ }),

/***/ 392:
/***/ (function(module, exports) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

const intToCharMap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".split("");

/**
 * Encode an integer in the range of 0 to 63 to a single base 64 digit.
 */
exports.encode = function(number) {
  if (0 <= number && number < intToCharMap.length) {
    return intToCharMap[number];
  }
  throw new TypeError("Must be between 0 and 63: " + number);
};


/***/ }),

/***/ 393:
/***/ (function(module, exports, __webpack_require__) {

"use strict";
/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */


/**
 * Browser 'URL' implementations have been found to handle non-standard URL
 * schemes poorly, and schemes like
 *
 *   webpack:///src/folder/file.js
 *
 * are very common in source maps. For the time being we use a JS
 * implementation in these contexts instead. See
 *
 * * https://bugzilla.mozilla.org/show_bug.cgi?id=1374505
 * * https://bugs.chromium.org/p/chromium/issues/detail?id=734880
 */
module.exports = __webpack_require__(507).URL;


/***/ }),

/***/ 402:
/***/ (function(module, exports, __webpack_require__) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2014 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

const util = __webpack_require__(61);

/**
 * Determine whether mappingB is after mappingA with respect to generated
 * position.
 */
function generatedPositionAfter(mappingA, mappingB) {
  // Optimized for most common case
  const lineA = mappingA.generatedLine;
  const lineB = mappingB.generatedLine;
  const columnA = mappingA.generatedColumn;
  const columnB = mappingB.generatedColumn;
  return lineB > lineA || lineB == lineA && columnB >= columnA ||
         util.compareByGeneratedPositionsInflated(mappingA, mappingB) <= 0;
}

/**
 * A data structure to provide a sorted view of accumulated mappings in a
 * performance conscious manner. It trades a negligible overhead in general
 * case for a large speedup in case of mappings being added in order.
 */
class MappingList {
  constructor() {
    this._array = [];
    this._sorted = true;
    // Serves as infimum
    this._last = {generatedLine: -1, generatedColumn: 0};
  }

  /**
   * Iterate through internal items. This method takes the same arguments that
   * `Array.prototype.forEach` takes.
   *
   * NOTE: The order of the mappings is NOT guaranteed.
   */
  unsortedForEach(aCallback, aThisArg) {
    this._array.forEach(aCallback, aThisArg);
  }

  /**
   * Add the given source mapping.
   *
   * @param Object aMapping
   */
  add(aMapping) {
    if (generatedPositionAfter(this._last, aMapping)) {
      this._last = aMapping;
      this._array.push(aMapping);
    } else {
      this._sorted = false;
      this._array.push(aMapping);
    }
  }

  /**
   * Returns the flat, sorted array of mappings. The mappings are sorted by
   * generated position.
   *
   * WARNING: This method returns internal data without copying, for
   * performance. The return value must NOT be mutated, and should be treated as
   * an immutable borrow. If you want to take ownership, you must make your own
   * copy.
   */
  toArray() {
    if (!this._sorted) {
      this._array.sort(util.compareByGeneratedPositionsInflated);
      this._sorted = true;
    }
    return this._array;
  }
}

exports.MappingList = MappingList;


/***/ }),

/***/ 403:
/***/ (function(module, exports, __webpack_require__) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

const util = __webpack_require__(61);
const binarySearch = __webpack_require__(404);
const ArraySet = __webpack_require__(177).ArraySet;
const base64VLQ = __webpack_require__(172); // eslint-disable-line no-unused-vars
const readWasm = __webpack_require__(178);
const wasm = __webpack_require__(405);

const INTERNAL = Symbol("smcInternal");

class SourceMapConsumer {
  constructor(aSourceMap, aSourceMapURL) {
    // If the constructor was called by super(), just return Promise<this>.
    // Yes, this is a hack to retain the pre-existing API of the base-class
    // constructor also being an async factory function.
    if (aSourceMap == INTERNAL) {
      return Promise.resolve(this);
    }

    return _factory(aSourceMap, aSourceMapURL);
  }

  static initialize(opts) {
    readWasm.initialize(opts["lib/mappings.wasm"]);
  }

  static fromSourceMap(aSourceMap, aSourceMapURL) {
    return _factoryBSM(aSourceMap, aSourceMapURL);
  }

  /**
   * Construct a new `SourceMapConsumer` from `rawSourceMap` and `sourceMapUrl`
   * (see the `SourceMapConsumer` constructor for details. Then, invoke the `async
   * function f(SourceMapConsumer) -> T` with the newly constructed consumer, wait
   * for `f` to complete, call `destroy` on the consumer, and return `f`'s return
   * value.
   *
   * You must not use the consumer after `f` completes!
   *
   * By using `with`, you do not have to remember to manually call `destroy` on
   * the consumer, since it will be called automatically once `f` completes.
   *
   * ```js
   * const xSquared = await SourceMapConsumer.with(
   *   myRawSourceMap,
   *   null,
   *   async function (consumer) {
   *     // Use `consumer` inside here and don't worry about remembering
   *     // to call `destroy`.
   *
   *     const x = await whatever(consumer);
   *     return x * x;
   *   }
   * );
   *
   * // You may not use that `consumer` anymore out here; it has
   * // been destroyed. But you can use `xSquared`.
   * console.log(xSquared);
   * ```
   */
  static async with(rawSourceMap, sourceMapUrl, f) {
    const consumer = await new SourceMapConsumer(rawSourceMap, sourceMapUrl);
    try {
      return await f(consumer);
    } finally {
      consumer.destroy();
    }
  }

  /**
   * Iterate over each mapping between an original source/line/column and a
   * generated line/column in this source map.
   *
   * @param Function aCallback
   *        The function that is called with each mapping.
   * @param Object aContext
   *        Optional. If specified, this object will be the value of `this` every
   *        time that `aCallback` is called.
   * @param aOrder
   *        Either `SourceMapConsumer.GENERATED_ORDER` or
   *        `SourceMapConsumer.ORIGINAL_ORDER`. Specifies whether you want to
   *        iterate over the mappings sorted by the generated file's line/column
   *        order or the original's source/line/column order, respectively. Defaults to
   *        `SourceMapConsumer.GENERATED_ORDER`.
   */
  eachMapping(aCallback, aContext, aOrder) {
    throw new Error("Subclasses must implement eachMapping");
  }

  /**
   * Returns all generated line and column information for the original source,
   * line, and column provided. If no column is provided, returns all mappings
   * corresponding to a either the line we are searching for or the next
   * closest line that has any mappings. Otherwise, returns all mappings
   * corresponding to the given line and either the column we are searching for
   * or the next closest column that has any offsets.
   *
   * The only argument is an object with the following properties:
   *
   *   - source: The filename of the original source.
   *   - line: The line number in the original source.  The line number is 1-based.
   *   - column: Optional. the column number in the original source.
   *    The column number is 0-based.
   *
   * and an array of objects is returned, each with the following properties:
   *
   *   - line: The line number in the generated source, or null.  The
   *    line number is 1-based.
   *   - column: The column number in the generated source, or null.
   *    The column number is 0-based.
   */
  allGeneratedPositionsFor(aArgs) {
    throw new Error("Subclasses must implement allGeneratedPositionsFor");
  }

  destroy() {
    throw new Error("Subclasses must implement destroy");
  }
}

/**
 * The version of the source mapping spec that we are consuming.
 */
SourceMapConsumer.prototype._version = 3;
SourceMapConsumer.GENERATED_ORDER = 1;
SourceMapConsumer.ORIGINAL_ORDER = 2;

SourceMapConsumer.GREATEST_LOWER_BOUND = 1;
SourceMapConsumer.LEAST_UPPER_BOUND = 2;

exports.SourceMapConsumer = SourceMapConsumer;

/**
 * A BasicSourceMapConsumer instance represents a parsed source map which we can
 * query for information about the original file positions by giving it a file
 * position in the generated source.
 *
 * The first parameter is the raw source map (either as a JSON string, or
 * already parsed to an object). According to the spec, source maps have the
 * following attributes:
 *
 *   - version: Which version of the source map spec this map is following.
 *   - sources: An array of URLs to the original source files.
 *   - names: An array of identifiers which can be referenced by individual mappings.
 *   - sourceRoot: Optional. The URL root from which all sources are relative.
 *   - sourcesContent: Optional. An array of contents of the original source files.
 *   - mappings: A string of base64 VLQs which contain the actual mappings.
 *   - file: Optional. The generated file this source map is associated with.
 *
 * Here is an example source map, taken from the source map spec[0]:
 *
 *     {
 *       version : 3,
 *       file: "out.js",
 *       sourceRoot : "",
 *       sources: ["foo.js", "bar.js"],
 *       names: ["src", "maps", "are", "fun"],
 *       mappings: "AA,AB;;ABCDE;"
 *     }
 *
 * The second parameter, if given, is a string whose value is the URL
 * at which the source map was found.  This URL is used to compute the
 * sources array.
 *
 * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit?pli=1#
 */
class BasicSourceMapConsumer extends SourceMapConsumer {
  constructor(aSourceMap, aSourceMapURL) {
    return super(INTERNAL).then(that => {
      let sourceMap = aSourceMap;
      if (typeof aSourceMap === "string") {
        sourceMap = util.parseSourceMapInput(aSourceMap);
      }

      const version = util.getArg(sourceMap, "version");
      const sources = util.getArg(sourceMap, "sources").map(String);
      // Sass 3.3 leaves out the 'names' array, so we deviate from the spec (which
      // requires the array) to play nice here.
      const names = util.getArg(sourceMap, "names", []);
      const sourceRoot = util.getArg(sourceMap, "sourceRoot", null);
      const sourcesContent = util.getArg(sourceMap, "sourcesContent", null);
      const mappings = util.getArg(sourceMap, "mappings");
      const file = util.getArg(sourceMap, "file", null);

      // Once again, Sass deviates from the spec and supplies the version as a
      // string rather than a number, so we use loose equality checking here.
      if (version != that._version) {
        throw new Error("Unsupported version: " + version);
      }

      that._sourceLookupCache = new Map();

      // Pass `true` below to allow duplicate names and sources. While source maps
      // are intended to be compressed and deduplicated, the TypeScript compiler
      // sometimes generates source maps with duplicates in them. See Github issue
      // #72 and bugzil.la/889492.
      that._names = ArraySet.fromArray(names.map(String), true);
      that._sources = ArraySet.fromArray(sources, true);

      that._absoluteSources = ArraySet.fromArray(that._sources.toArray().map(function(s) {
        return util.computeSourceURL(sourceRoot, s, aSourceMapURL);
      }), true);

      that.sourceRoot = sourceRoot;
      that.sourcesContent = sourcesContent;
      that._mappings = mappings;
      that._sourceMapURL = aSourceMapURL;
      that.file = file;

      that._computedColumnSpans = false;
      that._mappingsPtr = 0;
      that._wasm = null;

      return wasm().then(w => {
        that._wasm = w;
        return that;
      });
    });
  }

  /**
   * Utility function to find the index of a source.  Returns -1 if not
   * found.
   */
  _findSourceIndex(aSource) {
    // In the most common usecases, we'll be constantly looking up the index for the same source
    // files, so we cache the index lookup to avoid constantly recomputing the full URLs.
    const cachedIndex = this._sourceLookupCache.get(aSource);
    if (typeof cachedIndex === "number") {
      return cachedIndex;
    }

    // Treat the source as map-relative overall by default.
    const sourceAsMapRelative = util.computeSourceURL(null, aSource, this._sourceMapURL);
    if (this._absoluteSources.has(sourceAsMapRelative)) {
      const index = this._absoluteSources.indexOf(sourceAsMapRelative);
      this._sourceLookupCache.set(aSource, index);
      return index;
    }

    // Fall back to treating the source as sourceRoot-relative.
    const sourceAsSourceRootRelative = util.computeSourceURL(this.sourceRoot, aSource, this._sourceMapURL);
    if (this._absoluteSources.has(sourceAsSourceRootRelative)) {
      const index = this._absoluteSources.indexOf(sourceAsSourceRootRelative);
      this._sourceLookupCache.set(aSource, index);
      return index;
    }

    // To avoid this cache growing forever, we do not cache lookup misses.
    return -1;
  }

  /**
   * Create a BasicSourceMapConsumer from a SourceMapGenerator.
   *
   * @param SourceMapGenerator aSourceMap
   *        The source map that will be consumed.
   * @param String aSourceMapURL
   *        The URL at which the source map can be found (optional)
   * @returns BasicSourceMapConsumer
   */
  static fromSourceMap(aSourceMap, aSourceMapURL) {
    return new BasicSourceMapConsumer(aSourceMap.toString());
  }

  get sources() {
    return this._absoluteSources.toArray();
  }

  _getMappingsPtr() {
    if (this._mappingsPtr === 0) {
      this._parseMappings();
    }

    return this._mappingsPtr;
  }

  /**
   * Parse the mappings in a string in to a data structure which we can easily
   * query (the ordered arrays in the `this.__generatedMappings` and
   * `this.__originalMappings` properties).
   */
  _parseMappings() {
    const aStr = this._mappings;
    const size = aStr.length;

    const mappingsBufPtr = this._wasm.exports.allocate_mappings(size);
    const mappingsBuf = new Uint8Array(this._wasm.exports.memory.buffer, mappingsBufPtr, size);
    for (let i = 0; i < size; i++) {
      mappingsBuf[i] = aStr.charCodeAt(i);
    }

    const mappingsPtr = this._wasm.exports.parse_mappings(mappingsBufPtr);

    if (!mappingsPtr) {
      const error = this._wasm.exports.get_last_error();
      let msg = `Error parsing mappings (code ${error}): `;

      // XXX: keep these error codes in sync with `fitzgen/source-map-mappings`.
      switch (error) {
        case 1:
          msg += "the mappings contained a negative line, column, source index, or name index";
          break;
        case 2:
          msg += "the mappings contained a number larger than 2**32";
          break;
        case 3:
          msg += "reached EOF while in the middle of parsing a VLQ";
          break;
        case 4:
          msg += "invalid base 64 character while parsing a VLQ";
          break;
        default:
          msg += "unknown error code";
          break;
      }

      throw new Error(msg);
    }

    this._mappingsPtr = mappingsPtr;
  }

  eachMapping(aCallback, aContext, aOrder) {
    const context = aContext || null;
    const order = aOrder || SourceMapConsumer.GENERATED_ORDER;

    this._wasm.withMappingCallback(
      mapping => {
        if (mapping.source !== null) {
          mapping.source = this._absoluteSources.at(mapping.source);

          if (mapping.name !== null) {
            mapping.name = this._names.at(mapping.name);
          }
        }
        if (this._computedColumnSpans && mapping.lastGeneratedColumn === null) {
          mapping.lastGeneratedColumn = Infinity;
        }

        aCallback.call(context, mapping);
      },
      () => {
        switch (order) {
        case SourceMapConsumer.GENERATED_ORDER:
          this._wasm.exports.by_generated_location(this._getMappingsPtr());
          break;
        case SourceMapConsumer.ORIGINAL_ORDER:
          this._wasm.exports.by_original_location(this._getMappingsPtr());
          break;
        default:
          throw new Error("Unknown order of iteration.");
        }
      }
    );
  }

  allGeneratedPositionsFor(aArgs) {
    let source = util.getArg(aArgs, "source");
    const originalLine = util.getArg(aArgs, "line");
    const originalColumn = aArgs.column || 0;

    source = this._findSourceIndex(source);
    if (source < 0) {
      return [];
    }

    if (originalLine < 1) {
      throw new Error("Line numbers must be >= 1");
    }

    if (originalColumn < 0) {
      throw new Error("Column numbers must be >= 0");
    }

    const mappings = [];

    this._wasm.withMappingCallback(
      m => {
        let lastColumn = m.lastGeneratedColumn;
        if (this._computedColumnSpans && lastColumn === null) {
          lastColumn = Infinity;
        }
        mappings.push({
          line: m.generatedLine,
          column: m.generatedColumn,
          lastColumn,
        });
      }, () => {
        this._wasm.exports.all_generated_locations_for(
          this._getMappingsPtr(),
          source,
          originalLine - 1,
          "column" in aArgs,
          originalColumn
        );
      }
    );

    return mappings;
  }

  destroy() {
    if (this._mappingsPtr !== 0) {
      this._wasm.exports.free_mappings(this._mappingsPtr);
      this._mappingsPtr = 0;
    }
  }

  /**
   * Compute the last column for each generated mapping. The last column is
   * inclusive.
   */
  computeColumnSpans() {
    if (this._computedColumnSpans) {
      return;
    }

    this._wasm.exports.compute_column_spans(this._getMappingsPtr());
    this._computedColumnSpans = true;
  }

  /**
   * Returns the original source, line, and column information for the generated
   * source's line and column positions provided. The only argument is an object
   * with the following properties:
   *
   *   - line: The line number in the generated source.  The line number
   *     is 1-based.
   *   - column: The column number in the generated source.  The column
   *     number is 0-based.
   *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
   *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
   *     closest element that is smaller than or greater than the one we are
   *     searching for, respectively, if the exact element cannot be found.
   *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
   *
   * and an object is returned with the following properties:
   *
   *   - source: The original source file, or null.
   *   - line: The line number in the original source, or null.  The
   *     line number is 1-based.
   *   - column: The column number in the original source, or null.  The
   *     column number is 0-based.
   *   - name: The original identifier, or null.
   */
  originalPositionFor(aArgs) {
    const needle = {
      generatedLine: util.getArg(aArgs, "line"),
      generatedColumn: util.getArg(aArgs, "column")
    };

    if (needle.generatedLine < 1) {
      throw new Error("Line numbers must be >= 1");
    }

    if (needle.generatedColumn < 0) {
      throw new Error("Column numbers must be >= 0");
    }

    let bias = util.getArg(aArgs, "bias", SourceMapConsumer.GREATEST_LOWER_BOUND);
    if (bias == null) {
      bias = SourceMapConsumer.GREATEST_LOWER_BOUND;
    }

    let mapping;
    this._wasm.withMappingCallback(m => mapping = m, () => {
      this._wasm.exports.original_location_for(
        this._getMappingsPtr(),
        needle.generatedLine - 1,
        needle.generatedColumn,
        bias
      );
    });

    if (mapping) {
      if (mapping.generatedLine === needle.generatedLine) {
        let source = util.getArg(mapping, "source", null);
        if (source !== null) {
          source = this._absoluteSources.at(source);
        }

        let name = util.getArg(mapping, "name", null);
        if (name !== null) {
          name = this._names.at(name);
        }

        return {
          source,
          line: util.getArg(mapping, "originalLine", null),
          column: util.getArg(mapping, "originalColumn", null),
          name
        };
      }
    }

    return {
      source: null,
      line: null,
      column: null,
      name: null
    };
  }

  /**
   * Return true if we have the source content for every source in the source
   * map, false otherwise.
   */
  hasContentsOfAllSources() {
    if (!this.sourcesContent) {
      return false;
    }
    return this.sourcesContent.length >= this._sources.size() &&
      !this.sourcesContent.some(function(sc) { return sc == null; });
  }

  /**
   * Returns the original source content. The only argument is the url of the
   * original source file. Returns null if no original source content is
   * available.
   */
  sourceContentFor(aSource, nullOnMissing) {
    if (!this.sourcesContent) {
      return null;
    }

    const index = this._findSourceIndex(aSource);
    if (index >= 0) {
      return this.sourcesContent[index];
    }

    // This function is used recursively from
    // IndexedSourceMapConsumer.prototype.sourceContentFor. In that case, we
    // don't want to throw if we can't find the source - we just want to
    // return null, so we provide a flag to exit gracefully.
    if (nullOnMissing) {
      return null;
    }

    throw new Error('"' + aSource + '" is not in the SourceMap.');
  }

  /**
   * Returns the generated line and column information for the original source,
   * line, and column positions provided. The only argument is an object with
   * the following properties:
   *
   *   - source: The filename of the original source.
   *   - line: The line number in the original source.  The line number
   *     is 1-based.
   *   - column: The column number in the original source.  The column
   *     number is 0-based.
   *   - bias: Either 'SourceMapConsumer.GREATEST_LOWER_BOUND' or
   *     'SourceMapConsumer.LEAST_UPPER_BOUND'. Specifies whether to return the
   *     closest element that is smaller than or greater than the one we are
   *     searching for, respectively, if the exact element cannot be found.
   *     Defaults to 'SourceMapConsumer.GREATEST_LOWER_BOUND'.
   *
   * and an object is returned with the following properties:
   *
   *   - line: The line number in the generated source, or null.  The
   *     line number is 1-based.
   *   - column: The column number in the generated source, or null.
   *     The column number is 0-based.
   */
  generatedPositionFor(aArgs) {
    let source = util.getArg(aArgs, "source");
    source = this._findSourceIndex(source);
    if (source < 0) {
      return {
        line: null,
        column: null,
        lastColumn: null
      };
    }

    const needle = {
      source,
      originalLine: util.getArg(aArgs, "line"),
      originalColumn: util.getArg(aArgs, "column")
    };

    if (needle.originalLine < 1) {
      throw new Error("Line numbers must be >= 1");
    }

    if (needle.originalColumn < 0) {
      throw new Error("Column numbers must be >= 0");
    }

    let bias = util.getArg(aArgs, "bias", SourceMapConsumer.GREATEST_LOWER_BOUND);
    if (bias == null) {
      bias = SourceMapConsumer.GREATEST_LOWER_BOUND;
    }

    let mapping;
    this._wasm.withMappingCallback(m => mapping = m, () => {
      this._wasm.exports.generated_location_for(
        this._getMappingsPtr(),
        needle.source,
        needle.originalLine - 1,
        needle.originalColumn,
        bias
      );
    });

    if (mapping) {
      if (mapping.source === needle.source) {
        let lastColumn = mapping.lastGeneratedColumn;
        if (this._computedColumnSpans && lastColumn === null) {
          lastColumn = Infinity;
        }
        return {
          line: util.getArg(mapping, "generatedLine", null),
          column: util.getArg(mapping, "generatedColumn", null),
          lastColumn,
        };
      }
    }

    return {
      line: null,
      column: null,
      lastColumn: null
    };
  }
}

BasicSourceMapConsumer.prototype.consumer = SourceMapConsumer;
exports.BasicSourceMapConsumer = BasicSourceMapConsumer;

/**
 * An IndexedSourceMapConsumer instance represents a parsed source map which
 * we can query for information. It differs from BasicSourceMapConsumer in
 * that it takes "indexed" source maps (i.e. ones with a "sections" field) as
 * input.
 *
 * The first parameter is a raw source map (either as a JSON string, or already
 * parsed to an object). According to the spec for indexed source maps, they
 * have the following attributes:
 *
 *   - version: Which version of the source map spec this map is following.
 *   - file: Optional. The generated file this source map is associated with.
 *   - sections: A list of section definitions.
 *
 * Each value under the "sections" field has two fields:
 *   - offset: The offset into the original specified at which this section
 *       begins to apply, defined as an object with a "line" and "column"
 *       field.
 *   - map: A source map definition. This source map could also be indexed,
 *       but doesn't have to be.
 *
 * Instead of the "map" field, it's also possible to have a "url" field
 * specifying a URL to retrieve a source map from, but that's currently
 * unsupported.
 *
 * Here's an example source map, taken from the source map spec[0], but
 * modified to omit a section which uses the "url" field.
 *
 *  {
 *    version : 3,
 *    file: "app.js",
 *    sections: [{
 *      offset: {line:100, column:10},
 *      map: {
 *        version : 3,
 *        file: "section.js",
 *        sources: ["foo.js", "bar.js"],
 *        names: ["src", "maps", "are", "fun"],
 *        mappings: "AAAA,E;;ABCDE;"
 *      }
 *    }],
 *  }
 *
 * The second parameter, if given, is a string whose value is the URL
 * at which the source map was found.  This URL is used to compute the
 * sources array.
 *
 * [0]: https://docs.google.com/document/d/1U1RGAehQwRypUTovF1KRlpiOFze0b-_2gc6fAH0KY0k/edit#heading=h.535es3xeprgt
 */
class IndexedSourceMapConsumer extends SourceMapConsumer {
  constructor(aSourceMap, aSourceMapURL) {
    return super(INTERNAL).then(that => {
      let sourceMap = aSourceMap;
      if (typeof aSourceMap === "string") {
        sourceMap = util.parseSourceMapInput(aSourceMap);
      }

      const version = util.getArg(sourceMap, "version");
      const sections = util.getArg(sourceMap, "sections");

      if (version != that._version) {
        throw new Error("Unsupported version: " + version);
      }

      let lastOffset = {
        line: -1,
        column: 0
      };
      return Promise.all(sections.map(s => {
        if (s.url) {
          // The url field will require support for asynchronicity.
          // See https://github.com/mozilla/source-map/issues/16
          throw new Error("Support for url field in sections not implemented.");
        }
        const offset = util.getArg(s, "offset");
        const offsetLine = util.getArg(offset, "line");
        const offsetColumn = util.getArg(offset, "column");

        if (offsetLine < lastOffset.line ||
            (offsetLine === lastOffset.line && offsetColumn < lastOffset.column)) {
          throw new Error("Section offsets must be ordered and non-overlapping.");
        }
        lastOffset = offset;

        const cons = new SourceMapConsumer(util.getArg(s, "map"), aSourceMapURL);
        return cons.then(consumer => {
          return {
            generatedOffset: {
              // The offset fields are 0-based, but we use 1-based indices when
              // encoding/decoding from VLQ.
              generatedLine: offsetLine + 1,
              generatedColumn: offsetColumn + 1
            },
            consumer
          };
        });
      })).then(s => {
        that._sections = s;
        return that;
      });
    });
  }

  /**
   * The list of original sources.
   */
  get sources() {
    const sources = [];
    for (let i = 0; i < this._sections.length; i++) {
      for (let j = 0; j < this._sections[i].consumer.sources.length; j++) {
        sources.push(this._sections[i].consumer.sources[j]);
      }
    }
    return sources;
  }

  /**
   * Returns the original source, line, and column information for the generated
   * source's line and column positions provided. The only argument is an object
   * with the following properties:
   *
   *   - line: The line number in the generated source.  The line number
   *     is 1-based.
   *   - column: The column number in the generated source.  The column
   *     number is 0-based.
   *
   * and an object is returned with the following properties:
   *
   *   - source: The original source file, or null.
   *   - line: The line number in the original source, or null.  The
   *     line number is 1-based.
   *   - column: The column number in the original source, or null.  The
   *     column number is 0-based.
   *   - name: The original identifier, or null.
   */
  originalPositionFor(aArgs) {
    const needle = {
      generatedLine: util.getArg(aArgs, "line"),
      generatedColumn: util.getArg(aArgs, "column")
    };

    // Find the section containing the generated position we're trying to map
    // to an original position.
    const sectionIndex = binarySearch.search(needle, this._sections,
      function(aNeedle, section) {
        const cmp = aNeedle.generatedLine - section.generatedOffset.generatedLine;
        if (cmp) {
          return cmp;
        }

        return (aNeedle.generatedColumn -
                section.generatedOffset.generatedColumn);
      });
    const section = this._sections[sectionIndex];

    if (!section) {
      return {
        source: null,
        line: null,
        column: null,
        name: null
      };
    }

    return section.consumer.originalPositionFor({
      line: needle.generatedLine -
        (section.generatedOffset.generatedLine - 1),
      column: needle.generatedColumn -
        (section.generatedOffset.generatedLine === needle.generatedLine
         ? section.generatedOffset.generatedColumn - 1
         : 0),
      bias: aArgs.bias
    });
  }

  /**
   * Return true if we have the source content for every source in the source
   * map, false otherwise.
   */
  hasContentsOfAllSources() {
    return this._sections.every(function(s) {
      return s.consumer.hasContentsOfAllSources();
    });
  }

  /**
   * Returns the original source content. The only argument is the url of the
   * original source file. Returns null if no original source content is
   * available.
   */
  sourceContentFor(aSource, nullOnMissing) {
    for (let i = 0; i < this._sections.length; i++) {
      const section = this._sections[i];

      const content = section.consumer.sourceContentFor(aSource, true);
      if (content) {
        return content;
      }
    }
    if (nullOnMissing) {
      return null;
    }
    throw new Error('"' + aSource + '" is not in the SourceMap.');
  }

  _findSectionIndex(source) {
    for (let i = 0; i < this._sections.length; i++) {
      const { consumer } = this._sections[i];
      if (consumer._findSourceIndex(source) !== -1) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Returns the generated line and column information for the original source,
   * line, and column positions provided. The only argument is an object with
   * the following properties:
   *
   *   - source: The filename of the original source.
   *   - line: The line number in the original source.  The line number
   *     is 1-based.
   *   - column: The column number in the original source.  The column
   *     number is 0-based.
   *
   * and an object is returned with the following properties:
   *
   *   - line: The line number in the generated source, or null.  The
   *     line number is 1-based.
   *   - column: The column number in the generated source, or null.
   *     The column number is 0-based.
   */
  generatedPositionFor(aArgs) {
    const index = this._findSectionIndex(util.getArg(aArgs, "source"));
    const section = index >= 0 ? this._sections[index] : null;
    const nextSection =
      index >= 0 && index + 1 < this._sections.length
        ? this._sections[index + 1]
        : null;

    const generatedPosition =
      section && section.consumer.generatedPositionFor(aArgs);
    if (generatedPosition && generatedPosition.line !== null) {
      const lineShift = section.generatedOffset.generatedLine - 1;
      const columnShift = section.generatedOffset.generatedColumn - 1;

      if (generatedPosition.line === 1) {
        generatedPosition.column += columnShift;
        if (typeof generatedPosition.lastColumn === "number") {
          generatedPosition.lastColumn += columnShift;
        }
      }

      if (
        generatedPosition.lastColumn === Infinity &&
        nextSection &&
        generatedPosition.line === nextSection.generatedOffset.generatedLine
      ) {
        generatedPosition.lastColumn =
          nextSection.generatedOffset.generatedColumn - 2;
      }
      generatedPosition.line += lineShift;

      return generatedPosition;
    }

    return {
      line: null,
      column: null,
      lastColumn: null
    };
  }

  allGeneratedPositionsFor(aArgs) {
    const index = this._findSectionIndex(util.getArg(aArgs, "source"));
    const section = index >= 0 ? this._sections[index] : null;
    const nextSection =
      index >= 0 && index + 1 < this._sections.length
        ? this._sections[index + 1]
        : null;

    if (!section) return [];

    return section.consumer.allGeneratedPositionsFor(aArgs).map(
      generatedPosition => {
        const lineShift = section.generatedOffset.generatedLine - 1;
        const columnShift = section.generatedOffset.generatedColumn - 1;

        if (generatedPosition.line === 1) {
          generatedPosition.column += columnShift;
          if (typeof generatedPosition.lastColumn === "number") {
            generatedPosition.lastColumn += columnShift;
          }
        }

        if (
          generatedPosition.lastColumn === Infinity &&
          nextSection &&
          generatedPosition.line === nextSection.generatedOffset.generatedLine
        ) {
          generatedPosition.lastColumn =
            nextSection.generatedOffset.generatedColumn - 2;
        }
        generatedPosition.line += lineShift;

        return generatedPosition;
      }
    );
  }

  eachMapping(aCallback, aContext, aOrder) {
    this._sections.forEach((section, index) => {
      const nextSection =
        index + 1 < this._sections.length
          ? this._sections[index + 1]
          : null;
      const { generatedOffset } = section;

      const lineShift = generatedOffset.generatedLine - 1;
      const columnShift = generatedOffset.generatedColumn - 1;

      section.consumer.eachMapping(function(mapping) {
        if (mapping.generatedLine === 1) {
          mapping.generatedColumn += columnShift;

          if (typeof mapping.lastGeneratedColumn === "number") {
            mapping.lastGeneratedColumn += columnShift;
          }
        }

        if (
          mapping.lastGeneratedColumn === Infinity &&
          nextSection &&
          mapping.generatedLine === nextSection.generatedOffset.generatedLine
        ) {
          mapping.lastGeneratedColumn =
            nextSection.generatedOffset.generatedColumn - 2;
        }
        mapping.generatedLine += lineShift;

        aCallback.call(this, mapping);
      }, aContext, aOrder);
    });
  }

  computeColumnSpans() {
    for (let i = 0; i < this._sections.length; i++) {
      this._sections[i].consumer.computeColumnSpans();
    }
  }

  destroy() {
    for (let i = 0; i < this._sections.length; i++) {
      this._sections[i].consumer.destroy();
    }
  }
}
exports.IndexedSourceMapConsumer = IndexedSourceMapConsumer;

/*
 * Cheat to get around inter-twingled classes.  `factory()` can be at the end
 * where it has access to non-hoisted classes, but it gets hoisted itself.
 */
function _factory(aSourceMap, aSourceMapURL) {
  let sourceMap = aSourceMap;
  if (typeof aSourceMap === "string") {
    sourceMap = util.parseSourceMapInput(aSourceMap);
  }

  const consumer = sourceMap.sections != null
      ? new IndexedSourceMapConsumer(sourceMap, aSourceMapURL)
      : new BasicSourceMapConsumer(sourceMap, aSourceMapURL);
  return Promise.resolve(consumer);
}

function _factoryBSM(aSourceMap, aSourceMapURL) {
  return BasicSourceMapConsumer.fromSourceMap(aSourceMap, aSourceMapURL);
}


/***/ }),

/***/ 404:
/***/ (function(module, exports) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

exports.GREATEST_LOWER_BOUND = 1;
exports.LEAST_UPPER_BOUND = 2;

/**
 * Recursive implementation of binary search.
 *
 * @param aLow Indices here and lower do not contain the needle.
 * @param aHigh Indices here and higher do not contain the needle.
 * @param aNeedle The element being searched for.
 * @param aHaystack The non-empty array being searched.
 * @param aCompare Function which takes two elements and returns -1, 0, or 1.
 * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
 *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
 *     closest element that is smaller than or greater than the one we are
 *     searching for, respectively, if the exact element cannot be found.
 */
function recursiveSearch(aLow, aHigh, aNeedle, aHaystack, aCompare, aBias) {
  // This function terminates when one of the following is true:
  //
  //   1. We find the exact element we are looking for.
  //
  //   2. We did not find the exact element, but we can return the index of
  //      the next-closest element.
  //
  //   3. We did not find the exact element, and there is no next-closest
  //      element than the one we are searching for, so we return -1.
  const mid = Math.floor((aHigh - aLow) / 2) + aLow;
  const cmp = aCompare(aNeedle, aHaystack[mid], true);
  if (cmp === 0) {
    // Found the element we are looking for.
    return mid;
  } else if (cmp > 0) {
    // Our needle is greater than aHaystack[mid].
    if (aHigh - mid > 1) {
      // The element is in the upper half.
      return recursiveSearch(mid, aHigh, aNeedle, aHaystack, aCompare, aBias);
    }

    // The exact needle element was not found in this haystack. Determine if
    // we are in termination case (3) or (2) and return the appropriate thing.
    if (aBias == exports.LEAST_UPPER_BOUND) {
      return aHigh < aHaystack.length ? aHigh : -1;
    }
    return mid;
  }

  // Our needle is less than aHaystack[mid].
  if (mid - aLow > 1) {
    // The element is in the lower half.
    return recursiveSearch(aLow, mid, aNeedle, aHaystack, aCompare, aBias);
  }

  // we are in termination case (3) or (2) and return the appropriate thing.
  if (aBias == exports.LEAST_UPPER_BOUND) {
    return mid;
  }
  return aLow < 0 ? -1 : aLow;
}

/**
 * This is an implementation of binary search which will always try and return
 * the index of the closest element if there is no exact hit. This is because
 * mappings between original and generated line/col pairs are single points,
 * and there is an implicit region between each of them, so a miss just means
 * that you aren't on the very start of a region.
 *
 * @param aNeedle The element you are looking for.
 * @param aHaystack The array that is being searched.
 * @param aCompare A function which takes the needle and an element in the
 *     array and returns -1, 0, or 1 depending on whether the needle is less
 *     than, equal to, or greater than the element, respectively.
 * @param aBias Either 'binarySearch.GREATEST_LOWER_BOUND' or
 *     'binarySearch.LEAST_UPPER_BOUND'. Specifies whether to return the
 *     closest element that is smaller than or greater than the one we are
 *     searching for, respectively, if the exact element cannot be found.
 *     Defaults to 'binarySearch.GREATEST_LOWER_BOUND'.
 */
exports.search = function search(aNeedle, aHaystack, aCompare, aBias) {
  if (aHaystack.length === 0) {
    return -1;
  }

  let index = recursiveSearch(-1, aHaystack.length, aNeedle, aHaystack,
                              aCompare, aBias || exports.GREATEST_LOWER_BOUND);
  if (index < 0) {
    return -1;
  }

  // We have found either the exact element, or the next-closest element than
  // the one we are searching for. However, there may be more than one such
  // element. Make sure we always return the smallest of these.
  while (index - 1 >= 0) {
    if (aCompare(aHaystack[index], aHaystack[index - 1], true) !== 0) {
      break;
    }
    --index;
  }

  return index;
};


/***/ }),

/***/ 405:
/***/ (function(module, exports, __webpack_require__) {

const readWasm = __webpack_require__(178);

/**
 * Provide the JIT with a nice shape / hidden class.
 */
function Mapping() {
  this.generatedLine = 0;
  this.generatedColumn = 0;
  this.lastGeneratedColumn = null;
  this.source = null;
  this.originalLine = null;
  this.originalColumn = null;
  this.name = null;
}

let cachedWasm = null;

module.exports = function wasm() {
  if (cachedWasm) {
    return cachedWasm;
  }

  const callbackStack = [];

  cachedWasm = readWasm().then(buffer => {
      return WebAssembly.instantiate(buffer, {
        env: {
          mapping_callback(
            generatedLine,
            generatedColumn,

            hasLastGeneratedColumn,
            lastGeneratedColumn,

            hasOriginal,
            source,
            originalLine,
            originalColumn,

            hasName,
            name
          ) {
            const mapping = new Mapping();
            // JS uses 1-based line numbers, wasm uses 0-based.
            mapping.generatedLine = generatedLine + 1;
            mapping.generatedColumn = generatedColumn;

            if (hasLastGeneratedColumn) {
              // JS uses inclusive last generated column, wasm uses exclusive.
              mapping.lastGeneratedColumn = lastGeneratedColumn - 1;
            }

            if (hasOriginal) {
              mapping.source = source;
              // JS uses 1-based line numbers, wasm uses 0-based.
              mapping.originalLine = originalLine + 1;
              mapping.originalColumn = originalColumn;

              if (hasName) {
                mapping.name = name;
              }
            }

            callbackStack[callbackStack.length - 1](mapping);
          },

          start_all_generated_locations_for() { console.time("all_generated_locations_for"); },
          end_all_generated_locations_for() { console.timeEnd("all_generated_locations_for"); },

          start_compute_column_spans() { console.time("compute_column_spans"); },
          end_compute_column_spans() { console.timeEnd("compute_column_spans"); },

          start_generated_location_for() { console.time("generated_location_for"); },
          end_generated_location_for() { console.timeEnd("generated_location_for"); },

          start_original_location_for() { console.time("original_location_for"); },
          end_original_location_for() { console.timeEnd("original_location_for"); },

          start_parse_mappings() { console.time("parse_mappings"); },
          end_parse_mappings() { console.timeEnd("parse_mappings"); },

          start_sort_by_generated_location() { console.time("sort_by_generated_location"); },
          end_sort_by_generated_location() { console.timeEnd("sort_by_generated_location"); },

          start_sort_by_original_location() { console.time("sort_by_original_location"); },
          end_sort_by_original_location() { console.timeEnd("sort_by_original_location"); },
        }
      });
  }).then(Wasm => {
    return {
      exports: Wasm.instance.exports,
      withMappingCallback: (mappingCallback, f) => {
        callbackStack.push(mappingCallback);
        try {
          f();
        } finally {
          callbackStack.pop();
        }
      }
    };
  }).then(null, e => {
    cachedWasm = null;
    throw e;
  });

  return cachedWasm;
};


/***/ }),

/***/ 406:
/***/ (function(module, exports, __webpack_require__) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

const SourceMapGenerator = __webpack_require__(171).SourceMapGenerator;
const util = __webpack_require__(61);

// Matches a Windows-style `\r\n` newline or a `\n` newline used by all other
// operating systems these days (capturing the result).
const REGEX_NEWLINE = /(\r?\n)/;

// Newline character code for charCodeAt() comparisons
const NEWLINE_CODE = 10;

// Private symbol for identifying `SourceNode`s when multiple versions of
// the source-map library are loaded. This MUST NOT CHANGE across
// versions!
const isSourceNode = "$$$isSourceNode$$$";

/**
 * SourceNodes provide a way to abstract over interpolating/concatenating
 * snippets of generated JavaScript source code while maintaining the line and
 * column information associated with the original source code.
 *
 * @param aLine The original line number.
 * @param aColumn The original column number.
 * @param aSource The original source's filename.
 * @param aChunks Optional. An array of strings which are snippets of
 *        generated JS, or other SourceNodes.
 * @param aName The original identifier.
 */
class SourceNode {
  constructor(aLine, aColumn, aSource, aChunks, aName) {
    this.children = [];
    this.sourceContents = {};
    this.line = aLine == null ? null : aLine;
    this.column = aColumn == null ? null : aColumn;
    this.source = aSource == null ? null : aSource;
    this.name = aName == null ? null : aName;
    this[isSourceNode] = true;
    if (aChunks != null) this.add(aChunks);
  }

  /**
   * Creates a SourceNode from generated code and a SourceMapConsumer.
   *
   * @param aGeneratedCode The generated code
   * @param aSourceMapConsumer The SourceMap for the generated code
   * @param aRelativePath Optional. The path that relative sources in the
   *        SourceMapConsumer should be relative to.
   */
  static fromStringWithSourceMap(aGeneratedCode, aSourceMapConsumer, aRelativePath) {
    // The SourceNode we want to fill with the generated code
    // and the SourceMap
    const node = new SourceNode();

    // All even indices of this array are one line of the generated code,
    // while all odd indices are the newlines between two adjacent lines
    // (since `REGEX_NEWLINE` captures its match).
    // Processed fragments are accessed by calling `shiftNextLine`.
    const remainingLines = aGeneratedCode.split(REGEX_NEWLINE);
    let remainingLinesIndex = 0;
    const shiftNextLine = function() {
      const lineContents = getNextLine();
      // The last line of a file might not have a newline.
      const newLine = getNextLine() || "";
      return lineContents + newLine;

      function getNextLine() {
        return remainingLinesIndex < remainingLines.length ?
            remainingLines[remainingLinesIndex++] : undefined;
      }
    };

    // We need to remember the position of "remainingLines"
    let lastGeneratedLine = 1, lastGeneratedColumn = 0;

    // The generate SourceNodes we need a code range.
    // To extract it current and last mapping is used.
    // Here we store the last mapping.
    let lastMapping = null;
    let nextLine;

    aSourceMapConsumer.eachMapping(function(mapping) {
      if (lastMapping !== null) {
        // We add the code from "lastMapping" to "mapping":
        // First check if there is a new line in between.
        if (lastGeneratedLine < mapping.generatedLine) {
          // Associate first line with "lastMapping"
          addMappingWithCode(lastMapping, shiftNextLine());
          lastGeneratedLine++;
          lastGeneratedColumn = 0;
          // The remaining code is added without mapping
        } else {
          // There is no new line in between.
          // Associate the code between "lastGeneratedColumn" and
          // "mapping.generatedColumn" with "lastMapping"
          nextLine = remainingLines[remainingLinesIndex] || "";
          const code = nextLine.substr(0, mapping.generatedColumn -
                                        lastGeneratedColumn);
          remainingLines[remainingLinesIndex] = nextLine.substr(mapping.generatedColumn -
                                              lastGeneratedColumn);
          lastGeneratedColumn = mapping.generatedColumn;
          addMappingWithCode(lastMapping, code);
          // No more remaining code, continue
          lastMapping = mapping;
          return;
        }
      }
      // We add the generated code until the first mapping
      // to the SourceNode without any mapping.
      // Each line is added as separate string.
      while (lastGeneratedLine < mapping.generatedLine) {
        node.add(shiftNextLine());
        lastGeneratedLine++;
      }
      if (lastGeneratedColumn < mapping.generatedColumn) {
        nextLine = remainingLines[remainingLinesIndex] || "";
        node.add(nextLine.substr(0, mapping.generatedColumn));
        remainingLines[remainingLinesIndex] = nextLine.substr(mapping.generatedColumn);
        lastGeneratedColumn = mapping.generatedColumn;
      }
      lastMapping = mapping;
    }, this);
    // We have processed all mappings.
    if (remainingLinesIndex < remainingLines.length) {
      if (lastMapping) {
        // Associate the remaining code in the current line with "lastMapping"
        addMappingWithCode(lastMapping, shiftNextLine());
      }
      // and add the remaining lines without any mapping
      node.add(remainingLines.splice(remainingLinesIndex).join(""));
    }

    // Copy sourcesContent into SourceNode
    aSourceMapConsumer.sources.forEach(function(sourceFile) {
      const content = aSourceMapConsumer.sourceContentFor(sourceFile);
      if (content != null) {
        if (aRelativePath != null) {
          sourceFile = util.join(aRelativePath, sourceFile);
        }
        node.setSourceContent(sourceFile, content);
      }
    });

    return node;

    function addMappingWithCode(mapping, code) {
      if (mapping === null || mapping.source === undefined) {
        node.add(code);
      } else {
        const source = aRelativePath
          ? util.join(aRelativePath, mapping.source)
          : mapping.source;
        node.add(new SourceNode(mapping.originalLine,
                                mapping.originalColumn,
                                source,
                                code,
                                mapping.name));
      }
    }
  }

  /**
   * Add a chunk of generated JS to this source node.
   *
   * @param aChunk A string snippet of generated JS code, another instance of
   *        SourceNode, or an array where each member is one of those things.
   */
  add(aChunk) {
    if (Array.isArray(aChunk)) {
      aChunk.forEach(function(chunk) {
        this.add(chunk);
      }, this);
    } else if (aChunk[isSourceNode] || typeof aChunk === "string") {
      if (aChunk) {
        this.children.push(aChunk);
      }
    } else {
      throw new TypeError(
        "Expected a SourceNode, string, or an array of SourceNodes and strings. Got " + aChunk
      );
    }
    return this;
  }

  /**
   * Add a chunk of generated JS to the beginning of this source node.
   *
   * @param aChunk A string snippet of generated JS code, another instance of
   *        SourceNode, or an array where each member is one of those things.
   */
  prepend(aChunk) {
    if (Array.isArray(aChunk)) {
      for (let i = aChunk.length - 1; i >= 0; i--) {
        this.prepend(aChunk[i]);
      }
    } else if (aChunk[isSourceNode] || typeof aChunk === "string") {
      this.children.unshift(aChunk);
    } else {
      throw new TypeError(
        "Expected a SourceNode, string, or an array of SourceNodes and strings. Got " + aChunk
      );
    }
    return this;
  }

  /**
   * Walk over the tree of JS snippets in this node and its children. The
   * walking function is called once for each snippet of JS and is passed that
   * snippet and the its original associated source's line/column location.
   *
   * @param aFn The traversal function.
   */
  walk(aFn) {
    let chunk;
    for (let i = 0, len = this.children.length; i < len; i++) {
      chunk = this.children[i];
      if (chunk[isSourceNode]) {
        chunk.walk(aFn);
      } else if (chunk !== "") {
        aFn(chunk, { source: this.source,
                      line: this.line,
                      column: this.column,
                      name: this.name });
      }
    }
  }

  /**
   * Like `String.prototype.join` except for SourceNodes. Inserts `aStr` between
   * each of `this.children`.
   *
   * @param aSep The separator.
   */
  join(aSep) {
    let newChildren;
    let i;
    const len = this.children.length;
    if (len > 0) {
      newChildren = [];
      for (i = 0; i < len - 1; i++) {
        newChildren.push(this.children[i]);
        newChildren.push(aSep);
      }
      newChildren.push(this.children[i]);
      this.children = newChildren;
    }
    return this;
  }

  /**
   * Call String.prototype.replace on the very right-most source snippet. Useful
   * for trimming whitespace from the end of a source node, etc.
   *
   * @param aPattern The pattern to replace.
   * @param aReplacement The thing to replace the pattern with.
   */
  replaceRight(aPattern, aReplacement) {
    const lastChild = this.children[this.children.length - 1];
    if (lastChild[isSourceNode]) {
      lastChild.replaceRight(aPattern, aReplacement);
    } else if (typeof lastChild === "string") {
      this.children[this.children.length - 1] = lastChild.replace(aPattern, aReplacement);
    } else {
      this.children.push("".replace(aPattern, aReplacement));
    }
    return this;
  }

  /**
   * Set the source content for a source file. This will be added to the SourceMapGenerator
   * in the sourcesContent field.
   *
   * @param aSourceFile The filename of the source file
   * @param aSourceContent The content of the source file
   */
  setSourceContent(aSourceFile, aSourceContent) {
    this.sourceContents[util.toSetString(aSourceFile)] = aSourceContent;
  }

  /**
   * Walk over the tree of SourceNodes. The walking function is called for each
   * source file content and is passed the filename and source content.
   *
   * @param aFn The traversal function.
   */
  walkSourceContents(aFn) {
    for (let i = 0, len = this.children.length; i < len; i++) {
      if (this.children[i][isSourceNode]) {
        this.children[i].walkSourceContents(aFn);
      }
    }

    const sources = Object.keys(this.sourceContents);
    for (let i = 0, len = sources.length; i < len; i++) {
      aFn(util.fromSetString(sources[i]), this.sourceContents[sources[i]]);
    }
  }

  /**
   * Return the string representation of this source node. Walks over the tree
   * and concatenates all the various snippets together to one string.
   */
  toString() {
    let str = "";
    this.walk(function(chunk) {
      str += chunk;
    });
    return str;
  }

  /**
   * Returns the string representation of this source node along with a source
   * map.
   */
  toStringWithSourceMap(aArgs) {
    const generated = {
      code: "",
      line: 1,
      column: 0
    };
    const map = new SourceMapGenerator(aArgs);
    let sourceMappingActive = false;
    let lastOriginalSource = null;
    let lastOriginalLine = null;
    let lastOriginalColumn = null;
    let lastOriginalName = null;
    this.walk(function(chunk, original) {
      generated.code += chunk;
      if (original.source !== null
          && original.line !== null
          && original.column !== null) {
        if (lastOriginalSource !== original.source
          || lastOriginalLine !== original.line
          || lastOriginalColumn !== original.column
          || lastOriginalName !== original.name) {
          map.addMapping({
            source: original.source,
            original: {
              line: original.line,
              column: original.column
            },
            generated: {
              line: generated.line,
              column: generated.column
            },
            name: original.name
          });
        }
        lastOriginalSource = original.source;
        lastOriginalLine = original.line;
        lastOriginalColumn = original.column;
        lastOriginalName = original.name;
        sourceMappingActive = true;
      } else if (sourceMappingActive) {
        map.addMapping({
          generated: {
            line: generated.line,
            column: generated.column
          }
        });
        lastOriginalSource = null;
        sourceMappingActive = false;
      }
      for (let idx = 0, length = chunk.length; idx < length; idx++) {
        if (chunk.charCodeAt(idx) === NEWLINE_CODE) {
          generated.line++;
          generated.column = 0;
          // Mappings end at eol
          if (idx + 1 === length) {
            lastOriginalSource = null;
            sourceMappingActive = false;
          } else if (sourceMappingActive) {
            map.addMapping({
              source: original.source,
              original: {
                line: original.line,
                column: original.column
              },
              generated: {
                line: generated.line,
                column: generated.column
              },
              name: original.name
            });
          }
        } else {
          generated.column++;
        }
      }
    });
    this.walkSourceContents(function(sourceFile, sourceContent) {
      map.setSourceContent(sourceFile, sourceContent);
    });

    return { code: generated.code, map };
  }
}

exports.SourceNode = SourceNode;


/***/ }),

/***/ 407:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function assert(condition, message) {
  if (!condition) {
    throw new Error(`Assertion failure: ${message}`);
  }
}

module.exports = assert;

/***/ }),

/***/ 408:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  networkRequest
} = __webpack_require__(7);

const {
  getSourceMap,
  setSourceMap
} = __webpack_require__(104);

const {
  WasmRemap
} = __webpack_require__(409);

const {
  SourceMapConsumer
} = __webpack_require__(60);

const {
  convertToJSON
} = __webpack_require__(510);

const {
  createConsumer
} = __webpack_require__(179);

// URLs which have been seen in a completed source map request.
const originalURLs = new Set();

function clearOriginalURLs() {
  originalURLs.clear();
}

function hasOriginalURL(url) {
  return originalURLs.has(url);
}

function _resolveSourceMapURL(source) {
  const {
    url = "",
    sourceMapURL = ""
  } = source;

  if (!url) {
    // If the source doesn't have a URL, don't resolve anything.
    return {
      sourceMapURL,
      baseURL: sourceMapURL
    };
  }

  const resolvedURL = new URL(sourceMapURL, url);
  const resolvedString = resolvedURL.toString();
  let baseURL = resolvedString; // When the sourceMap is a data: URL, fall back to using the
  // source's URL, if possible.

  if (resolvedURL.protocol == "data:") {
    baseURL = url;
  }

  return {
    sourceMapURL: resolvedString,
    baseURL
  };
}

async function _resolveAndFetch(generatedSource) {
  // Fetch the sourcemap over the network and create it.
  const {
    sourceMapURL,
    baseURL
  } = _resolveSourceMapURL(generatedSource);

  let fetched = await networkRequest(sourceMapURL, {
    loadFromCache: false
  });

  if (fetched.isDwarf) {
    fetched = {
      content: await convertToJSON(fetched.content)
    };
  } // Create the source map and fix it up.


  let map = await createConsumer(fetched.content, baseURL);

  if (generatedSource.isWasm) {
    map = new WasmRemap(map); // Check if experimental scope info exists.

    if (fetched.content.includes("x-scopes")) {
      const parsedJSON = JSON.parse(fetched.content);
      map.xScopes = parsedJSON["x-scopes"];
    }
  }

  if (map && map.sources) {
    map.sources.forEach(url => originalURLs.add(url));
  }

  return map;
}

function fetchSourceMap(generatedSource) {
  const existingRequest = getSourceMap(generatedSource.id); // If it has already been requested, return the request. Make sure
  // to do this even if sourcemapping is turned off, because
  // pretty-printing uses sourcemaps.
  //
  // An important behavior here is that if it's in the middle of
  // requesting it, all subsequent calls will block on the initial
  // request.

  if (existingRequest) {
    return existingRequest;
  }

  if (!generatedSource.sourceMapURL) {
    return null;
  } // Fire off the request, set it in the cache, and return it.


  const req = _resolveAndFetch(generatedSource); // Make sure the cached promise does not reject, because we only
  // want to report the error once.


  setSourceMap(generatedSource.id, req.catch(() => null));
  return req;
}

module.exports = {
  fetchSourceMap,
  hasOriginalURL,
  clearOriginalURLs
};

/***/ }),

/***/ 409:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * SourceMapConsumer for WebAssembly source maps. It transposes columns with
 * lines, which allows mapping data to be used with SpiderMonkey Debugger API.
 */
class WasmRemap {
  /**
   * @param map SourceMapConsumer
   */
  constructor(map) {
    this._map = map;
    this.version = map.version;
    this.file = map.file;
    this._computeColumnSpans = false;
  }

  get sources() {
    return this._map.sources;
  }

  get sourceRoot() {
    return this._map.sourceRoot;
  }

  get names() {
    return this._map.names;
  }

  get sourcesContent() {
    return this._map.sourcesContent;
  }

  get mappings() {
    throw new Error("not supported");
  }

  computeColumnSpans() {
    this._computeColumnSpans = true;
  }

  originalPositionFor(generatedPosition) {
    const result = this._map.originalPositionFor({
      line: 1,
      column: generatedPosition.line,
      bias: generatedPosition.bias
    });

    return result;
  }

  _remapGeneratedPosition(position) {
    const generatedPosition = {
      line: position.column,
      column: 0
    };

    if (this._computeColumnSpans) {
      generatedPosition.lastColumn = 0;
    }

    return generatedPosition;
  }

  generatedPositionFor(originalPosition) {
    const position = this._map.generatedPositionFor(originalPosition);

    return this._remapGeneratedPosition(position);
  }

  allGeneratedPositionsFor(originalPosition) {
    const positions = this._map.allGeneratedPositionsFor(originalPosition);

    return positions.map(position => {
      return this._remapGeneratedPosition(position);
    });
  }

  hasContentsOfAllSources() {
    return this._map.hasContentsOfAllSources();
  }

  sourceContentFor(source, returnNullOnMissing) {
    return this._map.sourceContentFor(source, returnNullOnMissing);
  }

  eachMapping(callback, context, order) {
    this._map.eachMapping(entry => {
      const {
        source,
        generatedColumn,
        originalLine,
        originalColumn,
        name
      } = entry;
      callback({
        source,
        generatedLine: generatedColumn,
        generatedColumn: 0,
        lastGeneratedColumn: 0,
        originalLine,
        originalColumn,
        name
      });
    }, context, order);
  }

}

exports.WasmRemap = WasmRemap;

/***/ }),

/***/ 411:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  getWasmXScopes
} = __webpack_require__(510);

const {
  getSourceMap
} = __webpack_require__(104);

const {
  generatedToOriginalId
} = __webpack_require__(64); // Returns expanded stack frames details based on the generated location.
// The function return null if not information was found.


async function getOriginalStackFrames(generatedLocation) {
  const wasmXScopes = await getWasmXScopes(generatedLocation.sourceId, {
    getSourceMap,
    generatedToOriginalId
  });

  if (!wasmXScopes) {
    return null;
  }

  const scopes = wasmXScopes.search(generatedLocation);

  if (scopes.length === 0) {
    console.warn("Something wrong with debug data: none original frames found");
    return null;
  }

  return scopes;
}

module.exports = {
  getOriginalStackFrames
};

/***/ }),

/***/ 507:
/***/ (function(module, exports) {

module.exports = 
(() => { 
  importScripts("resource://devtools/client/shared/vendor/whatwg-url.js");
  return { URL }
})()
;

/***/ }),

/***/ 510:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  convertToJSON
} = __webpack_require__(512);

const {
  setAssetRootURL
} = __webpack_require__(511);

const {
  getWasmXScopes,
  clearWasmXScopes
} = __webpack_require__(513);

module.exports = {
  convertToJSON,
  setAssetRootURL,
  getWasmXScopes,
  clearWasmXScopes
};

/***/ }),

/***/ 511:
/***/ (function(module, exports) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let root;

function setAssetRootURL(assetRoot) {
  root = assetRoot;
}

async function getDwarfToWasmData(name) {
  if (!root) {
    throw new Error(`No wasm path - Unable to resolve ${name}`);
  }

  const response = await fetch(`${root}/dwarf_to_json.wasm`);
  return response.arrayBuffer();
}

module.exports = {
  setAssetRootURL,
  getDwarfToWasmData
};

/***/ }),

/***/ 512:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint camelcase: 0*/
const {
  getDwarfToWasmData
} = __webpack_require__(511);

let cachedWasmModule;
let utf8Decoder;

function convertDwarf(wasm, instance) {
  const {
    memory,
    alloc_mem,
    free_mem,
    convert_dwarf
  } = instance.exports;
  const wasmPtr = alloc_mem(wasm.byteLength);
  new Uint8Array(memory.buffer, wasmPtr, wasm.byteLength).set(new Uint8Array(wasm));
  const resultPtr = alloc_mem(12);
  const enableXScopes = true;
  const success = convert_dwarf(wasmPtr, wasm.byteLength, resultPtr, resultPtr + 4, enableXScopes);
  free_mem(wasmPtr);
  const resultView = new DataView(memory.buffer, resultPtr, 12);
  const outputPtr = resultView.getUint32(0, true),
        outputLen = resultView.getUint32(4, true);
  free_mem(resultPtr);

  if (!success) {
    throw new Error("Unable to convert from DWARF sections");
  }

  if (!utf8Decoder) {
    utf8Decoder = new TextDecoder("utf-8");
  }

  const output = utf8Decoder.decode(new Uint8Array(memory.buffer, outputPtr, outputLen));
  free_mem(outputPtr);
  return output;
}

async function convertToJSON(buffer) {
  // Note: We don't 'await' here because it could mean that multiple
  // calls to 'convertToJSON' could cause multiple fetches to be started.
  cachedWasmModule = cachedWasmModule || loadConverterModule();
  return convertDwarf(buffer, (await cachedWasmModule));
}

async function loadConverterModule() {
  const wasm = await getDwarfToWasmData();
  const imports = {};
  const {
    instance
  } = await WebAssembly.instantiate(wasm, imports);
  return instance;
}

module.exports = {
  convertToJSON
};

/***/ }),

/***/ 513:
/***/ (function(module, exports, __webpack_require__) {

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint camelcase: 0*/
const {
  decodeExpr
} = __webpack_require__(514);

const xScopes = new Map();

function indexLinkingNames(items) {
  const result = new Map();
  let queue = [...items];

  while (queue.length > 0) {
    const item = queue.shift();

    if ("uid" in item) {
      result.set(item.uid, item);
    } else if ("linkage_name" in item) {
      // TODO the linkage_name string value is used for compatibility
      // with old format. Remove in favour of the uid referencing.
      result.set(item.linkage_name, item);
    }

    if ("children" in item) {
      queue = [...queue, ...item.children];
    }
  }

  return result;
}

function getIndexedItem(index, key) {
  if (typeof key === "object" && key != null) {
    return index.get(key.uid);
  }

  if (typeof key === "string") {
    return index.get(key);
  }

  return null;
}

async function getXScopes(sourceId, getSourceMap) {
  if (xScopes.has(sourceId)) {
    return xScopes.get(sourceId);
  }

  const map = await getSourceMap(sourceId);

  if (!map || !map.xScopes) {
    xScopes.set(sourceId, null);
    return null;
  }

  const {
    code_section_offset,
    debug_info
  } = map.xScopes;
  const xScope = {
    code_section_offset,
    debug_info,
    idIndex: indexLinkingNames(debug_info),
    sources: map.sources
  };
  xScopes.set(sourceId, xScope);
  return xScope;
}

function isInRange(item, pc) {
  if ("ranges" in item) {
    return item.ranges.some(r => r[0] <= pc && pc < r[1]);
  }

  if ("high_pc" in item) {
    return item.low_pc <= pc && pc < item.high_pc;
  }

  return false;
}

function decodeExprAt(expr, pc) {
  if (typeof expr === "string") {
    return decodeExpr(expr);
  }

  const foundAt = expr.find(i => i.range[0] <= pc && pc < i.range[1]);
  return foundAt ? decodeExpr(foundAt.expr) : null;
}

function getVariables(scope, pc) {
  const vars = scope.children ? scope.children.reduce((result, item) => {
    switch (item.tag) {
      case "variable":
      case "formal_parameter":
        result.push({
          name: item.name || "",
          expr: item.location ? decodeExprAt(item.location, pc) : null
        });
        break;

      case "lexical_block":
        // FIXME build scope blocks (instead of combining)
        const tmp = getVariables(item, pc);
        result = [...tmp.vars, ...result];
        break;
    }

    return result;
  }, []) : [];
  const frameBase = scope.frame_base ? decodeExpr(scope.frame_base) : null;
  return {
    vars,
    frameBase
  };
}

function filterScopes(items, pc, lastItem, index) {
  if (!items) {
    return [];
  }

  return items.reduce((result, item) => {
    switch (item.tag) {
      case "compile_unit":
        if (isInRange(item, pc)) {
          result = [...result, ...filterScopes(item.children, pc, lastItem, index)];
        }

        break;

      case "namespace":
      case "structure_type":
      case "union_type":
        result = [...result, ...filterScopes(item.children, pc, lastItem, index)];
        break;

      case "subprogram":
        if (isInRange(item, pc)) {
          const s = {
            id: item.linkage_name,
            name: item.name,
            variables: getVariables(item, pc)
          };
          result = [...result, s, ...filterScopes(item.children, pc, s, index)];
        }

        break;

      case "inlined_subroutine":
        if (isInRange(item, pc)) {
          const linkedItem = getIndexedItem(index, item.abstract_origin);
          const s = {
            id: item.abstract_origin,
            name: linkedItem ? linkedItem.name : void 0,
            variables: getVariables(item, pc)
          };

          if (lastItem) {
            lastItem.file = item.call_file;
            lastItem.line = item.call_line;
          }

          result = [...result, s, ...filterScopes(item.children, pc, s, index)];
        }

        break;
    }

    return result;
  }, []);
}

class XScope {
  constructor(xScopeData, sourceMapContext) {
    _defineProperty(this, "xScope", void 0);

    _defineProperty(this, "sourceMapContext", void 0);

    this.xScope = xScopeData;
    this.sourceMapContext = sourceMapContext;
  }

  search(generatedLocation) {
    const {
      code_section_offset,
      debug_info,
      sources,
      idIndex
    } = this.xScope;
    const pc = generatedLocation.line - (code_section_offset || 0);
    const scopes = filterScopes(debug_info, pc, null, idIndex);
    scopes.reverse();
    return scopes.map(i => {
      if (!("file" in i)) {
        return {
          displayName: i.name || "",
          variables: i.variables
        };
      }

      const sourceId = this.sourceMapContext.generatedToOriginalId(generatedLocation.sourceId, sources[i.file || 0]);
      return {
        displayName: i.name || "",
        variables: i.variables,
        location: {
          line: i.line || 0,
          sourceId
        }
      };
    });
  }

}

async function getWasmXScopes(sourceId, sourceMapContext) {
  const {
    getSourceMap
  } = sourceMapContext;
  const xScopeData = await getXScopes(sourceId, getSourceMap);

  if (!xScopeData) {
    return null;
  }

  return new XScope(xScopeData, sourceMapContext);
}

function clearWasmXScopes() {
  xScopes.clear();
}

module.exports = {
  getWasmXScopes,
  clearWasmXScopes
};

/***/ }),

/***/ 514:
/***/ (function(module, exports) {

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint camelcase: 0*/

/* eslint-disable no-inline-comments */
class Value {
  constructor(val) {
    _defineProperty(this, "val", void 0);

    this.val = val;
  }

  toString() {
    return `${this.val}`;
  }

}

const Int32Formatter = {
  fromAddr(addr) {
    return `(new DataView(memory0.buffer).getInt32(${addr}, true))`;
  },

  fromValue(value) {
    return `${value.val}`;
  }

};
const Uint32Formatter = {
  fromAddr(addr) {
    return `(new DataView(memory0.buffer).getUint32(${addr}, true))`;
  },

  fromValue(value) {
    return `(${value.val} >>> 0)`;
  }

};

function createPieceFormatter(bytes) {
  let getter;

  switch (bytes) {
    case 0:
    case 1:
      getter = "getUint8";
      break;

    case 2:
      getter = "getUint16";
      break;

    case 3:
    case 4:
    default:
      // FIXME 64-bit
      getter = "getUint32";
      break;
  }

  const mask = (1 << 8 * bytes) - 1;
  return {
    fromAddr(addr) {
      return `(new DataView(memory0.buffer).${getter}(${addr}, true))`;
    },

    fromValue(value) {
      return `((${value.val} & ${mask}) >>> 0)`;
    }

  };
} // eslint-disable-next-line complexity


function toJS(buf, typeFormatter, frame_base = "fp()") {
  const readU8 = function () {
    return buf[i++];
  };

  const readS8 = function () {
    return readU8() << 24 >> 24;
  };

  const readU16 = function () {
    const w = buf[i] | buf[i + 1] << 8;
    i += 2;
    return w;
  };

  const readS16 = function () {
    return readU16() << 16 >> 16;
  };

  const readS32 = function () {
    const w = buf[i] | buf[i + 1] << 8 | buf[i + 2] << 16 | buf[i + 3] << 24;
    i += 4;
    return w;
  };

  const readU32 = function () {
    return readS32() >>> 0;
  };

  const readU = function () {
    let n = 0,
        shift = 0,
        b;

    while ((b = readU8()) & 0x80) {
      n |= (b & 0x7f) << shift;
      shift += 7;
    }

    return n | b << shift;
  };

  const readS = function () {
    let n = 0,
        shift = 0,
        b;

    while ((b = readU8()) & 0x80) {
      n |= (b & 0x7f) << shift;
      shift += 7;
    }

    n |= b << shift;
    shift += 7;
    return shift > 32 ? n << 32 - shift >> 32 - shift : n;
  };

  const popValue = function (formatter) {
    const loc = stack.pop();

    if (loc instanceof Value) {
      return formatter.fromValue(loc);
    }

    return formatter.fromAddr(loc);
  };

  let i = 0,
      a,
      b;
  const stack = [frame_base];

  while (i < buf.length) {
    const code = buf[i++];

    switch (code) {
      case 0x03
      /* DW_OP_addr */
      :
        stack.push(Uint32Formatter.fromAddr(readU32()));
        break;

      case 0x08
      /* DW_OP_const1u 0x08 1 1-byte constant */
      :
        stack.push(readU8());
        break;

      case 0x09
      /* DW_OP_const1s 0x09 1 1-byte constant */
      :
        stack.push(readS8());
        break;

      case 0x0a
      /* DW_OP_const2u 0x0a 1 2-byte constant */
      :
        stack.push(readU16());
        break;

      case 0x0b
      /* DW_OP_const2s 0x0b 1 2-byte constant */
      :
        stack.push(readS16());
        break;

      case 0x0c
      /* DW_OP_const2u 0x0a 1 2-byte constant */
      :
        stack.push(readU32());
        break;

      case 0x0d
      /* DW_OP_const2s 0x0b 1 2-byte constant */
      :
        stack.push(readS32());
        break;

      case 0x10
      /* DW_OP_constu 0x10 1 ULEB128 constant */
      :
        stack.push(readU());
        break;

      case 0x11
      /* DW_OP_const2s 0x0b 1 2-byte constant */
      :
        stack.push(readS());
        break;

      case 0x1c
      /* DW_OP_minus */
      :
        b = stack.pop();
        a = stack.pop();
        stack.push(`${a} - ${b}`);
        break;

      case 0x22
      /* DW_OP_plus */
      :
        b = stack.pop();
        a = stack.pop();
        stack.push(`${a} + ${b}`);
        break;

      case 0x23
      /* DW_OP_plus_uconst */
      :
        b = readU();
        a = stack.pop();
        stack.push(`${a} + ${b}`);
        break;

      case 0x30
      /* DW_OP_lit0 */
      :
      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36:
      case 0x37:
      case 0x38:
      case 0x39:
      case 0x3a:
      case 0x3b:
      case 0x3c:
      case 0x3d:
      case 0x3e:
      case 0x3f:
      case 0x40:
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
      case 0x45:
      case 0x46:
      case 0x47:
      case 0x48:
      case 0x49:
      case 0x4a:
      case 0x4b:
      case 0x4c:
      case 0x4d:
      case 0x4e:
      case 0x4f:
        stack.push(`${code - 0x30}`);
        break;

      case 0x93
      /* DW_OP_piece */
      :
        {
          a = readU();
          const formatter = createPieceFormatter(a);
          stack.push(popValue(formatter));
          break;
        }

      case 0x9f
      /* DW_OP_stack_value */
      :
        stack.push(new Value(stack.pop()));
        break;

      case 0xf6
      /* WASM ext (old, FIXME phase out) */
      :
      case 0xed
      /* WASM ext */
      :
        b = readU();
        a = readS();

        switch (b) {
          case 0:
            stack.push(`var${a}`);
            break;

          case 1:
            stack.push(`global${a}`);
            break;

          default:
            stack.push(`ti${b}(${a})`);
            break;
        }

        break;

      default:
        // Unknown encoding, baling out
        return null;
    }
  } // FIXME use real DWARF type information


  return popValue(typeFormatter);
}

function decodeExpr(expr) {
  if (expr.includes("//")) {
    expr = expr.slice(0, expr.indexOf("//")).trim();
  }

  const code = new Uint8Array(expr.length >> 1);

  for (let i = 0; i < code.length; i++) {
    code[i] = parseInt(expr.substr(i << 1, 2), 16);
  }

  const typeFormatter = Int32Formatter;
  return toJS(code, typeFormatter) || `dwarf("${expr}")`;
}

module.exports = {
  decodeExpr
};

/***/ }),

/***/ 515:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const {
  SourceMapConsumer
} = __webpack_require__(60);

const {
  setAssetRootURL: wasmDwarfSetAssetRootURL
} = __webpack_require__(510);

function setAssetRootURL(assetRoot) {
  // Remove any trailing slash so we don't generate a double-slash below.
  const root = assetRoot.replace(/\/$/, "");
  wasmDwarfSetAssetRootURL(root);
  SourceMapConsumer.initialize({
    "lib/mappings.wasm": `${root}/source-map-mappings.wasm`
  });
}

module.exports = {
  setAssetRootURL
};

/***/ }),

/***/ 60:
/***/ (function(module, exports, __webpack_require__) {

/*
 * Copyright 2009-2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE.txt or:
 * http://opensource.org/licenses/BSD-3-Clause
 */
exports.SourceMapGenerator = __webpack_require__(171).SourceMapGenerator;
exports.SourceMapConsumer = __webpack_require__(403).SourceMapConsumer;
exports.SourceNode = __webpack_require__(406).SourceNode;


/***/ }),

/***/ 61:
/***/ (function(module, exports, __webpack_require__) {

/* -*- Mode: js; js-indent-level: 2; -*- */
/*
 * Copyright 2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE or:
 * http://opensource.org/licenses/BSD-3-Clause
 */

const URL = __webpack_require__(393);

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
  }
    throw new Error('"' + aName + '" is a required argument.');

}
exports.getArg = getArg;

const supportsNullProto = (function() {
  const obj = Object.create(null);
  return !("__proto__" in obj);
}());

function identity(s) {
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
    return "$" + aStr;
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

  const length = s.length;

  if (length < 9 /* "__proto__".length */) {
    return false;
  }

  /* eslint-disable no-multi-spaces */
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
  /* eslint-enable no-multi-spaces */

  for (let i = length - 10; i >= 0; i--) {
    if (s.charCodeAt(i) !== 36 /* '$' */) {
      return false;
    }
  }

  return true;
}

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
  let cmp = mappingA.generatedLine - mappingB.generatedLine;
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
  return JSON.parse(str.replace(/^\)]}'[^\n]*\n/, ""));
}
exports.parseSourceMapInput = parseSourceMapInput;

// We use 'http' as the base here because we want URLs processed relative
// to the safe base to be treated as "special" URLs during parsing using
// the WHATWG URL parsing. This ensures that backslash normalization
// applies to the path and such.
const PROTOCOL = "http:";
const PROTOCOL_AND_HOST = `${PROTOCOL}//host`;

/**
 * Make it easy to create small utilities that tweak a URL's path.
 */
function createSafeHandler(cb) {
  return input => {
    const type = getURLType(input);
    const base = buildSafeBase(input);
    const url = new URL(input, base);

    cb(url);

    const result = url.toString();

    if (type === "absolute") {
      return result;
    } else if (type === "scheme-relative") {
      return result.slice(PROTOCOL.length);
    } else if (type === "path-absolute") {
      return result.slice(PROTOCOL_AND_HOST.length);
    }

    // This assumes that the callback will only change
    // the path, search and hash values.
    return computeRelativeURL(base, result);
  };
}

function withBase(url, base) {
  return new URL(url, base).toString();
}

function buildUniqueSegment(prefix, str) {
  let id = 0;
  do {
    const ident = prefix + (id++);
    if (str.indexOf(ident) === -1) return ident;
  } while (true);
}

function buildSafeBase(str) {
  const maxDotParts = str.split("..").length - 1;

  // If we used a segment that also existed in `str`, then we would be unable
  // to compute relative paths. For example, if `segment` were just "a":
  //
  //   const url = "../../a/"
  //   const base = buildSafeBase(url); // http://host/a/a/
  //   const joined = "http://host/a/";
  //   const result = relative(base, joined);
  //
  // Expected: "../../a/";
  // Actual: "a/"
  //
  const segment = buildUniqueSegment("p", str);

  let base = `${PROTOCOL_AND_HOST}/`;
  for (let i = 0; i < maxDotParts; i++) {
    base += `${segment}/`;
  }
  return base;
}

const ABSOLUTE_SCHEME = /^[A-Za-z0-9\+\-\.]+:/;
function getURLType(url) {
  if (url[0] === "/") {
    if (url[1] === "/") return "scheme-relative";
    return "path-absolute";
  }

  return ABSOLUTE_SCHEME.test(url) ? "absolute" : "path-relative";
}

/**
 * Given two URLs that are assumed to be on the same
 * protocol/host/user/password build a relative URL from the
 * path, params, and hash values.
 *
 * @param rootURL The root URL that the target will be relative to.
 * @param targetURL The target that the relative URL points to.
 * @return A rootURL-relative, normalized URL value.
 */
function computeRelativeURL(rootURL, targetURL) {
  if (typeof rootURL === "string") rootURL = new URL(rootURL);
  if (typeof targetURL === "string") targetURL = new URL(targetURL);

  const targetParts = targetURL.pathname.split("/");
  const rootParts = rootURL.pathname.split("/");

  // If we've got a URL path ending with a "/", we remove it since we'd
  // otherwise be relative to the wrong location.
  if (rootParts.length > 0 && !rootParts[rootParts.length - 1]) {
    rootParts.pop();
  }

  while (
    targetParts.length > 0 &&
    rootParts.length > 0 &&
    targetParts[0] === rootParts[0]
  ) {
    targetParts.shift();
    rootParts.shift();
  }

  const relativePath = rootParts
    .map(() => "..")
    .concat(targetParts)
    .join("/");

  return relativePath + targetURL.search + targetURL.hash;
}

/**
 * Given a URL, ensure that it is treated as a directory URL.
 *
 * @param url
 * @return A normalized URL value.
 */
const ensureDirectory = createSafeHandler(url => {
  url.pathname = url.pathname.replace(/\/?$/, "/");
});

/**
 * Given a URL, strip off any filename if one is present.
 *
 * @param url
 * @return A normalized URL value.
 */
const trimFilename = createSafeHandler(url => {
  url.href = new URL(".", url.toString()).toString();
});

/**
 * Normalize a given URL.
 * * Convert backslashes.
 * * Remove any ".." and "." segments.
 *
 * @param url
 * @return A normalized URL value.
 */
const normalize = createSafeHandler(url => {});
exports.normalize = normalize;

/**
 * Joins two paths/URLs.
 *
 * All returned URLs will be normalized.
 *
 * @param aRoot The root path or URL. Assumed to reference a directory.
 * @param aPath The path or URL to be joined with the root.
 * @return A joined and normalized URL value.
 */
function join(aRoot, aPath) {
  const pathType = getURLType(aPath);
  const rootType = getURLType(aRoot);

  aRoot = ensureDirectory(aRoot);

  if (pathType === "absolute") {
    return withBase(aPath, undefined);
  }
  if (rootType === "absolute") {
    return withBase(aPath, aRoot);
  }

  if (pathType === "scheme-relative") {
    return normalize(aPath);
  }
  if (rootType === "scheme-relative") {
    return withBase(aPath, withBase(aRoot, PROTOCOL_AND_HOST)).slice(PROTOCOL.length);
  }

  if (pathType === "path-absolute") {
    return normalize(aPath);
  }
  if (rootType === "path-absolute") {
    return withBase(aPath, withBase(aRoot, PROTOCOL_AND_HOST)).slice(PROTOCOL_AND_HOST.length);
  }

  const base = buildSafeBase(aPath + aRoot);
  const newPath = withBase(aPath, withBase(aRoot, base));
  return computeRelativeURL(base, newPath);
}
exports.join = join;

/**
 * Make a path relative to a URL or another path. If returning a
 * relative URL is not possible, the original target will be returned.
 * All returned URLs will be normalized.
 *
 * @param aRoot The root path or URL.
 * @param aPath The path or URL to be made relative to aRoot.
 * @return A rootURL-relative (if possible), normalized URL value.
 */
function relative(rootURL, targetURL) {
  const result = relativeIfPossible(rootURL, targetURL);

  return typeof result === "string" ? result : normalize(targetURL);
}
exports.relative = relative;

function relativeIfPossible(rootURL, targetURL) {
  const urlType = getURLType(rootURL);
  if (urlType !== getURLType(targetURL)) {
    return null;
  }

  const base = buildSafeBase(rootURL + targetURL);
  const root = new URL(rootURL, base);
  const target = new URL(targetURL, base);

  try {
    new URL("", target.toString());
  } catch (err) {
    // Bail if the URL doesn't support things being relative to it,
    // For example, data: and blob: URLs.
    return null;
  }

  if (
    target.protocol !== root.protocol ||
    target.user !== root.user ||
    target.password !== root.password ||
    target.hostname !== root.hostname ||
    target.port !== root.port
  ) {
    return null;
  }

  return computeRelativeURL(root, target);
}

/**
 * Compute the URL of a source given the the source root, the source's
 * URL, and the source map's URL.
 */
function computeSourceURL(sourceRoot, sourceURL, sourceMapURL) {
  // The source map spec states that "sourceRoot" and "sources" entries are to be appended. While
  // that is a little vague, implementations have generally interpreted that as joining the
  // URLs with a `/` between then, assuming the "sourceRoot" doesn't already end with one.
  // For example,
  //
  //   sourceRoot: "some-dir",
  //   sources: ["/some-path.js"]
  //
  // and
  //
  //   sourceRoot: "some-dir/",
  //   sources: ["/some-path.js"]
  //
  // must behave as "some-dir/some-path.js".
  //
  // With this library's the transition to a more URL-focused implementation, that behavior is
  // preserved here. To acheive that, we trim the "/" from absolute-path when a sourceRoot value
  // is present in order to make the sources entries behave as if they are relative to the
  // "sourceRoot", as they would have if the two strings were simply concated.
  if (sourceRoot && getURLType(sourceURL) === "path-absolute") {
    sourceURL = sourceURL.replace(/^\//, "");
  }

  let url = normalize(sourceURL || "");

  // Parsing URLs can be expensive, so we only perform these joins when needed.
  if (sourceRoot) url = join(sourceRoot, url);
  if (sourceMapURL) url = join(trimFilename(sourceMapURL), url);
  return url;
}
exports.computeSourceURL = computeSourceURL;


/***/ }),

/***/ 64:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const md5 = __webpack_require__(105);

function originalToGeneratedId(sourceId) {
  if (isGeneratedId(sourceId)) {
    return sourceId;
  }

  const match = sourceId.match(/(.*)\/originalSource/);
  return match ? match[1] : "";
}

const getMd5 = memoize(url => md5(url));

function generatedToOriginalId(generatedId, url) {
  return `${generatedId}/originalSource-${getMd5(url)}`;
}

function isOriginalId(id) {
  return /\/originalSource/.test(id);
}

function isGeneratedId(id) {
  return !isOriginalId(id);
}
/**
 * Trims the query part or reference identifier of a URL string, if necessary.
 */


function trimUrlQuery(url) {
  const length = url.length;
  const q1 = url.indexOf("?");
  const q2 = url.indexOf("&");
  const q3 = url.indexOf("#");
  const q = Math.min(q1 != -1 ? q1 : length, q2 != -1 ? q2 : length, q3 != -1 ? q3 : length);
  return url.slice(0, q);
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

/***/ 7:
/***/ (function(module, exports, __webpack_require__) {

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const networkRequest = __webpack_require__(13);

const workerUtils = __webpack_require__(14);

module.exports = {
  networkRequest,
  workerUtils
};

/***/ })

/******/ });
});