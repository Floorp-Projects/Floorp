console.time("Load Shared Dependencies");
var jsGlobal = function() {
  return this || (0,eval)("this");
}(), inBrowser = "undefined" != typeof console;
jsGlobal.performance || (jsGlobal.performance = {});
jsGlobal.performance.now || (jsGlobal.performance.now = "undefined" !== typeof dateNow ? dateNow : Date.now);
function log(d) {
  for (var m = 0;m < arguments.length - 1;m++) {
  }
  jsGlobal.print.apply(jsGlobal, arguments);
}
function warn(d) {
  for (var m = 0;m < arguments.length - 1;m++) {
  }
  inBrowser ? console.warn.apply(console, arguments) : jsGlobal.print(Shumway.IndentingWriter.RED + d + Shumway.IndentingWriter.ENDC);
}
var Shumway;
(function(d) {
  function m(b) {
    return(b | 0) === b;
  }
  function s(b) {
    return "object" === typeof b || "function" === typeof b;
  }
  function e(b) {
    return String(Number(b)) === b;
  }
  function t(b) {
    var h = 0;
    if ("number" === typeof b) {
      return h = b | 0, b === h && 0 <= h ? !0 : b >>> 0 === b;
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
    if (a > d.UINT32_CHAR_BUFFER_LENGTH) {
      return!1;
    }
    var n = 0, h = b.charCodeAt(n++) - 48;
    if (1 > h || 9 < h) {
      return!1;
    }
    for (var f = 0, q = 0;n < a;) {
      q = b.charCodeAt(n++) - 48;
      if (0 > q || 9 < q) {
        return!1;
      }
      f = h;
      h = 10 * h + q;
    }
    return f < d.UINT32_MAX_DIV_10 || f === d.UINT32_MAX_DIV_10 && q <= d.UINT32_MAX_MOD_10 ? !0 : !1;
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
  })(d.CharacterCodes || (d.CharacterCodes = {}));
  d.UINT32_CHAR_BUFFER_LENGTH = 10;
  d.UINT32_MAX = 4294967295;
  d.UINT32_MAX_DIV_10 = 429496729;
  d.UINT32_MAX_MOD_10 = 5;
  d.isString = function(b) {
    return "string" === typeof b;
  };
  d.isFunction = function(b) {
    return "function" === typeof b;
  };
  d.isNumber = function(b) {
    return "number" === typeof b;
  };
  d.isInteger = m;
  d.isArray = function(b) {
    return b instanceof Array;
  };
  d.isNumberOrString = function(b) {
    return "number" === typeof b || "string" === typeof b;
  };
  d.isObject = s;
  d.toNumber = function(b) {
    return+b;
  };
  d.isNumericString = e;
  d.isNumeric = function(b) {
    if ("number" === typeof b) {
      return!0;
    }
    if ("string" === typeof b) {
      var h = b.charCodeAt(0);
      return 65 <= h && 90 >= h || 97 <= h && 122 >= h || 36 === h || 95 === h ? !1 : t(b) || e(b);
    }
    return!1;
  };
  d.isIndex = t;
  d.isNullOrUndefined = function(b) {
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
    b.error = function(h) {
      inBrowser ? warn(h) : warn(h + "\n\nStack Trace:\n" + b.backtrace());
      throw Error(h);
    };
    b.assert = function(h, n) {
      "undefined" === typeof n && (n = "assertion failed");
      "" === h && (h = !0);
      h || b.error(n.toString());
    };
    b.assertUnreachable = function(b) {
      throw Error("Reached unreachable location " + Error().stack.split("\n")[1] + b);
    };
    b.assertNotImplemented = function(h, n) {
      h || b.error("notImplemented: " + n);
    };
    b.warning = function(b) {
      warn(b);
    };
    b.notUsed = function(h) {
      b.assert(!1, "Not Used " + h);
    };
    b.notImplemented = function(h) {
      log("release: false");
      b.assert(!1, "Not Implemented " + h);
    };
    b.abstractMethod = function(h) {
      b.assert(!1, "Abstract Method " + h);
    };
    var h = {};
    b.somewhatImplemented = function(a) {
      h[a] || (h[a] = !0, b.warning("somewhatImplemented: " + a));
    };
    b.unexpected = function(h) {
      b.assert(!1, "Unexpected: " + h);
    };
    b.untested = function(h) {
      b.warning("Congratulations, you've found a code path for which we haven't found a test case. Please submit the test case: " + h);
    };
  })(d.Debug || (d.Debug = {}));
  var k = d.Debug;
  d.getTicks = function() {
    return performance.now();
  };
  (function(b) {
    function h(f, b) {
      for (var n = 0, h = f.length;n < h;n++) {
        if (f[n] === b) {
          return n;
        }
      }
      f.push(b);
      return f.length - 1;
    }
    var a = d.Debug.assert;
    b.popManyInto = function(f, b, n) {
      a(f.length >= b);
      for (var h = b - 1;0 <= h;h--) {
        n[h] = f.pop();
      }
      n.length = b;
    };
    b.popMany = function(f, b) {
      a(f.length >= b);
      var n = f.length - b, h = f.slice(n, this.length);
      f.splice(n, b);
      return h;
    };
    b.popManyIntoVoid = function(f, b) {
      a(f.length >= b);
      f.length -= b;
    };
    b.pushMany = function(f, b) {
      for (var n = 0;n < b.length;n++) {
        f.push(b[n]);
      }
    };
    b.top = function(f) {
      return f.length && f[f.length - 1];
    };
    b.last = function(f) {
      return f.length && f[f.length - 1];
    };
    b.peek = function(f) {
      a(0 < f.length);
      return f[f.length - 1];
    };
    b.indexOf = function(f, b) {
      for (var n = 0, h = f.length;n < h;n++) {
        if (f[n] === b) {
          return n;
        }
      }
      return-1;
    };
    b.pushUnique = h;
    b.unique = function(f) {
      for (var b = [], n = 0;n < f.length;n++) {
        h(b, f[n]);
      }
      return b;
    };
    b.copyFrom = function(f, n) {
      f.length = 0;
      b.pushMany(f, n);
    };
    b.ensureTypedArrayCapacity = function(f, b) {
      if (f.length < b) {
        var n = f;
        f = new f.constructor(d.IntegerUtilities.nearestPowerOfTwo(b));
        f.set(n, 0);
      }
      return f;
    };
    var n = function() {
      function f(f) {
        "undefined" === typeof f && (f = 16);
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
        a(1 === f || 2 === f || 4 === f || 8 === f || 16 === f);
        f = this._offset / f;
        a((f | 0) === f);
        return f;
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
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(f);
      };
      f.prototype.writeIntAt = function(f, b) {
        a(0 <= b && b <= this._offset);
        a(0 === (b & 3));
        this.ensureCapacity(b + 4);
        this._i32[b >> 2] = f;
      };
      f.prototype.writeIntUnsafe = function(f) {
        this._i32[this._offset >> 2] = f;
        this._offset += 4;
      };
      f.prototype.writeFloat = function(f) {
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(f);
      };
      f.prototype.writeFloatUnsafe = function(f) {
        this._f32[this._offset >> 2] = f;
        this._offset += 4;
      };
      f.prototype.write4Floats = function(f, b, n, h) {
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(f, b, n, h);
      };
      f.prototype.write4FloatsUnsafe = function(f, b, n, h) {
        var a = this._offset >> 2;
        this._f32[a + 0] = f;
        this._f32[a + 1] = b;
        this._f32[a + 2] = n;
        this._f32[a + 3] = h;
        this._offset += 16;
      };
      f.prototype.write6Floats = function(f, b, n, h, c, g) {
        a(0 === (this._offset & 3));
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(f, b, n, h, c, g);
      };
      f.prototype.write6FloatsUnsafe = function(f, b, n, h, a, c) {
        var g = this._offset >> 2;
        this._f32[g + 0] = f;
        this._f32[g + 1] = b;
        this._f32[g + 2] = n;
        this._f32[g + 3] = h;
        this._f32[g + 4] = a;
        this._f32[g + 5] = c;
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
      f.prototype.hashWords = function(f, b, n) {
        b = this._i32;
        for (var h = 0;h < n;h++) {
          f = (31 * f | 0) + b[h] | 0;
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
    b.ArrayWriter = n;
  })(d.ArrayUtilities || (d.ArrayUtilities = {}));
  var a = d.ArrayUtilities, c = function() {
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
      k.assert(0 === (this._offset & 3));
      k.assert(this._offset <= this._u8.length - 4);
      var b = this._i32[this._offset >> 2];
      this._offset += 4;
      return b;
    };
    b.prototype.readFloat = function() {
      k.assert(0 === (this._offset & 3));
      k.assert(this._offset <= this._u8.length - 4);
      var b = this._f32[this._offset >> 2];
      this._offset += 4;
      return b;
    };
    return b;
  }();
  d.ArrayReader = c;
  (function(b) {
    function h(b, f) {
      return Object.prototype.hasOwnProperty.call(b, f);
    }
    function a(b, f) {
      for (var q in f) {
        h(f, q) && (b[q] = f[q]);
      }
    }
    b.boxValue = function(b) {
      return void 0 == b || s(b) ? b : Object(b);
    };
    b.toKeyValueArray = function(b) {
      var f = Object.prototype.hasOwnProperty, h = [], a;
      for (a in b) {
        f.call(b, a) && h.push([a, b[a]]);
      }
      return h;
    };
    b.isPrototypeWriteable = function(b) {
      return Object.getOwnPropertyDescriptor(b, "prototype").writable;
    };
    b.hasOwnProperty = h;
    b.propertyIsEnumerable = function(b, f) {
      return Object.prototype.propertyIsEnumerable.call(b, f);
    };
    b.getOwnPropertyDescriptor = function(b, f) {
      return Object.getOwnPropertyDescriptor(b, f);
    };
    b.hasOwnGetter = function(b, f) {
      var h = Object.getOwnPropertyDescriptor(b, f);
      return!(!h || !h.get);
    };
    b.getOwnGetter = function(b, f) {
      var h = Object.getOwnPropertyDescriptor(b, f);
      return h ? h.get : null;
    };
    b.hasOwnSetter = function(b, f) {
      var h = Object.getOwnPropertyDescriptor(b, f);
      return!(!h || !h.set);
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
    b.defineReadOnlyProperty = function(b, f, h) {
      Object.defineProperty(b, f, {value:h, writable:!1, configurable:!0, enumerable:!1});
    };
    b.getOwnPropertyDescriptors = function(h) {
      for (var f = b.createMap(), q = Object.getOwnPropertyNames(h), a = 0;a < q.length;a++) {
        f[q[a]] = Object.getOwnPropertyDescriptor(h, q[a]);
      }
      return f;
    };
    b.cloneObject = function(b) {
      var f = Object.create(Object.getPrototypeOf(b));
      a(f, b);
      return f;
    };
    b.copyProperties = function(b, f) {
      for (var h in f) {
        b[h] = f[h];
      }
    };
    b.copyOwnProperties = a;
    b.copyOwnPropertyDescriptors = function(b, f, q) {
      "undefined" === typeof q && (q = !0);
      for (var a in f) {
        if (h(f, a)) {
          var c = Object.getOwnPropertyDescriptor(f, a);
          if (q || !h(b, a)) {
            k.assert(c);
            try {
              Object.defineProperty(b, a, c);
            } catch (g) {
            }
          }
        }
      }
    };
    b.getLatestGetterOrSetterPropertyDescriptor = function(b, f) {
      for (var h = {};b;) {
        var a = Object.getOwnPropertyDescriptor(b, f);
        a && (h.get = h.get || a.get, h.set = h.set || a.set);
        if (h.get && h.set) {
          break;
        }
        b = Object.getPrototypeOf(b);
      }
      return h;
    };
    b.defineNonEnumerableGetterOrSetter = function(h, f, q, a) {
      var c = b.getLatestGetterOrSetterPropertyDescriptor(h, f);
      c.configurable = !0;
      c.enumerable = !1;
      a ? c.get = q : c.set = q;
      Object.defineProperty(h, f, c);
    };
    b.defineNonEnumerableGetter = function(b, f, h) {
      Object.defineProperty(b, f, {get:h, configurable:!0, enumerable:!1});
    };
    b.defineNonEnumerableSetter = function(b, f, h) {
      Object.defineProperty(b, f, {set:h, configurable:!0, enumerable:!1});
    };
    b.defineNonEnumerableProperty = function(b, f, h) {
      Object.defineProperty(b, f, {value:h, writable:!0, configurable:!0, enumerable:!1});
    };
    b.defineNonEnumerableForwardingProperty = function(b, f, h) {
      Object.defineProperty(b, f, {get:g.makeForwardingGetter(h), set:g.makeForwardingSetter(h), writable:!0, configurable:!0, enumerable:!1});
    };
    b.defineNewNonEnumerableProperty = function(h, f, a) {
      k.assert(!Object.prototype.hasOwnProperty.call(h, f), "Property: " + f + " already exits.");
      b.defineNonEnumerableProperty(h, f, a);
    };
  })(d.ObjectUtilities || (d.ObjectUtilities = {}));
  (function(b) {
    b.makeForwardingGetter = function(b) {
      return new Function('return this["' + b + '"]');
    };
    b.makeForwardingSetter = function(b) {
      return new Function("value", 'this["' + b + '"] = value;');
    };
    b.bindSafely = function(b, a) {
      k.assert(!b.boundTo && a);
      var n = b.bind(a);
      n.boundTo = a;
      return n;
    };
  })(d.FunctionUtilities || (d.FunctionUtilities = {}));
  var g = d.FunctionUtilities;
  (function(b) {
    function h(f) {
      return "string" === typeof f ? '"' + f + '"' : "number" === typeof f || "boolean" === typeof f ? String(f) : f instanceof Array ? "[] " + f.length : typeof f;
    }
    var a = d.Debug.assert;
    b.repeatString = function(f, b) {
      for (var h = "", n = 0;n < b;n++) {
        h += f;
      }
      return h;
    };
    b.memorySizeToString = function(f) {
      f |= 0;
      return 1024 > f ? f + " B" : 1048576 > f ? (f / 1024).toFixed(2) + "KB" : (f / 1048576).toFixed(2) + "MB";
    };
    b.toSafeString = h;
    b.toSafeArrayString = function(f) {
      for (var b = [], n = 0;n < f.length;n++) {
        b.push(h(f[n]));
      }
      return b.join(", ");
    };
    b.utf8decode = function(f) {
      for (var b = new Uint8Array(4 * f.length), h = 0, n = 0, a = f.length;n < a;n++) {
        var q = f.charCodeAt(n);
        if (127 >= q) {
          b[h++] = q;
        } else {
          if (55296 <= q && 56319 >= q) {
            var c = f.charCodeAt(n + 1);
            56320 <= c && 57343 >= c && (q = ((q & 1023) << 10) + (c & 1023) + 65536, ++n);
          }
          0 !== (q & 4292870144) ? (b[h++] = 248 | q >>> 24 & 3, b[h++] = 128 | q >>> 18 & 63, b[h++] = 128 | q >>> 12 & 63, b[h++] = 128 | q >>> 6 & 63) : 0 !== (q & 4294901760) ? (b[h++] = 240 | q >>> 18 & 7, b[h++] = 128 | q >>> 12 & 63, b[h++] = 128 | q >>> 6 & 63) : 0 !== (q & 4294965248) ? (b[h++] = 224 | q >>> 12 & 15, b[h++] = 128 | q >>> 6 & 63) : b[h++] = 192 | q >>> 6 & 31;
          b[h++] = 128 | q & 63;
        }
      }
      return b.subarray(0, h);
    };
    b.utf8encode = function(f) {
      for (var b = 0, h = "";b < f.length;) {
        var n = f[b++] & 255;
        if (127 >= n) {
          h += String.fromCharCode(n);
        } else {
          var q = 192, a = 5;
          do {
            if ((n & (q >> 1 | 128)) === q) {
              break;
            }
            q = q >> 1 | 128;
            --a;
          } while (0 <= a);
          if (0 >= a) {
            h += String.fromCharCode(n);
          } else {
            for (var n = n & (1 << a) - 1, q = !1, c = 5;c >= a;--c) {
              var g = f[b++];
              if (128 != (g & 192)) {
                q = !0;
                break;
              }
              n = n << 6 | g & 63;
            }
            if (q) {
              for (a = b - (7 - c);a < b;++a) {
                h += String.fromCharCode(f[a] & 255);
              }
            } else {
              h = 65536 <= n ? h + String.fromCharCode(n - 65536 >> 10 & 1023 | 55296, n & 1023 | 56320) : h + String.fromCharCode(n);
            }
          }
        }
      }
      return h;
    };
    b.base64ArrayBuffer = function(f) {
      var b = "";
      f = new Uint8Array(f);
      for (var h = f.byteLength, n = h % 3, h = h - n, q, a, c, g, p = 0;p < h;p += 3) {
        g = f[p] << 16 | f[p + 1] << 8 | f[p + 2], q = (g & 16515072) >> 18, a = (g & 258048) >> 12, c = (g & 4032) >> 6, g &= 63, b += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[q] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[a] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[c] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[g];
      }
      1 == n ? (g = f[h], b += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(g & 252) >> 2] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(g & 3) << 4] + "==") : 2 == n && (g = f[h] << 8 | f[h + 1], b += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(g & 64512) >> 10] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(g & 1008) >> 4] + "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(g & 15) << 
      2] + "=");
      return b;
    };
    b.escapeString = function(f) {
      void 0 !== f && (f = f.replace(/[^\w$]/gi, "$"), /^\d/.test(f) && (f = "$" + f));
      return f;
    };
    b.fromCharCodeArray = function(f) {
      for (var b = "", h = 0;h < f.length;h += 16384) {
        var n = Math.min(f.length - h, 16384), b = b + String.fromCharCode.apply(null, f.subarray(h, h + n))
      }
      return b;
    };
    b.variableLengthEncodeInt32 = function(f) {
      var h = 32 - Math.clz32(f);
      a(32 >= h, h);
      for (var n = Math.ceil(h / 6), q = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[n], c = n - 1;0 <= c;c--) {
        q += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[f >> 6 * c & 63];
      }
      a(b.variableLengthDecodeInt32(q) === f, f + " : " + q + " - " + n + " bits: " + h);
      return q;
    };
    b.toEncoding = function(f) {
      return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_"[f];
    };
    b.fromEncoding = function(f) {
      f = f.charCodeAt(0);
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
      a(!1, "Invalid Encoding");
    };
    b.variableLengthDecodeInt32 = function(f) {
      for (var h = b.fromEncoding(f[0]), n = 0, q = 0;q < h;q++) {
        var a = 6 * (h - q - 1), n = n | b.fromEncoding(f[1 + q]) << a
      }
      return n;
    };
    b.trimMiddle = function(f, b) {
      if (f.length <= b) {
        return f;
      }
      var h = b >> 1, n = b - h - 1;
      return f.substr(0, h) + "\u2026" + f.substr(f.length - n, n);
    };
    b.multiple = function(f, b) {
      for (var h = "", n = 0;n < b;n++) {
        h += f;
      }
      return h;
    };
    b.indexOfAny = function(f, b, h) {
      for (var n = f.length, q = 0;q < b.length;q++) {
        var a = f.indexOf(b[q], h);
        0 <= a && (n = Math.min(n, a));
      }
      return n === f.length ? -1 : n;
    };
    var n = Array(3), f = Array(4), q = Array(5), c = Array(6), g = Array(7), p = Array(8), r = Array(9);
    b.concat3 = function(f, b, h) {
      n[0] = f;
      n[1] = b;
      n[2] = h;
      return n.join("");
    };
    b.concat4 = function(b, h, n, q) {
      f[0] = b;
      f[1] = h;
      f[2] = n;
      f[3] = q;
      return f.join("");
    };
    b.concat5 = function(f, b, h, n, a) {
      q[0] = f;
      q[1] = b;
      q[2] = h;
      q[3] = n;
      q[4] = a;
      return q.join("");
    };
    b.concat6 = function(f, b, h, n, q, a) {
      c[0] = f;
      c[1] = b;
      c[2] = h;
      c[3] = n;
      c[4] = q;
      c[5] = a;
      return c.join("");
    };
    b.concat7 = function(f, b, h, n, q, a, c) {
      g[0] = f;
      g[1] = b;
      g[2] = h;
      g[3] = n;
      g[4] = q;
      g[5] = a;
      g[6] = c;
      return g.join("");
    };
    b.concat8 = function(f, b, h, n, q, a, c, g) {
      p[0] = f;
      p[1] = b;
      p[2] = h;
      p[3] = n;
      p[4] = q;
      p[5] = a;
      p[6] = c;
      p[7] = g;
      return p.join("");
    };
    b.concat9 = function(f, b, h, n, q, a, c, g, p) {
      r[0] = f;
      r[1] = b;
      r[2] = h;
      r[3] = n;
      r[4] = q;
      r[5] = a;
      r[6] = c;
      r[7] = g;
      r[8] = p;
      return r.join("");
    };
  })(d.StringUtilities || (d.StringUtilities = {}));
  (function(b) {
    var h = new Uint8Array([7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]), a = new Int32Array([-680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426, -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162, 1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632, 
    643717713, -373897302, -701558691, 38016083, -660478335, -405537848, 568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784, 1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556, -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222, -722521979, 76029189, -640364487, -421815835, 530742520, -995338651, -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606, -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649, 
    -145523070, -1120210379, 718787259, -343485551]);
    b.hashBytesTo32BitsMD5 = function(b, f, q) {
      var c = 1732584193, g = -271733879, p = -1732584194, r = 271733878, l = q + 72 & -64, k = new Uint8Array(l), v;
      for (v = 0;v < q;++v) {
        k[v] = b[f++];
      }
      k[v++] = 128;
      for (b = l - 8;v < b;) {
        k[v++] = 0;
      }
      k[v++] = q << 3 & 255;
      k[v++] = q >> 5 & 255;
      k[v++] = q >> 13 & 255;
      k[v++] = q >> 21 & 255;
      k[v++] = q >>> 29 & 255;
      k[v++] = 0;
      k[v++] = 0;
      k[v++] = 0;
      b = new Int32Array(16);
      for (v = 0;v < l;) {
        for (q = 0;16 > q;++q, v += 4) {
          b[q] = k[v] | k[v + 1] << 8 | k[v + 2] << 16 | k[v + 3] << 24;
        }
        var e = c;
        f = g;
        var u = p, d = r, t, m;
        for (q = 0;64 > q;++q) {
          16 > q ? (t = f & u | ~f & d, m = q) : 32 > q ? (t = d & f | ~d & u, m = 5 * q + 1 & 15) : 48 > q ? (t = f ^ u ^ d, m = 3 * q + 5 & 15) : (t = u ^ (f | ~d), m = 7 * q & 15);
          var s = d, e = e + t + a[q] + b[m] | 0;
          t = h[q];
          d = u;
          u = f;
          f = f + (e << t | e >>> 32 - t) | 0;
          e = s;
        }
        c = c + e | 0;
        g = g + f | 0;
        p = p + u | 0;
        r = r + d | 0;
      }
      return c;
    };
    b.hashBytesTo32BitsAdler = function(b, f, h) {
      var a = 1, c = 0;
      for (h = f + h;f < h;++f) {
        a = (a + (b[f] & 255)) % 65521, c = (c + a) % 65521;
      }
      return c << 16 | a;
    };
  })(d.HashUtilities || (d.HashUtilities = {}));
  var p = function() {
    function b() {
    }
    b.seed = function(h) {
      b._state[0] = h;
      b._state[1] = h;
    };
    b.next = function() {
      var b = this._state, a = Math.imul(18273, b[0] & 65535) + (b[0] >>> 16) | 0;
      b[0] = a;
      var n = Math.imul(36969, b[1] & 65535) + (b[1] >>> 16) | 0;
      b[1] = n;
      b = (a << 16) + (n & 65535) | 0;
      return 2.3283064365386963E-10 * (0 > b ? b + 4294967296 : b);
    };
    b._state = new Uint32Array([57005, 48879]);
    return b;
  }();
  d.Random = p;
  Math.random = function() {
    return p.next();
  };
  (function() {
    function b() {
      this.id = "$weakmap" + h++;
    }
    if ("function" !== typeof jsGlobal.WeakMap) {
      var h = 0;
      b.prototype = {has:function(b) {
        return b.hasOwnProperty(this.id);
      }, get:function(b, h) {
        return b.hasOwnProperty(this.id) ? b[this.id] : h;
      }, set:function(b, h) {
        Object.defineProperty(b, this.id, {value:h, enumerable:!1, configurable:!0});
      }};
      jsGlobal.WeakMap = b;
    }
  })();
  c = function() {
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
        "undefined" !== typeof netscape && netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect"), Components.utils.nondeterministicGetWeakMapKeys(this._map).forEach(function(f) {
          0 !== f._referenceCount && b(f);
        });
      } else {
        for (var a = this._list, n = 0, f = 0;f < a.length;f++) {
          var q = a[f];
          0 === q._referenceCount ? n++ : b(q);
        }
        if (16 < n && n > a.length >> 2) {
          n = [];
          for (f = 0;f < a.length;f++) {
            0 < a[f]._referenceCount && n.push(a[f]);
          }
          this._list = n;
        }
      }
    };
    Object.defineProperty(b.prototype, "length", {get:function() {
      return this._map ? -1 : this._list.length;
    }, enumerable:!0, configurable:!0});
    return b;
  }();
  d.WeakList = c;
  (function(b) {
    b.pow2 = function(b) {
      return b === (b | 0) ? 0 > b ? 1 / (1 << -b) : 1 << b : Math.pow(2, b);
    };
    b.clamp = function(b, a, n) {
      return Math.max(a, Math.min(n, b));
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
  })(d.NumberUtilities || (d.NumberUtilities = {}));
  (function(b) {
    b[b.MaxU16 = 65535] = "MaxU16";
    b[b.MaxI16 = 32767] = "MaxI16";
    b[b.MinI16 = -32768] = "MinI16";
  })(d.Numbers || (d.Numbers = {}));
  (function(b) {
    function h(b) {
      return 256 * b << 16 >> 16;
    }
    var a = new ArrayBuffer(8);
    b.i8 = new Int8Array(a);
    b.u8 = new Uint8Array(a);
    b.i32 = new Int32Array(a);
    b.f32 = new Float32Array(a);
    b.f64 = new Float64Array(a);
    b.nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
    b.floatToInt32 = function(h) {
      b.f32[0] = h;
      return b.i32[0];
    };
    b.int32ToFloat = function(h) {
      b.i32[0] = h;
      return b.f32[0];
    };
    b.swap16 = function(b) {
      return(b & 255) << 8 | b >> 8 & 255;
    };
    b.swap32 = function(b) {
      return(b & 255) << 24 | (b & 65280) << 8 | b >> 8 & 65280 | b >> 24 & 255;
    };
    b.toS8U8 = h;
    b.fromS8U8 = function(b) {
      return b / 256;
    };
    b.clampS8U8 = function(b) {
      return h(b) / 256;
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
    b.trailingZeros = function(h) {
      return b.ones((h & -h) - 1);
    };
    b.getFlags = function(b, f) {
      var h = "";
      for (b = 0;b < f.length;b++) {
        b & 1 << b && (h += f[b] + " ");
      }
      return 0 === h.length ? "" : h.trim();
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
    b.roundToMultipleOfPowerOfTwo = function(b, f) {
      var h = (1 << f) - 1;
      return b + h & ~h;
    };
    Math.imul || (Math.imul = function(b, f) {
      var h = b & 65535, a = f & 65535;
      return h * a + ((b >>> 16 & 65535) * a + h * (f >>> 16 & 65535) << 16 >>> 0) | 0;
    });
    Math.clz32 || (Math.clz32 = function(h) {
      h |= h >> 1;
      h |= h >> 2;
      h |= h >> 4;
      h |= h >> 8;
      return 32 - b.ones(h | h >> 16);
    });
  })(d.IntegerUtilities || (d.IntegerUtilities = {}));
  var v = d.IntegerUtilities;
  (function(b) {
    function h(b, h, f, a, c, g) {
      return(f - b) * (g - h) - (a - h) * (c - b);
    }
    b.pointInPolygon = function(b, h, f) {
      for (var a = 0, c = f.length - 2, g = 0;g < c;g += 2) {
        var p = f[g + 0], r = f[g + 1], l = f[g + 2], k = f[g + 3];
        (r <= h && k > h || r > h && k <= h) && b < p + (h - r) / (k - r) * (l - p) && a++;
      }
      return 1 === (a & 1);
    };
    b.signedArea = h;
    b.counterClockwise = function(b, a, f, q, c, g) {
      return 0 < h(b, a, f, q, c, g);
    };
    b.clockwise = function(b, a, f, q, c, g) {
      return 0 > h(b, a, f, q, c, g);
    };
    b.pointInPolygonInt32 = function(b, h, f) {
      b |= 0;
      h |= 0;
      for (var a = 0, c = f.length - 2, g = 0;g < c;g += 2) {
        var p = f[g + 0], r = f[g + 1], l = f[g + 2], k = f[g + 3];
        (r <= h && k > h || r > h && k <= h) && b < p + (h - r) / (k - r) * (l - p) && a++;
      }
      return 1 === (a & 1);
    };
  })(d.GeometricUtilities || (d.GeometricUtilities = {}));
  (function(b) {
    b[b.Error = 1] = "Error";
    b[b.Warn = 2] = "Warn";
    b[b.Debug = 4] = "Debug";
    b[b.Log = 8] = "Log";
    b[b.Info = 16] = "Info";
    b[b.All = 31] = "All";
  })(d.LogLevel || (d.LogLevel = {}));
  c = function() {
    function b(h, a) {
      "undefined" === typeof h && (h = !1);
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = h;
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
    b.prototype.errorLn = function(h) {
      b.logLevel & 1 && this.boldRedLn(h);
    };
    b.prototype.warnLn = function(h) {
      b.logLevel & 2 && this.yellowLn(h);
    };
    b.prototype.debugLn = function(h) {
      b.logLevel & 4 && this.purpleLn(h);
    };
    b.prototype.logLn = function(h) {
      b.logLevel & 8 && this.writeLn(h);
    };
    b.prototype.infoLn = function(h) {
      b.logLevel & 16 && this.writeLn(h);
    };
    b.prototype.yellowLn = function(h) {
      this.colorLn(b.YELLOW, h);
    };
    b.prototype.greenLn = function(h) {
      this.colorLn(b.GREEN, h);
    };
    b.prototype.boldRedLn = function(h) {
      this.colorLn(b.BOLD_RED, h);
    };
    b.prototype.redLn = function(h) {
      this.colorLn(b.RED, h);
    };
    b.prototype.purpleLn = function(h) {
      this.colorLn(b.PURPLE, h);
    };
    b.prototype.colorLn = function(h, a) {
      this._suppressOutput || (inBrowser ? this._out(this._padding + a) : this._out(this._padding + h + a + b.ENDC));
    };
    b.prototype.redLns = function(h) {
      this.colorLns(b.RED, h);
    };
    b.prototype.colorLns = function(b, a) {
      for (var n = a.split("\n"), f = 0;f < n.length;f++) {
        this.colorLn(b, n[f]);
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
    b.prototype.writeArray = function(b, a, n) {
      "undefined" === typeof a && (a = !1);
      "undefined" === typeof n && (n = !1);
      a = a || !1;
      for (var f = 0, q = b.length;f < q;f++) {
        var c = "";
        a && (c = null === b[f] ? "null" : void 0 === b[f] ? "undefined" : b[f].constructor.name, c += " ");
        var g = n ? "" : ("" + f).padRight(" ", 4);
        this.writeLn(g + c + b[f]);
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
  d.IndentingWriter = c;
  var u = function() {
    return function(b, h) {
      this.value = b;
      this.next = h;
    };
  }(), c = function() {
    function b(b) {
      k.assert(b);
      this._compare = b;
      this._head = null;
      this._length = 0;
    }
    b.prototype.push = function(b) {
      k.assert(void 0 !== b);
      this._length++;
      if (this._head) {
        var a = this._head, n = null;
        b = new u(b, null);
        for (var f = this._compare;a;) {
          if (0 < f(a.value, b.value)) {
            n ? (b.next = a, n.next = b) : (b.next = this._head, this._head = b);
            return;
          }
          n = a;
          a = a.next;
        }
        n.next = b;
      } else {
        this._head = new u(b, null);
      }
    };
    b.prototype.forEach = function(h) {
      for (var a = this._head, n = null;a;) {
        var f = h(a.value);
        if (f === b.RETURN) {
          break;
        } else {
          f === b.DELETE ? a = n ? n.next = a.next : this._head = this._head.next : (n = a, a = a.next);
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
  d.SortedList = c;
  c = function() {
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
        for (var a = 0 === this.index ? this._size - 1 : this.index - 1, n = this.start - 1 & this._mask;a !== n && !b(this.array[a], a);) {
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
  d.CircularBuffer = c;
  (function(b) {
    function h(f) {
      return f + (b.BITS_PER_WORD - 1) >> b.ADDRESS_BITS_PER_WORD << b.ADDRESS_BITS_PER_WORD;
    }
    function a(f, b) {
      f = f || "1";
      b = b || "0";
      for (var h = "", q = 0;q < length;q++) {
        h += this.get(q) ? f : b;
      }
      return h;
    }
    function n(f) {
      for (var b = [], h = 0;h < length;h++) {
        this.get(h) && b.push(f ? f[h] : h);
      }
      return b.join(", ");
    }
    var f = d.Debug.assert;
    b.ADDRESS_BITS_PER_WORD = 5;
    b.BITS_PER_WORD = 1 << b.ADDRESS_BITS_PER_WORD;
    b.BIT_INDEX_MASK = b.BITS_PER_WORD - 1;
    var q = function() {
      function a(f) {
        this.size = h(f);
        this.dirty = this.count = 0;
        this.length = f;
        this.bits = new Uint32Array(this.size >> b.ADDRESS_BITS_PER_WORD);
      }
      a.prototype.recount = function() {
        if (this.dirty) {
          for (var f = this.bits, b = 0, h = 0, a = f.length;h < a;h++) {
            var q = f[h], q = q - (q >> 1 & 1431655765), q = (q & 858993459) + (q >> 2 & 858993459), b = b + (16843009 * (q + (q >> 4) & 252645135) >> 24)
          }
          this.count = b;
          this.dirty = 0;
        }
      };
      a.prototype.set = function(f) {
        var h = f >> b.ADDRESS_BITS_PER_WORD, a = this.bits[h];
        f = a | 1 << (f & b.BIT_INDEX_MASK);
        this.bits[h] = f;
        this.dirty |= a ^ f;
      };
      a.prototype.setAll = function() {
        for (var f = this.bits, b = 0, h = f.length;b < h;b++) {
          f[b] = 4294967295;
        }
        this.count = this.size;
        this.dirty = 0;
      };
      a.prototype.assign = function(f) {
        this.count = f.count;
        this.dirty = f.dirty;
        this.size = f.size;
        for (var b = 0, h = this.bits.length;b < h;b++) {
          this.bits[b] = f.bits[b];
        }
      };
      a.prototype.clear = function(f) {
        var h = f >> b.ADDRESS_BITS_PER_WORD, a = this.bits[h];
        f = a & ~(1 << (f & b.BIT_INDEX_MASK));
        this.bits[h] = f;
        this.dirty |= a ^ f;
      };
      a.prototype.get = function(f) {
        return 0 !== (this.bits[f >> b.ADDRESS_BITS_PER_WORD] & 1 << (f & b.BIT_INDEX_MASK));
      };
      a.prototype.clearAll = function() {
        for (var f = this.bits, b = 0, h = f.length;b < h;b++) {
          f[b] = 0;
        }
        this.dirty = this.count = 0;
      };
      a.prototype._union = function(f) {
        var b = this.dirty, h = this.bits;
        f = f.bits;
        for (var a = 0, q = h.length;a < q;a++) {
          var n = h[a], c = n | f[a];
          h[a] = c;
          b |= n ^ c;
        }
        this.dirty = b;
      };
      a.prototype.intersect = function(f) {
        var b = this.dirty, h = this.bits;
        f = f.bits;
        for (var a = 0, q = h.length;a < q;a++) {
          var n = h[a], c = n & f[a];
          h[a] = c;
          b |= n ^ c;
        }
        this.dirty = b;
      };
      a.prototype.subtract = function(f) {
        var b = this.dirty, h = this.bits;
        f = f.bits;
        for (var a = 0, q = h.length;a < q;a++) {
          var n = h[a], c = n & ~f[a];
          h[a] = c;
          b |= n ^ c;
        }
        this.dirty = b;
      };
      a.prototype.negate = function() {
        for (var f = this.dirty, b = this.bits, h = 0, a = b.length;h < a;h++) {
          var q = b[h], n = ~q;
          b[h] = n;
          f |= q ^ n;
        }
        this.dirty = f;
      };
      a.prototype.forEach = function(h) {
        f(h);
        for (var a = this.bits, q = 0, n = a.length;q < n;q++) {
          var c = a[q];
          if (c) {
            for (var g = 0;g < b.BITS_PER_WORD;g++) {
              c & 1 << g && h(q * b.BITS_PER_WORD + g);
            }
          }
        }
      };
      a.prototype.toArray = function() {
        for (var f = [], h = this.bits, a = 0, q = h.length;a < q;a++) {
          var n = h[a];
          if (n) {
            for (var c = 0;c < b.BITS_PER_WORD;c++) {
              n & 1 << c && f.push(a * b.BITS_PER_WORD + c);
            }
          }
        }
        return f;
      };
      a.prototype.equals = function(f) {
        if (this.size !== f.size) {
          return!1;
        }
        var b = this.bits;
        f = f.bits;
        for (var h = 0, a = b.length;h < a;h++) {
          if (b[h] !== f[h]) {
            return!1;
          }
        }
        return!0;
      };
      a.prototype.contains = function(f) {
        if (this.size !== f.size) {
          return!1;
        }
        var b = this.bits;
        f = f.bits;
        for (var h = 0, a = b.length;h < a;h++) {
          if ((b[h] | f[h]) !== b[h]) {
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
        var f = new a(this.length);
        f._union(this);
        return f;
      };
      return a;
    }();
    b.Uint32ArrayBitSet = q;
    var c = function() {
      function a(f) {
        this.dirty = this.count = 0;
        this.size = h(f);
        this.bits = 0;
        this.singleWord = !0;
        this.length = f;
      }
      a.prototype.recount = function() {
        if (this.dirty) {
          var f = this.bits, f = f - (f >> 1 & 1431655765), f = (f & 858993459) + (f >> 2 & 858993459);
          this.count = 0 + (16843009 * (f + (f >> 4) & 252645135) >> 24);
          this.dirty = 0;
        }
      };
      a.prototype.set = function(f) {
        var a = this.bits;
        this.bits = f = a | 1 << (f & b.BIT_INDEX_MASK);
        this.dirty |= a ^ f;
      };
      a.prototype.setAll = function() {
        this.bits = 4294967295;
        this.count = this.size;
        this.dirty = 0;
      };
      a.prototype.assign = function(f) {
        this.count = f.count;
        this.dirty = f.dirty;
        this.size = f.size;
        this.bits = f.bits;
      };
      a.prototype.clear = function(f) {
        var a = this.bits;
        this.bits = f = a & ~(1 << (f & b.BIT_INDEX_MASK));
        this.dirty |= a ^ f;
      };
      a.prototype.get = function(f) {
        return 0 !== (this.bits & 1 << (f & b.BIT_INDEX_MASK));
      };
      a.prototype.clearAll = function() {
        this.dirty = this.count = this.bits = 0;
      };
      a.prototype._union = function(f) {
        var b = this.bits;
        this.bits = f = b | f.bits;
        this.dirty = b ^ f;
      };
      a.prototype.intersect = function(f) {
        var b = this.bits;
        this.bits = f = b & f.bits;
        this.dirty = b ^ f;
      };
      a.prototype.subtract = function(f) {
        var b = this.bits;
        this.bits = f = b & ~f.bits;
        this.dirty = b ^ f;
      };
      a.prototype.negate = function() {
        var f = this.bits, b = ~f;
        this.bits = b;
        this.dirty = f ^ b;
      };
      a.prototype.forEach = function(a) {
        f(a);
        var h = this.bits;
        if (h) {
          for (var q = 0;q < b.BITS_PER_WORD;q++) {
            h & 1 << q && a(q);
          }
        }
      };
      a.prototype.toArray = function() {
        var f = [], a = this.bits;
        if (a) {
          for (var h = 0;h < b.BITS_PER_WORD;h++) {
            a & 1 << h && f.push(h);
          }
        }
        return f;
      };
      a.prototype.equals = function(f) {
        return this.bits === f.bits;
      };
      a.prototype.contains = function(f) {
        var b = this.bits;
        return(b | f.bits) === b;
      };
      a.prototype.isEmpty = function() {
        this.recount();
        return 0 === this.count;
      };
      a.prototype.clone = function() {
        var f = new a(this.length);
        f._union(this);
        return f;
      };
      return a;
    }();
    b.Uint32BitSet = c;
    c.prototype.toString = n;
    c.prototype.toBitString = a;
    q.prototype.toString = n;
    q.prototype.toBitString = a;
    b.BitSetFunctor = function(f) {
      var a = 1 === h(f) >> b.ADDRESS_BITS_PER_WORD ? c : q;
      return function() {
        return new a(f);
      };
    };
  })(d.BitSets || (d.BitSets = {}));
  c = function() {
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
  d.ColorStyle = c;
  c = function() {
    function b(b, a, n, f) {
      this.xMin = b | 0;
      this.yMin = a | 0;
      this.xMax = n | 0;
      this.yMax = f | 0;
    }
    b.FromUntyped = function(a) {
      return new b(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    b.FromRectangle = function(a) {
      return new b(20 * a.x | 0, 20 * a.y | 0, 20 * (a.x + a.width) | 0, 20 * (a.y + a.height) | 0);
    };
    b.prototype.setElements = function(b, a, n, f) {
      this.xMin = b;
      this.yMin = a;
      this.xMax = n;
      this.yMax = f;
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
  d.Bounds = c;
  c = function() {
    function b(b, a, n, f) {
      k.assert(m(b));
      k.assert(m(a));
      k.assert(m(n));
      k.assert(m(f));
      this._xMin = b | 0;
      this._yMin = a | 0;
      this._xMax = n | 0;
      this._yMax = f | 0;
    }
    b.FromUntyped = function(a) {
      return new b(a.xMin, a.yMin, a.xMax, a.yMax);
    };
    b.FromRectangle = function(a) {
      return new b(20 * a.x | 0, 20 * a.y | 0, 20 * (a.x + a.width) | 0, 20 * (a.y + a.height) | 0);
    };
    b.prototype.setElements = function(b, a, n, f) {
      this.xMin = b;
      this.yMin = a;
      this.xMax = n;
      this.yMax = f;
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
      k.assert(m(b));
      this._xMin = b;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "yMin", {get:function() {
      return this._yMin;
    }, set:function(b) {
      k.assert(m(b));
      this._yMin = b | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "xMax", {get:function() {
      return this._xMax;
    }, set:function(b) {
      k.assert(m(b));
      this._xMax = b | 0;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "width", {get:function() {
      return this._xMax - this._xMin;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(b.prototype, "yMax", {get:function() {
      return this._yMax;
    }, set:function(b) {
      k.assert(m(b));
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
  d.DebugBounds = c;
  c = function() {
    function b(b, a, n, f) {
      this.r = b;
      this.g = a;
      this.b = n;
      this.a = f;
    }
    b.FromARGB = function(a) {
      return new b((a >> 16 & 255) / 255, (a >> 8 & 255) / 255, (a >> 0 & 255) / 255, (a >> 24 & 255) / 255);
    };
    b.FromRGBA = function(a) {
      return b.FromARGB(l.RGBAToARGB(a));
    };
    b.prototype.toRGBA = function() {
      return 255 * this.r << 24 | 255 * this.g << 16 | 255 * this.b << 8 | 255 * this.a;
    };
    b.prototype.toCSSStyle = function() {
      return l.rgbaToCSSStyle(this.toRGBA());
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
      var c = document.createElement("span");
      document.body.appendChild(c);
      c.style.backgroundColor = a;
      var n = getComputedStyle(c).backgroundColor;
      document.body.removeChild(c);
      (c = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(n)) || (c = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(n));
      n = new b(0, 0, 0, 0);
      n.r = parseFloat(c[1]) / 255;
      n.g = parseFloat(c[2]) / 255;
      n.b = parseFloat(c[3]) / 255;
      n.a = c[4] ? parseFloat(c[4]) / 255 : 1;
      return b.colorCache[a] = n;
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
  d.Color = c;
  (function(b) {
    function a(f) {
      var b, h, n = f >> 24 & 255;
      h = (Math.imul(f >> 16 & 255, n) + 127) / 255 | 0;
      b = (Math.imul(f >> 8 & 255, n) + 127) / 255 | 0;
      f = (Math.imul(f >> 0 & 255, n) + 127) / 255 | 0;
      return n << 24 | h << 16 | b << 8 | f;
    }
    b.RGBAToARGB = function(f) {
      return f >> 8 & 16777215 | (f & 255) << 24;
    };
    b.ARGBToRGBA = function(f) {
      return f << 8 | f >> 24 & 255;
    };
    b.rgbaToCSSStyle = function(f) {
      return d.StringUtilities.concat9("rgba(", f >> 24 & 255, ",", f >> 16 & 255, ",", f >> 8 & 255, ",", (f & 255) / 255, ")");
    };
    b.cssStyleToRGBA = function(f) {
      if ("#" === f[0]) {
        if (7 === f.length) {
          return parseInt(f.substring(1), 16) << 8 | 255;
        }
      } else {
        if ("r" === f[0]) {
          return f = f.substring(5, f.length - 1).split(","), (parseInt(f[0]) & 255) << 24 | (parseInt(f[1]) & 255) << 16 | (parseInt(f[2]) & 255) << 8 | 255 * parseFloat(f[3]) & 255;
        }
      }
      return 4278190335;
    };
    b.hexToRGB = function(f) {
      return parseInt(f.slice(1), 16);
    };
    b.rgbToHex = function(f) {
      return "#" + ("000000" + f.toString(16)).slice(-6);
    };
    b.isValidHexColor = function(f) {
      return/^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(f);
    };
    b.clampByte = function(f) {
      return Math.max(0, Math.min(255, f));
    };
    b.unpremultiplyARGB = function(f) {
      var b, a, h = f >> 24 & 255;
      a = Math.imul(255, f >> 16 & 255) / h & 255;
      b = Math.imul(255, f >> 8 & 255) / h & 255;
      f = Math.imul(255, f >> 0 & 255) / h & 255;
      return h << 24 | a << 16 | b << 8 | f;
    };
    b.premultiplyARGB = a;
    var c;
    b.ensureUnpremultiplyTable = function() {
      if (!c) {
        c = new Uint8Array(65536);
        for (var f = 0;256 > f;f++) {
          for (var b = 0;256 > b;b++) {
            c[(b << 8) + f] = Math.imul(255, f) / b;
          }
        }
      }
    };
    b.tableLookupUnpremultiplyARGB = function(f) {
      f |= 0;
      var b = f >> 24 & 255;
      if (0 === b) {
        return 0;
      }
      if (255 === b) {
        return f;
      }
      var a, h, n = b << 8, g = c;
      h = g[n + (f >> 16 & 255)];
      a = g[n + (f >> 8 & 255)];
      f = g[n + (f >> 0 & 255)];
      return b << 24 | h << 16 | a << 8 | f;
    };
    b.blendPremultipliedBGRA = function(f, b) {
      var a, h;
      h = 256 - (b & 255);
      a = Math.imul(f & 16711935, h) >> 8;
      h = Math.imul(f >> 8 & 16711935, h) >> 8;
      return((b >> 8 & 16711935) + h & 16711935) << 8 | (b & 16711935) + a & 16711935;
    };
    var n = v.swap32;
    b.convertImage = function(f, b, g, p) {
      g !== p && k.assert(g.buffer !== p.buffer, "Can't handle overlapping views.");
      var l = g.length;
      if (f === b) {
        if (g !== p) {
          for (f = 0;f < l;f++) {
            p[f] = g[f];
          }
        }
      } else {
        if (1 === f && 3 === b) {
          for (d.ColorUtilities.ensureUnpremultiplyTable(), f = 0;f < l;f++) {
            var v = g[f];
            b = v & 255;
            if (0 === b) {
              p[f] = 0;
            } else {
              if (255 === b) {
                p[f] = (v & 255) << 24 | v >> 8 & 16777215;
              } else {
                var e = v >> 24 & 255, u = v >> 16 & 255, v = v >> 8 & 255, t = b << 8, m = c, v = m[t + v], u = m[t + u], e = m[t + e];
                p[f] = b << 24 | e << 16 | u << 8 | v;
              }
            }
          }
        } else {
          if (2 === f && 3 === b) {
            for (f = 0;f < l;f++) {
              p[f] = n(g[f]);
            }
          } else {
            if (3 === f && 1 === b) {
              for (f = 0;f < l;f++) {
                b = g[f], p[f] = n(a(b & 4278255360 | b >> 16 & 255 | (b & 255) << 16));
              }
            } else {
              for (k.somewhatImplemented("Image Format Conversion: " + r[f] + " -> " + r[b]), f = 0;f < l;f++) {
                p[f] = g[f];
              }
            }
          }
        }
      }
    };
  })(d.ColorUtilities || (d.ColorUtilities = {}));
  var l = d.ColorUtilities, c = function() {
    function b(b) {
      "undefined" === typeof b && (b = 32);
      this._list = [];
      this._maxSize = b;
    }
    b.prototype.acquire = function(a) {
      if (b._enabled) {
        for (var c = this._list, n = 0;n < c.length;n++) {
          var f = c[n];
          if (f.byteLength >= a) {
            return c.splice(n, 1), f;
          }
        }
      }
      return new ArrayBuffer(a);
    };
    b.prototype.release = function(h) {
      if (b._enabled) {
        var c = this._list;
        k.assert(0 > a.indexOf(c, h));
        c.length === this._maxSize && c.shift();
        c.push(h);
      }
    };
    b.prototype.ensureUint8ArrayLength = function(b, a) {
      if (b.length >= a) {
        return b;
      }
      var c = Math.max(b.length + a, (3 * b.length >> 1) + 1), c = new Uint8Array(this.acquire(c), 0, c);
      c.set(b);
      this.release(b.buffer);
      return c;
    };
    b.prototype.ensureFloat64ArrayLength = function(b, a) {
      if (b.length >= a) {
        return b;
      }
      var c = Math.max(b.length + a, (3 * b.length >> 1) + 1), c = new Float64Array(this.acquire(c * Float64Array.BYTES_PER_ELEMENT), 0, c);
      c.set(b);
      this.release(b.buffer);
      return c;
    };
    b._enabled = !0;
    return b;
  }();
  d.ArrayBufferPool = c;
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
  })(d.Telemetry || (d.Telemetry = {}));
  (function(b) {
    b.instance;
  })(d.FileLoadingService || (d.FileLoadingService = {}));
  (function(b) {
    b.instance = {enabled:!1, initJS:function(b) {
    }, registerCallback:function(b) {
    }, unregisterCallback:function(b) {
    }, eval:function(b) {
    }, call:function(b) {
    }, getId:function() {
      return null;
    }};
  })(d.ExternalInterfaceService || (d.ExternalInterfaceService = {}));
  c = function() {
    function b() {
    }
    b.prototype.setClipboard = function(b) {
      k.abstractMethod("public ClipboardService::setClipboard");
    };
    b.instance = null;
    return b;
  }();
  d.ClipboardService = c;
  c = function() {
    function b() {
      this._queues = {};
    }
    b.prototype.register = function(b, a) {
      k.assert(b);
      k.assert(a);
      var c = this._queues[b];
      if (c) {
        if (-1 < c.indexOf(a)) {
          return;
        }
      } else {
        c = this._queues[b] = [];
      }
      c.push(a);
    };
    b.prototype.unregister = function(b, a) {
      k.assert(b);
      k.assert(a);
      var c = this._queues[b];
      if (c) {
        var f = c.indexOf(a);
        -1 !== f && c.splice(f, 1);
        0 === c.length && (this._queues[b] = null);
      }
    };
    b.prototype.notify = function(b, a) {
      var c = this._queues[b];
      if (c) {
        c = c.slice();
        a = Array.prototype.slice.call(arguments, 0);
        for (var f = 0;f < c.length;f++) {
          c[f].apply(null, a);
        }
      }
    };
    b.prototype.notify1 = function(b, a) {
      var c = this._queues[b];
      if (c) {
        for (var c = c.slice(), f = 0;f < c.length;f++) {
          (0,c[f])(b, a);
        }
      }
    };
    return b;
  }();
  d.Callback = c;
  (function(b) {
    b[b.None = 0] = "None";
    b[b.PremultipliedAlphaARGB = 1] = "PremultipliedAlphaARGB";
    b[b.StraightAlphaARGB = 2] = "StraightAlphaARGB";
    b[b.StraightAlphaRGBA = 3] = "StraightAlphaRGBA";
    b[b.JPEG = 4] = "JPEG";
    b[b.PNG = 5] = "PNG";
    b[b.GIF = 6] = "GIF";
  })(d.ImageType || (d.ImageType = {}));
  var r = d.ImageType;
  d.PromiseWrapper = function() {
    return function() {
      this.promise = new Promise(function(b, a) {
        this.resolve = b;
        this.reject = a;
      }.bind(this));
    };
  }();
})(Shumway || (Shumway = {}));
(function() {
  function d(b) {
    if ("function" !== typeof b) {
      throw new TypeError("Invalid deferred constructor");
    }
    var f = u();
    b = new b(f);
    var a = f.resolve;
    if ("function" !== typeof a) {
      throw new TypeError("Invalid resolve construction function");
    }
    f = f.reject;
    if ("function" !== typeof f) {
      throw new TypeError("Invalid reject construction function");
    }
    return{promise:b, resolve:a, reject:f};
  }
  function m(b, f) {
    if ("object" !== typeof b || null === b) {
      return!1;
    }
    try {
      var a = b.then;
      if ("function" !== typeof a) {
        return!1;
      }
      a.call(b, f.resolve, f.reject);
    } catch (c) {
      a = f.reject, a(c);
    }
    return!0;
  }
  function s(b) {
    return "object" === typeof b && null !== b && "undefined" !== typeof b.promiseStatus;
  }
  function e(b, f) {
    if ("unresolved" === b.promiseStatus) {
      var a = b.rejectReactions;
      b.result = f;
      b.resolveReactions = void 0;
      b.rejectReactions = void 0;
      b.promiseStatus = "has-rejection";
      t(a, f);
    }
  }
  function t(b, f) {
    for (var a = 0;a < b.length;a++) {
      k({reaction:b[a], argument:f});
    }
  }
  function k(b) {
    0 === w.length && setTimeout(a, 0);
    w.push(b);
  }
  function a() {
    for (;0 < w.length;) {
      var a = w[0];
      try {
        a: {
          var f = a.reaction, c = f.deferred, h = f.handler, g = void 0, p = void 0;
          try {
            g = h(a.argument);
          } catch (r) {
            var l = c.reject;
            l(r);
            break a;
          }
          if (g === c.promise) {
            l = c.reject, l(new TypeError("Self resolution"));
          } else {
            try {
              if (p = m(g, c), !p) {
                var k = c.resolve;
                k(g);
              }
            } catch (v) {
              l = c.reject, l(v);
            }
          }
        }
      } catch (e) {
        if ("function" === typeof b.onerror) {
          b.onerror(e);
        }
      }
      w.shift();
    }
  }
  function c(b) {
    throw b;
  }
  function g(b) {
    return b;
  }
  function p(b) {
    return function(f) {
      e(b, f);
    };
  }
  function v(b) {
    return function(f) {
      if ("unresolved" === b.promiseStatus) {
        var a = b.resolveReactions;
        b.result = f;
        b.resolveReactions = void 0;
        b.rejectReactions = void 0;
        b.promiseStatus = "has-resolution";
        t(a, f);
      }
    };
  }
  function u() {
    function b(f, a) {
      b.resolve = f;
      b.reject = a;
    }
    return b;
  }
  function l(b, f, a) {
    return function(c) {
      if (c === b) {
        return a(new TypeError("Self resolution"));
      }
      var h = b.promiseConstructor;
      if (s(c) && c.promiseConstructor === h) {
        return c.then(f, a);
      }
      h = d(h);
      return m(c, h) ? h.promise.then(f, a) : f(c);
    };
  }
  function r(b, f, a, c) {
    return function(h) {
      f[b] = h;
      c.countdown--;
      0 === c.countdown && a.resolve(f);
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
    var f = v(this), c = p(this);
    try {
      a(f, c);
    } catch (h) {
      e(this, h);
    }
    this.promiseConstructor = b;
    return this;
  }
  var h = Function("return this")();
  if (h.Promise) {
    "function" !== typeof h.Promise.all && (h.Promise.all = function(b) {
      var f = 0, a = [], c, g, p = new h.Promise(function(b, f) {
        c = b;
        g = f;
      });
      b.forEach(function(b, h) {
        f++;
        b.then(function(b) {
          a[h] = b;
          f--;
          0 === f && c(a);
        }, g);
      });
      0 === f && c(a);
      return p;
    }), "function" !== typeof h.Promise.resolve && (h.Promise.resolve = function(b) {
      return new h.Promise(function(f) {
        f(b);
      });
    });
  } else {
    var w = [];
    b.all = function(b) {
      var f = d(this), a = [], c = {countdown:0}, h = 0;
      b.forEach(function(b) {
        this.cast(b).then(r(h, a, f, c), f.reject);
        h++;
        c.countdown++;
      }, this);
      0 === h && f.resolve(a);
      return f.promise;
    };
    b.cast = function(b) {
      if (s(b)) {
        return b;
      }
      var f = d(this);
      f.resolve(b);
      return f.promise;
    };
    b.reject = function(b) {
      var f = d(this);
      f.reject(b);
      return f.promise;
    };
    b.resolve = function(b) {
      var f = d(this);
      f.resolve(b);
      return f.promise;
    };
    b.prototype = {"catch":function(b) {
      this.then(void 0, b);
    }, then:function(b, f) {
      if (!s(this)) {
        throw new TypeError("this is not a Promises");
      }
      var a = d(this.promiseConstructor), h = "function" === typeof f ? f : c, p = {deferred:a, handler:l(this, "function" === typeof b ? b : g, h)}, h = {deferred:a, handler:h};
      switch(this.promiseStatus) {
        case "unresolved":
          this.resolveReactions.push(p);
          this.rejectReactions.push(h);
          break;
        case "has-resolution":
          k({reaction:p, argument:this.result});
          break;
        case "has-rejection":
          k({reaction:h, argument:this.result});
      }
      return a.promise;
    }};
    h.Promise = b;
  }
})();
"undefined" !== typeof exports && (exports.Shumway = Shumway);
(function() {
  function d(d, s, e) {
    d[s] || Object.defineProperty(d, s, {value:e, writable:!0, configurable:!0, enumerable:!1});
  }
  d(String.prototype, "padRight", function(d, s) {
    var e = this, t = e.replace(/\033\[[0-9]*m/g, "").length;
    if (!d || t >= s) {
      return e;
    }
    for (var t = (s - t) / d.length, k = 0;k < t;k++) {
      e += d;
    }
    return e;
  });
  d(String.prototype, "padLeft", function(d, s) {
    var e = this, t = e.length;
    if (!d || t >= s) {
      return e;
    }
    for (var t = (s - t) / d.length, k = 0;k < t;k++) {
      e = d + e;
    }
    return e;
  });
  d(String.prototype, "trim", function() {
    return this.replace(/^\s+|\s+$/g, "");
  });
  d(String.prototype, "endsWith", function(d) {
    return-1 !== this.indexOf(d, this.length - d.length);
  });
  d(Array.prototype, "replace", function(d, s) {
    if (d === s) {
      return 0;
    }
    for (var e = 0, t = 0;t < this.length;t++) {
      this[t] === d && (this[t] = s, e++);
    }
    return e;
  });
})();
(function(d) {
  (function(m) {
    var s = d.isObject, e = d.Debug.assert, t = function() {
      function a(c, g, k, l) {
        this.shortName = c;
        this.longName = g;
        this.type = k;
        l = l || {};
        this.positional = l.positional;
        this.parseFn = l.parse;
        this.value = l.defaultValue;
      }
      a.prototype.parse = function(a) {
        "boolean" === this.type ? (e("boolean" === typeof a), this.value = a) : "number" === this.type ? (e(!isNaN(a), a + " is not a number"), this.value = parseInt(a, 10)) : this.value = a;
        this.parseFn && this.parseFn(this.value);
      };
      return a;
    }();
    m.Argument = t;
    var k = function() {
      function g() {
        this.args = [];
      }
      g.prototype.addArgument = function(a, c, g, l) {
        a = new t(a, c, g, l);
        this.args.push(a);
        return a;
      };
      g.prototype.addBoundOption = function(a) {
        this.args.push(new t(a.shortName, a.longName, a.type, {parse:function(c) {
          a.value = c;
        }}));
      };
      g.prototype.addBoundOptionSet = function(g) {
        var k = this;
        g.options.forEach(function(g) {
          g instanceof a ? k.addBoundOptionSet(g) : (e(g instanceof c), k.addBoundOption(g));
        });
      };
      g.prototype.getUsage = function() {
        var a = "";
        this.args.forEach(function(c) {
          a = c.positional ? a + c.longName : a + ("[-" + c.shortName + "|--" + c.longName + ("boolean" === c.type ? "" : " " + c.type[0].toUpperCase()) + "]");
          a += " ";
        });
        return a;
      };
      g.prototype.parse = function(a) {
        var c = {}, g = [];
        this.args.forEach(function(b) {
          b.positional ? g.push(b) : (c["-" + b.shortName] = b, c["--" + b.longName] = b);
        });
        for (var l = [];a.length;) {
          var r = a.shift(), b = null, h = r;
          if ("--" == r) {
            l = l.concat(a);
            break;
          } else {
            if ("-" == r.slice(0, 1) || "--" == r.slice(0, 2)) {
              b = c[r];
              if (!b) {
                continue;
              }
              "boolean" !== b.type ? (h = a.shift(), e("-" !== h && "--" !== h, "Argument " + r + " must have a value.")) : h = !0;
            } else {
              g.length ? b = g.shift() : l.push(h);
            }
          }
          b && b.parse(h);
        }
        e(0 === g.length, "Missing positional arguments.");
        return l;
      };
      return g;
    }();
    m.ArgumentParser = k;
    var a = function() {
      function a(c, g) {
        "undefined" === typeof g && (g = null);
        this.open = !1;
        this.name = c;
        this.settings = g || {};
        this.options = [];
      }
      a.prototype.register = function(c) {
        if (c instanceof a) {
          for (var k = 0;k < this.options.length;k++) {
            var e = this.options[k];
            if (e instanceof a && e.name === c.name) {
              return e;
            }
          }
        }
        this.options.push(c);
        if (this.settings) {
          if (c instanceof a) {
            k = this.settings[c.name], s(k) && (c.settings = k.settings, c.open = k.open);
          } else {
            if ("undefined" !== typeof this.settings[c.longName]) {
              switch(c.type) {
                case "boolean":
                  c.value = !!this.settings[c.longName];
                  break;
                case "number":
                  c.value = +this.settings[c.longName];
                  break;
                default:
                  c.value = this.settings[c.longName];
              }
            }
          }
        }
        return c;
      };
      a.prototype.trace = function(a) {
        a.enter(this.name + " {");
        this.options.forEach(function(c) {
          c.trace(a);
        });
        a.leave("}");
      };
      a.prototype.getSettings = function() {
        var c = {};
        this.options.forEach(function(k) {
          k instanceof a ? c[k.name] = {settings:k.getSettings(), open:k.open} : c[k.longName] = k.value;
        });
        return c;
      };
      a.prototype.setSettings = function(c) {
        c && this.options.forEach(function(k) {
          k instanceof a ? k.name in c && k.setSettings(c[k.name].settings) : k.longName in c && (k.value = c[k.longName]);
        });
      };
      return a;
    }();
    m.OptionSet = a;
    var c = function() {
      function a(c, g, k, l, r, b) {
        "undefined" === typeof b && (b = null);
        this.longName = g;
        this.shortName = c;
        this.type = k;
        this.value = this.defaultValue = l;
        this.description = r;
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
    m.Option = c;
  })(d.Options || (d.Options = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    function s() {
      try {
        return "undefined" !== typeof window && "localStorage" in window && null !== window.localStorage;
      } catch (e) {
        return!1;
      }
    }
    function e(e) {
      "undefined" === typeof e && (e = m.ROOT);
      var k = {};
      if (s() && (e = window.localStorage[e])) {
        try {
          k = JSON.parse(e);
        } catch (a) {
        }
      }
      return k;
    }
    m.ROOT = "Shumway Options";
    m.shumwayOptions = new d.Options.OptionSet(m.ROOT, e());
    m.isStorageSupported = s;
    m.load = e;
    m.save = function(e, k) {
      "undefined" === typeof e && (e = null);
      "undefined" === typeof k && (k = m.ROOT);
      if (s()) {
        try {
          window.localStorage[k] = JSON.stringify(e ? e : m.shumwayOptions.getSettings());
        } catch (a) {
        }
      }
    };
    m.setSettings = function(e) {
      m.shumwayOptions.setSettings(e);
    };
    m.getSettings = function(e) {
      return m.shumwayOptions.getSettings();
    };
  })(d.Settings || (d.Settings = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = function() {
      function e(e, k) {
        this._parent = e;
        this._timers = d.ObjectUtilities.createMap();
        this._name = k;
        this._count = this._total = this._last = this._begin = 0;
      }
      e.time = function(d, k) {
        e.start(d);
        k();
        e.stop();
      };
      e.start = function(d) {
        e._top = e._top._timers[d] || (e._top._timers[d] = new e(e._top, d));
        e._top.start();
        d = e._flat._timers[d] || (e._flat._timers[d] = new e(e._flat, d));
        d.start();
        e._flatStack.push(d);
      };
      e.stop = function() {
        e._top.stop();
        e._top = e._top._parent;
        e._flatStack.pop().stop();
      };
      e.stopStart = function(d) {
        e.stop();
        e.start(d);
      };
      e.prototype.start = function() {
        this._begin = d.getTicks();
      };
      e.prototype.stop = function() {
        this._last = d.getTicks() - this._begin;
        this._total += this._last;
        this._count += 1;
      };
      e.prototype.toJSON = function() {
        return{name:this._name, total:this._total, timers:this._timers};
      };
      e.prototype.trace = function(e) {
        e.enter(this._name + ": " + this._total.toFixed(2) + " ms, count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var k in this._timers) {
          this._timers[k].trace(e);
        }
        e.outdent();
      };
      e.trace = function(d) {
        e._base.trace(d);
        e._flat.trace(d);
      };
      e._base = new e(null, "Total");
      e._top = e._base;
      e._flat = new e(null, "Flat");
      e._flatStack = [];
      return e;
    }();
    m.Timer = s;
    s = function() {
      function e(e) {
        this._enabled = e;
        this.clear();
      }
      Object.defineProperty(e.prototype, "counts", {get:function() {
        return this._counts;
      }, enumerable:!0, configurable:!0});
      e.prototype.setEnabled = function(e) {
        this._enabled = e;
      };
      e.prototype.clear = function() {
        this._counts = d.ObjectUtilities.createMap();
        this._times = d.ObjectUtilities.createMap();
      };
      e.prototype.toJSON = function() {
        return{counts:this._counts, times:this._times};
      };
      e.prototype.count = function(e, k, a) {
        "undefined" === typeof k && (k = 1);
        "undefined" === typeof a && (a = 0);
        if (this._enabled) {
          return void 0 === this._counts[e] && (this._counts[e] = 0, this._times[e] = 0), this._counts[e] += k, this._times[e] += a, this._counts[e];
        }
      };
      e.prototype.trace = function(e) {
        for (var k in this._counts) {
          e.writeLn(k + ": " + this._counts[k]);
        }
      };
      e.prototype._pairToString = function(e, k) {
        var a = k[0], c = k[1], g = e[a], a = a + ": " + c;
        g && (a += ", " + g.toFixed(4), 1 < c && (a += " (" + (g / c).toFixed(4) + ")"));
        return a;
      };
      e.prototype.toStringSorted = function() {
        var e = this, k = this._times, a = [], c;
        for (c in this._counts) {
          a.push([c, this._counts[c]]);
        }
        a.sort(function(a, c) {
          return c[1] - a[1];
        });
        return a.map(function(a) {
          return e._pairToString(k, a);
        }).join(", ");
      };
      e.prototype.traceSorted = function(e, k) {
        "undefined" === typeof k && (k = !1);
        var a = this, c = this._times, g = [], p;
        for (p in this._counts) {
          g.push([p, this._counts[p]]);
        }
        g.sort(function(a, c) {
          return c[1] - a[1];
        });
        k ? e.writeLn(g.map(function(g) {
          return a._pairToString(c, g);
        }).join(", ")) : g.forEach(function(g) {
          e.writeLn(a._pairToString(c, g));
        });
      };
      e.instance = new e(!0);
      return e;
    }();
    m.Counter = s;
    s = function() {
      function e(e) {
        this._samples = new Float64Array(e);
        this._index = this._count = 0;
      }
      e.prototype.push = function(e) {
        this._count < this._samples.length && this._count++;
        this._index++;
        this._samples[this._index % this._samples.length] = e;
      };
      e.prototype.average = function() {
        for (var e = 0, k = 0;k < this._count;k++) {
          e += this._samples[k];
        }
        return e / this._count;
      };
      return e;
    }();
    m.Average = s;
  })(d.Metrics || (d.Metrics = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    function s(b) {
      for (var a = Math.max.apply(null, b), c = b.length, g = 1 << a, f = new Uint32Array(g), q = a << 16 | 65535, p = 0;p < g;p++) {
        f[p] = q;
      }
      for (var q = 0, p = 1, r = 2;p <= a;q <<= 1, ++p, r <<= 1) {
        for (var l = 0;l < c;++l) {
          if (b[l] === p) {
            for (var k = 0, e = 0;e < p;++e) {
              k = 2 * k + (q >> e & 1);
            }
            for (e = k;e < g;e += r) {
              f[e] = p << 16 | l;
            }
            ++q;
          }
        }
      }
      return{codes:f, maxBits:a};
    }
    var e;
    (function(b) {
      b[b.INIT = 0] = "INIT";
      b[b.BLOCK_0 = 1] = "BLOCK_0";
      b[b.BLOCK_1 = 2] = "BLOCK_1";
      b[b.BLOCK_2_PRE = 3] = "BLOCK_2_PRE";
      b[b.BLOCK_2 = 4] = "BLOCK_2";
      b[b.DONE = 5] = "DONE";
      b[b.ERROR = 6] = "ERROR";
      b[b.VERIFY_HEADER = 7] = "VERIFY_HEADER";
    })(e || (e = {}));
    e = function() {
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
        if (!u) {
          t = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);
          k = new Uint16Array(30);
          a = new Uint8Array(30);
          for (var r = b = 0, n = 1;30 > b;++b) {
            k[b] = n, n += 1 << (a[b] = ~~((r += 2 < b ? 1 : 0) / 2));
          }
          var f = new Uint8Array(288);
          for (b = 0;32 > b;++b) {
            f[b] = 5;
          }
          c = s(f.subarray(0, 32));
          g = new Uint16Array(29);
          p = new Uint8Array(29);
          r = b = 0;
          for (n = 3;29 > b;++b) {
            g[b] = n - (28 == b ? 1 : 0), n += 1 << (p[b] = ~~((r += 4 < b ? 1 : 0) / 4 % 6));
          }
          for (b = 0;288 > b;++b) {
            f[b] = 144 > b || 279 < b ? 8 : 256 > b ? 9 : 7;
          }
          v = s(f);
          u = !0;
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
        var b = this._buffer, a = this._bufferSize, g = this._bitBuffer, f = this._bitLength, q = this._bufferPosition;
        if (3 > (a - q << 3) + f) {
          return!0;
        }
        3 > f && (g |= b[q++] << f, f += 8);
        var p = g & 7, g = g >> 3, f = f - 3;
        switch(p >> 1) {
          case 0:
            f = g = 0;
            if (4 > a - q) {
              return!0;
            }
            var r = b[q] | b[q + 1] << 8, b = b[q + 2] | b[q + 3] << 8, q = q + 4;
            if (65535 !== (r ^ b)) {
              this._error = "inflate: invalid block 0 length";
              b = 6;
              break;
            }
            0 === r ? b = 0 : (this._block0Read = r, b = 1);
            break;
          case 1:
            b = 2;
            this._literalTable = v;
            this._distanceTable = c;
            break;
          case 2:
            if (26 > (a - q << 3) + f) {
              return!0;
            }
            for (;14 > f;) {
              g |= b[q++] << f, f += 8;
            }
            r = (g >> 10 & 15) + 4;
            if ((a - q << 3) + f < 14 + 3 * r) {
              return!0;
            }
            for (var a = {numLiteralCodes:(g & 31) + 257, numDistanceCodes:(g >> 5 & 31) + 1, codeLengthTable:void 0, bitLengths:void 0, codesRead:0, dupBits:0}, g = g >> 14, f = f - 14, l = new Uint8Array(19), k = 0;k < r;++k) {
              3 > f && (g |= b[q++] << f, f += 8), l[t[k]] = g & 7, g >>= 3, f -= 3;
            }
            for (;19 > k;k++) {
              l[t[k]] = 0;
            }
            a.bitLengths = new Uint8Array(a.numLiteralCodes + a.numDistanceCodes);
            a.codeLengthTable = s(l);
            this._block2State = a;
            b = 3;
            break;
          default:
            return this._error = "inflate: unsupported block type", !1;
        }
        this._isFinalBlock = !!(p & 1);
        this._state = b;
        this._bufferPosition = q;
        this._bitBuffer = g;
        this._bitLength = f;
        return!1;
      };
      b.prototype._decodeBlock0 = function() {
        var b = this._bufferPosition, a = this._windowPosition, c = this._block0Read, f = 65794 - a, q = this._bufferSize - b, g = q < c, p = Math.min(f, q, c);
        this._window.set(this._buffer.subarray(b, b + p), a);
        this._windowPosition = a + p;
        this._bufferPosition = b + p;
        this._block0Read = c - p;
        return c === p ? (this._state = 0, !1) : g && f < q;
      };
      b.prototype._readBits = function(b) {
        var a = this._bitBuffer, c = this._bitLength;
        if (b > c) {
          var f = this._bufferPosition, q = this._bufferSize;
          do {
            if (f >= q) {
              return this._bufferPosition = f, this._bitBuffer = a, this._bitLength = c, -1;
            }
            a |= this._buffer[f++] << c;
            c += 8;
          } while (b > c);
          this._bufferPosition = f;
        }
        this._bitBuffer = a >> b;
        this._bitLength = c - b;
        return a & (1 << b) - 1;
      };
      b.prototype._readCode = function(b) {
        var a = this._bitBuffer, c = this._bitLength, f = b.maxBits;
        if (f > c) {
          var q = this._bufferPosition, g = this._bufferSize;
          do {
            if (q >= g) {
              return this._bufferPosition = q, this._bitBuffer = a, this._bitLength = c, -1;
            }
            a |= this._buffer[q++] << c;
            c += 8;
          } while (f > c);
          this._bufferPosition = q;
        }
        b = b.codes[a & (1 << f) - 1];
        f = b >> 16;
        if (b & 32768) {
          return this._error = "inflate: invalid encoding", this._state = 6, -1;
        }
        this._bitBuffer = a >> f;
        this._bitLength = c - f;
        return b & 65535;
      };
      b.prototype._decodeBlock2Pre = function() {
        var b = this._block2State, a = b.numLiteralCodes + b.numDistanceCodes, c = b.bitLengths, f = b.codesRead, q = 0 < f ? c[f - 1] : 0, g = b.codeLengthTable, p;
        if (0 < b.dupBits) {
          p = this._readBits(b.dupBits);
          if (0 > p) {
            return!0;
          }
          for (;p--;) {
            c[f++] = q;
          }
          b.dupBits = 0;
        }
        for (;f < a;) {
          var r = this._readCode(g);
          if (0 > r) {
            return b.codesRead = f, !0;
          }
          if (16 > r) {
            c[f++] = q = r;
          } else {
            var l;
            switch(r) {
              case 16:
                l = 2;
                p = 3;
                r = q;
                break;
              case 17:
                p = l = 3;
                r = 0;
                break;
              case 18:
                l = 7, p = 11, r = 0;
            }
            for (;p--;) {
              c[f++] = r;
            }
            p = this._readBits(l);
            if (0 > p) {
              return b.codesRead = f, b.dupBits = l, !0;
            }
            for (;p--;) {
              c[f++] = r;
            }
            q = r;
          }
        }
        this._literalTable = s(c.subarray(0, b.numLiteralCodes));
        this._distanceTable = s(c.subarray(b.numLiteralCodes));
        this._state = 4;
        this._block2State = null;
        return!1;
      };
      b.prototype._decodeBlock = function() {
        var b = this._literalTable, c = this._distanceTable, n = this._window, f = this._windowPosition, q = this._copyState, r, l, e, v;
        if (0 !== q.state) {
          switch(q.state) {
            case 1:
              if (0 > (r = this._readBits(q.lenBits))) {
                return!0;
              }
              q.len += r;
              q.state = 2;
            case 2:
              if (0 > (r = this._readCode(c))) {
                return!0;
              }
              q.distBits = a[r];
              q.dist = k[r];
              q.state = 3;
            case 3:
              r = 0;
              if (0 < q.distBits && 0 > (r = this._readBits(q.distBits))) {
                return!0;
              }
              v = q.dist + r;
              l = q.len;
              for (r = f - v;l--;) {
                n[f++] = n[r++];
              }
              q.state = 0;
              if (65536 <= f) {
                return this._windowPosition = f, !1;
              }
              break;
          }
        }
        do {
          r = this._readCode(b);
          if (0 > r) {
            return this._windowPosition = f, !0;
          }
          if (256 > r) {
            n[f++] = r;
          } else {
            if (256 < r) {
              this._windowPosition = f;
              r -= 257;
              e = p[r];
              l = g[r];
              r = 0 === e ? 0 : this._readBits(e);
              if (0 > r) {
                return q.state = 1, q.len = l, q.lenBits = e, !0;
              }
              l += r;
              r = this._readCode(c);
              if (0 > r) {
                return q.state = 2, q.len = l, !0;
              }
              e = a[r];
              v = k[r];
              r = 0 === e ? 0 : this._readBits(e);
              if (0 > r) {
                return q.state = 3, q.len = l, q.dist = v, q.distBits = e, !0;
              }
              v += r;
              for (r = f - v;l--;) {
                n[f++] = n[r++];
              }
            } else {
              this._state = 0;
              break;
            }
          }
        } while (65536 > f);
        this._windowPosition = f;
        return!1;
      };
      b.inflate = function(a, c, g) {
        var f = new Uint8Array(c), q = 0;
        c = new b(g);
        c.onData = function(b) {
          f.set(b, q);
          q += b.length;
        };
        c.push(a);
        return f;
      };
      return b;
    }();
    d.Inflate = e;
    var t, k, a, c, g, p, v, u = !1, l;
    (function(b) {
      b[b.WRITE = 0] = "WRITE";
      b[b.DONE = 1] = "DONE";
      b[b.ZLIB_HEADER = 2] = "ZLIB_HEADER";
    })(l || (l = {}));
    var r = function() {
      function b() {
        this.a = 1;
        this.b = 0;
      }
      b.prototype.update = function(b, a, c) {
        for (var f = this.a, q = this.b;a < c;++a) {
          f = (f + (b[a] & 255)) % 65521, q = (q + f) % 65521;
        }
        this.a = f;
        this.b = q;
      };
      b.prototype.getChecksum = function() {
        return this.b << 16 | this.a;
      };
      return b;
    }();
    d.Adler32 = r;
    l = function() {
      function b(b) {
        this._state = (this._writeZlibHeader = b) ? 2 : 0;
        this._adler32 = b ? new r : null;
      }
      b.prototype.push = function(b) {
        2 === this._state && (this.onData(new Uint8Array([120, 156])), this._state = 0);
        for (var a = b.length, c = new Uint8Array(a + 5 * Math.ceil(a / 65535)), f = 0, q = 0;65535 < a;) {
          c.set(new Uint8Array([0, 255, 255, 0, 0]), f), f += 5, c.set(b.subarray(q, q + 65535), f), q += 65535, f += 65535, a -= 65535;
        }
        c.set(new Uint8Array([0, a & 255, a >> 8 & 255, ~a & 255, ~a >> 8 & 255]), f);
        c.set(b.subarray(q, a), f + 5);
        this.onData(c);
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
    d.Deflate = l;
  })(d.ArrayUtilities || (d.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    function s() {
      t("throwEOFError");
    }
    function e(a) {
      return "string" === typeof a ? a : void 0 == a ? null : a + "";
    }
    var t = d.Debug.notImplemented, k = d.StringUtilities.utf8decode, a = d.StringUtilities.utf8encode, c = d.NumberUtilities.clamp, g = function() {
      return function(a, c, b) {
        this.buffer = a;
        this.length = c;
        this.littleEndian = b;
      };
    }();
    m.PlainObjectDataBuffer = g;
    for (var p = new Uint32Array(33), v = 1, u = 0;32 >= v;v++) {
      p[v] = u = u << 1 | 1;
    }
    v = function() {
      function l(a) {
        "undefined" === typeof a && (a = l.INITIAL_SIZE);
        this._buffer || (this._buffer = new ArrayBuffer(a), this._position = this._length = 0, this._updateViews(), this._littleEndian = l._nativeLittleEndian, this._bitLength = this._bitBuffer = 0);
      }
      l.FromArrayBuffer = function(a, b) {
        "undefined" === typeof b && (b = -1);
        var c = Object.create(l.prototype);
        c._buffer = a;
        c._length = -1 === b ? a.byteLength : b;
        c._position = 0;
        c._updateViews();
        c._littleEndian = l._nativeLittleEndian;
        c._bitBuffer = 0;
        c._bitLength = 0;
        return c;
      };
      l.FromPlainObject = function(a) {
        var b = l.FromArrayBuffer(a.buffer, a.length);
        b._littleEndian = a.littleEndian;
        return b;
      };
      l.prototype.toPlainObject = function() {
        return new g(this._buffer, this._length, this._littleEndian);
      };
      l.prototype._updateViews = function() {
        this._u8 = new Uint8Array(this._buffer);
        0 === (this._buffer.byteLength & 3) && (this._i32 = new Int32Array(this._buffer), this._f32 = new Float32Array(this._buffer));
      };
      l.prototype.getBytes = function() {
        return new Uint8Array(this._buffer, 0, this._length);
      };
      l.prototype._ensureCapacity = function(a) {
        var b = this._buffer;
        if (b.byteLength < a) {
          for (var c = Math.max(b.byteLength, 1);c < a;) {
            c *= 2;
          }
          a = l._arrayBufferPool.acquire(c);
          c = this._u8;
          this._buffer = a;
          this._updateViews();
          this._u8.set(c);
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
        this._position + 1 > this._length && s();
        return this._u8[this._position++];
      };
      l.prototype.readBytes = function(a, b) {
        var c = 0;
        "undefined" === typeof c && (c = 0);
        "undefined" === typeof b && (b = 0);
        var g = this._position;
        c || (c = 0);
        b || (b = this._length - g);
        g + b > this._length && s();
        a.length < c + b && (a._ensureCapacity(c + b), a.length = c + b);
        a._u8.set(new Uint8Array(this._buffer, g, b), c);
        this._position += b;
      };
      l.prototype.readShort = function() {
        return this.readUnsignedShort() << 16 >> 16;
      };
      l.prototype.readUnsignedShort = function() {
        var a = this._u8, b = this._position;
        b + 2 > this._length && s();
        var c = a[b + 0], a = a[b + 1];
        this._position = b + 2;
        return this._littleEndian ? a << 8 | c : c << 8 | a;
      };
      l.prototype.readInt = function() {
        var a = this._u8, b = this._position;
        b + 4 > this._length && s();
        var c = a[b + 0], g = a[b + 1], n = a[b + 2], a = a[b + 3];
        this._position = b + 4;
        return this._littleEndian ? a << 24 | n << 16 | g << 8 | c : c << 24 | g << 16 | n << 8 | a;
      };
      l.prototype.readUnsignedInt = function() {
        return this.readInt() >>> 0;
      };
      l.prototype.readFloat = function() {
        var a = this._position;
        a + 4 > this._length && s();
        this._position = a + 4;
        if (this._littleEndian && 0 === (a & 3) && this._f32) {
          return this._f32[a >> 2];
        }
        var b = this._u8, c = d.IntegerUtilities.u8;
        this._littleEndian ? (c[0] = b[a + 0], c[1] = b[a + 1], c[2] = b[a + 2], c[3] = b[a + 3]) : (c[3] = b[a + 0], c[2] = b[a + 1], c[1] = b[a + 2], c[0] = b[a + 3]);
        return d.IntegerUtilities.f32[0];
      };
      l.prototype.readDouble = function() {
        var a = this._u8, b = this._position;
        b + 8 > this._length && s();
        var c = d.IntegerUtilities.u8;
        this._littleEndian ? (c[0] = a[b + 0], c[1] = a[b + 1], c[2] = a[b + 2], c[3] = a[b + 3], c[4] = a[b + 4], c[5] = a[b + 5], c[6] = a[b + 6], c[7] = a[b + 7]) : (c[0] = a[b + 7], c[1] = a[b + 6], c[2] = a[b + 5], c[3] = a[b + 4], c[4] = a[b + 3], c[5] = a[b + 2], c[6] = a[b + 1], c[7] = a[b + 0]);
        this._position = b + 8;
        return d.IntegerUtilities.f64[0];
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
      l.prototype.writeBytes = function(a, b, g) {
        "undefined" === typeof b && (b = 0);
        "undefined" === typeof g && (g = 0);
        2 > arguments.length && (b = 0);
        3 > arguments.length && (g = 0);
        b !== c(b, 0, a.length) && t("throwRangeError");
        var p = b + g;
        p !== c(p, 0, a.length) && t("throwRangeError");
        0 === g && (g = a.length - b);
        this.writeRawBytes(new Int8Array(a._buffer, b, g));
      };
      l.prototype.writeShort = function(a) {
        this.writeUnsignedShort(a);
      };
      l.prototype.writeUnsignedShort = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 2);
        var c = this._u8;
        this._littleEndian ? (c[b + 0] = a, c[b + 1] = a >> 8) : (c[b + 0] = a >> 8, c[b + 1] = a);
        this._position = b += 2;
        b > this._length && (this._length = b);
      };
      l.prototype.writeInt = function(a) {
        this.writeUnsignedInt(a);
      };
      l.prototype.write4Ints = function(a, b, c, g) {
        this.write4UnsignedInts(a, b, c, g);
      };
      l.prototype.writeUnsignedInt = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 4);
        if (this._littleEndian === l._nativeLittleEndian && 0 === (b & 3) && this._i32) {
          this._i32[b >> 2] = a;
        } else {
          var c = this._u8;
          this._littleEndian ? (c[b + 0] = a, c[b + 1] = a >> 8, c[b + 2] = a >> 16, c[b + 3] = a >> 24) : (c[b + 0] = a >> 24, c[b + 1] = a >> 16, c[b + 2] = a >> 8, c[b + 3] = a);
        }
        this._position = b += 4;
        b > this._length && (this._length = b);
      };
      l.prototype.write4UnsignedInts = function(a, b, c, g) {
        var n = this._position;
        this._ensureCapacity(n + 16);
        this._littleEndian === l._nativeLittleEndian && 0 === (n & 3) && this._i32 ? (this._i32[(n >> 2) + 0] = a, this._i32[(n >> 2) + 1] = b, this._i32[(n >> 2) + 2] = c, this._i32[(n >> 2) + 3] = g, this._position = n += 16, n > this._length && (this._length = n)) : (this.writeUnsignedInt(a), this.writeUnsignedInt(b), this.writeUnsignedInt(c), this.writeUnsignedInt(g));
      };
      l.prototype.writeFloat = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 4);
        if (this._littleEndian === l._nativeLittleEndian && 0 === (b & 3) && this._f32) {
          this._f32[b >> 2] = a;
        } else {
          var c = this._u8;
          d.IntegerUtilities.f32[0] = a;
          a = d.IntegerUtilities.u8;
          this._littleEndian ? (c[b + 0] = a[0], c[b + 1] = a[1], c[b + 2] = a[2], c[b + 3] = a[3]) : (c[b + 0] = a[3], c[b + 1] = a[2], c[b + 2] = a[1], c[b + 3] = a[0]);
        }
        this._position = b += 4;
        b > this._length && (this._length = b);
      };
      l.prototype.write6Floats = function(a, b, c, g, n, f) {
        var q = this._position;
        this._ensureCapacity(q + 24);
        this._littleEndian === l._nativeLittleEndian && 0 === (q & 3) && this._f32 ? (this._f32[(q >> 2) + 0] = a, this._f32[(q >> 2) + 1] = b, this._f32[(q >> 2) + 2] = c, this._f32[(q >> 2) + 3] = g, this._f32[(q >> 2) + 4] = n, this._f32[(q >> 2) + 5] = f, this._position = q += 24, q > this._length && (this._length = q)) : (this.writeFloat(a), this.writeFloat(b), this.writeFloat(c), this.writeFloat(g), this.writeFloat(n), this.writeFloat(f));
      };
      l.prototype.writeDouble = function(a) {
        var b = this._position;
        this._ensureCapacity(b + 8);
        var c = this._u8;
        d.IntegerUtilities.f64[0] = a;
        a = d.IntegerUtilities.u8;
        this._littleEndian ? (c[b + 0] = a[0], c[b + 1] = a[1], c[b + 2] = a[2], c[b + 3] = a[3], c[b + 4] = a[4], c[b + 5] = a[5], c[b + 6] = a[6], c[b + 7] = a[7]) : (c[b + 0] = a[7], c[b + 1] = a[6], c[b + 2] = a[5], c[b + 3] = a[4], c[b + 4] = a[3], c[b + 5] = a[2], c[b + 6] = a[1], c[b + 7] = a[0]);
        this._position = b += 8;
        b > this._length && (this._length = b);
      };
      l.prototype.readRawBytes = function() {
        return new Int8Array(this._buffer, 0, this._length);
      };
      l.prototype.writeUTF = function(a) {
        a = e(a);
        a = k(a);
        this.writeShort(a.length);
        this.writeRawBytes(a);
      };
      l.prototype.writeUTFBytes = function(a) {
        a = e(a);
        a = k(a);
        this.writeRawBytes(a);
      };
      l.prototype.readUTF = function() {
        return this.readUTFBytes(this.readShort());
      };
      l.prototype.readUTFBytes = function(c) {
        c >>>= 0;
        var b = this._position;
        b + c > this._length && s();
        this._position += c;
        return a(new Int8Array(this._buffer, b, c));
      };
      Object.defineProperty(l.prototype, "length", {get:function() {
        return this._length;
      }, set:function(a) {
        a >>>= 0;
        a > this._buffer.byteLength && this._ensureCapacity(a);
        this._length = a;
        this._position = c(this._position, 0, this._length);
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
        a = e(a);
        this._littleEndian = "auto" === a ? l._nativeLittleEndian : "littleEndian" === a;
      }, enumerable:!0, configurable:!0});
      l.prototype.toString = function() {
        return a(new Int8Array(this._buffer, 0, this._length));
      };
      l.prototype.toBlob = function() {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)]);
      };
      l.prototype.writeMultiByte = function(a, b) {
        t("packageInternal flash.utils.ObjectOutput::writeMultiByte");
      };
      l.prototype.readMultiByte = function(a, b) {
        t("packageInternal flash.utils.ObjectInput::readMultiByte");
      };
      l.prototype.getValue = function(a) {
        a |= 0;
        return a >= this._length ? void 0 : this._u8[a];
      };
      l.prototype.setValue = function(a, b) {
        a |= 0;
        var c = a + 1;
        this._ensureCapacity(c);
        this._u8[a] = b;
        c > this._length && (this._length = c);
      };
      l.prototype.readFixed = function() {
        return this.readInt() / 65536;
      };
      l.prototype.readFixed8 = function() {
        return this.readShort() / 256;
      };
      l.prototype.readFloat16 = function() {
        var a = this.readUnsignedShort(), b = a >> 15 ? -1 : 1, c = (a & 31744) >> 10, a = a & 1023;
        return c ? 31 === c ? a ? NaN : Infinity * b : b * Math.pow(2, c - 15) * (1 + a / 1024) : a / 1024 * Math.pow(2, -14) * b;
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
        for (var b = this._bitBuffer, c = this._bitLength;a > c;) {
          b = b << 8 | this.readUnsignedByte(), c += 8;
        }
        c -= a;
        a = b >>> c & p[a];
        this._bitBuffer = b;
        this._bitLength = c;
        return a;
      };
      l.prototype.readFixedBits = function(a) {
        return this.readBits(a) / 65536;
      };
      l.prototype.readString = function(c) {
        var b = this._position;
        if (c) {
          b + c > this._length && s(), this._position += c;
        } else {
          c = 0;
          for (var g = b;g < this._length && this._u8[g];g++) {
            c++;
          }
          this._position += c + 1;
        }
        return a(new Int8Array(this._buffer, b, c));
      };
      l.prototype.align = function() {
        this._bitLength = this._bitBuffer = 0;
      };
      l.prototype._compress = function(a) {
        a = e(a);
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
        a = e(a);
        switch(a) {
          case "zlib":
            a = new m.Inflate(!0);
            break;
          case "deflate":
            a = new m.Inflate(!1);
            break;
          default:
            return;
        }
        var b = new l;
        a.onData = b.writeRawBytes.bind(b);
        a.push(this._u8.subarray(0, this._length));
        a.error && t("throwCompressedDataError");
        this._ensureCapacity(b._u8.length);
        this._u8.set(b._u8);
        this.length = b.length;
        this._position = 0;
      };
      l._nativeLittleEndian = 1 === (new Int8Array((new Int32Array([1])).buffer))[0];
      l.INITIAL_SIZE = 128;
      l._arrayBufferPool = new d.ArrayBufferPool;
      return l;
    }();
    m.DataBuffer = v;
  })(d.ArrayUtilities || (d.ArrayUtilities = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  var m = d.ArrayUtilities.DataBuffer, s = d.ArrayUtilities.ensureTypedArrayCapacity, e = d.Debug.assert;
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
  })(d.PathCommand || (d.PathCommand = {}));
  (function(a) {
    a[a.Linear = 16] = "Linear";
    a[a.Radial = 18] = "Radial";
  })(d.GradientType || (d.GradientType = {}));
  (function(a) {
    a[a.Pad = 0] = "Pad";
    a[a.Reflect = 1] = "Reflect";
    a[a.Repeat = 2] = "Repeat";
  })(d.GradientSpreadMethod || (d.GradientSpreadMethod = {}));
  (function(a) {
    a[a.RGB = 0] = "RGB";
    a[a.LinearRGB = 1] = "LinearRGB";
  })(d.GradientInterpolationMethod || (d.GradientInterpolationMethod = {}));
  (function(a) {
    a[a.None = 0] = "None";
    a[a.Normal = 1] = "Normal";
    a[a.Vertical = 2] = "Vertical";
    a[a.Horizontal = 3] = "Horizontal";
  })(d.LineScaleMode || (d.LineScaleMode = {}));
  var t = function() {
    return function(a, c, g, p, k, e, l, r, b) {
      this.commands = a;
      this.commandsPosition = c;
      this.coordinates = g;
      this.coordinatesPosition = p;
      this.morphCoordinates = k;
      this.styles = e;
      this.stylesLength = l;
      this.hasFills = r;
      this.hasLines = b;
    };
  }();
  d.PlainObjectShapeData = t;
  var k;
  (function(a) {
    a[a.Commands = 32] = "Commands";
    a[a.Coordinates = 128] = "Coordinates";
    a[a.Styles = 16] = "Styles";
  })(k || (k = {}));
  k = function() {
    function a(a) {
      "undefined" === typeof a && (a = !0);
      a && this.clear();
    }
    a.FromPlainObject = function(c) {
      var g = new a(!1);
      g.commands = c.commands;
      g.coordinates = c.coordinates;
      g.morphCoordinates = c.morphCoordinates;
      g.commandsPosition = c.commandsPosition;
      g.coordinatesPosition = c.coordinatesPosition;
      g.styles = m.FromArrayBuffer(c.styles, c.stylesLength);
      g.styles.endian = "auto";
      g.hasFills = c.hasFills;
      g.hasLines = c.hasLines;
      return g;
    };
    a.prototype.moveTo = function(a, g) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 9;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = g;
    };
    a.prototype.lineTo = function(a, g) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 10;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = g;
    };
    a.prototype.curveTo = function(a, g, p, k) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = g;
      this.coordinates[this.coordinatesPosition++] = p;
      this.coordinates[this.coordinatesPosition++] = k;
    };
    a.prototype.cubicCurveTo = function(a, g, p, k, e, l) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12;
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = g;
      this.coordinates[this.coordinatesPosition++] = p;
      this.coordinates[this.coordinatesPosition++] = k;
      this.coordinates[this.coordinatesPosition++] = e;
      this.coordinates[this.coordinatesPosition++] = l;
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
    a.prototype.lineStyle = function(a, g, p, k, d, l, r) {
      e(a === (a | 0), 0 <= a && 5100 >= a);
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5;
      this.coordinates[this.coordinatesPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(g);
      a.writeBoolean(p);
      a.writeUnsignedByte(k);
      a.writeUnsignedByte(d);
      a.writeUnsignedByte(l);
      a.writeUnsignedByte(r);
      this.hasLines = !0;
    };
    a.prototype.beginBitmap = function(a, g, p, k, d) {
      e(3 === a || 7 === a);
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedInt(g);
      this._writeStyleMatrix(p);
      a.writeBoolean(k);
      a.writeBoolean(d);
      this.hasFills = !0;
    };
    a.prototype.beginGradient = function(a, g, p, k, d, l, r, b) {
      e(2 === a || 6 === a);
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = a;
      a = this.styles;
      a.writeUnsignedByte(k);
      e(b === (b | 0));
      a.writeShort(b);
      this._writeStyleMatrix(d);
      k = g.length;
      a.writeByte(k);
      for (d = 0;d < k;d++) {
        a.writeUnsignedByte(p[d]), a.writeUnsignedInt(g[d]);
      }
      a.writeUnsignedByte(l);
      a.writeUnsignedByte(r);
      this.hasFills = !0;
    };
    a.prototype.writeCommandAndCoordinates = function(a, g, p) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = g;
      this.coordinates[this.coordinatesPosition++] = p;
    };
    a.prototype.writeCoordinates = function(a, g) {
      this.ensurePathCapacities(0, 2);
      this.coordinates[this.coordinatesPosition++] = a;
      this.coordinates[this.coordinatesPosition++] = g;
    };
    a.prototype.writeMorphCoordinates = function(a, g) {
      this.morphCoordinates = s(this.morphCoordinates, this.coordinatesPosition);
      this.morphCoordinates[this.coordinatesPosition - 2] = a;
      this.morphCoordinates[this.coordinatesPosition - 1] = g;
    };
    a.prototype.clear = function() {
      this.commandsPosition = this.coordinatesPosition = 0;
      this.commands = new Uint8Array(32);
      this.coordinates = new Int32Array(128);
      this.styles = new m(16);
      this.styles.endian = "auto";
      this.hasFills = this.hasLines = !1;
    };
    a.prototype.isEmpty = function() {
      return 0 === this.commandsPosition;
    };
    a.prototype.clone = function() {
      var c = new a(!1);
      c.commands = new Uint8Array(this.commands);
      c.commandsPosition = this.commandsPosition;
      c.coordinates = new Int32Array(this.coordinates);
      c.coordinatesPosition = this.coordinatesPosition;
      c.styles = new m(this.styles.length);
      c.styles.writeRawBytes(this.styles.bytes);
      c.hasFills = this.hasFills;
      c.hasLines = this.hasLines;
      return c;
    };
    a.prototype.toPlainObject = function() {
      return new t(this.commands, this.commandsPosition, this.coordinates, this.coordinatesPosition, this.morphCoordinates, this.styles.buffer, this.styles.length, this.hasFills, this.hasLines);
    };
    Object.defineProperty(a.prototype, "buffers", {get:function() {
      var a = [this.commands.buffer, this.coordinates.buffer, this.styles.buffer];
      this.morphCoordinates && a.push(this.morphCoordinates.buffer);
      return a;
    }, enumerable:!0, configurable:!0});
    a.prototype._writeStyleMatrix = function(a) {
      var g = this.styles;
      g.writeFloat(a.a);
      g.writeFloat(a.b);
      g.writeFloat(a.c);
      g.writeFloat(a.d);
      g.writeFloat(a.tx);
      g.writeFloat(a.ty);
    };
    a.prototype.ensurePathCapacities = function(a, g) {
      this.commands = s(this.commands, this.commandsPosition + a);
      this.coordinates = s(this.coordinates, this.coordinatesPosition + g);
    };
    return a;
  }();
  d.ShapeData = k;
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      (function(e) {
        e[e.CODE_END = 0] = "CODE_END";
        e[e.CODE_SHOW_FRAME = 1] = "CODE_SHOW_FRAME";
        e[e.CODE_DEFINE_SHAPE = 2] = "CODE_DEFINE_SHAPE";
        e[e.CODE_FREE_CHARACTER = 3] = "CODE_FREE_CHARACTER";
        e[e.CODE_PLACE_OBJECT = 4] = "CODE_PLACE_OBJECT";
        e[e.CODE_REMOVE_OBJECT = 5] = "CODE_REMOVE_OBJECT";
        e[e.CODE_DEFINE_BITS = 6] = "CODE_DEFINE_BITS";
        e[e.CODE_DEFINE_BUTTON = 7] = "CODE_DEFINE_BUTTON";
        e[e.CODE_JPEG_TABLES = 8] = "CODE_JPEG_TABLES";
        e[e.CODE_SET_BACKGROUND_COLOR = 9] = "CODE_SET_BACKGROUND_COLOR";
        e[e.CODE_DEFINE_FONT = 10] = "CODE_DEFINE_FONT";
        e[e.CODE_DEFINE_TEXT = 11] = "CODE_DEFINE_TEXT";
        e[e.CODE_DO_ACTION = 12] = "CODE_DO_ACTION";
        e[e.CODE_DEFINE_FONT_INFO = 13] = "CODE_DEFINE_FONT_INFO";
        e[e.CODE_DEFINE_SOUND = 14] = "CODE_DEFINE_SOUND";
        e[e.CODE_START_SOUND = 15] = "CODE_START_SOUND";
        e[e.CODE_STOP_SOUND = 16] = "CODE_STOP_SOUND";
        e[e.CODE_DEFINE_BUTTON_SOUND = 17] = "CODE_DEFINE_BUTTON_SOUND";
        e[e.CODE_SOUND_STREAM_HEAD = 18] = "CODE_SOUND_STREAM_HEAD";
        e[e.CODE_SOUND_STREAM_BLOCK = 19] = "CODE_SOUND_STREAM_BLOCK";
        e[e.CODE_DEFINE_BITS_LOSSLESS = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        e[e.CODE_DEFINE_BITS_JPEG2 = 21] = "CODE_DEFINE_BITS_JPEG2";
        e[e.CODE_DEFINE_SHAPE2 = 22] = "CODE_DEFINE_SHAPE2";
        e[e.CODE_DEFINE_BUTTON_CXFORM = 23] = "CODE_DEFINE_BUTTON_CXFORM";
        e[e.CODE_PROTECT = 24] = "CODE_PROTECT";
        e[e.CODE_PATHS_ARE_POSTSCRIPT = 25] = "CODE_PATHS_ARE_POSTSCRIPT";
        e[e.CODE_PLACE_OBJECT2 = 26] = "CODE_PLACE_OBJECT2";
        e[e.CODE_REMOVE_OBJECT2 = 28] = "CODE_REMOVE_OBJECT2";
        e[e.CODE_SYNC_FRAME = 29] = "CODE_SYNC_FRAME";
        e[e.CODE_FREE_ALL = 31] = "CODE_FREE_ALL";
        e[e.CODE_DEFINE_SHAPE3 = 32] = "CODE_DEFINE_SHAPE3";
        e[e.CODE_DEFINE_TEXT2 = 33] = "CODE_DEFINE_TEXT2";
        e[e.CODE_DEFINE_BUTTON2 = 34] = "CODE_DEFINE_BUTTON2";
        e[e.CODE_DEFINE_BITS_JPEG3 = 35] = "CODE_DEFINE_BITS_JPEG3";
        e[e.CODE_DEFINE_BITS_LOSSLESS2 = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        e[e.CODE_DEFINE_EDIT_TEXT = 37] = "CODE_DEFINE_EDIT_TEXT";
        e[e.CODE_DEFINE_VIDEO = 38] = "CODE_DEFINE_VIDEO";
        e[e.CODE_DEFINE_SPRITE = 39] = "CODE_DEFINE_SPRITE";
        e[e.CODE_NAME_CHARACTER = 40] = "CODE_NAME_CHARACTER";
        e[e.CODE_PRODUCT_INFO = 41] = "CODE_PRODUCT_INFO";
        e[e.CODE_DEFINE_TEXT_FORMAT = 42] = "CODE_DEFINE_TEXT_FORMAT";
        e[e.CODE_FRAME_LABEL = 43] = "CODE_FRAME_LABEL";
        e[e.CODE_DEFINE_BEHAVIOUR = 44] = "CODE_DEFINE_BEHAVIOUR";
        e[e.CODE_SOUND_STREAM_HEAD2 = 45] = "CODE_SOUND_STREAM_HEAD2";
        e[e.CODE_DEFINE_MORPH_SHAPE = 46] = "CODE_DEFINE_MORPH_SHAPE";
        e[e.CODE_FRAME_TAG = 47] = "CODE_FRAME_TAG";
        e[e.CODE_DEFINE_FONT2 = 48] = "CODE_DEFINE_FONT2";
        e[e.CODE_GEN_COMMAND = 49] = "CODE_GEN_COMMAND";
        e[e.CODE_DEFINE_COMMAND_OBJ = 50] = "CODE_DEFINE_COMMAND_OBJ";
        e[e.CODE_CHARACTER_SET = 51] = "CODE_CHARACTER_SET";
        e[e.CODE_FONT_REF = 52] = "CODE_FONT_REF";
        e[e.CODE_DEFINE_FUNCTION = 53] = "CODE_DEFINE_FUNCTION";
        e[e.CODE_PLACE_FUNCTION = 54] = "CODE_PLACE_FUNCTION";
        e[e.CODE_GEN_TAG_OBJECTS = 55] = "CODE_GEN_TAG_OBJECTS";
        e[e.CODE_EXPORT_ASSETS = 56] = "CODE_EXPORT_ASSETS";
        e[e.CODE_IMPORT_ASSETS = 57] = "CODE_IMPORT_ASSETS";
        e[e.CODE_ENABLE_DEBUGGER = 58] = "CODE_ENABLE_DEBUGGER";
        e[e.CODE_DO_INIT_ACTION = 59] = "CODE_DO_INIT_ACTION";
        e[e.CODE_DEFINE_VIDEO_STREAM = 60] = "CODE_DEFINE_VIDEO_STREAM";
        e[e.CODE_VIDEO_FRAME = 61] = "CODE_VIDEO_FRAME";
        e[e.CODE_DEFINE_FONT_INFO2 = 62] = "CODE_DEFINE_FONT_INFO2";
        e[e.CODE_DEBUG_ID = 63] = "CODE_DEBUG_ID";
        e[e.CODE_ENABLE_DEBUGGER2 = 64] = "CODE_ENABLE_DEBUGGER2";
        e[e.CODE_SCRIPT_LIMITS = 65] = "CODE_SCRIPT_LIMITS";
        e[e.CODE_SET_TAB_INDEX = 66] = "CODE_SET_TAB_INDEX";
        e[e.CODE_FILE_ATTRIBUTES = 69] = "CODE_FILE_ATTRIBUTES";
        e[e.CODE_PLACE_OBJECT3 = 70] = "CODE_PLACE_OBJECT3";
        e[e.CODE_IMPORT_ASSETS2 = 71] = "CODE_IMPORT_ASSETS2";
        e[e.CODE_DO_ABC_ = 72] = "CODE_DO_ABC_";
        e[e.CODE_DEFINE_FONT_ALIGN_ZONES = 73] = "CODE_DEFINE_FONT_ALIGN_ZONES";
        e[e.CODE_CSM_TEXT_SETTINGS = 74] = "CODE_CSM_TEXT_SETTINGS";
        e[e.CODE_DEFINE_FONT3 = 75] = "CODE_DEFINE_FONT3";
        e[e.CODE_SYMBOL_CLASS = 76] = "CODE_SYMBOL_CLASS";
        e[e.CODE_METADATA = 77] = "CODE_METADATA";
        e[e.CODE_DEFINE_SCALING_GRID = 78] = "CODE_DEFINE_SCALING_GRID";
        e[e.CODE_DO_ABC = 82] = "CODE_DO_ABC";
        e[e.CODE_DEFINE_SHAPE4 = 83] = "CODE_DEFINE_SHAPE4";
        e[e.CODE_DEFINE_MORPH_SHAPE2 = 84] = "CODE_DEFINE_MORPH_SHAPE2";
        e[e.CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA = 86] = "CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA";
        e[e.CODE_DEFINE_BINARY_DATA = 87] = "CODE_DEFINE_BINARY_DATA";
        e[e.CODE_DEFINE_FONT_NAME = 88] = "CODE_DEFINE_FONT_NAME";
        e[e.CODE_START_SOUND2 = 89] = "CODE_START_SOUND2";
        e[e.CODE_DEFINE_BITS_JPEG4 = 90] = "CODE_DEFINE_BITS_JPEG4";
        e[e.CODE_DEFINE_FONT4 = 91] = "CODE_DEFINE_FONT4";
      })(d.SwfTag || (d.SwfTag = {}));
      (function(e) {
        e[e.Reserved = 2048] = "Reserved";
        e[e.OpaqueBackground = 1024] = "OpaqueBackground";
        e[e.HasVisible = 512] = "HasVisible";
        e[e.HasImage = 256] = "HasImage";
        e[e.HasClassName = 2048] = "HasClassName";
        e[e.HasCacheAsBitmap = 1024] = "HasCacheAsBitmap";
        e[e.HasBlendMode = 512] = "HasBlendMode";
        e[e.HasFilterList = 256] = "HasFilterList";
        e[e.HasClipActions = 128] = "HasClipActions";
        e[e.HasClipDepth = 64] = "HasClipDepth";
        e[e.HasName = 32] = "HasName";
        e[e.HasRatio = 16] = "HasRatio";
        e[e.HasColorTransform = 8] = "HasColorTransform";
        e[e.HasMatrix = 4] = "HasMatrix";
        e[e.HasCharacter = 2] = "HasCharacter";
        e[e.Move = 1] = "Move";
      })(d.PlaceObjectFlags || (d.PlaceObjectFlags = {}));
    })(d.Parser || (d.Parser = {}));
  })(d.SWF || (d.SWF = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  var m = d.Debug.unexpected, s = function() {
    function e(e, k, a, c) {
      this.url = e;
      this.method = k;
      this.mimeType = a;
      this.data = c;
    }
    e.prototype.readAll = function(e, k) {
      var a = this.url, c = new XMLHttpRequest({mozSystem:!0});
      c.open(this.method || "GET", this.url, !0);
      c.responseType = "arraybuffer";
      e && (c.onprogress = function(a) {
        e(c.response, a.loaded, a.total);
      });
      c.onreadystatechange = function(g) {
        4 === c.readyState && (200 !== c.status && 0 !== c.status || null === c.response ? (m("Path: " + a + " not found."), k(null, c.statusText)) : k(c.response));
      };
      this.mimeType && c.setRequestHeader("Content-Type", this.mimeType);
      c.send(this.data || null);
    };
    e.prototype.readAsync = function(e, k, a, c, g) {
      var p = new XMLHttpRequest({mozSystem:!0}), d = this.url, u = 0, l = 0;
      p.open(this.method || "GET", d, !0);
      p.responseType = "moz-chunked-arraybuffer";
      var r = "moz-chunked-arraybuffer" !== p.responseType;
      r && (p.responseType = "arraybuffer");
      p.onprogress = function(b) {
        r || (u = b.loaded, l = b.total, e(new Uint8Array(p.response), {loaded:u, total:l}));
      };
      p.onreadystatechange = function(b) {
        2 === p.readyState && g && g(d, p.status, p.getAllResponseHeaders());
        4 === p.readyState && (200 !== p.status && 0 !== p.status || null === p.response && (0 === l || u !== l) ? k(p.statusText) : (r && (b = p.response, e(new Uint8Array(b), {loaded:0, total:b.byteLength})), c && c()));
      };
      this.mimeType && p.setRequestHeader("Content-Type", this.mimeType);
      p.send(this.data || null);
      a && a();
    };
    return e;
  }();
  d.BinaryFileReader = s;
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      d[d.Objects = 0] = "Objects";
      d[d.References = 1] = "References";
    })(d.RemotingPhase || (d.RemotingPhase = {}));
    (function(d) {
      d[d.HasMatrix = 1] = "HasMatrix";
      d[d.HasBounds = 2] = "HasBounds";
      d[d.HasChildren = 4] = "HasChildren";
      d[d.HasColorTransform = 8] = "HasColorTransform";
      d[d.HasClipRect = 16] = "HasClipRect";
      d[d.HasMiscellaneousProperties = 32] = "HasMiscellaneousProperties";
      d[d.HasMask = 64] = "HasMask";
      d[d.HasClip = 128] = "HasClip";
    })(d.MessageBits || (d.MessageBits = {}));
    (function(d) {
      d[d.None = 0] = "None";
      d[d.Asset = 134217728] = "Asset";
    })(d.IDMask || (d.IDMask = {}));
    (function(d) {
      d[d.EOF = 0] = "EOF";
      d[d.UpdateFrame = 100] = "UpdateFrame";
      d[d.UpdateGraphics = 101] = "UpdateGraphics";
      d[d.UpdateBitmapData = 102] = "UpdateBitmapData";
      d[d.UpdateTextContent = 103] = "UpdateTextContent";
      d[d.UpdateStage = 104] = "UpdateStage";
      d[d.UpdateNetStream = 105] = "UpdateNetStream";
      d[d.RequestBitmapData = 106] = "RequestBitmapData";
      d[d.DecodeImage = 107] = "DecodeImage";
      d[d.DecodeImageResponse = 108] = "DecodeImageResponse";
      d[d.RegisterFont = 200] = "RegisterFont";
      d[d.DrawToBitmap = 201] = "DrawToBitmap";
      d[d.MouseEvent = 300] = "MouseEvent";
      d[d.KeyboardEvent = 301] = "KeyboardEvent";
      d[d.FocusEvent = 302] = "FocusEvent";
    })(d.MessageTag || (d.MessageTag = {}));
    (function(d) {
      d[d.Identity = 0] = "Identity";
      d[d.AlphaMultiplierOnly = 1] = "AlphaMultiplierOnly";
      d[d.All = 2] = "All";
    })(d.ColorTransformEncoding || (d.ColorTransformEncoding = {}));
    d.MouseEventNames = ["click", "dblclick", "mousedown", "mousemove", "mouseup"];
    d.KeyboardEventNames = ["keydown", "keypress", "keyup"];
    (function(d) {
      d[d.CtrlKey = 1] = "CtrlKey";
      d[d.AltKey = 2] = "AltKey";
      d[d.ShiftKey = 4] = "ShiftKey";
    })(d.KeyboardEventFlags || (d.KeyboardEventFlags = {}));
    (function(d) {
      d[d.DocumentHidden = 0] = "DocumentHidden";
      d[d.DocumentVisible = 1] = "DocumentVisible";
      d[d.WindowBlur = 2] = "WindowBlur";
      d[d.WindowFocus = 3] = "WindowFocus";
    })(d.FocusEventType || (d.FocusEventType = {}));
  })(d.Remoting || (d.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      var e = function() {
        function k() {
        }
        k.toRGBA = function(a, c, g, p) {
          "undefined" === typeof p && (p = 1);
          return "rgba(" + a + "," + c + "," + g + "," + p + ")";
        };
        return k;
      }();
      d.UI = e;
      var m = function() {
        function k() {
        }
        k.prototype.tabToolbar = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(37, 44, 51, a);
        };
        k.prototype.toolbars = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(52, 60, 69, a);
        };
        k.prototype.selectionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(29, 79, 115, a);
        };
        k.prototype.selectionText = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(245, 247, 250, a);
        };
        k.prototype.splitters = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(0, 0, 0, a);
        };
        k.prototype.bodyBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(17, 19, 21, a);
        };
        k.prototype.sidebarBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(24, 29, 32, a);
        };
        k.prototype.attentionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(161, 134, 80, a);
        };
        k.prototype.bodyText = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(143, 161, 178, a);
        };
        k.prototype.foregroundTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(182, 186, 191, a);
        };
        k.prototype.contentTextHighContrast = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(169, 186, 203, a);
        };
        k.prototype.contentTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(143, 161, 178, a);
        };
        k.prototype.contentTextDarkGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(95, 115, 135, a);
        };
        k.prototype.blueHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(70, 175, 227, a);
        };
        k.prototype.purpleHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(107, 122, 187, a);
        };
        k.prototype.pinkHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(223, 128, 255, a);
        };
        k.prototype.redHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(235, 83, 104, a);
        };
        k.prototype.orangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(217, 102, 41, a);
        };
        k.prototype.lightOrangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(217, 155, 40, a);
        };
        k.prototype.greenHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(112, 191, 83, a);
        };
        k.prototype.blueGreyHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(94, 136, 176, a);
        };
        return k;
      }();
      d.UIThemeDark = m;
      m = function() {
        function k() {
        }
        k.prototype.tabToolbar = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(235, 236, 237, a);
        };
        k.prototype.toolbars = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(240, 241, 242, a);
        };
        k.prototype.selectionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(76, 158, 217, a);
        };
        k.prototype.selectionText = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(245, 247, 250, a);
        };
        k.prototype.splitters = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(170, 170, 170, a);
        };
        k.prototype.bodyBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(252, 252, 252, a);
        };
        k.prototype.sidebarBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(247, 247, 247, a);
        };
        k.prototype.attentionBackground = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(161, 134, 80, a);
        };
        k.prototype.bodyText = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(24, 25, 26, a);
        };
        k.prototype.foregroundTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(88, 89, 89, a);
        };
        k.prototype.contentTextHighContrast = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(41, 46, 51, a);
        };
        k.prototype.contentTextGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(143, 161, 178, a);
        };
        k.prototype.contentTextDarkGrey = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(102, 115, 128, a);
        };
        k.prototype.blueHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(0, 136, 204, a);
        };
        k.prototype.purpleHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(91, 95, 255, a);
        };
        k.prototype.pinkHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(184, 46, 229, a);
        };
        k.prototype.redHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(237, 38, 85, a);
        };
        k.prototype.orangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(241, 60, 0, a);
        };
        k.prototype.lightOrangeHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(217, 126, 0, a);
        };
        k.prototype.greenHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(44, 187, 15, a);
        };
        k.prototype.blueGreyHighlight = function(a) {
          "undefined" === typeof a && (a = 1);
          return e.toRGBA(95, 136, 176, a);
        };
        return k;
      }();
      d.UIThemeLight = m;
    })(d.Theme || (d.Theme = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      var e = function() {
        function d(k) {
          this._buffers = k || [];
          this._snapshots = [];
          this._maxDepth = 0;
        }
        d.prototype.addBuffer = function(d) {
          this._buffers.push(d);
        };
        d.prototype.getSnapshotAt = function(d) {
          return this._snapshots[d];
        };
        Object.defineProperty(d.prototype, "hasSnapshots", {get:function() {
          return 0 < this.snapshotCount;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "snapshotCount", {get:function() {
          return this._snapshots.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "startTime", {get:function() {
          return this._startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "endTime", {get:function() {
          return this._endTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "windowStart", {get:function() {
          return this._windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "windowEnd", {get:function() {
          return this._windowEnd;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "windowLength", {get:function() {
          return this.windowEnd - this.windowStart;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "maxDepth", {get:function() {
          return this._maxDepth;
        }, enumerable:!0, configurable:!0});
        d.prototype.forEachSnapshot = function(d) {
          for (var a = 0, c = this.snapshotCount;a < c;a++) {
            d(this._snapshots[a], a);
          }
        };
        d.prototype.createSnapshots = function() {
          var d = Number.MAX_VALUE, a = Number.MIN_VALUE, c = 0;
          for (this._snapshots = [];0 < this._buffers.length;) {
            var g = this._buffers.shift().createSnapshot();
            g && (d > g.startTime && (d = g.startTime), a < g.endTime && (a = g.endTime), c < g.maxDepth && (c = g.maxDepth), this._snapshots.push(g));
          }
          this._startTime = d;
          this._endTime = a;
          this._windowStart = d;
          this._windowEnd = a;
          this._maxDepth = c;
        };
        d.prototype.setWindow = function(d, a) {
          if (d > a) {
            var c = d;
            d = a;
            a = c;
          }
          c = Math.min(a - d, this.totalTime);
          d < this._startTime ? (d = this._startTime, a = this._startTime + c) : a > this._endTime && (d = this._endTime - c, a = this._endTime);
          this._windowStart = d;
          this._windowEnd = a;
        };
        d.prototype.moveWindowTo = function(d) {
          this.setWindow(d - this.windowLength / 2, d + this.windowLength / 2);
        };
        return d;
      }();
      d.Profile = e;
    })(d.Profiler || (d.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function(d, m) {
  function s() {
    this.constructor = d;
  }
  for (var e in m) {
    m.hasOwnProperty(e) && (d[e] = m[e]);
  }
  s.prototype = m.prototype;
  d.prototype = new s;
};
(function(d) {
  (function(d) {
    (function(d) {
      var e = function() {
        return function(d) {
          this.kind = d;
          this.totalTime = this.selfTime = this.count = 0;
        };
      }();
      d.TimelineFrameStatistics = e;
      var m = function() {
        function d(a, c, g, p, e, k) {
          this.parent = a;
          this.kind = c;
          this.startData = g;
          this.endData = p;
          this.startTime = e;
          this.endTime = k;
          this.maxDepth = 0;
        }
        Object.defineProperty(d.prototype, "totalTime", {get:function() {
          return this.endTime - this.startTime;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(d.prototype, "selfTime", {get:function() {
          var a = this.totalTime;
          if (this.children) {
            for (var c = 0, g = this.children.length;c < g;c++) {
              var p = this.children[c], a = a - (p.endTime - p.startTime)
            }
          }
          return a;
        }, enumerable:!0, configurable:!0});
        d.prototype.getChildIndex = function(a) {
          for (var c = this.children, g = 0;g < c.length;g++) {
            if (c[g].endTime > a) {
              return g;
            }
          }
          return 0;
        };
        d.prototype.getChildRange = function(a, c) {
          if (this.children && a <= this.endTime && c >= this.startTime && c >= a) {
            var g = this._getNearestChild(a), p = this._getNearestChildReverse(c);
            if (g <= p) {
              return a = this.children[g].startTime, c = this.children[p].endTime, {startIndex:g, endIndex:p, startTime:a, endTime:c, totalTime:c - a};
            }
          }
          return null;
        };
        d.prototype._getNearestChild = function(a) {
          var c = this.children;
          if (c && c.length) {
            if (a <= c[0].endTime) {
              return 0;
            }
            for (var g, p = 0, d = c.length - 1;d > p;) {
              g = (p + d) / 2 | 0;
              var e = c[g];
              if (a >= e.startTime && a <= e.endTime) {
                return g;
              }
              a > e.endTime ? p = g + 1 : d = g;
            }
            return Math.ceil((p + d) / 2);
          }
          return 0;
        };
        d.prototype._getNearestChildReverse = function(a) {
          var c = this.children;
          if (c && c.length) {
            var g = c.length - 1;
            if (a >= c[g].startTime) {
              return g;
            }
            for (var p, d = 0;g > d;) {
              p = Math.ceil((d + g) / 2);
              var e = c[p];
              if (a >= e.startTime && a <= e.endTime) {
                return p;
              }
              a > e.endTime ? d = p : g = p - 1;
            }
            return(d + g) / 2 | 0;
          }
          return 0;
        };
        d.prototype.query = function(a) {
          if (a < this.startTime || a > this.endTime) {
            return null;
          }
          var c = this.children;
          if (c && 0 < c.length) {
            for (var g, p = 0, d = c.length - 1;d > p;) {
              var e = (p + d) / 2 | 0;
              g = c[e];
              if (a >= g.startTime && a <= g.endTime) {
                return g.query(a);
              }
              a > g.endTime ? p = e + 1 : d = e;
            }
            g = c[d];
            if (a >= g.startTime && a <= g.endTime) {
              return g.query(a);
            }
          }
          return this;
        };
        d.prototype.queryNext = function(a) {
          for (var c = this;a > c.endTime;) {
            if (c.parent) {
              c = c.parent;
            } else {
              break;
            }
          }
          return c.query(a);
        };
        d.prototype.getDepth = function() {
          for (var a = 0, c = this;c;) {
            a++, c = c.parent;
          }
          return a;
        };
        d.prototype.calculateStatistics = function() {
          function a(g) {
            if (g.kind) {
              var p = c[g.kind.id] || (c[g.kind.id] = new e(g.kind));
              p.count++;
              p.selfTime += g.selfTime;
              p.totalTime += g.totalTime;
            }
            g.children && g.children.forEach(a);
          }
          var c = this.statistics = [];
          a(this);
        };
        return d;
      }();
      d.TimelineFrame = m;
      m = function(d) {
        function a(a) {
          d.call(this, null, null, null, null, NaN, NaN);
          this.name = a;
        }
        __extends(a, d);
        return a;
      }(m);
      d.TimelineBufferSnapshot = m;
    })(d.Profiler || (d.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(m) {
      var e = d.ObjectUtilities.createEmptyObject, t = function() {
        function k(a, c) {
          "undefined" === typeof a && (a = "");
          this.name = a || "";
          this._startTime = d.isNullOrUndefined(c) ? performance.now() : c;
        }
        k.prototype.getKind = function(a) {
          return this._kinds[a];
        };
        Object.defineProperty(k.prototype, "kinds", {get:function() {
          return this._kinds.concat();
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(k.prototype, "depth", {get:function() {
          return this._depth;
        }, enumerable:!0, configurable:!0});
        k.prototype._initialize = function() {
          this._depth = 0;
          this._stack = [];
          this._data = [];
          this._kinds = [];
          this._kindNameMap = e();
          this._marks = new d.CircularBuffer(Int32Array, 20);
          this._times = new d.CircularBuffer(Float64Array, 20);
        };
        k.prototype._getKindId = function(a) {
          var c = k.MAX_KINDID;
          if (void 0 === this._kindNameMap[a]) {
            if (c = this._kinds.length, c < k.MAX_KINDID) {
              var g = {id:c, name:a, visible:!0};
              this._kinds.push(g);
              this._kindNameMap[a] = g;
            } else {
              c = k.MAX_KINDID;
            }
          } else {
            c = this._kindNameMap[a].id;
          }
          return c;
        };
        k.prototype._getMark = function(a, c, g) {
          var p = k.MAX_DATAID;
          d.isNullOrUndefined(g) || c === k.MAX_KINDID || (p = this._data.length, p < k.MAX_DATAID ? this._data.push(g) : p = k.MAX_DATAID);
          return a | p << 16 | c;
        };
        k.prototype.enter = function(a, c, g) {
          g = (d.isNullOrUndefined(g) ? performance.now() : g) - this._startTime;
          this._marks || this._initialize();
          this._depth++;
          a = this._getKindId(a);
          this._marks.write(this._getMark(k.ENTER, a, c));
          this._times.write(g);
          this._stack.push(a);
        };
        k.prototype.leave = function(a, c, g) {
          g = (d.isNullOrUndefined(g) ? performance.now() : g) - this._startTime;
          var p = this._stack.pop();
          a && (p = this._getKindId(a));
          this._marks.write(this._getMark(k.LEAVE, p, c));
          this._times.write(g);
          this._depth--;
        };
        k.prototype.count = function(a, c, g) {
        };
        k.prototype.createSnapshot = function() {
          var a;
          "undefined" === typeof a && (a = Number.MAX_VALUE);
          if (!this._marks) {
            return null;
          }
          var c = this._times, g = this._kinds, p = this._data, e = new m.TimelineBufferSnapshot(this.name), u = [e], l = 0;
          this._marks || this._initialize();
          this._marks.forEachInReverse(function(e, b) {
            var h = p[e >>> 16 & k.MAX_DATAID], v = g[e & k.MAX_KINDID];
            if (d.isNullOrUndefined(v) || v.visible) {
              var n = e & 2147483648, f = c.get(b), q = u.length;
              if (n === k.LEAVE) {
                if (1 === q && (l++, l > a)) {
                  return!0;
                }
                u.push(new m.TimelineFrame(u[q - 1], v, null, h, NaN, f));
              } else {
                if (n === k.ENTER) {
                  if (v = u.pop(), n = u[u.length - 1]) {
                    for (n.children ? n.children.unshift(v) : n.children = [v], n = u.length, v.depth = n, v.startData = h, v.startTime = f;v;) {
                      if (v.maxDepth < n) {
                        v.maxDepth = n, v = v.parent;
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
          e.children && e.children.length && (e.startTime = e.children[0].startTime, e.endTime = e.children[e.children.length - 1].endTime);
          return e;
        };
        k.prototype.reset = function(a) {
          this._startTime = d.isNullOrUndefined(a) ? performance.now() : a;
          this._marks ? (this._depth = 0, this._data = [], this._marks.reset(), this._times.reset()) : this._initialize();
        };
        k.FromFirefoxProfile = function(a, c) {
          for (var g = a.profile.threads[0].samples, p = new k(c, g[0].time), d = [], e, l = 0;l < g.length;l++) {
            e = g[l];
            var r = e.time, b = e.frames, h = 0;
            for (e = Math.min(b.length, d.length);h < e && b[h].location === d[h].location;) {
              h++;
            }
            for (var w = d.length - h, n = 0;n < w;n++) {
              e = d.pop(), p.leave(e.location, null, r);
            }
            for (;h < b.length;) {
              e = b[h++], p.enter(e.location, null, r);
            }
            d = b;
          }
          for (;e = d.pop();) {
            p.leave(e.location, null, r);
          }
          return p;
        };
        k.FromChromeProfile = function(a, c) {
          var g = a.timestamps, p = a.samples, d = new k(c, g[0] / 1E3), e = [], l = {}, r;
          k._resolveIds(a.head, l);
          for (var b = 0;b < g.length;b++) {
            var h = g[b] / 1E3, w = [];
            for (r = l[p[b]];r;) {
              w.unshift(r), r = r.parent;
            }
            var n = 0;
            for (r = Math.min(w.length, e.length);n < r && w[n] === e[n];) {
              n++;
            }
            for (var f = e.length - n, q = 0;q < f;q++) {
              r = e.pop(), d.leave(r.functionName, null, h);
            }
            for (;n < w.length;) {
              r = w[n++], d.enter(r.functionName, null, h);
            }
            e = w;
          }
          for (;r = e.pop();) {
            d.leave(r.functionName, null, h);
          }
          return d;
        };
        k._resolveIds = function(a, c) {
          c[a.id] = a;
          if (a.children) {
            for (var g = 0;g < a.children.length;g++) {
              a.children[g].parent = a, k._resolveIds(a.children[g], c);
            }
          }
        };
        k.ENTER = 0;
        k.LEAVE = -2147483648;
        k.MAX_KINDID = 65535;
        k.MAX_DATAID = 32767;
        return k;
      }();
      m.TimelineBuffer = t;
    })(m.Profiler || (m.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      (function(d) {
        d[d.DARK = 0] = "DARK";
        d[d.LIGHT = 1] = "LIGHT";
      })(s.UIThemeType || (s.UIThemeType = {}));
      var e = function() {
        function e(d, a) {
          "undefined" === typeof a && (a = 0);
          this._container = d;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = a;
          this._tooltip = this._createTooltip();
        }
        e.prototype.createProfile = function(d, a) {
          "undefined" === typeof a && (a = !0);
          var c = new s.Profile(d);
          c.createSnapshots();
          this._profiles.push(c);
          a && this.activateProfile(c);
          return c;
        };
        e.prototype.activateProfile = function(d) {
          this.deactivateProfile();
          this._activeProfile = d;
          this._createViews();
          this._initializeViews();
        };
        e.prototype.activateProfileAt = function(d) {
          this.activateProfile(this.getProfileAt(d));
        };
        e.prototype.deactivateProfile = function() {
          this._activeProfile && (this._destroyViews(), this._activeProfile = null);
        };
        e.prototype.resize = function() {
          this._onResize();
        };
        e.prototype.getProfileAt = function(d) {
          return this._profiles[d];
        };
        Object.defineProperty(e.prototype, "activeProfile", {get:function() {
          return this._activeProfile;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(e.prototype, "profileCount", {get:function() {
          return this._profiles.length;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(e.prototype, "container", {get:function() {
          return this._container;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(e.prototype, "themeType", {get:function() {
          return this._themeType;
        }, set:function(d) {
          switch(d) {
            case 0:
              this._theme = new m.Theme.UIThemeDark;
              break;
            case 1:
              this._theme = new m.Theme.UIThemeLight;
          }
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(e.prototype, "theme", {get:function() {
          return this._theme;
        }, enumerable:!0, configurable:!0});
        e.prototype.getSnapshotAt = function(d) {
          return this._activeProfile.getSnapshotAt(d);
        };
        e.prototype._createViews = function() {
          if (this._activeProfile) {
            var d = this;
            this._overviewHeader = new s.FlameChartHeader(this, 0);
            this._overview = new s.FlameChartOverview(this, 0);
            this._activeProfile.forEachSnapshot(function(a, c) {
              d._headers.push(new s.FlameChartHeader(d, 1));
              d._charts.push(new s.FlameChart(d, a));
            });
            window.addEventListener("resize", this._onResize.bind(this));
          }
        };
        e.prototype._destroyViews = function() {
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
        e.prototype._initializeViews = function() {
          if (this._activeProfile) {
            var d = this, a = this._activeProfile.startTime, c = this._activeProfile.endTime;
            this._overviewHeader.initialize(a, c);
            this._overview.initialize(a, c);
            this._activeProfile.forEachSnapshot(function(g, p) {
              d._headers[p].initialize(a, c);
              d._charts[p].initialize(a, c);
            });
          }
        };
        e.prototype._onResize = function() {
          if (this._activeProfile) {
            var d = this, a = this._container.offsetWidth;
            this._overviewHeader.setSize(a);
            this._overview.setSize(a);
            this._activeProfile.forEachSnapshot(function(c, g) {
              d._headers[g].setSize(a);
              d._charts[g].setSize(a);
            });
          }
        };
        e.prototype._updateViews = function() {
          if (this._activeProfile) {
            var d = this, a = this._activeProfile.windowStart, c = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(a, c);
            this._overview.setWindow(a, c);
            this._activeProfile.forEachSnapshot(function(g, p) {
              d._headers[p].setWindow(a, c);
              d._charts[p].setWindow(a, c);
            });
          }
        };
        e.prototype._drawViews = function() {
        };
        e.prototype._createTooltip = function() {
          var d = document.createElement("div");
          d.classList.add("profiler-tooltip");
          d.style.display = "none";
          this._container.insertBefore(d, this._container.firstChild);
          return d;
        };
        e.prototype.setWindow = function(d, a) {
          this._activeProfile.setWindow(d, a);
          this._updateViews();
        };
        e.prototype.moveWindowTo = function(d) {
          this._activeProfile.moveWindowTo(d);
          this._updateViews();
        };
        e.prototype.showTooltip = function(d, a, c, g) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(d, a));
          this._tooltip.style.display = "block";
          var p = this._tooltip.firstChild;
          a = p.clientWidth;
          p = p.clientHeight;
          c += c + a >= d.canvas.clientWidth - 50 ? -(a + 20) : 25;
          g += d.canvas.offsetTop - p / 2;
          this._tooltip.style.left = c + "px";
          this._tooltip.style.top = g + "px";
        };
        e.prototype.hideTooltip = function() {
          this._tooltip.style.display = "none";
        };
        e.prototype.createTooltipContent = function(d, a) {
          var c = Math.round(1E5 * a.totalTime) / 1E5, g = Math.round(1E5 * a.selfTime) / 1E5, p = Math.round(1E4 * a.selfTime / a.totalTime) / 100, e = document.createElement("div"), u = document.createElement("h1");
          u.textContent = a.kind.name;
          e.appendChild(u);
          u = document.createElement("p");
          u.textContent = "Total: " + c + " ms";
          e.appendChild(u);
          c = document.createElement("p");
          c.textContent = "Self: " + g + " ms (" + p + "%)";
          e.appendChild(c);
          if (g = d.getStatistics(a.kind)) {
            p = document.createElement("p"), p.textContent = "Count: " + g.count, e.appendChild(p), p = Math.round(1E5 * g.totalTime) / 1E5, c = document.createElement("p"), c.textContent = "All Total: " + p + " ms", e.appendChild(c), g = Math.round(1E5 * g.selfTime) / 1E5, p = document.createElement("p"), p.textContent = "All Self: " + g + " ms", e.appendChild(p);
          }
          this.appendDataElements(e, a.startData);
          this.appendDataElements(e, a.endData);
          return e;
        };
        e.prototype.appendDataElements = function(e, a) {
          if (!d.isNullOrUndefined(a)) {
            e.appendChild(document.createElement("hr"));
            var c;
            if (d.isObject(a)) {
              for (var g in a) {
                c = document.createElement("p"), c.textContent = g + ": " + a[g], e.appendChild(c);
              }
            } else {
              c = document.createElement("p"), c.textContent = a.toString(), e.appendChild(c);
            }
          }
        };
        e.prototype.removeTooltipContent = function() {
          for (var d = this._tooltip;d.firstChild;) {
            d.removeChild(d.firstChild);
          }
        };
        return e;
      }();
      s.Controller = e;
    })(m.Profiler || (m.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(m) {
      var e = d.NumberUtilities.clamp, t = function() {
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
      m.MouseCursor = t;
      var k = function() {
        function a(a, g) {
          this._target = a;
          this._eventTarget = g;
          this._wheelDisabled = !1;
          this._boundOnMouseDown = this._onMouseDown.bind(this);
          this._boundOnMouseUp = this._onMouseUp.bind(this);
          this._boundOnMouseOver = this._onMouseOver.bind(this);
          this._boundOnMouseOut = this._onMouseOut.bind(this);
          this._boundOnMouseMove = this._onMouseMove.bind(this);
          this._boundOnMouseWheel = this._onMouseWheel.bind(this);
          this._boundOnDrag = this._onDrag.bind(this);
          g.addEventListener("mousedown", this._boundOnMouseDown, !1);
          g.addEventListener("mouseover", this._boundOnMouseOver, !1);
          g.addEventListener("mouseout", this._boundOnMouseOut, !1);
          g.addEventListener("onwheel" in document ? "wheel" : "mousewheel", this._boundOnMouseWheel, !1);
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
            var g = this._eventTarget.parentElement;
            a._cursor !== c && (a._cursor = c, ["", "-moz-", "-webkit-"].forEach(function(a) {
              g.style.cursor = a + c;
            }));
            a._cursorOwner = a._cursor === t.DEFAULT ? null : this._target;
          }
        };
        a.prototype._onMouseDown = function(a) {
          this._killHoverCheck();
          if (0 === a.button) {
            var g = this._getTargetMousePos(a, a.target);
            this._dragInfo = {start:g, current:g, delta:{x:0, y:0}, hasMoved:!1, originalTarget:a.target};
            window.addEventListener("mousemove", this._boundOnDrag, !1);
            window.addEventListener("mouseup", this._boundOnMouseUp, !1);
            this._target.onMouseDown(g.x, g.y);
          }
        };
        a.prototype._onDrag = function(a) {
          var g = this._dragInfo;
          a = this._getTargetMousePos(a, g.originalTarget);
          var p = {x:a.x - g.start.x, y:a.y - g.start.y};
          g.current = a;
          g.delta = p;
          g.hasMoved = !0;
          this._target.onDrag(g.start.x, g.start.y, a.x, a.y, p.x, p.y);
        };
        a.prototype._onMouseUp = function(a) {
          window.removeEventListener("mousemove", this._boundOnDrag);
          window.removeEventListener("mouseup", this._boundOnMouseUp);
          var g = this;
          a = this._dragInfo;
          if (a.hasMoved) {
            this._target.onDragEnd(a.start.x, a.start.y, a.current.x, a.current.y, a.delta.x, a.delta.y);
          } else {
            this._target.onClick(a.current.x, a.current.y);
          }
          this._dragInfo = null;
          this._wheelDisabled = !0;
          setTimeout(function() {
            g._wheelDisabled = !1;
          }, 500);
        };
        a.prototype._onMouseOver = function(a) {
          a.target.addEventListener("mousemove", this._boundOnMouseMove, !1);
          if (!this._dragInfo) {
            var g = this._getTargetMousePos(a, a.target);
            this._target.onMouseOver(g.x, g.y);
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
            var g = this._getTargetMousePos(a, a.target);
            this._target.onMouseMove(g.x, g.y);
            this._killHoverCheck();
            this._startHoverCheck(a);
          }
        };
        a.prototype._onMouseWheel = function(a) {
          if (!(a.altKey || a.metaKey || a.ctrlKey || a.shiftKey || (a.preventDefault(), this._dragInfo || this._wheelDisabled))) {
            var g = this._getTargetMousePos(a, a.target);
            a = e("undefined" !== typeof a.deltaY ? a.deltaY / 16 : -a.wheelDelta / 40, -1, 1);
            this._target.onMouseWheel(g.x, g.y, Math.pow(1.2, a) - 1);
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
        a.prototype._getTargetMousePos = function(a, g) {
          var p = g.getBoundingClientRect();
          return{x:a.clientX - p.left, y:a.clientY - p.top};
        };
        a.HOVER_TIMEOUT = 500;
        a._cursor = t.DEFAULT;
        return a;
      }();
      m.MouseController = k;
    })(m.Profiler || (m.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      (function(d) {
        d[d.NONE = 0] = "NONE";
        d[d.WINDOW = 1] = "WINDOW";
        d[d.HANDLE_LEFT = 2] = "HANDLE_LEFT";
        d[d.HANDLE_RIGHT = 3] = "HANDLE_RIGHT";
        d[d.HANDLE_BOTH = 4] = "HANDLE_BOTH";
      })(d.FlameChartDragTarget || (d.FlameChartDragTarget = {}));
      var e = function() {
        function e(k) {
          this._controller = k;
          this._initialized = !1;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
          this._mouseController = new d.MouseController(this, this._canvas);
          k = k.container;
          k.appendChild(this._canvas);
          k = k.getBoundingClientRect();
          this.setSize(k.width);
        }
        Object.defineProperty(e.prototype, "canvas", {get:function() {
          return this._canvas;
        }, enumerable:!0, configurable:!0});
        e.prototype.setSize = function(d, a) {
          "undefined" === typeof a && (a = 20);
          this._width = d;
          this._height = a;
          this._resetCanvas();
          this.draw();
        };
        e.prototype.initialize = function(d, a) {
          this._initialized = !0;
          this.setRange(d, a);
          this.setWindow(d, a, !1);
          this.draw();
        };
        e.prototype.setWindow = function(d, a, c) {
          "undefined" === typeof c && (c = !0);
          this._windowStart = d;
          this._windowEnd = a;
          !c || this.draw();
        };
        e.prototype.setRange = function(d, a) {
          var c = !1;
          "undefined" === typeof c && (c = !0);
          this._rangeStart = d;
          this._rangeEnd = a;
          !c || this.draw();
        };
        e.prototype.destroy = function() {
          this._mouseController.destroy();
          this._mouseController = null;
          this._controller.container.removeChild(this._canvas);
          this._controller = null;
        };
        e.prototype._resetCanvas = function() {
          var d = window.devicePixelRatio, a = this._canvas;
          a.width = this._width * d;
          a.height = this._height * d;
          a.style.width = this._width + "px";
          a.style.height = this._height + "px";
        };
        e.prototype.draw = function() {
        };
        e.prototype._almostEq = function(d, a) {
          var c;
          "undefined" === typeof c && (c = 10);
          return Math.abs(d - a) < 1 / Math.pow(10, c);
        };
        e.prototype._windowEqRange = function() {
          return this._almostEq(this._windowStart, this._rangeStart) && this._almostEq(this._windowEnd, this._rangeEnd);
        };
        e.prototype._decimalPlaces = function(d) {
          return(+d).toFixed(10).replace(/^-?\d*\.?|0+$/g, "").length;
        };
        e.prototype._toPixelsRelative = function(d) {
          return 0;
        };
        e.prototype._toPixels = function(d) {
          return 0;
        };
        e.prototype._toTimeRelative = function(d) {
          return 0;
        };
        e.prototype._toTime = function(d) {
          return 0;
        };
        e.prototype.onMouseWheel = function(d, a, c) {
          d = this._toTime(d);
          a = this._windowStart;
          var g = this._windowEnd, p = g - a;
          c = Math.max((e.MIN_WINDOW_LEN - p) / p, c);
          this._controller.setWindow(a + (a - d) * c, g + (g - d) * c);
          this.onHoverEnd();
        };
        e.prototype.onMouseDown = function(d, a) {
        };
        e.prototype.onMouseMove = function(d, a) {
        };
        e.prototype.onMouseOver = function(d, a) {
        };
        e.prototype.onMouseOut = function() {
        };
        e.prototype.onDrag = function(d, a, c, g, p, e) {
        };
        e.prototype.onDragEnd = function(d, a, c, g, p, e) {
        };
        e.prototype.onClick = function(d, a) {
        };
        e.prototype.onHoverStart = function(d, a) {
        };
        e.prototype.onHoverEnd = function() {
        };
        e.DRAGHANDLE_WIDTH = 4;
        e.MIN_WINDOW_LEN = .1;
        return e;
      }();
      d.FlameChartBase = e;
    })(d.Profiler || (d.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(m) {
      var e = d.StringUtilities.trimMiddle, t = d.ObjectUtilities.createEmptyObject, k = function(a) {
        function c(c, d) {
          a.call(this, c);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = d;
          this._kindStyle = t();
        }
        __extends(c, a);
        c.prototype.setSize = function(c, d) {
          a.prototype.setSize.call(this, c, d || this._initialized ? 12.5 * this._maxDepth : 100);
        };
        c.prototype.initialize = function(a, c) {
          this._initialized = !0;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(a, c);
          this.setWindow(a, c, !1);
          this.setSize(this._width, 12.5 * this._maxDepth);
        };
        c.prototype.destroy = function() {
          a.prototype.destroy.call(this);
          this._snapshot = null;
        };
        c.prototype.draw = function() {
          var a = this._context, c = window.devicePixelRatio;
          d.ColorStyle.reset();
          a.save();
          a.scale(c, c);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, this._width, this._height);
          this._initialized && this._drawChildren(this._snapshot);
          a.restore();
        };
        c.prototype._drawChildren = function(a, c) {
          "undefined" === typeof c && (c = 0);
          var d = a.getChildRange(this._windowStart, this._windowEnd);
          if (d) {
            for (var e = d.startIndex;e <= d.endIndex;e++) {
              var l = a.children[e];
              this._drawFrame(l, c) && this._drawChildren(l, c + 1);
            }
          }
        };
        c.prototype._drawFrame = function(a, c) {
          var e = this._context, k = this._toPixels(a.startTime), l = this._toPixels(a.endTime), r = l - k;
          if (r <= this._minFrameWidthInPixels) {
            return e.fillStyle = this._controller.theme.tabToolbar(1), e.fillRect(k, 12.5 * c, this._minFrameWidthInPixels, 12 + 12.5 * (a.maxDepth - a.depth)), !1;
          }
          0 > k && (l = r + k, k = 0);
          var l = l - k, b = this._kindStyle[a.kind.id];
          b || (b = d.ColorStyle.randomStyle(), b = this._kindStyle[a.kind.id] = {bgColor:b, textColor:d.ColorStyle.contrastStyle(b)});
          e.save();
          e.fillStyle = b.bgColor;
          e.fillRect(k, 12.5 * c, l, 12);
          12 < r && (r = a.kind.name) && r.length && (r = this._prepareText(e, r, l - 4), r.length && (e.fillStyle = b.textColor, e.textBaseline = "bottom", e.fillText(r, k + 2, 12.5 * (c + 1) - 1)));
          e.restore();
          return!0;
        };
        c.prototype._prepareText = function(a, c, d) {
          var k = this._measureWidth(a, c);
          if (d > k) {
            return c;
          }
          for (var k = 3, l = c.length;k < l;) {
            var r = k + l >> 1;
            this._measureWidth(a, e(c, r)) < d ? k = r + 1 : l = r;
          }
          c = e(c, l - 1);
          k = this._measureWidth(a, c);
          return k <= d ? c : "";
        };
        c.prototype._measureWidth = function(a, c) {
          var d = this._textWidth[c];
          d || (d = a.measureText(c).width, this._textWidth[c] = d);
          return d;
        };
        c.prototype._toPixelsRelative = function(a) {
          return a * this._width / (this._windowEnd - this._windowStart);
        };
        c.prototype._toPixels = function(a) {
          return this._toPixelsRelative(a - this._windowStart);
        };
        c.prototype._toTimeRelative = function(a) {
          return a * (this._windowEnd - this._windowStart) / this._width;
        };
        c.prototype._toTime = function(a) {
          return this._toTimeRelative(a) + this._windowStart;
        };
        c.prototype._getFrameAtPosition = function(a, c) {
          var d = 1 + c / 12.5 | 0, e = this._snapshot.query(this._toTime(a));
          if (e && e.depth >= d) {
            for (;e && e.depth > d;) {
              e = e.parent;
            }
            return e;
          }
          return null;
        };
        c.prototype.onMouseDown = function(a, c) {
          this._windowEqRange() || (this._mouseController.updateCursor(m.MouseCursor.ALL_SCROLL), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:1});
        };
        c.prototype.onMouseMove = function(a, c) {
        };
        c.prototype.onMouseOver = function(a, c) {
        };
        c.prototype.onMouseOut = function() {
        };
        c.prototype.onDrag = function(a, c, d, e, l, r) {
          if (a = this._dragInfo) {
            l = this._toTimeRelative(-l), this._controller.setWindow(a.windowStartInitial + l, a.windowEndInitial + l);
          }
        };
        c.prototype.onDragEnd = function(a, c, d, e, l, r) {
          this._dragInfo = null;
          this._mouseController.updateCursor(m.MouseCursor.DEFAULT);
        };
        c.prototype.onClick = function(a, c) {
          this._dragInfo = null;
          this._mouseController.updateCursor(m.MouseCursor.DEFAULT);
        };
        c.prototype.onHoverStart = function(a, c) {
          var d = this._getFrameAtPosition(a, c);
          d && (this._hoveredFrame = d, this._controller.showTooltip(this, d, a, c));
        };
        c.prototype.onHoverEnd = function() {
          this._hoveredFrame && (this._hoveredFrame = null, this._controller.hideTooltip());
        };
        c.prototype.getStatistics = function(a) {
          var c = this._snapshot;
          c.statistics || c.calculateStatistics();
          return c.statistics[a.id];
        };
        return c;
      }(m.FlameChartBase);
      m.FlameChart = k;
    })(m.Profiler || (m.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(m) {
      var e = d.NumberUtilities.clamp;
      (function(d) {
        d[d.OVERLAY = 0] = "OVERLAY";
        d[d.STACK = 1] = "STACK";
        d[d.UNION = 2] = "UNION";
      })(m.FlameChartOverviewMode || (m.FlameChartOverviewMode = {}));
      var t = function(d) {
        function a(a, g) {
          "undefined" === typeof g && (g = 1);
          this._mode = g;
          this._overviewCanvasDirty = !0;
          this._overviewCanvas = document.createElement("canvas");
          this._overviewContext = this._overviewCanvas.getContext("2d");
          d.call(this, a);
        }
        __extends(a, d);
        a.prototype.setSize = function(a, g) {
          d.prototype.setSize.call(this, a, g || 64);
        };
        Object.defineProperty(a.prototype, "mode", {set:function(a) {
          this._mode = a;
          this.draw();
        }, enumerable:!0, configurable:!0});
        a.prototype._resetCanvas = function() {
          d.prototype._resetCanvas.call(this);
          this._overviewCanvas.width = this._canvas.width;
          this._overviewCanvas.height = this._canvas.height;
          this._overviewCanvasDirty = !0;
        };
        a.prototype.draw = function() {
          var a = this._context, g = window.devicePixelRatio, d = this._width, e = this._height;
          a.save();
          a.scale(g, g);
          a.fillStyle = this._controller.theme.bodyBackground(1);
          a.fillRect(0, 0, d, e);
          a.restore();
          this._initialized && (this._overviewCanvasDirty && (this._drawChart(), this._overviewCanvasDirty = !1), a.drawImage(this._overviewCanvas, 0, 0), this._drawSelection());
        };
        a.prototype._drawSelection = function() {
          var a = this._context, g = this._height, d = window.devicePixelRatio, e = this._selection ? this._selection.left : this._toPixels(this._windowStart), k = this._selection ? this._selection.right : this._toPixels(this._windowEnd), l = this._controller.theme;
          a.save();
          a.scale(d, d);
          this._selection ? (a.fillStyle = l.selectionText(.15), a.fillRect(e, 1, k - e, g - 1), a.fillStyle = "rgba(133, 0, 0, 1)", a.fillRect(e + .5, 0, k - e - 1, 4), a.fillRect(e + .5, g - 4, k - e - 1, 4)) : (a.fillStyle = l.bodyBackground(.4), a.fillRect(0, 1, e, g - 1), a.fillRect(k, 1, this._width, g - 1));
          a.beginPath();
          a.moveTo(e, 0);
          a.lineTo(e, g);
          a.moveTo(k, 0);
          a.lineTo(k, g);
          a.lineWidth = .5;
          a.strokeStyle = l.foregroundTextGrey(1);
          a.stroke();
          g = Math.abs((this._selection ? this._toTime(this._selection.right) : this._windowEnd) - (this._selection ? this._toTime(this._selection.left) : this._windowStart));
          a.fillStyle = l.selectionText(.5);
          a.font = "8px sans-serif";
          a.textBaseline = "alphabetic";
          a.textAlign = "end";
          a.fillText(g.toFixed(2), Math.min(e, k) - 4, 10);
          a.fillText((g / 60).toFixed(2), Math.min(e, k) - 4, 20);
          a.restore();
        };
        a.prototype._drawChart = function() {
          var a = window.devicePixelRatio, g = this._height, d = this._controller.activeProfile, e = 4 * this._width, k = d.totalTime / e, l = this._overviewContext, r = this._controller.theme.blueHighlight(1);
          l.save();
          l.translate(0, a * g);
          var b = -a * g / (d.maxDepth - 1);
          l.scale(a / 4, b);
          l.clearRect(0, 0, e, d.maxDepth - 1);
          1 == this._mode && l.scale(1, 1 / d.snapshotCount);
          for (var h = 0, m = d.snapshotCount;h < m;h++) {
            var n = d.getSnapshotAt(h);
            if (n) {
              var f = null, q = 0;
              l.beginPath();
              l.moveTo(0, 0);
              for (var U = 0;U < e;U++) {
                q = d.startTime + U * k, q = (f = f ? f.queryNext(q) : n.query(q)) ? f.getDepth() - 1 : 0, l.lineTo(U, q);
              }
              l.lineTo(U, 0);
              l.fillStyle = r;
              l.fill();
              1 == this._mode && l.translate(0, -g * a / b);
            }
          }
          l.restore();
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
        a.prototype._getDragTargetUnderCursor = function(a, g) {
          if (0 <= g && g < this._height) {
            var d = this._toPixels(this._windowStart), e = this._toPixels(this._windowEnd), k = 2 + m.FlameChartBase.DRAGHANDLE_WIDTH / 2, l = a >= d - k && a <= d + k, r = a >= e - k && a <= e + k;
            if (l && r) {
              return 4;
            }
            if (l) {
              return 2;
            }
            if (r) {
              return 3;
            }
            if (!this._windowEqRange() && a > d + k && a < e - k) {
              return 1;
            }
          }
          return 0;
        };
        a.prototype.onMouseDown = function(a, g) {
          var d = this._getDragTargetUnderCursor(a, g);
          0 === d ? (this._selection = {left:a, right:a}, this.draw()) : (1 === d && this._mouseController.updateCursor(m.MouseCursor.GRABBING), this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:d});
        };
        a.prototype.onMouseMove = function(a, g) {
          var d = m.MouseCursor.DEFAULT, e = this._getDragTargetUnderCursor(a, g);
          0 === e || this._selection || (d = 1 === e ? m.MouseCursor.GRAB : m.MouseCursor.EW_RESIZE);
          this._mouseController.updateCursor(d);
        };
        a.prototype.onMouseOver = function(a, g) {
          this.onMouseMove(a, g);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(m.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, g, d, k, u, l) {
          if (this._selection) {
            this._selection = {left:a, right:e(d, 0, this._width - 1)}, this.draw();
          } else {
            a = this._dragInfo;
            if (4 === a.target) {
              if (0 !== u) {
                a.target = 0 > u ? 2 : 3;
              } else {
                return;
              }
            }
            g = this._windowStart;
            d = this._windowEnd;
            u = this._toTimeRelative(u);
            switch(a.target) {
              case 1:
                g = a.windowStartInitial + u;
                d = a.windowEndInitial + u;
                break;
              case 2:
                g = e(a.windowStartInitial + u, this._rangeStart, d - m.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3:
                d = e(a.windowEndInitial + u, g + m.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(g, d);
          }
        };
        a.prototype.onDragEnd = function(a, g, d, e, k, l) {
          this._selection && (this._selection = null, this._controller.setWindow(this._toTime(a), this._toTime(d)));
          this._dragInfo = null;
          this.onMouseMove(d, e);
        };
        a.prototype.onClick = function(a, g) {
          this._selection = this._dragInfo = null;
          this._windowEqRange() || (0 === this._getDragTargetUnderCursor(a, g) && this._controller.moveWindowTo(this._toTime(a)), this.onMouseMove(a, g));
          this.draw();
        };
        a.prototype.onHoverStart = function(a, g) {
        };
        a.prototype.onHoverEnd = function() {
        };
        return a;
      }(m.FlameChartBase);
      m.FlameChartOverview = t;
    })(m.Profiler || (m.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(m) {
      var e = d.NumberUtilities.clamp;
      (function(d) {
        d[d.OVERVIEW = 0] = "OVERVIEW";
        d[d.CHART = 1] = "CHART";
      })(m.FlameChartHeaderType || (m.FlameChartHeaderType = {}));
      var t = function(d) {
        function a(a, g) {
          this._type = g;
          d.call(this, a);
        }
        __extends(a, d);
        a.prototype.draw = function() {
          var a = this._context, g = window.devicePixelRatio, d = this._width, e = this._height;
          a.save();
          a.scale(g, g);
          a.fillStyle = this._controller.theme.tabToolbar(1);
          a.fillRect(0, 0, d, e);
          this._initialized && (0 == this._type ? (g = this._toPixels(this._windowStart), d = this._toPixels(this._windowEnd), a.fillStyle = this._controller.theme.bodyBackground(1), a.fillRect(g, 0, d - g, e), this._drawLabels(this._rangeStart, this._rangeEnd), this._drawDragHandle(g), this._drawDragHandle(d)) : this._drawLabels(this._windowStart, this._windowEnd));
          a.restore();
        };
        a.prototype._drawLabels = function(c, g) {
          var d = this._context, e = this._calculateTickInterval(c, g), k = Math.ceil(c / e) * e, l = 500 <= e, r = l ? 1E3 : 1, b = this._decimalPlaces(e / r), l = l ? "s" : "ms", h = this._toPixels(k), m = this._height / 2, n = this._controller.theme;
          d.lineWidth = 1;
          d.strokeStyle = n.contentTextDarkGrey(.5);
          d.fillStyle = n.contentTextDarkGrey(1);
          d.textAlign = "right";
          d.textBaseline = "middle";
          d.font = "11px sans-serif";
          for (n = this._width + a.TICK_MAX_WIDTH;h < n;) {
            d.fillText((k / r).toFixed(b) + " " + l, h - 7, m + 1), d.beginPath(), d.moveTo(h, 0), d.lineTo(h, this._height + 1), d.closePath(), d.stroke(), k += e, h = this._toPixels(k);
          }
        };
        a.prototype._calculateTickInterval = function(c, g) {
          var d = (g - c) / (this._width / a.TICK_MAX_WIDTH), e = Math.pow(10, Math.floor(Math.log(d) / Math.LN10)), d = d / e;
          return 5 < d ? 10 * e : 2 < d ? 5 * e : 1 < d ? 2 * e : e;
        };
        a.prototype._drawDragHandle = function(a) {
          var g = this._context;
          g.lineWidth = 2;
          g.strokeStyle = this._controller.theme.bodyBackground(1);
          g.fillStyle = this._controller.theme.foregroundTextGrey(.7);
          this._drawRoundedRect(g, a - m.FlameChartBase.DRAGHANDLE_WIDTH / 2, m.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2);
        };
        a.prototype._drawRoundedRect = function(a, g, d, e) {
          var k, l = !0;
          "undefined" === typeof l && (l = !0);
          "undefined" === typeof k && (k = !0);
          a.beginPath();
          a.moveTo(g + 2, 1);
          a.lineTo(g + d - 2, 1);
          a.quadraticCurveTo(g + d, 1, g + d, 3);
          a.lineTo(g + d, 1 + e - 2);
          a.quadraticCurveTo(g + d, 1 + e, g + d - 2, 1 + e);
          a.lineTo(g + 2, 1 + e);
          a.quadraticCurveTo(g, 1 + e, g, 1 + e - 2);
          a.lineTo(g, 3);
          a.quadraticCurveTo(g, 1, g + 2, 1);
          a.closePath();
          l && a.stroke();
          k && a.fill();
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
        a.prototype._getDragTargetUnderCursor = function(a, g) {
          if (0 <= g && g < this._height) {
            if (0 === this._type) {
              var d = this._toPixels(this._windowStart), e = this._toPixels(this._windowEnd), k = 2 + m.FlameChartBase.DRAGHANDLE_WIDTH / 2, d = a >= d - k && a <= d + k, e = a >= e - k && a <= e + k;
              if (d && e) {
                return 4;
              }
              if (d) {
                return 2;
              }
              if (e) {
                return 3;
              }
            }
            if (!this._windowEqRange()) {
              return 1;
            }
          }
          return 0;
        };
        a.prototype.onMouseDown = function(a, g) {
          var d = this._getDragTargetUnderCursor(a, g);
          1 === d && this._mouseController.updateCursor(m.MouseCursor.GRABBING);
          this._dragInfo = {windowStartInitial:this._windowStart, windowEndInitial:this._windowEnd, target:d};
        };
        a.prototype.onMouseMove = function(a, g) {
          var d = m.MouseCursor.DEFAULT, e = this._getDragTargetUnderCursor(a, g);
          0 !== e && (1 !== e ? d = m.MouseCursor.EW_RESIZE : 1 !== e || this._windowEqRange() || (d = m.MouseCursor.GRAB));
          this._mouseController.updateCursor(d);
        };
        a.prototype.onMouseOver = function(a, g) {
          this.onMouseMove(a, g);
        };
        a.prototype.onMouseOut = function() {
          this._mouseController.updateCursor(m.MouseCursor.DEFAULT);
        };
        a.prototype.onDrag = function(a, g, d, k, u, l) {
          a = this._dragInfo;
          if (4 === a.target) {
            if (0 !== u) {
              a.target = 0 > u ? 2 : 3;
            } else {
              return;
            }
          }
          g = this._windowStart;
          d = this._windowEnd;
          u = this._toTimeRelative(u);
          switch(a.target) {
            case 1:
              d = 0 === this._type ? 1 : -1;
              g = a.windowStartInitial + d * u;
              d = a.windowEndInitial + d * u;
              break;
            case 2:
              g = e(a.windowStartInitial + u, this._rangeStart, d - m.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3:
              d = e(a.windowEndInitial + u, g + m.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(g, d);
        };
        a.prototype.onDragEnd = function(a, g, d, e, k, l) {
          this._dragInfo = null;
          this.onMouseMove(d, e);
        };
        a.prototype.onClick = function(a, g) {
          1 === this._dragInfo.target && this._mouseController.updateCursor(m.MouseCursor.GRAB);
        };
        a.prototype.onHoverStart = function(a, g) {
        };
        a.prototype.onHoverEnd = function() {
        };
        a.TICK_MAX_WIDTH = 75;
        return a;
      }(m.FlameChartBase);
      m.FlameChartHeader = t;
    })(m.Profiler || (m.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      (function(d) {
        var m = function() {
          function a(a, g, d, e, k) {
            this.pageLoaded = a;
            this.threadsTotal = g;
            this.threadsLoaded = d;
            this.threadFilesTotal = e;
            this.threadFilesLoaded = k;
          }
          a.prototype.toString = function() {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function(a, g, d) {
              return a + ":" + this[a];
            }, this).join(", ") + "]";
          };
          return a;
        }();
        d.TraceLoggerProgressInfo = m;
        var k = function() {
          function a(a) {
            this._baseUrl = a;
            this._threads = [];
            this._progressInfo = null;
          }
          a.prototype.loadPage = function(a, g, d) {
            this._threads = [];
            this._pageLoadCallback = g;
            this._pageLoadProgressCallback = d;
            this._progressInfo = new m(!1, 0, 0, 0, 0);
            this._loadData([a], this._onLoadPage.bind(this));
          };
          Object.defineProperty(a.prototype, "buffers", {get:function() {
            for (var a = [], g = 0, d = this._threads.length;g < d;g++) {
              a.push(this._threads[g].buffer);
            }
            return a;
          }, enumerable:!0, configurable:!0});
          a.prototype._onProgress = function() {
            this._pageLoadProgressCallback && this._pageLoadProgressCallback.call(this, this._progressInfo);
          };
          a.prototype._onLoadPage = function(a) {
            if (a && 1 == a.length) {
              var g = this, p = 0;
              a = a[0];
              var k = a.length;
              this._threads = Array(k);
              this._progressInfo.pageLoaded = !0;
              this._progressInfo.threadsTotal = k;
              for (var m = 0;m < a.length;m++) {
                var l = a[m], r = [l.dict, l.tree];
                l.corrections && r.push(l.corrections);
                this._progressInfo.threadFilesTotal += r.length;
                this._loadData(r, function(a) {
                  return function(c) {
                    c && (c = new d.Thread(c), c.buffer.name = "Thread " + a, g._threads[a] = c);
                    p++;
                    g._progressInfo.threadsLoaded++;
                    g._onProgress();
                    p === k && g._pageLoadCallback.call(g, null, g._threads);
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
          a.prototype._loadData = function(a, g, d) {
            var e = 0, k = 0, l = a.length, r = [];
            r.length = l;
            for (var b = 0;b < l;b++) {
              var h = this._baseUrl + a[b], m = new XMLHttpRequest, n = /\.tl$/i.test(h) ? "arraybuffer" : "json";
              m.open("GET", h, !0);
              m.responseType = n;
              m.onload = function(a, b) {
                return function(c) {
                  if ("json" === b) {
                    if (c = this.response, "string" === typeof c) {
                      try {
                        c = JSON.parse(c), r[a] = c;
                      } catch (n) {
                        k++;
                      }
                    } else {
                      r[a] = c;
                    }
                  } else {
                    r[a] = this.response;
                  }
                  ++e;
                  d && d(e);
                  e === l && g(r);
                };
              }(b, n);
              m.send();
            }
          };
          a.colors = "#0044ff #8c4b00 #cc5c33 #ff80c4 #ffbfd9 #ff8800 #8c5e00 #adcc33 #b380ff #bfd9ff #ffaa00 #8c0038 #bf8f30 #f780ff #cc99c9 #aaff00 #000073 #452699 #cc8166 #cca799 #000066 #992626 #cc6666 #ccc299 #ff6600 #526600 #992663 #cc6681 #99ccc2 #ff0066 #520066 #269973 #61994d #739699 #ffcc00 #006629 #269199 #94994d #738299 #ff0000 #590000 #234d8c #8c6246 #7d7399 #ee00ff #00474d #8c2385 #8c7546 #7c8c69 #eeff00 #4d003d #662e1a #62468c #8c6969 #6600ff #4c2900 #1a6657 #8c464f #8c6981 #44ff00 #401100 #1a2466 #663355 #567365 #d90074 #403300 #101d40 #59562d #66614d #cc0000 #002b40 #234010 #4c2626 #4d5e66 #00a3cc #400011 #231040 #4c3626 #464359 #0000bf #331b00 #80e6ff #311a33 #4d3939 #a69b00 #003329 #80ffb2 #331a20 #40303d #00a658 #40ffd9 #ffc480 #ffe1bf #332b26 #8c2500 #9933cc #80fff6 #ffbfbf #303326 #005e8c #33cc47 #b2ff80 #c8bfff #263332 #00708c #cc33ad #ffe680 #f2ffbf #262a33 #388c00 #335ccc #8091ff #bfffd9".split(" ");
          return a;
        }();
        d.TraceLogger = k;
      })(d.TraceLogger || (d.TraceLogger = {}));
    })(d.Profiler || (d.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      (function(e) {
        var m;
        (function(d) {
          d[d.START_HI = 0] = "START_HI";
          d[d.START_LO = 4] = "START_LO";
          d[d.STOP_HI = 8] = "STOP_HI";
          d[d.STOP_LO = 12] = "STOP_LO";
          d[d.TEXTID = 16] = "TEXTID";
          d[d.NEXTID = 20] = "NEXTID";
        })(m || (m = {}));
        m = function() {
          function e(a) {
            2 <= a.length && (this._text = a[0], this._data = new DataView(a[1]), this._buffer = new d.TimelineBuffer, this._walkTree(0));
          }
          Object.defineProperty(e.prototype, "buffer", {get:function() {
            return this._buffer;
          }, enumerable:!0, configurable:!0});
          e.prototype._walkTree = function(a) {
            var c = this._data, g = this._buffer;
            do {
              var d = a * e.ITEM_SIZE, m = 4294967295 * c.getUint32(d + 0) + c.getUint32(d + 4), u = 4294967295 * c.getUint32(d + 8) + c.getUint32(d + 12), l = c.getUint32(d + 16), d = c.getUint32(d + 20), r = 1 === (l & 1), l = l >>> 1, l = this._text[l];
              g.enter(l, null, m / 1E6);
              r && this._walkTree(a + 1);
              g.leave(l, null, u / 1E6);
              a = d;
            } while (0 !== a);
          };
          e.ITEM_SIZE = 24;
          return e;
        }();
        e.Thread = m;
      })(d.TraceLogger || (d.TraceLogger = {}));
    })(d.Profiler || (d.Profiler = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(m) {
      var e = d.NumberUtilities.clamp, t = function() {
        function a() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        a.prototype.append = function(a, d) {
          var e = this.lines;
          0 < e.length && e[e.length - 1] === a ? this.repeat[e.length - 1]++ : (this.lines.push(a), this.repeat.push(1), this.format.push(d ? {backgroundFillStyle:d} : void 0), this.time.push(performance.now()), this.length++);
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
      m.Buffer = t;
      var k = function() {
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
          this.buffer = new t;
          a.addEventListener("keydown", function(a) {
            var c = 0;
            switch(a.keyCode) {
              case f:
                this.showLineNumbers = !this.showLineNumbers;
                break;
              case q:
                this.showLineTime = !this.showLineTime;
                break;
              case l:
                c = -1;
                break;
              case r:
                c = 1;
                break;
              case d:
                c = -this.pageLineCount;
                break;
              case e:
                c = this.pageLineCount;
                break;
              case k:
                c = -this.lineIndex;
                break;
              case m:
                c = this.buffer.length - this.lineIndex;
                break;
              case b:
                this.columnIndex -= a.metaKey ? 10 : 1;
                0 > this.columnIndex && (this.columnIndex = 0);
                a.preventDefault();
                break;
              case h:
                this.columnIndex += a.metaKey ? 10 : 1;
                a.preventDefault();
                break;
              case w:
                a.metaKey && (this.selection = {start:0, end:this.buffer.length}, a.preventDefault());
                break;
              case n:
                if (a.metaKey) {
                  var I = "";
                  if (this.selection) {
                    for (var B = this.selection.start;B <= this.selection.end;B++) {
                      I += this.buffer.get(B) + "\n";
                    }
                  } else {
                    I = this.buffer.get(this.lineIndex);
                  }
                  alert(I);
                }
              ;
            }
            a.metaKey && (c *= this.pageLineCount);
            c && (this.scroll(c), a.preventDefault());
            c && a.shiftKey ? this.selection ? this.lineIndex > this.selection.start ? this.selection.end = this.lineIndex : this.selection.start = this.lineIndex : 0 < c ? this.selection = {start:this.lineIndex - c, end:this.lineIndex} : 0 > c && (this.selection = {start:this.lineIndex, end:this.lineIndex - c}) : c && (this.selection = null);
            this.paint();
          }.bind(this), !1);
          a.addEventListener("focus", function(a) {
            this.hasFocus = !0;
          }.bind(this), !1);
          a.addEventListener("blur", function(a) {
            this.hasFocus = !1;
          }.bind(this), !1);
          var d = 33, e = 34, k = 36, m = 35, l = 38, r = 40, b = 37, h = 39, w = 65, n = 67, f = 78, q = 84;
        }
        a.prototype.resize = function() {
          this._resizeHandler();
        };
        a.prototype._resizeHandler = function() {
          var a = this.canvas.parentElement, d = a.clientWidth, a = a.clientHeight - 1, e = window.devicePixelRatio || 1;
          1 !== e ? (this.ratio = e / 1, this.canvas.width = d * this.ratio, this.canvas.height = a * this.ratio, this.canvas.style.width = d + "px", this.canvas.style.height = a + "px") : (this.ratio = 1, this.canvas.width = d, this.canvas.height = a);
          this.pageLineCount = Math.floor(this.canvas.height / this.lineHeight);
        };
        a.prototype.gotoLine = function(a) {
          this.lineIndex = e(a, 0, this.buffer.length - 1);
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
          var d = this.textMarginLeft, e = d + (this.showLineNumbers ? 5 * (String(this.buffer.length).length + 2) : 0), k = e + (this.showLineTime ? 40 : 10), m = k + 25;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);
          for (var l = this.canvas.width, r = this.lineHeight, b = 0;b < a;b++) {
            var h = b * this.lineHeight, w = this.pageIndex + b, n = this.buffer.get(w), f = this.buffer.getFormat(w), q = this.buffer.getRepeat(w), U = 1 < w ? this.buffer.getTime(w) - this.buffer.getTime(0) : 0;
            this.context.fillStyle = w % 2 ? this.lineColor : this.alternateLineColor;
            f && f.backgroundFillStyle && (this.context.fillStyle = f.backgroundFillStyle);
            this.context.fillRect(0, h, l, r);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;
            this.selection && w >= this.selection.start && w <= this.selection.end && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, h, l, r), this.context.fillStyle = this.selectionTextColor);
            this.hasFocus && w === this.lineIndex && (this.context.fillStyle = this.selectionColor, this.context.fillRect(0, h, l, r), this.context.fillStyle = this.selectionTextColor);
            0 < this.columnIndex && (n = n.substring(this.columnIndex));
            h = (b + 1) * this.lineHeight - this.textMarginBottom;
            this.showLineNumbers && this.context.fillText(String(w), d, h);
            this.showLineTime && this.context.fillText(U.toFixed(1).padLeft(" ", 6), e, h);
            1 < q && this.context.fillText(String(q).padLeft(" ", 3), k, h);
            this.context.fillText(n, m, h);
          }
        };
        a.prototype.refreshEvery = function(a) {
          function d() {
            e.paint();
            e.refreshFrequency && setTimeout(d, e.refreshFrequency);
          }
          var e = this;
          this.refreshFrequency = a;
          e.refreshFrequency && setTimeout(d, e.refreshFrequency);
        };
        a.prototype.isScrolledToBottom = function() {
          return this.lineIndex === this.buffer.length - 1;
        };
        return a;
      }();
      m.Terminal = k;
    })(m.Terminal || (m.Terminal = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      var e = function() {
        function d(e) {
          this._lastWeightedTime = this._lastTime = this._index = 0;
          this._gradient = "#FF0000 #FF1100 #FF2300 #FF3400 #FF4600 #FF5700 #FF6900 #FF7B00 #FF8C00 #FF9E00 #FFAF00 #FFC100 #FFD300 #FFE400 #FFF600 #F7FF00 #E5FF00 #D4FF00 #C2FF00 #B0FF00 #9FFF00 #8DFF00 #7CFF00 #6AFF00 #58FF00 #47FF00 #35FF00 #24FF00 #12FF00 #00FF00".split(" ");
          this._canvas = e;
          this._context = e.getContext("2d");
          window.addEventListener("resize", this._resizeHandler.bind(this), !1);
          this._resizeHandler();
        }
        d.prototype._resizeHandler = function() {
          var d = this._canvas.parentElement, a = d.clientWidth, d = d.clientHeight - 1, c = window.devicePixelRatio || 1;
          1 !== c ? (this._ratio = c / 1, this._canvas.width = a * this._ratio, this._canvas.height = d * this._ratio, this._canvas.style.width = a + "px", this._canvas.style.height = d + "px") : (this._ratio = 1, this._canvas.width = a, this._canvas.height = d);
        };
        d.prototype.tickAndRender = function(d) {
          "undefined" === typeof d && (d = !1);
          if (0 === this._lastTime) {
            this._lastTime = performance.now();
          } else {
            var a = (performance.now() - this._lastTime) * (1 - .9) + .9 * this._lastWeightedTime, c = this._context, g = 2 * this._ratio, e = (this._canvas.width - 20) / (g + 1) | 0, m = this._index++;
            this._index > e && (this._index = 0);
            c.globalAlpha = 1;
            c.fillStyle = "black";
            c.fillRect(20 + m * (g + 1), 0, 4 * g, this._canvas.height);
            e = 1E3 / 60 / a;
            c.fillStyle = this._gradient[e * (this._gradient.length - 1) | 0];
            c.globalAlpha = d ? .5 : 1;
            c.fillRect(20 + m * (g + 1), 0, g, this._canvas.height * e | 0);
            0 === m % 16 && (c.globalAlpha = 1, c.fillStyle = "black", c.fillRect(0, 0, 20, this._canvas.height), c.fillStyle = "white", c.font = "10px Arial", c.fillText((1E3 / a).toFixed(0), 2, 8));
            this._lastTime = performance.now();
            this._lastWeightedTime = a;
          }
        };
        return d;
      }();
      d.FPS = e;
    })(d.Mini || (d.Mini = {}));
  })(d.Tools || (d.Tools = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load Shared Dependencies");
console.time("Load GFX Dependencies");
(function(d) {
  (function(m) {
    function s(a, b, f) {
      return p && f ? "string" === typeof b ? (a = d.ColorUtilities.cssStyleToRGBA(b), d.ColorUtilities.rgbaToCSSStyle(f.transformRGBA(a))) : b instanceof CanvasGradient && b._template ? b._template.createCanvasGradient(a, f) : b : b;
    }
    var e = d.Debug.assert, t = d.NumberUtilities.clamp;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Brief = 1] = "Brief";
      a[a.Verbose = 2] = "Verbose";
    })(m.TraceLevel || (m.TraceLevel = {}));
    var k = d.Metrics.Counter.instance;
    m.frameCounter = new d.Metrics.Counter(!0);
    m.traceLevel = 2;
    m.writer = null;
    m.frameCount = function(a) {
      k.count(a);
      m.frameCounter.count(a);
    };
    m.timelineBuffer = new d.Tools.Profiler.TimelineBuffer("GFX");
    m.enterTimeline = function(a, b) {
    };
    m.leaveTimeline = function(a, b) {
    };
    var a = null, c = null, g = null, p = !0;
    p && "undefined" !== typeof CanvasRenderingContext2D && (a = CanvasGradient.prototype.addColorStop, c = CanvasRenderingContext2D.prototype.createLinearGradient, g = CanvasRenderingContext2D.prototype.createRadialGradient, CanvasRenderingContext2D.prototype.createLinearGradient = function(a, b, f, c) {
      return(new u(a, b, f, c)).createCanvasGradient(this, null);
    }, CanvasRenderingContext2D.prototype.createRadialGradient = function(a, b, f, c, d, q) {
      return(new l(a, b, f, c, d, q)).createCanvasGradient(this, null);
    }, CanvasGradient.prototype.addColorStop = function(b, f) {
      a.call(this, b, f);
      this._template.addColorStop(b, f);
    });
    var v = function() {
      return function(a, b) {
        this.offset = a;
        this.color = b;
      };
    }(), u = function() {
      function b(a, f, c, d) {
        this.x0 = a;
        this.y0 = f;
        this.x1 = c;
        this.y1 = d;
        this.colorStops = [];
      }
      b.prototype.addColorStop = function(a, b) {
        this.colorStops.push(new v(a, b));
      };
      b.prototype.createCanvasGradient = function(b, f) {
        for (var d = c.call(b, this.x0, this.y0, this.x1, this.y1), q = this.colorStops, g = 0;g < q.length;g++) {
          var n = q[g], e = n.offset, n = n.color, n = f ? s(b, n, f) : n;
          a.call(d, e, n);
        }
        d._template = this;
        d._transform = this._transform;
        return d;
      };
      return b;
    }(), l = function() {
      function b(a, f, c, d, q, g) {
        this.x0 = a;
        this.y0 = f;
        this.r0 = c;
        this.x1 = d;
        this.y1 = q;
        this.r1 = g;
        this.colorStops = [];
      }
      b.prototype.addColorStop = function(a, b) {
        this.colorStops.push(new v(a, b));
      };
      b.prototype.createCanvasGradient = function(b, f) {
        for (var c = g.call(b, this.x0, this.y0, this.r0, this.x1, this.y1, this.r1), d = this.colorStops, q = 0;q < d.length;q++) {
          var n = d[q], e = n.offset, n = n.color, n = f ? s(b, n, f) : n;
          a.call(c, e, n);
        }
        c._template = this;
        c._transform = this._transform;
        return c;
      };
      return b;
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
    var b = function() {
      function a(b) {
        this._commands = new Uint8Array(a._arrayBufferPool.acquire(8), 0, 8);
        this._commandPosition = 0;
        this._data = new Float64Array(a._arrayBufferPool.acquire(8 * Float64Array.BYTES_PER_ELEMENT), 0, 8);
        this._dataPosition = 0;
        b instanceof a && this.addPath(b);
      }
      a._apply = function(a, b) {
        var f = a._commands, c = a._data, d = 0, q = 0;
        b.beginPath();
        for (var g = a._commandPosition;d < g;) {
          switch(f[d++]) {
            case 1:
              b.closePath();
              break;
            case 2:
              b.moveTo(c[q++], c[q++]);
              break;
            case 3:
              b.lineTo(c[q++], c[q++]);
              break;
            case 4:
              b.quadraticCurveTo(c[q++], c[q++], c[q++], c[q++]);
              break;
            case 5:
              b.bezierCurveTo(c[q++], c[q++], c[q++], c[q++], c[q++], c[q++]);
              break;
            case 6:
              b.arcTo(c[q++], c[q++], c[q++], c[q++], c[q++]);
              break;
            case 7:
              b.rect(c[q++], c[q++], c[q++], c[q++]);
              break;
            case 8:
              b.arc(c[q++], c[q++], c[q++], c[q++], c[q++], !!c[q++]);
              break;
            case 9:
              b.save();
              break;
            case 10:
              b.restore();
              break;
            case 11:
              b.transform(c[q++], c[q++], c[q++], c[q++], c[q++], c[q++]);
          }
        }
      };
      a.prototype._ensureCommandCapacity = function(b) {
        this._commands = a._arrayBufferPool.ensureUint8ArrayLength(this._commands, b);
      };
      a.prototype._ensureDataCapacity = function(b) {
        this._data = a._arrayBufferPool.ensureFloat64ArrayLength(this._data, b);
      };
      a.prototype._writeCommand = function(a) {
        this._commandPosition >= this._commands.length && this._ensureCommandCapacity(this._commandPosition + 1);
        this._commands[this._commandPosition++] = a;
      };
      a.prototype._writeData = function(a, b, f, c, q, d) {
        var g = arguments.length;
        e(6 >= g && (0 === g % 2 || 5 === g));
        this._dataPosition + g >= this._data.length && this._ensureDataCapacity(this._dataPosition + g);
        var n = this._data, h = this._dataPosition;
        n[h] = a;
        n[h + 1] = b;
        2 < g && (n[h + 2] = f, n[h + 3] = c, 4 < g && (n[h + 4] = q, 6 === g && (n[h + 5] = d)));
        this._dataPosition += g;
      };
      a.prototype.closePath = function() {
        this._writeCommand(1);
      };
      a.prototype.moveTo = function(a, b) {
        this._writeCommand(2);
        this._writeData(a, b);
      };
      a.prototype.lineTo = function(a, b) {
        this._writeCommand(3);
        this._writeData(a, b);
      };
      a.prototype.quadraticCurveTo = function(a, b, f, c) {
        this._writeCommand(4);
        this._writeData(a, b, f, c);
      };
      a.prototype.bezierCurveTo = function(a, b, f, c, q, d) {
        this._writeCommand(5);
        this._writeData(a, b, f, c, q, d);
      };
      a.prototype.arcTo = function(a, b, f, c, q) {
        this._writeCommand(6);
        this._writeData(a, b, f, c, q);
      };
      a.prototype.rect = function(a, b, f, c) {
        this._writeCommand(7);
        this._writeData(a, b, f, c);
      };
      a.prototype.arc = function(a, b, f, c, q, d) {
        this._writeCommand(8);
        this._writeData(a, b, f, c, q, +d);
      };
      a.prototype.addPath = function(a, b) {
        b && (this._writeCommand(9), this._writeCommand(11), this._writeData(b.a, b.b, b.c, b.d, b.e, b.f));
        var f = this._commandPosition + a._commandPosition;
        f >= this._commands.length && this._ensureCommandCapacity(f);
        for (var c = this._commands, q = a._commands, d = this._commandPosition, g = 0;d < f;d++) {
          c[d] = q[g++];
        }
        this._commandPosition = f;
        f = this._dataPosition + a._dataPosition;
        f >= this._data.length && this._ensureDataCapacity(f);
        c = this._data;
        q = a._data;
        d = this._dataPosition;
        for (g = 0;d < f;d++) {
          c[d] = q[g++];
        }
        this._dataPosition = f;
        b && this._writeCommand(10);
      };
      a._arrayBufferPool = new d.ArrayBufferPool;
      return a;
    }();
    m.Path = b;
    if ("undefined" !== typeof CanvasRenderingContext2D && ("undefined" === typeof Path2D || !Path2D.prototype.addPath)) {
      var h = CanvasRenderingContext2D.prototype.fill;
      CanvasRenderingContext2D.prototype.fill = function(a, f) {
        arguments.length && (a instanceof b ? b._apply(a, this) : f = a);
        f ? h.call(this, f) : h.call(this);
      };
      var w = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.stroke = function(a, f) {
        arguments.length && (a instanceof b ? b._apply(a, this) : f = a);
        f ? w.call(this, f) : w.call(this);
      };
      var n = CanvasRenderingContext2D.prototype.clip;
      CanvasRenderingContext2D.prototype.clip = function(a, f) {
        arguments.length && (a instanceof b ? b._apply(a, this) : f = a);
        f ? n.call(this, f) : n.call(this);
      };
      window.Path2D = b;
    }
    if ("undefined" !== typeof CanvasPattern && Path2D.prototype.addPath) {
      r = function(a) {
        this._transform = a;
        this._template && (this._template._transform = a);
      };
      CanvasPattern.prototype.setTransform || (CanvasPattern.prototype.setTransform = r);
      CanvasGradient.prototype.setTransform || (CanvasGradient.prototype.setTransform = r);
      var f = CanvasRenderingContext2D.prototype.fill, q = CanvasRenderingContext2D.prototype.stroke;
      CanvasRenderingContext2D.prototype.fill = function(a, b) {
        var c = !!this.fillStyle._transform;
        if ((this.fillStyle instanceof CanvasPattern || this.fillStyle instanceof CanvasGradient) && c && a instanceof Path2D) {
          var c = this.fillStyle._transform, q;
          try {
            q = c.inverse();
          } catch (d) {
            q = c = m.Geometry.Matrix.createIdentitySVGMatrix();
          }
          this.transform(c.a, c.b, c.c, c.d, c.e, c.f);
          c = new Path2D;
          c.addPath(a, q);
          f.call(this, c, b);
          this.transform(q.a, q.b, q.c, q.d, q.e, q.f);
        } else {
          0 === arguments.length ? f.call(this) : 1 === arguments.length ? f.call(this, a) : 2 === arguments.length && f.call(this, a, b);
        }
      };
      CanvasRenderingContext2D.prototype.stroke = function(a) {
        var b = !!this.strokeStyle._transform;
        if ((this.strokeStyle instanceof CanvasPattern || this.strokeStyle instanceof CanvasGradient) && b && a instanceof Path2D) {
          var f = this.strokeStyle._transform, b = f.inverse();
          this.transform(f.a, f.b, f.c, f.d, f.e, f.f);
          f = new Path2D;
          f.addPath(a, b);
          var c = this.lineWidth;
          this.lineWidth *= (b.a + b.d) / 2;
          q.call(this, f);
          this.transform(b.a, b.b, b.c, b.d, b.e, b.f);
          this.lineWidth = c;
        } else {
          0 === arguments.length ? q.call(this) : 1 === arguments.length && q.call(this, a);
        }
      };
    }
    "undefined" !== typeof CanvasRenderingContext2D && function() {
      function a(b) {
        return b.a;
      }
      function b(a) {
        return a.d;
      }
      CanvasRenderingContext2D.prototype.flashStroke = function(f, c) {
        var q = this.currentTransform;
        if (!q) {
          if (q = this.mozCurrentTransform) {
            q = m.Geometry.Matrix.createSVGMatrixFromArray(q);
          } else {
            this.stroke(f);
            return;
          }
        }
        var d = new Path2D;
        d.addPath(f, q);
        var g = this.lineWidth;
        this.setTransform(1, 0, 0, 1, 0, 0);
        switch(c) {
          case 1:
            this.lineWidth = t(g * (a(q) + b(q)) / 2, 1, 1024);
            break;
          case 2:
            this.lineWidth = t(g * b(q), 1, 1024);
            break;
          case 3:
            this.lineWidth = t(g * a(q), 1, 1024);
        }
        this.stroke(d);
        this.setTransform(q.a, q.b, q.c, q.d, q.e, q.f);
        this.lineWidth = g;
      };
    }();
    if ("undefined" !== typeof CanvasRenderingContext2D && void 0 === CanvasRenderingContext2D.prototype.globalColorMatrix) {
      var U = CanvasRenderingContext2D.prototype.fill, L = CanvasRenderingContext2D.prototype.stroke, I = CanvasRenderingContext2D.prototype.fillText, B = CanvasRenderingContext2D.prototype.strokeText;
      Object.defineProperty(CanvasRenderingContext2D.prototype, "globalColorMatrix", {get:function() {
        return this._globalColorMatrix ? this._globalColorMatrix.clone() : null;
      }, set:function(a) {
        a ? this._globalColorMatrix ? this._globalColorMatrix.set(a) : this._globalColorMatrix = a.clone() : this._globalColorMatrix = null;
      }, enumerable:!0, configurable:!0});
      CanvasRenderingContext2D.prototype.fill = function(a, b) {
        var f = null;
        this._globalColorMatrix && (f = this.fillStyle, this.fillStyle = s(this, this.fillStyle, this._globalColorMatrix));
        0 === arguments.length ? U.call(this) : 1 === arguments.length ? U.call(this, a) : 2 === arguments.length && U.call(this, a, b);
        f && (this.fillStyle = f);
      };
      CanvasRenderingContext2D.prototype.stroke = function(a, b) {
        var f = null;
        this._globalColorMatrix && (f = this.strokeStyle, this.strokeStyle = s(this, this.strokeStyle, this._globalColorMatrix));
        0 === arguments.length ? L.call(this) : 1 === arguments.length && L.call(this, a);
        f && (this.strokeStyle = f);
      };
      CanvasRenderingContext2D.prototype.fillText = function(a, b, f, c) {
        var q = null;
        this._globalColorMatrix && (q = this.fillStyle, this.fillStyle = s(this, this.fillStyle, this._globalColorMatrix));
        3 === arguments.length ? I.call(this, a, b, f) : 4 === arguments.length ? I.call(this, a, b, f, c) : d.Debug.unexpected();
        q && (this.fillStyle = q);
      };
      CanvasRenderingContext2D.prototype.strokeText = function(a, b, f, c) {
        var q = null;
        this._globalColorMatrix && (q = this.strokeStyle, this.strokeStyle = s(this, this.strokeStyle, this._globalColorMatrix));
        3 === arguments.length ? B.call(this, a, b, f) : 4 === arguments.length ? B.call(this, a, b, f, c) : d.Debug.unexpected();
        q && (this.strokeStyle = q);
      };
    }
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  var m = d.Debug.assert, s = function() {
    function d() {
      this._count = 0;
      this._head = this._tail = null;
    }
    Object.defineProperty(d.prototype, "count", {get:function() {
      return this._count;
    }, enumerable:!0, configurable:!0});
    Object.defineProperty(d.prototype, "head", {get:function() {
      return this._head;
    }, enumerable:!0, configurable:!0});
    d.prototype._unshift = function(d) {
      m(!d.next && !d.previous);
      0 === this._count ? this._head = this._tail = d : (d.next = this._head, this._head = d.next.previous = d);
      this._count++;
    };
    d.prototype._remove = function(d) {
      m(0 < this._count);
      d === this._head && d === this._tail ? this._head = this._tail = null : d === this._head ? (this._head = d.next, this._head.previous = null) : d == this._tail ? (this._tail = d.previous, this._tail.next = null) : (d.previous.next = d.next, d.next.previous = d.previous);
      d.previous = d.next = null;
      this._count--;
    };
    d.prototype.use = function(d) {
      this._head !== d && ((d.next || d.previous || this._tail === d) && this._remove(d), this._unshift(d));
    };
    d.prototype.pop = function() {
      if (!this._tail) {
        return null;
      }
      var d = this._tail;
      this._remove(d);
      return d;
    };
    d.prototype.visit = function(d, e) {
      "undefined" === typeof e && (e = !0);
      for (var a = e ? this._head : this._tail;a && d(a);) {
        a = e ? a.next : a.previous;
      }
    };
    return d;
  }();
  d.LRUList = s;
})(Shumway || (Shumway = {}));
var Shumway$$inline_46 = Shumway || (Shumway = {}), GFX$$inline_47 = Shumway$$inline_46.GFX || (Shumway$$inline_46.GFX = {}), Option$$inline_48 = Shumway$$inline_46.Options.Option, OptionSet$$inline_49 = Shumway$$inline_46.Options.OptionSet, shumwayOptions$$inline_50 = Shumway$$inline_46.Settings.shumwayOptions, rendererOptions$$inline_51 = shumwayOptions$$inline_50.register(new OptionSet$$inline_49("Renderer Options"));
GFX$$inline_47.imageUpdateOption = rendererOptions$$inline_51.register(new Option$$inline_48("", "imageUpdate", "boolean", !0, "Enable image updating."));
GFX$$inline_47.imageConvertOption = rendererOptions$$inline_51.register(new Option$$inline_48("", "imageConvert", "boolean", !0, "Enable image conversion."));
GFX$$inline_47.stageOptions = shumwayOptions$$inline_50.register(new OptionSet$$inline_49("Stage Renderer Options"));
GFX$$inline_47.forcePaint = GFX$$inline_47.stageOptions.register(new Option$$inline_48("", "forcePaint", "boolean", !1, "Force repainting."));
GFX$$inline_47.ignoreViewport = GFX$$inline_47.stageOptions.register(new Option$$inline_48("", "ignoreViewport", "boolean", !1, "Cull elements outside of the viewport."));
GFX$$inline_47.viewportLoupeDiameter = GFX$$inline_47.stageOptions.register(new Option$$inline_48("", "viewportLoupeDiameter", "number", 256, "Size of the viewport loupe.", {range:{min:1, max:1024, step:1}}));
GFX$$inline_47.disableClipping = GFX$$inline_47.stageOptions.register(new Option$$inline_48("", "disableClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_47.debugClipping = GFX$$inline_47.stageOptions.register(new Option$$inline_48("", "debugClipping", "boolean", !1, "Disable clipping."));
GFX$$inline_47.backend = GFX$$inline_47.stageOptions.register(new Option$$inline_48("", "backend", "number", 0, "Backends", {choices:{Canvas2D:0, WebGL:1, Both:2}}));
GFX$$inline_47.hud = GFX$$inline_47.stageOptions.register(new Option$$inline_48("", "hud", "boolean", !1, "Enable HUD."));
var webGLOptions$$inline_52 = GFX$$inline_47.stageOptions.register(new OptionSet$$inline_49("WebGL Options"));
GFX$$inline_47.perspectiveCamera = webGLOptions$$inline_52.register(new Option$$inline_48("", "pc", "boolean", !1, "Use perspective camera."));
GFX$$inline_47.perspectiveCameraFOV = webGLOptions$$inline_52.register(new Option$$inline_48("", "pcFOV", "number", 60, "Perspective Camera FOV."));
GFX$$inline_47.perspectiveCameraDistance = webGLOptions$$inline_52.register(new Option$$inline_48("", "pcDistance", "number", 2, "Perspective Camera Distance."));
GFX$$inline_47.perspectiveCameraAngle = webGLOptions$$inline_52.register(new Option$$inline_48("", "pcAngle", "number", 0, "Perspective Camera Angle."));
GFX$$inline_47.perspectiveCameraAngleRotate = webGLOptions$$inline_52.register(new Option$$inline_48("", "pcRotate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_47.perspectiveCameraSpacing = webGLOptions$$inline_52.register(new Option$$inline_48("", "pcSpacing", "number", .01, "Element Spacing."));
GFX$$inline_47.perspectiveCameraSpacingInflate = webGLOptions$$inline_52.register(new Option$$inline_48("", "pcInflate", "boolean", !1, "Rotate Use perspective camera."));
GFX$$inline_47.drawTiles = webGLOptions$$inline_52.register(new Option$$inline_48("", "drawTiles", "boolean", !1, "Draw WebGL Tiles"));
GFX$$inline_47.drawSurfaces = webGLOptions$$inline_52.register(new Option$$inline_48("", "drawSurfaces", "boolean", !1, "Draw WebGL Surfaces."));
GFX$$inline_47.drawSurface = webGLOptions$$inline_52.register(new Option$$inline_48("", "drawSurface", "number", -1, "Draw WebGL Surface #"));
GFX$$inline_47.drawElements = webGLOptions$$inline_52.register(new Option$$inline_48("", "drawElements", "boolean", !0, "Actually call gl.drawElements. This is useful to test if the GPU is the bottleneck."));
GFX$$inline_47.disableSurfaceUploads = webGLOptions$$inline_52.register(new Option$$inline_48("", "disableSurfaceUploads", "boolean", !1, "Disable surface uploads."));
GFX$$inline_47.premultipliedAlpha = webGLOptions$$inline_52.register(new Option$$inline_48("", "premultipliedAlpha", "boolean", !1, "Set the premultipliedAlpha flag on getContext()."));
GFX$$inline_47.unpackPremultiplyAlpha = webGLOptions$$inline_52.register(new Option$$inline_48("", "unpackPremultiplyAlpha", "boolean", !0, "Use UNPACK_PREMULTIPLY_ALPHA_WEBGL in pixelStorei."));
var factorChoices$$inline_53 = {ZERO:0, ONE:1, SRC_COLOR:768, ONE_MINUS_SRC_COLOR:769, DST_COLOR:774, ONE_MINUS_DST_COLOR:775, SRC_ALPHA:770, ONE_MINUS_SRC_ALPHA:771, DST_ALPHA:772, ONE_MINUS_DST_ALPHA:773, SRC_ALPHA_SATURATE:776, CONSTANT_COLOR:32769, ONE_MINUS_CONSTANT_COLOR:32770, CONSTANT_ALPHA:32771, ONE_MINUS_CONSTANT_ALPHA:32772};
GFX$$inline_47.sourceBlendFactor = webGLOptions$$inline_52.register(new Option$$inline_48("", "sourceBlendFactor", "number", factorChoices$$inline_53.ONE, "", {choices:factorChoices$$inline_53}));
GFX$$inline_47.destinationBlendFactor = webGLOptions$$inline_52.register(new Option$$inline_48("", "destinationBlendFactor", "number", factorChoices$$inline_53.ONE_MINUS_SRC_ALPHA, "", {choices:factorChoices$$inline_53}));
var canvas2DOptions$$inline_54 = GFX$$inline_47.stageOptions.register(new OptionSet$$inline_49("Canvas2D Options"));
GFX$$inline_47.clipDirtyRegions = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "clipDirtyRegions", "boolean", !1, "Clip dirty regions."));
GFX$$inline_47.clipCanvas = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "clipCanvas", "boolean", !1, "Clip Regions."));
GFX$$inline_47.cull = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "cull", "boolean", !1, "Enable culling."));
GFX$$inline_47.compositeMask = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "compositeMask", "boolean", !1, "Composite Mask."));
GFX$$inline_47.snapToDevicePixels = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "snapToDevicePixels", "boolean", !1, ""));
GFX$$inline_47.imageSmoothing = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "imageSmoothing", "boolean", !1, ""));
GFX$$inline_47.blending = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "blending", "boolean", !0, ""));
GFX$$inline_47.cacheShapes = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "cacheShapes", "boolean", !1, ""));
GFX$$inline_47.cacheShapesMaxSize = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "cacheShapesMaxSize", "number", 256, "", {range:{min:1, max:1024, step:1}}));
GFX$$inline_47.cacheShapesThreshold = canvas2DOptions$$inline_54.register(new Option$$inline_48("", "cacheShapesThreshold", "number", 256, "", {range:{min:1, max:1024, step:1}}));
var GFX$$inline_55 = Shumway$$inline_46.GFX;
(function(d) {
  (function(m) {
    (function(s) {
      function e(a, b, c, d) {
        var g = 1 - d;
        return a * g * g + 2 * b * g * d + c * d * d;
      }
      function t(a, b, c, d, g) {
        var e = g * g, h = 1 - g, l = h * h;
        return a * h * l + 3 * b * g * l + 3 * c * h * e + d * g * e;
      }
      var k = d.NumberUtilities.clamp, a = d.NumberUtilities.pow2, c = d.NumberUtilities.epsilonEquals, g = d.Debug.assert;
      s.radianToDegrees = function(a) {
        return 180 * a / Math.PI;
      };
      s.degreesToRadian = function(a) {
        return a * Math.PI / 180;
      };
      s.quadraticBezier = e;
      s.quadraticBezierExtreme = function(a, b, c) {
        var d = (a - b) / (a - 2 * b + c);
        return 0 > d ? a : 1 < d ? c : e(a, b, c, d);
      };
      s.cubicBezier = t;
      s.cubicBezierExtremes = function(a, b, c, d) {
        var g = b - a, e;
        e = 2 * (c - b);
        var h = d - c;
        g + h === e && (h *= 1.0001);
        var l = 2 * g - e, p = e - 2 * g, p = Math.sqrt(p * p - 4 * g * (g - e + h));
        e = 2 * (g - e + h);
        g = (l + p) / e;
        l = (l - p) / e;
        p = [];
        0 <= g && 1 >= g && p.push(t(a, b, c, d, g));
        0 <= l && 1 >= l && p.push(t(a, b, c, d, l));
        return p;
      };
      var p = function() {
        function a(b, c) {
          this.x = b;
          this.y = c;
        }
        a.prototype.setElements = function(a, b) {
          this.x = a;
          this.y = b;
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
        a.prototype.toString = function() {
          return "{x: " + this.x + ", y: " + this.y + "}";
        };
        a.prototype.inTriangle = function(a, b, c) {
          var d = a.y * c.x - a.x * c.y + (c.y - a.y) * this.x + (a.x - c.x) * this.y, g = a.x * b.y - a.y * b.x + (a.y - b.y) * this.x + (b.x - a.x) * this.y;
          if (0 > d != 0 > g) {
            return!1;
          }
          a = -b.y * c.x + a.y * (c.x - b.x) + a.x * (b.y - c.y) + b.x * c.y;
          0 > a && (d = -d, g = -g, a = -a);
          return 0 < d && 0 < g && d + g < a;
        };
        a.createEmpty = function() {
          return new a(0, 0);
        };
        a.createEmptyPoints = function(b) {
          for (var c = [], d = 0;d < b;d++) {
            c.push(new a(0, 0));
          }
          return c;
        };
        return a;
      }();
      s.Point = p;
      var v = function() {
        function a(b, c, d) {
          this.x = b;
          this.y = c;
          this.z = d;
        }
        a.prototype.setElements = function(a, b, c) {
          this.x = a;
          this.y = b;
          this.z = c;
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
          var b = this.z * a.x - this.x * a.z, c = this.x * a.y - this.y * a.x;
          this.x = this.y * a.z - this.z * a.y;
          this.y = b;
          this.z = c;
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
        a.prototype.toString = function() {
          return "{x: " + this.x + ", y: " + this.y + ", z: " + this.z + "}";
        };
        a.createEmpty = function() {
          return new a(0, 0, 0);
        };
        a.createEmptyPoints = function(b) {
          for (var c = [], d = 0;d < b;d++) {
            c.push(new a(0, 0, 0));
          }
          return c;
        };
        return a;
      }();
      s.Point3D = v;
      var u = function() {
        function a(b, c, d, g) {
          this.setElements(b, c, d, g);
        }
        a.prototype.setElements = function(a, b, c, d) {
          this.x = a;
          this.y = b;
          this.w = c;
          this.h = d;
        };
        a.prototype.set = function(a) {
          this.x = a.x;
          this.y = a.y;
          this.w = a.w;
          this.h = a.h;
        };
        a.prototype.contains = function(a) {
          var b = a.x + a.w, c = a.y + a.h, d = this.x + this.w, g = this.y + this.h;
          return a.x >= this.x && a.x < d && a.y >= this.y && a.y < g && b > this.x && b <= d && c > this.y && c <= g;
        };
        a.prototype.containsPoint = function(a) {
          return a.x >= this.x && a.x < this.x + this.w && a.y >= this.y && a.y < this.y + this.h;
        };
        a.prototype.isContained = function(a) {
          for (var b = 0;b < a.length;b++) {
            if (a[b].contains(this)) {
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
            var b = this.x, c = this.y;
            this.x > a.x && (b = a.x);
            this.y > a.y && (c = a.y);
            var d = this.x + this.w;
            d < a.x + a.w && (d = a.x + a.w);
            var g = this.y + this.h;
            g < a.y + a.h && (g = a.y + a.h);
            this.x = b;
            this.y = c;
            this.w = d - b;
            this.h = g - c;
          }
        };
        a.prototype.isEmpty = function() {
          return 0 >= this.w || 0 >= this.h;
        };
        a.prototype.setEmpty = function() {
          this.h = this.w = 0;
        };
        a.prototype.intersect = function(b) {
          var c = a.createEmpty();
          if (this.isEmpty() || b.isEmpty()) {
            return c.setEmpty(), c;
          }
          c.x = Math.max(this.x, b.x);
          c.y = Math.max(this.y, b.y);
          c.w = Math.min(this.x + this.w, b.x + b.w) - c.x;
          c.h = Math.min(this.y + this.h, b.y + b.h) - c.y;
          c.isEmpty() && c.setEmpty();
          this.set(c);
        };
        a.prototype.intersects = function(a) {
          if (this.isEmpty() || a.isEmpty()) {
            return!1;
          }
          var b = Math.max(this.x, a.x), c = Math.max(this.y, a.y), b = Math.min(this.x + this.w, a.x + a.w) - b;
          a = Math.min(this.y + this.h, a.y + a.h) - c;
          return!(0 >= b || 0 >= a);
        };
        a.prototype.intersectsTransformedAABB = function(b, c) {
          var d = a._temporary;
          d.set(b);
          c.transformRectangleAABB(d);
          return this.intersects(d);
        };
        a.prototype.intersectsTranslated = function(a, b, c) {
          if (this.isEmpty() || a.isEmpty()) {
            return!1;
          }
          var d = Math.max(this.x, a.x + b), g = Math.max(this.y, a.y + c);
          b = Math.min(this.x + this.w, a.x + b + a.w) - d;
          a = Math.min(this.y + this.h, a.y + c + a.h) - g;
          return!(0 >= b || 0 >= a);
        };
        a.prototype.area = function() {
          return this.w * this.h;
        };
        a.prototype.clone = function() {
          return new a(this.x, this.y, this.w, this.h);
        };
        a.prototype.snap = function() {
          var a = Math.ceil(this.x + this.w), b = Math.ceil(this.y + this.h);
          this.x = Math.floor(this.x);
          this.y = Math.floor(this.y);
          this.w = a - this.x;
          this.h = b - this.y;
          return this;
        };
        a.prototype.scale = function(a, b) {
          this.x *= a;
          this.y *= b;
          this.w *= a;
          this.h *= b;
          return this;
        };
        a.prototype.offset = function(a, b) {
          this.x += a;
          this.y += b;
          return this;
        };
        a.prototype.resize = function(a, b) {
          this.w += a;
          this.h += b;
          return this;
        };
        a.prototype.expand = function(a, b) {
          this.offset(-a, -b).resize(2 * a, 2 * b);
          return this;
        };
        a.prototype.getCenter = function() {
          return new p(this.x + this.w / 2, this.y + this.h / 2);
        };
        a.prototype.getAbsoluteBounds = function() {
          return new a(0, 0, this.w, this.h);
        };
        a.prototype.toString = function() {
          return "{" + this.x + ", " + this.y + ", " + this.w + ", " + this.h + "}";
        };
        a.createEmpty = function() {
          return new a(0, 0, 0, 0);
        };
        a.createSquare = function() {
          return new a(-512, -512, 1024, 1024);
        };
        a.createMaxI16 = function() {
          return new a(d.Numbers.MinI16, d.Numbers.MinI16, 65535, 65535);
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
        a._temporary = a.createEmpty();
        return a;
      }();
      s.Rectangle = u;
      var l = function() {
        function a(b) {
          this.corners = b.map(function(a) {
            return a.clone();
          });
          this.axes = [b[1].clone().sub(b[0]), b[3].clone().sub(b[0])];
          this.origins = [];
          for (var c = 0;2 > c;c++) {
            this.axes[c].mul(1 / this.axes[c].squaredLength()), this.origins.push(b[0].dot(this.axes[c]));
          }
        }
        a.prototype.getBounds = function() {
          return a.getBounds(this.corners);
        };
        a.getBounds = function(a) {
          for (var b = new p(Number.MAX_VALUE, Number.MAX_VALUE), c = new p(Number.MIN_VALUE, Number.MIN_VALUE), d = 0;4 > d;d++) {
            var g = a[d].x, e = a[d].y;
            b.x = Math.min(b.x, g);
            b.y = Math.min(b.y, e);
            c.x = Math.max(c.x, g);
            c.y = Math.max(c.y, e);
          }
          return new u(b.x, b.y, c.x - b.x, c.y - b.y);
        };
        a.prototype.intersects = function(a) {
          return this.intersectsOneWay(a) && a.intersectsOneWay(this);
        };
        a.prototype.intersectsOneWay = function(a) {
          for (var b = 0;2 > b;b++) {
            for (var c = 0;4 > c;c++) {
              var d = a.corners[c].dot(this.axes[b]), g, e;
              0 === c ? e = g = d : d < g ? g = d : d > e && (e = d);
            }
            if (g > 1 + this.origins[b] || e < this.origins[b]) {
              return!1;
            }
          }
          return!0;
        };
        return a;
      }();
      s.OBB = l;
      var r = function() {
        function a(b, c, d, g, e, h) {
          this._data = new Float64Array(6);
          this.setElements(b, c, d, g, e, h);
        }
        Object.defineProperty(a.prototype, "a", {get:function() {
          return this._data[0];
        }, set:function(a) {
          this._data[0] = a;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "b", {get:function() {
          return this._data[1];
        }, set:function(a) {
          this._data[1] = a;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "c", {get:function() {
          return this._data[2];
        }, set:function(a) {
          this._data[2] = a;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "d", {get:function() {
          return this._data[3];
        }, set:function(a) {
          this._data[3] = a;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "tx", {get:function() {
          return this._data[4];
        }, set:function(a) {
          this._data[4] = a;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(a.prototype, "ty", {get:function() {
          return this._data[5];
        }, set:function(a) {
          this._data[5] = a;
        }, enumerable:!0, configurable:!0});
        a.prototype.setElements = function(a, b, c, d, g, e) {
          var h = this._data;
          h[0] = a;
          h[1] = b;
          h[2] = c;
          h[3] = d;
          h[4] = g;
          h[5] = e;
        };
        a.prototype.set = function(a) {
          var b = this._data;
          a = a._data;
          b[0] = a[0];
          b[1] = a[1];
          b[2] = a[2];
          b[3] = a[3];
          b[4] = a[4];
          b[5] = a[5];
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
          var b = this._data;
          a = a._data;
          return b[0] === a[0] && b[1] === a[1] && b[2] === a[2] && b[3] === a[3] && b[4] === a[4] && b[5] === a[5];
        };
        a.prototype.clone = function() {
          var b = this._data;
          return new a(b[0], b[1], b[2], b[3], b[4], b[5]);
        };
        a.prototype.transform = function(a, b, c, d, g, e) {
          var h = this._data, n = h[0], l = h[1], p = h[2], r = h[3], k = h[4], m = h[5];
          h[0] = n * a + p * b;
          h[1] = l * a + r * b;
          h[2] = n * c + p * d;
          h[3] = l * c + r * d;
          h[4] = n * g + p * e + k;
          h[5] = l * g + r * e + m;
          return this;
        };
        a.prototype.transformRectangle = function(a, b) {
          var c = this._data, d = c[0], g = c[1], e = c[2], h = c[3], n = c[4], c = c[5], l = a.x, p = a.y, r = a.w, k = a.h;
          b[0].x = d * l + e * p + n;
          b[0].y = g * l + h * p + c;
          b[1].x = d * (l + r) + e * p + n;
          b[1].y = g * (l + r) + h * p + c;
          b[2].x = d * (l + r) + e * (p + k) + n;
          b[2].y = g * (l + r) + h * (p + k) + c;
          b[3].x = d * l + e * (p + k) + n;
          b[3].y = g * l + h * (p + k) + c;
        };
        a.prototype.isTranslationOnly = function() {
          var a = this._data;
          return 1 === a[0] && 0 === a[1] && 0 === a[2] && 1 === a[3] || c(a[0], 1) && c(a[1], 0) && c(a[2], 0) && c(a[3], 1) ? !0 : !1;
        };
        a.prototype.transformRectangleAABB = function(a) {
          var b = this._data, c = b[0], d = b[1], g = b[2], e = b[3], h = b[4], n = b[5], l = a.x, p = a.y, r = a.w, k = a.h, b = c * l + g * p + h, m = d * l + e * p + n, w = c * (l + r) + g * p + h, v = d * (l + r) + e * p + n, u = c * (l + r) + g * (p + k) + h, r = d * (l + r) + e * (p + k) + n, c = c * l + g * (p + k) + h, d = d * l + e * (p + k) + n, e = 0;
          b > w && (e = b, b = w, w = e);
          u > c && (e = u, u = c, c = e);
          a.x = b < u ? b : u;
          a.w = (w > c ? w : c) - a.x;
          m > v && (e = m, m = v, v = e);
          r > d && (e = r, r = d, d = e);
          a.y = m < r ? m : r;
          a.h = (v > d ? v : d) - a.y;
        };
        a.prototype.scale = function(a, b) {
          var c = this._data;
          c[0] *= a;
          c[1] *= b;
          c[2] *= a;
          c[3] *= b;
          c[4] *= a;
          c[5] *= b;
          return this;
        };
        a.prototype.scaleClone = function(a, b) {
          return 1 === a && 1 === b ? this : this.clone().scale(a, b);
        };
        a.prototype.rotate = function(a) {
          var b = this._data, c = b[0], d = b[1], g = b[2], e = b[3], h = b[4], n = b[5], l = Math.cos(a);
          a = Math.sin(a);
          b[0] = l * c - a * d;
          b[1] = a * c + l * d;
          b[2] = l * g - a * e;
          b[3] = a * g + l * e;
          b[4] = l * h - a * n;
          b[5] = a * h + l * n;
          return this;
        };
        a.prototype.concat = function(a) {
          var b = this._data;
          a = a._data;
          var c = b[0] * a[0], d = 0, g = 0, e = b[3] * a[3], h = b[4] * a[0] + a[4], n = b[5] * a[3] + a[5];
          if (0 !== b[1] || 0 !== b[2] || 0 !== a[1] || 0 !== a[2]) {
            c += b[1] * a[2], e += b[2] * a[1], d += b[0] * a[1] + b[1] * a[3], g += b[2] * a[0] + b[3] * a[2], h += b[5] * a[2], n += b[4] * a[1];
          }
          b[0] = c;
          b[1] = d;
          b[2] = g;
          b[3] = e;
          b[4] = h;
          b[5] = n;
        };
        a.prototype.preMultiply = function(a) {
          var b = this._data;
          a = a._data;
          var c = a[0] * b[0], d = 0, g = 0, e = a[3] * b[3], h = a[4] * b[0] + b[4], n = a[5] * b[3] + b[5];
          if (0 !== a[1] || 0 !== a[2] || 0 !== b[1] || 0 !== b[2]) {
            c += a[1] * b[2], e += a[2] * b[1], d += a[0] * b[1] + a[1] * b[3], g += a[2] * b[0] + a[3] * b[2], h += a[5] * b[2], n += a[4] * b[1];
          }
          b[0] = c;
          b[1] = d;
          b[2] = g;
          b[3] = e;
          b[4] = h;
          b[5] = n;
        };
        a.prototype.translate = function(a, b) {
          var c = this._data;
          c[4] += a;
          c[5] += b;
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
        };
        a.prototype.isIdentity = function() {
          var a = this._data;
          return 1 === a[0] && 0 === a[1] && 0 === a[2] && 1 === a[3] && 0 === a[4] && 0 === a[5];
        };
        a.prototype.transformPoint = function(a) {
          var b = this._data, c = a.x, d = a.y;
          a.x = b[0] * c + b[2] * d + b[4];
          a.y = b[1] * c + b[3] * d + b[5];
        };
        a.prototype.transformPoints = function(a) {
          for (var b = 0;b < a.length;b++) {
            this.transformPoint(a[b]);
          }
        };
        a.prototype.deltaTransformPoint = function(a) {
          var b = this._data, c = a.x, d = a.y;
          a.x = b[0] * c + b[2] * d;
          a.y = b[1] * c + b[3] * d;
        };
        a.prototype.inverse = function(a) {
          var b = this._data, c = a._data, d = b[1], g = b[2], e = b[4], h = b[5];
          if (0 === d && 0 === g) {
            var n = c[0] = 1 / b[0], b = c[3] = 1 / b[3];
            c[1] = 0;
            c[2] = 0;
            c[4] = -n * e;
            c[5] = -b * h;
          } else {
            var n = b[0], b = b[3], l = n * b - d * g;
            0 === l ? a.setIdentity() : (l = 1 / l, c[0] = b * l, d = c[1] = -d * l, g = c[2] = -g * l, b = c[3] = n * l, c[4] = -(c[0] * e + g * h), c[5] = -(d * e + b * h));
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
          var b = Math.sqrt(a[0] * a[0] + a[1] * a[1]);
          return 0 < a[0] ? b : -b;
        };
        a.prototype.getScaleY = function() {
          var a = this._data;
          if (0 === a[2] && 1 === a[3]) {
            return 1;
          }
          var b = Math.sqrt(a[2] * a[2] + a[3] * a[3]);
          return 0 < a[3] ? b : -b;
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
        a.prototype.toString = function() {
          var a = this._data;
          return "{" + a[0] + ", " + a[1] + ", " + a[2] + ", " + a[3] + ", " + a[4] + ", " + a[5] + "}";
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
          return new a(1, 0, 0, 1, 0, 0);
        };
        a.prototype.toSVGMatrix = function() {
          var b = this._data, c = a._svg.createSVGMatrix();
          c.a = b[0];
          c.b = b[1];
          c.c = b[2];
          c.d = b[3];
          c.e = b[4];
          c.f = b[5];
          return c;
        };
        a.prototype.snap = function() {
          var a = this._data;
          return this.isTranslationOnly() ? (a[0] = 1, a[1] = 0, a[2] = 0, a[3] = 1, a[4] = Math.round(a[4]), a[5] = Math.round(a[5]), !0) : !1;
        };
        a.createIdentitySVGMatrix = function() {
          return a._svg.createSVGMatrix();
        };
        a.createSVGMatrixFromArray = function(b) {
          var c = a._svg.createSVGMatrix();
          c.a = b[0];
          c.b = b[1];
          c.c = b[2];
          c.d = b[3];
          c.e = b[4];
          c.f = b[5];
          return c;
        };
        a._svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
        a.multiply = function(a, b) {
          var c = b._data;
          a.transform(c[0], c[1], c[2], c[3], c[4], c[5]);
        };
        return a;
      }();
      s.Matrix = r;
      r = function() {
        function a(b) {
          this._m = new Float32Array(b);
        }
        a.prototype.asWebGLMatrix = function() {
          return this._m;
        };
        a.createCameraLookAt = function(b, c, d) {
          c = b.clone().sub(c).normalize();
          d = d.clone().cross(c).normalize();
          var g = c.clone().cross(d);
          return new a([d.x, d.y, d.z, 0, g.x, g.y, g.z, 0, c.x, c.y, c.z, 0, b.x, b.y, b.z, 1]);
        };
        a.createLookAt = function(b, c, d) {
          c = b.clone().sub(c).normalize();
          d = d.clone().cross(c).normalize();
          var g = c.clone().cross(d);
          return new a([d.x, g.x, c.x, 0, g.x, g.y, c.y, 0, c.x, g.z, c.z, 0, -d.dot(b), -g.dot(b), -c.dot(b), 1]);
        };
        a.prototype.mul = function(a) {
          a = [a.x, a.y, a.z, 0];
          for (var b = this._m, c = [], d = 0;4 > d;d++) {
            c[d] = 0;
            for (var g = 4 * d, e = 0;4 > e;e++) {
              c[d] += b[g + e] * a[e];
            }
          }
          return new v(c[0], c[1], c[2]);
        };
        a.create2DProjection = function(b, c, d) {
          return new a([2 / b, 0, 0, 0, 0, -2 / c, 0, 0, 0, 0, 2 / d, 0, -1, 1, 0, 1]);
        };
        a.createPerspective = function(b) {
          b = Math.tan(.5 * Math.PI - .5 * b);
          var c = 1 / -4999.9;
          return new a([b / 1, 0, 0, 0, 0, b, 0, 0, 0, 0, 5000.1 * c, -1, 0, 0, 1E3 * c, 0]);
        };
        a.createIdentity = function() {
          return a.createTranslation(0, 0);
        };
        a.createTranslation = function(b, c) {
          return new a([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, b, c, 0, 1]);
        };
        a.createXRotation = function(b) {
          var c = Math.cos(b);
          b = Math.sin(b);
          return new a([1, 0, 0, 0, 0, c, b, 0, 0, -b, c, 0, 0, 0, 0, 1]);
        };
        a.createYRotation = function(b) {
          var c = Math.cos(b);
          b = Math.sin(b);
          return new a([c, 0, -b, 0, 0, 1, 0, 0, b, 0, c, 0, 0, 0, 0, 1]);
        };
        a.createZRotation = function(b) {
          var c = Math.cos(b);
          b = Math.sin(b);
          return new a([c, b, 0, 0, -b, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
        };
        a.createScale = function(b, c, d) {
          return new a([b, 0, 0, 0, 0, c, 0, 0, 0, 0, d, 0, 0, 0, 0, 1]);
        };
        a.createMultiply = function(b, c) {
          var d = b._m, g = c._m, e = d[0], h = d[1], l = d[2], p = d[3], r = d[4], k = d[5], m = d[6], w = d[7], v = d[8], u = d[9], s = d[10], t = d[11], y = d[12], A = d[13], D = d[14], d = d[15], J = g[0], H = g[1], K = g[2], M = g[3], N = g[4], O = g[5], P = g[6], Q = g[7], R = g[8], S = g[9], T = g[10], W = g[11], X = g[12], Y = g[13], Z = g[14], g = g[15];
          return new a([e * J + h * N + l * R + p * X, e * H + h * O + l * S + p * Y, e * K + h * P + l * T + p * Z, e * M + h * Q + l * W + p * g, r * J + k * N + m * R + w * X, r * H + k * O + m * S + w * Y, r * K + k * P + m * T + w * Z, r * M + k * Q + m * W + w * g, v * J + u * N + s * R + t * X, v * H + u * O + s * S + t * Y, v * K + u * P + s * T + t * Z, v * M + u * Q + s * W + t * g, y * J + A * N + D * R + d * X, y * H + A * O + D * S + d * Y, y * K + A * P + D * T + d * Z, y * M + A * 
          Q + D * W + d * g]);
        };
        a.createInverse = function(b) {
          var c = b._m;
          b = c[0];
          var d = c[1], g = c[2], e = c[3], h = c[4], l = c[5], p = c[6], r = c[7], k = c[8], m = c[9], w = c[10], v = c[11], u = c[12], s = c[13], t = c[14], c = c[15], y = w * c, A = t * v, D = p * c, J = t * r, H = p * v, K = w * r, M = g * c, N = t * e, O = g * v, P = w * e, Q = g * r, R = p * e, S = k * s, T = u * m, W = h * s, X = u * l, Y = h * m, Z = k * l, aa = b * s, ba = u * d, ca = b * m, da = k * d, ea = b * l, fa = h * d, ha = y * l + J * m + H * s - (A * l + D * m + K * s), ia = A * 
          d + M * m + P * s - (y * d + N * m + O * s), s = D * d + N * l + Q * s - (J * d + M * l + R * s), d = K * d + O * l + R * m - (H * d + P * l + Q * m), l = 1 / (b * ha + h * ia + k * s + u * d);
          return new a([l * ha, l * ia, l * s, l * d, l * (A * h + D * k + K * u - (y * h + J * k + H * u)), l * (y * b + N * k + O * u - (A * b + M * k + P * u)), l * (J * b + M * h + R * u - (D * b + N * h + Q * u)), l * (H * b + P * h + Q * k - (K * b + O * h + R * k)), l * (S * r + X * v + Y * c - (T * r + W * v + Z * c)), l * (T * e + aa * v + da * c - (S * e + ba * v + ca * c)), l * (W * e + ba * r + ea * c - (X * e + aa * r + fa * c)), l * (Z * e + ca * r + fa * v - (Y * e + da * r + ea * 
          v)), l * (W * w + Z * t + T * p - (Y * t + S * p + X * w)), l * (ca * t + S * g + ba * w - (aa * w + da * t + T * g)), l * (aa * p + fa * t + X * g - (ea * t + W * g + ba * p)), l * (ea * w + Y * g + da * p - (ca * p + fa * w + Z * g))]);
        };
        return a;
      }();
      s.Matrix3D = r;
      r = function() {
        function a(b, c, d) {
          "undefined" === typeof d && (d = 7);
          var g = this.size = 1 << d;
          this.sizeInBits = d;
          this.w = b;
          this.h = c;
          this.c = Math.ceil(b / g);
          this.r = Math.ceil(c / g);
          this.grid = [];
          for (b = 0;b < this.r;b++) {
            for (this.grid.push([]), c = 0;c < this.c;c++) {
              this.grid[b][c] = new a.Cell(new u(c * g, b * g, g, g));
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
          return new u(0, 0, this.w, this.h);
        };
        a.prototype.addDirtyRectangle = function(a) {
          var b = a.x >> this.sizeInBits, c = a.y >> this.sizeInBits;
          if (!(b >= this.c || c >= this.r)) {
            0 > b && (b = 0);
            0 > c && (c = 0);
            var d = this.grid[c][b];
            a = a.clone();
            a.snap();
            if (d.region.contains(a)) {
              d.bounds.isEmpty() ? d.bounds.set(a) : d.bounds.contains(a) || d.bounds.union(a);
            } else {
              for (var g = Math.min(this.c, Math.ceil((a.x + a.w) / this.size)) - b, e = Math.min(this.r, Math.ceil((a.y + a.h) / this.size)) - c, h = 0;h < g;h++) {
                for (var n = 0;n < e;n++) {
                  d = this.grid[c + n][b + h], d = d.region.clone(), d.intersect(a), d.isEmpty() || this.addDirtyRectangle(d);
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
            for (var d = 0;d < this.c;d++) {
              b += this.grid[c][d].region.area();
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
            for (var d = 0;d < this.r;d++) {
              for (var g = 0;g < this.c;g++) {
                var e = this.grid[d][g];
                a.beginPath();
                c(e.region);
                a.closePath();
                a.stroke();
              }
            }
          }
          a.strokeStyle = "#E0F8D8";
          for (d = 0;d < this.r;d++) {
            for (g = 0;g < this.c;g++) {
              e = this.grid[d][g], a.beginPath(), c(e.bounds), a.closePath(), a.stroke();
            }
          }
        };
        a.tmpRectangle = u.createEmpty();
        return a;
      }();
      s.DirtyRegion = r;
      (function(a) {
        var b = function() {
          function a(b) {
            this.region = b;
            this.bounds = u.createEmpty();
          }
          a.prototype.clear = function() {
            this.bounds.setEmpty();
          };
          return a;
        }();
        a.Cell = b;
      })(s.DirtyRegion || (s.DirtyRegion = {}));
      var r = s.DirtyRegion, b = function() {
        function a(b, c, d, g, e, h) {
          this.index = b;
          this.x = c;
          this.y = d;
          this.scale = h;
          this.bounds = new u(c * g, d * e, g, e);
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
      s.Tile = b;
      var h = function() {
        function a(c, d, e, h, n) {
          this.tileW = e;
          this.tileH = h;
          this.scale = n;
          this.w = c;
          this.h = d;
          this.rows = Math.ceil(d / h);
          this.columns = Math.ceil(c / e);
          g(2048 > this.rows && 2048 > this.columns);
          this.tiles = [];
          for (d = c = 0;d < this.rows;d++) {
            for (var l = 0;l < this.columns;l++) {
              this.tiles.push(new b(c++, l, d, e, h, n));
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
        a.prototype.getFewTiles = function(b, c, d) {
          "undefined" === typeof d && (d = !0);
          if (c.isTranslationOnly() && 1 === this.tiles.length) {
            return this.tiles[0].bounds.intersectsTranslated(b, c.tx, c.ty) ? [this.tiles[0]] : [];
          }
          c.transformRectangle(b, a._points);
          var g;
          b = new u(0, 0, this.w, this.h);
          d && (g = new l(a._points));
          b.intersect(l.getBounds(a._points));
          if (b.isEmpty()) {
            return[];
          }
          var e = b.x / this.tileW | 0;
          c = b.y / this.tileH | 0;
          var h = Math.ceil((b.x + b.w) / this.tileW) | 0, p = Math.ceil((b.y + b.h) / this.tileH) | 0, e = k(e, 0, this.columns), h = k(h, 0, this.columns);
          c = k(c, 0, this.rows);
          for (var p = k(p, 0, this.rows), r = [];e < h;e++) {
            for (var m = c;m < p;m++) {
              var w = this.tiles[m * this.columns + e];
              w.bounds.intersects(b) && (d ? w.getOBB().intersects(g) : 1) && r.push(w);
            }
          }
          return r;
        };
        a.prototype.getManyTiles = function(b, c) {
          function d(a, b, c) {
            return(a - b.x) * (c.y - b.y) / (c.x - b.x) + b.y;
          }
          function g(a, b, c, f, d) {
            if (!(0 > c || c >= b.columns)) {
              for (f = k(f, 0, b.rows), d = k(d + 1, 0, b.rows);f < d;f++) {
                a.push(b.tiles[f * b.columns + c]);
              }
            }
          }
          var e = a._points;
          c.transformRectangle(b, e);
          for (var h = e[0].x < e[1].x ? 0 : 1, l = e[2].x < e[3].x ? 2 : 3, l = e[h].x < e[l].x ? h : l, h = [], p = 0;5 > p;p++, l++) {
            h.push(e[l % 4]);
          }
          (h[1].x - h[0].x) * (h[3].y - h[0].y) < (h[1].y - h[0].y) * (h[3].x - h[0].x) && (e = h[1], h[1] = h[3], h[3] = e);
          var e = [], r, m, l = Math.floor(h[0].x / this.tileW), p = (l + 1) * this.tileW;
          if (h[2].x < p) {
            r = Math.min(h[0].y, h[1].y, h[2].y, h[3].y);
            m = Math.max(h[0].y, h[1].y, h[2].y, h[3].y);
            var w = Math.floor(r / this.tileH), v = Math.floor(m / this.tileH);
            g(e, this, l, w, v);
            return e;
          }
          var u = 0, s = 4, t = !1;
          if (h[0].x === h[1].x || h[0].x === h[3].x) {
            h[0].x === h[1].x ? (t = !0, u++) : s--, r = d(p, h[u], h[u + 1]), m = d(p, h[s], h[s - 1]), w = Math.floor(h[u].y / this.tileH), v = Math.floor(h[s].y / this.tileH), g(e, this, l, w, v), l++;
          }
          do {
            var F, y, A, D;
            h[u + 1].x < p ? (F = h[u + 1].y, A = !0) : (F = d(p, h[u], h[u + 1]), A = !1);
            h[s - 1].x < p ? (y = h[s - 1].y, D = !0) : (y = d(p, h[s], h[s - 1]), D = !1);
            w = Math.floor((h[u].y < h[u + 1].y ? r : F) / this.tileH);
            v = Math.floor((h[s].y > h[s - 1].y ? m : y) / this.tileH);
            g(e, this, l, w, v);
            if (A && t) {
              break;
            }
            A ? (t = !0, u++, r = d(p, h[u], h[u + 1])) : r = F;
            D ? (s--, m = d(p, h[s], h[s - 1])) : m = y;
            l++;
            p = (l + 1) * this.tileW;
          } while (u < s);
          return e;
        };
        a._points = p.createEmptyPoints(4);
        return a;
      }();
      s.TileCache = h;
      r = function() {
        function b(a, c, d) {
          this._cacheLevels = [];
          this._source = a;
          this._tileSize = c;
          this._minUntiledSize = d;
        }
        b.prototype._getTilesAtScale = function(b, c, d) {
          var e = Math.max(c.getAbsoluteScaleX(), c.getAbsoluteScaleY()), n = 0;
          1 !== e && (n = k(Math.round(Math.log(1 / e) / Math.LN2), -5, 3));
          e = a(n);
          if (this._source.hasFlags(1)) {
            for (;;) {
              e = a(n);
              if (d.contains(this._source.getBounds().getAbsoluteBounds().clone().scale(e, e))) {
                break;
              }
              n--;
              g(-5 <= n);
            }
          }
          this._source.hasFlags(4) || (n = k(n, -5, 0));
          e = a(n);
          d = 5 + n;
          n = this._cacheLevels[d];
          if (!n) {
            var n = this._source.getBounds().getAbsoluteBounds().clone().scale(e, e), l, p;
            this._source.hasFlags(1) || !this._source.hasFlags(8) || Math.max(n.w, n.h) <= this._minUntiledSize ? (l = n.w, p = n.h) : l = p = this._tileSize;
            n = this._cacheLevels[d] = new h(n.w, n.h, l, p, e);
          }
          return n.getTiles(b, c.scaleClone(e, e));
        };
        b.prototype.fetchTiles = function(a, b, c, d) {
          var g = new u(0, 0, c.canvas.width, c.canvas.height);
          a = this._getTilesAtScale(a, b, g);
          var h;
          b = this._source;
          for (var e = 0;e < a.length;e++) {
            var n = a[e];
            n.cachedSurfaceRegion && n.cachedSurfaceRegion.surface && !b.hasFlags(3) || (h || (h = []), h.push(n));
          }
          h && this._cacheTiles(c, h, d, g);
          b.removeFlags(2);
          return a;
        };
        b.prototype._getTileBounds = function(a) {
          for (var b = u.createEmpty(), c = 0;c < a.length;c++) {
            b.union(a[c].bounds);
          }
          return b;
        };
        b.prototype._cacheTiles = function(a, b, c, d, h) {
          "undefined" === typeof h && (h = 4);
          g(0 < h, "Infinite recursion is likely.");
          var e = this._getTileBounds(b);
          a.save();
          a.setTransform(1, 0, 0, 1, 0, 0);
          a.clearRect(0, 0, d.w, d.h);
          a.translate(-e.x, -e.y);
          a.scale(b[0].scale, b[0].scale);
          var n = this._source.getBounds();
          a.translate(-n.x, -n.y);
          2 <= m.traceLevel && m.writer && m.writer.writeLn("Rendering Tiles: " + e);
          this._source.render(a);
          a.restore();
          for (var n = null, l = 0;l < b.length;l++) {
            var p = b[l], r = p.bounds.clone();
            r.x -= e.x;
            r.y -= e.y;
            d.contains(r) || (n || (n = []), n.push(p));
            p.cachedSurfaceRegion = c(p.cachedSurfaceRegion, a, r);
          }
          n && (2 <= n.length ? (b = n.slice(0, n.length / 2 | 0), e = n.slice(b.length), this._cacheTiles(a, b, c, d, h - 1), this._cacheTiles(a, e, c, d, h - 1)) : this._cacheTiles(a, n, c, d, h - 1));
        };
        return b;
      }();
      s.RenderableTileCache = r;
      var w = function() {
        return function(a, b) {
          this.surfaceRegion = a;
          this.scale = b;
        };
      }();
      s.MipMapLevel = w;
      r = function() {
        function b(a, c, d) {
          this._source = a;
          this._levels = [];
          this._surfaceRegionAllocator = c;
          this._size = d;
        }
        b.prototype.render = function(a) {
        };
        b.prototype.getLevel = function(b) {
          b = Math.max(b.getAbsoluteScaleX(), b.getAbsoluteScaleY());
          var c = 0;
          1 !== b && (c = k(Math.round(Math.log(b) / Math.LN2), -5, 3));
          this._source.hasFlags(4) || (c = k(c, -5, 0));
          b = a(c);
          var d = 5 + c, g = this._levels[d];
          if (!g) {
            c = this._source.getBounds().clone();
            c.scale(b, b);
            c.snap();
            var g = this._surfaceRegionAllocator.allocate(c.w, c.h), h = g.region, g = this._levels[d] = new w(g, b), d = g.surfaceRegion.surface.context;
            d.save();
            d.beginPath();
            d.rect(h.x, h.y, h.w, h.h);
            d.clip();
            d.setTransform(b, 0, 0, b, h.x - c.x, h.y - c.y);
            this._source.render(d);
            d.restore();
          }
          return g;
        };
        return b;
      }();
      s.MipMap = r;
    })(m.Geometry || (m.Geometry = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(d, m) {
  function s() {
    this.constructor = d;
  }
  for (var e in m) {
    m.hasOwnProperty(e) && (d[e] = m[e]);
  }
  s.prototype = m.prototype;
  d.prototype = new s;
};
(function(d) {
  (function(m) {
    var s = d.IntegerUtilities.roundToMultipleOfPowerOfTwo, e = d.Debug.assert, t = m.Geometry.Rectangle;
    (function(d) {
      var a = function(a) {
        function b() {
          a.apply(this, arguments);
        }
        __extends(b, a);
        return b;
      }(m.Geometry.Rectangle);
      d.Region = a;
      var c = function() {
        function a(b, c) {
          this._root = new g(0, 0, b | 0, c | 0, !1);
        }
        a.prototype.allocate = function(a, c) {
          a = Math.ceil(a);
          c = Math.ceil(c);
          e(0 < a && 0 < c);
          var d = this._root.insert(a, c);
          d && (d.allocator = this, d.allocated = !0);
          return d;
        };
        a.prototype.free = function(a) {
          e(a.allocator === this);
          a.clear();
          a.allocated = !1;
        };
        a.RANDOM_ORIENTATION = !0;
        a.MAX_DEPTH = 256;
        return a;
      }();
      d.CompactAllocator = c;
      var g = function(a) {
        function b(b, c, d, f, g) {
          a.call(this, b, c, d, f);
          this._children = null;
          this._horizontal = g;
          this.allocated = !1;
        }
        __extends(b, a);
        b.prototype.clear = function() {
          this._children = null;
          this.allocated = !1;
        };
        b.prototype.insert = function(a, b) {
          return this._insert(a, b, 0);
        };
        b.prototype._insert = function(a, d, g) {
          if (!(g > c.MAX_DEPTH || this.allocated || this.w < a || this.h < d)) {
            if (this._children) {
              var f;
              if ((f = this._children[0]._insert(a, d, g + 1)) || (f = this._children[1]._insert(a, d, g + 1))) {
                return f;
              }
            } else {
              return f = !this._horizontal, c.RANDOM_ORIENTATION && (f = .5 <= Math.random()), this._children = this._horizontal ? [new b(this.x, this.y, this.w, d, !1), new b(this.x, this.y + d, this.w, this.h - d, f)] : [new b(this.x, this.y, a, this.h, !0), new b(this.x + a, this.y, this.w - a, this.h, f)], f = this._children[0], f.w === a && f.h === d ? (f.allocated = !0, f) : this._insert(a, d, g + 1);
            }
          }
        };
        return b;
      }(d.Region), p = function() {
        function a(b, c, d, g) {
          this._columns = b / d | 0;
          this._rows = c / g | 0;
          this._sizeW = d;
          this._sizeH = g;
          this._freeList = [];
          this._index = 0;
          this._total = this._columns * this._rows;
        }
        a.prototype.allocate = function(a, c) {
          a = Math.ceil(a);
          c = Math.ceil(c);
          e(0 < a && 0 < c);
          var d = this._sizeW, g = this._sizeH;
          if (a > d || c > g) {
            return null;
          }
          var f = this._freeList, l = this._index;
          return 0 < f.length ? (d = f.pop(), e(!1 === d.allocated), d.allocated = !0, d) : l < this._total ? (f = l / this._columns | 0, d = new v((l - f * this._columns) * d, f * g, a, c), d.index = l, d.allocator = this, d.allocated = !0, this._index++, d) : null;
        };
        a.prototype.free = function(a) {
          e(a.allocator === this);
          a.allocated = !1;
          this._freeList.push(a);
        };
        return a;
      }();
      d.GridAllocator = p;
      var v = function(a) {
        function b(b, c, d, f) {
          a.call(this, b, c, d, f);
          this.index = -1;
        }
        __extends(b, a);
        return b;
      }(d.Region);
      d.GridCell = v;
      var u = function() {
        return function(a, b, c) {
          this.size = a;
          this.region = b;
          this.allocator = c;
        };
      }(), l = function(a) {
        function b(b, c, d, f, g) {
          a.call(this, b, c, d, f);
          this.region = g;
        }
        __extends(b, a);
        return b;
      }(d.Region);
      d.BucketCell = l;
      a = function() {
        function a(b, c) {
          e(0 < b && 0 < c);
          this._buckets = [];
          this._w = b | 0;
          this._h = c | 0;
          this._filled = 0;
        }
        a.prototype.allocate = function(a, c) {
          a = Math.ceil(a);
          c = Math.ceil(c);
          e(0 < a && 0 < c);
          var d = Math.max(a, c);
          if (a > this._w || c > this._h) {
            return null;
          }
          var g = null, f, q = this._buckets;
          do {
            for (var r = 0;r < q.length && !(q[r].size >= d && (f = q[r], g = f.allocator.allocate(a, c)));r++) {
            }
            if (!g) {
              var k = this._h - this._filled;
              if (k < c) {
                return null;
              }
              var r = s(d, 2), m = 2 * r;
              m > k && (m = k);
              if (m < r) {
                return null;
              }
              k = new t(0, this._filled, this._w, m);
              this._buckets.push(new u(r, k, new p(k.w, k.h, r, r)));
              this._filled += m;
            }
          } while (!g);
          return new l(f.region.x + g.x, f.region.y + g.y, g.w, g.h, g);
        };
        a.prototype.free = function(a) {
          a.region.allocator.free(a.region);
        };
        return a;
      }();
      d.BucketAllocator = a;
    })(m.RegionAllocator || (m.RegionAllocator = {}));
    (function(d) {
      var a = function() {
        function a(c) {
          this._createSurface = c;
          this._surfaces = [];
        }
        Object.defineProperty(a.prototype, "surfaces", {get:function() {
          return this._surfaces;
        }, enumerable:!0, configurable:!0});
        a.prototype._createNewSurface = function(a, c) {
          var d = this._createSurface(a, c);
          this._surfaces.push(d);
          return d;
        };
        a.prototype.addSurface = function(a) {
          this._surfaces.push(a);
        };
        a.prototype.allocate = function(a, c) {
          for (var d = 0;d < this._surfaces.length;d++) {
            var e = this._surfaces[d].allocate(a, c);
            if (e) {
              return e;
            }
          }
          return this._createNewSurface(a, c).allocate(a, c);
        };
        a.prototype.free = function(a) {
        };
        return a;
      }();
      d.SimpleAllocator = a;
    })(m.SurfaceRegionAllocator || (m.SurfaceRegionAllocator = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = m.Geometry.Point, e = m.Geometry.Matrix, t = d.Debug.assert, k = d.Debug.unexpected;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Upward = 1] = "Upward";
      a[a.Downward = 2] = "Downward";
    })(m.Direction || (m.Direction = {}));
    (function(a) {
      a[a.Never = 0] = "Never";
      a[a.Always = 1] = "Always";
      a[a.Auto = 2] = "Auto";
    })(m.PixelSnapping || (m.PixelSnapping = {}));
    (function(a) {
      a[a.Never = 0] = "Never";
      a[a.Always = 1] = "Always";
    })(m.Smoothing || (m.Smoothing = {}));
    (function(a) {
      a[a.Empty = 0] = "Empty";
      a[a.Dirty = 1] = "Dirty";
      a[a.IsMask = 2] = "IsMask";
      a[a.IgnoreMask = 8] = "IgnoreMask";
      a[a.IgnoreQuery = 16] = "IgnoreQuery";
      a[a.InvalidBounds = 32] = "InvalidBounds";
      a[a.InvalidConcatenatedMatrix = 64] = "InvalidConcatenatedMatrix";
      a[a.InvalidInvertedConcatenatedMatrix = 128] = "InvalidInvertedConcatenatedMatrix";
      a[a.InvalidConcatenatedColorMatrix = 256] = "InvalidConcatenatedColorMatrix";
      a[a.InvalidPaint = 512] = "InvalidPaint";
      a[a.EnterClip = 4096] = "EnterClip";
      a[a.LeaveClip = 8192] = "LeaveClip";
      a[a.Visible = 16384] = "Visible";
      a[a.Transparent = 32768] = "Transparent";
    })(m.FrameFlags || (m.FrameFlags = {}));
    (function(a) {
      a[a.None = 0] = "None";
      a[a.AllowMatrixWrite = 1] = "AllowMatrixWrite";
      a[a.AllowColorMatrixWrite = 2] = "AllowColorMatrixWrite";
      a[a.AllowBlendModeWrite = 4] = "AllowBlendModeWrite";
      a[a.AllowFiltersWrite = 8] = "AllowFiltersWrite";
      a[a.AllowMaskWrite = 16] = "AllowMaskWrite";
      a[a.AllowChildrenWrite = 32] = "AllowChildrenWrite";
      a[a.AllowClipWrite = 64] = "AllowClipWrite";
      a[a.AllowAllWrite = a.AllowMatrixWrite | a.AllowColorMatrixWrite | a.AllowBlendModeWrite | a.AllowFiltersWrite | a.AllowMaskWrite | a.AllowChildrenWrite | a.AllowClipWrite] = "AllowAllWrite";
    })(m.FrameCapabilityFlags || (m.FrameCapabilityFlags = {}));
    var a = m.FrameCapabilityFlags, c = function() {
      function c() {
        this._id = c._nextID++;
        this._flags = 17376;
        this._capability = a.AllowAllWrite;
        this._parent = null;
        this._clip = -1;
        this._blendMode = 1;
        this._filters = [];
        this._mask = null;
        this._matrix = e.createIdentity();
        this._concatenatedMatrix = e.createIdentity();
        this._invertedConcatenatedMatrix = null;
        this._colorMatrix = m.ColorMatrix.createIdentity();
        this._concatenatedColorMatrix = m.ColorMatrix.createIdentity();
        this._pixelSnapping = this._smoothing = 0;
      }
      c._getAncestors = function(a, d) {
        "undefined" === typeof d && (d = null);
        var e = c._path;
        for (e.length = 0;a && a !== d;) {
          t(a !== a._parent), e.push(a), a = a._parent;
        }
        t(a === d, "Last ancestor is not an ancestor.");
        return e;
      };
      Object.defineProperty(c.prototype, "parent", {get:function() {
        return this._parent;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "id", {get:function() {
        return this._id;
      }, enumerable:!0, configurable:!0});
      c.prototype._setFlags = function(a) {
        this._flags |= a;
      };
      c.prototype._removeFlags = function(a) {
        this._flags &= ~a;
      };
      c.prototype._hasFlags = function(a) {
        return(this._flags & a) === a;
      };
      c.prototype._toggleFlags = function(a, c) {
        this._flags = c ? this._flags | a : this._flags & ~a;
      };
      c.prototype._hasAnyFlags = function(a) {
        return!!(this._flags & a);
      };
      c.prototype._findClosestAncestor = function(a) {
        for (var c = this;c;) {
          if (!1 === c._hasFlags(a)) {
            return c;
          }
          t(c !== c._parent);
          c = c._parent;
        }
        return null;
      };
      c.prototype._isAncestor = function(a) {
        for (;a;) {
          if (a === this) {
            return!0;
          }
          t(a !== a._parent);
          a = a._parent;
        }
        return!1;
      };
      c.prototype.setCapability = function(a, c, d) {
        "undefined" === typeof c && (c = !0);
        "undefined" === typeof d && (d = 0);
        this._capability = c ? this._capability | a : this._capability & ~a;
        if (1 === d && this._parent) {
          this._parent.setCapability(a, c, d);
        } else {
          if (2 === d && this instanceof m.FrameContainer) {
            for (var g = this._children, e = 0;e < g.length;e++) {
              g[e].setCapability(a, c, d);
            }
          }
        }
      };
      c.prototype.removeCapability = function(a) {
        this.setCapability(a, !1);
      };
      c.prototype.hasCapability = function() {
        return this._capability & 1;
      };
      c.prototype.checkCapability = function(c) {
        this._capability & c || k("Frame doesn't have capability: " + a[c]);
      };
      c.prototype._propagateFlagsUp = function(a) {
        if (!this._hasFlags(a)) {
          this._setFlags(a);
          var c = this._parent;
          c && c._propagateFlagsUp(a);
        }
      };
      c.prototype._propagateFlagsDown = function(a) {
        this._setFlags(a);
      };
      c.prototype._invalidatePosition = function() {
        this._propagateFlagsDown(192);
        this._parent && this._parent._invalidateBounds();
        this._invalidateParentPaint();
      };
      c.prototype.invalidatePaint = function() {
        this._propagateFlagsUp(512);
      };
      c.prototype._invalidateParentPaint = function() {
        this._parent && this._parent._propagateFlagsUp(512);
      };
      c.prototype._invalidateBounds = function() {
        this._propagateFlagsUp(32);
      };
      Object.defineProperty(c.prototype, "properties", {get:function() {
        return this._properties || (this._properties = Object.create(null));
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "x", {get:function() {
        return this._matrix.tx;
      }, set:function(a) {
        this.checkCapability(1);
        this._matrix.tx = a;
        this._invalidatePosition();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "y", {get:function() {
        return this._matrix.ty;
      }, set:function(a) {
        this.checkCapability(1);
        this._matrix.ty = a;
        this._invalidatePosition();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "matrix", {get:function() {
        return this._matrix;
      }, set:function(a) {
        this.checkCapability(1);
        this._matrix.set(a);
        this._invalidatePosition();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "blendMode", {get:function() {
        return this._blendMode;
      }, set:function(a) {
        a |= 0;
        this.checkCapability(4);
        this._blendMode = a;
        this._invalidateParentPaint();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "filters", {get:function() {
        return this._filters;
      }, set:function(a) {
        this.checkCapability(8);
        this._filters = a;
        this._invalidateParentPaint();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "colorMatrix", {get:function() {
        return this._colorMatrix;
      }, set:function(a) {
        this.checkCapability(2);
        this._colorMatrix.set(a);
        this._propagateFlagsDown(256);
        this._invalidateParentPaint();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "mask", {get:function() {
        return this._mask;
      }, set:function(a) {
        this.checkCapability(16);
        this._mask && this._mask !== a && this._mask._removeFlags(2);
        if (this._mask = a) {
          this._mask._setFlags(2), this._mask.invalidate();
        }
        this.invalidate();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "clip", {get:function() {
        return this._clip;
      }, set:function(a) {
        this.checkCapability(64);
        this._clip = a;
      }, enumerable:!0, configurable:!0});
      c.prototype.getBounds = function() {
        t(!1, "Override this.");
        return null;
      };
      c.prototype.gatherPreviousDirtyRegions = function() {
        var a = this.stage;
        a.trackDirtyRegions && this.visit(function(c) {
          if (c instanceof m.FrameContainer) {
            return 0;
          }
          c._previouslyRenderedAABB && a.dirtyRegion.addDirtyRectangle(c._previouslyRenderedAABB);
          return 0;
        });
      };
      c.prototype.getConcatenatedColorMatrix = function() {
        if (!this._parent) {
          return this._colorMatrix;
        }
        if (this._hasFlags(256)) {
          for (var a = this._findClosestAncestor(256), d = c._getAncestors(this, a), e = a ? a._concatenatedColorMatrix.clone() : m.ColorMatrix.createIdentity(), l = d.length - 1;0 <= l;l--) {
            a = d[l], t(a._hasFlags(256)), e.multiply(a._colorMatrix), a._concatenatedColorMatrix.set(e), a._removeFlags(256);
          }
        }
        return this._concatenatedColorMatrix;
      };
      c.prototype.getConcatenatedAlpha = function(a) {
        "undefined" === typeof a && (a = null);
        for (var c = this, d = 1;c && c !== a;) {
          d *= c._colorMatrix.alphaMultiplier, c = c._parent;
        }
        return d;
      };
      Object.defineProperty(c.prototype, "stage", {get:function() {
        for (var a = this;a._parent;) {
          a = a._parent;
        }
        return a instanceof m.Stage ? a : null;
      }, enumerable:!0, configurable:!0});
      c.prototype.getConcatenatedMatrix = function() {
        if (this._hasFlags(64)) {
          for (var a = this._findClosestAncestor(64), d = c._getAncestors(this, a), k = a ? a._concatenatedMatrix.clone() : e.createIdentity(), l = d.length - 1;0 <= l;l--) {
            a = d[l], t(a._hasFlags(64)), k.preMultiply(a._matrix), a._concatenatedMatrix.set(k), a._removeFlags(64);
          }
        }
        return this._concatenatedMatrix;
      };
      c.prototype._getInvertedConcatenatedMatrix = function() {
        this._hasFlags(128) && (this._invertedConcatenatedMatrix || (this._invertedConcatenatedMatrix = e.createIdentity()), this._invertedConcatenatedMatrix.set(this.getConcatenatedMatrix()), this._invertedConcatenatedMatrix.inverse(this._invertedConcatenatedMatrix), this._removeFlags(128));
        return this._invertedConcatenatedMatrix;
      };
      c.prototype.invalidate = function() {
        this._setFlags(1);
      };
      c.prototype.visit = function(a, c, d, g) {
        "undefined" === typeof d && (d = 0);
        "undefined" === typeof g && (g = 0);
        var e, b, h = g & 8;
        e = [this];
        var k, n = !!c;
        n && (k = [c.clone()]);
        for (var f = [d];0 < e.length;) {
          b = e.pop();
          n && (c = k.pop());
          d = f.pop() | b._flags;
          var q = a(b, c, d);
          if (0 === q) {
            if (b instanceof m.FrameContainer) {
              var s = b._children.length;
              if (g & 16 && !m.disableClipping.value) {
                for (var q = b.gatherLeaveClipEvents(), L = s - 1;0 <= L;L--) {
                  if (q && q[L]) {
                    for (;q[L].length;) {
                      if (s = q[L].shift(), e.push(s), f.push(8192), n) {
                        var I = c.clone();
                        I.preMultiply(s.matrix);
                        k.push(I);
                      }
                    }
                  }
                  var B = b._children[L];
                  t(B);
                  e.push(B);
                  n && (I = c.clone(), I.preMultiply(B.matrix), k.push(I));
                  0 <= B.clip ? f.push(d | 4096) : f.push(d);
                }
              } else {
                for (L = 0;L < s;L++) {
                  if (B = b._children[h ? L : s - 1 - L]) {
                    e.push(B), n && (I = c.clone(), I.preMultiply(B.matrix), k.push(I)), f.push(d);
                  }
                }
              }
            }
          } else {
            if (1 === q) {
              break;
            }
          }
        }
      };
      c.prototype.getDepth = function() {
        for (var a = 0, c = this;c._parent;) {
          a++, c = c._parent;
        }
        return a;
      };
      Object.defineProperty(c.prototype, "smoothing", {get:function() {
        return this._smoothing;
      }, set:function(a) {
        this._smoothing = a;
        this.invalidate();
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(c.prototype, "pixelSnapping", {get:function() {
        return this._pixelSnapping;
      }, set:function(a) {
        this._pixelSnapping = a;
        this.invalidate();
      }, enumerable:!0, configurable:!0});
      c.prototype.queryFramesByPoint = function(a) {
        var c = !0, d = !0;
        "undefined" === typeof c && (c = !1);
        "undefined" === typeof d && (d = !1);
        var g = e.createIdentity(), r = s.createEmpty(), b = [];
        this.visit(function(e, k, n) {
          if (n & 16) {
            return 2;
          }
          k.inverse(g);
          r.set(a);
          g.transformPoint(r);
          if (e.getBounds().containsPoint(r)) {
            if (e instanceof m.FrameContainer) {
              if (d && (b.push(e), !c)) {
                return 1;
              }
            } else {
              if (b.push(e), !c) {
                return 1;
              }
            }
            return 0;
          }
          return 2;
        }, e.createIdentity(), 0);
        b.reverse();
        return b;
      };
      c._path = [];
      c._nextID = 0;
      return c;
    }();
    m.Frame = c;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = m.Geometry.Rectangle, e = d.Debug.assert, t = function(d) {
      function a() {
        d.call(this);
        this._children = [];
      }
      __extends(a, d);
      a.prototype.addChild = function(a) {
        this.checkCapability(32);
        a && (e(a !== this), a._parent = this, a._invalidatePosition());
        this._children.push(a);
      };
      a.prototype.addChildAt = function(a, d) {
        this.checkCapability(32);
        e(0 <= d && d <= this._children.length);
        d === this._children.length ? this._children.push(a) : this._children.splice(d, 0, a);
        a && (e(a !== this), a._parent = this, a._invalidatePosition());
        return a;
      };
      a.prototype.removeChild = function(a) {
        this.checkCapability(32);
        a._parent === this && (a = this._children.indexOf(a), this.removeChildAt(a));
      };
      a.prototype.removeChildAt = function(a) {
        this.checkCapability(32);
        e(0 <= a && a < this._children.length);
        if (a = this._children.splice(a, 1)[0]) {
          a._parent = null, a._invalidatePosition();
        }
      };
      a.prototype.clearChildren = function() {
        this.checkCapability(32);
        for (var a = 0;a < this._children.length;a++) {
          var d = this._children[a];
          d && d._invalidatePosition();
        }
        this._children.length = 0;
      };
      a.prototype._propagateFlagsDown = function(a) {
        if (!this._hasFlags(a)) {
          this._setFlags(a);
          for (var d = this._children, e = 0;e < d.length;e++) {
            d[e]._propagateFlagsDown(a);
          }
        }
      };
      a.prototype.getBounds = function() {
        if (!this._hasFlags(32)) {
          return this._bounds;
        }
        for (var a = s.createEmpty(), d = 0;d < this._children.length;d++) {
          var e = this._children[d], k = e.getBounds().clone();
          e.matrix.transformRectangleAABB(k);
          a.union(k);
        }
        this._bounds = a;
        this._removeFlags(32);
        return a;
      };
      a.prototype.gatherLeaveClipEvents = function() {
        for (var a = this._children.length, d = null, e = 0;e < a;e++) {
          var k = this._children[e];
          if (0 <= k.clip) {
            var m = e + k.clip, d = d || [];
            d[m] || (d[m] = []);
            d[m].push(k);
          }
        }
        return d;
      };
      return a;
    }(m.Frame);
    m.FrameContainer = t;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = m.Geometry.Rectangle, e = m.Geometry.DirtyRegion, t = d.Debug.assert;
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
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Continue = 0] = "Continue";
      a[a.Stop = 1] = "Stop";
      a[a.Skip = 2] = "Skip";
      a[a.FrontToBack = 8] = "FrontToBack";
      a[a.Clips = 16] = "Clips";
    })(m.VisitorFlags || (m.VisitorFlags = {}));
    m.StageRendererOptions = function() {
      return function() {
        this.debug = !1;
        this.paintRenderable = !0;
        this.paintViewport = this.paintFlashing = this.paintBounds = !1;
      };
    }();
    (function(a) {
      a[a.Canvas2D = 0] = "Canvas2D";
      a[a.WebGL = 1] = "WebGL";
      a[a.Both = 2] = "Both";
      a[a.DOM = 3] = "DOM";
      a[a.SVG = 4] = "SVG";
    })(m.Backend || (m.Backend = {}));
    var k = function() {
      function a(a, d, e) {
        this._canvas = a;
        this._stage = d;
        this._options = e;
        this._viewport = s.createSquare();
      }
      Object.defineProperty(a.prototype, "viewport", {set:function(a) {
        this._viewport.set(a);
      }, enumerable:!0, configurable:!0});
      a.prototype.render = function() {
      };
      a.prototype.resize = function() {
      };
      return a;
    }();
    m.StageRenderer = k;
    k = function(a) {
      function c(c, d, k) {
        "undefined" === typeof k && (k = !1);
        a.call(this);
        this.w = c;
        this.h = d;
        this.dirtyRegion = new e(c, d);
        this.trackDirtyRegions = k;
        this._setFlags(1);
      }
      __extends(c, a);
      c.prototype.readyToRender = function() {
        var a;
        "undefined" === typeof a && (a = !0);
        if (!this._hasFlags(512)) {
          return!1;
        }
        a && this.visit(function(a) {
          return a._hasFlags(512) ? (a._toggleFlags(512, !1), 0) : 2;
        });
        return!0;
      };
      c.prototype.gatherMarkedDirtyRegions = function(a) {
        var c = this;
        this.visit(function(a, d, e) {
          a._removeFlags(1);
          if (a instanceof m.FrameContainer) {
            return 0;
          }
          e & 1 && (e = a.getBounds().clone(), d.transformRectangleAABB(e), c.dirtyRegion.addDirtyRectangle(e), a._previouslyRenderedAABB && c.dirtyRegion.addDirtyRectangle(a._previouslyRenderedAABB));
          return 0;
        }, a, 0);
      };
      c.prototype.gatherFrames = function() {
        var a = [];
        this.visit(function(c, d) {
          c instanceof m.FrameContainer || a.push(c);
          return 0;
        }, this.matrix);
        return a;
      };
      c.prototype.gatherLayers = function() {
        var a = [], c;
        this.visit(function(d, e) {
          if (d instanceof m.FrameContainer) {
            return 0;
          }
          var l = d.getBounds().clone();
          e.transformRectangleAABB(l);
          d._hasFlags(1) ? (c && a.push(c), a.push(l.clone()), c = null) : c ? c.union(l) : c = l.clone();
          return 0;
        }, this.matrix);
        c && a.push(c);
        return a;
      };
      return c;
    }(m.FrameContainer);
    m.Stage = k;
    k = function(a) {
      function c(c, e) {
        a.call(this);
        this.color = d.Color.None;
        this._bounds = new s(0, 0, c, e);
      }
      __extends(c, a);
      c.prototype.setBounds = function(a) {
        this._bounds.set(a);
      };
      c.prototype.getBounds = function() {
        return this._bounds;
      };
      return c;
    }(m.FrameContainer);
    m.ClipRectangle = k;
    k = function(a) {
      function c(c) {
        a.call(this);
        t(c);
        this._source = c;
      }
      __extends(c, a);
      Object.defineProperty(c.prototype, "source", {get:function() {
        return this._source;
      }, enumerable:!0, configurable:!0});
      c.prototype.getBounds = function() {
        return this.source.getBounds();
      };
      return c;
    }(m.Frame);
    m.Shape = k;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = m.Geometry.Rectangle, e = m.Geometry.Matrix, t = d.Debug.assertUnreachable, k = d.Debug.assert, a = d.Debug.unexpected, c = d.ArrayUtilities.indexOf;
    (function(a) {
      a[a.None = 0] = "None";
      a[a.Dynamic = 1] = "Dynamic";
      a[a.Dirty = 2] = "Dirty";
      a[a.Scalable = 4] = "Scalable";
      a[a.Tileable = 8] = "Tileable";
      a[a.Loading = 16] = "Loading";
    })(m.RenderableFlags || (m.RenderableFlags = {}));
    var g = function() {
      function a(b) {
        this._flags = 0;
        this.properties = {};
        this._frameReferrers = [];
        this._renderableReferrers = [];
        this._bounds = b.clone();
      }
      a.prototype.setFlags = function(a) {
        this._flags |= a;
      };
      a.prototype.hasFlags = function(a) {
        return(this._flags & a) === a;
      };
      a.prototype.removeFlags = function(a) {
        this._flags &= ~a;
      };
      a.prototype.addFrameReferrer = function(a) {
        c(this._frameReferrers, a);
        this._frameReferrers.push(a);
      };
      a.prototype.addRenderableReferrer = function(a) {
        c(this._renderableReferrers, a);
        this._renderableReferrers.push(a);
      };
      a.prototype.invalidatePaint = function() {
        this.setFlags(2);
        for (var a = this._frameReferrers, b = 0;b < a.length;b++) {
          a[b].invalidatePaint();
        }
        a = this._renderableReferrers;
        for (b = 0;b < a.length;b++) {
          a[b].invalidatePaint();
        }
      };
      a.prototype.getBounds = function() {
        return this._bounds;
      };
      a.prototype.render = function(a, b, c) {
      };
      return a;
    }();
    m.Renderable = g;
    var p = function(a) {
      function b(c, d) {
        a.call(this, c);
        this.render = d;
      }
      __extends(b, a);
      return b;
    }(g);
    m.CustomRenderable = p;
    p = function(a) {
      function b(c, d) {
        a.call(this, d);
        this._flags = 3;
        this._lastCurrentTime = 0;
        this._video = document.createElement("video");
        this._video.src = c;
        this._video.loop = !0;
        this._video.play();
        b._renderableVideos.push(this);
      }
      __extends(b, a);
      b.prototype.invalidatePaintCheck = function() {
        this._lastCurrentTime !== this._video.currentTime && this.invalidatePaint();
        this._lastCurrentTime = this._video.currentTime;
      };
      b.invalidateVideos = function() {
        for (var a = b._renderableVideos, c = 0;c < a.length;c++) {
          a[c].invalidatePaintCheck();
        }
      };
      b.prototype.render = function(a, b) {
        this._video && a.drawImage(this._video, 0, 0);
      };
      b._renderableVideos = [];
      return b;
    }(g);
    m.RenderableVideo = p;
    p = function(b) {
      function c(a, d) {
        b.call(this, d);
        this._flags = 3;
        this.properties = {};
        this._canvas = a;
        this._context = this._canvas.getContext("2d");
        this._imageData = this._context.createImageData(this._bounds.w, this._bounds.h);
      }
      __extends(c, b);
      c.FromDataBuffer = function(a, b, d) {
        var e = document.createElement("canvas");
        e.width = d.w;
        e.height = d.h;
        d = new c(e, d);
        d.updateFromDataBuffer(a, b);
        return d;
      };
      c.FromFrame = function(a, b, d, e, g) {
        var h = document.createElement("canvas"), l = a.getBounds();
        h.width = l.w;
        h.height = l.h;
        h = new c(h, l);
        h.drawFrame(a, b, d, e, g);
        return h;
      };
      c.prototype.updateFromDataBuffer = function(b, c) {
        if (m.imageUpdateOption.value) {
          if (4 === b || 5 === b || 6 === b) {
            var e = this;
            e.setFlags(16);
            var g = new Image;
            g.src = URL.createObjectURL(c.toBlob());
            g.onload = function() {
              e._context.drawImage(g, 0, 0);
              e.removeFlags(16);
              e.invalidatePaint();
            };
            g.onerror = function() {
              a("Image loading error: " + d.ImageType[b]);
            };
          } else {
            m.imageConvertOption.value && d.ColorUtilities.convertImage(b, 3, new Int32Array(c.buffer), new Int32Array(this._imageData.data.buffer)), this._context.putImageData(this._imageData, 0, 0);
          }
          this.invalidatePaint();
        }
      };
      c.prototype.readImageData = function(a) {
        var b = this._context.getImageData(0, 0, this._canvas.width, this._canvas.height).data;
        a.writeRawBytes(b);
      };
      c.prototype.render = function(a, b) {
        this._canvas ? a.drawImage(this._canvas, 0, 0) : this._renderFallback(a);
      };
      c.prototype.drawFrame = function(a, b, c, d, e) {
        c = m.Canvas2D;
        d = this.getBounds();
        var g = new c.Canvas2DStageRendererOptions;
        g.cacheShapes = !0;
        (new c.Canvas2DStageRenderer(this._canvas, null, g)).renderFrame(a, e || d, b);
      };
      c.prototype._renderFallback = function(a) {
        this.fillStyle || (this.fillStyle = d.ColorStyle.randomStyle());
        var b = this._bounds;
        a.save();
        a.beginPath();
        a.lineWidth = 2;
        a.fillStyle = this.fillStyle;
        a.fillRect(b.x, b.y, b.w, b.h);
        a.restore();
      };
      return c;
    }(g);
    m.RenderableBitmap = p;
    var v;
    (function(a) {
      a[a.Fill = 0] = "Fill";
      a[a.Stroke = 1] = "Stroke";
      a[a.StrokeFill = 2] = "StrokeFill";
    })(v || (v = {}));
    var u = function() {
      return function(a, b, c, d) {
        this.type = a;
        this.style = b;
        this.smoothImage = c;
        this.strokeProperties = d;
        this.path = new Path2D;
        k(1 === a === !!d);
      };
    }(), l = function() {
      return function(a, b, c, d, e) {
        this.thickness = a;
        this.scaleMode = b;
        this.capsStyle = c;
        this.jointsStyle = d;
        this.miterLimit = e;
      };
    }();
    v = function(a) {
      function b(c, d, e, g) {
        a.call(this, g);
        this._flags = 14;
        this.properties = {};
        this._id = c;
        this._pathData = d;
        this._textures = e;
        e.length && this.setFlags(1);
      }
      __extends(b, a);
      b.prototype.update = function(a, b, c) {
        this._bounds = c;
        this._pathData = a;
        this._paths = null;
        this._textures = b;
      };
      b.prototype.getBounds = function() {
        return this._bounds;
      };
      b.prototype.render = function(a, b, c) {
        "undefined" === typeof c && (c = !1);
        a.fillStyle = a.strokeStyle = "transparent";
        var d = this._textures;
        for (b = 0;b < d.length;b++) {
          if (d[b].hasFlags(16)) {
            return;
          }
        }
        (b = this._pathData) && this._deserializePaths(b, a);
        d = this._paths;
        k(d);
        for (b = 0;b < d.length;b++) {
          var e = d[b];
          a.mozImageSmoothingEnabled = a.msImageSmoothingEnabled = a.imageSmoothingEnabled = e.smoothImage;
          if (0 === e.type) {
            a.fillStyle = e.style, c ? a.clip(e.path, "evenodd") : a.fill(e.path, "evenodd"), a.fillStyle = "transparent";
          } else {
            if (!c) {
              a.strokeStyle = e.style;
              var g = 1;
              e.strokeProperties && (g = e.strokeProperties.scaleMode, a.lineWidth = e.strokeProperties.thickness, a.lineCap = e.strokeProperties.capsStyle, a.lineJoin = e.strokeProperties.jointsStyle, a.miterLimit = e.strokeProperties.miterLimit);
              var h = a.lineWidth;
              (h = 1 === h || 3 === h) && a.translate(.5, .5);
              a.flashStroke(e.path, g);
              h && a.translate(-.5, -.5);
              a.strokeStyle = "transparent";
            }
          }
        }
      };
      b.prototype._deserializePaths = function(a, c) {
        k(!this._paths);
        this._paths = [];
        for (var e = null, g = null, h = 0, r = 0, p, m, u = !1, v = 0, s = 0, ga = a.commands, E = a.coordinates, x = a.styles, z = x.position = 0, G = a.commandsPosition, F = 0;F < G;F++) {
          switch(p = ga[F], p) {
            case 9:
              k(z <= a.coordinatesPosition - 2);
              u && e && (e.lineTo(v, s), g && g.lineTo(v, s));
              u = !0;
              h = v = E[z++] / 20;
              r = s = E[z++] / 20;
              e && e.moveTo(h, r);
              g && g.moveTo(h, r);
              break;
            case 10:
              k(z <= a.coordinatesPosition - 2);
              h = E[z++] / 20;
              r = E[z++] / 20;
              e && e.lineTo(h, r);
              g && g.lineTo(h, r);
              break;
            case 11:
              k(z <= a.coordinatesPosition - 4);
              p = E[z++] / 20;
              m = E[z++] / 20;
              h = E[z++] / 20;
              r = E[z++] / 20;
              e && e.quadraticCurveTo(p, m, h, r);
              g && g.quadraticCurveTo(p, m, h, r);
              break;
            case 12:
              k(z <= a.coordinatesPosition - 6);
              p = E[z++] / 20;
              m = E[z++] / 20;
              var y = E[z++] / 20, A = E[z++] / 20, h = E[z++] / 20, r = E[z++] / 20;
              e && e.bezierCurveTo(p, m, y, A, h, r);
              g && g.bezierCurveTo(p, m, y, A, h, r);
              break;
            case 1:
              k(4 <= x.bytesAvailable);
              e = this._createPath(0, d.ColorUtilities.rgbaToCSSStyle(x.readUnsignedInt()), !1, null, h, r);
              break;
            case 3:
              p = this._readBitmap(x, c);
              e = this._createPath(0, p.style, p.smoothImage, null, h, r);
              break;
            case 2:
              e = this._createPath(0, this._readGradient(x, c), !1, null, h, r);
              break;
            case 4:
              e = null;
              break;
            case 5:
              g = d.ColorUtilities.rgbaToCSSStyle(x.readUnsignedInt());
              x.position += 1;
              p = x.readByte();
              m = b.LINE_CAPS_STYLES[x.readByte()];
              y = b.LINE_JOINTS_STYLES[x.readByte()];
              p = new l(E[z++] / 20, p, m, y, x.readByte());
              g = this._createPath(1, g, !1, p, h, r);
              break;
            case 6:
              g = this._createPath(2, this._readGradient(x, c), !1, null, h, r);
              break;
            case 7:
              p = this._readBitmap(x, c);
              g = this._createPath(2, p.style, p.smoothImage, null, h, r);
              break;
            case 8:
              g = null;
              break;
            default:
              t("Invalid command " + p + " encountered at index" + F + " of " + G);
          }
        }
        k(0 === x.bytesAvailable);
        k(F === G);
        k(z === a.coordinatesPosition);
        u && e && (e.lineTo(v, s), g && g.lineTo(v, s));
        this._pathData = null;
      };
      b.prototype._createPath = function(a, b, c, d, e, g) {
        a = new u(a, b, c, d);
        this._paths.push(a);
        a.path.moveTo(e, g);
        return a.path;
      };
      b.prototype._readMatrix = function(a) {
        return new e(a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat(), a.readFloat());
      };
      b.prototype._readGradient = function(a, b) {
        k(34 <= a.bytesAvailable);
        var c = a.readUnsignedByte(), e = 2 * a.readShort() / 255;
        k(-1 <= e && 1 >= e);
        var g = this._readMatrix(a), c = 16 === c ? b.createLinearGradient(-1, 0, 1, 0) : b.createRadialGradient(e, 0, 0, 0, 0, 1);
        c.setTransform && c.setTransform(g.toSVGMatrix());
        g = a.readUnsignedByte();
        for (e = 0;e < g;e++) {
          var h = a.readUnsignedByte() / 255, l = d.ColorUtilities.rgbaToCSSStyle(a.readUnsignedInt());
          c.addColorStop(h, l);
        }
        a.position += 2;
        return c;
      };
      b.prototype._readBitmap = function(a, b) {
        k(30 <= a.bytesAvailable);
        var c = a.readUnsignedInt(), d = this._readMatrix(a), e = a.readBoolean() ? "repeat" : "no-repeat", g = a.readBoolean(), c = this._textures[c];
        k(c._canvas);
        e = b.createPattern(c._canvas, e);
        e.setTransform(d.toSVGMatrix());
        return{style:e, smoothImage:g};
      };
      b.prototype._renderFallback = function(a) {
        this.fillStyle || (this.fillStyle = d.ColorStyle.randomStyle());
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
    }(g);
    m.RenderableShape = v;
    var r = function() {
      function a() {
        this.align = this.leading = this.descent = this.ascent = this.width = this.y = this.x = 0;
        this.runs = [];
      }
      a.prototype.addRun = function(c, d, f, e) {
        if (f) {
          a._measureContext.font = c;
          var g = a._measureContext.measureText(f).width | 0;
          this.runs.push(new b(c, d, f, g, e));
          this.width += g;
        }
      };
      a.prototype.wrap = function(c) {
        var d = [this], f = this.runs, e = this;
        e.width = 0;
        e.runs = [];
        for (var g = a._measureContext, l = 0;l < f.length;l++) {
          var r = f[l], k = r.text;
          r.text = "";
          r.width = 0;
          g.font = r.font;
          for (var p = c, m = k.split(/[\s.-]/), u = 0, v = 0;v < m.length;v++) {
            var s = m[v], t = k.substr(u, s.length + 1), x = g.measureText(t).width | 0;
            if (x > p) {
              do {
                if (e.runs.push(r), e.width += r.width, r = new b(r.font, r.fillStyle, "", 0, r.underline), p = new a, p.y = e.y + e.descent + e.leading + e.ascent | 0, p.ascent = e.ascent, p.descent = e.descent, p.leading = e.leading, p.align = e.align, d.push(p), e = p, p = c - x, 0 > p) {
                  var x = t.length, z, G;
                  do {
                    x--, z = t.substr(0, x), G = g.measureText(z).width | 0;
                  } while (G > c);
                  r.text = z;
                  r.width = G;
                  t = t.substr(x);
                  x = g.measureText(t).width | 0;
                }
              } while (0 > p);
            } else {
              p -= x;
            }
            r.text += t;
            r.width += x;
            u += s.length + 1;
          }
          e.runs.push(r);
          e.width += r.width;
        }
        return d;
      };
      a._measureContext = document.createElement("canvas").getContext("2d");
      return a;
    }();
    m.TextLine = r;
    var b = function() {
      return function(a, b, c, d, e) {
        "undefined" === typeof a && (a = "");
        "undefined" === typeof b && (b = "");
        "undefined" === typeof c && (c = "");
        "undefined" === typeof d && (d = 0);
        "undefined" === typeof e && (e = !1);
        this.font = a;
        this.fillStyle = b;
        this.text = c;
        this.width = d;
        this.underline = e;
      };
    }();
    m.TextRun = b;
    v = function(a) {
      function b(c) {
        a.call(this, c);
        this._flags = 3;
        this.properties = {};
        this._textBounds = c.clone();
        this._textRunData = null;
        this._plainText = "";
        this._borderColor = this._backgroundColor = 0;
        this._matrix = e.createIdentity();
        this._coords = null;
        this.textRect = c.clone();
        this.lines = [];
      }
      __extends(b, a);
      b.prototype.setBounds = function(a) {
        this._bounds.set(a);
        this._textBounds.set(a);
        this.textRect.setElements(a.x + 2, a.y + 2, a.x - 2, a.x - 2);
      };
      b.prototype.setContent = function(a, b, c, d) {
        this._textRunData = b;
        this._plainText = a;
        this._matrix.set(c);
        this._coords = d;
        this.lines = [];
      };
      b.prototype.setStyle = function(a, b) {
        this._backgroundColor = a;
        this._borderColor = b;
      };
      b.prototype.reflow = function(a, b) {
        var c = this._textRunData;
        if (c) {
          for (var e = this._bounds, g = e.w - 4, h = this._plainText, l = this.lines, p = new r, k = 0, m = 0, u = 0, v = 0, s = 0, w = -1;c.position < c.length;) {
            var t = c.readInt(), G = c.readInt(), F = c.readInt(), y = c.readInt(), y = y ? "swffont" + y : c.readUTF(), A = c.readInt(), D = c.readInt(), J = c.readInt();
            A > u && (u = A);
            D > v && (v = D);
            J > s && (s = J);
            A = c.readBoolean();
            D = "";
            c.readBoolean() && (D += "italic");
            A && (D += " bold");
            F = D + " " + F + "px " + y;
            y = c.readInt();
            y = d.ColorUtilities.rgbToHex(y);
            A = c.readInt();
            -1 === w && (w = A);
            c.readBoolean();
            c.readInt();
            c.readInt();
            c.readInt();
            c.readInt();
            c.readInt();
            for (var A = c.readBoolean(), H = "", D = !1;!D;t++) {
              D = t >= G - 1;
              J = h[t];
              if ("\r" !== J && "\n" !== J && (H += J, t < h.length - 1)) {
                continue;
              }
              p.addRun(F, y, H, A);
              if (p.runs.length) {
                k += u;
                p.y = k | 0;
                k += v + s;
                p.ascent = u;
                p.descent = v;
                p.leading = s;
                p.align = w;
                if (b && p.width > g) {
                  for (p = p.wrap(g), H = 0;H < p.length;H++) {
                    var K = p[H], k = K.y + K.descent + K.leading;
                    l.push(K);
                    K.width > m && (m = K.width);
                  }
                } else {
                  l.push(p), p.width > m && (m = p.width);
                }
                p = new r;
              } else {
                k += u + v + s;
              }
              H = "";
              if (D) {
                s = v = u = 0;
                w = -1;
                break;
              }
              "\r" === J && "\n" === h[t + 1] && t++;
            }
            p.addRun(F, y, H, A);
          }
          c = this.textRect;
          c.w = m;
          c.h = k;
          if (a) {
            if (!b) {
              g = m;
              m = e.w;
              switch(a) {
                case 1:
                  c.x = m - (g + 4) >> 1;
                  break;
                case 3:
                  c.x = m - (g + 4);
              }
              this._textBounds.setElements(c.x - 2, c.y - 2, c.w + 4, c.h + 4);
            }
            e.h = k + 4;
          } else {
            this._textBounds = e;
          }
          for (t = 0;t < l.length;t++) {
            if (e = l[t], e.width < g) {
              switch(e.align) {
                case 1:
                  e.x = g - e.width | 0;
                  break;
                case 2:
                  e.x = (g - e.width) / 2 | 0;
              }
            }
          }
          this.invalidatePaint();
        }
      };
      b.prototype.getBounds = function() {
        return this._bounds;
      };
      b.prototype.render = function(a) {
        a.save();
        var b = this._textBounds;
        this._backgroundColor && (a.fillStyle = d.ColorUtilities.rgbaToCSSStyle(this._backgroundColor), a.fillRect(b.x, b.y, b.w, b.h));
        this._borderColor && (a.strokeStyle = d.ColorUtilities.rgbaToCSSStyle(this._borderColor), a.lineCap = "square", a.lineWidth = 1, a.strokeRect(b.x, b.y, b.w, b.h));
        this._coords ? this._renderChars(a) : this._renderLines(a);
        a.restore();
      };
      b.prototype._renderChars = function(a) {
        if (this._matrix) {
          var b = this._matrix;
          a.transform(b.a, b.b, b.c, b.d, b.tx, b.ty);
        }
        for (var b = this.lines, c = this._coords, d = c.position = 0;d < b.length;d++) {
          for (var e = b[d].runs, g = 0;g < e.length;g++) {
            var h = e[g];
            a.font = h.font;
            a.fillStyle = h.fillStyle;
            for (var h = h.text, l = 0;l < h.length;l++) {
              var r = c.readInt() / 20, p = c.readInt() / 20;
              a.fillText(h[l], r, p);
            }
          }
        }
      };
      b.prototype._renderLines = function(a) {
        var b = this._textBounds;
        a.beginPath();
        a.rect(b.x, b.y, b.w, b.h);
        a.clip();
        a.translate(b.x + 2, b.y + 2);
        for (var b = this.lines, c = 0;c < b.length;c++) {
          for (var d = b[c], e = d.x, g = d.y, h = d.runs, l = 0;l < h.length;l++) {
            var r = h[l];
            a.font = r.font;
            a.fillStyle = r.fillStyle;
            r.underline && a.fillRect(e, g + d.descent / 2 | 0, r.width, 1);
            a.fillText(r.text, e, g);
            e += r.width;
          }
        }
      };
      return b;
    }(g);
    m.RenderableText = v;
    v = function(a) {
      function b(c, d) {
        a.call(this, new s(0, 0, c, d));
        this._flags = 5;
        this.properties = {};
      }
      __extends(b, a);
      Object.defineProperty(b.prototype, "text", {get:function() {
        return this._text;
      }, set:function(a) {
        this._text = a;
      }, enumerable:!0, configurable:!0});
      b.prototype.render = function(a, b) {
        a.save();
        a.textBaseline = "top";
        a.fillStyle = "white";
        a.fillText(this.text, 0, 0);
        a.restore();
      };
      return b;
    }(g);
    m.Label = v;
    g = function(a) {
      function b() {
        a.call(this, s.createMaxI16());
        this._flags = 14;
        this.properties = {};
      }
      __extends(b, a);
      b.prototype.render = function(a, b) {
        function c(b) {
          for (var d = Math.ceil((e.x + e.w) / b) * b, f = Math.floor(e.x / b) * b;f < d;f += b) {
            a.moveTo(f + .5, e.y), a.lineTo(f + .5, e.y + e.h);
          }
          d = Math.ceil((e.y + e.h) / b) * b;
          for (f = Math.floor(e.y / b) * b;f < d;f += b) {
            a.moveTo(e.x, f + .5), a.lineTo(e.x + e.w, f + .5);
          }
        }
        a.save();
        var e = b || this.getBounds();
        a.fillStyle = d.ColorStyle.VeryDark;
        a.fillRect(e.x, e.y, e.w, e.h);
        a.beginPath();
        c(100);
        a.lineWidth = 1;
        a.strokeStyle = d.ColorStyle.Dark;
        a.stroke();
        a.beginPath();
        c(500);
        a.lineWidth = 1;
        a.strokeStyle = d.ColorStyle.TabToolbar;
        a.stroke();
        a.beginPath();
        c(1E3);
        a.lineWidth = 3;
        a.strokeStyle = d.ColorStyle.Toolbars;
        a.stroke();
        a.lineWidth = 3;
        a.beginPath();
        a.moveTo(-1048576, .5);
        a.lineTo(1048576, .5);
        a.moveTo(.5, -1048576);
        a.lineTo(.5, 1048576);
        a.strokeStyle = d.ColorStyle.Orange;
        a.stroke();
        a.restore();
      };
      return b;
    }(g);
    m.Grid = g;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = d.ColorUtilities.clampByte, e = d.Debug.assert, t = function() {
      return function() {
      };
    }();
    m.Filter = t;
    var k = function(a) {
      function c(c, d, e) {
        a.call(this);
        this.blurX = c;
        this.blurY = d;
        this.quality = e;
      }
      __extends(c, a);
      return c;
    }(t);
    m.BlurFilter = k;
    k = function(a) {
      function c(c, d, e, k, l, r, b, h, m, n, f) {
        a.call(this);
        this.alpha = c;
        this.angle = d;
        this.blurX = e;
        this.blurY = k;
        this.color = l;
        this.distance = r;
        this.hideObject = b;
        this.inner = h;
        this.knockout = m;
        this.quality = n;
        this.strength = f;
      }
      __extends(c, a);
      return c;
    }(t);
    m.DropshadowFilter = k;
    t = function(a) {
      function c(c, d, e, k, l, r, b, h) {
        a.call(this);
        this.alpha = c;
        this.blurX = d;
        this.blurY = e;
        this.color = k;
        this.inner = l;
        this.knockout = r;
        this.quality = b;
        this.strength = h;
      }
      __extends(c, a);
      return c;
    }(t);
    m.GlowFilter = t;
    t = function() {
      function a(a) {
        e(20 === a.length);
        this._m = new Float32Array(a);
      }
      a.prototype.clone = function() {
        return new a(this._m);
      };
      a.prototype.set = function(a) {
        this._m.set(a._m);
      };
      a.prototype.toWebGLMatrix = function() {
        return new Float32Array(this._m);
      };
      a.prototype.asWebGLMatrix = function() {
        return this._m.subarray(0, 16);
      };
      a.prototype.asWebGLVector = function() {
        return this._m.subarray(16, 20);
      };
      a.prototype.getColorMatrix = function() {
        var a = new Float32Array(20), d = this._m;
        a[0] = d[0];
        a[1] = d[4];
        a[2] = d[8];
        a[3] = d[12];
        a[4] = 255 * d[16];
        a[5] = d[1];
        a[6] = d[5];
        a[7] = d[9];
        a[8] = d[13];
        a[9] = 255 * d[17];
        a[10] = d[2];
        a[11] = d[6];
        a[12] = d[10];
        a[13] = d[14];
        a[14] = 255 * d[18];
        a[15] = d[3];
        a[16] = d[7];
        a[17] = d[11];
        a[18] = d[15];
        a[19] = 255 * d[19];
        return a;
      };
      a.prototype.getColorTransform = function() {
        var a = new Float32Array(8), d = this._m;
        a[0] = d[0];
        a[1] = d[5];
        a[2] = d[10];
        a[3] = d[15];
        a[4] = 255 * d[16];
        a[5] = 255 * d[17];
        a[6] = 255 * d[18];
        a[7] = 255 * d[19];
        return a;
      };
      a.prototype.isIdentity = function() {
        var a = this._m;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 1 == a[15] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      a.createIdentity = function() {
        return new a([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0]);
      };
      a.prototype.setMultipliersAndOffsets = function(a, d, e, k, m, l, r, b) {
        for (var h = this._m, s = 0;s < h.length;s++) {
          h[s] = 0;
        }
        h[0] = a;
        h[5] = d;
        h[10] = e;
        h[15] = k;
        h[16] = m;
        h[17] = l;
        h[18] = r;
        h[19] = b;
      };
      a.prototype.transformRGBA = function(a) {
        var d = a >> 24 & 255, e = a >> 16 & 255, k = a >> 8 & 255, m = a & 255, l = this._m;
        a = s(d * l[0] + e * l[1] + k * l[2] + m * l[3] + l[16]);
        var r = s(d * l[4] + e * l[5] + k * l[6] + m * l[7] + l[17]), b = s(d * l[8] + e * l[9] + k * l[10] + m * l[11] + l[18]), d = s(d * l[12] + e * l[13] + k * l[14] + m * l[15] + l[19]);
        return a << 24 | r << 16 | b << 8 | d;
      };
      a.prototype.multiply = function(a) {
        var d = this._m, e = a._m;
        a = d[0];
        var k = d[1], m = d[2], l = d[3], r = d[4], b = d[5], h = d[6], s = d[7], n = d[8], f = d[9], q = d[10], t = d[11], L = d[12], I = d[13], B = d[14], V = d[15], $ = d[16], C = d[17], ja = d[18], ga = d[19], E = e[0], x = e[1], z = e[2], G = e[3], F = e[4], y = e[5], A = e[6], D = e[7], J = e[8], H = e[9], K = e[10], M = e[11], N = e[12], O = e[13], P = e[14], Q = e[15], R = e[16], S = e[17], T = e[18], e = e[19];
        d[0] = a * E + r * x + n * z + L * G;
        d[1] = k * E + b * x + f * z + I * G;
        d[2] = m * E + h * x + q * z + B * G;
        d[3] = l * E + s * x + t * z + V * G;
        d[4] = a * F + r * y + n * A + L * D;
        d[5] = k * F + b * y + f * A + I * D;
        d[6] = m * F + h * y + q * A + B * D;
        d[7] = l * F + s * y + t * A + V * D;
        d[8] = a * J + r * H + n * K + L * M;
        d[9] = k * J + b * H + f * K + I * M;
        d[10] = m * J + h * H + q * K + B * M;
        d[11] = l * J + s * H + t * K + V * M;
        d[12] = a * N + r * O + n * P + L * Q;
        d[13] = k * N + b * O + f * P + I * Q;
        d[14] = m * N + h * O + q * P + B * Q;
        d[15] = l * N + s * O + t * P + V * Q;
        d[16] = a * R + r * S + n * T + L * e + $;
        d[17] = k * R + b * S + f * T + I * e + C;
        d[18] = m * R + h * S + q * T + B * e + ja;
        d[19] = l * R + s * S + t * T + V * e + ga;
      };
      Object.defineProperty(a.prototype, "alphaMultiplier", {get:function() {
        return this._m[15];
      }, enumerable:!0, configurable:!0});
      a.prototype.hasOnlyAlphaMultiplier = function() {
        var a = this._m;
        return 1 == a[0] && 0 == a[1] && 0 == a[2] && 0 == a[3] && 0 == a[4] && 1 == a[5] && 0 == a[6] && 0 == a[7] && 0 == a[8] && 0 == a[9] && 1 == a[10] && 0 == a[11] && 0 == a[12] && 0 == a[13] && 0 == a[14] && 0 == a[16] && 0 == a[17] && 0 == a[18] && 0 == a[19];
      };
      a.prototype.equals = function(a) {
        if (!a) {
          return!1;
        }
        var d = this._m;
        a = a._m;
        for (var e = 0;20 > e;e++) {
          if (.001 < Math.abs(d[e] - a[e])) {
            return!1;
          }
        }
        return!0;
      };
      return a;
    }();
    m.ColorMatrix = t;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      function e(a, c) {
        return-1 !== a.indexOf(c, this.length - c.length);
      }
      var t = m.Geometry.Point3D, k = m.Geometry.Matrix3D, a = m.Geometry.degreesToRadian, c = d.Debug.assert, g = d.Debug.unexpected, p = d.Debug.notImplemented;
      s.SHADER_ROOT = "shaders/";
      var v = function() {
        function u(a, e) {
          this._fillColor = d.Color.Red;
          this._surfaceRegionCache = new d.LRUList;
          this.modelViewProjectionMatrix = k.createIdentity();
          this._canvas = a;
          this._options = e;
          this.gl = a.getContext("experimental-webgl", {preserveDrawingBuffer:!1, antialias:!0, stencil:!0, premultipliedAlpha:!1});
          c(this.gl, "Cannot create WebGL context.");
          this._programCache = Object.create(null);
          this._resize();
          this.gl.pixelStorei(this.gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, e.unpackPremultiplyAlpha ? this.gl.ONE : this.gl.ZERO);
          this._backgroundColor = d.Color.Black;
          this._geometry = new s.WebGLGeometry(this);
          this._tmpVertices = s.Vertex.createEmptyVertices(s.Vertex, 64);
          this._maxSurfaces = e.maxSurfaces;
          this._maxSurfaceSize = e.maxSurfaceSize;
          this.gl.blendFunc(this.gl.ONE, this.gl.ONE_MINUS_SRC_ALPHA);
          this.gl.enable(this.gl.BLEND);
          this.modelViewProjectionMatrix = k.create2DProjection(this._w, this._h, 2E3);
          var b = this;
          this._surfaceRegionAllocator = new m.SurfaceRegionAllocator.SimpleAllocator(function() {
            var a = b._createTexture();
            return new s.WebGLSurface(1024, 1024, a);
          });
        }
        Object.defineProperty(u.prototype, "surfaces", {get:function() {
          return this._surfaceRegionAllocator.surfaces;
        }, enumerable:!0, configurable:!0});
        Object.defineProperty(u.prototype, "fillStyle", {set:function(a) {
          this._fillColor.set(d.Color.parseColor(a));
        }, enumerable:!0, configurable:!0});
        u.prototype.setBlendMode = function(a) {
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
        u.prototype.setBlendOptions = function() {
          this.gl.blendFunc(this._options.sourceBlendFactor, this._options.destinationBlendFactor);
        };
        u.glSupportedBlendMode = function(a) {
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
        u.prototype.create2DProjectionMatrix = function() {
          return k.create2DProjection(this._w, this._h, -this._w);
        };
        u.prototype.createPerspectiveMatrix = function(c, d, b) {
          b = a(b);
          d = k.createPerspective(a(d));
          var e = new t(0, 1, 0), g = new t(0, 0, 0);
          c = new t(0, 0, c);
          c = k.createCameraLookAt(c, g, e);
          c = k.createInverse(c);
          e = k.createIdentity();
          e = k.createMultiply(e, k.createTranslation(-this._w / 2, -this._h / 2));
          e = k.createMultiply(e, k.createScale(1 / this._w, -1 / this._h, .01));
          e = k.createMultiply(e, k.createYRotation(b));
          e = k.createMultiply(e, c);
          return e = k.createMultiply(e, d);
        };
        u.prototype.discardCachedImages = function() {
          2 <= m.traceLevel && m.writer && m.writer.writeLn("Discard Cache");
          for (var a = this._surfaceRegionCache.count / 2 | 0, c = 0;c < a;c++) {
            var b = this._surfaceRegionCache.pop();
            2 <= m.traceLevel && m.writer && m.writer.writeLn("Discard: " + b);
            b.texture.atlas.remove(b.region);
            b.texture = null;
          }
        };
        u.prototype.cacheImage = function(a) {
          var c = this.allocateSurfaceRegion(a.width, a.height);
          2 <= m.traceLevel && m.writer && m.writer.writeLn("Uploading Image: @ " + c.region);
          this._surfaceRegionCache.use(c);
          this.updateSurfaceRegion(a, c);
          return c;
        };
        u.prototype.allocateSurfaceRegion = function(a, c) {
          return this._surfaceRegionAllocator.allocate(a, c);
        };
        u.prototype.updateSurfaceRegion = function(a, c) {
          var b = this.gl;
          b.bindTexture(b.TEXTURE_2D, c.surface.texture);
          b.texSubImage2D(b.TEXTURE_2D, 0, c.region.x, c.region.y, b.RGBA, b.UNSIGNED_BYTE, a);
        };
        u.prototype._resize = function() {
          var a = this.gl;
          this._w = this._canvas.width;
          this._h = this._canvas.height;
          a.viewport(0, 0, this._w, this._h);
          for (var c in this._programCache) {
            this._initializeProgram(this._programCache[c]);
          }
        };
        u.prototype._initializeProgram = function(a) {
          this.gl.useProgram(a);
        };
        u.prototype._createShaderFromFile = function(a) {
          var d = s.SHADER_ROOT + a, b = this.gl;
          a = new XMLHttpRequest;
          a.open("GET", d, !1);
          a.send();
          c(200 === a.status || 0 === a.status, "File : " + d + " not found.");
          if (e(d, ".vert")) {
            d = b.VERTEX_SHADER;
          } else {
            if (e(d, ".frag")) {
              d = b.FRAGMENT_SHADER;
            } else {
              throw "Shader Type: not supported.";
            }
          }
          return this._createShader(d, a.responseText);
        };
        u.prototype.createProgramFromFiles = function() {
          var a = this._programCache["combined.vert-combined.frag"];
          a || (a = this._createProgram([this._createShaderFromFile("combined.vert"), this._createShaderFromFile("combined.frag")]), this._queryProgramAttributesAndUniforms(a), this._initializeProgram(a), this._programCache["combined.vert-combined.frag"] = a);
          return a;
        };
        u.prototype._createProgram = function(a) {
          var c = this.gl, b = c.createProgram();
          a.forEach(function(a) {
            c.attachShader(b, a);
          });
          c.linkProgram(b);
          c.getProgramParameter(b, c.LINK_STATUS) || (g("Cannot link program: " + c.getProgramInfoLog(b)), c.deleteProgram(b));
          return b;
        };
        u.prototype._createShader = function(a, c) {
          var b = this.gl, d = b.createShader(a);
          b.shaderSource(d, c);
          b.compileShader(d);
          return b.getShaderParameter(d, b.COMPILE_STATUS) ? d : (g("Cannot compile shader: " + b.getShaderInfoLog(d)), b.deleteShader(d), null);
        };
        u.prototype._createTexture = function() {
          var a = this.gl, c = a.createTexture();
          a.bindTexture(a.TEXTURE_2D, c);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_S, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_WRAP_T, a.CLAMP_TO_EDGE);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MIN_FILTER, a.LINEAR);
          a.texParameteri(a.TEXTURE_2D, a.TEXTURE_MAG_FILTER, a.LINEAR);
          a.texImage2D(a.TEXTURE_2D, 0, a.RGBA, 1024, 1024, 0, a.RGBA, a.UNSIGNED_BYTE, null);
          return c;
        };
        u.prototype._createFramebuffer = function(a) {
          var c = this.gl, b = c.createFramebuffer();
          c.bindFramebuffer(c.FRAMEBUFFER, b);
          c.framebufferTexture2D(c.FRAMEBUFFER, c.COLOR_ATTACHMENT0, c.TEXTURE_2D, a, 0);
          c.bindFramebuffer(c.FRAMEBUFFER, null);
          return b;
        };
        u.prototype._queryProgramAttributesAndUniforms = function(a) {
          a.uniforms = {};
          a.attributes = {};
          for (var c = this.gl, b = 0, d = c.getProgramParameter(a, c.ACTIVE_ATTRIBUTES);b < d;b++) {
            var e = c.getActiveAttrib(a, b);
            a.attributes[e.name] = e;
            e.location = c.getAttribLocation(a, e.name);
          }
          b = 0;
          for (d = c.getProgramParameter(a, c.ACTIVE_UNIFORMS);b < d;b++) {
            e = c.getActiveUniform(a, b), a.uniforms[e.name] = e, e.location = c.getUniformLocation(a, e.name);
          }
        };
        Object.defineProperty(u.prototype, "target", {set:function(a) {
          var c = this.gl;
          a ? (c.viewport(0, 0, a.w, a.h), c.bindFramebuffer(c.FRAMEBUFFER, a.framebuffer)) : (c.viewport(0, 0, this._w, this._h), c.bindFramebuffer(c.FRAMEBUFFER, null));
        }, enumerable:!0, configurable:!0});
        u.prototype.clear = function(a) {
          a = this.gl;
          a.clearColor(0, 0, 0, 0);
          a.clear(a.COLOR_BUFFER_BIT);
        };
        u.prototype.clearTextureRegion = function(a, c) {
          "undefined" === typeof c && (c = d.Color.None);
          var b = this.gl, e = a.region;
          this.target = a.surface;
          b.enable(b.SCISSOR_TEST);
          b.scissor(e.x, e.y, e.w, e.h);
          b.clearColor(c.r, c.g, c.b, c.a);
          b.clear(b.COLOR_BUFFER_BIT | b.DEPTH_BUFFER_BIT);
          b.disable(b.SCISSOR_TEST);
        };
        u.prototype.sizeOf = function(a) {
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
        u.MAX_SURFACES = 8;
        return u;
      }();
      s.WebGLContext = v;
    })(m.WebGL || (m.WebGL = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
__extends = this.__extends || function(d, m) {
  function s() {
    this.constructor = d;
  }
  for (var e in m) {
    m.hasOwnProperty(e) && (d[e] = m[e]);
  }
  s.prototype = m.prototype;
  d.prototype = new s;
};
(function(d) {
  (function(m) {
    (function(s) {
      var e = d.Debug.assert, t = function(a) {
        function c() {
          a.apply(this, arguments);
        }
        __extends(c, a);
        c.prototype.ensureVertexCapacity = function(a) {
          e(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8 * a);
        };
        c.prototype.writeVertex = function(a, c) {
          e(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 8);
          this.writeVertexUnsafe(a, c);
        };
        c.prototype.writeVertexUnsafe = function(a, c) {
          var d = this._offset >> 2;
          this._f32[d] = a;
          this._f32[d + 1] = c;
          this._offset += 8;
        };
        c.prototype.writeVertex3D = function(a, c, d) {
          e(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 12);
          this.writeVertex3DUnsafe(a, c, d);
        };
        c.prototype.writeVertex3DUnsafe = function(a, c, d) {
          var e = this._offset >> 2;
          this._f32[e] = a;
          this._f32[e + 1] = c;
          this._f32[e + 2] = d;
          this._offset += 12;
        };
        c.prototype.writeTriangleElements = function(a, c, d) {
          e(0 === (this._offset & 1));
          this.ensureCapacity(this._offset + 6);
          var k = this._offset >> 1;
          this._u16[k] = a;
          this._u16[k + 1] = c;
          this._u16[k + 2] = d;
          this._offset += 6;
        };
        c.prototype.ensureColorCapacity = function(a) {
          e(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16 * a);
        };
        c.prototype.writeColorFloats = function(a, c, d, k) {
          e(0 === (this._offset & 2));
          this.ensureCapacity(this._offset + 16);
          this.writeColorFloatsUnsafe(a, c, d, k);
        };
        c.prototype.writeColorFloatsUnsafe = function(a, c, d, e) {
          var l = this._offset >> 2;
          this._f32[l] = a;
          this._f32[l + 1] = c;
          this._f32[l + 2] = d;
          this._f32[l + 3] = e;
          this._offset += 16;
        };
        c.prototype.writeColor = function() {
          var a = Math.random(), c = Math.random(), d = Math.random(), k = Math.random() / 2;
          e(0 === (this._offset & 3));
          this.ensureCapacity(this._offset + 4);
          this._i32[this._offset >> 2] = k << 24 | d << 16 | c << 8 | a;
          this._offset += 4;
        };
        c.prototype.writeColorUnsafe = function(a, c, d, e) {
          this._i32[this._offset >> 2] = e << 24 | d << 16 | c << 8 | a;
          this._offset += 4;
        };
        c.prototype.writeRandomColor = function() {
          this.writeColor();
        };
        return c;
      }(d.ArrayUtilities.ArrayWriter);
      s.BufferWriter = t;
      s.WebGLAttribute = function() {
        return function(a, c, d, e) {
          "undefined" === typeof e && (e = !1);
          this.name = a;
          this.size = c;
          this.type = d;
          this.normalized = e;
        };
      }();
      var k = function() {
        function a(a) {
          this.size = 0;
          this.attributes = a;
        }
        a.prototype.initialize = function(a) {
          for (var d = 0, e = 0;e < this.attributes.length;e++) {
            this.attributes[e].offset = d, d += a.sizeOf(this.attributes[e].type) * this.attributes[e].size;
          }
          this.size = d;
        };
        return a;
      }();
      s.WebGLAttributeList = k;
      k = function() {
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
      s.WebGLGeometry = k;
      k = function(a) {
        function c(c, d, e) {
          a.call(this, c, d, e);
        }
        __extends(c, a);
        c.createEmptyVertices = function(a, c) {
          for (var d = [], e = 0;e < c;e++) {
            d.push(new a(0, 0, 0));
          }
          return d;
        };
        return c;
      }(m.Geometry.Point3D);
      s.Vertex = k;
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
      })(s.WebGLBlendFactor || (s.WebGLBlendFactor = {}));
    })(m.WebGL || (m.WebGL = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(s) {
      var e = function() {
        function e(a, c, g) {
          this.texture = g;
          this.w = a;
          this.h = c;
          this._regionAllocator = new d.RegionAllocator.CompactAllocator(this.w, this.h);
        }
        e.prototype.allocate = function(a, c) {
          var d = this._regionAllocator.allocate(a, c);
          return d ? new t(this, d) : null;
        };
        e.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return e;
      }();
      s.WebGLSurface = e;
      var t = function() {
        return function(d, a) {
          this.surface = d;
          this.region = a;
          this.next = this.previous = null;
        };
      }();
      s.WebGLSurfaceRegion = t;
    })(d.WebGL || (d.WebGL = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      var e = d.Color;
      s.TILE_SIZE = 256;
      s.MIN_UNTILED_SIZE = 256;
      var t = m.Geometry.Matrix, k = m.Geometry.Rectangle, a = m.Geometry.RenderableTileCache, c = d.GFX.Shape, g = d.GFX.ColorMatrix, p = d.Debug.unexpected, v = function(a) {
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
      }(m.StageRendererOptions);
      s.WebGLStageRendererOptions = v;
      var u = function(d) {
        function r(a, c, e) {
          "undefined" === typeof e && (e = new v);
          d.call(this, a, c, e);
          this._tmpVertices = s.Vertex.createEmptyVertices(s.Vertex, 64);
          this._cachedTiles = [];
          a = this._context = new s.WebGLContext(this._canvas, e);
          this._updateSize();
          this._brush = new s.WebGLCombinedBrush(a, new s.WebGLGeometry(a));
          this._stencilBrush = new s.WebGLCombinedBrush(a, new s.WebGLGeometry(a));
          this._scratchCanvas = document.createElement("canvas");
          this._scratchCanvas.width = this._scratchCanvas.height = 2048;
          this._scratchCanvasContext = this._scratchCanvas.getContext("2d", {willReadFrequently:!0});
          this._dynamicScratchCanvas = document.createElement("canvas");
          this._dynamicScratchCanvas.width = this._dynamicScratchCanvas.height = 0;
          this._dynamicScratchCanvasContext = this._dynamicScratchCanvas.getContext("2d", {willReadFrequently:!0});
          this._uploadCanvas = document.createElement("canvas");
          this._uploadCanvas.width = this._uploadCanvas.height = 0;
          this._uploadCanvasContext = this._uploadCanvas.getContext("2d", {willReadFrequently:!0});
          e.showTemporaryCanvases && (document.getElementById("temporaryCanvasPanelContainer").appendChild(this._uploadCanvas), document.getElementById("temporaryCanvasPanelContainer").appendChild(this._scratchCanvas));
          this._clipStack = [];
        }
        __extends(r, d);
        r.prototype.resize = function() {
          this._updateSize();
          this.render();
        };
        r.prototype._updateSize = function() {
          this._viewport = new k(0, 0, this._canvas.width, this._canvas.height);
          this._context._resize();
        };
        r.prototype._cacheImageCallback = function(a, c, d) {
          var e = d.w, f = d.h, g = d.x;
          d = d.y;
          this._uploadCanvas.width = e + 2;
          this._uploadCanvas.height = f + 2;
          this._uploadCanvasContext.drawImage(c.canvas, g, d, e, f, 1, 1, e, f);
          this._uploadCanvasContext.drawImage(c.canvas, g, d, e, 1, 1, 0, e, 1);
          this._uploadCanvasContext.drawImage(c.canvas, g, d + f - 1, e, 1, 1, f + 1, e, 1);
          this._uploadCanvasContext.drawImage(c.canvas, g, d, 1, f, 0, 1, 1, f);
          this._uploadCanvasContext.drawImage(c.canvas, g + e - 1, d, 1, f, e + 1, 1, 1, f);
          return a && a.surface ? (this._options.disableSurfaceUploads || this._context.updateSurfaceRegion(this._uploadCanvas, a), a) : this._context.cacheImage(this._uploadCanvas);
        };
        r.prototype._enterClip = function(a, c, d, e) {
          d.flush();
          var f = this._context.gl;
          0 === this._clipStack.length && (f.enable(f.STENCIL_TEST), f.clear(f.STENCIL_BUFFER_BIT), f.stencilFunc(f.ALWAYS, 1, 1));
          this._clipStack.push(a);
          f.colorMask(!1, !1, !1, !1);
          f.stencilOp(f.KEEP, f.KEEP, f.INCR);
          this._renderFrame(a, c, d, e, 0);
          d.flush();
          f.colorMask(!0, !0, !0, !0);
          f.stencilFunc(f.NOTEQUAL, 0, this._clipStack.length);
          f.stencilOp(f.KEEP, f.KEEP, f.KEEP);
        };
        r.prototype._leaveClip = function(a, c, d, e) {
          d.flush();
          var f = this._context.gl;
          if (a = this._clipStack.pop()) {
            f.colorMask(!1, !1, !1, !1), f.stencilOp(f.KEEP, f.KEEP, f.DECR), this._renderFrame(a, c, d, e, 0), d.flush(), f.colorMask(!0, !0, !0, !0), f.stencilFunc(f.NOTEQUAL, 0, this._clipStack.length), f.stencilOp(f.KEEP, f.KEEP, f.KEEP);
          }
          0 === this._clipStack.length && f.disable(f.STENCIL_TEST);
        };
        r.prototype._renderFrame = function(b, d, l, n, f) {
          "undefined" === typeof f && (f = 0);
          var q = this, r = this._options, u = this._context, v = this._cacheImageCallback.bind(this), B = t.createIdentity(), V = g.createIdentity(), $ = t.createIdentity();
          b.visit(function(d, g, h) {
            f += r.frameSpacing;
            var E = d.getBounds();
            if (h & 4096) {
              q._enterClip(d, g, l, n);
            } else {
              if (h & 8192) {
                q._leaveClip(d, g, l, n);
              } else {
                if (!n.intersectsTransformedAABB(E, g)) {
                  return 2;
                }
                h = d.getConcatenatedAlpha(b);
                r.ignoreColorMatrix || (V = d.getConcatenatedColorMatrix());
                if (d instanceof m.FrameContainer) {
                  if (d instanceof m.ClipRectangle || r.paintBounds) {
                    d.color || (d.color = e.randomColor(.3)), l.fillRectangle(E, d.color, g, f);
                  }
                } else {
                  if (d instanceof c) {
                    if (1 !== d.blendMode) {
                      return 2;
                    }
                    E = d.source.getBounds();
                    if (!E.isEmpty()) {
                      var x = d.source, z = x.properties.tileCache;
                      z || (z = x.properties.tileCache = new a(x, s.TILE_SIZE, s.MIN_UNTILED_SIZE));
                      x = t.createIdentity().translate(E.x, E.y);
                      x.concat(g);
                      x.inverse($);
                      z = z.fetchTiles(n, $, q._scratchCanvasContext, v);
                      for (x = 0;x < z.length;x++) {
                        var G = z[x];
                        B.setIdentity();
                        B.translate(G.bounds.x, G.bounds.y);
                        B.scale(1 / G.scale, 1 / G.scale);
                        B.translate(E.x, E.y);
                        B.concat(g);
                        var F = G.cachedSurfaceRegion;
                        F && F.surface && u._surfaceRegionCache.use(F);
                        var y = new e(1, 1, 1, h);
                        r.paintFlashing && (y = e.randomColor(1));
                        l.drawImage(F, new k(0, 0, G.bounds.w, G.bounds.h), y, V, B, f, d.blendMode) || p();
                        r.drawTiles && (F = G.bounds.clone(), G.color || (G.color = e.randomColor(.4)), l.fillRectangle(new k(0, 0, F.w, F.h), G.color, B, f));
                      }
                    }
                  }
                }
                return 0;
              }
            }
          }, d, 0, 16);
        };
        r.prototype._renderSurfaces = function(a) {
          var c = this._options, d = this._context, g = this._viewport;
          if (c.drawSurfaces) {
            var f = d.surfaces, d = t.createIdentity();
            if (0 <= c.drawSurface && c.drawSurface < f.length) {
              for (var c = f[c.drawSurface | 0], f = new k(0, 0, c.w, c.h), l = f.clone();l.w > g.w;) {
                l.scale(.5, .5);
              }
              a.drawImage(new s.WebGLSurfaceRegion(c, f), l, e.White, null, d, .2);
            } else {
              l = g.w / 5;
              l > g.h / f.length && (l = g.h / f.length);
              a.fillRectangle(new k(g.w - l, 0, l, g.h), new e(0, 0, 0, .5), d, .1);
              for (var r = 0;r < f.length;r++) {
                var c = f[r], p = new k(g.w - l, r * l, l, l);
                a.drawImage(new s.WebGLSurfaceRegion(c, new k(0, 0, c.w, c.h)), p, e.White, null, d, .2);
              }
            }
            a.flush();
          }
        };
        r.prototype.render = function() {
          var a = this._stage, c = this._options, d = this._context.gl;
          this._context.modelViewProjectionMatrix = c.perspectiveCamera ? this._context.createPerspectiveMatrix(c.perspectiveCameraDistance + (c.animateZoom ? .8 * Math.sin(Date.now() / 3E3) : 0), c.perspectiveCameraFOV, c.perspectiveCameraAngle) : this._context.create2DProjectionMatrix();
          var g = this._brush;
          d.clearColor(0, 0, 0, 0);
          d.clear(d.COLOR_BUFFER_BIT | d.DEPTH_BUFFER_BIT);
          g.reset();
          d = this._viewport;
          this._renderFrame(a, a.matrix, g, d, 0);
          g.flush();
          c.paintViewport && (g.fillRectangle(d, new e(.5, 0, 0, .25), t.createIdentity(), 0), g.flush());
          this._renderSurfaces(g);
        };
        return r;
      }(m.StageRenderer);
      s.WebGLStageRenderer = u;
    })(m.WebGL || (m.WebGL = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      var e = d.Color, t = m.Geometry.Point, k = m.Geometry.Matrix3D, a = function() {
        function a(c, d, e) {
          this._target = e;
          this._context = c;
          this._geometry = d;
        }
        a.prototype.reset = function() {
          d.Debug.abstractMethod("reset");
        };
        a.prototype.flush = function() {
          d.Debug.abstractMethod("flush");
        };
        Object.defineProperty(a.prototype, "target", {get:function() {
          return this._target;
        }, set:function(a) {
          this._target !== a && this.flush();
          this._target = a;
        }, enumerable:!0, configurable:!0});
        return a;
      }();
      s.WebGLBrush = a;
      (function(a) {
        a[a.FillColor = 0] = "FillColor";
        a[a.FillTexture = 1] = "FillTexture";
        a[a.FillTextureWithColorMatrix = 2] = "FillTextureWithColorMatrix";
      })(s.WebGLCombinedBrushKind || (s.WebGLCombinedBrushKind = {}));
      var c = function(a) {
        function c(d, k, l) {
          a.call(this, d, k, l);
          this.kind = 0;
          this.color = new e(0, 0, 0, 0);
          this.sampler = 0;
          this.coordinate = new t(0, 0);
        }
        __extends(c, a);
        c.initializeAttributeList = function(a) {
          var d = a.gl;
          c.attributeList || (c.attributeList = new s.WebGLAttributeList([new s.WebGLAttribute("aPosition", 3, d.FLOAT), new s.WebGLAttribute("aCoordinate", 2, d.FLOAT), new s.WebGLAttribute("aColor", 4, d.UNSIGNED_BYTE, !0), new s.WebGLAttribute("aKind", 1, d.FLOAT), new s.WebGLAttribute("aSampler", 1, d.FLOAT)]), c.attributeList.initialize(a));
        };
        c.prototype.writeTo = function(a) {
          a = a.array;
          a.ensureAdditionalCapacity();
          a.writeVertex3DUnsafe(this.x, this.y, this.z);
          a.writeVertexUnsafe(this.coordinate.x, this.coordinate.y);
          a.writeColorUnsafe(255 * this.color.r, 255 * this.color.g, 255 * this.color.b, 255 * this.color.a);
          a.writeFloatUnsafe(this.kind);
          a.writeFloatUnsafe(this.sampler);
        };
        return c;
      }(s.Vertex);
      s.WebGLCombinedBrushVertex = c;
      a = function(a) {
        function d(e, k, l) {
          "undefined" === typeof l && (l = null);
          a.call(this, e, k, l);
          this._blendMode = 1;
          this._program = e.createProgramFromFiles();
          this._surfaces = [];
          c.initializeAttributeList(this._context);
        }
        __extends(d, a);
        d.prototype.reset = function() {
          this._surfaces = [];
          this._geometry.reset();
        };
        d.prototype.drawImage = function(a, c, e, g, b, h, k) {
          "undefined" === typeof h && (h = 0);
          "undefined" === typeof k && (k = 1);
          if (!a || !a.surface) {
            return!0;
          }
          c = c.clone();
          this._colorMatrix && (g && this._colorMatrix.equals(g) || this.flush());
          this._colorMatrix = g;
          this._blendMode !== k && (this.flush(), this._blendMode = k);
          k = this._surfaces.indexOf(a.surface);
          0 > k && (8 === this._surfaces.length && this.flush(), this._surfaces.push(a.surface), k = this._surfaces.length - 1);
          var n = d._tmpVertices, f = a.region.clone();
          f.offset(1, 1).resize(-2, -2);
          f.scale(1 / a.surface.w, 1 / a.surface.h);
          b.transformRectangle(c, n);
          for (a = 0;4 > a;a++) {
            n[a].z = h;
          }
          n[0].coordinate.x = f.x;
          n[0].coordinate.y = f.y;
          n[1].coordinate.x = f.x + f.w;
          n[1].coordinate.y = f.y;
          n[2].coordinate.x = f.x + f.w;
          n[2].coordinate.y = f.y + f.h;
          n[3].coordinate.x = f.x;
          n[3].coordinate.y = f.y + f.h;
          for (a = 0;4 > a;a++) {
            h = d._tmpVertices[a], h.kind = g ? 2 : 1, h.color.set(e), h.sampler = k, h.writeTo(this._geometry);
          }
          this._geometry.addQuad();
          return!0;
        };
        d.prototype.fillRectangle = function(a, c, e, g) {
          "undefined" === typeof g && (g = 0);
          e.transformRectangle(a, d._tmpVertices);
          for (a = 0;4 > a;a++) {
            e = d._tmpVertices[a], e.kind = 0, e.color.set(c), e.z = g, e.writeTo(this._geometry);
          }
          this._geometry.addQuad();
        };
        d.prototype.flush = function() {
          var a = this._geometry, d = this._program, e = this._context.gl, g;
          a.uploadBuffers();
          e.useProgram(d);
          this._target ? (g = k.create2DProjection(this._target.w, this._target.h, 2E3), g = k.createMultiply(g, k.createScale(1, -1, 1))) : g = this._context.modelViewProjectionMatrix;
          e.uniformMatrix4fv(d.uniforms.uTransformMatrix3D.location, !1, g.asWebGLMatrix());
          this._colorMatrix && (e.uniformMatrix4fv(d.uniforms.uColorMatrix.location, !1, this._colorMatrix.asWebGLMatrix()), e.uniform4fv(d.uniforms.uColorVector.location, this._colorMatrix.asWebGLVector()));
          for (g = 0;g < this._surfaces.length;g++) {
            e.activeTexture(e.TEXTURE0 + g), e.bindTexture(e.TEXTURE_2D, this._surfaces[g].texture);
          }
          e.uniform1iv(d.uniforms["uSampler[0]"].location, [0, 1, 2, 3, 4, 5, 6, 7]);
          e.bindBuffer(e.ARRAY_BUFFER, a.buffer);
          var b = c.attributeList.size, h = c.attributeList.attributes;
          for (g = 0;g < h.length;g++) {
            var p = h[g], n = d.attributes[p.name].location;
            e.enableVertexAttribArray(n);
            e.vertexAttribPointer(n, p.size, p.type, p.normalized, b, p.offset);
          }
          this._context.setBlendOptions();
          this._context.target = this._target;
          e.bindBuffer(e.ELEMENT_ARRAY_BUFFER, a.elementBuffer);
          e.drawElements(e.TRIANGLES, 3 * a.triangleCount, e.UNSIGNED_SHORT, 0);
          this.reset();
        };
        d._tmpVertices = s.Vertex.createEmptyVertices(c, 4);
        d._depth = 1;
        return d;
      }(a);
      s.WebGLCombinedBrush = a;
    })(m.WebGL || (m.WebGL = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      function e() {
        void 0 === this.stackDepth && (this.stackDepth = 0);
        void 0 === this.clipStack ? this.clipStack = [0] : this.clipStack.push(0);
        this.stackDepth++;
        v.call(this);
      }
      function t() {
        this.stackDepth--;
        this.clipStack.pop();
        b.call(this);
      }
      function k() {
        p(!this.buildingClippingRegionDepth);
        l.apply(this, arguments);
      }
      function a() {
        p(m.debugClipping.value || !this.buildingClippingRegionDepth);
        r.apply(this, arguments);
      }
      function c() {
        h.call(this);
      }
      function g() {
        void 0 === this.clipStack && (this.clipStack = [0]);
        this.clipStack[this.clipStack.length - 1]++;
        m.debugClipping.value ? (this.strokeStyle = d.ColorStyle.Pink, this.stroke.apply(this, arguments)) : u.apply(this, arguments);
      }
      var p = d.Debug.assert, v = CanvasRenderingContext2D.prototype.save, u = CanvasRenderingContext2D.prototype.clip, l = CanvasRenderingContext2D.prototype.fill, r = CanvasRenderingContext2D.prototype.stroke, b = CanvasRenderingContext2D.prototype.restore, h = CanvasRenderingContext2D.prototype.beginPath;
      s.notifyReleaseChanged = function() {
        CanvasRenderingContext2D.prototype.save = e;
        CanvasRenderingContext2D.prototype.clip = g;
        CanvasRenderingContext2D.prototype.fill = k;
        CanvasRenderingContext2D.prototype.stroke = a;
        CanvasRenderingContext2D.prototype.restore = t;
        CanvasRenderingContext2D.prototype.beginPath = c;
      };
      CanvasRenderingContext2D.prototype.enterBuildingClippingRegion = function() {
        this.buildingClippingRegionDepth || (this.buildingClippingRegionDepth = 0);
        this.buildingClippingRegionDepth++;
      };
      CanvasRenderingContext2D.prototype.leaveBuildingClippingRegion = function() {
        this.buildingClippingRegionDepth--;
      };
    })(m.Canvas2D || (m.Canvas2D = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    (function(d) {
      var e = function() {
        return function(d, a) {
          this.surface = d;
          this.region = a;
        };
      }();
      d.Canvas2DSurfaceRegion = e;
      var m = function() {
        function d(a, c) {
          this.canvas = a;
          this.context = a.getContext("2d");
          this.w = a.width;
          this.h = a.height;
          this._regionAllocator = c;
        }
        d.prototype.allocate = function(a, c) {
          var d = this._regionAllocator.allocate(a, c);
          return d ? new e(this, d) : null;
        };
        d.prototype.free = function(a) {
          this._regionAllocator.free(a.region);
        };
        return d;
      }();
      d.Canvas2DSurface = m;
    })(d.Canvas2D || (d.Canvas2D = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      var e = d.GFX.Geometry.Rectangle, t = d.GFX.Geometry.MipMap;
      (function(a) {
        a[a.NonZero = 0] = "NonZero";
        a[a.EvenOdd = 1] = "EvenOdd";
      })(s.FillRule || (s.FillRule = {}));
      var k = function(a) {
        function c() {
          a.apply(this, arguments);
          this.blending = this.imageSmoothing = this.snapToDevicePixels = !0;
          this.cacheShapes = !1;
          this.cacheShapesMaxSize = 256;
          this.cacheShapesThreshold = 16;
          this.alpha = !1;
        }
        __extends(c, a);
        return c;
      }(m.StageRendererOptions);
      s.Canvas2DStageRendererOptions = k;
      var a = function() {
        return function(a, c, d) {
          "undefined" === typeof c && (c = !1);
          "undefined" === typeof d && (d = null);
          this.options = a;
          this.clipRegion = c;
          this.ignoreMask = d;
        };
      }();
      s.Canvas2DStageRendererState = a;
      var c = e.createMaxI16(), g = function(g) {
        function v(a, c, d) {
          "undefined" === typeof d && (d = new k);
          g.call(this, a, c, d);
          c = this.context = a.getContext("2d", {alpha:d.alpha});
          this._viewport = new e(0, 0, a.width, a.height);
          this._fillRule = "nonzero";
          c.fillRule = c.mozFillRule = this._fillRule;
          v._prepareSurfaceAllocators();
        }
        __extends(v, g);
        v._prepareSurfaceAllocators = function() {
          v._initializedCaches || (v._surfaceCache = new m.SurfaceRegionAllocator.SimpleAllocator(function(a, c) {
            var d = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(d);
            var b = Math.max(1024, a), e = Math.max(1024, c);
            d.width = b;
            d.height = e;
            var g = null, g = 1024 <= a || 1024 <= c ? new m.RegionAllocator.GridAllocator(b, e, b, e) : new m.RegionAllocator.BucketAllocator(b, e);
            return new s.Canvas2DSurface(d, g);
          }), v._shapeCache = new m.SurfaceRegionAllocator.SimpleAllocator(function(a, c) {
            var d = document.createElement("canvas");
            "undefined" !== typeof registerScratchCanvas && registerScratchCanvas(d);
            d.width = 1024;
            d.height = 1024;
            var b = b = new m.RegionAllocator.CompactAllocator(1024, 1024);
            return new s.Canvas2DSurface(d, b);
          }), v._initializedCaches = !0);
        };
        v.prototype.resize = function() {
          var a = this._canvas, c = this.context;
          this._viewport = new e(0, 0, a.width, a.height);
          c.fillRule = c.mozFillRule = this._fillRule;
          this.render();
        };
        v.prototype.render = function() {
          var a = this._stage, c = this.context;
          c.setTransform(1, 0, 0, 1, 0, 0);
          c.save();
          var d = this._options;
          c.globalAlpha = 1;
          var b = this._viewport;
          this.renderFrame(a, b, a.matrix, !0);
          a.trackDirtyRegions && a.dirtyRegion.clear();
          c.restore();
          d && d.paintViewport && (c.beginPath(), c.rect(b.x, b.y, b.w, b.h), c.strokeStyle = "#FF4981", c.stroke());
        };
        v.prototype.renderFrame = function(c, d, e, b) {
          "undefined" === typeof b && (b = !1);
          var g = this.context;
          g.save();
          this._options.paintViewport || (g.beginPath(), g.rect(d.x, d.y, d.w, d.h), g.clip());
          b && g.clearRect(d.x, d.y, d.w, d.h);
          this._renderFrame(g, c, e, d, new a(this._options));
          g.restore();
        };
        v.prototype._renderToSurfaceRegion = function(c, d, g) {
          var b = c.getBounds().clone();
          d.transformRectangleAABB(b);
          b.snap();
          var h = b.x, k = b.y, n = b.clone();
          n.intersect(g);
          n.snap();
          h += n.x - b.x;
          k += n.y - b.y;
          g = v._surfaceCache.allocate(n.w, n.h);
          var b = g.region, b = new e(b.x, b.y, n.w, n.h), f = g.surface.context;
          f.setTransform(1, 0, 0, 1, 0, 0);
          f.clearRect(b.x, b.y, b.w, b.h);
          d = d.clone();
          d.translate(b.x - h, b.y - k);
          f.save();
          f.beginPath();
          f.rect(b.x, b.y, b.w, b.h);
          f.clip();
          this._renderFrame(f, c, d, b, new a(this._options));
          f.restore();
          return{surfaceRegion:g, surfaceRegionBounds:b, clippedBounds:n};
        };
        v.prototype._renderShape = function(a, c, e, b, g) {
          b = c.getBounds();
          if (!b.isEmpty() && g.options.paintRenderable) {
            c = c.source;
            var k = c.properties.renderCount || 0, n = g.options.cacheShapesMaxSize, f = Math.max(e.getAbsoluteScaleX(), e.getAbsoluteScaleY());
            !g.clipRegion && !c.hasFlags(1) && g.options.cacheShapes && k > g.options.cacheShapesThreshold && b.w * f <= n && b.h * f <= n ? ((k = c.properties.mipMap) || (k = c.properties.mipMap = new t(c, v._shapeCache, n)), e = k.getLevel(e), c = e.surfaceRegion, n = c.region, e && a.drawImage(c.surface.canvas, n.x, n.y, n.w, n.h, b.x, b.y, b.w, b.h), g.options.paintFlashing && (a.fillStyle = d.ColorStyle.Green, a.globalAlpha = .5, a.fillRect(b.x, b.y, b.w, b.h))) : (c.properties.renderCount = 
            ++k, c.render(a, null, g.clipRegion), g.options.paintFlashing && (a.fillStyle = d.ColorStyle.randomStyle(), a.globalAlpha = .1, a.fillRect(b.x, b.y, b.w, b.h)));
          }
        };
        v.prototype._renderFrame = function(e, g, k, b, h, p) {
          "undefined" === typeof p && (p = !1);
          var n = this;
          g.visit(function(f, k, r) {
            if (p && g === f) {
              return 0;
            }
            if (!f._hasFlags(16384)) {
              return 2;
            }
            var s = f.getBounds();
            if (h.ignoreMask !== f && f.mask && !h.clipRegion) {
              return e.save(), n._renderFrame(e, f.mask, f.mask.getConcatenatedMatrix(), b, new a(h.options, !0)), n._renderFrame(e, f, k, b, new a(h.options, !1, f)), e.restore(), 2;
            }
            if (r & 4096) {
              e.save(), e.enterBuildingClippingRegion(), n._renderFrame(e, f, k, c, new a(h.options, !0)), e.leaveBuildingClippingRegion();
            } else {
              if (r & 8192) {
                e.restore();
              } else {
                if (!b.intersectsTransformedAABB(s, k)) {
                  return 2;
                }
                (1 === f.pixelSnapping || h.options.snapToDevicePixels) && k.snap();
                e.imageSmoothingEnabled = 1 === f.smoothing || h.options.imageSmoothing;
                e.setTransform(k.a, k.b, k.c, k.d, k.tx, k.ty);
                var t = f.getConcatenatedColorMatrix();
                t.isIdentity() ? (e.globalAlpha = 1, e.globalColorMatrix = null) : t.hasOnlyAlphaMultiplier() ? (e.globalAlpha = t.alphaMultiplier, e.globalColorMatrix = null) : (e.globalAlpha = 1, e.globalColorMatrix = t);
                if (r & 2 && !h.clipRegion) {
                  return 2;
                }
                r = f.getBounds().clone();
                k.transformRectangleAABB(r);
                r.snap();
                if (f !== g && h.options.blending && (e.globalCompositeOperation = n._getCompositeOperation(f.blendMode), 1 !== f.blendMode)) {
                  return s = n._renderToSurfaceRegion(f, k, b), f = s.surfaceRegion, k = s.surfaceRegionBounds, s = s.clippedBounds, e.setTransform(1, 0, 0, 1, 0, 0), e.drawImage(f.surface.canvas, k.x, k.y, k.w, k.h, s.x, s.y, k.w, k.h), f.surface.free(f), 2;
                }
                if (f instanceof m.Shape) {
                  f._previouslyRenderedAABB = r, n._renderShape(e, f, k, b, h);
                } else {
                  if (f instanceof m.ClipRectangle) {
                    return e.save(), e.beginPath(), e.rect(s.x, s.y, s.w, s.h), e.clip(), r.intersect(b), f._hasFlags(32768) || (e.fillStyle = f.color.toCSSStyle(), e.fillRect(s.x, s.y, s.w, s.h)), n._renderFrame(e, f, k, r, h, !0), e.restore(), 2;
                  }
                  h.options.paintBounds && f instanceof m.FrameContainer && (s = f.getBounds().clone(), e.strokeStyle = d.ColorStyle.LightOrange, e.strokeRect(s.x, s.y, s.w, s.h));
                }
                return 0;
              }
            }
          }, k, 0, 16);
        };
        v.prototype._getCompositeOperation = function(a) {
          var c = "source-over";
          switch(a) {
            case 3:
              c = "multiply";
              break;
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
          }
          return c;
        };
        v._initializedCaches = !1;
        return v;
      }(m.StageRenderer);
      s.Canvas2DStageRenderer = g;
    })(m.Canvas2D || (m.Canvas2D = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(d) {
    var s = function() {
      function e(d, e) {
        this.container = d;
        this.pixelRatio = e;
      }
      e.prototype.render = function(e, k) {
        var a = 1 / this.pixelRatio, c = this;
        e.visit(function(e, k) {
          k = k.clone();
          k.scale(a, a);
          if (e instanceof d.Shape) {
            var s = c.getDIV(e);
            s.style.transform = s.style.webkitTransform = k.toCSSTransform();
          }
          return 0;
        }, e.matrix);
      };
      e.prototype.getDIV = function(d) {
        var e = d.properties, a = e.div;
        a || (a = e.div = document.createElement("div"), a.style.width = d.w + "px", a.style.height = d.h + "px", a.style.position = "absolute", e = document.createElement("canvas"), e.width = d.w, e.height = d.h, d.source.render(e.getContext("2d")), a.appendChild(e), a.style.transformOrigin = a.style.webkitTransformOrigin = "0px 0px", a.appendChild(e), this.container.appendChild(a));
        return a;
      };
      return e;
    }();
    d.DOMStageRenderer = s;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = m.Geometry.Point, e = m.Geometry.Matrix, t = m.Geometry.Rectangle, k = d.GFX.WebGL.WebGLStageRenderer, a = d.GFX.WebGL.WebGLStageRendererOptions, c = d.Tools.Mini.FPS, g = function() {
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
    m.State = g;
    var p = function(a) {
      function c() {
        a.apply(this, arguments);
        this._keyCodes = [];
      }
      __extends(c, a);
      c.prototype.onMouseDown = function(a, c) {
        c.altKey && (a.state = new u(a.worldView, a.getMousePosition(c, null), a.worldView.matrix.clone()));
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
    }(g), v = function(a) {
      function c() {
        a.apply(this, arguments);
        this._keyCodes = [];
        this._paused = !1;
        this._mousePosition = new s(0, 0);
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
          var e = a.getMousePosition(c, null), f = a.worldView.matrix.clone(), d = 1 + d / 1E3;
          f.translate(-e.x, -e.y);
          f.scale(d, d);
          f.translate(e.x, e.y);
          a.worldView.matrix = f;
        }
      };
      c.prototype.onKeyPress = function(a, c) {
        if (112 === c.keyCode || "p" === c.key) {
          this._paused = !this._paused;
        }
        this._keyCodes[83] && a.toggleOption("paintRenderable");
        this._keyCodes[86] && a.toggleOption("paintViewport");
        this._keyCodes[66] && a.toggleOption("paintBounds");
        this._keyCodes[70] && a.toggleOption("paintFlashing");
        this._update(a);
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
          var c = m.viewportLoupeDiameter.value, d = m.viewportLoupeDiameter.value;
          a.viewport = new t(this._mousePosition.x - c / 2, this._mousePosition.y - d / 2, c, d);
        } else {
          a.viewport = null;
        }
      };
      return c;
    }(g);
    (function(a) {
      function c() {
        a.apply(this, arguments);
        this._startTime = Date.now();
      }
      __extends(c, a);
      c.prototype.onMouseMove = function(a, c) {
        if (!(10 > Date.now() - this._startTime)) {
          var d = a.queryFrameUnderMouse(c);
          d && d.hasCapability() && (a.state = new u(d, a.getMousePosition(c, null), d.matrix.clone()));
        }
      };
      c.prototype.onMouseUp = function(a, c) {
        a.state = new p;
        a.selectFrameUnderMouse(c);
      };
      return c;
    })(g);
    var u = function(a) {
      function c(b, d, e) {
        a.call(this);
        this._target = b;
        this._startPosition = d;
        this._startMatrix = e;
      }
      __extends(c, a);
      c.prototype.onMouseMove = function(a, c) {
        c.preventDefault();
        var d = a.getMousePosition(c, null);
        d.sub(this._startPosition);
        this._target.matrix = this._startMatrix.clone().translate(d.x, d.y);
        a.state = this;
      };
      c.prototype.onMouseUp = function(a, c) {
        a.state = new p;
      };
      return c;
    }(g), g = function() {
      function g(e, b, h, l) {
        function n() {
          var a = document.createElement("canvas");
          a.style.backgroundColor = $;
          e.appendChild(a);
          u.push(a);
          var b = new m.Canvas2D.Canvas2DStageRendererOptions;
          b.alpha = V;
          t.push(b);
          B.push(new m.Canvas2D.Canvas2DStageRenderer(a, s, b));
        }
        function f() {
          var b = document.createElement("canvas");
          b.style.backgroundColor = $;
          e.appendChild(b);
          u.push(b);
          var c = new a;
          t.push(c);
          B.push(new k(b, s, c));
        }
        function q(a) {
          C.getMousePosition(a, C._world);
          C._state.onMouseWheel(C, a);
          C._persistentState.onMouseWheel(C, a);
        }
        "undefined" === typeof h && (h = !1);
        "undefined" === typeof l && (l = void 0);
        this._state = new p;
        this._persistentState = new v;
        this.paused = !1;
        this.viewport = null;
        this._selectedFrames = [];
        this._eventListeners = d.ObjectUtilities.createEmptyObject();
        var s = this._stage = new m.Stage(128, 128, !0);
        this._worldView = new m.FrameContainer;
        this._worldViewOverlay = new m.FrameContainer;
        this._world = new m.FrameContainer;
        this._stage.addChild(this._worldView);
        this._worldView.addChild(this._world);
        this._worldView.addChild(this._worldViewOverlay);
        this._disableHidpi = h;
        m.hud.value ? (h = document.createElement("div"), h.style.position = "absolute", h.style.bottom = "0", h.style.width = "100%", h.style.height = "16px", this._fpsCanvas = document.createElement("canvas"), h.appendChild(this._fpsCanvas), e.appendChild(h), this._fps = new c(this._fpsCanvas)) : this._fps = null;
        window.addEventListener("resize", this._deferredResizeHandler.bind(this), !1);
        var t = this._options = [], u = this._canvases = [], B = this._renderers = [], V = 0 === l;
        this.transparent = V;
        var $ = void 0 === l ? "#14171a" : 0 === l ? "transparent" : d.ColorUtilities.rgbaToCSSStyle(l);
        switch(b) {
          case 0:
            n();
            break;
          case 1:
            f();
            break;
          case 2:
            n(), f();
        }
        this._resizeHandler();
        this._onMouseUp = this._onMouseUp.bind(this);
        this._onMouseDown = this._onMouseDown.bind(this);
        this._onMouseMove = this._onMouseMove.bind(this);
        var C = this;
        window.addEventListener("mouseup", function(a) {
          C._state.onMouseUp(C, a);
          C._render();
        }, !1);
        window.addEventListener("mousemove", function(a) {
          C.getMousePosition(a, C._world);
          C._state.onMouseMove(C, a);
          C._persistentState.onMouseMove(C, a);
        }, !1);
        window.addEventListener("DOMMouseScroll", q);
        window.addEventListener("mousewheel", q);
        u.forEach(function(a) {
          return a.addEventListener("mousedown", function(a) {
            C._state.onMouseDown(C, a);
          }, !1);
        });
        window.addEventListener("keydown", function(a) {
          C._state.onKeyDown(C, a);
          C._persistentState.onKeyDown(C, a);
        }, !1);
        window.addEventListener("keypress", function(a) {
          C._state.onKeyPress(C, a);
          C._persistentState.onKeyPress(C, a);
        }, !1);
        window.addEventListener("keyup", function(a) {
          C._state.onKeyUp(C, a);
          C._persistentState.onKeyUp(C, a);
        }, !1);
        this._enterRenderLoop();
      }
      g.prototype.addEventListener = function(a, b) {
        this._eventListeners[a] || (this._eventListeners[a] = []);
        this._eventListeners[a].push(b);
      };
      g.prototype._dispatchEvent = function() {
        var a = this._eventListeners.render;
        if (a) {
          for (var b = 0;b < a.length;b++) {
            a[b]();
          }
        }
      };
      g.prototype._enterRenderLoop = function() {
        var a = this;
        requestAnimationFrame(function h() {
          a.render();
          requestAnimationFrame(h);
        });
      };
      Object.defineProperty(g.prototype, "state", {set:function(a) {
        this._state = a;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(g.prototype, "cursor", {set:function(a) {
        this._canvases.forEach(function(b) {
          return b.style.cursor = a;
        });
      }, enumerable:!0, configurable:!0});
      g.prototype._render = function() {
        var a = (this._stage.readyToRender() || m.forcePaint.value) && !this.paused;
        if (a) {
          for (var b = 0;b < this._renderers.length;b++) {
            var c = this._renderers[b];
            c.viewport = this.viewport ? this.viewport : new t(0, 0, this._canvases[b].width, this._canvases[b].height);
            this._dispatchEvent();
            c.render();
          }
        }
        this._fps && this._fps.tickAndRender(!a);
      };
      g.prototype.render = function() {
        this._render();
      };
      Object.defineProperty(g.prototype, "world", {get:function() {
        return this._world;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(g.prototype, "worldView", {get:function() {
        return this._worldView;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(g.prototype, "worldOverlay", {get:function() {
        return this._worldViewOverlay;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(g.prototype, "stage", {get:function() {
        return this._stage;
      }, enumerable:!0, configurable:!0});
      Object.defineProperty(g.prototype, "options", {get:function() {
        return this._options[0];
      }, enumerable:!0, configurable:!0});
      g.prototype.toggleOption = function(a) {
        for (var b = 0;b < this._options.length;b++) {
          var c = this._options[b];
          c[a] = !c[a];
        }
      };
      g.prototype.getOption = function() {
        return this._options[0].paintViewport;
      };
      g.prototype._deferredResizeHandler = function() {
        clearTimeout(this._deferredResizeHandlerTimeout);
        this._deferredResizeHandlerTimeout = setTimeout(this._resizeHandler.bind(this), 1E3);
      };
      g.prototype._resizeHandler = function() {
        var a = window.devicePixelRatio || 1, b = 1;
        1 === a || this._disableHidpi || (b = a / 1);
        for (a = 0;a < this._canvases.length;a++) {
          var c = this._canvases[a], d = c.parentElement, g = d.clientWidth, d = d.clientHeight / this._canvases.length;
          1 < b ? (c.width = g * b, c.height = d * b, c.style.width = g + "px", c.style.height = d + "px") : (c.width = g, c.height = d);
          this._stage.w = c.width;
          this._stage.h = c.height;
          this._renderers[a].resize();
        }
        this._stage.matrix.set(new e(b, 0, 0, b, 0, 0));
      };
      g.prototype.resize = function() {
        this._resizeHandler();
      };
      g.prototype.queryFrameUnderMouse = function(a) {
        a = this.stage.queryFramesByPoint(this.getMousePosition(a, null));
        return 0 < a.length ? a[0] : null;
      };
      g.prototype.selectFrameUnderMouse = function(a) {
        (a = this.queryFrameUnderMouse(a)) && a.hasCapability() ? this._selectedFrames.push(a) : this._selectedFrames = [];
        this._render();
      };
      g.prototype.getMousePosition = function(a, b) {
        var c = this._canvases[0], d = c.getBoundingClientRect(), c = new s(c.width / d.width * (a.clientX - d.left), c.height / d.height * (a.clientY - d.top));
        if (!b) {
          return c;
        }
        d = e.createIdentity();
        b.getConcatenatedMatrix().inverse(d);
        d.transformPoint(c);
        return c;
      };
      g.prototype.getMouseWorldPosition = function(a) {
        return this.getMousePosition(a, this._world);
      };
      g.prototype._onMouseDown = function(a) {
        this._renderers.forEach(function(a) {
          return a.render();
        });
      };
      g.prototype._onMouseUp = function(a) {
      };
      g.prototype._onMouseMove = function(a) {
      };
      return g;
    }();
    m.Easel = g;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = d.GFX.Geometry.Rectangle, e = d.GFX.Geometry.Matrix;
    (function(a) {
      a[a.Simple = 0] = "Simple";
    })(m.Layout || (m.Layout = {}));
    var t = function(a) {
      function c() {
        a.apply(this, arguments);
        this.layout = 0;
      }
      __extends(c, a);
      return c;
    }(m.StageRendererOptions);
    m.TreeStageRendererOptions = t;
    var k = function(a) {
      function c(c, d, e) {
        "undefined" === typeof e && (e = new t);
        a.call(this, c, d, e);
        this.context = c.getContext("2d");
        this._viewport = new s(0, 0, c.width, c.height);
      }
      __extends(c, a);
      c.prototype.render = function() {
        var a = this.context;
        a.save();
        a.clearRect(0, 0, this._stage.w, this._stage.h);
        a.scale(1, 1);
        0 === this._options.layout && this._renderFrameSimple(this.context, this._stage, e.createIdentity(), this._viewport);
        a.restore();
      };
      c.clearContext = function(a, c) {
        a.clearRect(c.x, c.y, c.w, c.h);
      };
      c.prototype._renderFrameSimple = function(a, c, d, e) {
        function k(c) {
          var d = c instanceof m.FrameContainer;
          a.fillStyle = c._hasFlags(512) ? "red" : c._hasFlags(64) ? "blue" : "white";
          var e = d ? 2 : 6;
          a.fillRect(b, h, e, 2);
          if (d) {
            b += e + 2;
            n = Math.max(n, b + 6);
            c = c._children;
            for (d = 0;d < c.length;d++) {
              k(c[d]), d < c.length - 1 && (h += 3, h > r._canvas.height && (a.fillStyle = "gray", a.fillRect(n + 4, 0, 2, r._canvas.height), b = b - s + n + 8, s = n + 8, h = 0, a.fillStyle = "white"));
            }
            b -= e + 2;
          }
        }
        var r = this;
        a.save();
        a.fillStyle = "white";
        var b = 0, h = 0, s = 0, n = 0;
        k(c);
        a.restore();
      };
      return c;
    }(m.StageRenderer);
    m.TreeStageRenderer = k;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(m) {
      var e = d.GFX.Shape, t = d.GFX.RenderableShape, k = d.GFX.RenderableBitmap, a = d.GFX.RenderableVideo, c = d.GFX.RenderableText, g = d.GFX.ColorMatrix, p = d.GFX.FrameContainer, v = d.GFX.ClipRectangle, u = d.ShapeData, l = d.ArrayUtilities.DataBuffer, r = d.GFX.Geometry.Matrix, b = d.GFX.Geometry.Rectangle, h = d.Debug.assert, w = function() {
        function a() {
        }
        a.prototype.writeMouseEvent = function(a, b) {
          var c = this.output;
          c.writeInt(300);
          var e = d.Remoting.MouseEventNames.indexOf(a.type);
          c.writeInt(e);
          c.writeFloat(b.x);
          c.writeFloat(b.y);
          c.writeInt(a.buttons);
          c.writeInt((a.ctrlKey ? 1 : 0) | (a.altKey ? 2 : 0) | (a.shiftKey ? 4 : 0));
        };
        a.prototype.writeKeyboardEvent = function(a) {
          var b = this.output;
          b.writeInt(301);
          var c = d.Remoting.KeyboardEventNames.indexOf(a.type);
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
        a.prototype.writeDecodeImageResponse = function(a, b, c, d, e) {
          var g = this.output;
          g.writeInt(108);
          g.writeInt(a);
          g.writeInt(b);
          this._writeAsset(c);
          g.writeInt(d);
          g.writeInt(e);
        };
        a.prototype._writeAsset = function(a) {
          this.output.writeInt(this.outputAssets.length);
          this.outputAssets.push(a);
        };
        return a;
      }();
      m.GFXChannelSerializer = w;
      w = function() {
        function a(b, c, d) {
          this.root = new v(128, 128);
          d && this.root._setFlags(32768);
          c.addChild(this.root);
          this._frames = [];
          this._assets = [];
          this._easelHost = b;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
        }
        a.prototype._registerAsset = function(a, b, c) {
          "undefined" !== typeof registerInspectorAsset && registerInspectorAsset(a, b, c);
          this._assets[a] = c;
        };
        a.prototype._makeFrame = function(a) {
          if (-1 === a) {
            return null;
          }
          var b = null;
          a & 134217728 ? (a &= -134217729, b = new e(this._assets[a]), this._assets[a].addFrameReferrer(b)) : b = this._frames[a];
          h(b, "Frame " + b + " of " + a + " has not been sent yet.");
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
        a.prototype._decodeImage = function(a, b) {
          var c = new Image, d = this;
          c.src = URL.createObjectURL(new Blob([a]));
          c.onload = function() {
            d._canvas.width = c.width;
            d._canvas.height = c.height;
            d._context.drawImage(c, 0, 0);
            b(d._context.getImageData(0, 0, c.width, c.height));
          };
          c.onerror = function() {
            b(null);
          };
        };
        return a;
      }();
      m.GFXChannelDeserializerContext = w;
      w = function() {
        function e() {
        }
        e.prototype.read = function() {
          for (var a = 0, b = this.input, c = 0, d = 0, e = 0, g = 0, k = 0, l = 0, m = 0, p = 0, n = 0;0 < b.bytesAvailable;) {
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
                g++;
                this._readUpdateFrame();
                break;
              case 104:
                k++;
                this._readUpdateStage();
                break;
              case 105:
                l++;
                this._readUpdateNetStream();
                break;
              case 200:
                m++;
                this._readRegisterFont();
                break;
              case 201:
                p++;
                this._readDrawToBitmap();
                break;
              case 106:
                p++;
                this._readRequestBitmapData();
                break;
              case 107:
                n++;
                this._readDecodeImage();
                break;
              default:
                h(!1, "Unknown MessageReader tag: " + a);
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
          var a = this.input, b = e._temporaryReadColorMatrix, c = 1, d = 1, g = 1, h = 1, k = 0, l = 0, m = 0, p = 0;
          switch(a.readInt()) {
            case 0:
              return e._temporaryReadColorMatrixIdentity;
            case 1:
              h = a.readFloat();
              break;
            case 2:
              c = a.readFloat(), d = a.readFloat(), g = a.readFloat(), h = a.readFloat(), k = a.readInt(), l = a.readInt(), m = a.readInt(), p = a.readInt();
          }
          b.setMultipliersAndOffsets(c, d, g, h, k, l, m, p);
          return b;
        };
        e.prototype._readAsset = function() {
          var a = this.input.readInt(), b = this.inputAssets[a];
          this.inputAssets[a] = null;
          return b;
        };
        e.prototype._readUpdateGraphics = function() {
          for (var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getAsset(c), g = this._readRectangle(), h = u.FromPlainObject(this._readAsset()), k = a.readInt(), l = [], m = 0;m < k;m++) {
            var p = a.readInt();
            l.push(b._getBitmapAsset(p));
          }
          if (e) {
            e.update(h, l, g);
          } else {
            a = new t(c, h, l, g);
            for (m = 0;m < l.length;m++) {
              l[m].addRenderableReferrer(a);
            }
            b._registerAsset(c, d, a);
          }
        };
        e.prototype._readUpdateBitmapData = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = b._getBitmapAsset(c), g = this._readRectangle(), a = a.readInt(), h = l.FromPlainObject(this._readAsset());
          e ? e.updateFromDataBuffer(a, h) : (e = k.FromDataBuffer(a, h, g), b._registerAsset(c, d, e));
        };
        e.prototype._readUpdateTextContent = function() {
          var a = this.input, b = this.context, d = a.readInt(), e = a.readInt(), g = b._getTextAsset(d), h = this._readRectangle(), k = this._readMatrix(), m = a.readInt(), p = a.readInt(), n = a.readInt(), r = a.readBoolean(), s = this._readAsset(), t = l.FromPlainObject(this._readAsset()), u = null, v = a.readInt();
          v && (u = new l(4 * v), a.readBytes(u, 4 * v));
          g ? (g.setBounds(h), g.setContent(s, t, k, u), g.setStyle(m, p), g.reflow(n, r)) : (g = new c(h), g.setContent(s, t, k, u), g.setStyle(m, p), g.reflow(n, r), b._registerAsset(d, e, g));
          if (this.output) {
            for (a = g.textRect, this.output.writeInt(20 * a.w), this.output.writeInt(20 * a.h), this.output.writeInt(20 * a.x), g = g.lines, a = g.length, this.output.writeInt(a), b = 0;b < a;b++) {
              this._writeLineMetrics(g[b]);
            }
          }
        };
        e.prototype._writeLineMetrics = function(a) {
          h(this.output);
          this.output.writeInt(a.x);
          this.output.writeInt(a.width);
          this.output.writeInt(a.ascent);
          this.output.writeInt(a.descent);
          this.output.writeInt(a.leading);
        };
        e.prototype._readUpdateStage = function() {
          var a = this.context, b = this.input.readInt();
          a._frames[b] || (a._frames[b] = a.root);
          var b = this.input.readInt(), c = this._readRectangle();
          a.root.setBounds(c);
          a.root.color = d.Color.FromARGB(b);
        };
        e.prototype._readUpdateNetStream = function() {
          var c = this.context, d = this.input.readInt(), e = c._getVideoAsset(d), g = this.input.readUTF();
          e || (e = new a(g, new b(0, 0, 960, 480)), c._registerAsset(d, 0, e));
        };
        e.prototype._readUpdateFrame = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = b._frames[c];
          d || (d = b._frames[c] = new p);
          var e = a.readInt();
          e & 1 && (d.matrix = this._readMatrix());
          e & 8 && (d.colorMatrix = this._readColorMatrix());
          e & 64 && (d.mask = b._makeFrame(a.readInt()));
          e & 128 && (d.clip = a.readInt());
          e & 32 && (d.blendMode = a.readInt(), d._toggleFlags(16384, a.readBoolean()), d.pixelSnapping = a.readInt(), d.smoothing = a.readInt());
          if (e & 4) {
            e = a.readInt();
            d.clearChildren();
            for (var g = 0;g < e;g++) {
              var k = a.readInt(), l = b._makeFrame(k);
              h(l, "Child " + k + " of " + c + " has not been sent yet.");
              d.addChild(l);
            }
          }
        };
        e.prototype._readRegisterFont = function() {
          var a = this.input, b = a.readInt();
          a.readBoolean();
          a.readBoolean();
          var a = this._readAsset(), c = document.head;
          c.insertBefore(document.createElement("style"), c.firstChild);
          c = document.styleSheets[0];
          c.insertRule("@font-face{font-family:swffont" + b + ";src:url(data:font/opentype;base64," + d.StringUtilities.base64ArrayBuffer(a.buffer) + ")}", c.cssRules.length);
        };
        e.prototype._readDrawToBitmap = function() {
          var a = this.input, b = this.context, c = a.readInt(), d = a.readInt(), e = a.readInt(), g, h, l;
          g = e & 1 ? this._readMatrix().clone() : r.createIdentity();
          e & 8 && (h = this._readColorMatrix());
          e & 16 && (l = this._readRectangle());
          e = a.readInt();
          a.readBoolean();
          a = b._getBitmapAsset(c);
          d = b._makeFrame(d);
          a ? a.drawFrame(d, g, h, e, l) : b._registerAsset(c, -1, k.FromFrame(d, g, h, e, l));
        };
        e.prototype._readRequestBitmapData = function() {
          var a = this.output, b = this.context, c = this.input.readInt();
          b._getBitmapAsset(c).readImageData(a);
        };
        e.prototype._readDecodeImage = function() {
          var a = this.input, b = a.readInt();
          a.readInt();
          var a = this._readAsset(), c = this;
          this.context._decodeImage(a, function(a) {
            var e = new l, f = new d.Remoting.GFX.GFXChannelSerializer, g = [];
            f.output = e;
            f.outputAssets = g;
            a ? f.writeDecodeImageResponse(b, 3, a.data, a.width, a.height) : f.writeDecodeImageResponse(b, 0, null, 0, 0);
            c.context._easelHost.onSendUpdates(e, g);
          });
        };
        e._temporaryReadMatrix = r.createIdentity();
        e._temporaryReadRectangle = b.createEmpty();
        e._temporaryReadColorMatrix = g.createIdentity();
        e._temporaryReadColorMatrixIdentity = g.createIdentity();
        return e;
      }();
      m.GFXChannelDeserializer = w;
    })(m.GFX || (m.GFX = {}));
  })(d.Remoting || (d.Remoting = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    var s = d.GFX.Geometry.Point, e = d.ArrayUtilities.DataBuffer, t = function() {
      function k(a) {
        this._easel = a;
        var c = a.transparent;
        this._frameContainer = a.world;
        this._context = new d.Remoting.GFX.GFXChannelDeserializerContext(this, this._frameContainer, c);
        this._addEventListeners();
      }
      k.prototype.onSendUpdates = function(a, c) {
        throw Error("This method is abstract");
      };
      Object.defineProperty(k.prototype, "stage", {get:function() {
        return this._easel.stage;
      }, enumerable:!0, configurable:!0});
      k.prototype._mouseEventListener = function(a) {
        var c = this._easel.getMouseWorldPosition(a), c = new s(c.x, c.y), g = new e, k = new d.Remoting.GFX.GFXChannelSerializer;
        k.output = g;
        k.writeMouseEvent(a, c);
        this.onSendUpdates(g, []);
      };
      k.prototype._keyboardEventListener = function(a) {
        var c = new e, g = new d.Remoting.GFX.GFXChannelSerializer;
        g.output = c;
        g.writeKeyboardEvent(a);
        this.onSendUpdates(c, []);
      };
      k.prototype._addEventListeners = function() {
        for (var a = this._mouseEventListener.bind(this), c = this._keyboardEventListener.bind(this), d = k._mouseEvents, e = 0;e < d.length;e++) {
          window.addEventListener(d[e], a);
        }
        a = k._keyboardEvents;
        for (e = 0;e < a.length;e++) {
          window.addEventListener(a[e], c);
        }
        this._addFocusEventListeners();
      };
      k.prototype._sendFocusEvent = function(a) {
        var c = new e, g = new d.Remoting.GFX.GFXChannelSerializer;
        g.output = c;
        g.writeFocusEvent(a);
        this.onSendUpdates(c, []);
      };
      k.prototype._addFocusEventListeners = function() {
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
      k.prototype.processUpdates = function(a, c, e) {
        "undefined" === typeof e && (e = null);
        var k = new d.Remoting.GFX.GFXChannelDeserializer;
        k.input = a;
        k.inputAssets = c;
        k.output = e;
        k.context = this._context;
        k.read();
      };
      k.prototype.processExternalCommand = function(a) {
        if ("isEnabled" === a.action) {
          a.result = !1;
        } else {
          throw Error("This command is not supported");
        }
      };
      k.prototype.processFSCommand = function(a, c) {
      };
      k.prototype.processFrame = function() {
      };
      k.prototype.onExernalCallback = function(a) {
        throw Error("This method is abstract");
      };
      k.prototype.sendExernalCallback = function(a, c) {
        var d = {functionName:a, args:c};
        this.onExernalCallback(d);
        if (d.error) {
          throw Error(d.error);
        }
        return d.result;
      };
      k._mouseEvents = d.Remoting.MouseEventNames;
      k._keyboardEvents = d.Remoting.KeyboardEventNames;
      return k;
    }();
    m.EaselHost = t;
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      var e = d.ArrayUtilities.DataBuffer, t = d.CircularBuffer, k = d.Tools.Profiler.TimelineBuffer, a = function(a) {
        function d(e, g, k) {
          a.call(this, e);
          this._timelineRequests = Object.create(null);
          this._playerWindow = g;
          this._window = k;
          this._window.addEventListener("message", function(a) {
            this.onWindowMessage(a.data);
          }.bind(this));
          this._window.addEventListener("syncmessage", function(a) {
            this.onWindowMessage(a.detail, !1);
          }.bind(this));
        }
        __extends(d, a);
        d.prototype.onSendUpdates = function(a, c) {
          var d = a.getBytes();
          this._playerWindow.postMessage({type:"gfx", updates:d, assets:c}, "*", [d.buffer]);
        };
        d.prototype.onExernalCallback = function(a) {
          var c = this._playerWindow.document.createEvent("CustomEvent");
          c.initCustomEvent("syncmessage", !1, !1, {type:"externalCallback", request:a});
          this._playerWindow.dispatchEvent(c);
        };
        d.prototype.requestTimeline = function(a, c) {
          return new Promise(function(d) {
            this._timelineRequests[a] = d;
            this._playerWindow.postMessage({type:"timeline", cmd:c, request:a}, "*");
          }.bind(this));
        };
        d.prototype.onWindowMessage = function(a, c) {
          "undefined" === typeof c && (c = !0);
          if ("object" === typeof a && null !== a) {
            if ("player" === a.type) {
              var d = e.FromArrayBuffer(a.updates.buffer);
              if (c) {
                this.processUpdates(d, a.assets);
              } else {
                var g = new e;
                this.processUpdates(d, a.assets, g);
                a.result = g.toPlainObject();
              }
            } else {
              "frame" !== a.type && ("external" === a.type ? this.processExternalCommand(a.request) : "fscommand" !== a.type && "timelineResponse" === a.type && a.timeline && (a.timeline.__proto__ = k.prototype, a.timeline._marks.__proto__ = t.prototype, a.timeline._times.__proto__ = t.prototype, this._timelineRequests[a.request](a.timeline)));
            }
          }
        };
        return d;
      }(m.EaselHost);
      s.WindowEaselHost = a;
    })(m.Window || (m.Window = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
(function(d) {
  (function(m) {
    (function(s) {
      var e = d.ArrayUtilities.DataBuffer, t = function(k) {
        function a(a) {
          k.call(this, a);
          this._worker = d.Player.Test.FakeSyncWorker.instance;
          this._worker.addEventListener("message", this._onWorkerMessage.bind(this));
          this._worker.addEventListener("syncmessage", this._onSyncWorkerMessage.bind(this));
        }
        __extends(a, k);
        a.prototype.onSendUpdates = function(a, d) {
          var e = a.getBytes();
          this._worker.postMessage({type:"gfx", updates:e, assets:d}, [e.buffer]);
        };
        a.prototype.onExernalCallback = function(a) {
          this._worker.postSyncMessage({type:"externalCallback", request:a});
        };
        a.prototype.requestTimeline = function(a, e) {
          var k;
          switch(a) {
            case "AVM2":
              k = d.AVM2.timelineBuffer;
              break;
            case "Player":
              k = d.Player.timelineBuffer;
              break;
            case "SWF":
              k = d.SWF.timelineBuffer;
          }
          "clear" === e && k && k.reset();
          return Promise.resolve(k);
        };
        a.prototype._onWorkerMessage = function(a, d) {
          "undefined" === typeof d && (d = !0);
          var k = a.data;
          if ("object" === typeof k && null !== k) {
            switch(k.type) {
              case "player":
                var m = e.FromArrayBuffer(k.updates.buffer);
                if (d) {
                  this.processUpdates(m, k.assets);
                } else {
                  var s = new e;
                  this.processUpdates(m, k.assets, s);
                  a.result = s.toPlainObject();
                  a.handled = !0;
                }
                break;
              case "external":
                a.result = this.processExternalCommand(k.command), a.handled = !0;
            }
          }
        };
        a.prototype._onSyncWorkerMessage = function(a) {
          return this._onWorkerMessage(a, !1);
        };
        return a;
      }(m.EaselHost);
      s.TestEaselHost = t;
    })(m.Test || (m.Test = {}));
  })(d.GFX || (d.GFX = {}));
})(Shumway || (Shumway = {}));
console.timeEnd("Load GFX Dependencies");

