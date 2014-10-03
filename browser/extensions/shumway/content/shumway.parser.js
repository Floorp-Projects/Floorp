console.time("Load Parser Dependencies");
var jsGlobal = function() {
  return this || (0,eval)("this");
}(), inBrowser = "undefined" != typeof console;
jsGlobal.performance || (jsGlobal.performance = {});
jsGlobal.performance.now || (jsGlobal.performance.now = "undefined" !== typeof dateNow ? dateNow : Date.now);
function log(f) {
  for (var u = 0;u < arguments.length - 1;u++) {
  }
  jsGlobal.print.apply(jsGlobal, arguments);
}
function warn(f) {
  for (var u = 0;u < arguments.length - 1;u++) {
  }
  inBrowser ? console.warn.apply(console, arguments) : jsGlobal.print(Shumway.IndentingWriter.RED + f + Shumway.IndentingWriter.ENDC);
}
var Shumway;
(function(f) {
  function u(b) {
    return(b | 0) === b;
  }
  function c(b) {
    return "object" === typeof b || "function" === typeof b;
  }
  function t(b) {
    return String(Number(b)) === b;
  }
  function m(b) {
    var g = 0;
    if ("number" === typeof b) {
      return g = b | 0, b === g && 0 <= g ? !0 : b >>> 0 === b;
    }
    if ("string" !== typeof b) {
      return!1;
    }
    var a = b.length;
    if (0 === a) {
      return!1;
    }
    if ("0" === b) {
      return!0;
    }
    if (a > f.UINT32_CHAR_BUFFER_LENGTH) {
      return!1;
    }
    var l = 0, g = b.charCodeAt(l++) - 48;
    if (1 > g || 9 < g) {
      return!1;
    }
    for (var s = 0, d = 0;l < a;) {
      d = b.charCodeAt(l++) - 48;
      if (0 > d || 9 < d) {
        return!1;
      }
      s = g;
      g = 10 * g + d;
    }
    return s < f.UINT32_MAX_DIV_10 || s === f.UINT32_MAX_DIV_10 && d <= f.UINT32_MAX_MOD_10 ? !0 : !1;
  }
  (function(b) {
    b[b._0 = 48] = "_0";
    b[b._1 = 49] = "_1";
    b[b._2 = 50] = "_2";
    b[b._3 = 51] = "_3";
    b[b._4 = 52] = "_4";
    b[b._5 = 53] = "_5";
    b[b._6 = 54] = "_6";
    b[b._7 = 55] = "_7";
    b[b._8 = 56] = "_8";
    b[b._9 = 57] = "_9";
  })(f.CharacterCodes || (f.CharacterCodes = {}));
  f.UINT32_CHAR_BUFFER_LENGTH = 10;
  f.UINT32_MAX = 4294967295;
  f.UINT32_MAX_DIV_10 = 429496729;
  f.UINT32_MAX_MOD_10 = 5;
  f.isString = function(b) {
    return "string" === typeof b;
  };
  f.isFunction = function(b) {
    return "function" === typeof b;
  };
  f.isNumber = function(b) {
    return "number" === typeof b;
  };
  f.isInteger = u;
  f.isArray = function(b) {
    return b instanceof Array;
  };
  f.isNumberOrString = function(b) {
    return "number" === typeof b || "string" === typeof b;
  };
  f.isObject = c;
  f.toNumber = function(b) {
    return+b;
  };
  f.isNumericString = t;
  f.isNumeric = function(b) {
    if ("number" === typeof b) {
      return!0;
    }
    if ("string" === typeof b) {
      var g = b.charCodeAt(0);
      return 65 <= g && 90 >= g || 97 <= g && 122 >= g || 36 === g || 95 === g ? !1 : m(b) || t(b);
    }
    return!1;
  };
  f.isIndex = m;
  f.isNullOrUndefined = function(b) {
    return void 0 == b;
  };
  (function(b) {
    b.backtrace = function() {
      try {
        throw Error();
      } catch (b) {
        return b.stack ? b.stack.split("\n").slice(2).join("\n") : "";
      }
    };
    b.error = function(g) {
      inBrowser ? warn(g) : warn(g + "\n\nStack Trace:\n" + b.backtrace());
      throw Error(g);
    };
    b.assert = function(g, l) {
      "undefined" === typeof l && (l = "assertion failed");
      "" === g && (g = !0);
      g || b.error(l.toString());
    };
    b.assertUnreachable = function(b) {
      throw Error("Reached unreachable location " + Error().stack.split("\n")[1] + b);
    };
    b.assertNotImplemented = function(g, l) {
      g || b.error("notImplemented: " + l);
    };
    b.warning = function(b) {
      warn(b);
    };
    b.notUsed = function(g) {
      b.assert(!1, "Not Used " + g);
    };
    b.notImplemented = function(g) {
      log("release: false");
      b.assert(!1, "Not Implemented " + g);
    };
    b.abstractMethod = function(g) {
      b.assert(!1, "Abstract Method " + g);
    };
    var g = {};
    b.somewhatImplemented = function(a) {
      g[a] || (g[a] = !0, b.warning("somewhatImplemented: " + a));
    };
    b.unexpected = function(g) {
      b.assert(!1, "Unexpected: " + g);
    };
    b.untested = function(g) {
      b.warning("Congratulations, you've found a code path for which we haven't found a test case. Please submit the test case: " + g);
    };
  })(f.Debug || (f.Debug = {}));
  var r = f.Debug;
  f.getTicks = function() {
    return performance.now();
  };
  (function(b) {
    function g(b, g) {
      for (var l = 0, a = b.length;l < a;l++) {
        if (b[l] === g) {
          return l;
        }
      }
      b.push(g);
      return b.length - 1;
    }
    var a = f.Debug.assert;
    b.popManyInto = function(b, g, l) {
      a(b.length >= g);
      for (var d = g - 1;0 <= d;d--) {
        l[d] = b.pop();
      }
      l.length = g;
    };
    b.popMany = function(b, g) {
      a(b.length >= g);
      var l = b.length - g, d = b.slice(l, this.length);
      b.splice(l, g);
      return d;
    };
    b.popManyIntoVoid = function(b, g) {
      a(b.length >= g);
      b.length -= g;
    };
    b.pushMany = function(b, g) {
      for (var l = 0;l < g.length;l++) {
        b.push(g[l]);
      }
    };
    b.top = function(b) {
      return b.length && b[b.length - 1];
    };
    b.last = function(b) {
      return b.length && b[b.length - 1];
    };
    b.peek = function(b) {
      a(0 < b.length);
      return b[b.length - 1];
    };
    b.indexOf = function(b, g) {
      for (var l = 0, a = b.length;l < a;l++) {
        if (b[l] === g) {
          return l;
        }
      }
      return-1;
    };
    b.pushUnique = g;
    b.unique = function(b) {
      for (var l = [], a = 0;a < b.length;a++) {
        g(l, b[a]);
      }
      return l;
    };
    b.copyFrom = function(g, l) {
      g.length = 0;
      b.pushMany(g, l);
    };
    b.ensureTypedArrayCapacity = function(b, g) {
      if (b.length < g) {
        var l = b;
        b = new b.constructor(f.IntegerUtilities.nearestPowerOfTwo(g));
        b.set(l, 0);
      }
      return b;
    };
    var l = function() {
      function b(g) {
        "undefined" === typeof g && (g = 16);
        this._f32 = this._i32 = this._u16 = this._u8 = null;
        this._offset = 0;
        this.ensureCapacity(g);
      }
      b.prototype.reset = function() {
        this._offset = 0;
      };
      Object.defineProperty(b.prototype, "offset", {get:function() {
        return this._offset;
      }, enumerable:!0, configurable:!0});
      b.prototype.getIndex = function(b) {
        a(1 === b || 2 === b || 4 === b || 8 === b || 16 === b);
        b = this._offset / b;
        a((b | 0) === b);
        return b;
      };
      b.prototype.ensureAdditionalCapacity = function(b) {
        this.ensureCapacity(this._offset + b);
      };
      b.prototype.ensureCapacity = function(b) {
        if (!this._u8) {
          this._u8 = new Uint8Array(b);
        } else {
          if (this._u8.length > b) {
            return;
          }
        }
        var g = 2 * this._u8.length;
        g < b && (g = b);
        b = new Uint8Array(g);
        b.set(this._u8, 0);
        this._u8 = b;
        this._u16 = new Uint16Array(b.buffer);
        this._i32 = new Int32Array(b.buffer);
        this._f32 = new Float32Array(b.buffer);
      };
      b.prototype.writeInt = function(b) {
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(b);
      };
      b.prototype.writeIntAt = function(b, g) {
        a(0 <= g && g <= this._offset);
        a(0 === (g & 3));
        this.ensureCapacity(g + 4);
        this._i32[g >> 2] = b;
      };
      b.prototype.writeIntUnsafe = function(b) {
        this._i32[this._offset >> 2] = b;
        this._offset += 4;
      };
      b.prototype.writeFloat = function(b) {
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(b);
      };
      b.prototype.writeFloatUnsafe = function(b) {
        this._f32[this._offset >> 2] = b;
        this._offset += 4;
      };
      b.prototype.write4Floats = function(b, g, l, s) {
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(b, g, l, s);
      };
      b.prototype.write4FloatsUnsafe = function(b, g, l, s) {
        var a = this._offset >> 2;
        this._f32[a + 0] = b;
        this._f32[a + 1] = g;
        this._f32[a + 2] = l;
        this._f32[a + 3] = s;
        this._offset += 16;
      };
      b.prototype.write6Floats = function(b, g, l, s, d, e) {
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(b, g, l, s, d, e);
      };
      b.prototype.write6FloatsUnsafe = function(b, g, l, s, a, d) {
        var e = this._offset >> 2;
        this._f32[e + 0] = b;
        this._f32[e + 1] = g;
        this._f32[e + 2] = l;
        this._f32[e + 3] = s;
        this._f32[e + 4] = a;
        this._f32[e + 5] = d;
        this._offset += 24;
      };
      b.prototype.subF32View = function() {
        return this._f32.subarray(0, this._offset >> 2);
      };
      b.prototype.subI32View = function() {
        return this._i32.subarray(0, this._offset >> 2);
      };
      b.prototype.subU16View = function() {
        return this._u16.subarray(0, this._offset >> 1);
      };
      b.prototype.subU8View = function() {
        return this._u8.subarray(0, this._offset);
      };
      b.prototype.hashWords = function(b, g, l) {
        g = this._i32;
        for (var s = 0;s < l;s++) {
          b = (31 * b | 0) + g[s] | 0;
        }
        return b;
      };
      b.prototype.reserve = function(b) {
        b = b + 3 & -4;
        this.ensureCapacity(this._offset + b);
        this._offset += b;
      };
      return b;
    }();
    b.ArrayWriter = l;
  })(f.ArrayUtilities || (f.ArrayUtilities = {}));
  var a = f.ArrayUtilities, d = function() {
    function b(b) {
      this._u8 = new Uint8Array(b);
      this._u16 = new Uint16Array(b);
      this._i32 = new Int32Array(b);
      this._f32 = new Float32Array(b);
      this._offset = 0;
    }
    Object.defineProperty(b.prototype, "offset", {get:function() {
      return this._offset;
    }, enumerable:!0, configurable:!0});
    b.prototype.isEmpty = function() {
      return this._offset === this._u8.length;
    };
    b.prototype.readInt = function() {
      r.assert(0 === (this._offset & 3));
      r.assert(this._offset <= this._u8.length - 4);
      var b = this._i32[this._offset >> 2];
      this._offset += 4;
      return b;
    };
    b.prototype.readFloat = function() {
      r.assert(0 === (this._offset & 3));
      r.assert(this._offset <= this._u8.length - 4);
      var b = this._f32[this._offset >> 2];
      this._offset += 4;
      return b;
    };
    return b;
  }();
  f.ArrayReader = d;
  (function(b) {
    function g(b, g) {
      return Object.prototype.hasOwnProperty.call(b, g);
    }
    function a(b, s) {
      for (var d in s) {
        g(s, d) && (b[d] = s[d]);
      }
    }
    b.boxValue = function(b) {
      return void 0 == b || c(b) ? b : Object(b);
    };
    b.toKeyValueArray = function(b) {
      var g = Object.prototype.hasOwnProperty, a = [], d;
      for (d in b) {
        g.call(b, d) && a.push([d, b[d]]);
      }
      return a;
    };
    b.isPrototypeWriteable = function(b) {
      return Object.getOwnPropertyDescriptor(b, "prototype").writable;
    };
    b.hasOwnProperty = g;
    b.propertyIsEnumerable = function(b, g) {
      return Object.prototype.propertyIsEnumerable.call(b, g);
    };
    b.getOwnPropertyDescriptor = function(b, g) {
      return Object.getOwnPropertyDescriptor(b, g);
    };
    b.hasOwnGetter = function(b, g) {
      var a = Object.getOwnPropertyDescriptor(b, g);
      return!(!a || !a.get);
    };
    b.getOwnGetter = function(b, g) {
      var a = Object.getOwnPropertyDescriptor(b, g);
      return a ? a.get : null;
    };
    b.hasOwnSetter = function(b, g) {
      var a = Object.getOwnPropertyDescriptor(b, g);
      return!(!a || !a.set);
    };
    b.createObject = function(b) {
      return Object.create(b);
    };
    b.createEmptyObject = function() {
      return Object.create(null);
    };
    b.createMap = function() {
      return Object.create(null);
    };
    b.createArrayMap = function() {
      return[];
    };
    b.defineReadOnlyProperty = function(b, g, a) {
      Object.defineProperty(b, g, {value:a, writable:!1, configurable:!0, enumerable:!1});
    };
    b.getOwnPropertyDescriptors = function(g) {
      for (var s = b.createMap(), a = Object.getOwnPropertyNames(g), d = 0;d < a.length;d++) {
        s[a[d]] = Object.getOwnPropertyDescriptor(g, a[d]);
      }
      return s;
    };
    b.cloneObject = function(b) {
      var g = Object.create(Object.getPrototypeOf(b));
      a(g, b);
      return g;
    };
    b.copyProperties = function(b, g) {
      for (var a in g) {
        b[a] = g[a];
      }
    };
    b.copyOwnProperties = a;
    b.copyOwnPropertyDescriptors = function(b, a, d) {
      "undefined" === typeof d && (d = !0);
      for (var e in a) {
        if (g(a, e)) {
          var h = Object.getOwnPropertyDescriptor(a, e);
          if (d || !g(b, e)) {
            r.assert(h);
            try {
              Object.defineProperty(b, e, h);
            } catch (c) {
            }
          }
        }
      }
    };
    b.getLatestGetterOrSetterPropertyDescriptor = function(b, g) {
      for (var a = {};b;) {
        var d = Object.getOwnPropertyDescriptor(b, g);
        d && (a.get = a.get || d.get, a.set = a.set || d.set);
        if (a.get && a.set) {
          break;
        }
        b = Object.getPrototypeOf(b);
      }
      return a;
    };
    b.defineNonEnumerableGetterOrSetter = function(g, a, d, e) {
      var h = b.getLatestGetterOrSetterPropertyDescriptor(g, a);
      h.configurable = !0;
      h.enumerable = !1;
      e ? h.get = d : h.set = d;
      Object.defineProperty(g, a, h);
    };
    b.defineNonEnumerableGetter = function(b, g, a) {
      Object.defineProperty(b, g, {get:a, configurable:!0, enumerable:!1});
    };
    b.defineNonEnumerableSetter = function(b, g, a) {
      Object.defineProperty(b, g, {set:a, configurable:!0, enumerable:!1});
    };
    b.defineNonEnumerableProperty = function(b, g, a) {
      Object.defineProperty(b, g, {value:a, writable:!0, configurable:!0, enumerable:!1});
    };
    b.defineNonEnumerableForwardingProperty = function(b, g, a) {
      Object.defineProperty(b, g, {get:p.makeForwardingGetter(a), set:p.makeForwardingSetter(a), writable:!0, configurable:!0, enumerable:!1});
    };
    b.defineNewNonEnumerableProperty = function(g, a, d) {
      r.assert(!Object.prototype.hasOwnProperty.call(g, a), "Property: " + a + " already exits.");
      b.defineNonEnumerableProperty(g, a, d);
    };
  })(f.ObjectUtilities || (f.ObjectUtilities = {}));
  (function(b) {
    b.makeForwardingGetter = function(b) {
      return new Function('return this["' + b + '"]');
    };
    b.makeForwardingSetter = function(b) {
      return new Function("value", 'this["' + b + '"] = value;');
    };
    b.bindSafely = function(b, a) {
      r.assert(!b.boundTo && a);
      var d = b.bind(a);
      d.boundTo = a;
      return d;
    };
  })(f.FunctionUtilities || (f.FunctionUtilities = {}));
  var p = f.FunctionUtilities;
  (function(b) {
    function g(b) {
      return "string" === typeof b ? '"' + b + '"' : "number" === typeof b || "boolean" === typeof b ? String(b) : b instanceof Array ? "[] " + b.length : typeof b;
    }
    var a = f.Debug.assert;
    b.repeatString = function(b, g) {
      for (var a = "", d = 0;d < g;d++) {
        a += b;
      }
      return a;
    };
    b.memorySizeToString = function(b) {
      b |= 0;
      return 1024 > b ? b + " B" : 1048576 > b ? (b / 1024).toFixed(2) + "KB" : (b / 1048576).toFixed(2) + "MB";
    };
    b.toSafeString = g;
    b.toSafeArrayString = function(b) {
      for (var a = [], d = 0;d < b.length;d++) {
        a.push(g(b[d]));
      }
      return a.join(", ");
    };
    b.utf8decode = function(b) {
      for (var g = new Uint8Array(4 * b.length), a = 0, d = 0, s = b.length;d < s;d++) {
        var l = b.charCodeAt(d);
        if (127 >= l) {
          g[a++] = l;
        } else {
          if (55296 <= l && 56319 >= l) {
            var e = b.charCodeAt(d + 1);
            56320 <= e && 57343 >= e && (l = ((l & 1023) << 10) + (e & 1023) + 65536, ++d);
          }
          0 !== (l & 4292870144) ? (g[a++] = 248 | l >>> 24 & 3, g[a++] = 128 | l >>> 18 & 63, g[a++] = 128 | l >>> 12 & 63, g[a++] = 128 | l >>> 6 & 63) : 0 !== (l & 4294901760) ? (g[a++] = 240 | l >>> 18 & 7, g[a++] = 128 | l >>> 12 & 63, g[a++] = 128 | l >>> 6 & 63) : 0 !== (l & 4294965248) ? (g[a++] = 224 | l >>> 12 & 15, g[a++] = 128 | l >>> 6 & 63) : g[a++] = 192 | l >>> 6 & 31;
          g[a++] = 128 | l & 63;
        }
      }
      return g.subarray(0, a);
    };
    b.utf8encode = function(b) {
      for (var g = 0, a = "";g < b.length;) {
        var d = b[g++] & 255;
        if (127 >= d) {
          a += String.fromCharCode(d);
        } else {
          var s = 192, l = 5;
          do {
            if ((d & (s >> 1 | 128)) === s) {
              break;
            }
            s = s >> 1 | 128;
            --l;
          } while (0 <= l);
          if (0 >= l) {
            a += String.fromCharCode(d);
          } else {
            for (var d = d & (1 << l) - 1, s = !1, e = 5;e >= l;--e) {
              var h = b[g++];
              if (128 != (h & 192)) {
                s = !0;
                break;
              }
              d = d << 6 | h & 63;
            }
            if (s) {
              for (l = g - (7 - e);l < g;++l) {
                a += String.fromCharCode(b[l] & 255);
              }
            } else {
              a = 65536 <= d ? a + String.fromCharCode(d - 65536 >> 10 & 1023 | 55296, d & 1023 | 56320) : a + String.fromCharCode(d);
            }
          }
        }
      }
      return a;
    };
    b.base64ArrayBuffer = function(b) {
      var g = "";
      b = new Uint8Array(b);
      for (var a = b.byteLength, d = a % 3, a = a - d, s, l, e, h, c = 0;c < a;c += 3) {
        h = b[c] << 16 | b[c + 1] << 8 | b[c + 2], s = (h & 16515072) >> 18, l = (h & 258048) >> 12, e = (h & 4032) >> 6, h &= 63, g += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[s] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[l] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[e] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[h];
      }
      1 == d ? (h = b[a], g += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 252) >> 2] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 3) << 4] + "==") : 2 == d && (h = b[a] << 8 | b[a + 1], g += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 64512) >> 10] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 1008) >> 4] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(h & 15) << 
      2] + "=");
      return g;
    };
    b.escapeString = function(b) {
      void 0 !== b && (b = b.replace(/[^\w$]/gi, "$"), /^\d/.test(b) && (b = "$" + b));
      return b;
    };
    b.fromCharCodeArray = function(b) {
      for (var g = "", a = 0;a < b.length;a += 16384) {
        var d = Math.min(b.length - a, 16384), g = g + String.fromCharCode.apply(null, b.subarray(a, a + d))
      }
      return g;
    };
    b.variableLengthEncodeInt32 = function(g) {
      var d = 32 - Math.clz32(g);
      a(32 >= d, d);
      for (var s = Math.ceil(d / 6), l = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[s], e = s - 1;0 <= e;e--) {
        l += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[g >> 6 * e & 63];
      }
      a(b.variableLengthDecodeInt32(l) === g, g + " : " + l + " - " + s + " bits: " + d);
      return l;
    };
    b.toEncoding = function(b) {
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[b];
    };
    b.fromEncoding = function(b) {
      b = b.charCodeAt(0);
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
      a(!1, "Invalid Encoding");
    };
    b.variableLengthDecodeInt32 = function(g) {
      for (var a = b.fromEncoding(g[0]), d = 0, l = 0;l < a;l++) {
        var s = 6 * (a - l - 1), d = d | b.fromEncoding(g[1 + l]) << s
      }
      return d;
    };
    b.trimMiddle = function(b, g) {
      if (b.length <= g) {
        return b;
      }
      var a = g >> 1, d = g - a - 1;
      return b.substr(0, a) + "\u2026" + b.substr(b.length - d, d);
    };
    b.multiple = function(b, g) {
      for (var a = "", d = 0;d < g;d++) {
        a += b;
      }
      return a;
    };
    b.indexOfAny = function(b, g, a) {
      for (var d = b.length, l = 0;l < g.length;l++) {
        var s = b.indexOf(g[l], a);
        0 <= s && (d = Math.min(d, s));
      }
      return d === b.length ? -1 : d;
    };
    var d = Array(3), s = Array(4), e = Array(5), h = Array(6), c = Array(7), k = Array(8), p = Array(9);
    b.concat3 = function(b, g, a) {
      d[0] = b;
      d[1] = g;
      d[2] = a;
      return d.join("");
    };
    b.concat4 = function(b, g, a, d) {
      s[0] = b;
      s[1] = g;
      s[2] = a;
      s[3] = d;
      return s.join("");
    };
    b.concat5 = function(b, g, a, d, l) {
      e[0] = b;
      e[1] = g;
      e[2] = a;
      e[3] = d;
      e[4] = l;
      return e.join("");
    };
    b.concat6 = function(b, g, a, d, l, s) {
      h[0] = b;
      h[1] = g;
      h[2] = a;
      h[3] = d;
      h[4] = l;
      h[5] = s;
      return h.join("");
    };
    b.concat7 = function(b, g, a, d, l, s, e) {
      c[0] = b;
      c[1] = g;
      c[2] = a;
      c[3] = d;
      c[4] = l;
      c[5] = s;
      c[6] = e;
      return c.join("");
    };
    b.concat8 = function(b, g, a, d, l, s, e, h) {
      k[0] = b;
      k[1] = g;
      k[2] = a;
      k[3] = d;
      k[4] = l;
      k[5] = s;
      k[6] = e;
      k[7] = h;
      return k.join("");
    };
    b.concat9 = function(b, g, a, d, l, s, e, h, c) {
      p[0] = b;
      p[1] = g;
      p[2] = a;
      p[3] = d;
      p[4] = l;
      p[5] = s;
      p[6] = e;
      p[7] = h;
      p[8] = c;
      return p.join("");
    };
  })(f.StringUtilities || (f.StringUtilities = {}));
  (function(b) {
    var g = new Uint8Array([7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]), a = new Int32Array([-680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426, -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162, 1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632, 
    643717713, -373897302, -701558691, 38016083, -660478335, -405537848, 568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784, 1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556, -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222, -722521979, 76029189, -640364487, -421815835, 530742520, -995338651, -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606, -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649, 
    -145523070, -1120210379, 718787259, -343485551]);
    b.hashBytesTo32BitsMD5 = function(b, d, e) {
      var h = 1732584193, c = -271733879, k = -1732584194, p = 271733878, n = e + 72 & -64, q = new Uint8Array(n), z;
      for (z = 0;z < e;++z) {
        q[z] = b[d++];
      }
      q[z++] = 128;
      for (b = n - 8;z < b;) {
        q[z++] = 0;
      }
      q[z++] = e << 3 & 255;
      q[z++] = e >> 5 & 255;
      q[z++] = e >> 13 & 255;
      q[z++] = e >> 21 & 255;
      q[z++] = e >>> 29 & 255;
      q[z++] = 0;
      q[z++] = 0;
      q[z++] = 0;
      b = new Int32Array(16);
      for (z = 0;z < n;) {
        for (e = 0;16 > e;++e, z += 4) {
          b[e] = q[z] | q[z + 1] << 8 | q[z + 2] << 16 | q[z + 3] << 24;
        }
        var v = h;
        d = c;
        var x = k, t = p, r, m;
        for (e = 0;64 > e;++e) {
          16 > e ? (r = d & x | ~d & t, m = e) : 32 > e ? (r = t & d | ~t & x, m = 5 * e + 1 & 15) : 48 > e ? (r = d ^ x ^ t, m = 3 * e + 5 & 15) : (r = x ^ (d | ~t), m = 7 * e & 15);
          var f = t, v = v + r + a[e] + b[m] | 0;
          r = g[e];
          t = x;
          x = d;
          d = d + (v << r | v >>> 32 - r) | 0;
          v = f;
        }
        h = h + v | 0;
        c = c + d | 0;
        k = k + x | 0;
        p = p + t | 0;
      }
      return h;
    };
    b.hashBytesTo32BitsAdler = function(b, g, a) {
      var d = 1, e = 0;
      for (a = g + a;g < a;++g) {
        d = (d + (b[g] & 255)) % 65521, e = (e + d) % 65521;
      }
      return e << 16 | d;
    };
  })(f.HashUtilities || (f.HashUtilities = {}));
  var e = function() {
    function b() {
    }
    b.seed = function(g) {
      b._state[0] = g;
      b._state[1] = g;
    };
    b.next = function() {
      var b = this._state, a = Math.imul(18273, b[0] & 65535) + (b[0] >>> 16) | 0;
      b[0] = a;
      var d = Math.imul(36969, b[1] & 65535) + (b[1] >>> 16) | 0;
      b[1] = d;
      b = (a << 16) + (d & 65535) | 0;
      return 2.3283064365386963E-10 * (0 > b ? b + 4294967296 : b);
    };
    b._state = new Uint32Array([57005, 48879]);
    return b;
  }();
  f.Random = e;
  Math.random = function() {
    return e.next();
  };
  (function() {
    function b() {
      this.id = "$weakmap" + g++;
    }
    if ("function" !== typeof jsGlobal.WeakMap) {
      var g = 0;
      b.prototype = {has:function(b) {
        return b.hasOwnProperty(this.id);
      }, get:function(b, g) {
        return b.hasOwnProperty(this.id) ? b[this.id] : g;
      }, set:function(b, g) {
        Object.defineProperty(b, this.id, {value:g, enumerable:!1, configurable:!0});
      }};
      jsGlobal.WeakMap = b;
    }
  })();
  d = function() {
    function b() {
      "undefined" !== typeof netscape && netscape.security.PrivilegeManager ? this._map = new WeakMap : this._list = [];
    }
    b.prototype.clear = function() {
      this._map ? this._map.clear() : this._list.length = 0;
    };
    b.prototype.push = function(b) {
      this._map ? this._map.set(b, null) : this._list.push(b);
    };
    b.prototype.forEach = function(b) {
      if (this._map) {
        "undefined" !== typeof netscape && netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect"), Components.utils.nondeterministicGetWeakMapKeys(this._map).forEach(function(a) {
          0 !== a._referenceCount && b(a);
        });
      } else {
        for (var a = this._list, d = 0, s = 0;s < a.length;s++) {
          var e = a[s];
          0 === e._referenceCount ? d++ : b(e);
        }
        if (16 < d && d > a.length >> 2) {
          d = [];
          for (s = 0;s < a.length;s++) {
            0 < a[s]._referenceCount && d.push(a[s]);
          }
          this._list = d;
        }
      }
    };
    Object.defineProperty(b.prototype, "length", {get:function() {
      return this._map ? -1 : this._list.length;
    }, enumerable:!0, configurable:!0});
    return b;
  }();
  f.WeakList = d;
  (function(b) {
    b.pow2 = function(b) {
      return b === (b | 0) ? 0 > b ? 1 / (1 << -b) : 1 << b : Math.pow(2, b);
    };
    b.clamp = function(b, a, d) {
      return Math.max(a, Math.min(d, b));
    };
    b.roundHalfEven = function(b) {
      if (.5 === Math.abs(b % 1)) {
        var a = Math.floor(b);
        return 0 === a % 2 ? a : Math.ceil(b);
      }
      return Math.round(b);
    };
    b.epsilonEquals = function(b, a) {
      return 1E-7 > Math.abs(b - a);
    };
  })(f.NumberUtilities || (f.NumberUtilities = {}));
  (function(b) {
    b[b.MaxU16 = 65535] = "MaxU16";
    b[b.MaxI16 = 32767] = "MaxI16";
    b[b.MinI16 = -32768] = "MinI16";
  })(f.Numbers || (f.Numbers = {}));
  (function(b) {
    function g(b) {
      return 256 * b << 16 >> 16;
    }
    var a = new ArrayBuffer(8);
    b.i8 = new Int8Array(a);
    b.u8 = new Uint8Array(a);
    b.i32 = new Int32Array(a);
    b.f32 = new Float32Array(a);
    b.f64 = new Float64Array(a);
    b.nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
    b.floatToInt32 = function(g) {
      b.f32[0] = g;
      return b.i32[0];
    };
    b.int32ToFloat = function(g) {
      b.i32[0] = g;
      return b.f32[0];
    };
    b.swap16 = function(b) {
      return(b & 255) << 8 | b >> 8 & 255;
    };
    b.swap32 = function(b) {
      return(b & 255) << 24 | (b & 65280) << 8 | b >> 8 & 65280 | b >> 24 & 255;
    };
    b.toS8U8 = g;
    b.fromS8U8 = function(b) {
      return b / 256;
    };
    b.clampS8U8 = function(b) {
      return g(b) / 256;
    };
    b.toS16 = function(b) {
      return b << 16 >> 16;
    };
    b.bitCount = function(b) {
      b -= b >> 1 & 1431655765;
      b = (b & 858993459) + (b >> 2 & 858993459);
      return 16843009 * (b + (b >> 4) & 252645135) >> 24;
    };
    b.ones = function(b) {
      b -= b >> 1 & 1431655765;
      b = (b & 858993459) + (b >> 2 & 858993459);
      return 16843009 * (b + (b >> 4) & 252645135) >> 24;
    };
    b.trailingZeros = function(g) {
      return b.ones((g & -g) - 1);
    };
    b.getFlags = function(b, g) {
      var a = "";
      for (b = 0;b < g.length;b++) {
        b & 1 << b && (a += g[b] + " ");
      }
      return 0 === a.length ? "" : a.trim();
    };
    b.isPowerOfTwo = function(b) {
      return b && 0 === (b & b - 1);
    };
    b.roundToMultipleOfFour = function(b) {
      return b + 3 & -4;
    };
    b.nearestPowerOfTwo = function(b) {
      b--;
      b |= b >> 1;
      b |= b >> 2;
      b |= b >> 4;
      b |= b >> 8;
      b |= b >> 16;
      b++;
      return b;
    };
    b.roundToMultipleOfPowerOfTwo = function(b, g) {
      var a = (1 << g) - 1;
      return b + a & ~a;
    };
    Math.imul || (Math.imul = function(b, g) {
      var a = b & 65535, d = g & 65535;
      return a * d + ((b >>> 16 & 65535) * d + a * (g >>> 16 & 65535) << 16 >>> 0) | 0;
    });
    Math.clz32 || (Math.clz32 = function(g) {
      g |= g >> 1;
      g |= g >> 2;
      g |= g >> 4;
      g |= g >> 8;
      return 32 - b.ones(g | g >> 16);
    });
  })(f.IntegerUtilities || (f.IntegerUtilities = {}));
  var h = f.IntegerUtilities;
  (function(b) {
    function g(b, g, a, d, e, h) {
      return(a - b) * (h - g) - (d - g) * (e - b);
    }
    b.pointInPolygon = function(b, g, a) {
      for (var d = 0, e = a.length - 2, h = 0;h < e;h += 2) {
        var c = a[h + 0], k = a[h + 1], p = a[h + 2], q = a[h + 3];
        (k <= g && q > g || k > g && q <= g) && b < c + (g - k) / (q - k) * (p - c) && d++;
      }
      return 1 === (d & 1);
    };
    b.signedArea = g;
    b.counterClockwise = function(b, a, d, e, h, c) {
      return 0 < g(b, a, d, e, h, c);
    };
    b.clockwise = function(b, a, d, e, h, c) {
      return 0 > g(b, a, d, e, h, c);
    };
    b.pointInPolygonInt32 = function(b, g, a) {
      b |= 0;
      g |= 0;
      for (var d = 0, e = a.length - 2, h = 0;h < e;h += 2) {
        var c = a[h + 0], k = a[h + 1], p = a[h + 2], q = a[h + 3];
        (k <= g && q > g || k > g && q <= g) && b < c + (g - k) / (q - k) * (p - c) && d++;
      }
      return 1 === (d & 1);
    };
  })(f.GeometricUtilities || (f.GeometricUtilities = {}));
  (function(b) {
    b[b.Error = 1] = "Error";
    b[b.Warn = 2] = "Warn";
    b[b.Debug = 4] = "Debug";
    b[b.Log = 8] = "Log";
    b[b.Info = 16] = "Info";
    b[b.All = 31] = "All";
  })(f.LogLevel || (f.LogLevel = {}));
  d = function() {
    function b(g, a) {
      "undefined" === typeof g && (g = !1);
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = g;
      this._out = a || b._consoleOut;
      this._outNoNewline = a || b._consoleOutNoNewline;
    }
    b.prototype.write = function(b, a) {
      "undefined" === typeof b && (b = "");
      "undefined" === typeof a && (a = !1);
      this._suppressOutput || this._outNoNewline((a ? this._padding : "") + b);
    };
    b.prototype.writeLn = function(b) {
      "undefined" === typeof b && (b = "");
      this._suppressOutput || this._out(this._padding + b);
    };
    b.prototype.writeTimeLn = function(b) {
      "undefined" === typeof b && (b = "");
      this._suppressOutput || this._out(this._padding + performance.now().toFixed(2) + " " + b);
    };
    b.prototype.writeComment = function(b) {
      b = b.split("\n");
      if (1 === b.length) {
        this.writeLn("// " + b[0]);
      } else {
        this.writeLn("/**");
        for (var a = 0;a < b.length;a++) {
          this.writeLn(" * " + b[a]);
        }
        this.writeLn(" */");
      }
    };
    b.prototype.writeLns = function(b) {
      b = b.split("\n");
      for (var a = 0;a < b.length;a++) {
        this.writeLn(b[a]);
      }
    };
    b.prototype.errorLn = function(g) {
      b.logLevel & 1 && this.boldRedLn(g);
    };
    b.prototype.warnLn = function(g) {
      b.logLevel & 2 && this.yellowLn(g);
    };
    b.prototype.debugLn = function(g) {
      b.logLevel & 4 && this.purpleLn(g);
    };
    b.prototype.logLn = function(g) {
      b.logLevel & 8 && this.writeLn(g);
    };
    b.prototype.infoLn = function(g) {
      b.logLevel & 16 && this.writeLn(g);
    };
    b.prototype.yellowLn = function(g) {
      this.colorLn(b.YELLOW, g);
    };
    b.prototype.greenLn = function(g) {
      this.colorLn(b.GREEN, g);
    };
    b.prototype.boldRedLn = function(g) {
      this.colorLn(b.BOLD_RED, g);
    };
    b.prototype.redLn = function(g) {
      this.colorLn(b.RED, g);
    };
    b.prototype.purpleLn = function(g) {
      this.colorLn(b.PURPLE, g);
    };
    b.prototype.colorLn = function(g, a) {
      this._suppressOutput || (inBrowser ? this._out(this._padding + a) : this._out(this._padding + g + a + b.ENDC));
    };
    b.prototype.redLns = function(g) {
      this.colorLns(b.RED, g);
    };
    b.prototype.colorLns = function(b, a) {
      for (var d = a.split("\n"), e = 0;e < d.length;e++) {
        this.colorLn(b, d[e]);
      }
    };
    b.prototype.enter = function(b) {
      this._suppressOutput || this._out(this._padding + b);
      this.indent();
    };
    b.prototype.leaveAndEnter = function(b) {
      this.leave(b);
      this.indent();
    };
    b.prototype.leave = function(b) {
      this.outdent();
      this._suppressOutput || this._out(this._padding + b);
    };
    b.prototype.indent = function() {
      this._padding += this._tab;
    };
    b.prototype.outdent = function() {
      0 < this._padding.length && (this._padding = this._padding.substring(0, this._padding.length - this._tab.length));
    };
    b.prototype.writeArray = function(b, a, d) {
      "undefined" === typeof a && (a = !1);
      "undefined" === typeof d && (d = !1);
      a = a || !1;
      for (var e = 0, h = b.length;e < h;e++) {
        var c = "";
        a && (c = null === b[e] ? "null" : void 0 === b[e] ? "undefined" : b[e].constructor.name, c += " ");
        var k = d ? "" : ("" + e).padRight(" ", 4);
        this.writeLn(k + c + b[e]);
      }
    };
    b.PURPLE = "\u001b[94m";
    b.YELLOW = "\u001b[93m";
    b.GREEN = "\u001b[92m";
    b.RED = "\u001b[91m";
    b.BOLD_RED = "\u001b[1;91m";
    b.ENDC = "\u001b[0m";
    b.logLevel = 31;
    b._consoleOut = inBrowser ? console.info.bind(console) : print;
    b._consoleOutNoNewline = inBrowser ? console.info.bind(console) : putstr;
    return b;
  }();
  f.IndentingWriter = d;
  var n = function() {
    return function(b, g) {
      this.value = b;
      this.next = g;
    };
  }(), d = function() {
    function b(b) {
      r.assert(b);
      this._compare = b;
      this._head = null;
      this._length = 0;
    }
    b.prototype.push = function(b) {
      r.assert(void 0 !== b);
      this._length++;
      if (this._head) {
        var a = this._head, d = null;
        b = new n(b, null);
        for (var e = this._compare;a;) {
          if (0 < e(a.value, b.value)) {
            d ? (b.next = a, d.next = b) : (b.next = this._head, this._head = b);
            return;
          }
          d = a;
          a = a.next;
        }
        d.next = b;
      } else {
        this._head = new n(b, null);
      }
    };
    b.prototype.forEach = function(g) {
      for (var a = this._head, d = null;a;) {
        var e = g(a.value);
        if (e === b.RETURN) {
          break;
        } else {
          e === b.DELETE ? a = d ? d.next = a.next : this._head = this._head.next : (d = a, a = a.next);
        }
      }
    };
    b.prototype.isEmpty = function() {
      return!this._head;
    };
    b.prototype.pop = function() {
      if (this._head) {
        this._length--;
        var b = this._head;
        this._head = this._head.next;
        return b.value;
      }
    };
    b.prototype.contains = function(b) {
      for (var a = this._head;a;) {
        if (a.value === b) {
          return!0;
        }
        a = a.next;
      }
      return!1;
    };
    b.prototype.toString = function() {
      for (var b = "[", a = this._head;a;) {
        b += a.value.toString(), (a = a.next) && (b += ",");
      }
      return b + "]";
    };
    b.RETURN = 1;
    b.DELETE = 2;
    return b;
  }();
  f.SortedList = d;
  d = function() {
    function b(b, a) {
      "undefined" === typeof a && (a = 12);
      this.start = this.index = 0;
      this._size = 1 << a;
      this._mask = this._size - 1;
      this.array = new b(this._size);
    }
    b.prototype.get = function(b) {
      return this.array[b];
    };
    b.prototype.forEachInReverse = function(b) {
      if (!this.isEmpty()) {
        for (var a = 0 === this.index ? this._size - 1 : this.index - 1, d = this.start - 1 & this._mask;a !== d && !b(this.array[a], a);) {
          a = 0 === a ? this._size - 1 : a - 1;
        }
      }
    };
    b.prototype.write = function(b) {
      this.array[this.index] = b;
      this.index = this.index + 1 & this._mask;
      this.index === this.start && (this.start = this.start + 1 & this._mask);
    };
    b.prototype.isFull = function() {
      return(this.index + 1 & this._mask) === this.start;
    };
    b.prototype.isEmpty = function() {
      return this.index === this.start;
    };
    b.prototype.reset = function() {
      this.start = this.index = 0;
    };
    return b;
  }();
  f.CircularBuffer = d;
  (function(b) {
    function g(g) {
      return g + (b.BITS_PER_WORD - 1) >> b.ADDRESS_BITS_PER_WORD << b.ADDRESS_BITS_PER_WORD;
    }
    function a(b, g) {
      b = b || "1";
      g = g || "0";
      for (var d = "", e = 0;e < length;e++) {
        d += this.get(e) ? b : g;
      }
      return d;
    }
    function d(b) {
      for (var g = [], a = 0;a < length;a++) {
        this.get(a) && g.push(b ? b[a] : a);
      }
      return g.join(", ");
    }
    var e = f.Debug.assert;
    b.ADDRESS_BITS_PER_WORD = 5;
    b.BITS_PER_WORD = 1 << b.ADDRESS_BITS_PER_WORD;
    b.BIT_INDEX_MASK = b.BITS_PER_WORD - 1;
    var h = function() {
      function a(d) {
        this.size = g(d);
        this.dirty = this.count = 0;
        this.length = d;
        this.bits = new Uint32Array(this.size >> b.ADDRESS_BITS_PER_WORD);
      }
      a.prototype.recount = function() {
        if (this.dirty) {
          for (var b = this.bits, g = 0, a = 0, d = b.length;a < d;a++) {
            var e = b[a], e = e - (e >> 1 & 1431655765), e = (e & 858993459) + (e >> 2 & 858993459), g = g + (16843009 * (e + (e >> 4) & 252645135) >> 24)
          }
          this.count = g;
          this.dirty = 0;
        }
      };
      a.prototype.set = function(g) {
        var a = g >> b.ADDRESS_BITS_PER_WORD, d = this.bits[a];
        g = d | 1 << (g & b.BIT_INDEX_MASK);
        this.bits[a] = g;
        this.dirty |= d ^ g;
      };
      a.prototype.setAll = function() {
        for (var b = this.bits, g = 0, a = b.length;g < a;g++) {
          b[g] = 4294967295;
        }
        this.count = this.size;
        this.dirty = 0;
      };
      a.prototype.assign = function(b) {
        this.count = b.count;
        this.dirty = b.dirty;
        this.size = b.size;
        for (var g = 0, a = this.bits.length;g < a;g++) {
          this.bits[g] = b.bits[g];
        }
      };
      a.prototype.clear = function(g) {
        var a = g >> b.ADDRESS_BITS_PER_WORD, d = this.bits[a];
        g = d & ~(1 << (g & b.BIT_INDEX_MASK));
        this.bits[a] = g;
        this.dirty |= d ^ g;
      };
      a.prototype.get = function(g) {
        return 0 !== (this.bits[g >> b.ADDRESS_BITS_PER_WORD] & 1 << (g & b.BIT_INDEX_MASK));
      };
      a.prototype.clearAll = function() {
        for (var b = this.bits, g = 0, a = b.length;g < a;g++) {
          b[g] = 0;
        }
        this.dirty = this.count = 0;
      };
      a.prototype._union = function(b) {
        var g = this.dirty, a = this.bits;
        b = b.bits;
        for (var d = 0, e = a.length;d < e;d++) {
          var s = a[d], h = s | b[d];
          a[d] = h;
          g |= s ^ h;
        }
        this.dirty = g;
      };
      a.prototype.intersect = function(b) {
        var g = this.dirty, a = this.bits;
        b = b.bits;
        for (var d = 0, e = a.length;d < e;d++) {
          var s = a[d], h = s & b[d];
          a[d] = h;
          g |= s ^ h;
        }
        this.dirty = g;
      };
      a.prototype.subtract = function(b) {
        var g = this.dirty, a = this.bits;
        b = b.bits;
        for (var d = 0, e = a.length;d < e;d++) {
          var s = a[d], h = s & ~b[d];
          a[d] = h;
          g |= s ^ h;
        }
        this.dirty = g;
      };
      a.prototype.negate = function() {
        for (var b = this.dirty, g = this.bits, a = 0, d = g.length;a < d;a++) {
          var e = g[a], s = ~e;
          g[a] = s;
          b |= e ^ s;
        }
        this.dirty = b;
      };
      a.prototype.forEach = function(g) {
        e(g);
        for (var a = this.bits, d = 0, h = a.length;d < h;d++) {
          var c = a[d];
          if (c) {
            for (var l = 0;l < b.BITS_PER_WORD;l++) {
              c & 1 << l && g(d * b.BITS_PER_WORD + l);
            }
          }
        }
      };
      a.prototype.toArray = function() {
        for (var g = [], a = this.bits, d = 0, e = a.length;d < e;d++) {
          var s = a[d];
          if (s) {
            for (var h = 0;h < b.BITS_PER_WORD;h++) {
              s & 1 << h && g.push(d * b.BITS_PER_WORD + h);
            }
          }
        }
        return g;
      };
      a.prototype.equals = function(b) {
        if (this.size !== b.size) {
          return!1;
        }
        var g = this.bits;
        b = b.bits;
        for (var a = 0, d = g.length;a < d;a++) {
          if (g[a] !== b[a]) {
            return!1;
          }
        }
        return!0;
      };
      a.prototype.contains = function(b) {
        if (this.size !== b.size) {
          return!1;
        }
        var g = this.bits;
        b = b.bits;
        for (var a = 0, d = g.length;a < d;a++) {
          if ((g[a] | b[a]) !== g[a]) {
            return!1;
          }
        }
        return!0;
      };
      a.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      a.prototype.clone = function() {
        var b = new a(this.length);
        b._union(this);
        return b;
      };
      return a;
    }();
    b.Uint32ArrayBitSet = h;
    var c = function() {
      function a(b) {
        this.dirty = this.count = 0;
        this.size = g(b);
        this.bits = 0;
        this.singleWord = !0;
        this.length = b;
      }
      a.prototype.recount = function() {
        if (this.dirty) {
          var b = this.bits, b = b - (b >> 1 & 1431655765), b = (b & 858993459) + (b >> 2 & 858993459);
          this.count = 0 + (16843009 * (b + (b >> 4) & 252645135) >> 24);
          this.dirty = 0;
        }
      };
      a.prototype.set = function(a) {
        var g = this.bits;
        this.bits = a = g | 1 << (a & b.BIT_INDEX_MASK);
        this.dirty |= g ^ a;
      };
      a.prototype.setAll = function() {
        this.bits = 4294967295;
        this.count = this.size;
        this.dirty = 0;
      };
      a.prototype.assign = function(b) {
        this.count = b.count;
        this.dirty = b.dirty;
        this.size = b.size;
        this.bits = b.bits;
      };
      a.prototype.clear = function(a) {
        var g = this.bits;
        this.bits = a = g & ~(1 << (a & b.BIT_INDEX_MASK));
        this.dirty |= g ^ a;
      };
      a.prototype.get = function(a) {
        return 0 !== (this.bits & 1 << (a & b.BIT_INDEX_MASK));
      };
      a.prototype.clearAll = function() {
        this.dirty = this.count = this.bits = 0;
      };
      a.prototype._union = function(b) {
        var a = this.bits;
        this.bits = b = a | b.bits;
        this.dirty = a ^ b;
      };
      a.prototype.intersect = function(b) {
        var a = this.bits;
        this.bits = b = a & b.bits;
        this.dirty = a ^ b;
      };
      a.prototype.subtract = function(b) {
        var a = this.bits;
        this.bits = b = a & ~b.bits;
        this.dirty = a ^ b;
      };
      a.prototype.negate = function() {
        var b = this.bits, a = ~b;
        this.bits = a;
        this.dirty = b ^ a;
      };
      a.prototype.forEach = function(a) {
        e(a);
        var g = this.bits;
        if (g) {
          for (var d = 0;d < b.BITS_PER_WORD;d++) {
            g & 1 << d && a(d);
          }
        }
      };
      a.prototype.toArray = function() {
        var a = [], g = this.bits;
        if (g) {
          for (var d = 0;d < b.BITS_PER_WORD;d++) {
            g & 1 << d && a.push(d);
          }
        }
        return a;
      };
      a.prototype.equals = function(b) {
        return this.bits === b.bits;
      };
      a.prototype.contains = function(b) {
        var a = this.bits;
        return(a | b.bits) === a;
      };
      a.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      a.prototype.clone = function() {
        var b = new a(this.length);
        b._union(this);
        return b;
      };
      return a;
    }();
    b.Uint32BitSet = c;
    c.prototype.toString = d;
    c.prototype.toBitString = a;
    h.prototype.toString = d;
    h.prototype.toBitString = a;
    b.BitSetFunctor = function(a) {
      var d = 1 === g(a) >> b.ADDRESS_BITS_PER_WORD ? c : h;
      return function() {
        return new d(a);
      };
    };
  })(f.BitSets || (f.BitSets = {}));
  d = function() {
    function b() {
    }
    b.randomStyle = function() {
      b._randomStyleCache || (b._randomStyleCache = "#ff5e3a #ff9500 #ffdb4c #87fc70 #52edc7 #1ad6fd #c644fc #ef4db6 #4a4a4a #dbddde #ff3b30 #ff9500 #ffcc00 #4cd964 #34aadc #007aff #5856d6 #ff2d55 #8e8e93 #c7c7cc #5ad427 #c86edf #d1eefc #e0f8d8 #fb2b69 #f7f7f7 #1d77ef #d6cec3 #55efcb #ff4981 #ffd3e0 #f7f7f7 #ff1300 #1f1f21 #bdbec2 #ff3a2d".split(" "));
      return b._randomStyleCache[b._nextStyle++ % b._randomStyleCache.length];
    };
    b.contrastStyle = function(b) {
      b = parseInt(b.substr(1), 16);
      return 128 <= (299 * (b >> 16) + 587 * (b >> 8 & 255) + 114 * (b & 255)) / 1E3 ? "#000000" : "#ffffff";
    };
    b.reset = function() {
      b._nextStyle = 0;
    };
    b.TabToolbar = "#252c33";
    b.Toolbars = "#343c45";
    b.HighlightBlue = "#1d4f73";
    b.LightText = "#f5f7fa";
    b.ForegroundText = "#b6babf";
    b.Black = "#000000";
    b.VeryDark = "#14171a";
    b.Dark = "#181d20";
    b.Light = "#a9bacb";
    b.Grey = "#8fa1b2";
    b.DarkGrey = "#5f7387";
    b.Blue = "#46afe3";
    b.Purple = "#6b7abb";
    b.Pink = "#df80ff";
    b.Red = "#eb5368";
    b.Orange = "#d96629";
    b.LightOrange = "#d99b28";
    b.Green = "#70bf53";
    b.BlueGrey = "#5e88b0";
    b._nextStyle = 0;
    return b;
  }();
  f.ColorStyle = d;
  d = function() {
    function b(b, a, d, e) {
      this.xMin = b | 0;
      this.yMin = a | 0;
      this.xMax = d | 0;
      this.yMax = e | 0;
    }
    b.FromUntyped = function(a) {
      return new b(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    b.FromRectangle = function(a) {
      return new b(20 * a.x | 0, 20 * a.y | 0, 20 * (a.x + a.width) | 0, 20 * (a.y + a.height) | 0);
    };
    b.prototype.setElements = function(b, a, d, e) {
      this.xMin = b;
      this.yMin = a;
      this.xMax = d;
      this.yMax = e;
    };
    b.prototype.copyFrom = function(b) {
      this.setElements(b.xMin, b.yMin, b.xMax, b.yMax);
    };
    b.prototype.contains = function(b, a) {
      return b < this.xMin !== b < this.xMax && a < this.yMin !== a < this.yMax;
    };
    b.prototype.unionInPlace = function(b) {
      this.xMin = Math.min(this.xMin, b.xMin);
      this.yMin = Math.min(this.yMin, b.yMin);
      this.xMax = Math.max(this.xMax, b.xMax);
      this.yMax = Math.max(this.yMax, b.yMax);
    };
    b.prototype.extendByPoint = function(b, a) {
      this.extendByX(b);
      this.extendByY(a);
    };
    b.prototype.extendByX = function(b) {
      134217728 === this.xMin ? this.xMin = this.xMax = b : (this.xMin = Math.min(this.xMin, b), this.xMax = Math.max(this.xMax, b));
    };
    b.prototype.extendByY = function(b) {
      134217728 === this.yMin ? this.yMin = this.yMax = b : (this.yMin = Math.min(this.yMin, b), this.yMax = Math.max(this.yMax, b));
    };
    b.prototype.intersects = function(b) {
      return this.contains(b.xMin, b.yMin) || this.contains(b.xMax, b.yMax);
    };
    b.prototype.isEmpty = function() {
      return this.xMax <= this.xMin || this.yMax <= this.yMin;
    };
    Object.defineProperty(b.prototype, "width", {get:function() {
      return this.xMax - this.xMin;
    }, set:function(b) {
      this.xMax = this.xMin + b;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "height", {get:function() {
      return this.yMax - this.yMin;
    }, set:function(b) {
      this.yMax = this.yMin + b;
    }, enumerable:!0, configurable:!0});
    b.prototype.getBaseWidth = function(b) {
      return Math.abs(Math.cos(b)) * (this.xMax - this.xMin) + Math.abs(Math.sin(b)) * (this.yMax - this.yMin);
    };
    b.prototype.getBaseHeight = function(b) {
      return Math.abs(Math.sin(b)) * (this.xMax - this.xMin) + Math.abs(Math.cos(b)) * (this.yMax - this.yMin);
    };
    b.prototype.setEmpty = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 0;
    };
    b.prototype.setToSentinels = function() {
      this.xMin = this.yMin = this.xMax = this.yMax = 134217728;
    };
    b.prototype.clone = function() {
      return new b(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    b.prototype.toString = function() {
      return "{ xMin: " + this.xMin + ", xMin: " + this.yMin + ", xMax: " + this.xMax + ", xMax: " + this.yMax + " }";
    };
    return b;
  }();
  f.Bounds = d;
  d = function() {
    function b(b, a, d, e) {
      r.assert(u(b));
      r.assert(u(a));
      r.assert(u(d));
      r.assert(u(e));
      this._xMin = b | 0;
      this._yMin = a | 0;
      this._xMax = d | 0;
      this._yMax = e | 0;
    }
    b.FromUntyped = function(a) {
      return new b(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    b.FromRectangle = function(a) {
      return new b(20 * a.x | 0, 20 * a.y | 0, 20 * (a.x + a.width) | 0, 20 * (a.y + a.height) | 0);
    };
    b.prototype.setElements = function(b, a, d, e) {
      this.xMin = b;
      this.yMin = a;
      this.xMax = d;
      this.yMax = e;
    };
    b.prototype.copyFrom = function(b) {
      this.setElements(b.xMin, b.yMin, b.xMax, b.yMax);
    };
    b.prototype.contains = function(b, a) {
      return b < this.xMin !== b < this.xMax && a < this.yMin !== a < this.yMax;
    };
    b.prototype.unionWith = function(b) {
      this._xMin = Math.min(this._xMin, b._xMin);
      this._yMin = Math.min(this._yMin, b._yMin);
      this._xMax = Math.max(this._xMax, b._xMax);
      this._yMax = Math.max(this._yMax, b._yMax);
    };
    b.prototype.extendByPoint = function(b, a) {
      this.extendByX(b);
      this.extendByY(a);
    };
    b.prototype.extendByX = function(b) {
      134217728 === this.xMin ? this.xMin = this.xMax = b : (this.xMin = Math.min(this.xMin, b), this.xMax = Math.max(this.xMax, b));
    };
    b.prototype.extendByY = function(b) {
      134217728 === this.yMin ? this.yMin = this.yMax = b : (this.yMin = Math.min(this.yMin, b), this.yMax = Math.max(this.yMax, b));
    };
    b.prototype.intersects = function(b) {
      return this.contains(b._xMin, b._yMin) || this.contains(b._xMax, b._yMax);
    };
    b.prototype.isEmpty = function() {
      return this._xMax <= this._xMin || this._yMax <= this._yMin;
    };
    Object.defineProperty(b.prototype, "xMin", {get:function() {
      return this._xMin;
    }, set:function(b) {
      r.assert(u(b));
      this._xMin = b;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "yMin", {get:function() {
      return this._yMin;
    }, set:function(b) {
      r.assert(u(b));
      this._yMin = b | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "xMax", {get:function() {
      return this._xMax;
    }, set:function(b) {
      r.assert(u(b));
      this._xMax = b | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "width", {get:function() {
      return this._xMax - this._xMin;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "yMax", {get:function() {
      return this._yMax;
    }, set:function(b) {
      r.assert(u(b));
      this._yMax = b | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "height", {get:function() {
      return this._yMax - this._yMin;
    }, enumerable:!0, configurable:!0});
    b.prototype.getBaseWidth = function(b) {
      return Math.abs(Math.cos(b)) * (this._xMax - this._xMin) + Math.abs(Math.sin(b)) * (this._yMax - this._yMin);
    };
    b.prototype.getBaseHeight = function(b) {
      return Math.abs(Math.sin(b)) * (this._xMax - this._xMin) + Math.abs(Math.cos(b)) * (this._yMax - this._yMin);
    };
    b.prototype.setEmpty = function() {
      this._xMin = this._yMin = this._xMax = this._yMax = 0;
    };
    b.prototype.clone = function() {
      return new b(this.xMin, this.yMin, this.xMax, this.yMax);
    };
    b.prototype.toString = function() {
      return "{ xMin: " + this._xMin + ", xMin: " + this._yMin + ", xMax: " + this._xMax + ", yMax: " + this._yMax + " }";
    };
    b.prototype.assertValid = function() {
    };
    return b;
  }();
  f.DebugBounds = d;
  d = function() {
    function b(b, a, d, e) {
      this.r = b;
      this.g = a;
      this.b = d;
      this.a = e;
    }
    b.FromARGB = function(a) {
      return new b((a >> 16 & 255) / 255, (a >> 8 & 255) / 255, (a >> 0 & 255) / 255, (a >> 24 & 255) / 255);
    };
    b.FromRGBA = function(a) {
      return b.FromARGB(q.RGBAToARGB(a));
    };
    b.prototype.toRGBA = function() {
      return 255 * this.r << 24 | 255 * this.g << 16 | 255 * this.b << 8 | 255 * this.a;
    };
    b.prototype.toCSSStyle = function() {
      return q.rgbaToCSSStyle(this.toRGBA());
    };
    b.prototype.set = function(b) {
      this.r = b.r;
      this.g = b.g;
      this.b = b.b;
      this.a = b.a;
    };
    b.randomColor = function(a) {
      "undefined" === typeof a && (a = 1);
      return new b(Math.random(), Math.random(), Math.random(), a);
    };
    b.parseColor = function(a) {
      b.colorCache || (b.colorCache = Object.create(null));
      if (b.colorCache[a]) {
        return b.colorCache[a];
      }
      var d = document.createElement("span");
      document.body.appendChild(d);
      d.style.backgroundColor = a;
      var e = getComputedStyle(d).backgroundColor;
      document.body.removeChild(d);
      (d = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(e)) || (d = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(e));
      e = new b(0, 0, 0, 0);
      e.r = parseFloat(d[1]) / 255;
      e.g = parseFloat(d[2]) / 255;
      e.b = parseFloat(d[3]) / 255;
      e.a = d[4] ? parseFloat(d[4]) / 255 : 1;
      return b.colorCache[a] = e;
    };
    b.Red = new b(1, 0, 0, 1);
    b.Green = new b(0, 1, 0, 1);
    b.Blue = new b(0, 0, 1, 1);
    b.None = new b(0, 0, 0, 0);
    b.White = new b(1, 1, 1, 1);
    b.Black = new b(0, 0, 0, 1);
    b.colorCache = {};
    return b;
  }();
  f.Color = d;
  (function(b) {
    function a(b) {
      var d, g, e = b >> 24 & 255;
      g = (Math.imul(b >> 16 & 255, e) + 127) / 255 | 0;
      d = (Math.imul(b >> 8 & 255, e) + 127) / 255 | 0;
      b = (Math.imul(b >> 0 & 255, e) + 127) / 255 | 0;
      return e << 24 | g << 16 | d << 8 | b;
    }
    b.RGBAToARGB = function(b) {
      return b >> 8 & 16777215 | (b & 255) << 24;
    };
    b.ARGBToRGBA = function(b) {
      return b << 8 | b >> 24 & 255;
    };
    b.rgbaToCSSStyle = function(b) {
      return f.StringUtilities.concat9("rgba(", b >> 24 & 255, ",", b >> 16 & 255, ",", b >> 8 & 255, ",", (b & 255) / 255, ")");
    };
    b.cssStyleToRGBA = function(b) {
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
    b.hexToRGB = function(b) {
      return parseInt(b.slice(1), 16);
    };
    b.rgbToHex = function(b) {
      return "#" + ("000000" + b.toString(16)).slice(-6);
    };
    b.isValidHexColor = function(b) {
      return/^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(b);
    };
    b.clampByte = function(b) {
      return Math.max(0, Math.min(255, b));
    };
    b.unpremultiplyARGB = function(b) {
      var a, d, g = b >> 24 & 255;
      d = Math.imul(255, b >> 16 & 255) / g & 255;
      a = Math.imul(255, b >> 8 & 255) / g & 255;
      b = Math.imul(255, b >> 0 & 255) / g & 255;
      return g << 24 | d << 16 | a << 8 | b;
    };
    b.premultiplyARGB = a;
    var d;
    b.ensureUnpremultiplyTable = function() {
      if (!d) {
        d = new Uint8Array(65536);
        for (var b = 0;256 > b;b++) {
          for (var a = 0;256 > a;a++) {
            d[(a << 8) + b] = Math.imul(255, b) / a;
          }
        }
      }
    };
    b.tableLookupUnpremultiplyARGB = function(b) {
      b |= 0;
      var a = b >> 24 & 255;
      if (0 === a) {
        return 0;
      }
      if (255 === a) {
        return b;
      }
      var g, e, h = a << 8, c = d;
      e = c[h + (b >> 16 & 255)];
      g = c[h + (b >> 8 & 255)];
      b = c[h + (b >> 0 & 255)];
      return a << 24 | e << 16 | g << 8 | b;
    };
    b.blendPremultipliedBGRA = function(b, a) {
      var d, g;
      g = 256 - (a & 255);
      d = Math.imul(b & 16711935, g) >> 8;
      g = Math.imul(b >> 8 & 16711935, g) >> 8;
      return((a >> 8 & 16711935) + g & 16711935) << 8 | (a & 16711935) + d & 16711935;
    };
    var e = h.swap32;
    b.convertImage = function(b, h, c, p) {
      c !== p && r.assert(c.buffer !== p.buffer, "Can't handle overlapping views.");
      var q = c.length;
      if (b === h) {
        if (c !== p) {
          for (b = 0;b < q;b++) {
            p[b] = c[b];
          }
        }
      } else {
        if (1 === b && 3 === h) {
          for (f.ColorUtilities.ensureUnpremultiplyTable(), b = 0;b < q;b++) {
            var n = c[b];
            h = n & 255;
            if (0 === h) {
              p[b] = 0;
            } else {
              if (255 === h) {
                p[b] = (n & 255) << 24 | n >> 8 & 16777215;
              } else {
                var t = n >> 24 & 255, m = n >> 16 & 255, n = n >> 8 & 255, z = h << 8, v = d, n = v[z + n], m = v[z + m], t = v[z + t];
                p[b] = h << 24 | t << 16 | m << 8 | n;
              }
            }
          }
        } else {
          if (2 === b && 3 === h) {
            for (b = 0;b < q;b++) {
              p[b] = e(c[b]);
            }
          } else {
            if (3 === b && 1 === h) {
              for (b = 0;b < q;b++) {
                h = c[b], p[b] = e(a(h & 4278255360 | h >> 16 & 255 | (h & 255) << 16));
              }
            } else {
              for (r.somewhatImplemented("Image Format Conversion: " + k[b] + " -> " + k[h]), b = 0;b < q;b++) {
                p[b] = c[b];
              }
            }
          }
        }
      }
    };
  })(f.ColorUtilities || (f.ColorUtilities = {}));
  var q = f.ColorUtilities, d = function() {
    function b(b) {
      "undefined" === typeof b && (b = 32);
      this._list = [];
      this._maxSize = b;
    }
    b.prototype.acquire = function(a) {
      if (b._enabled) {
        for (var d = this._list, e = 0;e < d.length;e++) {
          var h = d[e];
          if (h.byteLength >= a) {
            return d.splice(e, 1), h;
          }
        }
      }
      return new ArrayBuffer(a);
    };
    b.prototype.release = function(d) {
      if (b._enabled) {
        var e = this._list;
        r.assert(0 > a.indexOf(e, d));
        e.length === this._maxSize && e.shift();
        e.push(d);
      }
    };
    b.prototype.ensureUint8ArrayLength = function(b, a) {
      if (b.length >= a) {
        return b;
      }
      var d = Math.max(b.length + a, (3 * b.length >> 1) + 1), d = new Uint8Array(this.acquire(d), 0, d);
      d.set(b);
      this.release(b.buffer);
      return d;
    };
    b.prototype.ensureFloat64ArrayLength = function(b, a) {
      if (b.length >= a) {
        return b;
      }
      var d = Math.max(b.length + a, (3 * b.length >> 1) + 1), d = new Float64Array(this.acquire(d * Float64Array.BYTES_PER_ELEMENT), 0, d);
      d.set(b);
      this.release(b.buffer);
      return d;
    };
    b._enabled = !0;
    return b;
  }();
  f.ArrayBufferPool = d;
  (function(b) {
    (function(b) {
      b[b.EXTERNAL_INTERFACE_FEATURE = 1] = "EXTERNAL_INTERFACE_FEATURE";
      b[b.CLIPBOARD_FEATURE = 2] = "CLIPBOARD_FEATURE";
      b[b.SHAREDOBJECT_FEATURE = 3] = "SHAREDOBJECT_FEATURE";
      b[b.VIDEO_FEATURE = 4] = "VIDEO_FEATURE";
      b[b.SOUND_FEATURE = 5] = "SOUND_FEATURE";
      b[b.NETCONNECTION_FEATURE = 6] = "NETCONNECTION_FEATURE";
    })(b.Feature || (b.Feature = {}));
    (function(b) {
      b[b.AVM1_ERROR = 1] = "AVM1_ERROR";
      b[b.AVM2_ERROR = 2] = "AVM2_ERROR";
    })(b.ErrorTypes || (b.ErrorTypes = {}));
    b.instance;
  })(f.Telemetry || (f.Telemetry = {}));
  (function(b) {
    b.instance;
  })(f.FileLoadingService || (f.FileLoadingService = {}));
  (function(b) {
    b.instance = {enabled:!1, initJS:function(b) {
    }, registerCallback:function(b) {
    }, unregisterCallback:function(b) {
    }, eval:function(b) {
    }, call:function(b) {
    }, getId:function() {
      return null;
    }};
  })(f.ExternalInterfaceService || (f.ExternalInterfaceService = {}));
  d = function() {
    function b() {
    }
    b.prototype.setClipboard = function(b) {
      r.abstractMethod("public ClipboardService::setClipboard");
    };
    b.instance = null;
    return b;
  }();
  f.ClipboardService = d;
  d = function() {
    function b() {
      this._queues = {};
    }
    b.prototype.register = function(b, a) {
      r.assert(b);
      r.assert(a);
      var d = this._queues[b];
      if (d) {
        if (-1 < d.indexOf(a)) {
          return;
        }
      } else {
        d = this._queues[b] = [];
      }
      d.push(a);
    };
    b.prototype.unregister = function(b, a) {
      r.assert(b);
      r.assert(a);
      var d = this._queues[b];
      if (d) {
        var e = d.indexOf(a);
        -1 !== e && d.splice(e, 1);
        0 === d.length && (this._queues[b] = null);
      }
    };
    b.prototype.notify = function(b, a) {
      var d = this._queues[b];
      if (d) {
        d = d.slice();
        a = Array.prototype.slice.call(arguments, 0);
        for (var e = 0;e < d.length;e++) {
          d[e].apply(null, a);
        }
      }
    };
    b.prototype.notify1 = function(b, a) {
      var d = this._queues[b];
      if (d) {
        for (var d = d.slice(), e = 0;e < d.length;e++) {
          (0,d[e])(b, a);
        }
      }
    };
    return b;
  }();
  f.Callback = d;
  (function(b) {
    b[b.None = 0] = "None";
    b[b.PremultipliedAlphaARGB = 1] = "PremultipliedAlphaARGB";
    b[b.StraightAlphaARGB = 2] = "StraightAlphaARGB";
    b[b.StraightAlphaRGBA = 3] = "StraightAlphaRGBA";
    b[b.JPEG = 4] = "JPEG";
    b[b.PNG = 5] = "PNG";
    b[b.GIF = 6] = "GIF";
  })(f.ImageType || (f.ImageType = {}));
  var k = f.ImageType;
  f.PromiseWrapper = function() {
    return function() {
      this.promise = new Promise(function(b, a) {
        this.resolve = b;
        this.reject = a;
      }.bind(this));
    };
  }();
})(Shumway || (Shumway = {}));
(function() {
  function f(b) {
    if ("function" !== typeof b) {
      throw new TypeError("Invalid deferred constructor");
    }
    var a = n();
    b = new b(a);
    var d = a.resolve;
    if ("function" !== typeof d) {
      throw new TypeError("Invalid resolve construction function");
    }
    a = a.reject;
    if ("function" !== typeof a) {
      throw new TypeError("Invalid reject construction function");
    }
    return{promise:b, resolve:d, reject:a};
  }
  function u(b, a) {
    if ("object" !== typeof b || null === b) {
      return!1;
    }
    try {
      var d = b.then;
      if ("function" !== typeof d) {
        return!1;
      }
      d.call(b, a.resolve, a.reject);
    } catch (g) {
      d = a.reject, d(g);
    }
    return!0;
  }
  function c(b) {
    return "object" === typeof b && null !== b && "undefined" !== typeof b.promiseStatus;
  }
  function t(b, a) {
    if ("unresolved" === b.promiseStatus) {
      var d = b.rejectReactions;
      b.result = a;
      b.resolveReactions = void 0;
      b.rejectReactions = void 0;
      b.promiseStatus = "has-rejection";
      m(d, a);
    }
  }
  function m(b, a) {
    for (var d = 0;d < b.length;d++) {
      r({reaction:b[d], argument:a});
    }
  }
  function r(b) {
    0 === V.length && setTimeout(a, 0);
    V.push(b);
  }
  function a() {
    for (;0 < V.length;) {
      var a = V[0];
      try {
        a: {
          var d = a.reaction, g = d.deferred, e = d.handler, h = void 0, c = void 0;
          try {
            h = e(a.argument);
          } catch (k) {
            var p = g.reject;
            p(k);
            break a;
          }
          if (h === g.promise) {
            p = g.reject, p(new TypeError("Self resolution"));
          } else {
            try {
              if (c = u(h, g), !c) {
                var q = g.resolve;
                q(h);
              }
            } catch (n) {
              p = g.reject, p(n);
            }
          }
        }
      } catch (v) {
        if ("function" === typeof b.onerror) {
          b.onerror(v);
        }
      }
      V.shift();
    }
  }
  function d(b) {
    throw b;
  }
  function p(b) {
    return b;
  }
  function e(b) {
    return function(a) {
      t(b, a);
    };
  }
  function h(b) {
    return function(a) {
      if ("unresolved" === b.promiseStatus) {
        var d = b.resolveReactions;
        b.result = a;
        b.resolveReactions = void 0;
        b.rejectReactions = void 0;
        b.promiseStatus = "has-resolution";
        m(d, a);
      }
    };
  }
  function n() {
    function b(a, d) {
      b.resolve = a;
      b.reject = d;
    }
    return b;
  }
  function q(b, a, d) {
    return function(g) {
      if (g === b) {
        return d(new TypeError("Self resolution"));
      }
      var e = b.promiseConstructor;
      if (c(g) && g.promiseConstructor === e) {
        return g.then(a, d);
      }
      e = f(e);
      return u(g, e) ? e.promise.then(a, d) : a(g);
    };
  }
  function k(b, a, d, g) {
    return function(e) {
      a[b] = e;
      g.countdown--;
      0 === g.countdown && d.resolve(a);
    };
  }
  function b(a) {
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
    var d = h(this), g = e(this);
    try {
      a(d, g);
    } catch (c) {
      t(this, c);
    }
    this.promiseConstructor = b;
    return this;
  }
  var g = Function("return this")();
  if (g.Promise) {
    "function" !== typeof g.Promise.all && (g.Promise.all = function(b) {
      var a = 0, d = [], e, h, c = new g.Promise(function(b, a) {
        e = b;
        h = a;
      });
      b.forEach(function(b, g) {
        a++;
        b.then(function(b) {
          d[g] = b;
          a--;
          0 === a && e(d);
        }, h);
      });
      0 === a && e(d);
      return c;
    }), "function" !== typeof g.Promise.resolve && (g.Promise.resolve = function(b) {
      return new g.Promise(function(a) {
        a(b);
      });
    });
  } else {
    var V = [];
    b.all = function(b) {
      var a = f(this), d = [], g = {countdown:0}, e = 0;
      b.forEach(function(b) {
        this.cast(b).then(k(e, d, a, g), a.reject);
        e++;
        g.countdown++;
      }, this);
      0 === e && a.resolve(d);
      return a.promise;
    };
    b.cast = function(b) {
      if (c(b)) {
        return b;
      }
      var a = f(this);
      a.resolve(b);
      return a.promise;
    };
    b.reject = function(b) {
      var a = f(this);
      a.reject(b);
      return a.promise;
    };
    b.resolve = function(b) {
      var a = f(this);
      a.resolve(b);
      return a.promise;
    };
    b.prototype = {"catch":function(b) {
      this.then(void 0, b);
    }, then:function(b, a) {
      if (!c(this)) {
        throw new TypeError("this is not a Promises");
      }
      var g = f(this.promiseConstructor), e = "function" === typeof a ? a : d, h = {deferred:g, handler:q(this, "function" === typeof b ? b : p, e)}, e = {deferred:g, handler:e};
      switch(this.promiseStatus) {
        case "unresolved":
          this.resolveReactions.push(h);
          this.rejectReactions.push(e);
          break;
        case "has-resolution":
          r({reaction:h, argument:this.result});
          break;
        case "has-rejection":
          r({reaction:e, argument:this.result});
      }
      return g.promise;
    }};
    g.Promise = b;
  }
})();
"undefined" !== typeof exports && (exports.Shumway = Shumway);
(function() {
  function f(f, c, t) {
    f[c] || Object.defineProperty(f, c, {value:t, writable:!0, configurable:!0, enumerable:!1});
  }
  f(String.prototype, "padRight", function(f, c) {
    var t = this, m = t.replace(/\033\[[0-9]*m/g, "").length;
    if (!f || m >= c) {
      return t;
    }
    for (var m = (c - m) / f.length, r = 0;r < m;r++) {
      t += f;
    }
    return t;
  });
  f(String.prototype, "padLeft", function(f, c) {
    var t = this, m = t.length;
    if (!f || m >= c) {
      return t;
    }
    for (var m = (c - m) / f.length, r = 0;r < m;r++) {
      t = f + t;
    }
    return t;
  });
  f(String.prototype, "trim", function() {
    return this.replace(/^\s+|\s+$/g, "");
  });
  f(String.prototype, "endsWith", function(f) {
    return-1 !== this.indexOf(f, this.length - f.length);
  });
  f(Array.prototype, "replace", function(f, c) {
    if (f === c) {
      return 0;
    }
    for (var t = 0, m = 0;m < this.length;m++) {
      this[m] === f && (this[m] = c, t++);
    }
    return t;
  });
})();
(function(f) {
  (function(u) {
    var c = f.isObject, t = f.Debug.assert, m = function() {
      function a(d, h, c, p) {
        this.shortName = d;
        this.longName = h;
        this.type = c;
        p = p || {};
        this.positional = p.positional;
        this.parseFn = p.parse;
        this.value = p.defaultValue;
      }
      a.prototype.parse = function(a) {
        "boolean" === this.type ? (t("boolean" === typeof a), this.value = a) : "number" === this.type ? (t(!isNaN(a), a + " is not a number"), this.value = parseInt(a, 10)) : this.value = a;
        this.parseFn && this.parseFn(this.value);
      };
      return a;
    }();
    u.Argument = m;
    var r = function() {
      function c() {
        this.args = [];
      }
      c.prototype.addArgument = function(a, d, c, p) {
        a = new m(a, d, c, p);
        this.args.push(a);
        return a;
      };
      c.prototype.addBoundOption = function(a) {
        this.args.push(new m(a.shortName, a.longName, a.type, {parse:function(d) {
          a.value = d;
        }}));
      };
      c.prototype.addBoundOptionSet = function(e) {
        var h = this;
        e.options.forEach(function(e) {
          e instanceof a ? h.addBoundOptionSet(e) : (t(e instanceof d), h.addBoundOption(e));
        });
      };
      c.prototype.getUsage = function() {
        var a = "";
        this.args.forEach(function(d) {
          a = d.positional ? a + d.longName : a + ("[-" + d.shortName + "|--" + d.longName + ("boolean" === d.type ? "" : " " + d.type[0].toUpperCase()) + "]");
          a += " ";
        });
        return a;
      };
      c.prototype.parse = function(a) {
        var d = {}, c = [];
        this.args.forEach(function(b) {
          b.positional ? c.push(b) : (d["-" + b.shortName] = b, d["--" + b.longName] = b);
        });
        for (var p = [];a.length;) {
          var k = a.shift(), b = null, g = k;
          if ("--" == k) {
            p = p.concat(a);
            break;
          } else {
            if ("-" == k.slice(0, 1) || "--" == k.slice(0, 2)) {
              b = d[k];
              if (!b) {
                continue;
              }
              "boolean" !== b.type ? (g = a.shift(), t("-" !== g && "--" !== g, "Argument " + k + " must have a value.")) : g = !0;
            } else {
              c.length ? b = c.shift() : p.push(g);
            }
          }
          b && b.parse(g);
        }
        t(0 === c.length, "Missing positional arguments.");
        return p;
      };
      return c;
    }();
    u.ArgumentParser = r;
    var a = function() {
      function a(d, h) {
        "undefined" === typeof h && (h = null);
        this.open = !1;
        this.name = d;
        this.settings = h || {};
        this.options = [];
      }
      a.prototype.register = function(d) {
        if (d instanceof a) {
          for (var h = 0;h < this.options.length;h++) {
            var t = this.options[h];
            if (t instanceof a && t.name === d.name) {
              return t;
            }
          }
        }
        this.options.push(d);
        if (this.settings) {
          if (d instanceof a) {
            h = this.settings[d.name], c(h) && (d.settings = h.settings, d.open = h.open);
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
        this.options.forEach(function(d) {
          d.trace(a);
        });
        a.leave("}");
      };
      a.prototype.getSettings = function() {
        var d = {};
        this.options.forEach(function(h) {
          h instanceof a ? d[h.name] = {settings:h.getSettings(), open:h.open} : d[h.longName] = h.value;
        });
        return d;
      };
      a.prototype.setSettings = function(d) {
        d && this.options.forEach(function(h) {
          h instanceof a ? h.name in d && h.setSettings(d[h.name].settings) : h.longName in d && (h.value = d[h.longName]);
        });
      };
      return a;
    }();
    u.OptionSet = a;
    var d = function() {
      function a(d, h, c, p, k, b) {
        "undefined" === typeof b && (b = null);
        this.longName = h;
        this.shortName = d;
        this.type = c;
        this.value = this.defaultValue = p;
        this.description = k;
        this.config = b;
      }
      a.prototype.parse = function(a) {
        this.value = a;
      };
      a.prototype.trace = function(a) {
        a.writeLn(("-" + this.shortName + "|--" + this.longName).padRight(" ", 30) + " = " + this.type + " " + this.value + " [" + this.defaultValue + "] (" + this.description + ")");
      };
      return a;
    }();
    u.Option = d;
  })(f.Options || (f.Options = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    function c() {
      try {
        return "undefined" !== typeof window && "localStorage" in window && null !== window.localStorage;
      } catch (c) {
        return!1;
      }
    }
    function t(t) {
      "undefined" === typeof t && (t = u.ROOT);
      var r = {};
      if (c() && (t = window.localStorage[t])) {
        try {
          r = JSON.parse(t);
        } catch (a) {
        }
      }
      return r;
    }
    u.ROOT = "Shumway Options";
    u.shumwayOptions = new f.Options.OptionSet(u.ROOT, t());
    u.isStorageSupported = c;
    u.load = t;
    u.save = function(t, r) {
      "undefined" === typeof t && (t = null);
      "undefined" === typeof r && (r = u.ROOT);
      if (c()) {
        try {
          window.localStorage[r] = JSON.stringify(t ? t : u.shumwayOptions.getSettings());
        } catch (a) {
        }
      }
    };
    u.setSettings = function(c) {
      u.shumwayOptions.setSettings(c);
    };
    u.getSettings = function(c) {
      return u.shumwayOptions.getSettings();
    };
  })(f.Settings || (f.Settings = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    var c = function() {
      function c(t, r) {
        this._parent = t;
        this._timers = f.ObjectUtilities.createMap();
        this._name = r;
        this._count = this._total = this._last = this._begin = 0;
      }
      c.time = function(m, r) {
        c.start(m);
        r();
        c.stop();
      };
      c.start = function(m) {
        c._top = c._top._timers[m] || (c._top._timers[m] = new c(c._top, m));
        c._top.start();
        m = c._flat._timers[m] || (c._flat._timers[m] = new c(c._flat, m));
        m.start();
        c._flatStack.push(m);
      };
      c.stop = function() {
        c._top.stop();
        c._top = c._top._parent;
        c._flatStack.pop().stop();
      };
      c.stopStart = function(m) {
        c.stop();
        c.start(m);
      };
      c.prototype.start = function() {
        this._begin = f.getTicks();
      };
      c.prototype.stop = function() {
        this._last = f.getTicks() - this._begin;
        this._total += this._last;
        this._count += 1;
      };
      c.prototype.toJSON = function() {
        return{name:this._name, total:this._total, timers:this._timers};
      };
      c.prototype.trace = function(c) {
        c.enter(this._name + ": " + this._total.toFixed(2) + " ms, count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var t in this._timers) {
          this._timers[t].trace(c);
        }
        c.outdent();
      };
      c.trace = function(m) {
        c._base.trace(m);
        c._flat.trace(m);
      };
      c._base = new c(null, "Total");
      c._top = c._base;
      c._flat = new c(null, "Flat");
      c._flatStack = [];
      return c;
    }();
    u.Timer = c;
    c = function() {
      function c(t) {
        this._enabled = t;
        this.clear();
      }
      Object.defineProperty(c.prototype, "counts", {get:function() {
        return this._counts;
      }, enumerable:!0, configurable:!0});
      c.prototype.setEnabled = function(c) {
        this._enabled = c;
      };
      c.prototype.clear = function() {
        this._counts = f.ObjectUtilities.createMap();
        this._times = f.ObjectUtilities.createMap();
      };
      c.prototype.toJSON = function() {
        return{counts:this._counts, times:this._times};
      };
      c.prototype.count = function(c, r, a) {
        "undefined" === typeof r && (r = 1);
        "undefined" === typeof a && (a = 0);
        if (this._enabled) {
          return void 0 === this._counts[c] && (this._counts[c] = 0, this._times[c] = 0), this._counts[c] += r, this._times[c] += a, this._counts[c];
        }
      };
      c.prototype.trace = function(c) {
        for (var r in this._counts) {
          c.writeLn(r + ": " + this._counts[r]);
        }
      };
      c.prototype._pairToString = function(c, r) {
        var a = r[0], d = r[1], p = c[a], a = a + ": " + d;
        p && (a += ", " + p.toFixed(4), 1 < d && (a += " (" + (p / d).toFixed(4) + ")"));
        return a;
      };
      c.prototype.toStringSorted = function() {
        var c = this, r = this._times, a = [], d;
        for (d in this._counts) {
          a.push([d, this._counts[d]]);
        }
        a.sort(function(a, d) {
          return d[1] - a[1];
        });
        return a.map(function(a) {
          return c._pairToString(r, a);
        }).join(", ");
      };
      c.prototype.traceSorted = function(c, r) {
        "undefined" === typeof r && (r = !1);
        var a = this, d = this._times, p = [], e;
        for (e in this._counts) {
          p.push([e, this._counts[e]]);
        }
        p.sort(function(a, d) {
          return d[1] - a[1];
        });
        r ? c.writeLn(p.map(function(e) {
          return a._pairToString(d, e);
        }).join(", ")) : p.forEach(function(e) {
          c.writeLn(a._pairToString(d, e));
        });
      };
      c.instance = new c(!0);
      return c;
    }();
    u.Counter = c;
    c = function() {
      function c(t) {
        this._samples = new Float64Array(t);
        this._index = this._count = 0;
      }
      c.prototype.push = function(c) {
        this._count < this._samples.length && this._count++;
        this._index++;
        this._samples[this._index % this._samples.length] = c;
      };
      c.prototype.average = function() {
        for (var c = 0, r = 0;r < this._count;r++) {
          c += this._samples[r];
        }
        return c / this._count;
      };
      return c;
    }();
    u.Average = c;
  })(f.Metrics || (f.Metrics = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    function c(b) {
      for (var a = Math.max.apply(null, b), d = b.length, e = 1 << a, c = new Uint32Array(e), h = a << 16 | 65535, k = 0;k < e;k++) {
        c[k] = h;
      }
      for (var h = 0, k = 1, p = 2;k <= a;h <<= 1, ++k, p <<= 1) {
        for (var q = 0;q < d;++q) {
          if (b[q] === k) {
            for (var n = 0, r = 0;r < k;++r) {
              n = 2 * n + (h >> r & 1);
            }
            for (r = n;r < e;r += p) {
              c[r] = k << 16 | q;
            }
            ++h;
          }
        }
      }
      return{codes:c, maxBits:a};
    }
    var t;
    (function(b) {
      b[b.INIT = 0] = "INIT";
      b[b.BLOCK_0 = 1] = "BLOCK_0";
      b[b.BLOCK_1 = 2] = "BLOCK_1";
      b[b.BLOCK_2_PRE = 3] = "BLOCK_2_PRE";
      b[b.BLOCK_2 = 4] = "BLOCK_2";
      b[b.DONE = 5] = "DONE";
      b[b.ERROR = 6] = "ERROR";
      b[b.VERIFY_HEADER = 7] = "VERIFY_HEADER";
    })(t || (t = {}));
    t = function() {
      function b(b) {
        this._buffer = new Uint8Array(1024);
        this._bitLength = this._bitBuffer = this._bufferPosition = this._bufferSize = 0;
        this._window = new Uint8Array(65794);
        this._windowPosition = 0;
        this._state = b ? 7 : 0;
        this._isFinalBlock = !1;
        this._distanceTable = this._literalTable = null;
        this._block0Read = 0;
        this._block2State = null;
        this._copyState = {state:0, len:0, lenBits:0, dist:0, distBits:0};
        this._error = void 0;
        if (!n) {
          m = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);
          r = new Uint16Array(30);
          a = new Uint8Array(30);
          for (var k = b = 0, l = 1;30 > b;++b) {
            r[b] = l, l += 1 << (a[b] = ~~((k += 2 < b ? 1 : 0) / 2));
          }
          var s = new Uint8Array(288);
          for (b = 0;32 > b;++b) {
            s[b] = 5;
          }
          d = c(s.subarray(0, 32));
          p = new Uint16Array(29);
          e = new Uint8Array(29);
          k = b = 0;
          for (l = 3;29 > b;++b) {
            p[b] = l - (28 == b ? 1 : 0), l += 1 << (e[b] = ~~((k += 4 < b ? 1 : 0) / 4 % 6));
          }
          for (b = 0;288 > b;++b) {
            s[b] = 144 > b || 279 < b ? 8 : 256 > b ? 9 : 7;
          }
          h = c(s);
          n = !0;
        }
      }
      Object.defineProperty(b.prototype, "error", {get:function() {
        return this._error;
      }, enumerable:!0, configurable:!0});
      b.prototype.push = function(b) {
        if (this._buffer.length < this._bufferSize + b.length) {
          var a = new Uint8Array(this._bufferSize + b.length);
          a.set(this._buffer);
          this._buffer = a;
        }
        this._buffer.set(b, this._bufferSize);
        this._bufferSize += b.length;
        this._bufferPosition = 0;
        b = !1;
        do {
          a = this._windowPosition;
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
              b = this._verifyZlibHeader();
          }
          if (0 < this._windowPosition - a) {
            this.onData(this._window.subarray(a, this._windowPosition));
          }
          65536 <= this._windowPosition && (this._window.set(this._window.subarray(this._windowPosition - 32768, this._windowPosition)), this._windowPosition = 32768);
        } while (!b && this._bufferPosition < this._bufferSize);
        this._bufferPosition < this._bufferSize ? (this._buffer.set(this._buffer.subarray(this._bufferPosition, this._bufferSize)), this._bufferSize -= this._bufferPosition) : this._bufferSize = 0;
      };
      b.prototype._verifyZlibHeader = function() {
        var b = this._bufferPosition;
        if (b + 2 > this._bufferSize) {
          return!0;
        }
        var a = this._buffer, a = a[b] << 8 | a[b + 1];
        this._bufferPosition = b + 2;
        b = null;
        2048 !== (a & 3840) ? b = "inflate: unknown compression method" : 0 !== a % 31 ? b = "inflate: bad FCHECK" : 0 !== (a & 32) && (b = "inflate: FDICT bit set");
        b ? (this._error = b, this._state = 6) : this._state = 0;
      };
      b.prototype._decodeInitState = function() {
        if (this._isFinalBlock) {
          return this._state = 5, !1;
        }
        var b = this._buffer, a = this._bufferSize, e = this._bitBuffer, k = this._bitLength, p = this._bufferPosition;
        if (3 > (a - p << 3) + k) {
          return!0;
        }
        3 > k && (e |= b[p++] << k, k += 8);
        var q = e & 7, e = e >> 3, k = k - 3;
        switch(q >> 1) {
          case 0:
            k = e = 0;
            if (4 > a - p) {
              return!0;
            }
            var n = b[p] | b[p + 1] << 8, b = b[p + 2] | b[p + 3] << 8, p = p + 4;
            if (65535 !== (n ^ b)) {
              this._error = "inflate: invalid block 0 length";
              b = 6;
              break;
            }
            0 === n ? b = 0 : (this._block0Read = n, b = 1);
            break;
          case 1:
            b = 2;
            this._literalTable = h;
            this._distanceTable = d;
            break;
          case 2:
            if (26 > (a - p << 3) + k) {
              return!0;
            }
            for (;14 > k;) {
              e |= b[p++] << k, k += 8;
            }
            n = (e >> 10 & 15) + 4;
            if ((a - p << 3) + k < 14 + 3 * n) {
              return!0;
            }
            for (var a = {numLiteralCodes:(e & 31) + 257, numDistanceCodes:(e >> 5 & 31) + 1, codeLengthTable:void 0, bitLengths:void 0, codesRead:0, dupBits:0}, e = e >> 14, k = k - 14, r = new Uint8Array(19), t = 0;t < n;++t) {
              3 > k && (e |= b[p++] << k, k += 8), r[m[t]] = e & 7, e >>= 3, k -= 3;
            }
            for (;19 > t;t++) {
              r[m[t]] = 0;
            }
            a.bitLengths = new Uint8Array(a.numLiteralCodes + a.numDistanceCodes);
            a.codeLengthTable = c(r);
            this._block2State = a;
            b = 3;
            break;
          default:
            return this._error = "inflate: unsupported block type", !1;
        }
        this._isFinalBlock = !!(q & 1);
        this._state = b;
        this._bufferPosition = p;
        this._bitBuffer = e;
        this._bitLength = k;
        return!1;
      };
      b.prototype._decodeBlock0 = function() {
        var b = this._bufferPosition, a = this._windowPosition, d = this._block0Read, e = 65794 - a, c = this._bufferSize - b, h = c < d, k = Math.min(e, c, d);
        this._window.set(this._buffer.subarray(b, b + k), a);
        this._windowPosition = a + k;
        this._bufferPosition = b + k;
        this._block0Read = d - k;
        return d === k ? (this._state = 0, !1) : h && e < c;
      };
      b.prototype._readBits = function(b) {
        var a = this._bitBuffer, d = this._bitLength;
        if (b > d) {
          var e = this._bufferPosition, c = this._bufferSize;
          do {
            if (e >= c) {
              return this._bufferPosition = e, this._bitBuffer = a, this._bitLength = d, -1;
            }
            a |= this._buffer[e++] << d;
            d += 8;
          } while (b > d);
          this._bufferPosition = e;
        }
        this._bitBuffer = a >> b;
        this._bitLength = d - b;
        return a & (1 << b) - 1;
      };
      b.prototype._readCode = function(b) {
        var a = this._bitBuffer, d = this._bitLength, e = b.maxBits;
        if (e > d) {
          var c = this._bufferPosition, h = this._bufferSize;
          do {
            if (c >= h) {
              return this._bufferPosition = c, this._bitBuffer = a, this._bitLength = d, -1;
            }
            a |= this._buffer[c++] << d;
            d += 8;
          } while (e > d);
          this._bufferPosition = c;
        }
        b = b.codes[a & (1 << e) - 1];
        e = b >> 16;
        if (b & 32768) {
          return this._error = "inflate: invalid encoding", this._state = 6, -1;
        }
        this._bitBuffer = a >> e;
        this._bitLength = d - e;
        return b & 65535;
      };
      b.prototype._decodeBlock2Pre = function() {
        var b = this._block2State, a = b.numLiteralCodes + b.numDistanceCodes, d = b.bitLengths, e = b.codesRead, h = 0 < e ? d[e - 1] : 0, k = b.codeLengthTable, p;
        if (0 < b.dupBits) {
          p = this._readBits(b.dupBits);
          if (0 > p) {
            return!0;
          }
          for (;p--;) {
            d[e++] = h;
          }
          b.dupBits = 0;
        }
        for (;e < a;) {
          var q = this._readCode(k);
          if (0 > q) {
            return b.codesRead = e, !0;
          }
          if (16 > q) {
            d[e++] = h = q;
          } else {
            var n;
            switch(q) {
              case 16:
                n = 2;
                p = 3;
                q = h;
                break;
              case 17:
                p = n = 3;
                q = 0;
                break;
              case 18:
                n = 7, p = 11, q = 0;
            }
            for (;p--;) {
              d[e++] = q;
            }
            p = this._readBits(n);
            if (0 > p) {
              return b.codesRead = e, b.dupBits = n, !0;
            }
            for (;p--;) {
              d[e++] = q;
            }
            h = q;
          }
        }
        this._literalTable = c(d.subarray(0, b.numLiteralCodes));
        this._distanceTable = c(d.subarray(b.numLiteralCodes));
        this._state = 4;
        this._block2State = null;
        return!1;
      };
      b.prototype._decodeBlock = function() {
        var b = this._literalTable, d = this._distanceTable, c = this._window, h = this._windowPosition, k = this._copyState, q, n, t, f;
        if (0 !== k.state) {
          switch(k.state) {
            case 1:
              if (0 > (q = this._readBits(k.lenBits))) {
                return!0;
              }
              k.len += q;
              k.state = 2;
            case 2:
              if (0 > (q = this._readCode(d))) {
                return!0;
              }
              k.distBits = a[q];
              k.dist = r[q];
              k.state = 3;
            case 3:
              q = 0;
              if (0 < k.distBits && 0 > (q = this._readBits(k.distBits))) {
                return!0;
              }
              f = k.dist + q;
              n = k.len;
              for (q = h - f;n--;) {
                c[h++] = c[q++];
              }
              k.state = 0;
              if (65536 <= h) {
                return this._windowPosition = h, !1;
              }
              break;
          }
        }
        do {
          q = this._readCode(b);
          if (0 > q) {
            return this._windowPosition = h, !0;
          }
          if (256 > q) {
            c[h++] = q;
          } else {
            if (256 < q) {
              this._windowPosition = h;
              q -= 257;
              t = e[q];
              n = p[q];
              q = 0 === t ? 0 : this._readBits(t);
              if (0 > q) {
                return k.state = 1, k.len = n, k.lenBits = t, !0;
              }
              n += q;
              q = this._readCode(d);
              if (0 > q) {
                return k.state = 2, k.len = n, !0;
              }
              t = a[q];
              f = r[q];
              q = 0 === t ? 0 : this._readBits(t);
              if (0 > q) {
                return k.state = 3, k.len = n, k.dist = f, k.distBits = t, !0;
              }
              f += q;
              for (q = h - f;n--;) {
                c[h++] = c[q++];
              }
            } else {
              this._state = 0;
              break;
            }
          }
        } while (65536 > h);
        this._windowPosition = h;
        return!1;
      };
      b.inflate = function(a, d) {
        var e = new Uint8Array(d), c = 0, h = new b(!0);
        h.onData = function(b) {
          e.set(b, c);
          c += b.length;
        };
        h.push(a);
        return e;
      };
      return b;
    }();
    f.Inflate = t;
    var m, r, a, d, p, e, h, n = !1, q;
    (function(b) {
      b[b.WRITE = 0] = "WRITE";
      b[b.DONE = 1] = "DONE";
      b[b.ZLIB_HEADER = 2] = "ZLIB_HEADER";
    })(q || (q = {}));
    var k = function() {
      function b() {
        this.a = 1;
        this.b = 0;
      }
      b.prototype.update = function(b, a, d) {
        for (var e = this.a, c = this.b;a < d;++a) {
          e = (e + (b[a] & 255)) % 65521, c = (c + e) % 65521;
        }
        this.a = e;
        this.b = c;
      };
      b.prototype.getChecksum = function() {
        return this.b << 16 | this.a;
      };
      return b;
    }();
    f.Adler32 = k;
    q = function() {
      function b(b) {
        this._state = (this._writeZlibHeader = b) ? 2 : 0;
        this._adler32 = b ? new k : null;
      }
      b.prototype.push = function(b) {
        2 === this._state && (this.onData(new Uint8Array([120, 156])), this._state = 0);
        for (var a = b.length, d = new Uint8Array(a + 5 * Math.ceil(a / 65535)), e = 0, c = 0;65535 < a;) {
          d.set(new Uint8Array([0, 255, 255, 0, 0]), e), e += 5, d.set(b.subarray(c, c + 65535), e), c += 65535, e += 65535, a -= 65535;
        }
        d.set(new Uint8Array([0, a & 255, a >> 8 & 255, ~a & 255, ~a >> 8 & 255]), e);
        d.set(b.subarray(c, a), e + 5);
        this.onData(d);
        this._adler32 && this._adler32.update(b, 0, a);
      };
      b.prototype.finish = function() {
        this._state = 1;
        this.onData(new Uint8Array([1, 0, 0, 255, 255]));
        if (this._adler32) {
          var b = this._adler32.getChecksum();
          this.onData(new Uint8Array([b & 255, b >> 8 & 255, b >> 16 & 255, b >>> 24 & 255]));
        }
      };
      return b;
    }();
    f.Deflate = q;
  })(f.ArrayUtilities || (f.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    function c() {
      m("throwEOFError");
    }
    function t(a) {
      return "string" === typeof a ? a : void 0 == a ? null : a + "";
    }
    var m = f.Debug.notImplemented, r = f.StringUtilities.utf8decode, a = f.StringUtilities.utf8encode, d = f.NumberUtilities.clamp, p = function() {
      return function(a, d, b) {
        this.buffer = a;
        this.length = d;
        this.littleEndian = b;
      };
    }();
    u.PlainObjectDataBuffer = p;
    for (var e = new Uint32Array(33), h = 1, n = 0;32 >= h;h++) {
      e[h] = n = n << 1 | 1;
    }
    h = function() {
      function h(a) {
        "undefined" === typeof a && (a = h.INITIAL_SIZE);
        this._buffer || (this._buffer = new ArrayBuffer(a), this._position = this._length = 0, this._updateViews(), this._littleEndian = h._nativeLittleEndian, this._bitLength = this._bitBuffer = 0);
      }
      h.FromArrayBuffer = function(a, b) {
        "undefined" === typeof b && (b = -1);
        var d = Object.create(h.prototype);
        d._buffer = a;
        d._length = -1 === b ? a.byteLength : b;
        d._position = 0;
        d._updateViews();
        d._littleEndian = h._nativeLittleEndian;
        d._bitBuffer = 0;
        d._bitLength = 0;
        return d;
      };
      h.FromPlainObject = function(a) {
        var b = h.FromArrayBuffer(a.buffer, a.length);
        b._littleEndian = a.littleEndian;
        return b;
      };
      h.prototype.toPlainObject = function() {
        return new p(this._buffer, this._length, this._littleEndian);
      };
      h.prototype._updateViews = function() {
        this._u8 = new Uint8Array(this._buffer);
        0 === (this._buffer.byteLength & 3) && (this._i32 = new Int32Array(this._buffer), this._f32 = new Float32Array(this._buffer));
      };
      h.prototype.getBytes = function() {
        return new Uint8Array(this._buffer, 0, this._length);
      };
      h.prototype._ensureCapacity = function(a) {
        var b = this._buffer;
        if (b.byteLength < a) {
          for (var d = Math.max(b.byteLength, 1);d < a;) {
            d *= 2;
          }
          a = h._arrayBufferPool.acquire(d);
          d = this._u8;
          this._buffer = a;
          this._updateViews();
          this._u8.set(d);
          h._arrayBufferPool.release(b);
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
        this._position + 1 > this._length && c();
        return this._u8[this._position++];
      };
      h.prototype.readBytes = function(a, b, d) {
        "undefined" === typeof b && (b = 0);
        "undefined" === typeof d && (d = 0);
        var e = this._position;
        b || (b = 0);
        d || (d = this._length - e);
        e + d > this._length && c();
        a.length < b + d && (a._ensureCapacity(b + d), a.length = b + d);
        a._u8.set(new Uint8Array(this._buffer, e, d), b);
        this._position += d;
      };
      h.prototype.readShort = function() {
        return this.readUnsignedShort() << 16 >> 16;
      };
      h.prototype.readUnsignedShort = function() {
        var a = this._u8, b = this._position;
        b + 2 > this._length && c();
        var d = a[b + 0], a = a[b + 1];
        this._position = b + 2;
        return this._littleEndian ? a << 8 | d : d << 8 | a;
      };
      h.prototype.readInt = function() {
        var a = this._u8, b = this._position;
        b + 4 > this._length && c();
        var d = a[b + 0], e = a[b + 1], h = a[b + 2], a = a[b + 3];
        this._position = b + 4;
        return this._littleEndian ? a << 24 | h << 16 | e << 8 | d : d << 24 | e << 16 | h << 8 | a;
      };
      h.prototype.readUnsignedInt = function() {
        return this.readInt() >>> 0;
      };
      h.prototype.readFloat = function() {
        var a = this._position;
        a + 4 > this._length && c();
        this._position = a + 4;
        if (this._littleEndian && 0 === (a & 3) && this._f32) {
          return this._f32[a >> 2];
        }
        var b = this._u8, d = f.IntegerUtilities.u8;
        this._littleEndian ? (d[0] = b[a + 0], d[1] = b[a + 1], d[2] = b[a + 2], d[3] = b[a + 3]) : (d[3] = b[a + 0], d[2] = b[a + 1], d[1] = b[a + 2], d[0] = b[a + 3]);
        return f.IntegerUtilities.f32[0];
      };
      h.prototype.readDouble = function() {
        var a = this._u8, b = this._position;
        b + 8 > this._length && c();
        var d = f.IntegerUtilities.u8;
        this._littleEndian ? (d[0] = a[b + 0], d[1] = a[b + 1], d[2] = a[b + 2], d[3] = a[b + 3], d[4] = a[b + 4], d[5] = a[b + 5], d[6] = a[b + 6], d[7] = a[b + 7]) : (d[0] = a[b + 7], d[1] = a[b + 6], d[2] = a[b + 5], d[3] = a[b + 4], d[4] = a[b + 3], d[5] = a[b + 2], d[6] = a[b + 1], d[7] = a[b + 0]);
        this._position = b + 8;
        return f.IntegerUtilities.f64[0];
      };
      h.prototype.writeBoolean = function(a) {
        this.writeByte(a ? 1 : 0);
      };
      h.prototype.writeByte = function(a) {
        var b = this._position + 1;
        this._ensureCapacity(b);
        this._u8[this._position++] = a;
        b > this._length && (this._length = b);
      };
      h.prototype.writeUnsignedByte = function(a) {
        var b = this._position + 1;
        this._ensureCapacity(b);
        this._u8[this._position++] = a;
        b > this._length && (this._length = b);
      };
      h.prototype.writeRawBytes = function(a) {
        var b = this._position + a.length;
        this._ensureCapacity(b);
        this._u8.set(a, this._position);
        this._position = b;
        b > this._length && (this._length = b);
      };
      h.prototype.writeBytes = function(a, b, e) {
        "undefined" === typeof b && (b = 0);
        "undefined" === typeof e && (e = 0);
        2 > arguments.length && (b = 0);
        3 > arguments.length && (e = 0);
        b !== d(b, 0, a.length) && m("throwRangeError");
        var h = b + e;
        h !== d(h, 0, a.length) && m("throwRangeError");
        0 === e && (e = a.length - b);
        this.writeRawBytes(new Int8Array(a._buffer, b, e));
      };
      h.prototype.writeShort = function(a) {
        this.writeUnsignedShort(a);
      };
      h.prototype.writeUnsignedShort = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 2);
        var d = this._u8;
        this._littleEndian ? (d[b + 0] = a, d[b + 1] = a >> 8) : (d[b + 0] = a >> 8, d[b + 1] = a);
        this._position = b += 2;
        b > this._length && (this._length = b);
      };
      h.prototype.writeInt = function(a) {
        this.writeUnsignedInt(a);
      };
      h.prototype.write4Ints = function(a, b, d, e) {
        this.write4UnsignedInts(a, b, d, e);
      };
      h.prototype.writeUnsignedInt = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 4);
        if (this._littleEndian === h._nativeLittleEndian && 0 === (b & 3) && this._i32) {
          this._i32[b >> 2] = a;
        } else {
          var d = this._u8;
          this._littleEndian ? (d[b + 0] = a, d[b + 1] = a >> 8, d[b + 2] = a >> 16, d[b + 3] = a >> 24) : (d[b + 0] = a >> 24, d[b + 1] = a >> 16, d[b + 2] = a >> 8, d[b + 3] = a);
        }
        this._position = b += 4;
        b > this._length && (this._length = b);
      };
      h.prototype.write4UnsignedInts = function(a, b, d, e) {
        var c = this._position;
        this._ensureCapacity(c + 16);
        this._littleEndian === h._nativeLittleEndian && 0 === (c & 3) && this._i32 ? (this._i32[(c >> 2) + 0] = a, this._i32[(c >> 2) + 1] = b, this._i32[(c >> 2) + 2] = d, this._i32[(c >> 2) + 3] = e, this._position = c += 16, c > this._length && (this._length = c)) : (this.writeUnsignedInt(a), this.writeUnsignedInt(b), this.writeUnsignedInt(d), this.writeUnsignedInt(e));
      };
      h.prototype.writeFloat = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 4);
        if (this._littleEndian === h._nativeLittleEndian && 0 === (b & 3) && this._f32) {
          this._f32[b >> 2] = a;
        } else {
          var d = this._u8;
          f.IntegerUtilities.f32[0] = a;
          a = f.IntegerUtilities.u8;
          this._littleEndian ? (d[b + 0] = a[0], d[b + 1] = a[1], d[b + 2] = a[2], d[b + 3] = a[3]) : (d[b + 0] = a[3], d[b + 1] = a[2], d[b + 2] = a[1], d[b + 3] = a[0]);
        }
        this._position = b += 4;
        b > this._length && (this._length = b);
      };
      h.prototype.write6Floats = function(a, b, d, e, c, p) {
        var n = this._position;
        this._ensureCapacity(n + 24);
        this._littleEndian === h._nativeLittleEndian && 0 === (n & 3) && this._f32 ? (this._f32[(n >> 2) + 0] = a, this._f32[(n >> 2) + 1] = b, this._f32[(n >> 2) + 2] = d, this._f32[(n >> 2) + 3] = e, this._f32[(n >> 2) + 4] = c, this._f32[(n >> 2) + 5] = p, this._position = n += 24, n > this._length && (this._length = n)) : (this.writeFloat(a), this.writeFloat(b), this.writeFloat(d), this.writeFloat(e), this.writeFloat(c), this.writeFloat(p));
      };
      h.prototype.writeDouble = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 8);
        var d = this._u8;
        f.IntegerUtilities.f64[0] = a;
        a = f.IntegerUtilities.u8;
        this._littleEndian ? (d[b + 0] = a[0], d[b + 1] = a[1], d[b + 2] = a[2], d[b + 3] = a[3], d[b + 4] = a[4], d[b + 5] = a[5], d[b + 6] = a[6], d[b + 7] = a[7]) : (d[b + 0] = a[7], d[b + 1] = a[6], d[b + 2] = a[5], d[b + 3] = a[4], d[b + 4] = a[3], d[b + 5] = a[2], d[b + 6] = a[1], d[b + 7] = a[0]);
        this._position = b += 8;
        b > this._length && (this._length = b);
      };
      h.prototype.readRawBytes = function() {
        return new Int8Array(this._buffer, 0, this._length);
      };
      h.prototype.writeUTF = function(a) {
        a = t(a);
        a = r(a);
        this.writeShort(a.length);
        this.writeRawBytes(a);
      };
      h.prototype.writeUTFBytes = function(a) {
        a = t(a);
        a = r(a);
        this.writeRawBytes(a);
      };
      h.prototype.readUTF = function() {
        return this.readUTFBytes(this.readShort());
      };
      h.prototype.readUTFBytes = function(d) {
        d >>>= 0;
        var b = this._position;
        b + d > this._length && c();
        this._position += d;
        return a(new Int8Array(this._buffer, b, d));
      };
      Object.defineProperty(h.prototype, "length", {get:function() {
        return this._length;
      }, set:function(a) {
        a >>>= 0;
        a > this._buffer.byteLength && this._ensureCapacity(a);
        this._length = a;
        this._position = d(this._position, 0, this._length);
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "bytesAvailable", {get:function() {
        return this._length - this._position;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "position", {get:function() {
        return this._position;
      }, set:function(a) {
        this._position = a >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "buffer", {get:function() {
        return this._buffer;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "bytes", {get:function() {
        return this._u8;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "ints", {get:function() {
        return this._i32;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "objectEncoding", {get:function() {
        return this._objectEncoding;
      }, set:function(a) {
        this._objectEncoding = a >>> 0;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(h.prototype, "endian", {get:function() {
        return this._littleEndian ? "littleEndian" : "bigEndian";
      }, set:function(a) {
        a = t(a);
        this._littleEndian = "auto" === a ? h._nativeLittleEndian : "littleEndian" === a;
      }, enumerable:!0, configurable:!0});
      h.prototype.toString = function() {
        return a(new Int8Array(this._buffer, 0, this._length));
      };
      h.prototype.toBlob = function() {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)]);
      };
      h.prototype.writeMultiByte = function(a, b) {
        m("packageInternal flash.utils.ObjectOutput::writeMultiByte");
      };
      h.prototype.readMultiByte = function(a, b) {
        m("packageInternal flash.utils.ObjectInput::readMultiByte");
      };
      h.prototype.getValue = function(a) {
        a |= 0;
        return a >= this._length ? void 0 : this._u8[a];
      };
      h.prototype.setValue = function(a, b) {
        a |= 0;
        var d = a + 1;
        this._ensureCapacity(d);
        this._u8[a] = b;
        d > this._length && (this._length = d);
      };
      h.prototype.readFixed = function() {
        return this.readInt() / 65536;
      };
      h.prototype.readFixed8 = function() {
        return this.readShort() / 256;
      };
      h.prototype.readFloat16 = function() {
        var a = this.readUnsignedShort(), b = a >> 15 ? -1 : 1, d = (a & 31744) >> 10, a = a & 1023;
        return d ? 31 === d ? a ? NaN : Infinity * b : b * Math.pow(2, d - 15) * (1 + a / 1024) : a / 1024 * Math.pow(2, -14) * b;
      };
      h.prototype.readEncodedU32 = function() {
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
      h.prototype.readBits = function(a) {
        return this.readUnsignedBits(a) << 32 - a >> 32 - a;
      };
      h.prototype.readUnsignedBits = function(a) {
        for (var b = this._bitBuffer, d = this._bitLength;a > d;) {
          b = b << 8 | this.readUnsignedByte(), d += 8;
        }
        d -= a;
        a = b >>> d & e[a];
        this._bitBuffer = b;
        this._bitLength = d;
        return a;
      };
      h.prototype.readFixedBits = function(a) {
        return this.readBits(a) / 65536;
      };
      h.prototype.readString = function(d) {
        var b = this._position;
        if (d) {
          b + d > this._length && c(), this._position += d;
        } else {
          d = 0;
          for (var e = b;e < this._length && this._u8[e];e++) {
            d++;
          }
          this._position += d + 1;
        }
        return a(new Int8Array(this._buffer, b, d));
      };
      h.prototype.align = function() {
        this._bitLength = this._bitBuffer = 0;
      };
      h.prototype._compress = function(a) {
        a = t(a);
        switch(a) {
          case "zlib":
            a = new u.Deflate(!0);
            break;
          case "deflate":
            a = new u.Deflate(!1);
            break;
          default:
            return;
        }
        var b = new h;
        a.onData = b.writeRawBytes.bind(b);
        a.push(this._u8.subarray(0, this._length));
        a.finish();
        this._ensureCapacity(b._u8.length);
        this._u8.set(b._u8);
        this.length = b.length;
        this._position = 0;
      };
      h.prototype._uncompress = function(a) {
        a = t(a);
        switch(a) {
          case "zlib":
            a = new u.Inflate(!0);
            break;
          case "deflate":
            a = new u.Inflate(!1);
            break;
          default:
            return;
        }
        var b = new h;
        a.onData = b.writeRawBytes.bind(b);
        a.push(this._u8.subarray(0, this._length));
        a.error && m("throwCompressedDataError");
        this._ensureCapacity(b._u8.length);
        this._u8.set(b._u8);
        this.length = b.length;
        this._position = 0;
      };
      h._nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
      h.INITIAL_SIZE = 128;
      h._arrayBufferPool = new f.ArrayBufferPool;
      return h;
    }();
    u.DataBuffer = h;
  })(f.ArrayUtilities || (f.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  var u = f.ArrayUtilities.DataBuffer, c = f.ArrayUtilities.ensureTypedArrayCapacity, t = f.Debug.assert;
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
  })(f.PathCommand || (f.PathCommand = {}));
  (function(a) {
    a[a.Linear = 16] = "Linear";
    a[a.Radial = 18] = "Radial";
  })(f.GradientType || (f.GradientType = {}));
  (function(a) {
    a[a.Pad = 0] = "Pad";
    a[a.Reflect = 1] = "Reflect";
    a[a.Repeat = 2] = "Repeat";
  })(f.GradientSpreadMethod || (f.GradientSpreadMethod = {}));
  (function(a) {
    a[a.RGB = 0] = "RGB";
    a[a.LinearRGB = 1] = "LinearRGB";
  })(f.GradientInterpolationMethod || (f.GradientInterpolationMethod = {}));
  (function(a) {
    a[a.None = 0] = "None";
    a[a.Normal = 1] = "Normal";
    a[a.Vertical = 2] = "Vertical";
    a[a.Horizontal = 3] = "Horizontal";
  })(f.LineScaleMode || (f.LineScaleMode = {}));
  var m = function() {
    return function(a, d, c, e, h, n, r, t, b) {
      this.commands = a;
      this.commandsPosition = d;
      this.coordinates = c;
      this.coordinatesPosition = e;
      this.morphCoordinates = h;
      this.styles = n;
      this.stylesLength = r;
      this.hasFills = t;
      this.hasLines = b;
    };
  }();
  f.PlainObjectShapeData = m;
  var r;
  (function(a) {
    a[a.Commands = 32] = "Commands";
    a[a.Coordinates = 128] = "Coordinates";
    a[a.Styles = 16] = "Styles";
  })(r || (r = {}));
  r = function() {
    function a(a) {
      "undefined" === typeof a && (a = !0);
      a && this.clear();
    }
    a.FromPlainObject = function(d) {
      var c = new a(!1);
      c.commands = d.commands;
      c.coordinates = d.coordinates;
      c.morphCoordinates = d.morphCoordinates;
      c.commandsPosition = d.commandsPosition;
      c.coordinatesPosition = d.coordinatesPosition;
      c.styles = u.FromArrayBuffer(d.styles, d.stylesLength);
      c.styles.endian = "auto";
      c.hasFills = d.hasFills;
      c.hasLines = d.hasLines;
      return c;
    };
    a.prototype.moveTo = function(a, c) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 9;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = c;
    };
    a.prototype.lineTo = function(a, c) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 10;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = c;
    };
    a.prototype.curveTo = function(a, c, e, h) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = c;
      this.coordinates[this.coordinatesPosition++] = e;
      this.coordinates[this.coordinatesPosition++] = h;
    };
    a.prototype.cubicCurveTo = function(a, c, e, h, n, r) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = c;
      this.coordinates[this.coordinatesPosition++] = e;
      this.coordinates[this.coordinatesPosition++] = h;
      this.coordinates[this.coordinatesPosition++] = n;
      this.coordinates[this.coordinatesPosition++] = r;
    };
    a.prototype.beginFill = function(a) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 1;
      this.styles.writeUnsignedInt(a);
      this.hasFills = !0;
    };
    a.prototype.endFill = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 4;
    };
    a.prototype.endLine = function() {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 8;
    };
    a.prototype.lineStyle = function(a, c, e, h, n, r, k) {
      t(a === (a | 0), 0 <= a && 5100 >= a);
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5;
      this.coordinates[this.coordinatesPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(c);
      a.writeBoolean(e);
      a.writeUnsignedByte(h);
      a.writeUnsignedByte(n);
      a.writeUnsignedByte(r);
      a.writeUnsignedByte(k);
      this.hasLines = !0;
    };
    a.prototype.beginBitmap = function(a, c, e, h, n) {
      t(3 === a || 7 === a);
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(c);
      this._writeStyleMatrix(e);
      a.writeBoolean(h);
      a.writeBoolean(n);
      this.hasFills = !0;
    };
    a.prototype.beginGradient = function(a, c, e, h, n, r, k, b) {
      t(2 === a || 6 === a);
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedByte(h);
      t(b === (b | 0));
      a.writeShort(b);
      this._writeStyleMatrix(n);
      h = c.length;
      a.writeByte(h);
      for (n = 0;n < h;n++) {
        a.writeUnsignedByte(e[n]), a.writeUnsignedInt(c[n]);
      }
      a.writeUnsignedByte(r);
      a.writeUnsignedByte(k);
      this.hasFills = !0;
    };
    a.prototype.writeCommandAndCoordinates = function(a, c, e) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = c;
      this.coordinates[this.coordinatesPosition++] = e;
    };
    a.prototype.writeCoordinates = function(a, c) {
      this.ensurePathCapacities(0, 2);
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = c;
    };
    a.prototype.writeMorphCoordinates = function(a, p) {
      this.morphCoordinates = c(this.morphCoordinates, this.coordinatesPosition);
      this.morphCoordinates[this.coordinatesPosition - 2] = a;
      this.morphCoordinates[this.coordinatesPosition - 1] = p;
    };
    a.prototype.clear = function() {
      this.commandsPosition = this.coordinatesPosition = 0;
      this.commands = new Uint8Array(32);
      this.coordinates = new Int32Array(128);
      this.styles = new u(16);
      this.styles.endian = "auto";
      this.hasFills = this.hasLines = !1;
    };
    a.prototype.isEmpty = function() {
      return 0 === this.commandsPosition;
    };
    a.prototype.clone = function() {
      var d = new a(!1);
      d.commands = new Uint8Array(this.commands);
      d.commandsPosition = this.commandsPosition;
      d.coordinates = new Int32Array(this.coordinates);
      d.coordinatesPosition = this.coordinatesPosition;
      d.styles = new u(this.styles.length);
      d.styles.writeRawBytes(this.styles.bytes);
      d.hasFills = this.hasFills;
      d.hasLines = this.hasLines;
      return d;
    };
    a.prototype.toPlainObject = function() {
      return new m(this.commands, this.commandsPosition, this.coordinates, this.coordinatesPosition, this.morphCoordinates, this.styles.buffer, this.styles.length, this.hasFills, this.hasLines);
    };
    Object.defineProperty(a.prototype, "buffers", {get:function() {
      var a = [this.commands.buffer, this.coordinates.buffer, this.styles.buffer];
      this.morphCoordinates && a.push(this.morphCoordinates.buffer);
      return a;
    }, enumerable:!0, configurable:!0});
    a.prototype._writeStyleMatrix = function(a) {
      var c = this.styles;
      c.writeFloat(a.a);
      c.writeFloat(a.b);
      c.writeFloat(a.c);
      c.writeFloat(a.d);
      c.writeFloat(a.tx);
      c.writeFloat(a.ty);
    };
    a.prototype.ensurePathCapacities = function(a, p) {
      this.commands = c(this.commands, this.commandsPosition + a);
      this.coordinates = c(this.coordinates, this.coordinatesPosition + p);
    };
    return a;
  }();
  f.ShapeData = r;
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
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
        c[c.CODE_FRAME_TAG = 47] = "CODE_FRAME_TAG";
        c[c.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        c[c.CODE_GEN_COMMAND = 49] = "CODE_GEN_COMMAND";
        c[c.CODE_DEFINE_COMMAND_OBJ = 50] = "CODE_DEFINE_COMMAND_OBJ";
        c[c.CODE_CHARACTER_SET = 51] = "CODE_CHARACTER_SET";
        c[c.CODE_FONT_REF = 52] = "CODE_FONT_REF";
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
        c[c.CODE_DO_ABC_ = 72] = "CODE_DO_ABC_";
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
      })(c.SwfTag || (c.SwfTag = {}));
      (function(c) {
        c[c.Reserved = 2048] = "Reserved";
        c[c.OpaqueBackground = 1024] = "OpaqueBackground";
        c[c.HasVisible = 512] = "HasVisible";
        c[c.HasImage = 256] = "HasImage";
        c[c.HasClassName = 2048] = "HasClassName";
        c[c.HasCacheAsBitmap = 1024] = "HasCacheAsBitmap";
        c[c.HasBlendMode = 512] = "HasBlendMode";
        c[c.HasFilterList = 256] = "HasFilterList";
        c[c.HasClipActions = 128] = "HasClipActions";
        c[c.HasClipDepth = 64] = "HasClipDepth";
        c[c.HasName = 32] = "HasName";
        c[c.HasRatio = 16] = "HasRatio";
        c[c.HasColorTransform = 8] = "HasColorTransform";
        c[c.HasMatrix = 4] = "HasMatrix";
        c[c.HasCharacter = 2] = "HasCharacter";
        c[c.Move = 1] = "Move";
      })(c.PlaceObjectFlags || (c.PlaceObjectFlags = {}));
    })(f.Parser || (f.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  var u = f.Debug.unexpected, c = function() {
    function c(f, r, a, d) {
      this.url = f;
      this.method = r;
      this.mimeType = a;
      this.data = d;
    }
    c.prototype.readAll = function(c, r) {
      var a = this.url, d = new XMLHttpRequest({mozSystem:!0});
      d.open(this.method || "GET", this.url, !0);
      d.responseType = "arraybuffer";
      c && (d.onprogress = function(a) {
        c(d.response, a.loaded, a.total);
      });
      d.onreadystatechange = function(c) {
        4 === d.readyState && (200 !== d.status && 0 !== d.status || null === d.response ? (u("Path: " + a + " not found."), r(null, d.statusText)) : r(d.response));
      };
      this.mimeType && d.setRequestHeader("Content-Type", this.mimeType);
      d.send(this.data || null);
    };
    c.prototype.readAsync = function(c, r, a, d, p) {
      var e = new XMLHttpRequest({mozSystem:!0}), h = this.url, n = 0, q = 0;
      e.open(this.method || "GET", h, !0);
      e.responseType = "moz-chunked-arraybuffer";
      var k = "moz-chunked-arraybuffer" !== e.responseType;
      k && (e.responseType = "arraybuffer");
      e.onprogress = function(b) {
        k || (n = b.loaded, q = b.total, c(new Uint8Array(e.response), {loaded:n, total:q}));
      };
      e.onreadystatechange = function(b) {
        2 === e.readyState && p && p(h, e.status, e.getAllResponseHeaders());
        4 === e.readyState && (200 !== e.status && 0 !== e.status || null === e.response && (0 === q || n !== q) ? r(e.statusText) : (k && (b = e.response, c(new Uint8Array(b), {loaded:0, total:b.byteLength})), d && d()));
      };
      this.mimeType && e.setRequestHeader("Content-Type", this.mimeType);
      e.send(this.data || null);
      a && a();
    };
    return c;
  }();
  f.BinaryFileReader = c;
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      c[c.Objects = 0] = "Objects";
      c[c.References = 1] = "References";
    })(f.RemotingPhase || (f.RemotingPhase = {}));
    (function(c) {
      c[c.HasMatrix = 1] = "HasMatrix";
      c[c.HasBounds = 2] = "HasBounds";
      c[c.HasChildren = 4] = "HasChildren";
      c[c.HasColorTransform = 8] = "HasColorTransform";
      c[c.HasClipRect = 16] = "HasClipRect";
      c[c.HasMiscellaneousProperties = 32] = "HasMiscellaneousProperties";
      c[c.HasMask = 64] = "HasMask";
      c[c.HasClip = 128] = "HasClip";
    })(f.MessageBits || (f.MessageBits = {}));
    (function(c) {
      c[c.None = 0] = "None";
      c[c.Asset = 134217728] = "Asset";
    })(f.IDMask || (f.IDMask = {}));
    (function(c) {
      c[c.EOF = 0] = "EOF";
      c[c.UpdateFrame = 100] = "UpdateFrame";
      c[c.UpdateGraphics = 101] = "UpdateGraphics";
      c[c.UpdateBitmapData = 102] = "UpdateBitmapData";
      c[c.UpdateTextContent = 103] = "UpdateTextContent";
      c[c.UpdateStage = 104] = "UpdateStage";
      c[c.UpdateNetStream = 105] = "UpdateNetStream";
      c[c.RequestBitmapData = 106] = "RequestBitmapData";
      c[c.DecodeImage = 107] = "DecodeImage";
      c[c.DecodeImageResponse = 108] = "DecodeImageResponse";
      c[c.RegisterFont = 200] = "RegisterFont";
      c[c.DrawToBitmap = 201] = "DrawToBitmap";
      c[c.MouseEvent = 300] = "MouseEvent";
      c[c.KeyboardEvent = 301] = "KeyboardEvent";
      c[c.FocusEvent = 302] = "FocusEvent";
    })(f.MessageTag || (f.MessageTag = {}));
    (function(c) {
      c[c.Identity = 0] = "Identity";
      c[c.AlphaMultiplierOnly = 1] = "AlphaMultiplierOnly";
      c[c.All = 2] = "All";
    })(f.ColorTransformEncoding || (f.ColorTransformEncoding = {}));
    f.MouseEventNames = ["click", "dblclick", "mousedown", "mousemove", "mouseup"];
    f.KeyboardEventNames = ["keydown", "keypress", "keyup"];
    (function(c) {
      c[c.CtrlKey = 1] = "CtrlKey";
      c[c.AltKey = 2] = "AltKey";
      c[c.ShiftKey = 4] = "ShiftKey";
    })(f.KeyboardEventFlags || (f.KeyboardEventFlags = {}));
    (function(c) {
      c[c.DocumentHidden = 0] = "DocumentHidden";
      c[c.DocumentVisible = 1] = "DocumentVisible";
      c[c.WindowBlur = 2] = "WindowBlur";
      c[c.WindowFocus = 3] = "WindowFocus";
    })(f.FocusEventType || (f.FocusEventType = {}));
  })(f.Remoting || (f.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      var f = function() {
        function c() {
        }
        c.toRGBA = function(a, d, c, e) {
          "undefined" === typeof e && (e = 1);
          return "rgba(" + a + "," + d + "," + c + "," + e + ")";
        };
        return c;
      }();
      c.UI = f;
      var m = function() {
        function c() {
        }
        c.prototype.tabToolbar = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(37, 44, 51, a);
        };
        c.prototype.toolbars = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(52, 60, 69, a);
        };
        c.prototype.selectionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(29, 79, 115, a);
        };
        c.prototype.selectionText = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(245, 247, 250, a);
        };
        c.prototype.splitters = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(0, 0, 0, a);
        };
        c.prototype.bodyBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(17, 19, 21, a);
        };
        c.prototype.sidebarBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(24, 29, 32, a);
        };
        c.prototype.attentionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(161, 134, 80, a);
        };
        c.prototype.bodyText = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(143, 161, 178, a);
        };
        c.prototype.foregroundTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(182, 186, 191, a);
        };
        c.prototype.contentTextHighContrast = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(169, 186, 203, a);
        };
        c.prototype.contentTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(143, 161, 178, a);
        };
        c.prototype.contentTextDarkGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(95, 115, 135, a);
        };
        c.prototype.blueHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(70, 175, 227, a);
        };
        c.prototype.purpleHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(107, 122, 187, a);
        };
        c.prototype.pinkHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(223, 128, 255, a);
        };
        c.prototype.redHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(235, 83, 104, a);
        };
        c.prototype.orangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(217, 102, 41, a);
        };
        c.prototype.lightOrangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(217, 155, 40, a);
        };
        c.prototype.greenHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(112, 191, 83, a);
        };
        c.prototype.blueGreyHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(94, 136, 176, a);
        };
        return c;
      }();
      c.UIThemeDark = m;
      m = function() {
        function c() {
        }
        c.prototype.tabToolbar = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(235, 236, 237, a);
        };
        c.prototype.toolbars = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(240, 241, 242, a);
        };
        c.prototype.selectionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(76, 158, 217, a);
        };
        c.prototype.selectionText = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(245, 247, 250, a);
        };
        c.prototype.splitters = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(170, 170, 170, a);
        };
        c.prototype.bodyBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(252, 252, 252, a);
        };
        c.prototype.sidebarBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(247, 247, 247, a);
        };
        c.prototype.attentionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(161, 134, 80, a);
        };
        c.prototype.bodyText = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(24, 25, 26, a);
        };
        c.prototype.foregroundTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(88, 89, 89, a);
        };
        c.prototype.contentTextHighContrast = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(41, 46, 51, a);
        };
        c.prototype.contentTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(143, 161, 178, a);
        };
        c.prototype.contentTextDarkGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(102, 115, 128, a);
        };
        c.prototype.blueHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(0, 136, 204, a);
        };
        c.prototype.purpleHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(91, 95, 255, a);
        };
        c.prototype.pinkHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(184, 46, 229, a);
        };
        c.prototype.redHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(237, 38, 85, a);
        };
        c.prototype.orangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(241, 60, 0, a);
        };
        c.prototype.lightOrangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(217, 126, 0, a);
        };
        c.prototype.greenHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(44, 187, 15, a);
        };
        c.prototype.blueGreyHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return f.toRGBA(95, 136, 176, a);
        };
        return c;
      }();
      c.UIThemeLight = m;
    })(f.Theme || (f.Theme = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      var f = function() {
        function c(f) {
          this._buffers = f || [];
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
          for (var a = 0, d = this.snapshotCount;a < d;a++) {
            c(this._snapshots[a], a);
          }
        };
        c.prototype.createSnapshots = function() {
          var c = Number.MAX_VALUE, a = Number.MIN_VALUE, d = 0;
          for (this._snapshots = [];0 < this._buffers.length;) {
            var p = this._buffers.shift().createSnapshot();
            p && (c > p.startTime && (c = p.startTime), a < p.endTime && (a = p.endTime), d < p.maxDepth && (d = p.maxDepth), this._snapshots.push(p));
          }
          this._startTime = c;
          this._endTime = a;
          this._windowStart = c;
          this._windowEnd = a;
          this._maxDepth = d;
        };
        c.prototype.setWindow = function(c, a) {
          if (c > a) {
            var d = c;
            c = a;
            a = d;
          }
          d = Math.min(a - c, this.totalTime);
          c < this._startTime ? (c = this._startTime, a = this._startTime + d) : a > this._endTime && (c = this._endTime - d, a = this._endTime);
          this._windowStart = c;
          this._windowEnd = a;
        };
        c.prototype.moveWindowTo = function(c) {
          this.setWindow(c - this.windowLength / 2, c + this.windowLength / 2);
        };
        return c;
      }();
      c.Profile = f;
    })(f.Profiler || (f.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function(f, u) {
  function c() {
    this.constructor = f;
  }
  for (var t in u) {
    u.hasOwnProperty(t) && (f[t] = u[t]);
  }
  c.prototype = u.prototype;
  f.prototype = new c;
};
(function(f) {
  (function(f) {
    (function(c) {
      var f = function() {
        return function(c) {
          this.kind = c;
          this.totalTime = this.selfTime = this.count = 0;
        };
      }();
      c.TimelineFrameStatistics = f;
      var m = function() {
        function c(a, d, p, e, h, n) {
          this.parent = a;
          this.kind = d;
          this.startData = p;
          this.endData = e;
          this.startTime = h;
          this.endTime = n;
          this.maxDepth = 0;
        }
        Object.defineProperty(c.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(c.prototype, "selfTime", {get:function() {
          var a = this.totalTime;
          if (this.children) {
            for (var d = 0, c = this.children.length;d < c;d++) {
              var e = this.children[d], a = a - (e.endTime - e.startTime)
            }
          }
          return a;
        }, enumerable:!0, configurable:!0});
        c.prototype.getChildIndex = function(a) {
          for (var d = this.children, c = 0;c < d.length;c++) {
            if (d[c].endTime > a) {
              return c;
            }
          }
          return 0;
        };
        c.prototype.getChildRange = function(a, d) {
          if (this.children && a <= this.endTime && d >= this.startTime && d >= a) {
            var c = this._getNearestChild(a), e = this._getNearestChildReverse(d);
            if (c <= e) {
              return a = this.children[c].startTime, d = this.children[e].endTime, {startIndex:c, endIndex:e, startTime:a, endTime:d, totalTime:d - a};
            }
          }
          return null;
        };
        c.prototype._getNearestChild = function(a) {
          var d = this.children;
          if (d && d.length) {
            if (a <= d[0].endTime) {
              return 0;
            }
            for (var c, e = 0, h = d.length - 1;h > e;) {
              c = (e + h) / 2 | 0;
              var n = d[c];
              if (a >= n.startTime && a <= n.endTime) {
                return c;
              }
              a > n.endTime ? e = c + 1 : h = c;
            }
            return Math.ceil((e + h) / 2);
          }
          return 0;
        };
        c.prototype._getNearestChildReverse = function(a) {
          var d = this.children;
          if (d && d.length) {
            var c = d.length - 1;
            if (a >= d[c].startTime) {
              return c;
            }
            for (var e, h = 0;c > h;) {
              e = Math.ceil((h + c) / 2);
              var n = d[e];
              if (a >= n.startTime && a <= n.endTime) {
                return e;
              }
              a > n.endTime ? h = e : c = e - 1;
            }
            return(h + c) / 2 | 0;
          }
          return 0;
        };
        c.prototype.query = function(a) {
          if (a < this.startTime || a > this.endTime) {
            return null;
          }
          var d = this.children;
          if (d && 0 < d.length) {
            for (var c, e = 0, h = d.length - 1;h > e;) {
              var n = (e + h) / 2 | 0;
              c = d[n];
              if (a >= c.startTime && a <= c.endTime) {
                return c.query(a);
              }
              a > c.endTime ? e = n + 1 : h = n;
            }
            c = d[h];
            if (a >= c.startTime && a <= c.endTime) {
              return c.query(a);
            }
          }
          return this;
        };
        c.prototype.queryNext = function(a) {
          for (var d = this;a > d.endTime;) {
            if (d.parent) {
              d = d.parent;
            } else {
              break;
            }
          }
          return d.query(a);
        };
        c.prototype.getDepth = function() {
          for (var a = 0, d = this;d;) {
            a++, d = d.parent;
          }
          return a;
        };
        c.prototype.calculateStatistics = function() {
          function a(c) {
            if (c.kind) {
              var e = d[c.kind.id] || (d[c.kind.id] = new f(c.kind));
              e.count++;
              e.selfTime += c.selfTime;
              e.totalTime += c.totalTime;
            }
            c.children && c.children.forEach(a);
          }
          var d = this.statistics = [];
          a(this);
        };
        return c;
      }();
      c.TimelineFrame = m;
      m = function(c) {
        function a(a) {
          c.call(this, null, null, null, null, NaN, NaN);
          this.name = a;
        }
        __extends(a, c);
        return a;
      }(m);
      c.TimelineBufferSnapshot = m;
    })(f.Profiler || (f.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.ObjectUtilities.createEmptyObject, m = function() {
        function r(a, d) {
          "undefined" === typeof a && (a = "");
          this.name = a || "";
          this._startTime = f.isNullOrUndefined(d) ? performance.now() : d;
        }
        r.prototype.getKind = function(a) {
          return this._kinds[a];
        };
        Object.defineProperty(r.prototype, "kinds", {get:function() {
          return this._kinds.concat();
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(r.prototype, "depth", {get:function() {
          return this._depth;
        }, enumerable:!0, configurable:!0});
        r.prototype._initialize = function() {
          this._depth = 0;
          this._stack = [];
          this._data = [];
          this._kinds = [];
          this._kindNameMap = t();
          this._marks = new f.CircularBuffer(Int32Array, 20);
          this._times = new f.CircularBuffer(Float64Array, 20);
        };
        r.prototype._getKindId = function(a) {
          var d = r.MAX_KINDID;
          if (void 0 === this._kindNameMap[a]) {
            if (d = this._kinds.length, d < r.MAX_KINDID) {
              var c = {id:d, name:a, visible:!0};
              this._kinds.push(c);
              this._kindNameMap[a] = c;
            } else {
              d = r.MAX_KINDID;
            }
          } else {
            d = this._kindNameMap[a].id;
          }
          return d;
        };
        r.prototype._getMark = function(a, d, c) {
          var e = r.MAX_DATAID;
          f.isNullOrUndefined(c) || d === r.MAX_KINDID || (e = this._data.length, e < r.MAX_DATAID ? this._data.push(c) : e = r.MAX_DATAID);
          return a | e << 16 | d;
        };
        r.prototype.enter = function(a, d, c) {
          c = (f.isNullOrUndefined(c) ? performance.now() : c) - this._startTime;
          this._marks || this._initialize();
          this._depth++;
          a = this._getKindId(a);
          this._marks.write(this._getMark(r.ENTER, a, d));
          this._times.write(c);
          this._stack.push(a);
        };
        r.prototype.leave = function(a, d, c) {
          c = (f.isNullOrUndefined(c) ? performance.now() : c) - this._startTime;
          var e = this._stack.pop();
          a && (e = this._getKindId(a));
          this._marks.write(this._getMark(r.LEAVE, e, d));
          this._times.write(c);
          this._depth--;
        };
        r.prototype.count = function(a, d, c) {
        };
        r.prototype.createSnapshot = function() {
          var a;
          "undefined" === typeof a && (a = Number.MAX_VALUE);
          if (!this._marks) {
            return null;
          }
          var d = this._times, p = this._kinds, e = this._data, h = new c.TimelineBufferSnapshot(this.name), n = [h], q = 0;
          this._marks || this._initialize();
          this._marks.forEachInReverse(function(h, b) {
            var g = e[h >>> 16 & r.MAX_DATAID], m = p[h & r.MAX_KINDID];
            if (f.isNullOrUndefined(m) || m.visible) {
              var l = h & 2147483648, s = d.get(b), A = n.length;
              if (l === r.LEAVE) {
                if (1 === A && (q++, q > a)) {
                  return!0;
                }
                n.push(new c.TimelineFrame(n[A - 1], m, null, g, NaN, s));
              } else {
                if (l === r.ENTER) {
                  if (m = n.pop(), l = n[n.length - 1]) {
                    for (l.children ? l.children.unshift(m) : l.children = [m], l = n.length, m.depth = l, m.startData = g, m.startTime = s;m;) {
                      if (m.maxDepth < l) {
                        m.maxDepth = l, m = m.parent;
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
          h.children && h.children.length && (h.startTime = h.children[0].startTime, h.endTime = h.children[h.children.length - 1].endTime);
          return h;
        };
        r.prototype.reset = function(a) {
          this._startTime = f.isNullOrUndefined(a) ? performance.now() : a;
          this._marks ? (this._depth = 0, this._data = [], this._marks.reset(), this._times.reset()) : this._initialize();
        };
        r.FromFirefoxProfile = function(a, d) {
          for (var c = a.profile.threads[0].samples, e = new r(d, c[0].time), h = [], n, f = 0;f < c.length;f++) {
            n = c[f];
            var k = n.time, b = n.frames, g = 0;
            for (n = Math.min(b.length, h.length);g < n && b[g].location === h[g].location;) {
              g++;
            }
            for (var m = h.length - g, l = 0;l < m;l++) {
              n = h.pop(), e.leave(n.location, null, k);
            }
            for (;g < b.length;) {
              n = b[g++], e.enter(n.location, null, k);
            }
            h = b;
          }
          for (;n = h.pop();) {
            e.leave(n.location, null, k);
          }
          return e;
        };
        r.FromChromeProfile = function(a, d) {
          var c = a.timestamps, e = a.samples, h = new r(d, c[0] / 1E3), n = [], f = {}, k;
          r._resolveIds(a.head, f);
          for (var b = 0;b < c.length;b++) {
            var g = c[b] / 1E3, m = [];
            for (k = f[e[b]];k;) {
              m.unshift(k), k = k.parent;
            }
            var l = 0;
            for (k = Math.min(m.length, n.length);l < k && m[l] === n[l];) {
              l++;
            }
            for (var s = n.length - l, A = 0;A < s;A++) {
              k = n.pop(), h.leave(k.functionName, null, g);
            }
            for (;l < m.length;) {
              k = m[l++], h.enter(k.functionName, null, g);
            }
            n = m;
          }
          for (;k = n.pop();) {
            h.leave(k.functionName, null, g);
          }
          return h;
        };
        r._resolveIds = function(a, c) {
          c[a.id] = a;
          if (a.children) {
            for (var p = 0;p < a.children.length;p++) {
              a.children[p].parent = a, r._resolveIds(a.children[p], c);
            }
          }
        };
        r.ENTER = 0;
        r.LEAVE = -2147483648;
        r.MAX_KINDID = 65535;
        r.MAX_DATAID = 32767;
        return r;
      }();
      c.TimelineBuffer = m;
    })(u.Profiler || (u.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      (function(c) {
        c[c.DARK = 0] = "DARK";
        c[c.LIGHT = 1] = "LIGHT";
      })(c.UIThemeType || (c.UIThemeType = {}));
      var t = function() {
        function m(c, a) {
          "undefined" === typeof a && (a = 0);
          this._container = c;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = a;
          this._tooltip = this._createTooltip();
        }
        m.prototype.createProfile = function(f, a) {
          "undefined" === typeof a && (a = !0);
          var d = new c.Profile(f);
          d.createSnapshots();
          this._profiles.push(d);
          a && this.activateProfile(d);
          return d;
        };
        m.prototype.activateProfile = function(c) {
          this.deactivateProfile();
          this._activeProfile = c;
          this._createViews();
          this._initializeViews();
        };
        m.prototype.activateProfileAt = function(c) {
          this.activateProfile(this.getProfileAt(c));
        };
        m.prototype.deactivateProfile = function() {
          this._activeProfile && (this._destroyViews(), this._activeProfile = null);
        };
        m.prototype.resize = function() {
          this._onResize();
        };
        m.prototype.getProfileAt = function(c) {
          return this._profiles[c];
        };
        Object.defineProperty(m.prototype, "activeProfile", {get:function() {
          return this._activeProfile;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(m.prototype, "profileCount", {get:function() {
          return this._profiles.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(m.prototype, "container", {get:function() {
          return this._container;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(m.prototype, "themeType", {get:function() {
          return this._themeType;
        }, set:function(c) {
          switch(c) {
            case 0:
              this._theme = new u.Theme.UIThemeDark;
              break;
            case 1:
              this._theme = new u.Theme.UIThemeLight;
          }
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(m.prototype, "theme", {get:function() {
          return this._theme;
        }, enumerable:!0, configurable:!0});
        m.prototype.getSnapshotAt = function(c) {
          return this._activeProfile.getSnapshotAt(c);
        };
        m.prototype._createViews = function() {
          if (this._activeProfile) {
            var f = this;
            this._overviewHeader = new c.FlameChartHeader(this, 0);
            this._overview = new c.FlameChartOverview(this, 0);
            this._activeProfile.forEachSnapshot(function(a, d) {
              f._headers.push(new c.FlameChartHeader(f, 1));
              f._charts.push(new c.FlameChart(f, a));
            });
            window.addEventListener("resize", this._onResize.bind(this));
          }
        };
        m.prototype._destroyViews = function() {
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
        m.prototype._initializeViews = function() {
          if (this._activeProfile) {
            var c = this, a = this._activeProfile.startTime, d = this._activeProfile.endTime;
            this._overviewHeader.initialize(a, d);
            this._overview.initialize(a, d);
            this._activeProfile.forEachSnapshot(function(p, e) {
              c._headers[e].initialize(a, d);
              c._charts[e].initialize(a, d);
            });
          }
        };
        m.prototype._onResize = function() {
          if (this._activeProfile) {
            var c = this, a = this._container.offsetWidth;
            this._overviewHeader.setSize(a);
            this._overview.setSize(a);
            this._activeProfile.forEachSnapshot(function(d, p) {
              c._headers[p].setSize(a);
              c._charts[p].setSize(a);
            });
          }
        };
        m.prototype._updateViews = function() {
          if (this._activeProfile) {
            var c = this, a = this._activeProfile.windowStart, d = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(a, d);
            this._overview.setWindow(a, d);
            this._activeProfile.forEachSnapshot(function(p, e) {
              c._headers[e].setWindow(a, d);
              c._charts[e].setWindow(a, d);
            });
          }
        };
        m.prototype._drawViews = function() {
        };
        m.prototype._createTooltip = function() {
          var c = document.createElement("div");
          c.classList.add("profiler-tooltip");
          c.style.display = "none";
          this._container.insertBefore(c, this._container.firstChild);
          return c;
        };
        m.prototype.setWindow = function(c, a) {
          this._activeProfile.setWindow(c, a);
          this._updateViews();
        };
        m.prototype.moveWindowTo = function(c) {
          this._activeProfile.moveWindowTo(c);
          this._updateViews();
        };
        m.prototype.showTooltip = function(c, a, d, p) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(c, a));
          this._tooltip.style.display = "block";
          var e = this._tooltip.firstChild;
          a = e.clientWidth;
          e = e.clientHeight;
          d += d + a >= c.canvas.clientWidth - 50 ? -(a + 20) : 25;
          p += c.canvas.offsetTop - e / 2;
          this._tooltip.style.left = d + "px";
          this._tooltip.style.top = p + "px";
        };
        m.prototype.hideTooltip = function() {
          this._tooltip.style.display = "none";
        };
        m.prototype.createTooltipContent = function(c, a) {
          var d = Math.round(1E5 * a.totalTime) / 1E5, p = Math.round(1E5 * a.selfTime) / 1E5, e = Math.round(1E4 * a.selfTime / a.totalTime) / 100, h = document.createElement("div"), n = document.createElement("h1");
          n.textContent = a.kind.name;
          h.appendChild(n);
          n = document.createElement("p");
          n.textContent = "Total: " + d + " ms";
          h.appendChild(n);
          d = document.createElement("p");
          d.textContent = "Self: " + p + " ms (" + e + "%)";
          h.appendChild(d);
          if (p = c.getStatistics(a.kind)) {
            e = document.createElement("p"), e.textContent = "Count: " + p.count, h.appendChild(e), e = Math.round(1E5 * p.totalTime) / 1E5, d = document.createElement("p"), d.textContent = "All Total: " + e + " ms", h.appendChild(d), p = Math.round(1E5 * p.selfTime) / 1E5, e = document.createElement("p"), e.textContent = "All Self: " + p + " ms", h.appendChild(e);
          }
          this.appendDataElements(h, a.startData);
          this.appendDataElements(h, a.endData);
          return h;
        };
        m.prototype.appendDataElements = function(c, a) {
          if (!f.isNullOrUndefined(a)) {
            c.appendChild(document.createElement("hr"));
            var d;
            if (f.isObject(a)) {
              for (var p in a) {
                d = document.createElement("p"), d.textContent = p + ": " + a[p], c.appendChild(d);
              }
            } else {
              d = document.createElement("p"), d.textContent = a.toString(), c.appendChild(d);
            }
          }
        };
        m.prototype.removeTooltipContent = function() {
          for (var c = this._tooltip;c.firstChild;) {
            c.removeChild(c.firstChild);
          }
        };
        return m;
      }();
      c.Controller = t;
    })(u.Profiler || (u.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.NumberUtilities.clamp, m = function() {
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
      c.MouseCursor = m;
      var r = function() {
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
        a.prototype.updateCursor = function(c) {
          if (!a._cursorOwner || a._cursorOwner === this._target) {
            var p = this._eventTarget.parentElement;
            a._cursor !== c && (a._cursor = c, ["", "-moz-", "-webkit-"].forEach(function(a) {
              p.style.cursor = a + c;
            }));
            a._cursorOwner = a._cursor === m.DEFAULT ? null : this._target;
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
          var e = {x:a.x - c.start.x, y:a.y - c.start.y};
          c.current = a;
          c.delta = e;
          c.hasMoved = !0;
          this._target.onDrag(c.start.x, c.start.y, a.x, a.y, e.x, e.y);
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
            var c = this._getTargetMousePos(a, a.target);
            a = t("undefined" !== typeof a.deltaY ? a.deltaY / 16 : -a.wheelDelta / 40, -1, 1);
            this._target.onMouseWheel(c.x, c.y, Math.pow(1.2, a) - 1);
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
          var e = c.getBoundingClientRect();
          return{x:a.clientX - e.left, y:a.clientY - e.top};
        };
        a.HOVER_TIMEOUT = 500;
        a._cursor = m.DEFAULT;
        return a;
      }();
      c.MouseController = r;
    })(u.Profiler || (u.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      (function(c) {
        c[c.NONE = 0] = "NONE";
        c[c.WINDOW = 1] = "WINDOW";
        c[c.HANDLE_LEFT = 2] = "HANDLE_LEFT";
        c[c.HANDLE_RIGHT = 3] = "HANDLE_RIGHT";
        c[c.HANDLE_BOTH = 4] = "HANDLE_BOTH";
      })(c.FlameChartDragTarget || (c.FlameChartDragTarget = {}));
      var f = function() {
        function f(r) {
          this._controller = r;
          this._initialized = !1;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
          this._mouseController = new c.MouseController(this, this._canvas);
          r = r.container;
          r.appendChild(this._canvas);
          r = r.getBoundingClientRect();
          this.setSize(r.width);
        }
        Object.defineProperty(f.prototype, "canvas", {get:function() {
          return this._canvas;
        }, enumerable:!0, configurable:!0});
        f.prototype.setSize = function(c, a) {
          "undefined" === typeof a && (a = 20);
          this._width = c;
          this._height = a;
          this._resetCanvas();
          this.draw();
        };
        f.prototype.initialize = function(c, a) {
          this._initialized = !0;
          this.setRange(c, a);
          this.setWindow(c, a, !1);
          this.draw();
        };
        f.prototype.setWindow = function(c, a, d) {
          "undefined" === typeof d && (d = !0);
          this._windowStart = c;
          this._windowEnd = a;
          !d || this.draw();
        };
        f.prototype.setRange = function(c, a) {
          var d = !1;
          "undefined" === typeof d && (d = !0);
          this._rangeStart = c;
          this._rangeEnd = a;
          !d || this.draw();
        };
        f.prototype.destroy = function() {
          this._mouseController.destroy();
          this._mouseController = null;
          this._controller.container.removeChild(this._canvas);
          this._controller = null;
        };
        f.prototype._resetCanvas = function() {
          var c = window.devicePixelRatio, a = this._canvas;
          a.width = this._width * c;
          a.height = this._height * c;
          a.style.width = this._width + "px";
          a.style.height = this._height + "px";
        };
        f.prototype.draw = function() {
        };
        f.prototype._almostEq = function(c, a) {
          var d;
          "undefined" === typeof d && (d = 10);
          return Math.abs(c - a) < 1 / Math.pow(10, d);
        };
        f.prototype._windowEqRange = function() {
          return this._almostEq(this._windowStart, this._rangeStart) && this._almostEq(this._windowEnd, this._rangeEnd);
        };
        f.prototype._decimalPlaces = function(c) {
          return(+c).toFixed(10).replace(/^-?\d*\.?|0+$/g, "").length;
        };
        f.prototype._toPixelsRelative = function(c) {
          return 0;
        };
        f.prototype._toPixels = function(c) {
          return 0;
        };
        f.prototype._toTimeRelative = function(c) {
          return 0;
        };
        f.prototype._toTime = function(c) {
          return 0;
        };
        f.prototype.onMouseWheel = function(c, a, d) {
          c = this._toTime(c);
          a = this._windowStart;
          var p = this._windowEnd, e = p - a;
          d = Math.max((f.MIN_WINDOW_LEN - e) / e, d);
          this._controller.setWindow(a + (a - c) * d, p + (p - c) * d);
          this.onHoverEnd();
        };
        f.prototype.onMouseDown = function(c, a) {
        };
        f.prototype.onMouseMove = function(c, a) {
        };
        f.prototype.onMouseOver = function(c, a) {
        };
        f.prototype.onMouseOut = function() {
        };
        f.prototype.onDrag = function(c, a, d, p, e, h) {
        };
        f.prototype.onDragEnd = function(c, a, d, p, e, h) {
        };
        f.prototype.onClick = function(c, a) {
        };
        f.prototype.onHoverStart = function(c, a) {
        };
        f.prototype.onHoverEnd = function() {
        };
        f.DRAGHANDLE_WIDTH = 4;
        f.MIN_WINDOW_LEN = .1;
        return f;
      }();
      c.FlameChartBase = f;
    })(f.Profiler || (f.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.StringUtilities.trimMiddle, m = f.ObjectUtilities.createEmptyObject, r = function(a) {
        function d(c, d) {
          a.call(this, c);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = d;
          this._kindStyle = m();
        }
        __extends(d, a);
        d.prototype.setSize = function(c, d) {
          a.prototype.setSize.call(this, c, d || this._initialized ? 12.5 * this._maxDepth : 100);
        };
        d.prototype.initialize = function(a, c) {
          this._initialized = !0;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(a, c);
          this.setWindow(a, c, !1);
          this.setSize(this._width, 12.5 * this._maxDepth);
        };
        d.prototype.destroy = function() {
          a.prototype.destroy.call(this);
          this._snapshot = null;
        };
        d.prototype.draw = function() {
          var a = this._context, c = window.devicePixelRatio;
          f.ColorStyle.reset();
          a.save();
          a.scale(c, c);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, this._width, this._height);
          this._initialized && this._drawChildren(this._snapshot);
          a.restore();
        };
        d.prototype._drawChildren = function(a, c) {
          "undefined" === typeof c && (c = 0);
          var d = a.getChildRange(this._windowStart, this._windowEnd);
          if (d) {
            for (var n = d.startIndex;n <= d.endIndex;n++) {
              var f = a.children[n];
              this._drawFrame(f, c) && this._drawChildren(f, c + 1);
            }
          }
        };
        d.prototype._drawFrame = function(a, c) {
          var d = this._context, n = this._toPixels(a.startTime), q = this._toPixels(a.endTime), k = q - n;
          if (k <= this._minFrameWidthInPixels) {
            return d.fillStyle = this._controller.theme.tabToolbar(1), d.fillRect(n, 12.5 * c, this._minFrameWidthInPixels, 12 + 12.5 * (a.maxDepth - a.depth)), !1;
          }
          0 > n && (q = k + n, n = 0);
          var q = q - n, b = this._kindStyle[a.kind.id];
          b || (b = f.ColorStyle.randomStyle(), b = this._kindStyle[a.kind.id] = {bgColor:b, textColor:f.ColorStyle.contrastStyle(b)});
          d.save();
          d.fillStyle = b.bgColor;
          d.fillRect(n, 12.5 * c, q, 12);
          12 < k && (k = a.kind.name) && k.length && (k = this._prepareText(d, k, q - 4), k.length && (d.fillStyle = b.textColor, d.textBaseline = "bottom", d.fillText(k, n + 2, 12.5 * (c + 1) - 1)));
          d.restore();
          return!0;
        };
        d.prototype._prepareText = function(a, c, d) {
          var n = this._measureWidth(a, c);
          if (d > n) {
            return c;
          }
          for (var n = 3, f = c.length;n < f;) {
            var k = n + f >> 1;
            this._measureWidth(a, t(c, k)) < d ? n = k + 1 : f = k;
          }
          c = t(c, f - 1);
          n = this._measureWidth(a, c);
          return n <= d ? c : "";
        };
        d.prototype._measureWidth = function(a, c) {
          var d = this._textWidth[c];
          d || (d = a.measureText(c).width, this._textWidth[c] = d);
          return d;
        };
        d.prototype._toPixelsRelative = function(a) {
          return a * this._width / (this._windowEnd - this._windowStart);
        };
        d.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - this._windowStart);
        };
        d.prototype._toTimeRelative = function(a) {
          return a * (this._windowEnd - this._windowStart) / this._width;
        };
        d.prototype._toTime = function(a) {
          return this._toTimeRelative(a) + this._windowStart;
        };
        d.prototype._getFrameAtPosition = function(a, c) {
          var d = 1 + c / 12.5 | 0, n = this._snapshot.query(this._toTime(a));
          if (n && n.depth >= d) {
            for (;n && n.depth > d;) {
              n = n.parent;
            }
            return n;
          }
          return null;
        };
        d.prototype.onMouseDown = function(a, d) {
          this._windowEqRange() || (this._mouseController.updateCursor(c.MouseCursor.ALL_SCROLL), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:1});
        };
        d.prototype.onMouseMove = function(a, c) {
        };
        d.prototype.onMouseOver = function(a, c) {
        };
        d.prototype.onMouseOut = function() {
        };
        d.prototype.onDrag = function(a, c, d, n, f, k) {
          if (a = this._dragInfo) {
            f = this._toTimeRelative(-f), this._controller.setWindow(a.windowStartInitial + f, a.windowEndInitial + f);
          }
        };
        d.prototype.onDragEnd = function(a, d, h, n, f, k) {
          this._dragInfo = null;
          this._mouseController.updateCursor(c.MouseCursor.DEFAULT);
        };
        d.prototype.onClick = function(a, d) {
          this._dragInfo = null;
          this._mouseController.updateCursor(c.MouseCursor.DEFAULT);
        };
        d.prototype.onHoverStart = function(a, c) {
          var d = this._getFrameAtPosition(a, c);
          d && (this._hoveredFrame = d, this._controller.showTooltip(this, d, a, c));
        };
        d.prototype.onHoverEnd = function() {
          this._hoveredFrame && (this._hoveredFrame = null, this._controller.hideTooltip());
        };
        d.prototype.getStatistics = function(a) {
          var c = this._snapshot;
          c.statistics || c.calculateStatistics();
          return c.statistics[a.id];
        };
        return d;
      }(c.FlameChartBase);
      c.FlameChart = r;
    })(u.Profiler || (u.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.NumberUtilities.clamp;
      (function(c) {
        c[c.OVERLAY = 0] = "OVERLAY";
        c[c.STACK = 1] = "STACK";
        c[c.UNION = 2] = "UNION";
      })(c.FlameChartOverviewMode || (c.FlameChartOverviewMode = {}));
      var m = function(f) {
        function a(a, c) {
          "undefined" === typeof c && (c = 1);
          this._mode = c;
          this._overviewCanvasDirty = !0;
          this._overviewCanvas = document.createElement("canvas");
          this._overviewContext = this._overviewCanvas.getContext("2d");
          f.call(this, a);
        }
        __extends(a, f);
        a.prototype.setSize = function(a, c) {
          f.prototype.setSize.call(this, a, c || 64);
        };
        Object.defineProperty(a.prototype, "mode", {set:function(a) {
          this._mode = a;
          this.draw();
        }, enumerable:!0, configurable:!0});
        a.prototype._resetCanvas = function() {
          f.prototype._resetCanvas.call(this);
          this._overviewCanvas.width = this._canvas.width;
          this._overviewCanvas.height = this._canvas.height;
          this._overviewCanvasDirty = !0;
        };
        a.prototype.draw = function() {
          var a = this._context, c = window.devicePixelRatio, e = this._width, h = this._height;
          a.save();
          a.scale(c, c);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, e, h);
          a.restore();
          this._initialized && (this._overviewCanvasDirty && (this._drawChart(), this._overviewCanvasDirty = !1), a.drawImage(this._overviewCanvas, 0, 0), this._drawSelection());
        };
        a.prototype._drawSelection = function() {
          var a = this._context, c = this._height, e = window.devicePixelRatio, h = this._selection ? this._selection.left : this._toPixels(this._windowStart), f = this._selection ? this._selection.right : this._toPixels(this._windowEnd), q = this._controller.theme;
          a.save();
          a.scale(e, e);
          this._selection ? (a.fillStyle = q.selectionText(.15), a.fillRect(h, 1, f - h, c - 1), a.fillStyle = "rgba(133, 0, 0, 1)", a.fillRect(h + .5, 0, f - h - 1, 4), a.fillRect(h + .5, c - 4, f - h - 1, 4)) : (a.fillStyle = q.bodyBackground(.4), a.fillRect(0, 1, h, c - 1), a.fillRect(f, 1, this._width, c - 1));
          a.beginPath();
          a.moveTo(h, 0);
          a.lineTo(h, c);
          a.moveTo(f, 0);
          a.lineTo(f, c);
          a.lineWidth = .5;
          a.strokeStyle = q.foregroundTextGrey(1);
          a.stroke();
          c = Math.abs((this._selection ? this._toTime(this._selection.right) : this._windowEnd) - (this._selection ? this._toTime(this._selection.left) : this._windowStart));
          a.fillStyle = q.selectionText(.5);
          a.font = "8px sans-serif";
          a.textBaseline = "alphabetic";
          a.textAlign = "end";
          a.fillText(c.toFixed(2), Math.min(h, f) - 4, 10);
          a.fillText((c / 60).toFixed(2), Math.min(h, f) - 4, 20);
          a.restore();
        };
        a.prototype._drawChart = function() {
          var a = window.devicePixelRatio, c = this._height, e = this._controller.activeProfile, h = 4 * this._width, f = e.totalTime / h, q = this._overviewContext, k = this._controller.theme.blueHighlight(1);
          q.save();
          q.translate(0, a * c);
          var b = -a * c / (e.maxDepth - 1);
          q.scale(a / 4, b);
          q.clearRect(0, 0, h, e.maxDepth - 1);
          1 == this._mode && q.scale(1, 1 / e.snapshotCount);
          for (var g = 0, r = e.snapshotCount;g < r;g++) {
            var l = e.getSnapshotAt(g);
            if (l) {
              var s = null, A = 0;
              q.beginPath();
              q.moveTo(0, 0);
              for (var m = 0;m < h;m++) {
                A = e.startTime + m * f, A = (s = s ? s.queryNext(A) : l.query(A)) ? s.getDepth() - 1 : 0, q.lineTo(m, A);
              }
              q.lineTo(m, 0);
              q.fillStyle = k;
              q.fill();
              1 == this._mode && q.translate(0, -c * a / b);
            }
          }
          q.restore();
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
        a.prototype._getDragTargetUnderCursor = function(a, f) {
          if (0 <= f && f < this._height) {
            var e = this._toPixels(this._windowStart), h = this._toPixels(this._windowEnd), n = 2 + c.FlameChartBase.DRAGHANDLE_WIDTH / 2, q = a >= e - n && a <= e + n, k = a >= h - n && a <= h + n;
            if (q && k) {
              return 4;
            }
            if (q) {
              return 2;
            }
            if (k) {
              return 3;
            }
            if (!this._windowEqRange() && a > e + n && a < h - n) {
              return 1;
            }
          }
          return 0;
        };
        a.prototype.onMouseDown = function(a, f) {
          var e = this._getDragTargetUnderCursor(a, f);
          0 === e ? (this._selection = {left:a, right:a}, this.draw()) : (1 === e && this._mouseController.updateCursor(c.MouseCursor.GRABBING), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:e});
        };
        a.prototype.onMouseMove = function(a, f) {
          var e = c.MouseCursor.DEFAULT, h = this._getDragTargetUnderCursor(a, f);
          0 === h || this._selection || (e = 1 === h ? c.MouseCursor.GRAB : c.MouseCursor.EW_RESIZE);
          this._mouseController.updateCursor(e);
        };
        a.prototype.onMouseOver = function(a, c) {
          this.onMouseMove(a, c);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(c.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, f, e, h, n, q) {
          if (this._selection) {
            this._selection = {left:a, right:t(e, 0, this._width - 1)}, this.draw();
          } else {
            a = this._dragInfo;
            if (4 === a.target) {
              if (0 !== n) {
                a.target = 0 > n ? 2 : 3;
              } else {
                return;
              }
            }
            f = this._windowStart;
            e = this._windowEnd;
            n = this._toTimeRelative(n);
            switch(a.target) {
              case 1:
                f = a.windowStartInitial + n;
                e = a.windowEndInitial + n;
                break;
              case 2:
                f = t(a.windowStartInitial + n, this._rangeStart, e - c.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3:
                e = t(a.windowEndInitial + n, f + c.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(f, e);
          }
        };
        a.prototype.onDragEnd = function(a, c, e, h, f, q) {
          this._selection && (this._selection = null, this._controller.setWindow(this._toTime(a), this._toTime(e)));
          this._dragInfo = null;
          this.onMouseMove(e, h);
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
      }(c.FlameChartBase);
      c.FlameChartOverview = m;
    })(u.Profiler || (u.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.NumberUtilities.clamp;
      (function(c) {
        c[c.OVERVIEW = 0] = "OVERVIEW";
        c[c.CHART = 1] = "CHART";
      })(c.FlameChartHeaderType || (c.FlameChartHeaderType = {}));
      var m = function(f) {
        function a(a, c) {
          this._type = c;
          f.call(this, a);
        }
        __extends(a, f);
        a.prototype.draw = function() {
          var a = this._context, c = window.devicePixelRatio, e = this._width, h = this._height;
          a.save();
          a.scale(c, c);
          a.fillStyle = this._controller.theme.tabToolbar(1);
          a.fillRect(0, 0, e, h);
          this._initialized && (0 == this._type ? (c = this._toPixels(this._windowStart), e = this._toPixels(this._windowEnd), a.fillStyle = this._controller.theme.bodyBackground(1), a.fillRect(c, 0, e - c, h), this._drawLabels(this._rangeStart, this._rangeEnd), this._drawDragHandle(c), this._drawDragHandle(e)) : this._drawLabels(this._windowStart, this._windowEnd));
          a.restore();
        };
        a.prototype._drawLabels = function(c, f) {
          var e = this._context, h = this._calculateTickInterval(c, f), n = Math.ceil(c / h) * h, q = 500 <= h, k = q ? 1E3 : 1, b = this._decimalPlaces(h / k), q = q ? "s" : "ms", g = this._toPixels(n), m = this._height / 2, l = this._controller.theme;
          e.lineWidth = 1;
          e.strokeStyle = l.contentTextDarkGrey(.5);
          e.fillStyle = l.contentTextDarkGrey(1);
          e.textAlign = "right";
          e.textBaseline = "middle";
          e.font = "11px sans-serif";
          for (l = this._width + a.TICK_MAX_WIDTH;g < l;) {
            e.fillText((n / k).toFixed(b) + " " + q, g - 7, m + 1), e.beginPath(), e.moveTo(g, 0), e.lineTo(g, this._height + 1), e.closePath(), e.stroke(), n += h, g = this._toPixels(n);
          }
        };
        a.prototype._calculateTickInterval = function(c, f) {
          var e = (f - c) / (this._width / a.TICK_MAX_WIDTH), h = Math.pow(10, Math.floor(Math.log(e) / Math.LN10)), e = e / h;
          return 5 < e ? 10 * h : 2 < e ? 5 * h : 1 < e ? 2 * h : h;
        };
        a.prototype._drawDragHandle = function(a) {
          var f = this._context;
          f.lineWidth = 2;
          f.strokeStyle = this._controller.theme.bodyBackground(1);
          f.fillStyle = this._controller.theme.foregroundTextGrey(.7);
          this._drawRoundedRect(f, a - c.FlameChartBase.DRAGHANDLE_WIDTH / 2, c.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2);
        };
        a.prototype._drawRoundedRect = function(a, c, e, h) {
          var f, q = !0;
          "undefined" === typeof q && (q = !0);
          "undefined" === typeof f && (f = !0);
          a.beginPath();
          a.moveTo(c + 2, 1);
          a.lineTo(c + e - 2, 1);
          a.quadraticCurveTo(c + e, 1, c + e, 3);
          a.lineTo(c + e, 1 + h - 2);
          a.quadraticCurveTo(c + e, 1 + h, c + e - 2, 1 + h);
          a.lineTo(c + 2, 1 + h);
          a.quadraticCurveTo(c, 1 + h, c, 1 + h - 2);
          a.lineTo(c, 3);
          a.quadraticCurveTo(c, 1, c + 2, 1);
          a.closePath();
          q && a.stroke();
          f && a.fill();
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
        a.prototype._getDragTargetUnderCursor = function(a, f) {
          if (0 <= f && f < this._height) {
            if (0 === this._type) {
              var e = this._toPixels(this._windowStart), h = this._toPixels(this._windowEnd), n = 2 + c.FlameChartBase.DRAGHANDLE_WIDTH / 2, e = a >= e - n && a <= e + n, h = a >= h - n && a <= h + n;
              if (e && h) {
                return 4;
              }
              if (e) {
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
        a.prototype.onMouseDown = function(a, f) {
          var e = this._getDragTargetUnderCursor(a, f);
          1 === e && this._mouseController.updateCursor(c.MouseCursor.GRABBING);
          this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:e};
        };
        a.prototype.onMouseMove = function(a, f) {
          var e = c.MouseCursor.DEFAULT, h = this._getDragTargetUnderCursor(a, f);
          0 !== h && (1 !== h ? e = c.MouseCursor.EW_RESIZE : 1 !== h || this._windowEqRange() || (e = c.MouseCursor.GRAB));
          this._mouseController.updateCursor(e);
        };
        a.prototype.onMouseOver = function(a, c) {
          this.onMouseMove(a, c);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(c.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, f, e, h, n, q) {
          a = this._dragInfo;
          if (4 === a.target) {
            if (0 !== n) {
              a.target = 0 > n ? 2 : 3;
            } else {
              return;
            }
          }
          f = this._windowStart;
          e = this._windowEnd;
          n = this._toTimeRelative(n);
          switch(a.target) {
            case 1:
              e = 0 === this._type ? 1 : -1;
              f = a.windowStartInitial + e * n;
              e = a.windowEndInitial + e * n;
              break;
            case 2:
              f = t(a.windowStartInitial + n, this._rangeStart, e - c.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3:
              e = t(a.windowEndInitial + n, f + c.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(f, e);
        };
        a.prototype.onDragEnd = function(a, c, e, h, f, q) {
          this._dragInfo = null;
          this.onMouseMove(e, h);
        };
        a.prototype.onClick = function(a, f) {
          1 === this._dragInfo.target && this._mouseController.updateCursor(c.MouseCursor.GRAB);
        };
        a.prototype.onHoverStart = function(a, c) {
        };
        a.prototype.onHoverEnd = function() {
        };
        a.TICK_MAX_WIDTH = 75;
        return a;
      }(c.FlameChartBase);
      c.FlameChartHeader = m;
    })(u.Profiler || (u.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      (function(c) {
        var f = function() {
          function a(a, c, e, h, f) {
            this.pageLoaded = a;
            this.threadsTotal = c;
            this.threadsLoaded = e;
            this.threadFilesTotal = h;
            this.threadFilesLoaded = f;
          }
          a.prototype.toString = function() {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function(a, c, e) {
              return a + ":" + this[a];
            }, this).join(", ") + "]";
          };
          return a;
        }();
        c.TraceLoggerProgressInfo = f;
        var r = function() {
          function a(a) {
            this._baseUrl = a;
            this._threads = [];
            this._progressInfo = null;
          }
          a.prototype.loadPage = function(a, c, e) {
            this._threads = [];
            this._pageLoadCallback = c;
            this._pageLoadProgressCallback = e;
            this._progressInfo = new f(!1, 0, 0, 0, 0);
            this._loadData([a], this._onLoadPage.bind(this));
          };
          Object.defineProperty(a.prototype, "buffers", {get:function() {
            for (var a = [], c = 0, e = this._threads.length;c < e;c++) {
              a.push(this._threads[c].buffer);
            }
            return a;
          }, enumerable:!0, configurable:!0});
          a.prototype._onProgress = function() {
            this._pageLoadProgressCallback && this._pageLoadProgressCallback.call(this, this._progressInfo);
          };
          a.prototype._onLoadPage = function(a) {
            if (a && 1 == a.length) {
              var f = this, e = 0;
              a = a[0];
              var h = a.length;
              this._threads = Array(h);
              this._progressInfo.pageLoaded = !0;
              this._progressInfo.threadsTotal = h;
              for (var n = 0;n < a.length;n++) {
                var q = a[n], k = [q.dict, q.tree];
                q.corrections && k.push(q.corrections);
                this._progressInfo.threadFilesTotal += k.length;
                this._loadData(k, function(b) {
                  return function(a) {
                    a && (a = new c.Thread(a), a.buffer.name = "Thread " + b, f._threads[b] = a);
                    e++;
                    f._progressInfo.threadsLoaded++;
                    f._onProgress();
                    e === h && f._pageLoadCallback.call(f, null, f._threads);
                  };
                }(n), function(a) {
                  f._progressInfo.threadFilesLoaded++;
                  f._onProgress();
                });
              }
              this._onProgress();
            } else {
              this._pageLoadCallback.call(this, "Error loading page.", null);
            }
          };
          a.prototype._loadData = function(a, c, e) {
            var h = 0, f = 0, q = a.length, k = [];
            k.length = q;
            for (var b = 0;b < q;b++) {
              var g = this._baseUrl + a[b], m = new XMLHttpRequest, l = /\.tl$/i.test(g) ? "arraybuffer" : "json";
              m.open("GET", g, !0);
              m.responseType = l;
              m.onload = function(a, b) {
                return function(d) {
                  if ("json" === b) {
                    if (d = this.response, "string" === typeof d) {
                      try {
                        d = JSON.parse(d), k[a] = d;
                      } catch (g) {
                        f++;
                      }
                    } else {
                      k[a] = d;
                    }
                  } else {
                    k[a] = this.response;
                  }
                  ++h;
                  e && e(h);
                  h === q && c(k);
                };
              }(b, l);
              m.send();
            }
          };
          a.colors = "#0044ff #8c4b00 #cc5c33 #ff80c4 #ffbfd9 #ff8800 #8c5e00 #adcc33 #b380ff #bfd9ff #ffaa00 #8c0038 #bf8f30 #f780ff #cc99c9 #aaff00 #000073 #452699 #cc8166 #cca799 #000066 #992626 #cc6666 #ccc299 #ff6600 #526600 #992663 #cc6681 #99ccc2 #ff0066 #520066 #269973 #61994d #739699 #ffcc00 #006629 #269199 #94994d #738299 #ff0000 #590000 #234d8c #8c6246 #7d7399 #ee00ff #00474d #8c2385 #8c7546 #7c8c69 #eeff00 #4d003d #662e1a #62468c #8c6969 #6600ff #4c2900 #1a6657 #8c464f #8c6981 #44ff00 #401100 #1a2466 #663355 #567365 #d90074 #403300 #101d40 #59562d #66614d #cc0000 #002b40 #234010 #4c2626 #4d5e66 #00a3cc #400011 #231040 #4c3626 #464359 #0000bf #331b00 #80e6ff #311a33 #4d3939 #a69b00 #003329 #80ffb2 #331a20 #40303d #00a658 #40ffd9 #ffc480 #ffe1bf #332b26 #8c2500 #9933cc #80fff6 #ffbfbf #303326 #005e8c #33cc47 #b2ff80 #c8bfff #263332 #00708c #cc33ad #ffe680 #f2ffbf #262a33 #388c00 #335ccc #8091ff #bfffd9".split(" ");
          return a;
        }();
        c.TraceLogger = r;
      })(c.TraceLogger || (c.TraceLogger = {}));
    })(f.Profiler || (f.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      (function(f) {
        var m;
        (function(c) {
          c[c.START_HI = 0] = "START_HI";
          c[c.START_LO = 4] = "START_LO";
          c[c.STOP_HI = 8] = "STOP_HI";
          c[c.STOP_LO = 12] = "STOP_LO";
          c[c.TEXTID = 16] = "TEXTID";
          c[c.NEXTID = 20] = "NEXTID";
        })(m || (m = {}));
        m = function() {
          function f(a) {
            2 <= a.length && (this._text = a[0], this._data = new DataView(a[1]), this._buffer = new c.TimelineBuffer, this._walkTree(0));
          }
          Object.defineProperty(f.prototype, "buffer", {get:function() {
            return this._buffer;
          }, enumerable:!0, configurable:!0});
          f.prototype._walkTree = function(a) {
            var c = this._data, p = this._buffer;
            do {
              var e = a * f.ITEM_SIZE, h = 4294967295 * c.getUint32(e + 0) + c.getUint32(e + 4), n = 4294967295 * c.getUint32(e + 8) + c.getUint32(e + 12), q = c.getUint32(e + 16), e = c.getUint32(e + 20), k = 1 === (q & 1), q = q >>> 1, q = this._text[q];
              p.enter(q, null, h / 1E6);
              k && this._walkTree(a + 1);
              p.leave(q, null, n / 1E6);
              a = e;
            } while (0 !== a);
          };
          f.ITEM_SIZE = 24;
          return f;
        }();
        f.Thread = m;
      })(c.TraceLogger || (c.TraceLogger = {}));
    })(f.Profiler || (f.Profiler = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.NumberUtilities.clamp, m = function() {
        function a() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        a.prototype.append = function(a, c) {
          var e = this.lines;
          0 < e.length && e[e.length - 1] === a ? this.repeat[e.length - 1]++ : (this.lines.push(a), this.repeat.push(1), this.format.push(c ? {backgroundFillStyle:c} : void 0), this.time.push(performance.now()), this.length++);
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
      c.Buffer = m;
      var r = function() {
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
          this.buffer = new m;
          a.addEventListener("keydown", function(a) {
            var d = 0;
            switch(a.keyCode) {
              case s:
                this.showLineNumbers = !this.showLineNumbers;
                break;
              case A:
                this.showLineTime = !this.showLineTime;
                break;
              case q:
                d = -1;
                break;
              case k:
                d = 1;
                break;
              case c:
                d = -this.pageLineCount;
                break;
              case e:
                d = this.pageLineCount;
                break;
              case h:
                d = -this.lineIndex;
                break;
              case f:
                d = this.buffer.length - this.lineIndex;
                break;
              case b:
                this.columnIndex -= a.metaKey ? 10 : 1;
                0 > this.columnIndex && (this.columnIndex = 0);
                a.preventDefault();
                break;
              case g:
                this.columnIndex += a.metaKey ? 10 : 1;
                a.preventDefault();
                break;
              case r:
                a.metaKey && (this.selection = {start:0, end:this.buffer.length}, a.preventDefault());
                break;
              case l:
                if (a.metaKey) {
                  var m = "";
                  if (this.selection) {
                    for (var t = this.selection.start;t <= this.selection.end;t++) {
                      m += this.buffer.get(t) + "\n";
                    }
                  } else {
                    m = this.buffer.get(this.lineIndex);
                  }
                  alert(m);
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
          var c = 33, e = 34, h = 36, f = 35, q = 38, k = 40, b = 37, g = 39, r = 65, l = 67, s = 78, A = 84;
        }
        a.prototype.resize = function() {
          this._resizeHandler();
        };
        a.prototype._resizeHandler = function() {
          var a = this.canvas.parentElement, c = a.clientWidth, a = a.clientHeight - 1, e = window.devicePixelRatio || 1;
          1 !== e ? (this.ratio = e / 1, this.canvas.width = c * this.ratio, this.canvas.height = a * this.ratio, this.canvas.style.width = c + "px", this.canvas.style.height = a + "px") : (this.ratio = 1, this.canvas.width = c, this.canvas.height = a);
          this.pageLineCount = Math.floor(this.canvas.height / this.lineHeight);
        };
        a.prototype.gotoLine = function(a) {
          this.lineIndex = t(a, 0, this.buffer.length - 1);
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
          var c = this.textMarginLeft, e = c + (this.showLineNumbers ? 5 * (String(this.buffer.length).length + 2) : 0), h = e + (this.showLineTime ? 40 : 10), f = h + 25;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
          for (var q = this.canvas.width, k = this.lineHeight, b = 0;b < a;b++) {
            var g = b * this.lineHeight, m = this.pageIndex + b, l = this.buffer.get(m), s = this.buffer.getFormat(m), A = this.buffer.getRepeat(m), r = 1 < m ? this.buffer.getTime(m) - this.buffer.getTime(0) : 0;
            this.context.fillStyle = m % 2 ? this.lineColor : this.alternateLineColor;
            s && s.backgroundFillStyle && (this.context.fillStyle = s.backgroundFillStyle);
            this.context.fillRect(0, g, q, k);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;
            this.selection && m >= this.selection.start && m <= this.selection.end && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, g, q, k), this.context.fillStyle = this.selectionTextColor);
            this.hasFocus && m === this.lineIndex && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, g, q, k), this.context.fillStyle = this.selectionTextColor);
            0 < this.columnIndex && (l = l.substring(this.columnIndex));
            g = (b + 1) * this.lineHeight - this.textMarginBottom;
            this.showLineNumbers && this.context.fillText(String(m), c, g);
            this.showLineTime && this.context.fillText(r.toFixed(1).padLeft(" ", 6), e, g);
            1 < A && this.context.fillText(String(A).padLeft(" ", 3), h, g);
            this.context.fillText(l, f, g);
          }
        };
        a.prototype.refreshEvery = function(a) {
          function c() {
            e.paint();
            e.refreshFrequency && setTimeout(c, e.refreshFrequency);
          }
          var e = this;
          this.refreshFrequency = a;
          e.refreshFrequency && setTimeout(c, e.refreshFrequency);
        };
        a.prototype.isScrolledToBottom = function() {
          return this.lineIndex === this.buffer.length - 1;
        };
        return a;
      }();
      c.Terminal = r;
    })(u.Terminal || (u.Terminal = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      var f = function() {
        function c(f) {
          this._lastWeightedTime = this._lastTime = this._index = 0;
          this._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
          this._canvas = f;
          this._context = f.getContext("2d");
          window.addEventListener("resize", this._resizeHandler.bind(this), !1);
          this._resizeHandler();
        }
        c.prototype._resizeHandler = function() {
          var c = this._canvas.parentElement, a = c.clientWidth, c = c.clientHeight - 1, d = window.devicePixelRatio || 1;
          1 !== d ? (this._ratio = d / 1, this._canvas.width = a * this._ratio, this._canvas.height = c * this._ratio, this._canvas.style.width = a + "px", this._canvas.style.height = c + "px") : (this._ratio = 1, this._canvas.width = a, this._canvas.height = c);
        };
        c.prototype.tickAndRender = function(c) {
          "undefined" === typeof c && (c = !1);
          if (0 === this._lastTime) {
            this._lastTime = performance.now();
          } else {
            var a = (performance.now() - this._lastTime) * (1 - .9) + .9 * this._lastWeightedTime, d = this._context, f = 2 * this._ratio, e = (this._canvas.width - 20) / (f + 1) | 0, h = this._index++;
            this._index > e && (this._index = 0);
            d.globalAlpha = 1;
            d.fillStyle = "black";
            d.fillRect(20 + h * (f + 1), 0, 4 * f, this._canvas.height);
            e = 1E3 / 60 / a;
            d.fillStyle = this._gradient[e * (this._gradient.length - 1) | 0];
            d.globalAlpha = c ? .5 : 1;
            d.fillRect(20 + h * (f + 1), 0, f, this._canvas.height * e | 0);
            0 === h % 16 && (d.globalAlpha = 1, d.fillStyle = "black", d.fillRect(0, 0, 20, this._canvas.height), d.fillStyle = "white", d.font = "10px Arial", d.fillText((1E3 / a).toFixed(0), 2, 8));
            this._lastTime = performance.now();
            this._lastWeightedTime = a;
          }
        };
        return c;
      }();
      c.FPS = f;
    })(f.Mini || (f.Mini = {}));
  })(f.Tools || (f.Tools = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load Parser Dependencies");
console.time("Load SWF Parser");
(function(f) {
  (function(u) {
    u.timelineBuffer = new f.Tools.Profiler.TimelineBuffer("Parser");
    u.enterTimeline = function(c, f) {
    };
    u.leaveTimeline = function(c) {
    };
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      function t(a, c, d) {
        return m(a, c, d) << 32 - d >> 32 - d;
      }
      function m(c, d, f) {
        for (var q = d.bitBuffer, k = d.bitLength;f > k;) {
          q = q << 8 | c[d.pos++], k += 8;
        }
        k -= f;
        c = q >>> k & a[f];
        d.bitBuffer = q;
        d.bitLength = k;
        return c;
      }
      var r = Math.pow;
      c.readSi8 = function(a, c) {
        return c.getInt8(c.pos++);
      };
      c.readSi16 = function(a, c) {
        return c.getInt16(c.pos, c.pos += 2);
      };
      c.readSi32 = function(a, c) {
        return c.getInt32(c.pos, c.pos += 4);
      };
      c.readUi8 = function(a, c) {
        return a[c.pos++];
      };
      c.readUi16 = function(a, c) {
        return c.getUint16(c.pos, c.pos += 2);
      };
      c.readUi32 = function(a, c) {
        return c.getUint32(c.pos, c.pos += 4);
      };
      c.readFixed = function(a, c) {
        return c.getInt32(c.pos, c.pos += 4) / 65536;
      };
      c.readFixed8 = function(a, c) {
        return c.getInt16(c.pos, c.pos += 2) / 256;
      };
      c.readFloat16 = function(a, c) {
        var d = c.getUint16(c.pos);
        c.pos += 2;
        var f = d >> 15 ? -1 : 1, k = (d & 31744) >> 10, d = d & 1023;
        return k ? 31 === k ? d ? NaN : Infinity * f : f * r(2, k - 15) * (1 + d / 1024) : f * r(2, -14) * (d / 1024);
      };
      c.readFloat = function(a, c) {
        return c.getFloat32(c.pos, c.pos += 4);
      };
      c.readDouble = function(a, c) {
        return c.getFloat64(c.pos, c.pos += 8);
      };
      c.readEncodedU32 = function(a, c) {
        var d = a[c.pos++];
        if (!(d & 128)) {
          return d;
        }
        d = d & 127 | a[c.pos++] << 7;
        if (!(d & 16384)) {
          return d;
        }
        d = d & 16383 | a[c.pos++] << 14;
        if (!(d & 2097152)) {
          return d;
        }
        d = d & 2097151 | a[c.pos++] << 21;
        return d & 268435456 ? d & 268435455 | a[c.pos++] << 28 : d;
      };
      c.readBool = function(a, c) {
        return!!a[c.pos++];
      };
      c.align = function(a, c) {
        c.align();
      };
      c.readSb = t;
      for (var a = new Uint32Array(33), d = 1, p = 0;32 >= d;++d) {
        a[d] = p = p << 1 | 1;
      }
      c.readUb = m;
      c.readFb = function(a, c, d) {
        return t(a, c, d) / 65536;
      };
      c.readString = function(a, c, d) {
        var q = c.pos;
        if (d) {
          a = a.subarray(q, q += d);
        } else {
          d = 0;
          for (var k = q;a[k];k++) {
            d++;
          }
          a = a.subarray(q, q += d);
          q++;
        }
        c.pos = q;
        c = f.StringUtilities.utf8encode(a);
        0 <= c.indexOf("\x00") && (c = c.split("\x00").join(""));
        return c;
      };
      c.readBinary = function(a, c, d, f) {
        d || (d = c.end - c.pos);
        a = a.subarray(c.pos, c.pos += d);
        if (f) {
          return a;
        }
        d = new Uint8Array(d);
        d.set(a);
        return d;
      };
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      function f(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        var s = d.lineBounds = {};
        g(a, b, s, e, h);
        if (s = d.isMorph = 46 === h || 84 === h) {
          var k = d.lineBoundsMorph = {};
          g(a, b, k, e, h);
        }
        if (k = d.canHaveStrokes = 83 === h || 84 === h) {
          var n = d.fillBounds = {};
          g(a, b, n, e, h);
          s && (n = d.fillBoundsMorph = {}, g(a, b, n, e, h));
          d.flags = c.readUi8(a, b);
        }
        if (s) {
          d.offsetMorph = c.readUi32(a, b);
          var l = d, q, p, n = y(a, b, l, e, h, s, k), m = n.lineBits, A = n.fillBits, r = l.records = [];
          do {
            var V = {}, n = B(a, b, V, e, h, s, A, m, k, p);
            q = n.eos;
            A = n.fillBits;
            m = n.lineBits;
            p = n.bits;
            r.push(V);
          } while (!q);
          n = w(a, b, l, e, h);
          A = n.fillBits;
          m = n.lineBits;
          l = l.recordsMorph = [];
          do {
            r = {}, n = B(a, b, r, e, h, s, A, m, k, p), q = n.eos, A = n.fillBits, m = n.lineBits, p = n.bits, l.push(r);
          } while (!q);
        } else {
          A = d;
          p = y(a, b, A, e, h, s, k);
          n = p.fillBits;
          m = p.lineBits;
          l = A.records = [];
          do {
            r = {}, p = B(a, b, r, e, h, s, n, m, k, q), A = p.eos, n = p.fillBits, m = p.lineBits, q = p.bits, l.push(r);
          } while (!A);
        }
        return d;
      }
      function m(a, b, d, e, h) {
        var g;
        d || (d = {});
        if (4 < h) {
          g = d.flags = 26 < h ? c.readUi16(a, b) : c.readUi8(a, b);
          d.depth = c.readUi16(a, b);
          g & 2048 && (d.className = c.readString(a, b, 0));
          g & 2 && (d.symbolId = c.readUi16(a, b));
          if (g & 4) {
            var f = d.matrix = {};
            s(a, b, f, e, h);
          }
          g & 8 && (f = d.cxform = {}, A(a, b, f, e, h));
          g & 16 && (d.ratio = c.readUi16(a, b));
          g & 32 && (d.name = c.readString(a, b, 0));
          g & 64 && (d.clipDepth = c.readUi16(a, b));
          if (g & 256) {
            for (var n = c.readUi8(a, b), f = d.filters = [];n--;) {
              var k = {};
              D(a, b, k, e, h);
              f.push(k);
            }
          }
          g & 512 && (d.blendMode = c.readUi8(a, b));
          g & 1024 && (d.bmpCache = c.readUi8(a, b));
          if (g & 128) {
            c.readUi16(a, b);
            6 <= e ? c.readUi32(a, b) : c.readUi16(a, b);
            h = d.events = [];
            do {
              var f = {}, n = a, k = b, l = f, q = e, p = 6 <= q ? c.readUi32(n, k) : c.readUi16(n, k), m = l.eoe = !p, w = 0;
              l.onKeyUp = p >> 7 & 1;
              l.onKeyDown = p >> 6 & 1;
              l.onMouseUp = p >> 5 & 1;
              l.onMouseDown = p >> 4 & 1;
              l.onMouseMove = p >> 3 & 1;
              l.onUnload = p >> 2 & 1;
              l.onEnterFrame = p >> 1 & 1;
              l.onLoad = p & 1;
              6 <= q && (l.onDragOver = p >> 15 & 1, l.onRollOut = p >> 14 & 1, l.onRollOver = p >> 13 & 1, l.onReleaseOutside = p >> 12 & 1, l.onRelease = p >> 11 & 1, l.onPress = p >> 10 & 1, l.onInitialize = p >> 9 & 1, l.onData = p >> 8 & 1, l.onConstruct = 7 <= q ? p >> 18 & 1 : 0, w = l.keyPress = p >> 17 & 1, l.onDragOut = p >> 16 & 1);
              m || (q = l.length = c.readUi32(n, k), w && (l.keyCode = c.readUi8(n, k)), l.actionsData = c.readBinary(n, k, q - +w, !1));
              n = m;
              h.push(f);
            } while (!n);
          }
          g & 1024 && (e = d, h = c.readUi8(a, b) | c.readUi8(a, b) << 24 | c.readUi8(a, b) << 16 | c.readUi8(a, b) << 8, e.backgroundColor = h);
          g & 512 && (d.visibility = c.readUi8(a, b));
        } else {
          d.symbolId = c.readUi16(a, b), d.depth = c.readUi16(a, b), d.flags |= 4, g = d.matrix = {}, s(a, b, g, e, h), b.remaining() && (d.flags |= 8, g = d.cxform = {}, A(a, b, g, e, h));
        }
        return d;
      }
      function r(a, b, d, e, h) {
        d || (d = {});
        5 === h && (d.symbolId = c.readUi16(a, b));
        d.depth = c.readUi16(a, b);
        return d;
      }
      function a(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        21 < h ? (e = c.readUi32(a, b), 90 === h && (d.deblock = c.readFixed8(a, b)), e = d.imgData = c.readBinary(a, b, e, !0), d.alphaData = c.readBinary(a, b, 0, !0)) : e = d.imgData = c.readBinary(a, b, 0, !0);
        switch(e[0] << 8 | e[1]) {
          case 65496:
          ;
          case 65497:
            d.mimeType = "image/jpeg";
            break;
          case 35152:
            d.mimeType = "image/png";
            break;
          case 18249:
            d.mimeType = "image/gif";
            break;
          default:
            d.mimeType = "application/octet-stream";
        }
        6 === h && (d.incomplete = 1);
        return d;
      }
      function d(a, b, d, e, h) {
        var g;
        d || (d = {});
        d.id = c.readUi16(a, b);
        if (7 == h) {
          var f = d.characters = [];
          do {
            var n = {};
            g = a;
            var l = b, k = n, q = e, p = h, m = c.readUi8(g, l), w = k.eob = !m;
            k.flags = 8 <= q ? (m >> 5 & 1 ? 512 : 0) | (m >> 4 & 1 ? 256 : 0) : 0;
            k.stateHitTest = m >> 3 & 1;
            k.stateDown = m >> 2 & 1;
            k.stateOver = m >> 1 & 1;
            k.stateUp = m & 1;
            w || (k.symbolId = c.readUi16(g, l), k.depth = c.readUi16(g, l), m = k.matrix = {}, s(g, l, m, q, p), 34 === p && (m = k.cxform = {}, A(g, l, m, q, p)), k.flags & 256 && (k.filterCount = c.readUi8(g, l), m = k.filters = {}, D(g, l, m, q, p)), k.flags & 512 && (k.blendMode = c.readUi8(g, l)));
            g = w;
            f.push(n);
          } while (!g);
          d.actionsData = c.readBinary(a, b, 0, !1);
        } else {
          g = c.readUi8(a, b);
          d.trackAsMenu = g >> 7 & 1;
          f = c.readUi16(a, b);
          n = d.characters = [];
          do {
            l = {};
            k = c.readUi8(a, b);
            g = l.eob = !k;
            l.flags = 8 <= e ? (k >> 5 & 1 ? 512 : 0) | (k >> 4 & 1 ? 256 : 0) : 0;
            l.stateHitTest = k >> 3 & 1;
            l.stateDown = k >> 2 & 1;
            l.stateOver = k >> 1 & 1;
            l.stateUp = k & 1;
            if (!g) {
              l.symbolId = c.readUi16(a, b);
              l.depth = c.readUi16(a, b);
              k = l.matrix = {};
              s(a, b, k, e, h);
              34 === h && (k = l.cxform = {}, A(a, b, k, e, h));
              if (l.flags & 256) {
                for (q = c.readUi8(a, b), k = d.filters = [];q--;) {
                  p = {}, D(a, b, p, e, h), k.push(p);
                }
              }
              l.flags & 512 && (l.blendMode = c.readUi8(a, b));
            }
            n.push(l);
          } while (!g);
          if (f) {
            e = d.buttonActions = [];
            do {
              h = {}, g = a, f = b, n = h, l = c.readUi16(g, f), k = c.readUi16(g, f), n.keyCode = (k & 65024) >> 9, n.stateTransitionFlags = k & 511, n.actionsData = c.readBinary(g, f, (l || 4) - 4, !1), e.push(h);
            } while (0 < b.remaining());
          }
        }
        return d;
      }
      function p(a, b, d, e, h) {
        var f;
        d || (d = {});
        d.id = c.readUi16(a, b);
        var k = d.bbox = {};
        g(a, b, k, e, h);
        k = d.matrix = {};
        s(a, b, k, e, h);
        e = d.glyphBits = c.readUi8(a, b);
        var k = d.advanceBits = c.readUi8(a, b), n = d.records = [];
        do {
          var q = {};
          f = a;
          var p = b, m = q, A = h, w = e, r = k, B = void 0;
          c.align(f, p);
          var y = c.readUb(f, p, 8), t = m.eot = !y, B = f, D = p, u = m, G = y, y = u.hasFont = G >> 3 & 1, C = u.hasColor = G >> 2 & 1, K = u.hasMoveY = G >> 1 & 1, G = u.hasMoveX = G & 1;
          y && (u.fontId = c.readUi16(B, D));
          C && (u.color = 33 === A ? l(B, D) : V(B, D));
          G && (u.moveX = c.readSi16(B, D));
          K && (u.moveY = c.readSi16(B, D));
          y && (u.fontHeight = c.readUi16(B, D));
          if (!t) {
            for (B = c.readUi8(f, p), B = m.glyphCount = B, m = m.entries = [];B--;) {
              D = {}, u = f, A = p, y = D, C = r, y.glyphIndex = c.readUb(u, A, w), y.advance = c.readSb(u, A, C), m.push(D);
            }
          }
          f = t;
          n.push(q);
        } while (!f);
        return d;
      }
      function e(a, b, d, e, h) {
        d || (d = {});
        59 === h && (d.spriteId = c.readUi16(a, b));
        d.actionsData = c.readBinary(a, b, 0, !1);
        return d;
      }
      function h(a, b, d, e, h) {
        d || (d = {});
        15 == h && (d.soundId = c.readUi16(a, b));
        89 == h && (d.soundClassName = c.readString(a, b, 0));
        e = d.soundInfo = {};
        c.readUb(a, b, 2);
        e.stop = c.readUb(a, b, 1);
        e.noMultiple = c.readUb(a, b, 1);
        h = e.hasEnvelope = c.readUb(a, b, 1);
        var g = e.hasLoops = c.readUb(a, b, 1), f = e.hasOutPoint = c.readUb(a, b, 1);
        if (e.hasInPoint = c.readUb(a, b, 1)) {
          e.inPoint = c.readUi32(a, b);
        }
        f && (e.outPoint = c.readUi32(a, b));
        g && (e.loopCount = c.readUi16(a, b));
        if (h) {
          for (h = e.envelopeCount = c.readUi8(a, b), e = e.envelopes = [];h--;) {
            var g = {}, f = a, k = b, s = g;
            s.pos44 = c.readUi32(f, k);
            s.volumeLeft = c.readUi16(f, k);
            s.volumeRight = c.readUi16(f, k);
            e.push(g);
          }
        }
        return d;
      }
      function n(a, b, d, e, h) {
        d || (d = {});
        e = c.readUi8(a, b);
        d.playbackRate = e >> 2 & 3;
        d.playbackSize = e >> 1 & 1;
        d.playbackType = e & 1;
        e = c.readUi8(a, b);
        h = d.streamCompression = e >> 4 & 15;
        d.streamRate = e >> 2 & 3;
        d.streamSize = e >> 1 & 1;
        d.streamType = e & 1;
        d.samplesCount = c.readUi32(a, b);
        2 == h && (d.latencySeek = c.readSi16(a, b));
        return d;
      }
      function q(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        e = d.format = c.readUi8(a, b);
        d.width = c.readUi16(a, b);
        d.height = c.readUi16(a, b);
        d.hasAlpha = 36 === h;
        3 === e && (d.colorTableSize = c.readUi8(a, b));
        d.bmpData = c.readBinary(a, b, 0, !1);
        return d;
      }
      function k(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        var f = d.hasLayout = c.readUb(a, b, 1);
        5 < e ? d.shiftJis = c.readUb(a, b, 1) : c.readUb(a, b, 1);
        d.smallText = c.readUb(a, b, 1);
        d.ansi = c.readUb(a, b, 1);
        var k = d.wideOffset = c.readUb(a, b, 1), s = d.wide = c.readUb(a, b, 1);
        d.italic = c.readUb(a, b, 1);
        d.bold = c.readUb(a, b, 1);
        5 < e ? d.language = c.readUi8(a, b) : (c.readUi8(a, b), d.language = 0);
        var l = c.readUi8(a, b);
        d.name = c.readString(a, b, l);
        75 === h && (d.resolution = 20);
        var l = d.glyphCount = c.readUi16(a, b), n = b.pos;
        if (k) {
          for (var k = d.offsets = [], q = l;q--;) {
            k.push(c.readUi32(a, b));
          }
          d.mapOffset = c.readUi32(a, b);
        } else {
          k = d.offsets = [];
          for (q = l;q--;) {
            k.push(c.readUi16(a, b));
          }
          d.mapOffset = c.readUi16(a, b);
        }
        k = d.glyphs = [];
        for (q = l;q--;) {
          var p = {};
          1 === d.offsets[l - q] + n - b.pos ? (c.readUi8(a, b), k.push({records:[{type:0, eos:!0, hasNewStyles:0, hasLineStyle:0, hasFillStyle1:0, hasFillStyle0:0, move:0}]})) : (C(a, b, p, e, h), k.push(p));
        }
        if (s) {
          for (n = d.codes = [], k = l;k--;) {
            n.push(c.readUi16(a, b));
          }
        } else {
          for (n = d.codes = [], k = l;k--;) {
            n.push(c.readUi8(a, b));
          }
        }
        if (f) {
          d.ascent = c.readUi16(a, b);
          d.descent = c.readUi16(a, b);
          d.leading = c.readSi16(a, b);
          f = d.advance = [];
          for (n = l;n--;) {
            f.push(c.readSi16(a, b));
          }
          for (f = d.bbox = [];l--;) {
            n = {}, g(a, b, n, e, h), f.push(n);
          }
          h = c.readUi16(a, b);
          for (e = d.kerning = [];h--;) {
            l = {}, f = a, n = b, k = l, s ? (k.code1 = c.readUi16(f, n), k.code2 = c.readUi16(f, n)) : (k.code1 = c.readUi8(f, n), k.code2 = c.readUi8(f, n)), k.adjustment = c.readUi16(f, n), e.push(l);
          }
        }
        return d;
      }
      function b(a, b, d, e, h) {
        d || (d = {});
        d.flags = 82 === h ? c.readUi32(a, b) : 0;
        d.name = 82 === h ? c.readString(a, b, 0) : "";
        d.data = c.readBinary(a, b, 0, !1);
        return d;
      }
      function g(a, b, d, e, h) {
        c.align(a, b);
        var g = c.readUb(a, b, 5);
        e = c.readSb(a, b, g);
        h = c.readSb(a, b, g);
        var f = c.readSb(a, b, g), g = c.readSb(a, b, g);
        d.xMin = e;
        d.xMax = h;
        d.yMin = f;
        d.yMax = g;
        c.align(a, b);
      }
      function V(a, b) {
        return(c.readUi8(a, b) << 24 | c.readUi8(a, b) << 16 | c.readUi8(a, b) << 8 | 255) >>> 0;
      }
      function l(a, b) {
        return c.readUi8(a, b) << 24 | c.readUi8(a, b) << 16 | c.readUi8(a, b) << 8 | c.readUi8(a, b);
      }
      function s(a, b, d, e, h) {
        c.align(a, b);
        c.readUb(a, b, 1) ? (h = c.readUb(a, b, 5), d.a = c.readFb(a, b, h), d.d = c.readFb(a, b, h)) : (d.a = 1, d.d = 1);
        c.readUb(a, b, 1) ? (h = c.readUb(a, b, 5), d.b = c.readFb(a, b, h), d.c = c.readFb(a, b, h)) : (d.b = 0, d.c = 0);
        h = c.readUb(a, b, 5);
        e = c.readSb(a, b, h);
        h = c.readSb(a, b, h);
        d.tx = e;
        d.ty = h;
        c.align(a, b);
      }
      function A(a, b, d, e, h) {
        c.align(a, b);
        e = c.readUb(a, b, 1);
        var g = c.readUb(a, b, 1), f = c.readUb(a, b, 4);
        g ? (d.redMultiplier = c.readSb(a, b, f), d.greenMultiplier = c.readSb(a, b, f), d.blueMultiplier = c.readSb(a, b, f), d.alphaMultiplier = 4 < h ? c.readSb(a, b, f) : 256) : (d.redMultiplier = 256, d.greenMultiplier = 256, d.blueMultiplier = 256, d.alphaMultiplier = 256);
        e ? (d.redOffset = c.readSb(a, b, f), d.greenOffset = c.readSb(a, b, f), d.blueOffset = c.readSb(a, b, f), d.alphaOffset = 4 < h ? c.readSb(a, b, f) : 0) : (d.redOffset = 0, d.greenOffset = 0, d.blueOffset = 0, d.alphaOffset = 0);
        c.align(a, b);
      }
      function B(a, b, d, e, h, g, f, k, l, s) {
        var n, q = d.type = c.readUb(a, b, 1), p = c.readUb(a, b, 5);
        n = d.eos = !(q || p);
        if (q) {
          e = s, h = h = h = 0, h = d.isStraight = p >> 4, e = (p & 15) + 2, h ? (h = d.isGeneral = c.readUb(a, b, 1)) ? (d.deltaX = c.readSb(a, b, e), d.deltaY = c.readSb(a, b, e)) : (h = d.isVertical = c.readUb(a, b, 1)) ? d.deltaY = c.readSb(a, b, e) : d.deltaX = c.readSb(a, b, e) : (d.controlDeltaX = c.readSb(a, b, e), d.controlDeltaY = c.readSb(a, b, e), d.anchorDeltaX = c.readSb(a, b, e), d.anchorDeltaY = c.readSb(a, b, e)), a = e;
        } else {
          var m = 0, A = 0, w = 0, r = 0, B = 0, m = 2 < h ? d.hasNewStyles = p >> 4 : d.hasNewStyles = 0, A = d.hasLineStyle = p >> 3 & 1, w = d.hasFillStyle1 = p >> 2 & 1, r = d.hasFillStyle0 = p >> 1 & 1;
          if (B = d.move = p & 1) {
            s = c.readUb(a, b, 5), d.moveX = c.readSb(a, b, s), d.moveY = c.readSb(a, b, s);
          }
          r && (d.fillStyle0 = c.readUb(a, b, f));
          w && (d.fillStyle1 = c.readUb(a, b, f));
          A && (d.lineStyle = c.readUb(a, b, k));
          m && (a = y(a, b, d, e, h, g, l), k = a.lineBits, f = a.fillBits);
          a = s;
        }
        return{type:q, flags:p, eos:n, fillBits:f, lineBits:k, bits:a};
      }
      function y(a, b, d, e, h, g, f) {
        var k, s = c.readUi8(a, b);
        k = 2 < h && 255 === s ? c.readUi16(a, b) : s;
        for (s = d.fillStyles = [];k--;) {
          var n = {};
          u(a, b, n, e, h, g);
          s.push(n);
        }
        s = c.readUi8(a, b);
        k = 2 < h && 255 === s ? c.readUi16(a, b) : s;
        for (s = d.lineStyles = [];k--;) {
          var n = {}, q = a, p = b, m = n, A = e, r = h, B = g, y = f;
          m.width = c.readUi16(q, p);
          B && (m.widthMorph = c.readUi16(q, p));
          if (y) {
            c.align(q, p);
            m.startCapsStyle = c.readUb(q, p, 2);
            var y = m.jointStyle = c.readUb(q, p, 2), t = m.hasFill = c.readUb(q, p, 1);
            m.noHscale = c.readUb(q, p, 1);
            m.noVscale = c.readUb(q, p, 1);
            m.pixelHinting = c.readUb(q, p, 1);
            c.readUb(q, p, 5);
            m.noClose = c.readUb(q, p, 1);
            m.endCapsStyle = c.readUb(q, p, 2);
            2 === y && (m.miterLimitFactor = c.readFixed8(q, p));
            t ? (m = m.fillStyle = {}, u(q, p, m, A, r, B)) : (m.color = l(q, p), B && (m.colorMorph = l(q, p)));
          } else {
            m.color = 22 < r ? l(q, p) : V(q, p), B && (m.colorMorph = l(q, p));
          }
          s.push(n);
        }
        a = w(a, b, d, e, h);
        return{fillBits:a.fillBits, lineBits:a.lineBits};
      }
      function w(a, b, d, e, h) {
        c.align(a, b);
        d = c.readUb(a, b, 4);
        a = c.readUb(a, b, 4);
        return{fillBits:d, lineBits:a};
      }
      function u(a, b, d, e, h, g) {
        var f = d.type = c.readUi8(a, b);
        switch(f) {
          case 0:
            d.color = 22 < h || g ? l(a, b) : V(a, b);
            g && (d.colorMorph = l(a, b));
            break;
          case 16:
          ;
          case 18:
          ;
          case 19:
            var k = d.matrix = {};
            s(a, b, k, e, h);
            g && (k = d.matrixMorph = {}, s(a, b, k, e, h));
            83 === h ? (d.spreadMode = c.readUb(a, b, 2), d.interpolationMode = c.readUb(a, b, 2)) : c.readUb(a, b, 4);
            k = d.count = c.readUb(a, b, 4);
            for (e = d.records = [];k--;) {
              var n = {}, q = a, p = b, m = n, A = h, w = g;
              m.ratio = c.readUi8(q, p);
              m.color = 22 < A ? l(q, p) : V(q, p);
              w && (m.ratioMorph = c.readUi8(q, p), m.colorMorph = l(q, p));
              e.push(n);
            }
            19 === f && (d.focalPoint = c.readSi16(a, b), g && (d.focalPointMorph = c.readSi16(a, b)));
            break;
          case 64:
          ;
          case 65:
          ;
          case 66:
          ;
          case 67:
            d.bitmapId = c.readUi16(a, b), k = d.matrix = {}, s(a, b, k, e, h), g && (g = d.matrixMorph = {}, s(a, b, g, e, h)), d.condition = 64 === f || 67 === f;
        }
      }
      function D(a, b, d, e, h) {
        e = d.type = c.readUi8(a, b);
        switch(e) {
          case 0:
          ;
          case 2:
          ;
          case 3:
          ;
          case 4:
          ;
          case 7:
            h = 4 === e || 7 === e ? c.readUi8(a, b) : 1;
            for (var g = d.colors = [], f = h;f--;) {
              g.push(l(a, b));
            }
            3 === e && (d.hightlightColor = l(a, b));
            if (4 === e || 7 === e) {
              for (g = d.ratios = [];h--;) {
                g.push(c.readUi8(a, b));
              }
            }
            d.blurX = c.readFixed(a, b);
            d.blurY = c.readFixed(a, b);
            2 !== e && (d.angle = c.readFixed(a, b), d.distance = c.readFixed(a, b));
            d.strength = c.readFixed8(a, b);
            d.inner = c.readUb(a, b, 1);
            d.knockout = c.readUb(a, b, 1);
            d.compositeSource = c.readUb(a, b, 1);
            3 === e || 4 === e || 7 === e ? (d.onTop = c.readUb(a, b, 1), d.quality = c.readUb(a, b, 4)) : d.quality = c.readUb(a, b, 5);
            break;
          case 1:
            d.blurX = c.readFixed(a, b);
            d.blurY = c.readFixed(a, b);
            d.quality = c.readUb(a, b, 5);
            c.readUb(a, b, 3);
            break;
          case 5:
            h = d.matrixX = c.readUi8(a, b);
            g = d.matrixY = c.readUi8(a, b);
            d.divisor = c.readFloat(a, b);
            d.bias = c.readFloat(a, b);
            e = d.matrix = [];
            for (h *= g;h--;) {
              e.push(c.readFloat(a, b));
            }
            d.color = l(a, b);
            c.readUb(a, b, 6);
            d.clamp = c.readUb(a, b, 1);
            d.preserveAlpha = c.readUb(a, b, 1);
            break;
          case 6:
            for (d = d.matrix = [], e = 20;e--;) {
              d.push(c.readFloat(a, b));
            }
          ;
        }
      }
      function C(a, b, c, d, e) {
        var h;
        h = w(a, b, c, d, e);
        var g = h.fillBits;
        h = h.lineBits;
        var f = c.records = [];
        do {
          var k = {};
          h = B(a, b, k, d, e, !1, g, h, !1, void 0);
          c = h.eos;
          g = h.fillBits;
          h = h.lineBits;
          f.push(k);
        } while (!c);
      }
      c.tagHandler = {0:void 0, 1:void 0, 2:f, 4:m, 5:r, 6:a, 7:d, 8:function(a, b, d, e, h) {
        d || (d = {});
        d.id = 0;
        d.imgData = c.readBinary(a, b, 0, !1);
        d.mimeType = "application/octet-stream";
        return d;
      }, 9:function(a, b, c, d, e) {
        c || (c = {});
        c.color = V(a, b);
        return c;
      }, 10:function(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        for (var g = c.readUi16(a, b), f = d.glyphCount = g / 2, k = [], s = f - 1;s--;) {
          k.push(c.readUi16(a, b));
        }
        d.offsets = [g].concat(k);
        for (g = d.glyphs = [];f--;) {
          k = {}, C(a, b, k, e, h), g.push(k);
        }
        return d;
      }, 11:p, 12:e, 13:void 0, 14:function(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        e = c.readUi8(a, b);
        d.soundFormat = e >> 4 & 15;
        d.soundRate = e >> 2 & 3;
        d.soundSize = e >> 1 & 1;
        d.soundType = e & 1;
        d.samplesCount = c.readUi32(a, b);
        d.soundData = c.readBinary(a, b, 0, !1);
        return d;
      }, 15:h, 17:void 0, 18:n, 19:function(a, b, d, e, h) {
        d || (d = {});
        d.data = c.readBinary(a, b, 0, !1);
        return d;
      }, 20:q, 21:a, 22:f, 23:void 0, 24:void 0, 26:m, 28:r, 32:f, 33:p, 34:d, 35:a, 36:q, 37:function(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        var f = d.bbox = {};
        g(a, b, f, e, h);
        e = c.readUi16(a, b);
        h = d.hasText = e >> 7 & 1;
        d.wordWrap = e >> 6 & 1;
        d.multiline = e >> 5 & 1;
        d.password = e >> 4 & 1;
        d.readonly = e >> 3 & 1;
        var f = d.hasColor = e >> 2 & 1, k = d.hasMaxLength = e >> 1 & 1, s = d.hasFont = e & 1, n = d.hasFontClass = e >> 15 & 1;
        d.autoSize = e >> 14 & 1;
        var q = d.hasLayout = e >> 13 & 1;
        d.noSelect = e >> 12 & 1;
        d.border = e >> 11 & 1;
        d.wasStatic = e >> 10 & 1;
        d.html = e >> 9 & 1;
        d.useOutlines = e >> 8 & 1;
        s && (d.fontId = c.readUi16(a, b));
        n && (d.fontClass = c.readString(a, b, 0));
        s && (d.fontHeight = c.readUi16(a, b));
        f && (d.color = l(a, b));
        k && (d.maxLength = c.readUi16(a, b));
        q && (d.align = c.readUi8(a, b), d.leftMargin = c.readUi16(a, b), d.rightMargin = c.readUi16(a, b), d.indent = c.readSi16(a, b), d.leading = c.readSi16(a, b));
        d.variableName = c.readString(a, b, 0);
        h && (d.initialText = c.readString(a, b, 0));
        return d;
      }, 39:void 0, 43:function(a, b, d, e, h) {
        d || (d = {});
        d.name = c.readString(a, b, 0);
        return d;
      }, 45:n, 46:f, 48:k, 56:function(a, b, d, e, h) {
        d || (d = {});
        h = c.readUi16(a, b);
        for (e = d.exports = [];h--;) {
          var g = {};
          g.symbolId = c.readUi16(a, b);
          g.className = c.readString(a, b, 0);
          e.push(g);
        }
        return d;
      }, 57:void 0, 58:void 0, 59:e, 60:void 0, 61:void 0, 62:void 0, 64:void 0, 65:void 0, 66:void 0, 69:function(a, b, d, e, h) {
        d || (d = {});
        c.readUb(a, b, 1);
        d.useDirectBlit = c.readUb(a, b, 1);
        d.useGpu = c.readUb(a, b, 1);
        d.hasMetadata = c.readUb(a, b, 1);
        d.doAbc = c.readUb(a, b, 1);
        d.noCrossDomainCaching = c.readUb(a, b, 1);
        d.relativeUrls = c.readUb(a, b, 1);
        d.network = c.readUb(a, b, 1);
        c.readUb(a, b, 24);
        return d;
      }, 70:m, 71:void 0, 72:b, 73:void 0, 74:void 0, 75:k, 76:function(a, b, d, e, h) {
        d || (d = {});
        h = c.readUi16(a, b);
        for (e = d.exports = [];h--;) {
          var g = {};
          g.symbolId = c.readUi16(a, b);
          g.className = c.readString(a, b, 0);
          e.push(g);
        }
        return d;
      }, 77:void 0, 78:function(a, b, d, e, h) {
        d || (d = {});
        d.symbolId = c.readUi16(a, b);
        var f = d.splitter = {};
        g(a, b, f, e, h);
        return d;
      }, 82:b, 83:f, 84:f, 86:function(a, b, d, e, h) {
        d || (d = {});
        h = c.readEncodedU32(a, b);
        for (e = d.scenes = [];h--;) {
          var g = {};
          g.offset = c.readEncodedU32(a, b);
          g.name = c.readString(a, b, 0);
          e.push(g);
        }
        h = c.readEncodedU32(a, b);
        for (e = d.labels = [];h--;) {
          g = {}, g.frame = c.readEncodedU32(a, b), g.name = c.readString(a, b, 0), e.push(g);
        }
        return d;
      }, 87:function(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        c.readUi32(a, b);
        d.data = c.readBinary(a, b, 0, !1);
        return d;
      }, 88:void 0, 89:h, 90:a, 91:function(a, b, d, e, h) {
        d || (d = {});
        d.id = c.readUi16(a, b);
        c.readUb(a, b, 5);
        e = d.hasFontData = c.readUb(a, b, 1);
        d.italic = c.readUb(a, b, 1);
        d.bold = c.readUb(a, b, 1);
        d.name = c.readString(a, b, 0);
        e && (d.data = c.readBinary(a, b, 0, !1));
        return d;
      }};
      c.readHeader = function(a, b, d, e, h) {
        d || (d = {});
        e = d.bbox = {};
        c.align(a, b);
        var g = c.readUb(a, b, 5);
        h = c.readSb(a, b, g);
        var f = c.readSb(a, b, g), k = c.readSb(a, b, g), g = c.readSb(a, b, g);
        e.xMin = h;
        e.xMax = f;
        e.yMin = k;
        e.yMax = g;
        c.align(a, b);
        e = c.readUi8(a, b);
        d.frameRate = c.readUi8(a, b) + e / 256;
        d.frameCount = c.readUi16(a, b);
        return d;
      };
    })(f.Parser || (f.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      function t(a, d, n, q, k, b) {
        function g() {
          d.pos = s;
          a._readTag = m;
          return!1;
        }
        var p = a.tags, l = d.bytes, s, m = null;
        a._readTag && (m = a._readTag, a._readTag = null);
        try {
          for (;d.pos < d.end;) {
            s = d.pos;
            if (d.pos + 2 > d.end) {
              return g();
            }
            var r = c.readUi16(l, d);
            if (!r) {
              q = !0;
              break;
            }
            var y = r >> 6, w = r & 63;
            if (63 === w) {
              if (d.pos + 4 > d.end) {
                return g();
              }
              w = c.readUi32(l, d);
            }
            if (m) {
              if (1 === y && 1 === m.code) {
                m.repeat++;
                d.pos += w;
                continue;
              }
              p.push(m);
              k && void 0 !== m.id && (a.bytesLoaded = a.bytesTotal * d.pos / d.end | 0, k(a));
              m = null;
            }
            if (d.pos + w > d.end) {
              return g();
            }
            var u = d.substream(d.pos, d.pos += w), D = u.bytes, w = {code:y};
            if (39 === y) {
              w.type = "sprite", w.id = c.readUi16(D, u), w.frameCount = c.readUi16(D, u), w.tags = [], t(w, u, n, !0, null, null) || f.Debug.error("Invalid SWF tag structure");
            } else {
              if (1 === y) {
                w.repeat = 1;
              } else {
                var C = c.tagHandler[y];
                C && C(D, u, w, n, y);
              }
            }
            m = w;
          }
          m && q || d.pos >= d.end ? (m && (m.finalTag = !0, p.push(m)), k && (a.bytesLoaded = a.bytesTotal, k(a))) : a._readTag = m;
        } catch (z) {
          throw b && b(z), z;
        }
        return!0;
      }
      function m(e) {
        function h(a, b, d) {
          var h = new Uint8Array(b);
          h.set(a);
          var f = a.length;
          q = {push:function(a, b) {
            h.set(a, f);
            f += a.length;
          }, close:function() {
            var a = {}, b;
            "image/jpeg" == d ? b = c.parseJpegChunks(a, h) : (c.parsePngHeaders(a, h), b = [h.subarray(0, f)]);
            var k = 0, n = 0;
            b.forEach(function(a) {
              k += a.byteLength;
            });
            var q = new Uint8Array(k);
            b.forEach(function(a) {
              q.set(a, n);
              n += a.byteLength;
            });
            a.id = 0;
            a.data = q;
            a.mimeType = d;
            a = {command:"image", type:"image", props:a};
            e.oncomplete && e.oncomplete(a);
          }};
          e.onimgprogress(b);
        }
        var n = new a, q = null;
        return{push:function(a, b) {
          if (null !== q) {
            return q.push(a, b);
          }
          if (!n.push(a, 8)) {
            return null;
          }
          var g = n.getHead(), m = g[0], l = g[1], s = g[2];
          if (87 === l && 83 === s) {
            f.Debug.assert(70 === m || 67 === m, "Unsupported compression format: " + (90 === m ? "LZMA" : String(m))), m = 67 === m, g = g[3], l = n.createStream(), l.pos += 4, l = c.readUi32(null, l) - 8, q = new p(g, l, e), m && (q = new d(q)), q.push(n.getTail(8), b);
          } else {
            var g = !1, A;
            255 === m && 216 === l && 255 === s ? (g = !0, A = "image/jpeg") : 137 === m && 80 === l && 78 === s && (g = !0, A = "image/png");
            g && h(a, b.bytesTotal, A);
          }
          n = null;
        }, close:function() {
          if (n) {
            var a = {command:"empty", data:n.getBytes()};
            e.oncomplete && e.oncomplete(a);
          }
          q && q.close && q.close();
        }};
      }
      var r = f.ArrayUtilities.Inflate, a = function() {
        function a(c) {
          "undefined" === typeof c && (c = 16);
          this._bufferSize = c;
          this._buffer = new Uint8Array(this._bufferSize);
          this._pos = 0;
        }
        a.prototype.push = function(a, c) {
          var d = this._pos + a.length;
          if (this._bufferSize < d) {
            for (var e = this._bufferSize;e < d;) {
              e <<= 1;
            }
            d = new Uint8Array(e);
            0 < this._bufferSize && d.set(this._buffer);
            this._buffer = d;
            this._bufferSize = e;
          }
          this._buffer.set(a, this._pos);
          this._pos += a.length;
          if (c) {
            return this._pos >= c;
          }
        };
        a.prototype.getHead = function() {
          return this._buffer.subarray(0, 8);
        };
        a.prototype.getTail = function(a) {
          return this._buffer.subarray(a, this._pos);
        };
        a.prototype.removeHead = function(a) {
          a = this.getTail(a);
          this._buffer = new Uint8Array(this._bufferSize);
          this._buffer.set(a);
          this._pos = a.length;
        };
        Object.defineProperty(a.prototype, "arrayBuffer", {get:function() {
          return this._buffer.buffer;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "length", {get:function() {
          return this._pos;
        }, enumerable:!0, configurable:!0});
        a.prototype.getBytes = function() {
          return this._buffer.subarray(0, this._pos);
        };
        a.prototype.createStream = function() {
          return new u.Stream(this.arrayBuffer, 0, this.length);
        };
        return a;
      }(), d = function() {
        function a(c) {
          this._inflate = new r(!0);
          this._inflate.onData = function(a) {
            c.push(a, this._progressInfo);
          }.bind(this);
        }
        a.prototype.push = function(a, c) {
          this._progressInfo = c;
          this._inflate.push(a);
        };
        a.prototype.close = function() {
          this._inflate = null;
        };
        return a;
      }(), p = function() {
        function d(c, e, f) {
          this.swf = {swfVersion:c, parseTime:0, bytesLoaded:void 0, bytesTotal:void 0, fileAttributes:void 0, tags:void 0};
          this._buffer = new a(32768);
          this._initialize = !0;
          this._totalRead = 0;
          this._length = e;
          this._options = f;
        }
        d.prototype.push = function(a, d) {
          if (0 !== a.length) {
            var e = this.swf, k = e.swfVersion, b = this._buffer, g = this._options, p, l = !1;
            d && (e.bytesLoaded = d.bytesLoaded, e.bytesTotal = d.bytesTotal, l = d.bytesLoaded >= d.bytesTotal);
            if (this._initialize) {
              if (!b.push(a, 27)) {
                return;
              }
              p = b.createStream();
              var s = p.bytes;
              c.readHeader(s, p, e, null, null);
              if (4420 == c.readUi16(s, p)) {
                p.pos + 4 > p.end && f.Debug.error("Not enough data.");
                var s = p.substream(p.pos, p.pos += 4), m = {code:69};
                (0,c.tagHandler[69])(s.bytes, s, m, k, 69);
                e.fileAttributes = m;
              } else {
                p.pos -= 2, e.fileAttributes = {};
              }
              if (g.onstart) {
                g.onstart(e);
              }
              e.tags = [];
              this._initialize = !1;
            } else {
              b.push(a), p = b.createStream();
            }
            s = performance.now();
            t(e, p, k, l, g.onprogress, g.onexception);
            e.parseTime += performance.now() - s;
            k = p.pos;
            b.removeHead(k);
            this._totalRead += k;
            if (g.oncomplete && e.tags[e.tags.length - 1].finalTag) {
              g.oncomplete(e);
            }
          }
        };
        d.prototype.close = function() {
        };
        return d;
      }();
      c.parseAsync = m;
      c.parse = function(a, c) {
        "undefined" === typeof c && (c = {});
        var d = m(c), f = new Uint8Array(a);
        d.push(f, {bytesLoaded:f.length, bytesTotal:f.length});
        d.close();
      };
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.Debug.assert, m = f.Debug.assertUnreachable, r = f.IntegerUtilities.roundToMultipleOfFour, a = f.ArrayUtilities.Inflate;
      (function(a) {
        a[a.FORMAT_COLORMAPPED = 3] = "FORMAT_COLORMAPPED";
        a[a.FORMAT_15BPP = 4] = "FORMAT_15BPP";
        a[a.FORMAT_24BPP = 5] = "FORMAT_24BPP";
      })(c.BitmapFormat || (c.BitmapFormat = {}));
      c.defineBitmap = function(c) {
        var p, e = 0;
        switch(c.format) {
          case 3:
            p = c.width;
            var e = c.height, h = c.hasAlpha, n = r(p) - p, q = h ? 4 : 3, k = r((c.colorTableSize + 1) * q), b = k + (p + n) * e, g = a.inflate(c.bmpData, b), V = new Uint32Array(p * e), l = 0, s = 0;
            if (h) {
              for (h = 0;h < e;h++) {
                for (var A = 0;A < p;A++) {
                  var s = g[k++] << 2, B = g[s + 3], y = g[s + 0], w = g[s + 1], s = g[s + 2];
                  V[l++] = s << 24 | w << 16 | y << 8 | B;
                }
                k += n;
              }
            } else {
              for (h = 0;h < e;h++) {
                for (A = 0;A < p;A++) {
                  s = g[k++] * q, B = 255, y = g[s + 0], w = g[s + 1], s = g[s + 2], V[l++] = s << 24 | w << 16 | y << 8 | B;
                }
                k += n;
              }
            }
            t(k === b, "We should be at the end of the data buffer now.");
            t(l === p * e, "Should have filled the entire image.");
            p = new Uint8Array(V.buffer);
            e = 1;
            break;
          case 5:
            q = c.width;
            b = c.height;
            n = c.hasAlpha;
            p = b * q * 4;
            e = a.inflate(c.bmpData, p);
            if (n) {
              p = e;
            } else {
              n = new Uint32Array(q * b);
              q *= b;
              for (g = b = 0;g < q;g++) {
                b++, V = e[b++], k = e[b++], l = e[b++], n[g] = l << 24 | k << 16 | V << 8 | 255;
              }
              t(b === p, "We should be at the end of the data buffer now.");
              p = new Uint8Array(n.buffer);
            }
            e = 1;
            break;
          case 4:
            f.Debug.notImplemented("parse15BPP");
            p = null;
            e = 1;
            break;
          default:
            m("invalid bitmap format");
        }
        return{type:"image", id:c.id, width:c.width, height:c.height, mimeType:"application/octet-stream", data:p, dataType:e};
      };
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.Debug.assert;
      c.defineButton = function(c, f) {
        for (var a = c.characters, d = {up:[], over:[], down:[], hitTest:[]}, p = 0, e;(e = a[p++]) && !e.eob;) {
          var h = f[e.symbolId];
          t(h, "undefined character button");
          h = {symbolId:h.id, depth:e.depth, flags:e.matrix ? 4 : 0, matrix:e.matrix};
          e.stateUp && d.up.push(h);
          e.stateOver && d.over.push(h);
          e.stateDown && d.down.push(h);
          e.stateHitTest && d.hitTest.push(h);
        }
        return{type:"button", id:c.id, buttonActions:c.buttonActions, states:d};
      };
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      function f(a) {
        for (var c = 0;2 <= a;) {
          a /= 2, ++c;
        }
        return d(2, c);
      }
      function m(a) {
        return n(a >> 8 & 255, a & 255);
      }
      function r(a) {
        return m(a >> 16) + m(a);
      }
      function a(a) {
        for (var c = 0, b = 0, d = 0, e = 0, h = 0;h < a.length;h++) {
          var f = a[h];
          if (f) {
            for (var f = f.records, n, p = 0, m = 0, w = 0;w < f.length;w++) {
              n = f[w];
              if (n.type) {
                n.isStraight ? (p += n.deltaX || 0, m += -(n.deltaY || 0)) : (p += n.controlDeltaX, m += -n.controlDeltaY, p += n.anchorDeltaX, m += -n.anchorDeltaY);
              } else {
                if (n.eos) {
                  break;
                }
                n.move && (p = n.moveX, m = -n.moveY);
              }
              c > p && (c = p);
              b > m && (b = m);
              d < p && (d = p);
              e < m && (e = m);
            }
          }
        }
        return 5E3 < Math.max(d - c, e - b);
      }
      var d = Math.pow, p = Math.min, e = Math.max, h = Math.log, n = String.fromCharCode;
      c.defineFont = function(c, d) {
        var b = "swf-font-" + c.id, g = c.name || b, n = {type:"font", id:c.id, name:g, bold:1 === c.bold, italic:1 === c.italic, codes:null, metrics:null, data:c.data, originalSize:!1}, l = c.glyphs, s = l ? c.glyphCount = l.length : 0;
        if (!s) {
          return n;
        }
        var A = {}, B = [], y = {}, w = [], u, D = !("advance" in c), C = 48 === c.code, z = 75 === c.code;
        D && (c.advance = []);
        u = Math.max.apply(null, c.codes) || 35;
        if (c.codes) {
          for (var v = 0;v < c.codes.length;v++) {
            var x = c.codes[v];
            if (32 > x || -1 < B.indexOf(x)) {
              u++, 8232 == u && (u = 8240), x = u;
            }
            B.push(x);
            y[x] = v;
          }
          u = B.concat();
          B.sort(function(a, b) {
            return a - b;
          });
          for (var v = 0, H;void 0 !== (x = B[v++]);) {
            var T = x, J = T;
            for (H = [v - 1];void 0 !== (x = B[v]) && J + 1 === x;) {
              ++J, H.push(v), ++v;
            }
            w.push([T, J, H]);
          }
        } else {
          H = [];
          for (v = 0;v < s;v++) {
            x = 57344 + v, B.push(x), y[x] = v, H.push(v);
          }
          w.push([57344, 57344 + s - 1, H]);
          u = B.concat();
        }
        var E = c.resolution || 1;
        C && a(l) && (E = 20, n.originalSize = !0);
        C = Math.ceil(c.ascent / E) || 1024;
        H = -Math.ceil(c.descent / E) || 0;
        for (var R = c.leading / E || 0, F = A["OS/2"] = "", S = "", U = "", X = "", v = 0, I;I = w[v++];) {
          T = I[0], J = I[1], x = I[2][0], F += m(T), S += m(J), U += m(x - T + 1 & 65535), X += m(0);
        }
        S += "\u00ff\u00ff";
        F += "\u00ff\u00ff";
        U += "\x00\u0001";
        X += "\x00\x00";
        w = w.length + 1;
        v = 2 * f(w);
        T = 2 * w - v;
        v = "\x00\x00" + m(2 * w) + m(v) + m(h(w) / h(2)) + m(T) + S + "\x00\x00" + F + U + X;
        A.cmap = "\x00\x00\x00\u0001\x00\u0003\x00\u0001\x00\x00\x00\f\x00\u0004" + m(v.length + 4) + v;
        var Q = "\x00\u0001\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x001\x00", F = "\x00\x00", w = 16, S = 0, X = [], T = [], J = [];
        I = [];
        for (var v = U = 0, ba = {};void 0 !== (x = B[v++]);) {
          for (var O = l[y[x]], Y = O.records, L = 0, M = 0, N = "", ga = "", K = 0, P = [], K = -1, O = 0;O < Y.length;O++) {
            N = Y[O];
            if (N.type) {
              0 > K && (K = 0, P[K] = {data:[], commands:[], xMin:0, xMax:0, yMin:0, yMax:0});
              if (N.isStraight) {
                P[K].commands.push(2);
                var W = (N.deltaX || 0) / E, Z = -(N.deltaY || 0) / E;
              } else {
                P[K].commands.push(3), W = N.controlDeltaX / E, Z = -N.controlDeltaY / E, L += W, M += Z, P[K].data.push(L, M), W = N.anchorDeltaX / E, Z = -N.anchorDeltaY / E;
              }
              L += W;
              M += Z;
              P[K].data.push(L, M);
            } else {
              if (N.eos) {
                break;
              }
              if (N.move) {
                K++;
                P[K] = {data:[], commands:[], xMin:0, xMax:0, yMin:0, yMax:0};
                P[K].commands.push(1);
                var ea = N.moveX / E, N = -N.moveY / E, W = ea - L, Z = N - M, L = ea, M = N;
                P[K].data.push(L, M);
              }
            }
            -1 < K && (P[K].xMin > L && (P[K].xMin = L), P[K].yMin > M && (P[K].yMin = M), P[K].xMax < L && (P[K].xMax = L), P[K].yMax < M && (P[K].yMax = M));
          }
          z || P.sort(function(a, b) {
            return(b.xMax - b.xMin) * (b.yMax - b.yMin) - (a.xMax - a.xMin) * (a.yMax - a.yMin);
          });
          ba[x] = P;
        }
        for (v = 0;void 0 !== (x = B[v++]);) {
          for (var O = l[y[x]], Y = O.records, P = ba[x], ha = 1, K = 0, W = M = N = L = "", x = M = L = 0, Y = -1024, ea = 0, ia = -1024, ga = N = "", K = 0, K = -1, ca = [], ja = [], O = 0;O < P.length;O++) {
            ca = ca.concat(P[O].data), ja = ja.concat(P[O].commands);
          }
          for (var $ = M = L = 0, aa = 0, da = "", P = "", fa = 0, K = 0, ha = 1, ga = "", O = 0;O < ja.length;O++) {
            W = ja[O], 1 === W ? (K && (++ha, ga += m(K - 1)), $ = ca[fa++], aa = ca[fa++], W = $ - L, Z = aa - M, N += "\u0001", da += m(W), P += m(Z), L = $, M = aa) : 2 === W ? ($ = ca[fa++], aa = ca[fa++], W = $ - L, Z = aa - M, N += "\u0001", da += m(W), P += m(Z), L = $, M = aa) : 3 === W && ($ = ca[fa++], aa = ca[fa++], W = $ - L, Z = aa - M, N += "\x00", da += m(W), P += m(Z), L = $, M = aa, K++, $ = ca[fa++], aa = ca[fa++], W = $ - L, Z = aa - M, N += "\u0001", da += m(W), P += m(Z), L = 
            $, M = aa), K++, K > S && (S = K), x > L && (x = L), ea > M && (ea = M), Y < L && (Y = L), ia < M && (ia = M);
          }
          L = ga += m((K || 1) - 1);
          M = da;
          W = P;
          O || (x = Y = ea = ia = 0, N += "1");
          O = m(ha) + m(x) + m(ea) + m(Y) + m(ia) + L + "\x00\x00" + N + M + W;
          O.length & 1 && (O += "\x00");
          Q += O;
          F += m(w / 2);
          w += O.length;
          X.push(x);
          T.push(Y);
          J.push(ea);
          I.push(ia);
          ha > U && (U = ha);
          K > S && (S = K);
          D && c.advance.push((Y - x) * E * 1.3);
        }
        F += m(w / 2);
        A.glyf = Q;
        z || (v = Math.min.apply(null, J), 0 > v && (H = H || v));
        A["OS/2"] = "\x00\u0001\x00\x00" + m(c.bold ? 700 : 400) + "\x00\u0005\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00ALF " + m((c.italic ? 1 : 0) | (c.bold ? 32 : 0)) + m(B[0]) + m(B[B.length - 1]) + m(C) + m(H) + m(R) + m(C) + m(-H) + "\x00\x00\x00\x00\x00\x00\x00\x00";
        A.head = "\x00\u0001\x00\x00\x00\u0001\x00\x00\x00\x00\x00\x00_\u000f<\u00f5\x00\x0B\u0004\x00\x00\x00\x00\x00" + r(Date.now()) + "\x00\x00\x00\x00" + r(Date.now()) + m(p.apply(null, X)) + m(p.apply(null, J)) + m(e.apply(null, T)) + m(e.apply(null, I)) + m((c.italic ? 2 : 0) | (c.bold ? 1 : 0)) + "\x00\b\x00\u0002\x00\x00\x00\x00";
        B = c.advance;
        A.hhea = "\x00\u0001\x00\x00" + m(C) + m(H) + m(R) + m(B ? e.apply(null, B) : 1024) + "\x00\x00\x00\x00\u0003\u00b8\x00\u0001\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" + m(s + 1);
        l = "\x00\x00\x00\x00";
        for (v = 0;v < s;++v) {
          l += m(B ? B[v] / E : 1024) + "\x00\x00";
        }
        A.hmtx = l;
        if (c.kerning) {
          B = c.kerning;
          E = B.length;
          v = 2 * f(E);
          E = "\x00\x00\x00\u0001\x00\x00" + m(14 + 6 * E) + "\x00\u0001" + m(E) + m(v) + m(h(E) / h(2)) + m(2 * E - v);
          for (v = 0;N = B[v++];) {
            E += m(y[N.code1]) + m(y[N.code2]) + m(N.adjustment);
          }
          A.kern = E;
        }
        A.loca = F;
        A.maxp = "\x00\u0001\x00\x00" + m(s + 1) + m(S) + m(U) + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        v = g.replace(/ /g, "");
        g = [c.copyright || "Original licence", g, "Unknown", b, g, "1.0", v, "Unknown", "Unknown", "Unknown"];
        v = g.length;
        b = "\x00\x00" + m(v) + m(12 * v + 6);
        for (v = w = 0;s = g[v++];) {
          b += "\x00\u0001\x00\x00\x00\x00" + m(v - 1) + m(s.length) + m(w), w += s.length;
        }
        A.name = b + g.join("");
        A.post = "\x00\u0003\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        g = Object.keys(A);
        v = g.length;
        s = "\x00\u0001\x00\x00" + m(v) + "\x00\u0080\x00\u0003\x00 ";
        y = "";
        w = 16 * v + s.length;
        for (v = 0;b = g[v++];) {
          B = A[b];
          E = B.length;
          for (s += b + "\x00\x00\x00\x00" + r(w) + r(E);E & 3;) {
            B += "\x00", ++E;
          }
          for (y += B;w & 3;) {
            ++w;
          }
          w += E;
        }
        A = s + y;
        C = {ascent:C / 1024, descent:-H / 1024, leading:R / 1024};
        H = new Uint8Array(A.length);
        for (v = 0;v < A.length;v++) {
          H[v] = A.charCodeAt(v) & 255;
        }
        n.codes = u;
        n.metrics = C;
        n.data = H;
        return n;
      };
    })(f.Parser || (f.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      function t(a, c) {
        return a[c] << 8 | a[c + 1];
      }
      function m(a, c) {
        var f = 0, p = c.length, k = [], b;
        do {
          for (var g = f;f < p && 255 !== c[f];) {
            ++f;
          }
          for (;f < p && 255 === c[f];) {
            ++f;
          }
          b = c[f++];
          if (218 === b) {
            f = p;
          } else {
            if (217 === b) {
              f += 2;
              continue;
            } else {
              if (208 > b || 216 < b) {
                var m = t(c, f);
                192 <= b && 195 >= b && (a.height = t(c, f + 3), a.width = t(c, f + 5));
                f += m;
              }
            }
          }
          k.push(c.subarray(g, f));
        } while (f < p);
        d(a.width && a.height, "bad jpeg image");
        return k;
      }
      function r(a, c) {
        if (73 === c[12] && 72 === c[13] && 68 === c[14] && 82 === c[15]) {
          a.width = c[16] << 24 | c[17] << 16 | c[18] << 8 | c[19];
          a.height = c[20] << 24 | c[21] << 16 | c[22] << 8 | c[23];
          var d = c[26];
          a.hasAlpha = 4 === d || 6 === d;
        }
      }
      function a(a) {
        for (var c = 0, d = 0;d < a.length;d++) {
          c += a[d].length;
        }
        for (var c = new Uint8Array(c), f = 0, d = 0;d < a.length;d++) {
          var k = a[d];
          c.set(k, f);
          f += k.length;
        }
        return c;
      }
      var d = f.Debug.assert, p = f.ArrayUtilities.Inflate;
      c.parseJpegChunks = m;
      c.parsePngHeaders = r;
      c.defineImage = function(c, h) {
        var n = {type:"image", id:c.id, mimeType:c.mimeType}, q = c.imgData;
        if ("image/jpeg" === c.mimeType) {
          var k = c.alphaData;
          if (k) {
            var b = new f.JPEG.JpegImage;
            b.parse(a(m(n, q)));
            d(n.width === b.width);
            d(n.height === b.height);
            var q = n.width * n.height, k = p.inflate(k, q), g = n.data = new Uint8ClampedArray(4 * q);
            b.copyToImageData(n);
            for (var b = 0, t = 3;b < q;b++, t += 4) {
              g[t] = k[b];
            }
            n.mimeType = "application/octet-stream";
            n.dataType = 3;
          } else {
            q = m(n, q), c.incomplete && (b = h[0], d(b, "missing jpeg tables"), (b = b.data) && b.size && (q[0] = q[0].subarray(2), q.unshift(b.slice(0, b.size - 2)))), n.data = a(q), n.dataType = 4;
          }
        } else {
          r(n, q), n.data = q, n.dataType = 5;
        }
        return n;
      };
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      var t = f.Debug.assert;
      c.defineLabel = function(c, f) {
        for (var a = c.records, d = c.bbox, p = "", e = [], h = [], n = 12, q = "Times Roman", k = 0, b = 0, g = 0, u = 0, l, s;(l = a[u++]) && !l.eot;) {
          l.hasFont && (q = f[l.fontId], t(q, "undefined label font"), s = q.codes, h.push(q.id), n = l.fontHeight, q.originalSize || (n /= 20), q = "swffont" + q.id);
          l.hasColor && (k = l.color >>> 8);
          l.hasMoveX && (b = l.moveX, b < d.xMin && (d.xMin = b));
          l.hasMoveY && (g = l.moveY, g < d.yMin && (d.yMin = g));
          var A = "";
          l = l.entries;
          for (var B = 0, y;y = l[B++];) {
            var w = s[y.glyphIndex];
            t(w, "undefined label glyph");
            A += String.fromCharCode(w);
            e.push(b, g);
            b += y.advance;
          }
          p += '<font size="' + n + '" face="' + q + '" color="#' + ("000000" + k.toString(16)).slice(-6) + '">' + A.replace(/[<>&]/g, function(a) {
            return "<" === a ? "&lt;" : ">" === a ? "&gt;" : "&amp;";
          }) + "</font>";
        }
        a = {type:"text", id:c.id, fillBounds:d, matrix:c.matrix, tag:{hasText:!0, initialText:p, html:!0, readonly:!0}, coords:e, static:!0, require:null};
        h.length && (a.require = h);
        return a;
      };
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      function t(a, b, c, d) {
        if (a) {
          if (b.fill0) {
            if (d = d[b.fill0 - 1], b.fill1 || b.line) {
              d.addSegment(a.toReversed());
            } else {
              a.isReversed = !0;
              return;
            }
          }
          b.line && b.fill1 && (d = c[b.line - 1], d.addSegment(a.clone()));
        }
      }
      function m(a, b, c, d) {
        if (b) {
          a.miterLimit = 2 * (a.miterLimitFactor || 1.5);
          if (!a.color && a.hasFill) {
            var e = m(a.fillStyle, !1, c, d);
            a.type = e.type;
            a.transform = e.transform;
            a.records = e.records;
            a.colors = e.colors;
            a.ratios = e.ratios;
            a.focalPoint = e.focalPoint;
            a.bitmapId = e.bitmapId;
            a.bitmapIndex = e.bitmapIndex;
            a.repeat = e.repeat;
            a.fillStyle = null;
            return a;
          }
          a.type = 0;
        }
        if (void 0 === a.type || 0 === a.type) {
          return a;
        }
        switch(a.type) {
          case 16:
          ;
          case 18:
          ;
          case 19:
            e = a.records;
            b = a.colors = [];
            c = a.ratios = [];
            for (d = 0;d < e.length;d++) {
              var h = e[d];
              b.push(h.color);
              c.push(h.ratio);
            }
            e = 819.2;
            break;
          case 64:
          ;
          case 65:
          ;
          case 66:
          ;
          case 67:
            a.smooth = 66 !== a.type && 67 !== a.type;
            a.repeat = 65 !== a.type && 67 !== a.type;
            c[a.bitmapId] ? (a.bitmapIndex = d.length, d.push(a.bitmapId), e = .05) : a.bitmapIndex = -1;
            break;
          default:
            q("shape parser encountered invalid fill style");
        }
        if (!a.matrix) {
          return a.transform = g, a;
        }
        b = a.matrix;
        a.transform = {a:b.a * e, b:b.b * e, c:b.c * e, d:b.d * e, tx:b.tx / 20, ty:b.ty / 20};
        a.matrix = null;
        return a;
      }
      function r(a, b, c, d) {
        for (var e = [], g = 0;g < a.length;g++) {
          var h = m(a[g], b, c, d);
          e[g] = b ? new l(null, h) : new l(h, null);
        }
        return e;
      }
      function a(a, b) {
        var c = a.noHscale ? a.noVscale ? 0 : 2 : a.noVscale ? 3 : 1, d = h(a.width, 0, 5100) | 0;
        b.lineStyle(d, a.color, a.pixelHinting, c, a.endCapsStyle, a.jointStyle, a.miterLimit);
      }
      var d = f.Bounds, p = f.ArrayUtilities.DataBuffer, e = f.ShapeData, h = f.NumberUtilities.clamp, n = f.Debug.assert, q = f.Debug.assertUnreachable, k = Array.prototype.push, b;
      (function(a) {
        a[a.Solid = 0] = "Solid";
        a[a.LinearGradient = 16] = "LinearGradient";
        a[a.RadialGradient = 18] = "RadialGradient";
        a[a.FocalRadialGradient = 19] = "FocalRadialGradient";
        a[a.RepeatingBitmap = 64] = "RepeatingBitmap";
        a[a.ClippedBitmap = 65] = "ClippedBitmap";
        a[a.NonsmoothedRepeatingBitmap = 66] = "NonsmoothedRepeatingBitmap";
        a[a.NonsmoothedClippedBitmap = 67] = "NonsmoothedClippedBitmap";
      })(b || (b = {}));
      var g = {a:1, b:0, c:0, d:1, tx:0, ty:0};
      c.defineShape = function(a, b) {
        for (var c = [], g = r(a.fillStyles, !1, b, c), h = r(a.lineStyles, !0, b, c), f = a.records, p = h, q = a.recordsMorph || null, h = null !== q, z = {fill0:0, fill1:0, line:0}, v = null, x, H, T = f.length - 1, J = 0, E = 0, R = 0, F = 0, S, U = 0, X = 0;U < T;U++) {
          var I = f[U], Q;
          h && (Q = q[X++]);
          if (0 === I.type) {
            v && t(v, z, p, g), I.hasNewStyles && (x || (x = []), k.apply(x, g), g = r(I.fillStyles, !1, b, c), k.apply(x, p), p = r(I.lineStyles, !0, b, c), H && (x.push(H), H = null), z = {fill0:0, fill1:0, line:0}), I.hasFillStyle0 && (z.fill0 = I.fillStyle0), I.hasFillStyle1 && (z.fill1 = I.fillStyle1), I.hasLineStyle && (z.line = I.lineStyle), z.fill1 ? S = g[z.fill1 - 1] : z.line ? S = p[z.line - 1] : z.fill0 && (S = g[z.fill0 - 1]), I.move && (J = I.moveX | 0, E = I.moveY | 0), S && (v = u.FromDefaults(h), 
            S.addSegment(v), h ? (0 === Q.type ? (R = Q.moveX | 0, F = Q.moveY | 0) : (R = J, F = E, X--), v.morphMoveTo(J, E, R, F)) : v.moveTo(J, E));
          } else {
            n(1 === I.type);
            v || (H || (H = new l(null, m({color:{red:0, green:0, blue:0, alpha:0}, width:20}, !0, b, c))), v = u.FromDefaults(h), H.addSegment(v), h ? v.morphMoveTo(J, E, R, F) : v.moveTo(J, E));
            if (h) {
              for (;Q && 0 === Q.type;) {
                Q = q[X++];
              }
              Q || (Q = I);
            }
            if (!I.isStraight || h && !Q.isStraight) {
              var ba, O, Y;
              I.isStraight ? (Y = I.deltaX | 0, I = I.deltaY | 0, ba = J + (Y >> 1), O = E + (I >> 1), J += Y, E += I) : (ba = J + I.controlDeltaX | 0, O = E + I.controlDeltaY | 0, J = ba + I.anchorDeltaX | 0, E = O + I.anchorDeltaY | 0);
              v.curveTo(ba, O, J, E);
              if (h) {
                if (Q.isStraight) {
                  Y = Q.deltaX | 0, I = Q.deltaY | 0, L = R + (Y >> 1), M = F + (I >> 1), R += Y, F += I;
                } else {
                  var L = R + Q.controlDeltaX | 0, M = F + Q.controlDeltaY | 0, R = L + Q.anchorDeltaX | 0, F = M + Q.anchorDeltaY | 0
                }
                v.morphCurveTo(ba, O, J, E, L, M, R, F);
              }
            } else {
              J += I.deltaX | 0, E += I.deltaY | 0, h ? (R += Q.deltaX | 0, F += Q.deltaY | 0, v.morphLineTo(J, E, R, F)) : v.lineTo(J, E);
            }
          }
        }
        t(v, z, p, g);
        x ? k.apply(x, g) : x = g;
        k.apply(x, p);
        H && x.push(H);
        f = new e;
        h && (f.morphCoordinates = new Int32Array(f.coordinates.length));
        for (U = 0;U < x.length;U++) {
          x[U].serialize(f);
        }
        a.lineBoundsMorph && (x = a.lineBounds = d.FromUntyped(a.lineBounds), H = a.lineBoundsMorph, x.extendByPoint(H.xMin, H.yMin), x.extendByPoint(H.xMax, H.yMax), x = a.fillBoundsMorph) && (H = a.fillBounds = a.fillBounds ? d.FromUntyped(a.fillBounds) : null, H.extendByPoint(x.xMin, x.yMin), H.extendByPoint(x.xMax, x.yMax));
        return{type:a.isMorph ? "morphshape" : "shape", id:a.id, fillBounds:a.fillBounds, lineBounds:a.lineBounds, morphFillBounds:a.fillBoundsMorph || null, morphLineBounds:a.lineBoundsMorph || null, shape:f.toPlainObject(), transferables:f.buffers, require:c.length ? c : null};
      };
      var u = function() {
        function a(b, c, d, e, h, g) {
          this.commands = b;
          this.data = c;
          this.morphData = d;
          this.prev = e;
          this.next = h;
          this.isReversed = g;
          this.id = a._counter++;
        }
        a.FromDefaults = function(b) {
          var c = new p, d = new p;
          c.endian = d.endian = "auto";
          var e = null;
          b && (e = new p, e.endian = "auto");
          return new a(c, d, e, null, null, !1);
        };
        a.prototype.moveTo = function(a, b) {
          this.commands.writeUnsignedByte(9);
          this.data.writeInt(a);
          this.data.writeInt(b);
        };
        a.prototype.morphMoveTo = function(a, b, c, d) {
          this.moveTo(a, b);
          this.morphData.writeInt(c);
          this.morphData.writeInt(d);
        };
        a.prototype.lineTo = function(a, b) {
          this.commands.writeUnsignedByte(10);
          this.data.writeInt(a);
          this.data.writeInt(b);
        };
        a.prototype.morphLineTo = function(a, b, c, d) {
          this.lineTo(a, b);
          this.morphData.writeInt(c);
          this.morphData.writeInt(d);
        };
        a.prototype.curveTo = function(a, b, c, d) {
          this.commands.writeUnsignedByte(11);
          this.data.writeInt(a);
          this.data.writeInt(b);
          this.data.writeInt(c);
          this.data.writeInt(d);
        };
        a.prototype.morphCurveTo = function(a, b, c, d, e, h, g, f) {
          this.curveTo(a, b, c, d);
          this.morphData.writeInt(e);
          this.morphData.writeInt(h);
          this.morphData.writeInt(g);
          this.morphData.writeInt(f);
        };
        a.prototype.toReversed = function() {
          n(!this.isReversed);
          return new a(this.commands, this.data, this.morphData, null, null, !0);
        };
        a.prototype.clone = function() {
          return new a(this.commands, this.data, this.morphData, null, null, this.isReversed);
        };
        a.prototype.storeStartAndEnd = function() {
          var a = this.data.ints, b = a[0] + "," + a[1], c = (this.data.length >> 2) - 2, a = a[c] + "," + a[c + 1];
          this.isReversed ? (this.startPoint = a, this.endPoint = b) : (this.startPoint = b, this.endPoint = a);
        };
        a.prototype.connectsTo = function(a) {
          n(a !== this);
          n(this.endPoint);
          n(a.startPoint);
          return this.endPoint === a.startPoint;
        };
        a.prototype.startConnectsTo = function(a) {
          n(a !== this);
          return this.startPoint === a.startPoint;
        };
        a.prototype.flipDirection = function() {
          var a = "", a = this.startPoint;
          this.startPoint = this.endPoint;
          this.endPoint = a;
          this.isReversed = !this.isReversed;
        };
        a.prototype.serialize = function(a, b) {
          if (this.isReversed) {
            this._serializeReversed(a, b);
          } else {
            var c = this.commands.bytes, d = this.data.length >> 2, e = this.morphData ? this.morphData.ints : null, h = this.data.ints;
            n(9 === c[0]);
            var g = 0;
            h[0] === b.x && h[1] === b.y && g++;
            for (var f = this.commands.length, k = 2 * g;g < f;g++) {
              k = this._writeCommand(c[g], k, h, e, a);
            }
            n(k === d);
            b.x = h[d - 2];
            b.y = h[d - 1];
          }
        };
        a.prototype._serializeReversed = function(a, b) {
          var c = this.commands.length, d = (this.data.length >> 2) - 2, e = this.commands.bytes;
          n(9 === e[0]);
          var h = this.data.ints, g = this.morphData ? this.morphData.ints : null;
          h[d] === b.x && h[d + 1] === b.y || this._writeCommand(9, d, h, g, a);
          if (1 !== c) {
            for (;1 < c--;) {
              var d = d - 2, f = e[c];
              a.writeCommandAndCoordinates(f, h[d], h[d + 1]);
              g && a.writeMorphCoordinates(g[d], g[d + 1]);
              11 === f && (d -= 2, a.writeCoordinates(h[d], h[d + 1]), g && a.writeMorphCoordinates(g[d], g[d + 1]));
            }
            n(0 === d);
          }
          b.x = h[0];
          b.y = h[1];
        };
        a.prototype._writeCommand = function(a, b, c, d, e) {
          e.writeCommandAndCoordinates(a, c[b++], c[b++]);
          d && e.writeMorphCoordinates(d[b - 2], d[b - 1]);
          11 === a && (e.writeCoordinates(c[b++], c[b++]), d && e.writeMorphCoordinates(d[b - 2], d[b - 1]));
          return b;
        };
        a._counter = 0;
        return a;
      }(), l = function() {
        function b(a, c) {
          this.fillStyle = a;
          this.lineStyle = c;
          this._head = null;
        }
        b.prototype.addSegment = function(a) {
          n(a);
          n(null === a.next);
          n(null === a.prev);
          var b = this._head;
          b && (n(a !== b), b.next = a, a.prev = b);
          this._head = a;
        };
        b.prototype.removeSegment = function(a) {
          a.prev && (a.prev.next = a.next);
          a.next && (a.next.prev = a.prev);
        };
        b.prototype.insertSegment = function(a, b) {
          var c = b.prev;
          a.prev = c;
          a.next = b;
          c && (c.next = a);
          b.prev = a;
        };
        b.prototype.head = function() {
          return this._head;
        };
        b.prototype.serialize = function(b) {
          var c = this.head();
          if (c) {
            for (;c;) {
              c.storeStartAndEnd(), c = c.prev;
            }
            for (var d = this.head(), e = d, h = c = null, g = d.prev;d;) {
              for (;g;) {
                g.startConnectsTo(d) && g.flipDirection(), g.connectsTo(d) ? (g.next !== d && (this.removeSegment(g), this.insertSegment(g, d)), d = g, g = d.prev) : (g.startConnectsTo(e) && g.flipDirection(), e.connectsTo(g) ? (this.removeSegment(g), e.next = g, g = g.prev, e.next.prev = e, e.next.next = null, e = e.next) : g = g.prev);
              }
              g = d.prev;
              c ? (h.next = d, d.prev = h, h = e, h.next = null) : (c = d, h = e);
              if (!g) {
                break;
              }
              d = e = g;
              g = d.prev;
            }
            if (this.fillStyle) {
              switch(g = this.fillStyle, g.type) {
                case 0:
                  b.beginFill(g.color);
                  break;
                case 16:
                ;
                case 18:
                ;
                case 19:
                  d = 16 === g.type ? 16 : 18;
                  b.beginGradient(2, g.colors, g.ratios, d, g.transform, g.spreadMethod, g.interpolationMode, g.focalPoint | 0);
                  break;
                case 65:
                ;
                case 64:
                ;
                case 67:
                ;
                case 66:
                  n(-1 < g.bitmapIndex);
                  b.beginBitmap(3, g.bitmapIndex, g.transform, g.repeat, g.smooth);
                  break;
                default:
                  q("Invalid fill style type: " + g.type);
              }
            } else {
              switch(g = this.lineStyle, n(g), g.type) {
                case 0:
                  a(g, b);
                  break;
                case 16:
                ;
                case 18:
                ;
                case 19:
                  d = 16 === g.type ? 16 : 18;
                  a(g, b);
                  b.beginGradient(6, g.colors, g.ratios, d, g.transform, g.spreadMethod, g.interpolationMode, g.focalPoint | 0);
                  break;
                case 65:
                ;
                case 64:
                ;
                case 67:
                ;
                case 66:
                  n(-1 < g.bitmapIndex);
                  a(g, b);
                  b.beginBitmap(7, g.bitmapIndex, g.transform, g.repeat, g.smooth);
                  break;
                default:
                  console.error("Line style type not yet supported: " + g.type);
              }
            }
            d = {x:0, y:0};
            for (g = c;g;) {
              g.serialize(b, d), g = g.next;
            }
            this.fillStyle ? b.endFill() : b.endLine();
            return b;
          }
        };
        return b;
      }();
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    (function(c) {
      function f(a, b, c, d, e) {
        var k = d >> 3, n = c * b * k, k = c * k, p = a.length + (a.length & 1), q = new ArrayBuffer(h.length + p), m = new Uint8Array(q);
        m.set(h);
        if (e) {
          e = 0;
          for (var r = h.length;e < a.length;e += 2, r += 2) {
            m[r] = a[e + 1], m[r + 1] = a[e];
          }
        } else {
          m.set(a, h.length);
        }
        a = new DataView(q);
        a.setUint32(4, p + 36, !0);
        a.setUint16(22, c, !0);
        a.setUint32(24, b, !0);
        a.setUint32(28, n, !0);
        a.setUint16(32, k, !0);
        a.setUint16(34, d, !0);
        a.setUint32(40, p, !0);
        return{data:m, mimeType:"audio/wav"};
      }
      function m(a, b, c) {
        function d(b) {
          for (;f < b;) {
            h = h << 8 | a[e++], f += 8;
          }
          f -= b;
          return h >>> f & (1 << b) - 1;
        }
        for (var e = 0, h = 0, f = 0, k = 0, p = d(2), m = n[p];k < b.length;) {
          var r = b[k++] = d(16) << 16 >> 16, t, v = d(6), u;
          1 < c && (t = b[k++] = d(16) << 16 >> 16, u = d(6));
          for (var H = 1 << p + 1, T = 0;4095 > T;T++) {
            for (var J = d(p + 2), E = q[v], R = 0, F = H >> 1;F;F >>= 1, E >>= 1) {
              J & F && (R += E);
            }
            r += (J & H ? -1 : 1) * (R + E);
            b[k++] = r = -32768 > r ? -32768 : 32767 < r ? 32767 : r;
            v += m[J & ~H];
            v = 0 > v ? 0 : 88 < v ? 88 : v;
            if (1 < c) {
              J = d(p + 2);
              E = q[u];
              R = 0;
              for (F = H >> 1;F;F >>= 1, E >>= 1) {
                J & F && (R += E);
              }
              t += (J & H ? -1 : 1) * (R + E);
              b[k++] = t = -32768 > t ? -32768 : 32767 < t ? 32767 : t;
              u += m[J & ~H];
              u = 0 > u ? 0 : 88 < u ? 88 : u;
            }
          }
        }
      }
      function r(a) {
        for (var b = new Float32Array(a.length), c = 0;c < b.length;c++) {
          b[c] = (a[c] - 128) / 128;
        }
        this.currentSample += b.length / this.channels;
        return{streamId:this.streamId, samplesCount:b.length / this.channels, pcm:b};
      }
      function a(a) {
        for (var b = new Float32Array(a.length / 2), c = 0, d = 0;c < b.length;c++, d += 2) {
          b[c] = (a[d] << 24 | a[d + 1] << 16) / 2147483648;
        }
        this.currentSample += b.length / this.channels;
        return{streamId:this.streamId, samplesCount:b.length / this.channels, pcm:b};
      }
      function d(a) {
        for (var b = new Float32Array(a.length / 2), c = 0, d = 0;c < b.length;c++, d += 2) {
          b[c] = (a[d + 1] << 24 | a[d] << 16) / 2147483648;
        }
        this.currentSample += b.length / this.channels;
        return{streamId:this.streamId, samplesCount:b.length / this.channels, pcm:b};
      }
      function p(a) {
        var b = a[1] << 8 | a[0], c = a[3] << 8 | a[2];
        this.currentSample += b;
        return{streamId:this.streamId, samplesCount:b, data:new Uint8Array(a.subarray(4)), seek:c};
      }
      var e = [5512, 11250, 22500, 44100], h = new Uint8Array([82, 73, 70, 70, 0, 0, 0, 0, 87, 65, 86, 69, 102, 109, 116, 32, 16, 0, 0, 0, 1, 0, 2, 0, 68, 172, 0, 0, 16, 177, 2, 0, 4, 0, 16, 0, 100, 97, 116, 97, 0, 0, 0, 0]);
      c.defineSound = function(a, b) {
        var c = 1 == a.soundType ? 2 : 1, d = a.samplesCount, h = e[a.soundRate], k = a.soundData, n;
        switch(a.soundFormat) {
          case 0:
            n = new Float32Array(d * c);
            if (1 == a.soundSize) {
              for (var p = d = 0;d < n.length;d++, p += 2) {
                n[d] = (k[p] << 24 | k[p + 1] << 16) / 2147483648;
              }
              k = f(k, h, c, 16, !0);
            } else {
              for (d = 0;d < n.length;d++) {
                n[d] = (k[d] - 128) / 128;
              }
              k = f(k, h, c, 8, !1);
            }
            break;
          case 3:
            n = new Float32Array(d * c);
            if (1 == a.soundSize) {
              for (p = d = 0;d < n.length;d++, p += 2) {
                n[d] = (k[p + 1] << 24 | k[p] << 16) / 2147483648;
              }
              k = f(k, h, c, 16, !1);
            } else {
              for (d = 0;d < n.length;d++) {
                n[d] = (k[d] - 128) / 128;
              }
              k = f(k, h, c, 8, !1);
            }
            break;
          case 2:
            k = {data:new Uint8Array(k.subarray(2)), mimeType:"audio/mpeg"};
            break;
          case 1:
            p = new Int16Array(d * c);
            m(k, p, c);
            n = new Float32Array(d * c);
            for (d = 0;d < n.length;d++) {
              n[d] = p[d] / 32768;
            }
            k = f(new Uint8Array(p.buffer), h, c, 16, !(new Uint8Array((new Uint16Array([1])).buffer))[0]);
            break;
          default:
            throw Error("Unsupported audio format: " + a.soundFormat);;
        }
        c = {type:"sound", id:a.id, sampleRate:h, channels:c, pcm:n, packaged:void 0};
        k && (c.packaged = k);
        return c;
      };
      var n = [[-1, 2], [-1, -1, 2, 4], [-1, -1, -1, -1, 2, 4, 6, 8], [-1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16]], q = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 
      9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767], k = 0, b = function() {
        function a(b, c, d) {
          this.streamId = k++;
          this.samplesCount = b;
          this.sampleRate = c;
          this.channels = d;
          this.format = null;
          this.currentSample = 0;
        }
        Object.defineProperty(a.prototype, "info", {get:function() {
          return{samplesCount:this.samplesCount, sampleRate:this.sampleRate, channels:this.channels, format:this.format, streamId:this.streamId};
        }, enumerable:!0, configurable:!0});
        return a;
      }();
      c.SwfSoundStream = b;
      c.createSoundStream = function(c) {
        var h = new b(c.samplesCount, e[c.streamRate], 1 == c.streamType ? 2 : 1);
        switch(c.streamCompression) {
          case 0:
            h.format = "wave";
            h.decode = 1 == c.soundSize ? a : r;
            break;
          case 3:
            h.format = "wave";
            h.decode = 1 == c.soundSize ? d : r;
            break;
          case 2:
            h.format = "mp3";
            h.decode = p;
            break;
          default:
            throw Error("Unsupported audio format: " + c.soundFormat);;
        }
        return h;
      };
    })(f.Parser || (f.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    (function(c) {
      c.defineText = function(c, m) {
        var r = [], a = !1, d = !1;
        if (c.hasFont) {
          var p = m[c.fontId];
          p ? (r.push(p.id), a = p.bold, d = p.italic) : f.Debug.warning("Font is not defined.");
        }
        a = {type:"text", id:c.id, fillBounds:c.bbox, variableName:c.variableName, tag:c, bold:a, italic:d, require:void 0};
        r.length && (a.require = r);
        return a;
      };
    })(u.Parser || (u.Parser = {}));
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    function c(a, c) {
      for (var d = 0, f = [], q, k, b = 16;0 < b && !a[b - 1];) {
        b--;
      }
      f.push({children:[], index:0});
      var g = f[0], m;
      for (q = 0;q < b;q++) {
        for (k = 0;k < a[q];k++) {
          g = f.pop();
          for (g.children[g.index] = c[d];0 < g.index;) {
            g = f.pop();
          }
          g.index++;
          for (f.push(g);f.length <= q;) {
            f.push(m = {children:[], index:0}), g.children[g.index] = m.children, g = m;
          }
          d++;
        }
        q + 1 < b && (f.push(m = {children:[], index:0}), g.children[g.index] = m.children, g = m);
      }
      return f[0].children;
    }
    function t(c, d, h, f, m, k, b, g, r) {
      function l() {
        if (0 < H) {
          return H--, x >> H & 1;
        }
        x = c[d++];
        if (255 == x) {
          var a = c[d++];
          if (a) {
            throw "unexpected marker: " + (x << 8 | a).toString(16);
          }
        }
        H = 7;
        return x >>> 7;
      }
      function s(a) {
        for (var b;null !== (b = l());) {
          a = a[b];
          if ("number" === typeof a) {
            return a;
          }
          if ("object" !== typeof a) {
            throw "invalid huffman sequence";
          }
        }
        return null;
      }
      function t(a) {
        for (var b = 0;0 < a;) {
          var c = l();
          if (null === c) {
            return;
          }
          b = b << 1 | c;
          a--;
        }
        return b;
      }
      function u(a) {
        if (1 === a) {
          return 1 === l() ? 1 : -1;
        }
        var b = t(a);
        return b >= 1 << a - 1 ? b : b + (-1 << a) + 1;
      }
      function y(b, c) {
        var d = s(b.huffmanTableDC), d = 0 === d ? 0 : u(d);
        b.blockData[c] = b.pred += d;
        for (d = 1;64 > d;) {
          var e = s(b.huffmanTableAC), h = e & 15, e = e >> 4;
          if (0 === h) {
            if (15 > e) {
              break;
            }
            d += 16;
          } else {
            d += e, b.blockData[c + a[d]] = u(h), d++;
          }
        }
      }
      function w(a, b) {
        var c = s(a.huffmanTableDC), c = 0 === c ? 0 : u(c) << r;
        a.blockData[b] = a.pred += c;
      }
      function G(a, b) {
        a.blockData[b] |= l() << r;
      }
      function D(c, d) {
        if (0 < T) {
          T--;
        } else {
          for (var e = k;e <= b;) {
            var h = s(c.huffmanTableAC), g = h & 15, h = h >> 4;
            if (0 === g) {
              if (15 > h) {
                T = t(h) + (1 << h) - 1;
                break;
              }
              e += 16;
            } else {
              e += h, c.blockData[d + a[e]] = u(g) * (1 << r), e++;
            }
          }
        }
      }
      function C(c, d) {
        for (var e = k, h = 0, g;e <= b;) {
          g = a[e];
          switch(J) {
            case 0:
              h = s(c.huffmanTableAC);
              g = h & 15;
              h >>= 4;
              if (0 === g) {
                15 > h ? (T = t(h) + (1 << h), J = 4) : (h = 16, J = 1);
              } else {
                if (1 !== g) {
                  throw "invalid ACn encoding";
                }
                E = u(g);
                J = h ? 2 : 3;
              }
              continue;
            case 1:
            ;
            case 2:
              c.blockData[d + g] ? c.blockData[d + g] += l() << r : (h--, 0 === h && (J = 2 == J ? 3 : 0));
              break;
            case 3:
              c.blockData[d + g] ? c.blockData[d + g] += l() << r : (c.blockData[d + g] = E << r, J = 0);
              break;
            case 4:
              c.blockData[d + g] && (c.blockData[d + g] += l() << r);
          }
          e++;
        }
        4 === J && (T--, 0 === T && (J = 0));
      }
      var z = h.mcusPerLine, v = d, x = 0, H = 0, T = 0, J = 0, E, R = f.length, F, S, U, X, I;
      g = h.progressive ? 0 === k ? 0 === g ? w : G : 0 === g ? D : C : y;
      var Q = 0;
      h = 1 == R ? f[0].blocksPerLine * f[0].blocksPerColumn : z * h.mcusPerColumn;
      m || (m = h);
      for (var ba, O;Q < h;) {
        for (S = 0;S < R;S++) {
          f[S].pred = 0;
        }
        T = 0;
        if (1 == R) {
          for (F = f[0], I = 0;I < m;I++) {
            g(F, 64 * ((F.blocksPerLine + 1) * (Q / F.blocksPerLine | 0) + Q % F.blocksPerLine)), Q++;
          }
        } else {
          for (I = 0;I < m;I++) {
            for (S = 0;S < R;S++) {
              for (F = f[S], ba = F.h, O = F.v, U = 0;U < O;U++) {
                for (X = 0;X < ba;X++) {
                  g(F, 64 * ((F.blocksPerLine + 1) * ((Q / z | 0) * F.v + U) + (Q % z * F.h + X)));
                }
              }
            }
            Q++;
          }
        }
        H = 0;
        F = c[d] << 8 | c[d + 1];
        if (65280 >= F) {
          throw "marker was not found";
        }
        if (65488 <= F && 65495 >= F) {
          d += 2;
        } else {
          break;
        }
      }
      return d - v;
    }
    function m(a, c) {
      for (var d = c.blocksPerLine, f = c.blocksPerColumn, m = new Int32Array(64), k = 0;k < f;k++) {
        for (var b = 0;b < d;b++) {
          for (var g = c, r = 64 * ((c.blocksPerLine + 1) * k + b), l = m, s = g.quantizationTable, t = void 0, u = void 0, y = void 0, w = void 0, G = void 0, D = void 0, C = void 0, z = void 0, v = void 0, x = void 0, x = 0;64 > x;x++) {
            l[x] = g.blockData[r + x] * s[x];
          }
          for (x = 0;8 > x;++x) {
            s = 8 * x, 0 === l[1 + s] && 0 === l[2 + s] && 0 === l[3 + s] && 0 === l[4 + s] && 0 === l[5 + s] && 0 === l[6 + s] && 0 === l[7 + s] ? (v = 5793 * l[0 + s] + 512 >> 10, l[0 + s] = v, l[1 + s] = v, l[2 + s] = v, l[3 + s] = v, l[4 + s] = v, l[5 + s] = v, l[6 + s] = v, l[7 + s] = v) : (t = 5793 * l[0 + s] + 128 >> 8, u = 5793 * l[4 + s] + 128 >> 8, y = l[2 + s], w = l[6 + s], G = 2896 * (l[1 + s] - l[7 + s]) + 128 >> 8, z = 2896 * (l[1 + s] + l[7 + s]) + 128 >> 8, D = l[3 + s] << 4, C = 
            l[5 + s] << 4, v = t - u + 1 >> 1, t = t + u + 1 >> 1, u = v, v = 3784 * y + 1567 * w + 128 >> 8, y = 1567 * y - 3784 * w + 128 >> 8, w = v, v = G - C + 1 >> 1, G = G + C + 1 >> 1, C = v, v = z + D + 1 >> 1, D = z - D + 1 >> 1, z = v, v = t - w + 1 >> 1, t = t + w + 1 >> 1, w = v, v = u - y + 1 >> 1, u = u + y + 1 >> 1, y = v, v = 2276 * G + 3406 * z + 2048 >> 12, G = 3406 * G - 2276 * z + 2048 >> 12, z = v, v = 799 * D + 4017 * C + 2048 >> 12, D = 4017 * D - 799 * C + 2048 >> 12, C = 
            v, l[0 + s] = t + z, l[7 + s] = t - z, l[1 + s] = u + C, l[6 + s] = u - C, l[2 + s] = y + D, l[5 + s] = y - D, l[3 + s] = w + G, l[4 + s] = w - G);
          }
          for (x = 0;8 > x;++x) {
            s = x, 0 === l[8 + s] && 0 === l[16 + s] && 0 === l[24 + s] && 0 === l[32 + s] && 0 === l[40 + s] && 0 === l[48 + s] && 0 === l[56 + s] ? (v = 5793 * l[x + 0] + 8192 >> 14, l[0 + s] = v, l[8 + s] = v, l[16 + s] = v, l[24 + s] = v, l[32 + s] = v, l[40 + s] = v, l[48 + s] = v, l[56 + s] = v) : (t = 5793 * l[0 + s] + 2048 >> 12, u = 5793 * l[32 + s] + 2048 >> 12, y = l[16 + s], w = l[48 + s], G = 2896 * (l[8 + s] - l[56 + s]) + 2048 >> 12, z = 2896 * (l[8 + s] + l[56 + s]) + 2048 >> 12, 
            D = l[24 + s], C = l[40 + s], v = t - u + 1 >> 1, t = t + u + 1 >> 1, u = v, v = 3784 * y + 1567 * w + 2048 >> 12, y = 1567 * y - 3784 * w + 2048 >> 12, w = v, v = G - C + 1 >> 1, G = G + C + 1 >> 1, C = v, v = z + D + 1 >> 1, D = z - D + 1 >> 1, z = v, v = t - w + 1 >> 1, t = t + w + 1 >> 1, w = v, v = u - y + 1 >> 1, u = u + y + 1 >> 1, y = v, v = 2276 * G + 3406 * z + 2048 >> 12, G = 3406 * G - 2276 * z + 2048 >> 12, z = v, v = 799 * D + 4017 * C + 2048 >> 12, D = 4017 * D - 799 * 
            C + 2048 >> 12, C = v, l[0 + s] = t + z, l[56 + s] = t - z, l[8 + s] = u + C, l[48 + s] = u - C, l[16 + s] = y + D, l[40 + s] = y - D, l[24 + s] = w + G, l[32 + s] = w - G);
          }
          for (x = 0;64 > x;++x) {
            t = r + x, u = l[x], u = -2056 >= u ? 0 : 2024 <= u ? 255 : u + 2056 >> 4, g.blockData[t] = u;
          }
        }
      }
      return c.blockData;
    }
    function r(a) {
      return 0 >= a ? 0 : 255 <= a ? 255 : a;
    }
    var a = new Int32Array([0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63]), d = function() {
      function d() {
      }
      d.prototype.parse = function(d) {
        function h() {
          var a = d[k] << 8 | d[k + 1];
          k += 2;
          return a;
        }
        function f() {
          var a = h(), a = d.subarray(k, k + a - 2);
          k += a.length;
          return a;
        }
        function p(a) {
          for (var b = Math.ceil(a.samplesPerLine / 8 / a.maxH), c = Math.ceil(a.scanLines / 8 / a.maxV), d = 0;d < a.components.length;d++) {
            x = a.components[d];
            var e = Math.ceil(Math.ceil(a.samplesPerLine / 8) * x.h / a.maxH), h = Math.ceil(Math.ceil(a.scanLines / 8) * x.v / a.maxV);
            x.blockData = new Int16Array(64 * c * x.v * (b * x.h + 1));
            x.blocksPerLine = e;
            x.blocksPerColumn = h;
          }
          a.mcusPerLine = b;
          a.mcusPerColumn = c;
        }
        var k = 0, b = null, g = null, r, l, s = [], u = [], B = [], y = h();
        if (65496 != y) {
          throw "SOI not found";
        }
        for (y = h();65497 != y;) {
          var w, G;
          switch(y) {
            case 65504:
            ;
            case 65505:
            ;
            case 65506:
            ;
            case 65507:
            ;
            case 65508:
            ;
            case 65509:
            ;
            case 65510:
            ;
            case 65511:
            ;
            case 65512:
            ;
            case 65513:
            ;
            case 65514:
            ;
            case 65515:
            ;
            case 65516:
            ;
            case 65517:
            ;
            case 65518:
            ;
            case 65519:
            ;
            case 65534:
              w = f();
              65504 === y && 74 === w[0] && 70 === w[1] && 73 === w[2] && 70 === w[3] && 0 === w[4] && (b = {version:{major:w[5], minor:w[6]}, densityUnits:w[7], xDensity:w[8] << 8 | w[9], yDensity:w[10] << 8 | w[11], thumbWidth:w[12], thumbHeight:w[13], thumbData:w.subarray(14, 14 + 3 * w[12] * w[13])});
              65518 === y && 65 === w[0] && 100 === w[1] && 111 === w[2] && 98 === w[3] && 101 === w[4] && 0 === w[5] && (g = {version:w[6], flags0:w[7] << 8 | w[8], flags1:w[9] << 8 | w[10], transformCode:w[11]});
              break;
            case 65499:
              for (var y = h() + k - 2, D;k < y;) {
                var C = d[k++], z = new Int32Array(64);
                if (0 === C >> 4) {
                  for (w = 0;64 > w;w++) {
                    D = a[w], z[D] = d[k++];
                  }
                } else {
                  if (1 === C >> 4) {
                    for (w = 0;64 > w;w++) {
                      D = a[w], z[D] = h();
                    }
                  } else {
                    throw "DQT: invalid table spec";
                  }
                }
                s[C & 15] = z;
              }
              break;
            case 65472:
            ;
            case 65473:
            ;
            case 65474:
              if (r) {
                throw "Only single frame JPEGs supported";
              }
              h();
              r = {};
              r.extended = 65473 === y;
              r.progressive = 65474 === y;
              r.precision = d[k++];
              r.scanLines = h();
              r.samplesPerLine = h();
              r.components = [];
              r.componentIds = {};
              w = d[k++];
              for (y = z = C = 0;y < w;y++) {
                D = d[k];
                G = d[k + 1] >> 4;
                var v = d[k + 1] & 15;
                C < G && (C = G);
                z < v && (z = v);
                G = r.components.push({h:G, v:v, quantizationTable:s[d[k + 2]]});
                r.componentIds[D] = G - 1;
                k += 3;
              }
              r.maxH = C;
              r.maxV = z;
              p(r);
              break;
            case 65476:
              D = h();
              for (y = 2;y < D;) {
                C = d[k++];
                z = new Uint8Array(16);
                for (w = G = 0;16 > w;w++, k++) {
                  G += z[w] = d[k];
                }
                v = new Uint8Array(G);
                for (w = 0;w < G;w++, k++) {
                  v[w] = d[k];
                }
                y += 17 + G;
                (0 === C >> 4 ? B : u)[C & 15] = c(z, v);
              }
              break;
            case 65501:
              h();
              l = h();
              break;
            case 65498:
              h();
              D = d[k++];
              w = [];
              for (var x, y = 0;y < D;y++) {
                C = r.componentIds[d[k++]], x = r.components[C], C = d[k++], x.huffmanTableDC = B[C >> 4], x.huffmanTableAC = u[C & 15], w.push(x);
              }
              y = d[k++];
              D = d[k++];
              C = d[k++];
              y = t(d, k, r, w, l, y, D, C >> 4, C & 15);
              k += y;
              break;
            default:
              if (255 == d[k - 3] && 192 <= d[k - 2] && 254 >= d[k - 2]) {
                k -= 3;
                break;
              }
              throw "unknown JPEG marker " + y.toString(16);;
          }
          y = h();
        }
        this.width = r.samplesPerLine;
        this.height = r.scanLines;
        this.jfif = b;
        this.adobe = g;
        this.components = [];
        for (y = 0;y < r.components.length;y++) {
          x = r.components[y], this.components.push({output:m(r, x), scaleX:x.h / r.maxH, scaleY:x.v / r.maxV, blocksPerLine:x.blocksPerLine, blocksPerColumn:x.blocksPerColumn});
        }
        this.numComponents = this.components.length;
      };
      d.prototype._getLinearizedBlockData = function(a, c) {
        var d = this.width / a, f = this.height / c, k, b, g, p, l, m, r = 0, t, u = this.components.length, w = a * c * u, G = new Uint8Array(w), D = new Uint32Array(a);
        for (m = 0;m < u;m++) {
          k = this.components[m];
          b = k.scaleX * d;
          g = k.scaleY * f;
          r = m;
          t = k.output;
          p = k.blocksPerLine + 1 << 3;
          for (l = 0;l < a;l++) {
            k = 0 | l * b, D[l] = (k & 4294967288) << 3 | k & 7;
          }
          for (b = 0;b < c;b++) {
            for (k = 0 | b * g, k = p * (k & 4294967288) | (k & 7) << 3, l = 0;l < a;l++) {
              G[r] = t[k + D[l]], r += u;
            }
          }
        }
        if (f = this.decodeTransform) {
          for (m = 0;m < w;) {
            for (d = k = 0;k < u;k++, m++, d += 2) {
              G[m] = (G[m] * f[d] >> 8) + f[d + 1];
            }
          }
        }
        return G;
      };
      d.prototype._isColorConversionNeeded = function() {
        return this.adobe && this.adobe.transformCode ? !0 : 3 == this.numComponents ? !0 : !1;
      };
      d.prototype._convertYccToRgb = function(a) {
        for (var c, d, f, k = 0, b = a.length;k < b;k += 3) {
          c = a[k], d = a[k + 1], f = a[k + 2], a[k] = r(c - 179.456 + 1.402 * f), a[k + 1] = r(c + 135.459 - .344 * d - .714 * f), a[k + 2] = r(c - 226.816 + 1.772 * d);
        }
        return a;
      };
      d.prototype._convertYcckToRgb = function(a) {
        for (var c, d, f, k, b, g, p, l, m, t, u, y, w, G, D = 0, C = 0, z = a.length;C < z;C += 4) {
          c = a[C];
          d = a[C + 1];
          f = a[C + 2];
          k = a[C + 3];
          b = d * d;
          g = d * f;
          p = d * c;
          l = d * k;
          m = f * f;
          t = f * k;
          u = f * c;
          y = c * c;
          w = c * k;
          G = k * k;
          var v = -122.67195406894 - 6.60635669420364E-5 * b + 4.37130475926232E-4 * g - 5.4080610064599E-5 * p + 4.8449797120281E-4 * l - .154362151871126 * d - 9.57964378445773E-4 * m + 8.17076911346625E-4 * u - .00477271405408747 * t + 1.53380253221734 * f + 9.61250184130688E-4 * y - .00266257332283933 * w + .48357088451265 * c - 3.36197177618394E-4 * G + .484791561490776 * k, x = 107.268039397724 + 2.19927104525741E-5 * b - 6.40992018297945E-4 * g + 6.59397001245577E-4 * p + 4.26105652938837E-4 * 
          l - .176491792462875 * d - 7.78269941513683E-4 * m + .00130872261408275 * u + 7.70482631801132E-4 * t - .151051492775562 * f + .00126935368114843 * y - .00265090189010898 * w + .25802910206845 * c - 3.18913117588328E-4 * G - .213742400323665 * k;
          c = -20.810012546947 - 5.70115196973677E-4 * b - 2.63409051004589E-5 * g + .0020741088115012 * p - .00288260236853442 * l + .814272968359295 * d - 1.53496057440975E-5 * m - 1.32689043961446E-4 * u + 5.60833691242812E-4 * t - .195152027534049 * f + .00174418132927582 * y - .00255243321439347 * w + .116935020465145 * c - 3.43531996510555E-4 * G + .24165260232407 * k;
          a[D++] = r(v);
          a[D++] = r(x);
          a[D++] = r(c);
        }
        return a;
      };
      d.prototype._convertYcckToCmyk = function(a) {
        for (var c, d, f, k = 0, b = a.length;k < b;k += 4) {
          c = a[k], d = a[k + 1], f = a[k + 2], a[k] = r(434.456 - c - 1.402 * f), a[k + 1] = r(119.541 - c + .344 * d + .714 * f), a[k + 2] = r(481.816 - c - 1.772 * d);
        }
        return a;
      };
      d.prototype._convertCmykToRgb = function(a) {
        for (var c, d, f, k, b = 0, g = 1 / 255 / 255, p = 0, l = a.length;p < l;p += 4) {
          c = a[p];
          d = a[p + 1];
          f = a[p + 2];
          k = a[p + 3];
          var m = c * (-4.387332384609988 * c + 54.48615194189176 * d + 18.82290502165302 * f + 212.25662451639585 * k - 72734.4411664936) + d * (1.7149763477362134 * d - 5.6096736904047315 * f - 17.873870861415444 * k - 1401.7366389350734) + f * (-2.5217340131683033 * f - 21.248923337353073 * k + 4465.541406466231) - k * (21.86122147463605 * k + 48317.86113160301), r = c * (8.841041422036149 * c + 60.118027045597366 * d + 6.871425592049007 * f + 31.159100130055922 * k - 20220.756542821975) + d * 
          (-15.310361306967817 * d + 17.575251261109482 * f + 131.35250912493976 * k - 48691.05921601825) + f * (4.444339102852739 * f + 9.8632861493405 * k - 6341.191035517494) - k * (20.737325471181034 * k + 47890.15695978492);
          c = c * (.8842522430003296 * c + 8.078677503112928 * d + 30.89978309703729 * f - .23883238689178934 * k - 3616.812083916688) + d * (10.49593273432072 * d + 63.02378494754052 * f + 50.606957656360734 * k - 28620.90484698408) + f * (.03296041114873217 * f + 115.60384449646641 * k - 49363.43385999684) - k * (22.33816807309886 * k + 45932.16563550634);
          a[b++] = 0 <= m ? 255 : -16581375 >= m ? 0 : 255 + m * g | 0;
          a[b++] = 0 <= r ? 255 : -16581375 >= r ? 0 : 255 + r * g | 0;
          a[b++] = 0 <= c ? 255 : -16581375 >= c ? 0 : 255 + c * g | 0;
        }
        return a;
      };
      d.prototype.getData = function(a, c, d) {
        if (4 < this.numComponents) {
          throw "Unsupported color mode";
        }
        a = this._getLinearizedBlockData(a, c);
        return 3 === this.numComponents ? this._convertYccToRgb(a) : 4 === this.numComponents ? this._isColorConversionNeeded() ? d ? this._convertYcckToRgb(a) : this._convertYcckToCmyk(a) : this._convertCmykToRgb(a) : a;
      };
      d.prototype.copyToImageData = function(a) {
        var c = a.width, d = a.height, f = c * d * 4;
        a = a.data;
        var c = this.getData(c, d, !0), k = d = 0, b, g, p, l;
        switch(this.components.length) {
          case 1:
            for (;k < f;) {
              b = c[d++], a[k++] = b, a[k++] = b, a[k++] = b, a[k++] = 255;
            }
            break;
          case 3:
            for (;k < f;) {
              p = c[d++], l = c[d++], b = c[d++], a[k++] = p, a[k++] = l, a[k++] = b, a[k++] = 255;
            }
            break;
          case 4:
            for (;k < f;) {
              p = c[d++], l = c[d++], b = c[d++], g = c[d++], p = 255 - r(p * (1 - g / 255) + g), l = 255 - r(l * (1 - g / 255) + g), b = 255 - r(b * (1 - g / 255) + g), a[k++] = p, a[k++] = l, a[k++] = b, a[k++] = 255;
            }
            break;
          default:
            throw "Unsupported color mode";;
        }
      };
      return d;
    }();
    f.JpegImage = d;
  })(f.JPEG || (f.JPEG = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(f) {
    function c() {
      this.bitBuffer = this.bitLength = 0;
    }
    function t(a) {
      if (this.pos + a > this.end) {
        throw f.StreamNoDataError;
      }
    }
    function m() {
      return this.end - this.pos;
    }
    function r(a, c) {
      var f = new d(this.bytes);
      f.pos = a;
      f.end = c;
      return f;
    }
    function a(a) {
      var c = this.bytes, d = this.end + a.length;
      if (d > c.length) {
        throw "stream buffer overfow";
      }
      c.set(a, this.end);
      this.end = d;
    }
    f.StreamNoDataError = {};
    var d = function() {
      return function(d, e, f, n) {
        void 0 === e && (e = 0);
        d.buffer instanceof ArrayBuffer && (e += d.byteOffset, d = d.buffer);
        void 0 === f && (f = d.byteLength - e);
        void 0 === n && (n = f);
        var q = new Uint8Array(d, e, n);
        d = new DataView(d, e, n);
        d.bytes = q;
        d.pos = 0;
        d.end = f;
        d.bitBuffer = 0;
        d.bitLength = 0;
        d.align = c;
        d.ensure = t;
        d.remaining = m;
        d.substream = r;
        d.push = a;
        return d;
      };
    }();
    f.Stream = d;
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
(function(f) {
  (function(u) {
    function c(a, p, e) {
      var h;
      switch(a.code) {
        case 6:
        ;
        case 21:
        ;
        case 35:
        ;
        case 90:
        ;
        case 8:
          h = f.SWF.Parser.defineImage(a, p);
          break;
        case 20:
        ;
        case 36:
          h = f.SWF.Parser.defineBitmap(a);
          break;
        case 7:
        ;
        case 34:
          h = f.SWF.Parser.defineButton(a, p);
          break;
        case 37:
          h = f.SWF.Parser.defineText(a, p);
          break;
        case 10:
        ;
        case 48:
        ;
        case 75:
        ;
        case 91:
          h = f.SWF.Parser.defineFont(a, p);
          break;
        case 46:
        ;
        case 84:
        ;
        case 2:
        ;
        case 22:
        ;
        case 32:
        ;
        case 83:
          h = f.SWF.Parser.defineShape(a, p);
          break;
        case 14:
          h = f.SWF.Parser.defineSound(a, p);
          break;
        case 87:
          h = {type:"binary", id:a.id, data:a.data};
          break;
        case 39:
          for (var n = [], q = {type:"frame"}, k = [], b = a.tags, g = null, t = 0, l = null, s = 0, u = b.length;s < u;s++) {
            var B = b[s];
            if ("id" in B) {
              h = c(B, p, e), e(h, h.transferables);
            } else {
              switch(B.code) {
                case 12:
                  g || (g = []);
                  g.push(t);
                  g.push(B.actionsData);
                  break;
                case 15:
                  n.push(B);
                  break;
                case 18:
                  try {
                    l = r(B), q.soundStream = l.info;
                  } catch (y) {
                  }
                  break;
                case 19:
                  l && (q.soundStreamBlock = l.decode(B.data));
                  break;
                case 43:
                  q.labelName = B.name;
                  break;
                case 4:
                ;
                case 26:
                ;
                case 70:
                  n.push(B);
                  break;
                case 5:
                ;
                case 28:
                  n.push(B);
                  break;
                case 1:
                  t += B.repeat;
                  q.repeat = B.repeat;
                  q.commands = n;
                  k.push(q);
                  n = [];
                  q = {type:"frame"};
                  break;
                default:
                  f.Debug.warning("Dropped tag during parsing. Code: " + B.code + " (" + m[B.code] + ")");
              }
            }
          }
          0 === k.length && (q.repeat = 1, q.commands = n, k.push(q));
          h = {type:"sprite", id:a.id, frameCount:a.frameCount, frames:k, frameScripts:g};
          break;
        case 11:
        ;
        case 33:
          h = f.SWF.Parser.defineLabel(a, p);
          break;
        default:
          f.Debug.warning("Dropped tag during parsing. Code: " + B.code + " (" + m[B.code] + ")");
      }
      if (!h) {
        return{command:"error", message:"unknown symbol type: " + a.code};
      }
      h.isSymbol = !0;
      return p[a.id] = h;
    }
    function t(a) {
      var p = [], e = {}, h = {type:"frame"}, n = 0, q = null, k = 0;
      return{onstart:function(b) {
        a({command:"init", result:b});
      }, onimgprogress:function(b) {
        for (;k <= b;) {
          a({command:"progress", result:{bytesLoaded:k, bytesTotal:b, open:!0}}), k += Math.min(b - k || 1024, 1024);
        }
      }, onprogress:function(b) {
        if (65536 <= b.bytesLoaded - k) {
          for (;k < b.bytesLoaded;) {
            k && a({command:"progress", result:{bytesLoaded:k, bytesTotal:b.bytesTotal}}), k += 65536;
          }
        }
        for (var g = b.tags, t = g.length;n < t;n++) {
          var l = g[n];
          if ("id" in l) {
            l = c(l, e, a), a(l, l.transferables);
          } else {
            switch(l.code) {
              case 86:
                h.sceneData = l;
                break;
              case 78:
                a({isSymbol:!0, id:l.symbolId, updates:{scale9Grid:l.splitter}});
                break;
              case 82:
              ;
              case 72:
                a({type:"abc", flags:l.flags, name:l.name, data:l.data});
                break;
              case 12:
                var s = h.actionBlocks;
                s ? s.push(l.actionsData) : h.actionBlocks = [l.actionsData];
                break;
              case 59:
                (h.initActionBlocks || (h.initActionBlocks = [])).push({spriteId:l.spriteId, actionsData:l.actionsData});
                break;
              case 15:
                p.push(l);
                break;
              case 18:
                try {
                  q = r(l), h.soundStream = q.info;
                } catch (u) {
                }
                break;
              case 19:
                q && (h.soundStreamBlock = q.decode(l.data));
                break;
              case 56:
                s = h.exports;
                h.exports = s ? s.concat(l.exports) : l.exports.slice(0);
                break;
              case 76:
                s = h.symbolClasses;
                h.symbolClasses = s ? s.concat(l.exports) : l.exports.slice(0);
                break;
              case 43:
                h.labelName = l.name;
                break;
              case 4:
              ;
              case 26:
              ;
              case 70:
                p.push(l);
                break;
              case 5:
              ;
              case 28:
                p.push(l);
                break;
              case 9:
                h.bgcolor = l.color;
                break;
              case 1:
                h.repeat = l.repeat;
                h.commands = p;
                h.complete = !!l.finalTag;
                a(h);
                p = [];
                h = {type:"frame"};
                break;
              default:
                f.Debug.warning("Dropped tag during parsing. Code: " + l.code + " (" + m[l.code] + ")");
            }
          }
        }
        b.bytesLoaded >= b.bytesTotal && a({command:"progress", result:{bytesLoaded:b.bytesLoaded, bytesTotal:b.bytesTotal}});
      }, oncomplete:function(b) {
        a(b);
        var c;
        "number" === typeof b.swfVersion && (c = b.bbox, c = {topic:"parseInfo", parseTime:b.parseTime, bytesTotal:b.bytesTotal, swfVersion:b.swfVersion, frameRate:b.frameRate, width:(c.xMax - c.xMin) / 20, height:(c.yMax - c.yMin) / 20, isAvm2:!!b.fileAttributes.doAbc});
        a({command:"complete", stats:c});
      }, onexception:function(b) {
        a({type:"exception", message:b.message, stack:b.stack});
      }};
    }
    var m = f.SWF.Parser.SwfTag, r = f.SWF.Parser.createSoundStream, a = function() {
      function a(c, d) {
        this._subscription = null;
        var f = this;
        d ? (this._messenger = c, c.onmessage = function(a) {
          f.listener(a.data);
        }) : this._messenger = {postMessage:function(a) {
          f.onmessage({data:a});
        }};
      }
      a.prototype.terminate = function() {
        this.listener = this._messenger = null;
      };
      a.prototype.onmessage = function(a) {
        this.listener(a.data);
      };
      a.prototype.postMessage = function(a) {
        this.listener && this.listener(a);
      };
      a.prototype.listener = function(a) {
        this._subscription ? this._subscription.callback(a.data, a.progress) : "pipe:" === a ? (this._subscription = {subscribe:function(a) {
          this.callback = a;
        }}, this.parseLoadedData(this._messenger, this._subscription)) : this.parseLoadedData(this._messenger, a);
      };
      a.prototype.parseLoadedData = function(a, c) {
        function d(c, b) {
          try {
            a.postMessage(c, b);
          } catch (e) {
            if ("DataCloneError" != e) {
              throw e;
            }
            a.postMessage(c);
          }
        }
        if (c instanceof ArrayBuffer) {
          f.SWF.Parser.parse(c, t(d));
        } else {
          if ("subscribe" in c) {
            var m = f.SWF.Parser.parseAsync(t(d));
            c.subscribe(function(a, b) {
              a ? m.push(a, b) : m.close();
            });
          } else {
            if ("undefined" !== typeof FileReaderSync) {
              var q = (new FileReaderSync).readAsArrayBuffer(c);
              f.SWF.Parser.parse(q, t(d));
            } else {
              q = new FileReader, q.onload = function() {
                f.SWF.Parser.parse(this.result, t(d));
              }, q.readAsArrayBuffer(c);
            }
          }
        }
      };
      return a;
    }();
    u.ResourceLoader = a;
  })(f.SWF || (f.SWF = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load SWF Parser");

