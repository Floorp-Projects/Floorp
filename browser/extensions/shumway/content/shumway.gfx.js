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
Shumway$$inline_0.version = "0.11.422";
Shumway$$inline_0.build = "137ba70";
var jsGlobal = function() {
  return this || (0,eval)("this//# sourceURL=jsGlobal-getter");
}(), inBrowser = "undefined" !== typeof window && "document" in window && "plugins" in window.document, inFirefox = "undefined" !== typeof navigator && 0 <= navigator.userAgent.indexOf("Firefox");
jsGlobal.performance || (jsGlobal.performance = {});
jsGlobal.performance.now || (jsGlobal.performance.now = function() {
  return Date.now();
});
function lazyInitializer(k, p, u) {
  Object.defineProperty(k, p, {get:function() {
    var a = u();
    Object.defineProperty(k, p, {value:a, configurable:!0, enumerable:!0});
    return a;
  }, configurable:!0, enumerable:!0});
}
var START_TIME = performance.now();
(function(k) {
  function p(e) {
    return (e | 0) === e;
  }
  function u(e) {
    return "object" === typeof e || "function" === typeof e;
  }
  function a(e) {
    return String(Number(e)) === e;
  }
  function w(e) {
    var g = 0;
    if ("number" === typeof e) {
      return g = e | 0, e === g && 0 <= g ? !0 : e >>> 0 === e;
    }
    if ("string" !== typeof e) {
      return !1;
    }
    var d = e.length;
    if (0 === d) {
      return !1;
    }
    if ("0" === e) {
      return !0;
    }
    if (d > k.UINT32_CHAR_BUFFER_LENGTH) {
      return !1;
    }
    var c = 0, g = e.charCodeAt(c++) - 48;
    if (1 > g || 9 < g) {
      return !1;
    }
    for (var f = 0, y = 0;c < d;) {
      y = e.charCodeAt(c++) - 48;
      if (0 > y || 9 < y) {
        return !1;
      }
      f = g;
      g = 10 * g + y;
    }
    return f < k.UINT32_MAX_DIV_10 || f === k.UINT32_MAX_DIV_10 && y <= k.UINT32_MAX_MOD_10 ? !0 : !1;
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
  k.isInteger = p;
  k.isArray = function(e) {
    return e instanceof Array;
  };
  k.isNumberOrString = function(e) {
    return "number" === typeof e || "string" === typeof e;
  };
  k.isObject = u;
  k.toNumber = function(e) {
    return +e;
  };
  k.isNumericString = a;
  k.isNumeric = function(e) {
    if ("number" === typeof e) {
      return !0;
    }
    if ("string" === typeof e) {
      var g = e.charCodeAt(0);
      return 65 <= g && 90 >= g || 97 <= g && 122 >= g || 36 === g || 95 === g ? !1 : w(e) || a(e);
    }
    return !1;
  };
  k.isIndex = w;
  k.isNullOrUndefined = function(e) {
    return void 0 == e;
  };
  k.argumentsToString = function(e) {
    for (var g = [], d = 0;d < e.length;d++) {
      var c = e[d];
      try {
        var f;
        f = "object" === typeof c && c ? "toString" in c ? c.toString() : Object.prototype.toString.call(c) : c + "";
        g.push(f);
      } catch (y) {
        g.push("<unprintable value>");
      }
    }
    return g.join(", ");
  };
  var m;
  (function(e) {
    e.error = function(c) {
      console.error(c);
      throw Error(c);
    };
    e.assert = function(c, f) {
      void 0 === f && (f = "assertion failed");
      "" === c && (c = !0);
      if (!c) {
        if ("undefined" !== typeof console && "assert" in console) {
          throw console.assert(!1, f), Error(f);
        }
        e.error(f.toString());
      }
    };
    e.assertUnreachable = function(c) {
      var f = Error().stack.split("\n")[1];
      throw Error("Reached unreachable location " + f + c);
    };
    e.assertNotImplemented = function(c, f) {
      c || e.error("notImplemented: " + f);
    };
    var g = Object.create(null);
    e.warning = function(c, f, d) {
    };
    e.warnCounts = function() {
      var c = [], f;
      for (f in g) {
        c.push({key:f, count:g[f]});
      }
      c.sort(function(f, c) {
        return c.count - f.count;
      });
      return c.reduce(function(f, c) {
        return f + ("\n" + c.count + "\t" + c.key);
      }, "");
    };
    e.notUsed = function(c) {
    };
    e.notImplemented = function(c) {
    };
    e.dummyConstructor = function(c) {
    };
    e.abstractMethod = function(c) {
    };
    var d = {};
    e.somewhatImplemented = function(c) {
      d[c] || (d[c] = !0, e.warning("somewhatImplemented: " + c));
    };
    e.unexpected = function(c) {
      e.assert(!1, "Unexpected: " + c);
    };
    e.unexpectedCase = function(c) {
      e.assert(!1, "Unexpected Case: " + c);
    };
  })(m = k.Debug || (k.Debug = {}));
  k.getTicks = function() {
    return performance.now();
  };
  (function(e) {
    function g(c, f) {
      for (var d = 0, x = c.length;d < x;d++) {
        if (c[d] === f) {
          return d;
        }
      }
      c.push(f);
      return c.length - 1;
    }
    e.popManyInto = function(c, f, d) {
      for (var x = f - 1;0 <= x;x--) {
        d[x] = c.pop();
      }
      d.length = f;
    };
    e.popMany = function(c, f) {
      var d = c.length - f, x = c.slice(d, this.length);
      c.length = d;
      return x;
    };
    e.popManyIntoVoid = function(c, f) {
      c.length -= f;
    };
    e.pushMany = function(c, f) {
      for (var d = 0;d < f.length;d++) {
        c.push(f[d]);
      }
    };
    e.top = function(c) {
      return c.length && c[c.length - 1];
    };
    e.last = function(c) {
      return c.length && c[c.length - 1];
    };
    e.peek = function(c) {
      return c[c.length - 1];
    };
    e.indexOf = function(c, f) {
      for (var d = 0, x = c.length;d < x;d++) {
        if (c[d] === f) {
          return d;
        }
      }
      return -1;
    };
    e.equals = function(c, f) {
      if (c.length !== f.length) {
        return !1;
      }
      for (var d = 0;d < c.length;d++) {
        if (c[d] !== f[d]) {
          return !1;
        }
      }
      return !0;
    };
    e.pushUnique = g;
    e.unique = function(c) {
      for (var f = [], d = 0;d < c.length;d++) {
        g(f, c[d]);
      }
      return f;
    };
    e.copyFrom = function(c, f) {
      c.length = 0;
      e.pushMany(c, f);
    };
    e.ensureTypedArrayCapacity = function(c, f) {
      if (c.length < f) {
        var d = c;
        c = new c.constructor(k.IntegerUtilities.nearestPowerOfTwo(f));
        c.set(d, 0);
      }
      return c;
    };
    e.memCopy = function(c, f, d, x, g) {
      void 0 === d && (d = 0);
      void 0 === x && (x = 0);
      void 0 === g && (g = 0);
      0 < x || 0 < g && g < f.length ? (0 >= g && (g = f.length - x), c.set(f.subarray(x, x + g), d)) : c.set(f, d);
    };
    var d = function() {
      function c(f) {
        void 0 === f && (f = 16);
        this._f32 = this._i32 = this._u16 = this._u8 = null;
        this._offset = 0;
        this.ensureCapacity(f);
      }
      c.prototype.reset = function() {
        this._offset = 0;
      };
      Object.defineProperty(c.prototype, "offset", {get:function() {
        return this._offset;
      }, enumerable:!0, configurable:!0});
      c.prototype.getIndex = function(f) {
        return this._offset / f;
      };
      c.prototype.ensureAdditionalCapacity = function(f) {
        this.ensureCapacity(this._offset + f);
      };
      c.prototype.ensureCapacity = function(f) {
        if (!this._u8) {
          this._u8 = new Uint8Array(f);
        } else {
          if (this._u8.length > f) {
            return;
          }
        }
        var c = 2 * this._u8.length;
        c < f && (c = f);
        f = new Uint8Array(c);
        f.set(this._u8, 0);
        this._u8 = f;
        this._u16 = new Uint16Array(f.buffer);
        this._i32 = new Int32Array(f.buffer);
        this._f32 = new Float32Array(f.buffer);
      };
      c.prototype.writeInt = function(f) {
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(f);
      };
      c.prototype.writeIntAt = function(f, c) {
        this.ensureCapacity(c + 4);
        this._i32[c >> 2] = f;
      };
      c.prototype.writeIntUnsafe = function(f) {
        this._i32[this._offset >> 2] = f;
        this._offset += 4;
      };
      c.prototype.writeFloat = function(f) {
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(f);
      };
      c.prototype.writeFloatUnsafe = function(f) {
        this._f32[this._offset >> 2] = f;
        this._offset += 4;
      };
      c.prototype.write4Floats = function(f, c, d, g) {
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(f, c, d, g);
      };
      c.prototype.write4FloatsUnsafe = function(f, c, d, g) {
        var e = this._offset >> 2;
        this._f32[e + 0] = f;
        this._f32[e + 1] = c;
        this._f32[e + 2] = d;
        this._f32[e + 3] = g;
        this._offset += 16;
      };
      c.prototype.write6Floats = function(f, c, d, g, e, b) {
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(f, c, d, g, e, b);
      };
      c.prototype.write6FloatsUnsafe = function(f, c, d, g, e, b) {
        var a = this._offset >> 2;
        this._f32[a + 0] = f;
        this._f32[a + 1] = c;
        this._f32[a + 2] = d;
        this._f32[a + 3] = g;
        this._f32[a + 4] = e;
        this._f32[a + 5] = b;
        this._offset += 24;
      };
      c.prototype.subF32View = function() {
        return this._f32.subarray(0, this._offset >> 2);
      };
      c.prototype.subI32View = function() {
        return this._i32.subarray(0, this._offset >> 2);
      };
      c.prototype.subU16View = function() {
        return this._u16.subarray(0, this._offset >> 1);
      };
      c.prototype.subU8View = function() {
        return this._u8.subarray(0, this._offset);
      };
      c.prototype.hashWords = function(f, c, d) {
        c = this._i32;
        for (var g = 0;g < d;g++) {
          f = (31 * f | 0) + c[g] | 0;
        }
        return f;
      };
      c.prototype.reserve = function(f) {
        f = f + 3 & -4;
        this.ensureCapacity(this._offset + f);
        this._offset += f;
      };
      return c;
    }();
    e.ArrayWriter = d;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
  var b = function() {
    function e(g) {
      this._u8 = new Uint8Array(g);
      this._u16 = new Uint16Array(g);
      this._i32 = new Int32Array(g);
      this._f32 = new Float32Array(g);
      this._offset = 0;
    }
    Object.defineProperty(e.prototype, "offset", {get:function() {
      return this._offset;
    }, enumerable:!0, configurable:!0});
    e.prototype.isEmpty = function() {
      return this._offset === this._u8.length;
    };
    e.prototype.readInt = function() {
      var g = this._i32[this._offset >> 2];
      this._offset += 4;
      return g;
    };
    e.prototype.readFloat = function() {
      var g = this._f32[this._offset >> 2];
      this._offset += 4;
      return g;
    };
    return e;
  }();
  k.ArrayReader = b;
  (function(e) {
    function g(c, f) {
      return Object.prototype.hasOwnProperty.call(c, f);
    }
    function d(c, f) {
      for (var d in f) {
        g(f, d) && (c[d] = f[d]);
      }
    }
    e.boxValue = function(c) {
      return void 0 == c || u(c) ? c : Object(c);
    };
    e.toKeyValueArray = function(c) {
      var f = Object.prototype.hasOwnProperty, d = [], g;
      for (g in c) {
        f.call(c, g) && d.push([g, c[g]]);
      }
      return d;
    };
    e.isPrototypeWriteable = function(c) {
      return Object.getOwnPropertyDescriptor(c, "prototype").writable;
    };
    e.hasOwnProperty = g;
    e.propertyIsEnumerable = function(c, f) {
      return Object.prototype.propertyIsEnumerable.call(c, f);
    };
    e.getPropertyDescriptor = function(c, f) {
      do {
        var d = Object.getOwnPropertyDescriptor(c, f);
        if (d) {
          return d;
        }
        c = Object.getPrototypeOf(c);
      } while (c);
      return null;
    };
    e.hasOwnGetter = function(c, f) {
      var d = Object.getOwnPropertyDescriptor(c, f);
      return !(!d || !d.get);
    };
    e.getOwnGetter = function(c, f) {
      var d = Object.getOwnPropertyDescriptor(c, f);
      return d ? d.get : null;
    };
    e.hasOwnSetter = function(c, f) {
      var d = Object.getOwnPropertyDescriptor(c, f);
      return !(!d || !d.set);
    };
    e.createMap = function() {
      return Object.create(null);
    };
    e.createArrayMap = function() {
      return [];
    };
    e.defineReadOnlyProperty = function(c, f, d) {
      Object.defineProperty(c, f, {value:d, writable:!1, configurable:!0, enumerable:!1});
    };
    e.getOwnPropertyDescriptors = function(c) {
      for (var f = e.createMap(), d = Object.getOwnPropertyNames(c), g = 0;g < d.length;g++) {
        f[d[g]] = Object.getOwnPropertyDescriptor(c, d[g]);
      }
      return f;
    };
    e.cloneObject = function(c) {
      var f = Object.create(Object.getPrototypeOf(c));
      d(f, c);
      return f;
    };
    e.copyProperties = function(c, f) {
      for (var d in f) {
        c[d] = f[d];
      }
    };
    e.copyOwnProperties = d;
    e.copyOwnPropertyDescriptors = function(c, f, d, e, b) {
      void 0 === d && (d = null);
      void 0 === e && (e = !0);
      void 0 === b && (b = !1);
      for (var a in f) {
        if (g(f, a) && (!d || d(a))) {
          var l = Object.getOwnPropertyDescriptor(f, a);
          if (e || !g(c, a)) {
            try {
              b && !1 === l.writable && (l.writable = !0), Object.defineProperty(c, a, l);
            } catch (h) {
              m.assert("Can't define: " + a);
            }
          }
        }
      }
    };
    e.copyPropertiesByList = function(c, f, d) {
      for (var g = 0;g < d.length;g++) {
        var e = d[g];
        c[e] = f[e];
      }
    };
    e.getLatestGetterOrSetterPropertyDescriptor = function(c, f) {
      for (var d = {};c;) {
        var g = Object.getOwnPropertyDescriptor(c, f);
        g && (d.get = d.get || g.get, d.set = d.set || g.set);
        if (d.get && d.set) {
          break;
        }
        c = Object.getPrototypeOf(c);
      }
      return d;
    };
    e.defineNonEnumerableGetterOrSetter = function(c, f, d, g) {
      var b = e.getLatestGetterOrSetterPropertyDescriptor(c, f);
      b.configurable = !0;
      b.enumerable = !1;
      g ? b.get = d : b.set = d;
      Object.defineProperty(c, f, b);
    };
    e.defineNonEnumerableGetter = function(c, f, d) {
      Object.defineProperty(c, f, {get:d, configurable:!0, enumerable:!1});
    };
    e.defineNonEnumerableSetter = function(c, f, d) {
      Object.defineProperty(c, f, {set:d, configurable:!0, enumerable:!1});
    };
    e.defineNonEnumerableProperty = function(c, f, d) {
      Object.defineProperty(c, f, {value:d, writable:!0, configurable:!0, enumerable:!1});
    };
    e.defineNonEnumerableForwardingProperty = function(c, f, d) {
      Object.defineProperty(c, f, {get:l.makeForwardingGetter(d), set:l.makeForwardingSetter(d), writable:!0, configurable:!0, enumerable:!1});
    };
    e.defineNewNonEnumerableProperty = function(c, f, d) {
      e.defineNonEnumerableProperty(c, f, d);
    };
    e.createPublicAliases = function(c, f) {
      for (var d = {value:null, writable:!0, configurable:!0, enumerable:!1}, g = 0;g < f.length;g++) {
        var e = f[g];
        d.value = c[e];
        Object.defineProperty(c, "$Bg" + e, d);
      }
    };
  })(k.ObjectUtilities || (k.ObjectUtilities = {}));
  var l;
  (function(e) {
    e.makeForwardingGetter = function(g) {
      return new Function('return this["' + g + '"]//# sourceURL=fwd-get-' + g + ".as");
    };
    e.makeForwardingSetter = function(g) {
      return new Function("value", 'this["' + g + '"] = value;//# sourceURL=fwd-set-' + g + ".as");
    };
    e.bindSafely = function(g, d) {
      function c() {
        return g.apply(d, arguments);
      }
      c.boundTo = d;
      return c;
    };
  })(l = k.FunctionUtilities || (k.FunctionUtilities = {}));
  (function(e) {
    function g(f) {
      return "string" === typeof f ? '"' + f + '"' : "number" === typeof f || "boolean" === typeof f ? String(f) : f instanceof Array ? "[] " + f.length : typeof f;
    }
    function d(c, d, g) {
      f[0] = c;
      f[1] = d;
      f[2] = g;
      return f.join("");
    }
    function c(f, c, d, g) {
      y[0] = f;
      y[1] = c;
      y[2] = d;
      y[3] = g;
      return y.join("");
    }
    e.repeatString = function(f, c) {
      for (var d = "", g = 0;g < c;g++) {
        d += f;
      }
      return d;
    };
    e.memorySizeToString = function(f) {
      f |= 0;
      return 1024 > f ? f + " B" : 1048576 > f ? (f / 1024).toFixed(2) + "KB" : (f / 1048576).toFixed(2) + "MB";
    };
    e.toSafeString = g;
    e.toSafeArrayString = function(f) {
      for (var c = [], d = 0;d < f.length;d++) {
        c.push(g(f[d]));
      }
      return c.join(", ");
    };
    e.utf8decode = function(f) {
      for (var c = new Uint8Array(4 * f.length), d = 0, g = 0, e = f.length;g < e;g++) {
        var x = f.charCodeAt(g);
        if (127 >= x) {
          c[d++] = x;
        } else {
          if (55296 <= x && 56319 >= x) {
            var y = f.charCodeAt(g + 1);
            56320 <= y && 57343 >= y && (x = ((x & 1023) << 10) + (y & 1023) + 65536, ++g);
          }
          0 !== (x & 4292870144) ? (c[d++] = 248 | x >>> 24 & 3, c[d++] = 128 | x >>> 18 & 63, c[d++] = 128 | x >>> 12 & 63, c[d++] = 128 | x >>> 6 & 63) : 0 !== (x & 4294901760) ? (c[d++] = 240 | x >>> 18 & 7, c[d++] = 128 | x >>> 12 & 63, c[d++] = 128 | x >>> 6 & 63) : 0 !== (x & 4294965248) ? (c[d++] = 224 | x >>> 12 & 15, c[d++] = 128 | x >>> 6 & 63) : c[d++] = 192 | x >>> 6 & 31;
          c[d++] = 128 | x & 63;
        }
      }
      return c.subarray(0, d);
    };
    e.utf8encode = function(f) {
      for (var c = 0, d = "";c < f.length;) {
        var g = f[c++] & 255;
        if (127 >= g) {
          d += String.fromCharCode(g);
        } else {
          var e = 192, x = 5;
          do {
            if ((g & (e >> 1 | 128)) === e) {
              break;
            }
            e = e >> 1 | 128;
            --x;
          } while (0 <= x);
          if (0 >= x) {
            d += String.fromCharCode(g);
          } else {
            for (var g = g & (1 << x) - 1, e = !1, y = 5;y >= x;--y) {
              var b = f[c++];
              if (128 != (b & 192)) {
                e = !0;
                break;
              }
              g = g << 6 | b & 63;
            }
            if (e) {
              for (x = c - (7 - y);x < c;++x) {
                d += String.fromCharCode(f[x] & 255);
              }
            } else {
              d = 65536 <= g ? d + String.fromCharCode(g - 65536 >> 10 & 1023 | 55296, g & 1023 | 56320) : d + String.fromCharCode(g);
            }
          }
        }
      }
      return d;
    };
    e.base64ArrayBuffer = function(f) {
      var g = "";
      f = new Uint8Array(f);
      for (var e = f.byteLength, x = e % 3, e = e - x, y, b, a, I, l = 0;l < e;l += 3) {
        I = f[l] << 16 | f[l + 1] << 8 | f[l + 2], y = (I & 16515072) >> 18, b = (I & 258048) >> 12, a = (I & 4032) >> 6, I &= 63, g += c("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[y], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[b], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[a], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[I]);
      }
      1 == x ? (I = f[e], g += d("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(I & 252) >> 2], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(I & 3) << 4], "==")) : 2 == x && (I = f[e] << 8 | f[e + 1], g += c("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(I & 64512) >> 10], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(I & 1008) >> 4], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(I & 15) << 
      2], "="));
      return g;
    };
    e.escapeString = function(f) {
      void 0 !== f && (f = f.replace(/[^\w$]/gi, "$"), /^\d/.test(f) && (f = "$" + f));
      return f;
    };
    e.fromCharCodeArray = function(f) {
      for (var c = "", d = 0;d < f.length;d += 16384) {
        var g = Math.min(f.length - d, 16384), c = c + String.fromCharCode.apply(null, f.subarray(d, d + g))
      }
      return c;
    };
    e.variableLengthEncodeInt32 = function(f) {
      for (var c = 32 - Math.clz32(f), d = Math.ceil(c / 6), c = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[d], d = d - 1;0 <= d;d--) {
        c += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[f >> 6 * d & 63];
      }
      return c;
    };
    e.toEncoding = function(f) {
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[f];
    };
    e.fromEncoding = function(f) {
      if (65 <= f && 90 >= f) {
        return f - 65;
      }
      if (97 <= f && 122 >= f) {
        return f - 71;
      }
      if (48 <= f && 57 >= f) {
        return f + 4;
      }
      if (36 === f) {
        return 62;
      }
      if (95 === f) {
        return 63;
      }
    };
    e.variableLengthDecodeInt32 = function(f) {
      for (var c = e.fromEncoding(f.charCodeAt(0)), d = 0, g = 0;g < c;g++) {
        var x = 6 * (c - g - 1), d = d | e.fromEncoding(f.charCodeAt(1 + g)) << x
      }
      return d;
    };
    e.trimMiddle = function(f, c) {
      if (f.length <= c) {
        return f;
      }
      var d = c >> 1, g = c - d - 1;
      return f.substr(0, d) + "\u2026" + f.substr(f.length - g, g);
    };
    e.multiple = function(f, c) {
      for (var d = "", g = 0;g < c;g++) {
        d += f;
      }
      return d;
    };
    e.indexOfAny = function(f, c, d) {
      for (var g = f.length, e = 0;e < c.length;e++) {
        var x = f.indexOf(c[e], d);
        0 <= x && (g = Math.min(g, x));
      }
      return g === f.length ? -1 : g;
    };
    var f = Array(3), y = Array(4), x = Array(5), b = Array(6), a = Array(7), l = Array(8), h = Array(9);
    e.concat3 = d;
    e.concat4 = c;
    e.concat5 = function(f, c, d, g, e) {
      x[0] = f;
      x[1] = c;
      x[2] = d;
      x[3] = g;
      x[4] = e;
      return x.join("");
    };
    e.concat6 = function(f, c, d, g, e, x) {
      b[0] = f;
      b[1] = c;
      b[2] = d;
      b[3] = g;
      b[4] = e;
      b[5] = x;
      return b.join("");
    };
    e.concat7 = function(f, c, d, g, e, x, y) {
      a[0] = f;
      a[1] = c;
      a[2] = d;
      a[3] = g;
      a[4] = e;
      a[5] = x;
      a[6] = y;
      return a.join("");
    };
    e.concat8 = function(f, c, d, g, e, x, y, b) {
      l[0] = f;
      l[1] = c;
      l[2] = d;
      l[3] = g;
      l[4] = e;
      l[5] = x;
      l[6] = y;
      l[7] = b;
      return l.join("");
    };
    e.concat9 = function(f, c, d, g, e, x, y, b, a) {
      h[0] = f;
      h[1] = c;
      h[2] = d;
      h[3] = g;
      h[4] = e;
      h[5] = x;
      h[6] = y;
      h[7] = b;
      h[8] = a;
      return h.join("");
    };
  })(k.StringUtilities || (k.StringUtilities = {}));
  (function(e) {
    var g = new Uint8Array([7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]), d = new Int32Array([-680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426, -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162, 1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632, 
    643717713, -373897302, -701558691, 38016083, -660478335, -405537848, 568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784, 1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556, -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222, -722521979, 76029189, -640364487, -421815835, 530742520, -995338651, -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606, -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649, 
    -145523070, -1120210379, 718787259, -343485551]);
    e.hashBytesTo32BitsMD5 = function(c, f, e) {
      var x = 1732584193, b = -271733879, a = -1732584194, l = 271733878, h = e + 72 & -64, v = new Uint8Array(h), r;
      for (r = 0;r < e;++r) {
        v[r] = c[f++];
      }
      v[r++] = 128;
      for (c = h - 8;r < c;) {
        v[r++] = 0;
      }
      v[r++] = e << 3 & 255;
      v[r++] = e >> 5 & 255;
      v[r++] = e >> 13 & 255;
      v[r++] = e >> 21 & 255;
      v[r++] = e >>> 29 & 255;
      v[r++] = 0;
      v[r++] = 0;
      v[r++] = 0;
      c = new Int32Array(16);
      for (r = 0;r < h;) {
        for (e = 0;16 > e;++e, r += 4) {
          c[e] = v[r] | v[r + 1] << 8 | v[r + 2] << 16 | v[r + 3] << 24;
        }
        var n = x;
        f = b;
        var t = a, m = l, q, k;
        for (e = 0;64 > e;++e) {
          16 > e ? (q = f & t | ~f & m, k = e) : 32 > e ? (q = m & f | ~m & t, k = 5 * e + 1 & 15) : 48 > e ? (q = f ^ t ^ m, k = 3 * e + 5 & 15) : (q = t ^ (f | ~m), k = 7 * e & 15);
          var w = m, n = n + q + d[e] + c[k] | 0;
          q = g[e];
          m = t;
          t = f;
          f = f + (n << q | n >>> 32 - q) | 0;
          n = w;
        }
        x = x + n | 0;
        b = b + f | 0;
        a = a + t | 0;
        l = l + m | 0;
      }
      return x;
    };
    e.hashBytesTo32BitsAdler = function(c, f, d) {
      var g = 1, e = 0;
      for (d = f + d;f < d;++f) {
        g = (g + (c[f] & 255)) % 65521, e = (e + g) % 65521;
      }
      return e << 16 | g;
    };
    e.mixHash = function(c, f) {
      return (31 * c | 0) + f | 0;
    };
  })(k.HashUtilities || (k.HashUtilities = {}));
  var r = function() {
    function e() {
    }
    e.seed = function(g) {
      e._state[0] = g;
      e._state[1] = g;
    };
    e.reset = function() {
      e._state[0] = 57005;
      e._state[1] = 48879;
    };
    e.next = function() {
      var g = this._state, d = Math.imul(18273, g[0] & 65535) + (g[0] >>> 16) | 0;
      g[0] = d;
      var c = Math.imul(36969, g[1] & 65535) + (g[1] >>> 16) | 0;
      g[1] = c;
      g = (d << 16) + (c & 65535) | 0;
      return 2.3283064365386963E-10 * (0 > g ? g + 4294967296 : g);
    };
    e._state = new Uint32Array([57005, 48879]);
    return e;
  }();
  k.Random = r;
  Math.random = function() {
    return r.next();
  };
  k.installTimeWarper = function() {
    var e = Date, g = 1428107694580;
    jsGlobal.Date = function(d, c, f, y, x, b, a) {
      switch(arguments.length) {
        case 0:
          return new e(g);
        case 1:
          return new e(d);
        case 2:
          return new e(d, c);
        case 3:
          return new e(d, c, f);
        case 4:
          return new e(d, c, f, y);
        case 5:
          return new e(d, c, f, y, x);
        case 6:
          return new e(d, c, f, y, x, b);
        default:
          return new e(d, c, f, y, x, b, a);
      }
    };
    jsGlobal.Date.now = function() {
      return g += 10;
    };
    jsGlobal.Date.UTC = function() {
      return e.UTC.apply(e, arguments);
    };
  };
  (function() {
    function e() {
      this.id = "$weakmap" + g++;
    }
    if ("function" !== typeof jsGlobal.WeakMap) {
      var g = 0;
      e.prototype = {has:function(d) {
        return d.hasOwnProperty(this.id);
      }, get:function(d, c) {
        return d.hasOwnProperty(this.id) ? d[this.id] : c;
      }, set:function(d, c) {
        Object.defineProperty(d, this.id, {value:c, enumerable:!1, configurable:!0});
      }, delete:function(d) {
        delete d[this.id];
      }};
      jsGlobal.WeakMap = e;
    }
  })();
  b = function() {
    function e() {
      "undefined" !== typeof ShumwayCom && ShumwayCom.getWeakMapKeys ? (this._map = new WeakMap, this._id = 0, this._newAdditions = []) : this._list = [];
    }
    e.prototype.clear = function() {
      this._map ? this._map = new WeakMap() : this._list.length = 0;
    };
    e.prototype.push = function(g) {
      this._map ? (this._map.set(g, this._id++), this._newAdditions.forEach(function(d) {
        d.push(g);
      })) : this._list.push(g);
    };
    e.prototype.remove = function(g) {
      this._map ? this._map.delete(g) : this._list[this._list.indexOf(g)] = null;
    };
    e.prototype.forEach = function(g) {
      if (this._map) {
        var d = [];
        this._newAdditions.push(d);
        var c = this._map, f = ShumwayCom.getWeakMapKeys(c);
        f.sort(function(f, d) {
          return c.get(f) - c.get(d);
        });
        f.forEach(function(f) {
          0 !== f._referenceCount && g(f);
        });
        d.forEach(function(f) {
          0 !== f._referenceCount && g(f);
        });
        this._newAdditions.splice(this._newAdditions.indexOf(d), 1);
      } else {
        for (var d = this._list, e = 0, f = 0;f < d.length;f++) {
          var x = d[f];
          x && (0 === x._referenceCount ? (d[f] = null, e++) : g(x));
        }
        if (16 < e && e > d.length >> 2) {
          e = [];
          for (f = 0;f < d.length;f++) {
            (x = d[f]) && 0 < x._referenceCount && e.push(x);
          }
          this._list = e;
        }
      }
    };
    Object.defineProperty(e.prototype, "length", {get:function() {
      return this._map ? -1 : this._list.length;
    }, enumerable:!0, configurable:!0});
    return e;
  }();
  k.WeakList = b;
  var h;
  (function(e) {
    e.pow2 = function(g) {
      return g === (g | 0) ? 0 > g ? 1 / (1 << -g) : 1 << g : Math.pow(2, g);
    };
    e.clamp = function(g, d, c) {
      return Math.max(d, Math.min(c, g));
    };
    e.roundHalfEven = function(g) {
      if (.5 === Math.abs(g % 1)) {
        var d = Math.floor(g);
        return 0 === d % 2 ? d : Math.ceil(g);
      }
      return Math.round(g);
    };
    e.altTieBreakRound = function(g, d) {
      return .5 !== Math.abs(g % 1) || d ? Math.round(g) : g | 0;
    };
    e.epsilonEquals = function(g, d) {
      return 1E-7 > Math.abs(g - d);
    };
  })(h = k.NumberUtilities || (k.NumberUtilities = {}));
  (function(e) {
    e[e.MaxU16 = 65535] = "MaxU16";
    e[e.MaxI16 = 32767] = "MaxI16";
    e[e.MinI16 = -32768] = "MinI16";
  })(k.Numbers || (k.Numbers = {}));
  var t;
  (function(e) {
    function g(f) {
      return 256 * f << 16 >> 16;
    }
    function d(f) {
      return f / 256;
    }
    var c = new ArrayBuffer(8);
    e.i8 = new Int8Array(c);
    e.u8 = new Uint8Array(c);
    e.i32 = new Int32Array(c);
    e.f32 = new Float32Array(c);
    e.f64 = new Float64Array(c);
    e.nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
    e.floatToInt32 = function(f) {
      e.f32[0] = f;
      return e.i32[0];
    };
    e.int32ToFloat = function(f) {
      e.i32[0] = f;
      return e.f32[0];
    };
    e.swap16 = function(f) {
      return (f & 255) << 8 | f >> 8 & 255;
    };
    e.swap32 = function(f) {
      return (f & 255) << 24 | (f & 65280) << 8 | f >> 8 & 65280 | f >> 24 & 255;
    };
    e.toS8U8 = g;
    e.fromS8U8 = d;
    e.clampS8U8 = function(f) {
      return d(g(f));
    };
    e.toS16 = function(f) {
      return f << 16 >> 16;
    };
    e.bitCount = function(f) {
      f -= f >> 1 & 1431655765;
      f = (f & 858993459) + (f >> 2 & 858993459);
      return 16843009 * (f + (f >> 4) & 252645135) >> 24;
    };
    e.ones = function(f) {
      f -= f >> 1 & 1431655765;
      f = (f & 858993459) + (f >> 2 & 858993459);
      return 16843009 * (f + (f >> 4) & 252645135) >> 24;
    };
    e.trailingZeros = function(f) {
      return e.ones((f & -f) - 1);
    };
    e.getFlags = function(f, c) {
      var d = "";
      for (f = 0;f < c.length;f++) {
        f & 1 << f && (d += c[f] + " ");
      }
      return 0 === d.length ? "" : d.trim();
    };
    e.isPowerOfTwo = function(f) {
      return f && 0 === (f & f - 1);
    };
    e.roundToMultipleOfFour = function(f) {
      return f + 3 & -4;
    };
    e.nearestPowerOfTwo = function(f) {
      f--;
      f |= f >> 1;
      f |= f >> 2;
      f |= f >> 4;
      f |= f >> 8;
      f |= f >> 16;
      f++;
      return f;
    };
    e.roundToMultipleOfPowerOfTwo = function(f, c) {
      var d = (1 << c) - 1;
      return f + d & ~d;
    };
    e.toHEX = function(f) {
      return "0x" + ("00000000" + (0 > f ? 4294967295 + f + 1 : f).toString(16)).substr(-8);
    };
    Math.imul || (Math.imul = function(f, c) {
      var d = f & 65535, g = c & 65535;
      return d * g + ((f >>> 16 & 65535) * g + d * (c >>> 16 & 65535) << 16 >>> 0) | 0;
    });
    Math.clz32 || (Math.clz32 = function(f) {
      f |= f >> 1;
      f |= f >> 2;
      f |= f >> 4;
      f |= f >> 8;
      return 32 - e.ones(f | f >> 16);
    });
  })(t = k.IntegerUtilities || (k.IntegerUtilities = {}));
  (function(e) {
    function g(d, c, f, g, e, b) {
      return (f - d) * (b - c) - (g - c) * (e - d);
    }
    e.pointInPolygon = function(d, c, f) {
      for (var g = 0, e = f.length - 2, b = 0;b < e;b += 2) {
        var a = f[b + 0], l = f[b + 1], h = f[b + 2], v = f[b + 3];
        (l <= c && v > c || l > c && v <= c) && d < a + (c - l) / (v - l) * (h - a) && g++;
      }
      return 1 === (g & 1);
    };
    e.signedArea = g;
    e.counterClockwise = function(d, c, f, e, x, b) {
      return 0 < g(d, c, f, e, x, b);
    };
    e.clockwise = function(d, c, f, e, x, b) {
      return 0 > g(d, c, f, e, x, b);
    };
    e.pointInPolygonInt32 = function(d, c, f) {
      d |= 0;
      c |= 0;
      for (var g = 0, e = f.length - 2, b = 0;b < e;b += 2) {
        var a = f[b + 0], l = f[b + 1], h = f[b + 2], v = f[b + 3];
        (l <= c && v > c || l > c && v <= c) && d < a + (c - l) / (v - l) * (h - a) && g++;
      }
      return 1 === (g & 1);
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
  b = function() {
    function e(g, d) {
      void 0 === g && (g = !1);
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = g;
      this._out = d || e._consoleOut;
      this._outNoNewline = d || e._consoleOutNoNewline;
    }
    Object.defineProperty(e.prototype, "suppressOutput", {get:function() {
      return this._suppressOutput;
    }, set:function(g) {
      this._suppressOutput = g;
    }, enumerable:!0, configurable:!0});
    e.prototype.write = function(g, d) {
      void 0 === g && (g = "");
      void 0 === d && (d = !1);
      this._suppressOutput || this._outNoNewline((d ? this._padding : "") + g);
    };
    e.prototype.writeLn = function(g) {
      void 0 === g && (g = "");
      this._suppressOutput || this._out(this._padding + g);
    };
    e.prototype.writeObject = function(g, d) {
      void 0 === g && (g = "");
      this._suppressOutput || this._out(this._padding + g, d);
    };
    e.prototype.writeTimeLn = function(g) {
      void 0 === g && (g = "");
      this._suppressOutput || this._out(this._padding + performance.now().toFixed(2) + " " + g);
    };
    e.prototype.writeComment = function(g) {
      g = (g || "").split("\n");
      if (1 === g.length) {
        this.writeLn("// " + g[0]);
      } else {
        this.writeLn("/**");
        for (var d = 0;d < g.length;d++) {
          this.writeLn(" * " + g[d]);
        }
        this.writeLn(" */");
      }
    };
    e.prototype.writeLns = function(g) {
      g = (g || "").split("\n");
      for (var d = 0;d < g.length;d++) {
        this.writeLn(g[d]);
      }
    };
    e.prototype.errorLn = function(g) {
      e.logLevel & 1 && this.boldRedLn(g);
    };
    e.prototype.warnLn = function(g) {
      e.logLevel & 2 && this.yellowLn(g);
    };
    e.prototype.debugLn = function(g) {
      e.logLevel & 4 && this.purpleLn(g);
    };
    e.prototype.logLn = function(g) {
      e.logLevel & 8 && this.writeLn(g);
    };
    e.prototype.infoLn = function(g) {
      e.logLevel & 16 && this.writeLn(g);
    };
    e.prototype.yellowLn = function(g) {
      this.colorLn(e.YELLOW, g);
    };
    e.prototype.greenLn = function(g) {
      this.colorLn(e.GREEN, g);
    };
    e.prototype.boldRedLn = function(g) {
      this.colorLn(e.BOLD_RED, g);
    };
    e.prototype.redLn = function(g) {
      this.colorLn(e.RED, g);
    };
    e.prototype.purpleLn = function(g) {
      this.colorLn(e.PURPLE, g);
    };
    e.prototype.colorLn = function(g, d) {
      this._suppressOutput || (inBrowser ? this._out(this._padding + d) : this._out(this._padding + g + d + e.ENDC));
    };
    e.prototype.redLns = function(g) {
      this.colorLns(e.RED, g);
    };
    e.prototype.colorLns = function(g, d) {
      for (var c = (d || "").split("\n"), f = 0;f < c.length;f++) {
        this.colorLn(g, c[f]);
      }
    };
    e.prototype.enter = function(g) {
      this._suppressOutput || this._out(this._padding + g);
      this.indent();
    };
    e.prototype.leaveAndEnter = function(g) {
      this.leave(g);
      this.indent();
    };
    e.prototype.leave = function(g) {
      this.outdent();
      !this._suppressOutput && g && this._out(this._padding + g);
    };
    e.prototype.indent = function() {
      this._padding += this._tab;
    };
    e.prototype.outdent = function() {
      0 < this._padding.length && (this._padding = this._padding.substring(0, this._padding.length - this._tab.length));
    };
    e.prototype.writeArray = function(g, d, c) {
      void 0 === d && (d = !1);
      void 0 === c && (c = !1);
      d = d || !1;
      for (var f = 0, e = g.length;f < e;f++) {
        var x = "";
        d && (x = null === g[f] ? "null" : void 0 === g[f] ? "undefined" : g[f].constructor.name, x += " ");
        var b = c ? "" : ("" + f).padRight(" ", 4);
        this.writeLn(b + x + g[f]);
      }
    };
    e.PURPLE = "\u001b[94m";
    e.YELLOW = "\u001b[93m";
    e.GREEN = "\u001b[92m";
    e.RED = "\u001b[91m";
    e.BOLD_RED = "\u001b[1;91m";
    e.ENDC = "\u001b[0m";
    e.logLevel = 31;
    e._consoleOut = console.log.bind(console);
    e._consoleOutNoNewline = console.log.bind(console);
    return e;
  }();
  k.IndentingWriter = b;
  var q = function() {
    return function(e, g) {
      this.value = e;
      this.next = g;
    };
  }(), b = function() {
    function e(g) {
      this._compare = g;
      this._head = null;
      this._length = 0;
    }
    e.prototype.push = function(g) {
      this._length++;
      if (this._head) {
        var d = this._head, c = null;
        g = new q(g, null);
        for (var f = this._compare;d;) {
          if (0 < f(d.value, g.value)) {
            c ? (g.next = d, c.next = g) : (g.next = this._head, this._head = g);
            return;
          }
          c = d;
          d = d.next;
        }
        c.next = g;
      } else {
        this._head = new q(g, null);
      }
    };
    e.prototype.forEach = function(g) {
      for (var d = this._head, c = null;d;) {
        var f = g(d.value);
        if (f === e.RETURN) {
          break;
        } else {
          f === e.DELETE ? d = c ? c.next = d.next : this._head = this._head.next : (c = d, d = d.next);
        }
      }
    };
    e.prototype.isEmpty = function() {
      return !this._head;
    };
    e.prototype.pop = function() {
      if (this._head) {
        this._length--;
        var g = this._head;
        this._head = this._head.next;
        return g.value;
      }
    };
    e.prototype.contains = function(g) {
      for (var d = this._head;d;) {
        if (d.value === g) {
          return !0;
        }
        d = d.next;
      }
      return !1;
    };
    e.prototype.toString = function() {
      for (var g = "[", d = this._head;d;) {
        g += d.value.toString(), (d = d.next) && (g += ",");
      }
      return g + "]";
    };
    e.RETURN = 1;
    e.DELETE = 2;
    return e;
  }();
  k.SortedList = b;
  b = function() {
    function e(g, d) {
      void 0 === d && (d = 12);
      this.start = this.index = 0;
      this._size = 1 << d;
      this._mask = this._size - 1;
      this.array = new g(this._size);
    }
    e.prototype.get = function(g) {
      return this.array[g];
    };
    e.prototype.forEachInReverse = function(g) {
      if (!this.isEmpty()) {
        for (var d = 0 === this.index ? this._size - 1 : this.index - 1, c = this.start - 1 & this._mask;d !== c && !g(this.array[d], d);) {
          d = 0 === d ? this._size - 1 : d - 1;
        }
      }
    };
    e.prototype.write = function(g) {
      this.array[this.index] = g;
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
  k.CircularBuffer = b;
  (function(e) {
    function g(f) {
      return f + (e.BITS_PER_WORD - 1) >> e.ADDRESS_BITS_PER_WORD << e.ADDRESS_BITS_PER_WORD;
    }
    function d(f, c) {
      f = f || "1";
      c = c || "0";
      for (var d = "", g = 0;g < length;g++) {
        d += this.get(g) ? f : c;
      }
      return d;
    }
    function c(f) {
      for (var c = [], d = 0;d < length;d++) {
        this.get(d) && c.push(f ? f[d] : d);
      }
      return c.join(", ");
    }
    e.ADDRESS_BITS_PER_WORD = 5;
    e.BITS_PER_WORD = 1 << e.ADDRESS_BITS_PER_WORD;
    e.BIT_INDEX_MASK = e.BITS_PER_WORD - 1;
    var f = function() {
      function f(c) {
        this.size = g(c);
        this.dirty = this.count = 0;
        this.length = c;
        this.bits = new Uint32Array(this.size >> e.ADDRESS_BITS_PER_WORD);
      }
      f.prototype.recount = function() {
        if (this.dirty) {
          for (var f = this.bits, c = 0, d = 0, g = f.length;d < g;d++) {
            var e = f[d], e = e - (e >> 1 & 1431655765), e = (e & 858993459) + (e >> 2 & 858993459), c = c + (16843009 * (e + (e >> 4) & 252645135) >> 24)
          }
          this.count = c;
          this.dirty = 0;
        }
      };
      f.prototype.set = function(f) {
        var c = f >> e.ADDRESS_BITS_PER_WORD, d = this.bits[c];
        f = d | 1 << (f & e.BIT_INDEX_MASK);
        this.bits[c] = f;
        this.dirty |= d ^ f;
      };
      f.prototype.setAll = function() {
        for (var f = this.bits, c = 0, d = f.length;c < d;c++) {
          f[c] = 4294967295;
        }
        this.count = this.size;
        this.dirty = 0;
      };
      f.prototype.assign = function(f) {
        this.count = f.count;
        this.dirty = f.dirty;
        this.size = f.size;
        for (var c = 0, d = this.bits.length;c < d;c++) {
          this.bits[c] = f.bits[c];
        }
      };
      f.prototype.clear = function(f) {
        var c = f >> e.ADDRESS_BITS_PER_WORD, d = this.bits[c];
        f = d & ~(1 << (f & e.BIT_INDEX_MASK));
        this.bits[c] = f;
        this.dirty |= d ^ f;
      };
      f.prototype.get = function(f) {
        return 0 !== (this.bits[f >> e.ADDRESS_BITS_PER_WORD] & 1 << (f & e.BIT_INDEX_MASK));
      };
      f.prototype.clearAll = function() {
        for (var f = this.bits, c = 0, d = f.length;c < d;c++) {
          f[c] = 0;
        }
        this.dirty = this.count = 0;
      };
      f.prototype._union = function(f) {
        var c = this.dirty, d = this.bits;
        f = f.bits;
        for (var g = 0, e = d.length;g < e;g++) {
          var x = d[g], y = x | f[g];
          d[g] = y;
          c |= x ^ y;
        }
        this.dirty = c;
      };
      f.prototype.intersect = function(f) {
        var c = this.dirty, d = this.bits;
        f = f.bits;
        for (var g = 0, e = d.length;g < e;g++) {
          var x = d[g], y = x & f[g];
          d[g] = y;
          c |= x ^ y;
        }
        this.dirty = c;
      };
      f.prototype.subtract = function(f) {
        var c = this.dirty, d = this.bits;
        f = f.bits;
        for (var g = 0, e = d.length;g < e;g++) {
          var x = d[g], y = x & ~f[g];
          d[g] = y;
          c |= x ^ y;
        }
        this.dirty = c;
      };
      f.prototype.negate = function() {
        for (var f = this.dirty, c = this.bits, d = 0, g = c.length;d < g;d++) {
          var e = c[d], x = ~e;
          c[d] = x;
          f |= e ^ x;
        }
        this.dirty = f;
      };
      f.prototype.forEach = function(f) {
        for (var c = this.bits, d = 0, g = c.length;d < g;d++) {
          var x = c[d];
          if (x) {
            for (var y = 0;y < e.BITS_PER_WORD;y++) {
              x & 1 << y && f(d * e.BITS_PER_WORD + y);
            }
          }
        }
      };
      f.prototype.toArray = function() {
        for (var f = [], c = this.bits, d = 0, g = c.length;d < g;d++) {
          var x = c[d];
          if (x) {
            for (var y = 0;y < e.BITS_PER_WORD;y++) {
              x & 1 << y && f.push(d * e.BITS_PER_WORD + y);
            }
          }
        }
        return f;
      };
      f.prototype.equals = function(f) {
        if (this.size !== f.size) {
          return !1;
        }
        var c = this.bits;
        f = f.bits;
        for (var d = 0, g = c.length;d < g;d++) {
          if (c[d] !== f[d]) {
            return !1;
          }
        }
        return !0;
      };
      f.prototype.contains = function(f) {
        if (this.size !== f.size) {
          return !1;
        }
        var c = this.bits;
        f = f.bits;
        for (var d = 0, g = c.length;d < g;d++) {
          if ((c[d] | f[d]) !== c[d]) {
            return !1;
          }
        }
        return !0;
      };
      f.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      f.prototype.clone = function() {
        var c = new f(this.length);
        c._union(this);
        return c;
      };
      return f;
    }();
    e.Uint32ArrayBitSet = f;
    var y = function() {
      function f(c) {
        this.dirty = this.count = 0;
        this.size = g(c);
        this.bits = 0;
        this.singleWord = !0;
        this.length = c;
      }
      f.prototype.recount = function() {
        if (this.dirty) {
          var f = this.bits, f = f - (f >> 1 & 1431655765), f = (f & 858993459) + (f >> 2 & 858993459);
          this.count = 0 + (16843009 * (f + (f >> 4) & 252645135) >> 24);
          this.dirty = 0;
        }
      };
      f.prototype.set = function(f) {
        var c = this.bits;
        this.bits = f = c | 1 << (f & e.BIT_INDEX_MASK);
        this.dirty |= c ^ f;
      };
      f.prototype.setAll = function() {
        this.bits = 4294967295;
        this.count = this.size;
        this.dirty = 0;
      };
      f.prototype.assign = function(f) {
        this.count = f.count;
        this.dirty = f.dirty;
        this.size = f.size;
        this.bits = f.bits;
      };
      f.prototype.clear = function(f) {
        var c = this.bits;
        this.bits = f = c & ~(1 << (f & e.BIT_INDEX_MASK));
        this.dirty |= c ^ f;
      };
      f.prototype.get = function(f) {
        return 0 !== (this.bits & 1 << (f & e.BIT_INDEX_MASK));
      };
      f.prototype.clearAll = function() {
        this.dirty = this.count = this.bits = 0;
      };
      f.prototype._union = function(f) {
        var c = this.bits;
        this.bits = f = c | f.bits;
        this.dirty = c ^ f;
      };
      f.prototype.intersect = function(f) {
        var c = this.bits;
        this.bits = f = c & f.bits;
        this.dirty = c ^ f;
      };
      f.prototype.subtract = function(f) {
        var c = this.bits;
        this.bits = f = c & ~f.bits;
        this.dirty = c ^ f;
      };
      f.prototype.negate = function() {
        var f = this.bits, c = ~f;
        this.bits = c;
        this.dirty = f ^ c;
      };
      f.prototype.forEach = function(f) {
        var c = this.bits;
        if (c) {
          for (var d = 0;d < e.BITS_PER_WORD;d++) {
            c & 1 << d && f(d);
          }
        }
      };
      f.prototype.toArray = function() {
        var f = [], c = this.bits;
        if (c) {
          for (var d = 0;d < e.BITS_PER_WORD;d++) {
            c & 1 << d && f.push(d);
          }
        }
        return f;
      };
      f.prototype.equals = function(f) {
        return this.bits === f.bits;
      };
      f.prototype.contains = function(f) {
        var c = this.bits;
        return (c | f.bits) === c;
      };
      f.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      f.prototype.clone = function() {
        var c = new f(this.length);
        c._union(this);
        return c;
      };
      return f;
    }();
    e.Uint32BitSet = y;
    y.prototype.toString = c;
    y.prototype.toBitString = d;
    f.prototype.toString = c;
    f.prototype.toBitString = d;
    e.BitSetFunctor = function(c) {
      var d = 1 === g(c) >> e.ADDRESS_BITS_PER_WORD ? y : f;
      return function() {
        return new d(c);
      };
    };
  })(k.BitSets || (k.BitSets = {}));
  b = function() {
    function e() {
    }
    e.randomStyle = function() {
      e._randomStyleCache || (e._randomStyleCache = "#ff5e3a #ff9500 #ffdb4c #87fc70 #52edc7 #1ad6fd #c644fc #ef4db6 #4a4a4a #dbddde #ff3b30 #ff9500 #ffcc00 #4cd964 #34aadc #007aff #5856d6 #ff2d55 #8e8e93 #c7c7cc #5ad427 #c86edf #d1eefc #e0f8d8 #fb2b69 #f7f7f7 #1d77ef #d6cec3 #55efcb #ff4981 #ffd3e0 #f7f7f7 #ff1300 #1f1f21 #bdbec2 #ff3a2d".split(" "));
      return e._randomStyleCache[e._nextStyle++ % e._randomStyleCache.length];
    };
    e.gradientColor = function(g) {
      return e._gradient[e._gradient.length * h.clamp(g, 0, 1) | 0];
    };
    e.contrastStyle = function(g) {
      g = parseInt(g.substr(1), 16);
      return 128 <= (299 * (g >> 16) + 587 * (g >> 8 & 255) + 114 * (g & 255)) / 1E3 ? "#000000" : "#ffffff";
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
  k.ColorStyle = b;
  b = function() {
    function e(g, d, c, f) {
      this.xMin = g | 0;
      this.yMin = d | 0;
      this.xMax = c | 0;
      this.yMax = f | 0;
    }
    e.FromUntyped = function(g) {
      return new e(g.xMin, g.yMin, g.xMax, g.yMax);
    };
    e.FromRectangle = function(g) {
      return new e(20 * g.x | 0, 20 * g.y | 0, 20 * (g.x + g.width) | 0, 20 * (g.y + g.height) | 0);
    };
    e.prototype.setElements = function(g, d, c, f) {
      this.xMin = g;
      this.yMin = d;
      this.xMax = c;
      this.yMax = f;
    };
    e.prototype.copyFrom = function(g) {
      this.setElements(g.xMin, g.yMin, g.xMax, g.yMax);
    };
    e.prototype.contains = function(g, d) {
      return g < this.xMin !== g < this.xMax && d < this.yMin !== d < this.yMax;
    };
    e.prototype.unionInPlace = function(g) {
      g.isEmpty() || (this.extendByPoint(g.xMin, g.yMin), this.extendByPoint(g.xMax, g.yMax));
    };
    e.prototype.extendByPoint = function(g, d) {
      this.extendByX(g);
      this.extendByY(d);
    };
    e.prototype.extendByX = function(g) {
      134217728 === this.xMin ? this.xMin = this.xMax = g : (this.xMin = Math.min(this.xMin, g), this.xMax = Math.max(this.xMax, g));
    };
    e.prototype.extendByY = function(g) {
      134217728 === this.yMin ? this.yMin = this.yMax = g : (this.yMin = Math.min(this.yMin, g), this.yMax = Math.max(this.yMax, g));
    };
    e.prototype.intersects = function(g) {
      return this.contains(g.xMin, g.yMin) || this.contains(g.xMax, g.yMax);
    };
    e.prototype.isEmpty = function() {
      return this.xMax <= this.xMin || this.yMax <= this.yMin;
    };
    Object.defineProperty(e.prototype, "width", {get:function() {
      return this.xMax - this.xMin;
    }, set:function(g) {
      this.xMax = this.xMin + g;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "height", {get:function() {
      return this.yMax - this.yMin;
    }, set:function(g) {
      this.yMax = this.yMin + g;
    }, enumerable:!0, configurable:!0});
    e.prototype.getBaseWidth = function(g) {
      var d = Math.abs(Math.cos(g));
      g = Math.abs(Math.sin(g));
      return d * (this.xMax - this.xMin) + g * (this.yMax - this.yMin);
    };
    e.prototype.getBaseHeight = function(g) {
      var d = Math.abs(Math.cos(g));
      return Math.abs(Math.sin(g)) * (this.xMax - this.xMin) + d * (this.yMax - this.yMin);
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
  k.Bounds = b;
  b = function() {
    function e(g, d, c, f) {
      m.assert(p(g));
      m.assert(p(d));
      m.assert(p(c));
      m.assert(p(f));
      this._xMin = g | 0;
      this._yMin = d | 0;
      this._xMax = c | 0;
      this._yMax = f | 0;
    }
    e.FromUntyped = function(g) {
      return new e(g.xMin, g.yMin, g.xMax, g.yMax);
    };
    e.FromRectangle = function(g) {
      return new e(20 * g.x | 0, 20 * g.y | 0, 20 * (g.x + g.width) | 0, 20 * (g.y + g.height) | 0);
    };
    e.prototype.setElements = function(g, d, c, f) {
      this.xMin = g;
      this.yMin = d;
      this.xMax = c;
      this.yMax = f;
    };
    e.prototype.copyFrom = function(g) {
      this.setElements(g.xMin, g.yMin, g.xMax, g.yMax);
    };
    e.prototype.contains = function(g, d) {
      return g < this.xMin !== g < this.xMax && d < this.yMin !== d < this.yMax;
    };
    e.prototype.unionInPlace = function(g) {
      g.isEmpty() || (this.extendByPoint(g.xMin, g.yMin), this.extendByPoint(g.xMax, g.yMax));
    };
    e.prototype.extendByPoint = function(g, d) {
      this.extendByX(g);
      this.extendByY(d);
    };
    e.prototype.extendByX = function(g) {
      134217728 === this.xMin ? this.xMin = this.xMax = g : (this.xMin = Math.min(this.xMin, g), this.xMax = Math.max(this.xMax, g));
    };
    e.prototype.extendByY = function(g) {
      134217728 === this.yMin ? this.yMin = this.yMax = g : (this.yMin = Math.min(this.yMin, g), this.yMax = Math.max(this.yMax, g));
    };
    e.prototype.intersects = function(g) {
      return this.contains(g._xMin, g._yMin) || this.contains(g._xMax, g._yMax);
    };
    e.prototype.isEmpty = function() {
      return this._xMax <= this._xMin || this._yMax <= this._yMin;
    };
    Object.defineProperty(e.prototype, "xMin", {get:function() {
      return this._xMin;
    }, set:function(g) {
      m.assert(p(g));
      this._xMin = g;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "yMin", {get:function() {
      return this._yMin;
    }, set:function(g) {
      m.assert(p(g));
      this._yMin = g | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "xMax", {get:function() {
      return this._xMax;
    }, set:function(g) {
      m.assert(p(g));
      this._xMax = g | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "width", {get:function() {
      return this._xMax - this._xMin;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "yMax", {get:function() {
      return this._yMax;
    }, set:function(g) {
      m.assert(p(g));
      this._yMax = g | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(e.prototype, "height", {get:function() {
      return this._yMax - this._yMin;
    }, enumerable:!0, configurable:!0});
    e.prototype.getBaseWidth = function(g) {
      var d = Math.abs(Math.cos(g));
      g = Math.abs(Math.sin(g));
      return d * (this._xMax - this._xMin) + g * (this._yMax - this._yMin);
    };
    e.prototype.getBaseHeight = function(g) {
      var d = Math.abs(Math.cos(g));
      return Math.abs(Math.sin(g)) * (this._xMax - this._xMin) + d * (this._yMax - this._yMin);
    };
    e.prototype.setEmpty = function() {
      this._xMin = this._yMin = this._xMax = this._yMax = 0;
    };
    e.prototype.clone = function() {
      return new e(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    e.prototype.toString = function() {
      return "{ xMin: " + this._xMin + ", yMin: " + this._yMin + ", xMax: " + this._xMax + ", yMax: " + this._yMax + " }";
    };
    e.prototype.assertValid = function() {
    };
    return e;
  }();
  k.DebugBounds = b;
  b = function() {
    function e(g, d, c, f) {
      this.r = g;
      this.g = d;
      this.b = c;
      this.a = f;
    }
    e.FromARGB = function(g) {
      return new e((g >> 16 & 255) / 255, (g >> 8 & 255) / 255, (g >> 0 & 255) / 255, (g >> 24 & 255) / 255);
    };
    e.FromRGBA = function(g) {
      return e.FromARGB(n.RGBAToARGB(g));
    };
    e.prototype.toRGBA = function() {
      return 255 * this.r << 24 | 255 * this.g << 16 | 255 * this.b << 8 | 255 * this.a;
    };
    e.prototype.toCSSStyle = function() {
      return n.rgbaToCSSStyle(this.toRGBA());
    };
    e.prototype.set = function(g) {
      this.r = g.r;
      this.g = g.g;
      this.b = g.b;
      this.a = g.a;
    };
    e.randomColor = function(g) {
      void 0 === g && (g = 1);
      return new e(Math.random(), Math.random(), Math.random(), g);
    };
    e.parseColor = function(g) {
      e.colorCache || (e.colorCache = Object.create(null));
      if (e.colorCache[g]) {
        return e.colorCache[g];
      }
      var d = document.createElement("span");
      document.body.appendChild(d);
      d.style.backgroundColor = g;
      var c = getComputedStyle(d).backgroundColor;
      document.body.removeChild(d);
      (d = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(c)) || (d = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(c));
      c = new e(0, 0, 0, 0);
      c.r = parseFloat(d[1]) / 255;
      c.g = parseFloat(d[2]) / 255;
      c.b = parseFloat(d[3]) / 255;
      c.a = d[4] ? parseFloat(d[4]) / 255 : 1;
      return e.colorCache[g] = c;
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
  k.Color = b;
  var n;
  (function(e) {
    function g(f) {
      var c = f >> 0 & 255, d = f >> 8 & 255, g = f >> 24 & 255;
      f = (Math.imul(f >> 16 & 255, g) + 127) / 255 | 0;
      d = (Math.imul(d, g) + 127) / 255 | 0;
      c = (Math.imul(c, g) + 127) / 255 | 0;
      return g << 24 | f << 16 | d << 8 | c;
    }
    function d() {
      if (!c) {
        c = new Uint8Array(65536);
        for (var f = 0;256 > f;f++) {
          for (var d = 0;256 > d;d++) {
            c[(d << 8) + f] = Math.imul(255, f) / d;
          }
        }
      }
    }
    e.RGBAToARGB = function(f) {
      return f >> 8 & 16777215 | (f & 255) << 24;
    };
    e.ARGBToRGBA = function(f) {
      return f << 8 | f >> 24 & 255;
    };
    e.rgbaToCSSStyle = function(f) {
      return k.StringUtilities.concat9("rgba(", f >> 24 & 255, ",", f >> 16 & 255, ",", f >> 8 & 255, ",", (f & 255) / 255, ")");
    };
    e.cssStyleToRGBA = function(f) {
      if ("#" === f[0]) {
        if (7 === f.length) {
          return parseInt(f.substring(1), 16) << 8 | 255;
        }
      } else {
        if ("r" === f[0]) {
          var c = f.substring(5, f.length - 1).split(",");
          f = parseInt(c[0]);
          var d = parseInt(c[1]), g = parseInt(c[2]), c = parseFloat(c[3]);
          return (f & 255) << 24 | (d & 255) << 16 | (g & 255) << 8 | 255 * c & 255;
        }
      }
      return 4278190335;
    };
    e.hexToRGB = function(f) {
      return parseInt(f.slice(1), 16);
    };
    e.rgbToHex = function(f) {
      return "#" + ("000000" + (f >>> 0).toString(16)).slice(-6);
    };
    e.isValidHexColor = function(f) {
      return /^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(f);
    };
    e.clampByte = function(f) {
      return Math.max(0, Math.min(255, f));
    };
    e.unpremultiplyARGB = function(f) {
      var c = f >> 0 & 255, d = f >> 8 & 255, g = f >> 24 & 255;
      f = Math.imul(255, f >> 16 & 255) / g & 255;
      d = Math.imul(255, d) / g & 255;
      c = Math.imul(255, c) / g & 255;
      return g << 24 | f << 16 | d << 8 | c;
    };
    e.premultiplyARGB = g;
    var c;
    e.ensureUnpremultiplyTable = d;
    e.getUnpremultiplyTable = function() {
      d();
      return c;
    };
    e.tableLookupUnpremultiplyARGB = function(f) {
      f |= 0;
      var d = f >> 24 & 255;
      if (0 === d) {
        return 0;
      }
      if (255 === d) {
        return f;
      }
      var g, e, b = d << 8, a = c;
      e = a[b + (f >> 16 & 255)];
      g = a[b + (f >> 8 & 255)];
      f = a[b + (f >> 0 & 255)];
      return d << 24 | e << 16 | g << 8 | f;
    };
    e.blendPremultipliedBGRA = function(f, c) {
      var d = c & 16711935, g = c >> 8 & 16711935, e, b = f >> 8 & 16711935, a = 256 - (c & 255);
      e = Math.imul(f & 16711935, a) >> 8;
      b = Math.imul(b, a) >> 8;
      return (g + b & 16711935) << 8 | d + e & 16711935;
    };
    var f = t.swap32;
    e.convertImage = function(d, e, b, a) {
      var l = b.length;
      if (d === e) {
        if (b !== a) {
          for (d = 0;d < l;d++) {
            a[d] = b[d];
          }
        }
      } else {
        if (1 === d && 3 === e) {
          for (k.ColorUtilities.ensureUnpremultiplyTable(), d = 0;d < l;d++) {
            var h = b[d];
            e = h & 255;
            if (0 === e) {
              a[d] = 0;
            } else {
              if (255 === e) {
                a[d] = 4278190080 | h >> 8 & 16777215;
              } else {
                var r = h >> 24 & 255, n = h >> 16 & 255, h = h >> 8 & 255, t = e << 8, q = c, h = q[t + h], n = q[t + n], r = q[t + r];
                a[d] = e << 24 | r << 16 | n << 8 | h;
              }
            }
          }
        } else {
          if (2 === d && 3 === e) {
            for (d = 0;d < l;d++) {
              a[d] = f(b[d]);
            }
          } else {
            if (3 === d && 1 === e) {
              for (d = 0;d < l;d++) {
                e = b[d], a[d] = f(g(e & 4278255360 | e >> 16 & 255 | (e & 255) << 16));
              }
            } else {
              for (m.somewhatImplemented("Image Format Conversion: " + v[d] + " -> " + v[e]), d = 0;d < l;d++) {
                a[d] = b[d];
              }
            }
          }
        }
      }
    };
  })(n = k.ColorUtilities || (k.ColorUtilities = {}));
  b = function() {
    function e(g) {
      void 0 === g && (g = 32);
      this._list = [];
      this._maxSize = g;
    }
    e.prototype.acquire = function(g) {
      if (e._enabled) {
        for (var d = this._list, c = 0;c < d.length;c++) {
          var f = d[c];
          if (f.byteLength >= g) {
            return d.splice(c, 1), f;
          }
        }
      }
      return new ArrayBuffer(g);
    };
    e.prototype.release = function(g) {
      if (e._enabled) {
        var d = this._list;
        d.length === this._maxSize && d.shift();
        d.push(g);
      }
    };
    e.prototype.ensureUint8ArrayLength = function(g, d) {
      if (g.length >= d) {
        return g;
      }
      var c = Math.max(g.length + d, (3 * g.length >> 1) + 1), c = new Uint8Array(this.acquire(c), 0, c);
      c.set(g);
      this.release(g.buffer);
      return c;
    };
    e.prototype.ensureFloat64ArrayLength = function(g, d) {
      if (g.length >= d) {
        return g;
      }
      var c = Math.max(g.length + d, (3 * g.length >> 1) + 1), c = new Float64Array(this.acquire(c * Float64Array.BYTES_PER_ELEMENT), 0, c);
      c.set(g);
      this.release(g.buffer);
      return c;
    };
    e._enabled = !0;
    return e;
  }();
  k.ArrayBufferPool = b;
  (function(e) {
    (function(g) {
      g[g.EXTERNAL_INTERFACE_FEATURE = 1] = "EXTERNAL_INTERFACE_FEATURE";
      g[g.CLIPBOARD_FEATURE = 2] = "CLIPBOARD_FEATURE";
      g[g.SHAREDOBJECT_FEATURE = 3] = "SHAREDOBJECT_FEATURE";
      g[g.VIDEO_FEATURE = 4] = "VIDEO_FEATURE";
      g[g.SOUND_FEATURE = 5] = "SOUND_FEATURE";
      g[g.NETCONNECTION_FEATURE = 6] = "NETCONNECTION_FEATURE";
    })(e.Feature || (e.Feature = {}));
    (function(g) {
      g[g.AVM1_ERROR = 1] = "AVM1_ERROR";
      g[g.AVM2_ERROR = 2] = "AVM2_ERROR";
    })(e.ErrorTypes || (e.ErrorTypes = {}));
    (function(g) {
      g[g.LoadSource = 0] = "LoadSource";
      g[g.LoadWhitelistAllowed = 1] = "LoadWhitelistAllowed";
      g[g.LoadWhitelistDenied = 2] = "LoadWhitelistDenied";
      g[g.StreamAllowed = 3] = "StreamAllowed";
      g[g.StreamDenied = 4] = "StreamDenied";
      g[g.StreamCrossdomain = 5] = "StreamCrossdomain";
    })(e.LoadResource || (e.LoadResource = {}));
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
  k.registerCSSFont = function(e, g, d) {
    if (inBrowser) {
      var c = document.head;
      c.insertBefore(document.createElement("style"), c.firstChild);
      c = document.styleSheets[0];
      g = "@font-face{font-family:swffont" + e + ";src:url(data:font/opentype;base64," + k.StringUtilities.base64ArrayBuffer(g.buffer) + ")}";
      c.insertRule(g, c.cssRules.length);
      d && (d = document.createElement("div"), d.style.fontFamily = "swffont" + e, d.innerHTML = "hello", document.body.appendChild(d), document.body.removeChild(d));
    } else {
      m.warning("Cannot register CSS font outside the browser");
    }
  };
  k.registerFallbackFont = function() {
    var e = document.styleSheets[0];
    e.insertRule('@font-face{font-family:AdobeBlank;src:url("data:font/opentype;base64,T1RUTwAKAIAAAwAgQ0ZGIDTeCDQAACFkAAAZPERTSUcAAAABAABKqAAAAAhPUy8yAF+xmwAAARAAAABgY21hcCRDbtEAAAdcAAAZ6GhlYWQFl9tDAAAArAAAADZoaGVhB1oD7wAAAOQAAAAkaG10eAPoAHwAADqgAAAQBm1heHAIAVAAAAABCAAAAAZuYW1lIE0HkgAAAXAAAAXrcG9zdP+4ADIAACFEAAAAIAABAAAAAQuFfcPHtV8PPPUAAwPoAAAAANFMRfMAAAAA0UxF8wB8/4gDbANwAAAAAwACAAAAAAAAAAEAAANw/4gAAAPoAHwAfANsAAEAAAAAAAAAAAAAAAAAAAACAABQAAgBAAAAAwPoAZAABQAAAooCWAAAAEsCigJYAAABXgAyANwAAAAAAAAAAAAAAAD3/67/+9///w/gAD8AAAAAQURCTwBAAAD//wNw/4gAAANwAHhgLwH/AAAAAAAAAAAAAAAgAAAAAAARANIAAQAAAAAAAQALAAAAAQAAAAAAAgAHAAsAAQAAAAAAAwAbABIAAQAAAAAABAALAAAAAQAAAAAABQA6AC0AAQAAAAAABgAKAGcAAwABBAkAAACUAHEAAwABBAkAAQAWAQUAAwABBAkAAgAOARsAAwABBAkAAwA2ASkAAwABBAkABAAWAQUAAwABBAkABQB0AV8AAwABBAkABgAUAdMAAwABBAkACAA0AecAAwABBAkACwA0AhsAAwABBAkADQKWAk8AAwABBAkADgA0BOVBZG9iZSBCbGFua1JlZ3VsYXIxLjA0NTtBREJPO0Fkb2JlQmxhbms7QURPQkVWZXJzaW9uIDEuMDQ1O1BTIDEuMDQ1O2hvdGNvbnYgMS4wLjgyO21ha2VvdGYubGliMi41LjYzNDA2QWRvYmVCbGFuawBDAG8AcAB5AHIAaQBnAGgAdAAgAKkAIAAyADAAMQAzACwAIAAyADAAMQA1ACAAQQBkAG8AYgBlACAAUwB5AHMAdABlAG0AcwAgAEkAbgBjAG8AcgBwAG8AcgBhAHQAZQBkACAAKABoAHQAdABwADoALwAvAHcAdwB3AC4AYQBkAG8AYgBlAC4AYwBvAG0ALwApAC4AQQBkAG8AYgBlACAAQgBsAGEAbgBrAFIAZQBnAHUAbABhAHIAMQAuADAANAA1ADsAQQBEAEIATwA7AEEAZABvAGIAZQBCAGwAYQBuAGsAOwBBAEQATwBCAEUAVgBlAHIAcwBpAG8AbgAgADEALgAwADQANQA7AFAAUwAgADEALgAwADQANQA7AGgAbwB0AGMAbwBuAHYAIAAxAC4AMAAuADgAMgA7AG0AYQBrAGUAbwB0AGYALgBsAGkAYgAyAC4ANQAuADYAMwA0ADAANgBBAGQAbwBiAGUAQgBsAGEAbgBrAEEAZABvAGIAZQAgAFMAeQBzAHQAZQBtAHMAIABJAG4AYwBvAHIAcABvAHIAYQB0AGUAZABoAHQAdABwADoALwAvAHcAdwB3AC4AYQBkAG8AYgBlAC4AYwBvAG0ALwB0AHkAcABlAC8AVABoAGkAcwAgAEYAbwBuAHQAIABTAG8AZgB0AHcAYQByAGUAIABpAHMAIABsAGkAYwBlAG4AcwBlAGQAIAB1AG4AZABlAHIAIAB0AGgAZQAgAFMASQBMACAATwBwAGUAbgAgAEYAbwBuAHQAIABMAGkAYwBlAG4AcwBlACwAIABWAGUAcgBzAGkAbwBuACAAMQAuADEALgAgAFQAaABpAHMAIABGAG8AbgB0ACAAUwBvAGYAdAB3AGEAcgBlACAAaQBzACAAZABpAHMAdAByAGkAYgB1AHQAZQBkACAAbwBuACAAYQBuACAAIgBBAFMAIABJAFMAIgAgAEIAQQBTAEkAUwAsACAAVwBJAFQASABPAFUAVAAgAFcAQQBSAFIAQQBOAFQASQBFAFMAIABPAFIAIABDAE8ATgBEAEkAVABJAE8ATgBTACAATwBGACAAQQBOAFkAIABLAEkATgBEACwAIABlAGkAdABoAGUAcgAgAGUAeABwAHIAZQBzAHMAIABvAHIAIABpAG0AcABsAGkAZQBkAC4AIABTAGUAZQAgAHQAaABlACAAUwBJAEwAIABPAHAAZQBuACAARgBvAG4AdAAgAEwAaQBjAGUAbgBzAGUAIABmAG8AcgAgAHQAaABlACAAcwBwAGUAYwBpAGYAaQBjACAAbABhAG4AZwB1AGEAZwBlACwAIABwAGUAcgBtAGkAcwBzAGkAbwBuAHMAIABhAG4AZAAgAGwAaQBtAGkAdABhAHQAaQBvAG4AcwAgAGcAbwB2AGUAcgBuAGkAbgBnACAAeQBvAHUAcgAgAHUAcwBlACAAbwBmACAAdABoAGkAcwAgAEYAbwBuAHQAIABTAG8AZgB0AHcAYQByAGUALgBoAHQAdABwADoALwAvAHMAYwByAGkAcAB0AHMALgBzAGkAbAAuAG8AcgBnAC8ATwBGAEwAAAAABQAAAAMAAAA4AAAABAAAAFgAAQAAAAAALAADAAEAAAA4AAMACgAAAFgABgAMAAAAAAABAAAABAAgAAAABAAEAAEAAAf///8AAAAA//8AAQABAAAAAAAMAAAAABmQAAAAAAAAAiAAAAAAAAAH/wAAAAEAAAgAAAAP/wAAAAEAABAAAAAX/wAAAAEAABgAAAAf/wAAAAEAACAAAAAn/wAAAAEAACgAAAAv/wAAAAEAADAAAAA3/wAAAAEAADgAAAA//wAAAAEAAEAAAABH/wAAAAEAAEgAAABP/wAAAAEAAFAAAABX/wAAAAEAAFgAAABf/wAAAAEAAGAAAABn/wAAAAEAAGgAAABv/wAAAAEAAHAAAAB3/wAAAAEAAHgAAAB//wAAAAEAAIAAAACH/wAAAAEAAIgAAACP/wAAAAEAAJAAAACX/wAAAAEAAJgAAACf/wAAAAEAAKAAAACn/wAAAAEAAKgAAACv/wAAAAEAALAAAAC3/wAAAAEAALgAAAC//wAAAAEAAMAAAADH/wAAAAEAAMgAAADP/wAAAAEAANAAAADX/wAAAAEAAOAAAADn/wAAAAEAAOgAAADv/wAAAAEAAPAAAAD3/wAAAAEAAPgAAAD9zwAAAAEAAP3wAAD//QAABfEAAQAAAAEH/wAAAAEAAQgAAAEP/wAAAAEAARAAAAEX/wAAAAEAARgAAAEf/wAAAAEAASAAAAEn/wAAAAEAASgAAAEv/wAAAAEAATAAAAE3/wAAAAEAATgAAAE//wAAAAEAAUAAAAFH/wAAAAEAAUgAAAFP/wAAAAEAAVAAAAFX/wAAAAEAAVgAAAFf/wAAAAEAAWAAAAFn/wAAAAEAAWgAAAFv/wAAAAEAAXAAAAF3/wAAAAEAAXgAAAF//wAAAAEAAYAAAAGH/wAAAAEAAYgAAAGP/wAAAAEAAZAAAAGX/wAAAAEAAZgAAAGf/wAAAAEAAaAAAAGn/wAAAAEAAagAAAGv/wAAAAEAAbAAAAG3/wAAAAEAAbgAAAG//wAAAAEAAcAAAAHH/wAAAAEAAcgAAAHP/wAAAAEAAdAAAAHX/wAAAAEAAdgAAAHf/wAAAAEAAeAAAAHn/wAAAAEAAegAAAHv/wAAAAEAAfAAAAH3/wAAAAEAAfgAAAH//QAAAAEAAgAAAAIH/wAAAAEAAggAAAIP/wAAAAEAAhAAAAIX/wAAAAEAAhgAAAIf/wAAAAEAAiAAAAIn/wAAAAEAAigAAAIv/wAAAAEAAjAAAAI3/wAAAAEAAjgAAAI//wAAAAEAAkAAAAJH/wAAAAEAAkgAAAJP/wAAAAEAAlAAAAJX/wAAAAEAAlgAAAJf/wAAAAEAAmAAAAJn/wAAAAEAAmgAAAJv/wAAAAEAAnAAAAJ3/wAAAAEAAngAAAJ//wAAAAEAAoAAAAKH/wAAAAEAAogAAAKP/wAAAAEAApAAAAKX/wAAAAEAApgAAAKf/wAAAAEAAqAAAAKn/wAAAAEAAqgAAAKv/wAAAAEAArAAAAK3/wAAAAEAArgAAAK//wAAAAEAAsAAAALH/wAAAAEAAsgAAALP/wAAAAEAAtAAAALX/wAAAAEAAtgAAALf/wAAAAEAAuAAAALn/wAAAAEAAugAAALv/wAAAAEAAvAAAAL3/wAAAAEAAvgAAAL//QAAAAEAAwAAAAMH/wAAAAEAAwgAAAMP/wAAAAEAAxAAAAMX/wAAAAEAAxgAAAMf/wAAAAEAAyAAAAMn/wAAAAEAAygAAAMv/wAAAAEAAzAAAAM3/wAAAAEAAzgAAAM//wAAAAEAA0AAAANH/wAAAAEAA0gAAANP/wAAAAEAA1AAAANX/wAAAAEAA1gAAANf/wAAAAEAA2AAAANn/wAAAAEAA2gAAANv/wAAAAEAA3AAAAN3/wAAAAEAA3gAAAN//wAAAAEAA4AAAAOH/wAAAAEAA4gAAAOP/wAAAAEAA5AAAAOX/wAAAAEAA5gAAAOf/wAAAAEAA6AAAAOn/wAAAAEAA6gAAAOv/wAAAAEAA7AAAAO3/wAAAAEAA7gAAAO//wAAAAEAA8AAAAPH/wAAAAEAA8gAAAPP/wAAAAEAA9AAAAPX/wAAAAEAA9gAAAPf/wAAAAEAA+AAAAPn/wAAAAEAA+gAAAPv/wAAAAEAA/AAAAP3/wAAAAEAA/gAAAP//QAAAAEABAAAAAQH/wAAAAEABAgAAAQP/wAAAAEABBAAAAQX/wAAAAEABBgAAAQf/wAAAAEABCAAAAQn/wAAAAEABCgAAAQv/wAAAAEABDAAAAQ3/wAAAAEABDgAAAQ//wAAAAEABEAAAARH/wAAAAEABEgAAARP/wAAAAEABFAAAARX/wAAAAEABFgAAARf/wAAAAEABGAAAARn/wAAAAEABGgAAARv/wAAAAEABHAAAAR3/wAAAAEABHgAAAR//wAAAAEABIAAAASH/wAAAAEABIgAAASP/wAAAAEABJAAAASX/wAAAAEABJgAAASf/wAAAAEABKAAAASn/wAAAAEABKgAAASv/wAAAAEABLAAAAS3/wAAAAEABLgAAAS//wAAAAEABMAAAATH/wAAAAEABMgAAATP/wAAAAEABNAAAATX/wAAAAEABNgAAATf/wAAAAEABOAAAATn/wAAAAEABOgAAATv/wAAAAEABPAAAAT3/wAAAAEABPgAAAT//QAAAAEABQAAAAUH/wAAAAEABQgAAAUP/wAAAAEABRAAAAUX/wAAAAEABRgAAAUf/wAAAAEABSAAAAUn/wAAAAEABSgAAAUv/wAAAAEABTAAAAU3/wAAAAEABTgAAAU//wAAAAEABUAAAAVH/wAAAAEABUgAAAVP/wAAAAEABVAAAAVX/wAAAAEABVgAAAVf/wAAAAEABWAAAAVn/wAAAAEABWgAAAVv/wAAAAEABXAAAAV3/wAAAAEABXgAAAV//wAAAAEABYAAAAWH/wAAAAEABYgAAAWP/wAAAAEABZAAAAWX/wAAAAEABZgAAAWf/wAAAAEABaAAAAWn/wAAAAEABagAAAWv/wAAAAEABbAAAAW3/wAAAAEABbgAAAW//wAAAAEABcAAAAXH/wAAAAEABcgAAAXP/wAAAAEABdAAAAXX/wAAAAEABdgAAAXf/wAAAAEABeAAAAXn/wAAAAEABegAAAXv/wAAAAEABfAAAAX3/wAAAAEABfgAAAX//QAAAAEABgAAAAYH/wAAAAEABggAAAYP/wAAAAEABhAAAAYX/wAAAAEABhgAAAYf/wAAAAEABiAAAAYn/wAAAAEABigAAAYv/wAAAAEABjAAAAY3/wAAAAEABjgAAAY//wAAAAEABkAAAAZH/wAAAAEABkgAAAZP/wAAAAEABlAAAAZX/wAAAAEABlgAAAZf/wAAAAEABmAAAAZn/wAAAAEABmgAAAZv/wAAAAEABnAAAAZ3/wAAAAEABngAAAZ//wAAAAEABoAAAAaH/wAAAAEABogAAAaP/wAAAAEABpAAAAaX/wAAAAEABpgAAAaf/wAAAAEABqAAAAan/wAAAAEABqgAAAav/wAAAAEABrAAAAa3/wAAAAEABrgAAAa//wAAAAEABsAAAAbH/wAAAAEABsgAAAbP/wAAAAEABtAAAAbX/wAAAAEABtgAAAbf/wAAAAEABuAAAAbn/wAAAAEABugAAAbv/wAAAAEABvAAAAb3/wAAAAEABvgAAAb//QAAAAEABwAAAAcH/wAAAAEABwgAAAcP/wAAAAEABxAAAAcX/wAAAAEABxgAAAcf/wAAAAEAByAAAAcn/wAAAAEABygAAAcv/wAAAAEABzAAAAc3/wAAAAEABzgAAAc//wAAAAEAB0AAAAdH/wAAAAEAB0gAAAdP/wAAAAEAB1AAAAdX/wAAAAEAB1gAAAdf/wAAAAEAB2AAAAdn/wAAAAEAB2gAAAdv/wAAAAEAB3AAAAd3/wAAAAEAB3gAAAd//wAAAAEAB4AAAAeH/wAAAAEAB4gAAAeP/wAAAAEAB5AAAAeX/wAAAAEAB5gAAAef/wAAAAEAB6AAAAen/wAAAAEAB6gAAAev/wAAAAEAB7AAAAe3/wAAAAEAB7gAAAe//wAAAAEAB8AAAAfH/wAAAAEAB8gAAAfP/wAAAAEAB9AAAAfX/wAAAAEAB9gAAAff/wAAAAEAB+AAAAfn/wAAAAEAB+gAAAfv/wAAAAEAB/AAAAf3/wAAAAEAB/gAAAf//QAAAAEACAAAAAgH/wAAAAEACAgAAAgP/wAAAAEACBAAAAgX/wAAAAEACBgAAAgf/wAAAAEACCAAAAgn/wAAAAEACCgAAAgv/wAAAAEACDAAAAg3/wAAAAEACDgAAAg//wAAAAEACEAAAAhH/wAAAAEACEgAAAhP/wAAAAEACFAAAAhX/wAAAAEACFgAAAhf/wAAAAEACGAAAAhn/wAAAAEACGgAAAhv/wAAAAEACHAAAAh3/wAAAAEACHgAAAh//wAAAAEACIAAAAiH/wAAAAEACIgAAAiP/wAAAAEACJAAAAiX/wAAAAEACJgAAAif/wAAAAEACKAAAAin/wAAAAEACKgAAAiv/wAAAAEACLAAAAi3/wAAAAEACLgAAAi//wAAAAEACMAAAAjH/wAAAAEACMgAAAjP/wAAAAEACNAAAAjX/wAAAAEACNgAAAjf/wAAAAEACOAAAAjn/wAAAAEACOgAAAjv/wAAAAEACPAAAAj3/wAAAAEACPgAAAj//QAAAAEACQAAAAkH/wAAAAEACQgAAAkP/wAAAAEACRAAAAkX/wAAAAEACRgAAAkf/wAAAAEACSAAAAkn/wAAAAEACSgAAAkv/wAAAAEACTAAAAk3/wAAAAEACTgAAAk//wAAAAEACUAAAAlH/wAAAAEACUgAAAlP/wAAAAEACVAAAAlX/wAAAAEACVgAAAlf/wAAAAEACWAAAAln/wAAAAEACWgAAAlv/wAAAAEACXAAAAl3/wAAAAEACXgAAAl//wAAAAEACYAAAAmH/wAAAAEACYgAAAmP/wAAAAEACZAAAAmX/wAAAAEACZgAAAmf/wAAAAEACaAAAAmn/wAAAAEACagAAAmv/wAAAAEACbAAAAm3/wAAAAEACbgAAAm//wAAAAEACcAAAAnH/wAAAAEACcgAAAnP/wAAAAEACdAAAAnX/wAAAAEACdgAAAnf/wAAAAEACeAAAAnn/wAAAAEACegAAAnv/wAAAAEACfAAAAn3/wAAAAEACfgAAAn//QAAAAEACgAAAAoH/wAAAAEACggAAAoP/wAAAAEAChAAAAoX/wAAAAEAChgAAAof/wAAAAEACiAAAAon/wAAAAEACigAAAov/wAAAAEACjAAAAo3/wAAAAEACjgAAAo//wAAAAEACkAAAApH/wAAAAEACkgAAApP/wAAAAEAClAAAApX/wAAAAEAClgAAApf/wAAAAEACmAAAApn/wAAAAEACmgAAApv/wAAAAEACnAAAAp3/wAAAAEACngAAAp//wAAAAEACoAAAAqH/wAAAAEACogAAAqP/wAAAAEACpAAAAqX/wAAAAEACpgAAAqf/wAAAAEACqAAAAqn/wAAAAEACqgAAAqv/wAAAAEACrAAAAq3/wAAAAEACrgAAAq//wAAAAEACsAAAArH/wAAAAEACsgAAArP/wAAAAEACtAAAArX/wAAAAEACtgAAArf/wAAAAEACuAAAArn/wAAAAEACugAAArv/wAAAAEACvAAAAr3/wAAAAEACvgAAAr//QAAAAEACwAAAAsH/wAAAAEACwgAAAsP/wAAAAEACxAAAAsX/wAAAAEACxgAAAsf/wAAAAEACyAAAAsn/wAAAAEACygAAAsv/wAAAAEACzAAAAs3/wAAAAEACzgAAAs//wAAAAEAC0AAAAtH/wAAAAEAC0gAAAtP/wAAAAEAC1AAAAtX/wAAAAEAC1gAAAtf/wAAAAEAC2AAAAtn/wAAAAEAC2gAAAtv/wAAAAEAC3AAAAt3/wAAAAEAC3gAAAt//wAAAAEAC4AAAAuH/wAAAAEAC4gAAAuP/wAAAAEAC5AAAAuX/wAAAAEAC5gAAAuf/wAAAAEAC6AAAAun/wAAAAEAC6gAAAuv/wAAAAEAC7AAAAu3/wAAAAEAC7gAAAu//wAAAAEAC8AAAAvH/wAAAAEAC8gAAAvP/wAAAAEAC9AAAAvX/wAAAAEAC9gAAAvf/wAAAAEAC+AAAAvn/wAAAAEAC+gAAAvv/wAAAAEAC/AAAAv3/wAAAAEAC/gAAAv//QAAAAEADAAAAAwH/wAAAAEADAgAAAwP/wAAAAEADBAAAAwX/wAAAAEADBgAAAwf/wAAAAEADCAAAAwn/wAAAAEADCgAAAwv/wAAAAEADDAAAAw3/wAAAAEADDgAAAw//wAAAAEADEAAAAxH/wAAAAEADEgAAAxP/wAAAAEADFAAAAxX/wAAAAEADFgAAAxf/wAAAAEADGAAAAxn/wAAAAEADGgAAAxv/wAAAAEADHAAAAx3/wAAAAEADHgAAAx//wAAAAEADIAAAAyH/wAAAAEADIgAAAyP/wAAAAEADJAAAAyX/wAAAAEADJgAAAyf/wAAAAEADKAAAAyn/wAAAAEADKgAAAyv/wAAAAEADLAAAAy3/wAAAAEADLgAAAy//wAAAAEADMAAAAzH/wAAAAEADMgAAAzP/wAAAAEADNAAAAzX/wAAAAEADNgAAAzf/wAAAAEADOAAAAzn/wAAAAEADOgAAAzv/wAAAAEADPAAAAz3/wAAAAEADPgAAAz//QAAAAEADQAAAA0H/wAAAAEADQgAAA0P/wAAAAEADRAAAA0X/wAAAAEADRgAAA0f/wAAAAEADSAAAA0n/wAAAAEADSgAAA0v/wAAAAEADTAAAA03/wAAAAEADTgAAA0//wAAAAEADUAAAA1H/wAAAAEADUgAAA1P/wAAAAEADVAAAA1X/wAAAAEADVgAAA1f/wAAAAEADWAAAA1n/wAAAAEADWgAAA1v/wAAAAEADXAAAA13/wAAAAEADXgAAA1//wAAAAEADYAAAA2H/wAAAAEADYgAAA2P/wAAAAEADZAAAA2X/wAAAAEADZgAAA2f/wAAAAEADaAAAA2n/wAAAAEADagAAA2v/wAAAAEADbAAAA23/wAAAAEADbgAAA2//wAAAAEADcAAAA3H/wAAAAEADcgAAA3P/wAAAAEADdAAAA3X/wAAAAEADdgAAA3f/wAAAAEADeAAAA3n/wAAAAEADegAAA3v/wAAAAEADfAAAA33/wAAAAEADfgAAA3//QAAAAEADgAAAA4H/wAAAAEADggAAA4P/wAAAAEADhAAAA4X/wAAAAEADhgAAA4f/wAAAAEADiAAAA4n/wAAAAEADigAAA4v/wAAAAEADjAAAA43/wAAAAEADjgAAA4//wAAAAEADkAAAA5H/wAAAAEADkgAAA5P/wAAAAEADlAAAA5X/wAAAAEADlgAAA5f/wAAAAEADmAAAA5n/wAAAAEADmgAAA5v/wAAAAEADnAAAA53/wAAAAEADngAAA5//wAAAAEADoAAAA6H/wAAAAEADogAAA6P/wAAAAEADpAAAA6X/wAAAAEADpgAAA6f/wAAAAEADqAAAA6n/wAAAAEADqgAAA6v/wAAAAEADrAAAA63/wAAAAEADrgAAA6//wAAAAEADsAAAA7H/wAAAAEADsgAAA7P/wAAAAEADtAAAA7X/wAAAAEADtgAAA7f/wAAAAEADuAAAA7n/wAAAAEADugAAA7v/wAAAAEADvAAAA73/wAAAAEADvgAAA7//QAAAAEADwAAAA8H/wAAAAEADwgAAA8P/wAAAAEADxAAAA8X/wAAAAEADxgAAA8f/wAAAAEADyAAAA8n/wAAAAEADygAAA8v/wAAAAEADzAAAA83/wAAAAEADzgAAA8//wAAAAEAD0AAAA9H/wAAAAEAD0gAAA9P/wAAAAEAD1AAAA9X/wAAAAEAD1gAAA9f/wAAAAEAD2AAAA9n/wAAAAEAD2gAAA9v/wAAAAEAD3AAAA93/wAAAAEAD3gAAA9//wAAAAEAD4AAAA+H/wAAAAEAD4gAAA+P/wAAAAEAD5AAAA+X/wAAAAEAD5gAAA+f/wAAAAEAD6AAAA+n/wAAAAEAD6gAAA+v/wAAAAEAD7AAAA+3/wAAAAEAD7gAAA+//wAAAAEAD8AAAA/H/wAAAAEAD8gAAA/P/wAAAAEAD9AAAA/X/wAAAAEAD9gAAA/f/wAAAAEAD+AAAA/n/wAAAAEAD+gAAA/v/wAAAAEAD/AAAA/3/wAAAAEAD/gAAA///QAAAAEAEAAAABAH/wAAAAEAEAgAABAP/wAAAAEAEBAAABAX/wAAAAEAEBgAABAf/wAAAAEAECAAABAn/wAAAAEAECgAABAv/wAAAAEAEDAAABA3/wAAAAEAEDgAABA//wAAAAEAEEAAABBH/wAAAAEAEEgAABBP/wAAAAEAEFAAABBX/wAAAAEAEFgAABBf/wAAAAEAEGAAABBn/wAAAAEAEGgAABBv/wAAAAEAEHAAABB3/wAAAAEAEHgAABB//wAAAAEAEIAAABCH/wAAAAEAEIgAABCP/wAAAAEAEJAAABCX/wAAAAEAEJgAABCf/wAAAAEAEKAAABCn/wAAAAEAEKgAABCv/wAAAAEAELAAABC3/wAAAAEAELgAABC//wAAAAEAEMAAABDH/wAAAAEAEMgAABDP/wAAAAEAENAAABDX/wAAAAEAENgAABDf/wAAAAEAEOAAABDn/wAAAAEAEOgAABDv/wAAAAEAEPAAABD3/wAAAAEAEPgAABD//QAAAAEAAwAAAAAAAP+1ADIAAAAAAAAAAAAAAAAAAAAAAAAAAAEABAIAAQEBC0Fkb2JlQmxhbmsAAQEBMPgb+ByLDB74HQH4HgKL+wz6APoEBR4aBF8MHxwIAQwi91UP92IR91oMJRwZHwwkAAUBAQYOVmFwQWRvYmVJZGVudGl0eUNvcHlyaWdodCAyMDEzLCAyMDE1IEFkb2JlIFN5c3RlbXMgSW5jb3Jwb3JhdGVkIChodHRwOi8vd3d3LmFkb2JlLmNvbS8pLkFkb2JlIEJsYW5rQWRvYmVCbGFuay0yMDQ5AAACAAEH/wMAAQAAAAgBCAECAAEASwBMAE0ATgBPAFAAUQBSAFMAVABVAFYAVwBYAFkAWgBbAFwAXQBeAF8AYABhAGIAYwBkAGUAZgBnAGgAaQBqAGsAbABtAG4AbwBwAHEAcgBzAHQAdQB2AHcAeAB5AHoAewB8AH0AfgB/AIAAgQCCAIMAhACFAIYAhwCIAIkAigCLAIwAjQCOAI8AkACRAJIAkwCUAJUAlgCXAJgAmQCaAJsAnACdAJ4AnwCgAKEAogCjAKQApQCmAKcAqACpAKoAqwCsAK0ArgCvALAAsQCyALMAtAC1ALYAtwC4ALkAugC7ALwAvQC+AL8AwADBAMIAwwDEAMUAxgDHAMgAyQDKAMsAzADNAM4AzwDQANEA0gDTANQA1QDWANcA2ADZANoA2wDcAN0A3gDfAOAA4QDiAOMA5ADlAOYA5wDoAOkA6gDrAOwA7QDuAO8A8ADxAPIA8wD0APUA9gD3APgA+QD6APsA/AD9AP4A/wEAAQEBAgEDAQQBBQEGAQcBCAEJAQoBCwEMAQ0BDgEPARABEQESARMBFAEVARYBFwEYARkBGgEbARwBHQEeAR8BIAEhASIBIwEkASUBJgEnASgBKQEqASsBLAEtAS4BLwEwATEBMgEzATQBNQE2ATcBOAE5AToBOwE8AT0BPgE/AUABQQFCAUMBRAFFAUYBRwFIAUkBSgFLAUwBTQFOAU8BUAFRAVIBUwFUAVUBVgFXAVgBWQFaAVsBXAFdAV4BXwFgAWEBYgFjAWQBZQFmAWcBaAFpAWoBawFsAW0BbgFvAXABcQFyAXMBdAF1AXYBdwF4AXkBegF7AXwBfQF+AX8BgAGBAYIBgwGEAYUBhgGHAYgBiQGKAYsBjAGNAY4BjwGQAZEBkgGTAZQBlQGWAZcBmAGZAZoBmwGcAZ0BngGfAaABoQGiAaMBpAGlAaYBpwGoAakBqgGrAawBrQGuAa8BsAGxAbIBswG0AbUBtgG3AbgBuQG6AbsBvAG9Ab4BvwHAAcEBwgHDAcQBxQHGAccByAHJAcoBywHMAc0BzgHPAdAB0QHSAdMB1AHVAdYB1wHYAdkB2gHbAdwB3QHeAd8B4AHhAeIB4wHkAeUB5gHnAegB6QHqAesB7AHtAe4B7wHwAfEB8gHzAfQB9QH2AfcB+AH5AfoB+wH8Af0B/gH/AgACAQICAgMCBAIFAgYCBwIIAgkCCgILAgwCDQIOAg8CEAIRAhICEwIUAhUCFgIXAhgCGQIaAhsCHAIdAh4CHwIgAiECIgIjAiQCJQImAicCKAIpAioCKwIsAi0CLgIvAjACMQIyAjMCNAI1AjYCNwI4AjkCOgI7AjwCPQI+Aj8CQAJBAkICQwJEAkUCRgJHAkgCSQJKAksCTAJNAk4CTwJQAlECUgJTAlQCVQJWAlcCWAJZAloCWwJcAl0CXgJfAmACYQJiAmMCZAJlAmYCZwJoAmkCagJrAmwCbQJuAm8CcAJxAnICcwJ0AnUCdgJ3AngCeQJ6AnsCfAJ9An4CfwKAAoECggKDAoQChQKGAocCiAKJAooCiwKMAo0CjgKPApACkQKSApMClAKVApYClwKYApkCmgKbApwCnQKeAp8CoAKhAqICowKkAqUCpgKnAqgCqQKqAqsCrAKtAq4CrwKwArECsgKzArQCtQK2ArcCuAK5AroCuwK8Ar0CvgK/AsACwQLCAsMCxALFAsYCxwLIAskCygLLAswCzQLOAs8C0ALRAtIC0wLUAtUC1gLXAtgC2QLaAtsC3ALdAt4C3wLgAuEC4gLjAuQC5QLmAucC6ALpAuoC6wLsAu0C7gLvAvAC8QLyAvMC9AL1AvYC9wL4AvkC+gL7AvwC/QL+Av8DAAMBAwIDAwMEAwUDBgMHAwgDCQMKAwsDDAMNAw4DDwMQAxEDEgMTAxQDFQMWAxcDGAMZAxoDGwMcAx0DHgMfAyADIQMiAyMDJAMlAyYDJwMoAykDKgMrAywDLQMuAy8DMAMxAzIDMwM0AzUDNgM3AzgDOQM6AzsDPAM9Az4DPwNAA0EDQgNDA0QDRQNGA0cDSANJA0oDSwNMA00DTgNPA1ADUQNSA1MDVANVA1YDVwNYA1kDWgNbA1wDXQNeA18DYANhA2IDYwNkA2UDZgNnA2gDaQNqA2sDbANtA24DbwNwA3EDcgNzA3QDdQN2A3cDeAN5A3oDewN8A30DfgN/A4ADgQOCA4MDhAOFA4YDhwOIA4kDigOLA4wDjQOOA48DkAORA5IDkwOUA5UDlgOXA5gDmQOaA5sDnAOdA54DnwOgA6EDogOjA6QDpQOmA6cDqAOpA6oDqwOsA60DrgOvA7ADsQOyA7MDtAO1A7YDtwO4A7kDugO7A7wDvQO+A78DwAPBA8IDwwPEA8UDxgPHA8gDyQPKA8sDzAPNA84DzwPQA9ED0gPTA9QD1QPWA9cD2APZA9oD2wPcA90D3gPfA+AD4QPiA+MD5APlA+YD5wPoA+kD6gPrA+wD7QPuA+8D8APxA/ID8wP0A/UD9gP3A/gD+QP6A/sD/AP9A/4D/wQABAEEAgQDBAQEBQQGBAcECAQJBAoECwQMBA0EDgQPBBAEEQQSBBMEFAQVBBYEFwQYBBkEGgQbBBwEHQQeBB8EIAQhBCIEIwQkBCUEJgQnBCgEKQQqBCsELAQtBC4ELwQwBDEEMgQzBDQENQQ2BDcEOAQ5BDoEOwQ8BD0EPgQ/BEAEQQRCBEMERARFBEYERwRIBEkESgRLBEwETQROBE8EUARRBFIEUwRUBFUEVgRXBFgEWQRaBFsEXARdBF4EXwRgBGEEYgRjBGQEZQRmBGcEaARpBGoEawRsBG0EbgRvBHAEcQRyBHMEdAR1BHYEdwR4BHkEegR7BHwEfQR+BH8EgASBBIIEgwSEBIUEhgSHBIgEiQSKBIsEjASNBI4EjwSQBJEEkgSTBJQElQSWBJcEmASZBJoEmwScBJ0EngSfBKAEoQSiBKMEpASlBKYEpwSoBKkEqgSrBKwErQSuBK8EsASxBLIEswS0BLUEtgS3BLgEuQS6BLsEvAS9BL4EvwTABMEEwgTDBMQExQTGBMcEyATJBMoEywTMBM0EzgTPBNAE0QTSBNME1ATVBNYE1wTYBNkE2gTbBNwE3QTeBN8E4AThBOIE4wTkBOUE5gTnBOgE6QTqBOsE7ATtBO4E7wTwBPEE8gTzBPQE9QT2BPcE+AT5BPoE+wT8BP0E/gT/BQAFAQUCBQMFBAUFBQYFBwUIBQkFCgULBQwFDQUOBQ8FEAURBRIFEwUUBRUFFgUXBRgFGQUaBRsFHAUdBR4FHwUgBSEFIgUjBSQFJQUmBScFKAUpBSoFKwUsBS0FLgUvBTAFMQUyBTMFNAU1BTYFNwU4BTkFOgU7BTwFPQU+BT8FQAVBBUIFQwVEBUUFRgVHBUgFSQVKBUsFTAVNBU4FTwVQBVEFUgVTBVQFVQVWBVcFWAVZBVoFWwVcBV0FXgVfBWAFYQViBWMFZAVlBWYFZwVoBWkFagVrBWwFbQVuBW8FcAVxBXIFcwV0BXUFdgV3BXgFeQV6BXsFfAV9BX4FfwWABYEFggWDBYQFhQWGBYcFiAWJBYoFiwWMBY0FjgWPBZAFkQWSBZMFlAWVBZYFlwWYBZkFmgWbBZwFnQWeBZ8FoAWhBaIFowWkBaUFpgWnBagFqQWqBasFrAWtBa4FrwWwBbEFsgWzBbQFtQW2BbcFuAW5BboFuwW8Bb0FvgW/BcAFwQXCBcMFxAXFBcYFxwXIBckFygXLBcwFzQXOBc8F0AXRBdIF0wXUBdUF1gXXBdgF2QXaBdsF3AXdBd4F3wXgBeEF4gXjBeQF5QXmBecF6AXpBeoF6wXsBe0F7gXvBfAF8QXyBfMF9AX1BfYF9wX4BfkF+gX7BfwF/QX+Bf8GAAYBBgIGAwYEBgUGBgYHBggGCQYKBgsGDAYNBg4GDwYQBhEGEgYTBhQGFQYWBhcGGAYZBhoGGwYcBh0GHgYfBiAGIQYiBiMGJAYlBiYGJwYoBikGKgYrBiwGLQYuBi8GMAYxBjIGMwY0BjUGNgY3BjgGOQY6BjsGPAY9Bj4GPwZABkEGQgZDBkQGRQZGBkcGSAZJBkoGSwZMBk0GTgZPBlAGUQZSBlMGVAZVBlYGVwZYBlkGWgZbBlwGXQZeBl8GYAZhBmIGYwZkBmUGZgZnBmgGaQZqBmsGbAZtBm4GbwZwBnEGcgZzBnQGdQZ2BncGeAZ5BnoGewZ8Bn0GfgZ/BoAGgQaCBoMGhAaFBoYGhwaIBokGigaLBowGjQaOBo8GkAaRBpIGkwaUBpUGlgaXBpgGmQaaBpsGnAadBp4GnwagBqEGogajBqQGpQamBqcGqAapBqoGqwasBq0GrgavBrAGsQayBrMGtAa1BrYGtwa4BrkGuga7BrwGvQa+Br8GwAbBBsIGwwbEBsUGxgbHBsgGyQbKBssGzAbNBs4GzwbQBtEG0gbTBtQG1QbWBtcG2AbZBtoG2wbcBt0G3gbfBuAG4QbiBuMG5AblBuYG5wboBukG6gbrBuwG7QbuBu8G8AbxBvIG8wb0BvUG9gb3BvgG+Qb6BvsG/Ab9Bv4G/wcABwEHAgcDBwQHBQcGBwcHCAcJBwoHCwcMBw0HDgcPBxAHEQcSBxMHFAcVBxYHFwcYBxkHGgcbBxwHHQceBx8HIAchByIHIwckByUHJgcnBygHKQcqBysHLActBy4HLwcwBzEHMgczBzQHNQc2BzcHOAc5BzoHOwc8Bz0HPgc/B0AHQQdCB0MHRAdFB0YHRwdIB0kHSgdLB0wHTQdOB08HUAdRB1IHUwdUB1UHVgdXB1gHWQdaB1sHXAddB14HXwdgB2EHYgdjB2QHZQdmB2cHaAdpB2oHawdsB20HbgdvB3AHcQdyB3MHdAd1B3YHdwd4B3kHegd7B3wHfQd+B38HgAeBB4IHgweEB4UHhgeHB4gHiQeKB4sHjAeNB44HjweQB5EHkgeTB5QHlQeWB5cHmAeZB5oHmwecB50HngefB6AHoQeiB6MHpAelB6YHpweoB6kHqgerB6wHrQeuB68HsAexB7IHswe0B7UHtge3B7gHuQe6B7sHvAe9B74HvwfAB8EHwgfDB8QHxQfGB8cHyAfJB8oHywfMB80HzgfPB9AH0QfSB9MH1AfVB9YH1wfYB9kH2gfbB9wH3QfeB98H4AfhB+IH4wfkB+UH5gfnB+gH6QfqB+sH7AftB+4H7wfwB/EH8gfzB/QH9Qf2B/cH+Af5B/oH+wf8B/0H/gf/CAAIAQgCCAMIBAgFCAYIBwgICAkICggLCAwIDQgOCA8IEAgRCBIIEwgUCBUIFggXCBgIGQgaCBsIHAgdCB4IHwggCCEIIggjCCQIJQgmCCcIKAgpCCoIKwgsCC0ILggvCDAIMQgyCDMINAg1CDYINwg4CDkIOgg7CDwIPQg+CD8IQAhBCEIIQwhECEUIRghHCEgISQhKCEsg+wy3+iS3AfcQt/kstwP3EPoEFf58+YT6fAf9WP4nFfnSB/fF/DMFprAV+8X4NwX49gamYhX90gf7xfgzBXBmFffF/DcF/PYGDg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4ODg4OAAEBAQr4HwwmmhwZLRL7joscBUaLBr0KvQv65xUD6AB8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAA==");}', 
    e.cssRules.length);
  };
  (function(e) {
    e.instance = {enabled:!1, initJS:function(g) {
    }, registerCallback:function(g) {
    }, unregisterCallback:function(g) {
    }, eval:function(g) {
    }, call:function(g) {
    }, getId:function() {
      return null;
    }};
  })(k.ExternalInterfaceService || (k.ExternalInterfaceService = {}));
  (function(e) {
    e[e.InvalidCallback = -3] = "InvalidCallback";
    e[e.AlreadyTaken = -2] = "AlreadyTaken";
    e[e.InvalidName = -1] = "InvalidName";
    e[e.Success = 0] = "Success";
  })(k.LocalConnectionConnectResult || (k.LocalConnectionConnectResult = {}));
  (function(e) {
    e[e.NotConnected = -1] = "NotConnected";
    e[e.Success = 0] = "Success";
  })(k.LocalConnectionCloseResult || (k.LocalConnectionCloseResult = {}));
  (function(e) {
    e.instance;
  })(k.LocalConnectionService || (k.LocalConnectionService = {}));
  (function(e) {
    e.instance = {setClipboard:function(g) {
    }};
  })(k.ClipboardService || (k.ClipboardService = {}));
  b = function() {
    function e() {
      this._queues = {};
    }
    e.prototype.register = function(g, d) {
      m.assert(g);
      m.assert(d);
      var c = this._queues[g];
      if (c) {
        if (-1 < c.indexOf(d)) {
          return;
        }
      } else {
        c = this._queues[g] = [];
      }
      c.push(d);
    };
    e.prototype.unregister = function(g, d) {
      m.assert(g);
      m.assert(d);
      var c = this._queues[g];
      if (c) {
        var f = c.indexOf(d);
        -1 !== f && c.splice(f, 1);
        0 === c.length && (this._queues[g] = null);
      }
    };
    e.prototype.notify = function(g, d) {
      var c = this._queues[g];
      if (c) {
        c = c.slice();
        d = Array.prototype.slice.call(arguments, 0);
        for (var f = 0;f < c.length;f++) {
          c[f].apply(null, d);
        }
      }
    };
    e.prototype.notify1 = function(g, d) {
      var c = this._queues[g];
      if (c) {
        for (var c = c.slice(), f = 0;f < c.length;f++) {
          (0,c[f])(g, d);
        }
      }
    };
    return e;
  }();
  k.Callback = b;
  (function(e) {
    e[e.None = 0] = "None";
    e[e.PremultipliedAlphaARGB = 1] = "PremultipliedAlphaARGB";
    e[e.StraightAlphaARGB = 2] = "StraightAlphaARGB";
    e[e.StraightAlphaRGBA = 3] = "StraightAlphaRGBA";
    e[e.JPEG = 4] = "JPEG";
    e[e.PNG = 5] = "PNG";
    e[e.GIF = 6] = "GIF";
  })(k.ImageType || (k.ImageType = {}));
  var v = k.ImageType;
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
    e.toCSSCursor = function(g) {
      switch(g) {
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
  b = function() {
    function e() {
      this.promise = new Promise(function(g, d) {
        this.resolve = g;
        this.reject = d;
      }.bind(this));
    }
    e.prototype.then = function(g, d) {
      return this.promise.then(g, d);
    };
    return e;
  }();
  k.PromiseWrapper = b;
})(Shumway || (Shumway = {}));
(function() {
  function k(c) {
    if ("function" !== typeof c) {
      throw new TypeError("Invalid deferred constructor");
    }
    var f = q();
    c = new c(f);
    var d = f.resolve;
    if ("function" !== typeof d) {
      throw new TypeError("Invalid resolve construction function");
    }
    f = f.reject;
    if ("function" !== typeof f) {
      throw new TypeError("Invalid reject construction function");
    }
    return {promise:c, resolve:d, reject:f};
  }
  function p(c, f) {
    if ("object" !== typeof c || null === c) {
      return !1;
    }
    try {
      var d = c.then;
      if ("function" !== typeof d) {
        return !1;
      }
      d.call(c, f.resolve, f.reject);
    } catch (g) {
      d = f.reject, d(g);
    }
    return !0;
  }
  function u(c) {
    return "object" === typeof c && null !== c && "undefined" !== typeof c.promiseStatus;
  }
  function a(c, f) {
    if ("unresolved" === c.promiseStatus) {
      var d = c.rejectReactions;
      c.result = f;
      c.resolveReactions = void 0;
      c.rejectReactions = void 0;
      c.promiseStatus = "has-rejection";
      w(d, f);
    }
  }
  function w(c, f) {
    for (var d = 0;d < c.length;d++) {
      m({reaction:c[d], argument:f});
    }
  }
  function m(c) {
    0 === d.length && setTimeout(b, 0);
    d.push(c);
  }
  function b() {
    for (;0 < d.length;) {
      var c = d[0];
      try {
        a: {
          var f = c.reaction, g = f.deferred, x = f.handler, b = void 0, a = void 0;
          try {
            b = x(c.argument);
          } catch (l) {
            var h = g.reject;
            h(l);
            break a;
          }
          if (b === g.promise) {
            h = g.reject, h(new TypeError("Self resolution"));
          } else {
            try {
              if (a = p(b, g), !a) {
                var v = g.resolve;
                v(b);
              }
            } catch (r) {
              h = g.reject, h(r);
            }
          }
        }
      } catch (n) {
        if ("function" === typeof e.onerror) {
          e.onerror(n);
        }
      }
      d.shift();
    }
  }
  function l(c) {
    throw c;
  }
  function r(c) {
    return c;
  }
  function h(c) {
    return function(f) {
      a(c, f);
    };
  }
  function t(c) {
    return function(f) {
      if ("unresolved" === c.promiseStatus) {
        var d = c.resolveReactions;
        c.result = f;
        c.resolveReactions = void 0;
        c.rejectReactions = void 0;
        c.promiseStatus = "has-resolution";
        w(d, f);
      }
    };
  }
  function q() {
    function c(f, d) {
      c.resolve = f;
      c.reject = d;
    }
    return c;
  }
  function n(c, f, d) {
    return function(g) {
      if (g === c) {
        return d(new TypeError("Self resolution"));
      }
      var e = c.promiseConstructor;
      if (u(g) && g.promiseConstructor === e) {
        return g.then(f, d);
      }
      e = k(e);
      return p(g, e) ? e.promise.then(f, d) : f(g);
    };
  }
  function v(c, f, d, g) {
    return function(e) {
      f[c] = e;
      g.countdown--;
      0 === g.countdown && d.resolve(f);
    };
  }
  function e(c) {
    if ("function" !== typeof c) {
      throw new TypeError("resolver is not a function");
    }
    if ("object" !== typeof this) {
      throw new TypeError("Promise to initialize is not an object");
    }
    this.promiseStatus = "unresolved";
    this.resolveReactions = [];
    this.rejectReactions = [];
    this.result = void 0;
    var f = t(this), d = h(this);
    try {
      c(f, d);
    } catch (g) {
      a(this, g);
    }
    this.promiseConstructor = e;
    return this;
  }
  var g = Function("return this")();
  if (g.Promise) {
    "function" !== typeof g.Promise.all && (g.Promise.all = function(c) {
      var f = 0, d = [], e, b, a = new g.Promise(function(f, c) {
        e = f;
        b = c;
      });
      c.forEach(function(c, g) {
        f++;
        c.then(function(c) {
          d[g] = c;
          f--;
          0 === f && e(d);
        }, b);
      });
      0 === f && e(d);
      return a;
    }), "function" !== typeof g.Promise.resolve && (g.Promise.resolve = function(c) {
      return new g.Promise(function(f) {
        f(c);
      });
    });
  } else {
    var d = [];
    e.all = function(c) {
      var f = k(this), d = [], g = {countdown:0}, e = 0;
      c.forEach(function(c) {
        c = this.cast(c);
        var b = v(e, d, f, g);
        c.then(b, f.reject);
        e++;
        g.countdown++;
      }, this);
      0 === e && f.resolve(d);
      return f.promise;
    };
    e.cast = function(c) {
      if (u(c)) {
        return c;
      }
      var f = k(this);
      f.resolve(c);
      return f.promise;
    };
    e.reject = function(c) {
      var f = k(this);
      f.reject(c);
      return f.promise;
    };
    e.resolve = function(c) {
      var f = k(this);
      f.resolve(c);
      return f.promise;
    };
    e.prototype = {"catch":function(c) {
      this.then(void 0, c);
    }, then:function(c, f) {
      if (!u(this)) {
        throw new TypeError("this is not a Promises");
      }
      var d = k(this.promiseConstructor), g = "function" === typeof f ? f : l, e = n(this, "function" === typeof c ? c : r, g), e = {deferred:d, handler:e}, g = {deferred:d, handler:g};
      switch(this.promiseStatus) {
        case "unresolved":
          this.resolveReactions.push(e);
          this.rejectReactions.push(g);
          break;
        case "has-resolution":
          m({reaction:e, argument:this.result});
          break;
        case "has-rejection":
          m({reaction:g, argument:this.result});
      }
      return d.promise;
    }};
    g.Promise = e;
  }
})();
"undefined" !== typeof exports && (exports.Shumway = Shumway);
(function() {
  function k(k, u, a) {
    k[u] || Object.defineProperty(k, u, {value:a, writable:!0, configurable:!0, enumerable:!1});
  }
  k(String.prototype, "padRight", function(k, u) {
    var a = this, w = a.replace(/\033\[[0-9]*m/g, "").length;
    if (!k || w >= u) {
      return a;
    }
    for (var w = (u - w) / k.length, m = 0;m < w;m++) {
      a += k;
    }
    return a;
  });
  k(String.prototype, "padLeft", function(k, u) {
    var a = this, w = a.length;
    if (!k || w >= u) {
      return a;
    }
    for (var w = (u - w) / k.length, m = 0;m < w;m++) {
      a = k + a;
    }
    return a;
  });
  k(String.prototype, "trim", function() {
    return this.replace(/^\s+|\s+$/g, "");
  });
  k(String.prototype, "endsWith", function(k) {
    return -1 !== this.indexOf(k, this.length - k.length);
  });
  k(Array.prototype, "replace", function(k, u) {
    if (k === u) {
      return 0;
    }
    for (var a = 0, w = 0;w < this.length;w++) {
      this[w] === k && (this[w] = u, a++);
    }
    return a;
  });
})();
(function(k) {
  (function(p) {
    var u = k.isObject, a = function() {
      function b(a, l, t, m) {
        this.shortName = a;
        this.longName = l;
        this.type = t;
        m = m || {};
        this.positional = m.positional;
        this.parseFn = m.parse;
        this.value = m.defaultValue;
      }
      b.prototype.parse = function(b) {
        this.value = "boolean" === this.type ? b : "number" === this.type ? parseInt(b, 10) : b;
        this.parseFn && this.parseFn(this.value);
      };
      return b;
    }();
    p.Argument = a;
    var w = function() {
      function b() {
        this.args = [];
      }
      b.prototype.addArgument = function(b, l, t, m) {
        b = new a(b, l, t, m);
        this.args.push(b);
        return b;
      };
      b.prototype.addBoundOption = function(b) {
        this.args.push(new a(b.shortName, b.longName, b.type, {parse:function(a) {
          b.value = a;
        }}));
      };
      b.prototype.addBoundOptionSet = function(b) {
        var a = this;
        b.options.forEach(function(b) {
          m.isOptionSet(b) ? a.addBoundOptionSet(b) : a.addBoundOption(b);
        });
      };
      b.prototype.getUsage = function() {
        var b = "";
        this.args.forEach(function(a) {
          b = a.positional ? b + a.longName : b + ("[-" + a.shortName + "|--" + a.longName + ("boolean" === a.type ? "" : " " + a.type[0].toUpperCase()) + "]");
          b += " ";
        });
        return b;
      };
      b.prototype.parse = function(b) {
        var a = {}, l = [];
        this.args.forEach(function(g) {
          g.positional ? l.push(g) : (a["-" + g.shortName] = g, a["--" + g.longName] = g);
        });
        for (var m = [];b.length;) {
          var n = b.shift(), v = null, e = n;
          if ("--" == n) {
            m = m.concat(b);
            break;
          } else {
            if ("-" == n.slice(0, 1) || "--" == n.slice(0, 2)) {
              v = a[n];
              if (!v) {
                continue;
              }
              e = "boolean" !== v.type ? b.shift() : !0;
            } else {
              l.length ? v = l.shift() : m.push(e);
            }
          }
          v && v.parse(e);
        }
        return m;
      };
      return b;
    }();
    p.ArgumentParser = w;
    var m = function() {
      function a(b, l) {
        void 0 === l && (l = null);
        this.open = !1;
        this.name = b;
        this.settings = l || {};
        this.options = [];
      }
      a.isOptionSet = function(r) {
        return r instanceof a ? !0 : "object" !== typeof r || null === r || r instanceof b ? !1 : "options" in r && "name" in r && "settings" in r;
      };
      a.prototype.register = function(b) {
        if (a.isOptionSet(b)) {
          for (var h = 0;h < this.options.length;h++) {
            var t = this.options[h];
            if (a.isOptionSet(t) && t.name === b.name) {
              return t;
            }
          }
        }
        this.options.push(b);
        if (this.settings) {
          if (a.isOptionSet(b)) {
            h = this.settings[b.name], u(h) && (b.settings = h.settings, b.open = h.open);
          } else {
            if ("undefined" !== typeof this.settings[b.longName]) {
              switch(b.type) {
                case "boolean":
                  b.value = !!this.settings[b.longName];
                  break;
                case "number":
                  b.value = +this.settings[b.longName];
                  break;
                default:
                  b.value = this.settings[b.longName];
              }
            }
          }
        }
        return b;
      };
      a.prototype.trace = function(b) {
        b.enter(this.name + " {");
        this.options.forEach(function(a) {
          a.trace(b);
        });
        b.leave("}");
      };
      a.prototype.getSettings = function() {
        var b = {};
        this.options.forEach(function(h) {
          a.isOptionSet(h) ? b[h.name] = {settings:h.getSettings(), open:h.open} : b[h.longName] = h.value;
        });
        return b;
      };
      a.prototype.setSettings = function(b) {
        b && this.options.forEach(function(h) {
          a.isOptionSet(h) ? h.name in b && h.setSettings(b[h.name].settings) : h.longName in b && (h.value = b[h.longName]);
        });
      };
      return a;
    }();
    p.OptionSet = m;
    var b = function() {
      function b(a, l, t, m, n, v) {
        void 0 === v && (v = null);
        this.longName = l;
        this.shortName = a;
        this.type = t;
        this.value = this.defaultValue = m;
        this.description = n;
        this.config = v;
      }
      b.prototype.parse = function(b) {
        this.value = b;
      };
      b.prototype.trace = function(b) {
        b.writeLn(("-" + this.shortName + "|--" + this.longName).padRight(" ", 30) + " = " + this.type + " " + this.value + " [" + this.defaultValue + "] (" + this.description + ")");
      };
      return b;
    }();
    p.Option = b;
  })(k.Options || (k.Options = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(u) {
    u.ROOT = "Shumway Options";
    u.shumwayOptions = new k.Options.OptionSet(u.ROOT);
    u.setSettings = function(a) {
      u.shumwayOptions.setSettings(a);
    };
    u.getSettings = function() {
      return u.shumwayOptions.getSettings();
    };
  })(k.Settings || (k.Settings = {}));
  var p = k.Options.Option;
  k.loggingOptions = k.Settings.shumwayOptions.register(new k.Options.OptionSet("Logging Options"));
  k.omitRepeatedWarnings = k.loggingOptions.register(new p("wo", "warnOnce", "boolean", !0, "Omit Repeated Warnings"));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var u = function() {
      function a(a, m) {
        this._parent = a;
        this._timers = k.ObjectUtilities.createMap();
        this._name = m;
        this._count = this._total = this._last = this._begin = 0;
      }
      a.time = function(k, m) {
        a.start(k);
        m();
        a.stop();
      };
      a.start = function(k) {
        a._top = a._top._timers[k] || (a._top._timers[k] = new a(a._top, k));
        a._top.start();
        k = a._flat._timers[k] || (a._flat._timers[k] = new a(a._flat, k));
        k.start();
        a._flatStack.push(k);
      };
      a.stop = function() {
        a._top.stop();
        a._top = a._top._parent;
        a._flatStack.pop().stop();
      };
      a.stopStart = function(k) {
        a.stop();
        a.start(k);
      };
      a.prototype.start = function() {
        this._begin = k.getTicks();
      };
      a.prototype.stop = function() {
        this._last = k.getTicks() - this._begin;
        this._total += this._last;
        this._count += 1;
      };
      a.prototype.toJSON = function() {
        return {name:this._name, total:this._total, timers:this._timers};
      };
      a.prototype.trace = function(a) {
        a.enter(this._name + ": " + this._total.toFixed(2) + " ms, count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var m in this._timers) {
          this._timers[m].trace(a);
        }
        a.outdent();
      };
      a.trace = function(k) {
        a._base.trace(k);
        a._flat.trace(k);
      };
      a._base = new a(null, "Total");
      a._top = a._base;
      a._flat = new a(null, "Flat");
      a._flatStack = [];
      return a;
    }();
    p.Timer = u;
    u = function() {
      function a(a) {
        this._enabled = a;
        this.clear();
      }
      Object.defineProperty(a.prototype, "counts", {get:function() {
        return this._counts;
      }, enumerable:!0, configurable:!0});
      a.prototype.setEnabled = function(a) {
        this._enabled = a;
      };
      a.prototype.clear = function() {
        this._counts = k.ObjectUtilities.createMap();
        this._times = k.ObjectUtilities.createMap();
      };
      a.prototype.toJSON = function() {
        return {counts:this._counts, times:this._times};
      };
      a.prototype.count = function(a, m, b) {
        void 0 === m && (m = 1);
        void 0 === b && (b = 0);
        if (this._enabled) {
          return void 0 === this._counts[a] && (this._counts[a] = 0, this._times[a] = 0), this._counts[a] += m, this._times[a] += b, this._counts[a];
        }
      };
      a.prototype.trace = function(a) {
        for (var m in this._counts) {
          a.writeLn(m + ": " + this._counts[m]);
        }
      };
      a.prototype._pairToString = function(a, m) {
        var b = m[0], l = m[1], r = a[b], b = b + ": " + l;
        r && (b += ", " + r.toFixed(4), 1 < l && (b += " (" + (r / l).toFixed(4) + ")"));
        return b;
      };
      a.prototype.toStringSorted = function() {
        var a = this, m = this._times, b = [], l;
        for (l in this._counts) {
          b.push([l, this._counts[l]]);
        }
        b.sort(function(b, a) {
          return a[1] - b[1];
        });
        return b.map(function(b) {
          return a._pairToString(m, b);
        }).join(", ");
      };
      a.prototype.traceSorted = function(a, m) {
        void 0 === m && (m = !1);
        var b = this, l = this._times, r = [], h;
        for (h in this._counts) {
          r.push([h, this._counts[h]]);
        }
        r.sort(function(b, a) {
          return a[1] - b[1];
        });
        m ? a.writeLn(r.map(function(a) {
          return b._pairToString(l, a);
        }).join(", ")) : r.forEach(function(h) {
          a.writeLn(b._pairToString(l, h));
        });
      };
      a.instance = new a(!0);
      return a;
    }();
    p.Counter = u;
    u = function() {
      function a(a) {
        this._samples = new Float64Array(a);
        this._index = this._count = 0;
      }
      a.prototype.push = function(a) {
        this._count < this._samples.length && this._count++;
        this._index++;
        this._samples[this._index % this._samples.length] = a;
      };
      a.prototype.average = function() {
        for (var a = 0, m = 0;m < this._count;m++) {
          a += this._samples[m];
        }
        return a / this._count;
      };
      return a;
    }();
    p.Average = u;
  })(k.Metrics || (k.Metrics = {}));
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function(k, p) {
  function u() {
    this.constructor = k;
  }
  for (var a in p) {
    p.hasOwnProperty(a) && (k[a] = p[a]);
  }
  u.prototype = p.prototype;
  k.prototype = new u;
};
(function(k) {
  (function(k) {
    function u(d) {
      for (var c = Math.max.apply(null, d), f = d.length, g = 1 << c, e = new Uint32Array(g), b = c << 16 | 65535, a = 0;a < g;a++) {
        e[a] = b;
      }
      for (var b = 0, a = 1, l = 2;a <= c;b <<= 1, ++a, l <<= 1) {
        for (var h = 0;h < f;++h) {
          if (d[h] === a) {
            for (var v = 0, n = 0;n < a;++n) {
              v = 2 * v + (b >> n & 1);
            }
            for (n = v;n < g;n += l) {
              e[n] = a << 16 | h;
            }
            ++b;
          }
        }
      }
      return {codes:e, maxBits:c};
    }
    var a;
    (function(d) {
      d[d.INIT = 0] = "INIT";
      d[d.BLOCK_0 = 1] = "BLOCK_0";
      d[d.BLOCK_1 = 2] = "BLOCK_1";
      d[d.BLOCK_2_PRE = 3] = "BLOCK_2_PRE";
      d[d.BLOCK_2 = 4] = "BLOCK_2";
      d[d.DONE = 5] = "DONE";
      d[d.ERROR = 6] = "ERROR";
      d[d.VERIFY_HEADER = 7] = "VERIFY_HEADER";
    })(a || (a = {}));
    a = function() {
      function d(c) {
      }
      d.prototype.push = function(c) {
      };
      d.prototype.close = function() {
      };
      d.create = function(c) {
        return "undefined" !== typeof ShumwayCom && ShumwayCom.createSpecialInflate ? new v(c, ShumwayCom.createSpecialInflate) : new w(c);
      };
      d.prototype._processZLibHeader = function(c, f, d) {
        if (f + 2 > d) {
          return 0;
        }
        c = c[f] << 8 | c[f + 1];
        f = null;
        2048 !== (c & 3840) ? f = "inflate: unknown compression method" : 0 !== c % 31 ? f = "inflate: bad FCHECK" : 0 !== (c & 32) && (f = "inflate: FDICT bit set");
        if (f) {
          if (this.onError) {
            this.onError(f);
          }
          return -1;
        }
        return 2;
      };
      d.inflate = function(c, f, g) {
        var e = new Uint8Array(f), b = 0;
        f = d.create(g);
        f.onData = function(f) {
          var c = Math.min(f.length, e.length - b);
          c && k.memCopy(e, f, b, 0, c);
          b += c;
        };
        f.onError = function(f) {
          throw Error(f);
        };
        f.push(c);
        f.close();
        return e;
      };
      return d;
    }();
    k.Inflate = a;
    var w = function(d) {
      function c(f) {
        d.call(this, f);
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
        if (!n) {
          m = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);
          b = new Uint16Array(30);
          l = new Uint8Array(30);
          for (var c = f = 0, g = 1;30 > f;++f) {
            b[f] = g, g += 1 << (l[f] = ~~((c += 2 < f ? 1 : 0) / 2));
          }
          var e = new Uint8Array(288);
          for (f = 0;32 > f;++f) {
            e[f] = 5;
          }
          r = u(e.subarray(0, 32));
          h = new Uint16Array(29);
          t = new Uint8Array(29);
          c = f = 0;
          for (g = 3;29 > f;++f) {
            h[f] = g - (28 == f ? 1 : 0), g += 1 << (t[f] = ~~((c += 4 < f ? 1 : 0) / 4 % 6));
          }
          for (f = 0;288 > f;++f) {
            e[f] = 144 > f || 279 < f ? 8 : 256 > f ? 9 : 7;
          }
          q = u(e);
          n = !0;
        }
      }
      __extends(c, d);
      c.prototype.push = function(f) {
        if (!this._buffer || this._buffer.length < this._bufferSize + f.length) {
          var c = new Uint8Array(this._bufferSize + f.length);
          this._buffer && c.set(this._buffer);
          this._buffer = c;
        }
        this._buffer.set(f, this._bufferSize);
        this._bufferSize += f.length;
        this._bufferPosition = 0;
        f = !1;
        do {
          c = this._windowPosition;
          if (0 === this._state && (f = this._decodeInitState())) {
            break;
          }
          switch(this._state) {
            case 1:
              f = this._decodeBlock0();
              break;
            case 3:
              if (f = this._decodeBlock2Pre()) {
                break;
              }
            ;
            case 2:
            ;
            case 4:
              f = this._decodeBlock();
              break;
            case 6:
            ;
            case 5:
              this._bufferPosition = this._bufferSize;
              break;
            case 7:
              var d = this._processZLibHeader(this._buffer, this._bufferPosition, this._bufferSize);
              0 < d ? (this._bufferPosition += d, this._state = 0) : 0 === d ? f = !0 : this._state = 6;
          }
          if (0 < this._windowPosition - c) {
            this.onData(this._window.subarray(c, this._windowPosition));
          }
          65536 <= this._windowPosition && ("copyWithin" in this._buffer ? this._window.copyWithin(0, this._windowPosition - 32768, this._windowPosition) : this._window.set(this._window.subarray(this._windowPosition - 32768, this._windowPosition)), this._windowPosition = 32768);
        } while (!f && this._bufferPosition < this._bufferSize);
        this._bufferPosition < this._bufferSize ? ("copyWithin" in this._buffer ? this._buffer.copyWithin(0, this._bufferPosition, this._bufferSize) : this._buffer.set(this._buffer.subarray(this._bufferPosition, this._bufferSize)), this._bufferSize -= this._bufferPosition) : this._bufferSize = 0;
      };
      c.prototype._decodeInitState = function() {
        if (this._isFinalBlock) {
          return this._state = 5, !1;
        }
        var f = this._buffer, c = this._bufferSize, d = this._bitBuffer, g = this._bitLength, e = this._bufferPosition;
        if (3 > (c - e << 3) + g) {
          return !0;
        }
        3 > g && (d |= f[e++] << g, g += 8);
        var b = d & 7, d = d >> 3, g = g - 3;
        switch(b >> 1) {
          case 0:
            g = d = 0;
            if (4 > c - e) {
              return !0;
            }
            var a = f[e] | f[e + 1] << 8, f = f[e + 2] | f[e + 3] << 8, e = e + 4;
            if (65535 !== (a ^ f)) {
              this._error("inflate: invalid block 0 length");
              f = 6;
              break;
            }
            0 === a ? f = 0 : (this._block0Read = a, f = 1);
            break;
          case 1:
            f = 2;
            this._literalTable = q;
            this._distanceTable = r;
            break;
          case 2:
            if (26 > (c - e << 3) + g) {
              return !0;
            }
            for (;14 > g;) {
              d |= f[e++] << g, g += 8;
            }
            a = (d >> 10 & 15) + 4;
            if ((c - e << 3) + g < 14 + 3 * a) {
              return !0;
            }
            for (var c = {numLiteralCodes:(d & 31) + 257, numDistanceCodes:(d >> 5 & 31) + 1, codeLengthTable:void 0, bitLengths:void 0, codesRead:0, dupBits:0}, d = d >> 14, g = g - 14, l = new Uint8Array(19), h = 0;h < a;++h) {
              3 > g && (d |= f[e++] << g, g += 8), l[m[h]] = d & 7, d >>= 3, g -= 3;
            }
            for (;19 > h;h++) {
              l[m[h]] = 0;
            }
            c.bitLengths = new Uint8Array(c.numLiteralCodes + c.numDistanceCodes);
            c.codeLengthTable = u(l);
            this._block2State = c;
            f = 3;
            break;
          default:
            return this._error("inflate: unsupported block type"), !1;
        }
        this._isFinalBlock = !!(b & 1);
        this._state = f;
        this._bufferPosition = e;
        this._bitBuffer = d;
        this._bitLength = g;
        return !1;
      };
      c.prototype._error = function(f) {
        if (this.onError) {
          this.onError(f);
        }
      };
      c.prototype._decodeBlock0 = function() {
        var f = this._bufferPosition, c = this._windowPosition, d = this._block0Read, g = 65794 - c, e = this._bufferSize - f, b = e < d, a = Math.min(g, e, d);
        this._window.set(this._buffer.subarray(f, f + a), c);
        this._windowPosition = c + a;
        this._bufferPosition = f + a;
        this._block0Read = d - a;
        return d === a ? (this._state = 0, !1) : b && g < e;
      };
      c.prototype._readBits = function(f) {
        var c = this._bitBuffer, d = this._bitLength;
        if (f > d) {
          var g = this._bufferPosition, e = this._bufferSize;
          do {
            if (g >= e) {
              return this._bufferPosition = g, this._bitBuffer = c, this._bitLength = d, -1;
            }
            c |= this._buffer[g++] << d;
            d += 8;
          } while (f > d);
          this._bufferPosition = g;
        }
        this._bitBuffer = c >> f;
        this._bitLength = d - f;
        return c & (1 << f) - 1;
      };
      c.prototype._readCode = function(f) {
        var c = this._bitBuffer, d = this._bitLength, g = f.maxBits;
        if (g > d) {
          var e = this._bufferPosition, b = this._bufferSize;
          do {
            if (e >= b) {
              return this._bufferPosition = e, this._bitBuffer = c, this._bitLength = d, -1;
            }
            c |= this._buffer[e++] << d;
            d += 8;
          } while (g > d);
          this._bufferPosition = e;
        }
        f = f.codes[c & (1 << g) - 1];
        g = f >> 16;
        if (f & 32768) {
          return this._error("inflate: invalid encoding"), this._state = 6, -1;
        }
        this._bitBuffer = c >> g;
        this._bitLength = d - g;
        return f & 65535;
      };
      c.prototype._decodeBlock2Pre = function() {
        var f = this._block2State, c = f.numLiteralCodes + f.numDistanceCodes, d = f.bitLengths, g = f.codesRead, e = 0 < g ? d[g - 1] : 0, b = f.codeLengthTable, a;
        if (0 < f.dupBits) {
          a = this._readBits(f.dupBits);
          if (0 > a) {
            return !0;
          }
          for (;a--;) {
            d[g++] = e;
          }
          f.dupBits = 0;
        }
        for (;g < c;) {
          var l = this._readCode(b);
          if (0 > l) {
            return f.codesRead = g, !0;
          }
          if (16 > l) {
            d[g++] = e = l;
          } else {
            var h;
            switch(l) {
              case 16:
                h = 2;
                a = 3;
                l = e;
                break;
              case 17:
                a = h = 3;
                l = 0;
                break;
              case 18:
                h = 7, a = 11, l = 0;
            }
            for (;a--;) {
              d[g++] = l;
            }
            a = this._readBits(h);
            if (0 > a) {
              return f.codesRead = g, f.dupBits = h, !0;
            }
            for (;a--;) {
              d[g++] = l;
            }
            e = l;
          }
        }
        this._literalTable = u(d.subarray(0, f.numLiteralCodes));
        this._distanceTable = u(d.subarray(f.numLiteralCodes));
        this._state = 4;
        this._block2State = null;
        return !1;
      };
      c.prototype._decodeBlock = function() {
        var f = this._literalTable, c = this._distanceTable, d = this._window, g = this._windowPosition, e = this._copyState, a, v, n, r;
        if (0 !== e.state) {
          switch(e.state) {
            case 1:
              if (0 > (a = this._readBits(e.lenBits))) {
                return !0;
              }
              e.len += a;
              e.state = 2;
            case 2:
              if (0 > (a = this._readCode(c))) {
                return !0;
              }
              e.distBits = l[a];
              e.dist = b[a];
              e.state = 3;
            case 3:
              a = 0;
              if (0 < e.distBits && 0 > (a = this._readBits(e.distBits))) {
                return !0;
              }
              r = e.dist + a;
              v = e.len;
              for (a = g - r;v--;) {
                d[g++] = d[a++];
              }
              e.state = 0;
              if (65536 <= g) {
                return this._windowPosition = g, !1;
              }
              break;
          }
        }
        do {
          a = this._readCode(f);
          if (0 > a) {
            return this._windowPosition = g, !0;
          }
          if (256 > a) {
            d[g++] = a;
          } else {
            if (256 < a) {
              this._windowPosition = g;
              a -= 257;
              n = t[a];
              v = h[a];
              a = 0 === n ? 0 : this._readBits(n);
              if (0 > a) {
                return e.state = 1, e.len = v, e.lenBits = n, !0;
              }
              v += a;
              a = this._readCode(c);
              if (0 > a) {
                return e.state = 2, e.len = v, !0;
              }
              n = l[a];
              r = b[a];
              a = 0 === n ? 0 : this._readBits(n);
              if (0 > a) {
                return e.state = 3, e.len = v, e.dist = r, e.distBits = n, !0;
              }
              r += a;
              for (a = g - r;v--;) {
                d[g++] = d[a++];
              }
            } else {
              this._state = 0;
              break;
            }
          }
        } while (65536 > g);
        this._windowPosition = g;
        return !1;
      };
      return c;
    }(a), m, b, l, r, h, t, q, n = !1, v = function(d) {
      function c(f, c) {
        d.call(this, f);
        this._verifyHeader = f;
        this._specialInflate = c();
        this._specialInflate.setDataCallback(function(f) {
          this.onData(f);
        }.bind(this));
      }
      __extends(c, d);
      c.prototype.push = function(f) {
        if (this._verifyHeader) {
          var c;
          this._buffer ? (c = new Uint8Array(this._buffer.length + f.length), c.set(this._buffer), c.set(f, this._buffer.length), this._buffer = null) : c = new Uint8Array(f);
          var d = this._processZLibHeader(c, 0, c.length);
          if (0 === d) {
            this._buffer = c;
            return;
          }
          this._verifyHeader = !0;
          0 < d && (f = c.subarray(d));
        }
        this._specialInflate.push(f);
      };
      c.prototype.close = function() {
        this._specialInflate && (this._specialInflate.close(), this._specialInflate = null);
      };
      return c;
    }(a), e;
    (function(d) {
      d[d.WRITE = 0] = "WRITE";
      d[d.DONE = 1] = "DONE";
      d[d.ZLIB_HEADER = 2] = "ZLIB_HEADER";
    })(e || (e = {}));
    var g = function() {
      function d() {
        this.a = 1;
        this.b = 0;
      }
      d.prototype.update = function(c, f, d) {
        for (var g = this.a, e = this.b;f < d;++f) {
          g = (g + (c[f] & 255)) % 65521, e = (e + g) % 65521;
        }
        this.a = g;
        this.b = e;
      };
      d.prototype.getChecksum = function() {
        return this.b << 16 | this.a;
      };
      return d;
    }();
    k.Adler32 = g;
    e = function() {
      function d(c) {
        this._state = (this._writeZlibHeader = c) ? 2 : 0;
        this._adler32 = c ? new g : null;
      }
      d.prototype.push = function(c) {
        2 === this._state && (this.onData(new Uint8Array([120, 156])), this._state = 0);
        for (var f = c.length, d = f + 5 * Math.ceil(f / 65535), d = new Uint8Array(d), g = 0, e = 0;65535 < f;) {
          d.set(new Uint8Array([0, 255, 255, 0, 0]), g), g += 5, d.set(c.subarray(e, e + 65535), g), e += 65535, g += 65535, f -= 65535;
        }
        d.set(new Uint8Array([0, f & 255, f >> 8 & 255, ~f & 255, ~f >> 8 & 255]), g);
        g += 5;
        d.set(c.subarray(e, f), g);
        this.onData(d);
        this._adler32 && this._adler32.update(c, 0, f);
      };
      d.prototype.close = function() {
        this._state = 1;
        this.onData(new Uint8Array([1, 0, 0, 255, 255]));
        if (this._adler32) {
          var c = this._adler32.getChecksum();
          this.onData(new Uint8Array([c & 255, c >> 8 & 255, c >> 16 & 255, c >>> 24 & 255]));
        }
      };
      return d;
    }();
    k.Deflate = e;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    function u(c) {
      for (var f = new Uint16Array(c), d = 0;d < c;d++) {
        f[d] = 1024;
      }
      return f;
    }
    function a(c, f, d, g) {
      for (var e = 1, b = 0, a = 0;a < d;a++) {
        var l = g.decodeBit(c, e + f), e = (e << 1) + l, b = b | l << a
      }
      return b;
    }
    function w(c, f) {
      var d = [];
      d.length = f;
      for (var g = 0;g < f;g++) {
        d[g] = new h(c);
      }
      return d;
    }
    var m = function() {
      function c() {
        this.pos = this.available = 0;
        this.buffer = new Uint8Array(2E3);
      }
      c.prototype.append = function(f) {
        var c = this.pos + this.available, d = c + f.length;
        if (d > this.buffer.length) {
          for (var g = 2 * this.buffer.length;g < d;) {
            g *= 2;
          }
          d = new Uint8Array(g);
          d.set(this.buffer);
          this.buffer = d;
        }
        this.buffer.set(f, c);
        this.available += f.length;
      };
      c.prototype.compact = function() {
        0 !== this.available && (this.buffer.set(this.buffer.subarray(this.pos, this.pos + this.available), 0), this.pos = 0);
      };
      c.prototype.readByte = function() {
        if (0 >= this.available) {
          throw Error("Unexpected end of file");
        }
        this.available--;
        return this.buffer[this.pos++];
      };
      return c;
    }(), b = function() {
      function c(f) {
        this.onData = f;
        this.processed = 0;
      }
      c.prototype.writeBytes = function(f) {
        this.onData.call(null, f);
        this.processed += f.length;
      };
      return c;
    }(), l = function() {
      function c(f) {
        this.outStream = f;
        this.buf = null;
        this.size = this.pos = 0;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      }
      c.prototype.create = function(f) {
        this.buf = new Uint8Array(f);
        this.pos = 0;
        this.size = f;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      };
      c.prototype.putByte = function(f) {
        this.totalPos++;
        this.buf[this.pos++] = f;
        this.pos === this.size && (this.flush(), this.pos = 0, this.isFull = !0);
      };
      c.prototype.getByte = function(f) {
        return this.buf[f <= this.pos ? this.pos - f : this.size - f + this.pos];
      };
      c.prototype.flush = function() {
        this.writePos < this.pos && (this.outStream.writeBytes(this.buf.subarray(this.writePos, this.pos)), this.writePos = this.pos === this.size ? 0 : this.pos);
      };
      c.prototype.copyMatch = function(f, c) {
        for (var d = this.pos, g = this.size, e = this.buf, b = f <= d ? d - f : g - f + d, a = c;0 < a;) {
          for (var l = Math.min(Math.min(a, g - d), g - b), h = 0;h < l;h++) {
            var v = e[b++];
            e[d++] = v;
          }
          d === g && (this.pos = d, this.flush(), d = 0, this.isFull = !0);
          b === g && (b = 0);
          a -= l;
        }
        this.pos = d;
        this.totalPos += c;
      };
      c.prototype.checkDistance = function(f) {
        return f <= this.pos || this.isFull;
      };
      c.prototype.isEmpty = function() {
        return 0 === this.pos && !this.isFull;
      };
      return c;
    }(), r = function() {
      function c(f) {
        this.inStream = f;
        this.code = this.range = 0;
        this.corrupted = !1;
      }
      c.prototype.init = function() {
        0 !== this.inStream.readByte() && (this.corrupted = !0);
        this.range = -1;
        for (var f = 0, c = 0;4 > c;c++) {
          f = f << 8 | this.inStream.readByte();
        }
        f === this.range && (this.corrupted = !0);
        this.code = f;
      };
      c.prototype.isFinishedOK = function() {
        return 0 === this.code;
      };
      c.prototype.decodeDirectBits = function(f) {
        var c = 0, d = this.range, g = this.code;
        do {
          var d = d >>> 1 | 0, g = g - d | 0, e = g >> 31, g = g + (d & e) | 0;
          g === d && (this.corrupted = !0);
          0 <= d && 16777216 > d && (d <<= 8, g = g << 8 | this.inStream.readByte());
          c = (c << 1) + e + 1 | 0;
        } while (--f);
        this.range = d;
        this.code = g;
        return c;
      };
      c.prototype.decodeBit = function(f, c) {
        var d = this.range, g = this.code, e = f[c], b = (d >>> 11) * e;
        g >>> 0 < b ? (e = e + (2048 - e >> 5) | 0, d = b | 0, b = 0) : (e = e - (e >> 5) | 0, g = g - b | 0, d = d - b | 0, b = 1);
        f[c] = e & 65535;
        0 <= d && 16777216 > d && (d <<= 8, g = g << 8 | this.inStream.readByte());
        this.range = d;
        this.code = g;
        return b;
      };
      return c;
    }(), h = function() {
      function d(f) {
        this.numBits = f;
        this.probs = u(1 << f);
      }
      d.prototype.decode = function(f) {
        for (var d = 1, c = 0;c < this.numBits;c++) {
          d = (d << 1) + f.decodeBit(this.probs, d);
        }
        return d - (1 << this.numBits);
      };
      d.prototype.reverseDecode = function(f) {
        return a(this.probs, 0, this.numBits, f);
      };
      return d;
    }(), t = function() {
      function d() {
        this.choice = u(2);
        this.lowCoder = w(3, 16);
        this.midCoder = w(3, 16);
        this.highCoder = new h(8);
      }
      d.prototype.decode = function(f, d) {
        return 0 === f.decodeBit(this.choice, 0) ? this.lowCoder[d].decode(f) : 0 === f.decodeBit(this.choice, 1) ? 8 + this.midCoder[d].decode(f) : 16 + this.highCoder.decode(f);
      };
      return d;
    }(), q = function() {
      function d(f, c) {
        this.rangeDec = new r(f);
        this.outWindow = new l(c);
        this.markerIsMandatory = !1;
        this.dictSizeInProperties = this.dictSize = this.lp = this.pb = this.lc = 0;
        this.leftToUnpack = this.unpackSize = void 0;
        this.reps = new Int32Array(4);
        this.state = 0;
      }
      d.prototype.decodeProperties = function(f) {
        var d = f[0];
        if (225 <= d) {
          throw Error("Incorrect LZMA properties");
        }
        this.lc = d % 9;
        d = d / 9 | 0;
        this.pb = d / 5 | 0;
        this.lp = d % 5;
        for (d = this.dictSizeInProperties = 0;4 > d;d++) {
          this.dictSizeInProperties |= f[d + 1] << 8 * d;
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
      d.prototype.decodeLiteral = function(f, d) {
        var c = this.outWindow, g = this.rangeDec, e = 0;
        c.isEmpty() || (e = c.getByte(1));
        var b = 1, e = 768 * (((c.totalPos & (1 << this.lp) - 1) << this.lc) + (e >> 8 - this.lc));
        if (7 <= f) {
          c = c.getByte(d + 1);
          do {
            var a = c >> 7 & 1, c = c << 1, l = g.decodeBit(this.litProbs, e + ((1 + a << 8) + b)), b = b << 1 | l;
            if (a !== l) {
              break;
            }
          } while (256 > b);
        }
        for (;256 > b;) {
          b = b << 1 | g.decodeBit(this.litProbs, e + b);
        }
        return b - 256 & 255;
      };
      d.prototype.decodeDistance = function(f) {
        var d = f;
        3 < d && (d = 3);
        f = this.rangeDec;
        d = this.posSlotDecoder[d].decode(f);
        if (4 > d) {
          return d;
        }
        var c = (d >> 1) - 1, g = (2 | d & 1) << c;
        14 > d ? g = g + a(this.posDecoders, g - d, c, f) | 0 : (g = g + (f.decodeDirectBits(c - 4) << 4) | 0, g = g + this.alignDecoder.reverseDecode(f) | 0);
        return g;
      };
      d.prototype.init = function() {
        this.litProbs = u(768 << this.lc + this.lp);
        this.posSlotDecoder = w(6, 4);
        this.alignDecoder = new h(4);
        this.posDecoders = u(115);
        this.isMatch = u(192);
        this.isRep = u(12);
        this.isRepG0 = u(12);
        this.isRepG1 = u(12);
        this.isRepG2 = u(12);
        this.isRep0Long = u(192);
        this.lenDecoder = new t;
        this.repLenDecoder = new t;
      };
      d.prototype.decode = function(f) {
        for (var d = this.rangeDec, c = this.outWindow, b = this.pb, a = this.dictSize, l = this.markerIsMandatory, h = this.leftToUnpack, r = this.isMatch, m = this.isRep, t = this.isRepG0, q = this.isRepG1, k = this.isRepG2, u = this.isRep0Long, p = this.lenDecoder, w = this.repLenDecoder, z = this.reps[0], B = this.reps[1], E = this.reps[2], A = this.reps[3], C = this.state;;) {
          if (f && 48 > d.inStream.available) {
            this.outWindow.flush();
            break;
          }
          if (0 === h && !l && (this.outWindow.flush(), d.isFinishedOK())) {
            return e;
          }
          var F = c.totalPos & (1 << b) - 1;
          if (0 === d.decodeBit(r, (C << 4) + F)) {
            if (0 === h) {
              return n;
            }
            c.putByte(this.decodeLiteral(C, z));
            C = 4 > C ? 0 : 10 > C ? C - 3 : C - 6;
            h--;
          } else {
            if (0 !== d.decodeBit(m, C)) {
              if (0 === h || c.isEmpty()) {
                return n;
              }
              if (0 === d.decodeBit(t, C)) {
                if (0 === d.decodeBit(u, (C << 4) + F)) {
                  C = 7 > C ? 9 : 11;
                  c.putByte(c.getByte(z + 1));
                  h--;
                  continue;
                }
              } else {
                var G;
                0 === d.decodeBit(q, C) ? G = B : (0 === d.decodeBit(k, C) ? G = E : (G = A, A = E), E = B);
                B = z;
                z = G;
              }
              F = w.decode(d, F);
              C = 7 > C ? 8 : 11;
            } else {
              A = E;
              E = B;
              B = z;
              F = p.decode(d, F);
              C = 7 > C ? 7 : 10;
              z = this.decodeDistance(F);
              if (-1 === z) {
                return this.outWindow.flush(), d.isFinishedOK() ? v : n;
              }
              if (0 === h || z >= a || !c.checkDistance(z)) {
                return n;
              }
            }
            F += 2;
            G = !1;
            void 0 !== h && h < F && (F = h, G = !0);
            c.copyMatch(z + 1, F);
            h -= F;
            if (G) {
              return n;
            }
          }
        }
        this.state = C;
        this.reps[0] = z;
        this.reps[1] = B;
        this.reps[2] = E;
        this.reps[3] = A;
        this.leftToUnpack = h;
        return g;
      };
      d.prototype.flushOutput = function() {
        this.outWindow.flush();
      };
      return d;
    }(), n = 0, v = 1, e = 2, g = 3, d;
    (function(d) {
      d[d.WAIT_FOR_LZMA_HEADER = 0] = "WAIT_FOR_LZMA_HEADER";
      d[d.WAIT_FOR_SWF_HEADER = 1] = "WAIT_FOR_SWF_HEADER";
      d[d.PROCESS_DATA = 2] = "PROCESS_DATA";
      d[d.CLOSED = 3] = "CLOSED";
      d[d.ERROR = 4] = "ERROR";
    })(d || (d = {}));
    d = function() {
      function d(f) {
        void 0 === f && (f = !1);
        this._state = f ? 1 : 0;
        this.buffer = null;
      }
      d.prototype.push = function(f) {
        if (2 > this._state) {
          var d = this.buffer ? this.buffer.length : 0, c = (1 === this._state ? 17 : 13) + 5;
          if (d + f.length < c) {
            c = new Uint8Array(d + f.length);
            0 < d && c.set(this.buffer);
            c.set(f, d);
            this.buffer = c;
            return;
          }
          var e = new Uint8Array(c);
          0 < d && e.set(this.buffer);
          e.set(f.subarray(0, c - d), d);
          this._inStream = new m;
          this._inStream.append(e.subarray(c - 5));
          this._outStream = new b(function(d) {
            this.onData.call(null, d);
          }.bind(this));
          this._decoder = new q(this._inStream, this._outStream);
          if (1 === this._state) {
            this._decoder.decodeProperties(e.subarray(12, 17)), this._decoder.markerIsMandatory = !1, this._decoder.unpackSize = ((e[4] | e[5] << 8 | e[6] << 16 | e[7] << 24) >>> 0) - 8;
          } else {
            this._decoder.decodeProperties(e.subarray(0, 5));
            for (var d = 0, a = !1, l = 0;8 > l;l++) {
              var h = e[5 + l];
              255 !== h && (a = !0);
              d |= h << 8 * l;
            }
            this._decoder.markerIsMandatory = !a;
            this._decoder.unpackSize = a ? d : void 0;
          }
          this._decoder.create();
          f = f.subarray(c);
          this._state = 2;
        } else {
          if (2 !== this._state) {
            return;
          }
        }
        try {
          this._inStream.append(f);
          var v = this._decoder.decode(!0);
          this._inStream.compact();
          v !== g && this._checkError(v);
        } catch (n) {
          this._decoder.flushOutput(), this._decoder = null, this._error(n);
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
        d === n ? c = "LZMA decoding error" : d === g ? c = "Decoding is not complete" : d === v ? void 0 !== this._decoder.unpackSize && this._decoder.unpackSize !== this._outStream.processed && (c = "Finished with end marker before than specified size") : c = "Internal LZMA Error";
        c && this._error(c);
      };
      return d;
    }();
    k.LzmaDecoder = d;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    function u(b) {
      return "string" === typeof b ? b : void 0 == b ? null : b + "";
    }
    var a = k.Debug.notImplemented, w = k.StringUtilities.utf8decode, m = k.StringUtilities.utf8encode, b = k.NumberUtilities.clamp, l = function() {
      return function(b, a, e) {
        this.buffer = b;
        this.length = a;
        this.littleEndian = e;
      };
    }();
    p.PlainObjectDataBuffer = l;
    for (var r = new Uint32Array(33), h = 1, t = 0;32 >= h;h++) {
      r[h] = t = t << 1 | 1;
    }
    var q;
    (function(b) {
      b[b.U8 = 1] = "U8";
      b[b.I32 = 2] = "I32";
      b[b.F32 = 4] = "F32";
    })(q || (q = {}));
    h = function() {
      function h(b) {
        void 0 === b && (b = h.INITIAL_SIZE);
        this._buffer || (this._buffer = new ArrayBuffer(b), this._position = this._length = 0, this._resetViews(), this._littleEndian = h._nativeLittleEndian, this._bitLength = this._bitBuffer = 0);
      }
      h.FromArrayBuffer = function(b, e) {
        void 0 === e && (e = -1);
        var g = Object.create(h.prototype);
        g._buffer = b;
        g._length = -1 === e ? b.byteLength : e;
        g._position = 0;
        g._resetViews();
        g._littleEndian = h._nativeLittleEndian;
        g._bitBuffer = 0;
        g._bitLength = 0;
        return g;
      };
      h.FromPlainObject = function(b) {
        var e = h.FromArrayBuffer(b.buffer, b.length);
        e._littleEndian = b.littleEndian;
        return e;
      };
      h.prototype.toPlainObject = function() {
        return new l(this._buffer, this._length, this._littleEndian);
      };
      h.prototype._resetViews = function() {
        this._u8 = new Uint8Array(this._buffer);
        this._f32 = this._i32 = null;
      };
      h.prototype._requestViews = function(b) {
        0 === (this._buffer.byteLength & 3) && (null === this._i32 && b & 2 && (this._i32 = new Int32Array(this._buffer)), null === this._f32 && b & 4 && (this._f32 = new Float32Array(this._buffer)));
      };
      h.prototype.getBytes = function() {
        return new Uint8Array(this._buffer, 0, this._length);
      };
      h.prototype._ensureCapacity = function(b) {
        var e = this._buffer;
        if (!(e.byteLength >= b)) {
          for (var g = Math.max(e.byteLength, 1);g < b;) {
            g *= 2;
          }
          4294967295 < g && this.sec.throwError("RangeError", Errors.ParamRangeError);
          g = h._arrayBufferPool.acquire(g);
          b = this._u8;
          this._buffer = g;
          this._resetViews();
          this._u8.set(b);
          g = this._u8;
          for (b = b.length;b < g.length;b++) {
            g[b] = 0;
          }
          h._arrayBufferPool.release(e);
        }
      };
      h.prototype.clear = function() {
        this._position = this._length = 0;
      };
      h.prototype.readBoolean = function() {
        return 0 !== this.readUnsignedByte();
      };
      h.prototype.readByte = function() {
        return this.readUnsignedByte() << 24 >> 24;
      };
      h.prototype.readUnsignedByte = function() {
        this._position + 1 > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError);
        return this._u8[this._position++];
      };
      h.prototype.readBytes = function(b, e, g) {
        var d = this._position;
        e >>>= 0;
        g >>>= 0;
        0 === g && (g = this._length - d);
        d + g > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError);
        b.length < e + g && (b._ensureCapacity(e + g), b.length = e + g);
        b._u8.set(new Uint8Array(this._buffer, d, g), e);
        this._position += g;
      };
      h.prototype.readShort = function() {
        return this.readUnsignedShort() << 16 >> 16;
      };
      h.prototype.readUnsignedShort = function() {
        var b = this._u8, e = this._position;
        e + 2 > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError);
        var g = b[e + 0], b = b[e + 1];
        this._position = e + 2;
        return this._littleEndian ? b << 8 | g : g << 8 | b;
      };
      h.prototype.readInt = function() {
        var b = this._u8, e = this._position;
        e + 4 > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError);
        var g = b[e + 0], d = b[e + 1], c = b[e + 2], b = b[e + 3];
        this._position = e + 4;
        return this._littleEndian ? b << 24 | c << 16 | d << 8 | g : g << 24 | d << 16 | c << 8 | b;
      };
      h.prototype.readUnsignedInt = function() {
        return this.readInt() >>> 0;
      };
      h.prototype.readFloat = function() {
        var b = this._position;
        b + 4 > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError);
        this._position = b + 4;
        this._requestViews(4);
        if (this._littleEndian && 0 === (b & 3) && this._f32) {
          return this._f32[b >> 2];
        }
        var e = this._u8, g = k.IntegerUtilities.u8;
        this._littleEndian ? (g[0] = e[b + 0], g[1] = e[b + 1], g[2] = e[b + 2], g[3] = e[b + 3]) : (g[3] = e[b + 0], g[2] = e[b + 1], g[1] = e[b + 2], g[0] = e[b + 3]);
        return k.IntegerUtilities.f32[0];
      };
      h.prototype.readDouble = function() {
        var b = this._u8, e = this._position;
        e + 8 > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError);
        var g = k.IntegerUtilities.u8;
        this._littleEndian ? (g[0] = b[e + 0], g[1] = b[e + 1], g[2] = b[e + 2], g[3] = b[e + 3], g[4] = b[e + 4], g[5] = b[e + 5], g[6] = b[e + 6], g[7] = b[e + 7]) : (g[0] = b[e + 7], g[1] = b[e + 6], g[2] = b[e + 5], g[3] = b[e + 4], g[4] = b[e + 3], g[5] = b[e + 2], g[6] = b[e + 1], g[7] = b[e + 0]);
        this._position = e + 8;
        return k.IntegerUtilities.f64[0];
      };
      h.prototype.writeBoolean = function(b) {
        this.writeByte(b ? 1 : 0);
      };
      h.prototype.writeByte = function(b) {
        var e = this._position + 1;
        this._ensureCapacity(e);
        this._u8[this._position++] = b;
        e > this._length && (this._length = e);
      };
      h.prototype.writeUnsignedByte = function(b) {
        var e = this._position + 1;
        this._ensureCapacity(e);
        this._u8[this._position++] = b;
        e > this._length && (this._length = e);
      };
      h.prototype.writeRawBytes = function(b) {
        var e = this._position + b.length;
        this._ensureCapacity(e);
        this._u8.set(b, this._position);
        this._position = e;
        e > this._length && (this._length = e);
      };
      h.prototype.writeBytes = function(a, e, g) {
        k.isNullOrUndefined(a) && this.sec.throwError("TypeError", Errors.NullPointerError, "bytes");
        e >>>= 0;
        g >>>= 0;
        2 > arguments.length && (e = 0);
        3 > arguments.length && (g = 0);
        e === b(e, 0, a.length) && e + g === b(e + g, 0, a.length) || this.sec.throwError("RangeError", Errors.ParamRangeError);
        0 === g && (g = a.length - e);
        this.writeRawBytes(new Int8Array(a._buffer, e, g));
      };
      h.prototype.writeShort = function(b) {
        this.writeUnsignedShort(b);
      };
      h.prototype.writeUnsignedShort = function(b) {
        var e = this._position;
        this._ensureCapacity(e + 2);
        var g = this._u8;
        this._littleEndian ? (g[e + 0] = b, g[e + 1] = b >> 8) : (g[e + 0] = b >> 8, g[e + 1] = b);
        this._position = e += 2;
        e > this._length && (this._length = e);
      };
      h.prototype.writeInt = function(b) {
        this.writeUnsignedInt(b);
      };
      h.prototype.write2Ints = function(b, e) {
        this.write2UnsignedInts(b, e);
      };
      h.prototype.write4Ints = function(b, e, g, d) {
        this.write4UnsignedInts(b, e, g, d);
      };
      h.prototype.writeUnsignedInt = function(b) {
        var e = this._position;
        this._ensureCapacity(e + 4);
        this._requestViews(2);
        if (this._littleEndian === h._nativeLittleEndian && 0 === (e & 3) && this._i32) {
          this._i32[e >> 2] = b;
        } else {
          var g = this._u8;
          this._littleEndian ? (g[e + 0] = b, g[e + 1] = b >> 8, g[e + 2] = b >> 16, g[e + 3] = b >> 24) : (g[e + 0] = b >> 24, g[e + 1] = b >> 16, g[e + 2] = b >> 8, g[e + 3] = b);
        }
        this._position = e += 4;
        e > this._length && (this._length = e);
      };
      h.prototype.write2UnsignedInts = function(b, e) {
        var g = this._position;
        this._ensureCapacity(g + 8);
        this._requestViews(2);
        this._littleEndian === h._nativeLittleEndian && 0 === (g & 3) && this._i32 ? (this._i32[(g >> 2) + 0] = b, this._i32[(g >> 2) + 1] = e, this._position = g += 8, g > this._length && (this._length = g)) : (this.writeUnsignedInt(b), this.writeUnsignedInt(e));
      };
      h.prototype.write4UnsignedInts = function(b, e, g, d) {
        var c = this._position;
        this._ensureCapacity(c + 16);
        this._requestViews(2);
        this._littleEndian === h._nativeLittleEndian && 0 === (c & 3) && this._i32 ? (this._i32[(c >> 2) + 0] = b, this._i32[(c >> 2) + 1] = e, this._i32[(c >> 2) + 2] = g, this._i32[(c >> 2) + 3] = d, this._position = c += 16, c > this._length && (this._length = c)) : (this.writeUnsignedInt(b), this.writeUnsignedInt(e), this.writeUnsignedInt(g), this.writeUnsignedInt(d));
      };
      h.prototype.writeFloat = function(b) {
        var e = this._position;
        this._ensureCapacity(e + 4);
        this._requestViews(4);
        if (this._littleEndian === h._nativeLittleEndian && 0 === (e & 3) && this._f32) {
          this._f32[e >> 2] = b;
        } else {
          var g = this._u8;
          k.IntegerUtilities.f32[0] = b;
          b = k.IntegerUtilities.u8;
          this._littleEndian ? (g[e + 0] = b[0], g[e + 1] = b[1], g[e + 2] = b[2], g[e + 3] = b[3]) : (g[e + 0] = b[3], g[e + 1] = b[2], g[e + 2] = b[1], g[e + 3] = b[0]);
        }
        this._position = e += 4;
        e > this._length && (this._length = e);
      };
      h.prototype.write6Floats = function(b, e, g, d, c, f) {
        var a = this._position;
        this._ensureCapacity(a + 24);
        this._requestViews(4);
        this._littleEndian === h._nativeLittleEndian && 0 === (a & 3) && this._f32 ? (this._f32[(a >> 2) + 0] = b, this._f32[(a >> 2) + 1] = e, this._f32[(a >> 2) + 2] = g, this._f32[(a >> 2) + 3] = d, this._f32[(a >> 2) + 4] = c, this._f32[(a >> 2) + 5] = f, this._position = a += 24, a > this._length && (this._length = a)) : (this.writeFloat(b), this.writeFloat(e), this.writeFloat(g), this.writeFloat(d), this.writeFloat(c), this.writeFloat(f));
      };
      h.prototype.writeDouble = function(b) {
        var e = this._position;
        this._ensureCapacity(e + 8);
        var g = this._u8;
        k.IntegerUtilities.f64[0] = b;
        b = k.IntegerUtilities.u8;
        this._littleEndian ? (g[e + 0] = b[0], g[e + 1] = b[1], g[e + 2] = b[2], g[e + 3] = b[3], g[e + 4] = b[4], g[e + 5] = b[5], g[e + 6] = b[6], g[e + 7] = b[7]) : (g[e + 0] = b[7], g[e + 1] = b[6], g[e + 2] = b[5], g[e + 3] = b[4], g[e + 4] = b[3], g[e + 5] = b[2], g[e + 6] = b[1], g[e + 7] = b[0]);
        this._position = e += 8;
        e > this._length && (this._length = e);
      };
      h.prototype.readRawBytes = function() {
        return new Int8Array(this._buffer, 0, this._length);
      };
      h.prototype.writeUTF = function(b) {
        b = u(b);
        b = w(b);
        this.writeShort(b.length);
        this.writeRawBytes(b);
      };
      h.prototype.writeUTFBytes = function(b) {
        b = u(b);
        b = w(b);
        this.writeRawBytes(b);
      };
      h.prototype.readUTF = function() {
        return this.readUTFBytes(this.readShort());
      };
      h.prototype.readUTFBytes = function(b) {
        b >>>= 0;
        var e = this._position;
        e + b > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError);
        this._position += b;
        return m(new Int8Array(this._buffer, e, b));
      };
      Object.defineProperty(h.prototype, "length", {get:function() {
        return this._length;
      }, set:function(a) {
        a >>>= 0;
        a > this._buffer.byteLength && this._ensureCapacity(a);
        this._length = a;
        this._position = b(this._position, 0, this._length);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "bytesAvailable", {get:function() {
        return this._length - this._position;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "position", {get:function() {
        return this._position;
      }, set:function(b) {
        this._position = b >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "buffer", {get:function() {
        return this._buffer;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "bytes", {get:function() {
        return this._u8;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "ints", {get:function() {
        this._requestViews(2);
        return this._i32;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "objectEncoding", {get:function() {
        return this._objectEncoding;
      }, set:function(b) {
        this._objectEncoding = b >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "endian", {get:function() {
        return this._littleEndian ? "littleEndian" : "bigEndian";
      }, set:function(b) {
        b = u(b);
        this._littleEndian = "auto" === b ? h._nativeLittleEndian : "littleEndian" === b;
      }, enumerable:!0, configurable:!0});
      h.prototype.toString = function() {
        return m(new Int8Array(this._buffer, 0, this._length));
      };
      h.prototype.toBlob = function(b) {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)], {type:b});
      };
      h.prototype.writeMultiByte = function(b, e) {
        u(b);
        u(e);
        a("packageInternal flash.utils.ObjectOutput::writeMultiByte");
      };
      h.prototype.readMultiByte = function(b, e) {
        u(e);
        a("packageInternal flash.utils.ObjectInput::readMultiByte");
      };
      h.prototype.getValue = function(b) {
        b |= 0;
        return b >= this._length ? void 0 : this._u8[b];
      };
      h.prototype.setValue = function(b, e) {
        b |= 0;
        var g = b + 1;
        this._ensureCapacity(g);
        this._u8[b] = e;
        g > this._length && (this._length = g);
      };
      h.prototype.readFixed = function() {
        return this.readInt() / 65536;
      };
      h.prototype.readFixed8 = function() {
        return this.readShort() / 256;
      };
      h.prototype.readFloat16 = function() {
        var b = this.readUnsignedShort(), e = b >> 15 ? -1 : 1, g = (b & 31744) >> 10, b = b & 1023;
        return g ? 31 === g ? b ? NaN : Infinity * e : e * Math.pow(2, g - 15) * (1 + b / 1024) : e * Math.pow(2, -14) * (b / 1024);
      };
      h.prototype.readEncodedU32 = function() {
        var b = this.readUnsignedByte();
        if (!(b & 128)) {
          return b;
        }
        b = b & 127 | this.readUnsignedByte() << 7;
        if (!(b & 16384)) {
          return b;
        }
        b = b & 16383 | this.readUnsignedByte() << 14;
        if (!(b & 2097152)) {
          return b;
        }
        b = b & 2097151 | this.readUnsignedByte() << 21;
        return b & 268435456 ? b & 268435455 | this.readUnsignedByte() << 28 : b;
      };
      h.prototype.readBits = function(b) {
        return this.readUnsignedBits(b) << 32 - b >> 32 - b;
      };
      h.prototype.readUnsignedBits = function(b) {
        for (var e = this._bitBuffer, g = this._bitLength;b > g;) {
          e = e << 8 | this.readUnsignedByte(), g += 8;
        }
        g -= b;
        b = e >>> g & r[b];
        this._bitBuffer = e;
        this._bitLength = g;
        return b;
      };
      h.prototype.readFixedBits = function(b) {
        return this.readBits(b) / 65536;
      };
      h.prototype.readString = function(b) {
        var e = this._position;
        if (b) {
          e + b > this._length && this.sec.throwError("flash.errors.EOFError", Errors.EOFError), this._position += b;
        } else {
          b = 0;
          for (var g = e;g < this._length && this._u8[g];g++) {
            b++;
          }
          this._position += b + 1;
        }
        return m(new Int8Array(this._buffer, e, b));
      };
      h.prototype.align = function() {
        this._bitLength = this._bitBuffer = 0;
      };
      h.prototype.deflate = function() {
        this.compress("deflate");
      };
      h.prototype.inflate = function() {
        this.uncompress("deflate");
      };
      h.prototype.compress = function(b) {
        b = 0 === arguments.length ? "zlib" : u(b);
        var e;
        switch(b) {
          case "zlib":
            e = new p.Deflate(!0);
            break;
          case "deflate":
            e = new p.Deflate(!1);
            break;
          default:
            return;
        }
        var g = new h;
        e.onData = g.writeRawBytes.bind(g);
        e.push(this._u8.subarray(0, this._length));
        e.close();
        this._ensureCapacity(g._u8.length);
        this._u8.set(g._u8);
        this.length = g.length;
        this._position = 0;
      };
      h.prototype.uncompress = function(b) {
        b = 0 === arguments.length ? "zlib" : u(b);
        var e;
        switch(b) {
          case "zlib":
            e = p.Inflate.create(!0);
            break;
          case "deflate":
            e = p.Inflate.create(!1);
            break;
          case "lzma":
            e = new p.LzmaDecoder(!1);
            break;
          default:
            return;
        }
        var g = new h, d;
        e.onData = g.writeRawBytes.bind(g);
        e.onError = function(c) {
          return d = c;
        };
        e.push(this._u8.subarray(0, this._length));
        d && this.sec.throwError("IOError", Errors.CompressedDataError);
        e.close();
        this._ensureCapacity(g._u8.length);
        this._u8.set(g._u8);
        this.length = g.length;
        this._position = 0;
      };
      h._nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
      h.INITIAL_SIZE = 128;
      h._arrayBufferPool = new k.ArrayBufferPool;
      return h;
    }();
    p.DataBuffer = h;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var p = k.ArrayUtilities.DataBuffer, u = k.ArrayUtilities.ensureTypedArrayCapacity;
  (function(a) {
    a[a.BeginSolidFill = 1] = "BeginSolidFill";
    a[a.BeginGradientFill = 2] = "BeginGradientFill";
    a[a.BeginBitmapFill = 3] = "BeginBitmapFill";
    a[a.EndFill = 4] = "EndFill";
    a[a.LineStyleSolid = 5] = "LineStyleSolid";
    a[a.LineStyleGradient = 6] = "LineStyleGradient";
    a[a.LineStyleBitmap = 7] = "LineStyleBitmap";
    a[a.LineEnd = 8] = "LineEnd";
    a[a.MoveTo = 9] = "MoveTo";
    a[a.LineTo = 10] = "LineTo";
    a[a.CurveTo = 11] = "CurveTo";
    a[a.CubicCurveTo = 12] = "CubicCurveTo";
  })(k.PathCommand || (k.PathCommand = {}));
  (function(a) {
    a[a.Linear = 16] = "Linear";
    a[a.Radial = 18] = "Radial";
  })(k.GradientType || (k.GradientType = {}));
  (function(a) {
    a[a.Pad = 0] = "Pad";
    a[a.Reflect = 1] = "Reflect";
    a[a.Repeat = 2] = "Repeat";
  })(k.GradientSpreadMethod || (k.GradientSpreadMethod = {}));
  (function(a) {
    a[a.RGB = 0] = "RGB";
    a[a.LinearRGB = 1] = "LinearRGB";
  })(k.GradientInterpolationMethod || (k.GradientInterpolationMethod = {}));
  (function(a) {
    a[a.None = 0] = "None";
    a[a.Normal = 1] = "Normal";
    a[a.Vertical = 2] = "Vertical";
    a[a.Horizontal = 3] = "Horizontal";
  })(k.LineScaleMode || (k.LineScaleMode = {}));
  var a = function() {
    return function(a, b, l, r, h, t, q, n, v, e, g) {
      this.commands = a;
      this.commandsPosition = b;
      this.coordinates = l;
      this.morphCoordinates = r;
      this.coordinatesPosition = h;
      this.styles = t;
      this.stylesLength = q;
      this.morphStyles = n;
      this.morphStylesLength = v;
      this.hasFills = e;
      this.hasLines = g;
    };
  }();
  k.PlainObjectShapeData = a;
  var w;
  (function(a) {
    a[a.Commands = 32] = "Commands";
    a[a.Coordinates = 128] = "Coordinates";
    a[a.Styles = 16] = "Styles";
  })(w || (w = {}));
  w = function() {
    function m(b) {
      void 0 === b && (b = !0);
      b && this.clear();
    }
    m.FromPlainObject = function(b) {
      var a = new m(!1);
      a.commands = b.commands;
      a.coordinates = b.coordinates;
      a.morphCoordinates = b.morphCoordinates;
      a.commandsPosition = b.commandsPosition;
      a.coordinatesPosition = b.coordinatesPosition;
      a.styles = p.FromArrayBuffer(b.styles, b.stylesLength);
      a.styles.endian = "auto";
      b.morphStyles && (a.morphStyles = p.FromArrayBuffer(b.morphStyles, b.morphStylesLength), a.morphStyles.endian = "auto");
      a.hasFills = b.hasFills;
      a.hasLines = b.hasLines;
      return a;
    };
    m.prototype.moveTo = function(b, a) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 9;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = a;
    };
    m.prototype.lineTo = function(b, a) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 10;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = a;
    };
    m.prototype.curveTo = function(b, a, r, h) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = r;
      this.coordinates[this.coordinatesPosition++] = h;
    };
    m.prototype.cubicCurveTo = function(b, a, r, h, t, q) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = r;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = t;
      this.coordinates[this.coordinatesPosition++] = q;
    };
    m.prototype.beginFill = function(b) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 1;
      this.styles.writeUnsignedInt(b);
      this.hasFills = !0;
    };
    m.prototype.writeMorphFill = function(b) {
      this.morphStyles.writeUnsignedInt(b);
    };
    m.prototype.endFill = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 4;
    };
    m.prototype.endLine = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 8;
    };
    m.prototype.lineStyle = function(b, a, r, h, t, q, n) {
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5;
      this.coordinates[this.coordinatesPosition++] = b;
      b = this.styles;
      b.writeUnsignedInt(a);
      b.writeBoolean(r);
      b.writeUnsignedByte(h);
      b.writeUnsignedByte(t);
      b.writeUnsignedByte(q);
      b.writeUnsignedByte(n);
      this.hasLines = !0;
    };
    m.prototype.writeMorphLineStyle = function(b, a) {
      this.morphCoordinates[this.coordinatesPosition - 1] = b;
      this.morphStyles.writeUnsignedInt(a);
    };
    m.prototype.beginBitmap = function(b, a, r, h, t) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = b;
      b = this.styles;
      b.writeUnsignedInt(a);
      this._writeStyleMatrix(r, !1);
      b.writeBoolean(h);
      b.writeBoolean(t);
      this.hasFills = !0;
    };
    m.prototype.writeMorphBitmap = function(b) {
      this._writeStyleMatrix(b, !0);
    };
    m.prototype.beginGradient = function(b, a, r, h, t, q, n, m) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = b;
      b = this.styles;
      b.writeUnsignedByte(h);
      b.writeShort(m);
      this._writeStyleMatrix(t, !1);
      h = a.length;
      b.writeByte(h);
      for (t = 0;t < h;t++) {
        b.writeUnsignedByte(r[t]), b.writeUnsignedInt(a[t]);
      }
      b.writeUnsignedByte(q);
      b.writeUnsignedByte(n);
      this.hasFills = !0;
    };
    m.prototype.writeMorphGradient = function(b, a, r) {
      this._writeStyleMatrix(r, !0);
      r = this.morphStyles;
      for (var h = 0;h < b.length;h++) {
        r.writeUnsignedByte(a[h]), r.writeUnsignedInt(b[h]);
      }
    };
    m.prototype.writeCommandAndCoordinates = function(b, a, r) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = r;
    };
    m.prototype.writeCoordinates = function(b, a) {
      this.ensurePathCapacities(0, 2);
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = a;
    };
    m.prototype.writeMorphCoordinates = function(b, a) {
      this.morphCoordinates = u(this.morphCoordinates, this.coordinatesPosition);
      this.morphCoordinates[this.coordinatesPosition - 2] = b;
      this.morphCoordinates[this.coordinatesPosition - 1] = a;
    };
    m.prototype.clear = function() {
      this.commandsPosition = this.coordinatesPosition = 0;
      this.commands = new Uint8Array(32);
      this.coordinates = new Int32Array(128);
      this.styles = new p(16);
      this.styles.endian = "auto";
      this.hasFills = this.hasLines = !1;
    };
    m.prototype.isEmpty = function() {
      return 0 === this.commandsPosition;
    };
    m.prototype.clone = function() {
      var b = new m(!1);
      b.commands = new Uint8Array(this.commands);
      b.commandsPosition = this.commandsPosition;
      b.coordinates = new Int32Array(this.coordinates);
      b.coordinatesPosition = this.coordinatesPosition;
      b.styles = new p(this.styles.length);
      b.styles.writeRawBytes(this.styles.bytes.subarray(0, this.styles.length));
      this.morphStyles && (b.morphStyles = new p(this.morphStyles.length), b.morphStyles.writeRawBytes(this.morphStyles.bytes.subarray(0, this.morphStyles.length)));
      b.hasFills = this.hasFills;
      b.hasLines = this.hasLines;
      return b;
    };
    m.prototype.toPlainObject = function() {
      return new a(this.commands, this.commandsPosition, this.coordinates, this.morphCoordinates, this.coordinatesPosition, this.styles.buffer, this.styles.length, this.morphStyles && this.morphStyles.buffer, this.morphStyles ? this.morphStyles.length : 0, this.hasFills, this.hasLines);
    };
    Object.defineProperty(m.prototype, "buffers", {get:function() {
      var b = [this.commands.buffer, this.coordinates.buffer, this.styles.buffer];
      this.morphCoordinates && b.push(this.morphCoordinates.buffer);
      this.morphStyles && b.push(this.morphStyles.buffer);
      return b;
    }, enumerable:!0, configurable:!0});
    m.prototype._writeStyleMatrix = function(b, a) {
      (a ? this.morphStyles : this.styles).write6Floats(b.a, b.b, b.c, b.d, b.tx, b.ty);
    };
    m.prototype.ensurePathCapacities = function(b, a) {
      this.commands = u(this.commands, this.commandsPosition + b);
      this.coordinates = u(this.coordinates, this.coordinatesPosition + a);
    };
    return m;
  }();
  k.ShapeData = w;
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(k) {
      (function(a) {
        a[a.CODE_END = 0] = "CODE_END";
        a[a.CODE_SHOW_FRAME = 1] = "CODE_SHOW_FRAME";
        a[a.CODE_DEFINE_SHAPE = 2] = "CODE_DEFINE_SHAPE";
        a[a.CODE_FREE_CHARACTER = 3] = "CODE_FREE_CHARACTER";
        a[a.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        a[a.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        a[a.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        a[a.CODE_DEFINE_BUTTON = 7] = "CODE_DEFINE_BUTTON";
        a[a.CODE_JPEG_TABLES = 8] = "CODE_JPEG_TABLES";
        a[a.CODE_SET_BACKGROUND_COLOR = 9] = "CODE_SET_BACKGROUND_COLOR";
        a[a.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        a[a.CODE_DEFINE_TEXT = 11] = "CODE_DEFINE_TEXT";
        a[a.CODE_DO_ACTION = 12] = "CODE_DO_ACTION";
        a[a.CODE_DEFINE_FONT_INFO = 13] = "CODE_DEFINE_FONT_INFO";
        a[a.CODE_DEFINE_SOUND = 14] = "CODE_DEFINE_SOUND";
        a[a.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        a[a.CODE_STOP_SOUND = 16] = "CODE_STOP_SOUND";
        a[a.CODE_DEFINE_BUTTON_SOUND = 17] = "CODE_DEFINE_BUTTON_SOUND";
        a[a.CODE_SOUND_STREAM_HEAD = 18] = "CODE_SOUND_STREAM_HEAD";
        a[a.CODE_SOUND_STREAM_BLOCK = 19] = "CODE_SOUND_STREAM_BLOCK";
        a[a.CODE_DEFINE_BITS_LOSSLESS = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        a[a.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        a[a.CODE_DEFINE_SHAPE2 = 22] = "CODE_DEFINE_SHAPE2";
        a[a.CODE_DEFINE_BUTTON_CXFORM = 23] = "CODE_DEFINE_BUTTON_CXFORM";
        a[a.CODE_PROTECT = 24] = "CODE_PROTECT";
        a[a.CODE_PATHS_ARE_POSTSCRIPT = 25] = "CODE_PATHS_ARE_POSTSCRIPT";
        a[a.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        a[a.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        a[a.CODE_SYNC_FRAME = 29] = "CODE_SYNC_FRAME";
        a[a.CODE_FREE_ALL = 31] = "CODE_FREE_ALL";
        a[a.CODE_DEFINE_SHAPE3 = 32] = "CODE_DEFINE_SHAPE3";
        a[a.CODE_DEFINE_TEXT2 = 33] = "CODE_DEFINE_TEXT2";
        a[a.CODE_DEFINE_BUTTON2 = 34] = "CODE_DEFINE_BUTTON2";
        a[a.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        a[a.CODE_DEFINE_BITS_LOSSLESS2 = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        a[a.CODE_DEFINE_EDIT_TEXT = 37] = "CODE_DEFINE_EDIT_TEXT";
        a[a.CODE_DEFINE_VIDEO = 38] = "CODE_DEFINE_VIDEO";
        a[a.CODE_DEFINE_SPRITE = 39] = "CODE_DEFINE_SPRITE";
        a[a.CODE_NAME_CHARACTER = 40] = "CODE_NAME_CHARACTER";
        a[a.CODE_PRODUCT_INFO = 41] = "CODE_PRODUCT_INFO";
        a[a.CODE_DEFINE_TEXT_FORMAT = 42] = "CODE_DEFINE_TEXT_FORMAT";
        a[a.CODE_FRAME_LABEL = 43] = "CODE_FRAME_LABEL";
        a[a.CODE_DEFINE_BEHAVIOUR = 44] = "CODE_DEFINE_BEHAVIOUR";
        a[a.CODE_SOUND_STREAM_HEAD2 = 45] = "CODE_SOUND_STREAM_HEAD2";
        a[a.CODE_DEFINE_MORPH_SHAPE = 46] = "CODE_DEFINE_MORPH_SHAPE";
        a[a.CODE_GENERATE_FRAME = 47] = "CODE_GENERATE_FRAME";
        a[a.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        a[a.CODE_GEN_COMMAND = 49] = "CODE_GEN_COMMAND";
        a[a.CODE_DEFINE_COMMAND_OBJECT = 50] = "CODE_DEFINE_COMMAND_OBJECT";
        a[a.CODE_CHARACTER_SET = 51] = "CODE_CHARACTER_SET";
        a[a.CODE_EXTERNAL_FONT = 52] = "CODE_EXTERNAL_FONT";
        a[a.CODE_DEFINE_FUNCTION = 53] = "CODE_DEFINE_FUNCTION";
        a[a.CODE_PLACE_FUNCTION = 54] = "CODE_PLACE_FUNCTION";
        a[a.CODE_GEN_TAG_OBJECTS = 55] = "CODE_GEN_TAG_OBJECTS";
        a[a.CODE_EXPORT_ASSETS = 56] = "CODE_EXPORT_ASSETS";
        a[a.CODE_IMPORT_ASSETS = 57] = "CODE_IMPORT_ASSETS";
        a[a.CODE_ENABLE_DEBUGGER = 58] = "CODE_ENABLE_DEBUGGER";
        a[a.CODE_DO_INIT_ACTION = 59] = "CODE_DO_INIT_ACTION";
        a[a.CODE_DEFINE_VIDEO_STREAM = 60] = "CODE_DEFINE_VIDEO_STREAM";
        a[a.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
        a[a.CODE_DEFINE_FONT_INFO2 = 62] = "CODE_DEFINE_FONT_INFO2";
        a[a.CODE_DEBUG_ID = 63] = "CODE_DEBUG_ID";
        a[a.CODE_ENABLE_DEBUGGER2 = 64] = "CODE_ENABLE_DEBUGGER2";
        a[a.CODE_SCRIPT_LIMITS = 65] = "CODE_SCRIPT_LIMITS";
        a[a.CODE_SET_TAB_INDEX = 66] = "CODE_SET_TAB_INDEX";
        a[a.CODE_FILE_ATTRIBUTES = 69] = "CODE_FILE_ATTRIBUTES";
        a[a.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        a[a.CODE_IMPORT_ASSETS2 = 71] = "CODE_IMPORT_ASSETS2";
        a[a.CODE_DO_ABC_DEFINE = 72] = "CODE_DO_ABC_DEFINE";
        a[a.CODE_DEFINE_FONT_ALIGN_ZONES = 73] = "CODE_DEFINE_FONT_ALIGN_ZONES";
        a[a.CODE_CSM_TEXT_SETTINGS = 74] = "CODE_CSM_TEXT_SETTINGS";
        a[a.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        a[a.CODE_SYMBOL_CLASS = 76] = "CODE_SYMBOL_CLASS";
        a[a.CODE_METADATA = 77] = "CODE_METADATA";
        a[a.CODE_DEFINE_SCALING_GRID = 78] = "CODE_DEFINE_SCALING_GRID";
        a[a.CODE_DO_ABC = 82] = "CODE_DO_ABC";
        a[a.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        a[a.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        a[a.CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA = 86] = "CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA";
        a[a.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        a[a.CODE_DEFINE_FONT_NAME = 88] = "CODE_DEFINE_FONT_NAME";
        a[a.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
        a[a.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        a[a.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
        a[a.CODE_TELEMETRY = 93] = "CODE_TELEMETRY";
      })(k.SwfTagCode || (k.SwfTagCode = {}));
      (function(a) {
        a[a.CODE_DEFINE_SHAPE = 2] = "CODE_DEFINE_SHAPE";
        a[a.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        a[a.CODE_DEFINE_BUTTON = 7] = "CODE_DEFINE_BUTTON";
        a[a.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        a[a.CODE_DEFINE_TEXT = 11] = "CODE_DEFINE_TEXT";
        a[a.CODE_DEFINE_SOUND = 14] = "CODE_DEFINE_SOUND";
        a[a.CODE_DEFINE_BITS_LOSSLESS = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        a[a.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        a[a.CODE_DEFINE_SHAPE2 = 22] = "CODE_DEFINE_SHAPE2";
        a[a.CODE_DEFINE_SHAPE3 = 32] = "CODE_DEFINE_SHAPE3";
        a[a.CODE_DEFINE_TEXT2 = 33] = "CODE_DEFINE_TEXT2";
        a[a.CODE_DEFINE_BUTTON2 = 34] = "CODE_DEFINE_BUTTON2";
        a[a.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        a[a.CODE_DEFINE_BITS_LOSSLESS2 = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        a[a.CODE_DEFINE_EDIT_TEXT = 37] = "CODE_DEFINE_EDIT_TEXT";
        a[a.CODE_DEFINE_SPRITE = 39] = "CODE_DEFINE_SPRITE";
        a[a.CODE_DEFINE_MORPH_SHAPE = 46] = "CODE_DEFINE_MORPH_SHAPE";
        a[a.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        a[a.CODE_DEFINE_VIDEO_STREAM = 60] = "CODE_DEFINE_VIDEO_STREAM";
        a[a.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        a[a.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        a[a.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        a[a.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        a[a.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        a[a.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(k.DefinitionTags || (k.DefinitionTags = {}));
      (function(a) {
        a[a.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        a[a.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        a[a.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        a[a.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
      })(k.ImageDefinitionTags || (k.ImageDefinitionTags = {}));
      (function(a) {
        a[a.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        a[a.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        a[a.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        a[a.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(k.FontDefinitionTags || (k.FontDefinitionTags = {}));
      (function(a) {
        a[a.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        a[a.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        a[a.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        a[a.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        a[a.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        a[a.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        a[a.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
        a[a.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
      })(k.ControlTags || (k.ControlTags = {}));
      (function(a) {
        a[a.Move = 1] = "Move";
        a[a.HasCharacter = 2] = "HasCharacter";
        a[a.HasMatrix = 4] = "HasMatrix";
        a[a.HasColorTransform = 8] = "HasColorTransform";
        a[a.HasRatio = 16] = "HasRatio";
        a[a.HasName = 32] = "HasName";
        a[a.HasClipDepth = 64] = "HasClipDepth";
        a[a.HasClipActions = 128] = "HasClipActions";
        a[a.HasFilterList = 256] = "HasFilterList";
        a[a.HasBlendMode = 512] = "HasBlendMode";
        a[a.HasCacheAsBitmap = 1024] = "HasCacheAsBitmap";
        a[a.HasClassName = 2048] = "HasClassName";
        a[a.HasImage = 4096] = "HasImage";
        a[a.HasVisible = 8192] = "HasVisible";
        a[a.OpaqueBackground = 16384] = "OpaqueBackground";
        a[a.Reserved = 32768] = "Reserved";
      })(k.PlaceObjectFlags || (k.PlaceObjectFlags = {}));
      (function(a) {
        a[a.Load = 1] = "Load";
        a[a.EnterFrame = 2] = "EnterFrame";
        a[a.Unload = 4] = "Unload";
        a[a.MouseMove = 8] = "MouseMove";
        a[a.MouseDown = 16] = "MouseDown";
        a[a.MouseUp = 32] = "MouseUp";
        a[a.KeyDown = 64] = "KeyDown";
        a[a.KeyUp = 128] = "KeyUp";
        a[a.Data = 256] = "Data";
        a[a.Initialize = 512] = "Initialize";
        a[a.Press = 1024] = "Press";
        a[a.Release = 2048] = "Release";
        a[a.ReleaseOutside = 4096] = "ReleaseOutside";
        a[a.RollOver = 8192] = "RollOver";
        a[a.RollOut = 16384] = "RollOut";
        a[a.DragOver = 32768] = "DragOver";
        a[a.DragOut = 65536] = "DragOut";
        a[a.KeyPress = 131072] = "KeyPress";
        a[a.Construct = 262144] = "Construct";
      })(k.AVM1ClipEvents || (k.AVM1ClipEvents = {}));
      (function(a) {
        a[a.StateUp = 1] = "StateUp";
        a[a.StateOver = 2] = "StateOver";
        a[a.StateDown = 4] = "StateDown";
        a[a.StateHitTest = 8] = "StateHitTest";
        a[a.HasFilterList = 16] = "HasFilterList";
        a[a.HasBlendMode = 32] = "HasBlendMode";
      })(k.ButtonCharacterFlags || (k.ButtonCharacterFlags = {}));
      (function(a) {
        a[a.Bold = 1] = "Bold";
        a[a.Italic = 2] = "Italic";
        a[a.WideOrHasFontData = 4] = "WideOrHasFontData";
        a[a.WideOffset = 8] = "WideOffset";
        a[a.Ansi = 16] = "Ansi";
        a[a.SmallText = 32] = "SmallText";
        a[a.ShiftJis = 64] = "ShiftJis";
        a[a.HasLayout = 128] = "HasLayout";
      })(k.FontFlags || (k.FontFlags = {}));
      (function(a) {
        a[a.HasMoveX = 1] = "HasMoveX";
        a[a.HasMoveY = 2] = "HasMoveY";
        a[a.HasColor = 4] = "HasColor";
        a[a.HasFont = 8] = "HasFont";
      })(k.TextRecordFlags || (k.TextRecordFlags = {}));
      (function(a) {
        a[a.HasInPoint = 1] = "HasInPoint";
        a[a.HasOutPoint = 2] = "HasOutPoint";
        a[a.HasLoops = 4] = "HasLoops";
        a[a.HasEnvelope = 8] = "HasEnvelope";
        a[a.NoMultiple = 16] = "NoMultiple";
        a[a.Stop = 32] = "Stop";
      })(k.SoundInfoFlags || (k.SoundInfoFlags = {}));
      (function(a) {
        a[a.HasFont = 1] = "HasFont";
        a[a.HasMaxLength = 2] = "HasMaxLength";
        a[a.HasColor = 4] = "HasColor";
        a[a.ReadOnly = 8] = "ReadOnly";
        a[a.Password = 16] = "Password";
        a[a.Multiline = 32] = "Multiline";
        a[a.WordWrap = 64] = "WordWrap";
        a[a.HasText = 128] = "HasText";
        a[a.UseOutlines = 256] = "UseOutlines";
        a[a.Html = 512] = "Html";
        a[a.WasStatic = 1024] = "WasStatic";
        a[a.Border = 2048] = "Border";
        a[a.NoSelect = 4096] = "NoSelect";
        a[a.HasLayout = 8192] = "HasLayout";
        a[a.AutoSize = 16384] = "AutoSize";
        a[a.HasFontClass = 32768] = "HasFontClass";
      })(k.TextFlags || (k.TextFlags = {}));
      (function(a) {
        a[a.UsesScalingStrokes = 1] = "UsesScalingStrokes";
        a[a.UsesNonScalingStrokes = 2] = "UsesNonScalingStrokes";
        a[a.UsesFillWindingRule = 4] = "UsesFillWindingRule";
        a[a.IsMorph = 8] = "IsMorph";
      })(k.ShapeFlags || (k.ShapeFlags = {}));
      (function(a) {
        a[a.Move = 1] = "Move";
        a[a.HasFillStyle0 = 2] = "HasFillStyle0";
        a[a.HasFillStyle1 = 4] = "HasFillStyle1";
        a[a.HasLineStyle = 8] = "HasLineStyle";
        a[a.HasNewStyles = 16] = "HasNewStyles";
        a[a.IsStraight = 32] = "IsStraight";
        a[a.IsGeneral = 64] = "IsGeneral";
        a[a.IsVertical = 128] = "IsVertical";
      })(k.ShapeRecordFlags || (k.ShapeRecordFlags = {}));
    })(k.Parser || (k.Parser = {}));
  })(k.SWF || (k.SWF = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var p = k.Debug.unexpected, u = function() {
    function a(a, m, b, l) {
      this.url = a;
      this.method = m;
      this.mimeType = b;
      this.data = l;
    }
    a.prototype.readAll = function(a, m) {
      var b = this.url, l = this.xhr = new XMLHttpRequest({mozSystem:!0});
      l.open(this.method || "GET", this.url, !0);
      l.responseType = "arraybuffer";
      a && (l.onprogress = function(b) {
        a(l.response, b.loaded, b.total);
      });
      l.onreadystatechange = function(a) {
        4 === l.readyState && (200 !== l.status && 0 !== l.status || null === l.response ? (p("Path: " + b + " not found."), m(null, l.statusText)) : m(l.response));
      };
      this.mimeType && l.setRequestHeader("Content-Type", this.mimeType);
      l.send(this.data || null);
    };
    a.prototype.readChunked = function(a, m, b, l, r, h) {
      if (0 >= a) {
        this.readAsync(m, b, l, r, h);
      } else {
        var t = 0, q = new Uint8Array(a), n = 0, k;
        this.readAsync(function(b, g) {
          k = g.total;
          for (var d = b.length, c = 0;t + d >= a;) {
            var f = a - t;
            q.set(b.subarray(c, c + f), t);
            c += f;
            d -= f;
            n += a;
            m(q, {loaded:n, total:k});
            t = 0;
          }
          q.set(b.subarray(c), t);
          t += d;
        }, b, l, function() {
          0 < t && (n += t, m(q.subarray(0, t), {loaded:n, total:k}), t = 0);
          r && r();
        }, h);
      }
    };
    a.prototype.readAsync = function(a, m, b, l, r) {
      var h = this.xhr = new XMLHttpRequest({mozSystem:!0}), t = this.url, q = 0, n = 0;
      h.open(this.method || "GET", t, !0);
      h.responseType = "moz-chunked-arraybuffer";
      var k = "moz-chunked-arraybuffer" !== h.responseType;
      k && (h.responseType = "arraybuffer");
      h.onprogress = function(b) {
        k || (q = b.loaded, n = b.total, b = new Uint8Array(h.response), q = Math.max(q, b.byteLength), n = Math.max(n, b.byteLength), a(b, {loaded:q, total:n}));
      };
      h.onreadystatechange = function(b) {
        2 === h.readyState && r && r(t, h.status, h.getAllResponseHeaders());
        4 === h.readyState && (200 !== h.status && 0 !== h.status || null === h.response && (0 === n || q !== n) ? m(h.statusText) : k && (b = h.response, a(new Uint8Array(b), {loaded:b.byteLength, total:b.byteLength})));
      };
      h.onload = function() {
        l && l();
      };
      this.mimeType && h.setRequestHeader("Content-Type", this.mimeType);
      h.send(this.data || null);
      b && b();
    };
    a.prototype.abort = function() {
      this.xhr && (this.xhr.abort(), this.xhr = null);
    };
    return a;
  }();
  k.BinaryFileReader = u;
})(Shumway || (Shumway = {}));
(function(k) {
  var p = function() {
    function k() {
      this.isAS3TraceOn = !0;
      this._startTime = Date.now();
    }
    Object.defineProperty(k.prototype, "currentTimestamp", {get:function() {
      return Date.now() - this._startTime;
    }, enumerable:!0, configurable:!0});
    k.prototype._writeLine = function(a) {
    };
    k.prototype.writeAS3Trace = function(a) {
    };
    return k;
  }();
  k.FlashLog = p;
  k.flashlog = null;
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(a) {
      a[a.Objects = 0] = "Objects";
      a[a.References = 1] = "References";
    })(k.RemotingPhase || (k.RemotingPhase = {}));
    (function(a) {
      a[a.HasMatrix = 1] = "HasMatrix";
      a[a.HasBounds = 2] = "HasBounds";
      a[a.HasChildren = 4] = "HasChildren";
      a[a.HasColorTransform = 8] = "HasColorTransform";
      a[a.HasClipRect = 16] = "HasClipRect";
      a[a.HasMiscellaneousProperties = 32] = "HasMiscellaneousProperties";
      a[a.HasMask = 64] = "HasMask";
      a[a.HasClip = 128] = "HasClip";
    })(k.MessageBits || (k.MessageBits = {}));
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Asset = 134217728] = "Asset";
    })(k.IDMask || (k.IDMask = {}));
    (function(a) {
      a[a.EOF = 0] = "EOF";
      a[a.UpdateFrame = 100] = "UpdateFrame";
      a[a.UpdateGraphics = 101] = "UpdateGraphics";
      a[a.UpdateBitmapData = 102] = "UpdateBitmapData";
      a[a.UpdateTextContent = 103] = "UpdateTextContent";
      a[a.UpdateStage = 104] = "UpdateStage";
      a[a.UpdateNetStream = 105] = "UpdateNetStream";
      a[a.RequestBitmapData = 106] = "RequestBitmapData";
      a[a.DrawToBitmap = 200] = "DrawToBitmap";
      a[a.MouseEvent = 300] = "MouseEvent";
      a[a.KeyboardEvent = 301] = "KeyboardEvent";
      a[a.FocusEvent = 302] = "FocusEvent";
    })(k.MessageTag || (k.MessageTag = {}));
    (function(a) {
      a[a.Blur = 0] = "Blur";
      a[a.DropShadow = 1] = "DropShadow";
      a[a.ColorMatrix = 2] = "ColorMatrix";
    })(k.FilterType || (k.FilterType = {}));
    (function(a) {
      a[a.Identity = 0] = "Identity";
      a[a.AlphaMultiplierOnly = 1] = "AlphaMultiplierOnly";
      a[a.All = 2] = "All";
    })(k.ColorTransformEncoding || (k.ColorTransformEncoding = {}));
    (function(a) {
      a[a.Initialized = 0] = "Initialized";
      a[a.Metadata = 1] = "Metadata";
      a[a.PlayStart = 2] = "PlayStart";
      a[a.PlayStop = 3] = "PlayStop";
      a[a.BufferEmpty = 4] = "BufferEmpty";
      a[a.BufferFull = 5] = "BufferFull";
      a[a.Pause = 6] = "Pause";
      a[a.Unpause = 7] = "Unpause";
      a[a.Seeking = 8] = "Seeking";
      a[a.Seeked = 9] = "Seeked";
      a[a.Progress = 10] = "Progress";
      a[a.Error = 11] = "Error";
    })(k.VideoPlaybackEvent || (k.VideoPlaybackEvent = {}));
    (function(a) {
      a[a.Init = 1] = "Init";
      a[a.Pause = 2] = "Pause";
      a[a.Seek = 3] = "Seek";
      a[a.GetTime = 4] = "GetTime";
      a[a.GetBufferLength = 5] = "GetBufferLength";
      a[a.SetSoundLevels = 6] = "SetSoundLevels";
      a[a.GetBytesLoaded = 7] = "GetBytesLoaded";
      a[a.GetBytesTotal = 8] = "GetBytesTotal";
      a[a.EnsurePlaying = 9] = "EnsurePlaying";
    })(k.VideoControlEvent || (k.VideoControlEvent = {}));
    (function(a) {
      a[a.ShowAll = 0] = "ShowAll";
      a[a.ExactFit = 1] = "ExactFit";
      a[a.NoBorder = 2] = "NoBorder";
      a[a.NoScale = 4] = "NoScale";
    })(k.StageScaleMode || (k.StageScaleMode = {}));
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Top = 1] = "Top";
      a[a.Bottom = 2] = "Bottom";
      a[a.Left = 4] = "Left";
      a[a.Right = 8] = "Right";
      a[a.TopLeft = a.Top | a.Left] = "TopLeft";
      a[a.BottomLeft = a.Bottom | a.Left] = "BottomLeft";
      a[a.BottomRight = a.Bottom | a.Right] = "BottomRight";
      a[a.TopRight = a.Top | a.Right] = "TopRight";
    })(k.StageAlignFlags || (k.StageAlignFlags = {}));
    k.MouseEventNames = "click dblclick mousedown mousemove mouseup mouseover mouseout".split(" ");
    k.KeyboardEventNames = ["keydown", "keypress", "keyup"];
    (function(a) {
      a[a.CtrlKey = 1] = "CtrlKey";
      a[a.AltKey = 2] = "AltKey";
      a[a.ShiftKey = 4] = "ShiftKey";
    })(k.KeyboardEventFlags || (k.KeyboardEventFlags = {}));
    (function(a) {
      a[a.DocumentHidden = 0] = "DocumentHidden";
      a[a.DocumentVisible = 1] = "DocumentVisible";
      a[a.WindowBlur = 2] = "WindowBlur";
      a[a.WindowFocus = 3] = "WindowFocus";
    })(k.FocusEventType || (k.FocusEventType = {}));
    var u = function() {
      function a(a, m) {
        this.window = a;
        this.target = m;
      }
      Object.defineProperty(a.prototype, "onAsyncMessage", {set:function(a) {
        this.window.addEventListener("message", function(m) {
          Promise.resolve(m.data).then(function(b) {
            a(b);
          });
        });
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "onSyncMessage", {set:function(a) {
        this.window.addEventListener("syncmessage", function(m) {
          m = m.detail;
          m.result = a(m.msg);
        });
      }, enumerable:!0, configurable:!0});
      a.prototype.postAsyncMessage = function(a, m) {
        this.target.postMessage(a, "*", m);
      };
      a.prototype.sendSyncMessage = function(a, m) {
        var b = this.target.document.createEvent("CustomEvent"), l = {msg:a, result:void 0};
        b.initCustomEvent("syncmessage", !1, !1, l);
        this.target.dispatchEvent(b);
        return l.result;
      };
      return a;
    }();
    k.WindowTransportPeer = u;
    u = function() {
      function a() {
      }
      Object.defineProperty(a.prototype, "onAsyncMessage", {set:function(a) {
        ShumwayCom.setAsyncMessageCallback(a);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "onSyncMessage", {set:function(a) {
        ShumwayCom.setSyncMessageCallback(a);
      }, enumerable:!0, configurable:!0});
      a.prototype.postAsyncMessage = function(a, m) {
        ShumwayCom.postAsyncMessage(a);
      };
      a.prototype.sendSyncMessage = function(a, m) {
        return ShumwayCom.sendSyncMessage(a);
      };
      return a;
    }();
    k.ShumwayComTransportPeer = u;
  })(k.Remoting || (k.Remoting = {}));
})(Shumway || (Shumway = {}));
var throwError, Errors;
(function(k) {
  (function(k) {
    (function(k) {
      var a = function() {
        function a() {
        }
        a.toRGBA = function(b, a, r, h) {
          void 0 === h && (h = 1);
          return "rgba(" + b + "," + a + "," + r + "," + h + ")";
        };
        return a;
      }();
      k.UI = a;
      var p = function() {
        function m() {
        }
        m.prototype.tabToolbar = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(37, 44, 51, b);
        };
        m.prototype.toolbars = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(52, 60, 69, b);
        };
        m.prototype.selectionBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(29, 79, 115, b);
        };
        m.prototype.selectionText = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(245, 247, 250, b);
        };
        m.prototype.splitters = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(0, 0, 0, b);
        };
        m.prototype.bodyBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(17, 19, 21, b);
        };
        m.prototype.sidebarBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(24, 29, 32, b);
        };
        m.prototype.attentionBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(161, 134, 80, b);
        };
        m.prototype.bodyText = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(143, 161, 178, b);
        };
        m.prototype.foregroundTextGrey = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(182, 186, 191, b);
        };
        m.prototype.contentTextHighContrast = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(169, 186, 203, b);
        };
        m.prototype.contentTextGrey = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(143, 161, 178, b);
        };
        m.prototype.contentTextDarkGrey = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(95, 115, 135, b);
        };
        m.prototype.blueHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(70, 175, 227, b);
        };
        m.prototype.purpleHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(107, 122, 187, b);
        };
        m.prototype.pinkHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(223, 128, 255, b);
        };
        m.prototype.redHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(235, 83, 104, b);
        };
        m.prototype.orangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(217, 102, 41, b);
        };
        m.prototype.lightOrangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(217, 155, 40, b);
        };
        m.prototype.greenHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(112, 191, 83, b);
        };
        m.prototype.blueGreyHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(94, 136, 176, b);
        };
        return m;
      }();
      k.UIThemeDark = p;
      p = function() {
        function m() {
        }
        m.prototype.tabToolbar = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(235, 236, 237, b);
        };
        m.prototype.toolbars = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(240, 241, 242, b);
        };
        m.prototype.selectionBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(76, 158, 217, b);
        };
        m.prototype.selectionText = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(245, 247, 250, b);
        };
        m.prototype.splitters = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(170, 170, 170, b);
        };
        m.prototype.bodyBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(252, 252, 252, b);
        };
        m.prototype.sidebarBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(247, 247, 247, b);
        };
        m.prototype.attentionBackground = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(161, 134, 80, b);
        };
        m.prototype.bodyText = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(24, 25, 26, b);
        };
        m.prototype.foregroundTextGrey = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(88, 89, 89, b);
        };
        m.prototype.contentTextHighContrast = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(41, 46, 51, b);
        };
        m.prototype.contentTextGrey = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(143, 161, 178, b);
        };
        m.prototype.contentTextDarkGrey = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(102, 115, 128, b);
        };
        m.prototype.blueHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(0, 136, 204, b);
        };
        m.prototype.purpleHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(91, 95, 255, b);
        };
        m.prototype.pinkHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(184, 46, 229, b);
        };
        m.prototype.redHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(237, 38, 85, b);
        };
        m.prototype.orangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(241, 60, 0, b);
        };
        m.prototype.lightOrangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(217, 126, 0, b);
        };
        m.prototype.greenHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(44, 187, 15, b);
        };
        m.prototype.blueGreyHighlight = function(b) {
          void 0 === b && (b = 1);
          return a.toRGBA(95, 136, 176, b);
        };
        return m;
      }();
      k.UIThemeLight = p;
    })(k.Theme || (k.Theme = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(k) {
      var a = function() {
        function a(m, b) {
          this._buffers = m || [];
          this._snapshots = [];
          this._windowStart = this._startTime = b;
          this._maxDepth = 0;
        }
        a.prototype.addBuffer = function(a) {
          this._buffers.push(a);
        };
        a.prototype.getSnapshotAt = function(a) {
          return this._snapshots[a];
        };
        Object.defineProperty(a.prototype, "hasSnapshots", {get:function() {
          return 0 < this.snapshotCount;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "snapshotCount", {get:function() {
          return this._snapshots.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "startTime", {get:function() {
          return this._startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "endTime", {get:function() {
          return this._endTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "windowStart", {get:function() {
          return this._windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "windowEnd", {get:function() {
          return this._windowEnd;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "windowLength", {get:function() {
          return this.windowEnd - this.windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "maxDepth", {get:function() {
          return this._maxDepth;
        }, enumerable:!0, configurable:!0});
        a.prototype.forEachSnapshot = function(a) {
          for (var b = 0, l = this.snapshotCount;b < l;b++) {
            a(this._snapshots[b], b);
          }
        };
        a.prototype.createSnapshots = function() {
          var a = Number.MIN_VALUE, b = 0;
          for (this._snapshots = [];0 < this._buffers.length;) {
            var l = this._buffers.shift().createSnapshot();
            l && (a < l.endTime && (a = l.endTime), b < l.maxDepth && (b = l.maxDepth), this._snapshots.push(l));
          }
          this._windowEnd = this._endTime = a;
          this._maxDepth = b;
        };
        a.prototype.setWindow = function(a, b) {
          if (a > b) {
            var l = a;
            a = b;
            b = l;
          }
          l = Math.min(b - a, this.totalTime);
          a < this._startTime ? (a = this._startTime, b = this._startTime + l) : b > this._endTime && (a = this._endTime - l, b = this._endTime);
          this._windowStart = a;
          this._windowEnd = b;
        };
        a.prototype.moveWindowTo = function(a) {
          this.setWindow(a - this.windowLength / 2, a + this.windowLength / 2);
        };
        return a;
      }();
      k.Profile = a;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, p) {
  function u() {
    this.constructor = k;
  }
  for (var a in p) {
    p.hasOwnProperty(a) && (k[a] = p[a]);
  }
  u.prototype = p.prototype;
  k.prototype = new u;
};
(function(k) {
  (function(k) {
    (function(k) {
      var a = function() {
        return function(a) {
          this.kind = a;
          this.totalTime = this.selfTime = this.count = 0;
        };
      }();
      k.TimelineFrameStatistics = a;
      var p = function() {
        function k(b, a, r, h, t, q) {
          this.parent = b;
          this.kind = a;
          this.startData = r;
          this.endData = h;
          this.startTime = t;
          this.endTime = q;
          this.maxDepth = 0;
        }
        Object.defineProperty(k.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(k.prototype, "selfTime", {get:function() {
          var b = this.totalTime;
          if (this.children) {
            for (var a = 0, r = this.children.length;a < r;a++) {
              var h = this.children[a], b = b - (h.endTime - h.startTime)
            }
          }
          return b;
        }, enumerable:!0, configurable:!0});
        k.prototype.getChildIndex = function(b) {
          for (var a = this.children, r = 0;r < a.length;r++) {
            if (a[r].endTime > b) {
              return r;
            }
          }
          return 0;
        };
        k.prototype.getChildRange = function(b, a) {
          if (this.children && b <= this.endTime && a >= this.startTime && a >= b) {
            var r = this._getNearestChild(b), h = this._getNearestChildReverse(a);
            if (r <= h) {
              return b = this.children[r].startTime, a = this.children[h].endTime, {startIndex:r, endIndex:h, startTime:b, endTime:a, totalTime:a - b};
            }
          }
          return null;
        };
        k.prototype._getNearestChild = function(b) {
          var a = this.children;
          if (a && a.length) {
            if (b <= a[0].endTime) {
              return 0;
            }
            for (var r, h = 0, t = a.length - 1;t > h;) {
              r = (h + t) / 2 | 0;
              var q = a[r];
              if (b >= q.startTime && b <= q.endTime) {
                return r;
              }
              b > q.endTime ? h = r + 1 : t = r;
            }
            return Math.ceil((h + t) / 2);
          }
          return 0;
        };
        k.prototype._getNearestChildReverse = function(b) {
          var a = this.children;
          if (a && a.length) {
            var r = a.length - 1;
            if (b >= a[r].startTime) {
              return r;
            }
            for (var h, t = 0;r > t;) {
              h = Math.ceil((t + r) / 2);
              var q = a[h];
              if (b >= q.startTime && b <= q.endTime) {
                return h;
              }
              b > q.endTime ? t = h : r = h - 1;
            }
            return (t + r) / 2 | 0;
          }
          return 0;
        };
        k.prototype.query = function(b) {
          if (b < this.startTime || b > this.endTime) {
            return null;
          }
          var a = this.children;
          if (a && 0 < a.length) {
            for (var r, h = 0, t = a.length - 1;t > h;) {
              var q = (h + t) / 2 | 0;
              r = a[q];
              if (b >= r.startTime && b <= r.endTime) {
                return r.query(b);
              }
              b > r.endTime ? h = q + 1 : t = q;
            }
            r = a[t];
            if (b >= r.startTime && b <= r.endTime) {
              return r.query(b);
            }
          }
          return this;
        };
        k.prototype.queryNext = function(b) {
          for (var a = this;b > a.endTime;) {
            if (a.parent) {
              a = a.parent;
            } else {
              break;
            }
          }
          return a.query(b);
        };
        k.prototype.getDepth = function() {
          for (var b = 0, a = this;a;) {
            b++, a = a.parent;
          }
          return b;
        };
        k.prototype.calculateStatistics = function() {
          function b(r) {
            if (r.kind) {
              var h = l[r.kind.id] || (l[r.kind.id] = new a(r.kind));
              h.count++;
              h.selfTime += r.selfTime;
              h.totalTime += r.totalTime;
            }
            r.children && r.children.forEach(b);
          }
          var l = this.statistics = [];
          b(this);
        };
        k.prototype.trace = function(b) {
          var a = (this.kind ? this.kind.name + ": " : "Profile: ") + (this.endTime - this.startTime).toFixed(2);
          if (this.children && this.children.length) {
            b.enter(a);
            for (a = 0;a < this.children.length;a++) {
              this.children[a].trace(b);
            }
            b.outdent();
          } else {
            b.writeLn(a);
          }
        };
        return k;
      }();
      k.TimelineFrame = p;
      p = function(a) {
        function b(b) {
          a.call(this, null, null, null, null, NaN, NaN);
          this.name = b;
        }
        __extends(b, a);
        return b;
      }(p);
      k.TimelineBufferSnapshot = p;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = function() {
        function a(m, b) {
          void 0 === m && (m = "");
          this.name = m || "";
          this._startTime = k.isNullOrUndefined(b) ? jsGlobal.START_TIME : b;
        }
        a.prototype.getKind = function(a) {
          return this._kinds[a];
        };
        Object.defineProperty(a.prototype, "kinds", {get:function() {
          return this._kinds.concat();
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "depth", {get:function() {
          return this._depth;
        }, enumerable:!0, configurable:!0});
        a.prototype._initialize = function() {
          this._depth = 0;
          this._stack = [];
          this._data = [];
          this._kinds = [];
          this._kindNameMap = Object.create(null);
          this._marks = new k.CircularBuffer(Int32Array, 20);
          this._times = new k.CircularBuffer(Float64Array, 20);
        };
        a.prototype._getKindId = function(k) {
          var b = a.MAX_KINDID;
          if (void 0 === this._kindNameMap[k]) {
            if (b = this._kinds.length, b < a.MAX_KINDID) {
              var l = {id:b, name:k, visible:!0};
              this._kinds.push(l);
              this._kindNameMap[k] = l;
            } else {
              b = a.MAX_KINDID;
            }
          } else {
            b = this._kindNameMap[k].id;
          }
          return b;
        };
        a.prototype._getMark = function(m, b, l) {
          var r = a.MAX_DATAID;
          k.isNullOrUndefined(l) || b === a.MAX_KINDID || (r = this._data.length, r < a.MAX_DATAID ? this._data.push(l) : r = a.MAX_DATAID);
          return m | r << 16 | b;
        };
        a.prototype.enter = function(m, b, l) {
          l = (k.isNullOrUndefined(l) ? performance.now() : l) - this._startTime;
          this._marks || this._initialize();
          this._depth++;
          m = this._getKindId(m);
          this._marks.write(this._getMark(a.ENTER, m, b));
          this._times.write(l);
          this._stack.push(m);
        };
        a.prototype.leave = function(m, b, l) {
          l = (k.isNullOrUndefined(l) ? performance.now() : l) - this._startTime;
          var r = this._stack.pop();
          m && (r = this._getKindId(m));
          this._marks.write(this._getMark(a.LEAVE, r, b));
          this._times.write(l);
          this._depth--;
        };
        a.prototype.count = function(a, b, l) {
        };
        a.prototype.createSnapshot = function(m) {
          void 0 === m && (m = Number.MAX_VALUE);
          if (!this._marks) {
            return null;
          }
          var b = this._times, l = this._kinds, r = this._data, h = new u.TimelineBufferSnapshot(this.name), t = [h], q = 0;
          this._marks || this._initialize();
          this._marks.forEachInReverse(function(h, v) {
            var e = r[h >>> 16 & a.MAX_DATAID], g = l[h & a.MAX_KINDID];
            if (k.isNullOrUndefined(g) || g.visible) {
              var d = h & 2147483648, c = b.get(v), f = t.length;
              if (d === a.LEAVE) {
                if (1 === f && (q++, q > m)) {
                  return !0;
                }
                t.push(new u.TimelineFrame(t[f - 1], g, null, e, NaN, c));
              } else {
                if (d === a.ENTER) {
                  if (g = t.pop(), d = t[t.length - 1]) {
                    for (d.children ? d.children.unshift(g) : d.children = [g], d = t.length, g.depth = d, g.startData = e, g.startTime = c;g;) {
                      if (g.maxDepth < d) {
                        g.maxDepth = d, g = g.parent;
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
          h.children && h.children.length && (h.startTime = h.children[0].startTime, h.endTime = h.children[h.children.length - 1].endTime);
          return h;
        };
        a.prototype.reset = function(a) {
          this._startTime = k.isNullOrUndefined(a) ? performance.now() : a;
          this._marks ? (this._depth = 0, this._data = [], this._marks.reset(), this._times.reset()) : this._initialize();
        };
        a.FromFirefoxProfile = function(k, b) {
          for (var l = k.profile.threads[0].samples, r = new a(b, l[0].time), h = [], t, q = 0;q < l.length;q++) {
            t = l[q];
            var n = t.time, v = t.frames, e = 0;
            for (t = Math.min(v.length, h.length);e < t && v[e].location === h[e].location;) {
              e++;
            }
            for (var g = h.length - e, d = 0;d < g;d++) {
              t = h.pop(), r.leave(t.location, null, n);
            }
            for (;e < v.length;) {
              t = v[e++], r.enter(t.location, null, n);
            }
            h = v;
          }
          for (;t = h.pop();) {
            r.leave(t.location, null, n);
          }
          return r;
        };
        a.FromChromeProfile = function(k, b) {
          var l = k.timestamps, r = k.samples, h = new a(b, l[0] / 1E3), t = [], q = {}, n;
          a._resolveIds(k.head, q);
          for (var v = 0;v < l.length;v++) {
            var e = l[v] / 1E3, g = [];
            for (n = q[r[v]];n;) {
              g.unshift(n), n = n.parent;
            }
            var d = 0;
            for (n = Math.min(g.length, t.length);d < n && g[d] === t[d];) {
              d++;
            }
            for (var c = t.length - d, f = 0;f < c;f++) {
              n = t.pop(), h.leave(n.functionName, null, e);
            }
            for (;d < g.length;) {
              n = g[d++], h.enter(n.functionName, null, e);
            }
            t = g;
          }
          for (;n = t.pop();) {
            h.leave(n.functionName, null, e);
          }
          return h;
        };
        a._resolveIds = function(k, b) {
          b[k.id] = k;
          if (k.children) {
            for (var l = 0;l < k.children.length;l++) {
              k.children[l].parent = k, a._resolveIds(k.children[l], b);
            }
          }
        };
        a.ENTER = 0;
        a.LEAVE = -2147483648;
        a.MAX_KINDID = 65535;
        a.MAX_DATAID = 32767;
        return a;
      }();
      u.TimelineBuffer = a;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      (function(a) {
        a[a.DARK = 0] = "DARK";
        a[a.LIGHT = 1] = "LIGHT";
      })(u.UIThemeType || (u.UIThemeType = {}));
      var a = function() {
        function a(k, b) {
          void 0 === b && (b = 0);
          this._container = k;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = b;
          this._tooltip = this._createTooltip();
        }
        a.prototype.createProfile = function(a, b) {
          var l = new u.Profile(a, b);
          l.createSnapshots();
          this._profiles.push(l);
          this.activateProfile(l);
          return l;
        };
        a.prototype.activateProfile = function(a) {
          this.deactivateProfile();
          this._activeProfile = a;
          this._createViews();
          this._initializeViews();
        };
        a.prototype.activateProfileAt = function(a) {
          this.activateProfile(this.getProfileAt(a));
        };
        a.prototype.deactivateProfile = function() {
          this._activeProfile && (this._destroyViews(), this._activeProfile = null);
        };
        a.prototype.resize = function() {
          this._onResize();
        };
        a.prototype.getProfileAt = function(a) {
          return this._profiles[a];
        };
        Object.defineProperty(a.prototype, "activeProfile", {get:function() {
          return this._activeProfile;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "profileCount", {get:function() {
          return this._profiles.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "container", {get:function() {
          return this._container;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "themeType", {get:function() {
          return this._themeType;
        }, set:function(a) {
          switch(a) {
            case 0:
              this._theme = new p.Theme.UIThemeDark;
              break;
            case 1:
              this._theme = new p.Theme.UIThemeLight;
          }
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "theme", {get:function() {
          return this._theme;
        }, enumerable:!0, configurable:!0});
        a.prototype.getSnapshotAt = function(a) {
          return this._activeProfile.getSnapshotAt(a);
        };
        a.prototype._createViews = function() {
          if (this._activeProfile) {
            var a = this;
            this._overviewHeader = new u.FlameChartHeader(this, 0);
            this._overview = new u.FlameChartOverview(this, 0);
            this._activeProfile.forEachSnapshot(function(b, l) {
              a._headers.push(new u.FlameChartHeader(a, 1));
              a._charts.push(new u.FlameChart(a, b));
            });
            window.addEventListener("resize", this._onResize.bind(this));
          }
        };
        a.prototype._destroyViews = function() {
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
        a.prototype._initializeViews = function() {
          if (this._activeProfile) {
            var a = this, b = this._activeProfile.startTime, l = this._activeProfile.endTime;
            this._overviewHeader.initialize(b, l);
            this._overview.initialize(b, l);
            this._activeProfile.forEachSnapshot(function(r, h) {
              a._headers[h].initialize(b, l);
              a._charts[h].initialize(b, l);
            });
          }
        };
        a.prototype._onResize = function() {
          if (this._activeProfile) {
            var a = this, b = this._container.offsetWidth;
            this._overviewHeader.setSize(b);
            this._overview.setSize(b);
            this._activeProfile.forEachSnapshot(function(l, r) {
              a._headers[r].setSize(b);
              a._charts[r].setSize(b);
            });
          }
        };
        a.prototype._updateViews = function() {
          if (this._activeProfile) {
            var a = this, b = this._activeProfile.windowStart, l = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(b, l);
            this._overview.setWindow(b, l);
            this._activeProfile.forEachSnapshot(function(r, h) {
              a._headers[h].setWindow(b, l);
              a._charts[h].setWindow(b, l);
            });
          }
        };
        a.prototype._drawViews = function() {
        };
        a.prototype._createTooltip = function() {
          var a = document.createElement("div");
          a.classList.add("profiler-tooltip");
          a.style.display = "none";
          this._container.insertBefore(a, this._container.firstChild);
          return a;
        };
        a.prototype.setWindow = function(a, b) {
          this._activeProfile.setWindow(a, b);
          this._updateViews();
        };
        a.prototype.moveWindowTo = function(a) {
          this._activeProfile.moveWindowTo(a);
          this._updateViews();
        };
        a.prototype.showTooltip = function(a, b, l, r) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(a, b));
          this._tooltip.style.display = "block";
          var h = this._tooltip.firstChild;
          b = h.clientWidth;
          h = h.clientHeight;
          l += l + b >= a.canvas.clientWidth - 50 ? -(b + 20) : 25;
          r += a.canvas.offsetTop - h / 2;
          this._tooltip.style.left = l + "px";
          this._tooltip.style.top = r + "px";
        };
        a.prototype.hideTooltip = function() {
          this._tooltip.style.display = "none";
        };
        a.prototype.createTooltipContent = function(a, b) {
          var l = Math.round(1E5 * b.totalTime) / 1E5, r = Math.round(1E5 * b.selfTime) / 1E5, h = Math.round(1E4 * b.selfTime / b.totalTime) / 100, t = document.createElement("div"), k = document.createElement("h1");
          k.textContent = b.kind.name;
          t.appendChild(k);
          k = document.createElement("p");
          k.textContent = "Total: " + l + " ms";
          t.appendChild(k);
          l = document.createElement("p");
          l.textContent = "Self: " + r + " ms (" + h + "%)";
          t.appendChild(l);
          if (r = a.getStatistics(b.kind)) {
            h = document.createElement("p"), h.textContent = "Count: " + r.count, t.appendChild(h), h = Math.round(1E5 * r.totalTime) / 1E5, l = document.createElement("p"), l.textContent = "All Total: " + h + " ms", t.appendChild(l), r = Math.round(1E5 * r.selfTime) / 1E5, h = document.createElement("p"), h.textContent = "All Self: " + r + " ms", t.appendChild(h);
          }
          this.appendDataElements(t, b.startData);
          this.appendDataElements(t, b.endData);
          return t;
        };
        a.prototype.appendDataElements = function(a, b) {
          if (!k.isNullOrUndefined(b)) {
            a.appendChild(document.createElement("hr"));
            var l;
            if (k.isObject(b)) {
              for (var r in b) {
                l = document.createElement("p"), l.textContent = r + ": " + b[r], a.appendChild(l);
              }
            } else {
              l = document.createElement("p"), l.textContent = b.toString(), a.appendChild(l);
            }
          }
        };
        a.prototype.removeTooltipContent = function() {
          for (var a = this._tooltip;a.firstChild;) {
            a.removeChild(a.firstChild);
          }
        };
        return a;
      }();
      u.Controller = a;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.NumberUtilities.clamp, p = function() {
        function a(b) {
          this.value = b;
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
      u.MouseCursor = p;
      var m = function() {
        function b(a, b) {
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
        b.prototype.destroy = function() {
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
        b.prototype.updateCursor = function(a) {
          if (!b._cursorOwner || b._cursorOwner === this._target) {
            var r = this._eventTarget.parentElement;
            b._cursor !== a && (b._cursor = a, ["", "-moz-", "-webkit-"].forEach(function(b) {
              r.style.cursor = b + a;
            }));
            b._cursorOwner = b._cursor === p.DEFAULT ? null : this._target;
          }
        };
        b.prototype._onMouseDown = function(a) {
          this._killHoverCheck();
          if (0 === a.button) {
            var b = this._getTargetMousePos(a, a.target);
            this._dragInfo = {start:b, current:b, delta:{x:0, y:0}, hasMoved:!1, originalTarget:a.target};
            window.addEventListener("mousemove", this._boundOnDrag, !1);
            window.addEventListener("mouseup", this._boundOnMouseUp, !1);
            this._target.onMouseDown(b.x, b.y);
          }
        };
        b.prototype._onDrag = function(a) {
          var b = this._dragInfo;
          a = this._getTargetMousePos(a, b.originalTarget);
          var h = {x:a.x - b.start.x, y:a.y - b.start.y};
          b.current = a;
          b.delta = h;
          b.hasMoved = !0;
          this._target.onDrag(b.start.x, b.start.y, a.x, a.y, h.x, h.y);
        };
        b.prototype._onMouseUp = function(a) {
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
        b.prototype._onMouseOver = function(a) {
          a.target.addEventListener("mousemove", this._boundOnMouseMove, !1);
          if (!this._dragInfo) {
            var b = this._getTargetMousePos(a, a.target);
            this._target.onMouseOver(b.x, b.y);
            this._startHoverCheck(a);
          }
        };
        b.prototype._onMouseOut = function(a) {
          a.target.removeEventListener("mousemove", this._boundOnMouseMove, !1);
          if (!this._dragInfo) {
            this._target.onMouseOut();
          }
          this._killHoverCheck();
        };
        b.prototype._onMouseMove = function(a) {
          if (!this._dragInfo) {
            var b = this._getTargetMousePos(a, a.target);
            this._target.onMouseMove(b.x, b.y);
            this._killHoverCheck();
            this._startHoverCheck(a);
          }
        };
        b.prototype._onMouseWheel = function(b) {
          if (!(b.altKey || b.metaKey || b.ctrlKey || b.shiftKey || (b.preventDefault(), this._dragInfo || this._wheelDisabled))) {
            var r = this._getTargetMousePos(b, b.target);
            b = a("undefined" !== typeof b.deltaY ? b.deltaY / 16 : -b.wheelDelta / 40, -1, 1);
            b = Math.pow(1.2, b) - 1;
            this._target.onMouseWheel(r.x, r.y, b);
          }
        };
        b.prototype._startHoverCheck = function(a) {
          this._hoverInfo = {isHovering:!1, timeoutHandle:setTimeout(this._onMouseMoveIdleHandler.bind(this), b.HOVER_TIMEOUT), pos:this._getTargetMousePos(a, a.target)};
        };
        b.prototype._killHoverCheck = function() {
          if (this._hoverInfo) {
            clearTimeout(this._hoverInfo.timeoutHandle);
            if (this._hoverInfo.isHovering) {
              this._target.onHoverEnd();
            }
            this._hoverInfo = null;
          }
        };
        b.prototype._onMouseMoveIdleHandler = function() {
          var a = this._hoverInfo;
          a.isHovering = !0;
          this._target.onHoverStart(a.pos.x, a.pos.y);
        };
        b.prototype._getTargetMousePos = function(a, b) {
          var h = b.getBoundingClientRect();
          return {x:a.clientX - h.left, y:a.clientY - h.top};
        };
        b.HOVER_TIMEOUT = 500;
        b._cursor = p.DEFAULT;
        return b;
      }();
      u.MouseController = m;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(k) {
      (function(a) {
        a[a.NONE = 0] = "NONE";
        a[a.WINDOW = 1] = "WINDOW";
        a[a.HANDLE_LEFT = 2] = "HANDLE_LEFT";
        a[a.HANDLE_RIGHT = 3] = "HANDLE_RIGHT";
        a[a.HANDLE_BOTH = 4] = "HANDLE_BOTH";
      })(k.FlameChartDragTarget || (k.FlameChartDragTarget = {}));
      var a = function() {
        function a(m) {
          this._controller = m;
          this._initialized = !1;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
          this._mouseController = new k.MouseController(this, this._canvas);
          m = m.container;
          m.appendChild(this._canvas);
          m = m.getBoundingClientRect();
          this.setSize(m.width);
        }
        Object.defineProperty(a.prototype, "canvas", {get:function() {
          return this._canvas;
        }, enumerable:!0, configurable:!0});
        a.prototype.setSize = function(a, b) {
          void 0 === b && (b = 20);
          this._width = a;
          this._height = b;
          this._resetCanvas();
          this.draw();
        };
        a.prototype.initialize = function(a, b) {
          this._initialized = !0;
          this.setRange(a, b, !1);
          this.setWindow(a, b, !1);
          this.draw();
        };
        a.prototype.setWindow = function(a, b, l) {
          void 0 === l && (l = !0);
          this._windowStart = a;
          this._windowEnd = b;
          !l || this.draw();
        };
        a.prototype.setRange = function(a, b, l) {
          void 0 === l && (l = !0);
          this._rangeStart = a;
          this._rangeEnd = b;
          !l || this.draw();
        };
        a.prototype.destroy = function() {
          this._mouseController.destroy();
          this._mouseController = null;
          this._controller.container.removeChild(this._canvas);
          this._controller = null;
        };
        a.prototype._resetCanvas = function() {
          var a = window.devicePixelRatio, b = this._canvas;
          b.width = this._width * a;
          b.height = this._height * a;
          b.style.width = this._width + "px";
          b.style.height = this._height + "px";
        };
        a.prototype.draw = function() {
        };
        a.prototype._almostEq = function(a, b, l) {
          void 0 === l && (l = 10);
          l = Math.pow(10, l);
          return Math.abs(a - b) < 1 / l;
        };
        a.prototype._windowEqRange = function() {
          return this._almostEq(this._windowStart, this._rangeStart) && this._almostEq(this._windowEnd, this._rangeEnd);
        };
        a.prototype._decimalPlaces = function(a) {
          return (+a).toFixed(10).replace(/^-?\d*\.?|0+$/g, "").length;
        };
        a.prototype._toPixelsRelative = function(a) {
          return 0;
        };
        a.prototype._toPixels = function(a) {
          return 0;
        };
        a.prototype._toTimeRelative = function(a) {
          return 0;
        };
        a.prototype._toTime = function(a) {
          return 0;
        };
        a.prototype.onMouseWheel = function(k, b, l) {
          k = this._toTime(k);
          b = this._windowStart;
          var r = this._windowEnd, h = r - b;
          l = Math.max((a.MIN_WINDOW_LEN - h) / h, l);
          this._controller.setWindow(b + (b - k) * l, r + (r - k) * l);
          this.onHoverEnd();
        };
        a.prototype.onMouseDown = function(a, b) {
        };
        a.prototype.onMouseMove = function(a, b) {
        };
        a.prototype.onMouseOver = function(a, b) {
        };
        a.prototype.onMouseOut = function() {
        };
        a.prototype.onDrag = function(a, b, l, k, h, t) {
        };
        a.prototype.onDragEnd = function(a, b, l, k, h, t) {
        };
        a.prototype.onClick = function(a, b) {
        };
        a.prototype.onHoverStart = function(a, b) {
        };
        a.prototype.onHoverEnd = function() {
        };
        a.DRAGHANDLE_WIDTH = 4;
        a.MIN_WINDOW_LEN = .1;
        return a;
      }();
      k.FlameChartBase = a;
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.StringUtilities.trimMiddle, p = function(m) {
        function b(a, b) {
          m.call(this, a);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = b;
          this._kindStyle = Object.create(null);
        }
        __extends(b, m);
        b.prototype.setSize = function(a, b) {
          m.prototype.setSize.call(this, a, b || this._initialized ? 12.5 * this._maxDepth : 100);
        };
        b.prototype.initialize = function(a, b) {
          this._initialized = !0;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(a, b, !1);
          this.setWindow(a, b, !1);
          this.setSize(this._width, 12.5 * this._maxDepth);
        };
        b.prototype.destroy = function() {
          m.prototype.destroy.call(this);
          this._snapshot = null;
        };
        b.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio;
          k.ColorStyle.reset();
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, this._width, this._height);
          this._initialized && this._drawChildren(this._snapshot);
          a.restore();
        };
        b.prototype._drawChildren = function(a, b) {
          void 0 === b && (b = 0);
          var h = a.getChildRange(this._windowStart, this._windowEnd);
          if (h) {
            for (var t = h.startIndex;t <= h.endIndex;t++) {
              var k = a.children[t];
              this._drawFrame(k, b) && this._drawChildren(k, b + 1);
            }
          }
        };
        b.prototype._drawFrame = function(a, b) {
          var h = this._context, t = this._toPixels(a.startTime), q = this._toPixels(a.endTime), n = q - t;
          if (n <= this._minFrameWidthInPixels) {
            return h.fillStyle = this._controller.theme.tabToolbar(1), h.fillRect(t, 12.5 * b, this._minFrameWidthInPixels, 12 + 12.5 * (a.maxDepth - a.depth)), !1;
          }
          0 > t && (q = n + t, t = 0);
          var q = q - t, v = this._kindStyle[a.kind.id];
          v || (v = k.ColorStyle.randomStyle(), v = this._kindStyle[a.kind.id] = {bgColor:v, textColor:k.ColorStyle.contrastStyle(v)});
          h.save();
          h.fillStyle = v.bgColor;
          h.fillRect(t, 12.5 * b, q, 12);
          12 < n && (n = a.kind.name) && n.length && (n = this._prepareText(h, n, q - 4), n.length && (h.fillStyle = v.textColor, h.textBaseline = "bottom", h.fillText(n, t + 2, 12.5 * (b + 1) - 1)));
          h.restore();
          return !0;
        };
        b.prototype._prepareText = function(b, k, h) {
          var t = this._measureWidth(b, k);
          if (h > t) {
            return k;
          }
          for (var t = 3, q = k.length;t < q;) {
            var n = t + q >> 1;
            this._measureWidth(b, a(k, n)) < h ? t = n + 1 : q = n;
          }
          k = a(k, q - 1);
          t = this._measureWidth(b, k);
          return t <= h ? k : "";
        };
        b.prototype._measureWidth = function(a, b) {
          var h = this._textWidth[b];
          h || (h = a.measureText(b).width, this._textWidth[b] = h);
          return h;
        };
        b.prototype._toPixelsRelative = function(a) {
          return a * this._width / (this._windowEnd - this._windowStart);
        };
        b.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - this._windowStart);
        };
        b.prototype._toTimeRelative = function(a) {
          return a * (this._windowEnd - this._windowStart) / this._width;
        };
        b.prototype._toTime = function(a) {
          return this._toTimeRelative(a) + this._windowStart;
        };
        b.prototype._getFrameAtPosition = function(a, b) {
          var h = this._toTime(a), k = 1 + b / 12.5 | 0;
          if ((h = this._snapshot.query(h)) && h.depth >= k) {
            for (;h && h.depth > k;) {
              h = h.parent;
            }
            return h;
          }
          return null;
        };
        b.prototype.onMouseDown = function(a, b) {
          this._windowEqRange() || (this._mouseController.updateCursor(u.MouseCursor.ALL_SCROLL), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:1});
        };
        b.prototype.onMouseMove = function(a, b) {
        };
        b.prototype.onMouseOver = function(a, b) {
        };
        b.prototype.onMouseOut = function() {
        };
        b.prototype.onDrag = function(a, b, h, k, q, n) {
          if (a = this._dragInfo) {
            q = this._toTimeRelative(-q), this._controller.setWindow(a.windowStartInitial + q, a.windowEndInitial + q);
          }
        };
        b.prototype.onDragEnd = function(a, b, h, k, q, n) {
          this._dragInfo = null;
          this._mouseController.updateCursor(u.MouseCursor.DEFAULT);
        };
        b.prototype.onClick = function(a, b) {
          this._dragInfo = null;
          this._mouseController.updateCursor(u.MouseCursor.DEFAULT);
        };
        b.prototype.onHoverStart = function(a, b) {
          var h = this._getFrameAtPosition(a, b);
          h && (this._hoveredFrame = h, this._controller.showTooltip(this, h, a, b));
        };
        b.prototype.onHoverEnd = function() {
          this._hoveredFrame && (this._hoveredFrame = null, this._controller.hideTooltip());
        };
        b.prototype.getStatistics = function(a) {
          var b = this._snapshot;
          b.statistics || b.calculateStatistics();
          return b.statistics[a.id];
        };
        return b;
      }(u.FlameChartBase);
      u.FlameChart = p;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.NumberUtilities.clamp;
      (function(a) {
        a[a.OVERLAY = 0] = "OVERLAY";
        a[a.STACK = 1] = "STACK";
        a[a.UNION = 2] = "UNION";
      })(u.FlameChartOverviewMode || (u.FlameChartOverviewMode = {}));
      var p = function(k) {
        function b(a, b) {
          void 0 === b && (b = 1);
          this._mode = b;
          this._overviewCanvasDirty = !0;
          this._overviewCanvas = document.createElement("canvas");
          this._overviewContext = this._overviewCanvas.getContext("2d");
          k.call(this, a);
        }
        __extends(b, k);
        b.prototype.setSize = function(a, b) {
          k.prototype.setSize.call(this, a, b || 64);
        };
        Object.defineProperty(b.prototype, "mode", {set:function(a) {
          this._mode = a;
          this.draw();
        }, enumerable:!0, configurable:!0});
        b.prototype._resetCanvas = function() {
          k.prototype._resetCanvas.call(this);
          this._overviewCanvas.width = this._canvas.width;
          this._overviewCanvas.height = this._canvas.height;
          this._overviewCanvasDirty = !0;
        };
        b.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio, h = this._width, k = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, h, k);
          a.restore();
          this._initialized && (this._overviewCanvasDirty && (this._drawChart(), this._overviewCanvasDirty = !1), a.drawImage(this._overviewCanvas, 0, 0), this._drawSelection());
        };
        b.prototype._drawSelection = function() {
          var a = this._context, b = this._height, h = window.devicePixelRatio, k = this._selection ? this._selection.left : this._toPixels(this._windowStart), q = this._selection ? this._selection.right : this._toPixels(this._windowEnd), n = this._controller.theme;
          a.save();
          a.scale(h, h);
          this._selection ? (a.fillStyle = n.selectionText(.15), a.fillRect(k, 1, q - k, b - 1), a.fillStyle = "rgba(133, 0, 0, 1)", a.fillRect(k + .5, 0, q - k - 1, 4), a.fillRect(k + .5, b - 4, q - k - 1, 4)) : (a.fillStyle = n.bodyBackground(.4), a.fillRect(0, 1, k, b - 1), a.fillRect(q, 1, this._width, b - 1));
          a.beginPath();
          a.moveTo(k, 0);
          a.lineTo(k, b);
          a.moveTo(q, 0);
          a.lineTo(q, b);
          a.lineWidth = .5;
          a.strokeStyle = n.foregroundTextGrey(1);
          a.stroke();
          b = this._selection ? this._toTime(this._selection.left) : this._windowStart;
          h = this._selection ? this._toTime(this._selection.right) : this._windowEnd;
          b = Math.abs(h - b);
          a.fillStyle = n.selectionText(.5);
          a.font = "8px sans-serif";
          a.textBaseline = "alphabetic";
          a.textAlign = "end";
          a.fillText(b.toFixed(2), Math.min(k, q) - 4, 10);
          a.fillText((b / 60).toFixed(2), Math.min(k, q) - 4, 20);
          a.restore();
        };
        b.prototype._drawChart = function() {
          var a = window.devicePixelRatio, b = this._height, h = this._controller.activeProfile, k = 4 * this._width, q = h.totalTime / k, n = this._overviewContext, v = this._controller.theme.blueHighlight(1);
          n.save();
          n.translate(0, a * b);
          var e = -a * b / (h.maxDepth - 1);
          n.scale(a / 4, e);
          n.clearRect(0, 0, k, h.maxDepth - 1);
          1 == this._mode && n.scale(1, 1 / h.snapshotCount);
          for (var g = 0, d = h.snapshotCount;g < d;g++) {
            var c = h.getSnapshotAt(g);
            if (c) {
              var f = null, y = 0;
              n.beginPath();
              n.moveTo(0, 0);
              for (var x = 0;x < k;x++) {
                y = h.startTime + x * q, y = (f = f ? f.queryNext(y) : c.query(y)) ? f.getDepth() - 1 : 0, n.lineTo(x, y);
              }
              n.lineTo(x, 0);
              n.fillStyle = v;
              n.fill();
              1 == this._mode && n.translate(0, -b * a / e);
            }
          }
          n.restore();
        };
        b.prototype._toPixelsRelative = function(a) {
          return a * this._width / (this._rangeEnd - this._rangeStart);
        };
        b.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - this._rangeStart);
        };
        b.prototype._toTimeRelative = function(a) {
          return a * (this._rangeEnd - this._rangeStart) / this._width;
        };
        b.prototype._toTime = function(a) {
          return this._toTimeRelative(a) + this._rangeStart;
        };
        b.prototype._getDragTargetUnderCursor = function(a, b) {
          if (0 <= b && b < this._height) {
            var h = this._toPixels(this._windowStart), k = this._toPixels(this._windowEnd), q = 2 + u.FlameChartBase.DRAGHANDLE_WIDTH / 2, n = a >= h - q && a <= h + q, v = a >= k - q && a <= k + q;
            if (n && v) {
              return 4;
            }
            if (n) {
              return 2;
            }
            if (v) {
              return 3;
            }
            if (!this._windowEqRange() && a > h + q && a < k - q) {
              return 1;
            }
          }
          return 0;
        };
        b.prototype.onMouseDown = function(a, b) {
          var h = this._getDragTargetUnderCursor(a, b);
          0 === h ? (this._selection = {left:a, right:a}, this.draw()) : (1 === h && this._mouseController.updateCursor(u.MouseCursor.GRABBING), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:h});
        };
        b.prototype.onMouseMove = function(a, b) {
          var h = u.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, b);
          0 === k || this._selection || (h = 1 === k ? u.MouseCursor.GRAB : u.MouseCursor.EW_RESIZE);
          this._mouseController.updateCursor(h);
        };
        b.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        b.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(u.MouseCursor.DEFAULT);
        };
        b.prototype.onDrag = function(b, k, h, t, q, n) {
          if (this._selection) {
            this._selection = {left:b, right:a(h, 0, this._width - 1)}, this.draw();
          } else {
            b = this._dragInfo;
            if (4 === b.target) {
              if (0 !== q) {
                b.target = 0 > q ? 2 : 3;
              } else {
                return;
              }
            }
            k = this._windowStart;
            h = this._windowEnd;
            q = this._toTimeRelative(q);
            switch(b.target) {
              case 1:
                k = b.windowStartInitial + q;
                h = b.windowEndInitial + q;
                break;
              case 2:
                k = a(b.windowStartInitial + q, this._rangeStart, h - u.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3:
                h = a(b.windowEndInitial + q, k + u.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(k, h);
          }
        };
        b.prototype.onDragEnd = function(a, b, h, k, q, n) {
          this._selection && (this._selection = null, this._controller.setWindow(this._toTime(a), this._toTime(h)));
          this._dragInfo = null;
          this.onMouseMove(h, k);
        };
        b.prototype.onClick = function(a, b) {
          this._selection = this._dragInfo = null;
          this._windowEqRange() || (0 === this._getDragTargetUnderCursor(a, b) && this._controller.moveWindowTo(this._toTime(a)), this.onMouseMove(a, b));
          this.draw();
        };
        b.prototype.onHoverStart = function(a, b) {
        };
        b.prototype.onHoverEnd = function() {
        };
        return b;
      }(u.FlameChartBase);
      u.FlameChartOverview = p;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(p) {
      var a = k.NumberUtilities.clamp;
      (function(a) {
        a[a.OVERVIEW = 0] = "OVERVIEW";
        a[a.CHART = 1] = "CHART";
      })(p.FlameChartHeaderType || (p.FlameChartHeaderType = {}));
      var w = function(k) {
        function b(a, b) {
          this._type = b;
          k.call(this, a);
        }
        __extends(b, k);
        b.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio, h = this._width, k = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.tabToolbar(1);
          a.fillRect(0, 0, h, k);
          this._initialized && (0 == this._type ? (b = this._toPixels(this._windowStart), h = this._toPixels(this._windowEnd), a.fillStyle = this._controller.theme.bodyBackground(1), a.fillRect(b, 0, h - b, k), this._drawLabels(this._rangeStart, this._rangeEnd), this._drawDragHandle(b), this._drawDragHandle(h)) : this._drawLabels(this._windowStart, this._windowEnd));
          a.restore();
        };
        b.prototype._drawLabels = function(a, k) {
          var h = this._context, t = this._calculateTickInterval(a, k), q = Math.ceil(a / t) * t, n = 500 <= t, v = n ? 1E3 : 1, e = this._decimalPlaces(t / v), n = n ? "s" : "ms", g = this._toPixels(q), d = this._height / 2, c = this._controller.theme;
          h.lineWidth = 1;
          h.strokeStyle = c.contentTextDarkGrey(.5);
          h.fillStyle = c.contentTextDarkGrey(1);
          h.textAlign = "right";
          h.textBaseline = "middle";
          h.font = "11px sans-serif";
          for (c = this._width + b.TICK_MAX_WIDTH;g < c;) {
            var f = (q / v).toFixed(e) + " " + n;
            h.fillText(f, g - 7, d + 1);
            h.beginPath();
            h.moveTo(g, 0);
            h.lineTo(g, this._height + 1);
            h.closePath();
            h.stroke();
            q += t;
            g = this._toPixels(q);
          }
        };
        b.prototype._calculateTickInterval = function(a, k) {
          var h = (k - a) / (this._width / b.TICK_MAX_WIDTH), t = Math.pow(10, Math.floor(Math.log(h) / Math.LN10)), h = h / t;
          return 5 < h ? 10 * t : 2 < h ? 5 * t : 1 < h ? 2 * t : t;
        };
        b.prototype._drawDragHandle = function(a) {
          var b = this._context;
          b.lineWidth = 2;
          b.strokeStyle = this._controller.theme.bodyBackground(1);
          b.fillStyle = this._controller.theme.foregroundTextGrey(.7);
          this._drawRoundedRect(b, a - p.FlameChartBase.DRAGHANDLE_WIDTH / 2, 1, p.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2, 2, !0);
        };
        b.prototype._drawRoundedRect = function(a, b, h, k, q, n, v, e) {
          void 0 === v && (v = !0);
          void 0 === e && (e = !0);
          a.beginPath();
          a.moveTo(b + n, h);
          a.lineTo(b + k - n, h);
          a.quadraticCurveTo(b + k, h, b + k, h + n);
          a.lineTo(b + k, h + q - n);
          a.quadraticCurveTo(b + k, h + q, b + k - n, h + q);
          a.lineTo(b + n, h + q);
          a.quadraticCurveTo(b, h + q, b, h + q - n);
          a.lineTo(b, h + n);
          a.quadraticCurveTo(b, h, b + n, h);
          a.closePath();
          v && a.stroke();
          e && a.fill();
        };
        b.prototype._toPixelsRelative = function(a) {
          return a * this._width / (0 === this._type ? this._rangeEnd - this._rangeStart : this._windowEnd - this._windowStart);
        };
        b.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - (0 === this._type ? this._rangeStart : this._windowStart));
        };
        b.prototype._toTimeRelative = function(a) {
          return a * (0 === this._type ? this._rangeEnd - this._rangeStart : this._windowEnd - this._windowStart) / this._width;
        };
        b.prototype._toTime = function(a) {
          var b = 0 === this._type ? this._rangeStart : this._windowStart;
          return this._toTimeRelative(a) + b;
        };
        b.prototype._getDragTargetUnderCursor = function(a, b) {
          if (0 <= b && b < this._height) {
            if (0 === this._type) {
              var h = this._toPixels(this._windowStart), k = this._toPixels(this._windowEnd), q = 2 + p.FlameChartBase.DRAGHANDLE_WIDTH / 2, h = a >= h - q && a <= h + q, k = a >= k - q && a <= k + q;
              if (h && k) {
                return 4;
              }
              if (h) {
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
        b.prototype.onMouseDown = function(a, b) {
          var h = this._getDragTargetUnderCursor(a, b);
          1 === h && this._mouseController.updateCursor(p.MouseCursor.GRABBING);
          this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:h};
        };
        b.prototype.onMouseMove = function(a, b) {
          var h = p.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, b);
          0 !== k && (1 !== k ? h = p.MouseCursor.EW_RESIZE : 1 !== k || this._windowEqRange() || (h = p.MouseCursor.GRAB));
          this._mouseController.updateCursor(h);
        };
        b.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        b.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(p.MouseCursor.DEFAULT);
        };
        b.prototype.onDrag = function(b, k, h, t, q, n) {
          b = this._dragInfo;
          if (4 === b.target) {
            if (0 !== q) {
              b.target = 0 > q ? 2 : 3;
            } else {
              return;
            }
          }
          k = this._windowStart;
          h = this._windowEnd;
          q = this._toTimeRelative(q);
          switch(b.target) {
            case 1:
              h = 0 === this._type ? 1 : -1;
              k = b.windowStartInitial + h * q;
              h = b.windowEndInitial + h * q;
              break;
            case 2:
              k = a(b.windowStartInitial + q, this._rangeStart, h - p.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3:
              h = a(b.windowEndInitial + q, k + p.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(k, h);
        };
        b.prototype.onDragEnd = function(a, b, h, k, q, n) {
          this._dragInfo = null;
          this.onMouseMove(h, k);
        };
        b.prototype.onClick = function(a, b) {
          1 === this._dragInfo.target && this._mouseController.updateCursor(p.MouseCursor.GRAB);
        };
        b.prototype.onHoverStart = function(a, b) {
        };
        b.prototype.onHoverEnd = function() {
        };
        b.TICK_MAX_WIDTH = 75;
        return b;
      }(p.FlameChartBase);
      p.FlameChartHeader = w;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(k) {
      (function(a) {
        var k = function() {
          function a(b, k, h, t, q) {
            this.pageLoaded = b;
            this.threadsTotal = k;
            this.threadsLoaded = h;
            this.threadFilesTotal = t;
            this.threadFilesLoaded = q;
          }
          a.prototype.toString = function() {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function(a, b, h) {
              return a + ":" + this[a];
            }, this).join(", ") + "]";
          };
          return a;
        }();
        a.TraceLoggerProgressInfo = k;
        var m = function() {
          function b(a) {
            this._baseUrl = a;
            this._threads = [];
            this._progressInfo = null;
          }
          b.prototype.loadPage = function(a, b, h) {
            this._threads = [];
            this._pageLoadCallback = b;
            this._pageLoadProgressCallback = h;
            this._progressInfo = new k(!1, 0, 0, 0, 0);
            this._loadData([a], this._onLoadPage.bind(this));
          };
          Object.defineProperty(b.prototype, "buffers", {get:function() {
            for (var a = [], b = 0, h = this._threads.length;b < h;b++) {
              a.push(this._threads[b].buffer);
            }
            return a;
          }, enumerable:!0, configurable:!0});
          b.prototype._onProgress = function() {
            this._pageLoadProgressCallback && this._pageLoadProgressCallback.call(this, this._progressInfo);
          };
          b.prototype._onLoadPage = function(b) {
            if (b && 1 == b.length) {
              var k = this, h = 0;
              b = b[0];
              var t = b.length;
              this._threads = Array(t);
              this._progressInfo.pageLoaded = !0;
              this._progressInfo.threadsTotal = t;
              for (var q = 0;q < b.length;q++) {
                var n = b[q], v = [n.dict, n.tree];
                n.corrections && v.push(n.corrections);
                this._progressInfo.threadFilesTotal += v.length;
                this._loadData(v, function(b) {
                  return function(g) {
                    g && (g = new a.Thread(g), g.buffer.name = "Thread " + b, k._threads[b] = g);
                    h++;
                    k._progressInfo.threadsLoaded++;
                    k._onProgress();
                    h === t && k._pageLoadCallback.call(k, null, k._threads);
                  };
                }(q), function(a) {
                  k._progressInfo.threadFilesLoaded++;
                  k._onProgress();
                });
              }
              this._onProgress();
            } else {
              this._pageLoadCallback.call(this, "Error loading page.", null);
            }
          };
          b.prototype._loadData = function(a, b, h) {
            var k = 0, q = 0, n = a.length, v = [];
            v.length = n;
            for (var e = 0;e < n;e++) {
              var g = this._baseUrl + a[e], d = /\.tl$/i.test(g), c = new XMLHttpRequest, d = d ? "arraybuffer" : "json";
              c.open("GET", g, !0);
              c.responseType = d;
              c.onload = function(d, c) {
                return function(a) {
                  if ("json" === c) {
                    if (a = this.response, "string" === typeof a) {
                      try {
                        a = JSON.parse(a), v[d] = a;
                      } catch (g) {
                        q++;
                      }
                    } else {
                      v[d] = a;
                    }
                  } else {
                    v[d] = this.response;
                  }
                  ++k;
                  h && h(k);
                  k === n && b(v);
                };
              }(e, d);
              c.send();
            }
          };
          b.colors = "#0044ff #8c4b00 #cc5c33 #ff80c4 #ffbfd9 #ff8800 #8c5e00 #adcc33 #b380ff #bfd9ff #ffaa00 #8c0038 #bf8f30 #f780ff #cc99c9 #aaff00 #000073 #452699 #cc8166 #cca799 #000066 #992626 #cc6666 #ccc299 #ff6600 #526600 #992663 #cc6681 #99ccc2 #ff0066 #520066 #269973 #61994d #739699 #ffcc00 #006629 #269199 #94994d #738299 #ff0000 #590000 #234d8c #8c6246 #7d7399 #ee00ff #00474d #8c2385 #8c7546 #7c8c69 #eeff00 #4d003d #662e1a #62468c #8c6969 #6600ff #4c2900 #1a6657 #8c464f #8c6981 #44ff00 #401100 #1a2466 #663355 #567365 #d90074 #403300 #101d40 #59562d #66614d #cc0000 #002b40 #234010 #4c2626 #4d5e66 #00a3cc #400011 #231040 #4c3626 #464359 #0000bf #331b00 #80e6ff #311a33 #4d3939 #a69b00 #003329 #80ffb2 #331a20 #40303d #00a658 #40ffd9 #ffc480 #ffe1bf #332b26 #8c2500 #9933cc #80fff6 #ffbfbf #303326 #005e8c #33cc47 #b2ff80 #c8bfff #263332 #00708c #cc33ad #ffe680 #f2ffbf #262a33 #388c00 #335ccc #8091ff #bfffd9".split(" ");
          return b;
        }();
        a.TraceLogger = m;
      })(k.TraceLogger || (k.TraceLogger = {}));
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(k) {
      (function(a) {
        var p;
        (function(a) {
          a[a.START_HI = 0] = "START_HI";
          a[a.START_LO = 4] = "START_LO";
          a[a.STOP_HI = 8] = "STOP_HI";
          a[a.STOP_LO = 12] = "STOP_LO";
          a[a.TEXTID = 16] = "TEXTID";
          a[a.NEXTID = 20] = "NEXTID";
        })(p || (p = {}));
        p = function() {
          function a(b) {
            2 <= b.length && (this._text = b[0], this._data = new DataView(b[1]), this._buffer = new k.TimelineBuffer, this._walkTree(0));
          }
          Object.defineProperty(a.prototype, "buffer", {get:function() {
            return this._buffer;
          }, enumerable:!0, configurable:!0});
          a.prototype._walkTree = function(b) {
            var k = this._data, r = this._buffer;
            do {
              var h = b * a.ITEM_SIZE, t = 4294967295 * k.getUint32(h + 0) + k.getUint32(h + 4), q = 4294967295 * k.getUint32(h + 8) + k.getUint32(h + 12), n = k.getUint32(h + 16), h = k.getUint32(h + 20), v = 1 === (n & 1), n = n >>> 1, n = this._text[n];
              r.enter(n, null, t / 1E6);
              v && this._walkTree(b + 1);
              r.leave(n, null, q / 1E6);
              b = h;
            } while (0 !== b);
          };
          a.ITEM_SIZE = 24;
          return a;
        }();
        a.Thread = p;
      })(k.TraceLogger || (k.TraceLogger = {}));
    })(k.Profiler || (k.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(p) {
      var a = k.NumberUtilities.clamp, w = function() {
        function a() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        a.prototype.append = function(a, b) {
          var h = this.lines;
          0 < h.length && h[h.length - 1] === a ? this.repeat[h.length - 1]++ : (this.lines.push(a), this.repeat.push(1), this.format.push(b ? {backgroundFillStyle:b} : void 0), this.time.push(performance.now()), this.length++);
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
      p.Buffer = w;
      var m = function() {
        function b(a) {
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
          this.buffer = new w;
          a.addEventListener("keydown", function(a) {
            var l = 0;
            switch(a.keyCode) {
              case x:
                this.printHelp();
                break;
              case f:
                this.showLineNumbers = !this.showLineNumbers;
                break;
              case y:
                this.showLineTime = !this.showLineTime;
                break;
              case n:
                l = -1;
                break;
              case v:
                l = 1;
                break;
              case b:
                l = -this.pageLineCount;
                break;
              case h:
                l = this.pageLineCount;
                break;
              case k:
                l = -this.lineIndex;
                break;
              case q:
                l = this.buffer.length - this.lineIndex;
                break;
              case e:
                this.columnIndex -= a.metaKey ? 10 : 1;
                0 > this.columnIndex && (this.columnIndex = 0);
                a.preventDefault();
                break;
              case g:
                this.columnIndex += a.metaKey ? 10 : 1;
                a.preventDefault();
                break;
              case d:
                if (a.metaKey || a.ctrlKey) {
                  this.selection = {start:0, end:this.buffer.length - 1}, a.preventDefault();
                }
                break;
              case c:
              ;
              case I:
                if (a.metaKey || a.ctrlKey) {
                  var m = "";
                  if (this.selection) {
                    for (var S = this.selection.start;S <= this.selection.end;S++) {
                      m += this.buffer.get(S) + "\n";
                    }
                  } else {
                    m = this.buffer.get(this.lineIndex);
                  }
                  a.keyCode === c ? alert(m) : window.open(URL.createObjectURL(new Blob([m], {type:"text/plain"})), "_blank");
                }
              ;
            }
            a.metaKey && (l *= this.pageLineCount);
            l && (this.scroll(l), a.preventDefault());
            l && a.shiftKey ? this.selection ? this.lineIndex > this.selection.start ? this.selection.end = this.lineIndex : this.selection.start = this.lineIndex : 0 < l ? this.selection = {start:this.lineIndex - l, end:this.lineIndex} : 0 > l && (this.selection = {start:this.lineIndex, end:this.lineIndex - l}) : l && (this.selection = null);
            this.paint();
          }.bind(this), !1);
          a.addEventListener("focus", function(d) {
            this.hasFocus = !0;
          }.bind(this), !1);
          a.addEventListener("blur", function(d) {
            this.hasFocus = !1;
          }.bind(this), !1);
          var b = 33, h = 34, k = 36, q = 35, n = 38, v = 40, e = 37, g = 39, d = 65, c = 67, f = 78, y = 84, x = 72, I = 83;
        }
        b.prototype.printHelp = function() {
          var a = this;
          "h - help;n - turn on/off line numbers;t - turn on/off line time;arrow_keys - navigation;cmd/ctrl+a - select all;cmd/ctrl+c - copy/alert selection;cmd/ctrl+s - open selection in new tab;shift+arrow_keys - selection".split(";").forEach(function(b) {
            return a.buffer.append(b, "#002000");
          });
        };
        b.prototype.resize = function() {
          this._resizeHandler();
        };
        b.prototype._resizeHandler = function() {
          var a = this.canvas.parentElement, b = a.clientWidth, a = a.clientHeight && a.clientHeight - 1, h = window.devicePixelRatio || 1;
          1 !== h ? (this.ratio = h / 1, this.canvas.width = b * this.ratio, this.canvas.height = a * this.ratio, this.canvas.style.width = b + "px", this.canvas.style.height = a + "px") : (this.ratio = 1, this.canvas.width = b, this.canvas.height = a);
          this.pageLineCount = Math.floor(this.canvas.height / this.lineHeight);
        };
        b.prototype.gotoLine = function(b) {
          this.lineIndex = a(b, 0, this.buffer.length - 1);
        };
        b.prototype.scrollIntoView = function() {
          this.lineIndex < this.pageIndex ? this.pageIndex = this.lineIndex : this.lineIndex >= this.pageIndex + this.pageLineCount && (this.pageIndex = this.lineIndex - this.pageLineCount + 1);
        };
        b.prototype.scroll = function(a) {
          this.gotoLine(this.lineIndex + a);
          this.scrollIntoView();
        };
        b.prototype.paint = function() {
          var a = this.pageLineCount;
          this.pageIndex + a > this.buffer.length && (a = this.buffer.length - this.pageIndex);
          var b = this.textMarginLeft, h = b + (this.showLineNumbers ? 5 * (String(this.buffer.length).length + 2) : 0), k = h + (this.showLineTime ? 40 : 10), q = k + 25;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
          for (var n = this.canvas.width, v = this.lineHeight, e = 0;e < a;e++) {
            var g = e * this.lineHeight, d = this.pageIndex + e, c = this.buffer.get(d), f = this.buffer.getFormat(d), y = this.buffer.getRepeat(d), x = 1 < d ? this.buffer.getTime(d) - this.buffer.getTime(0) : 0;
            this.context.fillStyle = d % 2 ? this.lineColor : this.alternateLineColor;
            f && f.backgroundFillStyle && (this.context.fillStyle = f.backgroundFillStyle);
            this.context.fillRect(0, g, n, v);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;
            this.selection && d >= this.selection.start && d <= this.selection.end && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, g, n, v), this.context.fillStyle = this.selectionTextColor);
            this.hasFocus && d === this.lineIndex && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, g, n, v), this.context.fillStyle = this.selectionTextColor);
            0 < this.columnIndex && (c = c.substring(this.columnIndex));
            g = (e + 1) * this.lineHeight - this.textMarginBottom;
            this.showLineNumbers && this.context.fillText(String(d), b, g);
            this.showLineTime && this.context.fillText(x.toFixed(1).padLeft(" ", 6), h, g);
            1 < y && this.context.fillText(String(y).padLeft(" ", 3), k, g);
            this.context.fillText(c, q, g);
          }
        };
        b.prototype.refreshEvery = function(a) {
          function b() {
            h.paint();
            h.refreshFrequency && setTimeout(b, h.refreshFrequency);
          }
          var h = this;
          this.refreshFrequency = a;
          h.refreshFrequency && setTimeout(b, h.refreshFrequency);
        };
        b.prototype.isScrolledToBottom = function() {
          return this.lineIndex === this.buffer.length - 1;
        };
        return b;
      }();
      p.Terminal = m;
    })(p.Terminal || (p.Terminal = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(k) {
      var a = function() {
        function a(k) {
          this._lastWeightedTime = this._lastTime = this._index = 0;
          this._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
          this._container = k;
          this._canvas = document.createElement("canvas");
          this._container.appendChild(this._canvas);
          this._context = this._canvas.getContext("2d");
          this._listenForContainerSizeChanges();
        }
        a.prototype._listenForContainerSizeChanges = function() {
          var a = this._containerWidth, b = this._containerHeight;
          this._onContainerSizeChanged();
          var k = this;
          setInterval(function() {
            if (a !== k._containerWidth || b !== k._containerHeight) {
              k._onContainerSizeChanged(), a = k._containerWidth, b = k._containerHeight;
            }
          }, 10);
        };
        a.prototype._onContainerSizeChanged = function() {
          var a = this._containerWidth, b = this._containerHeight, k = window.devicePixelRatio || 1;
          1 !== k ? (this._ratio = k / 1, this._canvas.width = a * this._ratio, this._canvas.height = b * this._ratio, this._canvas.style.width = a + "px", this._canvas.style.height = b + "px") : (this._ratio = 1, this._canvas.width = a, this._canvas.height = b);
        };
        Object.defineProperty(a.prototype, "_containerWidth", {get:function() {
          return this._container.clientWidth;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "_containerHeight", {get:function() {
          return this._container.clientHeight;
        }, enumerable:!0, configurable:!0});
        a.prototype.tickAndRender = function(a, b) {
          void 0 === a && (a = !1);
          void 0 === b && (b = 0);
          if (0 === this._lastTime) {
            this._lastTime = performance.now();
          } else {
            var k = 1 * (performance.now() - this._lastTime) + 0 * this._lastWeightedTime, r = this._context, h = 2 * this._ratio, t = 30 * this._ratio, q = performance;
            q.memory && (t += 30 * this._ratio);
            var n = (this._canvas.width - t) / (h + 1) | 0, v = this._index++;
            this._index > n && (this._index = 0);
            n = this._canvas.height;
            r.globalAlpha = 1;
            r.fillStyle = "black";
            r.fillRect(t + v * (h + 1), 0, 4 * h, this._canvas.height);
            var e = Math.min(1E3 / 60 / k, 1);
            r.fillStyle = "#00FF00";
            r.globalAlpha = a ? .5 : 1;
            e = n / 2 * e | 0;
            r.fillRect(t + v * (h + 1), n - e, h, e);
            b && (e = Math.min(1E3 / 240 / b, 1), r.fillStyle = "#FF6347", e = n / 2 * e | 0, r.fillRect(t + v * (h + 1), n / 2 - e, h, e));
            0 === v % 16 && (r.globalAlpha = 1, r.fillStyle = "black", r.fillRect(0, 0, t, this._canvas.height), r.fillStyle = "white", r.font = 8 * this._ratio + "px Arial", r.textBaseline = "middle", h = (1E3 / k).toFixed(0), b && (h += " " + b.toFixed(0)), q.memory && (h += " " + (q.memory.usedJSHeapSize / 1024 / 1024).toFixed(2)), r.fillText(h, 2 * this._ratio, this._containerHeight / 2 * this._ratio));
            this._lastTime = performance.now();
            this._lastWeightedTime = k;
          }
        };
        return a;
      }();
      k.FPS = a;
    })(k.Mini || (k.Mini = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load Shared Dependencies");
console.time("Load GFX Dependencies");
(function(k) {
  (function(p) {
    function u(d, f, a) {
      return r && a ? "string" === typeof f ? (d = k.ColorUtilities.cssStyleToRGBA(f), k.ColorUtilities.rgbaToCSSStyle(a.transformRGBA(d))) : f instanceof CanvasGradient && f._template ? f._template.createCanvasGradient(d, a) : f : f;
    }
    var a = k.NumberUtilities.clamp;
    (function(d) {
      d[d.None = 0] = "None";
      d[d.Brief = 1] = "Brief";
      d[d.Verbose = 2] = "Verbose";
    })(p.TraceLevel || (p.TraceLevel = {}));
    var w = k.Metrics.Counter.instance;
    p.frameCounter = new k.Metrics.Counter(!0);
    p.traceLevel = 2;
    p.writer = null;
    p.frameCount = function(d) {
      w.count(d);
      p.frameCounter.count(d);
    };
    p.timelineBuffer = new k.Tools.Profiler.TimelineBuffer("GFX");
    p.enterTimeline = function(d, f) {
    };
    p.leaveTimeline = function(d, f) {
    };
    var m = null, b = null, l = null, r = !0;
    r && "undefined" !== typeof CanvasRenderingContext2D && (m = CanvasGradient.prototype.addColorStop, b = CanvasRenderingContext2D.prototype.createLinearGradient, l = CanvasRenderingContext2D.prototype.createRadialGradient, CanvasRenderingContext2D.prototype.createLinearGradient = function(d, f, a, c) {
      return (new t(d, f, a, c)).createCanvasGradient(this, null);
    }, CanvasRenderingContext2D.prototype.createRadialGradient = function(d, f, a, c, b, g) {
      return (new q(d, f, a, c, b, g)).createCanvasGradient(this, null);
    }, CanvasGradient.prototype.addColorStop = function(d, f) {
      m.call(this, d, f);
      this._template.addColorStop(d, f);
    });
    var h = function() {
      return function(d, f) {
        this.offset = d;
        this.color = f;
      };
    }(), t = function() {
      function d(f, a, c, b) {
        this.x0 = f;
        this.y0 = a;
        this.x1 = c;
        this.y1 = b;
        this.colorStops = [];
      }
      d.prototype.addColorStop = function(d, f) {
        this.colorStops.push(new h(d, f));
      };
      d.prototype.createCanvasGradient = function(d, f) {
        for (var a = b.call(d, this.x0, this.y0, this.x1, this.y1), c = this.colorStops, g = 0;g < c.length;g++) {
          var e = c[g], x = e.offset, e = e.color, e = f ? u(d, e, f) : e;
          m.call(a, x, e);
        }
        a._template = this;
        a._transform = this._transform;
        return a;
      };
      return d;
    }(), q = function() {
      function d(f, a, c, b, g, e) {
        this.x0 = f;
        this.y0 = a;
        this.r0 = c;
        this.x1 = b;
        this.y1 = g;
        this.r1 = e;
        this.colorStops = [];
      }
      d.prototype.addColorStop = function(d, f) {
        this.colorStops.push(new h(d, f));
      };
      d.prototype.createCanvasGradient = function(d, f) {
        for (var a = l.call(d, this.x0, this.y0, this.r0, this.x1, this.y1, this.r1), c = this.colorStops, b = 0;b < c.length;b++) {
          var g = c[b], e = g.offset, g = g.color, g = f ? u(d, g, f) : g;
          m.call(a, e, g);
        }
        a._template = this;
        a._transform = this._transform;
        return a;
      };
      return d;
    }(), n;
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
    })(n || (n = {}));
    var v = function() {
      function d(f) {
        this._commands = new Uint8Array(d._arrayBufferPool.acquire(8), 0, 8);
        this._commandPosition = 0;
        this._data = new Float64Array(d._arrayBufferPool.acquire(8 * Float64Array.BYTES_PER_ELEMENT), 0, 8);
        this._dataPosition = 0;
        f instanceof d && this.addPath(f);
      }
      d._apply = function(d, f) {
        var a = d._commands, c = d._data, b = 0, g = 0;
        f.beginPath();
        for (var e = d._commandPosition;b < e;) {
          switch(a[b++]) {
            case 1:
              f.closePath();
              break;
            case 2:
              f.moveTo(c[g++], c[g++]);
              break;
            case 3:
              f.lineTo(c[g++], c[g++]);
              break;
            case 4:
              f.quadraticCurveTo(c[g++], c[g++], c[g++], c[g++]);
              break;
            case 5:
              f.bezierCurveTo(c[g++], c[g++], c[g++], c[g++], c[g++], c[g++]);
              break;
            case 6:
              f.arcTo(c[g++], c[g++], c[g++], c[g++], c[g++]);
              break;
            case 7:
              f.rect(c[g++], c[g++], c[g++], c[g++]);
              break;
            case 8:
              f.arc(c[g++], c[g++], c[g++], c[g++], c[g++], !!c[g++]);
              break;
            case 9:
              f.save();
              break;
            case 10:
              f.restore();
              break;
            case 11:
              f.transform(c[g++], c[g++], c[g++], c[g++], c[g++], c[g++]);
          }
        }
      };
      d.prototype._ensureCommandCapacity = function(f) {
        this._commands = d._arrayBufferPool.ensureUint8ArrayLength(this._commands, f);
      };
      d.prototype._ensureDataCapacity = function(f) {
        this._data = d._arrayBufferPool.ensureFloat64ArrayLength(this._data, f);
      };
      d.prototype._writeCommand = function(d) {
        this._commandPosition >= this._commands.length && this._ensureCommandCapacity(this._commandPosition + 1);
        this._commands[this._commandPosition++] = d;
      };
      d.prototype._writeData = function(d, f, a, c, b, g) {
        var e = arguments.length;
        this._dataPosition + e >= this._data.length && this._ensureDataCapacity(this._dataPosition + e);
        var x = this._data, h = this._dataPosition;
        x[h] = d;
        x[h + 1] = f;
        2 < e && (x[h + 2] = a, x[h + 3] = c, 4 < e && (x[h + 4] = b, 6 === e && (x[h + 5] = g)));
        this._dataPosition += e;
      };
      d.prototype.closePath = function() {
        this._writeCommand(1);
      };
      d.prototype.moveTo = function(d, f) {
        this._writeCommand(2);
        this._writeData(d, f);
      };
      d.prototype.lineTo = function(d, f) {
        this._writeCommand(3);
        this._writeData(d, f);
      };
      d.prototype.quadraticCurveTo = function(d, f, a, c) {
        this._writeCommand(4);
        this._writeData(d, f, a, c);
      };
      d.prototype.bezierCurveTo = function(d, f, a, c, b, g) {
        this._writeCommand(5);
        this._writeData(d, f, a, c, b, g);
      };
      d.prototype.arcTo = function(d, f, a, c, b) {
        this._writeCommand(6);
        this._writeData(d, f, a, c, b);
      };
      d.prototype.rect = function(d, f, a, c) {
        this._writeCommand(7);
        this._writeData(d, f, a, c);
      };
      d.prototype.arc = function(d, f, a, c, b, g) {
        this._writeCommand(8);
        this._writeData(d, f, a, c, b, +g);
      };
      d.prototype.addPath = function(d, f) {
        f && (this._writeCommand(9), this._writeCommand(11), this._writeData(f.a, f.b, f.c, f.d, f.e, f.f));
        var a = this._commandPosition + d._commandPosition;
        a >= this._commands.length && this._ensureCommandCapacity(a);
        for (var c = this._commands, b = d._commands, g = this._commandPosition, e = 0;g < a;g++) {
          c[g] = b[e++];
        }
        this._commandPosition = a;
        a = this._dataPosition + d._dataPosition;
        a >= this._data.length && this._ensureDataCapacity(a);
        c = this._data;
        b = d._data;
        g = this._dataPosition;
        for (e = 0;g < a;g++) {
          c[g] = b[e++];
        }
        this._dataPosition = a;
        f && this._writeCommand(10);
      };
      d._arrayBufferPool = new k.ArrayBufferPool;
      return d;
    }();
    p.Path = v;
    if ("undefined" !== typeof CanvasRenderingContext2D && ("undefined" === typeof Path2D || !Path2D.prototype.addPath)) {
      var e = CanvasRenderingContext2D.prototype.fill;
      CanvasRenderingContext2D.prototype.fill = function(d, f) {
        arguments.length && (d instanceof v ? v._apply(d, this) : f = d);
        f ? e.call(this, f) : e.call(this);
      };
      var g = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.stroke = function(d, f) {
        arguments.length && (d instanceof v ? v._apply(d, this) : f = d);
        f ? g.call(this, f) : g.call(this);
      };
      var d = CanvasRenderingContext2D.prototype.clip;
      CanvasRenderingContext2D.prototype.clip = function(f, a) {
        arguments.length && (f instanceof v ? v._apply(f, this) : a = f);
        a ? d.call(this, a) : d.call(this);
      };
      window.Path2D = v;
    }
    if ("undefined" !== typeof CanvasPattern && Path2D.prototype.addPath) {
      n = function(d) {
        this._transform = d;
        this._template && (this._template._transform = d);
      };
      CanvasPattern.prototype.setTransform || (CanvasPattern.prototype.setTransform = n);
      CanvasGradient.prototype.setTransform || (CanvasGradient.prototype.setTransform = n);
      var c = CanvasRenderingContext2D.prototype.fill, f = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.fill = function(d, f) {
        var a = !!this.fillStyle._transform;
        if ((this.fillStyle instanceof CanvasPattern || this.fillStyle instanceof CanvasGradient) && a && d instanceof Path2D) {
          var a = this.fillStyle._transform, b;
          try {
            b = a.inverse();
          } catch (g) {
            b = a = p.Geometry.Matrix.createIdentitySVGMatrix();
          }
          this.transform(a.a, a.b, a.c, a.d, a.e, a.f);
          a = new Path2D;
          a.addPath(d, b);
          c.call(this, a, f);
          this.transform(b.a, b.b, b.c, b.d, b.e, b.f);
        } else {
          0 === arguments.length ? c.call(this) : 1 === arguments.length ? c.call(this, d) : 2 === arguments.length && c.call(this, d, f);
        }
      };
      CanvasRenderingContext2D.prototype.stroke = function(d) {
        var a = !!this.strokeStyle._transform;
        if ((this.strokeStyle instanceof CanvasPattern || this.strokeStyle instanceof CanvasGradient) && a && d instanceof Path2D) {
          var c = this.strokeStyle._transform, a = c.inverse();
          this.transform(c.a, c.b, c.c, c.d, c.e, c.f);
          c = new Path2D;
          c.addPath(d, a);
          var b = this.lineWidth;
          this.lineWidth *= (a.a + a.d) / 2;
          f.call(this, c);
          this.transform(a.a, a.b, a.c, a.d, a.e, a.f);
          this.lineWidth = b;
        } else {
          0 === arguments.length ? f.call(this) : 1 === arguments.length && f.call(this, d);
        }
      };
    }
    "undefined" !== typeof CanvasRenderingContext2D && function() {
      function d() {
        return p.Geometry.Matrix.createSVGMatrixFromArray(this.mozCurrentTransform);
      }
      var f = "currentTransform" in CanvasRenderingContext2D.prototype;
      CanvasRenderingContext2D.prototype.flashStroke = function(d, c) {
        if (f) {
          var b = this.currentTransform, g = new Path2D;
          g.addPath(d, b);
          var e = this.lineWidth;
          this.setTransform(1, 0, 0, 1, 0, 0);
          switch(c) {
            case 1:
              var x = Math.sqrt((b.a + b.c) * (b.a + b.c) + (b.d + b.b) * (b.d + b.b)) * Math.SQRT1_2;
              this.lineWidth = a(e * x, 1, 1024);
              break;
            case 2:
              this.lineWidth = a(e * (b.d + b.b), 1, 1024);
              break;
            case 3:
              this.lineWidth = a(e * (b.a + b.c), 1, 1024);
          }
          this.stroke(g);
          this.setTransform(b.a, b.b, b.c, b.d, b.e, b.f);
          this.lineWidth = e;
        } else {
          this.stroke(d);
        }
      };
      if (!f) {
        if ("mozCurrentTransform" in CanvasRenderingContext2D.prototype) {
          Object.defineProperty(CanvasRenderingContext2D.prototype, "currentTransform", {get:d}), f = !0;
        } else {
          var c = CanvasRenderingContext2D.prototype.setTransform;
          CanvasRenderingContext2D.prototype.setTransform = function(d, f, a, b, g, e) {
            var x = this.currentTransform;
            x.a = d;
            x.b = f;
            x.c = a;
            x.d = b;
            x.e = g;
            x.f = e;
            c.call(this, d, f, a, b, g, e);
          };
          Object.defineProperty(CanvasRenderingContext2D.prototype, "currentTransform", {get:function() {
            return this._currentTransform || (this._currentTransform = p.Geometry.Matrix.createIdentitySVGMatrix());
          }});
        }
      }
    }();
    if ("undefined" !== typeof CanvasRenderingContext2D && void 0 === CanvasRenderingContext2D.prototype.globalColorMatrix) {
      var y = CanvasRenderingContext2D.prototype.fill, x = CanvasRenderingContext2D.prototype.stroke, I = CanvasRenderingContext2D.prototype.fillText, da = CanvasRenderingContext2D.prototype.strokeText;
      Object.defineProperty(CanvasRenderingContext2D.prototype, "globalColorMatrix", {get:function() {
        return this._globalColorMatrix ? this._globalColorMatrix.clone() : null;
      }, set:function(d) {
        d ? this._globalColorMatrix ? this._globalColorMatrix.set(d) : this._globalColorMatrix = d.clone() : this._globalColorMatrix = null;
      }, enumerable:!0, configurable:!0});
      CanvasRenderingContext2D.prototype.fill = function(d, f) {
        var a = null;
        this._globalColorMatrix && (a = this.fillStyle, this.fillStyle = u(this, this.fillStyle, this._globalColorMatrix));
        0 === arguments.length ? y.call(this) : 1 === arguments.length ? y.call(this, d) : 2 === arguments.length && y.call(this, d, f);
        a && (this.fillStyle = a);
      };
      CanvasRenderingContext2D.prototype.stroke = function(d, f) {
        var a = null;
        this._globalColorMatrix && (a = this.strokeStyle, this.strokeStyle = u(this, this.strokeStyle, this._globalColorMatrix));
        0 === arguments.length ? x.call(this) : 1 === arguments.length && x.call(this, d);
        a && (this.strokeStyle = a);
      };
      CanvasRenderingContext2D.prototype.fillText = function(d, f, a, c) {
        var b = null;
        this._globalColorMatrix && (b = this.fillStyle, this.fillStyle = u(this, this.fillStyle, this._globalColorMatrix));
        3 === arguments.length ? I.call(this, d, f, a) : 4 === arguments.length ? I.call(this, d, f, a, c) : k.Debug.unexpected();
        b && (this.fillStyle = b);
      };
      CanvasRenderingContext2D.prototype.strokeText = function(d, f, a, c) {
        var b = null;
        this._globalColorMatrix && (b = this.strokeStyle, this.strokeStyle = u(this, this.strokeStyle, this._globalColorMatrix));
        3 === arguments.length ? da.call(this, d, f, a) : 4 === arguments.length ? da.call(this, d, f, a, c) : k.Debug.unexpected();
        b && (this.strokeStyle = b);
      };
    }
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    var u = function() {
      return function(a, k, m, b) {
        this.dataURL = a;
        this.w = k;
        this.h = m;
        this.pixelRatio = b;
      };
    }();
    k.ScreenShot = u;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var p = function() {
    function k() {
      this._count = 0;
      this._head = this._tail = null;
    }
    Object.defineProperty(k.prototype, "count", {get:function() {
      return this._count;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(k.prototype, "head", {get:function() {
      return this._head;
    }, enumerable:!0, configurable:!0});
    k.prototype._unshift = function(a) {
      0 === this._count ? this._head = this._tail = a : (a.next = this._head, this._head = a.next.previous = a);
      this._count++;
    };
    k.prototype._remove = function(a) {
      a === this._head && a === this._tail ? this._head = this._tail = null : a === this._head ? (this._head = a.next, this._head.previous = null) : a == this._tail ? (this._tail = a.previous, this._tail.next = null) : (a.previous.next = a.next, a.next.previous = a.previous);
      a.previous = a.next = null;
      this._count--;
    };
    k.prototype.use = function(a) {
      this._head !== a && ((a.next || a.previous || this._tail === a) && this._remove(a), this._unshift(a));
    };
    k.prototype.pop = function() {
      if (!this._tail) {
        return null;
      }
      var a = this._tail;
      this._remove(a);
      return a;
    };
    k.prototype.visit = function(a, k) {
      void 0 === k && (k = !0);
      for (var m = k ? this._head : this._tail;m && a(m);) {
        m = k ? m.next : m.previous;
      }
    };
    return k;
  }();
  k.LRUList = p;
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
GFX$$inline_40.filters = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "filters", "boolean", !0, ""));
GFX$$inline_40.cacheShapes = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "cacheShapes", "boolean", !0, ""));
GFX$$inline_40.cacheShapesMaxSize = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "cacheShapesMaxSize", "number", 256, "", {range:{min:1, max:1024, step:1}}));
GFX$$inline_40.cacheShapesThreshold = canvas2DOptions$$inline_47.register(new Option$$inline_41("", "cacheShapesThreshold", "number", 256, "", {range:{min:1, max:1024, step:1}}));
(function(k) {
  (function(p) {
    (function(u) {
      function a(a, d, c, f) {
        var b = 1 - f;
        return a * b * b + 2 * d * b * f + c * f * f;
      }
      function w(a, d, c, f, b) {
        var e = b * b, h = 1 - b, k = h * h;
        return a * h * k + 3 * d * b * k + 3 * c * h * e + f * b * e;
      }
      var m = k.NumberUtilities.clamp, b = k.NumberUtilities.pow2, l = k.NumberUtilities.epsilonEquals;
      u.radianToDegrees = function(a) {
        return 180 * a / Math.PI;
      };
      u.degreesToRadian = function(a) {
        return a * Math.PI / 180;
      };
      u.quadraticBezier = a;
      u.quadraticBezierExtreme = function(b, d, c) {
        var f = (b - d) / (b - 2 * d + c);
        return 0 > f ? b : 1 < f ? c : a(b, d, c, f);
      };
      u.cubicBezier = w;
      u.cubicBezierExtremes = function(a, d, c, f) {
        var b = d - a, e;
        e = 2 * (c - d);
        var h = f - c;
        b + h === e && (h *= 1.0001);
        var k = 2 * b - e, n = e - 2 * b, n = Math.sqrt(n * n - 4 * b * (b - e + h));
        e = 2 * (b - e + h);
        b = (k + n) / e;
        k = (k - n) / e;
        n = [];
        0 <= b && 1 >= b && n.push(w(a, d, c, f, b));
        0 <= k && 1 >= k && n.push(w(a, d, c, f, k));
        return n;
      };
      var r = function() {
        function a(d, c) {
          this.x = d;
          this.y = c;
        }
        a.prototype.setElements = function(d, a) {
          this.x = d;
          this.y = a;
          return this;
        };
        a.prototype.set = function(d) {
          this.x = d.x;
          this.y = d.y;
          return this;
        };
        a.prototype.dot = function(d) {
          return this.x * d.x + this.y * d.y;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.distanceTo = function(d) {
          return Math.sqrt(this.dot(d));
        };
        a.prototype.sub = function(d) {
          this.x -= d.x;
          this.y -= d.y;
          return this;
        };
        a.prototype.mul = function(d) {
          this.x *= d;
          this.y *= d;
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y);
        };
        a.prototype.toString = function(d) {
          void 0 === d && (d = 2);
          return "{x: " + this.x.toFixed(d) + ", y: " + this.y.toFixed(d) + "}";
        };
        a.prototype.inTriangle = function(d, a, f) {
          var b = d.y * f.x - d.x * f.y + (f.y - d.y) * this.x + (d.x - f.x) * this.y, g = d.x * a.y - d.y * a.x + (d.y - a.y) * this.x + (a.x - d.x) * this.y;
          if (0 > b != 0 > g) {
            return !1;
          }
          d = -a.y * f.x + d.y * (f.x - a.x) + d.x * (a.y - f.y) + a.x * f.y;
          0 > d && (b = -b, g = -g, d = -d);
          return 0 < b && 0 < g && b + g < d;
        };
        a.createEmpty = function() {
          return new a(0, 0);
        };
        a.createEmptyPoints = function(d) {
          for (var c = [], f = 0;f < d;f++) {
            c.push(new a(0, 0));
          }
          return c;
        };
        return a;
      }();
      u.Point = r;
      var h = function() {
        function a(d, c, f) {
          this.x = d;
          this.y = c;
          this.z = f;
        }
        a.prototype.setElements = function(d, a, f) {
          this.x = d;
          this.y = a;
          this.z = f;
          return this;
        };
        a.prototype.set = function(d) {
          this.x = d.x;
          this.y = d.y;
          this.z = d.z;
          return this;
        };
        a.prototype.dot = function(d) {
          return this.x * d.x + this.y * d.y + this.z * d.z;
        };
        a.prototype.cross = function(d) {
          var a = this.z * d.x - this.x * d.z, f = this.x * d.y - this.y * d.x;
          this.x = this.y * d.z - this.z * d.y;
          this.y = a;
          this.z = f;
          return this;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.sub = function(d) {
          this.x -= d.x;
          this.y -= d.y;
          this.z -= d.z;
          return this;
        };
        a.prototype.mul = function(d) {
          this.x *= d;
          this.y *= d;
          this.z *= d;
          return this;
        };
        a.prototype.normalize = function() {
          var d = Math.sqrt(this.squaredLength());
          1E-5 < d ? this.mul(1 / d) : this.setElements(0, 0, 0);
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y, this.z);
        };
        a.prototype.toString = function(d) {
          void 0 === d && (d = 2);
          return "{x: " + this.x.toFixed(d) + ", y: " + this.y.toFixed(d) + ", z: " + this.z.toFixed(d) + "}";
        };
        a.createEmpty = function() {
          return new a(0, 0, 0);
        };
        a.createEmptyPoints = function(d) {
          for (var c = [], f = 0;f < d;f++) {
            c.push(new a(0, 0, 0));
          }
          return c;
        };
        return a;
      }();
      u.Point3D = h;
      var t = function() {
        function a(d, c, f, b) {
          this.setElements(d, c, f, b);
          a.allocationCount++;
        }
        a.prototype.setElements = function(d, a, f, b) {
          this.x = d;
          this.y = a;
          this.w = f;
          this.h = b;
        };
        a.prototype.set = function(a) {
          this.x = a.x;
          this.y = a.y;
          this.w = a.w;
          this.h = a.h;
        };
        a.prototype.contains = function(a) {
          var c = a.x + a.w, f = a.y + a.h, b = this.x + this.w, g = this.y + this.h;
          return a.x >= this.x && a.x < b && a.y >= this.y && a.y < g && c > this.x && c <= b && f > this.y && f <= g;
        };
        a.prototype.containsPoint = function(a) {
          return a.x >= this.x && a.x < this.x + this.w && a.y >= this.y && a.y < this.y + this.h;
        };
        a.prototype.isContained = function(a) {
          for (var c = 0;c < a.length;c++) {
            if (a[c].contains(this)) {
              return !0;
            }
          }
          return !1;
        };
        a.prototype.isSmallerThan = function(a) {
          return this.w < a.w && this.h < a.h;
        };
        a.prototype.isLargerThan = function(a) {
          return this.w > a.w && this.h > a.h;
        };
        a.prototype.union = function(a) {
          if (this.isEmpty()) {
            this.set(a);
          } else {
            if (!a.isEmpty()) {
              var c = this.x, f = this.y;
              this.x > a.x && (c = a.x);
              this.y > a.y && (f = a.y);
              var b = this.x + this.w;
              b < a.x + a.w && (b = a.x + a.w);
              var g = this.y + this.h;
              g < a.y + a.h && (g = a.y + a.h);
              this.x = c;
              this.y = f;
              this.w = b - c;
              this.h = g - f;
            }
          }
        };
        a.prototype.isEmpty = function() {
          return 0 >= this.w || 0 >= this.h;
        };
        a.prototype.setEmpty = function() {
          this.h = this.w = this.y = this.x = 0;
        };
        a.prototype.intersect = function(d) {
          var c = a.createEmpty();
          if (this.isEmpty() || d.isEmpty()) {
            return c.setEmpty(), c;
          }
          c.x = Math.max(this.x, d.x);
          c.y = Math.max(this.y, d.y);
          c.w = Math.min(this.x + this.w, d.x + d.w) - c.x;
          c.h = Math.min(this.y + this.h, d.y + d.h) - c.y;
          c.isEmpty() && c.setEmpty();
          this.set(c);
        };
        a.prototype.intersects = function(a) {
          if (this.isEmpty() || a.isEmpty()) {
            return !1;
          }
          var c = Math.max(this.x, a.x), f = Math.max(this.y, a.y), c = Math.min(this.x + this.w, a.x + a.w) - c;
          a = Math.min(this.y + this.h, a.y + a.h) - f;
          return !(0 >= c || 0 >= a);
        };
        a.prototype.intersectsTransformedAABB = function(d, c) {
          var f = a._temporary;
          f.set(d);
          c.transformRectangleAABB(f);
          return this.intersects(f);
        };
        a.prototype.intersectsTranslated = function(a, c, f) {
          if (this.isEmpty() || a.isEmpty()) {
            return !1;
          }
          var b = Math.max(this.x, a.x + c), g = Math.max(this.y, a.y + f);
          c = Math.min(this.x + this.w, a.x + c + a.w) - b;
          a = Math.min(this.y + this.h, a.y + f + a.h) - g;
          return !(0 >= c || 0 >= a);
        };
        a.prototype.area = function() {
          return this.w * this.h;
        };
        a.prototype.clone = function() {
          var d = a.allocate();
          d.set(this);
          return d;
        };
        a.allocate = function() {
          var d = a._dirtyStack;
          return d.length ? d.pop() : new a(12345, 67890, 12345, 67890);
        };
        a.prototype.free = function() {
          a._dirtyStack.push(this);
        };
        a.prototype.snap = function() {
          var a = Math.ceil(this.x + this.w), c = Math.ceil(this.y + this.h);
          this.x = Math.floor(this.x);
          this.y = Math.floor(this.y);
          this.w = a - this.x;
          this.h = c - this.y;
          return this;
        };
        a.prototype.scale = function(a, c) {
          this.x *= a;
          this.y *= c;
          this.w *= a;
          this.h *= c;
          return this;
        };
        a.prototype.offset = function(a, c) {
          this.x += a;
          this.y += c;
          return this;
        };
        a.prototype.resize = function(a, c) {
          this.w += a;
          this.h += c;
          return this;
        };
        a.prototype.expand = function(a, c) {
          this.offset(-a, -c).resize(2 * a, 2 * c);
          return this;
        };
        a.prototype.getCenter = function() {
          return new r(this.x + this.w / 2, this.y + this.h / 2);
        };
        a.prototype.getAbsoluteBounds = function() {
          return new a(0, 0, this.w, this.h);
        };
        a.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          return "{" + this.x.toFixed(a) + ", " + this.y.toFixed(a) + ", " + this.w.toFixed(a) + ", " + this.h.toFixed(a) + "}";
        };
        a.createEmpty = function() {
          var d = a.allocate();
          d.setEmpty();
          return d;
        };
        a.createSquare = function(d) {
          return new a(-d / 2, -d / 2, d, d);
        };
        a.createMaxI16 = function() {
          return new a(-32768, -32768, 65535, 65535);
        };
        a.prototype.setMaxI16 = function() {
          this.setElements(-32768, -32768, 65535, 65535);
        };
        a.prototype.getCorners = function(a) {
          a[0].x = this.x;
          a[0].y = this.y;
          a[1].x = this.x + this.w;
          a[1].y = this.y;
          a[2].x = this.x + this.w;
          a[2].y = this.y + this.h;
          a[3].x = this.x;
          a[3].y = this.y + this.h;
        };
        a.allocationCount = 0;
        a._temporary = new a(0, 0, 0, 0);
        a._dirtyStack = [];
        return a;
      }();
      u.Rectangle = t;
      var q = function() {
        function a(d) {
          this.corners = d.map(function(a) {
            return a.clone();
          });
          this.axes = [d[1].clone().sub(d[0]), d[3].clone().sub(d[0])];
          this.origins = [];
          for (var c = 0;2 > c;c++) {
            this.axes[c].mul(1 / this.axes[c].squaredLength()), this.origins.push(d[0].dot(this.axes[c]));
          }
        }
        a.prototype.getBounds = function() {
          return a.getBounds(this.corners);
        };
        a.getBounds = function(a) {
          for (var c = new r(Number.MAX_VALUE, Number.MAX_VALUE), f = new r(Number.MIN_VALUE, Number.MIN_VALUE), b = 0;4 > b;b++) {
            var g = a[b].x, e = a[b].y;
            c.x = Math.min(c.x, g);
            c.y = Math.min(c.y, e);
            f.x = Math.max(f.x, g);
            f.y = Math.max(f.y, e);
          }
          return new t(c.x, c.y, f.x - c.x, f.y - c.y);
        };
        a.prototype.intersects = function(a) {
          return this.intersectsOneWay(a) && a.intersectsOneWay(this);
        };
        a.prototype.intersectsOneWay = function(a) {
          for (var c = 0;2 > c;c++) {
            for (var f = 0;4 > f;f++) {
              var b = a.corners[f].dot(this.axes[c]), g, e;
              0 === f ? e = g = b : b < g ? g = b : b > e && (e = b);
            }
            if (g > 1 + this.origins[c] || e < this.origins[c]) {
              return !1;
            }
          }
          return !0;
        };
        return a;
      }();
      u.OBB = q;
      (function(a) {
        a[a.Unknown = 0] = "Unknown";
        a[a.Identity = 1] = "Identity";
        a[a.Translation = 2] = "Translation";
      })(u.MatrixType || (u.MatrixType = {}));
      var n = function() {
        function a(d, c, f, b, e, h) {
          this._data = new Float64Array(6);
          this._type = 0;
          this.setElements(d, c, f, b, e, h);
          a.allocationCount++;
        }
        Object.defineProperty(a.prototype, "a", {get:function() {
          return this._data[0];
        }, set:function(a) {
          this._data[0] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "b", {get:function() {
          return this._data[1];
        }, set:function(a) {
          this._data[1] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "c", {get:function() {
          return this._data[2];
        }, set:function(a) {
          this._data[2] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "d", {get:function() {
          return this._data[3];
        }, set:function(a) {
          this._data[3] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "tx", {get:function() {
          return this._data[4];
        }, set:function(a) {
          this._data[4] = a;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "ty", {get:function() {
          return this._data[5];
        }, set:function(a) {
          this._data[5] = a;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        a._createSVGMatrix = function() {
          a._svg || (a._svg = document.createElementNS("http://www.w3.org/2000/svg", "svg"));
          return a._svg.createSVGMatrix();
        };
        a.prototype.setElements = function(a, c, f, b, e, g) {
          var h = this._data;
          h[0] = a;
          h[1] = c;
          h[2] = f;
          h[3] = b;
          h[4] = e;
          h[5] = g;
          this._type = 0;
        };
        a.prototype.set = function(a) {
          var c = this._data, f = a._data;
          c[0] = f[0];
          c[1] = f[1];
          c[2] = f[2];
          c[3] = f[3];
          c[4] = f[4];
          c[5] = f[5];
          this._type = a._type;
        };
        a.prototype.emptyArea = function(a) {
          a = this._data;
          return 0 === a[0] || 0 === a[3] ? !0 : !1;
        };
        a.prototype.infiniteArea = function(a) {
          a = this._data;
          return Infinity === Math.abs(a[0]) || Infinity === Math.abs(a[3]) ? !0 : !1;
        };
        a.prototype.isEqual = function(a) {
          if (1 === this._type && 1 === a._type) {
            return !0;
          }
          var c = this._data;
          a = a._data;
          return c[0] === a[0] && c[1] === a[1] && c[2] === a[2] && c[3] === a[3] && c[4] === a[4] && c[5] === a[5];
        };
        a.prototype.clone = function() {
          var d = a.allocate();
          d.set(this);
          return d;
        };
        a.allocate = function() {
          var d = a._dirtyStack;
          return d.length ? d.pop() : new a(12345, 12345, 12345, 12345, 12345, 12345);
        };
        a.prototype.free = function() {
          a._dirtyStack.push(this);
        };
        a.prototype.transform = function(a, c, f, b, e, g) {
          var h = this._data, k = h[0], n = h[1], q = h[2], t = h[3], l = h[4], v = h[5];
          h[0] = k * a + q * c;
          h[1] = n * a + t * c;
          h[2] = k * f + q * b;
          h[3] = n * f + t * b;
          h[4] = k * e + q * g + l;
          h[5] = n * e + t * g + v;
          this._type = 0;
          return this;
        };
        a.prototype.transformRectangle = function(a, c) {
          var f = this._data, b = f[0], e = f[1], g = f[2], h = f[3], k = f[4], f = f[5], n = a.x, q = a.y, t = a.w, l = a.h;
          c[0].x = b * n + g * q + k;
          c[0].y = e * n + h * q + f;
          c[1].x = b * (n + t) + g * q + k;
          c[1].y = e * (n + t) + h * q + f;
          c[2].x = b * (n + t) + g * (q + l) + k;
          c[2].y = e * (n + t) + h * (q + l) + f;
          c[3].x = b * n + g * (q + l) + k;
          c[3].y = e * n + h * (q + l) + f;
        };
        a.prototype.isTranslationOnly = function() {
          if (2 === this._type) {
            return !0;
          }
          var a = this._data;
          return 1 === a[0] && 0 === a[1] && 0 === a[2] && 1 === a[3] || l(a[0], 1) && l(a[1], 0) && l(a[2], 0) && l(a[3], 1) ? (this._type = 2, !0) : !1;
        };
        a.prototype.transformRectangleAABB = function(a) {
          var c = this._data;
          if (1 !== this._type) {
            if (2 === this._type) {
              a.x += c[4], a.y += c[5];
            } else {
              var f = c[0], b = c[1], e = c[2], g = c[3], h = c[4], k = c[5], n = a.x, q = a.y, t = a.w, l = a.h, c = f * n + e * q + h, v = b * n + g * q + k, r = f * (n + t) + e * q + h, m = b * (n + t) + g * q + k, p = f * (n + t) + e * (q + l) + h, t = b * (n + t) + g * (q + l) + k, f = f * n + e * (q + l) + h, b = b * n + g * (q + l) + k, g = 0;
              c > r && (g = c, c = r, r = g);
              p > f && (g = p, p = f, f = g);
              a.x = c < p ? c : p;
              a.w = (r > f ? r : f) - a.x;
              v > m && (g = v, v = m, m = g);
              t > b && (g = t, t = b, b = g);
              a.y = v < t ? v : t;
              a.h = (m > b ? m : b) - a.y;
            }
          }
        };
        a.prototype.scale = function(a, c) {
          var f = this._data;
          f[0] *= a;
          f[1] *= c;
          f[2] *= a;
          f[3] *= c;
          f[4] *= a;
          f[5] *= c;
          this._type = 0;
          return this;
        };
        a.prototype.scaleClone = function(a, c) {
          return 1 === a && 1 === c ? this : this.clone().scale(a, c);
        };
        a.prototype.rotate = function(a) {
          var c = this._data, f = c[0], b = c[1], e = c[2], g = c[3], h = c[4], k = c[5], n = Math.cos(a);
          a = Math.sin(a);
          c[0] = n * f - a * b;
          c[1] = a * f + n * b;
          c[2] = n * e - a * g;
          c[3] = a * e + n * g;
          c[4] = n * h - a * k;
          c[5] = a * h + n * k;
          this._type = 0;
          return this;
        };
        a.prototype.concat = function(a) {
          if (1 === a._type) {
            return this;
          }
          var c = this._data;
          a = a._data;
          var f = c[0] * a[0], b = 0, e = 0, g = c[3] * a[3], h = c[4] * a[0] + a[4], k = c[5] * a[3] + a[5];
          if (0 !== c[1] || 0 !== c[2] || 0 !== a[1] || 0 !== a[2]) {
            f += c[1] * a[2], g += c[2] * a[1], b += c[0] * a[1] + c[1] * a[3], e += c[2] * a[0] + c[3] * a[2], h += c[5] * a[2], k += c[4] * a[1];
          }
          c[0] = f;
          c[1] = b;
          c[2] = e;
          c[3] = g;
          c[4] = h;
          c[5] = k;
          this._type = 0;
          return this;
        };
        a.prototype.concatClone = function(a) {
          return this.clone().concat(a);
        };
        a.prototype.preMultiply = function(a) {
          var c = this._data, f = a._data;
          if (2 === a._type && this._type & 3) {
            c[4] += f[4], c[5] += f[5], this._type = 2;
          } else {
            if (1 !== a._type) {
              a = f[0] * c[0];
              var b = 0, e = 0, g = f[3] * c[3], h = f[4] * c[0] + c[4], k = f[5] * c[3] + c[5];
              if (0 !== f[1] || 0 !== f[2] || 0 !== c[1] || 0 !== c[2]) {
                a += f[1] * c[2], g += f[2] * c[1], b += f[0] * c[1] + f[1] * c[3], e += f[2] * c[0] + f[3] * c[2], h += f[5] * c[2], k += f[4] * c[1];
              }
              c[0] = a;
              c[1] = b;
              c[2] = e;
              c[3] = g;
              c[4] = h;
              c[5] = k;
              this._type = 0;
            }
          }
        };
        a.prototype.translate = function(a, c) {
          var f = this._data;
          f[4] += a;
          f[5] += c;
          1 === this._type && (this._type = 2);
          return this;
        };
        a.prototype.setIdentity = function() {
          var a = this._data;
          a[0] = 1;
          a[1] = 0;
          a[2] = 0;
          a[3] = 1;
          a[4] = 0;
          a[5] = 0;
          this._type = 1;
        };
        a.prototype.isIdentity = function() {
          if (1 === this._type) {
            return !0;
          }
          var a = this._data;
          return 1 === a[0] && 0 === a[1] && 0 === a[2] && 1 === a[3] && 0 === a[4] && 0 === a[5];
        };
        a.prototype.transformPoint = function(a) {
          if (1 !== this._type) {
            var c = this._data, f = a.x, b = a.y;
            a.x = c[0] * f + c[2] * b + c[4];
            a.y = c[1] * f + c[3] * b + c[5];
          }
        };
        a.prototype.transformPoints = function(a) {
          if (1 !== this._type) {
            for (var c = 0;c < a.length;c++) {
              this.transformPoint(a[c]);
            }
          }
        };
        a.prototype.deltaTransformPoint = function(a) {
          if (1 !== this._type) {
            var c = this._data, f = a.x, b = a.y;
            a.x = c[0] * f + c[2] * b;
            a.y = c[1] * f + c[3] * b;
          }
        };
        a.prototype.inverse = function(a) {
          var c = this._data, f = a._data;
          if (1 === this._type) {
            a.setIdentity();
          } else {
            if (2 === this._type) {
              f[0] = 1, f[1] = 0, f[2] = 0, f[3] = 1, f[4] = -c[4], f[5] = -c[5], a._type = 2;
            } else {
              var b = c[1], e = c[2], g = c[4], h = c[5];
              if (0 === b && 0 === e) {
                var k = f[0] = 1 / c[0], c = f[3] = 1 / c[3];
                f[1] = 0;
                f[2] = 0;
                f[4] = -k * g;
                f[5] = -c * h;
              } else {
                var k = c[0], c = c[3], n = k * c - b * e;
                if (0 === n) {
                  a.setIdentity();
                  return;
                }
                n = 1 / n;
                f[0] = c * n;
                b = f[1] = -b * n;
                e = f[2] = -e * n;
                c = f[3] = k * n;
                f[4] = -(f[0] * g + e * h);
                f[5] = -(b * g + c * h);
              }
              a._type = 0;
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
          var a = this._data;
          if (1 === a[0] && 0 === a[1]) {
            return 1;
          }
          var c = Math.sqrt(a[0] * a[0] + a[1] * a[1]);
          return 0 < a[0] ? c : -c;
        };
        a.prototype.getScaleY = function() {
          var a = this._data;
          if (0 === a[2] && 1 === a[3]) {
            return 1;
          }
          var c = Math.sqrt(a[2] * a[2] + a[3] * a[3]);
          return 0 < a[3] ? c : -c;
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
          var a = this._data;
          return 180 * Math.atan(a[1] / a[0]) / Math.PI;
        };
        a.prototype.isScaleOrRotation = function() {
          var a = this._data;
          return .01 > Math.abs(a[0] * a[2] + a[1] * a[3]);
        };
        a.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          var c = this._data;
          return "{" + c[0].toFixed(a) + ", " + c[1].toFixed(a) + ", " + c[2].toFixed(a) + ", " + c[3].toFixed(a) + ", " + c[4].toFixed(a) + ", " + c[5].toFixed(a) + "}";
        };
        a.prototype.toWebGLMatrix = function() {
          var a = this._data;
          return new Float32Array([a[0], a[1], 0, a[2], a[3], 0, a[4], a[5], 1]);
        };
        a.prototype.toCSSTransform = function() {
          var a = this._data;
          return "matrix(" + a[0] + ", " + a[1] + ", " + a[2] + ", " + a[3] + ", " + a[4] + ", " + a[5] + ")";
        };
        a.createIdentity = function() {
          var d = a.allocate();
          d.setIdentity();
          return d;
        };
        a.prototype.toSVGMatrix = function() {
          var d = this._data, c = a._createSVGMatrix();
          try {
            c.a = d[0], c.b = d[1], c.c = d[2], c.d = d[3], c.e = d[4], c.f = d[5];
          } catch (f) {
            return a._createSVGMatrix();
          }
          return c;
        };
        a.prototype.snap = function() {
          var a = this._data;
          return this.isTranslationOnly() ? (a[0] = 1, a[1] = 0, a[2] = 0, a[3] = 1, a[4] = Math.round(a[4]), a[5] = Math.round(a[5]), this._type = 2, !0) : !1;
        };
        a.createIdentitySVGMatrix = function() {
          return a._createSVGMatrix();
        };
        a.createSVGMatrixFromArray = function(d) {
          var c = a._createSVGMatrix();
          c.a = d[0];
          c.b = d[1];
          c.c = d[2];
          c.d = d[3];
          c.e = d[4];
          c.f = d[5];
          return c;
        };
        a.allocationCount = 0;
        a._dirtyStack = [];
        a.multiply = function(a, c) {
          var f = c._data;
          a.transform(f[0], f[1], f[2], f[3], f[4], f[5]);
        };
        return a;
      }();
      u.Matrix = n;
      n = function() {
        function a(d) {
          this._m = new Float32Array(d);
        }
        a.prototype.asWebGLMatrix = function() {
          return this._m;
        };
        a.createCameraLookAt = function(d, c, f) {
          c = d.clone().sub(c).normalize();
          f = f.clone().cross(c).normalize();
          var b = c.clone().cross(f);
          return new a([f.x, f.y, f.z, 0, b.x, b.y, b.z, 0, c.x, c.y, c.z, 0, d.x, d.y, d.z, 1]);
        };
        a.createLookAt = function(d, c, f) {
          c = d.clone().sub(c).normalize();
          f = f.clone().cross(c).normalize();
          var b = c.clone().cross(f);
          return new a([f.x, b.x, c.x, 0, b.x, b.y, c.y, 0, c.x, b.z, c.z, 0, -f.dot(d), -b.dot(d), -c.dot(d), 1]);
        };
        a.prototype.mul = function(a) {
          a = [a.x, a.y, a.z, 0];
          for (var c = this._m, f = [], b = 0;4 > b;b++) {
            f[b] = 0;
            for (var e = 4 * b, g = 0;4 > g;g++) {
              f[b] += c[e + g] * a[g];
            }
          }
          return new h(f[0], f[1], f[2]);
        };
        a.create2DProjection = function(d, c, f) {
          return new a([2 / d, 0, 0, 0, 0, -2 / c, 0, 0, 0, 0, 2 / f, 0, -1, 1, 0, 1]);
        };
        a.createPerspective = function(d, c, f, b) {
          d = Math.tan(.5 * Math.PI - .5 * d);
          var e = 1 / (f - b);
          return new a([d / c, 0, 0, 0, 0, d, 0, 0, 0, 0, (f + b) * e, -1, 0, 0, f * b * e * 2, 0]);
        };
        a.createIdentity = function() {
          return a.createTranslation(0, 0, 0);
        };
        a.createTranslation = function(d, c, f) {
          return new a([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, d, c, f, 1]);
        };
        a.createXRotation = function(d) {
          var c = Math.cos(d);
          d = Math.sin(d);
          return new a([1, 0, 0, 0, 0, c, d, 0, 0, -d, c, 0, 0, 0, 0, 1]);
        };
        a.createYRotation = function(d) {
          var c = Math.cos(d);
          d = Math.sin(d);
          return new a([c, 0, -d, 0, 0, 1, 0, 0, d, 0, c, 0, 0, 0, 0, 1]);
        };
        a.createZRotation = function(d) {
          var c = Math.cos(d);
          d = Math.sin(d);
          return new a([c, d, 0, 0, -d, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
        };
        a.createScale = function(d, c, f) {
          return new a([d, 0, 0, 0, 0, c, 0, 0, 0, 0, f, 0, 0, 0, 0, 1]);
        };
        a.createMultiply = function(d, c) {
          var f = d._m, b = c._m, e = f[0], h = f[1], k = f[2], n = f[3], q = f[4], t = f[5], l = f[6], v = f[7], r = f[8], m = f[9], p = f[10], u = f[11], w = f[12], z = f[13], B = f[14], f = f[15], E = b[0], A = b[1], C = b[2], F = b[3], G = b[4], J = b[5], H = b[6], K = b[7], L = b[8], M = b[9], N = b[10], O = b[11], P = b[12], Q = b[13], R = b[14], b = b[15];
          return new a([e * E + h * G + k * L + n * P, e * A + h * J + k * M + n * Q, e * C + h * H + k * N + n * R, e * F + h * K + k * O + n * b, q * E + t * G + l * L + v * P, q * A + t * J + l * M + v * Q, q * C + t * H + l * N + v * R, q * F + t * K + l * O + v * b, r * E + m * G + p * L + u * P, r * A + m * J + p * M + u * Q, r * C + m * H + p * N + u * R, r * F + m * K + p * O + u * b, w * E + z * G + B * L + f * P, w * A + z * J + B * M + f * Q, w * C + z * H + B * N + f * R, w * F + z * 
          K + B * O + f * b]);
        };
        a.createInverse = function(d) {
          var c = d._m;
          d = c[0];
          var f = c[1], b = c[2], e = c[3], h = c[4], k = c[5], n = c[6], q = c[7], t = c[8], l = c[9], v = c[10], r = c[11], m = c[12], p = c[13], u = c[14], c = c[15], w = v * c, z = u * r, B = n * c, E = u * q, A = n * r, C = v * q, F = b * c, G = u * e, J = b * r, H = v * e, K = b * q, L = n * e, M = t * p, N = m * l, O = h * p, P = m * k, Q = h * l, R = t * k, X = d * p, Y = m * f, Z = d * l, aa = t * f, ba = d * k, ca = h * f, ea = w * k + E * l + A * p - (z * k + B * l + C * p), fa = z * f + 
          F * l + H * p - (w * f + G * l + J * p), p = B * f + G * k + K * p - (E * f + F * k + L * p), f = C * f + J * k + L * l - (A * f + H * k + K * l), k = 1 / (d * ea + h * fa + t * p + m * f);
          return new a([k * ea, k * fa, k * p, k * f, k * (z * h + B * t + C * m - (w * h + E * t + A * m)), k * (w * d + G * t + J * m - (z * d + F * t + H * m)), k * (E * d + F * h + L * m - (B * d + G * h + K * m)), k * (A * d + H * h + K * t - (C * d + J * h + L * t)), k * (M * q + P * r + Q * c - (N * q + O * r + R * c)), k * (N * e + X * r + aa * c - (M * e + Y * r + Z * c)), k * (O * e + Y * q + ba * c - (P * e + X * q + ca * c)), k * (R * e + Z * q + ca * r - (Q * e + aa * q + ba * r)), k * 
          (O * v + R * u + N * n - (Q * u + M * n + P * v)), k * (Z * u + M * b + Y * v - (X * v + aa * u + N * b)), k * (X * n + ca * u + P * b - (ba * u + O * b + Y * n)), k * (ba * v + Q * b + aa * n - (Z * n + ca * v + R * b))]);
        };
        return a;
      }();
      u.Matrix3D = n;
      n = function() {
        function a(d, c, f) {
          void 0 === f && (f = 7);
          var b = this.size = 1 << f;
          this.sizeInBits = f;
          this.w = d;
          this.h = c;
          this.c = Math.ceil(d / b);
          this.r = Math.ceil(c / b);
          this.grid = [];
          for (d = 0;d < this.r;d++) {
            for (this.grid.push([]), c = 0;c < this.c;c++) {
              this.grid[d][c] = new a.Cell(new t(c * b, d * b, b, b));
            }
          }
        }
        a.prototype.clear = function() {
          for (var a = 0;a < this.r;a++) {
            for (var c = 0;c < this.c;c++) {
              this.grid[a][c].clear();
            }
          }
        };
        a.prototype.getBounds = function() {
          return new t(0, 0, this.w, this.h);
        };
        a.prototype.addDirtyRectangle = function(a) {
          var c = a.x >> this.sizeInBits, f = a.y >> this.sizeInBits;
          if (!(c >= this.c || f >= this.r)) {
            0 > c && (c = 0);
            0 > f && (f = 0);
            var b = this.grid[f][c];
            a = a.clone();
            a.snap();
            if (b.region.contains(a)) {
              b.bounds.isEmpty() ? b.bounds.set(a) : b.bounds.contains(a) || b.bounds.union(a);
            } else {
              for (var e = Math.min(this.c, Math.ceil((a.x + a.w) / this.size)) - c, g = Math.min(this.r, Math.ceil((a.y + a.h) / this.size)) - f, h = 0;h < e;h++) {
                for (var k = 0;k < g;k++) {
                  b = this.grid[f + k][c + h], b = b.region.clone(), b.intersect(a), b.isEmpty() || this.addDirtyRectangle(b);
                }
              }
            }
          }
        };
        a.prototype.gatherRegions = function(a) {
          for (var c = 0;c < this.r;c++) {
            for (var f = 0;f < this.c;f++) {
              this.grid[c][f].bounds.isEmpty() || a.push(this.grid[c][f].bounds);
            }
          }
        };
        a.prototype.gatherOptimizedRegions = function(a) {
          this.gatherRegions(a);
        };
        a.prototype.getDirtyRatio = function() {
          var a = this.w * this.h;
          if (0 === a) {
            return 0;
          }
          for (var c = 0, f = 0;f < this.r;f++) {
            for (var b = 0;b < this.c;b++) {
              c += this.grid[f][b].region.area();
            }
          }
          return c / a;
        };
        a.prototype.render = function(a, c) {
          function f(f) {
            a.rect(f.x, f.y, f.w, f.h);
          }
          if (c && c.drawGrid) {
            a.strokeStyle = "white";
            for (var b = 0;b < this.r;b++) {
              for (var e = 0;e < this.c;e++) {
                var g = this.grid[b][e];
                a.beginPath();
                f(g.region);
                a.closePath();
                a.stroke();
              }
            }
          }
          a.strokeStyle = "#E0F8D8";
          for (b = 0;b < this.r;b++) {
            for (e = 0;e < this.c;e++) {
              g = this.grid[b][e], a.beginPath(), f(g.bounds), a.closePath(), a.stroke();
            }
          }
        };
        a.tmpRectangle = t.createEmpty();
        return a;
      }();
      u.DirtyRegion = n;
      (function(a) {
        var d = function() {
          function a(f) {
            this.region = f;
            this.bounds = t.createEmpty();
          }
          a.prototype.clear = function() {
            this.bounds.setEmpty();
          };
          return a;
        }();
        a.Cell = d;
      })(n = u.DirtyRegion || (u.DirtyRegion = {}));
      var v = function() {
        function a(d, c, f, b, e, g) {
          this.index = d;
          this.x = c;
          this.y = f;
          this.scale = g;
          this.bounds = new t(c * b, f * e, b, e);
        }
        a.prototype.getOBB = function() {
          if (this._obb) {
            return this._obb;
          }
          this.bounds.getCorners(a.corners);
          return this._obb = new q(a.corners);
        };
        a.corners = r.createEmptyPoints(4);
        return a;
      }();
      u.Tile = v;
      var e = function() {
        function a(d, c, f, b, e) {
          this.tileW = f;
          this.tileH = b;
          this.scale = e;
          this.w = d;
          this.h = c;
          this.rows = Math.ceil(c / b);
          this.columns = Math.ceil(d / f);
          this.tiles = [];
          for (c = d = 0;c < this.rows;c++) {
            for (var g = 0;g < this.columns;g++) {
              this.tiles.push(new v(d++, g, c, f, b, e));
            }
          }
        }
        a.prototype.getTiles = function(a, c) {
          if (c.emptyArea(a)) {
            return [];
          }
          if (c.infiniteArea(a)) {
            return this.tiles;
          }
          var f = this.columns * this.rows;
          return 40 > f && c.isScaleOrRotation() ? this.getFewTiles(a, c, 10 < f) : this.getManyTiles(a, c);
        };
        a.prototype.getFewTiles = function(d, c, f) {
          void 0 === f && (f = !0);
          if (c.isTranslationOnly() && 1 === this.tiles.length) {
            return this.tiles[0].bounds.intersectsTranslated(d, c.tx, c.ty) ? [this.tiles[0]] : [];
          }
          c.transformRectangle(d, a._points);
          var b;
          d = new t(0, 0, this.w, this.h);
          f && (b = new q(a._points));
          d.intersect(q.getBounds(a._points));
          if (d.isEmpty()) {
            return [];
          }
          var e = d.x / this.tileW | 0;
          c = d.y / this.tileH | 0;
          var h = Math.ceil((d.x + d.w) / this.tileW) | 0, k = Math.ceil((d.y + d.h) / this.tileH) | 0, e = m(e, 0, this.columns), h = m(h, 0, this.columns);
          c = m(c, 0, this.rows);
          for (var k = m(k, 0, this.rows), n = [];e < h;e++) {
            for (var l = c;l < k;l++) {
              var v = this.tiles[l * this.columns + e];
              v.bounds.intersects(d) && (f ? v.getOBB().intersects(b) : 1) && n.push(v);
            }
          }
          return n;
        };
        a.prototype.getManyTiles = function(d, c) {
          function f(a, f, c) {
            return (a - f.x) * (c.y - f.y) / (c.x - f.x) + f.y;
          }
          function b(a, f, c, d, e) {
            if (!(0 > c || c >= f.columns)) {
              for (d = m(d, 0, f.rows), e = m(e + 1, 0, f.rows);d < e;d++) {
                a.push(f.tiles[d * f.columns + c]);
              }
            }
          }
          var e = a._points;
          c.transformRectangle(d, e);
          for (var h = e[0].x < e[1].x ? 0 : 1, k = e[2].x < e[3].x ? 2 : 3, k = e[h].x < e[k].x ? h : k, h = [], n = 0;5 > n;n++, k++) {
            h.push(e[k % 4]);
          }
          (h[1].x - h[0].x) * (h[3].y - h[0].y) < (h[1].y - h[0].y) * (h[3].x - h[0].x) && (e = h[1], h[1] = h[3], h[3] = e);
          var e = [], q, t, k = Math.floor(h[0].x / this.tileW), n = (k + 1) * this.tileW;
          if (h[2].x < n) {
            q = Math.min(h[0].y, h[1].y, h[2].y, h[3].y);
            t = Math.max(h[0].y, h[1].y, h[2].y, h[3].y);
            var l = Math.floor(q / this.tileH), v = Math.floor(t / this.tileH);
            b(e, this, k, l, v);
            return e;
          }
          var r = 0, p = 4, u = !1;
          if (h[0].x === h[1].x || h[0].x === h[3].x) {
            h[0].x === h[1].x ? (u = !0, r++) : p--, q = f(n, h[r], h[r + 1]), t = f(n, h[p], h[p - 1]), l = Math.floor(h[r].y / this.tileH), v = Math.floor(h[p].y / this.tileH), b(e, this, k, l, v), k++;
          }
          do {
            var w, D, z, B;
            h[r + 1].x < n ? (w = h[r + 1].y, z = !0) : (w = f(n, h[r], h[r + 1]), z = !1);
            h[p - 1].x < n ? (D = h[p - 1].y, B = !0) : (D = f(n, h[p], h[p - 1]), B = !1);
            l = Math.floor((h[r].y < h[r + 1].y ? q : w) / this.tileH);
            v = Math.floor((h[p].y > h[p - 1].y ? t : D) / this.tileH);
            b(e, this, k, l, v);
            if (z && u) {
              break;
            }
            z ? (u = !0, r++, q = f(n, h[r], h[r + 1])) : q = w;
            B ? (p--, t = f(n, h[p], h[p - 1])) : t = D;
            k++;
            n = (k + 1) * this.tileW;
          } while (r < p);
          return e;
        };
        a._points = r.createEmptyPoints(4);
        return a;
      }();
      u.TileCache = e;
      n = function() {
        function a(d, c, f) {
          this._cacheLevels = [];
          this._source = d;
          this._tileSize = c;
          this._minUntiledSize = f;
        }
        a.prototype._getTilesAtScale = function(a, c, f) {
          var g = Math.max(c.getAbsoluteScaleX(), c.getAbsoluteScaleY()), h = 0;
          1 !== g && (h = m(Math.round(Math.log(1 / g) / Math.LN2), -5, 3));
          g = b(h);
          if (this._source.hasFlags(1048576)) {
            for (;;) {
              g = b(h);
              if (f.contains(this._source.getBounds().getAbsoluteBounds().clone().scale(g, g))) {
                break;
              }
              h--;
            }
          }
          this._source.hasFlags(2097152) || (h = m(h, -5, 0));
          g = b(h);
          f = 5 + h;
          h = this._cacheLevels[f];
          if (!h) {
            var h = this._source.getBounds().getAbsoluteBounds().clone().scale(g, g), k, n;
            this._source.hasFlags(1048576) || !this._source.hasFlags(4194304) || Math.max(h.w, h.h) <= this._minUntiledSize ? (k = h.w, n = h.h) : k = n = this._tileSize;
            h = this._cacheLevels[f] = new e(h.w, h.h, k, n, g);
          }
          return h.getTiles(a, c.scaleClone(g, g));
        };
        a.prototype.fetchTiles = function(a, c, f, b) {
          var e = new t(0, 0, f.canvas.width, f.canvas.height);
          a = this._getTilesAtScale(a, c, e);
          var g;
          c = this._source;
          for (var h = 0;h < a.length;h++) {
            var k = a[h];
            k.cachedSurfaceRegion && k.cachedSurfaceRegion.surface && !c.hasFlags(1048592) || (g || (g = []), g.push(k));
          }
          g && this._cacheTiles(f, g, b, e);
          c.removeFlags(16);
          return a;
        };
        a.prototype._getTileBounds = function(a) {
          for (var c = t.createEmpty(), f = 0;f < a.length;f++) {
            c.union(a[f].bounds);
          }
          return c;
        };
        a.prototype._cacheTiles = function(a, c, f, b, e) {
          void 0 === e && (e = 4);
          var g = this._getTileBounds(c);
          a.save();
          a.setTransform(1, 0, 0, 1, 0, 0);
          a.clearRect(0, 0, b.w, b.h);
          a.translate(-g.x, -g.y);
          a.scale(c[0].scale, c[0].scale);
          var h = this._source.getBounds();
          a.translate(-h.x, -h.y);
          2 <= p.traceLevel && p.writer && p.writer.writeLn("Rendering Tiles: " + g);
          this._source.render(a, 0);
          a.restore();
          for (var h = null, k = 0;k < c.length;k++) {
            var n = c[k], q = n.bounds.clone();
            q.x -= g.x;
            q.y -= g.y;
            b.contains(q) || (h || (h = []), h.push(n));
            n.cachedSurfaceRegion = f(n.cachedSurfaceRegion, a, q);
          }
          h && (2 <= h.length ? (c = h.slice(0, h.length / 2 | 0), g = h.slice(c.length), this._cacheTiles(a, c, f, b, e - 1), this._cacheTiles(a, g, f, b, e - 1)) : this._cacheTiles(a, h, f, b, e - 1));
        };
        return a;
      }();
      u.RenderableTileCache = n;
    })(p.Geometry || (p.Geometry = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, p) {
  function u() {
    this.constructor = k;
  }
  for (var a in p) {
    p.hasOwnProperty(a) && (k[a] = p[a]);
  }
  u.prototype = p.prototype;
  k.prototype = new u;
};
(function(k) {
  (function(p) {
    var u = k.IntegerUtilities.roundToMultipleOfPowerOfTwo, a = p.Geometry.Rectangle;
    (function(k) {
      var m = function(a) {
        function b() {
          a.apply(this, arguments);
        }
        __extends(b, a);
        return b;
      }(p.Geometry.Rectangle);
      k.Region = m;
      var b = function() {
        function a(b, e) {
          this._root = new l(0, 0, b | 0, e | 0, !1);
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          var g = this._root.insert(a, b);
          g && (g.allocator = this, g.allocated = !0);
          return g;
        };
        a.prototype.free = function(a) {
          a.clear();
          a.allocated = !1;
        };
        a.RANDOM_ORIENTATION = !0;
        a.MAX_DEPTH = 256;
        return a;
      }();
      k.CompactAllocator = b;
      var l = function(a) {
        function h(b, g, d, c, f) {
          a.call(this, b, g, d, c);
          this._children = null;
          this._horizontal = f;
          this.allocated = !1;
        }
        __extends(h, a);
        h.prototype.clear = function() {
          this._children = null;
          this.allocated = !1;
        };
        h.prototype.insert = function(a, b) {
          return this._insert(a, b, 0);
        };
        h.prototype._insert = function(a, g, d) {
          if (!(d > b.MAX_DEPTH || this.allocated || this.w < a || this.h < g)) {
            if (this._children) {
              var c;
              if ((c = this._children[0]._insert(a, g, d + 1)) || (c = this._children[1]._insert(a, g, d + 1))) {
                return c;
              }
            } else {
              return c = !this._horizontal, b.RANDOM_ORIENTATION && (c = .5 <= Math.random()), this._children = this._horizontal ? [new h(this.x, this.y, this.w, g, !1), new h(this.x, this.y + g, this.w, this.h - g, c)] : [new h(this.x, this.y, a, this.h, !0), new h(this.x + a, this.y, this.w - a, this.h, c)], c = this._children[0], c.w === a && c.h === g ? (c.allocated = !0, c) : this._insert(a, g, d + 1);
            }
          }
        };
        return h;
      }(k.Region), r = function() {
        function a(b, e, g, d) {
          this._columns = b / g | 0;
          this._rows = e / d | 0;
          this._sizeW = g;
          this._sizeH = d;
          this._freeList = [];
          this._index = 0;
          this._total = this._columns * this._rows;
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          var g = this._sizeW, d = this._sizeH;
          if (a > g || b > d) {
            return null;
          }
          var c = this._freeList, f = this._index;
          return 0 < c.length ? (g = c.pop(), g.w = a, g.h = b, g.allocated = !0, g) : f < this._total ? (c = f / this._columns | 0, g = new h((f - c * this._columns) * g, c * d, a, b), g.index = f, g.allocator = this, g.allocated = !0, this._index++, g) : null;
        };
        a.prototype.free = function(a) {
          a.allocated = !1;
          this._freeList.push(a);
        };
        return a;
      }();
      k.GridAllocator = r;
      var h = function(a) {
        function b(e, g, d, c) {
          a.call(this, e, g, d, c);
          this.index = -1;
        }
        __extends(b, a);
        return b;
      }(k.Region);
      k.GridCell = h;
      var t = function() {
        return function(a, b, e) {
          this.size = a;
          this.region = b;
          this.allocator = e;
        };
      }(), q = function(a) {
        function b(e, g, d, c, f) {
          a.call(this, e, g, d, c);
          this.region = f;
        }
        __extends(b, a);
        return b;
      }(k.Region);
      k.BucketCell = q;
      m = function() {
        function b(a, e) {
          this._buckets = [];
          this._w = a | 0;
          this._h = e | 0;
          this._filled = 0;
        }
        b.prototype.allocate = function(b, e) {
          b = Math.ceil(b);
          e = Math.ceil(e);
          var g = Math.max(b, e);
          if (b > this._w || e > this._h) {
            return null;
          }
          var d = null, c, f = this._buckets;
          do {
            for (var h = 0;h < f.length && !(f[h].size >= g && (c = f[h], d = c.allocator.allocate(b, e)));h++) {
            }
            if (!d) {
              var k = this._h - this._filled;
              if (k < e) {
                return null;
              }
              var h = u(g, 8), n = 2 * h;
              n > k && (n = k);
              if (n < h) {
                return null;
              }
              k = new a(0, this._filled, this._w, n);
              this._buckets.push(new t(h, k, new r(k.w, k.h, h, h)));
              this._filled += n;
            }
          } while (!d);
          return new q(c.region.x + d.x, c.region.y + d.y, d.w, d.h, d);
        };
        b.prototype.free = function(a) {
          a.region.allocator.free(a.region);
        };
        return b;
      }();
      k.BucketAllocator = m;
    })(p.RegionAllocator || (p.RegionAllocator = {}));
    (function(a) {
      var k = function() {
        function a(b) {
          this._createSurface = b;
          this._surfaces = [];
        }
        Object.defineProperty(a.prototype, "surfaces", {get:function() {
          return this._surfaces;
        }, enumerable:!0, configurable:!0});
        a.prototype._createNewSurface = function(a, b) {
          var h = this._createSurface(a, b);
          this._surfaces.push(h);
          return h;
        };
        a.prototype.addSurface = function(a) {
          this._surfaces.push(a);
        };
        a.prototype.allocate = function(a, b, h) {
          for (var k = 0;k < this._surfaces.length;k++) {
            var q = this._surfaces[k];
            if (q !== h && (q = q.allocate(a, b))) {
              return q;
            }
          }
          return this._createNewSurface(a, b).allocate(a, b);
        };
        a.prototype.free = function(a) {
        };
        return a;
      }();
      a.SimpleAllocator = k;
    })(p.SurfaceRegionAllocator || (p.SurfaceRegionAllocator = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var u = p.Geometry.Rectangle, a = p.Geometry.Matrix, w = p.Geometry.DirtyRegion;
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
    })(p.BlendMode || (p.BlendMode = {}));
    var m = p.BlendMode;
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
    })(p.NodeFlags || (p.NodeFlags = {}));
    var b = p.NodeFlags;
    (function(a) {
      a[a.Node = 1] = "Node";
      a[a.Shape = 3] = "Shape";
      a[a.Group = 5] = "Group";
      a[a.Stage = 13] = "Stage";
      a[a.Renderable = 33] = "Renderable";
    })(p.NodeType || (p.NodeType = {}));
    var l = p.NodeType;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.OnStageBoundsChanged = 1] = "OnStageBoundsChanged";
      a[a.RemovedFromStage = 2] = "RemovedFromStage";
    })(p.NodeEventType || (p.NodeEventType = {}));
    var r = function() {
      function a() {
      }
      a.prototype.visitNode = function(a, f) {
      };
      a.prototype.visitShape = function(a, f) {
        this.visitNode(a, f);
      };
      a.prototype.visitGroup = function(a, f) {
        this.visitNode(a, f);
        for (var b = a.getChildren(), d = 0;d < b.length;d++) {
          b[d].visit(this, f);
        }
      };
      a.prototype.visitStage = function(a, f) {
        this.visitGroup(a, f);
      };
      a.prototype.visitRenderable = function(a, f) {
        this.visitNode(a, f);
      };
      return a;
    }();
    p.NodeVisitor = r;
    var h = function() {
      return function() {
      };
    }();
    p.State = h;
    var t = function(b) {
      function c() {
        b.call(this);
        this.matrix = a.createIdentity();
        this.depth = 0;
      }
      __extends(c, b);
      c.prototype.transform = function(a) {
        var c = this.clone();
        c.matrix.preMultiply(a.getMatrix());
        return c;
      };
      c.allocate = function() {
        var a = c._dirtyStack, b = null;
        a.length && (b = a.pop());
        return b;
      };
      c.prototype.clone = function() {
        var a = c.allocate();
        a || (a = new c);
        a.set(this);
        return a;
      };
      c.prototype.set = function(a) {
        this.matrix.set(a.matrix);
      };
      c.prototype.free = function() {
        c._dirtyStack.push(this);
      };
      c._dirtyStack = [];
      return c;
    }(h);
    p.PreRenderState = t;
    var q = function(a) {
      function c() {
        a.apply(this, arguments);
        this.isDirty = !0;
      }
      __extends(c, a);
      c.prototype.start = function(a, c) {
        this._dirtyRegion = c;
        var b = new t;
        b.matrix.setIdentity();
        a.visit(this, b);
        b.free();
      };
      c.prototype.visitGroup = function(a, c) {
        var b = a.getChildren();
        this.visitNode(a, c);
        for (var d = 0;d < b.length;d++) {
          var e = b[d], g = c.transform(e.getTransform());
          e.visit(this, g);
          g.free();
        }
      };
      c.prototype.visitNode = function(a, c) {
        a.hasFlags(16) && (this.isDirty = !0);
        a.toggleFlags(16, !1);
        a.depth = c.depth++;
      };
      return c;
    }(r);
    p.PreRenderVisitor = q;
    h = function(a) {
      function c(f) {
        a.call(this);
        this.writer = f;
      }
      __extends(c, a);
      c.prototype.visitNode = function(a, c) {
      };
      c.prototype.visitShape = function(a, c) {
        this.writer.writeLn(a.toString());
        this.visitNode(a, c);
      };
      c.prototype.visitGroup = function(a, c) {
        this.visitNode(a, c);
        var b = a.getChildren();
        this.writer.enter(a.toString() + " " + b.length);
        for (var d = 0;d < b.length;d++) {
          b[d].visit(this, c);
        }
        this.writer.outdent();
      };
      c.prototype.visitStage = function(a, c) {
        this.visitGroup(a, c);
      };
      return c;
    }(r);
    p.TracingNodeVisitor = h;
    var n = function() {
      function a() {
        this._clip = -1;
        this._eventListeners = null;
        this._id = a._nextId++;
        this._type = 1;
        this._index = -1;
        this._parent = null;
        this.reset();
      }
      Object.defineProperty(a.prototype, "id", {get:function() {
        return this._id;
      }, enumerable:!0, configurable:!0});
      a.prototype._dispatchEvent = function(a) {
        if (this._eventListeners) {
          for (var f = this._eventListeners, b = 0;b < f.length;b++) {
            var d = f[b];
            d.type === a && d.listener(this, a);
          }
        }
      };
      a.prototype.addEventListener = function(a, f) {
        this._eventListeners || (this._eventListeners = []);
        this._eventListeners.push({type:a, listener:f});
      };
      a.prototype.removeEventListener = function(a, f) {
        for (var b = this._eventListeners, d = 0;d < b.length;d++) {
          var e = b[d];
          if (e.type === a && e.listener === f) {
            b.splice(d, 1);
            break;
          }
        }
      };
      Object.defineProperty(a.prototype, "properties", {get:function() {
        return this._properties || (this._properties = {});
      }, enumerable:!0, configurable:!0});
      a.prototype.reset = function() {
        this._flags = b.Default;
        this._properties = this._transform = this._layer = this._bounds = null;
        this.depth = -1;
      };
      Object.defineProperty(a.prototype, "clip", {get:function() {
        return this._clip;
      }, set:function(a) {
        this._clip = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "parent", {get:function() {
        return this._parent;
      }, enumerable:!0, configurable:!0});
      a.prototype.getTransformedBounds = function(a) {
        var f = this.getBounds(!0);
        if (a !== this && !f.isEmpty()) {
          var b = this.getTransform().getConcatenatedMatrix();
          a ? (a = a.getTransform().getInvertedConcatenatedMatrix(!0), a.preMultiply(b), a.transformRectangleAABB(f), a.free()) : b.transformRectangleAABB(f);
        }
        return f;
      };
      a.prototype._markCurrentBoundsAsDirtyRegion = function() {
      };
      a.prototype.getStage = function(a) {
        void 0 === a && (a = !0);
        for (var f = this._parent;f;) {
          if (f.isType(13)) {
            var b = f;
            if (a) {
              if (b.dirtyRegion) {
                return b;
              }
            } else {
              return b;
            }
          }
          f = f._parent;
        }
        return null;
      };
      a.prototype.getChildren = function(a) {
        throw void 0;
      };
      a.prototype.getBounds = function(a) {
        throw void 0;
      };
      a.prototype.setBounds = function(a) {
        (this._bounds || (this._bounds = u.createEmpty())).set(a);
        this.removeFlags(256);
      };
      a.prototype.clone = function() {
        throw void 0;
      };
      a.prototype.setFlags = function(a) {
        this._flags |= a;
      };
      a.prototype.hasFlags = function(a) {
        return (this._flags & a) === a;
      };
      a.prototype.hasAnyFlags = function(a) {
        return !!(this._flags & a);
      };
      a.prototype.removeFlags = function(a) {
        this._flags &= ~a;
      };
      a.prototype.toggleFlags = function(a, f) {
        this._flags = f ? this._flags | a : this._flags & ~a;
      };
      a.prototype._propagateFlagsUp = function(a) {
        if (0 !== a && !this.hasFlags(a)) {
          this.hasFlags(2) || (a &= -257);
          this.setFlags(a);
          var f = this._parent;
          f && f._propagateFlagsUp(a);
        }
      };
      a.prototype._propagateFlagsDown = function(a) {
        throw void 0;
      };
      a.prototype.isAncestor = function(a) {
        for (;a;) {
          if (a === this) {
            return !0;
          }
          a = a._parent;
        }
        return !1;
      };
      a._getAncestors = function(c, f) {
        var b = a._path;
        for (b.length = 0;c && c !== f;) {
          b.push(c), c = c._parent;
        }
        return b;
      };
      a.prototype._findClosestAncestor = function(a, f) {
        for (var b = this;b;) {
          if (b.hasFlags(a) === f) {
            return b;
          }
          b = b._parent;
        }
        return null;
      };
      a.prototype.isType = function(a) {
        return this._type === a;
      };
      a.prototype.isTypeOf = function(a) {
        return (this._type & a) === a;
      };
      a.prototype.isLeaf = function() {
        return this.isType(33) || this.isType(3);
      };
      a.prototype.isLinear = function() {
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
      a.prototype.getTransformMatrix = function(a) {
        void 0 === a && (a = !1);
        return this.getTransform().getMatrix(a);
      };
      a.prototype.getTransform = function() {
        null === this._transform && (this._transform = new e(this));
        return this._transform;
      };
      a.prototype.getLayer = function() {
        null === this._layer && (this._layer = new g(this));
        return this._layer;
      };
      a.prototype.getLayerBounds = function(a) {
        var f = this.getBounds();
        a && this._layer && this._layer.expandBounds(f);
        return f;
      };
      a.prototype.visit = function(a, f) {
        switch(this._type) {
          case 1:
            a.visitNode(this, f);
            break;
          case 5:
            a.visitGroup(this, f);
            break;
          case 13:
            a.visitStage(this, f);
            break;
          case 3:
            a.visitShape(this, f);
            break;
          case 33:
            a.visitRenderable(this, f);
            break;
          default:
            k.Debug.unexpectedCase();
        }
      };
      a.prototype.invalidate = function() {
        this._propagateFlagsUp(b.UpOnInvalidate);
      };
      a.prototype.toString = function(a) {
        void 0 === a && (a = !1);
        var f = l[this._type] + " " + this._id;
        a && (f += " " + this._bounds.toString());
        return f;
      };
      a._path = [];
      a._nextId = 0;
      return a;
    }();
    p.Node = n;
    var v = function(a) {
      function c() {
        a.call(this);
        this._type = 5;
        this._children = [];
      }
      __extends(c, a);
      c.prototype.getChildren = function(a) {
        void 0 === a && (a = !1);
        return a ? this._children.slice(0) : this._children;
      };
      c.prototype.childAt = function(a) {
        return this._children[a];
      };
      Object.defineProperty(c.prototype, "child", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "groupChild", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      c.prototype.addChild = function(a) {
        a._parent && a._parent.removeChildAt(a._index);
        a._parent = this;
        a._index = this._children.length;
        this._children.push(a);
        this._propagateFlagsUp(b.UpOnAddedOrRemoved);
        a._propagateFlagsDown(b.DownOnAddedOrRemoved);
      };
      c.prototype.removeChildAt = function(a) {
        var c = this._children[a];
        this._children.splice(a, 1);
        c._index = -1;
        c._parent = null;
        this._propagateFlagsUp(b.UpOnAddedOrRemoved);
        c._propagateFlagsDown(b.DownOnAddedOrRemoved);
      };
      c.prototype.clearChildren = function() {
        for (var a = 0;a < this._children.length;a++) {
          var c = this._children[a];
          c && (c._index = -1, c._parent = null, c._propagateFlagsDown(b.DownOnAddedOrRemoved));
        }
        this._children.length = 0;
        this._propagateFlagsUp(b.UpOnAddedOrRemoved);
      };
      c.prototype._propagateFlagsDown = function(a) {
        if (!this.hasFlags(a)) {
          this.setFlags(a);
          for (var c = this._children, b = 0;b < c.length;b++) {
            c[b]._propagateFlagsDown(a);
          }
        }
      };
      c.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var c = this._bounds || (this._bounds = u.createEmpty());
        if (this.hasFlags(256)) {
          c.setEmpty();
          for (var b = this._children, d = u.allocate(), e = 0;e < b.length;e++) {
            var g = b[e];
            d.set(g.getBounds());
            g.getTransformMatrix().transformRectangleAABB(d);
            c.union(d);
          }
          d.free();
          this.removeFlags(256);
        }
        return a ? c.clone() : c;
      };
      c.prototype.getLayerBounds = function(a) {
        if (!a) {
          return this.getBounds();
        }
        for (var c = u.createEmpty(), b = this._children, d = u.allocate(), e = 0;e < b.length;e++) {
          var g = b[e];
          d.set(g.getLayerBounds(a));
          g.getTransformMatrix().transformRectangleAABB(d);
          c.union(d);
        }
        d.free();
        a && this._layer && this._layer.expandBounds(c);
        return c;
      };
      return c;
    }(n);
    p.Group = v;
    var e = function() {
      function d(c) {
        this._node = c;
        this._matrix = a.createIdentity();
        this._colorMatrix = p.ColorMatrix.createIdentity();
        this._concatenatedMatrix = a.createIdentity();
        this._invertedConcatenatedMatrix = a.createIdentity();
        this._concatenatedColorMatrix = p.ColorMatrix.createIdentity();
      }
      d.prototype.setMatrix = function(a) {
        this._matrix.isEqual(a) || (this._matrix.set(a), this._node._propagateFlagsUp(b.UpOnMoved), this._node._propagateFlagsDown(b.DownOnMoved));
      };
      d.prototype.setColorMatrix = function(a) {
        this._colorMatrix.set(a);
        this._node._propagateFlagsUp(b.UpOnColorMatrixChanged);
        this._node._propagateFlagsDown(b.DownOnColorMatrixChanged);
      };
      d.prototype.getMatrix = function(a) {
        void 0 === a && (a = !1);
        return a ? this._matrix.clone() : this._matrix;
      };
      d.prototype.hasColorMatrix = function() {
        return null !== this._colorMatrix;
      };
      d.prototype.getColorMatrix = function(a) {
        void 0 === a && (a = !1);
        null === this._colorMatrix && (this._colorMatrix = p.ColorMatrix.createIdentity());
        return a ? this._colorMatrix.clone() : this._colorMatrix;
      };
      d.prototype.getConcatenatedMatrix = function(c) {
        void 0 === c && (c = !1);
        if (this._node.hasFlags(512)) {
          for (var f = this._node._findClosestAncestor(512, !1), b = n._getAncestors(this._node, f), d = f ? f.getTransform()._concatenatedMatrix.clone() : a.createIdentity(), e = b.length - 1;0 <= e;e--) {
            var f = b[e], g = f.getTransform();
            d.preMultiply(g._matrix);
            g._concatenatedMatrix.set(d);
            f.removeFlags(512);
          }
        }
        return c ? this._concatenatedMatrix.clone() : this._concatenatedMatrix;
      };
      d.prototype.getInvertedConcatenatedMatrix = function(a) {
        void 0 === a && (a = !1);
        this._node.hasFlags(1024) && (this.getConcatenatedMatrix().inverse(this._invertedConcatenatedMatrix), this._node.removeFlags(1024));
        return a ? this._invertedConcatenatedMatrix.clone() : this._invertedConcatenatedMatrix;
      };
      d.prototype.toString = function() {
        return this._matrix.toString();
      };
      return d;
    }();
    p.Transform = e;
    var g = function() {
      function a(c) {
        this._node = c;
        this._mask = null;
        this._blendMode = 1;
      }
      Object.defineProperty(a.prototype, "filters", {get:function() {
        return this._filters;
      }, set:function(a) {
        this._filters = a;
        a.length && this._node.invalidate();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "blendMode", {get:function() {
        return this._blendMode;
      }, set:function(a) {
        this._blendMode = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "mask", {get:function() {
        return this._mask;
      }, set:function(a) {
        this._mask !== a && (this._node.invalidate(), this._mask && this._mask.removeFlags(4));
        (this._mask = a) && this._mask.setFlags(4);
      }, enumerable:!0, configurable:!0});
      a.prototype.toString = function() {
        return m[this._blendMode];
      };
      a.prototype.expandBounds = function(a) {
        var f = this._filters;
        if (f) {
          for (var b = 0;b < f.length;b++) {
            f[b].expandBounds(a);
          }
        }
      };
      return a;
    }();
    p.Layer = g;
    h = function(a) {
      function b(f) {
        a.call(this);
        this._source = f;
        this._type = 3;
        this._ratio = 0;
      }
      __extends(b, a);
      b.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var b = this._bounds || (this._bounds = u.createEmpty());
        this.hasFlags(256) && (b.set(this._source.getBounds()), this.removeFlags(256));
        return a ? b.clone() : b;
      };
      Object.defineProperty(b.prototype, "source", {get:function() {
        return this._source;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "ratio", {get:function() {
        return this._ratio;
      }, set:function(a) {
        a !== this._ratio && (this.invalidate(), this._ratio = a);
      }, enumerable:!0, configurable:!0});
      b.prototype._propagateFlagsDown = function(a) {
        this.setFlags(a);
      };
      b.prototype.getChildren = function(a) {
        return [this._source];
      };
      return b;
    }(n);
    p.Shape = h;
    h = function() {
      return function() {
        this.debug = !1;
        this.paintRenderable = !0;
        this.paintViewport = this.paintFlashing = this.paintDirtyRegion = this.paintBounds = !1;
        this.clear = !0;
      };
    }();
    p.RendererOptions = h;
    (function(a) {
      a[a.Canvas2D = 0] = "Canvas2D";
      a[a.WebGL = 1] = "WebGL";
      a[a.Both = 2] = "Both";
      a[a.DOM = 3] = "DOM";
      a[a.SVG = 4] = "SVG";
    })(p.Backend || (p.Backend = {}));
    r = function(a) {
      function b(f, c, e) {
        a.call(this);
        this._container = f;
        this._stage = c;
        this._options = e;
        this._viewport = u.createSquare(1024);
        this._devicePixelRatio = 1;
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "viewport", {set:function(a) {
        this._viewport.set(a);
      }, enumerable:!0, configurable:!0});
      b.prototype.render = function() {
        throw void 0;
      };
      b.prototype.resize = function() {
        throw void 0;
      };
      b.prototype.screenShot = function(a, b, c) {
        throw void 0;
      };
      return b;
    }(r);
    p.Renderer = r;
    r = function(b) {
      function c(a, e, g) {
        void 0 === g && (g = !1);
        b.call(this);
        this._preVisitor = new q;
        this._flags &= -3;
        this._type = 13;
        this._scaleMode = c.DEFAULT_SCALE;
        this._align = c.DEFAULT_ALIGN;
        this._content = new v;
        this._content._flags &= -3;
        this.addChild(this._content);
        this.setFlags(16);
        this.setBounds(new u(0, 0, a, e));
        g ? (this._dirtyRegion = new w(a, e), this._dirtyRegion.addDirtyRectangle(new u(0, 0, a, e))) : this._dirtyRegion = null;
        this._updateContentMatrix();
      }
      __extends(c, b);
      Object.defineProperty(c.prototype, "dirtyRegion", {get:function() {
        return this._dirtyRegion;
      }, enumerable:!0, configurable:!0});
      c.prototype.setBounds = function(a) {
        b.prototype.setBounds.call(this, a);
        this._updateContentMatrix();
        this._dispatchEvent(1);
        this._dirtyRegion && (this._dirtyRegion = new w(a.w, a.h), this._dirtyRegion.addDirtyRectangle(a));
      };
      Object.defineProperty(c.prototype, "content", {get:function() {
        return this._content;
      }, enumerable:!0, configurable:!0});
      c.prototype.readyToRender = function() {
        this._preVisitor.isDirty = !1;
        this._preVisitor.start(this, this._dirtyRegion);
        return this._preVisitor.isDirty ? !0 : !1;
      };
      Object.defineProperty(c.prototype, "align", {get:function() {
        return this._align;
      }, set:function(a) {
        this._align = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "scaleMode", {get:function() {
        return this._scaleMode;
      }, set:function(a) {
        this._scaleMode = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      c.prototype._updateContentMatrix = function() {
        if (this._scaleMode === c.DEFAULT_SCALE && this._align === c.DEFAULT_ALIGN) {
          this._content.getTransform().setMatrix(new a(1, 0, 0, 1, 0, 0));
        } else {
          var f = this.getBounds(), b = this._content.getBounds(), d = f.w / b.w, e = f.h / b.h;
          switch(this._scaleMode) {
            case 2:
              d = e = Math.max(d, e);
              break;
            case 4:
              d = e = 1;
              break;
            case 1:
              break;
            default:
              d = e = Math.min(d, e);
          }
          var g;
          g = this._align & 4 ? 0 : this._align & 8 ? f.w - b.w * d : (f.w - b.w * d) / 2;
          f = this._align & 1 ? 0 : this._align & 2 ? f.h - b.h * e : (f.h - b.h * e) / 2;
          this._content.getTransform().setMatrix(new a(d, 0, 0, e, g, f));
        }
      };
      c.DEFAULT_SCALE = 4;
      c.DEFAULT_ALIGN = 5;
      return c;
    }(v);
    p.Stage = r;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    function u(a, b, f) {
      return a + (b - a) * f;
    }
    function a(a, b, f) {
      return u(a >> 24 & 255, b >> 24 & 255, f) << 24 | u(a >> 16 & 255, b >> 16 & 255, f) << 16 | u(a >> 8 & 255, b >> 8 & 255, f) << 8 | u(a & 255, b & 255, f);
    }
    function w(a, b, f) {
      a = a.measureText(b).width | 0;
      0 < f && (a += b.length * f);
      return a;
    }
    var m = p.Geometry.Point, b = p.Geometry.Rectangle, l = p.Geometry.Matrix, r = k.ArrayUtilities.indexOf, h = function(a) {
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
      b.prototype.addParent = function(a) {
        r(this._parents, a);
        this._parents.push(a);
      };
      b.prototype.willRender = function() {
        for (var a = this._parents, b = 0;b < a.length;b++) {
          for (var c = a[b];c;) {
            if (c.isType(13)) {
              return !0;
            }
            if (!c.hasFlags(65536)) {
              break;
            }
            c = c._parent;
          }
        }
        return !1;
      };
      b.prototype.addRenderableParent = function(a) {
        this._renderableParents.push(a);
      };
      b.prototype.wrap = function() {
        for (var a, b = this._parents, c = 0;c < b.length;c++) {
          if (a = b[c], !a._parent) {
            return a;
          }
        }
        a = new p.Shape(this);
        this.addParent(a);
        return a;
      };
      b.prototype.invalidate = function() {
        this.setFlags(16);
        for (var a = this._parents, b = 0;b < a.length;b++) {
          a[b].invalidate();
        }
        a = this._renderableParents;
        for (b = 0;b < a.length;b++) {
          a[b].invalidate();
        }
        if (a = this._invalidateEventListeners) {
          for (b = 0;b < a.length;b++) {
            a[b](this);
          }
        }
      };
      b.prototype.addInvalidateEventListener = function(a) {
        this._invalidateEventListeners || (this._invalidateEventListeners = []);
        r(this._invalidateEventListeners, a);
        this._invalidateEventListeners.push(a);
      };
      b.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        return a ? this._bounds.clone() : this._bounds;
      };
      b.prototype.getChildren = function(a) {
        return null;
      };
      b.prototype._propagateFlagsUp = function(a) {
        if (0 !== a && !this.hasFlags(a)) {
          for (var b = 0;b < this._parents.length;b++) {
            this._parents[b]._propagateFlagsUp(a);
          }
        }
      };
      b.prototype.render = function(a, b, c, d, e) {
      };
      return b;
    }(p.Node);
    p.Renderable = h;
    var t = function(a) {
      function b(f, c) {
        a.call(this);
        this.setBounds(f);
        this.render = c;
      }
      __extends(b, a);
      return b;
    }(h);
    p.CustomRenderable = t;
    (function(a) {
      a[a.Idle = 1] = "Idle";
      a[a.Playing = 2] = "Playing";
      a[a.Paused = 3] = "Paused";
      a[a.Ended = 4] = "Ended";
    })(p.RenderableVideoState || (p.RenderableVideoState = {}));
    t = function(a) {
      function c(f, e) {
        a.call(this);
        this._flags = 1048592;
        this._lastPausedTime = this._lastTimeInvalidated = 0;
        this._pauseHappening = this._seekHappening = !1;
        this._isDOMElement = !0;
        this.setBounds(new b(0, 0, 1, 1));
        this._assetId = f;
        this._eventSerializer = e;
        var g = document.createElement("video"), h = this._handleVideoEvent.bind(this);
        g.preload = "metadata";
        g.addEventListener("play", h);
        g.addEventListener("pause", h);
        g.addEventListener("ended", h);
        g.addEventListener("loadeddata", h);
        g.addEventListener("progress", h);
        g.addEventListener("suspend", h);
        g.addEventListener("loadedmetadata", h);
        g.addEventListener("error", h);
        g.addEventListener("seeking", h);
        g.addEventListener("seeked", h);
        g.addEventListener("canplay", h);
        g.style.position = "absolute";
        this._video = g;
        this._videoEventHandler = h;
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
        var b = null, c = this._video;
        switch(a.type) {
          case "play":
            if (!this._pauseHappening) {
              return;
            }
            a = 7;
            break;
          case "pause":
            if (2 === this._state) {
              c.play();
              return;
            }
            a = 6;
            this._pauseHappening = !0;
            break;
          case "ended":
            this._state = 4;
            this._notifyNetStream(3, b);
            a = 4;
            break;
          case "loadeddata":
            this._pauseHappening = !1;
            this._notifyNetStream(2, b);
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
            b = {videoWidth:c.videoWidth, videoHeight:c.videoHeight, duration:c.duration};
            break;
          case "error":
            a = 11;
            b = {code:c.error.code};
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
        this._notifyNetStream(a, b);
      };
      c.prototype._notifyNetStream = function(a, b) {
        this._eventSerializer.sendVideoPlaybackEvent(this._assetId, a, b);
      };
      c.prototype.processControlRequest = function(a, b) {
        var c = this._video;
        switch(a) {
          case 1:
            c.src = b.url;
            this.play();
            this._notifyNetStream(0, null);
            break;
          case 9:
            c.paused && c.play();
            break;
          case 2:
            c && (b.paused && !c.paused ? (isNaN(b.time) ? this._lastPausedTime = c.currentTime : (0 !== c.seekable.length && (c.currentTime = b.time), this._lastPausedTime = b.time), this.pause()) : !b.paused && c.paused && (this.play(), isNaN(b.time) || this._lastPausedTime === b.time || 0 === c.seekable.length || (c.currentTime = b.time)));
            break;
          case 3:
            c && 0 !== c.seekable.length && (this._seekHappening = !0, c.currentTime = b.time);
            break;
          case 4:
            return c ? c.currentTime : 0;
          case 5:
            return c ? c.duration : 0;
          case 6:
            c && (c.volume = b.volume);
            break;
          case 7:
            if (!c) {
              return 0;
            }
            var d = -1;
            if (c.buffered) {
              for (var e = 0;e < c.buffered.length;e++) {
                d = Math.max(d, c.buffered.end(e));
              }
            } else {
              d = c.duration;
            }
            return Math.round(500 * d);
          case 8:
            return c ? Math.round(500 * c.duration) : 0;
        }
      };
      c.prototype.checkForUpdate = function() {
        this._lastTimeInvalidated !== this._video.currentTime && (this._isDOMElement || this.invalidate());
        this._lastTimeInvalidated = this._video.currentTime;
      };
      c.checkForVideoUpdates = function() {
        for (var a = c._renderableVideos, b = 0;b < a.length;b++) {
          var d = a[b];
          d.willRender() ? (d._video.parentElement || d.invalidate(), d._video.style.zIndex = d.parents[0].depth + "") : d._video.parentElement && d._dispatchEvent(2);
          a[b].checkForUpdate();
        }
      };
      c.prototype.render = function(a, b, c) {
        (b = this._video) && 0 < b.videoWidth && a.drawImage(b, 0, 0, b.videoWidth, b.videoHeight, 0, 0, this._bounds.w, this._bounds.h);
      };
      c._renderableVideos = [];
      return c;
    }(h);
    p.RenderableVideo = t;
    t = function(a) {
      function c(b, c) {
        a.call(this);
        this._flags = 1048592;
        this.properties = {};
        this.setBounds(c);
        b instanceof HTMLCanvasElement ? this._initializeSourceCanvas(b) : this._sourceImage = b;
      }
      __extends(c, a);
      c.FromDataBuffer = function(a, b, d) {
        var e = document.createElement("canvas");
        e.width = d.w;
        e.height = d.h;
        d = new c(e, d);
        d.updateFromDataBuffer(a, b);
        return d;
      };
      c.FromNode = function(a, b, d, e, g) {
        var h = document.createElement("canvas"), k = a.getBounds();
        h.width = k.w;
        h.height = k.h;
        h = new c(h, k);
        h.drawNode(a, b, d, e, g);
        return h;
      };
      c.FromImage = function(a, d, e) {
        return new c(a, new b(0, 0, d, e));
      };
      c.prototype.updateFromDataBuffer = function(a, b) {
        if (p.imageUpdateOption.value) {
          var c = b.buffer;
          if (4 !== a && 5 !== a && 6 !== a) {
            var d = this._bounds, e = this._imageData;
            e && e.width === d.w && e.height === d.h || (e = this._imageData = this._context.createImageData(d.w, d.h));
            p.imageConvertOption.value && (c = new Int32Array(c), d = new Int32Array(e.data.buffer), k.ColorUtilities.convertImage(a, 3, c, d));
            this._ensureSourceCanvas();
            this._context.putImageData(e, 0, 0);
          }
          this.invalidate();
        }
      };
      c.prototype.readImageData = function(a) {
        a.writeRawBytes(this.imageData.data);
      };
      c.prototype.render = function(a, b, c) {
        this.renderSource ? a.drawImage(this.renderSource, 0, 0) : this._renderFallback(a);
      };
      c.prototype.drawNode = function(a, b, c, d, e) {
        c = p.Canvas2D;
        d = this.getBounds();
        (new c.Canvas2DRenderer(this._canvas, null)).renderNode(a, e || d, b);
      };
      c.prototype.mask = function(a) {
        for (var b = this.imageData, c = new Int32Array(b.data.buffer), d = k.ColorUtilities.getUnpremultiplyTable(), e = 0;e < a.length;e++) {
          var g = a[e];
          if (0 === g) {
            c[e] = 0;
          } else {
            if (255 !== g) {
              var h = c[e], n = h >> 0 & 255, q = h >> 8 & 255, h = h >> 16 & 255, t = g << 8, n = d[t + Math.min(n, g)], q = d[t + Math.min(q, g)], h = d[t + Math.min(h, g)];
              c[e] = g << 24 | h << 16 | q << 8 | n;
            }
          }
        }
        this._context.putImageData(b, 0, 0);
      };
      c.prototype._initializeSourceCanvas = function(a) {
        this._canvas = a;
        this._context = this._canvas.getContext("2d");
      };
      c.prototype._ensureSourceCanvas = function() {
        if (!this._canvas) {
          var a = document.createElement("canvas"), b = this._bounds;
          a.width = b.w;
          a.height = b.h;
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
    }(h);
    p.RenderableBitmap = t;
    (function(a) {
      a[a.Fill = 0] = "Fill";
      a[a.Stroke = 1] = "Stroke";
      a[a.StrokeFill = 2] = "StrokeFill";
    })(p.PathType || (p.PathType = {}));
    var q = function() {
      return function(a, b, f, e) {
        this.type = a;
        this.style = b;
        this.smoothImage = f;
        this.strokeProperties = e;
        this.path = new Path2D;
      };
    }();
    p.StyledPath = q;
    var n = function() {
      return function(a, b, f, e, g) {
        this.thickness = a;
        this.scaleMode = b;
        this.capsStyle = f;
        this.jointsStyle = e;
        this.miterLimit = g;
      };
    }();
    p.StrokeProperties = n;
    var v = function(a) {
      function b(c, e, g, h) {
        a.call(this);
        this._flags = 6291472;
        this.properties = {};
        this.setBounds(h);
        this._id = c;
        this._pathData = e;
        this._textures = g;
        g.length && this.setFlags(1048576);
      }
      __extends(b, a);
      b.prototype.update = function(a, b, c) {
        this.setBounds(c);
        this._pathData = a;
        this._paths = null;
        this._textures = b;
        this.setFlags(1048576);
        this.invalidate();
      };
      b.prototype.render = function(a, b, c, d, e) {
        void 0 === d && (d = null);
        void 0 === e && (e = !1);
        a.fillStyle = a.strokeStyle = "transparent";
        b = this._deserializePaths(this._pathData, a, b);
        for (c = 0;c < b.length;c++) {
          var g = b[c];
          a.mozImageSmoothingEnabled = a.msImageSmoothingEnabled = a.imageSmoothingEnabled = g.smoothImage;
          if (0 === g.type) {
            d ? d.addPath(g.path, a.currentTransform) : (a.fillStyle = e ? "#000000" : g.style, a.fill(g.path, "evenodd"), a.fillStyle = "transparent");
          } else {
            if (!d && !e) {
              a.strokeStyle = g.style;
              var h = 1;
              g.strokeProperties && (h = g.strokeProperties.scaleMode, a.lineWidth = g.strokeProperties.thickness, a.lineCap = g.strokeProperties.capsStyle, a.lineJoin = g.strokeProperties.jointsStyle, a.miterLimit = g.strokeProperties.miterLimit);
              var k = a.lineWidth;
              (k = 1 === k || 3 === k) && a.translate(.5, .5);
              a.flashStroke(g.path, h);
              k && a.translate(-.5, -.5);
              a.strokeStyle = "transparent";
            }
          }
        }
      };
      b.prototype._deserializePaths = function(a, d, e) {
        if (this._paths) {
          return this._paths;
        }
        e = this._paths = [];
        var g = null, h = null, q = 0, t = 0, l, v, r = !1, m = 0, p = 0, u = a.commands, w = a.coordinates, D = a.styles, z = D.position = 0;
        a = a.commandsPosition;
        for (var B = 0;B < a;B++) {
          switch(u[B]) {
            case 9:
              r && g && (g.lineTo(m, p), h && h.lineTo(m, p));
              r = !0;
              q = m = w[z++] / 20;
              t = p = w[z++] / 20;
              g && g.moveTo(q, t);
              h && h.moveTo(q, t);
              break;
            case 10:
              q = w[z++] / 20;
              t = w[z++] / 20;
              g && g.lineTo(q, t);
              h && h.lineTo(q, t);
              break;
            case 11:
              l = w[z++] / 20;
              v = w[z++] / 20;
              q = w[z++] / 20;
              t = w[z++] / 20;
              g && g.quadraticCurveTo(l, v, q, t);
              h && h.quadraticCurveTo(l, v, q, t);
              break;
            case 12:
              l = w[z++] / 20;
              v = w[z++] / 20;
              var E = w[z++] / 20, A = w[z++] / 20, q = w[z++] / 20, t = w[z++] / 20;
              g && g.bezierCurveTo(l, v, E, A, q, t);
              h && h.bezierCurveTo(l, v, E, A, q, t);
              break;
            case 1:
              g = this._createPath(0, k.ColorUtilities.rgbaToCSSStyle(D.readUnsignedInt()), !1, null, q, t);
              break;
            case 3:
              l = this._readBitmap(D, d);
              g = this._createPath(0, l.style, l.smoothImage, null, q, t);
              break;
            case 2:
              g = this._createPath(0, this._readGradient(D, d), !1, null, q, t);
              break;
            case 4:
              g = null;
              break;
            case 5:
              h = k.ColorUtilities.rgbaToCSSStyle(D.readUnsignedInt());
              D.position += 1;
              l = D.readByte();
              v = b.LINE_CAPS_STYLES[D.readByte()];
              E = b.LINE_JOINTS_STYLES[D.readByte()];
              l = new n(w[z++] / 20, l, v, E, D.readByte());
              h = this._createPath(1, h, !1, l, q, t);
              break;
            case 6:
              h = this._createPath(2, this._readGradient(D, d), !1, null, q, t);
              break;
            case 7:
              l = this._readBitmap(D, d);
              h = this._createPath(2, l.style, l.smoothImage, null, q, t);
              break;
            case 8:
              h = null;
          }
        }
        r && g && (g.lineTo(m, p), h && h.lineTo(m, p));
        this._pathData = null;
        return e;
      };
      b.prototype._createPath = function(a, b, c, d, e, g) {
        a = new q(a, b, c, d);
        this._paths.push(a);
        a.path.moveTo(e, g);
        return a.path;
      };
      b.prototype._readMatrix = function(a) {
        return new l(a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat());
      };
      b.prototype._readGradient = function(a, b) {
        var c = a.readUnsignedByte(), d = 2 * a.readShort() / 255, e = this._readMatrix(a), c = 16 === c ? b.createLinearGradient(-1, 0, 1, 0) : b.createRadialGradient(d, 0, 0, 0, 0, 1);
        c.setTransform && c.setTransform(e.toSVGMatrix());
        e = a.readUnsignedByte();
        for (d = 0;d < e;d++) {
          var g = a.readUnsignedByte() / 255, h = k.ColorUtilities.rgbaToCSSStyle(a.readUnsignedInt());
          c.addColorStop(g, h);
        }
        a.position += 2;
        return c;
      };
      b.prototype._readBitmap = function(a, b) {
        var c = a.readUnsignedInt(), d = this._readMatrix(a), e = a.readBoolean() ? "repeat" : "no-repeat", g = a.readBoolean();
        (c = this._textures[c]) ? (e = b.createPattern(c.renderSource, e), e.setTransform(d.toSVGMatrix())) : e = null;
        return {style:e, smoothImage:g};
      };
      b.prototype._renderFallback = function(a) {
        this.fillStyle || (this.fillStyle = k.ColorStyle.randomStyle());
        var b = this._bounds;
        a.save();
        a.beginPath();
        a.lineWidth = 2;
        a.fillStyle = this.fillStyle;
        a.fillRect(b.x, b.y, b.w, b.h);
        a.restore();
      };
      b.LINE_CAPS_STYLES = ["round", "butt", "square"];
      b.LINE_JOINTS_STYLES = ["round", "bevel", "miter"];
      return b;
    }(h);
    p.RenderableShape = v;
    t = function(b) {
      function c() {
        b.apply(this, arguments);
        this._flags = 7340048;
        this._morphPaths = Object.create(null);
      }
      __extends(c, b);
      c.prototype._deserializePaths = function(b, c, d) {
        if (this._morphPaths[d]) {
          return this._morphPaths[d];
        }
        var e = this._morphPaths[d] = [], g = null, h = null, q = 0, t = 0, l, r, m = !1, p = 0, w = 0, T = b.commands, D = b.coordinates, z = b.morphCoordinates, B = b.styles, E = b.morphStyles;
        B.position = 0;
        var A = E.position = 0;
        b = b.commandsPosition;
        for (var C = 0;C < b;C++) {
          switch(T[C]) {
            case 9:
              m && g && (g.lineTo(p, w), h && h.lineTo(p, w));
              m = !0;
              q = p = u(D[A], z[A++], d) / 20;
              t = w = u(D[A], z[A++], d) / 20;
              g && g.moveTo(q, t);
              h && h.moveTo(q, t);
              break;
            case 10:
              q = u(D[A], z[A++], d) / 20;
              t = u(D[A], z[A++], d) / 20;
              g && g.lineTo(q, t);
              h && h.lineTo(q, t);
              break;
            case 11:
              l = u(D[A], z[A++], d) / 20;
              r = u(D[A], z[A++], d) / 20;
              q = u(D[A], z[A++], d) / 20;
              t = u(D[A], z[A++], d) / 20;
              g && g.quadraticCurveTo(l, r, q, t);
              h && h.quadraticCurveTo(l, r, q, t);
              break;
            case 12:
              l = u(D[A], z[A++], d) / 20;
              r = u(D[A], z[A++], d) / 20;
              var F = u(D[A], z[A++], d) / 20, G = u(D[A], z[A++], d) / 20, q = u(D[A], z[A++], d) / 20, t = u(D[A], z[A++], d) / 20;
              g && g.bezierCurveTo(l, r, F, G, q, t);
              h && h.bezierCurveTo(l, r, F, G, q, t);
              break;
            case 1:
              g = this._createMorphPath(0, d, k.ColorUtilities.rgbaToCSSStyle(a(B.readUnsignedInt(), E.readUnsignedInt(), d)), !1, null, q, t);
              break;
            case 3:
              l = this._readMorphBitmap(B, E, d, c);
              g = this._createMorphPath(0, d, l.style, l.smoothImage, null, q, t);
              break;
            case 2:
              l = this._readMorphGradient(B, E, d, c);
              g = this._createMorphPath(0, d, l, !1, null, q, t);
              break;
            case 4:
              g = null;
              break;
            case 5:
              l = u(D[A], z[A++], d) / 20;
              h = k.ColorUtilities.rgbaToCSSStyle(a(B.readUnsignedInt(), E.readUnsignedInt(), d));
              B.position += 1;
              r = B.readByte();
              F = v.LINE_CAPS_STYLES[B.readByte()];
              G = v.LINE_JOINTS_STYLES[B.readByte()];
              l = new n(l, r, F, G, B.readByte());
              h = this._createMorphPath(1, d, h, !1, l, q, t);
              break;
            case 6:
              l = this._readMorphGradient(B, E, d, c);
              h = this._createMorphPath(2, d, l, !1, null, q, t);
              break;
            case 7:
              l = this._readMorphBitmap(B, E, d, c);
              h = this._createMorphPath(2, d, l.style, l.smoothImage, null, q, t);
              break;
            case 8:
              h = null;
          }
        }
        m && g && (g.lineTo(p, w), h && h.lineTo(p, w));
        return e;
      };
      c.prototype._createMorphPath = function(a, b, c, d, e, g, h) {
        a = new q(a, c, d, e);
        this._morphPaths[b].push(a);
        a.path.moveTo(g, h);
        return a.path;
      };
      c.prototype._readMorphMatrix = function(a, b, c) {
        return new l(u(a.readFloat(), b.readFloat(), c), u(a.readFloat(), b.readFloat(), c), u(a.readFloat(), b.readFloat(), c), u(a.readFloat(), b.readFloat(), c), u(a.readFloat(), b.readFloat(), c), u(a.readFloat(), b.readFloat(), c));
      };
      c.prototype._readMorphGradient = function(b, c, d, e) {
        var g = b.readUnsignedByte(), h = 2 * b.readShort() / 255, q = this._readMorphMatrix(b, c, d);
        e = 16 === g ? e.createLinearGradient(-1, 0, 1, 0) : e.createRadialGradient(h, 0, 0, 0, 0, 1);
        e.setTransform && e.setTransform(q.toSVGMatrix());
        q = b.readUnsignedByte();
        for (g = 0;g < q;g++) {
          var h = u(b.readUnsignedByte() / 255, c.readUnsignedByte() / 255, d), n = a(b.readUnsignedInt(), c.readUnsignedInt(), d), n = k.ColorUtilities.rgbaToCSSStyle(n);
          e.addColorStop(h, n);
        }
        b.position += 2;
        return e;
      };
      c.prototype._readMorphBitmap = function(a, b, c, d) {
        var e = a.readUnsignedInt();
        b = this._readMorphMatrix(a, b, c);
        c = a.readBoolean() ? "repeat" : "no-repeat";
        a = a.readBoolean();
        d = d.createPattern(this._textures[e]._canvas, c);
        d.setTransform(b.toSVGMatrix());
        return {style:d, smoothImage:a};
      };
      return c;
    }(v);
    p.RenderableMorphShape = t;
    var e = function() {
      function a() {
        this.align = this.leading = this.descent = this.ascent = this.width = this.y = this.x = 0;
        this.runs = [];
      }
      a._getMeasureContext = function() {
        a._measureContext || (a._measureContext = document.createElement("canvas").getContext("2d"));
        return a._measureContext;
      };
      a.prototype.addRun = function(b, f, e, h, k) {
        if (e) {
          var q = a._getMeasureContext();
          q.font = b;
          q = w(q, e, h);
          this.runs.push(new g(b, f, e, q, h, k));
          this.width += q;
        }
      };
      a.prototype.wrap = function(b) {
        var f = [this], e = this.runs, h = this;
        h.width = 0;
        h.runs = [];
        for (var k = a._getMeasureContext(), q = 0;q < e.length;q++) {
          var n = e[q], t = n.text;
          n.text = "";
          n.width = 0;
          k.font = n.font;
          for (var l = b, v = t.split(/[\s.-]/), r = 0, m = 0;m < v.length;m++) {
            var p = v[m], u = t.substr(r, p.length + 1), T = n.letterSpacing, D = w(k, u, T);
            if (D > l) {
              do {
                if (n.text && (h.runs.push(n), h.width += n.width, n = new g(n.font, n.fillStyle, "", 0, n.letterSpacing, n.underline), l = new a, l.y = h.y + h.descent + h.leading + h.ascent | 0, l.ascent = h.ascent, l.descent = h.descent, l.leading = h.leading, l.align = h.align, f.push(l), h = l), l = b - D, 0 > l) {
                  for (var z = u.length, B = u;1 < z && !(z--, B = u.substr(0, z), D = w(k, B, T), D <= b);) {
                  }
                  n.text = B;
                  n.width = D;
                  u = u.substr(z);
                  D = w(k, u, T);
                }
              } while (u && 0 > l);
            } else {
              l -= D;
            }
            n.text += u;
            n.width += D;
            r += p.length + 1;
          }
          h.runs.push(n);
          h.width += n.width;
        }
        return f;
      };
      a.prototype.toString = function() {
        return "TextLine {x: " + this.x + ", y: " + this.y + ", width: " + this.width + ", height: " + (this.ascent + this.descent + this.leading) + "}";
      };
      return a;
    }();
    p.TextLine = e;
    var g = function() {
      return function(a, b, f, e, g, h) {
        void 0 === a && (a = "");
        void 0 === b && (b = "");
        void 0 === f && (f = "");
        void 0 === e && (e = 0);
        void 0 === g && (g = 0);
        void 0 === h && (h = !1);
        this.font = a;
        this.fillStyle = b;
        this.text = f;
        this.width = e;
        this.letterSpacing = g;
        this.underline = h;
      };
    }();
    p.TextRun = g;
    t = function(a) {
      function b(c) {
        a.call(this);
        this._flags = 1048592;
        this.properties = {};
        this._textBounds = c.clone();
        this._textRunData = null;
        this._plainText = "";
        this._borderColor = this._backgroundColor = 0;
        this._matrix = l.createIdentity();
        this._coords = null;
        this._scrollV = 1;
        this._scrollH = 0;
        this.textRect = c.clone();
        this.lines = [];
        this.setBounds(c);
      }
      __extends(b, a);
      b.prototype.setBounds = function(b) {
        a.prototype.setBounds.call(this, b);
        this._textBounds.set(b);
        this.textRect.setElements(b.x + 2, b.y + 2, b.w - 2, b.h - 2);
      };
      b.prototype.setContent = function(a, b, c, d) {
        this._textRunData = b;
        this._plainText = a;
        this._matrix.set(c);
        this._coords = d;
        this.lines = [];
      };
      b.prototype.setStyle = function(a, b, c, d) {
        this._backgroundColor = a;
        this._borderColor = b;
        this._scrollV = c;
        this._scrollH = d;
      };
      b.prototype.reflow = function(a, b) {
        var c = this._textRunData;
        if (c) {
          for (var d = this._bounds, g = d.w - 4, h = this._plainText, n = this.lines, q = new e, t = 0, l = 0, v = 0, r = 0, m = -4294967295, p = -1;c.position < c.length;) {
            var u = c.readInt(), w = c.readInt(), B = c.readInt(), E = c.readUTF(), A = c.readInt(), C = c.readInt(), F = c.readInt();
            A > v && (v = A);
            C > r && (r = C);
            F > m && (m = F);
            A = c.readBoolean();
            C = "";
            c.readBoolean() && (C += "italic ");
            A && (C += "bold ");
            B = C + B + "px " + E + ", AdobeBlank";
            E = c.readInt();
            E = k.ColorUtilities.rgbToHex(E);
            A = c.readInt();
            -1 === p && (p = A);
            c.readBoolean();
            c.readInt();
            c.readInt();
            c.readInt();
            A = c.readInt();
            c.readInt();
            for (var C = c.readBoolean(), G = "", F = !1;!F;u++) {
              var F = u >= w - 1, J = h[u];
              if ("\r" !== J && "\n" !== J && (G += J, u < h.length - 1)) {
                continue;
              }
              q.addRun(B, E, G, A, C);
              if (q.runs.length) {
                n.length && (t += m);
                t += v;
                q.y = t | 0;
                t += r;
                q.ascent = v;
                q.descent = r;
                q.leading = m;
                q.align = p;
                if (b && q.width > g) {
                  for (q = q.wrap(g), G = 0;G < q.length;G++) {
                    var H = q[G], t = H.y + H.descent + H.leading;
                    n.push(H);
                    H.width > l && (l = H.width);
                  }
                } else {
                  n.push(q), q.width > l && (l = q.width);
                }
                q = new e;
              } else {
                t += v + r + m;
              }
              G = "";
              if (F) {
                r = v = 0;
                m = -4294967295;
                p = -1;
                break;
              }
              "\r" === J && "\n" === h[u + 1] && u++;
            }
            q.addRun(B, E, G, A, C);
          }
          c = h[h.length - 1];
          "\r" !== c && "\n" !== c || n.push(q);
          c = this.textRect;
          c.w = l;
          c.h = t;
          if (a) {
            if (!b) {
              g = l;
              l = d.w;
              switch(a) {
                case 1:
                  c.x = l - (g + 4) >> 1;
                  break;
                case 3:
                  c.x = l - (g + 4);
              }
              this._textBounds.setElements(c.x - 2, c.y - 2, c.w + 4, c.h + 4);
              d.w = g + 4;
            }
            d.x = c.x - 2;
            d.h = t + 4;
          } else {
            this._textBounds = d;
          }
          for (u = 0;u < n.length;u++) {
            if (d = n[u], d.width < g) {
              switch(d.align) {
                case 1:
                  d.x = g - d.width | 0;
                  break;
                case 2:
                  d.x = (g - d.width) / 2 | 0;
              }
            }
          }
          this.invalidate();
        }
      };
      b.roundBoundPoints = function(a) {
        for (var b = 0;b < a.length;b++) {
          var c = a[b];
          c.x = Math.floor(c.x + .1) + .5;
          c.y = Math.floor(c.y + .1) + .5;
        }
      };
      b.prototype.render = function(a) {
        a.save();
        var d = this._textBounds;
        this._backgroundColor && (a.fillStyle = k.ColorUtilities.rgbaToCSSStyle(this._backgroundColor), a.fillRect(d.x, d.y, d.w, d.h));
        if (this._borderColor) {
          a.strokeStyle = k.ColorUtilities.rgbaToCSSStyle(this._borderColor);
          a.lineCap = "square";
          a.lineWidth = 1;
          var e = b.absoluteBoundPoints, g = a.currentTransform;
          g ? (d = d.clone(), (new l(g.a, g.b, g.c, g.d, g.e, g.f)).transformRectangle(d, e), a.setTransform(1, 0, 0, 1, 0, 0)) : (e[0].x = d.x, e[0].y = d.y, e[1].x = d.x + d.w, e[1].y = d.y, e[2].x = d.x + d.w, e[2].y = d.y + d.h, e[3].x = d.x, e[3].y = d.y + d.h);
          b.roundBoundPoints(e);
          d = new Path2D;
          d.moveTo(e[0].x, e[0].y);
          d.lineTo(e[1].x, e[1].y);
          d.lineTo(e[2].x, e[2].y);
          d.lineTo(e[3].x, e[3].y);
          d.lineTo(e[0].x, e[0].y);
          a.stroke(d);
          g && a.setTransform(g.a, g.b, g.c, g.d, g.e, g.f);
        }
        this._coords ? this._renderChars(a) : this._renderLines(a);
        a.restore();
      };
      b.prototype._renderChars = function(a) {
        if (this._matrix) {
          var b = this._matrix;
          a.transform(b.a, b.b, b.c, b.d, b.tx, b.ty);
        }
        var b = this.lines, c = this._coords;
        c.position = 0;
        for (var d = "", e = "", g = 0;g < b.length;g++) {
          for (var h = b[g].runs, k = 0;k < h.length;k++) {
            var q = h[k];
            q.font !== d && (a.font = d = q.font);
            q.fillStyle !== e && (a.fillStyle = e = q.fillStyle);
            for (var q = q.text, n = 0;n < q.length;n++) {
              var t = c.readInt() / 20, l = c.readInt() / 20;
              a.fillText(q[n], t, l);
            }
          }
        }
      };
      b.prototype._renderLines = function(a) {
        var b = this._textBounds;
        a.beginPath();
        a.rect(b.x + 2, b.y + 2, b.w - 4, b.h - 4);
        a.clip();
        a.translate(b.x - this._scrollH + 2, b.y + 2);
        var c = this.lines, d = this._scrollV, e = 0, g = "", h = "";
        a.textAlign = "left";
        a.textBaseline = "alphabetic";
        for (var k = 0;k < c.length;k++) {
          var q = c[k], n = q.x, t = q.y;
          if (k + 1 < d) {
            e = t + q.descent + q.leading;
          } else {
            t -= e;
            if (k + 1 - d && t > b.h) {
              break;
            }
            for (var l = q.runs, v = 0;v < l.length;v++) {
              var r = l[v];
              r.font !== g && (a.font = g = r.font);
              r.fillStyle !== h && (a.fillStyle = h = r.fillStyle);
              r.underline && a.fillRect(n, t + q.descent / 2 | 0, r.width, 1);
              a.textAlign = "left";
              a.textBaseline = "alphabetic";
              if (0 < r.letterSpacing) {
                for (var m = r.text, p = 0;p < m.length;p++) {
                  a.fillText(m[p], n, t), n += w(a, m[p], r.letterSpacing);
                }
              } else {
                a.fillText(r.text, n, t), n += r.width;
              }
            }
          }
        }
      };
      b.absoluteBoundPoints = [new m(0, 0), new m(0, 0), new m(0, 0), new m(0, 0)];
      return b;
    }(h);
    p.RenderableText = t;
    h = function(a) {
      function c(c, e) {
        a.call(this);
        this._flags = 3145728;
        this.properties = {};
        this.setBounds(new b(0, 0, c, e));
      }
      __extends(c, a);
      Object.defineProperty(c.prototype, "text", {get:function() {
        return this._text;
      }, set:function(a) {
        this._text = a;
      }, enumerable:!0, configurable:!0});
      c.prototype.render = function(a, b, c) {
        a.save();
        a.textBaseline = "top";
        a.fillStyle = "white";
        a.fillText(this.text, 0, 0);
        a.restore();
      };
      return c;
    }(h);
    p.Label = h;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    function u(a, b, h, k, q) {
      b = m[b - 1];
      q && (q = b / 4, h -= q, k -= q);
      q = Math.ceil((1 > h ? 1 : h) / (b - 1E-9));
      b = Math.ceil((1 > k ? 1 : k) / (b - 1E-9));
      a.x -= q;
      a.w += 2 * q;
      a.y -= b;
      a.h += 2 * b;
    }
    var a = k.ColorUtilities.clampByte, w = function() {
      function a() {
      }
      a.prototype.expandBounds = function(a) {
      };
      return a;
    }();
    p.Filter = w;
    var m = [2, 1 / 1.05, 1 / 1.35, 1 / 1.55, 1 / 1.75, 1 / 1.9, .5, 1 / 2.1, 1 / 2.2, 1 / 2.3, .4, 1 / 3, 1 / 3, 1 / 3.5, 1 / 3.5], b = function(a) {
      function b(h, k, q) {
        a.call(this);
        this.blurX = h;
        this.blurY = k;
        this.quality = q;
      }
      __extends(b, a);
      b.prototype.expandBounds = function(a) {
        u(a, this.quality, this.blurX, this.blurY, !0);
      };
      return b;
    }(w);
    p.BlurFilter = b;
    b = function(a) {
      function b(h, k, q, n, v, e, g, d, c, f, r) {
        a.call(this);
        this.alpha = h;
        this.angle = k;
        this.blurX = q;
        this.blurY = n;
        this.color = v;
        this.distance = e;
        this.hideObject = g;
        this.inner = d;
        this.knockout = c;
        this.quality = f;
        this.strength = r;
      }
      __extends(b, a);
      b.prototype.expandBounds = function(a) {
        if (!this.inner && (u(a, this.quality, this.blurX, this.blurY, !1), this.distance)) {
          var b = this.angle * Math.PI / 180, k = Math.cos(b) * this.distance, n = Math.sin(b) * this.distance, b = a.x + (0 <= k ? 0 : Math.floor(k)), k = a.x + a.w + Math.ceil(Math.abs(k)), l = a.y + (0 <= n ? 0 : Math.floor(n)), n = a.y + a.h + Math.ceil(Math.abs(n));
          a.x = b;
          a.w = k - b;
          a.y = l;
          a.h = n - l;
        }
      };
      return b;
    }(w);
    p.DropshadowFilter = b;
    b = function(a) {
      function b(h, k, q, n, v, e, g, d) {
        a.call(this);
        this.alpha = h;
        this.blurX = k;
        this.blurY = q;
        this.color = n;
        this.inner = v;
        this.knockout = e;
        this.quality = g;
        this.strength = d;
      }
      __extends(b, a);
      b.prototype.expandBounds = function(a) {
        this.inner || u(a, this.quality, this.blurX, this.blurY, !1);
      };
      return b;
    }(w);
    p.GlowFilter = b;
    (function(a) {
      a[a.Unknown = 0] = "Unknown";
      a[a.Identity = 1] = "Identity";
    })(p.ColorMatrixType || (p.ColorMatrixType = {}));
    w = function(b) {
      function k(a) {
        b.call(this);
        this._data = new Float32Array(a);
        this._type = 0;
      }
      __extends(k, b);
      k.prototype.clone = function() {
        var a = new k(this._data);
        a._type = this._type;
        return a;
      };
      k.prototype.set = function(a) {
        this._data.set(a._data);
        this._type = a._type;
      };
      k.prototype.toWebGLMatrix = function() {
        return new Float32Array(this._data);
      };
      k.prototype.asWebGLMatrix = function() {
        return this._data.subarray(0, 16);
      };
      k.prototype.asWebGLVector = function() {
        return this._data.subarray(16, 20);
      };
      k.prototype.isIdentity = function() {
        if (this._type & 1) {
          return !0;
        }
        var a = this._data;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 1 == a[15] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      k.createIdentity = function() {
        var a = new k([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]);
        a._type = 1;
        return a;
      };
      k.prototype.setMultipliersAndOffsets = function(a, b, k, n, l, e, g, d) {
        for (var c = this._data, f = 0;f < c.length;f++) {
          c[f] = 0;
        }
        c[0] = a;
        c[5] = b;
        c[10] = k;
        c[15] = n;
        c[16] = l / 255;
        c[17] = e / 255;
        c[18] = g / 255;
        c[19] = d / 255;
        this._type = 0;
      };
      k.prototype.transformRGBA = function(b) {
        var k = b >> 24 & 255, q = b >> 16 & 255, n = b >> 8 & 255, l = b & 255, e = this._data;
        b = a(k * e[0] + q * e[1] + n * e[2] + l * e[3] + 255 * e[16]);
        var g = a(k * e[4] + q * e[5] + n * e[6] + l * e[7] + 255 * e[17]), d = a(k * e[8] + q * e[9] + n * e[10] + l * e[11] + 255 * e[18]), k = a(k * e[12] + q * e[13] + n * e[14] + l * e[15] + 255 * e[19]);
        return b << 24 | g << 16 | d << 8 | k;
      };
      k.prototype.multiply = function(a) {
        if (!(a._type & 1)) {
          var b = this._data, k = a._data;
          a = b[0];
          var n = b[1], l = b[2], e = b[3], g = b[4], d = b[5], c = b[6], f = b[7], r = b[8], x = b[9], m = b[10], p = b[11], u = b[12], w = b[13], S = b[14], W = b[15], ga = b[16], ha = b[17], ia = b[18], ja = b[19], T = k[0], D = k[1], z = k[2], B = k[3], E = k[4], A = k[5], C = k[6], F = k[7], G = k[8], J = k[9], H = k[10], K = k[11], L = k[12], M = k[13], N = k[14], O = k[15], P = k[16], Q = k[17], R = k[18], k = k[19];
          b[0] = a * T + g * D + r * z + u * B;
          b[1] = n * T + d * D + x * z + w * B;
          b[2] = l * T + c * D + m * z + S * B;
          b[3] = e * T + f * D + p * z + W * B;
          b[4] = a * E + g * A + r * C + u * F;
          b[5] = n * E + d * A + x * C + w * F;
          b[6] = l * E + c * A + m * C + S * F;
          b[7] = e * E + f * A + p * C + W * F;
          b[8] = a * G + g * J + r * H + u * K;
          b[9] = n * G + d * J + x * H + w * K;
          b[10] = l * G + c * J + m * H + S * K;
          b[11] = e * G + f * J + p * H + W * K;
          b[12] = a * L + g * M + r * N + u * O;
          b[13] = n * L + d * M + x * N + w * O;
          b[14] = l * L + c * M + m * N + S * O;
          b[15] = e * L + f * M + p * N + W * O;
          b[16] = a * P + g * Q + r * R + u * k + ga;
          b[17] = n * P + d * Q + x * R + w * k + ha;
          b[18] = l * P + c * Q + m * R + S * k + ia;
          b[19] = e * P + f * Q + p * R + W * k + ja;
          this._type = 0;
        }
      };
      Object.defineProperty(k.prototype, "alphaMultiplier", {get:function() {
        return this._data[15];
      }, enumerable:!0, configurable:!0});
      k.prototype.hasOnlyAlphaMultiplier = function() {
        var a = this._data;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      k.prototype.equals = function(a) {
        if (!a) {
          return !1;
        }
        if (this._type === a._type && 1 === this._type) {
          return !0;
        }
        var b = this._data;
        a = a._data;
        for (var k = 0;20 > k;k++) {
          if (.001 < Math.abs(b[k] - a[k])) {
            return !1;
          }
        }
        return !0;
      };
      k.prototype.toSVGFilterMatrix = function() {
        var a = this._data;
        return [a[0], a[4], a[8], a[12], a[16], a[1], a[5], a[9], a[13], a[17], a[2], a[6], a[10], a[14], a[18], a[3], a[7], a[11], a[15], a[19]].join(" ");
      };
      return k;
    }(w);
    p.ColorMatrix = w;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      function a(a, b) {
        return -1 !== a.indexOf(b, this.length - b.length);
      }
      var w = p.Geometry.Point3D, m = p.Geometry.Matrix3D, b = p.Geometry.degreesToRadian, l = k.Debug.unexpected, r = k.Debug.notImplemented;
      u.SHADER_ROOT = "shaders/";
      var h = function() {
        function h(a, b) {
          this._fillColor = k.Color.Red;
          this._surfaceRegionCache = new k.LRUList;
          this.modelViewProjectionMatrix = m.createIdentity();
          this._canvas = a;
          this._options = b;
          this.gl = a.getContext("experimental-webgl", {preserveDrawingBuffer:!1, antialias:!0, stencil:!0, premultipliedAlpha:!1});
          this._programCache = Object.create(null);
          this._resize();
          this.gl.pixelStorei(this.gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, b.unpackPremultiplyAlpha ? this.gl.ONE : this.gl.ZERO);
          this._backgroundColor = k.Color.Black;
          this._geometry = new u.WebGLGeometry(this);
          this._tmpVertices = u.Vertex.createEmptyVertices(u.Vertex, 64);
          this._maxSurfaces = b.maxSurfaces;
          this._maxSurfaceSize = b.maxSurfaceSize;
          this.gl.blendFunc(this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
          this.gl.enable(this.gl.BLEND);
          this.modelViewProjectionMatrix = m.create2DProjection(this._w, this._h, 2E3);
          var l = this;
          this._surfaceRegionAllocator = new p.SurfaceRegionAllocator.SimpleAllocator(function() {
            var a = l._createTexture(1024, 1024);
            return new u.WebGLSurface(1024, 1024, a);
          });
        }
        Object.defineProperty(h.prototype, "surfaces", {get:function() {
          return this._surfaceRegionAllocator.surfaces;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(h.prototype, "fillStyle", {set:function(a) {
          this._fillColor.set(k.Color.parseColor(a));
        }, enumerable:!0, configurable:!0});
        h.prototype.setBlendMode = function(a) {
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
              r("Blend Mode: " + a);
          }
        };
        h.prototype.setBlendOptions = function() {
          this.gl.blendFunc(this._options.sourceBlendFactor, this._options.destinationBlendFactor);
        };
        h.glSupportedBlendMode = function(a) {
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
        h.prototype.create2DProjectionMatrix = function() {
          return m.create2DProjection(this._w, this._h, -this._w);
        };
        h.prototype.createPerspectiveMatrix = function(a, h, k) {
          k = b(k);
          h = m.createPerspective(b(h), 1, .1, 5E3);
          var e = new w(0, 1, 0), g = new w(0, 0, 0);
          a = new w(0, 0, a);
          a = m.createCameraLookAt(a, g, e);
          a = m.createInverse(a);
          e = m.createIdentity();
          e = m.createMultiply(e, m.createTranslation(-this._w / 2, -this._h / 2, 0));
          e = m.createMultiply(e, m.createScale(1 / this._w, -1 / this._h, .01));
          e = m.createMultiply(e, m.createYRotation(k));
          e = m.createMultiply(e, a);
          return e = m.createMultiply(e, h);
        };
        h.prototype.discardCachedImages = function() {
          2 <= p.traceLevel && p.writer && p.writer.writeLn("Discard Cache");
          for (var a = this._surfaceRegionCache.count / 2 | 0, b = 0;b < a;b++) {
            var h = this._surfaceRegionCache.pop();
            2 <= p.traceLevel && p.writer && p.writer.writeLn("Discard: " + h);
            h.texture.atlas.remove(h.region);
            h.texture = null;
          }
        };
        h.prototype.cacheImage = function(a) {
          var b = this.allocateSurfaceRegion(a.width, a.height);
          2 <= p.traceLevel && p.writer && p.writer.writeLn("Uploading Image: @ " + b.region);
          this._surfaceRegionCache.use(b);
          this.updateSurfaceRegion(a, b);
          return b;
        };
        h.prototype.allocateSurfaceRegion = function(a, b, h) {
          return this._surfaceRegionAllocator.allocate(a, b, null);
        };
        h.prototype.updateSurfaceRegion = function(a, b) {
          var h = this.gl;
          h.bindTexture(h.TEXTURE_2D, b.surface.texture);
          h.texSubImage2D(h.TEXTURE_2D, 0, b.region.x, b.region.y, h.RGBA, h.UNSIGNED_BYTE, a);
        };
        h.prototype._resize = function() {
          var a = this.gl;
          this._w = this._canvas.width;
          this._h = this._canvas.height;
          a.viewport(0, 0, this._w, this._h);
          for (var b in this._programCache) {
            this._initializeProgram(this._programCache[b]);
          }
        };
        h.prototype._initializeProgram = function(a) {
          this.gl.useProgram(a);
        };
        h.prototype._createShaderFromFile = function(b) {
          var h = u.SHADER_ROOT + b, k = this.gl;
          b = new XMLHttpRequest;
          b.open("GET", h, !1);
          b.send();
          if (a(h, ".vert")) {
            h = k.VERTEX_SHADER;
          } else {
            if (a(h, ".frag")) {
              h = k.FRAGMENT_SHADER;
            } else {
              throw "Shader Type: not supported.";
            }
          }
          return this._createShader(h, b.responseText);
        };
        h.prototype.createProgramFromFiles = function(a, b) {
          var h = a + "-" + b, e = this._programCache[h];
          e || (e = this._createProgram([this._createShaderFromFile(a), this._createShaderFromFile(b)]), this._queryProgramAttributesAndUniforms(e), this._initializeProgram(e), this._programCache[h] = e);
          return e;
        };
        h.prototype._createProgram = function(a) {
          var b = this.gl, h = b.createProgram();
          a.forEach(function(a) {
            b.attachShader(h, a);
          });
          b.linkProgram(h);
          b.getProgramParameter(h, b.LINK_STATUS) || (a = b.getProgramInfoLog(h), l("Cannot link program: " + a), b.deleteProgram(h));
          return h;
        };
        h.prototype._createShader = function(a, b) {
          var h = this.gl, e = h.createShader(a);
          h.shaderSource(e, b);
          h.compileShader(e);
          if (!h.getShaderParameter(e, h.COMPILE_STATUS)) {
            var g = h.getShaderInfoLog(e);
            l("Cannot compile shader: " + g);
            h.deleteShader(e);
            return null;
          }
          return e;
        };
        h.prototype._createTexture = function(a, b) {
          var h = this.gl, e = h.createTexture();
          h.bindTexture(h.TEXTURE_2D, e);
          h.texParameteri(h.TEXTURE_2D, h.TEXTURE_WRAP_S, h.CLAMP_TO_EDGE);
          h.texParameteri(h.TEXTURE_2D, h.TEXTURE_WRAP_T, h.CLAMP_TO_EDGE);
          h.texParameteri(h.TEXTURE_2D, h.TEXTURE_MIN_FILTER, h.LINEAR);
          h.texParameteri(h.TEXTURE_2D, h.TEXTURE_MAG_FILTER, h.LINEAR);
          h.texImage2D(h.TEXTURE_2D, 0, h.RGBA, a, b, 0, h.RGBA, h.UNSIGNED_BYTE, null);
          return e;
        };
        h.prototype._createFramebuffer = function(a) {
          var b = this.gl, h = b.createFramebuffer();
          b.bindFramebuffer(b.FRAMEBUFFER, h);
          b.framebufferTexture2D(b.FRAMEBUFFER, b.COLOR_ATTACHMENT0, b.TEXTURE_2D, a, 0);
          b.bindFramebuffer(b.FRAMEBUFFER, null);
          return h;
        };
        h.prototype._queryProgramAttributesAndUniforms = function(a) {
          a.uniforms = {};
          a.attributes = {};
          for (var b = this.gl, h = 0, e = b.getProgramParameter(a, b.ACTIVE_ATTRIBUTES);h < e;h++) {
            var g = b.getActiveAttrib(a, h);
            a.attributes[g.name] = g;
            g.location = b.getAttribLocation(a, g.name);
          }
          h = 0;
          for (e = b.getProgramParameter(a, b.ACTIVE_UNIFORMS);h < e;h++) {
            g = b.getActiveUniform(a, h), a.uniforms[g.name] = g, g.location = b.getUniformLocation(a, g.name);
          }
        };
        Object.defineProperty(h.prototype, "target", {set:function(a) {
          var b = this.gl;
          a ? (b.viewport(0, 0, a.w, a.h), b.bindFramebuffer(b.FRAMEBUFFER, a.framebuffer)) : (b.viewport(0, 0, this._w, this._h), b.bindFramebuffer(b.FRAMEBUFFER, null));
        }, enumerable:!0, configurable:!0});
        h.prototype.clear = function(a) {
          a = this.gl;
          a.clearColor(0, 0, 0, 0);
          a.clear(a.COLOR_BUFFER_BIT);
        };
        h.prototype.clearTextureRegion = function(a, b) {
          void 0 === b && (b = k.Color.None);
          var h = this.gl, e = a.region;
          this.target = a.surface;
          h.enable(h.SCISSOR_TEST);
          h.scissor(e.x, e.y, e.w, e.h);
          h.clearColor(b.r, b.g, b.b, b.a);
          h.clear(h.COLOR_BUFFER_BIT | h.DEPTH_BUFFER_BIT);
          h.disable(h.SCISSOR_TEST);
        };
        h.prototype.sizeOf = function(a) {
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
              r(a);
          }
        };
        h.MAX_SURFACES = 8;
        return h;
      }();
      u.WebGLContext = h;
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, p) {
  function u() {
    this.constructor = k;
  }
  for (var a in p) {
    p.hasOwnProperty(a) && (k[a] = p[a]);
  }
  u.prototype = p.prototype;
  k.prototype = new u;
};
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.Debug.assert, w = function(b) {
        function k() {
          b.apply(this, arguments);
        }
        __extends(k, b);
        k.prototype.ensureVertexCapacity = function(b) {
          a(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8 * b);
        };
        k.prototype.writeVertex = function(b, h) {
          a(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8);
          this.writeVertexUnsafe(b, h);
        };
        k.prototype.writeVertexUnsafe = function(a, b) {
          var k = this._offset >> 2;
          this._f32[k] = a;
          this._f32[k + 1] = b;
          this._offset += 8;
        };
        k.prototype.writeVertex3D = function(b, h, k) {
          a(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 12);
          this.writeVertex3DUnsafe(b, h, k);
        };
        k.prototype.writeVertex3DUnsafe = function(a, b, k) {
          var l = this._offset >> 2;
          this._f32[l] = a;
          this._f32[l + 1] = b;
          this._f32[l + 2] = k;
          this._offset += 12;
        };
        k.prototype.writeTriangleElements = function(b, h, k) {
          a(0 === (this._offset & 1));
          this.ensureCapacity(this._offset + 6);
          var l = this._offset >> 1;
          this._u16[l] = b;
          this._u16[l + 1] = h;
          this._u16[l + 2] = k;
          this._offset += 6;
        };
        k.prototype.ensureColorCapacity = function(b) {
          a(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16 * b);
        };
        k.prototype.writeColorFloats = function(b, h, k, l) {
          a(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16);
          this.writeColorFloatsUnsafe(b, h, k, l);
        };
        k.prototype.writeColorFloatsUnsafe = function(a, b, k, l) {
          var n = this._offset >> 2;
          this._f32[n] = a;
          this._f32[n + 1] = b;
          this._f32[n + 2] = k;
          this._f32[n + 3] = l;
          this._offset += 16;
        };
        k.prototype.writeColor = function(b, h, k, l) {
          a(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 4);
          this._i32[this._offset >> 2] = l << 24 | k << 16 | h << 8 | b;
          this._offset += 4;
        };
        k.prototype.writeColorUnsafe = function(a, b, k, l) {
          this._i32[this._offset >> 2] = l << 24 | k << 16 | b << 8 | a;
          this._offset += 4;
        };
        k.prototype.writeRandomColor = function() {
          this.writeColor(Math.random(), Math.random(), Math.random(), Math.random() / 2);
        };
        return k;
      }(k.ArrayUtilities.ArrayWriter);
      u.BufferWriter = w;
      var m = function() {
        return function(a, k, r, h) {
          void 0 === h && (h = !1);
          this.name = a;
          this.size = k;
          this.type = r;
          this.normalized = h;
        };
      }();
      u.WebGLAttribute = m;
      m = function() {
        function a(b) {
          this.size = 0;
          this.attributes = b;
        }
        a.prototype.initialize = function(a) {
          for (var b = 0, h = 0;h < this.attributes.length;h++) {
            this.attributes[h].offset = b, b += a.sizeOf(this.attributes[h].type) * this.attributes[h].size;
          }
          this.size = b;
        };
        return a;
      }();
      u.WebGLAttributeList = m;
      m = function() {
        function a(b) {
          this._elementOffset = this.triangleCount = 0;
          this.context = b;
          this.array = new w(8);
          this.buffer = b.gl.createBuffer();
          this.elementArray = new w(8);
          this.elementBuffer = b.gl.createBuffer();
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
      u.WebGLGeometry = m;
      m = function(a) {
        function k(l, h, t) {
          a.call(this, l, h, t);
        }
        __extends(k, a);
        k.createEmptyVertices = function(a, b) {
          for (var k = [], l = 0;l < b;l++) {
            k.push(new a(0, 0, 0));
          }
          return k;
        };
        return k;
      }(p.Geometry.Point3D);
      u.Vertex = m;
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
      })(u.WebGLBlendFactor || (u.WebGLBlendFactor = {}));
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(u) {
      var a = function() {
        function a(b, l, r) {
          this.texture = r;
          this.w = b;
          this.h = l;
          this._regionAllocator = new k.RegionAllocator.CompactAllocator(this.w, this.h);
        }
        a.prototype.allocate = function(a, k) {
          var r = this._regionAllocator.allocate(a, k);
          return r ? new w(this, r) : null;
        };
        a.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return a;
      }();
      u.WebGLSurface = a;
      var w = function() {
        return function(a, b) {
          this.surface = a;
          this.region = b;
          this.next = this.previous = null;
        };
      }();
      u.WebGLSurfaceRegion = w;
    })(k.WebGL || (k.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.Color;
      u.TILE_SIZE = 256;
      u.MIN_UNTILED_SIZE = 256;
      var w = p.Geometry.Matrix, m = p.Geometry.Rectangle, b = function(a) {
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
      }(p.RendererOptions);
      u.WebGLRendererOptions = b;
      var l = function(k) {
        function h(a, h, n) {
          void 0 === n && (n = new b);
          k.call(this, a, h, n);
          this._tmpVertices = u.Vertex.createEmptyVertices(u.Vertex, 64);
          this._cachedTiles = [];
          a = this._context = new u.WebGLContext(this._canvas, n);
          this._updateSize();
          this._brush = new u.WebGLCombinedBrush(a, new u.WebGLGeometry(a));
          this._stencilBrush = new u.WebGLCombinedBrush(a, new u.WebGLGeometry(a));
          this._scratchCanvas = document.createElement("canvas");
          this._scratchCanvas.width = this._scratchCanvas.height = 2048;
          this._scratchCanvasContext = this._scratchCanvas.getContext("2d", {willReadFrequently:!0});
          this._dynamicScratchCanvas = document.createElement("canvas");
          this._dynamicScratchCanvas.width = this._dynamicScratchCanvas.height = 0;
          this._dynamicScratchCanvasContext = this._dynamicScratchCanvas.getContext("2d", {willReadFrequently:!0});
          this._uploadCanvas = document.createElement("canvas");
          this._uploadCanvas.width = this._uploadCanvas.height = 0;
          this._uploadCanvasContext = this._uploadCanvas.getContext("2d", {willReadFrequently:!0});
          n.showTemporaryCanvases && (document.getElementById("temporaryCanvasPanelContainer").appendChild(this._uploadCanvas), document.getElementById("temporaryCanvasPanelContainer").appendChild(this._scratchCanvas));
          this._clipStack = [];
        }
        __extends(h, k);
        h.prototype.resize = function() {
          this._updateSize();
          this.render();
        };
        h.prototype._updateSize = function() {
          this._viewport = new m(0, 0, this._canvas.width, this._canvas.height);
          this._context._resize();
        };
        h.prototype._cacheImageCallback = function(a, b, h) {
          var k = h.w, e = h.h, g = h.x;
          h = h.y;
          this._uploadCanvas.width = k + 2;
          this._uploadCanvas.height = e + 2;
          this._uploadCanvasContext.drawImage(b.canvas, g, h, k, e, 1, 1, k, e);
          this._uploadCanvasContext.drawImage(b.canvas, g, h, k, 1, 1, 0, k, 1);
          this._uploadCanvasContext.drawImage(b.canvas, g, h + e - 1, k, 1, 1, e + 1, k, 1);
          this._uploadCanvasContext.drawImage(b.canvas, g, h, 1, e, 0, 1, 1, e);
          this._uploadCanvasContext.drawImage(b.canvas, g + k - 1, h, 1, e, k + 1, 1, 1, e);
          return a && a.surface ? (this._options.disableSurfaceUploads || this._context.updateSurfaceRegion(this._uploadCanvas, a), a) : this._context.cacheImage(this._uploadCanvas);
        };
        h.prototype._enterClip = function(a, b, h, k) {
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
        h.prototype._leaveClip = function(a, b, h, k) {
          h.flush();
          b = this._context.gl;
          if (a = this._clipStack.pop()) {
            b.colorMask(!1, !1, !1, !1), b.stencilOp(b.KEEP, b.KEEP, b.DECR), h.flush(), b.colorMask(!0, !0, !0, !0), b.stencilFunc(b.NOTEQUAL, 0, this._clipStack.length), b.stencilOp(b.KEEP, b.KEEP, b.KEEP);
          }
          0 === this._clipStack.length && b.disable(b.STENCIL_TEST);
        };
        h.prototype._renderFrame = function(a, b, h, k, e) {
        };
        h.prototype._renderSurfaces = function(b) {
          var h = this._options, k = this._context, l = this._viewport;
          if (h.drawSurfaces) {
            var e = k.surfaces, k = w.createIdentity();
            if (0 <= h.drawSurface && h.drawSurface < e.length) {
              for (var h = e[h.drawSurface | 0], e = new m(0, 0, h.w, h.h), g = e.clone();g.w > l.w;) {
                g.scale(.5, .5);
              }
              b.drawImage(new u.WebGLSurfaceRegion(h, e), g, a.White, null, k, .2);
            } else {
              g = l.w / 5;
              g > l.h / e.length && (g = l.h / e.length);
              b.fillRectangle(new m(l.w - g, 0, g, l.h), new a(0, 0, 0, .5), k, .1);
              for (var d = 0;d < e.length;d++) {
                var h = e[d], c = new m(l.w - g, d * g, g, g);
                b.drawImage(new u.WebGLSurfaceRegion(h, new m(0, 0, h.w, h.h)), c, a.White, null, k, .2);
              }
            }
            b.flush();
          }
        };
        h.prototype.render = function() {
          var b = this._options, h = this._context.gl;
          this._context.modelViewProjectionMatrix = b.perspectiveCamera ? this._context.createPerspectiveMatrix(b.perspectiveCameraDistance + (b.animateZoom ? .8 * Math.sin(Date.now() / 3E3) : 0), b.perspectiveCameraFOV, b.perspectiveCameraAngle) : this._context.create2DProjectionMatrix();
          var k = this._brush;
          h.clearColor(0, 0, 0, 0);
          h.clear(h.COLOR_BUFFER_BIT | h.DEPTH_BUFFER_BIT);
          k.reset();
          h = this._viewport;
          k.flush();
          b.paintViewport && (k.fillRectangle(h, new a(.5, 0, 0, .25), w.createIdentity(), 0), k.flush());
          this._renderSurfaces(k);
        };
        return h;
      }(p.Renderer);
      u.WebGLRenderer = l;
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.Color, w = p.Geometry.Point, m = p.Geometry.Matrix3D, b = function() {
        function a(b, k, l) {
          this._target = l;
          this._context = b;
          this._geometry = k;
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
      u.WebGLBrush = b;
      (function(a) {
        a[a.FillColor = 0] = "FillColor";
        a[a.FillTexture = 1] = "FillTexture";
        a[a.FillTextureWithColorMatrix = 2] = "FillTextureWithColorMatrix";
      })(u.WebGLCombinedBrushKind || (u.WebGLCombinedBrushKind = {}));
      var l = function(b) {
        function h(h, k, l) {
          b.call(this, h, k, l);
          this.kind = 0;
          this.color = new a(0, 0, 0, 0);
          this.sampler = 0;
          this.coordinate = new w(0, 0);
        }
        __extends(h, b);
        h.initializeAttributeList = function(a) {
          var b = a.gl;
          h.attributeList || (h.attributeList = new u.WebGLAttributeList([new u.WebGLAttribute("aPosition", 3, b.FLOAT), new u.WebGLAttribute("aCoordinate", 2, b.FLOAT), new u.WebGLAttribute("aColor", 4, b.UNSIGNED_BYTE, !0), new u.WebGLAttribute("aKind", 1, b.FLOAT), new u.WebGLAttribute("aSampler", 1, b.FLOAT)]), h.attributeList.initialize(a));
        };
        h.prototype.writeTo = function(a) {
          a = a.array;
          a.ensureAdditionalCapacity(68);
          a.writeVertex3DUnsafe(this.x, this.y, this.z);
          a.writeVertexUnsafe(this.coordinate.x, this.coordinate.y);
          a.writeColorUnsafe(255 * this.color.r, 255 * this.color.g, 255 * this.color.b, 255 * this.color.a);
          a.writeFloatUnsafe(this.kind);
          a.writeFloatUnsafe(this.sampler);
        };
        return h;
      }(u.Vertex);
      u.WebGLCombinedBrushVertex = l;
      b = function(a) {
        function b(h, k, n) {
          void 0 === n && (n = null);
          a.call(this, h, k, n);
          this._blendMode = 1;
          this._program = h.createProgramFromFiles("combined.vert", "combined.frag");
          this._surfaces = [];
          l.initializeAttributeList(this._context);
        }
        __extends(b, a);
        b.prototype.reset = function() {
          this._surfaces = [];
          this._geometry.reset();
        };
        b.prototype.drawImage = function(a, k, l, m, e, g, d) {
          void 0 === g && (g = 0);
          void 0 === d && (d = 1);
          if (!a || !a.surface) {
            return !0;
          }
          k = k.clone();
          this._colorMatrix && (m && this._colorMatrix.equals(m) || this.flush());
          this._colorMatrix = m;
          this._blendMode !== d && (this.flush(), this._blendMode = d);
          d = this._surfaces.indexOf(a.surface);
          0 > d && (8 === this._surfaces.length && this.flush(), this._surfaces.push(a.surface), d = this._surfaces.length - 1);
          var c = b._tmpVertices, f = a.region.clone();
          f.offset(1, 1).resize(-2, -2);
          f.scale(1 / a.surface.w, 1 / a.surface.h);
          e.transformRectangle(k, c);
          for (a = 0;4 > a;a++) {
            c[a].z = g;
          }
          c[0].coordinate.x = f.x;
          c[0].coordinate.y = f.y;
          c[1].coordinate.x = f.x + f.w;
          c[1].coordinate.y = f.y;
          c[2].coordinate.x = f.x + f.w;
          c[2].coordinate.y = f.y + f.h;
          c[3].coordinate.x = f.x;
          c[3].coordinate.y = f.y + f.h;
          for (a = 0;4 > a;a++) {
            g = b._tmpVertices[a], g.kind = m ? 2 : 1, g.color.set(l), g.sampler = d, g.writeTo(this._geometry);
          }
          this._geometry.addQuad();
          return !0;
        };
        b.prototype.fillRectangle = function(a, k, l, m) {
          void 0 === m && (m = 0);
          l.transformRectangle(a, b._tmpVertices);
          for (a = 0;4 > a;a++) {
            l = b._tmpVertices[a], l.kind = 0, l.color.set(k), l.z = m, l.writeTo(this._geometry);
          }
          this._geometry.addQuad();
        };
        b.prototype.flush = function() {
          var a = this._geometry, b = this._program, h = this._context.gl, k;
          a.uploadBuffers();
          h.useProgram(b);
          this._target ? (k = m.create2DProjection(this._target.w, this._target.h, 2E3), k = m.createMultiply(k, m.createScale(1, -1, 1))) : k = this._context.modelViewProjectionMatrix;
          h.uniformMatrix4fv(b.uniforms.uTransformMatrix3D.location, !1, k.asWebGLMatrix());
          this._colorMatrix && (h.uniformMatrix4fv(b.uniforms.uColorMatrix.location, !1, this._colorMatrix.asWebGLMatrix()), h.uniform4fv(b.uniforms.uColorVector.location, this._colorMatrix.asWebGLVector()));
          for (k = 0;k < this._surfaces.length;k++) {
            h.activeTexture(h.TEXTURE0 + k), h.bindTexture(h.TEXTURE_2D, this._surfaces[k].texture);
          }
          h.uniform1iv(b.uniforms["uSampler[0]"].location, [0, 1, 2, 3, 4, 5, 6, 7]);
          h.bindBuffer(h.ARRAY_BUFFER, a.buffer);
          var e = l.attributeList.size, g = l.attributeList.attributes;
          for (k = 0;k < g.length;k++) {
            var d = g[k], c = b.attributes[d.name].location;
            h.enableVertexAttribArray(c);
            h.vertexAttribPointer(c, d.size, d.type, d.normalized, e, d.offset);
          }
          this._context.setBlendOptions();
          this._context.target = this._target;
          h.bindBuffer(h.ELEMENT_ARRAY_BUFFER, a.elementBuffer);
          h.drawElements(h.TRIANGLES, 3 * a.triangleCount, h.UNSIGNED_SHORT, 0);
          this.reset();
        };
        b._tmpVertices = u.Vertex.createEmptyVertices(l, 4);
        b._depth = 1;
        return b;
      }(b);
      u.WebGLCombinedBrush = b;
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(k) {
      var a = CanvasRenderingContext2D.prototype.save, p = CanvasRenderingContext2D.prototype.clip, m = CanvasRenderingContext2D.prototype.fill, b = CanvasRenderingContext2D.prototype.stroke, l = CanvasRenderingContext2D.prototype.restore, r = CanvasRenderingContext2D.prototype.beginPath;
      k.notifyReleaseChanged = function() {
        CanvasRenderingContext2D.prototype.save = a;
        CanvasRenderingContext2D.prototype.clip = p;
        CanvasRenderingContext2D.prototype.fill = m;
        CanvasRenderingContext2D.prototype.stroke = b;
        CanvasRenderingContext2D.prototype.restore = l;
        CanvasRenderingContext2D.prototype.beginPath = r;
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
  (function(p) {
    (function(u) {
      function a(a, b) {
        var k = a / 2;
        switch(b) {
          case 0:
            return 0;
          case 1:
            return k / 2.7;
          case 2:
            return k / 1.28;
          default:
            return k;
        }
      }
      function w(a) {
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
            k.Debug.somewhatImplemented("Blend Mode: " + p.BlendMode[a]);
        }
        return b;
      }
      var m = k.NumberUtilities.clamp;
      navigator.userAgent.indexOf("Firefox");
      var b = function() {
        function b() {
        }
        b._prepareSVGFilters = function() {
          if (!b._svgBlurFilter) {
            var a = document.createElementNS("http://www.w3.org/2000/svg", "svg");
            a.setAttribute("style", "display:block;width:0px;height:0px");
            var k = document.createElementNS("http://www.w3.org/2000/svg", "defs"), l = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            l.setAttribute("id", "svgBlurFilter");
            var m = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            m.setAttribute("stdDeviation", "0 0");
            l.appendChild(m);
            k.appendChild(l);
            b._svgBlurFilter = m;
            l = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            l.setAttribute("id", "svgDropShadowFilter");
            m = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            m.setAttribute("in", "SourceAlpha");
            m.setAttribute("stdDeviation", "3");
            l.appendChild(m);
            b._svgDropshadowFilterBlur = m;
            m = document.createElementNS("http://www.w3.org/2000/svg", "feOffset");
            m.setAttribute("dx", "0");
            m.setAttribute("dy", "0");
            m.setAttribute("result", "offsetblur");
            l.appendChild(m);
            b._svgDropshadowFilterOffset = m;
            m = document.createElementNS("http://www.w3.org/2000/svg", "feFlood");
            m.setAttribute("flood-color", "rgba(0,0,0,1)");
            l.appendChild(m);
            b._svgDropshadowFilterFlood = m;
            m = document.createElementNS("http://www.w3.org/2000/svg", "feComposite");
            m.setAttribute("in2", "offsetblur");
            m.setAttribute("operator", "in");
            l.appendChild(m);
            var m = document.createElementNS("http://www.w3.org/2000/svg", "feMerge"), e = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            m.appendChild(e);
            e = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            e.setAttribute("in", "SourceGraphic");
            m.appendChild(e);
            l.appendChild(m);
            k.appendChild(l);
            l = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            l.setAttribute("id", "svgColorMatrixFilter");
            m = document.createElementNS("http://www.w3.org/2000/svg", "feColorMatrix");
            m.setAttribute("color-interpolation-filters", "sRGB");
            m.setAttribute("in", "SourceGraphic");
            m.setAttribute("type", "matrix");
            l.appendChild(m);
            k.appendChild(l);
            b._svgColorMatrixFilter = m;
            a.appendChild(k);
            document.documentElement.appendChild(a);
          }
        };
        b._applyFilter = function(l, q, n) {
          if (b._svgFiltersAreSupported) {
            if (b._prepareSVGFilters(), b._removeFilter(q), n instanceof p.BlurFilter) {
              var m = a(l, n.quality);
              b._svgBlurFilter.setAttribute("stdDeviation", n.blurX * m + " " + n.blurY * m);
              q.filter = "url(#svgBlurFilter)";
            } else {
              n instanceof p.DropshadowFilter ? (m = a(l, n.quality), b._svgDropshadowFilterBlur.setAttribute("stdDeviation", n.blurX * m + " " + n.blurY * m), b._svgDropshadowFilterOffset.setAttribute("dx", String(Math.cos(n.angle * Math.PI / 180) * n.distance * l)), b._svgDropshadowFilterOffset.setAttribute("dy", String(Math.sin(n.angle * Math.PI / 180) * n.distance * l)), b._svgDropshadowFilterFlood.setAttribute("flood-color", k.ColorUtilities.rgbaToCSSStyle(n.color << 8 | Math.round(255 * n.alpha))), 
              q.filter = "url(#svgDropShadowFilter)") : n instanceof p.ColorMatrix && (b._svgColorMatrixFilter.setAttribute("values", n.toSVGFilterMatrix()), q.filter = "url(#svgColorMatrixFilter)");
            }
          }
        };
        b._removeFilter = function(a) {
          a.filter = "none";
        };
        b._applyColorMatrix = function(a, b) {
          b.isIdentity() ? (a.globalAlpha = 1, a.globalColorMatrix = null) : b.hasOnlyAlphaMultiplier() ? (a.globalAlpha = m(b.alphaMultiplier, 0, 1), a.globalColorMatrix = null) : (a.globalAlpha = 1, a.globalColorMatrix = b);
        };
        b._svgFiltersAreSupported = !!Object.getOwnPropertyDescriptor(CanvasRenderingContext2D.prototype, "filter");
        return b;
      }();
      u.Filters = b;
      p.filters && b._svgFiltersAreSupported && ("registerScratchCanvas" in window || (window.registerScratchCanvas = function(a) {
        a.style.display = "none";
        document.body.appendChild(a);
      }));
      var l = function() {
        function a(b, h, k, l) {
          this.surface = b;
          this.region = h;
          this.w = k;
          this.h = l;
        }
        a.prototype.free = function() {
          this.surface.free(this);
        };
        a._ensureCopyCanvasSize = function(b, l) {
          var n;
          if (a._copyCanvasContext) {
            if (n = a._copyCanvasContext.canvas, n.width < b || n.height < l) {
              n.width = k.IntegerUtilities.nearestPowerOfTwo(b), n.height = k.IntegerUtilities.nearestPowerOfTwo(l);
            }
          } else {
            n = document.createElement("canvas"), "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(n), n.width = k.IntegerUtilities.nearestPowerOfTwo(b), n.height = k.IntegerUtilities.nearestPowerOfTwo(l), a._copyCanvasContext = n.getContext("2d");
          }
        };
        a.prototype.draw = function(k, l, n, m, e, g, d, c, f) {
          this.context.setTransform(1, 0, 0, 1, 0, 0);
          var r, x = 0, p = 0;
          k.context.canvas === this.context.canvas ? (a._ensureCopyCanvasSize(m, e), r = a._copyCanvasContext, r.clearRect(0, 0, m, e), r.drawImage(k.surface.canvas, k.region.x, k.region.y, m, e, 0, 0, m, e), g = r, p = x = 0) : (g = k.surface.context, x = k.region.x, p = k.region.y);
          a: {
            switch(d) {
              case 11:
                k = !0;
                break a;
              default:
                k = !1;
            }
          }
          k && (this.context.save(), this.context.beginPath(), this.context.rect(l, n, m, e), this.context.clip());
          this.context.globalAlpha = 1;
          this.context.globalCompositeOperation = w(d);
          if (c) {
            d = 0;
            if (1 < c.length) {
              var u, V, U, S;
              r ? (U = r, r = g, g = U) : (a._ensureCopyCanvasSize(m, e), r = a._copyCanvasContext, V = u = 0);
              for (;d < c.length - 1;d++) {
                r.clearRect(0, 0, m, e), b._applyFilter(f, r, c[d]), r.drawImage(g.canvas, x, p, m, e, u, V, m, e), b._removeFilter(r), U = r, V = x, S = p, r = g, g = U, p = x = u, u = V, V = S;
              }
              b._removeFilter(g);
              b._removeFilter(r);
            }
            b._applyFilter(f, this.context, c[d]);
          }
          this.context.drawImage(g.canvas, x, p, m, e, l, n, m, e);
          this.context.globalCompositeOperation = w(1);
          b._removeFilter(this.context);
          k && this.context.restore();
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
          a.fillStyle = "#000000";
          a.strokeStyle = "#000000";
          a.globalAlpha = 1;
          a.globalColorMatrix = null;
          a.globalCompositeOperation = w(1);
        };
        a.prototype.fill = function(a) {
          var b = this.surface.context, h = this.region;
          b.fillStyle = a;
          b.fillRect(h.x, h.y, h.w, h.h);
        };
        a.prototype.clear = function(a) {
          var b = this.surface.context, h = this.region;
          a || (a = h);
          b.clearRect(a.x, a.y, a.w, a.h);
        };
        return a;
      }();
      u.Canvas2DSurfaceRegion = l;
      var r = function() {
        function a(b, h) {
          this.canvas = b;
          this.context = b.getContext("2d");
          this.w = b.width;
          this.h = b.height;
          this._regionAllocator = h;
        }
        a.prototype.allocate = function(a, b) {
          var h = this._regionAllocator.allocate(a, b);
          return h ? new l(this, h, a, b) : null;
        };
        a.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return a;
      }();
      u.Canvas2DSurface = r;
    })(p.Canvas2D || (p.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.Debug.assert, w = k.GFX.Geometry.Rectangle, m = k.GFX.Geometry.Point, b = k.GFX.Geometry.Matrix, l = k.NumberUtilities.clamp, r = k.NumberUtilities.pow2, h = function() {
        return function(a, b) {
          this.surfaceRegion = a;
          this.scale = b;
        };
      }();
      u.MipMapLevel = h;
      var t = function() {
        function a(b, c, f, e) {
          this._node = c;
          this._levels = [];
          this._surfaceRegionAllocator = f;
          this._size = e;
          this._renderer = b;
        }
        a.prototype.getLevel = function(a) {
          a = Math.max(a.getAbsoluteScaleX(), a.getAbsoluteScaleY());
          var b = 0;
          1 !== a && (b = l(Math.round(Math.log(a) / Math.LN2), -5, 3));
          this._node.hasFlags(2097152) || (b = l(b, -5, 0));
          a = r(b);
          var f = 5 + b, b = this._levels[f];
          if (!b) {
            var e = this._node.getBounds().clone();
            e.scale(a, a);
            e.snap();
            var g = this._surfaceRegionAllocator.allocate(e.w, e.h, null), k = g.region, b = this._levels[f] = new h(g, a), f = new n(g);
            f.clip.set(k);
            f.matrix.setElements(a, 0, 0, a, k.x - e.x, k.y - e.y);
            f.flags |= 64;
            this._renderer.renderNodeWithState(this._node, f);
            f.free();
          }
          return b;
        };
        return a;
      }();
      u.MipMap = t;
      (function(a) {
        a[a.NonZero = 0] = "NonZero";
        a[a.EvenOdd = 1] = "EvenOdd";
      })(u.FillRule || (u.FillRule = {}));
      var q = function(a) {
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
      }(p.RendererOptions);
      u.Canvas2DRendererOptions = q;
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
      })(u.RenderFlags || (u.RenderFlags = {}));
      w.createMaxI16();
      var n = function(a) {
        function d(c) {
          a.call(this);
          this.clip = w.createEmpty();
          this.clipList = [];
          this.clipPath = null;
          this.flags = 0;
          this.target = null;
          this.matrix = b.createIdentity();
          this.colorMatrix = p.ColorMatrix.createIdentity();
          d.allocationCount++;
          this.target = c;
        }
        __extends(d, a);
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
      }(p.State);
      u.RenderState = n;
      var v = function() {
        function a() {
          this.culledNodes = this.groups = this.shapes = this._count = 0;
        }
        a.prototype.enter = function(a) {
          this._count++;
        };
        a.prototype.leave = function() {
        };
        return a;
      }();
      u.FrameInfo = v;
      var e = function(e) {
        function d(a, b, h) {
          void 0 === h && (h = new q);
          e.call(this, a, b, h);
          this._visited = 0;
          this._frameInfo = new v;
          this._fontSize = 0;
          this._layers = [];
          if (a instanceof HTMLCanvasElement) {
            var k = a;
            this._viewport = new w(0, 0, k.width, k.height);
            this._target = this._createTarget(k);
          } else {
            this._addLayer("Background Layer");
            h = this._addLayer("Canvas Layer");
            k = document.createElement("canvas");
            h.appendChild(k);
            this._viewport = new w(0, 0, a.scrollWidth, a.scrollHeight);
            var l = this;
            b.addEventListener(1, function() {
              l._onStageBoundsChanged(k);
            });
            this._onStageBoundsChanged(k);
          }
          d._prepareSurfaceAllocators();
        }
        __extends(d, e);
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
          return new u.Canvas2DSurfaceRegion(new u.Canvas2DSurface(a), new p.RegionAllocator.Region(0, 0, a.width, a.height), a.width, a.height);
        };
        d.prototype._onStageBoundsChanged = function(a) {
          var b = this._stage.getBounds(!0);
          b.snap();
          for (var d = this._devicePixelRatio = window.devicePixelRatio || 1, e = b.w / d + "px", d = b.h / d + "px", g = 0;g < this._layers.length;g++) {
            var h = this._layers[g];
            h.style.width = e;
            h.style.height = d;
          }
          a.width = b.w;
          a.height = b.h;
          a.style.position = "absolute";
          a.style.width = e;
          a.style.height = d;
          this._target = this._createTarget(a);
          this._fontSize = 10 * this._devicePixelRatio;
        };
        d._prepareSurfaceAllocators = function() {
          d._initializedCaches || (d._surfaceCache = new p.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var d = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(d);
            var e = Math.max(1024, a), g = Math.max(1024, b);
            d.width = e;
            d.height = g;
            var h = null, h = 512 <= a || 512 <= b ? new p.RegionAllocator.GridAllocator(e, g, e, g) : new p.RegionAllocator.BucketAllocator(e, g);
            return new u.Canvas2DSurface(d, h);
          }), d._shapeCache = new p.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var d = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(d);
            d.width = 1024;
            d.height = 1024;
            var e = e = new p.RegionAllocator.CompactAllocator(1024, 1024);
            return new u.Canvas2DSurface(d, e);
          }), d._initializedCaches = !0);
        };
        d.prototype.render = function() {
          var a = this._stage, b = this._target, d = this._options, e = this._viewport;
          b.reset();
          b.context.save();
          b.context.beginPath();
          b.context.rect(e.x, e.y, e.w, e.h);
          b.context.clip();
          this._renderStageToTarget(b, a, e);
          b.reset();
          d.paintViewport && (b.context.beginPath(), b.context.rect(e.x, e.y, e.w, e.h), b.context.strokeStyle = "#FF4981", b.context.lineWidth = 2, b.context.stroke());
          b.context.restore();
        };
        d.prototype.renderNode = function(a, b, d) {
          var e = new n(this._target);
          e.clip.set(b);
          e.flags = 256;
          e.matrix.set(d);
          a.visit(this, e);
          e.free();
        };
        d.prototype.renderNodeWithState = function(a, b) {
          a.visit(this, b);
        };
        d.prototype._renderWithCache = function(a, b) {
          var e = b.matrix, g = a.getBounds();
          if (g.isEmpty()) {
            return !1;
          }
          var h = this._options.cacheShapesMaxSize, k = Math.max(e.getAbsoluteScaleX(), e.getAbsoluteScaleY()), l = !!(b.flags & 16), n = !!(b.flags & 8);
          if (b.hasFlags(256)) {
            if (n || l || !b.colorMatrix.isIdentity() || a.hasFlags(1048576) || 100 < this._options.cacheShapesThreshold || g.w * k > h || g.h * k > h) {
              return !1;
            }
            (k = a.properties.mipMap) || (k = a.properties.mipMap = new t(this, a, d._shapeCache, h));
            l = k.getLevel(e);
            h = l.surfaceRegion;
            k = h.region;
            return l ? (l = b.target.context, l.imageSmoothingEnabled = l.mozImageSmoothingEnabled = !0, l.setTransform(e.a, e.b, e.c, e.d, e.tx, e.ty), l.drawImage(h.surface.canvas, k.x, k.y, k.w, k.h, g.x, g.y, g.w, g.h), !0) : !1;
          }
        };
        d.prototype._intersectsClipList = function(a, b) {
          var d = a.getBounds(!0), e = !1;
          b.matrix.transformRectangleAABB(d);
          b.clip.intersects(d) && (e = !0);
          var g = b.clipList;
          if (e && g.length) {
            for (var e = !1, h = 0;h < g.length;h++) {
              if (d.intersects(g[h])) {
                e = !0;
                break;
              }
            }
          }
          d.free();
          return e;
        };
        d.prototype.visitGroup = function(a, b) {
          this._frameInfo.groups++;
          a.getBounds();
          if ((!a.hasFlags(4) || b.flags & 4) && a.hasFlags(65536)) {
            var d = b.flags & 1;
            if (!d && ((1 !== a.getLayer().blendMode || a.getLayer().mask) && this._options.blending || a.getLayer().filters && this._options.filters)) {
              b = b.clone(), b.flags |= 1, this._renderLayer(a, b), b.free();
            } else {
              if (d && b.removeFlags(1), this._intersectsClipList(a, b)) {
                for (var d = null, e = a.getChildren(), g = 0;g < e.length;g++) {
                  var h = e[g], k = h.getTransform(), l = b.transform(k);
                  l.toggleFlags(4096, h.hasFlags(524288));
                  if (0 <= h.clip) {
                    d = d || new Uint8Array(e.length);
                    d[h.clip + g]++;
                    var n = l.clone();
                    n.flags |= 16;
                    n.beginClipPath(k.getMatrix());
                    h.visit(this, n);
                    n.applyClipPath();
                    n.free();
                  } else {
                    h.visit(this, l);
                  }
                  if (d && 0 < d[g]) {
                    for (;d[g]--;) {
                      b.closeClipPath();
                    }
                  }
                  l.free();
                }
              } else {
                this._frameInfo.culledNodes++;
              }
            }
            this._renderDebugInfo(a, b);
          }
        };
        d.prototype._renderDebugInfo = function(a, b) {
          if (b.flags & 1024) {
            var e = b.target.context, g = a.getBounds(!0), h = a.properties.style;
            h || (h = a.properties.style = k.Color.randomColor(.4).toCSSStyle());
            e.strokeStyle = h;
            b.matrix.transformRectangleAABB(g);
            e.setTransform(1, 0, 0, 1, 0, 0);
            g.free();
            g = a.getBounds();
            h = d._debugPoints;
            b.matrix.transformRectangle(g, h);
            e.lineWidth = 1;
            e.beginPath();
            e.moveTo(h[0].x, h[0].y);
            e.lineTo(h[1].x, h[1].y);
            e.lineTo(h[2].x, h[2].y);
            e.lineTo(h[3].x, h[3].y);
            e.lineTo(h[0].x, h[0].y);
            e.stroke();
          }
        };
        d.prototype.visitStage = function(a, b) {
          var d = b.target.context, e = a.getBounds(!0);
          b.matrix.transformRectangleAABB(e);
          e.intersect(b.clip);
          b.target.reset();
          b = b.clone();
          this._options.clear && b.target.clear(b.clip);
          a.hasFlags(32768) || !a.color || b.flags & 32 || (this._container.style.backgroundColor = a.color.toCSSStyle());
          this.visitGroup(a, b);
          a.dirtyRegion && (d.restore(), b.target.reset(), d.globalAlpha = .4, b.hasFlags(2048) && a.dirtyRegion.render(b.target.context), a.dirtyRegion.clear());
          b.free();
        };
        d.prototype.visitShape = function(a, b) {
          if (this._intersectsClipList(a, b)) {
            var d = b.matrix;
            b.flags & 8192 && (d = d.clone(), d.snap());
            var e = b.target.context;
            u.Filters._applyColorMatrix(e, b.colorMatrix);
            a.source instanceof p.RenderableVideo ? this.visitRenderableVideo(a.source, b) : 0 < e.globalAlpha && this.visitRenderable(a.source, b, a.ratio);
            b.flags & 8192 && d.free();
            u.Filters._removeFilter(e);
          }
        };
        d.prototype.visitRenderableVideo = function(a, b) {
          if (a.video && a.video.videoWidth) {
            var d = this._devicePixelRatio, e = b.matrix.clone();
            e.scale(1 / d, 1 / d);
            var d = a.getBounds(), g = k.GFX.Geometry.Matrix.createIdentity();
            g.scale(d.w / a.video.videoWidth, d.h / a.video.videoHeight);
            e.preMultiply(g);
            g.free();
            d = e.toCSSTransform();
            a.video.style.transformOrigin = "0 0";
            a.video.style.transform = d;
            var h = this._backgroundVideoLayer;
            h !== a.video.parentElement && (h.appendChild(a.video), a.addEventListener(2, function U(a) {
              h.removeChild(a.video);
              a.removeEventListener(2, U);
            }));
            e.free();
          }
        };
        d.prototype.visitRenderable = function(a, b, d) {
          var e = a.getBounds();
          if (!(b.flags & 32 || e.isEmpty())) {
            if (b.hasFlags(64)) {
              b.removeFlags(64);
            } else {
              if (this._renderWithCache(a, b)) {
                return;
              }
            }
            var g = b.matrix, e = b.target.context, h = !!(b.flags & 16), k = !!(b.flags & 8);
            e.setTransform(g.a, g.b, g.c, g.d, g.tx, g.ty);
            this._frameInfo.shapes++;
            e.imageSmoothingEnabled = e.mozImageSmoothingEnabled = b.hasFlags(4096);
            g = a.properties.renderCount || 0;
            a.properties.renderCount = ++g;
            a.render(e, d, null, h ? b.clipPath : null, k);
          }
        };
        d.prototype._renderLayer = function(a, b) {
          var d = a.getLayer(), e = d.mask;
          if (e) {
            var g = !a.hasFlags(131072) || !e.hasFlags(131072);
            this._renderWithMask(a, e, d.blendMode, g, b);
          } else {
            e = w.allocate();
            if (g = this._renderToTemporarySurface(a, a.getLayerBounds(!!this._options.filters), b, e, b.target.surface)) {
              b.target.draw(g, e.x, e.y, e.w, e.h, b.colorMatrix, this._options.blending ? d.blendMode : 1, this._options.filters ? d.filters : null, this._devicePixelRatio), g.free();
            }
            e.free();
          }
        };
        d.prototype._renderWithMask = function(a, b, d, e, g) {
          var h = b.getTransform().getConcatenatedMatrix(!0);
          b.parent || (h = h.scale(this._devicePixelRatio, this._devicePixelRatio));
          var k = a.getBounds().clone();
          g.matrix.transformRectangleAABB(k);
          k.snap();
          if (!k.isEmpty()) {
            var l = b.getBounds().clone();
            h.transformRectangleAABB(l);
            l.snap();
            if (!l.isEmpty()) {
              var n = g.clip.clone();
              n.intersect(k);
              n.intersect(l);
              n.snap();
              n.isEmpty() || (k = g.clone(), k.clip.set(n), a = this._renderToTemporarySurface(a, a.getBounds(), k, w.createEmpty(), null), k.free(), l = g.clone(), l.clip.set(n), l.matrix = h, l.flags |= 4, e && (l.flags |= 8), b = this._renderToTemporarySurface(b, b.getBounds(), l, w.createEmpty(), a.surface), l.free(), a.draw(b, 0, 0, n.w, n.h, l.colorMatrix, 11, null, this._devicePixelRatio), g.target.draw(a, n.x, n.y, n.w, n.h, k.colorMatrix, d, null, this._devicePixelRatio), b.free(), a.free());
            }
          }
        };
        d.prototype._renderStageToTarget = function(a, d, e) {
          w.allocationCount = b.allocationCount = n.allocationCount = 0;
          a = new n(a);
          a.clip.set(e);
          this._options.paintRenderable || (a.flags |= 32);
          this._options.paintBounds && (a.flags |= 1024);
          this._options.paintDirtyRegion && (a.flags |= 2048);
          this._options.paintFlashing && (a.flags |= 512);
          this._options.cacheShapes && (a.flags |= 256);
          this._options.imageSmoothing && (a.flags |= 4096);
          this._options.snapToDevicePixels && (a.flags |= 8192);
          this._frameInfo.enter(a);
          d.visit(this, a);
          this._frameInfo.leave();
        };
        d.prototype._renderToTemporarySurface = function(a, b, d, e, g) {
          var h = d.matrix;
          b = b.clone();
          h.transformRectangleAABB(b);
          b.snap();
          e.set(b);
          e.intersect(d.clip);
          e.snap();
          if (e.isEmpty()) {
            return null;
          }
          g = this._allocateSurface(e.w, e.h, g);
          b = g.region;
          b = new w(b.x, b.y, e.w, e.h);
          g.context.setTransform(1, 0, 0, 1, 0, 0);
          g.clear();
          h = h.clone();
          h.translate(b.x - e.x, b.y - e.y);
          g.context.save();
          d = d.clone();
          d.target = g;
          d.matrix = h;
          d.clip.set(b);
          a.visit(this, d);
          d.free();
          g.context.restore();
          return g;
        };
        d.prototype._allocateSurface = function(a, b, e) {
          return d._surfaceCache.allocate(a, b, e);
        };
        d.prototype.screenShot = function(b, d, e) {
          d && (d = this._stage.content.groupChild.child, a(d instanceof p.Stage), b = d.content.getBounds(!0), d.content.getTransform().getConcatenatedMatrix().transformRectangleAABB(b), b.intersect(this._viewport));
          b || (b = new w(0, 0, this._target.w, this._target.h));
          d = b.w;
          var g = b.h, h = this._devicePixelRatio;
          e && (d /= h, g /= h, h = 1);
          e = document.createElement("canvas");
          e.width = d;
          e.height = g;
          var k = e.getContext("2d");
          k.fillStyle = this._container.style.backgroundColor;
          k.fillRect(0, 0, d, g);
          k.drawImage(this._target.context.canvas, b.x, b.y, b.w, b.h, 0, 0, d, g);
          return new p.ScreenShot(e.toDataURL("image/png"), d, g, h);
        };
        d._initializedCaches = !1;
        d._debugPoints = m.createEmptyPoints(4);
        return d;
      }(p.Renderer);
      u.Canvas2DRenderer = e;
    })(p.Canvas2D || (p.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var u = p.Geometry.Point, a = p.Geometry.Matrix, w = p.Geometry.Rectangle, m = k.Tools.Mini.FPS, b = function() {
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
    p.UIState = b;
    var l = function(a) {
      function b() {
        a.apply(this, arguments);
        this._keyCodes = [];
      }
      __extends(b, a);
      b.prototype.onMouseDown = function(a, b) {
        b.altKey && (a.state = new h(a.worldView, a.getMousePosition(b, null), a.worldView.getTransform().getMatrix(!0)));
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
    }(b), r = function(a) {
      function b() {
        a.apply(this, arguments);
        this._keyCodes = [];
        this._paused = !1;
        this._mousePosition = new u(0, 0);
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
          var g = a.getMousePosition(b, null), d = a.worldView.getTransform().getMatrix(!0), e = 1 + e / 1E3;
          d.translate(-g.x, -g.y);
          d.scale(e, e);
          d.translate(g.x, g.y);
          a.worldView.getTransform().setMatrix(d);
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
        if (a.getOption("paintViewport")) {
          var b = p.viewportLoupeDiameter.value, e = p.viewportLoupeDiameter.value;
          a.viewport = new w(this._mousePosition.x - b / 2, this._mousePosition.y - e / 2, b, e);
        } else {
          a.viewport = null;
        }
      };
      return b;
    }(b);
    (function(a) {
      function b() {
        a.apply(this, arguments);
        this._startTime = Date.now();
      }
      __extends(b, a);
      b.prototype.onMouseMove = function(a, b) {
        if (!(10 > Date.now() - this._startTime)) {
          var e = a._world;
          e && (a.state = new h(e, a.getMousePosition(b, null), e.getTransform().getMatrix(!0)));
        }
      };
      b.prototype.onMouseUp = function(a, b) {
        a.state = new l;
        a.selectNodeUnderMouse(b);
      };
      return b;
    })(b);
    var h = function(a) {
      function b(h, k, e) {
        a.call(this);
        this._target = h;
        this._startPosition = k;
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
        a.state = new l;
      };
      return b;
    }(b), b = function() {
      function b(a, h, t) {
        function e(a) {
          c._state.onMouseWheel(c, a);
          c._persistentState.onMouseWheel(c, a);
        }
        void 0 === h && (h = !1);
        void 0 === t && (t = void 0);
        this._state = new l;
        this._persistentState = new r;
        this.paused = !1;
        this.viewport = null;
        this._selectedNodes = [];
        this._isRendering = !1;
        this._rAF = void 0;
        this._eventListeners = Object.create(null);
        this._fullScreen = !1;
        this._container = a;
        this._stage = new p.Stage(512, 512, !0);
        this._worldView = this._stage.content;
        this._world = new p.Group;
        this._worldView.addChild(this._world);
        this._disableHiDPI = h;
        h = document.createElement("div");
        h.style.position = "absolute";
        h.style.width = "100%";
        h.style.height = "100%";
        h.style.zIndex = "0";
        a.appendChild(h);
        if (p.hud.value) {
          var g = document.createElement("div");
          g.style.position = "absolute";
          g.style.width = "100%";
          g.style.height = "100%";
          g.style.pointerEvents = "none";
          var d = document.createElement("div");
          d.style.position = "absolute";
          d.style.width = "100%";
          d.style.height = "20px";
          d.style.pointerEvents = "none";
          g.appendChild(d);
          a.appendChild(g);
          this._fps = new m(d);
        } else {
          this._fps = null;
        }
        this.transparent = g = 0 === t;
        void 0 === t || 0 === t || k.ColorUtilities.rgbaToCSSStyle(t);
        this._options = new p.Canvas2D.Canvas2DRendererOptions;
        this._options.alpha = g;
        this._renderer = new p.Canvas2D.Canvas2DRenderer(h, this._stage, this._options);
        this._listenForContainerSizeChanges();
        this._onMouseUp = this._onMouseUp.bind(this);
        this._onMouseDown = this._onMouseDown.bind(this);
        this._onMouseMove = this._onMouseMove.bind(this);
        var c = this;
        window.addEventListener("mouseup", function(a) {
          c._state.onMouseUp(c, a);
          c._render();
        }, !1);
        window.addEventListener("mousemove", function(a) {
          c._state.onMouseMove(c, a);
          c._persistentState.onMouseMove(c, a);
        }, !1);
        window.addEventListener("DOMMouseScroll", e);
        window.addEventListener("mousewheel", e);
        a.addEventListener("mousedown", function(a) {
          c._state.onMouseDown(c, a);
        });
        window.addEventListener("keydown", function(a) {
          c._state.onKeyDown(c, a);
          if (c._state !== c._persistentState) {
            c._persistentState.onKeyDown(c, a);
          }
        }, !1);
        window.addEventListener("keypress", function(a) {
          c._state.onKeyPress(c, a);
          if (c._state !== c._persistentState) {
            c._persistentState.onKeyPress(c, a);
          }
        }, !1);
        window.addEventListener("keyup", function(a) {
          c._state.onKeyUp(c, a);
          if (c._state !== c._persistentState) {
            c._persistentState.onKeyUp(c, a);
          }
        }, !1);
      }
      b.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var h = this;
        setInterval(function() {
          if (a !== h._containerWidth || b !== h._containerHeight) {
            h._onContainerSizeChanged(), a = h._containerWidth, b = h._containerHeight;
          }
        }, 10);
      };
      b.prototype._onContainerSizeChanged = function() {
        var b = this.getRatio(), h = Math.ceil(this._containerWidth * b), k = Math.ceil(this._containerHeight * b);
        this._stage.setBounds(new w(0, 0, h, k));
        this._stage.content.setBounds(new w(0, 0, h, k));
        this._worldView.getTransform().setMatrix(new a(b, 0, 0, b, 0, 0));
        this._dispatchEvent("resize");
      };
      b.prototype.addEventListener = function(a, b) {
        this._eventListeners[a] || (this._eventListeners[a] = []);
        this._eventListeners[a].push(b);
      };
      b.prototype._dispatchEvent = function(a) {
        if (a = this._eventListeners[a]) {
          for (var b = 0;b < a.length;b++) {
            a[b]();
          }
        }
      };
      b.prototype.startRendering = function() {
        if (!this._isRendering) {
          this._isRendering = !0;
          var a = this;
          this._rAF = requestAnimationFrame(function v() {
            a.render();
            a._rAF = requestAnimationFrame(v);
          });
        }
      };
      b.prototype.stopRendering = function() {
        this._isRendering && (this._isRendering = !1, cancelAnimationFrame(this._rAF));
      };
      Object.defineProperty(b.prototype, "state", {set:function(a) {
        this._state = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "cursor", {set:function(a) {
        this._container.style.cursor = a;
      }, enumerable:!0, configurable:!0});
      b.prototype._render = function() {
        p.RenderableVideo.checkForVideoUpdates();
        var a = (this._stage.readyToRender() || p.forcePaint.value) && !this.paused, b = 0;
        if (a) {
          var h = this._renderer;
          h.viewport = this.viewport ? this.viewport : this._stage.getBounds();
          this._dispatchEvent("render");
          b = performance.now();
          h.render();
          b = performance.now() - b;
        }
        this._fps && this._fps.tickAndRender(!a, b);
      };
      b.prototype.render = function() {
        this._render();
      };
      Object.defineProperty(b.prototype, "world", {get:function() {
        return this._world;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "worldView", {get:function() {
        return this._worldView;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "stage", {get:function() {
        return this._stage;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "options", {get:function() {
        return this._options;
      }, enumerable:!0, configurable:!0});
      b.prototype.getDisplayParameters = function() {
        var a = this.getRatio();
        return {stageWidth:this._containerWidth, stageHeight:this._containerHeight, pixelRatio:a, screenWidth:window.screen ? window.screen.width : 640, screenHeight:window.screen ? window.screen.height : 480};
      };
      b.prototype.toggleOption = function(a) {
        var b = this._options;
        b[a] = !b[a];
      };
      b.prototype.getOption = function(a) {
        return this._options[a];
      };
      b.prototype.getRatio = function() {
        var a = window.devicePixelRatio || 1, b = 1;
        1 === a || this._disableHiDPI || (b = a / 1);
        return b;
      };
      Object.defineProperty(b.prototype, "_containerWidth", {get:function() {
        return this._container.clientWidth;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "_containerHeight", {get:function() {
        return this._container.clientHeight;
      }, enumerable:!0, configurable:!0});
      b.prototype.queryNodeUnderMouse = function(a) {
        return this._world;
      };
      b.prototype.selectNodeUnderMouse = function(a) {
        (a = this._world) && this._selectedNodes.push(a);
        this._render();
      };
      b.prototype.getMousePosition = function(b, h) {
        var k = this._container, e = k.getBoundingClientRect(), g = this.getRatio(), k = new u(k.scrollWidth / e.width * (b.clientX - e.left) * g, k.scrollHeight / e.height * (b.clientY - e.top) * g);
        if (!h) {
          return k;
        }
        e = a.createIdentity();
        h.getTransform().getConcatenatedMatrix().inverse(e);
        e.transformPoint(k);
        return k;
      };
      b.prototype.getMouseWorldPosition = function(a) {
        return this.getMousePosition(a, this._world);
      };
      b.prototype._onMouseDown = function(a) {
      };
      b.prototype._onMouseUp = function(a) {
      };
      b.prototype._onMouseMove = function(a) {
      };
      b.prototype.screenShot = function(a, b, h) {
        return this._renderer.screenShot(a, b, h);
      };
      return b;
    }();
    p.Easel = b;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var u = k.GFX.Geometry.Matrix;
    (function(a) {
      a[a.Simple = 0] = "Simple";
    })(p.Layout || (p.Layout = {}));
    var a = function(a) {
      function b() {
        a.apply(this, arguments);
        this.layout = 0;
      }
      __extends(b, a);
      return b;
    }(p.RendererOptions);
    p.TreeRendererOptions = a;
    var w = function(k) {
      function b(b, r, h) {
        void 0 === h && (h = new a);
        k.call(this, b, r, h);
        this._canvas = document.createElement("canvas");
        this._container.appendChild(this._canvas);
        this._context = this._canvas.getContext("2d");
        this._listenForContainerSizeChanges();
      }
      __extends(b, k);
      b.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var h = this;
        setInterval(function() {
          if (a !== h._containerWidth || b !== h._containerHeight) {
            h._onContainerSizeChanged(), a = h._containerWidth, b = h._containerHeight;
          }
        }, 10);
      };
      b.prototype._getRatio = function() {
        var a = window.devicePixelRatio || 1, b = 1;
        1 !== a && (b = a / 1);
        return b;
      };
      b.prototype._onContainerSizeChanged = function() {
        var a = this._getRatio(), b = Math.ceil(this._containerWidth * a), h = Math.ceil(this._containerHeight * a), k = this._canvas;
        0 < a ? (k.width = b * a, k.height = h * a, k.style.width = b + "px", k.style.height = h + "px") : (k.width = b, k.height = h);
      };
      Object.defineProperty(b.prototype, "_containerWidth", {get:function() {
        return this._container.clientWidth;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "_containerHeight", {get:function() {
        return this._container.clientHeight;
      }, enumerable:!0, configurable:!0});
      b.prototype.render = function() {
        var a = this._context;
        a.save();
        a.clearRect(0, 0, this._canvas.width, this._canvas.height);
        a.scale(1, 1);
        0 === this._options.layout && this._renderNodeSimple(this._context, this._stage, u.createIdentity());
        a.restore();
      };
      b.prototype._renderNodeSimple = function(a, b, h) {
        function k(b) {
          var c = b.getChildren();
          b.hasFlags(16) ? a.fillStyle = "red" : a.fillStyle = "white";
          var f = String(b.id);
          b instanceof p.RenderableText ? f = "T" + f : b instanceof p.RenderableShape ? f = "S" + f : b instanceof p.RenderableBitmap ? f = "B" + f : b instanceof p.RenderableVideo && (f = "V" + f);
          b instanceof p.Renderable && (f = f + " [" + b._parents.length + "]");
          b = a.measureText(f).width;
          a.fillText(f, n, u);
          if (c) {
            n += b + 4;
            g = Math.max(g, n + 20);
            for (f = 0;f < c.length;f++) {
              k(c[f]), f < c.length - 1 && (u += 18, u > m._canvas.height && (a.fillStyle = "gray", n = n - e + g + 8, e = g + 8, u = 0, a.fillStyle = "white"));
            }
            n -= b + 4;
          }
        }
        var m = this;
        a.save();
        a.font = "16px Arial";
        a.fillStyle = "white";
        var n = 0, u = 0, e = 0, g = 0;
        k(b);
        a.restore();
      };
      return b;
    }(p.Renderer);
    p.TreeRenderer = w;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.GFX.BlurFilter, w = k.GFX.DropshadowFilter, m = k.GFX.Shape, b = k.GFX.Group, l = k.GFX.RenderableShape, r = k.GFX.RenderableMorphShape, h = k.GFX.RenderableBitmap, t = k.GFX.RenderableVideo, q = k.GFX.RenderableText, n = k.GFX.ColorMatrix, v = k.ShapeData, e = k.ArrayUtilities.DataBuffer, g = k.GFX.Stage, d = k.GFX.Geometry.Matrix, c = k.GFX.Geometry.Rectangle, f = function() {
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
      u.GFXChannelSerializer = f;
      f = function() {
        function a(b, c, d) {
          function e(a) {
            a = a.getBounds(!0);
            var c = b.easel.getRatio();
            a.scale(1 / c, 1 / c);
            a.snap();
            f.setBounds(a);
          }
          var f = this.stage = new g(128, 512);
          "undefined" !== typeof registerInspectorStage && registerInspectorStage(f);
          e(b.stage);
          b.stage.addEventListener(1, e);
          b.content = f.content;
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
          k.registerCSSFont(a, b, !inFirefox);
          inFirefox ? c(null) : window.setTimeout(c, 400);
        };
        a.prototype.registerImage = function(a, b, c, d, e, f) {
          this._registerAsset(a, b, this._decodeImage(c, d, e, f));
        };
        a.prototype.registerVideo = function(a) {
          this._registerAsset(a, 0, new t(a, this));
        };
        a.prototype._decodeImage = function(a, b, d, e) {
          var f = new Image, g = h.FromImage(f, -1, -1);
          f.src = URL.createObjectURL(new Blob([b], {type:k.getMIMETypeForImageType(a)}));
          f.onload = function() {
            g.setBounds(new c(0, 0, f.width, f.height));
            d && g.mask(d);
            g.invalidate();
            e({width:f.width, height:f.height});
          };
          f.onerror = function() {
            e(null);
          };
          return g;
        };
        a.prototype.sendVideoPlaybackEvent = function(a, b, c) {
          this._easelHost.sendVideoPlaybackEvent(a, b, c);
        };
        return a;
      }();
      u.GFXChannelDeserializerContext = f;
      f = function() {
        function f() {
        }
        f.prototype.read = function() {
          for (var a = 0, b = this.input, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0, k = 0, l = 0;0 < b.bytesAvailable;) {
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
                k++;
                this._readDrawToBitmap();
                break;
              case 106:
                l++, this._readRequestBitmapData();
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
          var a = this.input, b = f._temporaryReadColorMatrix, c = 1, d = 1, e = 1, g = 1, h = 0, k = 0, l = 0, m = 0;
          switch(a.readInt()) {
            case 0:
              return f._temporaryReadColorMatrixIdentity;
            case 1:
              g = a.readFloat();
              break;
            case 2:
              c = a.readFloat(), d = a.readFloat(), e = a.readFloat(), g = a.readFloat(), h = a.readInt(), k = a.readInt(), l = a.readInt(), m = a.readInt();
          }
          b.setMultipliersAndOffsets(c, d, e, g, h, k, l, m);
          return b;
        };
        f.prototype._readAsset = function() {
          var a = this.input.readInt(), b = this.inputAssets[a];
          this.inputAssets[a] = null;
          return b;
        };
        f.prototype._readUpdateGraphics = function() {
          for (var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getAsset(c), f = this._readRectangle(), g = v.FromPlainObject(this._readAsset()), h = a.readInt(), k = [], m = 0;m < h;m++) {
            var n = a.readInt();
            k.push(b._getBitmapAsset(n));
          }
          if (e) {
            e.update(g, k, f);
          } else {
            a = g.morphCoordinates ? new r(c, g, k, f) : new l(c, g, k, f);
            for (m = 0;m < k.length;m++) {
              k[m] && k[m].addRenderableParent(a);
            }
            b._registerAsset(c, d, a);
          }
        };
        f.prototype._readUpdateBitmapData = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), f = b._getBitmapAsset(c), g = this._readRectangle(), a = a.readInt(), k = e.FromPlainObject(this._readAsset());
          f ? f.updateFromDataBuffer(a, k) : (f = h.FromDataBuffer(a, k, g), b._registerAsset(c, d, f));
        };
        f.prototype._readUpdateTextContent = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), f = b._getTextAsset(c), g = this._readRectangle(), h = this._readMatrix(), k = a.readInt(), l = a.readInt(), m = a.readInt(), n = a.readBoolean(), p = a.readInt(), r = a.readInt(), t = this._readAsset(), u = e.FromPlainObject(this._readAsset()), v = null, w = a.readInt();
          w && (v = new e(4 * w), a.readBytes(v, 0, 4 * w));
          f ? (f.setBounds(g), f.setContent(t, u, h, v), f.setStyle(k, l, p, r), f.reflow(m, n)) : (f = new q(g), f.setContent(t, u, h, v), f.setStyle(k, l, p, r), f.reflow(m, n), b._registerAsset(c, d, f));
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
        f.prototype._readFilters = function(b) {
          var c = this.input, d = c.readInt(), e = [];
          if (d) {
            for (var f = 0;f < d;f++) {
              var g = c.readInt();
              switch(g) {
                case 0:
                  e.push(new a(c.readFloat(), c.readFloat(), c.readInt()));
                  break;
                case 1:
                  e.push(new w(c.readFloat(), c.readFloat(), c.readFloat(), c.readFloat(), c.readInt(), c.readFloat(), c.readBoolean(), c.readBoolean(), c.readBoolean(), c.readInt(), c.readFloat()));
                  break;
                case 2:
                  g = new Float32Array(20);
                  g[0] = c.readFloat();
                  g[4] = c.readFloat();
                  g[8] = c.readFloat();
                  g[12] = c.readFloat();
                  g[16] = c.readFloat() / 255;
                  g[1] = c.readFloat();
                  g[5] = c.readFloat();
                  g[9] = c.readFloat();
                  g[13] = c.readFloat();
                  g[17] = c.readFloat() / 255;
                  g[2] = c.readFloat();
                  g[6] = c.readFloat();
                  g[10] = c.readFloat();
                  g[14] = c.readFloat();
                  g[18] = c.readFloat() / 255;
                  g[3] = c.readFloat();
                  g[7] = c.readFloat();
                  g[11] = c.readFloat();
                  g[15] = c.readFloat();
                  g[19] = c.readFloat() / 255;
                  e.push(new n(g));
                  break;
                default:
                  k.Debug.somewhatImplemented(p.FilterType[g]);
              }
            }
            b.getLayer().filters = e;
          }
        };
        f.prototype._readUpdateFrame = function() {
          var a = this.input, c = this.context, d = a.readInt(), e = 0, f = c._nodes[d];
          f || (f = c._nodes[d] = new b);
          d = a.readInt();
          d & 1 && f.getTransform().setMatrix(this._readMatrix());
          d & 8 && f.getTransform().setColorMatrix(this._readColorMatrix());
          if (d & 64) {
            var g = a.readInt();
            f.getLayer().mask = 0 <= g ? c._makeNode(g) : null;
          }
          d & 128 && (f.clip = a.readInt());
          d & 32 && (e = a.readInt() / 65535, g = a.readInt(), 1 !== g && (f.getLayer().blendMode = g), this._readFilters(f), f.toggleFlags(65536, a.readBoolean()), f.toggleFlags(131072, a.readBoolean()), f.toggleFlags(262144, !!a.readInt()), f.toggleFlags(524288, !!a.readInt()));
          if (d & 4) {
            d = a.readInt();
            g = f;
            g.clearChildren();
            for (var h = 0;h < d;h++) {
              var k = a.readInt(), k = c._makeNode(k);
              g.addChild(k);
            }
          }
          e && (k = f.getChildren()[0], k instanceof m && (k.ratio = e));
        };
        f.prototype._readDrawToBitmap = function() {
          var a = this.input, b = this.context, c = a.readInt(), e = a.readInt(), f = a.readInt(), g, k, l;
          g = f & 1 ? this._readMatrix().clone() : d.createIdentity();
          f & 8 && (k = this._readColorMatrix());
          f & 16 && (l = this._readRectangle());
          f = a.readInt();
          a.readBoolean();
          a = b._getBitmapAsset(c);
          e = b._makeNode(e);
          a ? a.drawNode(e, g, k, f, l) : b._registerAsset(c, -1, h.FromNode(e, g, k, f, l));
        };
        f.prototype._readRequestBitmapData = function() {
          var a = this.output, b = this.context, c = this.input.readInt();
          b._getBitmapAsset(c).readImageData(a);
        };
        f._temporaryReadMatrix = d.createIdentity();
        f._temporaryReadRectangle = c.createEmpty();
        f._temporaryReadColorMatrix = n.createIdentity();
        f._temporaryReadColorMatrixIdentity = n.createIdentity();
        return f;
      }();
      u.GFXChannelDeserializer = f;
    })(p.GFX || (p.GFX = {}));
  })(k.Remoting || (k.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var u = k.GFX.Geometry.Point, a = k.ArrayUtilities.DataBuffer;
    p.ContextMenuButton = 2;
    var w = function() {
      function m(a) {
        this._easel = a;
        var l = a.transparent;
        this._group = a.world;
        this._content = null;
        this._fullscreen = !1;
        this._context = new k.Remoting.GFX.GFXChannelDeserializerContext(this, this._group, l);
        this._addEventListeners();
        k.registerFallbackFont();
      }
      m.prototype.onSendUpdates = function(a, k) {
        throw Error("This method is abstract");
      };
      Object.defineProperty(m.prototype, "easel", {get:function() {
        return this._easel;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "stage", {get:function() {
        return this._easel.stage;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "content", {set:function(a) {
        this._content = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "cursor", {set:function(a) {
        this._easel.cursor = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "fullscreen", {set:function(a) {
        this._fullscreen !== a && (this._fullscreen = a, "undefined" !== typeof ShumwayCom && ShumwayCom.setFullscreen && ShumwayCom.setFullscreen(a));
      }, enumerable:!0, configurable:!0});
      m.prototype._mouseEventListener = function(b) {
        if (b.button !== p.ContextMenuButton) {
          var l = this._easel.getMousePosition(b, this._content), l = new u(l.x, l.y), m = new a, h = new k.Remoting.GFX.GFXChannelSerializer;
          h.output = m;
          h.writeMouseEvent(b, l);
          this.onSendUpdates(m, []);
        }
      };
      m.prototype._keyboardEventListener = function(b) {
        var l = new a, m = new k.Remoting.GFX.GFXChannelSerializer;
        m.output = l;
        m.writeKeyboardEvent(b);
        this.onSendUpdates(l, []);
      };
      m.prototype._addEventListeners = function() {
        for (var a = this._mouseEventListener.bind(this), k = this._keyboardEventListener.bind(this), p = m._mouseEvents, h = 0;h < p.length;h++) {
          window.addEventListener(p[h], a);
        }
        a = m._keyboardEvents;
        for (h = 0;h < a.length;h++) {
          window.addEventListener(a[h], k);
        }
        this._addFocusEventListeners();
        this._easel.addEventListener("resize", this._resizeEventListener.bind(this));
      };
      m.prototype._sendFocusEvent = function(b) {
        var l = new a, m = new k.Remoting.GFX.GFXChannelSerializer;
        m.output = l;
        m.writeFocusEvent(b);
        this.onSendUpdates(l, []);
      };
      m.prototype._addFocusEventListeners = function() {
        var a = this;
        document.addEventListener("visibilitychange", function(k) {
          a._sendFocusEvent(document.hidden ? 0 : 1);
        });
        window.addEventListener("focus", function(k) {
          a._sendFocusEvent(3);
        });
        window.addEventListener("blur", function(k) {
          a._sendFocusEvent(2);
        });
      };
      m.prototype._resizeEventListener = function() {
        this.onDisplayParameters(this._easel.getDisplayParameters());
      };
      m.prototype.onDisplayParameters = function(a) {
        throw Error("This method is abstract");
      };
      m.prototype.processUpdates = function(a, l, m) {
        void 0 === m && (m = null);
        var h = new k.Remoting.GFX.GFXChannelDeserializer;
        h.input = a;
        h.inputAssets = l;
        h.output = m;
        h.context = this._context;
        h.read();
      };
      m.prototype.processVideoControl = function(a, k, m) {
        var h = this._context, p = h._getVideoAsset(a);
        if (!p) {
          if (1 !== k) {
            return;
          }
          h.registerVideo(a);
          p = h._getVideoAsset(a);
        }
        return p.processControlRequest(k, m);
      };
      m.prototype.processRegisterFont = function(a, k, m) {
        this._context.registerFont(a, k, m);
      };
      m.prototype.processRegisterImage = function(a, k, m, h, p, q) {
        this._context.registerImage(a, k, m, h, p, q);
      };
      m.prototype.processFSCommand = function(a, k) {
        "undefined" !== typeof ShumwayCom && "test" === ShumwayCom.environment && ShumwayCom.processFSCommand(a, k);
      };
      m.prototype.processFrame = function() {
        "undefined" !== typeof ShumwayCom && "test" === ShumwayCom.environment && ShumwayCom.processFrame();
      };
      m.prototype.onVideoPlaybackEvent = function(a, k, m) {
        throw Error("This method is abstract");
      };
      m.prototype.sendVideoPlaybackEvent = function(a, k, m) {
        this.onVideoPlaybackEvent(a, k, m);
      };
      m._mouseEvents = k.Remoting.MouseEventNames;
      m._keyboardEvents = k.Remoting.KeyboardEventNames;
      return m;
    }();
    p.EaselHost = w;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.ArrayUtilities.DataBuffer, w = function(k) {
        function b(a, b) {
          k.call(this, a);
          this._peer = b;
          this._peer.onSyncMessage = function(a) {
            return this._onWindowMessage(a, !1);
          }.bind(this);
          this._peer.onAsyncMessage = function(a) {
            this._onWindowMessage(a, !0);
          }.bind(this);
        }
        __extends(b, k);
        b.prototype.onSendUpdates = function(a, b) {
          var h = a.getBytes();
          this._peer.postAsyncMessage({type:"gfx", updates:h, assets:b}, [h.buffer]);
        };
        b.prototype.onDisplayParameters = function(a) {
          this._peer.postAsyncMessage({type:"displayParameters", params:a});
        };
        b.prototype.onVideoPlaybackEvent = function(a, b, h) {
          this._peer.postAsyncMessage({type:"videoPlayback", id:a, eventType:b, data:h});
        };
        b.prototype._sendRegisterFontResponse = function(a, b) {
          this._peer.postAsyncMessage({type:"registerFontResponse", requestId:a, result:b});
        };
        b.prototype._sendRegisterImageResponse = function(a, b) {
          this._peer.postAsyncMessage({type:"registerImageResponse", requestId:a, result:b});
        };
        b.prototype._onWindowMessage = function(b, k) {
          var h;
          if ("object" === typeof b && null !== b) {
            if ("player" === b.type) {
              var m = a.FromArrayBuffer(b.updates.buffer);
              k ? this.processUpdates(m, b.assets) : (h = new a, this.processUpdates(m, b.assets, h), h = h.toPlainObject());
            } else {
              "frame" === b.type ? this.processFrame() : "videoControl" === b.type ? h = this.processVideoControl(b.id, b.eventType, b.data) : "registerFont" === b.type ? this.processRegisterFont(b.syncId, b.data, this._sendRegisterFontResponse.bind(this, b.requestId)) : "registerImage" === b.type ? this.processRegisterImage(b.syncId, b.symbolId, b.imageType, b.data, b.alphaData, this._sendRegisterImageResponse.bind(this, b.requestId)) : "fscommand" === b.type && this.processFSCommand(b.command, 
              b.args);
            }
          }
          return h;
        };
        return b;
      }(p.EaselHost);
      u.WindowEaselHost = w;
    })(p.Window || (p.Window = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(p) {
      function a(a, b) {
        a.writeInt(b.length);
        a.writeRawBytes(b);
      }
      function w(a, b) {
        return "byteLength" in a && "buffer" in a && (a.constructor && a.constructor.name) === b;
      }
      function m(a) {
        return "byteLength" in a && "ArrayBuffer" === (a.constructor && a.constructor.name);
      }
      function b(b) {
        function g(b) {
          switch(typeof b) {
            case "undefined":
              d.writeByte(0);
              break;
            case "boolean":
              d.writeByte(b ? 2 : 3);
              break;
            case "number":
              d.writeByte(4);
              d.writeDouble(b);
              break;
            case "string":
              d.writeByte(5);
              d.writeUTF(b);
              break;
            default:
              if (null === b) {
                d.writeByte(1);
                break;
              }
              if (Array.isArray(b)) {
                d.writeByte(6);
                d.writeInt(b.length);
                for (var e = 0;e < b.length;e++) {
                  g(b[e]);
                }
              } else {
                if (w(b, "Uint8Array")) {
                  d.writeByte(9), a(d, b);
                } else {
                  if ("length" in b && "buffer" in b && "littleEndian" in b) {
                    d.writeByte(b.littleEndian ? 10 : 11), a(d, new Uint8Array(b.buffer, 0, b.length));
                  } else {
                    if (m(b)) {
                      d.writeByte(8), a(d, new Uint8Array(b));
                    } else {
                      if (w(b, "Int32Array")) {
                        d.writeByte(12), a(d, new Uint8Array(b.buffer, b.byteOffset, b.byteLength));
                      } else {
                        if (!k.isNullOrUndefined(b.buffer) && m(b.buffer) && "number" === typeof b.byteOffset) {
                          throw Error("Some unsupported TypedArray is used");
                        }
                        d.writeByte(7);
                        for (e in b) {
                          d.writeUTF(e), g(b[e]);
                        }
                        d.writeUTF("");
                      }
                    }
                  }
                }
              }
            ;
          }
        }
        var d = new t;
        g(b);
        return d.getBytes();
      }
      function l(a) {
        var b = new t, d = a.readInt();
        a.readBytes(b, 0, d);
        return b.getBytes();
      }
      function r(a) {
        var b = new t, d = a.readInt();
        a.readBytes(b, 0, d);
        return h(b);
      }
      function h(a) {
        var b = a.readByte();
        switch(b) {
          case 1:
            return null;
          case 2:
            return !0;
          case 3:
            return !1;
          case 4:
            return a.readDouble();
          case 5:
            return a.readUTF();
          case 6:
            for (var b = [], d = a.readInt(), c = 0;c < d;c++) {
              b[c] = h(a);
            }
            return b;
          case 7:
            for (b = {};d = a.readUTF();) {
              b[d] = h(a);
            }
            return b;
          case 8:
            return l(a).buffer;
          case 9:
            return l(a);
          case 11:
          ;
          case 10:
            return a = l(a), new q(a.buffer, a.length, 10 === b);
          case 12:
            return new Int32Array(l(a).buffer);
        }
      }
      var t = k.ArrayUtilities.DataBuffer, q = k.ArrayUtilities.PlainObjectDataBuffer, n;
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
      })(n || (n = {}));
      (function(a) {
        a[a.None = 0] = "None";
        a[a.PlayerCommand = 1] = "PlayerCommand";
        a[a.PlayerCommandAsync = 2] = "PlayerCommandAsync";
        a[a.Frame = 3] = "Frame";
        a[a.Font = 4] = "Font";
        a[a.Image = 5] = "Image";
        a[a.FSCommand = 6] = "FSCommand";
      })(p.MovieRecordType || (p.MovieRecordType = {}));
      n = function() {
        function e(a) {
          this._maxRecordingSize = a;
          this._recording = new t;
          this._recordingStarted = Date.now();
          this._recording.writeRawBytes(new Uint8Array([77, 83, 87, 70]));
          this._stopped = !1;
        }
        e.prototype.stop = function() {
          this._stopped = !0;
        };
        e.prototype.getRecording = function() {
          return new Blob([this._recording.getBytes()], {type:"application/octet-stream"});
        };
        e.prototype.dump = function() {
          (new v(this._recording.getBytes())).dump();
        };
        e.prototype._createRecord = function(a, b) {
          this._stopped || (this._recording.length + 8 + (b ? b.length : 0) >= this._maxRecordingSize ? (console.error("Recording limit reached"), this._stopped = !0) : (this._recording.writeInt(Date.now() - this._recordingStarted), this._recording.writeInt(a), null !== b ? (this._recording.writeInt(b.length), this._recording.writeRawBytes(b.getBytes())) : this._recording.writeInt(0)));
        };
        e.prototype.recordPlayerCommand = function(e, d, c) {
          var f = new t;
          a(f, d);
          f.writeInt(c.length);
          c.forEach(function(c) {
            c = b(c);
            a(f, c);
          });
          this._createRecord(e ? 2 : 1, f);
        };
        e.prototype.recordFrame = function() {
          this._createRecord(3, null);
        };
        e.prototype.recordFont = function(e, d) {
          var c = new t;
          c.writeInt(e);
          a(c, b(d));
          this._createRecord(4, c);
        };
        e.prototype.recordImage = function(e, d, c, f, h) {
          var k = new t;
          k.writeInt(e);
          k.writeInt(d);
          k.writeInt(c);
          a(k, b(f));
          a(k, b(h));
          this._createRecord(5, k);
        };
        e.prototype.recordFSCommand = function(a, b) {
          var c = new t;
          c.writeUTF(a);
          c.writeUTF(b || "");
          this._createRecord(6, c);
        };
        return e;
      }();
      p.MovieRecorder = n;
      var v = function() {
        function a(b) {
          this._buffer = new t;
          this._buffer.writeRawBytes(b);
          this._buffer.position = 4;
        }
        a.prototype.readNextRecord = function() {
          if (this._buffer.position >= this._buffer.length) {
            return 0;
          }
          var a = this._buffer.readInt(), b = this._buffer.readInt(), c = this._buffer.readInt(), e = null;
          0 < c && (e = new t, this._buffer.readBytes(e, 0, c));
          this.currentTimestamp = a;
          this.currentType = b;
          this.currentData = e;
          return b;
        };
        a.prototype.parsePlayerCommand = function() {
          for (var a = l(this.currentData), b = this.currentData.readInt(), c = [], e = 0;e < b;e++) {
            c.push(r(this.currentData));
          }
          return {updates:a, assets:c};
        };
        a.prototype.parseFSCommand = function() {
          var a = this.currentData.readUTF(), b = this.currentData.readUTF();
          return {command:a, args:b};
        };
        a.prototype.parseFont = function() {
          var a = this.currentData.readInt(), b = r(this.currentData);
          return {syncId:a, data:b};
        };
        a.prototype.parseImage = function() {
          var a = this.currentData.readInt(), b = this.currentData.readInt(), c = this.currentData.readInt(), e = r(this.currentData);
          return {syncId:a, symbolId:b, imageType:c, data:e};
        };
        a.prototype.dump = function() {
          for (var a;a = this.readNextRecord();) {
            switch(console.log("record " + a + " @" + this.currentTimestamp), a) {
              case 1:
              ;
              case 2:
                console.log(this.parsePlayerCommand());
                break;
              case 6:
                console.log(this.parseFSCommand());
                break;
              case 4:
                console.log(this.parseFont());
                break;
              case 5:
                console.log(this.parseImage());
            }
          }
        };
        return a;
      }();
      p.MovieRecordParser = v;
    })(p.Test || (p.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(u) {
      var a = k.ArrayUtilities.DataBuffer, w = function(k) {
        function b(a) {
          k.call(this, a);
          this.alwaysRenderFrame = this.ignoreTimestamps = !1;
          this.cpuTimeRendering = this.cpuTimeUpdates = 0;
          this.onComplete = null;
        }
        __extends(b, k);
        Object.defineProperty(b.prototype, "cpuTime", {get:function() {
          return this.cpuTimeUpdates + this.cpuTimeRendering;
        }, enumerable:!0, configurable:!0});
        b.prototype.playUrl = function(a) {
          var b = new XMLHttpRequest;
          b.open("GET", a, !0);
          b.responseType = "arraybuffer";
          b.onload = function() {
            this.playBytes(new Uint8Array(b.response));
          }.bind(this);
          b.send();
        };
        b.prototype.playBytes = function(a) {
          this._parser = new u.MovieRecordParser(a);
          this._lastTimestamp = 0;
          this._parseNext();
        };
        b.prototype.onSendUpdates = function(a, b) {
        };
        b.prototype.onDisplayParameters = function(a) {
        };
        b.prototype.onVideoPlaybackEvent = function(a, b, h) {
        };
        b.prototype._parseNext = function() {
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
        b.prototype._runRecord = function() {
          var b, k = performance.now();
          switch(this._parser.currentType) {
            case 1:
            ;
            case 2:
              b = this._parser.parsePlayerCommand();
              var h = 2 === this._parser.currentType, m = a.FromArrayBuffer(b.updates.buffer);
              h ? this.processUpdates(m, b.assets) : (h = new a, this.processUpdates(m, b.assets, h));
              break;
            case 3:
              this.processFrame();
              break;
            case 4:
              b = this._parser.parseFont();
              this.processRegisterFont(b.syncId, b.data, function() {
              });
              break;
            case 5:
              b = this._parser.parseImage();
              this.processRegisterImage(b.syncId, b.symbolId, b.imageType, b.data, b.alphaData, function() {
              });
              break;
            case 6:
              b = this._parser.parseFSCommand();
              this.processFSCommand(b.command, b.args);
              break;
            default:
              throw Error("Invalid movie record type");;
          }
          this.cpuTimeUpdates += performance.now() - k;
          3 === this._parser.currentType && this.alwaysRenderFrame ? requestAnimationFrame(this._renderFrameJustAfterRAF.bind(this)) : this._parseNext();
        };
        b.prototype._renderFrameJustAfterRAF = function() {
          var a = performance.now();
          this.easel.render();
          this.cpuTimeRendering += performance.now() - a;
          this._parseNext();
        };
        return b;
      }(p.EaselHost);
      u.PlaybackEaselHost = w;
    })(p.Test || (p.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(p) {
      var a = function(a) {
        function k(b, l, m) {
          void 0 === m && (m = 0);
          a.call(this, b, l);
          this._recorder = null;
          this._recorder = new p.MovieRecorder(m);
        }
        __extends(k, a);
        Object.defineProperty(k.prototype, "recorder", {get:function() {
          return this._recorder;
        }, enumerable:!0, configurable:!0});
        k.prototype._onWindowMessage = function(b, k) {
          switch(b.type) {
            case "player":
              this._recorder.recordPlayerCommand(k, b.updates, b.assets);
              break;
            case "frame":
              this._recorder.recordFrame();
              break;
            case "registerFont":
              this._recorder.recordFont(b.syncId, b.data);
              break;
            case "registerImage":
              this._recorder.recordImage(b.syncId, b.symbolId, b.imageType, b.data, b.alphaData);
              break;
            case "fscommand":
              this._recorder.recordFSCommand(b.command, b.args);
          }
          return a.prototype._onWindowMessage.call(this, b, k);
        };
        return k;
      }(k.GFX.Window.WindowEaselHost);
      p.RecordingEaselHost = a;
    })(p.Test || (p.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load GFX Dependencies");

