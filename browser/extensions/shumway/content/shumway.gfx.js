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
Shumway$$inline_0.version = "0.10.182";
Shumway$$inline_0.build = "0195a96";
var jsGlobal = function() {
  return this || (0,eval)("this//# sourceURL=jsGlobal-getter");
}(), inBrowser = "undefined" !== typeof window && "document" in window && "plugins" in window.document, inFirefox = "undefined" !== typeof navigator && 0 <= navigator.userAgent.indexOf("Firefox");
function dumpLine(k) {
}
jsGlobal.performance || (jsGlobal.performance = {});
jsGlobal.performance.now || (jsGlobal.performance.now = "undefined" !== typeof dateNow ? dateNow : Date.now);
function lazyInitializer(k, p, g) {
  Object.defineProperty(k, p, {get:function() {
    var b = g();
    Object.defineProperty(k, p, {value:b, configurable:!0, enumerable:!0});
    return b;
  }, configurable:!0, enumerable:!0});
}
var START_TIME = performance.now();
(function(k) {
  function p(a) {
    return(a | 0) === a;
  }
  function g(a) {
    return "object" === typeof a || "function" === typeof a;
  }
  function b(a) {
    return String(Number(a)) === a;
  }
  function u(a) {
    var l = 0;
    if ("number" === typeof a) {
      return l = a | 0, a === l && 0 <= l ? !0 : a >>> 0 === a;
    }
    if ("string" !== typeof a) {
      return!1;
    }
    var c = a.length;
    if (0 === c) {
      return!1;
    }
    if ("0" === a) {
      return!0;
    }
    if (c > k.UINT32_CHAR_BUFFER_LENGTH) {
      return!1;
    }
    var e = 0, l = a.charCodeAt(e++) - 48;
    if (1 > l || 9 < l) {
      return!1;
    }
    for (var q = 0, s = 0;e < c;) {
      s = a.charCodeAt(e++) - 48;
      if (0 > s || 9 < s) {
        return!1;
      }
      q = l;
      l = 10 * l + s;
    }
    return q < k.UINT32_MAX_DIV_10 || q === k.UINT32_MAX_DIV_10 && s <= k.UINT32_MAX_MOD_10 ? !0 : !1;
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
  })(k.CharacterCodes || (k.CharacterCodes = {}));
  k.UINT32_CHAR_BUFFER_LENGTH = 10;
  k.UINT32_MAX = 4294967295;
  k.UINT32_MAX_DIV_10 = 429496729;
  k.UINT32_MAX_MOD_10 = 5;
  k.isString = function(a) {
    return "string" === typeof a;
  };
  k.isFunction = function(a) {
    return "function" === typeof a;
  };
  k.isNumber = function(a) {
    return "number" === typeof a;
  };
  k.isInteger = p;
  k.isArray = function(a) {
    return a instanceof Array;
  };
  k.isNumberOrString = function(a) {
    return "number" === typeof a || "string" === typeof a;
  };
  k.isObject = g;
  k.toNumber = function(a) {
    return+a;
  };
  k.isNumericString = b;
  k.isNumeric = function(a) {
    if ("number" === typeof a) {
      return!0;
    }
    if ("string" === typeof a) {
      var l = a.charCodeAt(0);
      return 65 <= l && 90 >= l || 97 <= l && 122 >= l || 36 === l || 95 === l ? !1 : u(a) || b(a);
    }
    return!1;
  };
  k.isIndex = u;
  k.isNullOrUndefined = function(a) {
    return void 0 == a;
  };
  var h;
  (function(a) {
    a.error = function(c) {
      console.error(c);
      throw Error(c);
    };
    a.assert = function(c, e) {
      void 0 === e && (e = "assertion failed");
      "" === c && (c = !0);
      if (!c) {
        if ("undefined" !== typeof console && "assert" in console) {
          throw console.assert(!1, e), Error(e);
        }
        a.error(e.toString());
      }
    };
    a.assertUnreachable = function(c) {
      throw Error("Reached unreachable location " + Error().stack.split("\n")[1] + c);
    };
    a.assertNotImplemented = function(c, e) {
      c || a.error("notImplemented: " + e);
    };
    a.warning = function(c, e, q) {
    };
    a.notUsed = function(c) {
    };
    a.notImplemented = function(c) {
    };
    a.dummyConstructor = function(c) {
    };
    a.abstractMethod = function(c) {
    };
    var l = {};
    a.somewhatImplemented = function(c) {
      l[c] || (l[c] = !0);
    };
    a.unexpected = function(c) {
      a.assert(!1, "Unexpected: " + c);
    };
    a.unexpectedCase = function(c) {
      a.assert(!1, "Unexpected Case: " + c);
    };
  })(h = k.Debug || (k.Debug = {}));
  k.getTicks = function() {
    return performance.now();
  };
  (function(a) {
    function l(e, q) {
      for (var c = 0, a = e.length;c < a;c++) {
        if (e[c] === q) {
          return c;
        }
      }
      e.push(q);
      return e.length - 1;
    }
    a.popManyInto = function(e, q, c) {
      for (var a = q - 1;0 <= a;a--) {
        c[a] = e.pop();
      }
      c.length = q;
    };
    a.popMany = function(e, q) {
      var c = e.length - q, a = e.slice(c, this.length);
      e.length = c;
      return a;
    };
    a.popManyIntoVoid = function(e, q) {
      e.length -= q;
    };
    a.pushMany = function(e, q) {
      for (var c = 0;c < q.length;c++) {
        e.push(q[c]);
      }
    };
    a.top = function(e) {
      return e.length && e[e.length - 1];
    };
    a.last = function(e) {
      return e.length && e[e.length - 1];
    };
    a.peek = function(e) {
      return e[e.length - 1];
    };
    a.indexOf = function(e, q) {
      for (var c = 0, a = e.length;c < a;c++) {
        if (e[c] === q) {
          return c;
        }
      }
      return-1;
    };
    a.equals = function(e, c) {
      if (e.length !== c.length) {
        return!1;
      }
      for (var s = 0;s < e.length;s++) {
        if (e[s] !== c[s]) {
          return!1;
        }
      }
      return!0;
    };
    a.pushUnique = l;
    a.unique = function(e) {
      for (var c = [], s = 0;s < e.length;s++) {
        l(c, e[s]);
      }
      return c;
    };
    a.copyFrom = function(e, c) {
      e.length = 0;
      a.pushMany(e, c);
    };
    a.ensureTypedArrayCapacity = function(e, c) {
      if (e.length < c) {
        var s = e;
        e = new e.constructor(k.IntegerUtilities.nearestPowerOfTwo(c));
        e.set(s, 0);
      }
      return e;
    };
    var c = function() {
      function e(e) {
        void 0 === e && (e = 16);
        this._f32 = this._i32 = this._u16 = this._u8 = null;
        this._offset = 0;
        this.ensureCapacity(e);
      }
      e.prototype.reset = function() {
        this._offset = 0;
      };
      Object.defineProperty(e.prototype, "offset", {get:function() {
        return this._offset;
      }, enumerable:!0, configurable:!0});
      e.prototype.getIndex = function(e) {
        return this._offset / e;
      };
      e.prototype.ensureAdditionalCapacity = function() {
        this.ensureCapacity(this._offset + 68);
      };
      e.prototype.ensureCapacity = function(e) {
        if (!this._u8) {
          this._u8 = new Uint8Array(e);
        } else {
          if (this._u8.length > e) {
            return;
          }
        }
        var c = 2 * this._u8.length;
        c < e && (c = e);
        e = new Uint8Array(c);
        e.set(this._u8, 0);
        this._u8 = e;
        this._u16 = new Uint16Array(e.buffer);
        this._i32 = new Int32Array(e.buffer);
        this._f32 = new Float32Array(e.buffer);
      };
      e.prototype.writeInt = function(e) {
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(e);
      };
      e.prototype.writeIntAt = function(e, c) {
        this.ensureCapacity(c + 4);
        this._i32[c >> 2] = e;
      };
      e.prototype.writeIntUnsafe = function(e) {
        this._i32[this._offset >> 2] = e;
        this._offset += 4;
      };
      e.prototype.writeFloat = function(e) {
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(e);
      };
      e.prototype.writeFloatUnsafe = function(e) {
        this._f32[this._offset >> 2] = e;
        this._offset += 4;
      };
      e.prototype.write4Floats = function(e, c, a, l) {
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(e, c, a, l);
      };
      e.prototype.write4FloatsUnsafe = function(e, c, a, l) {
        var d = this._offset >> 2;
        this._f32[d + 0] = e;
        this._f32[d + 1] = c;
        this._f32[d + 2] = a;
        this._f32[d + 3] = l;
        this._offset += 16;
      };
      e.prototype.write6Floats = function(e, c, a, l, d, t) {
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(e, c, a, l, d, t);
      };
      e.prototype.write6FloatsUnsafe = function(e, c, a, l, d, t) {
        var n = this._offset >> 2;
        this._f32[n + 0] = e;
        this._f32[n + 1] = c;
        this._f32[n + 2] = a;
        this._f32[n + 3] = l;
        this._f32[n + 4] = d;
        this._f32[n + 5] = t;
        this._offset += 24;
      };
      e.prototype.subF32View = function() {
        return this._f32.subarray(0, this._offset >> 2);
      };
      e.prototype.subI32View = function() {
        return this._i32.subarray(0, this._offset >> 2);
      };
      e.prototype.subU16View = function() {
        return this._u16.subarray(0, this._offset >> 1);
      };
      e.prototype.subU8View = function() {
        return this._u8.subarray(0, this._offset);
      };
      e.prototype.hashWords = function(e, c, a) {
        c = this._i32;
        for (var l = 0;l < a;l++) {
          e = (31 * e | 0) + c[l] | 0;
        }
        return e;
      };
      e.prototype.reserve = function(e) {
        e = e + 3 & -4;
        this.ensureCapacity(this._offset + e);
        this._offset += e;
      };
      return e;
    }();
    a.ArrayWriter = c;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
  var a = function() {
    function a(l) {
      this._u8 = new Uint8Array(l);
      this._u16 = new Uint16Array(l);
      this._i32 = new Int32Array(l);
      this._f32 = new Float32Array(l);
      this._offset = 0;
    }
    Object.defineProperty(a.prototype, "offset", {get:function() {
      return this._offset;
    }, enumerable:!0, configurable:!0});
    a.prototype.isEmpty = function() {
      return this._offset === this._u8.length;
    };
    a.prototype.readInt = function() {
      var a = this._i32[this._offset >> 2];
      this._offset += 4;
      return a;
    };
    a.prototype.readFloat = function() {
      var a = this._f32[this._offset >> 2];
      this._offset += 4;
      return a;
    };
    return a;
  }();
  k.ArrayReader = a;
  (function(a) {
    function l(e, c) {
      return Object.prototype.hasOwnProperty.call(e, c);
    }
    function c(e, c) {
      for (var a in c) {
        l(c, a) && (e[a] = c[a]);
      }
    }
    a.boxValue = function(e) {
      return void 0 == e || g(e) ? e : Object(e);
    };
    a.toKeyValueArray = function(e) {
      var c = Object.prototype.hasOwnProperty, a = [], l;
      for (l in e) {
        c.call(e, l) && a.push([l, e[l]]);
      }
      return a;
    };
    a.isPrototypeWriteable = function(e) {
      return Object.getOwnPropertyDescriptor(e, "prototype").writable;
    };
    a.hasOwnProperty = l;
    a.propertyIsEnumerable = function(e, c) {
      return Object.prototype.propertyIsEnumerable.call(e, c);
    };
    a.getOwnPropertyDescriptor = function(e, c) {
      return Object.getOwnPropertyDescriptor(e, c);
    };
    a.hasOwnGetter = function(e, c) {
      var a = Object.getOwnPropertyDescriptor(e, c);
      return!(!a || !a.get);
    };
    a.getOwnGetter = function(e, c) {
      var a = Object.getOwnPropertyDescriptor(e, c);
      return a ? a.get : null;
    };
    a.hasOwnSetter = function(e, c) {
      var a = Object.getOwnPropertyDescriptor(e, c);
      return!(!a || !a.set);
    };
    a.createMap = function() {
      return Object.create(null);
    };
    a.createArrayMap = function() {
      return[];
    };
    a.defineReadOnlyProperty = function(e, c, a) {
      Object.defineProperty(e, c, {value:a, writable:!1, configurable:!0, enumerable:!1});
    };
    a.getOwnPropertyDescriptors = function(e) {
      for (var c = a.createMap(), s = Object.getOwnPropertyNames(e), l = 0;l < s.length;l++) {
        c[s[l]] = Object.getOwnPropertyDescriptor(e, s[l]);
      }
      return c;
    };
    a.cloneObject = function(e) {
      var q = Object.create(Object.getPrototypeOf(e));
      c(q, e);
      return q;
    };
    a.copyProperties = function(e, c) {
      for (var a in c) {
        e[a] = c[a];
      }
    };
    a.copyOwnProperties = c;
    a.copyOwnPropertyDescriptors = function(e, c, a) {
      void 0 === a && (a = !0);
      for (var y in c) {
        if (l(c, y)) {
          var d = Object.getOwnPropertyDescriptor(c, y);
          if (a || !l(e, y)) {
            try {
              Object.defineProperty(e, y, d);
            } catch (t) {
            }
          }
        }
      }
    };
    a.getLatestGetterOrSetterPropertyDescriptor = function(e, c) {
      for (var a = {};e;) {
        var l = Object.getOwnPropertyDescriptor(e, c);
        l && (a.get = a.get || l.get, a.set = a.set || l.set);
        if (a.get && a.set) {
          break;
        }
        e = Object.getPrototypeOf(e);
      }
      return a;
    };
    a.defineNonEnumerableGetterOrSetter = function(e, c, l, y) {
      var d = a.getLatestGetterOrSetterPropertyDescriptor(e, c);
      d.configurable = !0;
      d.enumerable = !1;
      y ? d.get = l : d.set = l;
      Object.defineProperty(e, c, d);
    };
    a.defineNonEnumerableGetter = function(c, q, a) {
      Object.defineProperty(c, q, {get:a, configurable:!0, enumerable:!1});
    };
    a.defineNonEnumerableSetter = function(c, q, a) {
      Object.defineProperty(c, q, {set:a, configurable:!0, enumerable:!1});
    };
    a.defineNonEnumerableProperty = function(c, q, a) {
      Object.defineProperty(c, q, {value:a, writable:!0, configurable:!0, enumerable:!1});
    };
    a.defineNonEnumerableForwardingProperty = function(c, q, a) {
      Object.defineProperty(c, q, {get:d.makeForwardingGetter(a), set:d.makeForwardingSetter(a), writable:!0, configurable:!0, enumerable:!1});
    };
    a.defineNewNonEnumerableProperty = function(c, q, l) {
      a.defineNonEnumerableProperty(c, q, l);
    };
    a.createPublicAliases = function(c, q) {
      for (var a = {value:null, writable:!0, configurable:!0, enumerable:!1}, l = 0;l < q.length;l++) {
        var d = q[l];
        a.value = c[d];
        Object.defineProperty(c, "$Bg" + d, a);
      }
    };
  })(k.ObjectUtilities || (k.ObjectUtilities = {}));
  var d;
  (function(a) {
    a.makeForwardingGetter = function(a) {
      return new Function('return this["' + a + '"]//# sourceURL=fwd-get-' + a + ".as");
    };
    a.makeForwardingSetter = function(a) {
      return new Function("value", 'this["' + a + '"] = value;//# sourceURL=fwd-set-' + a + ".as");
    };
    a.bindSafely = function(a, c) {
      function e() {
        return a.apply(c, arguments);
      }
      e.boundTo = c;
      return e;
    };
  })(d = k.FunctionUtilities || (k.FunctionUtilities = {}));
  (function(a) {
    function l(c) {
      return "string" === typeof c ? '"' + c + '"' : "number" === typeof c || "boolean" === typeof c ? String(c) : c instanceof Array ? "[] " + c.length : typeof c;
    }
    a.repeatString = function(c, e) {
      for (var q = "", a = 0;a < e;a++) {
        q += c;
      }
      return q;
    };
    a.memorySizeToString = function(c) {
      c |= 0;
      return 1024 > c ? c + " B" : 1048576 > c ? (c / 1024).toFixed(2) + "KB" : (c / 1048576).toFixed(2) + "MB";
    };
    a.toSafeString = l;
    a.toSafeArrayString = function(c) {
      for (var e = [], q = 0;q < c.length;q++) {
        e.push(l(c[q]));
      }
      return e.join(", ");
    };
    a.utf8decode = function(c) {
      for (var e = new Uint8Array(4 * c.length), q = 0, a = 0, l = c.length;a < l;a++) {
        var s = c.charCodeAt(a);
        if (127 >= s) {
          e[q++] = s;
        } else {
          if (55296 <= s && 56319 >= s) {
            var y = c.charCodeAt(a + 1);
            56320 <= y && 57343 >= y && (s = ((s & 1023) << 10) + (y & 1023) + 65536, ++a);
          }
          0 !== (s & 4292870144) ? (e[q++] = 248 | s >>> 24 & 3, e[q++] = 128 | s >>> 18 & 63, e[q++] = 128 | s >>> 12 & 63, e[q++] = 128 | s >>> 6 & 63) : 0 !== (s & 4294901760) ? (e[q++] = 240 | s >>> 18 & 7, e[q++] = 128 | s >>> 12 & 63, e[q++] = 128 | s >>> 6 & 63) : 0 !== (s & 4294965248) ? (e[q++] = 224 | s >>> 12 & 15, e[q++] = 128 | s >>> 6 & 63) : e[q++] = 192 | s >>> 6 & 31;
          e[q++] = 128 | s & 63;
        }
      }
      return e.subarray(0, q);
    };
    a.utf8encode = function(c) {
      for (var e = 0, q = "";e < c.length;) {
        var a = c[e++] & 255;
        if (127 >= a) {
          q += String.fromCharCode(a);
        } else {
          var l = 192, s = 5;
          do {
            if ((a & (l >> 1 | 128)) === l) {
              break;
            }
            l = l >> 1 | 128;
            --s;
          } while (0 <= s);
          if (0 >= s) {
            q += String.fromCharCode(a);
          } else {
            for (var a = a & (1 << s) - 1, l = !1, y = 5;y >= s;--y) {
              var d = c[e++];
              if (128 != (d & 192)) {
                l = !0;
                break;
              }
              a = a << 6 | d & 63;
            }
            if (l) {
              for (s = e - (7 - y);s < e;++s) {
                q += String.fromCharCode(c[s] & 255);
              }
            } else {
              q = 65536 <= a ? q + String.fromCharCode(a - 65536 >> 10 & 1023 | 55296, a & 1023 | 56320) : q + String.fromCharCode(a);
            }
          }
        }
      }
      return q;
    };
    a.base64ArrayBuffer = function(c) {
      var e = "";
      c = new Uint8Array(c);
      for (var q = c.byteLength, a = q % 3, q = q - a, l, s, y, d, t = 0;t < q;t += 3) {
        d = c[t] << 16 | c[t + 1] << 8 | c[t + 2], l = (d & 16515072) >> 18, s = (d & 258048) >> 12, y = (d & 4032) >> 6, d &= 63, e += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[l] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[s] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[y] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[d];
      }
      1 == a ? (d = c[q], e += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(d & 252) >> 2] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(d & 3) << 4] + "==") : 2 == a && (d = c[q] << 8 | c[q + 1], e += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(d & 64512) >> 10] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(d & 1008) >> 4] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(d & 15) << 
      2] + "=");
      return e;
    };
    a.escapeString = function(c) {
      void 0 !== c && (c = c.replace(/[^\w$]/gi, "$"), /^\d/.test(c) && (c = "$" + c));
      return c;
    };
    a.fromCharCodeArray = function(c) {
      for (var e = "", q = 0;q < c.length;q += 16384) {
        var a = Math.min(c.length - q, 16384), e = e + String.fromCharCode.apply(null, c.subarray(q, q + a))
      }
      return e;
    };
    a.variableLengthEncodeInt32 = function(c) {
      for (var e = 32 - Math.clz32(c), q = Math.ceil(e / 6), e = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[q], q = q - 1;0 <= q;q--) {
        e += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[c >> 6 * q & 63];
      }
      return e;
    };
    a.toEncoding = function(c) {
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[c];
    };
    a.fromEncoding = function(c) {
      if (65 <= c && 90 >= c) {
        return c - 65;
      }
      if (97 <= c && 122 >= c) {
        return c - 71;
      }
      if (48 <= c && 57 >= c) {
        return c + 4;
      }
      if (36 === c) {
        return 62;
      }
      if (95 === c) {
        return 63;
      }
    };
    a.variableLengthDecodeInt32 = function(c) {
      for (var e = a.fromEncoding(c.charCodeAt(0)), q = 0, l = 0;l < e;l++) {
        var s = 6 * (e - l - 1), q = q | a.fromEncoding(c.charCodeAt(1 + l)) << s
      }
      return q;
    };
    a.trimMiddle = function(c, e) {
      if (c.length <= e) {
        return c;
      }
      var q = e >> 1, a = e - q - 1;
      return c.substr(0, q) + "\u2026" + c.substr(c.length - a, a);
    };
    a.multiple = function(c, e) {
      for (var q = "", a = 0;a < e;a++) {
        q += c;
      }
      return q;
    };
    a.indexOfAny = function(c, e, q) {
      for (var a = c.length, l = 0;l < e.length;l++) {
        var s = c.indexOf(e[l], q);
        0 <= s && (a = Math.min(a, s));
      }
      return a === c.length ? -1 : a;
    };
    var c = Array(3), e = Array(4), q = Array(5), s = Array(6), y = Array(7), d = Array(8), n = Array(9);
    a.concat3 = function(e, q, a) {
      c[0] = e;
      c[1] = q;
      c[2] = a;
      return c.join("");
    };
    a.concat4 = function(c, q, a, l) {
      e[0] = c;
      e[1] = q;
      e[2] = a;
      e[3] = l;
      return e.join("");
    };
    a.concat5 = function(c, e, a, l, s) {
      q[0] = c;
      q[1] = e;
      q[2] = a;
      q[3] = l;
      q[4] = s;
      return q.join("");
    };
    a.concat6 = function(c, e, q, a, l, y) {
      s[0] = c;
      s[1] = e;
      s[2] = q;
      s[3] = a;
      s[4] = l;
      s[5] = y;
      return s.join("");
    };
    a.concat7 = function(c, e, q, a, l, s, d) {
      y[0] = c;
      y[1] = e;
      y[2] = q;
      y[3] = a;
      y[4] = l;
      y[5] = s;
      y[6] = d;
      return y.join("");
    };
    a.concat8 = function(c, e, q, a, l, s, y, t) {
      d[0] = c;
      d[1] = e;
      d[2] = q;
      d[3] = a;
      d[4] = l;
      d[5] = s;
      d[6] = y;
      d[7] = t;
      return d.join("");
    };
    a.concat9 = function(c, e, q, a, l, s, y, d, t) {
      n[0] = c;
      n[1] = e;
      n[2] = q;
      n[3] = a;
      n[4] = l;
      n[5] = s;
      n[6] = y;
      n[7] = d;
      n[8] = t;
      return n.join("");
    };
  })(k.StringUtilities || (k.StringUtilities = {}));
  (function(a) {
    var l = new Uint8Array([7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]), c = new Int32Array([-680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426, -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162, 1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632, 
    643717713, -373897302, -701558691, 38016083, -660478335, -405537848, 568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784, 1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556, -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222, -722521979, 76029189, -640364487, -421815835, 530742520, -995338651, -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606, -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649, 
    -145523070, -1120210379, 718787259, -343485551]);
    a.hashBytesTo32BitsMD5 = function(e, q, a) {
      var y = 1732584193, d = -271733879, t = -1732584194, n = 271733878, b = a + 72 & -64, f = new Uint8Array(b), h;
      for (h = 0;h < a;++h) {
        f[h] = e[q++];
      }
      f[h++] = 128;
      for (e = b - 8;h < e;) {
        f[h++] = 0;
      }
      f[h++] = a << 3 & 255;
      f[h++] = a >> 5 & 255;
      f[h++] = a >> 13 & 255;
      f[h++] = a >> 21 & 255;
      f[h++] = a >>> 29 & 255;
      f[h++] = 0;
      f[h++] = 0;
      f[h++] = 0;
      e = new Int32Array(16);
      for (h = 0;h < b;) {
        for (a = 0;16 > a;++a, h += 4) {
          e[a] = f[h] | f[h + 1] << 8 | f[h + 2] << 16 | f[h + 3] << 24;
        }
        var m = y;
        q = d;
        var v = t, r = n, g, w;
        for (a = 0;64 > a;++a) {
          16 > a ? (g = q & v | ~q & r, w = a) : 32 > a ? (g = r & q | ~r & v, w = 5 * a + 1 & 15) : 48 > a ? (g = q ^ v ^ r, w = 3 * a + 5 & 15) : (g = v ^ (q | ~r), w = 7 * a & 15);
          var k = r, m = m + g + c[a] + e[w] | 0;
          g = l[a];
          r = v;
          v = q;
          q = q + (m << g | m >>> 32 - g) | 0;
          m = k;
        }
        y = y + m | 0;
        d = d + q | 0;
        t = t + v | 0;
        n = n + r | 0;
      }
      return y;
    };
    a.hashBytesTo32BitsAdler = function(c, q, a) {
      var l = 1, d = 0;
      for (a = q + a;q < a;++q) {
        l = (l + (c[q] & 255)) % 65521, d = (d + l) % 65521;
      }
      return d << 16 | l;
    };
  })(k.HashUtilities || (k.HashUtilities = {}));
  var n = function() {
    function a() {
    }
    a.seed = function(l) {
      a._state[0] = l;
      a._state[1] = l;
    };
    a.next = function() {
      var a = this._state, c = Math.imul(18273, a[0] & 65535) + (a[0] >>> 16) | 0;
      a[0] = c;
      var e = Math.imul(36969, a[1] & 65535) + (a[1] >>> 16) | 0;
      a[1] = e;
      a = (c << 16) + (e & 65535) | 0;
      return 2.3283064365386963E-10 * (0 > a ? a + 4294967296 : a);
    };
    a._state = new Uint32Array([57005, 48879]);
    return a;
  }();
  k.Random = n;
  Math.random = function() {
    return n.next();
  };
  (function() {
    function a() {
      this.id = "$weakmap" + l++;
    }
    if ("function" !== typeof jsGlobal.WeakMap) {
      var l = 0;
      a.prototype = {has:function(c) {
        return c.hasOwnProperty(this.id);
      }, get:function(c, e) {
        return c.hasOwnProperty(this.id) ? c[this.id] : e;
      }, set:function(c, e) {
        Object.defineProperty(c, this.id, {value:e, enumerable:!1, configurable:!0});
      }, delete:function(c) {
        delete c[this.id];
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
      this._map ? this._map.set(a, null) : this._list.push(a);
    };
    a.prototype.remove = function(a) {
      this._map ? this._map.delete(a) : this._list[this._list.indexOf(a)] = null;
    };
    a.prototype.forEach = function(a) {
      if (this._map) {
        "undefined" !== typeof netscape && netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect"), Components.utils.nondeterministicGetWeakMapKeys(this._map).forEach(function(c) {
          0 !== c._referenceCount && a(c);
        });
      } else {
        for (var c = this._list, e = 0, q = 0;q < c.length;q++) {
          var s = c[q];
          s && (0 === s._referenceCount ? (c[q] = null, e++) : a(s));
        }
        if (16 < e && e > c.length >> 2) {
          e = [];
          for (q = 0;q < c.length;q++) {
            (s = c[q]) && 0 < s._referenceCount && e.push(s);
          }
          this._list = e;
        }
      }
    };
    Object.defineProperty(a.prototype, "length", {get:function() {
      return this._map ? -1 : this._list.length;
    }, enumerable:!0, configurable:!0});
    return a;
  }();
  k.WeakList = a;
  var f;
  (function(a) {
    a.pow2 = function(a) {
      return a === (a | 0) ? 0 > a ? 1 / (1 << -a) : 1 << a : Math.pow(2, a);
    };
    a.clamp = function(a, c, e) {
      return Math.max(c, Math.min(e, a));
    };
    a.roundHalfEven = function(a) {
      if (.5 === Math.abs(a % 1)) {
        var c = Math.floor(a);
        return 0 === c % 2 ? c : Math.ceil(a);
      }
      return Math.round(a);
    };
    a.altTieBreakRound = function(a, c) {
      return.5 !== Math.abs(a % 1) || c ? Math.round(a) : a | 0;
    };
    a.epsilonEquals = function(a, c) {
      return 1E-7 > Math.abs(a - c);
    };
  })(f = k.NumberUtilities || (k.NumberUtilities = {}));
  (function(a) {
    a[a.MaxU16 = 65535] = "MaxU16";
    a[a.MaxI16 = 32767] = "MaxI16";
    a[a.MinI16 = -32768] = "MinI16";
  })(k.Numbers || (k.Numbers = {}));
  var v;
  (function(a) {
    function l(c) {
      return 256 * c << 16 >> 16;
    }
    var c = new ArrayBuffer(8);
    a.i8 = new Int8Array(c);
    a.u8 = new Uint8Array(c);
    a.i32 = new Int32Array(c);
    a.f32 = new Float32Array(c);
    a.f64 = new Float64Array(c);
    a.nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
    a.floatToInt32 = function(c) {
      a.f32[0] = c;
      return a.i32[0];
    };
    a.int32ToFloat = function(c) {
      a.i32[0] = c;
      return a.f32[0];
    };
    a.swap16 = function(c) {
      return(c & 255) << 8 | c >> 8 & 255;
    };
    a.swap32 = function(c) {
      return(c & 255) << 24 | (c & 65280) << 8 | c >> 8 & 65280 | c >> 24 & 255;
    };
    a.toS8U8 = l;
    a.fromS8U8 = function(c) {
      return c / 256;
    };
    a.clampS8U8 = function(c) {
      return l(c) / 256;
    };
    a.toS16 = function(c) {
      return c << 16 >> 16;
    };
    a.bitCount = function(c) {
      c -= c >> 1 & 1431655765;
      c = (c & 858993459) + (c >> 2 & 858993459);
      return 16843009 * (c + (c >> 4) & 252645135) >> 24;
    };
    a.ones = function(c) {
      c -= c >> 1 & 1431655765;
      c = (c & 858993459) + (c >> 2 & 858993459);
      return 16843009 * (c + (c >> 4) & 252645135) >> 24;
    };
    a.trailingZeros = function(c) {
      return a.ones((c & -c) - 1);
    };
    a.getFlags = function(c, q) {
      var a = "";
      for (c = 0;c < q.length;c++) {
        c & 1 << c && (a += q[c] + " ");
      }
      return 0 === a.length ? "" : a.trim();
    };
    a.isPowerOfTwo = function(c) {
      return c && 0 === (c & c - 1);
    };
    a.roundToMultipleOfFour = function(c) {
      return c + 3 & -4;
    };
    a.nearestPowerOfTwo = function(c) {
      c--;
      c |= c >> 1;
      c |= c >> 2;
      c |= c >> 4;
      c |= c >> 8;
      c |= c >> 16;
      c++;
      return c;
    };
    a.roundToMultipleOfPowerOfTwo = function(c, q) {
      var a = (1 << q) - 1;
      return c + a & ~a;
    };
    Math.imul || (Math.imul = function(c, q) {
      var a = c & 65535, l = q & 65535;
      return a * l + ((c >>> 16 & 65535) * l + a * (q >>> 16 & 65535) << 16 >>> 0) | 0;
    });
    Math.clz32 || (Math.clz32 = function(c) {
      c |= c >> 1;
      c |= c >> 2;
      c |= c >> 4;
      c |= c >> 8;
      return 32 - a.ones(c | c >> 16);
    });
  })(v = k.IntegerUtilities || (k.IntegerUtilities = {}));
  (function(a) {
    function l(c, e, q, a, l, d) {
      return(q - c) * (d - e) - (a - e) * (l - c);
    }
    a.pointInPolygon = function(c, e, q) {
      for (var a = 0, l = q.length - 2, d = 0;d < l;d += 2) {
        var t = q[d + 0], n = q[d + 1], f = q[d + 2], b = q[d + 3];
        (n <= e && b > e || n > e && b <= e) && c < t + (e - n) / (b - n) * (f - t) && a++;
      }
      return 1 === (a & 1);
    };
    a.signedArea = l;
    a.counterClockwise = function(c, e, q, a, d, I) {
      return 0 < l(c, e, q, a, d, I);
    };
    a.clockwise = function(c, e, q, a, d, I) {
      return 0 > l(c, e, q, a, d, I);
    };
    a.pointInPolygonInt32 = function(c, e, q) {
      c |= 0;
      e |= 0;
      for (var a = 0, l = q.length - 2, d = 0;d < l;d += 2) {
        var t = q[d + 0], n = q[d + 1], f = q[d + 2], b = q[d + 3];
        (n <= e && b > e || n > e && b <= e) && c < t + (e - n) / (b - n) * (f - t) && a++;
      }
      return 1 === (a & 1);
    };
  })(k.GeometricUtilities || (k.GeometricUtilities = {}));
  (function(a) {
    a[a.Error = 1] = "Error";
    a[a.Warn = 2] = "Warn";
    a[a.Debug = 4] = "Debug";
    a[a.Log = 8] = "Log";
    a[a.Info = 16] = "Info";
    a[a.All = 31] = "All";
  })(k.LogLevel || (k.LogLevel = {}));
  a = function() {
    function a(l, c) {
      void 0 === l && (l = !1);
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = l;
      this._out = c || a._consoleOut;
      this._outNoNewline = c || a._consoleOutNoNewline;
    }
    a.prototype.write = function(a, c) {
      void 0 === a && (a = "");
      void 0 === c && (c = !1);
      this._suppressOutput || this._outNoNewline((c ? this._padding : "") + a);
    };
    a.prototype.writeLn = function(a) {
      void 0 === a && (a = "");
      this._suppressOutput || this._out(this._padding + a);
    };
    a.prototype.writeObject = function(a, c) {
      void 0 === a && (a = "");
      this._suppressOutput || this._out(this._padding + a, c);
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
        for (var c = 0;c < a.length;c++) {
          this.writeLn(" * " + a[c]);
        }
        this.writeLn(" */");
      }
    };
    a.prototype.writeLns = function(a) {
      a = a.split("\n");
      for (var c = 0;c < a.length;c++) {
        this.writeLn(a[c]);
      }
    };
    a.prototype.errorLn = function(l) {
      a.logLevel & 1 && this.boldRedLn(l);
    };
    a.prototype.warnLn = function(l) {
      a.logLevel & 2 && this.yellowLn(l);
    };
    a.prototype.debugLn = function(l) {
      a.logLevel & 4 && this.purpleLn(l);
    };
    a.prototype.logLn = function(l) {
      a.logLevel & 8 && this.writeLn(l);
    };
    a.prototype.infoLn = function(l) {
      a.logLevel & 16 && this.writeLn(l);
    };
    a.prototype.yellowLn = function(l) {
      this.colorLn(a.YELLOW, l);
    };
    a.prototype.greenLn = function(l) {
      this.colorLn(a.GREEN, l);
    };
    a.prototype.boldRedLn = function(l) {
      this.colorLn(a.BOLD_RED, l);
    };
    a.prototype.redLn = function(l) {
      this.colorLn(a.RED, l);
    };
    a.prototype.purpleLn = function(l) {
      this.colorLn(a.PURPLE, l);
    };
    a.prototype.colorLn = function(l, c) {
      this._suppressOutput || (inBrowser ? this._out(this._padding + c) : this._out(this._padding + l + c + a.ENDC));
    };
    a.prototype.redLns = function(l) {
      this.colorLns(a.RED, l);
    };
    a.prototype.colorLns = function(a, c) {
      for (var e = c.split("\n"), q = 0;q < e.length;q++) {
        this.colorLn(a, e[q]);
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
    a.prototype.writeArray = function(a, c, e) {
      void 0 === c && (c = !1);
      void 0 === e && (e = !1);
      c = c || !1;
      for (var q = 0, s = a.length;q < s;q++) {
        var d = "";
        c && (d = null === a[q] ? "null" : void 0 === a[q] ? "undefined" : a[q].constructor.name, d += " ");
        var I = e ? "" : ("" + q).padRight(" ", 4);
        this.writeLn(I + d + a[q]);
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
  k.IndentingWriter = a;
  var m = function() {
    return function(a, l) {
      this.value = a;
      this.next = l;
    };
  }(), a = function() {
    function a(l) {
      this._compare = l;
      this._head = null;
      this._length = 0;
    }
    a.prototype.push = function(a) {
      this._length++;
      if (this._head) {
        var c = this._head, e = null;
        a = new m(a, null);
        for (var q = this._compare;c;) {
          if (0 < q(c.value, a.value)) {
            e ? (a.next = c, e.next = a) : (a.next = this._head, this._head = a);
            return;
          }
          e = c;
          c = c.next;
        }
        e.next = a;
      } else {
        this._head = new m(a, null);
      }
    };
    a.prototype.forEach = function(l) {
      for (var c = this._head, e = null;c;) {
        var q = l(c.value);
        if (q === a.RETURN) {
          break;
        } else {
          q === a.DELETE ? c = e ? e.next = c.next : this._head = this._head.next : (e = c, c = c.next);
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
      for (var c = this._head;c;) {
        if (c.value === a) {
          return!0;
        }
        c = c.next;
      }
      return!1;
    };
    a.prototype.toString = function() {
      for (var a = "[", c = this._head;c;) {
        a += c.value.toString(), (c = c.next) && (a += ",");
      }
      return a + "]";
    };
    a.RETURN = 1;
    a.DELETE = 2;
    return a;
  }();
  k.SortedList = a;
  a = function() {
    function a(l, c) {
      void 0 === c && (c = 12);
      this.start = this.index = 0;
      this._size = 1 << c;
      this._mask = this._size - 1;
      this.array = new l(this._size);
    }
    a.prototype.get = function(a) {
      return this.array[a];
    };
    a.prototype.forEachInReverse = function(a) {
      if (!this.isEmpty()) {
        for (var c = 0 === this.index ? this._size - 1 : this.index - 1, e = this.start - 1 & this._mask;c !== e && !a(this.array[c], c);) {
          c = 0 === c ? this._size - 1 : c - 1;
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
  k.CircularBuffer = a;
  (function(a) {
    function l(c) {
      return c + (a.BITS_PER_WORD - 1) >> a.ADDRESS_BITS_PER_WORD << a.ADDRESS_BITS_PER_WORD;
    }
    function c(c, a) {
      c = c || "1";
      a = a || "0";
      for (var e = "", q = 0;q < length;q++) {
        e += this.get(q) ? c : a;
      }
      return e;
    }
    function e(c) {
      for (var a = [], e = 0;e < length;e++) {
        this.get(e) && a.push(c ? c[e] : e);
      }
      return a.join(", ");
    }
    a.ADDRESS_BITS_PER_WORD = 5;
    a.BITS_PER_WORD = 1 << a.ADDRESS_BITS_PER_WORD;
    a.BIT_INDEX_MASK = a.BITS_PER_WORD - 1;
    var q = function() {
      function c(e) {
        this.size = l(e);
        this.dirty = this.count = 0;
        this.length = e;
        this.bits = new Uint32Array(this.size >> a.ADDRESS_BITS_PER_WORD);
      }
      c.prototype.recount = function() {
        if (this.dirty) {
          for (var c = this.bits, a = 0, e = 0, q = c.length;e < q;e++) {
            var s = c[e], s = s - (s >> 1 & 1431655765), s = (s & 858993459) + (s >> 2 & 858993459), a = a + (16843009 * (s + (s >> 4) & 252645135) >> 24)
          }
          this.count = a;
          this.dirty = 0;
        }
      };
      c.prototype.set = function(c) {
        var e = c >> a.ADDRESS_BITS_PER_WORD, q = this.bits[e];
        c = q | 1 << (c & a.BIT_INDEX_MASK);
        this.bits[e] = c;
        this.dirty |= q ^ c;
      };
      c.prototype.setAll = function() {
        for (var c = this.bits, a = 0, e = c.length;a < e;a++) {
          c[a] = 4294967295;
        }
        this.count = this.size;
        this.dirty = 0;
      };
      c.prototype.assign = function(c) {
        this.count = c.count;
        this.dirty = c.dirty;
        this.size = c.size;
        for (var a = 0, e = this.bits.length;a < e;a++) {
          this.bits[a] = c.bits[a];
        }
      };
      c.prototype.clear = function(c) {
        var e = c >> a.ADDRESS_BITS_PER_WORD, q = this.bits[e];
        c = q & ~(1 << (c & a.BIT_INDEX_MASK));
        this.bits[e] = c;
        this.dirty |= q ^ c;
      };
      c.prototype.get = function(c) {
        return 0 !== (this.bits[c >> a.ADDRESS_BITS_PER_WORD] & 1 << (c & a.BIT_INDEX_MASK));
      };
      c.prototype.clearAll = function() {
        for (var c = this.bits, a = 0, e = c.length;a < e;a++) {
          c[a] = 0;
        }
        this.dirty = this.count = 0;
      };
      c.prototype._union = function(c) {
        var a = this.dirty, e = this.bits;
        c = c.bits;
        for (var q = 0, s = e.length;q < s;q++) {
          var l = e[q], d = l | c[q];
          e[q] = d;
          a |= l ^ d;
        }
        this.dirty = a;
      };
      c.prototype.intersect = function(c) {
        var a = this.dirty, e = this.bits;
        c = c.bits;
        for (var q = 0, s = e.length;q < s;q++) {
          var l = e[q], d = l & c[q];
          e[q] = d;
          a |= l ^ d;
        }
        this.dirty = a;
      };
      c.prototype.subtract = function(c) {
        var a = this.dirty, e = this.bits;
        c = c.bits;
        for (var q = 0, s = e.length;q < s;q++) {
          var l = e[q], d = l & ~c[q];
          e[q] = d;
          a |= l ^ d;
        }
        this.dirty = a;
      };
      c.prototype.negate = function() {
        for (var c = this.dirty, a = this.bits, e = 0, q = a.length;e < q;e++) {
          var s = a[e], l = ~s;
          a[e] = l;
          c |= s ^ l;
        }
        this.dirty = c;
      };
      c.prototype.forEach = function(c) {
        for (var e = this.bits, q = 0, s = e.length;q < s;q++) {
          var l = e[q];
          if (l) {
            for (var d = 0;d < a.BITS_PER_WORD;d++) {
              l & 1 << d && c(q * a.BITS_PER_WORD + d);
            }
          }
        }
      };
      c.prototype.toArray = function() {
        for (var c = [], e = this.bits, q = 0, s = e.length;q < s;q++) {
          var l = e[q];
          if (l) {
            for (var d = 0;d < a.BITS_PER_WORD;d++) {
              l & 1 << d && c.push(q * a.BITS_PER_WORD + d);
            }
          }
        }
        return c;
      };
      c.prototype.equals = function(c) {
        if (this.size !== c.size) {
          return!1;
        }
        var a = this.bits;
        c = c.bits;
        for (var e = 0, q = a.length;e < q;e++) {
          if (a[e] !== c[e]) {
            return!1;
          }
        }
        return!0;
      };
      c.prototype.contains = function(c) {
        if (this.size !== c.size) {
          return!1;
        }
        var a = this.bits;
        c = c.bits;
        for (var e = 0, q = a.length;e < q;e++) {
          if ((a[e] | c[e]) !== a[e]) {
            return!1;
          }
        }
        return!0;
      };
      c.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      c.prototype.clone = function() {
        var a = new c(this.length);
        a._union(this);
        return a;
      };
      return c;
    }();
    a.Uint32ArrayBitSet = q;
    var s = function() {
      function c(a) {
        this.dirty = this.count = 0;
        this.size = l(a);
        this.bits = 0;
        this.singleWord = !0;
        this.length = a;
      }
      c.prototype.recount = function() {
        if (this.dirty) {
          var c = this.bits, c = c - (c >> 1 & 1431655765), c = (c & 858993459) + (c >> 2 & 858993459);
          this.count = 0 + (16843009 * (c + (c >> 4) & 252645135) >> 24);
          this.dirty = 0;
        }
      };
      c.prototype.set = function(c) {
        var e = this.bits;
        this.bits = c = e | 1 << (c & a.BIT_INDEX_MASK);
        this.dirty |= e ^ c;
      };
      c.prototype.setAll = function() {
        this.bits = 4294967295;
        this.count = this.size;
        this.dirty = 0;
      };
      c.prototype.assign = function(c) {
        this.count = c.count;
        this.dirty = c.dirty;
        this.size = c.size;
        this.bits = c.bits;
      };
      c.prototype.clear = function(c) {
        var e = this.bits;
        this.bits = c = e & ~(1 << (c & a.BIT_INDEX_MASK));
        this.dirty |= e ^ c;
      };
      c.prototype.get = function(c) {
        return 0 !== (this.bits & 1 << (c & a.BIT_INDEX_MASK));
      };
      c.prototype.clearAll = function() {
        this.dirty = this.count = this.bits = 0;
      };
      c.prototype._union = function(c) {
        var a = this.bits;
        this.bits = c = a | c.bits;
        this.dirty = a ^ c;
      };
      c.prototype.intersect = function(c) {
        var a = this.bits;
        this.bits = c = a & c.bits;
        this.dirty = a ^ c;
      };
      c.prototype.subtract = function(c) {
        var a = this.bits;
        this.bits = c = a & ~c.bits;
        this.dirty = a ^ c;
      };
      c.prototype.negate = function() {
        var c = this.bits, a = ~c;
        this.bits = a;
        this.dirty = c ^ a;
      };
      c.prototype.forEach = function(c) {
        var e = this.bits;
        if (e) {
          for (var q = 0;q < a.BITS_PER_WORD;q++) {
            e & 1 << q && c(q);
          }
        }
      };
      c.prototype.toArray = function() {
        var c = [], e = this.bits;
        if (e) {
          for (var q = 0;q < a.BITS_PER_WORD;q++) {
            e & 1 << q && c.push(q);
          }
        }
        return c;
      };
      c.prototype.equals = function(c) {
        return this.bits === c.bits;
      };
      c.prototype.contains = function(c) {
        var a = this.bits;
        return(a | c.bits) === a;
      };
      c.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      c.prototype.clone = function() {
        var a = new c(this.length);
        a._union(this);
        return a;
      };
      return c;
    }();
    a.Uint32BitSet = s;
    s.prototype.toString = e;
    s.prototype.toBitString = c;
    q.prototype.toString = e;
    q.prototype.toBitString = c;
    a.BitSetFunctor = function(c) {
      var e = 1 === l(c) >> a.ADDRESS_BITS_PER_WORD ? s : q;
      return function() {
        return new e(c);
      };
    };
  })(k.BitSets || (k.BitSets = {}));
  a = function() {
    function a() {
    }
    a.randomStyle = function() {
      a._randomStyleCache || (a._randomStyleCache = "#ff5e3a #ff9500 #ffdb4c #87fc70 #52edc7 #1ad6fd #c644fc #ef4db6 #4a4a4a #dbddde #ff3b30 #ff9500 #ffcc00 #4cd964 #34aadc #007aff #5856d6 #ff2d55 #8e8e93 #c7c7cc #5ad427 #c86edf #d1eefc #e0f8d8 #fb2b69 #f7f7f7 #1d77ef #d6cec3 #55efcb #ff4981 #ffd3e0 #f7f7f7 #ff1300 #1f1f21 #bdbec2 #ff3a2d".split(" "));
      return a._randomStyleCache[a._nextStyle++ % a._randomStyleCache.length];
    };
    a.gradientColor = function(l) {
      return a._gradient[a._gradient.length * f.clamp(l, 0, 1) | 0];
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
  k.ColorStyle = a;
  a = function() {
    function a(l, c, e, q) {
      this.xMin = l | 0;
      this.yMin = c | 0;
      this.xMax = e | 0;
      this.yMax = q | 0;
    }
    a.FromUntyped = function(l) {
      return new a(l.xMin, l.yMin, l.xMax, l.yMax);
    };
    a.FromRectangle = function(l) {
      return new a(20 * l.x | 0, 20 * l.y | 0, 20 * (l.x + l.width) | 0, 20 * (l.y + l.height) | 0);
    };
    a.prototype.setElements = function(a, c, e, q) {
      this.xMin = a;
      this.yMin = c;
      this.xMax = e;
      this.yMax = q;
    };
    a.prototype.copyFrom = function(a) {
      this.setElements(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    a.prototype.contains = function(a, c) {
      return a < this.xMin !== a < this.xMax && c < this.yMin !== c < this.yMax;
    };
    a.prototype.unionInPlace = function(a) {
      a.isEmpty() || (this.extendByPoint(a.xMin, a.yMin), this.extendByPoint(a.xMax, a.yMax));
    };
    a.prototype.extendByPoint = function(a, c) {
      this.extendByX(a);
      this.extendByY(c);
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
  k.Bounds = a;
  a = function() {
    function a(l, c, e, q) {
      h.assert(p(l));
      h.assert(p(c));
      h.assert(p(e));
      h.assert(p(q));
      this._xMin = l | 0;
      this._yMin = c | 0;
      this._xMax = e | 0;
      this._yMax = q | 0;
    }
    a.FromUntyped = function(l) {
      return new a(l.xMin, l.yMin, l.xMax, l.yMax);
    };
    a.FromRectangle = function(l) {
      return new a(20 * l.x | 0, 20 * l.y | 0, 20 * (l.x + l.width) | 0, 20 * (l.y + l.height) | 0);
    };
    a.prototype.setElements = function(a, c, e, q) {
      this.xMin = a;
      this.yMin = c;
      this.xMax = e;
      this.yMax = q;
    };
    a.prototype.copyFrom = function(a) {
      this.setElements(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    a.prototype.contains = function(a, c) {
      return a < this.xMin !== a < this.xMax && c < this.yMin !== c < this.yMax;
    };
    a.prototype.unionInPlace = function(a) {
      a.isEmpty() || (this.extendByPoint(a.xMin, a.yMin), this.extendByPoint(a.xMax, a.yMax));
    };
    a.prototype.extendByPoint = function(a, c) {
      this.extendByX(a);
      this.extendByY(c);
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
      h.assert(p(a));
      this._xMin = a;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "yMin", {get:function() {
      return this._yMin;
    }, set:function(a) {
      h.assert(p(a));
      this._yMin = a | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "xMax", {get:function() {
      return this._xMax;
    }, set:function(a) {
      h.assert(p(a));
      this._xMax = a | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "width", {get:function() {
      return this._xMax - this._xMin;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(a.prototype, "yMax", {get:function() {
      return this._yMax;
    }, set:function(a) {
      h.assert(p(a));
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
  k.DebugBounds = a;
  a = function() {
    function a(d, c, e, q) {
      this.r = d;
      this.g = c;
      this.b = e;
      this.a = q;
    }
    a.FromARGB = function(d) {
      return new a((d >> 16 & 255) / 255, (d >> 8 & 255) / 255, (d >> 0 & 255) / 255, (d >> 24 & 255) / 255);
    };
    a.FromRGBA = function(d) {
      return a.FromARGB(r.RGBAToARGB(d));
    };
    a.prototype.toRGBA = function() {
      return 255 * this.r << 24 | 255 * this.g << 16 | 255 * this.b << 8 | 255 * this.a;
    };
    a.prototype.toCSSStyle = function() {
      return r.rgbaToCSSStyle(this.toRGBA());
    };
    a.prototype.set = function(a) {
      this.r = a.r;
      this.g = a.g;
      this.b = a.b;
      this.a = a.a;
    };
    a.randomColor = function() {
      var d = .4;
      void 0 === d && (d = 1);
      return new a(Math.random(), Math.random(), Math.random(), d);
    };
    a.parseColor = function(d) {
      a.colorCache || (a.colorCache = Object.create(null));
      if (a.colorCache[d]) {
        return a.colorCache[d];
      }
      var c = document.createElement("span");
      document.body.appendChild(c);
      c.style.backgroundColor = d;
      var e = getComputedStyle(c).backgroundColor;
      document.body.removeChild(c);
      (c = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(e)) || (c = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(e));
      e = new a(0, 0, 0, 0);
      e.r = parseFloat(c[1]) / 255;
      e.g = parseFloat(c[2]) / 255;
      e.b = parseFloat(c[3]) / 255;
      e.a = c[4] ? parseFloat(c[4]) / 255 : 1;
      return a.colorCache[d] = e;
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
  k.Color = a;
  var r;
  (function(a) {
    function d(c) {
      var a, e, l = c >> 24 & 255;
      e = (Math.imul(c >> 16 & 255, l) + 127) / 255 | 0;
      a = (Math.imul(c >> 8 & 255, l) + 127) / 255 | 0;
      c = (Math.imul(c >> 0 & 255, l) + 127) / 255 | 0;
      return l << 24 | e << 16 | a << 8 | c;
    }
    a.RGBAToARGB = function(c) {
      return c >> 8 & 16777215 | (c & 255) << 24;
    };
    a.ARGBToRGBA = function(c) {
      return c << 8 | c >> 24 & 255;
    };
    a.rgbaToCSSStyle = function(c) {
      return k.StringUtilities.concat9("rgba(", c >> 24 & 255, ",", c >> 16 & 255, ",", c >> 8 & 255, ",", (c & 255) / 255, ")");
    };
    a.cssStyleToRGBA = function(c) {
      if ("#" === c[0]) {
        if (7 === c.length) {
          return parseInt(c.substring(1), 16) << 8 | 255;
        }
      } else {
        if ("r" === c[0]) {
          return c = c.substring(5, c.length - 1).split(","), (parseInt(c[0]) & 255) << 24 | (parseInt(c[1]) & 255) << 16 | (parseInt(c[2]) & 255) << 8 | 255 * parseFloat(c[3]) & 255;
        }
      }
      return 4278190335;
    };
    a.hexToRGB = function(c) {
      return parseInt(c.slice(1), 16);
    };
    a.rgbToHex = function(c) {
      return "#" + ("000000" + (c >>> 0).toString(16)).slice(-6);
    };
    a.isValidHexColor = function(c) {
      return/^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(c);
    };
    a.clampByte = function(c) {
      return Math.max(0, Math.min(255, c));
    };
    a.unpremultiplyARGB = function(c) {
      var a, e, d = c >> 24 & 255;
      e = Math.imul(255, c >> 16 & 255) / d & 255;
      a = Math.imul(255, c >> 8 & 255) / d & 255;
      c = Math.imul(255, c >> 0 & 255) / d & 255;
      return d << 24 | e << 16 | a << 8 | c;
    };
    a.premultiplyARGB = d;
    var c;
    a.ensureUnpremultiplyTable = function() {
      if (!c) {
        c = new Uint8Array(65536);
        for (var a = 0;256 > a;a++) {
          for (var e = 0;256 > e;e++) {
            c[(e << 8) + a] = Math.imul(255, a) / e;
          }
        }
      }
    };
    a.tableLookupUnpremultiplyARGB = function(a) {
      a |= 0;
      var e = a >> 24 & 255;
      if (0 === e) {
        return 0;
      }
      if (255 === e) {
        return a;
      }
      var d, l, n = e << 8, f = c;
      l = f[n + (a >> 16 & 255)];
      d = f[n + (a >> 8 & 255)];
      a = f[n + (a >> 0 & 255)];
      return e << 24 | l << 16 | d << 8 | a;
    };
    a.blendPremultipliedBGRA = function(c, a) {
      var e, d;
      d = 256 - (a & 255);
      e = Math.imul(c & 16711935, d) >> 8;
      d = Math.imul(c >> 8 & 16711935, d) >> 8;
      return((a >> 8 & 16711935) + d & 16711935) << 8 | (a & 16711935) + e & 16711935;
    };
    var e = v.swap32;
    a.convertImage = function(a, s, y, n) {
      var f = y.length;
      if (a === s) {
        if (y !== n) {
          for (a = 0;a < f;a++) {
            n[a] = y[a];
          }
        }
      } else {
        if (1 === a && 3 === s) {
          for (k.ColorUtilities.ensureUnpremultiplyTable(), a = 0;a < f;a++) {
            var b = y[a];
            s = b & 255;
            if (0 === s) {
              n[a] = 0;
            } else {
              if (255 === s) {
                n[a] = 4278190080 | b >> 8 & 16777215;
              } else {
                var t = b >> 24 & 255, m = b >> 16 & 255, b = b >> 8 & 255, v = s << 8, r = c, b = r[v + b], m = r[v + m], t = r[v + t];
                n[a] = s << 24 | t << 16 | m << 8 | b;
              }
            }
          }
        } else {
          if (2 === a && 3 === s) {
            for (a = 0;a < f;a++) {
              n[a] = e(y[a]);
            }
          } else {
            if (3 === a && 1 === s) {
              for (a = 0;a < f;a++) {
                s = y[a], n[a] = e(d(s & 4278255360 | s >> 16 & 255 | (s & 255) << 16));
              }
            } else {
              for (h.somewhatImplemented("Image Format Conversion: " + w[a] + " -> " + w[s]), a = 0;a < f;a++) {
                n[a] = y[a];
              }
            }
          }
        }
      }
    };
  })(r = k.ColorUtilities || (k.ColorUtilities = {}));
  a = function() {
    function a(d) {
      void 0 === d && (d = 32);
      this._list = [];
      this._maxSize = d;
    }
    a.prototype.acquire = function(d) {
      if (a._enabled) {
        for (var c = this._list, e = 0;e < c.length;e++) {
          var q = c[e];
          if (q.byteLength >= d) {
            return c.splice(e, 1), q;
          }
        }
      }
      return new ArrayBuffer(d);
    };
    a.prototype.release = function(d) {
      if (a._enabled) {
        var c = this._list;
        c.length === this._maxSize && c.shift();
        c.push(d);
      }
    };
    a.prototype.ensureUint8ArrayLength = function(a, c) {
      if (a.length >= c) {
        return a;
      }
      var e = Math.max(a.length + c, (3 * a.length >> 1) + 1), e = new Uint8Array(this.acquire(e), 0, e);
      e.set(a);
      this.release(a.buffer);
      return e;
    };
    a.prototype.ensureFloat64ArrayLength = function(a, c) {
      if (a.length >= c) {
        return a;
      }
      var e = Math.max(a.length + c, (3 * a.length >> 1) + 1), e = new Float64Array(this.acquire(e * Float64Array.BYTES_PER_ELEMENT), 0, e);
      e.set(a);
      this.release(a.buffer);
      return e;
    };
    a._enabled = !0;
    return a;
  }();
  k.ArrayBufferPool = a;
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
  })(k.Telemetry || (k.Telemetry = {}));
  (function(a) {
    a.instance;
  })(k.FileLoadingService || (k.FileLoadingService = {}));
  k.registerCSSFont = function(a, d, c) {
    if (inBrowser) {
      var e = document.head;
      e.insertBefore(document.createElement("style"), e.firstChild);
      e = document.styleSheets[0];
      d = "@font-face{font-family:swffont" + a + ";src:url(data:font/opentype;base64," + k.StringUtilities.base64ArrayBuffer(d) + ")}";
      e.insertRule(d, e.cssRules.length);
      c && (c = document.createElement("div"), c.style.fontFamily = "swffont" + a, c.innerHTML = "hello", document.body.appendChild(c), document.body.removeChild(c));
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
  })(k.ExternalInterfaceService || (k.ExternalInterfaceService = {}));
  a = function() {
    function a() {
    }
    a.prototype.setClipboard = function(a) {
    };
    a.instance = null;
    return a;
  }();
  k.ClipboardService = a;
  a = function() {
    function a() {
      this._queues = {};
    }
    a.prototype.register = function(a, c) {
      h.assert(a);
      h.assert(c);
      var e = this._queues[a];
      if (e) {
        if (-1 < e.indexOf(c)) {
          return;
        }
      } else {
        e = this._queues[a] = [];
      }
      e.push(c);
    };
    a.prototype.unregister = function(a, c) {
      h.assert(a);
      h.assert(c);
      var e = this._queues[a];
      if (e) {
        var q = e.indexOf(c);
        -1 !== q && e.splice(q, 1);
        0 === e.length && (this._queues[a] = null);
      }
    };
    a.prototype.notify = function(a, c) {
      var e = this._queues[a];
      if (e) {
        e = e.slice();
        c = Array.prototype.slice.call(arguments, 0);
        for (var q = 0;q < e.length;q++) {
          e[q].apply(null, c);
        }
      }
    };
    a.prototype.notify1 = function(a, c) {
      var e = this._queues[a];
      if (e) {
        for (var e = e.slice(), q = 0;q < e.length;q++) {
          (0,e[q])(a, c);
        }
      }
    };
    return a;
  }();
  k.Callback = a;
  (function(a) {
    a[a.None = 0] = "None";
    a[a.PremultipliedAlphaARGB = 1] = "PremultipliedAlphaARGB";
    a[a.StraightAlphaARGB = 2] = "StraightAlphaARGB";
    a[a.StraightAlphaRGBA = 3] = "StraightAlphaRGBA";
    a[a.JPEG = 4] = "JPEG";
    a[a.PNG = 5] = "PNG";
    a[a.GIF = 6] = "GIF";
  })(k.ImageType || (k.ImageType = {}));
  var w = k.ImageType;
  k.getMIMETypeForImageType = function(a) {
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
  })(k.UI || (k.UI = {}));
  a = function() {
    function a() {
      this.promise = new Promise(function(a, c) {
        this.resolve = a;
        this.reject = c;
      }.bind(this));
    }
    a.prototype.then = function(a, c) {
      return this.promise.then(a, c);
    };
    return a;
  }();
  k.PromiseWrapper = a;
})(Shumway || (Shumway = {}));
(function() {
  function k(a) {
    if ("function" !== typeof a) {
      throw new TypeError("Invalid deferred constructor");
    }
    var c = r();
    a = new a(c);
    var e = c.resolve;
    if ("function" !== typeof e) {
      throw new TypeError("Invalid resolve construction function");
    }
    c = c.reject;
    if ("function" !== typeof c) {
      throw new TypeError("Invalid reject construction function");
    }
    return{promise:a, resolve:e, reject:c};
  }
  function p(a, c) {
    if ("object" !== typeof a || null === a) {
      return!1;
    }
    try {
      var e = a.then;
      if ("function" !== typeof e) {
        return!1;
      }
      e.call(a, c.resolve, c.reject);
    } catch (d) {
      e = c.reject, e(d);
    }
    return!0;
  }
  function g(a) {
    return "object" === typeof a && null !== a && "undefined" !== typeof a.promiseStatus;
  }
  function b(a, c) {
    if ("unresolved" === a.promiseStatus) {
      var e = a.rejectReactions;
      a.result = c;
      a.resolveReactions = void 0;
      a.rejectReactions = void 0;
      a.promiseStatus = "has-rejection";
      u(e, c);
    }
  }
  function u(a, c) {
    for (var e = 0;e < a.length;e++) {
      h({reaction:a[e], argument:c});
    }
  }
  function h(a) {
    0 === e.length && setTimeout(d, 0);
    e.push(a);
  }
  function a(a, c) {
    var e = a.deferred, d = a.handler, l, n;
    try {
      l = d(c);
    } catch (f) {
      return e = e.reject, e(f);
    }
    if (l === e.promise) {
      return e = e.reject, e(new TypeError("Self resolution"));
    }
    try {
      if (n = p(l, e), !n) {
        var b = e.resolve;
        return b(l);
      }
    } catch (h) {
      return e = e.reject, e(h);
    }
  }
  function d() {
    for (;0 < e.length;) {
      var c = e[0];
      try {
        a(c.reaction, c.argument);
      } catch (d) {
        if ("function" === typeof l.onerror) {
          l.onerror(d);
        }
      }
      e.shift();
    }
  }
  function n(a) {
    throw a;
  }
  function f(a) {
    return a;
  }
  function v(a) {
    return function(c) {
      b(a, c);
    };
  }
  function m(a) {
    return function(c) {
      if ("unresolved" === a.promiseStatus) {
        var e = a.resolveReactions;
        a.result = c;
        a.resolveReactions = void 0;
        a.rejectReactions = void 0;
        a.promiseStatus = "has-resolution";
        u(e, c);
      }
    };
  }
  function r() {
    function a(c, e) {
      a.resolve = c;
      a.reject = e;
    }
    return a;
  }
  function w(a, c, e) {
    return function(d) {
      if (d === a) {
        return e(new TypeError("Self resolution"));
      }
      var l = a.promiseConstructor;
      if (g(d) && d.promiseConstructor === l) {
        return d.then(c, e);
      }
      l = k(l);
      return p(d, l) ? l.promise.then(c, e) : c(d);
    };
  }
  function t(a, c, e, d) {
    return function(l) {
      c[a] = l;
      d.countdown--;
      0 === d.countdown && e.resolve(c);
    };
  }
  function l(a) {
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
    var c = m(this), e = v(this);
    try {
      a(c, e);
    } catch (d) {
      b(this, d);
    }
    this.promiseConstructor = l;
    return this;
  }
  var c = Function("return this")();
  if (c.Promise) {
    "function" !== typeof c.Promise.all && (c.Promise.all = function(a) {
      var e = 0, d = [], l, n, f = new c.Promise(function(a, c) {
        l = a;
        n = c;
      });
      a.forEach(function(a, c) {
        e++;
        a.then(function(a) {
          d[c] = a;
          e--;
          0 === e && l(d);
        }, n);
      });
      0 === e && l(d);
      return f;
    }), "function" !== typeof c.Promise.resolve && (c.Promise.resolve = function(a) {
      return new c.Promise(function(c) {
        c(a);
      });
    });
  } else {
    var e = [];
    l.all = function(a) {
      var c = k(this), e = [], d = {countdown:0}, l = 0;
      a.forEach(function(a) {
        this.cast(a).then(t(l, e, c, d), c.reject);
        l++;
        d.countdown++;
      }, this);
      0 === l && c.resolve(e);
      return c.promise;
    };
    l.cast = function(a) {
      if (g(a)) {
        return a;
      }
      var c = k(this);
      c.resolve(a);
      return c.promise;
    };
    l.reject = function(a) {
      var c = k(this);
      c.reject(a);
      return c.promise;
    };
    l.resolve = function(a) {
      var c = k(this);
      c.resolve(a);
      return c.promise;
    };
    l.prototype = {"catch":function(a) {
      this.then(void 0, a);
    }, then:function(a, c) {
      if (!g(this)) {
        throw new TypeError("this is not a Promises");
      }
      var e = k(this.promiseConstructor), d = "function" === typeof c ? c : n, l = {deferred:e, handler:w(this, "function" === typeof a ? a : f, d)}, d = {deferred:e, handler:d};
      switch(this.promiseStatus) {
        case "unresolved":
          this.resolveReactions.push(l);
          this.rejectReactions.push(d);
          break;
        case "has-resolution":
          h({reaction:l, argument:this.result});
          break;
        case "has-rejection":
          h({reaction:d, argument:this.result});
      }
      return e.promise;
    }};
    c.Promise = l;
  }
})();
"undefined" !== typeof exports && (exports.Shumway = Shumway);
(function() {
  function k(k, g, b) {
    k[g] || Object.defineProperty(k, g, {value:b, writable:!0, configurable:!0, enumerable:!1});
  }
  k(String.prototype, "padRight", function(k, g) {
    var b = this, u = b.replace(/\033\[[0-9]*m/g, "").length;
    if (!k || u >= g) {
      return b;
    }
    for (var u = (g - u) / k.length, h = 0;h < u;h++) {
      b += k;
    }
    return b;
  });
  k(String.prototype, "padLeft", function(k, g) {
    var b = this, u = b.length;
    if (!k || u >= g) {
      return b;
    }
    for (var u = (g - u) / k.length, h = 0;h < u;h++) {
      b = k + b;
    }
    return b;
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
    for (var b = 0, u = 0;u < this.length;u++) {
      this[u] === k && (this[u] = g, b++);
    }
    return b;
  });
})();
(function(k) {
  (function(p) {
    var g = k.isObject, b = function() {
      function a(a, n, f, b) {
        this.shortName = a;
        this.longName = n;
        this.type = f;
        b = b || {};
        this.positional = b.positional;
        this.parseFn = b.parse;
        this.value = b.defaultValue;
      }
      a.prototype.parse = function(a) {
        this.value = "boolean" === this.type ? a : "number" === this.type ? parseInt(a, 10) : a;
        this.parseFn && this.parseFn(this.value);
      };
      return a;
    }();
    p.Argument = b;
    var u = function() {
      function a() {
        this.args = [];
      }
      a.prototype.addArgument = function(a, n, f, h) {
        a = new b(a, n, f, h);
        this.args.push(a);
        return a;
      };
      a.prototype.addBoundOption = function(a) {
        this.args.push(new b(a.shortName, a.longName, a.type, {parse:function(n) {
          a.value = n;
        }}));
      };
      a.prototype.addBoundOptionSet = function(a) {
        var n = this;
        a.options.forEach(function(a) {
          a instanceof h ? n.addBoundOptionSet(a) : n.addBoundOption(a);
        });
      };
      a.prototype.getUsage = function() {
        var a = "";
        this.args.forEach(function(n) {
          a = n.positional ? a + n.longName : a + ("[-" + n.shortName + "|--" + n.longName + ("boolean" === n.type ? "" : " " + n.type[0].toUpperCase()) + "]");
          a += " ";
        });
        return a;
      };
      a.prototype.parse = function(a) {
        var n = {}, b = [];
        this.args.forEach(function(a) {
          a.positional ? b.push(a) : (n["-" + a.shortName] = a, n["--" + a.longName] = a);
        });
        for (var h = [];a.length;) {
          var m = a.shift(), r = null, g = m;
          if ("--" == m) {
            h = h.concat(a);
            break;
          } else {
            if ("-" == m.slice(0, 1) || "--" == m.slice(0, 2)) {
              r = n[m];
              if (!r) {
                continue;
              }
              g = "boolean" !== r.type ? a.shift() : !0;
            } else {
              b.length ? r = b.shift() : h.push(g);
            }
          }
          r && r.parse(g);
        }
        return h;
      };
      return a;
    }();
    p.ArgumentParser = u;
    var h = function() {
      function a(a, n) {
        void 0 === n && (n = null);
        this.open = !1;
        this.name = a;
        this.settings = n || {};
        this.options = [];
      }
      a.prototype.register = function(d) {
        if (d instanceof a) {
          for (var n = 0;n < this.options.length;n++) {
            var b = this.options[n];
            if (b instanceof a && b.name === d.name) {
              return b;
            }
          }
        }
        this.options.push(d);
        if (this.settings) {
          if (d instanceof a) {
            n = this.settings[d.name], g(n) && (d.settings = n.settings, d.open = n.open);
          } else {
            if ("undefined" !== typeof this.settings[d.longName]) {
              switch(d.type) {
                case "boolean":
                  d.value = !!this.settings[d.longName];
                  break;
                case "number":
                  d.value = +this.settings[d.longName];
                  break;
                default:
                  d.value = this.settings[d.longName];
              }
            }
          }
        }
        return d;
      };
      a.prototype.trace = function(a) {
        a.enter(this.name + " {");
        this.options.forEach(function(n) {
          n.trace(a);
        });
        a.leave("}");
      };
      a.prototype.getSettings = function() {
        var d = {};
        this.options.forEach(function(n) {
          n instanceof a ? d[n.name] = {settings:n.getSettings(), open:n.open} : d[n.longName] = n.value;
        });
        return d;
      };
      a.prototype.setSettings = function(d) {
        d && this.options.forEach(function(n) {
          n instanceof a ? n.name in d && n.setSettings(d[n.name].settings) : n.longName in d && (n.value = d[n.longName]);
        });
      };
      return a;
    }();
    p.OptionSet = h;
    u = function() {
      function a(a, n, b, h, m, r) {
        void 0 === r && (r = null);
        this.longName = n;
        this.shortName = a;
        this.type = b;
        this.value = this.defaultValue = h;
        this.description = m;
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
    p.Option = u;
  })(k.Options || (k.Options = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    function g() {
      try {
        return "undefined" !== typeof window && "localStorage" in window && null !== window.localStorage;
      } catch (b) {
        return!1;
      }
    }
    function b(b) {
      void 0 === b && (b = p.ROOT);
      var h = {};
      if (g() && (b = window.localStorage[b])) {
        try {
          h = JSON.parse(b);
        } catch (a) {
        }
      }
      return h;
    }
    p.ROOT = "Shumway Options";
    p.shumwayOptions = new k.Options.OptionSet(p.ROOT, b());
    p.isStorageSupported = g;
    p.load = b;
    p.save = function(b, h) {
      void 0 === b && (b = null);
      void 0 === h && (h = p.ROOT);
      if (g()) {
        try {
          window.localStorage[h] = JSON.stringify(b ? b : p.shumwayOptions.getSettings());
        } catch (a) {
        }
      }
    };
    p.setSettings = function(b) {
      p.shumwayOptions.setSettings(b);
    };
    p.getSettings = function(b) {
      return p.shumwayOptions.getSettings();
    };
  })(k.Settings || (k.Settings = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var g = function() {
      function b(b, h) {
        this._parent = b;
        this._timers = k.ObjectUtilities.createMap();
        this._name = h;
        this._count = this._total = this._last = this._begin = 0;
      }
      b.time = function(g, h) {
        b.start(g);
        h();
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
        return{name:this._name, total:this._total, timers:this._timers};
      };
      b.prototype.trace = function(b) {
        b.enter(this._name + ": " + this._total.toFixed(2) + " ms, count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var h in this._timers) {
          this._timers[h].trace(b);
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
    p.Timer = g;
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
        return{counts:this._counts, times:this._times};
      };
      b.prototype.count = function(b, h, a) {
        void 0 === h && (h = 1);
        void 0 === a && (a = 0);
        if (this._enabled) {
          return void 0 === this._counts[b] && (this._counts[b] = 0, this._times[b] = 0), this._counts[b] += h, this._times[b] += a, this._counts[b];
        }
      };
      b.prototype.trace = function(b) {
        for (var h in this._counts) {
          b.writeLn(h + ": " + this._counts[h]);
        }
      };
      b.prototype._pairToString = function(b, h) {
        var a = h[0], d = h[1], n = b[a], a = a + ": " + d;
        n && (a += ", " + n.toFixed(4), 1 < d && (a += " (" + (n / d).toFixed(4) + ")"));
        return a;
      };
      b.prototype.toStringSorted = function() {
        var b = this, h = this._times, a = [], d;
        for (d in this._counts) {
          a.push([d, this._counts[d]]);
        }
        a.sort(function(a, d) {
          return d[1] - a[1];
        });
        return a.map(function(a) {
          return b._pairToString(h, a);
        }).join(", ");
      };
      b.prototype.traceSorted = function(b, h) {
        void 0 === h && (h = !1);
        var a = this, d = this._times, n = [], f;
        for (f in this._counts) {
          n.push([f, this._counts[f]]);
        }
        n.sort(function(a, d) {
          return d[1] - a[1];
        });
        h ? b.writeLn(n.map(function(n) {
          return a._pairToString(d, n);
        }).join(", ")) : n.forEach(function(n) {
          b.writeLn(a._pairToString(d, n));
        });
      };
      b.instance = new b(!0);
      return b;
    }();
    p.Counter = g;
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
        for (var b = 0, h = 0;h < this._count;h++) {
          b += this._samples[h];
        }
        return b / this._count;
      };
      return b;
    }();
    p.Average = g;
  })(k.Metrics || (k.Metrics = {}));
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function(k, p) {
  function g() {
    this.constructor = k;
  }
  for (var b in p) {
    p.hasOwnProperty(b) && (k[b] = p[b]);
  }
  g.prototype = p.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(k) {
    function g(a) {
      for (var e = Math.max.apply(null, a), q = a.length, d = 1 << e, l = new Uint32Array(d), n = e << 16 | 65535, b = 0;b < d;b++) {
        l[b] = n;
      }
      for (var n = 0, b = 1, f = 2;b <= e;n <<= 1, ++b, f <<= 1) {
        for (var h = 0;h < q;++h) {
          if (a[h] === b) {
            for (var m = 0, t = 0;t < b;++t) {
              m = 2 * m + (n >> t & 1);
            }
            for (t = m;t < d;t += f) {
              l[t] = b << 16 | h;
            }
            ++n;
          }
        }
      }
      return{codes:l, maxBits:e};
    }
    var b;
    (function(a) {
      a[a.INIT = 0] = "INIT";
      a[a.BLOCK_0 = 1] = "BLOCK_0";
      a[a.BLOCK_1 = 2] = "BLOCK_1";
      a[a.BLOCK_2_PRE = 3] = "BLOCK_2_PRE";
      a[a.BLOCK_2 = 4] = "BLOCK_2";
      a[a.DONE = 5] = "DONE";
      a[a.ERROR = 6] = "ERROR";
      a[a.VERIFY_HEADER = 7] = "VERIFY_HEADER";
    })(b || (b = {}));
    b = function() {
      function a(c) {
      }
      a.prototype.push = function(a) {
      };
      a.prototype.close = function() {
      };
      a.create = function(a) {
        return "undefined" !== typeof SpecialInflate ? new w(a) : new u(a);
      };
      a.prototype._processZLibHeader = function(a, c, d) {
        if (c + 2 > d) {
          return 0;
        }
        a = a[c] << 8 | a[c + 1];
        c = null;
        2048 !== (a & 3840) ? c = "inflate: unknown compression method" : 0 !== a % 31 ? c = "inflate: bad FCHECK" : 0 !== (a & 32) && (c = "inflate: FDICT bit set");
        if (c) {
          if (this.onError) {
            this.onError(c);
          }
          return-1;
        }
        return 2;
      };
      a.inflate = function(e, q, d) {
        var l = new Uint8Array(q), n = 0;
        q = a.create(d);
        q.onData = function(a) {
          l.set(a, n);
          n += a.length;
        };
        q.push(e);
        q.close();
        return l;
      };
      return a;
    }();
    k.Inflate = b;
    var u = function(c) {
      function e(e) {
        c.call(this, e);
        this._buffer = null;
        this._bitLength = this._bitBuffer = this._bufferPosition = this._bufferSize = 0;
        this._window = new Uint8Array(65794);
        this._windowPosition = 0;
        this._state = e ? 7 : 0;
        this._isFinalBlock = !1;
        this._distanceTable = this._literalTable = null;
        this._block0Read = 0;
        this._block2State = null;
        this._copyState = {state:0, len:0, lenBits:0, dist:0, distBits:0};
        if (!r) {
          h = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);
          a = new Uint16Array(30);
          d = new Uint8Array(30);
          for (var s = e = 0, l = 1;30 > e;++e) {
            a[e] = l, l += 1 << (d[e] = ~~((s += 2 < e ? 1 : 0) / 2));
          }
          var b = new Uint8Array(288);
          for (e = 0;32 > e;++e) {
            b[e] = 5;
          }
          n = g(b.subarray(0, 32));
          f = new Uint16Array(29);
          v = new Uint8Array(29);
          s = e = 0;
          for (l = 3;29 > e;++e) {
            f[e] = l - (28 == e ? 1 : 0), l += 1 << (v[e] = ~~((s += 4 < e ? 1 : 0) / 4 % 6));
          }
          for (e = 0;288 > e;++e) {
            b[e] = 144 > e || 279 < e ? 8 : 256 > e ? 9 : 7;
          }
          m = g(b);
          r = !0;
        }
      }
      __extends(e, c);
      e.prototype.push = function(a) {
        if (!this._buffer || this._buffer.length < this._bufferSize + a.length) {
          var c = new Uint8Array(this._bufferSize + a.length);
          this._buffer && c.set(this._buffer);
          this._buffer = c;
        }
        this._buffer.set(a, this._bufferSize);
        this._bufferSize += a.length;
        this._bufferPosition = 0;
        a = !1;
        do {
          c = this._windowPosition;
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
              var e = this._processZLibHeader(this._buffer, this._bufferPosition, this._bufferSize);
              0 < e ? (this._bufferPosition += e, this._state = 0) : 0 === e ? a = !0 : this._state = 6;
          }
          if (0 < this._windowPosition - c) {
            this.onData(this._window.subarray(c, this._windowPosition));
          }
          65536 <= this._windowPosition && ("copyWithin" in this._buffer ? this._window.copyWithin(0, this._windowPosition - 32768, this._windowPosition) : this._window.set(this._window.subarray(this._windowPosition - 32768, this._windowPosition)), this._windowPosition = 32768);
        } while (!a && this._bufferPosition < this._bufferSize);
        this._bufferPosition < this._bufferSize ? ("copyWithin" in this._buffer ? this._buffer.copyWithin(0, this._bufferPosition, this._bufferSize) : this._buffer.set(this._buffer.subarray(this._bufferPosition, this._bufferSize)), this._bufferSize -= this._bufferPosition) : this._bufferSize = 0;
      };
      e.prototype._decodeInitState = function() {
        if (this._isFinalBlock) {
          return this._state = 5, !1;
        }
        var a = this._buffer, c = this._bufferSize, e = this._bitBuffer, d = this._bitLength, l = this._bufferPosition;
        if (3 > (c - l << 3) + d) {
          return!0;
        }
        3 > d && (e |= a[l++] << d, d += 8);
        var b = e & 7, e = e >> 3, d = d - 3;
        switch(b >> 1) {
          case 0:
            d = e = 0;
            if (4 > c - l) {
              return!0;
            }
            var f = a[l] | a[l + 1] << 8, a = a[l + 2] | a[l + 3] << 8, l = l + 4;
            if (65535 !== (f ^ a)) {
              this._error("inflate: invalid block 0 length");
              a = 6;
              break;
            }
            0 === f ? a = 0 : (this._block0Read = f, a = 1);
            break;
          case 1:
            a = 2;
            this._literalTable = m;
            this._distanceTable = n;
            break;
          case 2:
            if (26 > (c - l << 3) + d) {
              return!0;
            }
            for (;14 > d;) {
              e |= a[l++] << d, d += 8;
            }
            f = (e >> 10 & 15) + 4;
            if ((c - l << 3) + d < 14 + 3 * f) {
              return!0;
            }
            for (var c = {numLiteralCodes:(e & 31) + 257, numDistanceCodes:(e >> 5 & 31) + 1, codeLengthTable:void 0, bitLengths:void 0, codesRead:0, dupBits:0}, e = e >> 14, d = d - 14, t = new Uint8Array(19), v = 0;v < f;++v) {
              3 > d && (e |= a[l++] << d, d += 8), t[h[v]] = e & 7, e >>= 3, d -= 3;
            }
            for (;19 > v;v++) {
              t[h[v]] = 0;
            }
            c.bitLengths = new Uint8Array(c.numLiteralCodes + c.numDistanceCodes);
            c.codeLengthTable = g(t);
            this._block2State = c;
            a = 3;
            break;
          default:
            return this._error("inflate: unsupported block type"), !1;
        }
        this._isFinalBlock = !!(b & 1);
        this._state = a;
        this._bufferPosition = l;
        this._bitBuffer = e;
        this._bitLength = d;
        return!1;
      };
      e.prototype._error = function(a) {
        if (this.onError) {
          this.onError(a);
        }
      };
      e.prototype._decodeBlock0 = function() {
        var a = this._bufferPosition, c = this._windowPosition, e = this._block0Read, d = 65794 - c, l = this._bufferSize - a, n = l < e, b = Math.min(d, l, e);
        this._window.set(this._buffer.subarray(a, a + b), c);
        this._windowPosition = c + b;
        this._bufferPosition = a + b;
        this._block0Read = e - b;
        return e === b ? (this._state = 0, !1) : n && d < l;
      };
      e.prototype._readBits = function(a) {
        var c = this._bitBuffer, e = this._bitLength;
        if (a > e) {
          var d = this._bufferPosition, l = this._bufferSize;
          do {
            if (d >= l) {
              return this._bufferPosition = d, this._bitBuffer = c, this._bitLength = e, -1;
            }
            c |= this._buffer[d++] << e;
            e += 8;
          } while (a > e);
          this._bufferPosition = d;
        }
        this._bitBuffer = c >> a;
        this._bitLength = e - a;
        return c & (1 << a) - 1;
      };
      e.prototype._readCode = function(a) {
        var c = this._bitBuffer, e = this._bitLength, d = a.maxBits;
        if (d > e) {
          var l = this._bufferPosition, n = this._bufferSize;
          do {
            if (l >= n) {
              return this._bufferPosition = l, this._bitBuffer = c, this._bitLength = e, -1;
            }
            c |= this._buffer[l++] << e;
            e += 8;
          } while (d > e);
          this._bufferPosition = l;
        }
        a = a.codes[c & (1 << d) - 1];
        d = a >> 16;
        if (a & 32768) {
          return this._error("inflate: invalid encoding"), this._state = 6, -1;
        }
        this._bitBuffer = c >> d;
        this._bitLength = e - d;
        return a & 65535;
      };
      e.prototype._decodeBlock2Pre = function() {
        var a = this._block2State, c = a.numLiteralCodes + a.numDistanceCodes, e = a.bitLengths, d = a.codesRead, l = 0 < d ? e[d - 1] : 0, n = a.codeLengthTable, b;
        if (0 < a.dupBits) {
          b = this._readBits(a.dupBits);
          if (0 > b) {
            return!0;
          }
          for (;b--;) {
            e[d++] = l;
          }
          a.dupBits = 0;
        }
        for (;d < c;) {
          var f = this._readCode(n);
          if (0 > f) {
            return a.codesRead = d, !0;
          }
          if (16 > f) {
            e[d++] = l = f;
          } else {
            var h;
            switch(f) {
              case 16:
                h = 2;
                b = 3;
                f = l;
                break;
              case 17:
                b = h = 3;
                f = 0;
                break;
              case 18:
                h = 7, b = 11, f = 0;
            }
            for (;b--;) {
              e[d++] = f;
            }
            b = this._readBits(h);
            if (0 > b) {
              return a.codesRead = d, a.dupBits = h, !0;
            }
            for (;b--;) {
              e[d++] = f;
            }
            l = f;
          }
        }
        this._literalTable = g(e.subarray(0, a.numLiteralCodes));
        this._distanceTable = g(e.subarray(a.numLiteralCodes));
        this._state = 4;
        this._block2State = null;
        return!1;
      };
      e.prototype._decodeBlock = function() {
        var c = this._literalTable, e = this._distanceTable, l = this._window, n = this._windowPosition, b = this._copyState, h, m, t, r;
        if (0 !== b.state) {
          switch(b.state) {
            case 1:
              if (0 > (h = this._readBits(b.lenBits))) {
                return!0;
              }
              b.len += h;
              b.state = 2;
            case 2:
              if (0 > (h = this._readCode(e))) {
                return!0;
              }
              b.distBits = d[h];
              b.dist = a[h];
              b.state = 3;
            case 3:
              h = 0;
              if (0 < b.distBits && 0 > (h = this._readBits(b.distBits))) {
                return!0;
              }
              r = b.dist + h;
              m = b.len;
              for (h = n - r;m--;) {
                l[n++] = l[h++];
              }
              b.state = 0;
              if (65536 <= n) {
                return this._windowPosition = n, !1;
              }
              break;
          }
        }
        do {
          h = this._readCode(c);
          if (0 > h) {
            return this._windowPosition = n, !0;
          }
          if (256 > h) {
            l[n++] = h;
          } else {
            if (256 < h) {
              this._windowPosition = n;
              h -= 257;
              t = v[h];
              m = f[h];
              h = 0 === t ? 0 : this._readBits(t);
              if (0 > h) {
                return b.state = 1, b.len = m, b.lenBits = t, !0;
              }
              m += h;
              h = this._readCode(e);
              if (0 > h) {
                return b.state = 2, b.len = m, !0;
              }
              t = d[h];
              r = a[h];
              h = 0 === t ? 0 : this._readBits(t);
              if (0 > h) {
                return b.state = 3, b.len = m, b.dist = r, b.distBits = t, !0;
              }
              r += h;
              for (h = n - r;m--;) {
                l[n++] = l[h++];
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
      return e;
    }(b), h, a, d, n, f, v, m, r = !1, w = function(a) {
      function e(e) {
        a.call(this, e);
        this._verifyHeader = e;
        this._specialInflate = new SpecialInflate;
        this._specialInflate.onData = function(a) {
          this.onData(a);
        }.bind(this);
      }
      __extends(e, a);
      e.prototype.push = function(a) {
        if (this._verifyHeader) {
          var c;
          this._buffer ? (c = new Uint8Array(this._buffer.length + a.length), c.set(this._buffer), c.set(a, this._buffer.length), this._buffer = null) : c = new Uint8Array(a);
          var e = this._processZLibHeader(c, 0, c.length);
          if (0 === e) {
            this._buffer = c;
            return;
          }
          this._verifyHeader = !0;
          0 < e && (a = c.subarray(e));
        }
        this._specialInflate.push(a);
      };
      e.prototype.close = function() {
        this._specialInflate && (this._specialInflate.close(), this._specialInflate = null);
      };
      return e;
    }(b), t;
    (function(a) {
      a[a.WRITE = 0] = "WRITE";
      a[a.DONE = 1] = "DONE";
      a[a.ZLIB_HEADER = 2] = "ZLIB_HEADER";
    })(t || (t = {}));
    var l = function() {
      function a() {
        this.a = 1;
        this.b = 0;
      }
      a.prototype.update = function(a, c, d) {
        for (var l = this.a, n = this.b;c < d;++c) {
          l = (l + (a[c] & 255)) % 65521, n = (n + l) % 65521;
        }
        this.a = l;
        this.b = n;
      };
      a.prototype.getChecksum = function() {
        return this.b << 16 | this.a;
      };
      return a;
    }();
    k.Adler32 = l;
    t = function() {
      function a(c) {
        this._state = (this._writeZlibHeader = c) ? 2 : 0;
        this._adler32 = c ? new l : null;
      }
      a.prototype.push = function(a) {
        2 === this._state && (this.onData(new Uint8Array([120, 156])), this._state = 0);
        for (var c = a.length, d = new Uint8Array(c + 5 * Math.ceil(c / 65535)), l = 0, n = 0;65535 < c;) {
          d.set(new Uint8Array([0, 255, 255, 0, 0]), l), l += 5, d.set(a.subarray(n, n + 65535), l), n += 65535, l += 65535, c -= 65535;
        }
        d.set(new Uint8Array([0, c & 255, c >> 8 & 255, ~c & 255, ~c >> 8 & 255]), l);
        d.set(a.subarray(n, c), l + 5);
        this.onData(d);
        this._adler32 && this._adler32.update(a, 0, c);
      };
      a.prototype.close = function() {
        this._state = 1;
        this.onData(new Uint8Array([1, 0, 0, 255, 255]));
        if (this._adler32) {
          var a = this._adler32.getChecksum();
          this.onData(new Uint8Array([a & 255, a >> 8 & 255, a >> 16 & 255, a >>> 24 & 255]));
        }
      };
      return a;
    }();
    k.Deflate = t;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    function g(a) {
      for (var c = new Uint16Array(a), d = 0;d < a;d++) {
        c[d] = 1024;
      }
      return c;
    }
    function b(a, c, d, l) {
      for (var n = 1, b = 0, h = 0;h < d;h++) {
        var f = l.decodeBit(a, n + c), n = (n << 1) + f, b = b | f << h
      }
      return b;
    }
    function u(a, c) {
      var d = [];
      d.length = c;
      for (var l = 0;l < c;l++) {
        d[l] = new f(a);
      }
      return d;
    }
    var h = function() {
      function a() {
        this.pos = this.available = 0;
        this.buffer = new Uint8Array(2E3);
      }
      a.prototype.append = function(a) {
        var c = this.pos + this.available, e = c + a.length;
        if (e > this.buffer.length) {
          for (var d = 2 * this.buffer.length;d < e;) {
            d *= 2;
          }
          e = new Uint8Array(d);
          e.set(this.buffer);
          this.buffer = e;
        }
        this.buffer.set(a, c);
        this.available += a.length;
      };
      a.prototype.compact = function() {
        0 !== this.available && (this.buffer.set(this.buffer.subarray(this.pos, this.pos + this.available), 0), this.pos = 0);
      };
      a.prototype.readByte = function() {
        if (0 >= this.available) {
          throw Error("Unexpected end of file");
        }
        this.available--;
        return this.buffer[this.pos++];
      };
      return a;
    }(), a = function() {
      function a(c) {
        this.onData = c;
        this.processed = 0;
      }
      a.prototype.writeBytes = function(a) {
        this.onData.call(null, a);
        this.processed += a.length;
      };
      return a;
    }(), d = function() {
      function a(c) {
        this.outStream = c;
        this.buf = null;
        this.size = this.pos = 0;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      }
      a.prototype.create = function(a) {
        this.buf = new Uint8Array(a);
        this.pos = 0;
        this.size = a;
        this.isFull = !1;
        this.totalPos = this.writePos = 0;
      };
      a.prototype.putByte = function(a) {
        this.totalPos++;
        this.buf[this.pos++] = a;
        this.pos === this.size && (this.flush(), this.pos = 0, this.isFull = !0);
      };
      a.prototype.getByte = function(a) {
        return this.buf[a <= this.pos ? this.pos - a : this.size - a + this.pos];
      };
      a.prototype.flush = function() {
        this.writePos < this.pos && (this.outStream.writeBytes(this.buf.subarray(this.writePos, this.pos)), this.writePos = this.pos === this.size ? 0 : this.pos);
      };
      a.prototype.copyMatch = function(a, c) {
        for (var e = this.pos, d = this.size, l = this.buf, n = a <= e ? e - a : d - a + e, b = c;0 < b;) {
          for (var h = Math.min(Math.min(b, d - e), d - n), f = 0;f < h;f++) {
            var m = l[n++];
            l[e++] = m;
          }
          e === d && (this.pos = e, this.flush(), e = 0, this.isFull = !0);
          n === d && (n = 0);
          b -= h;
        }
        this.pos = e;
        this.totalPos += c;
      };
      a.prototype.checkDistance = function(a) {
        return a <= this.pos || this.isFull;
      };
      a.prototype.isEmpty = function() {
        return 0 === this.pos && !this.isFull;
      };
      return a;
    }(), n = function() {
      function a(c) {
        this.inStream = c;
        this.code = this.range = 0;
        this.corrupted = !1;
      }
      a.prototype.init = function() {
        0 !== this.inStream.readByte() && (this.corrupted = !0);
        this.range = -1;
        for (var a = 0, c = 0;4 > c;c++) {
          a = a << 8 | this.inStream.readByte();
        }
        a === this.range && (this.corrupted = !0);
        this.code = a;
      };
      a.prototype.isFinishedOK = function() {
        return 0 === this.code;
      };
      a.prototype.decodeDirectBits = function(a) {
        var c = 0, e = this.range, d = this.code;
        do {
          var e = e >>> 1 | 0, d = d - e | 0, l = d >> 31, d = d + (e & l) | 0;
          d === e && (this.corrupted = !0);
          0 <= e && 16777216 > e && (e <<= 8, d = d << 8 | this.inStream.readByte());
          c = (c << 1) + l + 1 | 0;
        } while (--a);
        this.range = e;
        this.code = d;
        return c;
      };
      a.prototype.decodeBit = function(a, c) {
        var e = this.range, d = this.code, l = a[c], n = (e >>> 11) * l;
        d >>> 0 < n ? (l = l + (2048 - l >> 5) | 0, e = n | 0, n = 0) : (l = l - (l >> 5) | 0, d = d - n | 0, e = e - n | 0, n = 1);
        a[c] = l & 65535;
        0 <= e && 16777216 > e && (e <<= 8, d = d << 8 | this.inStream.readByte());
        this.range = e;
        this.code = d;
        return n;
      };
      return a;
    }(), f = function() {
      function a(c) {
        this.numBits = c;
        this.probs = g(1 << c);
      }
      a.prototype.decode = function(a) {
        for (var c = 1, e = 0;e < this.numBits;e++) {
          c = (c << 1) + a.decodeBit(this.probs, c);
        }
        return c - (1 << this.numBits);
      };
      a.prototype.reverseDecode = function(a) {
        return b(this.probs, 0, this.numBits, a);
      };
      return a;
    }(), v = function() {
      function a() {
        this.choice = g(2);
        this.lowCoder = u(3, 16);
        this.midCoder = u(3, 16);
        this.highCoder = new f(8);
      }
      a.prototype.decode = function(a, c) {
        return 0 === a.decodeBit(this.choice, 0) ? this.lowCoder[c].decode(a) : 0 === a.decodeBit(this.choice, 1) ? 8 + this.midCoder[c].decode(a) : 16 + this.highCoder.decode(a);
      };
      return a;
    }(), m = function() {
      function a(c, e) {
        this.rangeDec = new n(c);
        this.outWindow = new d(e);
        this.markerIsMandatory = !1;
        this.dictSizeInProperties = this.dictSize = this.lp = this.pb = this.lc = 0;
        this.leftToUnpack = this.unpackSize = void 0;
        this.reps = new Int32Array(4);
        this.state = 0;
      }
      a.prototype.decodeProperties = function(a) {
        var c = a[0];
        if (225 <= c) {
          throw Error("Incorrect LZMA properties");
        }
        this.lc = c % 9;
        c = c / 9 | 0;
        this.pb = c / 5 | 0;
        this.lp = c % 5;
        for (c = this.dictSizeInProperties = 0;4 > c;c++) {
          this.dictSizeInProperties |= a[c + 1] << 8 * c;
        }
        this.dictSize = this.dictSizeInProperties;
        4096 > this.dictSize && (this.dictSize = 4096);
      };
      a.prototype.create = function() {
        this.outWindow.create(this.dictSize);
        this.init();
        this.rangeDec.init();
        this.reps[0] = 0;
        this.reps[1] = 0;
        this.reps[2] = 0;
        this.state = this.reps[3] = 0;
        this.leftToUnpack = this.unpackSize;
      };
      a.prototype.decodeLiteral = function(a, c) {
        var e = this.outWindow, d = this.rangeDec, l = 0;
        e.isEmpty() || (l = e.getByte(1));
        var n = 1, l = 768 * (((e.totalPos & (1 << this.lp) - 1) << this.lc) + (l >> 8 - this.lc));
        if (7 <= a) {
          e = e.getByte(c + 1);
          do {
            var b = e >> 7 & 1, e = e << 1, h = d.decodeBit(this.litProbs, l + ((1 + b << 8) + n)), n = n << 1 | h;
            if (b !== h) {
              break;
            }
          } while (256 > n);
        }
        for (;256 > n;) {
          n = n << 1 | d.decodeBit(this.litProbs, l + n);
        }
        return n - 256 & 255;
      };
      a.prototype.decodeDistance = function(a) {
        var c = a;
        3 < c && (c = 3);
        a = this.rangeDec;
        c = this.posSlotDecoder[c].decode(a);
        if (4 > c) {
          return c;
        }
        var e = (c >> 1) - 1, d = (2 | c & 1) << e;
        14 > c ? d = d + b(this.posDecoders, d - c, e, a) | 0 : (d = d + (a.decodeDirectBits(e - 4) << 4) | 0, d = d + this.alignDecoder.reverseDecode(a) | 0);
        return d;
      };
      a.prototype.init = function() {
        this.litProbs = g(768 << this.lc + this.lp);
        this.posSlotDecoder = u(6, 4);
        this.alignDecoder = new f(4);
        this.posDecoders = g(115);
        this.isMatch = g(192);
        this.isRep = g(12);
        this.isRepG0 = g(12);
        this.isRepG1 = g(12);
        this.isRepG2 = g(12);
        this.isRep0Long = g(192);
        this.lenDecoder = new v;
        this.repLenDecoder = new v;
      };
      a.prototype.decode = function(a) {
        for (var c = this.rangeDec, e = this.outWindow, d = this.pb, n = this.dictSize, b = this.markerIsMandatory, h = this.leftToUnpack, f = this.isMatch, m = this.isRep, v = this.isRepG0, g = this.isRepG1, k = this.isRepG2, p = this.isRep0Long, u = this.lenDecoder, z = this.repLenDecoder, A = this.reps[0], C = this.reps[1], x = this.reps[2], F = this.reps[3], B = this.state;;) {
          if (a && 48 > c.inStream.available) {
            this.outWindow.flush();
            break;
          }
          if (0 === h && !b && (this.outWindow.flush(), c.isFinishedOK())) {
            return t;
          }
          var E = e.totalPos & (1 << d) - 1;
          if (0 === c.decodeBit(f, (B << 4) + E)) {
            if (0 === h) {
              return r;
            }
            e.putByte(this.decodeLiteral(B, A));
            B = 4 > B ? 0 : 10 > B ? B - 3 : B - 6;
            h--;
          } else {
            if (0 !== c.decodeBit(m, B)) {
              if (0 === h || e.isEmpty()) {
                return r;
              }
              if (0 === c.decodeBit(v, B)) {
                if (0 === c.decodeBit(p, (B << 4) + E)) {
                  B = 7 > B ? 9 : 11;
                  e.putByte(e.getByte(A + 1));
                  h--;
                  continue;
                }
              } else {
                var G;
                0 === c.decodeBit(g, B) ? G = C : (0 === c.decodeBit(k, B) ? G = x : (G = F, F = x), x = C);
                C = A;
                A = G;
              }
              E = z.decode(c, E);
              B = 7 > B ? 8 : 11;
            } else {
              F = x;
              x = C;
              C = A;
              E = u.decode(c, E);
              B = 7 > B ? 7 : 10;
              A = this.decodeDistance(E);
              if (-1 === A) {
                return this.outWindow.flush(), c.isFinishedOK() ? w : r;
              }
              if (0 === h || A >= n || !e.checkDistance(A)) {
                return r;
              }
            }
            E += 2;
            G = !1;
            void 0 !== h && h < E && (E = h, G = !0);
            e.copyMatch(A + 1, E);
            h -= E;
            if (G) {
              return r;
            }
          }
        }
        this.state = B;
        this.reps[0] = A;
        this.reps[1] = C;
        this.reps[2] = x;
        this.reps[3] = F;
        this.leftToUnpack = h;
        return l;
      };
      return a;
    }(), r = 0, w = 1, t = 2, l = 3, c;
    (function(a) {
      a[a.WAIT_FOR_LZMA_HEADER = 0] = "WAIT_FOR_LZMA_HEADER";
      a[a.WAIT_FOR_SWF_HEADER = 1] = "WAIT_FOR_SWF_HEADER";
      a[a.PROCESS_DATA = 2] = "PROCESS_DATA";
      a[a.CLOSED = 3] = "CLOSED";
    })(c || (c = {}));
    c = function() {
      function c(a) {
        void 0 === a && (a = !1);
        this._state = a ? 1 : 0;
        this.buffer = null;
      }
      c.prototype.push = function(c) {
        if (2 > this._state) {
          var e = this.buffer ? this.buffer.length : 0, d = (1 === this._state ? 17 : 13) + 5;
          if (e + c.length < d) {
            d = new Uint8Array(e + c.length);
            0 < e && d.set(this.buffer);
            d.set(c, e);
            this.buffer = d;
            return;
          }
          var n = new Uint8Array(d);
          0 < e && n.set(this.buffer);
          n.set(c.subarray(0, d - e), e);
          this._inStream = new h;
          this._inStream.append(n.subarray(d - 5));
          this._outStream = new a(function(a) {
            this.onData.call(null, a);
          }.bind(this));
          this._decoder = new m(this._inStream, this._outStream);
          if (1 === this._state) {
            this._decoder.decodeProperties(n.subarray(12, 17)), this._decoder.markerIsMandatory = !1, this._decoder.unpackSize = ((n[4] | n[5] << 8 | n[6] << 16 | n[7] << 24) >>> 0) - 8;
          } else {
            this._decoder.decodeProperties(n.subarray(0, 5));
            for (var e = 0, b = !1, f = 0;8 > f;f++) {
              var t = n[5 + f];
              255 !== t && (b = !0);
              e |= t << 8 * f;
            }
            this._decoder.markerIsMandatory = !b;
            this._decoder.unpackSize = b ? e : void 0;
          }
          this._decoder.create();
          c = c.subarray(d);
          this._state = 2;
        }
        this._inStream.append(c);
        c = this._decoder.decode(!0);
        this._inStream.compact();
        c !== l && this._checkError(c);
      };
      c.prototype.close = function() {
        this._state = 3;
        var a = this._decoder.decode(!1);
        this._checkError(a);
        this._decoder = null;
      };
      c.prototype._checkError = function(a) {
        var c;
        a === r ? c = "LZMA decoding error" : a === l ? c = "Decoding is not complete" : a === w ? void 0 !== this._decoder.unpackSize && this._decoder.unpackSize !== this._outStream.processed && (c = "Finished with end marker before than specified size") : c = "Internal LZMA Error";
        if (c && this.onError) {
          this.onError(c);
        }
      };
      return c;
    }();
    k.LzmaDecoder = c;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    function g(a, n) {
      a !== d(a, 0, n) && throwError("RangeError", Errors.ParamRangeError);
    }
    function b(a) {
      return "string" === typeof a ? a : void 0 == a ? null : a + "";
    }
    var u = k.Debug.notImplemented, h = k.StringUtilities.utf8decode, a = k.StringUtilities.utf8encode, d = k.NumberUtilities.clamp, n = function() {
      return function(a, d, l) {
        this.buffer = a;
        this.length = d;
        this.littleEndian = l;
      };
    }();
    p.PlainObjectDataBuffer = n;
    for (var f = new Uint32Array(33), v = 1, m = 0;32 >= v;v++) {
      f[v] = m = m << 1 | 1;
    }
    var r;
    (function(a) {
      a[a.U8 = 1] = "U8";
      a[a.I32 = 2] = "I32";
      a[a.F32 = 4] = "F32";
    })(r || (r = {}));
    v = function() {
      function m(a) {
        void 0 === a && (a = m.INITIAL_SIZE);
        this._buffer || (this._buffer = new ArrayBuffer(a), this._position = this._length = 0, this._resetViews(), this._littleEndian = m._nativeLittleEndian, this._bitLength = this._bitBuffer = 0);
      }
      m.FromArrayBuffer = function(a, d) {
        void 0 === d && (d = -1);
        var c = Object.create(m.prototype);
        c._buffer = a;
        c._length = -1 === d ? a.byteLength : d;
        c._position = 0;
        c._resetViews();
        c._littleEndian = m._nativeLittleEndian;
        c._bitBuffer = 0;
        c._bitLength = 0;
        return c;
      };
      m.FromPlainObject = function(a) {
        var d = m.FromArrayBuffer(a.buffer, a.length);
        d._littleEndian = a.littleEndian;
        return d;
      };
      m.prototype.toPlainObject = function() {
        return new n(this._buffer, this._length, this._littleEndian);
      };
      m.prototype._resetViews = function() {
        this._u8 = new Uint8Array(this._buffer);
        this._f32 = this._i32 = null;
      };
      m.prototype._requestViews = function(a) {
        0 === (this._buffer.byteLength & 3) && (null === this._i32 && a & 2 && (this._i32 = new Int32Array(this._buffer)), null === this._f32 && a & 4 && (this._f32 = new Float32Array(this._buffer)));
      };
      m.prototype.getBytes = function() {
        return new Uint8Array(this._buffer, 0, this._length);
      };
      m.prototype._ensureCapacity = function(a) {
        var d = this._buffer;
        if (d.byteLength < a) {
          for (var c = Math.max(d.byteLength, 1);c < a;) {
            c *= 2;
          }
          a = m._arrayBufferPool.acquire(c);
          c = this._u8;
          this._buffer = a;
          this._resetViews();
          this._u8.set(c);
          m._arrayBufferPool.release(d);
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
      m.prototype.readBytes = function(a, d) {
        var c = 0;
        void 0 === c && (c = 0);
        void 0 === d && (d = 0);
        var e = this._position;
        c || (c = 0);
        d || (d = this._length - e);
        e + d > this._length && throwError("EOFError", Errors.EOFError);
        a.length < c + d && (a._ensureCapacity(c + d), a.length = c + d);
        a._u8.set(new Uint8Array(this._buffer, e, d), c);
        this._position += d;
      };
      m.prototype.readShort = function() {
        return this.readUnsignedShort() << 16 >> 16;
      };
      m.prototype.readUnsignedShort = function() {
        var a = this._u8, d = this._position;
        d + 2 > this._length && throwError("EOFError", Errors.EOFError);
        var c = a[d + 0], a = a[d + 1];
        this._position = d + 2;
        return this._littleEndian ? a << 8 | c : c << 8 | a;
      };
      m.prototype.readInt = function() {
        var a = this._u8, d = this._position;
        d + 4 > this._length && throwError("EOFError", Errors.EOFError);
        var c = a[d + 0], e = a[d + 1], q = a[d + 2], a = a[d + 3];
        this._position = d + 4;
        return this._littleEndian ? a << 24 | q << 16 | e << 8 | c : c << 24 | e << 16 | q << 8 | a;
      };
      m.prototype.readUnsignedInt = function() {
        return this.readInt() >>> 0;
      };
      m.prototype.readFloat = function() {
        var a = this._position;
        a + 4 > this._length && throwError("EOFError", Errors.EOFError);
        this._position = a + 4;
        this._requestViews(4);
        if (this._littleEndian && 0 === (a & 3) && this._f32) {
          return this._f32[a >> 2];
        }
        var d = this._u8, c = k.IntegerUtilities.u8;
        this._littleEndian ? (c[0] = d[a + 0], c[1] = d[a + 1], c[2] = d[a + 2], c[3] = d[a + 3]) : (c[3] = d[a + 0], c[2] = d[a + 1], c[1] = d[a + 2], c[0] = d[a + 3]);
        return k.IntegerUtilities.f32[0];
      };
      m.prototype.readDouble = function() {
        var a = this._u8, d = this._position;
        d + 8 > this._length && throwError("EOFError", Errors.EOFError);
        var c = k.IntegerUtilities.u8;
        this._littleEndian ? (c[0] = a[d + 0], c[1] = a[d + 1], c[2] = a[d + 2], c[3] = a[d + 3], c[4] = a[d + 4], c[5] = a[d + 5], c[6] = a[d + 6], c[7] = a[d + 7]) : (c[0] = a[d + 7], c[1] = a[d + 6], c[2] = a[d + 5], c[3] = a[d + 4], c[4] = a[d + 3], c[5] = a[d + 2], c[6] = a[d + 1], c[7] = a[d + 0]);
        this._position = d + 8;
        return k.IntegerUtilities.f64[0];
      };
      m.prototype.writeBoolean = function(a) {
        this.writeByte(a ? 1 : 0);
      };
      m.prototype.writeByte = function(a) {
        var d = this._position + 1;
        this._ensureCapacity(d);
        this._u8[this._position++] = a;
        d > this._length && (this._length = d);
      };
      m.prototype.writeUnsignedByte = function(a) {
        var d = this._position + 1;
        this._ensureCapacity(d);
        this._u8[this._position++] = a;
        d > this._length && (this._length = d);
      };
      m.prototype.writeRawBytes = function(a) {
        var d = this._position + a.length;
        this._ensureCapacity(d);
        this._u8.set(a, this._position);
        this._position = d;
        d > this._length && (this._length = d);
      };
      m.prototype.writeBytes = function(a, d, c) {
        void 0 === d && (d = 0);
        void 0 === c && (c = 0);
        k.isNullOrUndefined(a) && throwError("TypeError", Errors.NullPointerError, "bytes");
        2 > arguments.length && (d = 0);
        3 > arguments.length && (c = 0);
        g(d, a.length);
        g(d + c, a.length);
        0 === c && (c = a.length - d);
        this.writeRawBytes(new Int8Array(a._buffer, d, c));
      };
      m.prototype.writeShort = function(a) {
        this.writeUnsignedShort(a);
      };
      m.prototype.writeUnsignedShort = function(a) {
        var d = this._position;
        this._ensureCapacity(d + 2);
        var c = this._u8;
        this._littleEndian ? (c[d + 0] = a, c[d + 1] = a >> 8) : (c[d + 0] = a >> 8, c[d + 1] = a);
        this._position = d += 2;
        d > this._length && (this._length = d);
      };
      m.prototype.writeInt = function(a) {
        this.writeUnsignedInt(a);
      };
      m.prototype.write2Ints = function(a, d) {
        this.write2UnsignedInts(a, d);
      };
      m.prototype.write4Ints = function(a, d, c, e) {
        this.write4UnsignedInts(a, d, c, e);
      };
      m.prototype.writeUnsignedInt = function(a) {
        var d = this._position;
        this._ensureCapacity(d + 4);
        this._requestViews(2);
        if (this._littleEndian === m._nativeLittleEndian && 0 === (d & 3) && this._i32) {
          this._i32[d >> 2] = a;
        } else {
          var c = this._u8;
          this._littleEndian ? (c[d + 0] = a, c[d + 1] = a >> 8, c[d + 2] = a >> 16, c[d + 3] = a >> 24) : (c[d + 0] = a >> 24, c[d + 1] = a >> 16, c[d + 2] = a >> 8, c[d + 3] = a);
        }
        this._position = d += 4;
        d > this._length && (this._length = d);
      };
      m.prototype.write2UnsignedInts = function(a, d) {
        var c = this._position;
        this._ensureCapacity(c + 8);
        this._requestViews(2);
        this._littleEndian === m._nativeLittleEndian && 0 === (c & 3) && this._i32 ? (this._i32[(c >> 2) + 0] = a, this._i32[(c >> 2) + 1] = d, this._position = c += 8, c > this._length && (this._length = c)) : (this.writeUnsignedInt(a), this.writeUnsignedInt(d));
      };
      m.prototype.write4UnsignedInts = function(a, d, c, e) {
        var q = this._position;
        this._ensureCapacity(q + 16);
        this._requestViews(2);
        this._littleEndian === m._nativeLittleEndian && 0 === (q & 3) && this._i32 ? (this._i32[(q >> 2) + 0] = a, this._i32[(q >> 2) + 1] = d, this._i32[(q >> 2) + 2] = c, this._i32[(q >> 2) + 3] = e, this._position = q += 16, q > this._length && (this._length = q)) : (this.writeUnsignedInt(a), this.writeUnsignedInt(d), this.writeUnsignedInt(c), this.writeUnsignedInt(e));
      };
      m.prototype.writeFloat = function(a) {
        var d = this._position;
        this._ensureCapacity(d + 4);
        this._requestViews(4);
        if (this._littleEndian === m._nativeLittleEndian && 0 === (d & 3) && this._f32) {
          this._f32[d >> 2] = a;
        } else {
          var c = this._u8;
          k.IntegerUtilities.f32[0] = a;
          a = k.IntegerUtilities.u8;
          this._littleEndian ? (c[d + 0] = a[0], c[d + 1] = a[1], c[d + 2] = a[2], c[d + 3] = a[3]) : (c[d + 0] = a[3], c[d + 1] = a[2], c[d + 2] = a[1], c[d + 3] = a[0]);
        }
        this._position = d += 4;
        d > this._length && (this._length = d);
      };
      m.prototype.write6Floats = function(a, d, c, e, q, s) {
        var n = this._position;
        this._ensureCapacity(n + 24);
        this._requestViews(4);
        this._littleEndian === m._nativeLittleEndian && 0 === (n & 3) && this._f32 ? (this._f32[(n >> 2) + 0] = a, this._f32[(n >> 2) + 1] = d, this._f32[(n >> 2) + 2] = c, this._f32[(n >> 2) + 3] = e, this._f32[(n >> 2) + 4] = q, this._f32[(n >> 2) + 5] = s, this._position = n += 24, n > this._length && (this._length = n)) : (this.writeFloat(a), this.writeFloat(d), this.writeFloat(c), this.writeFloat(e), this.writeFloat(q), this.writeFloat(s));
      };
      m.prototype.writeDouble = function(a) {
        var d = this._position;
        this._ensureCapacity(d + 8);
        var c = this._u8;
        k.IntegerUtilities.f64[0] = a;
        a = k.IntegerUtilities.u8;
        this._littleEndian ? (c[d + 0] = a[0], c[d + 1] = a[1], c[d + 2] = a[2], c[d + 3] = a[3], c[d + 4] = a[4], c[d + 5] = a[5], c[d + 6] = a[6], c[d + 7] = a[7]) : (c[d + 0] = a[7], c[d + 1] = a[6], c[d + 2] = a[5], c[d + 3] = a[4], c[d + 4] = a[3], c[d + 5] = a[2], c[d + 6] = a[1], c[d + 7] = a[0]);
        this._position = d += 8;
        d > this._length && (this._length = d);
      };
      m.prototype.readRawBytes = function() {
        return new Int8Array(this._buffer, 0, this._length);
      };
      m.prototype.writeUTF = function(a) {
        a = b(a);
        a = h(a);
        this.writeShort(a.length);
        this.writeRawBytes(a);
      };
      m.prototype.writeUTFBytes = function(a) {
        a = b(a);
        a = h(a);
        this.writeRawBytes(a);
      };
      m.prototype.readUTF = function() {
        return this.readUTFBytes(this.readShort());
      };
      m.prototype.readUTFBytes = function(d) {
        d >>>= 0;
        var l = this._position;
        l + d > this._length && throwError("EOFError", Errors.EOFError);
        this._position += d;
        return a(new Int8Array(this._buffer, l, d));
      };
      Object.defineProperty(m.prototype, "length", {get:function() {
        return this._length;
      }, set:function(a) {
        a >>>= 0;
        a > this._buffer.byteLength && this._ensureCapacity(a);
        this._length = a;
        this._position = d(this._position, 0, this._length);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "bytesAvailable", {get:function() {
        return this._length - this._position;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "position", {get:function() {
        return this._position;
      }, set:function(a) {
        this._position = a >>> 0;
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
      }, set:function(a) {
        this._objectEncoding = a >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(m.prototype, "endian", {get:function() {
        return this._littleEndian ? "littleEndian" : "bigEndian";
      }, set:function(a) {
        a = b(a);
        this._littleEndian = "auto" === a ? m._nativeLittleEndian : "littleEndian" === a;
      }, enumerable:!0, configurable:!0});
      m.prototype.toString = function() {
        return a(new Int8Array(this._buffer, 0, this._length));
      };
      m.prototype.toBlob = function(a) {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)], {type:a});
      };
      m.prototype.writeMultiByte = function(a, d) {
        u("packageInternal flash.utils.ObjectOutput::writeMultiByte");
      };
      m.prototype.readMultiByte = function(a, d) {
        u("packageInternal flash.utils.ObjectInput::readMultiByte");
      };
      m.prototype.getValue = function(a) {
        a |= 0;
        return a >= this._length ? void 0 : this._u8[a];
      };
      m.prototype.setValue = function(a, d) {
        a |= 0;
        var c = a + 1;
        this._ensureCapacity(c);
        this._u8[a] = d;
        c > this._length && (this._length = c);
      };
      m.prototype.readFixed = function() {
        return this.readInt() / 65536;
      };
      m.prototype.readFixed8 = function() {
        return this.readShort() / 256;
      };
      m.prototype.readFloat16 = function() {
        var a = this.readUnsignedShort(), d = a >> 15 ? -1 : 1, c = (a & 31744) >> 10, a = a & 1023;
        return c ? 31 === c ? a ? NaN : Infinity * d : d * Math.pow(2, c - 15) * (1 + a / 1024) : a / 1024 * Math.pow(2, -14) * d;
      };
      m.prototype.readEncodedU32 = function() {
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
      m.prototype.readBits = function(a) {
        return this.readUnsignedBits(a) << 32 - a >> 32 - a;
      };
      m.prototype.readUnsignedBits = function(a) {
        for (var d = this._bitBuffer, c = this._bitLength;a > c;) {
          d = d << 8 | this.readUnsignedByte(), c += 8;
        }
        c -= a;
        a = d >>> c & f[a];
        this._bitBuffer = d;
        this._bitLength = c;
        return a;
      };
      m.prototype.readFixedBits = function(a) {
        return this.readBits(a) / 65536;
      };
      m.prototype.readString = function(d) {
        var l = this._position;
        if (d) {
          l + d > this._length && throwError("EOFError", Errors.EOFError), this._position += d;
        } else {
          d = 0;
          for (var c = l;c < this._length && this._u8[c];c++) {
            d++;
          }
          this._position += d + 1;
        }
        return a(new Int8Array(this._buffer, l, d));
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
      m.prototype.compress = function(a) {
        a = 0 === arguments.length ? "zlib" : b(a);
        var d;
        switch(a) {
          case "zlib":
            d = new p.Deflate(!0);
            break;
          case "deflate":
            d = new p.Deflate(!1);
            break;
          default:
            return;
        }
        var c = new m;
        d.onData = c.writeRawBytes.bind(c);
        d.push(this._u8.subarray(0, this._length));
        d.close();
        this._ensureCapacity(c._u8.length);
        this._u8.set(c._u8);
        this.length = c.length;
        this._position = 0;
      };
      m.prototype.uncompress = function(a) {
        a = 0 === arguments.length ? "zlib" : b(a);
        var d;
        switch(a) {
          case "zlib":
            d = p.Inflate.create(!0);
            break;
          case "deflate":
            d = p.Inflate.create(!1);
            break;
          case "lzma":
            d = new p.LzmaDecoder(!1);
            break;
          default:
            return;
        }
        var c = new m, e;
        d.onData = c.writeRawBytes.bind(c);
        d.onError = function(a) {
          return e = a;
        };
        d.push(this._u8.subarray(0, this._length));
        e && throwError("IOError", Errors.CompressedDataError);
        d.close();
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
    p.DataBuffer = v;
  })(k.ArrayUtilities || (k.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  var p = k.ArrayUtilities.DataBuffer, g = k.ArrayUtilities.ensureTypedArrayCapacity;
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
    return function(b, a, d, n, f, v, m, r, g, k, l) {
      this.commands = b;
      this.commandsPosition = a;
      this.coordinates = d;
      this.morphCoordinates = n;
      this.coordinatesPosition = f;
      this.styles = v;
      this.stylesLength = m;
      this.morphStyles = r;
      this.morphStylesLength = g;
      this.hasFills = k;
      this.hasLines = l;
    };
  }();
  k.PlainObjectShapeData = b;
  var u;
  (function(b) {
    b[b.Commands = 32] = "Commands";
    b[b.Coordinates = 128] = "Coordinates";
    b[b.Styles = 16] = "Styles";
  })(u || (u = {}));
  u = function() {
    function h(a) {
      void 0 === a && (a = !0);
      a && this.clear();
    }
    h.FromPlainObject = function(a) {
      var d = new h(!1);
      d.commands = a.commands;
      d.coordinates = a.coordinates;
      d.morphCoordinates = a.morphCoordinates;
      d.commandsPosition = a.commandsPosition;
      d.coordinatesPosition = a.coordinatesPosition;
      d.styles = p.FromArrayBuffer(a.styles, a.stylesLength);
      d.styles.endian = "auto";
      a.morphStyles && (d.morphStyles = p.FromArrayBuffer(a.morphStyles, a.morphStylesLength), d.morphStyles.endian = "auto");
      d.hasFills = a.hasFills;
      d.hasLines = a.hasLines;
      return d;
    };
    h.prototype.moveTo = function(a, d) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 9;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = d;
    };
    h.prototype.lineTo = function(a, d) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 10;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = d;
    };
    h.prototype.curveTo = function(a, d, b, f) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = d;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = f;
    };
    h.prototype.cubicCurveTo = function(a, d, b, f, h, m) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = d;
      this.coordinates[this.coordinatesPosition++] = b;
      this.coordinates[this.coordinatesPosition++] = f;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = m;
    };
    h.prototype.beginFill = function(a) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 1;
      this.styles.writeUnsignedInt(a);
      this.hasFills = !0;
    };
    h.prototype.writeMorphFill = function(a) {
      this.morphStyles.writeUnsignedInt(a);
    };
    h.prototype.endFill = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 4;
    };
    h.prototype.endLine = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 8;
    };
    h.prototype.lineStyle = function(a, d, b, f, h, m, r) {
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5;
      this.coordinates[this.coordinatesPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(d);
      a.writeBoolean(b);
      a.writeUnsignedByte(f);
      a.writeUnsignedByte(h);
      a.writeUnsignedByte(m);
      a.writeUnsignedByte(r);
      this.hasLines = !0;
    };
    h.prototype.writeMorphLineStyle = function(a, d) {
      this.morphCoordinates[this.coordinatesPosition - 1] = a;
      this.morphStyles.writeUnsignedInt(d);
    };
    h.prototype.beginBitmap = function(a, d, b, f, h) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(d);
      this._writeStyleMatrix(b, !1);
      a.writeBoolean(f);
      a.writeBoolean(h);
      this.hasFills = !0;
    };
    h.prototype.writeMorphBitmap = function(a) {
      this._writeStyleMatrix(a, !0);
    };
    h.prototype.beginGradient = function(a, d, b, f, h, m, r, g) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedByte(f);
      a.writeShort(g);
      this._writeStyleMatrix(h, !1);
      f = d.length;
      a.writeByte(f);
      for (h = 0;h < f;h++) {
        a.writeUnsignedByte(b[h]), a.writeUnsignedInt(d[h]);
      }
      a.writeUnsignedByte(m);
      a.writeUnsignedByte(r);
      this.hasFills = !0;
    };
    h.prototype.writeMorphGradient = function(a, d, b) {
      this._writeStyleMatrix(b, !0);
      b = this.morphStyles;
      for (var f = 0;f < a.length;f++) {
        b.writeUnsignedByte(d[f]), b.writeUnsignedInt(a[f]);
      }
    };
    h.prototype.writeCommandAndCoordinates = function(a, d, b) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = d;
      this.coordinates[this.coordinatesPosition++] = b;
    };
    h.prototype.writeCoordinates = function(a, d) {
      this.ensurePathCapacities(0, 2);
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = d;
    };
    h.prototype.writeMorphCoordinates = function(a, d) {
      this.morphCoordinates = g(this.morphCoordinates, this.coordinatesPosition);
      this.morphCoordinates[this.coordinatesPosition - 2] = a;
      this.morphCoordinates[this.coordinatesPosition - 1] = d;
    };
    h.prototype.clear = function() {
      this.commandsPosition = this.coordinatesPosition = 0;
      this.commands = new Uint8Array(32);
      this.coordinates = new Int32Array(128);
      this.styles = new p(16);
      this.styles.endian = "auto";
      this.hasFills = this.hasLines = !1;
    };
    h.prototype.isEmpty = function() {
      return 0 === this.commandsPosition;
    };
    h.prototype.clone = function() {
      var a = new h(!1);
      a.commands = new Uint8Array(this.commands);
      a.commandsPosition = this.commandsPosition;
      a.coordinates = new Int32Array(this.coordinates);
      a.coordinatesPosition = this.coordinatesPosition;
      a.styles = new p(this.styles.length);
      a.styles.writeRawBytes(this.styles.bytes);
      this.morphStyles && (a.morphStyles = new p(this.morphStyles.length), a.morphStyles.writeRawBytes(this.morphStyles.bytes));
      a.hasFills = this.hasFills;
      a.hasLines = this.hasLines;
      return a;
    };
    h.prototype.toPlainObject = function() {
      return new b(this.commands, this.commandsPosition, this.coordinates, this.morphCoordinates, this.coordinatesPosition, this.styles.buffer, this.styles.length, this.morphStyles && this.morphStyles.buffer, this.morphStyles ? this.morphStyles.length : 0, this.hasFills, this.hasLines);
    };
    Object.defineProperty(h.prototype, "buffers", {get:function() {
      var a = [this.commands.buffer, this.coordinates.buffer, this.styles.buffer];
      this.morphCoordinates && a.push(this.morphCoordinates.buffer);
      this.morphStyles && a.push(this.morphStyles.buffer);
      return a;
    }, enumerable:!0, configurable:!0});
    h.prototype._writeStyleMatrix = function(a, d) {
      (d ? this.morphStyles : this.styles).write6Floats(a.a, a.b, a.c, a.d, a.tx, a.ty);
    };
    h.prototype.ensurePathCapacities = function(a, d) {
      this.commands = g(this.commands, this.commandsPosition + a);
      this.coordinates = g(this.coordinates, this.coordinatesPosition + d);
    };
    return h;
  }();
  k.ShapeData = u;
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
        b[b.CODE_DEFINE_VIDEO_STREAM = 60] = "CODE_DEFINE_VIDEO_STREAM";
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
        b[b.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
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
  var p = k.Debug.unexpected, g = function() {
    function b(b, h, a, d) {
      this.url = b;
      this.method = h;
      this.mimeType = a;
      this.data = d;
    }
    b.prototype.readAll = function(b, h) {
      var a = this.url, d = new XMLHttpRequest({mozSystem:!0});
      d.open(this.method || "GET", this.url, !0);
      d.responseType = "arraybuffer";
      b && (d.onprogress = function(a) {
        b(d.response, a.loaded, a.total);
      });
      d.onreadystatechange = function(b) {
        4 === d.readyState && (200 !== d.status && 0 !== d.status || null === d.response ? (p("Path: " + a + " not found."), h(null, d.statusText)) : h(d.response));
      };
      this.mimeType && d.setRequestHeader("Content-Type", this.mimeType);
      d.send(this.data || null);
    };
    b.prototype.readChunked = function(b, h, a, d, n, f) {
      if (0 >= b) {
        this.readAsync(h, a, d, n, f);
      } else {
        var v = 0, m = new Uint8Array(b), r = 0, g;
        this.readAsync(function(a, d) {
          g = d.total;
          for (var c = a.length, e = 0;v + c >= b;) {
            var q = b - v;
            m.set(a.subarray(e, e + q), v);
            e += q;
            c -= q;
            r += b;
            h(m, {loaded:r, total:g});
            v = 0;
          }
          m.set(a.subarray(e), v);
          v += c;
        }, a, d, function() {
          0 < v && (r += v, h(m.subarray(0, v), {loaded:r, total:g}), v = 0);
          n && n();
        }, f);
      }
    };
    b.prototype.readAsync = function(b, h, a, d, n) {
      var f = new XMLHttpRequest({mozSystem:!0}), v = this.url, m = 0, r = 0;
      f.open(this.method || "GET", v, !0);
      f.responseType = "moz-chunked-arraybuffer";
      var g = "moz-chunked-arraybuffer" !== f.responseType;
      g && (f.responseType = "arraybuffer");
      f.onprogress = function(a) {
        g || (m = a.loaded, r = a.total, b(new Uint8Array(f.response), {loaded:m, total:r}));
      };
      f.onreadystatechange = function(a) {
        2 === f.readyState && n && n(v, f.status, f.getAllResponseHeaders());
        4 === f.readyState && (200 !== f.status && 0 !== f.status || null === f.response && (0 === r || m !== r) ? h(f.statusText) : (g && (a = f.response, b(new Uint8Array(a), {loaded:0, total:a.byteLength})), d && d()));
      };
      this.mimeType && f.setRequestHeader("Content-Type", this.mimeType);
      f.send(this.data || null);
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
        b.toRGBA = function(a, d, b, f) {
          void 0 === f && (f = 1);
          return "rgba(" + a + "," + d + "," + b + "," + f + ")";
        };
        return b;
      }();
      g.UI = b;
      var k = function() {
        function h() {
        }
        h.prototype.tabToolbar = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(37, 44, 51, a);
        };
        h.prototype.toolbars = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(52, 60, 69, a);
        };
        h.prototype.selectionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(29, 79, 115, a);
        };
        h.prototype.selectionText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(245, 247, 250, a);
        };
        h.prototype.splitters = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(0, 0, 0, a);
        };
        h.prototype.bodyBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(17, 19, 21, a);
        };
        h.prototype.sidebarBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(24, 29, 32, a);
        };
        h.prototype.attentionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(161, 134, 80, a);
        };
        h.prototype.bodyText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(143, 161, 178, a);
        };
        h.prototype.foregroundTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(182, 186, 191, a);
        };
        h.prototype.contentTextHighContrast = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(169, 186, 203, a);
        };
        h.prototype.contentTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(143, 161, 178, a);
        };
        h.prototype.contentTextDarkGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(95, 115, 135, a);
        };
        h.prototype.blueHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(70, 175, 227, a);
        };
        h.prototype.purpleHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(107, 122, 187, a);
        };
        h.prototype.pinkHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(223, 128, 255, a);
        };
        h.prototype.redHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(235, 83, 104, a);
        };
        h.prototype.orangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(217, 102, 41, a);
        };
        h.prototype.lightOrangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(217, 155, 40, a);
        };
        h.prototype.greenHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(112, 191, 83, a);
        };
        h.prototype.blueGreyHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(94, 136, 176, a);
        };
        return h;
      }();
      g.UIThemeDark = k;
      k = function() {
        function h() {
        }
        h.prototype.tabToolbar = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(235, 236, 237, a);
        };
        h.prototype.toolbars = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(240, 241, 242, a);
        };
        h.prototype.selectionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(76, 158, 217, a);
        };
        h.prototype.selectionText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(245, 247, 250, a);
        };
        h.prototype.splitters = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(170, 170, 170, a);
        };
        h.prototype.bodyBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(252, 252, 252, a);
        };
        h.prototype.sidebarBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(247, 247, 247, a);
        };
        h.prototype.attentionBackground = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(161, 134, 80, a);
        };
        h.prototype.bodyText = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(24, 25, 26, a);
        };
        h.prototype.foregroundTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(88, 89, 89, a);
        };
        h.prototype.contentTextHighContrast = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(41, 46, 51, a);
        };
        h.prototype.contentTextGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(143, 161, 178, a);
        };
        h.prototype.contentTextDarkGrey = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(102, 115, 128, a);
        };
        h.prototype.blueHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(0, 136, 204, a);
        };
        h.prototype.purpleHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(91, 95, 255, a);
        };
        h.prototype.pinkHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(184, 46, 229, a);
        };
        h.prototype.redHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(237, 38, 85, a);
        };
        h.prototype.orangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(241, 60, 0, a);
        };
        h.prototype.lightOrangeHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(217, 126, 0, a);
        };
        h.prototype.greenHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(44, 187, 15, a);
        };
        h.prototype.blueGreyHighlight = function(a) {
          void 0 === a && (a = 1);
          return b.toRGBA(95, 136, 176, a);
        };
        return h;
      }();
      g.UIThemeLight = k;
    })(k.Theme || (k.Theme = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var b = function() {
        function b(h, a) {
          this._buffers = h || [];
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
          for (var a = 0, d = this.snapshotCount;a < d;a++) {
            b(this._snapshots[a], a);
          }
        };
        b.prototype.createSnapshots = function() {
          var b = Number.MIN_VALUE, a = 0;
          for (this._snapshots = [];0 < this._buffers.length;) {
            var d = this._buffers.shift().createSnapshot();
            d && (b < d.endTime && (b = d.endTime), a < d.maxDepth && (a = d.maxDepth), this._snapshots.push(d));
          }
          this._windowEnd = this._endTime = b;
          this._maxDepth = a;
        };
        b.prototype.setWindow = function(b, a) {
          if (b > a) {
            var d = b;
            b = a;
            a = d;
          }
          d = Math.min(a - b, this.totalTime);
          b < this._startTime ? (b = this._startTime, a = this._startTime + d) : a > this._endTime && (b = this._endTime - d, a = this._endTime);
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
__extends = this.__extends || function(k, p) {
  function g() {
    this.constructor = k;
  }
  for (var b in p) {
    p.hasOwnProperty(b) && (k[b] = p[b]);
  }
  g.prototype = p.prototype;
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
        function h(a, d, b, f, h, m) {
          this.parent = a;
          this.kind = d;
          this.startData = b;
          this.endData = f;
          this.startTime = h;
          this.endTime = m;
          this.maxDepth = 0;
        }
        Object.defineProperty(h.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(h.prototype, "selfTime", {get:function() {
          var a = this.totalTime;
          if (this.children) {
            for (var d = 0, b = this.children.length;d < b;d++) {
              var f = this.children[d], a = a - (f.endTime - f.startTime)
            }
          }
          return a;
        }, enumerable:!0, configurable:!0});
        h.prototype.getChildIndex = function(a) {
          for (var d = this.children, b = 0;b < d.length;b++) {
            if (d[b].endTime > a) {
              return b;
            }
          }
          return 0;
        };
        h.prototype.getChildRange = function(a, d) {
          if (this.children && a <= this.endTime && d >= this.startTime && d >= a) {
            var b = this._getNearestChild(a), f = this._getNearestChildReverse(d);
            if (b <= f) {
              return a = this.children[b].startTime, d = this.children[f].endTime, {startIndex:b, endIndex:f, startTime:a, endTime:d, totalTime:d - a};
            }
          }
          return null;
        };
        h.prototype._getNearestChild = function(a) {
          var d = this.children;
          if (d && d.length) {
            if (a <= d[0].endTime) {
              return 0;
            }
            for (var b, f = 0, h = d.length - 1;h > f;) {
              b = (f + h) / 2 | 0;
              var m = d[b];
              if (a >= m.startTime && a <= m.endTime) {
                return b;
              }
              a > m.endTime ? f = b + 1 : h = b;
            }
            return Math.ceil((f + h) / 2);
          }
          return 0;
        };
        h.prototype._getNearestChildReverse = function(a) {
          var d = this.children;
          if (d && d.length) {
            var b = d.length - 1;
            if (a >= d[b].startTime) {
              return b;
            }
            for (var f, h = 0;b > h;) {
              f = Math.ceil((h + b) / 2);
              var m = d[f];
              if (a >= m.startTime && a <= m.endTime) {
                return f;
              }
              a > m.endTime ? h = f : b = f - 1;
            }
            return(h + b) / 2 | 0;
          }
          return 0;
        };
        h.prototype.query = function(a) {
          if (a < this.startTime || a > this.endTime) {
            return null;
          }
          var d = this.children;
          if (d && 0 < d.length) {
            for (var b, f = 0, h = d.length - 1;h > f;) {
              var m = (f + h) / 2 | 0;
              b = d[m];
              if (a >= b.startTime && a <= b.endTime) {
                return b.query(a);
              }
              a > b.endTime ? f = m + 1 : h = m;
            }
            b = d[h];
            if (a >= b.startTime && a <= b.endTime) {
              return b.query(a);
            }
          }
          return this;
        };
        h.prototype.queryNext = function(a) {
          for (var d = this;a > d.endTime;) {
            if (d.parent) {
              d = d.parent;
            } else {
              break;
            }
          }
          return d.query(a);
        };
        h.prototype.getDepth = function() {
          for (var a = 0, d = this;d;) {
            a++, d = d.parent;
          }
          return a;
        };
        h.prototype.calculateStatistics = function() {
          function a(n) {
            if (n.kind) {
              var f = d[n.kind.id] || (d[n.kind.id] = new b(n.kind));
              f.count++;
              f.selfTime += n.selfTime;
              f.totalTime += n.totalTime;
            }
            n.children && n.children.forEach(a);
          }
          var d = this.statistics = [];
          a(this);
        };
        h.prototype.trace = function(a) {
          var d = (this.kind ? this.kind.name + ": " : "Profile: ") + (this.endTime - this.startTime).toFixed(2);
          if (this.children && this.children.length) {
            a.enter(d);
            for (d = 0;d < this.children.length;d++) {
              this.children[d].trace(a);
            }
            a.outdent();
          } else {
            a.writeLn(d);
          }
        };
        return h;
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
  (function(p) {
    (function(g) {
      var b = function() {
        function b(h, a) {
          void 0 === h && (h = "");
          this.name = h || "";
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
        b.prototype._getKindId = function(h) {
          var a = b.MAX_KINDID;
          if (void 0 === this._kindNameMap[h]) {
            if (a = this._kinds.length, a < b.MAX_KINDID) {
              var d = {id:a, name:h, visible:!0};
              this._kinds.push(d);
              this._kindNameMap[h] = d;
            } else {
              a = b.MAX_KINDID;
            }
          } else {
            a = this._kindNameMap[h].id;
          }
          return a;
        };
        b.prototype._getMark = function(h, a, d) {
          var n = b.MAX_DATAID;
          k.isNullOrUndefined(d) || a === b.MAX_KINDID || (n = this._data.length, n < b.MAX_DATAID ? this._data.push(d) : n = b.MAX_DATAID);
          return h | n << 16 | a;
        };
        b.prototype.enter = function(h, a, d) {
          d = (k.isNullOrUndefined(d) ? performance.now() : d) - this._startTime;
          this._marks || this._initialize();
          this._depth++;
          h = this._getKindId(h);
          this._marks.write(this._getMark(b.ENTER, h, a));
          this._times.write(d);
          this._stack.push(h);
        };
        b.prototype.leave = function(h, a, d) {
          d = (k.isNullOrUndefined(d) ? performance.now() : d) - this._startTime;
          var n = this._stack.pop();
          h && (n = this._getKindId(h));
          this._marks.write(this._getMark(b.LEAVE, n, a));
          this._times.write(d);
          this._depth--;
        };
        b.prototype.count = function(b, a, d) {
        };
        b.prototype.createSnapshot = function() {
          var h;
          void 0 === h && (h = Number.MAX_VALUE);
          if (!this._marks) {
            return null;
          }
          var a = this._times, d = this._kinds, n = this._data, f = new g.TimelineBufferSnapshot(this.name), v = [f], m = 0;
          this._marks || this._initialize();
          this._marks.forEachInReverse(function(f, w) {
            var p = n[f >>> 16 & b.MAX_DATAID], l = d[f & b.MAX_KINDID];
            if (k.isNullOrUndefined(l) || l.visible) {
              var c = f & 2147483648, e = a.get(w), q = v.length;
              if (c === b.LEAVE) {
                if (1 === q && (m++, m > h)) {
                  return!0;
                }
                v.push(new g.TimelineFrame(v[q - 1], l, null, p, NaN, e));
              } else {
                if (c === b.ENTER) {
                  if (l = v.pop(), c = v[v.length - 1]) {
                    for (c.children ? c.children.unshift(l) : c.children = [l], c = v.length, l.depth = c, l.startData = p, l.startTime = e;l;) {
                      if (l.maxDepth < c) {
                        l.maxDepth = c, l = l.parent;
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
          f.children && f.children.length && (f.startTime = f.children[0].startTime, f.endTime = f.children[f.children.length - 1].endTime);
          return f;
        };
        b.prototype.reset = function(b) {
          this._startTime = k.isNullOrUndefined(b) ? performance.now() : b;
          this._marks ? (this._depth = 0, this._data = [], this._marks.reset(), this._times.reset()) : this._initialize();
        };
        b.FromFirefoxProfile = function(h, a) {
          for (var d = h.profile.threads[0].samples, n = new b(a, d[0].time), f = [], g, m = 0;m < d.length;m++) {
            g = d[m];
            var r = g.time, k = g.frames, p = 0;
            for (g = Math.min(k.length, f.length);p < g && k[p].location === f[p].location;) {
              p++;
            }
            for (var l = f.length - p, c = 0;c < l;c++) {
              g = f.pop(), n.leave(g.location, null, r);
            }
            for (;p < k.length;) {
              g = k[p++], n.enter(g.location, null, r);
            }
            f = k;
          }
          for (;g = f.pop();) {
            n.leave(g.location, null, r);
          }
          return n;
        };
        b.FromChromeProfile = function(h, a) {
          var d = h.timestamps, n = h.samples, f = new b(a, d[0] / 1E3), g = [], m = {}, r;
          b._resolveIds(h.head, m);
          for (var k = 0;k < d.length;k++) {
            var p = d[k] / 1E3, l = [];
            for (r = m[n[k]];r;) {
              l.unshift(r), r = r.parent;
            }
            var c = 0;
            for (r = Math.min(l.length, g.length);c < r && l[c] === g[c];) {
              c++;
            }
            for (var e = g.length - c, q = 0;q < e;q++) {
              r = g.pop(), f.leave(r.functionName, null, p);
            }
            for (;c < l.length;) {
              r = l[c++], f.enter(r.functionName, null, p);
            }
            g = l;
          }
          for (;r = g.pop();) {
            f.leave(r.functionName, null, p);
          }
          return f;
        };
        b._resolveIds = function(h, a) {
          a[h.id] = h;
          if (h.children) {
            for (var d = 0;d < h.children.length;d++) {
              h.children[d].parent = h, b._resolveIds(h.children[d], a);
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
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      (function(b) {
        b[b.DARK = 0] = "DARK";
        b[b.LIGHT = 1] = "LIGHT";
      })(g.UIThemeType || (g.UIThemeType = {}));
      var b = function() {
        function b(h, a) {
          void 0 === a && (a = 0);
          this._container = h;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = a;
          this._tooltip = this._createTooltip();
        }
        b.prototype.createProfile = function(b, a) {
          var d = new g.Profile(b, a);
          d.createSnapshots();
          this._profiles.push(d);
          this.activateProfile(d);
          return d;
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
              this._theme = new p.Theme.UIThemeDark;
              break;
            case 1:
              this._theme = new p.Theme.UIThemeLight;
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
            this._activeProfile.forEachSnapshot(function(a, d) {
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
            var b = this, a = this._activeProfile.startTime, d = this._activeProfile.endTime;
            this._overviewHeader.initialize(a, d);
            this._overview.initialize(a, d);
            this._activeProfile.forEachSnapshot(function(n, f) {
              b._headers[f].initialize(a, d);
              b._charts[f].initialize(a, d);
            });
          }
        };
        b.prototype._onResize = function() {
          if (this._activeProfile) {
            var b = this, a = this._container.offsetWidth;
            this._overviewHeader.setSize(a);
            this._overview.setSize(a);
            this._activeProfile.forEachSnapshot(function(d, n) {
              b._headers[n].setSize(a);
              b._charts[n].setSize(a);
            });
          }
        };
        b.prototype._updateViews = function() {
          if (this._activeProfile) {
            var b = this, a = this._activeProfile.windowStart, d = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(a, d);
            this._overview.setWindow(a, d);
            this._activeProfile.forEachSnapshot(function(n, f) {
              b._headers[f].setWindow(a, d);
              b._charts[f].setWindow(a, d);
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
        b.prototype.showTooltip = function(b, a, d, n) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(b, a));
          this._tooltip.style.display = "block";
          var f = this._tooltip.firstChild;
          a = f.clientWidth;
          f = f.clientHeight;
          d += d + a >= b.canvas.clientWidth - 50 ? -(a + 20) : 25;
          n += b.canvas.offsetTop - f / 2;
          this._tooltip.style.left = d + "px";
          this._tooltip.style.top = n + "px";
        };
        b.prototype.hideTooltip = function() {
          this._tooltip.style.display = "none";
        };
        b.prototype.createTooltipContent = function(b, a) {
          var d = Math.round(1E5 * a.totalTime) / 1E5, n = Math.round(1E5 * a.selfTime) / 1E5, f = Math.round(1E4 * a.selfTime / a.totalTime) / 100, g = document.createElement("div"), m = document.createElement("h1");
          m.textContent = a.kind.name;
          g.appendChild(m);
          m = document.createElement("p");
          m.textContent = "Total: " + d + " ms";
          g.appendChild(m);
          d = document.createElement("p");
          d.textContent = "Self: " + n + " ms (" + f + "%)";
          g.appendChild(d);
          if (n = b.getStatistics(a.kind)) {
            f = document.createElement("p"), f.textContent = "Count: " + n.count, g.appendChild(f), f = Math.round(1E5 * n.totalTime) / 1E5, d = document.createElement("p"), d.textContent = "All Total: " + f + " ms", g.appendChild(d), n = Math.round(1E5 * n.selfTime) / 1E5, f = document.createElement("p"), f.textContent = "All Self: " + n + " ms", g.appendChild(f);
          }
          this.appendDataElements(g, a.startData);
          this.appendDataElements(g, a.endData);
          return g;
        };
        b.prototype.appendDataElements = function(b, a) {
          if (!k.isNullOrUndefined(a)) {
            b.appendChild(document.createElement("hr"));
            var d;
            if (k.isObject(a)) {
              for (var n in a) {
                d = document.createElement("p"), d.textContent = n + ": " + a[n], b.appendChild(d);
              }
            } else {
              d = document.createElement("p"), d.textContent = a.toString(), b.appendChild(d);
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
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.NumberUtilities.clamp, p = function() {
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
      g.MouseCursor = p;
      var h = function() {
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
        a.prototype.updateCursor = function(d) {
          if (!a._cursorOwner || a._cursorOwner === this._target) {
            var b = this._eventTarget.parentElement;
            a._cursor !== d && (a._cursor = d, ["", "-moz-", "-webkit-"].forEach(function(a) {
              b.style.cursor = a + d;
            }));
            a._cursorOwner = a._cursor === p.DEFAULT ? null : this._target;
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
          var f = {x:a.x - b.start.x, y:a.y - b.start.y};
          b.current = a;
          b.delta = f;
          b.hasMoved = !0;
          this._target.onDrag(b.start.x, b.start.y, a.x, a.y, f.x, f.y);
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
            var n = this._getTargetMousePos(a, a.target);
            a = b("undefined" !== typeof a.deltaY ? a.deltaY / 16 : -a.wheelDelta / 40, -1, 1);
            this._target.onMouseWheel(n.x, n.y, Math.pow(1.2, a) - 1);
          }
        };
        a.prototype._startHoverCheck = function(d) {
          this._hoverInfo = {isHovering:!1, timeoutHandle:setTimeout(this._onMouseMoveIdleHandler.bind(this), a.HOVER_TIMEOUT), pos:this._getTargetMousePos(d, d.target)};
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
          var f = b.getBoundingClientRect();
          return{x:a.clientX - f.left, y:a.clientY - f.top};
        };
        a.HOVER_TIMEOUT = 500;
        a._cursor = p.DEFAULT;
        return a;
      }();
      g.MouseController = h;
    })(p.Profiler || (p.Profiler = {}));
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
        function b(h) {
          this._controller = h;
          this._initialized = !1;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
          this._mouseController = new g.MouseController(this, this._canvas);
          h = h.container;
          h.appendChild(this._canvas);
          h = h.getBoundingClientRect();
          this.setSize(h.width);
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
        b.prototype.setWindow = function(b, a, d) {
          void 0 === d && (d = !0);
          this._windowStart = b;
          this._windowEnd = a;
          !d || this.draw();
        };
        b.prototype.setRange = function(b, a) {
          var d = !1;
          void 0 === d && (d = !0);
          this._rangeStart = b;
          this._rangeEnd = a;
          !d || this.draw();
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
          var d;
          void 0 === d && (d = 10);
          return Math.abs(b - a) < 1 / Math.pow(10, d);
        };
        b.prototype._windowEqRange = function() {
          return this._almostEq(this._windowStart, this._rangeStart) && this._almostEq(this._windowEnd, this._rangeEnd);
        };
        b.prototype._decimalPlaces = function(b) {
          return(+b).toFixed(10).replace(/^-?\d*\.?|0+$/g, "").length;
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
        b.prototype.onMouseWheel = function(h, a, d) {
          h = this._toTime(h);
          a = this._windowStart;
          var n = this._windowEnd, f = n - a;
          d = Math.max((b.MIN_WINDOW_LEN - f) / f, d);
          this._controller.setWindow(a + (a - h) * d, n + (n - h) * d);
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
        b.prototype.onDrag = function(b, a, d, n, f, g) {
        };
        b.prototype.onDragEnd = function(b, a, d, n, f, g) {
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
  (function(p) {
    (function(g) {
      var b = k.StringUtilities.trimMiddle, p = function(h) {
        function a(a, b) {
          h.call(this, a);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = b;
          this._kindStyle = Object.create(null);
        }
        __extends(a, h);
        a.prototype.setSize = function(a, b) {
          h.prototype.setSize.call(this, a, b || this._initialized ? 12.5 * this._maxDepth : 100);
        };
        a.prototype.initialize = function(a, b) {
          this._initialized = !0;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(a, b);
          this.setWindow(a, b, !1);
          this.setSize(this._width, 12.5 * this._maxDepth);
        };
        a.prototype.destroy = function() {
          h.prototype.destroy.call(this);
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
          var f = a.getChildRange(this._windowStart, this._windowEnd);
          if (f) {
            for (var h = f.startIndex;h <= f.endIndex;h++) {
              var m = a.children[h];
              this._drawFrame(m, b) && this._drawChildren(m, b + 1);
            }
          }
        };
        a.prototype._drawFrame = function(a, b) {
          var f = this._context, h = this._toPixels(a.startTime), m = this._toPixels(a.endTime), g = m - h;
          if (g <= this._minFrameWidthInPixels) {
            return f.fillStyle = this._controller.theme.tabToolbar(1), f.fillRect(h, 12.5 * b, this._minFrameWidthInPixels, 12 + 12.5 * (a.maxDepth - a.depth)), !1;
          }
          0 > h && (m = g + h, h = 0);
          var m = m - h, w = this._kindStyle[a.kind.id];
          w || (w = k.ColorStyle.randomStyle(), w = this._kindStyle[a.kind.id] = {bgColor:w, textColor:k.ColorStyle.contrastStyle(w)});
          f.save();
          f.fillStyle = w.bgColor;
          f.fillRect(h, 12.5 * b, m, 12);
          12 < g && (g = a.kind.name) && g.length && (g = this._prepareText(f, g, m - 4), g.length && (f.fillStyle = w.textColor, f.textBaseline = "bottom", f.fillText(g, h + 2, 12.5 * (b + 1) - 1)));
          f.restore();
          return!0;
        };
        a.prototype._prepareText = function(a, n, f) {
          var h = this._measureWidth(a, n);
          if (f > h) {
            return n;
          }
          for (var h = 3, m = n.length;h < m;) {
            var g = h + m >> 1;
            this._measureWidth(a, b(n, g)) < f ? h = g + 1 : m = g;
          }
          n = b(n, m - 1);
          h = this._measureWidth(a, n);
          return h <= f ? n : "";
        };
        a.prototype._measureWidth = function(a, b) {
          var f = this._textWidth[b];
          f || (f = a.measureText(b).width, this._textWidth[b] = f);
          return f;
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
          var f = 1 + b / 12.5 | 0, h = this._snapshot.query(this._toTime(a));
          if (h && h.depth >= f) {
            for (;h && h.depth > f;) {
              h = h.parent;
            }
            return h;
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
        a.prototype.onDrag = function(a, b, f, h, m, g) {
          if (a = this._dragInfo) {
            m = this._toTimeRelative(-m), this._controller.setWindow(a.windowStartInitial + m, a.windowEndInitial + m);
          }
        };
        a.prototype.onDragEnd = function(a, b, f, h, m, r) {
          this._dragInfo = null;
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onClick = function(a, b) {
          this._dragInfo = null;
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onHoverStart = function(a, b) {
          var f = this._getFrameAtPosition(a, b);
          f && (this._hoveredFrame = f, this._controller.showTooltip(this, f, a, b));
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
      g.FlameChart = p;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.NumberUtilities.clamp;
      (function(b) {
        b[b.OVERLAY = 0] = "OVERLAY";
        b[b.STACK = 1] = "STACK";
        b[b.UNION = 2] = "UNION";
      })(g.FlameChartOverviewMode || (g.FlameChartOverviewMode = {}));
      var p = function(h) {
        function a(a, b) {
          void 0 === b && (b = 1);
          this._mode = b;
          this._overviewCanvasDirty = !0;
          this._overviewCanvas = document.createElement("canvas");
          this._overviewContext = this._overviewCanvas.getContext("2d");
          h.call(this, a);
        }
        __extends(a, h);
        a.prototype.setSize = function(a, b) {
          h.prototype.setSize.call(this, a, b || 64);
        };
        Object.defineProperty(a.prototype, "mode", {set:function(a) {
          this._mode = a;
          this.draw();
        }, enumerable:!0, configurable:!0});
        a.prototype._resetCanvas = function() {
          h.prototype._resetCanvas.call(this);
          this._overviewCanvas.width = this._canvas.width;
          this._overviewCanvas.height = this._canvas.height;
          this._overviewCanvasDirty = !0;
        };
        a.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio, f = this._width, h = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, f, h);
          a.restore();
          this._initialized && (this._overviewCanvasDirty && (this._drawChart(), this._overviewCanvasDirty = !1), a.drawImage(this._overviewCanvas, 0, 0), this._drawSelection());
        };
        a.prototype._drawSelection = function() {
          var a = this._context, b = this._height, f = window.devicePixelRatio, h = this._selection ? this._selection.left : this._toPixels(this._windowStart), m = this._selection ? this._selection.right : this._toPixels(this._windowEnd), g = this._controller.theme;
          a.save();
          a.scale(f, f);
          this._selection ? (a.fillStyle = g.selectionText(.15), a.fillRect(h, 1, m - h, b - 1), a.fillStyle = "rgba(133, 0, 0, 1)", a.fillRect(h + .5, 0, m - h - 1, 4), a.fillRect(h + .5, b - 4, m - h - 1, 4)) : (a.fillStyle = g.bodyBackground(.4), a.fillRect(0, 1, h, b - 1), a.fillRect(m, 1, this._width, b - 1));
          a.beginPath();
          a.moveTo(h, 0);
          a.lineTo(h, b);
          a.moveTo(m, 0);
          a.lineTo(m, b);
          a.lineWidth = .5;
          a.strokeStyle = g.foregroundTextGrey(1);
          a.stroke();
          b = Math.abs((this._selection ? this._toTime(this._selection.right) : this._windowEnd) - (this._selection ? this._toTime(this._selection.left) : this._windowStart));
          a.fillStyle = g.selectionText(.5);
          a.font = "8px sans-serif";
          a.textBaseline = "alphabetic";
          a.textAlign = "end";
          a.fillText(b.toFixed(2), Math.min(h, m) - 4, 10);
          a.fillText((b / 60).toFixed(2), Math.min(h, m) - 4, 20);
          a.restore();
        };
        a.prototype._drawChart = function() {
          var a = window.devicePixelRatio, b = this._height, f = this._controller.activeProfile, h = 4 * this._width, m = f.totalTime / h, g = this._overviewContext, k = this._controller.theme.blueHighlight(1);
          g.save();
          g.translate(0, a * b);
          var p = -a * b / (f.maxDepth - 1);
          g.scale(a / 4, p);
          g.clearRect(0, 0, h, f.maxDepth - 1);
          1 == this._mode && g.scale(1, 1 / f.snapshotCount);
          for (var l = 0, c = f.snapshotCount;l < c;l++) {
            var e = f.getSnapshotAt(l);
            if (e) {
              var q = null, s = 0;
              g.beginPath();
              g.moveTo(0, 0);
              for (var y = 0;y < h;y++) {
                s = f.startTime + y * m, s = (q = q ? q.queryNext(s) : e.query(s)) ? q.getDepth() - 1 : 0, g.lineTo(y, s);
              }
              g.lineTo(y, 0);
              g.fillStyle = k;
              g.fill();
              1 == this._mode && g.translate(0, -b * a / p);
            }
          }
          g.restore();
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
            var f = this._toPixels(this._windowStart), h = this._toPixels(this._windowEnd), m = 2 + g.FlameChartBase.DRAGHANDLE_WIDTH / 2, r = a >= f - m && a <= f + m, k = a >= h - m && a <= h + m;
            if (r && k) {
              return 4;
            }
            if (r) {
              return 2;
            }
            if (k) {
              return 3;
            }
            if (!this._windowEqRange() && a > f + m && a < h - m) {
              return 1;
            }
          }
          return 0;
        };
        a.prototype.onMouseDown = function(a, b) {
          var f = this._getDragTargetUnderCursor(a, b);
          0 === f ? (this._selection = {left:a, right:a}, this.draw()) : (1 === f && this._mouseController.updateCursor(g.MouseCursor.GRABBING), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:f});
        };
        a.prototype.onMouseMove = function(a, b) {
          var f = g.MouseCursor.DEFAULT, h = this._getDragTargetUnderCursor(a, b);
          0 === h || this._selection || (f = 1 === h ? g.MouseCursor.GRAB : g.MouseCursor.EW_RESIZE);
          this._mouseController.updateCursor(f);
        };
        a.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, n, f, h, m, r) {
          if (this._selection) {
            this._selection = {left:a, right:b(f, 0, this._width - 1)}, this.draw();
          } else {
            a = this._dragInfo;
            if (4 === a.target) {
              if (0 !== m) {
                a.target = 0 > m ? 2 : 3;
              } else {
                return;
              }
            }
            n = this._windowStart;
            f = this._windowEnd;
            m = this._toTimeRelative(m);
            switch(a.target) {
              case 1:
                n = a.windowStartInitial + m;
                f = a.windowEndInitial + m;
                break;
              case 2:
                n = b(a.windowStartInitial + m, this._rangeStart, f - g.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3:
                f = b(a.windowEndInitial + m, n + g.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(n, f);
          }
        };
        a.prototype.onDragEnd = function(a, b, f, h, m, g) {
          this._selection && (this._selection = null, this._controller.setWindow(this._toTime(a), this._toTime(f)));
          this._dragInfo = null;
          this.onMouseMove(f, h);
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
      g.FlameChartOverview = p;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.NumberUtilities.clamp;
      (function(b) {
        b[b.OVERVIEW = 0] = "OVERVIEW";
        b[b.CHART = 1] = "CHART";
      })(g.FlameChartHeaderType || (g.FlameChartHeaderType = {}));
      var p = function(h) {
        function a(a, b) {
          this._type = b;
          h.call(this, a);
        }
        __extends(a, h);
        a.prototype.draw = function() {
          var a = this._context, b = window.devicePixelRatio, f = this._width, h = this._height;
          a.save();
          a.scale(b, b);
          a.fillStyle = this._controller.theme.tabToolbar(1);
          a.fillRect(0, 0, f, h);
          this._initialized && (0 == this._type ? (b = this._toPixels(this._windowStart), f = this._toPixels(this._windowEnd), a.fillStyle = this._controller.theme.bodyBackground(1), a.fillRect(b, 0, f - b, h), this._drawLabels(this._rangeStart, this._rangeEnd), this._drawDragHandle(b), this._drawDragHandle(f)) : this._drawLabels(this._windowStart, this._windowEnd));
          a.restore();
        };
        a.prototype._drawLabels = function(b, n) {
          var f = this._context, h = this._calculateTickInterval(b, n), m = Math.ceil(b / h) * h, g = 500 <= h, k = g ? 1E3 : 1, p = this._decimalPlaces(h / k), g = g ? "s" : "ms", l = this._toPixels(m), c = this._height / 2, e = this._controller.theme;
          f.lineWidth = 1;
          f.strokeStyle = e.contentTextDarkGrey(.5);
          f.fillStyle = e.contentTextDarkGrey(1);
          f.textAlign = "right";
          f.textBaseline = "middle";
          f.font = "11px sans-serif";
          for (e = this._width + a.TICK_MAX_WIDTH;l < e;) {
            f.fillText((m / k).toFixed(p) + " " + g, l - 7, c + 1), f.beginPath(), f.moveTo(l, 0), f.lineTo(l, this._height + 1), f.closePath(), f.stroke(), m += h, l = this._toPixels(m);
          }
        };
        a.prototype._calculateTickInterval = function(b, n) {
          var f = (n - b) / (this._width / a.TICK_MAX_WIDTH), h = Math.pow(10, Math.floor(Math.log(f) / Math.LN10)), f = f / h;
          return 5 < f ? 10 * h : 2 < f ? 5 * h : 1 < f ? 2 * h : h;
        };
        a.prototype._drawDragHandle = function(a) {
          var b = this._context;
          b.lineWidth = 2;
          b.strokeStyle = this._controller.theme.bodyBackground(1);
          b.fillStyle = this._controller.theme.foregroundTextGrey(.7);
          this._drawRoundedRect(b, a - g.FlameChartBase.DRAGHANDLE_WIDTH / 2, g.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2);
        };
        a.prototype._drawRoundedRect = function(a, b, f, h) {
          var m, g = !0;
          void 0 === g && (g = !0);
          void 0 === m && (m = !0);
          a.beginPath();
          a.moveTo(b + 2, 1);
          a.lineTo(b + f - 2, 1);
          a.quadraticCurveTo(b + f, 1, b + f, 3);
          a.lineTo(b + f, 1 + h - 2);
          a.quadraticCurveTo(b + f, 1 + h, b + f - 2, 1 + h);
          a.lineTo(b + 2, 1 + h);
          a.quadraticCurveTo(b, 1 + h, b, 1 + h - 2);
          a.lineTo(b, 3);
          a.quadraticCurveTo(b, 1, b + 2, 1);
          a.closePath();
          g && a.stroke();
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
              var f = this._toPixels(this._windowStart), h = this._toPixels(this._windowEnd), m = 2 + g.FlameChartBase.DRAGHANDLE_WIDTH / 2, f = a >= f - m && a <= f + m, h = a >= h - m && a <= h + m;
              if (f && h) {
                return 4;
              }
              if (f) {
                return 2;
              }
              if (h) {
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
          var f = this._getDragTargetUnderCursor(a, b);
          1 === f && this._mouseController.updateCursor(g.MouseCursor.GRABBING);
          this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:f};
        };
        a.prototype.onMouseMove = function(a, b) {
          var f = g.MouseCursor.DEFAULT, h = this._getDragTargetUnderCursor(a, b);
          0 !== h && (1 !== h ? f = g.MouseCursor.EW_RESIZE : 1 !== h || this._windowEqRange() || (f = g.MouseCursor.GRAB));
          this._mouseController.updateCursor(f);
        };
        a.prototype.onMouseOver = function(a, b) {
          this.onMouseMove(a, b);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(g.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, n, f, h, m, k) {
          a = this._dragInfo;
          if (4 === a.target) {
            if (0 !== m) {
              a.target = 0 > m ? 2 : 3;
            } else {
              return;
            }
          }
          n = this._windowStart;
          f = this._windowEnd;
          m = this._toTimeRelative(m);
          switch(a.target) {
            case 1:
              f = 0 === this._type ? 1 : -1;
              n = a.windowStartInitial + f * m;
              f = a.windowEndInitial + f * m;
              break;
            case 2:
              n = b(a.windowStartInitial + m, this._rangeStart, f - g.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3:
              f = b(a.windowEndInitial + m, n + g.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(n, f);
        };
        a.prototype.onDragEnd = function(a, b, f, h, m, g) {
          this._dragInfo = null;
          this.onMouseMove(f, h);
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
      g.FlameChartHeader = p;
    })(p.Profiler || (p.Profiler = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      (function(b) {
        var g = function() {
          function a(a, b, f, h, m) {
            this.pageLoaded = a;
            this.threadsTotal = b;
            this.threadsLoaded = f;
            this.threadFilesTotal = h;
            this.threadFilesLoaded = m;
          }
          a.prototype.toString = function() {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function(a, b, f) {
              return a + ":" + this[a];
            }, this).join(", ") + "]";
          };
          return a;
        }();
        b.TraceLoggerProgressInfo = g;
        var h = function() {
          function a(a) {
            this._baseUrl = a;
            this._threads = [];
            this._progressInfo = null;
          }
          a.prototype.loadPage = function(a, b, f) {
            this._threads = [];
            this._pageLoadCallback = b;
            this._pageLoadProgressCallback = f;
            this._progressInfo = new g(!1, 0, 0, 0, 0);
            this._loadData([a], this._onLoadPage.bind(this));
          };
          Object.defineProperty(a.prototype, "buffers", {get:function() {
            for (var a = [], b = 0, f = this._threads.length;b < f;b++) {
              a.push(this._threads[b].buffer);
            }
            return a;
          }, enumerable:!0, configurable:!0});
          a.prototype._onProgress = function() {
            this._pageLoadProgressCallback && this._pageLoadProgressCallback.call(this, this._progressInfo);
          };
          a.prototype._onLoadPage = function(a) {
            if (a && 1 == a.length) {
              var h = this, f = 0;
              a = a[0];
              var g = a.length;
              this._threads = Array(g);
              this._progressInfo.pageLoaded = !0;
              this._progressInfo.threadsTotal = g;
              for (var m = 0;m < a.length;m++) {
                var k = a[m], w = [k.dict, k.tree];
                k.corrections && w.push(k.corrections);
                this._progressInfo.threadFilesTotal += w.length;
                this._loadData(w, function(a) {
                  return function(d) {
                    d && (d = new b.Thread(d), d.buffer.name = "Thread " + a, h._threads[a] = d);
                    f++;
                    h._progressInfo.threadsLoaded++;
                    h._onProgress();
                    f === g && h._pageLoadCallback.call(h, null, h._threads);
                  };
                }(m), function(a) {
                  h._progressInfo.threadFilesLoaded++;
                  h._onProgress();
                });
              }
              this._onProgress();
            } else {
              this._pageLoadCallback.call(this, "Error loading page.", null);
            }
          };
          a.prototype._loadData = function(a, b, f) {
            var h = 0, m = 0, g = a.length, k = [];
            k.length = g;
            for (var p = 0;p < g;p++) {
              var l = this._baseUrl + a[p], c = new XMLHttpRequest, e = /\.tl$/i.test(l) ? "arraybuffer" : "json";
              c.open("GET", l, !0);
              c.responseType = e;
              c.onload = function(a, c) {
                return function(e) {
                  if ("json" === c) {
                    if (e = this.response, "string" === typeof e) {
                      try {
                        e = JSON.parse(e), k[a] = e;
                      } catch (d) {
                        m++;
                      }
                    } else {
                      k[a] = e;
                    }
                  } else {
                    k[a] = this.response;
                  }
                  ++h;
                  f && f(h);
                  h === g && b(k);
                };
              }(p, e);
              c.send();
            }
          };
          a.colors = "#0044ff #8c4b00 #cc5c33 #ff80c4 #ffbfd9 #ff8800 #8c5e00 #adcc33 #b380ff #bfd9ff #ffaa00 #8c0038 #bf8f30 #f780ff #cc99c9 #aaff00 #000073 #452699 #cc8166 #cca799 #000066 #992626 #cc6666 #ccc299 #ff6600 #526600 #992663 #cc6681 #99ccc2 #ff0066 #520066 #269973 #61994d #739699 #ffcc00 #006629 #269199 #94994d #738299 #ff0000 #590000 #234d8c #8c6246 #7d7399 #ee00ff #00474d #8c2385 #8c7546 #7c8c69 #eeff00 #4d003d #662e1a #62468c #8c6969 #6600ff #4c2900 #1a6657 #8c464f #8c6981 #44ff00 #401100 #1a2466 #663355 #567365 #d90074 #403300 #101d40 #59562d #66614d #cc0000 #002b40 #234010 #4c2626 #4d5e66 #00a3cc #400011 #231040 #4c3626 #464359 #0000bf #331b00 #80e6ff #311a33 #4d3939 #a69b00 #003329 #80ffb2 #331a20 #40303d #00a658 #40ffd9 #ffc480 #ffe1bf #332b26 #8c2500 #9933cc #80fff6 #ffbfbf #303326 #005e8c #33cc47 #b2ff80 #c8bfff #263332 #00708c #cc33ad #ffe680 #f2ffbf #262a33 #388c00 #335ccc #8091ff #bfffd9".split(" ");
          return a;
        }();
        b.TraceLogger = h;
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
            var d = this._data, n = this._buffer;
            do {
              var f = a * b.ITEM_SIZE, g = 4294967295 * d.getUint32(f + 0) + d.getUint32(f + 4), m = 4294967295 * d.getUint32(f + 8) + d.getUint32(f + 12), k = d.getUint32(f + 16), f = d.getUint32(f + 20), p = 1 === (k & 1), k = k >>> 1, k = this._text[k];
              n.enter(k, null, g / 1E6);
              p && this._walkTree(a + 1);
              n.leave(k, null, m / 1E6);
              a = f;
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
  (function(p) {
    (function(g) {
      var b = k.NumberUtilities.clamp, p = function() {
        function a() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        a.prototype.append = function(a, b) {
          var f = this.lines;
          0 < f.length && f[f.length - 1] === a ? this.repeat[f.length - 1]++ : (this.lines.push(a), this.repeat.push(1), this.format.push(b ? {backgroundFillStyle:b} : void 0), this.time.push(performance.now()), this.length++);
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
      g.Buffer = p;
      var h = function() {
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
          this.buffer = new p;
          a.addEventListener("keydown", function(a) {
            var d = 0;
            switch(a.keyCode) {
              case q:
                this.showLineNumbers = !this.showLineNumbers;
                break;
              case s:
                this.showLineTime = !this.showLineTime;
                break;
              case g:
                d = -1;
                break;
              case k:
                d = 1;
                break;
              case b:
                d = -this.pageLineCount;
                break;
              case f:
                d = this.pageLineCount;
                break;
              case h:
                d = -this.lineIndex;
                break;
              case m:
                d = this.buffer.length - this.lineIndex;
                break;
              case t:
                this.columnIndex -= a.metaKey ? 10 : 1;
                0 > this.columnIndex && (this.columnIndex = 0);
                a.preventDefault();
                break;
              case l:
                this.columnIndex += a.metaKey ? 10 : 1;
                a.preventDefault();
                break;
              case c:
                a.metaKey && (this.selection = {start:0, end:this.buffer.length}, a.preventDefault());
                break;
              case e:
                if (a.metaKey) {
                  var V = "";
                  if (this.selection) {
                    for (var R = this.selection.start;R <= this.selection.end;R++) {
                      V += this.buffer.get(R) + "\n";
                    }
                  } else {
                    V = this.buffer.get(this.lineIndex);
                  }
                  alert(V);
                }
              ;
            }
            a.metaKey && (d *= this.pageLineCount);
            d && (this.scroll(d), a.preventDefault());
            d && a.shiftKey ? this.selection ? this.lineIndex > this.selection.start ? this.selection.end = this.lineIndex : this.selection.start = this.lineIndex : 0 < d ? this.selection = {start:this.lineIndex - d, end:this.lineIndex} : 0 > d && (this.selection = {start:this.lineIndex, end:this.lineIndex - d}) : d && (this.selection = null);
            this.paint();
          }.bind(this), !1);
          a.addEventListener("focus", function(a) {
            this.hasFocus = !0;
          }.bind(this), !1);
          a.addEventListener("blur", function(a) {
            this.hasFocus = !1;
          }.bind(this), !1);
          var b = 33, f = 34, h = 36, m = 35, g = 38, k = 40, t = 37, l = 39, c = 65, e = 67, q = 78, s = 84;
        }
        a.prototype.resize = function() {
          this._resizeHandler();
        };
        a.prototype._resizeHandler = function() {
          var a = this.canvas.parentElement, b = a.clientWidth, a = a.clientHeight - 1, f = window.devicePixelRatio || 1;
          1 !== f ? (this.ratio = f / 1, this.canvas.width = b * this.ratio, this.canvas.height = a * this.ratio, this.canvas.style.width = b + "px", this.canvas.style.height = a + "px") : (this.ratio = 1, this.canvas.width = b, this.canvas.height = a);
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
          var b = this.textMarginLeft, f = b + (this.showLineNumbers ? 5 * (String(this.buffer.length).length + 2) : 0), h = f + (this.showLineTime ? 40 : 10), m = h + 25;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
          for (var g = this.canvas.width, k = this.lineHeight, p = 0;p < a;p++) {
            var l = p * this.lineHeight, c = this.pageIndex + p, e = this.buffer.get(c), q = this.buffer.getFormat(c), s = this.buffer.getRepeat(c), y = 1 < c ? this.buffer.getTime(c) - this.buffer.getTime(0) : 0;
            this.context.fillStyle = c % 2 ? this.lineColor : this.alternateLineColor;
            q && q.backgroundFillStyle && (this.context.fillStyle = q.backgroundFillStyle);
            this.context.fillRect(0, l, g, k);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;
            this.selection && c >= this.selection.start && c <= this.selection.end && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, l, g, k), this.context.fillStyle = this.selectionTextColor);
            this.hasFocus && c === this.lineIndex && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, l, g, k), this.context.fillStyle = this.selectionTextColor);
            0 < this.columnIndex && (e = e.substring(this.columnIndex));
            l = (p + 1) * this.lineHeight - this.textMarginBottom;
            this.showLineNumbers && this.context.fillText(String(c), b, l);
            this.showLineTime && this.context.fillText(y.toFixed(1).padLeft(" ", 6), f, l);
            1 < s && this.context.fillText(String(s).padLeft(" ", 3), h, l);
            this.context.fillText(e, m, l);
          }
        };
        a.prototype.refreshEvery = function(a) {
          function b() {
            f.paint();
            f.refreshFrequency && setTimeout(b, f.refreshFrequency);
          }
          var f = this;
          this.refreshFrequency = a;
          f.refreshFrequency && setTimeout(b, f.refreshFrequency);
        };
        a.prototype.isScrolledToBottom = function() {
          return this.lineIndex === this.buffer.length - 1;
        };
        return a;
      }();
      g.Terminal = h;
    })(p.Terminal || (p.Terminal = {}));
  })(k.Tools || (k.Tools = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var b = function() {
        function b(h) {
          this._lastWeightedTime = this._lastTime = this._index = 0;
          this._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
          this._container = h;
          this._canvas = document.createElement("canvas");
          this._container.appendChild(this._canvas);
          this._context = this._canvas.getContext("2d");
          this._listenForContainerSizeChanges();
        }
        b.prototype._listenForContainerSizeChanges = function() {
          var b = this._containerWidth, a = this._containerHeight;
          this._onContainerSizeChanged();
          var d = this;
          setInterval(function() {
            if (b !== d._containerWidth || a !== d._containerHeight) {
              d._onContainerSizeChanged(), b = d._containerWidth, a = d._containerHeight;
            }
          }, 10);
        };
        b.prototype._onContainerSizeChanged = function() {
          var b = this._containerWidth, a = this._containerHeight, d = window.devicePixelRatio || 1;
          1 !== d ? (this._ratio = d / 1, this._canvas.width = b * this._ratio, this._canvas.height = a * this._ratio, this._canvas.style.width = b + "px", this._canvas.style.height = a + "px") : (this._ratio = 1, this._canvas.width = b, this._canvas.height = a);
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
            var d = 1 * (performance.now() - this._lastTime) + 0 * this._lastWeightedTime, g = this._context, f = 2 * this._ratio, k = 30 * this._ratio, m = performance;
            m.memory && (k += 30 * this._ratio);
            var r = (this._canvas.width - k) / (f + 1) | 0, p = this._index++;
            this._index > r && (this._index = 0);
            r = this._canvas.height;
            g.globalAlpha = 1;
            g.fillStyle = "black";
            g.fillRect(k + p * (f + 1), 0, 4 * f, this._canvas.height);
            var t = Math.min(1E3 / 60 / d, 1);
            g.fillStyle = "#00FF00";
            g.globalAlpha = b ? .5 : 1;
            t = r / 2 * t | 0;
            g.fillRect(k + p * (f + 1), r - t, f, t);
            a && (t = Math.min(1E3 / 240 / a, 1), g.fillStyle = "#FF6347", t = r / 2 * t | 0, g.fillRect(k + p * (f + 1), r / 2 - t, f, t));
            0 === p % 16 && (g.globalAlpha = 1, g.fillStyle = "black", g.fillRect(0, 0, k, this._canvas.height), g.fillStyle = "white", g.font = 8 * this._ratio + "px Arial", g.textBaseline = "middle", f = (1E3 / d).toFixed(0), a && (f += " " + a.toFixed(0)), m.memory && (f += " " + (m.memory.usedJSHeapSize / 1024 / 1024).toFixed(2)), g.fillText(f, 2 * this._ratio, this._containerHeight / 2 * this._ratio));
            this._lastTime = performance.now();
            this._lastWeightedTime = d;
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
  (function(p) {
    function g(a, c, e) {
      return n && e ? "string" === typeof c ? (a = k.ColorUtilities.cssStyleToRGBA(c), k.ColorUtilities.rgbaToCSSStyle(e.transformRGBA(a))) : c instanceof CanvasGradient && c._template ? c._template.createCanvasGradient(a, e) : c : c;
    }
    var b = k.NumberUtilities.clamp;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Brief = 1] = "Brief";
      a[a.Verbose = 2] = "Verbose";
    })(p.TraceLevel || (p.TraceLevel = {}));
    var u = k.Metrics.Counter.instance;
    p.frameCounter = new k.Metrics.Counter(!0);
    p.traceLevel = 2;
    p.writer = null;
    p.frameCount = function(a) {
      u.count(a);
      p.frameCounter.count(a);
    };
    p.timelineBuffer = new k.Tools.Profiler.TimelineBuffer("GFX");
    p.enterTimeline = function(a, c) {
    };
    p.leaveTimeline = function(a, c) {
    };
    var h = null, a = null, d = null, n = !0;
    n && "undefined" !== typeof CanvasRenderingContext2D && (h = CanvasGradient.prototype.addColorStop, a = CanvasRenderingContext2D.prototype.createLinearGradient, d = CanvasRenderingContext2D.prototype.createRadialGradient, CanvasRenderingContext2D.prototype.createLinearGradient = function(a, c, e, b) {
      return(new v(a, c, e, b)).createCanvasGradient(this, null);
    }, CanvasRenderingContext2D.prototype.createRadialGradient = function(a, c, e, b, q, d) {
      return(new m(a, c, e, b, q, d)).createCanvasGradient(this, null);
    }, CanvasGradient.prototype.addColorStop = function(a, c) {
      h.call(this, a, c);
      this._template.addColorStop(a, c);
    });
    var f = function() {
      return function(a, c) {
        this.offset = a;
        this.color = c;
      };
    }(), v = function() {
      function c(a, e, b, q) {
        this.x0 = a;
        this.y0 = e;
        this.x1 = b;
        this.y1 = q;
        this.colorStops = [];
      }
      c.prototype.addColorStop = function(a, c) {
        this.colorStops.push(new f(a, c));
      };
      c.prototype.createCanvasGradient = function(c, e) {
        for (var b = a.call(c, this.x0, this.y0, this.x1, this.y1), q = this.colorStops, d = 0;d < q.length;d++) {
          var s = q[d], l = s.offset, s = s.color, s = e ? g(c, s, e) : s;
          h.call(b, l, s);
        }
        b._template = this;
        b._transform = this._transform;
        return b;
      };
      return c;
    }(), m = function() {
      function a(c, e, b, q, d, s) {
        this.x0 = c;
        this.y0 = e;
        this.r0 = b;
        this.x1 = q;
        this.y1 = d;
        this.r1 = s;
        this.colorStops = [];
      }
      a.prototype.addColorStop = function(a, c) {
        this.colorStops.push(new f(a, c));
      };
      a.prototype.createCanvasGradient = function(a, c) {
        for (var e = d.call(a, this.x0, this.y0, this.r0, this.x1, this.y1, this.r1), b = this.colorStops, q = 0;q < b.length;q++) {
          var s = b[q], l = s.offset, s = s.color, s = c ? g(a, s, c) : s;
          h.call(e, l, s);
        }
        e._template = this;
        e._transform = this._transform;
        return e;
      };
      return a;
    }(), r;
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
    })(r || (r = {}));
    var w = function() {
      function a(c) {
        this._commands = new Uint8Array(a._arrayBufferPool.acquire(8), 0, 8);
        this._commandPosition = 0;
        this._data = new Float64Array(a._arrayBufferPool.acquire(8 * Float64Array.BYTES_PER_ELEMENT), 0, 8);
        this._dataPosition = 0;
        c instanceof a && this.addPath(c);
      }
      a._apply = function(a, c) {
        var e = a._commands, b = a._data, q = 0, d = 0;
        c.beginPath();
        for (var s = a._commandPosition;q < s;) {
          switch(e[q++]) {
            case 1:
              c.closePath();
              break;
            case 2:
              c.moveTo(b[d++], b[d++]);
              break;
            case 3:
              c.lineTo(b[d++], b[d++]);
              break;
            case 4:
              c.quadraticCurveTo(b[d++], b[d++], b[d++], b[d++]);
              break;
            case 5:
              c.bezierCurveTo(b[d++], b[d++], b[d++], b[d++], b[d++], b[d++]);
              break;
            case 6:
              c.arcTo(b[d++], b[d++], b[d++], b[d++], b[d++]);
              break;
            case 7:
              c.rect(b[d++], b[d++], b[d++], b[d++]);
              break;
            case 8:
              c.arc(b[d++], b[d++], b[d++], b[d++], b[d++], !!b[d++]);
              break;
            case 9:
              c.save();
              break;
            case 10:
              c.restore();
              break;
            case 11:
              c.transform(b[d++], b[d++], b[d++], b[d++], b[d++], b[d++]);
          }
        }
      };
      a.prototype._ensureCommandCapacity = function(c) {
        this._commands = a._arrayBufferPool.ensureUint8ArrayLength(this._commands, c);
      };
      a.prototype._ensureDataCapacity = function(c) {
        this._data = a._arrayBufferPool.ensureFloat64ArrayLength(this._data, c);
      };
      a.prototype._writeCommand = function(a) {
        this._commandPosition >= this._commands.length && this._ensureCommandCapacity(this._commandPosition + 1);
        this._commands[this._commandPosition++] = a;
      };
      a.prototype._writeData = function(a, c, e, b, q, d) {
        var s = arguments.length;
        this._dataPosition + s >= this._data.length && this._ensureDataCapacity(this._dataPosition + s);
        var l = this._data, m = this._dataPosition;
        l[m] = a;
        l[m + 1] = c;
        2 < s && (l[m + 2] = e, l[m + 3] = b, 4 < s && (l[m + 4] = q, 6 === s && (l[m + 5] = d)));
        this._dataPosition += s;
      };
      a.prototype.closePath = function() {
        this._writeCommand(1);
      };
      a.prototype.moveTo = function(a, c) {
        this._writeCommand(2);
        this._writeData(a, c);
      };
      a.prototype.lineTo = function(a, c) {
        this._writeCommand(3);
        this._writeData(a, c);
      };
      a.prototype.quadraticCurveTo = function(a, c, e, b) {
        this._writeCommand(4);
        this._writeData(a, c, e, b);
      };
      a.prototype.bezierCurveTo = function(a, c, e, b, q, d) {
        this._writeCommand(5);
        this._writeData(a, c, e, b, q, d);
      };
      a.prototype.arcTo = function(a, c, e, b, q) {
        this._writeCommand(6);
        this._writeData(a, c, e, b, q);
      };
      a.prototype.rect = function(a, c, e, b) {
        this._writeCommand(7);
        this._writeData(a, c, e, b);
      };
      a.prototype.arc = function(a, c, e, b, q, d) {
        this._writeCommand(8);
        this._writeData(a, c, e, b, q, +d);
      };
      a.prototype.addPath = function(a, c) {
        c && (this._writeCommand(9), this._writeCommand(11), this._writeData(c.a, c.b, c.c, c.d, c.e, c.f));
        var e = this._commandPosition + a._commandPosition;
        e >= this._commands.length && this._ensureCommandCapacity(e);
        for (var b = this._commands, q = a._commands, d = this._commandPosition, s = 0;d < e;d++) {
          b[d] = q[s++];
        }
        this._commandPosition = e;
        e = this._dataPosition + a._dataPosition;
        e >= this._data.length && this._ensureDataCapacity(e);
        b = this._data;
        q = a._data;
        d = this._dataPosition;
        for (s = 0;d < e;d++) {
          b[d] = q[s++];
        }
        this._dataPosition = e;
        c && this._writeCommand(10);
      };
      a._arrayBufferPool = new k.ArrayBufferPool;
      return a;
    }();
    p.Path = w;
    if ("undefined" !== typeof CanvasRenderingContext2D && ("undefined" === typeof Path2D || !Path2D.prototype.addPath)) {
      var t = CanvasRenderingContext2D.prototype.fill;
      CanvasRenderingContext2D.prototype.fill = function(a, c) {
        arguments.length && (a instanceof w ? w._apply(a, this) : c = a);
        c ? t.call(this, c) : t.call(this);
      };
      var l = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.stroke = function(a, c) {
        arguments.length && (a instanceof w ? w._apply(a, this) : c = a);
        c ? l.call(this, c) : l.call(this);
      };
      var c = CanvasRenderingContext2D.prototype.clip;
      CanvasRenderingContext2D.prototype.clip = function(a, e) {
        arguments.length && (a instanceof w ? w._apply(a, this) : e = a);
        e ? c.call(this, e) : c.call(this);
      };
      window.Path2D = w;
    }
    if ("undefined" !== typeof CanvasPattern && Path2D.prototype.addPath) {
      r = function(a) {
        this._transform = a;
        this._template && (this._template._transform = a);
      };
      CanvasPattern.prototype.setTransform || (CanvasPattern.prototype.setTransform = r);
      CanvasGradient.prototype.setTransform || (CanvasGradient.prototype.setTransform = r);
      var e = CanvasRenderingContext2D.prototype.fill, q = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.fill = function(a, c) {
        var b = !!this.fillStyle._transform;
        if ((this.fillStyle instanceof CanvasPattern || this.fillStyle instanceof CanvasGradient) && b && a instanceof Path2D) {
          var b = this.fillStyle._transform, q;
          try {
            q = b.inverse();
          } catch (d) {
            q = b = p.Geometry.Matrix.createIdentitySVGMatrix();
          }
          this.transform(b.a, b.b, b.c, b.d, b.e, b.f);
          b = new Path2D;
          b.addPath(a, q);
          e.call(this, b, c);
          this.transform(q.a, q.b, q.c, q.d, q.e, q.f);
        } else {
          0 === arguments.length ? e.call(this) : 1 === arguments.length ? e.call(this, a) : 2 === arguments.length && e.call(this, a, c);
        }
      };
      CanvasRenderingContext2D.prototype.stroke = function(a) {
        var c = !!this.strokeStyle._transform;
        if ((this.strokeStyle instanceof CanvasPattern || this.strokeStyle instanceof CanvasGradient) && c && a instanceof Path2D) {
          var e = this.strokeStyle._transform, c = e.inverse();
          this.transform(e.a, e.b, e.c, e.d, e.e, e.f);
          e = new Path2D;
          e.addPath(a, c);
          var b = this.lineWidth;
          this.lineWidth *= (c.a + c.d) / 2;
          q.call(this, e);
          this.transform(c.a, c.b, c.c, c.d, c.e, c.f);
          this.lineWidth = b;
        } else {
          0 === arguments.length ? q.call(this) : 1 === arguments.length && q.call(this, a);
        }
      };
    }
    "undefined" !== typeof CanvasRenderingContext2D && function() {
      function a() {
        return p.Geometry.Matrix.createSVGMatrixFromArray(this.mozCurrentTransform);
      }
      CanvasRenderingContext2D.prototype.flashStroke = function(a, c) {
        var e = this.currentTransform;
        if (e) {
          var q = new Path2D;
          q.addPath(a, e);
          var d = this.lineWidth;
          this.setTransform(1, 0, 0, 1, 0, 0);
          switch(c) {
            case 1:
              this.lineWidth = b(d * (k.getScaleX(e) + k.getScaleY(e)) / 2, 1, 1024);
              break;
            case 2:
              this.lineWidth = b(d * k.getScaleY(e), 1, 1024);
              break;
            case 3:
              this.lineWidth = b(d * k.getScaleX(e), 1, 1024);
          }
          this.stroke(q);
          this.setTransform(e.a, e.b, e.c, e.d, e.e, e.f);
          this.lineWidth = d;
        } else {
          this.stroke(a);
        }
      };
      !("currentTransform" in CanvasRenderingContext2D.prototype) && "mozCurrentTransform" in CanvasRenderingContext2D.prototype && Object.defineProperty(CanvasRenderingContext2D.prototype, "currentTransform", {get:a});
    }();
    if ("undefined" !== typeof CanvasRenderingContext2D && void 0 === CanvasRenderingContext2D.prototype.globalColorMatrix) {
      var s = CanvasRenderingContext2D.prototype.fill, y = CanvasRenderingContext2D.prototype.stroke, I = CanvasRenderingContext2D.prototype.fillText, V = CanvasRenderingContext2D.prototype.strokeText;
      Object.defineProperty(CanvasRenderingContext2D.prototype, "globalColorMatrix", {get:function() {
        return this._globalColorMatrix ? this._globalColorMatrix.clone() : null;
      }, set:function(a) {
        a ? this._globalColorMatrix ? this._globalColorMatrix.set(a) : this._globalColorMatrix = a.clone() : this._globalColorMatrix = null;
      }, enumerable:!0, configurable:!0});
      CanvasRenderingContext2D.prototype.fill = function(a, c) {
        var e = null;
        this._globalColorMatrix && (e = this.fillStyle, this.fillStyle = g(this, this.fillStyle, this._globalColorMatrix));
        0 === arguments.length ? s.call(this) : 1 === arguments.length ? s.call(this, a) : 2 === arguments.length && s.call(this, a, c);
        e && (this.fillStyle = e);
      };
      CanvasRenderingContext2D.prototype.stroke = function(a, c) {
        var e = null;
        this._globalColorMatrix && (e = this.strokeStyle, this.strokeStyle = g(this, this.strokeStyle, this._globalColorMatrix));
        0 === arguments.length ? y.call(this) : 1 === arguments.length && y.call(this, a);
        e && (this.strokeStyle = e);
      };
      CanvasRenderingContext2D.prototype.fillText = function(a, c, e, b) {
        var q = null;
        this._globalColorMatrix && (q = this.fillStyle, this.fillStyle = g(this, this.fillStyle, this._globalColorMatrix));
        3 === arguments.length ? I.call(this, a, c, e) : 4 === arguments.length ? I.call(this, a, c, e, b) : k.Debug.unexpected();
        q && (this.fillStyle = q);
      };
      CanvasRenderingContext2D.prototype.strokeText = function(a, c, e, b) {
        var q = null;
        this._globalColorMatrix && (q = this.strokeStyle, this.strokeStyle = g(this, this.strokeStyle, this._globalColorMatrix));
        3 === arguments.length ? V.call(this, a, c, e) : 4 === arguments.length ? V.call(this, a, c, e, b) : k.Debug.unexpected();
        q && (this.strokeStyle = q);
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
  var p = function() {
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
      for (var h = g ? this._head : this._tail;h && b(h);) {
        h = g ? h.next : h.previous;
      }
    };
    return g;
  }();
  k.LRUList = p;
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
  (function(p) {
    (function(g) {
      function b(a, c, e, b) {
        var d = 1 - b;
        return a * d * d + 2 * c * d * b + e * b * b;
      }
      function u(a, c, e, b, d) {
        var m = d * d, f = 1 - d, g = f * f;
        return a * f * g + 3 * c * d * g + 3 * e * f * m + b * d * m;
      }
      var h = k.NumberUtilities.clamp, a = k.NumberUtilities.pow2, d = k.NumberUtilities.epsilonEquals;
      g.radianToDegrees = function(a) {
        return 180 * a / Math.PI;
      };
      g.degreesToRadian = function(a) {
        return a * Math.PI / 180;
      };
      g.quadraticBezier = b;
      g.quadraticBezierExtreme = function(a, c, e) {
        var q = (a - c) / (a - 2 * c + e);
        return 0 > q ? a : 1 < q ? e : b(a, c, e, q);
      };
      g.cubicBezier = u;
      g.cubicBezierExtremes = function(a, c, e, b) {
        var d = c - a, m;
        m = 2 * (e - c);
        var f = b - e;
        d + f === m && (f *= 1.0001);
        var g = 2 * d - m, h = m - 2 * d, h = Math.sqrt(h * h - 4 * d * (d - m + f));
        m = 2 * (d - m + f);
        d = (g + h) / m;
        g = (g - h) / m;
        h = [];
        0 <= d && 1 >= d && h.push(u(a, c, e, b, d));
        0 <= g && 1 >= g && h.push(u(a, c, e, b, g));
        return h;
      };
      var n = function() {
        function a(c, e) {
          this.x = c;
          this.y = e;
        }
        a.prototype.setElements = function(a, e) {
          this.x = a;
          this.y = e;
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
        a.prototype.inTriangle = function(a, e, b) {
          var d = a.y * b.x - a.x * b.y + (b.y - a.y) * this.x + (a.x - b.x) * this.y, l = a.x * e.y - a.y * e.x + (a.y - e.y) * this.x + (e.x - a.x) * this.y;
          if (0 > d != 0 > l) {
            return!1;
          }
          a = -e.y * b.x + a.y * (b.x - e.x) + a.x * (e.y - b.y) + e.x * b.y;
          0 > a && (d = -d, l = -l, a = -a);
          return 0 < d && 0 < l && d + l < a;
        };
        a.createEmpty = function() {
          return new a(0, 0);
        };
        a.createEmptyPoints = function(c) {
          for (var e = [], b = 0;b < c;b++) {
            e.push(new a(0, 0));
          }
          return e;
        };
        return a;
      }();
      g.Point = n;
      var f = function() {
        function a(c, e, b) {
          this.x = c;
          this.y = e;
          this.z = b;
        }
        a.prototype.setElements = function(a, e, b) {
          this.x = a;
          this.y = e;
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
          var e = this.z * a.x - this.x * a.z, b = this.x * a.y - this.y * a.x;
          this.x = this.y * a.z - this.z * a.y;
          this.y = e;
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
        a.createEmptyPoints = function(c) {
          for (var e = [], b = 0;b < c;b++) {
            e.push(new a(0, 0, 0));
          }
          return e;
        };
        return a;
      }();
      g.Point3D = f;
      var v = function() {
        function a(c, e, b, d) {
          this.setElements(c, e, b, d);
          a.allocationCount++;
        }
        a.prototype.setElements = function(a, e, b, d) {
          this.x = a;
          this.y = e;
          this.w = b;
          this.h = d;
        };
        a.prototype.set = function(a) {
          this.x = a.x;
          this.y = a.y;
          this.w = a.w;
          this.h = a.h;
        };
        a.prototype.contains = function(a) {
          var e = a.x + a.w, b = a.y + a.h, d = this.x + this.w, l = this.y + this.h;
          return a.x >= this.x && a.x < d && a.y >= this.y && a.y < l && e > this.x && e <= d && b > this.y && b <= l;
        };
        a.prototype.containsPoint = function(a) {
          return a.x >= this.x && a.x < this.x + this.w && a.y >= this.y && a.y < this.y + this.h;
        };
        a.prototype.isContained = function(a) {
          for (var e = 0;e < a.length;e++) {
            if (a[e].contains(this)) {
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
              var e = this.x, b = this.y;
              this.x > a.x && (e = a.x);
              this.y > a.y && (b = a.y);
              var d = this.x + this.w;
              d < a.x + a.w && (d = a.x + a.w);
              var l = this.y + this.h;
              l < a.y + a.h && (l = a.y + a.h);
              this.x = e;
              this.y = b;
              this.w = d - e;
              this.h = l - b;
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
          var e = a.createEmpty();
          if (this.isEmpty() || c.isEmpty()) {
            return e.setEmpty(), e;
          }
          e.x = Math.max(this.x, c.x);
          e.y = Math.max(this.y, c.y);
          e.w = Math.min(this.x + this.w, c.x + c.w) - e.x;
          e.h = Math.min(this.y + this.h, c.y + c.h) - e.y;
          e.isEmpty() && e.setEmpty();
          this.set(e);
        };
        a.prototype.intersects = function(a) {
          if (this.isEmpty() || a.isEmpty()) {
            return!1;
          }
          var e = Math.max(this.x, a.x), b = Math.max(this.y, a.y), e = Math.min(this.x + this.w, a.x + a.w) - e;
          a = Math.min(this.y + this.h, a.y + a.h) - b;
          return!(0 >= e || 0 >= a);
        };
        a.prototype.intersectsTransformedAABB = function(c, e) {
          var b = a._temporary;
          b.set(c);
          e.transformRectangleAABB(b);
          return this.intersects(b);
        };
        a.prototype.intersectsTranslated = function(a, e, b) {
          if (this.isEmpty() || a.isEmpty()) {
            return!1;
          }
          var d = Math.max(this.x, a.x + e), l = Math.max(this.y, a.y + b);
          e = Math.min(this.x + this.w, a.x + e + a.w) - d;
          a = Math.min(this.y + this.h, a.y + b + a.h) - l;
          return!(0 >= e || 0 >= a);
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
          var a = Math.ceil(this.x + this.w), e = Math.ceil(this.y + this.h);
          this.x = Math.floor(this.x);
          this.y = Math.floor(this.y);
          this.w = a - this.x;
          this.h = e - this.y;
          return this;
        };
        a.prototype.scale = function(a, e) {
          this.x *= a;
          this.y *= e;
          this.w *= a;
          this.h *= e;
          return this;
        };
        a.prototype.offset = function(a, e) {
          this.x += a;
          this.y += e;
          return this;
        };
        a.prototype.resize = function(a, e) {
          this.w += a;
          this.h += e;
          return this;
        };
        a.prototype.expand = function(a, e) {
          this.offset(-a, -e).resize(2 * a, 2 * e);
          return this;
        };
        a.prototype.getCenter = function() {
          return new n(this.x + this.w / 2, this.y + this.h / 2);
        };
        a.prototype.getAbsoluteBounds = function() {
          return new a(0, 0, this.w, this.h);
        };
        a.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          return "{" + this.x.toFixed(a) + ", " + this.y.toFixed(a) + ", " + this.w.toFixed(a) + ", " + this.h.toFixed(a) + "}";
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
      g.Rectangle = v;
      var m = function() {
        function a(c) {
          this.corners = c.map(function(a) {
            return a.clone();
          });
          this.axes = [c[1].clone().sub(c[0]), c[3].clone().sub(c[0])];
          this.origins = [];
          for (var e = 0;2 > e;e++) {
            this.axes[e].mul(1 / this.axes[e].squaredLength()), this.origins.push(c[0].dot(this.axes[e]));
          }
        }
        a.prototype.getBounds = function() {
          return a.getBounds(this.corners);
        };
        a.getBounds = function(a) {
          for (var e = new n(Number.MAX_VALUE, Number.MAX_VALUE), b = new n(Number.MIN_VALUE, Number.MIN_VALUE), d = 0;4 > d;d++) {
            var l = a[d].x, m = a[d].y;
            e.x = Math.min(e.x, l);
            e.y = Math.min(e.y, m);
            b.x = Math.max(b.x, l);
            b.y = Math.max(b.y, m);
          }
          return new v(e.x, e.y, b.x - e.x, b.y - e.y);
        };
        a.prototype.intersects = function(a) {
          return this.intersectsOneWay(a) && a.intersectsOneWay(this);
        };
        a.prototype.intersectsOneWay = function(a) {
          for (var e = 0;2 > e;e++) {
            for (var b = 0;4 > b;b++) {
              var d = a.corners[b].dot(this.axes[e]), l, m;
              0 === b ? m = l = d : d < l ? l = d : d > m && (m = d);
            }
            if (l > 1 + this.origins[e] || m < this.origins[e]) {
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
      var r = function() {
        function a(c, e, b, d, m, f) {
          this._data = new Float64Array(6);
          this._type = 0;
          this.setElements(c, e, b, d, m, f);
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
        a.prototype.setElements = function(a, e, b, d, m, l) {
          var f = this._data;
          f[0] = a;
          f[1] = e;
          f[2] = b;
          f[3] = d;
          f[4] = m;
          f[5] = l;
          this._type = 0;
        };
        a.prototype.set = function(a) {
          var e = this._data, b = a._data;
          e[0] = b[0];
          e[1] = b[1];
          e[2] = b[2];
          e[3] = b[3];
          e[4] = b[4];
          e[5] = b[5];
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
            return!0;
          }
          var e = this._data;
          a = a._data;
          return e[0] === a[0] && e[1] === a[1] && e[2] === a[2] && e[3] === a[3] && e[4] === a[4] && e[5] === a[5];
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
        a.prototype.transform = function(a, e, b, d, m, l) {
          var f = this._data, g = f[0], h = f[1], n = f[2], k = f[3], r = f[4], v = f[5];
          f[0] = g * a + n * e;
          f[1] = h * a + k * e;
          f[2] = g * b + n * d;
          f[3] = h * b + k * d;
          f[4] = g * m + n * l + r;
          f[5] = h * m + k * l + v;
          this._type = 0;
          return this;
        };
        a.prototype.transformRectangle = function(a, e) {
          var b = this._data, d = b[0], m = b[1], l = b[2], f = b[3], g = b[4], b = b[5], h = a.x, n = a.y, k = a.w, r = a.h;
          e[0].x = d * h + l * n + g;
          e[0].y = m * h + f * n + b;
          e[1].x = d * (h + k) + l * n + g;
          e[1].y = m * (h + k) + f * n + b;
          e[2].x = d * (h + k) + l * (n + r) + g;
          e[2].y = m * (h + k) + f * (n + r) + b;
          e[3].x = d * h + l * (n + r) + g;
          e[3].y = m * h + f * (n + r) + b;
        };
        a.prototype.isTranslationOnly = function() {
          if (2 === this._type) {
            return!0;
          }
          var a = this._data;
          return 1 === a[0] && 0 === a[1] && 0 === a[2] && 1 === a[3] || d(a[0], 1) && d(a[1], 0) && d(a[2], 0) && d(a[3], 1) ? (this._type = 2, !0) : !1;
        };
        a.prototype.transformRectangleAABB = function(a) {
          var e = this._data;
          if (1 !== this._type) {
            if (2 === this._type) {
              a.x += e[4], a.y += e[5];
            } else {
              var b = e[0], d = e[1], m = e[2], l = e[3], f = e[4], g = e[5], h = a.x, n = a.y, k = a.w, r = a.h, e = b * h + m * n + f, v = d * h + l * n + g, p = b * (h + k) + m * n + f, w = d * (h + k) + l * n + g, t = b * (h + k) + m * (n + r) + f, k = d * (h + k) + l * (n + r) + g, b = b * h + m * (n + r) + f, d = d * h + l * (n + r) + g, l = 0;
              e > p && (l = e, e = p, p = l);
              t > b && (l = t, t = b, b = l);
              a.x = e < t ? e : t;
              a.w = (p > b ? p : b) - a.x;
              v > w && (l = v, v = w, w = l);
              k > d && (l = k, k = d, d = l);
              a.y = v < k ? v : k;
              a.h = (w > d ? w : d) - a.y;
            }
          }
        };
        a.prototype.scale = function(a, e) {
          var b = this._data;
          b[0] *= a;
          b[1] *= e;
          b[2] *= a;
          b[3] *= e;
          b[4] *= a;
          b[5] *= e;
          this._type = 0;
          return this;
        };
        a.prototype.scaleClone = function(a, e) {
          return 1 === a && 1 === e ? this : this.clone().scale(a, e);
        };
        a.prototype.rotate = function(a) {
          var e = this._data, b = e[0], d = e[1], m = e[2], l = e[3], f = e[4], h = e[5], g = Math.cos(a);
          a = Math.sin(a);
          e[0] = g * b - a * d;
          e[1] = a * b + g * d;
          e[2] = g * m - a * l;
          e[3] = a * m + g * l;
          e[4] = g * f - a * h;
          e[5] = a * f + g * h;
          this._type = 0;
          return this;
        };
        a.prototype.concat = function(a) {
          if (1 === a._type) {
            return this;
          }
          var e = this._data;
          a = a._data;
          var b = e[0] * a[0], d = 0, m = 0, l = e[3] * a[3], f = e[4] * a[0] + a[4], g = e[5] * a[3] + a[5];
          if (0 !== e[1] || 0 !== e[2] || 0 !== a[1] || 0 !== a[2]) {
            b += e[1] * a[2], l += e[2] * a[1], d += e[0] * a[1] + e[1] * a[3], m += e[2] * a[0] + e[3] * a[2], f += e[5] * a[2], g += e[4] * a[1];
          }
          e[0] = b;
          e[1] = d;
          e[2] = m;
          e[3] = l;
          e[4] = f;
          e[5] = g;
          this._type = 0;
          return this;
        };
        a.prototype.concatClone = function(a) {
          return this.clone().concat(a);
        };
        a.prototype.preMultiply = function(a) {
          var e = this._data, b = a._data;
          if (2 === a._type && this._type & 3) {
            e[4] += b[4], e[5] += b[5], this._type = 2;
          } else {
            if (1 !== a._type) {
              a = b[0] * e[0];
              var d = 0, m = 0, l = b[3] * e[3], f = b[4] * e[0] + e[4], g = b[5] * e[3] + e[5];
              if (0 !== b[1] || 0 !== b[2] || 0 !== e[1] || 0 !== e[2]) {
                a += b[1] * e[2], l += b[2] * e[1], d += b[0] * e[1] + b[1] * e[3], m += b[2] * e[0] + b[3] * e[2], f += b[5] * e[2], g += b[4] * e[1];
              }
              e[0] = a;
              e[1] = d;
              e[2] = m;
              e[3] = l;
              e[4] = f;
              e[5] = g;
              this._type = 0;
            }
          }
        };
        a.prototype.translate = function(a, e) {
          var b = this._data;
          b[4] += a;
          b[5] += e;
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
            return!0;
          }
          var a = this._data;
          return 1 === a[0] && 0 === a[1] && 0 === a[2] && 1 === a[3] && 0 === a[4] && 0 === a[5];
        };
        a.prototype.transformPoint = function(a) {
          if (1 !== this._type) {
            var e = this._data, b = a.x, d = a.y;
            a.x = e[0] * b + e[2] * d + e[4];
            a.y = e[1] * b + e[3] * d + e[5];
          }
        };
        a.prototype.transformPoints = function(a) {
          if (1 !== this._type) {
            for (var e = 0;e < a.length;e++) {
              this.transformPoint(a[e]);
            }
          }
        };
        a.prototype.deltaTransformPoint = function(a) {
          if (1 !== this._type) {
            var e = this._data, b = a.x, d = a.y;
            a.x = e[0] * b + e[2] * d;
            a.y = e[1] * b + e[3] * d;
          }
        };
        a.prototype.inverse = function(a) {
          var e = this._data, b = a._data;
          if (1 === this._type) {
            a.setIdentity();
          } else {
            if (2 === this._type) {
              b[0] = 1, b[1] = 0, b[2] = 0, b[3] = 1, b[4] = -e[4], b[5] = -e[5], a._type = 2;
            } else {
              var d = e[1], m = e[2], l = e[4], f = e[5];
              if (0 === d && 0 === m) {
                var g = b[0] = 1 / e[0], e = b[3] = 1 / e[3];
                b[1] = 0;
                b[2] = 0;
                b[4] = -g * l;
                b[5] = -e * f;
              } else {
                var g = e[0], e = e[3], h = g * e - d * m;
                if (0 === h) {
                  a.setIdentity();
                  return;
                }
                h = 1 / h;
                b[0] = e * h;
                d = b[1] = -d * h;
                m = b[2] = -m * h;
                e = b[3] = g * h;
                b[4] = -(b[0] * l + m * f);
                b[5] = -(d * l + e * f);
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
          var e = Math.sqrt(a[0] * a[0] + a[1] * a[1]);
          return 0 < a[0] ? e : -e;
        };
        a.prototype.getScaleY = function() {
          var a = this._data;
          if (0 === a[2] && 1 === a[3]) {
            return 1;
          }
          var e = Math.sqrt(a[2] * a[2] + a[3] * a[3]);
          return 0 < a[3] ? e : -e;
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
          var a = this._data;
          return 180 * Math.atan(a[1] / a[0]) / Math.PI;
        };
        a.prototype.isScaleOrRotation = function() {
          var a = this._data;
          return.01 > Math.abs(a[0] * a[2] + a[1] * a[3]);
        };
        a.prototype.toString = function(a) {
          void 0 === a && (a = 2);
          var e = this._data;
          return "{" + e[0].toFixed(a) + ", " + e[1].toFixed(a) + ", " + e[2].toFixed(a) + ", " + e[3].toFixed(a) + ", " + e[4].toFixed(a) + ", " + e[5].toFixed(a) + "}";
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
          var c = a.allocate();
          c.setIdentity();
          return c;
        };
        a.prototype.toSVGMatrix = function() {
          var c = this._data, e = a._svg.createSVGMatrix();
          e.a = c[0];
          e.b = c[1];
          e.c = c[2];
          e.d = c[3];
          e.e = c[4];
          e.f = c[5];
          return e;
        };
        a.prototype.snap = function() {
          var a = this._data;
          return this.isTranslationOnly() ? (a[0] = 1, a[1] = 0, a[2] = 0, a[3] = 1, a[4] = Math.round(a[4]), a[5] = Math.round(a[5]), this._type = 2, !0) : !1;
        };
        a.createIdentitySVGMatrix = function() {
          return a._svg.createSVGMatrix();
        };
        a.createSVGMatrixFromArray = function(c) {
          var e = a._svg.createSVGMatrix();
          e.a = c[0];
          e.b = c[1];
          e.c = c[2];
          e.d = c[3];
          e.e = c[4];
          e.f = c[5];
          return e;
        };
        a.allocationCount = 0;
        a._dirtyStack = [];
        a._svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
        a.multiply = function(a, e) {
          var b = e._data;
          a.transform(b[0], b[1], b[2], b[3], b[4], b[5]);
        };
        return a;
      }();
      g.Matrix = r;
      r = function() {
        function a(c) {
          this._m = new Float32Array(c);
        }
        a.prototype.asWebGLMatrix = function() {
          return this._m;
        };
        a.createCameraLookAt = function(c, e, b) {
          e = c.clone().sub(e).normalize();
          b = b.clone().cross(e).normalize();
          var d = e.clone().cross(b);
          return new a([b.x, b.y, b.z, 0, d.x, d.y, d.z, 0, e.x, e.y, e.z, 0, c.x, c.y, c.z, 1]);
        };
        a.createLookAt = function(c, e, b) {
          e = c.clone().sub(e).normalize();
          b = b.clone().cross(e).normalize();
          var d = e.clone().cross(b);
          return new a([b.x, d.x, e.x, 0, d.x, d.y, e.y, 0, e.x, d.z, e.z, 0, -b.dot(c), -d.dot(c), -e.dot(c), 1]);
        };
        a.prototype.mul = function(a) {
          a = [a.x, a.y, a.z, 0];
          for (var e = this._m, b = [], d = 0;4 > d;d++) {
            b[d] = 0;
            for (var m = 4 * d, l = 0;4 > l;l++) {
              b[d] += e[m + l] * a[l];
            }
          }
          return new f(b[0], b[1], b[2]);
        };
        a.create2DProjection = function(c, e, b) {
          return new a([2 / c, 0, 0, 0, 0, -2 / e, 0, 0, 0, 0, 2 / b, 0, -1, 1, 0, 1]);
        };
        a.createPerspective = function(c) {
          c = Math.tan(.5 * Math.PI - .5 * c);
          var e = 1 / -4999.9;
          return new a([c / 1, 0, 0, 0, 0, c, 0, 0, 0, 0, 5000.1 * e, -1, 0, 0, 1E3 * e, 0]);
        };
        a.createIdentity = function() {
          return a.createTranslation(0, 0);
        };
        a.createTranslation = function(c, e) {
          return new a([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, c, e, 0, 1]);
        };
        a.createXRotation = function(c) {
          var e = Math.cos(c);
          c = Math.sin(c);
          return new a([1, 0, 0, 0, 0, e, c, 0, 0, -c, e, 0, 0, 0, 0, 1]);
        };
        a.createYRotation = function(c) {
          var e = Math.cos(c);
          c = Math.sin(c);
          return new a([e, 0, -c, 0, 0, 1, 0, 0, c, 0, e, 0, 0, 0, 0, 1]);
        };
        a.createZRotation = function(c) {
          var e = Math.cos(c);
          c = Math.sin(c);
          return new a([e, c, 0, 0, -c, e, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
        };
        a.createScale = function(c, e, b) {
          return new a([c, 0, 0, 0, 0, e, 0, 0, 0, 0, b, 0, 0, 0, 0, 1]);
        };
        a.createMultiply = function(c, e) {
          var b = c._m, d = e._m, m = b[0], f = b[1], g = b[2], h = b[3], n = b[4], k = b[5], r = b[6], v = b[7], p = b[8], w = b[9], t = b[10], u = b[11], z = b[12], A = b[13], C = b[14], b = b[15], x = d[0], F = d[1], B = d[2], E = d[3], G = d[4], J = d[5], K = d[6], L = d[7], M = d[8], N = d[9], O = d[10], P = d[11], S = d[12], T = d[13], U = d[14], d = d[15];
          return new a([m * x + f * G + g * M + h * S, m * F + f * J + g * N + h * T, m * B + f * K + g * O + h * U, m * E + f * L + g * P + h * d, n * x + k * G + r * M + v * S, n * F + k * J + r * N + v * T, n * B + k * K + r * O + v * U, n * E + k * L + r * P + v * d, p * x + w * G + t * M + u * S, p * F + w * J + t * N + u * T, p * B + w * K + t * O + u * U, p * E + w * L + t * P + u * d, z * x + A * G + C * M + b * S, z * F + A * J + C * N + b * T, z * B + A * K + C * O + b * U, z * E + A * 
          L + C * P + b * d]);
        };
        a.createInverse = function(c) {
          var e = c._m;
          c = e[0];
          var b = e[1], d = e[2], m = e[3], f = e[4], g = e[5], h = e[6], n = e[7], k = e[8], r = e[9], v = e[10], p = e[11], w = e[12], t = e[13], u = e[14], e = e[15], z = v * e, A = u * p, C = h * e, x = u * n, F = h * p, B = v * n, E = d * e, G = u * m, J = d * p, K = v * m, L = d * n, M = h * m, N = k * t, O = w * r, P = f * t, S = w * g, T = f * r, U = k * g, X = c * t, Y = w * b, Z = c * r, $ = k * b, aa = c * g, ba = f * b, da = z * g + x * r + F * t - (A * g + C * r + B * t), ea = A * b + 
          E * r + K * t - (z * b + G * r + J * t), t = C * b + G * g + L * t - (x * b + E * g + M * t), b = B * b + J * g + M * r - (F * b + K * g + L * r), g = 1 / (c * da + f * ea + k * t + w * b);
          return new a([g * da, g * ea, g * t, g * b, g * (A * f + C * k + B * w - (z * f + x * k + F * w)), g * (z * c + G * k + J * w - (A * c + E * k + K * w)), g * (x * c + E * f + M * w - (C * c + G * f + L * w)), g * (F * c + K * f + L * k - (B * c + J * f + M * k)), g * (N * n + S * p + T * e - (O * n + P * p + U * e)), g * (O * m + X * p + $ * e - (N * m + Y * p + Z * e)), g * (P * m + Y * n + aa * e - (S * m + X * n + ba * e)), g * (U * m + Z * n + ba * p - (T * m + $ * n + aa * p)), g * 
          (P * v + U * u + O * h - (T * u + N * h + S * v)), g * (Z * u + N * d + Y * v - (X * v + $ * u + O * d)), g * (X * h + ba * u + S * d - (aa * u + P * d + Y * h)), g * (aa * v + T * d + $ * h - (Z * h + ba * v + U * d))]);
        };
        return a;
      }();
      g.Matrix3D = r;
      r = function() {
        function a(c, e, b) {
          void 0 === b && (b = 7);
          var d = this.size = 1 << b;
          this.sizeInBits = b;
          this.w = c;
          this.h = e;
          this.c = Math.ceil(c / d);
          this.r = Math.ceil(e / d);
          this.grid = [];
          for (c = 0;c < this.r;c++) {
            for (this.grid.push([]), e = 0;e < this.c;e++) {
              this.grid[c][e] = new a.Cell(new v(e * d, c * d, d, d));
            }
          }
        }
        a.prototype.clear = function() {
          for (var a = 0;a < this.r;a++) {
            for (var e = 0;e < this.c;e++) {
              this.grid[a][e].clear();
            }
          }
        };
        a.prototype.getBounds = function() {
          return new v(0, 0, this.w, this.h);
        };
        a.prototype.addDirtyRectangle = function(a) {
          var e = a.x >> this.sizeInBits, b = a.y >> this.sizeInBits;
          if (!(e >= this.c || b >= this.r)) {
            0 > e && (e = 0);
            0 > b && (b = 0);
            var d = this.grid[b][e];
            a = a.clone();
            a.snap();
            if (d.region.contains(a)) {
              d.bounds.isEmpty() ? d.bounds.set(a) : d.bounds.contains(a) || d.bounds.union(a);
            } else {
              for (var m = Math.min(this.c, Math.ceil((a.x + a.w) / this.size)) - e, f = Math.min(this.r, Math.ceil((a.y + a.h) / this.size)) - b, g = 0;g < m;g++) {
                for (var l = 0;l < f;l++) {
                  d = this.grid[b + l][e + g], d = d.region.clone(), d.intersect(a), d.isEmpty() || this.addDirtyRectangle(d);
                }
              }
            }
          }
        };
        a.prototype.gatherRegions = function(a) {
          for (var e = 0;e < this.r;e++) {
            for (var b = 0;b < this.c;b++) {
              this.grid[e][b].bounds.isEmpty() || a.push(this.grid[e][b].bounds);
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
          for (var e = 0, b = 0;b < this.r;b++) {
            for (var d = 0;d < this.c;d++) {
              e += this.grid[b][d].region.area();
            }
          }
          return e / a;
        };
        a.prototype.render = function(a, e) {
          function b(e) {
            a.rect(e.x, e.y, e.w, e.h);
          }
          if (e && e.drawGrid) {
            a.strokeStyle = "white";
            for (var d = 0;d < this.r;d++) {
              for (var m = 0;m < this.c;m++) {
                var f = this.grid[d][m];
                a.beginPath();
                b(f.region);
                a.closePath();
                a.stroke();
              }
            }
          }
          a.strokeStyle = "#E0F8D8";
          for (d = 0;d < this.r;d++) {
            for (m = 0;m < this.c;m++) {
              f = this.grid[d][m], a.beginPath(), b(f.bounds), a.closePath(), a.stroke();
            }
          }
        };
        a.tmpRectangle = v.createEmpty();
        return a;
      }();
      g.DirtyRegion = r;
      (function(a) {
        var c = function() {
          function a(c) {
            this.region = c;
            this.bounds = v.createEmpty();
          }
          a.prototype.clear = function() {
            this.bounds.setEmpty();
          };
          return a;
        }();
        a.Cell = c;
      })(r = g.DirtyRegion || (g.DirtyRegion = {}));
      var w = function() {
        function a(c, e, b, d, m, f) {
          this.index = c;
          this.x = e;
          this.y = b;
          this.scale = f;
          this.bounds = new v(e * d, b * m, d, m);
        }
        a.prototype.getOBB = function() {
          if (this._obb) {
            return this._obb;
          }
          this.bounds.getCorners(a.corners);
          return this._obb = new m(a.corners);
        };
        a.corners = n.createEmptyPoints(4);
        return a;
      }();
      g.Tile = w;
      var t = function() {
        function a(c, e, b, d, m) {
          this.tileW = b;
          this.tileH = d;
          this.scale = m;
          this.w = c;
          this.h = e;
          this.rows = Math.ceil(e / d);
          this.columns = Math.ceil(c / b);
          this.tiles = [];
          for (e = c = 0;e < this.rows;e++) {
            for (var f = 0;f < this.columns;f++) {
              this.tiles.push(new w(c++, f, e, b, d, m));
            }
          }
        }
        a.prototype.getTiles = function(a, e) {
          if (e.emptyArea(a)) {
            return[];
          }
          if (e.infiniteArea(a)) {
            return this.tiles;
          }
          var b = this.columns * this.rows;
          return 40 > b && e.isScaleOrRotation() ? this.getFewTiles(a, e, 10 < b) : this.getManyTiles(a, e);
        };
        a.prototype.getFewTiles = function(c, e, b) {
          void 0 === b && (b = !0);
          if (e.isTranslationOnly() && 1 === this.tiles.length) {
            return this.tiles[0].bounds.intersectsTranslated(c, e.tx, e.ty) ? [this.tiles[0]] : [];
          }
          e.transformRectangle(c, a._points);
          var d;
          c = new v(0, 0, this.w, this.h);
          b && (d = new m(a._points));
          c.intersect(m.getBounds(a._points));
          if (c.isEmpty()) {
            return[];
          }
          var f = c.x / this.tileW | 0;
          e = c.y / this.tileH | 0;
          var g = Math.ceil((c.x + c.w) / this.tileW) | 0, n = Math.ceil((c.y + c.h) / this.tileH) | 0, f = h(f, 0, this.columns), g = h(g, 0, this.columns);
          e = h(e, 0, this.rows);
          for (var n = h(n, 0, this.rows), k = [];f < g;f++) {
            for (var r = e;r < n;r++) {
              var p = this.tiles[r * this.columns + f];
              p.bounds.intersects(c) && (b ? p.getOBB().intersects(d) : 1) && k.push(p);
            }
          }
          return k;
        };
        a.prototype.getManyTiles = function(c, e) {
          function b(a, c, e) {
            return(a - c.x) * (e.y - c.y) / (e.x - c.x) + c.y;
          }
          function d(a, c, e, b, q) {
            if (!(0 > e || e >= c.columns)) {
              for (b = h(b, 0, c.rows), q = h(q + 1, 0, c.rows);b < q;b++) {
                a.push(c.tiles[b * c.columns + e]);
              }
            }
          }
          var m = a._points;
          e.transformRectangle(c, m);
          for (var f = m[0].x < m[1].x ? 0 : 1, g = m[2].x < m[3].x ? 2 : 3, g = m[f].x < m[g].x ? f : g, f = [], n = 0;5 > n;n++, g++) {
            f.push(m[g % 4]);
          }
          (f[1].x - f[0].x) * (f[3].y - f[0].y) < (f[1].y - f[0].y) * (f[3].x - f[0].x) && (m = f[1], f[1] = f[3], f[3] = m);
          var m = [], k, r, g = Math.floor(f[0].x / this.tileW), n = (g + 1) * this.tileW;
          if (f[2].x < n) {
            k = Math.min(f[0].y, f[1].y, f[2].y, f[3].y);
            r = Math.max(f[0].y, f[1].y, f[2].y, f[3].y);
            var v = Math.floor(k / this.tileH), p = Math.floor(r / this.tileH);
            d(m, this, g, v, p);
            return m;
          }
          var w = 0, t = 4, u = !1;
          if (f[0].x === f[1].x || f[0].x === f[3].x) {
            f[0].x === f[1].x ? (u = !0, w++) : t--, k = b(n, f[w], f[w + 1]), r = b(n, f[t], f[t - 1]), v = Math.floor(f[w].y / this.tileH), p = Math.floor(f[t].y / this.tileH), d(m, this, g, v, p), g++;
          }
          do {
            var D, z, A, C;
            f[w + 1].x < n ? (D = f[w + 1].y, A = !0) : (D = b(n, f[w], f[w + 1]), A = !1);
            f[t - 1].x < n ? (z = f[t - 1].y, C = !0) : (z = b(n, f[t], f[t - 1]), C = !1);
            v = Math.floor((f[w].y < f[w + 1].y ? k : D) / this.tileH);
            p = Math.floor((f[t].y > f[t - 1].y ? r : z) / this.tileH);
            d(m, this, g, v, p);
            if (A && u) {
              break;
            }
            A ? (u = !0, w++, k = b(n, f[w], f[w + 1])) : k = D;
            C ? (t--, r = b(n, f[t], f[t - 1])) : r = z;
            g++;
            n = (g + 1) * this.tileW;
          } while (w < t);
          return m;
        };
        a._points = n.createEmptyPoints(4);
        return a;
      }();
      g.TileCache = t;
      r = function() {
        function b(a, e, d) {
          this._cacheLevels = [];
          this._source = a;
          this._tileSize = e;
          this._minUntiledSize = d;
        }
        b.prototype._getTilesAtScale = function(c, e, b) {
          var d = Math.max(e.getAbsoluteScaleX(), e.getAbsoluteScaleY()), f = 0;
          1 !== d && (f = h(Math.round(Math.log(1 / d) / Math.LN2), -5, 3));
          d = a(f);
          if (this._source.hasFlags(1048576)) {
            for (;;) {
              d = a(f);
              if (b.contains(this._source.getBounds().getAbsoluteBounds().clone().scale(d, d))) {
                break;
              }
              f--;
            }
          }
          this._source.hasFlags(2097152) || (f = h(f, -5, 0));
          d = a(f);
          b = 5 + f;
          f = this._cacheLevels[b];
          if (!f) {
            var f = this._source.getBounds().getAbsoluteBounds().clone().scale(d, d), m, g;
            this._source.hasFlags(1048576) || !this._source.hasFlags(4194304) || Math.max(f.w, f.h) <= this._minUntiledSize ? (m = f.w, g = f.h) : m = g = this._tileSize;
            f = this._cacheLevels[b] = new t(f.w, f.h, m, g, d);
          }
          return f.getTiles(c, e.scaleClone(d, d));
        };
        b.prototype.fetchTiles = function(a, e, b, d) {
          var f = new v(0, 0, b.canvas.width, b.canvas.height);
          a = this._getTilesAtScale(a, e, f);
          var m;
          e = this._source;
          for (var g = 0;g < a.length;g++) {
            var l = a[g];
            l.cachedSurfaceRegion && l.cachedSurfaceRegion.surface && !e.hasFlags(1048592) || (m || (m = []), m.push(l));
          }
          m && this._cacheTiles(b, m, d, f);
          e.removeFlags(16);
          return a;
        };
        b.prototype._getTileBounds = function(a) {
          for (var e = v.createEmpty(), b = 0;b < a.length;b++) {
            e.union(a[b].bounds);
          }
          return e;
        };
        b.prototype._cacheTiles = function(a, e, b, d, f) {
          void 0 === f && (f = 4);
          var m = this._getTileBounds(e);
          a.save();
          a.setTransform(1, 0, 0, 1, 0, 0);
          a.clearRect(0, 0, d.w, d.h);
          a.translate(-m.x, -m.y);
          a.scale(e[0].scale, e[0].scale);
          var g = this._source.getBounds();
          a.translate(-g.x, -g.y);
          2 <= p.traceLevel && p.writer && p.writer.writeLn("Rendering Tiles: " + m);
          this._source.render(a, 0);
          a.restore();
          for (var g = null, l = 0;l < e.length;l++) {
            var h = e[l], n = h.bounds.clone();
            n.x -= m.x;
            n.y -= m.y;
            d.contains(n) || (g || (g = []), g.push(h));
            h.cachedSurfaceRegion = b(h.cachedSurfaceRegion, a, n);
          }
          g && (2 <= g.length ? (e = g.slice(0, g.length / 2 | 0), m = g.slice(e.length), this._cacheTiles(a, e, b, d, f - 1), this._cacheTiles(a, m, b, d, f - 1)) : this._cacheTiles(a, g, b, d, f - 1));
        };
        return b;
      }();
      g.RenderableTileCache = r;
    })(p.Geometry || (p.Geometry = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, p) {
  function g() {
    this.constructor = k;
  }
  for (var b in p) {
    p.hasOwnProperty(b) && (k[b] = p[b]);
  }
  g.prototype = p.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(p) {
    var g = k.IntegerUtilities.roundToMultipleOfPowerOfTwo, b = p.Geometry.Rectangle;
    (function(k) {
      var h = function(a) {
        function b() {
          a.apply(this, arguments);
        }
        __extends(b, a);
        return b;
      }(p.Geometry.Rectangle);
      k.Region = h;
      var a = function() {
        function a(b, f) {
          this._root = new d(0, 0, b | 0, f | 0, !1);
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          var d = this._root.insert(a, b);
          d && (d.allocator = this, d.allocated = !0);
          return d;
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
      var d = function(b) {
        function d(a, f, c, e, q) {
          b.call(this, a, f, c, e);
          this._children = null;
          this._horizontal = q;
          this.allocated = !1;
        }
        __extends(d, b);
        d.prototype.clear = function() {
          this._children = null;
          this.allocated = !1;
        };
        d.prototype.insert = function(a, b) {
          return this._insert(a, b, 0);
        };
        d.prototype._insert = function(b, f, c) {
          if (!(c > a.MAX_DEPTH || this.allocated || this.w < b || this.h < f)) {
            if (this._children) {
              var e;
              if ((e = this._children[0]._insert(b, f, c + 1)) || (e = this._children[1]._insert(b, f, c + 1))) {
                return e;
              }
            } else {
              return e = !this._horizontal, a.RANDOM_ORIENTATION && (e = .5 <= Math.random()), this._children = this._horizontal ? [new d(this.x, this.y, this.w, f, !1), new d(this.x, this.y + f, this.w, this.h - f, e)] : [new d(this.x, this.y, b, this.h, !0), new d(this.x + b, this.y, this.w - b, this.h, e)], e = this._children[0], e.w === b && e.h === f ? (e.allocated = !0, e) : this._insert(b, f, c + 1);
            }
          }
        };
        return d;
      }(k.Region), n = function() {
        function a(b, d, f, c) {
          this._columns = b / f | 0;
          this._rows = d / c | 0;
          this._sizeW = f;
          this._sizeH = c;
          this._freeList = [];
          this._index = 0;
          this._total = this._columns * this._rows;
        }
        a.prototype.allocate = function(a, b) {
          a = Math.ceil(a);
          b = Math.ceil(b);
          var d = this._sizeW, c = this._sizeH;
          if (a > d || b > c) {
            return null;
          }
          var e = this._freeList, q = this._index;
          return 0 < e.length ? (d = e.pop(), d.w = a, d.h = b, d.allocated = !0, d) : q < this._total ? (e = q / this._columns | 0, d = new f((q - e * this._columns) * d, e * c, a, b), d.index = q, d.allocator = this, d.allocated = !0, this._index++, d) : null;
        };
        a.prototype.free = function(a) {
          a.allocated = !1;
          this._freeList.push(a);
        };
        return a;
      }();
      k.GridAllocator = n;
      var f = function(a) {
        function b(d, f, c, e) {
          a.call(this, d, f, c, e);
          this.index = -1;
        }
        __extends(b, a);
        return b;
      }(k.Region);
      k.GridCell = f;
      var v = function() {
        return function(a, b, d) {
          this.size = a;
          this.region = b;
          this.allocator = d;
        };
      }(), m = function(a) {
        function b(d, f, c, e, q) {
          a.call(this, d, f, c, e);
          this.region = q;
        }
        __extends(b, a);
        return b;
      }(k.Region);
      k.BucketCell = m;
      h = function() {
        function a(b, d) {
          this._buckets = [];
          this._w = b | 0;
          this._h = d | 0;
          this._filled = 0;
        }
        a.prototype.allocate = function(a, d) {
          a = Math.ceil(a);
          d = Math.ceil(d);
          var f = Math.max(a, d);
          if (a > this._w || d > this._h) {
            return null;
          }
          var c = null, e, q = this._buckets;
          do {
            for (var s = 0;s < q.length && !(q[s].size >= f && (e = q[s], c = e.allocator.allocate(a, d)));s++) {
            }
            if (!c) {
              var h = this._h - this._filled;
              if (h < d) {
                return null;
              }
              var s = g(f, 8), k = 2 * s;
              k > h && (k = h);
              if (k < s) {
                return null;
              }
              h = new b(0, this._filled, this._w, k);
              this._buckets.push(new v(s, h, new n(h.w, h.h, s, s)));
              this._filled += k;
            }
          } while (!c);
          return new m(e.region.x + c.x, e.region.y + c.y, c.w, c.h, c);
        };
        a.prototype.free = function(a) {
          a.region.allocator.free(a.region);
        };
        return a;
      }();
      k.BucketAllocator = h;
    })(p.RegionAllocator || (p.RegionAllocator = {}));
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
          var f = this._createSurface(a, b);
          this._surfaces.push(f);
          return f;
        };
        a.prototype.addSurface = function(a) {
          this._surfaces.push(a);
        };
        a.prototype.allocate = function(a, b, f) {
          for (var g = 0;g < this._surfaces.length;g++) {
            var m = this._surfaces[g];
            if (m !== f && (m = m.allocate(a, b))) {
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
    })(p.SurfaceRegionAllocator || (p.SurfaceRegionAllocator = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var g = p.Geometry.Rectangle, b = p.Geometry.Matrix, u = p.Geometry.DirtyRegion;
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
    var h = p.BlendMode;
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
    var a = p.NodeFlags;
    (function(a) {
      a[a.Node = 1] = "Node";
      a[a.Shape = 3] = "Shape";
      a[a.Group = 5] = "Group";
      a[a.Stage = 13] = "Stage";
      a[a.Renderable = 33] = "Renderable";
    })(p.NodeType || (p.NodeType = {}));
    var d = p.NodeType;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.OnStageBoundsChanged = 1] = "OnStageBoundsChanged";
      a[a.RemovedFromStage = 2] = "RemovedFromStage";
    })(p.NodeEventType || (p.NodeEventType = {}));
    var n = function() {
      function a() {
      }
      a.prototype.visitNode = function(a, b) {
      };
      a.prototype.visitShape = function(a, b) {
        this.visitNode(a, b);
      };
      a.prototype.visitGroup = function(a, b) {
        this.visitNode(a, b);
        for (var c = a.getChildren(), d = 0;d < c.length;d++) {
          c[d].visit(this, b);
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
    p.NodeVisitor = n;
    var f = function() {
      return function() {
      };
    }();
    p.State = f;
    var v = function(a) {
      function e() {
        a.call(this);
        this.matrix = b.createIdentity();
        this.depth = 0;
      }
      __extends(e, a);
      e.prototype.transform = function(a) {
        var e = this.clone();
        e.matrix.preMultiply(a.getMatrix());
        return e;
      };
      e.allocate = function() {
        var a = e._dirtyStack, b = null;
        a.length && (b = a.pop());
        return b;
      };
      e.prototype.clone = function() {
        var a = e.allocate();
        a || (a = new e);
        a.set(this);
        return a;
      };
      e.prototype.set = function(a) {
        this.matrix.set(a.matrix);
      };
      e.prototype.free = function() {
        e._dirtyStack.push(this);
      };
      e._dirtyStack = [];
      return e;
    }(f);
    p.PreRenderState = v;
    var m = function(a) {
      function e() {
        a.apply(this, arguments);
        this.isDirty = !0;
      }
      __extends(e, a);
      e.prototype.start = function(a, e) {
        this._dirtyRegion = e;
        var b = new v;
        b.matrix.setIdentity();
        a.visit(this, b);
        b.free();
      };
      e.prototype.visitGroup = function(a, e) {
        var b = a.getChildren();
        this.visitNode(a, e);
        for (var c = 0;c < b.length;c++) {
          var d = b[c], f = e.transform(d.getTransform());
          d.visit(this, f);
          f.free();
        }
      };
      e.prototype.visitNode = function(a, e) {
        a.hasFlags(16) && (this.isDirty = !0);
        a.toggleFlags(16, !1);
        a.depth = e.depth++;
      };
      return e;
    }(n);
    p.PreRenderVisitor = m;
    f = function(a) {
      function e(e) {
        a.call(this);
        this.writer = e;
      }
      __extends(e, a);
      e.prototype.visitNode = function(a, e) {
      };
      e.prototype.visitShape = function(a, e) {
        this.writer.writeLn(a.toString());
        this.visitNode(a, e);
      };
      e.prototype.visitGroup = function(a, e) {
        this.visitNode(a, e);
        var b = a.getChildren();
        this.writer.enter(a.toString() + " " + b.length);
        for (var c = 0;c < b.length;c++) {
          b[c].visit(this, e);
        }
        this.writer.outdent();
      };
      e.prototype.visitStage = function(a, e) {
        this.visitGroup(a, e);
      };
      return e;
    }(n);
    p.TracingNodeVisitor = f;
    var r = function() {
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
            var d = b[c];
            d.type === a && d.listener(this, a);
          }
        }
      };
      b.prototype.addEventListener = function(a, b) {
        this._eventListeners || (this._eventListeners = []);
        this._eventListeners.push({type:a, listener:b});
      };
      b.prototype.removeEventListener = function(a, b) {
        for (var c = this._eventListeners, d = 0;d < c.length;d++) {
          var f = c[d];
          if (f.type === a && f.listener === b) {
            c.splice(d, 1);
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
        throw void 0;
      };
      b.prototype.getBounds = function(a) {
        throw void 0;
      };
      b.prototype.setBounds = function(a) {
        (this._bounds || (this._bounds = g.createEmpty())).set(a);
        this.removeFlags(256);
      };
      b.prototype.clone = function() {
        throw void 0;
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
        throw void 0;
      };
      b.prototype.isAncestor = function(a) {
        for (;a;) {
          if (a === this) {
            return!0;
          }
          a = a._parent;
        }
        return!1;
      };
      b._getAncestors = function(a, d) {
        var f = b._path;
        for (f.length = 0;a && a !== d;) {
          f.push(a), a = a._parent;
        }
        return f;
      };
      b.prototype._findClosestAncestor = function() {
        for (var a = this;a;) {
          if (!1 === a.hasFlags(512)) {
            return a;
          }
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
        null === this._transform && (this._transform = new t(this));
        return this._transform;
      };
      b.prototype.getLayer = function() {
        null === this._layer && (this._layer = new l(this));
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
            k.Debug.unexpectedCase();
        }
      };
      b.prototype.invalidate = function() {
        this._propagateFlagsUp(a.UpOnInvalidate);
      };
      b.prototype.toString = function(a) {
        void 0 === a && (a = !1);
        var b = d[this._type] + " " + this._id;
        a && (b += " " + this._bounds.toString());
        return b;
      };
      b._path = [];
      b._nextId = 0;
      return b;
    }();
    p.Node = r;
    var w = function(b) {
      function e() {
        b.call(this);
        this._type = 5;
        this._children = [];
      }
      __extends(e, b);
      e.prototype.getChildren = function(a) {
        void 0 === a && (a = !1);
        return a ? this._children.slice(0) : this._children;
      };
      e.prototype.childAt = function(a) {
        return this._children[a];
      };
      Object.defineProperty(e.prototype, "child", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(e.prototype, "groupChild", {get:function() {
        return this._children[0];
      }, enumerable:!0, configurable:!0});
      e.prototype.addChild = function(b) {
        b._parent && b._parent.removeChildAt(b._index);
        b._parent = this;
        b._index = this._children.length;
        this._children.push(b);
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
        b._propagateFlagsDown(a.DownOnAddedOrRemoved);
      };
      e.prototype.removeChildAt = function(b) {
        var e = this._children[b];
        this._children.splice(b, 1);
        e._index = -1;
        e._parent = null;
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
        e._propagateFlagsDown(a.DownOnAddedOrRemoved);
      };
      e.prototype.clearChildren = function() {
        for (var b = 0;b < this._children.length;b++) {
          var e = this._children[b];
          e && (e._index = -1, e._parent = null, e._propagateFlagsDown(a.DownOnAddedOrRemoved));
        }
        this._children.length = 0;
        this._propagateFlagsUp(a.UpOnAddedOrRemoved);
      };
      e.prototype._propagateFlagsDown = function(a) {
        if (!this.hasFlags(a)) {
          this.setFlags(a);
          for (var b = this._children, e = 0;e < b.length;e++) {
            b[e]._propagateFlagsDown(a);
          }
        }
      };
      e.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var b = this._bounds || (this._bounds = g.createEmpty());
        if (this.hasFlags(256)) {
          b.setEmpty();
          for (var e = this._children, c = g.allocate(), d = 0;d < e.length;d++) {
            var f = e[d];
            c.set(f.getBounds());
            f.getTransformMatrix().transformRectangleAABB(c);
            b.union(c);
          }
          c.free();
          this.removeFlags(256);
        }
        return a ? b.clone() : b;
      };
      return e;
    }(r);
    p.Group = w;
    var t = function() {
      function c(a) {
        this._node = a;
        this._matrix = b.createIdentity();
        this._colorMatrix = p.ColorMatrix.createIdentity();
        this._concatenatedMatrix = b.createIdentity();
        this._invertedConcatenatedMatrix = b.createIdentity();
        this._concatenatedColorMatrix = p.ColorMatrix.createIdentity();
      }
      c.prototype.setMatrix = function(b) {
        this._matrix.isEqual(b) || (this._matrix.set(b), this._node._propagateFlagsUp(a.UpOnMoved), this._node._propagateFlagsDown(a.DownOnMoved));
      };
      c.prototype.setColorMatrix = function(b) {
        this._colorMatrix.set(b);
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
        null === this._colorMatrix && (this._colorMatrix = p.ColorMatrix.createIdentity());
        return a ? this._colorMatrix.clone() : this._colorMatrix;
      };
      c.prototype.getConcatenatedMatrix = function(a) {
        void 0 === a && (a = !1);
        if (this._node.hasFlags(512)) {
          for (var c = this._node._findClosestAncestor(), d = r._getAncestors(this._node, c), f = c ? c.getTransform()._concatenatedMatrix.clone() : b.createIdentity(), m = d.length - 1;0 <= m;m--) {
            var c = d[m], g = c.getTransform();
            f.preMultiply(g._matrix);
            g._concatenatedMatrix.set(f);
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
    p.Transform = t;
    var l = function() {
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
        return h[this._blendMode];
      };
      return a;
    }();
    p.Layer = l;
    f = function(a) {
      function b(e) {
        a.call(this);
        this._source = e;
        this._type = 3;
        this.ratio = 0;
      }
      __extends(b, a);
      b.prototype.getBounds = function(a) {
        void 0 === a && (a = !1);
        var b = this._bounds || (this._bounds = g.createEmpty());
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
    }(r);
    p.Shape = f;
    p.RendererOptions = function() {
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
    })(p.Backend || (p.Backend = {}));
    n = function(a) {
      function b(e, d, f) {
        a.call(this);
        this._container = e;
        this._stage = d;
        this._options = f;
        this._viewport = g.createSquare();
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
      b.prototype.screenShot = function(a, b) {
        throw void 0;
      };
      return b;
    }(n);
    p.Renderer = n;
    n = function(a) {
      function e(b, d, f) {
        void 0 === f && (f = !1);
        a.call(this);
        this._preVisitor = new m;
        this._flags &= -3;
        this._type = 13;
        this._scaleMode = e.DEFAULT_SCALE;
        this._align = e.DEFAULT_ALIGN;
        this._content = new w;
        this._content._flags &= -3;
        this.addChild(this._content);
        this.setFlags(16);
        this.setBounds(new g(0, 0, b, d));
        f ? (this._dirtyRegion = new u(b, d), this._dirtyRegion.addDirtyRectangle(new g(0, 0, b, d))) : this._dirtyRegion = null;
        this._updateContentMatrix();
      }
      __extends(e, a);
      Object.defineProperty(e.prototype, "dirtyRegion", {get:function() {
        return this._dirtyRegion;
      }, enumerable:!0, configurable:!0});
      e.prototype.setBounds = function(b) {
        a.prototype.setBounds.call(this, b);
        this._updateContentMatrix();
        this._dispatchEvent(1);
        this._dirtyRegion && (this._dirtyRegion = new u(b.w, b.h), this._dirtyRegion.addDirtyRectangle(b));
      };
      Object.defineProperty(e.prototype, "content", {get:function() {
        return this._content;
      }, enumerable:!0, configurable:!0});
      e.prototype.readyToRender = function() {
        this._preVisitor.isDirty = !1;
        this._preVisitor.start(this, this._dirtyRegion);
        return this._preVisitor.isDirty ? !0 : !1;
      };
      Object.defineProperty(e.prototype, "align", {get:function() {
        return this._align;
      }, set:function(a) {
        this._align = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(e.prototype, "scaleMode", {get:function() {
        return this._scaleMode;
      }, set:function(a) {
        this._scaleMode = a;
        this._updateContentMatrix();
      }, enumerable:!0, configurable:!0});
      e.prototype._updateContentMatrix = function() {
        if (this._scaleMode === e.DEFAULT_SCALE && this._align === e.DEFAULT_ALIGN) {
          this._content.getTransform().setMatrix(new b(1, 0, 0, 1, 0, 0));
        } else {
          var a = this.getBounds(), c = this._content.getBounds(), d = a.w / c.w, f = a.h / c.h;
          switch(this._scaleMode) {
            case 2:
              d = f = Math.max(d, f);
              break;
            case 4:
              d = f = 1;
              break;
            case 1:
              break;
            default:
              d = f = Math.min(d, f);
          }
          var m;
          m = this._align & 4 ? 0 : this._align & 8 ? a.w - c.w * d : (a.w - c.w * d) / 2;
          a = this._align & 1 ? 0 : this._align & 2 ? a.h - c.h * f : (a.h - c.h * f) / 2;
          this._content.getTransform().setMatrix(new b(d, 0, 0, f, m, a));
        }
      };
      e.DEFAULT_SCALE = 4;
      e.DEFAULT_ALIGN = 5;
      return e;
    }(w);
    p.Stage = n;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    function g(a, b, e) {
      return a + (b - a) * e;
    }
    function b(a, b, e) {
      return g(a >> 24 & 255, b >> 24 & 255, e) << 24 | g(a >> 16 & 255, b >> 16 & 255, e) << 16 | g(a >> 8 & 255, b >> 8 & 255, e) << 8 | g(a & 255, b & 255, e);
    }
    var u = p.Geometry.Point, h = p.Geometry.Rectangle, a = p.Geometry.Matrix, d = k.ArrayUtilities.indexOf, n = function(a) {
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
        d(this._parents, a);
        this._parents.push(a);
      };
      b.prototype.willRender = function() {
        for (var a = this._parents, b = 0;b < a.length;b++) {
          for (var c = a[b];c;) {
            if (c.isType(13)) {
              return!0;
            }
            if (!c.hasFlags(65536)) {
              break;
            }
            c = c._parent;
          }
        }
        return!1;
      };
      b.prototype.addRenderableParent = function(a) {
        d(this._renderableParents, a);
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
        d(this._invalidateEventListeners, a);
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
      b.prototype.render = function(a, b, c, d, f) {
      };
      return b;
    }(p.Node);
    p.Renderable = n;
    var f = function(a) {
      function b(e, c) {
        a.call(this);
        this.setBounds(e);
        this.render = c;
      }
      __extends(b, a);
      return b;
    }(n);
    p.CustomRenderable = f;
    (function(a) {
      a[a.Idle = 1] = "Idle";
      a[a.Playing = 2] = "Playing";
      a[a.Paused = 3] = "Paused";
      a[a.Ended = 4] = "Ended";
    })(p.RenderableVideoState || (p.RenderableVideoState = {}));
    f = function(a) {
      function b(e, d) {
        a.call(this);
        this._flags = 1048592;
        this._lastPausedTime = this._lastTimeInvalidated = 0;
        this._pauseHappening = this._seekHappening = !1;
        this._isDOMElement = !0;
        this.setBounds(new h(0, 0, 1, 1));
        this._assetId = e;
        this._eventSerializer = d;
        var f = document.createElement("video"), m = this._handleVideoEvent.bind(this);
        f.preload = "metadata";
        f.addEventListener("play", m);
        f.addEventListener("pause", m);
        f.addEventListener("ended", m);
        f.addEventListener("loadeddata", m);
        f.addEventListener("progress", m);
        f.addEventListener("suspend", m);
        f.addEventListener("loadedmetadata", m);
        f.addEventListener("error", m);
        f.addEventListener("seeking", m);
        f.addEventListener("seeked", m);
        f.addEventListener("canplay", m);
        f.style.position = "absolute";
        this._video = f;
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
      b.prototype._handleVideoEvent = function(a) {
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
      b.prototype._notifyNetStream = function(a, b) {
        this._eventSerializer.sendVideoPlaybackEvent(this._assetId, a, b);
      };
      b.prototype.processControlRequest = function(a, b) {
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
              for (var f = 0;f < c.buffered.length;f++) {
                d = Math.max(d, c.buffered.end(f));
              }
            } else {
              d = c.duration;
            }
            return Math.round(500 * d);
          case 8:
            return c ? Math.round(500 * c.duration) : 0;
        }
      };
      b.prototype.checkForUpdate = function() {
        this._lastTimeInvalidated !== this._video.currentTime && (this._isDOMElement || this.invalidate());
        this._lastTimeInvalidated = this._video.currentTime;
      };
      b.checkForVideoUpdates = function() {
        for (var a = b._renderableVideos, d = 0;d < a.length;d++) {
          var f = a[d];
          f.willRender() ? (f._video.parentElement || f.invalidate(), f._video.style.zIndex = f.parents[0].depth + "") : f._video.parentElement && f._dispatchEvent(2);
          a[d].checkForUpdate();
        }
      };
      b.prototype.render = function(a, b, c) {
        (b = this._video) && 0 < b.videoWidth && a.drawImage(b, 0, 0, b.videoWidth, b.videoHeight, 0, 0, this._bounds.w, this._bounds.h);
      };
      b._renderableVideos = [];
      return b;
    }(n);
    p.RenderableVideo = f;
    f = function(a) {
      function b(e, c) {
        a.call(this);
        this._flags = 1048592;
        this.properties = {};
        this.setBounds(c);
        e instanceof HTMLCanvasElement ? this._initializeSourceCanvas(e) : this._sourceImage = e;
      }
      __extends(b, a);
      b.FromDataBuffer = function(a, d, f) {
        var m = document.createElement("canvas");
        m.width = f.w;
        m.height = f.h;
        f = new b(m, f);
        f.updateFromDataBuffer(a, d);
        return f;
      };
      b.FromNode = function(a, d, f, m, g) {
        var h = document.createElement("canvas"), n = a.getBounds();
        h.width = n.w;
        h.height = n.h;
        h = new b(h, n);
        h.drawNode(a, d, f, m, g);
        return h;
      };
      b.FromImage = function(a) {
        return new b(a, new h(0, 0, -1, -1));
      };
      b.prototype.updateFromDataBuffer = function(a, b) {
        if (p.imageUpdateOption.value) {
          var c = b.buffer;
          if (4 !== a && 5 !== a && 6 !== a) {
            var d = this._bounds, f = this._imageData;
            f && f.width === d.w && f.height === d.h || (f = this._imageData = this._context.createImageData(d.w, d.h));
            p.imageConvertOption.value && (c = new Int32Array(c), d = new Int32Array(f.data.buffer), k.ColorUtilities.convertImage(a, 3, c, d));
            this._ensureSourceCanvas();
            this._context.putImageData(f, 0, 0);
          }
          this.invalidate();
        }
      };
      b.prototype.readImageData = function(a) {
        a.writeRawBytes(this.imageData.data);
      };
      b.prototype.render = function(a, b, c) {
        this.renderSource ? a.drawImage(this.renderSource, 0, 0) : this._renderFallback(a);
      };
      b.prototype.drawNode = function(a, b, c, d, f) {
        c = p.Canvas2D;
        d = this.getBounds();
        (new c.Canvas2DRenderer(this._canvas, null)).renderNode(a, f || d, b);
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
        this._canvas || (this._ensureSourceCanvas(), this._context.drawImage(this._sourceImage, 0, 0), this._sourceImage = null);
        return this._context.getImageData(0, 0, this._bounds.w, this._bounds.h);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(b.prototype, "renderSource", {get:function() {
        return this._canvas || this._sourceImage;
      }, enumerable:!0, configurable:!0});
      b.prototype._renderFallback = function(a) {
      };
      return b;
    }(n);
    p.RenderableBitmap = f;
    (function(a) {
      a[a.Fill = 0] = "Fill";
      a[a.Stroke = 1] = "Stroke";
      a[a.StrokeFill = 2] = "StrokeFill";
    })(p.PathType || (p.PathType = {}));
    var v = function() {
      return function(a, b, e, d) {
        this.type = a;
        this.style = b;
        this.smoothImage = e;
        this.strokeProperties = d;
        this.path = new Path2D;
      };
    }();
    p.StyledPath = v;
    var m = function() {
      return function(a, b, e, d, f) {
        this.thickness = a;
        this.scaleMode = b;
        this.capsStyle = e;
        this.jointsStyle = d;
        this.miterLimit = f;
      };
    }();
    p.StrokeProperties = m;
    var r = function(b) {
      function c(a, c, d, f) {
        b.call(this);
        this._flags = 6291472;
        this.properties = {};
        this.setBounds(f);
        this._id = a;
        this._pathData = c;
        this._textures = d;
        d.length && this.setFlags(1048576);
      }
      __extends(c, b);
      c.prototype.update = function(a, b, c) {
        this.setBounds(c);
        this._pathData = a;
        this._paths = null;
        this._textures = b;
        this.setFlags(1048576);
        this.invalidate();
      };
      c.prototype.render = function(a, b, c, d, f) {
        void 0 === d && (d = !1);
        void 0 === f && (f = !1);
        a.fillStyle = a.strokeStyle = "transparent";
        b = this._deserializePaths(this._pathData, a, b);
        for (c = 0;c < b.length;c++) {
          var m = b[c];
          a.mozImageSmoothingEnabled = a.msImageSmoothingEnabled = a.imageSmoothingEnabled = m.smoothImage;
          if (0 === m.type) {
            d ? a.clip(m.path, "evenodd") : (a.fillStyle = f ? "#000000" : m.style, a.fill(m.path, "evenodd"), a.fillStyle = "transparent");
          } else {
            if (!d && !f) {
              a.strokeStyle = m.style;
              var g = 1;
              m.strokeProperties && (g = m.strokeProperties.scaleMode, a.lineWidth = m.strokeProperties.thickness, a.lineCap = m.strokeProperties.capsStyle, a.lineJoin = m.strokeProperties.jointsStyle, a.miterLimit = m.strokeProperties.miterLimit);
              var h = a.lineWidth;
              (h = 1 === h || 3 === h) && a.translate(.5, .5);
              a.flashStroke(m.path, g);
              h && a.translate(-.5, -.5);
              a.strokeStyle = "transparent";
            }
          }
        }
      };
      c.prototype._deserializePaths = function(a, b, d) {
        if (this._paths) {
          return this._paths;
        }
        d = this._paths = [];
        var f = null, g = null, h = 0, n = 0, l, r, v = !1, p = 0, w = 0, t = a.commands, u = a.coordinates, D = a.styles, z = D.position = 0;
        a = a.commandsPosition;
        for (var A = 0;A < a;A++) {
          switch(t[A]) {
            case 9:
              v && f && (f.lineTo(p, w), g && g.lineTo(p, w));
              v = !0;
              h = p = u[z++] / 20;
              n = w = u[z++] / 20;
              f && f.moveTo(h, n);
              g && g.moveTo(h, n);
              break;
            case 10:
              h = u[z++] / 20;
              n = u[z++] / 20;
              f && f.lineTo(h, n);
              g && g.lineTo(h, n);
              break;
            case 11:
              l = u[z++] / 20;
              r = u[z++] / 20;
              h = u[z++] / 20;
              n = u[z++] / 20;
              f && f.quadraticCurveTo(l, r, h, n);
              g && g.quadraticCurveTo(l, r, h, n);
              break;
            case 12:
              l = u[z++] / 20;
              r = u[z++] / 20;
              var C = u[z++] / 20, x = u[z++] / 20, h = u[z++] / 20, n = u[z++] / 20;
              f && f.bezierCurveTo(l, r, C, x, h, n);
              g && g.bezierCurveTo(l, r, C, x, h, n);
              break;
            case 1:
              f = this._createPath(0, k.ColorUtilities.rgbaToCSSStyle(D.readUnsignedInt()), !1, null, h, n);
              break;
            case 3:
              l = this._readBitmap(D, b);
              f = this._createPath(0, l.style, l.smoothImage, null, h, n);
              break;
            case 2:
              f = this._createPath(0, this._readGradient(D, b), !1, null, h, n);
              break;
            case 4:
              f = null;
              break;
            case 5:
              g = k.ColorUtilities.rgbaToCSSStyle(D.readUnsignedInt());
              D.position += 1;
              l = D.readByte();
              r = c.LINE_CAPS_STYLES[D.readByte()];
              C = c.LINE_JOINTS_STYLES[D.readByte()];
              l = new m(u[z++] / 20, l, r, C, D.readByte());
              g = this._createPath(1, g, !1, l, h, n);
              break;
            case 6:
              g = this._createPath(2, this._readGradient(D, b), !1, null, h, n);
              break;
            case 7:
              l = this._readBitmap(D, b);
              g = this._createPath(2, l.style, l.smoothImage, null, h, n);
              break;
            case 8:
              g = null;
          }
        }
        v && f && (f.lineTo(p, w), g && g.lineTo(p, w));
        this._pathData = null;
        return d;
      };
      c.prototype._createPath = function(a, b, c, d, f, m) {
        a = new v(a, b, c, d);
        this._paths.push(a);
        a.path.moveTo(f, m);
        return a.path;
      };
      c.prototype._readMatrix = function(b) {
        return new a(b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat(), b.readFloat());
      };
      c.prototype._readGradient = function(a, b) {
        var c = a.readUnsignedByte(), d = 2 * a.readShort() / 255, f = this._readMatrix(a), c = 16 === c ? b.createLinearGradient(-1, 0, 1, 0) : b.createRadialGradient(d, 0, 0, 0, 0, 1);
        c.setTransform && c.setTransform(f.toSVGMatrix());
        f = a.readUnsignedByte();
        for (d = 0;d < f;d++) {
          var m = a.readUnsignedByte() / 255, g = k.ColorUtilities.rgbaToCSSStyle(a.readUnsignedInt());
          c.addColorStop(m, g);
        }
        a.position += 2;
        return c;
      };
      c.prototype._readBitmap = function(a, b) {
        var c = a.readUnsignedInt(), d = this._readMatrix(a), f = a.readBoolean() ? "repeat" : "no-repeat", m = a.readBoolean();
        (c = this._textures[c]) ? (f = b.createPattern(c.renderSource, f), f.setTransform(d.toSVGMatrix())) : f = null;
        return{style:f, smoothImage:m};
      };
      c.prototype._renderFallback = function(a) {
        this.fillStyle || (this.fillStyle = k.ColorStyle.randomStyle());
        var b = this._bounds;
        a.save();
        a.beginPath();
        a.lineWidth = 2;
        a.fillStyle = this.fillStyle;
        a.fillRect(b.x, b.y, b.w, b.h);
        a.restore();
      };
      c.LINE_CAPS_STYLES = ["round", "butt", "square"];
      c.LINE_JOINTS_STYLES = ["round", "bevel", "miter"];
      return c;
    }(n);
    p.RenderableShape = r;
    f = function(d) {
      function c() {
        d.apply(this, arguments);
        this._flags = 7340048;
        this._morphPaths = Object.create(null);
      }
      __extends(c, d);
      c.prototype._deserializePaths = function(a, c, d) {
        if (this._morphPaths[d]) {
          return this._morphPaths[d];
        }
        var f = this._morphPaths[d] = [], h = null, n = null, l = 0, v = 0, p, w, t = !1, u = 0, Q = 0, H = a.commands, D = a.coordinates, z = a.morphCoordinates, A = a.styles, C = a.morphStyles;
        A.position = 0;
        var x = C.position = 0;
        a = a.commandsPosition;
        for (var F = 0;F < a;F++) {
          switch(H[F]) {
            case 9:
              t && h && (h.lineTo(u, Q), n && n.lineTo(u, Q));
              t = !0;
              l = u = g(D[x], z[x++], d) / 20;
              v = Q = g(D[x], z[x++], d) / 20;
              h && h.moveTo(l, v);
              n && n.moveTo(l, v);
              break;
            case 10:
              l = g(D[x], z[x++], d) / 20;
              v = g(D[x], z[x++], d) / 20;
              h && h.lineTo(l, v);
              n && n.lineTo(l, v);
              break;
            case 11:
              p = g(D[x], z[x++], d) / 20;
              w = g(D[x], z[x++], d) / 20;
              l = g(D[x], z[x++], d) / 20;
              v = g(D[x], z[x++], d) / 20;
              h && h.quadraticCurveTo(p, w, l, v);
              n && n.quadraticCurveTo(p, w, l, v);
              break;
            case 12:
              p = g(D[x], z[x++], d) / 20;
              w = g(D[x], z[x++], d) / 20;
              var B = g(D[x], z[x++], d) / 20, E = g(D[x], z[x++], d) / 20, l = g(D[x], z[x++], d) / 20, v = g(D[x], z[x++], d) / 20;
              h && h.bezierCurveTo(p, w, B, E, l, v);
              n && n.bezierCurveTo(p, w, B, E, l, v);
              break;
            case 1:
              h = this._createMorphPath(0, d, k.ColorUtilities.rgbaToCSSStyle(b(A.readUnsignedInt(), C.readUnsignedInt(), d)), !1, null, l, v);
              break;
            case 3:
              p = this._readMorphBitmap(A, C, d, c);
              h = this._createMorphPath(0, d, p.style, p.smoothImage, null, l, v);
              break;
            case 2:
              p = this._readMorphGradient(A, C, d, c);
              h = this._createMorphPath(0, d, p, !1, null, l, v);
              break;
            case 4:
              h = null;
              break;
            case 5:
              p = g(D[x], z[x++], d) / 20;
              n = k.ColorUtilities.rgbaToCSSStyle(b(A.readUnsignedInt(), C.readUnsignedInt(), d));
              A.position += 1;
              w = A.readByte();
              B = r.LINE_CAPS_STYLES[A.readByte()];
              E = r.LINE_JOINTS_STYLES[A.readByte()];
              p = new m(p, w, B, E, A.readByte());
              n = this._createMorphPath(1, d, n, !1, p, l, v);
              break;
            case 6:
              p = this._readMorphGradient(A, C, d, c);
              n = this._createMorphPath(2, d, p, !1, null, l, v);
              break;
            case 7:
              p = this._readMorphBitmap(A, C, d, c);
              n = this._createMorphPath(2, d, p.style, p.smoothImage, null, l, v);
              break;
            case 8:
              n = null;
          }
        }
        t && h && (h.lineTo(u, Q), n && n.lineTo(u, Q));
        return f;
      };
      c.prototype._createMorphPath = function(a, b, c, d, f, m, g) {
        a = new v(a, c, d, f);
        this._morphPaths[b].push(a);
        a.path.moveTo(m, g);
        return a.path;
      };
      c.prototype._readMorphMatrix = function(b, c, d) {
        return new a(g(b.readFloat(), c.readFloat(), d), g(b.readFloat(), c.readFloat(), d), g(b.readFloat(), c.readFloat(), d), g(b.readFloat(), c.readFloat(), d), g(b.readFloat(), c.readFloat(), d), g(b.readFloat(), c.readFloat(), d));
      };
      c.prototype._readMorphGradient = function(a, c, d, f) {
        var m = a.readUnsignedByte(), h = 2 * a.readShort() / 255, n = this._readMorphMatrix(a, c, d);
        f = 16 === m ? f.createLinearGradient(-1, 0, 1, 0) : f.createRadialGradient(h, 0, 0, 0, 0, 1);
        f.setTransform && f.setTransform(n.toSVGMatrix());
        n = a.readUnsignedByte();
        for (m = 0;m < n;m++) {
          var h = g(a.readUnsignedByte() / 255, c.readUnsignedByte() / 255, d), l = b(a.readUnsignedInt(), c.readUnsignedInt(), d), l = k.ColorUtilities.rgbaToCSSStyle(l);
          f.addColorStop(h, l);
        }
        a.position += 2;
        return f;
      };
      c.prototype._readMorphBitmap = function(a, b, c, d) {
        var f = a.readUnsignedInt();
        b = this._readMorphMatrix(a, b, c);
        c = a.readBoolean() ? "repeat" : "no-repeat";
        a = a.readBoolean();
        d = d.createPattern(this._textures[f]._canvas, c);
        d.setTransform(b.toSVGMatrix());
        return{style:d, smoothImage:a};
      };
      return c;
    }(r);
    p.RenderableMorphShape = f;
    var w = function() {
      function a() {
        this.align = this.leading = this.descent = this.ascent = this.width = this.y = this.x = 0;
        this.runs = [];
      }
      a.prototype.addRun = function(b, e, d, f) {
        if (d) {
          a._measureContext.font = b;
          var m = a._measureContext.measureText(d).width | 0;
          this.runs.push(new t(b, e, d, m, f));
          this.width += m;
        }
      };
      a.prototype.wrap = function(b) {
        var e = [this], d = this.runs, f = this;
        f.width = 0;
        f.runs = [];
        for (var m = a._measureContext, g = 0;g < d.length;g++) {
          var h = d[g], n = h.text;
          h.text = "";
          h.width = 0;
          m.font = h.font;
          for (var k = b, r = n.split(/[\s.-]/), v = 0, p = 0;p < r.length;p++) {
            var w = r[p], u = n.substr(v, w.length + 1), H = m.measureText(u).width | 0;
            if (H > k) {
              do {
                if (h.text && (f.runs.push(h), f.width += h.width, h = new t(h.font, h.fillStyle, "", 0, h.underline), k = new a, k.y = f.y + f.descent + f.leading + f.ascent | 0, k.ascent = f.ascent, k.descent = f.descent, k.leading = f.leading, k.align = f.align, e.push(k), f = k), k = b - H, 0 > k) {
                  var H = u.length, D, z;
                  do {
                    H--;
                    if (1 > H) {
                      throw Error("Shall never happen: bad maxWidth?");
                    }
                    D = u.substr(0, H);
                    z = m.measureText(D).width | 0;
                  } while (z > b);
                  h.text = D;
                  h.width = z;
                  u = u.substr(H);
                  H = m.measureText(u).width | 0;
                }
              } while (0 > k);
            } else {
              k -= H;
            }
            h.text += u;
            h.width += H;
            v += w.length + 1;
          }
          f.runs.push(h);
          f.width += h.width;
        }
        return e;
      };
      a._measureContext = document.createElement("canvas").getContext("2d");
      return a;
    }();
    p.TextLine = w;
    var t = function() {
      return function(a, b, e, d, f) {
        void 0 === a && (a = "");
        void 0 === b && (b = "");
        void 0 === e && (e = "");
        void 0 === d && (d = 0);
        void 0 === f && (f = !1);
        this.font = a;
        this.fillStyle = b;
        this.text = e;
        this.width = d;
        this.underline = f;
      };
    }();
    p.TextRun = t;
    f = function(b) {
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
      c.prototype.setContent = function(a, b, c, d) {
        this._textRunData = b;
        this._plainText = a;
        this._matrix.set(c);
        this._coords = d;
        this.lines = [];
      };
      c.prototype.setStyle = function(a, b, c, d) {
        this._backgroundColor = a;
        this._borderColor = b;
        this._scrollV = c;
        this._scrollH = d;
      };
      c.prototype.reflow = function(a, b) {
        var c = this._textRunData;
        if (c) {
          for (var d = this._bounds, f = d.w - 4, m = this._plainText, g = this.lines, h = new w, n = 0, l = 0, r = 0, v = 0, p = 0, t = -1;c.position < c.length;) {
            var u = c.readInt(), z = c.readInt(), A = c.readInt(), C = c.readUTF(), x = c.readInt(), F = c.readInt(), B = c.readInt();
            x > r && (r = x);
            F > v && (v = F);
            B > p && (p = B);
            x = c.readBoolean();
            F = "";
            c.readBoolean() && (F += "italic ");
            x && (F += "bold ");
            A = F + A + "px " + C;
            C = c.readInt();
            C = k.ColorUtilities.rgbToHex(C);
            x = c.readInt();
            -1 === t && (t = x);
            c.readBoolean();
            c.readInt();
            c.readInt();
            c.readInt();
            c.readInt();
            c.readInt();
            for (var x = c.readBoolean(), E = "", F = !1;!F;u++) {
              F = u >= z - 1;
              B = m[u];
              if ("\r" !== B && "\n" !== B && (E += B, u < m.length - 1)) {
                continue;
              }
              h.addRun(A, C, E, x);
              if (h.runs.length) {
                g.length && (n += p);
                n += r;
                h.y = n | 0;
                n += v;
                h.ascent = r;
                h.descent = v;
                h.leading = p;
                h.align = t;
                if (b && h.width > f) {
                  for (h = h.wrap(f), E = 0;E < h.length;E++) {
                    var G = h[E], n = G.y + G.descent + G.leading;
                    g.push(G);
                    G.width > l && (l = G.width);
                  }
                } else {
                  g.push(h), h.width > l && (l = h.width);
                }
                h = new w;
              } else {
                n += r + v + p;
              }
              E = "";
              if (F) {
                p = v = r = 0;
                t = -1;
                break;
              }
              "\r" === B && "\n" === m[u + 1] && u++;
            }
            h.addRun(A, C, E, x);
          }
          c = m[m.length - 1];
          "\r" !== c && "\n" !== c || g.push(h);
          c = this.textRect;
          c.w = l;
          c.h = n;
          if (a) {
            if (!b) {
              f = l;
              l = d.w;
              switch(a) {
                case 1:
                  c.x = l - (f + 4) >> 1;
                  break;
                case 3:
                  c.x = l - (f + 4);
              }
              this._textBounds.setElements(c.x - 2, c.y - 2, c.w + 4, c.h + 4);
              d.w = f + 4;
            }
            d.x = c.x - 2;
            d.h = n + 4;
          } else {
            this._textBounds = d;
          }
          for (u = 0;u < g.length;u++) {
            if (d = g[u], d.width < f) {
              switch(d.align) {
                case 1:
                  d.x = f - d.width | 0;
                  break;
                case 2:
                  d.x = (f - d.width) / 2 | 0;
              }
            }
          }
          this.invalidate();
        }
      };
      c.roundBoundPoints = function(a) {
        for (var b = 0;b < a.length;b++) {
          var c = a[b];
          c.x = Math.floor(c.x + .1) + .5;
          c.y = Math.floor(c.y + .1) + .5;
        }
      };
      c.prototype.render = function(b) {
        b.save();
        var d = this._textBounds;
        this._backgroundColor && (b.fillStyle = k.ColorUtilities.rgbaToCSSStyle(this._backgroundColor), b.fillRect(d.x, d.y, d.w, d.h));
        if (this._borderColor) {
          b.strokeStyle = k.ColorUtilities.rgbaToCSSStyle(this._borderColor);
          b.lineCap = "square";
          b.lineWidth = 1;
          var f = c.absoluteBoundPoints, m = b.currentTransform;
          m ? (d = d.clone(), (new a(m.a, m.b, m.c, m.d, m.e, m.f)).transformRectangle(d, f), b.setTransform(1, 0, 0, 1, 0, 0)) : (f[0].x = d.x, f[0].y = d.y, f[1].x = d.x + d.w, f[1].y = d.y, f[2].x = d.x + d.w, f[2].y = d.y + d.h, f[3].x = d.x, f[3].y = d.y + d.h);
          c.roundBoundPoints(f);
          d = new Path2D;
          d.moveTo(f[0].x, f[0].y);
          d.lineTo(f[1].x, f[1].y);
          d.lineTo(f[2].x, f[2].y);
          d.lineTo(f[3].x, f[3].y);
          d.lineTo(f[0].x, f[0].y);
          b.stroke(d);
          m && b.setTransform(m.a, m.b, m.c, m.d, m.e, m.f);
        }
        this._coords ? this._renderChars(b) : this._renderLines(b);
        b.restore();
      };
      c.prototype._renderChars = function(a) {
        if (this._matrix) {
          var b = this._matrix;
          a.transform(b.a, b.b, b.c, b.d, b.tx, b.ty);
        }
        for (var b = this.lines, c = this._coords, d = c.position = 0;d < b.length;d++) {
          for (var f = b[d].runs, m = 0;m < f.length;m++) {
            var g = f[m];
            a.font = g.font;
            a.fillStyle = g.fillStyle;
            for (var g = g.text, h = 0;h < g.length;h++) {
              var n = c.readInt() / 20, l = c.readInt() / 20;
              a.fillText(g[h], n, l);
            }
          }
        }
      };
      c.prototype._renderLines = function(a) {
        var b = this._textBounds;
        a.beginPath();
        a.rect(b.x + 2, b.y + 2, b.w - 4, b.h - 4);
        a.clip();
        a.translate(b.x - this._scrollH + 2, b.y + 2);
        for (var c = this.lines, d = this._scrollV, f = 0, m = 0;m < c.length;m++) {
          var g = c[m], h = g.x, n = g.y;
          if (m + 1 < d) {
            f = n + g.descent + g.leading;
          } else {
            n -= f;
            if (m + 1 - d && n > b.h) {
              break;
            }
            for (var l = g.runs, k = 0;k < l.length;k++) {
              var r = l[k];
              a.font = r.font;
              a.fillStyle = r.fillStyle;
              r.underline && a.fillRect(h, n + g.descent / 2 | 0, r.width, 1);
              a.textAlign = "left";
              a.textBaseline = "alphabetic";
              a.fillText(r.text, h, n);
              h += r.width;
            }
          }
        }
      };
      c.absoluteBoundPoints = [new u(0, 0), new u(0, 0), new u(0, 0), new u(0, 0)];
      return c;
    }(n);
    p.RenderableText = f;
    n = function(a) {
      function b(c, d) {
        a.call(this);
        this._flags = 3145728;
        this.properties = {};
        this.setBounds(new h(0, 0, c, d));
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "text", {get:function() {
        return this._text;
      }, set:function(a) {
        this._text = a;
      }, enumerable:!0, configurable:!0});
      b.prototype.render = function(a, b, c) {
        a.save();
        a.textBaseline = "top";
        a.fillStyle = "white";
        a.fillText(this.text, 0, 0);
        a.restore();
      };
      return b;
    }(n);
    p.Label = n;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var g = k.ColorUtilities.clampByte, b = function() {
      return function() {
      };
    }();
    p.Filter = b;
    var u = function(b) {
      function a(a, g, f) {
        b.call(this);
        this.blurX = a;
        this.blurY = g;
        this.quality = f;
      }
      __extends(a, b);
      return a;
    }(b);
    p.BlurFilter = u;
    u = function(b) {
      function a(a, g, f, k, m, r, p, t, l, c, e) {
        b.call(this);
        this.alpha = a;
        this.angle = g;
        this.blurX = f;
        this.blurY = k;
        this.color = m;
        this.distance = r;
        this.hideObject = p;
        this.inner = t;
        this.knockout = l;
        this.quality = c;
        this.strength = e;
      }
      __extends(a, b);
      return a;
    }(b);
    p.DropshadowFilter = u;
    b = function(b) {
      function a(a, g, f, k, m, r, p, t) {
        b.call(this);
        this.alpha = a;
        this.blurX = g;
        this.blurY = f;
        this.color = k;
        this.inner = m;
        this.knockout = r;
        this.quality = p;
        this.strength = t;
      }
      __extends(a, b);
      return a;
    }(b);
    p.GlowFilter = b;
    (function(b) {
      b[b.Unknown = 0] = "Unknown";
      b[b.Identity = 1] = "Identity";
    })(p.ColorMatrixType || (p.ColorMatrixType = {}));
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
      b.prototype.setMultipliersAndOffsets = function(a, b, g, f, h, m, k, p) {
        for (var t = this._data, l = 0;l < t.length;l++) {
          t[l] = 0;
        }
        t[0] = a;
        t[5] = b;
        t[10] = g;
        t[15] = f;
        t[16] = h / 255;
        t[17] = m / 255;
        t[18] = k / 255;
        t[19] = p / 255;
        this._type = 0;
      };
      b.prototype.transformRGBA = function(a) {
        var b = a >> 24 & 255, h = a >> 16 & 255, f = a >> 8 & 255, k = a & 255, m = this._data;
        a = g(b * m[0] + h * m[1] + f * m[2] + k * m[3] + 255 * m[16]);
        var r = g(b * m[4] + h * m[5] + f * m[6] + k * m[7] + 255 * m[17]), p = g(b * m[8] + h * m[9] + f * m[10] + k * m[11] + 255 * m[18]), b = g(b * m[12] + h * m[13] + f * m[14] + k * m[15] + 255 * m[19]);
        return a << 24 | r << 16 | p << 8 | b;
      };
      b.prototype.multiply = function(a) {
        if (!(a._type & 1)) {
          var b = this._data, g = a._data;
          a = b[0];
          var f = b[1], h = b[2], m = b[3], k = b[4], p = b[5], t = b[6], l = b[7], c = b[8], e = b[9], q = b[10], s = b[11], y = b[12], I = b[13], u = b[14], R = b[15], fa = b[16], ca = b[17], ga = b[18], ha = b[19], W = g[0], Q = g[1], H = g[2], D = g[3], z = g[4], A = g[5], C = g[6], x = g[7], F = g[8], B = g[9], E = g[10], G = g[11], J = g[12], K = g[13], L = g[14], M = g[15], N = g[16], O = g[17], P = g[18], g = g[19];
          b[0] = a * W + k * Q + c * H + y * D;
          b[1] = f * W + p * Q + e * H + I * D;
          b[2] = h * W + t * Q + q * H + u * D;
          b[3] = m * W + l * Q + s * H + R * D;
          b[4] = a * z + k * A + c * C + y * x;
          b[5] = f * z + p * A + e * C + I * x;
          b[6] = h * z + t * A + q * C + u * x;
          b[7] = m * z + l * A + s * C + R * x;
          b[8] = a * F + k * B + c * E + y * G;
          b[9] = f * F + p * B + e * E + I * G;
          b[10] = h * F + t * B + q * E + u * G;
          b[11] = m * F + l * B + s * E + R * G;
          b[12] = a * J + k * K + c * L + y * M;
          b[13] = f * J + p * K + e * L + I * M;
          b[14] = h * J + t * K + q * L + u * M;
          b[15] = m * J + l * K + s * L + R * M;
          b[16] = a * N + k * O + c * P + y * g + fa;
          b[17] = f * N + p * O + e * P + I * g + ca;
          b[18] = h * N + t * O + q * P + u * g + ga;
          b[19] = m * N + l * O + s * P + R * g + ha;
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
        for (var g = 0;20 > g;g++) {
          if (.001 < Math.abs(b[g] - a[g])) {
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
    p.ColorMatrix = b;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      function b(a, b) {
        return-1 !== a.indexOf(b, this.length - b.length);
      }
      var u = p.Geometry.Point3D, h = p.Geometry.Matrix3D, a = p.Geometry.degreesToRadian, d = k.Debug.unexpected, n = k.Debug.notImplemented;
      g.SHADER_ROOT = "shaders/";
      var f = function() {
        function f(a, b) {
          this._fillColor = k.Color.Red;
          this._surfaceRegionCache = new k.LRUList;
          this.modelViewProjectionMatrix = h.createIdentity();
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
          this.modelViewProjectionMatrix = h.create2DProjection(this._w, this._h, 2E3);
          var d = this;
          this._surfaceRegionAllocator = new p.SurfaceRegionAllocator.SimpleAllocator(function() {
            var a = d._createTexture();
            return new g.WebGLSurface(1024, 1024, a);
          });
        }
        Object.defineProperty(f.prototype, "surfaces", {get:function() {
          return this._surfaceRegionAllocator.surfaces;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(f.prototype, "fillStyle", {set:function(a) {
          this._fillColor.set(k.Color.parseColor(a));
        }, enumerable:!0, configurable:!0});
        f.prototype.setBlendMode = function(a) {
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
              n("Blend Mode: " + a);
          }
        };
        f.prototype.setBlendOptions = function() {
          this.gl.blendFunc(this._options.sourceBlendFactor, this._options.destinationBlendFactor);
        };
        f.glSupportedBlendMode = function(a) {
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
        f.prototype.create2DProjectionMatrix = function() {
          return h.create2DProjection(this._w, this._h, -this._w);
        };
        f.prototype.createPerspectiveMatrix = function(b, d, f) {
          f = a(f);
          d = h.createPerspective(a(d));
          var g = new u(0, 1, 0), n = new u(0, 0, 0);
          b = new u(0, 0, b);
          b = h.createCameraLookAt(b, n, g);
          b = h.createInverse(b);
          g = h.createIdentity();
          g = h.createMultiply(g, h.createTranslation(-this._w / 2, -this._h / 2));
          g = h.createMultiply(g, h.createScale(1 / this._w, -1 / this._h, .01));
          g = h.createMultiply(g, h.createYRotation(f));
          g = h.createMultiply(g, b);
          return g = h.createMultiply(g, d);
        };
        f.prototype.discardCachedImages = function() {
          2 <= p.traceLevel && p.writer && p.writer.writeLn("Discard Cache");
          for (var a = this._surfaceRegionCache.count / 2 | 0, b = 0;b < a;b++) {
            var d = this._surfaceRegionCache.pop();
            2 <= p.traceLevel && p.writer && p.writer.writeLn("Discard: " + d);
            d.texture.atlas.remove(d.region);
            d.texture = null;
          }
        };
        f.prototype.cacheImage = function(a) {
          var b = this.allocateSurfaceRegion(a.width, a.height);
          2 <= p.traceLevel && p.writer && p.writer.writeLn("Uploading Image: @ " + b.region);
          this._surfaceRegionCache.use(b);
          this.updateSurfaceRegion(a, b);
          return b;
        };
        f.prototype.allocateSurfaceRegion = function(a, b) {
          return this._surfaceRegionAllocator.allocate(a, b, null);
        };
        f.prototype.updateSurfaceRegion = function(a, b) {
          var d = this.gl;
          d.bindTexture(d.TEXTURE_2D, b.surface.texture);
          d.texSubImage2D(d.TEXTURE_2D, 0, b.region.x, b.region.y, d.RGBA, d.UNSIGNED_BYTE, a);
        };
        f.prototype._resize = function() {
          var a = this.gl;
          this._w = this._canvas.width;
          this._h = this._canvas.height;
          a.viewport(0, 0, this._w, this._h);
          for (var b in this._programCache) {
            this._initializeProgram(this._programCache[b]);
          }
        };
        f.prototype._initializeProgram = function(a) {
          this.gl.useProgram(a);
        };
        f.prototype._createShaderFromFile = function(a) {
          var d = g.SHADER_ROOT + a, f = this.gl;
          a = new XMLHttpRequest;
          a.open("GET", d, !1);
          a.send();
          if (b(d, ".vert")) {
            d = f.VERTEX_SHADER;
          } else {
            if (b(d, ".frag")) {
              d = f.FRAGMENT_SHADER;
            } else {
              throw "Shader Type: not supported.";
            }
          }
          return this._createShader(d, a.responseText);
        };
        f.prototype.createProgramFromFiles = function() {
          var a = this._programCache["combined.vert-combined.frag"];
          a || (a = this._createProgram([this._createShaderFromFile("combined.vert"), this._createShaderFromFile("combined.frag")]), this._queryProgramAttributesAndUniforms(a), this._initializeProgram(a), this._programCache["combined.vert-combined.frag"] = a);
          return a;
        };
        f.prototype._createProgram = function(a) {
          var b = this.gl, f = b.createProgram();
          a.forEach(function(a) {
            b.attachShader(f, a);
          });
          b.linkProgram(f);
          b.getProgramParameter(f, b.LINK_STATUS) || (d("Cannot link program: " + b.getProgramInfoLog(f)), b.deleteProgram(f));
          return f;
        };
        f.prototype._createShader = function(a, b) {
          var f = this.gl, g = f.createShader(a);
          f.shaderSource(g, b);
          f.compileShader(g);
          return f.getShaderParameter(g, f.COMPILE_STATUS) ? g : (d("Cannot compile shader: " + f.getShaderInfoLog(g)), f.deleteShader(g), null);
        };
        f.prototype._createTexture = function() {
          var a = this.gl, b = a.createTexture();
          a.bindTexture(a.TEXTURE_2D, b);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_S, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_T, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MIN_FILTER, a.LINEAR);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MAG_FILTER, a.LINEAR);
          a.texImage2D(a.TEXTURE_2D, 0, a.RGBA, 1024, 1024, 0, a.RGBA, a.UNSIGNED_BYTE, null);
          return b;
        };
        f.prototype._createFramebuffer = function(a) {
          var b = this.gl, d = b.createFramebuffer();
          b.bindFramebuffer(b.FRAMEBUFFER, d);
          b.framebufferTexture2D(b.FRAMEBUFFER, b.COLOR_ATTACHMENT0, b.TEXTURE_2D, a, 0);
          b.bindFramebuffer(b.FRAMEBUFFER, null);
          return d;
        };
        f.prototype._queryProgramAttributesAndUniforms = function(a) {
          a.uniforms = {};
          a.attributes = {};
          for (var b = this.gl, d = 0, f = b.getProgramParameter(a, b.ACTIVE_ATTRIBUTES);d < f;d++) {
            var g = b.getActiveAttrib(a, d);
            a.attributes[g.name] = g;
            g.location = b.getAttribLocation(a, g.name);
          }
          d = 0;
          for (f = b.getProgramParameter(a, b.ACTIVE_UNIFORMS);d < f;d++) {
            g = b.getActiveUniform(a, d), a.uniforms[g.name] = g, g.location = b.getUniformLocation(a, g.name);
          }
        };
        Object.defineProperty(f.prototype, "target", {set:function(a) {
          var b = this.gl;
          a ? (b.viewport(0, 0, a.w, a.h), b.bindFramebuffer(b.FRAMEBUFFER, a.framebuffer)) : (b.viewport(0, 0, this._w, this._h), b.bindFramebuffer(b.FRAMEBUFFER, null));
        }, enumerable:!0, configurable:!0});
        f.prototype.clear = function(a) {
          a = this.gl;
          a.clearColor(0, 0, 0, 0);
          a.clear(a.COLOR_BUFFER_BIT);
        };
        f.prototype.clearTextureRegion = function(a, b) {
          void 0 === b && (b = k.Color.None);
          var d = this.gl, f = a.region;
          this.target = a.surface;
          d.enable(d.SCISSOR_TEST);
          d.scissor(f.x, f.y, f.w, f.h);
          d.clearColor(b.r, b.g, b.b, b.a);
          d.clear(d.COLOR_BUFFER_BIT | d.DEPTH_BUFFER_BIT);
          d.disable(d.SCISSOR_TEST);
        };
        f.prototype.sizeOf = function(a) {
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
              n(a);
          }
        };
        f.MAX_SURFACES = 8;
        return f;
      }();
      g.WebGLContext = f;
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(k, p) {
  function g() {
    this.constructor = k;
  }
  for (var b in p) {
    p.hasOwnProperty(b) && (k[b] = p[b]);
  }
  g.prototype = p.prototype;
  k.prototype = new g;
};
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.Debug.assert, u = function(a) {
        function d() {
          a.apply(this, arguments);
        }
        __extends(d, a);
        d.prototype.ensureVertexCapacity = function(a) {
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8 * a);
        };
        d.prototype.writeVertex = function(a, d) {
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8);
          this.writeVertexUnsafe(a, d);
        };
        d.prototype.writeVertexUnsafe = function(a, b) {
          var d = this._offset >> 2;
          this._f32[d] = a;
          this._f32[d + 1] = b;
          this._offset += 8;
        };
        d.prototype.writeVertex3D = function(a, d, g) {
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 12);
          this.writeVertex3DUnsafe(a, d, g);
        };
        d.prototype.writeVertex3DUnsafe = function(a, b, d) {
          var g = this._offset >> 2;
          this._f32[g] = a;
          this._f32[g + 1] = b;
          this._f32[g + 2] = d;
          this._offset += 12;
        };
        d.prototype.writeTriangleElements = function(a, d, g) {
          b(0 === (this._offset & 1));
          this.ensureCapacity(this._offset + 6);
          var m = this._offset >> 1;
          this._u16[m] = a;
          this._u16[m + 1] = d;
          this._u16[m + 2] = g;
          this._offset += 6;
        };
        d.prototype.ensureColorCapacity = function(a) {
          b(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16 * a);
        };
        d.prototype.writeColorFloats = function(a, d, g, m) {
          b(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16);
          this.writeColorFloatsUnsafe(a, d, g, m);
        };
        d.prototype.writeColorFloatsUnsafe = function(a, b, d, g) {
          var h = this._offset >> 2;
          this._f32[h] = a;
          this._f32[h + 1] = b;
          this._f32[h + 2] = d;
          this._f32[h + 3] = g;
          this._offset += 16;
        };
        d.prototype.writeColor = function() {
          var a = Math.random(), d = Math.random(), g = Math.random(), m = Math.random() / 2;
          b(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 4);
          this._i32[this._offset >> 2] = m << 24 | g << 16 | d << 8 | a;
          this._offset += 4;
        };
        d.prototype.writeColorUnsafe = function(a, b, d, g) {
          this._i32[this._offset >> 2] = g << 24 | d << 16 | b << 8 | a;
          this._offset += 4;
        };
        d.prototype.writeRandomColor = function() {
          this.writeColor();
        };
        return d;
      }(k.ArrayUtilities.ArrayWriter);
      g.BufferWriter = u;
      g.WebGLAttribute = function() {
        return function(a, b, g, f) {
          void 0 === f && (f = !1);
          this.name = a;
          this.size = b;
          this.type = g;
          this.normalized = f;
        };
      }();
      var h = function() {
        function a(a) {
          this.size = 0;
          this.attributes = a;
        }
        a.prototype.initialize = function(a) {
          for (var b = 0, f = 0;f < this.attributes.length;f++) {
            this.attributes[f].offset = b, b += a.sizeOf(this.attributes[f].type) * this.attributes[f].size;
          }
          this.size = b;
        };
        return a;
      }();
      g.WebGLAttributeList = h;
      h = function() {
        function a(a) {
          this._elementOffset = this.triangleCount = 0;
          this.context = a;
          this.array = new u(8);
          this.buffer = a.gl.createBuffer();
          this.elementArray = new u(8);
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
      g.WebGLGeometry = h;
      h = function(a) {
        function b(d, f, g) {
          a.call(this, d, f, g);
        }
        __extends(b, a);
        b.createEmptyVertices = function(a, b) {
          for (var d = [], g = 0;g < b;g++) {
            d.push(new a(0, 0, 0));
          }
          return d;
        };
        return b;
      }(p.Geometry.Point3D);
      g.Vertex = h;
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
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var b = function() {
        function b(a, d, g) {
          this.texture = g;
          this.w = a;
          this.h = d;
          this._regionAllocator = new k.RegionAllocator.CompactAllocator(this.w, this.h);
        }
        b.prototype.allocate = function(a, b) {
          var g = this._regionAllocator.allocate(a, b);
          return g ? new u(this, g) : null;
        };
        b.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return b;
      }();
      g.WebGLSurface = b;
      var u = function() {
        return function(b, a) {
          this.surface = b;
          this.region = a;
          this.next = this.previous = null;
        };
      }();
      g.WebGLSurfaceRegion = u;
    })(k.WebGL || (k.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.Color;
      g.TILE_SIZE = 256;
      g.MIN_UNTILED_SIZE = 256;
      var u = p.Geometry.Matrix, h = p.Geometry.Rectangle, a = function(a) {
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
      g.WebGLRendererOptions = a;
      var d = function(d) {
        function f(b, f, h) {
          void 0 === h && (h = new a);
          d.call(this, b, f, h);
          this._tmpVertices = g.Vertex.createEmptyVertices(g.Vertex, 64);
          this._cachedTiles = [];
          b = this._context = new g.WebGLContext(this._canvas, h);
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
          h.showTemporaryCanvases && (document.getElementById("temporaryCanvasPanelContainer").appendChild(this._uploadCanvas), document.getElementById("temporaryCanvasPanelContainer").appendChild(this._scratchCanvas));
          this._clipStack = [];
        }
        __extends(f, d);
        f.prototype.resize = function() {
          this._updateSize();
          this.render();
        };
        f.prototype._updateSize = function() {
          this._viewport = new h(0, 0, this._canvas.width, this._canvas.height);
          this._context._resize();
        };
        f.prototype._cacheImageCallback = function(a, b, d) {
          var f = d.w, g = d.h, h = d.x;
          d = d.y;
          this._uploadCanvas.width = f + 2;
          this._uploadCanvas.height = g + 2;
          this._uploadCanvasContext.drawImage(b.canvas, h, d, f, g, 1, 1, f, g);
          this._uploadCanvasContext.drawImage(b.canvas, h, d, f, 1, 1, 0, f, 1);
          this._uploadCanvasContext.drawImage(b.canvas, h, d + g - 1, f, 1, 1, g + 1, f, 1);
          this._uploadCanvasContext.drawImage(b.canvas, h, d, 1, g, 0, 1, 1, g);
          this._uploadCanvasContext.drawImage(b.canvas, h + f - 1, d, 1, g, f + 1, 1, 1, g);
          return a && a.surface ? (this._options.disableSurfaceUploads || this._context.updateSurfaceRegion(this._uploadCanvas, a), a) : this._context.cacheImage(this._uploadCanvas);
        };
        f.prototype._enterClip = function(a, b, d, f) {
          d.flush();
          b = this._context.gl;
          0 === this._clipStack.length && (b.enable(b.STENCIL_TEST), b.clear(b.STENCIL_BUFFER_BIT), b.stencilFunc(b.ALWAYS, 1, 1));
          this._clipStack.push(a);
          b.colorMask(!1, !1, !1, !1);
          b.stencilOp(b.KEEP, b.KEEP, b.INCR);
          d.flush();
          b.colorMask(!0, !0, !0, !0);
          b.stencilFunc(b.NOTEQUAL, 0, this._clipStack.length);
          b.stencilOp(b.KEEP, b.KEEP, b.KEEP);
        };
        f.prototype._leaveClip = function(a, b, d, f) {
          d.flush();
          b = this._context.gl;
          if (a = this._clipStack.pop()) {
            b.colorMask(!1, !1, !1, !1), b.stencilOp(b.KEEP, b.KEEP, b.DECR), d.flush(), b.colorMask(!0, !0, !0, !0), b.stencilFunc(b.NOTEQUAL, 0, this._clipStack.length), b.stencilOp(b.KEEP, b.KEEP, b.KEEP);
          }
          0 === this._clipStack.length && b.disable(b.STENCIL_TEST);
        };
        f.prototype._renderFrame = function(a, b, d, f) {
        };
        f.prototype._renderSurfaces = function(a) {
          var d = this._options, f = this._context, k = this._viewport;
          if (d.drawSurfaces) {
            var n = f.surfaces, f = u.createIdentity();
            if (0 <= d.drawSurface && d.drawSurface < n.length) {
              for (var d = n[d.drawSurface | 0], n = new h(0, 0, d.w, d.h), l = n.clone();l.w > k.w;) {
                l.scale(.5, .5);
              }
              a.drawImage(new g.WebGLSurfaceRegion(d, n), l, b.White, null, f, .2);
            } else {
              l = k.w / 5;
              l > k.h / n.length && (l = k.h / n.length);
              a.fillRectangle(new h(k.w - l, 0, l, k.h), new b(0, 0, 0, .5), f, .1);
              for (var c = 0;c < n.length;c++) {
                var d = n[c], e = new h(k.w - l, c * l, l, l);
                a.drawImage(new g.WebGLSurfaceRegion(d, new h(0, 0, d.w, d.h)), e, b.White, null, f, .2);
              }
            }
            a.flush();
          }
        };
        f.prototype.render = function() {
          var a = this._options, d = this._context.gl;
          this._context.modelViewProjectionMatrix = a.perspectiveCamera ? this._context.createPerspectiveMatrix(a.perspectiveCameraDistance + (a.animateZoom ? .8 * Math.sin(Date.now() / 3E3) : 0), a.perspectiveCameraFOV, a.perspectiveCameraAngle) : this._context.create2DProjectionMatrix();
          var f = this._brush;
          d.clearColor(0, 0, 0, 0);
          d.clear(d.COLOR_BUFFER_BIT | d.DEPTH_BUFFER_BIT);
          f.reset();
          d = this._viewport;
          f.flush();
          a.paintViewport && (f.fillRectangle(d, new b(.5, 0, 0, .25), u.createIdentity(), 0), f.flush());
          this._renderSurfaces(f);
        };
        return f;
      }(p.Renderer);
      g.WebGLRenderer = d;
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.Color, u = p.Geometry.Point, h = p.Geometry.Matrix3D, a = function() {
        function a(b, d, g) {
          this._target = g;
          this._context = b;
          this._geometry = d;
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
      var d = function(a) {
        function d(f, g, h) {
          a.call(this, f, g, h);
          this.kind = 0;
          this.color = new b(0, 0, 0, 0);
          this.sampler = 0;
          this.coordinate = new u(0, 0);
        }
        __extends(d, a);
        d.initializeAttributeList = function(a) {
          var b = a.gl;
          d.attributeList || (d.attributeList = new g.WebGLAttributeList([new g.WebGLAttribute("aPosition", 3, b.FLOAT), new g.WebGLAttribute("aCoordinate", 2, b.FLOAT), new g.WebGLAttribute("aColor", 4, b.UNSIGNED_BYTE, !0), new g.WebGLAttribute("aKind", 1, b.FLOAT), new g.WebGLAttribute("aSampler", 1, b.FLOAT)]), d.attributeList.initialize(a));
        };
        d.prototype.writeTo = function(a) {
          a = a.array;
          a.ensureAdditionalCapacity();
          a.writeVertex3DUnsafe(this.x, this.y, this.z);
          a.writeVertexUnsafe(this.coordinate.x, this.coordinate.y);
          a.writeColorUnsafe(255 * this.color.r, 255 * this.color.g, 255 * this.color.b, 255 * this.color.a);
          a.writeFloatUnsafe(this.kind);
          a.writeFloatUnsafe(this.sampler);
        };
        return d;
      }(g.Vertex);
      g.WebGLCombinedBrushVertex = d;
      a = function(a) {
        function b(f, g, h) {
          void 0 === h && (h = null);
          a.call(this, f, g, h);
          this._blendMode = 1;
          this._program = f.createProgramFromFiles();
          this._surfaces = [];
          d.initializeAttributeList(this._context);
        }
        __extends(b, a);
        b.prototype.reset = function() {
          this._surfaces = [];
          this._geometry.reset();
        };
        b.prototype.drawImage = function(a, d, g, h, k, l, c) {
          void 0 === l && (l = 0);
          void 0 === c && (c = 1);
          if (!a || !a.surface) {
            return!0;
          }
          d = d.clone();
          this._colorMatrix && (h && this._colorMatrix.equals(h) || this.flush());
          this._colorMatrix = h;
          this._blendMode !== c && (this.flush(), this._blendMode = c);
          c = this._surfaces.indexOf(a.surface);
          0 > c && (8 === this._surfaces.length && this.flush(), this._surfaces.push(a.surface), c = this._surfaces.length - 1);
          var e = b._tmpVertices, q = a.region.clone();
          q.offset(1, 1).resize(-2, -2);
          q.scale(1 / a.surface.w, 1 / a.surface.h);
          k.transformRectangle(d, e);
          for (a = 0;4 > a;a++) {
            e[a].z = l;
          }
          e[0].coordinate.x = q.x;
          e[0].coordinate.y = q.y;
          e[1].coordinate.x = q.x + q.w;
          e[1].coordinate.y = q.y;
          e[2].coordinate.x = q.x + q.w;
          e[2].coordinate.y = q.y + q.h;
          e[3].coordinate.x = q.x;
          e[3].coordinate.y = q.y + q.h;
          for (a = 0;4 > a;a++) {
            l = b._tmpVertices[a], l.kind = h ? 2 : 1, l.color.set(g), l.sampler = c, l.writeTo(this._geometry);
          }
          this._geometry.addQuad();
          return!0;
        };
        b.prototype.fillRectangle = function(a, d, g, h) {
          void 0 === h && (h = 0);
          g.transformRectangle(a, b._tmpVertices);
          for (a = 0;4 > a;a++) {
            g = b._tmpVertices[a], g.kind = 0, g.color.set(d), g.z = h, g.writeTo(this._geometry);
          }
          this._geometry.addQuad();
        };
        b.prototype.flush = function() {
          var a = this._geometry, b = this._program, f = this._context.gl, g;
          a.uploadBuffers();
          f.useProgram(b);
          this._target ? (g = h.create2DProjection(this._target.w, this._target.h, 2E3), g = h.createMultiply(g, h.createScale(1, -1, 1))) : g = this._context.modelViewProjectionMatrix;
          f.uniformMatrix4fv(b.uniforms.uTransformMatrix3D.location, !1, g.asWebGLMatrix());
          this._colorMatrix && (f.uniformMatrix4fv(b.uniforms.uColorMatrix.location, !1, this._colorMatrix.asWebGLMatrix()), f.uniform4fv(b.uniforms.uColorVector.location, this._colorMatrix.asWebGLVector()));
          for (g = 0;g < this._surfaces.length;g++) {
            f.activeTexture(f.TEXTURE0 + g), f.bindTexture(f.TEXTURE_2D, this._surfaces[g].texture);
          }
          f.uniform1iv(b.uniforms["uSampler[0]"].location, [0, 1, 2, 3, 4, 5, 6, 7]);
          f.bindBuffer(f.ARRAY_BUFFER, a.buffer);
          var k = d.attributeList.size, l = d.attributeList.attributes;
          for (g = 0;g < l.length;g++) {
            var c = l[g], e = b.attributes[c.name].location;
            f.enableVertexAttribArray(e);
            f.vertexAttribPointer(e, c.size, c.type, c.normalized, k, c.offset);
          }
          this._context.setBlendOptions();
          this._context.target = this._target;
          f.bindBuffer(f.ELEMENT_ARRAY_BUFFER, a.elementBuffer);
          f.drawElements(f.TRIANGLES, 3 * a.triangleCount, f.UNSIGNED_SHORT, 0);
          this.reset();
        };
        b._tmpVertices = g.Vertex.createEmptyVertices(d, 4);
        b._depth = 1;
        return b;
      }(a);
      g.WebGLCombinedBrush = a;
    })(p.WebGL || (p.WebGL = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(k) {
    (function(g) {
      var b = CanvasRenderingContext2D.prototype.save, k = CanvasRenderingContext2D.prototype.clip, h = CanvasRenderingContext2D.prototype.fill, a = CanvasRenderingContext2D.prototype.stroke, d = CanvasRenderingContext2D.prototype.restore, n = CanvasRenderingContext2D.prototype.beginPath;
      g.notifyReleaseChanged = function() {
        CanvasRenderingContext2D.prototype.save = b;
        CanvasRenderingContext2D.prototype.clip = k;
        CanvasRenderingContext2D.prototype.fill = h;
        CanvasRenderingContext2D.prototype.stroke = a;
        CanvasRenderingContext2D.prototype.restore = d;
        CanvasRenderingContext2D.prototype.beginPath = n;
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
            k.Debug.somewhatImplemented("Blend Mode: " + p.BlendMode[a]);
        }
        return b;
      }
      var u = k.NumberUtilities.clamp;
      navigator.userAgent.indexOf("Firefox");
      var h = function() {
        function a() {
        }
        a._prepareSVGFilters = function() {
          if (!a._svgBlurFilter) {
            var b = document.createElementNS("http://www.w3.org/2000/svg", "svg"), f = document.createElementNS("http://www.w3.org/2000/svg", "defs"), g = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            g.setAttribute("id", "svgBlurFilter");
            var h = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            h.setAttribute("stdDeviation", "0 0");
            g.appendChild(h);
            f.appendChild(g);
            a._svgBlurFilter = h;
            g = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            g.setAttribute("id", "svgDropShadowFilter");
            h = document.createElementNS("http://www.w3.org/2000/svg", "feGaussianBlur");
            h.setAttribute("in", "SourceAlpha");
            h.setAttribute("stdDeviation", "3");
            g.appendChild(h);
            a._svgDropshadowFilterBlur = h;
            h = document.createElementNS("http://www.w3.org/2000/svg", "feOffset");
            h.setAttribute("dx", "0");
            h.setAttribute("dy", "0");
            h.setAttribute("result", "offsetblur");
            g.appendChild(h);
            a._svgDropshadowFilterOffset = h;
            h = document.createElementNS("http://www.w3.org/2000/svg", "feFlood");
            h.setAttribute("flood-color", "rgba(0,0,0,1)");
            g.appendChild(h);
            a._svgDropshadowFilterFlood = h;
            h = document.createElementNS("http://www.w3.org/2000/svg", "feComposite");
            h.setAttribute("in2", "offsetblur");
            h.setAttribute("operator", "in");
            g.appendChild(h);
            var h = document.createElementNS("http://www.w3.org/2000/svg", "feMerge"), k = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            h.appendChild(k);
            k = document.createElementNS("http://www.w3.org/2000/svg", "feMergeNode");
            k.setAttribute("in", "SourceGraphic");
            h.appendChild(k);
            g.appendChild(h);
            f.appendChild(g);
            g = document.createElementNS("http://www.w3.org/2000/svg", "filter");
            g.setAttribute("id", "svgColorMatrixFilter");
            h = document.createElementNS("http://www.w3.org/2000/svg", "feColorMatrix");
            h.setAttribute("color-interpolation-filters", "sRGB");
            h.setAttribute("in", "SourceGraphic");
            h.setAttribute("type", "matrix");
            g.appendChild(h);
            f.appendChild(g);
            a._svgColorMatrixFilter = h;
            b.appendChild(f);
            document.documentElement.appendChild(b);
          }
        };
        a._applyColorMatrixFilter = function(b, f) {
          a._prepareSVGFilters();
          a._svgColorMatrixFilter.setAttribute("values", f.toSVGFilterMatrix());
          b.filter = "url(#svgColorMatrixFilter)";
        };
        a._applyFilters = function(b, f, g) {
          function h(a) {
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
          a._removeFilters(f);
          for (var r = 0;r < g.length;r++) {
            var w = g[r];
            if (w instanceof p.BlurFilter) {
              var t = w, w = h(t.quality);
              a._svgBlurFilter.setAttribute("stdDeviation", t.blurX * w + " " + t.blurY * w);
              f.filter = "url(#svgBlurFilter)";
            } else {
              w instanceof p.DropshadowFilter && (t = w, w = h(t.quality), a._svgDropshadowFilterBlur.setAttribute("stdDeviation", t.blurX * w + " " + t.blurY * w), a._svgDropshadowFilterOffset.setAttribute("dx", String(Math.cos(t.angle * Math.PI / 180) * t.distance * b)), a._svgDropshadowFilterOffset.setAttribute("dy", String(Math.sin(t.angle * Math.PI / 180) * t.distance * b)), a._svgDropshadowFilterFlood.setAttribute("flood-color", k.ColorUtilities.rgbaToCSSStyle(t.color << 8 | Math.round(255 * 
              t.alpha))), f.filter = "url(#svgDropShadowFilter)");
            }
          }
        };
        a._removeFilters = function(a) {
          a.filter = "none";
        };
        a._applyColorMatrix = function(b, f) {
          a._removeFilters(b);
          f.isIdentity() ? (b.globalAlpha = 1, b.globalColorMatrix = null) : f.hasOnlyAlphaMultiplier() ? (b.globalAlpha = u(f.alphaMultiplier, 0, 1), b.globalColorMatrix = null) : (b.globalAlpha = 1, a._svgFiltersAreSupported ? (a._applyColorMatrixFilter(b, f), b.globalColorMatrix = null) : b.globalColorMatrix = f);
        };
        a._svgFiltersAreSupported = !!Object.getOwnPropertyDescriptor(CanvasRenderingContext2D.prototype, "filter");
        return a;
      }();
      g.Filters = h;
      var a = function() {
        function a(b, d, g, h) {
          this.surface = b;
          this.region = d;
          this.w = g;
          this.h = h;
        }
        a.prototype.free = function() {
          this.surface.free(this);
        };
        a._ensureCopyCanvasSize = function(b, f) {
          var g;
          if (a._copyCanvasContext) {
            if (g = a._copyCanvasContext.canvas, g.width < b || g.height < f) {
              g.width = k.IntegerUtilities.nearestPowerOfTwo(b), g.height = k.IntegerUtilities.nearestPowerOfTwo(f);
            }
          } else {
            g = document.createElement("canvas"), "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(g), g.width = 512, g.height = 512, a._copyCanvasContext = g.getContext("2d");
          }
        };
        a.prototype.draw = function(g, f, h, m, k, p) {
          this.context.setTransform(1, 0, 0, 1, 0, 0);
          var t, l = 0, c = 0;
          g.context.canvas === this.context.canvas ? (a._ensureCopyCanvasSize(m, k), t = a._copyCanvasContext, t.clearRect(0, 0, m, k), t.drawImage(g.surface.canvas, g.region.x, g.region.y, m, k, 0, 0, m, k), t = t.canvas, c = l = 0) : (t = g.surface.canvas, l = g.region.x, c = g.region.y);
          a: {
            switch(p) {
              case 11:
                g = !0;
                break a;
              default:
                g = !1;
            }
          }
          g && (this.context.save(), this.context.beginPath(), this.context.rect(f, h, m, k), this.context.clip());
          this.context.globalCompositeOperation = b(p);
          this.context.drawImage(t, l, c, m, k, f, h, m, k);
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
          var b = this.surface.context, d = this.region;
          b.fillStyle = a;
          b.fillRect(d.x, d.y, d.w, d.h);
        };
        a.prototype.clear = function(a) {
          var b = this.surface.context, d = this.region;
          a || (a = d);
          b.clearRect(a.x, a.y, a.w, a.h);
        };
        return a;
      }();
      g.Canvas2DSurfaceRegion = a;
      h = function() {
        function b(a, d) {
          this.canvas = a;
          this.context = a.getContext("2d");
          this.w = a.width;
          this.h = a.height;
          this._regionAllocator = d;
        }
        b.prototype.allocate = function(b, d) {
          var g = this._regionAllocator.allocate(b, d);
          return g ? new a(this, g, b, d) : null;
        };
        b.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return b;
      }();
      g.Canvas2DSurface = h;
    })(p.Canvas2D || (p.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.Debug.assert, u = k.GFX.Geometry.Rectangle, h = k.GFX.Geometry.Point, a = k.GFX.Geometry.Matrix, d = k.NumberUtilities.clamp, n = k.NumberUtilities.pow2, f = new k.IndentingWriter(!1, dumpLine), v = function() {
        return function(a, b) {
          this.surfaceRegion = a;
          this.scale = b;
        };
      }();
      g.MipMapLevel = v;
      var m = function() {
        function a(b, c, d, f) {
          this._node = c;
          this._levels = [];
          this._surfaceRegionAllocator = d;
          this._size = f;
          this._renderer = b;
        }
        a.prototype.getLevel = function(a) {
          a = Math.max(a.getAbsoluteScaleX(), a.getAbsoluteScaleY());
          var b = 0;
          1 !== a && (b = d(Math.round(Math.log(a) / Math.LN2), -5, 3));
          this._node.hasFlags(2097152) || (b = d(b, -5, 0));
          a = n(b);
          var c = 5 + b, b = this._levels[c];
          if (!b) {
            var f = this._node.getBounds().clone();
            f.scale(a, a);
            f.snap();
            var g = this._surfaceRegionAllocator.allocate(f.w, f.h, null), h = g.region, b = this._levels[c] = new v(g, a), c = new w(g);
            c.clip.set(h);
            c.matrix.setElements(a, 0, 0, a, h.x - f.x, h.y - f.y);
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
      var r = function(a) {
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
      g.Canvas2DRendererOptions = r;
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
      u.createMaxI16();
      var w = function(b) {
        function d(f) {
          b.call(this);
          this.clip = u.createEmpty();
          this.clipList = [];
          this.flags = 0;
          this.target = null;
          this.matrix = a.createIdentity();
          this.colorMatrix = p.ColorMatrix.createIdentity();
          d.allocationCount++;
          this.target = f;
        }
        __extends(d, b);
        d.prototype.set = function(a) {
          this.clip.set(a.clip);
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
          d._dirtyStack.push(this);
        };
        d.prototype.transform = function(a) {
          var b = this.clone();
          b.matrix.preMultiply(a.getMatrix());
          a.hasColorMatrix() && b.colorMatrix.multiply(a.getColorMatrix());
          return b;
        };
        d.prototype.hasFlags = function(a) {
          return(this.flags & a) === a;
        };
        d.prototype.removeFlags = function(a) {
          this.flags &= ~a;
        };
        d.prototype.toggleFlags = function(a, b) {
          this.flags = b ? this.flags | a : this.flags & ~a;
        };
        d.allocationCount = 0;
        d._dirtyStack = [];
        return d;
      }(p.State);
      g.RenderState = w;
      var t = function() {
        function b() {
          this.culledNodes = this.groups = this.shapes = this._count = 0;
        }
        b.prototype.enter = function(a) {
          this._count++;
          f && (f.enter("> Frame: " + this._count), this._enterTime = performance.now(), this.culledNodes = this.groups = this.shapes = 0);
        };
        b.prototype.leave = function() {
          f && (f.writeLn("Shapes: " + this.shapes + ", Groups: " + this.groups + ", Culled Nodes: " + this.culledNodes), f.writeLn("Elapsed: " + (performance.now() - this._enterTime).toFixed(2)), f.writeLn("Rectangle: " + u.allocationCount + ", Matrix: " + a.allocationCount + ", State: " + w.allocationCount), f.leave("<"));
        };
        return b;
      }();
      g.FrameInfo = t;
      var l = function(c) {
        function d(a, b, f) {
          void 0 === f && (f = new r);
          c.call(this, a, b, f);
          this._visited = 0;
          this._frameInfo = new t;
          this._fontSize = 0;
          this._layers = [];
          if (a instanceof HTMLCanvasElement) {
            var g = a;
            this._viewport = new u(0, 0, g.width, g.height);
            this._target = this._createTarget(g);
          } else {
            this._addLayer("Background Layer");
            f = this._addLayer("Canvas Layer");
            g = document.createElement("canvas");
            f.appendChild(g);
            this._viewport = new u(0, 0, a.scrollWidth, a.scrollHeight);
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
          return new g.Canvas2DSurfaceRegion(new g.Canvas2DSurface(a), new p.RegionAllocator.Region(0, 0, a.width, a.height), a.width, a.height);
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
          d._initializedCaches || (d._surfaceCache = new p.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            var d = Math.max(1024, a), e = Math.max(1024, b);
            c.width = d;
            c.height = e;
            var f = null, f = 512 <= a || 512 <= b ? new p.RegionAllocator.GridAllocator(d, e, d, e) : new p.RegionAllocator.BucketAllocator(d, e);
            return new g.Canvas2DSurface(c, f);
          }), d._shapeCache = new p.SurfaceRegionAllocator.SimpleAllocator(function(a, b) {
            var c = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(c);
            c.width = 1024;
            c.height = 1024;
            var d = d = new p.RegionAllocator.CompactAllocator(1024, 1024);
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
          var d = new w(this._target);
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
          var c = b.matrix, f = a.getBounds();
          if (f.isEmpty()) {
            return!1;
          }
          var g = this._options.cacheShapesMaxSize, h = Math.max(c.getAbsoluteScaleX(), c.getAbsoluteScaleY()), k = !!(b.flags & 16), l = !!(b.flags & 8);
          if (b.hasFlags(256)) {
            if (l || k || !b.colorMatrix.isIdentity() || a.hasFlags(1048576) || 100 < this._options.cacheShapesThreshold || f.w * h > g || f.h * h > g) {
              return!1;
            }
            (h = a.properties.mipMap) || (h = a.properties.mipMap = new m(this, a, d._shapeCache, g));
            k = h.getLevel(c);
            g = k.surfaceRegion;
            h = g.region;
            return k ? (k = b.target.context, k.imageSmoothingEnabled = k.mozImageSmoothingEnabled = !0, k.setTransform(c.a, c.b, c.c, c.d, c.tx, c.ty), k.drawImage(g.surface.canvas, h.x, h.y, h.w, h.h, f.x, f.y, f.w, f.h), !0) : !1;
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
                  var f = d[e], g = b.transform(f.getTransform());
                  g.toggleFlags(4096, f.hasFlags(524288));
                  if (0 <= f.clip) {
                    c = c || new Uint8Array(d.length);
                    c[f.clip + e]++;
                    var h = g.clone();
                    b.target.context.save();
                    h.flags |= 16;
                    f.visit(this, h);
                    h.free();
                  } else {
                    f.visit(this, g);
                  }
                  if (c && 0 < c[e]) {
                    for (;c[e]--;) {
                      b.target.context.restore();
                    }
                  }
                  g.free();
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
            var c = b.target.context, f = a.getBounds(!0), g = a.properties.style;
            g || (g = a.properties.style = k.Color.randomColor().toCSSStyle());
            c.strokeStyle = g;
            b.matrix.transformRectangleAABB(f);
            c.setTransform(1, 0, 0, 1, 0, 0);
            f.free();
            f = a.getBounds();
            g = d._debugPoints;
            b.matrix.transformRectangle(f, g);
            c.lineWidth = 1;
            c.beginPath();
            c.moveTo(g[0].x, g[0].y);
            c.lineTo(g[1].x, g[1].y);
            c.lineTo(g[2].x, g[2].y);
            c.lineTo(g[3].x, g[3].y);
            c.lineTo(g[0].x, g[0].y);
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
            a.source instanceof p.RenderableVideo ? this.visitRenderableVideo(a.source, b) : 0 < d.globalAlpha && this.visitRenderable(a.source, b, a.ratio);
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
            f !== a.video.parentElement && (f.appendChild(a.video), a.addEventListener(2, function ca(a) {
              f.removeChild(a.video);
              a.removeEventListener(2, ca);
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
            b = a.properties.renderCount || 0;
            a.properties.renderCount = ++b;
            a.render(d, c, null, f, g);
          }
        };
        d.prototype._renderLayer = function(a, b) {
          var c = a.getLayer(), d = c.mask;
          if (d) {
            this._renderWithMask(a, d, c.blendMode, !a.hasFlags(131072) || !d.hasFlags(131072), b);
          } else {
            var d = u.allocate(), e = this._renderToTemporarySurface(a, b, d, null);
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
              var k = e.clip.clone();
              k.intersect(g);
              k.intersect(h);
              k.snap();
              k.isEmpty() || (g = e.clone(), g.clip.set(k), a = this._renderToTemporarySurface(a, g, u.createEmpty(), null), g.free(), g = e.clone(), g.clip.set(k), g.matrix = f, g.flags |= 4, d && (g.flags |= 8), b = this._renderToTemporarySurface(b, g, u.createEmpty(), a.surface), g.free(), a.draw(b, 0, 0, k.w, k.h, 11), e.target.draw(a, k.x, k.y, k.w, k.h, c), b.free(), a.free());
            }
          }
        };
        d.prototype._renderStageToTarget = function(b, c, d) {
          u.allocationCount = a.allocationCount = w.allocationCount = 0;
          b = new w(b);
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
          f = new u(f.x, f.y, c.w, c.h);
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
            b(d instanceof p.Stage);
            a = d.content.getBounds(!0);
            d.content.getTransform().getConcatenatedMatrix().transformRectangleAABB(a);
            a.intersect(this._viewport);
          }
          a || (a = new u(0, 0, this._target.w, this._target.h));
          d = document.createElement("canvas");
          d.width = a.w;
          d.height = a.h;
          var e = d.getContext("2d");
          e.fillStyle = this._container.style.backgroundColor;
          e.fillRect(0, 0, a.w, a.h);
          e.drawImage(this._target.context.canvas, a.x, a.y, a.w, a.h, 0, 0, a.w, a.h);
          return new p.ScreenShot(d.toDataURL("image/png"), a.w, a.h);
        };
        d._initializedCaches = !1;
        d._debugPoints = h.createEmptyPoints(4);
        return d;
      }(p.Renderer);
      g.Canvas2DRenderer = l;
    })(p.Canvas2D || (p.Canvas2D = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var g = p.Geometry.Point, b = p.Geometry.Matrix, u = p.Geometry.Rectangle, h = k.Tools.Mini.FPS, a = function() {
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
    p.UIState = a;
    var d = function(a) {
      function b() {
        a.apply(this, arguments);
        this._keyCodes = [];
      }
      __extends(b, a);
      b.prototype.onMouseDown = function(a, b) {
        b.altKey && (a.state = new f(a.worldView, a.getMousePosition(b, null), a.worldView.getTransform().getMatrix(!0)));
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
    }(a), n = function(a) {
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
        var d = "DOMMouseScroll" === b.type ? -b.detail : b.wheelDelta / 40;
        if (b.altKey) {
          b.preventDefault();
          var f = a.getMousePosition(b, null), c = a.worldView.getTransform().getMatrix(!0), d = 1 + d / 1E3;
          c.translate(-f.x, -f.y);
          c.scale(d, d);
          c.translate(f.x, f.y);
          a.worldView.getTransform().setMatrix(c);
        }
      };
      b.prototype.onKeyPress = function(a, b) {
        if (b.altKey) {
          var d = b.keyCode || b.which;
          console.info("onKeyPress Code: " + d);
          switch(d) {
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
          var b = p.viewportLoupeDiameter.value, d = p.viewportLoupeDiameter.value;
          a.viewport = new u(this._mousePosition.x - b / 2, this._mousePosition.y - d / 2, b, d);
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
          var d = a._world;
          d && (a.state = new f(d, a.getMousePosition(b, null), d.getTransform().getMatrix(!0)));
        }
      };
      b.prototype.onMouseUp = function(a, b) {
        a.state = new d;
        a.selectNodeUnderMouse(b);
      };
      return b;
    })(a);
    var f = function(a) {
      function b(d, f, g) {
        a.call(this);
        this._target = d;
        this._startPosition = f;
        this._startMatrix = g;
      }
      __extends(b, a);
      b.prototype.onMouseMove = function(a, b) {
        b.preventDefault();
        var d = a.getMousePosition(b, null);
        d.sub(this._startPosition);
        this._target.getTransform().setMatrix(this._startMatrix.clone().translate(d.x, d.y));
        a.state = this;
      };
      b.prototype.onMouseUp = function(a, b) {
        a.state = new d;
      };
      return b;
    }(a), a = function() {
      function a(b, f, g) {
        function t(a) {
          e._state.onMouseWheel(e, a);
          e._persistentState.onMouseWheel(e, a);
        }
        void 0 === f && (f = !1);
        void 0 === g && (g = void 0);
        this._state = new d;
        this._persistentState = new n;
        this.paused = !1;
        this.viewport = null;
        this._selectedNodes = [];
        this._eventListeners = Object.create(null);
        this._fullScreen = !1;
        this._container = b;
        this._stage = new p.Stage(512, 512, !0);
        this._worldView = this._stage.content;
        this._world = new p.Group;
        this._worldView.addChild(this._world);
        this._disableHiDPI = f;
        f = document.createElement("div");
        f.style.position = "absolute";
        f.style.width = "100%";
        f.style.height = "100%";
        f.style.zIndex = "0";
        b.appendChild(f);
        if (p.hud.value) {
          var l = document.createElement("div");
          l.style.position = "absolute";
          l.style.width = "100%";
          l.style.height = "100%";
          l.style.pointerEvents = "none";
          var c = document.createElement("div");
          c.style.position = "absolute";
          c.style.width = "100%";
          c.style.height = "20px";
          c.style.pointerEvents = "none";
          l.appendChild(c);
          b.appendChild(l);
          this._fps = new h(c);
        } else {
          this._fps = null;
        }
        this.transparent = l = 0 === g;
        void 0 === g || 0 === g || k.ColorUtilities.rgbaToCSSStyle(g);
        this._options = new p.Canvas2D.Canvas2DRendererOptions;
        this._options.alpha = l;
        this._renderer = new p.Canvas2D.Canvas2DRenderer(f, this._stage, this._options);
        this._listenForContainerSizeChanges();
        this._onMouseUp = this._onMouseUp.bind(this);
        this._onMouseDown = this._onMouseDown.bind(this);
        this._onMouseMove = this._onMouseMove.bind(this);
        var e = this;
        window.addEventListener("mouseup", function(a) {
          e._state.onMouseUp(e, a);
          e._render();
        }, !1);
        window.addEventListener("mousemove", function(a) {
          e._state.onMouseMove(e, a);
          e._persistentState.onMouseMove(e, a);
        }, !1);
        window.addEventListener("DOMMouseScroll", t);
        window.addEventListener("mousewheel", t);
        b.addEventListener("mousedown", function(a) {
          e._state.onMouseDown(e, a);
        });
        window.addEventListener("keydown", function(a) {
          e._state.onKeyDown(e, a);
          if (e._state !== e._persistentState) {
            e._persistentState.onKeyDown(e, a);
          }
        }, !1);
        window.addEventListener("keypress", function(a) {
          e._state.onKeyPress(e, a);
          if (e._state !== e._persistentState) {
            e._persistentState.onKeyPress(e, a);
          }
        }, !1);
        window.addEventListener("keyup", function(a) {
          e._state.onKeyUp(e, a);
          if (e._state !== e._persistentState) {
            e._persistentState.onKeyUp(e, a);
          }
        }, !1);
        this._enterRenderLoop();
      }
      a.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var d = this;
        setInterval(function() {
          if (a !== d._containerWidth || b !== d._containerHeight) {
            d._onContainerSizeChanged(), a = d._containerWidth, b = d._containerHeight;
          }
        }, 10);
      };
      a.prototype._onContainerSizeChanged = function() {
        var a = this.getRatio(), d = Math.ceil(this._containerWidth * a), f = Math.ceil(this._containerHeight * a);
        this._stage.setBounds(new u(0, 0, d, f));
        this._stage.content.setBounds(new u(0, 0, d, f));
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
      a.prototype._enterRenderLoop = function() {
        var a = this;
        requestAnimationFrame(function w() {
          a.render();
          requestAnimationFrame(w);
        });
      };
      Object.defineProperty(a.prototype, "state", {set:function(a) {
        this._state = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(a.prototype, "cursor", {set:function(a) {
        this._container.style.cursor = a;
      }, enumerable:!0, configurable:!0});
      a.prototype._render = function() {
        p.RenderableVideo.checkForVideoUpdates();
        var a = (this._stage.readyToRender() || p.forcePaint.value) && !this.paused, b = 0;
        if (a) {
          var d = this._renderer;
          d.viewport = this.viewport ? this.viewport : this._stage.getBounds();
          this._dispatchEvent("render");
          b = performance.now();
          d.render();
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
      a.prototype.getMousePosition = function(a, d) {
        var f = this._container, h = f.getBoundingClientRect(), k = this.getRatio(), f = new g(f.scrollWidth / h.width * (a.clientX - h.left) * k, f.scrollHeight / h.height * (a.clientY - h.top) * k);
        if (!d) {
          return f;
        }
        h = b.createIdentity();
        d.getTransform().getConcatenatedMatrix().inverse(h);
        h.transformPoint(f);
        return f;
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
    p.Easel = a;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var g = k.GFX.Geometry.Matrix;
    (function(b) {
      b[b.Simple = 0] = "Simple";
    })(p.Layout || (p.Layout = {}));
    var b = function(b) {
      function a() {
        b.apply(this, arguments);
        this.layout = 0;
      }
      __extends(a, b);
      return a;
    }(p.RendererOptions);
    p.TreeRendererOptions = b;
    var u = function(h) {
      function a(a, g, f) {
        void 0 === f && (f = new b);
        h.call(this, a, g, f);
        this._canvas = document.createElement("canvas");
        this._container.appendChild(this._canvas);
        this._context = this._canvas.getContext("2d");
        this._listenForContainerSizeChanges();
      }
      __extends(a, h);
      a.prototype._listenForContainerSizeChanges = function() {
        var a = this._containerWidth, b = this._containerHeight;
        this._onContainerSizeChanged();
        var f = this;
        setInterval(function() {
          if (a !== f._containerWidth || b !== f._containerHeight) {
            f._onContainerSizeChanged(), a = f._containerWidth, b = f._containerHeight;
          }
        }, 10);
      };
      a.prototype._getRatio = function() {
        var a = window.devicePixelRatio || 1, b = 1;
        1 !== a && (b = a / 1);
        return b;
      };
      a.prototype._onContainerSizeChanged = function() {
        var a = this._getRatio(), b = Math.ceil(this._containerWidth * a), f = Math.ceil(this._containerHeight * a), g = this._canvas;
        0 < a ? (g.width = b * a, g.height = f * a, g.style.width = b + "px", g.style.height = f + "px") : (g.width = b, g.height = f);
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
      a.prototype._renderNodeSimple = function(a, b, f) {
        function g(b) {
          var e = b.getChildren();
          a.fillStyle = b.hasFlags(16) ? "red" : "white";
          var f = String(b.id);
          b instanceof p.RenderableText ? f = "T" + f : b instanceof p.RenderableShape ? f = "S" + f : b instanceof p.RenderableBitmap ? f = "B" + f : b instanceof p.RenderableVideo && (f = "V" + f);
          b instanceof p.Renderable && (f = f + " [" + b._parents.length + "]");
          b = a.measureText(f).width;
          a.fillText(f, k, u);
          if (e) {
            k += b + 4;
            l = Math.max(l, k + 20);
            for (f = 0;f < e.length;f++) {
              g(e[f]), f < e.length - 1 && (u += 18, u > h._canvas.height && (a.fillStyle = "gray", k = k - t + l + 8, t = l + 8, u = 0, a.fillStyle = "white"));
            }
            k -= b + 4;
          }
        }
        var h = this;
        a.save();
        a.font = "16px Arial";
        a.fillStyle = "white";
        var k = 0, u = 0, t = 0, l = 0;
        g(b);
        a.restore();
      };
      return a;
    }(p.Renderer);
    p.TreeRenderer = u;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.GFX.BlurFilter, u = k.GFX.DropshadowFilter, h = k.GFX.Shape, a = k.GFX.Group, d = k.GFX.RenderableShape, n = k.GFX.RenderableMorphShape, f = k.GFX.RenderableBitmap, v = k.GFX.RenderableVideo, m = k.GFX.RenderableText, r = k.GFX.ColorMatrix, w = k.ShapeData, t = k.ArrayUtilities.DataBuffer, l = k.GFX.Stage, c = k.GFX.Geometry.Matrix, e = k.GFX.Geometry.Rectangle, q = function() {
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
            f.setBounds(a);
          }
          var f = this.stage = new l(128, 512);
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
          k.registerCSSFont(a, b.data, !inFirefox);
          inFirefox ? c(null) : window.setTimeout(c, 400);
        };
        a.prototype.registerImage = function(a, b, c, d) {
          this._registerAsset(a, b, this._decodeImage(c.dataType, c.data, d));
        };
        a.prototype.registerVideo = function(a) {
          this._registerAsset(a, 0, new v(a, this));
        };
        a.prototype._decodeImage = function(a, b, c) {
          var d = new Image, g = f.FromImage(d);
          d.src = URL.createObjectURL(new Blob([b], {type:k.getMIMETypeForImageType(a)}));
          d.onload = function() {
            g.setBounds(new e(0, 0, d.width, d.height));
            g.invalidate();
            c({width:d.width, height:d.height});
          };
          d.onerror = function() {
            c(null);
          };
          return g;
        };
        a.prototype.sendVideoPlaybackEvent = function(a, b, c) {
          this._easelHost.sendVideoPlaybackEvent(a, b, c);
        };
        return a;
      }();
      g.GFXChannelDeserializerContext = q;
      q = function() {
        function g() {
        }
        g.prototype.read = function() {
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
        g.prototype._readMatrix = function() {
          var a = this.input, b = g._temporaryReadMatrix;
          b.setElements(a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat() / 20, a.readFloat() / 20);
          return b;
        };
        g.prototype._readRectangle = function() {
          var a = this.input, b = g._temporaryReadRectangle;
          b.setElements(a.readInt() / 20, a.readInt() / 20, a.readInt() / 20, a.readInt() / 20);
          return b;
        };
        g.prototype._readColorMatrix = function() {
          var a = this.input, b = g._temporaryReadColorMatrix, c = 1, d = 1, e = 1, f = 1, h = 0, k = 0, l = 0, m = 0;
          switch(a.readInt()) {
            case 0:
              return g._temporaryReadColorMatrixIdentity;
            case 1:
              f = a.readFloat();
              break;
            case 2:
              c = a.readFloat(), d = a.readFloat(), e = a.readFloat(), f = a.readFloat(), h = a.readInt(), k = a.readInt(), l = a.readInt(), m = a.readInt();
          }
          b.setMultipliersAndOffsets(c, d, e, f, h, k, l, m);
          return b;
        };
        g.prototype._readAsset = function() {
          var a = this.input.readInt(), b = this.inputAssets[a];
          this.inputAssets[a] = null;
          return b;
        };
        g.prototype._readUpdateGraphics = function() {
          for (var a = this.input, b = this.context, c = a.readInt(), e = a.readInt(), f = b._getAsset(c), g = this._readRectangle(), h = w.FromPlainObject(this._readAsset()), k = a.readInt(), l = [], m = 0;m < k;m++) {
            var p = a.readInt();
            l.push(b._getBitmapAsset(p));
          }
          if (f) {
            f.update(h, l, g);
          } else {
            a = h.morphCoordinates ? new n(c, h, l, g) : new d(c, h, l, g);
            for (m = 0;m < l.length;m++) {
              l[m] && l[m].addRenderableParent(a);
            }
            b._registerAsset(c, e, a);
          }
        };
        g.prototype._readUpdateBitmapData = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getBitmapAsset(c), g = this._readRectangle(), a = a.readInt(), h = t.FromPlainObject(this._readAsset());
          e ? e.updateFromDataBuffer(a, h) : (e = f.FromDataBuffer(a, h, g), b._registerAsset(c, d, e));
        };
        g.prototype._readUpdateTextContent = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getTextAsset(c), f = this._readRectangle(), g = this._readMatrix(), h = a.readInt(), k = a.readInt(), l = a.readInt(), n = a.readBoolean(), p = a.readInt(), q = a.readInt(), r = this._readAsset(), s = t.FromPlainObject(this._readAsset()), v = null, u = a.readInt();
          u && (v = new t(4 * u), a.readBytes(v, 4 * u));
          e ? (e.setBounds(f), e.setContent(r, s, g, v), e.setStyle(h, k, p, q), e.reflow(l, n)) : (e = new m(f), e.setContent(r, s, g, v), e.setStyle(h, k, p, q), e.reflow(l, n), b._registerAsset(c, d, e));
          if (this.output) {
            for (a = e.textRect, this.output.writeInt(20 * a.w), this.output.writeInt(20 * a.h), this.output.writeInt(20 * a.x), e = e.lines, a = e.length, this.output.writeInt(a), b = 0;b < a;b++) {
              this._writeLineMetrics(e[b]);
            }
          }
        };
        g.prototype._writeLineMetrics = function(a) {
          this.output.writeInt(a.x);
          this.output.writeInt(a.width);
          this.output.writeInt(a.ascent);
          this.output.writeInt(a.descent);
          this.output.writeInt(a.leading);
        };
        g.prototype._readUpdateStage = function() {
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
        g.prototype._readUpdateNetStream = function() {
          var a = this.context, b = this.input.readInt(), c = a._getVideoAsset(b), d = this._readRectangle();
          c || (a.registerVideo(b), c = a._getVideoAsset(b));
          c.setBounds(d);
        };
        g.prototype._readFilters = function(a) {
          var c = this.input, d = c.readInt(), e = [];
          if (d) {
            for (var f = 0;f < d;f++) {
              var g = c.readInt();
              switch(g) {
                case 0:
                  e.push(new b(c.readFloat(), c.readFloat(), c.readInt()));
                  break;
                case 1:
                  e.push(new u(c.readFloat(), c.readFloat(), c.readFloat(), c.readFloat(), c.readInt(), c.readFloat(), c.readBoolean(), c.readBoolean(), c.readBoolean(), c.readInt(), c.readFloat()));
                  break;
                default:
                  k.Debug.somewhatImplemented(p.FilterType[g]);
              }
            }
            a.getLayer().filters = e;
          }
        };
        g.prototype._readUpdateFrame = function() {
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
            for (var k = 0;k < d;k++) {
              var l = b.readInt(), l = c._makeNode(l);
              g.addChild(l);
            }
          }
          e && (l = f.getChildren()[0], l instanceof h && (l.ratio = e));
        };
        g.prototype._readDrawToBitmap = function() {
          var a = this.input, b = this.context, d = a.readInt(), e = a.readInt(), g = a.readInt(), h, k, l;
          h = g & 1 ? this._readMatrix().clone() : c.createIdentity();
          g & 8 && (k = this._readColorMatrix());
          g & 16 && (l = this._readRectangle());
          g = a.readInt();
          a.readBoolean();
          a = b._getBitmapAsset(d);
          e = b._makeNode(e);
          a ? a.drawNode(e, h, k, g, l) : b._registerAsset(d, -1, f.FromNode(e, h, k, g, l));
        };
        g.prototype._readRequestBitmapData = function() {
          var a = this.output, b = this.context, c = this.input.readInt();
          b._getBitmapAsset(c).readImageData(a);
        };
        g._temporaryReadMatrix = c.createIdentity();
        g._temporaryReadRectangle = e.createEmpty();
        g._temporaryReadColorMatrix = r.createIdentity();
        g._temporaryReadColorMatrixIdentity = r.createIdentity();
        return g;
      }();
      g.GFXChannelDeserializer = q;
    })(p.GFX || (p.GFX = {}));
  })(k.Remoting || (k.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    var g = k.GFX.Geometry.Point, b = k.ArrayUtilities.DataBuffer, u = function() {
      function h(a) {
        this._easel = a;
        var b = a.transparent;
        this._group = a.world;
        this._content = null;
        this._fullscreen = !1;
        this._context = new k.Remoting.GFX.GFXChannelDeserializerContext(this, this._group, b);
        this._addEventListeners();
      }
      h.prototype.onSendUpdates = function(a, b) {
        throw Error("This method is abstract");
      };
      Object.defineProperty(h.prototype, "easel", {get:function() {
        return this._easel;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "stage", {get:function() {
        return this._easel.stage;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "content", {set:function(a) {
        this._content = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "cursor", {set:function(a) {
        this._easel.cursor = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "fullscreen", {set:function(a) {
        if (this._fullscreen !== a) {
          this._fullscreen = a;
          var b = window.FirefoxCom;
          b && b.request("setFullscreen", a, null);
        }
      }, enumerable:!0, configurable:!0});
      h.prototype._mouseEventListener = function(a) {
        var d = this._easel.getMousePosition(a, this._content), d = new g(d.x, d.y), h = new b, f = new k.Remoting.GFX.GFXChannelSerializer;
        f.output = h;
        f.writeMouseEvent(a, d);
        this.onSendUpdates(h, []);
      };
      h.prototype._keyboardEventListener = function(a) {
        var d = new b, g = new k.Remoting.GFX.GFXChannelSerializer;
        g.output = d;
        g.writeKeyboardEvent(a);
        this.onSendUpdates(d, []);
      };
      h.prototype._addEventListeners = function() {
        for (var a = this._mouseEventListener.bind(this), b = this._keyboardEventListener.bind(this), g = h._mouseEvents, f = 0;f < g.length;f++) {
          window.addEventListener(g[f], a);
        }
        a = h._keyboardEvents;
        for (f = 0;f < a.length;f++) {
          window.addEventListener(a[f], b);
        }
        this._addFocusEventListeners();
        this._easel.addEventListener("resize", this._resizeEventListener.bind(this));
      };
      h.prototype._sendFocusEvent = function(a) {
        var d = new b, g = new k.Remoting.GFX.GFXChannelSerializer;
        g.output = d;
        g.writeFocusEvent(a);
        this.onSendUpdates(d, []);
      };
      h.prototype._addFocusEventListeners = function() {
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
      h.prototype._resizeEventListener = function() {
        this.onDisplayParameters(this._easel.getDisplayParameters());
      };
      h.prototype.onDisplayParameters = function(a) {
        throw Error("This method is abstract");
      };
      h.prototype.processUpdates = function(a, b, g) {
        void 0 === g && (g = null);
        var f = new k.Remoting.GFX.GFXChannelDeserializer;
        f.input = a;
        f.inputAssets = b;
        f.output = g;
        f.context = this._context;
        f.read();
      };
      h.prototype.processExternalCommand = function(a) {
        if ("isEnabled" === a.action) {
          a.result = !1;
        } else {
          throw Error("This command is not supported");
        }
      };
      h.prototype.processVideoControl = function(a, b, g) {
        var f = this._context, h = f._getVideoAsset(a);
        if (!h) {
          if (1 !== b) {
            return;
          }
          f.registerVideo(a);
          h = f._getVideoAsset(a);
        }
        return h.processControlRequest(b, g);
      };
      h.prototype.processRegisterFontOrImage = function(a, b, g, f, h) {
        "font" === g ? this._context.registerFont(a, f, h) : this._context.registerImage(a, b, f, h);
      };
      h.prototype.processFSCommand = function(a, b) {
      };
      h.prototype.processFrame = function() {
      };
      h.prototype.onExernalCallback = function(a) {
        throw Error("This method is abstract");
      };
      h.prototype.sendExernalCallback = function(a, b) {
        var g = {functionName:a, args:b};
        this.onExernalCallback(g);
        if (g.error) {
          throw Error(g.error);
        }
        return g.result;
      };
      h.prototype.onVideoPlaybackEvent = function(a, b, g) {
        throw Error("This method is abstract");
      };
      h.prototype.sendVideoPlaybackEvent = function(a, b, g) {
        this.onVideoPlaybackEvent(a, b, g);
      };
      h._mouseEvents = k.Remoting.MouseEventNames;
      h._keyboardEvents = k.Remoting.KeyboardEventNames;
      return h;
    }();
    p.EaselHost = u;
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.ArrayUtilities.DataBuffer, u = k.CircularBuffer, h = k.Tools.Profiler.TimelineBuffer, a = function(a) {
        function g(b, h, k) {
          a.call(this, b);
          this._timelineRequests = Object.create(null);
          this._playerWindow = h;
          this._window = k;
          this._window.addEventListener("message", function(a) {
            this.onWindowMessage(a.data);
          }.bind(this));
          this._window.addEventListener("syncmessage", function(a) {
            this.onWindowMessage(a.detail, !1);
          }.bind(this));
        }
        __extends(g, a);
        g.prototype.onSendUpdates = function(a, b) {
          var d = a.getBytes();
          this._playerWindow.postMessage({type:"gfx", updates:d, assets:b}, "*", [d.buffer]);
        };
        g.prototype.onExernalCallback = function(a) {
          var b = this._playerWindow.document.createEvent("CustomEvent");
          b.initCustomEvent("syncmessage", !1, !1, {type:"externalCallback", request:a});
          this._playerWindow.dispatchEvent(b);
        };
        g.prototype.onDisplayParameters = function(a) {
          this._playerWindow.postMessage({type:"displayParameters", params:a}, "*");
        };
        g.prototype.onVideoPlaybackEvent = function(a, b, d) {
          var g = this._playerWindow.document.createEvent("CustomEvent");
          g.initCustomEvent("syncmessage", !1, !1, {type:"videoPlayback", id:a, eventType:b, data:d});
          this._playerWindow.dispatchEvent(g);
        };
        g.prototype.requestTimeline = function(a, b) {
          return new Promise(function(d) {
            this._timelineRequests[a] = d;
            this._playerWindow.postMessage({type:"timeline", cmd:b, request:a}, "*");
          }.bind(this));
        };
        g.prototype.onWindowMessage = function(a, d) {
          void 0 === d && (d = !0);
          if ("object" === typeof a && null !== a) {
            if ("player" === a.type) {
              var g = b.FromArrayBuffer(a.updates.buffer);
              if (d) {
                this.processUpdates(g, a.assets);
              } else {
                var k = new b;
                this.processUpdates(g, a.assets, k);
                a.result = k.toPlainObject();
              }
            } else {
              "frame" !== a.type && ("external" === a.type ? this.processExternalCommand(a.request) : "videoControl" === a.type ? a.result = this.processVideoControl(a.id, a.eventType, a.data) : "registerFontOrImage" === a.type ? this.processRegisterFontOrImage(a.syncId, a.symbolId, a.assetType, a.data, a.resolve) : "fscommand" !== a.type && "timelineResponse" === a.type && a.timeline && (a.timeline.__proto__ = h.prototype, a.timeline._marks.__proto__ = u.prototype, a.timeline._times.__proto__ = 
              u.prototype, this._timelineRequests[a.request](a.timeline)));
            }
          }
        };
        return g;
      }(p.EaselHost);
      g.WindowEaselHost = a;
    })(p.Window || (p.Window = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
(function(k) {
  (function(p) {
    (function(g) {
      var b = k.ArrayUtilities.DataBuffer, u = function(g) {
        function a(a) {
          g.call(this, a);
          this._worker = k.Player.Test.FakeSyncWorker.instance;
          this._worker.addEventListener("message", this._onWorkerMessage.bind(this));
          this._worker.addEventListener("syncmessage", this._onSyncWorkerMessage.bind(this));
        }
        __extends(a, g);
        a.prototype.onSendUpdates = function(a, b) {
          var f = a.getBytes();
          this._worker.postMessage({type:"gfx", updates:f, assets:b}, [f.buffer]);
        };
        a.prototype.onExernalCallback = function(a) {
          this._worker.postSyncMessage({type:"externalCallback", request:a});
        };
        a.prototype.onDisplayParameters = function(a) {
          this._worker.postMessage({type:"displayParameters", params:a});
        };
        a.prototype.onVideoPlaybackEvent = function(a, b, f) {
          this._worker.postMessage({type:"videoPlayback", id:a, eventType:b, data:f});
        };
        a.prototype.requestTimeline = function(a, b) {
          var f;
          switch(a) {
            case "AVM2":
              f = k.AVM2.timelineBuffer;
              break;
            case "Player":
              f = k.Player.timelineBuffer;
              break;
            case "SWF":
              f = k.SWF.timelineBuffer;
          }
          "clear" === b && f && f.reset();
          return Promise.resolve(f);
        };
        a.prototype._onWorkerMessage = function(a, g) {
          void 0 === g && (g = !0);
          var f = a.data;
          if ("object" === typeof f && null !== f) {
            switch(f.type) {
              case "player":
                var h = b.FromArrayBuffer(f.updates.buffer);
                if (g) {
                  this.processUpdates(h, f.assets);
                } else {
                  var k = new b;
                  this.processUpdates(h, f.assets, k);
                  a.result = k.toPlainObject();
                  a.handled = !0;
                }
                break;
              case "external":
                a.result = this.processExternalCommand(f.command);
                a.handled = !0;
                break;
              case "videoControl":
                a.result = this.processVideoControl(f.id, f.eventType, f.data);
                a.handled = !0;
                break;
              case "registerFontOrImage":
                this.processRegisterFontOrImage(f.syncId, f.symbolId, f.assetType, f.data, f.resolve), a.handled = !0;
            }
          }
        };
        a.prototype._onSyncWorkerMessage = function(a) {
          return this._onWorkerMessage(a, !1);
        };
        return a;
      }(p.EaselHost);
      g.TestEaselHost = u;
    })(p.Test || (p.Test = {}));
  })(k.GFX || (k.GFX = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load GFX Dependencies");

