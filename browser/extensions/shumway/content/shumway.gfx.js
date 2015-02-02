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
var jsGlobal = function() {
  return this || (0,eval)("this//# sourceURL=jsGlobal-getter");
}(), inBrowser = "undefined" !== typeof window && "document" in window && "plugins" in window.document, inFirefox = "undefined" !== typeof navigator && 0 <= navigator.userAgent.indexOf("Firefox");
function dumpLine(g) {
  "undefined" !== typeof dump && dump(g + "\n");
}
jsGlobal.performance || (jsGlobal.performance = {});
jsGlobal.performance.now || (jsGlobal.performance.now = "undefined" !== typeof dateNow ? dateNow : Date.now);
var START_TIME = performance.now(), Shumway;
(function(g) {
  function m(a) {
    return(a | 0) === a;
  }
  function e(a) {
    return "object" === typeof a || "function" === typeof a;
  }
  function c(a) {
    return String(Number(a)) === a;
  }
  function w(a) {
    var h = 0;
    if ("number" === typeof a) {
      return h = a | 0, a === h && 0 <= h ? !0 : a >>> 0 === a;
    }
    if ("string" !== typeof a) {
      return!1;
    }
    var f = a.length;
    if (0 === f) {
      return!1;
    }
    if ("0" === a) {
      return!0;
    }
    if (f > g.UINT32_CHAR_BUFFER_LENGTH) {
      return!1;
    }
    var d = 0, h = a.charCodeAt(d++) - 48;
    if (1 > h || 9 < h) {
      return!1;
    }
    for (var q = 0, x = 0;d < f;) {
      x = a.charCodeAt(d++) - 48;
      if (0 > x || 9 < x) {
        return!1;
      }
      q = h;
      h = 10 * h + x;
    }
    return q < g.UINT32_MAX_DIV_10 || q === g.UINT32_MAX_DIV_10 && x <= g.UINT32_MAX_MOD_10 ? !0 : !1;
  }
  (function(a) {
    a[a._0 = 48] = "_0";
    a[a._1 = 49] = "_1";
    a[a._2 = 50] = "_2";
    a[a._3 = 51] = "_3";
    a[a._4 = 52] = "_4";
    a[a._5 = 53] = "_5";
    a[a._6 = 54] = "_6";
    a[a._7 = 55] = "_7";
    a[a._8 = 56] = "_8";
    a[a._9 = 57] = "_9";
  })(g.CharacterCodes || (g.CharacterCodes = {}));
  g.UINT32_CHAR_BUFFER_LENGTH = 10;
  g.UINT32_MAX = 4294967295;
  g.UINT32_MAX_DIV_10 = 429496729;
  g.UINT32_MAX_MOD_10 = 5;
  g.isString = function(a) {
    return "string" === typeof a;
  };
  g.isFunction = function(a) {
    return "function" === typeof a;
  };
  g.isNumber = function(a) {
    return "number" === typeof a;
  };
  g.isInteger = m;
  g.isArray = function(a) {
    return a instanceof Array;
  };
  g.isNumberOrString = function(a) {
    return "number" === typeof a || "string" === typeof a;
  };
  g.isObject = e;
  g.toNumber = function(a) {
    return+a;
  };
  g.isNumericString = c;
  g.isNumeric = function(a) {
    if ("number" === typeof a) {
      return!0;
    }
    if ("string" === typeof a) {
      var h = a.charCodeAt(0);
      return 65 <= h && 90 >= h || 97 <= h && 122 >= h || 36 === h || 95 === h ? !1 : w(a) || c(a);
    }
    return!1;
  };
  g.isIndex = w;
  g.isNullOrUndefined = function(a) {
    return void 0 == a;
  };
  var k;
  (function(a) {
    a.error = function(f) {
      console.error(f);
      throw Error(f);
    };
    a.assert = function(f, d) {
      void 0 === d && (d = "assertion failed");
      "" === f && (f = !0);
      if (!f) {
        if ("undefined" !== typeof console && "assert" in console) {
          throw console.assert(!1, d), Error(d);
        }
        a.error(d.toString());
      }
    };
    a.assertUnreachable = function(f) {
      throw Error("Reached unreachable location " + Error().stack.split("\n")[1] + f);
    };
    a.assertNotImplemented = function(f, d) {
      f || a.error("notImplemented: " + d);
    };
    a.warning = function(f, d, q) {
      console.warn.apply(console, arguments);
    };
    a.notUsed = function(f) {
      a.assert(!1, "Not Used " + f);
    };
    a.notImplemented = function(f) {
      a.assert(!1, "Not Implemented " + f);
    };
    a.dummyConstructor = function(f) {
      a.assert(!1, "Dummy Constructor: " + f);
    };
    a.abstractMethod = function(f) {
      a.assert(!1, "Abstract Method " + f);
    };
    var h = {};
    a.somewhatImplemented = function(f) {
      h[f] || (h[f] = !0, a.warning("somewhatImplemented: " + f));
    };
    a.unexpected = function(f) {
      a.assert(!1, "Unexpected: " + f);
    };
    a.unexpectedCase = function(f) {
      a.assert(!1, "Unexpected Case: " + f);
    };
  })(k = g.Debug || (g.Debug = {}));
  g.getTicks = function() {
    return performance.now();
  };
  var b;
  (function(a) {
    function h(d, f) {
      for (var a = 0, h = d.length;a < h;a++) {
        if (d[a] === f) {
          return a;
        }
      }
      d.push(f);
      return d.length - 1;
    }
    var f = g.Debug.assert;
    a.popManyInto = function(d, a, h) {
      f(d.length >= a);
      for (var b = a - 1;0 <= b;b--) {
        h[b] = d.pop();
      }
      h.length = a;
    };
    a.popMany = function(d, a) {
      f(d.length >= a);
      var h = d.length - a, b = d.slice(h, this.length);
      d.length = h;
      return b;
    };
    a.popManyIntoVoid = function(d, a) {
      f(d.length >= a);
      d.length -= a;
    };
    a.pushMany = function(d, f) {
      for (var a = 0;a < f.length;a++) {
        d.push(f[a]);
      }
    };
    a.top = function(d) {
      return d.length && d[d.length - 1];
    };
    a.last = function(d) {
      return d.length && d[d.length - 1];
    };
    a.peek = function(d) {
      f(0 < d.length);
      return d[d.length - 1];
    };
    a.indexOf = function(d, f) {
      for (var a = 0, h = d.length;a < h;a++) {
        if (d[a] === f) {
          return a;
        }
      }
      return-1;
    };
    a.equals = function(d, f) {
      if (d.length !== f.length) {
        return!1;
      }
      for (var a = 0;a < d.length;a++) {
        if (d[a] !== f[a]) {
          return!1;
        }
      }
      return!0;
    };
    a.pushUnique = h;
    a.unique = function(d) {
      for (var f = [], a = 0;a < d.length;a++) {
        h(f, d[a]);
      }
      return f;
    };
    a.copyFrom = function(d, f) {
      d.length = 0;
      a.pushMany(d, f);
    };
    a.ensureTypedArrayCapacity = function(d, f) {
      if (d.length < f) {
        var a = d;
        d = new d.constructor(g.IntegerUtilities.nearestPowerOfTwo(f));
        d.set(a, 0);
      }
      return d;
    };
    var d = function() {
      function d(f) {
        void 0 === f && (f = 16);
        this._f32 = this._i32 = this._u16 = this._u8 = null;
        this._offset = 0;
        this.ensureCapacity(f);
      }
      d.prototype.reset = function() {
        this._offset = 0;
      };
      Object.defineProperty(d.prototype, "offset", {get:function() {
        return this._offset;
      }, enumerable:!0, configurable:!0});
      d.prototype.getIndex = function(d) {
        f(1 === d || 2 === d || 4 === d || 8 === d || 16 === d);
        d = this._offset / d;
        f((d | 0) === d);
        return d;
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
        var f = 2 * this._u8.length;
        f < d && (f = d);
        d = new Uint8Array(f);
        d.set(this._u8, 0);
        this._u8 = d;
        this._u16 = new Uint16Array(d.buffer);
        this._i32 = new Int32Array(d.buffer);
        this._f32 = new Float32Array(d.buffer);
      };
      d.prototype.writeInt = function(d) {
        f(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(d);
      };
      d.prototype.writeIntAt = function(d, q) {
        f(0 <= q && q <= this._offset);
        f(0 === (q & 3));
        this.ensureCapacity(q + 4);
        this._i32[q >> 2] = d;
      };
      d.prototype.writeIntUnsafe = function(d) {
        this._i32[this._offset >> 2] = d;
        this._offset += 4;
      };
      d.prototype.writeFloat = function(d) {
        f(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(d);
      };
      d.prototype.writeFloatUnsafe = function(d) {
        this._f32[this._offset >> 2] = d;
        this._offset += 4;
      };
      d.prototype.write4Floats = function(d, q, a, h) {
        f(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(d, q, a, h);
      };
      d.prototype.write4FloatsUnsafe = function(d, f, q, a) {
        var h = this._offset >> 2;
        this._f32[h + 0] = d;
        this._f32[h + 1] = f;
        this._f32[h + 2] = q;
        this._f32[h + 3] = a;
        this._offset += 16;
      };
      d.prototype.write6Floats = function(d, q, a, h, b, u) {
        f(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(d, q, a, h, b, u);
      };
      d.prototype.write6FloatsUnsafe = function(d, f, q, a, h, b) {
        var u = this._offset >> 2;
        this._f32[u + 0] = d;
        this._f32[u + 1] = f;
        this._f32[u + 2] = q;
        this._f32[u + 3] = a;
        this._f32[u + 4] = h;
        this._f32[u + 5] = b;
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
      d.prototype.hashWords = function(d, f, q) {
        f = this._i32;
        for (var a = 0;a < q;a++) {
          d = (31 * d | 0) + f[a] | 0;
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
    a.ArrayWriter = d;
  })(b = g.ArrayUtilities || (g.ArrayUtilities = {}));
  var a = function() {
    function a(h) {
      this._u8 = new Uint8Array(h);
      this._u16 = new Uint16Array(h);
      this._i32 = new Int32Array(h);
      this._f32 = new Float32Array(h);
      this._offset = 0;
    }
    Object.defineProperty(a.prototype, "offset", {get:function() {
      return this._offset;
    }, enumerable:!0, configurable:!0});
    a.prototype.isEmpty = function() {
      return this._offset === this._u8.length;
    };
    a.prototype.readInt = function() {
      k.assert(0 === (this._offset & 3));
      k.assert(this._offset <= this._u8.length - 4);
      var a = this._i32[this._offset >> 2];
      this._offset += 4;
      return a;
    };
    a.prototype.readFloat = function() {
      k.assert(0 === (this._offset & 3));
      k.assert(this._offset <= this._u8.length - 4);
      var a = this._f32[this._offset >> 2];
      this._offset += 4;
      return a;
    };
    return a;
  }();
  g.ArrayReader = a;
  (function(a) {
    function h(d, f) {
      return Object.prototype.hasOwnProperty.call(d, f);
    }
    function f(d, f) {
      for (var a in f) {
        h(f, a) && (d[a] = f[a]);
      }
    }
    a.boxValue = function(d) {
      return void 0 == d || e(d) ? d : Object(d);
    };
    a.toKeyValueArray = function(d) {
      var f = Object.prototype.hasOwnProperty, a = [], h;
      for (h in d) {
        f.call(d, h) && a.push([h, d[h]]);
      }
      return a;
    };
    a.isPrototypeWriteable = function(d) {
      return Object.getOwnPropertyDescriptor(d, "prototype").writable;
    };
    a.hasOwnProperty = h;
    a.propertyIsEnumerable = function(d, f) {
      return Object.prototype.propertyIsEnumerable.call(d, f);
    };
    a.getOwnPropertyDescriptor = function(d, f) {
      return Object.getOwnPropertyDescriptor(d, f);
    };
    a.hasOwnGetter = function(d, f) {
      var a = Object.getOwnPropertyDescriptor(d, f);
      return!(!a || !a.get);
    };
    a.getOwnGetter = function(d, f) {
      var a = Object.getOwnPropertyDescriptor(d, f);
      return a ? a.get : null;
    };
    a.hasOwnSetter = function(d, f) {
      var a = Object.getOwnPropertyDescriptor(d, f);
      return!(!a || !a.set);
    };
    a.createMap = function() {
      return Object.create(null);
    };
    a.createArrayMap = function() {
      return[];
    };
    a.defineReadOnlyProperty = function(d, f, a) {
      Object.defineProperty(d, f, {value:a, writable:!1, configurable:!0, enumerable:!1});
    };
    a.getOwnPropertyDescriptors = function(d) {
      for (var f = a.createMap(), h = Object.getOwnPropertyNames(d), s = 0;s < h.length;s++) {
        f[h[s]] = Object.getOwnPropertyDescriptor(d, h[s]);
      }
      return f;
    };
    a.cloneObject = function(d) {
      var q = Object.create(Object.getPrototypeOf(d));
      f(q, d);
      return q;
    };
    a.copyProperties = function(d, f) {
      for (var a in f) {
        d[a] = f[a];
      }
    };
    a.copyOwnProperties = f;
    a.copyOwnPropertyDescriptors = function(d, f, a) {
      void 0 === a && (a = !0);
      for (var s in f) {
        if (h(f, s)) {
          var b = Object.getOwnPropertyDescriptor(f, s);
          if (a || !h(d, s)) {
            k.assert(b);
            try {
              Object.defineProperty(d, s, b);
            } catch (u) {
            }
          }
        }
      }
    };
    a.getLatestGetterOrSetterPropertyDescriptor = function(d, f) {
      for (var a = {};d;) {
        var h = Object.getOwnPropertyDescriptor(d, f);
        h && (a.get = a.get || h.get, a.set = a.set || h.set);
        if (a.get && a.set) {
          break;
        }
        d = Object.getPrototypeOf(d);
      }
      return a;
    };
    a.defineNonEnumerableGetterOrSetter = function(d, f, h, s) {
      var b = a.getLatestGetterOrSetterPropertyDescriptor(d, f);
      b.configurable = !0;
      b.enumerable = !1;
      s ? b.get = h : b.set = h;
      Object.defineProperty(d, f, b);
    };
    a.defineNonEnumerableGetter = function(d, f, a) {
      Object.defineProperty(d, f, {get:a, configurable:!0, enumerable:!1});
    };
    a.defineNonEnumerableSetter = function(d, f, a) {
      Object.defineProperty(d, f, {set:a, configurable:!0, enumerable:!1});
    };
    a.defineNonEnumerableProperty = function(d, f, a) {
      Object.defineProperty(d, f, {value:a, writable:!0, configurable:!0, enumerable:!1});
    };
    a.defineNonEnumerableForwardingProperty = function(d, f, a) {
      Object.defineProperty(d, f, {get:n.makeForwardingGetter(a), set:n.makeForwardingSetter(a), writable:!0, configurable:!0, enumerable:!1});
    };
    a.defineNewNonEnumerableProperty = function(d, f, h) {
      k.assert(!Object.prototype.hasOwnProperty.call(d, f), "Property: " + f + " already exits.");
      a.defineNonEnumerableProperty(d, f, h);
    };
    a.createPublicAliases = function(d, f) {
      for (var a = {value:null, writable:!0, configurable:!0, enumerable:!1}, h = 0;h < f.length;h++) {
        var b = f[h];
        a.value = d[b];
        Object.defineProperty(d, "$Bg" + b, a);
      }
    };
  })(g.ObjectUtilities || (g.ObjectUtilities = {}));
  var n;
  (function(a) {
    a.makeForwardingGetter = function(a) {
      return new Function('return this["' + a + '"]//# sourceURL=fwd-get-' + a + ".as");
    };
    a.makeForwardingSetter = function(a) {
      return new Function("value", 'this["' + a + '"] = value;//# sourceURL=fwd-set-' + a + ".as");
    };
    a.bindSafely = function(a, f) {
      function d() {
        return a.apply(f, arguments);
      }
      k.assert(!a.boundTo);
      k.assert(f);
      d.boundTo = f;
      return d;
    };
  })(n = g.FunctionUtilities || (g.FunctionUtilities = {}));
  (function(a) {
    function h(d) {
      return "string" === typeof d ? '"' + d + '"' : "number" === typeof d || "boolean" === typeof d ? String(d) : d instanceof Array ? "[] " + d.length : typeof d;
    }
    var f = g.Debug.assert;
    a.repeatString = function(d, f) {
      for (var q = "", a = 0;a < f;a++) {
        q += d;
      }
      return q;
    };
    a.memorySizeToString = function(d) {
      d |= 0;
      return 1024 > d ? d + " B" : 1048576 > d ? (d / 1024).toFixed(2) + "KB" : (d / 1048576).toFixed(2) + "MB";
    };
    a.toSafeString = h;
    a.toSafeArrayString = function(d) {
      for (var f = [], q = 0;q < d.length;q++) {
        f.push(h(d[q]));
      }
      return f.join(", ");
    };
    a.utf8decode = function(d) {
      for (var f = new Uint8Array(4 * d.length), q = 0, a = 0, h = d.length;a < h;a++) {
        var x = d.charCodeAt(a);
        if (127 >= x) {
          f[q++] = x;
        } else {
          if (55296 <= x && 56319 >= x) {
            var b = d.charCodeAt(a + 1);
            56320 <= b && 57343 >= b && (x = ((x & 1023) << 10) + (b & 1023) + 65536, ++a);
          }
          0 !== (x & 4292870144) ? (f[q++] = 248 | x >>> 24 & 3, f[q++] = 128 | x >>> 18 & 63, f[q++] = 128 | x >>> 12 & 63, f[q++] = 128 | x >>> 6 & 63) : 0 !== (x & 4294901760) ? (f[q++] = 240 | x >>> 18 & 7, f[q++] = 128 | x >>> 12 & 63, f[q++] = 128 | x >>> 6 & 63) : 0 !== (x & 4294965248) ? (f[q++] = 224 | x >>> 12 & 15, f[q++] = 128 | x >>> 6 & 63) : f[q++] = 192 | x >>> 6 & 31;
          f[q++] = 128 | x & 63;
        }
      }
      return f.subarray(0, q);
    };
    a.utf8encode = function(d) {
      for (var f = 0, q = "";f < d.length;) {
        var a = d[f++] & 255;
        if (127 >= a) {
          q += String.fromCharCode(a);
        } else {
          var h = 192, x = 5;
          do {
            if ((a & (h >> 1 | 128)) === h) {
              break;
            }
            h = h >> 1 | 128;
            --x;
          } while (0 <= x);
          if (0 >= x) {
            q += String.fromCharCode(a);
          } else {
            for (var a = a & (1 << x) - 1, h = !1, b = 5;b >= x;--b) {
              var s = d[f++];
              if (128 != (s & 192)) {
                h = !0;
                break;
              }
              a = a << 6 | s & 63;
            }
            if (h) {
              for (x = f - (7 - b);x < f;++x) {
                q += String.fromCharCode(d[x] & 255);
              }
            } else {
              q = 65536 <= a ? q + String.fromCharCode(a - 65536 >> 10 & 1023 | 55296, a & 1023 | 56320) : q + String.fromCharCode(a);
            }
          }
        }
      }
      return q;
    };
    a.base64ArrayBuffer = function(d) {
      var f = "";
      d = new Uint8Array(d);
      for (var q = d.byteLength, a = q % 3, q = q - a, h, x, b, s, u = 0;u < q;u += 3) {
        s = d[u] << 16 | d[u + 1] << 8 | d[u + 2], h = (s & 16515072) >> 18, x = (s & 258048) >> 12, b = (s & 4032) >> 6, s &= 63, f += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[h] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[x] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[b] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[s];
      }
      1 == a ? (s = d[q], f += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(s & 252) >> 2] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(s & 3) << 4] + "==") : 2 == a && (s = d[q] << 8 | d[q + 1], f += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(s & 64512) >> 10] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(s & 1008) >> 4] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(s & 15) << 
      2] + "=");
      return f;
    };
    a.escapeString = function(d) {
      void 0 !== d && (d = d.replace(/[^\w$]/gi, "$"), /^\d/.test(d) && (d = "$" + d));
      return d;
    };
    a.fromCharCodeArray = function(d) {
      for (var f = "", q = 0;q < d.length;q += 16384) {
        var a = Math.min(d.length - q, 16384), f = f + String.fromCharCode.apply(null, d.subarray(q, q + a))
      }
      return f;
    };
    a.variableLengthEncodeInt32 = function(d) {
      var q = 32 - Math.clz32(d);
      f(32 >= q, q);
      for (var h = Math.ceil(q / 6), x = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[h], b = h - 1;0 <= b;b--) {
        x += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[d >> 6 * b & 63];
      }
      f(a.variableLengthDecodeInt32(x) === d, d + " : " + x + " - " + h + " bits: " + q);
      return x;
    };
    a.toEncoding = function(d) {
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[d];
    };
    a.fromEncoding = function(d) {
      d = d.charCodeAt(0);
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
      f(!1, "Invalid Encoding");
    };
    a.variableLengthDecodeInt32 = function(d) {
      for (var f = a.fromEncoding(d[0]), q = 0, h = 0;h < f;h++) {
        var x = 6 * (f - h - 1), q = q | a.fromEncoding(d[1 + h]) << x
      }
      return q;
    };
    a.trimMiddle = function(d, f) {
      if (d.length <= f) {
        return d;
      }
      var q = f >> 1, a = f - q - 1;
      return d.substr(0, q) + "\u2026" + d.substr(d.length - a, a);
    };
    a.multiple = function(d, f) {
      for (var q = "", a = 0;a < f;a++) {
        q += d;
      }
      return q;
    };
    a.indexOfAny = function(d, f, q) {
      for (var a = d.length, h = 0;h < f.length;h++) {
        var x = d.indexOf(f[h], q);
        0 <= x && (a = Math.min(a, x));
      }
      return a === d.length ? -1 : a;
    };
    var d = Array(3), q = Array(4), x = Array(5), b = Array(6), n = Array(7), p = Array(8), r = Array(9);
    a.concat3 = function(f, q, a) {
      d[0] = f;
      d[1] = q;
      d[2] = a;
      return d.join("");
    };
    a.concat4 = function(d, f, a, h) {
      q[0] = d;
      q[1] = f;
      q[2] = a;
      q[3] = h;
      return q.join("");
    };
    a.concat5 = function(d, f, q, a, h) {
      x[0] = d;
      x[1] = f;
      x[2] = q;
      x[3] = a;
      x[4] = h;
      return x.join("");
    };
    a.concat6 = function(d, f, q, a, h, x) {
      b[0] = d;
      b[1] = f;
      b[2] = q;
      b[3] = a;
      b[4] = h;
      b[5] = x;
      return b.join("");
    };
    a.concat7 = function(d, f, q, a, h, x, b) {
      n[0] = d;
      n[1] = f;
      n[2] = q;
      n[3] = a;
      n[4] = h;
      n[5] = x;
      n[6] = b;
      return n.join("");
    };
    a.concat8 = function(d, f, q, a, h, x, b, s) {
      p[0] = d;
      p[1] = f;
      p[2] = q;
      p[3] = a;
      p[4] = h;
      p[5] = x;
      p[6] = b;
      p[7] = s;
      return p.join("");
    };
    a.concat9 = function(d, f, q, a, h, x, b, s, u) {
      r[0] = d;
      r[1] = f;
      r[2] = q;
      r[3] = a;
      r[4] = h;
      r[5] = x;
      r[6] = b;
      r[7] = s;
      r[8] = u;
      return r.join("");
    };
  })(g.StringUtilities || (g.StringUtilities = {}));
  (function(a) {
    var h = new Uint8Array([7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]), f = new Int32Array([-680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426, -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162, 1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632, 
    643717713, -373897302, -701558691, 38016083, -660478335, -405537848, 568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784, 1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556, -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222, -722521979, 76029189, -640364487, -421815835, 530742520, -995338651, -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606, -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649, 
    -145523070, -1120210379, 718787259, -343485551]);
    a.hashBytesTo32BitsMD5 = function(d, q, a) {
      var b = 1732584193, u = -271733879, n = -1732584194, p = 271733878, r = a + 72 & -64, c = new Uint8Array(r), k;
      for (k = 0;k < a;++k) {
        c[k] = d[q++];
      }
      c[k++] = 128;
      for (d = r - 8;k < d;) {
        c[k++] = 0;
      }
      c[k++] = a << 3 & 255;
      c[k++] = a >> 5 & 255;
      c[k++] = a >> 13 & 255;
      c[k++] = a >> 21 & 255;
      c[k++] = a >>> 29 & 255;
      c[k++] = 0;
      c[k++] = 0;
      c[k++] = 0;
      d = new Int32Array(16);
      for (k = 0;k < r;) {
        for (a = 0;16 > a;++a, k += 4) {
          d[a] = c[k] | c[k + 1] << 8 | c[k + 2] << 16 | c[k + 3] << 24;
        }
        var y = b;
        q = u;
        var l = n, t = p, v, e;
        for (a = 0;64 > a;++a) {
          16 > a ? (v = q & l | ~q & t, e = a) : 32 > a ? (v = t & q | ~t & l, e = 5 * a + 1 & 15) : 48 > a ? (v = q ^ l ^ t, e = 3 * a + 5 & 15) : (v = l ^ (q | ~t), e = 7 * a & 15);
          var g = t, y = y + v + f[a] + d[e] | 0;
          v = h[a];
          t = l;
          l = q;
          q = q + (y << v | y >>> 32 - v) | 0;
          y = g;
        }
        b = b + y | 0;
        u = u + q | 0;
        n = n + l | 0;
        p = p + t | 0;
      }
      return b;
    };
    a.hashBytesTo32BitsAdler = function(d, f, a) {
      var h = 1, b = 0;
      for (a = f + a;f < a;++f) {
        h = (h + (d[f] & 255)) % 65521, b = (b + h) % 65521;
      }
      return b << 16 | h;
    };
  })(g.HashUtilities || (g.HashUtilities = {}));
  var p = function() {
    function a() {
    }
    a.seed = function(h) {
      a._state[0] = h;
      a._state[1] = h;
    };
    a.next = function() {
      var a = this._state, f = Math.imul(18273, a[0] & 65535) + (a[0] >>> 16) | 0;
      a[0] = f;
      var d = Math.imul(36969, a[1] & 65535) + (a[1] >>> 16) | 0;
      a[1] = d;
      a = (f << 16) + (d & 65535) | 0;
      return 2.3283064365386963E-10 * (0 > a ? a + 4294967296 : a);
    };
    a._state = new Uint32Array([57005, 48879]);
    return a;
  }();
  g.Random = p;
  Math.random = function() {
    return p.next();
  };
  (function() {
    function a() {
      this.id = "$weakmap" + h++;
    }
    if ("function" !== typeof jsGlobal.WeakMap) {
      var h = 0;
      a.prototype = {has:function(f) {
        return f.hasOwnProperty(this.id);
      }, get:function(f, d) {
        return f.hasOwnProperty(this.id) ? f[this.id] : d;
      }, set:function(f, d) {
        Object.defineProperty(f, this.id, {value:d, enumerable:!1, configurable:!0});
      }};
      jsGlobal.WeakMap = a;
    }
  })();
  a = function() {
    function a() {
      "undefined" !== typeof netscape && netscape.security.PrivilegeManager ? this._map = new WeakMap : this._list = [];
    }
    a.prototype.clear = function() {
      this._map ? this._map.clear() : this._list.length = 0;
    };
    a.prototype.push = function(a) {
      this._map ? (k.assert(!this._map.has(a)), this._map.set(a, null)) : (k.assert(-1 === this._list.indexOf(a)), this._list.push(a));
    };
    a.prototype.remove = function(a) {
      this._map ? (k.assert(this._map.has(a)), this._map.delete(a)) : (k.assert(-1 < this._list.indexOf(a)), this._list[this._list.indexOf(a)] = null, k.assert(-1 === this._list.indexOf(a)));
    };
    a.prototype.forEach = function(a) {
      if (this._map) {
        "undefined" !== typeof netscape && netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect"), Components.utils.nondeterministicGetWeakMapKeys(this._map).forEach(function(d) {
          0 !== d._referenceCount && a(d);
        });
      } else {
        for (var f = this._list, d = 0, q = 0;q < f.length;q++) {
          var x = f[q];
          x && (0 === x._referenceCount ? (f[q] = null, d++) : a(x));
        }
        if (16 < d && d > f.length >> 2) {
          d = [];
          for (q = 0;q < f.length;q++) {
            (x = f[q]) && 0 < x._referenceCount && d.push(x);
          }
          this._list = d;
        }
      }
    };
    Object.defineProperty(a.prototype, "length", {get:function() {
      return this._map ? -1 : this._list.length;
    }, enumerable:!0, configurable:!0});
    return a;
  }();
  g.WeakList = a;
  var y;
  (function(a) {
    a.pow2 = function(a) {
      return a === (a | 0) ? 0 > a ? 1 / (1 << -a) : 1 << a : Math.pow(2, a);
    };
    a.clamp = function(a, f, d) {
      return Math.max(f, Math.min(d, a));
    };
    a.roundHalfEven = function(a) {
      if (.5 === Math.abs(a % 1)) {
        var f = Math.floor(a);
        return 0 === f % 2 ? f : Math.ceil(a);
      }
      return Math.round(a);
    };
    a.epsilonEquals = function(a, f) {
      return 1E-7 > Math.abs(a - f);
    };
  })(y = g.NumberUtilities || (g.NumberUtilities = {}));
  (function(a) {
    a[a.MaxU16 = 65535] = "MaxU16";
    a[a.MaxI16 = 32767] = "MaxI16";
    a[a.MinI16 = -32768] = "MinI16";
  })(g.Numbers || (g.Numbers = {}));
  var v;
  (function(a) {
    function h(d) {
      return 256 * d << 16 >> 16;
    }
    var f = new ArrayBuffer(8);
    a.i8 = new Int8Array(f);
    a.u8 = new Uint8Array(f);
    a.i32 = new Int32Array(f);
    a.f32 = new Float32Array(f);
    a.f64 = new Float64Array(f);
    a.nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
    a.floatToInt32 = function(d) {
      a.f32[0] = d;
      return a.i32[0];
    };
    a.int32ToFloat = function(d) {
      a.i32[0] = d;
      return a.f32[0];
    };
    a.swap16 = function(d) {
      return(d & 255) << 8 | d >> 8 & 255;
    };
    a.swap32 = function(d) {
      return(d & 255) << 24 | (d & 65280) << 8 | d >> 8 & 65280 | d >> 24 & 255;
    };
    a.toS8U8 = h;
    a.fromS8U8 = function(d) {
      return d / 256;
    };
    a.clampS8U8 = function(d) {
      return h(d) / 256;
    };
    a.toS16 = function(d) {
      return d << 16 >> 16;
    };
    a.bitCount = function(d) {
      d -= d >> 1 & 1431655765;
      d = (d & 858993459) + (d >> 2 & 858993459);
      return 16843009 * (d + (d >> 4) & 252645135) >> 24;
    };
    a.ones = function(d) {
      d -= d >> 1 & 1431655765;
      d = (d & 858993459) + (d >> 2 & 858993459);
      return 16843009 * (d + (d >> 4) & 252645135) >> 24;
    };
    a.trailingZeros = function(d) {
      return a.ones((d & -d) - 1);
    };
    a.getFlags = function(d, f) {
      var a = "";
      for (d = 0;d < f.length;d++) {
        d & 1 << d && (a += f[d] + " ");
      }
      return 0 === a.length ? "" : a.trim();
    };
    a.isPowerOfTwo = function(d) {
      return d && 0 === (d & d - 1);
    };
    a.roundToMultipleOfFour = function(d) {
      return d + 3 & -4;
    };
    a.nearestPowerOfTwo = function(d) {
      d--;
      d |= d >> 1;
      d |= d >> 2;
      d |= d >> 4;
      d |= d >> 8;
      d |= d >> 16;
      d++;
      return d;
    };
    a.roundToMultipleOfPowerOfTwo = function(d, f) {
      var a = (1 << f) - 1;
      return d + a & ~a;
    };
    Math.imul || (Math.imul = function(d, f) {
      var a = d & 65535, h = f & 65535;
      return a * h + ((d >>> 16 & 65535) * h + a * (f >>> 16 & 65535) << 16 >>> 0) | 0;
    });
    Math.clz32 || (Math.clz32 = function(d) {
      d |= d >> 1;
      d |= d >> 2;
      d |= d >> 4;
      d |= d >> 8;
      return 32 - a.ones(d | d >> 16);
    });
  })(v = g.IntegerUtilities || (g.IntegerUtilities = {}));
  (function(a) {
    function h(f, d, a, h, b, u) {
      return(a - f) * (u - d) - (h - d) * (b - f);
    }
    a.pointInPolygon = function(f, d, a) {
      for (var h = 0, b = a.length - 2, u = 0;u < b;u += 2) {
        var n = a[u + 0], p = a[u + 1], r = a[u + 2], c = a[u + 3];
        (p <= d && c > d || p > d && c <= d) && f < n + (d - p) / (c - p) * (r - n) && h++;
      }
      return 1 === (h & 1);
    };
    a.signedArea = h;
    a.counterClockwise = function(f, d, a, b, s, u) {
      return 0 < h(f, d, a, b, s, u);
    };
    a.clockwise = function(f, d, a, b, s, u) {
      return 0 > h(f, d, a, b, s, u);
    };
    a.pointInPolygonInt32 = function(f, d, a) {
      f |= 0;
      d |= 0;
      for (var h = 0, b = a.length - 2, u = 0;u < b;u += 2) {
        var n = a[u + 0], p = a[u + 1], r = a[u + 2], c = a[u + 3];
        (p <= d && c > d || p > d && c <= d) && f < n + (d - p) / (c - p) * (r - n) && h++;
      }
      return 1 === (h & 1);
    };
  })(g.GeometricUtilities || (g.GeometricUtilities = {}));
  (function(a) {
    a[a.Error = 1] = "Error";
    a[a.Warn = 2] = "Warn";
    a[a.Debug = 4] = "Debug";
    a[a.Log = 8] = "Log";
    a[a.Info = 16] = "Info";
    a[a.All = 31] = "All";
  })(g.LogLevel || (g.LogLevel = {}));
  a = function() {
    function a(h, f) {
      void 0 === h && (h = !1);
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = h;
      this._out = f || a._consoleOut;
      this._outNoNewline = f || a._consoleOutNoNewline;
    }
    a.prototype.write = function(a, f) {
      void 0 === a && (a = "");
      void 0 === f && (f = !1);
      this._suppressOutput || this._outNoNewline((f ? this._padding : "") + a);
    };
    a.prototype.writeLn = function(a) {
      void 0 === a && (a = "");
      this._suppressOutput || this._out(this._padding + a);
    };
    a.prototype.writeObject = function(a, f) {
      void 0 === a && (a = "");
      this._suppressOutput || this._out(this._padding + a, f);
    };
    a.prototype.writeTimeLn = function(a) {
      void 0 === a && (a = "");
      this._suppressOutput || this._out(this._padding + performance.now().toFixed(2) + " " + a);
    };
    a.prototype.writeComment = function(a) {
      a = a.split("\n");
      if (1 === a.length) {
        this.writeLn("// " + a[0]);
      } else {
        this.writeLn("/**");
        for (var f = 0;f < a.length;f++) {
          this.writeLn(" * " + a[f]);
        }
        this.writeLn(" */");
      }
    };
    a.prototype.writeLns = function(a) {
      a = a.split("\n");
      for (var f = 0;f < a.length;f++) {
        this.writeLn(a[f]);
      }
    };
    a.prototype.errorLn = function(h) {
      a.logLevel & 1 && this.boldRedLn(h);
    };
    a.prototype.warnLn = function(h) {
      a.logLevel & 2 && this.yellowLn(h);
    };
    a.prototype.debugLn = function(h) {
      a.logLevel & 4 && this.purpleLn(h);
    };
    a.prototype.logLn = function(h) {
      a.logLevel & 8 && this.writeLn(h);
    };
    a.prototype.infoLn = function(h) {
      a.logLevel & 16 && this.writeLn(h);
    };
    a.prototype.yellowLn = function(h) {
      this.colorLn(a.YELLOW, h);
    };
    a.prototype.greenLn = function(h) {
      this.colorLn(a.GREEN, h);
    };
    a.prototype.boldRedLn = function(h) {
      this.colorLn(a.BOLD_RED, h);
    };
    a.prototype.redLn = function(h) {
      this.colorLn(a.RED, h);
    };
    a.prototype.purpleLn = function(h) {
      this.colorLn(a.PURPLE, h);
    };
    a.prototype.colorLn = function(h, f) {
      this._suppressOutput || (inBrowser ? this._out(this._padding + f) : this._out(this._padding + h + f + a.ENDC));
    };
    a.prototype.redLns = function(h) {
      this.colorLns(a.RED, h);
    };
    a.prototype.colorLns = function(a, f) {
      for (var d = f.split("\n"), q = 0;q < d.length;q++) {
        this.colorLn(a, d[q]);
      }
    };
    a.prototype.enter = function(a) {
      this._suppressOutput || this._out(this._padding + a);
      this.indent();
    };
    a.prototype.leaveAndEnter = function(a) {
      this.leave(a);
      this.indent();
    };
    a.prototype.leave = function(a) {
      this.outdent();
      !this._suppressOutput && a && this._out(this._padding + a);
    };
    a.prototype.indent = function() {
      this._padding += this._tab;
    };
    a.prototype.outdent = function() {
      0 < this._padding.length && (this._padding = this._padding.substring(0, this._padding.length - this._tab.length));
    };
    a.prototype.writeArray = function(a, f, d) {
      void 0 === f && (f = !1);
      void 0 === d && (d = !1);
      f = f || !1;
      for (var q = 0, b = a.length;q < b;q++) {
        var s = "";
        f && (s = null === a[q] ? "null" : void 0 === a[q] ? "undefined" : a[q].constructor.name, s += " ");
        var n = d ? "" : ("" + q).padRight(" ", 4);
        this.writeLn(n + s + a[q]);
      }
    };
    a.PURPLE = "\u001b[94m";
    a.YELLOW = "\u001b[93m";
    a.GREEN = "\u001b[92m";
    a.RED = "\u001b[91m";
    a.BOLD_RED = "\u001b[1;91m";
    a.ENDC = "\u001b[0m";
    a.logLevel = 31;
    a._consoleOut = console.info.bind(console);
    a._consoleOutNoNewline = console.info.bind(console);
    return a;
  }();
  g.IndentingWriter = a;
  var l = function() {
    return function(a, h) {
      this.value = a;
      this.next = h;
    };
  }(), a = function() {
    function a(h) {
      k.assert(h);
      this._compare = h;
      this._head = null;
      this._length = 0;
    }
    a.prototype.push = function(a) {
      k.assert(void 0 !== a);
      this._length++;
      if (this._head) {
        var f = this._head, d = null;
        a = new l(a, null);
        for (var q = this._compare;f;) {
          if (0 < q(f.value, a.value)) {
            d ? (a.next = f, d.next = a) : (a.next = this._head, this._head = a);
            return;
          }
          d = f;
          f = f.next;
        }
        d.next = a;
      } else {
        this._head = new l(a, null);
      }
    };
    a.prototype.forEach = function(h) {
      for (var f = this._head, d = null;f;) {
        var q = h(f.value);
        if (q === a.RETURN) {
          break;
        } else {
          q === a.DELETE ? f = d ? d.next = f.next : this._head = this._head.next : (d = f, f = f.next);
        }
      }
    };
    a.prototype.isEmpty = function() {
      return!this._head;
    };
    a.prototype.pop = function() {
      if (this._head) {
        this._length--;
        var a = this._head;
        this._head = this._head.next;
        return a.value;
      }
    };
    a.prototype.contains = function(a) {
      for (var f = this._head;f;) {
        if (f.value === a) {
          return!0;
        }
        f = f.next;
      }
      return!1;
    };
    a.prototype.toString = function() {
      for (var a = "[", f = this._head;f;) {
        a += f.value.toString(), (f = f.next) && (a += ",");
      }
      return a + "]";
    };
    a.RETURN = 1;
    a.DELETE = 2;
    return a;
  }();
  g.SortedList = a;
  a = function() {
    function a(h, f) {
      void 0 === f && (f = 12);
      this.start = this.index = 0;
      this._size = 1 << f;
      this._mask = this._size - 1;
      this.array = new h(this._size);
    }
    a.prototype.get = function(a) {
      return this.array[a];
    };
    a.prototype.forEachInReverse = function(a) {
      if (!this.isEmpty()) {
        for (var f = 0 === this.index ? this._size - 1 : this.index - 1, d = this.start - 1 & this._mask;f !== d && !a(this.array[f], f);) {
          f = 0 === f ? this._size - 1 : f - 1;
        }
      }
    };
    a.prototype.write = function(a) {
      this.array[this.index] = a;
      this.index = this.index + 1 & this._mask;
      this.index === this.start && (this.start = this.start + 1 & this._mask);
    };
    a.prototype.isFull = function() {
      return(this.index + 1 & this._mask) === this.start;
    };
    a.prototype.isEmpty = function() {
      return this.index === this.start;
    };
    a.prototype.reset = function() {
      this.start = this.index = 0;
    };
    return a;
  }();
  g.CircularBuffer = a;
  (function(a) {
    function h(d) {
      return d + (a.BITS_PER_WORD - 1) >> a.ADDRESS_BITS_PER_WORD << a.ADDRESS_BITS_PER_WORD;
    }
    function f(d, a) {
      d = d || "1";
      a = a || "0";
      for (var f = "", q = 0;q < length;q++) {
        f += this.get(q) ? d : a;
      }
      return f;
    }
    function d(d) {
      for (var a = [], f = 0;f < length;f++) {
        this.get(f) && a.push(d ? d[f] : f);
      }
      return a.join(", ");
    }
    var q = g.Debug.assert;
    a.ADDRESS_BITS_PER_WORD = 5;
    a.BITS_PER_WORD = 1 << a.ADDRESS_BITS_PER_WORD;
    a.BIT_INDEX_MASK = a.BITS_PER_WORD - 1;
    var b = function() {
      function d(f) {
        this.size = h(f);
        this.dirty = this.count = 0;
        this.length = f;
        this.bits = new Uint32Array(this.size >> a.ADDRESS_BITS_PER_WORD);
      }
      d.prototype.recount = function() {
        if (this.dirty) {
          for (var d = this.bits, a = 0, f = 0, q = d.length;f < q;f++) {
            var h = d[f], h = h - (h >> 1 & 1431655765), h = (h & 858993459) + (h >> 2 & 858993459), a = a + (16843009 * (h + (h >> 4) & 252645135) >> 24)
          }
          this.count = a;
          this.dirty = 0;
        }
      };
      d.prototype.set = function(d) {
        var f = d >> a.ADDRESS_BITS_PER_WORD, q = this.bits[f];
        d = q | 1 << (d & a.BIT_INDEX_MASK);
        this.bits[f] = d;
        this.dirty |= q ^ d;
      };
      d.prototype.setAll = function() {
        for (var d = this.bits, a = 0, f = d.length;a < f;a++) {
          d[a] = 4294967295;
        }
        this.count = this.size;
        this.dirty = 0;
      };
      d.prototype.assign = function(d) {
        this.count = d.count;
        this.dirty = d.dirty;
        this.size = d.size;
        for (var a = 0, f = this.bits.length;a < f;a++) {
          this.bits[a] = d.bits[a];
        }
      };
      d.prototype.clear = function(d) {
        var f = d >> a.ADDRESS_BITS_PER_WORD, q = this.bits[f];
        d = q & ~(1 << (d & a.BIT_INDEX_MASK));
        this.bits[f] = d;
        this.dirty |= q ^ d;
      };
      d.prototype.get = function(d) {
        return 0 !== (this.bits[d >> a.ADDRESS_BITS_PER_WORD] & 1 << (d & a.BIT_INDEX_MASK));
      };
      d.prototype.clearAll = function() {
        for (var d = this.bits, a = 0, f = d.length;a < f;a++) {
          d[a] = 0;
        }
        this.dirty = this.count = 0;
      };
      d.prototype._union = function(d) {
        var a = this.dirty, f = this.bits;
        d = d.bits;
        for (var q = 0, h = f.length;q < h;q++) {
          var b = f[q], x = b | d[q];
          f[q] = x;
          a |= b ^ x;
        }
        this.dirty = a;
      };
      d.prototype.intersect = function(d) {
        var a = this.dirty, f = this.bits;
        d = d.bits;
        for (var q = 0, h = f.length;q < h;q++) {
          var b = f[q], x = b & d[q];
          f[q] = x;
          a |= b ^ x;
        }
        this.dirty = a;
      };
      d.prototype.subtract = function(d) {
        var a = this.dirty, f = this.bits;
        d = d.bits;
        for (var q = 0, h = f.length;q < h;q++) {
          var b = f[q], x = b & ~d[q];
          f[q] = x;
          a |= b ^ x;
        }
        this.dirty = a;
      };
      d.prototype.negate = function() {
        for (var d = this.dirty, a = this.bits, f = 0, q = a.length;f < q;f++) {
          var h = a[f], b = ~h;
          a[f] = b;
          d |= h ^ b;
        }
        this.dirty = d;
      };
      d.prototype.forEach = function(d) {
        q(d);
        for (var f = this.bits, h = 0, b = f.length;h < b;h++) {
          var x = f[h];
          if (x) {
            for (var s = 0;s < a.BITS_PER_WORD;s++) {
              x & 1 << s && d(h * a.BITS_PER_WORD + s);
            }
          }
        }
      };
      d.prototype.toArray = function() {
        for (var d = [], f = this.bits, q = 0, h = f.length;q < h;q++) {
          var b = f[q];
          if (b) {
            for (var x = 0;x < a.BITS_PER_WORD;x++) {
              b & 1 << x && d.push(q * a.BITS_PER_WORD + x);
            }
          }
        }
        return d;
      };
      d.prototype.equals = function(d) {
        if (this.size !== d.size) {
          return!1;
        }
        var a = this.bits;
        d = d.bits;
        for (var f = 0, q = a.length;f < q;f++) {
          if (a[f] !== d[f]) {
            return!1;
          }
        }
        return!0;
      };
      d.prototype.contains = function(d) {
        if (this.size !== d.size) {
          return!1;
        }
        var a = this.bits;
        d = d.bits;
        for (var f = 0, q = a.length;f < q;f++) {
          if ((a[f] | d[f]) !== a[f]) {
            return!1;
          }
        }
        return!0;
      };
      d.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      d.prototype.clone = function() {
        var a = new d(this.length);
        a._union(this);
        return a;
      };
      return d;
    }();
    a.Uint32ArrayBitSet = b;
    var s = function() {
      function d(a) {
        this.dirty = this.count = 0;
        this.size = h(a);
        this.bits = 0;
        this.singleWord = !0;
        this.length = a;
      }
      d.prototype.recount = function() {
        if (this.dirty) {
          var d = this.bits, d = d - (d >> 1 & 1431655765), d = (d & 858993459) + (d >> 2 & 858993459);
          this.count = 0 + (16843009 * (d + (d >> 4) & 252645135) >> 24);
          this.dirty = 0;
        }
      };
      d.prototype.set = function(d) {
        var f = this.bits;
        this.bits = d = f | 1 << (d & a.BIT_INDEX_MASK);
        this.dirty |= f ^ d;
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
        var f = this.bits;
        this.bits = d = f & ~(1 << (d & a.BIT_INDEX_MASK));
        this.dirty |= f ^ d;
      };
      d.prototype.get = function(d) {
        return 0 !== (this.bits & 1 << (d & a.BIT_INDEX_MASK));
      };
      d.prototype.clearAll = function() {
        this.dirty = this.count = this.bits = 0;
      };
      d.prototype._union = function(d) {
        var a = this.bits;
        this.bits = d = a | d.bits;
        this.dirty = a ^ d;
      };
      d.prototype.intersect = function(d) {
        var a = this.bits;
        this.bits = d = a & d.bits;
        this.dirty = a ^ d;
      };
      d.prototype.subtract = function(d) {
        var a = this.bits;
        this.bits = d = a & ~d.bits;
        this.dirty = a ^ d;
      };
      d.prototype.negate = function() {
        var d = this.bits, a = ~d;
        this.bits = a;
        this.dirty = d ^ a;
      };
      d.prototype.forEach = function(d) {
        q(d);
        var f = this.bits;
        if (f) {
          for (var h = 0;h < a.BITS_PER_WORD;h++) {
            f & 1 << h && d(h);
          }
        }
      };
      d.prototype.toArray = function() {
        var d = [], f = this.bits;
        if (f) {
          for (var q = 0;q < a.BITS_PER_WORD;q++) {
            f & 1 << q && d.push(q);
          }
        }
        return d;
      };
      d.prototype.equals = function(d) {
        return this.bits === d.bits;
      };
      d.prototype.contains = function(d) {
        var a = this.bits;
        return(a | d.bits) === a;
      };
      d.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      d.prototype.clone = function() {
        var a = new d(this.length);
        a._union(this);
        return a;
      };
      return d;
    }();
    a.Uint32BitSet = s;
    s.prototype.toString = d;
    s.prototype.toBitString = f;
    b.prototype.toString = d;
    b.prototype.toBitString = f;
    a.BitSetFunctor = function(d) {
      var f = 1 === h(d) >> a.ADDRESS_BITS_PER_WORD ? s : b;
      return function() {
        return new f(d);
      };
    };
  })(g.BitSets || (g.BitSets = {}));
  a = function() {
    function a() {
    }
    a.randomStyle = function() {
      a._randomStyleCache || (a._randomStyleCache = "#ff5e3a #ff9500 #ffdb4c #87fc70 #52edc7 #1ad6fd #c644fc #ef4db6 #4a4a4a #dbddde #ff3b30 #ff9500 #ffcc00 #4cd964 #34aadc #007aff #5856d6 #ff2d55 #8e8e93 #c7c7cc #5ad427 #c86edf #d1eefc #e0f8d8 #fb2b69 #f7f7f7 #1d77ef #d6cec3 #55efcb #ff4981 #ffd3e0 #f7f7f7 #ff1300 #1f1f21 #bdbec2 #ff3a2d".split(" "));
      return a._randomStyleCache[a._nextStyle++ % a._randomStyleCache.length];
    };
    a.gradientColor = function(h) {
      return a._gradient[a._gradient.length * y.clamp(h, 0, 1) | 0];
    };
    a.contrastStyle = function(a) {
      a = parseInt(a.substr(1), 16);
      return 128 <= (299 * (a >> 16) + 587 * (a >> 8 & 255) + 114 * (a & 255)) / 1E3 ? "#000000" : "#ffffff";
    };
    a.reset = function() {
      a._nextStyle = 0;
    };
    a.TabToolbar = "#252c33";
    a.Toolbars = "#343c45";
    a.HighlightBlue = "#1d4f73";
    a.LightText = "#f5f7fa";
    a.ForegroundText = "#b6babf";
    a.Black = "#000000";
    a.VeryDark = "#14171a";
    a.Dark = "#181d20";
    a.Light = "#a9bacb";
    a.Grey = "#8fa1b2";
    a.DarkGrey = "#5f7387";
    a.Blue = "#46afe3";
    a.Purple = "#6b7abb";
    a.Pink = "#df80ff";
    a.Red = "#eb5368";
    a.Orange = "#d96629";
    a.LightOrange = "#d99b28";
    a.Green = "#70bf53";
    a.BlueGrey = "#5e88b0";
    a._nextStyle = 0;
    a._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
    return a;
  }();
  g.ColorStyle = a;
  a = function() {
    function a(h, f, d, q) {
      this.xMin = h | 0;
      this.yMin = f | 0;
      this.xMax = d | 0;
      this.yMax = q | 0;
    }
    a.FromUntyped = function(h) {
      return new a(h.xMin, h.yMin, h.xMax, h.yMax);
    };
    a.FromRectangle = function(h) {
      return new a(20 * h.x | 0, 20 * h.y | 0, 20 * (h.x + h.width) | 0, 20 * (h.y + h.height) | 0);
    };
    a.prototype.setElements = function(a, f, d, q) {
      this.xMin = a;
      this.yMin = f;
      this.xMax = d;
      this.yMax = q;
    };
    a.prototype.copyFrom = function(a) {
      this.setElements(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    a.prototype.contains = function(a, f) {
      return a < this.xMin !== a < this.xMax && f < this.yMin !== f < this.yMax;
    };
    a.prototype.unionInPlace = function(a) {
      a.isEmpty() || (this.extendByPoint(a.xMin, a.yMin), this.extendByPoint(a.xMax, a.yMax));
    };
    a.prototype.extendByPoint = function(a, f) {
      this.extendByX(a);
      this.extendByY(f);
    };
    a.prototype.extendByX = function(a) {
      134217728 === this.xMin ? this.xMin = this.xMax = a : (this.xMin = Math.min(this.xMin, a), this.xMax = Math.max(this.xMax, a));
    };
    a.prototype.extendByY = function(a) {
      134217728 === this.yMin ? this.yMin = this.yMax = a : (this.yMin = Math.min(this.yMin, a), this.yMax = Math.max(this.yMax, a));
    };
    a.prototype.intersects = function(a) {
      return this.contains(a.xMin, a.yMin) || this.contains(a.xMax, a.yMax);
    };
    a.prototype.isEmpty = function() {
      return this.xMax <= this.xMin || this.yMax <= this.yMin;
    };
    Object.defineProperty(a.prototype, "width", {get:function() {
      return this.xMax - this.xMin;
    }, set:function(a) {
      this.xMax = this.xMin + a;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "height", {get:function() {
      return this.yMax - this.yMin;
    }, set:function(a) {
      this.yMax = this.yMin + a;
    }, enumerable:!0, configurable:!0});
    a.prototype.getBaseWidth = function(a) {
      return Math.abs(Math.cos(a)) * (this.xMax - this.xMin) + Math.abs(Math.sin(a)) * (this.yMax - this.yMin);
    };
    a.prototype.getBaseHeight = function(a) {
      return Math.abs(Math.sin(a)) * (this.xMax - this.xMin) + Math.abs(Math.cos(a)) * (this.yMax - this.yMin);
    };
    a.prototype.setEmpty = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 0;
    };
    a.prototype.setToSentinels = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 134217728;
    };
    a.prototype.clone = function() {
      return new a(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    a.prototype.toString = function() {
      return "{ xMin: " + this.xMin + ", xMin: " + this.yMin + ", xMax: " + this.xMax + ", xMax: " + this.yMax + " }";
    };
    return a;
  }();
  g.Bounds = a;
  a = function() {
    function a(h, f, d, q) {
      k.assert(m(h));
      k.assert(m(f));
      k.assert(m(d));
      k.assert(m(q));
      this._xMin = h | 0;
      this._yMin = f | 0;
      this._xMax = d | 0;
      this._yMax = q | 0;
    }
    a.FromUntyped = function(h) {
      return new a(h.xMin, h.yMin, h.xMax, h.yMax);
    };
    a.FromRectangle = function(h) {
      return new a(20 * h.x | 0, 20 * h.y | 0, 20 * (h.x + h.width) | 0, 20 * (h.y + h.height) | 0);
    };
    a.prototype.setElements = function(a, f, d, q) {
      this.xMin = a;
      this.yMin = f;
      this.xMax = d;
      this.yMax = q;
    };
    a.prototype.copyFrom = function(a) {
      this.setElements(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    a.prototype.contains = function(a, f) {
      return a < this.xMin !== a < this.xMax && f < this.yMin !== f < this.yMax;
    };
    a.prototype.unionInPlace = function(a) {
      a.isEmpty() || (this.extendByPoint(a.xMin, a.yMin), this.extendByPoint(a.xMax, a.yMax));
    };
    a.prototype.extendByPoint = function(a, f) {
      this.extendByX(a);
      this.extendByY(f);
    };
    a.prototype.extendByX = function(a) {
      134217728 === this.xMin ? this.xMin = this.xMax = a : (this.xMin = Math.min(this.xMin, a), this.xMax = Math.max(this.xMax, a));
    };
    a.prototype.extendByY = function(a) {
      134217728 === this.yMin ? this.yMin = this.yMax = a : (this.yMin = Math.min(this.yMin, a), this.yMax = Math.max(this.yMax, a));
    };
    a.prototype.intersects = function(a) {
      return this.contains(a._xMin, a._yMin) || this.contains(a._xMax, a._yMax);
    };
    a.prototype.isEmpty = function() {
      return this._xMax <= this._xMin || this._yMax <= this._yMin;
    };
    Object.defineProperty(a.prototype, "xMin", {get:function() {
      return this._xMin;
    }, set:function(a) {
      k.assert(m(a));
      this._xMin = a;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "yMin", {get:function() {
      return this._yMin;
    }, set:function(a) {
      k.assert(m(a));
      this._yMin = a | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "xMax", {get:function() {
      return this._xMax;
    }, set:function(a) {
      k.assert(m(a));
      this._xMax = a | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "width", {get:function() {
      return this._xMax - this._xMin;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "yMax", {get:function() {
      return this._yMax;
    }, set:function(a) {
      k.assert(m(a));
      this._yMax = a | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "height", {get:function() {
      return this._yMax - this._yMin;
    }, enumerable:!0, configurable:!0});
    a.prototype.getBaseWidth = function(a) {
      return Math.abs(Math.cos(a)) * (this._xMax - this._xMin) + Math.abs(Math.sin(a)) * (this._yMax - this._yMin);
    };
    a.prototype.getBaseHeight = function(a) {
      return Math.abs(Math.sin(a)) * (this._xMax - this._xMin) + Math.abs(Math.cos(a)) * (this._yMax - this._yMin);
    };
    a.prototype.setEmpty = function() {
      this._xMin = this._yMin = this._xMax = this._yMax = 0;
    };
    a.prototype.clone = function() {
      return new a(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    a.prototype.toString = function() {
      return "{ xMin: " + this._xMin + ", xMin: " + this._yMin + ", xMax: " + this._xMax + ", yMax: " + this._yMax + " }";
    };
    a.prototype.assertValid = function() {
    };
    return a;
  }();
  g.DebugBounds = a;
  a = function() {
    function a(h, f, d, q) {
      this.r = h;
      this.g = f;
      this.b = d;
      this.a = q;
    }
    a.FromARGB = function(h) {
      return new a((h >> 16 & 255) / 255, (h >> 8 & 255) / 255, (h >> 0 & 255) / 255, (h >> 24 & 255) / 255);
    };
    a.FromRGBA = function(h) {
      return a.FromARGB(t.RGBAToARGB(h));
    };
    a.prototype.toRGBA = function() {
      return 255 * this.r << 24 | 255 * this.g << 16 | 255 * this.b << 8 | 255 * this.a;
    };
    a.prototype.toCSSStyle = function() {
      return t.rgbaToCSSStyle(this.toRGBA());
    };
    a.prototype.set = function(a) {
      this.r = a.r;
      this.g = a.g;
      this.b = a.b;
      this.a = a.a;
    };
    a.randomColor = function() {
      var h = .4;
      void 0 === h && (h = 1);
      return new a(Math.random(), Math.random(), Math.random(), h);
    };
    a.parseColor = function(h) {
      a.colorCache || (a.colorCache = Object.create(null));
      if (a.colorCache[h]) {
        return a.colorCache[h];
      }
      var f = document.createElement("span");
      document.body.appendChild(f);
      f.style.backgroundColor = h;
      var d = getComputedStyle(f).backgroundColor;
      document.body.removeChild(f);
      (f = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(d)) || (f = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(d));
      d = new a(0, 0, 0, 0);
      d.r = parseFloat(f[1]) / 255;
      d.g = parseFloat(f[2]) / 255;
      d.b = parseFloat(f[3]) / 255;
      d.a = f[4] ? parseFloat(f[4]) / 255 : 1;
      return a.colorCache[h] = d;
    };
    a.Red = new a(1, 0, 0, 1);
    a.Green = new a(0, 1, 0, 1);
    a.Blue = new a(0, 0, 1, 1);
    a.None = new a(0, 0, 0, 0);
    a.White = new a(1, 1, 1, 1);
    a.Black = new a(0, 0, 0, 1);
    a.colorCache = {};
    return a;
  }();
  g.Color = a;
  var t;
  (function(a) {
    function h(a) {
      var d, f, h = a >> 24 & 255;
      f = (Math.imul(a >> 16 & 255, h) + 127) / 255 | 0;
      d = (Math.imul(a >> 8 & 255, h) + 127) / 255 | 0;
      a = (Math.imul(a >> 0 & 255, h) + 127) / 255 | 0;
      return h << 24 | f << 16 | d << 8 | a;
    }
    a.RGBAToARGB = function(a) {
      return a >> 8 & 16777215 | (a & 255) << 24;
    };
    a.ARGBToRGBA = function(a) {
      return a << 8 | a >> 24 & 255;
    };
    a.rgbaToCSSStyle = function(a) {
      return g.StringUtilities.concat9("rgba(", a >> 24 & 255, ",", a >> 16 & 255, ",", a >> 8 & 255, ",", (a & 255) / 255, ")");
    };
    a.cssStyleToRGBA = function(a) {
      if ("#" === a[0]) {
        if (7 === a.length) {
          return parseInt(a.substring(1), 16) << 8 | 255;
        }
      } else {
        if ("r" === a[0]) {
          return a = a.substring(5, a.length - 1).split(","), (parseInt(a[0]) & 255) << 24 | (parseInt(a[1]) & 255) << 16 | (parseInt(a[2]) & 255) << 8 | 255 * parseFloat(a[3]) & 255;
        }
      }
      return 4278190335;
    };
    a.hexToRGB = function(a) {
      return parseInt(a.slice(1), 16);
    };
    a.rgbToHex = function(a) {
      return "#" + ("000000" + (a >>> 0).toString(16)).slice(-6);
    };
    a.isValidHexColor = function(a) {
      return/^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(a);
    };
    a.clampByte = function(a) {
      return Math.max(0, Math.min(255, a));
    };
    a.unpremultiplyARGB = function(a) {
      var d, f, h = a >> 24 & 255;
      f = Math.imul(255, a >> 16 & 255) / h & 255;
      d = Math.imul(255, a >> 8 & 255) / h & 255;
      a = Math.imul(255, a >> 0 & 255) / h & 255;
      return h << 24 | f << 16 | d << 8 | a;
    };
    a.premultiplyARGB = h;
    var f;
    a.ensureUnpremultiplyTable = function() {
      if (!f) {
        f = new Uint8Array(65536);
        for (var a = 0;256 > a;a++) {
          for (var d = 0;256 > d;d++) {
            f[(d << 8) + a] = Math.imul(255, a) / d;
          }
        }
      }
    };
    a.tableLookupUnpremultiplyARGB = function(a) {
      a |= 0;
      var d = a >> 24 & 255;
      if (0 === d) {
        return 0;
      }
      if (255 === d) {
        return a;
      }
      var h, b, n = d << 8, p = f;
      b = p[n + (a >> 16 & 255)];
      h = p[n + (a >> 8 & 255)];
      a = p[n + (a >> 0 & 255)];
      return d << 24 | b << 16 | h << 8 | a;
    };
    a.blendPremultipliedBGRA = function(a, d) {
      var f, h;
      h = 256 - (d & 255);
      f = Math.imul(a & 16711935, h) >> 8;
      h = Math.imul(a >> 8 & 16711935, h) >> 8;
      return((d >> 8 & 16711935) + h & 16711935) << 8 | (d & 16711935) + f & 16711935;
    };
    var d = v.swap32;
    a.convertImage = function(a, b, s, n) {
      s !== n && k.assert(s.buffer !== n.buffer, "Can't handle overlapping views.");
      var p = s.length;
      if (a === b) {
        if (s !== n) {
          for (a = 0;a < p;a++) {
            n[a] = s[a];
          }
        }
      } else {
        if (1 === a && 3 === b) {
          for (g.ColorUtilities.ensureUnpremultiplyTable(), a = 0;a < p;a++) {
            var c = s[a];
            b = c & 255;
            if (0 === b) {
              n[a] = 0;
            } else {
              if (255 === b) {
                n[a] = 4278190080 | c >> 8 & 16777215;
              } else {
                var u = c >> 24 & 255, l = c >> 16 & 255, c = c >> 8 & 255, y = b << 8, t = f, c = t[y + c], l = t[y + l], u = t[y + u];
                n[a] = b << 24 | u << 16 | l << 8 | c;
              }
            }
          }
        } else {
          if (2 === a && 3 === b) {
            for (a = 0;a < p;a++) {
              n[a] = d(s[a]);
            }
          } else {
            if (3 === a && 1 === b) {
              for (a = 0;a < p;a++) {
                b = s[a], n[a] = d(h(b & 4278255360 | b >> 16 & 255 | (b & 255) << 16));
              }
            } else {
              for (k.somewhatImplemented("Image Format Conversion: " + r[a] + " -> " + r[b]), a = 0;a < p;a++) {
                n[a] = s[a];
              }
            }
          }
        }
      }
    };
  })(t = g.ColorUtilities || (g.ColorUtilities = {}));
  a = function() {
    function a(h) {
      void 0 === h && (h = 32);
      this._list = [];
      this._maxSize = h;
    }
    a.prototype.acquire = function(h) {
      if (a._enabled) {
        for (var f = this._list, d = 0;d < f.length;d++) {
          var q = f[d];
          if (q.byteLength >= h) {
            return f.splice(d, 1), q;
          }
        }
      }
      return new ArrayBuffer(h);
    };
    a.prototype.release = function(h) {
      if (a._enabled) {
        var f = this._list;
        k.assert(0 > b.indexOf(f, h));
        f.length === this._maxSize && f.shift();
        f.push(h);
      }
    };
    a.prototype.ensureUint8ArrayLength = function(a, f) {
      if (a.length >= f) {
        return a;
      }
      var d = Math.max(a.length + f, (3 * a.length >> 1) + 1), d = new Uint8Array(this.acquire(d), 0, d);
      d.set(a);
      this.release(a.buffer);
      return d;
    };
    a.prototype.ensureFloat64ArrayLength = function(a, f) {
      if (a.length >= f) {
        return a;
      }
      var d = Math.max(a.length + f, (3 * a.length >> 1) + 1), d = new Float64Array(this.acquire(d * Float64Array.BYTES_PER_ELEMENT), 0, d);
      d.set(a);
      this.release(a.buffer);
      return d;
    };
    a._enabled = !0;
    return a;
  }();
  g.ArrayBufferPool = a;
  (function(a) {
    (function(a) {
      a[a.EXTERNAL_INTERFACE_FEATURE = 1] = "EXTERNAL_INTERFACE_FEATURE";
      a[a.CLIPBOARD_FEATURE = 2] = "CLIPBOARD_FEATURE";
      a[a.SHAREDOBJECT_FEATURE = 3] = "SHAREDOBJECT_FEATURE";
      a[a.VIDEO_FEATURE = 4] = "VIDEO_FEATURE";
      a[a.SOUND_FEATURE = 5] = "SOUND_FEATURE";
      a[a.NETCONNECTION_FEATURE = 6] = "NETCONNECTION_FEATURE";
    })(a.Feature || (a.Feature = {}));
    (function(a) {
      a[a.AVM1_ERROR = 1] = "AVM1_ERROR";
      a[a.AVM2_ERROR = 2] = "AVM2_ERROR";
    })(a.ErrorTypes || (a.ErrorTypes = {}));
    a.instance;
  })(g.Telemetry || (g.Telemetry = {}));
  (function(a) {
    a.instance;
  })(g.FileLoadingService || (g.FileLoadingService = {}));
  g.registerCSSFont = function(a, b, f) {
    if (inBrowser) {
      var d = document.head;
      d.insertBefore(document.createElement("style"), d.firstChild);
      d = document.styleSheets[0];
      b = "@font-face{font-family:swffont" + a + ";src:url(data:font/opentype;base64," + g.StringUtilities.base64ArrayBuffer(b) + ")}";
      d.insertRule(b, d.cssRules.length);
      f && (f = document.createElement("div"), f.style.fontFamily = "swffont" + a, f.innerHTML = "hello", document.body.appendChild(f), document.body.removeChild(f));
    } else {
      k.warning("Cannot register CSS font outside the browser");
    }
  };
  (function(a) {
    a.instance = {enabled:!1, initJS:function(a) {
    }, registerCallback:function(a) {
    }, unregisterCallback:function(a) {
    }, eval:function(a) {
    }, call:function(a) {
    }, getId:function() {
      return null;
    }};
  })(g.ExternalInterfaceService || (g.ExternalInterfaceService = {}));
  a = function() {
    function a() {
    }
    a.prototype.setClipboard = function(a) {
      k.abstractMethod("public ClipboardService::setClipboard");
    };
    a.instance = null;
    return a;
  }();
  g.ClipboardService = a;
  a = function() {
    function a() {
      this._queues = {};
    }
    a.prototype.register = function(a, f) {
      k.assert(a);
      k.assert(f);
      var d = this._queues[a];
      if (d) {
        if (-1 < d.indexOf(f)) {
          return;
        }
      } else {
        d = this._queues[a] = [];
      }
      d.push(f);
    };
    a.prototype.unregister = function(a, f) {
      k.assert(a);
      k.assert(f);
      var d = this._queues[a];
      if (d) {
        var q = d.indexOf(f);
        -1 !== q && d.splice(q, 1);
        0 === d.length && (this._queues[a] = null);
      }
    };
    a.prototype.notify = function(a, f) {
      var d = this._queues[a];
      if (d) {
        d = d.slice();
        f = Array.prototype.slice.call(arguments, 0);
        for (var q = 0;q < d.length;q++) {
          d[q].apply(null, f);
        }
      }
    };
    a.prototype.notify1 = function(a, f) {
      var d = this._queues[a];
      if (d) {
        for (var d = d.slice(), q = 0;q < d.length;q++) {
          (0,d[q])(a, f);
        }
      }
    };
    return a;
  }();
  g.Callback = a;
  (function(a) {
    a[a.None = 0] = "None";
    a[a.PremultipliedAlphaARGB = 1] = "PremultipliedAlphaARGB";
    a[a.StraightAlphaARGB = 2] = "StraightAlphaARGB";
    a[a.StraightAlphaRGBA = 3] = "StraightAlphaRGBA";
    a[a.JPEG = 4] = "JPEG";
    a[a.PNG = 5] = "PNG";
    a[a.GIF = 6] = "GIF";
  })(g.ImageType || (g.ImageType = {}));
  var r = g.ImageType;
  g.getMIMETypeForImageType = function(a) {
    switch(a) {
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
  (function(a) {
    a.toCSSCursor = function(a) {
      switch(a) {
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
  })(g.UI || (g.UI = {}));
  a = function() {
    function a() {
      this.promise = new Promise(function(a, f) {
        this.resolve = a;
        this.reject = f;
      }.bind(this));
    }
    a.prototype.then = function(a, f) {
      return this.promise.then(a, f);
    };
    return a;
  }();
  g.PromiseWrapper = a;
})(Shumway || (Shumway = {}));
(function() {
  function g(a) {
    if ("function" !== typeof a) {
      throw new TypeError("Invalid deferred constructor");
    }
    var f = l();
    a = new a(f);
    var b = f.resolve;
    if ("function" !== typeof b) {
      throw new TypeError("Invalid resolve construction function");
    }
    f = f.reject;
    if ("function" !== typeof f) {
      throw new TypeError("Invalid reject construction function");
    }
    return{promise:a, resolve:b, reject:f};
  }
  function m(a, f) {
    if ("object" !== typeof a || null === a) {
      return!1;
    }
    try {
      var b = a.then;
      if ("function" !== typeof b) {
        return!1;
      }
      b.call(a, f.resolve, f.reject);
    } catch (h) {
      b = f.reject, b(h);
    }
    return!0;
  }
  function e(a) {
    return "object" === typeof a && null !== a && "undefined" !== typeof a.promiseStatus;
  }
  function c(a, f) {
    if ("unresolved" === a.promiseStatus) {
      var b = a.rejectReactions;
      a.result = f;
      a.resolveReactions = void 0;
      a.rejectReactions = void 0;
      a.promiseStatus = "has-rejection";
      w(b, f);
    }
  }
  function w(a, f) {
    for (var b = 0;b < a.length;b++) {
      k({reaction:a[b], argument:f});
    }
  }
  function k(d) {
    0 === f.length && setTimeout(a, 0);
    f.push(d);
  }
  function b(a, f) {
    var b = a.deferred, h = a.handler, n, p;
    try {
      n = h(f);
    } catch (c) {
      return b = b.reject, b(c);
    }
    if (n === b.promise) {
      return b = b.reject, b(new TypeError("Self resolution"));
    }
    try {
      if (p = m(n, b), !p) {
        var r = b.resolve;
        return r(n);
      }
    } catch (k) {
      return b = b.reject, b(k);
    }
  }
  function a() {
    for (;0 < f.length;) {
      var a = f[0];
      try {
        b(a.reaction, a.argument);
      } catch (q) {
        if ("function" === typeof u.onerror) {
          u.onerror(q);
        }
      }
      f.shift();
    }
  }
  function n(a) {
    throw a;
  }
  function p(a) {
    return a;
  }
  function y(a) {
    return function(f) {
      c(a, f);
    };
  }
  function v(a) {
    return function(f) {
      if ("unresolved" === a.promiseStatus) {
        var b = a.resolveReactions;
        a.result = f;
        a.resolveReactions = void 0;
        a.rejectReactions = void 0;
        a.promiseStatus = "has-resolution";
        w(b, f);
      }
    };
  }
  function l() {
    function a(f, b) {
      a.resolve = f;
      a.reject = b;
    }
    return a;
  }
  function t(a, f, b) {
    return function(h) {
      if (h === a) {
        return b(new TypeError("Self resolution"));
      }
      var n = a.promiseConstructor;
      if (e(h) && h.promiseConstructor === n) {
        return h.then(f, b);
      }
      n = g(n);
      return m(h, n) ? n.promise.then(f, b) : f(h);
    };
  }
  function r(a, f, b, h) {
    return function(n) {
      f[a] = n;
      h.countdown--;
      0 === h.countdown && b.resolve(f);
    };
  }
  function u(a) {
    if ("function" !== typeof a) {
      throw new TypeError("resolver is not a function");
    }
    if ("object" !== typeof this) {
      throw new TypeError("Promise to initialize is not an object");
    }
    this.promiseStatus = "unresolved";
    this.resolveReactions = [];
    this.rejectReactions = [];
    this.result = void 0;
    var f = v(this), b = y(this);
    try {
      a(f, b);
    } catch (h) {
      c(this, h);
    }
    this.promiseConstructor = u;
    return this;
  }
  var h = Function("return this")();
  if (h.Promise) {
    "function" !== typeof h.Promise.all && (h.Promise.all = function(a) {
      var f = 0, b = [], s, n, p = new h.Promise(function(a, d) {
        s = a;
        n = d;
      });
      a.forEach(function(a, d) {
        f++;
        a.then(function(a) {
          b[d] = a;
          f--;
          0 === f && s(b);
        }, n);
      });
      0 === f && s(b);
      return p;
    }), "function" !== typeof h.Promise.resolve && (h.Promise.resolve = function(a) {
      return new h.Promise(function(f) {
        f(a);
      });
    });
  } else {
    var f = [];
    u.all = function(a) {
      var f = g(this), b = [], h = {countdown:0}, n = 0;
      a.forEach(function(a) {
        this.cast(a).then(r(n, b, f, h), f.reject);
        n++;
        h.countdown++;
      }, this);
      0 === n && f.resolve(b);
      return f.promise;
    };
    u.cast = function(a) {
      if (e(a)) {
        return a;
      }
      var f = g(this);
      f.resolve(a);
      return f.promise;
    };
    u.reject = function(a) {
      var f = g(this);
      f.reject(a);
      return f.promise;
    };
    u.resolve = function(a) {
      var f = g(this);
      f.resolve(a);
      return f.promise;
    };
    u.prototype = {"catch":function(a) {
      this.then(void 0, a);
    }, then:function(a, f) {
      if (!e(this)) {
        throw new TypeError("this is not a Promises");
      }
      var b = g(this.promiseConstructor), h = "function" === typeof f ? f : n, c = {deferred:b, handler:t(this, "function" === typeof a ? a : p, h)}, h = {deferred:b, handler:h};
      switch(this.promiseStatus) {
        case "unresolved":
          this.resolveReactions.push(c);
          this.rejectReactions.push(h);
          break;
        case "has-resolution":
          k({reaction:c, argument:this.result});
          break;
        case "has-rejection":
          k({reaction:h, argument:this.result});
      }
      return b.promise;
    }};
    h.Promise = u;
  }
})();
"undefined" !== typeof exports && (exports.Shumway = Shumway);
(function() {
  function g(g, e, c) {
    g[e] || Object.defineProperty(g, e, {value:c, writable:!0, configurable:!0, enumerable:!1});
  }
  g(String.prototype, "padRight", function(g, e) {
    var c = this, w = c.replace(/\033\[[0-9]*m/g, "").length;
    if (!g || w >= e) {
      return c;
    }
    for (var w = (e - w) / g.length, k = 0;k < w;k++) {
      c += g;
    }
    return c;
  });
  g(String.prototype, "padLeft", function(g, e) {
    var c = this, w = c.length;
    if (!g || w >= e) {
      return c;
    }
    for (var w = (e - w) / g.length, k = 0;k < w;k++) {
      c = g + c;
    }
    return c;
  });
  g(String.prototype, "trim", function() {
    return this.replace(/^\s+|\s+$/g, "");
  });
  g(String.prototype, "endsWith", function(g) {
    return-1 !== this.indexOf(g, this.length - g.length);
  });
  g(Array.prototype, "replace", function(g, e) {
    if (g === e) {
      return 0;
    }
    for (var c = 0, w = 0;w < this.length;w++) {
      this[w] === g && (this[w] = e, c++);
    }
    return c;
  });
})();
(function(g) {
  (function(m) {
    var e = g.isObject, c = g.Debug.assert, w = function() {
      function a(b, n, c, k) {
        this.shortName = b;
        this.longName = n;
        this.type = c;
        k = k || {};
        this.positional = k.positional;
        this.parseFn = k.parse;
        this.value = k.defaultValue;
      }
      a.prototype.parse = function(a) {
        "boolean" === this.type ? (c("boolean" === typeof a), this.value = a) : "number" === this.type ? (c(!isNaN(a), a + " is not a number"), this.value = parseInt(a, 10)) : this.value = a;
        this.parseFn && this.parseFn(this.value);
      };
      return a;
    }();
    m.Argument = w;
    var k = function() {
      function n() {
        this.args = [];
      }
      n.prototype.addArgument = function(a, b, n, c) {
        a = new w(a, b, n, c);
        this.args.push(a);
        return a;
      };
      n.prototype.addBoundOption = function(a) {
        this.args.push(new w(a.shortName, a.longName, a.type, {parse:function(b) {
          a.value = b;
        }}));
      };
      n.prototype.addBoundOptionSet = function(n) {
        var k = this;
        n.options.forEach(function(n) {
          n instanceof b ? k.addBoundOptionSet(n) : (c(n instanceof a), k.addBoundOption(n));
        });
      };
      n.prototype.getUsage = function() {
        var a = "";
        this.args.forEach(function(b) {
          a = b.positional ? a + b.longName : a + ("[-" + b.shortName + "|--" + b.longName + ("boolean" === b.type ? "" : " " + b.type[0].toUpperCase()) + "]");
          a += " ";
        });
        return a;
      };
      n.prototype.parse = function(a) {
        var b = {}, n = [];
        this.args.forEach(function(a) {
          a.positional ? n.push(a) : (b["-" + a.shortName] = a, b["--" + a.longName] = a);
        });
        for (var k = [];a.length;) {
          var t = a.shift(), r = null, u = t;
          if ("--" == t) {
            k = k.concat(a);
            break;
          } else {
            if ("-" == t.slice(0, 1) || "--" == t.slice(0, 2)) {
              r = b[t];
              if (!r) {
                continue;
              }
              "boolean" !== r.type ? (u = a.shift(), c("-" !== u && "--" !== u, "Argument " + t + " must have a value.")) : u = !0;
            } else {
              n.length ? r = n.shift() : k.push(u);
            }
          }
          r && r.parse(u);
        }
        c(0 === n.length, "Missing positional arguments.");
        return k;
      };
      return n;
    }();
    m.ArgumentParser = k;
    var b = function() {
      function a(b, n) {
        void 0 === n && (n = null);
        this.open = !1;
        this.name = b;
        this.settings = n || {};
        this.options = [];
      }
      a.prototype.register = function(b) {
        if (b instanceof a) {
          for (var c = 0;c < this.options.length;c++) {
            var k = this.options[c];
            if (k instanceof a && k.name === b.name) {
              return k;
            }
          }
        }
        this.options.push(b);
        if (this.settings) {
          if (b instanceof a) {
            c = this.settings[b.name], e(c) && (b.settings = c.settings, b.open = c.open);
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
      a.prototype.trace = function(a) {
        a.enter(this.name + " {");
        this.options.forEach(function(b) {
          b.trace(a);
        });
        a.leave("}");
      };
      a.prototype.getSettings = function() {
        var b = {};
        this.options.forEach(function(c) {
          c instanceof a ? b[c.name] = {settings:c.getSettings(), open:c.open} : b[c.longName] = c.value;
        });
        return b;
      };
      a.prototype.setSettings = function(b) {
        b && this.options.forEach(function(c) {
          c instanceof a ? c.name in b && c.setSettings(b[c.name].settings) : c.longName in b && (c.value = b[c.longName]);
        });
      };
      return a;
    }();
    m.OptionSet = b;
    var a = function() {
      function a(b, n, c, k, t, r) {
        void 0 === r && (r = null);
        this.longName = n;
        this.shortName = b;
        this.type = c;
        this.value = this.defaultValue = k;
        this.description = t;
        this.config = r;
      }
      a.prototype.parse = function(a) {
        this.value = a;
      };
      a.prototype.trace = function(a) {
        a.writeLn(("-" + this.shortName + "|--" + this.longName).padRight(" ", 30) + " = " + this.type + " " + this.value + " [" + this.defaultValue + "] (" + this.description + ")");
      };
      return a;
    }();
    m.Option = a;
  })(g.Options || (g.Options = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    function e() {
      try {
        return "undefined" !== typeof window && "localStorage" in window && null !== window.localStorage;
      } catch (c) {
        return!1;
      }
    }
    function c(c) {
      void 0 === c && (c = m.ROOT);
      var k = {};
      if (e() && (c = window.localStorage[c])) {
        try {
          k = JSON.parse(c);
        } catch (b) {
        }
      }
      return k;
    }
    m.ROOT = "Shumway Options";
    m.shumwayOptions = new g.Options.OptionSet(m.ROOT, c());
    m.isStorageSupported = e;
    m.load = c;
    m.save = function(c, k) {
      void 0 === c && (c = null);
      void 0 === k && (k = m.ROOT);
      if (e()) {
        try {
          window.localStorage[k] = JSON.stringify(c ? c : m.shumwayOptions.getSettings());
        } catch (b) {
        }
      }
    };
    m.setSettings = function(c) {
      m.shumwayOptions.setSettings(c);
    };
    m.getSettings = function(c) {
      return m.shumwayOptions.getSettings();
    };
  })(g.Settings || (g.Settings = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    var e = function() {
      function c(c, k) {
        this._parent = c;
        this._timers = g.ObjectUtilities.createMap();
        this._name = k;
        this._count = this._total = this._last = this._begin = 0;
      }
      c.time = function(e, k) {
        c.start(e);
        k();
        c.stop();
      };
      c.start = function(e) {
        c._top = c._top._timers[e] || (c._top._timers[e] = new c(c._top, e));
        c._top.start();
        e = c._flat._timers[e] || (c._flat._timers[e] = new c(c._flat, e));
        e.start();
        c._flatStack.push(e);
      };
      c.stop = function() {
        c._top.stop();
        c._top = c._top._parent;
        c._flatStack.pop().stop();
      };
      c.stopStart = function(e) {
        c.stop();
        c.start(e);
      };
      c.prototype.start = function() {
        this._begin = g.getTicks();
      };
      c.prototype.stop = function() {
        this._last = g.getTicks() - this._begin;
        this._total += this._last;
        this._count += 1;
      };
      c.prototype.toJSON = function() {
        return{name:this._name, total:this._total, timers:this._timers};
      };
      c.prototype.trace = function(c) {
        c.enter(this._name + ": " + this._total.toFixed(2) + " ms, count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var k in this._timers) {
          this._timers[k].trace(c);
        }
        c.outdent();
      };
      c.trace = function(e) {
        c._base.trace(e);
        c._flat.trace(e);
      };
      c._base = new c(null, "Total");
      c._top = c._base;
      c._flat = new c(null, "Flat");
      c._flatStack = [];
      return c;
    }();
    m.Timer = e;
    e = function() {
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
        this._counts = g.ObjectUtilities.createMap();
        this._times = g.ObjectUtilities.createMap();
      };
      c.prototype.toJSON = function() {
        return{counts:this._counts, times:this._times};
      };
      c.prototype.count = function(c, k, b) {
        void 0 === k && (k = 1);
        void 0 === b && (b = 0);
        if (this._enabled) {
          return void 0 === this._counts[c] && (this._counts[c] = 0, this._times[c] = 0), this._counts[c] += k, this._times[c] += b, this._counts[c];
        }
      };
      c.prototype.trace = function(c) {
        for (var k in this._counts) {
          c.writeLn(k + ": " + this._counts[k]);
        }
      };
      c.prototype._pairToString = function(c, k) {
        var b = k[0], a = k[1], n = c[b], b = b + ": " + a;
        n && (b += ", " + n.toFixed(4), 1 < a && (b += " (" + (n / a).toFixed(4) + ")"));
        return b;
      };
      c.prototype.toStringSorted = function() {
        var c = this, k = this._times, b = [], a;
        for (a in this._counts) {
          b.push([a, this._counts[a]]);
        }
        b.sort(function(a, b) {
          return b[1] - a[1];
        });
        return b.map(function(a) {
          return c._pairToString(k, a);
        }).join(", ");
      };
      c.prototype.traceSorted = function(c, k) {
        void 0 === k && (k = !1);
        var b = this, a = this._times, n = [], p;
        for (p in this._counts) {
          n.push([p, this._counts[p]]);
        }
        n.sort(function(a, b) {
          return b[1] - a[1];
        });
        k ? c.writeLn(n.map(function(n) {
          return b._pairToString(a, n);
        }).join(", ")) : n.forEach(function(n) {
          c.writeLn(b._pairToString(a, n));
        });
      };
      c.instance = new c(!0);
      return c;
    }();
    m.Counter = e;
    e = function() {
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
        for (var c = 0, k = 0;k < this._count;k++) {
          c += this._samples[k];
        }
        return c / this._count;
      };
      return c;
    }();
    m.Average = e;
  })(g.Metrics || (g.Metrics = {}));
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function(g, m) {
  function e() {
    this.constructor = g;
  }
  for (var c in m) {
    m.hasOwnProperty(c) && (g[c] = m[c]);
  }
  e.prototype = m.prototype;
  g.prototype = new e;
};
(function(g) {
  (function(m) {
    function e(a) {
      for (var f = Math.max.apply(null, a), d = a.length, q = 1 << f, b = new Uint32Array(q), s = f << 16 | 65535, n = 0;n < q;n++) {
        b[n] = s;
      }
      for (var s = 0, n = 1, c = 2;n <= f;s <<= 1, ++n, c <<= 1) {
        for (var p = 0;p < d;++p) {
          if (a[p] === n) {
            for (var r = 0, k = 0;k < n;++k) {
              r = 2 * r + (s >> k & 1);
            }
            for (k = r;k < q;k += c) {
              b[k] = n << 16 | p;
            }
            ++s;
          }
        }
      }
      return{codes:b, maxBits:f};
    }
    var c;
    (function(a) {
      a[a.INIT = 0] = "INIT";
      a[a.BLOCK_0 = 1] = "BLOCK_0";
      a[a.BLOCK_1 = 2] = "BLOCK_1";
      a[a.BLOCK_2_PRE = 3] = "BLOCK_2_PRE";
      a[a.BLOCK_2 = 4] = "BLOCK_2";
      a[a.DONE = 5] = "DONE";
      a[a.ERROR = 6] = "ERROR";
      a[a.VERIFY_HEADER = 7] = "VERIFY_HEADER";
    })(c || (c = {}));
    c = function() {
      function a(f) {
        this._error = null;
      }
      Object.defineProperty(a.prototype, "error", {get:function() {
        return this._error;
      }, enumerable:!0, configurable:!0});
      a.prototype.push = function(a) {
        g.Debug.abstractMethod("Inflate.push");
      };
      a.prototype.close = function() {
      };
      a.create = function(a) {
        return "undefined" !== typeof SpecialInflate ? new t(a) : new w(a);
      };
      a.prototype._processZLibHeader = function(a, d, q) {
        if (d + 2 > q) {
          return 0;
        }
        a = a[d] << 8 | a[d + 1];
        d = null;
        2048 !== (a & 3840) ? d = "inflate: unknown compression method" : 0 !== a % 31 ? d = "inflate: bad FCHECK" : 0 !== (a & 32) && (d = "inflate: FDICT bit set");
        return d ? (this._error = d, -1) : 2;
      };
      a.inflate = function(f, d, q) {
        var b = new Uint8Array(d), s = 0;
        d = a.create(q);
        d.onData = function(a) {
          b.set(a, s);
          s += a.length;
        };
        d.push(f);
        d.close();
        return b;
      };
      return a;
    }();
    m.Inflate = c;
    var w = function(h) {
      function f(d) {
        h.call(this, d);
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
        this._error = void 0;
        if (!l) {
          k = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);
          b = new Uint16Array(30);
          a = new Uint8Array(30);
          for (var f = d = 0, x = 1;30 > d;++d) {
            b[d] = x, x += 1 << (a[d] = ~~((f += 2 < d ? 1 : 0) / 2));
          }
          var s = new Uint8Array(288);
          for (d = 0;32 > d;++d) {
            s[d] = 5;
          }
          n = e(s.subarray(0, 32));
          p = new Uint16Array(29);
          y = new Uint8Array(29);
          f = d = 0;
          for (x = 3;29 > d;++d) {
            p[d] = x - (28 == d ? 1 : 0), x += 1 << (y[d] = ~~((f += 4 < d ? 1 : 0) / 4 % 6));
          }
          for (d = 0;288 > d;++d) {
            s[d] = 144 > d || 279 < d ? 8 : 256 > d ? 9 : 7;
          }
          v = e(s);
          l = !0;
        }
      }
      __extends(f, h);
      Object.defineProperty(f.prototype, "error", {get:function() {
        return this._error;
      }, enumerable:!0, configurable:!0});
      f.prototype.push = function(a) {
        if (!this._buffer || this._buffer.length < this._bufferSize + a.length) {
          var f = new Uint8Array(this._bufferSize + a.length);
          this._buffer && f.set(this._buffer);
          this._buffer = f;
        }
        this._buffer.set(a, this._bufferSize);
        this._bufferSize += a.length;
        this._bufferPosition = 0;
        a = !1;
        do {
          f = this._windowPosition;
          if (0 === this._state && (a = this._decodeInitState())) {
            break;
          }
          switch(this._state) {
            case 1:
              a = this._decodeBlock0();
              break;
            case 3:
              if (a = this._decodeBlock2Pre()) {
                break;
              }
            ;
            case 2:
            ;
            case 4:
              a = this._decodeBlock();
              break;
            case 6:
            ;
            case 5:
              this._bufferPosition = this._bufferSize;
              break;
            case 7:
              var b = this._processZLibHeader(this._buffer, this._bufferPosition, this._bufferSize);
              0 < b ? (this._bufferPosition += b, this._state = 0) : 0 === b ? a = !0 : this._state = 6;
          }
          if (0 < this._windowPosition - f) {
            this.onData(this._window.subarray(f, this._windowPosition));
          }
          65536 <= this._windowPosition && ("copyWithin" in this._buffer ? this._window.copyWithin(0, this._windowPosition - 32768, this._windowPosition) : this._window.set(this._window.subarray(this._windowPosition - 32768, this._windowPosition)), this._windowPosition = 32768);
        } while (!a && this._bufferPosition < this._bufferSize);
        this._bufferPosition < this._bufferSize ? ("copyWithin" in this._buffer ? this._buffer.copyWithin(0, this._bufferPosition, this._bufferSize) : this._buffer.set(this._buffer.subarray(this._bufferPosition, this._bufferSize)), this._bufferSize -= this._bufferPosition) : this._bufferSize = 0;
      };
      f.prototype._decodeInitState = function() {
        if (this._isFinalBlock) {
          return this._state = 5, !1;
        }
        var a = this._buffer, f = this._bufferSize, b = this._bitBuffer, h = this._bitLength, c = this._bufferPosition;
        if (3 > (f - c << 3) + h) {
          return!0;
        }
        3 > h && (b |= a[c++] << h, h += 8);
        var p = b & 7, b = b >> 3, h = h - 3;
        switch(p >> 1) {
          case 0:
            h = b = 0;
            if (4 > f - c) {
              return!0;
            }
            var r = a[c] | a[c + 1] << 8, a = a[c + 2] | a[c + 3] << 8, c = c + 4;
            if (65535 !== (r ^ a)) {
              this._error = "inflate: invalid block 0 length";
              a = 6;
              break;
            }
            0 === r ? a = 0 : (this._block0Read = r, a = 1);
            break;
          case 1:
            a = 2;
            this._literalTable = v;
            this._distanceTable = n;
            break;
          case 2:
            if (26 > (f - c << 3) + h) {
              return!0;
            }
            for (;14 > h;) {
              b |= a[c++] << h, h += 8;
            }
            r = (b >> 10 & 15) + 4;
            if ((f - c << 3) + h < 14 + 3 * r) {
              return!0;
            }
            for (var f = {numLiteralCodes:(b & 31) + 257, numDistanceCodes:(b >> 5 & 31) + 1, codeLengthTable:void 0, bitLengths:void 0, codesRead:0, dupBits:0}, b = b >> 14, h = h - 14, t = new Uint8Array(19), l = 0;l < r;++l) {
              3 > h && (b |= a[c++] << h, h += 8), t[k[l]] = b & 7, b >>= 3, h -= 3;
            }
            for (;19 > l;l++) {
              t[k[l]] = 0;
            }
            f.bitLengths = new Uint8Array(f.numLiteralCodes + f.numDistanceCodes);
            f.codeLengthTable = e(t);
            this._block2State = f;
            a = 3;
            break;
          default:
            return this._error = "inflate: unsupported block type", !1;
        }
        this._isFinalBlock = !!(p & 1);
        this._state = a;
        this._bufferPosition = c;
        this._bitBuffer = b;
        this._bitLength = h;
        return!1;
      };
      f.prototype._decodeBlock0 = function() {
        var a = this._bufferPosition, f = this._windowPosition, b = this._block0Read, h = 65794 - f, n = this._bufferSize - a, c = n < b, p = Math.min(h, n, b);
        this._window.set(this._buffer.subarray(a, a + p), f);
        this._windowPosition = f + p;
        this._bufferPosition = a + p;
        this._block0Read = b - p;
        return b === p ? (this._state = 0, !1) : c && h < n;
      };
      f.prototype._readBits = function(a) {
        var f = this._bitBuffer, b = this._bitLength;
        if (a > b) {
          var h = this._bufferPosition, n = this._bufferSize;
          do {
            if (h >= n) {
              return this._bufferPosition = h, this._bitBuffer = f, this._bitLength = b, -1;
            }
            f |= this._buffer[h++] << b;
            b += 8;
          } while (a > b);
          this._bufferPosition = h;
        }
        this._bitBuffer = f >> a;
        this._bitLength = b - a;
        return f & (1 << a) - 1;
      };
      f.prototype._readCode = function(a) {
        var f = this._bitBuffer, b = this._bitLength, h = a.maxBits;
        if (h > b) {
          var n = this._bufferPosition, c = this._bufferSize;
          do {
            if (n >= c) {
              return this._bufferPosition = n, this._bitBuffer = f, this._bitLength = b, -1;
            }
            f |= this._buffer[n++] << b;
            b += 8;
          } while (h > b);
          this._bufferPosition = n;
        }
        a = a.codes[f & (1 << h) - 1];
        h = a >> 16;
        if (a & 32768) {
          return this._error = "inflate: invalid encoding", this._state = 6, -1;
        }
        this._bitBuffer = f >> h;
        this._bitLength = b - h;
        return a & 65535;
      };
      f.prototype._decodeBlock2Pre = function() {
        var a = this._block2State, f = a.numLiteralCodes + a.numDistanceCodes, b = a.bitLengths, h = a.codesRead, n = 0 < h ? b[h - 1] : 0, c = a.codeLengthTable, p;
        if (0 < a.dupBits) {
          p = this._readBits(a.dupBits);
          if (0 > p) {
            return!0;
          }
          for (;p--;) {
            b[h++] = n;
          }
          a.dupBits = 0;
        }
        for (;h < f;) {
          var r = this._readCode(c);
          if (0 > r) {
            return a.codesRead = h, !0;
          }
          if (16 > r) {
            b[h++] = n = r;
          } else {
            var k;
            switch(r) {
              case 16:
                k = 2;
                p = 3;
                r = n;
                break;
              case 17:
                p = k = 3;
                r = 0;
                break;
              case 18:
                k = 7, p = 11, r = 0;
            }
            for (;p--;) {
              b[h++] = r;
            }
            p = this._readBits(k);
            if (0 > p) {
              return a.codesRead = h, a.dupBits = k, !0;
            }
            for (;p--;) {
              b[h++] = r;
            }
            n = r;
          }
        }
        this._literalTable = e(b.subarray(0, a.numLiteralCodes));
        this._distanceTable = e(b.subarray(a.numLiteralCodes));
        this._state = 4;
        this._block2State = null;
        return!1;
      };
      f.prototype._decodeBlock = function() {
        var d = this._literalTable, f = this._distanceTable, h = this._window, n = this._windowPosition, c = this._copyState, r, k, t, l;
        if (0 !== c.state) {
          switch(c.state) {
            case 1:
              if (0 > (r = this._readBits(c.lenBits))) {
                return!0;
              }
              c.len += r;
              c.state = 2;
            case 2:
              if (0 > (r = this._readCode(f))) {
                return!0;
              }
              c.distBits = a[r];
              c.dist = b[r];
              c.state = 3;
            case 3:
              r = 0;
              if (0 < c.distBits && 0 > (r = this._readBits(c.distBits))) {
                return!0;
              }
              l = c.dist + r;
              k = c.len;
              for (r = n - l;k--;) {
                h[n++] = h[r++];
              }
              c.state = 0;
              if (65536 <= n) {
                return this._windowPosition = n, !1;
              }
              break;
          }
        }
        do {
          r = this._readCode(d);
          if (0 > r) {
            return this._windowPosition = n, !0;
          }
          if (256 > r) {
            h[n++] = r;
          } else {
            if (256 < r) {
              this._windowPosition = n;
              r -= 257;
              t = y[r];
              k = p[r];
              r = 0 === t ? 0 : this._readBits(t);
              if (0 > r) {
                return c.state = 1, c.len = k, c.lenBits = t, !0;
              }
              k += r;
              r = this._readCode(f);
              if (0 > r) {
                return c.state = 2, c.len = k, !0;
              }
              t = a[r];
              l = b[r];
              r = 0 === t ? 0 : this._readBits(t);
              if (0 > r) {
                return c.state = 3, c.len = k, c.dist = l, c.distBits = t, !0;
              }
              l += r;
              for (r = n - l;k--;) {
                h[n++] = h[r++];
              }
            } else {
              this._state = 0;
              break;
            }
          }
        } while (65536 > n);
        this._windowPosition = n;
        return!1;
      };
      return f;
    }(c), k, b, a, n, p, y, v, l = !1, t = function(a) {
      function f(d) {
        a.call(this, d);
        this._verifyHeader = d;
        this._specialInflate = new SpecialInflate;
        this._specialInflate.onData = function(a) {
          this.onData(a);
        }.bind(this);
      }
      __extends(f, a);
      f.prototype.push = function(a) {
        if (this._verifyHeader) {
          var f;
          this._buffer ? (f = new Uint8Array(this._buffer.length + a.length), f.set(this._buffer), f.set(a, this._buffer.length), this._buffer = null) : f = new Uint8Array(a);
          var b = this._processZLibHeader(f, 0, f.length);
          if (0 === b) {
            this._buffer = f;
            return;
          }
          this._verifyHeader = !0;
          0 < b && (a = f.subarray(b));
        }
        this._error || this._specialInflate.push(a);
      };
      f.prototype.close = function() {
        this._specialInflate && (this._specialInflate.close(), this._specialInflate = null);
      };
      return f;
    }(c), r;
    (function(a) {
      a[a.WRITE = 0] = "WRITE";
      a[a.DONE = 1] = "DONE";
      a[a.ZLIB_HEADER = 2] = "ZLIB_HEADER";
    })(r || (r = {}));
    var u = function() {
      function a() {
        this.a = 1;
        this.b = 0;
      }
      a.prototype.update = function(a, d, q) {
        for (var b = this.a, h = this.b;d < q;++d) {
          b = (b + (a[d] & 255)) % 65521, h = (h + b) % 65521;
        }
        this.a = b;
        this.b = h;
      };
      a.prototype.getChecksum = function() {
        return this.b << 16 | this.a;
      };
      return a;
    }();
    m.Adler32 = u;
    r = function() {
      function a(f) {
        this._state = (this._writeZlibHeader = f) ? 2 : 0;
        this._adler32 = f ? new u : null;
      }
      a.prototype.push = function(a) {
        2 === this._state && (this.onData(new Uint8Array([120, 156])), this._state = 0);
        for (var d = a.length, q = new Uint8Array(d + 5 * Math.ceil(d / 65535)), b = 0, h = 0;65535 < d;) {
          q.set(new Uint8Array([0, 255, 255, 0, 0]), b), b += 5, q.set(a.subarray(h, h + 65535), b), h += 65535, b += 65535, d -= 65535;
        }
        q.set(new Uint8Array([0, d & 255, d >> 8 & 255, ~d & 255, ~d >> 8 & 255]), b);
        q.set(a.subarray(h, d), b + 5);
        this.onData(q);
        this._adler32 && this._adler32.update(a, 0, d);
      };
      a.prototype.finish = function() {
        this._state = 1;
        this.onData(new Uint8Array([1, 0, 0, 255, 255]));
        if (this._adler32) {
          var a = this._adler32.getChecksum();
          this.onData(new Uint8Array([a & 255, a >> 8 & 255, a >> 16 & 255, a >>> 24 & 255]));
        }
      };
      return a;
    }();
    m.Deflate = r;
  })(g.ArrayUtilities || (g.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    function e(b, n) {
      b !== a(b, 0, n) && throwError("RangeError", Errors.ParamRangeError);
    }
    function c(a) {
      return "string" === typeof a ? a : void 0 == a ? null : a + "";
    }
    var w = g.Debug.notImplemented, k = g.StringUtilities.utf8decode, b = g.StringUtilities.utf8encode, a = g.NumberUtilities.clamp, n = function() {
      return function(a, b, n) {
        this.buffer = a;
        this.length = b;
        this.littleEndian = n;
      };
    }();
    m.PlainObjectDataBuffer = n;
    for (var p = new Uint32Array(33), y = 1, v = 0;32 >= y;y++) {
      p[y] = v = v << 1 | 1;
    }
    var l;
    (function(a) {
      a[a.U8 = 1] = "U8";
      a[a.I32 = 2] = "I32";
      a[a.F32 = 4] = "F32";
    })(l || (l = {}));
    y = function() {
      function l(a) {
        void 0 === a && (a = l.INITIAL_SIZE);
        this._buffer || (this._buffer = new ArrayBuffer(a), this._position = this._length = 0, this._resetViews(), this._littleEndian = l._nativeLittleEndian, this._bitLength = this._bitBuffer = 0);
      }
      l.FromArrayBuffer = function(a, b) {
        void 0 === b && (b = -1);
        var h = Object.create(l.prototype);
        h._buffer = a;
        h._length = -1 === b ? a.byteLength : b;
        h._position = 0;
        h._resetViews();
        h._littleEndian = l._nativeLittleEndian;
        h._bitBuffer = 0;
        h._bitLength = 0;
        return h;
      };
      l.FromPlainObject = function(a) {
        var b = l.FromArrayBuffer(a.buffer, a.length);
        b._littleEndian = a.littleEndian;
        return b;
      };
      l.prototype.toPlainObject = function() {
        return new n(this._buffer, this._length, this._littleEndian);
      };
      l.prototype._resetViews = function() {
        this._u8 = new Uint8Array(this._buffer);
        this._f32 = this._i32 = null;
      };
      l.prototype._requestViews = function(a) {
        0 === (this._buffer.byteLength & 3) && (null === this._i32 && a & 2 && (this._i32 = new Int32Array(this._buffer)), null === this._f32 && a & 4 && (this._f32 = new Float32Array(this._buffer)));
      };
      l.prototype.getBytes = function() {
        return new Uint8Array(this._buffer, 0, this._length);
      };
      l.prototype._ensureCapacity = function(a) {
        var b = this._buffer;
        if (b.byteLength < a) {
          for (var h = Math.max(b.byteLength, 1);h < a;) {
            h *= 2;
          }
          a = l._arrayBufferPool.acquire(h);
          h = this._u8;
          this._buffer = a;
          this._resetViews();
          this._u8.set(h);
          l._arrayBufferPool.release(b);
        }
      };
      l.prototype.clear = function() {
        this._position = this._length = 0;
      };
      l.prototype.readBoolean = function() {
        return 0 !== this.readUnsignedByte();
      };
      l.prototype.readByte = function() {
        return this.readUnsignedByte() << 24 >> 24;
      };
      l.prototype.readUnsignedByte = function() {
        this._position + 1 > this._length && throwError("EOFError", Errors.EOFError);
        return this._u8[this._position++];
      };
      l.prototype.readBytes = function(a, b) {
        var h = 0;
        void 0 === h && (h = 0);
        void 0 === b && (b = 0);
        var f = this._position;
        h || (h = 0);
        b || (b = this._length - f);
        f + b > this._length && throwError("EOFError", Errors.EOFError);
        a.length < h + b && (a._ensureCapacity(h + b), a.length = h + b);
        a._u8.set(new Uint8Array(this._buffer, f, b), h);
        this._position += b;
      };
      l.prototype.readShort = function() {
        return this.readUnsignedShort() << 16 >> 16;
      };
      l.prototype.readUnsignedShort = function() {
        var a = this._u8, b = this._position;
        b + 2 > this._length && throwError("EOFError", Errors.EOFError);
        var h = a[b + 0], a = a[b + 1];
        this._position = b + 2;
        return this._littleEndian ? a << 8 | h : h << 8 | a;
      };
      l.prototype.readInt = function() {
        var a = this._u8, b = this._position;
        b + 4 > this._length && throwError("EOFError", Errors.EOFError);
        var h = a[b + 0], f = a[b + 1], d = a[b + 2], a = a[b + 3];
        this._position = b + 4;
        return this._littleEndian ? a << 24 | d << 16 | f << 8 | h : h << 24 | f << 16 | d << 8 | a;
      };
      l.prototype.readUnsignedInt = function() {
        return this.readInt() >>> 0;
      };
      l.prototype.readFloat = function() {
        var a = this._position;
        a + 4 > this._length && throwError("EOFError", Errors.EOFError);
        this._position = a + 4;
        this._requestViews(4);
        if (this._littleEndian && 0 === (a & 3) && this._f32) {
          return this._f32[a >> 2];
        }
        var b = this._u8, h = g.IntegerUtilities.u8;
        this._littleEndian ? (h[0] = b[a + 0], h[1] = b[a + 1], h[2] = b[a + 2], h[3] = b[a + 3]) : (h[3] = b[a + 0], h[2] = b[a + 1], h[1] = b[a + 2], h[0] = b[a + 3]);
        return g.IntegerUtilities.f32[0];
      };
      l.prototype.readDouble = function() {
        var a = this._u8, b = this._position;
        b + 8 > this._length && throwError("EOFError", Errors.EOFError);
        var h = g.IntegerUtilities.u8;
        this._littleEndian ? (h[0] = a[b + 0], h[1] = a[b + 1], h[2] = a[b + 2], h[3] = a[b + 3], h[4] = a[b + 4], h[5] = a[b + 5], h[6] = a[b + 6], h[7] = a[b + 7]) : (h[0] = a[b + 7], h[1] = a[b + 6], h[2] = a[b + 5], h[3] = a[b + 4], h[4] = a[b + 3], h[5] = a[b + 2], h[6] = a[b + 1], h[7] = a[b + 0]);
        this._position = b + 8;
        return g.IntegerUtilities.f64[0];
      };
      l.prototype.writeBoolean = function(a) {
        this.writeByte(a ? 1 : 0);
      };
      l.prototype.writeByte = function(a) {
        var b = this._position + 1;
        this._ensureCapacity(b);
        this._u8[this._position++] = a;
        b > this._length && (this._length = b);
      };
      l.prototype.writeUnsignedByte = function(a) {
        var b = this._position + 1;
        this._ensureCapacity(b);
        this._u8[this._position++] = a;
        b > this._length && (this._length = b);
      };
      l.prototype.writeRawBytes = function(a) {
        var b = this._position + a.length;
        this._ensureCapacity(b);
        this._u8.set(a, this._position);
        this._position = b;
        b > this._length && (this._length = b);
      };
      l.prototype.writeBytes = function(a, b, h) {
        void 0 === b && (b = 0);
        void 0 === h && (h = 0);
        g.isNullOrUndefined(a) && throwError("TypeError", Errors.NullPointerError, "bytes");
        2 > arguments.length && (b = 0);
        3 > arguments.length && (h = 0);
        e(b, a.length);
        e(b + h, a.length);
        0 === h && (h = a.length - b);
        this.writeRawBytes(new Int8Array(a._buffer, b, h));
      };
      l.prototype.writeShort = function(a) {
        this.writeUnsignedShort(a);
      };
      l.prototype.writeUnsignedShort = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 2);
        var h = this._u8;
        this._littleEndian ? (h[b + 0] = a, h[b + 1] = a >> 8) : (h[b + 0] = a >> 8, h[b + 1] = a);
        this._position = b += 2;
        b > this._length && (this._length = b);
      };
      l.prototype.writeInt = function(a) {
        this.writeUnsignedInt(a);
      };
      l.prototype.write2Ints = function(a, b) {
        this.write2UnsignedInts(a, b);
      };
      l.prototype.write4Ints = function(a, b, h, f) {
        this.write4UnsignedInts(a, b, h, f);
      };
      l.prototype.writeUnsignedInt = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 4);
        this._requestViews(2);
        if (this._littleEndian === l._nativeLittleEndian && 0 === (b & 3) && this._i32) {
          this._i32[b >> 2] = a;
        } else {
          var h = this._u8;
          this._littleEndian ? (h[b + 0] = a, h[b + 1] = a >> 8, h[b + 2] = a >> 16, h[b + 3] = a >> 24) : (h[b + 0] = a >> 24, h[b + 1] = a >> 16, h[b + 2] = a >> 8, h[b + 3] = a);
        }
        this._position = b += 4;
        b > this._length && (this._length = b);
      };
      l.prototype.write2UnsignedInts = function(a, b) {
        var h = this._position;
        this._ensureCapacity(h + 8);
        this._requestViews(2);
        this._littleEndian === l._nativeLittleEndian && 0 === (h & 3) && this._i32 ? (this._i32[(h >> 2) + 0] = a, this._i32[(h >> 2) + 1] = b, this._position = h += 8, h > this._length && (this._length = h)) : (this.writeUnsignedInt(a), this.writeUnsignedInt(b));
      };
      l.prototype.write4UnsignedInts = function(a, b, h, f) {
        var d = this._position;
        this._ensureCapacity(d + 16);
        this._requestViews(2);
        this._littleEndian === l._nativeLittleEndian && 0 === (d & 3) && this._i32 ? (this._i32[(d >> 2) + 0] = a, this._i32[(d >> 2) + 1] = b, this._i32[(d >> 2) + 2] = h, this._i32[(d >> 2) + 3] = f, this._position = d += 16, d > this._length && (this._length = d)) : (this.writeUnsignedInt(a), this.writeUnsignedInt(b), this.writeUnsignedInt(h), this.writeUnsignedInt(f));
      };
      l.prototype.writeFloat = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 4);
        this._requestViews(4);
        if (this._littleEndian === l._nativeLittleEndian && 0 === (b & 3) && this._f32) {
          this._f32[b >> 2] = a;
        } else {
          var h = this._u8;
          g.IntegerUtilities.f32[0] = a;
          a = g.IntegerUtilities.u8;
          this._littleEndian ? (h[b + 0] = a[0], h[b + 1] = a[1], h[b + 2] = a[2], h[b + 3] = a[3]) : (h[b + 0] = a[3], h[b + 1] = a[2], h[b + 2] = a[1], h[b + 3] = a[0]);
        }
        this._position = b += 4;
        b > this._length && (this._length = b);
      };
      l.prototype.write6Floats = function(a, b, h, f, d, q) {
        var n = this._position;
        this._ensureCapacity(n + 24);
        this._requestViews(4);
        this._littleEndian === l._nativeLittleEndian && 0 === (n & 3) && this._f32 ? (this._f32[(n >> 2) + 0] = a, this._f32[(n >> 2) + 1] = b, this._f32[(n >> 2) + 2] = h, this._f32[(n >> 2) + 3] = f, this._f32[(n >> 2) + 4] = d, this._f32[(n >> 2) + 5] = q, this._position = n += 24, n > this._length && (this._length = n)) : (this.writeFloat(a), this.writeFloat(b), this.writeFloat(h), this.writeFloat(f), this.writeFloat(d), this.writeFloat(q));
      };
      l.prototype.writeDouble = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 8);
        var h = this._u8;
        g.IntegerUtilities.f64[0] = a;
        a = g.IntegerUtilities.u8;
        this._littleEndian ? (h[b + 0] = a[0], h[b + 1] = a[1], h[b + 2] = a[2], h[b + 3] = a[3], h[b + 4] = a[4], h[b + 5] = a[5], h[b + 6] = a[6], h[b + 7] = a[7]) : (h[b + 0] = a[7], h[b + 1] = a[6], h[b + 2] = a[5], h[b + 3] = a[4], h[b + 4] = a[3], h[b + 5] = a[2], h[b + 6] = a[1], h[b + 7] = a[0]);
        this._position = b += 8;
        b > this._length && (this._length = b);
      };
      l.prototype.readRawBytes = function() {
        return new Int8Array(this._buffer, 0, this._length);
      };
      l.prototype.writeUTF = function(a) {
        a = c(a);
        a = k(a);
        this.writeShort(a.length);
        this.writeRawBytes(a);
      };
      l.prototype.writeUTFBytes = function(a) {
        a = c(a);
        a = k(a);
        this.writeRawBytes(a);
      };
      l.prototype.readUTF = function() {
        return this.readUTFBytes(this.readShort());
      };
      l.prototype.readUTFBytes = function(a) {
        a >>>= 0;
        var n = this._position;
        n + a > this._length && throwError("EOFError", Errors.EOFError);
        this._position += a;
        return b(new Int8Array(this._buffer, n, a));
      };
      Object.defineProperty(l.prototype, "length", {get:function() {
        return this._length;
      }, set:function(b) {
        b >>>= 0;
        b > this._buffer.byteLength && this._ensureCapacity(b);
        this._length = b;
        this._position = a(this._position, 0, this._length);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(l.prototype, "bytesAvailable", {get:function() {
        return this._length - this._position;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(l.prototype, "position", {get:function() {
        return this._position;
      }, set:function(a) {
        this._position = a >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(l.prototype, "buffer", {get:function() {
        return this._buffer;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(l.prototype, "bytes", {get:function() {
        return this._u8;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(l.prototype, "ints", {get:function() {
        this._requestViews(2);
        return this._i32;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(l.prototype, "objectEncoding", {get:function() {
        return this._objectEncoding;
      }, set:function(a) {
        this._objectEncoding = a >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(l.prototype, "endian", {get:function() {
        return this._littleEndian ? "littleEndian" : "bigEndian";
      }, set:function(a) {
        a = c(a);
        this._littleEndian = "auto" === a ? l._nativeLittleEndian : "littleEndian" === a;
      }, enumerable:!0, configurable:!0});
      l.prototype.toString = function() {
        return b(new Int8Array(this._buffer, 0, this._length));
      };
      l.prototype.toBlob = function(a) {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)], {type:a});
      };
      l.prototype.writeMultiByte = function(a, b) {
        w("packageInternal flash.utils.ObjectOutput::writeMultiByte");
      };
      l.prototype.readMultiByte = function(a, b) {
        w("packageInternal flash.utils.ObjectInput::readMultiByte");
      };
      l.prototype.getValue = function(a) {
        a |= 0;
        return a >= this._length ? void 0 : this._u8[a];
      };
      l.prototype.setValue = function(a, b) {
        a |= 0;
        var h = a + 1;
        this._ensureCapacity(h);
        this._u8[a] = b;
        h > this._length && (this._length = h);
      };
      l.prototype.readFixed = function() {
        return this.readInt() / 65536;
      };
      l.prototype.readFixed8 = function() {
        return this.readShort() / 256;
      };
      l.prototype.readFloat16 = function() {
        var a = this.readUnsignedShort(), b = a >> 15 ? -1 : 1, h = (a & 31744) >> 10, a = a & 1023;
        return h ? 31 === h ? a ? NaN : Infinity * b : b * Math.pow(2, h - 15) * (1 + a / 1024) : a / 1024 * Math.pow(2, -14) * b;
      };
      l.prototype.readEncodedU32 = function() {
        var a = this.readUnsignedByte();
        if (!(a & 128)) {
          return a;
        }
        a = a & 127 | this.readUnsignedByte() << 7;
        if (!(a & 16384)) {
          return a;
        }
        a = a & 16383 | this.readUnsignedByte() << 14;
        if (!(a & 2097152)) {
          return a;
        }
        a = a & 2097151 | this.readUnsignedByte() << 21;
        return a & 268435456 ? a & 268435455 | this.readUnsignedByte() << 28 : a;
      };
      l.prototype.readBits = function(a) {
        return this.readUnsignedBits(a) << 32 - a >> 32 - a;
      };
      l.prototype.readUnsignedBits = function(a) {
        for (var b = this._bitBuffer, h = this._bitLength;a > h;) {
          b = b << 8 | this.readUnsignedByte(), h += 8;
        }
        h -= a;
        a = b >>> h & p[a];
        this._bitBuffer = b;
        this._bitLength = h;
        return a;
      };
      l.prototype.readFixedBits = function(a) {
        return this.readBits(a) / 65536;
      };
      l.prototype.readString = function(a) {
        var n = this._position;
        if (a) {
          n + a > this._length && throwError("EOFError", Errors.EOFError), this._position += a;
        } else {
          a = 0;
          for (var h = n;h < this._length && this._u8[h];h++) {
            a++;
          }
          this._position += a + 1;
        }
        return b(new Int8Array(this._buffer, n, a));
      };
      l.prototype.align = function() {
        this._bitLength = this._bitBuffer = 0;
      };
      l.prototype._compress = function(a) {
        a = c(a);
        switch(a) {
          case "zlib":
            a = new m.Deflate(!0);
            break;
          case "deflate":
            a = new m.Deflate(!1);
            break;
          default:
            return;
        }
        var b = new l;
        a.onData = b.writeRawBytes.bind(b);
        a.push(this._u8.subarray(0, this._length));
        a.finish();
        this._ensureCapacity(b._u8.length);
        this._u8.set(b._u8);
        this.length = b.length;
        this._position = 0;
      };
      l.prototype._uncompress = function(a) {
        a = c(a);
        switch(a) {
          case "zlib":
            a = m.Inflate.create(!0);
            break;
          case "deflate":
            a = m.Inflate.create(!1);
            break;
          default:
            return;
        }
        var b = new l;
        a.onData = b.writeRawBytes.bind(b);
        a.push(this._u8.subarray(0, this._length));
        a.error && throwError("IOError", Errors.CompressedDataError);
        a.close();
        this._ensureCapacity(b._u8.length);
        this._u8.set(b._u8);
        this.length = b.length;
        this._position = 0;
      };
      l._nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
      l.INITIAL_SIZE = 128;
      l._arrayBufferPool = new g.ArrayBufferPool;
      return l;
    }();
    m.DataBuffer = y;
  })(g.ArrayUtilities || (g.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  var m = g.ArrayUtilities.DataBuffer, e = g.ArrayUtilities.ensureTypedArrayCapacity, c = g.Debug.assert;
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
  })(g.PathCommand || (g.PathCommand = {}));
  (function(b) {
    b[b.Linear = 16] = "Linear";
    b[b.Radial = 18] = "Radial";
  })(g.GradientType || (g.GradientType = {}));
  (function(b) {
    b[b.Pad = 0] = "Pad";
    b[b.Reflect = 1] = "Reflect";
    b[b.Repeat = 2] = "Repeat";
  })(g.GradientSpreadMethod || (g.GradientSpreadMethod = {}));
  (function(b) {
    b[b.RGB = 0] = "RGB";
    b[b.LinearRGB = 1] = "LinearRGB";
  })(g.GradientInterpolationMethod || (g.GradientInterpolationMethod = {}));
  (function(b) {
    b[b.None = 0] = "None";
    b[b.Normal = 1] = "Normal";
    b[b.Vertical = 2] = "Vertical";
    b[b.Horizontal = 3] = "Horizontal";
  })(g.LineScaleMode || (g.LineScaleMode = {}));
  var w = function() {
    return function(b, a, n, c, k, e, l, g, r, m, h) {
      this.commands = b;
      this.commandsPosition = a;
      this.coordinates = n;
      this.morphCoordinates = c;
      this.coordinatesPosition = k;
      this.styles = e;
      this.stylesLength = l;
      this.morphStyles = g;
      this.morphStylesLength = r;
      this.hasFills = m;
      this.hasLines = h;
    };
  }();
  g.PlainObjectShapeData = w;
  var k;
  (function(b) {
    b[b.Commands = 32] = "Commands";
    b[b.Coordinates = 128] = "Coordinates";
    b[b.Styles = 16] = "Styles";
  })(k || (k = {}));
  k = function() {
    function b(a) {
      void 0 === a && (a = !0);
      a && this.clear();
    }
    b.FromPlainObject = function(a) {
      var n = new b(!1);
      n.commands = a.commands;
      n.coordinates = a.coordinates;
      n.morphCoordinates = a.morphCoordinates;
      n.commandsPosition = a.commandsPosition;
      n.coordinatesPosition = a.coordinatesPosition;
      n.styles = m.FromArrayBuffer(a.styles, a.stylesLength);
      n.styles.endian = "auto";
      a.morphStyles && (n.morphStyles = m.FromArrayBuffer(a.morphStyles, a.morphStylesLength), n.morphStyles.endian = "auto");
      n.hasFills = a.hasFills;
      n.hasLines = a.hasLines;
      return n;
    };
    b.prototype.moveTo = function(a, b) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 9;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = b;
    };
    b.prototype.lineTo = function(a, b) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 10;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = b;
    };
    b.prototype.curveTo = function(a, b, c, k) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = c;
      this.coordinates[this.coordinatesPosition++] = k;
    };
    b.prototype.cubicCurveTo = function(a, b, c, k, e, l) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = c;
      this.coordinates[this.coordinatesPosition++] = k;
      this.coordinates[this.coordinatesPosition++] = e;
      this.coordinates[this.coordinatesPosition++] = l;
    };
    b.prototype.beginFill = function(a) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 1;
      this.styles.writeUnsignedInt(a);
      this.hasFills = !0;
    };
    b.prototype.writeMorphFill = function(a) {
      this.morphStyles.writeUnsignedInt(a);
    };
    b.prototype.endFill = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 4;
    };
    b.prototype.endLine = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 8;
    };
    b.prototype.lineStyle = function(a, b, p, k, e, l, g) {
      c(a === (a | 0), 0 <= a && 5100 >= a);
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5;
      this.coordinates[this.coordinatesPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(b);
      a.writeBoolean(p);
      a.writeUnsignedByte(k);
      a.writeUnsignedByte(e);
      a.writeUnsignedByte(l);
      a.writeUnsignedByte(g);
      this.hasLines = !0;
    };
    b.prototype.writeMorphLineStyle = function(a, b) {
      this.morphCoordinates[this.coordinatesPosition - 1] = a;
      this.morphStyles.writeUnsignedInt(b);
    };
    b.prototype.beginBitmap = function(a, b, p, k, e) {
      c(3 === a || 7 === a);
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(b);
      this._writeStyleMatrix(p, !1);
      a.writeBoolean(k);
      a.writeBoolean(e);
      this.hasFills = !0;
    };
    b.prototype.writeMorphBitmap = function(a) {
      this._writeStyleMatrix(a, !0);
    };
    b.prototype.beginGradient = function(a, b, p, k, e, l, g, r) {
      c(2 === a || 6 === a);
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedByte(k);
      c(r === (r | 0));
      a.writeShort(r);
      this._writeStyleMatrix(e, !1);
      k = b.length;
      a.writeByte(k);
      for (e = 0;e < k;e++) {
        a.writeUnsignedByte(p[e]), a.writeUnsignedInt(b[e]);
      }
      a.writeUnsignedByte(l);
      a.writeUnsignedByte(g);
      this.hasFills = !0;
    };
    b.prototype.writeMorphGradient = function(a, b, c) {
      this._writeStyleMatrix(c, !0);
      c = this.morphStyles;
      for (var k = 0;k < a.length;k++) {
        c.writeUnsignedByte(b[k]), c.writeUnsignedInt(a[k]);
      }
    };
    b.prototype.writeCommandAndCoordinates = function(a, b, c) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = c;
    };
    b.prototype.writeCoordinates = function(a, b) {
      this.ensurePathCapacities(0, 2);
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = b;
    };
    b.prototype.writeMorphCoordinates = function(a, b) {
      this.morphCoordinates = e(this.morphCoordinates, this.coordinatesPosition);
      this.morphCoordinates[this.coordinatesPosition - 2] = a;
      this.morphCoordinates[this.coordinatesPosition - 1] = b;
    };
    b.prototype.clear = function() {
      this.commandsPosition = this.coordinatesPosition = 0;
      this.commands = new Uint8Array(32);
      this.coordinates = new Int32Array(128);
      this.styles = new m(16);
      this.styles.endian = "auto";
      this.hasFills = this.hasLines = !1;
    };
    b.prototype.isEmpty = function() {
      return 0 === this.commandsPosition;
    };
    b.prototype.clone = function() {
      var a = new b(!1);
      a.commands = new Uint8Array(this.commands);
      a.commandsPosition = this.commandsPosition;
      a.coordinates = new Int32Array(this.coordinates);
      a.coordinatesPosition = this.coordinatesPosition;
      a.styles = new m(this.styles.length);
      a.styles.writeRawBytes(this.styles.bytes);
      this.morphStyles && (a.morphStyles = new m(this.morphStyles.length), a.morphStyles.writeRawBytes(this.morphStyles.bytes));
      a.hasFills = this.hasFills;
      a.hasLines = this.hasLines;
      return a;
    };
    b.prototype.toPlainObject = function() {
      return new w(this.commands, this.commandsPosition, this.coordinates, this.morphCoordinates, this.coordinatesPosition, this.styles.buffer, this.styles.length, this.morphStyles && this.morphStyles.buffer, this.morphStyles ? this.morphStyles.length : 0, this.hasFills, this.hasLines);
    };
    Object.defineProperty(b.prototype, "buffers", {get:function() {
      var a = [this.commands.buffer, this.coordinates.buffer, this.styles.buffer];
      this.morphCoordinates && a.push(this.morphCoordinates.buffer);
      this.morphStyles && a.push(this.morphStyles.buffer);
      return a;
    }, enumerable:!0, configurable:!0});
    b.prototype._writeStyleMatrix = function(a, b) {
      (b ? this.morphStyles : this.styles).write6Floats(a.a, a.b, a.c, a.d, a.tx, a.ty);
    };
    b.prototype.ensurePathCapacities = function(a, b) {
      this.commands = e(this.commands, this.commandsPosition + a);
      this.coordinates = e(this.coordinates, this.coordinatesPosition + b);
    };
    return b;
  }();
  g.ShapeData = k;
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
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
      })(e.SwfTag || (e.SwfTag = {}));
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
        c[c.CODE_DEFINE_VIDEO_STREAM = 60] = "CODE_DEFINE_VIDEO_STREAM";
        c[c.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        c[c.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        c[c.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        c[c.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        c[c.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        c[c.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(e.DefinitionTags || (e.DefinitionTags = {}));
      (function(c) {
        c[c.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        c[c.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        c[c.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        c[c.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
      })(e.ImageDefinitionTags || (e.ImageDefinitionTags = {}));
      (function(c) {
        c[c.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        c[c.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        c[c.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        c[c.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(e.FontDefinitionTags || (e.FontDefinitionTags = {}));
      (function(c) {
        c[c.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        c[c.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        c[c.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        c[c.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        c[c.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        c[c.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        c[c.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
        c[c.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
      })(e.ControlTags || (e.ControlTags = {}));
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
      })(e.PlaceObjectFlags || (e.PlaceObjectFlags = {}));
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
      })(e.AVM1ClipEvents || (e.AVM1ClipEvents = {}));
    })(g.Parser || (g.Parser = {}));
  })(g.SWF || (g.SWF = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  var m = g.Debug.unexpected, e = function() {
    function c(c, k, b, a) {
      this.url = c;
      this.method = k;
      this.mimeType = b;
      this.data = a;
    }
    c.prototype.readAll = function(c, k) {
      var b = this.url, a = new XMLHttpRequest({mozSystem:!0});
      a.open(this.method || "GET", this.url, !0);
      a.responseType = "arraybuffer";
      c && (a.onprogress = function(b) {
        c(a.response, b.loaded, b.total);
      });
      a.onreadystatechange = function(n) {
        4 === a.readyState && (200 !== a.status && 0 !== a.status || null === a.response ? (m("Path: " + b + " not found."), k(null, a.statusText)) : k(a.response));
      };
      this.mimeType && a.setRequestHeader("Content-Type", this.mimeType);
      a.send(this.data || null);
    };
    c.prototype.readChunked = function(c, k, b, a, n, p) {
      if (0 >= c) {
        this.readAsync(k, b, a, n, p);
      } else {
        var e = 0, v = new Uint8Array(c), l = 0, g;
        this.readAsync(function(a, b) {
          g = b.total;
          for (var h = a.length, f = 0;e + h >= c;) {
            var d = c - e;
            v.set(a.subarray(f, f + d), e);
            f += d;
            h -= d;
            l += c;
            k(v, {loaded:l, total:g});
            e = 0;
          }
          v.set(a.subarray(f), e);
          e += h;
        }, b, a, function() {
          0 < e && (l += e, k(v.subarray(0, e), {loaded:l, total:g}), e = 0);
          n && n();
        }, p);
      }
    };
    c.prototype.readAsync = function(c, k, b, a, n) {
      var p = new XMLHttpRequest({mozSystem:!0}), e = this.url, g = 0, l = 0;
      p.open(this.method || "GET", e, !0);
      p.responseType = "moz-chunked-arraybuffer";
      var t = "moz-chunked-arraybuffer" !== p.responseType;
      t && (p.responseType = "arraybuffer");
      p.onprogress = function(a) {
        t || (g = a.loaded, l = a.total, c(new Uint8Array(p.response), {loaded:g, total:l}));
      };
      p.onreadystatechange = function(b) {
        2 === p.readyState && n && n(e, p.status, p.getAllResponseHeaders());
        4 === p.readyState && (200 !== p.status && 0 !== p.status || null === p.response && (0 === l || g !== l) ? k(p.statusText) : (t && (b = p.response, c(new Uint8Array(b), {loaded:0, total:b.byteLength})), a && a()));
      };
      this.mimeType && p.setRequestHeader("Content-Type", this.mimeType);
      p.send(this.data || null);
      b && b();
    };
    return c;
  }();
  g.BinaryFileReader = e;
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
      e[e.Objects = 0] = "Objects";
      e[e.References = 1] = "References";
    })(g.RemotingPhase || (g.RemotingPhase = {}));
    (function(e) {
      e[e.HasMatrix = 1] = "HasMatrix";
      e[e.HasBounds = 2] = "HasBounds";
      e[e.HasChildren = 4] = "HasChildren";
      e[e.HasColorTransform = 8] = "HasColorTransform";
      e[e.HasClipRect = 16] = "HasClipRect";
      e[e.HasMiscellaneousProperties = 32] = "HasMiscellaneousProperties";
      e[e.HasMask = 64] = "HasMask";
      e[e.HasClip = 128] = "HasClip";
    })(g.MessageBits || (g.MessageBits = {}));
    (function(e) {
      e[e.None = 0] = "None";
      e[e.Asset = 134217728] = "Asset";
    })(g.IDMask || (g.IDMask = {}));
    (function(e) {
      e[e.EOF = 0] = "EOF";
      e[e.UpdateFrame = 100] = "UpdateFrame";
      e[e.UpdateGraphics = 101] = "UpdateGraphics";
      e[e.UpdateBitmapData = 102] = "UpdateBitmapData";
      e[e.UpdateTextContent = 103] = "UpdateTextContent";
      e[e.UpdateStage = 104] = "UpdateStage";
      e[e.UpdateNetStream = 105] = "UpdateNetStream";
      e[e.RequestBitmapData = 106] = "RequestBitmapData";
      e[e.DrawToBitmap = 200] = "DrawToBitmap";
      e[e.MouseEvent = 300] = "MouseEvent";
      e[e.KeyboardEvent = 301] = "KeyboardEvent";
      e[e.FocusEvent = 302] = "FocusEvent";
    })(g.MessageTag || (g.MessageTag = {}));
    (function(e) {
      e[e.Blur = 0] = "Blur";
      e[e.DropShadow = 1] = "DropShadow";
    })(g.FilterType || (g.FilterType = {}));
    (function(e) {
      e[e.Identity = 0] = "Identity";
      e[e.AlphaMultiplierOnly = 1] = "AlphaMultiplierOnly";
      e[e.All = 2] = "All";
    })(g.ColorTransformEncoding || (g.ColorTransformEncoding = {}));
    (function(e) {
      e[e.Initialized = 0] = "Initialized";
      e[e.PlayStart = 1] = "PlayStart";
      e[e.PlayStop = 2] = "PlayStop";
      e[e.BufferFull = 3] = "BufferFull";
      e[e.Progress = 4] = "Progress";
      e[e.BufferEmpty = 5] = "BufferEmpty";
      e[e.Error = 6] = "Error";
      e[e.Metadata = 7] = "Metadata";
      e[e.Seeking = 8] = "Seeking";
    })(g.VideoPlaybackEvent || (g.VideoPlaybackEvent = {}));
    (function(e) {
      e[e.Init = 1] = "Init";
      e[e.Pause = 2] = "Pause";
      e[e.Seek = 3] = "Seek";
      e[e.GetTime = 4] = "GetTime";
      e[e.GetBufferLength = 5] = "GetBufferLength";
      e[e.SetSoundLevels = 6] = "SetSoundLevels";
      e[e.GetBytesLoaded = 7] = "GetBytesLoaded";
      e[e.GetBytesTotal = 8] = "GetBytesTotal";
    })(g.VideoControlEvent || (g.VideoControlEvent = {}));
    (function(e) {
      e[e.ShowAll = 0] = "ShowAll";
      e[e.ExactFit = 1] = "ExactFit";
      e[e.NoBorder = 2] = "NoBorder";
      e[e.NoScale = 4] = "NoScale";
    })(g.StageScaleMode || (g.StageScaleMode = {}));
    (function(e) {
      e[e.None = 0] = "None";
      e[e.Top = 1] = "Top";
      e[e.Bottom = 2] = "Bottom";
      e[e.Left = 4] = "Left";
      e[e.Right = 8] = "Right";
      e[e.TopLeft = e.Top | e.Left] = "TopLeft";
      e[e.BottomLeft = e.Bottom | e.Left] = "BottomLeft";
      e[e.BottomRight = e.Bottom | e.Right] = "BottomRight";
      e[e.TopRight = e.Top | e.Right] = "TopRight";
    })(g.StageAlignFlags || (g.StageAlignFlags = {}));
    g.MouseEventNames = "click dblclick mousedown mousemove mouseup mouseover mouseout".split(" ");
    g.KeyboardEventNames = ["keydown", "keypress", "keyup"];
    (function(e) {
      e[e.CtrlKey = 1] = "CtrlKey";
      e[e.AltKey = 2] = "AltKey";
      e[e.ShiftKey = 4] = "ShiftKey";
    })(g.KeyboardEventFlags || (g.KeyboardEventFlags = {}));
    (function(e) {
      e[e.DocumentHidden = 0] = "DocumentHidden";
      e[e.DocumentVisible = 1] = "DocumentVisible";
      e[e.WindowBlur = 2] = "WindowBlur";
      e[e.WindowFocus = 3] = "WindowFocus";
    })(g.FocusEventType || (g.FocusEventType = {}));
  })(g.Remoting || (g.Remoting = {}));
})(Shumway || (Shumway = {}));
var throwError, Errors;
(function(g) {
  (function(g) {
    (function(e) {
      var c = function() {
        function c() {
        }
        c.toRGBA = function(b, a, n, c) {
          void 0 === c && (c = 1);
          return "rgba(" + b + "," + a + "," + n + "," + c + ")";
        };
        return c;
      }();
      e.UI = c;
      var g = function() {
        function k() {
        }
        k.prototype.tabToolbar = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(37, 44, 51, b);
        };
        k.prototype.toolbars = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(52, 60, 69, b);
        };
        k.prototype.selectionBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(29, 79, 115, b);
        };
        k.prototype.selectionText = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(245, 247, 250, b);
        };
        k.prototype.splitters = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(0, 0, 0, b);
        };
        k.prototype.bodyBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(17, 19, 21, b);
        };
        k.prototype.sidebarBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(24, 29, 32, b);
        };
        k.prototype.attentionBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(161, 134, 80, b);
        };
        k.prototype.bodyText = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(143, 161, 178, b);
        };
        k.prototype.foregroundTextGrey = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(182, 186, 191, b);
        };
        k.prototype.contentTextHighContrast = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(169, 186, 203, b);
        };
        k.prototype.contentTextGrey = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(143, 161, 178, b);
        };
        k.prototype.contentTextDarkGrey = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(95, 115, 135, b);
        };
        k.prototype.blueHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(70, 175, 227, b);
        };
        k.prototype.purpleHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(107, 122, 187, b);
        };
        k.prototype.pinkHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(223, 128, 255, b);
        };
        k.prototype.redHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(235, 83, 104, b);
        };
        k.prototype.orangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(217, 102, 41, b);
        };
        k.prototype.lightOrangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(217, 155, 40, b);
        };
        k.prototype.greenHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(112, 191, 83, b);
        };
        k.prototype.blueGreyHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(94, 136, 176, b);
        };
        return k;
      }();
      e.UIThemeDark = g;
      g = function() {
        function k() {
        }
        k.prototype.tabToolbar = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(235, 236, 237, b);
        };
        k.prototype.toolbars = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(240, 241, 242, b);
        };
        k.prototype.selectionBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(76, 158, 217, b);
        };
        k.prototype.selectionText = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(245, 247, 250, b);
        };
        k.prototype.splitters = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(170, 170, 170, b);
        };
        k.prototype.bodyBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(252, 252, 252, b);
        };
        k.prototype.sidebarBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(247, 247, 247, b);
        };
        k.prototype.attentionBackground = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(161, 134, 80, b);
        };
        k.prototype.bodyText = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(24, 25, 26, b);
        };
        k.prototype.foregroundTextGrey = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(88, 89, 89, b);
        };
        k.prototype.contentTextHighContrast = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(41, 46, 51, b);
        };
        k.prototype.contentTextGrey = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(143, 161, 178, b);
        };
        k.prototype.contentTextDarkGrey = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(102, 115, 128, b);
        };
        k.prototype.blueHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(0, 136, 204, b);
        };
        k.prototype.purpleHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(91, 95, 255, b);
        };
        k.prototype.pinkHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(184, 46, 229, b);
        };
        k.prototype.redHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(237, 38, 85, b);
        };
        k.prototype.orangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(241, 60, 0, b);
        };
        k.prototype.lightOrangeHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(217, 126, 0, b);
        };
        k.prototype.greenHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(44, 187, 15, b);
        };
        k.prototype.blueGreyHighlight = function(b) {
          void 0 === b && (b = 1);
          return c.toRGBA(95, 136, 176, b);
        };
        return k;
      }();
      e.UIThemeLight = g;
    })(g.Theme || (g.Theme = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
      var c = function() {
        function c(k) {
          this._buffers = k || [];
          this._snapshots = [];
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
          for (var b = 0, a = this.snapshotCount;b < a;b++) {
            c(this._snapshots[b], b);
          }
        };
        c.prototype.createSnapshots = function() {
          var c = Number.MAX_VALUE, b = Number.MIN_VALUE, a = 0;
          for (this._snapshots = [];0 < this._buffers.length;) {
            var n = this._buffers.shift().createSnapshot();
            n && (c > n.startTime && (c = n.startTime), b < n.endTime && (b = n.endTime), a < n.maxDepth && (a = n.maxDepth), this._snapshots.push(n));
          }
          this._startTime = c;
          this._endTime = b;
          this._windowStart = c;
          this._windowEnd = b;
          this._maxDepth = a;
        };
        c.prototype.setWindow = function(c, b) {
          if (c > b) {
            var a = c;
            c = b;
            b = a;
          }
          a = Math.min(b - c, this.totalTime);
          c < this._startTime ? (c = this._startTime, b = this._startTime + a) : b > this._endTime && (c = this._endTime - a, b = this._endTime);
          this._windowStart = c;
          this._windowEnd = b;
        };
        c.prototype.moveWindowTo = function(c) {
          this.setWindow(c - this.windowLength / 2, c + this.windowLength / 2);
        };
        return c;
      }();
      e.Profile = c;
    })(g.Profiler || (g.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(g, m) {
  function e() {
    this.constructor = g;
  }
  for (var c in m) {
    m.hasOwnProperty(c) && (g[c] = m[c]);
  }
  e.prototype = m.prototype;
  g.prototype = new e;
};
(function(g) {
  (function(g) {
    (function(e) {
      var c = function() {
        return function(c) {
          this.kind = c;
          this.totalTime = this.selfTime = this.count = 0;
        };
      }();
      e.TimelineFrameStatistics = c;
      var g = function() {
        function k(b, a, c, p, k, e) {
          this.parent = b;
          this.kind = a;
          this.startData = c;
          this.endData = p;
          this.startTime = k;
          this.endTime = e;
          this.maxDepth = 0;
        }
        Object.defineProperty(k.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(k.prototype, "selfTime", {get:function() {
          var b = this.totalTime;
          if (this.children) {
            for (var a = 0, c = this.children.length;a < c;a++) {
              var p = this.children[a], b = b - (p.endTime - p.startTime)
            }
          }
          return b;
        }, enumerable:!0, configurable:!0});
        k.prototype.getChildIndex = function(b) {
          for (var a = this.children, c = 0;c < a.length;c++) {
            if (a[c].endTime > b) {
              return c;
            }
          }
          return 0;
        };
        k.prototype.getChildRange = function(b, a) {
          if (this.children && b <= this.endTime && a >= this.startTime && a >= b) {
            var c = this._getNearestChild(b), p = this._getNearestChildReverse(a);
            if (c <= p) {
              return b = this.children[c].startTime, a = this.children[p].endTime, {startIndex:c, endIndex:p, startTime:b, endTime:a, totalTime:a - b};
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
            for (var c, p = 0, k = a.length - 1;k > p;) {
              c = (p + k) / 2 | 0;
              var e = a[c];
              if (b >= e.startTime && b <= e.endTime) {
                return c;
              }
              b > e.endTime ? p = c + 1 : k = c;
            }
            return Math.ceil((p + k) / 2);
          }
          return 0;
        };
        k.prototype._getNearestChildReverse = function(b) {
          var a = this.children;
          if (a && a.length) {
            var c = a.length - 1;
            if (b >= a[c].startTime) {
              return c;
            }
            for (var p, k = 0;c > k;) {
              p = Math.ceil((k + c) / 2);
              var e = a[p];
              if (b >= e.startTime && b <= e.endTime) {
                return p;
              }
              b > e.endTime ? k = p : c = p - 1;
            }
            return(k + c) / 2 | 0;
          }
          return 0;
        };
        k.prototype.query = function(b) {
          if (b < this.startTime || b > this.endTime) {
            return null;
          }
          var a = this.children;
          if (a && 0 < a.length) {
            for (var c, p = 0, k = a.length - 1;k > p;) {
              var e = (p + k) / 2 | 0;
              c = a[e];
              if (b >= c.startTime && b <= c.endTime) {
                return c.query(b);
              }
              b > c.endTime ? p = e + 1 : k = e;
            }
            c = a[k];
            if (b >= c.startTime && b <= c.endTime) {
              return c.query(b);
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
          function b(n) {
            if (n.kind) {
              var p = a[n.kind.id] || (a[n.kind.id] = new c(n.kind));
              p.count++;
              p.selfTime += n.selfTime;
              p.totalTime += n.totalTime;
            }
            n.children && n.children.forEach(b);
          }
          var a = this.statistics = [];
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
      e.TimelineFrame = g;
      g = function(c) {
        function b(a) {
          c.call(this, null, null, null, null, NaN, NaN);
          this.name = a;
        }
        __extends(b, c);
        return b;
      }(g);
      e.TimelineBufferSnapshot = g;
    })(g.Profiler || (g.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = function() {
        function c(k, b) {
          void 0 === k && (k = "");
          this.name = k || "";
          this._startTime = g.isNullOrUndefined(b) ? jsGlobal.START_TIME : b;
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
          this._marks = new g.CircularBuffer(Int32Array, 20);
          this._times = new g.CircularBuffer(Float64Array, 20);
        };
        c.prototype._getKindId = function(k) {
          var b = c.MAX_KINDID;
          if (void 0 === this._kindNameMap[k]) {
            if (b = this._kinds.length, b < c.MAX_KINDID) {
              var a = {id:b, name:k, visible:!0};
              this._kinds.push(a);
              this._kindNameMap[k] = a;
            } else {
              b = c.MAX_KINDID;
            }
          } else {
            b = this._kindNameMap[k].id;
          }
          return b;
        };
        c.prototype._getMark = function(k, b, a) {
          var n = c.MAX_DATAID;
          g.isNullOrUndefined(a) || b === c.MAX_KINDID || (n = this._data.length, n < c.MAX_DATAID ? this._data.push(a) : n = c.MAX_DATAID);
          return k | n << 16 | b;
        };
        c.prototype.enter = function(k, b, a) {
          a = (g.isNullOrUndefined(a) ? performance.now() : a) - this._startTime;
          this._marks || this._initialize();
          this._depth++;
          k = this._getKindId(k);
          this._marks.write(this._getMark(c.ENTER, k, b));
          this._times.write(a);
          this._stack.push(k);
        };
        c.prototype.leave = function(k, b, a) {
          a = (g.isNullOrUndefined(a) ? performance.now() : a) - this._startTime;
          var n = this._stack.pop();
          k && (n = this._getKindId(k));
          this._marks.write(this._getMark(c.LEAVE, n, b));
          this._times.write(a);
          this._depth--;
        };
        c.prototype.count = function(c, b, a) {
        };
        c.prototype.createSnapshot = function() {
          var k;
          void 0 === k && (k = Number.MAX_VALUE);
          if (!this._marks) {
            return null;
          }
          var b = this._times, a = this._kinds, n = this._data, p = new e.TimelineBufferSnapshot(this.name), y = [p], v = 0;
          this._marks || this._initialize();
          this._marks.forEachInReverse(function(p, t) {
            var r = n[p >>> 16 & c.MAX_DATAID], u = a[p & c.MAX_KINDID];
            if (g.isNullOrUndefined(u) || u.visible) {
              var h = p & 2147483648, f = b.get(t), d = y.length;
              if (h === c.LEAVE) {
                if (1 === d && (v++, v > k)) {
                  return!0;
                }
                y.push(new e.TimelineFrame(y[d - 1], u, null, r, NaN, f));
              } else {
                if (h === c.ENTER) {
                  if (u = y.pop(), h = y[y.length - 1]) {
                    for (h.children ? h.children.unshift(u) : h.children = [u], h = y.length, u.depth = h, u.startData = r, u.startTime = f;u;) {
                      if (u.maxDepth < h) {
                        u.maxDepth = h, u = u.parent;
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
          p.children && p.children.length && (p.startTime = p.children[0].startTime, p.endTime = p.children[p.children.length - 1].endTime);
          return p;
        };
        c.prototype.reset = function(c) {
          this._startTime = g.isNullOrUndefined(c) ? performance.now() : c;
          this._marks ? (this._depth = 0, this._data = [], this._marks.reset(), this._times.reset()) : this._initialize();
        };
        c.FromFirefoxProfile = function(k, b) {
          for (var a = k.profile.threads[0].samples, n = new c(b, a[0].time), p = [], e, g = 0;g < a.length;g++) {
            e = a[g];
            var l = e.time, t = e.frames, r = 0;
            for (e = Math.min(t.length, p.length);r < e && t[r].location === p[r].location;) {
              r++;
            }
            for (var u = p.length - r, h = 0;h < u;h++) {
              e = p.pop(), n.leave(e.location, null, l);
            }
            for (;r < t.length;) {
              e = t[r++], n.enter(e.location, null, l);
            }
            p = t;
          }
          for (;e = p.pop();) {
            n.leave(e.location, null, l);
          }
          return n;
        };
        c.FromChromeProfile = function(k, b) {
          var a = k.timestamps, n = k.samples, p = new c(b, a[0] / 1E3), e = [], g = {}, l;
          c._resolveIds(k.head, g);
          for (var t = 0;t < a.length;t++) {
            var r = a[t] / 1E3, u = [];
            for (l = g[n[t]];l;) {
              u.unshift(l), l = l.parent;
            }
            var h = 0;
            for (l = Math.min(u.length, e.length);h < l && u[h] === e[h];) {
              h++;
            }
            for (var f = e.length - h, d = 0;d < f;d++) {
              l = e.pop(), p.leave(l.functionName, null, r);
            }
            for (;h < u.length;) {
              l = u[h++], p.enter(l.functionName, null, r);
            }
            e = u;
          }
          for (;l = e.pop();) {
            p.leave(l.functionName, null, r);
          }
          return p;
        };
        c._resolveIds = function(k, b) {
          b[k.id] = k;
          if (k.children) {
            for (var a = 0;a < k.children.length;a++) {
              k.children[a].parent = k, c._resolveIds(k.children[a], b);
            }
          }
        };
        c.ENTER = 0;
        c.LEAVE = -2147483648;
        c.MAX_KINDID = 65535;
        c.MAX_DATAID = 32767;
        return c;
      }();
      e.TimelineBuffer = c;
    })(m.Profiler || (m.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      (function(c) {
        c[c.DARK = 0] = "DARK";
        c[c.LIGHT = 1] = "LIGHT";
      })(e.UIThemeType || (e.UIThemeType = {}));
      var c = function() {
        function c(k, b) {
          void 0 === b && (b = 0);
          this._container = k;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = b;
          this._tooltip = this._createTooltip();
        }
        c.prototype.createProfile = function(c, b) {
          void 0 === b && (b = !0);
          var a = new e.Profile(c);
          a.createSnapshots();
          this._profiles.push(a);
          b && this.activateProfile(a);
          return a;
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
              this._theme = new m.Theme.UIThemeDark;
              break;
            case 1:
              this._theme = new m.Theme.UIThemeLight;
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
            this._overviewHeader = new e.FlameChartHeader(this, 0);
            this._overview = new e.FlameChartOverview(this, 0);
            this._activeProfile.forEachSnapshot(function(b, a) {
              c._headers.push(new e.FlameChartHeader(c, 1));
              c._charts.push(new e.FlameChart(c, b));
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
            var c = this, b = this._activeProfile.startTime, a = this._activeProfile.endTime;
            this._overviewHeader.initialize(b, a);
            this._overview.initialize(b, a);
            this._activeProfile.forEachSnapshot(function(n, p) {
              c._headers[p].initialize(b, a);
              c._charts[p].initialize(b, a);
            });
          }
        };
        c.prototype._onResize = function() {
          if (this._activeProfile) {
            var c = this, b = this._container.offsetWidth;
            this._overviewHeader.setSize(b);
            this._overview.setSize(b);
            this._activeProfile.forEachSnapshot(function(a, n) {
              c._headers[n].setSize(b);
              c._charts[n].setSize(b);
            });
          }
        };
        c.prototype._updateViews = function() {
          if (this._activeProfile) {
            var c = this, b = this._activeProfile.windowStart, a = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(b, a);
            this._overview.setWindow(b, a);
            this._activeProfile.forEachSnapshot(function(n, p) {
              c._headers[p].setWindow(b, a);
              c._charts[p].setWindow(b, a);
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
        c.prototype.setWindow = function(c, b) {
          this._activeProfile.setWindow(c, b);
          this._updateViews();
        };
        c.prototype.moveWindowTo = function(c) {
          this._activeProfile.moveWindowTo(c);
          this._updateViews();
        };
        c.prototype.showTooltip = function(c, b, a, n) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(c, b));
          this._tooltip.style.display = "block";
          var p = this._tooltip.firstChild;
          b = p.clientWidth;
          p = p.clientHeight;
          a += a + b >= c.canvas.clientWidth - 50 ? -(b + 20) : 25;
          n += c.canvas.offsetTop - p / 2;
          this._tooltip.style.left = a + "px";
          this._tooltip.style.top = n + "px";
        };
        c.prototype.hideTooltip = function() {
          this._tooltip.style.display = "none";
        };
        c.prototype.createTooltipContent = function(c, b) {
          var a = Math.round(1E5 * b.totalTime) / 1E5, n = Math.round(1E5 * b.selfTime) / 1E5, p = Math.round(1E4 * b.selfTime / b.totalTime) / 100, e = document.createElement("div"), g = document.createElement("h1");
          g.textContent = b.kind.name;
          e.appendChild(g);
          g = document.createElement("p");
          g.textContent = "Total: " + a + " ms";
          e.appendChild(g);
          a = document.createElement("p");
          a.textContent = "Self: " + n + " ms (" + p + "%)";
          e.appendChild(a);
          if (n = c.getStatistics(b.kind)) {
            p = document.createElement("p"), p.textContent = "Count: " + n.count, e.appendChild(p), p = Math.round(1E5 * n.totalTime) / 1E5, a = document.createElement("p"), a.textContent = "All Total: " + p + " ms", e.appendChild(a), n = Math.round(1E5 * n.selfTime) / 1E5, p = document.createElement("p"), p.textContent = "All Self: " + n + " ms", e.appendChild(p);
          }
          this.appendDataElements(e, b.startData);
          this.appendDataElements(e, b.endData);
          return e;
        };
        c.prototype.appendDataElements = function(c, b) {
          if (!g.isNullOrUndefined(b)) {
            c.appendChild(document.createElement("hr"));
            var a;
            if (g.isObject(b)) {
              for (var n in b) {
                a = document.createElement("p"), a.textContent = n + ": " + b[n], c.appendChild(a);
              }
            } else {
              a = document.createElement("p"), a.textContent = b.toString(), c.appendChild(a);
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
      e.Controller = c;
    })(m.Profiler || (m.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.NumberUtilities.clamp, m = function() {
        function b(a) {
          this.value = a;
        }
        b.prototype.toString = function() {
          return this.value;
        };
        b.AUTO = new b("auto");
        b.DEFAULT = new b("default");
        b.NONE = new b("none");
        b.HELP = new b("help");
        b.POINTER = new b("pointer");
        b.PROGRESS = new b("progress");
        b.WAIT = new b("wait");
        b.CELL = new b("cell");
        b.CROSSHAIR = new b("crosshair");
        b.TEXT = new b("text");
        b.ALIAS = new b("alias");
        b.COPY = new b("copy");
        b.MOVE = new b("move");
        b.NO_DROP = new b("no-drop");
        b.NOT_ALLOWED = new b("not-allowed");
        b.ALL_SCROLL = new b("all-scroll");
        b.COL_RESIZE = new b("col-resize");
        b.ROW_RESIZE = new b("row-resize");
        b.N_RESIZE = new b("n-resize");
        b.E_RESIZE = new b("e-resize");
        b.S_RESIZE = new b("s-resize");
        b.W_RESIZE = new b("w-resize");
        b.NE_RESIZE = new b("ne-resize");
        b.NW_RESIZE = new b("nw-resize");
        b.SE_RESIZE = new b("se-resize");
        b.SW_RESIZE = new b("sw-resize");
        b.EW_RESIZE = new b("ew-resize");
        b.NS_RESIZE = new b("ns-resize");
        b.NESW_RESIZE = new b("nesw-resize");
        b.NWSE_RESIZE = new b("nwse-resize");
        b.ZOOM_IN = new b("zoom-in");
        b.ZOOM_OUT = new b("zoom-out");
        b.GRAB = new b("grab");
        b.GRABBING = new b("grabbing");
        return b;
      }();
      e.MouseCursor = m;
      var k = function() {
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
            var c = this._eventTarget.parentElement;
            b._cursor !== a && (b._cursor = a, ["", "-moz-", "-webkit-"].forEach(function(b) {
              c.style.cursor = b + a;
            }));
            b._cursorOwner = b._cursor === m.DEFAULT ? null : this._target;
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
          var c = {x:a.x - b.start.x, y:a.y - b.start.y};
          b.current = a;
          b.delta = c;
          b.hasMoved = !0;
          this._target.onDrag(b.start.x, b.start.y, a.x, a.y, c.x, c.y);
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
        b.prototype._onMouseWheel = function(a) {
          if (!(a.altKey || a.metaKey || a.ctrlKey || a.shiftKey || (a.preventDefault(), this._dragInfo || this._wheelDisabled))) {
            var b = this._getTargetMousePos(a, a.target);
            a = c("undefined" !== typeof a.deltaY ? a.deltaY / 16 : -a.wheelDelta / 40, -1, 1);
            this._target.onMouseWheel(b.x, b.y, Math.pow(1.2, a) - 1);
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
          var c = b.getBoundingClientRect();
          return{x:a.clientX - c.left, y:a.clientY - c.top};
        };
        b.HOVER_TIMEOUT = 500;
        b._cursor = m.DEFAULT;
        return b;
      }();
      e.MouseController = k;
    })(m.Profiler || (m.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
      (function(c) {
        c[c.NONE = 0] = "NONE";
        c[c.WINDOW = 1] = "WINDOW";
        c[c.HANDLE_LEFT = 2] = "HANDLE_LEFT";
        c[c.HANDLE_RIGHT = 3] = "HANDLE_RIGHT";
        c[c.HANDLE_BOTH = 4] = "HANDLE_BOTH";
      })(e.FlameChartDragTarget || (e.FlameChartDragTarget = {}));
      var c = function() {
        function c(k) {
          this._controller = k;
          this._initialized = !1;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
          this._mouseController = new e.MouseController(this, this._canvas);
          k = k.container;
          k.appendChild(this._canvas);
          k = k.getBoundingClientRect();
          this.setSize(k.width);
        }
        Object.defineProperty(c.prototype, "canvas", {get:function() {
          return this._canvas;
        }, enumerable:!0, configurable:!0});
        c.prototype.setSize = function(c, b) {
          void 0 === b && (b = 20);
          this._width = c;
          this._height = b;
          this._resetCanvas();
          this.draw();
        };
        c.prototype.initialize = function(c, b) {
          this._initialized = !0;
          this.setRange(c, b);
          this.setWindow(c, b, !1);
          this.draw();
        };
        c.prototype.setWindow = function(c, b, a) {
          void 0 === a && (a = !0);
          this._windowStart = c;
          this._windowEnd = b;
          !a || this.draw();
        };
        c.prototype.setRange = function(c, b) {
          var a = !1;
          void 0 === a && (a = !0);
          this._rangeStart = c;
          this._rangeEnd = b;
          !a || this.draw();
        };
        c.prototype.destroy = function() {
          this._mouseController.destroy();
          this._mouseController = null;
          this._controller.container.removeChild(this._canvas);
          this._controller = null;
        };
        c.prototype._resetCanvas = function() {
          var c = window.devicePixelRatio, b = this._canvas;
          b.width = this._width * c;
          b.height = this._height * c;
          b.style.width = this._width + "px";
          b.style.height = this._height + "px";
        };
        c.prototype.draw = function() {
        };
        c.prototype._almostEq = function(c, b) {
          var a;
          void 0 === a && (a = 10);
          return Math.abs(c - b) < 1 / Math.pow(10, a);
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
        c.prototype.onMouseWheel = function(e, b, a) {
          e = this._toTime(e);
          b = this._windowStart;
          var n = this._windowEnd, p = n - b;
          a = Math.max((c.MIN_WINDOW_LEN - p) / p, a);
          this._controller.setWindow(b + (b - e) * a, n + (n - e) * a);
          this.onHoverEnd();
        };
        c.prototype.onMouseDown = function(c, b) {
        };
        c.prototype.onMouseMove = function(c, b) {
        };
        c.prototype.onMouseOver = function(c, b) {
        };
        c.prototype.onMouseOut = function() {
        };
        c.prototype.onDrag = function(c, b, a, n, p, e) {
        };
        c.prototype.onDragEnd = function(c, b, a, n, p, e) {
        };
        c.prototype.onClick = function(c, b) {
        };
        c.prototype.onHoverStart = function(c, b) {
        };
        c.prototype.onHoverEnd = function() {
        };
        c.DRAGHANDLE_WIDTH = 4;
        c.MIN_WINDOW_LEN = .1;
        return c;
      }();
      e.FlameChartBase = c;
    })(g.Profiler || (g.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.StringUtilities.trimMiddle, m = function(k) {
        function b(a, b) {
          k.call(this, a);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = b;
          this._kindStyle = Object.create(null);
        }
        __extends(b, k);
        b.prototype.setSize = function(a, b) {
          k.prototype.setSize.call(this, a, b || this._initialized ? 12.5 * this._maxDepth : 100);
        };
        b.prototype.initialize = function(a, b) {
          this._initialized = !0;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(a, b);
          this.setWindow(a, b, !1);
          this.setSize(this._width, 12.5 * this._maxDepth);
        };
        b.prototype.destroy = function() {
          k.prototype.destroy.call(this);
          this._snapshot = null;
        };
        b.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio;
          g.ColorStyle.reset();
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, this._width, this._height);
          this._initialized && this._drawChildren(this._snapshot);
          a.restore();
        };
        b.prototype._drawChildren = function(a, b) {
          void 0 === b && (b = 0);
          var c = a.getChildRange(this._windowStart, this._windowEnd);
          if (c) {
            for (var e = c.startIndex;e <= c.endIndex;e++) {
              var k = a.children[e];
              this._drawFrame(k, b) && this._drawChildren(k, b + 1);
            }
          }
        };
        b.prototype._drawFrame = function(a, b) {
          var c = this._context, e = this._toPixels(a.startTime), k = this._toPixels(a.endTime), l = k - e;
          if (l <= this._minFrameWidthInPixels) {
            return c.fillStyle = this._controller.theme.tabToolbar(1), c.fillRect(e, 12.5 * b, this._minFrameWidthInPixels, 12 + 12.5 * (a.maxDepth - a.depth)), !1;
          }
          0 > e && (k = l + e, e = 0);
          var k = k - e, t = this._kindStyle[a.kind.id];
          t || (t = g.ColorStyle.randomStyle(), t = this._kindStyle[a.kind.id] = {bgColor:t, textColor:g.ColorStyle.contrastStyle(t)});
          c.save();
          c.fillStyle = t.bgColor;
          c.fillRect(e, 12.5 * b, k, 12);
          12 < l && (l = a.kind.name) && l.length && (l = this._prepareText(c, l, k - 4), l.length && (c.fillStyle = t.textColor, c.textBaseline = "bottom", c.fillText(l, e + 2, 12.5 * (b + 1) - 1)));
          c.restore();
          return!0;
        };
        b.prototype._prepareText = function(a, b, p) {
          var e = this._measureWidth(a, b);
          if (p > e) {
            return b;
          }
          for (var e = 3, k = b.length;e < k;) {
            var l = e + k >> 1;
            this._measureWidth(a, c(b, l)) < p ? e = l + 1 : k = l;
          }
          b = c(b, k - 1);
          e = this._measureWidth(a, b);
          return e <= p ? b : "";
        };
        b.prototype._measureWidth = function(a, b) {
          var c = this._textWidth[b];
          c || (c = a.measureText(b).width, this._textWidth[b] = c);
          return c;
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
          var c = 1 + b / 12.5 | 0, e = this._snapshot.query(this._toTime(a));
          if (e && e.depth >= c) {
            for (;e && e.depth > c;) {
              e = e.parent;
            }
            return e;
          }
          return null;
        };
        b.prototype.onMouseDown = function(a, b) {
          this._windowEqRange() || (this._mouseController.updateCursor(e.MouseCursor.ALL_SCROLL), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:1});
        };
        b.prototype.onMouseMove = function(a, b) {
        };
        b.prototype.onMouseOver = function(a, b) {
        };
        b.prototype.onMouseOut = function() {
        };
        b.prototype.onDrag = function(a, b, c, e, k, l) {
          if (a = this._dragInfo) {
            k = this._toTimeRelative(-k), this._controller.setWindow(a.windowStartInitial + k, a.windowEndInitial + k);
          }
        };
        b.prototype.onDragEnd = function(a, b, c, k, g, l) {
          this._dragInfo = null;
          this._mouseController.updateCursor(e.MouseCursor.DEFAULT);
        };
        b.prototype.onClick = function(a, b) {
          this._dragInfo = null;
          this._mouseController.updateCursor(e.MouseCursor.DEFAULT);
        };
        b.prototype.onHoverStart = function(a, b) {
          var c = this._getFrameAtPosition(a, b);
          c && (this._hoveredFrame = c, this._controller.showTooltip(this, c, a, b));
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
      }(e.FlameChartBase);
      e.FlameChart = m;
    })(m.Profiler || (m.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.NumberUtilities.clamp;
      (function(c) {
        c[c.OVERLAY = 0] = "OVERLAY";
        c[c.STACK = 1] = "STACK";
        c[c.UNION = 2] = "UNION";
      })(e.FlameChartOverviewMode || (e.FlameChartOverviewMode = {}));
      var m = function(k) {
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
          var a = this._context, b = window.devicePixelRatio, c = this._width, e = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, c, e);
          a.restore();
          this._initialized && (this._overviewCanvasDirty && (this._drawChart(), this._overviewCanvasDirty = !1), a.drawImage(this._overviewCanvas, 0, 0), this._drawSelection());
        };
        b.prototype._drawSelection = function() {
          var a = this._context, b = this._height, c = window.devicePixelRatio, e = this._selection ? this._selection.left : this._toPixels(this._windowStart), k = this._selection ? this._selection.right : this._toPixels(this._windowEnd), l = this._controller.theme;
          a.save();
          a.scale(c, c);
          this._selection ? (a.fillStyle = l.selectionText(.15), a.fillRect(e, 1, k - e, b - 1), a.fillStyle = "rgba(133, 0, 0, 1)", a.fillRect(e + .5, 0, k - e - 1, 4), a.fillRect(e + .5, b - 4, k - e - 1, 4)) : (a.fillStyle = l.bodyBackground(.4), a.fillRect(0, 1, e, b - 1), a.fillRect(k, 1, this._width, b - 1));
          a.beginPath();
          a.moveTo(e, 0);
          a.lineTo(e, b);
          a.moveTo(k, 0);
          a.lineTo(k, b);
          a.lineWidth = .5;
          a.strokeStyle = l.foregroundTextGrey(1);
          a.stroke();
          b = Math.abs((this._selection ? this._toTime(this._selection.right) : this._windowEnd) - (this._selection ? this._toTime(this._selection.left) : this._windowStart));
          a.fillStyle = l.selectionText(.5);
          a.font = "8px sans-serif";
          a.textBaseline = "alphabetic";
          a.textAlign = "end";
          a.fillText(b.toFixed(2), Math.min(e, k) - 4, 10);
          a.fillText((b / 60).toFixed(2), Math.min(e, k) - 4, 20);
          a.restore();
        };
        b.prototype._drawChart = function() {
          var a = window.devicePixelRatio, b = this._height, c = this._controller.activeProfile, e = 4 * this._width, k = c.totalTime / e, l = this._overviewContext, g = this._controller.theme.blueHighlight(1);
          l.save();
          l.translate(0, a * b);
          var r = -a * b / (c.maxDepth - 1);
          l.scale(a / 4, r);
          l.clearRect(0, 0, e, c.maxDepth - 1);
          1 == this._mode && l.scale(1, 1 / c.snapshotCount);
          for (var m = 0, h = c.snapshotCount;m < h;m++) {
            var f = c.getSnapshotAt(m);
            if (f) {
              var d = null, q = 0;
              l.beginPath();
              l.moveTo(0, 0);
              for (var x = 0;x < e;x++) {
                q = c.startTime + x * k, q = (d = d ? d.queryNext(q) : f.query(q)) ? d.getDepth() - 1 : 0, l.lineTo(x, q);
              }
              l.lineTo(x, 0);
              l.fillStyle = g;
              l.fill();
              1 == this._mode && l.translate(0, -b * a / r);
            }
          }
          l.restore();
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
            var c = this._toPixels(this._windowStart), k = this._toPixels(this._windowEnd), g = 2 + e.FlameChartBase.DRAGHANDLE_WIDTH / 2, l = a >= c - g && a <= c + g, t = a >= k - g && a <= k + g;
            if (l && t) {
              return 4;
            }
            if (l) {
              return 2;
            }
            if (t) {
              return 3;
            }
            if (!this._windowEqRange() && a > c + g && a < k - g) {
              return 1;
            }
          }
          return 0;
        };
        b.prototype.onMouseDown = function(a, b) {
          var c = this._getDragTargetUnderCursor(a, b);
          0 === c ? (this._selection = {left:a, right:a}, this.draw()) : (1 === c && this._mouseController.updateCursor(e.MouseCursor.GRABBING), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:c});
        };
        b.prototype.onMouseMove = function(a, b) {
          var c = e.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, b);
          0 === k || this._selection || (c = 1 === k ? e.MouseCursor.GRAB : e.MouseCursor.EW_RESIZE);
          this._mouseController.updateCursor(c);
        };
        b.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        b.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(e.MouseCursor.DEFAULT);
        };
        b.prototype.onDrag = function(a, b, p, k, g, l) {
          if (this._selection) {
            this._selection = {left:a, right:c(p, 0, this._width - 1)}, this.draw();
          } else {
            a = this._dragInfo;
            if (4 === a.target) {
              if (0 !== g) {
                a.target = 0 > g ? 2 : 3;
              } else {
                return;
              }
            }
            b = this._windowStart;
            p = this._windowEnd;
            g = this._toTimeRelative(g);
            switch(a.target) {
              case 1:
                b = a.windowStartInitial + g;
                p = a.windowEndInitial + g;
                break;
              case 2:
                b = c(a.windowStartInitial + g, this._rangeStart, p - e.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3:
                p = c(a.windowEndInitial + g, b + e.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(b, p);
          }
        };
        b.prototype.onDragEnd = function(a, b, c, e, k, l) {
          this._selection && (this._selection = null, this._controller.setWindow(this._toTime(a), this._toTime(c)));
          this._dragInfo = null;
          this.onMouseMove(c, e);
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
      }(e.FlameChartBase);
      e.FlameChartOverview = m;
    })(m.Profiler || (m.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.NumberUtilities.clamp;
      (function(c) {
        c[c.OVERVIEW = 0] = "OVERVIEW";
        c[c.CHART = 1] = "CHART";
      })(e.FlameChartHeaderType || (e.FlameChartHeaderType = {}));
      var m = function(k) {
        function b(a, b) {
          this._type = b;
          k.call(this, a);
        }
        __extends(b, k);
        b.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio, c = this._width, e = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.tabToolbar(1);
          a.fillRect(0, 0, c, e);
          this._initialized && (0 == this._type ? (b = this._toPixels(this._windowStart), c = this._toPixels(this._windowEnd), a.fillStyle = this._controller.theme.bodyBackground(1), a.fillRect(b, 0, c - b, e), this._drawLabels(this._rangeStart, this._rangeEnd), this._drawDragHandle(b), this._drawDragHandle(c)) : this._drawLabels(this._windowStart, this._windowEnd));
          a.restore();
        };
        b.prototype._drawLabels = function(a, c) {
          var p = this._context, e = this._calculateTickInterval(a, c), k = Math.ceil(a / e) * e, l = 500 <= e, g = l ? 1E3 : 1, r = this._decimalPlaces(e / g), l = l ? "s" : "ms", m = this._toPixels(k), h = this._height / 2, f = this._controller.theme;
          p.lineWidth = 1;
          p.strokeStyle = f.contentTextDarkGrey(.5);
          p.fillStyle = f.contentTextDarkGrey(1);
          p.textAlign = "right";
          p.textBaseline = "middle";
          p.font = "11px sans-serif";
          for (f = this._width + b.TICK_MAX_WIDTH;m < f;) {
            p.fillText((k / g).toFixed(r) + " " + l, m - 7, h + 1), p.beginPath(), p.moveTo(m, 0), p.lineTo(m, this._height + 1), p.closePath(), p.stroke(), k += e, m = this._toPixels(k);
          }
        };
        b.prototype._calculateTickInterval = function(a, c) {
          var e = (c - a) / (this._width / b.TICK_MAX_WIDTH), k = Math.pow(10, Math.floor(Math.log(e) / Math.LN10)), e = e / k;
          return 5 < e ? 10 * k : 2 < e ? 5 * k : 1 < e ? 2 * k : k;
        };
        b.prototype._drawDragHandle = function(a) {
          var b = this._context;
          b.lineWidth = 2;
          b.strokeStyle = this._controller.theme.bodyBackground(1);
          b.fillStyle = this._controller.theme.foregroundTextGrey(.7);
          this._drawRoundedRect(b, a - e.FlameChartBase.DRAGHANDLE_WIDTH / 2, e.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2);
        };
        b.prototype._drawRoundedRect = function(a, b, c, e) {
          var k, l = !0;
          void 0 === l && (l = !0);
          void 0 === k && (k = !0);
          a.beginPath();
          a.moveTo(b + 2, 1);
          a.lineTo(b + c - 2, 1);
          a.quadraticCurveTo(b + c, 1, b + c, 3);
          a.lineTo(b + c, 1 + e - 2);
          a.quadraticCurveTo(b + c, 1 + e, b + c - 2, 1 + e);
          a.lineTo(b + 2, 1 + e);
          a.quadraticCurveTo(b, 1 + e, b, 1 + e - 2);
          a.lineTo(b, 3);
          a.quadraticCurveTo(b, 1, b + 2, 1);
          a.closePath();
          l && a.stroke();
          k && a.fill();
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
          return this._toTimeRelative(a) + (0 === this._type ? this._rangeStart : this._windowStart);
        };
        b.prototype._getDragTargetUnderCursor = function(a, b) {
          if (0 <= b && b < this._height) {
            if (0 === this._type) {
              var c = this._toPixels(this._windowStart), k = this._toPixels(this._windowEnd), g = 2 + e.FlameChartBase.DRAGHANDLE_WIDTH / 2, c = a >= c - g && a <= c + g, k = a >= k - g && a <= k + g;
              if (c && k) {
                return 4;
              }
              if (c) {
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
          var c = this._getDragTargetUnderCursor(a, b);
          1 === c && this._mouseController.updateCursor(e.MouseCursor.GRABBING);
          this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:c};
        };
        b.prototype.onMouseMove = function(a, b) {
          var c = e.MouseCursor.DEFAULT, k = this._getDragTargetUnderCursor(a, b);
          0 !== k && (1 !== k ? c = e.MouseCursor.EW_RESIZE : 1 !== k || this._windowEqRange() || (c = e.MouseCursor.GRAB));
          this._mouseController.updateCursor(c);
        };
        b.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        b.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(e.MouseCursor.DEFAULT);
        };
        b.prototype.onDrag = function(a, b, p, k, g, l) {
          a = this._dragInfo;
          if (4 === a.target) {
            if (0 !== g) {
              a.target = 0 > g ? 2 : 3;
            } else {
              return;
            }
          }
          b = this._windowStart;
          p = this._windowEnd;
          g = this._toTimeRelative(g);
          switch(a.target) {
            case 1:
              p = 0 === this._type ? 1 : -1;
              b = a.windowStartInitial + p * g;
              p = a.windowEndInitial + p * g;
              break;
            case 2:
              b = c(a.windowStartInitial + g, this._rangeStart, p - e.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3:
              p = c(a.windowEndInitial + g, b + e.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(b, p);
        };
        b.prototype.onDragEnd = function(a, b, c, e, k, l) {
          this._dragInfo = null;
          this.onMouseMove(c, e);
        };
        b.prototype.onClick = function(a, b) {
          1 === this._dragInfo.target && this._mouseController.updateCursor(e.MouseCursor.GRAB);
        };
        b.prototype.onHoverStart = function(a, b) {
        };
        b.prototype.onHoverEnd = function() {
        };
        b.TICK_MAX_WIDTH = 75;
        return b;
      }(e.FlameChartBase);
      e.FlameChartHeader = m;
    })(m.Profiler || (m.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
      (function(c) {
        var e = function() {
          function b(a, b, c, e, k) {
            this.pageLoaded = a;
            this.threadsTotal = b;
            this.threadsLoaded = c;
            this.threadFilesTotal = e;
            this.threadFilesLoaded = k;
          }
          b.prototype.toString = function() {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function(a, b, c) {
              return a + ":" + this[a];
            }, this).join(", ") + "]";
          };
          return b;
        }();
        c.TraceLoggerProgressInfo = e;
        var k = function() {
          function b(a) {
            this._baseUrl = a;
            this._threads = [];
            this._progressInfo = null;
          }
          b.prototype.loadPage = function(a, b, c) {
            this._threads = [];
            this._pageLoadCallback = b;
            this._pageLoadProgressCallback = c;
            this._progressInfo = new e(!1, 0, 0, 0, 0);
            this._loadData([a], this._onLoadPage.bind(this));
          };
          Object.defineProperty(b.prototype, "buffers", {get:function() {
            for (var a = [], b = 0, c = this._threads.length;b < c;b++) {
              a.push(this._threads[b].buffer);
            }
            return a;
          }, enumerable:!0, configurable:!0});
          b.prototype._onProgress = function() {
            this._pageLoadProgressCallback && this._pageLoadProgressCallback.call(this, this._progressInfo);
          };
          b.prototype._onLoadPage = function(a) {
            if (a && 1 == a.length) {
              var b = this, e = 0;
              a = a[0];
              var k = a.length;
              this._threads = Array(k);
              this._progressInfo.pageLoaded = !0;
              this._progressInfo.threadsTotal = k;
              for (var g = 0;g < a.length;g++) {
                var l = a[g], t = [l.dict, l.tree];
                l.corrections && t.push(l.corrections);
                this._progressInfo.threadFilesTotal += t.length;
                this._loadData(t, function(a) {
                  return function(l) {
                    l && (l = new c.Thread(l), l.buffer.name = "Thread " + a, b._threads[a] = l);
                    e++;
                    b._progressInfo.threadsLoaded++;
                    b._onProgress();
                    e === k && b._pageLoadCallback.call(b, null, b._threads);
                  };
                }(g), function(a) {
                  b._progressInfo.threadFilesLoaded++;
                  b._onProgress();
                });
              }
              this._onProgress();
            } else {
              this._pageLoadCallback.call(this, "Error loading page.", null);
            }
          };
          b.prototype._loadData = function(a, b, c) {
            var e = 0, k = 0, l = a.length, g = [];
            g.length = l;
            for (var r = 0;r < l;r++) {
              var m = this._baseUrl + a[r], h = new XMLHttpRequest, f = /\.tl$/i.test(m) ? "arraybuffer" : "json";
              h.open("GET", m, !0);
              h.responseType = f;
              h.onload = function(a, f) {
                return function(h) {
                  if ("json" === f) {
                    if (h = this.response, "string" === typeof h) {
                      try {
                        h = JSON.parse(h), g[a] = h;
                      } catch (s) {
                        k++;
                      }
                    } else {
                      g[a] = h;
                    }
                  } else {
                    g[a] = this.response;
                  }
                  ++e;
                  c && c(e);
                  e === l && b(g);
                };
              }(r, f);
              h.send();
            }
          };
          b.colors = "#0044ff #8c4b00 #cc5c33 #ff80c4 #ffbfd9 #ff8800 #8c5e00 #adcc33 #b380ff #bfd9ff #ffaa00 #8c0038 #bf8f30 #f780ff #cc99c9 #aaff00 #000073 #452699 #cc8166 #cca799 #000066 #992626 #cc6666 #ccc299 #ff6600 #526600 #992663 #cc6681 #99ccc2 #ff0066 #520066 #269973 #61994d #739699 #ffcc00 #006629 #269199 #94994d #738299 #ff0000 #590000 #234d8c #8c6246 #7d7399 #ee00ff #00474d #8c2385 #8c7546 #7c8c69 #eeff00 #4d003d #662e1a #62468c #8c6969 #6600ff #4c2900 #1a6657 #8c464f #8c6981 #44ff00 #401100 #1a2466 #663355 #567365 #d90074 #403300 #101d40 #59562d #66614d #cc0000 #002b40 #234010 #4c2626 #4d5e66 #00a3cc #400011 #231040 #4c3626 #464359 #0000bf #331b00 #80e6ff #311a33 #4d3939 #a69b00 #003329 #80ffb2 #331a20 #40303d #00a658 #40ffd9 #ffc480 #ffe1bf #332b26 #8c2500 #9933cc #80fff6 #ffbfbf #303326 #005e8c #33cc47 #b2ff80 #c8bfff #263332 #00708c #cc33ad #ffe680 #f2ffbf #262a33 #388c00 #335ccc #8091ff #bfffd9".split(" ");
          return b;
        }();
        c.TraceLogger = k;
      })(e.TraceLogger || (e.TraceLogger = {}));
    })(g.Profiler || (g.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
      (function(c) {
        var g;
        (function(c) {
          c[c.START_HI = 0] = "START_HI";
          c[c.START_LO = 4] = "START_LO";
          c[c.STOP_HI = 8] = "STOP_HI";
          c[c.STOP_LO = 12] = "STOP_LO";
          c[c.TEXTID = 16] = "TEXTID";
          c[c.NEXTID = 20] = "NEXTID";
        })(g || (g = {}));
        g = function() {
          function c(b) {
            2 <= b.length && (this._text = b[0], this._data = new DataView(b[1]), this._buffer = new e.TimelineBuffer, this._walkTree(0));
          }
          Object.defineProperty(c.prototype, "buffer", {get:function() {
            return this._buffer;
          }, enumerable:!0, configurable:!0});
          c.prototype._walkTree = function(b) {
            var a = this._data, e = this._buffer;
            do {
              var g = b * c.ITEM_SIZE, m = 4294967295 * a.getUint32(g + 0) + a.getUint32(g + 4), v = 4294967295 * a.getUint32(g + 8) + a.getUint32(g + 12), l = a.getUint32(g + 16), g = a.getUint32(g + 20), t = 1 === (l & 1), l = l >>> 1, l = this._text[l];
              e.enter(l, null, m / 1E6);
              t && this._walkTree(b + 1);
              e.leave(l, null, v / 1E6);
              b = g;
            } while (0 !== b);
          };
          c.ITEM_SIZE = 24;
          return c;
        }();
        c.Thread = g;
      })(e.TraceLogger || (e.TraceLogger = {}));
    })(g.Profiler || (g.Profiler = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.NumberUtilities.clamp, m = function() {
        function b() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        b.prototype.append = function(a, b) {
          var c = this.lines;
          0 < c.length && c[c.length - 1] === a ? this.repeat[c.length - 1]++ : (this.lines.push(a), this.repeat.push(1), this.format.push(b ? {backgroundFillStyle:b} : void 0), this.time.push(performance.now()), this.length++);
        };
        b.prototype.get = function(a) {
          return this.lines[a];
        };
        b.prototype.getFormat = function(a) {
          return this.format[a];
        };
        b.prototype.getTime = function(a) {
          return this.time[a];
        };
        b.prototype.getRepeat = function(a) {
          return this.repeat[a];
        };
        return b;
      }();
      e.Buffer = m;
      var k = function() {
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
          this.buffer = new m;
          a.addEventListener("keydown", function(a) {
            var s = 0;
            switch(a.keyCode) {
              case d:
                this.showLineNumbers = !this.showLineNumbers;
                break;
              case q:
                this.showLineTime = !this.showLineTime;
                break;
              case l:
                s = -1;
                break;
              case k:
                s = 1;
                break;
              case b:
                s = -this.pageLineCount;
                break;
              case c:
                s = this.pageLineCount;
                break;
              case e:
                s = -this.lineIndex;
                break;
              case g:
                s = this.buffer.length - this.lineIndex;
                break;
              case r:
                this.columnIndex -= a.metaKey ? 10 : 1;
                0 > this.columnIndex && (this.columnIndex = 0);
                a.preventDefault();
                break;
              case u:
                this.columnIndex += a.metaKey ? 10 : 1;
                a.preventDefault();
                break;
              case h:
                a.metaKey && (this.selection = {start:0, end:this.buffer.length}, a.preventDefault());
                break;
              case f:
                if (a.metaKey) {
                  var O = "";
                  if (this.selection) {
                    for (var T = this.selection.start;T <= this.selection.end;T++) {
                      O += this.buffer.get(T) + "\n";
                    }
                  } else {
                    O = this.buffer.get(this.lineIndex);
                  }
                  alert(O);
                }
              ;
            }
            a.metaKey && (s *= this.pageLineCount);
            s && (this.scroll(s), a.preventDefault());
            s && a.shiftKey ? this.selection ? this.lineIndex > this.selection.start ? this.selection.end = this.lineIndex : this.selection.start = this.lineIndex : 0 < s ? this.selection = {start:this.lineIndex - s, end:this.lineIndex} : 0 > s && (this.selection = {start:this.lineIndex, end:this.lineIndex - s}) : s && (this.selection = null);
            this.paint();
          }.bind(this), !1);
          a.addEventListener("focus", function(a) {
            this.hasFocus = !0;
          }.bind(this), !1);
          a.addEventListener("blur", function(a) {
            this.hasFocus = !1;
          }.bind(this), !1);
          var b = 33, c = 34, e = 36, g = 35, l = 38, k = 40, r = 37, u = 39, h = 65, f = 67, d = 78, q = 84;
        }
        b.prototype.resize = function() {
          this._resizeHandler();
        };
        b.prototype._resizeHandler = function() {
          var a = this.canvas.parentElement, b = a.clientWidth, a = a.clientHeight - 1, c = window.devicePixelRatio || 1;
          1 !== c ? (this.ratio = c / 1, this.canvas.width = b * this.ratio, this.canvas.height = a * this.ratio, this.canvas.style.width = b + "px", this.canvas.style.height = a + "px") : (this.ratio = 1, this.canvas.width = b, this.canvas.height = a);
          this.pageLineCount = Math.floor(this.canvas.height / this.lineHeight);
        };
        b.prototype.gotoLine = function(a) {
          this.lineIndex = c(a, 0, this.buffer.length - 1);
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
          var b = this.textMarginLeft, c = b + (this.showLineNumbers ? 5 * (String(this.buffer.length).length + 2) : 0), e = c + (this.showLineTime ? 40 : 10), g = e + 25;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
          for (var l = this.canvas.width, k = this.lineHeight, r = 0;r < a;r++) {
            var m = r * this.lineHeight, h = this.pageIndex + r, f = this.buffer.get(h), d = this.buffer.getFormat(h), q = this.buffer.getRepeat(h), x = 1 < h ? this.buffer.getTime(h) - this.buffer.getTime(0) : 0;
            this.context.fillStyle = h % 2 ? this.lineColor : this.alternateLineColor;
            d && d.backgroundFillStyle && (this.context.fillStyle = d.backgroundFillStyle);
            this.context.fillRect(0, m, l, k);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;
            this.selection && h >= this.selection.start && h <= this.selection.end && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, m, l, k), this.context.fillStyle = this.selectionTextColor);
            this.hasFocus && h === this.lineIndex && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, m, l, k), this.context.fillStyle = this.selectionTextColor);
            0 < this.columnIndex && (f = f.substring(this.columnIndex));
            m = (r + 1) * this.lineHeight - this.textMarginBottom;
            this.showLineNumbers && this.context.fillText(String(h), b, m);
            this.showLineTime && this.context.fillText(x.toFixed(1).padLeft(" ", 6), c, m);
            1 < q && this.context.fillText(String(q).padLeft(" ", 3), e, m);
            this.context.fillText(f, g, m);
          }
        };
        b.prototype.refreshEvery = function(a) {
          function b() {
            c.paint();
            c.refreshFrequency && setTimeout(b, c.refreshFrequency);
          }
          var c = this;
          this.refreshFrequency = a;
          c.refreshFrequency && setTimeout(b, c.refreshFrequency);
        };
        b.prototype.isScrolledToBottom = function() {
          return this.lineIndex === this.buffer.length - 1;
        };
        return b;
      }();
      e.Terminal = k;
    })(m.Terminal || (m.Terminal = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
      var c = function() {
        function c(e) {
          this._lastWeightedTime = this._lastTime = this._index = 0;
          this._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
          this._container = e;
          this._canvas = document.createElement("canvas");
          this._container.appendChild(this._canvas);
          this._context = this._canvas.getContext("2d");
          this._listenForContainerSizeChanges();
        }
        c.prototype._listenForContainerSizeChanges = function() {
          var c = this._containerWidth, b = this._containerHeight;
          this._onContainerSizeChanged();
          var a = this;
          setInterval(function() {
            if (c !== a._containerWidth || b !== a._containerHeight) {
              a._onContainerSizeChanged(), c = a._containerWidth, b = a._containerHeight;
            }
          }, 10);
        };
        c.prototype._onContainerSizeChanged = function() {
          var c = this._containerWidth, b = this._containerHeight, a = window.devicePixelRatio || 1;
          1 !== a ? (this._ratio = a / 1, this._canvas.width = c * this._ratio, this._canvas.height = b * this._ratio, this._canvas.style.width = c + "px", this._canvas.style.height = b + "px") : (this._ratio = 1, this._canvas.width = c, this._canvas.height = b);
        };
        Object.defineProperty(c.prototype, "_containerWidth", {get:function() {
          return this._container.clientWidth;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "_containerHeight", {get:function() {
          return this._container.clientHeight;
        }, enumerable:!0, configurable:!0});
        c.prototype.tickAndRender = function(c, b) {
          void 0 === c && (c = !1);
          void 0 === b && (b = 0);
          if (0 === this._lastTime) {
            this._lastTime = performance.now();
          } else {
            var a = 1 * (performance.now() - this._lastTime) + 0 * this._lastWeightedTime, e = this._context, g = 2 * this._ratio, m = 30 * this._ratio, v = performance;
            v.memory && (m += 30 * this._ratio);
            var l = (this._canvas.width - m) / (g + 1) | 0, t = this._index++;
            this._index > l && (this._index = 0);
            l = this._canvas.height;
            e.globalAlpha = 1;
            e.fillStyle = "black";
            e.fillRect(m + t * (g + 1), 0, 4 * g, this._canvas.height);
            var r = Math.min(1E3 / 60 / a, 1);
            e.fillStyle = "#00FF00";
            e.globalAlpha = c ? .5 : 1;
            r = l / 2 * r | 0;
            e.fillRect(m + t * (g + 1), l - r, g, r);
            b && (r = Math.min(1E3 / 240 / b, 1), e.fillStyle = "#FF6347", r = l / 2 * r | 0, e.fillRect(m + t * (g + 1), l / 2 - r, g, r));
            0 === t % 16 && (e.globalAlpha = 1, e.fillStyle = "black", e.fillRect(0, 0, m, this._canvas.height), e.fillStyle = "white", e.font = 8 * this._ratio + "px Arial", e.textBaseline = "middle", g = (1E3 / a).toFixed(0), b && (g += " " + b.toFixed(0)), v.memory && (g += " " + (v.memory.usedJSHeapSize / 1024 / 1024).toFixed(2)), e.fillText(g, 2 * this._ratio, this._containerHeight / 2 * this._ratio));
            this._lastTime = performance.now();
            this._lastWeightedTime = a;
          }
        };
        return c;
      }();
      e.FPS = c;
    })(g.Mini || (g.Mini = {}));
  })(g.Tools || (g.Tools = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load Shared Dependencies");
console.time("Load GFX Dependencies");
(function(g) {
  (function(m) {
    function e(a, d, f) {
      return p && f ? "string" === typeof d ? (a = g.ColorUtilities.cssStyleToRGBA(d), g.ColorUtilities.rgbaToCSSStyle(f.transformRGBA(a))) : d instanceof CanvasGradient && d._template ? d._template.createCanvasGradient(a, f) : d : d;
    }
    var c = g.Debug.assert, w = g.NumberUtilities.clamp;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Brief = 1] = "Brief";
      a[a.Verbose = 2] = "Verbose";
    })(m.TraceLevel || (m.TraceLevel = {}));
    var k = g.Metrics.Counter.instance;
    m.frameCounter = new g.Metrics.Counter(!0);
    m.traceLevel = 2;
    m.writer = null;
    m.frameCount = function(a) {
      k.count(a);
      m.frameCounter.count(a);
    };
    m.timelineBuffer = new g.Tools.Profiler.TimelineBuffer("GFX");
    m.enterTimeline = function(a, d) {
    };
    m.leaveTimeline = function(a, d) {
    };
    var b = null, a = null, n = null, p = !0;
    p && "undefined" !== typeof CanvasRenderingContext2D && (b = CanvasGradient.prototype.addColorStop, a = CanvasRenderingContext2D.prototype.createLinearGradient, n = CanvasRenderingContext2D.prototype.createRadialGradient, CanvasRenderingContext2D.prototype.createLinearGradient = function(a, d, f, b) {
      return(new v(a, d, f, b)).createCanvasGradient(this, null);
    }, CanvasRenderingContext2D.prototype.createRadialGradient = function(a, d, f, b, c, q) {
      return(new l(a, d, f, b, c, q)).createCanvasGradient(this, null);
    }, CanvasGradient.prototype.addColorStop = function(a, d) {
      b.call(this, a, d);
      this._template.addColorStop(a, d);
    });
    var y = function() {
      return function(a, d) {
        this.offset = a;
        this.color = d;
      };
    }(), v = function() {
      function d(a, f, b, c) {
        this.x0 = a;
        this.y0 = f;
        this.x1 = b;
        this.y1 = c;
        this.colorStops = [];
      }
      d.prototype.addColorStop = function(a, d) {
        this.colorStops.push(new y(a, d));
      };
      d.prototype.createCanvasGradient = function(d, f) {
        for (var c = a.call(d, this.x0, this.y0, this.x1, this.y1), q = this.colorStops, h = 0;h < q.length;h++) {
          var s = q[h], x = s.offset, s = s.color, s = f ? e(d, s, f) : s;
          b.call(c, x, s);
        }
        c._template = this;
        c._transform = this._transform;
        return c;
      };
      return d;
    }(), l = function() {
      function a(d, f, b, c, q, h) {
        this.x0 = d;
        this.y0 = f;
        this.r0 = b;
        this.x1 = c;
        this.y1 = q;
        this.r1 = h;
        this.colorStops = [];
      }
      a.prototype.addColorStop = function(a, d) {
        this.colorStops.push(new y(a, d));
      };
      a.prototype.createCanvasGradient = function(a, d) {
        for (var f = n.call(a, this.x0, this.y0, this.r0, this.x1, this.y1, this.r1), c = this.colorStops, q = 0;q < c.length;q++) {
          var h = c[q], s = h.offset, h = h.color, h = d ? e(a, h, d) : h;
          b.call(f, s, h);
        }
        f._template = this;
        f._transform = this._transform;
        return f;
      };
      return a;
    }(), t;
    (function(a) {
      a[a.ClosePath = 1] = "ClosePath";
      a[a.MoveTo = 2] = "MoveTo";
      a[a.LineTo = 3] = "LineTo";
      a[a.QuadraticCurveTo = 4] = "QuadraticCurveTo";
      a[a.BezierCurveTo = 5] = "BezierCurveTo";
      a[a.ArcTo = 6] = "ArcTo";
      a[a.Rect = 7] = "Rect";
      a[a.Arc = 8] = "Arc";
      a[a.Save = 9] = "Save";
      a[a.Restore = 10] = "Restore";
      a[a.Transform = 11] = "Transform";
    })(t || (t = {}));
    var r = function() {
      function a(d) {
        this._commands = new Uint8Array(a._arrayBufferPool.acquire(8), 0, 8);
        this._commandPosition = 0;
        this._data = new Float64Array(a._arrayBufferPool.acquire(8 * Float64Array.BYTES_PER_ELEMENT), 0, 8);
        this._dataPosition = 0;
        d instanceof a && this.addPath(d);
      }
      a._apply = function(a, d) {
        var f = a._commands, b = a._data, c = 0, q = 0;
        d.beginPath();
        for (var h = a._commandPosition;c < h;) {
          switch(f[c++]) {
            case 1:
              d.closePath();
              break;
            case 2:
              d.moveTo(b[q++], b[q++]);
              break;
            case 3:
              d.lineTo(b[q++], b[q++]);
              break;
            case 4:
              d.quadraticCurveTo(b[q++], b[q++], b[q++], b[q++]);
              break;
            case 5:
              d.bezierCurveTo(b[q++], b[q++], b[q++], b[q++], b[q++], b[q++]);
              break;
            case 6:
              d.arcTo(b[q++], b[q++], b[q++], b[q++], b[q++]);
              break;
            case 7:
              d.rect(b[q++], b[q++], b[q++], b[q++]);
              break;
            case 8:
              d.arc(b[q++], b[q++], b[q++], b[q++], b[q++], !!b[q++]);
              break;
            case 9:
              d.save();
              break;
            case 10:
              d.restore();
              break;
            case 11:
              d.transform(b[q++], b[q++], b[q++], b[q++], b[q++], b[q++]);
          }
        }
      };
      a.prototype._ensureCommandCapacity = function(d) {
        this._commands = a._arrayBufferPool.ensureUint8ArrayLength(this._commands, d);
      };
      a.prototype._ensureDataCapacity = function(d) {
        this._data = a._arrayBufferPool.ensureFloat64ArrayLength(this._data, d);
      };
      a.prototype._writeCommand = function(a) {
        this._commandPosition >= this._commands.length && this._ensureCommandCapacity(this._commandPosition + 1);
        this._commands[this._commandPosition++] = a;
      };
      a.prototype._writeData = function(a, d, f, b, q, h) {
        var s = arguments.length;
        c(6 >= s && (0 === s % 2 || 5 === s));
        this._dataPosition + s >= this._data.length && this._ensureDataCapacity(this._dataPosition + s);
        var x = this._data, e = this._dataPosition;
        x[e] = a;
        x[e + 1] = d;
        2 < s && (x[e + 2] = f, x[e + 3] = b, 4 < s && (x[e + 4] = q, 6 === s && (x[e + 5] = h)));
        this._dataPosition += s;
      };
      a.prototype.closePath = function() {
        this._writeCommand(1);
      };
      a.prototype.moveTo = function(a, d) {
        this._writeCommand(2);
        this._writeData(a, d);
      };
      a.prototype.lineTo = function(a, d) {
        this._writeCommand(3);
        this._writeData(a, d);
      };
      a.prototype.quadraticCurveTo = function(a, d, f, b) {
        this._writeCommand(4);
        this._writeData(a, d, f, b);
      };
      a.prototype.bezierCurveTo = function(a, d, f, b, c, q) {
        this._writeCommand(5);
        this._writeData(a, d, f, b, c, q);
      };
      a.prototype.arcTo = function(a, d, f, b, c) {
        this._writeCommand(6);
        this._writeData(a, d, f, b, c);
      };
      a.prototype.rect = function(a, d, f, b) {
        this._writeCommand(7);
        this._writeData(a, d, f, b);
      };
      a.prototype.arc = function(a, d, f, b, c, q) {
        this._writeCommand(8);
        this._writeData(a, d, f, b, c, +q);
      };
      a.prototype.addPath = function(a, d) {
        d && (this._writeCommand(9), this._writeCommand(11), this._writeData(d.a, d.b, d.c, d.d, d.e, d.f));
        var f = this._commandPosition + a._commandPosition;
        f >= this._commands.length && this._ensureCommandCapacity(f);
        for (var b = this._commands, c = a._commands, q = this._commandPosition, h = 0;q < f;q++) {
          b[q] = c[h++];
        }
        this._commandPosition = f;
        f = this._dataPosition + a._dataPosition;
        f >= this._data.length && this._ensureDataCapacity(f);
        b = this._data;
        c = a._data;
        q = this._dataPosition;
        for (h = 0;q < f;q++) {
          b[q] = c[h++];
        }
        this._dataPosition = f;
        d && this._writeCommand(10);
      };
      a._arrayBufferPool = new g.ArrayBufferPool;
      return a;
    }();
    m.Path = r;
    if ("undefined" !== typeof CanvasRenderingContext2D && ("undefined" === typeof Path2D || !Path2D.prototype.addPath)) {
      var u = CanvasRenderingContext2D.prototype.fill;
      CanvasRenderingContext2D.prototype.fill = function(a, d) {
        arguments.length && (a instanceof r ? r._apply(a, this) : d = a);
        d ? u.call(this, d) : u.call(this);
      };
      var h = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.stroke = function(a, d) {
        arguments.length && (a instanceof r ? r._apply(a, this) : d = a);
        d ? h.call(this, d) : h.call(this);
      };
      var f = CanvasRenderingContext2D.prototype.clip;
      CanvasRenderingContext2D.prototype.clip = function(a, d) {
        arguments.length && (a instanceof r ? r._apply(a, this) : d = a);
        d ? f.call(this, d) : f.call(this);
      };
      window.Path2D = r;
    }
    if ("undefined" !== typeof CanvasPattern && Path2D.prototype.addPath) {
      t = function(a) {
        this._transform = a;
        this._template && (this._template._transform = a);
      };
      CanvasPattern.prototype.setTransform || (CanvasPattern.prototype.setTransform = t);
      CanvasGradient.prototype.setTransform || (CanvasGradient.prototype.setTransform = t);
      var d = CanvasRenderingContext2D.prototype.fill, q = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.fill = function(a, f) {
        var b = !!this.fillStyle._transform;
        if ((this.fillStyle instanceof CanvasPattern || this.fillStyle instanceof CanvasGradient) && b && a instanceof Path2D) {
          var b = this.fillStyle._transform, c;
          try {
            c = b.inverse();
          } catch (q) {
            c = b = m.Geometry.Matrix.createIdentitySVGMatrix();
          }
          this.transform(b.a, b.b, b.c, b.d, b.e, b.f);
          b = new Path2D;
          b.addPath(a, c);
          d.call(this, b, f);
          this.transform(c.a, c.b, c.c, c.d, c.e, c.f);
        } else {
          0 === arguments.length ? d.call(this) : 1 === arguments.length ? d.call(this, a) : 2 === arguments.length && d.call(this, a, f);
        }
      };
      CanvasRenderingContext2D.prototype.stroke = function(a) {
        var d = !!this.strokeStyle._transform;
        if ((this.strokeStyle instanceof CanvasPattern || this.strokeStyle instanceof CanvasGradient) && d && a instanceof Path2D) {
          var f = this.strokeStyle._transform, d = f.inverse();
          this.transform(f.a, f.b, f.c, f.d, f.e, f.f);
          f = new Path2D;
          f.addPath(a, d);
          var b = this.lineWidth;
          this.lineWidth *= (d.a + d.d) / 2;
          q.call(this, f);
          this.transform(d.a, d.b, d.c, d.d, d.e, d.f);
          this.lineWidth = b;
        } else {
          0 === arguments.length ? q.call(this) : 1 === arguments.length && q.call(this, a);
        }
      };
    }
    "undefined" !== typeof CanvasRenderingContext2D && function() {
      function a() {
        c(this.mozCurrentTransform);
        return m.Geometry.Matrix.createSVGMatrixFromArray(this.mozCurrentTransform);
      }
      CanvasRenderingContext2D.prototype.flashStroke = function(a, d) {
        var f = this.currentTransform;
        if (f) {
          var b = new Path2D;
          b.addPath(a, f);
          var c = this.lineWidth;
          this.setTransform(1, 0, 0, 1, 0, 0);
          switch(d) {
            case 1:
              this.lineWidth = w(c * (g.getScaleX(f) + g.getScaleY(f)) / 2, 1, 1024);
              break;
            case 2:
              this.lineWidth = w(c * g.getScaleY(f), 1, 1024);
              break;
            case 3:
              this.lineWidth = w(c * g.getScaleX(f), 1, 1024);
          }
          this.stroke(b);
          this.setTransform(f.a, f.b, f.c, f.d, f.e, f.f);
          this.lineWidth = c;
        } else {
          this.stroke(a);
        }
      };
      !("currentTransform" in CanvasRenderingContext2D.prototype) && "mozCurrentTransform" in CanvasRenderingContext2D.prototype && Object.defineProperty(CanvasRenderingContext2D.prototype, "currentTransform", {get:a});
    }();
    if ("undefined" !== typeof CanvasRenderingContext2D && void 0 === CanvasRenderingContext2D.prototype.globalColorMatrix) {
      var x = CanvasRenderingContext2D.prototype.fill, s = CanvasRenderingContext2D.prototype.stroke, O = CanvasRenderingContext2D.prototype.fillText, T = CanvasRenderingContext2D.prototype.strokeText;
      Object.defineProperty(CanvasRenderingContext2D.prototype, "globalColorMatrix", {get:function() {
        return this._globalColorMatrix ? this._globalColorMatrix.clone() : null;
      }, set:function(a) {
        a ? this._globalColorMatrix ? this._globalColorMatrix.set(a) : this._globalColorMatrix = a.clone() : this._globalColorMatrix = null;
      }, enumerable:!0, configurable:!0});
      CanvasRenderingContext2D.prototype.fill = function(a, d) {
        var f = null;
        this._globalColorMatrix && (f = this.fillStyle, this.fillStyle = e(this, this.fillStyle, this._globalColorMatrix));
        0 === arguments.length ? x.call(this) : 1 === arguments.length ? x.call(this, a) : 2 === arguments.length && x.call(this, a, d);
        f && (this.fillStyle = f);
      };
      CanvasRenderingContext2D.prototype.stroke = function(a, d) {
        var f = null;
        this._globalColorMatrix && (f = this.strokeStyle, this.strokeStyle = e(this, this.strokeStyle, this._globalColorMatrix));
        0 === arguments.length ? s.call(this) : 1 === arguments.length && s.call(this, a);
        f && (this.strokeStyle = f);
      };
      CanvasRenderingContext2D.prototype.fillText = function(a, d, f, b) {
        var c = null;
        this._globalColorMatrix && (c = this.fillStyle, this.fillStyle = e(this, this.fillStyle, this._globalColorMatrix));
        3 === arguments.length ? O.call(this, a, d, f) : 4 === arguments.length ? O.call(this, a, d, f, b) : g.Debug.unexpected();
        c && (this.fillStyle = c);
      };
      CanvasRenderingContext2D.prototype.strokeText = function(a, d, f, b) {
        var c = null;
        this._globalColorMatrix && (c = this.strokeStyle, this.strokeStyle = e(this, this.strokeStyle, this._globalColorMatrix));
        3 === arguments.length ? T.call(this, a, d, f) : 4 === arguments.length ? T.call(this, a, d, f, b) : g.Debug.unexpected();
        c && (this.strokeStyle = c);
      };
    }
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    g.ScreenShot = function() {
      return function(e, c, g) {
        this.dataURL = e;
        this.w = c;
        this.h = g;
      };
    }();
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  var m = g.Debug.assert, e = function() {
    function c() {
      this._count = 0;
      this._head = this._tail = null;
    }
    Object.defineProperty(c.prototype, "count", {get:function() {
      return this._count;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(c.prototype, "head", {get:function() {
      return this._head;
    }, enumerable:!0, configurable:!0});
    c.prototype._unshift = function(c) {
      m(!c.next && !c.previous);
      0 === this._count ? this._head = this._tail = c : (c.next = this._head, this._head = c.next.previous = c);
      this._count++;
    };
    c.prototype._remove = function(c) {
      m(0 < this._count);
      c === this._head && c === this._tail ? this._head = this._tail = null : c === this._head ? (this._head = c.next, this._head.previous = null) : c == this._tail ? (this._tail = c.previous, this._tail.next = null) : (c.previous.next = c.next, c.next.previous = c.previous);
      c.previous = c.next = null;
      this._count--;
    };
    c.prototype.use = function(c) {
      this._head !== c && ((c.next || c.previous || this._tail === c) && this._remove(c), this._unshift(c));
    };
    c.prototype.pop = function() {
      if (!this._tail) {
        return null;
      }
      var c = this._tail;
      this._remove(c);
      return c;
    };
    c.prototype.visit = function(c, e) {
      void 0 === e && (e = !0);
      for (var b = e ? this._head : this._tail;b && c(b);) {
        b = e ? b.next : b.previous;
      }
    };
    return c;
  }();
  g.LRUList = e;
  g.getScaleX = function(c) {
    return c.a;
  };
  g.getScaleY = function(c) {
    return c.d;
  };
})(Shumway || (Shumway = {}));
var Shumway$$inline_24 = Shumway || (Shumway = {}), GFX$$inline_25 = Shumway$$inline_24.GFX || (Shumway$$inline_24.GFX = {}), Option$$inline_26 = Shumway$$inline_24.Options.Option, OptionSet$$inline_27 = Shumway$$inline_24.Options.OptionSet, shumwayOptions$$inline_28 = Shumway$$inline_24.Settings.shumwayOptions, rendererOptions$$inline_29 = shumwayOptions$$inline_28.register(new OptionSet$$inline_27("Renderer Options"));
GFX$$inline_25.imageUpdateOption = rendererOptions$$inline_29.register(new Option$$inline_26("", "imageUpdate", "boolean", !0, "Enable image updating."));
GFX$$inline_25.imageConvertOption = rendererOptions$$inline_29.register(new Option$$inline_26("", "imageConvert", "boolean", !0, "Enable image conversion."));
GFX$$inline_25.stageOptions = shumwayOptions$$inline_28.register(new OptionSet$$inline_27("Stage Renderer Options"));
GFX$$inline_25.forcePaint = GFX$$inline_25.stageOptions.register(new Option$$inline_26("", "forcePaint", "boolean", !1, "Force repainting."));
GFX$$inline_25.ignoreViewport = GFX$$inline_25.stageOptions.register(new Option$$inline_26("", "ignoreViewport", "boolean", !1, "Cull elements outside of the viewport."));
GFX$$inline_25.viewportLoupeDiameter = GFX$$inline_25.stageOptions.register(new Option$$inline_26("", "viewportLoupeDiameter", "number", 256, "Size of the viewport loupe.", {range:{min:1, max:1024, step:1}}));
GFX$$inline_25.disableClipping = GFX$$inline_25.stageOptions.register(new Option$$inline_26("", "disableClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_25.debugClipping = GFX$$inline_25.stageOptions.register(new Option$$inline_26("", "debugClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_25.hud = GFX$$inline_25.stageOptions.register(new Option$$inline_26("", "hud", "boolean", !1, "Enable HUD."));
var webGLOptions$$inline_30 = GFX$$inline_25.stageOptions.register(new OptionSet$$inline_27("WebGL Options"));
GFX$$inline_25.perspectiveCamera = webGLOptions$$inline_30.register(new Option$$inline_26("", "pc", "boolean", !1, "Use perspective camera."));
GFX$$inline_25.perspectiveCameraFOV = webGLOptions$$inline_30.register(new Option$$inline_26("", "pcFOV", "number", 60, "Perspective Camera FOV."));
GFX$$inline_25.perspectiveCameraDistance = webGLOptions$$inline_30.register(new Option$$inline_26("", "pcDistance", "number", 2, "Perspective Camera Distance."));
GFX$$inline_25.perspectiveCameraAngle = webGLOptions$$inline_30.register(new Option$$inline_26("", "pcAngle", "number", 0, "Perspective Camera Angle."));
GFX$$inline_25.perspectiveCameraAngleRotate = webGLOptions$$inline_30.register(new Option$$inline_26("", "pcRotate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_25.perspectiveCameraSpacing = webGLOptions$$inline_30.register(new Option$$inline_26("", "pcSpacing", "number", .01, "Element Spacing."));
GFX$$inline_25.perspectiveCameraSpacingInflate = webGLOptions$$inline_30.register(new Option$$inline_26("", "pcInflate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_25.drawTiles = webGLOptions$$inline_30.register(new Option$$inline_26("", "drawTiles", "boolean", !1, "Draw WebGL Tiles"));
GFX$$inline_25.drawSurfaces = webGLOptions$$inline_30.register(new Option$$inline_26("", "drawSurfaces", "boolean", !1, "Draw WebGL Surfaces."));
GFX$$inline_25.drawSurface = webGLOptions$$inline_30.register(new Option$$inline_26("", "drawSurface", "number", -1, "Draw WebGL Surface #"));
GFX$$inline_25.drawElements = webGLOptions$$inline_30.register(new Option$$inline_26("", "drawElements", "boolean", !0, "Actually call gl.drawElements. This is useful to test if the GPU is the bottleneck."));
GFX$$inline_25.disableSurfaceUploads = webGLOptions$$inline_30.register(new Option$$inline_26("", "disableSurfaceUploads", "boolean", !1, "Disable surface uploads."));
GFX$$inline_25.premultipliedAlpha = webGLOptions$$inline_30.register(new Option$$inline_26("", "premultipliedAlpha", "boolean", !1, "Set the premultipliedAlpha flag on getContext()."));
GFX$$inline_25.unpackPremultiplyAlpha = webGLOptions$$inline_30.register(new Option$$inline_26("", "unpackPremultiplyAlpha", "boolean", !0, "Use UNPACK_PREMULTIPLY_ALPHA_WEBGL in pixelStorei."));
var factorChoices$$inline_31 = {ZERO:0, ONE:1, SRC_COLOR:768, ONE_MINUS_SRC_COLOR:769, DST_COLOR:774, ONE_MINUS_DST_COLOR:775, SRC_ALPHA:770, ONE_MINUS_SRC_ALPHA:771, DST_ALPHA:772, ONE_MINUS_DST_ALPHA:773, SRC_ALPHA_SATURATE:776, CONSTANT_COLOR:32769, ONE_MINUS_CONSTANT_COLOR:32770, CONSTANT_ALPHA:32771, ONE_MINUS_CONSTANT_ALPHA:32772};
GFX$$inline_25.sourceBlendFactor = webGLOptions$$inline_30.register(new Option$$inline_26("", "sourceBlendFactor", "number", factorChoices$$inline_31.ONE, "", {choices:factorChoices$$inline_31}));
GFX$$inline_25.destinationBlendFactor = webGLOptions$$inline_30.register(new Option$$inline_26("", "destinationBlendFactor", "number", factorChoices$$inline_31.ONE_MINUS_SRC_ALPHA, "", {choices:factorChoices$$inline_31}));
var canvas2DOptions$$inline_32 = GFX$$inline_25.stageOptions.register(new OptionSet$$inline_27("Canvas2D Options"));
GFX$$inline_25.clipDirtyRegions = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "clipDirtyRegions", "boolean", !1, "Clip dirty regions."));
GFX$$inline_25.clipCanvas = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "clipCanvas", "boolean", !1, "Clip Regions."));
GFX$$inline_25.cull = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "cull", "boolean", !1, "Enable culling."));
GFX$$inline_25.snapToDevicePixels = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "snapToDevicePixels", "boolean", !1, ""));
GFX$$inline_25.imageSmoothing = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "imageSmoothing", "boolean", !1, ""));
GFX$$inline_25.masking = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "masking", "boolean", !0, "Composite Mask."));
GFX$$inline_25.blending = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "blending", "boolean", !0, ""));
GFX$$inline_25.debugLayers = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "debugLayers", "boolean", !1, ""));
GFX$$inline_25.filters = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "filters", "boolean", !1, ""));
GFX$$inline_25.cacheShapes = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "cacheShapes", "boolean", !0, ""));
GFX$$inline_25.cacheShapesMaxSize = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "cacheShapesMaxSize", "number", 256, "", {range:{min:1, max:1024, step:1}}));
GFX$$inline_25.cacheShapesThreshold = canvas2DOptions$$inline_32.register(new Option$$inline_26("", "cacheShapesThreshold", "number", 256, "", {range:{min:1, max:1024, step:1}}));
(function(g) {
  (function(m) {
    (function(e) {
      function c(a, f, d, b) {
        var c = 1 - b;
        return a * c * c + 2 * f * c * b + d * b * b;
      }
      function w(a, f, d, b, c) {
        var s = c * c, e = 1 - c, l = e * e;
        return a * e * l + 3 * f * c * l + 3 * d * e * s + b * c * s;
      }
      var k = g.NumberUtilities.clamp, b = g.NumberUtilities.pow2, a = g.NumberUtilities.epsilonEquals, n = g.Debug.assert;
      e.radianToDegrees = function(a) {
        return 180 * a / Math.PI;
      };
      e.degreesToRadian = function(a) {
        return a * Math.PI / 180;
      };
      e.quadraticBezier = c;
      e.quadraticBezierExtreme = function(a, f, d) {
        var b = (a - f) / (a - 2 * f + d);
        return 0 > b ? a : 1 < b ? d : c(a, f, d, b);
      };
      e.cubicBezier = w;
      e.cubicBezierExtremes = function(a, f, d, b) {
        var c = f - a, s;
        s = 2 * (d - f);
        var e = b - d;
        c + e === s && (e *= 1.0001);
        var l = 2 * c - s, g = s - 2 * c, g = Math.sqrt(g * g - 4 * c * (c - s + e));
        s = 2 * (c - s + e);
        c = (l + g) / s;
        l = (l - g) / s;
        g = [];
        0 <= c && 1 >= c && g.push(w(a, f, d, b, c));
        0 <= l && 1 >= l && g.push(w(a, f, d, b, l));
        return g;
      };
      var p = function() {
        function a(f, d) {
          this.x = f;
          this.y = d;
        }
        a.prototype.setElements = function(a, d) {
          this.x = a;
          this.y = d;
          return this;
        };
        a.prototype.set = function(a) {
          this.x = a.x;
          this.y = a.y;
          return this;
        };
        a.prototype.dot = function(a) {
          return this.x * a.x + this.y * a.y;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.distanceTo = function(a) {
          return Math.sqrt(this.dot(a));
        };
        a.prototype.sub = function(a) {
          this.x -= a.x;
          this.y -= a.y;
          return this;
        };
        a.prototype.mul = function(a) {
          this.x *= a;
          this.y *= a;
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y);
        };
        a.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          return "{x: " + this.x.toFixed(a) + ", y: " + this.y.toFixed(a) + "}";
        };
        a.prototype.inTriangle = function(a, d, b) {
          var c = a.y * b.x - a.x * b.y + (b.y - a.y) * this.x + (a.x - b.x) * this.y, h = a.x * d.y - a.y * d.x + (a.y - d.y) * this.x + (d.x - a.x) * this.y;
          if (0 > c != 0 > h) {
            return!1;
          }
          a = -d.y * b.x + a.y * (b.x - d.x) + a.x * (d.y - b.y) + d.x * b.y;
          0 > a && (c = -c, h = -h, a = -a);
          return 0 < c && 0 < h && c + h < a;
        };
        a.createEmpty = function() {
          return new a(0, 0);
        };
        a.createEmptyPoints = function(b) {
          for (var d = [], c = 0;c < b;c++) {
            d.push(new a(0, 0));
          }
          return d;
        };
        return a;
      }();
      e.Point = p;
      var y = function() {
        function a(b, d, c) {
          this.x = b;
          this.y = d;
          this.z = c;
        }
        a.prototype.setElements = function(a, d, b) {
          this.x = a;
          this.y = d;
          this.z = b;
          return this;
        };
        a.prototype.set = function(a) {
          this.x = a.x;
          this.y = a.y;
          this.z = a.z;
          return this;
        };
        a.prototype.dot = function(a) {
          return this.x * a.x + this.y * a.y + this.z * a.z;
        };
        a.prototype.cross = function(a) {
          var d = this.z * a.x - this.x * a.z, b = this.x * a.y - this.y * a.x;
          this.x = this.y * a.z - this.z * a.y;
          this.y = d;
          this.z = b;
          return this;
        };
        a.prototype.squaredLength = function() {
          return this.dot(this);
        };
        a.prototype.sub = function(a) {
          this.x -= a.x;
          this.y -= a.y;
          this.z -= a.z;
          return this;
        };
        a.prototype.mul = function(a) {
          this.x *= a;
          this.y *= a;
          this.z *= a;
          return this;
        };
        a.prototype.normalize = function() {
          var a = Math.sqrt(this.squaredLength());
          1E-5 < a ? this.mul(1 / a) : this.setElements(0, 0, 0);
          return this;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y, this.z);
        };
        a.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          return "{x: " + this.x.toFixed(a) + ", y: " + this.y.toFixed(a) + ", z: " + this.z.toFixed(a) + "}";
        };
        a.createEmpty = function() {
          return new a(0, 0, 0);
        };
        a.createEmptyPoints = function(b) {
          for (var d = [], c = 0;c < b;c++) {
            d.push(new a(0, 0, 0));
          }
          return d;
        };
        return a;
      }();
      e.Point3D = y;
      var v = function() {
        function a(b, d, c, x) {
          this.setElements(b, d, c, x);
          a.allocationCount++;
        }
        a.prototype.setElements = function(a, d, b, c) {
          this.x = a;
          this.y = d;
          this.w = b;
          this.h = c;
        };
        a.prototype.set = function(a) {
          this.x = a.x;
          this.y = a.y;
          this.w = a.w;
          this.h = a.h;
        };
        a.prototype.contains = function(a) {
          var d = a.x + a.w, b = a.y + a.h, c = this.x + this.w, h = this.y + this.h;
          return a.x >= this.x && a.x < c && a.y >= this.y && a.y < h && d > this.x && d <= c && b > this.y && b <= h;
        };
        a.prototype.containsPoint = function(a) {
          return a.x >= this.x && a.x < this.x + this.w && a.y >= this.y && a.y < this.y + this.h;
        };
        a.prototype.isContained = function(a) {
          for (var d = 0;d < a.length;d++) {
            if (a[d].contains(this)) {
              return!0;
            }
          }
          return!1;
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
              var d = this.x, b = this.y;
              this.x > a.x && (d = a.x);
              this.y > a.y && (b = a.y);
              var c = this.x + this.w;
              c < a.x + a.w && (c = a.x + a.w);
              var h = this.y + this.h;
              h < a.y + a.h && (h = a.y + a.h);
              this.x = d;
              this.y = b;
              this.w = c - d;
              this.h = h - b;
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
          var d = a.createEmpty();
          if (this.isEmpty() || b.isEmpty()) {
            return d.setEmpty(), d;
          }
          d.x = Math.max(this.x, b.x);
          d.y = Math.max(this.y, b.y);
          d.w = Math.min(this.x + this.w, b.x + b.w) - d.x;
          d.h = Math.min(this.y + this.h, b.y + b.h) - d.y;
          d.isEmpty() && d.setEmpty();
          this.set(d);
        };
        a.prototype.intersects = function(a) {
          if (this.isEmpty() || a.isEmpty()) {
            return!1;
          }
          var d = Math.max(this.x, a.x), b = Math.max(this.y, a.y), d = Math.min(this.x + this.w, a.x + a.w) - d;
          a = Math.min(this.y + this.h, a.y + a.h) - b;
          return!(0 >= d || 0 >= a);
        };
        a.prototype.intersectsTransformedAABB = function(b, d) {
          var c = a._temporary;
          c.set(b);
          d.transformRectangleAABB(c);
          return this.intersects(c);
        };
        a.prototype.intersectsTranslated = function(a, d, b) {
          if (this.isEmpty() || a.isEmpty()) {
            return!1;
          }
          var c = Math.max(this.x, a.x + d), h = Math.max(this.y, a.y + b);
          d = Math.min(this.x + this.w, a.x + d + a.w) - c;
          a = Math.min(this.y + this.h, a.y + b + a.h) - h;
          return!(0 >= d || 0 >= a);
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
          var a = Math.ceil(this.x + this.w), d = Math.ceil(this.y + this.h);
          this.x = Math.floor(this.x);
          this.y = Math.floor(this.y);
          this.w = a - this.x;
          this.h = d - this.y;
          return this;
        };
        a.prototype.scale = function(a, d) {
          this.x *= a;
          this.y *= d;
          this.w *= a;
          this.h *= d;
          return this;
        };
        a.prototype.offset = function(a, d) {
          this.x += a;
          this.y += d;
          return this;
        };
        a.prototype.resize = function(a, d) {
          this.w += a;
          this.h += d;
          return this;
        };
        a.prototype.expand = function(a, d) {
          this.offset(-a, -d).resize(2 * a, 2 * d);
          return this;
        };
        a.prototype.getCenter = function() {
          return new p(this.x + this.w / 2, this.y + this.h / 2);
        };
        a.prototype.getAbsoluteBounds = function() {
          return new a(0, 0, this.w, this.h);
        };
        a.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          return "{" + this.x.toFixed(a) + ", " + this.y.toFixed(a) + ", " + this.w.toFixed(a) + ", " + this.h.toFixed(a) + "}";
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
      e.Rectangle = v;
      var l = function() {
        function a(b) {
          this.corners = b.map(function(a) {
            return a.clone();
          });
          this.axes = [b[1].clone().sub(b[0]), b[3].clone().sub(b[0])];
          this.origins = [];
          for (var d = 0;2 > d;d++) {
            this.axes[d].mul(1 / this.axes[d].squaredLength()), this.origins.push(b[0].dot(this.axes[d]));
          }
        }
        a.prototype.getBounds = function() {
          return a.getBounds(this.corners);
        };
        a.getBounds = function(a) {
          for (var d = new p(Number.MAX_VALUE, Number.MAX_VALUE), b = new p(Number.MIN_VALUE, Number.MIN_VALUE), c = 0;4 > c;c++) {
            var h = a[c].x, e = a[c].y;
            d.x = Math.min(d.x, h);
            d.y = Math.min(d.y, e);
            b.x = Math.max(b.x, h);
            b.y = Math.max(b.y, e);
          }
          return new v(d.x, d.y, b.x - d.x, b.y - d.y);
        };
        a.prototype.intersects = function(a) {
          return this.intersectsOneWay(a) && a.intersectsOneWay(this);
        };
        a.prototype.intersectsOneWay = function(a) {
          for (var d = 0;2 > d;d++) {
            for (var b = 0;4 > b;b++) {
              var c = a.corners[b].dot(this.axes[d]), h, e;
              0 === b ? e = h = c : c < h ? h = c : c > e && (e = c);
            }
            if (h > 1 + this.origins[d] || e < this.origins[d]) {
              return!1;
            }
          }
          return!0;
        };
        return a;
      }();
      e.OBB = l;
      (function(a) {
        a[a.Unknown = 0] = "Unknown";
        a[a.Identity = 1] = "Identity";
        a[a.Translation = 2] = "Translation";
      })(e.MatrixType || (e.MatrixType = {}));
      var t = function() {
        function b(a, d, c, x, s, e) {
          this._data = new Float64Array(6);
          this._type = 0;
          this.setElements(a, d, c, x, s, e);
          b.allocationCount++;
        }
        Object.defineProperty(b.prototype, "a", {get:function() {
          return this._data[0];
        }, set:function(a) {
          this._data[0] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "b", {get:function() {
          return this._data[1];
        }, set:function(a) {
          this._data[1] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "c", {get:function() {
          return this._data[2];
        }, set:function(a) {
          this._data[2] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "d", {get:function() {
          return this._data[3];
        }, set:function(a) {
          this._data[3] = a;
          this._type = 0;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "tx", {get:function() {
          return this._data[4];
        }, set:function(a) {
          this._data[4] = a;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(b.prototype, "ty", {get:function() {
          return this._data[5];
        }, set:function(a) {
          this._data[5] = a;
          1 === this._type && (this._type = 2);
        }, enumerable:!0, configurable:!0});
        b.prototype.setElements = function(a, d, b, c, h, e) {
          var l = this._data;
          l[0] = a;
          l[1] = d;
          l[2] = b;
          l[3] = c;
          l[4] = h;
          l[5] = e;
          this._type = 0;
        };
        b.prototype.set = function(a) {
          var d = this._data, b = a._data;
          d[0] = b[0];
          d[1] = b[1];
          d[2] = b[2];
          d[3] = b[3];
          d[4] = b[4];
          d[5] = b[5];
          this._type = a._type;
        };
        b.prototype.emptyArea = function(a) {
          a = this._data;
          return 0 === a[0] || 0 === a[3] ? !0 : !1;
        };
        b.prototype.infiniteArea = function(a) {
          a = this._data;
          return Infinity === Math.abs(a[0]) || Infinity === Math.abs(a[3]) ? !0 : !1;
        };
        b.prototype.isEqual = function(a) {
          if (1 === this._type && 1 === a._type) {
            return!0;
          }
          var d = this._data;
          a = a._data;
          return d[0] === a[0] && d[1] === a[1] && d[2] === a[2] && d[3] === a[3] && d[4] === a[4] && d[5] === a[5];
        };
        b.prototype.clone = function() {
          var a = b.allocate();
          a.set(this);
          return a;
        };
        b.allocate = function() {
          var a = b._dirtyStack;
          return a.length ? a.pop() : new b(12345, 12345, 12345, 12345, 12345, 12345);
        };
        b.prototype.free = function() {
          b._dirtyStack.push(this);
        };
        b.prototype.transform = function(a, d, b, c, h, e) {
          var l = this._data, g = l[0], n = l[1], k = l[2], p = l[3], r = l[4], t = l[5];
          l[0] = g * a + k * d;
          l[1] = n * a + p * d;
          l[2] = g * b + k * c;
          l[3] = n * b + p * c;
          l[4] = g * h + k * e + r;
          l[5] = n * h + p * e + t;
          this._type = 0;
          return this;
        };
        b.prototype.transformRectangle = function(a, d) {
          n(4 === d.length);
          var b = this._data, c = b[0], h = b[1], e = b[2], l = b[3], g = b[4], b = b[5], k = a.x, p = a.y, r = a.w, t = a.h;
          d[0].x = c * k + e * p + g;
          d[0].y = h * k + l * p + b;
          d[1].x = c * (k + r) + e * p + g;
          d[1].y = h * (k + r) + l * p + b;
          d[2].x = c * (k + r) + e * (p + t) + g;
          d[2].y = h * (k + r) + l * (p + t) + b;
          d[3].x = c * k + e * (p + t) + g;
          d[3].y = h * k + l * (p + t) + b;
        };
        b.prototype.isTranslationOnly = function() {
          if (2 === this._type) {
            return!0;
          }
          var b = this._data;
          return 1 === b[0] && 0 === b[1] && 0 === b[2] && 1 === b[3] || a(b[0], 1) && a(b[1], 0) && a(b[2], 0) && a(b[3], 1) ? (this._type = 2, !0) : !1;
        };
        b.prototype.transformRectangleAABB = function(a) {
          var d = this._data;
          if (1 !== this._type) {
            if (2 === this._type) {
              a.x += d[4], a.y += d[5];
            } else {
              var b = d[0], c = d[1], h = d[2], e = d[3], l = d[4], g = d[5], n = a.x, k = a.y, p = a.w, r = a.h, d = b * n + h * k + l, t = c * n + e * k + g, m = b * (n + p) + h * k + l, v = c * (n + p) + e * k + g, u = b * (n + p) + h * (k + r) + l, p = c * (n + p) + e * (k + r) + g, b = b * n + h * (k + r) + l, c = c * n + e * (k + r) + g, e = 0;
              d > m && (e = d, d = m, m = e);
              u > b && (e = u, u = b, b = e);
              a.x = d < u ? d : u;
              a.w = (m > b ? m : b) - a.x;
              t > v && (e = t, t = v, v = e);
              p > c && (e = p, p = c, c = e);
              a.y = t < p ? t : p;
              a.h = (v > c ? v : c) - a.y;
            }
          }
        };
        b.prototype.scale = function(a, d) {
          var b = this._data;
          b[0] *= a;
          b[1] *= d;
          b[2] *= a;
          b[3] *= d;
          b[4] *= a;
          b[5] *= d;
          this._type = 0;
          return this;
        };
        b.prototype.scaleClone = function(a, d) {
          return 1 === a && 1 === d ? this : this.clone().scale(a, d);
        };
        b.prototype.rotate = function(a) {
          var d = this._data, b = d[0], c = d[1], h = d[2], e = d[3], l = d[4], g = d[5], n = Math.cos(a);
          a = Math.sin(a);
          d[0] = n * b - a * c;
          d[1] = a * b + n * c;
          d[2] = n * h - a * e;
          d[3] = a * h + n * e;
          d[4] = n * l - a * g;
          d[5] = a * l + n * g;
          this._type = 0;
          return this;
        };
        b.prototype.concat = function(a) {
          if (1 === a._type) {
            return this;
          }
          var d = this._data;
          a = a._data;
          var b = d[0] * a[0], c = 0, h = 0, e = d[3] * a[3], l = d[4] * a[0] + a[4], g = d[5] * a[3] + a[5];
          if (0 !== d[1] || 0 !== d[2] || 0 !== a[1] || 0 !== a[2]) {
            b += d[1] * a[2], e += d[2] * a[1], c += d[0] * a[1] + d[1] * a[3], h += d[2] * a[0] + d[3] * a[2], l += d[5] * a[2], g += d[4] * a[1];
          }
          d[0] = b;
          d[1] = c;
          d[2] = h;
          d[3] = e;
          d[4] = l;
          d[5] = g;
          this._type = 0;
          return this;
        };
        b.prototype.concatClone = function(a) {
          return this.clone().concat(a);
        };
        b.prototype.preMultiply = function(a) {
          var d = this._data, b = a._data;
          if (2 === a._type && this._type & 3) {
            d[4] += b[4], d[5] += b[5], this._type = 2;
          } else {
            if (1 !== a._type) {
              a = b[0] * d[0];
              var c = 0, h = 0, e = b[3] * d[3], l = b[4] * d[0] + d[4], g = b[5] * d[3] + d[5];
              if (0 !== b[1] || 0 !== b[2] || 0 !== d[1] || 0 !== d[2]) {
                a += b[1] * d[2], e += b[2] * d[1], c += b[0] * d[1] + b[1] * d[3], h += b[2] * d[0] + b[3] * d[2], l += b[5] * d[2], g += b[4] * d[1];
              }
              d[0] = a;
              d[1] = c;
              d[2] = h;
              d[3] = e;
              d[4] = l;
              d[5] = g;
              this._type = 0;
            }
          }
        };
        b.prototype.translate = function(a, d) {
          var b = this._data;
          b[4] += a;
          b[5] += d;
          1 === this._type && (this._type = 2);
          return this;
        };
        b.prototype.setIdentity = function() {
          var a = this._data;
          a[0] = 1;
          a[1] = 0;
          a[2] = 0;
          a[3] = 1;
          a[4] = 0;
          a[5] = 0;
          this._type = 1;
        };
        b.prototype.isIdentity = function() {
          if (1 === this._type) {
            return!0;
          }
          var a = this._data;
          return 1 === a[0] && 0 === a[1] && 0 === a[2] && 1 === a[3] && 0 === a[4] && 0 === a[5];
        };
        b.prototype.transformPoint = function(a) {
          if (1 !== this._type) {
            var d = this._data, b = a.x, c = a.y;
            a.x = d[0] * b + d[2] * c + d[4];
            a.y = d[1] * b + d[3] * c + d[5];
          }
        };
        b.prototype.transformPoints = function(a) {
          if (1 !== this._type) {
            for (var d = 0;d < a.length;d++) {
              this.transformPoint(a[d]);
            }
          }
        };
        b.prototype.deltaTransformPoint = function(a) {
          if (1 !== this._type) {
            var d = this._data, b = a.x, c = a.y;
            a.x = d[0] * b + d[2] * c;
            a.y = d[1] * b + d[3] * c;
          }
        };
        b.prototype.inverse = function(a) {
          var d = this._data, b = a._data;
          if (1 === this._type) {
            a.setIdentity();
          } else {
            if (2 === this._type) {
              b[0] = 1, b[1] = 0, b[2] = 0, b[3] = 1, b[4] = -d[4], b[5] = -d[5], a._type = 2;
            } else {
              var c = d[1], h = d[2], e = d[4], l = d[5];
              if (0 === c && 0 === h) {
                var g = b[0] = 1 / d[0], d = b[3] = 1 / d[3];
                b[1] = 0;
                b[2] = 0;
                b[4] = -g * e;
                b[5] = -d * l;
              } else {
                var g = d[0], d = d[3], n = g * d - c * h;
                if (0 === n) {
                  a.setIdentity();
                  return;
                }
                n = 1 / n;
                b[0] = d * n;
                c = b[1] = -c * n;
                h = b[2] = -h * n;
                d = b[3] = g * n;
                b[4] = -(b[0] * e + h * l);
                b[5] = -(c * e + d * l);
              }
              a._type = 0;
            }
          }
        };
        b.prototype.getTranslateX = function() {
          return this._data[4];
        };
        b.prototype.getTranslateY = function() {
          return this._data[4];
        };
        b.prototype.getScaleX = function() {
          var a = this._data;
          if (1 === a[0] && 0 === a[1]) {
            return 1;
          }
          var d = Math.sqrt(a[0] * a[0] + a[1] * a[1]);
          return 0 < a[0] ? d : -d;
        };
        b.prototype.getScaleY = function() {
          var a = this._data;
          if (0 === a[2] && 1 === a[3]) {
            return 1;
          }
          var d = Math.sqrt(a[2] * a[2] + a[3] * a[3]);
          return 0 < a[3] ? d : -d;
        };
        b.prototype.getScale = function() {
          return(this.getScaleX() + this.getScaleY()) / 2;
        };
        b.prototype.getAbsoluteScaleX = function() {
          return Math.abs(this.getScaleX());
        };
        b.prototype.getAbsoluteScaleY = function() {
          return Math.abs(this.getScaleY());
        };
        b.prototype.getRotation = function() {
          var a = this._data;
          return 180 * Math.atan(a[1] / a[0]) / Math.PI;
        };
        b.prototype.isScaleOrRotation = function() {
          var a = this._data;
          return.01 > Math.abs(a[0] * a[2] + a[1] * a[3]);
        };
        b.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          var d = this._data;
          return "{" + d[0].toFixed(a) + ", " + d[1].toFixed(a) + ", " + d[2].toFixed(a) + ", " + d[3].toFixed(a) + ", " + d[4].toFixed(a) + ", " + d[5].toFixed(a) + "}";
        };
        b.prototype.toWebGLMatrix = function() {
          var a = this._data;
          return new Float32Array([a[0], a[1], 0, a[2], a[3], 0, a[4], a[5], 1]);
        };
        b.prototype.toCSSTransform = function() {
          var a = this._data;
          return "matrix(" + a[0] + ", " + a[1] + ", " + a[2] + ", " + a[3] + ", " + a[4] + ", " + a[5] + ")";
        };
        b.createIdentity = function() {
          var a = b.allocate();
          a.setIdentity();
          return a;
        };
        b.prototype.toSVGMatrix = function() {
          var a = this._data, d = b._svg.createSVGMatrix();
          d.a = a[0];
          d.b = a[1];
          d.c = a[2];
          d.d = a[3];
          d.e = a[4];
          d.f = a[5];
          return d;
        };
        b.prototype.snap = function() {
          var a = this._data;
          return this.isTranslationOnly() ? (a[0] = 1, a[1] = 0, a[2] = 0, a[3] = 1, a[4] = Math.round(a[4]), a[5] = Math.round(a[5]), this._type = 2, !0) : !1;
        };
        b.createIdentitySVGMatrix = function() {
          return b._svg.createSVGMatrix();
        };
        b.createSVGMatrixFromArray = function(a) {
          var d = b._svg.createSVGMatrix();
          d.a = a[0];
          d.b = a[1];
          d.c = a[2];
          d.d = a[3];
          d.e = a[4];
          d.f = a[5];
          return d;
        };
        b.allocationCount = 0;
        b._dirtyStack = [];
        b._svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
        b.multiply = function(a, d) {
          var b = d._data;
          a.transform(b[0], b[1], b[2], b[3], b[4], b[5]);
        };
        return b;
      }();
      e.Matrix = t;
      t = function() {
        function a(b) {
          this._m = new Float32Array(b);
        }
        a.prototype.asWebGLMatrix = function() {
          return this._m;
        };
        a.createCameraLookAt = function(b, d, c) {
          d = b.clone().sub(d).normalize();
          c = c.clone().cross(d).normalize();
          var e = d.clone().cross(c);
          return new a([c.x, c.y, c.z, 0, e.x, e.y, e.z, 0, d.x, d.y, d.z, 0, b.x, b.y, b.z, 1]);
        };
        a.createLookAt = function(b, d, c) {
          d = b.clone().sub(d).normalize();
          c = c.clone().cross(d).normalize();
          var e = d.clone().cross(c);
          return new a([c.x, e.x, d.x, 0, e.x, e.y, d.y, 0, d.x, e.z, d.z, 0, -c.dot(b), -e.dot(b), -d.dot(b), 1]);
        };
        a.prototype.mul = function(a) {
          a = [a.x, a.y, a.z, 0];
          for (var b = this._m, c = [], h = 0;4 > h;h++) {
            c[h] = 0;
            for (var s = 4 * h, e = 0;4 > e;e++) {
              c[h] += b[s + e] * a[e];
            }
          }
          return new y(c[0], c[1], c[2]);
        };
        a.create2DProjection = function(b, d, c) {
          return new a([2 / b, 0, 0, 0, 0, -2 / d, 0, 0, 0, 0, 2 / c, 0, -1, 1, 0, 1]);
        };
        a.createPerspective = function(b) {
          b = Math.tan(.5 * Math.PI - .5 * b);
          var d = 1 / -4999.9;
          return new a([b / 1, 0, 0, 0, 0, b, 0, 0, 0, 0, 5000.1 * d, -1, 0, 0, 1E3 * d, 0]);
        };
        a.createIdentity = function() {
          return a.createTranslation(0, 0);
        };
        a.createTranslation = function(b, d) {
          return new a([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, b, d, 0, 1]);
        };
        a.createXRotation = function(b) {
          var d = Math.cos(b);
          b = Math.sin(b);
          return new a([1, 0, 0, 0, 0, d, b, 0, 0, -b, d, 0, 0, 0, 0, 1]);
        };
        a.createYRotation = function(b) {
          var d = Math.cos(b);
          b = Math.sin(b);
          return new a([d, 0, -b, 0, 0, 1, 0, 0, b, 0, d, 0, 0, 0, 0, 1]);
        };
        a.createZRotation = function(b) {
          var d = Math.cos(b);
          b = Math.sin(b);
          return new a([d, b, 0, 0, -b, d, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
        };
        a.createScale = function(b, d, c) {
          return new a([b, 0, 0, 0, 0, d, 0, 0, 0, 0, c, 0, 0, 0, 0, 1]);
        };
        a.createMultiply = function(b, d) {
          var c = b._m, e = d._m, s = c[0], l = c[1], g = c[2], n = c[3], k = c[4], p = c[5], r = c[6], t = c[7], m = c[8], v = c[9], u = c[10], y = c[11], w = c[12], A = c[13], C = c[14], c = c[15], D = e[0], z = e[1], E = e[2], G = e[3], F = e[4], H = e[5], I = e[6], J = e[7], K = e[8], L = e[9], M = e[10], N = e[11], Q = e[12], R = e[13], S = e[14], e = e[15];
          return new a([s * D + l * F + g * K + n * Q, s * z + l * H + g * L + n * R, s * E + l * I + g * M + n * S, s * G + l * J + g * N + n * e, k * D + p * F + r * K + t * Q, k * z + p * H + r * L + t * R, k * E + p * I + r * M + t * S, k * G + p * J + r * N + t * e, m * D + v * F + u * K + y * Q, m * z + v * H + u * L + y * R, m * E + v * I + u * M + y * S, m * G + v * J + u * N + y * e, w * D + A * F + C * K + c * Q, w * z + A * H + C * L + c * R, w * E + A * I + C * M + c * S, w * G + A * 
          J + C * N + c * e]);
        };
        a.createInverse = function(b) {
          var d = b._m;
          b = d[0];
          var c = d[1], e = d[2], s = d[3], l = d[4], g = d[5], n = d[6], k = d[7], p = d[8], r = d[9], t = d[10], m = d[11], v = d[12], u = d[13], y = d[14], d = d[15], w = t * d, A = y * m, C = n * d, D = y * k, z = n * m, E = t * k, G = e * d, F = y * s, H = e * m, I = t * s, J = e * k, K = n * s, L = p * u, M = v * r, N = l * u, Q = v * g, R = l * r, S = p * g, Y = b * u, Z = v * c, $ = b * r, aa = p * c, ba = b * g, ca = l * c, da = w * g + D * r + z * u - (A * g + C * r + E * u), ea = A * c + 
          G * r + I * u - (w * c + F * r + H * u), u = C * c + F * g + J * u - (D * c + G * g + K * u), c = E * c + H * g + K * r - (z * c + I * g + J * r), g = 1 / (b * da + l * ea + p * u + v * c);
          return new a([g * da, g * ea, g * u, g * c, g * (A * l + C * p + E * v - (w * l + D * p + z * v)), g * (w * b + F * p + H * v - (A * b + G * p + I * v)), g * (D * b + G * l + K * v - (C * b + F * l + J * v)), g * (z * b + I * l + J * p - (E * b + H * l + K * p)), g * (L * k + Q * m + R * d - (M * k + N * m + S * d)), g * (M * s + Y * m + aa * d - (L * s + Z * m + $ * d)), g * (N * s + Z * k + ba * d - (Q * s + Y * k + ca * d)), g * (S * s + $ * k + ca * m - (R * s + aa * k + ba * m)), g * 
          (N * t + S * y + M * n - (R * y + L * n + Q * t)), g * ($ * y + L * e + Z * t - (Y * t + aa * y + M * e)), g * (Y * n + ca * y + Q * e - (ba * y + N * e + Z * n)), g * (ba * t + R * e + aa * n - ($ * n + ca * t + S * e))]);
        };
        return a;
      }();
      e.Matrix3D = t;
      t = function() {
        function a(b, d, c) {
          void 0 === c && (c = 7);
          var e = this.size = 1 << c;
          this.sizeInBits = c;
          this.w = b;
          this.h = d;
          this.c = Math.ceil(b / e);
          this.r = Math.ceil(d / e);
          this.grid = [];
          for (b = 0;b < this.r;b++) {
            for (this.grid.push([]), d = 0;d < this.c;d++) {
              this.grid[b][d] = new a.Cell(new v(d * e, b * e, e, e));
            }
          }
        }
        a.prototype.clear = function() {
          for (var a = 0;a < this.r;a++) {
            for (var b = 0;b < this.c;b++) {
              this.grid[a][b].clear();
            }
          }
        };
        a.prototype.getBounds = function() {
          return new v(0, 0, this.w, this.h);
        };
        a.prototype.addDirtyRectangle = function(a) {
          var b = a.x >> this.sizeInBits, c = a.y >> this.sizeInBits;
          if (!(b >= this.c || c >= this.r)) {
            0 > b && (b = 0);
            0 > c && (c = 0);
            var h = this.grid[c][b];
            a = a.clone();
            a.snap();
            if (h.region.contains(a)) {
              h.bounds.isEmpty() ? h.bounds.set(a) : h.bounds.contains(a) || h.bounds.union(a);
            } else {
              for (var e = Math.min(this.c, Math.ceil((a.x + a.w) / this.size)) - b, l = Math.min(this.r, Math.ceil((a.y + a.h) / this.size)) - c, g = 0;g < e;g++) {
                for (var n = 0;n < l;n++) {
                  h = this.grid[c + n][b + g], h = h.region.clone(), h.intersect(a), h.isEmpty() || this.addDirtyRectangle(h);
                }
              }
            }
          }
        };
        a.prototype.gatherRegions = function(a) {
          for (var b = 0;b < this.r;b++) {
            for (var c = 0;c < this.c;c++) {
              this.grid[b][c].bounds.isEmpty() || a.push(this.grid[b][c].bounds);
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
          for (var b = 0, c = 0;c < this.r;c++) {
            for (var h = 0;h < this.c;h++) {
              b += this.grid[c][h].region.area();
            }
          }
          return b / a;
        };
        a.prototype.render = function(a, b) {
          function c(b) {
            a.rect(b.x, b.y, b.w, b.h);
          }
          if (b && b.drawGrid) {
            a.strokeStyle = "white";
            for (var h = 0;h < this.r;h++) {
              for (var e = 0;e < this.c;e++) {
                var g = this.grid[h][e];
                a.beginPath();
                c(g.region);
                a.closePath();
                a.stroke();
              }
            }
          }
          a.strokeStyle = "#E0F8D8";
          for (h = 0;h < this.r;h++) {
            for (e = 0;e < this.c;e++) {
              g = this.grid[h][e], a.beginPath(), c(g.bounds), a.closePath(), a.stroke();
            }
          }
        };
        a.tmpRectangle = v.createEmpty();
        return a;
      }();
      e.DirtyRegion = t;
      (function(a) {
        var b = function() {
          function a(b) {
            this.region = b;
            this.bounds = v.createEmpty();
          }
          a.prototype.clear = function() {
            this.bounds.setEmpty();
          };
          return a;
        }();
        a.Cell = b;
      })(t = e.DirtyRegion || (e.DirtyRegion = {}));
      var r = function() {
        function a(b, d, c, h, e, g) {
          this.index = b;
          this.x = d;
          this.y = c;
          this.scale = g;
          this.bounds = new v(d * h, c * e, h, e);
        }
        a.prototype.getOBB = function() {
          if (this._obb) {
            return this._obb;
          }
          this.bounds.getCorners(a.corners);
          return this._obb = new l(a.corners);
        };
        a.corners = p.createEmptyPoints(4);
        return a;
      }();
      e.Tile = r;
      var u = function() {
        function a(b, d, c, h, e) {
          this.tileW = c;
          this.tileH = h;
          this.scale = e;
          this.w = b;
          this.h = d;
          this.rows = Math.ceil(d / h);
          this.columns = Math.ceil(b / c);
          n(2048 > this.rows && 2048 > this.columns);
          this.tiles = [];
          for (d = b = 0;d < this.rows;d++) {
            for (var g = 0;g < this.columns;g++) {
              this.tiles.push(new r(b++, g, d, c, h, e));
            }
          }
        }
        a.prototype.getTiles = function(a, b) {
          if (b.emptyArea(a)) {
            return[];
          }
          if (b.infiniteArea(a)) {
            return this.tiles;
          }
          var c = this.columns * this.rows;
          return 40 > c && b.isScaleOrRotation() ? this.getFewTiles(a, b, 10 < c) : this.getManyTiles(a, b);
        };
        a.prototype.getFewTiles = function(b, d, c) {
          void 0 === c && (c = !0);
          if (d.isTranslationOnly() && 1 === this.tiles.length) {
            return this.tiles[0].bounds.intersectsTranslated(b, d.tx, d.ty) ? [this.tiles[0]] : [];
          }
          d.transformRectangle(b, a._points);
          var e;
          b = new v(0, 0, this.w, this.h);
          c && (e = new l(a._points));
          b.intersect(l.getBounds(a._points));
          if (b.isEmpty()) {
            return[];
          }
          var s = b.x / this.tileW | 0;
          d = b.y / this.tileH | 0;
          var g = Math.ceil((b.x + b.w) / this.tileW) | 0, n = Math.ceil((b.y + b.h) / this.tileH) | 0, s = k(s, 0, this.columns), g = k(g, 0, this.columns);
          d = k(d, 0, this.rows);
          for (var n = k(n, 0, this.rows), p = [];s < g;s++) {
            for (var r = d;r < n;r++) {
              var t = this.tiles[r * this.columns + s];
              t.bounds.intersects(b) && (c ? t.getOBB().intersects(e) : 1) && p.push(t);
            }
          }
          return p;
        };
        a.prototype.getManyTiles = function(b, d) {
          function c(a, b, d) {
            return(a - b.x) * (d.y - b.y) / (d.x - b.x) + b.y;
          }
          function e(a, b, d, c, f) {
            if (!(0 > d || d >= b.columns)) {
              for (c = k(c, 0, b.rows), f = k(f + 1, 0, b.rows);c < f;c++) {
                a.push(b.tiles[c * b.columns + d]);
              }
            }
          }
          var s = a._points;
          d.transformRectangle(b, s);
          for (var g = s[0].x < s[1].x ? 0 : 1, l = s[2].x < s[3].x ? 2 : 3, l = s[g].x < s[l].x ? g : l, g = [], n = 0;5 > n;n++, l++) {
            g.push(s[l % 4]);
          }
          (g[1].x - g[0].x) * (g[3].y - g[0].y) < (g[1].y - g[0].y) * (g[3].x - g[0].x) && (s = g[1], g[1] = g[3], g[3] = s);
          var s = [], p, r, l = Math.floor(g[0].x / this.tileW), n = (l + 1) * this.tileW;
          if (g[2].x < n) {
            p = Math.min(g[0].y, g[1].y, g[2].y, g[3].y);
            r = Math.max(g[0].y, g[1].y, g[2].y, g[3].y);
            var t = Math.floor(p / this.tileH), m = Math.floor(r / this.tileH);
            e(s, this, l, t, m);
            return s;
          }
          var v = 0, u = 4, y = !1;
          if (g[0].x === g[1].x || g[0].x === g[3].x) {
            g[0].x === g[1].x ? (y = !0, v++) : u--, p = c(n, g[v], g[v + 1]), r = c(n, g[u], g[u - 1]), t = Math.floor(g[v].y / this.tileH), m = Math.floor(g[u].y / this.tileH), e(s, this, l, t, m), l++;
          }
          do {
            var w, B, A, C;
            g[v + 1].x < n ? (w = g[v + 1].y, A = !0) : (w = c(n, g[v], g[v + 1]), A = !1);
            g[u - 1].x < n ? (B = g[u - 1].y, C = !0) : (B = c(n, g[u], g[u - 1]), C = !1);
            t = Math.floor((g[v].y < g[v + 1].y ? p : w) / this.tileH);
            m = Math.floor((g[u].y > g[u - 1].y ? r : B) / this.tileH);
            e(s, this, l, t, m);
            if (A && y) {
              break;
            }
            A ? (y = !0, v++, p = c(n, g[v], g[v + 1])) : p = w;
            C ? (u--, r = c(n, g[u], g[u - 1])) : r = B;
            l++;
            n = (l + 1) * this.tileW;
          } while (v < u);
          return s;
        };
        a._points = p.createEmptyPoints(4);
        return a;
      }();
      e.TileCache = u;
      t = function() {
        function a(b, d, c) {
          this._cacheLevels = [];
          this._source = b;
          this._tileSize = d;
          this._minUntiledSize = c;
        }
        a.prototype._getTilesAtScale = function(a, d, c) {
          var h = Math.max(d.getAbsoluteScaleX(), d.getAbsoluteScaleY()), e = 0;
          1 !== h && (e = k(Math.round(Math.log(1 / h) / Math.LN2), -5, 3));
          h = b(e);
          if (this._source.hasFlags(1048576)) {
            for (;;) {
              h = b(e);
              if (c.contains(this._source.getBounds().getAbsoluteBounds().clone().scale(h, h))) {
                break;
              }
              e--;
              n(-5 <= e);
            }
          }
          this._source.hasFlags(2097152) || (e = k(e, -5, 0));
          h = b(e);
          c = 5 + e;
          e = this._cacheLevels[c];
          if (!e) {
            var e = this._source.getBounds().getAbsoluteBounds().clone().scale(h, h), g, l;
            this._source.hasFlags(1048576) || !this._source.hasFlags(4194304) || Math.max(e.w, e.h) <= this._minUntiledSize ? (g = e.w, l = e.h) : g = l = this._tileSize;
            e = this._cacheLevels[c] = new u(e.w, e.h, g, l, h);
          }
          return e.getTiles(a, d.scaleClone(h, h));
        };
        a.prototype.fetchTiles = function(a, b, c, h) {
          var e = new v(0, 0, c.canvas.width, c.canvas.height);
          a = this._getTilesAtScale(a, b, e);
          var g;
          b = this._source;
          for (var l = 0;l < a.length;l++) {
            var n = a[l];
            n.cachedSurfaceRegion && n.cachedSurfaceRegion.surface && !b.hasFlags(1048592) || (g || (g = []), g.push(n));
          }
          g && this._cacheTiles(c, g, h, e);
          b.removeFlags(16);
          return a;
        };
        a.prototype._getTileBounds = function(a) {
          for (var b = v.createEmpty(), c = 0;c < a.length;c++) {
            b.union(a[c].bounds);
          }
          return b;
        };
        a.prototype._cacheTiles = function(a, b, c, h, e) {
          void 0 === e && (e = 4);
          n(0 < e, "Infinite recursion is likely.");
          var g = this._getTileBounds(b);
          a.save();
          a.setTransform(1, 0, 0, 1, 0, 0);
          a.clearRect(0, 0, h.w, h.h);
          a.translate(-g.x, -g.y);
          a.scale(b[0].scale, b[0].scale);
          var l = this._source.getBounds();
          a.translate(-l.x, -l.y);
          2 <= m.traceLevel && m.writer && m.writer.writeLn("Rendering Tiles: " + g);
          this._source.render(a, 0);
          a.restore();
          for (var l = null, k = 0;k < b.length;k++) {
            var p = b[k], r = p.bounds.clone();
            r.x -= g.x;
            r.y -= g.y;
            h.contains(r) || (l || (l = []), l.push(p));
            p.cachedSurfaceRegion = c(p.cachedSurfaceRegion, a, r);
          }
          l && (2 <= l.length ? (b = l.slice(0, l.length / 2 | 0), g = l.slice(b.length), this._cacheTiles(a, b, c, h, e - 1), this._cacheTiles(a, g, c, h, e - 1)) : this._cacheTiles(a, l, c, h, e - 1));
        };
        return a;
      }();
      e.RenderableTileCache = t;
    })(m.Geometry || (m.Geometry = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(g, m) {
  function e() {
    this.constructor = g;
  }
  for (var c in m) {
    m.hasOwnProperty(c) && (g[c] = m[c]);
  }
  e.prototype = m.prototype;
  g.prototype = new e;
};
(function(g) {
  (function(m) {
    var e = g.IntegerUtilities.roundToMultipleOfPowerOfTwo, c = g.Debug.assert, w = m.Geometry.Rectangle;
    (function(g) {
      var b = function(a) {
        function b() {
          a.apply(this, arguments);
        }
        __extends(b, a);
        return b;
      }(m.Geometry.Rectangle);
      g.Region = b;
      var a = function() {
        function a(b, c) {
          this._root = new n(0, 0, b | 0, c | 0, !1);
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          c(0 < a && 0 < b);
          var h = this._root.insert(a, b);
          h && (h.allocator = this, h.allocated = !0);
          return h;
        };
        a.prototype.free = function(a) {
          c(a.allocator === this);
          a.clear();
          a.allocated = !1;
        };
        a.RANDOM_ORIENTATION = !0;
        a.MAX_DEPTH = 256;
        return a;
      }();
      g.CompactAllocator = a;
      var n = function(b) {
        function c(a, h, f, d, q) {
          b.call(this, a, h, f, d);
          this._children = null;
          this._horizontal = q;
          this.allocated = !1;
        }
        __extends(c, b);
        c.prototype.clear = function() {
          this._children = null;
          this.allocated = !1;
        };
        c.prototype.insert = function(a, b) {
          return this._insert(a, b, 0);
        };
        c.prototype._insert = function(b, h, f) {
          if (!(f > a.MAX_DEPTH || this.allocated || this.w < b || this.h < h)) {
            if (this._children) {
              var d;
              if ((d = this._children[0]._insert(b, h, f + 1)) || (d = this._children[1]._insert(b, h, f + 1))) {
                return d;
              }
            } else {
              return d = !this._horizontal, a.RANDOM_ORIENTATION && (d = .5 <= Math.random()), this._children = this._horizontal ? [new c(this.x, this.y, this.w, h, !1), new c(this.x, this.y + h, this.w, this.h - h, d)] : [new c(this.x, this.y, b, this.h, !0), new c(this.x + b, this.y, this.w - b, this.h, d)], d = this._children[0], d.w === b && d.h === h ? (d.allocated = !0, d) : this._insert(b, h, f + 1);
            }
          }
        };
        return c;
      }(g.Region), p = function() {
        function a(b, c, h, f) {
          this._columns = b / h | 0;
          this._rows = c / f | 0;
          this._sizeW = h;
          this._sizeH = f;
          this._freeList = [];
          this._index = 0;
          this._total = this._columns * this._rows;
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          c(0 < a && 0 < b);
          var h = this._sizeW, f = this._sizeH;
          if (a > h || b > f) {
            return null;
          }
          var d = this._freeList, q = this._index;
          return 0 < d.length ? (h = d.pop(), c(!1 === h.allocated), h.w = a, h.h = b, h.allocated = !0, h) : q < this._total ? (d = q / this._columns | 0, h = new y((q - d * this._columns) * h, d * f, a, b), h.index = q, h.allocator = this, h.allocated = !0, this._index++, h) : null;
        };
        a.prototype.free = function(a) {
          c(a.allocator === this);
          a.allocated = !1;
          this._freeList.push(a);
        };
        return a;
      }();
      g.GridAllocator = p;
      var y = function(a) {
        function b(c, h, f, d) {
          a.call(this, c, h, f, d);
          this.index = -1;
        }
        __extends(b, a);
        return b;
      }(g.Region);
      g.GridCell = y;
      var v = function() {
        return function(a, b, c) {
          this.size = a;
          this.region = b;
          this.allocator = c;
        };
      }(), l = function(a) {
        function b(c, h, f, d, q) {
          a.call(this, c, h, f, d);
          this.region = q;
        }
        __extends(b, a);
        return b;
      }(g.Region);
      g.BucketCell = l;
      b = function() {
        function a(b, e) {
          c(0 < b && 0 < e);
          this._buckets = [];
          this._w = b | 0;
          this._h = e | 0;
          this._filled = 0;
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          c(0 < a && 0 < b);
          var h = Math.max(a, b);
          if (a > this._w || b > this._h) {
            return null;
          }
          var f = null, d, q = this._buckets;
          do {
            for (var g = 0;g < q.length && !(q[g].size >= h && (d = q[g], f = d.allocator.allocate(a, b)));g++) {
            }
            if (!f) {
              var s = this._h - this._filled;
              if (s < b) {
                return null;
              }
              var g = e(h, 8), n = 2 * g;
              n > s && (n = s);
              if (n < g) {
                return null;
              }
              s = new w(0, this._filled, this._w, n);
              this._buckets.push(new v(g, s, new p(s.w, s.h, g, g)));
              this._filled += n;
            }
          } while (!f);
          return new l(d.region.x + f.x, d.region.y + f.y, f.w, f.h, f);
        };
        a.prototype.free = function(a) {
          a.region.allocator.free(a.region);
        };
        return a;
      }();
      g.BucketAllocator = b;
    })(m.RegionAllocator || (m.RegionAllocator = {}));
    (function(c) {
      var b = function() {
        function a(a) {
          this._createSurface = a;
          this._surfaces = [];
        }
        Object.defineProperty(a.prototype, "surfaces", {get:function() {
          return this._surfaces;
        }, enumerable:!0, configurable:!0});
        a.prototype._createNewSurface = function(a, b) {
          var c = this._createSurface(a, b);
          this._surfaces.push(c);
          return c;
        };
        a.prototype.addSurface = function(a) {
          this._surfaces.push(a);
        };
        a.prototype.allocate = function(a, b) {
          for (var c = 0;c < this._surfaces.length;c++) {
            var e = this._surfaces[c].allocate(a, b);
            if (e) {
              return e;
            }
          }
          return this._createNewSurface(a, b).allocate(a, b);
        };
        a.prototype.free = function(a) {
        };
        return a;
      }();
      c.SimpleAllocator = b;
    })(m.SurfaceRegionAllocator || (m.SurfaceRegionAllocator = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    var e = m.Geometry.Rectangle, c = m.Geometry.Matrix, w = m.Geometry.DirtyRegion, k = g.Debug.assert;
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
    })(m.BlendMode || (m.BlendMode = {}));
    var b = m.BlendMode;
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
    })(m.NodeFlags || (m.NodeFlags = {}));
    var a = m.NodeFlags;
    (function(a) {
      a[a.Node = 1] = "Node";
      a[a.Shape = 3] = "Shape";
      a[a.Group = 5] = "Group";
      a[a.Stage = 13] = "Stage";
      a[a.Renderable = 33] = "Renderable";
    })(m.NodeType || (m.NodeType = {}));
    var n = m.NodeType;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.OnStageBoundsChanged = 1] = "OnStageBoundsChanged";
    })(m.NodeEventType || (m.NodeEventType = {}));
    var p = function() {
      function a() {
      }
      a.prototype.visitNode = function(a, b) {
      };
      a.prototype.visitShape = function(a, b) {
        this.visitNode(a, b);
      };
      a.prototype.visitGroup = function(a, b) {
        this.visitNode(a, b);
        for (var c = a.getChildren(), f = 0;f < c.length;f++) {
          c[f].visit(this, b);
        }
      };
      a.prototype.visitStage = function(a, b) {
        this.visitGroup(a, b);
      };
      a.prototype.visitRenderable = function(a, b) {
        this.visitNode(a, b);
      };
      return a;
    }();
    m.NodeVisitor = p;
    var y = function() {
      return function() {
      };
    }();
    m.State = y;
    var v = function(a) {
      function b() {
        a.call(this);
        this.matrix = c.createIdentity();
      }
      __extends(b, a);
      b.prototype.transform = function(a) {
        var b = this.clone();
        b.matrix.preMultiply(a.getMatrix());
        return b;
      };
      b.allocate = function() {
        var a = b._dirtyStack, c = null;
        a.length && (c = a.pop());
        return c;
      };
      b.prototype.clone = function() {
        var a = b.allocate();
        a || (a = new b);
        a.set(this);
        return a;
      };
      b.prototype.set = function(a) {
        this.matrix.set(a.matrix);
      };
      b.prototype.free = function() {
        b._dirtyStack.push(this);
      };
      b._dirtyStack = [];
      return b;
    }(y);
    m.MatrixState = v;
    var l = function(a) {
      function b() {
        a.apply(this, arguments);
        this.isDirty = !0;
      }
      __extends(b, a);
      b.prototype.start = function(a, b) {
        this._dirtyRegion = b;
        var d = new v;
        d.matrix.setIdentity();
        a.visit(this, d);
        d.free();
      };
      b.prototype.visitGroup = function(a, b) {
        var d = a.getChildren();
        this.visitNode(a, b);
        for (var c = 0;c < d.length;c++) {
          var f = d[c], h = b.transform(f.getTransform());
          f.visit(this, h);
          h.free();
        }
      };
      b.prototype.visitNode = function(a, b) {
        a.hasFlags(16) && (this.isDirty = !0);
        a.toggleFlags(16, !1);
      };
      return b;
    }(p);
    m.DirtyNodeVisitor = l;
    y = function(a) {
      function b(d) {
        a.call(this);
        this.writer = d;
      }
      __extends(b, a);
      b.prototype.visitNode = function(a, b) {
      };
      b.prototype.visitShape = function(a, b) {
        this.writer.writeLn(a.toString());
        this.visitNode(a, b);
      };
      b.prototype.visitGroup = function(a, b) {
        this.visitNode(a, b);
        var d = a.getChildren();
        this.writer.enter(a.toString() + " " + d.length);
        for (var c = 0;c < d.length;c++) {
          d[c].visit(this, b);
        }
        this.writer.outdent();
      };
      b.prototype.visitStage = function(a, b) {
        this.visitGroup(a, b);
      };
      return b;
    }(p);
    m.TracingNodeVisitor = y;
    var t = function() {
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
      b.prototype._dispatchEvent = function(a) {
        if (this._eventListeners) {
          for (var b = this._eventListeners, c = 0;c < b.length;c++) {
            var f = b[c];
            f.type === a && f.listener(this, a);
          }
        }
      };
      b.prototype.addEventListener = function(a, b) {
        this._eventListeners || (this._eventListeners = []);
        this._eventListeners.push({type:a, listener:b});
      };
      Object.defineProperty(b.prototype, "properties", {get:function() {
        return this._properties || (this._properties = {});
      }, enumerable:!0, configurable:!0});
      b.prototype.reset = function() {
        this._flags = a.Default;
        this._properties = this._transform = this._layer = this._bounds = null;
      };
      Object.defineProperty(b.prototype, "clip", {get:function() {
        return this._clip;
      }, set:function(a) {
        this._clip = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "parent", {get:function() {
        return this._parent;
      }, enumerable:!0, configurable:!0});
      b.prototype.getTransformedBounds = function(a) {
        var b = this.getBounds(!0);
        if (a !== this && !b.isEmpty()) {
          var c = this.getTransform().getConcatenatedMatrix();
          a ? (a = a.getTransform().getInvertedConcatenatedMatrix(), a.preMultiply(c), a.transformRectangleAABB(b), a.free()) : c.transformRectangleAABB(b);
        }
        return b;
      };
      b.prototype._markCurrentBoundsAsDirtyRegion = function() {
      };
      b.prototype.getStage = function(a) {
        void 0 === a && (a = !0);
        for (var b = this._parent;b;) {
          if (b.isType(13)) {
            var c = b;
            if (a) {
              if (c.dirtyRegion) {
                return c;
              }
            } else {
              return c;
            }
          }
          b = b._parent;
        }
        return null;
      };
      b.prototype.getChildren = function(a) {
        throw g.Debug.abstractMethod("Node::getChildren");
      };
      b.prototype.getBounds = function(a) {
        throw g.Debug.abstractMethod("Node::getBounds");
      };
      b.prototype.setBounds = function(a) {
        k(!(this._flags & 2));
        (this._bounds || (this._bounds = e.createEmpty())).set(a);
        this.removeFlags(256);
      };
      b.prototype.clone = function() {
        throw g.Debug.abstractMethod("Node::clone");
      };
      b.prototype.setFlags = function(a) {
        this._flags |= a;
      };
      b.prototype.hasFlags = function(a) {
        return(this._flags & a) === a;
      };
      b.prototype.hasAnyFlags = function(a) {
        return!!(this._flags & a);
      };
      b.prototype.removeFlags = function(a) {
        this._flags &= ~a;
      };
      b.prototype.toggleFlags = function(a, b) {
        this._flags = b ? this._flags | a : this._flags & ~a;
      };
      b.prototype._propagateFlagsUp = function(a) {
        if (0 !== a && !this.hasFlags(a)) {
          this.hasFlags(2) || (a &= -257);
          this.setFlags(a);
          var b = this._parent;
          b && b._propagateFlagsUp(a);
        }
      };
      b.prototype._propagateFlagsDown = function(a) {
        throw g.Debug.abstractMethod("Node::_propagateFlagsDown");
      };
      b.prototype.isAncestor = function(a) {
        for (;a;) {
          if (a === this) {
            return!0;
          }
          k(a !== a._parent);
          a = a._parent;
        }
        return!1;
      };
      b._getAncestors = function(a, c) {
        var h = b._path;
        for (h.length = 0;a && a !== c;) {
          k(a !== a._parent), h.push(a), a = a._parent;
        }
        k(a === c, "Last ancestor is not an ancestor.");
        return h;
      };
      b.prototype._findClosestAncestor = function() {
        for (var a = this;a;) {
          if (!1 === a.hasFlags(512)) {
            return a;
          }
          k(a !== a._parent);
          a = a._parent;
        }
        return null;
      };
      b.prototype.isType = function(a) {
        return this._type === a;
      };
      b.prototype.isTypeOf = function(a) {
        return(this._type & a) === a;
      };
      b.prototype.isLeaf = function() {
        return this.isType(33) || this.isType(3);
      };
      b.prototype.isLinear = function() {
        if (this.isLeaf()) {
          return!0;
        }
        if (this.isType(5)) {
          var a = this._children;
          if (1 === a.length && a[0].isLinear()) {
            return!0;
          }
        }
        return!1;
      };
      b.prototype.getTransformMatrix = function() {
        var a;
        void 0 === a && (a = !1);
        return this.getTransform().getMatrix(a);
      };
      b.prototype.getTransform = function() {
        null === this._transform && (this._transform = new u(this));
        return this._transform;
      };
      b.prototype.getLayer = function() {
        null === this._layer && (this._layer = new h(this));
        return this._layer;
      };
      b.prototype.visit = function(a, b) {
        switch(this._type) {
          case 1:
            a.visitNode(this, b);
            break;
          case 5:
            a.visitGroup(this, b);
            break;
          case 13:
            a.visitStage(this, b);
            break;
          case 3:
            a.visitShape(this, b);
            break;
          case 33:
            a.visitRenderable(this, b);
            break;
          default:
            g.Debug.unexpectedCase();
        }
      };
      b.prototype.invalidate = function() {
        this._propagateFlagsUp(a.UpOnInvalidate);
      };
      b.prototype.toString = function(a) {
        void 0 === a && (a = !1);
        var b = n[this._type] + " " + this._id;
        a && (b += " " + this._bounds.toString());
        return b;
      };
      b._path = [];
      b._nextId = 0;
      return b;
    }();
    m.Node = t;
    var r = function(b) {
      function d() {
        b.call(this);
        this._type = 5;
        this._children = [];
      }
      __extends(d, b);
      d.prototype.getChildren = function(a) {
        void 0 === a && (a = !1);
        return a ? this._children.slice(0) : this._children;
      };
      d.prototype.childAt = function(a) {
        k(0 <= a && a < this._children.length);
        return this._children[a];
      };
      Object.defineProperty(d.prototype, "child", {get:function() {
        k(1 === this._children.length);
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(d.prototype, "groupChild", {get:function() {
        k(1 === this._children.length);
        k(this._children[0] instanceof d);
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      d.prototype.addChild = function(b) {
        k(b);
        k(!b.isAncestor(this));
        b._parent && b._parent.removeChildAt(b._index);
        b._parent = this;
        b._index = this._children.length;
        this._children.push(b);
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
        b._propagateFlagsDown(a.DownOnAddedOrRemoved);
      };
      d.prototype.removeChildAt = function(b) {
        k(0 <= b && b < this._children.length);
        var d = this._children[b];
        k(b === d._index);
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
      d.prototype._propagateFlagsDown = function(a) {
        if (!this.hasFlags(a)) {
          this.setFlags(a);
          for (var b = this._children, d = 0;d < b.length;d++) {
            b[d]._propagateFlagsDown(a);
          }
        }
      };
      d.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var b = this._bounds || (this._bounds = e.createEmpty());
        if (this.hasFlags(256)) {
          b.setEmpty();
          for (var d = this._children, c = e.allocate(), f = 0;f < d.length;f++) {
            var h = d[f];
            c.set(h.getBounds());
            h.getTransformMatrix().transformRectangleAABB(c);
            b.union(c);
          }
          c.free();
          this.removeFlags(256);
        }
        return a ? b.clone() : b;
      };
      return d;
    }(t);
    m.Group = r;
    var u = function() {
      function b(a) {
        this._node = a;
        this._matrix = c.createIdentity();
        this._colorMatrix = m.ColorMatrix.createIdentity();
        this._concatenatedMatrix = c.createIdentity();
        this._invertedConcatenatedMatrix = c.createIdentity();
        this._concatenatedColorMatrix = m.ColorMatrix.createIdentity();
      }
      b.prototype.setMatrix = function(b) {
        this._matrix.isEqual(b) || (this._matrix.set(b), this._node._propagateFlagsUp(a.UpOnMoved), this._node._propagateFlagsDown(a.DownOnMoved));
      };
      b.prototype.setColorMatrix = function(b) {
        this._colorMatrix.set(b);
        this._node._propagateFlagsUp(a.UpOnColorMatrixChanged);
        this._node._propagateFlagsDown(a.DownOnColorMatrixChanged);
      };
      b.prototype.getMatrix = function(a) {
        void 0 === a && (a = !1);
        return a ? this._matrix.clone() : this._matrix;
      };
      b.prototype.hasColorMatrix = function() {
        return null !== this._colorMatrix;
      };
      b.prototype.getColorMatrix = function() {
        var a;
        void 0 === a && (a = !1);
        null === this._colorMatrix && (this._colorMatrix = m.ColorMatrix.createIdentity());
        return a ? this._colorMatrix.clone() : this._colorMatrix;
      };
      b.prototype.getConcatenatedMatrix = function(a) {
        void 0 === a && (a = !1);
        if (this._node.hasFlags(512)) {
          for (var b = this._node._findClosestAncestor(), f = t._getAncestors(this._node, b), h = b ? b.getTransform()._concatenatedMatrix.clone() : c.createIdentity(), e = f.length - 1;0 <= e;e--) {
            var b = f[e], g = b.getTransform();
            k(b.hasFlags(512));
            h.preMultiply(g._matrix);
            g._concatenatedMatrix.set(h);
            b.removeFlags(512);
          }
        }
        return a ? this._concatenatedMatrix.clone() : this._concatenatedMatrix;
      };
      b.prototype.getInvertedConcatenatedMatrix = function() {
        var a = !0;
        void 0 === a && (a = !1);
        this._node.hasFlags(1024) && (this.getConcatenatedMatrix().inverse(this._invertedConcatenatedMatrix), this._node.removeFlags(1024));
        return a ? this._invertedConcatenatedMatrix.clone() : this._invertedConcatenatedMatrix;
      };
      b.prototype.toString = function() {
        return this._matrix.toString();
      };
      return b;
    }();
    m.Transform = u;
    var h = function() {
      function a(b) {
        this._node = b;
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
        return b[this._blendMode];
      };
      return a;
    }();
    m.Layer = h;
    y = function(a) {
      function b(d) {
        a.call(this);
        k(d);
        this._source = d;
        this._type = 3;
        this.ratio = 0;
      }
      __extends(b, a);
      b.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var b = this._bounds || (this._bounds = e.createEmpty());
        this.hasFlags(256) && (b.set(this._source.getBounds()), this.removeFlags(256));
        return a ? b.clone() : b;
      };
      Object.defineProperty(b.prototype, "source", {get:function() {
        return this._source;
      }, enumerable:!0, configurable:!0});
      b.prototype._propagateFlagsDown = function(a) {
        this.setFlags(a);
      };
      b.prototype.getChildren = function(a) {
        return[this._source];
      };
      return b;
    }(t);
    m.Shape = y;
    m.RendererOptions = function() {
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
    })(m.Backend || (m.Backend = {}));
    p = function(a) {
      function b(d, c, h) {
        a.call(this);
        this._container = d;
        this._stage = c;
        this._options = h;
        this._viewport = e.createSquare();
        this._devicePixelRatio = 1;
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "viewport", {set:function(a) {
        this._viewport.set(a);
      }, enumerable:!0, configurable:!0});
      b.prototype.render = function() {
        throw g.Debug.abstractMethod("Renderer::render");
      };
      b.prototype.resize = function() {
        throw g.Debug.abstractMethod("Renderer::resize");
      };
      b.prototype.screenShot = function(a, b) {
        throw g.Debug.abstractMethod("Renderer::screenShot");
      };
      return b;
    }(p);
    m.Renderer = p;
    p = function(a) {
      function b(c, h, g) {
        void 0 === g && (g = !1);
        a.call(this);
        this._dirtyVisitor = new l;
        this._flags &= -3;
        this._type = 13;
        this._scaleMode = b.DEFAULT_SCALE;
        this._align = b.DEFAULT_ALIGN;
        this._content = new r;
        this._content._flags &= -3;
        this.addChild(this._content);
        this.setFlags(16);
        this.setBounds(new e(0, 0, c, h));
        g ? (this._dirtyRegion = new w(c, h), this._dirtyRegion.addDirtyRectangle(new e(0, 0, c, h))) : this._dirtyRegion = null;
        this._updateContentMatrix();
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "dirtyRegion", {get:function() {
        return this._dirtyRegion;
      }, enumerable:!0, configurable:!0});
      b.prototype.setBounds = function(b) {
        a.prototype.setBounds.call(this, b);
        this._updateContentMatrix();
        this._dispatchEvent(1);
        this._dirtyRegion && (this._dirtyRegion = new w(b.w, b.h), this._dirtyRegion.addDirtyRectangle(b));
      };
      Object.defineProperty(b.prototype, "content", {get:function() {
        return this._content;
      }, enumerable:!0, configurable:!0});
      b.prototype.readyToRender = function() {
        this._dirtyVisitor.isDirty = !1;
        this._dirtyVisitor.start(this, this._dirtyRegion);
        return this._dirtyVisitor.isDirty ? !0 : !1;
      };
      Object.defineProperty(b.prototype, "align", {get:function() {
        return this._align;
      }, set:function(a) {
        this._align = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "scaleMode", {get:function() {
        return this._scaleMode;
      }, set:function(a) {
        this._scaleMode = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      b.prototype._updateContentMatrix = function() {
        if (this._scaleMode === b.DEFAULT_SCALE && this._align === b.DEFAULT_ALIGN) {
          this._content.getTransform().setMatrix(new c(1, 0, 0, 1, 0, 0));
        } else {
          var a = this.getBounds(), f = this._content.getBounds(), h = a.w / f.w, e = a.h / f.h;
          switch(this._scaleMode) {
            case 2:
              h = e = Math.max(h, e);
              break;
            case 4:
              h = e = 1;
              break;
            case 1:
              break;
            default:
              h = e = Math.min(h, e);
          }
          var g;
          g = this._align & 4 ? 0 : this._align & 8 ? a.w - f.w * h : (a.w - f.w * h) / 2;
          a = this._align & 1 ? 0 : this._align & 2 ? a.h - f.h * e : (a.h - f.h * e) / 2;
          this._content.getTransform().setMatrix(new c(h, 0, 0, e, g, a));
        }
      };
      b.DEFAULT_SCALE = 4;
      b.DEFAULT_ALIGN = 5;
      return b;
    }(r);
    m.Stage = p;
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    function e(a, b, c) {
      return a + (b - a) * c;
    }
    function c(a, b, c) {
      return e(a >> 24 & 255, b >> 24 & 255, c) << 24 | e(a >> 16 & 255, b >> 16 & 255, c) << 16 | e(a >> 8 & 255, b >> 8 & 255, c) << 8 | e(a & 255, b & 255, c);
    }
    var w = m.Geometry.Point, k = m.Geometry.Rectangle, b = m.Geometry.Matrix, a = g.Debug.assertUnreachable, n = g.Debug.assert, p = g.ArrayUtilities.indexOf, y = function(a) {
      function b() {
        a.call(this);
        this._parents = [];
        this._renderableParents = [];
        this._invalidateEventListeners = null;
        this._flags &= -3;
        this._type = 33;
      }
      __extends(b, a);
      b.prototype.addParent = function(a) {
        n(a);
        var b = p(this._parents, a);
        n(0 > b);
        this._parents.push(a);
      };
      b.prototype.addRenderableParent = function(a) {
        n(a);
        var b = p(this._renderableParents, a);
        n(0 > b);
        this._renderableParents.push(a);
      };
      b.prototype.wrap = function() {
        for (var a, b = this._parents, d = 0;d < b.length;d++) {
          if (a = b[d], !a._parent) {
            return a;
          }
        }
        a = new m.Shape(this);
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
        var b = p(this._invalidateEventListeners, a);
        n(0 > b);
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
      b.prototype.render = function(a, b, d, c, f) {
      };
      return b;
    }(m.Node);
    m.Renderable = y;
    var v = function(a) {
      function b(d, c) {
        a.call(this);
        this.setBounds(d);
        this.render = c;
      }
      __extends(b, a);
      return b;
    }(y);
    m.CustomRenderable = v;
    (function(a) {
      a[a.Idle = 1] = "Idle";
      a[a.Playing = 2] = "Playing";
      a[a.Paused = 3] = "Paused";
    })(m.RenderableVideoState || (m.RenderableVideoState = {}));
    v = function(a) {
      function b(c, h) {
        a.call(this);
        this._flags = 1048592;
        this._lastPausedTime = this._lastTimeInvalidated = 0;
        this._seekHappens = !1;
        this._isDOMElement = !0;
        this.setBounds(new k(0, 0, 1, 1));
        this._assetId = c;
        this._eventSerializer = h;
        var e = document.createElement("video"), g = this._handleVideoEvent.bind(this);
        e.preload = "metadata";
        e.addEventListener("play", g);
        e.addEventListener("ended", g);
        e.addEventListener("loadeddata", g);
        e.addEventListener("progress", g);
        e.addEventListener("waiting", g);
        e.addEventListener("loadedmetadata", g);
        e.addEventListener("error", g);
        e.addEventListener("seeking", g);
        this._video = e;
        this._videoEventHandler = g;
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
        this._video.play();
        this._state = 2;
      };
      b.prototype.pause = function() {
        this._video.pause();
        this._state = 3;
      };
      b.prototype._handleVideoEvent = function(a) {
        var b = null, d = this._video;
        switch(a.type) {
          case "play":
            a = 1;
            break;
          case "ended":
            a = 2;
            break;
          case "loadeddata":
            a = 3;
            break;
          case "progress":
            a = 4;
            break;
          case "waiting":
            a = 5;
            break;
          case "loadedmetadata":
            a = 7;
            b = {videoWidth:d.videoWidth, videoHeight:d.videoHeight, duration:d.duration};
            break;
          case "error":
            a = 6;
            b = {code:d.error.code};
            break;
          case "seeking":
            a = 8;
            this._seekHappens = !0;
            break;
          default:
            return;
        }
        this._notifyNetStream(a, b);
      };
      b.prototype._notifyNetStream = function(a, b) {
        this._eventSerializer.sendVideoPlaybackEvent(this._assetId, a, b);
      };
      b.prototype.processControlRequest = function(a, b) {
        var d = this._video;
        switch(a) {
          case 1:
            d.src = b.url;
            this._notifyNetStream(0, null);
            break;
          case 2:
            d && (b.paused && !d.paused ? (this._lastPausedTime = isNaN(b.time) ? d.currentTime : d.currentTime = b.time, this.pause()) : !b.paused && d.paused && (this.play(), isNaN(b.time) || this._lastPausedTime === b.time || (d.currentTime = b.time), this._seekHappens && (this._seekHappens = !1, this._notifyNetStream(3, null))));
            break;
          case 3:
            d && (d.currentTime = b.time);
            break;
          case 4:
            return d ? d.currentTime : 0;
          case 5:
            return d ? d.duration : 0;
          case 6:
            d && (d.volume = b.volume);
            break;
          case 7:
            if (!d) {
              return 0;
            }
            var c = -1;
            if (d.buffered) {
              for (var f = 0;f < d.buffered.length;f++) {
                c = Math.max(c, d.buffered.end(f));
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
        for (var a = b._renderableVideos, c = 0;c < a.length;c++) {
          a[c].checkForUpdate();
        }
      };
      b.prototype.render = function(a, b, d) {
        (b = this._video) && 0 < b.videoWidth && a.drawImage(b, 0, 0, b.videoWidth, b.videoHeight, 0, 0, this._bounds.w, this._bounds.h);
      };
      b._renderableVideos = [];
      return b;
    }(y);
    m.RenderableVideo = v;
    v = function(a) {
      function b(d, c) {
        a.call(this);
        this._flags = 1048592;
        this.properties = {};
        this.setBounds(c);
        d instanceof HTMLCanvasElement ? this._initializeSourceCanvas(d) : this._sourceImage = d;
      }
      __extends(b, a);
      b.FromDataBuffer = function(a, c, f) {
        var h = document.createElement("canvas");
        h.width = f.w;
        h.height = f.h;
        f = new b(h, f);
        f.updateFromDataBuffer(a, c);
        return f;
      };
      b.FromNode = function(a, c, f, h, e) {
        var g = document.createElement("canvas"), l = a.getBounds();
        g.width = l.w;
        g.height = l.h;
        g = new b(g, l);
        g.drawNode(a, c, f, h, e);
        return g;
      };
      b.FromImage = function(a) {
        return new b(a, new k(0, 0, -1, -1));
      };
      b.prototype.updateFromDataBuffer = function(a, b) {
        if (m.imageUpdateOption.value) {
          var d = b.buffer;
          if (4 === a || 5 === a || 6 === a) {
            g.Debug.assertUnreachable("Mustn't encounter un-decoded images here");
          } else {
            var c = this._bounds, f = this._imageData;
            f && f.width === c.w && f.height === c.h || (f = this._imageData = this._context.createImageData(c.w, c.h));
            m.imageConvertOption.value && (d = new Int32Array(d), c = new Int32Array(f.data.buffer), g.ColorUtilities.convertImage(a, 3, d, c));
            this._ensureSourceCanvas();
            this._context.putImageData(f, 0, 0);
          }
          this.invalidate();
        }
      };
      b.prototype.readImageData = function(a) {
        a.writeRawBytes(this.imageData.data);
      };
      b.prototype.render = function(a, b, d) {
        this.renderSource ? a.drawImage(this.renderSource, 0, 0) : this._renderFallback(a);
      };
      b.prototype.drawNode = function(a, b, d, c, f) {
        d = m.Canvas2D;
        c = this.getBounds();
        (new d.Canvas2DRenderer(this._canvas, null)).renderNode(a, f || c, b);
      };
      b.prototype._initializeSourceCanvas = function(a) {
        this._canvas = a;
        this._context = this._canvas.getContext("2d");
      };
      b.prototype._ensureSourceCanvas = function() {
        if (!this._canvas) {
          var a = document.createElement("canvas"), b = this._bounds;
          a.width = b.w;
          a.height = b.h;
          this._initializeSourceCanvas(a);
        }
      };
      Object.defineProperty(b.prototype, "imageData", {get:function() {
        this._canvas || (n(this._sourceImage), this._ensureSourceCanvas(), this._context.drawImage(this._sourceImage, 0, 0), this._sourceImage = null);
        return this._context.getImageData(0, 0, this._bounds.w, this._bounds.h);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "renderSource", {get:function() {
        return this._canvas || this._sourceImage;
      }, enumerable:!0, configurable:!0});
      b.prototype._renderFallback = function(a) {
        this.fillStyle || (this.fillStyle = g.ColorStyle.randomStyle());
        var b = this._bounds;
        a.save();
        a.beginPath();
        a.lineWidth = 2;
        a.fillStyle = this.fillStyle;
        a.fillRect(b.x, b.y, b.w, b.h);
        a.restore();
      };
      return b;
    }(y);
    m.RenderableBitmap = v;
    (function(a) {
      a[a.Fill = 0] = "Fill";
      a[a.Stroke = 1] = "Stroke";
      a[a.StrokeFill = 2] = "StrokeFill";
    })(m.PathType || (m.PathType = {}));
    var l = function() {
      return function(a, b, c, h) {
        this.type = a;
        this.style = b;
        this.smoothImage = c;
        this.strokeProperties = h;
        this.path = new Path2D;
        n(1 === a === !!h);
      };
    }();
    m.StyledPath = l;
    var t = function() {
      return function(a, b, c, h, e) {
        this.thickness = a;
        this.scaleMode = b;
        this.capsStyle = c;
        this.jointsStyle = h;
        this.miterLimit = e;
      };
    }();
    m.StrokeProperties = t;
    var r = function(c) {
      function d(a, b, d, h) {
        c.call(this);
        this._flags = 6291472;
        this.properties = {};
        this.setBounds(h);
        this._id = a;
        this._pathData = b;
        this._textures = d;
        d.length && this.setFlags(1048576);
      }
      __extends(d, c);
      d.prototype.update = function(a, b, d) {
        this.setBounds(d);
        this._pathData = a;
        this._paths = null;
        this._textures = b;
        this.setFlags(1048576);
        this.invalidate();
      };
      d.prototype.render = function(a, b, d, c, f) {
        void 0 === c && (c = !1);
        void 0 === f && (f = !1);
        a.fillStyle = a.strokeStyle = "transparent";
        b = this._deserializePaths(this._pathData, a, b);
        n(b);
        for (d = 0;d < b.length;d++) {
          var h = b[d];
          a.mozImageSmoothingEnabled = a.msImageSmoothingEnabled = a.imageSmoothingEnabled = h.smoothImage;
          if (0 === h.type) {
            a.fillStyle = f ? "#FF4981" : h.style, c ? a.clip(h.path, "evenodd") : a.fill(h.path, "evenodd"), a.fillStyle = "transparent";
          } else {
            if (!c && !f) {
              a.strokeStyle = h.style;
              var e = 1;
              h.strokeProperties && (e = h.strokeProperties.scaleMode, a.lineWidth = h.strokeProperties.thickness, a.lineCap = h.strokeProperties.capsStyle, a.lineJoin = h.strokeProperties.jointsStyle, a.miterLimit = h.strokeProperties.miterLimit);
              var g = a.lineWidth;
              (g = 1 === g || 3 === g) && a.translate(.5, .5);
              a.flashStroke(h.path, e);
              g && a.translate(-.5, -.5);
              a.strokeStyle = "transparent";
            }
          }
        }
      };
      d.prototype._deserializePaths = function(b, c, f) {
        n(b ? !this._paths : this._paths);
        if (this._paths) {
          return this._paths;
        }
        f = this._paths = [];
        for (var h = null, e = null, l = 0, k = 0, p, r, v = !1, m = 0, y = 0, u = b.commands, w = b.coordinates, B = b.styles, A = B.position = 0, C = b.commandsPosition, D = 0;D < C;D++) {
          switch(p = u[D], p) {
            case 9:
              n(A <= b.coordinatesPosition - 2);
              v && h && (h.lineTo(m, y), e && e.lineTo(m, y));
              v = !0;
              l = m = w[A++] / 20;
              k = y = w[A++] / 20;
              h && h.moveTo(l, k);
              e && e.moveTo(l, k);
              break;
            case 10:
              n(A <= b.coordinatesPosition - 2);
              l = w[A++] / 20;
              k = w[A++] / 20;
              h && h.lineTo(l, k);
              e && e.lineTo(l, k);
              break;
            case 11:
              n(A <= b.coordinatesPosition - 4);
              p = w[A++] / 20;
              r = w[A++] / 20;
              l = w[A++] / 20;
              k = w[A++] / 20;
              h && h.quadraticCurveTo(p, r, l, k);
              e && e.quadraticCurveTo(p, r, l, k);
              break;
            case 12:
              n(A <= b.coordinatesPosition - 6);
              p = w[A++] / 20;
              r = w[A++] / 20;
              var z = w[A++] / 20, E = w[A++] / 20, l = w[A++] / 20, k = w[A++] / 20;
              h && h.bezierCurveTo(p, r, z, E, l, k);
              e && e.bezierCurveTo(p, r, z, E, l, k);
              break;
            case 1:
              n(4 <= B.bytesAvailable);
              h = this._createPath(0, g.ColorUtilities.rgbaToCSSStyle(B.readUnsignedInt()), !1, null, l, k);
              break;
            case 3:
              p = this._readBitmap(B, c);
              h = this._createPath(0, p.style, p.smoothImage, null, l, k);
              break;
            case 2:
              h = this._createPath(0, this._readGradient(B, c), !1, null, l, k);
              break;
            case 4:
              h = null;
              break;
            case 5:
              e = g.ColorUtilities.rgbaToCSSStyle(B.readUnsignedInt());
              B.position += 1;
              p = B.readByte();
              r = d.LINE_CAPS_STYLES[B.readByte()];
              z = d.LINE_JOINTS_STYLES[B.readByte()];
              p = new t(w[A++] / 20, p, r, z, B.readByte());
              e = this._createPath(1, e, !1, p, l, k);
              break;
            case 6:
              e = this._createPath(2, this._readGradient(B, c), !1, null, l, k);
              break;
            case 7:
              p = this._readBitmap(B, c);
              e = this._createPath(2, p.style, p.smoothImage, null, l, k);
              break;
            case 8:
              e = null;
              break;
            default:
              a("Invalid command " + p + " encountered at index" + D + " of " + C);
          }
        }
        n(0 === B.bytesAvailable);
        n(D === C);
        n(A === b.coordinatesPosition);
        v && h && (h.lineTo(m, y), e && e.lineTo(m, y));
        this._pathData = null;
        return f;
      };
      d.prototype._createPath = function(a, b, d, c, f, h) {
        a = new l(a, b, d, c);
        this._paths.push(a);
        a.path.moveTo(f, h);
        return a.path;
      };
      d.prototype._readMatrix = function(a) {
        return new b(a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat());
      };
      d.prototype._readGradient = function(a, b) {
        n(34 <= a.bytesAvailable);
        var d = a.readUnsignedByte(), c = 2 * a.readShort() / 255;
        n(-1 <= c && 1 >= c);
        var f = this._readMatrix(a), d = 16 === d ? b.createLinearGradient(-1, 0, 1, 0) : b.createRadialGradient(c, 0, 0, 0, 0, 1);
        d.setTransform && d.setTransform(f.toSVGMatrix());
        f = a.readUnsignedByte();
        for (c = 0;c < f;c++) {
          var h = a.readUnsignedByte() / 255, e = g.ColorUtilities.rgbaToCSSStyle(a.readUnsignedInt());
          d.addColorStop(h, e);
        }
        a.position += 2;
        return d;
      };
      d.prototype._readBitmap = function(a, b) {
        n(30 <= a.bytesAvailable);
        var d = a.readUnsignedInt(), c = this._readMatrix(a), f = a.readBoolean() ? "repeat" : "no-repeat", h = a.readBoolean();
        (d = this._textures[d]) ? (f = b.createPattern(d.renderSource, f), f.setTransform(c.toSVGMatrix())) : f = null;
        return{style:f, smoothImage:h};
      };
      d.prototype._renderFallback = function(a) {
        this.fillStyle || (this.fillStyle = g.ColorStyle.randomStyle());
        var b = this._bounds;
        a.save();
        a.beginPath();
        a.lineWidth = 2;
        a.fillStyle = this.fillStyle;
        a.fillRect(b.x, b.y, b.w, b.h);
        a.restore();
      };
      d.LINE_CAPS_STYLES = ["round", "butt", "square"];
      d.LINE_JOINTS_STYLES = ["round", "bevel", "miter"];
      return d;
    }(y);
    m.RenderableShape = r;
    v = function(f) {
      function d() {
        f.apply(this, arguments);
        this._flags = 7340048;
        this._morphPaths = Object.create(null);
      }
      __extends(d, f);
      d.prototype._deserializePaths = function(b, d, f) {
        if (this._morphPaths[f]) {
          return this._morphPaths[f];
        }
        var h = this._morphPaths[f] = [], l = null, k = null, p = 0, v = 0, m, y, u = !1, w = 0, P = 0, U = b.commands, B = b.coordinates, A = b.morphCoordinates, C = b.styles, D = b.morphStyles;
        C.position = 0;
        for (var z = D.position = 0, E = b.commandsPosition, G = 0;G < E;G++) {
          switch(m = U[G], m) {
            case 9:
              n(z <= b.coordinatesPosition - 2);
              u && l && (l.lineTo(w, P), k && k.lineTo(w, P));
              u = !0;
              p = w = e(B[z], A[z++], f) / 20;
              v = P = e(B[z], A[z++], f) / 20;
              l && l.moveTo(p, v);
              k && k.moveTo(p, v);
              break;
            case 10:
              n(z <= b.coordinatesPosition - 2);
              p = e(B[z], A[z++], f) / 20;
              v = e(B[z], A[z++], f) / 20;
              l && l.lineTo(p, v);
              k && k.lineTo(p, v);
              break;
            case 11:
              n(z <= b.coordinatesPosition - 4);
              m = e(B[z], A[z++], f) / 20;
              y = e(B[z], A[z++], f) / 20;
              p = e(B[z], A[z++], f) / 20;
              v = e(B[z], A[z++], f) / 20;
              l && l.quadraticCurveTo(m, y, p, v);
              k && k.quadraticCurveTo(m, y, p, v);
              break;
            case 12:
              n(z <= b.coordinatesPosition - 6);
              m = e(B[z], A[z++], f) / 20;
              y = e(B[z], A[z++], f) / 20;
              var F = e(B[z], A[z++], f) / 20, H = e(B[z], A[z++], f) / 20, p = e(B[z], A[z++], f) / 20, v = e(B[z], A[z++], f) / 20;
              l && l.bezierCurveTo(m, y, F, H, p, v);
              k && k.bezierCurveTo(m, y, F, H, p, v);
              break;
            case 1:
              n(4 <= C.bytesAvailable);
              l = this._createMorphPath(0, f, g.ColorUtilities.rgbaToCSSStyle(c(C.readUnsignedInt(), D.readUnsignedInt(), f)), !1, null, p, v);
              break;
            case 3:
              m = this._readMorphBitmap(C, D, f, d);
              l = this._createMorphPath(0, f, m.style, m.smoothImage, null, p, v);
              break;
            case 2:
              m = this._readMorphGradient(C, D, f, d);
              l = this._createMorphPath(0, f, m, !1, null, p, v);
              break;
            case 4:
              l = null;
              break;
            case 5:
              m = e(B[z], A[z++], f) / 20;
              k = g.ColorUtilities.rgbaToCSSStyle(c(C.readUnsignedInt(), D.readUnsignedInt(), f));
              C.position += 1;
              y = C.readByte();
              F = r.LINE_CAPS_STYLES[C.readByte()];
              H = r.LINE_JOINTS_STYLES[C.readByte()];
              m = new t(m, y, F, H, C.readByte());
              k = this._createMorphPath(1, f, k, !1, m, p, v);
              break;
            case 6:
              m = this._readMorphGradient(C, D, f, d);
              k = this._createMorphPath(2, f, m, !1, null, p, v);
              break;
            case 7:
              m = this._readMorphBitmap(C, D, f, d);
              k = this._createMorphPath(2, f, m.style, m.smoothImage, null, p, v);
              break;
            case 8:
              k = null;
              break;
            default:
              a("Invalid command " + m + " encountered at index" + G + " of " + E);
          }
        }
        n(0 === C.bytesAvailable);
        n(G === E);
        n(z === b.coordinatesPosition);
        u && l && (l.lineTo(w, P), k && k.lineTo(w, P));
        return h;
      };
      d.prototype._createMorphPath = function(a, b, d, c, f, h, e) {
        a = new l(a, d, c, f);
        this._morphPaths[b].push(a);
        a.path.moveTo(h, e);
        return a.path;
      };
      d.prototype._readMorphMatrix = function(a, d, c) {
        return new b(e(a.readFloat(), d.readFloat(), c), e(a.readFloat(), d.readFloat(), c), e(a.readFloat(), d.readFloat(), c), e(a.readFloat(), d.readFloat(), c), e(a.readFloat(), d.readFloat(), c), e(a.readFloat(), d.readFloat(), c));
      };
      d.prototype._readMorphGradient = function(a, b, d, f) {
        n(34 <= a.bytesAvailable);
        var h = a.readUnsignedByte(), l = 2 * a.readShort() / 255;
        n(-1 <= l && 1 >= l);
        var k = this._readMorphMatrix(a, b, d);
        f = 16 === h ? f.createLinearGradient(-1, 0, 1, 0) : f.createRadialGradient(l, 0, 0, 0, 0, 1);
        f.setTransform && f.setTransform(k.toSVGMatrix());
        k = a.readUnsignedByte();
        for (h = 0;h < k;h++) {
          var l = e(a.readUnsignedByte() / 255, b.readUnsignedByte() / 255, d), p = c(a.readUnsignedInt(), b.readUnsignedInt(), d), p = g.ColorUtilities.rgbaToCSSStyle(p);
          f.addColorStop(l, p);
        }
        a.position += 2;
        return f;
      };
      d.prototype._readMorphBitmap = function(a, b, d, c) {
        n(30 <= a.bytesAvailable);
        var f = a.readUnsignedInt();
        b = this._readMorphMatrix(a, b, d);
        d = a.readBoolean() ? "repeat" : "no-repeat";
        a = a.readBoolean();
        f = this._textures[f];
        n(f._canvas);
        c = c.createPattern(f._canvas, d);
        c.setTransform(b.toSVGMatrix());
        return{style:c, smoothImage:a};
      };
      return d;
    }(r);
    m.RenderableMorphShape = v;
    var u = function() {
      function a() {
        this.align = this.leading = this.descent = this.ascent = this.width = this.y = this.x = 0;
        this.runs = [];
      }
      a.prototype.addRun = function(b, c, e, g) {
        if (e) {
          a._measureContext.font = b;
          var l = a._measureContext.measureText(e).width | 0;
          this.runs.push(new h(b, c, e, l, g));
          this.width += l;
        }
      };
      a.prototype.wrap = function(b) {
        var c = [this], e = this.runs, g = this;
        g.width = 0;
        g.runs = [];
        for (var l = a._measureContext, n = 0;n < e.length;n++) {
          var k = e[n], p = k.text;
          k.text = "";
          k.width = 0;
          l.font = k.font;
          for (var r = b, t = p.split(/[\s.-]/), m = 0, v = 0;v < t.length;v++) {
            var y = t[v], u = p.substr(m, y.length + 1), w = l.measureText(u).width | 0;
            if (w > r) {
              do {
                if (k.text && (g.runs.push(k), g.width += k.width, k = new h(k.font, k.fillStyle, "", 0, k.underline), r = new a, r.y = g.y + g.descent + g.leading + g.ascent | 0, r.ascent = g.ascent, r.descent = g.descent, r.leading = g.leading, r.align = g.align, c.push(r), g = r), r = b - w, 0 > r) {
                  var w = u.length, B, A;
                  do {
                    w--;
                    if (1 > w) {
                      throw Error("Shall never happen: bad maxWidth?");
                    }
                    B = u.substr(0, w);
                    A = l.measureText(B).width | 0;
                  } while (A > b);
                  k.text = B;
                  k.width = A;
                  u = u.substr(w);
                  w = l.measureText(u).width | 0;
                }
              } while (0 > r);
            } else {
              r -= w;
            }
            k.text += u;
            k.width += w;
            m += y.length + 1;
          }
          g.runs.push(k);
          g.width += k.width;
        }
        return c;
      };
      a._measureContext = document.createElement("canvas").getContext("2d");
      return a;
    }();
    m.TextLine = u;
    var h = function() {
      return function(a, b, c, h, e) {
        void 0 === a && (a = "");
        void 0 === b && (b = "");
        void 0 === c && (c = "");
        void 0 === h && (h = 0);
        void 0 === e && (e = !1);
        this.font = a;
        this.fillStyle = b;
        this.text = c;
        this.width = h;
        this.underline = e;
      };
    }();
    m.TextRun = h;
    v = function(a) {
      function d(d) {
        a.call(this);
        this._flags = 1048592;
        this.properties = {};
        this._textBounds = d.clone();
        this._textRunData = null;
        this._plainText = "";
        this._borderColor = this._backgroundColor = 0;
        this._matrix = b.createIdentity();
        this._coords = null;
        this._scrollV = 1;
        this._scrollH = 0;
        this.textRect = d.clone();
        this.lines = [];
        this.setBounds(d);
      }
      __extends(d, a);
      d.prototype.setBounds = function(b) {
        a.prototype.setBounds.call(this, b);
        this._textBounds.set(b);
        this.textRect.setElements(b.x + 2, b.y + 2, b.w - 2, b.h - 2);
      };
      d.prototype.setContent = function(a, b, d, c) {
        this._textRunData = b;
        this._plainText = a;
        this._matrix.set(d);
        this._coords = c;
        this.lines = [];
      };
      d.prototype.setStyle = function(a, b, d, c) {
        this._backgroundColor = a;
        this._borderColor = b;
        this._scrollV = d;
        this._scrollH = c;
      };
      d.prototype.reflow = function(a, b) {
        var d = this._textRunData;
        if (d) {
          for (var c = this._bounds, f = c.w - 4, h = this._plainText, e = this.lines, l = new u, k = 0, n = 0, p = 0, r = 0, t = 0, m = -1;d.position < d.length;) {
            var v = d.readInt(), y = d.readInt(), w = d.readInt(), D = d.readUTF(), z = d.readInt(), E = d.readInt(), G = d.readInt();
            z > p && (p = z);
            E > r && (r = E);
            G > t && (t = G);
            z = d.readBoolean();
            E = "";
            d.readBoolean() && (E += "italic ");
            z && (E += "bold ");
            w = E + w + "px " + D;
            D = d.readInt();
            D = g.ColorUtilities.rgbToHex(D);
            z = d.readInt();
            -1 === m && (m = z);
            d.readBoolean();
            d.readInt();
            d.readInt();
            d.readInt();
            d.readInt();
            d.readInt();
            for (var z = d.readBoolean(), F = "", E = !1;!E;v++) {
              E = v >= y - 1;
              G = h[v];
              if ("\r" !== G && "\n" !== G && (F += G, v < h.length - 1)) {
                continue;
              }
              l.addRun(w, D, F, z);
              if (l.runs.length) {
                e.length && (k += t);
                k += p;
                l.y = k | 0;
                k += r;
                l.ascent = p;
                l.descent = r;
                l.leading = t;
                l.align = m;
                if (b && l.width > f) {
                  for (l = l.wrap(f), F = 0;F < l.length;F++) {
                    var H = l[F], k = H.y + H.descent + H.leading;
                    e.push(H);
                    H.width > n && (n = H.width);
                  }
                } else {
                  e.push(l), l.width > n && (n = l.width);
                }
                l = new u;
              } else {
                k += p + r + t;
              }
              F = "";
              if (E) {
                t = r = p = 0;
                m = -1;
                break;
              }
              "\r" === G && "\n" === h[v + 1] && v++;
            }
            l.addRun(w, D, F, z);
          }
          d = h[h.length - 1];
          "\r" !== d && "\n" !== d || e.push(l);
          d = this.textRect;
          d.w = n;
          d.h = k;
          if (a) {
            if (!b) {
              f = n;
              n = c.w;
              switch(a) {
                case 1:
                  d.x = n - (f + 4) >> 1;
                  break;
                case 3:
                  d.x = n - (f + 4);
              }
              this._textBounds.setElements(d.x - 2, d.y - 2, d.w + 4, d.h + 4);
            }
            c.h = k + 4;
          } else {
            this._textBounds = c;
          }
          for (v = 0;v < e.length;v++) {
            if (c = e[v], c.width < f) {
              switch(c.align) {
                case 1:
                  c.x = f - c.width | 0;
                  break;
                case 2:
                  c.x = (f - c.width) / 2 | 0;
              }
            }
          }
          this.invalidate();
        }
      };
      d.roundBoundPoints = function(a) {
        n(a === d.absoluteBoundPoints);
        for (var b = 0;b < a.length;b++) {
          var c = a[b];
          c.x = Math.floor(c.x + .1) + .5;
          c.y = Math.floor(c.y + .1) + .5;
        }
      };
      d.prototype.render = function(a) {
        a.save();
        var c = this._textBounds;
        this._backgroundColor && (a.fillStyle = g.ColorUtilities.rgbaToCSSStyle(this._backgroundColor), a.fillRect(c.x, c.y, c.w, c.h));
        if (this._borderColor) {
          a.strokeStyle = g.ColorUtilities.rgbaToCSSStyle(this._borderColor);
          a.lineCap = "square";
          a.lineWidth = 1;
          var f = d.absoluteBoundPoints, h = a.currentTransform;
          h ? (c = c.clone(), (new b(h.a, h.b, h.c, h.d, h.e, h.f)).transformRectangle(c, f), a.setTransform(1, 0, 0, 1, 0, 0)) : (f[0].x = c.x, f[0].y = c.y, f[1].x = c.x + c.w, f[1].y = c.y, f[2].x = c.x + c.w, f[2].y = c.y + c.h, f[3].x = c.x, f[3].y = c.y + c.h);
          d.roundBoundPoints(f);
          c = new Path2D;
          c.moveTo(f[0].x, f[0].y);
          c.lineTo(f[1].x, f[1].y);
          c.lineTo(f[2].x, f[2].y);
          c.lineTo(f[3].x, f[3].y);
          c.lineTo(f[0].x, f[0].y);
          a.stroke(c);
          h && a.setTransform(h.a, h.b, h.c, h.d, h.e, h.f);
        }
        this._coords ? this._renderChars(a) : this._renderLines(a);
        a.restore();
      };
      d.prototype._renderChars = function(a) {
        if (this._matrix) {
          var b = this._matrix;
          a.transform(b.a, b.b, b.c, b.d, b.tx, b.ty);
        }
        for (var b = this.lines, d = this._coords, c = d.position = 0;c < b.length;c++) {
          for (var f = b[c].runs, h = 0;h < f.length;h++) {
            var e = f[h];
            a.font = e.font;
            a.fillStyle = e.fillStyle;
            for (var e = e.text, g = 0;g < e.length;g++) {
              var l = d.readInt() / 20, k = d.readInt() / 20;
              a.fillText(e[g], l, k);
            }
          }
        }
      };
      d.prototype._renderLines = function(a) {
        var b = this._textBounds;
        a.beginPath();
        a.rect(b.x + 2, b.y + 2, b.w - 4, b.h - 4);
        a.clip();
        a.translate(b.x - this._scrollH + 2, b.y + 2);
        for (var d = this.lines, c = this._scrollV, f = 0, h = 0;h < d.length;h++) {
          var e = d[h], g = e.x, l = e.y;
          if (h + 1 < c) {
            f = l + e.descent + e.leading;
          } else {
            l -= f;
            if (h + 1 - c && l > b.h) {
              break;
            }
            for (var k = e.runs, n = 0;n < k.length;n++) {
              var p = k[n];
              a.font = p.font;
              a.fillStyle = p.fillStyle;
              p.underline && a.fillRect(g, l + e.descent / 2 | 0, p.width, 1);
              a.textAlign = "left";
              a.textBaseline = "alphabetic";
              a.fillText(p.text, g, l);
              g += p.width;
            }
          }
        }
      };
      d.absoluteBoundPoints = [new w(0, 0), new w(0, 0), new w(0, 0), new w(0, 0)];
      return d;
    }(y);
    m.RenderableText = v;
    y = function(a) {
      function b(d, c) {
        a.call(this);
        this._flags = 3145728;
        this.properties = {};
        this.setBounds(new k(0, 0, d, c));
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "text", {get:function() {
        return this._text;
      }, set:function(a) {
        this._text = a;
      }, enumerable:!0, configurable:!0});
      b.prototype.render = function(a, b, d) {
        a.save();
        a.textBaseline = "top";
        a.fillStyle = "white";
        a.fillText(this.text, 0, 0);
        a.restore();
      };
      return b;
    }(y);
    m.Label = y;
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    var e = g.ColorUtilities.clampByte, c = g.Debug.assert, w = function() {
      return function() {
      };
    }();
    m.Filter = w;
    var k = function(b) {
      function a(a, c, e) {
        b.call(this);
        this.blurX = a;
        this.blurY = c;
        this.quality = e;
      }
      __extends(a, b);
      return a;
    }(w);
    m.BlurFilter = k;
    k = function(b) {
      function a(a, c, e, g, l, k, r, m, h, f, d) {
        b.call(this);
        this.alpha = a;
        this.angle = c;
        this.blurX = e;
        this.blurY = g;
        this.color = l;
        this.distance = k;
        this.hideObject = r;
        this.inner = m;
        this.knockout = h;
        this.quality = f;
        this.strength = d;
      }
      __extends(a, b);
      return a;
    }(w);
    m.DropshadowFilter = k;
    w = function(b) {
      function a(a, c, e, g, l, k, r, m) {
        b.call(this);
        this.alpha = a;
        this.blurX = c;
        this.blurY = e;
        this.color = g;
        this.inner = l;
        this.knockout = k;
        this.quality = r;
        this.strength = m;
      }
      __extends(a, b);
      return a;
    }(w);
    m.GlowFilter = w;
    (function(b) {
      b[b.Unknown = 0] = "Unknown";
      b[b.Identity = 1] = "Identity";
    })(m.ColorMatrixType || (m.ColorMatrixType = {}));
    w = function() {
      function b(a) {
        c(20 === a.length);
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
          return!0;
        }
        var a = this._data;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 1 == a[15] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      b.createIdentity = function() {
        var a = new b([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]);
        a._type = 1;
        return a;
      };
      b.prototype.setMultipliersAndOffsets = function(a, b, c, e, g, l, k, r) {
        for (var m = this._data, h = 0;h < m.length;h++) {
          m[h] = 0;
        }
        m[0] = a;
        m[5] = b;
        m[10] = c;
        m[15] = e;
        m[16] = g / 255;
        m[17] = l / 255;
        m[18] = k / 255;
        m[19] = r / 255;
        this._type = 0;
      };
      b.prototype.transformRGBA = function(a) {
        var b = a >> 24 & 255, c = a >> 16 & 255, g = a >> 8 & 255, k = a & 255, l = this._data;
        a = e(b * l[0] + c * l[1] + g * l[2] + k * l[3] + 255 * l[16]);
        var t = e(b * l[4] + c * l[5] + g * l[6] + k * l[7] + 255 * l[17]), r = e(b * l[8] + c * l[9] + g * l[10] + k * l[11] + 255 * l[18]), b = e(b * l[12] + c * l[13] + g * l[14] + k * l[15] + 255 * l[19]);
        return a << 24 | t << 16 | r << 8 | b;
      };
      b.prototype.multiply = function(a) {
        if (!(a._type & 1)) {
          var b = this._data, c = a._data;
          a = b[0];
          var e = b[1], g = b[2], l = b[3], k = b[4], r = b[5], m = b[6], h = b[7], f = b[8], d = b[9], q = b[10], x = b[11], s = b[12], O = b[13], w = b[14], V = b[15], fa = b[16], ga = b[17], ha = b[18], ia = b[19], W = c[0], X = c[1], P = c[2], U = c[3], B = c[4], A = c[5], C = c[6], D = c[7], z = c[8], E = c[9], G = c[10], F = c[11], H = c[12], I = c[13], J = c[14], K = c[15], L = c[16], M = c[17], N = c[18], c = c[19];
          b[0] = a * W + k * X + f * P + s * U;
          b[1] = e * W + r * X + d * P + O * U;
          b[2] = g * W + m * X + q * P + w * U;
          b[3] = l * W + h * X + x * P + V * U;
          b[4] = a * B + k * A + f * C + s * D;
          b[5] = e * B + r * A + d * C + O * D;
          b[6] = g * B + m * A + q * C + w * D;
          b[7] = l * B + h * A + x * C + V * D;
          b[8] = a * z + k * E + f * G + s * F;
          b[9] = e * z + r * E + d * G + O * F;
          b[10] = g * z + m * E + q * G + w * F;
          b[11] = l * z + h * E + x * G + V * F;
          b[12] = a * H + k * I + f * J + s * K;
          b[13] = e * H + r * I + d * J + O * K;
          b[14] = g * H + m * I + q * J + w * K;
          b[15] = l * H + h * I + x * J + V * K;
          b[16] = a * L + k * M + f * N + s * c + fa;
          b[17] = e * L + r * M + d * N + O * c + ga;
          b[18] = g * L + m * M + q * N + w * c + ha;
          b[19] = l * L + h * M + x * N + V * c + ia;
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
          return!1;
        }
        if (this._type === a._type && 1 === this._type) {
          return!0;
        }
        var b = this._data;
        a = a._data;
        for (var c = 0;20 > c;c++) {
          if (.001 < Math.abs(b[c] - a[c])) {
            return!1;
          }
        }
        return!0;
      };
      b.prototype.toSVGFilterMatrix = function() {
        var a = this._data;
        return[a[0], a[4], a[8], a[12], a[16], a[1], a[5], a[9], a[13], a[17], a[2], a[6], a[10], a[14], a[18], a[3], a[7], a[11], a[15], a[19]].join(" ");
      };
      return b;
    }();
    m.ColorMatrix = w;
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      function c(a, b) {
        return-1 !== a.indexOf(b, this.length - b.length);
      }
      var w = m.Geometry.Point3D, k = m.Geometry.Matrix3D, b = m.Geometry.degreesToRadian, a = g.Debug.assert, n = g.Debug.unexpected, p = g.Debug.notImplemented;
      e.SHADER_ROOT = "shaders/";
      var y = function() {
        function v(b, c) {
          this._fillColor = g.Color.Red;
          this._surfaceRegionCache = new g.LRUList;
          this.modelViewProjectionMatrix = k.createIdentity();
          this._canvas = b;
          this._options = c;
          this.gl = b.getContext("experimental-webgl", {preserveDrawingBuffer:!1, antialias:!0, stencil:!0, premultipliedAlpha:!1});
          a(this.gl, "Cannot create WebGL context.");
          this._programCache = Object.create(null);
          this._resize();
          this.gl.pixelStorei(this.gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, c.unpackPremultiplyAlpha ? this.gl.ONE : this.gl.ZERO);
          this._backgroundColor = g.Color.Black;
          this._geometry = new e.WebGLGeometry(this);
          this._tmpVertices = e.Vertex.createEmptyVertices(e.Vertex, 64);
          this._maxSurfaces = c.maxSurfaces;
          this._maxSurfaceSize = c.maxSurfaceSize;
          this.gl.blendFunc(this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
          this.gl.enable(this.gl.BLEND);
          this.modelViewProjectionMatrix = k.create2DProjection(this._w, this._h, 2E3);
          var n = this;
          this._surfaceRegionAllocator = new m.SurfaceRegionAllocator.SimpleAllocator(function() {
            var a = n._createTexture();
            return new e.WebGLSurface(1024, 1024, a);
          });
        }
        Object.defineProperty(v.prototype, "surfaces", {get:function() {
          return this._surfaceRegionAllocator.surfaces;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(v.prototype, "fillStyle", {set:function(a) {
          this._fillColor.set(g.Color.parseColor(a));
        }, enumerable:!0, configurable:!0});
        v.prototype.setBlendMode = function(a) {
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
        v.prototype.setBlendOptions = function() {
          this.gl.blendFunc(this._options.sourceBlendFactor, this._options.destinationBlendFactor);
        };
        v.glSupportedBlendMode = function(a) {
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
        v.prototype.create2DProjectionMatrix = function() {
          return k.create2DProjection(this._w, this._h, -this._w);
        };
        v.prototype.createPerspectiveMatrix = function(a, c, e) {
          e = b(e);
          c = k.createPerspective(b(c));
          var g = new w(0, 1, 0), h = new w(0, 0, 0);
          a = new w(0, 0, a);
          a = k.createCameraLookAt(a, h, g);
          a = k.createInverse(a);
          g = k.createIdentity();
          g = k.createMultiply(g, k.createTranslation(-this._w / 2, -this._h / 2));
          g = k.createMultiply(g, k.createScale(1 / this._w, -1 / this._h, .01));
          g = k.createMultiply(g, k.createYRotation(e));
          g = k.createMultiply(g, a);
          return g = k.createMultiply(g, c);
        };
        v.prototype.discardCachedImages = function() {
          2 <= m.traceLevel && m.writer && m.writer.writeLn("Discard Cache");
          for (var a = this._surfaceRegionCache.count / 2 | 0, b = 0;b < a;b++) {
            var c = this._surfaceRegionCache.pop();
            2 <= m.traceLevel && m.writer && m.writer.writeLn("Discard: " + c);
            c.texture.atlas.remove(c.region);
            c.texture = null;
          }
        };
        v.prototype.cacheImage = function(a) {
          var b = this.allocateSurfaceRegion(a.width, a.height);
          2 <= m.traceLevel && m.writer && m.writer.writeLn("Uploading Image: @ " + b.region);
          this._surfaceRegionCache.use(b);
          this.updateSurfaceRegion(a, b);
          return b;
        };
        v.prototype.allocateSurfaceRegion = function(a, b) {
          return this._surfaceRegionAllocator.allocate(a, b);
        };
        v.prototype.updateSurfaceRegion = function(a, b) {
          var c = this.gl;
          c.bindTexture(c.TEXTURE_2D, b.surface.texture);
          c.texSubImage2D(c.TEXTURE_2D, 0, b.region.x, b.region.y, c.RGBA, c.UNSIGNED_BYTE, a);
        };
        v.prototype._resize = function() {
          var a = this.gl;
          this._w = this._canvas.width;
          this._h = this._canvas.height;
          a.viewport(0, 0, this._w, this._h);
          for (var b in this._programCache) {
            this._initializeProgram(this._programCache[b]);
          }
        };
        v.prototype._initializeProgram = function(a) {
          this.gl.useProgram(a);
        };
        v.prototype._createShaderFromFile = function(b) {
          var g = e.SHADER_ROOT + b, k = this.gl;
          b = new XMLHttpRequest;
          b.open("GET", g, !1);
          b.send();
          a(200 === b.status || 0 === b.status, "File : " + g + " not found.");
          if (c(g, ".vert")) {
            g = k.VERTEX_SHADER;
          } else {
            if (c(g, ".frag")) {
              g = k.FRAGMENT_SHADER;
            } else {
              throw "Shader Type: not supported.";
            }
          }
          return this._createShader(g, b.responseText);
        };
        v.prototype.createProgramFromFiles = function() {
          var a = this._programCache["combined.vert-combined.frag"];
          a || (a = this._createProgram([this._createShaderFromFile("combined.vert"), this._createShaderFromFile("combined.frag")]), this._queryProgramAttributesAndUniforms(a), this._initializeProgram(a), this._programCache["combined.vert-combined.frag"] = a);
          return a;
        };
        v.prototype._createProgram = function(a) {
          var b = this.gl, c = b.createProgram();
          a.forEach(function(a) {
            b.attachShader(c, a);
          });
          b.linkProgram(c);
          b.getProgramParameter(c, b.LINK_STATUS) || (n("Cannot link program: " + b.getProgramInfoLog(c)), b.deleteProgram(c));
          return c;
        };
        v.prototype._createShader = function(a, b) {
          var c = this.gl, e = c.createShader(a);
          c.shaderSource(e, b);
          c.compileShader(e);
          return c.getShaderParameter(e, c.COMPILE_STATUS) ? e : (n("Cannot compile shader: " + c.getShaderInfoLog(e)), c.deleteShader(e), null);
        };
        v.prototype._createTexture = function() {
          var a = this.gl, b = a.createTexture();
          a.bindTexture(a.TEXTURE_2D, b);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_S, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_T, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MIN_FILTER, a.LINEAR);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MAG_FILTER, a.LINEAR);
          a.texImage2D(a.TEXTURE_2D, 0, a.RGBA, 1024, 1024, 0, a.RGBA, a.UNSIGNED_BYTE, null);
          return b;
        };
        v.prototype._createFramebuffer = function(a) {
          var b = this.gl, c = b.createFramebuffer();
          b.bindFramebuffer(b.FRAMEBUFFER, c);
          b.framebufferTexture2D(b.FRAMEBUFFER, b.COLOR_ATTACHMENT0, b.TEXTURE_2D, a, 0);
          b.bindFramebuffer(b.FRAMEBUFFER, null);
          return c;
        };
        v.prototype._queryProgramAttributesAndUniforms = function(a) {
          a.uniforms = {};
          a.attributes = {};
          for (var b = this.gl, c = 0, e = b.getProgramParameter(a, b.ACTIVE_ATTRIBUTES);c < e;c++) {
            var h = b.getActiveAttrib(a, c);
            a.attributes[h.name] = h;
            h.location = b.getAttribLocation(a, h.name);
          }
          c = 0;
          for (e = b.getProgramParameter(a, b.ACTIVE_UNIFORMS);c < e;c++) {
            h = b.getActiveUniform(a, c), a.uniforms[h.name] = h, h.location = b.getUniformLocation(a, h.name);
          }
        };
        Object.defineProperty(v.prototype, "target", {set:function(a) {
          var b = this.gl;
          a ? (b.viewport(0, 0, a.w, a.h), b.bindFramebuffer(b.FRAMEBUFFER, a.framebuffer)) : (b.viewport(0, 0, this._w, this._h), b.bindFramebuffer(b.FRAMEBUFFER, null));
        }, enumerable:!0, configurable:!0});
        v.prototype.clear = function(a) {
          a = this.gl;
          a.clearColor(0, 0, 0, 0);
          a.clear(a.COLOR_BUFFER_BIT);
        };
        v.prototype.clearTextureRegion = function(a, b) {
          void 0 === b && (b = g.Color.None);
          var c = this.gl, e = a.region;
          this.target = a.surface;
          c.enable(c.SCISSOR_TEST);
          c.scissor(e.x, e.y, e.w, e.h);
          c.clearColor(b.r, b.g, b.b, b.a);
          c.clear(c.COLOR_BUFFER_BIT | c.DEPTH_BUFFER_BIT);
          c.disable(c.SCISSOR_TEST);
        };
        v.prototype.sizeOf = function(a) {
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
        v.MAX_SURFACES = 8;
        return v;
      }();
      e.WebGLContext = y;
    })(m.WebGL || (m.WebGL = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(g, m) {
  function e() {
    this.constructor = g;
  }
  for (var c in m) {
    m.hasOwnProperty(c) && (g[c] = m[c]);
  }
  e.prototype = m.prototype;
  g.prototype = new e;
};
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.Debug.assert, w = function(b) {
        function a() {
          b.apply(this, arguments);
        }
        __extends(a, b);
        a.prototype.ensureVertexCapacity = function(a) {
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8 * a);
        };
        a.prototype.writeVertex = function(a, b) {
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8);
          this.writeVertexUnsafe(a, b);
        };
        a.prototype.writeVertexUnsafe = function(a, b) {
          var c = this._offset >> 2;
          this._f32[c] = a;
          this._f32[c + 1] = b;
          this._offset += 8;
        };
        a.prototype.writeVertex3D = function(a, b, e) {
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 12);
          this.writeVertex3DUnsafe(a, b, e);
        };
        a.prototype.writeVertex3DUnsafe = function(a, b, c) {
          var e = this._offset >> 2;
          this._f32[e] = a;
          this._f32[e + 1] = b;
          this._f32[e + 2] = c;
          this._offset += 12;
        };
        a.prototype.writeTriangleElements = function(a, b, e) {
          c(0 === (this._offset & 1));
          this.ensureCapacity(this._offset + 6);
          var g = this._offset >> 1;
          this._u16[g] = a;
          this._u16[g + 1] = b;
          this._u16[g + 2] = e;
          this._offset += 6;
        };
        a.prototype.ensureColorCapacity = function(a) {
          c(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16 * a);
        };
        a.prototype.writeColorFloats = function(a, b, e, g) {
          c(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16);
          this.writeColorFloatsUnsafe(a, b, e, g);
        };
        a.prototype.writeColorFloatsUnsafe = function(a, b, c, e) {
          var g = this._offset >> 2;
          this._f32[g] = a;
          this._f32[g + 1] = b;
          this._f32[g + 2] = c;
          this._f32[g + 3] = e;
          this._offset += 16;
        };
        a.prototype.writeColor = function() {
          var a = Math.random(), b = Math.random(), e = Math.random(), g = Math.random() / 2;
          c(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 4);
          this._i32[this._offset >> 2] = g << 24 | e << 16 | b << 8 | a;
          this._offset += 4;
        };
        a.prototype.writeColorUnsafe = function(a, b, c, e) {
          this._i32[this._offset >> 2] = e << 24 | c << 16 | b << 8 | a;
          this._offset += 4;
        };
        a.prototype.writeRandomColor = function() {
          this.writeColor();
        };
        return a;
      }(g.ArrayUtilities.ArrayWriter);
      e.BufferWriter = w;
      e.WebGLAttribute = function() {
        return function(b, a, c, e) {
          void 0 === e && (e = !1);
          this.name = b;
          this.size = a;
          this.type = c;
          this.normalized = e;
        };
      }();
      var k = function() {
        function b(a) {
          this.size = 0;
          this.attributes = a;
        }
        b.prototype.initialize = function(a) {
          for (var b = 0, c = 0;c < this.attributes.length;c++) {
            this.attributes[c].offset = b, b += a.sizeOf(this.attributes[c].type) * this.attributes[c].size;
          }
          this.size = b;
        };
        return b;
      }();
      e.WebGLAttributeList = k;
      k = function() {
        function b(a) {
          this._elementOffset = this.triangleCount = 0;
          this.context = a;
          this.array = new w(8);
          this.buffer = a.gl.createBuffer();
          this.elementArray = new w(8);
          this.elementBuffer = a.gl.createBuffer();
        }
        Object.defineProperty(b.prototype, "elementOffset", {get:function() {
          return this._elementOffset;
        }, enumerable:!0, configurable:!0});
        b.prototype.addQuad = function() {
          var a = this._elementOffset;
          this.elementArray.writeTriangleElements(a, a + 1, a + 2);
          this.elementArray.writeTriangleElements(a, a + 2, a + 3);
          this.triangleCount += 2;
          this._elementOffset += 4;
        };
        b.prototype.resetElementOffset = function() {
          this._elementOffset = 0;
        };
        b.prototype.reset = function() {
          this.array.reset();
          this.elementArray.reset();
          this.resetElementOffset();
          this.triangleCount = 0;
        };
        b.prototype.uploadBuffers = function() {
          var a = this.context.gl;
          a.bindBuffer(a.ARRAY_BUFFER, this.buffer);
          a.bufferData(a.ARRAY_BUFFER, this.array.subU8View(), a.DYNAMIC_DRAW);
          a.bindBuffer(a.ELEMENT_ARRAY_BUFFER, this.elementBuffer);
          a.bufferData(a.ELEMENT_ARRAY_BUFFER, this.elementArray.subU8View(), a.DYNAMIC_DRAW);
        };
        return b;
      }();
      e.WebGLGeometry = k;
      k = function(b) {
        function a(a, c, e) {
          b.call(this, a, c, e);
        }
        __extends(a, b);
        a.createEmptyVertices = function(a, b) {
          for (var c = [], e = 0;e < b;e++) {
            c.push(new a(0, 0, 0));
          }
          return c;
        };
        return a;
      }(m.Geometry.Point3D);
      e.Vertex = k;
      (function(b) {
        b[b.ZERO = 0] = "ZERO";
        b[b.ONE = 1] = "ONE";
        b[b.SRC_COLOR = 768] = "SRC_COLOR";
        b[b.ONE_MINUS_SRC_COLOR = 769] = "ONE_MINUS_SRC_COLOR";
        b[b.DST_COLOR = 774] = "DST_COLOR";
        b[b.ONE_MINUS_DST_COLOR = 775] = "ONE_MINUS_DST_COLOR";
        b[b.SRC_ALPHA = 770] = "SRC_ALPHA";
        b[b.ONE_MINUS_SRC_ALPHA = 771] = "ONE_MINUS_SRC_ALPHA";
        b[b.DST_ALPHA = 772] = "DST_ALPHA";
        b[b.ONE_MINUS_DST_ALPHA = 773] = "ONE_MINUS_DST_ALPHA";
        b[b.SRC_ALPHA_SATURATE = 776] = "SRC_ALPHA_SATURATE";
        b[b.CONSTANT_COLOR = 32769] = "CONSTANT_COLOR";
        b[b.ONE_MINUS_CONSTANT_COLOR = 32770] = "ONE_MINUS_CONSTANT_COLOR";
        b[b.CONSTANT_ALPHA = 32771] = "CONSTANT_ALPHA";
        b[b.ONE_MINUS_CONSTANT_ALPHA = 32772] = "ONE_MINUS_CONSTANT_ALPHA";
      })(e.WebGLBlendFactor || (e.WebGLBlendFactor = {}));
    })(m.WebGL || (m.WebGL = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(g) {
    (function(e) {
      var c = function() {
        function c(b, a, e) {
          this.texture = e;
          this.w = b;
          this.h = a;
          this._regionAllocator = new g.RegionAllocator.CompactAllocator(this.w, this.h);
        }
        c.prototype.allocate = function(b, a) {
          var c = this._regionAllocator.allocate(b, a);
          return c ? new w(this, c) : null;
        };
        c.prototype.free = function(b) {
          this._regionAllocator.free(b.region);
        };
        return c;
      }();
      e.WebGLSurface = c;
      var w = function() {
        return function(c, b) {
          this.surface = c;
          this.region = b;
          this.next = this.previous = null;
        };
      }();
      e.WebGLSurfaceRegion = w;
    })(g.WebGL || (g.WebGL = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.Color;
      e.TILE_SIZE = 256;
      e.MIN_UNTILED_SIZE = 256;
      var w = m.Geometry.Matrix, k = m.Geometry.Rectangle, b = function(a) {
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
      }(m.RendererOptions);
      e.WebGLRendererOptions = b;
      var a = function(a) {
        function g(c, k, l) {
          void 0 === l && (l = new b);
          a.call(this, c, k, l);
          this._tmpVertices = e.Vertex.createEmptyVertices(e.Vertex, 64);
          this._cachedTiles = [];
          c = this._context = new e.WebGLContext(this._canvas, l);
          this._updateSize();
          this._brush = new e.WebGLCombinedBrush(c, new e.WebGLGeometry(c));
          this._stencilBrush = new e.WebGLCombinedBrush(c, new e.WebGLGeometry(c));
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
        __extends(g, a);
        g.prototype.resize = function() {
          this._updateSize();
          this.render();
        };
        g.prototype._updateSize = function() {
          this._viewport = new k(0, 0, this._canvas.width, this._canvas.height);
          this._context._resize();
        };
        g.prototype._cacheImageCallback = function(a, b, c) {
          var e = c.w, g = c.h, k = c.x;
          c = c.y;
          this._uploadCanvas.width = e + 2;
          this._uploadCanvas.height = g + 2;
          this._uploadCanvasContext.drawImage(b.canvas, k, c, e, g, 1, 1, e, g);
          this._uploadCanvasContext.drawImage(b.canvas, k, c, e, 1, 1, 0, e, 1);
          this._uploadCanvasContext.drawImage(b.canvas, k, c + g - 1, e, 1, 1, g + 1, e, 1);
          this._uploadCanvasContext.drawImage(b.canvas, k, c, 1, g, 0, 1, 1, g);
          this._uploadCanvasContext.drawImage(b.canvas, k + e - 1, c, 1, g, e + 1, 1, 1, g);
          return a && a.surface ? (this._options.disableSurfaceUploads || this._context.updateSurfaceRegion(this._uploadCanvas, a), a) : this._context.cacheImage(this._uploadCanvas);
        };
        g.prototype._enterClip = function(a, b, c, e) {
          c.flush();
          b = this._context.gl;
          0 === this._clipStack.length && (b.enable(b.STENCIL_TEST), b.clear(b.STENCIL_BUFFER_BIT), b.stencilFunc(b.ALWAYS, 1, 1));
          this._clipStack.push(a);
          b.colorMask(!1, !1, !1, !1);
          b.stencilOp(b.KEEP, b.KEEP, b.INCR);
          c.flush();
          b.colorMask(!0, !0, !0, !0);
          b.stencilFunc(b.NOTEQUAL, 0, this._clipStack.length);
          b.stencilOp(b.KEEP, b.KEEP, b.KEEP);
        };
        g.prototype._leaveClip = function(a, b, c, e) {
          c.flush();
          b = this._context.gl;
          if (a = this._clipStack.pop()) {
            b.colorMask(!1, !1, !1, !1), b.stencilOp(b.KEEP, b.KEEP, b.DECR), c.flush(), b.colorMask(!0, !0, !0, !0), b.stencilFunc(b.NOTEQUAL, 0, this._clipStack.length), b.stencilOp(b.KEEP, b.KEEP, b.KEEP);
          }
          0 === this._clipStack.length && b.disable(b.STENCIL_TEST);
        };
        g.prototype._renderFrame = function(a, b, c, e) {
        };
        g.prototype._renderSurfaces = function(a) {
          var b = this._options, g = this._context, n = this._viewport;
          if (b.drawSurfaces) {
            var p = g.surfaces, g = w.createIdentity();
            if (0 <= b.drawSurface && b.drawSurface < p.length) {
              for (var b = p[b.drawSurface | 0], p = new k(0, 0, b.w, b.h), m = p.clone();m.w > n.w;) {
                m.scale(.5, .5);
              }
              a.drawImage(new e.WebGLSurfaceRegion(b, p), m, c.White, null, g, .2);
            } else {
              m = n.w / 5;
              m > n.h / p.length && (m = n.h / p.length);
              a.fillRectangle(new k(n.w - m, 0, m, n.h), new c(0, 0, 0, .5), g, .1);
              for (var h = 0;h < p.length;h++) {
                var b = p[h], f = new k(n.w - m, h * m, m, m);
                a.drawImage(new e.WebGLSurfaceRegion(b, new k(0, 0, b.w, b.h)), f, c.White, null, g, .2);
              }
            }
            a.flush();
          }
        };
        g.prototype.render = function() {
          var a = this._options, b = this._context.gl;
          this._context.modelViewProjectionMatrix = a.perspectiveCamera ? this._context.createPerspectiveMatrix(a.perspectiveCameraDistance + (a.animateZoom ? .8 * Math.sin(Date.now() / 3E3) : 0), a.perspectiveCameraFOV, a.perspectiveCameraAngle) : this._context.create2DProjectionMatrix();
          var e = this._brush;
          b.clearColor(0, 0, 0, 0);
          b.clear(b.COLOR_BUFFER_BIT | b.DEPTH_BUFFER_BIT);
          e.reset();
          b = this._viewport;
          e.flush();
          a.paintViewport && (e.fillRectangle(b, new c(.5, 0, 0, .25), w.createIdentity(), 0), e.flush());
          this._renderSurfaces(e);
        };
        return g;
      }(m.Renderer);
      e.WebGLRenderer = a;
    })(m.WebGL || (m.WebGL = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.Color, w = m.Geometry.Point, k = m.Geometry.Matrix3D, b = function() {
        function a(b, c, e) {
          this._target = e;
          this._context = b;
          this._geometry = c;
        }
        a.prototype.reset = function() {
          g.Debug.abstractMethod("reset");
        };
        a.prototype.flush = function() {
          g.Debug.abstractMethod("flush");
        };
        Object.defineProperty(a.prototype, "target", {get:function() {
          return this._target;
        }, set:function(a) {
          this._target !== a && this.flush();
          this._target = a;
        }, enumerable:!0, configurable:!0});
        return a;
      }();
      e.WebGLBrush = b;
      (function(a) {
        a[a.FillColor = 0] = "FillColor";
        a[a.FillTexture = 1] = "FillTexture";
        a[a.FillTextureWithColorMatrix = 2] = "FillTextureWithColorMatrix";
      })(e.WebGLCombinedBrushKind || (e.WebGLCombinedBrushKind = {}));
      var a = function(a) {
        function b(e, g, k) {
          a.call(this, e, g, k);
          this.kind = 0;
          this.color = new c(0, 0, 0, 0);
          this.sampler = 0;
          this.coordinate = new w(0, 0);
        }
        __extends(b, a);
        b.initializeAttributeList = function(a) {
          var c = a.gl;
          b.attributeList || (b.attributeList = new e.WebGLAttributeList([new e.WebGLAttribute("aPosition", 3, c.FLOAT), new e.WebGLAttribute("aCoordinate", 2, c.FLOAT), new e.WebGLAttribute("aColor", 4, c.UNSIGNED_BYTE, !0), new e.WebGLAttribute("aKind", 1, c.FLOAT), new e.WebGLAttribute("aSampler", 1, c.FLOAT)]), b.attributeList.initialize(a));
        };
        b.prototype.writeTo = function(a) {
          a = a.array;
          a.ensureAdditionalCapacity();
          a.writeVertex3DUnsafe(this.x, this.y, this.z);
          a.writeVertexUnsafe(this.coordinate.x, this.coordinate.y);
          a.writeColorUnsafe(255 * this.color.r, 255 * this.color.g, 255 * this.color.b, 255 * this.color.a);
          a.writeFloatUnsafe(this.kind);
          a.writeFloatUnsafe(this.sampler);
        };
        return b;
      }(e.Vertex);
      e.WebGLCombinedBrushVertex = a;
      b = function(b) {
        function c(e, g, k) {
          void 0 === k && (k = null);
          b.call(this, e, g, k);
          this._blendMode = 1;
          this._program = e.createProgramFromFiles();
          this._surfaces = [];
          a.initializeAttributeList(this._context);
        }
        __extends(c, b);
        c.prototype.reset = function() {
          this._surfaces = [];
          this._geometry.reset();
        };
        c.prototype.drawImage = function(a, b, e, g, k, n, h) {
          void 0 === n && (n = 0);
          void 0 === h && (h = 1);
          if (!a || !a.surface) {
            return!0;
          }
          b = b.clone();
          this._colorMatrix && (g && this._colorMatrix.equals(g) || this.flush());
          this._colorMatrix = g;
          this._blendMode !== h && (this.flush(), this._blendMode = h);
          h = this._surfaces.indexOf(a.surface);
          0 > h && (8 === this._surfaces.length && this.flush(), this._surfaces.push(a.surface), h = this._surfaces.length - 1);
          var f = c._tmpVertices, d = a.region.clone();
          d.offset(1, 1).resize(-2, -2);
          d.scale(1 / a.surface.w, 1 / a.surface.h);
          k.transformRectangle(b, f);
          for (a = 0;4 > a;a++) {
            f[a].z = n;
          }
          f[0].coordinate.x = d.x;
          f[0].coordinate.y = d.y;
          f[1].coordinate.x = d.x + d.w;
          f[1].coordinate.y = d.y;
          f[2].coordinate.x = d.x + d.w;
          f[2].coordinate.y = d.y + d.h;
          f[3].coordinate.x = d.x;
          f[3].coordinate.y = d.y + d.h;
          for (a = 0;4 > a;a++) {
            n = c._tmpVertices[a], n.kind = g ? 2 : 1, n.color.set(e), n.sampler = h, n.writeTo(this._geometry);
          }
          this._geometry.addQuad();
          return!0;
        };
        c.prototype.fillRectangle = function(a, b, e, g) {
          void 0 === g && (g = 0);
          e.transformRectangle(a, c._tmpVertices);
          for (a = 0;4 > a;a++) {
            e = c._tmpVertices[a], e.kind = 0, e.color.set(b), e.z = g, e.writeTo(this._geometry);
          }
          this._geometry.addQuad();
        };
        c.prototype.flush = function() {
          var b = this._geometry, c = this._program, e = this._context.gl, g;
          b.uploadBuffers();
          e.useProgram(c);
          this._target ? (g = k.create2DProjection(this._target.w, this._target.h, 2E3), g = k.createMultiply(g, k.createScale(1, -1, 1))) : g = this._context.modelViewProjectionMatrix;
          e.uniformMatrix4fv(c.uniforms.uTransformMatrix3D.location, !1, g.asWebGLMatrix());
          this._colorMatrix && (e.uniformMatrix4fv(c.uniforms.uColorMatrix.location, !1, this._colorMatrix.asWebGLMatrix()), e.uniform4fv(c.uniforms.uColorVector.location, this._colorMatrix.asWebGLVector()));
          for (g = 0;g < this._surfaces.length;g++) {
            e.activeTexture(e.TEXTURE0 + g), e.bindTexture(e.TEXTURE_2D, this._surfaces[g].texture);
          }
          e.uniform1iv(c.uniforms["uSampler[0]"].location, [0, 1, 2, 3, 4, 5, 6, 7]);
          e.bindBuffer(e.ARRAY_BUFFER, b.buffer);
          var n = a.attributeList.size, p = a.attributeList.attributes;
          for (g = 0;g < p.length;g++) {
            var h = p[g], f = c.attributes[h.name].location;
            e.enableVertexAttribArray(f);
            e.vertexAttribPointer(f, h.size, h.type, h.normalized, n, h.offset);
          }
          this._context.setBlendOptions();
          this._context.target = this._target;
          e.bindBuffer(e.ELEMENT_ARRAY_BUFFER, b.elementBuffer);
          e.drawElements(e.TRIANGLES, 3 * b.triangleCount, e.UNSIGNED_SHORT, 0);
          this.reset();
        };
        c._tmpVertices = e.Vertex.createEmptyVertices(a, 4);
        c._depth = 1;
        return c;
      }(b);
      e.WebGLCombinedBrush = b;
    })(m.WebGL || (m.WebGL = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      function c() {
        void 0 === this.stackDepth && (this.stackDepth = 0);
        void 0 === this.clipStack ? this.clipStack = [0] : this.clipStack.push(0);
        this.stackDepth++;
        y.call(this);
      }
      function w() {
        this.stackDepth--;
        this.clipStack.pop();
        r.call(this);
      }
      function k() {
        p(!this.buildingClippingRegionDepth);
        l.apply(this, arguments);
      }
      function b() {
        p(m.debugClipping.value || !this.buildingClippingRegionDepth);
        t.apply(this, arguments);
      }
      function a() {
        u.call(this);
      }
      function n() {
        void 0 === this.clipStack && (this.clipStack = [0]);
        this.clipStack[this.clipStack.length - 1]++;
        m.debugClipping.value ? (this.strokeStyle = g.ColorStyle.Pink, this.stroke.apply(this, arguments)) : v.apply(this, arguments);
      }
      var p = g.Debug.assert, y = CanvasRenderingContext2D.prototype.save, v = CanvasRenderingContext2D.prototype.clip, l = CanvasRenderingContext2D.prototype.fill, t = CanvasRenderingContext2D.prototype.stroke, r = CanvasRenderingContext2D.prototype.restore, u = CanvasRenderingContext2D.prototype.beginPath;
      e.notifyReleaseChanged = function() {
        CanvasRenderingContext2D.prototype.save = c;
        CanvasRenderingContext2D.prototype.clip = n;
        CanvasRenderingContext2D.prototype.fill = k;
        CanvasRenderingContext2D.prototype.stroke = b;
        CanvasRenderingContext2D.prototype.restore = w;
        CanvasRenderingContext2D.prototype.beginPath = a;
      };
      CanvasRenderingContext2D.prototype.enterBuildingClippingRegion = function() {
        this.buildingClippingRegionDepth || (this.buildingClippingRegionDepth = 0);
        this.buildingClippingRegionDepth++;
      };
      CanvasRenderingContext2D.prototype.leaveBuildingClippingRegion = function() {
        this.buildingClippingRegionDepth--;
      };
    })(m.Canvas2D || (m.Canvas2D = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      function c(a) {
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
            g.Debug.somewhatImplemented("Blend Mode: " + m.BlendMode[a]);
        }
        return b;
      }
      var w = g.NumberUtilities.clamp;
      navigator.userAgent.indexOf("Firefox");
      var k = function() {
        function a() {
        }
        a._prepareSVGFilters = function() {
          if (!a._svgBlurFilter) {
            var b = document.createElementNS("http://www.w3.org/2000/svg", "svg"), c = document.createElementNS("http://www.w3.org/2000/svg", "defs"), e = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            e.setAttribute("id", "svgBlurFilter");
            var g = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            g.setAttribute("stdDeviation", "0 0");
            e.appendChild(g);
            c.appendChild(e);
            a._svgBlurFilter = g;
            e = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            e.setAttribute("id", "svgDropShadowFilter");
            g = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            g.setAttribute("in", "SourceAlpha");
            g.setAttribute("stdDeviation", "3");
            e.appendChild(g);
            a._svgDropshadowFilterBlur = g;
            g = document.createElementNS("http://www.w3.org/2000/svg", "feOffset");
            g.setAttribute("dx", "0");
            g.setAttribute("dy", "0");
            g.setAttribute("result", "offsetblur");
            e.appendChild(g);
            a._svgDropshadowFilterOffset = g;
            g = document.createElementNS("http://www.w3.org/2000/svg", "feFlood");
            g.setAttribute("flood-color", "rgba(0,0,0,1)");
            e.appendChild(g);
            a._svgDropshadowFilterFlood = g;
            g = document.createElementNS("http://www.w3.org/2000/svg", "feComposite");
            g.setAttribute("in2", "offsetblur");
            g.setAttribute("operator", "in");
            e.appendChild(g);
            var g = document.createElementNS("http://www.w3.org/2000/svg", "feMerge"), k = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            g.appendChild(k);
            k = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            k.setAttribute("in", "SourceGraphic");
            g.appendChild(k);
            e.appendChild(g);
            c.appendChild(e);
            e = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            e.setAttribute("id", "svgColorMatrixFilter");
            g = document.createElementNS("http://www.w3.org/2000/svg", "feColorMatrix");
            g.setAttribute("color-interpolation-filters", "sRGB");
            g.setAttribute("in", "SourceGraphic");
            g.setAttribute("type", "matrix");
            e.appendChild(g);
            c.appendChild(e);
            a._svgColorMatrixFilter = g;
            b.appendChild(c);
            document.documentElement.appendChild(b);
          }
        };
        a._applyColorMatrixFilter = function(b, c) {
          a._prepareSVGFilters();
          a._svgColorMatrixFilter.setAttribute("values", c.toSVGFilterMatrix());
          b.filter = "url(#svgColorMatrixFilter)";
        };
        a._applyFilters = function(b, c, e) {
          function k(a) {
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
          a._removeFilters(c);
          for (var l = 0;l < e.length;l++) {
            var t = e[l];
            if (t instanceof m.BlurFilter) {
              var r = t, t = k(r.quality);
              a._svgBlurFilter.setAttribute("stdDeviation", r.blurX * t + " " + r.blurY * t);
              c.filter = "url(#svgBlurFilter)";
            } else {
              t instanceof m.DropshadowFilter && (r = t, t = k(r.quality), a._svgDropshadowFilterBlur.setAttribute("stdDeviation", r.blurX * t + " " + r.blurY * t), a._svgDropshadowFilterOffset.setAttribute("dx", String(Math.cos(r.angle * Math.PI / 180) * r.distance * b)), a._svgDropshadowFilterOffset.setAttribute("dy", String(Math.sin(r.angle * Math.PI / 180) * r.distance * b)), a._svgDropshadowFilterFlood.setAttribute("flood-color", g.ColorUtilities.rgbaToCSSStyle(r.color << 8 | Math.round(255 * 
              r.alpha))), c.filter = "url(#svgDropShadowFilter)");
            }
          }
        };
        a._removeFilters = function(a) {
          a.filter = "none";
        };
        a._applyColorMatrix = function(b, c) {
          a._removeFilters(b);
          c.isIdentity() ? (b.globalAlpha = 1, b.globalColorMatrix = null) : c.hasOnlyAlphaMultiplier() ? (b.globalAlpha = w(c.alphaMultiplier, 0, 1), b.globalColorMatrix = null) : (b.globalAlpha = 1, a._svgFiltersAreSupported ? (a._applyColorMatrixFilter(b, c), b.globalColorMatrix = null) : b.globalColorMatrix = c);
        };
        a._svgFiltersAreSupported = !!Object.getOwnPropertyDescriptor(CanvasRenderingContext2D.prototype, "filter");
        return a;
      }();
      e.Filters = k;
      var b = function() {
        function a(a, b, c, e) {
          this.surface = a;
          this.region = b;
          this.w = c;
          this.h = e;
        }
        a.prototype.free = function() {
          this.surface.free(this);
        };
        a._ensureCopyCanvasSize = function(b, c) {
          var e;
          if (a._copyCanvasContext) {
            if (e = a._copyCanvasContext.canvas, e.width < b || e.height < c) {
              e.width = g.IntegerUtilities.nearestPowerOfTwo(b), e.height = g.IntegerUtilities.nearestPowerOfTwo(c);
            }
          } else {
            e = document.createElement("canvas"), "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(e), e.width = 512, e.height = 512, a._copyCanvasContext = e.getContext("2d");
          }
        };
        a.prototype.draw = function(b, e, g, k, l, m) {
          this.context.setTransform(1, 0, 0, 1, 0, 0);
          var r, u = 0, h = 0;
          b.context.canvas === this.context.canvas ? (a._ensureCopyCanvasSize(k, l), r = a._copyCanvasContext, r.clearRect(0, 0, k, l), r.drawImage(b.surface.canvas, b.region.x, b.region.y, k, l, 0, 0, k, l), r = r.canvas, h = u = 0) : (r = b.surface.canvas, u = b.region.x, h = b.region.y);
          a: {
            switch(m) {
              case 11:
                b = !0;
                break a;
              default:
                b = !1;
            }
          }
          b && (this.context.save(), this.context.beginPath(), this.context.rect(e, g, k, l), this.context.clip());
          this.context.globalCompositeOperation = c(m);
          this.context.drawImage(r, u, h, k, l, e, g, k, l);
          this.context.globalCompositeOperation = c(1);
          b && this.context.restore();
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
          var b = this.surface.context, c = this.region;
          b.fillStyle = a;
          b.fillRect(c.x, c.y, c.w, c.h);
        };
        a.prototype.clear = function(a) {
          var b = this.surface.context, c = this.region;
          a || (a = c);
          b.clearRect(a.x, a.y, a.w, a.h);
        };
        return a;
      }();
      e.Canvas2DSurfaceRegion = b;
      k = function() {
        function a(a, b) {
          this.canvas = a;
          this.context = a.getContext("2d");
          this.w = a.width;
          this.h = a.height;
          this._regionAllocator = b;
        }
        a.prototype.allocate = function(a, c) {
          var e = this._regionAllocator.allocate(a, c);
          return e ? new b(this, e, a, c) : null;
        };
        a.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return a;
      }();
      e.Canvas2DSurface = k;
    })(m.Canvas2D || (m.Canvas2D = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.Debug.assert, w = g.GFX.Geometry.Rectangle, k = g.GFX.Geometry.Point, b = g.GFX.Geometry.Matrix, a = g.NumberUtilities.clamp, n = g.NumberUtilities.pow2, p = new g.IndentingWriter(!1, dumpLine), y = function() {
        return function(a, b) {
          this.surfaceRegion = a;
          this.scale = b;
        };
      }();
      e.MipMapLevel = y;
      var v = function() {
        function b(a, c, e, h) {
          this._node = c;
          this._levels = [];
          this._surfaceRegionAllocator = e;
          this._size = h;
          this._renderer = a;
        }
        b.prototype.getLevel = function(b) {
          b = Math.max(b.getAbsoluteScaleX(), b.getAbsoluteScaleY());
          var c = 0;
          1 !== b && (c = a(Math.round(Math.log(b) / Math.LN2), -5, 3));
          this._node.hasFlags(2097152) || (c = a(c, -5, 0));
          b = n(c);
          var e = 5 + c, c = this._levels[e];
          if (!c) {
            var h = this._node.getBounds().clone();
            h.scale(b, b);
            h.snap();
            var g = this._surfaceRegionAllocator.allocate(h.w, h.h), k = g.region, c = this._levels[e] = new y(g, b), e = new t(g);
            e.clip.set(k);
            e.matrix.setElements(b, 0, 0, b, k.x - h.x, k.y - h.y);
            e.flags |= 64;
            this._renderer.renderNodeWithState(this._node, e);
            e.free();
          }
          return c;
        };
        return b;
      }();
      e.MipMap = v;
      (function(a) {
        a[a.NonZero = 0] = "NonZero";
        a[a.EvenOdd = 1] = "EvenOdd";
      })(e.FillRule || (e.FillRule = {}));
      var l = function(a) {
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
      }(m.RendererOptions);
      e.Canvas2DRendererOptions = l;
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
      })(e.RenderFlags || (e.RenderFlags = {}));
      w.createMaxI16();
      var t = function(a) {
        function c(d) {
          a.call(this);
          this.clip = w.createEmpty();
          this.clipList = [];
          this.flags = 0;
          this.target = null;
          this.matrix = b.createIdentity();
          this.colorMatrix = m.ColorMatrix.createIdentity();
          c.allocationCount++;
          this.target = d;
        }
        __extends(c, a);
        c.prototype.set = function(a) {
          this.clip.set(a.clip);
          this.target = a.target;
          this.matrix.set(a.matrix);
          this.colorMatrix.set(a.colorMatrix);
          this.flags = a.flags;
          g.ArrayUtilities.copyFrom(this.clipList, a.clipList);
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
      }(m.State);
      e.RenderState = t;
      var r = function() {
        function a() {
          this.culledNodes = this.groups = this.shapes = this._count = 0;
        }
        a.prototype.enter = function(a) {
          this._count++;
          p && (p.enter("> Frame: " + this._count), this._enterTime = performance.now(), this.culledNodes = this.groups = this.shapes = 0);
        };
        a.prototype.leave = function() {
          p && (p.writeLn("Shapes: " + this.shapes + ", Groups: " + this.groups + ", Culled Nodes: " + this.culledNodes), p.writeLn("Elapsed: " + (performance.now() - this._enterTime).toFixed(2)), p.writeLn("Rectangle: " + w.allocationCount + ", Matrix: " + b.allocationCount + ", State: " + t.allocationCount), p.leave("<"));
        };
        return a;
      }();
      e.FrameInfo = r;
      var u = function(a) {
        function f(b, c, e) {
          void 0 === e && (e = new l);
          a.call(this, b, c, e);
          this._visited = 0;
          this._frameInfo = new r;
          this._fontSize = 0;
          this._layers = [];
          if (b instanceof HTMLCanvasElement) {
            var g = b;
            this._viewport = new w(0, 0, g.width, g.height);
            this._target = this._createTarget(g);
          } else {
            this._addLayer("Background Layer");
            e = this._addLayer("Canvas Layer");
            g = document.createElement("canvas");
            e.appendChild(g);
            this._viewport = new w(0, 0, b.scrollWidth, b.scrollHeight);
            var k = this;
            c.addEventListener(1, function() {
              k._onStageBoundsChanged(g);
            });
            this._onStageBoundsChanged(g);
          }
          f._prepareSurfaceAllocators();
        }
        __extends(f, a);
        f.prototype._addLayer = function(a) {
          a = document.createElement("div");
          a.style.position = "absolute";
          a.style.overflow = "hidden";
          a.style.width = "100%";
          a.style.height = "100%";
          this._container.appendChild(a);
          this._layers.push(a);
          return a;
        };
        Object.defineProperty(f.prototype, "_backgroundVideoLayer", {get:function() {
          return this._layers[0];
        }, enumerable:!0, configurable:!0});
        f.prototype._createTarget = function(a) {
          return new e.Canvas2DSurfaceRegion(new e.Canvas2DSurface(a), new m.RegionAllocator.Region(0, 0, a.width, a.height), a.width, a.height);
        };
        f.prototype._onStageBoundsChanged = function(a) {
          var b = this._stage.getBounds(!0);
          b.snap();
          for (var c = this._devicePixelRatio = window.devicePixelRatio || 1, e = b.w / c + "px", c = b.h / c + "px", f = 0;f < this._layers.length;f++) {
            var g = this._layers[f];
            g.style.width = e;
            g.style.height = c;
          }
          a.width = b.w;
          a.height = b.h;
          a.style.position = "absolute";
          a.style.width = e;
          a.style.height = c;
          this._target = this._createTarget(a);
          this._fontSize = 10 * this._devicePixelRatio;
        };
        f._prepareSurfaceAllocators = function() {
          f._initializedCaches || (f._surfaceCache = new m.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            var f = Math.max(1024, a), g = Math.max(1024, b);
            c.width = f;
            c.height = g;
            var h = null, h = 512 <= a || 512 <= b ? new m.RegionAllocator.GridAllocator(f, g, f, g) : new m.RegionAllocator.BucketAllocator(f, g);
            return new e.Canvas2DSurface(c, h);
          }), f._shapeCache = new m.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            c.width = 1024;
            c.height = 1024;
            var f = f = new m.RegionAllocator.CompactAllocator(1024, 1024);
            return new e.Canvas2DSurface(c, f);
          }), f._initializedCaches = !0);
        };
        f.prototype.render = function() {
          var a = this._stage, b = this._target, c = this._options, e = this._viewport;
          b.reset();
          b.context.save();
          b.context.beginPath();
          b.context.rect(e.x, e.y, e.w, e.h);
          b.context.clip();
          this._renderStageToTarget(b, a, e);
          b.reset();
          c.paintViewport && (b.context.beginPath(), b.context.rect(e.x, e.y, e.w, e.h), b.context.strokeStyle = "#FF4981", b.context.lineWidth = 2, b.context.stroke());
          b.context.restore();
        };
        f.prototype.renderNode = function(a, b, c) {
          var e = new t(this._target);
          e.clip.set(b);
          e.flags = 256;
          e.matrix.set(c);
          a.visit(this, e);
          e.free();
        };
        f.prototype.renderNodeWithState = function(a, b) {
          a.visit(this, b);
        };
        f.prototype._renderWithCache = function(a, b) {
          var c = b.matrix, e = a.getBounds();
          if (e.isEmpty()) {
            return!1;
          }
          var g = this._options.cacheShapesMaxSize, h = Math.max(c.getAbsoluteScaleX(), c.getAbsoluteScaleY()), k = !!(b.flags & 16), l = !!(b.flags & 8);
          if (b.hasFlags(256)) {
            if (l || k || !b.colorMatrix.isIdentity() || a.hasFlags(1048576) || 100 < this._options.cacheShapesThreshold || e.w * h > g || e.h * h > g) {
              return!1;
            }
            (h = a.properties.mipMap) || (h = a.properties.mipMap = new v(this, a, f._shapeCache, g));
            k = h.getLevel(c);
            g = k.surfaceRegion;
            h = g.region;
            return k ? (k = b.target.context, k.imageSmoothingEnabled = k.mozImageSmoothingEnabled = !0, k.setTransform(c.a, c.b, c.c, c.d, c.tx, c.ty), k.drawImage(g.surface.canvas, h.x, h.y, h.w, h.h, e.x, e.y, e.w, e.h), !0) : !1;
          }
        };
        f.prototype._intersectsClipList = function(a, b) {
          var c = a.getBounds(!0), e = !1;
          b.matrix.transformRectangleAABB(c);
          b.clip.intersects(c) && (e = !0);
          var f = b.clipList;
          if (e && f.length) {
            for (var e = !1, g = 0;g < f.length;g++) {
              if (c.intersects(f[g])) {
                e = !0;
                break;
              }
            }
          }
          c.free();
          return e;
        };
        f.prototype.visitGroup = function(a, b) {
          this._frameInfo.groups++;
          a.getBounds();
          if ((!a.hasFlags(4) || b.flags & 4) && a.hasFlags(65536)) {
            if (b.flags & 1 || 1 === a.getLayer().blendMode && !a.getLayer().mask || !this._options.blending) {
              if (this._intersectsClipList(a, b)) {
                for (var c = null, e = a.getChildren(), f = 0;f < e.length;f++) {
                  var g = e[f], h = b.transform(g.getTransform());
                  h.toggleFlags(4096, g.hasFlags(524288));
                  if (0 <= g.clip) {
                    c = c || new Uint8Array(e.length);
                    c[g.clip + f]++;
                    var k = h.clone();
                    b.target.context.save();
                    k.flags |= 16;
                    g.visit(this, k);
                    k.free();
                  } else {
                    g.visit(this, h);
                  }
                  if (c && 0 < c[f]) {
                    for (;c[f]--;) {
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
        f.prototype._renderDebugInfo = function(a, b) {
          if (b.flags & 1024) {
            var c = b.target.context, e = a.getBounds(!0), h = a.properties.style;
            h || (h = a.properties.style = g.Color.randomColor().toCSSStyle());
            c.strokeStyle = h;
            b.matrix.transformRectangleAABB(e);
            c.setTransform(1, 0, 0, 1, 0, 0);
            e.free();
            e = a.getBounds();
            h = f._debugPoints;
            b.matrix.transformRectangle(e, h);
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
        f.prototype.visitStage = function(a, b) {
          var c = b.target.context, e = a.getBounds(!0);
          b.matrix.transformRectangleAABB(e);
          e.intersect(b.clip);
          b.target.reset();
          b = b.clone();
          this._options.clear && b.target.clear(b.clip);
          a.hasFlags(32768) || !a.color || b.flags & 32 || (this._container.style.backgroundColor = a.color.toCSSStyle());
          this.visitGroup(a, b);
          a.dirtyRegion && (c.restore(), b.target.reset(), c.globalAlpha = .4, b.hasFlags(2048) && a.dirtyRegion.render(b.target.context), a.dirtyRegion.clear());
          b.free();
        };
        f.prototype.visitShape = function(a, b) {
          if (this._intersectsClipList(a, b)) {
            var c = b.matrix;
            b.flags & 8192 && (c = c.clone(), c.snap());
            var f = b.target.context;
            e.Filters._applyColorMatrix(f, b.colorMatrix);
            a.source instanceof m.RenderableVideo ? this.visitRenderableVideo(a.source, b) : 0 < f.globalAlpha && this.visitRenderable(a.source, b, a.ratio);
            b.flags & 8192 && c.free();
          }
        };
        f.prototype.visitRenderableVideo = function(a, b) {
          if (a.video && a.video.videoWidth) {
            var c = this._devicePixelRatio, e = b.matrix.clone();
            e.scale(1 / c, 1 / c);
            var c = a.getBounds(), f = g.GFX.Geometry.Matrix.createIdentity();
            f.scale(c.w / a.video.videoWidth, c.h / a.video.videoHeight);
            e.preMultiply(f);
            f.free();
            c = e.toCSSTransform();
            a.video.style.transformOrigin = "0 0";
            a.video.style.transform = c;
            this._backgroundVideoLayer !== a.video.parentElement && (this._backgroundVideoLayer.appendChild(a.video), 1 === a.state && a.play());
            e.free();
          }
        };
        f.prototype.visitRenderable = function(a, b, c) {
          var e = a.getBounds(), f = b.matrix, h = b.target.context, k = !!(b.flags & 16), l = !!(b.flags & 8), p = !!(b.flags & 512);
          if (!(e.isEmpty() || b.flags & 32)) {
            if (b.hasFlags(64)) {
              b.removeFlags(64);
            } else {
              if (this._renderWithCache(a, b)) {
                return;
              }
            }
            h.setTransform(f.a, f.b, f.c, f.d, f.tx, f.ty);
            var n = 0;
            p && (n = performance.now());
            this._frameInfo.shapes++;
            h.imageSmoothingEnabled = h.mozImageSmoothingEnabled = b.hasFlags(4096);
            b = a.properties.renderCount || 0;
            Math.max(f.getAbsoluteScaleX(), f.getAbsoluteScaleY());
            a.properties.renderCount = ++b;
            a.render(h, c, null, k, l);
            p && (a = performance.now() - n, h.fillStyle = g.ColorStyle.gradientColor(.1 / a), h.globalAlpha = .3 + .1 * Math.random(), h.fillRect(e.x, e.y, e.w, e.h));
          }
        };
        f.prototype._renderLayer = function(a, b) {
          var c = a.getLayer(), e = c.mask;
          if (e) {
            this._renderWithMask(a, e, c.blendMode, !a.hasFlags(131072) || !e.hasFlags(131072), b);
          } else {
            var e = w.allocate(), f = this._renderToTemporarySurface(a, b, e);
            f && (b.target.draw(f, e.x, e.y, e.w, e.h, c.blendMode), f.free());
            e.free();
          }
        };
        f.prototype._renderWithMask = function(a, b, c, e, f) {
          var g = b.getTransform().getConcatenatedMatrix(!0);
          b.parent || (g = g.scale(this._devicePixelRatio, this._devicePixelRatio));
          var h = a.getBounds().clone();
          f.matrix.transformRectangleAABB(h);
          h.snap();
          if (!h.isEmpty()) {
            var k = b.getBounds().clone();
            g.transformRectangleAABB(k);
            k.snap();
            if (!k.isEmpty()) {
              var l = f.clip.clone();
              l.intersect(h);
              l.intersect(k);
              l.snap();
              l.isEmpty() || (h = f.clone(), h.clip.set(l), a = this._renderToTemporarySurface(a, h, w.createEmpty()), h.free(), h = f.clone(), h.clip.set(l), h.matrix = g, h.flags |= 4, e && (h.flags |= 8), b = this._renderToTemporarySurface(b, h, w.createEmpty()), h.free(), a.draw(b, 0, 0, l.w, l.h, 11), f.target.draw(a, l.x, l.y, l.w, l.h, c), b.free(), a.free());
            }
          }
        };
        f.prototype._renderStageToTarget = function(a, c, e) {
          w.allocationCount = b.allocationCount = t.allocationCount = 0;
          a = new t(a);
          a.clip.set(e);
          this._options.paintRenderable || (a.flags |= 32);
          this._options.paintBounds && (a.flags |= 1024);
          this._options.paintDirtyRegion && (a.flags |= 2048);
          this._options.paintFlashing && (a.flags |= 512);
          this._options.cacheShapes && (a.flags |= 256);
          this._options.imageSmoothing && (a.flags |= 4096);
          this._options.snapToDevicePixels && (a.flags |= 8192);
          this._frameInfo.enter(a);
          c.visit(this, a);
          this._frameInfo.leave();
        };
        f.prototype._renderToTemporarySurface = function(a, b, c) {
          var e = b.matrix, f = a.getBounds().clone();
          e.transformRectangleAABB(f);
          f.snap();
          c.set(f);
          c.intersect(b.clip);
          c.snap();
          if (c.isEmpty()) {
            return null;
          }
          var f = this._allocateSurface(c.w, c.h), h = f.region, h = new w(h.x, h.y, c.w, c.h);
          f.context.setTransform(1, 0, 0, 1, 0, 0);
          f.clear();
          e = e.clone();
          e.translate(h.x - c.x, h.y - c.y);
          f.context.save();
          b = b.clone();
          b.target = f;
          b.matrix = e;
          b.clip.set(h);
          a.visit(this, b);
          b.free();
          f.context.restore();
          return f;
        };
        f.prototype._allocateSurface = function(a, b) {
          var c = f._surfaceCache.allocate(a, b);
          c.fill("#FF4981");
          return c;
        };
        f.prototype.screenShot = function(a, b) {
          if (b) {
            var e = this._stage.content.groupChild.child;
            c(e instanceof m.Stage);
            a = e.content.getBounds(!0);
            e.content.getTransform().getConcatenatedMatrix().transformRectangleAABB(a);
            a.intersect(this._viewport);
          }
          a || (a = new w(0, 0, this._target.w, this._target.h));
          e = document.createElement("canvas");
          e.width = a.w;
          e.height = a.h;
          var f = e.getContext("2d");
          f.fillStyle = this._container.style.backgroundColor;
          f.fillRect(0, 0, a.w, a.h);
          f.drawImage(this._target.context.canvas, a.x, a.y, a.w, a.h, 0, 0, a.w, a.h);
          return new m.ScreenShot(e.toDataURL("image/png"), a.w, a.h);
        };
        f._initializedCaches = !1;
        f._debugPoints = k.createEmptyPoints(4);
        return f;
      }(m.Renderer);
      e.Canvas2DRenderer = u;
    })(m.Canvas2D || (m.Canvas2D = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    var e = g.Debug.assert, c = m.Geometry.Point, w = m.Geometry.Matrix, k = m.Geometry.Rectangle, b = g.Tools.Mini.FPS, a = function() {
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
    m.UIState = a;
    var n = function(a) {
      function b() {
        a.apply(this, arguments);
        this._keyCodes = [];
      }
      __extends(b, a);
      b.prototype.onMouseDown = function(a, b) {
        b.altKey && (a.state = new y(a.worldView, a.getMousePosition(b, null), a.worldView.getTransform().getMatrix(!0)));
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
        this._mousePosition = new c(0, 0);
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
        var c = "DOMMouseScroll" === b.type ? -b.detail : b.wheelDelta / 40;
        if (b.altKey) {
          b.preventDefault();
          var e = a.getMousePosition(b, null), f = a.worldView.getTransform().getMatrix(!0), c = 1 + c / 1E3;
          f.translate(-e.x, -e.y);
          f.scale(c, c);
          f.translate(e.x, e.y);
          a.worldView.getTransform().setMatrix(f);
        }
      };
      b.prototype.onKeyPress = function(a, b) {
        if (b.altKey) {
          var c = b.keyCode || b.which;
          console.info("onKeyPress Code: " + c);
          switch(c) {
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
          var b = m.viewportLoupeDiameter.value, c = m.viewportLoupeDiameter.value;
          a.viewport = new k(this._mousePosition.x - b / 2, this._mousePosition.y - c / 2, b, c);
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
          var c = a._world;
          c && (a.state = new y(c, a.getMousePosition(b, null), c.getTransform().getMatrix(!0)));
        }
      };
      b.prototype.onMouseUp = function(a, b) {
        a.state = new n;
        a.selectNodeUnderMouse(b);
      };
      return b;
    })(a);
    var y = function(a) {
      function b(c, e, g) {
        a.call(this);
        this._target = c;
        this._startPosition = e;
        this._startMatrix = g;
      }
      __extends(b, a);
      b.prototype.onMouseMove = function(a, b) {
        b.preventDefault();
        var c = a.getMousePosition(b, null);
        c.sub(this._startPosition);
        this._target.getTransform().setMatrix(this._startMatrix.clone().translate(c.x, c.y));
        a.state = this;
      };
      b.prototype.onMouseUp = function(a, b) {
        a.state = new n;
      };
      return b;
    }(a), a = function() {
      function a(c, k, r) {
        function v(a) {
          d._state.onMouseWheel(d, a);
          d._persistentState.onMouseWheel(d, a);
        }
        void 0 === k && (k = !1);
        void 0 === r && (r = void 0);
        this._state = new n;
        this._persistentState = new p;
        this.paused = !1;
        this.viewport = null;
        this._selectedNodes = [];
        this._eventListeners = Object.create(null);
        this._fullScreen = !1;
        e(c && 0 === c.children.length, "Easel container must be empty.");
        this._container = c;
        this._stage = new m.Stage(512, 512, !0);
        this._worldView = this._stage.content;
        this._world = new m.Group;
        this._worldView.addChild(this._world);
        this._disableHiDPI = k;
        k = document.createElement("div");
        k.style.position = "absolute";
        k.style.width = "100%";
        k.style.height = "100%";
        c.appendChild(k);
        if (m.hud.value) {
          var h = document.createElement("div");
          h.style.position = "absolute";
          h.style.width = "100%";
          h.style.height = "100%";
          h.style.pointerEvents = "none";
          var f = document.createElement("div");
          f.style.position = "absolute";
          f.style.width = "100%";
          f.style.height = "20px";
          f.style.pointerEvents = "none";
          h.appendChild(f);
          c.appendChild(h);
          this._fps = new b(f);
        } else {
          this._fps = null;
        }
        this.transparent = h = 0 === r;
        void 0 === r || 0 === r || g.ColorUtilities.rgbaToCSSStyle(r);
        this._options = new m.Canvas2D.Canvas2DRendererOptions;
        this._options.alpha = h;
        this._renderer = new m.Canvas2D.Canvas2DRenderer(k, this._stage, this._options);
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
        window.addEventListener("DOMMouseScroll", v);
        window.addEventListener("mousewheel", v);
        c.addEventListener("mousedown", function(a) {
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
        this._enterRenderLoop();
      }
      a.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var c = this;
        setInterval(function() {
          if (a !== c._containerWidth || b !== c._containerHeight) {
            c._onContainerSizeChanged(), a = c._containerWidth, b = c._containerHeight;
          }
        }, 10);
      };
      a.prototype._onContainerSizeChanged = function() {
        var a = this.getRatio(), b = Math.ceil(this._containerWidth * a), c = Math.ceil(this._containerHeight * a);
        this._stage.setBounds(new k(0, 0, b, c));
        this._stage.content.setBounds(new k(0, 0, b, c));
        this._worldView.getTransform().setMatrix(new w(a, 0, 0, a, 0, 0));
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
      a.prototype._enterRenderLoop = function() {
        var a = this;
        requestAnimationFrame(function r() {
          a.render();
          requestAnimationFrame(r);
        });
      };
      Object.defineProperty(a.prototype, "state", {set:function(a) {
        this._state = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "cursor", {set:function(a) {
        this._container.style.cursor = a;
      }, enumerable:!0, configurable:!0});
      a.prototype._render = function() {
        m.RenderableVideo.checkForVideoUpdates();
        var a = (this._stage.readyToRender() || m.forcePaint.value) && !this.paused, b = 0;
        if (a) {
          var c = this._renderer;
          c.viewport = this.viewport ? this.viewport : this._stage.getBounds();
          this._dispatchEvent("render");
          b = performance.now();
          c.render();
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
        return{stageWidth:this._containerWidth, stageHeight:this._containerHeight, pixelRatio:this.getRatio(), screenWidth:window.screen ? window.screen.width : 640, screenHeight:window.screen ? window.screen.height : 480};
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
      a.prototype.getMousePosition = function(a, b) {
        var e = this._container, g = e.getBoundingClientRect(), h = this.getRatio(), e = new c(e.scrollWidth / g.width * (a.clientX - g.left) * h, e.scrollHeight / g.height * (a.clientY - g.top) * h);
        if (!b) {
          return e;
        }
        g = w.createIdentity();
        b.getTransform().getConcatenatedMatrix().inverse(g);
        g.transformPoint(e);
        return e;
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
    m.Easel = a;
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    var e = g.GFX.Geometry.Matrix;
    (function(c) {
      c[c.Simple = 0] = "Simple";
    })(m.Layout || (m.Layout = {}));
    var c = function(c) {
      function b() {
        c.apply(this, arguments);
        this.layout = 0;
      }
      __extends(b, c);
      return b;
    }(m.RendererOptions);
    m.TreeRendererOptions = c;
    var w = function(g) {
      function b(a, b, e) {
        void 0 === e && (e = new c);
        g.call(this, a, b, e);
        this._canvas = document.createElement("canvas");
        this._container.appendChild(this._canvas);
        this._context = this._canvas.getContext("2d");
        this._listenForContainerSizeChanges();
      }
      __extends(b, g);
      b.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var c = this;
        setInterval(function() {
          if (a !== c._containerWidth || b !== c._containerHeight) {
            c._onContainerSizeChanged(), a = c._containerWidth, b = c._containerHeight;
          }
        }, 10);
      };
      b.prototype._getRatio = function() {
        var a = window.devicePixelRatio || 1, b = 1;
        1 !== a && (b = a / 1);
        return b;
      };
      b.prototype._onContainerSizeChanged = function() {
        var a = this._getRatio(), b = Math.ceil(this._containerWidth * a), c = Math.ceil(this._containerHeight * a), e = this._canvas;
        0 < a ? (e.width = b * a, e.height = c * a, e.style.width = b + "px", e.style.height = c + "px") : (e.width = b, e.height = c);
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
        0 === this._options.layout && this._renderNodeSimple(this._context, this._stage, e.createIdentity());
        a.restore();
      };
      b.prototype._renderNodeSimple = function(a, b, c) {
        function e(b) {
          var c = b.getChildren();
          a.fillStyle = b.hasFlags(16) ? "red" : "white";
          var d = String(b.id);
          b instanceof m.RenderableText ? d = "T" + d : b instanceof m.RenderableShape ? d = "S" + d : b instanceof m.RenderableBitmap ? d = "B" + d : b instanceof m.RenderableVideo && (d = "V" + d);
          b instanceof m.Renderable && (d = d + " [" + b._parents.length + "]");
          b = a.measureText(d).width;
          a.fillText(d, k, t);
          if (c) {
            k += b + 4;
            u = Math.max(u, k + 20);
            for (d = 0;d < c.length;d++) {
              e(c[d]), d < c.length - 1 && (t += 18, t > g._canvas.height && (a.fillStyle = "gray", k = k - r + u + 8, r = u + 8, t = 0, a.fillStyle = "white"));
            }
            k -= b + 4;
          }
        }
        var g = this;
        a.save();
        a.font = "16px Arial";
        a.fillStyle = "white";
        var k = 0, t = 0, r = 0, u = 0;
        e(b);
        a.restore();
      };
      return b;
    }(m.Renderer);
    m.TreeRenderer = w;
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.GFX.BlurFilter, w = g.GFX.DropshadowFilter, k = g.GFX.Shape, b = g.GFX.Group, a = g.GFX.RenderableShape, n = g.GFX.RenderableMorphShape, p = g.GFX.RenderableBitmap, y = g.GFX.RenderableVideo, v = g.GFX.RenderableText, l = g.GFX.ColorMatrix, t = g.ShapeData, r = g.ArrayUtilities.DataBuffer, u = g.GFX.Stage, h = g.GFX.Geometry.Matrix, f = g.GFX.Geometry.Rectangle, d = g.Debug.assert, q = function() {
        function a() {
        }
        a.prototype.writeMouseEvent = function(a, b) {
          var c = this.output;
          c.writeInt(300);
          var d = g.Remoting.MouseEventNames.indexOf(a.type);
          c.writeInt(d);
          c.writeFloat(b.x);
          c.writeFloat(b.y);
          c.writeInt(a.buttons);
          c.writeInt((a.ctrlKey ? 1 : 0) | (a.altKey ? 2 : 0) | (a.shiftKey ? 4 : 0));
        };
        a.prototype.writeKeyboardEvent = function(a) {
          var b = this.output;
          b.writeInt(301);
          var c = g.Remoting.KeyboardEventNames.indexOf(a.type);
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
      e.GFXChannelSerializer = q;
      q = function() {
        function a(b, c, d) {
          function e(a) {
            a = a.getBounds(!0);
            var c = b.easel.getRatio();
            a.scale(1 / c, 1 / c);
            a.snap();
            f.setBounds(a);
          }
          var f = this.stage = new u(128, 512);
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
          this._assets[a] && console.warn("Asset already exists: " + a + ". old:", this._assets[a], "new: " + c);
          this._assets[a] = c;
        };
        a.prototype._makeNode = function(a) {
          if (-1 === a) {
            return null;
          }
          var b = null;
          a & 134217728 ? (a &= -134217729, b = this._assets[a].wrap()) : b = this._nodes[a];
          d(b, "Node " + b + " of " + a + " has not been sent yet.");
          return b;
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
          g.registerCSSFont(a, b.data, !inFirefox);
          inFirefox ? c(null) : window.setTimeout(c, 400);
        };
        a.prototype.registerImage = function(a, b, c, d) {
          this._registerAsset(a, b, this._decodeImage(c.dataType, c.data, d));
        };
        a.prototype.registerVideo = function(a) {
          this._registerAsset(a, 0, new y(a, this));
        };
        a.prototype._decodeImage = function(a, b, c) {
          var e = new Image, h = p.FromImage(e);
          e.src = URL.createObjectURL(new Blob([b], {type:g.getMIMETypeForImageType(a)}));
          e.onload = function() {
            d(!h.parent);
            h.setBounds(new f(0, 0, e.width, e.height));
            h.invalidate();
            c({width:e.width, height:e.height});
          };
          e.onerror = function() {
            c(null);
          };
          return h;
        };
        a.prototype.sendVideoPlaybackEvent = function(a, b, c) {
          this._easelHost.sendVideoPlaybackEvent(a, b, c);
        };
        return a;
      }();
      e.GFXChannelDeserializerContext = q;
      q = function() {
        function e() {
        }
        e.prototype.read = function() {
          for (var a = 0, b = this.input, c = 0, e = 0, f = 0, g = 0, h = 0, k = 0, l = 0, p = 0;0 < b.bytesAvailable;) {
            switch(a = b.readInt(), a) {
              case 0:
                return;
              case 101:
                c++;
                this._readUpdateGraphics();
                break;
              case 102:
                e++;
                this._readUpdateBitmapData();
                break;
              case 103:
                f++;
                this._readUpdateTextContent();
                break;
              case 100:
                g++;
                this._readUpdateFrame();
                break;
              case 104:
                h++;
                this._readUpdateStage();
                break;
              case 105:
                k++;
                this._readUpdateNetStream();
                break;
              case 200:
                l++;
                this._readDrawToBitmap();
                break;
              case 106:
                p++;
                this._readRequestBitmapData();
                break;
              default:
                d(!1, "Unknown MessageReader tag: " + a);
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
          var a = this.input, b = e._temporaryReadColorMatrix, c = 1, d = 1, f = 1, g = 1, h = 0, k = 0, l = 0, p = 0;
          switch(a.readInt()) {
            case 0:
              return e._temporaryReadColorMatrixIdentity;
            case 1:
              g = a.readFloat();
              break;
            case 2:
              c = a.readFloat(), d = a.readFloat(), f = a.readFloat(), g = a.readFloat(), h = a.readInt(), k = a.readInt(), l = a.readInt(), p = a.readInt();
          }
          b.setMultipliersAndOffsets(c, d, f, g, h, k, l, p);
          return b;
        };
        e.prototype._readAsset = function() {
          var a = this.input.readInt(), b = this.inputAssets[a];
          this.inputAssets[a] = null;
          return b;
        };
        e.prototype._readUpdateGraphics = function() {
          for (var b = this.input, c = this.context, d = b.readInt(), e = b.readInt(), f = c._getAsset(d), g = this._readRectangle(), h = t.FromPlainObject(this._readAsset()), k = b.readInt(), l = [], p = 0;p < k;p++) {
            var m = b.readInt();
            l.push(c._getBitmapAsset(m));
          }
          if (f) {
            f.update(h, l, g);
          } else {
            b = h.morphCoordinates ? new n(d, h, l, g) : new a(d, h, l, g);
            for (p = 0;p < l.length;p++) {
              l[p] && l[p].addRenderableParent(b);
            }
            c._registerAsset(d, e, b);
          }
        };
        e.prototype._readUpdateBitmapData = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getBitmapAsset(c), f = this._readRectangle(), a = a.readInt(), g = r.FromPlainObject(this._readAsset());
          e ? e.updateFromDataBuffer(a, g) : (e = p.FromDataBuffer(a, g, f), b._registerAsset(c, d, e));
        };
        e.prototype._readUpdateTextContent = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getTextAsset(c), f = this._readRectangle(), g = this._readMatrix(), h = a.readInt(), k = a.readInt(), l = a.readInt(), p = a.readBoolean(), n = a.readInt(), m = a.readInt(), q = this._readAsset(), t = r.FromPlainObject(this._readAsset()), u = null, w = a.readInt();
          w && (u = new r(4 * w), a.readBytes(u, 4 * w));
          e ? (e.setBounds(f), e.setContent(q, t, g, u), e.setStyle(h, k, n, m), e.reflow(l, p)) : (e = new v(f), e.setContent(q, t, g, u), e.setStyle(h, k, n, m), e.reflow(l, p), b._registerAsset(c, d, e));
          if (this.output) {
            for (a = e.textRect, this.output.writeInt(20 * a.w), this.output.writeInt(20 * a.h), this.output.writeInt(20 * a.x), e = e.lines, a = e.length, this.output.writeInt(a), b = 0;b < a;b++) {
              this._writeLineMetrics(e[b]);
            }
          }
        };
        e.prototype._writeLineMetrics = function(a) {
          d(this.output);
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
          a.stage.color = g.Color.FromARGB(b);
          a.stage.align = this.input.readInt();
          a.stage.scaleMode = this.input.readInt();
          b = this.input.readInt();
          this.input.readInt();
          c = this.input.readInt();
          a._easelHost.cursor = g.UI.toCSSCursor(c);
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
              var h = b.readInt();
              switch(h) {
                case 0:
                  e.push(new c(b.readFloat(), b.readFloat(), b.readInt()));
                  break;
                case 1:
                  e.push(new w(b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat(), b.readInt(), b.readFloat(), b.readBoolean(), b.readBoolean(), b.readBoolean(), b.readInt(), b.readFloat()));
                  break;
                default:
                  g.Debug.somewhatImplemented(m.FilterType[h]);
              }
            }
            a.getLayer().filters = e;
          }
        };
        e.prototype._readUpdateFrame = function() {
          var a = this.input, c = this.context, e = a.readInt(), f = 0, g = c._nodes[e];
          g || (g = c._nodes[e] = new b);
          var h = a.readInt();
          h & 1 && g.getTransform().setMatrix(this._readMatrix());
          h & 8 && g.getTransform().setColorMatrix(this._readColorMatrix());
          if (h & 64) {
            var l = a.readInt();
            0 <= l && (g.getLayer().mask = c._makeNode(l));
          }
          h & 128 && (g.clip = a.readInt());
          h & 32 && (f = a.readInt() / 65535, d(0 <= f && 1 >= f), l = a.readInt(), 1 !== l && (g.getLayer().blendMode = l), this._readFilters(g), g.toggleFlags(65536, a.readBoolean()), g.toggleFlags(131072, a.readBoolean()), g.toggleFlags(262144, !!a.readInt()), g.toggleFlags(524288, !!a.readInt()));
          if (h & 4) {
            h = a.readInt();
            l = g;
            l.clearChildren();
            for (var p = 0;p < h;p++) {
              var n = a.readInt(), m = c._makeNode(n);
              d(m, "Child " + n + " of " + e + " has not been sent yet.");
              l.addChild(m);
            }
          }
          f && (m = g.getChildren()[0], m instanceof k && (m.ratio = f));
        };
        e.prototype._readDrawToBitmap = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = a.readInt(), f, g, k;
          f = e & 1 ? this._readMatrix().clone() : h.createIdentity();
          e & 8 && (g = this._readColorMatrix());
          e & 16 && (k = this._readRectangle());
          e = a.readInt();
          a.readBoolean();
          a = b._getBitmapAsset(c);
          d = b._makeNode(d);
          a ? a.drawNode(d, f, g, e, k) : b._registerAsset(c, -1, p.FromNode(d, f, g, e, k));
        };
        e.prototype._readRequestBitmapData = function() {
          var a = this.output, b = this.context, c = this.input.readInt();
          b._getBitmapAsset(c).readImageData(a);
        };
        e._temporaryReadMatrix = h.createIdentity();
        e._temporaryReadRectangle = f.createEmpty();
        e._temporaryReadColorMatrix = l.createIdentity();
        e._temporaryReadColorMatrixIdentity = l.createIdentity();
        return e;
      }();
      e.GFXChannelDeserializer = q;
    })(m.GFX || (m.GFX = {}));
  })(g.Remoting || (g.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    var e = g.GFX.Geometry.Point, c = g.ArrayUtilities.DataBuffer, w = function() {
      function k(b) {
        this._easel = b;
        var a = b.transparent;
        this._group = b.world;
        this._content = null;
        this._fullscreen = !1;
        this._context = new g.Remoting.GFX.GFXChannelDeserializerContext(this, this._group, a);
        this._addEventListeners();
      }
      k.prototype.onSendUpdates = function(b, a) {
        throw Error("This method is abstract");
      };
      Object.defineProperty(k.prototype, "easel", {get:function() {
        return this._easel;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(k.prototype, "stage", {get:function() {
        return this._easel.stage;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(k.prototype, "content", {set:function(b) {
        this._content = b;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(k.prototype, "cursor", {set:function(b) {
        this._easel.cursor = b;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(k.prototype, "fullscreen", {set:function(b) {
        if (this._fullscreen !== b) {
          this._fullscreen = b;
          var a = window.FirefoxCom;
          a && a.request("setFullscreen", b, null);
        }
      }, enumerable:!0, configurable:!0});
      k.prototype._mouseEventListener = function(b) {
        var a = this._easel.getMousePosition(b, this._content), a = new e(a.x, a.y), k = new c, p = new g.Remoting.GFX.GFXChannelSerializer;
        p.output = k;
        p.writeMouseEvent(b, a);
        this.onSendUpdates(k, []);
      };
      k.prototype._keyboardEventListener = function(b) {
        var a = new c, e = new g.Remoting.GFX.GFXChannelSerializer;
        e.output = a;
        e.writeKeyboardEvent(b);
        this.onSendUpdates(a, []);
      };
      k.prototype._addEventListeners = function() {
        for (var b = this._mouseEventListener.bind(this), a = this._keyboardEventListener.bind(this), c = k._mouseEvents, e = 0;e < c.length;e++) {
          window.addEventListener(c[e], b);
        }
        b = k._keyboardEvents;
        for (e = 0;e < b.length;e++) {
          window.addEventListener(b[e], a);
        }
        this._addFocusEventListeners();
        this._easel.addEventListener("resize", this._resizeEventListener.bind(this));
      };
      k.prototype._sendFocusEvent = function(b) {
        var a = new c, e = new g.Remoting.GFX.GFXChannelSerializer;
        e.output = a;
        e.writeFocusEvent(b);
        this.onSendUpdates(a, []);
      };
      k.prototype._addFocusEventListeners = function() {
        var b = this;
        document.addEventListener("visibilitychange", function(a) {
          b._sendFocusEvent(document.hidden ? 0 : 1);
        });
        window.addEventListener("focus", function(a) {
          b._sendFocusEvent(3);
        });
        window.addEventListener("blur", function(a) {
          b._sendFocusEvent(2);
        });
      };
      k.prototype._resizeEventListener = function() {
        this.onDisplayParameters(this._easel.getDisplayParameters());
      };
      k.prototype.onDisplayParameters = function(b) {
        throw Error("This method is abstract");
      };
      k.prototype.processUpdates = function(b, a, c) {
        void 0 === c && (c = null);
        var e = new g.Remoting.GFX.GFXChannelDeserializer;
        e.input = b;
        e.inputAssets = a;
        e.output = c;
        e.context = this._context;
        e.read();
      };
      k.prototype.processExternalCommand = function(b) {
        if ("isEnabled" === b.action) {
          b.result = !1;
        } else {
          throw Error("This command is not supported");
        }
      };
      k.prototype.processVideoControl = function(b, a, c) {
        var e = this._context, g = e._getVideoAsset(b);
        if (!g) {
          if (1 != a) {
            return;
          }
          e.registerVideo(b);
          g = e._getVideoAsset(b);
        }
        return g.processControlRequest(a, c);
      };
      k.prototype.processRegisterFontOrImage = function(b, a, c, e, k) {
        "font" === c ? this._context.registerFont(b, e, k) : (g.Debug.assert("image" === c), this._context.registerImage(b, a, e, k));
      };
      k.prototype.processFSCommand = function(b, a) {
      };
      k.prototype.processFrame = function() {
      };
      k.prototype.onExernalCallback = function(b) {
        throw Error("This method is abstract");
      };
      k.prototype.sendExernalCallback = function(b, a) {
        var c = {functionName:b, args:a};
        this.onExernalCallback(c);
        if (c.error) {
          throw Error(c.error);
        }
        return c.result;
      };
      k.prototype.onVideoPlaybackEvent = function(b, a, c) {
        throw Error("This method is abstract");
      };
      k.prototype.sendVideoPlaybackEvent = function(b, a, c) {
        this.onVideoPlaybackEvent(b, a, c);
      };
      k._mouseEvents = g.Remoting.MouseEventNames;
      k._keyboardEvents = g.Remoting.KeyboardEventNames;
      return k;
    }();
    m.EaselHost = w;
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.ArrayUtilities.DataBuffer, w = g.CircularBuffer, k = g.Tools.Profiler.TimelineBuffer, b = function(a) {
        function b(c, e, g) {
          a.call(this, c);
          this._timelineRequests = Object.create(null);
          this._playerWindow = e;
          this._window = g;
          this._window.addEventListener("message", function(a) {
            this.onWindowMessage(a.data);
          }.bind(this));
          this._window.addEventListener("syncmessage", function(a) {
            this.onWindowMessage(a.detail, !1);
          }.bind(this));
        }
        __extends(b, a);
        b.prototype.onSendUpdates = function(a, b) {
          var c = a.getBytes();
          this._playerWindow.postMessage({type:"gfx", updates:c, assets:b}, "*", [c.buffer]);
        };
        b.prototype.onExernalCallback = function(a) {
          var b = this._playerWindow.document.createEvent("CustomEvent");
          b.initCustomEvent("syncmessage", !1, !1, {type:"externalCallback", request:a});
          this._playerWindow.dispatchEvent(b);
        };
        b.prototype.onDisplayParameters = function(a) {
          this._playerWindow.postMessage({type:"displayParameters", params:a}, "*");
        };
        b.prototype.onVideoPlaybackEvent = function(a, b, c) {
          var e = this._playerWindow.document.createEvent("CustomEvent");
          e.initCustomEvent("syncmessage", !1, !1, {type:"videoPlayback", id:a, eventType:b, data:c});
          this._playerWindow.dispatchEvent(e);
        };
        b.prototype.requestTimeline = function(a, b) {
          return new Promise(function(c) {
            this._timelineRequests[a] = c;
            this._playerWindow.postMessage({type:"timeline", cmd:b, request:a}, "*");
          }.bind(this));
        };
        b.prototype.onWindowMessage = function(a, b) {
          void 0 === b && (b = !0);
          if ("object" === typeof a && null !== a) {
            if ("player" === a.type) {
              var e = c.FromArrayBuffer(a.updates.buffer);
              if (b) {
                this.processUpdates(e, a.assets);
              } else {
                var g = new c;
                this.processUpdates(e, a.assets, g);
                a.result = g.toPlainObject();
              }
            } else {
              "frame" !== a.type && ("external" === a.type ? this.processExternalCommand(a.request) : "videoControl" === a.type ? a.result = this.processVideoControl(a.id, a.eventType, a.data) : "registerFontOrImage" === a.type ? this.processRegisterFontOrImage(a.syncId, a.symbolId, a.assetType, a.data, a.resolve) : "fscommand" !== a.type && "timelineResponse" === a.type && a.timeline && (a.timeline.__proto__ = k.prototype, a.timeline._marks.__proto__ = w.prototype, a.timeline._times.__proto__ = 
              w.prototype, this._timelineRequests[a.request](a.timeline)));
            }
          }
        };
        return b;
      }(m.EaselHost);
      e.WindowEaselHost = b;
    })(m.Window || (m.Window = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
(function(g) {
  (function(m) {
    (function(e) {
      var c = g.ArrayUtilities.DataBuffer, w = function(e) {
        function b(a) {
          e.call(this, a);
          this._worker = g.Player.Test.FakeSyncWorker.instance;
          this._worker.addEventListener("message", this._onWorkerMessage.bind(this));
          this._worker.addEventListener("syncmessage", this._onSyncWorkerMessage.bind(this));
        }
        __extends(b, e);
        b.prototype.onSendUpdates = function(a, b) {
          var c = a.getBytes();
          this._worker.postMessage({type:"gfx", updates:c, assets:b}, [c.buffer]);
        };
        b.prototype.onExernalCallback = function(a) {
          this._worker.postSyncMessage({type:"externalCallback", request:a});
        };
        b.prototype.onDisplayParameters = function(a) {
          this._worker.postMessage({type:"displayParameters", params:a});
        };
        b.prototype.onVideoPlaybackEvent = function(a, b, c) {
          this._worker.postMessage({type:"videoPlayback", id:a, eventType:b, data:c});
        };
        b.prototype.requestTimeline = function(a, b) {
          var c;
          switch(a) {
            case "AVM2":
              c = g.AVM2.timelineBuffer;
              break;
            case "Player":
              c = g.Player.timelineBuffer;
              break;
            case "SWF":
              c = g.SWF.timelineBuffer;
          }
          "clear" === b && c && c.reset();
          return Promise.resolve(c);
        };
        b.prototype._onWorkerMessage = function(a, b) {
          void 0 === b && (b = !0);
          var e = a.data;
          if ("object" === typeof e && null !== e) {
            switch(e.type) {
              case "player":
                var g = c.FromArrayBuffer(e.updates.buffer);
                if (b) {
                  this.processUpdates(g, e.assets);
                } else {
                  var k = new c;
                  this.processUpdates(g, e.assets, k);
                  a.result = k.toPlainObject();
                  a.handled = !0;
                }
                break;
              case "external":
                a.result = this.processExternalCommand(e.command);
                a.handled = !0;
                break;
              case "videoControl":
                a.result = this.processVideoControl(e.id, e.eventType, e.data);
                a.handled = !0;
                break;
              case "registerFontOrImage":
                this.processRegisterFontOrImage(e.syncId, e.symbolId, e.assetType, e.data, e.resolve), a.handled = !0;
            }
          }
        };
        b.prototype._onSyncWorkerMessage = function(a) {
          return this._onWorkerMessage(a, !1);
        };
        return b;
      }(m.EaselHost);
      e.TestEaselHost = w;
    })(m.Test || (m.Test = {}));
  })(g.GFX || (g.GFX = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load GFX Dependencies");

