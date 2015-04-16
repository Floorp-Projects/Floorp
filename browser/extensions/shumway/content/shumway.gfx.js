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
Shumway$$inline_0.version = "0.10.346";
Shumway$$inline_0.build = "84cafb5";
var jsGlobal = function() {
  return this || (0,eval)("this//# sourceURL=jsGlobal-getter");
}(), inBrowser = "undefined" !== typeof window && "document" in window && "plugins" in window.document, inFirefox = "undefined" !== typeof navigator && 0 <= navigator.userAgent.indexOf("Firefox");
function dumpLine(k) {
}
jsGlobal.performance || (jsGlobal.performance = {});
jsGlobal.performance.now || (jsGlobal.performance.now = "undefined" !== typeof dateNow ? dateNow : Date.now);
function lazyInitializer(k, r, g) {
  Object.defineProperty(k, r, {get:function() {
    var b = g();
    Object.defineProperty(k, r, {value:b, configurable:!0, enumerable:!0});
    return b;
  }, configurable:!0, enumerable:!0});
}
var START_TIME = performance.now();
(function(k) {
  function r(e) {
    return (e | 0) === e;
  }
  function g(e) {
    return "object" === typeof e || "function" === typeof e;
  }
  function b(e) {
    return String(Number(e)) === e;
  }
  function v(e) {
    var f = 0;
    if ("number" === typeof e) {
      return f = e | 0, e === f && 0 <= f ? !0 : e >>> 0 === e;
    }
    if ("string" !== typeof e) {
      return !1;
    }
    var c = e.length;
    if (0 === c) {
      return !1;
    }
    if ("0" === e) {
      return !0;
    }
    if (c > k.UINT32_CHAR_BUFFER_LENGTH) {
      return !1;
    }
    var d = 0, f = e.charCodeAt(d++) - 48;
    if (1 > f || 9 < f) {
      return !1;
    }
    for (var q = 0, u = 0;d < c;) {
      u = e.charCodeAt(d++) - 48;
      if (0 > u || 9 < u) {
        return !1;
      }
      q = f;
      f = 10 * f + u;
    }
    return q < k.UINT32_MAX_DIV_10 || q === k.UINT32_MAX_DIV_10 && u <= k.UINT32_MAX_MOD_10 ? !0 : !1;
  }
  (function(e) {
    e[e._0 = 48] = "_0";
    e[e._1 = 49] = "_1";
    e[e._2 = 50] = "_2";
    e[e._3 = 51] = "_3";
    e[e._4 = 52] = "_4";
    e[e._5 = 53] = "_5";
    e[e._6 = 54] = "_6";
    e[e._7 = 55] = "_7";
    e[e._8 = 56] = "_8";
    e[e._9 = 57] = "_9";
  })(k.CharacterCodes || (k.CharacterCodes = {}));
  k.UINT32_CHAR_BUFFER_LENGTH = 10;
  k.UINT32_MAX = 4294967295;
  k.UINT32_MAX_DIV_10 = 429496729;
  k.UINT32_MAX_MOD_10 = 5;
  k.isString = function(e) {
    return "string" === typeof e;
  };
  k.isFunction = function(e) {
    return "function" === typeof e;
  };
  k.isNumber = function(e) {
    return "number" === typeof e;
  };
  k.isInteger = r;
  k.isArray = function(e) {
    return e instanceof Array;
  };
  k.isNumberOrString = function(e) {
    return "number" === typeof e || "string" === typeof e;
  };
  k.isObject = g;
  k.toNumber = function(e) {
    return +e;
  };
  k.isNumericString = b;
  k.isNumeric = function(e) {
    if ("number" === typeof e) {
      return !0;
    }
    if ("string" === typeof e) {
      var f = e.charCodeAt(0);
      return 65 <= f && 90 >= f || 97 <= f && 122 >= f || 36 === f || 95 === f ? !1 : v(e) || b(e);
    }
    return !1;
  };
  k.isIndex = v;
  k.isNullOrUndefined = function(e) {
    return void 0 == e;
  };
  var n;
  (function(e) {
    e.error = function(c) {
      console.error(c);
      throw Error(c);
    };
    e.assert = function(c, d) {
      void 0 === d && (d = "assertion failed");
      "" === c && (c = !0);
      if (!c) {
        if ("undefined" !== typeof console && "assert" in console) {
          throw console.assert(!1, d), Error(d);
        }
        e.error(d.toString());
      }
    };
    e.assertUnreachable = function(c) {
      throw Error("Reached unreachable location " + Error().stack.split("\n")[1] + c);
    };
    e.assertNotImplemented = function(c, d) {
      c || e.error("notImplemented: " + d);
    };
    e.warning = function(c, d, q) {
    };
    e.notUsed = function(c) {
    };
    e.notImplemented = function(c) {
    };
    e.dummyConstructor = function(c) {
    };
    e.abstractMethod = function(c) {
    };
    var f = {};
    e.somewhatImplemented = function(c) {
      f[c] || (f[c] = !0);
    };
    e.unexpected = function(c) {
      e.assert(!1, "Unexpected: " + c);
    };
    e.unexpectedCase = function(c) {
      e.assert(!1, "Unexpected Case: " + c);
    };
  })(n = k.Debug || (k.Debug = {}));
  k.getTicks = function() {
    return performance.now();
  };
  (function(e) {
    function f(d, c) {
      for (var u = 0, f = d.length;u < f;u++) {
        if (d[u] === c) {
          return u;
        }
      }
      d.push(c);
      return d.length - 1;
    }
    e.popManyInto = function(d, c, u) {
      for (var f = c - 1;0 <= f;f--) {
        u[f] = d.pop();
      }
      u.length = c;
    };
    e.popMany = function(d, c) {
      var u = d.length - c, f = d.slice(u, this.length);
      d.length = u;
      return f;
    };
    e.popManyIntoVoid = function(d, c) {
      d.length -= c;
    };
    e.pushMany = function(d, c) {
      for (var u = 0;u < c.length;u++) {
        d.push(c[u]);
      }
    };
    e.top = function(d) {
      return d.length && d[d.length - 1];
    };
    e.last = function(d) {
      return d.length && d[d.length - 1];
    };
    e.peek = function(d) {
      return d[d.length - 1];
    };
    e.indexOf = function(d, c) {
      for (var u = 0, f = d.length;u < f;u++) {
        if (d[u] === c) {
          return u;
        }
      }
      return -1;
    };
    e.equals = function(d, c) {
      if (d.length !== c.length) {
        return !1;
      }
      for (var u = 0;u < d.length;u++) {
        if (d[u] !== c[u]) {
          return !1;
        }
      }
      return !0;
    };
    e.pushUnique = f;
    e.unique = function(d) {
      for (var c = [], u = 0;u < d.length;u++) {
        f(c, d[u]);
      }
      return c;
    };
    e.copyFrom = function(d, c) {
      d.length = 0;
      e.pushMany(d, c);
    };
    e.ensureTypedArrayCapacity = function(d, c) {
      if (d.length < c) {
        var u = d;
        d = new d.constructor(k.IntegerUtilities.nearestPowerOfTwo(c));
        d.set(u, 0);
      }
      return d;
    };
    e.memCopy = function(d, c, u, f, e) {
      void 0 === u && (u = 0);
      void 0 === f && (f = 0);
      void 0 === e && (e = 0);
      0 < f || 0 < e && e < c.length ? (0 >= e && (e = c.length - f), d.set(c.subarray(f, f + e), u)) : d.set(c, u);
    };
    var c = function() {
      function d(d) {
        void 0 === d && (d = 16);
        this._f32 = this._i32 = this._u16 = this._u8 = null;
        this._offset = 0;
        this.ensureCapacity(d);
      }
      d.prototype.reset = function() {
        this._offset = 0;
      };
      Object.defineProperty(d.prototype, "offset", {get:function() {
        return this._offset;
      }, enumerable:!0, configurable:!0});
      d.prototype.getIndex = function(d) {
        return this._offset / d;
      };
      d.prototype.ensureAdditionalCapacity = function() {
        this.ensureCapacity(this._offset + 68);
      };
      d.prototype.ensureCapacity = function(d) {
        if (!this._u8) {
          this._u8 = new Uint8Array(d);
        } else {
          if (this._u8.length > d) {
            return;
          }
        }
        var c = 2 * this._u8.length;
        c < d && (c = d);
        d = new Uint8Array(c);
        d.set(this._u8, 0);
        this._u8 = d;
        this._u16 = new Uint16Array(d.buffer);
        this._i32 = new Int32Array(d.buffer);
        this._f32 = new Float32Array(d.buffer);
      };
      d.prototype.writeInt = function(d) {
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(d);
      };
      d.prototype.writeIntAt = function(d, c) {
        this.ensureCapacity(c + 4);
        this._i32[c >> 2] = d;
      };
      d.prototype.writeIntUnsafe = function(d) {
        this._i32[this._offset >> 2] = d;
        this._offset += 4;
      };
      d.prototype.writeFloat = function(d) {
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(d);
      };
      d.prototype.writeFloatUnsafe = function(d) {
        this._f32[this._offset >> 2] = d;
        this._offset += 4;
      };
      d.prototype.write4Floats = function(d, c, f, e) {
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(d, c, f, e);
      };
      d.prototype.write4FloatsUnsafe = function(d, c, f, e) {
        var a = this._offset >> 2;
        this._f32[a + 0] = d;
        this._f32[a + 1] = c;
        this._f32[a + 2] = f;
        this._f32[a + 3] = e;
        this._offset += 16;
      };
      d.prototype.write6Floats = function(d, c, f, e, a, h) {
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(d, c, f, e, a, h);
      };
      d.prototype.write6FloatsUnsafe = function(d, c, f, e, a, h) {
        var p = this._offset >> 2;
        this._f32[p + 0] = d;
        this._f32[p + 1] = c;
        this._f32[p + 2] = f;
        this._f32[p + 3] = e;
        this._f32[p + 4] = a;
        this._f32[p + 5] = h;
        this._offset += 24;
      };
      d.prototype.subF32View = function() {
        return this._f32.subarray(0, this._offset >> 2);
      };
      d.prototype.subI32View = function() {
        return this._i32.subarray(0, this._offset >> 2);
      };
      d.prototype.subU16View = function() {
        return this._u16.subarray(0, this._offset >> 1);
      };
      d.prototype.subU8View = function() {
        return this._u8.subarray(0, this._offset);
      };
      d.prototype.hashWords = function(d, c, f) {
        c = this._i32;
        for (var e = 0;e < f;e++) {
          d = (31 * d | 0) + c[e] | 0;
        }
        return d;
      };
      d.prototype.reserve = function(d) {
        d = d + 3 & -4;
        this.ensureCapacity(this._offset + d);
        this._offset += d;
      };
      return d;
    }();
    e.ArrayWriter = c;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
  var a = function() {
    function e(f) {
      this._u8 = new Uint8Array(f);
      this._u16 = new Uint16Array(f);
      this._i32 = new Int32Array(f);
      this._f32 = new Float32Array(f);
      this._offset = 0;
    }
    Object.defineProperty(e.prototype, "offset", {get:function() {
      return this._offset;
    }, enumerable:!0, configurable:!0});
    e.prototype.isEmpty = function() {
      return this._offset === this._u8.length;
    };
    e.prototype.readInt = function() {
      var f = this._i32[this._offset >> 2];
      this._offset += 4;
      return f;
    };
    e.prototype.readFloat = function() {
      var f = this._f32[this._offset >> 2];
      this._offset += 4;
      return f;
    };
    return e;
  }();
  k.ArrayReader = a;
  (function(e) {
    function f(d, c) {
      return Object.prototype.hasOwnProperty.call(d, c);
    }
    function c(d, c) {
      for (var u in c) {
        f(c, u) && (d[u] = c[u]);
      }
    }
    e.boxValue = function(d) {
      return void 0 == d || g(d) ? d : Object(d);
    };
    e.toKeyValueArray = function(d) {
      var c = Object.prototype.hasOwnProperty, f = [], e;
      for (e in d) {
        c.call(d, e) && f.push([e, d[e]]);
      }
      return f;
    };
    e.isPrototypeWriteable = function(d) {
      return Object.getOwnPropertyDescriptor(d, "prototype").writable;
    };
    e.hasOwnProperty = f;
    e.propertyIsEnumerable = function(d, c) {
      return Object.prototype.propertyIsEnumerable.call(d, c);
    };
    e.getOwnPropertyDescriptor = function(d, c) {
      return Object.getOwnPropertyDescriptor(d, c);
    };
    e.hasOwnGetter = function(d, c) {
      var f = Object.getOwnPropertyDescriptor(d, c);
      return !(!f || !f.get);
    };
    e.getOwnGetter = function(d, c) {
      var f = Object.getOwnPropertyDescriptor(d, c);
      return f ? f.get : null;
    };
    e.hasOwnSetter = function(d, c) {
      var f = Object.getOwnPropertyDescriptor(d, c);
      return !(!f || !f.set);
    };
    e.createMap = function() {
      return Object.create(null);
    };
    e.createArrayMap = function() {
      return [];
    };
    e.defineReadOnlyProperty = function(d, c, f) {
      Object.defineProperty(d, c, {value:f, writable:!1, configurable:!0, enumerable:!1});
    };
    e.getOwnPropertyDescriptors = function(d) {
      for (var c = e.createMap(), f = Object.getOwnPropertyNames(d), a = 0;a < f.length;a++) {
        c[f[a]] = Object.getOwnPropertyDescriptor(d, f[a]);
      }
      return c;
    };
    e.cloneObject = function(d) {
      var q = Object.create(Object.getPrototypeOf(d));
      c(q, d);
      return q;
    };
    e.copyProperties = function(d, c) {
      for (var f in c) {
        d[f] = c[f];
      }
    };
    e.copyOwnProperties = c;
    e.copyOwnPropertyDescriptors = function(d, c, u) {
      void 0 === u && (u = !0);
      for (var e in c) {
        if (f(c, e)) {
          var a = Object.getOwnPropertyDescriptor(c, e);
          if (u || !f(d, e)) {
            try {
              Object.defineProperty(d, e, a);
            } catch (h) {
            }
          }
        }
      }
    };
    e.getLatestGetterOrSetterPropertyDescriptor = function(d, c) {
      for (var f = {};d;) {
        var e = Object.getOwnPropertyDescriptor(d, c);
        e && (f.get = f.get || e.get, f.set = f.set || e.set);
        if (f.get && f.set) {
          break;
        }
        d = Object.getPrototypeOf(d);
      }
      return f;
    };
    e.defineNonEnumerableGetterOrSetter = function(d, c, f, a) {
      var h = e.getLatestGetterOrSetterPropertyDescriptor(d, c);
      h.configurable = !0;
      h.enumerable = !1;
      a ? h.get = f : h.set = f;
      Object.defineProperty(d, c, h);
    };
    e.defineNonEnumerableGetter = function(d, c, f) {
      Object.defineProperty(d, c, {get:f, configurable:!0, enumerable:!1});
    };
    e.defineNonEnumerableSetter = function(d, c, f) {
      Object.defineProperty(d, c, {set:f, configurable:!0, enumerable:!1});
    };
    e.defineNonEnumerableProperty = function(d, c, f) {
      Object.defineProperty(d, c, {value:f, writable:!0, configurable:!0, enumerable:!1});
    };
    e.defineNonEnumerableForwardingProperty = function(d, c, f) {
      Object.defineProperty(d, c, {get:h.makeForwardingGetter(f), set:h.makeForwardingSetter(f), writable:!0, configurable:!0, enumerable:!1});
    };
    e.defineNewNonEnumerableProperty = function(d, c, f) {
      e.defineNonEnumerableProperty(d, c, f);
    };
    e.createPublicAliases = function(d, c) {
      for (var f = {value:null, writable:!0, configurable:!0, enumerable:!1}, e = 0;e < c.length;e++) {
        var a = c[e];
        f.value = d[a];
        Object.defineProperty(d, "$Bg" + a, f);
      }
    };
  })(k.ObjectUtilities || (k.ObjectUtilities = {}));
  var h;
  (function(e) {
    e.makeForwardingGetter = function(f) {
      return new Function('return this["' + f + '"]//# sourceURL=fwd-get-' + f + ".as");
    };
    e.makeForwardingSetter = function(f) {
      return new Function("value", 'this["' + f + '"] = value;//# sourceURL=fwd-set-' + f + ".as");
    };
    e.bindSafely = function(f, c) {
      function d() {
        return f.apply(c, arguments);
      }
      d.boundTo = c;
      return d;
    };
  })(h = k.FunctionUtilities || (k.FunctionUtilities = {}));
  (function(e) {
    function f(d) {
      return "string" === typeof d ? '"' + d + '"' : "number" === typeof d || "boolean" === typeof d ? String(d) : d instanceof Array ? "[] " + d.length : typeof d;
    }
    function c(d, c, f) {
      q[0] = d;
      q[1] = c;
      q[2] = f;
      return q.join("");
    }
    function d(d, c, q, f) {
      u[0] = d;
      u[1] = c;
      u[2] = q;
      u[3] = f;
      return u.join("");
    }
    e.repeatString = function(d, c) {
      for (var q = "", f = 0;f < c;f++) {
        q += d;
      }
      return q;
    };
    e.memorySizeToString = function(d) {
      d |= 0;
      return 1024 > d ? d + " B" : 1048576 > d ? (d / 1024).toFixed(2) + "KB" : (d / 1048576).toFixed(2) + "MB";
    };
    e.toSafeString = f;
    e.toSafeArrayString = function(d) {
      for (var c = [], q = 0;q < d.length;q++) {
        c.push(f(d[q]));
      }
      return c.join(", ");
    };
    e.utf8decode = function(d) {
      for (var c = new Uint8Array(4 * d.length), q = 0, f = 0, e = d.length;f < e;f++) {
        var u = d.charCodeAt(f);
        if (127 >= u) {
          c[q++] = u;
        } else {
          if (55296 <= u && 56319 >= u) {
            var a = d.charCodeAt(f + 1);
            56320 <= a && 57343 >= a && (u = ((u & 1023) << 10) + (a & 1023) + 65536, ++f);
          }
          0 !== (u & 4292870144) ? (c[q++] = 248 | u >>> 24 & 3, c[q++] = 128 | u >>> 18 & 63, c[q++] = 128 | u >>> 12 & 63, c[q++] = 128 | u >>> 6 & 63) : 0 !== (u & 4294901760) ? (c[q++] = 240 | u >>> 18 & 7, c[q++] = 128 | u >>> 12 & 63, c[q++] = 128 | u >>> 6 & 63) : 0 !== (u & 4294965248) ? (c[q++] = 224 | u >>> 12 & 15, c[q++] = 128 | u >>> 6 & 63) : c[q++] = 192 | u >>> 6 & 31;
          c[q++] = 128 | u & 63;
        }
      }
      return c.subarray(0, q);
    };
    e.utf8encode = function(d) {
      for (var c = 0, q = "";c < d.length;) {
        var f = d[c++] & 255;
        if (127 >= f) {
          q += String.fromCharCode(f);
        } else {
          var u = 192, e = 5;
          do {
            if ((f & (u >> 1 | 128)) === u) {
              break;
            }
            u = u >> 1 | 128;
            --e;
          } while (0 <= e);
          if (0 >= e) {
            q += String.fromCharCode(f);
          } else {
            for (var f = f & (1 << e) - 1, u = !1, a = 5;a >= e;--a) {
              var h = d[c++];
              if (128 != (h & 192)) {
                u = !0;
                break;
              }
              f = f << 6 | h & 63;
            }
            if (u) {
              for (e = c - (7 - a);e < c;++e) {
                q += String.fromCharCode(d[e] & 255);
              }
            } else {
              q = 65536 <= f ? q + String.fromCharCode(f - 65536 >> 10 & 1023 | 55296, f & 1023 | 56320) : q + String.fromCharCode(f);
            }
          }
        }
      }
      return q;
    };
    e.base64ArrayBuffer = function(q) {
      var f = "";
      q = new Uint8Array(q);
      for (var u = q.byteLength, e = u % 3, u = u - e, a, h, I, H, p = 0;p < u;p += 3) {
        H = q[p] << 16 | q[p + 1] << 8 | q[p + 2], a = (H & 16515072) >> 18, h = (H & 258048) >> 12, I = (H & 4032) >> 6, H &= 63, f += d("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[a], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[h], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[I], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[H]);
      }
      1 == e ? (H = q[u], f += c("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(H & 252) >> 2], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(H & 3) << 4], "==")) : 2 == e && (H = q[u] << 8 | q[u + 1], f += d("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(H & 64512) >> 10], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(H & 1008) >> 4], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(H & 15) << 
      2], "="));
      return f;
    };
    e.escapeString = function(d) {
      void 0 !== d && (d = d.replace(/[^\w$]/gi, "$"), /^\d/.test(d) && (d = "$" + d));
      return d;
    };
    e.fromCharCodeArray = function(d) {
      for (var c = "", q = 0;q < d.length;q += 16384) {
        var f = Math.min(d.length - q, 16384), c = c + String.fromCharCode.apply(null, d.subarray(q, q + f))
      }
      return c;
    };
    e.variableLengthEncodeInt32 = function(d) {
      for (var c = 32 - Math.clz32(d), q = Math.ceil(c / 6), c = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[q], q = q - 1;0 <= q;q--) {
        c += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[d >> 6 * q & 63];
      }
      return c;
    };
    e.toEncoding = function(d) {
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[d];
    };
    e.fromEncoding = function(d) {
      if (65 <= d && 90 >= d) {
        return d - 65;
      }
      if (97 <= d && 122 >= d) {
        return d - 71;
      }
      if (48 <= d && 57 >= d) {
        return d + 4;
      }
      if (36 === d) {
        return 62;
      }
      if (95 === d) {
        return 63;
      }
    };
    e.variableLengthDecodeInt32 = function(d) {
      for (var c = e.fromEncoding(d.charCodeAt(0)), q = 0, f = 0;f < c;f++) {
        var u = 6 * (c - f - 1), q = q | e.fromEncoding(d.charCodeAt(1 + f)) << u
      }
      return q;
    };
    e.trimMiddle = function(d, c) {
      if (d.length <= c) {
        return d;
      }
      var q = c >> 1, f = c - q - 1;
      return d.substr(0, q) + "\u2026" + d.substr(d.length - f, f);
    };
    e.multiple = function(d, c) {
      for (var q = "", f = 0;f < c;f++) {
        q += d;
      }
      return q;
    };
    e.indexOfAny = function(d, c, q) {
      for (var f = d.length, u = 0;u < c.length;u++) {
        var e = d.indexOf(c[u], q);
        0 <= e && (f = Math.min(f, e));
      }
      return f === d.length ? -1 : f;
    };
    var q = Array(3), u = Array(4), a = Array(5), h = Array(6), p = Array(7), m = Array(8), b = Array(9);
    e.concat3 = c;
    e.concat4 = d;
    e.concat5 = function(d, c, q, f, u) {
      a[0] = d;
      a[1] = c;
      a[2] = q;
      a[3] = f;
      a[4] = u;
      return a.join("");
    };
    e.concat6 = function(d, c, q, f, u, e) {
      h[0] = d;
      h[1] = c;
      h[2] = q;
      h[3] = f;
      h[4] = u;
      h[5] = e;
      return h.join("");
    };
    e.concat7 = function(d, c, q, f, u, e, a) {
      p[0] = d;
      p[1] = c;
      p[2] = q;
      p[3] = f;
      p[4] = u;
      p[5] = e;
      p[6] = a;
      return p.join("");
    };
    e.concat8 = function(d, c, q, f, u, e, a, h) {
      m[0] = d;
      m[1] = c;
      m[2] = q;
      m[3] = f;
      m[4] = u;
      m[5] = e;
      m[6] = a;
      m[7] = h;
      return m.join("");
    };
    e.concat9 = function(d, c, q, f, u, e, a, h, I) {
      b[0] = d;
      b[1] = c;
      b[2] = q;
      b[3] = f;
      b[4] = u;
      b[5] = e;
      b[6] = a;
      b[7] = h;
      b[8] = I;
      return b.join("");
    };
  })(k.StringUtilities || (k.StringUtilities = {}));
  (function(e) {
    var f = new Uint8Array([7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]), c = new Int32Array([-680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426, -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162, 1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632, 
    643717713, -373897302, -701558691, 38016083, -660478335, -405537848, 568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784, 1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556, -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222, -722521979, 76029189, -640364487, -421815835, 530742520, -995338651, -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606, -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649, 
    -145523070, -1120210379, 718787259, -343485551]);
    e.hashBytesTo32BitsMD5 = function(d, q, u) {
      var e = 1732584193, a = -271733879, h = -1732584194, p = 271733878, m = u + 72 & -64, b = new Uint8Array(m), l;
      for (l = 0;l < u;++l) {
        b[l] = d[q++];
      }
      b[l++] = 128;
      for (d = m - 8;l < d;) {
        b[l++] = 0;
      }
      b[l++] = u << 3 & 255;
      b[l++] = u >> 5 & 255;
      b[l++] = u >> 13 & 255;
      b[l++] = u >> 21 & 255;
      b[l++] = u >>> 29 & 255;
      b[l++] = 0;
      b[l++] = 0;
      b[l++] = 0;
      d = new Int32Array(16);
      for (l = 0;l < m;) {
        for (u = 0;16 > u;++u, l += 4) {
          d[u] = b[l] | b[l + 1] << 8 | b[l + 2] << 16 | b[l + 3] << 24;
        }
        var n = e;
        q = a;
        var t = h, w = p, x, g;
        for (u = 0;64 > u;++u) {
          16 > u ? (x = q & t | ~q & w, g = u) : 32 > u ? (x = w & q | ~w & t, g = 5 * u + 1 & 15) : 48 > u ? (x = q ^ t ^ w, g = 3 * u + 5 & 15) : (x = t ^ (q | ~w), g = 7 * u & 15);
          var k = w, n = n + x + c[u] + d[g] | 0;
          x = f[u];
          w = t;
          t = q;
          q = q + (n << x | n >>> 32 - x) | 0;
          n = k;
        }
        e = e + n | 0;
        a = a + q | 0;
        h = h + t | 0;
        p = p + w | 0;
      }
      return e;
    };
    e.hashBytesTo32BitsAdler = function(d, c, f) {
      var e = 1, a = 0;
      for (f = c + f;c < f;++c) {
        e = (e + (d[c] & 255)) % 65521, a = (a + e) % 65521;
      }
      return a << 16 | e;
    };
  })(k.HashUtilities || (k.HashUtilities = {}));
  var p = function() {
    function e() {
    }
    e.seed = function(f) {
      e._state[0] = f;
      e._state[1] = f;
    };
    e.next = function() {
      var f = this._state, c = Math.imul(18273, f[0] & 65535) + (f[0] >>> 16) | 0;
      f[0] = c;
      var d = Math.imul(36969, f[1] & 65535) + (f[1] >>> 16) | 0;
      f[1] = d;
      f = (c << 16) + (d & 65535) | 0;
      return 2.3283064365386963E-10 * (0 > f ? f + 4294967296 : f);
    };
    e._state = new Uint32Array([57005, 48879]);
    return e;
  }();
  k.Random = p;
  Math.random = function() {
    return p.next();
  };
  (function() {
    function e() {
      this.id = "$weakmap" + f++;
    }
    if ("function" !== typeof jsGlobal.WeakMap) {
      var f = 0;
      e.prototype = {has:function(c) {
        return c.hasOwnProperty(this.id);
      }, get:function(c, d) {
        return c.hasOwnProperty(this.id) ? c[this.id] : d;
      }, set:function(c, d) {
        Object.defineProperty(c, this.id, {value:d, enumerable:!1, configurable:!0});
      }, delete:function(c) {
        delete c[this.id];
      }};
      jsGlobal.WeakMap = e;
    }
  })();
  a = function() {
    function e() {
      "undefined" !== typeof ShumwayCom && ShumwayCom.getWeakMapKeys ? this._map = new WeakMap : this._list = [];
    }
    e.prototype.clear = function() {
      this._map ? this._map.clear() : this._list.length = 0;
    };
    e.prototype.push = function(f) {
      this._map ? this._map.set(f, null) : this._list.push(f);
    };
    e.prototype.remove = function(f) {
      this._map ? this._map.delete(f) : this._list[this._list.indexOf(f)] = null;
    };
    e.prototype.forEach = function(f) {
      if (this._map) {
        ShumwayCom.getWeakMapKeys(this._map).forEach(function(d) {
          0 !== d._referenceCount && f(d);
        });
      } else {
        for (var c = this._list, d = 0, q = 0;q < c.length;q++) {
          var u = c[q];
          u && (0 === u._referenceCount ? (c[q] = null, d++) : f(u));
        }
        if (16 < d && d > c.length >> 2) {
          d = [];
          for (q = 0;q < c.length;q++) {
            (u = c[q]) && 0 < u._referenceCount && d.push(u);
          }
          this._list = d;
        }
      }
    };
    Object.defineProperty(e.prototype, "length", {get:function() {
      return this._map ? -1 : this._list.length;
    }, enumerable:!0, configurable:!0});
    return e;
  }();
  k.WeakList = a;
  var l;
  (function(e) {
    e.pow2 = function(f) {
      return f === (f | 0) ? 0 > f ? 1 / (1 << -f) : 1 << f : Math.pow(2, f);
    };
    e.clamp = function(f, c, d) {
      return Math.max(c, Math.min(d, f));
    };
    e.roundHalfEven = function(f) {
      if (.5 === Math.abs(f % 1)) {
        var c = Math.floor(f);
        return 0 === c % 2 ? c : Math.ceil(f);
      }
      return Math.round(f);
    };
    e.altTieBreakRound = function(f, c) {
      return .5 !== Math.abs(f % 1) || c ? Math.round(f) : f | 0;
    };
    e.epsilonEquals = function(f, c) {
      return 1E-7 > Math.abs(f - c);
    };
  })(l = k.NumberUtilities || (k.NumberUtilities = {}));
  (function(e) {
    e[e.MaxU16 = 65535] = "MaxU16";
    e[e.MaxI16 = 32767] = "MaxI16";
    e[e.MinI16 = -32768] = "MinI16";
  })(k.Numbers || (k.Numbers = {}));
  var w;
  (function(e) {
    function f(d) {
      return 256 * d << 16 >> 16;
    }
    var c = new ArrayBuffer(8);
    e.i8 = new Int8Array(c);
    e.u8 = new Uint8Array(c);
    e.i32 = new Int32Array(c);
    e.f32 = new Float32Array(c);
    e.f64 = new Float64Array(c);
    e.nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
    e.floatToInt32 = function(d) {
      e.f32[0] = d;
      return e.i32[0];
    };
    e.int32ToFloat = function(d) {
      e.i32[0] = d;
      return e.f32[0];
    };
    e.swap16 = function(d) {
      return (d & 255) << 8 | d >> 8 & 255;
    };
    e.swap32 = function(d) {
      return (d & 255) << 24 | (d & 65280) << 8 | d >> 8 & 65280 | d >> 24 & 255;
    };
    e.toS8U8 = f;
    e.fromS8U8 = function(d) {
      return d / 256;
    };
    e.clampS8U8 = function(d) {
      return f(d) / 256;
    };
    e.toS16 = function(d) {
      return d << 16 >> 16;
    };
    e.bitCount = function(d) {
      d -= d >> 1 & 1431655765;
      d = (d & 858993459) + (d >> 2 & 858993459);
      return 16843009 * (d + (d >> 4) & 252645135) >> 24;
    };
    e.ones = function(d) {
      d -= d >> 1 & 1431655765;
      d = (d & 858993459) + (d >> 2 & 858993459);
      return 16843009 * (d + (d >> 4) & 252645135) >> 24;
    };
    e.trailingZeros = function(d) {
      return e.ones((d & -d) - 1);
    };
    e.getFlags = function(d, c) {
      var f = "";
      for (d = 0;d < c.length;d++) {
        d & 1 << d && (f += c[d] + " ");
      }
      return 0 === f.length ? "" : f.trim();
    };
    e.isPowerOfTwo = function(d) {
      return d && 0 === (d & d - 1);
    };
    e.roundToMultipleOfFour = function(d) {
      return d + 3 & -4;
    };
    e.nearestPowerOfTwo = function(d) {
      d--;
      d |= d >> 1;
      d |= d >> 2;
      d |= d >> 4;
      d |= d >> 8;
      d |= d >> 16;
      d++;
      return d;
    };
    e.roundToMultipleOfPowerOfTwo = function(d, c) {
      var f = (1 << c) - 1;
      return d + f & ~f;
    };
    Math.imul || (Math.imul = function(d, c) {
      var f = d & 65535, e = c & 65535;
      return f * e + ((d >>> 16 & 65535) * e + f * (c >>> 16 & 65535) << 16 >>> 0) | 0;
    });
    Math.clz32 || (Math.clz32 = function(d) {
      d |= d >> 1;
      d |= d >> 2;
      d |= d >> 4;
      d |= d >> 8;
      return 32 - e.ones(d | d >> 16);
    });
  })(w = k.IntegerUtilities || (k.IntegerUtilities = {}));
  (function(e) {
    function f(c, d, q, f, e, a) {
      return (q - c) * (a - d) - (f - d) * (e - c);
    }
    e.pointInPolygon = function(c, d, q) {
      for (var f = 0, e = q.length - 2, a = 0;a < e;a += 2) {
        var h = q[a + 0], p = q[a + 1], m = q[a + 2], b = q[a + 3];
        (p <= d && b > d || p > d && b <= d) && c < h + (d - p) / (b - p) * (m - h) && f++;
      }
      return 1 === (f & 1);
    };
    e.signedArea = f;
    e.counterClockwise = function(c, d, q, e, a, h) {
      return 0 < f(c, d, q, e, a, h);
    };
    e.clockwise = function(c, d, q, e, a, h) {
      return 0 > f(c, d, q, e, a, h);
    };
    e.pointInPolygonInt32 = function(c, d, q) {
      c |= 0;
      d |= 0;
      for (var f = 0, e = q.length - 2, a = 0;a < e;a += 2) {
        var h = q[a + 0], p = q[a + 1], m = q[a + 2], b = q[a + 3];
        (p <= d && b > d || p > d && b <= d) && c < h + (d - p) / (b - p) * (m - h) && f++;
      }
      return 1 === (f & 1);
    };
  })(k.GeometricUtilities || (k.GeometricUtilities = {}));
  (function(e) {
    e[e.Error = 1] = "Error";
    e[e.Warn = 2] = "Warn";
    e[e.Debug = 4] = "Debug";
    e[e.Log = 8] = "Log";
    e[e.Info = 16] = "Info";
    e[e.All = 31] = "All";
  })(k.LogLevel || (k.LogLevel = {}));
  a = function() {
    function e(f, c) {
      void 0 === f && (f = !1);
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = f;
      this._out = c || e._consoleOut;
      this._outNoNewline = c || e._consoleOutNoNewline;
    }
    e.prototype.write = function(f, c) {
      void 0 === f && (f = "");
      void 0 === c && (c = !1);
      this._suppressOutput || this._outNoNewline((c ? this._padding : "") + f);
    };
    e.prototype.writeLn = function(f) {
      void 0 === f && (f = "");
      this._suppressOutput || this._out(this._padding + f);
    };
    e.prototype.writeObject = function(f, c) {
      void 0 === f && (f = "");
      this._suppressOutput || this._out(this._padding + f, c);
    };
    e.prototype.writeTimeLn = function(f) {
      void 0 === f && (f = "");
      this._suppressOutput || this._out(this._padding + performance.now().toFixed(2) + " " + f);
    };
    e.prototype.writeComment = function(f) {
      f = f.split("\n");
      if (1 === f.length) {
        this.writeLn("// " + f[0]);
      } else {
        this.writeLn("/**");
        for (var c = 0;c < f.length;c++) {
          this.writeLn(" * " + f[c]);
        }
        this.writeLn(" */");
      }
    };
    e.prototype.writeLns = function(f) {
      f = f.split("\n");
      for (var c = 0;c < f.length;c++) {
        this.writeLn(f[c]);
      }
    };
    e.prototype.errorLn = function(f) {
      e.logLevel & 1 && this.boldRedLn(f);
    };
    e.prototype.warnLn = function(f) {
      e.logLevel & 2 && this.yellowLn(f);
    };
    e.prototype.debugLn = function(f) {
      e.logLevel & 4 && this.purpleLn(f);
    };
    e.prototype.logLn = function(f) {
      e.logLevel & 8 && this.writeLn(f);
    };
    e.prototype.infoLn = function(f) {
      e.logLevel & 16 && this.writeLn(f);
    };
    e.prototype.yellowLn = function(f) {
      this.colorLn(e.YELLOW, f);
    };
    e.prototype.greenLn = function(f) {
      this.colorLn(e.GREEN, f);
    };
    e.prototype.boldRedLn = function(f) {
      this.colorLn(e.BOLD_RED, f);
    };
    e.prototype.redLn = function(f) {
      this.colorLn(e.RED, f);
    };
    e.prototype.purpleLn = function(f) {
      this.colorLn(e.PURPLE, f);
    };
    e.prototype.colorLn = function(f, c) {
      this._suppressOutput || (inBrowser ? this._out(this._padding + c) : this._out(this._padding + f + c + e.ENDC));
    };
    e.prototype.redLns = function(f) {
      this.colorLns(e.RED, f);
    };
    e.prototype.colorLns = function(f, c) {
      for (var d = c.split("\n"), q = 0;q < d.length;q++) {
        this.colorLn(f, d[q]);
      }
    };
    e.prototype.enter = function(f) {
      this._suppressOutput || this._out(this._padding + f);
      this.indent();
    };
    e.prototype.leaveAndEnter = function(f) {
      this.leave(f);
      this.indent();
    };
    e.prototype.leave = function(f) {
      this.outdent();
      !this._suppressOutput && f && this._out(this._padding + f);
    };
    e.prototype.indent = function() {
      this._padding += this._tab;
    };
    e.prototype.outdent = function() {
      0 < this._padding.length && (this._padding = this._padding.substring(0, this._padding.length - this._tab.length));
    };
    e.prototype.writeArray = function(f, c, d) {
      void 0 === c && (c = !1);
      void 0 === d && (d = !1);
      c = c || !1;
      for (var q = 0, e = f.length;q < e;q++) {
        var a = "";
        c && (a = null === f[q] ? "null" : void 0 === f[q] ? "undefined" : f[q].constructor.name, a += " ");
        var h = d ? "" : ("" + q).padRight(" ", 4);
        this.writeLn(h + a + f[q]);
      }
    };
    e.PURPLE = "\u001b[94m";
    e.YELLOW = "\u001b[93m";
    e.GREEN = "\u001b[92m";
    e.RED = "\u001b[91m";
    e.BOLD_RED = "\u001b[1;91m";
    e.ENDC = "\u001b[0m";
    e.logLevel = 31;
    e._consoleOut = console.info.bind(console);
    e._consoleOutNoNewline = console.info.bind(console);
    return e;
  }();
  k.IndentingWriter = a;
  var m = function() {
    return function(e, f) {
      this.value = e;
      this.next = f;
    };
  }(), a = function() {
    function e(f) {
      this._compare = f;
      this._head = null;
      this._length = 0;
    }
    e.prototype.push = function(f) {
      this._length++;
      if (this._head) {
        var c = this._head, d = null;
        f = new m(f, null);
        for (var q = this._compare;c;) {
          if (0 < q(c.value, f.value)) {
            d ? (f.next = c, d.next = f) : (f.next = this._head, this._head = f);
            return;
          }
          d = c;
          c = c.next;
        }
        d.next = f;
      } else {
        this._head = new m(f, null);
      }
    };
    e.prototype.forEach = function(f) {
      for (var c = this._head, d = null;c;) {
        var q = f(c.value);
        if (q === e.RETURN) {
          break;
        } else {
          q === e.DELETE ? c = d ? d.next = c.next : this._head = this._head.next : (d = c, c = c.next);
        }
      }
    };
    e.prototype.isEmpty = function() {
      return !this._head;
    };
    e.prototype.pop = function() {
      if (this._head) {
        this._length--;
        var f = this._head;
        this._head = this._head.next;
        return f.value;
      }
    };
    e.prototype.contains = function(f) {
      for (var c = this._head;c;) {
        if (c.value === f) {
          return !0;
        }
        c = c.next;
      }
      return !1;
    };
    e.prototype.toString = function() {
      for (var f = "[", c = this._head;c;) {
        f += c.value.toString(), (c = c.next) && (f += ",");
      }
      return f + "]";
    };
    e.RETURN = 1;
    e.DELETE = 2;
    return e;
  }();
  k.SortedList = a;
  a = function() {
    function e(f, c) {
      void 0 === c && (c = 12);
      this.start = this.index = 0;
      this._size = 1 << c;
      this._mask = this._size - 1;
      this.array = new f(this._size);
    }
    e.prototype.get = function(f) {
      return this.array[f];
    };
    e.prototype.forEachInReverse = function(f) {
      if (!this.isEmpty()) {
        for (var c = 0 === this.index ? this._size - 1 : this.index - 1, d = this.start - 1 & this._mask;c !== d && !f(this.array[c], c);) {
          c = 0 === c ? this._size - 1 : c - 1;
        }
      }
    };
    e.prototype.write = function(f) {
      this.array[this.index] = f;
      this.index = this.index + 1 & this._mask;
      this.index === this.start && (this.start = this.start + 1 & this._mask);
    };
    e.prototype.isFull = function() {
      return (this.index + 1 & this._mask) === this.start;
    };
    e.prototype.isEmpty = function() {
      return this.index === this.start;
    };
    e.prototype.reset = function() {
      this.start = this.index = 0;
    };
    return e;
  }();
  k.CircularBuffer = a;
  (function(e) {
    function f(d) {
      return d + (e.BITS_PER_WORD - 1) >> e.ADDRESS_BITS_PER_WORD << e.ADDRESS_BITS_PER_WORD;
    }
    function c(d, c) {
      d = d || "1";
      c = c || "0";
      for (var q = "", f = 0;f < length;f++) {
        q += this.get(f) ? d : c;
      }
      return q;
    }
    function d(d) {
      for (var c = [], q = 0;q < length;q++) {
        this.get(q) && c.push(d ? d[q] : q);
      }
      return c.join(", ");
    }
    e.ADDRESS_BITS_PER_WORD = 5;
    e.BITS_PER_WORD = 1 << e.ADDRESS_BITS_PER_WORD;
    e.BIT_INDEX_MASK = e.BITS_PER_WORD - 1;
    var q = function() {
      function d(c) {
        this.size = f(c);
        this.dirty = this.count = 0;
        this.length = c;
        this.bits = new Uint32Array(this.size >> e.ADDRESS_BITS_PER_WORD);
      }
      d.prototype.recount = function() {
        if (this.dirty) {
          for (var d = this.bits, c = 0, q = 0, f = d.length;q < f;q++) {
            var e = d[q], e = e - (e >> 1 & 1431655765), e = (e & 858993459) + (e >> 2 & 858993459), c = c + (16843009 * (e + (e >> 4) & 252645135) >> 24)
          }
          this.count = c;
          this.dirty = 0;
        }
      };
      d.prototype.set = function(d) {
        var c = d >> e.ADDRESS_BITS_PER_WORD, q = this.bits[c];
        d = q | 1 << (d & e.BIT_INDEX_MASK);
        this.bits[c] = d;
        this.dirty |= q ^ d;
      };
      d.prototype.setAll = function() {
        for (var d = this.bits, c = 0, q = d.length;c < q;c++) {
          d[c] = 4294967295;
        }
        this.count = this.size;
        this.dirty = 0;
      };
      d.prototype.assign = function(d) {
        this.count = d.count;
        this.dirty = d.dirty;
        this.size = d.size;
        for (var c = 0, q = this.bits.length;c < q;c++) {
          this.bits[c] = d.bits[c];
        }
      };
      d.prototype.clear = function(d) {
        var c = d >> e.ADDRESS_BITS_PER_WORD, q = this.bits[c];
        d = q & ~(1 << (d & e.BIT_INDEX_MASK));
        this.bits[c] = d;
        this.dirty |= q ^ d;
      };
      d.prototype.get = function(d) {
        return 0 !== (this.bits[d >> e.ADDRESS_BITS_PER_WORD] & 1 << (d & e.BIT_INDEX_MASK));
      };
      d.prototype.clearAll = function() {
        for (var d = this.bits, c = 0, q = d.length;c < q;c++) {
          d[c] = 0;
        }
        this.dirty = this.count = 0;
      };
      d.prototype._union = function(d) {
        var c = this.dirty, q = this.bits;
        d = d.bits;
        for (var f = 0, e = q.length;f < e;f++) {
          var u = q[f], a = u | d[f];
          q[f] = a;
          c |= u ^ a;
        }
        this.dirty = c;
      };
      d.prototype.intersect = function(d) {
        var c = this.dirty, q = this.bits;
        d = d.bits;
        for (var f = 0, e = q.length;f < e;f++) {
          var u = q[f], a = u & d[f];
          q[f] = a;
          c |= u ^ a;
        }
        this.dirty = c;
      };
      d.prototype.subtract = function(d) {
        var c = this.dirty, q = this.bits;
        d = d.bits;
        for (var f = 0, e = q.length;f < e;f++) {
          var u = q[f], a = u & ~d[f];
          q[f] = a;
          c |= u ^ a;
        }
        this.dirty = c;
      };
      d.prototype.negate = function() {
        for (var d = this.dirty, c = this.bits, q = 0, f = c.length;q < f;q++) {
          var e = c[q], u = ~e;
          c[q] = u;
          d |= e ^ u;
        }
        this.dirty = d;
      };
      d.prototype.forEach = function(d) {
        for (var c = this.bits, q = 0, f = c.length;q < f;q++) {
          var u = c[q];
          if (u) {
            for (var a = 0;a < e.BITS_PER_WORD;a++) {
              u & 1 << a && d(q * e.BITS_PER_WORD + a);
            }
          }
        }
      };
      d.prototype.toArray = function() {
        for (var d = [], c = this.bits, q = 0, f = c.length;q < f;q++) {
          var u = c[q];
          if (u) {
            for (var a = 0;a < e.BITS_PER_WORD;a++) {
              u & 1 << a && d.push(q * e.BITS_PER_WORD + a);
            }
          }
        }
        return d;
      };
      d.prototype.equals = function(d) {
        if (this.size !== d.size) {
          return !1;
        }
        var c = this.bits;
        d = d.bits;
        for (var q = 0, f = c.length;q < f;q++) {
          if (c[q] !== d[q]) {
            return !1;
          }
        }
        return !0;
      };
      d.prototype.contains = function(d) {
        if (this.size !== d.size) {
          return !1;
        }
        var c = this.bits;
        d = d.bits;
        for (var q = 0, f = c.length;q < f;q++) {
          if ((c[q] | d[q]) !== c[q]) {
            return !1;
          }
        }
        return !0;
      };
      d.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      d.prototype.clone = function() {
        var c = new d(this.length);
        c._union(this);
        return c;
      };
      return d;
    }();
    e.Uint32ArrayBitSet = q;
    var u = function() {
      function d(c) {
        this.dirty = this.count = 0;
        this.size = f(c);
        this.bits = 0;
        this.singleWord = !0;
        this.length = c;
      }
      d.prototype.recount = function() {
        if (this.dirty) {
          var d = this.bits, d = d - (d >> 1 & 1431655765), d = (d & 858993459) + (d >> 2 & 858993459);
          this.count = 0 + (16843009 * (d + (d >> 4) & 252645135) >> 24);
          this.dirty = 0;
        }
      };
      d.prototype.set = function(d) {
        var c = this.bits;
        this.bits = d = c | 1 << (d & e.BIT_INDEX_MASK);
        this.dirty |= c ^ d;
      };
      d.prototype.setAll = function() {
        this.bits = 4294967295;
        this.count = this.size;
        this.dirty = 0;
      };
      d.prototype.assign = function(d) {
        this.count = d.count;
        this.dirty = d.dirty;
        this.size = d.size;
        this.bits = d.bits;
      };
      d.prototype.clear = function(d) {
        var c = this.bits;
        this.bits = d = c & ~(1 << (d & e.BIT_INDEX_MASK));
        this.dirty |= c ^ d;
      };
      d.prototype.get = function(d) {
        return 0 !== (this.bits & 1 << (d & e.BIT_INDEX_MASK));
      };
      d.prototype.clearAll = function() {
        this.dirty = this.count = this.bits = 0;
      };
      d.prototype._union = function(d) {
        var c = this.bits;
        this.bits = d = c | d.bits;
        this.dirty = c ^ d;
      };
      d.prototype.intersect = function(d) {
        var c = this.bits;
        this.bits = d = c & d.bits;
        this.dirty = c ^ d;
      };
      d.prototype.subtract = function(d) {
        var c = this.bits;
        this.bits = d = c & ~d.bits;
        this.dirty = c ^ d;
      };
      d.prototype.negate = function() {
        var d = this.bits, c = ~d;
        this.bits = c;
        this.dirty = d ^ c;
      };
      d.prototype.forEach = function(d) {
        var c = this.bits;
        if (c) {
          for (var q = 0;q < e.BITS_PER_WORD;q++) {
            c & 1 << q && d(q);
          }
        }
      };
      d.prototype.toArray = function() {
        var d = [], c = this.bits;
        if (c) {
          for (var q = 0;q < e.BITS_PER_WORD;q++) {
            c & 1 << q && d.push(q);
          }
        }
        return d;
      };
      d.prototype.equals = function(d) {
        return this.bits === d.bits;
      };
      d.prototype.contains = function(d) {
        var c = this.bits;
        return (c | d.bits) === c;
      };
      d.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      d.prototype.clone = function() {
        var c = new d(this.length);
        c._union(this);
        return c;
      };
      return d;
    }();
    e.Uint32BitSet = u;
    u.prototype.toString = d;
    u.prototype.toBitString = c;
    q.prototype.toString = d;
    q.prototype.toBitString = c;
    e.BitSetFunctor = function(d) {
      var c = 1 === f(d) >> e.ADDRESS_BITS_PER_WORD ? u : q;
      return function() {
        return new c(d);
      };
    };
  })(k.BitSets || (k.BitSets = {}));
  a = function() {
    function e() {
    }
    e.randomStyle = function() {
      e._randomStyleCache || (e._randomStyleCache = "#ff5e3a #ff9500 #ffdb4c #87fc70 #52edc7 #1ad6fd #c644fc #ef4db6 #4a4a4a #dbddde #ff3b30 #ff9500 #ffcc00 #4cd964 #34aadc #007aff #5856d6 #ff2d55 #8e8e93 #c7c7cc #5ad427 #c86edf #d1eefc #e0f8d8 #fb2b69 #f7f7f7 #1d77ef #d6cec3 #55efcb #ff4981 #ffd3e0 #f7f7f7 #ff1300 #1f1f21 #bdbec2 #ff3a2d".split(" "));
      return e._randomStyleCache[e._nextStyle++ % e._randomStyleCache.length];
    };
    e.gradientColor = function(f) {
      return e._gradient[e._gradient.length * l.clamp(f, 0, 1) | 0];
    };
    e.contrastStyle = function(f) {
      f = parseInt(f.substr(1), 16);
      return 128 <= (299 * (f >> 16) + 587 * (f >> 8 & 255) + 114 * (f & 255)) / 1E3 ? "#000000" : "#ffffff";
    };
    e.reset = function() {
      e._nextStyle = 0;
    };
    e.TabToolbar = "#252c33";
    e.Toolbars = "#343c45";
    e.HighlightBlue = "#1d4f73";
    e.LightText = "#f5f7fa";
    e.ForegroundText = "#b6babf";
    e.Black = "#000000";
    e.VeryDark = "#14171a";
    e.Dark = "#181d20";
    e.Light = "#a9bacb";
    e.Grey = "#8fa1b2";
    e.DarkGrey = "#5f7387";
    e.Blue = "#46afe3";
    e.Purple = "#6b7abb";
    e.Pink = "#df80ff";
    e.Red = "#eb5368";
    e.Orange = "#d96629";
    e.LightOrange = "#d99b28";
    e.Green = "#70bf53";
    e.BlueGrey = "#5e88b0";
    e._nextStyle = 0;
    e._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
    return e;
  }();
  k.ColorStyle = a;
  a = function() {
    function e(f, c, d, q) {
      this.xMin = f | 0;
      this.yMin = c | 0;
      this.xMax = d | 0;
      this.yMax = q | 0;
    }
    e.FromUntyped = function(f) {
      return new e(f.xMin, f.yMin, f.xMax, f.yMax);
    };
    e.FromRectangle = function(f) {
      return new e(20 * f.x | 0, 20 * f.y | 0, 20 * (f.x + f.width) | 0, 20 * (f.y + f.height) | 0);
    };
    e.prototype.setElements = function(f, c, d, q) {
      this.xMin = f;
      this.yMin = c;
      this.xMax = d;
      this.yMax = q;
    };
    e.prototype.copyFrom = function(f) {
      this.setElements(f.xMin, f.yMin, f.xMax, f.yMax);
    };
    e.prototype.contains = function(f, c) {
      return f < this.xMin !== f < this.xMax && c < this.yMin !== c < this.yMax;
    };
    e.prototype.unionInPlace = function(f) {
      f.isEmpty() || (this.extendByPoint(f.xMin, f.yMin), this.extendByPoint(f.xMax, f.yMax));
    };
    e.prototype.extendByPoint = function(f, c) {
      this.extendByX(f);
      this.extendByY(c);
    };
    e.prototype.extendByX = function(f) {
      134217728 === this.xMin ? this.xMin = this.xMax = f : (this.xMin = Math.min(this.xMin, f), this.xMax = Math.max(this.xMax, f));
    };
    e.prototype.extendByY = function(f) {
      134217728 === this.yMin ? this.yMin = this.yMax = f : (this.yMin = Math.min(this.yMin, f), this.yMax = Math.max(this.yMax, f));
    };
    e.prototype.intersects = function(f) {
      return this.contains(f.xMin, f.yMin) || this.contains(f.xMax, f.yMax);
    };
    e.prototype.isEmpty = function() {
      return this.xMax <= this.xMin || this.yMax <= this.yMin;
    };
    Object.defineProperty(e.prototype, "width", {get:function() {
      return this.xMax - this.xMin;
    }, set:function(f) {
      this.xMax = this.xMin + f;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "height", {get:function() {
      return this.yMax - this.yMin;
    }, set:function(f) {
      this.yMax = this.yMin + f;
    }, enumerable:!0, configurable:!0});
    e.prototype.getBaseWidth = function(f) {
      return Math.abs(Math.cos(f)) * (this.xMax - this.xMin) + Math.abs(Math.sin(f)) * (this.yMax - this.yMin);
    };
    e.prototype.getBaseHeight = function(f) {
      return Math.abs(Math.sin(f)) * (this.xMax - this.xMin) + Math.abs(Math.cos(f)) * (this.yMax - this.yMin);
    };
    e.prototype.setEmpty = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 0;
    };
    e.prototype.setToSentinels = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 134217728;
    };
    e.prototype.clone = function() {
      return new e(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    e.prototype.toString = function() {
      return "{ xMin: " + this.xMin + ", xMin: " + this.yMin + ", xMax: " + this.xMax + ", xMax: " + this.yMax + " }";
    };
    return e;
  }();
  k.Bounds = a;
  a = function() {
    function e(f, c, d, q) {
      n.assert(r(f));
      n.assert(r(c));
      n.assert(r(d));
      n.assert(r(q));
      this._xMin = f | 0;
      this._yMin = c | 0;
      this._xMax = d | 0;
      this._yMax = q | 0;
    }
    e.FromUntyped = function(f) {
      return new e(f.xMin, f.yMin, f.xMax, f.yMax);
    };
    e.FromRectangle = function(f) {
      return new e(20 * f.x | 0, 20 * f.y | 0, 20 * (f.x + f.width) | 0, 20 * (f.y + f.height) | 0);
    };
    e.prototype.setElements = function(f, c, d, q) {
      this.xMin = f;
      this.yMin = c;
      this.xMax = d;
      this.yMax = q;
    };
    e.prototype.copyFrom = function(f) {
      this.setElements(f.xMin, f.yMin, f.xMax, f.yMax);
    };
    e.prototype.contains = function(f, c) {
      return f < this.xMin !== f < this.xMax && c < this.yMin !== c < this.yMax;
    };
    e.prototype.unionInPlace = function(f) {
      f.isEmpty() || (this.extendByPoint(f.xMin, f.yMin), this.extendByPoint(f.xMax, f.yMax));
    };
    e.prototype.extendByPoint = function(f, c) {
      this.extendByX(f);
      this.extendByY(c);
    };
    e.prototype.extendByX = function(f) {
      134217728 === this.xMin ? this.xMin = this.xMax = f : (this.xMin = Math.min(this.xMin, f), this.xMax = Math.max(this.xMax, f));
    };
    e.prototype.extendByY = function(f) {
      134217728 === this.yMin ? this.yMin = this.yMax = f : (this.yMin = Math.min(this.yMin, f), this.yMax = Math.max(this.yMax, f));
    };
    e.prototype.intersects = function(f) {
      return this.contains(f._xMin, f._yMin) || this.contains(f._xMax, f._yMax);
    };
    e.prototype.isEmpty = function() {
      return this._xMax <= this._xMin || this._yMax <= this._yMin;
    };
    Object.defineProperty(e.prototype, "xMin", {get:function() {
      return this._xMin;
    }, set:function(f) {
      n.assert(r(f));
      this._xMin = f;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "yMin", {get:function() {
      return this._yMin;
    }, set:function(f) {
      n.assert(r(f));
      this._yMin = f | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "xMax", {get:function() {
      return this._xMax;
    }, set:function(f) {
      n.assert(r(f));
      this._xMax = f | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "width", {get:function() {
      return this._xMax - this._xMin;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "yMax", {get:function() {
      return this._yMax;
    }, set:function(f) {
      n.assert(r(f));
      this._yMax = f | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "height", {get:function() {
      return this._yMax - this._yMin;
    }, enumerable:!0, configurable:!0});
    e.prototype.getBaseWidth = function(f) {
      return Math.abs(Math.cos(f)) * (this._xMax - this._xMin) + Math.abs(Math.sin(f)) * (this._yMax - this._yMin);
    };
    e.prototype.getBaseHeight = function(f) {
      return Math.abs(Math.sin(f)) * (this._xMax - this._xMin) + Math.abs(Math.cos(f)) * (this._yMax - this._yMin);
    };
    e.prototype.setEmpty = function() {
      this._xMin = this._yMin = this._xMax = this._yMax = 0;
    };
    e.prototype.clone = function() {
      return new e(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    e.prototype.toString = function() {
      return "{ xMin: " + this._xMin + ", xMin: " + this._yMin + ", xMax: " + this._xMax + ", yMax: " + this._yMax + " }";
    };
    e.prototype.assertValid = function() {
    };
    return e;
  }();
  k.DebugBounds = a;
  a = function() {
    function e(f, c, d, q) {
      this.r = f;
      this.g = c;
      this.b = d;
      this.a = q;
    }
    e.FromARGB = function(f) {
      return new e((f >> 16 & 255) / 255, (f >> 8 & 255) / 255, (f >> 0 & 255) / 255, (f >> 24 & 255) / 255);
    };
    e.FromRGBA = function(f) {
      return e.FromARGB(t.RGBAToARGB(f));
    };
    e.prototype.toRGBA = function() {
      return 255 * this.r << 24 | 255 * this.g << 16 | 255 * this.b << 8 | 255 * this.a;
    };
    e.prototype.toCSSStyle = function() {
      return t.rgbaToCSSStyle(this.toRGBA());
    };
    e.prototype.set = function(f) {
      this.r = f.r;
      this.g = f.g;
      this.b = f.b;
      this.a = f.a;
    };
    e.randomColor = function() {
      var f = .4;
      void 0 === f && (f = 1);
      return new e(Math.random(), Math.random(), Math.random(), f);
    };
    e.parseColor = function(f) {
      e.colorCache || (e.colorCache = Object.create(null));
      if (e.colorCache[f]) {
        return e.colorCache[f];
      }
      var c = document.createElement("span");
      document.body.appendChild(c);
      c.style.backgroundColor = f;
      var d = getComputedStyle(c).backgroundColor;
      document.body.removeChild(c);
      (c = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(d)) || (c = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(d));
      d = new e(0, 0, 0, 0);
      d.r = parseFloat(c[1]) / 255;
      d.g = parseFloat(c[2]) / 255;
      d.b = parseFloat(c[3]) / 255;
      d.a = c[4] ? parseFloat(c[4]) / 255 : 1;
      return e.colorCache[f] = d;
    };
    e.Red = new e(1, 0, 0, 1);
    e.Green = new e(0, 1, 0, 1);
    e.Blue = new e(0, 0, 1, 1);
    e.None = new e(0, 0, 0, 0);
    e.White = new e(1, 1, 1, 1);
    e.Black = new e(0, 0, 0, 1);
    e.colorCache = {};
    return e;
  }();
  k.Color = a;
  var t;
  (function(e) {
    function f(d) {
      var c, f, e = d >> 24 & 255;
      f = (Math.imul(d >> 16 & 255, e) + 127) / 255 | 0;
      c = (Math.imul(d >> 8 & 255, e) + 127) / 255 | 0;
      d = (Math.imul(d >> 0 & 255, e) + 127) / 255 | 0;
      return e << 24 | f << 16 | c << 8 | d;
    }
    e.RGBAToARGB = function(d) {
      return d >> 8 & 16777215 | (d & 255) << 24;
    };
    e.ARGBToRGBA = function(d) {
      return d << 8 | d >> 24 & 255;
    };
    e.rgbaToCSSStyle = function(d) {
      return k.StringUtilities.concat9("rgba(", d >> 24 & 255, ",", d >> 16 & 255, ",", d >> 8 & 255, ",", (d & 255) / 255, ")");
    };
    e.cssStyleToRGBA = function(d) {
      if ("#" === d[0]) {
        if (7 === d.length) {
          return parseInt(d.substring(1), 16) << 8 | 255;
        }
      } else {
        if ("r" === d[0]) {
          return d = d.substring(5, d.length - 1).split(","), (parseInt(d[0]) & 255) << 24 | (parseInt(d[1]) & 255) << 16 | (parseInt(d[2]) & 255) << 8 | 255 * parseFloat(d[3]) & 255;
        }
      }
      return 4278190335;
    };
    e.hexToRGB = function(d) {
      return parseInt(d.slice(1), 16);
    };
    e.rgbToHex = function(d) {
      return "#" + ("000000" + (d >>> 0).toString(16)).slice(-6);
    };
    e.isValidHexColor = function(d) {
      return /^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(d);
    };
    e.clampByte = function(d) {
      return Math.max(0, Math.min(255, d));
    };
    e.unpremultiplyARGB = function(d) {
      var c, f, e = d >> 24 & 255;
      f = Math.imul(255, d >> 16 & 255) / e & 255;
      c = Math.imul(255, d >> 8 & 255) / e & 255;
      d = Math.imul(255, d >> 0 & 255) / e & 255;
      return e << 24 | f << 16 | c << 8 | d;
    };
    e.premultiplyARGB = f;
    var c;
    e.ensureUnpremultiplyTable = function() {
      if (!c) {
        c = new Uint8Array(65536);
        for (var d = 0;256 > d;d++) {
          for (var f = 0;256 > f;f++) {
            c[(f << 8) + d] = Math.imul(255, d) / f;
          }
        }
      }
    };
    e.tableLookupUnpremultiplyARGB = function(d) {
      d |= 0;
      var f = d >> 24 & 255;
      if (0 === f) {
        return 0;
      }
      if (255 === f) {
        return d;
      }
      var e, a, h = f << 8, p = c;
      a = p[h + (d >> 16 & 255)];
      e = p[h + (d >> 8 & 255)];
      d = p[h + (d >> 0 & 255)];
      return f << 24 | a << 16 | e << 8 | d;
    };
    e.blendPremultipliedBGRA = function(d, c) {
      var f, e;
      e = 256 - (c & 255);
      f = Math.imul(d & 16711935, e) >> 8;
      e = Math.imul(d >> 8 & 16711935, e) >> 8;
      return ((c >> 8 & 16711935) + e & 16711935) << 8 | (c & 16711935) + f & 16711935;
    };
    var d = w.swap32;
    e.convertImage = function(q, e, a, h) {
      var p = a.length;
      if (q === e) {
        if (a !== h) {
          for (q = 0;q < p;q++) {
            h[q] = a[q];
          }
        }
      } else {
        if (1 === q && 3 === e) {
          for (k.ColorUtilities.ensureUnpremultiplyTable(), q = 0;q < p;q++) {
            var m = a[q];
            e = m & 255;
            if (0 === e) {
              h[q] = 0;
            } else {
              if (255 === e) {
                h[q] = 4278190080 | m >> 8 & 16777215;
              } else {
                var b = m >> 24 & 255, l = m >> 16 & 255, m = m >> 8 & 255, t = e << 8, w = c, m = w[t + m], l = w[t + l], b = w[t + b];
                h[q] = e << 24 | b << 16 | l << 8 | m;
              }
            }
          }
        } else {
          if (2 === q && 3 === e) {
            for (q = 0;q < p;q++) {
              h[q] = d(a[q]);
            }
          } else {
            if (3 === q && 1 === e) {
              for (q = 0;q < p;q++) {
                e = a[q], h[q] = d(f(e & 4278255360 | e >> 16 & 255 | (e & 255) << 16));
              }
            } else {
              for (n.somewhatImplemented("Image Format Conversion: " + x[q] + " -> " + x[e]), q = 0;q < p;q++) {
                h[q] = a[q];
              }
            }
          }
        }
      }
    };
  })(t = k.ColorUtilities || (k.ColorUtilities = {}));
  a = function() {
    function e(f) {
      void 0 === f && (f = 32);
      this._list = [];
      this._maxSize = f;
    }
    e.prototype.acquire = function(f) {
      if (e._enabled) {
        for (var c = this._list, d = 0;d < c.length;d++) {
          var q = c[d];
          if (q.byteLength >= f) {
            return c.splice(d, 1), q;
          }
        }
      }
      return new ArrayBuffer(f);
    };
    e.prototype.release = function(f) {
      if (e._enabled) {
        var c = this._list;
        c.length === this._maxSize && c.shift();
        c.push(f);
      }
    };
    e.prototype.ensureUint8ArrayLength = function(f, c) {
      if (f.length >= c) {
        return f;
      }
      var d = Math.max(f.length + c, (3 * f.length >> 1) + 1), d = new Uint8Array(this.acquire(d), 0, d);
      d.set(f);
      this.release(f.buffer);
      return d;
    };
    e.prototype.ensureFloat64ArrayLength = function(f, c) {
      if (f.length >= c) {
        return f;
      }
      var d = Math.max(f.length + c, (3 * f.length >> 1) + 1), d = new Float64Array(this.acquire(d * Float64Array.BYTES_PER_ELEMENT), 0, d);
      d.set(f);
      this.release(f.buffer);
      return d;
    };
    e._enabled = !0;
    return e;
  }();
  k.ArrayBufferPool = a;
  (function(e) {
    (function(f) {
      f[f.EXTERNAL_INTERFACE_FEATURE = 1] = "EXTERNAL_INTERFACE_FEATURE";
      f[f.CLIPBOARD_FEATURE = 2] = "CLIPBOARD_FEATURE";
      f[f.SHAREDOBJECT_FEATURE = 3] = "SHAREDOBJECT_FEATURE";
      f[f.VIDEO_FEATURE = 4] = "VIDEO_FEATURE";
      f[f.SOUND_FEATURE = 5] = "SOUND_FEATURE";
      f[f.NETCONNECTION_FEATURE = 6] = "NETCONNECTION_FEATURE";
    })(e.Feature || (e.Feature = {}));
    (function(f) {
      f[f.AVM1_ERROR = 1] = "AVM1_ERROR";
      f[f.AVM2_ERROR = 2] = "AVM2_ERROR";
    })(e.ErrorTypes || (e.ErrorTypes = {}));
    e.instance;
  })(k.Telemetry || (k.Telemetry = {}));
  (function(e) {
    e.instance;
  })(k.FileLoadingService || (k.FileLoadingService = {}));
  (function(e) {
    e[e.BuiltinAbc = 0] = "BuiltinAbc";
    e[e.PlayerglobalAbcs = 1] = "PlayerglobalAbcs";
    e[e.PlayerglobalManifest = 2] = "PlayerglobalManifest";
    e[e.ShellAbc = 3] = "ShellAbc";
  })(k.SystemResourceId || (k.SystemResourceId = {}));
  (function(e) {
    e.instance;
  })(k.SystemResourcesLoadingService || (k.SystemResourcesLoadingService = {}));
  k.registerCSSFont = function(e, f, c) {
    if (inBrowser) {
      var d = document.head;
      d.insertBefore(document.createElement("style"), d.firstChild);
      d = document.styleSheets[0];
      f = "@font-face{font-family:swffont" + e + ";src:url(data:font/opentype;base64," + k.StringUtilities.base64ArrayBuffer(f) + ")}";
      d.insertRule(f, d.cssRules.length);
      c && (c = document.createElement("div"), c.style.fontFamily = "swffont" + e, c.innerHTML = "hello", document.body.appendChild(c), document.body.removeChild(c));
    }
  };
  (function(e) {
    e.instance = {enabled:!1, initJS:function(f) {
    }, registerCallback:function(f) {
    }, unregisterCallback:function(f) {
    }, eval:function(f) {
    }, call:function(f) {
    }, getId:function() {
      return null;
    }};
  })(k.ExternalInterfaceService || (k.ExternalInterfaceService = {}));
  (function(e) {
    e.instance = {setClipboard:function(f) {
    }};
  })(k.ClipboardService || (k.ClipboardService = {}));
  a = function() {
    function e() {
      this._queues = {};
    }
    e.prototype.register = function(f, c) {
      n.assert(f);
      n.assert(c);
      var d = this._queues[f];
      if (d) {
        if (-1 < d.indexOf(c)) {
          return;
        }
      } else {
        d = this._queues[f] = [];
      }
      d.push(c);
    };
    e.prototype.unregister = function(f, c) {
      n.assert(f);
      n.assert(c);
      var d = this._queues[f];
      if (d) {
        var q = d.indexOf(c);
        -1 !== q && d.splice(q, 1);
        0 === d.length && (this._queues[f] = null);
      }
    };
    e.prototype.notify = function(f, c) {
      var d = this._queues[f];
      if (d) {
        d = d.slice();
        c = Array.prototype.slice.call(arguments, 0);
        for (var q = 0;q < d.length;q++) {
          d[q].apply(null, c);
        }
      }
    };
    e.prototype.notify1 = function(f, c) {
      var d = this._queues[f];
      if (d) {
        for (var d = d.slice(), q = 0;q < d.length;q++) {
          (0,d[q])(f, c);
        }
      }
    };
    return e;
  }();
  k.Callback = a;
  (function(e) {
    e[e.None = 0] = "None";
    e[e.PremultipliedAlphaARGB = 1] = "PremultipliedAlphaARGB";
    e[e.StraightAlphaARGB = 2] = "StraightAlphaARGB";
    e[e.StraightAlphaRGBA = 3] = "StraightAlphaRGBA";
    e[e.JPEG = 4] = "JPEG";
    e[e.PNG = 5] = "PNG";
    e[e.GIF = 6] = "GIF";
  })(k.ImageType || (k.ImageType = {}));
  var x = k.ImageType;
  k.getMIMETypeForImageType = function(e) {
    switch(e) {
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
  (function(e) {
    e.toCSSCursor = function(f) {
      switch(f) {
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
    function e() {
      this.promise = new Promise(function(f, c) {
        this.resolve = f;
        this.reject = c;
      }.bind(this));
    }
    e.prototype.then = function(f, c) {
      return this.promise.then(f, c);
    };
    return e;
  }();
  k.PromiseWrapper = a;
})(Shumway || (Shumway = {}));
(function() {
  function k(d) {
    if ("function" !== typeof d) {
      throw new TypeError("Invalid deferred constructor");
    }
    var c = m();
    d = new d(c);
    var f = c.resolve;
    if ("function" !== typeof f) {
      throw new TypeError("Invalid resolve construction function");
    }
    c = c.reject;
    if ("function" !== typeof c) {
      throw new TypeError("Invalid reject construction function");
    }
    return {promise:d, resolve:f, reject:c};
  }
  function r(d, c) {
    if ("object" !== typeof d || null === d) {
      return !1;
    }
    try {
      var f = d.then;
      if ("function" !== typeof f) {
        return !1;
      }
      f.call(d, c.resolve, c.reject);
    } catch (e) {
      f = c.reject, f(e);
    }
    return !0;
  }
  function g(d) {
    return "object" === typeof d && null !== d && "undefined" !== typeof d.promiseStatus;
  }
  function b(d, c) {
    if ("unresolved" === d.promiseStatus) {
      var f = d.rejectReactions;
      d.result = c;
      d.resolveReactions = void 0;
      d.rejectReactions = void 0;
      d.promiseStatus = "has-rejection";
      v(f, c);
    }
  }
  function v(d, c) {
    for (var f = 0;f < d.length;f++) {
      n({reaction:d[f], argument:c});
    }
  }
  function n(d) {
    0 === c.length && setTimeout(a, 0);
    c.push(d);
  }
  function a() {
    for (;0 < c.length;) {
      var d = c[0];
      try {
        a: {
          var q = d.reaction, f = q.deferred, a = q.handler, h = void 0, p = void 0;
          try {
            h = a(d.argument);
          } catch (m) {
            var b = f.reject;
            b(m);
            break a;
          }
          if (h === f.promise) {
            b = f.reject, b(new TypeError("Self resolution"));
          } else {
            try {
              if (p = r(h, f), !p) {
                var l = f.resolve;
                l(h);
              }
            } catch (n) {
              b = f.reject, b(n);
            }
          }
        }
      } catch (t) {
        if ("function" === typeof e.onerror) {
          e.onerror(t);
        }
      }
      c.shift();
    }
  }
  function h(d) {
    throw d;
  }
  function p(d) {
    return d;
  }
  function l(d) {
    return function(c) {
      b(d, c);
    };
  }
  function w(d) {
    return function(c) {
      if ("unresolved" === d.promiseStatus) {
        var f = d.resolveReactions;
        d.result = c;
        d.resolveReactions = void 0;
        d.rejectReactions = void 0;
        d.promiseStatus = "has-resolution";
        v(f, c);
      }
    };
  }
  function m() {
    function d(c, f) {
      d.resolve = c;
      d.reject = f;
    }
    return d;
  }
  function t(d, c, f) {
    return function(e) {
      if (e === d) {
        return f(new TypeError("Self resolution"));
      }
      var a = d.promiseConstructor;
      if (g(e) && e.promiseConstructor === a) {
        return e.then(c, f);
      }
      a = k(a);
      return r(e, a) ? a.promise.then(c, f) : c(e);
    };
  }
  function x(d, c, f, e) {
    return function(a) {
      c[d] = a;
      e.countdown--;
      0 === e.countdown && f.resolve(c);
    };
  }
  function e(d) {
    if ("function" !== typeof d) {
      throw new TypeError("resolver is not a function");
    }
    if ("object" !== typeof this) {
      throw new TypeError("Promise to initialize is not an object");
    }
    this.promiseStatus = "unresolved";
    this.resolveReactions = [];
    this.rejectReactions = [];
    this.result = void 0;
    var c = w(this), f = l(this);
    try {
      d(c, f);
    } catch (a) {
      b(this, a);
    }
    this.promiseConstructor = e;
    return this;
  }
  var f = Function("return this")();
  if (f.Promise) {
    "function" !== typeof f.Promise.all && (f.Promise.all = function(d) {
      var c = 0, e = [], a, h, p = new f.Promise(function(d, c) {
        a = d;
        h = c;
      });
      d.forEach(function(d, f) {
        c++;
        d.then(function(d) {
          e[f] = d;
          c--;
          0 === c && a(e);
        }, h);
      });
      0 === c && a(e);
      return p;
    }), "function" !== typeof f.Promise.resolve && (f.Promise.resolve = function(d) {
      return new f.Promise(function(c) {
        c(d);
      });
    });
  } else {
    var c = [];
    e.all = function(d) {
      var c = k(this), f = [], e = {countdown:0}, a = 0;
      d.forEach(function(d) {
        this.cast(d).then(x(a, f, c, e), c.reject);
        a++;
        e.countdown++;
      }, this);
      0 === a && c.resolve(f);
      return c.promise;
    };
    e.cast = function(d) {
      if (g(d)) {
        return d;
      }
      var c = k(this);
      c.resolve(d);
      return c.promise;
    };
    e.reject = function(d) {
      var c = k(this);
      c.reject(d);
      return c.promise;
    };
    e.resolve = function(d) {
      var c = k(this);
      c.resolve(d);
      return c.promise;
    };
    e.prototype = {"catch":function(d) {
      this.then(void 0, d);
    }, then:function(d, c) {
      if (!g(this)) {
        throw new TypeError("this is not a Promises");
      }
      var f = k(this.promiseConstructor), e = "function" === typeof c ? c : h, a = {deferred:f, handler:t(this, "function" === typeof d ? d : p, e)}, e = {deferred:f, handler:e};
      switch(this.promiseStatus) {
        case "unresolved":
          this.resolveReactions.push(a);
          this.rejectReactions.push(e);
          break;
        case "has-resolution":
          n({reaction:a, argument:this.result});
          break;
        case "has-rejection":
          n({reaction:e, argument:this.result});
      }
      return f.promise;
    }};
    f.Promise = e;
  }
})();
"undefined" !== typeof exports && (exports.Shumway = Shumway);
(function() {
  function k(k, g, b) {
    k[g] || Object.defineProperty(k, g, {value:b, writable:!0, configurable:!0, enumerable:!1});
  }
  k(String.prototype, "padRight", function(k, g) {
    var b = this, v = b.replace(/\033\[[0-9]*m/g, "").length;
    if (!k || v >= g) {
      return b;
    }
    for (var v = (g - v) / k.length, n = 0;n < v;n++) {
      b += k;
    }
    return b;
  });
  k(String.prototype, "padLeft", function(k, g) {
    var b = this, v = b.length;
    if (!k || v >= g) {
      return b;
    }
    for (var v = (g - v) / k.length, n = 0;n < v;n++) {
      b = k + b;
    }
    return b;
  });
  k(String.prototype, "trim", function() {
    return this.replace(/^\s+|\s+$/g, "");
  });
  k(String.prototype, "endsWith", function(k) {
    return -1 !== this.indexOf(k, this.length - k.length);
  });
  k(Array.prototype, "replace", function(k, g) {
    if (k === g) {
      return 0;
    }
    for (var b = 0, v = 0;v < this.length;v++) {
      this[v] === k && (this[v] = g, b++);
    }
    return b;
  });
})();
(function(k) {
  (function(r) {
    var g = k.isObject, b = function() {
      function a(a, p, b, n) {
        this.shortName = a;
        this.longName = p;
        this.type = b;
        n = n || {};
        this.positional = n.positional;
        this.parseFn = n.parse;
        this.value = n.defaultValue;
      }
      a.prototype.parse = function(a) {
        this.value = "boolean" === this.type ? a : "number" === this.type ? parseInt(a, 10) : a;
        this.parseFn && this.parseFn(this.value);
      };
      return a;
    }();
    r.Argument = b;
    var v = function() {
      function a() {
        this.args = [];
      }
      a.prototype.addArgument = function(a, p, l, n) {
        a = new b(a, p, l, n);
        this.args.push(a);
        return a;
      };
      a.prototype.addBoundOption = function(a) {
        this.args.push(new b(a.shortName, a.longName, a.type, {parse:function(p) {
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
        var p = {}, b = [];
        this.args.forEach(function(e) {
          e.positional ? b.push(e) : (p["-" + e.shortName] = e, p["--" + e.longName] = e);
        });
        for (var n = [];a.length;) {
          var m = a.shift(), t = null, x = m;
          if ("--" == m) {
            n = n.concat(a);
            break;
          } else {
            if ("-" == m.slice(0, 1) || "--" == m.slice(0, 2)) {
              t = p[m];
              if (!t) {
                continue;
              }
              x = "boolean" !== t.type ? a.shift() : !0;
            } else {
              b.length ? t = b.shift() : n.push(x);
            }
          }
          t && t.parse(x);
        }
        return n;
      };
      return a;
    }();
    r.ArgumentParser = v;
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
            var b = this.options[p];
            if (b instanceof a && b.name === h.name) {
              return b;
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
    v = function() {
      function a(a, p, b, n, m, t) {
        void 0 === t && (t = null);
        this.longName = p;
        this.shortName = a;
        this.type = b;
        this.value = this.defaultValue = n;
        this.description = m;
        this.config = t;
      }
      a.prototype.parse = function(a) {
        this.value = a;
      };
      a.prototype.trace = function(a) {
        a.writeLn(("-" + this.shortName + "|--" + this.longName).padRight(" ", 30) + " = " + this.type + " " + this.value + " [" + this.defaultValue + "] (" + this.description + ")");
      };
      return a;
    }();
    r.Option = v;
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
      function b(b, n) {
        this._parent = b;
        this._timers = k.ObjectUtilities.createMap();
        this._name = n;
        this._count = this._total = this._last = this._begin = 0;
      }
      b.time = function(g, n) {
        b.start(g);
        n();
        b.stop();
      };
      b.start = function(g) {
        b._top = b._top._timers[g] || (b._top._timers[g] = new b(b._top, g));
        b._top.start();
        g = b._flat._timers[g] || (b._flat._timers[g] = new b(b._flat, g));
        g.start();
        b._flatStack.push(g);
      };
      b.stop = function() {
        b._top.stop();
        b._top = b._top._parent;
        b._flatStack.pop().stop();
      };
      b.stopStart = function(g) {
        b.stop();
        b.start(g);
      };
      b.prototype.start = function() {
        this._begin = k.getTicks();
      };
      b.prototype.stop = function() {
        this._last = k.getTicks() - this._begin;
        this._total += this._last;
        this._count += 1;
      };
      b.prototype.toJSON = function() {
        return {name:this._name, total:this._total, timers:this._timers};
      };
      b.prototype.trace = function(b) {
        b.enter(this._name + ": " + this._total.toFixed(2) + " ms, count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var n in this._timers) {
          this._timers[n].trace(b);
        }
        b.outdent();
      };
      b.trace = function(g) {
        b._base.trace(g);
        b._flat.trace(g);
      };
      b._base = new b(null, "Total");
      b._top = b._base;
      b._flat = new b(null, "Flat");
      b._flatStack = [];
      return b;
    }();
    r.Timer = g;
    g = function() {
      function b(b) {
        this._enabled = b;
        this.clear();
      }
      Object.defineProperty(b.prototype, "counts", {get:function() {
        return this._counts;
      }, enumerable:!0, configurable:!0});
      b.prototype.setEnabled = function(b) {
        this._enabled = b;
      };
      b.prototype.clear = function() {
        this._counts = k.ObjectUtilities.createMap();
        this._times = k.ObjectUtilities.createMap();
      };
      b.prototype.toJSON = function() {
        return {counts:this._counts, times:this._times};
      };
      b.prototype.count = function(b, n, a) {
        void 0 === n && (n = 1);
        void 0 === a && (a = 0);
        if (this._enabled) {
          return void 0 === this._counts[b] && (this._counts[b] = 0, this._times[b] = 0), this._counts[b] += n, this._times[b] += a, this._counts[b];
        }
      };
      b.prototype.trace = function(b) {
        for (var n in this._counts) {
          b.writeLn(n + ": " + this._counts[n]);
        }
      };
      b.prototype._pairToString = function(b, n) {
        var a = n[0], h = n[1], p = b[a], a = a + ": " + h;
        p && (a += ", " + p.toFixed(4), 1 < h && (a += " (" + (p / h).toFixed(4) + ")"));
        return a;
      };
      b.prototype.toStringSorted = function() {
        var b = this, n = this._times, a = [], h;
        for (h in this._counts) {
          a.push([h, this._counts[h]]);
        }
        a.sort(function(a, h) {
          return h[1] - a[1];
        });
        return a.map(function(a) {
          return b._pairToString(n, a);
        }).join(", ");
      };
      b.prototype.traceSorted = function(b, n) {
        void 0 === n && (n = !1);
        var a = this, h = this._times, p = [], l;
        for (l in this._counts) {
          p.push([l, this._counts[l]]);
        }
        p.sort(function(a, h) {
          return h[1] - a[1];
        });
        n ? b.writeLn(p.map(function(p) {
          return a._pairToString(h, p);
        }).join(", ")) : p.forEach(function(p) {
          b.writeLn(a._pairToString(h, p));
        });
      };
      b.instance = new b(!0);
      return b;
    }();
    r.Counter = g;
    g = function() {
      function b(b) {
        this._samples = new Float64Array(b);
        this._index = this._count = 0;
      }
      b.prototype.push = function(b) {
        this._count < this._samples.length && this._count++;
        this._index++;
        this._samples[this._index % this._samples.length] = b;
      };
      b.prototype.average = function() {
        for (var b = 0, n = 0;n < this._count;n++) {
          b += this._samples[n];
        }
        return b / this._count;
      };
      return b;
    }();
    r.Average = g;
  })(k.Metrics || (k.Metrics = {}));
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function(k, r) {
  function g() {
    this.constructor = k;
  }
  for (var b in r) {
    r.hasOwnProperty(b) && (k[b] = r[b]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(k) {
    function g(c) {
      for (var d = Math.max.apply(null, c), q = c.length, f = 1 << d, e = new Uint32Array(f), a = d << 16 | 65535, h = 0;h < f;h++) {
        e[h] = a;
      }
      for (var a = 0, h = 1, p = 2;h <= d;a <<= 1, ++h, p <<= 1) {
        for (var b = 0;b < q;++b) {
          if (c[b] === h) {
            for (var m = 0, l = 0;l < h;++l) {
              m = 2 * m + (a >> l & 1);
            }
            for (l = m;l < f;l += p) {
              e[l] = h << 16 | b;
            }
            ++a;
          }
        }
      }
      return {codes:e, maxBits:d};
    }
    var b;
    (function(c) {
      c[c.INIT = 0] = "INIT";
      c[c.BLOCK_0 = 1] = "BLOCK_0";
      c[c.BLOCK_1 = 2] = "BLOCK_1";
      c[c.BLOCK_2_PRE = 3] = "BLOCK_2_PRE";
      c[c.BLOCK_2 = 4] = "BLOCK_2";
      c[c.DONE = 5] = "DONE";
      c[c.ERROR = 6] = "ERROR";
      c[c.VERIFY_HEADER = 7] = "VERIFY_HEADER";
    })(b || (b = {}));
    b = function() {
      function c(d) {
      }
      c.prototype.push = function(d) {
      };
      c.prototype.close = function() {
      };
      c.create = function(d) {
        return "undefined" !== typeof ShumwayCom && ShumwayCom.createSpecialInflate ? new x(d, ShumwayCom.createSpecialInflate) : new v(d);
      };
      c.prototype._processZLibHeader = function(d, c, f) {
        if (c + 2 > f) {
          return 0;
        }
        d = d[c] << 8 | d[c + 1];
        c = null;
        2048 !== (d & 3840) ? c = "inflate: unknown compression method" : 0 !== d % 31 ? c = "inflate: bad FCHECK" : 0 !== (d & 32) && (c = "inflate: FDICT bit set");
        if (c) {
          if (this.onError) {
            this.onError(c);
          }
          return -1;
        }
        return 2;
      };
      c.inflate = function(d, q, f) {
        var e = new Uint8Array(q), a = 0;
        q = c.create(f);
        q.onData = function(d) {
          var c = Math.min(d.length, e.length - a);
          c && k.memCopy(e, d, a, 0, c);
          a += c;
        };
        q.onError = function(d) {
          throw Error(d);
        };
        q.push(d);
        q.close();
        return e;
      };
      return c;
    }();
    k.Inflate = b;
    var v = function(c) {
      function d(d) {
        c.call(this, d);
        this._buffer = null;
        this._bitLength = this._bitBuffer = this._bufferPosition = this._bufferSize = 0;
        this._window = new Uint8Array(65794);
        this._windowPosition = 0;
        this._state = d ? 7 : 0;
        this._isFinalBlock = !1;
        this._distanceTable = this._literalTable = null;
        this._block0Read = 0;
        this._block2State = null;
        this._copyState = {state:0, len:0, lenBits:0, dist:0, distBits:0};
        if (!t) {
          n = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);
          a = new Uint16Array(30);
          h = new Uint8Array(30);
          for (var f = d = 0, e = 1;30 > d;++d) {
            a[d] = e, e += 1 << (h[d] = ~~((f += 2 < d ? 1 : 0) / 2));
          }
          var b = new Uint8Array(288);
          for (d = 0;32 > d;++d) {
            b[d] = 5;
          }
          p = g(b.subarray(0, 32));
          l = new Uint16Array(29);
          w = new Uint8Array(29);
          f = d = 0;
          for (e = 3;29 > d;++d) {
            l[d] = e - (28 == d ? 1 : 0), e += 1 << (w[d] = ~~((f += 4 < d ? 1 : 0) / 4 % 6));
          }
          for (d = 0;288 > d;++d) {
            b[d] = 144 > d || 279 < d ? 8 : 256 > d ? 9 : 7;
          }
          m = g(b);
          t = !0;
        }
      }
      __extends(d, c);
      d.prototype.push = function(d) {
        if (!this._buffer || this._buffer.length < this._bufferSize + d.length) {
          var c = new Uint8Array(this._bufferSize + d.length);
          this._buffer && c.set(this._buffer);
          this._buffer = c;
        }
        this._buffer.set(d, this._bufferSize);
        this._bufferSize += d.length;
        this._bufferPosition = 0;
        d = !1;
        do {
          c = this._windowPosition;
          if (0 === this._state && (d = this._decodeInitState())) {
            break;
          }
          switch(this._state) {
            case 1:
              d = this._decodeBlock0();
              break;
            case 3:
              if (d = this._decodeBlock2Pre()) {
                break;
              }
            ;
            case 2:
            ;
            case 4:
              d = this._decodeBlock();
              break;
            case 6:
            ;
            case 5:
              this._bufferPosition = this._bufferSize;
              break;
            case 7:
              var f = this._processZLibHeader(this._buffer, this._bufferPosition, this._bufferSize);
              0 < f ? (this._bufferPosition += f, this._state = 0) : 0 === f ? d = !0 : this._state = 6;
          }
          if (0 < this._windowPosition - c) {
            this.onData(this._window.subarray(c, this._windowPosition));
          }
          65536 <= this._windowPosition && ("copyWithin" in this._buffer ? this._window.copyWithin(0, this._windowPosition - 32768, this._windowPosition) : this._window.set(this._window.subarray(this._windowPosition - 32768, this._windowPosition)), this._windowPosition = 32768);
        } while (!d && this._bufferPosition < this._bufferSize);
        this._bufferPosition < this._bufferSize ? ("copyWithin" in this._buffer ? this._buffer.copyWithin(0, this._bufferPosition, this._bufferSize) : this._buffer.set(this._buffer.subarray(this._bufferPosition, this._bufferSize)), this._bufferSize -= this._bufferPosition) : this._bufferSize = 0;
      };
      d.prototype._decodeInitState = function() {
        if (this._isFinalBlock) {
          return this._state = 5, !1;
        }
        var d = this._buffer, c = this._bufferSize, f = this._bitBuffer, e = this._bitLength, a = this._bufferPosition;
        if (3 > (c - a << 3) + e) {
          return !0;
        }
        3 > e && (f |= d[a++] << e, e += 8);
        var h = f & 7, f = f >> 3, e = e - 3;
        switch(h >> 1) {
          case 0:
            e = f = 0;
            if (4 > c - a) {
              return !0;
            }
            var b = d[a] | d[a + 1] << 8, d = d[a + 2] | d[a + 3] << 8, a = a + 4;
            if (65535 !== (b ^ d)) {
              this._error("inflate: invalid block 0 length");
              d = 6;
              break;
            }
            0 === b ? d = 0 : (this._block0Read = b, d = 1);
            break;
          case 1:
            d = 2;
            this._literalTable = m;
            this._distanceTable = p;
            break;
          case 2:
            if (26 > (c - a << 3) + e) {
              return !0;
            }
            for (;14 > e;) {
              f |= d[a++] << e, e += 8;
            }
            b = (f >> 10 & 15) + 4;
            if ((c - a << 3) + e < 14 + 3 * b) {
              return !0;
            }
            for (var c = {numLiteralCodes:(f & 31) + 257, numDistanceCodes:(f >> 5 & 31) + 1, codeLengthTable:void 0, bitLengths:void 0, codesRead:0, dupBits:0}, f = f >> 14, e = e - 14, l = new Uint8Array(19), t = 0;t < b;++t) {
              3 > e && (f |= d[a++] << e, e += 8), l[n[t]] = f & 7, f >>= 3, e -= 3;
            }
            for (;19 > t;t++) {
              l[n[t]] = 0;
            }
            c.bitLengths = new Uint8Array(c.numLiteralCodes + c.numDistanceCodes);
            c.codeLengthTable = g(l);
            this._block2State = c;
            d = 3;
            break;
          default:
            return this._error("inflate: unsupported block type"), !1;
        }
        this._isFinalBlock = !!(h & 1);
        this._state = d;
        this._bufferPosition = a;
        this._bitBuffer = f;
        this._bitLength = e;
        return !1;
      };
      d.prototype._error = function(d) {
        if (this.onError) {
          this.onError(d);
        }
      };
      d.prototype._decodeBlock0 = function() {
        var d = this._bufferPosition, c = this._windowPosition, f = this._block0Read, e = 65794 - c, a = this._bufferSize - d, h = a < f, p = Math.min(e, a, f);
        this._window.set(this._buffer.subarray(d, d + p), c);
        this._windowPosition = c + p;
        this._bufferPosition = d + p;
        this._block0Read = f - p;
        return f === p ? (this._state = 0, !1) : h && e < a;
      };
      d.prototype._readBits = function(d) {
        var c = this._bitBuffer, f = this._bitLength;
        if (d > f) {
          var e = this._bufferPosition, a = this._bufferSize;
          do {
            if (e >= a) {
              return this._bufferPosition = e, this._bitBuffer = c, this._bitLength = f, -1;
            }
            c |= this._buffer[e++] << f;
            f += 8;
          } while (d > f);
          this._bufferPosition = e;
        }
        this._bitBuffer = c >> d;
        this._bitLength = f - d;
        return c & (1 << d) - 1;
      };
      d.prototype._readCode = function(d) {
        var c = this._bitBuffer, f = this._bitLength, e = d.maxBits;
        if (e > f) {
          var a = this._bufferPosition, h = this._bufferSize;
          do {
            if (a >= h) {
              return this._bufferPosition = a, this._bitBuffer = c, this._bitLength = f, -1;
            }
            c |= this._buffer[a++] << f;
            f += 8;
          } while (e > f);
          this._bufferPosition = a;
        }
        d = d.codes[c & (1 << e) - 1];
        e = d >> 16;
        if (d & 32768) {
          return this._error("inflate: invalid encoding"), this._state = 6, -1;
        }
        this._bitBuffer = c >> e;
        this._bitLength = f - e;
        return d & 65535;
      };
      d.prototype._decodeBlock2Pre = function() {
        var d = this._block2State, c = d.numLiteralCodes + d.numDistanceCodes, f = d.bitLengths, e = d.codesRead, a = 0 < e ? f[e - 1] : 0, h = d.codeLengthTable, p;
        if (0 < d.dupBits) {
          p = this._readBits(d.dupBits);
          if (0 > p) {
            return !0;
          }
          for (;p--;) {
            f[e++] = a;
          }
          d.dupBits = 0;
        }
        for (;e < c;) {
          var b = this._readCode(h);
          if (0 > b) {
            return d.codesRead = e, !0;
          }
          if (16 > b) {
            f[e++] = a = b;
          } else {
            var m;
            switch(b) {
              case 16:
                m = 2;
                p = 3;
                b = a;
                break;
              case 17:
                p = m = 3;
                b = 0;
                break;
              case 18:
                m = 7, p = 11, b = 0;
            }
            for (;p--;) {
              f[e++] = b;
            }
            p = this._readBits(m);
            if (0 > p) {
              return d.codesRead = e, d.dupBits = m, !0;
            }
            for (;p--;) {
              f[e++] = b;
            }
            a = b;
          }
        }
        this._literalTable = g(f.subarray(0, d.numLiteralCodes));
        this._distanceTable = g(f.subarray(d.numLiteralCodes));
        this._state = 4;
        this._block2State = null;
        return !1;
      };
      d.prototype._decodeBlock = function() {
        var d = this._literalTable, c = this._distanceTable, f = this._window, e = this._windowPosition, p = this._copyState, b, m, n, t;
        if (0 !== p.state) {
          switch(p.state) {
            case 1:
              if (0 > (b = this._readBits(p.lenBits))) {
                return !0;
              }
              p.len += b;
              p.state = 2;
            case 2:
              if (0 > (b = this._readCode(c))) {
                return !0;
              }
              p.distBits = h[b];
              p.dist = a[b];
              p.state = 3;
            case 3:
              b = 0;
              if (0 < p.distBits && 0 > (b = this._readBits(p.distBits))) {
                return !0;
              }
              t = p.dist + b;
              m = p.len;
              for (b = e - t;m--;) {
                f[e++] = f[b++];
              }
              p.state = 0;
              if (65536 <= e) {
                return this._windowPosition = e, !1;
              }
              break;
          }
        }
        do {
          b = this._readCode(d);
          if (0 > b) {
            return this._windowPosition = e, !0;
          }
          if (256 > b) {
            f[e++] = b;
          } else {
            if (256 < b) {
              this._windowPosition = e;
              b -= 257;
              n = w[b];
              m = l[b];
              b = 0 === n ? 0 : this._readBits(n);
              if (0 > b) {
                return p.state = 1, p.len = m, p.lenBits = n, !0;
              }
              m += b;
              b = this._readCode(c);
              if (0 > b) {
                return p.state = 2, p.len = m, !0;
              }
              n = h[b];
              t = a[b];
              b = 0 === n ? 0 : this._readBits(n);
              if (0 > b) {
                return p.state = 3, p.len = m, p.dist = t, p.distBits = n, !0;
              }
              t += b;
              for (b = e - t;m--;) {
                f[e++] = f[b++];
              }
            } else {
              this._state = 0;
              break;
            }
          }
        } while (65536 > e);
        this._windowPosition = e;
        return !1;
      };
      return d;
    }(b), n, a, h, p, l, w, m, t = !1, x = function(c) {
      function d(d, f) {
        c.call(this, d);
        this._verifyHeader = d;
        this._specialInflate = f();
        this._specialInflate.setDataCallback(function(d) {
          this.onData(d);
        }.bind(this));
      }
      __extends(d, c);
      d.prototype.push = function(d) {
        if (this._verifyHeader) {
          var c;
          this._buffer ? (c = new Uint8Array(this._buffer.length + d.length), c.set(this._buffer), c.set(d, this._buffer.length), this._buffer = null) : c = new Uint8Array(d);
          var f = this._processZLibHeader(c, 0, c.length);
          if (0 === f) {
            this._buffer = c;
            return;
          }
          this._verifyHeader = !0;
          0 < f && (d = c.subarray(f));
        }
        this._specialInflate.push(d);
      };
      d.prototype.close = function() {
        this._specialInflate && (this._specialInflate.close(), this._specialInflate = null);
      };
      return d;
    }(b), e;
    (function(c) {
      c[c.WRITE = 0] = "WRITE";
      c[c.DONE = 1] = "DONE";
      c[c.ZLIB_HEADER = 2] = "ZLIB_HEADER";
    })(e || (e = {}));
    var f = function() {
      function c() {
        this.a = 1;
        this.b = 0;
      }
      c.prototype.update = function(d, c, f) {
        for (var e = this.a, a = this.b;c < f;++c) {
          e = (e + (d[c] & 255)) % 65521, a = (a + e) % 65521;
        }
        this.a = e;
        this.b = a;
      };
      c.prototype.getChecksum = function() {
        return this.b << 16 | this.a;
      };
      return c;
    }();
    k.Adler32 = f;
    e = function() {
      function c(d) {
        this._state = (this._writeZlibHeader = d) ? 2 : 0;
        this._adler32 = d ? new f : null;
      }
      c.prototype.push = function(d) {
        2 === this._state && (this.onData(new Uint8Array([120, 156])), this._state = 0);
        for (var c = d.length, f = new Uint8Array(c + 5 * Math.ceil(c / 65535)), e = 0, a = 0;65535 < c;) {
          f.set(new Uint8Array([0, 255, 255, 0, 0]), e), e += 5, f.set(d.subarray(a, a + 65535), e), a += 65535, e += 65535, c -= 65535;
        }
        f.set(new Uint8Array([0, c & 255, c >> 8 & 255, ~c & 255, ~c >> 8 & 255]), e);
        f.set(d.subarray(a, c), e + 5);
        this.onData(f);
        this._adler32 && this._adler32.update(d, 0, c);
      };
      c.prototype.close = function() {
        this._state = 1;
        this.onData(new Uint8Array([1, 0, 0, 255, 255]));
        if (this._adler32) {
          var d = this._adler32.getChecksum();
          this.onData(new Uint8Array([d & 255, d >> 8 & 255, d >> 16 & 255, d >>> 24 & 255]));
        }
      };
      return c;
    }();
    k.Deflate = e;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    function g(d) {
      for (var c = new Uint16Array(d), f = 0;f < d;f++) {
        c[f] = 1024;
      }
      return c;
    }
    function b(d, c, f, e) {
      for (var a = 1, h = 0, p = 0;p < f;p++) {
        var b = e.decodeBit(d, a + c), a = (a << 1) + b, h = h | b << p
      }
      return h;
    }
    function v(d, c) {
      var f = [];
      f.length = c;
      for (var e = 0;e < c;e++) {
        f[e] = new l(d);
      }
      return f;
    }
    var n = function() {
      function d() {
        this.pos = this.available = 0;
        this.buffer = new Uint8Array(2E3);
      }
      d.prototype.append = function(d) {
        var c = this.pos + this.available, f = c + d.length;
        if (f > this.buffer.length) {
          for (var e = 2 * this.buffer.length;e < f;) {
            e *= 2;
          }
          f = new Uint8Array(e);
          f.set(this.buffer);
          this.buffer = f;
        }
        this.buffer.set(d, c);
        this.available += d.length;
      };
      d.prototype.compact = function() {
        0 !== this.available && (this.buffer.set(this.buffer.subarray(this.pos, this.pos + this.available), 0), this.pos = 0);
      };
      d.prototype.readByte = function() {
        if (0 >= this.available) {
          throw Error("Unexpected end of file");
        }
        this.available--;
        return this.buffer[this.pos++];
      };
      return d;
    }(), a = function() {
      function d(d) {
        this.onData = d;
        this.processed = 0;
      }
      d.prototype.writeBytes = function(d) {
        this.onData.call(null, d);
        this.processed += d.length;
      };
      return d;
    }(), h = function() {
      function d(d) {
        this.outStream = d;
        this.buf = null;
        this.size = this.pos = 0;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      }
      d.prototype.create = function(d) {
        this.buf = new Uint8Array(d);
        this.pos = 0;
        this.size = d;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      };
      d.prototype.putByte = function(d) {
        this.totalPos++;
        this.buf[this.pos++] = d;
        this.pos === this.size && (this.flush(), this.pos = 0, this.isFull = !0);
      };
      d.prototype.getByte = function(d) {
        return this.buf[d <= this.pos ? this.pos - d : this.size - d + this.pos];
      };
      d.prototype.flush = function() {
        this.writePos < this.pos && (this.outStream.writeBytes(this.buf.subarray(this.writePos, this.pos)), this.writePos = this.pos === this.size ? 0 : this.pos);
      };
      d.prototype.copyMatch = function(d, c) {
        for (var f = this.pos, e = this.size, a = this.buf, h = d <= f ? f - d : e - d + f, p = c;0 < p;) {
          for (var b = Math.min(Math.min(p, e - f), e - h), m = 0;m < b;m++) {
            var l = a[h++];
            a[f++] = l;
          }
          f === e && (this.pos = f, this.flush(), f = 0, this.isFull = !0);
          h === e && (h = 0);
          p -= b;
        }
        this.pos = f;
        this.totalPos += c;
      };
      d.prototype.checkDistance = function(d) {
        return d <= this.pos || this.isFull;
      };
      d.prototype.isEmpty = function() {
        return 0 === this.pos && !this.isFull;
      };
      return d;
    }(), p = function() {
      function d(d) {
        this.inStream = d;
        this.code = this.range = 0;
        this.corrupted = !1;
      }
      d.prototype.init = function() {
        0 !== this.inStream.readByte() && (this.corrupted = !0);
        this.range = -1;
        for (var d = 0, c = 0;4 > c;c++) {
          d = d << 8 | this.inStream.readByte();
        }
        d === this.range && (this.corrupted = !0);
        this.code = d;
      };
      d.prototype.isFinishedOK = function() {
        return 0 === this.code;
      };
      d.prototype.decodeDirectBits = function(d) {
        var c = 0, f = this.range, e = this.code;
        do {
          var f = f >>> 1 | 0, e = e - f | 0, a = e >> 31, e = e + (f & a) | 0;
          e === f && (this.corrupted = !0);
          0 <= f && 16777216 > f && (f <<= 8, e = e << 8 | this.inStream.readByte());
          c = (c << 1) + a + 1 | 0;
        } while (--d);
        this.range = f;
        this.code = e;
        return c;
      };
      d.prototype.decodeBit = function(d, c) {
        var f = this.range, e = this.code, a = d[c], h = (f >>> 11) * a;
        e >>> 0 < h ? (a = a + (2048 - a >> 5) | 0, f = h | 0, h = 0) : (a = a - (a >> 5) | 0, e = e - h | 0, f = f - h | 0, h = 1);
        d[c] = a & 65535;
        0 <= f && 16777216 > f && (f <<= 8, e = e << 8 | this.inStream.readByte());
        this.range = f;
        this.code = e;
        return h;
      };
      return d;
    }(), l = function() {
      function d(d) {
        this.numBits = d;
        this.probs = g(1 << d);
      }
      d.prototype.decode = function(d) {
        for (var c = 1, f = 0;f < this.numBits;f++) {
          c = (c << 1) + d.decodeBit(this.probs, c);
        }
        return c - (1 << this.numBits);
      };
      d.prototype.reverseDecode = function(d) {
        return b(this.probs, 0, this.numBits, d);
      };
      return d;
    }(), w = function() {
      function d() {
        this.choice = g(2);
        this.lowCoder = v(3, 16);
        this.midCoder = v(3, 16);
        this.highCoder = new l(8);
      }
      d.prototype.decode = function(d, c) {
        return 0 === d.decodeBit(this.choice, 0) ? this.lowCoder[c].decode(d) : 0 === d.decodeBit(this.choice, 1) ? 8 + this.midCoder[c].decode(d) : 16 + this.highCoder.decode(d);
      };
      return d;
    }(), m = function() {
      function d(d, c) {
        this.rangeDec = new p(d);
        this.outWindow = new h(c);
        this.markerIsMandatory = !1;
        this.dictSizeInProperties = this.dictSize = this.lp = this.pb = this.lc = 0;
        this.leftToUnpack = this.unpackSize = void 0;
        this.reps = new Int32Array(4);
        this.state = 0;
      }
      d.prototype.decodeProperties = function(d) {
        var c = d[0];
        if (225 <= c) {
          throw Error("Incorrect LZMA properties");
        }
        this.lc = c % 9;
        c = c / 9 | 0;
        this.pb = c / 5 | 0;
        this.lp = c % 5;
        for (c = this.dictSizeInProperties = 0;4 > c;c++) {
          this.dictSizeInProperties |= d[c + 1] << 8 * c;
        }
        this.dictSize = this.dictSizeInProperties;
        4096 > this.dictSize && (this.dictSize = 4096);
      };
      d.prototype.create = function() {
        this.outWindow.create(this.dictSize);
        this.init();
        this.rangeDec.init();
        this.reps[0] = 0;
        this.reps[1] = 0;
        this.reps[2] = 0;
        this.state = this.reps[3] = 0;
        this.leftToUnpack = this.unpackSize;
      };
      d.prototype.decodeLiteral = function(d, c) {
        var f = this.outWindow, e = this.rangeDec, a = 0;
        f.isEmpty() || (a = f.getByte(1));
        var h = 1, a = 768 * (((f.totalPos & (1 << this.lp) - 1) << this.lc) + (a >> 8 - this.lc));
        if (7 <= d) {
          f = f.getByte(c + 1);
          do {
            var p = f >> 7 & 1, f = f << 1, b = e.decodeBit(this.litProbs, a + ((1 + p << 8) + h)), h = h << 1 | b;
            if (p !== b) {
              break;
            }
          } while (256 > h);
        }
        for (;256 > h;) {
          h = h << 1 | e.decodeBit(this.litProbs, a + h);
        }
        return h - 256 & 255;
      };
      d.prototype.decodeDistance = function(d) {
        var c = d;
        3 < c && (c = 3);
        d = this.rangeDec;
        c = this.posSlotDecoder[c].decode(d);
        if (4 > c) {
          return c;
        }
        var f = (c >> 1) - 1, e = (2 | c & 1) << f;
        14 > c ? e = e + b(this.posDecoders, e - c, f, d) | 0 : (e = e + (d.decodeDirectBits(f - 4) << 4) | 0, e = e + this.alignDecoder.reverseDecode(d) | 0);
        return e;
      };
      d.prototype.init = function() {
        this.litProbs = g(768 << this.lc + this.lp);
        this.posSlotDecoder = v(6, 4);
        this.alignDecoder = new l(4);
        this.posDecoders = g(115);
        this.isMatch = g(192);
        this.isRep = g(12);
        this.isRepG0 = g(12);
        this.isRepG1 = g(12);
        this.isRepG2 = g(12);
        this.isRep0Long = g(192);
        this.lenDecoder = new w;
        this.repLenDecoder = new w;
      };
      d.prototype.decode = function(d) {
        for (var c = this.rangeDec, a = this.outWindow, h = this.pb, p = this.dictSize, b = this.markerIsMandatory, m = this.leftToUnpack, l = this.isMatch, n = this.isRep, w = this.isRepG0, g = this.isRepG1, k = this.isRepG2, r = this.isRep0Long, v = this.lenDecoder, z = this.repLenDecoder, A = this.reps[0], C = this.reps[1], y = this.reps[2], F = this.reps[3], B = this.state;;) {
          if (d && 48 > c.inStream.available) {
            this.outWindow.flush();
            break;
          }
          if (0 === m && !b && (this.outWindow.flush(), c.isFinishedOK())) {
            return e;
          }
          var E = a.totalPos & (1 << h) - 1;
          if (0 === c.decodeBit(l, (B << 4) + E)) {
            if (0 === m) {
              return t;
            }
            a.putByte(this.decodeLiteral(B, A));
            B = 4 > B ? 0 : 10 > B ? B - 3 : B - 6;
            m--;
          } else {
            if (0 !== c.decodeBit(n, B)) {
              if (0 === m || a.isEmpty()) {
                return t;
              }
              if (0 === c.decodeBit(w, B)) {
                if (0 === c.decodeBit(r, (B << 4) + E)) {
                  B = 7 > B ? 9 : 11;
                  a.putByte(a.getByte(A + 1));
                  m--;
                  continue;
                }
              } else {
                var G;
                0 === c.decodeBit(g, B) ? G = C : (0 === c.decodeBit(k, B) ? G = y : (G = F, F = y), y = C);
                C = A;
                A = G;
              }
              E = z.decode(c, E);
              B = 7 > B ? 8 : 11;
            } else {
              F = y;
              y = C;
              C = A;
              E = v.decode(c, E);
              B = 7 > B ? 7 : 10;
              A = this.decodeDistance(E);
              if (-1 === A) {
                return this.outWindow.flush(), c.isFinishedOK() ? x : t;
              }
              if (0 === m || A >= p || !a.checkDistance(A)) {
                return t;
              }
            }
            E += 2;
            G = !1;
            void 0 !== m && m < E && (E = m, G = !0);
            a.copyMatch(A + 1, E);
            m -= E;
            if (G) {
              return t;
            }
          }
        }
        this.state = B;
        this.reps[0] = A;
        this.reps[1] = C;
        this.reps[2] = y;
        this.reps[3] = F;
        this.leftToUnpack = m;
        return f;
      };
      d.prototype.flushOutput = function() {
        this.outWindow.flush();
      };
      return d;
    }(), t = 0, x = 1, e = 2, f = 3, c;
    (function(d) {
      d[d.WAIT_FOR_LZMA_HEADER = 0] = "WAIT_FOR_LZMA_HEADER";
      d[d.WAIT_FOR_SWF_HEADER = 1] = "WAIT_FOR_SWF_HEADER";
      d[d.PROCESS_DATA = 2] = "PROCESS_DATA";
      d[d.CLOSED = 3] = "CLOSED";
      d[d.ERROR = 4] = "ERROR";
    })(c || (c = {}));
    c = function() {
      function d(d) {
        void 0 === d && (d = !1);
        this._state = d ? 1 : 0;
        this.buffer = null;
      }
      d.prototype.push = function(d) {
        if (2 > this._state) {
          var c = this.buffer ? this.buffer.length : 0, e = (1 === this._state ? 17 : 13) + 5;
          if (c + d.length < e) {
            e = new Uint8Array(c + d.length);
            0 < c && e.set(this.buffer);
            e.set(d, c);
            this.buffer = e;
            return;
          }
          var h = new Uint8Array(e);
          0 < c && h.set(this.buffer);
          h.set(d.subarray(0, e - c), c);
          this._inStream = new n;
          this._inStream.append(h.subarray(e - 5));
          this._outStream = new a(function(d) {
            this.onData.call(null, d);
          }.bind(this));
          this._decoder = new m(this._inStream, this._outStream);
          if (1 === this._state) {
            this._decoder.decodeProperties(h.subarray(12, 17)), this._decoder.markerIsMandatory = !1, this._decoder.unpackSize = ((h[4] | h[5] << 8 | h[6] << 16 | h[7] << 24) >>> 0) - 8;
          } else {
            this._decoder.decodeProperties(h.subarray(0, 5));
            for (var c = 0, p = !1, b = 0;8 > b;b++) {
              var l = h[5 + b];
              255 !== l && (p = !0);
              c |= l << 8 * b;
            }
            this._decoder.markerIsMandatory = !p;
            this._decoder.unpackSize = p ? c : void 0;
          }
          this._decoder.create();
          d = d.subarray(e);
          this._state = 2;
        } else {
          if (2 !== this._state) {
            return;
          }
        }
        try {
          this._inStream.append(d);
          var t = this._decoder.decode(!0);
          this._inStream.compact();
          t !== f && this._checkError(t);
        } catch (w) {
          this._decoder.flushOutput(), this._decoder = null, this._error(w);
        }
      };
      d.prototype.close = function() {
        if (2 === this._state) {
          this._state = 3;
          try {
            var d = this._decoder.decode(!1);
            this._checkError(d);
          } catch (c) {
            this._decoder.flushOutput(), this._error(c);
          }
          this._decoder = null;
        }
      };
      d.prototype._error = function(d) {
        this._state = 4;
        if (this.onError) {
          this.onError(d);
        }
      };
      d.prototype._checkError = function(d) {
        var c;
        d === t ? c = "LZMA decoding error" : d === f ? c = "Decoding is not complete" : d === x ? void 0 !== this._decoder.unpackSize && this._decoder.unpackSize !== this._outStream.processed && (c = "Finished with end marker before than specified size") : c = "Internal LZMA Error";
        c && this._error(c);
      };
      return d;
    }();
    k.LzmaDecoder = c;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    function g(a, e) {
      a !== h(a, 0, e) && throwError("RangeError", Errors.ParamRangeError);
    }
    function b(a) {
      return "string" === typeof a ? a : void 0 == a ? null : a + "";
    }
    var v = k.Debug.notImplemented, n = k.StringUtilities.utf8decode, a = k.StringUtilities.utf8encode, h = k.NumberUtilities.clamp, p = function() {
      return function(a, e, f) {
        this.buffer = a;
        this.length = e;
        this.littleEndian = f;
      };
    }();
    r.PlainObjectDataBuffer = p;
    for (var l = new Uint32Array(33), w = 1, m = 0;32 >= w;w++) {
      l[w] = m = m << 1 | 1;
    }
    var t;
    (function(a) {
      a[a.U8 = 1] = "U8";
      a[a.I32 = 2] = "I32";
      a[a.F32 = 4] = "F32";
    })(t || (t = {}));
    w = function() {
      function m(e) {
        void 0 === e && (e = m.INITIAL_SIZE);
        this._buffer || (this._buffer = new ArrayBuffer(e), this._position = this._length = 0, this._resetViews(), this._littleEndian = m._nativeLittleEndian, this._bitLength = this._bitBuffer = 0);
      }
      m.FromArrayBuffer = function(e, f) {
        void 0 === f && (f = -1);
        var c = Object.create(m.prototype);
        c._buffer = e;
        c._length = -1 === f ? e.byteLength : f;
        c._position = 0;
        c._resetViews();
        c._littleEndian = m._nativeLittleEndian;
        c._bitBuffer = 0;
        c._bitLength = 0;
        return c;
      };
      m.FromPlainObject = function(e) {
        var f = m.FromArrayBuffer(e.buffer, e.length);
        f._littleEndian = e.littleEndian;
        return f;
      };
      m.prototype.toPlainObject = function() {
        return new p(this._buffer, this._length, this._littleEndian);
      };
      m.prototype._resetViews = function() {
        this._u8 = new Uint8Array(this._buffer);
        this._f32 = this._i32 = null;
      };
      m.prototype._requestViews = function(e) {
        0 === (this._buffer.byteLength & 3) && (null === this._i32 && e & 2 && (this._i32 = new Int32Array(this._buffer)), null === this._f32 && e & 4 && (this._f32 = new Float32Array(this._buffer)));
      };
      m.prototype.getBytes = function() {
        return new Uint8Array(this._buffer, 0, this._length);
      };
      m.prototype._ensureCapacity = function(e) {
        var f = this._buffer;
        if (f.byteLength < e) {
          for (var c = Math.max(f.byteLength, 1);c < e;) {
            c *= 2;
          }
          e = m._arrayBufferPool.acquire(c);
          c = this._u8;
          this._buffer = e;
          this._resetViews();
          this._u8.set(c);
          m._arrayBufferPool.release(f);
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
      m.prototype.readBytes = function(e, f) {
        var c = 0;
        void 0 === c && (c = 0);
        void 0 === f && (f = 0);
        var d = this._position;
        c || (c = 0);
        f || (f = this._length - d);
        d + f > this._length && throwError("EOFError", Errors.EOFError);
        e.length < c + f && (e._ensureCapacity(c + f), e.length = c + f);
        e._u8.set(new Uint8Array(this._buffer, d, f), c);
        this._position += f;
      };
      m.prototype.readShort = function() {
        return this.readUnsignedShort() << 16 >> 16;
      };
      m.prototype.readUnsignedShort = function() {
        var e = this._u8, f = this._position;
        f + 2 > this._length && throwError("EOFError", Errors.EOFError);
        var c = e[f + 0], e = e[f + 1];
        this._position = f + 2;
        return this._littleEndian ? e << 8 | c : c << 8 | e;
      };
      m.prototype.readInt = function() {
        var e = this._u8, f = this._position;
        f + 4 > this._length && throwError("EOFError", Errors.EOFError);
        var c = e[f + 0], d = e[f + 1], a = e[f + 2], e = e[f + 3];
        this._position = f + 4;
        return this._littleEndian ? e << 24 | a << 16 | d << 8 | c : c << 24 | d << 16 | a << 8 | e;
      };
      m.prototype.readUnsignedInt = function() {
        return this.readInt() >>> 0;
      };
      m.prototype.readFloat = function() {
        var e = this._position;
        e + 4 > this._length && throwError("EOFError", Errors.EOFError);
        this._position = e + 4;
        this._requestViews(4);
        if (this._littleEndian && 0 === (e & 3) && this._f32) {
          return this._f32[e >> 2];
        }
        var f = this._u8, c = k.IntegerUtilities.u8;
        this._littleEndian ? (c[0] = f[e + 0], c[1] = f[e + 1], c[2] = f[e + 2], c[3] = f[e + 3]) : (c[3] = f[e + 0], c[2] = f[e + 1], c[1] = f[e + 2], c[0] = f[e + 3]);
        return k.IntegerUtilities.f32[0];
      };
      m.prototype.readDouble = function() {
        var e = this._u8, f = this._position;
        f + 8 > this._length && throwError("EOFError", Errors.EOFError);
        var c = k.IntegerUtilities.u8;
        this._littleEndian ? (c[0] = e[f + 0], c[1] = e[f + 1], c[2] = e[f + 2], c[3] = e[f + 3], c[4] = e[f + 4], c[5] = e[f + 5], c[6] = e[f + 6], c[7] = e[f + 7]) : (c[0] = e[f + 7], c[1] = e[f + 6], c[2] = e[f + 5], c[3] = e[f + 4], c[4] = e[f + 3], c[5] = e[f + 2], c[6] = e[f + 1], c[7] = e[f + 0]);
        this._position = f + 8;
        return k.IntegerUtilities.f64[0];
      };
      m.prototype.writeBoolean = function(e) {
        this.writeByte(e ? 1 : 0);
      };
      m.prototype.writeByte = function(e) {
        var f = this._position + 1;
        this._ensureCapacity(f);
        this._u8[this._position++] = e;
        f > this._length && (this._length = f);
      };
      m.prototype.writeUnsignedByte = function(e) {
        var f = this._position + 1;
        this._ensureCapacity(f);
        this._u8[this._position++] = e;
        f > this._length && (this._length = f);
      };
      m.prototype.writeRawBytes = function(e) {
        var f = this._position + e.length;
        this._ensureCapacity(f);
        this._u8.set(e, this._position);
        this._position = f;
        f > this._length && (this._length = f);
      };
      m.prototype.writeBytes = function(e, f, c) {
        void 0 === f && (f = 0);
        void 0 === c && (c = 0);
        k.isNullOrUndefined(e) && throwError("TypeError", Errors.NullPointerError, "bytes");
        2 > arguments.length && (f = 0);
        3 > arguments.length && (c = 0);
        g(f, e.length);
        g(f + c, e.length);
        0 === c && (c = e.length - f);
        this.writeRawBytes(new Int8Array(e._buffer, f, c));
      };
      m.prototype.writeShort = function(e) {
        this.writeUnsignedShort(e);
      };
      m.prototype.writeUnsignedShort = function(e) {
        var f = this._position;
        this._ensureCapacity(f + 2);
        var c = this._u8;
        this._littleEndian ? (c[f + 0] = e, c[f + 1] = e >> 8) : (c[f + 0] = e >> 8, c[f + 1] = e);
        this._position = f += 2;
        f > this._length && (this._length = f);
      };
      m.prototype.writeInt = function(e) {
        this.writeUnsignedInt(e);
      };
      m.prototype.write2Ints = function(e, f) {
        this.write2UnsignedInts(e, f);
      };
      m.prototype.write4Ints = function(e, f, c, d) {
        this.write4UnsignedInts(e, f, c, d);
      };
      m.prototype.writeUnsignedInt = function(e) {
        var f = this._position;
        this._ensureCapacity(f + 4);
        this._requestViews(2);
        if (this._littleEndian === m._nativeLittleEndian && 0 === (f & 3) && this._i32) {
          this._i32[f >> 2] = e;
        } else {
          var c = this._u8;
          this._littleEndian ? (c[f + 0] = e, c[f + 1] = e >> 8, c[f + 2] = e >> 16, c[f + 3] = e >> 24) : (c[f + 0] = e >> 24, c[f + 1] = e >> 16, c[f + 2] = e >> 8, c[f + 3] = e);
        }
        this._position = f += 4;
        f > this._length && (this._length = f);
      };
      m.prototype.write2UnsignedInts = function(e, f) {
        var c = this._position;
        this._ensureCapacity(c + 8);
        this._requestViews(2);
        this._littleEndian === m._nativeLittleEndian && 0 === (c & 3) && this._i32 ? (this._i32[(c >> 2) + 0] = e, this._i32[(c >> 2) + 1] = f, this._position = c += 8, c > this._length && (this._length = c)) : (this.writeUnsignedInt(e), this.writeUnsignedInt(f));
      };
      m.prototype.write4UnsignedInts = function(e, f, c, d) {
        var a = this._position;
        this._ensureCapacity(a + 16);
        this._requestViews(2);
        this._littleEndian === m._nativeLittleEndian && 0 === (a & 3) && this._i32 ? (this._i32[(a >> 2) + 0] = e, this._i32[(a >> 2) + 1] = f, this._i32[(a >> 2) + 2] = c, this._i32[(a >> 2) + 3] = d, this._position = a += 16, a > this._length && (this._length = a)) : (this.writeUnsignedInt(e), this.writeUnsignedInt(f), this.writeUnsignedInt(c), this.writeUnsignedInt(d));
      };
      m.prototype.writeFloat = function(e) {
        var f = this._position;
        this._ensureCapacity(f + 4);
        this._requestViews(4);
        if (this._littleEndian === m._nativeLittleEndian && 0 === (f & 3) && this._f32) {
          this._f32[f >> 2] = e;
        } else {
          var c = this._u8;
          k.IntegerUtilities.f32[0] = e;
          e = k.IntegerUtilities.u8;
          this._littleEndian ? (c[f + 0] = e[0], c[f + 1] = e[1], c[f + 2] = e[2], c[f + 3] = e[3]) : (c[f + 0] = e[3], c[f + 1] = e[2], c[f + 2] = e[1], c[f + 3] = e[0]);
        }
        this._position = f += 4;
        f > this._length && (this._length = f);
      };
      m.prototype.write6Floats = function(e, f, c, d, a, h) {
        var p = this._position;
        this._ensureCapacity(p + 24);
        this._requestViews(4);
        this._littleEndian === m._nativeLittleEndian && 0 === (p & 3) && this._f32 ? (this._f32[(p >> 2) + 0] = e, this._f32[(p >> 2) + 1] = f, this._f32[(p >> 2) + 2] = c, this._f32[(p >> 2) + 3] = d, this._f32[(p >> 2) + 4] = a, this._f32[(p >> 2) + 5] = h, this._position = p += 24, p > this._length && (this._length = p)) : (this.writeFloat(e), this.writeFloat(f), this.writeFloat(c), this.writeFloat(d), this.writeFloat(a), this.writeFloat(h));
      };
      m.prototype.writeDouble = function(e) {
        var f = this._position;
        this._ensureCapacity(f + 8);
        var c = this._u8;
        k.IntegerUtilities.f64[0] = e;
        e = k.IntegerUtilities.u8;
        this._littleEndian ? (c[f + 0] = e[0], c[f + 1] = e[1], c[f + 2] = e[2], c[f + 3] = e[3], c[f + 4] = e[4], c[f + 5] = e[5], c[f + 6] = e[6], c[f + 7] = e[7]) : (c[f + 0] = e[7], c[f + 1] = e[6], c[f + 2] = e[5], c[f + 3] = e[4], c[f + 4] = e[3], c[f + 5] = e[2], c[f + 6] = e[1], c[f + 7] = e[0]);
        this._position = f += 8;
        f > this._length && (this._length = f);
      };
      m.prototype.readRawBytes = function() {
        return new Int8Array(this._buffer, 0, this._length);
      };
      m.prototype.writeUTF = function(e) {
        e = b(e);
        e = n(e);
        this.writeShort(e.length);
        this.writeRawBytes(e);
      };
      m.prototype.writeUTFBytes = function(e) {
        e = b(e);
        e = n(e);
        this.writeRawBytes(e);
      };
      m.prototype.readUTF = function() {
        return this.readUTFBytes(this.readShort());
      };
      m.prototype.readUTFBytes = function(e) {
        e >>>= 0;
        var f = this._position;
        f + e > this._length && throwError("EOFError", Errors.EOFError);
        this._position += e;
        return a(new Int8Array(this._buffer, f, e));
      };
      Object.defineProperty(m.prototype, "length", {get:function() {
        return this._length;
      }, set:function(e) {
        e >>>= 0;
        e > this._buffer.byteLength && this._ensureCapacity(e);
        this._length = e;
        this._position = h(this._position, 0, this._length);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "bytesAvailable", {get:function() {
        return this._length - this._position;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "position", {get:function() {
        return this._position;
      }, set:function(e) {
        this._position = e >>> 0;
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
      }, set:function(e) {
        this._objectEncoding = e >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "endian", {get:function() {
        return this._littleEndian ? "littleEndian" : "bigEndian";
      }, set:function(e) {
        e = b(e);
        this._littleEndian = "auto" === e ? m._nativeLittleEndian : "littleEndian" === e;
      }, enumerable:!0, configurable:!0});
      m.prototype.toString = function() {
        return a(new Int8Array(this._buffer, 0, this._length));
      };
      m.prototype.toBlob = function(e) {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)], {type:e});
      };
      m.prototype.writeMultiByte = function(e, f) {
        v("packageInternal flash.utils.ObjectOutput::writeMultiByte");
      };
      m.prototype.readMultiByte = function(e, f) {
        v("packageInternal flash.utils.ObjectInput::readMultiByte");
      };
      m.prototype.getValue = function(e) {
        e |= 0;
        return e >= this._length ? void 0 : this._u8[e];
      };
      m.prototype.setValue = function(e, f) {
        e |= 0;
        var c = e + 1;
        this._ensureCapacity(c);
        this._u8[e] = f;
        c > this._length && (this._length = c);
      };
      m.prototype.readFixed = function() {
        return this.readInt() / 65536;
      };
      m.prototype.readFixed8 = function() {
        return this.readShort() / 256;
      };
      m.prototype.readFloat16 = function() {
        var e = this.readUnsignedShort(), f = e >> 15 ? -1 : 1, c = (e & 31744) >> 10, e = e & 1023;
        return c ? 31 === c ? e ? NaN : Infinity * f : f * Math.pow(2, c - 15) * (1 + e / 1024) : e / 1024 * Math.pow(2, -14) * f;
      };
      m.prototype.readEncodedU32 = function() {
        var e = this.readUnsignedByte();
        if (!(e & 128)) {
          return e;
        }
        e = e & 127 | this.readUnsignedByte() << 7;
        if (!(e & 16384)) {
          return e;
        }
        e = e & 16383 | this.readUnsignedByte() << 14;
        if (!(e & 2097152)) {
          return e;
        }
        e = e & 2097151 | this.readUnsignedByte() << 21;
        return e & 268435456 ? e & 268435455 | this.readUnsignedByte() << 28 : e;
      };
      m.prototype.readBits = function(e) {
        return this.readUnsignedBits(e) << 32 - e >> 32 - e;
      };
      m.prototype.readUnsignedBits = function(e) {
        for (var f = this._bitBuffer, c = this._bitLength;e > c;) {
          f = f << 8 | this.readUnsignedByte(), c += 8;
        }
        c -= e;
        e = f >>> c & l[e];
        this._bitBuffer = f;
        this._bitLength = c;
        return e;
      };
      m.prototype.readFixedBits = function(e) {
        return this.readBits(e) / 65536;
      };
      m.prototype.readString = function(e) {
        var f = this._position;
        if (e) {
          f + e > this._length && throwError("EOFError", Errors.EOFError), this._position += e;
        } else {
          e = 0;
          for (var c = f;c < this._length && this._u8[c];c++) {
            e++;
          }
          this._position += e + 1;
        }
        return a(new Int8Array(this._buffer, f, e));
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
      m.prototype.compress = function(e) {
        e = 0 === arguments.length ? "zlib" : b(e);
        var f;
        switch(e) {
          case "zlib":
            f = new r.Deflate(!0);
            break;
          case "deflate":
            f = new r.Deflate(!1);
            break;
          default:
            return;
        }
        var c = new m;
        f.onData = c.writeRawBytes.bind(c);
        f.push(this._u8.subarray(0, this._length));
        f.close();
        this._ensureCapacity(c._u8.length);
        this._u8.set(c._u8);
        this.length = c.length;
        this._position = 0;
      };
      m.prototype.uncompress = function(e) {
        e = 0 === arguments.length ? "zlib" : b(e);
        var f;
        switch(e) {
          case "zlib":
            f = r.Inflate.create(!0);
            break;
          case "deflate":
            f = r.Inflate.create(!1);
            break;
          case "lzma":
            f = new r.LzmaDecoder(!1);
            break;
          default:
            return;
        }
        var c = new m, d;
        f.onData = c.writeRawBytes.bind(c);
        f.onError = function(c) {
          return d = c;
        };
        f.push(this._u8.subarray(0, this._length));
        d && throwError("IOError", Errors.CompressedDataError);
        f.close();
        this._ensureCapacity(c._u8.length);
        this._u8.set(c._u8);
        this.length = c.length;
        this._position = 0;
      };
      m._nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
      m.INITIAL_SIZE = 128;
      m._arrayBufferPool = new k.ArrayBufferPool;
      return m;
    }();
    r.DataBuffer = w;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var r = k.ArrayUtilities.DataBuffer, g = k.ArrayUtilities.ensureTypedArrayCapacity;
  (function(b) {
    b[b.BeginSolidFill = 1] = "BeginSolidFill";
    b[b.BeginGradientFill = 2] = "BeginGradientFill";
    b[b.BeginBitmapFill = 3] = "BeginBitmapFill";
    b[b.EndFill = 4] = "EndFill";
    b[b.LineStyleSolid = 5] = "LineStyleSolid";
    b[b.LineStyleGradient = 6] = "LineStyleGradient";
    b[b.LineStyleBitmap = 7] = "LineStyleBitmap";
    b[b.LineEnd = 8] = "LineEnd";
    b[b.MoveTo = 9] = "MoveTo";
    b[b.LineTo = 10] = "LineTo";
    b[b.CurveTo = 11] = "CurveTo";
    b[b.CubicCurveTo = 12] = "CubicCurveTo";
  })(k.PathCommand || (k.PathCommand = {}));
  (function(b) {
    b[b.Linear = 16] = "Linear";
    b[b.Radial = 18] = "Radial";
  })(k.GradientType || (k.GradientType = {}));
  (function(b) {
    b[b.Pad = 0] = "Pad";
    b[b.Reflect = 1] = "Reflect";
    b[b.Repeat = 2] = "Repeat";
  })(k.GradientSpreadMethod || (k.GradientSpreadMethod = {}));
  (function(b) {
    b[b.RGB = 0] = "RGB";
    b[b.LinearRGB = 1] = "LinearRGB";
  })(k.GradientInterpolationMethod || (k.GradientInterpolationMethod = {}));
  (function(b) {
    b[b.None = 0] = "None";
    b[b.Normal = 1] = "Normal";
    b[b.Vertical = 2] = "Vertical";
    b[b.Horizontal = 3] = "Horizontal";
  })(k.LineScaleMode || (k.LineScaleMode = {}));
  var b = function() {
    return function(b, a, h, p, l, w, m, t, g, e, f) {
      this.commands = b;
      this.commandsPosition = a;
      this.coordinates = h;
      this.morphCoordinates = p;
      this.coordinatesPosition = l;
      this.styles = w;
      this.stylesLength = m;
      this.morphStyles = t;
      this.morphStylesLength = g;
      this.hasFills = e;
      this.hasLines = f;
    };
  }();
  k.PlainObjectShapeData = b;
  var v;
  (function(b) {
    b[b.Commands = 32] = "Commands";
    b[b.Coordinates = 128] = "Coordinates";
    b[b.Styles = 16] = "Styles";
  })(v || (v = {}));
  v = function() {
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
    n.prototype.curveTo = function(a, h, b, l) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = l;
    };
    n.prototype.cubicCurveTo = function(a, h, b, l, n, m) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = l;
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
    n.prototype.lineStyle = function(a, h, b, l, n, m, t) {
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5;
      this.coordinates[this.coordinatesPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(h);
      a.writeBoolean(b);
      a.writeUnsignedByte(l);
      a.writeUnsignedByte(n);
      a.writeUnsignedByte(m);
      a.writeUnsignedByte(t);
      this.hasLines = !0;
    };
    n.prototype.writeMorphLineStyle = function(a, h) {
      this.morphCoordinates[this.coordinatesPosition - 1] = a;
      this.morphStyles.writeUnsignedInt(h);
    };
    n.prototype.beginBitmap = function(a, h, b, l, n) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(h);
      this._writeStyleMatrix(b, !1);
      a.writeBoolean(l);
      a.writeBoolean(n);
      this.hasFills = !0;
    };
    n.prototype.writeMorphBitmap = function(a) {
      this._writeStyleMatrix(a, !0);
    };
    n.prototype.beginGradient = function(a, h, b, l, n, m, t, g) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedByte(l);
      a.writeShort(g);
      this._writeStyleMatrix(n, !1);
      l = h.length;
      a.writeByte(l);
      for (n = 0;n < l;n++) {
        a.writeUnsignedByte(b[n]), a.writeUnsignedInt(h[n]);
      }
      a.writeUnsignedByte(m);
      a.writeUnsignedByte(t);
      this.hasFills = !0;
    };
    n.prototype.writeMorphGradient = function(a, h, b) {
      this._writeStyleMatrix(b, !0);
      b = this.morphStyles;
      for (var l = 0;l < a.length;l++) {
        b.writeUnsignedByte(h[l]), b.writeUnsignedInt(a[l]);
      }
    };
    n.prototype.writeCommandAndCoordinates = function(a, h, b) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = b;
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
      return new b(this.commands, this.commandsPosition, this.coordinates, this.morphCoordinates, this.coordinatesPosition, this.styles.buffer, this.styles.length, this.morphStyles && this.morphStyles.buffer, this.morphStyles ? this.morphStyles.length : 0, this.hasFills, this.hasLines);
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
  k.ShapeData = v;
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      (function(b) {
        b[b.CODE_END = 0] = "CODE_END";
        b[b.CODE_SHOW_FRAME = 1] = "CODE_SHOW_FRAME";
        b[b.CODE_DEFINE_SHAPE = 2] = "CODE_DEFINE_SHAPE";
        b[b.CODE_FREE_CHARACTER = 3] = "CODE_FREE_CHARACTER";
        b[b.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        b[b.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        b[b.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        b[b.CODE_DEFINE_BUTTON = 7] = "CODE_DEFINE_BUTTON";
        b[b.CODE_JPEG_TABLES = 8] = "CODE_JPEG_TABLES";
        b[b.CODE_SET_BACKGROUND_COLOR = 9] = "CODE_SET_BACKGROUND_COLOR";
        b[b.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        b[b.CODE_DEFINE_TEXT = 11] = "CODE_DEFINE_TEXT";
        b[b.CODE_DO_ACTION = 12] = "CODE_DO_ACTION";
        b[b.CODE_DEFINE_FONT_INFO = 13] = "CODE_DEFINE_FONT_INFO";
        b[b.CODE_DEFINE_SOUND = 14] = "CODE_DEFINE_SOUND";
        b[b.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        b[b.CODE_STOP_SOUND = 16] = "CODE_STOP_SOUND";
        b[b.CODE_DEFINE_BUTTON_SOUND = 17] = "CODE_DEFINE_BUTTON_SOUND";
        b[b.CODE_SOUND_STREAM_HEAD = 18] = "CODE_SOUND_STREAM_HEAD";
        b[b.CODE_SOUND_STREAM_BLOCK = 19] = "CODE_SOUND_STREAM_BLOCK";
        b[b.CODE_DEFINE_BITS_LOSSLESS = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        b[b.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        b[b.CODE_DEFINE_SHAPE2 = 22] = "CODE_DEFINE_SHAPE2";
        b[b.CODE_DEFINE_BUTTON_CXFORM = 23] = "CODE_DEFINE_BUTTON_CXFORM";
        b[b.CODE_PROTECT = 24] = "CODE_PROTECT";
        b[b.CODE_PATHS_ARE_POSTSCRIPT = 25] = "CODE_PATHS_ARE_POSTSCRIPT";
        b[b.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        b[b.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        b[b.CODE_SYNC_FRAME = 29] = "CODE_SYNC_FRAME";
        b[b.CODE_FREE_ALL = 31] = "CODE_FREE_ALL";
        b[b.CODE_DEFINE_SHAPE3 = 32] = "CODE_DEFINE_SHAPE3";
        b[b.CODE_DEFINE_TEXT2 = 33] = "CODE_DEFINE_TEXT2";
        b[b.CODE_DEFINE_BUTTON2 = 34] = "CODE_DEFINE_BUTTON2";
        b[b.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        b[b.CODE_DEFINE_BITS_LOSSLESS2 = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        b[b.CODE_DEFINE_EDIT_TEXT = 37] = "CODE_DEFINE_EDIT_TEXT";
        b[b.CODE_DEFINE_VIDEO = 38] = "CODE_DEFINE_VIDEO";
        b[b.CODE_DEFINE_SPRITE = 39] = "CODE_DEFINE_SPRITE";
        b[b.CODE_NAME_CHARACTER = 40] = "CODE_NAME_CHARACTER";
        b[b.CODE_PRODUCT_INFO = 41] = "CODE_PRODUCT_INFO";
        b[b.CODE_DEFINE_TEXT_FORMAT = 42] = "CODE_DEFINE_TEXT_FORMAT";
        b[b.CODE_FRAME_LABEL = 43] = "CODE_FRAME_LABEL";
        b[b.CODE_DEFINE_BEHAVIOUR = 44] = "CODE_DEFINE_BEHAVIOUR";
        b[b.CODE_SOUND_STREAM_HEAD2 = 45] = "CODE_SOUND_STREAM_HEAD2";
        b[b.CODE_DEFINE_MORPH_SHAPE = 46] = "CODE_DEFINE_MORPH_SHAPE";
        b[b.CODE_GENERATE_FRAME = 47] = "CODE_GENERATE_FRAME";
        b[b.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        b[b.CODE_GEN_COMMAND = 49] = "CODE_GEN_COMMAND";
        b[b.CODE_DEFINE_COMMAND_OBJECT = 50] = "CODE_DEFINE_COMMAND_OBJECT";
        b[b.CODE_CHARACTER_SET = 51] = "CODE_CHARACTER_SET";
        b[b.CODE_EXTERNAL_FONT = 52] = "CODE_EXTERNAL_FONT";
        b[b.CODE_DEFINE_FUNCTION = 53] = "CODE_DEFINE_FUNCTION";
        b[b.CODE_PLACE_FUNCTION = 54] = "CODE_PLACE_FUNCTION";
        b[b.CODE_GEN_TAG_OBJECTS = 55] = "CODE_GEN_TAG_OBJECTS";
        b[b.CODE_EXPORT_ASSETS = 56] = "CODE_EXPORT_ASSETS";
        b[b.CODE_IMPORT_ASSETS = 57] = "CODE_IMPORT_ASSETS";
        b[b.CODE_ENABLE_DEBUGGER = 58] = "CODE_ENABLE_DEBUGGER";
        b[b.CODE_DO_INIT_ACTION = 59] = "CODE_DO_INIT_ACTION";
        b[b.CODE_DEFINE_VIDEO_STREAM = 60] = "CODE_DEFINE_VIDEO_STREAM";
        b[b.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
        b[b.CODE_DEFINE_FONT_INFO2 = 62] = "CODE_DEFINE_FONT_INFO2";
        b[b.CODE_DEBUG_ID = 63] = "CODE_DEBUG_ID";
        b[b.CODE_ENABLE_DEBUGGER2 = 64] = "CODE_ENABLE_DEBUGGER2";
        b[b.CODE_SCRIPT_LIMITS = 65] = "CODE_SCRIPT_LIMITS";
        b[b.CODE_SET_TAB_INDEX = 66] = "CODE_SET_TAB_INDEX";
        b[b.CODE_FILE_ATTRIBUTES = 69] = "CODE_FILE_ATTRIBUTES";
        b[b.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        b[b.CODE_IMPORT_ASSETS2 = 71] = "CODE_IMPORT_ASSETS2";
        b[b.CODE_DO_ABC_DEFINE = 72] = "CODE_DO_ABC_DEFINE";
        b[b.CODE_DEFINE_FONT_ALIGN_ZONES = 73] = "CODE_DEFINE_FONT_ALIGN_ZONES";
        b[b.CODE_CSM_TEXT_SETTINGS = 74] = "CODE_CSM_TEXT_SETTINGS";
        b[b.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        b[b.CODE_SYMBOL_CLASS = 76] = "CODE_SYMBOL_CLASS";
        b[b.CODE_METADATA = 77] = "CODE_METADATA";
        b[b.CODE_DEFINE_SCALING_GRID = 78] = "CODE_DEFINE_SCALING_GRID";
        b[b.CODE_DO_ABC = 82] = "CODE_DO_ABC";
        b[b.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        b[b.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        b[b.CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA = 86] = "CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA";
        b[b.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        b[b.CODE_DEFINE_FONT_NAME = 88] = "CODE_DEFINE_FONT_NAME";
        b[b.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
        b[b.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        b[b.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(g.SwfTag || (g.SwfTag = {}));
      (function(b) {
        b[b.CODE_DEFINE_SHAPE = 2] = "CODE_DEFINE_SHAPE";
        b[b.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        b[b.CODE_DEFINE_BUTTON = 7] = "CODE_DEFINE_BUTTON";
        b[b.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        b[b.CODE_DEFINE_TEXT = 11] = "CODE_DEFINE_TEXT";
        b[b.CODE_DEFINE_SOUND = 14] = "CODE_DEFINE_SOUND";
        b[b.CODE_DEFINE_BITS_LOSSLESS = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        b[b.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        b[b.CODE_DEFINE_SHAPE2 = 22] = "CODE_DEFINE_SHAPE2";
        b[b.CODE_DEFINE_SHAPE3 = 32] = "CODE_DEFINE_SHAPE3";
        b[b.CODE_DEFINE_TEXT2 = 33] = "CODE_DEFINE_TEXT2";
        b[b.CODE_DEFINE_BUTTON2 = 34] = "CODE_DEFINE_BUTTON2";
        b[b.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        b[b.CODE_DEFINE_BITS_LOSSLESS2 = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        b[b.CODE_DEFINE_EDIT_TEXT = 37] = "CODE_DEFINE_EDIT_TEXT";
        b[b.CODE_DEFINE_SPRITE = 39] = "CODE_DEFINE_SPRITE";
        b[b.CODE_DEFINE_MORPH_SHAPE = 46] = "CODE_DEFINE_MORPH_SHAPE";
        b[b.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        b[b.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        b[b.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        b[b.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        b[b.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        b[b.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        b[b.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(g.DefinitionTags || (g.DefinitionTags = {}));
      (function(b) {
        b[b.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        b[b.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        b[b.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        b[b.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
      })(g.ImageDefinitionTags || (g.ImageDefinitionTags = {}));
      (function(b) {
        b[b.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        b[b.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        b[b.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        b[b.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(g.FontDefinitionTags || (g.FontDefinitionTags = {}));
      (function(b) {
        b[b.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        b[b.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        b[b.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        b[b.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        b[b.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        b[b.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        b[b.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
      })(g.ControlTags || (g.ControlTags = {}));
      (function(b) {
        b[b.Move = 1] = "Move";
        b[b.HasCharacter = 2] = "HasCharacter";
        b[b.HasMatrix = 4] = "HasMatrix";
        b[b.HasColorTransform = 8] = "HasColorTransform";
        b[b.HasRatio = 16] = "HasRatio";
        b[b.HasName = 32] = "HasName";
        b[b.HasClipDepth = 64] = "HasClipDepth";
        b[b.HasClipActions = 128] = "HasClipActions";
        b[b.HasFilterList = 256] = "HasFilterList";
        b[b.HasBlendMode = 512] = "HasBlendMode";
        b[b.HasCacheAsBitmap = 1024] = "HasCacheAsBitmap";
        b[b.HasClassName = 2048] = "HasClassName";
        b[b.HasImage = 4096] = "HasImage";
        b[b.HasVisible = 8192] = "HasVisible";
        b[b.OpaqueBackground = 16384] = "OpaqueBackground";
        b[b.Reserved = 32768] = "Reserved";
      })(g.PlaceObjectFlags || (g.PlaceObjectFlags = {}));
      (function(b) {
        b[b.Load = 1] = "Load";
        b[b.EnterFrame = 2] = "EnterFrame";
        b[b.Unload = 4] = "Unload";
        b[b.MouseMove = 8] = "MouseMove";
        b[b.MouseDown = 16] = "MouseDown";
        b[b.MouseUp = 32] = "MouseUp";
        b[b.KeyDown = 64] = "KeyDown";
        b[b.KeyUp = 128] = "KeyUp";
        b[b.Data = 256] = "Data";
        b[b.Initialize = 512] = "Initialize";
        b[b.Press = 1024] = "Press";
        b[b.Release = 2048] = "Release";
        b[b.ReleaseOutside = 4096] = "ReleaseOutside";
        b[b.RollOver = 8192] = "RollOver";
        b[b.RollOut = 16384] = "RollOut";
        b[b.DragOver = 32768] = "DragOver";
        b[b.DragOut = 65536] = "DragOut";
        b[b.KeyPress = 131072] = "KeyPress";
        b[b.Construct = 262144] = "Construct";
      })(g.AVM1ClipEvents || (g.AVM1ClipEvents = {}));
    })(k.Parser || (k.Parser = {}));
  })(k.SWF || (k.SWF = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var r = k.Debug.unexpected, g = function() {
    function b(b, n, a, h) {
      this.url = b;
      this.method = n;
      this.mimeType = a;
      this.data = h;
    }
    b.prototype.readAll = function(b, n) {
      var a = this.url, h = new XMLHttpRequest({mozSystem:!0});
      h.open(this.method || "GET", this.url, !0);
      h.responseType = "arraybuffer";
      b && (h.onprogress = function(a) {
        b(h.response, a.loaded, a.total);
      });
      h.onreadystatechange = function(b) {
        4 === h.readyState && (200 !== h.status && 0 !== h.status || null === h.response ? (r("Path: " + a + " not found."), n(null, h.statusText)) : n(h.response));
      };
      this.mimeType && h.setRequestHeader("Content-Type", this.mimeType);
      h.send(this.data || null);
    };
    b.prototype.readChunked = function(b, n, a, h, p, l) {
      if (0 >= b) {
        this.readAsync(n, a, h, p, l);
      } else {
        var w = 0, m = new Uint8Array(b), t = 0, g;
        this.readAsync(function(e, f) {
          g = f.total;
          for (var c = e.length, d = 0;w + c >= b;) {
            var a = b - w;
            m.set(e.subarray(d, d + a), w);
            d += a;
            c -= a;
            t += b;
            n(m, {loaded:t, total:g});
            w = 0;
          }
          m.set(e.subarray(d), w);
          w += c;
        }, a, h, function() {
          0 < w && (t += w, n(m.subarray(0, w), {loaded:t, total:g}), w = 0);
          p && p();
        }, l);
      }
    };
    b.prototype.readAsync = function(b, n, a, h, p) {
      var l = new XMLHttpRequest({mozSystem:!0}), w = this.url, m = 0, t = 0;
      l.open(this.method || "GET", w, !0);
      l.responseType = "moz-chunked-arraybuffer";
      var g = "moz-chunked-arraybuffer" !== l.responseType;
      g && (l.responseType = "arraybuffer");
      l.onprogress = function(e) {
        g || (m = e.loaded, t = e.total, b(new Uint8Array(l.response), {loaded:m, total:t}));
      };
      l.onreadystatechange = function(e) {
        2 === l.readyState && p && p(w, l.status, l.getAllResponseHeaders());
        4 === l.readyState && (200 !== l.status && 0 !== l.status || null === l.response && (0 === t || m !== t) ? n(l.statusText) : (g && (e = l.response, b(new Uint8Array(e), {loaded:0, total:e.byteLength})), h && h()));
      };
      this.mimeType && l.setRequestHeader("Content-Type", this.mimeType);
      l.send(this.data || null);
      a && a();
    };
    return b;
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
      var b = function() {
        function b() {
        }
        b.toRGBA = function(a, h, b, l) {
          void 0 === l && (l = 1);
          return "rgba(" + a + "," + h + "," + b + "," + l + ")";
        };
        return b;
      }();
      g.UI = b;
      var k = function() {
        function n() {
        }
        n.prototype.tabToolbar = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(37, 44, 51, a);
        };
        n.prototype.toolbars = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(52, 60, 69, a);
        };
        n.prototype.selectionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(29, 79, 115, a);
        };
        n.prototype.selectionText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(245, 247, 250, a);
        };
        n.prototype.splitters = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(0, 0, 0, a);
        };
        n.prototype.bodyBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(17, 19, 21, a);
        };
        n.prototype.sidebarBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(24, 29, 32, a);
        };
        n.prototype.attentionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(161, 134, 80, a);
        };
        n.prototype.bodyText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(143, 161, 178, a);
        };
        n.prototype.foregroundTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(182, 186, 191, a);
        };
        n.prototype.contentTextHighContrast = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(169, 186, 203, a);
        };
        n.prototype.contentTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(143, 161, 178, a);
        };
        n.prototype.contentTextDarkGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(95, 115, 135, a);
        };
        n.prototype.blueHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(70, 175, 227, a);
        };
        n.prototype.purpleHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(107, 122, 187, a);
        };
        n.prototype.pinkHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(223, 128, 255, a);
        };
        n.prototype.redHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(235, 83, 104, a);
        };
        n.prototype.orangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(217, 102, 41, a);
        };
        n.prototype.lightOrangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(217, 155, 40, a);
        };
        n.prototype.greenHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(112, 191, 83, a);
        };
        n.prototype.blueGreyHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(94, 136, 176, a);
        };
        return n;
      }();
      g.UIThemeDark = k;
      k = function() {
        function n() {
        }
        n.prototype.tabToolbar = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(235, 236, 237, a);
        };
        n.prototype.toolbars = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(240, 241, 242, a);
        };
        n.prototype.selectionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(76, 158, 217, a);
        };
        n.prototype.selectionText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(245, 247, 250, a);
        };
        n.prototype.splitters = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(170, 170, 170, a);
        };
        n.prototype.bodyBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(252, 252, 252, a);
        };
        n.prototype.sidebarBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(247, 247, 247, a);
        };
        n.prototype.attentionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(161, 134, 80, a);
        };
        n.prototype.bodyText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(24, 25, 26, a);
        };
        n.prototype.foregroundTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(88, 89, 89, a);
        };
        n.prototype.contentTextHighContrast = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(41, 46, 51, a);
        };
        n.prototype.contentTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(143, 161, 178, a);
        };
        n.prototype.contentTextDarkGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(102, 115, 128, a);
        };
        n.prototype.blueHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(0, 136, 204, a);
        };
        n.prototype.purpleHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(91, 95, 255, a);
        };
        n.prototype.pinkHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(184, 46, 229, a);
        };
        n.prototype.redHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(237, 38, 85, a);
        };
        n.prototype.orangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(241, 60, 0, a);
        };
        n.prototype.lightOrangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(217, 126, 0, a);
        };
        n.prototype.greenHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(44, 187, 15, a);
        };
        n.prototype.blueGreyHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(95, 136, 176, a);
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
      var b = function() {
        function b(n, a) {
          this._buffers = n || [];
          this._snapshots = [];
          this._windowStart = this._startTime = a;
          this._maxDepth = 0;
        }
        b.prototype.addBuffer = function(b) {
          this._buffers.push(b);
        };
        b.prototype.getSnapshotAt = function(b) {
          return this._snapshots[b];
        };
        Object.defineProperty(b.prototype, "hasSnapshots", {get:function() {
          return 0 < this.snapshotCount;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "snapshotCount", {get:function() {
          return this._snapshots.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "startTime", {get:function() {
          return this._startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "endTime", {get:function() {
          return this._endTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "windowStart", {get:function() {
          return this._windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "windowEnd", {get:function() {
          return this._windowEnd;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "windowLength", {get:function() {
          return this.windowEnd - this.windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "maxDepth", {get:function() {
          return this._maxDepth;
        }, enumerable:!0, configurable:!0});
        b.prototype.forEachSnapshot = function(b) {
          for (var a = 0, h = this.snapshotCount;a < h;a++) {
            b(this._snapshots[a], a);
          }
        };
        b.prototype.createSnapshots = function() {
          var b = Number.MIN_VALUE, a = 0;
          for (this._snapshots = [];0 < this._buffers.length;) {
            var h = this._buffers.shift().createSnapshot();
            h && (b < h.endTime && (b = h.endTime), a < h.maxDepth && (a = h.maxDepth), this._snapshots.push(h));
          }
          this._windowEnd = this._endTime = b;
          this._maxDepth = a;
        };
        b.prototype.setWindow = function(b, a) {
          if (b > a) {
            var h = b;
            b = a;
            a = h;
          }
          h = Math.min(a - b, this.totalTime);
          b < this._startTime ? (b = this._startTime, a = this._startTime + h) : a > this._endTime && (b = this._endTime - h, a = this._endTime);
          this._windowStart = b;
          this._windowEnd = a;
        };
        b.prototype.moveWindowTo = function(b) {
          this.setWindow(b - this.windowLength / 2, b + this.windowLength / 2);
        };
        return b;
      }();
      g.Profile = b;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, r) {
  function g() {
    this.constructor = k;
  }
  for (var b in r) {
    r.hasOwnProperty(b) && (k[b] = r[b]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(k) {
    (function(g) {
      var b = function() {
        return function(b) {
          this.kind = b;
          this.totalTime = this.selfTime = this.count = 0;
        };
      }();
      g.TimelineFrameStatistics = b;
      var k = function() {
        function n(a, h, b, l, n, m) {
          this.parent = a;
          this.kind = h;
          this.startData = b;
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
            for (var h = 0, b = this.children.length;h < b;h++) {
              var l = this.children[h], a = a - (l.endTime - l.startTime)
            }
          }
          return a;
        }, enumerable:!0, configurable:!0});
        n.prototype.getChildIndex = function(a) {
          for (var h = this.children, b = 0;b < h.length;b++) {
            if (h[b].endTime > a) {
              return b;
            }
          }
          return 0;
        };
        n.prototype.getChildRange = function(a, h) {
          if (this.children && a <= this.endTime && h >= this.startTime && h >= a) {
            var b = this._getNearestChild(a), l = this._getNearestChildReverse(h);
            if (b <= l) {
              return a = this.children[b].startTime, h = this.children[l].endTime, {startIndex:b, endIndex:l, startTime:a, endTime:h, totalTime:h - a};
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
            for (var b, l = 0, n = h.length - 1;n > l;) {
              b = (l + n) / 2 | 0;
              var m = h[b];
              if (a >= m.startTime && a <= m.endTime) {
                return b;
              }
              a > m.endTime ? l = b + 1 : n = b;
            }
            return Math.ceil((l + n) / 2);
          }
          return 0;
        };
        n.prototype._getNearestChildReverse = function(a) {
          var h = this.children;
          if (h && h.length) {
            var b = h.length - 1;
            if (a >= h[b].startTime) {
              return b;
            }
            for (var l, n = 0;b > n;) {
              l = Math.ceil((n + b) / 2);
              var m = h[l];
              if (a >= m.startTime && a <= m.endTime) {
                return l;
              }
              a > m.endTime ? n = l : b = l - 1;
            }
            return (n + b) / 2 | 0;
          }
          return 0;
        };
        n.prototype.query = function(a) {
          if (a < this.startTime || a > this.endTime) {
            return null;
          }
          var h = this.children;
          if (h && 0 < h.length) {
            for (var b, l = 0, n = h.length - 1;n > l;) {
              var m = (l + n) / 2 | 0;
              b = h[m];
              if (a >= b.startTime && a <= b.endTime) {
                return b.query(a);
              }
              a > b.endTime ? l = m + 1 : n = m;
            }
            b = h[n];
            if (a >= b.startTime && a <= b.endTime) {
              return b.query(a);
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
              var l = h[p.kind.id] || (h[p.kind.id] = new b(p.kind));
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
      k = function(b) {
        function a(a) {
          b.call(this, null, null, null, null, NaN, NaN);
          this.name = a;
        }
        __extends(a, b);
        return a;
      }(k);
      g.TimelineBufferSnapshot = k;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = function() {
        function b(n, a) {
          void 0 === n && (n = "");
          this.name = n || "";
          this._startTime = k.isNullOrUndefined(a) ? jsGlobal.START_TIME : a;
        }
        b.prototype.getKind = function(b) {
          return this._kinds[b];
        };
        Object.defineProperty(b.prototype, "kinds", {get:function() {
          return this._kinds.concat();
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "depth", {get:function() {
          return this._depth;
        }, enumerable:!0, configurable:!0});
        b.prototype._initialize = function() {
          this._depth = 0;
          this._stack = [];
          this._data = [];
          this._kinds = [];
          this._kindNameMap = Object.create(null);
          this._marks = new k.CircularBuffer(Int32Array, 20);
          this._times = new k.CircularBuffer(Float64Array, 20);
        };
        b.prototype._getKindId = function(n) {
          var a = b.MAX_KINDID;
          if (void 0 === this._kindNameMap[n]) {
            if (a = this._kinds.length, a < b.MAX_KINDID) {
              var h = {id:a, name:n, visible:!0};
              this._kinds.push(h);
              this._kindNameMap[n] = h;
            } else {
              a = b.MAX_KINDID;
            }
          } else {
            a = this._kindNameMap[n].id;
          }
          return a;
        };
        b.prototype._getMark = function(n, a, h) {
          var p = b.MAX_DATAID;
          k.isNullOrUndefined(h) || a === b.MAX_KINDID || (p = this._data.length, p < b.MAX_DATAID ? this._data.push(h) : p = b.MAX_DATAID);
          return n | p << 16 | a;
        };
        b.prototype.enter = function(n, a, h) {
          h = (k.isNullOrUndefined(h) ? performance.now() : h) - this._startTime;
          this._marks || this._initialize();
          this._depth++;
          n = this._getKindId(n);
          this._marks.write(this._getMark(b.ENTER, n, a));
          this._times.write(h);
          this._stack.push(n);
        };
        b.prototype.leave = function(n, a, h) {
          h = (k.isNullOrUndefined(h) ? performance.now() : h) - this._startTime;
          var p = this._stack.pop();
          n && (p = this._getKindId(n));
          this._marks.write(this._getMark(b.LEAVE, p, a));
          this._times.write(h);
          this._depth--;
        };
        b.prototype.count = function(b, a, h) {
        };
        b.prototype.createSnapshot = function() {
          var n;
          void 0 === n && (n = Number.MAX_VALUE);
          if (!this._marks) {
            return null;
          }
          var a = this._times, h = this._kinds, p = this._data, l = new g.TimelineBufferSnapshot(this.name), w = [l], m = 0;
          this._marks || this._initialize();
          this._marks.forEachInReverse(function(l, x) {
            var e = p[l >>> 16 & b.MAX_DATAID], f = h[l & b.MAX_KINDID];
            if (k.isNullOrUndefined(f) || f.visible) {
              var c = l & 2147483648, d = a.get(x), q = w.length;
              if (c === b.LEAVE) {
                if (1 === q && (m++, m > n)) {
                  return !0;
                }
                w.push(new g.TimelineFrame(w[q - 1], f, null, e, NaN, d));
              } else {
                if (c === b.ENTER) {
                  if (f = w.pop(), c = w[w.length - 1]) {
                    for (c.children ? c.children.unshift(f) : c.children = [f], c = w.length, f.depth = c, f.startData = e, f.startTime = d;f;) {
                      if (f.maxDepth < c) {
                        f.maxDepth = c, f = f.parent;
                      } else {
                        break;
                      }
                    }
                  } else {
                    return !0;
                  }
                }
              }
            }
          });
          l.children && l.children.length && (l.startTime = l.children[0].startTime, l.endTime = l.children[l.children.length - 1].endTime);
          return l;
        };
        b.prototype.reset = function(b) {
          this._startTime = k.isNullOrUndefined(b) ? performance.now() : b;
          this._marks ? (this._depth = 0, this._data = [], this._marks.reset(), this._times.reset()) : this._initialize();
        };
        b.FromFirefoxProfile = function(n, a) {
          for (var h = n.profile.threads[0].samples, p = new b(a, h[0].time), l = [], g, m = 0;m < h.length;m++) {
            g = h[m];
            var t = g.time, k = g.frames, e = 0;
            for (g = Math.min(k.length, l.length);e < g && k[e].location === l[e].location;) {
              e++;
            }
            for (var f = l.length - e, c = 0;c < f;c++) {
              g = l.pop(), p.leave(g.location, null, t);
            }
            for (;e < k.length;) {
              g = k[e++], p.enter(g.location, null, t);
            }
            l = k;
          }
          for (;g = l.pop();) {
            p.leave(g.location, null, t);
          }
          return p;
        };
        b.FromChromeProfile = function(n, a) {
          var h = n.timestamps, p = n.samples, l = new b(a, h[0] / 1E3), g = [], m = {}, t;
          b._resolveIds(n.head, m);
          for (var k = 0;k < h.length;k++) {
            var e = h[k] / 1E3, f = [];
            for (t = m[p[k]];t;) {
              f.unshift(t), t = t.parent;
            }
            var c = 0;
            for (t = Math.min(f.length, g.length);c < t && f[c] === g[c];) {
              c++;
            }
            for (var d = g.length - c, q = 0;q < d;q++) {
              t = g.pop(), l.leave(t.functionName, null, e);
            }
            for (;c < f.length;) {
              t = f[c++], l.enter(t.functionName, null, e);
            }
            g = f;
          }
          for (;t = g.pop();) {
            l.leave(t.functionName, null, e);
          }
          return l;
        };
        b._resolveIds = function(n, a) {
          a[n.id] = n;
          if (n.children) {
            for (var h = 0;h < n.children.length;h++) {
              n.children[h].parent = n, b._resolveIds(n.children[h], a);
            }
          }
        };
        b.ENTER = 0;
        b.LEAVE = -2147483648;
        b.MAX_KINDID = 65535;
        b.MAX_DATAID = 32767;
        return b;
      }();
      g.TimelineBuffer = b;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      (function(b) {
        b[b.DARK = 0] = "DARK";
        b[b.LIGHT = 1] = "LIGHT";
      })(g.UIThemeType || (g.UIThemeType = {}));
      var b = function() {
        function b(n, a) {
          void 0 === a && (a = 0);
          this._container = n;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = a;
          this._tooltip = this._createTooltip();
        }
        b.prototype.createProfile = function(b, a) {
          var h = new g.Profile(b, a);
          h.createSnapshots();
          this._profiles.push(h);
          this.activateProfile(h);
          return h;
        };
        b.prototype.activateProfile = function(b) {
          this.deactivateProfile();
          this._activeProfile = b;
          this._createViews();
          this._initializeViews();
        };
        b.prototype.activateProfileAt = function(b) {
          this.activateProfile(this.getProfileAt(b));
        };
        b.prototype.deactivateProfile = function() {
          this._activeProfile && (this._destroyViews(), this._activeProfile = null);
        };
        b.prototype.resize = function() {
          this._onResize();
        };
        b.prototype.getProfileAt = function(b) {
          return this._profiles[b];
        };
        Object.defineProperty(b.prototype, "activeProfile", {get:function() {
          return this._activeProfile;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "profileCount", {get:function() {
          return this._profiles.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "container", {get:function() {
          return this._container;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "themeType", {get:function() {
          return this._themeType;
        }, set:function(b) {
          switch(b) {
            case 0:
              this._theme = new r.Theme.UIThemeDark;
              break;
            case 1:
              this._theme = new r.Theme.UIThemeLight;
          }
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "theme", {get:function() {
          return this._theme;
        }, enumerable:!0, configurable:!0});
        b.prototype.getSnapshotAt = function(b) {
          return this._activeProfile.getSnapshotAt(b);
        };
        b.prototype._createViews = function() {
          if (this._activeProfile) {
            var b = this;
            this._overviewHeader = new g.FlameChartHeader(this, 0);
            this._overview = new g.FlameChartOverview(this, 0);
            this._activeProfile.forEachSnapshot(function(a, h) {
              b._headers.push(new g.FlameChartHeader(b, 1));
              b._charts.push(new g.FlameChart(b, a));
            });
            window.addEventListener("resize", this._onResize.bind(this));
          }
        };
        b.prototype._destroyViews = function() {
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
        b.prototype._initializeViews = function() {
          if (this._activeProfile) {
            var b = this, a = this._activeProfile.startTime, h = this._activeProfile.endTime;
            this._overviewHeader.initialize(a, h);
            this._overview.initialize(a, h);
            this._activeProfile.forEachSnapshot(function(p, l) {
              b._headers[l].initialize(a, h);
              b._charts[l].initialize(a, h);
            });
          }
        };
        b.prototype._onResize = function() {
          if (this._activeProfile) {
            var b = this, a = this._container.offsetWidth;
            this._overviewHeader.setSize(a);
            this._overview.setSize(a);
            this._activeProfile.forEachSnapshot(function(h, p) {
              b._headers[p].setSize(a);
              b._charts[p].setSize(a);
            });
          }
        };
        b.prototype._updateViews = function() {
          if (this._activeProfile) {
            var b = this, a = this._activeProfile.windowStart, h = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(a, h);
            this._overview.setWindow(a, h);
            this._activeProfile.forEachSnapshot(function(p, l) {
              b._headers[l].setWindow(a, h);
              b._charts[l].setWindow(a, h);
            });
          }
        };
        b.prototype._drawViews = function() {
        };
        b.prototype._createTooltip = function() {
          var b = document.createElement("div");
          b.classList.add("profiler-tooltip");
          b.style.display = "none";
          this._container.insertBefore(b, this._container.firstChild);
          return b;
        };
        b.prototype.setWindow = function(b, a) {
          this._activeProfile.setWindow(b, a);
          this._updateViews();
        };
        b.prototype.moveWindowTo = function(b) {
          this._activeProfile.moveWindowTo(b);
          this._updateViews();
        };
        b.prototype.showTooltip = function(b, a, h, p) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(b, a));
          this._tooltip.style.display = "block";
          var l = this._tooltip.firstChild;
          a = l.clientWidth;
          l = l.clientHeight;
          h += h + a >= b.canvas.clientWidth - 50 ? -(a + 20) : 25;
          p += b.canvas.offsetTop - l / 2;
          this._tooltip.style.left = h + "px";
          this._tooltip.style.top = p + "px";
        };
        b.prototype.hideTooltip = function() {
          this._tooltip.style.display = "none";
        };
        b.prototype.createTooltipContent = function(b, a) {
          var h = Math.round(1E5 * a.totalTime) / 1E5, p = Math.round(1E5 * a.selfTime) / 1E5, l = Math.round(1E4 * a.selfTime / a.totalTime) / 100, g = document.createElement("div"), m = document.createElement("h1");
          m.textContent = a.kind.name;
          g.appendChild(m);
          m = document.createElement("p");
          m.textContent = "Total: " + h + " ms";
          g.appendChild(m);
          h = document.createElement("p");
          h.textContent = "Self: " + p + " ms (" + l + "%)";
          g.appendChild(h);
          if (p = b.getStatistics(a.kind)) {
            l = document.createElement("p"), l.textContent = "Count: " + p.count, g.appendChild(l), l = document.createElement("p"), l.textContent = "All Total: " + Math.round(1E5 * p.totalTime) / 1E5 + " ms", g.appendChild(l), l = document.createElement("p"), l.textContent = "All Self: " + Math.round(1E5 * p.selfTime) / 1E5 + " ms", g.appendChild(l);
          }
          this.appendDataElements(g, a.startData);
          this.appendDataElements(g, a.endData);
          return g;
        };
        b.prototype.appendDataElements = function(b, a) {
          if (!k.isNullOrUndefined(a)) {
            b.appendChild(document.createElement("hr"));
            var h;
            if (k.isObject(a)) {
              for (var p in a) {
                h = document.createElement("p"), h.textContent = p + ": " + a[p], b.appendChild(h);
              }
            } else {
              h = document.createElement("p"), h.textContent = a.toString(), b.appendChild(h);
            }
          }
        };
        b.prototype.removeTooltipContent = function() {
          for (var b = this._tooltip;b.firstChild;) {
            b.removeChild(b.firstChild);
          }
        };
        return b;
      }();
      g.Controller = b;
    })(r.Profiler || (r.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.NumberUtilities.clamp, r = function() {
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
        function a(a, b) {
          this._target = a;
          this._eventTarget = b;
          this._wheelDisabled = !1;
          this._boundOnMouseDown = this._onMouseDown.bind(this);
          this._boundOnMouseUp = this._onMouseUp.bind(this);
          this._boundOnMouseOver = this._onMouseOver.bind(this);
          this._boundOnMouseOut = this._onMouseOut.bind(this);
          this._boundOnMouseMove = this._onMouseMove.bind(this);
          this._boundOnMouseWheel = this._onMouseWheel.bind(this);
          this._boundOnDrag = this._onDrag.bind(this);
          b.addEventListener("mousedown", this._boundOnMouseDown, !1);
          b.addEventListener("mouseover", this._boundOnMouseOver, !1);
          b.addEventListener("mouseout", this._boundOnMouseOut, !1);
          b.addEventListener("onwheel" in document ? "wheel" : "mousewheel", this._boundOnMouseWheel, !1);
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
        a.prototype.updateCursor = function(b) {
          if (!a._cursorOwner || a._cursorOwner === this._target) {
            var p = this._eventTarget.parentElement;
            a._cursor !== b && (a._cursor = b, ["", "-moz-", "-webkit-"].forEach(function(a) {
              p.style.cursor = a + b;
            }));
            a._cursorOwner = a._cursor === r.DEFAULT ? null : this._target;
          }
        };
        a.prototype._onMouseDown = function(a) {
          this._killHoverCheck();
          if (0 === a.button) {
            var b = this._getTargetMousePos(a, a.target);
            this._dragInfo = {start:b, current:b, delta:{x:0, y:0}, hasMoved:!1, originalTarget:a.target};
            window.addEventListener("mousemove", this._boundOnDrag, !1);
            window.addEventListener("mouseup", this._boundOnMouseUp, !1);
            this._target.onMouseDown(b.x, b.y);
          }
        };
        a.prototype._onDrag = function(a) {
          var b = this._dragInfo;
          a = this._getTargetMousePos(a, b.originalTarget);
          var l = {x:a.x - b.start.x, y:a.y - b.start.y};
          b.current = a;
          b.delta = l;
          b.hasMoved = !0;
          this._target.onDrag(b.start.x, b.start.y, a.x, a.y, l.x, l.y);
        };
        a.prototype._onMouseUp = function(a) {
          window.removeEventListener("mousemove", this._boundOnDrag);
          window.removeEventListener("mouseup", this._boundOnMouseUp);
          var b = this;
          a = this._dragInfo;
          if (a.hasMoved) {
            this._target.onDragEnd(a.start.x, a.start.y, a.current.x, a.current.y, a.delta.x, a.delta.y);
          } else {
            this._target.onClick(a.current.x, a.current.y);
          }
          this._dragInfo = null;
          this._wheelDisabled = !0;
          setTimeout(function() {
            b._wheelDisabled = !1;
          }, 500);
        };
        a.prototype._onMouseOver = function(a) {
          a.target.addEventListener("mousemove", this._boundOnMouseMove, !1);
          if (!this._dragInfo) {
            var b = this._getTargetMousePos(a, a.target);
            this._target.onMouseOver(b.x, b.y);
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
            var b = this._getTargetMousePos(a, a.target);
            this._target.onMouseMove(b.x, b.y);
            this._killHoverCheck();
            this._startHoverCheck(a);
          }
        };
        a.prototype._onMouseWheel = function(a) {
          if (!(a.altKey || a.metaKey || a.ctrlKey || a.shiftKey || (a.preventDefault(), this._dragInfo || this._wheelDisabled))) {
            var p = this._getTargetMousePos(a, a.target);
            a = b("undefined" !== typeof a.deltaY ? a.deltaY / 16 : -a.wheelDelta / 40, -1, 1);
            this._target.onMouseWheel(p.x, p.y, Math.pow(1.2, a) - 1);
          }
        };
        a.prototype._startHoverCheck = function(b) {
          this._hoverInfo = {isHovering:!1, timeoutHandle:setTimeout(this._onMouseMoveIdleHandler.bind(this), a.HOVER_TIMEOUT), pos:this._getTargetMousePos(b, b.target)};
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
        a.prototype._getTargetMousePos = function(a, b) {
          var l = b.getBoundingClientRect();
          return {x:a.clientX - l.left, y:a.clientY - l.top};
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
      (function(b) {
        b[b.NONE = 0] = "NONE";
        b[b.WINDOW = 1] = "WINDOW";
        b[b.HANDLE_LEFT = 2] = "HANDLE_LEFT";
        b[b.HANDLE_RIGHT = 3] = "HANDLE_RIGHT";
        b[b.HANDLE_BOTH = 4] = "HANDLE_BOTH";
      })(g.FlameChartDragTarget || (g.FlameChartDragTarget = {}));
      var b = function() {
        function b(n) {
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
        Object.defineProperty(b.prototype, "canvas", {get:function() {
          return this._canvas;
        }, enumerable:!0, configurable:!0});
        b.prototype.setSize = function(b, a) {
          void 0 === a && (a = 20);
          this._width = b;
          this._height = a;
          this._resetCanvas();
          this.draw();
        };
        b.prototype.initialize = function(b, a) {
          this._initialized = !0;
          this.setRange(b, a);
          this.setWindow(b, a, !1);
          this.draw();
        };
        b.prototype.setWindow = function(b, a, h) {
          void 0 === h && (h = !0);
          this._windowStart = b;
          this._windowEnd = a;
          !h || this.draw();
        };
        b.prototype.setRange = function(b, a) {
          var h = !1;
          void 0 === h && (h = !0);
          this._rangeStart = b;
          this._rangeEnd = a;
          !h || this.draw();
        };
        b.prototype.destroy = function() {
          this._mouseController.destroy();
          this._mouseController = null;
          this._controller.container.removeChild(this._canvas);
          this._controller = null;
        };
        b.prototype._resetCanvas = function() {
          var b = window.devicePixelRatio, a = this._canvas;
          a.width = this._width * b;
          a.height = this._height * b;
          a.style.width = this._width + "px";
          a.style.height = this._height + "px";
        };
        b.prototype.draw = function() {
        };
        b.prototype._almostEq = function(b, a) {
          var h;
          void 0 === h && (h = 10);
          return Math.abs(b - a) < 1 / Math.pow(10, h);
        };
        b.prototype._windowEqRange = function() {
          return this._almostEq(this._windowStart, this._rangeStart) && this._almostEq(this._windowEnd, this._rangeEnd);
        };
        b.prototype._decimalPlaces = function(b) {
          return (+b).toFixed(10).replace(/^-?\d*\.?|0+$/g, "").length;
        };
        b.prototype._toPixelsRelative = function(b) {
          return 0;
        };
        b.prototype._toPixels = function(b) {
          return 0;
        };
        b.prototype._toTimeRelative = function(b) {
          return 0;
        };
        b.prototype._toTime = function(b) {
          return 0;
        };
        b.prototype.onMouseWheel = function(g, a, h) {
          g = this._toTime(g);
          a = this._windowStart;
          var p = this._windowEnd, l = p - a;
          h = Math.max((b.MIN_WINDOW_LEN - l) / l, h);
          this._controller.setWindow(a + (a - g) * h, p + (p - g) * h);
          this.onHoverEnd();
        };
        b.prototype.onMouseDown = function(b, a) {
        };
        b.prototype.onMouseMove = function(b, a) {
        };
        b.prototype.onMouseOver = function(b, a) {
        };
        b.prototype.onMouseOut = function() {
        };
        b.prototype.onDrag = function(b, a, h, p, l, g) {
        };
        b.prototype.onDragEnd = function(b, a, h, p, l, g) {
        };
        b.prototype.onClick = function(b, a) {
        };
        b.prototype.onHoverStart = function(b, a) {
        };
        b.prototype.onHoverEnd = function() {
        };
        b.DRAGHANDLE_WIDTH = 4;
        b.MIN_WINDOW_LEN = .1;
        return b;
      }();
      g.FlameChartBase = b;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.StringUtilities.trimMiddle, r = function(n) {
        function a(a, b) {
          n.call(this, a);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = b;
          this._kindStyle = Object.create(null);
        }
        __extends(a, n);
        a.prototype.setSize = function(a, b) {
          n.prototype.setSize.call(this, a, b || this._initialized ? 12.5 * this._maxDepth : 100);
        };
        a.prototype.initialize = function(a, b) {
          this._initialized = !0;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(a, b);
          this.setWindow(a, b, !1);
          this.setSize(this._width, 12.5 * this._maxDepth);
        };
        a.prototype.destroy = function() {
          n.prototype.destroy.call(this);
          this._snapshot = null;
        };
        a.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio;
          k.ColorStyle.reset();
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, this._width, this._height);
          this._initialized && this._drawChildren(this._snapshot);
          a.restore();
        };
        a.prototype._drawChildren = function(a, b) {
          void 0 === b && (b = 0);
          var l = a.getChildRange(this._windowStart, this._windowEnd);
          if (l) {
            for (var g = l.startIndex;g <= l.endIndex;g++) {
              var m = a.children[g];
              this._drawFrame(m, b) && this._drawChildren(m, b + 1);
            }
          }
        };
        a.prototype._drawFrame = function(a, b) {
          var l = this._context, g = this._toPixels(a.startTime), m = this._toPixels(a.endTime), t = m - g;
          if (t <= this._minFrameWidthInPixels) {
            return l.fillStyle = this._controller.theme.tabToolbar(1), l.fillRect(g, 12.5 * b, this._minFrameWidthInPixels, 12 + 12.5 * (a.maxDepth - a.depth)), !1;
          }
          0 > g && (m = t + g, g = 0);
          var m = m - g, n = this._kindStyle[a.kind.id];
          n || (n = k.ColorStyle.randomStyle(), n = this._kindStyle[a.kind.id] = {bgColor:n, textColor:k.ColorStyle.contrastStyle(n)});
          l.save();
          l.fillStyle = n.bgColor;
          l.fillRect(g, 12.5 * b, m, 12);
          12 < t && (t = a.kind.name) && t.length && (t = this._prepareText(l, t, m - 4), t.length && (l.fillStyle = n.textColor, l.textBaseline = "bottom", l.fillText(t, g + 2, 12.5 * (b + 1) - 1)));
          l.restore();
          return !0;
        };
        a.prototype._prepareText = function(a, p, l) {
          var g = this._measureWidth(a, p);
          if (l > g) {
            return p;
          }
          for (var g = 3, m = p.length;g < m;) {
            var t = g + m >> 1;
            this._measureWidth(a, b(p, t)) < l ? g = t + 1 : m = t;
          }
          p = b(p, m - 1);
          g = this._measureWidth(a, p);
          return g <= l ? p : "";
        };
        a.prototype._measureWidth = function(a, b) {
          var l = this._textWidth[b];
          l || (l = a.measureText(b).width, this._textWidth[b] = l);
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
        a.prototype._getFrameAtPosition = function(a, b) {
          var l = 1 + b / 12.5 | 0, g = this._snapshot.query(this._toTime(a));
          if (g && g.depth >= l) {
            for (;g && g.depth > l;) {
              g = g.parent;
            }
            return g;
          }
          return null;
        };
        a.prototype.onMouseDown = function(a, b) {
          this._windowEqRange() || (this._mouseController.updateCursor(g.MouseCursor.ALL_SCROLL), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:1});
        };
        a.prototype.onMouseMove = function(a, b) {
        };
        a.prototype.onMouseOver = function(a, b) {
        };
        a.prototype.onMouseOut = function() {
        };
        a.prototype.onDrag = function(a, b, l, g, m, t) {
          if (a = this._dragInfo) {
            m = this._toTimeRelative(-m), this._controller.setWindow(a.windowStartInitial + m, a.windowEndInitial + m);
          }
        };
        a.prototype.onDragEnd = function(a, b, l, n, m, t) {
          this._dragInfo = null;
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onClick = function(a, b) {
          this._dragInfo = null;
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onHoverStart = function(a, b) {
          var l = this._getFrameAtPosition(a, b);
          l && (this._hoveredFrame = l, this._controller.showTooltip(this, l, a, b));
        };
        a.prototype.onHoverEnd = function() {
          this._hoveredFrame && (this._hoveredFrame = null, this._controller.hideTooltip());
        };
        a.prototype.getStatistics = function(a) {
          var b = this._snapshot;
          b.statistics || b.calculateStatistics();
          return b.statistics[a.id];
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
      var b = k.NumberUtilities.clamp;
      (function(b) {
        b[b.OVERLAY = 0] = "OVERLAY";
        b[b.STACK = 1] = "STACK";
        b[b.UNION = 2] = "UNION";
      })(g.FlameChartOverviewMode || (g.FlameChartOverviewMode = {}));
      var r = function(k) {
        function a(a, b) {
          void 0 === b && (b = 1);
          this._mode = b;
          this._overviewCanvasDirty = !0;
          this._overviewCanvas = document.createElement("canvas");
          this._overviewContext = this._overviewCanvas.getContext("2d");
          k.call(this, a);
        }
        __extends(a, k);
        a.prototype.setSize = function(a, b) {
          k.prototype.setSize.call(this, a, b || 64);
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
          var a = this._context, b = window.devicePixelRatio, l = this._width, g = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, l, g);
          a.restore();
          this._initialized && (this._overviewCanvasDirty && (this._drawChart(), this._overviewCanvasDirty = !1), a.drawImage(this._overviewCanvas, 0, 0), this._drawSelection());
        };
        a.prototype._drawSelection = function() {
          var a = this._context, b = this._height, l = window.devicePixelRatio, g = this._selection ? this._selection.left : this._toPixels(this._windowStart), m = this._selection ? this._selection.right : this._toPixels(this._windowEnd), t = this._controller.theme;
          a.save();
          a.scale(l, l);
          this._selection ? (a.fillStyle = t.selectionText(.15), a.fillRect(g, 1, m - g, b - 1), a.fillStyle = "rgba(133, 0, 0, 1)", a.fillRect(g + .5, 0, m - g - 1, 4), a.fillRect(g + .5, b - 4, m - g - 1, 4)) : (a.fillStyle = t.bodyBackground(.4), a.fillRect(0, 1, g, b - 1), a.fillRect(m, 1, this._width, b - 1));
          a.beginPath();
          a.moveTo(g, 0);
          a.lineTo(g, b);
          a.moveTo(m, 0);
          a.lineTo(m, b);
          a.lineWidth = .5;
          a.strokeStyle = t.foregroundTextGrey(1);
          a.stroke();
          b = Math.abs((this._selection ? this._toTime(this._selection.right) : this._windowEnd) - (this._selection ? this._toTime(this._selection.left) : this._windowStart));
          a.fillStyle = t.selectionText(.5);
          a.font = "8px sans-serif";
          a.textBaseline = "alphabetic";
          a.textAlign = "end";
          a.fillText(b.toFixed(2), Math.min(g, m) - 4, 10);
          a.fillText((b / 60).toFixed(2), Math.min(g, m) - 4, 20);
          a.restore();
        };
        a.prototype._drawChart = function() {
          var a = window.devicePixelRatio, b = this._height, l = this._controller.activeProfile, g = 4 * this._width, m = l.totalTime / g, t = this._overviewContext, k = this._controller.theme.blueHighlight(1);
          t.save();
          t.translate(0, a * b);
          var e = -a * b / (l.maxDepth - 1);
          t.scale(a / 4, e);
          t.clearRect(0, 0, g, l.maxDepth - 1);
          1 == this._mode && t.scale(1, 1 / l.snapshotCount);
          for (var f = 0, c = l.snapshotCount;f < c;f++) {
            var d = l.getSnapshotAt(f);
            if (d) {
              var q = null, u = 0;
              t.beginPath();
              t.moveTo(0, 0);
              for (var I = 0;I < g;I++) {
                u = l.startTime + I * m, u = (q = q ? q.queryNext(u) : d.query(u)) ? q.getDepth() - 1 : 0, t.lineTo(I, u);
              }
              t.lineTo(I, 0);
              t.fillStyle = k;
              t.fill();
              1 == this._mode && t.translate(0, -b * a / e);
            }
          }
          t.restore();
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
        a.prototype._getDragTargetUnderCursor = function(a, b) {
          if (0 <= b && b < this._height) {
            var l = this._toPixels(this._windowStart), k = this._toPixels(this._windowEnd), m = 2 + g.FlameChartBase.DRAGHANDLE_WIDTH / 2, t = a >= l - m && a <= l + m, n = a >= k - m && a <= k + m;
            if (t && n) {
              return 4;
            }
            if (t) {
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
        a.prototype.onMouseDown = function(a, b) {
          var l = this._getDragTargetUnderCursor(a, b);
          0 === l ? (this._selection = {left:a, right:a}, this.draw()) : (1 === l && this._mouseController.updateCursor(g.MouseCursor.GRABBING), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:l});
        };
        a.prototype.onMouseMove = function(a, b) {
          var l = g.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, b);
          0 === k || this._selection || (l = 1 === k ? g.MouseCursor.GRAB : g.MouseCursor.EW_RESIZE);
          this._mouseController.updateCursor(l);
        };
        a.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, p, l, k, m, t) {
          if (this._selection) {
            this._selection = {left:a, right:b(l, 0, this._width - 1)}, this.draw();
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
                p = b(a.windowStartInitial + m, this._rangeStart, l - g.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3:
                l = b(a.windowEndInitial + m, p + g.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(p, l);
          }
        };
        a.prototype.onDragEnd = function(a, b, l, g, m, t) {
          this._selection && (this._selection = null, this._controller.setWindow(this._toTime(a), this._toTime(l)));
          this._dragInfo = null;
          this.onMouseMove(l, g);
        };
        a.prototype.onClick = function(a, b) {
          this._selection = this._dragInfo = null;
          this._windowEqRange() || (0 === this._getDragTargetUnderCursor(a, b) && this._controller.moveWindowTo(this._toTime(a)), this.onMouseMove(a, b));
          this.draw();
        };
        a.prototype.onHoverStart = function(a, b) {
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
      var b = k.NumberUtilities.clamp;
      (function(b) {
        b[b.OVERVIEW = 0] = "OVERVIEW";
        b[b.CHART = 1] = "CHART";
      })(g.FlameChartHeaderType || (g.FlameChartHeaderType = {}));
      var r = function(k) {
        function a(a, b) {
          this._type = b;
          k.call(this, a);
        }
        __extends(a, k);
        a.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio, l = this._width, g = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.tabToolbar(1);
          a.fillRect(0, 0, l, g);
          this._initialized && (0 == this._type ? (b = this._toPixels(this._windowStart), l = this._toPixels(this._windowEnd), a.fillStyle = this._controller.theme.bodyBackground(1), a.fillRect(b, 0, l - b, g), this._drawLabels(this._rangeStart, this._rangeEnd), this._drawDragHandle(b), this._drawDragHandle(l)) : this._drawLabels(this._windowStart, this._windowEnd));
          a.restore();
        };
        a.prototype._drawLabels = function(b, p) {
          var l = this._context, g = this._calculateTickInterval(b, p), m = Math.ceil(b / g) * g, t = 500 <= g, k = t ? 1E3 : 1, e = this._decimalPlaces(g / k), t = t ? "s" : "ms", f = this._toPixels(m), c = this._height / 2, d = this._controller.theme;
          l.lineWidth = 1;
          l.strokeStyle = d.contentTextDarkGrey(.5);
          l.fillStyle = d.contentTextDarkGrey(1);
          l.textAlign = "right";
          l.textBaseline = "middle";
          l.font = "11px sans-serif";
          for (d = this._width + a.TICK_MAX_WIDTH;f < d;) {
            l.fillText((m / k).toFixed(e) + " " + t, f - 7, c + 1), l.beginPath(), l.moveTo(f, 0), l.lineTo(f, this._height + 1), l.closePath(), l.stroke(), m += g, f = this._toPixels(m);
          }
        };
        a.prototype._calculateTickInterval = function(b, p) {
          var l = (p - b) / (this._width / a.TICK_MAX_WIDTH), g = Math.pow(10, Math.floor(Math.log(l) / Math.LN10)), l = l / g;
          return 5 < l ? 10 * g : 2 < l ? 5 * g : 1 < l ? 2 * g : g;
        };
        a.prototype._drawDragHandle = function(a) {
          var b = this._context;
          b.lineWidth = 2;
          b.strokeStyle = this._controller.theme.bodyBackground(1);
          b.fillStyle = this._controller.theme.foregroundTextGrey(.7);
          this._drawRoundedRect(b, a - g.FlameChartBase.DRAGHANDLE_WIDTH / 2, g.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2);
        };
        a.prototype._drawRoundedRect = function(a, b, l, g) {
          var m, t = !0;
          void 0 === t && (t = !0);
          void 0 === m && (m = !0);
          a.beginPath();
          a.moveTo(b + 2, 1);
          a.lineTo(b + l - 2, 1);
          a.quadraticCurveTo(b + l, 1, b + l, 3);
          a.lineTo(b + l, 1 + g - 2);
          a.quadraticCurveTo(b + l, 1 + g, b + l - 2, 1 + g);
          a.lineTo(b + 2, 1 + g);
          a.quadraticCurveTo(b, 1 + g, b, 1 + g - 2);
          a.lineTo(b, 3);
          a.quadraticCurveTo(b, 1, b + 2, 1);
          a.closePath();
          t && a.stroke();
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
        a.prototype._getDragTargetUnderCursor = function(a, b) {
          if (0 <= b && b < this._height) {
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
        a.prototype.onMouseDown = function(a, b) {
          var l = this._getDragTargetUnderCursor(a, b);
          1 === l && this._mouseController.updateCursor(g.MouseCursor.GRABBING);
          this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:l};
        };
        a.prototype.onMouseMove = function(a, b) {
          var l = g.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, b);
          0 !== k && (1 !== k ? l = g.MouseCursor.EW_RESIZE : 1 !== k || this._windowEqRange() || (l = g.MouseCursor.GRAB));
          this._mouseController.updateCursor(l);
        };
        a.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, p, l, k, m, t) {
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
              p = b(a.windowStartInitial + m, this._rangeStart, l - g.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3:
              l = b(a.windowEndInitial + m, p + g.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(p, l);
        };
        a.prototype.onDragEnd = function(a, b, l, g, m, t) {
          this._dragInfo = null;
          this.onMouseMove(l, g);
        };
        a.prototype.onClick = function(a, b) {
          1 === this._dragInfo.target && this._mouseController.updateCursor(g.MouseCursor.GRAB);
        };
        a.prototype.onHoverStart = function(a, b) {
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
      (function(b) {
        var g = function() {
          function a(a, b, l, g, m) {
            this.pageLoaded = a;
            this.threadsTotal = b;
            this.threadsLoaded = l;
            this.threadFilesTotal = g;
            this.threadFilesLoaded = m;
          }
          a.prototype.toString = function() {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function(a, b, l) {
              return a + ":" + this[a];
            }, this).join(", ") + "]";
          };
          return a;
        }();
        b.TraceLoggerProgressInfo = g;
        var k = function() {
          function a(a) {
            this._baseUrl = a;
            this._threads = [];
            this._progressInfo = null;
          }
          a.prototype.loadPage = function(a, b, l) {
            this._threads = [];
            this._pageLoadCallback = b;
            this._pageLoadProgressCallback = l;
            this._progressInfo = new g(!1, 0, 0, 0, 0);
            this._loadData([a], this._onLoadPage.bind(this));
          };
          Object.defineProperty(a.prototype, "buffers", {get:function() {
            for (var a = [], b = 0, l = this._threads.length;b < l;b++) {
              a.push(this._threads[b].buffer);
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
                var t = a[m], n = [t.dict, t.tree];
                t.corrections && n.push(t.corrections);
                this._progressInfo.threadFilesTotal += n.length;
                this._loadData(n, function(a) {
                  return function(f) {
                    f && (f = new b.Thread(f), f.buffer.name = "Thread " + a, g._threads[a] = f);
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
          a.prototype._loadData = function(a, b, l) {
            var g = 0, m = 0, t = a.length, k = [];
            k.length = t;
            for (var e = 0;e < t;e++) {
              var f = this._baseUrl + a[e], c = new XMLHttpRequest, d = /\.tl$/i.test(f) ? "arraybuffer" : "json";
              c.open("GET", f, !0);
              c.responseType = d;
              c.onload = function(d, c) {
                return function(a) {
                  if ("json" === c) {
                    if (a = this.response, "string" === typeof a) {
                      try {
                        a = JSON.parse(a), k[d] = a;
                      } catch (f) {
                        m++;
                      }
                    } else {
                      k[d] = a;
                    }
                  } else {
                    k[d] = this.response;
                  }
                  ++g;
                  l && l(g);
                  g === t && b(k);
                };
              }(e, d);
              c.send();
            }
          };
          a.colors = "#0044ff #8c4b00 #cc5c33 #ff80c4 #ffbfd9 #ff8800 #8c5e00 #adcc33 #b380ff #bfd9ff #ffaa00 #8c0038 #bf8f30 #f780ff #cc99c9 #aaff00 #000073 #452699 #cc8166 #cca799 #000066 #992626 #cc6666 #ccc299 #ff6600 #526600 #992663 #cc6681 #99ccc2 #ff0066 #520066 #269973 #61994d #739699 #ffcc00 #006629 #269199 #94994d #738299 #ff0000 #590000 #234d8c #8c6246 #7d7399 #ee00ff #00474d #8c2385 #8c7546 #7c8c69 #eeff00 #4d003d #662e1a #62468c #8c6969 #6600ff #4c2900 #1a6657 #8c464f #8c6981 #44ff00 #401100 #1a2466 #663355 #567365 #d90074 #403300 #101d40 #59562d #66614d #cc0000 #002b40 #234010 #4c2626 #4d5e66 #00a3cc #400011 #231040 #4c3626 #464359 #0000bf #331b00 #80e6ff #311a33 #4d3939 #a69b00 #003329 #80ffb2 #331a20 #40303d #00a658 #40ffd9 #ffc480 #ffe1bf #332b26 #8c2500 #9933cc #80fff6 #ffbfbf #303326 #005e8c #33cc47 #b2ff80 #c8bfff #263332 #00708c #cc33ad #ffe680 #f2ffbf #262a33 #388c00 #335ccc #8091ff #bfffd9".split(" ");
          return a;
        }();
        b.TraceLogger = k;
      })(g.TraceLogger || (g.TraceLogger = {}));
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      (function(b) {
        var k;
        (function(b) {
          b[b.START_HI = 0] = "START_HI";
          b[b.START_LO = 4] = "START_LO";
          b[b.STOP_HI = 8] = "STOP_HI";
          b[b.STOP_LO = 12] = "STOP_LO";
          b[b.TEXTID = 16] = "TEXTID";
          b[b.NEXTID = 20] = "NEXTID";
        })(k || (k = {}));
        k = function() {
          function b(a) {
            2 <= a.length && (this._text = a[0], this._data = new DataView(a[1]), this._buffer = new g.TimelineBuffer, this._walkTree(0));
          }
          Object.defineProperty(b.prototype, "buffer", {get:function() {
            return this._buffer;
          }, enumerable:!0, configurable:!0});
          b.prototype._walkTree = function(a) {
            var h = this._data, g = this._buffer;
            do {
              var l = a * b.ITEM_SIZE, k = 4294967295 * h.getUint32(l + 0) + h.getUint32(l + 4), m = 4294967295 * h.getUint32(l + 8) + h.getUint32(l + 12), t = h.getUint32(l + 16), l = h.getUint32(l + 20), x = 1 === (t & 1), t = t >>> 1, t = this._text[t];
              g.enter(t, null, k / 1E6);
              x && this._walkTree(a + 1);
              g.leave(t, null, m / 1E6);
              a = l;
            } while (0 !== a);
          };
          b.ITEM_SIZE = 24;
          return b;
        }();
        b.Thread = k;
      })(g.TraceLogger || (g.TraceLogger = {}));
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.NumberUtilities.clamp, r = function() {
        function a() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        a.prototype.append = function(a, b) {
          var l = this.lines;
          0 < l.length && l[l.length - 1] === a ? this.repeat[l.length - 1]++ : (this.lines.push(a), this.repeat.push(1), this.format.push(b ? {backgroundFillStyle:b} : void 0), this.time.push(performance.now()), this.length++);
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
              case u:
                this.showLineTime = !this.showLineTime;
                break;
              case t:
                h = -1;
                break;
              case k:
                h = 1;
                break;
              case b:
                h = -this.pageLineCount;
                break;
              case l:
                h = this.pageLineCount;
                break;
              case g:
                h = -this.lineIndex;
                break;
              case m:
                h = this.buffer.length - this.lineIndex;
                break;
              case e:
                this.columnIndex -= a.metaKey ? 10 : 1;
                0 > this.columnIndex && (this.columnIndex = 0);
                a.preventDefault();
                break;
              case f:
                this.columnIndex += a.metaKey ? 10 : 1;
                a.preventDefault();
                break;
              case c:
                a.metaKey && (this.selection = {start:0, end:this.buffer.length}, a.preventDefault());
                break;
              case d:
                if (a.metaKey) {
                  var n = "";
                  if (this.selection) {
                    for (var R = this.selection.start;R <= this.selection.end;R++) {
                      n += this.buffer.get(R) + "\n";
                    }
                  } else {
                    n = this.buffer.get(this.lineIndex);
                  }
                  alert(n);
                }
              ;
            }
            a.metaKey && (h *= this.pageLineCount);
            h && (this.scroll(h), a.preventDefault());
            h && a.shiftKey ? this.selection ? this.lineIndex > this.selection.start ? this.selection.end = this.lineIndex : this.selection.start = this.lineIndex : 0 < h ? this.selection = {start:this.lineIndex - h, end:this.lineIndex} : 0 > h && (this.selection = {start:this.lineIndex, end:this.lineIndex - h}) : h && (this.selection = null);
            this.paint();
          }.bind(this), !1);
          a.addEventListener("focus", function(d) {
            this.hasFocus = !0;
          }.bind(this), !1);
          a.addEventListener("blur", function(d) {
            this.hasFocus = !1;
          }.bind(this), !1);
          var b = 33, l = 34, g = 36, m = 35, t = 38, k = 40, e = 37, f = 39, c = 65, d = 67, q = 78, u = 84;
        }
        a.prototype.resize = function() {
          this._resizeHandler();
        };
        a.prototype._resizeHandler = function() {
          var a = this.canvas.parentElement, b = a.clientWidth, a = a.clientHeight - 1, l = window.devicePixelRatio || 1;
          1 !== l ? (this.ratio = l / 1, this.canvas.width = b * this.ratio, this.canvas.height = a * this.ratio, this.canvas.style.width = b + "px", this.canvas.style.height = a + "px") : (this.ratio = 1, this.canvas.width = b, this.canvas.height = a);
          this.pageLineCount = Math.floor(this.canvas.height / this.lineHeight);
        };
        a.prototype.gotoLine = function(a) {
          this.lineIndex = b(a, 0, this.buffer.length - 1);
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
          var b = this.textMarginLeft, l = b + (this.showLineNumbers ? 5 * (String(this.buffer.length).length + 2) : 0), g = l + (this.showLineTime ? 40 : 10), m = g + 25;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
          for (var t = this.canvas.width, k = this.lineHeight, e = 0;e < a;e++) {
            var f = e * this.lineHeight, c = this.pageIndex + e, d = this.buffer.get(c), q = this.buffer.getFormat(c), u = this.buffer.getRepeat(c), I = 1 < c ? this.buffer.getTime(c) - this.buffer.getTime(0) : 0;
            this.context.fillStyle = c % 2 ? this.lineColor : this.alternateLineColor;
            q && q.backgroundFillStyle && (this.context.fillStyle = q.backgroundFillStyle);
            this.context.fillRect(0, f, t, k);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;
            this.selection && c >= this.selection.start && c <= this.selection.end && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, f, t, k), this.context.fillStyle = this.selectionTextColor);
            this.hasFocus && c === this.lineIndex && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, f, t, k), this.context.fillStyle = this.selectionTextColor);
            0 < this.columnIndex && (d = d.substring(this.columnIndex));
            f = (e + 1) * this.lineHeight - this.textMarginBottom;
            this.showLineNumbers && this.context.fillText(String(c), b, f);
            this.showLineTime && this.context.fillText(I.toFixed(1).padLeft(" ", 6), l, f);
            1 < u && this.context.fillText(String(u).padLeft(" ", 3), g, f);
            this.context.fillText(d, m, f);
          }
        };
        a.prototype.refreshEvery = function(a) {
          function b() {
            l.paint();
            l.refreshFrequency && setTimeout(b, l.refreshFrequency);
          }
          var l = this;
          this.refreshFrequency = a;
          l.refreshFrequency && setTimeout(b, l.refreshFrequency);
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
      var b = function() {
        function b(g) {
          this._lastWeightedTime = this._lastTime = this._index = 0;
          this._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
          this._container = g;
          this._canvas = document.createElement("canvas");
          this._container.appendChild(this._canvas);
          this._context = this._canvas.getContext("2d");
          this._listenForContainerSizeChanges();
        }
        b.prototype._listenForContainerSizeChanges = function() {
          var b = this._containerWidth, a = this._containerHeight;
          this._onContainerSizeChanged();
          var h = this;
          setInterval(function() {
            if (b !== h._containerWidth || a !== h._containerHeight) {
              h._onContainerSizeChanged(), b = h._containerWidth, a = h._containerHeight;
            }
          }, 10);
        };
        b.prototype._onContainerSizeChanged = function() {
          var b = this._containerWidth, a = this._containerHeight, h = window.devicePixelRatio || 1;
          1 !== h ? (this._ratio = h / 1, this._canvas.width = b * this._ratio, this._canvas.height = a * this._ratio, this._canvas.style.width = b + "px", this._canvas.style.height = a + "px") : (this._ratio = 1, this._canvas.width = b, this._canvas.height = a);
        };
        Object.defineProperty(b.prototype, "_containerWidth", {get:function() {
          return this._container.clientWidth;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "_containerHeight", {get:function() {
          return this._container.clientHeight;
        }, enumerable:!0, configurable:!0});
        b.prototype.tickAndRender = function(b, a) {
          void 0 === b && (b = !1);
          void 0 === a && (a = 0);
          if (0 === this._lastTime) {
            this._lastTime = performance.now();
          } else {
            var h = 1 * (performance.now() - this._lastTime) + 0 * this._lastWeightedTime, g = this._context, l = 2 * this._ratio, k = 30 * this._ratio, m = performance;
            m.memory && (k += 30 * this._ratio);
            var t = (this._canvas.width - k) / (l + 1) | 0, x = this._index++;
            this._index > t && (this._index = 0);
            t = this._canvas.height;
            g.globalAlpha = 1;
            g.fillStyle = "black";
            g.fillRect(k + x * (l + 1), 0, 4 * l, this._canvas.height);
            var e = Math.min(1E3 / 60 / h, 1);
            g.fillStyle = "#00FF00";
            g.globalAlpha = b ? .5 : 1;
            e = t / 2 * e | 0;
            g.fillRect(k + x * (l + 1), t - e, l, e);
            a && (e = Math.min(1E3 / 240 / a, 1), g.fillStyle = "#FF6347", e = t / 2 * e | 0, g.fillRect(k + x * (l + 1), t / 2 - e, l, e));
            0 === x % 16 && (g.globalAlpha = 1, g.fillStyle = "black", g.fillRect(0, 0, k, this._canvas.height), g.fillStyle = "white", g.font = 8 * this._ratio + "px Arial", g.textBaseline = "middle", l = (1E3 / h).toFixed(0), a && (l += " " + a.toFixed(0)), m.memory && (l += " " + (m.memory.usedJSHeapSize / 1024 / 1024).toFixed(2)), g.fillText(l, 2 * this._ratio, this._containerHeight / 2 * this._ratio));
            this._lastTime = performance.now();
            this._lastWeightedTime = h;
          }
        };
        return b;
      }();
      g.FPS = b;
    })(k.Mini || (k.Mini = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load Shared Dependencies");
console.time("Load GFX Dependencies");
(function(k) {
  (function(r) {
    function g(d, c, a) {
      return p && a ? "string" === typeof c ? (d = k.ColorUtilities.cssStyleToRGBA(c), k.ColorUtilities.rgbaToCSSStyle(a.transformRGBA(d))) : c instanceof CanvasGradient && c._template ? c._template.createCanvasGradient(d, a) : c : c;
    }
    var b = k.NumberUtilities.clamp;
    (function(d) {
      d[d.None = 0] = "None";
      d[d.Brief = 1] = "Brief";
      d[d.Verbose = 2] = "Verbose";
    })(r.TraceLevel || (r.TraceLevel = {}));
    var v = k.Metrics.Counter.instance;
    r.frameCounter = new k.Metrics.Counter(!0);
    r.traceLevel = 2;
    r.writer = null;
    r.frameCount = function(d) {
      v.count(d);
      r.frameCounter.count(d);
    };
    r.timelineBuffer = new k.Tools.Profiler.TimelineBuffer("GFX");
    r.enterTimeline = function(d, c) {
    };
    r.leaveTimeline = function(d, c) {
    };
    var n = null, a = null, h = null, p = !0;
    p && "undefined" !== typeof CanvasRenderingContext2D && (n = CanvasGradient.prototype.addColorStop, a = CanvasRenderingContext2D.prototype.createLinearGradient, h = CanvasRenderingContext2D.prototype.createRadialGradient, CanvasRenderingContext2D.prototype.createLinearGradient = function(d, c, a, f) {
      return (new w(d, c, a, f)).createCanvasGradient(this, null);
    }, CanvasRenderingContext2D.prototype.createRadialGradient = function(d, c, a, f, e, b) {
      return (new m(d, c, a, f, e, b)).createCanvasGradient(this, null);
    }, CanvasGradient.prototype.addColorStop = function(d, c) {
      n.call(this, d, c);
      this._template.addColorStop(d, c);
    });
    var l = function() {
      return function(d, c) {
        this.offset = d;
        this.color = c;
      };
    }(), w = function() {
      function d(c, a, f, e) {
        this.x0 = c;
        this.y0 = a;
        this.x1 = f;
        this.y1 = e;
        this.colorStops = [];
      }
      d.prototype.addColorStop = function(d, c) {
        this.colorStops.push(new l(d, c));
      };
      d.prototype.createCanvasGradient = function(d, c) {
        for (var f = a.call(d, this.x0, this.y0, this.x1, this.y1), e = this.colorStops, b = 0;b < e.length;b++) {
          var q = e[b], u = q.offset, q = q.color, q = c ? g(d, q, c) : q;
          n.call(f, u, q);
        }
        f._template = this;
        f._transform = this._transform;
        return f;
      };
      return d;
    }(), m = function() {
      function d(c, a, f, e, b, q) {
        this.x0 = c;
        this.y0 = a;
        this.r0 = f;
        this.x1 = e;
        this.y1 = b;
        this.r1 = q;
        this.colorStops = [];
      }
      d.prototype.addColorStop = function(d, c) {
        this.colorStops.push(new l(d, c));
      };
      d.prototype.createCanvasGradient = function(d, c) {
        for (var a = h.call(d, this.x0, this.y0, this.r0, this.x1, this.y1, this.r1), f = this.colorStops, e = 0;e < f.length;e++) {
          var b = f[e], q = b.offset, b = b.color, b = c ? g(d, b, c) : b;
          n.call(a, q, b);
        }
        a._template = this;
        a._transform = this._transform;
        return a;
      };
      return d;
    }(), t;
    (function(d) {
      d[d.ClosePath = 1] = "ClosePath";
      d[d.MoveTo = 2] = "MoveTo";
      d[d.LineTo = 3] = "LineTo";
      d[d.QuadraticCurveTo = 4] = "QuadraticCurveTo";
      d[d.BezierCurveTo = 5] = "BezierCurveTo";
      d[d.ArcTo = 6] = "ArcTo";
      d[d.Rect = 7] = "Rect";
      d[d.Arc = 8] = "Arc";
      d[d.Save = 9] = "Save";
      d[d.Restore = 10] = "Restore";
      d[d.Transform = 11] = "Transform";
    })(t || (t = {}));
    var x = function() {
      function d(c) {
        this._commands = new Uint8Array(d._arrayBufferPool.acquire(8), 0, 8);
        this._commandPosition = 0;
        this._data = new Float64Array(d._arrayBufferPool.acquire(8 * Float64Array.BYTES_PER_ELEMENT), 0, 8);
        this._dataPosition = 0;
        c instanceof d && this.addPath(c);
      }
      d._apply = function(d, c) {
        var a = d._commands, f = d._data, e = 0, b = 0;
        c.beginPath();
        for (var q = d._commandPosition;e < q;) {
          switch(a[e++]) {
            case 1:
              c.closePath();
              break;
            case 2:
              c.moveTo(f[b++], f[b++]);
              break;
            case 3:
              c.lineTo(f[b++], f[b++]);
              break;
            case 4:
              c.quadraticCurveTo(f[b++], f[b++], f[b++], f[b++]);
              break;
            case 5:
              c.bezierCurveTo(f[b++], f[b++], f[b++], f[b++], f[b++], f[b++]);
              break;
            case 6:
              c.arcTo(f[b++], f[b++], f[b++], f[b++], f[b++]);
              break;
            case 7:
              c.rect(f[b++], f[b++], f[b++], f[b++]);
              break;
            case 8:
              c.arc(f[b++], f[b++], f[b++], f[b++], f[b++], !!f[b++]);
              break;
            case 9:
              c.save();
              break;
            case 10:
              c.restore();
              break;
            case 11:
              c.transform(f[b++], f[b++], f[b++], f[b++], f[b++], f[b++]);
          }
        }
      };
      d.prototype._ensureCommandCapacity = function(c) {
        this._commands = d._arrayBufferPool.ensureUint8ArrayLength(this._commands, c);
      };
      d.prototype._ensureDataCapacity = function(c) {
        this._data = d._arrayBufferPool.ensureFloat64ArrayLength(this._data, c);
      };
      d.prototype._writeCommand = function(d) {
        this._commandPosition >= this._commands.length && this._ensureCommandCapacity(this._commandPosition + 1);
        this._commands[this._commandPosition++] = d;
      };
      d.prototype._writeData = function(d, c, a, f, b, e) {
        var q = arguments.length;
        this._dataPosition + q >= this._data.length && this._ensureDataCapacity(this._dataPosition + q);
        var u = this._data, m = this._dataPosition;
        u[m] = d;
        u[m + 1] = c;
        2 < q && (u[m + 2] = a, u[m + 3] = f, 4 < q && (u[m + 4] = b, 6 === q && (u[m + 5] = e)));
        this._dataPosition += q;
      };
      d.prototype.closePath = function() {
        this._writeCommand(1);
      };
      d.prototype.moveTo = function(d, c) {
        this._writeCommand(2);
        this._writeData(d, c);
      };
      d.prototype.lineTo = function(d, c) {
        this._writeCommand(3);
        this._writeData(d, c);
      };
      d.prototype.quadraticCurveTo = function(d, c, a, f) {
        this._writeCommand(4);
        this._writeData(d, c, a, f);
      };
      d.prototype.bezierCurveTo = function(d, c, a, f, b, e) {
        this._writeCommand(5);
        this._writeData(d, c, a, f, b, e);
      };
      d.prototype.arcTo = function(d, c, a, f, b) {
        this._writeCommand(6);
        this._writeData(d, c, a, f, b);
      };
      d.prototype.rect = function(d, c, a, f) {
        this._writeCommand(7);
        this._writeData(d, c, a, f);
      };
      d.prototype.arc = function(d, c, a, f, b, e) {
        this._writeCommand(8);
        this._writeData(d, c, a, f, b, +e);
      };
      d.prototype.addPath = function(d, c) {
        c && (this._writeCommand(9), this._writeCommand(11), this._writeData(c.a, c.b, c.c, c.d, c.e, c.f));
        var a = this._commandPosition + d._commandPosition;
        a >= this._commands.length && this._ensureCommandCapacity(a);
        for (var f = this._commands, b = d._commands, e = this._commandPosition, q = 0;e < a;e++) {
          f[e] = b[q++];
        }
        this._commandPosition = a;
        a = this._dataPosition + d._dataPosition;
        a >= this._data.length && this._ensureDataCapacity(a);
        f = this._data;
        b = d._data;
        e = this._dataPosition;
        for (q = 0;e < a;e++) {
          f[e] = b[q++];
        }
        this._dataPosition = a;
        c && this._writeCommand(10);
      };
      d._arrayBufferPool = new k.ArrayBufferPool;
      return d;
    }();
    r.Path = x;
    if ("undefined" !== typeof CanvasRenderingContext2D && ("undefined" === typeof Path2D || !Path2D.prototype.addPath)) {
      var e = CanvasRenderingContext2D.prototype.fill;
      CanvasRenderingContext2D.prototype.fill = function(d, c) {
        arguments.length && (d instanceof x ? x._apply(d, this) : c = d);
        c ? e.call(this, c) : e.call(this);
      };
      var f = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.stroke = function(d, c) {
        arguments.length && (d instanceof x ? x._apply(d, this) : c = d);
        c ? f.call(this, c) : f.call(this);
      };
      var c = CanvasRenderingContext2D.prototype.clip;
      CanvasRenderingContext2D.prototype.clip = function(d, a) {
        arguments.length && (d instanceof x ? x._apply(d, this) : a = d);
        a ? c.call(this, a) : c.call(this);
      };
      window.Path2D = x;
    }
    if ("undefined" !== typeof CanvasPattern && Path2D.prototype.addPath) {
      t = function(d) {
        this._transform = d;
        this._template && (this._template._transform = d);
      };
      CanvasPattern.prototype.setTransform || (CanvasPattern.prototype.setTransform = t);
      CanvasGradient.prototype.setTransform || (CanvasGradient.prototype.setTransform = t);
      var d = CanvasRenderingContext2D.prototype.fill, q = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.fill = function(c, a) {
        var f = !!this.fillStyle._transform;
        if ((this.fillStyle instanceof CanvasPattern || this.fillStyle instanceof CanvasGradient) && f && c instanceof Path2D) {
          var f = this.fillStyle._transform, b;
          try {
            b = f.inverse();
          } catch (e) {
            b = f = r.Geometry.Matrix.createIdentitySVGMatrix();
          }
          this.transform(f.a, f.b, f.c, f.d, f.e, f.f);
          f = new Path2D;
          f.addPath(c, b);
          d.call(this, f, a);
          this.transform(b.a, b.b, b.c, b.d, b.e, b.f);
        } else {
          0 === arguments.length ? d.call(this) : 1 === arguments.length ? d.call(this, c) : 2 === arguments.length && d.call(this, c, a);
        }
      };
      CanvasRenderingContext2D.prototype.stroke = function(d) {
        var c = !!this.strokeStyle._transform;
        if ((this.strokeStyle instanceof CanvasPattern || this.strokeStyle instanceof CanvasGradient) && c && d instanceof Path2D) {
          var a = this.strokeStyle._transform, c = a.inverse();
          this.transform(a.a, a.b, a.c, a.d, a.e, a.f);
          a = new Path2D;
          a.addPath(d, c);
          var f = this.lineWidth;
          this.lineWidth *= (c.a + c.d) / 2;
          q.call(this, a);
          this.transform(c.a, c.b, c.c, c.d, c.e, c.f);
          this.lineWidth = f;
        } else {
          0 === arguments.length ? q.call(this) : 1 === arguments.length && q.call(this, d);
        }
      };
    }
    "undefined" !== typeof CanvasRenderingContext2D && function() {
      function d() {
        return r.Geometry.Matrix.createSVGMatrixFromArray(this.mozCurrentTransform);
      }
      var c = "currentTransform" in CanvasRenderingContext2D.prototype;
      CanvasRenderingContext2D.prototype.flashStroke = function(d, a) {
        if (c) {
          var f = this.currentTransform, e = new Path2D;
          e.addPath(d, f);
          var q = this.lineWidth;
          this.setTransform(1, 0, 0, 1, 0, 0);
          switch(a) {
            case 1:
              this.lineWidth = b(q * (k.getScaleX(f) + k.getScaleY(f)) / 2, 1, 1024);
              break;
            case 2:
              this.lineWidth = b(q * k.getScaleY(f), 1, 1024);
              break;
            case 3:
              this.lineWidth = b(q * k.getScaleX(f), 1, 1024);
          }
          this.stroke(e);
          this.setTransform(f.a, f.b, f.c, f.d, f.e, f.f);
          this.lineWidth = q;
        } else {
          this.stroke(d);
        }
      };
      if (!c) {
        if ("mozCurrentTransform" in CanvasRenderingContext2D.prototype) {
          Object.defineProperty(CanvasRenderingContext2D.prototype, "currentTransform", {get:d}), c = !0;
        } else {
          var a = CanvasRenderingContext2D.prototype.setTransform;
          CanvasRenderingContext2D.prototype.setTransform = function(d, c, f, b, e, q) {
            var u = this.currentTransform;
            u.a = d;
            u.b = c;
            u.c = f;
            u.d = b;
            u.e = e;
            u.f = q;
            a.call(this, d, c, f, b, e, q);
          };
          Object.defineProperty(CanvasRenderingContext2D.prototype, "currentTransform", {get:function() {
            return this._currentTransform || (this._currentTransform = r.Geometry.Matrix.createIdentitySVGMatrix());
          }});
        }
      }
    }();
    if ("undefined" !== typeof CanvasRenderingContext2D && void 0 === CanvasRenderingContext2D.prototype.globalColorMatrix) {
      var u = CanvasRenderingContext2D.prototype.fill, I = CanvasRenderingContext2D.prototype.stroke, H = CanvasRenderingContext2D.prototype.fillText, ea = CanvasRenderingContext2D.prototype.strokeText;
      Object.defineProperty(CanvasRenderingContext2D.prototype, "globalColorMatrix", {get:function() {
        return this._globalColorMatrix ? this._globalColorMatrix.clone() : null;
      }, set:function(d) {
        d ? this._globalColorMatrix ? this._globalColorMatrix.set(d) : this._globalColorMatrix = d.clone() : this._globalColorMatrix = null;
      }, enumerable:!0, configurable:!0});
      CanvasRenderingContext2D.prototype.fill = function(d, c) {
        var a = null;
        this._globalColorMatrix && (a = this.fillStyle, this.fillStyle = g(this, this.fillStyle, this._globalColorMatrix));
        0 === arguments.length ? u.call(this) : 1 === arguments.length ? u.call(this, d) : 2 === arguments.length && u.call(this, d, c);
        a && (this.fillStyle = a);
      };
      CanvasRenderingContext2D.prototype.stroke = function(d, c) {
        var a = null;
        this._globalColorMatrix && (a = this.strokeStyle, this.strokeStyle = g(this, this.strokeStyle, this._globalColorMatrix));
        0 === arguments.length ? I.call(this) : 1 === arguments.length && I.call(this, d);
        a && (this.strokeStyle = a);
      };
      CanvasRenderingContext2D.prototype.fillText = function(d, c, a, f) {
        var b = null;
        this._globalColorMatrix && (b = this.fillStyle, this.fillStyle = g(this, this.fillStyle, this._globalColorMatrix));
        3 === arguments.length ? H.call(this, d, c, a) : 4 === arguments.length ? H.call(this, d, c, a, f) : k.Debug.unexpected();
        b && (this.fillStyle = b);
      };
      CanvasRenderingContext2D.prototype.strokeText = function(d, c, a, f) {
        var b = null;
        this._globalColorMatrix && (b = this.strokeStyle, this.strokeStyle = g(this, this.strokeStyle, this._globalColorMatrix));
        3 === arguments.length ? ea.call(this, d, c, a) : 4 === arguments.length ? ea.call(this, d, c, a, f) : k.Debug.unexpected();
        b && (this.strokeStyle = b);
      };
    }
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    k.ScreenShot = function() {
      return function(g, b, k) {
        this.dataURL = g;
        this.w = b;
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
    g.prototype._unshift = function(b) {
      0 === this._count ? this._head = this._tail = b : (b.next = this._head, this._head = b.next.previous = b);
      this._count++;
    };
    g.prototype._remove = function(b) {
      b === this._head && b === this._tail ? this._head = this._tail = null : b === this._head ? (this._head = b.next, this._head.previous = null) : b == this._tail ? (this._tail = b.previous, this._tail.next = null) : (b.previous.next = b.next, b.next.previous = b.previous);
      b.previous = b.next = null;
      this._count--;
    };
    g.prototype.use = function(b) {
      this._head !== b && ((b.next || b.previous || this._tail === b) && this._remove(b), this._unshift(b));
    };
    g.prototype.pop = function() {
      if (!this._tail) {
        return null;
      }
      var b = this._tail;
      this._remove(b);
      return b;
    };
    g.prototype.visit = function(b, g) {
      void 0 === g && (g = !0);
      for (var k = g ? this._head : this._tail;k && b(k);) {
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
var Shumway$$inline_39 = Shumway || (Shumway = {}), GFX$$inline_40 = Shumway$$inline_39.GFX || (Shumway$$inline_39.GFX = {}), Option$$inline_41 = Shumway$$inline_39.Options.Option, OptionSet$$inline_42 = Shumway$$inline_39.Options.OptionSet, shumwayOptions$$inline_43 = Shumway$$inline_39.Settings.shumwayOptions, rendererOptions$$inline_44 = shumwayOptions$$inline_43.register(new OptionSet$$inline_42("Renderer Options"));
GFX$$inline_40.imageUpdateOption = rendererOptions$$inline_44.register(new Option$$inline_41("", "imageUpdate", "boolean", !0, "Enable image updating."));
GFX$$inline_40.imageConvertOption = rendererOptions$$inline_44.register(new Option$$inline_41("", "imageConvert", "boolean", !0, "Enable image conversion."));
GFX$$inline_40.stageOptions = shumwayOptions$$inline_43.register(new OptionSet$$inline_42("Stage Renderer Options"));
GFX$$inline_40.forcePaint = GFX$$inline_40.stageOptions.register(new Option$$inline_41("", "forcePaint", "boolean", !1, "Force repainting."));
GFX$$inline_40.ignoreViewport = GFX$$inline_40.stageOptions.register(new Option$$inline_41("", "ignoreViewport", "boolean", !1, "Cull elements outside of the viewport."));
GFX$$inline_40.viewportLoupeDiameter = GFX$$inline_40.stageOptions.register(new Option$$inline_41("", "viewportLoupeDiameter", "number", 256, "Size of the viewport loupe.", {range:{min:1, max:1024, step:1}}));
GFX$$inline_40.disableClipping = GFX$$inline_40.stageOptions.register(new Option$$inline_41("", "disableClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_40.debugClipping = GFX$$inline_40.stageOptions.register(new Option$$inline_41("", "debugClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_40.hud = GFX$$inline_40.stageOptions.register(new Option$$inline_41("", "hud", "boolean", !1, "Enable HUD."));
var webGLOptions$$inline_45 = GFX$$inline_40.stageOptions.register(new OptionSet$$inline_42("WebGL Options"));
GFX$$inline_40.perspectiveCamera = webGLOptions$$inline_45.register(new Option$$inline_41("", "pc", "boolean", !1, "Use perspective camera."));
GFX$$inline_40.perspectiveCameraFOV = webGLOptions$$inline_45.register(new Option$$inline_41("", "pcFOV", "number", 60, "Perspective Camera FOV."));
GFX$$inline_40.perspectiveCameraDistance = webGLOptions$$inline_45.register(new Option$$inline_41("", "pcDistance", "number", 2, "Perspective Camera Distance."));
GFX$$inline_40.perspectiveCameraAngle = webGLOptions$$inline_45.register(new Option$$inline_41("", "pcAngle", "number", 0, "Perspective Camera Angle."));
GFX$$inline_40.perspectiveCameraAngleRotate = webGLOptions$$inline_45.register(new Option$$inline_41("", "pcRotate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_40.perspectiveCameraSpacing = webGLOptions$$inline_45.register(new Option$$inline_41("", "pcSpacing", "number", .01, "Element Spacing."));
GFX$$inline_40.perspectiveCameraSpacingInflate = webGLOptions$$inline_45.register(new Option$$inline_41("", "pcInflate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_40.drawTiles = webGLOptions$$inline_45.register(new Option$$inline_41("", "drawTiles", "boolean", !1, "Draw WebGL Tiles"));
GFX$$inline_40.drawSurfaces = webGLOptions$$inline_45.register(new Option$$inline_41("", "drawSurfaces", "boolean", !1, "Draw WebGL Surfaces."));
GFX$$inline_40.drawSurface = webGLOptions$$inline_45.register(new Option$$inline_41("", "drawSurface", "number", -1, "Draw WebGL Surface #"));
GFX$$inline_40.drawElements = webGLOptions$$inline_45.register(new Option$$inline_41("", "drawElements", "boolean", !0, "Actually call gl.drawElements. This is useful to test if the GPU is the bottleneck."));
GFX$$inline_40.disableSurfaceUploads = webGLOptions$$inline_45.register(new Option$$inline_41("", "disableSurfaceUploads", "boolean", !1, "Disable surface uploads."));
GFX$$inline_40.premultipliedAlpha = webGLOptions$$inline_45.register(new Option$$inline_41("", "premultipliedAlpha", "boolean", !1, "Set the premultipliedAlpha flag on getContext()."));
GFX$$inline_40.unpackPremultiplyAlpha = webGLOptions$$inline_45.register(new Option$$inline_41("", "unpackPremultiplyAlpha", "boolean", !0, "Use UNPACK_PREMULTIPLY_ALPHA_WEBGL in pixelStorei."));
var factorChoices$$inline_46 = {ZERO:0, ONE:1, SRC_COLOR:768, ONE_MINUS_SRC_COLOR:769, DST_COLOR:774, ONE_MINUS_DST_COLOR:775, SRC_ALPHA:770, ONE_MINUS_SRC_ALPHA:771, DST_ALPHA:772, ONE_MINUS_DST_ALPHA:773, SRC_ALPHA_SATURATE:776, CONSTANT_COLOR:32769, ONE_MINUS_CONSTANT_COLOR:32770, CONSTANT_ALPHA:32771, ONE_MINUS_CONSTANT_ALPHA:32772};
GFX$$inline_40.sourceBlendFactor = webGLOptions$$inline_45.register(new Option$$inline_41("", "sourceBlendFactor", "number", factorChoices$$inline_46.ONE, "", {choices:factorChoices$$inline_46}));
GFX$$inline_40.destinationBlendFactor = webGLOptions$$inline_45.register(new Option$$inline_41("", "destinationBlendFactor", "number", factorChoices$$inline_46.ONE_MINUS_SRC_ALPHA, "", {choices:factorChoices$$inline_46}));
var canvas2DOptions$$inline_47 = GFX$$inline_40.stageOptions.register(new OptionSet$$inline_42("Canvas2D Options"));
GFX$$inline_40.clipDirtyRegions = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "clipDirtyRegions", "boolean", !1, "Clip dirty regions."));
GFX$$inline_40.clipCanvas = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "clipCanvas", "boolean", !1, "Clip Regions."));
GFX$$inline_40.cull = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "cull", "boolean", !1, "Enable culling."));
GFX$$inline_40.snapToDevicePixels = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "snapToDevicePixels", "boolean", !1, ""));
GFX$$inline_40.imageSmoothing = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "imageSmoothing", "boolean", !1, ""));
GFX$$inline_40.masking = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "masking", "boolean", !0, "Composite Mask."));
GFX$$inline_40.blending = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "blending", "boolean", !0, ""));
GFX$$inline_40.debugLayers = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "debugLayers", "boolean", !1, ""));
GFX$$inline_40.filters = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "filters", "boolean", !1, ""));
GFX$$inline_40.cacheShapes = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "cacheShapes", "boolean", !0, ""));
GFX$$inline_40.cacheShapesMaxSize = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "cacheShapesMaxSize", "number", 256, "", {range:{min:1, max:1024, step:1}}));
GFX$$inline_40.cacheShapesThreshold = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "cacheShapesThreshold", "number", 256, "", {range:{min:1, max:1024, step:1}}));
(function(k) {
  (function(r) {
    (function(g) {
      function b(a, c, d, b) {
        var e = 1 - b;
        return a * e * e + 2 * c * e * b + d * b * b;
      }
      function v(a, c, d, b, e) {
        var m = e * e, h = 1 - e, g = h * h;
        return a * h * g + 3 * c * e * g + 3 * d * h * m + b * e * m;
      }
      var n = k.NumberUtilities.clamp, a = k.NumberUtilities.pow2, h = k.NumberUtilities.epsilonEquals;
      g.radianToDegrees = function(a) {
        return 180 * a / Math.PI;
      };
      g.degreesToRadian = function(a) {
        return a * Math.PI / 180;
      };
      g.quadraticBezier = b;
      g.quadraticBezierExtreme = function(a, c, d) {
        var e = (a - c) / (a - 2 * c + d);
        return 0 > e ? a : 1 < e ? d : b(a, c, d, e);
      };
      g.cubicBezier = v;
      g.cubicBezierExtremes = function(a, c, d, b) {
        var e = c - a, m;
        m = 2 * (d - c);
        var h = b - d;
        e + h === m && (h *= 1.0001);
        var g = 2 * e - m, l = m - 2 * e, l = Math.sqrt(l * l - 4 * e * (e - m + h));
        m = 2 * (e - m + h);
        e = (g + l) / m;
        g = (g - l) / m;
        l = [];
        0 <= e && 1 >= e && l.push(v(a, c, d, b, e));
        0 <= g && 1 >= g && l.push(v(a, c, d, b, g));
        return l;
      };
      var p = function() {
        function a(c, d) {
          this.x = c;
          this.y = d;
        }
        a.prototype.setElements = function(c, d) {
          this.x = c;
          this.y = d;
          return this;
        };
        a.prototype.set = function(c) {
          this.x = c.x;
          this.y = c.y;
          return this;
        };
        a.prototype.dot = function(c) {
          return this.x * c.x + this.y * c.y;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.distanceTo = function(c) {
          return Math.sqrt(this.dot(c));
        };
        a.prototype.sub = function(c) {
          this.x -= c.x;
          this.y -= c.y;
          return this;
        };
        a.prototype.mul = function(c) {
          this.x *= c;
          this.y *= c;
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y);
        };
        a.prototype.toString = function(c) {
          void 0 === c && (c = 2);
          return "{x: " + this.x.toFixed(c) + ", y: " + this.y.toFixed(c) + "}";
        };
        a.prototype.inTriangle = function(c, d, a) {
          var f = c.y * a.x - c.x * a.y + (a.y - c.y) * this.x + (c.x - a.x) * this.y, b = c.x * d.y - c.y * d.x + (c.y - d.y) * this.x + (d.x - c.x) * this.y;
          if (0 > f != 0 > b) {
            return !1;
          }
          c = -d.y * a.x + c.y * (a.x - d.x) + c.x * (d.y - a.y) + d.x * a.y;
          0 > c && (f = -f, b = -b, c = -c);
          return 0 < f && 0 < b && f + b < c;
        };
        a.createEmpty = function() {
          return new a(0, 0);
        };
        a.createEmptyPoints = function(c) {
          for (var d = [], b = 0;b < c;b++) {
            d.push(new a(0, 0));
          }
          return d;
        };
        return a;
      }();
      g.Point = p;
      var l = function() {
        function a(c, d, f) {
          this.x = c;
          this.y = d;
          this.z = f;
        }
        a.prototype.setElements = function(c, d, a) {
          this.x = c;
          this.y = d;
          this.z = a;
          return this;
        };
        a.prototype.set = function(c) {
          this.x = c.x;
          this.y = c.y;
          this.z = c.z;
          return this;
        };
        a.prototype.dot = function(c) {
          return this.x * c.x + this.y * c.y + this.z * c.z;
        };
        a.prototype.cross = function(c) {
          var d = this.z * c.x - this.x * c.z, a = this.x * c.y - this.y * c.x;
          this.x = this.y * c.z - this.z * c.y;
          this.y = d;
          this.z = a;
          return this;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.sub = function(c) {
          this.x -= c.x;
          this.y -= c.y;
          this.z -= c.z;
          return this;
        };
        a.prototype.mul = function(c) {
          this.x *= c;
          this.y *= c;
          this.z *= c;
          return this;
        };
        a.prototype.normalize = function() {
          var c = Math.sqrt(this.squaredLength());
          1E-5 < c ? this.mul(1 / c) : this.setElements(0, 0, 0);
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y, this.z);
        };
        a.prototype.toString = function(c) {
          void 0 === c && (c = 2);
          return "{x: " + this.x.toFixed(c) + ", y: " + this.y.toFixed(c) + ", z: " + this.z.toFixed(c) + "}";
        };
        a.createEmpty = function() {
          return new a(0, 0, 0);
        };
        a.createEmptyPoints = function(c) {
          for (var d = [], b = 0;b < c;b++) {
            d.push(new a(0, 0, 0));
          }
          return d;
        };
        return a;
      }();
      g.Point3D = l;
      var w = function() {
        function a(c, d, b, e) {
          this.setElements(c, d, b, e);
          a.allocationCount++;
        }
        a.prototype.setElements = function(c, d, a, f) {
          this.x = c;
          this.y = d;
          this.w = a;
          this.h = f;
        };
        a.prototype.set = function(c) {
          this.x = c.x;
          this.y = c.y;
          this.w = c.w;
          this.h = c.h;
        };
        a.prototype.contains = function(c) {
          var d = c.x + c.w, a = c.y + c.h, f = this.x + this.w, b = this.y + this.h;
          return c.x >= this.x && c.x < f && c.y >= this.y && c.y < b && d > this.x && d <= f && a > this.y && a <= b;
        };
        a.prototype.containsPoint = function(c) {
          return c.x >= this.x && c.x < this.x + this.w && c.y >= this.y && c.y < this.y + this.h;
        };
        a.prototype.isContained = function(c) {
          for (var d = 0;d < c.length;d++) {
            if (c[d].contains(this)) {
              return !0;
            }
          }
          return !1;
        };
        a.prototype.isSmallerThan = function(c) {
          return this.w < c.w && this.h < c.h;
        };
        a.prototype.isLargerThan = function(c) {
          return this.w > c.w && this.h > c.h;
        };
        a.prototype.union = function(c) {
          if (this.isEmpty()) {
            this.set(c);
          } else {
            if (!c.isEmpty()) {
              var d = this.x, a = this.y;
              this.x > c.x && (d = c.x);
              this.y > c.y && (a = c.y);
              var f = this.x + this.w;
              f < c.x + c.w && (f = c.x + c.w);
              var b = this.y + this.h;
              b < c.y + c.h && (b = c.y + c.h);
              this.x = d;
              this.y = a;
              this.w = f - d;
              this.h = b - a;
            }
          }
        };
        a.prototype.isEmpty = function() {
          return 0 >= this.w || 0 >= this.h;
        };
        a.prototype.setEmpty = function() {
          this.h = this.w = this.y = this.x = 0;
        };
        a.prototype.intersect = function(c) {
          var d = a.createEmpty();
          if (this.isEmpty() || c.isEmpty()) {
            return d.setEmpty(), d;
          }
          d.x = Math.max(this.x, c.x);
          d.y = Math.max(this.y, c.y);
          d.w = Math.min(this.x + this.w, c.x + c.w) - d.x;
          d.h = Math.min(this.y + this.h, c.y + c.h) - d.y;
          d.isEmpty() && d.setEmpty();
          this.set(d);
        };
        a.prototype.intersects = function(c) {
          if (this.isEmpty() || c.isEmpty()) {
            return !1;
          }
          var d = Math.max(this.x, c.x), a = Math.max(this.y, c.y), d = Math.min(this.x + this.w, c.x + c.w) - d;
          c = Math.min(this.y + this.h, c.y + c.h) - a;
          return !(0 >= d || 0 >= c);
        };
        a.prototype.intersectsTransformedAABB = function(c, d) {
          var b = a._temporary;
          b.set(c);
          d.transformRectangleAABB(b);
          return this.intersects(b);
        };
        a.prototype.intersectsTranslated = function(c, d, a) {
          if (this.isEmpty() || c.isEmpty()) {
            return !1;
          }
          var f = Math.max(this.x, c.x + d), b = Math.max(this.y, c.y + a);
          d = Math.min(this.x + this.w, c.x + d + c.w) - f;
          c = Math.min(this.y + this.h, c.y + a + c.h) - b;
          return !(0 >= d || 0 >= c);
        };
        a.prototype.area = function() {
          return this.w * this.h;
        };
        a.prototype.clone = function() {
          var c = a.allocate();
          c.set(this);
          return c;
        };
        a.allocate = function() {
          var c = a._dirtyStack;
          return c.length ? c.pop() : new a(12345, 67890, 12345, 67890);
        };
        a.prototype.free = function() {
          a._dirtyStack.push(this);
        };
        a.prototype.snap = function() {
          var c = Math.ceil(this.x + this.w), d = Math.ceil(this.y + this.h);
          this.x = Math.floor(this.x);
          this.y = Math.floor(this.y);
          this.w = c - this.x;
          this.h = d - this.y;
          return this;
        };
        a.prototype.scale = function(c, d) {
          this.x *= c;
          this.y *= d;
          this.w *= c;
          this.h *= d;
          return this;
        };
        a.prototype.offset = function(c, d) {
          this.x += c;
          this.y += d;
          return this;
        };
        a.prototype.resize = function(c, d) {
          this.w += c;
          this.h += d;
          return this;
        };
        a.prototype.expand = function(c, d) {
          this.offset(-c, -d).resize(2 * c, 2 * d);
          return this;
        };
        a.prototype.getCenter = function() {
          return new p(this.x + this.w / 2, this.y + this.h / 2);
        };
        a.prototype.getAbsoluteBounds = function() {
          return new a(0, 0, this.w, this.h);
        };
        a.prototype.toString = function(c) {
          void 0 === c && (c = 2);
          return "{" + this.x.toFixed(c) + ", " + this.y.toFixed(c) + ", " + this.w.toFixed(c) + ", " + this.h.toFixed(c) + "}";
        };
        a.createEmpty = function() {
          var c = a.allocate();
          c.setEmpty();
          return c;
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
        a.prototype.getCorners = function(c) {
          c[0].x = this.x;
          c[0].y = this.y;
          c[1].x = this.x + this.w;
          c[1].y = this.y;
          c[2].x = this.x + this.w;
          c[2].y = this.y + this.h;
          c[3].x = this.x;
          c[3].y = this.y + this.h;
        };
        a.allocationCount = 0;
        a._temporary = new a(0, 0, 0, 0);
        a._dirtyStack = [];
        return a;
      }();
      g.Rectangle = w;
      var m = function() {
        function a(c) {
          this.corners = c.map(function(d) {
            return d.clone();
          });
          this.axes = [c[1].clone().sub(c[0]), c[3].clone().sub(c[0])];
          this.origins = [];
          for (var d = 0;2 > d;d++) {
            this.axes[d].mul(1 / this.axes[d].squaredLength()), this.origins.push(c[0].dot(this.axes[d]));
          }
        }
        a.prototype.getBounds = function() {
          return a.getBounds(this.corners);
        };
        a.getBounds = function(c) {
          for (var d = new p(Number.MAX_VALUE, Number.MAX_VALUE), a = new p(Number.MIN_VALUE, Number.MIN_VALUE), f = 0;4 > f;f++) {
            var b = c[f].x, e = c[f].y;
            d.x = Math.min(d.x, b);
            d.y = Math.min(d.y, e);
            a.x = Math.max(a.x, b);
            a.y = Math.max(a.y, e);
          }
          return new w(d.x, d.y, a.x - d.x, a.y - d.y);
        };
        a.prototype.intersects = function(c) {
          return this.intersectsOneWay(c) && c.intersectsOneWay(this);
        };
        a.prototype.intersectsOneWay = function(c) {
          for (var d = 0;2 > d;d++) {
            for (var a = 0;4 > a;a++) {
              var f = c.corners[a].dot(this.axes[d]), b, e;
              0 === a ? e = b = f : f < b ? b = f : f > e && (e = f);
            }
            if (b > 1 + this.origins[d] || e < this.origins[d]) {
              return !1;
            }
          }
          return !0;
        };
        return a;
      }();
      g.OBB = m;
      (function(a) {
        a[a.Unknown = 0] = "Unknown";
        a[a.Identity = 1] = "Identity";
        a[a.Translation = 2] = "Translation";
      })(g.MatrixType || (g.MatrixType = {}));
      var t = function() {
        function a(c, d, b, e, m, h) {
          this._data = new Float64Array(6);
          this._type = 0;
          this.setElements(c, d, b, e, m, h);
          a.allocationCount++;
        }
        Object.defineProperty(a.prototype, "a", {get:function() {
          return this._data[0];
        }, set:function(c) {
          this._data[0] = c;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "b", {get:function() {
          return this._data[1];
        }, set:function(c) {
          this._data[1] = c;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "c", {get:function() {
          return this._data[2];
        }, set:function(c) {
          this._data[2] = c;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "d", {get:function() {
          return this._data[3];
        }, set:function(c) {
          this._data[3] = c;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "tx", {get:function() {
          return this._data[4];
        }, set:function(c) {
          this._data[4] = c;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "ty", {get:function() {
          return this._data[5];
        }, set:function(c) {
          this._data[5] = c;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        a._createSVGMatrix = function() {
          a._svg || (a._svg = document.createElementNS("http://www.w3.org/2000/svg", "svg"));
          return a._svg.createSVGMatrix();
        };
        a.prototype.setElements = function(c, d, a, b, f, e) {
          var m = this._data;
          m[0] = c;
          m[1] = d;
          m[2] = a;
          m[3] = b;
          m[4] = f;
          m[5] = e;
          this._type = 0;
        };
        a.prototype.set = function(c) {
          var d = this._data, a = c._data;
          d[0] = a[0];
          d[1] = a[1];
          d[2] = a[2];
          d[3] = a[3];
          d[4] = a[4];
          d[5] = a[5];
          this._type = c._type;
        };
        a.prototype.emptyArea = function(c) {
          c = this._data;
          return 0 === c[0] || 0 === c[3] ? !0 : !1;
        };
        a.prototype.infiniteArea = function(c) {
          c = this._data;
          return Infinity === Math.abs(c[0]) || Infinity === Math.abs(c[3]) ? !0 : !1;
        };
        a.prototype.isEqual = function(c) {
          if (1 === this._type && 1 === c._type) {
            return !0;
          }
          var d = this._data;
          c = c._data;
          return d[0] === c[0] && d[1] === c[1] && d[2] === c[2] && d[3] === c[3] && d[4] === c[4] && d[5] === c[5];
        };
        a.prototype.clone = function() {
          var c = a.allocate();
          c.set(this);
          return c;
        };
        a.allocate = function() {
          var c = a._dirtyStack;
          return c.length ? c.pop() : new a(12345, 12345, 12345, 12345, 12345, 12345);
        };
        a.prototype.free = function() {
          a._dirtyStack.push(this);
        };
        a.prototype.transform = function(c, d, a, b, f, e) {
          var m = this._data, h = m[0], g = m[1], l = m[2], p = m[3], k = m[4], t = m[5];
          m[0] = h * c + l * d;
          m[1] = g * c + p * d;
          m[2] = h * a + l * b;
          m[3] = g * a + p * b;
          m[4] = h * f + l * e + k;
          m[5] = g * f + p * e + t;
          this._type = 0;
          return this;
        };
        a.prototype.transformRectangle = function(c, d) {
          var a = this._data, b = a[0], f = a[1], e = a[2], m = a[3], h = a[4], a = a[5], g = c.x, l = c.y, p = c.w, k = c.h;
          d[0].x = b * g + e * l + h;
          d[0].y = f * g + m * l + a;
          d[1].x = b * (g + p) + e * l + h;
          d[1].y = f * (g + p) + m * l + a;
          d[2].x = b * (g + p) + e * (l + k) + h;
          d[2].y = f * (g + p) + m * (l + k) + a;
          d[3].x = b * g + e * (l + k) + h;
          d[3].y = f * g + m * (l + k) + a;
        };
        a.prototype.isTranslationOnly = function() {
          if (2 === this._type) {
            return !0;
          }
          var c = this._data;
          return 1 === c[0] && 0 === c[1] && 0 === c[2] && 1 === c[3] || h(c[0], 1) && h(c[1], 0) && h(c[2], 0) && h(c[3], 1) ? (this._type = 2, !0) : !1;
        };
        a.prototype.transformRectangleAABB = function(c) {
          var d = this._data;
          if (1 !== this._type) {
            if (2 === this._type) {
              c.x += d[4], c.y += d[5];
            } else {
              var a = d[0], b = d[1], f = d[2], e = d[3], m = d[4], h = d[5], g = c.x, l = c.y, p = c.w, k = c.h, d = a * g + f * l + m, t = b * g + e * l + h, n = a * (g + p) + f * l + m, x = b * (g + p) + e * l + h, w = a * (g + p) + f * (l + k) + m, p = b * (g + p) + e * (l + k) + h, a = a * g + f * (l + k) + m, b = b * g + e * (l + k) + h, e = 0;
              d > n && (e = d, d = n, n = e);
              w > a && (e = w, w = a, a = e);
              c.x = d < w ? d : w;
              c.w = (n > a ? n : a) - c.x;
              t > x && (e = t, t = x, x = e);
              p > b && (e = p, p = b, b = e);
              c.y = t < p ? t : p;
              c.h = (x > b ? x : b) - c.y;
            }
          }
        };
        a.prototype.scale = function(c, d) {
          var a = this._data;
          a[0] *= c;
          a[1] *= d;
          a[2] *= c;
          a[3] *= d;
          a[4] *= c;
          a[5] *= d;
          this._type = 0;
          return this;
        };
        a.prototype.scaleClone = function(c, d) {
          return 1 === c && 1 === d ? this : this.clone().scale(c, d);
        };
        a.prototype.rotate = function(c) {
          var d = this._data, a = d[0], b = d[1], f = d[2], e = d[3], m = d[4], h = d[5], g = Math.cos(c);
          c = Math.sin(c);
          d[0] = g * a - c * b;
          d[1] = c * a + g * b;
          d[2] = g * f - c * e;
          d[3] = c * f + g * e;
          d[4] = g * m - c * h;
          d[5] = c * m + g * h;
          this._type = 0;
          return this;
        };
        a.prototype.concat = function(c) {
          if (1 === c._type) {
            return this;
          }
          var d = this._data;
          c = c._data;
          var a = d[0] * c[0], b = 0, f = 0, e = d[3] * c[3], m = d[4] * c[0] + c[4], h = d[5] * c[3] + c[5];
          if (0 !== d[1] || 0 !== d[2] || 0 !== c[1] || 0 !== c[2]) {
            a += d[1] * c[2], e += d[2] * c[1], b += d[0] * c[1] + d[1] * c[3], f += d[2] * c[0] + d[3] * c[2], m += d[5] * c[2], h += d[4] * c[1];
          }
          d[0] = a;
          d[1] = b;
          d[2] = f;
          d[3] = e;
          d[4] = m;
          d[5] = h;
          this._type = 0;
          return this;
        };
        a.prototype.concatClone = function(c) {
          return this.clone().concat(c);
        };
        a.prototype.preMultiply = function(c) {
          var d = this._data, a = c._data;
          if (2 === c._type && this._type & 3) {
            d[4] += a[4], d[5] += a[5], this._type = 2;
          } else {
            if (1 !== c._type) {
              c = a[0] * d[0];
              var b = 0, f = 0, e = a[3] * d[3], m = a[4] * d[0] + d[4], h = a[5] * d[3] + d[5];
              if (0 !== a[1] || 0 !== a[2] || 0 !== d[1] || 0 !== d[2]) {
                c += a[1] * d[2], e += a[2] * d[1], b += a[0] * d[1] + a[1] * d[3], f += a[2] * d[0] + a[3] * d[2], m += a[5] * d[2], h += a[4] * d[1];
              }
              d[0] = c;
              d[1] = b;
              d[2] = f;
              d[3] = e;
              d[4] = m;
              d[5] = h;
              this._type = 0;
            }
          }
        };
        a.prototype.translate = function(c, d) {
          var a = this._data;
          a[4] += c;
          a[5] += d;
          1 === this._type && (this._type = 2);
          return this;
        };
        a.prototype.setIdentity = function() {
          var c = this._data;
          c[0] = 1;
          c[1] = 0;
          c[2] = 0;
          c[3] = 1;
          c[4] = 0;
          c[5] = 0;
          this._type = 1;
        };
        a.prototype.isIdentity = function() {
          if (1 === this._type) {
            return !0;
          }
          var c = this._data;
          return 1 === c[0] && 0 === c[1] && 0 === c[2] && 1 === c[3] && 0 === c[4] && 0 === c[5];
        };
        a.prototype.transformPoint = function(c) {
          if (1 !== this._type) {
            var d = this._data, a = c.x, b = c.y;
            c.x = d[0] * a + d[2] * b + d[4];
            c.y = d[1] * a + d[3] * b + d[5];
          }
        };
        a.prototype.transformPoints = function(c) {
          if (1 !== this._type) {
            for (var d = 0;d < c.length;d++) {
              this.transformPoint(c[d]);
            }
          }
        };
        a.prototype.deltaTransformPoint = function(c) {
          if (1 !== this._type) {
            var d = this._data, a = c.x, b = c.y;
            c.x = d[0] * a + d[2] * b;
            c.y = d[1] * a + d[3] * b;
          }
        };
        a.prototype.inverse = function(c) {
          var d = this._data, a = c._data;
          if (1 === this._type) {
            c.setIdentity();
          } else {
            if (2 === this._type) {
              a[0] = 1, a[1] = 0, a[2] = 0, a[3] = 1, a[4] = -d[4], a[5] = -d[5], c._type = 2;
            } else {
              var b = d[1], f = d[2], e = d[4], m = d[5];
              if (0 === b && 0 === f) {
                var h = a[0] = 1 / d[0], d = a[3] = 1 / d[3];
                a[1] = 0;
                a[2] = 0;
                a[4] = -h * e;
                a[5] = -d * m;
              } else {
                var h = d[0], d = d[3], g = h * d - b * f;
                if (0 === g) {
                  c.setIdentity();
                  return;
                }
                g = 1 / g;
                a[0] = d * g;
                b = a[1] = -b * g;
                f = a[2] = -f * g;
                d = a[3] = h * g;
                a[4] = -(a[0] * e + f * m);
                a[5] = -(b * e + d * m);
              }
              c._type = 0;
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
          var c = this._data;
          if (1 === c[0] && 0 === c[1]) {
            return 1;
          }
          var d = Math.sqrt(c[0] * c[0] + c[1] * c[1]);
          return 0 < c[0] ? d : -d;
        };
        a.prototype.getScaleY = function() {
          var c = this._data;
          if (0 === c[2] && 1 === c[3]) {
            return 1;
          }
          var d = Math.sqrt(c[2] * c[2] + c[3] * c[3]);
          return 0 < c[3] ? d : -d;
        };
        a.prototype.getScale = function() {
          return (this.getScaleX() + this.getScaleY()) / 2;
        };
        a.prototype.getAbsoluteScaleX = function() {
          return Math.abs(this.getScaleX());
        };
        a.prototype.getAbsoluteScaleY = function() {
          return Math.abs(this.getScaleY());
        };
        a.prototype.getRotation = function() {
          var c = this._data;
          return 180 * Math.atan(c[1] / c[0]) / Math.PI;
        };
        a.prototype.isScaleOrRotation = function() {
          var c = this._data;
          return .01 > Math.abs(c[0] * c[2] + c[1] * c[3]);
        };
        a.prototype.toString = function(c) {
          void 0 === c && (c = 2);
          var d = this._data;
          return "{" + d[0].toFixed(c) + ", " + d[1].toFixed(c) + ", " + d[2].toFixed(c) + ", " + d[3].toFixed(c) + ", " + d[4].toFixed(c) + ", " + d[5].toFixed(c) + "}";
        };
        a.prototype.toWebGLMatrix = function() {
          var c = this._data;
          return new Float32Array([c[0], c[1], 0, c[2], c[3], 0, c[4], c[5], 1]);
        };
        a.prototype.toCSSTransform = function() {
          var c = this._data;
          return "matrix(" + c[0] + ", " + c[1] + ", " + c[2] + ", " + c[3] + ", " + c[4] + ", " + c[5] + ")";
        };
        a.createIdentity = function() {
          var c = a.allocate();
          c.setIdentity();
          return c;
        };
        a.prototype.toSVGMatrix = function() {
          var c = this._data, d = a._createSVGMatrix();
          d.a = c[0];
          d.b = c[1];
          d.c = c[2];
          d.d = c[3];
          d.e = c[4];
          d.f = c[5];
          return d;
        };
        a.prototype.snap = function() {
          var c = this._data;
          return this.isTranslationOnly() ? (c[0] = 1, c[1] = 0, c[2] = 0, c[3] = 1, c[4] = Math.round(c[4]), c[5] = Math.round(c[5]), this._type = 2, !0) : !1;
        };
        a.createIdentitySVGMatrix = function() {
          return a._createSVGMatrix();
        };
        a.createSVGMatrixFromArray = function(c) {
          var d = a._createSVGMatrix();
          d.a = c[0];
          d.b = c[1];
          d.c = c[2];
          d.d = c[3];
          d.e = c[4];
          d.f = c[5];
          return d;
        };
        a.allocationCount = 0;
        a._dirtyStack = [];
        a.multiply = function(c, d) {
          var a = d._data;
          c.transform(a[0], a[1], a[2], a[3], a[4], a[5]);
        };
        return a;
      }();
      g.Matrix = t;
      t = function() {
        function a(c) {
          this._m = new Float32Array(c);
        }
        a.prototype.asWebGLMatrix = function() {
          return this._m;
        };
        a.createCameraLookAt = function(c, d, b) {
          d = c.clone().sub(d).normalize();
          b = b.clone().cross(d).normalize();
          var e = d.clone().cross(b);
          return new a([b.x, b.y, b.z, 0, e.x, e.y, e.z, 0, d.x, d.y, d.z, 0, c.x, c.y, c.z, 1]);
        };
        a.createLookAt = function(c, d, b) {
          d = c.clone().sub(d).normalize();
          b = b.clone().cross(d).normalize();
          var e = d.clone().cross(b);
          return new a([b.x, e.x, d.x, 0, e.x, e.y, d.y, 0, d.x, e.z, d.z, 0, -b.dot(c), -e.dot(c), -d.dot(c), 1]);
        };
        a.prototype.mul = function(c) {
          c = [c.x, c.y, c.z, 0];
          for (var d = this._m, a = [], b = 0;4 > b;b++) {
            a[b] = 0;
            for (var e = 4 * b, f = 0;4 > f;f++) {
              a[b] += d[e + f] * c[f];
            }
          }
          return new l(a[0], a[1], a[2]);
        };
        a.create2DProjection = function(c, d, b) {
          return new a([2 / c, 0, 0, 0, 0, -2 / d, 0, 0, 0, 0, 2 / b, 0, -1, 1, 0, 1]);
        };
        a.createPerspective = function(c) {
          c = Math.tan(.5 * Math.PI - .5 * c);
          var d = 1 / -4999.9;
          return new a([c / 1, 0, 0, 0, 0, c, 0, 0, 0, 0, 5000.1 * d, -1, 0, 0, 1E3 * d, 0]);
        };
        a.createIdentity = function() {
          return a.createTranslation(0, 0);
        };
        a.createTranslation = function(c, d) {
          return new a([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, c, d, 0, 1]);
        };
        a.createXRotation = function(c) {
          var d = Math.cos(c);
          c = Math.sin(c);
          return new a([1, 0, 0, 0, 0, d, c, 0, 0, -c, d, 0, 0, 0, 0, 1]);
        };
        a.createYRotation = function(c) {
          var d = Math.cos(c);
          c = Math.sin(c);
          return new a([d, 0, -c, 0, 0, 1, 0, 0, c, 0, d, 0, 0, 0, 0, 1]);
        };
        a.createZRotation = function(c) {
          var d = Math.cos(c);
          c = Math.sin(c);
          return new a([d, c, 0, 0, -c, d, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
        };
        a.createScale = function(c, d, b) {
          return new a([c, 0, 0, 0, 0, d, 0, 0, 0, 0, b, 0, 0, 0, 0, 1]);
        };
        a.createMultiply = function(c, d) {
          var b = c._m, e = d._m, m = b[0], h = b[1], g = b[2], l = b[3], p = b[4], k = b[5], t = b[6], n = b[7], x = b[8], w = b[9], r = b[10], v = b[11], z = b[12], A = b[13], C = b[14], b = b[15], y = e[0], F = e[1], B = e[2], E = e[3], G = e[4], J = e[5], K = e[6], L = e[7], M = e[8], N = e[9], O = e[10], P = e[11], S = e[12], T = e[13], U = e[14], e = e[15];
          return new a([m * y + h * G + g * M + l * S, m * F + h * J + g * N + l * T, m * B + h * K + g * O + l * U, m * E + h * L + g * P + l * e, p * y + k * G + t * M + n * S, p * F + k * J + t * N + n * T, p * B + k * K + t * O + n * U, p * E + k * L + t * P + n * e, x * y + w * G + r * M + v * S, x * F + w * J + r * N + v * T, x * B + w * K + r * O + v * U, x * E + w * L + r * P + v * e, z * y + A * G + C * M + b * S, z * F + A * J + C * N + b * T, z * B + A * K + C * O + b * U, z * E + A * 
          L + C * P + b * e]);
        };
        a.createInverse = function(c) {
          var d = c._m;
          c = d[0];
          var b = d[1], e = d[2], m = d[3], h = d[4], g = d[5], l = d[6], p = d[7], k = d[8], t = d[9], n = d[10], x = d[11], w = d[12], r = d[13], v = d[14], d = d[15], z = n * d, A = v * x, C = l * d, y = v * p, F = l * x, B = n * p, E = e * d, G = v * m, J = e * x, K = n * m, L = e * p, M = l * m, N = k * r, O = w * t, P = h * r, S = w * g, T = h * t, U = k * g, X = c * r, Y = w * b, Z = c * t, aa = k * b, ba = c * g, ca = h * b, fa = z * g + y * t + F * r - (A * g + C * t + B * r), ga = A * b + 
          E * t + K * r - (z * b + G * t + J * r), r = C * b + G * g + L * r - (y * b + E * g + M * r), b = B * b + J * g + M * t - (F * b + K * g + L * t), g = 1 / (c * fa + h * ga + k * r + w * b);
          return new a([g * fa, g * ga, g * r, g * b, g * (A * h + C * k + B * w - (z * h + y * k + F * w)), g * (z * c + G * k + J * w - (A * c + E * k + K * w)), g * (y * c + E * h + M * w - (C * c + G * h + L * w)), g * (F * c + K * h + L * k - (B * c + J * h + M * k)), g * (N * p + S * x + T * d - (O * p + P * x + U * d)), g * (O * m + X * x + aa * d - (N * m + Y * x + Z * d)), g * (P * m + Y * p + ba * d - (S * m + X * p + ca * d)), g * (U * m + Z * p + ca * x - (T * m + aa * p + ba * x)), g * 
          (P * n + U * v + O * l - (T * v + N * l + S * n)), g * (Z * v + N * e + Y * n - (X * n + aa * v + O * e)), g * (X * l + ca * v + S * e - (ba * v + P * e + Y * l)), g * (ba * n + T * e + aa * l - (Z * l + ca * n + U * e))]);
        };
        return a;
      }();
      g.Matrix3D = t;
      t = function() {
        function a(c, d, b) {
          void 0 === b && (b = 7);
          var e = this.size = 1 << b;
          this.sizeInBits = b;
          this.w = c;
          this.h = d;
          this.c = Math.ceil(c / e);
          this.r = Math.ceil(d / e);
          this.grid = [];
          for (c = 0;c < this.r;c++) {
            for (this.grid.push([]), d = 0;d < this.c;d++) {
              this.grid[c][d] = new a.Cell(new w(d * e, c * e, e, e));
            }
          }
        }
        a.prototype.clear = function() {
          for (var c = 0;c < this.r;c++) {
            for (var d = 0;d < this.c;d++) {
              this.grid[c][d].clear();
            }
          }
        };
        a.prototype.getBounds = function() {
          return new w(0, 0, this.w, this.h);
        };
        a.prototype.addDirtyRectangle = function(c) {
          var d = c.x >> this.sizeInBits, a = c.y >> this.sizeInBits;
          if (!(d >= this.c || a >= this.r)) {
            0 > d && (d = 0);
            0 > a && (a = 0);
            var b = this.grid[a][d];
            c = c.clone();
            c.snap();
            if (b.region.contains(c)) {
              b.bounds.isEmpty() ? b.bounds.set(c) : b.bounds.contains(c) || b.bounds.union(c);
            } else {
              for (var e = Math.min(this.c, Math.ceil((c.x + c.w) / this.size)) - d, f = Math.min(this.r, Math.ceil((c.y + c.h) / this.size)) - a, m = 0;m < e;m++) {
                for (var h = 0;h < f;h++) {
                  b = this.grid[a + h][d + m], b = b.region.clone(), b.intersect(c), b.isEmpty() || this.addDirtyRectangle(b);
                }
              }
            }
          }
        };
        a.prototype.gatherRegions = function(c) {
          for (var d = 0;d < this.r;d++) {
            for (var a = 0;a < this.c;a++) {
              this.grid[d][a].bounds.isEmpty() || c.push(this.grid[d][a].bounds);
            }
          }
        };
        a.prototype.gatherOptimizedRegions = function(c) {
          this.gatherRegions(c);
        };
        a.prototype.getDirtyRatio = function() {
          var c = this.w * this.h;
          if (0 === c) {
            return 0;
          }
          for (var d = 0, a = 0;a < this.r;a++) {
            for (var b = 0;b < this.c;b++) {
              d += this.grid[a][b].region.area();
            }
          }
          return d / c;
        };
        a.prototype.render = function(c, d) {
          function a(d) {
            c.rect(d.x, d.y, d.w, d.h);
          }
          if (d && d.drawGrid) {
            c.strokeStyle = "white";
            for (var b = 0;b < this.r;b++) {
              for (var e = 0;e < this.c;e++) {
                var f = this.grid[b][e];
                c.beginPath();
                a(f.region);
                c.closePath();
                c.stroke();
              }
            }
          }
          c.strokeStyle = "#E0F8D8";
          for (b = 0;b < this.r;b++) {
            for (e = 0;e < this.c;e++) {
              f = this.grid[b][e], c.beginPath(), a(f.bounds), c.closePath(), c.stroke();
            }
          }
        };
        a.tmpRectangle = w.createEmpty();
        return a;
      }();
      g.DirtyRegion = t;
      (function(a) {
        var c = function() {
          function d(d) {
            this.region = d;
            this.bounds = w.createEmpty();
          }
          d.prototype.clear = function() {
            this.bounds.setEmpty();
          };
          return d;
        }();
        a.Cell = c;
      })(t = g.DirtyRegion || (g.DirtyRegion = {}));
      var x = function() {
        function a(c, d, b, e, f, m) {
          this.index = c;
          this.x = d;
          this.y = b;
          this.scale = m;
          this.bounds = new w(d * e, b * f, e, f);
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
      g.Tile = x;
      var e = function() {
        function a(c, d, b, e, f) {
          this.tileW = b;
          this.tileH = e;
          this.scale = f;
          this.w = c;
          this.h = d;
          this.rows = Math.ceil(d / e);
          this.columns = Math.ceil(c / b);
          this.tiles = [];
          for (d = c = 0;d < this.rows;d++) {
            for (var m = 0;m < this.columns;m++) {
              this.tiles.push(new x(c++, m, d, b, e, f));
            }
          }
        }
        a.prototype.getTiles = function(c, d) {
          if (d.emptyArea(c)) {
            return [];
          }
          if (d.infiniteArea(c)) {
            return this.tiles;
          }
          var a = this.columns * this.rows;
          return 40 > a && d.isScaleOrRotation() ? this.getFewTiles(c, d, 10 < a) : this.getManyTiles(c, d);
        };
        a.prototype.getFewTiles = function(c, d, b) {
          void 0 === b && (b = !0);
          if (d.isTranslationOnly() && 1 === this.tiles.length) {
            return this.tiles[0].bounds.intersectsTranslated(c, d.tx, d.ty) ? [this.tiles[0]] : [];
          }
          d.transformRectangle(c, a._points);
          var e;
          c = new w(0, 0, this.w, this.h);
          b && (e = new m(a._points));
          c.intersect(m.getBounds(a._points));
          if (c.isEmpty()) {
            return [];
          }
          var h = c.x / this.tileW | 0;
          d = c.y / this.tileH | 0;
          var g = Math.ceil((c.x + c.w) / this.tileW) | 0, l = Math.ceil((c.y + c.h) / this.tileH) | 0, h = n(h, 0, this.columns), g = n(g, 0, this.columns);
          d = n(d, 0, this.rows);
          for (var l = n(l, 0, this.rows), p = [];h < g;h++) {
            for (var k = d;k < l;k++) {
              var t = this.tiles[k * this.columns + h];
              t.bounds.intersects(c) && (b ? t.getOBB().intersects(e) : 1) && p.push(t);
            }
          }
          return p;
        };
        a.prototype.getManyTiles = function(c, d) {
          function b(d, c, a) {
            return (d - c.x) * (a.y - c.y) / (a.x - c.x) + c.y;
          }
          function e(d, c, a, b, f) {
            if (!(0 > a || a >= c.columns)) {
              for (b = n(b, 0, c.rows), f = n(f + 1, 0, c.rows);b < f;b++) {
                d.push(c.tiles[b * c.columns + a]);
              }
            }
          }
          var m = a._points;
          d.transformRectangle(c, m);
          for (var h = m[0].x < m[1].x ? 0 : 1, g = m[2].x < m[3].x ? 2 : 3, g = m[h].x < m[g].x ? h : g, h = [], l = 0;5 > l;l++, g++) {
            h.push(m[g % 4]);
          }
          (h[1].x - h[0].x) * (h[3].y - h[0].y) < (h[1].y - h[0].y) * (h[3].x - h[0].x) && (m = h[1], h[1] = h[3], h[3] = m);
          var m = [], p, k, g = Math.floor(h[0].x / this.tileW), l = (g + 1) * this.tileW;
          if (h[2].x < l) {
            p = Math.min(h[0].y, h[1].y, h[2].y, h[3].y);
            k = Math.max(h[0].y, h[1].y, h[2].y, h[3].y);
            var t = Math.floor(p / this.tileH), x = Math.floor(k / this.tileH);
            e(m, this, g, t, x);
            return m;
          }
          var w = 0, r = 4, v = !1;
          if (h[0].x === h[1].x || h[0].x === h[3].x) {
            h[0].x === h[1].x ? (v = !0, w++) : r--, p = b(l, h[w], h[w + 1]), k = b(l, h[r], h[r - 1]), t = Math.floor(h[w].y / this.tileH), x = Math.floor(h[r].y / this.tileH), e(m, this, g, t, x), g++;
          }
          do {
            var D, z, A, C;
            h[w + 1].x < l ? (D = h[w + 1].y, A = !0) : (D = b(l, h[w], h[w + 1]), A = !1);
            h[r - 1].x < l ? (z = h[r - 1].y, C = !0) : (z = b(l, h[r], h[r - 1]), C = !1);
            t = Math.floor((h[w].y < h[w + 1].y ? p : D) / this.tileH);
            x = Math.floor((h[r].y > h[r - 1].y ? k : z) / this.tileH);
            e(m, this, g, t, x);
            if (A && v) {
              break;
            }
            A ? (v = !0, w++, p = b(l, h[w], h[w + 1])) : p = D;
            C ? (r--, k = b(l, h[r], h[r - 1])) : k = z;
            g++;
            l = (g + 1) * this.tileW;
          } while (w < r);
          return m;
        };
        a._points = p.createEmptyPoints(4);
        return a;
      }();
      g.TileCache = e;
      t = function() {
        function b(c, d, a) {
          this._cacheLevels = [];
          this._source = c;
          this._tileSize = d;
          this._minUntiledSize = a;
        }
        b.prototype._getTilesAtScale = function(c, d, b) {
          var f = Math.max(d.getAbsoluteScaleX(), d.getAbsoluteScaleY()), m = 0;
          1 !== f && (m = n(Math.round(Math.log(1 / f) / Math.LN2), -5, 3));
          f = a(m);
          if (this._source.hasFlags(1048576)) {
            for (;;) {
              f = a(m);
              if (b.contains(this._source.getBounds().getAbsoluteBounds().clone().scale(f, f))) {
                break;
              }
              m--;
            }
          }
          this._source.hasFlags(2097152) || (m = n(m, -5, 0));
          f = a(m);
          b = 5 + m;
          m = this._cacheLevels[b];
          if (!m) {
            var m = this._source.getBounds().getAbsoluteBounds().clone().scale(f, f), h, g;
            this._source.hasFlags(1048576) || !this._source.hasFlags(4194304) || Math.max(m.w, m.h) <= this._minUntiledSize ? (h = m.w, g = m.h) : h = g = this._tileSize;
            m = this._cacheLevels[b] = new e(m.w, m.h, h, g, f);
          }
          return m.getTiles(c, d.scaleClone(f, f));
        };
        b.prototype.fetchTiles = function(c, d, a, b) {
          var e = new w(0, 0, a.canvas.width, a.canvas.height);
          c = this._getTilesAtScale(c, d, e);
          var f;
          d = this._source;
          for (var m = 0;m < c.length;m++) {
            var h = c[m];
            h.cachedSurfaceRegion && h.cachedSurfaceRegion.surface && !d.hasFlags(1048592) || (f || (f = []), f.push(h));
          }
          f && this._cacheTiles(a, f, b, e);
          d.removeFlags(16);
          return c;
        };
        b.prototype._getTileBounds = function(c) {
          for (var d = w.createEmpty(), a = 0;a < c.length;a++) {
            d.union(c[a].bounds);
          }
          return d;
        };
        b.prototype._cacheTiles = function(c, d, a, b, e) {
          void 0 === e && (e = 4);
          var f = this._getTileBounds(d);
          c.save();
          c.setTransform(1, 0, 0, 1, 0, 0);
          c.clearRect(0, 0, b.w, b.h);
          c.translate(-f.x, -f.y);
          c.scale(d[0].scale, d[0].scale);
          var m = this._source.getBounds();
          c.translate(-m.x, -m.y);
          2 <= r.traceLevel && r.writer && r.writer.writeLn("Rendering Tiles: " + f);
          this._source.render(c, 0);
          c.restore();
          for (var m = null, h = 0;h < d.length;h++) {
            var g = d[h], l = g.bounds.clone();
            l.x -= f.x;
            l.y -= f.y;
            b.contains(l) || (m || (m = []), m.push(g));
            g.cachedSurfaceRegion = a(g.cachedSurfaceRegion, c, l);
          }
          m && (2 <= m.length ? (d = m.slice(0, m.length / 2 | 0), f = m.slice(d.length), this._cacheTiles(c, d, a, b, e - 1), this._cacheTiles(c, f, a, b, e - 1)) : this._cacheTiles(c, m, a, b, e - 1));
        };
        return b;
      }();
      g.RenderableTileCache = t;
    })(r.Geometry || (r.Geometry = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, r) {
  function g() {
    this.constructor = k;
  }
  for (var b in r) {
    r.hasOwnProperty(b) && (k[b] = r[b]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(r) {
    var g = k.IntegerUtilities.roundToMultipleOfPowerOfTwo, b = r.Geometry.Rectangle;
    (function(k) {
      var n = function(a) {
        function b() {
          a.apply(this, arguments);
        }
        __extends(b, a);
        return b;
      }(r.Geometry.Rectangle);
      k.Region = n;
      var a = function() {
        function a(b, e) {
          this._root = new h(0, 0, b | 0, e | 0, !1);
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          var f = this._root.insert(a, b);
          f && (f.allocator = this, f.allocated = !0);
          return f;
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
      var h = function(b) {
        function m(a, f, c, d, q) {
          b.call(this, a, f, c, d);
          this._children = null;
          this._horizontal = q;
          this.allocated = !1;
        }
        __extends(m, b);
        m.prototype.clear = function() {
          this._children = null;
          this.allocated = !1;
        };
        m.prototype.insert = function(a, b) {
          return this._insert(a, b, 0);
        };
        m.prototype._insert = function(b, f, c) {
          if (!(c > a.MAX_DEPTH || this.allocated || this.w < b || this.h < f)) {
            if (this._children) {
              var d;
              if ((d = this._children[0]._insert(b, f, c + 1)) || (d = this._children[1]._insert(b, f, c + 1))) {
                return d;
              }
            } else {
              return d = !this._horizontal, a.RANDOM_ORIENTATION && (d = .5 <= Math.random()), this._children = this._horizontal ? [new m(this.x, this.y, this.w, f, !1), new m(this.x, this.y + f, this.w, this.h - f, d)] : [new m(this.x, this.y, b, this.h, !0), new m(this.x + b, this.y, this.w - b, this.h, d)], d = this._children[0], d.w === b && d.h === f ? (d.allocated = !0, d) : this._insert(b, f, c + 1);
            }
          }
        };
        return m;
      }(k.Region), p = function() {
        function a(b, e, f, c) {
          this._columns = b / f | 0;
          this._rows = e / c | 0;
          this._sizeW = f;
          this._sizeH = c;
          this._freeList = [];
          this._index = 0;
          this._total = this._columns * this._rows;
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          var f = this._sizeW, c = this._sizeH;
          if (a > f || b > c) {
            return null;
          }
          var d = this._freeList, q = this._index;
          return 0 < d.length ? (f = d.pop(), f.w = a, f.h = b, f.allocated = !0, f) : q < this._total ? (d = q / this._columns | 0, f = new l((q - d * this._columns) * f, d * c, a, b), f.index = q, f.allocator = this, f.allocated = !0, this._index++, f) : null;
        };
        a.prototype.free = function(a) {
          a.allocated = !1;
          this._freeList.push(a);
        };
        return a;
      }();
      k.GridAllocator = p;
      var l = function(a) {
        function b(e, f, c, d) {
          a.call(this, e, f, c, d);
          this.index = -1;
        }
        __extends(b, a);
        return b;
      }(k.Region);
      k.GridCell = l;
      var w = function() {
        return function(a, b, e) {
          this.size = a;
          this.region = b;
          this.allocator = e;
        };
      }(), m = function(a) {
        function b(e, f, c, d, q) {
          a.call(this, e, f, c, d);
          this.region = q;
        }
        __extends(b, a);
        return b;
      }(k.Region);
      k.BucketCell = m;
      n = function() {
        function a(b, e) {
          this._buckets = [];
          this._w = b | 0;
          this._h = e | 0;
          this._filled = 0;
        }
        a.prototype.allocate = function(a, e) {
          a = Math.ceil(a);
          e = Math.ceil(e);
          var f = Math.max(a, e);
          if (a > this._w || e > this._h) {
            return null;
          }
          var c = null, d, q = this._buckets;
          do {
            for (var h = 0;h < q.length && !(q[h].size >= f && (d = q[h], c = d.allocator.allocate(a, e)));h++) {
            }
            if (!c) {
              var l = this._h - this._filled;
              if (l < e) {
                return null;
              }
              var h = g(f, 8), k = 2 * h;
              k > l && (k = l);
              if (k < h) {
                return null;
              }
              l = new b(0, this._filled, this._w, k);
              this._buckets.push(new w(h, l, new p(l.w, l.h, h, h)));
              this._filled += k;
            }
          } while (!c);
          return new m(d.region.x + c.x, d.region.y + c.y, c.w, c.h, c);
        };
        a.prototype.free = function(a) {
          a.region.allocator.free(a.region);
        };
        return a;
      }();
      k.BucketAllocator = n;
    })(r.RegionAllocator || (r.RegionAllocator = {}));
    (function(b) {
      var g = function() {
        function a(a) {
          this._createSurface = a;
          this._surfaces = [];
        }
        Object.defineProperty(a.prototype, "surfaces", {get:function() {
          return this._surfaces;
        }, enumerable:!0, configurable:!0});
        a.prototype._createNewSurface = function(a, b) {
          var g = this._createSurface(a, b);
          this._surfaces.push(g);
          return g;
        };
        a.prototype.addSurface = function(a) {
          this._surfaces.push(a);
        };
        a.prototype.allocate = function(a, b, g) {
          for (var k = 0;k < this._surfaces.length;k++) {
            var m = this._surfaces[k];
            if (m !== g && (m = m.allocate(a, b))) {
              return m;
            }
          }
          return this._createNewSurface(a, b).allocate(a, b);
        };
        a.prototype.free = function(a) {
        };
        return a;
      }();
      b.SimpleAllocator = g;
    })(r.SurfaceRegionAllocator || (r.SurfaceRegionAllocator = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = r.Geometry.Rectangle, b = r.Geometry.Matrix, v = r.Geometry.DirtyRegion;
    (function(a) {
      a[a.Normal = 1] = "Normal";
      a[a.Layer = 2] = "Layer";
      a[a.Multiply = 3] = "Multiply";
      a[a.Screen = 4] = "Screen";
      a[a.Lighten = 5] = "Lighten";
      a[a.Darken = 6] = "Darken";
      a[a.Difference = 7] = "Difference";
      a[a.Add = 8] = "Add";
      a[a.Subtract = 9] = "Subtract";
      a[a.Invert = 10] = "Invert";
      a[a.Alpha = 11] = "Alpha";
      a[a.Erase = 12] = "Erase";
      a[a.Overlay = 13] = "Overlay";
      a[a.HardLight = 14] = "HardLight";
    })(r.BlendMode || (r.BlendMode = {}));
    var n = r.BlendMode;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.BoundsAutoCompute = 2] = "BoundsAutoCompute";
      a[a.IsMask = 4] = "IsMask";
      a[a.Dirty = 16] = "Dirty";
      a[a.InvalidBounds = 256] = "InvalidBounds";
      a[a.InvalidConcatenatedMatrix = 512] = "InvalidConcatenatedMatrix";
      a[a.InvalidInvertedConcatenatedMatrix = 1024] = "InvalidInvertedConcatenatedMatrix";
      a[a.InvalidConcatenatedColorMatrix = 2048] = "InvalidConcatenatedColorMatrix";
      a[a.UpOnAddedOrRemoved = a.InvalidBounds | a.Dirty] = "UpOnAddedOrRemoved";
      a[a.UpOnMoved = a.InvalidBounds | a.Dirty] = "UpOnMoved";
      a[a.DownOnAddedOrRemoved = a.InvalidConcatenatedMatrix | a.InvalidInvertedConcatenatedMatrix | a.InvalidConcatenatedColorMatrix] = "DownOnAddedOrRemoved";
      a[a.DownOnMoved = a.InvalidConcatenatedMatrix | a.InvalidInvertedConcatenatedMatrix | a.InvalidConcatenatedColorMatrix] = "DownOnMoved";
      a[a.UpOnColorMatrixChanged = a.Dirty] = "UpOnColorMatrixChanged";
      a[a.DownOnColorMatrixChanged = a.InvalidConcatenatedColorMatrix] = "DownOnColorMatrixChanged";
      a[a.Visible = 65536] = "Visible";
      a[a.UpOnInvalidate = a.InvalidBounds | a.Dirty] = "UpOnInvalidate";
      a[a.Default = a.BoundsAutoCompute | a.InvalidBounds | a.InvalidConcatenatedMatrix | a.InvalidInvertedConcatenatedMatrix | a.Visible] = "Default";
      a[a.CacheAsBitmap = 131072] = "CacheAsBitmap";
      a[a.PixelSnapping = 262144] = "PixelSnapping";
      a[a.ImageSmoothing = 524288] = "ImageSmoothing";
      a[a.Dynamic = 1048576] = "Dynamic";
      a[a.Scalable = 2097152] = "Scalable";
      a[a.Tileable = 4194304] = "Tileable";
      a[a.Transparent = 32768] = "Transparent";
    })(r.NodeFlags || (r.NodeFlags = {}));
    var a = r.NodeFlags;
    (function(a) {
      a[a.Node = 1] = "Node";
      a[a.Shape = 3] = "Shape";
      a[a.Group = 5] = "Group";
      a[a.Stage = 13] = "Stage";
      a[a.Renderable = 33] = "Renderable";
    })(r.NodeType || (r.NodeType = {}));
    var h = r.NodeType;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.OnStageBoundsChanged = 1] = "OnStageBoundsChanged";
      a[a.RemovedFromStage = 2] = "RemovedFromStage";
    })(r.NodeEventType || (r.NodeEventType = {}));
    var p = function() {
      function a() {
      }
      a.prototype.visitNode = function(d, a) {
      };
      a.prototype.visitShape = function(d, a) {
        this.visitNode(d, a);
      };
      a.prototype.visitGroup = function(d, a) {
        this.visitNode(d, a);
        for (var c = d.getChildren(), b = 0;b < c.length;b++) {
          c[b].visit(this, a);
        }
      };
      a.prototype.visitStage = function(d, a) {
        this.visitGroup(d, a);
      };
      a.prototype.visitRenderable = function(d, a) {
        this.visitNode(d, a);
      };
      return a;
    }();
    r.NodeVisitor = p;
    var l = function() {
      return function() {
      };
    }();
    r.State = l;
    var w = function(a) {
      function d() {
        a.call(this);
        this.matrix = b.createIdentity();
        this.depth = 0;
      }
      __extends(d, a);
      d.prototype.transform = function(d) {
        var a = this.clone();
        a.matrix.preMultiply(d.getMatrix());
        return a;
      };
      d.allocate = function() {
        var a = d._dirtyStack, c = null;
        a.length && (c = a.pop());
        return c;
      };
      d.prototype.clone = function() {
        var a = d.allocate();
        a || (a = new d);
        a.set(this);
        return a;
      };
      d.prototype.set = function(a) {
        this.matrix.set(a.matrix);
      };
      d.prototype.free = function() {
        d._dirtyStack.push(this);
      };
      d._dirtyStack = [];
      return d;
    }(l);
    r.PreRenderState = w;
    var m = function(a) {
      function d() {
        a.apply(this, arguments);
        this.isDirty = !0;
      }
      __extends(d, a);
      d.prototype.start = function(a, d) {
        this._dirtyRegion = d;
        var c = new w;
        c.matrix.setIdentity();
        a.visit(this, c);
        c.free();
      };
      d.prototype.visitGroup = function(a, d) {
        var c = a.getChildren();
        this.visitNode(a, d);
        for (var b = 0;b < c.length;b++) {
          var e = c[b], f = d.transform(e.getTransform());
          e.visit(this, f);
          f.free();
        }
      };
      d.prototype.visitNode = function(a, d) {
        a.hasFlags(16) && (this.isDirty = !0);
        a.toggleFlags(16, !1);
        a.depth = d.depth++;
      };
      return d;
    }(p);
    r.PreRenderVisitor = m;
    l = function(a) {
      function d(d) {
        a.call(this);
        this.writer = d;
      }
      __extends(d, a);
      d.prototype.visitNode = function(a, d) {
      };
      d.prototype.visitShape = function(a, d) {
        this.writer.writeLn(a.toString());
        this.visitNode(a, d);
      };
      d.prototype.visitGroup = function(a, d) {
        this.visitNode(a, d);
        var c = a.getChildren();
        this.writer.enter(a.toString() + " " + c.length);
        for (var b = 0;b < c.length;b++) {
          c[b].visit(this, d);
        }
        this.writer.outdent();
      };
      d.prototype.visitStage = function(a, d) {
        this.visitGroup(a, d);
      };
      return d;
    }(p);
    r.TracingNodeVisitor = l;
    var t = function() {
      function c() {
        this._clip = -1;
        this._eventListeners = null;
        this._id = c._nextId++;
        this._type = 1;
        this._index = -1;
        this._parent = null;
        this.reset();
      }
      Object.defineProperty(c.prototype, "id", {get:function() {
        return this._id;
      }, enumerable:!0, configurable:!0});
      c.prototype._dispatchEvent = function(a) {
        if (this._eventListeners) {
          for (var c = this._eventListeners, b = 0;b < c.length;b++) {
            var e = c[b];
            e.type === a && e.listener(this, a);
          }
        }
      };
      c.prototype.addEventListener = function(a, c) {
        this._eventListeners || (this._eventListeners = []);
        this._eventListeners.push({type:a, listener:c});
      };
      c.prototype.removeEventListener = function(a, c) {
        for (var b = this._eventListeners, e = 0;e < b.length;e++) {
          var f = b[e];
          if (f.type === a && f.listener === c) {
            b.splice(e, 1);
            break;
          }
        }
      };
      Object.defineProperty(c.prototype, "properties", {get:function() {
        return this._properties || (this._properties = {});
      }, enumerable:!0, configurable:!0});
      c.prototype.reset = function() {
        this._flags = a.Default;
        this._properties = this._transform = this._layer = this._bounds = null;
        this.depth = -1;
      };
      Object.defineProperty(c.prototype, "clip", {get:function() {
        return this._clip;
      }, set:function(a) {
        this._clip = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "parent", {get:function() {
        return this._parent;
      }, enumerable:!0, configurable:!0});
      c.prototype.getTransformedBounds = function(a) {
        var c = this.getBounds(!0);
        if (a !== this && !c.isEmpty()) {
          var b = this.getTransform().getConcatenatedMatrix();
          a ? (a = a.getTransform().getInvertedConcatenatedMatrix(), a.preMultiply(b), a.transformRectangleAABB(c), a.free()) : b.transformRectangleAABB(c);
        }
        return c;
      };
      c.prototype._markCurrentBoundsAsDirtyRegion = function() {
      };
      c.prototype.getStage = function(a) {
        void 0 === a && (a = !0);
        for (var c = this._parent;c;) {
          if (c.isType(13)) {
            var b = c;
            if (a) {
              if (b.dirtyRegion) {
                return b;
              }
            } else {
              return b;
            }
          }
          c = c._parent;
        }
        return null;
      };
      c.prototype.getChildren = function(a) {
        throw void 0;
      };
      c.prototype.getBounds = function(a) {
        throw void 0;
      };
      c.prototype.setBounds = function(a) {
        (this._bounds || (this._bounds = g.createEmpty())).set(a);
        this.removeFlags(256);
      };
      c.prototype.clone = function() {
        throw void 0;
      };
      c.prototype.setFlags = function(a) {
        this._flags |= a;
      };
      c.prototype.hasFlags = function(a) {
        return (this._flags & a) === a;
      };
      c.prototype.hasAnyFlags = function(a) {
        return !!(this._flags & a);
      };
      c.prototype.removeFlags = function(a) {
        this._flags &= ~a;
      };
      c.prototype.toggleFlags = function(a, c) {
        this._flags = c ? this._flags | a : this._flags & ~a;
      };
      c.prototype._propagateFlagsUp = function(a) {
        if (0 !== a && !this.hasFlags(a)) {
          this.hasFlags(2) || (a &= -257);
          this.setFlags(a);
          var c = this._parent;
          c && c._propagateFlagsUp(a);
        }
      };
      c.prototype._propagateFlagsDown = function(a) {
        throw void 0;
      };
      c.prototype.isAncestor = function(a) {
        for (;a;) {
          if (a === this) {
            return !0;
          }
          a = a._parent;
        }
        return !1;
      };
      c._getAncestors = function(a, b) {
        var e = c._path;
        for (e.length = 0;a && a !== b;) {
          e.push(a), a = a._parent;
        }
        return e;
      };
      c.prototype._findClosestAncestor = function() {
        for (var a = this;a;) {
          if (!1 === a.hasFlags(512)) {
            return a;
          }
          a = a._parent;
        }
        return null;
      };
      c.prototype.isType = function(a) {
        return this._type === a;
      };
      c.prototype.isTypeOf = function(a) {
        return (this._type & a) === a;
      };
      c.prototype.isLeaf = function() {
        return this.isType(33) || this.isType(3);
      };
      c.prototype.isLinear = function() {
        if (this.isLeaf()) {
          return !0;
        }
        if (this.isType(5)) {
          var a = this._children;
          if (1 === a.length && a[0].isLinear()) {
            return !0;
          }
        }
        return !1;
      };
      c.prototype.getTransformMatrix = function() {
        var a;
        void 0 === a && (a = !1);
        return this.getTransform().getMatrix(a);
      };
      c.prototype.getTransform = function() {
        null === this._transform && (this._transform = new e(this));
        return this._transform;
      };
      c.prototype.getLayer = function() {
        null === this._layer && (this._layer = new f(this));
        return this._layer;
      };
      c.prototype.visit = function(a, c) {
        switch(this._type) {
          case 1:
            a.visitNode(this, c);
            break;
          case 5:
            a.visitGroup(this, c);
            break;
          case 13:
            a.visitStage(this, c);
            break;
          case 3:
            a.visitShape(this, c);
            break;
          case 33:
            a.visitRenderable(this, c);
            break;
          default:
            k.Debug.unexpectedCase();
        }
      };
      c.prototype.invalidate = function() {
        this._propagateFlagsUp(a.UpOnInvalidate);
      };
      c.prototype.toString = function(a) {
        void 0 === a && (a = !1);
        var c = h[this._type] + " " + this._id;
        a && (c += " " + this._bounds.toString());
        return c;
      };
      c._path = [];
      c._nextId = 0;
      return c;
    }();
    r.Node = t;
    var x = function(c) {
      function d() {
        c.call(this);
        this._type = 5;
        this._children = [];
      }
      __extends(d, c);
      d.prototype.getChildren = function(a) {
        void 0 === a && (a = !1);
        return a ? this._children.slice(0) : this._children;
      };
      d.prototype.childAt = function(a) {
        return this._children[a];
      };
      Object.defineProperty(d.prototype, "child", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(d.prototype, "groupChild", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      d.prototype.addChild = function(d) {
        d._parent && d._parent.removeChildAt(d._index);
        d._parent = this;
        d._index = this._children.length;
        this._children.push(d);
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
        d._propagateFlagsDown(a.DownOnAddedOrRemoved);
      };
      d.prototype.removeChildAt = function(d) {
        var c = this._children[d];
        this._children.splice(d, 1);
        c._index = -1;
        c._parent = null;
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
        c._propagateFlagsDown(a.DownOnAddedOrRemoved);
      };
      d.prototype.clearChildren = function() {
        for (var d = 0;d < this._children.length;d++) {
          var c = this._children[d];
          c && (c._index = -1, c._parent = null, c._propagateFlagsDown(a.DownOnAddedOrRemoved));
        }
        this._children.length = 0;
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
      };
      d.prototype._propagateFlagsDown = function(a) {
        if (!this.hasFlags(a)) {
          this.setFlags(a);
          for (var d = this._children, c = 0;c < d.length;c++) {
            d[c]._propagateFlagsDown(a);
          }
        }
      };
      d.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var d = this._bounds || (this._bounds = g.createEmpty());
        if (this.hasFlags(256)) {
          d.setEmpty();
          for (var c = this._children, b = g.allocate(), e = 0;e < c.length;e++) {
            var f = c[e];
            b.set(f.getBounds());
            f.getTransformMatrix().transformRectangleAABB(b);
            d.union(b);
          }
          b.free();
          this.removeFlags(256);
        }
        return a ? d.clone() : d;
      };
      return d;
    }(t);
    r.Group = x;
    var e = function() {
      function c(a) {
        this._node = a;
        this._matrix = b.createIdentity();
        this._colorMatrix = r.ColorMatrix.createIdentity();
        this._concatenatedMatrix = b.createIdentity();
        this._invertedConcatenatedMatrix = b.createIdentity();
        this._concatenatedColorMatrix = r.ColorMatrix.createIdentity();
      }
      c.prototype.setMatrix = function(d) {
        this._matrix.isEqual(d) || (this._matrix.set(d), this._node._propagateFlagsUp(a.UpOnMoved), this._node._propagateFlagsDown(a.DownOnMoved));
      };
      c.prototype.setColorMatrix = function(d) {
        this._colorMatrix.set(d);
        this._node._propagateFlagsUp(a.UpOnColorMatrixChanged);
        this._node._propagateFlagsDown(a.DownOnColorMatrixChanged);
      };
      c.prototype.getMatrix = function(a) {
        void 0 === a && (a = !1);
        return a ? this._matrix.clone() : this._matrix;
      };
      c.prototype.hasColorMatrix = function() {
        return null !== this._colorMatrix;
      };
      c.prototype.getColorMatrix = function() {
        var a;
        void 0 === a && (a = !1);
        null === this._colorMatrix && (this._colorMatrix = r.ColorMatrix.createIdentity());
        return a ? this._colorMatrix.clone() : this._colorMatrix;
      };
      c.prototype.getConcatenatedMatrix = function(a) {
        void 0 === a && (a = !1);
        if (this._node.hasFlags(512)) {
          for (var c = this._node._findClosestAncestor(), e = t._getAncestors(this._node, c), f = c ? c.getTransform()._concatenatedMatrix.clone() : b.createIdentity(), m = e.length - 1;0 <= m;m--) {
            var c = e[m], h = c.getTransform();
            f.preMultiply(h._matrix);
            h._concatenatedMatrix.set(f);
            c.removeFlags(512);
          }
        }
        return a ? this._concatenatedMatrix.clone() : this._concatenatedMatrix;
      };
      c.prototype.getInvertedConcatenatedMatrix = function() {
        var a = !0;
        void 0 === a && (a = !1);
        this._node.hasFlags(1024) && (this.getConcatenatedMatrix().inverse(this._invertedConcatenatedMatrix), this._node.removeFlags(1024));
        return a ? this._invertedConcatenatedMatrix.clone() : this._invertedConcatenatedMatrix;
      };
      c.prototype.toString = function() {
        return this._matrix.toString();
      };
      return c;
    }();
    r.Transform = e;
    var f = function() {
      function a(d) {
        this._node = d;
        this._mask = null;
        this._blendMode = 1;
      }
      Object.defineProperty(a.prototype, "filters", {get:function() {
        return this._filters;
      }, set:function(a) {
        this._filters = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "blendMode", {get:function() {
        return this._blendMode;
      }, set:function(a) {
        this._blendMode = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "mask", {get:function() {
        return this._mask;
      }, set:function(a) {
        this._mask && this._mask !== a && this._mask.removeFlags(4);
        (this._mask = a) && this._mask.setFlags(4);
      }, enumerable:!0, configurable:!0});
      a.prototype.toString = function() {
        return n[this._blendMode];
      };
      return a;
    }();
    r.Layer = f;
    l = function(a) {
      function d(d) {
        a.call(this);
        this._source = d;
        this._type = 3;
        this.ratio = 0;
      }
      __extends(d, a);
      d.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var d = this._bounds || (this._bounds = g.createEmpty());
        this.hasFlags(256) && (d.set(this._source.getBounds()), this.removeFlags(256));
        return a ? d.clone() : d;
      };
      Object.defineProperty(d.prototype, "source", {get:function() {
        return this._source;
      }, enumerable:!0, configurable:!0});
      d.prototype._propagateFlagsDown = function(a) {
        this.setFlags(a);
      };
      d.prototype.getChildren = function(a) {
        return [this._source];
      };
      return d;
    }(t);
    r.Shape = l;
    r.RendererOptions = function() {
      return function() {
        this.debug = !1;
        this.paintRenderable = !0;
        this.paintViewport = this.paintFlashing = this.paintDirtyRegion = this.paintBounds = !1;
        this.clear = !0;
      };
    }();
    (function(a) {
      a[a.Canvas2D = 0] = "Canvas2D";
      a[a.WebGL = 1] = "WebGL";
      a[a.Both = 2] = "Both";
      a[a.DOM = 3] = "DOM";
      a[a.SVG = 4] = "SVG";
    })(r.Backend || (r.Backend = {}));
    p = function(a) {
      function d(d, b, e) {
        a.call(this);
        this._container = d;
        this._stage = b;
        this._options = e;
        this._viewport = g.createSquare();
        this._devicePixelRatio = 1;
      }
      __extends(d, a);
      Object.defineProperty(d.prototype, "viewport", {set:function(a) {
        this._viewport.set(a);
      }, enumerable:!0, configurable:!0});
      d.prototype.render = function() {
        throw void 0;
      };
      d.prototype.resize = function() {
        throw void 0;
      };
      d.prototype.screenShot = function(a, d) {
        throw void 0;
      };
      return d;
    }(p);
    r.Renderer = p;
    p = function(a) {
      function d(b, e, f) {
        void 0 === f && (f = !1);
        a.call(this);
        this._preVisitor = new m;
        this._flags &= -3;
        this._type = 13;
        this._scaleMode = d.DEFAULT_SCALE;
        this._align = d.DEFAULT_ALIGN;
        this._content = new x;
        this._content._flags &= -3;
        this.addChild(this._content);
        this.setFlags(16);
        this.setBounds(new g(0, 0, b, e));
        f ? (this._dirtyRegion = new v(b, e), this._dirtyRegion.addDirtyRectangle(new g(0, 0, b, e))) : this._dirtyRegion = null;
        this._updateContentMatrix();
      }
      __extends(d, a);
      Object.defineProperty(d.prototype, "dirtyRegion", {get:function() {
        return this._dirtyRegion;
      }, enumerable:!0, configurable:!0});
      d.prototype.setBounds = function(d) {
        a.prototype.setBounds.call(this, d);
        this._updateContentMatrix();
        this._dispatchEvent(1);
        this._dirtyRegion && (this._dirtyRegion = new v(d.w, d.h), this._dirtyRegion.addDirtyRectangle(d));
      };
      Object.defineProperty(d.prototype, "content", {get:function() {
        return this._content;
      }, enumerable:!0, configurable:!0});
      d.prototype.readyToRender = function() {
        this._preVisitor.isDirty = !1;
        this._preVisitor.start(this, this._dirtyRegion);
        return this._preVisitor.isDirty ? !0 : !1;
      };
      Object.defineProperty(d.prototype, "align", {get:function() {
        return this._align;
      }, set:function(a) {
        this._align = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(d.prototype, "scaleMode", {get:function() {
        return this._scaleMode;
      }, set:function(a) {
        this._scaleMode = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      d.prototype._updateContentMatrix = function() {
        if (this._scaleMode === d.DEFAULT_SCALE && this._align === d.DEFAULT_ALIGN) {
          this._content.getTransform().setMatrix(new b(1, 0, 0, 1, 0, 0));
        } else {
          var a = this.getBounds(), c = this._content.getBounds(), e = a.w / c.w, f = a.h / c.h;
          switch(this._scaleMode) {
            case 2:
              e = f = Math.max(e, f);
              break;
            case 4:
              e = f = 1;
              break;
            case 1:
              break;
            default:
              e = f = Math.min(e, f);
          }
          var m;
          m = this._align & 4 ? 0 : this._align & 8 ? a.w - c.w * e : (a.w - c.w * e) / 2;
          a = this._align & 1 ? 0 : this._align & 2 ? a.h - c.h * f : (a.h - c.h * f) / 2;
          this._content.getTransform().setMatrix(new b(e, 0, 0, f, m, a));
        }
      };
      d.DEFAULT_SCALE = 4;
      d.DEFAULT_ALIGN = 5;
      return d;
    }(x);
    r.Stage = p;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    function g(a, c, d) {
      return a + (c - a) * d;
    }
    function b(a, c, d) {
      return g(a >> 24 & 255, c >> 24 & 255, d) << 24 | g(a >> 16 & 255, c >> 16 & 255, d) << 16 | g(a >> 8 & 255, c >> 8 & 255, d) << 8 | g(a & 255, c & 255, d);
    }
    var v = r.Geometry.Point, n = r.Geometry.Rectangle, a = r.Geometry.Matrix, h = k.ArrayUtilities.indexOf, p = function(a) {
      function c() {
        a.call(this);
        this._parents = [];
        this._renderableParents = [];
        this._invalidateEventListeners = null;
        this._flags &= -3;
        this._type = 33;
      }
      __extends(c, a);
      Object.defineProperty(c.prototype, "parents", {get:function() {
        return this._parents;
      }, enumerable:!0, configurable:!0});
      c.prototype.addParent = function(a) {
        h(this._parents, a);
        this._parents.push(a);
      };
      c.prototype.willRender = function() {
        for (var a = this._parents, c = 0;c < a.length;c++) {
          for (var b = a[c];b;) {
            if (b.isType(13)) {
              return !0;
            }
            if (!b.hasFlags(65536)) {
              break;
            }
            b = b._parent;
          }
        }
        return !1;
      };
      c.prototype.addRenderableParent = function(a) {
        h(this._renderableParents, a);
        this._renderableParents.push(a);
      };
      c.prototype.wrap = function() {
        for (var a, c = this._parents, b = 0;b < c.length;b++) {
          if (a = c[b], !a._parent) {
            return a;
          }
        }
        a = new r.Shape(this);
        this.addParent(a);
        return a;
      };
      c.prototype.invalidate = function() {
        this.setFlags(16);
        for (var a = this._parents, c = 0;c < a.length;c++) {
          a[c].invalidate();
        }
        a = this._renderableParents;
        for (c = 0;c < a.length;c++) {
          a[c].invalidate();
        }
        if (a = this._invalidateEventListeners) {
          for (c = 0;c < a.length;c++) {
            a[c](this);
          }
        }
      };
      c.prototype.addInvalidateEventListener = function(a) {
        this._invalidateEventListeners || (this._invalidateEventListeners = []);
        h(this._invalidateEventListeners, a);
        this._invalidateEventListeners.push(a);
      };
      c.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        return a ? this._bounds.clone() : this._bounds;
      };
      c.prototype.getChildren = function(a) {
        return null;
      };
      c.prototype._propagateFlagsUp = function(a) {
        if (0 !== a && !this.hasFlags(a)) {
          for (var c = 0;c < this._parents.length;c++) {
            this._parents[c]._propagateFlagsUp(a);
          }
        }
      };
      c.prototype.render = function(a, c, b, e, f) {
      };
      return c;
    }(r.Node);
    r.Renderable = p;
    var l = function(a) {
      function c(d, c) {
        a.call(this);
        this.setBounds(d);
        this.render = c;
      }
      __extends(c, a);
      return c;
    }(p);
    r.CustomRenderable = l;
    (function(a) {
      a[a.Idle = 1] = "Idle";
      a[a.Playing = 2] = "Playing";
      a[a.Paused = 3] = "Paused";
      a[a.Ended = 4] = "Ended";
    })(r.RenderableVideoState || (r.RenderableVideoState = {}));
    l = function(a) {
      function c(d, b) {
        a.call(this);
        this._flags = 1048592;
        this._lastPausedTime = this._lastTimeInvalidated = 0;
        this._pauseHappening = this._seekHappening = !1;
        this._isDOMElement = !0;
        this.setBounds(new n(0, 0, 1, 1));
        this._assetId = d;
        this._eventSerializer = b;
        var e = document.createElement("video"), m = this._handleVideoEvent.bind(this);
        e.preload = "metadata";
        e.addEventListener("play", m);
        e.addEventListener("pause", m);
        e.addEventListener("ended", m);
        e.addEventListener("loadeddata", m);
        e.addEventListener("progress", m);
        e.addEventListener("suspend", m);
        e.addEventListener("loadedmetadata", m);
        e.addEventListener("error", m);
        e.addEventListener("seeking", m);
        e.addEventListener("seeked", m);
        e.addEventListener("canplay", m);
        e.style.position = "absolute";
        this._video = e;
        this._videoEventHandler = m;
        c._renderableVideos.push(this);
        "undefined" !== typeof registerInspectorAsset && registerInspectorAsset(-1, -1, this);
        this._state = 1;
      }
      __extends(c, a);
      Object.defineProperty(c.prototype, "video", {get:function() {
        return this._video;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "state", {get:function() {
        return this._state;
      }, enumerable:!0, configurable:!0});
      c.prototype.play = function() {
        this._state = 2;
        this._video.play();
      };
      c.prototype.pause = function() {
        this._state = 3;
        this._video.pause();
      };
      c.prototype._handleVideoEvent = function(a) {
        var c = null, b = this._video;
        switch(a.type) {
          case "play":
            if (!this._pauseHappening) {
              return;
            }
            a = 7;
            break;
          case "pause":
            if (2 === this._state) {
              b.play();
              return;
            }
            a = 6;
            this._pauseHappening = !0;
            break;
          case "ended":
            this._state = 4;
            this._notifyNetStream(3, c);
            a = 4;
            break;
          case "loadeddata":
            this._pauseHappening = !1;
            this._notifyNetStream(2, c);
            this.play();
            return;
          case "canplay":
            if (this._pauseHappening) {
              return;
            }
            a = 5;
            break;
          case "progress":
            a = 10;
            break;
          case "suspend":
            return;
          case "loadedmetadata":
            a = 1;
            c = {videoWidth:b.videoWidth, videoHeight:b.videoHeight, duration:b.duration};
            break;
          case "error":
            a = 11;
            c = {code:b.error.code};
            break;
          case "seeking":
            if (!this._seekHappening) {
              return;
            }
            a = 8;
            break;
          case "seeked":
            if (!this._seekHappening) {
              return;
            }
            a = 9;
            this._seekHappening = !1;
            break;
          default:
            return;
        }
        this._notifyNetStream(a, c);
      };
      c.prototype._notifyNetStream = function(a, c) {
        this._eventSerializer.sendVideoPlaybackEvent(this._assetId, a, c);
      };
      c.prototype.processControlRequest = function(a, c) {
        var b = this._video;
        switch(a) {
          case 1:
            b.src = c.url;
            this.play();
            this._notifyNetStream(0, null);
            break;
          case 9:
            b.paused && b.play();
            break;
          case 2:
            b && (c.paused && !b.paused ? (isNaN(c.time) ? this._lastPausedTime = b.currentTime : (0 !== b.seekable.length && (b.currentTime = c.time), this._lastPausedTime = c.time), this.pause()) : !c.paused && b.paused && (this.play(), isNaN(c.time) || this._lastPausedTime === c.time || 0 === b.seekable.length || (b.currentTime = c.time)));
            break;
          case 3:
            b && 0 !== b.seekable.length && (this._seekHappening = !0, b.currentTime = c.time);
            break;
          case 4:
            return b ? b.currentTime : 0;
          case 5:
            return b ? b.duration : 0;
          case 6:
            b && (b.volume = c.volume);
            break;
          case 7:
            if (!b) {
              return 0;
            }
            var e = -1;
            if (b.buffered) {
              for (var f = 0;f < b.buffered.length;f++) {
                e = Math.max(e, b.buffered.end(f));
              }
            } else {
              e = b.duration;
            }
            return Math.round(500 * e);
          case 8:
            return b ? Math.round(500 * b.duration) : 0;
        }
      };
      c.prototype.checkForUpdate = function() {
        this._lastTimeInvalidated !== this._video.currentTime && (this._isDOMElement || this.invalidate());
        this._lastTimeInvalidated = this._video.currentTime;
      };
      c.checkForVideoUpdates = function() {
        for (var a = c._renderableVideos, b = 0;b < a.length;b++) {
          var e = a[b];
          e.willRender() ? (e._video.parentElement || e.invalidate(), e._video.style.zIndex = e.parents[0].depth + "") : e._video.parentElement && e._dispatchEvent(2);
          a[b].checkForUpdate();
        }
      };
      c.prototype.render = function(a, c, b) {
        (c = this._video) && 0 < c.videoWidth && a.drawImage(c, 0, 0, c.videoWidth, c.videoHeight, 0, 0, this._bounds.w, this._bounds.h);
      };
      c._renderableVideos = [];
      return c;
    }(p);
    r.RenderableVideo = l;
    l = function(a) {
      function c(d, c) {
        a.call(this);
        this._flags = 1048592;
        this.properties = {};
        this.setBounds(c);
        d instanceof HTMLCanvasElement ? this._initializeSourceCanvas(d) : this._sourceImage = d;
      }
      __extends(c, a);
      c.FromDataBuffer = function(a, b, e) {
        var f = document.createElement("canvas");
        f.width = e.w;
        f.height = e.h;
        e = new c(f, e);
        e.updateFromDataBuffer(a, b);
        return e;
      };
      c.FromNode = function(a, b, e, f, m) {
        var h = document.createElement("canvas"), g = a.getBounds();
        h.width = g.w;
        h.height = g.h;
        h = new c(h, g);
        h.drawNode(a, b, e, f, m);
        return h;
      };
      c.FromImage = function(a) {
        return new c(a, new n(0, 0, -1, -1));
      };
      c.prototype.updateFromDataBuffer = function(a, c) {
        if (r.imageUpdateOption.value) {
          var b = c.buffer;
          if (4 !== a && 5 !== a && 6 !== a) {
            var e = this._bounds, f = this._imageData;
            f && f.width === e.w && f.height === e.h || (f = this._imageData = this._context.createImageData(e.w, e.h));
            r.imageConvertOption.value && (b = new Int32Array(b), e = new Int32Array(f.data.buffer), k.ColorUtilities.convertImage(a, 3, b, e));
            this._ensureSourceCanvas();
            this._context.putImageData(f, 0, 0);
          }
          this.invalidate();
        }
      };
      c.prototype.readImageData = function(a) {
        a.writeRawBytes(this.imageData.data);
      };
      c.prototype.render = function(a, c, b) {
        this.renderSource ? a.drawImage(this.renderSource, 0, 0) : this._renderFallback(a);
      };
      c.prototype.drawNode = function(a, c, b, e, f) {
        b = r.Canvas2D;
        e = this.getBounds();
        (new b.Canvas2DRenderer(this._canvas, null)).renderNode(a, f || e, c);
      };
      c.prototype._initializeSourceCanvas = function(a) {
        this._canvas = a;
        this._context = this._canvas.getContext("2d");
      };
      c.prototype._ensureSourceCanvas = function() {
        if (!this._canvas) {
          var a = document.createElement("canvas"), c = this._bounds;
          a.width = c.w;
          a.height = c.h;
          this._initializeSourceCanvas(a);
        }
      };
      Object.defineProperty(c.prototype, "imageData", {get:function() {
        this._canvas || (this._ensureSourceCanvas(), this._context.drawImage(this._sourceImage, 0, 0), this._sourceImage = null);
        return this._context.getImageData(0, 0, this._bounds.w, this._bounds.h);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "renderSource", {get:function() {
        return this._canvas || this._sourceImage;
      }, enumerable:!0, configurable:!0});
      c.prototype._renderFallback = function(a) {
      };
      return c;
    }(p);
    r.RenderableBitmap = l;
    (function(a) {
      a[a.Fill = 0] = "Fill";
      a[a.Stroke = 1] = "Stroke";
      a[a.StrokeFill = 2] = "StrokeFill";
    })(r.PathType || (r.PathType = {}));
    var w = function() {
      return function(a, c, d, b) {
        this.type = a;
        this.style = c;
        this.smoothImage = d;
        this.strokeProperties = b;
        this.path = new Path2D;
      };
    }();
    r.StyledPath = w;
    var m = function() {
      return function(a, c, d, b, e) {
        this.thickness = a;
        this.scaleMode = c;
        this.capsStyle = d;
        this.jointsStyle = b;
        this.miterLimit = e;
      };
    }();
    r.StrokeProperties = m;
    var t = function(b) {
      function c(a, c, e, m) {
        b.call(this);
        this._flags = 6291472;
        this.properties = {};
        this.setBounds(m);
        this._id = a;
        this._pathData = c;
        this._textures = e;
        e.length && this.setFlags(1048576);
      }
      __extends(c, b);
      c.prototype.update = function(a, c, b) {
        this.setBounds(b);
        this._pathData = a;
        this._paths = null;
        this._textures = c;
        this.setFlags(1048576);
        this.invalidate();
      };
      c.prototype.render = function(a, c, b, e, f) {
        void 0 === e && (e = null);
        void 0 === f && (f = !1);
        a.fillStyle = a.strokeStyle = "transparent";
        c = this._deserializePaths(this._pathData, a, c);
        for (b = 0;b < c.length;b++) {
          var m = c[b];
          a.mozImageSmoothingEnabled = a.msImageSmoothingEnabled = a.imageSmoothingEnabled = m.smoothImage;
          if (0 === m.type) {
            e ? e.addPath(m.path, a.currentTransform) : (a.fillStyle = f ? "#000000" : m.style, a.fill(m.path, "evenodd"), a.fillStyle = "transparent");
          } else {
            if (!e && !f) {
              a.strokeStyle = m.style;
              var h = 1;
              m.strokeProperties && (h = m.strokeProperties.scaleMode, a.lineWidth = m.strokeProperties.thickness, a.lineCap = m.strokeProperties.capsStyle, a.lineJoin = m.strokeProperties.jointsStyle, a.miterLimit = m.strokeProperties.miterLimit);
              var g = a.lineWidth;
              (g = 1 === g || 3 === g) && a.translate(.5, .5);
              a.flashStroke(m.path, h);
              g && a.translate(-.5, -.5);
              a.strokeStyle = "transparent";
            }
          }
        }
      };
      c.prototype._deserializePaths = function(a, b, e) {
        if (this._paths) {
          return this._paths;
        }
        e = this._paths = [];
        var f = null, h = null, g = 0, l = 0, p, t, n = !1, w = 0, x = 0, r = a.commands, v = a.coordinates, D = a.styles, z = D.position = 0;
        a = a.commandsPosition;
        for (var A = 0;A < a;A++) {
          switch(r[A]) {
            case 9:
              n && f && (f.lineTo(w, x), h && h.lineTo(w, x));
              n = !0;
              g = w = v[z++] / 20;
              l = x = v[z++] / 20;
              f && f.moveTo(g, l);
              h && h.moveTo(g, l);
              break;
            case 10:
              g = v[z++] / 20;
              l = v[z++] / 20;
              f && f.lineTo(g, l);
              h && h.lineTo(g, l);
              break;
            case 11:
              p = v[z++] / 20;
              t = v[z++] / 20;
              g = v[z++] / 20;
              l = v[z++] / 20;
              f && f.quadraticCurveTo(p, t, g, l);
              h && h.quadraticCurveTo(p, t, g, l);
              break;
            case 12:
              p = v[z++] / 20;
              t = v[z++] / 20;
              var C = v[z++] / 20, y = v[z++] / 20, g = v[z++] / 20, l = v[z++] / 20;
              f && f.bezierCurveTo(p, t, C, y, g, l);
              h && h.bezierCurveTo(p, t, C, y, g, l);
              break;
            case 1:
              f = this._createPath(0, k.ColorUtilities.rgbaToCSSStyle(D.readUnsignedInt()), !1, null, g, l);
              break;
            case 3:
              p = this._readBitmap(D, b);
              f = this._createPath(0, p.style, p.smoothImage, null, g, l);
              break;
            case 2:
              f = this._createPath(0, this._readGradient(D, b), !1, null, g, l);
              break;
            case 4:
              f = null;
              break;
            case 5:
              h = k.ColorUtilities.rgbaToCSSStyle(D.readUnsignedInt());
              D.position += 1;
              p = D.readByte();
              t = c.LINE_CAPS_STYLES[D.readByte()];
              C = c.LINE_JOINTS_STYLES[D.readByte()];
              p = new m(v[z++] / 20, p, t, C, D.readByte());
              h = this._createPath(1, h, !1, p, g, l);
              break;
            case 6:
              h = this._createPath(2, this._readGradient(D, b), !1, null, g, l);
              break;
            case 7:
              p = this._readBitmap(D, b);
              h = this._createPath(2, p.style, p.smoothImage, null, g, l);
              break;
            case 8:
              h = null;
          }
        }
        n && f && (f.lineTo(w, x), h && h.lineTo(w, x));
        this._pathData = null;
        return e;
      };
      c.prototype._createPath = function(a, c, b, e, f, m) {
        a = new w(a, c, b, e);
        this._paths.push(a);
        a.path.moveTo(f, m);
        return a.path;
      };
      c.prototype._readMatrix = function(d) {
        return new a(d.readFloat(), d.readFloat(), d.readFloat(), d.readFloat(), d.readFloat(), d.readFloat());
      };
      c.prototype._readGradient = function(a, c) {
        var b = a.readUnsignedByte(), e = 2 * a.readShort() / 255, f = this._readMatrix(a), b = 16 === b ? c.createLinearGradient(-1, 0, 1, 0) : c.createRadialGradient(e, 0, 0, 0, 0, 1);
        b.setTransform && b.setTransform(f.toSVGMatrix());
        f = a.readUnsignedByte();
        for (e = 0;e < f;e++) {
          var m = a.readUnsignedByte() / 255, h = k.ColorUtilities.rgbaToCSSStyle(a.readUnsignedInt());
          b.addColorStop(m, h);
        }
        a.position += 2;
        return b;
      };
      c.prototype._readBitmap = function(a, c) {
        var b = a.readUnsignedInt(), e = this._readMatrix(a), f = a.readBoolean() ? "repeat" : "no-repeat", m = a.readBoolean();
        (b = this._textures[b]) ? (f = c.createPattern(b.renderSource, f), f.setTransform(e.toSVGMatrix())) : f = null;
        return {style:f, smoothImage:m};
      };
      c.prototype._renderFallback = function(a) {
        this.fillStyle || (this.fillStyle = k.ColorStyle.randomStyle());
        var c = this._bounds;
        a.save();
        a.beginPath();
        a.lineWidth = 2;
        a.fillStyle = this.fillStyle;
        a.fillRect(c.x, c.y, c.w, c.h);
        a.restore();
      };
      c.LINE_CAPS_STYLES = ["round", "butt", "square"];
      c.LINE_JOINTS_STYLES = ["round", "bevel", "miter"];
      return c;
    }(p);
    r.RenderableShape = t;
    l = function(e) {
      function c() {
        e.apply(this, arguments);
        this._flags = 7340048;
        this._morphPaths = Object.create(null);
      }
      __extends(c, e);
      c.prototype._deserializePaths = function(a, c, e) {
        if (this._morphPaths[e]) {
          return this._morphPaths[e];
        }
        var f = this._morphPaths[e] = [], h = null, l = null, p = 0, n = 0, w, x, r = !1, v = 0, Q = 0, V = a.commands, D = a.coordinates, z = a.morphCoordinates, A = a.styles, C = a.morphStyles;
        A.position = 0;
        var y = C.position = 0;
        a = a.commandsPosition;
        for (var F = 0;F < a;F++) {
          switch(V[F]) {
            case 9:
              r && h && (h.lineTo(v, Q), l && l.lineTo(v, Q));
              r = !0;
              p = v = g(D[y], z[y++], e) / 20;
              n = Q = g(D[y], z[y++], e) / 20;
              h && h.moveTo(p, n);
              l && l.moveTo(p, n);
              break;
            case 10:
              p = g(D[y], z[y++], e) / 20;
              n = g(D[y], z[y++], e) / 20;
              h && h.lineTo(p, n);
              l && l.lineTo(p, n);
              break;
            case 11:
              w = g(D[y], z[y++], e) / 20;
              x = g(D[y], z[y++], e) / 20;
              p = g(D[y], z[y++], e) / 20;
              n = g(D[y], z[y++], e) / 20;
              h && h.quadraticCurveTo(w, x, p, n);
              l && l.quadraticCurveTo(w, x, p, n);
              break;
            case 12:
              w = g(D[y], z[y++], e) / 20;
              x = g(D[y], z[y++], e) / 20;
              var B = g(D[y], z[y++], e) / 20, E = g(D[y], z[y++], e) / 20, p = g(D[y], z[y++], e) / 20, n = g(D[y], z[y++], e) / 20;
              h && h.bezierCurveTo(w, x, B, E, p, n);
              l && l.bezierCurveTo(w, x, B, E, p, n);
              break;
            case 1:
              h = this._createMorphPath(0, e, k.ColorUtilities.rgbaToCSSStyle(b(A.readUnsignedInt(), C.readUnsignedInt(), e)), !1, null, p, n);
              break;
            case 3:
              w = this._readMorphBitmap(A, C, e, c);
              h = this._createMorphPath(0, e, w.style, w.smoothImage, null, p, n);
              break;
            case 2:
              w = this._readMorphGradient(A, C, e, c);
              h = this._createMorphPath(0, e, w, !1, null, p, n);
              break;
            case 4:
              h = null;
              break;
            case 5:
              w = g(D[y], z[y++], e) / 20;
              l = k.ColorUtilities.rgbaToCSSStyle(b(A.readUnsignedInt(), C.readUnsignedInt(), e));
              A.position += 1;
              x = A.readByte();
              B = t.LINE_CAPS_STYLES[A.readByte()];
              E = t.LINE_JOINTS_STYLES[A.readByte()];
              w = new m(w, x, B, E, A.readByte());
              l = this._createMorphPath(1, e, l, !1, w, p, n);
              break;
            case 6:
              w = this._readMorphGradient(A, C, e, c);
              l = this._createMorphPath(2, e, w, !1, null, p, n);
              break;
            case 7:
              w = this._readMorphBitmap(A, C, e, c);
              l = this._createMorphPath(2, e, w.style, w.smoothImage, null, p, n);
              break;
            case 8:
              l = null;
          }
        }
        r && h && (h.lineTo(v, Q), l && l.lineTo(v, Q));
        return f;
      };
      c.prototype._createMorphPath = function(a, c, b, e, f, m, h) {
        a = new w(a, b, e, f);
        this._morphPaths[c].push(a);
        a.path.moveTo(m, h);
        return a.path;
      };
      c.prototype._readMorphMatrix = function(c, b, e) {
        return new a(g(c.readFloat(), b.readFloat(), e), g(c.readFloat(), b.readFloat(), e), g(c.readFloat(), b.readFloat(), e), g(c.readFloat(), b.readFloat(), e), g(c.readFloat(), b.readFloat(), e), g(c.readFloat(), b.readFloat(), e));
      };
      c.prototype._readMorphGradient = function(a, c, e, f) {
        var m = a.readUnsignedByte(), h = 2 * a.readShort() / 255, l = this._readMorphMatrix(a, c, e);
        f = 16 === m ? f.createLinearGradient(-1, 0, 1, 0) : f.createRadialGradient(h, 0, 0, 0, 0, 1);
        f.setTransform && f.setTransform(l.toSVGMatrix());
        l = a.readUnsignedByte();
        for (m = 0;m < l;m++) {
          var h = g(a.readUnsignedByte() / 255, c.readUnsignedByte() / 255, e), p = b(a.readUnsignedInt(), c.readUnsignedInt(), e), p = k.ColorUtilities.rgbaToCSSStyle(p);
          f.addColorStop(h, p);
        }
        a.position += 2;
        return f;
      };
      c.prototype._readMorphBitmap = function(a, c, b, e) {
        var f = a.readUnsignedInt();
        c = this._readMorphMatrix(a, c, b);
        b = a.readBoolean() ? "repeat" : "no-repeat";
        a = a.readBoolean();
        e = e.createPattern(this._textures[f]._canvas, b);
        e.setTransform(c.toSVGMatrix());
        return {style:e, smoothImage:a};
      };
      return c;
    }(t);
    r.RenderableMorphShape = l;
    var x = function() {
      function a() {
        this.align = this.leading = this.descent = this.ascent = this.width = this.y = this.x = 0;
        this.runs = [];
      }
      a._getMeasureContext = function() {
        a._measureContext || (a._measureContext = document.createElement("canvas").getContext("2d"));
        return a._measureContext;
      };
      a.prototype.addRun = function(c, d, b, m) {
        if (b) {
          var h = a._getMeasureContext();
          h.font = c;
          h = h.measureText(b).width | 0;
          this.runs.push(new e(c, d, b, h, m));
          this.width += h;
        }
      };
      a.prototype.wrap = function(c) {
        var d = [this], b = this.runs, m = this;
        m.width = 0;
        m.runs = [];
        for (var h = a._getMeasureContext(), g = 0;g < b.length;g++) {
          var l = b[g], p = l.text;
          l.text = "";
          l.width = 0;
          h.font = l.font;
          for (var k = c, t = p.split(/[\s.-]/), n = 0, w = 0;w < t.length;w++) {
            var x = t[w], r = p.substr(n, x.length + 1), v = h.measureText(r).width | 0;
            if (v > k) {
              do {
                if (l.text && (m.runs.push(l), m.width += l.width, l = new e(l.font, l.fillStyle, "", 0, l.underline), k = new a, k.y = m.y + m.descent + m.leading + m.ascent | 0, k.ascent = m.ascent, k.descent = m.descent, k.leading = m.leading, k.align = m.align, d.push(k), m = k), k = c - v, 0 > k) {
                  for (var D = r.length, z = r;1 < D && !(D--, z = r.substr(0, D), v = h.measureText(z).width | 0, v <= c);) {
                  }
                  l.text = z;
                  l.width = v;
                  r = r.substr(D);
                  v = h.measureText(r).width | 0;
                }
              } while (r && 0 > k);
            } else {
              k -= v;
            }
            l.text += r;
            l.width += v;
            n += x.length + 1;
          }
          m.runs.push(l);
          m.width += l.width;
        }
        return d;
      };
      return a;
    }();
    r.TextLine = x;
    var e = function() {
      return function(a, c, d, b, e) {
        void 0 === a && (a = "");
        void 0 === c && (c = "");
        void 0 === d && (d = "");
        void 0 === b && (b = 0);
        void 0 === e && (e = !1);
        this.font = a;
        this.fillStyle = c;
        this.text = d;
        this.width = b;
        this.underline = e;
      };
    }();
    r.TextRun = e;
    l = function(b) {
      function c(c) {
        b.call(this);
        this._flags = 1048592;
        this.properties = {};
        this._textBounds = c.clone();
        this._textRunData = null;
        this._plainText = "";
        this._borderColor = this._backgroundColor = 0;
        this._matrix = a.createIdentity();
        this._coords = null;
        this._scrollV = 1;
        this._scrollH = 0;
        this.textRect = c.clone();
        this.lines = [];
        this.setBounds(c);
      }
      __extends(c, b);
      c.prototype.setBounds = function(a) {
        b.prototype.setBounds.call(this, a);
        this._textBounds.set(a);
        this.textRect.setElements(a.x + 2, a.y + 2, a.w - 2, a.h - 2);
      };
      c.prototype.setContent = function(a, c, b, e) {
        this._textRunData = c;
        this._plainText = a;
        this._matrix.set(b);
        this._coords = e;
        this.lines = [];
      };
      c.prototype.setStyle = function(a, c, b, e) {
        this._backgroundColor = a;
        this._borderColor = c;
        this._scrollV = b;
        this._scrollH = e;
      };
      c.prototype.reflow = function(a, c) {
        var b = this._textRunData;
        if (b) {
          for (var e = this._bounds, f = e.w - 4, m = this._plainText, h = this.lines, g = new x, l = 0, p = 0, t = 0, n = 0, w = 0, r = -1;b.position < b.length;) {
            var v = b.readInt(), z = b.readInt(), A = b.readInt(), C = b.readUTF(), y = b.readInt(), F = b.readInt(), B = b.readInt();
            y > t && (t = y);
            F > n && (n = F);
            B > w && (w = B);
            y = b.readBoolean();
            F = "";
            b.readBoolean() && (F += "italic ");
            y && (F += "bold ");
            A = F + A + "px " + C;
            C = b.readInt();
            C = k.ColorUtilities.rgbToHex(C);
            y = b.readInt();
            -1 === r && (r = y);
            b.readBoolean();
            b.readInt();
            b.readInt();
            b.readInt();
            b.readInt();
            b.readInt();
            for (var y = b.readBoolean(), E = "", F = !1;!F;v++) {
              F = v >= z - 1;
              B = m[v];
              if ("\r" !== B && "\n" !== B && (E += B, v < m.length - 1)) {
                continue;
              }
              g.addRun(A, C, E, y);
              if (g.runs.length) {
                h.length && (l += w);
                l += t;
                g.y = l | 0;
                l += n;
                g.ascent = t;
                g.descent = n;
                g.leading = w;
                g.align = r;
                if (c && g.width > f) {
                  for (g = g.wrap(f), E = 0;E < g.length;E++) {
                    var G = g[E], l = G.y + G.descent + G.leading;
                    h.push(G);
                    G.width > p && (p = G.width);
                  }
                } else {
                  h.push(g), g.width > p && (p = g.width);
                }
                g = new x;
              } else {
                l += t + n + w;
              }
              E = "";
              if (F) {
                w = n = t = 0;
                r = -1;
                break;
              }
              "\r" === B && "\n" === m[v + 1] && v++;
            }
            g.addRun(A, C, E, y);
          }
          b = m[m.length - 1];
          "\r" !== b && "\n" !== b || h.push(g);
          b = this.textRect;
          b.w = p;
          b.h = l;
          if (a) {
            if (!c) {
              f = p;
              p = e.w;
              switch(a) {
                case 1:
                  b.x = p - (f + 4) >> 1;
                  break;
                case 3:
                  b.x = p - (f + 4);
              }
              this._textBounds.setElements(b.x - 2, b.y - 2, b.w + 4, b.h + 4);
              e.w = f + 4;
            }
            e.x = b.x - 2;
            e.h = l + 4;
          } else {
            this._textBounds = e;
          }
          for (v = 0;v < h.length;v++) {
            if (e = h[v], e.width < f) {
              switch(e.align) {
                case 1:
                  e.x = f - e.width | 0;
                  break;
                case 2:
                  e.x = (f - e.width) / 2 | 0;
              }
            }
          }
          this.invalidate();
        }
      };
      c.roundBoundPoints = function(a) {
        for (var c = 0;c < a.length;c++) {
          var b = a[c];
          b.x = Math.floor(b.x + .1) + .5;
          b.y = Math.floor(b.y + .1) + .5;
        }
      };
      c.prototype.render = function(d) {
        d.save();
        var b = this._textBounds;
        this._backgroundColor && (d.fillStyle = k.ColorUtilities.rgbaToCSSStyle(this._backgroundColor), d.fillRect(b.x, b.y, b.w, b.h));
        if (this._borderColor) {
          d.strokeStyle = k.ColorUtilities.rgbaToCSSStyle(this._borderColor);
          d.lineCap = "square";
          d.lineWidth = 1;
          var e = c.absoluteBoundPoints, f = d.currentTransform;
          f ? (b = b.clone(), (new a(f.a, f.b, f.c, f.d, f.e, f.f)).transformRectangle(b, e), d.setTransform(1, 0, 0, 1, 0, 0)) : (e[0].x = b.x, e[0].y = b.y, e[1].x = b.x + b.w, e[1].y = b.y, e[2].x = b.x + b.w, e[2].y = b.y + b.h, e[3].x = b.x, e[3].y = b.y + b.h);
          c.roundBoundPoints(e);
          b = new Path2D;
          b.moveTo(e[0].x, e[0].y);
          b.lineTo(e[1].x, e[1].y);
          b.lineTo(e[2].x, e[2].y);
          b.lineTo(e[3].x, e[3].y);
          b.lineTo(e[0].x, e[0].y);
          d.stroke(b);
          f && d.setTransform(f.a, f.b, f.c, f.d, f.e, f.f);
        }
        this._coords ? this._renderChars(d) : this._renderLines(d);
        d.restore();
      };
      c.prototype._renderChars = function(a) {
        if (this._matrix) {
          var c = this._matrix;
          a.transform(c.a, c.b, c.c, c.d, c.tx, c.ty);
        }
        for (var c = this.lines, b = this._coords, e = b.position = 0;e < c.length;e++) {
          for (var f = c[e].runs, m = 0;m < f.length;m++) {
            var h = f[m];
            a.font = h.font;
            a.fillStyle = h.fillStyle;
            for (var h = h.text, g = 0;g < h.length;g++) {
              var l = b.readInt() / 20, p = b.readInt() / 20;
              a.fillText(h[g], l, p);
            }
          }
        }
      };
      c.prototype._renderLines = function(a) {
        var c = this._textBounds;
        a.beginPath();
        a.rect(c.x + 2, c.y + 2, c.w - 4, c.h - 4);
        a.clip();
        a.translate(c.x - this._scrollH + 2, c.y + 2);
        for (var b = this.lines, e = this._scrollV, f = 0, m = 0;m < b.length;m++) {
          var h = b[m], g = h.x, l = h.y;
          if (m + 1 < e) {
            f = l + h.descent + h.leading;
          } else {
            l -= f;
            if (m + 1 - e && l > c.h) {
              break;
            }
            for (var p = h.runs, k = 0;k < p.length;k++) {
              var t = p[k];
              a.font = t.font;
              a.fillStyle = t.fillStyle;
              t.underline && a.fillRect(g, l + h.descent / 2 | 0, t.width, 1);
              a.textAlign = "left";
              a.textBaseline = "alphabetic";
              a.fillText(t.text, g, l);
              g += t.width;
            }
          }
        }
      };
      c.absoluteBoundPoints = [new v(0, 0), new v(0, 0), new v(0, 0), new v(0, 0)];
      return c;
    }(p);
    r.RenderableText = l;
    p = function(a) {
      function c(c, b) {
        a.call(this);
        this._flags = 3145728;
        this.properties = {};
        this.setBounds(new n(0, 0, c, b));
      }
      __extends(c, a);
      Object.defineProperty(c.prototype, "text", {get:function() {
        return this._text;
      }, set:function(a) {
        this._text = a;
      }, enumerable:!0, configurable:!0});
      c.prototype.render = function(a, c, b) {
        a.save();
        a.textBaseline = "top";
        a.fillStyle = "white";
        a.fillText(this.text, 0, 0);
        a.restore();
      };
      return c;
    }(p);
    r.Label = p;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = k.ColorUtilities.clampByte, b = function() {
      return function() {
      };
    }();
    r.Filter = b;
    var v = function(b) {
      function a(a, g, l) {
        b.call(this);
        this.blurX = a;
        this.blurY = g;
        this.quality = l;
      }
      __extends(a, b);
      return a;
    }(b);
    r.BlurFilter = v;
    v = function(b) {
      function a(a, g, l, k, m, t, x, e, f, c, d) {
        b.call(this);
        this.alpha = a;
        this.angle = g;
        this.blurX = l;
        this.blurY = k;
        this.color = m;
        this.distance = t;
        this.hideObject = x;
        this.inner = e;
        this.knockout = f;
        this.quality = c;
        this.strength = d;
      }
      __extends(a, b);
      return a;
    }(b);
    r.DropshadowFilter = v;
    b = function(b) {
      function a(a, g, l, k, m, t, x, e) {
        b.call(this);
        this.alpha = a;
        this.blurX = g;
        this.blurY = l;
        this.color = k;
        this.inner = m;
        this.knockout = t;
        this.quality = x;
        this.strength = e;
      }
      __extends(a, b);
      return a;
    }(b);
    r.GlowFilter = b;
    (function(b) {
      b[b.Unknown = 0] = "Unknown";
      b[b.Identity = 1] = "Identity";
    })(r.ColorMatrixType || (r.ColorMatrixType = {}));
    b = function() {
      function b(a) {
        this._data = new Float32Array(a);
        this._type = 0;
      }
      b.prototype.clone = function() {
        var a = new b(this._data);
        a._type = this._type;
        return a;
      };
      b.prototype.set = function(a) {
        this._data.set(a._data);
        this._type = a._type;
      };
      b.prototype.toWebGLMatrix = function() {
        return new Float32Array(this._data);
      };
      b.prototype.asWebGLMatrix = function() {
        return this._data.subarray(0, 16);
      };
      b.prototype.asWebGLVector = function() {
        return this._data.subarray(16, 20);
      };
      b.prototype.isIdentity = function() {
        if (this._type & 1) {
          return !0;
        }
        var a = this._data;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 1 == a[15] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      b.createIdentity = function() {
        var a = new b([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]);
        a._type = 1;
        return a;
      };
      b.prototype.setMultipliersAndOffsets = function(a, b, g, l, k, m, t, n) {
        for (var e = this._data, f = 0;f < e.length;f++) {
          e[f] = 0;
        }
        e[0] = a;
        e[5] = b;
        e[10] = g;
        e[15] = l;
        e[16] = k / 255;
        e[17] = m / 255;
        e[18] = t / 255;
        e[19] = n / 255;
        this._type = 0;
      };
      b.prototype.transformRGBA = function(a) {
        var b = a >> 24 & 255, p = a >> 16 & 255, l = a >> 8 & 255, k = a & 255, m = this._data;
        a = g(b * m[0] + p * m[1] + l * m[2] + k * m[3] + 255 * m[16]);
        var t = g(b * m[4] + p * m[5] + l * m[6] + k * m[7] + 255 * m[17]), n = g(b * m[8] + p * m[9] + l * m[10] + k * m[11] + 255 * m[18]), b = g(b * m[12] + p * m[13] + l * m[14] + k * m[15] + 255 * m[19]);
        return a << 24 | t << 16 | n << 8 | b;
      };
      b.prototype.multiply = function(a) {
        if (!(a._type & 1)) {
          var b = this._data, g = a._data;
          a = b[0];
          var l = b[1], k = b[2], m = b[3], t = b[4], n = b[5], e = b[6], f = b[7], c = b[8], d = b[9], q = b[10], u = b[11], r = b[12], H = b[13], v = b[14], R = b[15], ha = b[16], da = b[17], ia = b[18], ja = b[19], W = g[0], Q = g[1], V = g[2], D = g[3], z = g[4], A = g[5], C = g[6], y = g[7], F = g[8], B = g[9], E = g[10], G = g[11], J = g[12], K = g[13], L = g[14], M = g[15], N = g[16], O = g[17], P = g[18], g = g[19];
          b[0] = a * W + t * Q + c * V + r * D;
          b[1] = l * W + n * Q + d * V + H * D;
          b[2] = k * W + e * Q + q * V + v * D;
          b[3] = m * W + f * Q + u * V + R * D;
          b[4] = a * z + t * A + c * C + r * y;
          b[5] = l * z + n * A + d * C + H * y;
          b[6] = k * z + e * A + q * C + v * y;
          b[7] = m * z + f * A + u * C + R * y;
          b[8] = a * F + t * B + c * E + r * G;
          b[9] = l * F + n * B + d * E + H * G;
          b[10] = k * F + e * B + q * E + v * G;
          b[11] = m * F + f * B + u * E + R * G;
          b[12] = a * J + t * K + c * L + r * M;
          b[13] = l * J + n * K + d * L + H * M;
          b[14] = k * J + e * K + q * L + v * M;
          b[15] = m * J + f * K + u * L + R * M;
          b[16] = a * N + t * O + c * P + r * g + ha;
          b[17] = l * N + n * O + d * P + H * g + da;
          b[18] = k * N + e * O + q * P + v * g + ia;
          b[19] = m * N + f * O + u * P + R * g + ja;
          this._type = 0;
        }
      };
      Object.defineProperty(b.prototype, "alphaMultiplier", {get:function() {
        return this._data[15];
      }, enumerable:!0, configurable:!0});
      b.prototype.hasOnlyAlphaMultiplier = function() {
        var a = this._data;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      b.prototype.equals = function(a) {
        if (!a) {
          return !1;
        }
        if (this._type === a._type && 1 === this._type) {
          return !0;
        }
        var b = this._data;
        a = a._data;
        for (var g = 0;20 > g;g++) {
          if (.001 < Math.abs(b[g] - a[g])) {
            return !1;
          }
        }
        return !0;
      };
      b.prototype.toSVGFilterMatrix = function() {
        var a = this._data;
        return [a[0], a[4], a[8], a[12], a[16], a[1], a[5], a[9], a[13], a[17], a[2], a[6], a[10], a[14], a[18], a[3], a[7], a[11], a[15], a[19]].join(" ");
      };
      return b;
    }();
    r.ColorMatrix = b;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      function b(a, b) {
        return -1 !== a.indexOf(b, this.length - b.length);
      }
      var v = r.Geometry.Point3D, n = r.Geometry.Matrix3D, a = r.Geometry.degreesToRadian, h = k.Debug.unexpected, p = k.Debug.notImplemented;
      g.SHADER_ROOT = "shaders/";
      var l = function() {
        function l(a, b) {
          this._fillColor = k.Color.Red;
          this._surfaceRegionCache = new k.LRUList;
          this.modelViewProjectionMatrix = n.createIdentity();
          this._canvas = a;
          this._options = b;
          this.gl = a.getContext("experimental-webgl", {preserveDrawingBuffer:!1, antialias:!0, stencil:!0, premultipliedAlpha:!1});
          this._programCache = Object.create(null);
          this._resize();
          this.gl.pixelStorei(this.gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, b.unpackPremultiplyAlpha ? this.gl.ONE : this.gl.ZERO);
          this._backgroundColor = k.Color.Black;
          this._geometry = new g.WebGLGeometry(this);
          this._tmpVertices = g.Vertex.createEmptyVertices(g.Vertex, 64);
          this._maxSurfaces = b.maxSurfaces;
          this._maxSurfaceSize = b.maxSurfaceSize;
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
          var b = this.gl;
          switch(a) {
            case 8:
              b.blendFunc(b.SRC_ALPHA, b.DST_ALPHA);
              break;
            case 3:
              b.blendFunc(b.DST_COLOR, b.ONE_MINUS_SRC_ALPHA);
              break;
            case 4:
              b.blendFunc(b.SRC_ALPHA, b.ONE);
              break;
            case 2:
            ;
            case 1:
              b.blendFunc(b.ONE, b.ONE_MINUS_SRC_ALPHA);
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
              return !0;
            default:
              return !1;
          }
        };
        l.prototype.create2DProjectionMatrix = function() {
          return n.create2DProjection(this._w, this._h, -this._w);
        };
        l.prototype.createPerspectiveMatrix = function(b, h, g) {
          g = a(g);
          h = n.createPerspective(a(h));
          var e = new v(0, 1, 0), f = new v(0, 0, 0);
          b = new v(0, 0, b);
          b = n.createCameraLookAt(b, f, e);
          b = n.createInverse(b);
          e = n.createIdentity();
          e = n.createMultiply(e, n.createTranslation(-this._w / 2, -this._h / 2));
          e = n.createMultiply(e, n.createScale(1 / this._w, -1 / this._h, .01));
          e = n.createMultiply(e, n.createYRotation(g));
          e = n.createMultiply(e, b);
          return e = n.createMultiply(e, h);
        };
        l.prototype.discardCachedImages = function() {
          2 <= r.traceLevel && r.writer && r.writer.writeLn("Discard Cache");
          for (var a = this._surfaceRegionCache.count / 2 | 0, b = 0;b < a;b++) {
            var h = this._surfaceRegionCache.pop();
            2 <= r.traceLevel && r.writer && r.writer.writeLn("Discard: " + h);
            h.texture.atlas.remove(h.region);
            h.texture = null;
          }
        };
        l.prototype.cacheImage = function(a) {
          var b = this.allocateSurfaceRegion(a.width, a.height);
          2 <= r.traceLevel && r.writer && r.writer.writeLn("Uploading Image: @ " + b.region);
          this._surfaceRegionCache.use(b);
          this.updateSurfaceRegion(a, b);
          return b;
        };
        l.prototype.allocateSurfaceRegion = function(a, b) {
          return this._surfaceRegionAllocator.allocate(a, b, null);
        };
        l.prototype.updateSurfaceRegion = function(a, b) {
          var h = this.gl;
          h.bindTexture(h.TEXTURE_2D, b.surface.texture);
          h.texSubImage2D(h.TEXTURE_2D, 0, b.region.x, b.region.y, h.RGBA, h.UNSIGNED_BYTE, a);
        };
        l.prototype._resize = function() {
          var a = this.gl;
          this._w = this._canvas.width;
          this._h = this._canvas.height;
          a.viewport(0, 0, this._w, this._h);
          for (var b in this._programCache) {
            this._initializeProgram(this._programCache[b]);
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
          if (b(h, ".vert")) {
            h = l.VERTEX_SHADER;
          } else {
            if (b(h, ".frag")) {
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
          var b = this.gl, g = b.createProgram();
          a.forEach(function(a) {
            b.attachShader(g, a);
          });
          b.linkProgram(g);
          b.getProgramParameter(g, b.LINK_STATUS) || (h("Cannot link program: " + b.getProgramInfoLog(g)), b.deleteProgram(g));
          return g;
        };
        l.prototype._createShader = function(a, b) {
          var g = this.gl, e = g.createShader(a);
          g.shaderSource(e, b);
          g.compileShader(e);
          return g.getShaderParameter(e, g.COMPILE_STATUS) ? e : (h("Cannot compile shader: " + g.getShaderInfoLog(e)), g.deleteShader(e), null);
        };
        l.prototype._createTexture = function() {
          var a = this.gl, b = a.createTexture();
          a.bindTexture(a.TEXTURE_2D, b);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_S, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_T, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MIN_FILTER, a.LINEAR);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MAG_FILTER, a.LINEAR);
          a.texImage2D(a.TEXTURE_2D, 0, a.RGBA, 1024, 1024, 0, a.RGBA, a.UNSIGNED_BYTE, null);
          return b;
        };
        l.prototype._createFramebuffer = function(a) {
          var b = this.gl, h = b.createFramebuffer();
          b.bindFramebuffer(b.FRAMEBUFFER, h);
          b.framebufferTexture2D(b.FRAMEBUFFER, b.COLOR_ATTACHMENT0, b.TEXTURE_2D, a, 0);
          b.bindFramebuffer(b.FRAMEBUFFER, null);
          return h;
        };
        l.prototype._queryProgramAttributesAndUniforms = function(a) {
          a.uniforms = {};
          a.attributes = {};
          for (var b = this.gl, h = 0, e = b.getProgramParameter(a, b.ACTIVE_ATTRIBUTES);h < e;h++) {
            var f = b.getActiveAttrib(a, h);
            a.attributes[f.name] = f;
            f.location = b.getAttribLocation(a, f.name);
          }
          h = 0;
          for (e = b.getProgramParameter(a, b.ACTIVE_UNIFORMS);h < e;h++) {
            f = b.getActiveUniform(a, h), a.uniforms[f.name] = f, f.location = b.getUniformLocation(a, f.name);
          }
        };
        Object.defineProperty(l.prototype, "target", {set:function(a) {
          var b = this.gl;
          a ? (b.viewport(0, 0, a.w, a.h), b.bindFramebuffer(b.FRAMEBUFFER, a.framebuffer)) : (b.viewport(0, 0, this._w, this._h), b.bindFramebuffer(b.FRAMEBUFFER, null));
        }, enumerable:!0, configurable:!0});
        l.prototype.clear = function(a) {
          a = this.gl;
          a.clearColor(0, 0, 0, 0);
          a.clear(a.COLOR_BUFFER_BIT);
        };
        l.prototype.clearTextureRegion = function(a, b) {
          void 0 === b && (b = k.Color.None);
          var h = this.gl, e = a.region;
          this.target = a.surface;
          h.enable(h.SCISSOR_TEST);
          h.scissor(e.x, e.y, e.w, e.h);
          h.clearColor(b.r, b.g, b.b, b.a);
          h.clear(h.COLOR_BUFFER_BIT | h.DEPTH_BUFFER_BIT);
          h.disable(h.SCISSOR_TEST);
        };
        l.prototype.sizeOf = function(a) {
          var b = this.gl;
          switch(a) {
            case b.UNSIGNED_BYTE:
              return 1;
            case b.UNSIGNED_SHORT:
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
  for (var b in r) {
    r.hasOwnProperty(b) && (k[b] = r[b]);
  }
  g.prototype = r.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.Debug.assert, v = function(a) {
        function h() {
          a.apply(this, arguments);
        }
        __extends(h, a);
        h.prototype.ensureVertexCapacity = function(a) {
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8 * a);
        };
        h.prototype.writeVertex = function(a, h) {
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8);
          this.writeVertexUnsafe(a, h);
        };
        h.prototype.writeVertexUnsafe = function(a, b) {
          var h = this._offset >> 2;
          this._f32[h] = a;
          this._f32[h + 1] = b;
          this._offset += 8;
        };
        h.prototype.writeVertex3D = function(a, h, g) {
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 12);
          this.writeVertex3DUnsafe(a, h, g);
        };
        h.prototype.writeVertex3DUnsafe = function(a, b, h) {
          var m = this._offset >> 2;
          this._f32[m] = a;
          this._f32[m + 1] = b;
          this._f32[m + 2] = h;
          this._offset += 12;
        };
        h.prototype.writeTriangleElements = function(a, h, g) {
          b(0 === (this._offset & 1));
          this.ensureCapacity(this._offset + 6);
          var m = this._offset >> 1;
          this._u16[m] = a;
          this._u16[m + 1] = h;
          this._u16[m + 2] = g;
          this._offset += 6;
        };
        h.prototype.ensureColorCapacity = function(a) {
          b(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16 * a);
        };
        h.prototype.writeColorFloats = function(a, h, g, m) {
          b(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16);
          this.writeColorFloatsUnsafe(a, h, g, m);
        };
        h.prototype.writeColorFloatsUnsafe = function(a, b, h, m) {
          var g = this._offset >> 2;
          this._f32[g] = a;
          this._f32[g + 1] = b;
          this._f32[g + 2] = h;
          this._f32[g + 3] = m;
          this._offset += 16;
        };
        h.prototype.writeColor = function() {
          var a = Math.random(), h = Math.random(), g = Math.random(), m = Math.random() / 2;
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 4);
          this._i32[this._offset >> 2] = m << 24 | g << 16 | h << 8 | a;
          this._offset += 4;
        };
        h.prototype.writeColorUnsafe = function(a, b, h, m) {
          this._i32[this._offset >> 2] = m << 24 | h << 16 | b << 8 | a;
          this._offset += 4;
        };
        h.prototype.writeRandomColor = function() {
          this.writeColor();
        };
        return h;
      }(k.ArrayUtilities.ArrayWriter);
      g.BufferWriter = v;
      g.WebGLAttribute = function() {
        return function(a, b, g, l) {
          void 0 === l && (l = !1);
          this.name = a;
          this.size = b;
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
          for (var b = 0, g = 0;g < this.attributes.length;g++) {
            this.attributes[g].offset = b, b += a.sizeOf(this.attributes[g].type) * this.attributes[g].size;
          }
          this.size = b;
        };
        return a;
      }();
      g.WebGLAttributeList = n;
      n = function() {
        function a(a) {
          this._elementOffset = this.triangleCount = 0;
          this.context = a;
          this.array = new v(8);
          this.buffer = a.gl.createBuffer();
          this.elementArray = new v(8);
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
        function b(h, g, k) {
          a.call(this, h, g, k);
        }
        __extends(b, a);
        b.createEmptyVertices = function(a, b) {
          for (var h = [], g = 0;g < b;g++) {
            h.push(new a(0, 0, 0));
          }
          return h;
        };
        return b;
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
      var b = function() {
        function b(a, h, g) {
          this.texture = g;
          this.w = a;
          this.h = h;
          this._regionAllocator = new k.RegionAllocator.CompactAllocator(this.w, this.h);
        }
        b.prototype.allocate = function(a, b) {
          var g = this._regionAllocator.allocate(a, b);
          return g ? new v(this, g) : null;
        };
        b.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return b;
      }();
      g.WebGLSurface = b;
      var v = function() {
        return function(b, a) {
          this.surface = b;
          this.region = a;
          this.next = this.previous = null;
        };
      }();
      g.WebGLSurfaceRegion = v;
    })(k.WebGL || (k.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.Color;
      g.TILE_SIZE = 256;
      g.MIN_UNTILED_SIZE = 256;
      var v = r.Geometry.Matrix, n = r.Geometry.Rectangle, a = function(a) {
        function b() {
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
        __extends(b, a);
        return b;
      }(r.RendererOptions);
      g.WebGLRendererOptions = a;
      var h = function(h) {
        function l(b, m, l) {
          void 0 === l && (l = new a);
          h.call(this, b, m, l);
          this._tmpVertices = g.Vertex.createEmptyVertices(g.Vertex, 64);
          this._cachedTiles = [];
          b = this._context = new g.WebGLContext(this._canvas, l);
          this._updateSize();
          this._brush = new g.WebGLCombinedBrush(b, new g.WebGLGeometry(b));
          this._stencilBrush = new g.WebGLCombinedBrush(b, new g.WebGLGeometry(b));
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
        l.prototype._cacheImageCallback = function(a, b, h) {
          var g = h.w, e = h.h, f = h.x;
          h = h.y;
          this._uploadCanvas.width = g + 2;
          this._uploadCanvas.height = e + 2;
          this._uploadCanvasContext.drawImage(b.canvas, f, h, g, e, 1, 1, g, e);
          this._uploadCanvasContext.drawImage(b.canvas, f, h, g, 1, 1, 0, g, 1);
          this._uploadCanvasContext.drawImage(b.canvas, f, h + e - 1, g, 1, 1, e + 1, g, 1);
          this._uploadCanvasContext.drawImage(b.canvas, f, h, 1, e, 0, 1, 1, e);
          this._uploadCanvasContext.drawImage(b.canvas, f + g - 1, h, 1, e, g + 1, 1, 1, e);
          return a && a.surface ? (this._options.disableSurfaceUploads || this._context.updateSurfaceRegion(this._uploadCanvas, a), a) : this._context.cacheImage(this._uploadCanvas);
        };
        l.prototype._enterClip = function(a, b, h, g) {
          h.flush();
          b = this._context.gl;
          0 === this._clipStack.length && (b.enable(b.STENCIL_TEST), b.clear(b.STENCIL_BUFFER_BIT), b.stencilFunc(b.ALWAYS, 1, 1));
          this._clipStack.push(a);
          b.colorMask(!1, !1, !1, !1);
          b.stencilOp(b.KEEP, b.KEEP, b.INCR);
          h.flush();
          b.colorMask(!0, !0, !0, !0);
          b.stencilFunc(b.NOTEQUAL, 0, this._clipStack.length);
          b.stencilOp(b.KEEP, b.KEEP, b.KEEP);
        };
        l.prototype._leaveClip = function(a, b, h, g) {
          h.flush();
          b = this._context.gl;
          if (a = this._clipStack.pop()) {
            b.colorMask(!1, !1, !1, !1), b.stencilOp(b.KEEP, b.KEEP, b.DECR), h.flush(), b.colorMask(!0, !0, !0, !0), b.stencilFunc(b.NOTEQUAL, 0, this._clipStack.length), b.stencilOp(b.KEEP, b.KEEP, b.KEEP);
          }
          0 === this._clipStack.length && b.disable(b.STENCIL_TEST);
        };
        l.prototype._renderFrame = function(a, b, h, g) {
        };
        l.prototype._renderSurfaces = function(a) {
          var h = this._options, l = this._context, k = this._viewport;
          if (h.drawSurfaces) {
            var e = l.surfaces, l = v.createIdentity();
            if (0 <= h.drawSurface && h.drawSurface < e.length) {
              for (var h = e[h.drawSurface | 0], e = new n(0, 0, h.w, h.h), f = e.clone();f.w > k.w;) {
                f.scale(.5, .5);
              }
              a.drawImage(new g.WebGLSurfaceRegion(h, e), f, b.White, null, l, .2);
            } else {
              f = k.w / 5;
              f > k.h / e.length && (f = k.h / e.length);
              a.fillRectangle(new n(k.w - f, 0, f, k.h), new b(0, 0, 0, .5), l, .1);
              for (var c = 0;c < e.length;c++) {
                var h = e[c], d = new n(k.w - f, c * f, f, f);
                a.drawImage(new g.WebGLSurfaceRegion(h, new n(0, 0, h.w, h.h)), d, b.White, null, l, .2);
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
          a.paintViewport && (g.fillRectangle(h, new b(.5, 0, 0, .25), v.createIdentity(), 0), g.flush());
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
      var b = k.Color, v = r.Geometry.Point, n = r.Geometry.Matrix3D, a = function() {
        function a(b, h, g) {
          this._target = g;
          this._context = b;
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
          this.color = new b(0, 0, 0, 0);
          this.sampler = 0;
          this.coordinate = new v(0, 0);
        }
        __extends(h, a);
        h.initializeAttributeList = function(a) {
          var b = a.gl;
          h.attributeList || (h.attributeList = new g.WebGLAttributeList([new g.WebGLAttribute("aPosition", 3, b.FLOAT), new g.WebGLAttribute("aCoordinate", 2, b.FLOAT), new g.WebGLAttribute("aColor", 4, b.UNSIGNED_BYTE, !0), new g.WebGLAttribute("aKind", 1, b.FLOAT), new g.WebGLAttribute("aSampler", 1, b.FLOAT)]), h.attributeList.initialize(a));
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
        function b(g, m, l) {
          void 0 === l && (l = null);
          a.call(this, g, m, l);
          this._blendMode = 1;
          this._program = g.createProgramFromFiles();
          this._surfaces = [];
          h.initializeAttributeList(this._context);
        }
        __extends(b, a);
        b.prototype.reset = function() {
          this._surfaces = [];
          this._geometry.reset();
        };
        b.prototype.drawImage = function(a, h, g, k, e, f, c) {
          void 0 === f && (f = 0);
          void 0 === c && (c = 1);
          if (!a || !a.surface) {
            return !0;
          }
          h = h.clone();
          this._colorMatrix && (k && this._colorMatrix.equals(k) || this.flush());
          this._colorMatrix = k;
          this._blendMode !== c && (this.flush(), this._blendMode = c);
          c = this._surfaces.indexOf(a.surface);
          0 > c && (8 === this._surfaces.length && this.flush(), this._surfaces.push(a.surface), c = this._surfaces.length - 1);
          var d = b._tmpVertices, q = a.region.clone();
          q.offset(1, 1).resize(-2, -2);
          q.scale(1 / a.surface.w, 1 / a.surface.h);
          e.transformRectangle(h, d);
          for (a = 0;4 > a;a++) {
            d[a].z = f;
          }
          d[0].coordinate.x = q.x;
          d[0].coordinate.y = q.y;
          d[1].coordinate.x = q.x + q.w;
          d[1].coordinate.y = q.y;
          d[2].coordinate.x = q.x + q.w;
          d[2].coordinate.y = q.y + q.h;
          d[3].coordinate.x = q.x;
          d[3].coordinate.y = q.y + q.h;
          for (a = 0;4 > a;a++) {
            f = b._tmpVertices[a], f.kind = k ? 2 : 1, f.color.set(g), f.sampler = c, f.writeTo(this._geometry);
          }
          this._geometry.addQuad();
          return !0;
        };
        b.prototype.fillRectangle = function(a, h, g, k) {
          void 0 === k && (k = 0);
          g.transformRectangle(a, b._tmpVertices);
          for (a = 0;4 > a;a++) {
            g = b._tmpVertices[a], g.kind = 0, g.color.set(h), g.z = k, g.writeTo(this._geometry);
          }
          this._geometry.addQuad();
        };
        b.prototype.flush = function() {
          var a = this._geometry, b = this._program, g = this._context.gl, l;
          a.uploadBuffers();
          g.useProgram(b);
          this._target ? (l = n.create2DProjection(this._target.w, this._target.h, 2E3), l = n.createMultiply(l, n.createScale(1, -1, 1))) : l = this._context.modelViewProjectionMatrix;
          g.uniformMatrix4fv(b.uniforms.uTransformMatrix3D.location, !1, l.asWebGLMatrix());
          this._colorMatrix && (g.uniformMatrix4fv(b.uniforms.uColorMatrix.location, !1, this._colorMatrix.asWebGLMatrix()), g.uniform4fv(b.uniforms.uColorVector.location, this._colorMatrix.asWebGLVector()));
          for (l = 0;l < this._surfaces.length;l++) {
            g.activeTexture(g.TEXTURE0 + l), g.bindTexture(g.TEXTURE_2D, this._surfaces[l].texture);
          }
          g.uniform1iv(b.uniforms["uSampler[0]"].location, [0, 1, 2, 3, 4, 5, 6, 7]);
          g.bindBuffer(g.ARRAY_BUFFER, a.buffer);
          var e = h.attributeList.size, f = h.attributeList.attributes;
          for (l = 0;l < f.length;l++) {
            var c = f[l], d = b.attributes[c.name].location;
            g.enableVertexAttribArray(d);
            g.vertexAttribPointer(d, c.size, c.type, c.normalized, e, c.offset);
          }
          this._context.setBlendOptions();
          this._context.target = this._target;
          g.bindBuffer(g.ELEMENT_ARRAY_BUFFER, a.elementBuffer);
          g.drawElements(g.TRIANGLES, 3 * a.triangleCount, g.UNSIGNED_SHORT, 0);
          this.reset();
        };
        b._tmpVertices = g.Vertex.createEmptyVertices(h, 4);
        b._depth = 1;
        return b;
      }(a);
      g.WebGLCombinedBrush = a;
    })(r.WebGL || (r.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var b = CanvasRenderingContext2D.prototype.save, k = CanvasRenderingContext2D.prototype.clip, n = CanvasRenderingContext2D.prototype.fill, a = CanvasRenderingContext2D.prototype.stroke, h = CanvasRenderingContext2D.prototype.restore, p = CanvasRenderingContext2D.prototype.beginPath;
      g.notifyReleaseChanged = function() {
        CanvasRenderingContext2D.prototype.save = b;
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
      function b(a) {
        var b = "source-over";
        switch(a) {
          case 1:
          ;
          case 2:
            break;
          case 3:
            b = "multiply";
            break;
          case 8:
          ;
          case 4:
            b = "screen";
            break;
          case 5:
            b = "lighten";
            break;
          case 6:
            b = "darken";
            break;
          case 7:
            b = "difference";
            break;
          case 13:
            b = "overlay";
            break;
          case 14:
            b = "hard-light";
            break;
          case 11:
            b = "destination-in";
            break;
          case 12:
            b = "destination-out";
            break;
          default:
            k.Debug.somewhatImplemented("Blend Mode: " + r.BlendMode[a]);
        }
        return b;
      }
      var v = k.NumberUtilities.clamp;
      navigator.userAgent.indexOf("Firefox");
      var n = function() {
        function a() {
        }
        a._prepareSVGFilters = function() {
          if (!a._svgBlurFilter) {
            var b = document.createElementNS("http://www.w3.org/2000/svg", "svg"), g = document.createElementNS("http://www.w3.org/2000/svg", "defs"), k = document.createElementNS("http://www.w3.org/2000/svg", "filter");
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
            b.appendChild(g);
            document.documentElement.appendChild(b);
          }
        };
        a._applyColorMatrixFilter = function(b, g) {
          a._prepareSVGFilters();
          a._svgColorMatrixFilter.setAttribute("values", g.toSVGFilterMatrix());
          b.filter = "url(#svgColorMatrixFilter)";
        };
        a._applyFilters = function(b, g, n) {
          function m(a) {
            var c = b / 2;
            switch(a) {
              case 0:
                return 0;
              case 1:
                return c / 2.7;
              case 2:
                return c / 1.28;
              default:
                return c;
            }
          }
          a._prepareSVGFilters();
          a._removeFilters(g);
          for (var t = 0;t < n.length;t++) {
            var x = n[t];
            if (x instanceof r.BlurFilter) {
              var e = x, x = m(e.quality);
              a._svgBlurFilter.setAttribute("stdDeviation", e.blurX * x + " " + e.blurY * x);
              g.filter = "url(#svgBlurFilter)";
            } else {
              x instanceof r.DropshadowFilter && (e = x, x = m(e.quality), a._svgDropshadowFilterBlur.setAttribute("stdDeviation", e.blurX * x + " " + e.blurY * x), a._svgDropshadowFilterOffset.setAttribute("dx", String(Math.cos(e.angle * Math.PI / 180) * e.distance * b)), a._svgDropshadowFilterOffset.setAttribute("dy", String(Math.sin(e.angle * Math.PI / 180) * e.distance * b)), a._svgDropshadowFilterFlood.setAttribute("flood-color", k.ColorUtilities.rgbaToCSSStyle(e.color << 8 | Math.round(255 * 
              e.alpha))), g.filter = "url(#svgDropShadowFilter)");
            }
          }
        };
        a._removeFilters = function(a) {
          a.filter = "none";
        };
        a._applyColorMatrix = function(b, g) {
          a._removeFilters(b);
          g.isIdentity() ? (b.globalAlpha = 1, b.globalColorMatrix = null) : g.hasOnlyAlphaMultiplier() ? (b.globalAlpha = v(g.alphaMultiplier, 0, 1), b.globalColorMatrix = null) : (b.globalAlpha = 1, a._svgFiltersAreSupported ? (a._applyColorMatrixFilter(b, g), b.globalColorMatrix = null) : b.globalColorMatrix = g);
        };
        a._svgFiltersAreSupported = !!Object.getOwnPropertyDescriptor(CanvasRenderingContext2D.prototype, "filter");
        return a;
      }();
      g.Filters = n;
      var a = function() {
        function a(b, g, h, m) {
          this.surface = b;
          this.region = g;
          this.w = h;
          this.h = m;
        }
        a.prototype.free = function() {
          this.surface.free(this);
        };
        a._ensureCopyCanvasSize = function(b, g) {
          var n;
          if (a._copyCanvasContext) {
            if (n = a._copyCanvasContext.canvas, n.width < b || n.height < g) {
              n.width = k.IntegerUtilities.nearestPowerOfTwo(b), n.height = k.IntegerUtilities.nearestPowerOfTwo(g);
            }
          } else {
            n = document.createElement("canvas"), "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(n), n.width = 512, n.height = 512, a._copyCanvasContext = n.getContext("2d");
          }
        };
        a.prototype.draw = function(g, l, k, m, n, r) {
          this.context.setTransform(1, 0, 0, 1, 0, 0);
          var e, f = 0, c = 0;
          g.context.canvas === this.context.canvas ? (a._ensureCopyCanvasSize(m, n), e = a._copyCanvasContext, e.clearRect(0, 0, m, n), e.drawImage(g.surface.canvas, g.region.x, g.region.y, m, n, 0, 0, m, n), e = e.canvas, c = f = 0) : (e = g.surface.canvas, f = g.region.x, c = g.region.y);
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
          this.context.globalCompositeOperation = b(r);
          this.context.drawImage(e, f, c, m, n, l, k, m, n);
          this.context.globalCompositeOperation = b(1);
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
          a.globalCompositeOperation = b(1);
        };
        a.prototype.fill = function(a) {
          var b = this.surface.context, g = this.region;
          b.fillStyle = a;
          b.fillRect(g.x, g.y, g.w, g.h);
        };
        a.prototype.clear = function(a) {
          var b = this.surface.context, g = this.region;
          a || (a = g);
          b.clearRect(a.x, a.y, a.w, a.h);
        };
        return a;
      }();
      g.Canvas2DSurfaceRegion = a;
      n = function() {
        function b(a, g) {
          this.canvas = a;
          this.context = a.getContext("2d");
          this.w = a.width;
          this.h = a.height;
          this._regionAllocator = g;
        }
        b.prototype.allocate = function(b, g) {
          var h = this._regionAllocator.allocate(b, g);
          return h ? new a(this, h, b, g) : null;
        };
        b.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return b;
      }();
      g.Canvas2DSurface = n;
    })(r.Canvas2D || (r.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.Debug.assert, v = k.GFX.Geometry.Rectangle, n = k.GFX.Geometry.Point, a = k.GFX.Geometry.Matrix, h = k.NumberUtilities.clamp, p = k.NumberUtilities.pow2, l = new k.IndentingWriter(!1, dumpLine), w = function() {
        return function(a, b) {
          this.surfaceRegion = a;
          this.scale = b;
        };
      }();
      g.MipMapLevel = w;
      var m = function() {
        function a(b, c, e, f) {
          this._node = c;
          this._levels = [];
          this._surfaceRegionAllocator = e;
          this._size = f;
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
            var e = this._node.getBounds().clone();
            e.scale(a, a);
            e.snap();
            var f = this._surfaceRegionAllocator.allocate(e.w, e.h, null), g = f.region, b = this._levels[c] = new w(f, a), c = new x(f);
            c.clip.set(g);
            c.matrix.setElements(a, 0, 0, a, g.x - e.x, g.y - e.y);
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
      var t = function(a) {
        function b() {
          a.apply(this, arguments);
          this.blending = this.imageSmoothing = this.snapToDevicePixels = !0;
          this.debugLayers = !1;
          this.filters = this.masking = !0;
          this.cacheShapes = !1;
          this.cacheShapesMaxSize = 256;
          this.cacheShapesThreshold = 16;
          this.alpha = !1;
        }
        __extends(b, a);
        return b;
      }(r.RendererOptions);
      g.Canvas2DRendererOptions = t;
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
      v.createMaxI16();
      var x = function(b) {
        function d(e) {
          b.call(this);
          this.clip = v.createEmpty();
          this.clipList = [];
          this.clipPath = null;
          this.flags = 0;
          this.target = null;
          this.matrix = a.createIdentity();
          this.colorMatrix = r.ColorMatrix.createIdentity();
          d.allocationCount++;
          this.target = e;
        }
        __extends(d, b);
        d.prototype.set = function(a) {
          this.clip.set(a.clip);
          this.clipPath = a.clipPath;
          this.target = a.target;
          this.matrix.set(a.matrix);
          this.colorMatrix.set(a.colorMatrix);
          this.flags = a.flags;
          k.ArrayUtilities.copyFrom(this.clipList, a.clipList);
        };
        d.prototype.clone = function() {
          var a = d.allocate();
          a || (a = new d(this.target));
          a.set(this);
          return a;
        };
        d.allocate = function() {
          var a = d._dirtyStack, b = null;
          a.length && (b = a.pop());
          return b;
        };
        d.prototype.free = function() {
          this.clipPath = null;
          d._dirtyStack.push(this);
        };
        d.prototype.transform = function(a) {
          var b = this.clone();
          b.matrix.preMultiply(a.getMatrix());
          a.hasColorMatrix() && b.colorMatrix.multiply(a.getColorMatrix());
          return b;
        };
        d.prototype.hasFlags = function(a) {
          return (this.flags & a) === a;
        };
        d.prototype.removeFlags = function(a) {
          this.flags &= ~a;
        };
        d.prototype.toggleFlags = function(a, b) {
          this.flags = b ? this.flags | a : this.flags & ~a;
        };
        d.prototype.beginClipPath = function(a) {
          this.target.context.save();
          this.clipPath = new Path2D;
        };
        d.prototype.applyClipPath = function() {
          var a = this.target.context;
          a.setTransform(1, 0, 0, 1, 0, 0);
          a.clip(this.clipPath);
          var b = this.matrix;
          a.setTransform(b.a, b.b, b.c, b.d, b.tx, b.ty);
        };
        d.prototype.closeClipPath = function() {
          this.target.context.restore();
        };
        d.allocationCount = 0;
        d._dirtyStack = [];
        return d;
      }(r.State);
      g.RenderState = x;
      var e = function() {
        function b() {
          this.culledNodes = this.groups = this.shapes = this._count = 0;
        }
        b.prototype.enter = function(a) {
          this._count++;
          l && (l.enter("> Frame: " + this._count), this._enterTime = performance.now(), this.culledNodes = this.groups = this.shapes = 0);
        };
        b.prototype.leave = function() {
          l && (l.writeLn("Shapes: " + this.shapes + ", Groups: " + this.groups + ", Culled Nodes: " + this.culledNodes), l.writeLn("Elapsed: " + (performance.now() - this._enterTime).toFixed(2)), l.writeLn("Rectangle: " + v.allocationCount + ", Matrix: " + a.allocationCount + ", State: " + x.allocationCount), l.leave("<"));
        };
        return b;
      }();
      g.FrameInfo = e;
      var f = function(c) {
        function d(a, b, f) {
          void 0 === f && (f = new t);
          c.call(this, a, b, f);
          this._visited = 0;
          this._frameInfo = new e;
          this._fontSize = 0;
          this._layers = [];
          if (a instanceof HTMLCanvasElement) {
            var g = a;
            this._viewport = new v(0, 0, g.width, g.height);
            this._target = this._createTarget(g);
          } else {
            this._addLayer("Background Layer");
            f = this._addLayer("Canvas Layer");
            g = document.createElement("canvas");
            f.appendChild(g);
            this._viewport = new v(0, 0, a.scrollWidth, a.scrollHeight);
            var h = this;
            b.addEventListener(1, function() {
              h._onStageBoundsChanged(g);
            });
            this._onStageBoundsChanged(g);
          }
          d._prepareSurfaceAllocators();
        }
        __extends(d, c);
        d.prototype._addLayer = function(a) {
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
        Object.defineProperty(d.prototype, "_backgroundVideoLayer", {get:function() {
          return this._layers[0];
        }, enumerable:!0, configurable:!0});
        d.prototype._createTarget = function(a) {
          return new g.Canvas2DSurfaceRegion(new g.Canvas2DSurface(a), new r.RegionAllocator.Region(0, 0, a.width, a.height), a.width, a.height);
        };
        d.prototype._onStageBoundsChanged = function(a) {
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
        d._prepareSurfaceAllocators = function() {
          d._initializedCaches || (d._surfaceCache = new r.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            var d = Math.max(1024, a), e = Math.max(1024, b);
            c.width = d;
            c.height = e;
            var f = null, f = 512 <= a || 512 <= b ? new r.RegionAllocator.GridAllocator(d, e, d, e) : new r.RegionAllocator.BucketAllocator(d, e);
            return new g.Canvas2DSurface(c, f);
          }), d._shapeCache = new r.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            c.width = 1024;
            c.height = 1024;
            var d = d = new r.RegionAllocator.CompactAllocator(1024, 1024);
            return new g.Canvas2DSurface(c, d);
          }), d._initializedCaches = !0);
        };
        d.prototype.render = function() {
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
        d.prototype.renderNode = function(a, b, c) {
          var d = new x(this._target);
          d.clip.set(b);
          d.flags = 256;
          d.matrix.set(c);
          a.visit(this, d);
          d.free();
        };
        d.prototype.renderNodeWithState = function(a, b) {
          a.visit(this, b);
        };
        d.prototype._renderWithCache = function(a, b) {
          var c = b.matrix, e = a.getBounds();
          if (e.isEmpty()) {
            return !1;
          }
          var f = this._options.cacheShapesMaxSize, g = Math.max(c.getAbsoluteScaleX(), c.getAbsoluteScaleY()), h = !!(b.flags & 16), l = !!(b.flags & 8);
          if (b.hasFlags(256)) {
            if (l || h || !b.colorMatrix.isIdentity() || a.hasFlags(1048576) || 100 < this._options.cacheShapesThreshold || e.w * g > f || e.h * g > f) {
              return !1;
            }
            (g = a.properties.mipMap) || (g = a.properties.mipMap = new m(this, a, d._shapeCache, f));
            h = g.getLevel(c);
            f = h.surfaceRegion;
            g = f.region;
            return h ? (h = b.target.context, h.imageSmoothingEnabled = h.mozImageSmoothingEnabled = !0, h.setTransform(c.a, c.b, c.c, c.d, c.tx, c.ty), h.drawImage(f.surface.canvas, g.x, g.y, g.w, g.h, e.x, e.y, e.w, e.h), !0) : !1;
          }
        };
        d.prototype._intersectsClipList = function(a, b) {
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
        d.prototype.visitGroup = function(a, b) {
          this._frameInfo.groups++;
          a.getBounds();
          if ((!a.hasFlags(4) || b.flags & 4) && a.hasFlags(65536)) {
            if (b.flags & 1 || 1 === a.getLayer().blendMode && !a.getLayer().mask || !this._options.blending) {
              if (this._intersectsClipList(a, b)) {
                for (var c = null, d = a.getChildren(), e = 0;e < d.length;e++) {
                  var f = d[e], g = f.getTransform(), h = b.transform(g);
                  h.toggleFlags(4096, f.hasFlags(524288));
                  if (0 <= f.clip) {
                    c = c || new Uint8Array(d.length);
                    c[f.clip + e]++;
                    var m = h.clone();
                    m.flags |= 16;
                    m.beginClipPath(g.getMatrix());
                    f.visit(this, m);
                    m.applyClipPath();
                    m.free();
                  } else {
                    f.visit(this, h);
                  }
                  if (c && 0 < c[e]) {
                    for (;c[e]--;) {
                      b.closeClipPath();
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
        d.prototype._renderDebugInfo = function(a, b) {
          if (b.flags & 1024) {
            var c = b.target.context, e = a.getBounds(!0), f = a.properties.style;
            f || (f = a.properties.style = k.Color.randomColor().toCSSStyle());
            c.strokeStyle = f;
            b.matrix.transformRectangleAABB(e);
            c.setTransform(1, 0, 0, 1, 0, 0);
            e.free();
            e = a.getBounds();
            f = d._debugPoints;
            b.matrix.transformRectangle(e, f);
            c.lineWidth = 1;
            c.beginPath();
            c.moveTo(f[0].x, f[0].y);
            c.lineTo(f[1].x, f[1].y);
            c.lineTo(f[2].x, f[2].y);
            c.lineTo(f[3].x, f[3].y);
            c.lineTo(f[0].x, f[0].y);
            c.stroke();
          }
        };
        d.prototype.visitStage = function(a, b) {
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
        d.prototype.visitShape = function(a, b) {
          if (this._intersectsClipList(a, b)) {
            var c = b.matrix;
            b.flags & 8192 && (c = c.clone(), c.snap());
            var d = b.target.context;
            g.Filters._applyColorMatrix(d, b.colorMatrix);
            a.source instanceof r.RenderableVideo ? this.visitRenderableVideo(a.source, b) : 0 < d.globalAlpha && this.visitRenderable(a.source, b, a.ratio);
            b.flags & 8192 && c.free();
          }
        };
        d.prototype.visitRenderableVideo = function(a, b) {
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
            f !== a.video.parentElement && (f.appendChild(a.video), a.addEventListener(2, function da(a) {
              f.removeChild(a.video);
              a.removeEventListener(2, da);
            }));
            d.free();
          }
        };
        d.prototype.visitRenderable = function(a, b, c) {
          var d = a.getBounds();
          if (!(b.flags & 32 || d.isEmpty())) {
            if (b.hasFlags(64)) {
              b.removeFlags(64);
            } else {
              if (this._renderWithCache(a, b)) {
                return;
              }
            }
            var e = b.matrix, d = b.target.context, f = !!(b.flags & 16), g = !!(b.flags & 8);
            d.setTransform(e.a, e.b, e.c, e.d, e.tx, e.ty);
            this._frameInfo.shapes++;
            d.imageSmoothingEnabled = d.mozImageSmoothingEnabled = b.hasFlags(4096);
            e = a.properties.renderCount || 0;
            a.properties.renderCount = ++e;
            a.render(d, c, null, f ? b.clipPath : null, g);
          }
        };
        d.prototype._renderLayer = function(a, b) {
          var c = a.getLayer(), d = c.mask;
          if (d) {
            this._renderWithMask(a, d, c.blendMode, !a.hasFlags(131072) || !d.hasFlags(131072), b);
          } else {
            var d = v.allocate(), e = this._renderToTemporarySurface(a, b, d, null);
            e && (b.target.draw(e, d.x, d.y, d.w, d.h, c.blendMode), e.free());
            d.free();
          }
        };
        d.prototype._renderWithMask = function(a, b, c, d, e) {
          var f = b.getTransform().getConcatenatedMatrix(!0);
          b.parent || (f = f.scale(this._devicePixelRatio, this._devicePixelRatio));
          var g = a.getBounds().clone();
          e.matrix.transformRectangleAABB(g);
          g.snap();
          if (!g.isEmpty()) {
            var h = b.getBounds().clone();
            f.transformRectangleAABB(h);
            h.snap();
            if (!h.isEmpty()) {
              var m = e.clip.clone();
              m.intersect(g);
              m.intersect(h);
              m.snap();
              m.isEmpty() || (g = e.clone(), g.clip.set(m), a = this._renderToTemporarySurface(a, g, v.createEmpty(), null), g.free(), g = e.clone(), g.clip.set(m), g.matrix = f, g.flags |= 4, d && (g.flags |= 8), b = this._renderToTemporarySurface(b, g, v.createEmpty(), a.surface), g.free(), a.draw(b, 0, 0, m.w, m.h, 11), e.target.draw(a, m.x, m.y, m.w, m.h, c), b.free(), a.free());
            }
          }
        };
        d.prototype._renderStageToTarget = function(b, c, d) {
          v.allocationCount = a.allocationCount = x.allocationCount = 0;
          b = new x(b);
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
        d.prototype._renderToTemporarySurface = function(a, b, c, d) {
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
          f = new v(f.x, f.y, c.w, c.h);
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
        d.prototype._allocateSurface = function(a, b, c) {
          return d._surfaceCache.allocate(a, b, c);
        };
        d.prototype.screenShot = function(a, c) {
          if (c) {
            var d = this._stage.content.groupChild.child;
            b(d instanceof r.Stage);
            a = d.content.getBounds(!0);
            d.content.getTransform().getConcatenatedMatrix().transformRectangleAABB(a);
            a.intersect(this._viewport);
          }
          a || (a = new v(0, 0, this._target.w, this._target.h));
          d = document.createElement("canvas");
          d.width = a.w;
          d.height = a.h;
          var e = d.getContext("2d");
          e.fillStyle = this._container.style.backgroundColor;
          e.fillRect(0, 0, a.w, a.h);
          e.drawImage(this._target.context.canvas, a.x, a.y, a.w, a.h, 0, 0, a.w, a.h);
          return new r.ScreenShot(d.toDataURL("image/png"), a.w, a.h);
        };
        d._initializedCaches = !1;
        d._debugPoints = n.createEmptyPoints(4);
        return d;
      }(r.Renderer);
      g.Canvas2DRenderer = f;
    })(r.Canvas2D || (r.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = r.Geometry.Point, b = r.Geometry.Matrix, v = r.Geometry.Rectangle, n = k.Tools.Mini.FPS, a = function() {
      function a() {
      }
      a.prototype.onMouseUp = function(a, b) {
        a.state = this;
      };
      a.prototype.onMouseDown = function(a, b) {
        a.state = this;
      };
      a.prototype.onMouseMove = function(a, b) {
        a.state = this;
      };
      a.prototype.onMouseWheel = function(a, b) {
        a.state = this;
      };
      a.prototype.onMouseClick = function(a, b) {
        a.state = this;
      };
      a.prototype.onKeyUp = function(a, b) {
        a.state = this;
      };
      a.prototype.onKeyDown = function(a, b) {
        a.state = this;
      };
      a.prototype.onKeyPress = function(a, b) {
        a.state = this;
      };
      return a;
    }();
    r.UIState = a;
    var h = function(a) {
      function b() {
        a.apply(this, arguments);
        this._keyCodes = [];
      }
      __extends(b, a);
      b.prototype.onMouseDown = function(a, b) {
        b.altKey && (a.state = new l(a.worldView, a.getMousePosition(b, null), a.worldView.getTransform().getMatrix(!0)));
      };
      b.prototype.onMouseClick = function(a, b) {
      };
      b.prototype.onKeyDown = function(a, b) {
        this._keyCodes[b.keyCode] = !0;
      };
      b.prototype.onKeyUp = function(a, b) {
        this._keyCodes[b.keyCode] = !1;
      };
      return b;
    }(a), p = function(a) {
      function b() {
        a.apply(this, arguments);
        this._keyCodes = [];
        this._paused = !1;
        this._mousePosition = new g(0, 0);
      }
      __extends(b, a);
      b.prototype.onMouseMove = function(a, b) {
        this._mousePosition = a.getMousePosition(b, null);
        this._update(a);
      };
      b.prototype.onMouseDown = function(a, b) {
      };
      b.prototype.onMouseClick = function(a, b) {
      };
      b.prototype.onMouseWheel = function(a, b) {
        var e = "DOMMouseScroll" === b.type ? -b.detail : b.wheelDelta / 40;
        if (b.altKey) {
          b.preventDefault();
          var f = a.getMousePosition(b, null), c = a.worldView.getTransform().getMatrix(!0), e = 1 + e / 1E3;
          c.translate(-f.x, -f.y);
          c.scale(e, e);
          c.translate(f.x, f.y);
          a.worldView.getTransform().setMatrix(c);
        }
      };
      b.prototype.onKeyPress = function(a, b) {
        if (b.altKey) {
          var e = b.keyCode || b.which;
          console.info("onKeyPress Code: " + e);
          switch(e) {
            case 248:
              this._paused = !this._paused;
              b.preventDefault();
              break;
            case 223:
              a.toggleOption("paintRenderable");
              b.preventDefault();
              break;
            case 8730:
              a.toggleOption("paintViewport");
              b.preventDefault();
              break;
            case 8747:
              a.toggleOption("paintBounds");
              b.preventDefault();
              break;
            case 8706:
              a.toggleOption("paintDirtyRegion");
              b.preventDefault();
              break;
            case 231:
              a.toggleOption("clear");
              b.preventDefault();
              break;
            case 402:
              a.toggleOption("paintFlashing"), b.preventDefault();
          }
          this._update(a);
        }
      };
      b.prototype.onKeyDown = function(a, b) {
        this._keyCodes[b.keyCode] = !0;
        this._update(a);
      };
      b.prototype.onKeyUp = function(a, b) {
        this._keyCodes[b.keyCode] = !1;
        this._update(a);
      };
      b.prototype._update = function(a) {
        a.paused = this._paused;
        if (a.getOption()) {
          var b = r.viewportLoupeDiameter.value, e = r.viewportLoupeDiameter.value;
          a.viewport = new v(this._mousePosition.x - b / 2, this._mousePosition.y - e / 2, b, e);
        } else {
          a.viewport = null;
        }
      };
      return b;
    }(a);
    (function(a) {
      function b() {
        a.apply(this, arguments);
        this._startTime = Date.now();
      }
      __extends(b, a);
      b.prototype.onMouseMove = function(a, b) {
        if (!(10 > Date.now() - this._startTime)) {
          var e = a._world;
          e && (a.state = new l(e, a.getMousePosition(b, null), e.getTransform().getMatrix(!0)));
        }
      };
      b.prototype.onMouseUp = function(a, b) {
        a.state = new h;
        a.selectNodeUnderMouse(b);
      };
      return b;
    })(a);
    var l = function(a) {
      function b(g, h, e) {
        a.call(this);
        this._target = g;
        this._startPosition = h;
        this._startMatrix = e;
      }
      __extends(b, a);
      b.prototype.onMouseMove = function(a, b) {
        b.preventDefault();
        var e = a.getMousePosition(b, null);
        e.sub(this._startPosition);
        this._target.getTransform().setMatrix(this._startMatrix.clone().translate(e.x, e.y));
        a.state = this;
      };
      b.prototype.onMouseUp = function(a, b) {
        a.state = new h;
      };
      return b;
    }(a), a = function() {
      function a(b, g, l) {
        function e(a) {
          d._state.onMouseWheel(d, a);
          d._persistentState.onMouseWheel(d, a);
        }
        void 0 === g && (g = !1);
        void 0 === l && (l = void 0);
        this._state = new h;
        this._persistentState = new p;
        this.paused = !1;
        this.viewport = null;
        this._selectedNodes = [];
        this._isRendering = !1;
        this._rAF = void 0;
        this._eventListeners = Object.create(null);
        this._fullScreen = !1;
        this._container = b;
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
        b.appendChild(g);
        if (r.hud.value) {
          var f = document.createElement("div");
          f.style.position = "absolute";
          f.style.width = "100%";
          f.style.height = "100%";
          f.style.pointerEvents = "none";
          var c = document.createElement("div");
          c.style.position = "absolute";
          c.style.width = "100%";
          c.style.height = "20px";
          c.style.pointerEvents = "none";
          f.appendChild(c);
          b.appendChild(f);
          this._fps = new n(c);
        } else {
          this._fps = null;
        }
        this.transparent = f = 0 === l;
        void 0 === l || 0 === l || k.ColorUtilities.rgbaToCSSStyle(l);
        this._options = new r.Canvas2D.Canvas2DRendererOptions;
        this._options.alpha = f;
        this._renderer = new r.Canvas2D.Canvas2DRenderer(g, this._stage, this._options);
        this._listenForContainerSizeChanges();
        this._onMouseUp = this._onMouseUp.bind(this);
        this._onMouseDown = this._onMouseDown.bind(this);
        this._onMouseMove = this._onMouseMove.bind(this);
        var d = this;
        window.addEventListener("mouseup", function(a) {
          d._state.onMouseUp(d, a);
          d._render();
        }, !1);
        window.addEventListener("mousemove", function(a) {
          d._state.onMouseMove(d, a);
          d._persistentState.onMouseMove(d, a);
        }, !1);
        window.addEventListener("DOMMouseScroll", e);
        window.addEventListener("mousewheel", e);
        b.addEventListener("mousedown", function(a) {
          d._state.onMouseDown(d, a);
        });
        window.addEventListener("keydown", function(a) {
          d._state.onKeyDown(d, a);
          if (d._state !== d._persistentState) {
            d._persistentState.onKeyDown(d, a);
          }
        }, !1);
        window.addEventListener("keypress", function(a) {
          d._state.onKeyPress(d, a);
          if (d._state !== d._persistentState) {
            d._persistentState.onKeyPress(d, a);
          }
        }, !1);
        window.addEventListener("keyup", function(a) {
          d._state.onKeyUp(d, a);
          if (d._state !== d._persistentState) {
            d._persistentState.onKeyUp(d, a);
          }
        }, !1);
      }
      a.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var g = this;
        setInterval(function() {
          if (a !== g._containerWidth || b !== g._containerHeight) {
            g._onContainerSizeChanged(), a = g._containerWidth, b = g._containerHeight;
          }
        }, 10);
      };
      a.prototype._onContainerSizeChanged = function() {
        var a = this.getRatio(), g = Math.ceil(this._containerWidth * a), h = Math.ceil(this._containerHeight * a);
        this._stage.setBounds(new v(0, 0, g, h));
        this._stage.content.setBounds(new v(0, 0, g, h));
        this._worldView.getTransform().setMatrix(new b(a, 0, 0, a, 0, 0));
        this._dispatchEvent("resize");
      };
      a.prototype.addEventListener = function(a, b) {
        this._eventListeners[a] || (this._eventListeners[a] = []);
        this._eventListeners[a].push(b);
      };
      a.prototype._dispatchEvent = function(a) {
        if (a = this._eventListeners[a]) {
          for (var b = 0;b < a.length;b++) {
            a[b]();
          }
        }
      };
      a.prototype.startRendering = function() {
        if (!this._isRendering) {
          this._isRendering = !0;
          var a = this;
          this._rAF = requestAnimationFrame(function x() {
            a.render();
            a._rAF = requestAnimationFrame(x);
          });
        }
      };
      a.prototype.stopRendering = function() {
        this._isRendering && (this._isRendering = !1, cancelAnimationFrame(this._rAF));
      };
      Object.defineProperty(a.prototype, "state", {set:function(a) {
        this._state = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "cursor", {set:function(a) {
        this._container.style.cursor = a;
      }, enumerable:!0, configurable:!0});
      a.prototype._render = function() {
        r.RenderableVideo.checkForVideoUpdates();
        var a = (this._stage.readyToRender() || r.forcePaint.value) && !this.paused, b = 0;
        if (a) {
          var g = this._renderer;
          g.viewport = this.viewport ? this.viewport : this._stage.getBounds();
          this._dispatchEvent("render");
          b = performance.now();
          g.render();
          b = performance.now() - b;
        }
        this._fps && this._fps.tickAndRender(!a, b);
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
        return {stageWidth:this._containerWidth, stageHeight:this._containerHeight, pixelRatio:this.getRatio(), screenWidth:window.screen ? window.screen.width : 640, screenHeight:window.screen ? window.screen.height : 480};
      };
      a.prototype.toggleOption = function(a) {
        var b = this._options;
        b[a] = !b[a];
      };
      a.prototype.getOption = function() {
        return this._options.paintViewport;
      };
      a.prototype.getRatio = function() {
        var a = window.devicePixelRatio || 1, b = 1;
        1 === a || this._disableHiDPI || (b = a / 1);
        return b;
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
        var l = this._container, e = l.getBoundingClientRect(), f = this.getRatio(), l = new g(l.scrollWidth / e.width * (a.clientX - e.left) * f, l.scrollHeight / e.height * (a.clientY - e.top) * f);
        if (!h) {
          return l;
        }
        e = b.createIdentity();
        h.getTransform().getConcatenatedMatrix().inverse(e);
        e.transformPoint(l);
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
      a.prototype.screenShot = function(a, b) {
        return this._renderer.screenShot(a, b);
      };
      return a;
    }();
    r.Easel = a;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = k.GFX.Geometry.Matrix;
    (function(b) {
      b[b.Simple = 0] = "Simple";
    })(r.Layout || (r.Layout = {}));
    var b = function(b) {
      function a() {
        b.apply(this, arguments);
        this.layout = 0;
      }
      __extends(a, b);
      return a;
    }(r.RendererOptions);
    r.TreeRendererOptions = b;
    var v = function(k) {
      function a(a, g, l) {
        void 0 === l && (l = new b);
        k.call(this, a, g, l);
        this._canvas = document.createElement("canvas");
        this._container.appendChild(this._canvas);
        this._context = this._canvas.getContext("2d");
        this._listenForContainerSizeChanges();
      }
      __extends(a, k);
      a.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var g = this;
        setInterval(function() {
          if (a !== g._containerWidth || b !== g._containerHeight) {
            g._onContainerSizeChanged(), a = g._containerWidth, b = g._containerHeight;
          }
        }, 10);
      };
      a.prototype._getRatio = function() {
        var a = window.devicePixelRatio || 1, b = 1;
        1 !== a && (b = a / 1);
        return b;
      };
      a.prototype._onContainerSizeChanged = function() {
        var a = this._getRatio(), b = Math.ceil(this._containerWidth * a), g = Math.ceil(this._containerHeight * a), k = this._canvas;
        0 < a ? (k.width = b * a, k.height = g * a, k.style.width = b + "px", k.style.height = g + "px") : (k.width = b, k.height = g);
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
      a.prototype._renderNodeSimple = function(a, b, g) {
        function k(b) {
          var d = b.getChildren();
          a.fillStyle = b.hasFlags(16) ? "red" : "white";
          var g = String(b.id);
          b instanceof r.RenderableText ? g = "T" + g : b instanceof r.RenderableShape ? g = "S" + g : b instanceof r.RenderableBitmap ? g = "B" + g : b instanceof r.RenderableVideo && (g = "V" + g);
          b instanceof r.Renderable && (g = g + " [" + b._parents.length + "]");
          b = a.measureText(g).width;
          a.fillText(g, n, x);
          if (d) {
            n += b + 4;
            f = Math.max(f, n + 20);
            for (g = 0;g < d.length;g++) {
              k(d[g]), g < d.length - 1 && (x += 18, x > m._canvas.height && (a.fillStyle = "gray", n = n - e + f + 8, e = f + 8, x = 0, a.fillStyle = "white"));
            }
            n -= b + 4;
          }
        }
        var m = this;
        a.save();
        a.font = "16px Arial";
        a.fillStyle = "white";
        var n = 0, x = 0, e = 0, f = 0;
        k(b);
        a.restore();
      };
      return a;
    }(r.Renderer);
    r.TreeRenderer = v;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.GFX.BlurFilter, v = k.GFX.DropshadowFilter, n = k.GFX.Shape, a = k.GFX.Group, h = k.GFX.RenderableShape, p = k.GFX.RenderableMorphShape, l = k.GFX.RenderableBitmap, w = k.GFX.RenderableVideo, m = k.GFX.RenderableText, t = k.GFX.ColorMatrix, x = k.ShapeData, e = k.ArrayUtilities.DataBuffer, f = k.GFX.Stage, c = k.GFX.Geometry.Matrix, d = k.GFX.Geometry.Rectangle, q = function() {
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
          function e(a) {
            a = a.getBounds(!0);
            var c = b.easel.getRatio();
            a.scale(1 / c, 1 / c);
            a.snap();
            g.setBounds(a);
          }
          var g = this.stage = new f(128, 512);
          "undefined" !== typeof registerInspectorStage && registerInspectorStage(g);
          e(b.stage);
          b.stage.addEventListener(1, e);
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
          this._registerAsset(a, 0, new w(a, this));
        };
        a.prototype._decodeImage = function(a, b, c) {
          var e = new Image, f = l.FromImage(e);
          e.src = URL.createObjectURL(new Blob([b], {type:k.getMIMETypeForImageType(a)}));
          e.onload = function() {
            f.setBounds(new d(0, 0, e.width, e.height));
            f.invalidate();
            c({width:e.width, height:e.height});
          };
          e.onerror = function() {
            c(null);
          };
          return f;
        };
        a.prototype.sendVideoPlaybackEvent = function(a, b, c) {
          this._easelHost.sendVideoPlaybackEvent(a, b, c);
        };
        return a;
      }();
      g.GFXChannelDeserializerContext = q;
      q = function() {
        function f() {
        }
        f.prototype.read = function() {
          for (var a = 0, b = this.input, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0, l = 0, m = 0;0 < b.bytesAvailable;) {
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
                m++, this._readRequestBitmapData();
            }
          }
        };
        f.prototype._readMatrix = function() {
          var a = this.input, b = f._temporaryReadMatrix;
          b.setElements(a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat() / 20, a.readFloat() / 20);
          return b;
        };
        f.prototype._readRectangle = function() {
          var a = this.input, b = f._temporaryReadRectangle;
          b.setElements(a.readInt() / 20, a.readInt() / 20, a.readInt() / 20, a.readInt() / 20);
          return b;
        };
        f.prototype._readColorMatrix = function() {
          var a = this.input, b = f._temporaryReadColorMatrix, c = 1, d = 1, e = 1, g = 1, h = 0, l = 0, m = 0, k = 0;
          switch(a.readInt()) {
            case 0:
              return f._temporaryReadColorMatrixIdentity;
            case 1:
              g = a.readFloat();
              break;
            case 2:
              c = a.readFloat(), d = a.readFloat(), e = a.readFloat(), g = a.readFloat(), h = a.readInt(), l = a.readInt(), m = a.readInt(), k = a.readInt();
          }
          b.setMultipliersAndOffsets(c, d, e, g, h, l, m, k);
          return b;
        };
        f.prototype._readAsset = function() {
          var a = this.input.readInt(), b = this.inputAssets[a];
          this.inputAssets[a] = null;
          return b;
        };
        f.prototype._readUpdateGraphics = function() {
          for (var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getAsset(c), f = this._readRectangle(), g = x.FromPlainObject(this._readAsset()), l = a.readInt(), m = [], k = 0;k < l;k++) {
            var n = a.readInt();
            m.push(b._getBitmapAsset(n));
          }
          if (e) {
            e.update(g, m, f);
          } else {
            a = g.morphCoordinates ? new p(c, g, m, f) : new h(c, g, m, f);
            for (k = 0;k < m.length;k++) {
              m[k] && m[k].addRenderableParent(a);
            }
            b._registerAsset(c, d, a);
          }
        };
        f.prototype._readUpdateBitmapData = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), f = b._getBitmapAsset(c), g = this._readRectangle(), a = a.readInt(), h = e.FromPlainObject(this._readAsset());
          f ? f.updateFromDataBuffer(a, h) : (f = l.FromDataBuffer(a, h, g), b._registerAsset(c, d, f));
        };
        f.prototype._readUpdateTextContent = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), f = b._getTextAsset(c), g = this._readRectangle(), h = this._readMatrix(), l = a.readInt(), k = a.readInt(), n = a.readInt(), p = a.readBoolean(), q = a.readInt(), r = a.readInt(), t = this._readAsset(), u = e.FromPlainObject(this._readAsset()), w = null, x = a.readInt();
          x && (w = new e(4 * x), a.readBytes(w, 4 * x));
          f ? (f.setBounds(g), f.setContent(t, u, h, w), f.setStyle(l, k, q, r), f.reflow(n, p)) : (f = new m(g), f.setContent(t, u, h, w), f.setStyle(l, k, q, r), f.reflow(n, p), b._registerAsset(c, d, f));
          if (this.output) {
            for (a = f.textRect, this.output.writeInt(20 * a.w), this.output.writeInt(20 * a.h), this.output.writeInt(20 * a.x), f = f.lines, a = f.length, this.output.writeInt(a), b = 0;b < a;b++) {
              this._writeLineMetrics(f[b]);
            }
          }
        };
        f.prototype._writeLineMetrics = function(a) {
          this.output.writeInt(a.x);
          this.output.writeInt(a.width);
          this.output.writeInt(a.ascent);
          this.output.writeInt(a.descent);
          this.output.writeInt(a.leading);
        };
        f.prototype._readUpdateStage = function() {
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
        f.prototype._readUpdateNetStream = function() {
          var a = this.context, b = this.input.readInt(), c = a._getVideoAsset(b), d = this._readRectangle();
          c || (a.registerVideo(b), c = a._getVideoAsset(b));
          c.setBounds(d);
        };
        f.prototype._readFilters = function(a) {
          var c = this.input, d = c.readInt(), e = [];
          if (d) {
            for (var f = 0;f < d;f++) {
              var g = c.readInt();
              switch(g) {
                case 0:
                  e.push(new b(c.readFloat(), c.readFloat(), c.readInt()));
                  break;
                case 1:
                  e.push(new v(c.readFloat(), c.readFloat(), c.readFloat(), c.readFloat(), c.readInt(), c.readFloat(), c.readBoolean(), c.readBoolean(), c.readBoolean(), c.readInt(), c.readFloat()));
                  break;
                default:
                  k.Debug.somewhatImplemented(r.FilterType[g]);
              }
            }
            a.getLayer().filters = e;
          }
        };
        f.prototype._readUpdateFrame = function() {
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
        f.prototype._readDrawToBitmap = function() {
          var a = this.input, b = this.context, d = a.readInt(), e = a.readInt(), f = a.readInt(), g, h, m;
          g = f & 1 ? this._readMatrix().clone() : c.createIdentity();
          f & 8 && (h = this._readColorMatrix());
          f & 16 && (m = this._readRectangle());
          f = a.readInt();
          a.readBoolean();
          a = b._getBitmapAsset(d);
          e = b._makeNode(e);
          a ? a.drawNode(e, g, h, f, m) : b._registerAsset(d, -1, l.FromNode(e, g, h, f, m));
        };
        f.prototype._readRequestBitmapData = function() {
          var a = this.output, b = this.context, c = this.input.readInt();
          b._getBitmapAsset(c).readImageData(a);
        };
        f._temporaryReadMatrix = c.createIdentity();
        f._temporaryReadRectangle = d.createEmpty();
        f._temporaryReadColorMatrix = t.createIdentity();
        f._temporaryReadColorMatrixIdentity = t.createIdentity();
        return f;
      }();
      g.GFXChannelDeserializer = q;
    })(r.GFX || (r.GFX = {}));
  })(k.Remoting || (k.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    var g = k.GFX.Geometry.Point, b = k.ArrayUtilities.DataBuffer, v = function() {
      function n(a) {
        this._easel = a;
        var b = a.transparent;
        this._group = a.world;
        this._content = null;
        this._fullscreen = !1;
        this._context = new k.Remoting.GFX.GFXChannelDeserializerContext(this, this._group, b);
        this._addEventListeners();
      }
      n.prototype.onSendUpdates = function(a, b) {
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
        var h = this._easel.getMousePosition(a, this._content), h = new g(h.x, h.y), n = new b, l = new k.Remoting.GFX.GFXChannelSerializer;
        l.output = n;
        l.writeMouseEvent(a, h);
        this.onSendUpdates(n, []);
      };
      n.prototype._keyboardEventListener = function(a) {
        var g = new b, n = new k.Remoting.GFX.GFXChannelSerializer;
        n.output = g;
        n.writeKeyboardEvent(a);
        this.onSendUpdates(g, []);
      };
      n.prototype._addEventListeners = function() {
        for (var a = this._mouseEventListener.bind(this), b = this._keyboardEventListener.bind(this), g = n._mouseEvents, l = 0;l < g.length;l++) {
          window.addEventListener(g[l], a);
        }
        a = n._keyboardEvents;
        for (l = 0;l < a.length;l++) {
          window.addEventListener(a[l], b);
        }
        this._addFocusEventListeners();
        this._easel.addEventListener("resize", this._resizeEventListener.bind(this));
      };
      n.prototype._sendFocusEvent = function(a) {
        var g = new b, n = new k.Remoting.GFX.GFXChannelSerializer;
        n.output = g;
        n.writeFocusEvent(a);
        this.onSendUpdates(g, []);
      };
      n.prototype._addFocusEventListeners = function() {
        var a = this;
        document.addEventListener("visibilitychange", function(b) {
          a._sendFocusEvent(document.hidden ? 0 : 1);
        });
        window.addEventListener("focus", function(b) {
          a._sendFocusEvent(3);
        });
        window.addEventListener("blur", function(b) {
          a._sendFocusEvent(2);
        });
      };
      n.prototype._resizeEventListener = function() {
        this.onDisplayParameters(this._easel.getDisplayParameters());
      };
      n.prototype.onDisplayParameters = function(a) {
        throw Error("This method is abstract");
      };
      n.prototype.processUpdates = function(a, b, g) {
        void 0 === g && (g = null);
        var l = new k.Remoting.GFX.GFXChannelDeserializer;
        l.input = a;
        l.inputAssets = b;
        l.output = g;
        l.context = this._context;
        l.read();
      };
      n.prototype.processVideoControl = function(a, b, g) {
        var l = this._context, k = l._getVideoAsset(a);
        if (!k) {
          if (1 !== b) {
            return;
          }
          l.registerVideo(a);
          k = l._getVideoAsset(a);
        }
        return k.processControlRequest(b, g);
      };
      n.prototype.processRegisterFontOrImage = function(a, b, g, l, k) {
        "font" === g ? this._context.registerFont(a, l, k) : this._context.registerImage(a, b, l, k);
      };
      n.prototype.processFSCommand = function(a, b) {
      };
      n.prototype.processFrame = function() {
      };
      n.prototype.onVideoPlaybackEvent = function(a, b, g) {
        throw Error("This method is abstract");
      };
      n.prototype.sendVideoPlaybackEvent = function(a, b, g) {
        this.onVideoPlaybackEvent(a, b, g);
      };
      n._mouseEvents = k.Remoting.MouseEventNames;
      n._keyboardEvents = k.Remoting.KeyboardEventNames;
      return n;
    }();
    r.EaselHost = v;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.ArrayUtilities.DataBuffer, v = k.CircularBuffer, n = k.Tools.Profiler.TimelineBuffer, a = function(a) {
        function g(b, k, m) {
          a.call(this, b);
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
        g.prototype.onSendUpdates = function(a, b) {
          var g = a.getBytes();
          this._playerWindow.postMessage({type:"gfx", updates:g, assets:b}, "*", [g.buffer]);
        };
        g.prototype.onDisplayParameters = function(a) {
          this._playerWindow.postMessage({type:"displayParameters", params:a}, "*");
        };
        g.prototype.onVideoPlaybackEvent = function(a, b, g) {
          this._playerWindow.postMessage({type:"videoPlayback", id:a, eventType:b, data:g}, "*");
        };
        g.prototype.requestTimeline = function(a, b) {
          return new Promise(function(g) {
            this._timelineRequests[a] = g;
            this._playerWindow.postMessage({type:"timeline", cmd:b, request:a}, "*");
          }.bind(this));
        };
        g.prototype._sendRegisterFontOrImageResponse = function(a, b) {
          this._playerWindow.postMessage({type:"registerFontOrImageResponse", requestId:a, result:b}, "*");
        };
        g.prototype.onWindowMessage = function(a, g) {
          void 0 === g && (g = !0);
          if ("object" === typeof a && null !== a) {
            if ("player" === a.type) {
              var h = b.FromArrayBuffer(a.updates.buffer);
              if (g) {
                this.processUpdates(h, a.assets);
              } else {
                var k = new b;
                this.processUpdates(h, a.assets, k);
                a.result = k.toPlainObject();
              }
            } else {
              "frame" !== a.type && ("videoControl" === a.type ? a.result = this.processVideoControl(a.id, a.eventType, a.data) : "registerFontOrImage" === a.type ? this.processRegisterFontOrImage(a.syncId, a.symbolId, a.assetType, a.data, this._sendRegisterFontOrImageResponse.bind(this, a.requestId)) : "fscommand" !== a.type && "timelineResponse" === a.type && a.timeline && (a.timeline.__proto__ = n.prototype, a.timeline._marks.__proto__ = v.prototype, a.timeline._times.__proto__ = v.prototype, 
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
      var b = k.ArrayUtilities.DataBuffer, v = function(g) {
        function a(a) {
          g.call(this, a);
          this._worker = k.Player.Test.FakeSyncWorker.instance;
          this._worker.addEventListener("message", this._onWorkerMessage.bind(this));
          this._worker.addEventListener("syncmessage", this._onSyncWorkerMessage.bind(this));
        }
        __extends(a, g);
        a.prototype.onSendUpdates = function(a, b) {
          var g = a.getBytes();
          this._worker.postMessage({type:"gfx", updates:g, assets:b}, [g.buffer]);
        };
        a.prototype.onDisplayParameters = function(a) {
          this._worker.postMessage({type:"displayParameters", params:a});
        };
        a.prototype.onVideoPlaybackEvent = function(a, b, g) {
          this._worker.postMessage({type:"videoPlayback", id:a, eventType:b, data:g});
        };
        a.prototype.requestTimeline = function(a, b) {
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
          "clear" === b && g && g.reset();
          return Promise.resolve(g);
        };
        a.prototype._sendRegisterFontOrImageResponse = function(a, b) {
          this._worker.postMessage({type:"registerFontOrImageResponse", requestId:a, result:b});
        };
        a.prototype._onWorkerMessage = function(a, g) {
          void 0 === g && (g = !0);
          var l = a.data;
          if ("object" === typeof l && null !== l) {
            switch(l.type) {
              case "player":
                var k = b.FromArrayBuffer(l.updates.buffer);
                if (g) {
                  this.processUpdates(k, l.assets);
                } else {
                  var m = new b;
                  this.processUpdates(k, l.assets, m);
                  a.result = m.toPlainObject();
                  a.handled = !0;
                }
                break;
              case "videoControl":
                a.result = this.processVideoControl(l.id, l.eventType, l.data);
                a.handled = !0;
                break;
              case "registerFontOrImage":
                this.processRegisterFontOrImage(l.syncId, l.symbolId, l.assetType, l.data, this._sendRegisterFontOrImageResponse.bind(this, l.requestId)), a.handled = !0;
            }
          }
        };
        a.prototype._onSyncWorkerMessage = function(a) {
          return this._onWorkerMessage(a, !1);
        };
        return a;
      }(r.EaselHost);
      g.TestEaselHost = v;
    })(r.Test || (r.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      function b(a, b) {
        a.writeInt(b.length);
        a.writeRawBytes(b);
      }
      function r(a) {
        function g(a) {
          switch(typeof a) {
            case "undefined":
              l.writeByte(0);
              break;
            case "boolean":
              l.writeByte(a ? 2 : 3);
              break;
            case "number":
              l.writeByte(4);
              l.writeDouble(a);
              break;
            case "string":
              l.writeByte(5);
              l.writeUTF(a);
              break;
            default:
              if (null === a) {
                l.writeByte(1);
                break;
              }
              if (Array.isArray(a) && a instanceof Int32Array) {
                l.writeByte(6);
                l.writeInt(a.length);
                for (var f = 0;f < a.length;f++) {
                  g(a[f]);
                }
              } else {
                if (a instanceof Uint8Array) {
                  l.writeByte(9), b(l, a);
                } else {
                  if ("length" in a && "buffer" in a && "littleEndian" in a) {
                    l.writeByte(a.littleEndian ? 10 : 11), b(l, new Uint8Array(a.buffer, 0, a.length));
                  } else {
                    if (a instanceof ArrayBuffer) {
                      l.writeByte(8), b(l, new Uint8Array(a));
                    } else {
                      if (a instanceof Int32Array) {
                        l.writeByte(12), b(l, new Uint8Array(a.buffer, a.byteOffset, a.byteLength));
                      } else {
                        if (a.buffer instanceof ArrayBuffer && "number" === typeof a.byteOffset) {
                          throw Error("Some unsupported TypedArray is used");
                        }
                        l.writeByte(7);
                        for (f in a) {
                          l.writeUTF(f), g(a[f]);
                        }
                        l.writeUTF("");
                      }
                    }
                  }
                }
              }
            ;
          }
        }
        var l = new h;
        g(a);
        return l.getBytes();
      }
      function n(a) {
        var b = new h, g = a.readInt();
        a.readBytes(b, g);
        return b.getBytes();
      }
      function a(a) {
        function b() {
          var a = g.readByte();
          switch(a) {
            case 1:
              return null;
            case 2:
              return !0;
            case 3:
              return !1;
            case 4:
              return g.readDouble();
            case 5:
              return g.readUTF();
            case 6:
              for (var a = [], c = g.readInt(), d = 0;d < c;d++) {
                a[d] = b();
              }
              return a;
            case 7:
              for (a = {};c = g.readUTF();) {
                a[c] = b();
              }
              return a;
            case 8:
              return n(g).buffer;
            case 9:
              return n(g);
            case 11:
            ;
            case 10:
              return c = n(g), new p(c.buffer, c.length, 10 === a);
            case 12:
              return new Int32Array(n(g).buffer);
          }
        }
        var g = new h, e = a.readInt();
        a.readBytes(g, e);
        return b();
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
        function a(b) {
          this._maxRecordingSize = b;
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
          (new w(this._recording.getBytes())).dump();
        };
        a.prototype._createRecord = function(a, b) {
          this._stopped || (this._recording.length + 8 + (b ? b.length : 0) >= this._maxRecordingSize ? (console.error("Recording limit reached"), this._stopped = !0) : (this._recording.writeInt(Date.now() - this._recordingStarted), this._recording.writeInt(a), null !== b ? (this._recording.writeInt(b.length), this._recording.writeRawBytes(b.getBytes())) : this._recording.writeInt(0)));
        };
        a.prototype.recordPlayerCommand = function(a, g, e) {
          var f = new h;
          b(f, g);
          f.writeInt(e.length);
          e.forEach(function(a) {
            a = r(a);
            b(f, a);
          });
          this._createRecord(a ? 2 : 1, f);
        };
        a.prototype.recordFrame = function() {
          this._createRecord(3, null);
        };
        a.prototype.recordFontOrImage = function(a, g, e, f) {
          var c = new h;
          c.writeInt(a);
          c.writeInt(g);
          c.writeUTF(e);
          b(c, r(f));
          this._createRecord(4, c);
        };
        a.prototype.recordFSCommand = function(a, b) {
          var e = new h;
          e.writeUTF(a);
          e.writeUTF(b || "");
          this._createRecord(5, e);
        };
        return a;
      }();
      g.MovieRecorder = l;
      var w = function() {
        function b(a) {
          this._buffer = new h;
          this._buffer.writeRawBytes(a);
          this._buffer.position = 4;
        }
        b.prototype.readNextRecord = function() {
          if (this._buffer.position >= this._buffer.length) {
            return 0;
          }
          var a = this._buffer.readInt(), b = this._buffer.readInt(), e = this._buffer.readInt(), f = null;
          0 < e && (f = new h, this._buffer.readBytes(f, e));
          this.currentTimestamp = a;
          this.currentType = b;
          this.currentData = f;
          return b;
        };
        b.prototype.parsePlayerCommand = function() {
          for (var b = n(this.currentData), g = this.currentData.readInt(), e = [], f = 0;f < g;f++) {
            e.push(a(this.currentData));
          }
          return {updates:b, assets:e};
        };
        b.prototype.parseFSCommand = function() {
          var a = this.currentData.readUTF(), b = this.currentData.readUTF();
          return {command:a, args:b};
        };
        b.prototype.parseFontOrImage = function() {
          var b = this.currentData.readInt(), g = this.currentData.readInt(), e = this.currentData.readUTF(), f = a(this.currentData);
          return {syncId:b, symbolId:g, assetType:e, data:f};
        };
        b.prototype.dump = function() {
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
        return b;
      }();
      g.MovieRecordParser = w;
    })(r.Test || (r.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(r) {
    (function(g) {
      var b = k.ArrayUtilities.DataBuffer, v = function(k) {
        function a(a) {
          k.call(this, a);
          this.alwaysRenderFrame = this.ignoreTimestamps = !1;
          this.cpuTimeRendering = this.cpuTimeUpdates = 0;
          this.onComplete = null;
        }
        __extends(a, k);
        Object.defineProperty(a.prototype, "cpuTime", {get:function() {
          return this.cpuTimeUpdates + this.cpuTimeRendering;
        }, enumerable:!0, configurable:!0});
        a.prototype.playUrl = function(a) {
          var b = new XMLHttpRequest;
          b.open("GET", a, !0);
          b.responseType = "arraybuffer";
          b.onload = function() {
            this.playBytes(new Uint8Array(b.response));
          }.bind(this);
          b.send();
        };
        a.prototype.playBytes = function(a) {
          this._parser = new g.MovieRecordParser(a);
          this._lastTimestamp = 0;
          this._parseNext();
        };
        a.prototype.onSendUpdates = function(a, b) {
        };
        a.prototype.onDisplayParameters = function(a) {
        };
        a.prototype.onVideoPlaybackEvent = function(a, b, g) {
        };
        a.prototype.requestTimeline = function(a, b) {
          return Promise.resolve(void 0);
        };
        a.prototype._parseNext = function() {
          if (0 !== this._parser.readNextRecord()) {
            var a = this._runRecord.bind(this), b = this._parser.currentTimestamp - this._lastTimestamp;
            this._lastTimestamp = this._parser.currentTimestamp;
            5 > b ? Promise.resolve(void 0).then(a) : this.ignoreTimestamps ? setTimeout(a) : setTimeout(a, b);
          } else {
            if (this.onComplete) {
              this.onComplete();
            }
          }
        };
        a.prototype._runRecord = function() {
          var a, g = performance.now();
          switch(this._parser.currentType) {
            case 1:
            ;
            case 2:
              a = this._parser.parsePlayerCommand();
              var l = 2 === this._parser.currentType, k = b.FromArrayBuffer(a.updates.buffer);
              l ? this.processUpdates(k, a.assets) : (l = new b, this.processUpdates(k, a.assets, l));
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
          this.cpuTimeUpdates += performance.now() - g;
          3 === this._parser.currentType && this.alwaysRenderFrame ? requestAnimationFrame(this._renderFrameJustAfterRAF.bind(this)) : this._parseNext();
        };
        a.prototype._renderFrameJustAfterRAF = function() {
          var a = performance.now();
          this.easel.render();
          this.cpuTimeRendering += performance.now() - a;
          this._parseNext();
        };
        return a;
      }(r.EaselHost);
      g.PlaybackEaselHost = v;
    })(r.Test || (r.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var b = function(b) {
        function k(a, h) {
          void 0 === h && (h = 0);
          b.call(this, a);
          this._recorder = null;
          this._recorder = new g.MovieRecorder(h);
        }
        __extends(k, b);
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
            b.prototype._onWorkerMessage.call(this, a, g);
          }
        };
        return k;
      }(g.TestEaselHost);
      g.RecordingEaselHost = b;
    })(k.Test || (k.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load GFX Dependencies");

