/*

 Copyright 2014 Mozilla Foundation

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
console.time("Load Shared Dependencies");
var Shumway, Shumway$$inline_0 = Shumway || (Shumway = {});
Shumway$$inline_0.version = "0.10.268";
Shumway$$inline_0.build = "02fd13d";
var jsGlobal = function() {
  return this || (0,eval)("this//# sourceURL=jsGlobal-getter");
}(), inBrowser = "undefined" !== typeof window && "document" in window && "plugins" in window.document, inFirefox = "undefined" !== typeof navigator && 0 <= navigator.userAgent.indexOf("Firefox");
function dumpLine(k) {
}
jsGlobal.performance || (jsGlobal.performance = {});
jsGlobal.performance.now || (jsGlobal.performance.now = "undefined" !== typeof dateNow ? dateNow : Date.now);
function lazyInitializer(k, r, g) {
  Object.defineProperty(k, r, {get:function() {
    var c = g();
    Object.defineProperty(k, r, {value:c, configurable:!0, enumerable:!0});
    return c;
  }, configurable:!0, enumerable:!0});
}
var START_TIME = performance.now();
(function(k) {
  function r(d) {
    return(d | 0) === d;
  }
  function g(d) {
    return "object" === typeof d || "function" === typeof d;
  }
  function c(d) {
    return String(Number(d)) === d;
  }
  function t(d) {
    var e = 0;
    if ("number" === typeof d) {
      return e = d | 0, d === e && 0 <= e ? !0 : d >>> 0 === d;
    }
    if ("string" !== typeof d) {
      return!1;
    }
    var b = d.length;
    if (0 === b) {
      return!1;
    }
    if ("0" === d) {
      return!0;
    }
    if (b > k.UINT32_CHAR_BUFFER_LENGTH) {
      return!1;
    }
    var f = 0, e = d.charCodeAt(f++) - 48;
    if (1 > e || 9 < e) {
      return!1;
    }
    for (var q = 0, w = 0;f < b;) {
      w = d.charCodeAt(f++) - 48;
      if (0 > w || 9 < w) {
        return!1;
      }
      q = e;
      e = 10 * e + w;
    }
    return q < k.UINT32_MAX_DIV_10 || q === k.UINT32_MAX_DIV_10 && w <= k.UINT32_MAX_MOD_10 ? !0 : !1;
  }
  (function(d) {
    d[d._0 = 48] = "_0";
    d[d._1 = 49] = "_1";
    d[d._2 = 50] = "_2";
    d[d._3 = 51] = "_3";
    d[d._4 = 52] = "_4";
    d[d._5 = 53] = "_5";
    d[d._6 = 54] = "_6";
    d[d._7 = 55] = "_7";
    d[d._8 = 56] = "_8";
    d[d._9 = 57] = "_9";
  })(k.CharacterCodes || (k.CharacterCodes = {}));
  k.UINT32_CHAR_BUFFER_LENGTH = 10;
  k.UINT32_MAX = 4294967295;
  k.UINT32_MAX_DIV_10 = 429496729;
  k.UINT32_MAX_MOD_10 = 5;
  k.isString = function(d) {
    return "string" === typeof d;
  };
  k.isFunction = function(d) {
    return "function" === typeof d;
  };
  k.isNumber = function(d) {
    return "number" === typeof d;
  };
  k.isInteger = r;
  k.isArray = function(d) {
    return d instanceof Array;
  };
  k.isNumberOrString = function(d) {
    return "number" === typeof d || "string" === typeof d;
  };
  k.isObject = g;
  k.toNumber = function(d) {
    return+d;
  };
  k.isNumericString = c;
  k.isNumeric = function(d) {
    if ("number" === typeof d) {
      return!0;
    }
    if ("string" === typeof d) {
      var e = d.charCodeAt(0);
      return 65 <= e && 90 >= e || 97 <= e && 122 >= e || 36 === e || 95 === e ? !1 : t(d) || c(d);
    }
    return!1;
  };
  k.isIndex = t;
  k.isNullOrUndefined = function(d) {
    return void 0 == d;
  };
  var n;
  (function(d) {
    d.error = function(b) {
      console.error(b);
      throw Error(b);
    };
    d.assert = function(b, f) {
      void 0 === f && (f = "assertion failed");
      "" === b && (b = !0);
      if (!b) {
        if ("undefined" !== typeof console && "assert" in console) {
          throw console.assert(!1, f), Error(f);
        }
        d.error(f.toString());
      }
    };
    d.assertUnreachable = function(b) {
      throw Error("Reached unreachable location " + Error().stack.split("\n")[1] + b);
    };
    d.assertNotImplemented = function(b, f) {
      b || d.error("notImplemented: " + f);
    };
    d.warning = function(b, f, q) {
    };
    d.notUsed = function(b) {
    };
    d.notImplemented = function(b) {
    };
    d.dummyConstructor = function(b) {
    };
    d.abstractMethod = function(b) {
    };
    var e = {};
    d.somewhatImplemented = function(b) {
      e[b] || (e[b] = !0);
    };
    d.unexpected = function(b) {
      d.assert(!1, "Unexpected: " + b);
    };
    d.unexpectedCase = function(b) {
      d.assert(!1, "Unexpected Case: " + b);
    };
  })(n = k.Debug || (k.Debug = {}));
  k.getTicks = function() {
    return performance.now();
  };
  (function(d) {
    function e(f, q) {
      for (var b = 0, e = f.length;b < e;b++) {
        if (f[b] === q) {
          return b;
        }
      }
      f.push(q);
      return f.length - 1;
    }
    d.popManyInto = function(f, q, b) {
      for (var e = q - 1;0 <= e;e--) {
        b[e] = f.pop();
      }
      b.length = q;
    };
    d.popMany = function(f, b) {
      var w = f.length - b, e = f.slice(w, this.length);
      f.length = w;
      return e;
    };
    d.popManyIntoVoid = function(f, b) {
      f.length -= b;
    };
    d.pushMany = function(f, b) {
      for (var w = 0;w < b.length;w++) {
        f.push(b[w]);
      }
    };
    d.top = function(f) {
      return f.length && f[f.length - 1];
    };
    d.last = function(f) {
      return f.length && f[f.length - 1];
    };
    d.peek = function(f) {
      return f[f.length - 1];
    };
    d.indexOf = function(f, b) {
      for (var w = 0, e = f.length;w < e;w++) {
        if (f[w] === b) {
          return w;
        }
      }
      return-1;
    };
    d.equals = function(f, b) {
      if (f.length !== b.length) {
        return!1;
      }
      for (var w = 0;w < f.length;w++) {
        if (f[w] !== b[w]) {
          return!1;
        }
      }
      return!0;
    };
    d.pushUnique = e;
    d.unique = function(f) {
      for (var b = [], w = 0;w < f.length;w++) {
        e(b, f[w]);
      }
      return b;
    };
    d.copyFrom = function(f, b) {
      f.length = 0;
      d.pushMany(f, b);
    };
    d.ensureTypedArrayCapacity = function(f, b) {
      if (f.length < b) {
        var w = f;
        f = new f.constructor(k.IntegerUtilities.nearestPowerOfTwo(b));
        f.set(w, 0);
      }
      return f;
    };
    d.memCopy = function(f, b, w, e, d) {
      void 0 === w && (w = 0);
      void 0 === e && (e = 0);
      void 0 === d && (d = 0);
      0 < e || 0 < d && d < b.length ? (0 >= d && (d = b.length - e), f.set(b.subarray(e, e + d), w)) : f.set(b, w);
    };
    var b = function() {
      function f(f) {
        void 0 === f && (f = 16);
        this._f32 = this._i32 = this._u16 = this._u8 = null;
        this._offset = 0;
        this.ensureCapacity(f);
      }
      f.prototype.reset = function() {
        this._offset = 0;
      };
      Object.defineProperty(f.prototype, "offset", {get:function() {
        return this._offset;
      }, enumerable:!0, configurable:!0});
      f.prototype.getIndex = function(f) {
        return this._offset / f;
      };
      f.prototype.ensureAdditionalCapacity = function() {
        this.ensureCapacity(this._offset + 68);
      };
      f.prototype.ensureCapacity = function(f) {
        if (!this._u8) {
          this._u8 = new Uint8Array(f);
        } else {
          if (this._u8.length > f) {
            return;
          }
        }
        var b = 2 * this._u8.length;
        b < f && (b = f);
        f = new Uint8Array(b);
        f.set(this._u8, 0);
        this._u8 = f;
        this._u16 = new Uint16Array(f.buffer);
        this._i32 = new Int32Array(f.buffer);
        this._f32 = new Float32Array(f.buffer);
      };
      f.prototype.writeInt = function(f) {
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(f);
      };
      f.prototype.writeIntAt = function(f, b) {
        this.ensureCapacity(b + 4);
        this._i32[b >> 2] = f;
      };
      f.prototype.writeIntUnsafe = function(f) {
        this._i32[this._offset >> 2] = f;
        this._offset += 4;
      };
      f.prototype.writeFloat = function(f) {
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(f);
      };
      f.prototype.writeFloatUnsafe = function(f) {
        this._f32[this._offset >> 2] = f;
        this._offset += 4;
      };
      f.prototype.write4Floats = function(f, b, e, d) {
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(f, b, e, d);
      };
      f.prototype.write4FloatsUnsafe = function(f, b, e, d) {
        var a = this._offset >> 2;
        this._f32[a + 0] = f;
        this._f32[a + 1] = b;
        this._f32[a + 2] = e;
        this._f32[a + 3] = d;
        this._offset += 16;
      };
      f.prototype.write6Floats = function(f, b, e, d, a, h) {
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(f, b, e, d, a, h);
      };
      f.prototype.write6FloatsUnsafe = function(f, b, e, d, a, h) {
        var p = this._offset >> 2;
        this._f32[p + 0] = f;
        this._f32[p + 1] = b;
        this._f32[p + 2] = e;
        this._f32[p + 3] = d;
        this._f32[p + 4] = a;
        this._f32[p + 5] = h;
        this._offset += 24;
      };
      f.prototype.subF32View = function() {
        return this._f32.subarray(0, this._offset >> 2);
      };
      f.prototype.subI32View = function() {
        return this._i32.subarray(0, this._offset >> 2);
      };
      f.prototype.subU16View = function() {
        return this._u16.subarray(0, this._offset >> 1);
      };
      f.prototype.subU8View = function() {
        return this._u8.subarray(0, this._offset);
      };
      f.prototype.hashWords = function(f, b, e) {
        b = this._i32;
        for (var d = 0;d < e;d++) {
          f = (31 * f | 0) + b[d] | 0;
        }
        return f;
      };
      f.prototype.reserve = function(f) {
        f = f + 3 & -4;
        this.ensureCapacity(this._offset + f);
        this._offset += f;
      };
      return f;
    }();
    d.ArrayWriter = b;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
  var a = function() {
    function d(e) {
      this._u8 = new Uint8Array(e);
      this._u16 = new Uint16Array(e);
      this._i32 = new Int32Array(e);
      this._f32 = new Float32Array(e);
      this._offset = 0;
    }
    Object.defineProperty(d.prototype, "offset", {get:function() {
      return this._offset;
    }, enumerable:!0, configurable:!0});
    d.prototype.isEmpty = function() {
      return this._offset === this._u8.length;
    };
    d.prototype.readInt = function() {
      var e = this._i32[this._offset >> 2];
      this._offset += 4;
      return e;
    };
    d.prototype.readFloat = function() {
      var e = this._f32[this._offset >> 2];
      this._offset += 4;
      return e;
    };
    return d;
  }();
  k.ArrayReader = a;
  (function(d) {
    function e(f, b) {
      return Object.prototype.hasOwnProperty.call(f, b);
    }
    function b(f, b) {
      for (var w in b) {
        e(b, w) && (f[w] = b[w]);
      }
    }
    d.boxValue = function(f) {
      return void 0 == f || g(f) ? f : Object(f);
    };
    d.toKeyValueArray = function(f) {
      var b = Object.prototype.hasOwnProperty, e = [], d;
      for (d in f) {
        b.call(f, d) && e.push([d, f[d]]);
      }
      return e;
    };
    d.isPrototypeWriteable = function(f) {
      return Object.getOwnPropertyDescriptor(f, "prototype").writable;
    };
    d.hasOwnProperty = e;
    d.propertyIsEnumerable = function(f, b) {
      return Object.prototype.propertyIsEnumerable.call(f, b);
    };
    d.getOwnPropertyDescriptor = function(f, b) {
      return Object.getOwnPropertyDescriptor(f, b);
    };
    d.hasOwnGetter = function(f, b) {
      var e = Object.getOwnPropertyDescriptor(f, b);
      return!(!e || !e.get);
    };
    d.getOwnGetter = function(f, b) {
      var e = Object.getOwnPropertyDescriptor(f, b);
      return e ? e.get : null;
    };
    d.hasOwnSetter = function(f, b) {
      var e = Object.getOwnPropertyDescriptor(f, b);
      return!(!e || !e.set);
    };
    d.createMap = function() {
      return Object.create(null);
    };
    d.createArrayMap = function() {
      return[];
    };
    d.defineReadOnlyProperty = function(f, b, e) {
      Object.defineProperty(f, b, {value:e, writable:!1, configurable:!0, enumerable:!1});
    };
    d.getOwnPropertyDescriptors = function(f) {
      for (var b = d.createMap(), e = Object.getOwnPropertyNames(f), a = 0;a < e.length;a++) {
        b[e[a]] = Object.getOwnPropertyDescriptor(f, e[a]);
      }
      return b;
    };
    d.cloneObject = function(f) {
      var q = Object.create(Object.getPrototypeOf(f));
      b(q, f);
      return q;
    };
    d.copyProperties = function(f, b) {
      for (var e in b) {
        f[e] = b[e];
      }
    };
    d.copyOwnProperties = b;
    d.copyOwnPropertyDescriptors = function(f, b, w) {
      void 0 === w && (w = !0);
      for (var d in b) {
        if (e(b, d)) {
          var a = Object.getOwnPropertyDescriptor(b, d);
          if (w || !e(f, d)) {
            try {
              Object.defineProperty(f, d, a);
            } catch (h) {
            }
          }
        }
      }
    };
    d.getLatestGetterOrSetterPropertyDescriptor = function(b, q) {
      for (var e = {};b;) {
        var d = Object.getOwnPropertyDescriptor(b, q);
        d && (e.get = e.get || d.get, e.set = e.set || d.set);
        if (e.get && e.set) {
          break;
        }
        b = Object.getPrototypeOf(b);
      }
      return e;
    };
    d.defineNonEnumerableGetterOrSetter = function(b, q, e, a) {
      var h = d.getLatestGetterOrSetterPropertyDescriptor(b, q);
      h.configurable = !0;
      h.enumerable = !1;
      a ? h.get = e : h.set = e;
      Object.defineProperty(b, q, h);
    };
    d.defineNonEnumerableGetter = function(b, q, e) {
      Object.defineProperty(b, q, {get:e, configurable:!0, enumerable:!1});
    };
    d.defineNonEnumerableSetter = function(b, q, e) {
      Object.defineProperty(b, q, {set:e, configurable:!0, enumerable:!1});
    };
    d.defineNonEnumerableProperty = function(b, q, e) {
      Object.defineProperty(b, q, {value:e, writable:!0, configurable:!0, enumerable:!1});
    };
    d.defineNonEnumerableForwardingProperty = function(b, q, e) {
      Object.defineProperty(b, q, {get:h.makeForwardingGetter(e), set:h.makeForwardingSetter(e), writable:!0, configurable:!0, enumerable:!1});
    };
    d.defineNewNonEnumerableProperty = function(b, q, e) {
      d.defineNonEnumerableProperty(b, q, e);
    };
    d.createPublicAliases = function(b, q) {
      for (var e = {value:null, writable:!0, configurable:!0, enumerable:!1}, d = 0;d < q.length;d++) {
        var a = q[d];
        e.value = b[a];
        Object.defineProperty(b, "$Bg" + a, e);
      }
    };
  })(k.ObjectUtilities || (k.ObjectUtilities = {}));
  var h;
  (function(d) {
    d.makeForwardingGetter = function(e) {
      return new Function('return this["' + e + '"]//# sourceURL=fwd-get-' + e + ".as");
    };
    d.makeForwardingSetter = function(e) {
      return new Function("value", 'this["' + e + '"] = value;//# sourceURL=fwd-set-' + e + ".as");
    };
    d.bindSafely = function(e, b) {
      function f() {
        return e.apply(b, arguments);
      }
      f.boundTo = b;
      return f;
    };
  })(h = k.FunctionUtilities || (k.FunctionUtilities = {}));
  (function(d) {
    function e(b) {
      return "string" === typeof b ? '"' + b + '"' : "number" === typeof b || "boolean" === typeof b ? String(b) : b instanceof Array ? "[] " + b.length : typeof b;
    }
    d.repeatString = function(b, f) {
      for (var q = "", e = 0;e < f;e++) {
        q += b;
      }
      return q;
    };
    d.memorySizeToString = function(b) {
      b |= 0;
      return 1024 > b ? b + " B" : 1048576 > b ? (b / 1024).toFixed(2) + "KB" : (b / 1048576).toFixed(2) + "MB";
    };
    d.toSafeString = e;
    d.toSafeArrayString = function(b) {
      for (var f = [], q = 0;q < b.length;q++) {
        f.push(e(b[q]));
      }
      return f.join(", ");
    };
    d.utf8decode = function(b) {
      for (var f = new Uint8Array(4 * b.length), q = 0, e = 0, w = b.length;e < w;e++) {
        var d = b.charCodeAt(e);
        if (127 >= d) {
          f[q++] = d;
        } else {
          if (55296 <= d && 56319 >= d) {
            var a = b.charCodeAt(e + 1);
            56320 <= a && 57343 >= a && (d = ((d & 1023) << 10) + (a & 1023) + 65536, ++e);
          }
          0 !== (d & 4292870144) ? (f[q++] = 248 | d >>> 24 & 3, f[q++] = 128 | d >>> 18 & 63, f[q++] = 128 | d >>> 12 & 63, f[q++] = 128 | d >>> 6 & 63) : 0 !== (d & 4294901760) ? (f[q++] = 240 | d >>> 18 & 7, f[q++] = 128 | d >>> 12 & 63, f[q++] = 128 | d >>> 6 & 63) : 0 !== (d & 4294965248) ? (f[q++] = 224 | d >>> 12 & 15, f[q++] = 128 | d >>> 6 & 63) : f[q++] = 192 | d >>> 6 & 31;
          f[q++] = 128 | d & 63;
        }
      }
      return f.subarray(0, q);
    };
    d.utf8encode = function(b) {
      for (var f = 0, q = "";f < b.length;) {
        var e = b[f++] & 255;
        if (127 >= e) {
          q += String.fromCharCode(e);
        } else {
          var d = 192, w = 5;
          do {
            if ((e & (d >> 1 | 128)) === d) {
              break;
            }
            d = d >> 1 | 128;
            --w;
          } while (0 <= w);
          if (0 >= w) {
            q += String.fromCharCode(e);
          } else {
            for (var e = e & (1 << w) - 1, d = !1, a = 5;a >= w;--a) {
              var h = b[f++];
              if (128 != (h & 192)) {
                d = !0;
                break;
              }
              e = e << 6 | h & 63;
            }
            if (d) {
              for (w = f - (7 - a);w < f;++w) {
                q += String.fromCharCode(b[w] & 255);
              }
            } else {
              q = 65536 <= e ? q + String.fromCharCode(e - 65536 >> 10 & 1023 | 55296, e & 1023 | 56320) : q + String.fromCharCode(e);
            }
          }
        }
      }
      return q;
    };
    d.base64ArrayBuffer = function(b) {
      var f = "";
      b = new Uint8Array(b);
      for (var q = b.byteLength, e = q % 3, q = q - e, d, w, a, h, G = 0;G < q;G += 3) {
        h = b[G] << 16 | b[G + 1] << 8 | b[G + 2], d = (h & 16515072) >> 18, w = (h & 258048) >> 12, a = (h & 4032) >> 6, h &= 63, f += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[d] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[w] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[a] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[h];
      }
      1 == e ? (h = b[q], f += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 252) >> 2] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 3) << 4] + "==") : 2 == e && (h = b[q] << 8 | b[q + 1], f += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 64512) >> 10] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 1008) >> 4] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 15) << 
      2] + "=");
      return f;
    };
    d.escapeString = function(b) {
      void 0 !== b && (b = b.replace(/[^\w$]/gi, "$"), /^\d/.test(b) && (b = "$" + b));
      return b;
    };
    d.fromCharCodeArray = function(b) {
      for (var f = "", q = 0;q < b.length;q += 16384) {
        var e = Math.min(b.length - q, 16384), f = f + String.fromCharCode.apply(null, b.subarray(q, q + e))
      }
      return f;
    };
    d.variableLengthEncodeInt32 = function(b) {
      for (var f = 32 - Math.clz32(b), q = Math.ceil(f / 6), f = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[q], q = q - 1;0 <= q;q--) {
        f += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[b >> 6 * q & 63];
      }
      return f;
    };
    d.toEncoding = function(b) {
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[b];
    };
    d.fromEncoding = function(b) {
      if (65 <= b && 90 >= b) {
        return b - 65;
      }
      if (97 <= b && 122 >= b) {
        return b - 71;
      }
      if (48 <= b && 57 >= b) {
        return b + 4;
      }
      if (36 === b) {
        return 62;
      }
      if (95 === b) {
        return 63;
      }
    };
    d.variableLengthDecodeInt32 = function(b) {
      for (var f = d.fromEncoding(b.charCodeAt(0)), q = 0, e = 0;e < f;e++) {
        var w = 6 * (f - e - 1), q = q | d.fromEncoding(b.charCodeAt(1 + e)) << w
      }
      return q;
    };
    d.trimMiddle = function(b, f) {
      if (b.length <= f) {
        return b;
      }
      var q = f >> 1, e = f - q - 1;
      return b.substr(0, q) + "\u2026" + b.substr(b.length - e, e);
    };
    d.multiple = function(b, f) {
      for (var q = "", e = 0;e < f;e++) {
        q += b;
      }
      return q;
    };
    d.indexOfAny = function(b, f, q) {
      for (var e = b.length, d = 0;d < f.length;d++) {
        var w = b.indexOf(f[d], q);
        0 <= w && (e = Math.min(e, w));
      }
      return e === b.length ? -1 : e;
    };
    var b = Array(3), f = Array(4), q = Array(5), w = Array(6), a = Array(7), h = Array(8), p = Array(9);
    d.concat3 = function(f, q, e) {
      b[0] = f;
      b[1] = q;
      b[2] = e;
      return b.join("");
    };
    d.concat4 = function(b, q, e, d) {
      f[0] = b;
      f[1] = q;
      f[2] = e;
      f[3] = d;
      return f.join("");
    };
    d.concat5 = function(b, f, e, d, w) {
      q[0] = b;
      q[1] = f;
      q[2] = e;
      q[3] = d;
      q[4] = w;
      return q.join("");
    };
    d.concat6 = function(b, f, q, e, d, a) {
      w[0] = b;
      w[1] = f;
      w[2] = q;
      w[3] = e;
      w[4] = d;
      w[5] = a;
      return w.join("");
    };
    d.concat7 = function(b, f, q, e, d, w, h) {
      a[0] = b;
      a[1] = f;
      a[2] = q;
      a[3] = e;
      a[4] = d;
      a[5] = w;
      a[6] = h;
      return a.join("");
    };
    d.concat8 = function(b, f, q, e, d, w, a, G) {
      h[0] = b;
      h[1] = f;
      h[2] = q;
      h[3] = e;
      h[4] = d;
      h[5] = w;
      h[6] = a;
      h[7] = G;
      return h.join("");
    };
    d.concat9 = function(b, f, q, e, d, w, a, h, G) {
      p[0] = b;
      p[1] = f;
      p[2] = q;
      p[3] = e;
      p[4] = d;
      p[5] = w;
      p[6] = a;
      p[7] = h;
      p[8] = G;
      return p.join("");
    };
  })(k.StringUtilities || (k.StringUtilities = {}));
  (function(d) {
    var e = new Uint8Array([7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]), b = new Int32Array([-680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426, -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162, 1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632, 
    643717713, -373897302, -701558691, 38016083, -660478335, -405537848, 568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784, 1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556, -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222, -722521979, 76029189, -640364487, -421815835, 530742520, -995338651, -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606, -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649, 
    -145523070, -1120210379, 718787259, -343485551]);
    d.hashBytesTo32BitsMD5 = function(f, q, d) {
      var a = 1732584193, h = -271733879, p = -1732584194, m = 271733878, c = d + 72 & -64, l = new Uint8Array(c), n;
      for (n = 0;n < d;++n) {
        l[n] = f[q++];
      }
      l[n++] = 128;
      for (f = c - 8;n < f;) {
        l[n++] = 0;
      }
      l[n++] = d << 3 & 255;
      l[n++] = d >> 5 & 255;
      l[n++] = d >> 13 & 255;
      l[n++] = d >> 21 & 255;
      l[n++] = d >>> 29 & 255;
      l[n++] = 0;
      l[n++] = 0;
      l[n++] = 0;
      f = new Int32Array(16);
      for (n = 0;n < c;) {
        for (d = 0;16 > d;++d, n += 4) {
          f[d] = l[n] | l[n + 1] << 8 | l[n + 2] << 16 | l[n + 3] << 24;
        }
        var s = a;
        q = h;
        var u = p, v = m, g, k;
        for (d = 0;64 > d;++d) {
          16 > d ? (g = q & u | ~q & v, k = d) : 32 > d ? (g = v & q | ~v & u, k = 5 * d + 1 & 15) : 48 > d ? (g = q ^ u ^ v, k = 3 * d + 5 & 15) : (g = u ^ (q | ~v), k = 7 * d & 15);
          var r = v, s = s + g + b[d] + f[k] | 0;
          g = e[d];
          v = u;
          u = q;
          q = q + (s << g | s >>> 32 - g) | 0;
          s = r;
        }
        a = a + s | 0;
        h = h + q | 0;
        p = p + u | 0;
        m = m + v | 0;
      }
      return a;
    };
    d.hashBytesTo32BitsAdler = function(b, q, e) {
      var d = 1, a = 0;
      for (e = q + e;q < e;++q) {
        d = (d + (b[q] & 255)) % 65521, a = (a + d) % 65521;
      }
      return a << 16 | d;
    };
  })(k.HashUtilities || (k.HashUtilities = {}));
  var p = function() {
    function d() {
    }
    d.seed = function(e) {
      d._state[0] = e;
      d._state[1] = e;
    };
    d.next = function() {
      var e = this._state, b = Math.imul(18273, e[0] & 65535) + (e[0] >>> 16) | 0;
      e[0] = b;
      var f = Math.imul(36969, e[1] & 65535) + (e[1] >>> 16) | 0;
      e[1] = f;
      e = (b << 16) + (f & 65535) | 0;
      return 2.3283064365386963E-10 * (0 > e ? e + 4294967296 : e);
    };
    d._state = new Uint32Array([57005, 48879]);
    return d;
  }();
  k.Random = p;
  Math.random = function() {
    return p.next();
  };
  (function() {
    function d() {
      this.id = "$weakmap" + e++;
    }
    if ("function" !== typeof jsGlobal.WeakMap) {
      var e = 0;
      d.prototype = {has:function(b) {
        return b.hasOwnProperty(this.id);
      }, get:function(b, f) {
        return b.hasOwnProperty(this.id) ? b[this.id] : f;
      }, set:function(b, f) {
        Object.defineProperty(b, this.id, {value:f, enumerable:!1, configurable:!0});
      }, delete:function(b) {
        delete b[this.id];
      }};
      jsGlobal.WeakMap = d;
    }
  })();
  a = function() {
    function d() {
      "undefined" !== typeof netscape && netscape.security.PrivilegeManager ? this._map = new WeakMap : this._list = [];
    }
    d.prototype.clear = function() {
      this._map ? this._map.clear() : this._list.length = 0;
    };
    d.prototype.push = function(e) {
      this._map ? this._map.set(e, null) : this._list.push(e);
    };
    d.prototype.remove = function(e) {
      this._map ? this._map.delete(e) : this._list[this._list.indexOf(e)] = null;
    };
    d.prototype.forEach = function(e) {
      if (this._map) {
        "undefined" !== typeof netscape && netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect"), Components.utils.nondeterministicGetWeakMapKeys(this._map).forEach(function(b) {
          0 !== b._referenceCount && e(b);
        });
      } else {
        for (var b = this._list, f = 0, q = 0;q < b.length;q++) {
          var d = b[q];
          d && (0 === d._referenceCount ? (b[q] = null, f++) : e(d));
        }
        if (16 < f && f > b.length >> 2) {
          f = [];
          for (q = 0;q < b.length;q++) {
            (d = b[q]) && 0 < d._referenceCount && f.push(d);
          }
          this._list = f;
        }
      }
    };
    Object.defineProperty(d.prototype, "length", {get:function() {
      return this._map ? -1 : this._list.length;
    }, enumerable:!0, configurable:!0});
    return d;
  }();
  k.WeakList = a;
  var l;
  (function(d) {
    d.pow2 = function(e) {
      return e === (e | 0) ? 0 > e ? 1 / (1 << -e) : 1 << e : Math.pow(2, e);
    };
    d.clamp = function(e, b, f) {
      return Math.max(b, Math.min(f, e));
    };
    d.roundHalfEven = function(e) {
      if (.5 === Math.abs(e % 1)) {
        var b = Math.floor(e);
        return 0 === b % 2 ? b : Math.ceil(e);
      }
      return Math.round(e);
    };
    d.altTieBreakRound = function(e, b) {
      return.5 !== Math.abs(e % 1) || b ? Math.round(e) : e | 0;
    };
    d.epsilonEquals = function(e, b) {
      return 1E-7 > Math.abs(e - b);
    };
  })(l = k.NumberUtilities || (k.NumberUtilities = {}));
  (function(d) {
    d[d.MaxU16 = 65535] = "MaxU16";
    d[d.MaxI16 = 32767] = "MaxI16";
    d[d.MinI16 = -32768] = "MinI16";
  })(k.Numbers || (k.Numbers = {}));
  var u;
  (function(d) {
    function e(b) {
      return 256 * b << 16 >> 16;
    }
    var b = new ArrayBuffer(8);
    d.i8 = new Int8Array(b);
    d.u8 = new Uint8Array(b);
    d.i32 = new Int32Array(b);
    d.f32 = new Float32Array(b);
    d.f64 = new Float64Array(b);
    d.nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
    d.floatToInt32 = function(b) {
      d.f32[0] = b;
      return d.i32[0];
    };
    d.int32ToFloat = function(b) {
      d.i32[0] = b;
      return d.f32[0];
    };
    d.swap16 = function(b) {
      return(b & 255) << 8 | b >> 8 & 255;
    };
    d.swap32 = function(b) {
      return(b & 255) << 24 | (b & 65280) << 8 | b >> 8 & 65280 | b >> 24 & 255;
    };
    d.toS8U8 = e;
    d.fromS8U8 = function(b) {
      return b / 256;
    };
    d.clampS8U8 = function(b) {
      return e(b) / 256;
    };
    d.toS16 = function(b) {
      return b << 16 >> 16;
    };
    d.bitCount = function(b) {
      b -= b >> 1 & 1431655765;
      b = (b & 858993459) + (b >> 2 & 858993459);
      return 16843009 * (b + (b >> 4) & 252645135) >> 24;
    };
    d.ones = function(b) {
      b -= b >> 1 & 1431655765;
      b = (b & 858993459) + (b >> 2 & 858993459);
      return 16843009 * (b + (b >> 4) & 252645135) >> 24;
    };
    d.trailingZeros = function(b) {
      return d.ones((b & -b) - 1);
    };
    d.getFlags = function(b, q) {
      var e = "";
      for (b = 0;b < q.length;b++) {
        b & 1 << b && (e += q[b] + " ");
      }
      return 0 === e.length ? "" : e.trim();
    };
    d.isPowerOfTwo = function(b) {
      return b && 0 === (b & b - 1);
    };
    d.roundToMultipleOfFour = function(b) {
      return b + 3 & -4;
    };
    d.nearestPowerOfTwo = function(b) {
      b--;
      b |= b >> 1;
      b |= b >> 2;
      b |= b >> 4;
      b |= b >> 8;
      b |= b >> 16;
      b++;
      return b;
    };
    d.roundToMultipleOfPowerOfTwo = function(b, q) {
      var e = (1 << q) - 1;
      return b + e & ~e;
    };
    Math.imul || (Math.imul = function(b, q) {
      var e = b & 65535, d = q & 65535;
      return e * d + ((b >>> 16 & 65535) * d + e * (q >>> 16 & 65535) << 16 >>> 0) | 0;
    });
    Math.clz32 || (Math.clz32 = function(b) {
      b |= b >> 1;
      b |= b >> 2;
      b |= b >> 4;
      b |= b >> 8;
      return 32 - d.ones(b | b >> 16);
    });
  })(u = k.IntegerUtilities || (k.IntegerUtilities = {}));
  (function(d) {
    function e(b, f, q, e, d, a) {
      return(q - b) * (a - f) - (e - f) * (d - b);
    }
    d.pointInPolygon = function(b, f, q) {
      for (var e = 0, d = q.length - 2, a = 0;a < d;a += 2) {
        var h = q[a + 0], p = q[a + 1], m = q[a + 2], l = q[a + 3];
        (p <= f && l > f || p > f && l <= f) && b < h + (f - p) / (l - p) * (m - h) && e++;
      }
      return 1 === (e & 1);
    };
    d.signedArea = e;
    d.counterClockwise = function(b, f, q, d, a, h) {
      return 0 < e(b, f, q, d, a, h);
    };
    d.clockwise = function(b, f, q, d, a, h) {
      return 0 > e(b, f, q, d, a, h);
    };
    d.pointInPolygonInt32 = function(b, f, q) {
      b |= 0;
      f |= 0;
      for (var e = 0, d = q.length - 2, a = 0;a < d;a += 2) {
        var h = q[a + 0], p = q[a + 1], m = q[a + 2], l = q[a + 3];
        (p <= f && l > f || p > f && l <= f) && b < h + (f - p) / (l - p) * (m - h) && e++;
      }
      return 1 === (e & 1);
    };
  })(k.GeometricUtilities || (k.GeometricUtilities = {}));
  (function(d) {
    d[d.Error = 1] = "Error";
    d[d.Warn = 2] = "Warn";
    d[d.Debug = 4] = "Debug";
    d[d.Log = 8] = "Log";
    d[d.Info = 16] = "Info";
    d[d.All = 31] = "All";
  })(k.LogLevel || (k.LogLevel = {}));
  a = function() {
    function d(e, b) {
      void 0 === e && (e = !1);
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = e;
      this._out = b || d._consoleOut;
      this._outNoNewline = b || d._consoleOutNoNewline;
    }
    d.prototype.write = function(e, b) {
      void 0 === e && (e = "");
      void 0 === b && (b = !1);
      this._suppressOutput || this._outNoNewline((b ? this._padding : "") + e);
    };
    d.prototype.writeLn = function(e) {
      void 0 === e && (e = "");
      this._suppressOutput || this._out(this._padding + e);
    };
    d.prototype.writeObject = function(e, b) {
      void 0 === e && (e = "");
      this._suppressOutput || this._out(this._padding + e, b);
    };
    d.prototype.writeTimeLn = function(e) {
      void 0 === e && (e = "");
      this._suppressOutput || this._out(this._padding + performance.now().toFixed(2) + " " + e);
    };
    d.prototype.writeComment = function(e) {
      e = e.split("\n");
      if (1 === e.length) {
        this.writeLn("// " + e[0]);
      } else {
        this.writeLn("/**");
        for (var b = 0;b < e.length;b++) {
          this.writeLn(" * " + e[b]);
        }
        this.writeLn(" */");
      }
    };
    d.prototype.writeLns = function(e) {
      e = e.split("\n");
      for (var b = 0;b < e.length;b++) {
        this.writeLn(e[b]);
      }
    };
    d.prototype.errorLn = function(e) {
      d.logLevel & 1 && this.boldRedLn(e);
    };
    d.prototype.warnLn = function(e) {
      d.logLevel & 2 && this.yellowLn(e);
    };
    d.prototype.debugLn = function(e) {
      d.logLevel & 4 && this.purpleLn(e);
    };
    d.prototype.logLn = function(e) {
      d.logLevel & 8 && this.writeLn(e);
    };
    d.prototype.infoLn = function(e) {
      d.logLevel & 16 && this.writeLn(e);
    };
    d.prototype.yellowLn = function(e) {
      this.colorLn(d.YELLOW, e);
    };
    d.prototype.greenLn = function(e) {
      this.colorLn(d.GREEN, e);
    };
    d.prototype.boldRedLn = function(e) {
      this.colorLn(d.BOLD_RED, e);
    };
    d.prototype.redLn = function(e) {
      this.colorLn(d.RED, e);
    };
    d.prototype.purpleLn = function(e) {
      this.colorLn(d.PURPLE, e);
    };
    d.prototype.colorLn = function(e, b) {
      this._suppressOutput || (inBrowser ? this._out(this._padding + b) : this._out(this._padding + e + b + d.ENDC));
    };
    d.prototype.redLns = function(e) {
      this.colorLns(d.RED, e);
    };
    d.prototype.colorLns = function(e, b) {
      for (var f = b.split("\n"), q = 0;q < f.length;q++) {
        this.colorLn(e, f[q]);
      }
    };
    d.prototype.enter = function(e) {
      this._suppressOutput || this._out(this._padding + e);
      this.indent();
    };
    d.prototype.leaveAndEnter = function(e) {
      this.leave(e);
      this.indent();
    };
    d.prototype.leave = function(e) {
      this.outdent();
      !this._suppressOutput && e && this._out(this._padding + e);
    };
    d.prototype.indent = function() {
      this._padding += this._tab;
    };
    d.prototype.outdent = function() {
      0 < this._padding.length && (this._padding = this._padding.substring(0, this._padding.length - this._tab.length));
    };
    d.prototype.writeArray = function(e, b, f) {
      void 0 === b && (b = !1);
      void 0 === f && (f = !1);
      b = b || !1;
      for (var q = 0, d = e.length;q < d;q++) {
        var a = "";
        b && (a = null === e[q] ? "null" : void 0 === e[q] ? "undefined" : e[q].constructor.name, a += " ");
        var h = f ? "" : ("" + q).padRight(" ", 4);
        this.writeLn(h + a + e[q]);
      }
    };
    d.PURPLE = "\u001b[94m";
    d.YELLOW = "\u001b[93m";
    d.GREEN = "\u001b[92m";
    d.RED = "\u001b[91m";
    d.BOLD_RED = "\u001b[1;91m";
    d.ENDC = "\u001b[0m";
    d.logLevel = 31;
    d._consoleOut = console.info.bind(console);
    d._consoleOutNoNewline = console.info.bind(console);
    return d;
  }();
  k.IndentingWriter = a;
  var m = function() {
    return function(d, e) {
      this.value = d;
      this.next = e;
    };
  }(), a = function() {
    function d(e) {
      this._compare = e;
      this._head = null;
      this._length = 0;
    }
    d.prototype.push = function(e) {
      this._length++;
      if (this._head) {
        var b = this._head, f = null;
        e = new m(e, null);
        for (var q = this._compare;b;) {
          if (0 < q(b.value, e.value)) {
            f ? (e.next = b, f.next = e) : (e.next = this._head, this._head = e);
            return;
          }
          f = b;
          b = b.next;
        }
        f.next = e;
      } else {
        this._head = new m(e, null);
      }
    };
    d.prototype.forEach = function(e) {
      for (var b = this._head, f = null;b;) {
        var q = e(b.value);
        if (q === d.RETURN) {
          break;
        } else {
          q === d.DELETE ? b = f ? f.next = b.next : this._head = this._head.next : (f = b, b = b.next);
        }
      }
    };
    d.prototype.isEmpty = function() {
      return!this._head;
    };
    d.prototype.pop = function() {
      if (this._head) {
        this._length--;
        var e = this._head;
        this._head = this._head.next;
        return e.value;
      }
    };
    d.prototype.contains = function(e) {
      for (var b = this._head;b;) {
        if (b.value === e) {
          return!0;
        }
        b = b.next;
      }
      return!1;
    };
    d.prototype.toString = function() {
      for (var e = "[", b = this._head;b;) {
        e += b.value.toString(), (b = b.next) && (e += ",");
      }
      return e + "]";
    };
    d.RETURN = 1;
    d.DELETE = 2;
    return d;
  }();
  k.SortedList = a;
  a = function() {
    function d(e, b) {
      void 0 === b && (b = 12);
      this.start = this.index = 0;
      this._size = 1 << b;
      this._mask = this._size - 1;
      this.array = new e(this._size);
    }
    d.prototype.get = function(e) {
      return this.array[e];
    };
    d.prototype.forEachInReverse = function(e) {
      if (!this.isEmpty()) {
        for (var b = 0 === this.index ? this._size - 1 : this.index - 1, f = this.start - 1 & this._mask;b !== f && !e(this.array[b], b);) {
          b = 0 === b ? this._size - 1 : b - 1;
        }
      }
    };
    d.prototype.write = function(e) {
      this.array[this.index] = e;
      this.index = this.index + 1 & this._mask;
      this.index === this.start && (this.start = this.start + 1 & this._mask);
    };
    d.prototype.isFull = function() {
      return(this.index + 1 & this._mask) === this.start;
    };
    d.prototype.isEmpty = function() {
      return this.index === this.start;
    };
    d.prototype.reset = function() {
      this.start = this.index = 0;
    };
    return d;
  }();
  k.CircularBuffer = a;
  (function(d) {
    function e(b) {
      return b + (d.BITS_PER_WORD - 1) >> d.ADDRESS_BITS_PER_WORD << d.ADDRESS_BITS_PER_WORD;
    }
    function b(b, f) {
      b = b || "1";
      f = f || "0";
      for (var q = "", e = 0;e < length;e++) {
        q += this.get(e) ? b : f;
      }
      return q;
    }
    function f(b) {
      for (var f = [], q = 0;q < length;q++) {
        this.get(q) && f.push(b ? b[q] : q);
      }
      return f.join(", ");
    }
    d.ADDRESS_BITS_PER_WORD = 5;
    d.BITS_PER_WORD = 1 << d.ADDRESS_BITS_PER_WORD;
    d.BIT_INDEX_MASK = d.BITS_PER_WORD - 1;
    var q = function() {
      function b(f) {
        this.size = e(f);
        this.dirty = this.count = 0;
        this.length = f;
        this.bits = new Uint32Array(this.size >> d.ADDRESS_BITS_PER_WORD);
      }
      b.prototype.recount = function() {
        if (this.dirty) {
          for (var b = this.bits, f = 0, q = 0, e = b.length;q < e;q++) {
            var d = b[q], d = d - (d >> 1 & 1431655765), d = (d & 858993459) + (d >> 2 & 858993459), f = f + (16843009 * (d + (d >> 4) & 252645135) >> 24)
          }
          this.count = f;
          this.dirty = 0;
        }
      };
      b.prototype.set = function(b) {
        var f = b >> d.ADDRESS_BITS_PER_WORD, q = this.bits[f];
        b = q | 1 << (b & d.BIT_INDEX_MASK);
        this.bits[f] = b;
        this.dirty |= q ^ b;
      };
      b.prototype.setAll = function() {
        for (var b = this.bits, f = 0, q = b.length;f < q;f++) {
          b[f] = 4294967295;
        }
        this.count = this.size;
        this.dirty = 0;
      };
      b.prototype.assign = function(b) {
        this.count = b.count;
        this.dirty = b.dirty;
        this.size = b.size;
        for (var f = 0, q = this.bits.length;f < q;f++) {
          this.bits[f] = b.bits[f];
        }
      };
      b.prototype.clear = function(b) {
        var f = b >> d.ADDRESS_BITS_PER_WORD, q = this.bits[f];
        b = q & ~(1 << (b & d.BIT_INDEX_MASK));
        this.bits[f] = b;
        this.dirty |= q ^ b;
      };
      b.prototype.get = function(b) {
        return 0 !== (this.bits[b >> d.ADDRESS_BITS_PER_WORD] & 1 << (b & d.BIT_INDEX_MASK));
      };
      b.prototype.clearAll = function() {
        for (var b = this.bits, f = 0, q = b.length;f < q;f++) {
          b[f] = 0;
        }
        this.dirty = this.count = 0;
      };
      b.prototype._union = function(b) {
        var f = this.dirty, q = this.bits;
        b = b.bits;
        for (var e = 0, d = q.length;e < d;e++) {
          var a = q[e], w = a | b[e];
          q[e] = w;
          f |= a ^ w;
        }
        this.dirty = f;
      };
      b.prototype.intersect = function(b) {
        var f = this.dirty, q = this.bits;
        b = b.bits;
        for (var e = 0, d = q.length;e < d;e++) {
          var a = q[e], w = a & b[e];
          q[e] = w;
          f |= a ^ w;
        }
        this.dirty = f;
      };
      b.prototype.subtract = function(b) {
        var f = this.dirty, q = this.bits;
        b = b.bits;
        for (var e = 0, d = q.length;e < d;e++) {
          var a = q[e], w = a & ~b[e];
          q[e] = w;
          f |= a ^ w;
        }
        this.dirty = f;
      };
      b.prototype.negate = function() {
        for (var b = this.dirty, f = this.bits, q = 0, e = f.length;q < e;q++) {
          var d = f[q], a = ~d;
          f[q] = a;
          b |= d ^ a;
        }
        this.dirty = b;
      };
      b.prototype.forEach = function(b) {
        for (var f = this.bits, q = 0, e = f.length;q < e;q++) {
          var a = f[q];
          if (a) {
            for (var w = 0;w < d.BITS_PER_WORD;w++) {
              a & 1 << w && b(q * d.BITS_PER_WORD + w);
            }
          }
        }
      };
      b.prototype.toArray = function() {
        for (var b = [], f = this.bits, q = 0, e = f.length;q < e;q++) {
          var a = f[q];
          if (a) {
            for (var w = 0;w < d.BITS_PER_WORD;w++) {
              a & 1 << w && b.push(q * d.BITS_PER_WORD + w);
            }
          }
        }
        return b;
      };
      b.prototype.equals = function(b) {
        if (this.size !== b.size) {
          return!1;
        }
        var f = this.bits;
        b = b.bits;
        for (var q = 0, e = f.length;q < e;q++) {
          if (f[q] !== b[q]) {
            return!1;
          }
        }
        return!0;
      };
      b.prototype.contains = function(b) {
        if (this.size !== b.size) {
          return!1;
        }
        var f = this.bits;
        b = b.bits;
        for (var q = 0, e = f.length;q < e;q++) {
          if ((f[q] | b[q]) !== f[q]) {
            return!1;
          }
        }
        return!0;
      };
      b.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      b.prototype.clone = function() {
        var f = new b(this.length);
        f._union(this);
        return f;
      };
      return b;
    }();
    d.Uint32ArrayBitSet = q;
    var a = function() {
      function b(f) {
        this.dirty = this.count = 0;
        this.size = e(f);
        this.bits = 0;
        this.singleWord = !0;
        this.length = f;
      }
      b.prototype.recount = function() {
        if (this.dirty) {
          var b = this.bits, b = b - (b >> 1 & 1431655765), b = (b & 858993459) + (b >> 2 & 858993459);
          this.count = 0 + (16843009 * (b + (b >> 4) & 252645135) >> 24);
          this.dirty = 0;
        }
      };
      b.prototype.set = function(b) {
        var f = this.bits;
        this.bits = b = f | 1 << (b & d.BIT_INDEX_MASK);
        this.dirty |= f ^ b;
      };
      b.prototype.setAll = function() {
        this.bits = 4294967295;
        this.count = this.size;
        this.dirty = 0;
      };
      b.prototype.assign = function(b) {
        this.count = b.count;
        this.dirty = b.dirty;
        this.size = b.size;
        this.bits = b.bits;
      };
      b.prototype.clear = function(b) {
        var f = this.bits;
        this.bits = b = f & ~(1 << (b & d.BIT_INDEX_MASK));
        this.dirty |= f ^ b;
      };
      b.prototype.get = function(b) {
        return 0 !== (this.bits & 1 << (b & d.BIT_INDEX_MASK));
      };
      b.prototype.clearAll = function() {
        this.dirty = this.count = this.bits = 0;
      };
      b.prototype._union = function(b) {
        var f = this.bits;
        this.bits = b = f | b.bits;
        this.dirty = f ^ b;
      };
      b.prototype.intersect = function(b) {
        var f = this.bits;
        this.bits = b = f & b.bits;
        this.dirty = f ^ b;
      };
      b.prototype.subtract = function(b) {
        var f = this.bits;
        this.bits = b = f & ~b.bits;
        this.dirty = f ^ b;
      };
      b.prototype.negate = function() {
        var b = this.bits, f = ~b;
        this.bits = f;
        this.dirty = b ^ f;
      };
      b.prototype.forEach = function(b) {
        var f = this.bits;
        if (f) {
          for (var q = 0;q < d.BITS_PER_WORD;q++) {
            f & 1 << q && b(q);
          }
        }
      };
      b.prototype.toArray = function() {
        var b = [], f = this.bits;
        if (f) {
          for (var q = 0;q < d.BITS_PER_WORD;q++) {
            f & 1 << q && b.push(q);
          }
        }
        return b;
      };
      b.prototype.equals = function(b) {
        return this.bits === b.bits;
      };
      b.prototype.contains = function(b) {
        var f = this.bits;
        return(f | b.bits) === f;
      };
      b.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      b.prototype.clone = function() {
        var f = new b(this.length);
        f._union(this);
        return f;
      };
      return b;
    }();
    d.Uint32BitSet = a;
    a.prototype.toString = f;
    a.prototype.toBitString = b;
    q.prototype.toString = f;
    q.prototype.toBitString = b;
    d.BitSetFunctor = function(b) {
      var f = 1 === e(b) >> d.ADDRESS_BITS_PER_WORD ? a : q;
      return function() {
        return new f(b);
      };
    };
  })(k.BitSets || (k.BitSets = {}));
  a = function() {
    function d() {
    }
    d.randomStyle = function() {
      d._randomStyleCache || (d._randomStyleCache = "#ff5e3a #ff9500 #ffdb4c #87fc70 #52edc7 #1ad6fd #c644fc #ef4db6 #4a4a4a #dbddde #ff3b30 #ff9500 #ffcc00 #4cd964 #34aadc #007aff #5856d6 #ff2d55 #8e8e93 #c7c7cc #5ad427 #c86edf #d1eefc #e0f8d8 #fb2b69 #f7f7f7 #1d77ef #d6cec3 #55efcb #ff4981 #ffd3e0 #f7f7f7 #ff1300 #1f1f21 #bdbec2 #ff3a2d".split(" "));
      return d._randomStyleCache[d._nextStyle++ % d._randomStyleCache.length];
    };
    d.gradientColor = function(e) {
      return d._gradient[d._gradient.length * l.clamp(e, 0, 1) | 0];
    };
    d.contrastStyle = function(e) {
      e = parseInt(e.substr(1), 16);
      return 128 <= (299 * (e >> 16) + 587 * (e >> 8 & 255) + 114 * (e & 255)) / 1E3 ? "#000000" : "#ffffff";
    };
    d.reset = function() {
      d._nextStyle = 0;
    };
    d.TabToolbar = "#252c33";
    d.Toolbars = "#343c45";
    d.HighlightBlue = "#1d4f73";
    d.LightText = "#f5f7fa";
    d.ForegroundText = "#b6babf";
    d.Black = "#000000";
    d.VeryDark = "#14171a";
    d.Dark = "#181d20";
    d.Light = "#a9bacb";
    d.Grey = "#8fa1b2";
    d.DarkGrey = "#5f7387";
    d.Blue = "#46afe3";
    d.Purple = "#6b7abb";
    d.Pink = "#df80ff";
    d.Red = "#eb5368";
    d.Orange = "#d96629";
    d.LightOrange = "#d99b28";
    d.Green = "#70bf53";
    d.BlueGrey = "#5e88b0";
    d._nextStyle = 0;
    d._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
    return d;
  }();
  k.ColorStyle = a;
  a = function() {
    function d(e, b, f, q) {
      this.xMin = e | 0;
      this.yMin = b | 0;
      this.xMax = f | 0;
      this.yMax = q | 0;
    }
    d.FromUntyped = function(e) {
      return new d(e.xMin, e.yMin, e.xMax, e.yMax);
    };
    d.FromRectangle = function(e) {
      return new d(20 * e.x | 0, 20 * e.y | 0, 20 * (e.x + e.width) | 0, 20 * (e.y + e.height) | 0);
    };
    d.prototype.setElements = function(e, b, f, q) {
      this.xMin = e;
      this.yMin = b;
      this.xMax = f;
      this.yMax = q;
    };
    d.prototype.copyFrom = function(e) {
      this.setElements(e.xMin, e.yMin, e.xMax, e.yMax);
    };
    d.prototype.contains = function(e, b) {
      return e < this.xMin !== e < this.xMax && b < this.yMin !== b < this.yMax;
    };
    d.prototype.unionInPlace = function(e) {
      e.isEmpty() || (this.extendByPoint(e.xMin, e.yMin), this.extendByPoint(e.xMax, e.yMax));
    };
    d.prototype.extendByPoint = function(e, b) {
      this.extendByX(e);
      this.extendByY(b);
    };
    d.prototype.extendByX = function(e) {
      134217728 === this.xMin ? this.xMin = this.xMax = e : (this.xMin = Math.min(this.xMin, e), this.xMax = Math.max(this.xMax, e));
    };
    d.prototype.extendByY = function(e) {
      134217728 === this.yMin ? this.yMin = this.yMax = e : (this.yMin = Math.min(this.yMin, e), this.yMax = Math.max(this.yMax, e));
    };
    d.prototype.intersects = function(e) {
      return this.contains(e.xMin, e.yMin) || this.contains(e.xMax, e.yMax);
    };
    d.prototype.isEmpty = function() {
      return this.xMax <= this.xMin || this.yMax <= this.yMin;
    };
    Object.defineProperty(d.prototype, "width", {get:function() {
      return this.xMax - this.xMin;
    }, set:function(e) {
      this.xMax = this.xMin + e;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(d.prototype, "height", {get:function() {
      return this.yMax - this.yMin;
    }, set:function(e) {
      this.yMax = this.yMin + e;
    }, enumerable:!0, configurable:!0});
    d.prototype.getBaseWidth = function(e) {
      return Math.abs(Math.cos(e)) * (this.xMax - this.xMin) + Math.abs(Math.sin(e)) * (this.yMax - this.yMin);
    };
    d.prototype.getBaseHeight = function(e) {
      return Math.abs(Math.sin(e)) * (this.xMax - this.xMin) + Math.abs(Math.cos(e)) * (this.yMax - this.yMin);
    };
    d.prototype.setEmpty = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 0;
    };
    d.prototype.setToSentinels = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 134217728;
    };
    d.prototype.clone = function() {
      return new d(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    d.prototype.toString = function() {
      return "{ xMin: " + this.xMin + ", xMin: " + this.yMin + ", xMax: " + this.xMax + ", xMax: " + this.yMax + " }";
    };
    return d;
  }();
  k.Bounds = a;
  a = function() {
    function d(e, b, f, q) {
      n.assert(r(e));
      n.assert(r(b));
      n.assert(r(f));
      n.assert(r(q));
      this._xMin = e | 0;
      this._yMin = b | 0;
      this._xMax = f | 0;
      this._yMax = q | 0;
    }
    d.FromUntyped = function(e) {
      return new d(e.xMin, e.yMin, e.xMax, e.yMax);
    };
    d.FromRectangle = function(e) {
      return new d(20 * e.x | 0, 20 * e.y | 0, 20 * (e.x + e.width) | 0, 20 * (e.y + e.height) | 0);
    };
    d.prototype.setElements = function(e, b, f, q) {
      this.xMin = e;
      this.yMin = b;
      this.xMax = f;
      this.yMax = q;
    };
    d.prototype.copyFrom = function(e) {
      this.setElements(e.xMin, e.yMin, e.xMax, e.yMax);
    };
    d.prototype.contains = function(e, b) {
      return e < this.xMin !== e < this.xMax && b < this.yMin !== b < this.yMax;
    };
    d.prototype.unionInPlace = function(e) {
      e.isEmpty() || (this.extendByPoint(e.xMin, e.yMin), this.extendByPoint(e.xMax, e.yMax));
    };
    d.prototype.extendByPoint = function(e, b) {
      this.extendByX(e);
      this.extendByY(b);
    };
    d.prototype.extendByX = function(e) {
      134217728 === this.xMin ? this.xMin = this.xMax = e : (this.xMin = Math.min(this.xMin, e), this.xMax = Math.max(this.xMax, e));
    };
    d.prototype.extendByY = function(e) {
      134217728 === this.yMin ? this.yMin = this.yMax = e : (this.yMin = Math.min(this.yMin, e), this.yMax = Math.max(this.yMax, e));
    };
    d.prototype.intersects = function(e) {
      return this.contains(e._xMin, e._yMin) || this.contains(e._xMax, e._yMax);
    };
    d.prototype.isEmpty = function() {
      return this._xMax <= this._xMin || this._yMax <= this._yMin;
    };
    Object.defineProperty(d.prototype, "xMin", {get:function() {
      return this._xMin;
    }, set:function(e) {
      n.assert(r(e));
      this._xMin = e;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(d.prototype, "yMin", {get:function() {
      return this._yMin;
    }, set:function(e) {
      n.assert(r(e));
      this._yMin = e | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(d.prototype, "xMax", {get:function() {
      return this._xMax;
    }, set:function(e) {
      n.assert(r(e));
      this._xMax = e | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(d.prototype, "width", {get:function() {
      return this._xMax - this._xMin;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(d.prototype, "yMax", {get:function() {
      return this._yMax;
    }, set:function(e) {
      n.assert(r(e));
      this._yMax = e | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(d.prototype, "height", {get:function() {
      return this._yMax - this._yMin;
    }, enumerable:!0, configurable:!0});
    d.prototype.getBaseWidth = function(e) {
      return Math.abs(Math.cos(e)) * (this._xMax - this._xMin) + Math.abs(Math.sin(e)) * (this._yMax - this._yMin);
    };
    d.prototype.getBaseHeight = function(e) {
      return Math.abs(Math.sin(e)) * (this._xMax - this._xMin) + Math.abs(Math.cos(e)) * (this._yMax - this._yMin);
    };
    d.prototype.setEmpty = function() {
      this._xMin = this._yMin = this._xMax = this._yMax = 0;
    };
    d.prototype.clone = function() {
      return new d(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    d.prototype.toString = function() {
      return "{ xMin: " + this._xMin + ", xMin: " + this._yMin + ", xMax: " + this._xMax + ", yMax: " + this._yMax + " }";
    };
    d.prototype.assertValid = function() {
    };
    return d;
  }();
  k.DebugBounds = a;
  a = function() {
    function d(e, b, f, q) {
      this.r = e;
      this.g = b;
      this.b = f;
      this.a = q;
    }
    d.FromARGB = function(e) {
      return new d((e >> 16 & 255) / 255, (e >> 8 & 255) / 255, (e >> 0 & 255) / 255, (e >> 24 & 255) / 255);
    };
    d.FromRGBA = function(e) {
      return d.FromARGB(s.RGBAToARGB(e));
    };
    d.prototype.toRGBA = function() {
      return 255 * this.r << 24 | 255 * this.g << 16 | 255 * this.b << 8 | 255 * this.a;
    };
    d.prototype.toCSSStyle = function() {
      return s.rgbaToCSSStyle(this.toRGBA());
    };
    d.prototype.set = function(e) {
      this.r = e.r;
      this.g = e.g;
      this.b = e.b;
      this.a = e.a;
    };
    d.randomColor = function() {
      var e = .4;
      void 0 === e && (e = 1);
      return new d(Math.random(), Math.random(), Math.random(), e);
    };
    d.parseColor = function(e) {
      d.colorCache || (d.colorCache = Object.create(null));
      if (d.colorCache[e]) {
        return d.colorCache[e];
      }
      var b = document.createElement("span");
      document.body.appendChild(b);
      b.style.backgroundColor = e;
      var f = getComputedStyle(b).backgroundColor;
      document.body.removeChild(b);
      (b = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(f)) || (b = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(f));
      f = new d(0, 0, 0, 0);
      f.r = parseFloat(b[1]) / 255;
      f.g = parseFloat(b[2]) / 255;
      f.b = parseFloat(b[3]) / 255;
      f.a = b[4] ? parseFloat(b[4]) / 255 : 1;
      return d.colorCache[e] = f;
    };
    d.Red = new d(1, 0, 0, 1);
    d.Green = new d(0, 1, 0, 1);
    d.Blue = new d(0, 0, 1, 1);
    d.None = new d(0, 0, 0, 0);
    d.White = new d(1, 1, 1, 1);
    d.Black = new d(0, 0, 0, 1);
    d.colorCache = {};
    return d;
  }();
  k.Color = a;
  var s;
  (function(d) {
    function e(b) {
      var f, e, d = b >> 24 & 255;
      e = (Math.imul(b >> 16 & 255, d) + 127) / 255 | 0;
      f = (Math.imul(b >> 8 & 255, d) + 127) / 255 | 0;
      b = (Math.imul(b >> 0 & 255, d) + 127) / 255 | 0;
      return d << 24 | e << 16 | f << 8 | b;
    }
    d.RGBAToARGB = function(b) {
      return b >> 8 & 16777215 | (b & 255) << 24;
    };
    d.ARGBToRGBA = function(b) {
      return b << 8 | b >> 24 & 255;
    };
    d.rgbaToCSSStyle = function(b) {
      return k.StringUtilities.concat9("rgba(", b >> 24 & 255, ",", b >> 16 & 255, ",", b >> 8 & 255, ",", (b & 255) / 255, ")");
    };
    d.cssStyleToRGBA = function(b) {
      if ("#" === b[0]) {
        if (7 === b.length) {
          return parseInt(b.substring(1), 16) << 8 | 255;
        }
      } else {
        if ("r" === b[0]) {
          return b = b.substring(5, b.length - 1).split(","), (parseInt(b[0]) & 255) << 24 | (parseInt(b[1]) & 255) << 16 | (parseInt(b[2]) & 255) << 8 | 255 * parseFloat(b[3]) & 255;
        }
      }
      return 4278190335;
    };
    d.hexToRGB = function(b) {
      return parseInt(b.slice(1), 16);
    };
    d.rgbToHex = function(b) {
      return "#" + ("000000" + (b >>> 0).toString(16)).slice(-6);
    };
    d.isValidHexColor = function(b) {
      return/^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(b);
    };
    d.clampByte = function(b) {
      return Math.max(0, Math.min(255, b));
    };
    d.unpremultiplyARGB = function(b) {
      var f, e, d = b >> 24 & 255;
      e = Math.imul(255, b >> 16 & 255) / d & 255;
      f = Math.imul(255, b >> 8 & 255) / d & 255;
      b = Math.imul(255, b >> 0 & 255) / d & 255;
      return d << 24 | e << 16 | f << 8 | b;
    };
    d.premultiplyARGB = e;
    var b;
    d.ensureUnpremultiplyTable = function() {
      if (!b) {
        b = new Uint8Array(65536);
        for (var f = 0;256 > f;f++) {
          for (var e = 0;256 > e;e++) {
            b[(e << 8) + f] = Math.imul(255, f) / e;
          }
        }
      }
    };
    d.tableLookupUnpremultiplyARGB = function(f) {
      f |= 0;
      var e = f >> 24 & 255;
      if (0 === e) {
        return 0;
      }
      if (255 === e) {
        return f;
      }
      var d, a, h = e << 8, p = b;
      a = p[h + (f >> 16 & 255)];
      d = p[h + (f >> 8 & 255)];
      f = p[h + (f >> 0 & 255)];
      return e << 24 | a << 16 | d << 8 | f;
    };
    d.blendPremultipliedBGRA = function(b, f) {
      var e, d;
      d = 256 - (f & 255);
      e = Math.imul(b & 16711935, d) >> 8;
      d = Math.imul(b >> 8 & 16711935, d) >> 8;
      return((f >> 8 & 16711935) + d & 16711935) << 8 | (f & 16711935) + e & 16711935;
    };
    var f = u.swap32;
    d.convertImage = function(q, d, a, h) {
      var p = a.length;
      if (q === d) {
        if (a !== h) {
          for (q = 0;q < p;q++) {
            h[q] = a[q];
          }
        }
      } else {
        if (1 === q && 3 === d) {
          for (k.ColorUtilities.ensureUnpremultiplyTable(), q = 0;q < p;q++) {
            var m = a[q];
            d = m & 255;
            if (0 === d) {
              h[q] = 0;
            } else {
              if (255 === d) {
                h[q] = 4278190080 | m >> 8 & 16777215;
              } else {
                var l = m >> 24 & 255, c = m >> 16 & 255, m = m >> 8 & 255, s = d << 8, u = b, m = u[s + m], c = u[s + c], l = u[s + l];
                h[q] = d << 24 | l << 16 | c << 8 | m;
              }
            }
          }
        } else {
          if (2 === q && 3 === d) {
            for (q = 0;q < p;q++) {
              h[q] = f(a[q]);
            }
          } else {
            if (3 === q && 1 === d) {
              for (q = 0;q < p;q++) {
                d = a[q], h[q] = f(e(d & 4278255360 | d >> 16 & 255 | (d & 255) << 16));
              }
            } else {
              for (n.somewhatImplemented("Image Format Conversion: " + v[q] + " -> " + v[d]), q = 0;q < p;q++) {
                h[q] = a[q];
              }
            }
          }
        }
      }
    };
  })(s = k.ColorUtilities || (k.ColorUtilities = {}));
  a = function() {
    function d(e) {
      void 0 === e && (e = 32);
      this._list = [];
      this._maxSize = e;
    }
    d.prototype.acquire = function(e) {
      if (d._enabled) {
        for (var b = this._list, f = 0;f < b.length;f++) {
          var q = b[f];
          if (q.byteLength >= e) {
            return b.splice(f, 1), q;
          }
        }
      }
      return new ArrayBuffer(e);
    };
    d.prototype.release = function(e) {
      if (d._enabled) {
        var b = this._list;
        b.length === this._maxSize && b.shift();
        b.push(e);
      }
    };
    d.prototype.ensureUint8ArrayLength = function(e, b) {
      if (e.length >= b) {
        return e;
      }
      var f = Math.max(e.length + b, (3 * e.length >> 1) + 1), f = new Uint8Array(this.acquire(f), 0, f);
      f.set(e);
      this.release(e.buffer);
      return f;
    };
    d.prototype.ensureFloat64ArrayLength = function(e, b) {
      if (e.length >= b) {
        return e;
      }
      var f = Math.max(e.length + b, (3 * e.length >> 1) + 1), f = new Float64Array(this.acquire(f * Float64Array.BYTES_PER_ELEMENT), 0, f);
      f.set(e);
      this.release(e.buffer);
      return f;
    };
    d._enabled = !0;
    return d;
  }();
  k.ArrayBufferPool = a;
  (function(d) {
    (function(e) {
      e[e.EXTERNAL_INTERFACE_FEATURE = 1] = "EXTERNAL_INTERFACE_FEATURE";
      e[e.CLIPBOARD_FEATURE = 2] = "CLIPBOARD_FEATURE";
      e[e.SHAREDOBJECT_FEATURE = 3] = "SHAREDOBJECT_FEATURE";
      e[e.VIDEO_FEATURE = 4] = "VIDEO_FEATURE";
      e[e.SOUND_FEATURE = 5] = "SOUND_FEATURE";
      e[e.NETCONNECTION_FEATURE = 6] = "NETCONNECTION_FEATURE";
    })(d.Feature || (d.Feature = {}));
    (function(e) {
      e[e.AVM1_ERROR = 1] = "AVM1_ERROR";
      e[e.AVM2_ERROR = 2] = "AVM2_ERROR";
    })(d.ErrorTypes || (d.ErrorTypes = {}));
    d.instance;
  })(k.Telemetry || (k.Telemetry = {}));
  (function(d) {
    d.instance;
  })(k.FileLoadingService || (k.FileLoadingService = {}));
  (function(d) {
    d[d.BuiltinAbc = 0] = "BuiltinAbc";
    d[d.PlayerglobalAbcs = 1] = "PlayerglobalAbcs";
    d[d.PlayerglobalManifest = 2] = "PlayerglobalManifest";
    d[d.ShellAbc = 3] = "ShellAbc";
  })(k.SystemResourceId || (k.SystemResourceId = {}));
  (function(d) {
    d.instance;
  })(k.SystemResourcesLoadingService || (k.SystemResourcesLoadingService = {}));
  k.registerCSSFont = function(d, e, b) {
    if (inBrowser) {
      var f = document.head;
      f.insertBefore(document.createElement("style"), f.firstChild);
      f = document.styleSheets[0];
      e = "@font-face{font-family:swffont" + d + ";src:url(data:font/opentype;base64," + k.StringUtilities.base64ArrayBuffer(e) + ")}";
      f.insertRule(e, f.cssRules.length);
      b && (b = document.createElement("div"), b.style.fontFamily = "swffont" + d, b.innerHTML = "hello", document.body.appendChild(b), document.body.removeChild(b));
    }
  };
  (function(d) {
    d.instance = {enabled:!1, initJS:function(e) {
    }, registerCallback:function(e) {
    }, unregisterCallback:function(e) {
    }, eval:function(e) {
    }, call:function(e) {
    }, getId:function() {
      return null;
    }};
  })(k.ExternalInterfaceService || (k.ExternalInterfaceService = {}));
  (function(d) {
    d.instance = {setClipboard:function(e) {
    }};
  })(k.ClipboardService || (k.ClipboardService = {}));
  a = function() {
    function d() {
      this._queues = {};
    }
    d.prototype.register = function(e, b) {
      n.assert(e);
      n.assert(b);
      var f = this._queues[e];
      if (f) {
        if (-1 < f.indexOf(b)) {
          return;
        }
      } else {
        f = this._queues[e] = [];
      }
      f.push(b);
    };
    d.prototype.unregister = function(e, b) {
      n.assert(e);
      n.assert(b);
      var f = this._queues[e];
      if (f) {
        var q = f.indexOf(b);
        -1 !== q && f.splice(q, 1);
        0 === f.length && (this._queues[e] = null);
      }
    };
    d.prototype.notify = function(e, b) {
      var f = this._queues[e];
      if (f) {
        f = f.slice();
        b = Array.prototype.slice.call(arguments, 0);
        for (var q = 0;q < f.length;q++) {
          f[q].apply(null, b);
        }
      }
    };
    d.prototype.notify1 = function(e, b) {
      var f = this._queues[e];
      if (f) {
        for (var f = f.slice(), q = 0;q < f.length;q++) {
          (0,f[q])(e, b);
        }
      }
    };
    return d;
  }();
  k.Callback = a;
  (function(d) {
    d[d.None = 0] = "None";
    d[d.PremultipliedAlphaARGB = 1] = "PremultipliedAlphaARGB";
    d[d.StraightAlphaARGB = 2] = "StraightAlphaARGB";
    d[d.StraightAlphaRGBA = 3] = "StraightAlphaRGBA";
    d[d.JPEG = 4] = "JPEG";
    d[d.PNG = 5] = "PNG";
    d[d.GIF = 6] = "GIF";
  })(k.ImageType || (k.ImageType = {}));
  var v = k.ImageType;
  k.getMIMETypeForImageType = function(d) {
    switch(d) {
      case 4:
        return "image/jpeg";
      case 5:
        return "image/png";
      case 6:
        return "image/gif";
      default:
        return "text/plain";
    }
  };
  (function(d) {
    d.toCSSCursor = function(e) {
      switch(e) {
        case 0:
          return "auto";
        case 2:
          return "pointer";
        case 3:
          return "grab";
        case 4:
          return "text";
        default:
          return "default";
      }
    };
  })(k.UI || (k.UI = {}));
  a = function() {
    function d() {
      this.promise = new Promise(function(e, b) {
        this.resolve = e;
        this.reject = b;
      }.bind(this));
    }
    d.prototype.then = function(e, b) {
      return this.promise.then(e, b);
    };
    return d;
  }();
  k.PromiseWrapper = a;
})(Shumway || (Shumway = {}));
(function() {
  function k(b) {
    if ("function" !== typeof b) {
      throw new TypeError("Invalid deferred constructor");
    }
    var f = s();
    b = new b(f);
    var e = f.resolve;
    if ("function" !== typeof e) {
      throw new TypeError("Invalid resolve construction function");
    }
    f = f.reject;
    if ("function" !== typeof f) {
      throw new TypeError("Invalid reject construction function");
    }
    return{promise:b, resolve:e, reject:f};
  }
  function r(b, f) {
    if ("object" !== typeof b || null === b) {
      return!1;
    }
    try {
      var e = b.then;
      if ("function" !== typeof e) {
        return!1;
      }
      e.call(b, f.resolve, f.reject);
    } catch (d) {
      e = f.reject, e(d);
    }
    return!0;
  }
  function g(b) {
    return "object" === typeof b && null !== b && "undefined" !== typeof b.promiseStatus;
  }
  function c(b, f) {
    if ("unresolved" === b.promiseStatus) {
      var e = b.rejectReactions;
      b.result = f;
      b.resolveReactions = void 0;
      b.rejectReactions = void 0;
      b.promiseStatus = "has-rejection";
      t(e, f);
    }
  }
  function t(b, f) {
    for (var e = 0;e < b.length;e++) {
      n({reaction:b[e], argument:f});
    }
  }
  function n(b) {
    0 === f.length && setTimeout(h, 0);
    f.push(b);
  }
  function a(b, f) {
    var e = b.deferred, d = b.handler, a, h;
    try {
      a = d(f);
    } catch (p) {
      return e = e.reject, e(p);
    }
    if (a === e.promise) {
      return e = e.reject, e(new TypeError("Self resolution"));
    }
    try {
      if (h = r(a, e), !h) {
        var m = e.resolve;
        return m(a);
      }
    } catch (l) {
      return e = e.reject, e(l);
    }
  }
  function h() {
    for (;0 < f.length;) {
      var b = f[0];
      try {
        a(b.reaction, b.argument);
      } catch (d) {
        if ("function" === typeof e.onerror) {
          e.onerror(d);
        }
      }
      f.shift();
    }
  }
  function p(b) {
    throw b;
  }
  function l(b) {
    return b;
  }
  function u(b) {
    return function(f) {
      c(b, f);
    };
  }
  function m(b) {
    return function(f) {
      if ("unresolved" === b.promiseStatus) {
        var e = b.resolveReactions;
        b.result = f;
        b.resolveReactions = void 0;
        b.rejectReactions = void 0;
        b.promiseStatus = "has-resolution";
        t(e, f);
      }
    };
  }
  function s() {
    function b(f, e) {
      b.resolve = f;
      b.reject = e;
    }
    return b;
  }
  function v(b, f, e) {
    return function(d) {
      if (d === b) {
        return e(new TypeError("Self resolution"));
      }
      var a = b.promiseConstructor;
      if (g(d) && d.promiseConstructor === a) {
        return d.then(f, e);
      }
      a = k(a);
      return r(d, a) ? a.promise.then(f, e) : f(d);
    };
  }
  function d(b, f, e, d) {
    return function(a) {
      f[b] = a;
      d.countdown--;
      0 === d.countdown && e.resolve(f);
    };
  }
  function e(b) {
    if ("function" !== typeof b) {
      throw new TypeError("resolver is not a function");
    }
    if ("object" !== typeof this) {
      throw new TypeError("Promise to initialize is not an object");
    }
    this.promiseStatus = "unresolved";
    this.resolveReactions = [];
    this.rejectReactions = [];
    this.result = void 0;
    var f = m(this), d = u(this);
    try {
      b(f, d);
    } catch (a) {
      c(this, a);
    }
    this.promiseConstructor = e;
    return this;
  }
  var b = Function("return this")();
  if (b.Promise) {
    "function" !== typeof b.Promise.all && (b.Promise.all = function(f) {
      var e = 0, d = [], a, h, p = new b.Promise(function(b, f) {
        a = b;
        h = f;
      });
      f.forEach(function(b, f) {
        e++;
        b.then(function(b) {
          d[f] = b;
          e--;
          0 === e && a(d);
        }, h);
      });
      0 === e && a(d);
      return p;
    }), "function" !== typeof b.Promise.resolve && (b.Promise.resolve = function(f) {
      return new b.Promise(function(b) {
        b(f);
      });
    });
  } else {
    var f = [];
    e.all = function(b) {
      var f = k(this), e = [], a = {countdown:0}, h = 0;
      b.forEach(function(b) {
        this.cast(b).then(d(h, e, f, a), f.reject);
        h++;
        a.countdown++;
      }, this);
      0 === h && f.resolve(e);
      return f.promise;
    };
    e.cast = function(b) {
      if (g(b)) {
        return b;
      }
      var f = k(this);
      f.resolve(b);
      return f.promise;
    };
    e.reject = function(b) {
      var f = k(this);
      f.reject(b);
      return f.promise;
    };
    e.resolve = function(b) {
      var f = k(this);
      f.resolve(b);
      return f.promise;
    };
    e.prototype = {"catch":function(b) {
      this.then(void 0, b);
    }, then:function(b, f) {
      if (!g(this)) {
        throw new TypeError("this is not a Promises");
      }
      var e = k(this.promiseConstructor), d = "function" === typeof f ? f : p, a = {deferred:e, handler:v(this, "function" === typeof b ? b : l, d)}, d = {deferred:e, handler:d};
      switch(this.promiseStatus) {
        case "unresolved":
          this.resolveReactions.push(a);
          this.rejectReactions.push(d);
          break;
        case "has-resolution":
          n({reaction:a, argument:this.result});
          break;
        case "has-rejection":
          n({reaction:d, argument:this.result});
      }
      return e.promise;
    }};
    b.Promise = e;
  }
})();
"undefined" !== typeof exports && (exports.Shumway = Shumway);
(function() {
  function k(k, g, c) {
    k[g] || Object.defineProperty(k, g, {value:c, writable:!0, configurable:!0, enumerable:!1});
  }
  k(String.prototype, "padRight", function(k, g) {
    var c = this, t = c.replace(/\033\[[0-9]*m/g, "").length;
    if (!k || t >= g) {
      return c;
    }
    for (var t = (g - t) / k.length, n = 0;n < t;n++) {
      c += k;
    }
    return c;
  });
  k(String.prototype, "padLeft", function(k, g) {
    var c = this, t = c.length;
    if (!k || t >= g) {
      return c;
    }
    for (var t = (g - t) / k.length, n = 0;n < t;n++) {
      c = k + c;
    }
    return c;
  });
  k(String.prototype, "trim", function() {
    return this.replace(/^\s+|\s+$/g, "");
  });
  k(String.prototype, "endsWith", function(k) {
    return-1 !== this.indexOf(k, this.length - k.length);
  });
  k(Array.prototype, "replace", function(k, g) {
    if (k === g) {
      return 0;
    }
    for (var c = 0, t = 0;t < this.length;t++) {
      this[t] === k && (this[t] = g, c++);
    }
    return c;
  });
})();
(function(k) {
  (function(r) {
    var g = k.isObject, c = function() {
      function a(a, p, l, c) {
        this.shortName = a;
        this.longName = p;
        this.type = l;
        c = c || {};
        this.positional = c.positional;
        this.parseFn = c.parse;
        this.value = c.defaultValue;
      }
      a.prototype.parse = function(a) {
        this.value = "boolean" === this.type ? a : "number" === this.type ? parseInt(a, 10) : a;
        this.parseFn && this.parseFn(this.value);
      };
      return a;
    }();
    r.Argument = c;
    var t = function() {
      function a() {
        this.args = [];
      }
      a.prototype.addArgument = function(a, p, l, n) {
        a = new c(a, p, l, n);
        this.args.push(a);
        return a;
      };
      a.prototype.addBoundOption = function(a) {
        this.args.push(new c(a.shortName, a.longName, a.type, {parse:function(p) {
          a.value = p;
        }}));
      };
      a.prototype.addBoundOptionSet = function(a) {
        var p = this;
        a.options.forEach(function(a) {
          a instanceof n ? p.addBoundOptionSet(a) : p.addBoundOption(a);
        });
      };
      a.prototype.getUsage = function() {
        var a = "";
        this.args.forEach(function(p) {
          a = p.positional ? a + p.longName : a + ("[-" + p.shortName + "|--" + p.longName + ("boolean" === p.type ? "" : " " + p.type[0].toUpperCase()) + "]");
          a += " ";
        });
        return a;
      };
      a.prototype.parse = function(a) {
        var p = {}, l = [];
        this.args.forEach(function(d) {
          d.positional ? l.push(d) : (p["-" + d.shortName] = d, p["--" + d.longName] = d);
        });
        for (var c = [];a.length;) {
          var m = a.shift(), n = null, v = m;
          if ("--" == m) {
            c = c.concat(a);
            break;
          } else {
            if ("-" == m.slice(0, 1) || "--" == m.slice(0, 2)) {
              n = p[m];
              if (!n) {
                continue;
              }
              v = "boolean" !== n.type ? a.shift() : !0;
            } else {
              l.length ? n = l.shift() : c.push(v);
            }
          }
          n && n.parse(v);
        }
        return c;
      };
      return a;
    }();
    r.ArgumentParser = t;
    var n = function() {
      function a(a, p) {
        void 0 === p && (p = null);
        this.open = !1;
        this.name = a;
        this.settings = p || {};
        this.options = [];
      }
      a.prototype.register = function(h) {
        if (h instanceof a) {
          for (var p = 0;p < this.options.length;p++) {
            var c = this.options[p];
            if (c instanceof a && c.name === h.name) {
              return c;
            }
          }
        }
        this.options.push(h);
        if (this.settings) {
          if (h instanceof a) {
            p = this.settings[h.name], g(p) && (h.settings = p.settings, h.open = p.open);
          } else {
            if ("undefined" !== typeof this.settings[h.longName]) {
              switch(h.type) {
                case "boolean":
                  h.value = !!this.settings[h.longName];
                  break;
                case "number":
                  h.value = +this.settings[h.longName];
                  break;
                default:
                  h.value = this.settings[h.longName];
              }
            }
          }
        }
        return h;
      };
      a.prototype.trace = function(a) {
        a.enter(this.name + " {");
        this.options.forEach(function(p) {
          p.trace(a);
        });
        a.leave("}");
      };
      a.prototype.getSettings = function() {
        var h = {};
        this.options.forEach(function(p) {
          p instanceof a ? h[p.name] = {settings:p.getSettings(), open:p.open} : h[p.longName] = p.value;
        });
        return h;
      };
      a.prototype.setSettings = function(h) {
        h && this.options.forEach(function(p) {
          p instanceof a ? p.name in h && p.setSettings(h[p.name].settings) : p.longName in h && (p.value = h[p.longName]);
        });
      };
      return a;
    }();
    r.OptionSet = n;
    t = function() {
      function a(a, p, c, n, m, s) {
        void 0 === s && (s = null);
        this.longName = p;
        this.shortName = a;
        this.type = c;
        this.value = this.defaultValue = n;
        this.description = m;
        this.config = s;
      }
      a.prototype.parse = function(a) {
        this.value = a;
      };
      a.prototype.trace = function(a) {
        a.writeLn(("-" + this.shortName + "|--" + this.longName).padRight(" ", 30) + " = " + this.type + " " + this.value + " [" + this.defaultValue + "] (" + this.description + ")");
      };
      return a;
    }();
    r.Option = t;
  })(k.Options || (k.Options = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    r.ROOT = "Shumway Options";
    r.shumwayOptions = new k.Options.OptionSet(r.ROOT);
    r.setSettings = function(g) {
      r.shumwayOptions.setSettings(g);
    };
    r.getSettings = function() {
      return r.shumwayOptions.getSettings();
    };
  })(k.Settings || (k.Settings = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = function() {
      function c(c, n) {
        this._parent = c;
        this._timers = k.ObjectUtilities.createMap();
        this._name = n;
        this._count = this._total = this._last = this._begin = 0;
      }
      c.time = function(g, n) {
        c.start(g);
        n();
        c.stop();
      };
      c.start = function(g) {
        c._top = c._top._timers[g] || (c._top._timers[g] = new c(c._top, g));
        c._top.start();
        g = c._flat._timers[g] || (c._flat._timers[g] = new c(c._flat, g));
        g.start();
        c._flatStack.push(g);
      };
      c.stop = function() {
        c._top.stop();
        c._top = c._top._parent;
        c._flatStack.pop().stop();
      };
      c.stopStart = function(g) {
        c.stop();
        c.start(g);
      };
      c.prototype.start = function() {
        this._begin = k.getTicks();
      };
      c.prototype.stop = function() {
        this._last = k.getTicks() - this._begin;
        this._total += this._last;
        this._count += 1;
      };
      c.prototype.toJSON = function() {
        return{name:this._name, total:this._total, timers:this._timers};
      };
      c.prototype.trace = function(c) {
        c.enter(this._name + ": " + this._total.toFixed(2) + " ms, count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var n in this._timers) {
          this._timers[n].trace(c);
        }
        c.outdent();
      };
      c.trace = function(g) {
        c._base.trace(g);
        c._flat.trace(g);
      };
      c._base = new c(null, "Total");
      c._top = c._base;
      c._flat = new c(null, "Flat");
      c._flatStack = [];
      return c;
    }();
    r.Timer = g;
    g = function() {
      function c(c) {
        this._enabled = c;
        this.clear();
      }
      Object.defineProperty(c.prototype, "counts", {get:function() {
        return this._counts;
      }, enumerable:!0, configurable:!0});
      c.prototype.setEnabled = function(c) {
        this._enabled = c;
      };
      c.prototype.clear = function() {
        this._counts = k.ObjectUtilities.createMap();
        this._times = k.ObjectUtilities.createMap();
      };
      c.prototype.toJSON = function() {
        return{counts:this._counts, times:this._times};
      };
      c.prototype.count = function(c, n, a) {
        void 0 === n && (n = 1);
        void 0 === a && (a = 0);
        if (this._enabled) {
          return void 0 === this._counts[c] && (this._counts[c] = 0, this._times[c] = 0), this._counts[c] += n, this._times[c] += a, this._counts[c];
        }
      };
      c.prototype.trace = function(c) {
        for (var n in this._counts) {
          c.writeLn(n + ": " + this._counts[n]);
        }
      };
      c.prototype._pairToString = function(c, n) {
        var a = n[0], h = n[1], p = c[a], a = a + ": " + h;
        p && (a += ", " + p.toFixed(4), 1 < h && (a += " (" + (p / h).toFixed(4) + ")"));
        return a;
      };
      c.prototype.toStringSorted = function() {
        var c = this, n = this._times, a = [], h;
        for (h in this._counts) {
          a.push([h, this._counts[h]]);
        }
        a.sort(function(a, h) {
          return h[1] - a[1];
        });
        return a.map(function(a) {
          return c._pairToString(n, a);
        }).join(", ");
      };
      c.prototype.traceSorted = function(c, n) {
        void 0 === n && (n = !1);
        var a = this, h = this._times, p = [], l;
        for (l in this._counts) {
          p.push([l, this._counts[l]]);
        }
        p.sort(function(a, h) {
          return h[1] - a[1];
        });
        n ? c.writeLn(p.map(function(p) {
          return a._pairToString(h, p);
        }).join(", ")) : p.forEach(function(p) {
          c.writeLn(a._pairToString(h, p));
        });
      };
      c.instance = new c(!0);
      return c;
    }();
    r.Counter = g;
    g = function() {
      function c(c) {
        this._samples = new Float64Array(c);
        this._index = this._count = 0;
      }
      c.prototype.push = function(c) {
        this._count < this._samples.length && this._count++;
        this._index++;
        this._samples[this._index % this._samples.length] = c;
      };
      c.prototype.average = function() {
        for (var c = 0, n = 0;n < this._count;n++) {
          c += this._samples[n];
        }
        return c / this._count;
      };
      return c;
    }();
    r.Average = g;
  })(k.Metrics || (k.Metrics = {}));
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function(k, r) {
  function g() {
    this.constructor = k;
  }
  for (var c in r) {
    r.hasOwnProperty(c) && (k[c] = r[c]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(k) {
    function g(b) {
      for (var f = Math.max.apply(null, b), q = b.length, e = 1 << f, d = new Uint32Array(e), a = f << 16 | 65535, h = 0;h < e;h++) {
        d[h] = a;
      }
      for (var a = 0, h = 1, p = 2;h <= f;a <<= 1, ++h, p <<= 1) {
        for (var c = 0;c < q;++c) {
          if (b[c] === h) {
            for (var m = 0, l = 0;l < h;++l) {
              m = 2 * m + (a >> l & 1);
            }
            for (l = m;l < e;l += p) {
              d[l] = h << 16 | c;
            }
            ++a;
          }
        }
      }
      return{codes:d, maxBits:f};
    }
    var c;
    (function(b) {
      b[b.INIT = 0] = "INIT";
      b[b.BLOCK_0 = 1] = "BLOCK_0";
      b[b.BLOCK_1 = 2] = "BLOCK_1";
      b[b.BLOCK_2_PRE = 3] = "BLOCK_2_PRE";
      b[b.BLOCK_2 = 4] = "BLOCK_2";
      b[b.DONE = 5] = "DONE";
      b[b.ERROR = 6] = "ERROR";
      b[b.VERIFY_HEADER = 7] = "VERIFY_HEADER";
    })(c || (c = {}));
    c = function() {
      function b(b) {
      }
      b.prototype.push = function(b) {
      };
      b.prototype.close = function() {
      };
      b.create = function(b) {
        return "undefined" !== typeof ShumwayCom && ShumwayCom.createSpecialInflate ? new v(b, ShumwayCom.createSpecialInflate) : new t(b);
      };
      b.prototype._processZLibHeader = function(b, q, e) {
        if (q + 2 > e) {
          return 0;
        }
        b = b[q] << 8 | b[q + 1];
        q = null;
        2048 !== (b & 3840) ? q = "inflate: unknown compression method" : 0 !== b % 31 ? q = "inflate: bad FCHECK" : 0 !== (b & 32) && (q = "inflate: FDICT bit set");
        if (q) {
          if (this.onError) {
            this.onError(q);
          }
          return-1;
        }
        return 2;
      };
      b.inflate = function(f, q, e) {
        var d = new Uint8Array(q), a = 0;
        q = b.create(e);
        q.onData = function(b) {
          var f = Math.min(b.length, d.length - a);
          f && k.memCopy(d, b, a, 0, f);
          a += f;
        };
        q.onError = function(b) {
          throw Error(b);
        };
        q.push(f);
        q.close();
        return d;
      };
      return b;
    }();
    k.Inflate = c;
    var t = function(b) {
      function f(f) {
        b.call(this, f);
        this._buffer = null;
        this._bitLength = this._bitBuffer = this._bufferPosition = this._bufferSize = 0;
        this._window = new Uint8Array(65794);
        this._windowPosition = 0;
        this._state = f ? 7 : 0;
        this._isFinalBlock = !1;
        this._distanceTable = this._literalTable = null;
        this._block0Read = 0;
        this._block2State = null;
        this._copyState = {state:0, len:0, lenBits:0, dist:0, distBits:0};
        if (!s) {
          n = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);
          a = new Uint16Array(30);
          h = new Uint8Array(30);
          for (var e = f = 0, d = 1;30 > f;++f) {
            a[f] = d, d += 1 << (h[f] = ~~((e += 2 < f ? 1 : 0) / 2));
          }
          var c = new Uint8Array(288);
          for (f = 0;32 > f;++f) {
            c[f] = 5;
          }
          p = g(c.subarray(0, 32));
          l = new Uint16Array(29);
          u = new Uint8Array(29);
          e = f = 0;
          for (d = 3;29 > f;++f) {
            l[f] = d - (28 == f ? 1 : 0), d += 1 << (u[f] = ~~((e += 4 < f ? 1 : 0) / 4 % 6));
          }
          for (f = 0;288 > f;++f) {
            c[f] = 144 > f || 279 < f ? 8 : 256 > f ? 9 : 7;
          }
          m = g(c);
          s = !0;
        }
      }
      __extends(f, b);
      f.prototype.push = function(b) {
        if (!this._buffer || this._buffer.length < this._bufferSize + b.length) {
          var f = new Uint8Array(this._bufferSize + b.length);
          this._buffer && f.set(this._buffer);
          this._buffer = f;
        }
        this._buffer.set(b, this._bufferSize);
        this._bufferSize += b.length;
        this._bufferPosition = 0;
        b = !1;
        do {
          f = this._windowPosition;
          if (0 === this._state && (b = this._decodeInitState())) {
            break;
          }
          switch(this._state) {
            case 1:
              b = this._decodeBlock0();
              break;
            case 3:
              if (b = this._decodeBlock2Pre()) {
                break;
              }
            ;
            case 2:
            ;
            case 4:
              b = this._decodeBlock();
              break;
            case 6:
            ;
            case 5:
              this._bufferPosition = this._bufferSize;
              break;
            case 7:
              var e = this._processZLibHeader(this._buffer, this._bufferPosition, this._bufferSize);
              0 < e ? (this._bufferPosition += e, this._state = 0) : 0 === e ? b = !0 : this._state = 6;
          }
          if (0 < this._windowPosition - f) {
            this.onData(this._window.subarray(f, this._windowPosition));
          }
          65536 <= this._windowPosition && ("copyWithin" in this._buffer ? this._window.copyWithin(0, this._windowPosition - 32768, this._windowPosition) : this._window.set(this._window.subarray(this._windowPosition - 32768, this._windowPosition)), this._windowPosition = 32768);
        } while (!b && this._bufferPosition < this._bufferSize);
        this._bufferPosition < this._bufferSize ? ("copyWithin" in this._buffer ? this._buffer.copyWithin(0, this._bufferPosition, this._bufferSize) : this._buffer.set(this._buffer.subarray(this._bufferPosition, this._bufferSize)), this._bufferSize -= this._bufferPosition) : this._bufferSize = 0;
      };
      f.prototype._decodeInitState = function() {
        if (this._isFinalBlock) {
          return this._state = 5, !1;
        }
        var b = this._buffer, f = this._bufferSize, e = this._bitBuffer, d = this._bitLength, a = this._bufferPosition;
        if (3 > (f - a << 3) + d) {
          return!0;
        }
        3 > d && (e |= b[a++] << d, d += 8);
        var h = e & 7, e = e >> 3, d = d - 3;
        switch(h >> 1) {
          case 0:
            d = e = 0;
            if (4 > f - a) {
              return!0;
            }
            var c = b[a] | b[a + 1] << 8, b = b[a + 2] | b[a + 3] << 8, a = a + 4;
            if (65535 !== (c ^ b)) {
              this._error("inflate: invalid block 0 length");
              b = 6;
              break;
            }
            0 === c ? b = 0 : (this._block0Read = c, b = 1);
            break;
          case 1:
            b = 2;
            this._literalTable = m;
            this._distanceTable = p;
            break;
          case 2:
            if (26 > (f - a << 3) + d) {
              return!0;
            }
            for (;14 > d;) {
              e |= b[a++] << d, d += 8;
            }
            c = (e >> 10 & 15) + 4;
            if ((f - a << 3) + d < 14 + 3 * c) {
              return!0;
            }
            for (var f = {numLiteralCodes:(e & 31) + 257, numDistanceCodes:(e >> 5 & 31) + 1, codeLengthTable:void 0, bitLengths:void 0, codesRead:0, dupBits:0}, e = e >> 14, d = d - 14, l = new Uint8Array(19), s = 0;s < c;++s) {
              3 > d && (e |= b[a++] << d, d += 8), l[n[s]] = e & 7, e >>= 3, d -= 3;
            }
            for (;19 > s;s++) {
              l[n[s]] = 0;
            }
            f.bitLengths = new Uint8Array(f.numLiteralCodes + f.numDistanceCodes);
            f.codeLengthTable = g(l);
            this._block2State = f;
            b = 3;
            break;
          default:
            return this._error("inflate: unsupported block type"), !1;
        }
        this._isFinalBlock = !!(h & 1);
        this._state = b;
        this._bufferPosition = a;
        this._bitBuffer = e;
        this._bitLength = d;
        return!1;
      };
      f.prototype._error = function(b) {
        if (this.onError) {
          this.onError(b);
        }
      };
      f.prototype._decodeBlock0 = function() {
        var b = this._bufferPosition, f = this._windowPosition, e = this._block0Read, d = 65794 - f, a = this._bufferSize - b, h = a < e, p = Math.min(d, a, e);
        this._window.set(this._buffer.subarray(b, b + p), f);
        this._windowPosition = f + p;
        this._bufferPosition = b + p;
        this._block0Read = e - p;
        return e === p ? (this._state = 0, !1) : h && d < a;
      };
      f.prototype._readBits = function(b) {
        var f = this._bitBuffer, e = this._bitLength;
        if (b > e) {
          var d = this._bufferPosition, a = this._bufferSize;
          do {
            if (d >= a) {
              return this._bufferPosition = d, this._bitBuffer = f, this._bitLength = e, -1;
            }
            f |= this._buffer[d++] << e;
            e += 8;
          } while (b > e);
          this._bufferPosition = d;
        }
        this._bitBuffer = f >> b;
        this._bitLength = e - b;
        return f & (1 << b) - 1;
      };
      f.prototype._readCode = function(b) {
        var f = this._bitBuffer, e = this._bitLength, d = b.maxBits;
        if (d > e) {
          var a = this._bufferPosition, h = this._bufferSize;
          do {
            if (a >= h) {
              return this._bufferPosition = a, this._bitBuffer = f, this._bitLength = e, -1;
            }
            f |= this._buffer[a++] << e;
            e += 8;
          } while (d > e);
          this._bufferPosition = a;
        }
        b = b.codes[f & (1 << d) - 1];
        d = b >> 16;
        if (b & 32768) {
          return this._error("inflate: invalid encoding"), this._state = 6, -1;
        }
        this._bitBuffer = f >> d;
        this._bitLength = e - d;
        return b & 65535;
      };
      f.prototype._decodeBlock2Pre = function() {
        var b = this._block2State, f = b.numLiteralCodes + b.numDistanceCodes, e = b.bitLengths, d = b.codesRead, a = 0 < d ? e[d - 1] : 0, h = b.codeLengthTable, p;
        if (0 < b.dupBits) {
          p = this._readBits(b.dupBits);
          if (0 > p) {
            return!0;
          }
          for (;p--;) {
            e[d++] = a;
          }
          b.dupBits = 0;
        }
        for (;d < f;) {
          var c = this._readCode(h);
          if (0 > c) {
            return b.codesRead = d, !0;
          }
          if (16 > c) {
            e[d++] = a = c;
          } else {
            var m;
            switch(c) {
              case 16:
                m = 2;
                p = 3;
                c = a;
                break;
              case 17:
                p = m = 3;
                c = 0;
                break;
              case 18:
                m = 7, p = 11, c = 0;
            }
            for (;p--;) {
              e[d++] = c;
            }
            p = this._readBits(m);
            if (0 > p) {
              return b.codesRead = d, b.dupBits = m, !0;
            }
            for (;p--;) {
              e[d++] = c;
            }
            a = c;
          }
        }
        this._literalTable = g(e.subarray(0, b.numLiteralCodes));
        this._distanceTable = g(e.subarray(b.numLiteralCodes));
        this._state = 4;
        this._block2State = null;
        return!1;
      };
      f.prototype._decodeBlock = function() {
        var b = this._literalTable, f = this._distanceTable, e = this._window, d = this._windowPosition, p = this._copyState, c, m, n, s;
        if (0 !== p.state) {
          switch(p.state) {
            case 1:
              if (0 > (c = this._readBits(p.lenBits))) {
                return!0;
              }
              p.len += c;
              p.state = 2;
            case 2:
              if (0 > (c = this._readCode(f))) {
                return!0;
              }
              p.distBits = h[c];
              p.dist = a[c];
              p.state = 3;
            case 3:
              c = 0;
              if (0 < p.distBits && 0 > (c = this._readBits(p.distBits))) {
                return!0;
              }
              s = p.dist + c;
              m = p.len;
              for (c = d - s;m--;) {
                e[d++] = e[c++];
              }
              p.state = 0;
              if (65536 <= d) {
                return this._windowPosition = d, !1;
              }
              break;
          }
        }
        do {
          c = this._readCode(b);
          if (0 > c) {
            return this._windowPosition = d, !0;
          }
          if (256 > c) {
            e[d++] = c;
          } else {
            if (256 < c) {
              this._windowPosition = d;
              c -= 257;
              n = u[c];
              m = l[c];
              c = 0 === n ? 0 : this._readBits(n);
              if (0 > c) {
                return p.state = 1, p.len = m, p.lenBits = n, !0;
              }
              m += c;
              c = this._readCode(f);
              if (0 > c) {
                return p.state = 2, p.len = m, !0;
              }
              n = h[c];
              s = a[c];
              c = 0 === n ? 0 : this._readBits(n);
              if (0 > c) {
                return p.state = 3, p.len = m, p.dist = s, p.distBits = n, !0;
              }
              s += c;
              for (c = d - s;m--;) {
                e[d++] = e[c++];
              }
            } else {
              this._state = 0;
              break;
            }
          }
        } while (65536 > d);
        this._windowPosition = d;
        return!1;
      };
      return f;
    }(c), n, a, h, p, l, u, m, s = !1, v = function(b) {
      function f(f, e) {
        b.call(this, f);
        this._verifyHeader = f;
        this._specialInflate = e();
        this._specialInflate.setDataCallback(function(b) {
          this.onData(b);
        }.bind(this));
      }
      __extends(f, b);
      f.prototype.push = function(b) {
        if (this._verifyHeader) {
          var f;
          this._buffer ? (f = new Uint8Array(this._buffer.length + b.length), f.set(this._buffer), f.set(b, this._buffer.length), this._buffer = null) : f = new Uint8Array(b);
          var e = this._processZLibHeader(f, 0, f.length);
          if (0 === e) {
            this._buffer = f;
            return;
          }
          this._verifyHeader = !0;
          0 < e && (b = f.subarray(e));
        }
        this._specialInflate.push(b);
      };
      f.prototype.close = function() {
        this._specialInflate && (this._specialInflate.close(), this._specialInflate = null);
      };
      return f;
    }(c), d;
    (function(b) {
      b[b.WRITE = 0] = "WRITE";
      b[b.DONE = 1] = "DONE";
      b[b.ZLIB_HEADER = 2] = "ZLIB_HEADER";
    })(d || (d = {}));
    var e = function() {
      function b() {
        this.a = 1;
        this.b = 0;
      }
      b.prototype.update = function(b, e, d) {
        for (var a = this.a, h = this.b;e < d;++e) {
          a = (a + (b[e] & 255)) % 65521, h = (h + a) % 65521;
        }
        this.a = a;
        this.b = h;
      };
      b.prototype.getChecksum = function() {
        return this.b << 16 | this.a;
      };
      return b;
    }();
    k.Adler32 = e;
    d = function() {
      function b(b) {
        this._state = (this._writeZlibHeader = b) ? 2 : 0;
        this._adler32 = b ? new e : null;
      }
      b.prototype.push = function(b) {
        2 === this._state && (this.onData(new Uint8Array([120, 156])), this._state = 0);
        for (var e = b.length, d = new Uint8Array(e + 5 * Math.ceil(e / 65535)), a = 0, h = 0;65535 < e;) {
          d.set(new Uint8Array([0, 255, 255, 0, 0]), a), a += 5, d.set(b.subarray(h, h + 65535), a), h += 65535, a += 65535, e -= 65535;
        }
        d.set(new Uint8Array([0, e & 255, e >> 8 & 255, ~e & 255, ~e >> 8 & 255]), a);
        d.set(b.subarray(h, e), a + 5);
        this.onData(d);
        this._adler32 && this._adler32.update(b, 0, e);
      };
      b.prototype.close = function() {
        this._state = 1;
        this.onData(new Uint8Array([1, 0, 0, 255, 255]));
        if (this._adler32) {
          var b = this._adler32.getChecksum();
          this.onData(new Uint8Array([b & 255, b >> 8 & 255, b >> 16 & 255, b >>> 24 & 255]));
        }
      };
      return b;
    }();
    k.Deflate = d;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    function g(b) {
      for (var e = new Uint16Array(b), d = 0;d < b;d++) {
        e[d] = 1024;
      }
      return e;
    }
    function c(b, e, d, a) {
      for (var h = 1, p = 0, c = 0;c < d;c++) {
        var m = a.decodeBit(b, h + e), h = (h << 1) + m, p = p | m << c
      }
      return p;
    }
    function t(b, e) {
      var d = [];
      d.length = e;
      for (var a = 0;a < e;a++) {
        d[a] = new l(b);
      }
      return d;
    }
    var n = function() {
      function b() {
        this.pos = this.available = 0;
        this.buffer = new Uint8Array(2E3);
      }
      b.prototype.append = function(b) {
        var f = this.pos + this.available, e = f + b.length;
        if (e > this.buffer.length) {
          for (var d = 2 * this.buffer.length;d < e;) {
            d *= 2;
          }
          e = new Uint8Array(d);
          e.set(this.buffer);
          this.buffer = e;
        }
        this.buffer.set(b, f);
        this.available += b.length;
      };
      b.prototype.compact = function() {
        0 !== this.available && (this.buffer.set(this.buffer.subarray(this.pos, this.pos + this.available), 0), this.pos = 0);
      };
      b.prototype.readByte = function() {
        if (0 >= this.available) {
          throw Error("Unexpected end of file");
        }
        this.available--;
        return this.buffer[this.pos++];
      };
      return b;
    }(), a = function() {
      function b(f) {
        this.onData = f;
        this.processed = 0;
      }
      b.prototype.writeBytes = function(b) {
        this.onData.call(null, b);
        this.processed += b.length;
      };
      return b;
    }(), h = function() {
      function b(f) {
        this.outStream = f;
        this.buf = null;
        this.size = this.pos = 0;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      }
      b.prototype.create = function(b) {
        this.buf = new Uint8Array(b);
        this.pos = 0;
        this.size = b;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      };
      b.prototype.putByte = function(b) {
        this.totalPos++;
        this.buf[this.pos++] = b;
        this.pos === this.size && (this.flush(), this.pos = 0, this.isFull = !0);
      };
      b.prototype.getByte = function(b) {
        return this.buf[b <= this.pos ? this.pos - b : this.size - b + this.pos];
      };
      b.prototype.flush = function() {
        this.writePos < this.pos && (this.outStream.writeBytes(this.buf.subarray(this.writePos, this.pos)), this.writePos = this.pos === this.size ? 0 : this.pos);
      };
      b.prototype.copyMatch = function(b, f) {
        for (var e = this.pos, d = this.size, a = this.buf, h = b <= e ? e - b : d - b + e, p = f;0 < p;) {
          for (var c = Math.min(Math.min(p, d - e), d - h), m = 0;m < c;m++) {
            var l = a[h++];
            a[e++] = l;
          }
          e === d && (this.pos = e, this.flush(), e = 0, this.isFull = !0);
          h === d && (h = 0);
          p -= c;
        }
        this.pos = e;
        this.totalPos += f;
      };
      b.prototype.checkDistance = function(b) {
        return b <= this.pos || this.isFull;
      };
      b.prototype.isEmpty = function() {
        return 0 === this.pos && !this.isFull;
      };
      return b;
    }(), p = function() {
      function b(f) {
        this.inStream = f;
        this.code = this.range = 0;
        this.corrupted = !1;
      }
      b.prototype.init = function() {
        0 !== this.inStream.readByte() && (this.corrupted = !0);
        this.range = -1;
        for (var b = 0, f = 0;4 > f;f++) {
          b = b << 8 | this.inStream.readByte();
        }
        b === this.range && (this.corrupted = !0);
        this.code = b;
      };
      b.prototype.isFinishedOK = function() {
        return 0 === this.code;
      };
      b.prototype.decodeDirectBits = function(b) {
        var f = 0, e = this.range, d = this.code;
        do {
          var e = e >>> 1 | 0, d = d - e | 0, a = d >> 31, d = d + (e & a) | 0;
          d === e && (this.corrupted = !0);
          0 <= e && 16777216 > e && (e <<= 8, d = d << 8 | this.inStream.readByte());
          f = (f << 1) + a + 1 | 0;
        } while (--b);
        this.range = e;
        this.code = d;
        return f;
      };
      b.prototype.decodeBit = function(b, f) {
        var e = this.range, d = this.code, a = b[f], h = (e >>> 11) * a;
        d >>> 0 < h ? (a = a + (2048 - a >> 5) | 0, e = h | 0, h = 0) : (a = a - (a >> 5) | 0, d = d - h | 0, e = e - h | 0, h = 1);
        b[f] = a & 65535;
        0 <= e && 16777216 > e && (e <<= 8, d = d << 8 | this.inStream.readByte());
        this.range = e;
        this.code = d;
        return h;
      };
      return b;
    }(), l = function() {
      function b(f) {
        this.numBits = f;
        this.probs = g(1 << f);
      }
      b.prototype.decode = function(b) {
        for (var f = 1, e = 0;e < this.numBits;e++) {
          f = (f << 1) + b.decodeBit(this.probs, f);
        }
        return f - (1 << this.numBits);
      };
      b.prototype.reverseDecode = function(b) {
        return c(this.probs, 0, this.numBits, b);
      };
      return b;
    }(), u = function() {
      function b() {
        this.choice = g(2);
        this.lowCoder = t(3, 16);
        this.midCoder = t(3, 16);
        this.highCoder = new l(8);
      }
      b.prototype.decode = function(b, f) {
        return 0 === b.decodeBit(this.choice, 0) ? this.lowCoder[f].decode(b) : 0 === b.decodeBit(this.choice, 1) ? 8 + this.midCoder[f].decode(b) : 16 + this.highCoder.decode(b);
      };
      return b;
    }(), m = function() {
      function b(f, e) {
        this.rangeDec = new p(f);
        this.outWindow = new h(e);
        this.markerIsMandatory = !1;
        this.dictSizeInProperties = this.dictSize = this.lp = this.pb = this.lc = 0;
        this.leftToUnpack = this.unpackSize = void 0;
        this.reps = new Int32Array(4);
        this.state = 0;
      }
      b.prototype.decodeProperties = function(b) {
        var f = b[0];
        if (225 <= f) {
          throw Error("Incorrect LZMA properties");
        }
        this.lc = f % 9;
        f = f / 9 | 0;
        this.pb = f / 5 | 0;
        this.lp = f % 5;
        for (f = this.dictSizeInProperties = 0;4 > f;f++) {
          this.dictSizeInProperties |= b[f + 1] << 8 * f;
        }
        this.dictSize = this.dictSizeInProperties;
        4096 > this.dictSize && (this.dictSize = 4096);
      };
      b.prototype.create = function() {
        this.outWindow.create(this.dictSize);
        this.init();
        this.rangeDec.init();
        this.reps[0] = 0;
        this.reps[1] = 0;
        this.reps[2] = 0;
        this.state = this.reps[3] = 0;
        this.leftToUnpack = this.unpackSize;
      };
      b.prototype.decodeLiteral = function(b, f) {
        var e = this.outWindow, d = this.rangeDec, a = 0;
        e.isEmpty() || (a = e.getByte(1));
        var h = 1, a = 768 * (((e.totalPos & (1 << this.lp) - 1) << this.lc) + (a >> 8 - this.lc));
        if (7 <= b) {
          e = e.getByte(f + 1);
          do {
            var p = e >> 7 & 1, e = e << 1, c = d.decodeBit(this.litProbs, a + ((1 + p << 8) + h)), h = h << 1 | c;
            if (p !== c) {
              break;
            }
          } while (256 > h);
        }
        for (;256 > h;) {
          h = h << 1 | d.decodeBit(this.litProbs, a + h);
        }
        return h - 256 & 255;
      };
      b.prototype.decodeDistance = function(b) {
        var f = b;
        3 < f && (f = 3);
        b = this.rangeDec;
        f = this.posSlotDecoder[f].decode(b);
        if (4 > f) {
          return f;
        }
        var e = (f >> 1) - 1, d = (2 | f & 1) << e;
        14 > f ? d = d + c(this.posDecoders, d - f, e, b) | 0 : (d = d + (b.decodeDirectBits(e - 4) << 4) | 0, d = d + this.alignDecoder.reverseDecode(b) | 0);
        return d;
      };
      b.prototype.init = function() {
        this.litProbs = g(768 << this.lc + this.lp);
        this.posSlotDecoder = t(6, 4);
        this.alignDecoder = new l(4);
        this.posDecoders = g(115);
        this.isMatch = g(192);
        this.isRep = g(12);
        this.isRepG0 = g(12);
        this.isRepG1 = g(12);
        this.isRepG2 = g(12);
        this.isRep0Long = g(192);
        this.lenDecoder = new u;
        this.repLenDecoder = new u;
      };
      b.prototype.decode = function(b) {
        for (var f = this.rangeDec, a = this.outWindow, h = this.pb, p = this.dictSize, c = this.markerIsMandatory, m = this.leftToUnpack, l = this.isMatch, n = this.isRep, u = this.isRepG0, g = this.isRepG1, k = this.isRepG2, r = this.isRep0Long, t = this.lenDecoder, y = this.repLenDecoder, z = this.reps[0], B = this.reps[1], x = this.reps[2], E = this.reps[3], A = this.state;;) {
          if (b && 48 > f.inStream.available) {
            this.outWindow.flush();
            break;
          }
          if (0 === m && !c && (this.outWindow.flush(), f.isFinishedOK())) {
            return d;
          }
          var D = a.totalPos & (1 << h) - 1;
          if (0 === f.decodeBit(l, (A << 4) + D)) {
            if (0 === m) {
              return s;
            }
            a.putByte(this.decodeLiteral(A, z));
            A = 4 > A ? 0 : 10 > A ? A - 3 : A - 6;
            m--;
          } else {
            if (0 !== f.decodeBit(n, A)) {
              if (0 === m || a.isEmpty()) {
                return s;
              }
              if (0 === f.decodeBit(u, A)) {
                if (0 === f.decodeBit(r, (A << 4) + D)) {
                  A = 7 > A ? 9 : 11;
                  a.putByte(a.getByte(z + 1));
                  m--;
                  continue;
                }
              } else {
                var F;
                0 === f.decodeBit(g, A) ? F = B : (0 === f.decodeBit(k, A) ? F = x : (F = E, E = x), x = B);
                B = z;
                z = F;
              }
              D = y.decode(f, D);
              A = 7 > A ? 8 : 11;
            } else {
              E = x;
              x = B;
              B = z;
              D = t.decode(f, D);
              A = 7 > A ? 7 : 10;
              z = this.decodeDistance(D);
              if (-1 === z) {
                return this.outWindow.flush(), f.isFinishedOK() ? v : s;
              }
              if (0 === m || z >= p || !a.checkDistance(z)) {
                return s;
              }
            }
            D += 2;
            F = !1;
            void 0 !== m && m < D && (D = m, F = !0);
            a.copyMatch(z + 1, D);
            m -= D;
            if (F) {
              return s;
            }
          }
        }
        this.state = A;
        this.reps[0] = z;
        this.reps[1] = B;
        this.reps[2] = x;
        this.reps[3] = E;
        this.leftToUnpack = m;
        return e;
      };
      b.prototype.flushOutput = function() {
        this.outWindow.flush();
      };
      return b;
    }(), s = 0, v = 1, d = 2, e = 3, b;
    (function(b) {
      b[b.WAIT_FOR_LZMA_HEADER = 0] = "WAIT_FOR_LZMA_HEADER";
      b[b.WAIT_FOR_SWF_HEADER = 1] = "WAIT_FOR_SWF_HEADER";
      b[b.PROCESS_DATA = 2] = "PROCESS_DATA";
      b[b.CLOSED = 3] = "CLOSED";
      b[b.ERROR = 4] = "ERROR";
    })(b || (b = {}));
    b = function() {
      function b(f) {
        void 0 === f && (f = !1);
        this._state = f ? 1 : 0;
        this.buffer = null;
      }
      b.prototype.push = function(b) {
        if (2 > this._state) {
          var f = this.buffer ? this.buffer.length : 0, d = (1 === this._state ? 17 : 13) + 5;
          if (f + b.length < d) {
            d = new Uint8Array(f + b.length);
            0 < f && d.set(this.buffer);
            d.set(b, f);
            this.buffer = d;
            return;
          }
          var h = new Uint8Array(d);
          0 < f && h.set(this.buffer);
          h.set(b.subarray(0, d - f), f);
          this._inStream = new n;
          this._inStream.append(h.subarray(d - 5));
          this._outStream = new a(function(b) {
            this.onData.call(null, b);
          }.bind(this));
          this._decoder = new m(this._inStream, this._outStream);
          if (1 === this._state) {
            this._decoder.decodeProperties(h.subarray(12, 17)), this._decoder.markerIsMandatory = !1, this._decoder.unpackSize = ((h[4] | h[5] << 8 | h[6] << 16 | h[7] << 24) >>> 0) - 8;
          } else {
            this._decoder.decodeProperties(h.subarray(0, 5));
            for (var f = 0, p = !1, c = 0;8 > c;c++) {
              var l = h[5 + c];
              255 !== l && (p = !0);
              f |= l << 8 * c;
            }
            this._decoder.markerIsMandatory = !p;
            this._decoder.unpackSize = p ? f : void 0;
          }
          this._decoder.create();
          b = b.subarray(d);
          this._state = 2;
        } else {
          if (2 !== this._state) {
            return;
          }
        }
        try {
          this._inStream.append(b);
          var s = this._decoder.decode(!0);
          this._inStream.compact();
          s !== e && this._checkError(s);
        } catch (u) {
          this._decoder.flushOutput(), this._decoder = null, this._error(u);
        }
      };
      b.prototype.close = function() {
        if (2 === this._state) {
          this._state = 3;
          try {
            var b = this._decoder.decode(!1);
            this._checkError(b);
          } catch (f) {
            this._decoder.flushOutput(), this._error(f);
          }
          this._decoder = null;
        }
      };
      b.prototype._error = function(b) {
        this._state = 4;
        if (this.onError) {
          this.onError(b);
        }
      };
      b.prototype._checkError = function(b) {
        var f;
        b === s ? f = "LZMA decoding error" : b === e ? f = "Decoding is not complete" : b === v ? void 0 !== this._decoder.unpackSize && this._decoder.unpackSize !== this._outStream.processed && (f = "Finished with end marker before than specified size") : f = "Internal LZMA Error";
        f && this._error(f);
      };
      return b;
    }();
    k.LzmaDecoder = b;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    function g(a, d) {
      a !== h(a, 0, d) && throwError("RangeError", Errors.ParamRangeError);
    }
    function c(a) {
      return "string" === typeof a ? a : void 0 == a ? null : a + "";
    }
    var t = k.Debug.notImplemented, n = k.StringUtilities.utf8decode, a = k.StringUtilities.utf8encode, h = k.NumberUtilities.clamp, p = function() {
      return function(a, d, e) {
        this.buffer = a;
        this.length = d;
        this.littleEndian = e;
      };
    }();
    r.PlainObjectDataBuffer = p;
    for (var l = new Uint32Array(33), u = 1, m = 0;32 >= u;u++) {
      l[u] = m = m << 1 | 1;
    }
    var s;
    (function(a) {
      a[a.U8 = 1] = "U8";
      a[a.I32 = 2] = "I32";
      a[a.F32 = 4] = "F32";
    })(s || (s = {}));
    u = function() {
      function m(d) {
        void 0 === d && (d = m.INITIAL_SIZE);
        this._buffer || (this._buffer = new ArrayBuffer(d), this._position = this._length = 0, this._resetViews(), this._littleEndian = m._nativeLittleEndian, this._bitLength = this._bitBuffer = 0);
      }
      m.FromArrayBuffer = function(d, e) {
        void 0 === e && (e = -1);
        var b = Object.create(m.prototype);
        b._buffer = d;
        b._length = -1 === e ? d.byteLength : e;
        b._position = 0;
        b._resetViews();
        b._littleEndian = m._nativeLittleEndian;
        b._bitBuffer = 0;
        b._bitLength = 0;
        return b;
      };
      m.FromPlainObject = function(d) {
        var e = m.FromArrayBuffer(d.buffer, d.length);
        e._littleEndian = d.littleEndian;
        return e;
      };
      m.prototype.toPlainObject = function() {
        return new p(this._buffer, this._length, this._littleEndian);
      };
      m.prototype._resetViews = function() {
        this._u8 = new Uint8Array(this._buffer);
        this._f32 = this._i32 = null;
      };
      m.prototype._requestViews = function(d) {
        0 === (this._buffer.byteLength & 3) && (null === this._i32 && d & 2 && (this._i32 = new Int32Array(this._buffer)), null === this._f32 && d & 4 && (this._f32 = new Float32Array(this._buffer)));
      };
      m.prototype.getBytes = function() {
        return new Uint8Array(this._buffer, 0, this._length);
      };
      m.prototype._ensureCapacity = function(d) {
        var e = this._buffer;
        if (e.byteLength < d) {
          for (var b = Math.max(e.byteLength, 1);b < d;) {
            b *= 2;
          }
          d = m._arrayBufferPool.acquire(b);
          b = this._u8;
          this._buffer = d;
          this._resetViews();
          this._u8.set(b);
          m._arrayBufferPool.release(e);
        }
      };
      m.prototype.clear = function() {
        this._position = this._length = 0;
      };
      m.prototype.readBoolean = function() {
        return 0 !== this.readUnsignedByte();
      };
      m.prototype.readByte = function() {
        return this.readUnsignedByte() << 24 >> 24;
      };
      m.prototype.readUnsignedByte = function() {
        this._position + 1 > this._length && throwError("EOFError", Errors.EOFError);
        return this._u8[this._position++];
      };
      m.prototype.readBytes = function(d, e) {
        var b = 0;
        void 0 === b && (b = 0);
        void 0 === e && (e = 0);
        var f = this._position;
        b || (b = 0);
        e || (e = this._length - f);
        f + e > this._length && throwError("EOFError", Errors.EOFError);
        d.length < b + e && (d._ensureCapacity(b + e), d.length = b + e);
        d._u8.set(new Uint8Array(this._buffer, f, e), b);
        this._position += e;
      };
      m.prototype.readShort = function() {
        return this.readUnsignedShort() << 16 >> 16;
      };
      m.prototype.readUnsignedShort = function() {
        var d = this._u8, e = this._position;
        e + 2 > this._length && throwError("EOFError", Errors.EOFError);
        var b = d[e + 0], d = d[e + 1];
        this._position = e + 2;
        return this._littleEndian ? d << 8 | b : b << 8 | d;
      };
      m.prototype.readInt = function() {
        var d = this._u8, e = this._position;
        e + 4 > this._length && throwError("EOFError", Errors.EOFError);
        var b = d[e + 0], f = d[e + 1], a = d[e + 2], d = d[e + 3];
        this._position = e + 4;
        return this._littleEndian ? d << 24 | a << 16 | f << 8 | b : b << 24 | f << 16 | a << 8 | d;
      };
      m.prototype.readUnsignedInt = function() {
        return this.readInt() >>> 0;
      };
      m.prototype.readFloat = function() {
        var d = this._position;
        d + 4 > this._length && throwError("EOFError", Errors.EOFError);
        this._position = d + 4;
        this._requestViews(4);
        if (this._littleEndian && 0 === (d & 3) && this._f32) {
          return this._f32[d >> 2];
        }
        var e = this._u8, b = k.IntegerUtilities.u8;
        this._littleEndian ? (b[0] = e[d + 0], b[1] = e[d + 1], b[2] = e[d + 2], b[3] = e[d + 3]) : (b[3] = e[d + 0], b[2] = e[d + 1], b[1] = e[d + 2], b[0] = e[d + 3]);
        return k.IntegerUtilities.f32[0];
      };
      m.prototype.readDouble = function() {
        var d = this._u8, e = this._position;
        e + 8 > this._length && throwError("EOFError", Errors.EOFError);
        var b = k.IntegerUtilities.u8;
        this._littleEndian ? (b[0] = d[e + 0], b[1] = d[e + 1], b[2] = d[e + 2], b[3] = d[e + 3], b[4] = d[e + 4], b[5] = d[e + 5], b[6] = d[e + 6], b[7] = d[e + 7]) : (b[0] = d[e + 7], b[1] = d[e + 6], b[2] = d[e + 5], b[3] = d[e + 4], b[4] = d[e + 3], b[5] = d[e + 2], b[6] = d[e + 1], b[7] = d[e + 0]);
        this._position = e + 8;
        return k.IntegerUtilities.f64[0];
      };
      m.prototype.writeBoolean = function(d) {
        this.writeByte(d ? 1 : 0);
      };
      m.prototype.writeByte = function(d) {
        var e = this._position + 1;
        this._ensureCapacity(e);
        this._u8[this._position++] = d;
        e > this._length && (this._length = e);
      };
      m.prototype.writeUnsignedByte = function(d) {
        var e = this._position + 1;
        this._ensureCapacity(e);
        this._u8[this._position++] = d;
        e > this._length && (this._length = e);
      };
      m.prototype.writeRawBytes = function(d) {
        var e = this._position + d.length;
        this._ensureCapacity(e);
        this._u8.set(d, this._position);
        this._position = e;
        e > this._length && (this._length = e);
      };
      m.prototype.writeBytes = function(d, e, b) {
        void 0 === e && (e = 0);
        void 0 === b && (b = 0);
        k.isNullOrUndefined(d) && throwError("TypeError", Errors.NullPointerError, "bytes");
        2 > arguments.length && (e = 0);
        3 > arguments.length && (b = 0);
        g(e, d.length);
        g(e + b, d.length);
        0 === b && (b = d.length - e);
        this.writeRawBytes(new Int8Array(d._buffer, e, b));
      };
      m.prototype.writeShort = function(d) {
        this.writeUnsignedShort(d);
      };
      m.prototype.writeUnsignedShort = function(d) {
        var e = this._position;
        this._ensureCapacity(e + 2);
        var b = this._u8;
        this._littleEndian ? (b[e + 0] = d, b[e + 1] = d >> 8) : (b[e + 0] = d >> 8, b[e + 1] = d);
        this._position = e += 2;
        e > this._length && (this._length = e);
      };
      m.prototype.writeInt = function(d) {
        this.writeUnsignedInt(d);
      };
      m.prototype.write2Ints = function(d, e) {
        this.write2UnsignedInts(d, e);
      };
      m.prototype.write4Ints = function(d, e, b, f) {
        this.write4UnsignedInts(d, e, b, f);
      };
      m.prototype.writeUnsignedInt = function(d) {
        var e = this._position;
        this._ensureCapacity(e + 4);
        this._requestViews(2);
        if (this._littleEndian === m._nativeLittleEndian && 0 === (e & 3) && this._i32) {
          this._i32[e >> 2] = d;
        } else {
          var b = this._u8;
          this._littleEndian ? (b[e + 0] = d, b[e + 1] = d >> 8, b[e + 2] = d >> 16, b[e + 3] = d >> 24) : (b[e + 0] = d >> 24, b[e + 1] = d >> 16, b[e + 2] = d >> 8, b[e + 3] = d);
        }
        this._position = e += 4;
        e > this._length && (this._length = e);
      };
      m.prototype.write2UnsignedInts = function(d, e) {
        var b = this._position;
        this._ensureCapacity(b + 8);
        this._requestViews(2);
        this._littleEndian === m._nativeLittleEndian && 0 === (b & 3) && this._i32 ? (this._i32[(b >> 2) + 0] = d, this._i32[(b >> 2) + 1] = e, this._position = b += 8, b > this._length && (this._length = b)) : (this.writeUnsignedInt(d), this.writeUnsignedInt(e));
      };
      m.prototype.write4UnsignedInts = function(d, e, b, f) {
        var a = this._position;
        this._ensureCapacity(a + 16);
        this._requestViews(2);
        this._littleEndian === m._nativeLittleEndian && 0 === (a & 3) && this._i32 ? (this._i32[(a >> 2) + 0] = d, this._i32[(a >> 2) + 1] = e, this._i32[(a >> 2) + 2] = b, this._i32[(a >> 2) + 3] = f, this._position = a += 16, a > this._length && (this._length = a)) : (this.writeUnsignedInt(d), this.writeUnsignedInt(e), this.writeUnsignedInt(b), this.writeUnsignedInt(f));
      };
      m.prototype.writeFloat = function(d) {
        var e = this._position;
        this._ensureCapacity(e + 4);
        this._requestViews(4);
        if (this._littleEndian === m._nativeLittleEndian && 0 === (e & 3) && this._f32) {
          this._f32[e >> 2] = d;
        } else {
          var b = this._u8;
          k.IntegerUtilities.f32[0] = d;
          d = k.IntegerUtilities.u8;
          this._littleEndian ? (b[e + 0] = d[0], b[e + 1] = d[1], b[e + 2] = d[2], b[e + 3] = d[3]) : (b[e + 0] = d[3], b[e + 1] = d[2], b[e + 2] = d[1], b[e + 3] = d[0]);
        }
        this._position = e += 4;
        e > this._length && (this._length = e);
      };
      m.prototype.write6Floats = function(d, e, b, f, a, h) {
        var p = this._position;
        this._ensureCapacity(p + 24);
        this._requestViews(4);
        this._littleEndian === m._nativeLittleEndian && 0 === (p & 3) && this._f32 ? (this._f32[(p >> 2) + 0] = d, this._f32[(p >> 2) + 1] = e, this._f32[(p >> 2) + 2] = b, this._f32[(p >> 2) + 3] = f, this._f32[(p >> 2) + 4] = a, this._f32[(p >> 2) + 5] = h, this._position = p += 24, p > this._length && (this._length = p)) : (this.writeFloat(d), this.writeFloat(e), this.writeFloat(b), this.writeFloat(f), this.writeFloat(a), this.writeFloat(h));
      };
      m.prototype.writeDouble = function(d) {
        var e = this._position;
        this._ensureCapacity(e + 8);
        var b = this._u8;
        k.IntegerUtilities.f64[0] = d;
        d = k.IntegerUtilities.u8;
        this._littleEndian ? (b[e + 0] = d[0], b[e + 1] = d[1], b[e + 2] = d[2], b[e + 3] = d[3], b[e + 4] = d[4], b[e + 5] = d[5], b[e + 6] = d[6], b[e + 7] = d[7]) : (b[e + 0] = d[7], b[e + 1] = d[6], b[e + 2] = d[5], b[e + 3] = d[4], b[e + 4] = d[3], b[e + 5] = d[2], b[e + 6] = d[1], b[e + 7] = d[0]);
        this._position = e += 8;
        e > this._length && (this._length = e);
      };
      m.prototype.readRawBytes = function() {
        return new Int8Array(this._buffer, 0, this._length);
      };
      m.prototype.writeUTF = function(d) {
        d = c(d);
        d = n(d);
        this.writeShort(d.length);
        this.writeRawBytes(d);
      };
      m.prototype.writeUTFBytes = function(d) {
        d = c(d);
        d = n(d);
        this.writeRawBytes(d);
      };
      m.prototype.readUTF = function() {
        return this.readUTFBytes(this.readShort());
      };
      m.prototype.readUTFBytes = function(d) {
        d >>>= 0;
        var e = this._position;
        e + d > this._length && throwError("EOFError", Errors.EOFError);
        this._position += d;
        return a(new Int8Array(this._buffer, e, d));
      };
      Object.defineProperty(m.prototype, "length", {get:function() {
        return this._length;
      }, set:function(d) {
        d >>>= 0;
        d > this._buffer.byteLength && this._ensureCapacity(d);
        this._length = d;
        this._position = h(this._position, 0, this._length);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "bytesAvailable", {get:function() {
        return this._length - this._position;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "position", {get:function() {
        return this._position;
      }, set:function(d) {
        this._position = d >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "buffer", {get:function() {
        return this._buffer;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "bytes", {get:function() {
        return this._u8;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "ints", {get:function() {
        this._requestViews(2);
        return this._i32;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "objectEncoding", {get:function() {
        return this._objectEncoding;
      }, set:function(d) {
        this._objectEncoding = d >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "endian", {get:function() {
        return this._littleEndian ? "littleEndian" : "bigEndian";
      }, set:function(d) {
        d = c(d);
        this._littleEndian = "auto" === d ? m._nativeLittleEndian : "littleEndian" === d;
      }, enumerable:!0, configurable:!0});
      m.prototype.toString = function() {
        return a(new Int8Array(this._buffer, 0, this._length));
      };
      m.prototype.toBlob = function(d) {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)], {type:d});
      };
      m.prototype.writeMultiByte = function(d, e) {
        t("packageInternal flash.utils.ObjectOutput::writeMultiByte");
      };
      m.prototype.readMultiByte = function(d, e) {
        t("packageInternal flash.utils.ObjectInput::readMultiByte");
      };
      m.prototype.getValue = function(d) {
        d |= 0;
        return d >= this._length ? void 0 : this._u8[d];
      };
      m.prototype.setValue = function(d, e) {
        d |= 0;
        var b = d + 1;
        this._ensureCapacity(b);
        this._u8[d] = e;
        b > this._length && (this._length = b);
      };
      m.prototype.readFixed = function() {
        return this.readInt() / 65536;
      };
      m.prototype.readFixed8 = function() {
        return this.readShort() / 256;
      };
      m.prototype.readFloat16 = function() {
        var d = this.readUnsignedShort(), e = d >> 15 ? -1 : 1, b = (d & 31744) >> 10, d = d & 1023;
        return b ? 31 === b ? d ? NaN : Infinity * e : e * Math.pow(2, b - 15) * (1 + d / 1024) : d / 1024 * Math.pow(2, -14) * e;
      };
      m.prototype.readEncodedU32 = function() {
        var d = this.readUnsignedByte();
        if (!(d & 128)) {
          return d;
        }
        d = d & 127 | this.readUnsignedByte() << 7;
        if (!(d & 16384)) {
          return d;
        }
        d = d & 16383 | this.readUnsignedByte() << 14;
        if (!(d & 2097152)) {
          return d;
        }
        d = d & 2097151 | this.readUnsignedByte() << 21;
        return d & 268435456 ? d & 268435455 | this.readUnsignedByte() << 28 : d;
      };
      m.prototype.readBits = function(d) {
        return this.readUnsignedBits(d) << 32 - d >> 32 - d;
      };
      m.prototype.readUnsignedBits = function(d) {
        for (var e = this._bitBuffer, b = this._bitLength;d > b;) {
          e = e << 8 | this.readUnsignedByte(), b += 8;
        }
        b -= d;
        d = e >>> b & l[d];
        this._bitBuffer = e;
        this._bitLength = b;
        return d;
      };
      m.prototype.readFixedBits = function(d) {
        return this.readBits(d) / 65536;
      };
      m.prototype.readString = function(d) {
        var e = this._position;
        if (d) {
          e + d > this._length && throwError("EOFError", Errors.EOFError), this._position += d;
        } else {
          d = 0;
          for (var b = e;b < this._length && this._u8[b];b++) {
            d++;
          }
          this._position += d + 1;
        }
        return a(new Int8Array(this._buffer, e, d));
      };
      m.prototype.align = function() {
        this._bitLength = this._bitBuffer = 0;
      };
      m.prototype.deflate = function() {
        this.compress("deflate");
      };
      m.prototype.inflate = function() {
        this.uncompress("deflate");
      };
      m.prototype.compress = function(d) {
        d = 0 === arguments.length ? "zlib" : c(d);
        var e;
        switch(d) {
          case "zlib":
            e = new r.Deflate(!0);
            break;
          case "deflate":
            e = new r.Deflate(!1);
            break;
          default:
            return;
        }
        var b = new m;
        e.onData = b.writeRawBytes.bind(b);
        e.push(this._u8.subarray(0, this._length));
        e.close();
        this._ensureCapacity(b._u8.length);
        this._u8.set(b._u8);
        this.length = b.length;
        this._position = 0;
      };
      m.prototype.uncompress = function(d) {
        d = 0 === arguments.length ? "zlib" : c(d);
        var e;
        switch(d) {
          case "zlib":
            e = r.Inflate.create(!0);
            break;
          case "deflate":
            e = r.Inflate.create(!1);
            break;
          case "lzma":
            e = new r.LzmaDecoder(!1);
            break;
          default:
            return;
        }
        var b = new m, f;
        e.onData = b.writeRawBytes.bind(b);
        e.onError = function(b) {
          return f = b;
        };
        e.push(this._u8.subarray(0, this._length));
        f && throwError("IOError", Errors.CompressedDataError);
        e.close();
        this._ensureCapacity(b._u8.length);
        this._u8.set(b._u8);
        this.length = b.length;
        this._position = 0;
      };
      m._nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
      m.INITIAL_SIZE = 128;
      m._arrayBufferPool = new k.ArrayBufferPool;
      return m;
    }();
    r.DataBuffer = u;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var r = k.ArrayUtilities.DataBuffer, g = k.ArrayUtilities.ensureTypedArrayCapacity;
  (function(c) {
    c[c.BeginSolidFill = 1] = "BeginSolidFill";
    c[c.BeginGradientFill = 2] = "BeginGradientFill";
    c[c.BeginBitmapFill = 3] = "BeginBitmapFill";
    c[c.EndFill = 4] = "EndFill";
    c[c.LineStyleSolid = 5] = "LineStyleSolid";
    c[c.LineStyleGradient = 6] = "LineStyleGradient";
    c[c.LineStyleBitmap = 7] = "LineStyleBitmap";
    c[c.LineEnd = 8] = "LineEnd";
    c[c.MoveTo = 9] = "MoveTo";
    c[c.LineTo = 10] = "LineTo";
    c[c.CurveTo = 11] = "CurveTo";
    c[c.CubicCurveTo = 12] = "CubicCurveTo";
  })(k.PathCommand || (k.PathCommand = {}));
  (function(c) {
    c[c.Linear = 16] = "Linear";
    c[c.Radial = 18] = "Radial";
  })(k.GradientType || (k.GradientType = {}));
  (function(c) {
    c[c.Pad = 0] = "Pad";
    c[c.Reflect = 1] = "Reflect";
    c[c.Repeat = 2] = "Repeat";
  })(k.GradientSpreadMethod || (k.GradientSpreadMethod = {}));
  (function(c) {
    c[c.RGB = 0] = "RGB";
    c[c.LinearRGB = 1] = "LinearRGB";
  })(k.GradientInterpolationMethod || (k.GradientInterpolationMethod = {}));
  (function(c) {
    c[c.None = 0] = "None";
    c[c.Normal = 1] = "Normal";
    c[c.Vertical = 2] = "Vertical";
    c[c.Horizontal = 3] = "Horizontal";
  })(k.LineScaleMode || (k.LineScaleMode = {}));
  var c = function() {
    return function(c, a, h, p, l, u, m, s, g, d, e) {
      this.commands = c;
      this.commandsPosition = a;
      this.coordinates = h;
      this.morphCoordinates = p;
      this.coordinatesPosition = l;
      this.styles = u;
      this.stylesLength = m;
      this.morphStyles = s;
      this.morphStylesLength = g;
      this.hasFills = d;
      this.hasLines = e;
    };
  }();
  k.PlainObjectShapeData = c;
  var t;
  (function(c) {
    c[c.Commands = 32] = "Commands";
    c[c.Coordinates = 128] = "Coordinates";
    c[c.Styles = 16] = "Styles";
  })(t || (t = {}));
  t = function() {
    function n(a) {
      void 0 === a && (a = !0);
      a && this.clear();
    }
    n.FromPlainObject = function(a) {
      var h = new n(!1);
      h.commands = a.commands;
      h.coordinates = a.coordinates;
      h.morphCoordinates = a.morphCoordinates;
      h.commandsPosition = a.commandsPosition;
      h.coordinatesPosition = a.coordinatesPosition;
      h.styles = r.FromArrayBuffer(a.styles, a.stylesLength);
      h.styles.endian = "auto";
      a.morphStyles && (h.morphStyles = r.FromArrayBuffer(a.morphStyles, a.morphStylesLength), h.morphStyles.endian = "auto");
      h.hasFills = a.hasFills;
      h.hasLines = a.hasLines;
      return h;
    };
    n.prototype.moveTo = function(a, h) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 9;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
    };
    n.prototype.lineTo = function(a, h) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 10;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
    };
    n.prototype.curveTo = function(a, h, p, c) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = p;
      this.coordinates[this.coordinatesPosition++] = c;
    };
    n.prototype.cubicCurveTo = function(a, h, p, c, n, m) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = p;
      this.coordinates[this.coordinatesPosition++] = c;
      this.coordinates[this.coordinatesPosition++] = n;
      this.coordinates[this.coordinatesPosition++] = m;
    };
    n.prototype.beginFill = function(a) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 1;
      this.styles.writeUnsignedInt(a);
      this.hasFills = !0;
    };
    n.prototype.writeMorphFill = function(a) {
      this.morphStyles.writeUnsignedInt(a);
    };
    n.prototype.endFill = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 4;
    };
    n.prototype.endLine = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 8;
    };
    n.prototype.lineStyle = function(a, h, p, c, n, m, s) {
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5;
      this.coordinates[this.coordinatesPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(h);
      a.writeBoolean(p);
      a.writeUnsignedByte(c);
      a.writeUnsignedByte(n);
      a.writeUnsignedByte(m);
      a.writeUnsignedByte(s);
      this.hasLines = !0;
    };
    n.prototype.writeMorphLineStyle = function(a, h) {
      this.morphCoordinates[this.coordinatesPosition - 1] = a;
      this.morphStyles.writeUnsignedInt(h);
    };
    n.prototype.beginBitmap = function(a, h, p, c, n) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(h);
      this._writeStyleMatrix(p, !1);
      a.writeBoolean(c);
      a.writeBoolean(n);
      this.hasFills = !0;
    };
    n.prototype.writeMorphBitmap = function(a) {
      this._writeStyleMatrix(a, !0);
    };
    n.prototype.beginGradient = function(a, h, p, c, n, m, s, g) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedByte(c);
      a.writeShort(g);
      this._writeStyleMatrix(n, !1);
      c = h.length;
      a.writeByte(c);
      for (n = 0;n < c;n++) {
        a.writeUnsignedByte(p[n]), a.writeUnsignedInt(h[n]);
      }
      a.writeUnsignedByte(m);
      a.writeUnsignedByte(s);
      this.hasFills = !0;
    };
    n.prototype.writeMorphGradient = function(a, h, p) {
      this._writeStyleMatrix(p, !0);
      p = this.morphStyles;
      for (var c = 0;c < a.length;c++) {
        p.writeUnsignedByte(h[c]), p.writeUnsignedInt(a[c]);
      }
    };
    n.prototype.writeCommandAndCoordinates = function(a, h, c) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = c;
    };
    n.prototype.writeCoordinates = function(a, h) {
      this.ensurePathCapacities(0, 2);
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
    };
    n.prototype.writeMorphCoordinates = function(a, h) {
      this.morphCoordinates = g(this.morphCoordinates, this.coordinatesPosition);
      this.morphCoordinates[this.coordinatesPosition - 2] = a;
      this.morphCoordinates[this.coordinatesPosition - 1] = h;
    };
    n.prototype.clear = function() {
      this.commandsPosition = this.coordinatesPosition = 0;
      this.commands = new Uint8Array(32);
      this.coordinates = new Int32Array(128);
      this.styles = new r(16);
      this.styles.endian = "auto";
      this.hasFills = this.hasLines = !1;
    };
    n.prototype.isEmpty = function() {
      return 0 === this.commandsPosition;
    };
    n.prototype.clone = function() {
      var a = new n(!1);
      a.commands = new Uint8Array(this.commands);
      a.commandsPosition = this.commandsPosition;
      a.coordinates = new Int32Array(this.coordinates);
      a.coordinatesPosition = this.coordinatesPosition;
      a.styles = new r(this.styles.length);
      a.styles.writeRawBytes(this.styles.bytes);
      this.morphStyles && (a.morphStyles = new r(this.morphStyles.length), a.morphStyles.writeRawBytes(this.morphStyles.bytes));
      a.hasFills = this.hasFills;
      a.hasLines = this.hasLines;
      return a;
    };
    n.prototype.toPlainObject = function() {
      return new c(this.commands, this.commandsPosition, this.coordinates, this.morphCoordinates, this.coordinatesPosition, this.styles.buffer, this.styles.length, this.morphStyles && this.morphStyles.buffer, this.morphStyles ? this.morphStyles.length : 0, this.hasFills, this.hasLines);
    };
    Object.defineProperty(n.prototype, "buffers", {get:function() {
      var a = [this.commands.buffer, this.coordinates.buffer, this.styles.buffer];
      this.morphCoordinates && a.push(this.morphCoordinates.buffer);
      this.morphStyles && a.push(this.morphStyles.buffer);
      return a;
    }, enumerable:!0, configurable:!0});
    n.prototype._writeStyleMatrix = function(a, h) {
      (h ? this.morphStyles : this.styles).write6Floats(a.a, a.b, a.c, a.d, a.tx, a.ty);
    };
    n.prototype.ensurePathCapacities = function(a, h) {
      this.commands = g(this.commands, this.commandsPosition + a);
      this.coordinates = g(this.coordinates, this.coordinatesPosition + h);
    };
    return n;
  }();
  k.ShapeData = t;
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      (function(c) {
        c[c.CODE_END = 0] = "CODE_END";
        c[c.CODE_SHOW_FRAME = 1] = "CODE_SHOW_FRAME";
        c[c.CODE_DEFINE_SHAPE = 2] = "CODE_DEFINE_SHAPE";
        c[c.CODE_FREE_CHARACTER = 3] = "CODE_FREE_CHARACTER";
        c[c.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        c[c.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        c[c.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        c[c.CODE_DEFINE_BUTTON = 7] = "CODE_DEFINE_BUTTON";
        c[c.CODE_JPEG_TABLES = 8] = "CODE_JPEG_TABLES";
        c[c.CODE_SET_BACKGROUND_COLOR = 9] = "CODE_SET_BACKGROUND_COLOR";
        c[c.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        c[c.CODE_DEFINE_TEXT = 11] = "CODE_DEFINE_TEXT";
        c[c.CODE_DO_ACTION = 12] = "CODE_DO_ACTION";
        c[c.CODE_DEFINE_FONT_INFO = 13] = "CODE_DEFINE_FONT_INFO";
        c[c.CODE_DEFINE_SOUND = 14] = "CODE_DEFINE_SOUND";
        c[c.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        c[c.CODE_STOP_SOUND = 16] = "CODE_STOP_SOUND";
        c[c.CODE_DEFINE_BUTTON_SOUND = 17] = "CODE_DEFINE_BUTTON_SOUND";
        c[c.CODE_SOUND_STREAM_HEAD = 18] = "CODE_SOUND_STREAM_HEAD";
        c[c.CODE_SOUND_STREAM_BLOCK = 19] = "CODE_SOUND_STREAM_BLOCK";
        c[c.CODE_DEFINE_BITS_LOSSLESS = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        c[c.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        c[c.CODE_DEFINE_SHAPE2 = 22] = "CODE_DEFINE_SHAPE2";
        c[c.CODE_DEFINE_BUTTON_CXFORM = 23] = "CODE_DEFINE_BUTTON_CXFORM";
        c[c.CODE_PROTECT = 24] = "CODE_PROTECT";
        c[c.CODE_PATHS_ARE_POSTSCRIPT = 25] = "CODE_PATHS_ARE_POSTSCRIPT";
        c[c.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        c[c.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        c[c.CODE_SYNC_FRAME = 29] = "CODE_SYNC_FRAME";
        c[c.CODE_FREE_ALL = 31] = "CODE_FREE_ALL";
        c[c.CODE_DEFINE_SHAPE3 = 32] = "CODE_DEFINE_SHAPE3";
        c[c.CODE_DEFINE_TEXT2 = 33] = "CODE_DEFINE_TEXT2";
        c[c.CODE_DEFINE_BUTTON2 = 34] = "CODE_DEFINE_BUTTON2";
        c[c.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        c[c.CODE_DEFINE_BITS_LOSSLESS2 = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        c[c.CODE_DEFINE_EDIT_TEXT = 37] = "CODE_DEFINE_EDIT_TEXT";
        c[c.CODE_DEFINE_VIDEO = 38] = "CODE_DEFINE_VIDEO";
        c[c.CODE_DEFINE_SPRITE = 39] = "CODE_DEFINE_SPRITE";
        c[c.CODE_NAME_CHARACTER = 40] = "CODE_NAME_CHARACTER";
        c[c.CODE_PRODUCT_INFO = 41] = "CODE_PRODUCT_INFO";
        c[c.CODE_DEFINE_TEXT_FORMAT = 42] = "CODE_DEFINE_TEXT_FORMAT";
        c[c.CODE_FRAME_LABEL = 43] = "CODE_FRAME_LABEL";
        c[c.CODE_DEFINE_BEHAVIOUR = 44] = "CODE_DEFINE_BEHAVIOUR";
        c[c.CODE_SOUND_STREAM_HEAD2 = 45] = "CODE_SOUND_STREAM_HEAD2";
        c[c.CODE_DEFINE_MORPH_SHAPE = 46] = "CODE_DEFINE_MORPH_SHAPE";
        c[c.CODE_GENERATE_FRAME = 47] = "CODE_GENERATE_FRAME";
        c[c.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        c[c.CODE_GEN_COMMAND = 49] = "CODE_GEN_COMMAND";
        c[c.CODE_DEFINE_COMMAND_OBJECT = 50] = "CODE_DEFINE_COMMAND_OBJECT";
        c[c.CODE_CHARACTER_SET = 51] = "CODE_CHARACTER_SET";
        c[c.CODE_EXTERNAL_FONT = 52] = "CODE_EXTERNAL_FONT";
        c[c.CODE_DEFINE_FUNCTION = 53] = "CODE_DEFINE_FUNCTION";
        c[c.CODE_PLACE_FUNCTION = 54] = "CODE_PLACE_FUNCTION";
        c[c.CODE_GEN_TAG_OBJECTS = 55] = "CODE_GEN_TAG_OBJECTS";
        c[c.CODE_EXPORT_ASSETS = 56] = "CODE_EXPORT_ASSETS";
        c[c.CODE_IMPORT_ASSETS = 57] = "CODE_IMPORT_ASSETS";
        c[c.CODE_ENABLE_DEBUGGER = 58] = "CODE_ENABLE_DEBUGGER";
        c[c.CODE_DO_INIT_ACTION = 59] = "CODE_DO_INIT_ACTION";
        c[c.CODE_DEFINE_VIDEO_STREAM = 60] = "CODE_DEFINE_VIDEO_STREAM";
        c[c.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
        c[c.CODE_DEFINE_FONT_INFO2 = 62] = "CODE_DEFINE_FONT_INFO2";
        c[c.CODE_DEBUG_ID = 63] = "CODE_DEBUG_ID";
        c[c.CODE_ENABLE_DEBUGGER2 = 64] = "CODE_ENABLE_DEBUGGER2";
        c[c.CODE_SCRIPT_LIMITS = 65] = "CODE_SCRIPT_LIMITS";
        c[c.CODE_SET_TAB_INDEX = 66] = "CODE_SET_TAB_INDEX";
        c[c.CODE_FILE_ATTRIBUTES = 69] = "CODE_FILE_ATTRIBUTES";
        c[c.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        c[c.CODE_IMPORT_ASSETS2 = 71] = "CODE_IMPORT_ASSETS2";
        c[c.CODE_DO_ABC_DEFINE = 72] = "CODE_DO_ABC_DEFINE";
        c[c.CODE_DEFINE_FONT_ALIGN_ZONES = 73] = "CODE_DEFINE_FONT_ALIGN_ZONES";
        c[c.CODE_CSM_TEXT_SETTINGS = 74] = "CODE_CSM_TEXT_SETTINGS";
        c[c.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        c[c.CODE_SYMBOL_CLASS = 76] = "CODE_SYMBOL_CLASS";
        c[c.CODE_METADATA = 77] = "CODE_METADATA";
        c[c.CODE_DEFINE_SCALING_GRID = 78] = "CODE_DEFINE_SCALING_GRID";
        c[c.CODE_DO_ABC = 82] = "CODE_DO_ABC";
        c[c.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        c[c.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        c[c.CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA = 86] = "CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA";
        c[c.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        c[c.CODE_DEFINE_FONT_NAME = 88] = "CODE_DEFINE_FONT_NAME";
        c[c.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
        c[c.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        c[c.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(g.SwfTag || (g.SwfTag = {}));
      (function(c) {
        c[c.CODE_DEFINE_SHAPE = 2] = "CODE_DEFINE_SHAPE";
        c[c.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        c[c.CODE_DEFINE_BUTTON = 7] = "CODE_DEFINE_BUTTON";
        c[c.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        c[c.CODE_DEFINE_TEXT = 11] = "CODE_DEFINE_TEXT";
        c[c.CODE_DEFINE_SOUND = 14] = "CODE_DEFINE_SOUND";
        c[c.CODE_DEFINE_BITS_LOSSLESS = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        c[c.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        c[c.CODE_DEFINE_SHAPE2 = 22] = "CODE_DEFINE_SHAPE2";
        c[c.CODE_DEFINE_SHAPE3 = 32] = "CODE_DEFINE_SHAPE3";
        c[c.CODE_DEFINE_TEXT2 = 33] = "CODE_DEFINE_TEXT2";
        c[c.CODE_DEFINE_BUTTON2 = 34] = "CODE_DEFINE_BUTTON2";
        c[c.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        c[c.CODE_DEFINE_BITS_LOSSLESS2 = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        c[c.CODE_DEFINE_EDIT_TEXT = 37] = "CODE_DEFINE_EDIT_TEXT";
        c[c.CODE_DEFINE_SPRITE = 39] = "CODE_DEFINE_SPRITE";
        c[c.CODE_DEFINE_MORPH_SHAPE = 46] = "CODE_DEFINE_MORPH_SHAPE";
        c[c.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        c[c.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        c[c.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        c[c.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        c[c.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        c[c.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        c[c.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(g.DefinitionTags || (g.DefinitionTags = {}));
      (function(c) {
        c[c.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        c[c.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        c[c.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        c[c.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
      })(g.ImageDefinitionTags || (g.ImageDefinitionTags = {}));
      (function(c) {
        c[c.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        c[c.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        c[c.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        c[c.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(g.FontDefinitionTags || (g.FontDefinitionTags = {}));
      (function(c) {
        c[c.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        c[c.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        c[c.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        c[c.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        c[c.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        c[c.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        c[c.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
        c[c.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
      })(g.ControlTags || (g.ControlTags = {}));
      (function(c) {
        c[c.Move = 1] = "Move";
        c[c.HasCharacter = 2] = "HasCharacter";
        c[c.HasMatrix = 4] = "HasMatrix";
        c[c.HasColorTransform = 8] = "HasColorTransform";
        c[c.HasRatio = 16] = "HasRatio";
        c[c.HasName = 32] = "HasName";
        c[c.HasClipDepth = 64] = "HasClipDepth";
        c[c.HasClipActions = 128] = "HasClipActions";
        c[c.HasFilterList = 256] = "HasFilterList";
        c[c.HasBlendMode = 512] = "HasBlendMode";
        c[c.HasCacheAsBitmap = 1024] = "HasCacheAsBitmap";
        c[c.HasClassName = 2048] = "HasClassName";
        c[c.HasImage = 4096] = "HasImage";
        c[c.HasVisible = 8192] = "HasVisible";
        c[c.OpaqueBackground = 16384] = "OpaqueBackground";
        c[c.Reserved = 32768] = "Reserved";
      })(g.PlaceObjectFlags || (g.PlaceObjectFlags = {}));
      (function(c) {
        c[c.Load = 1] = "Load";
        c[c.EnterFrame = 2] = "EnterFrame";
        c[c.Unload = 4] = "Unload";
        c[c.MouseMove = 8] = "MouseMove";
        c[c.MouseDown = 16] = "MouseDown";
        c[c.MouseUp = 32] = "MouseUp";
        c[c.KeyDown = 64] = "KeyDown";
        c[c.KeyUp = 128] = "KeyUp";
        c[c.Data = 256] = "Data";
        c[c.Initialize = 512] = "Initialize";
        c[c.Press = 1024] = "Press";
        c[c.Release = 2048] = "Release";
        c[c.ReleaseOutside = 4096] = "ReleaseOutside";
        c[c.RollOver = 8192] = "RollOver";
        c[c.RollOut = 16384] = "RollOut";
        c[c.DragOver = 32768] = "DragOver";
        c[c.DragOut = 65536] = "DragOut";
        c[c.KeyPress = 131072] = "KeyPress";
        c[c.Construct = 262144] = "Construct";
      })(g.AVM1ClipEvents || (g.AVM1ClipEvents = {}));
    })(k.Parser || (k.Parser = {}));
  })(k.SWF || (k.SWF = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var r = k.Debug.unexpected, g = function() {
    function c(c, n, a, h) {
      this.url = c;
      this.method = n;
      this.mimeType = a;
      this.data = h;
    }
    c.prototype.readAll = function(c, n) {
      var a = this.url, h = new XMLHttpRequest({mozSystem:!0});
      h.open(this.method || "GET", this.url, !0);
      h.responseType = "arraybuffer";
      c && (h.onprogress = function(a) {
        c(h.response, a.loaded, a.total);
      });
      h.onreadystatechange = function(c) {
        4 === h.readyState && (200 !== h.status && 0 !== h.status || null === h.response ? (r("Path: " + a + " not found."), n(null, h.statusText)) : n(h.response));
      };
      this.mimeType && h.setRequestHeader("Content-Type", this.mimeType);
      h.send(this.data || null);
    };
    c.prototype.readChunked = function(c, n, a, h, p, l) {
      if (0 >= c) {
        this.readAsync(n, a, h, p, l);
      } else {
        var g = 0, m = new Uint8Array(c), s = 0, k;
        this.readAsync(function(d, e) {
          k = e.total;
          for (var b = d.length, f = 0;g + b >= c;) {
            var a = c - g;
            m.set(d.subarray(f, f + a), g);
            f += a;
            b -= a;
            s += c;
            n(m, {loaded:s, total:k});
            g = 0;
          }
          m.set(d.subarray(f), g);
          g += b;
        }, a, h, function() {
          0 < g && (s += g, n(m.subarray(0, g), {loaded:s, total:k}), g = 0);
          p && p();
        }, l);
      }
    };
    c.prototype.readAsync = function(c, n, a, h, p) {
      var l = new XMLHttpRequest({mozSystem:!0}), g = this.url, m = 0, s = 0;
      l.open(this.method || "GET", g, !0);
      l.responseType = "moz-chunked-arraybuffer";
      var k = "moz-chunked-arraybuffer" !== l.responseType;
      k && (l.responseType = "arraybuffer");
      l.onprogress = function(d) {
        k || (m = d.loaded, s = d.total, c(new Uint8Array(l.response), {loaded:m, total:s}));
      };
      l.onreadystatechange = function(d) {
        2 === l.readyState && p && p(g, l.status, l.getAllResponseHeaders());
        4 === l.readyState && (200 !== l.status && 0 !== l.status || null === l.response && (0 === s || m !== s) ? n(l.statusText) : (k && (d = l.response, c(new Uint8Array(d), {loaded:0, total:d.byteLength})), h && h()));
      };
      this.mimeType && l.setRequestHeader("Content-Type", this.mimeType);
      l.send(this.data || null);
      a && a();
    };
    return c;
  }();
  k.BinaryFileReader = g;
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      g[g.Objects = 0] = "Objects";
      g[g.References = 1] = "References";
    })(k.RemotingPhase || (k.RemotingPhase = {}));
    (function(g) {
      g[g.HasMatrix = 1] = "HasMatrix";
      g[g.HasBounds = 2] = "HasBounds";
      g[g.HasChildren = 4] = "HasChildren";
      g[g.HasColorTransform = 8] = "HasColorTransform";
      g[g.HasClipRect = 16] = "HasClipRect";
      g[g.HasMiscellaneousProperties = 32] = "HasMiscellaneousProperties";
      g[g.HasMask = 64] = "HasMask";
      g[g.HasClip = 128] = "HasClip";
    })(k.MessageBits || (k.MessageBits = {}));
    (function(g) {
      g[g.None = 0] = "None";
      g[g.Asset = 134217728] = "Asset";
    })(k.IDMask || (k.IDMask = {}));
    (function(g) {
      g[g.EOF = 0] = "EOF";
      g[g.UpdateFrame = 100] = "UpdateFrame";
      g[g.UpdateGraphics = 101] = "UpdateGraphics";
      g[g.UpdateBitmapData = 102] = "UpdateBitmapData";
      g[g.UpdateTextContent = 103] = "UpdateTextContent";
      g[g.UpdateStage = 104] = "UpdateStage";
      g[g.UpdateNetStream = 105] = "UpdateNetStream";
      g[g.RequestBitmapData = 106] = "RequestBitmapData";
      g[g.DrawToBitmap = 200] = "DrawToBitmap";
      g[g.MouseEvent = 300] = "MouseEvent";
      g[g.KeyboardEvent = 301] = "KeyboardEvent";
      g[g.FocusEvent = 302] = "FocusEvent";
    })(k.MessageTag || (k.MessageTag = {}));
    (function(g) {
      g[g.Blur = 0] = "Blur";
      g[g.DropShadow = 1] = "DropShadow";
    })(k.FilterType || (k.FilterType = {}));
    (function(g) {
      g[g.Identity = 0] = "Identity";
      g[g.AlphaMultiplierOnly = 1] = "AlphaMultiplierOnly";
      g[g.All = 2] = "All";
    })(k.ColorTransformEncoding || (k.ColorTransformEncoding = {}));
    (function(g) {
      g[g.Initialized = 0] = "Initialized";
      g[g.Metadata = 1] = "Metadata";
      g[g.PlayStart = 2] = "PlayStart";
      g[g.PlayStop = 3] = "PlayStop";
      g[g.BufferEmpty = 4] = "BufferEmpty";
      g[g.BufferFull = 5] = "BufferFull";
      g[g.Pause = 6] = "Pause";
      g[g.Unpause = 7] = "Unpause";
      g[g.Seeking = 8] = "Seeking";
      g[g.Seeked = 9] = "Seeked";
      g[g.Progress = 10] = "Progress";
      g[g.Error = 11] = "Error";
    })(k.VideoPlaybackEvent || (k.VideoPlaybackEvent = {}));
    (function(g) {
      g[g.Init = 1] = "Init";
      g[g.Pause = 2] = "Pause";
      g[g.Seek = 3] = "Seek";
      g[g.GetTime = 4] = "GetTime";
      g[g.GetBufferLength = 5] = "GetBufferLength";
      g[g.SetSoundLevels = 6] = "SetSoundLevels";
      g[g.GetBytesLoaded = 7] = "GetBytesLoaded";
      g[g.GetBytesTotal = 8] = "GetBytesTotal";
      g[g.EnsurePlaying = 9] = "EnsurePlaying";
    })(k.VideoControlEvent || (k.VideoControlEvent = {}));
    (function(g) {
      g[g.ShowAll = 0] = "ShowAll";
      g[g.ExactFit = 1] = "ExactFit";
      g[g.NoBorder = 2] = "NoBorder";
      g[g.NoScale = 4] = "NoScale";
    })(k.StageScaleMode || (k.StageScaleMode = {}));
    (function(g) {
      g[g.None = 0] = "None";
      g[g.Top = 1] = "Top";
      g[g.Bottom = 2] = "Bottom";
      g[g.Left = 4] = "Left";
      g[g.Right = 8] = "Right";
      g[g.TopLeft = g.Top | g.Left] = "TopLeft";
      g[g.BottomLeft = g.Bottom | g.Left] = "BottomLeft";
      g[g.BottomRight = g.Bottom | g.Right] = "BottomRight";
      g[g.TopRight = g.Top | g.Right] = "TopRight";
    })(k.StageAlignFlags || (k.StageAlignFlags = {}));
    k.MouseEventNames = "click dblclick mousedown mousemove mouseup mouseover mouseout".split(" ");
    k.KeyboardEventNames = ["keydown", "keypress", "keyup"];
    (function(g) {
      g[g.CtrlKey = 1] = "CtrlKey";
      g[g.AltKey = 2] = "AltKey";
      g[g.ShiftKey = 4] = "ShiftKey";
    })(k.KeyboardEventFlags || (k.KeyboardEventFlags = {}));
    (function(g) {
      g[g.DocumentHidden = 0] = "DocumentHidden";
      g[g.DocumentVisible = 1] = "DocumentVisible";
      g[g.WindowBlur = 2] = "WindowBlur";
      g[g.WindowFocus = 3] = "WindowFocus";
    })(k.FocusEventType || (k.FocusEventType = {}));
  })(k.Remoting || (k.Remoting = {}));
})(Shumway || (Shumway = {}));
var throwError, Errors;
(function(k) {
  (function(k) {
    (function(g) {
      var c = function() {
        function c() {
        }
        c.toRGBA = function(a, h, c, l) {
          void 0 === l && (l = 1);
          return "rgba(" + a + "," + h + "," + c + "," + l + ")";
        };
        return c;
      }();
      g.UI = c;
      var k = function() {
        function n() {
        }
        n.prototype.tabToolbar = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(37, 44, 51, a);
        };
        n.prototype.toolbars = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(52, 60, 69, a);
        };
        n.prototype.selectionBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(29, 79, 115, a);
        };
        n.prototype.selectionText = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(245, 247, 250, a);
        };
        n.prototype.splitters = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(0, 0, 0, a);
        };
        n.prototype.bodyBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(17, 19, 21, a);
        };
        n.prototype.sidebarBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(24, 29, 32, a);
        };
        n.prototype.attentionBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(161, 134, 80, a);
        };
        n.prototype.bodyText = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(143, 161, 178, a);
        };
        n.prototype.foregroundTextGrey = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(182, 186, 191, a);
        };
        n.prototype.contentTextHighContrast = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(169, 186, 203, a);
        };
        n.prototype.contentTextGrey = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(143, 161, 178, a);
        };
        n.prototype.contentTextDarkGrey = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(95, 115, 135, a);
        };
        n.prototype.blueHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(70, 175, 227, a);
        };
        n.prototype.purpleHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(107, 122, 187, a);
        };
        n.prototype.pinkHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(223, 128, 255, a);
        };
        n.prototype.redHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(235, 83, 104, a);
        };
        n.prototype.orangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(217, 102, 41, a);
        };
        n.prototype.lightOrangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(217, 155, 40, a);
        };
        n.prototype.greenHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(112, 191, 83, a);
        };
        n.prototype.blueGreyHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(94, 136, 176, a);
        };
        return n;
      }();
      g.UIThemeDark = k;
      k = function() {
        function n() {
        }
        n.prototype.tabToolbar = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(235, 236, 237, a);
        };
        n.prototype.toolbars = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(240, 241, 242, a);
        };
        n.prototype.selectionBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(76, 158, 217, a);
        };
        n.prototype.selectionText = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(245, 247, 250, a);
        };
        n.prototype.splitters = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(170, 170, 170, a);
        };
        n.prototype.bodyBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(252, 252, 252, a);
        };
        n.prototype.sidebarBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(247, 247, 247, a);
        };
        n.prototype.attentionBackground = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(161, 134, 80, a);
        };
        n.prototype.bodyText = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(24, 25, 26, a);
        };
        n.prototype.foregroundTextGrey = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(88, 89, 89, a);
        };
        n.prototype.contentTextHighContrast = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(41, 46, 51, a);
        };
        n.prototype.contentTextGrey = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(143, 161, 178, a);
        };
        n.prototype.contentTextDarkGrey = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(102, 115, 128, a);
        };
        n.prototype.blueHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(0, 136, 204, a);
        };
        n.prototype.purpleHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(91, 95, 255, a);
        };
        n.prototype.pinkHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(184, 46, 229, a);
        };
        n.prototype.redHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(237, 38, 85, a);
        };
        n.prototype.orangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(241, 60, 0, a);
        };
        n.prototype.lightOrangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(217, 126, 0, a);
        };
        n.prototype.greenHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(44, 187, 15, a);
        };
        n.prototype.blueGreyHighlight = function(a) {
          void 0 === a && (a = 1);
          return c.toRGBA(95, 136, 176, a);
        };
        return n;
      }();
      g.UIThemeLight = k;
    })(k.Theme || (k.Theme = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var c = function() {
        function c(n, a) {
          this._buffers = n || [];
          this._snapshots = [];
          this._windowStart = this._startTime = a;
          this._maxDepth = 0;
        }
        c.prototype.addBuffer = function(c) {
          this._buffers.push(c);
        };
        c.prototype.getSnapshotAt = function(c) {
          return this._snapshots[c];
        };
        Object.defineProperty(c.prototype, "hasSnapshots", {get:function() {
          return 0 < this.snapshotCount;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "snapshotCount", {get:function() {
          return this._snapshots.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "startTime", {get:function() {
          return this._startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "endTime", {get:function() {
          return this._endTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "windowStart", {get:function() {
          return this._windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "windowEnd", {get:function() {
          return this._windowEnd;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "windowLength", {get:function() {
          return this.windowEnd - this.windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "maxDepth", {get:function() {
          return this._maxDepth;
        }, enumerable:!0, configurable:!0});
        c.prototype.forEachSnapshot = function(c) {
          for (var a = 0, h = this.snapshotCount;a < h;a++) {
            c(this._snapshots[a], a);
          }
        };
        c.prototype.createSnapshots = function() {
          var c = Number.MIN_VALUE, a = 0;
          for (this._snapshots = [];0 < this._buffers.length;) {
            var h = this._buffers.shift().createSnapshot();
            h && (c < h.endTime && (c = h.endTime), a < h.maxDepth && (a = h.maxDepth), this._snapshots.push(h));
          }
          this._windowEnd = this._endTime = c;
          this._maxDepth = a;
        };
        c.prototype.setWindow = function(c, a) {
          if (c > a) {
            var h = c;
            c = a;
            a = h;
          }
          h = Math.min(a - c, this.totalTime);
          c < this._startTime ? (c = this._startTime, a = this._startTime + h) : a > this._endTime && (c = this._endTime - h, a = this._endTime);
          this._windowStart = c;
          this._windowEnd = a;
        };
        c.prototype.moveWindowTo = function(c) {
          this.setWindow(c - this.windowLength / 2, c + this.windowLength / 2);
        };
        return c;
      }();
      g.Profile = c;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, r) {
  function g() {
    this.constructor = k;
  }
  for (var c in r) {
    r.hasOwnProperty(c) && (k[c] = r[c]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(k) {
    (function(g) {
      var c = function() {
        return function(c) {
          this.kind = c;
          this.totalTime = this.selfTime = this.count = 0;
        };
      }();
      g.TimelineFrameStatistics = c;
      var k = function() {
        function n(a, h, c, l, n, m) {
          this.parent = a;
          this.kind = h;
          this.startData = c;
          this.endData = l;
          this.startTime = n;
          this.endTime = m;
          this.maxDepth = 0;
        }
        Object.defineProperty(n.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(n.prototype, "selfTime", {get:function() {
          var a = this.totalTime;
          if (this.children) {
            for (var h = 0, c = this.children.length;h < c;h++) {
              var l = this.children[h], a = a - (l.endTime - l.startTime)
            }
          }
          return a;
        }, enumerable:!0, configurable:!0});
        n.prototype.getChildIndex = function(a) {
          for (var h = this.children, c = 0;c < h.length;c++) {
            if (h[c].endTime > a) {
              return c;
            }
          }
          return 0;
        };
        n.prototype.getChildRange = function(a, h) {
          if (this.children && a <= this.endTime && h >= this.startTime && h >= a) {
            var c = this._getNearestChild(a), l = this._getNearestChildReverse(h);
            if (c <= l) {
              return a = this.children[c].startTime, h = this.children[l].endTime, {startIndex:c, endIndex:l, startTime:a, endTime:h, totalTime:h - a};
            }
          }
          return null;
        };
        n.prototype._getNearestChild = function(a) {
          var h = this.children;
          if (h && h.length) {
            if (a <= h[0].endTime) {
              return 0;
            }
            for (var c, l = 0, n = h.length - 1;n > l;) {
              c = (l + n) / 2 | 0;
              var m = h[c];
              if (a >= m.startTime && a <= m.endTime) {
                return c;
              }
              a > m.endTime ? l = c + 1 : n = c;
            }
            return Math.ceil((l + n) / 2);
          }
          return 0;
        };
        n.prototype._getNearestChildReverse = function(a) {
          var h = this.children;
          if (h && h.length) {
            var c = h.length - 1;
            if (a >= h[c].startTime) {
              return c;
            }
            for (var l, n = 0;c > n;) {
              l = Math.ceil((n + c) / 2);
              var m = h[l];
              if (a >= m.startTime && a <= m.endTime) {
                return l;
              }
              a > m.endTime ? n = l : c = l - 1;
            }
            return(n + c) / 2 | 0;
          }
          return 0;
        };
        n.prototype.query = function(a) {
          if (a < this.startTime || a > this.endTime) {
            return null;
          }
          var h = this.children;
          if (h && 0 < h.length) {
            for (var c, l = 0, n = h.length - 1;n > l;) {
              var m = (l + n) / 2 | 0;
              c = h[m];
              if (a >= c.startTime && a <= c.endTime) {
                return c.query(a);
              }
              a > c.endTime ? l = m + 1 : n = m;
            }
            c = h[n];
            if (a >= c.startTime && a <= c.endTime) {
              return c.query(a);
            }
          }
          return this;
        };
        n.prototype.queryNext = function(a) {
          for (var h = this;a > h.endTime;) {
            if (h.parent) {
              h = h.parent;
            } else {
              break;
            }
          }
          return h.query(a);
        };
        n.prototype.getDepth = function() {
          for (var a = 0, h = this;h;) {
            a++, h = h.parent;
          }
          return a;
        };
        n.prototype.calculateStatistics = function() {
          function a(p) {
            if (p.kind) {
              var l = h[p.kind.id] || (h[p.kind.id] = new c(p.kind));
              l.count++;
              l.selfTime += p.selfTime;
              l.totalTime += p.totalTime;
            }
            p.children && p.children.forEach(a);
          }
          var h = this.statistics = [];
          a(this);
        };
        n.prototype.trace = function(a) {
          var h = (this.kind ? this.kind.name + ": " : "Profile: ") + (this.endTime - this.startTime).toFixed(2);
          if (this.children && this.children.length) {
            a.enter(h);
            for (h = 0;h < this.children.length;h++) {
              this.children[h].trace(a);
            }
            a.outdent();
          } else {
            a.writeLn(h);
          }
        };
        return n;
      }();
      g.TimelineFrame = k;
      k = function(c) {
        function a(a) {
          c.call(this, null, null, null, null, NaN, NaN);
          this.name = a;
        }
        __extends(a, c);
        return a;
      }(k);
      g.TimelineBufferSnapshot = k;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = function() {
        function c(n, a) {
          void 0 === n && (n = "");
          this.name = n || "";
          this._startTime = k.isNullOrUndefined(a) ? jsGlobal.START_TIME : a;
        }
        c.prototype.getKind = function(c) {
          return this._kinds[c];
        };
        Object.defineProperty(c.prototype, "kinds", {get:function() {
          return this._kinds.concat();
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "depth", {get:function() {
          return this._depth;
        }, enumerable:!0, configurable:!0});
        c.prototype._initialize = function() {
          this._depth = 0;
          this._stack = [];
          this._data = [];
          this._kinds = [];
          this._kindNameMap = Object.create(null);
          this._marks = new k.CircularBuffer(Int32Array, 20);
          this._times = new k.CircularBuffer(Float64Array, 20);
        };
        c.prototype._getKindId = function(n) {
          var a = c.MAX_KINDID;
          if (void 0 === this._kindNameMap[n]) {
            if (a = this._kinds.length, a < c.MAX_KINDID) {
              var h = {id:a, name:n, visible:!0};
              this._kinds.push(h);
              this._kindNameMap[n] = h;
            } else {
              a = c.MAX_KINDID;
            }
          } else {
            a = this._kindNameMap[n].id;
          }
          return a;
        };
        c.prototype._getMark = function(n, a, h) {
          var p = c.MAX_DATAID;
          k.isNullOrUndefined(h) || a === c.MAX_KINDID || (p = this._data.length, p < c.MAX_DATAID ? this._data.push(h) : p = c.MAX_DATAID);
          return n | p << 16 | a;
        };
        c.prototype.enter = function(n, a, h) {
          h = (k.isNullOrUndefined(h) ? performance.now() : h) - this._startTime;
          this._marks || this._initialize();
          this._depth++;
          n = this._getKindId(n);
          this._marks.write(this._getMark(c.ENTER, n, a));
          this._times.write(h);
          this._stack.push(n);
        };
        c.prototype.leave = function(n, a, h) {
          h = (k.isNullOrUndefined(h) ? performance.now() : h) - this._startTime;
          var p = this._stack.pop();
          n && (p = this._getKindId(n));
          this._marks.write(this._getMark(c.LEAVE, p, a));
          this._times.write(h);
          this._depth--;
        };
        c.prototype.count = function(c, a, h) {
        };
        c.prototype.createSnapshot = function() {
          var n;
          void 0 === n && (n = Number.MAX_VALUE);
          if (!this._marks) {
            return null;
          }
          var a = this._times, h = this._kinds, p = this._data, l = new g.TimelineBufferSnapshot(this.name), u = [l], m = 0;
          this._marks || this._initialize();
          this._marks.forEachInReverse(function(l, v) {
            var d = p[l >>> 16 & c.MAX_DATAID], e = h[l & c.MAX_KINDID];
            if (k.isNullOrUndefined(e) || e.visible) {
              var b = l & 2147483648, f = a.get(v), q = u.length;
              if (b === c.LEAVE) {
                if (1 === q && (m++, m > n)) {
                  return!0;
                }
                u.push(new g.TimelineFrame(u[q - 1], e, null, d, NaN, f));
              } else {
                if (b === c.ENTER) {
                  if (e = u.pop(), b = u[u.length - 1]) {
                    for (b.children ? b.children.unshift(e) : b.children = [e], b = u.length, e.depth = b, e.startData = d, e.startTime = f;e;) {
                      if (e.maxDepth < b) {
                        e.maxDepth = b, e = e.parent;
                      } else {
                        break;
                      }
                    }
                  } else {
                    return!0;
                  }
                }
              }
            }
          });
          l.children && l.children.length && (l.startTime = l.children[0].startTime, l.endTime = l.children[l.children.length - 1].endTime);
          return l;
        };
        c.prototype.reset = function(c) {
          this._startTime = k.isNullOrUndefined(c) ? performance.now() : c;
          this._marks ? (this._depth = 0, this._data = [], this._marks.reset(), this._times.reset()) : this._initialize();
        };
        c.FromFirefoxProfile = function(n, a) {
          for (var h = n.profile.threads[0].samples, p = new c(a, h[0].time), l = [], g, m = 0;m < h.length;m++) {
            g = h[m];
            var s = g.time, k = g.frames, d = 0;
            for (g = Math.min(k.length, l.length);d < g && k[d].location === l[d].location;) {
              d++;
            }
            for (var e = l.length - d, b = 0;b < e;b++) {
              g = l.pop(), p.leave(g.location, null, s);
            }
            for (;d < k.length;) {
              g = k[d++], p.enter(g.location, null, s);
            }
            l = k;
          }
          for (;g = l.pop();) {
            p.leave(g.location, null, s);
          }
          return p;
        };
        c.FromChromeProfile = function(n, a) {
          var h = n.timestamps, p = n.samples, l = new c(a, h[0] / 1E3), g = [], m = {}, s;
          c._resolveIds(n.head, m);
          for (var k = 0;k < h.length;k++) {
            var d = h[k] / 1E3, e = [];
            for (s = m[p[k]];s;) {
              e.unshift(s), s = s.parent;
            }
            var b = 0;
            for (s = Math.min(e.length, g.length);b < s && e[b] === g[b];) {
              b++;
            }
            for (var f = g.length - b, q = 0;q < f;q++) {
              s = g.pop(), l.leave(s.functionName, null, d);
            }
            for (;b < e.length;) {
              s = e[b++], l.enter(s.functionName, null, d);
            }
            g = e;
          }
          for (;s = g.pop();) {
            l.leave(s.functionName, null, d);
          }
          return l;
        };
        c._resolveIds = function(n, a) {
          a[n.id] = n;
          if (n.children) {
            for (var h = 0;h < n.children.length;h++) {
              n.children[h].parent = n, c._resolveIds(n.children[h], a);
            }
          }
        };
        c.ENTER = 0;
        c.LEAVE = -2147483648;
        c.MAX_KINDID = 65535;
        c.MAX_DATAID = 32767;
        return c;
      }();
      g.TimelineBuffer = c;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      (function(c) {
        c[c.DARK = 0] = "DARK";
        c[c.LIGHT = 1] = "LIGHT";
      })(g.UIThemeType || (g.UIThemeType = {}));
      var c = function() {
        function c(n, a) {
          void 0 === a && (a = 0);
          this._container = n;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = a;
          this._tooltip = this._createTooltip();
        }
        c.prototype.createProfile = function(c, a) {
          var h = new g.Profile(c, a);
          h.createSnapshots();
          this._profiles.push(h);
          this.activateProfile(h);
          return h;
        };
        c.prototype.activateProfile = function(c) {
          this.deactivateProfile();
          this._activeProfile = c;
          this._createViews();
          this._initializeViews();
        };
        c.prototype.activateProfileAt = function(c) {
          this.activateProfile(this.getProfileAt(c));
        };
        c.prototype.deactivateProfile = function() {
          this._activeProfile && (this._destroyViews(), this._activeProfile = null);
        };
        c.prototype.resize = function() {
          this._onResize();
        };
        c.prototype.getProfileAt = function(c) {
          return this._profiles[c];
        };
        Object.defineProperty(c.prototype, "activeProfile", {get:function() {
          return this._activeProfile;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "profileCount", {get:function() {
          return this._profiles.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "container", {get:function() {
          return this._container;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "themeType", {get:function() {
          return this._themeType;
        }, set:function(c) {
          switch(c) {
            case 0:
              this._theme = new r.Theme.UIThemeDark;
              break;
            case 1:
              this._theme = new r.Theme.UIThemeLight;
          }
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "theme", {get:function() {
          return this._theme;
        }, enumerable:!0, configurable:!0});
        c.prototype.getSnapshotAt = function(c) {
          return this._activeProfile.getSnapshotAt(c);
        };
        c.prototype._createViews = function() {
          if (this._activeProfile) {
            var c = this;
            this._overviewHeader = new g.FlameChartHeader(this, 0);
            this._overview = new g.FlameChartOverview(this, 0);
            this._activeProfile.forEachSnapshot(function(a, h) {
              c._headers.push(new g.FlameChartHeader(c, 1));
              c._charts.push(new g.FlameChart(c, a));
            });
            window.addEventListener("resize", this._onResize.bind(this));
          }
        };
        c.prototype._destroyViews = function() {
          if (this._activeProfile) {
            this._overviewHeader.destroy();
            for (this._overview.destroy();this._headers.length;) {
              this._headers.pop().destroy();
            }
            for (;this._charts.length;) {
              this._charts.pop().destroy();
            }
            window.removeEventListener("resize", this._onResize.bind(this));
          }
        };
        c.prototype._initializeViews = function() {
          if (this._activeProfile) {
            var c = this, a = this._activeProfile.startTime, h = this._activeProfile.endTime;
            this._overviewHeader.initialize(a, h);
            this._overview.initialize(a, h);
            this._activeProfile.forEachSnapshot(function(p, l) {
              c._headers[l].initialize(a, h);
              c._charts[l].initialize(a, h);
            });
          }
        };
        c.prototype._onResize = function() {
          if (this._activeProfile) {
            var c = this, a = this._container.offsetWidth;
            this._overviewHeader.setSize(a);
            this._overview.setSize(a);
            this._activeProfile.forEachSnapshot(function(h, p) {
              c._headers[p].setSize(a);
              c._charts[p].setSize(a);
            });
          }
        };
        c.prototype._updateViews = function() {
          if (this._activeProfile) {
            var c = this, a = this._activeProfile.windowStart, h = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(a, h);
            this._overview.setWindow(a, h);
            this._activeProfile.forEachSnapshot(function(p, l) {
              c._headers[l].setWindow(a, h);
              c._charts[l].setWindow(a, h);
            });
          }
        };
        c.prototype._drawViews = function() {
        };
        c.prototype._createTooltip = function() {
          var c = document.createElement("div");
          c.classList.add("profiler-tooltip");
          c.style.display = "none";
          this._container.insertBefore(c, this._container.firstChild);
          return c;
        };
        c.prototype.setWindow = function(c, a) {
          this._activeProfile.setWindow(c, a);
          this._updateViews();
        };
        c.prototype.moveWindowTo = function(c) {
          this._activeProfile.moveWindowTo(c);
          this._updateViews();
        };
        c.prototype.showTooltip = function(c, a, h, p) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(c, a));
          this._tooltip.style.display = "block";
          var l = this._tooltip.firstChild;
          a = l.clientWidth;
          l = l.clientHeight;
          h += h + a >= c.canvas.clientWidth - 50 ? -(a + 20) : 25;
          p += c.canvas.offsetTop - l / 2;
          this._tooltip.style.left = h + "px";
          this._tooltip.style.top = p + "px";
        };
        c.prototype.hideTooltip = function() {
          this._tooltip.style.display = "none";
        };
        c.prototype.createTooltipContent = function(c, a) {
          var h = Math.round(1E5 * a.totalTime) / 1E5, p = Math.round(1E5 * a.selfTime) / 1E5, l = Math.round(1E4 * a.selfTime / a.totalTime) / 100, g = document.createElement("div"), m = document.createElement("h1");
          m.textContent = a.kind.name;
          g.appendChild(m);
          m = document.createElement("p");
          m.textContent = "Total: " + h + " ms";
          g.appendChild(m);
          h = document.createElement("p");
          h.textContent = "Self: " + p + " ms (" + l + "%)";
          g.appendChild(h);
          if (p = c.getStatistics(a.kind)) {
            l = document.createElement("p"), l.textContent = "Count: " + p.count, g.appendChild(l), l = Math.round(1E5 * p.totalTime) / 1E5, h = document.createElement("p"), h.textContent = "All Total: " + l + " ms", g.appendChild(h), p = Math.round(1E5 * p.selfTime) / 1E5, l = document.createElement("p"), l.textContent = "All Self: " + p + " ms", g.appendChild(l);
          }
          this.appendDataElements(g, a.startData);
          this.appendDataElements(g, a.endData);
          return g;
        };
        c.prototype.appendDataElements = function(c, a) {
          if (!k.isNullOrUndefined(a)) {
            c.appendChild(document.createElement("hr"));
            var h;
            if (k.isObject(a)) {
              for (var p in a) {
                h = document.createElement("p"), h.textContent = p + ": " + a[p], c.appendChild(h);
              }
            } else {
              h = document.createElement("p"), h.textContent = a.toString(), c.appendChild(h);
            }
          }
        };
        c.prototype.removeTooltipContent = function() {
          for (var c = this._tooltip;c.firstChild;) {
            c.removeChild(c.firstChild);
          }
        };
        return c;
      }();
      g.Controller = c;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.NumberUtilities.clamp, r = function() {
        function a(a) {
          this.value = a;
        }
        a.prototype.toString = function() {
          return this.value;
        };
        a.AUTO = new a("auto");
        a.DEFAULT = new a("default");
        a.NONE = new a("none");
        a.HELP = new a("help");
        a.POINTER = new a("pointer");
        a.PROGRESS = new a("progress");
        a.WAIT = new a("wait");
        a.CELL = new a("cell");
        a.CROSSHAIR = new a("crosshair");
        a.TEXT = new a("text");
        a.ALIAS = new a("alias");
        a.COPY = new a("copy");
        a.MOVE = new a("move");
        a.NO_DROP = new a("no-drop");
        a.NOT_ALLOWED = new a("not-allowed");
        a.ALL_SCROLL = new a("all-scroll");
        a.COL_RESIZE = new a("col-resize");
        a.ROW_RESIZE = new a("row-resize");
        a.N_RESIZE = new a("n-resize");
        a.E_RESIZE = new a("e-resize");
        a.S_RESIZE = new a("s-resize");
        a.W_RESIZE = new a("w-resize");
        a.NE_RESIZE = new a("ne-resize");
        a.NW_RESIZE = new a("nw-resize");
        a.SE_RESIZE = new a("se-resize");
        a.SW_RESIZE = new a("sw-resize");
        a.EW_RESIZE = new a("ew-resize");
        a.NS_RESIZE = new a("ns-resize");
        a.NESW_RESIZE = new a("nesw-resize");
        a.NWSE_RESIZE = new a("nwse-resize");
        a.ZOOM_IN = new a("zoom-in");
        a.ZOOM_OUT = new a("zoom-out");
        a.GRAB = new a("grab");
        a.GRABBING = new a("grabbing");
        return a;
      }();
      g.MouseCursor = r;
      var n = function() {
        function a(a, c) {
          this._target = a;
          this._eventTarget = c;
          this._wheelDisabled = !1;
          this._boundOnMouseDown = this._onMouseDown.bind(this);
          this._boundOnMouseUp = this._onMouseUp.bind(this);
          this._boundOnMouseOver = this._onMouseOver.bind(this);
          this._boundOnMouseOut = this._onMouseOut.bind(this);
          this._boundOnMouseMove = this._onMouseMove.bind(this);
          this._boundOnMouseWheel = this._onMouseWheel.bind(this);
          this._boundOnDrag = this._onDrag.bind(this);
          c.addEventListener("mousedown", this._boundOnMouseDown, !1);
          c.addEventListener("mouseover", this._boundOnMouseOver, !1);
          c.addEventListener("mouseout", this._boundOnMouseOut, !1);
          c.addEventListener("onwheel" in document ? "wheel" : "mousewheel", this._boundOnMouseWheel, !1);
        }
        a.prototype.destroy = function() {
          var a = this._eventTarget;
          a.removeEventListener("mousedown", this._boundOnMouseDown);
          a.removeEventListener("mouseover", this._boundOnMouseOver);
          a.removeEventListener("mouseout", this._boundOnMouseOut);
          a.removeEventListener("onwheel" in document ? "wheel" : "mousewheel", this._boundOnMouseWheel);
          window.removeEventListener("mousemove", this._boundOnDrag);
          window.removeEventListener("mouseup", this._boundOnMouseUp);
          this._killHoverCheck();
          this._target = this._eventTarget = null;
        };
        a.prototype.updateCursor = function(h) {
          if (!a._cursorOwner || a._cursorOwner === this._target) {
            var c = this._eventTarget.parentElement;
            a._cursor !== h && (a._cursor = h, ["", "-moz-", "-webkit-"].forEach(function(a) {
              c.style.cursor = a + h;
            }));
            a._cursorOwner = a._cursor === r.DEFAULT ? null : this._target;
          }
        };
        a.prototype._onMouseDown = function(a) {
          this._killHoverCheck();
          if (0 === a.button) {
            var c = this._getTargetMousePos(a, a.target);
            this._dragInfo = {start:c, current:c, delta:{x:0, y:0}, hasMoved:!1, originalTarget:a.target};
            window.addEventListener("mousemove", this._boundOnDrag, !1);
            window.addEventListener("mouseup", this._boundOnMouseUp, !1);
            this._target.onMouseDown(c.x, c.y);
          }
        };
        a.prototype._onDrag = function(a) {
          var c = this._dragInfo;
          a = this._getTargetMousePos(a, c.originalTarget);
          var l = {x:a.x - c.start.x, y:a.y - c.start.y};
          c.current = a;
          c.delta = l;
          c.hasMoved = !0;
          this._target.onDrag(c.start.x, c.start.y, a.x, a.y, l.x, l.y);
        };
        a.prototype._onMouseUp = function(a) {
          window.removeEventListener("mousemove", this._boundOnDrag);
          window.removeEventListener("mouseup", this._boundOnMouseUp);
          var c = this;
          a = this._dragInfo;
          if (a.hasMoved) {
            this._target.onDragEnd(a.start.x, a.start.y, a.current.x, a.current.y, a.delta.x, a.delta.y);
          } else {
            this._target.onClick(a.current.x, a.current.y);
          }
          this._dragInfo = null;
          this._wheelDisabled = !0;
          setTimeout(function() {
            c._wheelDisabled = !1;
          }, 500);
        };
        a.prototype._onMouseOver = function(a) {
          a.target.addEventListener("mousemove", this._boundOnMouseMove, !1);
          if (!this._dragInfo) {
            var c = this._getTargetMousePos(a, a.target);
            this._target.onMouseOver(c.x, c.y);
            this._startHoverCheck(a);
          }
        };
        a.prototype._onMouseOut = function(a) {
          a.target.removeEventListener("mousemove", this._boundOnMouseMove, !1);
          if (!this._dragInfo) {
            this._target.onMouseOut();
          }
          this._killHoverCheck();
        };
        a.prototype._onMouseMove = function(a) {
          if (!this._dragInfo) {
            var c = this._getTargetMousePos(a, a.target);
            this._target.onMouseMove(c.x, c.y);
            this._killHoverCheck();
            this._startHoverCheck(a);
          }
        };
        a.prototype._onMouseWheel = function(a) {
          if (!(a.altKey || a.metaKey || a.ctrlKey || a.shiftKey || (a.preventDefault(), this._dragInfo || this._wheelDisabled))) {
            var p = this._getTargetMousePos(a, a.target);
            a = c("undefined" !== typeof a.deltaY ? a.deltaY / 16 : -a.wheelDelta / 40, -1, 1);
            this._target.onMouseWheel(p.x, p.y, Math.pow(1.2, a) - 1);
          }
        };
        a.prototype._startHoverCheck = function(c) {
          this._hoverInfo = {isHovering:!1, timeoutHandle:setTimeout(this._onMouseMoveIdleHandler.bind(this), a.HOVER_TIMEOUT), pos:this._getTargetMousePos(c, c.target)};
        };
        a.prototype._killHoverCheck = function() {
          if (this._hoverInfo) {
            clearTimeout(this._hoverInfo.timeoutHandle);
            if (this._hoverInfo.isHovering) {
              this._target.onHoverEnd();
            }
            this._hoverInfo = null;
          }
        };
        a.prototype._onMouseMoveIdleHandler = function() {
          var a = this._hoverInfo;
          a.isHovering = !0;
          this._target.onHoverStart(a.pos.x, a.pos.y);
        };
        a.prototype._getTargetMousePos = function(a, c) {
          var l = c.getBoundingClientRect();
          return{x:a.clientX - l.left, y:a.clientY - l.top};
        };
        a.HOVER_TIMEOUT = 500;
        a._cursor = r.DEFAULT;
        return a;
      }();
      g.MouseController = n;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      (function(c) {
        c[c.NONE = 0] = "NONE";
        c[c.WINDOW = 1] = "WINDOW";
        c[c.HANDLE_LEFT = 2] = "HANDLE_LEFT";
        c[c.HANDLE_RIGHT = 3] = "HANDLE_RIGHT";
        c[c.HANDLE_BOTH = 4] = "HANDLE_BOTH";
      })(g.FlameChartDragTarget || (g.FlameChartDragTarget = {}));
      var c = function() {
        function c(n) {
          this._controller = n;
          this._initialized = !1;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
          this._mouseController = new g.MouseController(this, this._canvas);
          n = n.container;
          n.appendChild(this._canvas);
          n = n.getBoundingClientRect();
          this.setSize(n.width);
        }
        Object.defineProperty(c.prototype, "canvas", {get:function() {
          return this._canvas;
        }, enumerable:!0, configurable:!0});
        c.prototype.setSize = function(c, a) {
          void 0 === a && (a = 20);
          this._width = c;
          this._height = a;
          this._resetCanvas();
          this.draw();
        };
        c.prototype.initialize = function(c, a) {
          this._initialized = !0;
          this.setRange(c, a);
          this.setWindow(c, a, !1);
          this.draw();
        };
        c.prototype.setWindow = function(c, a, h) {
          void 0 === h && (h = !0);
          this._windowStart = c;
          this._windowEnd = a;
          !h || this.draw();
        };
        c.prototype.setRange = function(c, a) {
          var h = !1;
          void 0 === h && (h = !0);
          this._rangeStart = c;
          this._rangeEnd = a;
          !h || this.draw();
        };
        c.prototype.destroy = function() {
          this._mouseController.destroy();
          this._mouseController = null;
          this._controller.container.removeChild(this._canvas);
          this._controller = null;
        };
        c.prototype._resetCanvas = function() {
          var c = window.devicePixelRatio, a = this._canvas;
          a.width = this._width * c;
          a.height = this._height * c;
          a.style.width = this._width + "px";
          a.style.height = this._height + "px";
        };
        c.prototype.draw = function() {
        };
        c.prototype._almostEq = function(c, a) {
          var h;
          void 0 === h && (h = 10);
          return Math.abs(c - a) < 1 / Math.pow(10, h);
        };
        c.prototype._windowEqRange = function() {
          return this._almostEq(this._windowStart, this._rangeStart) && this._almostEq(this._windowEnd, this._rangeEnd);
        };
        c.prototype._decimalPlaces = function(c) {
          return(+c).toFixed(10).replace(/^-?\d*\.?|0+$/g, "").length;
        };
        c.prototype._toPixelsRelative = function(c) {
          return 0;
        };
        c.prototype._toPixels = function(c) {
          return 0;
        };
        c.prototype._toTimeRelative = function(c) {
          return 0;
        };
        c.prototype._toTime = function(c) {
          return 0;
        };
        c.prototype.onMouseWheel = function(g, a, h) {
          g = this._toTime(g);
          a = this._windowStart;
          var p = this._windowEnd, l = p - a;
          h = Math.max((c.MIN_WINDOW_LEN - l) / l, h);
          this._controller.setWindow(a + (a - g) * h, p + (p - g) * h);
          this.onHoverEnd();
        };
        c.prototype.onMouseDown = function(c, a) {
        };
        c.prototype.onMouseMove = function(c, a) {
        };
        c.prototype.onMouseOver = function(c, a) {
        };
        c.prototype.onMouseOut = function() {
        };
        c.prototype.onDrag = function(c, a, h, p, l, g) {
        };
        c.prototype.onDragEnd = function(c, a, h, p, l, g) {
        };
        c.prototype.onClick = function(c, a) {
        };
        c.prototype.onHoverStart = function(c, a) {
        };
        c.prototype.onHoverEnd = function() {
        };
        c.DRAGHANDLE_WIDTH = 4;
        c.MIN_WINDOW_LEN = .1;
        return c;
      }();
      g.FlameChartBase = c;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.StringUtilities.trimMiddle, r = function(n) {
        function a(a, c) {
          n.call(this, a);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = c;
          this._kindStyle = Object.create(null);
        }
        __extends(a, n);
        a.prototype.setSize = function(a, c) {
          n.prototype.setSize.call(this, a, c || this._initialized ? 12.5 * this._maxDepth : 100);
        };
        a.prototype.initialize = function(a, c) {
          this._initialized = !0;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(a, c);
          this.setWindow(a, c, !1);
          this.setSize(this._width, 12.5 * this._maxDepth);
        };
        a.prototype.destroy = function() {
          n.prototype.destroy.call(this);
          this._snapshot = null;
        };
        a.prototype.draw = function() {
          var a = this._context, c = window.devicePixelRatio;
          k.ColorStyle.reset();
          a.save();
          a.scale(c, c);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, this._width, this._height);
          this._initialized && this._drawChildren(this._snapshot);
          a.restore();
        };
        a.prototype._drawChildren = function(a, c) {
          void 0 === c && (c = 0);
          var l = a.getChildRange(this._windowStart, this._windowEnd);
          if (l) {
            for (var g = l.startIndex;g <= l.endIndex;g++) {
              var m = a.children[g];
              this._drawFrame(m, c) && this._drawChildren(m, c + 1);
            }
          }
        };
        a.prototype._drawFrame = function(a, c) {
          var l = this._context, g = this._toPixels(a.startTime), m = this._toPixels(a.endTime), s = m - g;
          if (s <= this._minFrameWidthInPixels) {
            return l.fillStyle = this._controller.theme.tabToolbar(1), l.fillRect(g, 12.5 * c, this._minFrameWidthInPixels, 12 + 12.5 * (a.maxDepth - a.depth)), !1;
          }
          0 > g && (m = s + g, g = 0);
          var m = m - g, n = this._kindStyle[a.kind.id];
          n || (n = k.ColorStyle.randomStyle(), n = this._kindStyle[a.kind.id] = {bgColor:n, textColor:k.ColorStyle.contrastStyle(n)});
          l.save();
          l.fillStyle = n.bgColor;
          l.fillRect(g, 12.5 * c, m, 12);
          12 < s && (s = a.kind.name) && s.length && (s = this._prepareText(l, s, m - 4), s.length && (l.fillStyle = n.textColor, l.textBaseline = "bottom", l.fillText(s, g + 2, 12.5 * (c + 1) - 1)));
          l.restore();
          return!0;
        };
        a.prototype._prepareText = function(a, p, l) {
          var g = this._measureWidth(a, p);
          if (l > g) {
            return p;
          }
          for (var g = 3, m = p.length;g < m;) {
            var s = g + m >> 1;
            this._measureWidth(a, c(p, s)) < l ? g = s + 1 : m = s;
          }
          p = c(p, m - 1);
          g = this._measureWidth(a, p);
          return g <= l ? p : "";
        };
        a.prototype._measureWidth = function(a, c) {
          var l = this._textWidth[c];
          l || (l = a.measureText(c).width, this._textWidth[c] = l);
          return l;
        };
        a.prototype._toPixelsRelative = function(a) {
          return a * this._width / (this._windowEnd - this._windowStart);
        };
        a.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - this._windowStart);
        };
        a.prototype._toTimeRelative = function(a) {
          return a * (this._windowEnd - this._windowStart) / this._width;
        };
        a.prototype._toTime = function(a) {
          return this._toTimeRelative(a) + this._windowStart;
        };
        a.prototype._getFrameAtPosition = function(a, c) {
          var l = 1 + c / 12.5 | 0, g = this._snapshot.query(this._toTime(a));
          if (g && g.depth >= l) {
            for (;g && g.depth > l;) {
              g = g.parent;
            }
            return g;
          }
          return null;
        };
        a.prototype.onMouseDown = function(a, c) {
          this._windowEqRange() || (this._mouseController.updateCursor(g.MouseCursor.ALL_SCROLL), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:1});
        };
        a.prototype.onMouseMove = function(a, c) {
        };
        a.prototype.onMouseOver = function(a, c) {
        };
        a.prototype.onMouseOut = function() {
        };
        a.prototype.onDrag = function(a, c, l, g, m, s) {
          if (a = this._dragInfo) {
            m = this._toTimeRelative(-m), this._controller.setWindow(a.windowStartInitial + m, a.windowEndInitial + m);
          }
        };
        a.prototype.onDragEnd = function(a, c, l, n, m, s) {
          this._dragInfo = null;
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onClick = function(a, c) {
          this._dragInfo = null;
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onHoverStart = function(a, c) {
          var l = this._getFrameAtPosition(a, c);
          l && (this._hoveredFrame = l, this._controller.showTooltip(this, l, a, c));
        };
        a.prototype.onHoverEnd = function() {
          this._hoveredFrame && (this._hoveredFrame = null, this._controller.hideTooltip());
        };
        a.prototype.getStatistics = function(a) {
          var c = this._snapshot;
          c.statistics || c.calculateStatistics();
          return c.statistics[a.id];
        };
        return a;
      }(g.FlameChartBase);
      g.FlameChart = r;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.NumberUtilities.clamp;
      (function(c) {
        c[c.OVERLAY = 0] = "OVERLAY";
        c[c.STACK = 1] = "STACK";
        c[c.UNION = 2] = "UNION";
      })(g.FlameChartOverviewMode || (g.FlameChartOverviewMode = {}));
      var r = function(k) {
        function a(a, c) {
          void 0 === c && (c = 1);
          this._mode = c;
          this._overviewCanvasDirty = !0;
          this._overviewCanvas = document.createElement("canvas");
          this._overviewContext = this._overviewCanvas.getContext("2d");
          k.call(this, a);
        }
        __extends(a, k);
        a.prototype.setSize = function(a, c) {
          k.prototype.setSize.call(this, a, c || 64);
        };
        Object.defineProperty(a.prototype, "mode", {set:function(a) {
          this._mode = a;
          this.draw();
        }, enumerable:!0, configurable:!0});
        a.prototype._resetCanvas = function() {
          k.prototype._resetCanvas.call(this);
          this._overviewCanvas.width = this._canvas.width;
          this._overviewCanvas.height = this._canvas.height;
          this._overviewCanvasDirty = !0;
        };
        a.prototype.draw = function() {
          var a = this._context, c = window.devicePixelRatio, l = this._width, g = this._height;
          a.save();
          a.scale(c, c);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, l, g);
          a.restore();
          this._initialized && (this._overviewCanvasDirty && (this._drawChart(), this._overviewCanvasDirty = !1), a.drawImage(this._overviewCanvas, 0, 0), this._drawSelection());
        };
        a.prototype._drawSelection = function() {
          var a = this._context, c = this._height, l = window.devicePixelRatio, g = this._selection ? this._selection.left : this._toPixels(this._windowStart), m = this._selection ? this._selection.right : this._toPixels(this._windowEnd), s = this._controller.theme;
          a.save();
          a.scale(l, l);
          this._selection ? (a.fillStyle = s.selectionText(.15), a.fillRect(g, 1, m - g, c - 1), a.fillStyle = "rgba(133, 0, 0, 1)", a.fillRect(g + .5, 0, m - g - 1, 4), a.fillRect(g + .5, c - 4, m - g - 1, 4)) : (a.fillStyle = s.bodyBackground(.4), a.fillRect(0, 1, g, c - 1), a.fillRect(m, 1, this._width, c - 1));
          a.beginPath();
          a.moveTo(g, 0);
          a.lineTo(g, c);
          a.moveTo(m, 0);
          a.lineTo(m, c);
          a.lineWidth = .5;
          a.strokeStyle = s.foregroundTextGrey(1);
          a.stroke();
          c = Math.abs((this._selection ? this._toTime(this._selection.right) : this._windowEnd) - (this._selection ? this._toTime(this._selection.left) : this._windowStart));
          a.fillStyle = s.selectionText(.5);
          a.font = "8px sans-serif";
          a.textBaseline = "alphabetic";
          a.textAlign = "end";
          a.fillText(c.toFixed(2), Math.min(g, m) - 4, 10);
          a.fillText((c / 60).toFixed(2), Math.min(g, m) - 4, 20);
          a.restore();
        };
        a.prototype._drawChart = function() {
          var a = window.devicePixelRatio, c = this._height, l = this._controller.activeProfile, g = 4 * this._width, m = l.totalTime / g, s = this._overviewContext, k = this._controller.theme.blueHighlight(1);
          s.save();
          s.translate(0, a * c);
          var d = -a * c / (l.maxDepth - 1);
          s.scale(a / 4, d);
          s.clearRect(0, 0, g, l.maxDepth - 1);
          1 == this._mode && s.scale(1, 1 / l.snapshotCount);
          for (var e = 0, b = l.snapshotCount;e < b;e++) {
            var f = l.getSnapshotAt(e);
            if (f) {
              var q = null, w = 0;
              s.beginPath();
              s.moveTo(0, 0);
              for (var G = 0;G < g;G++) {
                w = l.startTime + G * m, w = (q = q ? q.queryNext(w) : f.query(w)) ? q.getDepth() - 1 : 0, s.lineTo(G, w);
              }
              s.lineTo(G, 0);
              s.fillStyle = k;
              s.fill();
              1 == this._mode && s.translate(0, -c * a / d);
            }
          }
          s.restore();
        };
        a.prototype._toPixelsRelative = function(a) {
          return a * this._width / (this._rangeEnd - this._rangeStart);
        };
        a.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - this._rangeStart);
        };
        a.prototype._toTimeRelative = function(a) {
          return a * (this._rangeEnd - this._rangeStart) / this._width;
        };
        a.prototype._toTime = function(a) {
          return this._toTimeRelative(a) + this._rangeStart;
        };
        a.prototype._getDragTargetUnderCursor = function(a, c) {
          if (0 <= c && c < this._height) {
            var l = this._toPixels(this._windowStart), k = this._toPixels(this._windowEnd), m = 2 + g.FlameChartBase.DRAGHANDLE_WIDTH / 2, s = a >= l - m && a <= l + m, n = a >= k - m && a <= k + m;
            if (s && n) {
              return 4;
            }
            if (s) {
              return 2;
            }
            if (n) {
              return 3;
            }
            if (!this._windowEqRange() && a > l + m && a < k - m) {
              return 1;
            }
          }
          return 0;
        };
        a.prototype.onMouseDown = function(a, c) {
          var l = this._getDragTargetUnderCursor(a, c);
          0 === l ? (this._selection = {left:a, right:a}, this.draw()) : (1 === l && this._mouseController.updateCursor(g.MouseCursor.GRABBING), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:l});
        };
        a.prototype.onMouseMove = function(a, c) {
          var l = g.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, c);
          0 === k || this._selection || (l = 1 === k ? g.MouseCursor.GRAB : g.MouseCursor.EW_RESIZE);
          this._mouseController.updateCursor(l);
        };
        a.prototype.onMouseOver = function(a, c) {
          this.onMouseMove(a, c);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, p, l, k, m, s) {
          if (this._selection) {
            this._selection = {left:a, right:c(l, 0, this._width - 1)}, this.draw();
          } else {
            a = this._dragInfo;
            if (4 === a.target) {
              if (0 !== m) {
                a.target = 0 > m ? 2 : 3;
              } else {
                return;
              }
            }
            p = this._windowStart;
            l = this._windowEnd;
            m = this._toTimeRelative(m);
            switch(a.target) {
              case 1:
                p = a.windowStartInitial + m;
                l = a.windowEndInitial + m;
                break;
              case 2:
                p = c(a.windowStartInitial + m, this._rangeStart, l - g.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3:
                l = c(a.windowEndInitial + m, p + g.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(p, l);
          }
        };
        a.prototype.onDragEnd = function(a, c, l, g, m, s) {
          this._selection && (this._selection = null, this._controller.setWindow(this._toTime(a), this._toTime(l)));
          this._dragInfo = null;
          this.onMouseMove(l, g);
        };
        a.prototype.onClick = function(a, c) {
          this._selection = this._dragInfo = null;
          this._windowEqRange() || (0 === this._getDragTargetUnderCursor(a, c) && this._controller.moveWindowTo(this._toTime(a)), this.onMouseMove(a, c));
          this.draw();
        };
        a.prototype.onHoverStart = function(a, c) {
        };
        a.prototype.onHoverEnd = function() {
        };
        return a;
      }(g.FlameChartBase);
      g.FlameChartOverview = r;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.NumberUtilities.clamp;
      (function(c) {
        c[c.OVERVIEW = 0] = "OVERVIEW";
        c[c.CHART = 1] = "CHART";
      })(g.FlameChartHeaderType || (g.FlameChartHeaderType = {}));
      var r = function(k) {
        function a(a, c) {
          this._type = c;
          k.call(this, a);
        }
        __extends(a, k);
        a.prototype.draw = function() {
          var a = this._context, c = window.devicePixelRatio, l = this._width, g = this._height;
          a.save();
          a.scale(c, c);
          a.fillStyle = this._controller.theme.tabToolbar(1);
          a.fillRect(0, 0, l, g);
          this._initialized && (0 == this._type ? (c = this._toPixels(this._windowStart), l = this._toPixels(this._windowEnd), a.fillStyle = this._controller.theme.bodyBackground(1), a.fillRect(c, 0, l - c, g), this._drawLabels(this._rangeStart, this._rangeEnd), this._drawDragHandle(c), this._drawDragHandle(l)) : this._drawLabels(this._windowStart, this._windowEnd));
          a.restore();
        };
        a.prototype._drawLabels = function(c, p) {
          var l = this._context, g = this._calculateTickInterval(c, p), m = Math.ceil(c / g) * g, s = 500 <= g, k = s ? 1E3 : 1, d = this._decimalPlaces(g / k), s = s ? "s" : "ms", e = this._toPixels(m), b = this._height / 2, f = this._controller.theme;
          l.lineWidth = 1;
          l.strokeStyle = f.contentTextDarkGrey(.5);
          l.fillStyle = f.contentTextDarkGrey(1);
          l.textAlign = "right";
          l.textBaseline = "middle";
          l.font = "11px sans-serif";
          for (f = this._width + a.TICK_MAX_WIDTH;e < f;) {
            l.fillText((m / k).toFixed(d) + " " + s, e - 7, b + 1), l.beginPath(), l.moveTo(e, 0), l.lineTo(e, this._height + 1), l.closePath(), l.stroke(), m += g, e = this._toPixels(m);
          }
        };
        a.prototype._calculateTickInterval = function(c, p) {
          var l = (p - c) / (this._width / a.TICK_MAX_WIDTH), g = Math.pow(10, Math.floor(Math.log(l) / Math.LN10)), l = l / g;
          return 5 < l ? 10 * g : 2 < l ? 5 * g : 1 < l ? 2 * g : g;
        };
        a.prototype._drawDragHandle = function(a) {
          var c = this._context;
          c.lineWidth = 2;
          c.strokeStyle = this._controller.theme.bodyBackground(1);
          c.fillStyle = this._controller.theme.foregroundTextGrey(.7);
          this._drawRoundedRect(c, a - g.FlameChartBase.DRAGHANDLE_WIDTH / 2, g.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2);
        };
        a.prototype._drawRoundedRect = function(a, c, l, g) {
          var m, s = !0;
          void 0 === s && (s = !0);
          void 0 === m && (m = !0);
          a.beginPath();
          a.moveTo(c + 2, 1);
          a.lineTo(c + l - 2, 1);
          a.quadraticCurveTo(c + l, 1, c + l, 3);
          a.lineTo(c + l, 1 + g - 2);
          a.quadraticCurveTo(c + l, 1 + g, c + l - 2, 1 + g);
          a.lineTo(c + 2, 1 + g);
          a.quadraticCurveTo(c, 1 + g, c, 1 + g - 2);
          a.lineTo(c, 3);
          a.quadraticCurveTo(c, 1, c + 2, 1);
          a.closePath();
          s && a.stroke();
          m && a.fill();
        };
        a.prototype._toPixelsRelative = function(a) {
          return a * this._width / (0 === this._type ? this._rangeEnd - this._rangeStart : this._windowEnd - this._windowStart);
        };
        a.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - (0 === this._type ? this._rangeStart : this._windowStart));
        };
        a.prototype._toTimeRelative = function(a) {
          return a * (0 === this._type ? this._rangeEnd - this._rangeStart : this._windowEnd - this._windowStart) / this._width;
        };
        a.prototype._toTime = function(a) {
          return this._toTimeRelative(a) + (0 === this._type ? this._rangeStart : this._windowStart);
        };
        a.prototype._getDragTargetUnderCursor = function(a, c) {
          if (0 <= c && c < this._height) {
            if (0 === this._type) {
              var l = this._toPixels(this._windowStart), k = this._toPixels(this._windowEnd), m = 2 + g.FlameChartBase.DRAGHANDLE_WIDTH / 2, l = a >= l - m && a <= l + m, k = a >= k - m && a <= k + m;
              if (l && k) {
                return 4;
              }
              if (l) {
                return 2;
              }
              if (k) {
                return 3;
              }
            }
            if (!this._windowEqRange()) {
              return 1;
            }
          }
          return 0;
        };
        a.prototype.onMouseDown = function(a, c) {
          var l = this._getDragTargetUnderCursor(a, c);
          1 === l && this._mouseController.updateCursor(g.MouseCursor.GRABBING);
          this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:l};
        };
        a.prototype.onMouseMove = function(a, c) {
          var l = g.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, c);
          0 !== k && (1 !== k ? l = g.MouseCursor.EW_RESIZE : 1 !== k || this._windowEqRange() || (l = g.MouseCursor.GRAB));
          this._mouseController.updateCursor(l);
        };
        a.prototype.onMouseOver = function(a, c) {
          this.onMouseMove(a, c);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, p, l, k, m, s) {
          a = this._dragInfo;
          if (4 === a.target) {
            if (0 !== m) {
              a.target = 0 > m ? 2 : 3;
            } else {
              return;
            }
          }
          p = this._windowStart;
          l = this._windowEnd;
          m = this._toTimeRelative(m);
          switch(a.target) {
            case 1:
              l = 0 === this._type ? 1 : -1;
              p = a.windowStartInitial + l * m;
              l = a.windowEndInitial + l * m;
              break;
            case 2:
              p = c(a.windowStartInitial + m, this._rangeStart, l - g.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3:
              l = c(a.windowEndInitial + m, p + g.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(p, l);
        };
        a.prototype.onDragEnd = function(a, c, l, g, m, s) {
          this._dragInfo = null;
          this.onMouseMove(l, g);
        };
        a.prototype.onClick = function(a, c) {
          1 === this._dragInfo.target && this._mouseController.updateCursor(g.MouseCursor.GRAB);
        };
        a.prototype.onHoverStart = function(a, c) {
        };
        a.prototype.onHoverEnd = function() {
        };
        a.TICK_MAX_WIDTH = 75;
        return a;
      }(g.FlameChartBase);
      g.FlameChartHeader = r;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      (function(c) {
        var g = function() {
          function a(a, c, g, k, m) {
            this.pageLoaded = a;
            this.threadsTotal = c;
            this.threadsLoaded = g;
            this.threadFilesTotal = k;
            this.threadFilesLoaded = m;
          }
          a.prototype.toString = function() {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function(a, c, g) {
              return a + ":" + this[a];
            }, this).join(", ") + "]";
          };
          return a;
        }();
        c.TraceLoggerProgressInfo = g;
        var k = function() {
          function a(a) {
            this._baseUrl = a;
            this._threads = [];
            this._progressInfo = null;
          }
          a.prototype.loadPage = function(a, c, l) {
            this._threads = [];
            this._pageLoadCallback = c;
            this._pageLoadProgressCallback = l;
            this._progressInfo = new g(!1, 0, 0, 0, 0);
            this._loadData([a], this._onLoadPage.bind(this));
          };
          Object.defineProperty(a.prototype, "buffers", {get:function() {
            for (var a = [], c = 0, g = this._threads.length;c < g;c++) {
              a.push(this._threads[c].buffer);
            }
            return a;
          }, enumerable:!0, configurable:!0});
          a.prototype._onProgress = function() {
            this._pageLoadProgressCallback && this._pageLoadProgressCallback.call(this, this._progressInfo);
          };
          a.prototype._onLoadPage = function(a) {
            if (a && 1 == a.length) {
              var g = this, l = 0;
              a = a[0];
              var k = a.length;
              this._threads = Array(k);
              this._progressInfo.pageLoaded = !0;
              this._progressInfo.threadsTotal = k;
              for (var m = 0;m < a.length;m++) {
                var s = a[m], n = [s.dict, s.tree];
                s.corrections && n.push(s.corrections);
                this._progressInfo.threadFilesTotal += n.length;
                this._loadData(n, function(a) {
                  return function(e) {
                    e && (e = new c.Thread(e), e.buffer.name = "Thread " + a, g._threads[a] = e);
                    l++;
                    g._progressInfo.threadsLoaded++;
                    g._onProgress();
                    l === k && g._pageLoadCallback.call(g, null, g._threads);
                  };
                }(m), function(a) {
                  g._progressInfo.threadFilesLoaded++;
                  g._onProgress();
                });
              }
              this._onProgress();
            } else {
              this._pageLoadCallback.call(this, "Error loading page.", null);
            }
          };
          a.prototype._loadData = function(a, c, g) {
            var k = 0, m = 0, s = a.length, n = [];
            n.length = s;
            for (var d = 0;d < s;d++) {
              var e = this._baseUrl + a[d], b = new XMLHttpRequest, f = /\.tl$/i.test(e) ? "arraybuffer" : "json";
              b.open("GET", e, !0);
              b.responseType = f;
              b.onload = function(b, f) {
                return function(a) {
                  if ("json" === f) {
                    if (a = this.response, "string" === typeof a) {
                      try {
                        a = JSON.parse(a), n[b] = a;
                      } catch (e) {
                        m++;
                      }
                    } else {
                      n[b] = a;
                    }
                  } else {
                    n[b] = this.response;
                  }
                  ++k;
                  g && g(k);
                  k === s && c(n);
                };
              }(d, f);
              b.send();
            }
          };
          a.colors = "#0044ff #8c4b00 #cc5c33 #ff80c4 #ffbfd9 #ff8800 #8c5e00 #adcc33 #b380ff #bfd9ff #ffaa00 #8c0038 #bf8f30 #f780ff #cc99c9 #aaff00 #000073 #452699 #cc8166 #cca799 #000066 #992626 #cc6666 #ccc299 #ff6600 #526600 #992663 #cc6681 #99ccc2 #ff0066 #520066 #269973 #61994d #739699 #ffcc00 #006629 #269199 #94994d #738299 #ff0000 #590000 #234d8c #8c6246 #7d7399 #ee00ff #00474d #8c2385 #8c7546 #7c8c69 #eeff00 #4d003d #662e1a #62468c #8c6969 #6600ff #4c2900 #1a6657 #8c464f #8c6981 #44ff00 #401100 #1a2466 #663355 #567365 #d90074 #403300 #101d40 #59562d #66614d #cc0000 #002b40 #234010 #4c2626 #4d5e66 #00a3cc #400011 #231040 #4c3626 #464359 #0000bf #331b00 #80e6ff #311a33 #4d3939 #a69b00 #003329 #80ffb2 #331a20 #40303d #00a658 #40ffd9 #ffc480 #ffe1bf #332b26 #8c2500 #9933cc #80fff6 #ffbfbf #303326 #005e8c #33cc47 #b2ff80 #c8bfff #263332 #00708c #cc33ad #ffe680 #f2ffbf #262a33 #388c00 #335ccc #8091ff #bfffd9".split(" ");
          return a;
        }();
        c.TraceLogger = k;
      })(g.TraceLogger || (g.TraceLogger = {}));
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      (function(c) {
        var k;
        (function(c) {
          c[c.START_HI = 0] = "START_HI";
          c[c.START_LO = 4] = "START_LO";
          c[c.STOP_HI = 8] = "STOP_HI";
          c[c.STOP_LO = 12] = "STOP_LO";
          c[c.TEXTID = 16] = "TEXTID";
          c[c.NEXTID = 20] = "NEXTID";
        })(k || (k = {}));
        k = function() {
          function c(a) {
            2 <= a.length && (this._text = a[0], this._data = new DataView(a[1]), this._buffer = new g.TimelineBuffer, this._walkTree(0));
          }
          Object.defineProperty(c.prototype, "buffer", {get:function() {
            return this._buffer;
          }, enumerable:!0, configurable:!0});
          c.prototype._walkTree = function(a) {
            var h = this._data, g = this._buffer;
            do {
              var l = a * c.ITEM_SIZE, k = 4294967295 * h.getUint32(l + 0) + h.getUint32(l + 4), m = 4294967295 * h.getUint32(l + 8) + h.getUint32(l + 12), s = h.getUint32(l + 16), l = h.getUint32(l + 20), v = 1 === (s & 1), s = s >>> 1, s = this._text[s];
              g.enter(s, null, k / 1E6);
              v && this._walkTree(a + 1);
              g.leave(s, null, m / 1E6);
              a = l;
            } while (0 !== a);
          };
          c.ITEM_SIZE = 24;
          return c;
        }();
        c.Thread = k;
      })(g.TraceLogger || (g.TraceLogger = {}));
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.NumberUtilities.clamp, r = function() {
        function a() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        a.prototype.append = function(a, c) {
          var g = this.lines;
          0 < g.length && g[g.length - 1] === a ? this.repeat[g.length - 1]++ : (this.lines.push(a), this.repeat.push(1), this.format.push(c ? {backgroundFillStyle:c} : void 0), this.time.push(performance.now()), this.length++);
        };
        a.prototype.get = function(a) {
          return this.lines[a];
        };
        a.prototype.getFormat = function(a) {
          return this.format[a];
        };
        a.prototype.getTime = function(a) {
          return this.time[a];
        };
        a.prototype.getRepeat = function(a) {
          return this.repeat[a];
        };
        return a;
      }();
      g.Buffer = r;
      var n = function() {
        function a(a) {
          this.lineColor = "#2A2A2A";
          this.alternateLineColor = "#262626";
          this.textColor = "#FFFFFF";
          this.selectionColor = "#96C9F3";
          this.selectionTextColor = "#000000";
          this.ratio = 1;
          this.showLineNumbers = !0;
          this.showLineCounter = this.showLineTime = !1;
          this.canvas = a;
          this.canvas.focus();
          this.context = a.getContext("2d", {original:!0});
          this.context.fillStyle = "#FFFFFF";
          this.fontSize = 10;
          this.columnIndex = this.pageIndex = this.lineIndex = 0;
          this.selection = null;
          this.lineHeight = 15;
          this.hasFocus = !1;
          window.addEventListener("resize", this._resizeHandler.bind(this), !1);
          this._resizeHandler();
          this.textMarginBottom = this.textMarginLeft = 4;
          this.refreshFrequency = 0;
          this.buffer = new r;
          a.addEventListener("keydown", function(a) {
            var h = 0;
            switch(a.keyCode) {
              case q:
                this.showLineNumbers = !this.showLineNumbers;
                break;
              case w:
                this.showLineTime = !this.showLineTime;
                break;
              case s:
                h = -1;
                break;
              case n:
                h = 1;
                break;
              case c:
                h = -this.pageLineCount;
                break;
              case g:
                h = this.pageLineCount;
                break;
              case k:
                h = -this.lineIndex;
                break;
              case m:
                h = this.buffer.length - this.lineIndex;
                break;
              case d:
                this.columnIndex -= a.metaKey ? 10 : 1;
                0 > this.columnIndex && (this.columnIndex = 0);
                a.preventDefault();
                break;
              case e:
                this.columnIndex += a.metaKey ? 10 : 1;
                a.preventDefault();
                break;
              case b:
                a.metaKey && (this.selection = {start:0, end:this.buffer.length}, a.preventDefault());
                break;
              case f:
                if (a.metaKey) {
                  var V = "";
                  if (this.selection) {
                    for (var Q = this.selection.start;Q <= this.selection.end;Q++) {
                      V += this.buffer.get(Q) + "\n";
                    }
                  } else {
                    V = this.buffer.get(this.lineIndex);
                  }
                  alert(V);
                }
              ;
            }
            a.metaKey && (h *= this.pageLineCount);
            h && (this.scroll(h), a.preventDefault());
            h && a.shiftKey ? this.selection ? this.lineIndex > this.selection.start ? this.selection.end = this.lineIndex : this.selection.start = this.lineIndex : 0 < h ? this.selection = {start:this.lineIndex - h, end:this.lineIndex} : 0 > h && (this.selection = {start:this.lineIndex, end:this.lineIndex - h}) : h && (this.selection = null);
            this.paint();
          }.bind(this), !1);
          a.addEventListener("focus", function(b) {
            this.hasFocus = !0;
          }.bind(this), !1);
          a.addEventListener("blur", function(b) {
            this.hasFocus = !1;
          }.bind(this), !1);
          var c = 33, g = 34, k = 36, m = 35, s = 38, n = 40, d = 37, e = 39, b = 65, f = 67, q = 78, w = 84;
        }
        a.prototype.resize = function() {
          this._resizeHandler();
        };
        a.prototype._resizeHandler = function() {
          var a = this.canvas.parentElement, c = a.clientWidth, a = a.clientHeight - 1, g = window.devicePixelRatio || 1;
          1 !== g ? (this.ratio = g / 1, this.canvas.width = c * this.ratio, this.canvas.height = a * this.ratio, this.canvas.style.width = c + "px", this.canvas.style.height = a + "px") : (this.ratio = 1, this.canvas.width = c, this.canvas.height = a);
          this.pageLineCount = Math.floor(this.canvas.height / this.lineHeight);
        };
        a.prototype.gotoLine = function(a) {
          this.lineIndex = c(a, 0, this.buffer.length - 1);
        };
        a.prototype.scrollIntoView = function() {
          this.lineIndex < this.pageIndex ? this.pageIndex = this.lineIndex : this.lineIndex >= this.pageIndex + this.pageLineCount && (this.pageIndex = this.lineIndex - this.pageLineCount + 1);
        };
        a.prototype.scroll = function(a) {
          this.gotoLine(this.lineIndex + a);
          this.scrollIntoView();
        };
        a.prototype.paint = function() {
          var a = this.pageLineCount;
          this.pageIndex + a > this.buffer.length && (a = this.buffer.length - this.pageIndex);
          var c = this.textMarginLeft, g = c + (this.showLineNumbers ? 5 * (String(this.buffer.length).length + 2) : 0), k = g + (this.showLineTime ? 40 : 10), m = k + 25;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
          for (var s = this.canvas.width, n = this.lineHeight, d = 0;d < a;d++) {
            var e = d * this.lineHeight, b = this.pageIndex + d, f = this.buffer.get(b), q = this.buffer.getFormat(b), w = this.buffer.getRepeat(b), G = 1 < b ? this.buffer.getTime(b) - this.buffer.getTime(0) : 0;
            this.context.fillStyle = b % 2 ? this.lineColor : this.alternateLineColor;
            q && q.backgroundFillStyle && (this.context.fillStyle = q.backgroundFillStyle);
            this.context.fillRect(0, e, s, n);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;
            this.selection && b >= this.selection.start && b <= this.selection.end && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, e, s, n), this.context.fillStyle = this.selectionTextColor);
            this.hasFocus && b === this.lineIndex && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, e, s, n), this.context.fillStyle = this.selectionTextColor);
            0 < this.columnIndex && (f = f.substring(this.columnIndex));
            e = (d + 1) * this.lineHeight - this.textMarginBottom;
            this.showLineNumbers && this.context.fillText(String(b), c, e);
            this.showLineTime && this.context.fillText(G.toFixed(1).padLeft(" ", 6), g, e);
            1 < w && this.context.fillText(String(w).padLeft(" ", 3), k, e);
            this.context.fillText(f, m, e);
          }
        };
        a.prototype.refreshEvery = function(a) {
          function c() {
            g.paint();
            g.refreshFrequency && setTimeout(c, g.refreshFrequency);
          }
          var g = this;
          this.refreshFrequency = a;
          g.refreshFrequency && setTimeout(c, g.refreshFrequency);
        };
        a.prototype.isScrolledToBottom = function() {
          return this.lineIndex === this.buffer.length - 1;
        };
        return a;
      }();
      g.Terminal = n;
    })(r.Terminal || (r.Terminal = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var c = function() {
        function c(g) {
          this._lastWeightedTime = this._lastTime = this._index = 0;
          this._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
          this._container = g;
          this._canvas = document.createElement("canvas");
          this._container.appendChild(this._canvas);
          this._context = this._canvas.getContext("2d");
          this._listenForContainerSizeChanges();
        }
        c.prototype._listenForContainerSizeChanges = function() {
          var c = this._containerWidth, a = this._containerHeight;
          this._onContainerSizeChanged();
          var h = this;
          setInterval(function() {
            if (c !== h._containerWidth || a !== h._containerHeight) {
              h._onContainerSizeChanged(), c = h._containerWidth, a = h._containerHeight;
            }
          }, 10);
        };
        c.prototype._onContainerSizeChanged = function() {
          var c = this._containerWidth, a = this._containerHeight, h = window.devicePixelRatio || 1;
          1 !== h ? (this._ratio = h / 1, this._canvas.width = c * this._ratio, this._canvas.height = a * this._ratio, this._canvas.style.width = c + "px", this._canvas.style.height = a + "px") : (this._ratio = 1, this._canvas.width = c, this._canvas.height = a);
        };
        Object.defineProperty(c.prototype, "_containerWidth", {get:function() {
          return this._container.clientWidth;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "_containerHeight", {get:function() {
          return this._container.clientHeight;
        }, enumerable:!0, configurable:!0});
        c.prototype.tickAndRender = function(c, a) {
          void 0 === c && (c = !1);
          void 0 === a && (a = 0);
          if (0 === this._lastTime) {
            this._lastTime = performance.now();
          } else {
            var h = 1 * (performance.now() - this._lastTime) + 0 * this._lastWeightedTime, g = this._context, l = 2 * this._ratio, k = 30 * this._ratio, m = performance;
            m.memory && (k += 30 * this._ratio);
            var s = (this._canvas.width - k) / (l + 1) | 0, v = this._index++;
            this._index > s && (this._index = 0);
            s = this._canvas.height;
            g.globalAlpha = 1;
            g.fillStyle = "black";
            g.fillRect(k + v * (l + 1), 0, 4 * l, this._canvas.height);
            var d = Math.min(1E3 / 60 / h, 1);
            g.fillStyle = "#00FF00";
            g.globalAlpha = c ? .5 : 1;
            d = s / 2 * d | 0;
            g.fillRect(k + v * (l + 1), s - d, l, d);
            a && (d = Math.min(1E3 / 240 / a, 1), g.fillStyle = "#FF6347", d = s / 2 * d | 0, g.fillRect(k + v * (l + 1), s / 2 - d, l, d));
            0 === v % 16 && (g.globalAlpha = 1, g.fillStyle = "black", g.fillRect(0, 0, k, this._canvas.height), g.fillStyle = "white", g.font = 8 * this._ratio + "px Arial", g.textBaseline = "middle", l = (1E3 / h).toFixed(0), a && (l += " " + a.toFixed(0)), m.memory && (l += " " + (m.memory.usedJSHeapSize / 1024 / 1024).toFixed(2)), g.fillText(l, 2 * this._ratio, this._containerHeight / 2 * this._ratio));
            this._lastTime = performance.now();
            this._lastWeightedTime = h;
          }
        };
        return c;
      }();
      g.FPS = c;
    })(k.Mini || (k.Mini = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load Shared Dependencies");
console.time("Load GFX Dependencies");
(function(k) {
  (function(r) {
    function g(b, f, a) {
      return p && a ? "string" === typeof f ? (b = k.ColorUtilities.cssStyleToRGBA(f), k.ColorUtilities.rgbaToCSSStyle(a.transformRGBA(b))) : f instanceof CanvasGradient && f._template ? f._template.createCanvasGradient(b, a) : f : f;
    }
    var c = k.NumberUtilities.clamp;
    (function(b) {
      b[b.None = 0] = "None";
      b[b.Brief = 1] = "Brief";
      b[b.Verbose = 2] = "Verbose";
    })(r.TraceLevel || (r.TraceLevel = {}));
    var t = k.Metrics.Counter.instance;
    r.frameCounter = new k.Metrics.Counter(!0);
    r.traceLevel = 2;
    r.writer = null;
    r.frameCount = function(b) {
      t.count(b);
      r.frameCounter.count(b);
    };
    r.timelineBuffer = new k.Tools.Profiler.TimelineBuffer("GFX");
    r.enterTimeline = function(b, f) {
    };
    r.leaveTimeline = function(b, f) {
    };
    var n = null, a = null, h = null, p = !0;
    p && "undefined" !== typeof CanvasRenderingContext2D && (n = CanvasGradient.prototype.addColorStop, a = CanvasRenderingContext2D.prototype.createLinearGradient, h = CanvasRenderingContext2D.prototype.createRadialGradient, CanvasRenderingContext2D.prototype.createLinearGradient = function(b, f, a, e) {
      return(new u(b, f, a, e)).createCanvasGradient(this, null);
    }, CanvasRenderingContext2D.prototype.createRadialGradient = function(b, f, a, e, d, c) {
      return(new m(b, f, a, e, d, c)).createCanvasGradient(this, null);
    }, CanvasGradient.prototype.addColorStop = function(b, f) {
      n.call(this, b, f);
      this._template.addColorStop(b, f);
    });
    var l = function() {
      return function(b, f) {
        this.offset = b;
        this.color = f;
      };
    }(), u = function() {
      function b(f, a, e, d) {
        this.x0 = f;
        this.y0 = a;
        this.x1 = e;
        this.y1 = d;
        this.colorStops = [];
      }
      b.prototype.addColorStop = function(b, f) {
        this.colorStops.push(new l(b, f));
      };
      b.prototype.createCanvasGradient = function(b, f) {
        for (var e = a.call(b, this.x0, this.y0, this.x1, this.y1), d = this.colorStops, c = 0;c < d.length;c++) {
          var q = d[c], w = q.offset, q = q.color, q = f ? g(b, q, f) : q;
          n.call(e, w, q);
        }
        e._template = this;
        e._transform = this._transform;
        return e;
      };
      return b;
    }(), m = function() {
      function b(f, a, e, d, c, q) {
        this.x0 = f;
        this.y0 = a;
        this.r0 = e;
        this.x1 = d;
        this.y1 = c;
        this.r1 = q;
        this.colorStops = [];
      }
      b.prototype.addColorStop = function(b, f) {
        this.colorStops.push(new l(b, f));
      };
      b.prototype.createCanvasGradient = function(b, f) {
        for (var a = h.call(b, this.x0, this.y0, this.r0, this.x1, this.y1, this.r1), e = this.colorStops, d = 0;d < e.length;d++) {
          var c = e[d], q = c.offset, c = c.color, c = f ? g(b, c, f) : c;
          n.call(a, q, c);
        }
        a._template = this;
        a._transform = this._transform;
        return a;
      };
      return b;
    }(), s;
    (function(b) {
      b[b.ClosePath = 1] = "ClosePath";
      b[b.MoveTo = 2] = "MoveTo";
      b[b.LineTo = 3] = "LineTo";
      b[b.QuadraticCurveTo = 4] = "QuadraticCurveTo";
      b[b.BezierCurveTo = 5] = "BezierCurveTo";
      b[b.ArcTo = 6] = "ArcTo";
      b[b.Rect = 7] = "Rect";
      b[b.Arc = 8] = "Arc";
      b[b.Save = 9] = "Save";
      b[b.Restore = 10] = "Restore";
      b[b.Transform = 11] = "Transform";
    })(s || (s = {}));
    var v = function() {
      function b(f) {
        this._commands = new Uint8Array(b._arrayBufferPool.acquire(8), 0, 8);
        this._commandPosition = 0;
        this._data = new Float64Array(b._arrayBufferPool.acquire(8 * Float64Array.BYTES_PER_ELEMENT), 0, 8);
        this._dataPosition = 0;
        f instanceof b && this.addPath(f);
      }
      b._apply = function(b, f) {
        var a = b._commands, e = b._data, d = 0, c = 0;
        f.beginPath();
        for (var q = b._commandPosition;d < q;) {
          switch(a[d++]) {
            case 1:
              f.closePath();
              break;
            case 2:
              f.moveTo(e[c++], e[c++]);
              break;
            case 3:
              f.lineTo(e[c++], e[c++]);
              break;
            case 4:
              f.quadraticCurveTo(e[c++], e[c++], e[c++], e[c++]);
              break;
            case 5:
              f.bezierCurveTo(e[c++], e[c++], e[c++], e[c++], e[c++], e[c++]);
              break;
            case 6:
              f.arcTo(e[c++], e[c++], e[c++], e[c++], e[c++]);
              break;
            case 7:
              f.rect(e[c++], e[c++], e[c++], e[c++]);
              break;
            case 8:
              f.arc(e[c++], e[c++], e[c++], e[c++], e[c++], !!e[c++]);
              break;
            case 9:
              f.save();
              break;
            case 10:
              f.restore();
              break;
            case 11:
              f.transform(e[c++], e[c++], e[c++], e[c++], e[c++], e[c++]);
          }
        }
      };
      b.prototype._ensureCommandCapacity = function(f) {
        this._commands = b._arrayBufferPool.ensureUint8ArrayLength(this._commands, f);
      };
      b.prototype._ensureDataCapacity = function(f) {
        this._data = b._arrayBufferPool.ensureFloat64ArrayLength(this._data, f);
      };
      b.prototype._writeCommand = function(b) {
        this._commandPosition >= this._commands.length && this._ensureCommandCapacity(this._commandPosition + 1);
        this._commands[this._commandPosition++] = b;
      };
      b.prototype._writeData = function(b, f, a, e, d, c) {
        var q = arguments.length;
        this._dataPosition + q >= this._data.length && this._ensureDataCapacity(this._dataPosition + q);
        var w = this._data, h = this._dataPosition;
        w[h] = b;
        w[h + 1] = f;
        2 < q && (w[h + 2] = a, w[h + 3] = e, 4 < q && (w[h + 4] = d, 6 === q && (w[h + 5] = c)));
        this._dataPosition += q;
      };
      b.prototype.closePath = function() {
        this._writeCommand(1);
      };
      b.prototype.moveTo = function(b, f) {
        this._writeCommand(2);
        this._writeData(b, f);
      };
      b.prototype.lineTo = function(b, f) {
        this._writeCommand(3);
        this._writeData(b, f);
      };
      b.prototype.quadraticCurveTo = function(b, f, a, e) {
        this._writeCommand(4);
        this._writeData(b, f, a, e);
      };
      b.prototype.bezierCurveTo = function(b, f, a, e, d, c) {
        this._writeCommand(5);
        this._writeData(b, f, a, e, d, c);
      };
      b.prototype.arcTo = function(b, f, a, e, d) {
        this._writeCommand(6);
        this._writeData(b, f, a, e, d);
      };
      b.prototype.rect = function(b, f, a, e) {
        this._writeCommand(7);
        this._writeData(b, f, a, e);
      };
      b.prototype.arc = function(b, f, a, e, d, c) {
        this._writeCommand(8);
        this._writeData(b, f, a, e, d, +c);
      };
      b.prototype.addPath = function(b, f) {
        f && (this._writeCommand(9), this._writeCommand(11), this._writeData(f.a, f.b, f.c, f.d, f.e, f.f));
        var a = this._commandPosition + b._commandPosition;
        a >= this._commands.length && this._ensureCommandCapacity(a);
        for (var e = this._commands, d = b._commands, c = this._commandPosition, q = 0;c < a;c++) {
          e[c] = d[q++];
        }
        this._commandPosition = a;
        a = this._dataPosition + b._dataPosition;
        a >= this._data.length && this._ensureDataCapacity(a);
        e = this._data;
        d = b._data;
        c = this._dataPosition;
        for (q = 0;c < a;c++) {
          e[c] = d[q++];
        }
        this._dataPosition = a;
        f && this._writeCommand(10);
      };
      b._arrayBufferPool = new k.ArrayBufferPool;
      return b;
    }();
    r.Path = v;
    if ("undefined" !== typeof CanvasRenderingContext2D && ("undefined" === typeof Path2D || !Path2D.prototype.addPath)) {
      var d = CanvasRenderingContext2D.prototype.fill;
      CanvasRenderingContext2D.prototype.fill = function(b, f) {
        arguments.length && (b instanceof v ? v._apply(b, this) : f = b);
        f ? d.call(this, f) : d.call(this);
      };
      var e = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.stroke = function(b, f) {
        arguments.length && (b instanceof v ? v._apply(b, this) : f = b);
        f ? e.call(this, f) : e.call(this);
      };
      var b = CanvasRenderingContext2D.prototype.clip;
      CanvasRenderingContext2D.prototype.clip = function(f, a) {
        arguments.length && (f instanceof v ? v._apply(f, this) : a = f);
        a ? b.call(this, a) : b.call(this);
      };
      window.Path2D = v;
    }
    if ("undefined" !== typeof CanvasPattern && Path2D.prototype.addPath) {
      s = function(b) {
        this._transform = b;
        this._template && (this._template._transform = b);
      };
      CanvasPattern.prototype.setTransform || (CanvasPattern.prototype.setTransform = s);
      CanvasGradient.prototype.setTransform || (CanvasGradient.prototype.setTransform = s);
      var f = CanvasRenderingContext2D.prototype.fill, q = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.fill = function(b, a) {
        var e = !!this.fillStyle._transform;
        if ((this.fillStyle instanceof CanvasPattern || this.fillStyle instanceof CanvasGradient) && e && b instanceof Path2D) {
          var e = this.fillStyle._transform, d;
          try {
            d = e.inverse();
          } catch (c) {
            d = e = r.Geometry.Matrix.createIdentitySVGMatrix();
          }
          this.transform(e.a, e.b, e.c, e.d, e.e, e.f);
          e = new Path2D;
          e.addPath(b, d);
          f.call(this, e, a);
          this.transform(d.a, d.b, d.c, d.d, d.e, d.f);
        } else {
          0 === arguments.length ? f.call(this) : 1 === arguments.length ? f.call(this, b) : 2 === arguments.length && f.call(this, b, a);
        }
      };
      CanvasRenderingContext2D.prototype.stroke = function(b) {
        var f = !!this.strokeStyle._transform;
        if ((this.strokeStyle instanceof CanvasPattern || this.strokeStyle instanceof CanvasGradient) && f && b instanceof Path2D) {
          var a = this.strokeStyle._transform, f = a.inverse();
          this.transform(a.a, a.b, a.c, a.d, a.e, a.f);
          a = new Path2D;
          a.addPath(b, f);
          var e = this.lineWidth;
          this.lineWidth *= (f.a + f.d) / 2;
          q.call(this, a);
          this.transform(f.a, f.b, f.c, f.d, f.e, f.f);
          this.lineWidth = e;
        } else {
          0 === arguments.length ? q.call(this) : 1 === arguments.length && q.call(this, b);
        }
      };
    }
    "undefined" !== typeof CanvasRenderingContext2D && function() {
      function b() {
        return r.Geometry.Matrix.createSVGMatrixFromArray(this.mozCurrentTransform);
      }
      CanvasRenderingContext2D.prototype.flashStroke = function(b, f) {
        var a = this.currentTransform;
        if (a) {
          var e = new Path2D;
          e.addPath(b, a);
          var d = this.lineWidth;
          this.setTransform(1, 0, 0, 1, 0, 0);
          switch(f) {
            case 1:
              this.lineWidth = c(d * (k.getScaleX(a) + k.getScaleY(a)) / 2, 1, 1024);
              break;
            case 2:
              this.lineWidth = c(d * k.getScaleY(a), 1, 1024);
              break;
            case 3:
              this.lineWidth = c(d * k.getScaleX(a), 1, 1024);
          }
          this.stroke(e);
          this.setTransform(a.a, a.b, a.c, a.d, a.e, a.f);
          this.lineWidth = d;
        } else {
          this.stroke(b);
        }
      };
      !("currentTransform" in CanvasRenderingContext2D.prototype) && "mozCurrentTransform" in CanvasRenderingContext2D.prototype && Object.defineProperty(CanvasRenderingContext2D.prototype, "currentTransform", {get:b});
    }();
    if ("undefined" !== typeof CanvasRenderingContext2D && void 0 === CanvasRenderingContext2D.prototype.globalColorMatrix) {
      var w = CanvasRenderingContext2D.prototype.fill, G = CanvasRenderingContext2D.prototype.stroke, U = CanvasRenderingContext2D.prototype.fillText, V = CanvasRenderingContext2D.prototype.strokeText;
      Object.defineProperty(CanvasRenderingContext2D.prototype, "globalColorMatrix", {get:function() {
        return this._globalColorMatrix ? this._globalColorMatrix.clone() : null;
      }, set:function(b) {
        b ? this._globalColorMatrix ? this._globalColorMatrix.set(b) : this._globalColorMatrix = b.clone() : this._globalColorMatrix = null;
      }, enumerable:!0, configurable:!0});
      CanvasRenderingContext2D.prototype.fill = function(b, f) {
        var a = null;
        this._globalColorMatrix && (a = this.fillStyle, this.fillStyle = g(this, this.fillStyle, this._globalColorMatrix));
        0 === arguments.length ? w.call(this) : 1 === arguments.length ? w.call(this, b) : 2 === arguments.length && w.call(this, b, f);
        a && (this.fillStyle = a);
      };
      CanvasRenderingContext2D.prototype.stroke = function(b, f) {
        var a = null;
        this._globalColorMatrix && (a = this.strokeStyle, this.strokeStyle = g(this, this.strokeStyle, this._globalColorMatrix));
        0 === arguments.length ? G.call(this) : 1 === arguments.length && G.call(this, b);
        a && (this.strokeStyle = a);
      };
      CanvasRenderingContext2D.prototype.fillText = function(b, f, a, e) {
        var d = null;
        this._globalColorMatrix && (d = this.fillStyle, this.fillStyle = g(this, this.fillStyle, this._globalColorMatrix));
        3 === arguments.length ? U.call(this, b, f, a) : 4 === arguments.length ? U.call(this, b, f, a, e) : k.Debug.unexpected();
        d && (this.fillStyle = d);
      };
      CanvasRenderingContext2D.prototype.strokeText = function(b, f, a, e) {
        var d = null;
        this._globalColorMatrix && (d = this.strokeStyle, this.strokeStyle = g(this, this.strokeStyle, this._globalColorMatrix));
        3 === arguments.length ? V.call(this, b, f, a) : 4 === arguments.length ? V.call(this, b, f, a, e) : k.Debug.unexpected();
        d && (this.strokeStyle = d);
      };
    }
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    k.ScreenShot = function() {
      return function(g, c, k) {
        this.dataURL = g;
        this.w = c;
        this.h = k;
      };
    }();
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var r = function() {
    function g() {
      this._count = 0;
      this._head = this._tail = null;
    }
    Object.defineProperty(g.prototype, "count", {get:function() {
      return this._count;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(g.prototype, "head", {get:function() {
      return this._head;
    }, enumerable:!0, configurable:!0});
    g.prototype._unshift = function(c) {
      0 === this._count ? this._head = this._tail = c : (c.next = this._head, this._head = c.next.previous = c);
      this._count++;
    };
    g.prototype._remove = function(c) {
      c === this._head && c === this._tail ? this._head = this._tail = null : c === this._head ? (this._head = c.next, this._head.previous = null) : c == this._tail ? (this._tail = c.previous, this._tail.next = null) : (c.previous.next = c.next, c.next.previous = c.previous);
      c.previous = c.next = null;
      this._count--;
    };
    g.prototype.use = function(c) {
      this._head !== c && ((c.next || c.previous || this._tail === c) && this._remove(c), this._unshift(c));
    };
    g.prototype.pop = function() {
      if (!this._tail) {
        return null;
      }
      var c = this._tail;
      this._remove(c);
      return c;
    };
    g.prototype.visit = function(c, g) {
      void 0 === g && (g = !0);
      for (var k = g ? this._head : this._tail;k && c(k);) {
        k = g ? k.next : k.previous;
      }
    };
    return g;
  }();
  k.LRUList = r;
  k.getScaleX = function(g) {
    return g.a;
  };
  k.getScaleY = function(g) {
    return g.d;
  };
})(Shumway || (Shumway = {}));
var Shumway$$inline_28 = Shumway || (Shumway = {}), GFX$$inline_29 = Shumway$$inline_28.GFX || (Shumway$$inline_28.GFX = {}), Option$$inline_30 = Shumway$$inline_28.Options.Option, OptionSet$$inline_31 = Shumway$$inline_28.Options.OptionSet, shumwayOptions$$inline_32 = Shumway$$inline_28.Settings.shumwayOptions, rendererOptions$$inline_33 = shumwayOptions$$inline_32.register(new OptionSet$$inline_31("Renderer Options"));
GFX$$inline_29.imageUpdateOption = rendererOptions$$inline_33.register(new Option$$inline_30("", "imageUpdate", "boolean", !0, "Enable image updating."));
GFX$$inline_29.imageConvertOption = rendererOptions$$inline_33.register(new Option$$inline_30("", "imageConvert", "boolean", !0, "Enable image conversion."));
GFX$$inline_29.stageOptions = shumwayOptions$$inline_32.register(new OptionSet$$inline_31("Stage Renderer Options"));
GFX$$inline_29.forcePaint = GFX$$inline_29.stageOptions.register(new Option$$inline_30("", "forcePaint", "boolean", !1, "Force repainting."));
GFX$$inline_29.ignoreViewport = GFX$$inline_29.stageOptions.register(new Option$$inline_30("", "ignoreViewport", "boolean", !1, "Cull elements outside of the viewport."));
GFX$$inline_29.viewportLoupeDiameter = GFX$$inline_29.stageOptions.register(new Option$$inline_30("", "viewportLoupeDiameter", "number", 256, "Size of the viewport loupe.", {range:{min:1, max:1024, step:1}}));
GFX$$inline_29.disableClipping = GFX$$inline_29.stageOptions.register(new Option$$inline_30("", "disableClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_29.debugClipping = GFX$$inline_29.stageOptions.register(new Option$$inline_30("", "debugClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_29.hud = GFX$$inline_29.stageOptions.register(new Option$$inline_30("", "hud", "boolean", !1, "Enable HUD."));
var webGLOptions$$inline_34 = GFX$$inline_29.stageOptions.register(new OptionSet$$inline_31("WebGL Options"));
GFX$$inline_29.perspectiveCamera = webGLOptions$$inline_34.register(new Option$$inline_30("", "pc", "boolean", !1, "Use perspective camera."));
GFX$$inline_29.perspectiveCameraFOV = webGLOptions$$inline_34.register(new Option$$inline_30("", "pcFOV", "number", 60, "Perspective Camera FOV."));
GFX$$inline_29.perspectiveCameraDistance = webGLOptions$$inline_34.register(new Option$$inline_30("", "pcDistance", "number", 2, "Perspective Camera Distance."));
GFX$$inline_29.perspectiveCameraAngle = webGLOptions$$inline_34.register(new Option$$inline_30("", "pcAngle", "number", 0, "Perspective Camera Angle."));
GFX$$inline_29.perspectiveCameraAngleRotate = webGLOptions$$inline_34.register(new Option$$inline_30("", "pcRotate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_29.perspectiveCameraSpacing = webGLOptions$$inline_34.register(new Option$$inline_30("", "pcSpacing", "number", .01, "Element Spacing."));
GFX$$inline_29.perspectiveCameraSpacingInflate = webGLOptions$$inline_34.register(new Option$$inline_30("", "pcInflate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_29.drawTiles = webGLOptions$$inline_34.register(new Option$$inline_30("", "drawTiles", "boolean", !1, "Draw WebGL Tiles"));
GFX$$inline_29.drawSurfaces = webGLOptions$$inline_34.register(new Option$$inline_30("", "drawSurfaces", "boolean", !1, "Draw WebGL Surfaces."));
GFX$$inline_29.drawSurface = webGLOptions$$inline_34.register(new Option$$inline_30("", "drawSurface", "number", -1, "Draw WebGL Surface #"));
GFX$$inline_29.drawElements = webGLOptions$$inline_34.register(new Option$$inline_30("", "drawElements", "boolean", !0, "Actually call gl.drawElements. This is useful to test if the GPU is the bottleneck."));
GFX$$inline_29.disableSurfaceUploads = webGLOptions$$inline_34.register(new Option$$inline_30("", "disableSurfaceUploads", "boolean", !1, "Disable surface uploads."));
GFX$$inline_29.premultipliedAlpha = webGLOptions$$inline_34.register(new Option$$inline_30("", "premultipliedAlpha", "boolean", !1, "Set the premultipliedAlpha flag on getContext()."));
GFX$$inline_29.unpackPremultiplyAlpha = webGLOptions$$inline_34.register(new Option$$inline_30("", "unpackPremultiplyAlpha", "boolean", !0, "Use UNPACK_PREMULTIPLY_ALPHA_WEBGL in pixelStorei."));
var factorChoices$$inline_35 = {ZERO:0, ONE:1, SRC_COLOR:768, ONE_MINUS_SRC_COLOR:769, DST_COLOR:774, ONE_MINUS_DST_COLOR:775, SRC_ALPHA:770, ONE_MINUS_SRC_ALPHA:771, DST_ALPHA:772, ONE_MINUS_DST_ALPHA:773, SRC_ALPHA_SATURATE:776, CONSTANT_COLOR:32769, ONE_MINUS_CONSTANT_COLOR:32770, CONSTANT_ALPHA:32771, ONE_MINUS_CONSTANT_ALPHA:32772};
GFX$$inline_29.sourceBlendFactor = webGLOptions$$inline_34.register(new Option$$inline_30("", "sourceBlendFactor", "number", factorChoices$$inline_35.ONE, "", {choices:factorChoices$$inline_35}));
GFX$$inline_29.destinationBlendFactor = webGLOptions$$inline_34.register(new Option$$inline_30("", "destinationBlendFactor", "number", factorChoices$$inline_35.ONE_MINUS_SRC_ALPHA, "", {choices:factorChoices$$inline_35}));
var canvas2DOptions$$inline_36 = GFX$$inline_29.stageOptions.register(new OptionSet$$inline_31("Canvas2D Options"));
GFX$$inline_29.clipDirtyRegions = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "clipDirtyRegions", "boolean", !1, "Clip dirty regions."));
GFX$$inline_29.clipCanvas = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "clipCanvas", "boolean", !1, "Clip Regions."));
GFX$$inline_29.cull = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "cull", "boolean", !1, "Enable culling."));
GFX$$inline_29.snapToDevicePixels = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "snapToDevicePixels", "boolean", !1, ""));
GFX$$inline_29.imageSmoothing = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "imageSmoothing", "boolean", !1, ""));
GFX$$inline_29.masking = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "masking", "boolean", !0, "Composite Mask."));
GFX$$inline_29.blending = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "blending", "boolean", !0, ""));
GFX$$inline_29.debugLayers = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "debugLayers", "boolean", !1, ""));
GFX$$inline_29.filters = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "filters", "boolean", !1, ""));
GFX$$inline_29.cacheShapes = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "cacheShapes", "boolean", !0, ""));
GFX$$inline_29.cacheShapesMaxSize = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "cacheShapesMaxSize", "number", 256, "", {range:{min:1, max:1024, step:1}}));
GFX$$inline_29.cacheShapesThreshold = canvas2DOptions$$inline_36.register(new Option$$inline_30("", "cacheShapesThreshold", "number", 256, "", {range:{min:1, max:1024, step:1}}));
(function(k) {
  (function(r) {
    (function(g) {
      function c(a, b, f, d) {
        var c = 1 - d;
        return a * c * c + 2 * b * c * d + f * d * d;
      }
      function t(a, b, f, d, c) {
        var h = c * c, m = 1 - c, g = m * m;
        return a * m * g + 3 * b * c * g + 3 * f * m * h + d * c * h;
      }
      var n = k.NumberUtilities.clamp, a = k.NumberUtilities.pow2, h = k.NumberUtilities.epsilonEquals;
      g.radianToDegrees = function(a) {
        return 180 * a / Math.PI;
      };
      g.degreesToRadian = function(a) {
        return a * Math.PI / 180;
      };
      g.quadraticBezier = c;
      g.quadraticBezierExtreme = function(a, b, f) {
        var d = (a - b) / (a - 2 * b + f);
        return 0 > d ? a : 1 < d ? f : c(a, b, f, d);
      };
      g.cubicBezier = t;
      g.cubicBezierExtremes = function(a, b, f, d) {
        var c = b - a, h;
        h = 2 * (f - b);
        var m = d - f;
        c + m === h && (m *= 1.0001);
        var g = 2 * c - h, l = h - 2 * c, l = Math.sqrt(l * l - 4 * c * (c - h + m));
        h = 2 * (c - h + m);
        c = (g + l) / h;
        g = (g - l) / h;
        l = [];
        0 <= c && 1 >= c && l.push(t(a, b, f, d, c));
        0 <= g && 1 >= g && l.push(t(a, b, f, d, g));
        return l;
      };
      var p = function() {
        function a(b, f) {
          this.x = b;
          this.y = f;
        }
        a.prototype.setElements = function(b, f) {
          this.x = b;
          this.y = f;
          return this;
        };
        a.prototype.set = function(b) {
          this.x = b.x;
          this.y = b.y;
          return this;
        };
        a.prototype.dot = function(b) {
          return this.x * b.x + this.y * b.y;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.distanceTo = function(b) {
          return Math.sqrt(this.dot(b));
        };
        a.prototype.sub = function(b) {
          this.x -= b.x;
          this.y -= b.y;
          return this;
        };
        a.prototype.mul = function(b) {
          this.x *= b;
          this.y *= b;
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y);
        };
        a.prototype.toString = function(b) {
          void 0 === b && (b = 2);
          return "{x: " + this.x.toFixed(b) + ", y: " + this.y.toFixed(b) + "}";
        };
        a.prototype.inTriangle = function(b, f, a) {
          var e = b.y * a.x - b.x * a.y + (a.y - b.y) * this.x + (b.x - a.x) * this.y, d = b.x * f.y - b.y * f.x + (b.y - f.y) * this.x + (f.x - b.x) * this.y;
          if (0 > e != 0 > d) {
            return!1;
          }
          b = -f.y * a.x + b.y * (a.x - f.x) + b.x * (f.y - a.y) + f.x * a.y;
          0 > b && (e = -e, d = -d, b = -b);
          return 0 < e && 0 < d && e + d < b;
        };
        a.createEmpty = function() {
          return new a(0, 0);
        };
        a.createEmptyPoints = function(b) {
          for (var f = [], d = 0;d < b;d++) {
            f.push(new a(0, 0));
          }
          return f;
        };
        return a;
      }();
      g.Point = p;
      var l = function() {
        function a(b, f, e) {
          this.x = b;
          this.y = f;
          this.z = e;
        }
        a.prototype.setElements = function(b, f, a) {
          this.x = b;
          this.y = f;
          this.z = a;
          return this;
        };
        a.prototype.set = function(b) {
          this.x = b.x;
          this.y = b.y;
          this.z = b.z;
          return this;
        };
        a.prototype.dot = function(b) {
          return this.x * b.x + this.y * b.y + this.z * b.z;
        };
        a.prototype.cross = function(b) {
          var f = this.z * b.x - this.x * b.z, a = this.x * b.y - this.y * b.x;
          this.x = this.y * b.z - this.z * b.y;
          this.y = f;
          this.z = a;
          return this;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.sub = function(b) {
          this.x -= b.x;
          this.y -= b.y;
          this.z -= b.z;
          return this;
        };
        a.prototype.mul = function(b) {
          this.x *= b;
          this.y *= b;
          this.z *= b;
          return this;
        };
        a.prototype.normalize = function() {
          var b = Math.sqrt(this.squaredLength());
          1E-5 < b ? this.mul(1 / b) : this.setElements(0, 0, 0);
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y, this.z);
        };
        a.prototype.toString = function(b) {
          void 0 === b && (b = 2);
          return "{x: " + this.x.toFixed(b) + ", y: " + this.y.toFixed(b) + ", z: " + this.z.toFixed(b) + "}";
        };
        a.createEmpty = function() {
          return new a(0, 0, 0);
        };
        a.createEmptyPoints = function(b) {
          for (var f = [], d = 0;d < b;d++) {
            f.push(new a(0, 0, 0));
          }
          return f;
        };
        return a;
      }();
      g.Point3D = l;
      var u = function() {
        function a(b, f, d, c) {
          this.setElements(b, f, d, c);
          a.allocationCount++;
        }
        a.prototype.setElements = function(b, f, a, e) {
          this.x = b;
          this.y = f;
          this.w = a;
          this.h = e;
        };
        a.prototype.set = function(b) {
          this.x = b.x;
          this.y = b.y;
          this.w = b.w;
          this.h = b.h;
        };
        a.prototype.contains = function(b) {
          var f = b.x + b.w, a = b.y + b.h, e = this.x + this.w, d = this.y + this.h;
          return b.x >= this.x && b.x < e && b.y >= this.y && b.y < d && f > this.x && f <= e && a > this.y && a <= d;
        };
        a.prototype.containsPoint = function(b) {
          return b.x >= this.x && b.x < this.x + this.w && b.y >= this.y && b.y < this.y + this.h;
        };
        a.prototype.isContained = function(b) {
          for (var f = 0;f < b.length;f++) {
            if (b[f].contains(this)) {
              return!0;
            }
          }
          return!1;
        };
        a.prototype.isSmallerThan = function(b) {
          return this.w < b.w && this.h < b.h;
        };
        a.prototype.isLargerThan = function(b) {
          return this.w > b.w && this.h > b.h;
        };
        a.prototype.union = function(b) {
          if (this.isEmpty()) {
            this.set(b);
          } else {
            if (!b.isEmpty()) {
              var f = this.x, a = this.y;
              this.x > b.x && (f = b.x);
              this.y > b.y && (a = b.y);
              var e = this.x + this.w;
              e < b.x + b.w && (e = b.x + b.w);
              var d = this.y + this.h;
              d < b.y + b.h && (d = b.y + b.h);
              this.x = f;
              this.y = a;
              this.w = e - f;
              this.h = d - a;
            }
          }
        };
        a.prototype.isEmpty = function() {
          return 0 >= this.w || 0 >= this.h;
        };
        a.prototype.setEmpty = function() {
          this.h = this.w = this.y = this.x = 0;
        };
        a.prototype.intersect = function(b) {
          var f = a.createEmpty();
          if (this.isEmpty() || b.isEmpty()) {
            return f.setEmpty(), f;
          }
          f.x = Math.max(this.x, b.x);
          f.y = Math.max(this.y, b.y);
          f.w = Math.min(this.x + this.w, b.x + b.w) - f.x;
          f.h = Math.min(this.y + this.h, b.y + b.h) - f.y;
          f.isEmpty() && f.setEmpty();
          this.set(f);
        };
        a.prototype.intersects = function(b) {
          if (this.isEmpty() || b.isEmpty()) {
            return!1;
          }
          var f = Math.max(this.x, b.x), a = Math.max(this.y, b.y), f = Math.min(this.x + this.w, b.x + b.w) - f;
          b = Math.min(this.y + this.h, b.y + b.h) - a;
          return!(0 >= f || 0 >= b);
        };
        a.prototype.intersectsTransformedAABB = function(b, f) {
          var d = a._temporary;
          d.set(b);
          f.transformRectangleAABB(d);
          return this.intersects(d);
        };
        a.prototype.intersectsTranslated = function(b, f, a) {
          if (this.isEmpty() || b.isEmpty()) {
            return!1;
          }
          var e = Math.max(this.x, b.x + f), d = Math.max(this.y, b.y + a);
          f = Math.min(this.x + this.w, b.x + f + b.w) - e;
          b = Math.min(this.y + this.h, b.y + a + b.h) - d;
          return!(0 >= f || 0 >= b);
        };
        a.prototype.area = function() {
          return this.w * this.h;
        };
        a.prototype.clone = function() {
          var b = a.allocate();
          b.set(this);
          return b;
        };
        a.allocate = function() {
          var b = a._dirtyStack;
          return b.length ? b.pop() : new a(12345, 67890, 12345, 67890);
        };
        a.prototype.free = function() {
          a._dirtyStack.push(this);
        };
        a.prototype.snap = function() {
          var b = Math.ceil(this.x + this.w), f = Math.ceil(this.y + this.h);
          this.x = Math.floor(this.x);
          this.y = Math.floor(this.y);
          this.w = b - this.x;
          this.h = f - this.y;
          return this;
        };
        a.prototype.scale = function(b, f) {
          this.x *= b;
          this.y *= f;
          this.w *= b;
          this.h *= f;
          return this;
        };
        a.prototype.offset = function(b, f) {
          this.x += b;
          this.y += f;
          return this;
        };
        a.prototype.resize = function(b, f) {
          this.w += b;
          this.h += f;
          return this;
        };
        a.prototype.expand = function(b, f) {
          this.offset(-b, -f).resize(2 * b, 2 * f);
          return this;
        };
        a.prototype.getCenter = function() {
          return new p(this.x + this.w / 2, this.y + this.h / 2);
        };
        a.prototype.getAbsoluteBounds = function() {
          return new a(0, 0, this.w, this.h);
        };
        a.prototype.toString = function(b) {
          void 0 === b && (b = 2);
          return "{" + this.x.toFixed(b) + ", " + this.y.toFixed(b) + ", " + this.w.toFixed(b) + ", " + this.h.toFixed(b) + "}";
        };
        a.createEmpty = function() {
          var b = a.allocate();
          b.setEmpty();
          return b;
        };
        a.createSquare = function() {
          return new a(-512, -512, 1024, 1024);
        };
        a.createMaxI16 = function() {
          return new a(-32768, -32768, 65535, 65535);
        };
        a.prototype.setMaxI16 = function() {
          this.setElements(-32768, -32768, 65535, 65535);
        };
        a.prototype.getCorners = function(b) {
          b[0].x = this.x;
          b[0].y = this.y;
          b[1].x = this.x + this.w;
          b[1].y = this.y;
          b[2].x = this.x + this.w;
          b[2].y = this.y + this.h;
          b[3].x = this.x;
          b[3].y = this.y + this.h;
        };
        a.allocationCount = 0;
        a._temporary = new a(0, 0, 0, 0);
        a._dirtyStack = [];
        return a;
      }();
      g.Rectangle = u;
      var m = function() {
        function a(b) {
          this.corners = b.map(function(b) {
            return b.clone();
          });
          this.axes = [b[1].clone().sub(b[0]), b[3].clone().sub(b[0])];
          this.origins = [];
          for (var f = 0;2 > f;f++) {
            this.axes[f].mul(1 / this.axes[f].squaredLength()), this.origins.push(b[0].dot(this.axes[f]));
          }
        }
        a.prototype.getBounds = function() {
          return a.getBounds(this.corners);
        };
        a.getBounds = function(b) {
          for (var f = new p(Number.MAX_VALUE, Number.MAX_VALUE), a = new p(Number.MIN_VALUE, Number.MIN_VALUE), e = 0;4 > e;e++) {
            var d = b[e].x, c = b[e].y;
            f.x = Math.min(f.x, d);
            f.y = Math.min(f.y, c);
            a.x = Math.max(a.x, d);
            a.y = Math.max(a.y, c);
          }
          return new u(f.x, f.y, a.x - f.x, a.y - f.y);
        };
        a.prototype.intersects = function(b) {
          return this.intersectsOneWay(b) && b.intersectsOneWay(this);
        };
        a.prototype.intersectsOneWay = function(b) {
          for (var f = 0;2 > f;f++) {
            for (var a = 0;4 > a;a++) {
              var e = b.corners[a].dot(this.axes[f]), d, c;
              0 === a ? c = d = e : e < d ? d = e : e > c && (c = e);
            }
            if (d > 1 + this.origins[f] || c < this.origins[f]) {
              return!1;
            }
          }
          return!0;
        };
        return a;
      }();
      g.OBB = m;
      (function(a) {
        a[a.Unknown = 0] = "Unknown";
        a[a.Identity = 1] = "Identity";
        a[a.Translation = 2] = "Translation";
      })(g.MatrixType || (g.MatrixType = {}));
      var s = function() {
        function a(b, f, d, c, h, m) {
          this._data = new Float64Array(6);
          this._type = 0;
          this.setElements(b, f, d, c, h, m);
          a.allocationCount++;
        }
        Object.defineProperty(a.prototype, "a", {get:function() {
          return this._data[0];
        }, set:function(b) {
          this._data[0] = b;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "b", {get:function() {
          return this._data[1];
        }, set:function(b) {
          this._data[1] = b;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "c", {get:function() {
          return this._data[2];
        }, set:function(b) {
          this._data[2] = b;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "d", {get:function() {
          return this._data[3];
        }, set:function(b) {
          this._data[3] = b;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "tx", {get:function() {
          return this._data[4];
        }, set:function(b) {
          this._data[4] = b;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "ty", {get:function() {
          return this._data[5];
        }, set:function(b) {
          this._data[5] = b;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        a._createSVGMatrix = function() {
          a._svg || (a._svg = document.createElementNS("http://www.w3.org/2000/svg", "svg"));
          return a._svg.createSVGMatrix();
        };
        a.prototype.setElements = function(b, a, d, e, c, h) {
          var m = this._data;
          m[0] = b;
          m[1] = a;
          m[2] = d;
          m[3] = e;
          m[4] = c;
          m[5] = h;
          this._type = 0;
        };
        a.prototype.set = function(b) {
          var a = this._data, d = b._data;
          a[0] = d[0];
          a[1] = d[1];
          a[2] = d[2];
          a[3] = d[3];
          a[4] = d[4];
          a[5] = d[5];
          this._type = b._type;
        };
        a.prototype.emptyArea = function(b) {
          b = this._data;
          return 0 === b[0] || 0 === b[3] ? !0 : !1;
        };
        a.prototype.infiniteArea = function(b) {
          b = this._data;
          return Infinity === Math.abs(b[0]) || Infinity === Math.abs(b[3]) ? !0 : !1;
        };
        a.prototype.isEqual = function(b) {
          if (1 === this._type && 1 === b._type) {
            return!0;
          }
          var a = this._data;
          b = b._data;
          return a[0] === b[0] && a[1] === b[1] && a[2] === b[2] && a[3] === b[3] && a[4] === b[4] && a[5] === b[5];
        };
        a.prototype.clone = function() {
          var b = a.allocate();
          b.set(this);
          return b;
        };
        a.allocate = function() {
          var b = a._dirtyStack;
          return b.length ? b.pop() : new a(12345, 12345, 12345, 12345, 12345, 12345);
        };
        a.prototype.free = function() {
          a._dirtyStack.push(this);
        };
        a.prototype.transform = function(b, a, d, e, c, h) {
          var m = this._data, g = m[0], l = m[1], p = m[2], k = m[3], s = m[4], n = m[5];
          m[0] = g * b + p * a;
          m[1] = l * b + k * a;
          m[2] = g * d + p * e;
          m[3] = l * d + k * e;
          m[4] = g * c + p * h + s;
          m[5] = l * c + k * h + n;
          this._type = 0;
          return this;
        };
        a.prototype.transformRectangle = function(b, a) {
          var d = this._data, e = d[0], c = d[1], m = d[2], h = d[3], g = d[4], d = d[5], l = b.x, p = b.y, k = b.w, s = b.h;
          a[0].x = e * l + m * p + g;
          a[0].y = c * l + h * p + d;
          a[1].x = e * (l + k) + m * p + g;
          a[1].y = c * (l + k) + h * p + d;
          a[2].x = e * (l + k) + m * (p + s) + g;
          a[2].y = c * (l + k) + h * (p + s) + d;
          a[3].x = e * l + m * (p + s) + g;
          a[3].y = c * l + h * (p + s) + d;
        };
        a.prototype.isTranslationOnly = function() {
          if (2 === this._type) {
            return!0;
          }
          var b = this._data;
          return 1 === b[0] && 0 === b[1] && 0 === b[2] && 1 === b[3] || h(b[0], 1) && h(b[1], 0) && h(b[2], 0) && h(b[3], 1) ? (this._type = 2, !0) : !1;
        };
        a.prototype.transformRectangleAABB = function(b) {
          var a = this._data;
          if (1 !== this._type) {
            if (2 === this._type) {
              b.x += a[4], b.y += a[5];
            } else {
              var d = a[0], e = a[1], c = a[2], m = a[3], h = a[4], g = a[5], l = b.x, p = b.y, k = b.w, s = b.h, a = d * l + c * p + h, n = e * l + m * p + g, v = d * (l + k) + c * p + h, u = e * (l + k) + m * p + g, r = d * (l + k) + c * (p + s) + h, k = e * (l + k) + m * (p + s) + g, d = d * l + c * (p + s) + h, e = e * l + m * (p + s) + g, m = 0;
              a > v && (m = a, a = v, v = m);
              r > d && (m = r, r = d, d = m);
              b.x = a < r ? a : r;
              b.w = (v > d ? v : d) - b.x;
              n > u && (m = n, n = u, u = m);
              k > e && (m = k, k = e, e = m);
              b.y = n < k ? n : k;
              b.h = (u > e ? u : e) - b.y;
            }
          }
        };
        a.prototype.scale = function(b, a) {
          var d = this._data;
          d[0] *= b;
          d[1] *= a;
          d[2] *= b;
          d[3] *= a;
          d[4] *= b;
          d[5] *= a;
          this._type = 0;
          return this;
        };
        a.prototype.scaleClone = function(b, a) {
          return 1 === b && 1 === a ? this : this.clone().scale(b, a);
        };
        a.prototype.rotate = function(b) {
          var a = this._data, d = a[0], e = a[1], c = a[2], m = a[3], h = a[4], g = a[5], l = Math.cos(b);
          b = Math.sin(b);
          a[0] = l * d - b * e;
          a[1] = b * d + l * e;
          a[2] = l * c - b * m;
          a[3] = b * c + l * m;
          a[4] = l * h - b * g;
          a[5] = b * h + l * g;
          this._type = 0;
          return this;
        };
        a.prototype.concat = function(b) {
          if (1 === b._type) {
            return this;
          }
          var a = this._data;
          b = b._data;
          var d = a[0] * b[0], e = 0, c = 0, m = a[3] * b[3], h = a[4] * b[0] + b[4], g = a[5] * b[3] + b[5];
          if (0 !== a[1] || 0 !== a[2] || 0 !== b[1] || 0 !== b[2]) {
            d += a[1] * b[2], m += a[2] * b[1], e += a[0] * b[1] + a[1] * b[3], c += a[2] * b[0] + a[3] * b[2], h += a[5] * b[2], g += a[4] * b[1];
          }
          a[0] = d;
          a[1] = e;
          a[2] = c;
          a[3] = m;
          a[4] = h;
          a[5] = g;
          this._type = 0;
          return this;
        };
        a.prototype.concatClone = function(b) {
          return this.clone().concat(b);
        };
        a.prototype.preMultiply = function(b) {
          var a = this._data, d = b._data;
          if (2 === b._type && this._type & 3) {
            a[4] += d[4], a[5] += d[5], this._type = 2;
          } else {
            if (1 !== b._type) {
              b = d[0] * a[0];
              var e = 0, c = 0, m = d[3] * a[3], h = d[4] * a[0] + a[4], g = d[5] * a[3] + a[5];
              if (0 !== d[1] || 0 !== d[2] || 0 !== a[1] || 0 !== a[2]) {
                b += d[1] * a[2], m += d[2] * a[1], e += d[0] * a[1] + d[1] * a[3], c += d[2] * a[0] + d[3] * a[2], h += d[5] * a[2], g += d[4] * a[1];
              }
              a[0] = b;
              a[1] = e;
              a[2] = c;
              a[3] = m;
              a[4] = h;
              a[5] = g;
              this._type = 0;
            }
          }
        };
        a.prototype.translate = function(b, a) {
          var d = this._data;
          d[4] += b;
          d[5] += a;
          1 === this._type && (this._type = 2);
          return this;
        };
        a.prototype.setIdentity = function() {
          var b = this._data;
          b[0] = 1;
          b[1] = 0;
          b[2] = 0;
          b[3] = 1;
          b[4] = 0;
          b[5] = 0;
          this._type = 1;
        };
        a.prototype.isIdentity = function() {
          if (1 === this._type) {
            return!0;
          }
          var b = this._data;
          return 1 === b[0] && 0 === b[1] && 0 === b[2] && 1 === b[3] && 0 === b[4] && 0 === b[5];
        };
        a.prototype.transformPoint = function(b) {
          if (1 !== this._type) {
            var a = this._data, d = b.x, e = b.y;
            b.x = a[0] * d + a[2] * e + a[4];
            b.y = a[1] * d + a[3] * e + a[5];
          }
        };
        a.prototype.transformPoints = function(b) {
          if (1 !== this._type) {
            for (var a = 0;a < b.length;a++) {
              this.transformPoint(b[a]);
            }
          }
        };
        a.prototype.deltaTransformPoint = function(b) {
          if (1 !== this._type) {
            var a = this._data, d = b.x, e = b.y;
            b.x = a[0] * d + a[2] * e;
            b.y = a[1] * d + a[3] * e;
          }
        };
        a.prototype.inverse = function(b) {
          var a = this._data, d = b._data;
          if (1 === this._type) {
            b.setIdentity();
          } else {
            if (2 === this._type) {
              d[0] = 1, d[1] = 0, d[2] = 0, d[3] = 1, d[4] = -a[4], d[5] = -a[5], b._type = 2;
            } else {
              var e = a[1], c = a[2], m = a[4], h = a[5];
              if (0 === e && 0 === c) {
                var g = d[0] = 1 / a[0], a = d[3] = 1 / a[3];
                d[1] = 0;
                d[2] = 0;
                d[4] = -g * m;
                d[5] = -a * h;
              } else {
                var g = a[0], a = a[3], l = g * a - e * c;
                if (0 === l) {
                  b.setIdentity();
                  return;
                }
                l = 1 / l;
                d[0] = a * l;
                e = d[1] = -e * l;
                c = d[2] = -c * l;
                a = d[3] = g * l;
                d[4] = -(d[0] * m + c * h);
                d[5] = -(e * m + a * h);
              }
              b._type = 0;
            }
          }
        };
        a.prototype.getTranslateX = function() {
          return this._data[4];
        };
        a.prototype.getTranslateY = function() {
          return this._data[4];
        };
        a.prototype.getScaleX = function() {
          var b = this._data;
          if (1 === b[0] && 0 === b[1]) {
            return 1;
          }
          var a = Math.sqrt(b[0] * b[0] + b[1] * b[1]);
          return 0 < b[0] ? a : -a;
        };
        a.prototype.getScaleY = function() {
          var b = this._data;
          if (0 === b[2] && 1 === b[3]) {
            return 1;
          }
          var a = Math.sqrt(b[2] * b[2] + b[3] * b[3]);
          return 0 < b[3] ? a : -a;
        };
        a.prototype.getScale = function() {
          return(this.getScaleX() + this.getScaleY()) / 2;
        };
        a.prototype.getAbsoluteScaleX = function() {
          return Math.abs(this.getScaleX());
        };
        a.prototype.getAbsoluteScaleY = function() {
          return Math.abs(this.getScaleY());
        };
        a.prototype.getRotation = function() {
          var b = this._data;
          return 180 * Math.atan(b[1] / b[0]) / Math.PI;
        };
        a.prototype.isScaleOrRotation = function() {
          var b = this._data;
          return.01 > Math.abs(b[0] * b[2] + b[1] * b[3]);
        };
        a.prototype.toString = function(b) {
          void 0 === b && (b = 2);
          var a = this._data;
          return "{" + a[0].toFixed(b) + ", " + a[1].toFixed(b) + ", " + a[2].toFixed(b) + ", " + a[3].toFixed(b) + ", " + a[4].toFixed(b) + ", " + a[5].toFixed(b) + "}";
        };
        a.prototype.toWebGLMatrix = function() {
          var b = this._data;
          return new Float32Array([b[0], b[1], 0, b[2], b[3], 0, b[4], b[5], 1]);
        };
        a.prototype.toCSSTransform = function() {
          var b = this._data;
          return "matrix(" + b[0] + ", " + b[1] + ", " + b[2] + ", " + b[3] + ", " + b[4] + ", " + b[5] + ")";
        };
        a.createIdentity = function() {
          var b = a.allocate();
          b.setIdentity();
          return b;
        };
        a.prototype.toSVGMatrix = function() {
          var b = this._data, f = a._createSVGMatrix();
          f.a = b[0];
          f.b = b[1];
          f.c = b[2];
          f.d = b[3];
          f.e = b[4];
          f.f = b[5];
          return f;
        };
        a.prototype.snap = function() {
          var b = this._data;
          return this.isTranslationOnly() ? (b[0] = 1, b[1] = 0, b[2] = 0, b[3] = 1, b[4] = Math.round(b[4]), b[5] = Math.round(b[5]), this._type = 2, !0) : !1;
        };
        a.createIdentitySVGMatrix = function() {
          return a._createSVGMatrix();
        };
        a.createSVGMatrixFromArray = function(b) {
          var f = a._createSVGMatrix();
          f.a = b[0];
          f.b = b[1];
          f.c = b[2];
          f.d = b[3];
          f.e = b[4];
          f.f = b[5];
          return f;
        };
        a.allocationCount = 0;
        a._dirtyStack = [];
        a.multiply = function(b, a) {
          var d = a._data;
          b.transform(d[0], d[1], d[2], d[3], d[4], d[5]);
        };
        return a;
      }();
      g.Matrix = s;
      s = function() {
        function a(b) {
          this._m = new Float32Array(b);
        }
        a.prototype.asWebGLMatrix = function() {
          return this._m;
        };
        a.createCameraLookAt = function(b, f, d) {
          f = b.clone().sub(f).normalize();
          d = d.clone().cross(f).normalize();
          var c = f.clone().cross(d);
          return new a([d.x, d.y, d.z, 0, c.x, c.y, c.z, 0, f.x, f.y, f.z, 0, b.x, b.y, b.z, 1]);
        };
        a.createLookAt = function(b, f, d) {
          f = b.clone().sub(f).normalize();
          d = d.clone().cross(f).normalize();
          var c = f.clone().cross(d);
          return new a([d.x, c.x, f.x, 0, c.x, c.y, f.y, 0, f.x, c.z, f.z, 0, -d.dot(b), -c.dot(b), -f.dot(b), 1]);
        };
        a.prototype.mul = function(b) {
          b = [b.x, b.y, b.z, 0];
          for (var a = this._m, d = [], e = 0;4 > e;e++) {
            d[e] = 0;
            for (var c = 4 * e, m = 0;4 > m;m++) {
              d[e] += a[c + m] * b[m];
            }
          }
          return new l(d[0], d[1], d[2]);
        };
        a.create2DProjection = function(b, f, d) {
          return new a([2 / b, 0, 0, 0, 0, -2 / f, 0, 0, 0, 0, 2 / d, 0, -1, 1, 0, 1]);
        };
        a.createPerspective = function(b) {
          b = Math.tan(.5 * Math.PI - .5 * b);
          var f = 1 / -4999.9;
          return new a([b / 1, 0, 0, 0, 0, b, 0, 0, 0, 0, 5000.1 * f, -1, 0, 0, 1E3 * f, 0]);
        };
        a.createIdentity = function() {
          return a.createTranslation(0, 0);
        };
        a.createTranslation = function(b, f) {
          return new a([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, b, f, 0, 1]);
        };
        a.createXRotation = function(b) {
          var f = Math.cos(b);
          b = Math.sin(b);
          return new a([1, 0, 0, 0, 0, f, b, 0, 0, -b, f, 0, 0, 0, 0, 1]);
        };
        a.createYRotation = function(b) {
          var f = Math.cos(b);
          b = Math.sin(b);
          return new a([f, 0, -b, 0, 0, 1, 0, 0, b, 0, f, 0, 0, 0, 0, 1]);
        };
        a.createZRotation = function(b) {
          var f = Math.cos(b);
          b = Math.sin(b);
          return new a([f, b, 0, 0, -b, f, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
        };
        a.createScale = function(b, f, d) {
          return new a([b, 0, 0, 0, 0, f, 0, 0, 0, 0, d, 0, 0, 0, 0, 1]);
        };
        a.createMultiply = function(b, f) {
          var d = b._m, c = f._m, m = d[0], h = d[1], g = d[2], l = d[3], p = d[4], k = d[5], s = d[6], n = d[7], v = d[8], u = d[9], r = d[10], t = d[11], y = d[12], z = d[13], B = d[14], d = d[15], x = c[0], E = c[1], A = c[2], D = c[3], F = c[4], I = c[5], J = c[6], K = c[7], L = c[8], M = c[9], N = c[10], O = c[11], R = c[12], S = c[13], T = c[14], c = c[15];
          return new a([m * x + h * F + g * L + l * R, m * E + h * I + g * M + l * S, m * A + h * J + g * N + l * T, m * D + h * K + g * O + l * c, p * x + k * F + s * L + n * R, p * E + k * I + s * M + n * S, p * A + k * J + s * N + n * T, p * D + k * K + s * O + n * c, v * x + u * F + r * L + t * R, v * E + u * I + r * M + t * S, v * A + u * J + r * N + t * T, v * D + u * K + r * O + t * c, y * x + z * F + B * L + d * R, y * E + z * I + B * M + d * S, y * A + z * J + B * N + d * T, y * D + z * 
          K + B * O + d * c]);
        };
        a.createInverse = function(b) {
          var f = b._m;
          b = f[0];
          var d = f[1], c = f[2], m = f[3], h = f[4], g = f[5], l = f[6], p = f[7], k = f[8], s = f[9], n = f[10], v = f[11], u = f[12], r = f[13], t = f[14], f = f[15], y = n * f, z = t * v, B = l * f, x = t * p, E = l * v, A = n * p, D = c * f, F = t * m, I = c * v, J = n * m, K = c * p, L = l * m, M = k * r, N = u * s, O = h * r, R = u * g, S = h * s, T = k * g, X = b * r, Y = u * d, Z = b * s, $ = k * d, aa = b * g, ba = h * d, da = y * g + x * s + E * r - (z * g + B * s + A * r), ea = z * d + 
          D * s + J * r - (y * d + F * s + I * r), r = B * d + F * g + K * r - (x * d + D * g + L * r), d = A * d + I * g + L * s - (E * d + J * g + K * s), g = 1 / (b * da + h * ea + k * r + u * d);
          return new a([g * da, g * ea, g * r, g * d, g * (z * h + B * k + A * u - (y * h + x * k + E * u)), g * (y * b + F * k + I * u - (z * b + D * k + J * u)), g * (x * b + D * h + L * u - (B * b + F * h + K * u)), g * (E * b + J * h + K * k - (A * b + I * h + L * k)), g * (M * p + R * v + S * f - (N * p + O * v + T * f)), g * (N * m + X * v + $ * f - (M * m + Y * v + Z * f)), g * (O * m + Y * p + aa * f - (R * m + X * p + ba * f)), g * (T * m + Z * p + ba * v - (S * m + $ * p + aa * v)), g * 
          (O * n + T * t + N * l - (S * t + M * l + R * n)), g * (Z * t + M * c + Y * n - (X * n + $ * t + N * c)), g * (X * l + ba * t + R * c - (aa * t + O * c + Y * l)), g * (aa * n + S * c + $ * l - (Z * l + ba * n + T * c))]);
        };
        return a;
      }();
      g.Matrix3D = s;
      s = function() {
        function a(b, f, d) {
          void 0 === d && (d = 7);
          var c = this.size = 1 << d;
          this.sizeInBits = d;
          this.w = b;
          this.h = f;
          this.c = Math.ceil(b / c);
          this.r = Math.ceil(f / c);
          this.grid = [];
          for (b = 0;b < this.r;b++) {
            for (this.grid.push([]), f = 0;f < this.c;f++) {
              this.grid[b][f] = new a.Cell(new u(f * c, b * c, c, c));
            }
          }
        }
        a.prototype.clear = function() {
          for (var b = 0;b < this.r;b++) {
            for (var a = 0;a < this.c;a++) {
              this.grid[b][a].clear();
            }
          }
        };
        a.prototype.getBounds = function() {
          return new u(0, 0, this.w, this.h);
        };
        a.prototype.addDirtyRectangle = function(b) {
          var a = b.x >> this.sizeInBits, d = b.y >> this.sizeInBits;
          if (!(a >= this.c || d >= this.r)) {
            0 > a && (a = 0);
            0 > d && (d = 0);
            var c = this.grid[d][a];
            b = b.clone();
            b.snap();
            if (c.region.contains(b)) {
              c.bounds.isEmpty() ? c.bounds.set(b) : c.bounds.contains(b) || c.bounds.union(b);
            } else {
              for (var e = Math.min(this.c, Math.ceil((b.x + b.w) / this.size)) - a, m = Math.min(this.r, Math.ceil((b.y + b.h) / this.size)) - d, h = 0;h < e;h++) {
                for (var g = 0;g < m;g++) {
                  c = this.grid[d + g][a + h], c = c.region.clone(), c.intersect(b), c.isEmpty() || this.addDirtyRectangle(c);
                }
              }
            }
          }
        };
        a.prototype.gatherRegions = function(b) {
          for (var a = 0;a < this.r;a++) {
            for (var d = 0;d < this.c;d++) {
              this.grid[a][d].bounds.isEmpty() || b.push(this.grid[a][d].bounds);
            }
          }
        };
        a.prototype.gatherOptimizedRegions = function(b) {
          this.gatherRegions(b);
        };
        a.prototype.getDirtyRatio = function() {
          var b = this.w * this.h;
          if (0 === b) {
            return 0;
          }
          for (var a = 0, d = 0;d < this.r;d++) {
            for (var c = 0;c < this.c;c++) {
              a += this.grid[d][c].region.area();
            }
          }
          return a / b;
        };
        a.prototype.render = function(b, a) {
          function d(a) {
            b.rect(a.x, a.y, a.w, a.h);
          }
          if (a && a.drawGrid) {
            b.strokeStyle = "white";
            for (var c = 0;c < this.r;c++) {
              for (var e = 0;e < this.c;e++) {
                var m = this.grid[c][e];
                b.beginPath();
                d(m.region);
                b.closePath();
                b.stroke();
              }
            }
          }
          b.strokeStyle = "#E0F8D8";
          for (c = 0;c < this.r;c++) {
            for (e = 0;e < this.c;e++) {
              m = this.grid[c][e], b.beginPath(), d(m.bounds), b.closePath(), b.stroke();
            }
          }
        };
        a.tmpRectangle = u.createEmpty();
        return a;
      }();
      g.DirtyRegion = s;
      (function(a) {
        var b = function() {
          function b(a) {
            this.region = a;
            this.bounds = u.createEmpty();
          }
          b.prototype.clear = function() {
            this.bounds.setEmpty();
          };
          return b;
        }();
        a.Cell = b;
      })(s = g.DirtyRegion || (g.DirtyRegion = {}));
      var v = function() {
        function a(b, f, d, c, e, m) {
          this.index = b;
          this.x = f;
          this.y = d;
          this.scale = m;
          this.bounds = new u(f * c, d * e, c, e);
        }
        a.prototype.getOBB = function() {
          if (this._obb) {
            return this._obb;
          }
          this.bounds.getCorners(a.corners);
          return this._obb = new m(a.corners);
        };
        a.corners = p.createEmptyPoints(4);
        return a;
      }();
      g.Tile = v;
      var d = function() {
        function a(b, f, d, c, e) {
          this.tileW = d;
          this.tileH = c;
          this.scale = e;
          this.w = b;
          this.h = f;
          this.rows = Math.ceil(f / c);
          this.columns = Math.ceil(b / d);
          this.tiles = [];
          for (f = b = 0;f < this.rows;f++) {
            for (var m = 0;m < this.columns;m++) {
              this.tiles.push(new v(b++, m, f, d, c, e));
            }
          }
        }
        a.prototype.getTiles = function(b, a) {
          if (a.emptyArea(b)) {
            return[];
          }
          if (a.infiniteArea(b)) {
            return this.tiles;
          }
          var d = this.columns * this.rows;
          return 40 > d && a.isScaleOrRotation() ? this.getFewTiles(b, a, 10 < d) : this.getManyTiles(b, a);
        };
        a.prototype.getFewTiles = function(b, f, d) {
          void 0 === d && (d = !0);
          if (f.isTranslationOnly() && 1 === this.tiles.length) {
            return this.tiles[0].bounds.intersectsTranslated(b, f.tx, f.ty) ? [this.tiles[0]] : [];
          }
          f.transformRectangle(b, a._points);
          var c;
          b = new u(0, 0, this.w, this.h);
          d && (c = new m(a._points));
          b.intersect(m.getBounds(a._points));
          if (b.isEmpty()) {
            return[];
          }
          var h = b.x / this.tileW | 0;
          f = b.y / this.tileH | 0;
          var g = Math.ceil((b.x + b.w) / this.tileW) | 0, l = Math.ceil((b.y + b.h) / this.tileH) | 0, h = n(h, 0, this.columns), g = n(g, 0, this.columns);
          f = n(f, 0, this.rows);
          for (var l = n(l, 0, this.rows), p = [];h < g;h++) {
            for (var k = f;k < l;k++) {
              var s = this.tiles[k * this.columns + h];
              s.bounds.intersects(b) && (d ? s.getOBB().intersects(c) : 1) && p.push(s);
            }
          }
          return p;
        };
        a.prototype.getManyTiles = function(b, f) {
          function d(b, a, f) {
            return(b - a.x) * (f.y - a.y) / (f.x - a.x) + a.y;
          }
          function c(b, a, f, d, e) {
            if (!(0 > f || f >= a.columns)) {
              for (d = n(d, 0, a.rows), e = n(e + 1, 0, a.rows);d < e;d++) {
                b.push(a.tiles[d * a.columns + f]);
              }
            }
          }
          var m = a._points;
          f.transformRectangle(b, m);
          for (var h = m[0].x < m[1].x ? 0 : 1, g = m[2].x < m[3].x ? 2 : 3, g = m[h].x < m[g].x ? h : g, h = [], l = 0;5 > l;l++, g++) {
            h.push(m[g % 4]);
          }
          (h[1].x - h[0].x) * (h[3].y - h[0].y) < (h[1].y - h[0].y) * (h[3].x - h[0].x) && (m = h[1], h[1] = h[3], h[3] = m);
          var m = [], p, k, g = Math.floor(h[0].x / this.tileW), l = (g + 1) * this.tileW;
          if (h[2].x < l) {
            p = Math.min(h[0].y, h[1].y, h[2].y, h[3].y);
            k = Math.max(h[0].y, h[1].y, h[2].y, h[3].y);
            var s = Math.floor(p / this.tileH), v = Math.floor(k / this.tileH);
            c(m, this, g, s, v);
            return m;
          }
          var u = 0, r = 4, t = !1;
          if (h[0].x === h[1].x || h[0].x === h[3].x) {
            h[0].x === h[1].x ? (t = !0, u++) : r--, p = d(l, h[u], h[u + 1]), k = d(l, h[r], h[r - 1]), s = Math.floor(h[u].y / this.tileH), v = Math.floor(h[r].y / this.tileH), c(m, this, g, s, v), g++;
          }
          do {
            var C, y, z, B;
            h[u + 1].x < l ? (C = h[u + 1].y, z = !0) : (C = d(l, h[u], h[u + 1]), z = !1);
            h[r - 1].x < l ? (y = h[r - 1].y, B = !0) : (y = d(l, h[r], h[r - 1]), B = !1);
            s = Math.floor((h[u].y < h[u + 1].y ? p : C) / this.tileH);
            v = Math.floor((h[r].y > h[r - 1].y ? k : y) / this.tileH);
            c(m, this, g, s, v);
            if (z && t) {
              break;
            }
            z ? (t = !0, u++, p = d(l, h[u], h[u + 1])) : p = C;
            B ? (r--, k = d(l, h[r], h[r - 1])) : k = y;
            g++;
            l = (g + 1) * this.tileW;
          } while (u < r);
          return m;
        };
        a._points = p.createEmptyPoints(4);
        return a;
      }();
      g.TileCache = d;
      s = function() {
        function c(b, a, d) {
          this._cacheLevels = [];
          this._source = b;
          this._tileSize = a;
          this._minUntiledSize = d;
        }
        c.prototype._getTilesAtScale = function(b, f, c) {
          var e = Math.max(f.getAbsoluteScaleX(), f.getAbsoluteScaleY()), h = 0;
          1 !== e && (h = n(Math.round(Math.log(1 / e) / Math.LN2), -5, 3));
          e = a(h);
          if (this._source.hasFlags(1048576)) {
            for (;;) {
              e = a(h);
              if (c.contains(this._source.getBounds().getAbsoluteBounds().clone().scale(e, e))) {
                break;
              }
              h--;
            }
          }
          this._source.hasFlags(2097152) || (h = n(h, -5, 0));
          e = a(h);
          c = 5 + h;
          h = this._cacheLevels[c];
          if (!h) {
            var h = this._source.getBounds().getAbsoluteBounds().clone().scale(e, e), m, g;
            this._source.hasFlags(1048576) || !this._source.hasFlags(4194304) || Math.max(h.w, h.h) <= this._minUntiledSize ? (m = h.w, g = h.h) : m = g = this._tileSize;
            h = this._cacheLevels[c] = new d(h.w, h.h, m, g, e);
          }
          return h.getTiles(b, f.scaleClone(e, e));
        };
        c.prototype.fetchTiles = function(b, a, d, c) {
          var e = new u(0, 0, d.canvas.width, d.canvas.height);
          b = this._getTilesAtScale(b, a, e);
          var h;
          a = this._source;
          for (var m = 0;m < b.length;m++) {
            var g = b[m];
            g.cachedSurfaceRegion && g.cachedSurfaceRegion.surface && !a.hasFlags(1048592) || (h || (h = []), h.push(g));
          }
          h && this._cacheTiles(d, h, c, e);
          a.removeFlags(16);
          return b;
        };
        c.prototype._getTileBounds = function(b) {
          for (var a = u.createEmpty(), d = 0;d < b.length;d++) {
            a.union(b[d].bounds);
          }
          return a;
        };
        c.prototype._cacheTiles = function(b, a, d, c, e) {
          void 0 === e && (e = 4);
          var h = this._getTileBounds(a);
          b.save();
          b.setTransform(1, 0, 0, 1, 0, 0);
          b.clearRect(0, 0, c.w, c.h);
          b.translate(-h.x, -h.y);
          b.scale(a[0].scale, a[0].scale);
          var m = this._source.getBounds();
          b.translate(-m.x, -m.y);
          2 <= r.traceLevel && r.writer && r.writer.writeLn("Rendering Tiles: " + h);
          this._source.render(b, 0);
          b.restore();
          for (var m = null, g = 0;g < a.length;g++) {
            var l = a[g], p = l.bounds.clone();
            p.x -= h.x;
            p.y -= h.y;
            c.contains(p) || (m || (m = []), m.push(l));
            l.cachedSurfaceRegion = d(l.cachedSurfaceRegion, b, p);
          }
          m && (2 <= m.length ? (a = m.slice(0, m.length / 2 | 0), h = m.slice(a.length), this._cacheTiles(b, a, d, c, e - 1), this._cacheTiles(b, h, d, c, e - 1)) : this._cacheTiles(b, m, d, c, e - 1));
        };
        return c;
      }();
      g.RenderableTileCache = s;
    })(r.Geometry || (r.Geometry = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, r) {
  function g() {
    this.constructor = k;
  }
  for (var c in r) {
    r.hasOwnProperty(c) && (k[c] = r[c]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(r) {
    var g = k.IntegerUtilities.roundToMultipleOfPowerOfTwo, c = r.Geometry.Rectangle;
    (function(k) {
      var n = function(a) {
        function c() {
          a.apply(this, arguments);
        }
        __extends(c, a);
        return c;
      }(r.Geometry.Rectangle);
      k.Region = n;
      var a = function() {
        function a(c, d) {
          this._root = new h(0, 0, c | 0, d | 0, !1);
        }
        a.prototype.allocate = function(a, d) {
          a = Math.ceil(a);
          d = Math.ceil(d);
          var c = this._root.insert(a, d);
          c && (c.allocator = this, c.allocated = !0);
          return c;
        };
        a.prototype.free = function(a) {
          a.clear();
          a.allocated = !1;
        };
        a.RANDOM_ORIENTATION = !0;
        a.MAX_DEPTH = 256;
        return a;
      }();
      k.CompactAllocator = a;
      var h = function(c) {
        function h(a, e, b, f, q) {
          c.call(this, a, e, b, f);
          this._children = null;
          this._horizontal = q;
          this.allocated = !1;
        }
        __extends(h, c);
        h.prototype.clear = function() {
          this._children = null;
          this.allocated = !1;
        };
        h.prototype.insert = function(a, c) {
          return this._insert(a, c, 0);
        };
        h.prototype._insert = function(d, c, b) {
          if (!(b > a.MAX_DEPTH || this.allocated || this.w < d || this.h < c)) {
            if (this._children) {
              var f;
              if ((f = this._children[0]._insert(d, c, b + 1)) || (f = this._children[1]._insert(d, c, b + 1))) {
                return f;
              }
            } else {
              return f = !this._horizontal, a.RANDOM_ORIENTATION && (f = .5 <= Math.random()), this._children = this._horizontal ? [new h(this.x, this.y, this.w, c, !1), new h(this.x, this.y + c, this.w, this.h - c, f)] : [new h(this.x, this.y, d, this.h, !0), new h(this.x + d, this.y, this.w - d, this.h, f)], f = this._children[0], f.w === d && f.h === c ? (f.allocated = !0, f) : this._insert(d, c, b + 1);
            }
          }
        };
        return h;
      }(k.Region), p = function() {
        function a(c, d, e, b) {
          this._columns = c / e | 0;
          this._rows = d / b | 0;
          this._sizeW = e;
          this._sizeH = b;
          this._freeList = [];
          this._index = 0;
          this._total = this._columns * this._rows;
        }
        a.prototype.allocate = function(a, d) {
          a = Math.ceil(a);
          d = Math.ceil(d);
          var c = this._sizeW, b = this._sizeH;
          if (a > c || d > b) {
            return null;
          }
          var f = this._freeList, q = this._index;
          return 0 < f.length ? (c = f.pop(), c.w = a, c.h = d, c.allocated = !0, c) : q < this._total ? (f = q / this._columns | 0, c = new l((q - f * this._columns) * c, f * b, a, d), c.index = q, c.allocator = this, c.allocated = !0, this._index++, c) : null;
        };
        a.prototype.free = function(a) {
          a.allocated = !1;
          this._freeList.push(a);
        };
        return a;
      }();
      k.GridAllocator = p;
      var l = function(a) {
        function c(d, e, b, f) {
          a.call(this, d, e, b, f);
          this.index = -1;
        }
        __extends(c, a);
        return c;
      }(k.Region);
      k.GridCell = l;
      var u = function() {
        return function(a, c, d) {
          this.size = a;
          this.region = c;
          this.allocator = d;
        };
      }(), m = function(a) {
        function c(d, e, b, f, q) {
          a.call(this, d, e, b, f);
          this.region = q;
        }
        __extends(c, a);
        return c;
      }(k.Region);
      k.BucketCell = m;
      n = function() {
        function a(c, d) {
          this._buckets = [];
          this._w = c | 0;
          this._h = d | 0;
          this._filled = 0;
        }
        a.prototype.allocate = function(a, d) {
          a = Math.ceil(a);
          d = Math.ceil(d);
          var e = Math.max(a, d);
          if (a > this._w || d > this._h) {
            return null;
          }
          var b = null, f, q = this._buckets;
          do {
            for (var h = 0;h < q.length && !(q[h].size >= e && (f = q[h], b = f.allocator.allocate(a, d)));h++) {
            }
            if (!b) {
              var l = this._h - this._filled;
              if (l < d) {
                return null;
              }
              var h = g(e, 8), k = 2 * h;
              k > l && (k = l);
              if (k < h) {
                return null;
              }
              l = new c(0, this._filled, this._w, k);
              this._buckets.push(new u(h, l, new p(l.w, l.h, h, h)));
              this._filled += k;
            }
          } while (!b);
          return new m(f.region.x + b.x, f.region.y + b.y, b.w, b.h, b);
        };
        a.prototype.free = function(a) {
          a.region.allocator.free(a.region);
        };
        return a;
      }();
      k.BucketAllocator = n;
    })(r.RegionAllocator || (r.RegionAllocator = {}));
    (function(c) {
      var g = function() {
        function a(a) {
          this._createSurface = a;
          this._surfaces = [];
        }
        Object.defineProperty(a.prototype, "surfaces", {get:function() {
          return this._surfaces;
        }, enumerable:!0, configurable:!0});
        a.prototype._createNewSurface = function(a, c) {
          var g = this._createSurface(a, c);
          this._surfaces.push(g);
          return g;
        };
        a.prototype.addSurface = function(a) {
          this._surfaces.push(a);
        };
        a.prototype.allocate = function(a, c, g) {
          for (var k = 0;k < this._surfaces.length;k++) {
            var m = this._surfaces[k];
            if (m !== g && (m = m.allocate(a, c))) {
              return m;
            }
          }
          return this._createNewSurface(a, c).allocate(a, c);
        };
        a.prototype.free = function(a) {
        };
        return a;
      }();
      c.SimpleAllocator = g;
    })(r.SurfaceRegionAllocator || (r.SurfaceRegionAllocator = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = r.Geometry.Rectangle, c = r.Geometry.Matrix, t = r.Geometry.DirtyRegion;
    (function(b) {
      b[b.Normal = 1] = "Normal";
      b[b.Layer = 2] = "Layer";
      b[b.Multiply = 3] = "Multiply";
      b[b.Screen = 4] = "Screen";
      b[b.Lighten = 5] = "Lighten";
      b[b.Darken = 6] = "Darken";
      b[b.Difference = 7] = "Difference";
      b[b.Add = 8] = "Add";
      b[b.Subtract = 9] = "Subtract";
      b[b.Invert = 10] = "Invert";
      b[b.Alpha = 11] = "Alpha";
      b[b.Erase = 12] = "Erase";
      b[b.Overlay = 13] = "Overlay";
      b[b.HardLight = 14] = "HardLight";
    })(r.BlendMode || (r.BlendMode = {}));
    var n = r.BlendMode;
    (function(b) {
      b[b.None = 0] = "None";
      b[b.BoundsAutoCompute = 2] = "BoundsAutoCompute";
      b[b.IsMask = 4] = "IsMask";
      b[b.Dirty = 16] = "Dirty";
      b[b.InvalidBounds = 256] = "InvalidBounds";
      b[b.InvalidConcatenatedMatrix = 512] = "InvalidConcatenatedMatrix";
      b[b.InvalidInvertedConcatenatedMatrix = 1024] = "InvalidInvertedConcatenatedMatrix";
      b[b.InvalidConcatenatedColorMatrix = 2048] = "InvalidConcatenatedColorMatrix";
      b[b.UpOnAddedOrRemoved = b.InvalidBounds | b.Dirty] = "UpOnAddedOrRemoved";
      b[b.UpOnMoved = b.InvalidBounds | b.Dirty] = "UpOnMoved";
      b[b.DownOnAddedOrRemoved = b.InvalidConcatenatedMatrix | b.InvalidInvertedConcatenatedMatrix | b.InvalidConcatenatedColorMatrix] = "DownOnAddedOrRemoved";
      b[b.DownOnMoved = b.InvalidConcatenatedMatrix | b.InvalidInvertedConcatenatedMatrix | b.InvalidConcatenatedColorMatrix] = "DownOnMoved";
      b[b.UpOnColorMatrixChanged = b.Dirty] = "UpOnColorMatrixChanged";
      b[b.DownOnColorMatrixChanged = b.InvalidConcatenatedColorMatrix] = "DownOnColorMatrixChanged";
      b[b.Visible = 65536] = "Visible";
      b[b.UpOnInvalidate = b.InvalidBounds | b.Dirty] = "UpOnInvalidate";
      b[b.Default = b.BoundsAutoCompute | b.InvalidBounds | b.InvalidConcatenatedMatrix | b.InvalidInvertedConcatenatedMatrix | b.Visible] = "Default";
      b[b.CacheAsBitmap = 131072] = "CacheAsBitmap";
      b[b.PixelSnapping = 262144] = "PixelSnapping";
      b[b.ImageSmoothing = 524288] = "ImageSmoothing";
      b[b.Dynamic = 1048576] = "Dynamic";
      b[b.Scalable = 2097152] = "Scalable";
      b[b.Tileable = 4194304] = "Tileable";
      b[b.Transparent = 32768] = "Transparent";
    })(r.NodeFlags || (r.NodeFlags = {}));
    var a = r.NodeFlags;
    (function(b) {
      b[b.Node = 1] = "Node";
      b[b.Shape = 3] = "Shape";
      b[b.Group = 5] = "Group";
      b[b.Stage = 13] = "Stage";
      b[b.Renderable = 33] = "Renderable";
    })(r.NodeType || (r.NodeType = {}));
    var h = r.NodeType;
    (function(b) {
      b[b.None = 0] = "None";
      b[b.OnStageBoundsChanged = 1] = "OnStageBoundsChanged";
      b[b.RemovedFromStage = 2] = "RemovedFromStage";
    })(r.NodeEventType || (r.NodeEventType = {}));
    var p = function() {
      function b() {
      }
      b.prototype.visitNode = function(b, a) {
      };
      b.prototype.visitShape = function(b, a) {
        this.visitNode(b, a);
      };
      b.prototype.visitGroup = function(b, a) {
        this.visitNode(b, a);
        for (var d = b.getChildren(), c = 0;c < d.length;c++) {
          d[c].visit(this, a);
        }
      };
      b.prototype.visitStage = function(b, a) {
        this.visitGroup(b, a);
      };
      b.prototype.visitRenderable = function(b, a) {
        this.visitNode(b, a);
      };
      return b;
    }();
    r.NodeVisitor = p;
    var l = function() {
      return function() {
      };
    }();
    r.State = l;
    var u = function(b) {
      function a() {
        b.call(this);
        this.matrix = c.createIdentity();
        this.depth = 0;
      }
      __extends(a, b);
      a.prototype.transform = function(b) {
        var a = this.clone();
        a.matrix.preMultiply(b.getMatrix());
        return a;
      };
      a.allocate = function() {
        var b = a._dirtyStack, d = null;
        b.length && (d = b.pop());
        return d;
      };
      a.prototype.clone = function() {
        var b = a.allocate();
        b || (b = new a);
        b.set(this);
        return b;
      };
      a.prototype.set = function(b) {
        this.matrix.set(b.matrix);
      };
      a.prototype.free = function() {
        a._dirtyStack.push(this);
      };
      a._dirtyStack = [];
      return a;
    }(l);
    r.PreRenderState = u;
    var m = function(b) {
      function a() {
        b.apply(this, arguments);
        this.isDirty = !0;
      }
      __extends(a, b);
      a.prototype.start = function(b, a) {
        this._dirtyRegion = a;
        var d = new u;
        d.matrix.setIdentity();
        b.visit(this, d);
        d.free();
      };
      a.prototype.visitGroup = function(b, a) {
        var d = b.getChildren();
        this.visitNode(b, a);
        for (var f = 0;f < d.length;f++) {
          var c = d[f], e = a.transform(c.getTransform());
          c.visit(this, e);
          e.free();
        }
      };
      a.prototype.visitNode = function(b, a) {
        b.hasFlags(16) && (this.isDirty = !0);
        b.toggleFlags(16, !1);
        b.depth = a.depth++;
      };
      return a;
    }(p);
    r.PreRenderVisitor = m;
    l = function(b) {
      function a(d) {
        b.call(this);
        this.writer = d;
      }
      __extends(a, b);
      a.prototype.visitNode = function(b, a) {
      };
      a.prototype.visitShape = function(b, a) {
        this.writer.writeLn(b.toString());
        this.visitNode(b, a);
      };
      a.prototype.visitGroup = function(b, a) {
        this.visitNode(b, a);
        var d = b.getChildren();
        this.writer.enter(b.toString() + " " + d.length);
        for (var f = 0;f < d.length;f++) {
          d[f].visit(this, a);
        }
        this.writer.outdent();
      };
      a.prototype.visitStage = function(b, a) {
        this.visitGroup(b, a);
      };
      return a;
    }(p);
    r.TracingNodeVisitor = l;
    var s = function() {
      function b() {
        this._clip = -1;
        this._eventListeners = null;
        this._id = b._nextId++;
        this._type = 1;
        this._index = -1;
        this._parent = null;
        this.reset();
      }
      Object.defineProperty(b.prototype, "id", {get:function() {
        return this._id;
      }, enumerable:!0, configurable:!0});
      b.prototype._dispatchEvent = function(b) {
        if (this._eventListeners) {
          for (var a = this._eventListeners, d = 0;d < a.length;d++) {
            var c = a[d];
            c.type === b && c.listener(this, b);
          }
        }
      };
      b.prototype.addEventListener = function(b, a) {
        this._eventListeners || (this._eventListeners = []);
        this._eventListeners.push({type:b, listener:a});
      };
      b.prototype.removeEventListener = function(b, a) {
        for (var d = this._eventListeners, c = 0;c < d.length;c++) {
          var e = d[c];
          if (e.type === b && e.listener === a) {
            d.splice(c, 1);
            break;
          }
        }
      };
      Object.defineProperty(b.prototype, "properties", {get:function() {
        return this._properties || (this._properties = {});
      }, enumerable:!0, configurable:!0});
      b.prototype.reset = function() {
        this._flags = a.Default;
        this._properties = this._transform = this._layer = this._bounds = null;
        this.depth = -1;
      };
      Object.defineProperty(b.prototype, "clip", {get:function() {
        return this._clip;
      }, set:function(b) {
        this._clip = b;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "parent", {get:function() {
        return this._parent;
      }, enumerable:!0, configurable:!0});
      b.prototype.getTransformedBounds = function(b) {
        var a = this.getBounds(!0);
        if (b !== this && !a.isEmpty()) {
          var d = this.getTransform().getConcatenatedMatrix();
          b ? (b = b.getTransform().getInvertedConcatenatedMatrix(), b.preMultiply(d), b.transformRectangleAABB(a), b.free()) : d.transformRectangleAABB(a);
        }
        return a;
      };
      b.prototype._markCurrentBoundsAsDirtyRegion = function() {
      };
      b.prototype.getStage = function(b) {
        void 0 === b && (b = !0);
        for (var a = this._parent;a;) {
          if (a.isType(13)) {
            var d = a;
            if (b) {
              if (d.dirtyRegion) {
                return d;
              }
            } else {
              return d;
            }
          }
          a = a._parent;
        }
        return null;
      };
      b.prototype.getChildren = function(b) {
        throw void 0;
      };
      b.prototype.getBounds = function(b) {
        throw void 0;
      };
      b.prototype.setBounds = function(b) {
        (this._bounds || (this._bounds = g.createEmpty())).set(b);
        this.removeFlags(256);
      };
      b.prototype.clone = function() {
        throw void 0;
      };
      b.prototype.setFlags = function(b) {
        this._flags |= b;
      };
      b.prototype.hasFlags = function(b) {
        return(this._flags & b) === b;
      };
      b.prototype.hasAnyFlags = function(b) {
        return!!(this._flags & b);
      };
      b.prototype.removeFlags = function(b) {
        this._flags &= ~b;
      };
      b.prototype.toggleFlags = function(b, a) {
        this._flags = a ? this._flags | b : this._flags & ~b;
      };
      b.prototype._propagateFlagsUp = function(b) {
        if (0 !== b && !this.hasFlags(b)) {
          this.hasFlags(2) || (b &= -257);
          this.setFlags(b);
          var a = this._parent;
          a && a._propagateFlagsUp(b);
        }
      };
      b.prototype._propagateFlagsDown = function(b) {
        throw void 0;
      };
      b.prototype.isAncestor = function(b) {
        for (;b;) {
          if (b === this) {
            return!0;
          }
          b = b._parent;
        }
        return!1;
      };
      b._getAncestors = function(a, d) {
        var c = b._path;
        for (c.length = 0;a && a !== d;) {
          c.push(a), a = a._parent;
        }
        return c;
      };
      b.prototype._findClosestAncestor = function() {
        for (var b = this;b;) {
          if (!1 === b.hasFlags(512)) {
            return b;
          }
          b = b._parent;
        }
        return null;
      };
      b.prototype.isType = function(b) {
        return this._type === b;
      };
      b.prototype.isTypeOf = function(b) {
        return(this._type & b) === b;
      };
      b.prototype.isLeaf = function() {
        return this.isType(33) || this.isType(3);
      };
      b.prototype.isLinear = function() {
        if (this.isLeaf()) {
          return!0;
        }
        if (this.isType(5)) {
          var b = this._children;
          if (1 === b.length && b[0].isLinear()) {
            return!0;
          }
        }
        return!1;
      };
      b.prototype.getTransformMatrix = function() {
        var b;
        void 0 === b && (b = !1);
        return this.getTransform().getMatrix(b);
      };
      b.prototype.getTransform = function() {
        null === this._transform && (this._transform = new d(this));
        return this._transform;
      };
      b.prototype.getLayer = function() {
        null === this._layer && (this._layer = new e(this));
        return this._layer;
      };
      b.prototype.visit = function(b, a) {
        switch(this._type) {
          case 1:
            b.visitNode(this, a);
            break;
          case 5:
            b.visitGroup(this, a);
            break;
          case 13:
            b.visitStage(this, a);
            break;
          case 3:
            b.visitShape(this, a);
            break;
          case 33:
            b.visitRenderable(this, a);
            break;
          default:
            k.Debug.unexpectedCase();
        }
      };
      b.prototype.invalidate = function() {
        this._propagateFlagsUp(a.UpOnInvalidate);
      };
      b.prototype.toString = function(b) {
        void 0 === b && (b = !1);
        var a = h[this._type] + " " + this._id;
        b && (a += " " + this._bounds.toString());
        return a;
      };
      b._path = [];
      b._nextId = 0;
      return b;
    }();
    r.Node = s;
    var v = function(b) {
      function d() {
        b.call(this);
        this._type = 5;
        this._children = [];
      }
      __extends(d, b);
      d.prototype.getChildren = function(b) {
        void 0 === b && (b = !1);
        return b ? this._children.slice(0) : this._children;
      };
      d.prototype.childAt = function(b) {
        return this._children[b];
      };
      Object.defineProperty(d.prototype, "child", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(d.prototype, "groupChild", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      d.prototype.addChild = function(b) {
        b._parent && b._parent.removeChildAt(b._index);
        b._parent = this;
        b._index = this._children.length;
        this._children.push(b);
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
        b._propagateFlagsDown(a.DownOnAddedOrRemoved);
      };
      d.prototype.removeChildAt = function(b) {
        var d = this._children[b];
        this._children.splice(b, 1);
        d._index = -1;
        d._parent = null;
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
        d._propagateFlagsDown(a.DownOnAddedOrRemoved);
      };
      d.prototype.clearChildren = function() {
        for (var b = 0;b < this._children.length;b++) {
          var d = this._children[b];
          d && (d._index = -1, d._parent = null, d._propagateFlagsDown(a.DownOnAddedOrRemoved));
        }
        this._children.length = 0;
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
      };
      d.prototype._propagateFlagsDown = function(b) {
        if (!this.hasFlags(b)) {
          this.setFlags(b);
          for (var a = this._children, d = 0;d < a.length;d++) {
            a[d]._propagateFlagsDown(b);
          }
        }
      };
      d.prototype.getBounds = function(b) {
        void 0 === b && (b = !1);
        var a = this._bounds || (this._bounds = g.createEmpty());
        if (this.hasFlags(256)) {
          a.setEmpty();
          for (var d = this._children, c = g.allocate(), f = 0;f < d.length;f++) {
            var e = d[f];
            c.set(e.getBounds());
            e.getTransformMatrix().transformRectangleAABB(c);
            a.union(c);
          }
          c.free();
          this.removeFlags(256);
        }
        return b ? a.clone() : a;
      };
      return d;
    }(s);
    r.Group = v;
    var d = function() {
      function b(b) {
        this._node = b;
        this._matrix = c.createIdentity();
        this._colorMatrix = r.ColorMatrix.createIdentity();
        this._concatenatedMatrix = c.createIdentity();
        this._invertedConcatenatedMatrix = c.createIdentity();
        this._concatenatedColorMatrix = r.ColorMatrix.createIdentity();
      }
      b.prototype.setMatrix = function(b) {
        this._matrix.isEqual(b) || (this._matrix.set(b), this._node._propagateFlagsUp(a.UpOnMoved), this._node._propagateFlagsDown(a.DownOnMoved));
      };
      b.prototype.setColorMatrix = function(b) {
        this._colorMatrix.set(b);
        this._node._propagateFlagsUp(a.UpOnColorMatrixChanged);
        this._node._propagateFlagsDown(a.DownOnColorMatrixChanged);
      };
      b.prototype.getMatrix = function(b) {
        void 0 === b && (b = !1);
        return b ? this._matrix.clone() : this._matrix;
      };
      b.prototype.hasColorMatrix = function() {
        return null !== this._colorMatrix;
      };
      b.prototype.getColorMatrix = function() {
        var b;
        void 0 === b && (b = !1);
        null === this._colorMatrix && (this._colorMatrix = r.ColorMatrix.createIdentity());
        return b ? this._colorMatrix.clone() : this._colorMatrix;
      };
      b.prototype.getConcatenatedMatrix = function(b) {
        void 0 === b && (b = !1);
        if (this._node.hasFlags(512)) {
          for (var a = this._node._findClosestAncestor(), d = s._getAncestors(this._node, a), e = a ? a.getTransform()._concatenatedMatrix.clone() : c.createIdentity(), h = d.length - 1;0 <= h;h--) {
            var a = d[h], m = a.getTransform();
            e.preMultiply(m._matrix);
            m._concatenatedMatrix.set(e);
            a.removeFlags(512);
          }
        }
        return b ? this._concatenatedMatrix.clone() : this._concatenatedMatrix;
      };
      b.prototype.getInvertedConcatenatedMatrix = function() {
        var b = !0;
        void 0 === b && (b = !1);
        this._node.hasFlags(1024) && (this.getConcatenatedMatrix().inverse(this._invertedConcatenatedMatrix), this._node.removeFlags(1024));
        return b ? this._invertedConcatenatedMatrix.clone() : this._invertedConcatenatedMatrix;
      };
      b.prototype.toString = function() {
        return this._matrix.toString();
      };
      return b;
    }();
    r.Transform = d;
    var e = function() {
      function b(b) {
        this._node = b;
        this._mask = null;
        this._blendMode = 1;
      }
      Object.defineProperty(b.prototype, "filters", {get:function() {
        return this._filters;
      }, set:function(b) {
        this._filters = b;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "blendMode", {get:function() {
        return this._blendMode;
      }, set:function(b) {
        this._blendMode = b;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "mask", {get:function() {
        return this._mask;
      }, set:function(b) {
        this._mask && this._mask !== b && this._mask.removeFlags(4);
        (this._mask = b) && this._mask.setFlags(4);
      }, enumerable:!0, configurable:!0});
      b.prototype.toString = function() {
        return n[this._blendMode];
      };
      return b;
    }();
    r.Layer = e;
    l = function(b) {
      function a(d) {
        b.call(this);
        this._source = d;
        this._type = 3;
        this.ratio = 0;
      }
      __extends(a, b);
      a.prototype.getBounds = function(b) {
        void 0 === b && (b = !1);
        var a = this._bounds || (this._bounds = g.createEmpty());
        this.hasFlags(256) && (a.set(this._source.getBounds()), this.removeFlags(256));
        return b ? a.clone() : a;
      };
      Object.defineProperty(a.prototype, "source", {get:function() {
        return this._source;
      }, enumerable:!0, configurable:!0});
      a.prototype._propagateFlagsDown = function(b) {
        this.setFlags(b);
      };
      a.prototype.getChildren = function(b) {
        return[this._source];
      };
      return a;
    }(s);
    r.Shape = l;
    r.RendererOptions = function() {
      return function() {
        this.debug = !1;
        this.paintRenderable = !0;
        this.paintViewport = this.paintFlashing = this.paintDirtyRegion = this.paintBounds = !1;
        this.clear = !0;
      };
    }();
    (function(b) {
      b[b.Canvas2D = 0] = "Canvas2D";
      b[b.WebGL = 1] = "WebGL";
      b[b.Both = 2] = "Both";
      b[b.DOM = 3] = "DOM";
      b[b.SVG = 4] = "SVG";
    })(r.Backend || (r.Backend = {}));
    p = function(b) {
      function a(d, c, f) {
        b.call(this);
        this._container = d;
        this._stage = c;
        this._options = f;
        this._viewport = g.createSquare();
        this._devicePixelRatio = 1;
      }
      __extends(a, b);
      Object.defineProperty(a.prototype, "viewport", {set:function(b) {
        this._viewport.set(b);
      }, enumerable:!0, configurable:!0});
      a.prototype.render = function() {
        throw void 0;
      };
      a.prototype.resize = function() {
        throw void 0;
      };
      a.prototype.screenShot = function(b, a) {
        throw void 0;
      };
      return a;
    }(p);
    r.Renderer = p;
    p = function(b) {
      function a(d, c, e) {
        void 0 === e && (e = !1);
        b.call(this);
        this._preVisitor = new m;
        this._flags &= -3;
        this._type = 13;
        this._scaleMode = a.DEFAULT_SCALE;
        this._align = a.DEFAULT_ALIGN;
        this._content = new v;
        this._content._flags &= -3;
        this.addChild(this._content);
        this.setFlags(16);
        this.setBounds(new g(0, 0, d, c));
        e ? (this._dirtyRegion = new t(d, c), this._dirtyRegion.addDirtyRectangle(new g(0, 0, d, c))) : this._dirtyRegion = null;
        this._updateContentMatrix();
      }
      __extends(a, b);
      Object.defineProperty(a.prototype, "dirtyRegion", {get:function() {
        return this._dirtyRegion;
      }, enumerable:!0, configurable:!0});
      a.prototype.setBounds = function(a) {
        b.prototype.setBounds.call(this, a);
        this._updateContentMatrix();
        this._dispatchEvent(1);
        this._dirtyRegion && (this._dirtyRegion = new t(a.w, a.h), this._dirtyRegion.addDirtyRectangle(a));
      };
      Object.defineProperty(a.prototype, "content", {get:function() {
        return this._content;
      }, enumerable:!0, configurable:!0});
      a.prototype.readyToRender = function() {
        this._preVisitor.isDirty = !1;
        this._preVisitor.start(this, this._dirtyRegion);
        return this._preVisitor.isDirty ? !0 : !1;
      };
      Object.defineProperty(a.prototype, "align", {get:function() {
        return this._align;
      }, set:function(b) {
        this._align = b;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "scaleMode", {get:function() {
        return this._scaleMode;
      }, set:function(b) {
        this._scaleMode = b;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      a.prototype._updateContentMatrix = function() {
        if (this._scaleMode === a.DEFAULT_SCALE && this._align === a.DEFAULT_ALIGN) {
          this._content.getTransform().setMatrix(new c(1, 0, 0, 1, 0, 0));
        } else {
          var b = this.getBounds(), d = this._content.getBounds(), e = b.w / d.w, h = b.h / d.h;
          switch(this._scaleMode) {
            case 2:
              e = h = Math.max(e, h);
              break;
            case 4:
              e = h = 1;
              break;
            case 1:
              break;
            default:
              e = h = Math.min(e, h);
          }
          var m;
          m = this._align & 4 ? 0 : this._align & 8 ? b.w - d.w * e : (b.w - d.w * e) / 2;
          b = this._align & 1 ? 0 : this._align & 2 ? b.h - d.h * h : (b.h - d.h * h) / 2;
          this._content.getTransform().setMatrix(new c(e, 0, 0, h, m, b));
        }
      };
      a.DEFAULT_SCALE = 4;
      a.DEFAULT_ALIGN = 5;
      return a;
    }(v);
    r.Stage = p;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    function g(a, b, d) {
      return a + (b - a) * d;
    }
    function c(a, b, d) {
      return g(a >> 24 & 255, b >> 24 & 255, d) << 24 | g(a >> 16 & 255, b >> 16 & 255, d) << 16 | g(a >> 8 & 255, b >> 8 & 255, d) << 8 | g(a & 255, b & 255, d);
    }
    var t = r.Geometry.Point, n = r.Geometry.Rectangle, a = r.Geometry.Matrix, h = k.ArrayUtilities.indexOf, p = function(a) {
      function b() {
        a.call(this);
        this._parents = [];
        this._renderableParents = [];
        this._invalidateEventListeners = null;
        this._flags &= -3;
        this._type = 33;
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "parents", {get:function() {
        return this._parents;
      }, enumerable:!0, configurable:!0});
      b.prototype.addParent = function(b) {
        h(this._parents, b);
        this._parents.push(b);
      };
      b.prototype.willRender = function() {
        for (var b = this._parents, a = 0;a < b.length;a++) {
          for (var d = b[a];d;) {
            if (d.isType(13)) {
              return!0;
            }
            if (!d.hasFlags(65536)) {
              break;
            }
            d = d._parent;
          }
        }
        return!1;
      };
      b.prototype.addRenderableParent = function(b) {
        h(this._renderableParents, b);
        this._renderableParents.push(b);
      };
      b.prototype.wrap = function() {
        for (var b, a = this._parents, d = 0;d < a.length;d++) {
          if (b = a[d], !b._parent) {
            return b;
          }
        }
        b = new r.Shape(this);
        this.addParent(b);
        return b;
      };
      b.prototype.invalidate = function() {
        this.setFlags(16);
        for (var b = this._parents, a = 0;a < b.length;a++) {
          b[a].invalidate();
        }
        b = this._renderableParents;
        for (a = 0;a < b.length;a++) {
          b[a].invalidate();
        }
        if (b = this._invalidateEventListeners) {
          for (a = 0;a < b.length;a++) {
            b[a](this);
          }
        }
      };
      b.prototype.addInvalidateEventListener = function(b) {
        this._invalidateEventListeners || (this._invalidateEventListeners = []);
        h(this._invalidateEventListeners, b);
        this._invalidateEventListeners.push(b);
      };
      b.prototype.getBounds = function(b) {
        void 0 === b && (b = !1);
        return b ? this._bounds.clone() : this._bounds;
      };
      b.prototype.getChildren = function(b) {
        return null;
      };
      b.prototype._propagateFlagsUp = function(b) {
        if (0 !== b && !this.hasFlags(b)) {
          for (var a = 0;a < this._parents.length;a++) {
            this._parents[a]._propagateFlagsUp(b);
          }
        }
      };
      b.prototype.render = function(b, a, d, c, e) {
      };
      return b;
    }(r.Node);
    r.Renderable = p;
    var l = function(a) {
      function b(b, d) {
        a.call(this);
        this.setBounds(b);
        this.render = d;
      }
      __extends(b, a);
      return b;
    }(p);
    r.CustomRenderable = l;
    (function(a) {
      a[a.Idle = 1] = "Idle";
      a[a.Playing = 2] = "Playing";
      a[a.Paused = 3] = "Paused";
      a[a.Ended = 4] = "Ended";
    })(r.RenderableVideoState || (r.RenderableVideoState = {}));
    l = function(a) {
      function b(d, c) {
        a.call(this);
        this._flags = 1048592;
        this._lastPausedTime = this._lastTimeInvalidated = 0;
        this._pauseHappening = this._seekHappening = !1;
        this._isDOMElement = !0;
        this.setBounds(new n(0, 0, 1, 1));
        this._assetId = d;
        this._eventSerializer = c;
        var h = document.createElement("video"), m = this._handleVideoEvent.bind(this);
        h.preload = "metadata";
        h.addEventListener("play", m);
        h.addEventListener("pause", m);
        h.addEventListener("ended", m);
        h.addEventListener("loadeddata", m);
        h.addEventListener("progress", m);
        h.addEventListener("suspend", m);
        h.addEventListener("loadedmetadata", m);
        h.addEventListener("error", m);
        h.addEventListener("seeking", m);
        h.addEventListener("seeked", m);
        h.addEventListener("canplay", m);
        h.style.position = "absolute";
        this._video = h;
        this._videoEventHandler = m;
        b._renderableVideos.push(this);
        "undefined" !== typeof registerInspectorAsset && registerInspectorAsset(-1, -1, this);
        this._state = 1;
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "video", {get:function() {
        return this._video;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "state", {get:function() {
        return this._state;
      }, enumerable:!0, configurable:!0});
      b.prototype.play = function() {
        this._state = 2;
        this._video.play();
      };
      b.prototype.pause = function() {
        this._state = 3;
        this._video.pause();
      };
      b.prototype._handleVideoEvent = function(b) {
        var a = null, d = this._video;
        switch(b.type) {
          case "play":
            if (!this._pauseHappening) {
              return;
            }
            b = 7;
            break;
          case "pause":
            if (2 === this._state) {
              d.play();
              return;
            }
            b = 6;
            this._pauseHappening = !0;
            break;
          case "ended":
            this._state = 4;
            this._notifyNetStream(3, a);
            b = 4;
            break;
          case "loadeddata":
            this._pauseHappening = !1;
            this._notifyNetStream(2, a);
            this.play();
            return;
          case "canplay":
            if (this._pauseHappening) {
              return;
            }
            b = 5;
            break;
          case "progress":
            b = 10;
            break;
          case "suspend":
            return;
          case "loadedmetadata":
            b = 1;
            a = {videoWidth:d.videoWidth, videoHeight:d.videoHeight, duration:d.duration};
            break;
          case "error":
            b = 11;
            a = {code:d.error.code};
            break;
          case "seeking":
            if (!this._seekHappening) {
              return;
            }
            b = 8;
            break;
          case "seeked":
            if (!this._seekHappening) {
              return;
            }
            b = 9;
            this._seekHappening = !1;
            break;
          default:
            return;
        }
        this._notifyNetStream(b, a);
      };
      b.prototype._notifyNetStream = function(b, a) {
        this._eventSerializer.sendVideoPlaybackEvent(this._assetId, b, a);
      };
      b.prototype.processControlRequest = function(b, a) {
        var d = this._video;
        switch(b) {
          case 1:
            d.src = a.url;
            this.play();
            this._notifyNetStream(0, null);
            break;
          case 9:
            d.paused && d.play();
            break;
          case 2:
            d && (a.paused && !d.paused ? (isNaN(a.time) ? this._lastPausedTime = d.currentTime : (0 !== d.seekable.length && (d.currentTime = a.time), this._lastPausedTime = a.time), this.pause()) : !a.paused && d.paused && (this.play(), isNaN(a.time) || this._lastPausedTime === a.time || 0 === d.seekable.length || (d.currentTime = a.time)));
            break;
          case 3:
            d && 0 !== d.seekable.length && (this._seekHappening = !0, d.currentTime = a.time);
            break;
          case 4:
            return d ? d.currentTime : 0;
          case 5:
            return d ? d.duration : 0;
          case 6:
            d && (d.volume = a.volume);
            break;
          case 7:
            if (!d) {
              return 0;
            }
            var c = -1;
            if (d.buffered) {
              for (var e = 0;e < d.buffered.length;e++) {
                c = Math.max(c, d.buffered.end(e));
              }
            } else {
              c = d.duration;
            }
            return Math.round(500 * c);
          case 8:
            return d ? Math.round(500 * d.duration) : 0;
        }
      };
      b.prototype.checkForUpdate = function() {
        this._lastTimeInvalidated !== this._video.currentTime && (this._isDOMElement || this.invalidate());
        this._lastTimeInvalidated = this._video.currentTime;
      };
      b.checkForVideoUpdates = function() {
        for (var a = b._renderableVideos, d = 0;d < a.length;d++) {
          var c = a[d];
          c.willRender() ? (c._video.parentElement || c.invalidate(), c._video.style.zIndex = c.parents[0].depth + "") : c._video.parentElement && c._dispatchEvent(2);
          a[d].checkForUpdate();
        }
      };
      b.prototype.render = function(b, a, d) {
        (a = this._video) && 0 < a.videoWidth && b.drawImage(a, 0, 0, a.videoWidth, a.videoHeight, 0, 0, this._bounds.w, this._bounds.h);
      };
      b._renderableVideos = [];
      return b;
    }(p);
    r.RenderableVideo = l;
    l = function(a) {
      function b(b, d) {
        a.call(this);
        this._flags = 1048592;
        this.properties = {};
        this.setBounds(d);
        b instanceof HTMLCanvasElement ? this._initializeSourceCanvas(b) : this._sourceImage = b;
      }
      __extends(b, a);
      b.FromDataBuffer = function(a, d, c) {
        var e = document.createElement("canvas");
        e.width = c.w;
        e.height = c.h;
        c = new b(e, c);
        c.updateFromDataBuffer(a, d);
        return c;
      };
      b.FromNode = function(a, d, c, e, h) {
        var m = document.createElement("canvas"), g = a.getBounds();
        m.width = g.w;
        m.height = g.h;
        m = new b(m, g);
        m.drawNode(a, d, c, e, h);
        return m;
      };
      b.FromImage = function(a) {
        return new b(a, new n(0, 0, -1, -1));
      };
      b.prototype.updateFromDataBuffer = function(b, a) {
        if (r.imageUpdateOption.value) {
          var d = a.buffer;
          if (4 !== b && 5 !== b && 6 !== b) {
            var c = this._bounds, e = this._imageData;
            e && e.width === c.w && e.height === c.h || (e = this._imageData = this._context.createImageData(c.w, c.h));
            r.imageConvertOption.value && (d = new Int32Array(d), c = new Int32Array(e.data.buffer), k.ColorUtilities.convertImage(b, 3, d, c));
            this._ensureSourceCanvas();
            this._context.putImageData(e, 0, 0);
          }
          this.invalidate();
        }
      };
      b.prototype.readImageData = function(b) {
        b.writeRawBytes(this.imageData.data);
      };
      b.prototype.render = function(b, a, d) {
        this.renderSource ? b.drawImage(this.renderSource, 0, 0) : this._renderFallback(b);
      };
      b.prototype.drawNode = function(b, a, d, c, e) {
        d = r.Canvas2D;
        c = this.getBounds();
        (new d.Canvas2DRenderer(this._canvas, null)).renderNode(b, e || c, a);
      };
      b.prototype._initializeSourceCanvas = function(b) {
        this._canvas = b;
        this._context = this._canvas.getContext("2d");
      };
      b.prototype._ensureSourceCanvas = function() {
        if (!this._canvas) {
          var b = document.createElement("canvas"), a = this._bounds;
          b.width = a.w;
          b.height = a.h;
          this._initializeSourceCanvas(b);
        }
      };
      Object.defineProperty(b.prototype, "imageData", {get:function() {
        this._canvas || (this._ensureSourceCanvas(), this._context.drawImage(this._sourceImage, 0, 0), this._sourceImage = null);
        return this._context.getImageData(0, 0, this._bounds.w, this._bounds.h);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "renderSource", {get:function() {
        return this._canvas || this._sourceImage;
      }, enumerable:!0, configurable:!0});
      b.prototype._renderFallback = function(b) {
      };
      return b;
    }(p);
    r.RenderableBitmap = l;
    (function(a) {
      a[a.Fill = 0] = "Fill";
      a[a.Stroke = 1] = "Stroke";
      a[a.StrokeFill = 2] = "StrokeFill";
    })(r.PathType || (r.PathType = {}));
    var u = function() {
      return function(a, b, d, c) {
        this.type = a;
        this.style = b;
        this.smoothImage = d;
        this.strokeProperties = c;
        this.path = new Path2D;
      };
    }();
    r.StyledPath = u;
    var m = function() {
      return function(a, b, d, c, h) {
        this.thickness = a;
        this.scaleMode = b;
        this.capsStyle = d;
        this.jointsStyle = c;
        this.miterLimit = h;
      };
    }();
    r.StrokeProperties = m;
    var s = function(d) {
      function b(b, a, c, h) {
        d.call(this);
        this._flags = 6291472;
        this.properties = {};
        this.setBounds(h);
        this._id = b;
        this._pathData = a;
        this._textures = c;
        c.length && this.setFlags(1048576);
      }
      __extends(b, d);
      b.prototype.update = function(b, a, d) {
        this.setBounds(d);
        this._pathData = b;
        this._paths = null;
        this._textures = a;
        this.setFlags(1048576);
        this.invalidate();
      };
      b.prototype.render = function(b, a, d, c, e) {
        void 0 === c && (c = !1);
        void 0 === e && (e = !1);
        b.fillStyle = b.strokeStyle = "transparent";
        a = this._deserializePaths(this._pathData, b, a);
        for (d = 0;d < a.length;d++) {
          var h = a[d];
          b.mozImageSmoothingEnabled = b.msImageSmoothingEnabled = b.imageSmoothingEnabled = h.smoothImage;
          if (0 === h.type) {
            c ? b.clip(h.path, "evenodd") : (b.fillStyle = e ? "#000000" : h.style, b.fill(h.path, "evenodd"), b.fillStyle = "transparent");
          } else {
            if (!c && !e) {
              b.strokeStyle = h.style;
              var m = 1;
              h.strokeProperties && (m = h.strokeProperties.scaleMode, b.lineWidth = h.strokeProperties.thickness, b.lineCap = h.strokeProperties.capsStyle, b.lineJoin = h.strokeProperties.jointsStyle, b.miterLimit = h.strokeProperties.miterLimit);
              var g = b.lineWidth;
              (g = 1 === g || 3 === g) && b.translate(.5, .5);
              b.flashStroke(h.path, m);
              g && b.translate(-.5, -.5);
              b.strokeStyle = "transparent";
            }
          }
        }
      };
      b.prototype._deserializePaths = function(a, d, c) {
        if (this._paths) {
          return this._paths;
        }
        c = this._paths = [];
        var e = null, h = null, g = 0, l = 0, p, s, n = !1, u = 0, v = 0, r = a.commands, t = a.coordinates, C = a.styles, y = C.position = 0;
        a = a.commandsPosition;
        for (var z = 0;z < a;z++) {
          switch(r[z]) {
            case 9:
              n && e && (e.lineTo(u, v), h && h.lineTo(u, v));
              n = !0;
              g = u = t[y++] / 20;
              l = v = t[y++] / 20;
              e && e.moveTo(g, l);
              h && h.moveTo(g, l);
              break;
            case 10:
              g = t[y++] / 20;
              l = t[y++] / 20;
              e && e.lineTo(g, l);
              h && h.lineTo(g, l);
              break;
            case 11:
              p = t[y++] / 20;
              s = t[y++] / 20;
              g = t[y++] / 20;
              l = t[y++] / 20;
              e && e.quadraticCurveTo(p, s, g, l);
              h && h.quadraticCurveTo(p, s, g, l);
              break;
            case 12:
              p = t[y++] / 20;
              s = t[y++] / 20;
              var B = t[y++] / 20, x = t[y++] / 20, g = t[y++] / 20, l = t[y++] / 20;
              e && e.bezierCurveTo(p, s, B, x, g, l);
              h && h.bezierCurveTo(p, s, B, x, g, l);
              break;
            case 1:
              e = this._createPath(0, k.ColorUtilities.rgbaToCSSStyle(C.readUnsignedInt()), !1, null, g, l);
              break;
            case 3:
              p = this._readBitmap(C, d);
              e = this._createPath(0, p.style, p.smoothImage, null, g, l);
              break;
            case 2:
              e = this._createPath(0, this._readGradient(C, d), !1, null, g, l);
              break;
            case 4:
              e = null;
              break;
            case 5:
              h = k.ColorUtilities.rgbaToCSSStyle(C.readUnsignedInt());
              C.position += 1;
              p = C.readByte();
              s = b.LINE_CAPS_STYLES[C.readByte()];
              B = b.LINE_JOINTS_STYLES[C.readByte()];
              p = new m(t[y++] / 20, p, s, B, C.readByte());
              h = this._createPath(1, h, !1, p, g, l);
              break;
            case 6:
              h = this._createPath(2, this._readGradient(C, d), !1, null, g, l);
              break;
            case 7:
              p = this._readBitmap(C, d);
              h = this._createPath(2, p.style, p.smoothImage, null, g, l);
              break;
            case 8:
              h = null;
          }
        }
        n && e && (e.lineTo(u, v), h && h.lineTo(u, v));
        this._pathData = null;
        return c;
      };
      b.prototype._createPath = function(b, a, d, c, e, h) {
        b = new u(b, a, d, c);
        this._paths.push(b);
        b.path.moveTo(e, h);
        return b.path;
      };
      b.prototype._readMatrix = function(b) {
        return new a(b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat());
      };
      b.prototype._readGradient = function(b, a) {
        var d = b.readUnsignedByte(), c = 2 * b.readShort() / 255, e = this._readMatrix(b), d = 16 === d ? a.createLinearGradient(-1, 0, 1, 0) : a.createRadialGradient(c, 0, 0, 0, 0, 1);
        d.setTransform && d.setTransform(e.toSVGMatrix());
        e = b.readUnsignedByte();
        for (c = 0;c < e;c++) {
          var h = b.readUnsignedByte() / 255, m = k.ColorUtilities.rgbaToCSSStyle(b.readUnsignedInt());
          d.addColorStop(h, m);
        }
        b.position += 2;
        return d;
      };
      b.prototype._readBitmap = function(b, a) {
        var d = b.readUnsignedInt(), c = this._readMatrix(b), e = b.readBoolean() ? "repeat" : "no-repeat", h = b.readBoolean();
        (d = this._textures[d]) ? (e = a.createPattern(d.renderSource, e), e.setTransform(c.toSVGMatrix())) : e = null;
        return{style:e, smoothImage:h};
      };
      b.prototype._renderFallback = function(b) {
        this.fillStyle || (this.fillStyle = k.ColorStyle.randomStyle());
        var a = this._bounds;
        b.save();
        b.beginPath();
        b.lineWidth = 2;
        b.fillStyle = this.fillStyle;
        b.fillRect(a.x, a.y, a.w, a.h);
        b.restore();
      };
      b.LINE_CAPS_STYLES = ["round", "butt", "square"];
      b.LINE_JOINTS_STYLES = ["round", "bevel", "miter"];
      return b;
    }(p);
    r.RenderableShape = s;
    l = function(d) {
      function b() {
        d.apply(this, arguments);
        this._flags = 7340048;
        this._morphPaths = Object.create(null);
      }
      __extends(b, d);
      b.prototype._deserializePaths = function(b, a, d) {
        if (this._morphPaths[d]) {
          return this._morphPaths[d];
        }
        var e = this._morphPaths[d] = [], h = null, l = null, p = 0, n = 0, u, v, r = !1, t = 0, P = 0, H = b.commands, C = b.coordinates, y = b.morphCoordinates, z = b.styles, B = b.morphStyles;
        z.position = 0;
        var x = B.position = 0;
        b = b.commandsPosition;
        for (var E = 0;E < b;E++) {
          switch(H[E]) {
            case 9:
              r && h && (h.lineTo(t, P), l && l.lineTo(t, P));
              r = !0;
              p = t = g(C[x], y[x++], d) / 20;
              n = P = g(C[x], y[x++], d) / 20;
              h && h.moveTo(p, n);
              l && l.moveTo(p, n);
              break;
            case 10:
              p = g(C[x], y[x++], d) / 20;
              n = g(C[x], y[x++], d) / 20;
              h && h.lineTo(p, n);
              l && l.lineTo(p, n);
              break;
            case 11:
              u = g(C[x], y[x++], d) / 20;
              v = g(C[x], y[x++], d) / 20;
              p = g(C[x], y[x++], d) / 20;
              n = g(C[x], y[x++], d) / 20;
              h && h.quadraticCurveTo(u, v, p, n);
              l && l.quadraticCurveTo(u, v, p, n);
              break;
            case 12:
              u = g(C[x], y[x++], d) / 20;
              v = g(C[x], y[x++], d) / 20;
              var A = g(C[x], y[x++], d) / 20, D = g(C[x], y[x++], d) / 20, p = g(C[x], y[x++], d) / 20, n = g(C[x], y[x++], d) / 20;
              h && h.bezierCurveTo(u, v, A, D, p, n);
              l && l.bezierCurveTo(u, v, A, D, p, n);
              break;
            case 1:
              h = this._createMorphPath(0, d, k.ColorUtilities.rgbaToCSSStyle(c(z.readUnsignedInt(), B.readUnsignedInt(), d)), !1, null, p, n);
              break;
            case 3:
              u = this._readMorphBitmap(z, B, d, a);
              h = this._createMorphPath(0, d, u.style, u.smoothImage, null, p, n);
              break;
            case 2:
              u = this._readMorphGradient(z, B, d, a);
              h = this._createMorphPath(0, d, u, !1, null, p, n);
              break;
            case 4:
              h = null;
              break;
            case 5:
              u = g(C[x], y[x++], d) / 20;
              l = k.ColorUtilities.rgbaToCSSStyle(c(z.readUnsignedInt(), B.readUnsignedInt(), d));
              z.position += 1;
              v = z.readByte();
              A = s.LINE_CAPS_STYLES[z.readByte()];
              D = s.LINE_JOINTS_STYLES[z.readByte()];
              u = new m(u, v, A, D, z.readByte());
              l = this._createMorphPath(1, d, l, !1, u, p, n);
              break;
            case 6:
              u = this._readMorphGradient(z, B, d, a);
              l = this._createMorphPath(2, d, u, !1, null, p, n);
              break;
            case 7:
              u = this._readMorphBitmap(z, B, d, a);
              l = this._createMorphPath(2, d, u.style, u.smoothImage, null, p, n);
              break;
            case 8:
              l = null;
          }
        }
        r && h && (h.lineTo(t, P), l && l.lineTo(t, P));
        return e;
      };
      b.prototype._createMorphPath = function(b, a, d, c, e, h, m) {
        b = new u(b, d, c, e);
        this._morphPaths[a].push(b);
        b.path.moveTo(h, m);
        return b.path;
      };
      b.prototype._readMorphMatrix = function(b, d, c) {
        return new a(g(b.readFloat(), d.readFloat(), c), g(b.readFloat(), d.readFloat(), c), g(b.readFloat(), d.readFloat(), c), g(b.readFloat(), d.readFloat(), c), g(b.readFloat(), d.readFloat(), c), g(b.readFloat(), d.readFloat(), c));
      };
      b.prototype._readMorphGradient = function(b, a, d, e) {
        var h = b.readUnsignedByte(), m = 2 * b.readShort() / 255, l = this._readMorphMatrix(b, a, d);
        e = 16 === h ? e.createLinearGradient(-1, 0, 1, 0) : e.createRadialGradient(m, 0, 0, 0, 0, 1);
        e.setTransform && e.setTransform(l.toSVGMatrix());
        l = b.readUnsignedByte();
        for (h = 0;h < l;h++) {
          var m = g(b.readUnsignedByte() / 255, a.readUnsignedByte() / 255, d), p = c(b.readUnsignedInt(), a.readUnsignedInt(), d), p = k.ColorUtilities.rgbaToCSSStyle(p);
          e.addColorStop(m, p);
        }
        b.position += 2;
        return e;
      };
      b.prototype._readMorphBitmap = function(b, a, d, c) {
        var e = b.readUnsignedInt();
        a = this._readMorphMatrix(b, a, d);
        d = b.readBoolean() ? "repeat" : "no-repeat";
        b = b.readBoolean();
        c = c.createPattern(this._textures[e]._canvas, d);
        c.setTransform(a.toSVGMatrix());
        return{style:c, smoothImage:b};
      };
      return b;
    }(s);
    r.RenderableMorphShape = l;
    var v = function() {
      function a() {
        this.align = this.leading = this.descent = this.ascent = this.width = this.y = this.x = 0;
        this.runs = [];
      }
      a._getMeasureContext = function() {
        a._measureContext || (a._measureContext = document.createElement("canvas").getContext("2d"));
        return a._measureContext;
      };
      a.prototype.addRun = function(b, c, h, m) {
        if (h) {
          var g = a._getMeasureContext();
          g.font = b;
          g = g.measureText(h).width | 0;
          this.runs.push(new d(b, c, h, g, m));
          this.width += g;
        }
      };
      a.prototype.wrap = function(b) {
        var c = [this], h = this.runs, m = this;
        m.width = 0;
        m.runs = [];
        for (var g = a._getMeasureContext(), l = 0;l < h.length;l++) {
          var p = h[l], k = p.text;
          p.text = "";
          p.width = 0;
          g.font = p.font;
          for (var s = b, n = k.split(/[\s.-]/), u = 0, v = 0;v < n.length;v++) {
            var r = n[v], t = k.substr(u, r.length + 1), H = g.measureText(t).width | 0;
            if (H > s) {
              do {
                if (p.text && (m.runs.push(p), m.width += p.width, p = new d(p.font, p.fillStyle, "", 0, p.underline), s = new a, s.y = m.y + m.descent + m.leading + m.ascent | 0, s.ascent = m.ascent, s.descent = m.descent, s.leading = m.leading, s.align = m.align, c.push(s), m = s), s = b - H, 0 > s) {
                  var H = t.length, C, y;
                  do {
                    H--;
                    if (1 > H) {
                      throw Error("Shall never happen: bad maxWidth?");
                    }
                    C = t.substr(0, H);
                    y = g.measureText(C).width | 0;
                  } while (y > b);
                  p.text = C;
                  p.width = y;
                  t = t.substr(H);
                  H = g.measureText(t).width | 0;
                }
              } while (0 > s);
            } else {
              s -= H;
            }
            p.text += t;
            p.width += H;
            u += r.length + 1;
          }
          m.runs.push(p);
          m.width += p.width;
        }
        return c;
      };
      return a;
    }();
    r.TextLine = v;
    var d = function() {
      return function(a, b, d, c, h) {
        void 0 === a && (a = "");
        void 0 === b && (b = "");
        void 0 === d && (d = "");
        void 0 === c && (c = 0);
        void 0 === h && (h = !1);
        this.font = a;
        this.fillStyle = b;
        this.text = d;
        this.width = c;
        this.underline = h;
      };
    }();
    r.TextRun = d;
    l = function(d) {
      function b(b) {
        d.call(this);
        this._flags = 1048592;
        this.properties = {};
        this._textBounds = b.clone();
        this._textRunData = null;
        this._plainText = "";
        this._borderColor = this._backgroundColor = 0;
        this._matrix = a.createIdentity();
        this._coords = null;
        this._scrollV = 1;
        this._scrollH = 0;
        this.textRect = b.clone();
        this.lines = [];
        this.setBounds(b);
      }
      __extends(b, d);
      b.prototype.setBounds = function(b) {
        d.prototype.setBounds.call(this, b);
        this._textBounds.set(b);
        this.textRect.setElements(b.x + 2, b.y + 2, b.w - 2, b.h - 2);
      };
      b.prototype.setContent = function(b, a, d, c) {
        this._textRunData = a;
        this._plainText = b;
        this._matrix.set(d);
        this._coords = c;
        this.lines = [];
      };
      b.prototype.setStyle = function(b, a, d, c) {
        this._backgroundColor = b;
        this._borderColor = a;
        this._scrollV = d;
        this._scrollH = c;
      };
      b.prototype.reflow = function(b, a) {
        var d = this._textRunData;
        if (d) {
          for (var c = this._bounds, e = c.w - 4, h = this._plainText, m = this.lines, g = new v, l = 0, p = 0, s = 0, n = 0, u = 0, r = -1;d.position < d.length;) {
            var t = d.readInt(), y = d.readInt(), z = d.readInt(), B = d.readUTF(), x = d.readInt(), E = d.readInt(), A = d.readInt();
            x > s && (s = x);
            E > n && (n = E);
            A > u && (u = A);
            x = d.readBoolean();
            E = "";
            d.readBoolean() && (E += "italic ");
            x && (E += "bold ");
            z = E + z + "px " + B;
            B = d.readInt();
            B = k.ColorUtilities.rgbToHex(B);
            x = d.readInt();
            -1 === r && (r = x);
            d.readBoolean();
            d.readInt();
            d.readInt();
            d.readInt();
            d.readInt();
            d.readInt();
            for (var x = d.readBoolean(), D = "", E = !1;!E;t++) {
              E = t >= y - 1;
              A = h[t];
              if ("\r" !== A && "\n" !== A && (D += A, t < h.length - 1)) {
                continue;
              }
              g.addRun(z, B, D, x);
              if (g.runs.length) {
                m.length && (l += u);
                l += s;
                g.y = l | 0;
                l += n;
                g.ascent = s;
                g.descent = n;
                g.leading = u;
                g.align = r;
                if (a && g.width > e) {
                  for (g = g.wrap(e), D = 0;D < g.length;D++) {
                    var F = g[D], l = F.y + F.descent + F.leading;
                    m.push(F);
                    F.width > p && (p = F.width);
                  }
                } else {
                  m.push(g), g.width > p && (p = g.width);
                }
                g = new v;
              } else {
                l += s + n + u;
              }
              D = "";
              if (E) {
                u = n = s = 0;
                r = -1;
                break;
              }
              "\r" === A && "\n" === h[t + 1] && t++;
            }
            g.addRun(z, B, D, x);
          }
          d = h[h.length - 1];
          "\r" !== d && "\n" !== d || m.push(g);
          d = this.textRect;
          d.w = p;
          d.h = l;
          if (b) {
            if (!a) {
              e = p;
              p = c.w;
              switch(b) {
                case 1:
                  d.x = p - (e + 4) >> 1;
                  break;
                case 3:
                  d.x = p - (e + 4);
              }
              this._textBounds.setElements(d.x - 2, d.y - 2, d.w + 4, d.h + 4);
              c.w = e + 4;
            }
            c.x = d.x - 2;
            c.h = l + 4;
          } else {
            this._textBounds = c;
          }
          for (t = 0;t < m.length;t++) {
            if (c = m[t], c.width < e) {
              switch(c.align) {
                case 1:
                  c.x = e - c.width | 0;
                  break;
                case 2:
                  c.x = (e - c.width) / 2 | 0;
              }
            }
          }
          this.invalidate();
        }
      };
      b.roundBoundPoints = function(b) {
        for (var a = 0;a < b.length;a++) {
          var d = b[a];
          d.x = Math.floor(d.x + .1) + .5;
          d.y = Math.floor(d.y + .1) + .5;
        }
      };
      b.prototype.render = function(d) {
        d.save();
        var c = this._textBounds;
        this._backgroundColor && (d.fillStyle = k.ColorUtilities.rgbaToCSSStyle(this._backgroundColor), d.fillRect(c.x, c.y, c.w, c.h));
        if (this._borderColor) {
          d.strokeStyle = k.ColorUtilities.rgbaToCSSStyle(this._borderColor);
          d.lineCap = "square";
          d.lineWidth = 1;
          var e = b.absoluteBoundPoints, h = d.currentTransform;
          h ? (c = c.clone(), (new a(h.a, h.b, h.c, h.d, h.e, h.f)).transformRectangle(c, e), d.setTransform(1, 0, 0, 1, 0, 0)) : (e[0].x = c.x, e[0].y = c.y, e[1].x = c.x + c.w, e[1].y = c.y, e[2].x = c.x + c.w, e[2].y = c.y + c.h, e[3].x = c.x, e[3].y = c.y + c.h);
          b.roundBoundPoints(e);
          c = new Path2D;
          c.moveTo(e[0].x, e[0].y);
          c.lineTo(e[1].x, e[1].y);
          c.lineTo(e[2].x, e[2].y);
          c.lineTo(e[3].x, e[3].y);
          c.lineTo(e[0].x, e[0].y);
          d.stroke(c);
          h && d.setTransform(h.a, h.b, h.c, h.d, h.e, h.f);
        }
        this._coords ? this._renderChars(d) : this._renderLines(d);
        d.restore();
      };
      b.prototype._renderChars = function(b) {
        if (this._matrix) {
          var a = this._matrix;
          b.transform(a.a, a.b, a.c, a.d, a.tx, a.ty);
        }
        for (var a = this.lines, d = this._coords, c = d.position = 0;c < a.length;c++) {
          for (var e = a[c].runs, h = 0;h < e.length;h++) {
            var m = e[h];
            b.font = m.font;
            b.fillStyle = m.fillStyle;
            for (var m = m.text, g = 0;g < m.length;g++) {
              var l = d.readInt() / 20, p = d.readInt() / 20;
              b.fillText(m[g], l, p);
            }
          }
        }
      };
      b.prototype._renderLines = function(b) {
        var a = this._textBounds;
        b.beginPath();
        b.rect(a.x + 2, a.y + 2, a.w - 4, a.h - 4);
        b.clip();
        b.translate(a.x - this._scrollH + 2, a.y + 2);
        for (var d = this.lines, c = this._scrollV, e = 0, h = 0;h < d.length;h++) {
          var m = d[h], g = m.x, l = m.y;
          if (h + 1 < c) {
            e = l + m.descent + m.leading;
          } else {
            l -= e;
            if (h + 1 - c && l > a.h) {
              break;
            }
            for (var p = m.runs, k = 0;k < p.length;k++) {
              var s = p[k];
              b.font = s.font;
              b.fillStyle = s.fillStyle;
              s.underline && b.fillRect(g, l + m.descent / 2 | 0, s.width, 1);
              b.textAlign = "left";
              b.textBaseline = "alphabetic";
              b.fillText(s.text, g, l);
              g += s.width;
            }
          }
        }
      };
      b.absoluteBoundPoints = [new t(0, 0), new t(0, 0), new t(0, 0), new t(0, 0)];
      return b;
    }(p);
    r.RenderableText = l;
    p = function(a) {
      function b(b, d) {
        a.call(this);
        this._flags = 3145728;
        this.properties = {};
        this.setBounds(new n(0, 0, b, d));
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "text", {get:function() {
        return this._text;
      }, set:function(b) {
        this._text = b;
      }, enumerable:!0, configurable:!0});
      b.prototype.render = function(b, a, d) {
        b.save();
        b.textBaseline = "top";
        b.fillStyle = "white";
        b.fillText(this.text, 0, 0);
        b.restore();
      };
      return b;
    }(p);
    r.Label = p;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = k.ColorUtilities.clampByte, c = function() {
      return function() {
      };
    }();
    r.Filter = c;
    var t = function(c) {
      function a(a, g, l) {
        c.call(this);
        this.blurX = a;
        this.blurY = g;
        this.quality = l;
      }
      __extends(a, c);
      return a;
    }(c);
    r.BlurFilter = t;
    t = function(c) {
      function a(a, g, l, k, m, s, v, d, e, b, f) {
        c.call(this);
        this.alpha = a;
        this.angle = g;
        this.blurX = l;
        this.blurY = k;
        this.color = m;
        this.distance = s;
        this.hideObject = v;
        this.inner = d;
        this.knockout = e;
        this.quality = b;
        this.strength = f;
      }
      __extends(a, c);
      return a;
    }(c);
    r.DropshadowFilter = t;
    c = function(c) {
      function a(a, g, l, k, m, s, v, d) {
        c.call(this);
        this.alpha = a;
        this.blurX = g;
        this.blurY = l;
        this.color = k;
        this.inner = m;
        this.knockout = s;
        this.quality = v;
        this.strength = d;
      }
      __extends(a, c);
      return a;
    }(c);
    r.GlowFilter = c;
    (function(c) {
      c[c.Unknown = 0] = "Unknown";
      c[c.Identity = 1] = "Identity";
    })(r.ColorMatrixType || (r.ColorMatrixType = {}));
    c = function() {
      function c(a) {
        this._data = new Float32Array(a);
        this._type = 0;
      }
      c.prototype.clone = function() {
        var a = new c(this._data);
        a._type = this._type;
        return a;
      };
      c.prototype.set = function(a) {
        this._data.set(a._data);
        this._type = a._type;
      };
      c.prototype.toWebGLMatrix = function() {
        return new Float32Array(this._data);
      };
      c.prototype.asWebGLMatrix = function() {
        return this._data.subarray(0, 16);
      };
      c.prototype.asWebGLVector = function() {
        return this._data.subarray(16, 20);
      };
      c.prototype.isIdentity = function() {
        if (this._type & 1) {
          return!0;
        }
        var a = this._data;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 1 == a[15] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      c.createIdentity = function() {
        var a = new c([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]);
        a._type = 1;
        return a;
      };
      c.prototype.setMultipliersAndOffsets = function(a, c, g, l, k, m, s, n) {
        for (var d = this._data, e = 0;e < d.length;e++) {
          d[e] = 0;
        }
        d[0] = a;
        d[5] = c;
        d[10] = g;
        d[15] = l;
        d[16] = k / 255;
        d[17] = m / 255;
        d[18] = s / 255;
        d[19] = n / 255;
        this._type = 0;
      };
      c.prototype.transformRGBA = function(a) {
        var c = a >> 24 & 255, p = a >> 16 & 255, l = a >> 8 & 255, k = a & 255, m = this._data;
        a = g(c * m[0] + p * m[1] + l * m[2] + k * m[3] + 255 * m[16]);
        var s = g(c * m[4] + p * m[5] + l * m[6] + k * m[7] + 255 * m[17]), n = g(c * m[8] + p * m[9] + l * m[10] + k * m[11] + 255 * m[18]), c = g(c * m[12] + p * m[13] + l * m[14] + k * m[15] + 255 * m[19]);
        return a << 24 | s << 16 | n << 8 | c;
      };
      c.prototype.multiply = function(a) {
        if (!(a._type & 1)) {
          var c = this._data, g = a._data;
          a = c[0];
          var l = c[1], k = c[2], m = c[3], s = c[4], n = c[5], d = c[6], e = c[7], b = c[8], f = c[9], q = c[10], r = c[11], G = c[12], U = c[13], t = c[14], Q = c[15], fa = c[16], ca = c[17], ga = c[18], ha = c[19], W = g[0], P = g[1], H = g[2], C = g[3], y = g[4], z = g[5], B = g[6], x = g[7], E = g[8], A = g[9], D = g[10], F = g[11], I = g[12], J = g[13], K = g[14], L = g[15], M = g[16], N = g[17], O = g[18], g = g[19];
          c[0] = a * W + s * P + b * H + G * C;
          c[1] = l * W + n * P + f * H + U * C;
          c[2] = k * W + d * P + q * H + t * C;
          c[3] = m * W + e * P + r * H + Q * C;
          c[4] = a * y + s * z + b * B + G * x;
          c[5] = l * y + n * z + f * B + U * x;
          c[6] = k * y + d * z + q * B + t * x;
          c[7] = m * y + e * z + r * B + Q * x;
          c[8] = a * E + s * A + b * D + G * F;
          c[9] = l * E + n * A + f * D + U * F;
          c[10] = k * E + d * A + q * D + t * F;
          c[11] = m * E + e * A + r * D + Q * F;
          c[12] = a * I + s * J + b * K + G * L;
          c[13] = l * I + n * J + f * K + U * L;
          c[14] = k * I + d * J + q * K + t * L;
          c[15] = m * I + e * J + r * K + Q * L;
          c[16] = a * M + s * N + b * O + G * g + fa;
          c[17] = l * M + n * N + f * O + U * g + ca;
          c[18] = k * M + d * N + q * O + t * g + ga;
          c[19] = m * M + e * N + r * O + Q * g + ha;
          this._type = 0;
        }
      };
      Object.defineProperty(c.prototype, "alphaMultiplier", {get:function() {
        return this._data[15];
      }, enumerable:!0, configurable:!0});
      c.prototype.hasOnlyAlphaMultiplier = function() {
        var a = this._data;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      c.prototype.equals = function(a) {
        if (!a) {
          return!1;
        }
        if (this._type === a._type && 1 === this._type) {
          return!0;
        }
        var c = this._data;
        a = a._data;
        for (var g = 0;20 > g;g++) {
          if (.001 < Math.abs(c[g] - a[g])) {
            return!1;
          }
        }
        return!0;
      };
      c.prototype.toSVGFilterMatrix = function() {
        var a = this._data;
        return[a[0], a[4], a[8], a[12], a[16], a[1], a[5], a[9], a[13], a[17], a[2], a[6], a[10], a[14], a[18], a[3], a[7], a[11], a[15], a[19]].join(" ");
      };
      return c;
    }();
    r.ColorMatrix = c;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      function c(a, c) {
        return-1 !== a.indexOf(c, this.length - c.length);
      }
      var t = r.Geometry.Point3D, n = r.Geometry.Matrix3D, a = r.Geometry.degreesToRadian, h = k.Debug.unexpected, p = k.Debug.notImplemented;
      g.SHADER_ROOT = "shaders/";
      var l = function() {
        function l(a, c) {
          this._fillColor = k.Color.Red;
          this._surfaceRegionCache = new k.LRUList;
          this.modelViewProjectionMatrix = n.createIdentity();
          this._canvas = a;
          this._options = c;
          this.gl = a.getContext("experimental-webgl", {preserveDrawingBuffer:!1, antialias:!0, stencil:!0, premultipliedAlpha:!1});
          this._programCache = Object.create(null);
          this._resize();
          this.gl.pixelStorei(this.gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, c.unpackPremultiplyAlpha ? this.gl.ONE : this.gl.ZERO);
          this._backgroundColor = k.Color.Black;
          this._geometry = new g.WebGLGeometry(this);
          this._tmpVertices = g.Vertex.createEmptyVertices(g.Vertex, 64);
          this._maxSurfaces = c.maxSurfaces;
          this._maxSurfaceSize = c.maxSurfaceSize;
          this.gl.blendFunc(this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
          this.gl.enable(this.gl.BLEND);
          this.modelViewProjectionMatrix = n.create2DProjection(this._w, this._h, 2E3);
          var h = this;
          this._surfaceRegionAllocator = new r.SurfaceRegionAllocator.SimpleAllocator(function() {
            var a = h._createTexture();
            return new g.WebGLSurface(1024, 1024, a);
          });
        }
        Object.defineProperty(l.prototype, "surfaces", {get:function() {
          return this._surfaceRegionAllocator.surfaces;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(l.prototype, "fillStyle", {set:function(a) {
          this._fillColor.set(k.Color.parseColor(a));
        }, enumerable:!0, configurable:!0});
        l.prototype.setBlendMode = function(a) {
          var c = this.gl;
          switch(a) {
            case 8:
              c.blendFunc(c.SRC_ALPHA, c.DST_ALPHA);
              break;
            case 3:
              c.blendFunc(c.DST_COLOR, c.ONE_MINUS_SRC_ALPHA);
              break;
            case 4:
              c.blendFunc(c.SRC_ALPHA, c.ONE);
              break;
            case 2:
            ;
            case 1:
              c.blendFunc(c.ONE, c.ONE_MINUS_SRC_ALPHA);
              break;
            default:
              p("Blend Mode: " + a);
          }
        };
        l.prototype.setBlendOptions = function() {
          this.gl.blendFunc(this._options.sourceBlendFactor, this._options.destinationBlendFactor);
        };
        l.glSupportedBlendMode = function(a) {
          switch(a) {
            case 8:
            ;
            case 3:
            ;
            case 4:
            ;
            case 1:
              return!0;
            default:
              return!1;
          }
        };
        l.prototype.create2DProjectionMatrix = function() {
          return n.create2DProjection(this._w, this._h, -this._w);
        };
        l.prototype.createPerspectiveMatrix = function(c, h, g) {
          g = a(g);
          h = n.createPerspective(a(h));
          var d = new t(0, 1, 0), e = new t(0, 0, 0);
          c = new t(0, 0, c);
          c = n.createCameraLookAt(c, e, d);
          c = n.createInverse(c);
          d = n.createIdentity();
          d = n.createMultiply(d, n.createTranslation(-this._w / 2, -this._h / 2));
          d = n.createMultiply(d, n.createScale(1 / this._w, -1 / this._h, .01));
          d = n.createMultiply(d, n.createYRotation(g));
          d = n.createMultiply(d, c);
          return d = n.createMultiply(d, h);
        };
        l.prototype.discardCachedImages = function() {
          2 <= r.traceLevel && r.writer && r.writer.writeLn("Discard Cache");
          for (var a = this._surfaceRegionCache.count / 2 | 0, c = 0;c < a;c++) {
            var h = this._surfaceRegionCache.pop();
            2 <= r.traceLevel && r.writer && r.writer.writeLn("Discard: " + h);
            h.texture.atlas.remove(h.region);
            h.texture = null;
          }
        };
        l.prototype.cacheImage = function(a) {
          var c = this.allocateSurfaceRegion(a.width, a.height);
          2 <= r.traceLevel && r.writer && r.writer.writeLn("Uploading Image: @ " + c.region);
          this._surfaceRegionCache.use(c);
          this.updateSurfaceRegion(a, c);
          return c;
        };
        l.prototype.allocateSurfaceRegion = function(a, c) {
          return this._surfaceRegionAllocator.allocate(a, c, null);
        };
        l.prototype.updateSurfaceRegion = function(a, c) {
          var h = this.gl;
          h.bindTexture(h.TEXTURE_2D, c.surface.texture);
          h.texSubImage2D(h.TEXTURE_2D, 0, c.region.x, c.region.y, h.RGBA, h.UNSIGNED_BYTE, a);
        };
        l.prototype._resize = function() {
          var a = this.gl;
          this._w = this._canvas.width;
          this._h = this._canvas.height;
          a.viewport(0, 0, this._w, this._h);
          for (var c in this._programCache) {
            this._initializeProgram(this._programCache[c]);
          }
        };
        l.prototype._initializeProgram = function(a) {
          this.gl.useProgram(a);
        };
        l.prototype._createShaderFromFile = function(a) {
          var h = g.SHADER_ROOT + a, l = this.gl;
          a = new XMLHttpRequest;
          a.open("GET", h, !1);
          a.send();
          if (c(h, ".vert")) {
            h = l.VERTEX_SHADER;
          } else {
            if (c(h, ".frag")) {
              h = l.FRAGMENT_SHADER;
            } else {
              throw "Shader Type: not supported.";
            }
          }
          return this._createShader(h, a.responseText);
        };
        l.prototype.createProgramFromFiles = function() {
          var a = this._programCache["combined.vert-combined.frag"];
          a || (a = this._createProgram([this._createShaderFromFile("combined.vert"), this._createShaderFromFile("combined.frag")]), this._queryProgramAttributesAndUniforms(a), this._initializeProgram(a), this._programCache["combined.vert-combined.frag"] = a);
          return a;
        };
        l.prototype._createProgram = function(a) {
          var c = this.gl, g = c.createProgram();
          a.forEach(function(a) {
            c.attachShader(g, a);
          });
          c.linkProgram(g);
          c.getProgramParameter(g, c.LINK_STATUS) || (h("Cannot link program: " + c.getProgramInfoLog(g)), c.deleteProgram(g));
          return g;
        };
        l.prototype._createShader = function(a, c) {
          var g = this.gl, d = g.createShader(a);
          g.shaderSource(d, c);
          g.compileShader(d);
          return g.getShaderParameter(d, g.COMPILE_STATUS) ? d : (h("Cannot compile shader: " + g.getShaderInfoLog(d)), g.deleteShader(d), null);
        };
        l.prototype._createTexture = function() {
          var a = this.gl, c = a.createTexture();
          a.bindTexture(a.TEXTURE_2D, c);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_S, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_T, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MIN_FILTER, a.LINEAR);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MAG_FILTER, a.LINEAR);
          a.texImage2D(a.TEXTURE_2D, 0, a.RGBA, 1024, 1024, 0, a.RGBA, a.UNSIGNED_BYTE, null);
          return c;
        };
        l.prototype._createFramebuffer = function(a) {
          var c = this.gl, h = c.createFramebuffer();
          c.bindFramebuffer(c.FRAMEBUFFER, h);
          c.framebufferTexture2D(c.FRAMEBUFFER, c.COLOR_ATTACHMENT0, c.TEXTURE_2D, a, 0);
          c.bindFramebuffer(c.FRAMEBUFFER, null);
          return h;
        };
        l.prototype._queryProgramAttributesAndUniforms = function(a) {
          a.uniforms = {};
          a.attributes = {};
          for (var c = this.gl, h = 0, d = c.getProgramParameter(a, c.ACTIVE_ATTRIBUTES);h < d;h++) {
            var e = c.getActiveAttrib(a, h);
            a.attributes[e.name] = e;
            e.location = c.getAttribLocation(a, e.name);
          }
          h = 0;
          for (d = c.getProgramParameter(a, c.ACTIVE_UNIFORMS);h < d;h++) {
            e = c.getActiveUniform(a, h), a.uniforms[e.name] = e, e.location = c.getUniformLocation(a, e.name);
          }
        };
        Object.defineProperty(l.prototype, "target", {set:function(a) {
          var c = this.gl;
          a ? (c.viewport(0, 0, a.w, a.h), c.bindFramebuffer(c.FRAMEBUFFER, a.framebuffer)) : (c.viewport(0, 0, this._w, this._h), c.bindFramebuffer(c.FRAMEBUFFER, null));
        }, enumerable:!0, configurable:!0});
        l.prototype.clear = function(a) {
          a = this.gl;
          a.clearColor(0, 0, 0, 0);
          a.clear(a.COLOR_BUFFER_BIT);
        };
        l.prototype.clearTextureRegion = function(a, c) {
          void 0 === c && (c = k.Color.None);
          var h = this.gl, d = a.region;
          this.target = a.surface;
          h.enable(h.SCISSOR_TEST);
          h.scissor(d.x, d.y, d.w, d.h);
          h.clearColor(c.r, c.g, c.b, c.a);
          h.clear(h.COLOR_BUFFER_BIT | h.DEPTH_BUFFER_BIT);
          h.disable(h.SCISSOR_TEST);
        };
        l.prototype.sizeOf = function(a) {
          var c = this.gl;
          switch(a) {
            case c.UNSIGNED_BYTE:
              return 1;
            case c.UNSIGNED_SHORT:
              return 2;
            case this.gl.INT:
            ;
            case this.gl.FLOAT:
              return 4;
            default:
              p(a);
          }
        };
        l.MAX_SURFACES = 8;
        return l;
      }();
      g.WebGLContext = l;
    })(r.WebGL || (r.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, r) {
  function g() {
    this.constructor = k;
  }
  for (var c in r) {
    r.hasOwnProperty(c) && (k[c] = r[c]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.Debug.assert, t = function(a) {
        function h() {
          a.apply(this, arguments);
        }
        __extends(h, a);
        h.prototype.ensureVertexCapacity = function(a) {
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8 * a);
        };
        h.prototype.writeVertex = function(a, h) {
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8);
          this.writeVertexUnsafe(a, h);
        };
        h.prototype.writeVertexUnsafe = function(a, c) {
          var h = this._offset >> 2;
          this._f32[h] = a;
          this._f32[h + 1] = c;
          this._offset += 8;
        };
        h.prototype.writeVertex3D = function(a, h, g) {
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 12);
          this.writeVertex3DUnsafe(a, h, g);
        };
        h.prototype.writeVertex3DUnsafe = function(a, c, h) {
          var g = this._offset >> 2;
          this._f32[g] = a;
          this._f32[g + 1] = c;
          this._f32[g + 2] = h;
          this._offset += 12;
        };
        h.prototype.writeTriangleElements = function(a, h, g) {
          c(0 === (this._offset & 1));
          this.ensureCapacity(this._offset + 6);
          var m = this._offset >> 1;
          this._u16[m] = a;
          this._u16[m + 1] = h;
          this._u16[m + 2] = g;
          this._offset += 6;
        };
        h.prototype.ensureColorCapacity = function(a) {
          c(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16 * a);
        };
        h.prototype.writeColorFloats = function(a, h, g, m) {
          c(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16);
          this.writeColorFloatsUnsafe(a, h, g, m);
        };
        h.prototype.writeColorFloatsUnsafe = function(a, c, h, g) {
          var k = this._offset >> 2;
          this._f32[k] = a;
          this._f32[k + 1] = c;
          this._f32[k + 2] = h;
          this._f32[k + 3] = g;
          this._offset += 16;
        };
        h.prototype.writeColor = function() {
          var a = Math.random(), h = Math.random(), g = Math.random(), m = Math.random() / 2;
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 4);
          this._i32[this._offset >> 2] = m << 24 | g << 16 | h << 8 | a;
          this._offset += 4;
        };
        h.prototype.writeColorUnsafe = function(a, c, h, g) {
          this._i32[this._offset >> 2] = g << 24 | h << 16 | c << 8 | a;
          this._offset += 4;
        };
        h.prototype.writeRandomColor = function() {
          this.writeColor();
        };
        return h;
      }(k.ArrayUtilities.ArrayWriter);
      g.BufferWriter = t;
      g.WebGLAttribute = function() {
        return function(a, c, g, l) {
          void 0 === l && (l = !1);
          this.name = a;
          this.size = c;
          this.type = g;
          this.normalized = l;
        };
      }();
      var n = function() {
        function a(a) {
          this.size = 0;
          this.attributes = a;
        }
        a.prototype.initialize = function(a) {
          for (var c = 0, g = 0;g < this.attributes.length;g++) {
            this.attributes[g].offset = c, c += a.sizeOf(this.attributes[g].type) * this.attributes[g].size;
          }
          this.size = c;
        };
        return a;
      }();
      g.WebGLAttributeList = n;
      n = function() {
        function a(a) {
          this._elementOffset = this.triangleCount = 0;
          this.context = a;
          this.array = new t(8);
          this.buffer = a.gl.createBuffer();
          this.elementArray = new t(8);
          this.elementBuffer = a.gl.createBuffer();
        }
        Object.defineProperty(a.prototype, "elementOffset", {get:function() {
          return this._elementOffset;
        }, enumerable:!0, configurable:!0});
        a.prototype.addQuad = function() {
          var a = this._elementOffset;
          this.elementArray.writeTriangleElements(a, a + 1, a + 2);
          this.elementArray.writeTriangleElements(a, a + 2, a + 3);
          this.triangleCount += 2;
          this._elementOffset += 4;
        };
        a.prototype.resetElementOffset = function() {
          this._elementOffset = 0;
        };
        a.prototype.reset = function() {
          this.array.reset();
          this.elementArray.reset();
          this.resetElementOffset();
          this.triangleCount = 0;
        };
        a.prototype.uploadBuffers = function() {
          var a = this.context.gl;
          a.bindBuffer(a.ARRAY_BUFFER, this.buffer);
          a.bufferData(a.ARRAY_BUFFER, this.array.subU8View(), a.DYNAMIC_DRAW);
          a.bindBuffer(a.ELEMENT_ARRAY_BUFFER, this.elementBuffer);
          a.bufferData(a.ELEMENT_ARRAY_BUFFER, this.elementArray.subU8View(), a.DYNAMIC_DRAW);
        };
        return a;
      }();
      g.WebGLGeometry = n;
      n = function(a) {
        function c(h, g, k) {
          a.call(this, h, g, k);
        }
        __extends(c, a);
        c.createEmptyVertices = function(a, c) {
          for (var h = [], g = 0;g < c;g++) {
            h.push(new a(0, 0, 0));
          }
          return h;
        };
        return c;
      }(r.Geometry.Point3D);
      g.Vertex = n;
      (function(a) {
        a[a.ZERO = 0] = "ZERO";
        a[a.ONE = 1] = "ONE";
        a[a.SRC_COLOR = 768] = "SRC_COLOR";
        a[a.ONE_MINUS_SRC_COLOR = 769] = "ONE_MINUS_SRC_COLOR";
        a[a.DST_COLOR = 774] = "DST_COLOR";
        a[a.ONE_MINUS_DST_COLOR = 775] = "ONE_MINUS_DST_COLOR";
        a[a.SRC_ALPHA = 770] = "SRC_ALPHA";
        a[a.ONE_MINUS_SRC_ALPHA = 771] = "ONE_MINUS_SRC_ALPHA";
        a[a.DST_ALPHA = 772] = "DST_ALPHA";
        a[a.ONE_MINUS_DST_ALPHA = 773] = "ONE_MINUS_DST_ALPHA";
        a[a.SRC_ALPHA_SATURATE = 776] = "SRC_ALPHA_SATURATE";
        a[a.CONSTANT_COLOR = 32769] = "CONSTANT_COLOR";
        a[a.ONE_MINUS_CONSTANT_COLOR = 32770] = "ONE_MINUS_CONSTANT_COLOR";
        a[a.CONSTANT_ALPHA = 32771] = "CONSTANT_ALPHA";
        a[a.ONE_MINUS_CONSTANT_ALPHA = 32772] = "ONE_MINUS_CONSTANT_ALPHA";
      })(g.WebGLBlendFactor || (g.WebGLBlendFactor = {}));
    })(r.WebGL || (r.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var c = function() {
        function c(a, h, g) {
          this.texture = g;
          this.w = a;
          this.h = h;
          this._regionAllocator = new k.RegionAllocator.CompactAllocator(this.w, this.h);
        }
        c.prototype.allocate = function(a, c) {
          var g = this._regionAllocator.allocate(a, c);
          return g ? new t(this, g) : null;
        };
        c.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return c;
      }();
      g.WebGLSurface = c;
      var t = function() {
        return function(c, a) {
          this.surface = c;
          this.region = a;
          this.next = this.previous = null;
        };
      }();
      g.WebGLSurfaceRegion = t;
    })(k.WebGL || (k.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.Color;
      g.TILE_SIZE = 256;
      g.MIN_UNTILED_SIZE = 256;
      var t = r.Geometry.Matrix, n = r.Geometry.Rectangle, a = function(a) {
        function c() {
          a.apply(this, arguments);
          this.maxSurfaces = 8;
          this.maxSurfaceSize = 4096;
          this.animateZoom = !0;
          this.disableSurfaceUploads = !1;
          this.frameSpacing = 1E-4;
          this.drawSurfaces = this.ignoreColorMatrix = !1;
          this.drawSurface = -1;
          this.premultipliedAlpha = !1;
          this.unpackPremultiplyAlpha = !0;
          this.showTemporaryCanvases = !1;
          this.sourceBlendFactor = 1;
          this.destinationBlendFactor = 771;
        }
        __extends(c, a);
        return c;
      }(r.RendererOptions);
      g.WebGLRendererOptions = a;
      var h = function(h) {
        function l(c, m, l) {
          void 0 === l && (l = new a);
          h.call(this, c, m, l);
          this._tmpVertices = g.Vertex.createEmptyVertices(g.Vertex, 64);
          this._cachedTiles = [];
          c = this._context = new g.WebGLContext(this._canvas, l);
          this._updateSize();
          this._brush = new g.WebGLCombinedBrush(c, new g.WebGLGeometry(c));
          this._stencilBrush = new g.WebGLCombinedBrush(c, new g.WebGLGeometry(c));
          this._scratchCanvas = document.createElement("canvas");
          this._scratchCanvas.width = this._scratchCanvas.height = 2048;
          this._scratchCanvasContext = this._scratchCanvas.getContext("2d", {willReadFrequently:!0});
          this._dynamicScratchCanvas = document.createElement("canvas");
          this._dynamicScratchCanvas.width = this._dynamicScratchCanvas.height = 0;
          this._dynamicScratchCanvasContext = this._dynamicScratchCanvas.getContext("2d", {willReadFrequently:!0});
          this._uploadCanvas = document.createElement("canvas");
          this._uploadCanvas.width = this._uploadCanvas.height = 0;
          this._uploadCanvasContext = this._uploadCanvas.getContext("2d", {willReadFrequently:!0});
          l.showTemporaryCanvases && (document.getElementById("temporaryCanvasPanelContainer").appendChild(this._uploadCanvas), document.getElementById("temporaryCanvasPanelContainer").appendChild(this._scratchCanvas));
          this._clipStack = [];
        }
        __extends(l, h);
        l.prototype.resize = function() {
          this._updateSize();
          this.render();
        };
        l.prototype._updateSize = function() {
          this._viewport = new n(0, 0, this._canvas.width, this._canvas.height);
          this._context._resize();
        };
        l.prototype._cacheImageCallback = function(a, c, h) {
          var g = h.w, d = h.h, e = h.x;
          h = h.y;
          this._uploadCanvas.width = g + 2;
          this._uploadCanvas.height = d + 2;
          this._uploadCanvasContext.drawImage(c.canvas, e, h, g, d, 1, 1, g, d);
          this._uploadCanvasContext.drawImage(c.canvas, e, h, g, 1, 1, 0, g, 1);
          this._uploadCanvasContext.drawImage(c.canvas, e, h + d - 1, g, 1, 1, d + 1, g, 1);
          this._uploadCanvasContext.drawImage(c.canvas, e, h, 1, d, 0, 1, 1, d);
          this._uploadCanvasContext.drawImage(c.canvas, e + g - 1, h, 1, d, g + 1, 1, 1, d);
          return a && a.surface ? (this._options.disableSurfaceUploads || this._context.updateSurfaceRegion(this._uploadCanvas, a), a) : this._context.cacheImage(this._uploadCanvas);
        };
        l.prototype._enterClip = function(a, c, h, g) {
          h.flush();
          c = this._context.gl;
          0 === this._clipStack.length && (c.enable(c.STENCIL_TEST), c.clear(c.STENCIL_BUFFER_BIT), c.stencilFunc(c.ALWAYS, 1, 1));
          this._clipStack.push(a);
          c.colorMask(!1, !1, !1, !1);
          c.stencilOp(c.KEEP, c.KEEP, c.INCR);
          h.flush();
          c.colorMask(!0, !0, !0, !0);
          c.stencilFunc(c.NOTEQUAL, 0, this._clipStack.length);
          c.stencilOp(c.KEEP, c.KEEP, c.KEEP);
        };
        l.prototype._leaveClip = function(a, c, h, g) {
          h.flush();
          c = this._context.gl;
          if (a = this._clipStack.pop()) {
            c.colorMask(!1, !1, !1, !1), c.stencilOp(c.KEEP, c.KEEP, c.DECR), h.flush(), c.colorMask(!0, !0, !0, !0), c.stencilFunc(c.NOTEQUAL, 0, this._clipStack.length), c.stencilOp(c.KEEP, c.KEEP, c.KEEP);
          }
          0 === this._clipStack.length && c.disable(c.STENCIL_TEST);
        };
        l.prototype._renderFrame = function(a, c, h, g) {
        };
        l.prototype._renderSurfaces = function(a) {
          var h = this._options, l = this._context, k = this._viewport;
          if (h.drawSurfaces) {
            var d = l.surfaces, l = t.createIdentity();
            if (0 <= h.drawSurface && h.drawSurface < d.length) {
              for (var h = d[h.drawSurface | 0], d = new n(0, 0, h.w, h.h), e = d.clone();e.w > k.w;) {
                e.scale(.5, .5);
              }
              a.drawImage(new g.WebGLSurfaceRegion(h, d), e, c.White, null, l, .2);
            } else {
              e = k.w / 5;
              e > k.h / d.length && (e = k.h / d.length);
              a.fillRectangle(new n(k.w - e, 0, e, k.h), new c(0, 0, 0, .5), l, .1);
              for (var b = 0;b < d.length;b++) {
                var h = d[b], f = new n(k.w - e, b * e, e, e);
                a.drawImage(new g.WebGLSurfaceRegion(h, new n(0, 0, h.w, h.h)), f, c.White, null, l, .2);
              }
            }
            a.flush();
          }
        };
        l.prototype.render = function() {
          var a = this._options, h = this._context.gl;
          this._context.modelViewProjectionMatrix = a.perspectiveCamera ? this._context.createPerspectiveMatrix(a.perspectiveCameraDistance + (a.animateZoom ? .8 * Math.sin(Date.now() / 3E3) : 0), a.perspectiveCameraFOV, a.perspectiveCameraAngle) : this._context.create2DProjectionMatrix();
          var g = this._brush;
          h.clearColor(0, 0, 0, 0);
          h.clear(h.COLOR_BUFFER_BIT | h.DEPTH_BUFFER_BIT);
          g.reset();
          h = this._viewport;
          g.flush();
          a.paintViewport && (g.fillRectangle(h, new c(.5, 0, 0, .25), t.createIdentity(), 0), g.flush());
          this._renderSurfaces(g);
        };
        return l;
      }(r.Renderer);
      g.WebGLRenderer = h;
    })(r.WebGL || (r.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.Color, t = r.Geometry.Point, n = r.Geometry.Matrix3D, a = function() {
        function a(c, h, g) {
          this._target = g;
          this._context = c;
          this._geometry = h;
        }
        a.prototype.reset = function() {
        };
        a.prototype.flush = function() {
        };
        Object.defineProperty(a.prototype, "target", {get:function() {
          return this._target;
        }, set:function(a) {
          this._target !== a && this.flush();
          this._target = a;
        }, enumerable:!0, configurable:!0});
        return a;
      }();
      g.WebGLBrush = a;
      (function(a) {
        a[a.FillColor = 0] = "FillColor";
        a[a.FillTexture = 1] = "FillTexture";
        a[a.FillTextureWithColorMatrix = 2] = "FillTextureWithColorMatrix";
      })(g.WebGLCombinedBrushKind || (g.WebGLCombinedBrushKind = {}));
      var h = function(a) {
        function h(g, m, l) {
          a.call(this, g, m, l);
          this.kind = 0;
          this.color = new c(0, 0, 0, 0);
          this.sampler = 0;
          this.coordinate = new t(0, 0);
        }
        __extends(h, a);
        h.initializeAttributeList = function(a) {
          var c = a.gl;
          h.attributeList || (h.attributeList = new g.WebGLAttributeList([new g.WebGLAttribute("aPosition", 3, c.FLOAT), new g.WebGLAttribute("aCoordinate", 2, c.FLOAT), new g.WebGLAttribute("aColor", 4, c.UNSIGNED_BYTE, !0), new g.WebGLAttribute("aKind", 1, c.FLOAT), new g.WebGLAttribute("aSampler", 1, c.FLOAT)]), h.attributeList.initialize(a));
        };
        h.prototype.writeTo = function(a) {
          a = a.array;
          a.ensureAdditionalCapacity();
          a.writeVertex3DUnsafe(this.x, this.y, this.z);
          a.writeVertexUnsafe(this.coordinate.x, this.coordinate.y);
          a.writeColorUnsafe(255 * this.color.r, 255 * this.color.g, 255 * this.color.b, 255 * this.color.a);
          a.writeFloatUnsafe(this.kind);
          a.writeFloatUnsafe(this.sampler);
        };
        return h;
      }(g.Vertex);
      g.WebGLCombinedBrushVertex = h;
      a = function(a) {
        function c(g, m, l) {
          void 0 === l && (l = null);
          a.call(this, g, m, l);
          this._blendMode = 1;
          this._program = g.createProgramFromFiles();
          this._surfaces = [];
          h.initializeAttributeList(this._context);
        }
        __extends(c, a);
        c.prototype.reset = function() {
          this._surfaces = [];
          this._geometry.reset();
        };
        c.prototype.drawImage = function(a, h, g, k, d, e, b) {
          void 0 === e && (e = 0);
          void 0 === b && (b = 1);
          if (!a || !a.surface) {
            return!0;
          }
          h = h.clone();
          this._colorMatrix && (k && this._colorMatrix.equals(k) || this.flush());
          this._colorMatrix = k;
          this._blendMode !== b && (this.flush(), this._blendMode = b);
          b = this._surfaces.indexOf(a.surface);
          0 > b && (8 === this._surfaces.length && this.flush(), this._surfaces.push(a.surface), b = this._surfaces.length - 1);
          var f = c._tmpVertices, q = a.region.clone();
          q.offset(1, 1).resize(-2, -2);
          q.scale(1 / a.surface.w, 1 / a.surface.h);
          d.transformRectangle(h, f);
          for (a = 0;4 > a;a++) {
            f[a].z = e;
          }
          f[0].coordinate.x = q.x;
          f[0].coordinate.y = q.y;
          f[1].coordinate.x = q.x + q.w;
          f[1].coordinate.y = q.y;
          f[2].coordinate.x = q.x + q.w;
          f[2].coordinate.y = q.y + q.h;
          f[3].coordinate.x = q.x;
          f[3].coordinate.y = q.y + q.h;
          for (a = 0;4 > a;a++) {
            e = c._tmpVertices[a], e.kind = k ? 2 : 1, e.color.set(g), e.sampler = b, e.writeTo(this._geometry);
          }
          this._geometry.addQuad();
          return!0;
        };
        c.prototype.fillRectangle = function(a, h, g, k) {
          void 0 === k && (k = 0);
          g.transformRectangle(a, c._tmpVertices);
          for (a = 0;4 > a;a++) {
            g = c._tmpVertices[a], g.kind = 0, g.color.set(h), g.z = k, g.writeTo(this._geometry);
          }
          this._geometry.addQuad();
        };
        c.prototype.flush = function() {
          var a = this._geometry, c = this._program, g = this._context.gl, l;
          a.uploadBuffers();
          g.useProgram(c);
          this._target ? (l = n.create2DProjection(this._target.w, this._target.h, 2E3), l = n.createMultiply(l, n.createScale(1, -1, 1))) : l = this._context.modelViewProjectionMatrix;
          g.uniformMatrix4fv(c.uniforms.uTransformMatrix3D.location, !1, l.asWebGLMatrix());
          this._colorMatrix && (g.uniformMatrix4fv(c.uniforms.uColorMatrix.location, !1, this._colorMatrix.asWebGLMatrix()), g.uniform4fv(c.uniforms.uColorVector.location, this._colorMatrix.asWebGLVector()));
          for (l = 0;l < this._surfaces.length;l++) {
            g.activeTexture(g.TEXTURE0 + l), g.bindTexture(g.TEXTURE_2D, this._surfaces[l].texture);
          }
          g.uniform1iv(c.uniforms["uSampler[0]"].location, [0, 1, 2, 3, 4, 5, 6, 7]);
          g.bindBuffer(g.ARRAY_BUFFER, a.buffer);
          var d = h.attributeList.size, e = h.attributeList.attributes;
          for (l = 0;l < e.length;l++) {
            var b = e[l], f = c.attributes[b.name].location;
            g.enableVertexAttribArray(f);
            g.vertexAttribPointer(f, b.size, b.type, b.normalized, d, b.offset);
          }
          this._context.setBlendOptions();
          this._context.target = this._target;
          g.bindBuffer(g.ELEMENT_ARRAY_BUFFER, a.elementBuffer);
          g.drawElements(g.TRIANGLES, 3 * a.triangleCount, g.UNSIGNED_SHORT, 0);
          this.reset();
        };
        c._tmpVertices = g.Vertex.createEmptyVertices(h, 4);
        c._depth = 1;
        return c;
      }(a);
      g.WebGLCombinedBrush = a;
    })(r.WebGL || (r.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var c = CanvasRenderingContext2D.prototype.save, k = CanvasRenderingContext2D.prototype.clip, n = CanvasRenderingContext2D.prototype.fill, a = CanvasRenderingContext2D.prototype.stroke, h = CanvasRenderingContext2D.prototype.restore, p = CanvasRenderingContext2D.prototype.beginPath;
      g.notifyReleaseChanged = function() {
        CanvasRenderingContext2D.prototype.save = c;
        CanvasRenderingContext2D.prototype.clip = k;
        CanvasRenderingContext2D.prototype.fill = n;
        CanvasRenderingContext2D.prototype.stroke = a;
        CanvasRenderingContext2D.prototype.restore = h;
        CanvasRenderingContext2D.prototype.beginPath = p;
      };
      CanvasRenderingContext2D.prototype.enterBuildingClippingRegion = function() {
        this.buildingClippingRegionDepth || (this.buildingClippingRegionDepth = 0);
        this.buildingClippingRegionDepth++;
      };
      CanvasRenderingContext2D.prototype.leaveBuildingClippingRegion = function() {
        this.buildingClippingRegionDepth--;
      };
    })(k.Canvas2D || (k.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      function c(a) {
        var c = "source-over";
        switch(a) {
          case 1:
          ;
          case 2:
            break;
          case 3:
            c = "multiply";
            break;
          case 8:
          ;
          case 4:
            c = "screen";
            break;
          case 5:
            c = "lighten";
            break;
          case 6:
            c = "darken";
            break;
          case 7:
            c = "difference";
            break;
          case 13:
            c = "overlay";
            break;
          case 14:
            c = "hard-light";
            break;
          case 11:
            c = "destination-in";
            break;
          case 12:
            c = "destination-out";
            break;
          default:
            k.Debug.somewhatImplemented("Blend Mode: " + r.BlendMode[a]);
        }
        return c;
      }
      var t = k.NumberUtilities.clamp;
      navigator.userAgent.indexOf("Firefox");
      var n = function() {
        function a() {
        }
        a._prepareSVGFilters = function() {
          if (!a._svgBlurFilter) {
            var c = document.createElementNS("http://www.w3.org/2000/svg", "svg"), g = document.createElementNS("http://www.w3.org/2000/svg", "defs"), k = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            k.setAttribute("id", "svgBlurFilter");
            var m = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            m.setAttribute("stdDeviation", "0 0");
            k.appendChild(m);
            g.appendChild(k);
            a._svgBlurFilter = m;
            k = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            k.setAttribute("id", "svgDropShadowFilter");
            m = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            m.setAttribute("in", "SourceAlpha");
            m.setAttribute("stdDeviation", "3");
            k.appendChild(m);
            a._svgDropshadowFilterBlur = m;
            m = document.createElementNS("http://www.w3.org/2000/svg", "feOffset");
            m.setAttribute("dx", "0");
            m.setAttribute("dy", "0");
            m.setAttribute("result", "offsetblur");
            k.appendChild(m);
            a._svgDropshadowFilterOffset = m;
            m = document.createElementNS("http://www.w3.org/2000/svg", "feFlood");
            m.setAttribute("flood-color", "rgba(0,0,0,1)");
            k.appendChild(m);
            a._svgDropshadowFilterFlood = m;
            m = document.createElementNS("http://www.w3.org/2000/svg", "feComposite");
            m.setAttribute("in2", "offsetblur");
            m.setAttribute("operator", "in");
            k.appendChild(m);
            var m = document.createElementNS("http://www.w3.org/2000/svg", "feMerge"), n = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            m.appendChild(n);
            n = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            n.setAttribute("in", "SourceGraphic");
            m.appendChild(n);
            k.appendChild(m);
            g.appendChild(k);
            k = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            k.setAttribute("id", "svgColorMatrixFilter");
            m = document.createElementNS("http://www.w3.org/2000/svg", "feColorMatrix");
            m.setAttribute("color-interpolation-filters", "sRGB");
            m.setAttribute("in", "SourceGraphic");
            m.setAttribute("type", "matrix");
            k.appendChild(m);
            g.appendChild(k);
            a._svgColorMatrixFilter = m;
            c.appendChild(g);
            document.documentElement.appendChild(c);
          }
        };
        a._applyColorMatrixFilter = function(c, g) {
          a._prepareSVGFilters();
          a._svgColorMatrixFilter.setAttribute("values", g.toSVGFilterMatrix());
          c.filter = "url(#svgColorMatrixFilter)";
        };
        a._applyFilters = function(c, g, n) {
          function m(a) {
            var b = c / 2;
            switch(a) {
              case 0:
                return 0;
              case 1:
                return b / 2.7;
              case 2:
                return b / 1.28;
              default:
                return b;
            }
          }
          a._prepareSVGFilters();
          a._removeFilters(g);
          for (var s = 0;s < n.length;s++) {
            var v = n[s];
            if (v instanceof r.BlurFilter) {
              var d = v, v = m(d.quality);
              a._svgBlurFilter.setAttribute("stdDeviation", d.blurX * v + " " + d.blurY * v);
              g.filter = "url(#svgBlurFilter)";
            } else {
              v instanceof r.DropshadowFilter && (d = v, v = m(d.quality), a._svgDropshadowFilterBlur.setAttribute("stdDeviation", d.blurX * v + " " + d.blurY * v), a._svgDropshadowFilterOffset.setAttribute("dx", String(Math.cos(d.angle * Math.PI / 180) * d.distance * c)), a._svgDropshadowFilterOffset.setAttribute("dy", String(Math.sin(d.angle * Math.PI / 180) * d.distance * c)), a._svgDropshadowFilterFlood.setAttribute("flood-color", k.ColorUtilities.rgbaToCSSStyle(d.color << 8 | Math.round(255 * 
              d.alpha))), g.filter = "url(#svgDropShadowFilter)");
            }
          }
        };
        a._removeFilters = function(a) {
          a.filter = "none";
        };
        a._applyColorMatrix = function(c, g) {
          a._removeFilters(c);
          g.isIdentity() ? (c.globalAlpha = 1, c.globalColorMatrix = null) : g.hasOnlyAlphaMultiplier() ? (c.globalAlpha = t(g.alphaMultiplier, 0, 1), c.globalColorMatrix = null) : (c.globalAlpha = 1, a._svgFiltersAreSupported ? (a._applyColorMatrixFilter(c, g), c.globalColorMatrix = null) : c.globalColorMatrix = g);
        };
        a._svgFiltersAreSupported = !!Object.getOwnPropertyDescriptor(CanvasRenderingContext2D.prototype, "filter");
        return a;
      }();
      g.Filters = n;
      var a = function() {
        function a(c, h, g, m) {
          this.surface = c;
          this.region = h;
          this.w = g;
          this.h = m;
        }
        a.prototype.free = function() {
          this.surface.free(this);
        };
        a._ensureCopyCanvasSize = function(c, g) {
          var n;
          if (a._copyCanvasContext) {
            if (n = a._copyCanvasContext.canvas, n.width < c || n.height < g) {
              n.width = k.IntegerUtilities.nearestPowerOfTwo(c), n.height = k.IntegerUtilities.nearestPowerOfTwo(g);
            }
          } else {
            n = document.createElement("canvas"), "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(n), n.width = 512, n.height = 512, a._copyCanvasContext = n.getContext("2d");
          }
        };
        a.prototype.draw = function(g, l, k, m, n, r) {
          this.context.setTransform(1, 0, 0, 1, 0, 0);
          var d, e = 0, b = 0;
          g.context.canvas === this.context.canvas ? (a._ensureCopyCanvasSize(m, n), d = a._copyCanvasContext, d.clearRect(0, 0, m, n), d.drawImage(g.surface.canvas, g.region.x, g.region.y, m, n, 0, 0, m, n), d = d.canvas, b = e = 0) : (d = g.surface.canvas, e = g.region.x, b = g.region.y);
          a: {
            switch(r) {
              case 11:
                g = !0;
                break a;
              default:
                g = !1;
            }
          }
          g && (this.context.save(), this.context.beginPath(), this.context.rect(l, k, m, n), this.context.clip());
          this.context.globalCompositeOperation = c(r);
          this.context.drawImage(d, e, b, m, n, l, k, m, n);
          this.context.globalCompositeOperation = c(1);
          g && this.context.restore();
        };
        Object.defineProperty(a.prototype, "context", {get:function() {
          return this.surface.context;
        }, enumerable:!0, configurable:!0});
        a.prototype.resetTransform = function() {
          this.surface.context.setTransform(1, 0, 0, 1, 0, 0);
        };
        a.prototype.reset = function() {
          var a = this.surface.context;
          a.setTransform(1, 0, 0, 1, 0, 0);
          a.fillStyle = null;
          a.strokeStyle = null;
          a.globalAlpha = 1;
          a.globalColorMatrix = null;
          a.globalCompositeOperation = c(1);
        };
        a.prototype.fill = function(a) {
          var c = this.surface.context, h = this.region;
          c.fillStyle = a;
          c.fillRect(h.x, h.y, h.w, h.h);
        };
        a.prototype.clear = function(a) {
          var c = this.surface.context, h = this.region;
          a || (a = h);
          c.clearRect(a.x, a.y, a.w, a.h);
        };
        return a;
      }();
      g.Canvas2DSurfaceRegion = a;
      n = function() {
        function c(a, h) {
          this.canvas = a;
          this.context = a.getContext("2d");
          this.w = a.width;
          this.h = a.height;
          this._regionAllocator = h;
        }
        c.prototype.allocate = function(c, h) {
          var g = this._regionAllocator.allocate(c, h);
          return g ? new a(this, g, c, h) : null;
        };
        c.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return c;
      }();
      g.Canvas2DSurface = n;
    })(r.Canvas2D || (r.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.Debug.assert, t = k.GFX.Geometry.Rectangle, n = k.GFX.Geometry.Point, a = k.GFX.Geometry.Matrix, h = k.NumberUtilities.clamp, p = k.NumberUtilities.pow2, l = new k.IndentingWriter(!1, dumpLine), u = function() {
        return function(a, c) {
          this.surfaceRegion = a;
          this.scale = c;
        };
      }();
      g.MipMapLevel = u;
      var m = function() {
        function a(b, c, d, e) {
          this._node = c;
          this._levels = [];
          this._surfaceRegionAllocator = d;
          this._size = e;
          this._renderer = b;
        }
        a.prototype.getLevel = function(a) {
          a = Math.max(a.getAbsoluteScaleX(), a.getAbsoluteScaleY());
          var b = 0;
          1 !== a && (b = h(Math.round(Math.log(a) / Math.LN2), -5, 3));
          this._node.hasFlags(2097152) || (b = h(b, -5, 0));
          a = p(b);
          var c = 5 + b, b = this._levels[c];
          if (!b) {
            var d = this._node.getBounds().clone();
            d.scale(a, a);
            d.snap();
            var e = this._surfaceRegionAllocator.allocate(d.w, d.h, null), g = e.region, b = this._levels[c] = new u(e, a), c = new v(e);
            c.clip.set(g);
            c.matrix.setElements(a, 0, 0, a, g.x - d.x, g.y - d.y);
            c.flags |= 64;
            this._renderer.renderNodeWithState(this._node, c);
            c.free();
          }
          return b;
        };
        return a;
      }();
      g.MipMap = m;
      (function(a) {
        a[a.NonZero = 0] = "NonZero";
        a[a.EvenOdd = 1] = "EvenOdd";
      })(g.FillRule || (g.FillRule = {}));
      var s = function(a) {
        function c() {
          a.apply(this, arguments);
          this.blending = this.imageSmoothing = this.snapToDevicePixels = !0;
          this.debugLayers = !1;
          this.filters = this.masking = !0;
          this.cacheShapes = !1;
          this.cacheShapesMaxSize = 256;
          this.cacheShapesThreshold = 16;
          this.alpha = !1;
        }
        __extends(c, a);
        return c;
      }(r.RendererOptions);
      g.Canvas2DRendererOptions = s;
      (function(a) {
        a[a.None = 0] = "None";
        a[a.IgnoreNextLayer = 1] = "IgnoreNextLayer";
        a[a.RenderMask = 2] = "RenderMask";
        a[a.IgnoreMask = 4] = "IgnoreMask";
        a[a.PaintStencil = 8] = "PaintStencil";
        a[a.PaintClip = 16] = "PaintClip";
        a[a.IgnoreRenderable = 32] = "IgnoreRenderable";
        a[a.IgnoreNextRenderWithCache = 64] = "IgnoreNextRenderWithCache";
        a[a.CacheShapes = 256] = "CacheShapes";
        a[a.PaintFlashing = 512] = "PaintFlashing";
        a[a.PaintBounds = 1024] = "PaintBounds";
        a[a.PaintDirtyRegion = 2048] = "PaintDirtyRegion";
        a[a.ImageSmoothing = 4096] = "ImageSmoothing";
        a[a.PixelSnapping = 8192] = "PixelSnapping";
      })(g.RenderFlags || (g.RenderFlags = {}));
      t.createMaxI16();
      var v = function(b) {
        function c(d) {
          b.call(this);
          this.clip = t.createEmpty();
          this.clipList = [];
          this.flags = 0;
          this.target = null;
          this.matrix = a.createIdentity();
          this.colorMatrix = r.ColorMatrix.createIdentity();
          c.allocationCount++;
          this.target = d;
        }
        __extends(c, b);
        c.prototype.set = function(a) {
          this.clip.set(a.clip);
          this.target = a.target;
          this.matrix.set(a.matrix);
          this.colorMatrix.set(a.colorMatrix);
          this.flags = a.flags;
          k.ArrayUtilities.copyFrom(this.clipList, a.clipList);
        };
        c.prototype.clone = function() {
          var a = c.allocate();
          a || (a = new c(this.target));
          a.set(this);
          return a;
        };
        c.allocate = function() {
          var a = c._dirtyStack, b = null;
          a.length && (b = a.pop());
          return b;
        };
        c.prototype.free = function() {
          c._dirtyStack.push(this);
        };
        c.prototype.transform = function(a) {
          var b = this.clone();
          b.matrix.preMultiply(a.getMatrix());
          a.hasColorMatrix() && b.colorMatrix.multiply(a.getColorMatrix());
          return b;
        };
        c.prototype.hasFlags = function(a) {
          return(this.flags & a) === a;
        };
        c.prototype.removeFlags = function(a) {
          this.flags &= ~a;
        };
        c.prototype.toggleFlags = function(a, b) {
          this.flags = b ? this.flags | a : this.flags & ~a;
        };
        c.allocationCount = 0;
        c._dirtyStack = [];
        return c;
      }(r.State);
      g.RenderState = v;
      var d = function() {
        function b() {
          this.culledNodes = this.groups = this.shapes = this._count = 0;
        }
        b.prototype.enter = function(a) {
          this._count++;
          l && (l.enter("> Frame: " + this._count), this._enterTime = performance.now(), this.culledNodes = this.groups = this.shapes = 0);
        };
        b.prototype.leave = function() {
          l && (l.writeLn("Shapes: " + this.shapes + ", Groups: " + this.groups + ", Culled Nodes: " + this.culledNodes), l.writeLn("Elapsed: " + (performance.now() - this._enterTime).toFixed(2)), l.writeLn("Rectangle: " + t.allocationCount + ", Matrix: " + a.allocationCount + ", State: " + v.allocationCount), l.leave("<"));
        };
        return b;
      }();
      g.FrameInfo = d;
      var e = function(b) {
        function e(a, c, h) {
          void 0 === h && (h = new s);
          b.call(this, a, c, h);
          this._visited = 0;
          this._frameInfo = new d;
          this._fontSize = 0;
          this._layers = [];
          if (a instanceof HTMLCanvasElement) {
            var g = a;
            this._viewport = new t(0, 0, g.width, g.height);
            this._target = this._createTarget(g);
          } else {
            this._addLayer("Background Layer");
            h = this._addLayer("Canvas Layer");
            g = document.createElement("canvas");
            h.appendChild(g);
            this._viewport = new t(0, 0, a.scrollWidth, a.scrollHeight);
            var m = this;
            c.addEventListener(1, function() {
              m._onStageBoundsChanged(g);
            });
            this._onStageBoundsChanged(g);
          }
          e._prepareSurfaceAllocators();
        }
        __extends(e, b);
        e.prototype._addLayer = function(a) {
          a = document.createElement("div");
          a.style.position = "absolute";
          a.style.overflow = "hidden";
          a.style.width = "100%";
          a.style.height = "100%";
          a.style.zIndex = this._layers.length + "";
          this._container.appendChild(a);
          this._layers.push(a);
          return a;
        };
        Object.defineProperty(e.prototype, "_backgroundVideoLayer", {get:function() {
          return this._layers[0];
        }, enumerable:!0, configurable:!0});
        e.prototype._createTarget = function(a) {
          return new g.Canvas2DSurfaceRegion(new g.Canvas2DSurface(a), new r.RegionAllocator.Region(0, 0, a.width, a.height), a.width, a.height);
        };
        e.prototype._onStageBoundsChanged = function(a) {
          var b = this._stage.getBounds(!0);
          b.snap();
          for (var c = this._devicePixelRatio = window.devicePixelRatio || 1, d = b.w / c + "px", c = b.h / c + "px", e = 0;e < this._layers.length;e++) {
            var f = this._layers[e];
            f.style.width = d;
            f.style.height = c;
          }
          a.width = b.w;
          a.height = b.h;
          a.style.position = "absolute";
          a.style.width = d;
          a.style.height = c;
          this._target = this._createTarget(a);
          this._fontSize = 10 * this._devicePixelRatio;
        };
        e._prepareSurfaceAllocators = function() {
          e._initializedCaches || (e._surfaceCache = new r.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            var d = Math.max(1024, a), e = Math.max(1024, b);
            c.width = d;
            c.height = e;
            var f = null, f = 512 <= a || 512 <= b ? new r.RegionAllocator.GridAllocator(d, e, d, e) : new r.RegionAllocator.BucketAllocator(d, e);
            return new g.Canvas2DSurface(c, f);
          }), e._shapeCache = new r.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            c.width = 1024;
            c.height = 1024;
            var d = d = new r.RegionAllocator.CompactAllocator(1024, 1024);
            return new g.Canvas2DSurface(c, d);
          }), e._initializedCaches = !0);
        };
        e.prototype.render = function() {
          var a = this._stage, b = this._target, c = this._options, d = this._viewport;
          b.reset();
          b.context.save();
          b.context.beginPath();
          b.context.rect(d.x, d.y, d.w, d.h);
          b.context.clip();
          this._renderStageToTarget(b, a, d);
          b.reset();
          c.paintViewport && (b.context.beginPath(), b.context.rect(d.x, d.y, d.w, d.h), b.context.strokeStyle = "#FF4981", b.context.lineWidth = 2, b.context.stroke());
          b.context.restore();
        };
        e.prototype.renderNode = function(a, b, c) {
          var d = new v(this._target);
          d.clip.set(b);
          d.flags = 256;
          d.matrix.set(c);
          a.visit(this, d);
          d.free();
        };
        e.prototype.renderNodeWithState = function(a, b) {
          a.visit(this, b);
        };
        e.prototype._renderWithCache = function(a, b) {
          var c = b.matrix, d = a.getBounds();
          if (d.isEmpty()) {
            return!1;
          }
          var h = this._options.cacheShapesMaxSize, g = Math.max(c.getAbsoluteScaleX(), c.getAbsoluteScaleY()), l = !!(b.flags & 16), k = !!(b.flags & 8);
          if (b.hasFlags(256)) {
            if (k || l || !b.colorMatrix.isIdentity() || a.hasFlags(1048576) || 100 < this._options.cacheShapesThreshold || d.w * g > h || d.h * g > h) {
              return!1;
            }
            (g = a.properties.mipMap) || (g = a.properties.mipMap = new m(this, a, e._shapeCache, h));
            l = g.getLevel(c);
            h = l.surfaceRegion;
            g = h.region;
            return l ? (l = b.target.context, l.imageSmoothingEnabled = l.mozImageSmoothingEnabled = !0, l.setTransform(c.a, c.b, c.c, c.d, c.tx, c.ty), l.drawImage(h.surface.canvas, g.x, g.y, g.w, g.h, d.x, d.y, d.w, d.h), !0) : !1;
          }
        };
        e.prototype._intersectsClipList = function(a, b) {
          var c = a.getBounds(!0), d = !1;
          b.matrix.transformRectangleAABB(c);
          b.clip.intersects(c) && (d = !0);
          var e = b.clipList;
          if (d && e.length) {
            for (var d = !1, f = 0;f < e.length;f++) {
              if (c.intersects(e[f])) {
                d = !0;
                break;
              }
            }
          }
          c.free();
          return d;
        };
        e.prototype.visitGroup = function(a, b) {
          this._frameInfo.groups++;
          a.getBounds();
          if ((!a.hasFlags(4) || b.flags & 4) && a.hasFlags(65536)) {
            if (b.flags & 1 || 1 === a.getLayer().blendMode && !a.getLayer().mask || !this._options.blending) {
              if (this._intersectsClipList(a, b)) {
                for (var c = null, d = a.getChildren(), e = 0;e < d.length;e++) {
                  var f = d[e], h = b.transform(f.getTransform());
                  h.toggleFlags(4096, f.hasFlags(524288));
                  if (0 <= f.clip) {
                    c = c || new Uint8Array(d.length);
                    c[f.clip + e]++;
                    var g = h.clone();
                    b.target.context.save();
                    g.flags |= 16;
                    f.visit(this, g);
                    g.free();
                  } else {
                    f.visit(this, h);
                  }
                  if (c && 0 < c[e]) {
                    for (;c[e]--;) {
                      b.target.context.restore();
                    }
                  }
                  h.free();
                }
              } else {
                this._frameInfo.culledNodes++;
              }
            } else {
              b = b.clone(), b.flags |= 1, this._renderLayer(a, b), b.free();
            }
            this._renderDebugInfo(a, b);
          }
        };
        e.prototype._renderDebugInfo = function(a, b) {
          if (b.flags & 1024) {
            var c = b.target.context, d = a.getBounds(!0), h = a.properties.style;
            h || (h = a.properties.style = k.Color.randomColor().toCSSStyle());
            c.strokeStyle = h;
            b.matrix.transformRectangleAABB(d);
            c.setTransform(1, 0, 0, 1, 0, 0);
            d.free();
            d = a.getBounds();
            h = e._debugPoints;
            b.matrix.transformRectangle(d, h);
            c.lineWidth = 1;
            c.beginPath();
            c.moveTo(h[0].x, h[0].y);
            c.lineTo(h[1].x, h[1].y);
            c.lineTo(h[2].x, h[2].y);
            c.lineTo(h[3].x, h[3].y);
            c.lineTo(h[0].x, h[0].y);
            c.stroke();
          }
        };
        e.prototype.visitStage = function(a, b) {
          var c = b.target.context, d = a.getBounds(!0);
          b.matrix.transformRectangleAABB(d);
          d.intersect(b.clip);
          b.target.reset();
          b = b.clone();
          this._options.clear && b.target.clear(b.clip);
          a.hasFlags(32768) || !a.color || b.flags & 32 || (this._container.style.backgroundColor = a.color.toCSSStyle());
          this.visitGroup(a, b);
          a.dirtyRegion && (c.restore(), b.target.reset(), c.globalAlpha = .4, b.hasFlags(2048) && a.dirtyRegion.render(b.target.context), a.dirtyRegion.clear());
          b.free();
        };
        e.prototype.visitShape = function(a, b) {
          if (this._intersectsClipList(a, b)) {
            var c = b.matrix;
            b.flags & 8192 && (c = c.clone(), c.snap());
            var d = b.target.context;
            g.Filters._applyColorMatrix(d, b.colorMatrix);
            a.source instanceof r.RenderableVideo ? this.visitRenderableVideo(a.source, b) : 0 < d.globalAlpha && this.visitRenderable(a.source, b, a.ratio);
            b.flags & 8192 && c.free();
          }
        };
        e.prototype.visitRenderableVideo = function(a, b) {
          if (a.video && a.video.videoWidth) {
            var c = this._devicePixelRatio, d = b.matrix.clone();
            d.scale(1 / c, 1 / c);
            var c = a.getBounds(), e = k.GFX.Geometry.Matrix.createIdentity();
            e.scale(c.w / a.video.videoWidth, c.h / a.video.videoHeight);
            d.preMultiply(e);
            e.free();
            c = d.toCSSTransform();
            a.video.style.transformOrigin = "0 0";
            a.video.style.transform = c;
            var f = this._backgroundVideoLayer;
            f !== a.video.parentElement && (f.appendChild(a.video), a.addEventListener(2, function ca(a) {
              f.removeChild(a.video);
              a.removeEventListener(2, ca);
            }));
            d.free();
          }
        };
        e.prototype.visitRenderable = function(a, b, c) {
          var d = a.getBounds();
          if (!(b.flags & 32 || d.isEmpty())) {
            if (b.hasFlags(64)) {
              b.removeFlags(64);
            } else {
              if (this._renderWithCache(a, b)) {
                return;
              }
            }
            var e = b.matrix, d = b.target.context, f = !!(b.flags & 16), h = !!(b.flags & 8);
            d.setTransform(e.a, e.b, e.c, e.d, e.tx, e.ty);
            this._frameInfo.shapes++;
            d.imageSmoothingEnabled = d.mozImageSmoothingEnabled = b.hasFlags(4096);
            b = a.properties.renderCount || 0;
            a.properties.renderCount = ++b;
            a.render(d, c, null, f, h);
          }
        };
        e.prototype._renderLayer = function(a, b) {
          var c = a.getLayer(), d = c.mask;
          if (d) {
            this._renderWithMask(a, d, c.blendMode, !a.hasFlags(131072) || !d.hasFlags(131072), b);
          } else {
            var d = t.allocate(), e = this._renderToTemporarySurface(a, b, d, null);
            e && (b.target.draw(e, d.x, d.y, d.w, d.h, c.blendMode), e.free());
            d.free();
          }
        };
        e.prototype._renderWithMask = function(a, b, c, d, e) {
          var f = b.getTransform().getConcatenatedMatrix(!0);
          b.parent || (f = f.scale(this._devicePixelRatio, this._devicePixelRatio));
          var h = a.getBounds().clone();
          e.matrix.transformRectangleAABB(h);
          h.snap();
          if (!h.isEmpty()) {
            var g = b.getBounds().clone();
            f.transformRectangleAABB(g);
            g.snap();
            if (!g.isEmpty()) {
              var m = e.clip.clone();
              m.intersect(h);
              m.intersect(g);
              m.snap();
              m.isEmpty() || (h = e.clone(), h.clip.set(m), a = this._renderToTemporarySurface(a, h, t.createEmpty(), null), h.free(), h = e.clone(), h.clip.set(m), h.matrix = f, h.flags |= 4, d && (h.flags |= 8), b = this._renderToTemporarySurface(b, h, t.createEmpty(), a.surface), h.free(), a.draw(b, 0, 0, m.w, m.h, 11), e.target.draw(a, m.x, m.y, m.w, m.h, c), b.free(), a.free());
            }
          }
        };
        e.prototype._renderStageToTarget = function(b, c, d) {
          t.allocationCount = a.allocationCount = v.allocationCount = 0;
          b = new v(b);
          b.clip.set(d);
          this._options.paintRenderable || (b.flags |= 32);
          this._options.paintBounds && (b.flags |= 1024);
          this._options.paintDirtyRegion && (b.flags |= 2048);
          this._options.paintFlashing && (b.flags |= 512);
          this._options.cacheShapes && (b.flags |= 256);
          this._options.imageSmoothing && (b.flags |= 4096);
          this._options.snapToDevicePixels && (b.flags |= 8192);
          this._frameInfo.enter(b);
          c.visit(this, b);
          this._frameInfo.leave();
        };
        e.prototype._renderToTemporarySurface = function(a, b, c, d) {
          var e = b.matrix, f = a.getBounds().clone();
          e.transformRectangleAABB(f);
          f.snap();
          c.set(f);
          c.intersect(b.clip);
          c.snap();
          if (c.isEmpty()) {
            return null;
          }
          d = this._allocateSurface(c.w, c.h, d);
          f = d.region;
          f = new t(f.x, f.y, c.w, c.h);
          d.context.setTransform(1, 0, 0, 1, 0, 0);
          d.clear();
          e = e.clone();
          e.translate(f.x - c.x, f.y - c.y);
          d.context.save();
          b = b.clone();
          b.target = d;
          b.matrix = e;
          b.clip.set(f);
          a.visit(this, b);
          b.free();
          d.context.restore();
          return d;
        };
        e.prototype._allocateSurface = function(a, b, c) {
          return e._surfaceCache.allocate(a, b, c);
        };
        e.prototype.screenShot = function(a, b) {
          if (b) {
            var d = this._stage.content.groupChild.child;
            c(d instanceof r.Stage);
            a = d.content.getBounds(!0);
            d.content.getTransform().getConcatenatedMatrix().transformRectangleAABB(a);
            a.intersect(this._viewport);
          }
          a || (a = new t(0, 0, this._target.w, this._target.h));
          d = document.createElement("canvas");
          d.width = a.w;
          d.height = a.h;
          var e = d.getContext("2d");
          e.fillStyle = this._container.style.backgroundColor;
          e.fillRect(0, 0, a.w, a.h);
          e.drawImage(this._target.context.canvas, a.x, a.y, a.w, a.h, 0, 0, a.w, a.h);
          return new r.ScreenShot(d.toDataURL("image/png"), a.w, a.h);
        };
        e._initializedCaches = !1;
        e._debugPoints = n.createEmptyPoints(4);
        return e;
      }(r.Renderer);
      g.Canvas2DRenderer = e;
    })(r.Canvas2D || (r.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = r.Geometry.Point, c = r.Geometry.Matrix, t = r.Geometry.Rectangle, n = k.Tools.Mini.FPS, a = function() {
      function a() {
      }
      a.prototype.onMouseUp = function(a, c) {
        a.state = this;
      };
      a.prototype.onMouseDown = function(a, c) {
        a.state = this;
      };
      a.prototype.onMouseMove = function(a, c) {
        a.state = this;
      };
      a.prototype.onMouseWheel = function(a, c) {
        a.state = this;
      };
      a.prototype.onMouseClick = function(a, c) {
        a.state = this;
      };
      a.prototype.onKeyUp = function(a, c) {
        a.state = this;
      };
      a.prototype.onKeyDown = function(a, c) {
        a.state = this;
      };
      a.prototype.onKeyPress = function(a, c) {
        a.state = this;
      };
      return a;
    }();
    r.UIState = a;
    var h = function(a) {
      function c() {
        a.apply(this, arguments);
        this._keyCodes = [];
      }
      __extends(c, a);
      c.prototype.onMouseDown = function(a, c) {
        c.altKey && (a.state = new l(a.worldView, a.getMousePosition(c, null), a.worldView.getTransform().getMatrix(!0)));
      };
      c.prototype.onMouseClick = function(a, c) {
      };
      c.prototype.onKeyDown = function(a, c) {
        this._keyCodes[c.keyCode] = !0;
      };
      c.prototype.onKeyUp = function(a, c) {
        this._keyCodes[c.keyCode] = !1;
      };
      return c;
    }(a), p = function(a) {
      function c() {
        a.apply(this, arguments);
        this._keyCodes = [];
        this._paused = !1;
        this._mousePosition = new g(0, 0);
      }
      __extends(c, a);
      c.prototype.onMouseMove = function(a, c) {
        this._mousePosition = a.getMousePosition(c, null);
        this._update(a);
      };
      c.prototype.onMouseDown = function(a, c) {
      };
      c.prototype.onMouseClick = function(a, c) {
      };
      c.prototype.onMouseWheel = function(a, c) {
        var d = "DOMMouseScroll" === c.type ? -c.detail : c.wheelDelta / 40;
        if (c.altKey) {
          c.preventDefault();
          var e = a.getMousePosition(c, null), b = a.worldView.getTransform().getMatrix(!0), d = 1 + d / 1E3;
          b.translate(-e.x, -e.y);
          b.scale(d, d);
          b.translate(e.x, e.y);
          a.worldView.getTransform().setMatrix(b);
        }
      };
      c.prototype.onKeyPress = function(a, c) {
        if (c.altKey) {
          var d = c.keyCode || c.which;
          console.info("onKeyPress Code: " + d);
          switch(d) {
            case 248:
              this._paused = !this._paused;
              c.preventDefault();
              break;
            case 223:
              a.toggleOption("paintRenderable");
              c.preventDefault();
              break;
            case 8730:
              a.toggleOption("paintViewport");
              c.preventDefault();
              break;
            case 8747:
              a.toggleOption("paintBounds");
              c.preventDefault();
              break;
            case 8706:
              a.toggleOption("paintDirtyRegion");
              c.preventDefault();
              break;
            case 231:
              a.toggleOption("clear");
              c.preventDefault();
              break;
            case 402:
              a.toggleOption("paintFlashing"), c.preventDefault();
          }
          this._update(a);
        }
      };
      c.prototype.onKeyDown = function(a, c) {
        this._keyCodes[c.keyCode] = !0;
        this._update(a);
      };
      c.prototype.onKeyUp = function(a, c) {
        this._keyCodes[c.keyCode] = !1;
        this._update(a);
      };
      c.prototype._update = function(a) {
        a.paused = this._paused;
        if (a.getOption()) {
          var c = r.viewportLoupeDiameter.value, d = r.viewportLoupeDiameter.value;
          a.viewport = new t(this._mousePosition.x - c / 2, this._mousePosition.y - d / 2, c, d);
        } else {
          a.viewport = null;
        }
      };
      return c;
    }(a);
    (function(a) {
      function c() {
        a.apply(this, arguments);
        this._startTime = Date.now();
      }
      __extends(c, a);
      c.prototype.onMouseMove = function(a, c) {
        if (!(10 > Date.now() - this._startTime)) {
          var d = a._world;
          d && (a.state = new l(d, a.getMousePosition(c, null), d.getTransform().getMatrix(!0)));
        }
      };
      c.prototype.onMouseUp = function(a, c) {
        a.state = new h;
        a.selectNodeUnderMouse(c);
      };
      return c;
    })(a);
    var l = function(a) {
      function c(h, g, d) {
        a.call(this);
        this._target = h;
        this._startPosition = g;
        this._startMatrix = d;
      }
      __extends(c, a);
      c.prototype.onMouseMove = function(a, c) {
        c.preventDefault();
        var d = a.getMousePosition(c, null);
        d.sub(this._startPosition);
        this._target.getTransform().setMatrix(this._startMatrix.clone().translate(d.x, d.y));
        a.state = this;
      };
      c.prototype.onMouseUp = function(a, c) {
        a.state = new h;
      };
      return c;
    }(a), a = function() {
      function a(c, g, l) {
        function d(a) {
          f._state.onMouseWheel(f, a);
          f._persistentState.onMouseWheel(f, a);
        }
        void 0 === g && (g = !1);
        void 0 === l && (l = void 0);
        this._state = new h;
        this._persistentState = new p;
        this.paused = !1;
        this.viewport = null;
        this._selectedNodes = [];
        this._eventListeners = Object.create(null);
        this._fullScreen = !1;
        this._container = c;
        this._stage = new r.Stage(512, 512, !0);
        this._worldView = this._stage.content;
        this._world = new r.Group;
        this._worldView.addChild(this._world);
        this._disableHiDPI = g;
        g = document.createElement("div");
        g.style.position = "absolute";
        g.style.width = "100%";
        g.style.height = "100%";
        g.style.zIndex = "0";
        c.appendChild(g);
        if (r.hud.value) {
          var e = document.createElement("div");
          e.style.position = "absolute";
          e.style.width = "100%";
          e.style.height = "100%";
          e.style.pointerEvents = "none";
          var b = document.createElement("div");
          b.style.position = "absolute";
          b.style.width = "100%";
          b.style.height = "20px";
          b.style.pointerEvents = "none";
          e.appendChild(b);
          c.appendChild(e);
          this._fps = new n(b);
        } else {
          this._fps = null;
        }
        this.transparent = e = 0 === l;
        void 0 === l || 0 === l || k.ColorUtilities.rgbaToCSSStyle(l);
        this._options = new r.Canvas2D.Canvas2DRendererOptions;
        this._options.alpha = e;
        this._renderer = new r.Canvas2D.Canvas2DRenderer(g, this._stage, this._options);
        this._listenForContainerSizeChanges();
        this._onMouseUp = this._onMouseUp.bind(this);
        this._onMouseDown = this._onMouseDown.bind(this);
        this._onMouseMove = this._onMouseMove.bind(this);
        var f = this;
        window.addEventListener("mouseup", function(a) {
          f._state.onMouseUp(f, a);
          f._render();
        }, !1);
        window.addEventListener("mousemove", function(a) {
          f._state.onMouseMove(f, a);
          f._persistentState.onMouseMove(f, a);
        }, !1);
        window.addEventListener("DOMMouseScroll", d);
        window.addEventListener("mousewheel", d);
        c.addEventListener("mousedown", function(a) {
          f._state.onMouseDown(f, a);
        });
        window.addEventListener("keydown", function(a) {
          f._state.onKeyDown(f, a);
          if (f._state !== f._persistentState) {
            f._persistentState.onKeyDown(f, a);
          }
        }, !1);
        window.addEventListener("keypress", function(a) {
          f._state.onKeyPress(f, a);
          if (f._state !== f._persistentState) {
            f._persistentState.onKeyPress(f, a);
          }
        }, !1);
        window.addEventListener("keyup", function(a) {
          f._state.onKeyUp(f, a);
          if (f._state !== f._persistentState) {
            f._persistentState.onKeyUp(f, a);
          }
        }, !1);
        this._enterRenderLoop();
      }
      a.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, c = this._containerHeight;
        this._onContainerSizeChanged();
        var h = this;
        setInterval(function() {
          if (a !== h._containerWidth || c !== h._containerHeight) {
            h._onContainerSizeChanged(), a = h._containerWidth, c = h._containerHeight;
          }
        }, 10);
      };
      a.prototype._onContainerSizeChanged = function() {
        var a = this.getRatio(), h = Math.ceil(this._containerWidth * a), g = Math.ceil(this._containerHeight * a);
        this._stage.setBounds(new t(0, 0, h, g));
        this._stage.content.setBounds(new t(0, 0, h, g));
        this._worldView.getTransform().setMatrix(new c(a, 0, 0, a, 0, 0));
        this._dispatchEvent("resize");
      };
      a.prototype.addEventListener = function(a, c) {
        this._eventListeners[a] || (this._eventListeners[a] = []);
        this._eventListeners[a].push(c);
      };
      a.prototype._dispatchEvent = function(a) {
        if (a = this._eventListeners[a]) {
          for (var c = 0;c < a.length;c++) {
            a[c]();
          }
        }
      };
      a.prototype._enterRenderLoop = function() {
        var a = this;
        requestAnimationFrame(function v() {
          a.render();
          requestAnimationFrame(v);
        });
      };
      Object.defineProperty(a.prototype, "state", {set:function(a) {
        this._state = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "cursor", {set:function(a) {
        this._container.style.cursor = a;
      }, enumerable:!0, configurable:!0});
      a.prototype._render = function() {
        r.RenderableVideo.checkForVideoUpdates();
        var a = (this._stage.readyToRender() || r.forcePaint.value) && !this.paused, c = 0;
        if (a) {
          var h = this._renderer;
          h.viewport = this.viewport ? this.viewport : this._stage.getBounds();
          this._dispatchEvent("render");
          c = performance.now();
          h.render();
          c = performance.now() - c;
        }
        this._fps && this._fps.tickAndRender(!a, c);
      };
      a.prototype.render = function() {
        this._render();
      };
      Object.defineProperty(a.prototype, "world", {get:function() {
        return this._world;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "worldView", {get:function() {
        return this._worldView;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "stage", {get:function() {
        return this._stage;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "options", {get:function() {
        return this._options;
      }, enumerable:!0, configurable:!0});
      a.prototype.getDisplayParameters = function() {
        return{stageWidth:this._containerWidth, stageHeight:this._containerHeight, pixelRatio:this.getRatio(), screenWidth:window.screen ? window.screen.width : 640, screenHeight:window.screen ? window.screen.height : 480};
      };
      a.prototype.toggleOption = function(a) {
        var c = this._options;
        c[a] = !c[a];
      };
      a.prototype.getOption = function() {
        return this._options.paintViewport;
      };
      a.prototype.getRatio = function() {
        var a = window.devicePixelRatio || 1, c = 1;
        1 === a || this._disableHiDPI || (c = a / 1);
        return c;
      };
      Object.defineProperty(a.prototype, "_containerWidth", {get:function() {
        return this._container.clientWidth;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "_containerHeight", {get:function() {
        return this._container.clientHeight;
      }, enumerable:!0, configurable:!0});
      a.prototype.queryNodeUnderMouse = function(a) {
        return this._world;
      };
      a.prototype.selectNodeUnderMouse = function(a) {
        (a = this._world) && this._selectedNodes.push(a);
        this._render();
      };
      a.prototype.getMousePosition = function(a, h) {
        var l = this._container, d = l.getBoundingClientRect(), e = this.getRatio(), l = new g(l.scrollWidth / d.width * (a.clientX - d.left) * e, l.scrollHeight / d.height * (a.clientY - d.top) * e);
        if (!h) {
          return l;
        }
        d = c.createIdentity();
        h.getTransform().getConcatenatedMatrix().inverse(d);
        d.transformPoint(l);
        return l;
      };
      a.prototype.getMouseWorldPosition = function(a) {
        return this.getMousePosition(a, this._world);
      };
      a.prototype._onMouseDown = function(a) {
      };
      a.prototype._onMouseUp = function(a) {
      };
      a.prototype._onMouseMove = function(a) {
      };
      a.prototype.screenShot = function(a, c) {
        return this._renderer.screenShot(a, c);
      };
      return a;
    }();
    r.Easel = a;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = k.GFX.Geometry.Matrix;
    (function(c) {
      c[c.Simple = 0] = "Simple";
    })(r.Layout || (r.Layout = {}));
    var c = function(c) {
      function a() {
        c.apply(this, arguments);
        this.layout = 0;
      }
      __extends(a, c);
      return a;
    }(r.RendererOptions);
    r.TreeRendererOptions = c;
    var t = function(k) {
      function a(a, g, l) {
        void 0 === l && (l = new c);
        k.call(this, a, g, l);
        this._canvas = document.createElement("canvas");
        this._container.appendChild(this._canvas);
        this._context = this._canvas.getContext("2d");
        this._listenForContainerSizeChanges();
      }
      __extends(a, k);
      a.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, c = this._containerHeight;
        this._onContainerSizeChanged();
        var g = this;
        setInterval(function() {
          if (a !== g._containerWidth || c !== g._containerHeight) {
            g._onContainerSizeChanged(), a = g._containerWidth, c = g._containerHeight;
          }
        }, 10);
      };
      a.prototype._getRatio = function() {
        var a = window.devicePixelRatio || 1, c = 1;
        1 !== a && (c = a / 1);
        return c;
      };
      a.prototype._onContainerSizeChanged = function() {
        var a = this._getRatio(), c = Math.ceil(this._containerWidth * a), g = Math.ceil(this._containerHeight * a), k = this._canvas;
        0 < a ? (k.width = c * a, k.height = g * a, k.style.width = c + "px", k.style.height = g + "px") : (k.width = c, k.height = g);
      };
      Object.defineProperty(a.prototype, "_containerWidth", {get:function() {
        return this._container.clientWidth;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "_containerHeight", {get:function() {
        return this._container.clientHeight;
      }, enumerable:!0, configurable:!0});
      a.prototype.render = function() {
        var a = this._context;
        a.save();
        a.clearRect(0, 0, this._canvas.width, this._canvas.height);
        a.scale(1, 1);
        0 === this._options.layout && this._renderNodeSimple(this._context, this._stage, g.createIdentity());
        a.restore();
      };
      a.prototype._renderNodeSimple = function(a, c, g) {
        function k(b) {
          var c = b.getChildren();
          a.fillStyle = b.hasFlags(16) ? "red" : "white";
          var g = String(b.id);
          b instanceof r.RenderableText ? g = "T" + g : b instanceof r.RenderableShape ? g = "S" + g : b instanceof r.RenderableBitmap ? g = "B" + g : b instanceof r.RenderableVideo && (g = "V" + g);
          b instanceof r.Renderable && (g = g + " [" + b._parents.length + "]");
          b = a.measureText(g).width;
          a.fillText(g, n, v);
          if (c) {
            n += b + 4;
            e = Math.max(e, n + 20);
            for (g = 0;g < c.length;g++) {
              k(c[g]), g < c.length - 1 && (v += 18, v > m._canvas.height && (a.fillStyle = "gray", n = n - d + e + 8, d = e + 8, v = 0, a.fillStyle = "white"));
            }
            n -= b + 4;
          }
        }
        var m = this;
        a.save();
        a.font = "16px Arial";
        a.fillStyle = "white";
        var n = 0, v = 0, d = 0, e = 0;
        k(c);
        a.restore();
      };
      return a;
    }(r.Renderer);
    r.TreeRenderer = t;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.GFX.BlurFilter, t = k.GFX.DropshadowFilter, n = k.GFX.Shape, a = k.GFX.Group, h = k.GFX.RenderableShape, p = k.GFX.RenderableMorphShape, l = k.GFX.RenderableBitmap, u = k.GFX.RenderableVideo, m = k.GFX.RenderableText, s = k.GFX.ColorMatrix, v = k.ShapeData, d = k.ArrayUtilities.DataBuffer, e = k.GFX.Stage, b = k.GFX.Geometry.Matrix, f = k.GFX.Geometry.Rectangle, q = function() {
        function a() {
        }
        a.prototype.writeMouseEvent = function(a, b) {
          var c = this.output;
          c.writeInt(300);
          var d = k.Remoting.MouseEventNames.indexOf(a.type);
          c.writeInt(d);
          c.writeFloat(b.x);
          c.writeFloat(b.y);
          c.writeInt(a.buttons);
          c.writeInt((a.ctrlKey ? 1 : 0) | (a.altKey ? 2 : 0) | (a.shiftKey ? 4 : 0));
        };
        a.prototype.writeKeyboardEvent = function(a) {
          var b = this.output;
          b.writeInt(301);
          var c = k.Remoting.KeyboardEventNames.indexOf(a.type);
          b.writeInt(c);
          b.writeInt(a.keyCode);
          b.writeInt(a.charCode);
          b.writeInt(a.location);
          b.writeInt((a.ctrlKey ? 1 : 0) | (a.altKey ? 2 : 0) | (a.shiftKey ? 4 : 0));
        };
        a.prototype.writeFocusEvent = function(a) {
          var b = this.output;
          b.writeInt(302);
          b.writeInt(a);
        };
        return a;
      }();
      g.GFXChannelSerializer = q;
      q = function() {
        function a(b, c, d) {
          function f(a) {
            a = a.getBounds(!0);
            var c = b.easel.getRatio();
            a.scale(1 / c, 1 / c);
            a.snap();
            g.setBounds(a);
          }
          var g = this.stage = new e(128, 512);
          "undefined" !== typeof registerInspectorStage && registerInspectorStage(g);
          f(b.stage);
          b.stage.addEventListener(1, f);
          b.content = g.content;
          d && this.stage.setFlags(32768);
          c.addChild(this.stage);
          this._nodes = [];
          this._assets = [];
          this._easelHost = b;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
        }
        a.prototype._registerAsset = function(a, b, c) {
          "undefined" !== typeof registerInspectorAsset && registerInspectorAsset(a, b, c);
          this._assets[a] = c;
        };
        a.prototype._makeNode = function(a) {
          if (-1 === a) {
            return null;
          }
          var b = null;
          return b = a & 134217728 ? this._assets[a & -134217729].wrap() : this._nodes[a];
        };
        a.prototype._getAsset = function(a) {
          return this._assets[a];
        };
        a.prototype._getBitmapAsset = function(a) {
          return this._assets[a];
        };
        a.prototype._getVideoAsset = function(a) {
          return this._assets[a];
        };
        a.prototype._getTextAsset = function(a) {
          return this._assets[a];
        };
        a.prototype.registerFont = function(a, b, c) {
          k.registerCSSFont(a, b.data, !inFirefox);
          inFirefox ? c(null) : window.setTimeout(c, 400);
        };
        a.prototype.registerImage = function(a, b, c, d) {
          this._registerAsset(a, b, this._decodeImage(c.dataType, c.data, d));
        };
        a.prototype.registerVideo = function(a) {
          this._registerAsset(a, 0, new u(a, this));
        };
        a.prototype._decodeImage = function(a, b, c) {
          var d = new Image, e = l.FromImage(d);
          d.src = URL.createObjectURL(new Blob([b], {type:k.getMIMETypeForImageType(a)}));
          d.onload = function() {
            e.setBounds(new f(0, 0, d.width, d.height));
            e.invalidate();
            c({width:d.width, height:d.height});
          };
          d.onerror = function() {
            c(null);
          };
          return e;
        };
        a.prototype.sendVideoPlaybackEvent = function(a, b, c) {
          this._easelHost.sendVideoPlaybackEvent(a, b, c);
        };
        return a;
      }();
      g.GFXChannelDeserializerContext = q;
      q = function() {
        function e() {
        }
        e.prototype.read = function() {
          for (var a = 0, b = this.input, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0, l = 0, k = 0;0 < b.bytesAvailable;) {
            switch(a = b.readInt(), a) {
              case 0:
                return;
              case 101:
                c++;
                this._readUpdateGraphics();
                break;
              case 102:
                d++;
                this._readUpdateBitmapData();
                break;
              case 103:
                e++;
                this._readUpdateTextContent();
                break;
              case 100:
                f++;
                this._readUpdateFrame();
                break;
              case 104:
                g++;
                this._readUpdateStage();
                break;
              case 105:
                h++;
                this._readUpdateNetStream();
                break;
              case 200:
                l++;
                this._readDrawToBitmap();
                break;
              case 106:
                k++, this._readRequestBitmapData();
            }
          }
        };
        e.prototype._readMatrix = function() {
          var a = this.input, b = e._temporaryReadMatrix;
          b.setElements(a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat() / 20, a.readFloat() / 20);
          return b;
        };
        e.prototype._readRectangle = function() {
          var a = this.input, b = e._temporaryReadRectangle;
          b.setElements(a.readInt() / 20, a.readInt() / 20, a.readInt() / 20, a.readInt() / 20);
          return b;
        };
        e.prototype._readColorMatrix = function() {
          var a = this.input, b = e._temporaryReadColorMatrix, c = 1, d = 1, f = 1, g = 1, h = 0, l = 0, k = 0, m = 0;
          switch(a.readInt()) {
            case 0:
              return e._temporaryReadColorMatrixIdentity;
            case 1:
              g = a.readFloat();
              break;
            case 2:
              c = a.readFloat(), d = a.readFloat(), f = a.readFloat(), g = a.readFloat(), h = a.readInt(), l = a.readInt(), k = a.readInt(), m = a.readInt();
          }
          b.setMultipliersAndOffsets(c, d, f, g, h, l, k, m);
          return b;
        };
        e.prototype._readAsset = function() {
          var a = this.input.readInt(), b = this.inputAssets[a];
          this.inputAssets[a] = null;
          return b;
        };
        e.prototype._readUpdateGraphics = function() {
          for (var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getAsset(c), f = this._readRectangle(), g = v.FromPlainObject(this._readAsset()), l = a.readInt(), k = [], m = 0;m < l;m++) {
            var n = a.readInt();
            k.push(b._getBitmapAsset(n));
          }
          if (e) {
            e.update(g, k, f);
          } else {
            a = g.morphCoordinates ? new p(c, g, k, f) : new h(c, g, k, f);
            for (m = 0;m < k.length;m++) {
              k[m] && k[m].addRenderableParent(a);
            }
            b._registerAsset(c, d, a);
          }
        };
        e.prototype._readUpdateBitmapData = function() {
          var a = this.input, b = this.context, c = a.readInt(), e = a.readInt(), f = b._getBitmapAsset(c), g = this._readRectangle(), a = a.readInt(), h = d.FromPlainObject(this._readAsset());
          f ? f.updateFromDataBuffer(a, h) : (f = l.FromDataBuffer(a, h, g), b._registerAsset(c, e, f));
        };
        e.prototype._readUpdateTextContent = function() {
          var a = this.input, b = this.context, c = a.readInt(), e = a.readInt(), f = b._getTextAsset(c), g = this._readRectangle(), h = this._readMatrix(), l = a.readInt(), k = a.readInt(), p = a.readInt(), n = a.readBoolean(), q = a.readInt(), r = a.readInt(), s = this._readAsset(), v = d.FromPlainObject(this._readAsset()), t = null, u = a.readInt();
          u && (t = new d(4 * u), a.readBytes(t, 4 * u));
          f ? (f.setBounds(g), f.setContent(s, v, h, t), f.setStyle(l, k, q, r), f.reflow(p, n)) : (f = new m(g), f.setContent(s, v, h, t), f.setStyle(l, k, q, r), f.reflow(p, n), b._registerAsset(c, e, f));
          if (this.output) {
            for (a = f.textRect, this.output.writeInt(20 * a.w), this.output.writeInt(20 * a.h), this.output.writeInt(20 * a.x), f = f.lines, a = f.length, this.output.writeInt(a), b = 0;b < a;b++) {
              this._writeLineMetrics(f[b]);
            }
          }
        };
        e.prototype._writeLineMetrics = function(a) {
          this.output.writeInt(a.x);
          this.output.writeInt(a.width);
          this.output.writeInt(a.ascent);
          this.output.writeInt(a.descent);
          this.output.writeInt(a.leading);
        };
        e.prototype._readUpdateStage = function() {
          var a = this.context, b = this.input.readInt();
          a._nodes[b] || (a._nodes[b] = a.stage.content);
          var b = this.input.readInt(), c = this._readRectangle();
          a.stage.content.setBounds(c);
          a.stage.color = k.Color.FromARGB(b);
          a.stage.align = this.input.readInt();
          a.stage.scaleMode = this.input.readInt();
          b = this.input.readInt();
          this.input.readInt();
          c = this.input.readInt();
          a._easelHost.cursor = k.UI.toCSSCursor(c);
          a._easelHost.fullscreen = 0 === b || 1 === b;
        };
        e.prototype._readUpdateNetStream = function() {
          var a = this.context, b = this.input.readInt(), c = a._getVideoAsset(b), d = this._readRectangle();
          c || (a.registerVideo(b), c = a._getVideoAsset(b));
          c.setBounds(d);
        };
        e.prototype._readFilters = function(a) {
          var b = this.input, d = b.readInt(), e = [];
          if (d) {
            for (var f = 0;f < d;f++) {
              var g = b.readInt();
              switch(g) {
                case 0:
                  e.push(new c(b.readFloat(), b.readFloat(), b.readInt()));
                  break;
                case 1:
                  e.push(new t(b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat(), b.readInt(), b.readFloat(), b.readBoolean(), b.readBoolean(), b.readBoolean(), b.readInt(), b.readFloat()));
                  break;
                default:
                  k.Debug.somewhatImplemented(r.FilterType[g]);
              }
            }
            a.getLayer().filters = e;
          }
        };
        e.prototype._readUpdateFrame = function() {
          var b = this.input, c = this.context, d = b.readInt(), e = 0, f = c._nodes[d];
          f || (f = c._nodes[d] = new a);
          d = b.readInt();
          d & 1 && f.getTransform().setMatrix(this._readMatrix());
          d & 8 && f.getTransform().setColorMatrix(this._readColorMatrix());
          if (d & 64) {
            var g = b.readInt();
            0 <= g && (f.getLayer().mask = c._makeNode(g));
          }
          d & 128 && (f.clip = b.readInt());
          d & 32 && (e = b.readInt() / 65535, g = b.readInt(), 1 !== g && (f.getLayer().blendMode = g), this._readFilters(f), f.toggleFlags(65536, b.readBoolean()), f.toggleFlags(131072, b.readBoolean()), f.toggleFlags(262144, !!b.readInt()), f.toggleFlags(524288, !!b.readInt()));
          if (d & 4) {
            d = b.readInt();
            g = f;
            g.clearChildren();
            for (var h = 0;h < d;h++) {
              var l = b.readInt(), l = c._makeNode(l);
              g.addChild(l);
            }
          }
          e && (l = f.getChildren()[0], l instanceof n && (l.ratio = e));
        };
        e.prototype._readDrawToBitmap = function() {
          var a = this.input, c = this.context, d = a.readInt(), e = a.readInt(), f = a.readInt(), g, h, k;
          g = f & 1 ? this._readMatrix().clone() : b.createIdentity();
          f & 8 && (h = this._readColorMatrix());
          f & 16 && (k = this._readRectangle());
          f = a.readInt();
          a.readBoolean();
          a = c._getBitmapAsset(d);
          e = c._makeNode(e);
          a ? a.drawNode(e, g, h, f, k) : c._registerAsset(d, -1, l.FromNode(e, g, h, f, k));
        };
        e.prototype._readRequestBitmapData = function() {
          var a = this.output, b = this.context, c = this.input.readInt();
          b._getBitmapAsset(c).readImageData(a);
        };
        e._temporaryReadMatrix = b.createIdentity();
        e._temporaryReadRectangle = f.createEmpty();
        e._temporaryReadColorMatrix = s.createIdentity();
        e._temporaryReadColorMatrixIdentity = s.createIdentity();
        return e;
      }();
      g.GFXChannelDeserializer = q;
    })(r.GFX || (r.GFX = {}));
  })(k.Remoting || (k.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = k.GFX.Geometry.Point, c = k.ArrayUtilities.DataBuffer, t = function() {
      function n(a) {
        this._easel = a;
        var c = a.transparent;
        this._group = a.world;
        this._content = null;
        this._fullscreen = !1;
        this._context = new k.Remoting.GFX.GFXChannelDeserializerContext(this, this._group, c);
        this._addEventListeners();
      }
      n.prototype.onSendUpdates = function(a, c) {
        throw Error("This method is abstract");
      };
      Object.defineProperty(n.prototype, "easel", {get:function() {
        return this._easel;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(n.prototype, "stage", {get:function() {
        return this._easel.stage;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(n.prototype, "content", {set:function(a) {
        this._content = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(n.prototype, "cursor", {set:function(a) {
        this._easel.cursor = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(n.prototype, "fullscreen", {set:function(a) {
        this._fullscreen !== a && (this._fullscreen = a, "undefined" !== typeof ShumwayCom && ShumwayCom.setFullscreen && ShumwayCom.setFullscreen(a));
      }, enumerable:!0, configurable:!0});
      n.prototype._mouseEventListener = function(a) {
        var h = this._easel.getMousePosition(a, this._content), h = new g(h.x, h.y), p = new c, l = new k.Remoting.GFX.GFXChannelSerializer;
        l.output = p;
        l.writeMouseEvent(a, h);
        this.onSendUpdates(p, []);
      };
      n.prototype._keyboardEventListener = function(a) {
        var g = new c, p = new k.Remoting.GFX.GFXChannelSerializer;
        p.output = g;
        p.writeKeyboardEvent(a);
        this.onSendUpdates(g, []);
      };
      n.prototype._addEventListeners = function() {
        for (var a = this._mouseEventListener.bind(this), c = this._keyboardEventListener.bind(this), g = n._mouseEvents, l = 0;l < g.length;l++) {
          window.addEventListener(g[l], a);
        }
        a = n._keyboardEvents;
        for (l = 0;l < a.length;l++) {
          window.addEventListener(a[l], c);
        }
        this._addFocusEventListeners();
        this._easel.addEventListener("resize", this._resizeEventListener.bind(this));
      };
      n.prototype._sendFocusEvent = function(a) {
        var g = new c, p = new k.Remoting.GFX.GFXChannelSerializer;
        p.output = g;
        p.writeFocusEvent(a);
        this.onSendUpdates(g, []);
      };
      n.prototype._addFocusEventListeners = function() {
        var a = this;
        document.addEventListener("visibilitychange", function(c) {
          a._sendFocusEvent(document.hidden ? 0 : 1);
        });
        window.addEventListener("focus", function(c) {
          a._sendFocusEvent(3);
        });
        window.addEventListener("blur", function(c) {
          a._sendFocusEvent(2);
        });
      };
      n.prototype._resizeEventListener = function() {
        this.onDisplayParameters(this._easel.getDisplayParameters());
      };
      n.prototype.onDisplayParameters = function(a) {
        throw Error("This method is abstract");
      };
      n.prototype.processUpdates = function(a, c, g) {
        void 0 === g && (g = null);
        var l = new k.Remoting.GFX.GFXChannelDeserializer;
        l.input = a;
        l.inputAssets = c;
        l.output = g;
        l.context = this._context;
        l.read();
      };
      n.prototype.processVideoControl = function(a, c, g) {
        var l = this._context, k = l._getVideoAsset(a);
        if (!k) {
          if (1 !== c) {
            return;
          }
          l.registerVideo(a);
          k = l._getVideoAsset(a);
        }
        return k.processControlRequest(c, g);
      };
      n.prototype.processRegisterFontOrImage = function(a, c, g, l, k) {
        "font" === g ? this._context.registerFont(a, l, k) : this._context.registerImage(a, c, l, k);
      };
      n.prototype.processFSCommand = function(a, c) {
      };
      n.prototype.processFrame = function() {
      };
      n.prototype.onVideoPlaybackEvent = function(a, c, g) {
        throw Error("This method is abstract");
      };
      n.prototype.sendVideoPlaybackEvent = function(a, c, g) {
        this.onVideoPlaybackEvent(a, c, g);
      };
      n._mouseEvents = k.Remoting.MouseEventNames;
      n._keyboardEvents = k.Remoting.KeyboardEventNames;
      return n;
    }();
    r.EaselHost = t;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.ArrayUtilities.DataBuffer, t = k.CircularBuffer, n = k.Tools.Profiler.TimelineBuffer, a = function(a) {
        function g(c, k, m) {
          a.call(this, c);
          this._timelineRequests = Object.create(null);
          this._playerWindow = k;
          this._window = m;
          this._window.addEventListener("message", function(a) {
            this.onWindowMessage(a.data);
          }.bind(this));
          "undefined" !== typeof ShumwayCom ? ShumwayCom.setSyncMessageCallback(function(a) {
            this.onWindowMessage(a, !1);
            return a.result;
          }.bind(this)) : this._window.addEventListener("syncmessage", function(a) {
            this.onWindowMessage(a.detail, !1);
          }.bind(this));
        }
        __extends(g, a);
        g.prototype.onSendUpdates = function(a, c) {
          var g = a.getBytes();
          this._playerWindow.postMessage({type:"gfx", updates:g, assets:c}, "*", [g.buffer]);
        };
        g.prototype.onDisplayParameters = function(a) {
          this._playerWindow.postMessage({type:"displayParameters", params:a}, "*");
        };
        g.prototype.onVideoPlaybackEvent = function(a, c, g) {
          this._playerWindow.postMessage({type:"videoPlayback", id:a, eventType:c, data:g}, "*");
        };
        g.prototype.requestTimeline = function(a, c) {
          return new Promise(function(g) {
            this._timelineRequests[a] = g;
            this._playerWindow.postMessage({type:"timeline", cmd:c, request:a}, "*");
          }.bind(this));
        };
        g.prototype._sendRegisterFontOrImageResponse = function(a, c) {
          this._playerWindow.postMessage({type:"registerFontOrImageResponse", requestId:a, result:c}, "*");
        };
        g.prototype.onWindowMessage = function(a, g) {
          void 0 === g && (g = !0);
          if ("object" === typeof a && null !== a) {
            if ("player" === a.type) {
              var h = c.FromArrayBuffer(a.updates.buffer);
              if (g) {
                this.processUpdates(h, a.assets);
              } else {
                var k = new c;
                this.processUpdates(h, a.assets, k);
                a.result = k.toPlainObject();
              }
            } else {
              "frame" !== a.type && ("videoControl" === a.type ? a.result = this.processVideoControl(a.id, a.eventType, a.data) : "registerFontOrImage" === a.type ? this.processRegisterFontOrImage(a.syncId, a.symbolId, a.assetType, a.data, this._sendRegisterFontOrImageResponse.bind(this, a.requestId)) : "fscommand" !== a.type && "timelineResponse" === a.type && a.timeline && (a.timeline.__proto__ = n.prototype, a.timeline._marks.__proto__ = t.prototype, a.timeline._times.__proto__ = t.prototype, 
              this._timelineRequests[a.request](a.timeline)));
            }
          }
        };
        return g;
      }(r.EaselHost);
      g.WindowEaselHost = a;
    })(r.Window || (r.Window = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.ArrayUtilities.DataBuffer, t = function(g) {
        function a(a) {
          g.call(this, a);
          this._worker = k.Player.Test.FakeSyncWorker.instance;
          this._worker.addEventListener("message", this._onWorkerMessage.bind(this));
          this._worker.addEventListener("syncmessage", this._onSyncWorkerMessage.bind(this));
        }
        __extends(a, g);
        a.prototype.onSendUpdates = function(a, c) {
          var g = a.getBytes();
          this._worker.postMessage({type:"gfx", updates:g, assets:c}, [g.buffer]);
        };
        a.prototype.onDisplayParameters = function(a) {
          this._worker.postMessage({type:"displayParameters", params:a});
        };
        a.prototype.onVideoPlaybackEvent = function(a, c, g) {
          this._worker.postMessage({type:"videoPlayback", id:a, eventType:c, data:g});
        };
        a.prototype.requestTimeline = function(a, c) {
          var g;
          switch(a) {
            case "AVM2":
              g = k.AVM2.timelineBuffer;
              break;
            case "Player":
              g = k.Player.timelineBuffer;
              break;
            case "SWF":
              g = k.SWF.timelineBuffer;
          }
          "clear" === c && g && g.reset();
          return Promise.resolve(g);
        };
        a.prototype._sendRegisterFontOrImageResponse = function(a, c) {
          this._worker.postMessage({type:"registerFontOrImageResponse", requestId:a, result:c});
        };
        a.prototype._onWorkerMessage = function(a, g) {
          void 0 === g && (g = !0);
          var k = a.data;
          if ("object" === typeof k && null !== k) {
            switch(k.type) {
              case "player":
                var n = c.FromArrayBuffer(k.updates.buffer);
                if (g) {
                  this.processUpdates(n, k.assets);
                } else {
                  var m = new c;
                  this.processUpdates(n, k.assets, m);
                  a.result = m.toPlainObject();
                  a.handled = !0;
                }
                break;
              case "videoControl":
                a.result = this.processVideoControl(k.id, k.eventType, k.data);
                a.handled = !0;
                break;
              case "registerFontOrImage":
                this.processRegisterFontOrImage(k.syncId, k.symbolId, k.assetType, k.data, this._sendRegisterFontOrImageResponse.bind(this, k.requestId)), a.handled = !0;
            }
          }
        };
        a.prototype._onSyncWorkerMessage = function(a) {
          return this._onWorkerMessage(a, !1);
        };
        return a;
      }(r.EaselHost);
      g.TestEaselHost = t;
    })(r.Test || (r.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      function c(a, c) {
        a.writeInt(c.length);
        a.writeRawBytes(c);
      }
      function r(a) {
        function g(a) {
          switch(typeof a) {
            case "undefined":
              k.writeByte(0);
              break;
            case "boolean":
              k.writeByte(a ? 2 : 3);
              break;
            case "number":
              k.writeByte(4);
              k.writeDouble(a);
              break;
            case "string":
              k.writeByte(5);
              k.writeUTF(a);
              break;
            default:
              if (null === a) {
                k.writeByte(1);
                break;
              }
              if (Array.isArray(a) && a instanceof Int32Array) {
                k.writeByte(6);
                k.writeInt(a.length);
                for (var e = 0;e < a.length;e++) {
                  g(a[e]);
                }
              } else {
                if (a instanceof Uint8Array) {
                  k.writeByte(9), c(k, a);
                } else {
                  if ("length" in a && "buffer" in a && "littleEndian" in a) {
                    k.writeByte(a.littleEndian ? 10 : 11), c(k, new Uint8Array(a.buffer, 0, a.length));
                  } else {
                    if (a instanceof ArrayBuffer) {
                      k.writeByte(8), c(k, new Uint8Array(a));
                    } else {
                      if (a instanceof Int32Array) {
                        k.writeByte(12), c(k, new Uint8Array(a.buffer, a.byteOffset, a.byteLength));
                      } else {
                        if (a.buffer instanceof ArrayBuffer && "number" === typeof a.byteOffset) {
                          throw Error("Some unsupported TypedArray is used");
                        }
                        k.writeByte(7);
                        for (e in a) {
                          k.writeUTF(e), g(a[e]);
                        }
                        k.writeUTF("");
                      }
                    }
                  }
                }
              }
            ;
          }
        }
        var k = new h;
        g(a);
        return k.getBytes();
      }
      function n(a) {
        var c = new h, g = a.readInt();
        a.readBytes(c, g);
        return c.getBytes();
      }
      function a(a) {
        function c() {
          var a = g.readByte();
          switch(a) {
            case 1:
              return null;
            case 2:
              return!0;
            case 3:
              return!1;
            case 4:
              return g.readDouble();
            case 5:
              return g.readUTF();
            case 6:
              for (var a = [], b = g.readInt(), d = 0;d < b;d++) {
                a[d] = c();
              }
              return a;
            case 7:
              for (a = {};b = g.readUTF();) {
                a[b] = c();
              }
              return a;
            case 8:
              return n(g).buffer;
            case 9:
              return n(g);
            case 11:
            ;
            case 10:
              return b = n(g), new p(b.buffer, b.length, 10 === a);
            case 12:
              return new Int32Array(n(g).buffer);
          }
        }
        var g = new h, d = a.readInt();
        a.readBytes(g, d);
        return c();
      }
      var h = k.ArrayUtilities.DataBuffer, p = k.ArrayUtilities.PlainObjectDataBuffer, l;
      (function(a) {
        a[a.Undefined = 0] = "Undefined";
        a[a.Null = 1] = "Null";
        a[a.True = 2] = "True";
        a[a.False = 3] = "False";
        a[a.Number = 4] = "Number";
        a[a.String = 5] = "String";
        a[a.Array = 6] = "Array";
        a[a.Object = 7] = "Object";
        a[a.ArrayBuffer = 8] = "ArrayBuffer";
        a[a.Uint8Array = 9] = "Uint8Array";
        a[a.PlainObjectDataBufferLE = 10] = "PlainObjectDataBufferLE";
        a[a.PlainObjectDataBufferBE = 11] = "PlainObjectDataBufferBE";
        a[a.Int32Array = 12] = "Int32Array";
      })(l || (l = {}));
      (function(a) {
        a[a.None = 0] = "None";
        a[a.PlayerCommand = 1] = "PlayerCommand";
        a[a.PlayerCommandAsync = 2] = "PlayerCommandAsync";
        a[a.Frame = 3] = "Frame";
        a[a.FontOrImage = 4] = "FontOrImage";
        a[a.FSCommand = 5] = "FSCommand";
      })(g.MovieRecordType || (g.MovieRecordType = {}));
      l = function() {
        function a(c) {
          this._maxRecordingSize = c;
          this._recording = new h;
          this._recordingStarted = Date.now();
          this._recording.writeRawBytes(new Uint8Array([77, 83, 87, 70]));
          this._stopped = !1;
        }
        a.prototype.stop = function() {
          this._stopped = !0;
        };
        a.prototype.getRecording = function() {
          return new Blob([this._recording.getBytes()], {type:"application/octet-stream"});
        };
        a.prototype.dump = function() {
          (new u(this._recording.getBytes())).dump();
        };
        a.prototype._createRecord = function(a, c) {
          this._stopped || (this._recording.length + 8 + (c ? c.length : 0) >= this._maxRecordingSize ? (console.error("Recording limit reached"), this._stopped = !0) : (this._recording.writeInt(Date.now() - this._recordingStarted), this._recording.writeInt(a), null !== c ? (this._recording.writeInt(c.length), this._recording.writeRawBytes(c.getBytes())) : this._recording.writeInt(0)));
        };
        a.prototype.recordPlayerCommand = function(a, g, d) {
          var e = new h;
          c(e, g);
          e.writeInt(d.length);
          d.forEach(function(a) {
            a = r(a);
            c(e, a);
          });
          this._createRecord(a ? 2 : 1, e);
        };
        a.prototype.recordFrame = function() {
          this._createRecord(3, null);
        };
        a.prototype.recordFontOrImage = function(a, g, d, e) {
          var b = new h;
          b.writeInt(a);
          b.writeInt(g);
          b.writeUTF(d);
          c(b, r(e));
          this._createRecord(4, b);
        };
        a.prototype.recordFSCommand = function(a, c) {
          var d = new h;
          d.writeUTF(a);
          d.writeUTF(c || "");
          this._createRecord(5, d);
        };
        return a;
      }();
      g.MovieRecorder = l;
      var u = function() {
        function c(a) {
          this._buffer = new h;
          this._buffer.writeRawBytes(a);
          this._buffer.position = 4;
        }
        c.prototype.readNextRecord = function() {
          if (this._buffer.position >= this._buffer.length) {
            return 0;
          }
          var a = this._buffer.readInt(), c = this._buffer.readInt(), d = this._buffer.readInt(), e = null;
          0 < d && (e = new h, this._buffer.readBytes(e, d));
          this.currentTimestamp = a;
          this.currentType = c;
          this.currentData = e;
          return c;
        };
        c.prototype.parsePlayerCommand = function() {
          for (var c = n(this.currentData), g = this.currentData.readInt(), d = [], e = 0;e < g;e++) {
            d.push(a(this.currentData));
          }
          return{updates:c, assets:d};
        };
        c.prototype.parseFSCommand = function() {
          var a = this.currentData.readUTF(), c = this.currentData.readUTF();
          return{command:a, args:c};
        };
        c.prototype.parseFontOrImage = function() {
          var c = this.currentData.readInt(), g = this.currentData.readInt(), d = this.currentData.readUTF(), e = a(this.currentData);
          return{syncId:c, symbolId:g, assetType:d, data:e};
        };
        c.prototype.dump = function() {
          for (var a;a = this.readNextRecord();) {
            console.log("record " + a + " @" + this.currentTimestamp);
            debugger;
            switch(a) {
              case 1:
              ;
              case 2:
                console.log(this.parsePlayerCommand());
                break;
              case 5:
                console.log(this.parseFSCommand());
                break;
              case 4:
                console.log(this.parseFontOrImage());
            }
          }
        };
        return c;
      }();
      g.MovieRecordParser = u;
    })(r.Test || (r.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var c = k.ArrayUtilities.DataBuffer, t = function(k) {
        function a(a) {
          k.call(this, a);
        }
        __extends(a, k);
        a.prototype.playUrl = function(a) {
          var c = new XMLHttpRequest;
          c.open("GET", a, !0);
          c.responseType = "arraybuffer";
          c.onload = function() {
            this.playBytes(new Uint8Array(c.response));
          }.bind(this);
          c.send();
        };
        a.prototype.playBytes = function(a) {
          this._parser = new g.MovieRecordParser(a);
          this._lastTimestamp = 0;
          this._parseNext();
        };
        a.prototype.onSendUpdates = function(a, c) {
        };
        a.prototype.onDisplayParameters = function(a) {
        };
        a.prototype.onVideoPlaybackEvent = function(a, c, g) {
        };
        a.prototype.requestTimeline = function(a, c) {
          return Promise.resolve(void 0);
        };
        a.prototype._parseNext = function() {
          if (0 !== this._parser.readNextRecord()) {
            var a = this._parser.currentTimestamp - this._lastTimestamp;
            this._lastTimestamp = this._parser.currentTimestamp;
            setTimeout(this._runRecord.bind(this), a);
          }
        };
        a.prototype._runRecord = function() {
          var a;
          switch(this._parser.currentType) {
            case 1:
            ;
            case 2:
              a = this._parser.parsePlayerCommand();
              var g = 2 === this._parser.currentType, k = c.FromArrayBuffer(a.updates.buffer);
              g ? this.processUpdates(k, a.assets) : (g = new c, this.processUpdates(k, a.assets, g));
              break;
            case 3:
              break;
            case 4:
              a = this._parser.parseFontOrImage();
              this.processRegisterFontOrImage(a.syncId, a.symbolId, a.assetType, a.data, function() {
              });
              break;
            case 5:
              a = this._parser.parseFSCommand();
              break;
            default:
              throw Error("Invalid movie record type");;
          }
          this._parseNext();
        };
        return a;
      }(r.EaselHost);
      g.PlaybackEaselHost = t;
    })(r.Test || (r.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var c = function(c) {
        function k(a, h) {
          void 0 === h && (h = 0);
          c.call(this, a);
          this._recorder = null;
          this._recorder = new g.MovieRecorder(h);
        }
        __extends(k, c);
        Object.defineProperty(k.prototype, "recorder", {get:function() {
          return this._recorder;
        }, enumerable:!0, configurable:!0});
        k.prototype._onWorkerMessage = function(a, g) {
          void 0 === g && (g = !0);
          var k = a.data;
          if ("object" === typeof k && null !== k) {
            switch(k.type) {
              case "player":
                this._recorder.recordPlayerCommand(g, k.updates, k.assets);
                break;
              case "frame":
                this._recorder.recordFrame();
                break;
              case "registerFontOrImage":
                this._recorder.recordFontOrImage(k.syncId, k.symbolId, k.assetType, k.data);
                break;
              case "fscommand":
                this._recorder.recordFSCommand(k.command, k.args);
            }
            c.prototype._onWorkerMessage.call(this, a, g);
          }
        };
        return k;
      }(g.TestEaselHost);
      g.RecordingEaselHost = c;
    })(k.Test || (k.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load GFX Dependencies");

