/**
 * Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Manifest parser
console.time("Load Parser Dependencies");
var jsGlobal = (function () {
  return this || (1, eval)('this');
})();
var inBrowser = typeof console != "undefined";

var release = false;
var profile = false;

if (!jsGlobal.performance) {
  jsGlobal.performance = {};
}

if (!jsGlobal.performance.now) {
  jsGlobal.performance.now = typeof dateNow !== 'undefined' ? dateNow : Date.now;
}

function log(message) {
  var optionalParams = [];
  for (var _i = 0; _i < (arguments.length - 1); _i++) {
    optionalParams[_i] = arguments[_i + 1];
  }
  jsGlobal.print.apply(jsGlobal, arguments);
}

function warn(message) {
  var optionalParams = [];
  for (var _i = 0; _i < (arguments.length - 1); _i++) {
    optionalParams[_i] = arguments[_i + 1];
  }
  if (inBrowser) {
    console.warn.apply(console, arguments);
  } else {
    jsGlobal.print(Shumway.IndentingWriter.RED + message + Shumway.IndentingWriter.ENDC);
  }
}

var Shumway;
(function (Shumway) {
  (function (CharacterCodes) {
    CharacterCodes[CharacterCodes["_0"] = 48] = "_0";
    CharacterCodes[CharacterCodes["_1"] = 49] = "_1";
    CharacterCodes[CharacterCodes["_2"] = 50] = "_2";
    CharacterCodes[CharacterCodes["_3"] = 51] = "_3";
    CharacterCodes[CharacterCodes["_4"] = 52] = "_4";
    CharacterCodes[CharacterCodes["_5"] = 53] = "_5";
    CharacterCodes[CharacterCodes["_6"] = 54] = "_6";
    CharacterCodes[CharacterCodes["_7"] = 55] = "_7";
    CharacterCodes[CharacterCodes["_8"] = 56] = "_8";
    CharacterCodes[CharacterCodes["_9"] = 57] = "_9";
  })(Shumway.CharacterCodes || (Shumway.CharacterCodes = {}));
  var CharacterCodes = Shumway.CharacterCodes;

  Shumway.UINT32_CHAR_BUFFER_LENGTH = 10;
  Shumway.UINT32_MAX = 0xFFFFFFFF;
  Shumway.UINT32_MAX_DIV_10 = 0x19999999;
  Shumway.UINT32_MAX_MOD_10 = 0x5;

  function isString(value) {
    return typeof value === "string";
  }
  Shumway.isString = isString;

  function isFunction(value) {
    return typeof value === "function";
  }
  Shumway.isFunction = isFunction;

  function isNumber(value) {
    return typeof value === "number";
  }
  Shumway.isNumber = isNumber;

  function isInteger(value) {
    return (value | 0) === value;
  }
  Shumway.isInteger = isInteger;

  function isArray(value) {
    return value instanceof Array;
  }
  Shumway.isArray = isArray;

  function isNumberOrString(value) {
    return typeof value === "number" || typeof value === "string";
  }
  Shumway.isNumberOrString = isNumberOrString;

  function isObject(value) {
    return typeof value === "object" || typeof value === 'function';
  }
  Shumway.isObject = isObject;

  function toNumber(x) {
    return +x;
  }
  Shumway.toNumber = toNumber;

  function isNumericString(value) {
    return String(Number(value)) === value;
  }
  Shumway.isNumericString = isNumericString;

  function isNumeric(value) {
    if (typeof value === "number") {
      return true;
    }
    if (typeof value === "string") {
      var c = value.charCodeAt(0);
      if ((65 <= c && c <= 90) || (97 <= c && c <= 122) || (c === 36) || (c === 95)) {
        return false;
      }
      return isIndex(value) || isNumericString(value);
    }

    return false;
  }
  Shumway.isNumeric = isNumeric;

  function isIndex(value) {
    var index = 0;
    if (typeof value === "number") {
      index = (value | 0);
      if (value === index && index >= 0) {
        return true;
      }
      return value >>> 0 === value;
    }
    if (typeof value !== "string") {
      return false;
    }
    var length = value.length;
    if (length === 0) {
      return false;
    }
    if (value === "0") {
      return true;
    }

    if (length > Shumway.UINT32_CHAR_BUFFER_LENGTH) {
      return false;
    }
    var i = 0;
    index = value.charCodeAt(i++) - 48 /* _0 */;
    if (index < 1 || index > 9) {
      return false;
    }
    var oldIndex = 0;
    var c = 0;
    while (i < length) {
      c = value.charCodeAt(i++) - 48 /* _0 */;
      if (c < 0 || c > 9) {
        return false;
      }
      oldIndex = index;
      index = 10 * index + c;
    }

    if ((oldIndex < Shumway.UINT32_MAX_DIV_10) || (oldIndex === Shumway.UINT32_MAX_DIV_10 && c <= Shumway.UINT32_MAX_MOD_10)) {
      return true;
    }
    return false;
  }
  Shumway.isIndex = isIndex;

  function isNullOrUndefined(value) {
    return value == undefined;
  }
  Shumway.isNullOrUndefined = isNullOrUndefined;

  (function (Debug) {
    function backtrace() {
      try  {
        throw new Error();
      } catch (e) {
        return e.stack ? e.stack.split('\n').slice(2).join('\n') : '';
      }
    }
    Debug.backtrace = backtrace;

    function error(message) {
      if (!inBrowser) {
        warn(message + "\n\nStack Trace:\n" + Debug.backtrace());
      }
      throw new Error(message);
    }
    Debug.error = error;

    function assert(condition, message) {
      if (typeof message === "undefined") { message = "assertion failed"; }
      if (condition === "") {
        condition = true;
      }
      if (!condition) {
        Debug.error(message.toString());
      }
    }
    Debug.assert = assert;

    function assertUnreachable(msg) {
      var location = new Error().stack.split('\n')[1];
      throw new Error("Reached unreachable location " + location + msg);
    }
    Debug.assertUnreachable = assertUnreachable;

    function assertNotImplemented(condition, message) {
      if (!condition) {
        Debug.error("NotImplemented: " + message);
      }
    }
    Debug.assertNotImplemented = assertNotImplemented;

    function warning(message) {
      release || warn(message);
    }
    Debug.warning = warning;

    function notUsed(message) {
      release || Debug.assert(false, "Not Used " + message);
    }
    Debug.notUsed = notUsed;

    function notImplemented(message) {
      log("release: " + release);
      release || Debug.assert(false, "Not Implemented " + message);
    }
    Debug.notImplemented = notImplemented;

    function abstractMethod(message) {
      Debug.assert(false, "Abstract Method " + message);
    }
    Debug.abstractMethod = abstractMethod;

    var somewhatImplementedCache = {};

    function somewhatImplemented(message) {
      if (somewhatImplementedCache[message]) {
        return;
      }
      somewhatImplementedCache[message] = true;
      Debug.warning("somewhatImplemented: " + message);
    }
    Debug.somewhatImplemented = somewhatImplemented;

    function unexpected(message) {
      Debug.assert(false, "Unexpected: " + message);
    }
    Debug.unexpected = unexpected;

    function untested(message) {
      Debug.warning("Congratulations, you've found a code path for which we haven't found a test case. Please submit the test case: " + message);
    }
    Debug.untested = untested;
  })(Shumway.Debug || (Shumway.Debug = {}));
  var Debug = Shumway.Debug;

  function getTicks() {
    return performance.now();
  }
  Shumway.getTicks = getTicks;

  (function (ArrayUtilities) {
    var assert = Shumway.Debug.assert;

    function popManyInto(src, count, dst) {
      release || assert(src.length >= count);
      for (var i = count - 1; i >= 0; i--) {
        dst[i] = src.pop();
      }
      dst.length = count;
    }
    ArrayUtilities.popManyInto = popManyInto;

    function popMany(array, count) {
      release || assert(array.length >= count);
      var start = array.length - count;
      var result = array.slice(start, this.length);
      array.splice(start, count);
      return result;
    }
    ArrayUtilities.popMany = popMany;

    function popManyIntoVoid(array, count) {
      release || assert(array.length >= count);
      array.length = array.length - count;
    }
    ArrayUtilities.popManyIntoVoid = popManyIntoVoid;

    function pushMany(dst, src) {
      for (var i = 0; i < src.length; i++) {
        dst.push(src[i]);
      }
    }
    ArrayUtilities.pushMany = pushMany;

    function top(array) {
      return array.length && array[array.length - 1];
    }
    ArrayUtilities.top = top;

    function last(array) {
      return array.length && array[array.length - 1];
    }
    ArrayUtilities.last = last;

    function peek(array) {
      release || assert(array.length > 0);
      return array[array.length - 1];
    }
    ArrayUtilities.peek = peek;

    function indexOf(array, value) {
      for (var i = 0, j = array.length; i < j; i++) {
        if (array[i] === value) {
          return i;
        }
      }
      return -1;
    }
    ArrayUtilities.indexOf = indexOf;

    function pushUnique(array, value) {
      for (var i = 0, j = array.length; i < j; i++) {
        if (array[i] === value) {
          return i;
        }
      }
      array.push(value);
      return array.length - 1;
    }
    ArrayUtilities.pushUnique = pushUnique;

    function unique(array) {
      var result = [];
      for (var i = 0; i < array.length; i++) {
        pushUnique(result, array[i]);
      }
      return result;
    }
    ArrayUtilities.unique = unique;

    function copyFrom(dst, src) {
      dst.length = 0;
      ArrayUtilities.pushMany(dst, src);
    }
    ArrayUtilities.copyFrom = copyFrom;

    function ensureTypedArrayCapacity(array, capacity) {
      if (array.length < capacity) {
        var oldArray = array;
        array = new array.constructor(Shumway.IntegerUtilities.nearestPowerOfTwo(capacity));
        array.set(oldArray, 0);
      }
      return array;
    }
    ArrayUtilities.ensureTypedArrayCapacity = ensureTypedArrayCapacity;

    var ArrayWriter = (function () {
      function ArrayWriter(initialCapacity) {
        if (typeof initialCapacity === "undefined") { initialCapacity = 16; }
        this._u8 = null;
        this._u16 = null;
        this._i32 = null;
        this._f32 = null;
        this._offset = 0;
        this.ensureCapacity(initialCapacity);
      }
      ArrayWriter.prototype.reset = function () {
        this._offset = 0;
      };

      Object.defineProperty(ArrayWriter.prototype, "offset", {
        get: function () {
          return this._offset;
        },
        enumerable: true,
        configurable: true
      });

      ArrayWriter.prototype.getIndex = function (size) {
        release || assert(size === 1 || size === 2 || size === 4 || size === 8 || size === 16);
        var index = this._offset / size;
        release || assert((index | 0) === index);
        return index;
      };

      ArrayWriter.prototype.ensureAdditionalCapacity = function (size) {
        this.ensureCapacity(this._offset + size);
      };

      ArrayWriter.prototype.ensureCapacity = function (minCapacity) {
        if (!this._u8) {
          this._u8 = new Uint8Array(minCapacity);
        } else if (this._u8.length > minCapacity) {
          return;
        }
        var oldCapacity = this._u8.length;

        var newCapacity = oldCapacity * 2;
        if (newCapacity < minCapacity) {
          newCapacity = minCapacity;
        }
        var u8 = new Uint8Array(newCapacity);
        u8.set(this._u8, 0);
        this._u8 = u8;
        this._u16 = new Uint16Array(u8.buffer);
        this._i32 = new Int32Array(u8.buffer);
        this._f32 = new Float32Array(u8.buffer);
      };

      ArrayWriter.prototype.writeInt = function (v) {
        release || assert((this._offset & 0x3) === 0);
        this.ensureCapacity(this._offset + 4);
        this.writeIntUnsafe(v);
      };

      ArrayWriter.prototype.writeIntAt = function (v, offset) {
        release || assert(offset >= 0 && offset <= this._offset);
        release || assert((offset & 0x3) === 0);
        this.ensureCapacity(offset + 4);
        var index = offset >> 2;
        this._i32[index] = v;
      };

      ArrayWriter.prototype.writeIntUnsafe = function (v) {
        var index = this._offset >> 2;
        this._i32[index] = v;
        this._offset += 4;
      };

      ArrayWriter.prototype.writeFloat = function (v) {
        release || assert((this._offset & 0x3) === 0);
        this.ensureCapacity(this._offset + 4);
        this.writeFloatUnsafe(v);
      };

      ArrayWriter.prototype.writeFloatUnsafe = function (v) {
        var index = this._offset >> 2;
        this._f32[index] = v;
        this._offset += 4;
      };

      ArrayWriter.prototype.write4Floats = function (a, b, c, d) {
        release || assert((this._offset & 0x3) === 0);
        this.ensureCapacity(this._offset + 16);
        this.write4FloatsUnsafe(a, b, c, d);
      };

      ArrayWriter.prototype.write4FloatsUnsafe = function (a, b, c, d) {
        var index = this._offset >> 2;
        this._f32[index + 0] = a;
        this._f32[index + 1] = b;
        this._f32[index + 2] = c;
        this._f32[index + 3] = d;
        this._offset += 16;
      };

      ArrayWriter.prototype.write6Floats = function (a, b, c, d, e, f) {
        release || assert((this._offset & 0x3) === 0);
        this.ensureCapacity(this._offset + 24);
        this.write6FloatsUnsafe(a, b, c, d, e, f);
      };

      ArrayWriter.prototype.write6FloatsUnsafe = function (a, b, c, d, e, f) {
        var index = this._offset >> 2;
        this._f32[index + 0] = a;
        this._f32[index + 1] = b;
        this._f32[index + 2] = c;
        this._f32[index + 3] = d;
        this._f32[index + 4] = e;
        this._f32[index + 5] = f;
        this._offset += 24;
      };

      ArrayWriter.prototype.subF32View = function () {
        return this._f32.subarray(0, this._offset >> 2);
      };

      ArrayWriter.prototype.subI32View = function () {
        return this._i32.subarray(0, this._offset >> 2);
      };

      ArrayWriter.prototype.subU16View = function () {
        return this._u16.subarray(0, this._offset >> 1);
      };

      ArrayWriter.prototype.subU8View = function () {
        return this._u8.subarray(0, this._offset);
      };

      ArrayWriter.prototype.hashWords = function (hash, offset, length) {
        var i32 = this._i32;
        for (var i = 0; i < length; i++) {
          hash = (((31 * hash) | 0) + i32[i]) | 0;
        }
        return hash;
      };

      ArrayWriter.prototype.reserve = function (size) {
        size = (size + 3) & ~0x3;
        this.ensureCapacity(this._offset + size);
        this._offset += size;
      };
      return ArrayWriter;
    })();
    ArrayUtilities.ArrayWriter = ArrayWriter;
  })(Shumway.ArrayUtilities || (Shumway.ArrayUtilities = {}));
  var ArrayUtilities = Shumway.ArrayUtilities;

  var ArrayReader = (function () {
    function ArrayReader(buffer) {
      this._u8 = new Uint8Array(buffer);
      this._u16 = new Uint16Array(buffer);
      this._i32 = new Int32Array(buffer);
      this._f32 = new Float32Array(buffer);
      this._offset = 0;
    }
    Object.defineProperty(ArrayReader.prototype, "offset", {
      get: function () {
        return this._offset;
      },
      enumerable: true,
      configurable: true
    });

    ArrayReader.prototype.isEmpty = function () {
      return this._offset === this._u8.length;
    };

    ArrayReader.prototype.readInt = function () {
      release || Debug.assert((this._offset & 0x3) === 0);
      release || Debug.assert(this._offset <= this._u8.length - 4);
      var v = this._i32[this._offset >> 2];
      this._offset += 4;
      return v;
    };

    ArrayReader.prototype.readFloat = function () {
      release || Debug.assert((this._offset & 0x3) === 0);
      release || Debug.assert(this._offset <= this._u8.length - 4);
      var v = this._f32[this._offset >> 2];
      this._offset += 4;
      return v;
    };
    return ArrayReader;
  })();
  Shumway.ArrayReader = ArrayReader;

  (function (ObjectUtilities) {
    function boxValue(value) {
      if (isNullOrUndefined(value) || isObject(value)) {
        return value;
      }
      return Object(value);
    }
    ObjectUtilities.boxValue = boxValue;

    function toKeyValueArray(object) {
      var hasOwnProperty = Object.prototype.hasOwnProperty;
      var array = [];
      for (var k in object) {
        if (hasOwnProperty.call(object, k)) {
          array.push([k, object[k]]);
        }
      }
      return array;
    }
    ObjectUtilities.toKeyValueArray = toKeyValueArray;

    function isPrototypeWriteable(object) {
      return Object.getOwnPropertyDescriptor(object, "prototype").writable;
    }
    ObjectUtilities.isPrototypeWriteable = isPrototypeWriteable;

    function hasOwnProperty(object, name) {
      return Object.prototype.hasOwnProperty.call(object, name);
    }
    ObjectUtilities.hasOwnProperty = hasOwnProperty;

    function propertyIsEnumerable(object, name) {
      return Object.prototype.propertyIsEnumerable.call(object, name);
    }
    ObjectUtilities.propertyIsEnumerable = propertyIsEnumerable;

    function getOwnPropertyDescriptor(object, name) {
      return Object.getOwnPropertyDescriptor(object, name);
    }
    ObjectUtilities.getOwnPropertyDescriptor = getOwnPropertyDescriptor;

    function hasOwnGetter(object, name) {
      var d = Object.getOwnPropertyDescriptor(object, name);
      return !!(d && d.get);
    }
    ObjectUtilities.hasOwnGetter = hasOwnGetter;

    function getOwnGetter(object, name) {
      var d = Object.getOwnPropertyDescriptor(object, name);
      return d ? d.get : null;
    }
    ObjectUtilities.getOwnGetter = getOwnGetter;

    function hasOwnSetter(object, name) {
      var d = Object.getOwnPropertyDescriptor(object, name);
      return !!(d && !!d.set);
    }
    ObjectUtilities.hasOwnSetter = hasOwnSetter;

    function createObject(prototype) {
      return Object.create(prototype);
    }
    ObjectUtilities.createObject = createObject;

    function createEmptyObject() {
      return Object.create(null);
    }
    ObjectUtilities.createEmptyObject = createEmptyObject;

    function createMap() {
      return Object.create(null);
    }
    ObjectUtilities.createMap = createMap;

    function createArrayMap() {
      return [];
    }
    ObjectUtilities.createArrayMap = createArrayMap;

    function defineReadOnlyProperty(object, name, value) {
      Object.defineProperty(object, name, {
        value: value,
        writable: false,
        configurable: true,
        enumerable: false
      });
    }
    ObjectUtilities.defineReadOnlyProperty = defineReadOnlyProperty;

    function getOwnPropertyDescriptors(object) {
      var o = ObjectUtilities.createMap();
      var properties = Object.getOwnPropertyNames(object);
      for (var i = 0; i < properties.length; i++) {
        o[properties[i]] = Object.getOwnPropertyDescriptor(object, properties[i]);
      }
      return o;
    }
    ObjectUtilities.getOwnPropertyDescriptors = getOwnPropertyDescriptors;

    function cloneObject(object) {
      var clone = Object.create(Object.getPrototypeOf(object));
      copyOwnProperties(clone, object);
      return clone;
    }
    ObjectUtilities.cloneObject = cloneObject;

    function copyProperties(object, template) {
      for (var property in template) {
        object[property] = template[property];
      }
    }
    ObjectUtilities.copyProperties = copyProperties;

    function copyOwnProperties(object, template) {
      for (var property in template) {
        if (hasOwnProperty(template, property)) {
          object[property] = template[property];
        }
      }
    }
    ObjectUtilities.copyOwnProperties = copyOwnProperties;

    function copyOwnPropertyDescriptors(object, template, overwrite) {
      if (typeof overwrite === "undefined") { overwrite = true; }
      for (var property in template) {
        if (hasOwnProperty(template, property)) {
          var descriptor = Object.getOwnPropertyDescriptor(template, property);
          if (!overwrite && hasOwnProperty(object, property)) {
            continue;
          }
          release || Debug.assert(descriptor);
          try  {
            Object.defineProperty(object, property, descriptor);
          } catch (e) {
          }
        }
      }
    }
    ObjectUtilities.copyOwnPropertyDescriptors = copyOwnPropertyDescriptors;

    function getLatestGetterOrSetterPropertyDescriptor(object, name) {
      var descriptor = {};
      while (object) {
        var tmp = Object.getOwnPropertyDescriptor(object, name);
        if (tmp) {
          descriptor.get = descriptor.get || tmp.get;
          descriptor.set = descriptor.set || tmp.set;
        }
        if (descriptor.get && descriptor.set) {
          break;
        }
        object = Object.getPrototypeOf(object);
      }
      return descriptor;
    }
    ObjectUtilities.getLatestGetterOrSetterPropertyDescriptor = getLatestGetterOrSetterPropertyDescriptor;

    function defineNonEnumerableGetterOrSetter(obj, name, value, isGetter) {
      var descriptor = ObjectUtilities.getLatestGetterOrSetterPropertyDescriptor(obj, name);
      descriptor.configurable = true;
      descriptor.enumerable = false;
      if (isGetter) {
        descriptor.get = value;
      } else {
        descriptor.set = value;
      }
      Object.defineProperty(obj, name, descriptor);
    }
    ObjectUtilities.defineNonEnumerableGetterOrSetter = defineNonEnumerableGetterOrSetter;

    function defineNonEnumerableGetter(obj, name, getter) {
      Object.defineProperty(obj, name, {
        get: getter,
        configurable: true,
        enumerable: false
      });
    }
    ObjectUtilities.defineNonEnumerableGetter = defineNonEnumerableGetter;

    function defineNonEnumerableSetter(obj, name, setter) {
      Object.defineProperty(obj, name, {
        set: setter,
        configurable: true,
        enumerable: false
      });
    }
    ObjectUtilities.defineNonEnumerableSetter = defineNonEnumerableSetter;

    function defineNonEnumerableProperty(obj, name, value) {
      Object.defineProperty(obj, name, {
        value: value,
        writable: true,
        configurable: true,
        enumerable: false
      });
    }
    ObjectUtilities.defineNonEnumerableProperty = defineNonEnumerableProperty;

    function defineNonEnumerableForwardingProperty(obj, name, otherName) {
      Object.defineProperty(obj, name, {
        get: FunctionUtilities.makeForwardingGetter(otherName),
        set: FunctionUtilities.makeForwardingSetter(otherName),
        writable: true,
        configurable: true,
        enumerable: false
      });
    }
    ObjectUtilities.defineNonEnumerableForwardingProperty = defineNonEnumerableForwardingProperty;

    function defineNewNonEnumerableProperty(obj, name, value) {
      release || Debug.assert(!Object.prototype.hasOwnProperty.call(obj, name), "Property: " + name + " already exits.");
      ObjectUtilities.defineNonEnumerableProperty(obj, name, value);
    }
    ObjectUtilities.defineNewNonEnumerableProperty = defineNewNonEnumerableProperty;
  })(Shumway.ObjectUtilities || (Shumway.ObjectUtilities = {}));
  var ObjectUtilities = Shumway.ObjectUtilities;

  (function (FunctionUtilities) {
    function makeForwardingGetter(target) {
      return new Function("return this[\"" + target + "\"]");
    }
    FunctionUtilities.makeForwardingGetter = makeForwardingGetter;

    function makeForwardingSetter(target) {
      return new Function("value", "this[\"" + target + "\"] = value;");
    }
    FunctionUtilities.makeForwardingSetter = makeForwardingSetter;

    function bindSafely(fn, object) {
      release || Debug.assert(!fn.boundTo && object);
      var f = fn.bind(object);
      f.boundTo = object;
      return f;
    }
    FunctionUtilities.bindSafely = bindSafely;
  })(Shumway.FunctionUtilities || (Shumway.FunctionUtilities = {}));
  var FunctionUtilities = Shumway.FunctionUtilities;

  (function (StringUtilities) {
    var assert = Shumway.Debug.assert;

    function memorySizeToString(value) {
      value |= 0;
      var K = 1024;
      var M = K * K;
      if (value < K) {
        return value + " B";
      } else if (value < M) {
        return (value / K).toFixed(2) + "KB";
      } else {
        return (value / M).toFixed(2) + "MB";
      }
    }
    StringUtilities.memorySizeToString = memorySizeToString;

    function toSafeString(value) {
      if (typeof value === "string") {
        return "\"" + value + "\"";
      }
      if (typeof value === "number" || typeof value === "boolean") {
        return String(value);
      }
      return typeof value;
    }
    StringUtilities.toSafeString = toSafeString;

    function toSafeArrayString(array) {
      var str = [];
      for (var i = 0; i < array.length; i++) {
        str.push(toSafeString(array[i]));
      }
      return str.join(", ");
    }
    StringUtilities.toSafeArrayString = toSafeArrayString;

    function utf8decode(str) {
      var bytes = new Uint8Array(str.length * 4);
      var b = 0;
      for (var i = 0, j = str.length; i < j; i++) {
        var code = str.charCodeAt(i);
        if (code <= 0x7f) {
          bytes[b++] = code;
          continue;
        }

        if (0xD800 <= code && code <= 0xDBFF) {
          var codeLow = str.charCodeAt(i + 1);
          if (0xDC00 <= codeLow && codeLow <= 0xDFFF) {
            code = ((code & 0x3FF) << 10) + (codeLow & 0x3FF) + 0x10000;
            ++i;
          }
        }

        if ((code & 0xFFE00000) !== 0) {
          bytes[b++] = 0xF8 | ((code >>> 24) & 0x03);
          bytes[b++] = 0x80 | ((code >>> 18) & 0x3F);
          bytes[b++] = 0x80 | ((code >>> 12) & 0x3F);
          bytes[b++] = 0x80 | ((code >>> 6) & 0x3F);
          bytes[b++] = 0x80 | (code & 0x3F);
        } else if ((code & 0xFFFF0000) !== 0) {
          bytes[b++] = 0xF0 | ((code >>> 18) & 0x07);
          bytes[b++] = 0x80 | ((code >>> 12) & 0x3F);
          bytes[b++] = 0x80 | ((code >>> 6) & 0x3F);
          bytes[b++] = 0x80 | (code & 0x3F);
        } else if ((code & 0xFFFFF800) !== 0) {
          bytes[b++] = 0xE0 | ((code >>> 12) & 0x0F);
          bytes[b++] = 0x80 | ((code >>> 6) & 0x3F);
          bytes[b++] = 0x80 | (code & 0x3F);
        } else {
          bytes[b++] = 0xC0 | ((code >>> 6) & 0x1F);
          bytes[b++] = 0x80 | (code & 0x3F);
        }
      }
      return bytes.subarray(0, b);
    }
    StringUtilities.utf8decode = utf8decode;

    function utf8encode(bytes) {
      var j = 0, str = "";
      while (j < bytes.length) {
        var b1 = bytes[j++] & 0xFF;
        if (b1 <= 0x7F) {
          str += String.fromCharCode(b1);
        } else {
          var currentPrefix = 0xC0;
          var validBits = 5;
          do {
            var mask = (currentPrefix >> 1) | 0x80;
            if ((b1 & mask) === currentPrefix)
              break;
            currentPrefix = (currentPrefix >> 1) | 0x80;
            --validBits;
          } while(validBits >= 0);

          if (validBits <= 0) {
            str += String.fromCharCode(b1);
            continue;
          }
          var code = (b1 & ((1 << validBits) - 1));
          var invalid = false;
          for (var i = 5; i >= validBits; --i) {
            var bi = bytes[j++];
            if ((bi & 0xC0) != 0x80) {
              invalid = true;
              break;
            }
            code = (code << 6) | (bi & 0x3F);
          }
          if (invalid) {
            for (var k = j - (7 - i); k < j; ++k) {
              str += String.fromCharCode(bytes[k] & 255);
            }
            continue;
          }
          if (code >= 0x10000) {
            str += String.fromCharCode((((code - 0x10000) >> 10) & 0x3FF) | 0xD800, (code & 0x3FF) | 0xDC00);
          } else {
            str += String.fromCharCode(code);
          }
        }
      }
      return str;
    }
    StringUtilities.utf8encode = utf8encode;

    function base64ArrayBuffer(arrayBuffer) {
      var base64 = '';
      var encodings = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

      var bytes = new Uint8Array(arrayBuffer);
      var byteLength = bytes.byteLength;
      var byteRemainder = byteLength % 3;
      var mainLength = byteLength - byteRemainder;

      var a, b, c, d;
      var chunk;

      for (var i = 0; i < mainLength; i = i + 3) {
        chunk = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];

        a = (chunk & 16515072) >> 18;
        b = (chunk & 258048) >> 12;
        c = (chunk & 4032) >> 6;
        d = chunk & 63;

        base64 += encodings[a] + encodings[b] + encodings[c] + encodings[d];
      }

      if (byteRemainder == 1) {
        chunk = bytes[mainLength];

        a = (chunk & 252) >> 2;

        b = (chunk & 3) << 4;

        base64 += encodings[a] + encodings[b] + '==';
      } else if (byteRemainder == 2) {
        chunk = (bytes[mainLength] << 8) | bytes[mainLength + 1];

        a = (chunk & 64512) >> 10;
        b = (chunk & 1008) >> 4;

        c = (chunk & 15) << 2;

        base64 += encodings[a] + encodings[b] + encodings[c] + '=';
      }
      return base64;
    }
    StringUtilities.base64ArrayBuffer = base64ArrayBuffer;

    function escapeString(str) {
      if (str !== undefined) {
        str = str.replace(/[^\w$]/gi, "$");
        if (/^\d/.test(str)) {
          str = '$' + str;
        }
      }
      return str;
    }
    StringUtilities.escapeString = escapeString;

    function fromCharCodeArray(buffer) {
      var str = "", SLICE = 1024 * 16;
      for (var i = 0; i < buffer.length; i += SLICE) {
        var chunk = Math.min(buffer.length - i, SLICE);
        str += String.fromCharCode.apply(null, buffer.subarray(i, i + chunk));
      }
      return str;
    }
    StringUtilities.fromCharCodeArray = fromCharCodeArray;

    var _encoding = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789$_';
    function variableLengthEncodeInt32(n) {
      var e = _encoding;
      var bitCount = (32 - Math.clz32(n));
      release || assert(bitCount <= 32, bitCount);
      var l = Math.ceil(bitCount / 6);

      var s = e[l];
      for (var i = l - 1; i >= 0; i--) {
        var offset = (i * 6);
        s += e[(n >> offset) & 0x3F];
      }
      release || assert(StringUtilities.variableLengthDecodeInt32(s) === n, n + " : " + s + " - " + l + " bits: " + bitCount);
      return s;
    }
    StringUtilities.variableLengthEncodeInt32 = variableLengthEncodeInt32;

    function toEncoding(n) {
      return _encoding[n];
    }
    StringUtilities.toEncoding = toEncoding;

    function fromEncoding(s) {
      var c = s.charCodeAt(0);
      var e = 0;
      if (c >= 65 && c <= 90) {
        return c - 65;
      } else if (c >= 97 && c <= 122) {
        return c - 71;
      } else if (c >= 48 && c <= 57) {
        return c + 4;
      } else if (c === 36) {
        return 62;
      } else if (c === 95) {
        return 63;
      }
      release || assert(false, "Invalid Encoding");
    }
    StringUtilities.fromEncoding = fromEncoding;

    function variableLengthDecodeInt32(s) {
      var l = StringUtilities.fromEncoding(s[0]);
      var n = 0;
      for (var i = 0; i < l; i++) {
        var offset = ((l - i - 1) * 6);
        n |= StringUtilities.fromEncoding(s[1 + i]) << offset;
      }
      return n;
    }
    StringUtilities.variableLengthDecodeInt32 = variableLengthDecodeInt32;

    function trimMiddle(s, maxLength) {
      if (s.length <= maxLength) {
        return s;
      }
      var leftHalf = maxLength >> 1;
      var rightHalf = maxLength - leftHalf - 1;
      return s.substr(0, leftHalf) + "\u2026" + s.substr(s.length - rightHalf, rightHalf);
    }
    StringUtilities.trimMiddle = trimMiddle;

    function multiple(s, count) {
      var o = "";
      for (var i = 0; i < count; i++) {
        o += s;
      }
      return o;
    }
    StringUtilities.multiple = multiple;

    function indexOfAny(s, chars, position) {
      var index = s.length;
      for (var i = 0; i < chars.length; i++) {
        var j = s.indexOf(chars[i], position);
        if (j >= 0) {
          index = Math.min(index, j);
        }
      }
      return index === s.length ? -1 : index;
    }
    StringUtilities.indexOfAny = indexOfAny;

    var _concat3array = new Array(3);
    var _concat4array = new Array(4);
    var _concat5array = new Array(5);
    var _concat6array = new Array(6);
    var _concat7array = new Array(7);
    var _concat8array = new Array(8);
    var _concat9array = new Array(9);

    function concat3(s0, s1, s2) {
      _concat3array[0] = s0;
      _concat3array[1] = s1;
      _concat3array[2] = s2;
      return _concat3array.join('');
    }
    StringUtilities.concat3 = concat3;

    function concat4(s0, s1, s2, s3) {
      _concat4array[0] = s0;
      _concat4array[1] = s1;
      _concat4array[2] = s2;
      _concat4array[3] = s3;
      return _concat4array.join('');
    }
    StringUtilities.concat4 = concat4;

    function concat5(s0, s1, s2, s3, s4) {
      _concat5array[0] = s0;
      _concat5array[1] = s1;
      _concat5array[2] = s2;
      _concat5array[3] = s3;
      _concat5array[4] = s4;
      return _concat5array.join('');
    }
    StringUtilities.concat5 = concat5;

    function concat6(s0, s1, s2, s3, s4, s5) {
      _concat6array[0] = s0;
      _concat6array[1] = s1;
      _concat6array[2] = s2;
      _concat6array[3] = s3;
      _concat6array[4] = s4;
      _concat6array[5] = s5;
      return _concat6array.join('');
    }
    StringUtilities.concat6 = concat6;

    function concat7(s0, s1, s2, s3, s4, s5, s6) {
      _concat7array[0] = s0;
      _concat7array[1] = s1;
      _concat7array[2] = s2;
      _concat7array[3] = s3;
      _concat7array[4] = s4;
      _concat7array[5] = s5;
      _concat7array[6] = s6;
      return _concat7array.join('');
    }
    StringUtilities.concat7 = concat7;

    function concat8(s0, s1, s2, s3, s4, s5, s6, s7) {
      _concat8array[0] = s0;
      _concat8array[1] = s1;
      _concat8array[2] = s2;
      _concat8array[3] = s3;
      _concat8array[4] = s4;
      _concat8array[5] = s5;
      _concat8array[6] = s6;
      _concat8array[7] = s7;
      return _concat8array.join('');
    }
    StringUtilities.concat8 = concat8;

    function concat9(s0, s1, s2, s3, s4, s5, s6, s7, s8) {
      _concat9array[0] = s0;
      _concat9array[1] = s1;
      _concat9array[2] = s2;
      _concat9array[3] = s3;
      _concat9array[4] = s4;
      _concat9array[5] = s5;
      _concat9array[6] = s6;
      _concat9array[7] = s7;
      _concat9array[8] = s8;
      return _concat9array.join('');
    }
    StringUtilities.concat9 = concat9;
  })(Shumway.StringUtilities || (Shumway.StringUtilities = {}));
  var StringUtilities = Shumway.StringUtilities;

  (function (HashUtilities) {
    var _md5R = new Uint8Array([
      7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
      5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
      4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
      6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21]);

    var _md5K = new Int32Array([
      -680876936, -389564586, 606105819, -1044525330, -176418897, 1200080426,
      -1473231341, -45705983, 1770035416, -1958414417, -42063, -1990404162,
      1804603682, -40341101, -1502002290, 1236535329, -165796510, -1069501632,
      643717713, -373897302, -701558691, 38016083, -660478335, -405537848,
      568446438, -1019803690, -187363961, 1163531501, -1444681467, -51403784,
      1735328473, -1926607734, -378558, -2022574463, 1839030562, -35309556,
      -1530992060, 1272893353, -155497632, -1094730640, 681279174, -358537222,
      -722521979, 76029189, -640364487, -421815835, 530742520, -995338651,
      -198630844, 1126891415, -1416354905, -57434055, 1700485571, -1894986606,
      -1051523, -2054922799, 1873313359, -30611744, -1560198380, 1309151649,
      -145523070, -1120210379, 718787259, -343485551]);

    function hashBytesTo32BitsMD5(data, offset, length) {
      var r = _md5R;
      var k = _md5K;
      var h0 = 1732584193, h1 = -271733879, h2 = -1732584194, h3 = 271733878;

      var paddedLength = (length + 72) & ~63;
      var padded = new Uint8Array(paddedLength);
      var i, j, n;
      for (i = 0; i < length; ++i) {
        padded[i] = data[offset++];
      }
      padded[i++] = 0x80;
      n = paddedLength - 8;
      while (i < n) {
        padded[i++] = 0;
      }
      padded[i++] = (length << 3) & 0xFF;
      padded[i++] = (length >> 5) & 0xFF;
      padded[i++] = (length >> 13) & 0xFF;
      padded[i++] = (length >> 21) & 0xFF;
      padded[i++] = (length >>> 29) & 0xFF;
      padded[i++] = 0;
      padded[i++] = 0;
      padded[i++] = 0;

      var w = new Int32Array(16);
      for (i = 0; i < paddedLength;) {
        for (j = 0; j < 16; ++j, i += 4) {
          w[j] = (padded[i] | (padded[i + 1] << 8) | (padded[i + 2] << 16) | (padded[i + 3] << 24));
        }
        var a = h0, b = h1, c = h2, d = h3, f, g;
        for (j = 0; j < 64; ++j) {
          if (j < 16) {
            f = (b & c) | ((~b) & d);
            g = j;
          } else if (j < 32) {
            f = (d & b) | ((~d) & c);
            g = (5 * j + 1) & 15;
          } else if (j < 48) {
            f = b ^ c ^ d;
            g = (3 * j + 5) & 15;
          } else {
            f = c ^ (b | (~d));
            g = (7 * j) & 15;
          }
          var tmp = d, rotateArg = (a + f + k[j] + w[g]) | 0, rotate = r[j];
          d = c;
          c = b;
          b = (b + ((rotateArg << rotate) | (rotateArg >>> (32 - rotate)))) | 0;
          a = tmp;
        }
        h0 = (h0 + a) | 0;
        h1 = (h1 + b) | 0;
        h2 = (h2 + c) | 0;
        h3 = (h3 + d) | 0;
      }
      return h0;
    }
    HashUtilities.hashBytesTo32BitsMD5 = hashBytesTo32BitsMD5;

    function hashBytesTo32BitsAdler(data, offset, length) {
      var a = 1;
      var b = 0;
      var end = offset + length;
      for (var i = offset; i < end; ++i) {
        a = (a + (data[i] & 0xff)) % 65521;
        b = (b + a) % 65521;
      }
      return (b << 16) | a;
    }
    HashUtilities.hashBytesTo32BitsAdler = hashBytesTo32BitsAdler;
  })(Shumway.HashUtilities || (Shumway.HashUtilities = {}));
  var HashUtilities = Shumway.HashUtilities;

  var Random = (function () {
    function Random() {
    }
    Random.seed = function (seed) {
      Random._state[0] = seed;
      Random._state[1] = seed;
    };

    Random.next = function () {
      var s = this._state;
      var r0 = (Math.imul(18273, s[0] & 0xFFFF) + (s[0] >>> 16)) | 0;
      s[0] = r0;
      var r1 = (Math.imul(36969, s[1] & 0xFFFF) + (s[1] >>> 16)) | 0;
      s[1] = r1;
      var x = ((r0 << 16) + (r1 & 0xFFFF)) | 0;

      return (x < 0 ? (x + 0x100000000) : x) * 2.3283064365386962890625e-10;
    };
    Random._state = new Uint32Array([0xDEAD, 0xBEEF]);
    return Random;
  })();
  Shumway.Random = Random;

  Math.random = function random() {
    return Random.next();
  };

  function polyfillWeakMap() {
    if (typeof jsGlobal.WeakMap === 'function') {
      return;
    }
    var id = 0;
    function WeakMap() {
      this.id = '$weakmap' + (id++);
    }
    ;
    WeakMap.prototype = {
      has: function (obj) {
        return obj.hasOwnProperty(this.id);
      },
      get: function (obj, defaultValue) {
        return obj.hasOwnProperty(this.id) ? obj[this.id] : defaultValue;
      },
      set: function (obj, value) {
        Object.defineProperty(obj, this.id, {
          value: value,
          enumerable: false,
          configurable: true
        });
      }
    };
    jsGlobal.WeakMap = WeakMap;
  }

  polyfillWeakMap();

  var useReferenceCounting = true;

  var WeakList = (function () {
    function WeakList() {
      if (typeof netscape !== "undefined" && netscape.security.PrivilegeManager) {
        this._map = new WeakMap();
      } else {
        this._list = [];
      }
    }
    WeakList.prototype.clear = function () {
      if (this._map) {
        this._map.clear();
      } else {
        this._list.length = 0;
      }
    };
    WeakList.prototype.push = function (value) {
      if (this._map) {
        this._map.set(value, null);
      } else {
        this._list.push(value);
      }
    };
    WeakList.prototype.forEach = function (callback) {
      if (this._map) {
        if (typeof netscape !== "undefined") {
          netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
        }
        Components.utils.nondeterministicGetWeakMapKeys(this._map).forEach(function (value) {
          if (value._referenceCount !== 0) {
            callback(value);
          }
        });
        return;
      }
      var list = this._list;
      var zeroCount = 0;
      for (var i = 0; i < list.length; i++) {
        var value = list[i];
        if (useReferenceCounting && value._referenceCount === 0) {
          zeroCount++;
        } else {
          callback(value);
        }
      }
      if (zeroCount > 16 && zeroCount > (list.length >> 2)) {
        var newList = [];
        for (var i = 0; i < list.length; i++) {
          if (list[i]._referenceCount > 0) {
            newList.push(list[i]);
          }
        }
        this._list = newList;
      }
    };
    Object.defineProperty(WeakList.prototype, "length", {
      get: function () {
        if (this._map) {
          return -1;
        } else {
          return this._list.length;
        }
      },
      enumerable: true,
      configurable: true
    });
    return WeakList;
  })();
  Shumway.WeakList = WeakList;

  (function (NumberUtilities) {
    function pow2(exponent) {
      if (exponent === (exponent | 0)) {
        if (exponent < 0) {
          return 1 / (1 << -exponent);
        }
        return 1 << exponent;
      }
      return Math.pow(2, exponent);
    }
    NumberUtilities.pow2 = pow2;

    function clamp(value, min, max) {
      return Math.max(min, Math.min(max, value));
    }
    NumberUtilities.clamp = clamp;

    function roundHalfEven(value) {
      if (Math.abs(value % 1) === 0.5) {
        var floor = Math.floor(value);
        return floor % 2 === 0 ? floor : Math.ceil(value);
      }
      return Math.round(value);
    }
    NumberUtilities.roundHalfEven = roundHalfEven;

    function epsilonEquals(value, other) {
      return Math.abs(value - other) < 0.0000001;
    }
    NumberUtilities.epsilonEquals = epsilonEquals;
  })(Shumway.NumberUtilities || (Shumway.NumberUtilities = {}));
  var NumberUtilities = Shumway.NumberUtilities;

  (function (Numbers) {
    Numbers[Numbers["MaxU16"] = 0xFFFF] = "MaxU16";
    Numbers[Numbers["MaxI16"] = 0x7FFF] = "MaxI16";
    Numbers[Numbers["MinI16"] = -0x8000] = "MinI16";
  })(Shumway.Numbers || (Shumway.Numbers = {}));
  var Numbers = Shumway.Numbers;

  (function (IntegerUtilities) {
    var sharedBuffer = new ArrayBuffer(8);
    var i8 = new Int8Array(sharedBuffer);
    var i32 = new Int32Array(sharedBuffer);
    var f32 = new Float32Array(sharedBuffer);
    var f64 = new Float64Array(sharedBuffer);
    var nativeLittleEndian = new Int8Array(new Int32Array([1]).buffer)[0] === 1;

    function floatToInt32(v) {
      f32[0] = v;
      return i32[0];
    }
    IntegerUtilities.floatToInt32 = floatToInt32;

    function int32ToFloat(i) {
      i32[0] = i;
      return f32[0];
    }
    IntegerUtilities.int32ToFloat = int32ToFloat;

    function swap16(i) {
      return ((i & 0xFF) << 8) | ((i >> 8) & 0xFF);
    }
    IntegerUtilities.swap16 = swap16;

    function swap32(i) {
      return ((i & 0xFF) << 24) | ((i & 0xFF00) << 8) | ((i >> 8) & 0xFF00) | ((i >> 24) & 0xFF);
    }
    IntegerUtilities.swap32 = swap32;

    function toS8U8(v) {
      return ((v * 256) << 16) >> 16;
    }
    IntegerUtilities.toS8U8 = toS8U8;

    function fromS8U8(i) {
      return i / 256;
    }
    IntegerUtilities.fromS8U8 = fromS8U8;

    function clampS8U8(v) {
      return fromS8U8(toS8U8(v));
    }
    IntegerUtilities.clampS8U8 = clampS8U8;

    function toS16(v) {
      return (v << 16) >> 16;
    }
    IntegerUtilities.toS16 = toS16;

    function bitCount(i) {
      i = i - ((i >> 1) & 0x55555555);
      i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
      return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
    }
    IntegerUtilities.bitCount = bitCount;

    function ones(i) {
      i = i - ((i >> 1) & 0x55555555);
      i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
      return ((i + (i >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
    }
    IntegerUtilities.ones = ones;

    function trailingZeros(i) {
      return IntegerUtilities.ones((i & -i) - 1);
    }
    IntegerUtilities.trailingZeros = trailingZeros;

    function getFlags(i, flags) {
      var str = "";
      for (var i = 0; i < flags.length; i++) {
        if (i & (1 << i)) {
          str += flags[i] + " ";
        }
      }
      if (str.length === 0) {
        return "";
      }
      return str.trim();
    }
    IntegerUtilities.getFlags = getFlags;

    function isPowerOfTwo(x) {
      return x && ((x & (x - 1)) === 0);
    }
    IntegerUtilities.isPowerOfTwo = isPowerOfTwo;

    function roundToMultipleOfFour(x) {
      return (x + 3) & ~0x3;
    }
    IntegerUtilities.roundToMultipleOfFour = roundToMultipleOfFour;

    function nearestPowerOfTwo(x) {
      x--;
      x |= x >> 1;
      x |= x >> 2;
      x |= x >> 4;
      x |= x >> 8;
      x |= x >> 16;
      x++;
      return x;
    }
    IntegerUtilities.nearestPowerOfTwo = nearestPowerOfTwo;

    function roundToMultipleOfPowerOfTwo(i, powerOfTwo) {
      var x = (1 << powerOfTwo) - 1;
      return (i + x) & ~x;
    }
    IntegerUtilities.roundToMultipleOfPowerOfTwo = roundToMultipleOfPowerOfTwo;

    if (!Math.imul) {
      Math.imul = function imul(a, b) {
        var ah = (a >>> 16) & 0xffff;
        var al = a & 0xffff;
        var bh = (b >>> 16) & 0xffff;
        var bl = b & 0xffff;

        return ((al * bl) + (((ah * bl + al * bh) << 16) >>> 0) | 0);
      };
    }

    if (!Math.clz32) {
      Math.clz32 = function clz32(i) {
        i |= (i >> 1);
        i |= (i >> 2);
        i |= (i >> 4);
        i |= (i >> 8);
        i |= (i >> 16);
        return 32 - IntegerUtilities.ones(i);
      };
    }
  })(Shumway.IntegerUtilities || (Shumway.IntegerUtilities = {}));
  var IntegerUtilities = Shumway.IntegerUtilities;

  (function (GeometricUtilities) {
    function pointInPolygon(x, y, polygon) {
      var crosses = 0;
      var n = polygon.length - 2;
      var p = polygon;

      for (var i = 0; i < n; i += 2) {
        var x0 = p[i + 0];
        var y0 = p[i + 1];
        var x1 = p[i + 2];
        var y1 = p[i + 3];
        if (((y0 <= y) && (y1 > y)) || ((y0 > y) && (y1 <= y))) {
          var t = (y - y0) / (y1 - y0);
          if (x < x0 + t * (x1 - x0)) {
            crosses++;
          }
        }
      }
      return (crosses & 1) === 1;
    }
    GeometricUtilities.pointInPolygon = pointInPolygon;

    function signedArea(x0, y0, x1, y1, x2, y2) {
      return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    }
    GeometricUtilities.signedArea = signedArea;

    function counterClockwise(x0, y0, x1, y1, x2, y2) {
      return signedArea(x0, y0, x1, y1, x2, y2) > 0;
    }
    GeometricUtilities.counterClockwise = counterClockwise;

    function clockwise(x0, y0, x1, y1, x2, y2) {
      return signedArea(x0, y0, x1, y1, x2, y2) < 0;
    }
    GeometricUtilities.clockwise = clockwise;

    function pointInPolygonInt32(x, y, polygon) {
      x = x | 0;
      y = y | 0;
      var crosses = 0;
      var n = polygon.length - 2;
      var p = polygon;

      for (var i = 0; i < n; i += 2) {
        var x0 = p[i + 0];
        var y0 = p[i + 1];
        var x1 = p[i + 2];
        var y1 = p[i + 3];
        if (((y0 <= y) && (y1 > y)) || ((y0 > y) && (y1 <= y))) {
          var t = (y - y0) / (y1 - y0);
          if (x < x0 + t * (x1 - x0)) {
            crosses++;
          }
        }
      }
      return (crosses & 1) === 1;
    }
    GeometricUtilities.pointInPolygonInt32 = pointInPolygonInt32;
  })(Shumway.GeometricUtilities || (Shumway.GeometricUtilities = {}));
  var GeometricUtilities = Shumway.GeometricUtilities;

  (function (LogLevel) {
    LogLevel[LogLevel["Error"] = 0x1] = "Error";
    LogLevel[LogLevel["Warn"] = 0x2] = "Warn";
    LogLevel[LogLevel["Debug"] = 0x4] = "Debug";
    LogLevel[LogLevel["Log"] = 0x8] = "Log";
    LogLevel[LogLevel["Info"] = 0x10] = "Info";
    LogLevel[LogLevel["All"] = 0x1f] = "All";
  })(Shumway.LogLevel || (Shumway.LogLevel = {}));
  var LogLevel = Shumway.LogLevel;

  var IndentingWriter = (function () {
    function IndentingWriter(suppressOutput, out) {
      if (typeof suppressOutput === "undefined") { suppressOutput = false; }
      this._tab = "  ";
      this._padding = "";
      this._suppressOutput = suppressOutput;
      this._out = out || IndentingWriter._consoleOut;
      this._outNoNewline = out || IndentingWriter._consoleOutNoNewline;
    }
    IndentingWriter.prototype.write = function (str, writePadding) {
      if (typeof str === "undefined") { str = ""; }
      if (typeof writePadding === "undefined") { writePadding = false; }
      if (!this._suppressOutput) {
        this._outNoNewline((writePadding ? this._padding : "") + str);
      }
    };

    IndentingWriter.prototype.writeLn = function (str) {
      if (typeof str === "undefined") { str = ""; }
      if (!this._suppressOutput) {
        this._out(this._padding + str);
      }
    };

    IndentingWriter.prototype.writeComment = function (str) {
      var lines = str.split("\n");
      if (lines.length === 1) {
        this.writeLn("// " + lines[0]);
      } else {
        this.writeLn("/**");
        for (var i = 0; i < lines.length; i++) {
          this.writeLn(" * " + lines[i]);
        }
        this.writeLn(" */");
      }
    };

    IndentingWriter.prototype.writeLns = function (str) {
      var lines = str.split("\n");
      for (var i = 0; i < lines.length; i++) {
        this.writeLn(lines[i]);
      }
    };

    IndentingWriter.prototype.errorLn = function (str) {
      if (IndentingWriter.logLevel & 1 /* Error */) {
        this.boldRedLn(str);
      }
    };

    IndentingWriter.prototype.warnLn = function (str) {
      if (IndentingWriter.logLevel & 2 /* Warn */) {
        this.yellowLn(str);
      }
    };

    IndentingWriter.prototype.debugLn = function (str) {
      if (IndentingWriter.logLevel & 4 /* Debug */) {
        this.purpleLn(str);
      }
    };

    IndentingWriter.prototype.logLn = function (str) {
      if (IndentingWriter.logLevel & 8 /* Log */) {
        this.writeLn(str);
      }
    };

    IndentingWriter.prototype.infoLn = function (str) {
      if (IndentingWriter.logLevel & 16 /* Info */) {
        this.writeLn(str);
      }
    };

    IndentingWriter.prototype.yellowLn = function (str) {
      this.colorLn(IndentingWriter.YELLOW, str);
    };

    IndentingWriter.prototype.greenLn = function (str) {
      this.colorLn(IndentingWriter.GREEN, str);
    };

    IndentingWriter.prototype.boldRedLn = function (str) {
      this.colorLn(IndentingWriter.BOLD_RED, str);
    };

    IndentingWriter.prototype.redLn = function (str) {
      this.colorLn(IndentingWriter.RED, str);
    };

    IndentingWriter.prototype.purpleLn = function (str) {
      this.colorLn(IndentingWriter.PURPLE, str);
    };

    IndentingWriter.prototype.colorLn = function (color, str) {
      if (!this._suppressOutput) {
        if (!inBrowser) {
          this._out(this._padding + color + str + IndentingWriter.ENDC);
        } else {
          this._out(this._padding + str);
        }
      }
    };

    IndentingWriter.prototype.redLns = function (str) {
      this.colorLns(IndentingWriter.RED, str);
    };

    IndentingWriter.prototype.colorLns = function (color, str) {
      var lines = str.split("\n");
      for (var i = 0; i < lines.length; i++) {
        this.colorLn(color, lines[i]);
      }
    };

    IndentingWriter.prototype.enter = function (str) {
      if (!this._suppressOutput) {
        this._out(this._padding + str);
      }
      this.indent();
    };

    IndentingWriter.prototype.leaveAndEnter = function (str) {
      this.leave(str);
      this.indent();
    };

    IndentingWriter.prototype.leave = function (str) {
      this.outdent();
      if (!this._suppressOutput) {
        this._out(this._padding + str);
      }
    };

    IndentingWriter.prototype.indent = function () {
      this._padding += this._tab;
    };

    IndentingWriter.prototype.outdent = function () {
      if (this._padding.length > 0) {
        this._padding = this._padding.substring(0, this._padding.length - this._tab.length);
      }
    };

    IndentingWriter.prototype.writeArray = function (arr, detailed, noNumbers) {
      if (typeof detailed === "undefined") { detailed = false; }
      if (typeof noNumbers === "undefined") { noNumbers = false; }
      detailed = detailed || false;
      for (var i = 0, j = arr.length; i < j; i++) {
        var prefix = "";
        if (detailed) {
          if (arr[i] === null) {
            prefix = "null";
          } else if (arr[i] === undefined) {
            prefix = "undefined";
          } else {
            prefix = arr[i].constructor.name;
          }
          prefix += " ";
        }
        var number = noNumbers ? "" : ("" + i).padRight(' ', 4);
        this.writeLn(number + prefix + arr[i]);
      }
    };
    IndentingWriter.PURPLE = '\033[94m';
    IndentingWriter.YELLOW = '\033[93m';
    IndentingWriter.GREEN = '\033[92m';
    IndentingWriter.RED = '\033[91m';
    IndentingWriter.BOLD_RED = '\033[1;91m';
    IndentingWriter.ENDC = '\033[0m';

    IndentingWriter.logLevel = 31 /* All */;

    IndentingWriter._consoleOut = inBrowser ? console.info.bind(console) : print;
    IndentingWriter._consoleOutNoNewline = inBrowser ? console.info.bind(console) : putstr;
    return IndentingWriter;
  })();
  Shumway.IndentingWriter = IndentingWriter;

  var SortedListNode = (function () {
    function SortedListNode(value, next) {
      this.value = value;
      this.next = next;
    }
    return SortedListNode;
  })();

  var SortedList = (function () {
    function SortedList(compare) {
      release || Debug.assert(compare);
      this._compare = compare;
      this._head = null;
      this._length = 0;
    }
    SortedList.prototype.push = function (value) {
      release || Debug.assert(value !== undefined);
      this._length++;
      if (!this._head) {
        this._head = new SortedListNode(value, null);
        return;
      }

      var curr = this._head;
      var prev = null;
      var node = new SortedListNode(value, null);
      var compare = this._compare;
      while (curr) {
        if (compare(curr.value, node.value) > 0) {
          if (prev) {
            node.next = curr;
            prev.next = node;
          } else {
            node.next = this._head;
            this._head = node;
          }
          return;
        }
        prev = curr;
        curr = curr.next;
      }
      prev.next = node;
    };

    SortedList.prototype.forEach = function (visitor) {
      var curr = this._head;
      var last = null;
      while (curr) {
        var result = visitor(curr.value);
        if (result === SortedList.RETURN) {
          return;
        } else if (result === SortedList.DELETE) {
          if (!last) {
            curr = this._head = this._head.next;
          } else {
            curr = last.next = curr.next;
          }
        } else {
          last = curr;
          curr = curr.next;
        }
      }
    };

    SortedList.prototype.isEmpty = function () {
      return !this._head;
    };

    SortedList.prototype.pop = function () {
      if (!this._head) {
        return undefined;
      }
      this._length--;
      var ret = this._head;
      this._head = this._head.next;
      return ret.value;
    };

    SortedList.prototype.contains = function (value) {
      var curr = this._head;
      while (curr) {
        if (curr.value === value) {
          return true;
        }
        curr = curr.next;
      }
      return false;
    };

    SortedList.prototype.toString = function () {
      var str = "[";
      var curr = this._head;
      while (curr) {
        str += curr.value.toString();
        curr = curr.next;
        if (curr) {
          str += ",";
        }
      }
      str += "]";
      return str;
    };
    SortedList.RETURN = 1;
    SortedList.DELETE = 2;
    return SortedList;
  })();
  Shumway.SortedList = SortedList;

  var CircularBuffer = (function () {
    function CircularBuffer(Type, sizeInBits) {
      if (typeof sizeInBits === "undefined") { sizeInBits = 12; }
      this.index = 0;
      this.start = 0;
      this._size = 1 << sizeInBits;
      this._mask = this._size - 1;
      this.array = new Type(this._size);
    }
    CircularBuffer.prototype.get = function (i) {
      return this.array[i];
    };

    CircularBuffer.prototype.forEachInReverse = function (visitor) {
      if (this.isEmpty()) {
        return;
      }
      var i = this.index === 0 ? this._size - 1 : this.index - 1;
      var end = (this.start - 1) & this._mask;
      while (i !== end) {
        if (visitor(this.array[i], i)) {
          break;
        }
        i = i === 0 ? this._size - 1 : i - 1;
      }
    };

    CircularBuffer.prototype.write = function (value) {
      this.array[this.index] = value;
      this.index = (this.index + 1) & this._mask;
      if (this.index === this.start) {
        this.start = (this.start + 1) & this._mask;
      }
    };

    CircularBuffer.prototype.isFull = function () {
      return ((this.index + 1) & this._mask) === this.start;
    };

    CircularBuffer.prototype.isEmpty = function () {
      return this.index === this.start;
    };

    CircularBuffer.prototype.reset = function () {
      this.index = 0;
      this.start = 0;
    };
    return CircularBuffer;
  })();
  Shumway.CircularBuffer = CircularBuffer;

  (function (BitSets) {
    var assert = Shumway.Debug.assert;

    BitSets.ADDRESS_BITS_PER_WORD = 5;
    BitSets.BITS_PER_WORD = 1 << BitSets.ADDRESS_BITS_PER_WORD;
    BitSets.BIT_INDEX_MASK = BitSets.BITS_PER_WORD - 1;

    function getSize(length) {
      return ((length + (BitSets.BITS_PER_WORD - 1)) >> BitSets.ADDRESS_BITS_PER_WORD) << BitSets.ADDRESS_BITS_PER_WORD;
    }

    function toBitString(on, off) {
      var self = this;
      on = on || "1";
      off = off || "0";
      var str = "";
      for (var i = 0; i < length; i++) {
        str += self.get(i) ? on : off;
      }
      return str;
    }

    function toString(names) {
      var self = this;
      var set = [];
      for (var i = 0; i < length; i++) {
        if (self.get(i)) {
          set.push(names ? names[i] : i);
        }
      }
      return set.join(", ");
    }

    var Uint32ArrayBitSet = (function () {
      function Uint32ArrayBitSet(length) {
        this.size = getSize(length);
        this.count = 0;
        this.dirty = 0;
        this.length = length;
        this.bits = new Uint32Array(this.size >> BitSets.ADDRESS_BITS_PER_WORD);
      }
      Uint32ArrayBitSet.prototype.recount = function () {
        if (!this.dirty) {
          return;
        }

        var bits = this.bits;
        var c = 0;
        for (var i = 0, j = bits.length; i < j; i++) {
          var v = bits[i];
          v = v - ((v >> 1) & 0x55555555);
          v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
          c += ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
        }

        this.count = c;
        this.dirty = 0;
      };

      Uint32ArrayBitSet.prototype.set = function (i) {
        var n = i >> BitSets.ADDRESS_BITS_PER_WORD;
        var old = this.bits[n];
        var b = old | (1 << (i & BitSets.BIT_INDEX_MASK));
        this.bits[n] = b;
        this.dirty |= old ^ b;
      };

      Uint32ArrayBitSet.prototype.setAll = function () {
        var bits = this.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          bits[i] = 0xFFFFFFFF;
        }
        this.count = this.size;
        this.dirty = 0;
      };

      Uint32ArrayBitSet.prototype.assign = function (set) {
        this.count = set.count;
        this.dirty = set.dirty;
        this.size = set.size;
        for (var i = 0, j = this.bits.length; i < j; i++) {
          this.bits[i] = set.bits[i];
        }
      };

      Uint32ArrayBitSet.prototype.clear = function (i) {
        var n = i >> BitSets.ADDRESS_BITS_PER_WORD;
        var old = this.bits[n];
        var b = old & ~(1 << (i & BitSets.BIT_INDEX_MASK));
        this.bits[n] = b;
        this.dirty |= old ^ b;
      };

      Uint32ArrayBitSet.prototype.get = function (i) {
        var word = this.bits[i >> BitSets.ADDRESS_BITS_PER_WORD];
        return ((word & 1 << (i & BitSets.BIT_INDEX_MASK))) !== 0;
      };

      Uint32ArrayBitSet.prototype.clearAll = function () {
        var bits = this.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          bits[i] = 0;
        }
        this.count = 0;
        this.dirty = 0;
      };

      Uint32ArrayBitSet.prototype._union = function (other) {
        var dirty = this.dirty;
        var bits = this.bits;
        var otherBits = other.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          var old = bits[i];
          var b = old | otherBits[i];
          bits[i] = b;
          dirty |= old ^ b;
        }
        this.dirty = dirty;
      };

      Uint32ArrayBitSet.prototype.intersect = function (other) {
        var dirty = this.dirty;
        var bits = this.bits;
        var otherBits = other.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          var old = bits[i];
          var b = old & otherBits[i];
          bits[i] = b;
          dirty |= old ^ b;
        }
        this.dirty = dirty;
      };

      Uint32ArrayBitSet.prototype.subtract = function (other) {
        var dirty = this.dirty;
        var bits = this.bits;
        var otherBits = other.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          var old = bits[i];
          var b = old & ~otherBits[i];
          bits[i] = b;
          dirty |= old ^ b;
        }
        this.dirty = dirty;
      };

      Uint32ArrayBitSet.prototype.negate = function () {
        var dirty = this.dirty;
        var bits = this.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          var old = bits[i];
          var b = ~old;
          bits[i] = b;
          dirty |= old ^ b;
        }
        this.dirty = dirty;
      };

      Uint32ArrayBitSet.prototype.forEach = function (fn) {
        release || assert(fn);
        var bits = this.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          var word = bits[i];
          if (word) {
            for (var k = 0; k < BitSets.BITS_PER_WORD; k++) {
              if (word & (1 << k)) {
                fn(i * BitSets.BITS_PER_WORD + k);
              }
            }
          }
        }
      };

      Uint32ArrayBitSet.prototype.toArray = function () {
        var set = [];
        var bits = this.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          var word = bits[i];
          if (word) {
            for (var k = 0; k < BitSets.BITS_PER_WORD; k++) {
              if (word & (1 << k)) {
                set.push(i * BitSets.BITS_PER_WORD + k);
              }
            }
          }
        }
        return set;
      };

      Uint32ArrayBitSet.prototype.equals = function (other) {
        if (this.size !== other.size) {
          return false;
        }
        var bits = this.bits;
        var otherBits = other.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          if (bits[i] !== otherBits[i]) {
            return false;
          }
        }
        return true;
      };

      Uint32ArrayBitSet.prototype.contains = function (other) {
        if (this.size !== other.size) {
          return false;
        }
        var bits = this.bits;
        var otherBits = other.bits;
        for (var i = 0, j = bits.length; i < j; i++) {
          if ((bits[i] | otherBits[i]) !== bits[i]) {
            return false;
          }
        }
        return true;
      };

      Uint32ArrayBitSet.prototype.isEmpty = function () {
        this.recount();
        return this.count === 0;
      };

      Uint32ArrayBitSet.prototype.clone = function () {
        var set = new Uint32ArrayBitSet(this.length);
        set._union(this);
        return set;
      };
      return Uint32ArrayBitSet;
    })();
    BitSets.Uint32ArrayBitSet = Uint32ArrayBitSet;

    var Uint32BitSet = (function () {
      function Uint32BitSet(length) {
        this.count = 0;
        this.dirty = 0;
        this.size = getSize(length);
        this.bits = 0;
        this.singleWord = true;
        this.length = length;
      }
      Uint32BitSet.prototype.recount = function () {
        if (!this.dirty) {
          return;
        }

        var c = 0;
        var v = this.bits;
        v = v - ((v >> 1) & 0x55555555);
        v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
        c += ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;

        this.count = c;
        this.dirty = 0;
      };

      Uint32BitSet.prototype.set = function (i) {
        var old = this.bits;
        var b = old | (1 << (i & BitSets.BIT_INDEX_MASK));
        this.bits = b;
        this.dirty |= old ^ b;
      };

      Uint32BitSet.prototype.setAll = function () {
        this.bits = 0xFFFFFFFF;
        this.count = this.size;
        this.dirty = 0;
      };

      Uint32BitSet.prototype.assign = function (set) {
        this.count = set.count;
        this.dirty = set.dirty;
        this.size = set.size;
        this.bits = set.bits;
      };

      Uint32BitSet.prototype.clear = function (i) {
        var old = this.bits;
        var b = old & ~(1 << (i & BitSets.BIT_INDEX_MASK));
        this.bits = b;
        this.dirty |= old ^ b;
      };

      Uint32BitSet.prototype.get = function (i) {
        return ((this.bits & 1 << (i & BitSets.BIT_INDEX_MASK))) !== 0;
      };

      Uint32BitSet.prototype.clearAll = function () {
        this.bits = 0;
        this.count = 0;
        this.dirty = 0;
      };

      Uint32BitSet.prototype._union = function (other) {
        var old = this.bits;
        var b = old | other.bits;
        this.bits = b;
        this.dirty = old ^ b;
      };

      Uint32BitSet.prototype.intersect = function (other) {
        var old = this.bits;
        var b = old & other.bits;
        this.bits = b;
        this.dirty = old ^ b;
      };

      Uint32BitSet.prototype.subtract = function (other) {
        var old = this.bits;
        var b = old & ~other.bits;
        this.bits = b;
        this.dirty = old ^ b;
      };

      Uint32BitSet.prototype.negate = function () {
        var old = this.bits;
        var b = ~old;
        this.bits = b;
        this.dirty = old ^ b;
      };

      Uint32BitSet.prototype.forEach = function (fn) {
        release || assert(fn);
        var word = this.bits;
        if (word) {
          for (var k = 0; k < BitSets.BITS_PER_WORD; k++) {
            if (word & (1 << k)) {
              fn(k);
            }
          }
        }
      };

      Uint32BitSet.prototype.toArray = function () {
        var set = [];
        var word = this.bits;
        if (word) {
          for (var k = 0; k < BitSets.BITS_PER_WORD; k++) {
            if (word & (1 << k)) {
              set.push(k);
            }
          }
        }
        return set;
      };

      Uint32BitSet.prototype.equals = function (other) {
        return this.bits === other.bits;
      };

      Uint32BitSet.prototype.contains = function (other) {
        var bits = this.bits;
        return (bits | other.bits) === bits;
      };

      Uint32BitSet.prototype.isEmpty = function () {
        this.recount();
        return this.count === 0;
      };

      Uint32BitSet.prototype.clone = function () {
        var set = new Uint32BitSet(this.length);
        set._union(this);
        return set;
      };
      return Uint32BitSet;
    })();
    BitSets.Uint32BitSet = Uint32BitSet;

    Uint32BitSet.prototype.toString = toString;
    Uint32BitSet.prototype.toBitString = toBitString;
    Uint32ArrayBitSet.prototype.toString = toString;
    Uint32ArrayBitSet.prototype.toBitString = toBitString;

    function BitSetFunctor(length) {
      var shouldUseSingleWord = (getSize(length) >> BitSets.ADDRESS_BITS_PER_WORD) === 1;
      var type = (shouldUseSingleWord ? Uint32BitSet : Uint32ArrayBitSet);
      return function () {
        return new type(length);
      };
    }
    BitSets.BitSetFunctor = BitSetFunctor;
  })(Shumway.BitSets || (Shumway.BitSets = {}));
  var BitSets = Shumway.BitSets;

  var ColorStyle = (function () {
    function ColorStyle() {
    }
    ColorStyle.randomStyle = function () {
      if (!ColorStyle._randomStyleCache) {
        ColorStyle._randomStyleCache = [
          "#ff5e3a",
          "#ff9500",
          "#ffdb4c",
          "#87fc70",
          "#52edc7",
          "#1ad6fd",
          "#c644fc",
          "#ef4db6",
          "#4a4a4a",
          "#dbddde",
          "#ff3b30",
          "#ff9500",
          "#ffcc00",
          "#4cd964",
          "#34aadc",
          "#007aff",
          "#5856d6",
          "#ff2d55",
          "#8e8e93",
          "#c7c7cc",
          "#5ad427",
          "#c86edf",
          "#d1eefc",
          "#e0f8d8",
          "#fb2b69",
          "#f7f7f7",
          "#1d77ef",
          "#d6cec3",
          "#55efcb",
          "#ff4981",
          "#ffd3e0",
          "#f7f7f7",
          "#ff1300",
          "#1f1f21",
          "#bdbec2",
          "#ff3a2d"
        ];
      }
      return ColorStyle._randomStyleCache[(ColorStyle._nextStyle++) % ColorStyle._randomStyleCache.length];
    };

    ColorStyle.contrastStyle = function (rgb) {
      var c = parseInt(rgb.substr(1), 16);
      var yiq = (((c >> 16) * 299) + (((c >> 8) & 0xff) * 587) + ((c & 0xff) * 114)) / 1000;
      return (yiq >= 128) ? '#000000' : '#ffffff';
    };

    ColorStyle.reset = function () {
      ColorStyle._nextStyle = 0;
    };
    ColorStyle.TabToolbar = "#252c33";
    ColorStyle.Toolbars = "#343c45";
    ColorStyle.HighlightBlue = "#1d4f73";
    ColorStyle.LightText = "#f5f7fa";
    ColorStyle.ForegroundText = "#b6babf";
    ColorStyle.Black = "#000000";
    ColorStyle.VeryDark = "#14171a";
    ColorStyle.Dark = "#181d20";
    ColorStyle.Light = "#a9bacb";
    ColorStyle.Grey = "#8fa1b2";
    ColorStyle.DarkGrey = "#5f7387";
    ColorStyle.Blue = "#46afe3";
    ColorStyle.Purple = "#6b7abb";
    ColorStyle.Pink = "#df80ff";
    ColorStyle.Red = "#eb5368";
    ColorStyle.Orange = "#d96629";
    ColorStyle.LightOrange = "#d99b28";
    ColorStyle.Green = "#70bf53";
    ColorStyle.BlueGrey = "#5e88b0";

    ColorStyle._nextStyle = 0;
    return ColorStyle;
  })();
  Shumway.ColorStyle = ColorStyle;

  var Bounds = (function () {
    function Bounds(xMin, yMin, xMax, yMax) {
      this.xMin = xMin | 0;
      this.yMin = yMin | 0;
      this.xMax = xMax | 0;
      this.yMax = yMax | 0;
    }
    Bounds.FromUntyped = function (source) {
      return new Bounds(source.xMin, source.yMin, source.xMax, source.yMax);
    };

    Bounds.FromRectangle = function (source) {
      return new Bounds(source.x * 20 | 0, source.y * 20 | 0, (source.x + source.width) * 20 | 0, (source.y + source.height) * 20 | 0);
    };

    Bounds.prototype.setElements = function (xMin, yMin, xMax, yMax) {
      this.xMin = xMin;
      this.yMin = yMin;
      this.xMax = xMax;
      this.yMax = yMax;
    };

    Bounds.prototype.copyFrom = function (source) {
      this.setElements(source.xMin, source.yMin, source.xMax, source.yMax);
    };

    Bounds.prototype.contains = function (x, y) {
      return x < this.xMin !== x < this.xMax && y < this.yMin !== y < this.yMax;
    };

    Bounds.prototype.unionInPlace = function (other) {
      this.xMin = Math.min(this.xMin, other.xMin);
      this.yMin = Math.min(this.yMin, other.yMin);
      this.xMax = Math.max(this.xMax, other.xMax);
      this.yMax = Math.max(this.yMax, other.yMax);
    };

    Bounds.prototype.extendByPoint = function (x, y) {
      this.extendByX(x);
      this.extendByY(y);
    };

    Bounds.prototype.extendByX = function (x) {
      if (this.xMin === 0x8000000) {
        this.xMin = this.xMax = x;
        return;
      }
      this.xMin = Math.min(this.xMin, x);
      this.xMax = Math.max(this.xMax, x);
    };

    Bounds.prototype.extendByY = function (y) {
      if (this.yMin === 0x8000000) {
        this.yMin = this.yMax = y;
        return;
      }
      this.yMin = Math.min(this.yMin, y);
      this.yMax = Math.max(this.yMax, y);
    };

    Bounds.prototype.intersects = function (toIntersect) {
      return this.contains(toIntersect.xMin, toIntersect.yMin) || this.contains(toIntersect.xMax, toIntersect.yMax);
    };

    Bounds.prototype.isEmpty = function () {
      return this.xMax <= this.xMin || this.yMax <= this.yMin;
    };

    Object.defineProperty(Bounds.prototype, "width", {
      get: function () {
        return this.xMax - this.xMin;
      },
      set: function (value) {
        this.xMax = this.xMin + value;
      },
      enumerable: true,
      configurable: true
    });


    Object.defineProperty(Bounds.prototype, "height", {
      get: function () {
        return this.yMax - this.yMin;
      },
      set: function (value) {
        this.yMax = this.yMin + value;
      },
      enumerable: true,
      configurable: true
    });


    Bounds.prototype.getBaseWidth = function (angle) {
      var u = Math.abs(Math.cos(angle));
      var v = Math.abs(Math.sin(angle));
      return u * (this.xMax - this.xMin) + v * (this.yMax - this.yMin);
    };

    Bounds.prototype.getBaseHeight = function (angle) {
      var u = Math.abs(Math.cos(angle));
      var v = Math.abs(Math.sin(angle));
      return v * (this.xMax - this.xMin) + u * (this.yMax - this.yMin);
    };

    Bounds.prototype.setEmpty = function () {
      this.xMin = this.yMin = this.xMax = this.yMax = 0;
    };

    Bounds.prototype.setToSentinels = function () {
      this.xMin = this.yMin = this.xMax = this.yMax = 0x8000000;
    };

    Bounds.prototype.clone = function () {
      return new Bounds(this.xMin, this.yMin, this.xMax, this.yMax);
    };

    Bounds.prototype.toString = function () {
      return "{ " + "xMin: " + this.xMin + ", " + "xMin: " + this.yMin + ", " + "xMax: " + this.xMax + ", " + "xMax: " + this.yMax + " }";
    };
    return Bounds;
  })();
  Shumway.Bounds = Bounds;

  var DebugBounds = (function () {
    function DebugBounds(xMin, yMin, xMax, yMax) {
      Debug.assert(isInteger(xMin));
      Debug.assert(isInteger(yMin));
      Debug.assert(isInteger(xMax));
      Debug.assert(isInteger(yMax));
      this._xMin = xMin | 0;
      this._yMin = yMin | 0;
      this._xMax = xMax | 0;
      this._yMax = yMax | 0;
      this.assertValid();
    }
    DebugBounds.FromUntyped = function (source) {
      return new DebugBounds(source.xMin, source.yMin, source.xMax, source.yMax);
    };

    DebugBounds.FromRectangle = function (source) {
      return new DebugBounds(source.x * 20 | 0, source.y * 20 | 0, (source.x + source.width) * 20 | 0, (source.y + source.height) * 20 | 0);
    };

    DebugBounds.prototype.setElements = function (xMin, yMin, xMax, yMax) {
      this.xMin = xMin;
      this.yMin = yMin;
      this.xMax = xMax;
      this.yMax = yMax;
    };

    DebugBounds.prototype.copyFrom = function (source) {
      this.setElements(source.xMin, source.yMin, source.xMax, source.yMax);
    };

    DebugBounds.prototype.contains = function (x, y) {
      return x < this.xMin !== x < this.xMax && y < this.yMin !== y < this.yMax;
    };

    DebugBounds.prototype.unionWith = function (other) {
      this._xMin = Math.min(this._xMin, other._xMin);
      this._yMin = Math.min(this._yMin, other._yMin);
      this._xMax = Math.max(this._xMax, other._xMax);
      this._yMax = Math.max(this._yMax, other._yMax);
    };

    DebugBounds.prototype.extendByPoint = function (x, y) {
      this.extendByX(x);
      this.extendByY(y);
    };

    DebugBounds.prototype.extendByX = function (x) {
      if (this.xMin === 0x8000000) {
        this.xMin = this.xMax = x;
        return;
      }
      this.xMin = Math.min(this.xMin, x);
      this.xMax = Math.max(this.xMax, x);
    };

    DebugBounds.prototype.extendByY = function (y) {
      if (this.yMin === 0x8000000) {
        this.yMin = this.yMax = y;
        return;
      }
      this.yMin = Math.min(this.yMin, y);
      this.yMax = Math.max(this.yMax, y);
    };

    DebugBounds.prototype.intersects = function (toIntersect) {
      return this.contains(toIntersect._xMin, toIntersect._yMin) || this.contains(toIntersect._xMax, toIntersect._yMax);
    };

    DebugBounds.prototype.isEmpty = function () {
      return this._xMax <= this._xMin || this._yMax <= this._yMin;
    };


    Object.defineProperty(DebugBounds.prototype, "xMin", {
      get: function () {
        return this._xMin;
      },
      set: function (value) {
        Debug.assert(isInteger(value));
        this._xMin = value;
        this.assertValid();
      },
      enumerable: true,
      configurable: true
    });


    Object.defineProperty(DebugBounds.prototype, "yMin", {
      get: function () {
        return this._yMin;
      },
      set: function (value) {
        Debug.assert(isInteger(value));
        this._yMin = value | 0;
        this.assertValid();
      },
      enumerable: true,
      configurable: true
    });


    Object.defineProperty(DebugBounds.prototype, "xMax", {
      get: function () {
        return this._xMax;
      },
      set: function (value) {
        Debug.assert(isInteger(value));
        this._xMax = value | 0;
        this.assertValid();
      },
      enumerable: true,
      configurable: true
    });

    Object.defineProperty(DebugBounds.prototype, "width", {
      get: function () {
        return this._xMax - this._xMin;
      },
      enumerable: true,
      configurable: true
    });


    Object.defineProperty(DebugBounds.prototype, "yMax", {
      get: function () {
        return this._yMax;
      },
      set: function (value) {
        Debug.assert(isInteger(value));
        this._yMax = value | 0;
        this.assertValid();
      },
      enumerable: true,
      configurable: true
    });

    Object.defineProperty(DebugBounds.prototype, "height", {
      get: function () {
        return this._yMax - this._yMin;
      },
      enumerable: true,
      configurable: true
    });

    DebugBounds.prototype.getBaseWidth = function (angle) {
      var u = Math.abs(Math.cos(angle));
      var v = Math.abs(Math.sin(angle));
      return u * (this._xMax - this._xMin) + v * (this._yMax - this._yMin);
    };

    DebugBounds.prototype.getBaseHeight = function (angle) {
      var u = Math.abs(Math.cos(angle));
      var v = Math.abs(Math.sin(angle));
      return v * (this._xMax - this._xMin) + u * (this._yMax - this._yMin);
    };

    DebugBounds.prototype.setEmpty = function () {
      this._xMin = this._yMin = this._xMax = this._yMax = 0;
    };

    DebugBounds.prototype.clone = function () {
      return new DebugBounds(this.xMin, this.yMin, this.xMax, this.yMax);
    };

    DebugBounds.prototype.toString = function () {
      return "{ " + "xMin: " + this._xMin + ", " + "xMin: " + this._yMin + ", " + "xMax: " + this._xMax + ", " + "xMax: " + this._yMax + " }";
    };

    DebugBounds.prototype.assertValid = function () {
    };
    return DebugBounds;
  })();
  Shumway.DebugBounds = DebugBounds;

  var Color = (function () {
    function Color(r, g, b, a) {
      this.r = r;
      this.g = g;
      this.b = b;
      this.a = a;
    }
    Color.FromARGB = function (argb) {
      return new Color((argb >> 16 & 0xFF) / 255, (argb >> 8 & 0xFF) / 255, (argb >> 0 & 0xFF) / 255, (argb >> 24 & 0xFF) / 255);
    };
    Color.FromRGBA = function (rgba) {
      return Color.FromARGB(ColorUtilities.RGBAToARGB(rgba));
    };
    Color.prototype.toRGBA = function () {
      return (this.r * 255) << 24 | (this.g * 255) << 16 | (this.b * 255) << 8 | (this.a * 255);
    };
    Color.prototype.toCSSStyle = function () {
      return ColorUtilities.rgbaToCSSStyle(this.toRGBA());
    };
    Color.prototype.set = function (other) {
      this.r = other.r;
      this.g = other.g;
      this.b = other.b;
      this.a = other.a;
    };

    Color.randomColor = function (alpha) {
      if (typeof alpha === "undefined") { alpha = 1; }
      return new Color(Math.random(), Math.random(), Math.random(), alpha);
    };
    Color.parseColor = function (color) {
      if (!Color.colorCache) {
        Color.colorCache = Object.create(null);
      }
      if (Color.colorCache[color]) {
        return Color.colorCache[color];
      }

      var span = document.createElement('span');
      document.body.appendChild(span);
      span.style.backgroundColor = color;
      var rgb = getComputedStyle(span).backgroundColor;
      document.body.removeChild(span);
      var m = /^rgb\((\d+), (\d+), (\d+)\)$/.exec(rgb);
      if (!m)
        m = /^rgba\((\d+), (\d+), (\d+), ([\d.]+)\)$/.exec(rgb);
      var result = new Color(0, 0, 0, 0);
      result.r = parseFloat(m[1]) / 255;
      result.g = parseFloat(m[2]) / 255;
      result.b = parseFloat(m[3]) / 255;
      result.a = m[4] ? parseFloat(m[4]) / 255 : 1;
      return Color.colorCache[color] = result;
    };
    Color.Red = new Color(1, 0, 0, 1);
    Color.Green = new Color(0, 1, 0, 1);
    Color.Blue = new Color(0, 0, 1, 1);
    Color.None = new Color(0, 0, 0, 0);
    Color.White = new Color(1, 1, 1, 1);
    Color.Black = new Color(0, 0, 0, 1);
    Color.colorCache = {};
    return Color;
  })();
  Shumway.Color = Color;

  (function (ColorUtilities) {
    function RGBAToARGB(rgba) {
      return ((rgba >> 8) & 0x00ffffff) | ((rgba & 0xff) << 24);
    }
    ColorUtilities.RGBAToARGB = RGBAToARGB;

    function ARGBToRGBA(argb) {
      return argb << 8 | ((argb >> 24) & 0xff);
    }
    ColorUtilities.ARGBToRGBA = ARGBToRGBA;

    function rgbaToCSSStyle(color) {
      return Shumway.StringUtilities.concat9('rgba(', color >> 24 & 0xff, ',', color >> 16 & 0xff, ',', color >> 8 & 0xff, ',', (color & 0xff) / 0xff, ')');
    }
    ColorUtilities.rgbaToCSSStyle = rgbaToCSSStyle;

    function cssStyleToRGBA(style) {
      if (style[0] === "#") {
        if (style.length === 7) {
          var value = parseInt(style.substring(1), 16);
          return (value << 8) | 0xff;
        }
      } else if (style[0] === "r") {
        var values = style.substring(5, style.length - 1).split(",");
        var r = parseInt(values[0]);
        var g = parseInt(values[1]);
        var b = parseInt(values[2]);
        var a = parseFloat(values[3]);
        return (r & 0xff) << 24 | (g & 0xff) << 16 | (b & 0xff) << 8 | ((a * 255) & 0xff);
      }
      return 0xff0000ff;
    }
    ColorUtilities.cssStyleToRGBA = cssStyleToRGBA;

    function hexToRGB(color) {
      return parseInt(color.slice(1), 16);
    }
    ColorUtilities.hexToRGB = hexToRGB;

    function rgbToHex(color) {
      return '#' + ('000000' + color.toString(16)).slice(-6);
    }
    ColorUtilities.rgbToHex = rgbToHex;

    function isValidHexColor(value) {
      return /^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(value);
    }
    ColorUtilities.isValidHexColor = isValidHexColor;

    function clampByte(value) {
      return Math.max(0, Math.min(255, value));
    }
    ColorUtilities.clampByte = clampByte;

    function unpremultiplyARGB(pARGB) {
      var b = (pARGB >> 0) & 0xff;
      var g = (pARGB >> 8) & 0xff;
      var r = (pARGB >> 16) & 0xff;
      var a = (pARGB >> 24) & 0xff;
      r = Math.imul(255, r) / a & 0xff;
      g = Math.imul(255, g) / a & 0xff;
      b = Math.imul(255, b) / a & 0xff;
      return a << 24 | r << 16 | g << 8 | b;
    }
    ColorUtilities.unpremultiplyARGB = unpremultiplyARGB;

    function premultiplyARGB(uARGB) {
      var b = (uARGB >> 0) & 0xff;
      var g = (uARGB >> 8) & 0xff;
      var r = (uARGB >> 16) & 0xff;
      var a = (uARGB >> 24) & 0xff;
      r = ((Math.imul(r, a) + 127) / 255) | 0;
      g = ((Math.imul(g, a) + 127) / 255) | 0;
      b = ((Math.imul(b, a) + 127) / 255) | 0;
      return a << 24 | r << 16 | g << 8 | b;
    }
    ColorUtilities.premultiplyARGB = premultiplyARGB;

    var premultiplyTable;

    var unpremultiplyTable;

    function ensureUnpremultiplyTable() {
      if (!unpremultiplyTable) {
        unpremultiplyTable = new Uint8Array(256 * 256);
        for (var c = 0; c < 256; c++) {
          for (var a = 0; a < 256; a++) {
            unpremultiplyTable[(a << 8) + c] = Math.imul(255, c) / a;
          }
        }
      }
    }
    ColorUtilities.ensureUnpremultiplyTable = ensureUnpremultiplyTable;

    function tableLookupUnpremultiplyARGB(pARGB) {
      pARGB = pARGB | 0;
      var a = (pARGB >> 24) & 0xff;
      if (a === 0) {
        return 0;
      }
      var b = (pARGB >> 0) & 0xff;
      var g = (pARGB >> 8) & 0xff;
      var r = (pARGB >> 16) & 0xff;
      var o = a << 8;
      r = unpremultiplyTable[o + r];
      g = unpremultiplyTable[o + g];
      b = unpremultiplyTable[o + b];
      return a << 24 | r << 16 | g << 8 | b;
    }
    ColorUtilities.tableLookupUnpremultiplyARGB = tableLookupUnpremultiplyARGB;

    var inverseSourceAlphaTable;

    function ensureInverseSourceAlphaTable() {
      if (inverseSourceAlphaTable) {
        return;
      }
      inverseSourceAlphaTable = new Float64Array(256);
      for (var a = 0; a < 255; a++) {
        inverseSourceAlphaTable[a] = 1 - a / 255;
      }
    }
    ColorUtilities.ensureInverseSourceAlphaTable = ensureInverseSourceAlphaTable;

    function blendPremultipliedBGRA(tpBGRA, spBGRA) {
      var ta = (tpBGRA >> 0) & 0xff;
      var tr = (tpBGRA >> 8) & 0xff;
      var tg = (tpBGRA >> 16) & 0xff;
      var tb = (tpBGRA >> 24) & 0xff;

      var sa = (spBGRA >> 0) & 0xff;
      var sr = (spBGRA >> 8) & 0xff;
      var sg = (spBGRA >> 16) & 0xff;
      var sb = (spBGRA >> 24) & 0xff;

      var inverseSourceAlpha = inverseSourceAlphaTable[sa];
      var ta = sa + ta * inverseSourceAlpha;
      var tr = sr + tr * inverseSourceAlpha;
      var tg = sg + tg * inverseSourceAlpha;
      var tb = sb + tb * inverseSourceAlpha;
      return tb << 24 | tg << 16 | tr << 8 | ta;
    }
    ColorUtilities.blendPremultipliedBGRA = blendPremultipliedBGRA;
  })(Shumway.ColorUtilities || (Shumway.ColorUtilities = {}));
  var ColorUtilities = Shumway.ColorUtilities;

  var ArrayBufferPool = (function () {
    function ArrayBufferPool(maxSize) {
      if (typeof maxSize === "undefined") { maxSize = 32; }
      this._list = [];
      this._maxSize = maxSize;
    }
    ArrayBufferPool.prototype.acquire = function (length) {
      if (ArrayBufferPool._enabled) {
        var list = this._list;
        for (var i = 0; i < list.length; i++) {
          var buffer = list[i];
          if (buffer.byteLength >= length) {
            list.splice(i, 1);
            return buffer;
          }
        }
      }
      return new ArrayBuffer(length);
    };

    ArrayBufferPool.prototype.release = function (buffer) {
      if (ArrayBufferPool._enabled) {
        var list = this._list;
        release || Debug.assert(ArrayUtilities.indexOf(list, buffer) < 0);
        if (list.length === this._maxSize) {
          list.shift();
        }
        list.push(buffer);
      }
    };

    ArrayBufferPool.prototype.ensureUint8ArrayLength = function (array, length) {
      if (array.length >= length) {
        return array;
      }
      var newLength = Math.max(array.length + length, ((array.length * 3) >> 1) + 1);
      var newArray = new Uint8Array(this.acquire(newLength), 0, newLength);
      newArray.set(array);
      this.release(array.buffer);
      return newArray;
    };

    ArrayBufferPool.prototype.ensureFloat64ArrayLength = function (array, length) {
      if (array.length >= length) {
        return array;
      }
      var newLength = Math.max(array.length + length, ((array.length * 3) >> 1) + 1);
      var newArray = new Float64Array(this.acquire(newLength * Float64Array.BYTES_PER_ELEMENT), 0, newLength);
      newArray.set(array);
      this.release(array.buffer);
      return newArray;
    };
    ArrayBufferPool._enabled = true;
    return ArrayBufferPool;
  })();
  Shumway.ArrayBufferPool = ArrayBufferPool;

  (function (Telemetry) {
    (function (Feature) {
      Feature[Feature["EXTERNAL_INTERFACE_FEATURE"] = 1] = "EXTERNAL_INTERFACE_FEATURE";
      Feature[Feature["CLIPBOARD_FEATURE"] = 2] = "CLIPBOARD_FEATURE";
      Feature[Feature["SHAREDOBJECT_FEATURE"] = 3] = "SHAREDOBJECT_FEATURE";
      Feature[Feature["VIDEO_FEATURE"] = 4] = "VIDEO_FEATURE";
      Feature[Feature["SOUND_FEATURE"] = 5] = "SOUND_FEATURE";
      Feature[Feature["NETCONNECTION_FEATURE"] = 6] = "NETCONNECTION_FEATURE";
    })(Telemetry.Feature || (Telemetry.Feature = {}));
    var Feature = Telemetry.Feature;

    (function (ErrorTypes) {
      ErrorTypes[ErrorTypes["AVM1_ERROR"] = 1] = "AVM1_ERROR";
      ErrorTypes[ErrorTypes["AVM2_ERROR"] = 2] = "AVM2_ERROR";
    })(Telemetry.ErrorTypes || (Telemetry.ErrorTypes = {}));
    var ErrorTypes = Telemetry.ErrorTypes;

    Telemetry.instance;
  })(Shumway.Telemetry || (Shumway.Telemetry = {}));
  var Telemetry = Shumway.Telemetry;

  (function (FileLoadingService) {
    FileLoadingService.instance;
  })(Shumway.FileLoadingService || (Shumway.FileLoadingService = {}));
  var FileLoadingService = Shumway.FileLoadingService;

  (function (ExternalInterfaceService) {
    ExternalInterfaceService.instance = {
      enabled: false,
      initJS: function (callback) {
      },
      registerCallback: function (functionName) {
      },
      unregisterCallback: function (functionName) {
      },
      eval: function (expression) {
      },
      call: function (request) {
      },
      getId: function () {
        return null;
      }
    };
  })(Shumway.ExternalInterfaceService || (Shumway.ExternalInterfaceService = {}));
  var ExternalInterfaceService = Shumway.ExternalInterfaceService;

  var Callback = (function () {
    function Callback() {
      this._queues = {};
    }
    Callback.prototype.register = function (type, callback) {
      Debug.assert(type);
      Debug.assert(callback);
      var queue = this._queues[type];
      if (queue) {
        if (queue.indexOf(callback) > -1) {
          return;
        }
      } else {
        queue = this._queues[type] = [];
      }
      queue.push(callback);
    };

    Callback.prototype.unregister = function (type, callback) {
      Debug.assert(type);
      Debug.assert(callback);
      var queue = this._queues[type];
      if (!queue) {
        return;
      }
      var i = queue.indexOf(callback);
      if (i !== -1) {
        queue.splice(i, 1);
      }
      if (queue.length === 0) {
        this._queues[type] = null;
      }
    };

    Callback.prototype.notify = function (type, args) {
      var queue = this._queues[type];
      if (!queue) {
        return;
      }
      queue = queue.slice();
      var args = Array.prototype.slice.call(arguments, 0);
      for (var i = 0; i < queue.length; i++) {
        var callback = queue[i];
        callback.apply(null, args);
      }
    };

    Callback.prototype.notify1 = function (type, value) {
      var queue = this._queues[type];
      if (!queue) {
        return;
      }
      queue = queue.slice();
      for (var i = 0; i < queue.length; i++) {
        var callback = queue[i];
        callback(type, value);
      }
    };
    return Callback;
  })();
  Shumway.Callback = Callback;

  (function (ImageType) {
    ImageType[ImageType["None"] = 0] = "None";

    ImageType[ImageType["PremultipliedAlphaARGB"] = 1] = "PremultipliedAlphaARGB";

    ImageType[ImageType["StraightAlphaARGB"] = 2] = "StraightAlphaARGB";

    ImageType[ImageType["StraightAlphaRGBA"] = 3] = "StraightAlphaRGBA";

    ImageType[ImageType["JPEG"] = 4] = "JPEG";
    ImageType[ImageType["PNG"] = 5] = "PNG";
    ImageType[ImageType["GIF"] = 6] = "GIF";
  })(Shumway.ImageType || (Shumway.ImageType = {}));
  var ImageType = Shumway.ImageType;

  var PromiseWrapper = (function () {
    function PromiseWrapper() {
      this.promise = new Promise(function (resolve, reject) {
        this.resolve = resolve;
        this.reject = reject;
      }.bind(this));
    }
    return PromiseWrapper;
  })();
  Shumway.PromiseWrapper = PromiseWrapper;
})(Shumway || (Shumway = {}));

(function PromiseClosure() {
  var global = Function("return this")();
  if (global.Promise) {
    if (typeof global.Promise.all !== 'function') {
      global.Promise.all = function (iterable) {
        var count = 0, results = [], resolve, reject;
        var promise = new global.Promise(function (resolve_, reject_) {
          resolve = resolve_;
          reject = reject_;
        });
        iterable.forEach(function (p, i) {
          count++;
          p.then(function (result) {
            results[i] = result;
            count--;
            if (count === 0) {
              resolve(results);
            }
          }, reject);
        });
        if (count === 0) {
          resolve(results);
        }
        return promise;
      };
    }
    if (typeof global.Promise.resolve !== 'function') {
      global.Promise.resolve = function (x) {
        return new global.Promise(function (resolve) {
          resolve(x);
        });
      };
    }
    return;
  }

  function getDeferred(C) {
    if (typeof C !== 'function') {
      throw new TypeError('Invalid deferred constructor');
    }
    var resolver = createDeferredConstructionFunctions();
    var promise = new C(resolver);
    var resolve = resolver.resolve;
    if (typeof resolve !== 'function') {
      throw new TypeError('Invalid resolve construction function');
    }
    var reject = resolver.reject;
    if (typeof reject !== 'function') {
      throw new TypeError('Invalid reject construction function');
    }
    return { promise: promise, resolve: resolve, reject: reject };
  }

  function updateDeferredFromPotentialThenable(x, deferred) {
    if (typeof x !== 'object' || x === null) {
      return false;
    }
    try  {
      var then = x.then;
      if (typeof then !== 'function') {
        return false;
      }
      var thenCallResult = then.call(x, deferred.resolve, deferred.reject);
    } catch (e) {
      var reject = deferred.reject;
      reject(e);
    }
    return true;
  }

  function isPromise(x) {
    return typeof x === 'object' && x !== null && typeof x.promiseStatus !== 'undefined';
  }

  function rejectPromise(promise, reason) {
    if (promise.promiseStatus !== 'unresolved') {
      return;
    }
    var reactions = promise.rejectReactions;
    promise.result = reason;
    promise.resolveReactions = undefined;
    promise.rejectReactions = undefined;
    promise.promiseStatus = 'has-rejection';
    triggerPromiseReactions(reactions, reason);
  }

  function resolvePromise(promise, resolution) {
    if (promise.promiseStatus !== 'unresolved') {
      return;
    }
    var reactions = promise.resolveReactions;
    promise.result = resolution;
    promise.resolveReactions = undefined;
    promise.rejectReactions = undefined;
    promise.promiseStatus = 'has-resolution';
    triggerPromiseReactions(reactions, resolution);
  }

  function triggerPromiseReactions(reactions, argument) {
    for (var i = 0; i < reactions.length; i++) {
      queueMicrotask({ reaction: reactions[i], argument: argument });
    }
  }

  function queueMicrotask(task) {
    if (microtasksQueue.length === 0) {
      setTimeout(handleMicrotasksQueue, 0);
    }
    microtasksQueue.push(task);
  }

  function executePromiseReaction(reaction, argument) {
    var deferred = reaction.deferred;
    var handler = reaction.handler;
    var handlerResult, updateResult;
    try  {
      handlerResult = handler(argument);
    } catch (e) {
      var reject = deferred.reject;
      return reject(e);
    }

    if (handlerResult === deferred.promise) {
      var reject = deferred.reject;
      return reject(new TypeError('Self resolution'));
    }

    try  {
      updateResult = updateDeferredFromPotentialThenable(handlerResult, deferred);
      if (!updateResult) {
        var resolve = deferred.resolve;
        return resolve(handlerResult);
      }
    } catch (e) {
      var reject = deferred.reject;
      return reject(e);
    }
  }

  var microtasksQueue = [];

  function handleMicrotasksQueue() {
    while (microtasksQueue.length > 0) {
      var task = microtasksQueue[0];
      try  {
        executePromiseReaction(task.reaction, task.argument);
      } catch (e) {
        if (typeof Promise.onerror === 'function') {
          Promise.onerror(e);
        }
      }
      microtasksQueue.shift();
    }
  }

  function throwerFunction(e) {
    throw e;
  }

  function identityFunction(x) {
    return x;
  }

  function createRejectPromiseFunction(promise) {
    return function (reason) {
      rejectPromise(promise, reason);
    };
  }

  function createResolvePromiseFunction(promise) {
    return function (resolution) {
      resolvePromise(promise, resolution);
    };
  }

  function createDeferredConstructionFunctions() {
    var fn = function (resolve, reject) {
      fn.resolve = resolve;
      fn.reject = reject;
    };
    return fn;
  }

  function createPromiseResolutionHandlerFunctions(promise, fulfillmentHandler, rejectionHandler) {
    return function (x) {
      if (x === promise) {
        return rejectionHandler(new TypeError('Self resolution'));
      }
      var cstr = promise.promiseConstructor;
      if (isPromise(x)) {
        var xConstructor = x.promiseConstructor;
        if (xConstructor === cstr) {
          return x.then(fulfillmentHandler, rejectionHandler);
        }
      }
      var deferred = getDeferred(cstr);
      var updateResult = updateDeferredFromPotentialThenable(x, deferred);
      if (updateResult) {
        var deferredPromise = deferred.promise;
        return deferredPromise.then(fulfillmentHandler, rejectionHandler);
      }
      return fulfillmentHandler(x);
    };
  }

  function createPromiseAllCountdownFunction(index, values, deferred, countdownHolder) {
    return function (x) {
      values[index] = x;
      countdownHolder.countdown--;
      if (countdownHolder.countdown === 0) {
        deferred.resolve(values);
      }
    };
  }

  function Promise(resolver) {
    if (typeof resolver !== 'function') {
      throw new TypeError('resolver is not a function');
    }
    var promise = this;
    if (typeof promise !== 'object') {
      throw new TypeError('Promise to initialize is not an object');
    }
    promise.promiseStatus = 'unresolved';
    promise.resolveReactions = [];
    promise.rejectReactions = [];
    promise.result = undefined;

    var resolve = createResolvePromiseFunction(promise);
    var reject = createRejectPromiseFunction(promise);

    try  {
      var result = resolver(resolve, reject);
    } catch (e) {
      rejectPromise(promise, e);
    }

    promise.promiseConstructor = Promise;
    return promise;
  }

  Promise.all = function (iterable) {
    var deferred = getDeferred(this);
    var values = [];
    var countdownHolder = { countdown: 0 };
    var index = 0;
    iterable.forEach(function (nextValue) {
      var nextPromise = this.cast(nextValue);
      var fn = createPromiseAllCountdownFunction(index, values, deferred, countdownHolder);
      nextPromise.then(fn, deferred.reject);
      index++;
      countdownHolder.countdown++;
    }, this);
    if (index === 0) {
      deferred.resolve(values);
    }
    return deferred.promise;
  };
  Promise.cast = function (x) {
    if (isPromise(x)) {
      return x;
    }
    var deferred = getDeferred(this);
    deferred.resolve(x);
    return deferred.promise;
  };
  Promise.reject = function (r) {
    var deferred = getDeferred(this);
    var rejectResult = deferred.reject(r);
    return deferred.promise;
  };
  Promise.resolve = function (x) {
    var deferred = getDeferred(this);
    var rejectResult = deferred.resolve(x);
    return deferred.promise;
  };
  Promise.prototype = {
    'catch': function (onRejected) {
      this.then(undefined, onRejected);
    },
    then: function (onFulfilled, onRejected) {
      var promise = this;
      if (!isPromise(promise)) {
        throw new TypeError('this is not a Promises');
      }
      var cstr = promise.promiseConstructor;
      var deferred = getDeferred(cstr);

      var rejectionHandler = typeof onRejected === 'function' ? onRejected : throwerFunction;
      var fulfillmentHandler = typeof onFulfilled === 'function' ? onFulfilled : identityFunction;
      var resolutionHandler = createPromiseResolutionHandlerFunctions(promise, fulfillmentHandler, rejectionHandler);

      var resolveReaction = { deferred: deferred, handler: resolutionHandler };
      var rejectReaction = { deferred: deferred, handler: rejectionHandler };

      switch (promise.promiseStatus) {
        case 'unresolved':
          promise.resolveReactions.push(resolveReaction);
          promise.rejectReactions.push(rejectReaction);
          break;
        case 'has-resolution':
          var resolution = promise.result;
          queueMicrotask({ reaction: resolveReaction, argument: resolution });
          break;
        case 'has-rejection':
          var rejection = promise.result;
          queueMicrotask({ reaction: rejectReaction, argument: rejection });
          break;
      }
      return deferred.promise;
    }
  };

  global.Promise = Promise;
})();

if (typeof exports !== "undefined") {
  exports["Shumway"] = Shumway;
}

(function () {
  function extendBuiltin(prototype, property, value) {
    if (!prototype[property]) {
      Object.defineProperty(prototype, property, {
        value: value,
        writable: true,
        configurable: true,
        enumerable: false });
    }
  }

  function removeColors(s) {
    return s.replace(/\033\[[0-9]*m/g, "");
  }

  extendBuiltin(String.prototype, "padRight", function (c, n) {
    var str = this;
    var length = removeColors(str).length;
    if (!c || length >= n) {
      return str;
    }
    var max = (n - length) / c.length;
    for (var i = 0; i < max; i++) {
      str += c;
    }
    return str;
  });

  extendBuiltin(String.prototype, "padLeft", function (c, n) {
    var str = this;
    var length = str.length;
    if (!c || length >= n) {
      return str;
    }
    var max = (n - length) / c.length;
    for (var i = 0; i < max; i++) {
      str = c + str;
    }
    return str;
  });

  extendBuiltin(String.prototype, "trim", function () {
    return this.replace(/^\s+|\s+$/g, "");
  });

  extendBuiltin(String.prototype, "endsWith", function (str) {
    return this.indexOf(str, this.length - str.length) !== -1;
  });

  extendBuiltin(Array.prototype, "replace", function (x, y) {
    if (x === y) {
      return 0;
    }
    var count = 0;
    for (var i = 0; i < this.length; i++) {
      if (this[i] === x) {
        this[i] = y;
        count++;
      }
    }
    return count;
  });
})();
var Shumway;
(function (Shumway) {
  (function (Options) {
    var isObject = Shumway.isObject;

    var assert = Shumway.Debug.assert;

    var Argument = (function () {
      function Argument(shortName, longName, type, options) {
        this.shortName = shortName;
        this.longName = longName;
        this.type = type;
        options = options || {};
        this.positional = options.positional;
        this.parseFn = options.parse;
        this.value = options.defaultValue;
      }
      Argument.prototype.parse = function (value) {
        if (this.type === "boolean") {
          release || assert(typeof value === "boolean");
          this.value = value;
        } else if (this.type === "number") {
          release || assert(!isNaN(value), value + " is not a number");
          this.value = parseInt(value, 10);
        } else {
          this.value = value;
        }
        if (this.parseFn) {
          this.parseFn(this.value);
        }
      };
      return Argument;
    })();
    Options.Argument = Argument;

    var ArgumentParser = (function () {
      function ArgumentParser() {
        this.args = [];
      }
      ArgumentParser.prototype.addArgument = function (shortName, longName, type, options) {
        var argument = new Argument(shortName, longName, type, options);
        this.args.push(argument);
        return argument;
      };
      ArgumentParser.prototype.addBoundOption = function (option) {
        var options = { parse: function (x) {
            option.value = x;
          } };
        this.args.push(new Argument(option.shortName, option.longName, option.type, options));
      };
      ArgumentParser.prototype.addBoundOptionSet = function (optionSet) {
        var self = this;
        optionSet.options.forEach(function (x) {
          if (x instanceof OptionSet) {
            self.addBoundOptionSet(x);
          } else {
            release || assert(x instanceof Option);
            self.addBoundOption(x);
          }
        });
      };
      ArgumentParser.prototype.getUsage = function () {
        var str = "";
        this.args.forEach(function (x) {
          if (!x.positional) {
            str += "[-" + x.shortName + "|--" + x.longName + (x.type === "boolean" ? "" : " " + x.type[0].toUpperCase()) + "]";
          } else {
            str += x.longName;
          }
          str += " ";
        });
        return str;
      };
      ArgumentParser.prototype.parse = function (args) {
        var nonPositionalArgumentMap = {};
        var positionalArgumentList = [];
        this.args.forEach(function (x) {
          if (x.positional) {
            positionalArgumentList.push(x);
          } else {
            nonPositionalArgumentMap["-" + x.shortName] = x;
            nonPositionalArgumentMap["--" + x.longName] = x;
          }
        });

        var leftoverArguments = [];

        while (args.length) {
          var argString = args.shift();
          var argument = null, value = argString;
          if (argString == '--') {
            leftoverArguments = leftoverArguments.concat(args);
            break;
          } else if (argString.slice(0, 1) == '-' || argString.slice(0, 2) == '--') {
            argument = nonPositionalArgumentMap[argString];

            if (!argument) {
              continue;
            }
            if (argument.type !== "boolean") {
              value = args.shift();
              release || assert(value !== "-" && value !== "--", "Argument " + argString + " must have a value.");
            } else {
              value = true;
            }
          } else if (positionalArgumentList.length) {
            argument = positionalArgumentList.shift();
          } else {
            leftoverArguments.push(value);
          }
          if (argument) {
            argument.parse(value);
          }
        }
        release || assert(positionalArgumentList.length === 0, "Missing positional arguments.");
        return leftoverArguments;
      };
      return ArgumentParser;
    })();
    Options.ArgumentParser = ArgumentParser;

    var OptionSet = (function () {
      function OptionSet(name, settings) {
        if (typeof settings === "undefined") { settings = null; }
        this.open = false;
        this.name = name;
        this.settings = settings || {};
        this.options = [];
      }
      OptionSet.prototype.register = function (option) {
        if (option instanceof OptionSet) {
          for (var i = 0; i < this.options.length; i++) {
            var optionSet = this.options[i];
            if (optionSet instanceof OptionSet && optionSet.name === option.name) {
              return optionSet;
            }
          }
        }
        this.options.push(option);
        if (this.settings) {
          if (option instanceof OptionSet) {
            var optionSettings = this.settings[option.name];
            if (isObject(optionSettings)) {
              option.settings = optionSettings.settings;
              option.open = optionSettings.open;
            }
          } else {
            if (typeof this.settings[option.longName] !== "undefined") {
              switch (option.type) {
                case "boolean":
                  option.value = !!this.settings[option.longName];
                  break;
                case "number":
                  option.value = +this.settings[option.longName];
                  break;
                default:
                  option.value = this.settings[option.longName];
                  break;
              }
            }
          }
        }
        return option;
      };
      OptionSet.prototype.trace = function (writer) {
        writer.enter(this.name + " {");
        this.options.forEach(function (option) {
          option.trace(writer);
        });
        writer.leave("}");
      };
      OptionSet.prototype.getSettings = function () {
        var settings = {};
        this.options.forEach(function (option) {
          if (option instanceof OptionSet) {
            settings[option.name] = {
              settings: option.getSettings(),
              open: option.open
            };
          } else {
            settings[option.longName] = option.value;
          }
        });
        return settings;
      };
      OptionSet.prototype.setSettings = function (settings) {
        if (!settings) {
          return;
        }
        this.options.forEach(function (option) {
          if (option instanceof OptionSet) {
            if (option.name in settings) {
              option.setSettings(settings[option.name].settings);
            }
          } else {
            if (option.longName in settings) {
              option.value = settings[option.longName];
            }
          }
        });
      };
      return OptionSet;
    })();
    Options.OptionSet = OptionSet;

    var Option = (function () {
      function Option(shortName, longName, type, defaultValue, description, config) {
        if (typeof config === "undefined") { config = null; }
        this.longName = longName;
        this.shortName = shortName;
        this.type = type;
        this.defaultValue = defaultValue;
        this.value = defaultValue;
        this.description = description;
        this.config = config;
      }
      Option.prototype.parse = function (value) {
        this.value = value;
      };
      Option.prototype.trace = function (writer) {
        writer.writeLn(("-" + this.shortName + "|--" + this.longName).padRight(" ", 30) + " = " + this.type + " " + this.value + " [" + this.defaultValue + "]" + " (" + this.description + ")");
      };
      return Option;
    })();
    Options.Option = Option;
  })(Shumway.Options || (Shumway.Options = {}));
  var Options = Shumway.Options;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Settings) {
    Settings.ROOT = "Shumway Options";
    Settings.shumwayOptions = new Shumway.Options.OptionSet(Settings.ROOT, load());

    function isStorageSupported() {
      try  {
        return typeof window !== 'undefined' && "localStorage" in window && window["localStorage"] !== null;
      } catch (e) {
        return false;
      }
    }
    Settings.isStorageSupported = isStorageSupported;

    function load(key) {
      if (typeof key === "undefined") { key = Settings.ROOT; }
      var settings = {};
      if (isStorageSupported()) {
        var lsValue = window.localStorage[key];
        if (lsValue) {
          try  {
            settings = JSON.parse(lsValue);
          } catch (e) {
          }
        }
      }
      return settings;
    }
    Settings.load = load;

    function save(settings, key) {
      if (typeof settings === "undefined") { settings = null; }
      if (typeof key === "undefined") { key = Settings.ROOT; }
      if (isStorageSupported()) {
        try  {
          window.localStorage[key] = JSON.stringify(settings ? settings : Settings.shumwayOptions.getSettings());
        } catch (e) {
        }
      }
    }
    Settings.save = save;

    function setSettings(settings) {
      Settings.shumwayOptions.setSettings(settings);
    }
    Settings.setSettings = setSettings;

    function getSettings(settings) {
      return Settings.shumwayOptions.getSettings();
    }
    Settings.getSettings = getSettings;
  })(Shumway.Settings || (Shumway.Settings = {}));
  var Settings = Shumway.Settings;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Metrics) {
    var Timer = (function () {
      function Timer(parent, name) {
        this._parent = parent;
        this._timers = Shumway.ObjectUtilities.createMap();
        this._name = name;
        this._begin = 0;
        this._last = 0;
        this._total = 0;
        this._count = 0;
      }
      Timer.time = function (name, fn) {
        Timer.start(name);
        fn();
        Timer.stop();
      };
      Timer.start = function (name) {
        Timer._top = Timer._top._timers[name] || (Timer._top._timers[name] = new Timer(Timer._top, name));
        Timer._top.start();
        var tmp = Timer._flat._timers[name] || (Timer._flat._timers[name] = new Timer(Timer._flat, name));
        tmp.start();
        Timer._flatStack.push(tmp);
      };
      Timer.stop = function () {
        Timer._top.stop();
        Timer._top = Timer._top._parent;
        Timer._flatStack.pop().stop();
      };
      Timer.stopStart = function (name) {
        Timer.stop();
        Timer.start(name);
      };
      Timer.prototype.start = function () {
        this._begin = Shumway.getTicks();
      };
      Timer.prototype.stop = function () {
        this._last = Shumway.getTicks() - this._begin;
        this._total += this._last;
        this._count += 1;
      };
      Timer.prototype.toJSON = function () {
        return { name: this._name, total: this._total, timers: this._timers };
      };
      Timer.prototype.trace = function (writer) {
        writer.enter(this._name + ": " + this._total.toFixed(2) + " ms" + ", count: " + this._count + ", average: " + (this._total / this._count).toFixed(2) + " ms");
        for (var name in this._timers) {
          this._timers[name].trace(writer);
        }
        writer.outdent();
      };
      Timer.trace = function (writer) {
        Timer._base.trace(writer);
        Timer._flat.trace(writer);
      };
      Timer._base = new Timer(null, "Total");
      Timer._top = Timer._base;
      Timer._flat = new Timer(null, "Flat");
      Timer._flatStack = [];
      return Timer;
    })();
    Metrics.Timer = Timer;

    var Counter = (function () {
      function Counter(enabled) {
        this._enabled = enabled;
        this.clear();
      }
      Object.defineProperty(Counter.prototype, "counts", {
        get: function () {
          return this._counts;
        },
        enumerable: true,
        configurable: true
      });

      Counter.prototype.setEnabled = function (enabled) {
        this._enabled = enabled;
      };
      Counter.prototype.clear = function () {
        this._counts = Shumway.ObjectUtilities.createMap();
        this._times = Shumway.ObjectUtilities.createMap();
      };
      Counter.prototype.toJSON = function () {
        return {
          counts: this._counts,
          times: this._times
        };
      };
      Counter.prototype.count = function (name, increment, time) {
        if (typeof increment === "undefined") { increment = 1; }
        if (typeof time === "undefined") { time = 0; }
        if (!this._enabled) {
          return;
        }
        if (this._counts[name] === undefined) {
          this._counts[name] = 0;
          this._times[name] = 0;
        }
        this._counts[name] += increment;
        this._times[name] += time;
        return this._counts[name];
      };
      Counter.prototype.trace = function (writer) {
        for (var name in this._counts) {
          writer.writeLn(name + ": " + this._counts[name]);
        }
      };
      Counter.prototype._pairToString = function (times, pair) {
        var name = pair[0];
        var count = pair[1];
        var time = times[name];
        var line = name + ": " + count;
        if (time) {
          line += ", " + time.toFixed(4);
          if (count > 1) {
            line += " (" + (time / count).toFixed(4) + ")";
          }
        }
        return line;
      };
      Counter.prototype.toStringSorted = function () {
        var self = this;
        var times = this._times;
        var pairs = [];
        for (var name in this._counts) {
          pairs.push([name, this._counts[name]]);
        }
        pairs.sort(function (a, b) {
          return b[1] - a[1];
        });
        return (pairs.map(function (pair) {
          return self._pairToString(times, pair);
        }).join(", "));
      };
      Counter.prototype.traceSorted = function (writer, inline) {
        if (typeof inline === "undefined") { inline = false; }
        var self = this;
        var times = this._times;
        var pairs = [];
        for (var name in this._counts) {
          pairs.push([name, this._counts[name]]);
        }
        pairs.sort(function (a, b) {
          return b[1] - a[1];
        });
        if (inline) {
          writer.writeLn(pairs.map(function (pair) {
            return self._pairToString(times, pair);
          }).join(", "));
        } else {
          pairs.forEach(function (pair) {
            writer.writeLn(self._pairToString(times, pair));
          });
        }
      };
      Counter.instance = new Counter(true);
      return Counter;
    })();
    Metrics.Counter = Counter;

    var Average = (function () {
      function Average(max) {
        this._samples = new Float64Array(max);
        this._count = 0;
        this._index = 0;
      }
      Average.prototype.push = function (sample) {
        if (this._count < this._samples.length) {
          this._count++;
        }
        this._index++;
        this._samples[this._index % this._samples.length] = sample;
      };
      Average.prototype.average = function () {
        var sum = 0;
        for (var i = 0; i < this._count; i++) {
          sum += this._samples[i];
        }
        return sum / this._count;
      };
      return Average;
    })();
    Metrics.Average = Average;
  })(Shumway.Metrics || (Shumway.Metrics = {}));
  var Metrics = Shumway.Metrics;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (ArrayUtilities) {
    var InflateState;
    (function (InflateState) {
      InflateState[InflateState["INIT"] = 0] = "INIT";
      InflateState[InflateState["BLOCK_0"] = 1] = "BLOCK_0";
      InflateState[InflateState["BLOCK_1"] = 2] = "BLOCK_1";
      InflateState[InflateState["BLOCK_2_PRE"] = 3] = "BLOCK_2_PRE";
      InflateState[InflateState["BLOCK_2"] = 4] = "BLOCK_2";
      InflateState[InflateState["DONE"] = 5] = "DONE";
      InflateState[InflateState["ERROR"] = 6] = "ERROR";
      InflateState[InflateState["VERIFY_HEADER"] = 7] = "VERIFY_HEADER";
    })(InflateState || (InflateState = {}));

    var WINDOW_SIZE = 32768;
    var WINDOW_SHIFT_POSITION = 65536;
    var MAX_WINDOW_SIZE = WINDOW_SHIFT_POSITION + 258;

    var Inflate = (function () {
      function Inflate(verifyHeader) {
        this._buffer = new Uint8Array(1024);
        this._bufferSize = 0;
        this._bufferPosition = 0;
        this._bitBuffer = 0;
        this._bitLength = 0;
        this._window = new Uint8Array(MAX_WINDOW_SIZE);
        this._windowPosition = 0;
        this._state = verifyHeader ? 7 /* VERIFY_HEADER */ : 0 /* INIT */;
        this._isFinalBlock = false;
        this._literalTable = null;
        this._distanceTable = null;
        this._block0Read = 0;
        this._block2State = null;
        this._copyState = {
          state: 0,
          len: 0,
          lenBits: 0,
          dist: 0,
          distBits: 0
        };
        this._error = undefined;

        if (!areTablesInitialized) {
          initializeTables();
          areTablesInitialized = true;
        }
      }
      Object.defineProperty(Inflate.prototype, "error", {
        get: function () {
          return this._error;
        },
        enumerable: true,
        configurable: true
      });

      Inflate.prototype.push = function (data) {
        if (this._buffer.length < this._bufferSize + data.length) {
          var newBuffer = new Uint8Array(this._bufferSize + data.length);
          newBuffer.set(this._buffer);
          this._buffer = newBuffer;
        }
        this._buffer.set(data, this._bufferSize);
        this._bufferSize += data.length;
        this._bufferPosition = 0;

        var incomplete = false;
        do {
          var lastPosition = this._windowPosition;
          if (this._state === 0 /* INIT */) {
            incomplete = this._decodeInitState();
            if (incomplete) {
              break;
            }
          }

          switch (this._state) {
            case 1 /* BLOCK_0 */:
              incomplete = this._decodeBlock0();
              break;
            case 3 /* BLOCK_2_PRE */:
              incomplete = this._decodeBlock2Pre();
              if (incomplete) {
                break;
              }

            case 2 /* BLOCK_1 */:
            case 4 /* BLOCK_2 */:
              incomplete = this._decodeBlock();
              break;
            case 6 /* ERROR */:
            case 5 /* DONE */:
              this._bufferPosition = this._bufferSize;
              break;
            case 7 /* VERIFY_HEADER */:
              incomplete = this._verifyZlibHeader();
              break;
          }

          var decoded = this._windowPosition - lastPosition;
          if (decoded > 0) {
            this.onData(this._window.subarray(lastPosition, this._windowPosition));
          }
          if (this._windowPosition >= WINDOW_SHIFT_POSITION) {
            this._window.set(this._window.subarray(this._windowPosition - WINDOW_SIZE, this._windowPosition));
            this._windowPosition = WINDOW_SIZE;
          }
        } while(!incomplete && this._bufferPosition < this._bufferSize);

        if (this._bufferPosition < this._bufferSize) {
          this._buffer.set(this._buffer.subarray(this._bufferPosition, this._bufferSize));
          this._bufferSize -= this._bufferPosition;
        } else {
          this._bufferSize = 0;
        }
      };
      Inflate.prototype._verifyZlibHeader = function () {
        var position = this._bufferPosition;
        if (position + 2 > this._bufferSize) {
          return true;
        }
        var buffer = this._buffer;
        var header = (buffer[position] << 8) | buffer[position + 1];
        this._bufferPosition = position + 2;
        var error = null;
        if ((header & 0x0f00) !== 0x0800) {
          error = 'inflate: unknown compression method';
        } else if ((header % 31) !== 0) {
          error = 'inflate: bad FCHECK';
        } else if ((header & 0x20) !== 0) {
          error = 'inflate: FDICT bit set';
        }
        if (error) {
          this._error = error;
          this._state = 6 /* ERROR */;
        } else {
          this._state = 0 /* INIT */;
        }
      };
      Inflate.prototype._decodeInitState = function () {
        if (this._isFinalBlock) {
          this._state = 5 /* DONE */;
          return false;
        }

        var buffer = this._buffer, bufferSize = this._bufferSize;
        var bitBuffer = this._bitBuffer, bitLength = this._bitLength;
        var state;
        var position = this._bufferPosition;
        if (((bufferSize - position) << 3) + bitLength < 3) {
          return true;
        }
        if (bitLength < 3) {
          bitBuffer |= buffer[position++] << bitLength;
          bitLength += 8;
        }
        var type = bitBuffer & 7;
        bitBuffer >>= 3;
        bitLength -= 3;
        switch (type >> 1) {
          case 0:
            bitBuffer = 0;
            bitLength = 0;
            if (bufferSize - position < 4) {
              return true;
            }

            var length = buffer[position] | (buffer[position + 1] << 8);
            var length2 = buffer[position + 2] | (buffer[position + 3] << 8);
            position += 4;
            if ((length ^ length2) !== 0xFFFF) {
              this._error = 'inflate: invalid block 0 length';
              state = 6 /* ERROR */;
              break;
            }

            if (length === 0) {
              state = 0 /* INIT */;
            } else {
              this._block0Read = length;
              state = 1 /* BLOCK_0 */;
            }
            break;
          case 1:
            state = 2 /* BLOCK_1 */;
            this._literalTable = fixedLiteralTable;
            this._distanceTable = fixedDistanceTable;
            break;
          case 2:
            if (((bufferSize - position) << 3) + bitLength < 14 + 3 * 4) {
              return true;
            }
            while (bitLength < 14) {
              bitBuffer |= buffer[position++] << bitLength;
              bitLength += 8;
            }
            var numLengthCodes = ((bitBuffer >> 10) & 15) + 4;
            if (((bufferSize - position) << 3) + bitLength < 14 + 3 * numLengthCodes) {
              return true;
            }
            var block2State = {
              numLiteralCodes: (bitBuffer & 31) + 257,
              numDistanceCodes: ((bitBuffer >> 5) & 31) + 1,
              codeLengthTable: undefined,
              bitLengths: undefined,
              codesRead: 0,
              dupBits: 0
            };
            bitBuffer >>= 14;
            bitLength -= 14;
            var codeLengths = new Uint8Array(19);
            for (var i = 0; i < numLengthCodes; ++i) {
              if (bitLength < 3) {
                bitBuffer |= buffer[position++] << bitLength;
                bitLength += 8;
              }
              codeLengths[codeLengthOrder[i]] = bitBuffer & 7;
              bitBuffer >>= 3;
              bitLength -= 3;
            }
            for (; i < 19; i++) {
              codeLengths[codeLengthOrder[i]] = 0;
            }
            block2State.bitLengths = new Uint8Array(block2State.numLiteralCodes + block2State.numDistanceCodes);
            block2State.codeLengthTable = makeHuffmanTable(codeLengths);
            this._block2State = block2State;
            state = 3 /* BLOCK_2_PRE */;
            break;
          default:
            this._error = 'inflate: unsupported block type';
            state = 6 /* ERROR */;
            return false;
        }
        this._isFinalBlock = !!(type & 1);
        this._state = state;
        this._bufferPosition = position;
        this._bitBuffer = bitBuffer;
        this._bitLength = bitLength;
        return false;
      };
      Inflate.prototype._decodeBlock0 = function () {
        var position = this._bufferPosition;
        var windowPosition = this._windowPosition;
        var toRead = this._block0Read;
        var leftInWindow = MAX_WINDOW_SIZE - windowPosition;
        var leftInBuffer = this._bufferSize - position;
        var incomplete = leftInBuffer < toRead;
        var canFit = Math.min(leftInWindow, leftInBuffer, toRead);
        this._window.set(this._buffer.subarray(position, position + canFit), windowPosition);
        this._windowPosition = windowPosition + canFit;
        this._bufferPosition = position + canFit;
        this._block0Read = toRead - canFit;

        if (toRead === canFit) {
          this._state = 0 /* INIT */;
          return false;
        }

        return incomplete && leftInWindow < leftInBuffer;
      };
      Inflate.prototype._readBits = function (size) {
        var bitBuffer = this._bitBuffer;
        var bitLength = this._bitLength;
        if (size > bitLength) {
          var pos = this._bufferPosition;
          var end = this._bufferSize;
          do {
            if (pos >= end) {
              this._bufferPosition = pos;
              this._bitBuffer = bitBuffer;
              this._bitLength = bitLength;
              return -1;
            }
            bitBuffer |= this._buffer[pos++] << bitLength;
            bitLength += 8;
          } while(size > bitLength);
          this._bufferPosition = pos;
        }
        this._bitBuffer = bitBuffer >> size;
        this._bitLength = bitLength - size;
        return bitBuffer & ((1 << size) - 1);
      };
      Inflate.prototype._readCode = function (codeTable) {
        var bitBuffer = this._bitBuffer;
        var bitLength = this._bitLength;
        var maxBits = codeTable.maxBits;
        if (maxBits > bitLength) {
          var pos = this._bufferPosition;
          var end = this._bufferSize;
          do {
            if (pos >= end) {
              this._bufferPosition = pos;
              this._bitBuffer = bitBuffer;
              this._bitLength = bitLength;
              return -1;
            }
            bitBuffer |= this._buffer[pos++] << bitLength;
            bitLength += 8;
          } while(maxBits > bitLength);
          this._bufferPosition = pos;
        }

        var code = codeTable.codes[bitBuffer & ((1 << maxBits) - 1)];
        var len = code >> 16;
        if ((code & 0x8000)) {
          this._error = 'inflate: invalid encoding';
          this._state = 6 /* ERROR */;
          return -1;
        }

        this._bitBuffer = bitBuffer >> len;
        this._bitLength = bitLength - len;
        return code & 0xffff;
      };
      Inflate.prototype._decodeBlock2Pre = function () {
        var block2State = this._block2State;
        var numCodes = block2State.numLiteralCodes + block2State.numDistanceCodes;
        var bitLengths = block2State.bitLengths;
        var i = block2State.codesRead;
        var prev = i > 0 ? bitLengths[i - 1] : 0;
        var codeLengthTable = block2State.codeLengthTable;
        var j;
        if (block2State.dupBits > 0) {
          j = this._readBits(block2State.dupBits);
          if (j < 0) {
            return true;
          }
          while (j--) {
            bitLengths[i++] = prev;
          }
          block2State.dupBits = 0;
        }
        while (i < numCodes) {
          var sym = this._readCode(codeLengthTable);
          if (sym < 0) {
            block2State.codesRead = i;
            return true;
          } else if (sym < 16) {
            bitLengths[i++] = (prev = sym);
            continue;
          }
          var j, dupBits;
          switch (sym) {
            case 16:
              dupBits = 2;
              j = 3;
              sym = prev;
              break;
            case 17:
              dupBits = 3;
              j = 3;
              sym = 0;
              break;
            case 18:
              dupBits = 7;
              j = 11;
              sym = 0;
              break;
          }
          while (j--) {
            bitLengths[i++] = sym;
          }
          j = this._readBits(dupBits);
          if (j < 0) {
            block2State.codesRead = i;
            block2State.dupBits = dupBits;
            return true;
          }
          while (j--) {
            bitLengths[i++] = sym;
          }
          prev = sym;
        }
        this._literalTable = makeHuffmanTable(bitLengths.subarray(0, block2State.numLiteralCodes));
        this._distanceTable = makeHuffmanTable(bitLengths.subarray(block2State.numLiteralCodes));
        this._state = 4 /* BLOCK_2 */;
        this._block2State = null;
        return false;
      };
      Inflate.prototype._decodeBlock = function () {
        var literalTable = this._literalTable, distanceTable = this._distanceTable;
        var output = this._window, pos = this._windowPosition;
        var copyState = this._copyState;
        var i, j, sym;
        var len, lenBits, dist, distBits;

        if (copyState.state !== 0) {
          switch (copyState.state) {
            case 1:
              j = 0;
              if ((j = this._readBits(copyState.lenBits)) < 0) {
                return true;
              }
              copyState.len += j;
              copyState.state = 2;

            case 2:
              if ((sym = this._readCode(distanceTable)) < 0) {
                return true;
              }
              copyState.distBits = distanceExtraBits[sym];
              copyState.dist = distanceCodes[sym];
              copyState.state = 3;

            case 3:
              j = 0;
              if (copyState.distBits > 0 && (j = this._readBits(copyState.distBits)) < 0) {
                return true;
              }
              dist = copyState.dist + j;
              len = copyState.len;
              i = pos - dist;
              while (len--) {
                output[pos++] = output[i++];
              }
              copyState.state = 0;
              if (pos >= WINDOW_SHIFT_POSITION) {
                this._windowPosition = pos;
                return false;
              }
              break;
          }
        }

        do {
          sym = this._readCode(literalTable);
          if (sym < 0) {
            this._windowPosition = pos;
            return true;
          } else if (sym < 256) {
            output[pos++] = sym;
          } else if (sym > 256) {
            this._windowPosition = pos;
            sym -= 257;
            lenBits = lengthExtraBits[sym];
            len = lengthCodes[sym];
            j = lenBits === 0 ? 0 : this._readBits(lenBits);
            if (j < 0) {
              copyState.state = 1;
              copyState.len = len;
              copyState.lenBits = lenBits;
              return true;
            }
            len += j;
            sym = this._readCode(distanceTable);
            if (sym < 0) {
              copyState.state = 2;
              copyState.len = len;
              return true;
            }
            distBits = distanceExtraBits[sym];
            dist = distanceCodes[sym];
            j = distBits === 0 ? 0 : this._readBits(distBits);
            if (j < 0) {
              copyState.state = 3;
              copyState.len = len;
              copyState.dist = dist;
              copyState.distBits = distBits;
              return true;
            }
            dist += j;
            i = pos - dist;
            while (len--) {
              output[pos++] = output[i++];
            }
          } else {
            this._state = 0 /* INIT */;
            break;
          }
        } while(pos < WINDOW_SHIFT_POSITION);
        this._windowPosition = pos;
        return false;
      };

      Inflate.inflate = function (data, expectedLength, zlibHeader) {
        var output = new Uint8Array(expectedLength);
        var position = 0;
        var inflate = new Inflate(zlibHeader);
        inflate.onData = function (data) {
          output.set(data, position);
          position += data.length;
        };
        inflate.push(data);
        return output;
      };
      return Inflate;
    })();
    ArrayUtilities.Inflate = Inflate;

    var codeLengthOrder;
    var distanceCodes;
    var distanceExtraBits;
    var fixedDistanceTable;
    var lengthCodes;
    var lengthExtraBits;
    var fixedLiteralTable;

    var areTablesInitialized = false;

    function initializeTables() {
      codeLengthOrder = new Uint8Array([16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15]);

      distanceCodes = new Uint16Array(30);
      distanceExtraBits = new Uint8Array(30);
      for (var i = 0, j = 0, code = 1; i < 30; ++i) {
        distanceCodes[i] = code;
        code += 1 << (distanceExtraBits[i] = ~~((j += (i > 2 ? 1 : 0)) / 2));
      }

      var bitLengths = new Uint8Array(288);
      for (var i = 0; i < 32; ++i) {
        bitLengths[i] = 5;
      }
      fixedDistanceTable = makeHuffmanTable(bitLengths.subarray(0, 32));

      lengthCodes = new Uint16Array(29);
      lengthExtraBits = new Uint8Array(29);
      for (var i = 0, j = 0, code = 3; i < 29; ++i) {
        lengthCodes[i] = code - (i == 28 ? 1 : 0);
        code += 1 << (lengthExtraBits[i] = ~~(((j += (i > 4 ? 1 : 0)) / 4) % 6));
      }
      for (var i = 0; i < 288; ++i) {
        bitLengths[i] = i < 144 || i > 279 ? 8 : (i < 256 ? 9 : 7);
      }
      fixedLiteralTable = makeHuffmanTable(bitLengths);
    }

    function makeHuffmanTable(bitLengths) {
      var maxBits = Math.max.apply(null, bitLengths);
      var numLengths = bitLengths.length;
      var size = 1 << maxBits;
      var codes = new Uint32Array(size);

      var dummyCode = (maxBits << 16) | 0xFFFF;
      for (var j = 0; j < size; j++) {
        codes[j] = dummyCode;
      }
      for (var code = 0, len = 1, skip = 2; len <= maxBits; code <<= 1, ++len, skip <<= 1) {
        for (var val = 0; val < numLengths; ++val) {
          if (bitLengths[val] === len) {
            var lsb = 0;
            for (var i = 0; i < len; ++i)
              lsb = (lsb * 2) + ((code >> i) & 1);
            for (var i = lsb; i < size; i += skip)
              codes[i] = (len << 16) | val;
            ++code;
          }
        }
      }
      return { codes: codes, maxBits: maxBits };
    }

    var DeflateState;
    (function (DeflateState) {
      DeflateState[DeflateState["WRITE"] = 0] = "WRITE";
      DeflateState[DeflateState["DONE"] = 1] = "DONE";
      DeflateState[DeflateState["ZLIB_HEADER"] = 2] = "ZLIB_HEADER";
    })(DeflateState || (DeflateState = {}));

    var Adler32 = (function () {
      function Adler32() {
        this.a = 1;
        this.b = 0;
      }
      Adler32.prototype.update = function (data, start, end) {
        var a = this.a;
        var b = this.b;
        for (var i = start; i < end; ++i) {
          a = (a + (data[i] & 0xff)) % 65521;
          b = (b + a) % 65521;
        }
        this.a = a;
        this.b = b;
      };

      Adler32.prototype.getChecksum = function () {
        return (this.b << 16) | this.a;
      };
      return Adler32;
    })();
    ArrayUtilities.Adler32 = Adler32;

    var Deflate = (function () {
      function Deflate(writeZlibHeader) {
        this._writeZlibHeader = writeZlibHeader;
        this._state = writeZlibHeader ? 2 /* ZLIB_HEADER */ : 0 /* WRITE */;
        this._adler32 = writeZlibHeader ? new Adler32() : null;
      }
      Deflate.prototype.push = function (data) {
        if (this._state === 2 /* ZLIB_HEADER */) {
          this.onData(new Uint8Array([0x78, 0x9C]));
          this._state = 0 /* WRITE */;
        }

        var len = data.length;
        var outputSize = len + Math.ceil(len / 0xFFFF) * 5;
        var output = new Uint8Array(outputSize);
        var outputPos = 0;
        var pos = 0;
        while (len > 0xFFFF) {
          output.set(new Uint8Array([
            0x00,
            0xFF, 0xFF,
            0x00, 0x00
          ]), outputPos);
          outputPos += 5;
          output.set(data.subarray(pos, pos + 0xFFFF), outputPos);
          pos += 0xFFFF;
          outputPos += 0xFFFF;
          len -= 0xFFFF;
        }

        output.set(new Uint8Array([
          0x00,
          (len & 0xff), ((len >> 8) & 0xff),
          ((~len) & 0xff), (((~len) >> 8) & 0xff)
        ]), outputPos);
        outputPos += 5;
        output.set(data.subarray(pos, len), outputPos);

        this.onData(output);

        if (this._adler32) {
          this._adler32.update(data, 0, len);
        }
      };

      Deflate.prototype.finish = function () {
        this._state = 1 /* DONE */;
        this.onData(new Uint8Array([
          0x01,
          0x00, 0x00,
          0xFF, 0xFF
        ]));
        if (this._adler32) {
          var checksum = this._adler32.getChecksum();
          this.onData(new Uint8Array([
            checksum & 0xff, (checksum >> 8) & 0xff,
            (checksum >> 16) & 0xff, (checksum >>> 24) & 0xff
          ]));
        }
      };
      return Deflate;
    })();
    ArrayUtilities.Deflate = Deflate;
  })(Shumway.ArrayUtilities || (Shumway.ArrayUtilities = {}));
  var ArrayUtilities = Shumway.ArrayUtilities;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (ArrayUtilities) {
    var notImplemented = Shumway.Debug.notImplemented;

    var utf8decode = Shumway.StringUtilities.utf8decode;
    var utf8encode = Shumway.StringUtilities.utf8encode;
    var clamp = Shumway.NumberUtilities.clamp;

    var swap32 = Shumway.IntegerUtilities.swap32;
    var floatToInt32 = Shumway.IntegerUtilities.floatToInt32;
    var int32ToFloat = Shumway.IntegerUtilities.int32ToFloat;

    function throwEOFError() {
      notImplemented("throwEOFError");
    }

    function throwRangeError() {
      notImplemented("throwRangeError");
    }

    function throwCompressedDataError() {
      notImplemented("throwCompressedDataError");
    }

    function checkRange(x, min, max) {
      if (x !== clamp(x, min, max)) {
        throwRangeError();
      }
    }

    function asCoerceString(x) {
      if (typeof x === "string") {
        return x;
      } else if (x == undefined) {
        return null;
      }
      return x + '';
    }

    var PlainObjectDataBuffer = (function () {
      function PlainObjectDataBuffer(buffer, length, littleEndian) {
        this.buffer = buffer;
        this.length = length;
        this.littleEndian = littleEndian;
      }
      return PlainObjectDataBuffer;
    })();
    ArrayUtilities.PlainObjectDataBuffer = PlainObjectDataBuffer;

    var bitMasks = new Uint32Array(33);
    for (var i = 1, mask = 0; i <= 32; i++) {
      bitMasks[i] = mask = (mask << 1) | 1;
    }

    var DataBuffer = (function () {
      function DataBuffer(initialSize) {
        if (typeof initialSize === "undefined") { initialSize = DataBuffer.INITIAL_SIZE; }
        if (this._buffer) {
          return;
        }
        this._buffer = new ArrayBuffer(initialSize);
        this._length = 0;
        this._position = 0;
        this._updateViews();
        this._littleEndian = false;
        this._bitBuffer = 0;
        this._bitLength = 0;
      }
      DataBuffer.FromArrayBuffer = function (buffer, length) {
        if (typeof length === "undefined") { length = -1; }
        var dataBuffer = Object.create(DataBuffer.prototype);
        dataBuffer._buffer = buffer;
        dataBuffer._length = length === -1 ? buffer.byteLength : length;
        dataBuffer._position = 0;
        dataBuffer._updateViews();
        dataBuffer._littleEndian = false;
        dataBuffer._bitBuffer = 0;
        dataBuffer._bitLength = 0;
        return dataBuffer;
      };

      DataBuffer.FromPlainObject = function (source) {
        var dataBuffer = DataBuffer.FromArrayBuffer(source.buffer, source.length);
        dataBuffer._littleEndian = source.littleEndian;
        return dataBuffer;
      };

      DataBuffer.prototype.toPlainObject = function () {
        return new PlainObjectDataBuffer(this._buffer, this._length, this._littleEndian);
      };

      DataBuffer.prototype._get = function (m, size) {
        if (this._position + size > this._length) {
          throwEOFError();
        }
        var v = this._dataView[m](this._position, this._littleEndian);
        this._position += size;
        return v;
      };

      DataBuffer.prototype._set = function (m, size, v) {
        var length = this._position + size;
        this._ensureCapacity(length);
        this._dataView[m](this._position, v, this._littleEndian);
        this._position = length;
        if (length > this._length) {
          this._length = length;
        }
      };

      DataBuffer.prototype._updateViews = function () {
        this._i8View = new Int8Array(this._buffer);
        this._u8View = new Uint8Array(this._buffer);
        if ((this._buffer.byteLength & 0x3) === 0) {
          this._i32View = new Int32Array(this._buffer);
        }
        this._dataView = new DataView(this._buffer);
      };

      DataBuffer.prototype.getBytes = function () {
        return new Uint8Array(this._buffer, 0, this._length);
      };

      DataBuffer.prototype._ensureCapacity = function (length) {
        var currentBuffer = this._buffer;
        if (currentBuffer.byteLength < length) {
          var newLength = Math.max(currentBuffer.byteLength, 1);
          while (newLength < length) {
            newLength *= 2;
          }
          var newBuffer = DataBuffer._arrayBufferPool.acquire(newLength);
          var curentView = this._i8View;
          this._buffer = newBuffer;
          this._updateViews();
          this._i8View.set(curentView);
          DataBuffer._arrayBufferPool.release(currentBuffer);
        }
      };

      DataBuffer.prototype.clear = function () {
        this._length = 0;
        this._position = 0;
      };

      DataBuffer.prototype.readBoolean = function () {
        if (this._position + 1 > this._length) {
          throwEOFError();
        }
        return this._i8View[this._position++] !== 0;
      };

      DataBuffer.prototype.readByte = function () {
        if (this._position + 1 > this._length) {
          throwEOFError();
        }
        return this._i8View[this._position++];
      };

      DataBuffer.prototype.readUnsignedByte = function () {
        if (this._position + 1 > this._length) {
          throwEOFError();
        }
        return this._u8View[this._position++];
      };

      DataBuffer.prototype.readBytes = function (bytes, offset, length) {
        if (typeof offset === "undefined") { offset = 0; }
        if (typeof length === "undefined") { length = 0; }
        var pos = this._position;
        if (!offset) {
          offset = 0;
        }
        if (!length) {
          length = this._length - pos;
        }
        if (pos + length > this._length) {
          throwEOFError();
        }
        if (bytes.length < offset + length) {
          bytes._ensureCapacity(offset + length);
          bytes.length = offset + length;
        }
        bytes._i8View.set(new Int8Array(this._buffer, pos, length), offset);
        this._position += length;
      };

      DataBuffer.prototype.readShort = function () {
        return this._get('getInt16', 2);
      };

      DataBuffer.prototype.readUnsignedShort = function () {
        return this._get('getUint16', 2);
      };

      DataBuffer.prototype.readInt = function () {
        if ((this._position & 0x3) === 0 && this._i32View) {
          if (this._position + 4 > this._length) {
            throwEOFError();
          }
          var value = this._i32View[this._position >> 2];
          this._position += 4;
          if (this._littleEndian !== DataBuffer._nativeLittleEndian) {
            value = swap32(value);
          }
          return value;
        } else {
          return this._get('getInt32', 4);
        }
      };

      DataBuffer.prototype.readUnsignedInt = function () {
        return this._get('getUint32', 4);
      };

      DataBuffer.prototype.readFloat = function () {
        if ((this._position & 0x3) === 0 && this._i32View) {
          if (this._position + 4 > this._length) {
            throwEOFError();
          }
          var bytes = this._i32View[this._position >> 2];
          if (this._littleEndian !== DataBuffer._nativeLittleEndian) {
            bytes = swap32(bytes);
          }
          var value = int32ToFloat(bytes);
          this._position += 4;
          return value;
        } else {
          return this._get('getFloat32', 4);
        }
      };

      DataBuffer.prototype.readDouble = function () {
        return this._get('getFloat64', 8);
      };

      DataBuffer.prototype.writeBoolean = function (value) {
        value = !!value;
        var length = this._position + 1;
        this._ensureCapacity(length);
        this._i8View[this._position++] = value ? 1 : 0;
        if (length > this._length) {
          this._length = length;
        }
      };

      DataBuffer.prototype.writeByte = function (value) {
        var length = this._position + 1;
        this._ensureCapacity(length);
        this._i8View[this._position++] = value;
        if (length > this._length) {
          this._length = length;
        }
      };

      DataBuffer.prototype.writeUnsignedByte = function (value) {
        var length = this._position + 1;
        this._ensureCapacity(length);
        this._u8View[this._position++] = value;
        if (length > this._length) {
          this._length = length;
        }
      };

      DataBuffer.prototype.writeRawBytes = function (bytes) {
        var length = this._position + bytes.length;
        this._ensureCapacity(length);
        this._i8View.set(bytes, this._position);
        this._position = length;
        if (length > this._length) {
          this._length = length;
        }
      };

      DataBuffer.prototype.writeBytes = function (bytes, offset, length) {
        if (typeof offset === "undefined") { offset = 0; }
        if (typeof length === "undefined") { length = 0; }
        if (arguments.length < 2) {
          offset = 0;
        }
        if (arguments.length < 3) {
          length = 0;
        }
        checkRange(offset, 0, bytes.length);
        checkRange(offset + length, 0, bytes.length);
        if (length === 0) {
          length = bytes.length - offset;
        }
        this.writeRawBytes(new Int8Array(bytes._buffer, offset, length));
      };

      DataBuffer.prototype.writeShort = function (value) {
        this._set('setInt16', 2, value);
      };

      DataBuffer.prototype.writeUnsignedShort = function (value) {
        this._set('setUint16', 2, value);
      };

      DataBuffer.prototype.writeInt = function (value) {
        if ((this._position & 0x3) === 0 && this._i32View) {
          if (this._littleEndian !== DataBuffer._nativeLittleEndian) {
            value = swap32(value);
          }
          var length = this._position + 4;
          this._ensureCapacity(length);
          this._i32View[this._position >> 2] = value;
          this._position += 4;
          if (length > this._length) {
            this._length = length;
          }
        } else {
          this._set('setInt32', 4, value);
        }
      };

      DataBuffer.prototype.writeUnsignedInt = function (value) {
        this._set('setUint32', 4, value);
      };

      DataBuffer.prototype.writeFloat = function (value) {
        if ((this._position & 0x3) === 0 && this._i32View) {
          var length = this._position + 4;
          this._ensureCapacity(length);
          var bytes = floatToInt32(value);
          if (this._littleEndian !== DataBuffer._nativeLittleEndian) {
            bytes = swap32(bytes);
          }
          this._i32View[this._position >> 2] = bytes;
          this._position += 4;
          if (length > this._length) {
            this._length = length;
          }
        } else {
          this._set('setFloat32', 4, value);
        }
      };

      DataBuffer.prototype.writeDouble = function (value) {
        this._set('setFloat64', 8, value);
      };

      DataBuffer.prototype.readRawBytes = function () {
        return new Int8Array(this._buffer, 0, this._length);
      };

      DataBuffer.prototype.writeUTF = function (value) {
        value = asCoerceString(value);
        var bytes = utf8decode(value);
        this.writeShort(bytes.length);
        this.writeRawBytes(bytes);
      };

      DataBuffer.prototype.writeUTFBytes = function (value) {
        value = asCoerceString(value);
        var bytes = utf8decode(value);
        this.writeRawBytes(bytes);
      };

      DataBuffer.prototype.readUTF = function () {
        return this.readUTFBytes(this.readShort());
      };

      DataBuffer.prototype.readUTFBytes = function (length) {
        length = length >>> 0;
        var pos = this._position;
        if (pos + length > this._length) {
          throwEOFError();
        }
        this._position += length;
        return utf8encode(new Int8Array(this._buffer, pos, length));
      };

      Object.defineProperty(DataBuffer.prototype, "length", {
        get: function () {
          return this._length;
        },
        set: function (value) {
          value = value >>> 0;
          var capacity = this._buffer.byteLength;

          if (value > capacity) {
            this._ensureCapacity(value);
          }
          this._length = value;
          this._position = clamp(this._position, 0, this._length);
        },
        enumerable: true,
        configurable: true
      });


      Object.defineProperty(DataBuffer.prototype, "bytesAvailable", {
        get: function () {
          return this._length - this._position;
        },
        enumerable: true,
        configurable: true
      });

      Object.defineProperty(DataBuffer.prototype, "position", {
        get: function () {
          return this._position;
        },
        set: function (position) {
          this._position = position >>> 0;
        },
        enumerable: true,
        configurable: true
      });

      Object.defineProperty(DataBuffer.prototype, "buffer", {
        get: function () {
          return this._buffer;
        },
        enumerable: true,
        configurable: true
      });

      Object.defineProperty(DataBuffer.prototype, "bytes", {
        get: function () {
          return this._u8View;
        },
        enumerable: true,
        configurable: true
      });

      Object.defineProperty(DataBuffer.prototype, "ints", {
        get: function () {
          return this._i32View;
        },
        enumerable: true,
        configurable: true
      });


      Object.defineProperty(DataBuffer.prototype, "objectEncoding", {
        get: function () {
          return this._objectEncoding;
        },
        set: function (version) {
          version = version >>> 0;
          this._objectEncoding = version;
        },
        enumerable: true,
        configurable: true
      });


      Object.defineProperty(DataBuffer.prototype, "endian", {
        get: function () {
          return this._littleEndian ? "littleEndian" : "bigEndian";
        },
        set: function (type) {
          type = asCoerceString(type);
          if (type === "auto") {
            this._littleEndian = DataBuffer._nativeLittleEndian;
          } else {
            this._littleEndian = type === "littleEndian";
          }
        },
        enumerable: true,
        configurable: true
      });


      DataBuffer.prototype.toString = function () {
        return utf8encode(new Int8Array(this._buffer, 0, this._length));
      };

      DataBuffer.prototype.toBlob = function () {
        return new Blob([new Int8Array(this._buffer, this._position, this._length)]);
      };

      DataBuffer.prototype.writeMultiByte = function (value, charSet) {
        value = asCoerceString(value);
        charSet = asCoerceString(charSet);
        notImplemented("packageInternal flash.utils.ObjectOutput::writeMultiByte");
        return;
      };

      DataBuffer.prototype.readMultiByte = function (length, charSet) {
        length = length >>> 0;
        charSet = asCoerceString(charSet);
        notImplemented("packageInternal flash.utils.ObjectInput::readMultiByte");
        return;
      };

      DataBuffer.prototype.getValue = function (name) {
        name = name | 0;
        if (name >= this._length) {
          return undefined;
        }
        return this._u8View[name];
      };

      DataBuffer.prototype.setValue = function (name, value) {
        name = name | 0;
        var length = name + 1;
        this._ensureCapacity(length);
        this._u8View[name] = value;
        if (length > this._length) {
          this._length = length;
        }
      };

      DataBuffer.prototype.readFixed = function () {
        return this.readInt() / 65536;
      };

      DataBuffer.prototype.readFixed8 = function () {
        return this.readShort() / 256;
      };

      DataBuffer.prototype.readFloat16 = function () {
        var uint16 = this.readUnsignedShort();
        var sign = uint16 >> 15 ? -1 : 1;
        var exponent = (uint16 & 0x7c00) >> 10;
        var fraction = uint16 & 0x03ff;
        if (!exponent) {
          return sign * Math.pow(2, -14) * (fraction / 1024);
        }
        if (exponent === 0x1f) {
          return fraction ? NaN : sign * Infinity;
        }
        return sign * Math.pow(2, exponent - 15) * (1 + (fraction / 1024));
      };

      DataBuffer.prototype.readEncodedU32 = function () {
        var value = this.readUnsignedByte();
        if (!(value & 0x080)) {
          return value;
        }
        value |= this.readUnsignedByte() << 7;
        if (!(value & 0x4000)) {
          return value;
        }
        value |= this.readUnsignedByte() << 14;
        if (!(value & 0x200000)) {
          return value;
        }
        value |= this.readUnsignedByte() << 21;
        if (!(value & 0x10000000)) {
          return value;
        }
        return value | (this.readUnsignedByte() << 28);
      };

      DataBuffer.prototype.readBits = function (size) {
        return (this.readUnsignedBits(size) << (32 - size)) >> (32 - size);
      };

      DataBuffer.prototype.readUnsignedBits = function (size) {
        var buffer = this._bitBuffer;
        var length = this._bitLength;
        while (size > length) {
          buffer = (buffer << 8) | this.readUnsignedByte();
          length += 8;
        }
        length -= size;
        var value = (buffer >>> length) & bitMasks[size];
        this._bitBuffer = buffer;
        this._bitLength = length;
        return value;
      };

      DataBuffer.prototype.readFixedBits = function (size) {
        return this.readBits(size) / 65536;
      };

      DataBuffer.prototype.readString = function (length) {
        var position = this._position;
        if (length) {
          if (position + length > this._length) {
            throwEOFError();
          }
          this._position += length;
        } else {
          length = 0;
          for (var i = position; i < this._length && this._u8View[i]; i++) {
            length++;
          }
          this._position += length + 1;
        }
        return utf8encode(new Int8Array(this._buffer, position, length));
      };

      DataBuffer.prototype.align = function () {
        this._bitBuffer = 0;
        this._bitLength = 0;
      };

      DataBuffer.prototype._compress = function (algorithm) {
        algorithm = asCoerceString(algorithm);

        var deflate;
        switch (algorithm) {
          case 'zlib':
            deflate = new ArrayUtilities.Deflate(true);
            break;
          case 'deflate':
            deflate = new ArrayUtilities.Deflate(false);
            break;
          default:
            return;
        }

        var output = new DataBuffer();
        deflate.onData = output.writeRawBytes.bind(output);
        deflate.push(this._u8View.subarray(0, this._length));
        deflate.finish();

        this._ensureCapacity(output._u8View.length);
        this._u8View.set(output._u8View);
        this.length = output.length;
        this._position = 0;
      };

      DataBuffer.prototype._uncompress = function (algorithm) {
        algorithm = asCoerceString(algorithm);

        var inflate;
        switch (algorithm) {
          case 'zlib':
            inflate = new ArrayUtilities.Inflate(true);
            break;
          case 'deflate':
            inflate = new ArrayUtilities.Inflate(false);
            break;
          default:
            return;
        }

        var output = new DataBuffer();
        inflate.onData = output.writeRawBytes.bind(output);
        inflate.push(this._u8View.subarray(0, this._length));
        if (inflate.error) {
          throwCompressedDataError();
        }

        this._ensureCapacity(output._u8View.length);
        this._u8View.set(output._u8View);
        this.length = output.length;
        this._position = 0;
      };
      DataBuffer._nativeLittleEndian = new Int8Array(new Int32Array([1]).buffer)[0] === 1;

      DataBuffer.INITIAL_SIZE = 128;

      DataBuffer._arrayBufferPool = new Shumway.ArrayBufferPool();
      return DataBuffer;
    })();
    ArrayUtilities.DataBuffer = DataBuffer;
  })(Shumway.ArrayUtilities || (Shumway.ArrayUtilities = {}));
  var ArrayUtilities = Shumway.ArrayUtilities;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  var DataBuffer = Shumway.ArrayUtilities.DataBuffer;
  var ensureTypedArrayCapacity = Shumway.ArrayUtilities.ensureTypedArrayCapacity;

  var assert = Shumway.Debug.assert;

  (function (PathCommand) {
    PathCommand[PathCommand["BeginSolidFill"] = 1] = "BeginSolidFill";
    PathCommand[PathCommand["BeginGradientFill"] = 2] = "BeginGradientFill";
    PathCommand[PathCommand["BeginBitmapFill"] = 3] = "BeginBitmapFill";
    PathCommand[PathCommand["EndFill"] = 4] = "EndFill";
    PathCommand[PathCommand["LineStyleSolid"] = 5] = "LineStyleSolid";
    PathCommand[PathCommand["LineStyleGradient"] = 6] = "LineStyleGradient";
    PathCommand[PathCommand["LineStyleBitmap"] = 7] = "LineStyleBitmap";
    PathCommand[PathCommand["LineEnd"] = 8] = "LineEnd";
    PathCommand[PathCommand["MoveTo"] = 9] = "MoveTo";
    PathCommand[PathCommand["LineTo"] = 10] = "LineTo";
    PathCommand[PathCommand["CurveTo"] = 11] = "CurveTo";
    PathCommand[PathCommand["CubicCurveTo"] = 12] = "CubicCurveTo";
  })(Shumway.PathCommand || (Shumway.PathCommand = {}));
  var PathCommand = Shumway.PathCommand;

  (function (GradientType) {
    GradientType[GradientType["Linear"] = 0x10] = "Linear";
    GradientType[GradientType["Radial"] = 0x12] = "Radial";
  })(Shumway.GradientType || (Shumway.GradientType = {}));
  var GradientType = Shumway.GradientType;

  (function (GradientSpreadMethod) {
    GradientSpreadMethod[GradientSpreadMethod["Pad"] = 0] = "Pad";
    GradientSpreadMethod[GradientSpreadMethod["Reflect"] = 1] = "Reflect";
    GradientSpreadMethod[GradientSpreadMethod["Repeat"] = 2] = "Repeat";
  })(Shumway.GradientSpreadMethod || (Shumway.GradientSpreadMethod = {}));
  var GradientSpreadMethod = Shumway.GradientSpreadMethod;

  (function (GradientInterpolationMethod) {
    GradientInterpolationMethod[GradientInterpolationMethod["RGB"] = 0] = "RGB";
    GradientInterpolationMethod[GradientInterpolationMethod["LinearRGB"] = 1] = "LinearRGB";
  })(Shumway.GradientInterpolationMethod || (Shumway.GradientInterpolationMethod = {}));
  var GradientInterpolationMethod = Shumway.GradientInterpolationMethod;

  var PlainObjectShapeData = (function () {
    function PlainObjectShapeData(commands, commandsPosition, coordinates, coordinatesPosition, morphCoordinates, styles, stylesLength) {
      this.commands = commands;
      this.commandsPosition = commandsPosition;
      this.coordinates = coordinates;
      this.coordinatesPosition = coordinatesPosition;
      this.morphCoordinates = morphCoordinates;
      this.styles = styles;
      this.stylesLength = stylesLength;
    }
    return PlainObjectShapeData;
  })();
  Shumway.PlainObjectShapeData = PlainObjectShapeData;

  var DefaultSize;
  (function (DefaultSize) {
    DefaultSize[DefaultSize["Commands"] = 32] = "Commands";
    DefaultSize[DefaultSize["Coordinates"] = 128] = "Coordinates";
    DefaultSize[DefaultSize["Styles"] = 16] = "Styles";
  })(DefaultSize || (DefaultSize = {}));

  var ShapeData = (function () {
    function ShapeData(initialize) {
      if (typeof initialize === "undefined") { initialize = true; }
      if (initialize) {
        this.clear();
      }
    }
    ShapeData.FromPlainObject = function (source) {
      var data = new ShapeData(false);
      data.commands = source.commands;
      data.coordinates = source.coordinates;
      data.morphCoordinates = source.morphCoordinates;
      data.commandsPosition = source.commandsPosition;
      data.coordinatesPosition = source.coordinatesPosition;
      data.styles = DataBuffer.FromArrayBuffer(source.styles, source.stylesLength);
      data.styles.endian = 'auto';
      return data;
    };

    ShapeData.prototype.moveTo = function (x, y) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 9 /* MoveTo */;
      this.coordinates[this.coordinatesPosition++] = x;
      this.coordinates[this.coordinatesPosition++] = y;
    };

    ShapeData.prototype.lineTo = function (x, y) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = 10 /* LineTo */;
      this.coordinates[this.coordinatesPosition++] = x;
      this.coordinates[this.coordinatesPosition++] = y;
    };

    ShapeData.prototype.curveTo = function (controlX, controlY, anchorX, anchorY) {
      this.ensurePathCapacities(1, 4);
      this.commands[this.commandsPosition++] = 11 /* CurveTo */;
      this.coordinates[this.coordinatesPosition++] = controlX;
      this.coordinates[this.coordinatesPosition++] = controlY;
      this.coordinates[this.coordinatesPosition++] = anchorX;
      this.coordinates[this.coordinatesPosition++] = anchorY;
    };

    ShapeData.prototype.cubicCurveTo = function (controlX1, controlY1, controlX2, controlY2, anchorX, anchorY) {
      this.ensurePathCapacities(1, 6);
      this.commands[this.commandsPosition++] = 12 /* CubicCurveTo */;
      this.coordinates[this.coordinatesPosition++] = controlX1;
      this.coordinates[this.coordinatesPosition++] = controlY1;
      this.coordinates[this.coordinatesPosition++] = controlX2;
      this.coordinates[this.coordinatesPosition++] = controlY2;
      this.coordinates[this.coordinatesPosition++] = anchorX;
      this.coordinates[this.coordinatesPosition++] = anchorY;
    };

    ShapeData.prototype.beginFill = function (color) {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 1 /* BeginSolidFill */;
      this.styles.writeUnsignedInt(color);
    };

    ShapeData.prototype.endFill = function () {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 4 /* EndFill */;
    };

    ShapeData.prototype.endLine = function () {
      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = 8 /* LineEnd */;
    };

    ShapeData.prototype.lineStyle = function (thickness, color, pixelHinting, scaleMode, caps, joints, miterLimit) {
      release || assert(thickness === (thickness | 0), thickness >= 0 && thickness <= 0xff * 20);
      this.ensurePathCapacities(2, 0);
      this.commands[this.commandsPosition++] = 5 /* LineStyleSolid */;
      this.coordinates[this.coordinatesPosition++] = thickness;
      var styles = this.styles;
      styles.writeUnsignedInt(color);
      styles.writeBoolean(pixelHinting);
      styles.writeUnsignedByte(scaleMode);
      styles.writeUnsignedByte(caps);
      styles.writeUnsignedByte(joints);
      styles.writeUnsignedByte(miterLimit);
    };

    ShapeData.prototype.beginBitmap = function (pathCommand, bitmapId, matrix, repeat, smooth) {
      release || assert(pathCommand === 3 /* BeginBitmapFill */ || pathCommand === 7 /* LineStyleBitmap */);

      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = pathCommand;
      var styles = this.styles;
      styles.writeUnsignedInt(bitmapId);
      this._writeStyleMatrix(matrix);
      styles.writeBoolean(repeat);
      styles.writeBoolean(smooth);
    };

    ShapeData.prototype.beginGradient = function (pathCommand, colors, ratios, gradientType, matrix, spread, interpolation, focalPointRatio) {
      release || assert(pathCommand === 2 /* BeginGradientFill */ || pathCommand === 6 /* LineStyleGradient */);

      this.ensurePathCapacities(1, 0);
      this.commands[this.commandsPosition++] = pathCommand;
      var styles = this.styles;
      styles.writeUnsignedByte(gradientType);
      release || assert(focalPointRatio === (focalPointRatio | 0));
      styles.writeShort(focalPointRatio);

      this._writeStyleMatrix(matrix);

      var colorStops = colors.length;
      styles.writeByte(colorStops);
      for (var i = 0; i < colorStops; i++) {
        styles.writeUnsignedByte(ratios[i]);

        styles.writeUnsignedInt(colors[i]);
      }

      styles.writeUnsignedByte(spread);
      styles.writeUnsignedByte(interpolation);
    };

    ShapeData.prototype.writeCommandAndCoordinates = function (command, x, y) {
      this.ensurePathCapacities(1, 2);
      this.commands[this.commandsPosition++] = command;
      this.coordinates[this.coordinatesPosition++] = x;
      this.coordinates[this.coordinatesPosition++] = y;
    };

    ShapeData.prototype.writeCoordinates = function (x, y) {
      this.ensurePathCapacities(0, 2);
      this.coordinates[this.coordinatesPosition++] = x;
      this.coordinates[this.coordinatesPosition++] = y;
    };

    ShapeData.prototype.writeMorphCoordinates = function (x, y) {
      this.morphCoordinates = ensureTypedArrayCapacity(this.morphCoordinates, this.coordinatesPosition);
      this.morphCoordinates[this.coordinatesPosition - 2] = x;
      this.morphCoordinates[this.coordinatesPosition - 1] = y;
    };

    ShapeData.prototype.clear = function () {
      this.commandsPosition = this.coordinatesPosition = 0;
      this.commands = new Uint8Array(32 /* Commands */);
      this.coordinates = new Int32Array(128 /* Coordinates */);
      this.styles = new DataBuffer(16 /* Styles */);
      this.styles.endian = 'auto';
    };

    ShapeData.prototype.isEmpty = function () {
      return this.commandsPosition === 0;
    };

    ShapeData.prototype.clone = function () {
      var copy = new ShapeData(false);
      copy.commands = new Uint8Array(this.commands);
      copy.commandsPosition = this.commandsPosition;
      copy.coordinates = new Int32Array(this.coordinates);
      copy.coordinatesPosition = this.coordinatesPosition;
      copy.styles = new DataBuffer(this.styles.length);
      copy.styles.writeRawBytes(this.styles.bytes);
      return copy;
    };

    ShapeData.prototype.toPlainObject = function () {
      return new PlainObjectShapeData(this.commands, this.commandsPosition, this.coordinates, this.coordinatesPosition, this.morphCoordinates, this.styles.buffer, this.styles.length);
    };

    Object.defineProperty(ShapeData.prototype, "buffers", {
      get: function () {
        var buffers = [this.commands.buffer, this.coordinates.buffer, this.styles.buffer];
        if (this.morphCoordinates) {
          buffers.push(this.morphCoordinates.buffer);
        }
        return buffers;
      },
      enumerable: true,
      configurable: true
    });

    ShapeData.prototype._writeStyleMatrix = function (matrix) {
      var styles = this.styles;
      styles.writeFloat(matrix.a);
      styles.writeFloat(matrix.b);
      styles.writeFloat(matrix.c);
      styles.writeFloat(matrix.d);
      styles.writeFloat(matrix.tx);
      styles.writeFloat(matrix.ty);
    };

    ShapeData.prototype.ensurePathCapacities = function (numCommands, numCoordinates) {
      this.commands = ensureTypedArrayCapacity(this.commands, this.commandsPosition + numCommands);
      this.coordinates = ensureTypedArrayCapacity(this.coordinates, this.coordinatesPosition + numCoordinates);
    };
    return ShapeData;
  })();
  Shumway.ShapeData = ShapeData;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      (function (SwfTag) {
        SwfTag[SwfTag["CODE_END"] = 0] = "CODE_END";
        SwfTag[SwfTag["CODE_SHOW_FRAME"] = 1] = "CODE_SHOW_FRAME";
        SwfTag[SwfTag["CODE_DEFINE_SHAPE"] = 2] = "CODE_DEFINE_SHAPE";
        SwfTag[SwfTag["CODE_FREE_CHARACTER"] = 3] = "CODE_FREE_CHARACTER";
        SwfTag[SwfTag["CODE_PLACE_OBJECT"] = 4] = "CODE_PLACE_OBJECT";
        SwfTag[SwfTag["CODE_REMOVE_OBJECT"] = 5] = "CODE_REMOVE_OBJECT";
        SwfTag[SwfTag["CODE_DEFINE_BITS"] = 6] = "CODE_DEFINE_BITS";
        SwfTag[SwfTag["CODE_DEFINE_BUTTON"] = 7] = "CODE_DEFINE_BUTTON";
        SwfTag[SwfTag["CODE_JPEG_TABLES"] = 8] = "CODE_JPEG_TABLES";
        SwfTag[SwfTag["CODE_SET_BACKGROUND_COLOR"] = 9] = "CODE_SET_BACKGROUND_COLOR";
        SwfTag[SwfTag["CODE_DEFINE_FONT"] = 10] = "CODE_DEFINE_FONT";
        SwfTag[SwfTag["CODE_DEFINE_TEXT"] = 11] = "CODE_DEFINE_TEXT";
        SwfTag[SwfTag["CODE_DO_ACTION"] = 12] = "CODE_DO_ACTION";
        SwfTag[SwfTag["CODE_DEFINE_FONT_INFO"] = 13] = "CODE_DEFINE_FONT_INFO";
        SwfTag[SwfTag["CODE_DEFINE_SOUND"] = 14] = "CODE_DEFINE_SOUND";
        SwfTag[SwfTag["CODE_START_SOUND"] = 15] = "CODE_START_SOUND";
        SwfTag[SwfTag["CODE_STOP_SOUND"] = 16] = "CODE_STOP_SOUND";
        SwfTag[SwfTag["CODE_DEFINE_BUTTON_SOUND"] = 17] = "CODE_DEFINE_BUTTON_SOUND";
        SwfTag[SwfTag["CODE_SOUND_STREAM_HEAD"] = 18] = "CODE_SOUND_STREAM_HEAD";
        SwfTag[SwfTag["CODE_SOUND_STREAM_BLOCK"] = 19] = "CODE_SOUND_STREAM_BLOCK";
        SwfTag[SwfTag["CODE_DEFINE_BITS_LOSSLESS"] = 20] = "CODE_DEFINE_BITS_LOSSLESS";
        SwfTag[SwfTag["CODE_DEFINE_BITS_JPEG2"] = 21] = "CODE_DEFINE_BITS_JPEG2";
        SwfTag[SwfTag["CODE_DEFINE_SHAPE2"] = 22] = "CODE_DEFINE_SHAPE2";
        SwfTag[SwfTag["CODE_DEFINE_BUTTON_CXFORM"] = 23] = "CODE_DEFINE_BUTTON_CXFORM";
        SwfTag[SwfTag["CODE_PROTECT"] = 24] = "CODE_PROTECT";
        SwfTag[SwfTag["CODE_PATHS_ARE_POSTSCRIPT"] = 25] = "CODE_PATHS_ARE_POSTSCRIPT";
        SwfTag[SwfTag["CODE_PLACE_OBJECT2"] = 26] = "CODE_PLACE_OBJECT2";

        SwfTag[SwfTag["CODE_REMOVE_OBJECT2"] = 28] = "CODE_REMOVE_OBJECT2";
        SwfTag[SwfTag["CODE_SYNC_FRAME"] = 29] = "CODE_SYNC_FRAME";

        SwfTag[SwfTag["CODE_FREE_ALL"] = 31] = "CODE_FREE_ALL";
        SwfTag[SwfTag["CODE_DEFINE_SHAPE3"] = 32] = "CODE_DEFINE_SHAPE3";
        SwfTag[SwfTag["CODE_DEFINE_TEXT2"] = 33] = "CODE_DEFINE_TEXT2";
        SwfTag[SwfTag["CODE_DEFINE_BUTTON2"] = 34] = "CODE_DEFINE_BUTTON2";
        SwfTag[SwfTag["CODE_DEFINE_BITS_JPEG3"] = 35] = "CODE_DEFINE_BITS_JPEG3";
        SwfTag[SwfTag["CODE_DEFINE_BITS_LOSSLESS2"] = 36] = "CODE_DEFINE_BITS_LOSSLESS2";
        SwfTag[SwfTag["CODE_DEFINE_EDIT_TEXT"] = 37] = "CODE_DEFINE_EDIT_TEXT";
        SwfTag[SwfTag["CODE_DEFINE_VIDEO"] = 38] = "CODE_DEFINE_VIDEO";
        SwfTag[SwfTag["CODE_DEFINE_SPRITE"] = 39] = "CODE_DEFINE_SPRITE";
        SwfTag[SwfTag["CODE_NAME_CHARACTER"] = 40] = "CODE_NAME_CHARACTER";
        SwfTag[SwfTag["CODE_PRODUCT_INFO"] = 41] = "CODE_PRODUCT_INFO";
        SwfTag[SwfTag["CODE_DEFINE_TEXT_FORMAT"] = 42] = "CODE_DEFINE_TEXT_FORMAT";
        SwfTag[SwfTag["CODE_FRAME_LABEL"] = 43] = "CODE_FRAME_LABEL";
        SwfTag[SwfTag["CODE_DEFINE_BEHAVIOUR"] = 44] = "CODE_DEFINE_BEHAVIOUR";
        SwfTag[SwfTag["CODE_SOUND_STREAM_HEAD2"] = 45] = "CODE_SOUND_STREAM_HEAD2";
        SwfTag[SwfTag["CODE_DEFINE_MORPH_SHAPE"] = 46] = "CODE_DEFINE_MORPH_SHAPE";
        SwfTag[SwfTag["CODE_FRAME_TAG"] = 47] = "CODE_FRAME_TAG";
        SwfTag[SwfTag["CODE_DEFINE_FONT2"] = 48] = "CODE_DEFINE_FONT2";
        SwfTag[SwfTag["CODE_GEN_COMMAND"] = 49] = "CODE_GEN_COMMAND";
        SwfTag[SwfTag["CODE_DEFINE_COMMAND_OBJ"] = 50] = "CODE_DEFINE_COMMAND_OBJ";
        SwfTag[SwfTag["CODE_CHARACTER_SET"] = 51] = "CODE_CHARACTER_SET";
        SwfTag[SwfTag["CODE_FONT_REF"] = 52] = "CODE_FONT_REF";
        SwfTag[SwfTag["CODE_DEFINE_FUNCTION"] = 53] = "CODE_DEFINE_FUNCTION";
        SwfTag[SwfTag["CODE_PLACE_FUNCTION"] = 54] = "CODE_PLACE_FUNCTION";
        SwfTag[SwfTag["CODE_GEN_TAG_OBJECTS"] = 55] = "CODE_GEN_TAG_OBJECTS";
        SwfTag[SwfTag["CODE_EXPORT_ASSETS"] = 56] = "CODE_EXPORT_ASSETS";
        SwfTag[SwfTag["CODE_IMPORT_ASSETS"] = 57] = "CODE_IMPORT_ASSETS";
        SwfTag[SwfTag["CODE_ENABLE_DEBUGGER"] = 58] = "CODE_ENABLE_DEBUGGER";
        SwfTag[SwfTag["CODE_DO_INIT_ACTION"] = 59] = "CODE_DO_INIT_ACTION";
        SwfTag[SwfTag["CODE_DEFINE_VIDEO_STREAM"] = 60] = "CODE_DEFINE_VIDEO_STREAM";
        SwfTag[SwfTag["CODE_VIDEO_FRAME"] = 61] = "CODE_VIDEO_FRAME";
        SwfTag[SwfTag["CODE_DEFINE_FONT_INFO2"] = 62] = "CODE_DEFINE_FONT_INFO2";
        SwfTag[SwfTag["CODE_DEBUG_ID"] = 63] = "CODE_DEBUG_ID";
        SwfTag[SwfTag["CODE_ENABLE_DEBUGGER2"] = 64] = "CODE_ENABLE_DEBUGGER2";
        SwfTag[SwfTag["CODE_SCRIPT_LIMITS"] = 65] = "CODE_SCRIPT_LIMITS";
        SwfTag[SwfTag["CODE_SET_TAB_INDEX"] = 66] = "CODE_SET_TAB_INDEX";

        SwfTag[SwfTag["CODE_FILE_ATTRIBUTES"] = 69] = "CODE_FILE_ATTRIBUTES";
        SwfTag[SwfTag["CODE_PLACE_OBJECT3"] = 70] = "CODE_PLACE_OBJECT3";
        SwfTag[SwfTag["CODE_IMPORT_ASSETS2"] = 71] = "CODE_IMPORT_ASSETS2";
        SwfTag[SwfTag["CODE_DO_ABC_"] = 72] = "CODE_DO_ABC_";
        SwfTag[SwfTag["CODE_DEFINE_FONT_ALIGN_ZONES"] = 73] = "CODE_DEFINE_FONT_ALIGN_ZONES";
        SwfTag[SwfTag["CODE_CSM_TEXT_SETTINGS"] = 74] = "CODE_CSM_TEXT_SETTINGS";
        SwfTag[SwfTag["CODE_DEFINE_FONT3"] = 75] = "CODE_DEFINE_FONT3";
        SwfTag[SwfTag["CODE_SYMBOL_CLASS"] = 76] = "CODE_SYMBOL_CLASS";
        SwfTag[SwfTag["CODE_METADATA"] = 77] = "CODE_METADATA";
        SwfTag[SwfTag["CODE_DEFINE_SCALING_GRID"] = 78] = "CODE_DEFINE_SCALING_GRID";

        SwfTag[SwfTag["CODE_DO_ABC"] = 82] = "CODE_DO_ABC";
        SwfTag[SwfTag["CODE_DEFINE_SHAPE4"] = 83] = "CODE_DEFINE_SHAPE4";
        SwfTag[SwfTag["CODE_DEFINE_MORPH_SHAPE2"] = 84] = "CODE_DEFINE_MORPH_SHAPE2";

        SwfTag[SwfTag["CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA"] = 86] = "CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA";
        SwfTag[SwfTag["CODE_DEFINE_BINARY_DATA"] = 87] = "CODE_DEFINE_BINARY_DATA";
        SwfTag[SwfTag["CODE_DEFINE_FONT_NAME"] = 88] = "CODE_DEFINE_FONT_NAME";
        SwfTag[SwfTag["CODE_START_SOUND2"] = 89] = "CODE_START_SOUND2";
        SwfTag[SwfTag["CODE_DEFINE_BITS_JPEG4"] = 90] = "CODE_DEFINE_BITS_JPEG4";
        SwfTag[SwfTag["CODE_DEFINE_FONT4"] = 91] = "CODE_DEFINE_FONT4";
      })(Parser.SwfTag || (Parser.SwfTag = {}));
      var SwfTag = Parser.SwfTag;

      (function (PlaceObjectFlags) {
        PlaceObjectFlags[PlaceObjectFlags["Reserved"] = 0x800] = "Reserved";
        PlaceObjectFlags[PlaceObjectFlags["OpaqueBackground"] = 0x400] = "OpaqueBackground";
        PlaceObjectFlags[PlaceObjectFlags["HasVisible"] = 0x200] = "HasVisible";
        PlaceObjectFlags[PlaceObjectFlags["HasImage"] = 0x100] = "HasImage";
        PlaceObjectFlags[PlaceObjectFlags["HasClassName"] = 0x800] = "HasClassName";
        PlaceObjectFlags[PlaceObjectFlags["HasCacheAsBitmap"] = 0x400] = "HasCacheAsBitmap";
        PlaceObjectFlags[PlaceObjectFlags["HasBlendMode"] = 0x200] = "HasBlendMode";
        PlaceObjectFlags[PlaceObjectFlags["HasFilterList"] = 0x100] = "HasFilterList";
        PlaceObjectFlags[PlaceObjectFlags["HasClipActions"] = 0x080] = "HasClipActions";
        PlaceObjectFlags[PlaceObjectFlags["HasClipDepth"] = 0x040] = "HasClipDepth";
        PlaceObjectFlags[PlaceObjectFlags["HasName"] = 0x020] = "HasName";
        PlaceObjectFlags[PlaceObjectFlags["HasRatio"] = 0x010] = "HasRatio";
        PlaceObjectFlags[PlaceObjectFlags["HasColorTransform"] = 0x008] = "HasColorTransform";
        PlaceObjectFlags[PlaceObjectFlags["HasMatrix"] = 0x004] = "HasMatrix";
        PlaceObjectFlags[PlaceObjectFlags["HasCharacter"] = 0x002] = "HasCharacter";
        PlaceObjectFlags[PlaceObjectFlags["Move"] = 0x001] = "Move";
      })(Parser.PlaceObjectFlags || (Parser.PlaceObjectFlags = {}));
      var PlaceObjectFlags = Parser.PlaceObjectFlags;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  var unexpected = Shumway.Debug.unexpected;

  var BinaryFileReader = (function () {
    function BinaryFileReader(url, method, mimeType, data) {
      this.url = url;
      this.method = method;
      this.mimeType = mimeType;
      this.data = data;
    }
    BinaryFileReader.prototype.readAll = function (progress, complete) {
      var url = this.url;
      var xhr = new XMLHttpRequest({ mozSystem: true });
      var async = true;
      xhr.open(this.method || "GET", this.url, async);
      xhr.responseType = "arraybuffer";
      if (progress) {
        xhr.onprogress = function (event) {
          progress(xhr.response, event.loaded, event.total);
        };
      }
      xhr.onreadystatechange = function (event) {
        if (xhr.readyState === 4) {
          if (xhr.status !== 200 && xhr.status !== 0 || xhr.response === null) {
            unexpected("Path: " + url + " not found.");
            complete(null, xhr.statusText);
            return;
          }
          complete(xhr.response);
        }
      };
      if (this.mimeType) {
        xhr.setRequestHeader("Content-Type", this.mimeType);
      }
      xhr.send(this.data || null);
    };

    BinaryFileReader.prototype.readAsync = function (ondata, onerror, onopen, oncomplete, onhttpstatus) {
      var xhr = new XMLHttpRequest({ mozSystem: true });
      var url = this.url;
      var loaded = 0;
      var total = 0;
      xhr.open(this.method || "GET", url, true);
      xhr.responseType = 'moz-chunked-arraybuffer';
      var isNotProgressive = xhr.responseType !== 'moz-chunked-arraybuffer';
      if (isNotProgressive) {
        xhr.responseType = 'arraybuffer';
      }
      xhr.onprogress = function (e) {
        if (isNotProgressive)
          return;
        loaded = e.loaded;
        total = e.total;
        ondata(new Uint8Array(xhr.response), { loaded: loaded, total: total });
      };
      xhr.onreadystatechange = function (event) {
        if (xhr.readyState === 2 && onhttpstatus) {
          onhttpstatus(url, xhr.status, xhr.getAllResponseHeaders());
        }
        if (xhr.readyState === 4) {
          if (xhr.status !== 200 && xhr.status !== 0 || xhr.response === null && (total === 0 || loaded !== total)) {
            onerror(xhr.statusText);
            return;
          }
          if (isNotProgressive) {
            var buffer = xhr.response;
            ondata(new Uint8Array(buffer), { loaded: 0, total: buffer.byteLength });
          }
          if (oncomplete) {
            oncomplete();
          }
        }
      };
      if (this.mimeType) {
        xhr.setRequestHeader("Content-Type", this.mimeType);
      }
      xhr.send(this.data || null);
      if (onopen) {
        onopen();
      }
    };
    return BinaryFileReader;
  })();
  Shumway.BinaryFileReader = BinaryFileReader;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Remoting) {
    (function (RemotingPhase) {
      RemotingPhase[RemotingPhase["Objects"] = 0] = "Objects";

      RemotingPhase[RemotingPhase["References"] = 1] = "References";
    })(Remoting.RemotingPhase || (Remoting.RemotingPhase = {}));
    var RemotingPhase = Remoting.RemotingPhase;

    (function (MessageBits) {
      MessageBits[MessageBits["HasMatrix"] = 0x0001] = "HasMatrix";
      MessageBits[MessageBits["HasBounds"] = 0x0002] = "HasBounds";
      MessageBits[MessageBits["HasChildren"] = 0x0004] = "HasChildren";
      MessageBits[MessageBits["HasColorTransform"] = 0x0008] = "HasColorTransform";
      MessageBits[MessageBits["HasClipRect"] = 0x0010] = "HasClipRect";
      MessageBits[MessageBits["HasMiscellaneousProperties"] = 0x0020] = "HasMiscellaneousProperties";
      MessageBits[MessageBits["HasMask"] = 0x0040] = "HasMask";
      MessageBits[MessageBits["HasClip"] = 0x0080] = "HasClip";
    })(Remoting.MessageBits || (Remoting.MessageBits = {}));
    var MessageBits = Remoting.MessageBits;

    (function (IDMask) {
      IDMask[IDMask["None"] = 0x00000000] = "None";
      IDMask[IDMask["Asset"] = 0x08000000] = "Asset";
    })(Remoting.IDMask || (Remoting.IDMask = {}));
    var IDMask = Remoting.IDMask;

    (function (MessageTag) {
      MessageTag[MessageTag["EOF"] = 0] = "EOF";

      MessageTag[MessageTag["UpdateFrame"] = 100] = "UpdateFrame";
      MessageTag[MessageTag["UpdateGraphics"] = 101] = "UpdateGraphics";
      MessageTag[MessageTag["UpdateBitmapData"] = 102] = "UpdateBitmapData";
      MessageTag[MessageTag["UpdateTextContent"] = 103] = "UpdateTextContent";
      MessageTag[MessageTag["UpdateStage"] = 104] = "UpdateStage";
      MessageTag[MessageTag["RequestBitmapData"] = 105] = "RequestBitmapData";

      MessageTag[MessageTag["RegisterFont"] = 200] = "RegisterFont";
      MessageTag[MessageTag["DrawToBitmap"] = 201] = "DrawToBitmap";

      MessageTag[MessageTag["MouseEvent"] = 300] = "MouseEvent";
      MessageTag[MessageTag["KeyboardEvent"] = 301] = "KeyboardEvent";
      MessageTag[MessageTag["FocusEvent"] = 302] = "FocusEvent";
    })(Remoting.MessageTag || (Remoting.MessageTag = {}));
    var MessageTag = Remoting.MessageTag;

    (function (ColorTransformEncoding) {
      ColorTransformEncoding[ColorTransformEncoding["Identity"] = 0] = "Identity";

      ColorTransformEncoding[ColorTransformEncoding["AlphaMultiplierOnly"] = 1] = "AlphaMultiplierOnly";

      ColorTransformEncoding[ColorTransformEncoding["All"] = 2] = "All";
    })(Remoting.ColorTransformEncoding || (Remoting.ColorTransformEncoding = {}));
    var ColorTransformEncoding = Remoting.ColorTransformEncoding;

    Remoting.MouseEventNames = [
      'click',
      'dblclick',
      'mousedown',
      'mousemove',
      'mouseup'
    ];

    Remoting.KeyboardEventNames = [
      'keydown',
      'keypress',
      'keyup'
    ];

    (function (KeyboardEventFlags) {
      KeyboardEventFlags[KeyboardEventFlags["CtrlKey"] = 0x0001] = "CtrlKey";
      KeyboardEventFlags[KeyboardEventFlags["AltKey"] = 0x0002] = "AltKey";
      KeyboardEventFlags[KeyboardEventFlags["ShiftKey"] = 0x0004] = "ShiftKey";
    })(Remoting.KeyboardEventFlags || (Remoting.KeyboardEventFlags = {}));
    var KeyboardEventFlags = Remoting.KeyboardEventFlags;

    (function (FocusEventType) {
      FocusEventType[FocusEventType["DocumentHidden"] = 0] = "DocumentHidden";
      FocusEventType[FocusEventType["DocumentVisible"] = 1] = "DocumentVisible";
      FocusEventType[FocusEventType["WindowBlur"] = 2] = "WindowBlur";
      FocusEventType[FocusEventType["WindowFocus"] = 3] = "WindowFocus";
    })(Remoting.FocusEventType || (Remoting.FocusEventType = {}));
    var FocusEventType = Remoting.FocusEventType;
  })(Shumway.Remoting || (Shumway.Remoting = {}));
  var Remoting = Shumway.Remoting;
})(Shumway || (Shumway = {}));
//# sourceMappingURL=base.js.map

var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Theme) {
      var UI = (function () {
        function UI() {
        }
        UI.toRGBA = function (r, g, b, a) {
          if (typeof a === "undefined") { a = 1; }
          return "rgba(" + r + "," + g + "," + b + "," + a + ")";
        };
        return UI;
      })();
      Theme.UI = UI;

      var UIThemeDark = (function () {
        function UIThemeDark() {
        }
        UIThemeDark.prototype.tabToolbar = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(37, 44, 51, a);
        };
        UIThemeDark.prototype.toolbars = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(52, 60, 69, a);
        };
        UIThemeDark.prototype.selectionBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(29, 79, 115, a);
        };
        UIThemeDark.prototype.selectionText = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(245, 247, 250, a);
        };
        UIThemeDark.prototype.splitters = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(0, 0, 0, a);
        };

        UIThemeDark.prototype.bodyBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(17, 19, 21, a);
        };
        UIThemeDark.prototype.sidebarBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(24, 29, 32, a);
        };
        UIThemeDark.prototype.attentionBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(161, 134, 80, a);
        };

        UIThemeDark.prototype.bodyText = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(143, 161, 178, a);
        };
        UIThemeDark.prototype.foregroundTextGrey = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(182, 186, 191, a);
        };
        UIThemeDark.prototype.contentTextHighContrast = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(169, 186, 203, a);
        };
        UIThemeDark.prototype.contentTextGrey = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(143, 161, 178, a);
        };
        UIThemeDark.prototype.contentTextDarkGrey = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(95, 115, 135, a);
        };

        UIThemeDark.prototype.blueHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(70, 175, 227, a);
        };
        UIThemeDark.prototype.purpleHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(107, 122, 187, a);
        };
        UIThemeDark.prototype.pinkHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(223, 128, 255, a);
        };
        UIThemeDark.prototype.redHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(235, 83, 104, a);
        };
        UIThemeDark.prototype.orangeHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(217, 102, 41, a);
        };
        UIThemeDark.prototype.lightOrangeHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(217, 155, 40, a);
        };
        UIThemeDark.prototype.greenHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(112, 191, 83, a);
        };
        UIThemeDark.prototype.blueGreyHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(94, 136, 176, a);
        };
        return UIThemeDark;
      })();
      Theme.UIThemeDark = UIThemeDark;

      var UIThemeLight = (function () {
        function UIThemeLight() {
        }
        UIThemeLight.prototype.tabToolbar = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(235, 236, 237, a);
        };
        UIThemeLight.prototype.toolbars = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(240, 241, 242, a);
        };
        UIThemeLight.prototype.selectionBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(76, 158, 217, a);
        };
        UIThemeLight.prototype.selectionText = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(245, 247, 250, a);
        };
        UIThemeLight.prototype.splitters = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(170, 170, 170, a);
        };

        UIThemeLight.prototype.bodyBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(252, 252, 252, a);
        };
        UIThemeLight.prototype.sidebarBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(247, 247, 247, a);
        };
        UIThemeLight.prototype.attentionBackground = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(161, 134, 80, a);
        };

        UIThemeLight.prototype.bodyText = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(24, 25, 26, a);
        };
        UIThemeLight.prototype.foregroundTextGrey = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(88, 89, 89, a);
        };
        UIThemeLight.prototype.contentTextHighContrast = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(41, 46, 51, a);
        };
        UIThemeLight.prototype.contentTextGrey = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(143, 161, 178, a);
        };
        UIThemeLight.prototype.contentTextDarkGrey = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(102, 115, 128, a);
        };

        UIThemeLight.prototype.blueHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(0, 136, 204, a);
        };
        UIThemeLight.prototype.purpleHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(91, 95, 255, a);
        };
        UIThemeLight.prototype.pinkHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(184, 46, 229, a);
        };
        UIThemeLight.prototype.redHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(237, 38, 85, a);
        };
        UIThemeLight.prototype.orangeHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(241, 60, 0, a);
        };
        UIThemeLight.prototype.lightOrangeHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(217, 126, 0, a);
        };
        UIThemeLight.prototype.greenHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(44, 187, 15, a);
        };
        UIThemeLight.prototype.blueGreyHighlight = function (a) {
          if (typeof a === "undefined") { a = 1; }
          return UI.toRGBA(95, 136, 176, a);
        };
        return UIThemeLight;
      })();
      Theme.UIThemeLight = UIThemeLight;
    })(Tools.Theme || (Tools.Theme = {}));
    var Theme = Tools.Theme;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      var Profile = (function () {
        function Profile(buffers) {
          this._buffers = buffers || [];
          this._snapshots = [];
          this._maxDepth = 0;
        }
        Profile.prototype.addBuffer = function (buffer) {
          this._buffers.push(buffer);
        };

        Profile.prototype.getSnapshotAt = function (index) {
          return this._snapshots[index];
        };

        Object.defineProperty(Profile.prototype, "hasSnapshots", {
          get: function () {
            return (this.snapshotCount > 0);
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "snapshotCount", {
          get: function () {
            return this._snapshots.length;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "startTime", {
          get: function () {
            return this._startTime;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "endTime", {
          get: function () {
            return this._endTime;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "totalTime", {
          get: function () {
            return this.endTime - this.startTime;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "windowStart", {
          get: function () {
            return this._windowStart;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "windowEnd", {
          get: function () {
            return this._windowEnd;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "windowLength", {
          get: function () {
            return this.windowEnd - this.windowStart;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Profile.prototype, "maxDepth", {
          get: function () {
            return this._maxDepth;
          },
          enumerable: true,
          configurable: true
        });

        Profile.prototype.forEachSnapshot = function (visitor) {
          for (var i = 0, n = this.snapshotCount; i < n; i++) {
            visitor(this._snapshots[i], i);
          }
        };

        Profile.prototype.createSnapshots = function () {
          var startTime = Number.MAX_VALUE;
          var endTime = Number.MIN_VALUE;
          var maxDepth = 0;
          this._snapshots = [];
          while (this._buffers.length > 0) {
            var buffer = this._buffers.shift();
            var snapshot = buffer.createSnapshot();
            if (snapshot) {
              if (startTime > snapshot.startTime) {
                startTime = snapshot.startTime;
              }
              if (endTime < snapshot.endTime) {
                endTime = snapshot.endTime;
              }
              if (maxDepth < snapshot.maxDepth) {
                maxDepth = snapshot.maxDepth;
              }
              this._snapshots.push(snapshot);
            }
          }
          this._startTime = startTime;
          this._endTime = endTime;
          this._windowStart = startTime;
          this._windowEnd = endTime;
          this._maxDepth = maxDepth;
        };

        Profile.prototype.setWindow = function (start, end) {
          if (start > end) {
            var tmp = start;
            start = end;
            end = tmp;
          }
          var length = Math.min(end - start, this.totalTime);
          if (start < this._startTime) {
            start = this._startTime;
            end = this._startTime + length;
          } else if (end > this._endTime) {
            start = this._endTime - length;
            end = this._endTime;
          }
          this._windowStart = start;
          this._windowEnd = end;
        };

        Profile.prototype.moveWindowTo = function (time) {
          this.setWindow(time - this.windowLength / 2, time + this.windowLength / 2);
        };
        return Profile;
      })();
      Profiler.Profile = Profile;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var __extends = this.__extends || function (d, b) {
    for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p];
    function __() { this.constructor = d; }
    __.prototype = b.prototype;
    d.prototype = new __();
};
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      var TimelineFrameStatistics = (function () {
        function TimelineFrameStatistics(kind) {
          this.kind = kind;
          this.count = 0;
          this.selfTime = 0;
          this.totalTime = 0;
        }
        return TimelineFrameStatistics;
      })();
      Profiler.TimelineFrameStatistics = TimelineFrameStatistics;

      var TimelineFrame = (function () {
        function TimelineFrame(parent, kind, startData, endData, startTime, endTime) {
          this.parent = parent;
          this.kind = kind;
          this.startData = startData;
          this.endData = endData;
          this.startTime = startTime;
          this.endTime = endTime;
          this.maxDepth = 0;
        }
        Object.defineProperty(TimelineFrame.prototype, "totalTime", {
          get: function () {
            return this.endTime - this.startTime;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(TimelineFrame.prototype, "selfTime", {
          get: function () {
            var selfTime = this.totalTime;
            if (this.children) {
              for (var i = 0, n = this.children.length; i < n; i++) {
                var child = this.children[i];
                selfTime -= (child.endTime - child.startTime);
              }
            }
            return selfTime;
          },
          enumerable: true,
          configurable: true
        });

        TimelineFrame.prototype.getChildIndex = function (time) {
          var children = this.children;
          for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.endTime > time) {
              return i;
            }
          }
          return 0;
        };

        TimelineFrame.prototype.getChildRange = function (startTime, endTime) {
          if (this.children && startTime <= this.endTime && endTime >= this.startTime && endTime >= startTime) {
            var startIdx = this._getNearestChild(startTime);
            var endIdx = this._getNearestChildReverse(endTime);
            if (startIdx <= endIdx) {
              var startTime = this.children[startIdx].startTime;
              var endTime = this.children[endIdx].endTime;
              return {
                startIndex: startIdx,
                endIndex: endIdx,
                startTime: startTime,
                endTime: endTime,
                totalTime: endTime - startTime
              };
            }
          }
          return null;
        };

        TimelineFrame.prototype._getNearestChild = function (time) {
          var children = this.children;
          if (children && children.length) {
            if (time <= children[0].endTime) {
              return 0;
            }
            var imid;
            var imin = 0;
            var imax = children.length - 1;
            while (imax > imin) {
              imid = ((imin + imax) / 2) | 0;
              var child = children[imid];
              if (time >= child.startTime && time <= child.endTime) {
                return imid;
              } else if (time > child.endTime) {
                imin = imid + 1;
              } else {
                imax = imid;
              }
            }
            return Math.ceil((imin + imax) / 2);
          } else {
            return 0;
          }
        };

        TimelineFrame.prototype._getNearestChildReverse = function (time) {
          var children = this.children;
          if (children && children.length) {
            var imax = children.length - 1;
            if (time >= children[imax].startTime) {
              return imax;
            }
            var imid;
            var imin = 0;
            while (imax > imin) {
              imid = Math.ceil((imin + imax) / 2);
              var child = children[imid];
              if (time >= child.startTime && time <= child.endTime) {
                return imid;
              } else if (time > child.endTime) {
                imin = imid;
              } else {
                imax = imid - 1;
              }
            }
            return ((imin + imax) / 2) | 0;
          } else {
            return 0;
          }
        };

        TimelineFrame.prototype.query = function (time) {
          if (time < this.startTime || time > this.endTime) {
            return null;
          }
          var children = this.children;
          if (children && children.length > 0) {
            var child;
            var imin = 0;
            var imax = children.length - 1;
            while (imax > imin) {
              var imid = ((imin + imax) / 2) | 0;
              child = children[imid];
              if (time >= child.startTime && time <= child.endTime) {
                return child.query(time);
              } else if (time > child.endTime) {
                imin = imid + 1;
              } else {
                imax = imid;
              }
            }
            child = children[imax];
            if (time >= child.startTime && time <= child.endTime) {
              return child.query(time);
            }
          }
          return this;
        };

        TimelineFrame.prototype.queryNext = function (time) {
          var frame = this;
          while (time > frame.endTime) {
            if (frame.parent) {
              frame = frame.parent;
            } else {
              return frame.query(time);
            }
          }
          return frame.query(time);
        };

        TimelineFrame.prototype.getDepth = function () {
          var depth = 0;
          var self = this;
          while (self) {
            depth++;
            self = self.parent;
          }
          return depth;
        };

        TimelineFrame.prototype.calculateStatistics = function () {
          var statistics = this.statistics = [];
          function visit(frame) {
            if (frame.kind) {
              var s = statistics[frame.kind.id] || (statistics[frame.kind.id] = new TimelineFrameStatistics(frame.kind));
              s.count++;
              s.selfTime += frame.selfTime;
              s.totalTime += frame.totalTime;
            }
            if (frame.children) {
              frame.children.forEach(visit);
            }
          }
          visit(this);
        };
        return TimelineFrame;
      })();
      Profiler.TimelineFrame = TimelineFrame;

      var TimelineBufferSnapshot = (function (_super) {
        __extends(TimelineBufferSnapshot, _super);
        function TimelineBufferSnapshot(name) {
          _super.call(this, null, null, null, null, NaN, NaN);
          this.name = name;
        }
        return TimelineBufferSnapshot;
      })(TimelineFrame);
      Profiler.TimelineBufferSnapshot = TimelineBufferSnapshot;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      var createEmptyObject = Shumway.ObjectUtilities.createEmptyObject;

      var TimelineBuffer = (function () {
        function TimelineBuffer(name, startTime) {
          if (typeof name === "undefined") { name = ""; }
          this.name = name || "";
          this._startTime = Shumway.isNullOrUndefined(startTime) ? performance.now() : startTime;
        }
        TimelineBuffer.prototype.getKind = function (kind) {
          return this._kinds[kind];
        };

        Object.defineProperty(TimelineBuffer.prototype, "kinds", {
          get: function () {
            return this._kinds.concat();
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(TimelineBuffer.prototype, "depth", {
          get: function () {
            return this._depth;
          },
          enumerable: true,
          configurable: true
        });

        TimelineBuffer.prototype._initialize = function () {
          this._depth = 0;
          this._stack = [];
          this._data = [];
          this._kinds = [];
          this._kindNameMap = createEmptyObject();
          this._marks = new Shumway.CircularBuffer(Int32Array, 20);
          this._times = new Shumway.CircularBuffer(Float64Array, 20);
        };

        TimelineBuffer.prototype._getKindId = function (name) {
          var kindId = TimelineBuffer.MAX_KINDID;
          if (this._kindNameMap[name] === undefined) {
            kindId = this._kinds.length;
            if (kindId < TimelineBuffer.MAX_KINDID) {
              var kind = {
                id: kindId,
                name: name,
                visible: true
              };
              this._kinds.push(kind);
              this._kindNameMap[name] = kind;
            } else {
              kindId = TimelineBuffer.MAX_KINDID;
            }
          } else {
            kindId = this._kindNameMap[name].id;
          }
          return kindId;
        };

        TimelineBuffer.prototype._getMark = function (type, kindId, data) {
          var dataId = TimelineBuffer.MAX_DATAID;
          if (!Shumway.isNullOrUndefined(data) && kindId !== TimelineBuffer.MAX_KINDID) {
            dataId = this._data.length;
            if (dataId < TimelineBuffer.MAX_DATAID) {
              this._data.push(data);
            } else {
              dataId = TimelineBuffer.MAX_DATAID;
            }
          }
          return type | (dataId << 16) | kindId;
        };

        TimelineBuffer.prototype.enter = function (name, data, time) {
          time = (Shumway.isNullOrUndefined(time) ? performance.now() : time) - this._startTime;
          if (!this._marks) {
            this._initialize();
          }
          this._depth++;
          var kindId = this._getKindId(name);
          this._marks.write(this._getMark(TimelineBuffer.ENTER, kindId, data));
          this._times.write(time);
          this._stack.push(kindId);
        };

        TimelineBuffer.prototype.leave = function (name, data, time) {
          time = (Shumway.isNullOrUndefined(time) ? performance.now() : time) - this._startTime;
          var kindId = this._stack.pop();
          if (name) {
            kindId = this._getKindId(name);
          }
          this._marks.write(this._getMark(TimelineBuffer.LEAVE, kindId, data));
          this._times.write(time);
          this._depth--;
        };

        TimelineBuffer.prototype.count = function (name, value, data) {
        };

        TimelineBuffer.prototype.createSnapshot = function (count) {
          if (typeof count === "undefined") { count = Number.MAX_VALUE; }
          if (!this._marks) {
            return null;
          }
          var times = this._times;
          var kinds = this._kinds;
          var datastore = this._data;
          var snapshot = new Profiler.TimelineBufferSnapshot(this.name);
          var stack = [snapshot];
          var topLevelFrameCount = 0;

          if (!this._marks) {
            this._initialize();
          }

          this._marks.forEachInReverse(function (mark, i) {
            var dataId = (mark >>> 16) & TimelineBuffer.MAX_DATAID;
            var data = datastore[dataId];
            var kindId = mark & TimelineBuffer.MAX_KINDID;
            var kind = kinds[kindId];
            if (Shumway.isNullOrUndefined(kind) || kind.visible) {
              var action = mark & 0x80000000;
              var time = times.get(i);
              var stackLength = stack.length;
              if (action === TimelineBuffer.LEAVE) {
                if (stackLength === 1) {
                  topLevelFrameCount++;
                  if (topLevelFrameCount > count) {
                    return true;
                  }
                }
                stack.push(new Profiler.TimelineFrame(stack[stackLength - 1], kind, null, data, NaN, time));
              } else if (action === TimelineBuffer.ENTER) {
                var node = stack.pop();
                var top = stack[stack.length - 1];
                if (top) {
                  if (!top.children) {
                    top.children = [node];
                  } else {
                    top.children.unshift(node);
                  }
                  var currentDepth = stack.length;
                  node.depth = currentDepth;
                  node.startData = data;
                  node.startTime = time;
                  while (node) {
                    if (node.maxDepth < currentDepth) {
                      node.maxDepth = currentDepth;
                      node = node.parent;
                    } else {
                      break;
                    }
                  }
                } else {
                  return true;
                }
              }
            }
          });
          if (snapshot.children && snapshot.children.length) {
            snapshot.startTime = snapshot.children[0].startTime;
            snapshot.endTime = snapshot.children[snapshot.children.length - 1].endTime;
          }
          return snapshot;
        };

        TimelineBuffer.prototype.reset = function (startTime) {
          this._startTime = Shumway.isNullOrUndefined(startTime) ? performance.now() : startTime;
          if (!this._marks) {
            this._initialize();
            return;
          }
          this._depth = 0;
          this._data = [];
          this._marks.reset();
          this._times.reset();
        };

        TimelineBuffer.FromFirefoxProfile = function (profile, name) {
          var samples = profile.profile.threads[0].samples;
          var buffer = new TimelineBuffer(name, samples[0].time);
          var currentStack = [];
          var sample;
          for (var i = 0; i < samples.length; i++) {
            sample = samples[i];
            var time = sample.time;
            var stack = sample.frames;
            var j = 0;
            var minStackLen = Math.min(stack.length, currentStack.length);
            while (j < minStackLen && stack[j].location === currentStack[j].location) {
              j++;
            }
            var leaveCount = currentStack.length - j;
            for (var k = 0; k < leaveCount; k++) {
              sample = currentStack.pop();
              buffer.leave(sample.location, null, time);
            }
            while (j < stack.length) {
              sample = stack[j++];
              buffer.enter(sample.location, null, time);
            }
            currentStack = stack;
          }
          while (sample = currentStack.pop()) {
            buffer.leave(sample.location, null, time);
          }
          return buffer;
        };

        TimelineBuffer.FromChromeProfile = function (profile, name) {
          var timestamps = profile.timestamps;
          var samples = profile.samples;
          var buffer = new TimelineBuffer(name, timestamps[0] / 1000);
          var currentStack = [];
          var idMap = {};
          var sample;
          TimelineBuffer._resolveIds(profile.head, idMap);
          for (var i = 0; i < timestamps.length; i++) {
            var time = timestamps[i] / 1000;
            var stack = [];
            sample = idMap[samples[i]];
            while (sample) {
              stack.unshift(sample);
              sample = sample.parent;
            }
            var j = 0;
            var minStackLen = Math.min(stack.length, currentStack.length);
            while (j < minStackLen && stack[j] === currentStack[j]) {
              j++;
            }
            var leaveCount = currentStack.length - j;
            for (var k = 0; k < leaveCount; k++) {
              sample = currentStack.pop();
              buffer.leave(sample.functionName, null, time);
            }
            while (j < stack.length) {
              sample = stack[j++];
              buffer.enter(sample.functionName, null, time);
            }
            currentStack = stack;
          }
          while (sample = currentStack.pop()) {
            buffer.leave(sample.functionName, null, time);
          }
          return buffer;
        };

        TimelineBuffer._resolveIds = function (parent, idMap) {
          idMap[parent.id] = parent;
          if (parent.children) {
            for (var i = 0; i < parent.children.length; i++) {
              parent.children[i].parent = parent;
              TimelineBuffer._resolveIds(parent.children[i], idMap);
            }
          }
        };
        TimelineBuffer.ENTER = 0 << 31;
        TimelineBuffer.LEAVE = 1 << 31;

        TimelineBuffer.MAX_KINDID = 0xffff;
        TimelineBuffer.MAX_DATAID = 0x7fff;
        return TimelineBuffer;
      })();
      Profiler.TimelineBuffer = TimelineBuffer;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      (function (UIThemeType) {
        UIThemeType[UIThemeType["DARK"] = 0] = "DARK";
        UIThemeType[UIThemeType["LIGHT"] = 1] = "LIGHT";
      })(Profiler.UIThemeType || (Profiler.UIThemeType = {}));
      var UIThemeType = Profiler.UIThemeType;

      var Controller = (function () {
        function Controller(container, themeType) {
          if (typeof themeType === "undefined") { themeType = 0 /* DARK */; }
          this._container = container;
          this._headers = [];
          this._charts = [];
          this._profiles = [];
          this._activeProfile = null;
          this.themeType = themeType;
          this._tooltip = this._createTooltip();
        }
        Controller.prototype.createProfile = function (buffers, activate) {
          if (typeof activate === "undefined") { activate = true; }
          var profile = new Profiler.Profile(buffers);
          profile.createSnapshots();
          this._profiles.push(profile);
          if (activate) {
            this.activateProfile(profile);
          }
          return profile;
        };

        Controller.prototype.activateProfile = function (profile) {
          this.deactivateProfile();
          this._activeProfile = profile;
          this._createViews();
          this._initializeViews();
        };

        Controller.prototype.activateProfileAt = function (index) {
          this.activateProfile(this.getProfileAt(index));
        };

        Controller.prototype.deactivateProfile = function () {
          if (this._activeProfile) {
            this._destroyViews();
            this._activeProfile = null;
          }
        };

        Controller.prototype.resize = function () {
          this._onResize();
        };

        Controller.prototype.getProfileAt = function (index) {
          return this._profiles[index];
        };

        Object.defineProperty(Controller.prototype, "activeProfile", {
          get: function () {
            return this._activeProfile;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Controller.prototype, "profileCount", {
          get: function () {
            return this._profiles.length;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Controller.prototype, "container", {
          get: function () {
            return this._container;
          },
          enumerable: true,
          configurable: true
        });


        Object.defineProperty(Controller.prototype, "themeType", {
          get: function () {
            return this._themeType;
          },
          set: function (value) {
            switch (value) {
              case 0 /* DARK */:
                this._theme = new Tools.Theme.UIThemeDark();
                break;
              case 1 /* LIGHT */:
                this._theme = new Tools.Theme.UIThemeLight();
                break;
            }
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(Controller.prototype, "theme", {
          get: function () {
            return this._theme;
          },
          enumerable: true,
          configurable: true
        });

        Controller.prototype.getSnapshotAt = function (index) {
          return this._activeProfile.getSnapshotAt(index);
        };

        Controller.prototype._createViews = function () {
          if (this._activeProfile) {
            var self = this;
            this._overviewHeader = new Profiler.FlameChartHeader(this, 0 /* OVERVIEW */);
            this._overview = new Profiler.FlameChartOverview(this, 0 /* OVERLAY */);
            this._activeProfile.forEachSnapshot(function (snapshot, index) {
              self._headers.push(new Profiler.FlameChartHeader(self, 1 /* CHART */));
              self._charts.push(new Profiler.FlameChart(self, snapshot));
            });
            window.addEventListener("resize", this._onResize.bind(this));
          }
        };

        Controller.prototype._destroyViews = function () {
          if (this._activeProfile) {
            this._overviewHeader.destroy();
            this._overview.destroy();
            while (this._headers.length) {
              this._headers.pop().destroy();
            }
            while (this._charts.length) {
              this._charts.pop().destroy();
            }
            window.removeEventListener("resize", this._onResize.bind(this));
          }
        };

        Controller.prototype._initializeViews = function () {
          if (this._activeProfile) {
            var self = this;
            var startTime = this._activeProfile.startTime;
            var endTime = this._activeProfile.endTime;
            this._overviewHeader.initialize(startTime, endTime);
            this._overview.initialize(startTime, endTime);
            this._activeProfile.forEachSnapshot(function (snapshot, index) {
              self._headers[index].initialize(startTime, endTime);
              self._charts[index].initialize(startTime, endTime);
            });
          }
        };

        Controller.prototype._onResize = function () {
          if (this._activeProfile) {
            var self = this;
            var width = this._container.offsetWidth;
            this._overviewHeader.setSize(width);
            this._overview.setSize(width);
            this._activeProfile.forEachSnapshot(function (snapshot, index) {
              self._headers[index].setSize(width);
              self._charts[index].setSize(width);
            });
          }
        };

        Controller.prototype._updateViews = function () {
          if (this._activeProfile) {
            var self = this;
            var start = this._activeProfile.windowStart;
            var end = this._activeProfile.windowEnd;
            this._overviewHeader.setWindow(start, end);
            this._overview.setWindow(start, end);
            this._activeProfile.forEachSnapshot(function (snapshot, index) {
              self._headers[index].setWindow(start, end);
              self._charts[index].setWindow(start, end);
            });
          }
        };

        Controller.prototype._drawViews = function () {
        };

        Controller.prototype._createTooltip = function () {
          var el = document.createElement("div");
          el.classList.add("profiler-tooltip");
          el.style.display = "none";
          this._container.insertBefore(el, this._container.firstChild);
          return el;
        };

        Controller.prototype.setWindow = function (start, end) {
          this._activeProfile.setWindow(start, end);
          this._updateViews();
        };

        Controller.prototype.moveWindowTo = function (time) {
          this._activeProfile.moveWindowTo(time);
          this._updateViews();
        };

        Controller.prototype.showTooltip = function (chart, frame, x, y) {
          this.removeTooltipContent();
          this._tooltip.appendChild(this.createTooltipContent(chart, frame));
          this._tooltip.style.display = "block";
          var elContent = this._tooltip.firstChild;
          var tooltipWidth = elContent.clientWidth;
          var tooltipHeight = elContent.clientHeight;
          var totalWidth = chart.canvas.clientWidth;
          x += (x + tooltipWidth >= totalWidth - 50) ? -(tooltipWidth + 20) : 25;
          y += chart.canvas.offsetTop - tooltipHeight / 2;
          this._tooltip.style.left = x + "px";
          this._tooltip.style.top = y + "px";
        };

        Controller.prototype.hideTooltip = function () {
          this._tooltip.style.display = "none";
        };

        Controller.prototype.createTooltipContent = function (chart, frame) {
          var totalTime = Math.round(frame.totalTime * 100000) / 100000;
          var selfTime = Math.round(frame.selfTime * 100000) / 100000;
          var selfPercent = Math.round(frame.selfTime * 100 * 100 / frame.totalTime) / 100;

          var elContent = document.createElement("div");

          var elName = document.createElement("h1");
          elName.textContent = frame.kind.name;
          elContent.appendChild(elName);

          var elTotalTime = document.createElement("p");
          elTotalTime.textContent = "Total: " + totalTime + " ms";
          elContent.appendChild(elTotalTime);

          var elSelfTime = document.createElement("p");
          elSelfTime.textContent = "Self: " + selfTime + " ms (" + selfPercent + "%)";
          elContent.appendChild(elSelfTime);

          var statistics = chart.getStatistics(frame.kind);

          if (statistics) {
            var elAllCount = document.createElement("p");
            elAllCount.textContent = "Count: " + statistics.count;
            elContent.appendChild(elAllCount);

            var allTotalTime = Math.round(statistics.totalTime * 100000) / 100000;
            var elAllTotalTime = document.createElement("p");
            elAllTotalTime.textContent = "All Total: " + allTotalTime + " ms";
            elContent.appendChild(elAllTotalTime);

            var allSelfTime = Math.round(statistics.selfTime * 100000) / 100000;
            var elAllSelfTime = document.createElement("p");
            elAllSelfTime.textContent = "All Self: " + allSelfTime + " ms";
            elContent.appendChild(elAllSelfTime);
          }

          this.appendDataElements(elContent, frame.startData);
          this.appendDataElements(elContent, frame.endData);

          return elContent;
        };

        Controller.prototype.appendDataElements = function (el, data) {
          if (!Shumway.isNullOrUndefined(data)) {
            el.appendChild(document.createElement("hr"));
            var elData;
            if (Shumway.isObject(data)) {
              for (var key in data) {
                elData = document.createElement("p");
                elData.textContent = key + ": " + data[key];
                el.appendChild(elData);
              }
            } else {
              elData = document.createElement("p");
              elData.textContent = data.toString();
              el.appendChild(elData);
            }
          }
        };

        Controller.prototype.removeTooltipContent = function () {
          var el = this._tooltip;
          while (el.firstChild) {
            el.removeChild(el.firstChild);
          }
        };
        return Controller;
      })();
      Profiler.Controller = Controller;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      var clamp = Shumway.NumberUtilities.clamp;

      var MouseCursor = (function () {
        function MouseCursor(value) {
          this.value = value;
        }
        MouseCursor.prototype.toString = function () {
          return this.value;
        };
        MouseCursor.AUTO = new MouseCursor("auto");
        MouseCursor.DEFAULT = new MouseCursor("default");
        MouseCursor.NONE = new MouseCursor("none");
        MouseCursor.HELP = new MouseCursor("help");
        MouseCursor.POINTER = new MouseCursor("pointer");
        MouseCursor.PROGRESS = new MouseCursor("progress");
        MouseCursor.WAIT = new MouseCursor("wait");
        MouseCursor.CELL = new MouseCursor("cell");
        MouseCursor.CROSSHAIR = new MouseCursor("crosshair");
        MouseCursor.TEXT = new MouseCursor("text");
        MouseCursor.ALIAS = new MouseCursor("alias");
        MouseCursor.COPY = new MouseCursor("copy");
        MouseCursor.MOVE = new MouseCursor("move");
        MouseCursor.NO_DROP = new MouseCursor("no-drop");
        MouseCursor.NOT_ALLOWED = new MouseCursor("not-allowed");
        MouseCursor.ALL_SCROLL = new MouseCursor("all-scroll");
        MouseCursor.COL_RESIZE = new MouseCursor("col-resize");
        MouseCursor.ROW_RESIZE = new MouseCursor("row-resize");
        MouseCursor.N_RESIZE = new MouseCursor("n-resize");
        MouseCursor.E_RESIZE = new MouseCursor("e-resize");
        MouseCursor.S_RESIZE = new MouseCursor("s-resize");
        MouseCursor.W_RESIZE = new MouseCursor("w-resize");
        MouseCursor.NE_RESIZE = new MouseCursor("ne-resize");
        MouseCursor.NW_RESIZE = new MouseCursor("nw-resize");
        MouseCursor.SE_RESIZE = new MouseCursor("se-resize");
        MouseCursor.SW_RESIZE = new MouseCursor("sw-resize");
        MouseCursor.EW_RESIZE = new MouseCursor("ew-resize");
        MouseCursor.NS_RESIZE = new MouseCursor("ns-resize");
        MouseCursor.NESW_RESIZE = new MouseCursor("nesw-resize");
        MouseCursor.NWSE_RESIZE = new MouseCursor("nwse-resize");
        MouseCursor.ZOOM_IN = new MouseCursor("zoom-in");
        MouseCursor.ZOOM_OUT = new MouseCursor("zoom-out");
        MouseCursor.GRAB = new MouseCursor("grab");
        MouseCursor.GRABBING = new MouseCursor("grabbing");
        return MouseCursor;
      })();
      Profiler.MouseCursor = MouseCursor;

      var MouseController = (function () {
        function MouseController(target, eventTarget) {
          this._target = target;
          this._eventTarget = eventTarget;
          this._wheelDisabled = false;
          this._boundOnMouseDown = this._onMouseDown.bind(this);
          this._boundOnMouseUp = this._onMouseUp.bind(this);
          this._boundOnMouseOver = this._onMouseOver.bind(this);
          this._boundOnMouseOut = this._onMouseOut.bind(this);
          this._boundOnMouseMove = this._onMouseMove.bind(this);
          this._boundOnMouseWheel = this._onMouseWheel.bind(this);
          this._boundOnDrag = this._onDrag.bind(this);
          eventTarget.addEventListener("mousedown", this._boundOnMouseDown, false);
          eventTarget.addEventListener("mouseover", this._boundOnMouseOver, false);
          eventTarget.addEventListener("mouseout", this._boundOnMouseOut, false);
          eventTarget.addEventListener(("onwheel" in document ? "wheel" : "mousewheel"), this._boundOnMouseWheel, false);
        }
        MouseController.prototype.destroy = function () {
          var eventTarget = this._eventTarget;
          eventTarget.removeEventListener("mousedown", this._boundOnMouseDown);
          eventTarget.removeEventListener("mouseover", this._boundOnMouseOver);
          eventTarget.removeEventListener("mouseout", this._boundOnMouseOut);
          eventTarget.removeEventListener(("onwheel" in document ? "wheel" : "mousewheel"), this._boundOnMouseWheel);
          window.removeEventListener("mousemove", this._boundOnDrag);
          window.removeEventListener("mouseup", this._boundOnMouseUp);
          this._killHoverCheck();
          this._eventTarget = null;
          this._target = null;
        };

        MouseController.prototype.updateCursor = function (cursor) {
          if (!MouseController._cursorOwner || MouseController._cursorOwner === this._target) {
            var el = this._eventTarget.parentElement;
            if (MouseController._cursor !== cursor) {
              MouseController._cursor = cursor;
              var self = this;
              ["", "-moz-", "-webkit-"].forEach(function (prefix) {
                el.style.cursor = prefix + cursor;
              });
            }
            if (MouseController._cursor === MouseCursor.DEFAULT) {
              MouseController._cursorOwner = null;
            } else {
              MouseController._cursorOwner = this._target;
            }
          }
        };

        MouseController.prototype._onMouseDown = function (event) {
          this._killHoverCheck();
          if (event.button === 0) {
            var pos = this._getTargetMousePos(event, (event.target));
            this._dragInfo = {
              start: pos,
              current: pos,
              delta: { x: 0, y: 0 },
              hasMoved: false,
              originalTarget: event.target
            };
            window.addEventListener("mousemove", this._boundOnDrag, false);
            window.addEventListener("mouseup", this._boundOnMouseUp, false);
            this._target.onMouseDown(pos.x, pos.y);
          }
        };

        MouseController.prototype._onDrag = function (event) {
          var dragInfo = this._dragInfo;
          var current = this._getTargetMousePos(event, dragInfo.originalTarget);
          var delta = {
            x: current.x - dragInfo.start.x,
            y: current.y - dragInfo.start.y
          };
          dragInfo.current = current;
          dragInfo.delta = delta;
          dragInfo.hasMoved = true;
          this._target.onDrag(dragInfo.start.x, dragInfo.start.y, current.x, current.y, delta.x, delta.y);
        };

        MouseController.prototype._onMouseUp = function (event) {
          window.removeEventListener("mousemove", this._boundOnDrag);
          window.removeEventListener("mouseup", this._boundOnMouseUp);
          var self = this;
          var dragInfo = this._dragInfo;
          if (dragInfo.hasMoved) {
            this._target.onDragEnd(dragInfo.start.x, dragInfo.start.y, dragInfo.current.x, dragInfo.current.y, dragInfo.delta.x, dragInfo.delta.y);
          } else {
            this._target.onClick(dragInfo.current.x, dragInfo.current.y);
          }
          this._dragInfo = null;
          this._wheelDisabled = true;
          setTimeout(function () {
            self._wheelDisabled = false;
          }, 500);
        };

        MouseController.prototype._onMouseOver = function (event) {
          event.target.addEventListener("mousemove", this._boundOnMouseMove, false);
          if (!this._dragInfo) {
            var pos = this._getTargetMousePos(event, (event.target));
            this._target.onMouseOver(pos.x, pos.y);
            this._startHoverCheck(event);
          }
        };

        MouseController.prototype._onMouseOut = function (event) {
          event.target.removeEventListener("mousemove", this._boundOnMouseMove, false);
          if (!this._dragInfo) {
            this._target.onMouseOut();
          }
          this._killHoverCheck();
        };

        MouseController.prototype._onMouseMove = function (event) {
          if (!this._dragInfo) {
            var pos = this._getTargetMousePos(event, (event.target));
            this._target.onMouseMove(pos.x, pos.y);
            this._killHoverCheck();
            this._startHoverCheck(event);
          }
        };

        MouseController.prototype._onMouseWheel = function (event) {
          if (!event.altKey && !event.metaKey && !event.ctrlKey && !event.shiftKey) {
            event.preventDefault();
            if (!this._dragInfo && !this._wheelDisabled) {
              var pos = this._getTargetMousePos(event, (event.target));
              var delta = clamp((typeof event.deltaY !== "undefined") ? event.deltaY / 16 : -event.wheelDelta / 40, -1, 1);
              var zoom = Math.pow(1.2, delta) - 1;
              this._target.onMouseWheel(pos.x, pos.y, zoom);
            }
          }
        };

        MouseController.prototype._startHoverCheck = function (event) {
          this._hoverInfo = {
            isHovering: false,
            timeoutHandle: setTimeout(this._onMouseMoveIdleHandler.bind(this), MouseController.HOVER_TIMEOUT),
            pos: this._getTargetMousePos(event, (event.target))
          };
        };

        MouseController.prototype._killHoverCheck = function () {
          if (this._hoverInfo) {
            clearTimeout(this._hoverInfo.timeoutHandle);
            if (this._hoverInfo.isHovering) {
              this._target.onHoverEnd();
            }
            this._hoverInfo = null;
          }
        };

        MouseController.prototype._onMouseMoveIdleHandler = function () {
          var hoverInfo = this._hoverInfo;
          hoverInfo.isHovering = true;
          this._target.onHoverStart(hoverInfo.pos.x, hoverInfo.pos.y);
        };

        MouseController.prototype._getTargetMousePos = function (event, target) {
          var rect = target.getBoundingClientRect();
          return {
            x: event.clientX - rect.left,
            y: event.clientY - rect.top
          };
        };
        MouseController.HOVER_TIMEOUT = 500;

        MouseController._cursor = MouseCursor.DEFAULT;
        return MouseController;
      })();
      Profiler.MouseController = MouseController;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      (function (FlameChartDragTarget) {
        FlameChartDragTarget[FlameChartDragTarget["NONE"] = 0] = "NONE";
        FlameChartDragTarget[FlameChartDragTarget["WINDOW"] = 1] = "WINDOW";
        FlameChartDragTarget[FlameChartDragTarget["HANDLE_LEFT"] = 2] = "HANDLE_LEFT";
        FlameChartDragTarget[FlameChartDragTarget["HANDLE_RIGHT"] = 3] = "HANDLE_RIGHT";
        FlameChartDragTarget[FlameChartDragTarget["HANDLE_BOTH"] = 4] = "HANDLE_BOTH";
      })(Profiler.FlameChartDragTarget || (Profiler.FlameChartDragTarget = {}));
      var FlameChartDragTarget = Profiler.FlameChartDragTarget;

      var FlameChartBase = (function () {
        function FlameChartBase(controller) {
          this._controller = controller;
          this._initialized = false;
          this._canvas = document.createElement("canvas");
          this._context = this._canvas.getContext("2d");
          this._mouseController = new Profiler.MouseController(this, this._canvas);
          var container = controller.container;
          container.appendChild(this._canvas);
          var rect = container.getBoundingClientRect();
          this.setSize(rect.width);
        }
        Object.defineProperty(FlameChartBase.prototype, "canvas", {
          get: function () {
            return this._canvas;
          },
          enumerable: true,
          configurable: true
        });

        FlameChartBase.prototype.setSize = function (width, height) {
          if (typeof height === "undefined") { height = 20; }
          this._width = width;
          this._height = height;
          this._resetCanvas();
          this.draw();
        };

        FlameChartBase.prototype.initialize = function (rangeStart, rangeEnd) {
          this._initialized = true;
          this.setRange(rangeStart, rangeEnd, false);
          this.setWindow(rangeStart, rangeEnd, false);
          this.draw();
        };

        FlameChartBase.prototype.setWindow = function (start, end, draw) {
          if (typeof draw === "undefined") { draw = true; }
          this._windowStart = start;
          this._windowEnd = end;
          !draw || this.draw();
        };

        FlameChartBase.prototype.setRange = function (start, end, draw) {
          if (typeof draw === "undefined") { draw = true; }
          this._rangeStart = start;
          this._rangeEnd = end;
          !draw || this.draw();
        };

        FlameChartBase.prototype.destroy = function () {
          this._mouseController.destroy();
          this._mouseController = null;
          this._controller.container.removeChild(this._canvas);
          this._controller = null;
        };

        FlameChartBase.prototype._resetCanvas = function () {
          var ratio = window.devicePixelRatio;
          var canvas = this._canvas;
          canvas.width = this._width * ratio;
          canvas.height = this._height * ratio;
          canvas.style.width = this._width + "px";
          canvas.style.height = this._height + "px";
        };

        FlameChartBase.prototype.draw = function () {
        };

        FlameChartBase.prototype._almostEq = function (a, b, precision) {
          if (typeof precision === "undefined") { precision = 10; }
          var pow10 = Math.pow(10, precision);
          return Math.abs(a - b) < (1 / pow10);
        };

        FlameChartBase.prototype._windowEqRange = function () {
          return (this._almostEq(this._windowStart, this._rangeStart) && this._almostEq(this._windowEnd, this._rangeEnd));
        };

        FlameChartBase.prototype._decimalPlaces = function (value) {
          return ((+value).toFixed(10)).replace(/^-?\d*\.?|0+$/g, '').length;
        };

        FlameChartBase.prototype._toPixelsRelative = function (time) {
          return 0;
        };
        FlameChartBase.prototype._toPixels = function (time) {
          return 0;
        };
        FlameChartBase.prototype._toTimeRelative = function (px) {
          return 0;
        };
        FlameChartBase.prototype._toTime = function (px) {
          return 0;
        };

        FlameChartBase.prototype.onMouseWheel = function (x, y, delta) {
          var time = this._toTime(x);
          var windowStart = this._windowStart;
          var windowEnd = this._windowEnd;
          var windowLen = windowEnd - windowStart;

          var maxDelta = Math.max((FlameChartBase.MIN_WINDOW_LEN - windowLen) / windowLen, delta);
          var start = windowStart + (windowStart - time) * maxDelta;
          var end = windowEnd + (windowEnd - time) * maxDelta;
          this._controller.setWindow(start, end);
          this.onHoverEnd();
        };

        FlameChartBase.prototype.onMouseDown = function (x, y) {
        };
        FlameChartBase.prototype.onMouseMove = function (x, y) {
        };
        FlameChartBase.prototype.onMouseOver = function (x, y) {
        };
        FlameChartBase.prototype.onMouseOut = function () {
        };
        FlameChartBase.prototype.onDrag = function (startX, startY, currentX, currentY, deltaX, deltaY) {
        };
        FlameChartBase.prototype.onDragEnd = function (startX, startY, currentX, currentY, deltaX, deltaY) {
        };
        FlameChartBase.prototype.onClick = function (x, y) {
        };
        FlameChartBase.prototype.onHoverStart = function (x, y) {
        };
        FlameChartBase.prototype.onHoverEnd = function () {
        };
        FlameChartBase.DRAGHANDLE_WIDTH = 4;
        FlameChartBase.MIN_WINDOW_LEN = 0.1;
        return FlameChartBase;
      })();
      Profiler.FlameChartBase = FlameChartBase;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      var trimMiddle = Shumway.StringUtilities.trimMiddle;
      var createEmptyObject = Shumway.ObjectUtilities.createEmptyObject;

      var FlameChart = (function (_super) {
        __extends(FlameChart, _super);
        function FlameChart(controller, snapshot) {
          _super.call(this, controller);
          this._textWidth = {};
          this._minFrameWidthInPixels = 1;
          this._snapshot = snapshot;
          this._kindStyle = createEmptyObject();
        }
        FlameChart.prototype.setSize = function (width, height) {
          _super.prototype.setSize.call(this, width, height || this._initialized ? this._maxDepth * 12.5 : 100);
        };

        FlameChart.prototype.initialize = function (rangeStart, rangeEnd) {
          this._initialized = true;
          this._maxDepth = this._snapshot.maxDepth;
          this.setRange(rangeStart, rangeEnd, false);
          this.setWindow(rangeStart, rangeEnd, false);
          this.setSize(this._width, this._maxDepth * 12.5);
        };

        FlameChart.prototype.destroy = function () {
          _super.prototype.destroy.call(this);
          this._snapshot = null;
        };

        FlameChart.prototype.draw = function () {
          var context = this._context;
          var ratio = window.devicePixelRatio;

          Shumway.ColorStyle.reset();

          context.save();
          context.scale(ratio, ratio);
          context.fillStyle = this._controller.theme.bodyBackground(1);
          context.fillRect(0, 0, this._width, this._height);

          if (this._initialized) {
            this._drawChildren(this._snapshot);
          }

          context.restore();
        };

        FlameChart.prototype._drawChildren = function (parent, depth) {
          if (typeof depth === "undefined") { depth = 0; }
          var range = parent.getChildRange(this._windowStart, this._windowEnd);
          if (range) {
            for (var i = range.startIndex; i <= range.endIndex; i++) {
              var child = parent.children[i];
              if (this._drawFrame(child, depth)) {
                this._drawChildren(child, depth + 1);
              }
            }
          }
        };

        FlameChart.prototype._drawFrame = function (frame, depth) {
          var context = this._context;
          var frameHPadding = 0.5;
          var left = this._toPixels(frame.startTime);
          var right = this._toPixels(frame.endTime);
          var width = right - left;
          if (width <= this._minFrameWidthInPixels) {
            context.fillStyle = this._controller.theme.tabToolbar(1);
            context.fillRect(left, depth * (12 + frameHPadding), this._minFrameWidthInPixels, 12 + (frame.maxDepth - frame.depth) * 12.5);
            return false;
          }
          if (left < 0) {
            right = width + left;
            left = 0;
          }
          var adjustedWidth = right - left;
          var style = this._kindStyle[frame.kind.id];
          if (!style) {
            var background = Shumway.ColorStyle.randomStyle();
            style = this._kindStyle[frame.kind.id] = {
              bgColor: background,
              textColor: Shumway.ColorStyle.contrastStyle(background)
            };
          }
          context.save();

          context.fillStyle = style.bgColor;
          context.fillRect(left, depth * (12 + frameHPadding), adjustedWidth, 12);

          if (width > 12) {
            var label = frame.kind.name;
            if (label && label.length) {
              var labelHPadding = 2;
              label = this._prepareText(context, label, adjustedWidth - labelHPadding * 2);
              if (label.length) {
                context.fillStyle = style.textColor;
                context.textBaseline = "bottom";
                context.fillText(label, left + labelHPadding, (depth + 1) * (12 + frameHPadding) - 1);
              }
            }
          }
          context.restore();
          return true;
        };

        FlameChart.prototype._prepareText = function (context, title, maxSize) {
          var titleWidth = this._measureWidth(context, title);
          if (maxSize > titleWidth) {
            return title;
          }
          var l = 3;
          var r = title.length;
          while (l < r) {
            var m = (l + r) >> 1;
            if (this._measureWidth(context, trimMiddle(title, m)) < maxSize) {
              l = m + 1;
            } else {
              r = m;
            }
          }
          title = trimMiddle(title, r - 1);
          titleWidth = this._measureWidth(context, title);
          if (titleWidth <= maxSize) {
            return title;
          }
          return "";
        };

        FlameChart.prototype._measureWidth = function (context, text) {
          var width = this._textWidth[text];
          if (!width) {
            width = context.measureText(text).width;
            this._textWidth[text] = width;
          }
          return width;
        };

        FlameChart.prototype._toPixelsRelative = function (time) {
          return time * this._width / (this._windowEnd - this._windowStart);
        };

        FlameChart.prototype._toPixels = function (time) {
          return this._toPixelsRelative(time - this._windowStart);
        };

        FlameChart.prototype._toTimeRelative = function (px) {
          return px * (this._windowEnd - this._windowStart) / this._width;
        };

        FlameChart.prototype._toTime = function (px) {
          return this._toTimeRelative(px) + this._windowStart;
        };

        FlameChart.prototype._getFrameAtPosition = function (x, y) {
          var time = this._toTime(x);
          var depth = 1 + (y / 12.5) | 0;
          var frame = this._snapshot.query(time);
          if (frame && frame.depth >= depth) {
            while (frame && frame.depth > depth) {
              frame = frame.parent;
            }
            return frame;
          }
          return null;
        };

        FlameChart.prototype.onMouseDown = function (x, y) {
          if (!this._windowEqRange()) {
            this._mouseController.updateCursor(Profiler.MouseCursor.ALL_SCROLL);
            this._dragInfo = {
              windowStartInitial: this._windowStart,
              windowEndInitial: this._windowEnd,
              target: 1 /* WINDOW */
            };
          }
        };

        FlameChart.prototype.onMouseMove = function (x, y) {
        };
        FlameChart.prototype.onMouseOver = function (x, y) {
        };
        FlameChart.prototype.onMouseOut = function () {
        };

        FlameChart.prototype.onDrag = function (startX, startY, currentX, currentY, deltaX, deltaY) {
          var dragInfo = this._dragInfo;
          if (dragInfo) {
            var delta = this._toTimeRelative(-deltaX);
            var windowStart = dragInfo.windowStartInitial + delta;
            var windowEnd = dragInfo.windowEndInitial + delta;
            this._controller.setWindow(windowStart, windowEnd);
          }
        };

        FlameChart.prototype.onDragEnd = function (startX, startY, currentX, currentY, deltaX, deltaY) {
          this._dragInfo = null;
          this._mouseController.updateCursor(Profiler.MouseCursor.DEFAULT);
        };

        FlameChart.prototype.onClick = function (x, y) {
          this._dragInfo = null;
          this._mouseController.updateCursor(Profiler.MouseCursor.DEFAULT);
        };

        FlameChart.prototype.onHoverStart = function (x, y) {
          var frame = this._getFrameAtPosition(x, y);
          if (frame) {
            this._hoveredFrame = frame;
            this._controller.showTooltip(this, frame, x, y);
          }
        };

        FlameChart.prototype.onHoverEnd = function () {
          if (this._hoveredFrame) {
            this._hoveredFrame = null;
            this._controller.hideTooltip();
          }
        };

        FlameChart.prototype.getStatistics = function (kind) {
          var snapshot = this._snapshot;
          if (!snapshot.statistics) {
            snapshot.calculateStatistics();
          }
          return snapshot.statistics[kind.id];
        };
        return FlameChart;
      })(Profiler.FlameChartBase);
      Profiler.FlameChart = FlameChart;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      var clamp = Shumway.NumberUtilities.clamp;

      (function (FlameChartOverviewMode) {
        FlameChartOverviewMode[FlameChartOverviewMode["OVERLAY"] = 0] = "OVERLAY";
        FlameChartOverviewMode[FlameChartOverviewMode["STACK"] = 1] = "STACK";
        FlameChartOverviewMode[FlameChartOverviewMode["UNION"] = 2] = "UNION";
      })(Profiler.FlameChartOverviewMode || (Profiler.FlameChartOverviewMode = {}));
      var FlameChartOverviewMode = Profiler.FlameChartOverviewMode;

      var FlameChartOverview = (function (_super) {
        __extends(FlameChartOverview, _super);
        function FlameChartOverview(controller, mode) {
          if (typeof mode === "undefined") { mode = 1 /* STACK */; }
          this._mode = mode;
          this._overviewCanvasDirty = true;
          this._overviewCanvas = document.createElement("canvas");
          this._overviewContext = this._overviewCanvas.getContext("2d");
          _super.call(this, controller);
        }
        FlameChartOverview.prototype.setSize = function (width, height) {
          _super.prototype.setSize.call(this, width, height || 64);
        };

        Object.defineProperty(FlameChartOverview.prototype, "mode", {
          set: function (value) {
            this._mode = value;
            this.draw();
          },
          enumerable: true,
          configurable: true
        });

        FlameChartOverview.prototype._resetCanvas = function () {
          _super.prototype._resetCanvas.call(this);
          this._overviewCanvas.width = this._canvas.width;
          this._overviewCanvas.height = this._canvas.height;
          this._overviewCanvasDirty = true;
        };

        FlameChartOverview.prototype.draw = function () {
          var context = this._context;
          var ratio = window.devicePixelRatio;
          var width = this._width;
          var height = this._height;

          context.save();
          context.scale(ratio, ratio);
          context.fillStyle = this._controller.theme.bodyBackground(1);
          context.fillRect(0, 0, width, height);
          context.restore();

          if (this._initialized) {
            if (this._overviewCanvasDirty) {
              this._drawChart();
              this._overviewCanvasDirty = false;
            }
            context.drawImage(this._overviewCanvas, 0, 0);
            this._drawSelection();
          }
        };

        FlameChartOverview.prototype._drawSelection = function () {
          var context = this._context;
          var height = this._height;
          var ratio = window.devicePixelRatio;
          var left = this._selection ? this._selection.left : this._toPixels(this._windowStart);
          var right = this._selection ? this._selection.right : this._toPixels(this._windowEnd);
          var theme = this._controller.theme;

          context.save();
          context.scale(ratio, ratio);

          if (this._selection) {
            context.fillStyle = theme.selectionText(0.15);
            context.fillRect(left, 1, right - left, height - 1);
            context.fillStyle = "rgba(133, 0, 0, 1)";
            context.fillRect(left + 0.5, 0, right - left - 1, 4);
            context.fillRect(left + 0.5, height - 4, right - left - 1, 4);
          } else {
            context.fillStyle = theme.bodyBackground(0.4);
            context.fillRect(0, 1, left, height - 1);
            context.fillRect(right, 1, this._width, height - 1);
          }

          context.beginPath();
          context.moveTo(left, 0);
          context.lineTo(left, height);
          context.moveTo(right, 0);
          context.lineTo(right, height);
          context.lineWidth = 0.5;
          context.strokeStyle = theme.foregroundTextGrey(1);
          context.stroke();

          var start = this._selection ? this._toTime(this._selection.left) : this._windowStart;
          var end = this._selection ? this._toTime(this._selection.right) : this._windowEnd;
          var time = Math.abs(end - start);
          context.fillStyle = theme.selectionText(0.5);
          context.font = '8px sans-serif';
          context.textBaseline = "alphabetic";
          context.textAlign = "end";

          context.fillText(time.toFixed(2), Math.min(left, right) - 4, 10);

          context.fillText((time / 60).toFixed(2), Math.min(left, right) - 4, 20);
          context.restore();
        };

        FlameChartOverview.prototype._drawChart = function () {
          var ratio = window.devicePixelRatio;
          var width = this._width;
          var height = this._height;
          var profile = this._controller.activeProfile;
          var samplesPerPixel = 4;
          var samplesCount = width * samplesPerPixel;
          var sampleTimeInterval = profile.totalTime / samplesCount;
          var contextOverview = this._overviewContext;
          var overviewChartColor = this._controller.theme.blueHighlight(1);

          contextOverview.save();
          contextOverview.translate(0, ratio * height);
          var yScale = -ratio * height / (profile.maxDepth - 1);
          contextOverview.scale(ratio / samplesPerPixel, yScale);
          contextOverview.clearRect(0, 0, samplesCount, profile.maxDepth - 1);
          if (this._mode == 1 /* STACK */) {
            contextOverview.scale(1, 1 / profile.snapshotCount);
          }
          for (var i = 0, n = profile.snapshotCount; i < n; i++) {
            var snapshot = profile.getSnapshotAt(i);
            if (snapshot) {
              var deepestFrame = null;
              var depth = 0;
              contextOverview.beginPath();
              contextOverview.moveTo(0, 0);
              for (var x = 0; x < samplesCount; x++) {
                var time = profile.startTime + x * sampleTimeInterval;
                if (!deepestFrame) {
                  deepestFrame = snapshot.query(time);
                } else {
                  deepestFrame = deepestFrame.queryNext(time);
                }
                depth = deepestFrame ? deepestFrame.getDepth() - 1 : 0;
                contextOverview.lineTo(x, depth);
              }
              contextOverview.lineTo(x, 0);
              contextOverview.fillStyle = overviewChartColor;
              contextOverview.fill();
              if (this._mode == 1 /* STACK */) {
                contextOverview.translate(0, -height * ratio / yScale);
              }
            }
          }

          contextOverview.restore();
        };

        FlameChartOverview.prototype._toPixelsRelative = function (time) {
          return time * this._width / (this._rangeEnd - this._rangeStart);
        };

        FlameChartOverview.prototype._toPixels = function (time) {
          return this._toPixelsRelative(time - this._rangeStart);
        };

        FlameChartOverview.prototype._toTimeRelative = function (px) {
          return px * (this._rangeEnd - this._rangeStart) / this._width;
        };

        FlameChartOverview.prototype._toTime = function (px) {
          return this._toTimeRelative(px) + this._rangeStart;
        };

        FlameChartOverview.prototype._getDragTargetUnderCursor = function (x, y) {
          if (y >= 0 && y < this._height) {
            var left = this._toPixels(this._windowStart);
            var right = this._toPixels(this._windowEnd);
            var radius = 2 + (Profiler.FlameChartBase.DRAGHANDLE_WIDTH) / 2;
            var leftHandle = (x >= left - radius && x <= left + radius);
            var rightHandle = (x >= right - radius && x <= right + radius);
            if (leftHandle && rightHandle) {
              return 4 /* HANDLE_BOTH */;
            } else if (leftHandle) {
              return 2 /* HANDLE_LEFT */;
            } else if (rightHandle) {
              return 3 /* HANDLE_RIGHT */;
            } else if (!this._windowEqRange() && x > left + radius && x < right - radius) {
              return 1 /* WINDOW */;
            }
          }
          return 0 /* NONE */;
        };

        FlameChartOverview.prototype.onMouseDown = function (x, y) {
          var dragTarget = this._getDragTargetUnderCursor(x, y);
          if (dragTarget === 0 /* NONE */) {
            this._selection = { left: x, right: x };
            this.draw();
          } else {
            if (dragTarget === 1 /* WINDOW */) {
              this._mouseController.updateCursor(Profiler.MouseCursor.GRABBING);
            }
            this._dragInfo = {
              windowStartInitial: this._windowStart,
              windowEndInitial: this._windowEnd,
              target: dragTarget
            };
          }
        };

        FlameChartOverview.prototype.onMouseMove = function (x, y) {
          var cursor = Profiler.MouseCursor.DEFAULT;
          var dragTarget = this._getDragTargetUnderCursor(x, y);
          if (dragTarget !== 0 /* NONE */ && !this._selection) {
            cursor = (dragTarget === 1 /* WINDOW */) ? Profiler.MouseCursor.GRAB : Profiler.MouseCursor.EW_RESIZE;
          }
          this._mouseController.updateCursor(cursor);
        };

        FlameChartOverview.prototype.onMouseOver = function (x, y) {
          this.onMouseMove(x, y);
        };

        FlameChartOverview.prototype.onMouseOut = function () {
          this._mouseController.updateCursor(Profiler.MouseCursor.DEFAULT);
        };

        FlameChartOverview.prototype.onDrag = function (startX, startY, currentX, currentY, deltaX, deltaY) {
          if (this._selection) {
            this._selection = { left: startX, right: clamp(currentX, 0, this._width - 1) };
            this.draw();
          } else {
            var dragInfo = this._dragInfo;
            if (dragInfo.target === 4 /* HANDLE_BOTH */) {
              if (deltaX !== 0) {
                dragInfo.target = (deltaX < 0) ? 2 /* HANDLE_LEFT */ : 3 /* HANDLE_RIGHT */;
              } else {
                return;
              }
            }
            var windowStart = this._windowStart;
            var windowEnd = this._windowEnd;
            var delta = this._toTimeRelative(deltaX);
            switch (dragInfo.target) {
              case 1 /* WINDOW */:
                windowStart = dragInfo.windowStartInitial + delta;
                windowEnd = dragInfo.windowEndInitial + delta;
                break;
              case 2 /* HANDLE_LEFT */:
                windowStart = clamp(dragInfo.windowStartInitial + delta, this._rangeStart, windowEnd - Profiler.FlameChartBase.MIN_WINDOW_LEN);
                break;
              case 3 /* HANDLE_RIGHT */:
                windowEnd = clamp(dragInfo.windowEndInitial + delta, windowStart + Profiler.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
                break;
              default:
                return;
            }
            this._controller.setWindow(windowStart, windowEnd);
          }
        };

        FlameChartOverview.prototype.onDragEnd = function (startX, startY, currentX, currentY, deltaX, deltaY) {
          if (this._selection) {
            this._selection = null;
            this._controller.setWindow(this._toTime(startX), this._toTime(currentX));
          }
          this._dragInfo = null;
          this.onMouseMove(currentX, currentY);
        };

        FlameChartOverview.prototype.onClick = function (x, y) {
          this._dragInfo = null;
          this._selection = null;
          if (!this._windowEqRange()) {
            var dragTarget = this._getDragTargetUnderCursor(x, y);
            if (dragTarget === 0 /* NONE */) {
              this._controller.moveWindowTo(this._toTime(x));
            }
            this.onMouseMove(x, y);
          }
          this.draw();
        };

        FlameChartOverview.prototype.onHoverStart = function (x, y) {
        };
        FlameChartOverview.prototype.onHoverEnd = function () {
        };
        return FlameChartOverview;
      })(Profiler.FlameChartBase);
      Profiler.FlameChartOverview = FlameChartOverview;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      var clamp = Shumway.NumberUtilities.clamp;

      (function (FlameChartHeaderType) {
        FlameChartHeaderType[FlameChartHeaderType["OVERVIEW"] = 0] = "OVERVIEW";
        FlameChartHeaderType[FlameChartHeaderType["CHART"] = 1] = "CHART";
      })(Profiler.FlameChartHeaderType || (Profiler.FlameChartHeaderType = {}));
      var FlameChartHeaderType = Profiler.FlameChartHeaderType;

      var FlameChartHeader = (function (_super) {
        __extends(FlameChartHeader, _super);
        function FlameChartHeader(controller, type) {
          this._type = type;
          _super.call(this, controller);
        }
        FlameChartHeader.prototype.draw = function () {
          var context = this._context;
          var ratio = window.devicePixelRatio;
          var width = this._width;
          var height = this._height;

          context.save();
          context.scale(ratio, ratio);
          context.fillStyle = this._controller.theme.tabToolbar(1);
          context.fillRect(0, 0, width, height);

          if (this._initialized) {
            if (this._type == 0 /* OVERVIEW */) {
              var left = this._toPixels(this._windowStart);
              var right = this._toPixels(this._windowEnd);
              context.fillStyle = this._controller.theme.bodyBackground(1);
              context.fillRect(left, 0, right - left, height);
              this._drawLabels(this._rangeStart, this._rangeEnd);
              this._drawDragHandle(left);
              this._drawDragHandle(right);
            } else {
              this._drawLabels(this._windowStart, this._windowEnd);
            }
          }

          context.restore();
        };

        FlameChartHeader.prototype._drawLabels = function (rangeStart, rangeEnd) {
          var context = this._context;
          var tickInterval = this._calculateTickInterval(rangeStart, rangeEnd);
          var tick = Math.ceil(rangeStart / tickInterval) * tickInterval;
          var showSeconds = (tickInterval >= 500);
          var divisor = showSeconds ? 1000 : 1;
          var precision = this._decimalPlaces(tickInterval / divisor);
          var unit = showSeconds ? "s" : "ms";
          var x = this._toPixels(tick);
          var y = this._height / 2;
          var theme = this._controller.theme;
          context.lineWidth = 1;
          context.strokeStyle = theme.contentTextDarkGrey(0.5);
          context.fillStyle = theme.contentTextDarkGrey(1);
          context.textAlign = "right";
          context.textBaseline = "middle";
          context.font = '11px sans-serif';
          var maxWidth = this._width + FlameChartHeader.TICK_MAX_WIDTH;
          while (x < maxWidth) {
            var tickStr = (tick / divisor).toFixed(precision) + " " + unit;
            context.fillText(tickStr, x - 7, y + 1);
            context.beginPath();
            context.moveTo(x, 0);
            context.lineTo(x, this._height + 1);
            context.closePath();
            context.stroke();
            tick += tickInterval;
            x = this._toPixels(tick);
          }
        };

        FlameChartHeader.prototype._calculateTickInterval = function (rangeStart, rangeEnd) {
          var tickCount = this._width / FlameChartHeader.TICK_MAX_WIDTH;
          var range = rangeEnd - rangeStart;
          var minimum = range / tickCount;
          var magnitude = Math.pow(10, Math.floor(Math.log(minimum) / Math.LN10));
          var residual = minimum / magnitude;
          if (residual > 5) {
            return 10 * magnitude;
          } else if (residual > 2) {
            return 5 * magnitude;
          } else if (residual > 1) {
            return 2 * magnitude;
          }
          return magnitude;
        };

        FlameChartHeader.prototype._drawDragHandle = function (pos) {
          var context = this._context;
          context.lineWidth = 2;
          context.strokeStyle = this._controller.theme.bodyBackground(1);
          context.fillStyle = this._controller.theme.foregroundTextGrey(0.7);
          this._drawRoundedRect(context, pos - Profiler.FlameChartBase.DRAGHANDLE_WIDTH / 2, 1, Profiler.FlameChartBase.DRAGHANDLE_WIDTH, this._height - 2, 2, true);
        };

        FlameChartHeader.prototype._drawRoundedRect = function (context, x, y, width, height, radius, stroke, fill) {
          if (typeof stroke === "undefined") { stroke = true; }
          if (typeof fill === "undefined") { fill = true; }
          context.beginPath();
          context.moveTo(x + radius, y);
          context.lineTo(x + width - radius, y);
          context.quadraticCurveTo(x + width, y, x + width, y + radius);
          context.lineTo(x + width, y + height - radius);
          context.quadraticCurveTo(x + width, y + height, x + width - radius, y + height);
          context.lineTo(x + radius, y + height);
          context.quadraticCurveTo(x, y + height, x, y + height - radius);
          context.lineTo(x, y + radius);
          context.quadraticCurveTo(x, y, x + radius, y);
          context.closePath();
          if (stroke) {
            context.stroke();
          }
          if (fill) {
            context.fill();
          }
        };

        FlameChartHeader.prototype._toPixelsRelative = function (time) {
          var range = (this._type === 0 /* OVERVIEW */) ? this._rangeEnd - this._rangeStart : this._windowEnd - this._windowStart;
          return time * this._width / range;
        };

        FlameChartHeader.prototype._toPixels = function (time) {
          var start = (this._type === 0 /* OVERVIEW */) ? this._rangeStart : this._windowStart;
          return this._toPixelsRelative(time - start);
        };

        FlameChartHeader.prototype._toTimeRelative = function (px) {
          var range = (this._type === 0 /* OVERVIEW */) ? this._rangeEnd - this._rangeStart : this._windowEnd - this._windowStart;
          return px * range / this._width;
        };

        FlameChartHeader.prototype._toTime = function (px) {
          var start = (this._type === 0 /* OVERVIEW */) ? this._rangeStart : this._windowStart;
          return this._toTimeRelative(px) + start;
        };

        FlameChartHeader.prototype._getDragTargetUnderCursor = function (x, y) {
          if (y >= 0 && y < this._height) {
            if (this._type === 0 /* OVERVIEW */) {
              var left = this._toPixels(this._windowStart);
              var right = this._toPixels(this._windowEnd);
              var radius = 2 + (Profiler.FlameChartBase.DRAGHANDLE_WIDTH) / 2;
              var leftHandle = (x >= left - radius && x <= left + radius);
              var rightHandle = (x >= right - radius && x <= right + radius);
              if (leftHandle && rightHandle) {
                return 4 /* HANDLE_BOTH */;
              } else if (leftHandle) {
                return 2 /* HANDLE_LEFT */;
              } else if (rightHandle) {
                return 3 /* HANDLE_RIGHT */;
              } else if (!this._windowEqRange()) {
                return 1 /* WINDOW */;
              }
            } else if (!this._windowEqRange()) {
              return 1 /* WINDOW */;
            }
          }
          return 0 /* NONE */;
        };

        FlameChartHeader.prototype.onMouseDown = function (x, y) {
          var dragTarget = this._getDragTargetUnderCursor(x, y);
          if (dragTarget === 1 /* WINDOW */) {
            this._mouseController.updateCursor(Profiler.MouseCursor.GRABBING);
          }
          this._dragInfo = {
            windowStartInitial: this._windowStart,
            windowEndInitial: this._windowEnd,
            target: dragTarget
          };
        };

        FlameChartHeader.prototype.onMouseMove = function (x, y) {
          var cursor = Profiler.MouseCursor.DEFAULT;
          var dragTarget = this._getDragTargetUnderCursor(x, y);
          if (dragTarget !== 0 /* NONE */) {
            if (dragTarget !== 1 /* WINDOW */) {
              cursor = Profiler.MouseCursor.EW_RESIZE;
            } else if (dragTarget === 1 /* WINDOW */ && !this._windowEqRange()) {
              cursor = Profiler.MouseCursor.GRAB;
            }
          }
          this._mouseController.updateCursor(cursor);
        };

        FlameChartHeader.prototype.onMouseOver = function (x, y) {
          this.onMouseMove(x, y);
        };

        FlameChartHeader.prototype.onMouseOut = function () {
          this._mouseController.updateCursor(Profiler.MouseCursor.DEFAULT);
        };

        FlameChartHeader.prototype.onDrag = function (startX, startY, currentX, currentY, deltaX, deltaY) {
          var dragInfo = this._dragInfo;
          if (dragInfo.target === 4 /* HANDLE_BOTH */) {
            if (deltaX !== 0) {
              dragInfo.target = (deltaX < 0) ? 2 /* HANDLE_LEFT */ : 3 /* HANDLE_RIGHT */;
            } else {
              return;
            }
          }
          var windowStart = this._windowStart;
          var windowEnd = this._windowEnd;
          var delta = this._toTimeRelative(deltaX);
          switch (dragInfo.target) {
            case 1 /* WINDOW */:
              var mult = (this._type === 0 /* OVERVIEW */) ? 1 : -1;
              windowStart = dragInfo.windowStartInitial + mult * delta;
              windowEnd = dragInfo.windowEndInitial + mult * delta;
              break;
            case 2 /* HANDLE_LEFT */:
              windowStart = clamp(dragInfo.windowStartInitial + delta, this._rangeStart, windowEnd - Profiler.FlameChartBase.MIN_WINDOW_LEN);
              break;
            case 3 /* HANDLE_RIGHT */:
              windowEnd = clamp(dragInfo.windowEndInitial + delta, windowStart + Profiler.FlameChartBase.MIN_WINDOW_LEN, this._rangeEnd);
              break;
            default:
              return;
          }
          this._controller.setWindow(windowStart, windowEnd);
        };

        FlameChartHeader.prototype.onDragEnd = function (startX, startY, currentX, currentY, deltaX, deltaY) {
          this._dragInfo = null;
          this.onMouseMove(currentX, currentY);
        };

        FlameChartHeader.prototype.onClick = function (x, y) {
          if (this._dragInfo.target === 1 /* WINDOW */) {
            this._mouseController.updateCursor(Profiler.MouseCursor.GRAB);
          }
        };

        FlameChartHeader.prototype.onHoverStart = function (x, y) {
        };
        FlameChartHeader.prototype.onHoverEnd = function () {
        };
        FlameChartHeader.TICK_MAX_WIDTH = 75;
        return FlameChartHeader;
      })(Profiler.FlameChartBase);
      Profiler.FlameChartHeader = FlameChartHeader;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      (function (_TraceLogger) {
        var TraceLoggerProgressInfo = (function () {
          function TraceLoggerProgressInfo(pageLoaded, threadsTotal, threadsLoaded, threadFilesTotal, threadFilesLoaded) {
            this.pageLoaded = pageLoaded;
            this.threadsTotal = threadsTotal;
            this.threadsLoaded = threadsLoaded;
            this.threadFilesTotal = threadFilesTotal;
            this.threadFilesLoaded = threadFilesLoaded;
          }
          TraceLoggerProgressInfo.prototype.toString = function () {
            return "[" + ["pageLoaded", "threadsTotal", "threadsLoaded", "threadFilesTotal", "threadFilesLoaded"].map(function (value, i, arr) {
              return value + ":" + this[value];
            }, this).join(", ") + "]";
          };
          return TraceLoggerProgressInfo;
        })();
        _TraceLogger.TraceLoggerProgressInfo = TraceLoggerProgressInfo;

        var TraceLogger = (function () {
          function TraceLogger(baseUrl) {
            this._baseUrl = baseUrl;
            this._threads = [];
            this._progressInfo = null;
          }
          TraceLogger.prototype.loadPage = function (url, callback, progress) {
            this._threads = [];
            this._pageLoadCallback = callback;
            this._pageLoadProgressCallback = progress;
            this._progressInfo = new TraceLoggerProgressInfo(false, 0, 0, 0, 0);
            this._loadData([url], this._onLoadPage.bind(this));
          };

          Object.defineProperty(TraceLogger.prototype, "buffers", {
            get: function () {
              var buffers = [];
              for (var i = 0, n = this._threads.length; i < n; i++) {
                buffers.push(this._threads[i].buffer);
              }
              return buffers;
            },
            enumerable: true,
            configurable: true
          });

          TraceLogger.prototype._onProgress = function () {
            if (this._pageLoadProgressCallback) {
              this._pageLoadProgressCallback.call(this, this._progressInfo);
            }
          };

          TraceLogger.prototype._onLoadPage = function (result) {
            if (result && result.length == 1) {
              var self = this;
              var count = 0;
              var threads = result[0];
              var threadCount = threads.length;
              this._threads = Array(threadCount);
              this._progressInfo.pageLoaded = true;
              this._progressInfo.threadsTotal = threadCount;
              for (var i = 0; i < threads.length; i++) {
                var thread = threads[i];
                var urls = [thread.dict, thread.tree];
                if (thread.corrections) {
                  urls.push(thread.corrections);
                }
                this._progressInfo.threadFilesTotal += urls.length;
                this._loadData(urls, (function (index) {
                  return function (result) {
                    if (result) {
                      var thread = new _TraceLogger.Thread(result);
                      thread.buffer.name = "Thread " + index;
                      self._threads[index] = thread;
                    }
                    count++;
                    self._progressInfo.threadsLoaded++;
                    self._onProgress();
                    if (count === threadCount) {
                      self._pageLoadCallback.call(self, null, self._threads);
                    }
                  };
                })(i), function (count) {
                  self._progressInfo.threadFilesLoaded++;
                  self._onProgress();
                });
              }
              this._onProgress();
            } else {
              this._pageLoadCallback.call(this, "Error loading page.", null);
            }
          };

          TraceLogger.prototype._loadData = function (urls, callback, progress) {
            var count = 0;
            var errors = 0;
            var expected = urls.length;
            var received = [];
            received.length = expected;
            for (var i = 0; i < expected; i++) {
              var url = this._baseUrl + urls[i];
              var isTL = /\.tl$/i.test(url);
              var xhr = new XMLHttpRequest();
              var responseType = isTL ? "arraybuffer" : "json";
              xhr.open('GET', url, true);
              xhr.responseType = responseType;
              xhr.onload = (function (index, type) {
                return function (event) {
                  if (type === "json") {
                    var json = this.response;
                    if (typeof json === "string") {
                      try  {
                        json = JSON.parse(json);
                        received[index] = json;
                      } catch (e) {
                        errors++;
                      }
                    } else {
                      received[index] = json;
                    }
                  } else {
                    received[index] = this.response;
                  }
                  ++count;
                  if (progress) {
                    progress(count);
                  }
                  if (count === expected) {
                    callback(received);
                  }
                };
              })(i, responseType);
              xhr.send();
            }
          };

          TraceLogger.colors = [
            "#0044ff", "#8c4b00", "#cc5c33", "#ff80c4", "#ffbfd9", "#ff8800", "#8c5e00", "#adcc33", "#b380ff", "#bfd9ff",
            "#ffaa00", "#8c0038", "#bf8f30", "#f780ff", "#cc99c9", "#aaff00", "#000073", "#452699", "#cc8166", "#cca799",
            "#000066", "#992626", "#cc6666", "#ccc299", "#ff6600", "#526600", "#992663", "#cc6681", "#99ccc2", "#ff0066",
            "#520066", "#269973", "#61994d", "#739699", "#ffcc00", "#006629", "#269199", "#94994d", "#738299", "#ff0000",
            "#590000", "#234d8c", "#8c6246", "#7d7399", "#ee00ff", "#00474d", "#8c2385", "#8c7546", "#7c8c69", "#eeff00",
            "#4d003d", "#662e1a", "#62468c", "#8c6969", "#6600ff", "#4c2900", "#1a6657", "#8c464f", "#8c6981", "#44ff00",
            "#401100", "#1a2466", "#663355", "#567365", "#d90074", "#403300", "#101d40", "#59562d", "#66614d", "#cc0000",
            "#002b40", "#234010", "#4c2626", "#4d5e66", "#00a3cc", "#400011", "#231040", "#4c3626", "#464359", "#0000bf",
            "#331b00", "#80e6ff", "#311a33", "#4d3939", "#a69b00", "#003329", "#80ffb2", "#331a20", "#40303d", "#00a658",
            "#40ffd9", "#ffc480", "#ffe1bf", "#332b26", "#8c2500", "#9933cc", "#80fff6", "#ffbfbf", "#303326", "#005e8c",
            "#33cc47", "#b2ff80", "#c8bfff", "#263332", "#00708c", "#cc33ad", "#ffe680", "#f2ffbf", "#262a33", "#388c00",
            "#335ccc", "#8091ff", "#bfffd9"
          ];
          return TraceLogger;
        })();
        _TraceLogger.TraceLogger = TraceLogger;
      })(Profiler.TraceLogger || (Profiler.TraceLogger = {}));
      var TraceLogger = Profiler.TraceLogger;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Profiler) {
      (function (TraceLogger) {
        var Offsets;
        (function (Offsets) {
          Offsets[Offsets["START_HI"] = 0] = "START_HI";
          Offsets[Offsets["START_LO"] = 4] = "START_LO";
          Offsets[Offsets["STOP_HI"] = 8] = "STOP_HI";
          Offsets[Offsets["STOP_LO"] = 12] = "STOP_LO";
          Offsets[Offsets["TEXTID"] = 16] = "TEXTID";
          Offsets[Offsets["NEXTID"] = 20] = "NEXTID";
        })(Offsets || (Offsets = {}));

        var Thread = (function () {
          function Thread(data) {
            if (data.length >= 2) {
              this._text = data[0];
              this._data = new DataView(data[1]);
              this._buffer = new Profiler.TimelineBuffer();
              this._walkTree(0);
            }
          }
          Object.defineProperty(Thread.prototype, "buffer", {
            get: function () {
              return this._buffer;
            },
            enumerable: true,
            configurable: true
          });

          Thread.prototype._walkTree = function (id) {
            var data = this._data;
            var buffer = this._buffer;
            do {
              var index = id * Thread.ITEM_SIZE;
              var start = data.getUint32(index + 0 /* START_HI */) * 4294967295 + data.getUint32(index + 4 /* START_LO */);
              var stop = data.getUint32(index + 8 /* STOP_HI */) * 4294967295 + data.getUint32(index + 12 /* STOP_LO */);
              var textId = data.getUint32(index + 16 /* TEXTID */);
              var nextId = data.getUint32(index + 20 /* NEXTID */);
              var hasChildren = ((textId & 1) === 1);
              textId >>>= 1;
              var text = this._text[textId];
              buffer.enter(text, null, start / 1000000);
              if (hasChildren) {
                this._walkTree(id + 1);
              }
              buffer.leave(text, null, stop / 1000000);
              id = nextId;
            } while(id !== 0);
          };
          Thread.ITEM_SIZE = 8 + 8 + 4 + 4;
          return Thread;
        })();
        TraceLogger.Thread = Thread;
      })(Profiler.TraceLogger || (Profiler.TraceLogger = {}));
      var TraceLogger = Profiler.TraceLogger;
    })(Tools.Profiler || (Tools.Profiler = {}));
    var Profiler = Tools.Profiler;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (_Terminal) {
      var clamp = Shumway.NumberUtilities.clamp;

      var Buffer = (function () {
        function Buffer() {
          this.length = 0;
          this.lines = [];
          this.format = [];
          this.time = [];
          this.repeat = [];
          this.length = 0;
        }
        Buffer.prototype.append = function (line, color) {
          var lines = this.lines;
          if (lines.length > 0 && lines[lines.length - 1] === line) {
            this.repeat[lines.length - 1]++;
            return;
          }
          this.lines.push(line);
          this.repeat.push(1);
          this.format.push(color ? { backgroundFillStyle: color } : undefined);
          this.time.push(performance.now());
          this.length++;
        };
        Buffer.prototype.get = function (i) {
          return this.lines[i];
        };
        Buffer.prototype.getFormat = function (i) {
          return this.format[i];
        };
        Buffer.prototype.getTime = function (i) {
          return this.time[i];
        };
        Buffer.prototype.getRepeat = function (i) {
          return this.repeat[i];
        };
        return Buffer;
      })();
      _Terminal.Buffer = Buffer;

      var Terminal = (function () {
        function Terminal(canvas) {
          this.lineColor = "#2A2A2A";
          this.alternateLineColor = "#262626";
          this.textColor = "#FFFFFF";
          this.selectionColor = "#96C9F3";
          this.selectionTextColor = "#000000";
          this.ratio = 1;
          this.showLineNumbers = true;
          this.showLineTime = false;
          this.showLineCounter = false;
          this.canvas = canvas;
          this.canvas.focus();
          this.context = canvas.getContext('2d', { original: true });
          this.context.fillStyle = "#FFFFFF";
          this.fontSize = 10;
          this.lineIndex = 0;
          this.pageIndex = 0;
          this.columnIndex = 0;
          this.selection = null;
          this.lineHeight = 15;
          this.hasFocus = false;

          window.addEventListener('resize', this._resizeHandler.bind(this), false);
          this._resizeHandler();

          this.textMarginLeft = 4;
          this.textMarginBottom = 4;
          this.refreshFrequency = 0;

          this.buffer = new Buffer();

          canvas.addEventListener('keydown', onKeyDown.bind(this), false);
          canvas.addEventListener('focus', onFocusIn.bind(this), false);
          canvas.addEventListener('blur', onFocusOut.bind(this), false);

          var PAGE_UP = 33;
          var PAGE_DOWN = 34;
          var HOME = 36;
          var END = 35;
          var UP = 38;
          var DOWN = 40;
          var LEFT = 37;
          var RIGHT = 39;
          var KEY_A = 65;
          var KEY_C = 67;
          var KEY_F = 70;
          var ESCAPE = 27;
          var KEY_N = 78;
          var KEY_T = 84;

          function onFocusIn(event) {
            this.hasFocus = true;
          }
          function onFocusOut(event) {
            this.hasFocus = false;
          }

          function onKeyDown(event) {
            var delta = 0;
            switch (event.keyCode) {
              case KEY_N:
                this.showLineNumbers = !this.showLineNumbers;
                break;
              case KEY_T:
                this.showLineTime = !this.showLineTime;
                break;
              case UP:
                delta = -1;
                break;
              case DOWN:
                delta = +1;
                break;
              case PAGE_UP:
                delta = -this.pageLineCount;
                break;
              case PAGE_DOWN:
                delta = this.pageLineCount;
                break;
              case HOME:
                delta = -this.lineIndex;
                break;
              case END:
                delta = this.buffer.length - this.lineIndex;
                break;
              case LEFT:
                this.columnIndex -= event.metaKey ? 10 : 1;
                if (this.columnIndex < 0) {
                  this.columnIndex = 0;
                }
                event.preventDefault();
                break;
              case RIGHT:
                this.columnIndex += event.metaKey ? 10 : 1;
                event.preventDefault();
                break;
              case KEY_A:
                if (event.metaKey) {
                  this.selection = { start: 0, end: this.buffer.length };
                  event.preventDefault();
                }
                break;
              case KEY_C:
                if (event.metaKey) {
                  var str = "";
                  if (this.selection) {
                    for (var i = this.selection.start; i <= this.selection.end; i++) {
                      str += this.buffer.get(i) + "\n";
                    }
                  } else {
                    str = this.buffer.get(this.lineIndex);
                  }
                  alert(str);
                }
                break;
              default:
                break;
            }
            if (event.metaKey) {
              delta *= this.pageLineCount;
            }
            if (delta) {
              this.scroll(delta);
              event.preventDefault();
            }
            if (delta && event.shiftKey) {
              if (!this.selection) {
                if (delta > 0) {
                  this.selection = { start: this.lineIndex - delta, end: this.lineIndex };
                } else if (delta < 0) {
                  this.selection = { start: this.lineIndex, end: this.lineIndex - delta };
                }
              } else {
                if (this.lineIndex > this.selection.start) {
                  this.selection.end = this.lineIndex;
                } else {
                  this.selection.start = this.lineIndex;
                }
              }
            } else if (delta) {
              this.selection = null;
            }
            this.paint();
          }
        }
        Terminal.prototype.resize = function () {
          this._resizeHandler();
        };

        Terminal.prototype._resizeHandler = function () {
          var parent = this.canvas.parentElement;
          var cw = parent.clientWidth;
          var ch = parent.clientHeight - 1;

          var devicePixelRatio = window.devicePixelRatio || 1;
          var backingStoreRatio = 1;
          if (devicePixelRatio !== backingStoreRatio) {
            this.ratio = devicePixelRatio / backingStoreRatio;
            this.canvas.width = cw * this.ratio;
            this.canvas.height = ch * this.ratio;
            this.canvas.style.width = cw + 'px';
            this.canvas.style.height = ch + 'px';
          } else {
            this.ratio = 1;
            this.canvas.width = cw;
            this.canvas.height = ch;
          }
          this.pageLineCount = Math.floor(this.canvas.height / this.lineHeight);
        };

        Terminal.prototype.gotoLine = function (index) {
          this.lineIndex = clamp(index, 0, this.buffer.length - 1);
        };
        Terminal.prototype.scrollIntoView = function () {
          if (this.lineIndex < this.pageIndex) {
            this.pageIndex = this.lineIndex;
          } else if (this.lineIndex >= this.pageIndex + this.pageLineCount) {
            this.pageIndex = this.lineIndex - this.pageLineCount + 1;
          }
        };
        Terminal.prototype.scroll = function (delta) {
          this.gotoLine(this.lineIndex + delta);
          this.scrollIntoView();
        };
        Terminal.prototype.paint = function () {
          var lineCount = this.pageLineCount;
          if (this.pageIndex + lineCount > this.buffer.length) {
            lineCount = this.buffer.length - this.pageIndex;
          }

          var charSize = 5;
          var lineNumberMargin = this.textMarginLeft;
          var lineTimeMargin = lineNumberMargin + (this.showLineNumbers ? (String(this.buffer.length).length + 2) * charSize : 0);
          var lineRepeatMargin = lineTimeMargin + (this.showLineTime ? charSize * 8 : 2 * charSize);
          var lineMargin = lineRepeatMargin + charSize * 5;
          this.context.font = this.fontSize + 'px Consolas, "Liberation Mono", Courier, monospace';
          this.context.setTransform(this.ratio, 0, 0, this.ratio, 0, 0);

          var w = this.canvas.width;
          var h = this.lineHeight;

          for (var i = 0; i < lineCount; i++) {
            var y = i * this.lineHeight;
            var lineIndex = this.pageIndex + i;
            var line = this.buffer.get(lineIndex);
            var lineFormat = this.buffer.getFormat(lineIndex);
            var lineRepeat = this.buffer.getRepeat(lineIndex);

            var lineTimeDelta = lineIndex > 1 ? this.buffer.getTime(lineIndex) - this.buffer.getTime(0) : 0;

            this.context.fillStyle = lineIndex % 2 ? this.lineColor : this.alternateLineColor;
            if (lineFormat && lineFormat.backgroundFillStyle) {
              this.context.fillStyle = lineFormat.backgroundFillStyle;
            }
            this.context.fillRect(0, y, w, h);
            this.context.fillStyle = this.selectionTextColor;
            this.context.fillStyle = this.textColor;

            if (this.selection && lineIndex >= this.selection.start && lineIndex <= this.selection.end) {
              this.context.fillStyle = this.selectionColor;
              this.context.fillRect(0, y, w, h);
              this.context.fillStyle = this.selectionTextColor;
            }
            if (this.hasFocus && lineIndex === this.lineIndex) {
              this.context.fillStyle = this.selectionColor;
              this.context.fillRect(0, y, w, h);
              this.context.fillStyle = this.selectionTextColor;
            }
            if (this.columnIndex > 0) {
              line = line.substring(this.columnIndex);
            }
            var marginTop = (i + 1) * this.lineHeight - this.textMarginBottom;
            if (this.showLineNumbers) {
              this.context.fillText(String(lineIndex), lineNumberMargin, marginTop);
            }
            if (this.showLineTime) {
              this.context.fillText(lineTimeDelta.toFixed(1).padLeft(' ', 6), lineTimeMargin, marginTop);
            }
            if (lineRepeat > 1) {
              this.context.fillText(String(lineRepeat).padLeft(' ', 3), lineRepeatMargin, marginTop);
            }
            this.context.fillText(line, lineMargin, marginTop);
          }
        };

        Terminal.prototype.refreshEvery = function (ms) {
          var that = this;
          this.refreshFrequency = ms;
          function refresh() {
            that.paint();
            if (that.refreshFrequency) {
              setTimeout(refresh, that.refreshFrequency);
            }
          }
          if (that.refreshFrequency) {
            setTimeout(refresh, that.refreshFrequency);
          }
        };

        Terminal.prototype.isScrolledToBottom = function () {
          return this.lineIndex === this.buffer.length - 1;
        };
        return Terminal;
      })();
      _Terminal.Terminal = Terminal;
    })(Tools.Terminal || (Tools.Terminal = {}));
    var Terminal = Tools.Terminal;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (Tools) {
    (function (Mini) {
      var FPS = (function () {
        function FPS(canvas) {
          this._index = 0;
          this._lastTime = 0;
          this._lastWeightedTime = 0;
          this._gradient = [
            "#FF0000",
            "#FF1100",
            "#FF2300",
            "#FF3400",
            "#FF4600",
            "#FF5700",
            "#FF6900",
            "#FF7B00",
            "#FF8C00",
            "#FF9E00",
            "#FFAF00",
            "#FFC100",
            "#FFD300",
            "#FFE400",
            "#FFF600",
            "#F7FF00",
            "#E5FF00",
            "#D4FF00",
            "#C2FF00",
            "#B0FF00",
            "#9FFF00",
            "#8DFF00",
            "#7CFF00",
            "#6AFF00",
            "#58FF00",
            "#47FF00",
            "#35FF00",
            "#24FF00",
            "#12FF00",
            "#00FF00"
          ];
          this._canvas = canvas;
          this._context = canvas.getContext("2d");
          window.addEventListener('resize', this._resizeHandler.bind(this), false);
          this._resizeHandler();
        }
        FPS.prototype._resizeHandler = function () {
          var parent = this._canvas.parentElement;
          var cw = parent.clientWidth;
          var ch = parent.clientHeight - 1;
          var devicePixelRatio = window.devicePixelRatio || 1;
          var backingStoreRatio = 1;
          if (devicePixelRatio !== backingStoreRatio) {
            this._ratio = devicePixelRatio / backingStoreRatio;
            this._canvas.width = cw * this._ratio;
            this._canvas.height = ch * this._ratio;
            this._canvas.style.width = cw + 'px';
            this._canvas.style.height = ch + 'px';
          } else {
            this._ratio = 1;
            this._canvas.width = cw;
            this._canvas.height = ch;
          }
        };

        FPS.prototype.tickAndRender = function (idle) {
          if (typeof idle === "undefined") { idle = false; }
          if (this._lastTime === 0) {
            this._lastTime = performance.now();
            return;
          }

          var elapsedTime = performance.now() - this._lastTime;
          var weightRatio = 0.3;
          var weightedTime = elapsedTime * (1 - weightRatio) + this._lastWeightedTime * weightRatio;

          var context = this._context;
          var w = 2 * this._ratio;
          var wPadding = 1;
          var textWidth = 20;
          var count = ((this._canvas.width - textWidth) / (w + wPadding)) | 0;

          var index = this._index++;
          if (this._index > count) {
            this._index = 0;
          }

          context.globalAlpha = 1;
          context.fillStyle = "black";
          context.fillRect(textWidth + index * (w + wPadding), 0, w * 4, this._canvas.height);

          var r = (1000 / 60) / weightedTime;
          context.fillStyle = this._gradient[r * (this._gradient.length - 1) | 0];
          context.globalAlpha = idle ? 0.2 : 1;
          var v = this._canvas.height * r | 0;

          context.fillRect(textWidth + index * (w + wPadding), 0, w, v);
          if (index % 16 === 0) {
            context.globalAlpha = 1;
            context.fillStyle = "black";
            context.fillRect(0, 0, textWidth, this._canvas.height);
            context.fillStyle = "white";
            context.font = "10px Arial";
            context.fillText((1000 / weightedTime).toFixed(0), 2, 8);
          }

          this._lastTime = performance.now();
          this._lastWeightedTime = weightedTime;
        };
        return FPS;
      })();
      Mini.FPS = FPS;
    })(Tools.Mini || (Tools.Mini = {}));
    var Mini = Tools.Mini;
  })(Shumway.Tools || (Shumway.Tools = {}));
  var Tools = Shumway.Tools;
})(Shumway || (Shumway = {}));
//# sourceMappingURL=tools.js.map

console.timeEnd("Load Parser Dependencies");
console.time("Load SWF Parser");
var Shumway;
(function (Shumway) {
  (function (SWF) {
    SWF.timelineBuffer = new Shumway.Tools.Profiler.TimelineBuffer("Parser");

    function enterTimeline(name, data) {
      profile && SWF.timelineBuffer && SWF.timelineBuffer.enter(name, data);
    }
    SWF.enterTimeline = enterTimeline;

    function leaveTimeline(data) {
      profile && SWF.timelineBuffer && SWF.timelineBuffer.leave(null, data);
    }
    SWF.leaveTimeline = leaveTimeline;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var pow = Math.pow;
      var fromCharCode = String.fromCharCode;
      var slice = Array.prototype.slice;

      function readSi8($bytes, $stream) {
        return $stream.getInt8($stream.pos++);
      }
      Parser.readSi8 = readSi8;

      function readSi16($bytes, $stream) {
        return $stream.getInt16($stream.pos, $stream.pos += 2);
      }
      Parser.readSi16 = readSi16;

      function readSi32($bytes, $stream) {
        return $stream.getInt32($stream.pos, $stream.pos += 4);
      }
      Parser.readSi32 = readSi32;

      function readUi8($bytes, $stream) {
        return $bytes[$stream.pos++];
      }
      Parser.readUi8 = readUi8;

      function readUi16($bytes, $stream) {
        return $stream.getUint16($stream.pos, $stream.pos += 2);
      }
      Parser.readUi16 = readUi16;

      function readUi32($bytes, $stream) {
        return $stream.getUint32($stream.pos, $stream.pos += 4);
      }
      Parser.readUi32 = readUi32;

      function readFixed($bytes, $stream) {
        return $stream.getInt32($stream.pos, $stream.pos += 4) / 65536;
      }
      Parser.readFixed = readFixed;

      function readFixed8($bytes, $stream) {
        return $stream.getInt16($stream.pos, $stream.pos += 2) / 256;
      }
      Parser.readFixed8 = readFixed8;

      function readFloat16($bytes, $stream) {
        var ui16 = $stream.getUint16($stream.pos);
        $stream.pos += 2;
        var sign = ui16 >> 15 ? -1 : 1;
        var exponent = (ui16 & 0x7c00) >> 10;
        var fraction = ui16 & 0x03ff;
        if (!exponent)
          return sign * pow(2, -14) * (fraction / 1024);
        if (exponent === 0x1f)
          return fraction ? NaN : sign * Infinity;
        return sign * pow(2, exponent - 15) * (1 + (fraction / 1024));
      }
      Parser.readFloat16 = readFloat16;

      function readFloat($bytes, $stream) {
        return $stream.getFloat32($stream.pos, $stream.pos += 4);
      }
      Parser.readFloat = readFloat;

      function readDouble($bytes, $stream) {
        return $stream.getFloat64($stream.pos, $stream.pos += 8);
      }
      Parser.readDouble = readDouble;

      function readEncodedU32($bytes, $stream) {
        var val = $bytes[$stream.pos++];
        if (!(val & 0x080))
          return val;
        val |= $bytes[$stream.pos++] << 7;
        if (!(val & 0x4000))
          return val;
        val |= $bytes[$stream.pos++] << 14;
        if (!(val & 0x200000))
          return val;
        val |= $bytes[$stream.pos++] << 21;
        if (!(val & 0x10000000))
          return val;
        return val | ($bytes[$stream.pos++] << 28);
      }
      Parser.readEncodedU32 = readEncodedU32;

      function readBool($bytes, $stream) {
        return !!$bytes[$stream.pos++];
      }
      Parser.readBool = readBool;

      function align($bytes, $stream) {
        $stream.align();
      }
      Parser.align = align;

      function readSb($bytes, $stream, size) {
        return (readUb($bytes, $stream, size) << (32 - size)) >> (32 - size);
      }
      Parser.readSb = readSb;

      var masks = new Uint32Array(33);
      for (var i = 1, mask = 0; i <= 32; ++i) {
        masks[i] = mask = (mask << 1) | 1;
      }

      function readUb($bytes, $stream, size) {
        var buffer = $stream.bitBuffer;
        var bitlen = $stream.bitLength;
        while (size > bitlen) {
          buffer = (buffer << 8) | $bytes[$stream.pos++];
          bitlen += 8;
        }
        bitlen -= size;
        var val = (buffer >>> bitlen) & masks[size];
        $stream.bitBuffer = buffer;
        $stream.bitLength = bitlen;
        return val;
      }
      Parser.readUb = readUb;

      function readFb($bytes, $stream, size) {
        return readSb($bytes, $stream, size) / 65536;
      }
      Parser.readFb = readFb;

      function readString($bytes, $stream, length) {
        var codes;
        var pos = $stream.pos;
        if (length) {
          codes = $bytes.subarray(pos, pos += length);
        } else {
          length = 0;
          for (var i = pos; $bytes[i]; i++) {
            length++;
          }
          codes = $bytes.subarray(pos, pos += length);
          pos++;
        }
        $stream.pos = pos;
        var str = Shumway.StringUtilities.utf8encode(codes);
        if (str.indexOf('\0') >= 0) {
          str = str.split('\0').join('');
        }
        return str;
      }
      Parser.readString = readString;

      function readBinary($bytes, $stream, size, temporaryUsage) {
        if (!size) {
          size = $stream.end - $stream.pos;
        }
        var subArray = $bytes.subarray($stream.pos, $stream.pos = ($stream.pos + size));
        if (temporaryUsage) {
          return subArray;
        }
        var result = new Uint8Array(size);
        result.set(subArray);
        return result;
      }
      Parser.readBinary = readBinary;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      function defineShape($bytes, $stream, output, swfVersion, tagCode) {
        output || (output = {});
        output.id = Parser.readUi16($bytes, $stream);
        var lineBounds = output.lineBounds = {};
        bbox($bytes, $stream, lineBounds, swfVersion, tagCode);
        var isMorph = output.isMorph = tagCode === 46 || tagCode === 84;
        if (isMorph) {
          var lineBoundsMorph = output.lineBoundsMorph = {};
          bbox($bytes, $stream, lineBoundsMorph, swfVersion, tagCode);
        }
        var canHaveStrokes = output.canHaveStrokes = tagCode === 83 || tagCode === 84;
        if (canHaveStrokes) {
          var fillBounds = output.fillBounds = {};
          bbox($bytes, $stream, fillBounds, swfVersion, tagCode);
          if (isMorph) {
            var fillBoundsMorph = output.fillBoundsMorph = {};
            bbox($bytes, $stream, fillBoundsMorph, swfVersion, tagCode);
          }
          output.flags = Parser.readUi8($bytes, $stream);
        }
        if (isMorph) {
          output.offsetMorph = Parser.readUi32($bytes, $stream);
          morphShapeWithStyle($bytes, $stream, output, swfVersion, tagCode, isMorph, canHaveStrokes);
        } else {
          shapeWithStyle($bytes, $stream, output, swfVersion, tagCode, isMorph, canHaveStrokes);
        }
        return output;
      }

      function placeObject($bytes, $stream, $, swfVersion, tagCode) {
        var flags;
        $ || ($ = {});
        if (tagCode > 4 /* CODE_PLACE_OBJECT */) {
          flags = $.flags = tagCode > 26 /* CODE_PLACE_OBJECT2 */ ? Parser.readUi16($bytes, $stream) : Parser.readUi8($bytes, $stream);
          $.depth = Parser.readUi16($bytes, $stream);
          if (flags & 2048 /* HasClassName */) {
            $.className = Parser.readString($bytes, $stream, 0);
          }
          if (flags & 2 /* HasCharacter */) {
            $.symbolId = Parser.readUi16($bytes, $stream);
          }
          if (flags & 4 /* HasMatrix */) {
            var $0 = $.matrix = {};
            matrix($bytes, $stream, $0, swfVersion, tagCode);
          }
          if (flags & 8 /* HasColorTransform */) {
            var $1 = $.cxform = {};
            cxform($bytes, $stream, $1, swfVersion, tagCode);
          }
          if (flags & 16 /* HasRatio */) {
            $.ratio = Parser.readUi16($bytes, $stream);
          }
          if (flags & 32 /* HasName */) {
            $.name = Parser.readString($bytes, $stream, 0);
          }
          if (flags & 64 /* HasClipDepth */) {
            $.clipDepth = Parser.readUi16($bytes, $stream);
          }
          if (flags & 256 /* HasFilterList */) {
            var count = Parser.readUi8($bytes, $stream);
            var $2 = $.filters = [];
            var $3 = count;
            while ($3--) {
              var $4 = {};
              anyFilter($bytes, $stream, $4, swfVersion, tagCode);
              $2.push($4);
            }
          }
          if (flags & 512 /* HasBlendMode */) {
            $.blendMode = Parser.readUi8($bytes, $stream);
          }
          if (flags & 1024 /* HasCacheAsBitmap */) {
            $.bmpCache = Parser.readUi8($bytes, $stream);
          }
          if (flags & 128 /* HasClipActions */) {
            var reserved = Parser.readUi16($bytes, $stream);
            if (swfVersion >= 6) {
              var allFlags = Parser.readUi32($bytes, $stream);
            } else {
              var allFlags = Parser.readUi16($bytes, $stream);
            }
            var $28 = $.events = [];
            do {
              var $29 = {};
              var eoe = events($bytes, $stream, $29, swfVersion, tagCode);
              $28.push($29);
            } while(!eoe);
          }
          if (flags & 1024 /* OpaqueBackground */) {
            $.backgroundColor = argb($bytes, $stream);
          }
          if (flags & 512 /* HasVisible */) {
            $.visibility = Parser.readUi8($bytes, $stream);
          }
        } else {
          $.symbolId = Parser.readUi16($bytes, $stream);
          $.depth = Parser.readUi16($bytes, $stream);
          $.flags |= 4 /* HasMatrix */;
          var $30 = $.matrix = {};
          matrix($bytes, $stream, $30, swfVersion, tagCode);
          if ($stream.remaining()) {
            $.flags |= 8 /* HasColorTransform */;
            var $31 = $.cxform = {};
            cxform($bytes, $stream, $31, swfVersion, tagCode);
          }
        }
        return $;
      }

      function removeObject($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        if (tagCode === 5) {
          $.symbolId = Parser.readUi16($bytes, $stream);
        }
        $.depth = Parser.readUi16($bytes, $stream);
        return $;
      }

      function defineImage($bytes, $stream, $, swfVersion, tagCode) {
        var imgData;
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        if (tagCode > 21) {
          var alphaDataOffset = Parser.readUi32($bytes, $stream);
          if (tagCode === 90) {
            $.deblock = Parser.readFixed8($bytes, $stream);
          }
          imgData = $.imgData = Parser.readBinary($bytes, $stream, alphaDataOffset, true);
          $.alphaData = Parser.readBinary($bytes, $stream, 0, true);
        } else {
          imgData = $.imgData = Parser.readBinary($bytes, $stream, 0, true);
        }
        switch (imgData[0] << 8 | imgData[1]) {
          case 65496:
          case 65497:
            $.mimeType = "image/jpeg";
            break;
          case 35152:
            $.mimeType = "image/png";
            break;
          case 18249:
            $.mimeType = "image/gif";
            break;
          default:
            $.mimeType = "application/octet-stream";
        }
        if (tagCode === 6) {
          $.incomplete = 1;
        }
        return $;
      }

      function defineButton($bytes, $stream, $, swfVersion, tagCode) {
        var eob;
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        if (tagCode == 7) {
          var $0 = $.characters = [];
          do {
            var $1 = {};
            var temp = button($bytes, $stream, $1, swfVersion, tagCode);
            eob = temp.eob;
            $0.push($1);
          } while(!eob);
          $.actionsData = Parser.readBinary($bytes, $stream, 0, false);
        } else {
          var trackFlags = Parser.readUi8($bytes, $stream);
          $.trackAsMenu = trackFlags >> 7 & 1;
          var actionOffset = Parser.readUi16($bytes, $stream);
          var $28 = $.characters = [];
          do {
            var $29 = {};
            var flags = Parser.readUi8($bytes, $stream);
            var eob = $29.eob = !flags;
            if (swfVersion >= 8) {
              $29.flags = (flags >> 5 & 1 ? 512 /* HasBlendMode */ : 0) | (flags >> 4 & 1 ? 256 /* HasFilterList */ : 0);
            } else {
              $29.flags = 0;
            }
            $29.stateHitTest = flags >> 3 & 1;
            $29.stateDown = flags >> 2 & 1;
            $29.stateOver = flags >> 1 & 1;
            $29.stateUp = flags & 1;
            if (!eob) {
              $29.symbolId = Parser.readUi16($bytes, $stream);
              $29.depth = Parser.readUi16($bytes, $stream);
              var $30 = $29.matrix = {};
              matrix($bytes, $stream, $30, swfVersion, tagCode);
              if (tagCode === 34) {
                var $31 = $29.cxform = {};
                cxform($bytes, $stream, $31, swfVersion, tagCode);
              }
              if ($29.flags & 256 /* HasFilterList */) {
                var count = Parser.readUi8($bytes, $stream);
                var $2 = $.filters = [];
                var $3 = count;
                while ($3--) {
                  var $4 = {};
                  anyFilter($bytes, $stream, $4, swfVersion, tagCode);
                  $2.push($4);
                }
              }
              if ($29.flags & 512 /* HasBlendMode */) {
                $29.blendMode = Parser.readUi8($bytes, $stream);
              }
            }
            $28.push($29);
          } while(!eob);
          if (!!actionOffset) {
            var $56 = $.buttonActions = [];
            do {
              var $57 = {};
              buttonCondAction($bytes, $stream, $57, swfVersion, tagCode);
              $56.push($57);
            } while($stream.remaining() > 0);
          }
        }
        return $;
      }

      function defineJPEGTables($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = 0;
        $.imgData = Parser.readBinary($bytes, $stream, 0, false);
        $.mimeType = "application/octet-stream";
        return $;
      }

      function setBackgroundColor($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.color = rgb($bytes, $stream);
        return $;
      }

      function defineBinaryData($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var reserved = Parser.readUi32($bytes, $stream);
        $.data = Parser.readBinary($bytes, $stream, 0, false);
        return $;
      }

      function defineFont($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var firstOffset = Parser.readUi16($bytes, $stream);
        var glyphCount = $.glyphCount = firstOffset / 2;
        var restOffsets = [];
        var $0 = glyphCount - 1;
        while ($0--) {
          restOffsets.push(Parser.readUi16($bytes, $stream));
        }
        $.offsets = [firstOffset].concat(restOffsets);
        var $1 = $.glyphs = [];
        var $2 = glyphCount;
        while ($2--) {
          var $3 = {};
          shape($bytes, $stream, $3, swfVersion, tagCode);
          $1.push($3);
        }
        return $;
      }

      function defineLabel($bytes, $stream, $, swfVersion, tagCode) {
        var eot;
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var $0 = $.bbox = {};
        bbox($bytes, $stream, $0, swfVersion, tagCode);
        var $1 = $.matrix = {};
        matrix($bytes, $stream, $1, swfVersion, tagCode);
        var glyphBits = $.glyphBits = Parser.readUi8($bytes, $stream);
        var advanceBits = $.advanceBits = Parser.readUi8($bytes, $stream);
        var $2 = $.records = [];
        do {
          var $3 = {};
          var temp = textRecord($bytes, $stream, $3, swfVersion, tagCode, glyphBits, advanceBits);
          eot = temp.eot;
          $2.push($3);
        } while(!eot);
        return $;
      }

      function doAction($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        if (tagCode === 59) {
          $.spriteId = Parser.readUi16($bytes, $stream);
        }
        $.actionsData = Parser.readBinary($bytes, $stream, 0, false);
        return $;
      }

      function defineSound($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var soundFlags = Parser.readUi8($bytes, $stream);
        $.soundFormat = soundFlags >> 4 & 15;
        $.soundRate = soundFlags >> 2 & 3;
        $.soundSize = soundFlags >> 1 & 1;
        $.soundType = soundFlags & 1;
        $.samplesCount = Parser.readUi32($bytes, $stream);
        $.soundData = Parser.readBinary($bytes, $stream, 0, false);
        return $;
      }

      function startSound($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        if (tagCode == 15) {
          $.soundId = Parser.readUi16($bytes, $stream);
        }
        if (tagCode == 89) {
          $.soundClassName = Parser.readString($bytes, $stream, 0);
        }
        var $0 = $.soundInfo = {};
        soundInfo($bytes, $stream, $0, swfVersion, tagCode);
        return $;
      }

      function soundStreamHead($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        var playbackFlags = Parser.readUi8($bytes, $stream);
        $.playbackRate = playbackFlags >> 2 & 3;
        $.playbackSize = playbackFlags >> 1 & 1;
        $.playbackType = playbackFlags & 1;
        var streamFlags = Parser.readUi8($bytes, $stream);
        var streamCompression = $.streamCompression = streamFlags >> 4 & 15;
        $.streamRate = streamFlags >> 2 & 3;
        $.streamSize = streamFlags >> 1 & 1;
        $.streamType = streamFlags & 1;
        $.samplesCount = Parser.readUi32($bytes, $stream);
        if (streamCompression == 2) {
          $.latencySeek = Parser.readSi16($bytes, $stream);
        }
        return $;
      }

      function soundStreamBlock($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.data = Parser.readBinary($bytes, $stream, 0, false);
        return $;
      }

      function defineBitmap($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var format = $.format = Parser.readUi8($bytes, $stream);
        $.width = Parser.readUi16($bytes, $stream);
        $.height = Parser.readUi16($bytes, $stream);
        $.hasAlpha = tagCode === 36;
        if (format === 3) {
          $.colorTableSize = Parser.readUi8($bytes, $stream);
        }
        $.bmpData = Parser.readBinary($bytes, $stream, 0, false);
        return $;
      }

      function defineText($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var $0 = $.bbox = {};
        bbox($bytes, $stream, $0, swfVersion, tagCode);
        var flags = Parser.readUi16($bytes, $stream);
        var hasText = $.hasText = flags >> 7 & 1;
        $.wordWrap = flags >> 6 & 1;
        $.multiline = flags >> 5 & 1;
        $.password = flags >> 4 & 1;
        $.readonly = flags >> 3 & 1;
        var hasColor = $.hasColor = flags >> 2 & 1;
        var hasMaxLength = $.hasMaxLength = flags >> 1 & 1;
        var hasFont = $.hasFont = flags & 1;
        var hasFontClass = $.hasFontClass = flags >> 15 & 1;
        $.autoSize = flags >> 14 & 1;
        var hasLayout = $.hasLayout = flags >> 13 & 1;
        $.noSelect = flags >> 12 & 1;
        $.border = flags >> 11 & 1;
        $.wasStatic = flags >> 10 & 1;
        $.html = flags >> 9 & 1;
        $.useOutlines = flags >> 8 & 1;
        if (hasFont) {
          $.fontId = Parser.readUi16($bytes, $stream);
        }
        if (hasFontClass) {
          $.fontClass = Parser.readString($bytes, $stream, 0);
        }
        if (hasFont) {
          $.fontHeight = Parser.readUi16($bytes, $stream);
        }
        if (hasColor) {
          $.color = rgba($bytes, $stream);
        }
        if (hasMaxLength) {
          $.maxLength = Parser.readUi16($bytes, $stream);
        }
        if (hasLayout) {
          $.align = Parser.readUi8($bytes, $stream);
          $.leftMargin = Parser.readUi16($bytes, $stream);
          $.rightMargin = Parser.readUi16($bytes, $stream);
          $.indent = Parser.readSi16($bytes, $stream);
          $.leading = Parser.readSi16($bytes, $stream);
        }
        $.variableName = Parser.readString($bytes, $stream, 0);
        if (hasText) {
          $.initialText = Parser.readString($bytes, $stream, 0);
        }
        return $;
      }

      function frameLabel($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.name = Parser.readString($bytes, $stream, 0);
        return $;
      }

      function defineFont2($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var hasLayout = $.hasLayout = Parser.readUb($bytes, $stream, 1);
        var reserved;
        if (swfVersion > 5) {
          $.shiftJis = Parser.readUb($bytes, $stream, 1);
        } else {
          reserved = Parser.readUb($bytes, $stream, 1);
        }
        $.smallText = Parser.readUb($bytes, $stream, 1);
        $.ansi = Parser.readUb($bytes, $stream, 1);
        var wideOffset = $.wideOffset = Parser.readUb($bytes, $stream, 1);
        var wide = $.wide = Parser.readUb($bytes, $stream, 1);
        $.italic = Parser.readUb($bytes, $stream, 1);
        $.bold = Parser.readUb($bytes, $stream, 1);
        if (swfVersion > 5) {
          $.language = Parser.readUi8($bytes, $stream);
        } else {
          reserved = Parser.readUi8($bytes, $stream);
          $.language = 0;
        }
        var nameLength = Parser.readUi8($bytes, $stream);
        $.name = Parser.readString($bytes, $stream, nameLength);
        if (tagCode === 75) {
          $.resolution = 20;
        }
        var glyphCount = $.glyphCount = Parser.readUi16($bytes, $stream);
        var startpos = $stream.pos;
        if (wideOffset) {
          var $0 = $.offsets = [];
          var $1 = glyphCount;
          while ($1--) {
            $0.push(Parser.readUi32($bytes, $stream));
          }
          $.mapOffset = Parser.readUi32($bytes, $stream);
        } else {
          var $2 = $.offsets = [];
          var $3 = glyphCount;
          while ($3--) {
            $2.push(Parser.readUi16($bytes, $stream));
          }
          $.mapOffset = Parser.readUi16($bytes, $stream);
        }
        var $4 = $.glyphs = [];
        var $5 = glyphCount;
        while ($5--) {
          var $6 = {};
          var dist = $.offsets[glyphCount - $5] + startpos - $stream.pos;

          if (dist === 1) {
            Parser.readUi8($bytes, $stream);
            $4.push({ "records": [{ "type": 0, "eos": true, "hasNewStyles": 0, "hasLineStyle": 0, "hasFillStyle1": 0, "hasFillStyle0": 0, "move": 0 }] });
            continue;
          }
          shape($bytes, $stream, $6, swfVersion, tagCode);
          $4.push($6);
        }
        if (wide) {
          var $47 = $.codes = [];
          var $48 = glyphCount;
          while ($48--) {
            $47.push(Parser.readUi16($bytes, $stream));
          }
        } else {
          var $49 = $.codes = [];
          var $50 = glyphCount;
          while ($50--) {
            $49.push(Parser.readUi8($bytes, $stream));
          }
        }
        if (hasLayout) {
          $.ascent = Parser.readUi16($bytes, $stream);
          $.descent = Parser.readUi16($bytes, $stream);
          $.leading = Parser.readSi16($bytes, $stream);
          var $51 = $.advance = [];
          var $52 = glyphCount;
          while ($52--) {
            $51.push(Parser.readSi16($bytes, $stream));
          }
          var $53 = $.bbox = [];
          var $54 = glyphCount;
          while ($54--) {
            var $55 = {};
            bbox($bytes, $stream, $55, swfVersion, tagCode);
            $53.push($55);
          }
          var kerningCount = Parser.readUi16($bytes, $stream);
          var $56 = $.kerning = [];
          var $57 = kerningCount;
          while ($57--) {
            var $58 = {};
            kerning($bytes, $stream, $58, swfVersion, tagCode, wide);
            $56.push($58);
          }
        }
        return $;
      }

      function defineFont4($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.id = Parser.readUi16($bytes, $stream);
        var reserved = Parser.readUb($bytes, $stream, 5);
        var hasFontData = $.hasFontData = Parser.readUb($bytes, $stream, 1);
        $.italic = Parser.readUb($bytes, $stream, 1);
        $.bold = Parser.readUb($bytes, $stream, 1);
        $.name = Parser.readString($bytes, $stream, 0);
        if (hasFontData) {
          $.data = Parser.readBinary($bytes, $stream, 0, false);
        }
        return $;
      }

      function fileAttributes($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        var reserved = Parser.readUb($bytes, $stream, 1);
        $.useDirectBlit = Parser.readUb($bytes, $stream, 1);
        $.useGpu = Parser.readUb($bytes, $stream, 1);
        $.hasMetadata = Parser.readUb($bytes, $stream, 1);
        $.doAbc = Parser.readUb($bytes, $stream, 1);
        $.noCrossDomainCaching = Parser.readUb($bytes, $stream, 1);
        $.relativeUrls = Parser.readUb($bytes, $stream, 1);
        $.network = Parser.readUb($bytes, $stream, 1);
        var pad = Parser.readUb($bytes, $stream, 24);
        return $;
      }

      function doABC($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        if (tagCode === 82) {
          $.flags = Parser.readUi32($bytes, $stream);
        } else {
          $.flags = 0;
        }
        if (tagCode === 82) {
          $.name = Parser.readString($bytes, $stream, 0);
        } else {
          $.name = "";
        }
        $.data = Parser.readBinary($bytes, $stream, 0, false);
        return $;
      }

      function exportAssets($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        var exportsCount = Parser.readUi16($bytes, $stream);
        var $0 = $.exports = [];
        var $1 = exportsCount;
        while ($1--) {
          var $2 = {};
          $2.symbolId = Parser.readUi16($bytes, $stream);
          $2.className = Parser.readString($bytes, $stream, 0);
          $0.push($2);
        }
        return $;
      }

      function symbolClass($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        var symbolCount = Parser.readUi16($bytes, $stream);
        var $0 = $.exports = [];
        var $1 = symbolCount;
        while ($1--) {
          var $2 = {};
          $2.symbolId = Parser.readUi16($bytes, $stream);
          $2.className = Parser.readString($bytes, $stream, 0);
          $0.push($2);
        }
        return $;
      }

      function defineScalingGrid($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        $.symbolId = Parser.readUi16($bytes, $stream);
        var $0 = $.splitter = {};
        bbox($bytes, $stream, $0, swfVersion, tagCode);
        return $;
      }

      function defineScene($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        var sceneCount = Parser.readEncodedU32($bytes, $stream);
        var $0 = $.scenes = [];
        var $1 = sceneCount;
        while ($1--) {
          var $2 = {};
          $2.offset = Parser.readEncodedU32($bytes, $stream);
          $2.name = Parser.readString($bytes, $stream, 0);
          $0.push($2);
        }
        var labelCount = Parser.readEncodedU32($bytes, $stream);
        var $3 = $.labels = [];
        var $4 = labelCount;
        while ($4--) {
          var $5 = {};
          $5.frame = Parser.readEncodedU32($bytes, $stream);
          $5.name = Parser.readString($bytes, $stream, 0);
          $3.push($5);
        }
        return $;
      }

      function bbox($bytes, $stream, $, swfVersion, tagCode) {
        Parser.align($bytes, $stream);
        var bits = Parser.readUb($bytes, $stream, 5);
        var xMin = Parser.readSb($bytes, $stream, bits);
        var xMax = Parser.readSb($bytes, $stream, bits);
        var yMin = Parser.readSb($bytes, $stream, bits);
        var yMax = Parser.readSb($bytes, $stream, bits);
        $.xMin = xMin;
        $.xMax = xMax;
        $.yMin = yMin;
        $.yMax = yMax;
        Parser.align($bytes, $stream);
      }

      function rgb($bytes, $stream) {
        return ((Parser.readUi8($bytes, $stream) << 24) | (Parser.readUi8($bytes, $stream) << 16) | (Parser.readUi8($bytes, $stream) << 8) | 0xff) >>> 0;
      }

      function rgba($bytes, $stream) {
        return (Parser.readUi8($bytes, $stream) << 24) | (Parser.readUi8($bytes, $stream) << 16) | (Parser.readUi8($bytes, $stream) << 8) | Parser.readUi8($bytes, $stream);
      }

      function argb($bytes, $stream) {
        return Parser.readUi8($bytes, $stream) | (Parser.readUi8($bytes, $stream) << 24) | (Parser.readUi8($bytes, $stream) << 16) | (Parser.readUi8($bytes, $stream) << 8);
      }

      function fillSolid($bytes, $stream, $, swfVersion, tagCode, isMorph) {
        if (tagCode > 22 || isMorph) {
          $.color = rgba($bytes, $stream);
        } else {
          $.color = rgb($bytes, $stream);
        }
        if (isMorph) {
          $.colorMorph = rgba($bytes, $stream);
        }
      }

      function matrix($bytes, $stream, $, swfVersion, tagCode) {
        Parser.align($bytes, $stream);
        var hasScale = Parser.readUb($bytes, $stream, 1);
        if (hasScale) {
          var bits = Parser.readUb($bytes, $stream, 5);
          $.a = Parser.readFb($bytes, $stream, bits);
          $.d = Parser.readFb($bytes, $stream, bits);
        } else {
          $.a = 1;
          $.d = 1;
        }
        var hasRotate = Parser.readUb($bytes, $stream, 1);
        if (hasRotate) {
          var bits = Parser.readUb($bytes, $stream, 5);
          $.b = Parser.readFb($bytes, $stream, bits);
          $.c = Parser.readFb($bytes, $stream, bits);
        } else {
          $.b = 0;
          $.c = 0;
        }
        var bits = Parser.readUb($bytes, $stream, 5);
        var e = Parser.readSb($bytes, $stream, bits);
        var f = Parser.readSb($bytes, $stream, bits);
        $.tx = e;
        $.ty = f;
        Parser.align($bytes, $stream);
      }

      function cxform($bytes, $stream, $, swfVersion, tagCode) {
        Parser.align($bytes, $stream);
        var hasOffsets = Parser.readUb($bytes, $stream, 1);
        var hasMultipliers = Parser.readUb($bytes, $stream, 1);
        var bits = Parser.readUb($bytes, $stream, 4);
        if (hasMultipliers) {
          $.redMultiplier = Parser.readSb($bytes, $stream, bits);
          $.greenMultiplier = Parser.readSb($bytes, $stream, bits);
          $.blueMultiplier = Parser.readSb($bytes, $stream, bits);
          if (tagCode > 4) {
            $.alphaMultiplier = Parser.readSb($bytes, $stream, bits);
          } else {
            $.alphaMultiplier = 256;
          }
        } else {
          $.redMultiplier = 256;
          $.greenMultiplier = 256;
          $.blueMultiplier = 256;
          $.alphaMultiplier = 256;
        }
        if (hasOffsets) {
          $.redOffset = Parser.readSb($bytes, $stream, bits);
          $.greenOffset = Parser.readSb($bytes, $stream, bits);
          $.blueOffset = Parser.readSb($bytes, $stream, bits);
          if (tagCode > 4) {
            $.alphaOffset = Parser.readSb($bytes, $stream, bits);
          } else {
            $.alphaOffset = 0;
          }
        } else {
          $.redOffset = 0;
          $.greenOffset = 0;
          $.blueOffset = 0;
          $.alphaOffset = 0;
        }
        Parser.align($bytes, $stream);
      }

      function fillGradient($bytes, $stream, $, swfVersion, tagCode, isMorph, type) {
        var $128 = $.matrix = {};
        matrix($bytes, $stream, $128, swfVersion, tagCode);
        if (isMorph) {
          var $129 = $.matrixMorph = {};
          matrix($bytes, $stream, $129, swfVersion, tagCode);
        }
        gradient($bytes, $stream, $, swfVersion, tagCode, isMorph, type);
      }

      function gradient($bytes, $stream, $, swfVersion, tagCode, isMorph, type) {
        if (tagCode === 83) {
          $.spreadMode = Parser.readUb($bytes, $stream, 2);
          $.interpolationMode = Parser.readUb($bytes, $stream, 2);
        } else {
          var pad = Parser.readUb($bytes, $stream, 4);
        }
        var count = $.count = Parser.readUb($bytes, $stream, 4);
        var $130 = $.records = [];
        var $131 = count;
        while ($131--) {
          var $132 = {};
          gradientRecord($bytes, $stream, $132, swfVersion, tagCode, isMorph);
          $130.push($132);
        }
        if (type === 19) {
          $.focalPoint = Parser.readSi16($bytes, $stream);
          if (isMorph) {
            $.focalPointMorph = Parser.readSi16($bytes, $stream);
          }
        }
      }

      function gradientRecord($bytes, $stream, $, swfVersion, tagCode, isMorph) {
        $.ratio = Parser.readUi8($bytes, $stream);
        if (tagCode > 22) {
          $.color = rgba($bytes, $stream);
        } else {
          $.color = rgb($bytes, $stream);
        }
        if (isMorph) {
          $.ratioMorph = Parser.readUi8($bytes, $stream);
          $.colorMorph = rgba($bytes, $stream);
        }
      }

      function morphShapeWithStyle($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes) {
        var eos, bits, temp;
        temp = styles($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes);
        var lineBits = temp.lineBits;
        var fillBits = temp.fillBits;
        var $160 = $.records = [];
        do {
          var $161 = {};
          temp = shapeRecord($bytes, $stream, $161, swfVersion, tagCode, isMorph, fillBits, lineBits, hasStrokes, bits);
          eos = temp.eos;
          var flags = temp.flags;
          var type = temp.type;
          var fillBits = temp.fillBits;
          var lineBits = temp.lineBits;
          bits = temp.bits;
          $160.push($161);
        } while(!eos);
        temp = styleBits($bytes, $stream, $, swfVersion, tagCode);
        var fillBits = temp.fillBits;
        var lineBits = temp.lineBits;
        var $162 = $.recordsMorph = [];
        do {
          var $163 = {};
          temp = shapeRecord($bytes, $stream, $163, swfVersion, tagCode, isMorph, fillBits, lineBits, hasStrokes, bits);
          eos = temp.eos;
          var flags = temp.flags;
          var type = temp.type;
          var fillBits = temp.fillBits;
          var lineBits = temp.lineBits;
          bits = temp.bits;
          $162.push($163);
        } while(!eos);
      }

      function shapeWithStyle($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes) {
        var eos, bits, temp;
        temp = styles($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes);
        var fillBits = temp.fillBits;
        var lineBits = temp.lineBits;
        var $160 = $.records = [];
        do {
          var $161 = {};
          temp = shapeRecord($bytes, $stream, $161, swfVersion, tagCode, isMorph, fillBits, lineBits, hasStrokes, bits);
          eos = temp.eos;
          var flags = temp.flags;
          var type = temp.type;
          var fillBits = temp.fillBits;
          var lineBits = temp.lineBits;
          bits = temp.bits;
          $160.push($161);
        } while(!eos);
      }

      function shapeRecord($bytes, $stream, $, swfVersion, tagCode, isMorph, fillBits, lineBits, hasStrokes, bits) {
        var eos, temp;
        var type = $.type = Parser.readUb($bytes, $stream, 1);
        var flags = Parser.readUb($bytes, $stream, 5);
        eos = $.eos = !(type || flags);
        if (type) {
          temp = shapeRecordEdge($bytes, $stream, $, swfVersion, tagCode, flags, bits);
          bits = temp.bits;
        } else {
          temp = shapeRecordSetup($bytes, $stream, $, swfVersion, tagCode, flags, isMorph, fillBits, lineBits, hasStrokes, bits);
          var fillBits = temp.fillBits;
          var lineBits = temp.lineBits;
          bits = temp.bits;
        }
        return {
          type: type,
          flags: flags,
          eos: eos,
          fillBits: fillBits,
          lineBits: lineBits,
          bits: bits
        };
      }

      function shapeRecordEdge($bytes, $stream, $, swfVersion, tagCode, flags, bits) {
        var isStraight = 0, tmp = 0, bits = 0, isGeneral = 0, isVertical = 0;
        isStraight = $.isStraight = flags >> 4;
        tmp = flags & 0x0f;
        bits = tmp + 2;
        if (isStraight) {
          isGeneral = $.isGeneral = Parser.readUb($bytes, $stream, 1);
          if (isGeneral) {
            $.deltaX = Parser.readSb($bytes, $stream, bits);
            $.deltaY = Parser.readSb($bytes, $stream, bits);
          } else {
            isVertical = $.isVertical = Parser.readUb($bytes, $stream, 1);
            if (isVertical) {
              $.deltaY = Parser.readSb($bytes, $stream, bits);
            } else {
              $.deltaX = Parser.readSb($bytes, $stream, bits);
            }
          }
        } else {
          $.controlDeltaX = Parser.readSb($bytes, $stream, bits);
          $.controlDeltaY = Parser.readSb($bytes, $stream, bits);
          $.anchorDeltaX = Parser.readSb($bytes, $stream, bits);
          $.anchorDeltaY = Parser.readSb($bytes, $stream, bits);
        }
        return { bits: bits };
      }

      function shapeRecordSetup($bytes, $stream, $, swfVersion, tagCode, flags, isMorph, fillBits, lineBits, hasStrokes, bits) {
        var hasNewStyles = 0, hasLineStyle = 0, hasFillStyle1 = 0;
        var hasFillStyle0 = 0, move = 0;
        if (tagCode > 2) {
          hasNewStyles = $.hasNewStyles = flags >> 4;
        } else {
          hasNewStyles = $.hasNewStyles = 0;
        }
        hasLineStyle = $.hasLineStyle = flags >> 3 & 1;
        hasFillStyle1 = $.hasFillStyle1 = flags >> 2 & 1;
        hasFillStyle0 = $.hasFillStyle0 = flags >> 1 & 1;
        move = $.move = flags & 1;
        if (move) {
          bits = Parser.readUb($bytes, $stream, 5);
          $.moveX = Parser.readSb($bytes, $stream, bits);
          $.moveY = Parser.readSb($bytes, $stream, bits);
        }
        if (hasFillStyle0) {
          $.fillStyle0 = Parser.readUb($bytes, $stream, fillBits);
        }
        if (hasFillStyle1) {
          $.fillStyle1 = Parser.readUb($bytes, $stream, fillBits);
        }
        if (hasLineStyle) {
          $.lineStyle = Parser.readUb($bytes, $stream, lineBits);
        }
        if (hasNewStyles) {
          var temp = styles($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes);
          lineBits = temp.lineBits;
          fillBits = temp.fillBits;
        }
        return {
          lineBits: lineBits,
          fillBits: fillBits,
          bits: bits
        };
      }

      function styles($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes) {
        fillStyleArray($bytes, $stream, $, swfVersion, tagCode, isMorph);
        lineStyleArray($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes);
        var temp = styleBits($bytes, $stream, $, swfVersion, tagCode);
        var fillBits = temp.fillBits;
        var lineBits = temp.lineBits;
        return { fillBits: fillBits, lineBits: lineBits };
      }

      function fillStyleArray($bytes, $stream, $, swfVersion, tagCode, isMorph) {
        var count;
        var tmp = Parser.readUi8($bytes, $stream);
        if (tagCode > 2 && tmp === 255) {
          count = Parser.readUi16($bytes, $stream);
        } else {
          count = tmp;
        }
        var $4 = $.fillStyles = [];
        var $5 = count;
        while ($5--) {
          var $6 = {};
          fillStyle($bytes, $stream, $6, swfVersion, tagCode, isMorph);
          $4.push($6);
        }
      }

      function lineStyleArray($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes) {
        var count;
        var tmp = Parser.readUi8($bytes, $stream);
        if (tagCode > 2 && tmp === 255) {
          count = Parser.readUi16($bytes, $stream);
        } else {
          count = tmp;
        }
        var $138 = $.lineStyles = [];
        var $139 = count;
        while ($139--) {
          var $140 = {};
          lineStyle($bytes, $stream, $140, swfVersion, tagCode, isMorph, hasStrokes);
          $138.push($140);
        }
      }

      function styleBits($bytes, $stream, $, swfVersion, tagCode) {
        Parser.align($bytes, $stream);
        var fillBits = Parser.readUb($bytes, $stream, 4);
        var lineBits = Parser.readUb($bytes, $stream, 4);
        return {
          fillBits: fillBits,
          lineBits: lineBits
        };
      }

      function fillStyle($bytes, $stream, $, swfVersion, tagCode, isMorph) {
        var type = $.type = Parser.readUi8($bytes, $stream);
        switch (type) {
          case 0:
            fillSolid($bytes, $stream, $, swfVersion, tagCode, isMorph);
            break;
          case 16:
          case 18:
          case 19:
            fillGradient($bytes, $stream, $, swfVersion, tagCode, isMorph, type);
            break;
          case 64:
          case 65:
          case 66:
          case 67:
            fillBitmap($bytes, $stream, $, swfVersion, tagCode, isMorph, type);
            break;
          default:
        }
      }

      function lineStyle($bytes, $stream, $, swfVersion, tagCode, isMorph, hasStrokes) {
        $.width = Parser.readUi16($bytes, $stream);
        if (isMorph) {
          $.widthMorph = Parser.readUi16($bytes, $stream);
        }
        if (hasStrokes) {
          Parser.align($bytes, $stream);
          $.startCapsStyle = Parser.readUb($bytes, $stream, 2);
          var jointStyle = $.jointStyle = Parser.readUb($bytes, $stream, 2);
          var hasFill = $.hasFill = Parser.readUb($bytes, $stream, 1);
          $.noHscale = Parser.readUb($bytes, $stream, 1);
          $.noVscale = Parser.readUb($bytes, $stream, 1);
          $.pixelHinting = Parser.readUb($bytes, $stream, 1);
          var reserved = Parser.readUb($bytes, $stream, 5);
          $.noClose = Parser.readUb($bytes, $stream, 1);
          $.endCapsStyle = Parser.readUb($bytes, $stream, 2);
          if (jointStyle === 2) {
            $.miterLimitFactor = Parser.readFixed8($bytes, $stream);
          }
          if (hasFill) {
            var $141 = $.fillStyle = {};
            fillStyle($bytes, $stream, $141, swfVersion, tagCode, isMorph);
          } else {
            $.color = rgba($bytes, $stream);
            if (isMorph) {
              $.colorMorph = rgba($bytes, $stream);
            }
          }
        } else {
          if (tagCode > 22) {
            $.color = rgba($bytes, $stream);
          } else {
            $.color = rgb($bytes, $stream);
          }
          if (isMorph) {
            $.colorMorph = rgba($bytes, $stream);
          }
        }
      }

      function fillBitmap($bytes, $stream, $, swfVersion, tagCode, isMorph, type) {
        $.bitmapId = Parser.readUi16($bytes, $stream);
        var $18 = $.matrix = {};
        matrix($bytes, $stream, $18, swfVersion, tagCode);
        if (isMorph) {
          var $19 = $.matrixMorph = {};
          matrix($bytes, $stream, $19, swfVersion, tagCode);
        }
        $.condition = type === 64 || type === 67;
      }

      function filterGlow($bytes, $stream, $, swfVersion, tagCode, type) {
        var count;
        if (type === 4 || type === 7) {
          count = Parser.readUi8($bytes, $stream);
        } else {
          count = 1;
        }
        var $5 = $.colors = [];
        var $6 = count;
        while ($6--) {
          $5.push(rgba($bytes, $stream));
        }
        if (type === 3) {
          $.hightlightColor = rgba($bytes, $stream);
        }
        if (type === 4 || type === 7) {
          var $9 = $.ratios = [];
          var $10 = count;
          while ($10--) {
            $9.push(Parser.readUi8($bytes, $stream));
          }
        }
        $.blurX = Parser.readFixed($bytes, $stream);
        $.blurY = Parser.readFixed($bytes, $stream);
        if (type !== 2) {
          $.angle = Parser.readFixed($bytes, $stream);
          $.distance = Parser.readFixed($bytes, $stream);
        }
        $.strength = Parser.readFixed8($bytes, $stream);
        $.inner = Parser.readUb($bytes, $stream, 1);
        $.knockout = Parser.readUb($bytes, $stream, 1);
        $.compositeSource = Parser.readUb($bytes, $stream, 1);
        if (type === 3 || type === 4 || type === 7) {
          $.onTop = Parser.readUb($bytes, $stream, 1);
          $.quality = Parser.readUb($bytes, $stream, 4);
        } else {
          $.quality = Parser.readUb($bytes, $stream, 5);
        }
      }

      function filterBlur($bytes, $stream, $, swfVersion, tagCode) {
        $.blurX = Parser.readFixed($bytes, $stream);
        $.blurY = Parser.readFixed($bytes, $stream);
        $.quality = Parser.readUb($bytes, $stream, 5);
        var reserved = Parser.readUb($bytes, $stream, 3);
      }

      function filterConvolution($bytes, $stream, $, swfVersion, tagCode) {
        var matrixX = $.matrixX = Parser.readUi8($bytes, $stream);
        var matrixY = $.matrixY = Parser.readUi8($bytes, $stream);
        $.divisor = Parser.readFloat($bytes, $stream);
        $.bias = Parser.readFloat($bytes, $stream);
        var $17 = $.matrix = [];
        var $18 = matrixX * matrixY;
        while ($18--) {
          $17.push(Parser.readFloat($bytes, $stream));
        }
        $.color = rgba($bytes, $stream);
        var reserved = Parser.readUb($bytes, $stream, 6);
        $.clamp = Parser.readUb($bytes, $stream, 1);
        $.preserveAlpha = Parser.readUb($bytes, $stream, 1);
      }

      function filterColorMatrix($bytes, $stream, $, swfVersion, tagCode) {
        var $20 = $.matrix = [];
        var $21 = 20;
        while ($21--) {
          $20.push(Parser.readFloat($bytes, $stream));
        }
      }

      function anyFilter($bytes, $stream, $, swfVersion, tagCode) {
        var type = $.type = Parser.readUi8($bytes, $stream);
        switch (type) {
          case 0:
          case 2:
          case 3:
          case 4:
          case 7:
            filterGlow($bytes, $stream, $, swfVersion, tagCode, type);
            break;
          case 1:
            filterBlur($bytes, $stream, $, swfVersion, tagCode);
            break;
          case 5:
            filterConvolution($bytes, $stream, $, swfVersion, tagCode);
            break;
          case 6:
            filterColorMatrix($bytes, $stream, $, swfVersion, tagCode);
            break;
          default:
        }
      }

      function events($bytes, $stream, $, swfVersion, tagCode) {
        var flags = swfVersion >= 6 ? Parser.readUi32($bytes, $stream) : Parser.readUi16($bytes, $stream);
        var eoe = $.eoe = !flags;
        var keyPress = 0;
        $.onKeyUp = flags >> 7 & 1;
        $.onKeyDown = flags >> 6 & 1;
        $.onMouseUp = flags >> 5 & 1;
        $.onMouseDown = flags >> 4 & 1;
        $.onMouseMove = flags >> 3 & 1;
        $.onUnload = flags >> 2 & 1;
        $.onEnterFrame = flags >> 1 & 1;
        $.onLoad = flags & 1;
        if (swfVersion >= 6) {
          $.onDragOver = flags >> 15 & 1;
          $.onRollOut = flags >> 14 & 1;
          $.onRollOver = flags >> 13 & 1;
          $.onReleaseOutside = flags >> 12 & 1;
          $.onRelease = flags >> 11 & 1;
          $.onPress = flags >> 10 & 1;
          $.onInitialize = flags >> 9 & 1;
          $.onData = flags >> 8 & 1;
          if (swfVersion >= 7) {
            $.onConstruct = flags >> 18 & 1;
          } else {
            $.onConstruct = 0;
          }
          keyPress = $.keyPress = flags >> 17 & 1;
          $.onDragOut = flags >> 16 & 1;
        }
        if (!eoe) {
          var length = $.length = Parser.readUi32($bytes, $stream);
          if (keyPress) {
            $.keyCode = Parser.readUi8($bytes, $stream);
          }
          $.actionsData = Parser.readBinary($bytes, $stream, length - +keyPress, false);
        }
        return eoe;
      }

      function kerning($bytes, $stream, $, swfVersion, tagCode, wide) {
        if (wide) {
          $.code1 = Parser.readUi16($bytes, $stream);
          $.code2 = Parser.readUi16($bytes, $stream);
        } else {
          $.code1 = Parser.readUi8($bytes, $stream);
          $.code2 = Parser.readUi8($bytes, $stream);
        }
        $.adjustment = Parser.readUi16($bytes, $stream);
      }

      function textEntry($bytes, $stream, $, swfVersion, tagCode, glyphBits, advanceBits) {
        $.glyphIndex = Parser.readUb($bytes, $stream, glyphBits);
        $.advance = Parser.readSb($bytes, $stream, advanceBits);
      }

      function textRecordSetup($bytes, $stream, $, swfVersion, tagCode, flags) {
        var hasFont = $.hasFont = flags >> 3 & 1;
        var hasColor = $.hasColor = flags >> 2 & 1;
        var hasMoveY = $.hasMoveY = flags >> 1 & 1;
        var hasMoveX = $.hasMoveX = flags & 1;
        if (hasFont) {
          $.fontId = Parser.readUi16($bytes, $stream);
        }
        if (hasColor) {
          if (tagCode === 33) {
            $.color = rgba($bytes, $stream);
          } else {
            $.color = rgb($bytes, $stream);
          }
        }
        if (hasMoveX) {
          $.moveX = Parser.readSi16($bytes, $stream);
        }
        if (hasMoveY) {
          $.moveY = Parser.readSi16($bytes, $stream);
        }
        if (hasFont) {
          $.fontHeight = Parser.readUi16($bytes, $stream);
        }
      }

      function textRecord($bytes, $stream, $, swfVersion, tagCode, glyphBits, advanceBits) {
        var glyphCount;
        Parser.align($bytes, $stream);
        var flags = Parser.readUb($bytes, $stream, 8);
        var eot = $.eot = !flags;
        textRecordSetup($bytes, $stream, $, swfVersion, tagCode, flags);
        if (!eot) {
          var tmp = Parser.readUi8($bytes, $stream);
          if (swfVersion > 6) {
            glyphCount = $.glyphCount = tmp;
          } else {
            glyphCount = $.glyphCount = tmp;
          }
          var $6 = $.entries = [];
          var $7 = glyphCount;
          while ($7--) {
            var $8 = {};
            textEntry($bytes, $stream, $8, swfVersion, tagCode, glyphBits, advanceBits);
            $6.push($8);
          }
        }
        return { eot: eot };
      }

      function soundEnvelope($bytes, $stream, $, swfVersion, tagCode) {
        $.pos44 = Parser.readUi32($bytes, $stream);
        $.volumeLeft = Parser.readUi16($bytes, $stream);
        $.volumeRight = Parser.readUi16($bytes, $stream);
      }

      function soundInfo($bytes, $stream, $, swfVersion, tagCode) {
        var reserved = Parser.readUb($bytes, $stream, 2);
        $.stop = Parser.readUb($bytes, $stream, 1);
        $.noMultiple = Parser.readUb($bytes, $stream, 1);
        var hasEnvelope = $.hasEnvelope = Parser.readUb($bytes, $stream, 1);
        var hasLoops = $.hasLoops = Parser.readUb($bytes, $stream, 1);
        var hasOutPoint = $.hasOutPoint = Parser.readUb($bytes, $stream, 1);
        var hasInPoint = $.hasInPoint = Parser.readUb($bytes, $stream, 1);
        if (hasInPoint) {
          $.inPoint = Parser.readUi32($bytes, $stream);
        }
        if (hasOutPoint) {
          $.outPoint = Parser.readUi32($bytes, $stream);
        }
        if (hasLoops) {
          $.loopCount = Parser.readUi16($bytes, $stream);
        }
        if (hasEnvelope) {
          var envelopeCount = $.envelopeCount = Parser.readUi8($bytes, $stream);
          var $1 = $.envelopes = [];
          var $2 = envelopeCount;
          while ($2--) {
            var $3 = {};
            soundEnvelope($bytes, $stream, $3, swfVersion, tagCode);
            $1.push($3);
          }
        }
      }

      function button($bytes, $stream, $, swfVersion, tagCode) {
        var flags = Parser.readUi8($bytes, $stream);
        var eob = $.eob = !flags;
        if (swfVersion >= 8) {
          $.flags = (flags >> 5 & 1 ? 512 /* HasBlendMode */ : 0) | (flags >> 4 & 1 ? 256 /* HasFilterList */ : 0);
        } else {
          $.flags = 0;
        }
        $.stateHitTest = flags >> 3 & 1;
        $.stateDown = flags >> 2 & 1;
        $.stateOver = flags >> 1 & 1;
        $.stateUp = flags & 1;
        if (!eob) {
          $.symbolId = Parser.readUi16($bytes, $stream);
          $.depth = Parser.readUi16($bytes, $stream);
          var $2 = $.matrix = {};
          matrix($bytes, $stream, $2, swfVersion, tagCode);
          if (tagCode === 34 /* CODE_DEFINE_BUTTON2 */) {
            var $3 = $.cxform = {};
            cxform($bytes, $stream, $3, swfVersion, tagCode);
          }
          if ($.flags & 256 /* HasFilterList */) {
            $.filterCount = Parser.readUi8($bytes, $stream);
            var $4 = $.filters = {};
            anyFilter($bytes, $stream, $4, swfVersion, tagCode);
          }
          if ($.flags & 512 /* HasBlendMode */) {
            $.blendMode = Parser.readUi8($bytes, $stream);
          }
        }
        return { eob: eob };
      }

      function buttonCondAction($bytes, $stream, $, swfVersion, tagCode) {
        var tagSize = Parser.readUi16($bytes, $stream);
        var conditions = Parser.readUi16($bytes, $stream);

        $.keyCode = (conditions & 0xfe00) >> 9;

        $.stateTransitionFlags = conditions & 0x1ff;

        $.actionsData = Parser.readBinary($bytes, $stream, (tagSize || 4) - 4, false);
      }

      function shape($bytes, $stream, $, swfVersion, tagCode) {
        var eos, bits, temp;
        temp = styleBits($bytes, $stream, $, swfVersion, tagCode);
        var fillBits = temp.fillBits;
        var lineBits = temp.lineBits;
        var $4 = $.records = [];
        do {
          var $5 = {};
          var isMorph = false;
          var hasStrokes = false;
          temp = shapeRecord($bytes, $stream, $5, swfVersion, tagCode, isMorph, fillBits, lineBits, hasStrokes, bits);
          eos = temp.eos;
          var fillBits = temp.fillBits;
          var lineBits = temp.lineBits;
          bits = bits;
          $4.push($5);
        } while(!eos);
      }

      Parser.tagHandler = {
        0: undefined,
        1: undefined,
        2: defineShape,
        4: placeObject,
        5: removeObject,
        6: defineImage,
        7: defineButton,
        8: defineJPEGTables,
        9: setBackgroundColor,
        10: defineFont,
        11: defineLabel,
        12: doAction,
        13: undefined,
        14: defineSound,
        15: startSound,
        17: undefined,
        18: soundStreamHead,
        19: soundStreamBlock,
        20: defineBitmap,
        21: defineImage,
        22: defineShape,
        23: undefined,
        24: undefined,
        26: placeObject,
        28: removeObject,
        32: defineShape,
        33: defineLabel,
        34: defineButton,
        35: defineImage,
        36: defineBitmap,
        37: defineText,
        39: undefined,
        43: frameLabel,
        45: soundStreamHead,
        46: defineShape,
        48: defineFont2,
        56: exportAssets,
        57: undefined,
        58: undefined,
        59: doAction,
        60: undefined,
        61: undefined,
        62: undefined,
        64: undefined,
        65: undefined,
        66: undefined,
        69: fileAttributes,
        70: placeObject,
        71: undefined,
        72: doABC,
        73: undefined,
        74: undefined,
        75: defineFont2,
        76: symbolClass,
        77: undefined,
        78: defineScalingGrid,
        82: doABC,
        83: defineShape,
        84: defineShape,
        86: defineScene,
        87: defineBinaryData,
        88: undefined,
        89: startSound,
        90: defineImage,
        91: defineFont4
      };

      function readHeader($bytes, $stream, $, swfVersion, tagCode) {
        $ || ($ = {});
        var $0 = $.bbox = {};
        Parser.align($bytes, $stream);
        var bits = Parser.readUb($bytes, $stream, 5);
        var xMin = Parser.readSb($bytes, $stream, bits);
        var xMax = Parser.readSb($bytes, $stream, bits);
        var yMin = Parser.readSb($bytes, $stream, bits);
        var yMax = Parser.readSb($bytes, $stream, bits);
        $0.xMin = xMin;
        $0.xMax = xMax;
        $0.yMin = yMin;
        $0.yMax = yMax;
        Parser.align($bytes, $stream);
        var frameRateFraction = Parser.readUi8($bytes, $stream);
        $.frameRate = Parser.readUi8($bytes, $stream) + frameRateFraction / 256;
        $.frameCount = Parser.readUi16($bytes, $stream);
        return $;
      }
      Parser.readHeader = readHeader;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var Inflate = Shumway.ArrayUtilities.Inflate;

      function readTags(context, stream, swfVersion, final, onprogress, onexception) {
        var tags = context.tags;
        var bytes = stream.bytes;
        var lastSuccessfulPosition;

        var tag = null;
        if (context._readTag) {
          tag = context._readTag;
          context._readTag = null;
        }

        try  {
          while (stream.pos < stream.end) {
            lastSuccessfulPosition = stream.pos;

            stream.ensure(2);
            var tagCodeAndLength = Parser.readUi16(bytes, stream);
            if (!tagCodeAndLength) {
              final = true;
              break;
            }

            var tagCode = tagCodeAndLength >> 6;
            var length = tagCodeAndLength & 0x3f;
            if (length === 0x3f) {
              stream.ensure(4);
              length = Parser.readUi32(bytes, stream);
            }

            if (tag) {
              if (tagCode === 1 && tag.code === 1) {
                tag.repeat++;
                stream.pos += length;
                continue;
              }
              tags.push(tag);
              if (onprogress && tag.id !== undefined) {
                context.bytesLoaded = (context.bytesTotal * stream.pos / stream.end) | 0;
                onprogress(context);
              }
              tag = null;
            }

            stream.ensure(length);
            var substream = stream.substream(stream.pos, stream.pos += length);
            var subbytes = substream.bytes;
            var nextTag = { code: tagCode };

            if (tagCode === 39 /* CODE_DEFINE_SPRITE */) {
              nextTag.type = 'sprite';
              nextTag.id = Parser.readUi16(subbytes, substream);
              nextTag.frameCount = Parser.readUi16(subbytes, substream);
              nextTag.tags = [];
              readTags(nextTag, substream, swfVersion, true, null, null);
            } else if (tagCode === 1) {
              nextTag.repeat = 1;
            } else {
              var handler = Parser.tagHandler[tagCode];
              if (handler) {
                handler(subbytes, substream, nextTag, swfVersion, tagCode);
              }
            }

            tag = nextTag;
          }
          if ((tag && final) || (stream.pos >= stream.end)) {
            if (tag) {
              tag.finalTag = true;
              tags.push(tag);
            }
            if (onprogress) {
              context.bytesLoaded = context.bytesTotal;
              onprogress(context);
            }
          } else {
            context._readTag = tag;
          }
        } catch (e) {
          if (e !== SWF.StreamNoDataError) {
            onexception && onexception(e);
            throw e;
          }

          stream.pos = lastSuccessfulPosition;
          context._readTag = tag;
        }
      }

      var HeadTailBuffer = (function () {
        function HeadTailBuffer(defaultSize) {
          if (typeof defaultSize === "undefined") { defaultSize = 16; }
          this._bufferSize = defaultSize;
          this._buffer = new Uint8Array(this._bufferSize);
          this._pos = 0;
        }
        HeadTailBuffer.prototype.push = function (data, need) {
          var bufferLengthNeed = this._pos + data.length;
          if (this._bufferSize < bufferLengthNeed) {
            var newBufferSize = this._bufferSize;
            while (newBufferSize < bufferLengthNeed) {
              newBufferSize <<= 1;
            }
            var newBuffer = new Uint8Array(newBufferSize);
            if (this._bufferSize > 0) {
              newBuffer.set(this._buffer);
            }
            this._buffer = newBuffer;
            this._bufferSize = newBufferSize;
          }
          this._buffer.set(data, this._pos);
          this._pos += data.length;
          if (need) {
            return this._pos >= need;
          }
        };

        HeadTailBuffer.prototype.getHead = function (size) {
          return this._buffer.subarray(0, size);
        };

        HeadTailBuffer.prototype.getTail = function (offset) {
          return this._buffer.subarray(offset, this._pos);
        };

        HeadTailBuffer.prototype.removeHead = function (size) {
          var tail = this.getTail(size);
          this._buffer = new Uint8Array(this._bufferSize);
          this._buffer.set(tail);
          this._pos = tail.length;
        };

        Object.defineProperty(HeadTailBuffer.prototype, "arrayBuffer", {
          get: function () {
            return this._buffer.buffer;
          },
          enumerable: true,
          configurable: true
        });

        Object.defineProperty(HeadTailBuffer.prototype, "length", {
          get: function () {
            return this._pos;
          },
          enumerable: true,
          configurable: true
        });

        HeadTailBuffer.prototype.getBytes = function () {
          return this._buffer.subarray(0, this._pos);
        };

        HeadTailBuffer.prototype.createStream = function () {
          return new SWF.Stream(this.arrayBuffer, 0, this.length);
        };
        return HeadTailBuffer;
      })();

      var CompressedPipe = (function () {
        function CompressedPipe(target) {
          this._inflate = new Inflate(true);
          this._inflate.onData = function (data) {
            target.push(data, this._progressInfo);
          }.bind(this);
        }
        CompressedPipe.prototype.push = function (data, progressInfo) {
          this._progressInfo = progressInfo;
          this._inflate.push(data);
        };

        CompressedPipe.prototype.close = function () {
          this._inflate = null;
        };
        return CompressedPipe;
      })();

      var BodyParser = (function () {
        function BodyParser(swfVersion, length, options) {
          this.swf = {
            swfVersion: swfVersion,
            parseTime: 0,
            bytesLoaded: undefined,
            bytesTotal: undefined,
            fileAttributes: undefined,
            tags: undefined
          };
          this._buffer = new HeadTailBuffer(32768);
          this._initialize = true;
          this._totalRead = 0;
          this._length = length;
          this._options = options;
        }
        BodyParser.prototype.push = function (data, progressInfo) {
          if (data.length === 0) {
            return;
          }

          var swf = this.swf;
          var swfVersion = swf.swfVersion;
          var buffer = this._buffer;
          var options = this._options;
          var stream;

          var finalBlock = false;
          if (progressInfo) {
            swf.bytesLoaded = progressInfo.bytesLoaded;
            swf.bytesTotal = progressInfo.bytesTotal;
            finalBlock = progressInfo.bytesLoaded >= progressInfo.bytesTotal;
          }

          if (this._initialize) {
            var PREFETCH_SIZE = 17 + 4 + 6;
            if (!buffer.push(data, PREFETCH_SIZE))
              return;

            stream = buffer.createStream();
            var bytes = stream.bytes;
            Parser.readHeader(bytes, stream, swf, null, null);

            var nextTagHeader = Parser.readUi16(bytes, stream);
            var FILE_ATTRIBUTES_LENGTH = 4;
            if (nextTagHeader == ((69 /* CODE_FILE_ATTRIBUTES */ << 6) | FILE_ATTRIBUTES_LENGTH)) {
              stream.ensure(FILE_ATTRIBUTES_LENGTH);
              var substream = stream.substream(stream.pos, stream.pos += FILE_ATTRIBUTES_LENGTH);
              var handler = Parser.tagHandler[69 /* CODE_FILE_ATTRIBUTES */];
              var fileAttributesTag = { code: 69 /* CODE_FILE_ATTRIBUTES */ };
              handler(substream.bytes, substream, fileAttributesTag, swfVersion, 69 /* CODE_FILE_ATTRIBUTES */);
              swf.fileAttributes = fileAttributesTag;
            } else {
              stream.pos -= 2;
              swf.fileAttributes = {};
            }

            if (options.onstart)
              options.onstart(swf);

            swf.tags = [];

            this._initialize = false;
          } else {
            buffer.push(data);
            stream = buffer.createStream();
          }

          var readStartTime = performance.now();
          readTags(swf, stream, swfVersion, finalBlock, options.onprogress, options.onexception);
          swf.parseTime += performance.now() - readStartTime;

          var read = stream.pos;
          buffer.removeHead(read);
          this._totalRead += read;

          if (options.oncomplete && swf.tags[swf.tags.length - 1].finalTag) {
            options.oncomplete(swf);
          }
        };

        BodyParser.prototype.close = function () {
        };
        return BodyParser;
      })();

      function parseAsync(options) {
        var buffer = new HeadTailBuffer();
        var target = null;

        var pipe = {
          push: function (data, progressInfo) {
            if (target !== null) {
              return target.push(data, progressInfo);
            }
            if (!buffer.push(data, 8)) {
              return null;
            }
            var bytes = buffer.getHead(8);
            var magic1 = bytes[0];
            var magic2 = bytes[1];
            var magic3 = bytes[2];

            if ((magic1 === 70 || magic1 === 67) && magic2 === 87 && magic3 === 83) {
              var swfVersion = bytes[3];
              var compressed = magic1 === 67;
              parseSWF(compressed, swfVersion, progressInfo);
              buffer = null;
              return;
            }

            var isImage = false;
            var imageType;

            if (magic1 === 0xff && magic2 === 0xd8 && magic3 === 0xff) {
              isImage = true;
              imageType = 'image/jpeg';
            } else if (magic1 === 0x89 && magic2 === 0x50 && magic3 === 0x4e) {
              isImage = true;
              imageType = 'image/png';
            }

            if (isImage) {
              parseImage(data, progressInfo.bytesTotal, imageType);
            }
            buffer = null;
          },
          close: function () {
            if (buffer) {
              var symbol = {
                command: 'empty',
                data: buffer.getBytes()
              };
              options.oncomplete && options.oncomplete(symbol);
            }
            if (this.target !== undefined && this.target.close) {
              this.target.close();
            }
          }
        };

        function parseSWF(compressed, swfVersion, progressInfo) {
          var stream = buffer.createStream();
          stream.pos += 4;
          var fileLength = Parser.readUi32(null, stream);
          var bodyLength = fileLength - 8;

          target = new BodyParser(swfVersion, bodyLength, options);
          if (compressed) {
            target = new CompressedPipe(target);
          }
          target.push(buffer.getTail(8), progressInfo);
        }

        function parseImage(data, bytesTotal, type) {
          var buffer = new Uint8Array(bytesTotal);
          buffer.set(data);
          var bufferPos = data.length;

          target = {
            push: function (data) {
              buffer.set(data, bufferPos);
              bufferPos += data.length;
            },
            close: function () {
              var props = {};
              var chunks;
              if (type == 'image/jpeg') {
                chunks = Parser.parseJpegChunks(props, buffer);
              } else {
                chunks = [buffer];
              }
              var symbol = {
                type: 'image',
                props: props,
                data: new Blob(chunks, { type: type })
              };
              options.oncomplete && options.oncomplete(symbol);
            }
          };
        }

        return pipe;
      }
      Parser.parseAsync = parseAsync;

      function parse(buffer, options) {
        if (typeof options === "undefined") { options = {}; }
        var pipe = parseAsync(options);
        var bytes = new Uint8Array(buffer);
        var progressInfo = { bytesLoaded: bytes.length, bytesTotal: bytes.length };
        pipe.push(bytes, progressInfo);
        pipe.close();
      }
      Parser.parse = parse;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var assert = Shumway.Debug.assert;
      var assertUnreachable = Shumway.Debug.assertUnreachable;
      var roundToMultipleOfFour = Shumway.IntegerUtilities.roundToMultipleOfFour;
      var Inflate = Shumway.ArrayUtilities.Inflate;

      (function (BitmapFormat) {
        BitmapFormat[BitmapFormat["FORMAT_COLORMAPPED"] = 3] = "FORMAT_COLORMAPPED";

        BitmapFormat[BitmapFormat["FORMAT_15BPP"] = 4] = "FORMAT_15BPP";

        BitmapFormat[BitmapFormat["FORMAT_24BPP"] = 5] = "FORMAT_24BPP";
      })(Parser.BitmapFormat || (Parser.BitmapFormat = {}));
      var BitmapFormat = Parser.BitmapFormat;

      var FACTOR_5BBP = 255 / 31;

      function parseColorMapped(tag) {
        var width = tag.width, height = tag.height;
        var hasAlpha = tag.hasAlpha;

        var padding = roundToMultipleOfFour(width) - width;
        var colorTableLength = tag.colorTableSize + 1;
        var colorTableEntrySize = hasAlpha ? 4 : 3;
        var colorTableSize = roundToMultipleOfFour(colorTableLength * colorTableEntrySize);

        var dataSize = colorTableSize + ((width + padding) * height);

        var bytes = Inflate.inflate(tag.bmpData, dataSize, true);

        var view = new Uint32Array(width * height);

        var p = colorTableSize, i = 0, offset = 0;
        if (hasAlpha) {
          for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
              offset = bytes[p++] << 2;
              var a = bytes[offset + 3];
              var r = bytes[offset + 0];
              var g = bytes[offset + 1];
              var b = bytes[offset + 2];
              view[i++] = b << 24 | g << 16 | r << 8 | a;
            }
            p += padding;
          }
        } else {
          for (var y = 0; y < height; y++) {
            for (var x = 0; x < width; x++) {
              offset = bytes[p++] * colorTableEntrySize;
              var a = 0xff;
              var r = bytes[offset + 0];
              var g = bytes[offset + 1];
              var b = bytes[offset + 2];
              view[i++] = b << 24 | g << 16 | r << 8 | a;
            }
            p += padding;
          }
        }
        release || assert(p === dataSize, "We should be at the end of the data buffer now.");
        release || assert(i === width * height, "Should have filled the entire image.");
        return new Uint8Array(view.buffer);
      }

      function parse24BPP(tag) {
        var width = tag.width, height = tag.height;
        var hasAlpha = tag.hasAlpha;

        var dataSize = height * width * 4;

        var bytes = Inflate.inflate(tag.bmpData, dataSize, true);
        if (hasAlpha) {
          return bytes;
        }
        var view = new Uint32Array(width * height);
        var length = width * height, p = 0;

        for (var i = 0; i < length; i++) {
          p++;
          var r = bytes[p++];
          var g = bytes[p++];
          var b = bytes[p++];
          view[i] = b << 24 | g << 16 | r << 8 | 0xff;
        }
        release || assert(p === dataSize, "We should be at the end of the data buffer now.");
        return new Uint8Array(view.buffer);
      }

      function parse15BPP(tag) {
        Shumway.Debug.notImplemented("parse15BPP");

        return null;
      }

      function defineBitmap(tag) {
        SWF.enterTimeline("defineBitmap");
        var bmpData = tag.bmpData;
        var data;
        var type = 0 /* None */;
        switch (tag.format) {
          case 3 /* FORMAT_COLORMAPPED */:
            data = parseColorMapped(tag);
            type = 1 /* PremultipliedAlphaARGB */;
            break;
          case 5 /* FORMAT_24BPP */:
            data = parse24BPP(tag);
            type = 1 /* PremultipliedAlphaARGB */;
            break;
          case 4 /* FORMAT_15BPP */:
            data = parse15BPP(tag);
            type = 1 /* PremultipliedAlphaARGB */;
            break;
          default:
            release || assertUnreachable('invalid bitmap format');
        }
        SWF.leaveTimeline();
        return {
          type: 'image',
          id: tag.id,
          width: tag.width,
          height: tag.height,
          mimeType: 'application/octet-stream',
          data: data,
          dataType: type
        };
      }
      Parser.defineBitmap = defineBitmap;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var assert = Shumway.Debug.assert;

      function defineButton(tag, dictionary) {
        var characters = tag.characters;
        var states = {
          up: [],
          over: [],
          down: [],
          hitTest: []
        };
        var i = 0, character;
        while ((character = characters[i++])) {
          if (character.eob)
            break;
          var characterItem = dictionary[character.symbolId];
          release || assert(characterItem, 'undefined character button');
          var cmd = {
            symbolId: characterItem.id,
            depth: character.depth,
            flags: character.matrix ? 4 /* HasMatrix */ : 0,
            matrix: character.matrix
          };
          if (character.stateUp)
            states.up.push(cmd);
          if (character.stateOver)
            states.over.push(cmd);
          if (character.stateDown)
            states.down.push(cmd);
          if (character.stateHitTest)
            states.hitTest.push(cmd);
        }
        var button = {
          type: 'button',
          id: tag.id,
          buttonActions: tag.buttonActions,
          states: states
        };
        return button;
      }
      Parser.defineButton = defineButton;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var pow = Math.pow;
      var min = Math.min;
      var max = Math.max;
      var logE = Math.log;
      var fromCharCode = String.fromCharCode;

      var nextFontId = 1;

      function maxPower2(num) {
        var maxPower = 0;
        var val = num;
        while (val >= 2) {
          val /= 2;
          ++maxPower;
        }
        return pow(2, maxPower);
      }
      function toString16(val) {
        return fromCharCode((val >> 8) & 0xff, val & 0xff);
      }
      function toString32(val) {
        return toString16(val >> 16) + toString16(val);
      }

      function defineFont(tag, dictionary) {
        var uniqueName = 'swf-font-' + tag.id;
        var fontName = tag.name || uniqueName;

        var font = {
          type: 'font',
          id: tag.id,
          name: fontName,
          bold: tag.bold === 1,
          italic: tag.italic === 1,
          codes: null,
          metrics: null,
          data: tag.data
        };

        var glyphs = tag.glyphs;
        var glyphCount = glyphs ? tag.glyphCount = glyphs.length : 0;

        if (!glyphCount) {
          return font;
        }

        var tables = {};
        var codes = [];
        var glyphIndex = {};
        var ranges = [];

        var originalCode;
        var generateAdvancement = !('advance' in tag);
        var correction = 0;
        var isFont2 = (tag.code === 48);
        var isFont3 = (tag.code === 75);

        if (generateAdvancement) {
          tag.advance = [];
        }

        var maxCode = Math.max.apply(null, tag.codes) || 35;

        if (tag.codes) {
          for (var i = 0; i < tag.codes.length; i++) {
            var code = tag.codes[i];
            if (code < 32) {
              maxCode++;
              if (maxCode == 8232) {
                maxCode = 8240;
              }
              code = maxCode;
            }
            codes.push(code);
            glyphIndex[code] = i;
          }

          originalCode = codes.concat();

          codes.sort(function (a, b) {
            return a - b;
          });
          var i = 0;
          var code;
          var indices;
          while ((code = codes[i++]) !== undefined) {
            var start = code;
            var end = start;
            indices = [i - 1];
            while (((code = codes[i]) !== undefined) && end + 1 === code) {
              ++end;
              indices.push(i);
              ++i;
            }
            ranges.push([start, end, indices]);
          }
        } else {
          indices = [];
          var UAC_OFFSET = 0xe000;
          for (var i = 0; i < glyphCount; i++) {
            code = UAC_OFFSET + i;
            codes.push(code);
            glyphIndex[code] = i;
            indices.push(i);
          }
          ranges.push([UAC_OFFSET, UAC_OFFSET + glyphCount - 1, indices]);
          originalCode = codes.concat();
        }

        var resolution = tag.resolution || 1;
        if (isFont2 && !tag.hasLayout) {
          resolution = 20;
        }
        var ascent = Math.ceil(tag.ascent / resolution) || 1024;
        var descent = -Math.ceil(tag.descent / resolution) || 0;
        var leading = (tag.leading / resolution) || 0;
        tables['OS/2'] = '';

        var startCount = '';
        var endCount = '';
        var idDelta = '';
        var idRangeOffset = '';
        var i = 0;
        var range;
        while ((range = ranges[i++])) {
          var start = range[0];
          var end = range[1];
          var code = range[2][0];
          startCount += toString16(start);
          endCount += toString16(end);
          idDelta += toString16(((code - start) + 1) & 0xffff);
          idRangeOffset += toString16(0);
        }
        endCount += '\xff\xff';
        startCount += '\xff\xff';
        idDelta += '\x00\x01';
        idRangeOffset += '\x00\x00';
        var segCount = ranges.length + 1;
        var searchRange = maxPower2(segCount) * 2;
        var rangeShift = (2 * segCount) - searchRange;
        var format314 = '\x00\x00' + toString16(segCount * 2) + toString16(searchRange) + toString16(logE(segCount) / logE(2)) + toString16(rangeShift) + endCount + '\x00\x00' + startCount + idDelta + idRangeOffset;
        tables['cmap'] = '\x00\x00' + '\x00\x01' + '\x00\x03' + '\x00\x01' + '\x00\x00\x00\x0c' + '\x00\x04' + toString16(format314.length + 4) + format314;

        var glyf = '\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x31\x00';
        var loca = '\x00\x00';
        var offset = 16;
        var maxPoints = 0;
        var xMins = [];
        var xMaxs = [];
        var yMins = [];
        var yMaxs = [];
        var maxContours = 0;
        var i = 0;
        var code;
        var rawData = {};
        while ((code = codes[i++]) !== undefined) {
          var glyph = glyphs[glyphIndex[code]];
          var records = glyph.records;
          var x = 0;
          var y = 0;

          var myFlags = '';
          var myEndpts = '';
          var endPoint = 0;
          var segments = [];
          var segmentIndex = -1;

          for (var j = 0; j < records.length; j++) {
            record = records[j];
            if (record.type) {
              if (segmentIndex < 0) {
                segmentIndex = 0;
                segments[segmentIndex] = { data: [], commands: [], xMin: 0, xMax: 0, yMin: 0, yMax: 0 };
              }
              if (record.isStraight) {
                segments[segmentIndex].commands.push(2);
                var dx = (record.deltaX || 0) / resolution;
                var dy = -(record.deltaY || 0) / resolution;
                x += dx;
                y += dy;
                segments[segmentIndex].data.push(x, y);
              } else {
                segments[segmentIndex].commands.push(3);
                var cx = record.controlDeltaX / resolution;
                var cy = -record.controlDeltaY / resolution;
                x += cx;
                y += cy;
                segments[segmentIndex].data.push(x, y);
                var dx = record.anchorDeltaX / resolution;
                var dy = -record.anchorDeltaY / resolution;
                x += dx;
                y += dy;
                segments[segmentIndex].data.push(x, y);
              }
            } else {
              if (record.eos) {
                break;
              }
              if (record.move) {
                segmentIndex++;
                segments[segmentIndex] = { data: [], commands: [], xMin: 0, xMax: 0, yMin: 0, yMax: 0 };
                segments[segmentIndex].commands.push(1);
                var moveX = record.moveX / resolution;
                var moveY = -record.moveY / resolution;
                var dx = moveX - x;
                var dy = moveY - y;
                x = moveX;
                y = moveY;
                segments[segmentIndex].data.push(x, y);
              }
            }

            if (segmentIndex > -1) {
              if (segments[segmentIndex].xMin > x) {
                segments[segmentIndex].xMin = x;
              }
              if (segments[segmentIndex].yMin > y) {
                segments[segmentIndex].yMin = y;
              }
              if (segments[segmentIndex].xMax < x) {
                segments[segmentIndex].xMax = x;
              }
              if (segments[segmentIndex].yMax < y) {
                segments[segmentIndex].yMax = y;
              }
            }
          }

          if (!isFont3) {
            segments.sort(function (a, b) {
              return (b.xMax - b.xMin) * (b.yMax - b.yMin) - (a.xMax - a.xMin) * (a.yMax - a.yMin);
            });
          }

          rawData[code] = segments;
        }

        i = 0;
        while ((code = codes[i++]) !== undefined) {
          var glyph = glyphs[glyphIndex[code]];
          var records = glyph.records;
          segments = rawData[code];
          var numberOfContours = 1;
          var endPoint = 0;
          var endPtsOfContours = '';
          var flags = '';
          var xCoordinates = '';
          var yCoordinates = '';
          var x = 0;
          var y = 0;
          var xMin = 0;
          var xMax = -1024;
          var yMin = 0;
          var yMax = -1024;

          var myFlags = '';
          var myEndpts = '';
          var endPoint = 0;
          var segmentIndex = -1;

          var data = [];
          var commands = [];

          for (j = 0; j < segments.length; j++) {
            data = data.concat(segments[j].data);
            commands = commands.concat(segments[j].commands);
          }

          x = 0;
          y = 0;
          var nx = 0;
          var ny = 0;
          var myXCoordinates = '';
          var myYCoordinates = '';
          var dataIndex = 0;
          var endPoint = 0;
          var numberOfContours = 1;
          var myEndpts = '';
          for (j = 0; j < commands.length; j++) {
            var command = commands[j];
            if (command === 1) {
              if (endPoint) {
                ++numberOfContours;
                myEndpts += toString16(endPoint - 1);
              }
              nx = data[dataIndex++];
              ny = data[dataIndex++];
              var dx = nx - x;
              var dy = ny - y;
              myFlags += '\x01';
              myXCoordinates += toString16(dx);
              myYCoordinates += toString16(dy);
              x = nx;
              y = ny;
            } else if (command === 2) {
              nx = data[dataIndex++];
              ny = data[dataIndex++];
              var dx = nx - x;
              var dy = ny - y;
              myFlags += '\x01';
              myXCoordinates += toString16(dx);
              myYCoordinates += toString16(dy);
              x = nx;
              y = ny;
            } else if (command === 3) {
              nx = data[dataIndex++];
              ny = data[dataIndex++];
              var cx = nx - x;
              var cy = ny - y;
              myFlags += '\x00';
              myXCoordinates += toString16(cx);
              myYCoordinates += toString16(cy);
              x = nx;
              y = ny;
              endPoint++;

              nx = data[dataIndex++];
              ny = data[dataIndex++];
              var cx = nx - x;
              var cy = ny - y;
              myFlags += '\x01';
              myXCoordinates += toString16(cx);
              myYCoordinates += toString16(cy);
              x = nx;
              y = ny;
            }
            endPoint++;
            if (endPoint > maxPoints) {
              maxPoints = endPoint;
            }
            if (xMin > x) {
              xMin = x;
            }
            if (yMin > y) {
              yMin = y;
            }
            if (xMax < x) {
              xMax = x;
            }
            if (yMax < y) {
              yMax = y;
            }
          }
          myEndpts += toString16((endPoint || 1) - 1);

          endPtsOfContours = myEndpts;
          xCoordinates = myXCoordinates;
          yCoordinates = myYCoordinates;
          flags = myFlags;

          if (!j) {
            xMin = xMax = yMin = yMax = 0;
            flags += '\x31';
          }
          var entry = toString16(numberOfContours) + toString16(xMin) + toString16(yMin) + toString16(xMax) + toString16(yMax) + endPtsOfContours + '\x00\x00' + flags + xCoordinates + yCoordinates;
          if (entry.length & 1) {
            entry += '\x00';
          }
          glyf += entry;
          loca += toString16(offset / 2);
          offset += entry.length;
          xMins.push(xMin);
          xMaxs.push(xMax);
          yMins.push(yMin);
          yMaxs.push(yMax);
          if (numberOfContours > maxContours) {
            maxContours = numberOfContours;
          }
          if (endPoint > maxPoints) {
            maxPoints = endPoint;
          }
          if (generateAdvancement) {
            tag.advance.push((xMax - xMin) * resolution * 1.3);
          }
        }
        loca += toString16(offset / 2);
        tables['glyf'] = glyf;

        if (!isFont3) {
          var minYmin = Math.min.apply(null, yMins);
          if (minYmin < 0) {
            descent = descent || minYmin;
          }
        }

        tables['OS/2'] = '\x00\x01' + '\x00\x00' + toString16(tag.bold ? 700 : 400) + '\x00\x05' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00\x00\x00' + 'ALF ' + toString16((tag.italic ? 0x01 : 0) | (tag.bold ? 0x20 : 0)) + toString16(codes[0]) + toString16(codes[codes.length - 1]) + toString16(ascent) + toString16(descent) + toString16(leading) + toString16(ascent) + toString16(-descent) + '\x00\x00\x00\x00' + '\x00\x00\x00\x00';

        tables['head'] = '\x00\x01\x00\x00' + '\x00\x01\x00\x00' + '\x00\x00\x00\x00' + '\x5f\x0f\x3c\xf5' + '\x00\x0b' + '\x04\x00' + '\x00\x00\x00\x00' + toString32(Date.now()) + '\x00\x00\x00\x00' + toString32(Date.now()) + toString16(min.apply(null, xMins)) + toString16(min.apply(null, yMins)) + toString16(max.apply(null, xMaxs)) + toString16(max.apply(null, yMaxs)) + toString16((tag.italic ? 2 : 0) | (tag.bold ? 1 : 0)) + '\x00\x08' + '\x00\x02' + '\x00\x00' + '\x00\x00';

        var advance = tag.advance;
        tables['hhea'] = '\x00\x01\x00\x00' + toString16(ascent) + toString16(descent) + toString16(leading) + toString16(advance ? max.apply(null, advance) : 1024) + '\x00\x00' + '\x00\x00' + '\x03\xb8' + '\x00\x01' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + toString16(glyphCount + 1);

        var hmtx = '\x00\x00\x00\x00';
        for (var i = 0; i < glyphCount; ++i) {
          hmtx += toString16(advance ? (advance[i] / resolution) : 1024) + '\x00\x00';
        }
        tables['hmtx'] = hmtx;

        if (tag.kerning) {
          var kerning = tag.kerning;
          var nPairs = kerning.length;
          var searchRange = maxPower2(nPairs) * 2;
          var kern = '\x00\x00' + '\x00\x01' + '\x00\x00' + toString16(14 + (nPairs * 6)) + '\x00\x01' + toString16(nPairs) + toString16(searchRange) + toString16(logE(nPairs) / logE(2)) + toString16((2 * nPairs) - searchRange);
          var i = 0;
          var record;
          while ((record = kerning[i++])) {
            kern += toString16(glyphIndex[record.code1]) + toString16(glyphIndex[record.code2]) + toString16(record.adjustment);
          }
          tables['kern'] = kern;
        }

        tables['loca'] = loca;

        tables['maxp'] = '\x00\x01\x00\x00' + toString16(glyphCount + 1) + toString16(maxPoints) + toString16(maxContours) + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00';

        var psName = fontName.replace(/ /g, '');
        var strings = [
          tag.copyright || 'Original licence',
          fontName,
          'Unknown',
          uniqueName,
          fontName,
          '1.0',
          psName,
          'Unknown',
          'Unknown',
          'Unknown'
        ];
        var count = strings.length;
        var name = '\x00\x00' + toString16(count) + toString16((count * 12) + 6);
        var offset = 0;
        var i = 0;
        var str;
        while ((str = strings[i++])) {
          name += '\x00\x01' + '\x00\x00' + '\x00\x00' + toString16(i - 1) + toString16(str.length) + toString16(offset);
          offset += str.length;
        }
        tables['name'] = name + strings.join('');

        tables['post'] = '\x00\x03\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00' + '\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00\x00\x00' + '\x00\x00\x00\x00';

        var names = Object.keys(tables);
        var numTables = names.length;
        var header = '\x00\x01\x00\x00' + toString16(numTables) + '\x00\x80' + '\x00\x03' + '\x00\x20';
        var dataString = '';
        var offset = (numTables * 16) + header.length;
        var i = 0;
        while ((name = names[i++])) {
          var table = tables[name];
          var length = table.length;
          header += name + '\x00\x00\x00\x00' + toString32(offset) + toString32(length);
          while (length & 3) {
            table += '\x00';
            ++length;
          }
          dataString += table;
          while (offset & 3) {
            ++offset;
          }
          offset += length;
        }
        var otf = header + dataString;
        var unitPerEm = 1024;
        var metrics = {
          ascent: ascent / unitPerEm,
          descent: -descent / unitPerEm,
          leading: leading / unitPerEm
        };

        var dataBuffer = new Uint8Array(otf.length);
        for (var i = 0; i < otf.length; i++) {
          dataBuffer[i] = otf.charCodeAt(i) & 0xff;
        }

        font.codes = originalCode;
        font.metrics = metrics;
        font.data = dataBuffer;

        return font;
      }
      Parser.defineFont = defineFont;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var assert = Shumway.Debug.assert;
      var Inflate = Shumway.ArrayUtilities.Inflate;

      function readUint16(bytes, position) {
        return (bytes[position] << 8) | bytes[position + 1];
      }

      function parseJpegChunks(image, bytes) {
        var i = 0;
        var n = bytes.length;
        var chunks = [];
        var code;
        do {
          var begin = i;
          while (i < n && bytes[i] !== 0xff) {
            ++i;
          }
          while (i < n && bytes[i] === 0xff) {
            ++i;
          }
          code = bytes[i++];
          if (code === 0xda) {
            i = n;
          } else if (code === 0xd9) {
            i += 2;
            continue;
          } else if (code < 0xd0 || code > 0xd8) {
            var length = readUint16(bytes, i);
            if (code >= 0xc0 && code <= 0xc3) {
              image.height = readUint16(bytes, i + 3);
              image.width = readUint16(bytes, i + 5);
            }
            i += length;
          }
          chunks.push(bytes.subarray(begin, i));
        } while(i < n);
        release || assert(image.width && image.height, 'bad jpeg image');
        return chunks;
      }
      Parser.parseJpegChunks = parseJpegChunks;

      function joinChunks(chunks) {
        var length = 0;
        for (var i = 0; i < chunks.length; i++) {
          length += chunks[i].length;
        }
        var bytes = new Uint8Array(length);
        var offset = 0;
        for (var i = 0; i < chunks.length; i++) {
          var chunk = chunks[i];
          bytes.set(chunk, offset);
          offset += chunk.length;
        }
        return bytes;
      }

      function defineImage(tag, dictionary) {
        SWF.enterTimeline("defineImage");
        var image = {
          type: 'image',
          id: tag.id,
          mimeType: tag.mimeType
        };
        var imgData = tag.imgData;
        var chunks;

        if (tag.mimeType === 'image/jpeg') {
          var alphaData = tag.alphaData;
          if (alphaData) {
            var jpegImage = new Shumway.JPEG.JpegImage();
            jpegImage.parse(joinChunks(parseJpegChunks(image, imgData)));
            release || assert(image.width === jpegImage.width);
            release || assert(image.height === jpegImage.height);
            var width = image.width;
            var height = image.height;
            var length = width * height;

            var alphaMaskBytes = Inflate.inflate(alphaData, length, true);

            var data = image.data = new Uint8ClampedArray(length * 4);
            jpegImage.copyToImageData(image);
            for (var i = 0, k = 3; i < length; i++, k += 4) {
              data[k] = alphaMaskBytes[i];
            }
            image.mimeType = 'application/octet-stream';
            image.dataType = 3 /* StraightAlphaRGBA */;
          } else {
            chunks = parseJpegChunks(image, imgData);

            if (tag.incomplete) {
              var tables = dictionary[0];
              release || assert(tables, 'missing jpeg tables');
              var header = tables.data;
              if (header && header.size) {
                chunks[0] = chunks[0].subarray(2);
                chunks.unshift(header.slice(0, header.size - 2));
              }
            }
            image.data = joinChunks(chunks);
          }
        } else {
          image.data = imgData;
        }
        SWF.leaveTimeline();
        return image;
      }
      Parser.defineImage = defineImage;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var assert = Shumway.Debug.assert;

      function defineLabel(tag, dictionary) {
        var records = tag.records;
        var bbox = tag.bbox;
        var htmlText = '';
        var coords = [];
        var dependencies = [];
        var size = 12;
        var face = 'Times Roman';
        var color = 0;
        var x = 0;
        var y = 0;
        var i = 0;
        var record;
        var codes;
        var font;
        var fontAttributes;
        while ((record = records[i++])) {
          if (record.eot) {
            break;
          }
          if (record.hasFont) {
            font = dictionary[record.fontId];
            release || assert(font, 'undefined label font');
            codes = font.codes;
            dependencies.push(font.id);
            size = record.fontHeight / 20;
            face = 'swffont' + font.id;
          }
          if (record.hasColor) {
            color = record.color >>> 8;
          }
          if (record.hasMoveX) {
            x = record.moveX;
            if (x < bbox.xMin) {
              bbox.xMin = x;
            }
          }
          if (record.hasMoveY) {
            y = record.moveY;
            if (y < bbox.yMin) {
              bbox.yMin = y;
            }
          }
          var text = '';
          var entries = record.entries;
          var j = 0;
          var entry;
          while ((entry = entries[j++])) {
            var code = codes[entry.glyphIndex];
            release || assert(code, 'undefined label glyph');
            text += String.fromCharCode(code);
            coords.push(x, y);
            x += entry.advance;
          }
          htmlText += '<font size="' + size + '" face="' + face + '"' + ' color="#' + ('000000' + color.toString(16)).slice(-6) + '">' + text.replace(/[<>&]/g, function (s) {
            return s === '<' ? '&lt;' : (s === '>' ? '&gt;' : '&amp;');
          }) + '</font>';
        }
        var label = {
          type: 'text',
          id: tag.id,
          fillBounds: bbox,
          matrix: tag.matrix,
          tag: {
            hasText: true,
            initialText: htmlText,
            html: true,
            readonly: true
          },
          coords: coords,
          static: true,
          require: null
        };
        if (dependencies.length) {
          label.require = dependencies;
        }
        return label;
      }
      Parser.defineLabel = defineLabel;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var PathCommand = Shumway.PathCommand;
      var GradientType = Shumway.GradientType;

      var Bounds = Shumway.Bounds;
      var DataBuffer = Shumway.ArrayUtilities.DataBuffer;
      var ShapeData = Shumway.ShapeData;
      var clamp = Shumway.NumberUtilities.clamp;
      var assert = Shumway.Debug.assert;
      var assertUnreachable = Shumway.Debug.assertUnreachable;
      var push = Array.prototype.push;

      var FillType;
      (function (FillType) {
        FillType[FillType["Solid"] = 0] = "Solid";
        FillType[FillType["LinearGradient"] = 0x10] = "LinearGradient";
        FillType[FillType["RadialGradient"] = 0x12] = "RadialGradient";
        FillType[FillType["FocalRadialGradient"] = 0x13] = "FocalRadialGradient";
        FillType[FillType["RepeatingBitmap"] = 0x40] = "RepeatingBitmap";
        FillType[FillType["ClippedBitmap"] = 0x41] = "ClippedBitmap";
        FillType[FillType["NonsmoothedRepeatingBitmap"] = 0x42] = "NonsmoothedRepeatingBitmap";
        FillType[FillType["NonsmoothedClippedBitmap"] = 0x43] = "NonsmoothedClippedBitmap";
      })(FillType || (FillType = {}));

      function applySegmentToStyles(segment, styles, linePaths, fillPaths) {
        if (!segment) {
          return;
        }
        var path;
        if (styles.fill0) {
          path = fillPaths[styles.fill0 - 1];

          if (!(styles.fill1 || styles.line)) {
            segment.isReversed = true;
            return;
          } else {
            path.addSegment(segment.toReversed());
          }
        }
        if (styles.line && styles.fill1) {
          path = linePaths[styles.line - 1];
          path.addSegment(segment.clone());
        }
      }

      function convertRecordsToShapeData(records, fillPaths, linePaths, dictionary, dependencies, recordsMorph) {
        var isMorph = recordsMorph !== null;
        var styles = { fill0: 0, fill1: 0, line: 0 };
        var segment = null;

        var allPaths;

        var defaultPath;

        var numRecords = records.length - 1;
        var x = 0;
        var y = 0;
        var morphX = 0;
        var morphY = 0;
        var path;
        for (var i = 0, j = 0; i < numRecords; i++) {
          var record = records[i];
          var morphRecord;
          if (isMorph) {
            morphRecord = recordsMorph[j++];
          }

          if (record.type === 0) {
            if (segment) {
              applySegmentToStyles(segment, styles, linePaths, fillPaths);
            }

            if (record.hasNewStyles) {
              if (!allPaths) {
                allPaths = [];
              }
              push.apply(allPaths, fillPaths);
              fillPaths = createPathsList(record.fillStyles, false, dictionary, dependencies);
              push.apply(allPaths, linePaths);
              linePaths = createPathsList(record.lineStyles, true, dictionary, dependencies);
              if (defaultPath) {
                allPaths.push(defaultPath);
                defaultPath = null;
              }
              styles = { fill0: 0, fill1: 0, line: 0 };
            }

            if (record.hasFillStyle0) {
              styles.fill0 = record.fillStyle0;
            }
            if (record.hasFillStyle1) {
              styles.fill1 = record.fillStyle1;
            }
            if (record.hasLineStyle) {
              styles.line = record.lineStyle;
            }
            if (styles.fill1) {
              path = fillPaths[styles.fill1 - 1];
            } else if (styles.line) {
              path = linePaths[styles.line - 1];
            } else if (styles.fill0) {
              path = fillPaths[styles.fill0 - 1];
            }

            if (record.move) {
              x = record.moveX | 0;
              y = record.moveY | 0;
            }

            if (path) {
              segment = PathSegment.FromDefaults(isMorph);
              path.addSegment(segment);

              if (!isMorph) {
                segment.moveTo(x, y);
              } else {
                if (morphRecord.type === 0) {
                  morphX = morphRecord.moveX | 0;
                  morphY = morphRecord.moveY | 0;
                } else {
                  morphX = x;
                  morphY = y;

                  j--;
                }
                segment.morphMoveTo(x, y, morphX, morphY);
              }
            }
          } else {
            release || assert(record.type === 1);
            if (!segment) {
              if (!defaultPath) {
                var style = { color: { red: 0, green: 0, blue: 0, alpha: 0 }, width: 20 };
                defaultPath = new SegmentedPath(null, processStyle(style, true, dictionary, dependencies));
              }
              segment = PathSegment.FromDefaults(isMorph);
              defaultPath.addSegment(segment);
              if (!isMorph) {
                segment.moveTo(x, y);
              } else {
                segment.morphMoveTo(x, y, morphX, morphY);
              }
            }
            if (isMorph) {
              while (morphRecord && morphRecord.type === 0) {
                morphRecord = recordsMorph[j++];
              }

              if (!morphRecord) {
                morphRecord = record;
              }
            }

            if (record.isStraight && (!isMorph || morphRecord.isStraight)) {
              x += record.deltaX | 0;
              y += record.deltaY | 0;
              if (!isMorph) {
                segment.lineTo(x, y);
              } else {
                morphX += morphRecord.deltaX | 0;
                morphY += morphRecord.deltaY | 0;
                segment.morphLineTo(x, y, morphX, morphY);
              }
            } else {
              var cx, cy;
              var deltaX, deltaY;
              if (!record.isStraight) {
                cx = x + record.controlDeltaX | 0;
                cy = y + record.controlDeltaY | 0;
                x = cx + record.anchorDeltaX | 0;
                y = cy + record.anchorDeltaY | 0;
              } else {
                deltaX = record.deltaX | 0;
                deltaY = record.deltaY | 0;
                cx = x + (deltaX >> 1);
                cy = y + (deltaY >> 1);
                x += deltaX;
                y += deltaY;
              }
              segment.curveTo(cx, cy, x, y);
              if (!isMorph) {
              } else {
                if (!morphRecord.isStraight) {
                  var morphCX = morphX + morphRecord.controlDeltaX | 0;
                  var morphCY = morphY + morphRecord.controlDeltaY | 0;
                  morphX = morphCX + morphRecord.anchorDeltaX | 0;
                  morphY = morphCY + morphRecord.anchorDeltaY | 0;
                } else {
                  deltaX = morphRecord.deltaX | 0;
                  deltaY = morphRecord.deltaY | 0;
                  var morphCX = morphX + (deltaX >> 1);
                  var morphCY = morphY + (deltaY >> 1);
                  morphX += deltaX;
                  morphY += deltaY;
                }
                segment.morphCurveTo(cx, cy, x, y, morphCX, morphCY, morphX, morphY);
              }
            }
          }
        }
        applySegmentToStyles(segment, styles, linePaths, fillPaths);

        if (allPaths) {
          push.apply(allPaths, fillPaths);
        } else {
          allPaths = fillPaths;
        }
        push.apply(allPaths, linePaths);
        if (defaultPath) {
          allPaths.push(defaultPath);
        }

        var shape = new ShapeData();
        if (isMorph) {
          shape.morphCoordinates = new Int32Array(shape.coordinates.length);
        }
        for (i = 0; i < allPaths.length; i++) {
          allPaths[i].serialize(shape);
        }
        return shape;
      }

      var IDENTITY_MATRIX = { a: 1, b: 0, c: 0, d: 1, tx: 0, ty: 0 };
      function processStyle(style, isLineStyle, dictionary, dependencies) {
        if (isLineStyle) {
          style.miterLimit = (style.miterLimitFactor || 1.5) * 2;
          if (!style.color && style.hasFill) {
            var fillStyle = processStyle(style.fillStyle, false, dictionary, dependencies);
            style.type = fillStyle.type;
            style.transform = fillStyle.transform;
            style.records = fillStyle.records;
            style.colors = fillStyle.colors;
            style.ratios = fillStyle.ratios;
            style.focalPoint = fillStyle.focalPoint;
            style.bitmapId = fillStyle.bitmapId;
            style.bitmapIndex = fillStyle.bitmapIndex;
            style.repeat = fillStyle.repeat;
            style.fillStyle = null;
            return style;
          } else {
            style.type = 0 /* Solid */;
          }
        }
        if (style.type === undefined || style.type === 0 /* Solid */) {
          return style;
        }
        var scale;
        switch (style.type) {
          case 16 /* LinearGradient */:
          case 18 /* RadialGradient */:
          case 19 /* FocalRadialGradient */:
            var records = style.records;
            var colors = style.colors = [];
            var ratios = style.ratios = [];
            for (var i = 0; i < records.length; i++) {
              var record = records[i];
              colors.push(record.color);
              ratios.push(record.ratio);
            }
            scale = 819.2;
            break;
          case 64 /* RepeatingBitmap */:
          case 65 /* ClippedBitmap */:
          case 66 /* NonsmoothedRepeatingBitmap */:
          case 67 /* NonsmoothedClippedBitmap */:
            style.smooth = style.type !== 66 /* NonsmoothedRepeatingBitmap */ && style.type !== 67 /* NonsmoothedClippedBitmap */;
            style.repeat = style.type !== 65 /* ClippedBitmap */ && style.type !== 67 /* NonsmoothedClippedBitmap */;
            if (dictionary[style.bitmapId]) {
              style.bitmapIndex = dependencies.length;
              dependencies.push(style.bitmapId);
              scale = 0.05;
            } else {
              style.bitmapIndex = -1;
            }
            break;
          default:
            release || assertUnreachable('shape parser encountered invalid fill style');
        }
        if (!style.matrix) {
          style.transform = IDENTITY_MATRIX;
          return style;
        }
        var matrix = style.matrix;
        style.transform = {
          a: (matrix.a * scale),
          b: (matrix.b * scale),
          c: (matrix.c * scale),
          d: (matrix.d * scale),
          tx: matrix.tx / 20,
          ty: matrix.ty / 20
        };

        style.matrix = null;
        return style;
      }

      function createPathsList(styles, isLineStyle, dictionary, dependencies) {
        var paths = [];
        for (var i = 0; i < styles.length; i++) {
          var style = processStyle(styles[i], isLineStyle, dictionary, dependencies);
          if (!isLineStyle) {
            paths[i] = new SegmentedPath(style, null);
          } else {
            paths[i] = new SegmentedPath(null, style);
          }
        }
        return paths;
      }

      function defineShape(tag, dictionary) {
        var dependencies = [];
        var fillPaths = createPathsList(tag.fillStyles, false, dictionary, dependencies);
        var linePaths = createPathsList(tag.lineStyles, true, dictionary, dependencies);
        var shape = convertRecordsToShapeData(tag.records, fillPaths, linePaths, dictionary, dependencies, tag.recordsMorph || null);

        if (tag.lineBoundsMorph) {
          var lineBounds = tag.lineBounds = Bounds.FromUntyped(tag.lineBounds);
          var lineBoundsMorph = tag.lineBoundsMorph;
          lineBounds.extendByPoint(lineBoundsMorph.xMin, lineBoundsMorph.yMin);
          lineBounds.extendByPoint(lineBoundsMorph.xMax, lineBoundsMorph.yMax);
          var fillBoundsMorph = tag.fillBoundsMorph;
          if (fillBoundsMorph) {
            var fillBounds = tag.fillBounds = tag.fillBounds ? Bounds.FromUntyped(tag.fillBounds) : null;
            fillBounds.extendByPoint(fillBoundsMorph.xMin, fillBoundsMorph.yMin);
            fillBounds.extendByPoint(fillBoundsMorph.xMax, fillBoundsMorph.yMax);
          }
        }
        return {
          type: tag.isMorph ? 'morphshape' : 'shape',
          id: tag.id,
          fillBounds: tag.fillBounds,
          lineBounds: tag.lineBounds,
          morphFillBounds: tag.fillBoundsMorph || null,
          morphLineBounds: tag.lineBoundsMorph || null,
          hasFills: fillPaths.length > 0,
          hasLines: linePaths.length > 0,
          shape: shape.toPlainObject(),
          transferables: shape.buffers,
          require: dependencies.length ? dependencies : null
        };
      }
      Parser.defineShape = defineShape;

      var PathSegment = (function () {
        function PathSegment(commands, data, morphData, prev, next, isReversed) {
          this.commands = commands;
          this.data = data;
          this.morphData = morphData;
          this.prev = prev;
          this.next = next;
          this.isReversed = isReversed;
          this.id = PathSegment._counter++;
        }
        PathSegment.FromDefaults = function (isMorph) {
          var commands = new DataBuffer();
          var data = new DataBuffer();
          commands.endian = data.endian = 'auto';
          var morphData = null;
          if (isMorph) {
            morphData = new DataBuffer();
            morphData.endian = 'auto';
          }
          return new PathSegment(commands, data, morphData, null, null, false);
        };

        PathSegment.prototype.moveTo = function (x, y) {
          this.commands.writeUnsignedByte(9 /* MoveTo */);
          this.data.writeInt(x);
          this.data.writeInt(y);
        };

        PathSegment.prototype.morphMoveTo = function (x, y, mx, my) {
          this.moveTo(x, y);
          this.morphData.writeInt(mx);
          this.morphData.writeInt(my);
        };

        PathSegment.prototype.lineTo = function (x, y) {
          this.commands.writeUnsignedByte(10 /* LineTo */);
          this.data.writeInt(x);
          this.data.writeInt(y);
        };

        PathSegment.prototype.morphLineTo = function (x, y, mx, my) {
          this.lineTo(x, y);
          this.morphData.writeInt(mx);
          this.morphData.writeInt(my);
        };

        PathSegment.prototype.curveTo = function (cpx, cpy, x, y) {
          this.commands.writeUnsignedByte(11 /* CurveTo */);
          this.data.writeInt(cpx);
          this.data.writeInt(cpy);
          this.data.writeInt(x);
          this.data.writeInt(y);
        };

        PathSegment.prototype.morphCurveTo = function (cpx, cpy, x, y, mcpx, mcpy, mx, my) {
          this.curveTo(cpx, cpy, x, y);
          this.morphData.writeInt(mcpx);
          this.morphData.writeInt(mcpy);
          this.morphData.writeInt(mx);
          this.morphData.writeInt(my);
        };

        PathSegment.prototype.toReversed = function () {
          release || assert(!this.isReversed);
          return new PathSegment(this.commands, this.data, this.morphData, null, null, true);
        };

        PathSegment.prototype.clone = function () {
          return new PathSegment(this.commands, this.data, this.morphData, null, null, this.isReversed);
        };

        PathSegment.prototype.storeStartAndEnd = function () {
          var data = this.data.ints;
          var endPoint1 = data[0] + ',' + data[1];
          var endPoint2Offset = (this.data.length >> 2) - 2;
          var endPoint2 = data[endPoint2Offset] + ',' + data[endPoint2Offset + 1];
          if (!this.isReversed) {
            this.startPoint = endPoint1;
            this.endPoint = endPoint2;
          } else {
            this.startPoint = endPoint2;
            this.endPoint = endPoint1;
          }
        };

        PathSegment.prototype.connectsTo = function (other) {
          release || assert(other !== this);
          release || assert(this.endPoint);
          release || assert(other.startPoint);
          return this.endPoint === other.startPoint;
        };

        PathSegment.prototype.serialize = function (shape, lastPosition) {
          if (this.isReversed) {
            this._serializeReversed(shape, lastPosition);
            return;
          }
          var commands = this.commands.bytes;

          var dataLength = this.data.length >> 2;
          var morphData = this.morphData ? this.morphData.ints : null;
          var data = this.data.ints;
          release || assert(commands[0] === 9 /* MoveTo */);

          var offset = 0;
          if (data[0] === lastPosition.x && data[1] === lastPosition.y) {
            offset++;
          }
          var commandsCount = this.commands.length;
          var dataPosition = offset * 2;
          for (var i = offset; i < commandsCount; i++) {
            dataPosition = this._writeCommand(commands[i], dataPosition, data, morphData, shape);
          }
          release || assert(dataPosition === dataLength);
          lastPosition.x = data[dataLength - 2];
          lastPosition.y = data[dataLength - 1];
        };
        PathSegment.prototype._serializeReversed = function (shape, lastPosition) {
          var commandsCount = this.commands.length;
          var dataPosition = (this.data.length >> 2) - 2;
          var commands = this.commands.bytes;
          release || assert(commands[0] === 9 /* MoveTo */);
          var data = this.data.ints;
          var morphData = this.morphData ? this.morphData.ints : null;

          if (data[dataPosition] !== lastPosition.x || data[dataPosition + 1] !== lastPosition.y) {
            this._writeCommand(9 /* MoveTo */, dataPosition, data, morphData, shape);
          }
          if (commandsCount === 1) {
            lastPosition.x = data[0];
            lastPosition.y = data[1];
            return;
          }
          for (var i = commandsCount; i-- > 1;) {
            dataPosition -= 2;
            var command = commands[i];
            shape.writeCommandAndCoordinates(command, data[dataPosition], data[dataPosition + 1]);
            if (morphData) {
              shape.writeMorphCoordinates(morphData[dataPosition], morphData[dataPosition + 1]);
            }
            if (command === 11 /* CurveTo */) {
              dataPosition -= 2;
              shape.writeCoordinates(data[dataPosition], data[dataPosition + 1]);
              if (morphData) {
                shape.writeMorphCoordinates(morphData[dataPosition], morphData[dataPosition + 1]);
              }
            } else {
            }
          }
          release || assert(dataPosition === 0);
          lastPosition.x = data[0];
          lastPosition.y = data[1];
        };
        PathSegment.prototype._writeCommand = function (command, position, data, morphData, shape) {
          shape.writeCommandAndCoordinates(command, data[position++], data[position++]);
          if (morphData) {
            shape.writeMorphCoordinates(morphData[position - 2], morphData[position - 1]);
          }
          if (command === 11 /* CurveTo */) {
            shape.writeCoordinates(data[position++], data[position++]);
            if (morphData) {
              shape.writeMorphCoordinates(morphData[position - 2], morphData[position - 1]);
            }
          }
          return position;
        };
        PathSegment._counter = 0;
        return PathSegment;
      })();

      var SegmentedPath = (function () {
        function SegmentedPath(fillStyle, lineStyle) {
          this.fillStyle = fillStyle;
          this.lineStyle = lineStyle;
          this._head = null;
        }
        SegmentedPath.prototype.addSegment = function (segment) {
          release || assert(segment);
          release || assert(segment.next === null);
          release || assert(segment.prev === null);
          var currentHead = this._head;
          if (currentHead) {
            release || assert(segment !== currentHead);
            currentHead.next = segment;
            segment.prev = currentHead;
          }
          this._head = segment;
        };

        SegmentedPath.prototype.removeSegment = function (segment) {
          if (segment.prev) {
            segment.prev.next = segment.next;
          }
          if (segment.next) {
            segment.next.prev = segment.prev;
          }
        };

        SegmentedPath.prototype.insertSegment = function (segment, next) {
          var prev = next.prev;
          segment.prev = prev;
          segment.next = next;
          if (prev) {
            prev.next = segment;
          }
          next.prev = segment;
        };

        SegmentedPath.prototype.head = function () {
          return this._head;
        };

        SegmentedPath.prototype.serialize = function (shape) {
          var segment = this.head();
          if (!segment) {
            return;
          }

          while (segment) {
            segment.storeStartAndEnd();
            segment = segment.prev;
          }

          var start = this.head();
          var end = start;

          var finalRoot = null;
          var finalHead = null;

          var current = start.prev;
          while (start) {
            while (current) {
              if (current.connectsTo(start)) {
                if (current.next !== start) {
                  this.removeSegment(current);
                  this.insertSegment(current, start);
                }
                start = current;
                current = start.prev;
                continue;
              }
              if (end.connectsTo(current)) {
                this.removeSegment(current);
                end.next = current;
                current = current.prev;
                end.next.prev = end;
                end.next.next = null;
                end = end.next;
                continue;
              }
              current = current.prev;
            }

            current = start.prev;
            if (!finalRoot) {
              finalRoot = start;
              finalHead = end;
            } else {
              finalHead.next = start;
              start.prev = finalHead;
              finalHead = end;
              finalHead.next = null;
            }
            if (!current) {
              break;
            }
            start = end = current;
            current = start.prev;
          }

          if (this.fillStyle) {
            var style = this.fillStyle;
            switch (style.type) {
              case 0 /* Solid */:
                shape.beginFill(style.color);
                break;
              case 16 /* LinearGradient */:
              case 18 /* RadialGradient */:
              case 19 /* FocalRadialGradient */:
                var gradientType = style.type === 16 /* LinearGradient */ ? 16 /* Linear */ : 18 /* Radial */;
                shape.beginGradient(2 /* BeginGradientFill */, style.colors, style.ratios, gradientType, style.transform, style.spreadMethod, style.interpolationMode, style.focalPoint | 0);
                break;
              case 65 /* ClippedBitmap */:
              case 64 /* RepeatingBitmap */:
              case 67 /* NonsmoothedClippedBitmap */:
              case 66 /* NonsmoothedRepeatingBitmap */:
                release || assert(style.bitmapIndex > -1);
                shape.beginBitmap(3 /* BeginBitmapFill */, style.bitmapIndex, style.transform, style.repeat, style.smooth);
                break;
              default:
                release || assertUnreachable('Invalid fill style type: ' + style.type);
            }
          } else {
            var style = this.lineStyle;
            release || assert(style);
            switch (style.type) {
              case 0 /* Solid */:
                writeLineStyle(style, shape);
                break;
              case 16 /* LinearGradient */:
              case 18 /* RadialGradient */:
              case 19 /* FocalRadialGradient */:
                var gradientType = style.type === 16 /* LinearGradient */ ? 16 /* Linear */ : 18 /* Radial */;
                writeLineStyle(style, shape);
                shape.beginGradient(6 /* LineStyleGradient */, style.colors, style.ratios, gradientType, style.transform, style.spreadMethod, style.interpolationMode, style.focalPoint | 0);
                break;
              case 65 /* ClippedBitmap */:
              case 64 /* RepeatingBitmap */:
              case 67 /* NonsmoothedClippedBitmap */:
              case 66 /* NonsmoothedRepeatingBitmap */:
                release || assert(style.bitmapIndex > -1);
                writeLineStyle(style, shape);
                shape.beginBitmap(7 /* LineStyleBitmap */, style.bitmapIndex, style.transform, style.repeat, style.smooth);
                break;
              default:
                console.error('Line style type not yet supported: ' + style.type);
            }
          }

          var lastPosition = { x: 0, y: 0 };
          current = finalRoot;
          while (current) {
            current.serialize(shape, lastPosition);
            current = current.next;
          }
          if (this.fillStyle) {
            shape.endFill();
          } else {
            shape.endLine();
          }
          return shape;
        };
        return SegmentedPath;
      })();

      function writeLineStyle(style, shape) {
        var scaleMode = style.noHscale ? (style.noVscale ? 0 : 2) : style.noVscale ? 3 : 1;

        var thickness = clamp(style.width, 0, 0xff * 20) | 0;
        shape.lineStyle(thickness, style.color, style.pixelHinting, scaleMode, style.endCapsStyle, style.jointStyle, style.miterLimit);
      }
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      var SOUND_SIZE_8_BIT = 0;
      var SOUND_SIZE_16_BIT = 1;
      var SOUND_TYPE_MONO = 0;
      var SOUND_TYPE_STEREO = 1;

      var SOUND_FORMAT_PCM_BE = 0;
      var SOUND_FORMAT_ADPCM = 1;
      var SOUND_FORMAT_MP3 = 2;
      var SOUND_FORMAT_PCM_LE = 3;
      var SOUND_FORMAT_NELLYMOSER_16 = 4;
      var SOUND_FORMAT_NELLYMOSER_8 = 5;
      var SOUND_FORMAT_NELLYMOSER = 6;
      var SOUND_FORMAT_SPEEX = 11;

      var SOUND_RATES = [5512, 11250, 22500, 44100];

      var WaveHeader = new Uint8Array([
        0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00,
        0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x02, 0x00, 0x44, 0xAC, 0x00, 0x00, 0x10, 0xB1, 0x02, 0x00,
        0x04, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x00, 0x00, 0x00, 0x00]);

      function packageWave(data, sampleRate, channels, size, swapBytes) {
        var sizeInBytes = size >> 3;
        var sizePerSecond = channels * sampleRate * sizeInBytes;
        var sizePerSample = channels * sizeInBytes;
        var dataLength = data.length + (data.length & 1);
        var buffer = new ArrayBuffer(WaveHeader.length + dataLength);
        var bytes = new Uint8Array(buffer);
        bytes.set(WaveHeader);
        if (swapBytes) {
          for (var i = 0, j = WaveHeader.length; i < data.length; i += 2, j += 2) {
            bytes[j] = data[i + 1];
            bytes[j + 1] = data[i];
          }
        } else {
          bytes.set(data, WaveHeader.length);
        }
        var view = new DataView(buffer);
        view.setUint32(4, dataLength + 36, true);
        view.setUint16(22, channels, true);
        view.setUint32(24, sampleRate, true);
        view.setUint32(28, sizePerSecond, true);
        view.setUint16(32, sizePerSample, true);
        view.setUint16(34, size, true);
        view.setUint32(40, dataLength, true);
        return {
          data: bytes,
          mimeType: 'audio/wav'
        };
      }

      function defineSound(tag, dictionary) {
        var channels = tag.soundType == SOUND_TYPE_STEREO ? 2 : 1;
        var samplesCount = tag.samplesCount;
        var sampleRate = SOUND_RATES[tag.soundRate];

        var data = tag.soundData;
        var pcm, packaged;
        switch (tag.soundFormat) {
          case SOUND_FORMAT_PCM_BE:
            pcm = new Float32Array(samplesCount * channels);
            if (tag.soundSize == SOUND_SIZE_16_BIT) {
              for (var i = 0, j = 0; i < pcm.length; i++, j += 2)
                pcm[i] = ((data[j] << 24) | (data[j + 1] << 16)) / 2147483648;
              packaged = packageWave(data, sampleRate, channels, 16, true);
            } else {
              for (var i = 0; i < pcm.length; i++)
                pcm[i] = (data[i] - 128) / 128;
              packaged = packageWave(data, sampleRate, channels, 8, false);
            }
            break;
          case SOUND_FORMAT_PCM_LE:
            pcm = new Float32Array(samplesCount * channels);
            if (tag.soundSize == SOUND_SIZE_16_BIT) {
              for (var i = 0, j = 0; i < pcm.length; i++, j += 2)
                pcm[i] = ((data[j + 1] << 24) | (data[j] << 16)) / 2147483648;
              packaged = packageWave(data, sampleRate, channels, 16, false);
            } else {
              for (var i = 0; i < pcm.length; i++)
                pcm[i] = (data[i] - 128) / 128;
              packaged = packageWave(data, sampleRate, channels, 8, false);
            }
            break;
          case SOUND_FORMAT_MP3:
            packaged = {
              data: new Uint8Array(data.subarray(2)),
              mimeType: 'audio/mpeg'
            };
            break;
          case SOUND_FORMAT_ADPCM:
            var pcm16 = new Int16Array(samplesCount * channels);
            decodeACPCMSoundData(data, pcm16, channels);
            pcm = new Float32Array(samplesCount * channels);
            for (var i = 0; i < pcm.length; i++)
              pcm[i] = pcm16[i] / 32768;
            packaged = packageWave(new Uint8Array(pcm16.buffer), sampleRate, channels, 16, !(new Uint8Array(new Uint16Array([1]).buffer))[0]);
            break;
          default:
            throw new Error('Unsupported audio format: ' + tag.soundFormat);
        }

        var sound = {
          type: 'sound',
          id: tag.id,
          sampleRate: sampleRate,
          channels: channels,
          pcm: pcm,
          packaged: undefined
        };
        if (packaged) {
          sound.packaged = packaged;
        }
        return sound;
      }
      Parser.defineSound = defineSound;

      var ACPCMIndexTables = [
        [-1, 2],
        [-1, -1, 2, 4],
        [-1, -1, -1, -1, 2, 4, 6, 8],
        [-1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16]
      ];

      var ACPCMStepSizeTable = [
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
        253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
        1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
        3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
        12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
      ];

      function decodeACPCMSoundData(data, pcm16, channels) {
        function readBits(n) {
          while (dataBufferLength < n) {
            dataBuffer = (dataBuffer << 8) | data[dataPosition++];
            dataBufferLength += 8;
          }
          dataBufferLength -= n;
          return (dataBuffer >>> dataBufferLength) & ((1 << n) - 1);
        }
        var dataPosition = 0;
        var dataBuffer = 0;
        var dataBufferLength = 0;

        var pcmPosition = 0;
        var codeSize = readBits(2);
        var indexTable = ACPCMIndexTables[codeSize];
        while (pcmPosition < pcm16.length) {
          var x = pcm16[pcmPosition++] = (readBits(16) << 16) >> 16, x2;
          var stepIndex = readBits(6), stepIndex2;
          if (channels > 1) {
            x2 = pcm16[pcmPosition++] = (readBits(16) << 16) >> 16;
            stepIndex2 = readBits(6);
          }
          var signMask = 1 << (codeSize + 1);
          for (var i = 0; i < 4095; i++) {
            var nibble = readBits(codeSize + 2);
            var step = ACPCMStepSizeTable[stepIndex];
            var sum = 0;
            for (var currentBit = signMask >> 1; currentBit; currentBit >>= 1, step >>= 1) {
              if (nibble & currentBit)
                sum += step;
            }
            x += (nibble & signMask ? -1 : 1) * (sum + step);
            pcm16[pcmPosition++] = (x = (x < -32768 ? -32768 : x > 32767 ? 32767 : x));
            stepIndex += indexTable[nibble & ~signMask];
            stepIndex = stepIndex < 0 ? 0 : stepIndex > 88 ? 88 : stepIndex;
            if (channels > 1) {
              nibble = readBits(codeSize + 2);
              step = ACPCMStepSizeTable[stepIndex2];
              sum = 0;
              for (var currentBit = signMask >> 1; currentBit; currentBit >>= 1, step >>= 1) {
                if (nibble & currentBit)
                  sum += step;
              }
              x2 += (nibble & signMask ? -1 : 1) * (sum + step);
              pcm16[pcmPosition++] = (x2 = (x2 < -32768 ? -32768 : x2 > 32767 ? 32767 : x2));
              stepIndex2 += indexTable[nibble & ~signMask];
              stepIndex2 = stepIndex2 < 0 ? 0 : stepIndex2 > 88 ? 88 : stepIndex2;
            }
          }
        }
      }

      var nextSoundStreamId = 0;

      var SwfSoundStream = (function () {
        function SwfSoundStream(samplesCount, sampleRate, channels) {
          this.streamId = (nextSoundStreamId++);
          this.samplesCount = samplesCount;
          this.sampleRate = sampleRate;
          this.channels = channels;
          this.format = null;
          this.currentSample = 0;
        }
        Object.defineProperty(SwfSoundStream.prototype, "info", {
          get: function () {
            return {
              samplesCount: this.samplesCount,
              sampleRate: this.sampleRate,
              channels: this.channels,
              format: this.format,
              streamId: this.streamId
            };
          },
          enumerable: true,
          configurable: true
        });
        return SwfSoundStream;
      })();
      Parser.SwfSoundStream = SwfSoundStream;

      function SwfSoundStream_decode_PCM(data) {
        var pcm = new Float32Array(data.length);
        for (var i = 0; i < pcm.length; i++)
          pcm[i] = (data[i] - 128) / 128;
        this.currentSample += pcm.length / this.channels;
        return {
          streamId: this.streamId,
          samplesCount: pcm.length / this.channels,
          pcm: pcm
        };
      }

      function SwfSoundStream_decode_PCM_be(data) {
        var pcm = new Float32Array(data.length / 2);
        for (var i = 0, j = 0; i < pcm.length; i++, j += 2)
          pcm[i] = ((data[j] << 24) | (data[j + 1] << 16)) / 2147483648;
        this.currentSample += pcm.length / this.channels;
        return {
          streamId: this.streamId,
          samplesCount: pcm.length / this.channels,
          pcm: pcm
        };
      }

      function SwfSoundStream_decode_PCM_le(data) {
        var pcm = new Float32Array(data.length / 2);
        for (var i = 0, j = 0; i < pcm.length; i++, j += 2)
          pcm[i] = ((data[j + 1] << 24) | (data[j] << 16)) / 2147483648;
        this.currentSample += pcm.length / this.channels;
        return {
          streamId: this.streamId,
          samplesCount: pcm.length / this.channels,
          pcm: pcm
        };
      }

      function SwfSoundStream_decode_MP3(data) {
        var samplesCount = (data[1] << 8) | data[0];
        var seek = (data[3] << 8) | data[2];
        this.currentSample += samplesCount;
        return {
          streamId: this.streamId,
          samplesCount: samplesCount,
          data: new Uint8Array(data.subarray(4)),
          seek: seek
        };
      }

      function createSoundStream(tag) {
        var channels = tag.streamType == SOUND_TYPE_STEREO ? 2 : 1;
        var samplesCount = tag.samplesCount;
        var sampleRate = SOUND_RATES[tag.streamRate];
        var stream = new SwfSoundStream(samplesCount, sampleRate, channels);

        switch (tag.streamCompression) {
          case SOUND_FORMAT_PCM_BE:
            stream.format = 'wave';
            if (tag.soundSize == SOUND_SIZE_16_BIT) {
              stream.decode = SwfSoundStream_decode_PCM_be;
            } else {
              stream.decode = SwfSoundStream_decode_PCM;
            }
            break;
          case SOUND_FORMAT_PCM_LE:
            stream.format = 'wave';
            if (tag.soundSize == SOUND_SIZE_16_BIT) {
              stream.decode = SwfSoundStream_decode_PCM_le;
            } else {
              stream.decode = SwfSoundStream_decode_PCM;
            }
            break;
          case SOUND_FORMAT_MP3:
            stream.format = 'mp3';
            stream.decode = SwfSoundStream_decode_MP3;
            break;
          default:
            throw new Error('Unsupported audio format: ' + tag.soundFormat);
        }

        return stream;
      }
      Parser.createSoundStream = createSoundStream;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    (function (Parser) {
      function defineText(tag, dictionary) {
        var dependencies = [];
        var bold = false;
        var italic = false;
        if (tag.hasFont) {
          var font = dictionary[tag.fontId];
          if (font) {
            dependencies.push(font.id);
            bold = font.bold;
            italic = font.italic;
          } else {
            Shumway.Debug.warning("Font is not defined.");
          }
        }

        var props = {
          type: 'text',
          id: tag.id,
          fillBounds: tag.bbox,
          variableName: tag.variableName,
          tag: tag,
          bold: bold,
          italic: italic,
          require: undefined
        };
        if (dependencies.length) {
          props.require = dependencies;
        }
        return props;
      }
      Parser.defineText = defineText;
    })(SWF.Parser || (SWF.Parser = {}));
    var Parser = SWF.Parser;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (JPEG) {
    var dctZigZag = new Int32Array([
      0,
      1, 8,
      16, 9, 2,
      3, 10, 17, 24,
      32, 25, 18, 11, 4,
      5, 12, 19, 26, 33, 40,
      48, 41, 34, 27, 20, 13, 6,
      7, 14, 21, 28, 35, 42, 49, 56,
      57, 50, 43, 36, 29, 22, 15,
      23, 30, 37, 44, 51, 58,
      59, 52, 45, 38, 31,
      39, 46, 53, 60,
      61, 54, 47,
      55, 62,
      63
    ]);

    var dctCos1 = 4017;
    var dctSin1 = 799;
    var dctCos3 = 3406;
    var dctSin3 = 2276;
    var dctCos6 = 1567;
    var dctSin6 = 3784;
    var dctSqrt2 = 5793;
    var dctSqrt1d2 = 2896;

    function constructor() {
    }

    function buildHuffmanTable(codeLengths, values) {
      var k = 0, code = [], i, j, length = 16;
      while (length > 0 && !codeLengths[length - 1]) {
        length--;
      }
      code.push({ children: [], index: 0 });
      var p = code[0], q;
      for (i = 0; i < length; i++) {
        for (j = 0; j < codeLengths[i]; j++) {
          p = code.pop();
          p.children[p.index] = values[k];
          while (p.index > 0) {
            p = code.pop();
          }
          p.index++;
          code.push(p);
          while (code.length <= i) {
            code.push(q = { children: [], index: 0 });
            p.children[p.index] = q.children;
            p = q;
          }
          k++;
        }
        if (i + 1 < length) {
          code.push(q = { children: [], index: 0 });
          p.children[p.index] = q.children;
          p = q;
        }
      }
      return code[0].children;
    }

    function getBlockBufferOffset(component, row, col) {
      return 64 * ((component.blocksPerLine + 1) * row + col);
    }

    function decodeScan(data, offset, frame, components, resetInterval, spectralStart, spectralEnd, successivePrev, successive) {
      var precision = frame.precision;
      var samplesPerLine = frame.samplesPerLine;
      var scanLines = frame.scanLines;
      var mcusPerLine = frame.mcusPerLine;
      var progressive = frame.progressive;
      var maxH = frame.maxH, maxV = frame.maxV;

      var startOffset = offset, bitsData = 0, bitsCount = 0;

      function readBit() {
        if (bitsCount > 0) {
          bitsCount--;
          return (bitsData >> bitsCount) & 1;
        }
        bitsData = data[offset++];
        if (bitsData == 0xFF) {
          var nextByte = data[offset++];
          if (nextByte) {
            throw 'unexpected marker: ' + ((bitsData << 8) | nextByte).toString(16);
          }
        }
        bitsCount = 7;
        return bitsData >>> 7;
      }

      function decodeHuffman(tree) {
        var node = tree;
        var bit;
        while ((bit = readBit()) !== null) {
          node = node[bit];
          if (typeof node === 'number') {
            return node;
          }
          if (typeof node !== 'object') {
            throw 'invalid huffman sequence';
          }
        }
        return null;
      }

      function receive(length) {
        var n = 0;
        while (length > 0) {
          var bit = readBit();
          if (bit === null) {
            return;
          }
          n = (n << 1) | bit;
          length--;
        }
        return n;
      }

      function receiveAndExtend(length) {
        if (length === 1) {
          return readBit() === 1 ? 1 : -1;
        }
        var n = receive(length);
        if (n >= 1 << (length - 1)) {
          return n;
        }
        return n + (-1 << length) + 1;
      }

      function decodeBaseline(component, offset) {
        var t = decodeHuffman(component.huffmanTableDC);
        var diff = t === 0 ? 0 : receiveAndExtend(t);
        component.blockData[offset] = (component.pred += diff);
        var k = 1;
        while (k < 64) {
          var rs = decodeHuffman(component.huffmanTableAC);
          var s = rs & 15, r = rs >> 4;
          if (s === 0) {
            if (r < 15) {
              break;
            }
            k += 16;
            continue;
          }
          k += r;
          var z = dctZigZag[k];
          component.blockData[offset + z] = receiveAndExtend(s);
          k++;
        }
      }

      function decodeDCFirst(component, offset) {
        var t = decodeHuffman(component.huffmanTableDC);
        var diff = t === 0 ? 0 : (receiveAndExtend(t) << successive);
        component.blockData[offset] = (component.pred += diff);
      }

      function decodeDCSuccessive(component, offset) {
        component.blockData[offset] |= readBit() << successive;
      }

      var eobrun = 0;
      function decodeACFirst(component, offset) {
        if (eobrun > 0) {
          eobrun--;
          return;
        }
        var k = spectralStart, e = spectralEnd;
        while (k <= e) {
          var rs = decodeHuffman(component.huffmanTableAC);
          var s = rs & 15, r = rs >> 4;
          if (s === 0) {
            if (r < 15) {
              eobrun = receive(r) + (1 << r) - 1;
              break;
            }
            k += 16;
            continue;
          }
          k += r;
          var z = dctZigZag[k];
          component.blockData[offset + z] = receiveAndExtend(s) * (1 << successive);
          k++;
        }
      }

      var successiveACState = 0, successiveACNextValue;
      function decodeACSuccessive(component, offset) {
        var k = spectralStart;
        var e = spectralEnd;
        var r = 0;
        var s;
        var rs;
        while (k <= e) {
          var z = dctZigZag[k];
          switch (successiveACState) {
            case 0:
              rs = decodeHuffman(component.huffmanTableAC);
              s = rs & 15;
              r = rs >> 4;
              if (s === 0) {
                if (r < 15) {
                  eobrun = receive(r) + (1 << r);
                  successiveACState = 4;
                } else {
                  r = 16;
                  successiveACState = 1;
                }
              } else {
                if (s !== 1) {
                  throw 'invalid ACn encoding';
                }
                successiveACNextValue = receiveAndExtend(s);
                successiveACState = r ? 2 : 3;
              }
              continue;
            case 1:
            case 2:
              if (component.blockData[offset + z]) {
                component.blockData[offset + z] += (readBit() << successive);
              } else {
                r--;
                if (r === 0) {
                  successiveACState = successiveACState == 2 ? 3 : 0;
                }
              }
              break;
            case 3:
              if (component.blockData[offset + z]) {
                component.blockData[offset + z] += (readBit() << successive);
              } else {
                component.blockData[offset + z] = successiveACNextValue << successive;
                successiveACState = 0;
              }
              break;
            case 4:
              if (component.blockData[offset + z]) {
                component.blockData[offset + z] += (readBit() << successive);
              }
              break;
          }
          k++;
        }
        if (successiveACState === 4) {
          eobrun--;
          if (eobrun === 0) {
            successiveACState = 0;
          }
        }
      }

      function decodeMcu(component, decode, mcu, row, col) {
        var mcuRow = (mcu / mcusPerLine) | 0;
        var mcuCol = mcu % mcusPerLine;
        var blockRow = mcuRow * component.v + row;
        var blockCol = mcuCol * component.h + col;
        var offset = getBlockBufferOffset(component, blockRow, blockCol);
        decode(component, offset);
      }

      function decodeBlock(component, decode, mcu) {
        var blockRow = (mcu / component.blocksPerLine) | 0;
        var blockCol = mcu % component.blocksPerLine;
        var offset = getBlockBufferOffset(component, blockRow, blockCol);
        decode(component, offset);
      }

      var componentsLength = components.length;
      var component, i, j, k, n;
      var decodeFn;
      if (progressive) {
        if (spectralStart === 0) {
          decodeFn = successivePrev === 0 ? decodeDCFirst : decodeDCSuccessive;
        } else {
          decodeFn = successivePrev === 0 ? decodeACFirst : decodeACSuccessive;
        }
      } else {
        decodeFn = decodeBaseline;
      }

      var mcu = 0, marker;
      var mcuExpected;
      if (componentsLength == 1) {
        mcuExpected = components[0].blocksPerLine * components[0].blocksPerColumn;
      } else {
        mcuExpected = mcusPerLine * frame.mcusPerColumn;
      }
      if (!resetInterval) {
        resetInterval = mcuExpected;
      }

      var h, v;
      while (mcu < mcuExpected) {
        for (i = 0; i < componentsLength; i++) {
          components[i].pred = 0;
        }
        eobrun = 0;

        if (componentsLength == 1) {
          component = components[0];
          for (n = 0; n < resetInterval; n++) {
            decodeBlock(component, decodeFn, mcu);
            mcu++;
          }
        } else {
          for (n = 0; n < resetInterval; n++) {
            for (i = 0; i < componentsLength; i++) {
              component = components[i];
              h = component.h;
              v = component.v;
              for (j = 0; j < v; j++) {
                for (k = 0; k < h; k++) {
                  decodeMcu(component, decodeFn, mcu, j, k);
                }
              }
            }
            mcu++;
          }
        }

        bitsCount = 0;
        marker = (data[offset] << 8) | data[offset + 1];
        if (marker <= 0xFF00) {
          throw 'marker was not found';
        }

        if (marker >= 0xFFD0 && marker <= 0xFFD7) {
          offset += 2;
        } else {
          break;
        }
      }

      return offset - startOffset;
    }

    function quantizeAndInverse(component, blockBufferOffset, p) {
      var qt = component.quantizationTable;
      var v0, v1, v2, v3, v4, v5, v6, v7, t;
      var i;

      for (i = 0; i < 64; i++) {
        p[i] = component.blockData[blockBufferOffset + i] * qt[i];
      }

      for (i = 0; i < 8; ++i) {
        var row = 8 * i;

        if (p[1 + row] === 0 && p[2 + row] === 0 && p[3 + row] === 0 && p[4 + row] === 0 && p[5 + row] === 0 && p[6 + row] === 0 && p[7 + row] === 0) {
          t = (dctSqrt2 * p[0 + row] + 512) >> 10;
          p[0 + row] = t;
          p[1 + row] = t;
          p[2 + row] = t;
          p[3 + row] = t;
          p[4 + row] = t;
          p[5 + row] = t;
          p[6 + row] = t;
          p[7 + row] = t;
          continue;
        }

        v0 = (dctSqrt2 * p[0 + row] + 128) >> 8;
        v1 = (dctSqrt2 * p[4 + row] + 128) >> 8;
        v2 = p[2 + row];
        v3 = p[6 + row];
        v4 = (dctSqrt1d2 * (p[1 + row] - p[7 + row]) + 128) >> 8;
        v7 = (dctSqrt1d2 * (p[1 + row] + p[7 + row]) + 128) >> 8;
        v5 = p[3 + row] << 4;
        v6 = p[5 + row] << 4;

        t = (v0 - v1 + 1) >> 1;
        v0 = (v0 + v1 + 1) >> 1;
        v1 = t;
        t = (v2 * dctSin6 + v3 * dctCos6 + 128) >> 8;
        v2 = (v2 * dctCos6 - v3 * dctSin6 + 128) >> 8;
        v3 = t;
        t = (v4 - v6 + 1) >> 1;
        v4 = (v4 + v6 + 1) >> 1;
        v6 = t;
        t = (v7 + v5 + 1) >> 1;
        v5 = (v7 - v5 + 1) >> 1;
        v7 = t;

        t = (v0 - v3 + 1) >> 1;
        v0 = (v0 + v3 + 1) >> 1;
        v3 = t;
        t = (v1 - v2 + 1) >> 1;
        v1 = (v1 + v2 + 1) >> 1;
        v2 = t;
        t = (v4 * dctSin3 + v7 * dctCos3 + 2048) >> 12;
        v4 = (v4 * dctCos3 - v7 * dctSin3 + 2048) >> 12;
        v7 = t;
        t = (v5 * dctSin1 + v6 * dctCos1 + 2048) >> 12;
        v5 = (v5 * dctCos1 - v6 * dctSin1 + 2048) >> 12;
        v6 = t;

        p[0 + row] = v0 + v7;
        p[7 + row] = v0 - v7;
        p[1 + row] = v1 + v6;
        p[6 + row] = v1 - v6;
        p[2 + row] = v2 + v5;
        p[5 + row] = v2 - v5;
        p[3 + row] = v3 + v4;
        p[4 + row] = v3 - v4;
      }

      for (i = 0; i < 8; ++i) {
        var col = i;

        if (p[1 * 8 + col] === 0 && p[2 * 8 + col] === 0 && p[3 * 8 + col] === 0 && p[4 * 8 + col] === 0 && p[5 * 8 + col] === 0 && p[6 * 8 + col] === 0 && p[7 * 8 + col] === 0) {
          t = (dctSqrt2 * p[i + 0] + 8192) >> 14;
          p[0 * 8 + col] = t;
          p[1 * 8 + col] = t;
          p[2 * 8 + col] = t;
          p[3 * 8 + col] = t;
          p[4 * 8 + col] = t;
          p[5 * 8 + col] = t;
          p[6 * 8 + col] = t;
          p[7 * 8 + col] = t;
          continue;
        }

        v0 = (dctSqrt2 * p[0 * 8 + col] + 2048) >> 12;
        v1 = (dctSqrt2 * p[4 * 8 + col] + 2048) >> 12;
        v2 = p[2 * 8 + col];
        v3 = p[6 * 8 + col];
        v4 = (dctSqrt1d2 * (p[1 * 8 + col] - p[7 * 8 + col]) + 2048) >> 12;
        v7 = (dctSqrt1d2 * (p[1 * 8 + col] + p[7 * 8 + col]) + 2048) >> 12;
        v5 = p[3 * 8 + col];
        v6 = p[5 * 8 + col];

        t = (v0 - v1 + 1) >> 1;
        v0 = (v0 + v1 + 1) >> 1;
        v1 = t;
        t = (v2 * dctSin6 + v3 * dctCos6 + 2048) >> 12;
        v2 = (v2 * dctCos6 - v3 * dctSin6 + 2048) >> 12;
        v3 = t;
        t = (v4 - v6 + 1) >> 1;
        v4 = (v4 + v6 + 1) >> 1;
        v6 = t;
        t = (v7 + v5 + 1) >> 1;
        v5 = (v7 - v5 + 1) >> 1;
        v7 = t;

        t = (v0 - v3 + 1) >> 1;
        v0 = (v0 + v3 + 1) >> 1;
        v3 = t;
        t = (v1 - v2 + 1) >> 1;
        v1 = (v1 + v2 + 1) >> 1;
        v2 = t;
        t = (v4 * dctSin3 + v7 * dctCos3 + 2048) >> 12;
        v4 = (v4 * dctCos3 - v7 * dctSin3 + 2048) >> 12;
        v7 = t;
        t = (v5 * dctSin1 + v6 * dctCos1 + 2048) >> 12;
        v5 = (v5 * dctCos1 - v6 * dctSin1 + 2048) >> 12;
        v6 = t;

        p[0 * 8 + col] = v0 + v7;
        p[7 * 8 + col] = v0 - v7;
        p[1 * 8 + col] = v1 + v6;
        p[6 * 8 + col] = v1 - v6;
        p[2 * 8 + col] = v2 + v5;
        p[5 * 8 + col] = v2 - v5;
        p[3 * 8 + col] = v3 + v4;
        p[4 * 8 + col] = v3 - v4;
      }

      for (i = 0; i < 64; ++i) {
        var index = blockBufferOffset + i;
        var q = p[i];
        q = (q <= -2056) ? 0 : (q >= 2024) ? 255 : (q + 2056) >> 4;
        component.blockData[index] = q;
      }
    }

    function buildComponentData(frame, component) {
      var lines = [];
      var blocksPerLine = component.blocksPerLine;
      var blocksPerColumn = component.blocksPerColumn;
      var samplesPerLine = blocksPerLine << 3;
      var computationBuffer = new Int32Array(64);

      var i, j, ll = 0;
      for (var blockRow = 0; blockRow < blocksPerColumn; blockRow++) {
        for (var blockCol = 0; blockCol < blocksPerLine; blockCol++) {
          var offset = getBlockBufferOffset(component, blockRow, blockCol);
          quantizeAndInverse(component, offset, computationBuffer);
        }
      }
      return component.blockData;
    }

    function clamp0to255(a) {
      return a <= 0 ? 0 : a >= 255 ? 255 : a;
    }

    var JpegImage = (function () {
      function JpegImage() {
      }
      JpegImage.prototype.parse = function (data) {
        function readUint16() {
          var value = (data[offset] << 8) | data[offset + 1];
          offset += 2;
          return value;
        }

        function readDataBlock() {
          var length = readUint16();
          var array = data.subarray(offset, offset + length - 2);
          offset += array.length;
          return array;
        }

        function prepareComponents(frame) {
          var mcusPerLine = Math.ceil(frame.samplesPerLine / 8 / frame.maxH);
          var mcusPerColumn = Math.ceil(frame.scanLines / 8 / frame.maxV);
          for (var i = 0; i < frame.components.length; i++) {
            component = frame.components[i];
            var blocksPerLine = Math.ceil(Math.ceil(frame.samplesPerLine / 8) * component.h / frame.maxH);
            var blocksPerColumn = Math.ceil(Math.ceil(frame.scanLines / 8) * component.v / frame.maxV);
            var blocksPerLineForMcu = mcusPerLine * component.h;
            var blocksPerColumnForMcu = mcusPerColumn * component.v;

            var blocksBufferSize = 64 * blocksPerColumnForMcu * (blocksPerLineForMcu + 1);
            component.blockData = new Int16Array(blocksBufferSize);
            component.blocksPerLine = blocksPerLine;
            component.blocksPerColumn = blocksPerColumn;
          }
          frame.mcusPerLine = mcusPerLine;
          frame.mcusPerColumn = mcusPerColumn;
        }

        var offset = 0, length = data.length;
        var jfif = null;
        var adobe = null;
        var pixels = null;
        var frame, resetInterval;
        var quantizationTables = [];
        var huffmanTablesAC = [], huffmanTablesDC = [];
        var fileMarker = readUint16();
        if (fileMarker != 0xFFD8) {
          throw 'SOI not found';
        }

        fileMarker = readUint16();
        while (fileMarker != 0xFFD9) {
          var i, j, l;
          switch (fileMarker) {
            case 0xFFE0:
            case 0xFFE1:
            case 0xFFE2:
            case 0xFFE3:
            case 0xFFE4:
            case 0xFFE5:
            case 0xFFE6:
            case 0xFFE7:
            case 0xFFE8:
            case 0xFFE9:
            case 0xFFEA:
            case 0xFFEB:
            case 0xFFEC:
            case 0xFFED:
            case 0xFFEE:
            case 0xFFEF:
            case 0xFFFE:
              var appData = readDataBlock();

              if (fileMarker === 0xFFE0) {
                if (appData[0] === 0x4A && appData[1] === 0x46 && appData[2] === 0x49 && appData[3] === 0x46 && appData[4] === 0) {
                  jfif = {
                    version: { major: appData[5], minor: appData[6] },
                    densityUnits: appData[7],
                    xDensity: (appData[8] << 8) | appData[9],
                    yDensity: (appData[10] << 8) | appData[11],
                    thumbWidth: appData[12],
                    thumbHeight: appData[13],
                    thumbData: appData.subarray(14, 14 + 3 * appData[12] * appData[13])
                  };
                }
              }

              if (fileMarker === 0xFFEE) {
                if (appData[0] === 0x41 && appData[1] === 0x64 && appData[2] === 0x6F && appData[3] === 0x62 && appData[4] === 0x65 && appData[5] === 0) {
                  adobe = {
                    version: appData[6],
                    flags0: (appData[7] << 8) | appData[8],
                    flags1: (appData[9] << 8) | appData[10],
                    transformCode: appData[11]
                  };
                }
              }
              break;

            case 0xFFDB:
              var quantizationTablesLength = readUint16();
              var quantizationTablesEnd = quantizationTablesLength + offset - 2;
              var z;
              while (offset < quantizationTablesEnd) {
                var quantizationTableSpec = data[offset++];
                var tableData = new Int32Array(64);
                if ((quantizationTableSpec >> 4) === 0) {
                  for (j = 0; j < 64; j++) {
                    z = dctZigZag[j];
                    tableData[z] = data[offset++];
                  }
                } else if ((quantizationTableSpec >> 4) === 1) {
                  for (j = 0; j < 64; j++) {
                    z = dctZigZag[j];
                    tableData[z] = readUint16();
                  }
                } else {
                  throw 'DQT: invalid table spec';
                }
                quantizationTables[quantizationTableSpec & 15] = tableData;
              }
              break;

            case 0xFFC0:
            case 0xFFC1:
            case 0xFFC2:
              if (frame) {
                throw 'Only single frame JPEGs supported';
              }
              readUint16();
              frame = {};
              frame.extended = (fileMarker === 0xFFC1);
              frame.progressive = (fileMarker === 0xFFC2);
              frame.precision = data[offset++];
              frame.scanLines = readUint16();
              frame.samplesPerLine = readUint16();
              frame.components = [];
              frame.componentIds = {};
              var componentsCount = data[offset++], componentId;
              var maxH = 0, maxV = 0;
              for (i = 0; i < componentsCount; i++) {
                componentId = data[offset];
                var h = data[offset + 1] >> 4;
                var v = data[offset + 1] & 15;
                if (maxH < h) {
                  maxH = h;
                }
                if (maxV < v) {
                  maxV = v;
                }
                var qId = data[offset + 2];
                l = frame.components.push({
                  h: h,
                  v: v,
                  quantizationTable: quantizationTables[qId]
                });
                frame.componentIds[componentId] = l - 1;
                offset += 3;
              }
              frame.maxH = maxH;
              frame.maxV = maxV;
              prepareComponents(frame);
              break;

            case 0xFFC4:
              var huffmanLength = readUint16();
              for (i = 2; i < huffmanLength;) {
                var huffmanTableSpec = data[offset++];
                var codeLengths = new Uint8Array(16);
                var codeLengthSum = 0;
                for (j = 0; j < 16; j++, offset++) {
                  codeLengthSum += (codeLengths[j] = data[offset]);
                }
                var huffmanValues = new Uint8Array(codeLengthSum);
                for (j = 0; j < codeLengthSum; j++, offset++) {
                  huffmanValues[j] = data[offset];
                }
                i += 17 + codeLengthSum;

                ((huffmanTableSpec >> 4) === 0 ? huffmanTablesDC : huffmanTablesAC)[huffmanTableSpec & 15] = buildHuffmanTable(codeLengths, huffmanValues);
              }
              break;

            case 0xFFDD:
              readUint16();
              resetInterval = readUint16();
              break;

            case 0xFFDA:
              var scanLength = readUint16();
              var selectorsCount = data[offset++];
              var components = [], component;
              for (i = 0; i < selectorsCount; i++) {
                var componentIndex = frame.componentIds[data[offset++]];
                component = frame.components[componentIndex];
                var tableSpec = data[offset++];
                component.huffmanTableDC = huffmanTablesDC[tableSpec >> 4];
                component.huffmanTableAC = huffmanTablesAC[tableSpec & 15];
                components.push(component);
              }
              var spectralStart = data[offset++];
              var spectralEnd = data[offset++];
              var successiveApproximation = data[offset++];
              var processed = decodeScan(data, offset, frame, components, resetInterval, spectralStart, spectralEnd, successiveApproximation >> 4, successiveApproximation & 15);
              offset += processed;
              break;
            default:
              if (data[offset - 3] == 0xFF && data[offset - 2] >= 0xC0 && data[offset - 2] <= 0xFE) {
                offset -= 3;
                break;
              }
              throw 'unknown JPEG marker ' + fileMarker.toString(16);
          }
          fileMarker = readUint16();
        }

        this.width = frame.samplesPerLine;
        this.height = frame.scanLines;
        this.jfif = jfif;
        this.adobe = adobe;
        this.components = [];
        for (i = 0; i < frame.components.length; i++) {
          component = frame.components[i];
          this.components.push({
            output: buildComponentData(frame, component),
            scaleX: component.h / frame.maxH,
            scaleY: component.v / frame.maxV,
            blocksPerLine: component.blocksPerLine,
            blocksPerColumn: component.blocksPerColumn
          });
        }
        this.numComponents = this.components.length;
      };

      JpegImage.prototype._getLinearizedBlockData = function (width, height) {
        var scaleX = this.width / width, scaleY = this.height / height;

        var component, componentScaleX, componentScaleY, blocksPerScanline;
        var x, y, i, j, k;
        var index;
        var offset = 0;
        var output;
        var numComponents = this.components.length;
        var dataLength = width * height * numComponents;
        var data = new Uint8Array(dataLength);
        var xScaleBlockOffset = new Uint32Array(width);
        var mask3LSB = 0xfffffff8;

        for (i = 0; i < numComponents; i++) {
          component = this.components[i];
          componentScaleX = component.scaleX * scaleX;
          componentScaleY = component.scaleY * scaleY;
          offset = i;
          output = component.output;
          blocksPerScanline = (component.blocksPerLine + 1) << 3;

          for (x = 0; x < width; x++) {
            j = 0 | (x * componentScaleX);
            xScaleBlockOffset[x] = ((j & mask3LSB) << 3) | (j & 7);
          }

          for (y = 0; y < height; y++) {
            j = 0 | (y * componentScaleY);
            index = blocksPerScanline * (j & mask3LSB) | ((j & 7) << 3);
            for (x = 0; x < width; x++) {
              data[offset] = output[index + xScaleBlockOffset[x]];
              offset += numComponents;
            }
          }
        }

        var transform = this.decodeTransform;
        if (transform) {
          for (i = 0; i < dataLength;) {
            for (j = 0, k = 0; j < numComponents; j++, i++, k += 2) {
              data[i] = ((data[i] * transform[k]) >> 8) + transform[k + 1];
            }
          }
        }
        return data;
      };

      JpegImage.prototype._isColorConversionNeeded = function () {
        if (this.adobe && this.adobe.transformCode) {
          return true;
        } else if (this.numComponents == 3) {
          return true;
        } else {
          return false;
        }
      };

      JpegImage.prototype._convertYccToRgb = function (data) {
        var Y, Cb, Cr;
        for (var i = 0, length = data.length; i < length; i += 3) {
          Y = data[i];
          Cb = data[i + 1];
          Cr = data[i + 2];
          data[i] = clamp0to255(Y - 179.456 + 1.402 * Cr);
          data[i + 1] = clamp0to255(Y + 135.459 - 0.344 * Cb - 0.714 * Cr);
          data[i + 2] = clamp0to255(Y - 226.816 + 1.772 * Cb);
        }
        return data;
      };

      JpegImage.prototype._convertYcckToRgb = function (data) {
        var Y, Cb, Cr, k, CbCb, CbCr, CbY, Cbk, CrCr, Crk, CrY, YY, Yk, kk;
        var offset = 0;
        for (var i = 0, length = data.length; i < length; i += 4) {
          Y = data[i];
          Cb = data[i + 1];
          Cr = data[i + 2];
          k = data[i + 3];

          CbCb = Cb * Cb;
          CbCr = Cb * Cr;
          CbY = Cb * Y;
          Cbk = Cb * k;
          CrCr = Cr * Cr;
          Crk = Cr * k;
          CrY = Cr * Y;
          YY = Y * Y;
          Yk = Y * k;
          kk = k * k;

          var r = -122.67195406894 - 6.60635669420364e-5 * CbCb + 0.000437130475926232 * CbCr - 5.4080610064599e-5 * CbY + 0.00048449797120281 * Cbk - 0.154362151871126 * Cb - 0.000957964378445773 * CrCr + 0.000817076911346625 * CrY - 0.00477271405408747 * Crk + 1.53380253221734 * Cr + 0.000961250184130688 * YY - 0.00266257332283933 * Yk + 0.48357088451265 * Y - 0.000336197177618394 * kk + 0.484791561490776 * k;

          var g = 107.268039397724 + 2.19927104525741e-5 * CbCb - 0.000640992018297945 * CbCr + 0.000659397001245577 * CbY + 0.000426105652938837 * Cbk - 0.176491792462875 * Cb - 0.000778269941513683 * CrCr + 0.00130872261408275 * CrY + 0.000770482631801132 * Crk - 0.151051492775562 * Cr + 0.00126935368114843 * YY - 0.00265090189010898 * Yk + 0.25802910206845 * Y - 0.000318913117588328 * kk - 0.213742400323665 * k;

          var b = -20.810012546947 - 0.000570115196973677 * CbCb - 2.63409051004589e-5 * CbCr + 0.0020741088115012 * CbY - 0.00288260236853442 * Cbk + 0.814272968359295 * Cb - 1.53496057440975e-5 * CrCr - 0.000132689043961446 * CrY + 0.000560833691242812 * Crk - 0.195152027534049 * Cr + 0.00174418132927582 * YY - 0.00255243321439347 * Yk + 0.116935020465145 * Y - 0.000343531996510555 * kk + 0.24165260232407 * k;

          data[offset++] = clamp0to255(r);
          data[offset++] = clamp0to255(g);
          data[offset++] = clamp0to255(b);
        }
        return data;
      };

      JpegImage.prototype._convertYcckToCmyk = function (data) {
        var Y, Cb, Cr;
        for (var i = 0, length = data.length; i < length; i += 4) {
          Y = data[i];
          Cb = data[i + 1];
          Cr = data[i + 2];
          data[i] = clamp0to255(434.456 - Y - 1.402 * Cr);
          data[i + 1] = clamp0to255(119.541 - Y + 0.344 * Cb + 0.714 * Cr);
          data[i + 2] = clamp0to255(481.816 - Y - 1.772 * Cb);
        }
        return data;
      };

      JpegImage.prototype._convertCmykToRgb = function (data) {
        var c, m, y, k;
        var offset = 0;
        var min = -255 * 255 * 255;
        var scale = 1 / 255 / 255;
        for (var i = 0, length = data.length; i < length; i += 4) {
          c = data[i];
          m = data[i + 1];
          y = data[i + 2];
          k = data[i + 3];

          var r = c * (-4.387332384609988 * c + 54.48615194189176 * m + 18.82290502165302 * y + 212.25662451639585 * k - 72734.4411664936) + m * (1.7149763477362134 * m - 5.6096736904047315 * y - 17.873870861415444 * k - 1401.7366389350734) + y * (-2.5217340131683033 * y - 21.248923337353073 * k + 4465.541406466231) - k * (21.86122147463605 * k + 48317.86113160301);
          var g = c * (8.841041422036149 * c + 60.118027045597366 * m + 6.871425592049007 * y + 31.159100130055922 * k - 20220.756542821975) + m * (-15.310361306967817 * m + 17.575251261109482 * y + 131.35250912493976 * k - 48691.05921601825) + y * (4.444339102852739 * y + 9.8632861493405 * k - 6341.191035517494) - k * (20.737325471181034 * k + 47890.15695978492);
          var b = c * (0.8842522430003296 * c + 8.078677503112928 * m + 30.89978309703729 * y - 0.23883238689178934 * k - 3616.812083916688) + m * (10.49593273432072 * m + 63.02378494754052 * y + 50.606957656360734 * k - 28620.90484698408) + y * (0.03296041114873217 * y + 115.60384449646641 * k - 49363.43385999684) - k * (22.33816807309886 * k + 45932.16563550634);

          data[offset++] = r >= 0 ? 255 : r <= min ? 0 : 255 + r * scale | 0;
          data[offset++] = g >= 0 ? 255 : g <= min ? 0 : 255 + g * scale | 0;
          data[offset++] = b >= 0 ? 255 : b <= min ? 0 : 255 + b * scale | 0;
        }
        return data;
      };

      JpegImage.prototype.getData = function (width, height, forceRGBoutput) {
        if (this.numComponents > 4) {
          throw 'Unsupported color mode';
        }

        var data = this._getLinearizedBlockData(width, height);

        if (this.numComponents === 3) {
          return this._convertYccToRgb(data);
        } else if (this.numComponents === 4) {
          if (this._isColorConversionNeeded()) {
            if (forceRGBoutput) {
              return this._convertYcckToRgb(data);
            } else {
              return this._convertYcckToCmyk(data);
            }
          } else {
            return this._convertCmykToRgb(data);
          }
        }
        return data;
      };

      JpegImage.prototype.copyToImageData = function (imageData) {
        var width = imageData.width, height = imageData.height;
        var imageDataBytes = width * height * 4;
        var imageDataArray = imageData.data;
        var data = this.getData(width, height, true);
        var i = 0, j = 0;
        var Y, K, C, M, R, G, B;
        switch (this.components.length) {
          case 1:
            while (j < imageDataBytes) {
              Y = data[i++];

              imageDataArray[j++] = Y;
              imageDataArray[j++] = Y;
              imageDataArray[j++] = Y;
              imageDataArray[j++] = 255;
            }
            break;
          case 3:
            while (j < imageDataBytes) {
              R = data[i++];
              G = data[i++];
              B = data[i++];

              imageDataArray[j++] = R;
              imageDataArray[j++] = G;
              imageDataArray[j++] = B;
              imageDataArray[j++] = 255;
            }
            break;
          case 4:
            while (j < imageDataBytes) {
              C = data[i++];
              M = data[i++];
              Y = data[i++];
              K = data[i++];

              R = 255 - clamp0to255(C * (1 - K / 255) + K);
              G = 255 - clamp0to255(M * (1 - K / 255) + K);
              B = 255 - clamp0to255(Y * (1 - K / 255) + K);

              imageDataArray[j++] = R;
              imageDataArray[j++] = G;
              imageDataArray[j++] = B;
              imageDataArray[j++] = 255;
            }
            break;
          default:
            throw 'Unsupported color mode';
        }
      };
      return JpegImage;
    })();
    JPEG.JpegImage = JpegImage;
  })(Shumway.JPEG || (Shumway.JPEG = {}));
  var JPEG = Shumway.JPEG;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    SWF.StreamNoDataError = {};

    function Stream_align() {
      this.bitBuffer = this.bitLength = 0;
    }

    function Stream_ensure(size) {
      if (this.pos + size > this.end) {
        throw SWF.StreamNoDataError;
      }
    }

    function Stream_remaining() {
      return this.end - this.pos;
    }

    function Stream_substream(begin, end) {
      var stream = new Stream(this.bytes);
      stream.pos = begin;
      stream.end = end;
      return stream;
    }

    function Stream_push(data) {
      var bytes = this.bytes;
      var newBytesLength = this.end + data.length;
      if (newBytesLength > bytes.length) {
        throw 'stream buffer overfow';
      }
      bytes.set(data, this.end);
      this.end = newBytesLength;
    }

    var Stream = (function () {
      function Stream(buffer, offset, length, maxLength) {
        if (offset === undefined)
          offset = 0;
        if (buffer.buffer instanceof ArrayBuffer) {
          offset += buffer.byteOffset;
          buffer = buffer.buffer;
        }
        if (length === undefined)
          length = buffer.byteLength - offset;
        if (maxLength === undefined)
          maxLength = length;

        var bytes = new Uint8Array(buffer, offset, maxLength);
        var stream = (new DataView(buffer, offset, maxLength));

        stream.bytes = bytes;
        stream.pos = 0;
        stream.end = length;
        stream.bitBuffer = 0;
        stream.bitLength = 0;

        stream.align = Stream_align;
        stream.ensure = Stream_ensure;
        stream.remaining = Stream_remaining;
        stream.substream = Stream_substream;
        stream.push = Stream_push;
        return stream;
      }
      return Stream;
    })();
    SWF.Stream = Stream;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
var Shumway;
(function (Shumway) {
  (function (SWF) {
    var SwfTag = Shumway.SWF.Parser.SwfTag;
    var createSoundStream = Shumway.SWF.Parser.createSoundStream;

    function defineSymbol(swfTag, symbols) {
      var symbol;

      switch (swfTag.code) {
        case 6 /* CODE_DEFINE_BITS */:
        case 21 /* CODE_DEFINE_BITS_JPEG2 */:
        case 35 /* CODE_DEFINE_BITS_JPEG3 */:
        case 90 /* CODE_DEFINE_BITS_JPEG4 */:
        case 8 /* CODE_JPEG_TABLES */:
          symbol = Shumway.SWF.Parser.defineImage(swfTag, symbols);
          break;
        case 20 /* CODE_DEFINE_BITS_LOSSLESS */:
        case 36 /* CODE_DEFINE_BITS_LOSSLESS2 */:
          symbol = Shumway.SWF.Parser.defineBitmap(swfTag);
          break;
        case 7 /* CODE_DEFINE_BUTTON */:
        case 34 /* CODE_DEFINE_BUTTON2 */:
          symbol = Shumway.SWF.Parser.defineButton(swfTag, symbols);
          break;
        case 37 /* CODE_DEFINE_EDIT_TEXT */:
          symbol = Shumway.SWF.Parser.defineText(swfTag, symbols);
          break;
        case 10 /* CODE_DEFINE_FONT */:
        case 48 /* CODE_DEFINE_FONT2 */:
        case 75 /* CODE_DEFINE_FONT3 */:
        case 91 /* CODE_DEFINE_FONT4 */:
          symbol = Shumway.SWF.Parser.defineFont(swfTag, symbols);
          break;
        case 46 /* CODE_DEFINE_MORPH_SHAPE */:
        case 84 /* CODE_DEFINE_MORPH_SHAPE2 */:
        case 2 /* CODE_DEFINE_SHAPE */:
        case 22 /* CODE_DEFINE_SHAPE2 */:
        case 32 /* CODE_DEFINE_SHAPE3 */:
        case 83 /* CODE_DEFINE_SHAPE4 */:
          symbol = Shumway.SWF.Parser.defineShape(swfTag, symbols);
          break;
        case 14 /* CODE_DEFINE_SOUND */:
          symbol = Shumway.SWF.Parser.defineSound(swfTag, symbols);
          break;
        case 87 /* CODE_DEFINE_BINARY_DATA */:
          symbol = {
            type: 'binary',
            id: swfTag.id,
            data: swfTag.data
          };
          break;
        case 39 /* CODE_DEFINE_SPRITE */:
          var commands = [];
          var frame = { type: 'frame' };
          var frames = [];
          var tags = swfTag.tags;
          var frameScripts = null;
          var frameIndex = 0;
          var soundStream = null;
          for (var i = 0, n = tags.length; i < n; i++) {
            var tag = tags[i];
            switch (tag.code) {
              case 12 /* CODE_DO_ACTION */:
                if (!frameScripts)
                  frameScripts = [];
                frameScripts.push(frameIndex);
                frameScripts.push(tag.actionsData);
                break;

              case 15 /* CODE_START_SOUND */:
                var startSounds = frame.startSounds || (frame.startSounds = []);
                startSounds.push(tag);
                break;
              case 18 /* CODE_SOUND_STREAM_HEAD */:
                try  {
                  soundStream = createSoundStream(tag);
                  frame.soundStream = soundStream.info;
                } catch (e) {
                }
                break;
              case 19 /* CODE_SOUND_STREAM_BLOCK */:
                if (soundStream) {
                  frame.soundStreamBlock = soundStream.decode(tag.data);
                }
                break;
              case 43 /* CODE_FRAME_LABEL */:
                frame.labelName = tag.name;
                break;
              case 4 /* CODE_PLACE_OBJECT */:
              case 26 /* CODE_PLACE_OBJECT2 */:
              case 70 /* CODE_PLACE_OBJECT3 */:
                commands.push(tag);
                break;
              case 5 /* CODE_REMOVE_OBJECT */:
              case 28 /* CODE_REMOVE_OBJECT2 */:
                commands.push(tag);
                break;
              case 1 /* CODE_SHOW_FRAME */:
                frameIndex += tag.repeat;
                frame.repeat = tag.repeat;
                frame.commands = commands;
                frames.push(frame);
                commands = [];
                frame = { type: 'frame' };
                break;
            }
          }
          symbol = {
            type: 'sprite',
            id: swfTag.id,
            frameCount: swfTag.frameCount,
            frames: frames,
            frameScripts: frameScripts
          };
          break;
        case 11 /* CODE_DEFINE_TEXT */:
        case 33 /* CODE_DEFINE_TEXT2 */:
          symbol = Shumway.SWF.Parser.defineLabel(swfTag, symbols);
          break;
      }

      if (!symbol) {
        return { command: 'error', message: 'unknown symbol type: ' + swfTag.code };
      }

      symbol.isSymbol = true;
      symbols[swfTag.id] = symbol;
      return symbol;
    }

    function createParsingContext(commitData) {
      var commands = [];
      var symbols = {};
      var frame = { type: 'frame' };
      var tagsProcessed = 0;
      var soundStream = null;
      var bytesLoaded = 0;

      return {
        onstart: function (result) {
          commitData({ command: 'init', result: result });
        },
        onprogress: function (result) {
          if (result.bytesLoaded - bytesLoaded >= 65536) {
            while (bytesLoaded < result.bytesLoaded) {
              if (bytesLoaded) {
                commitData({
                  command: 'progress', result: {
                    bytesLoaded: bytesLoaded,
                    bytesTotal: result.bytesTotal
                  } });
              }
              bytesLoaded += 65536;
            }
          }

          var tags = result.tags;
          for (var n = tags.length; tagsProcessed < n; tagsProcessed++) {
            var tag = tags[tagsProcessed];
            if ('id' in tag) {
              var symbol = defineSymbol(tag, symbols);
              commitData(symbol, symbol.transferables);
              continue;
            }

            switch (tag.code) {
              case 86 /* CODE_DEFINE_SCENE_AND_FRAME_LABEL_DATA */:
                frame.sceneData = tag;
                break;
              case 78 /* CODE_DEFINE_SCALING_GRID */:
                var symbolUpdate = {
                  isSymbol: true,
                  id: tag.symbolId,
                  updates: {
                    scale9Grid: tag.splitter
                  }
                };
                commitData(symbolUpdate);
                break;
              case 82 /* CODE_DO_ABC */:
              case 72 /* CODE_DO_ABC_ */:
                commitData({
                  type: 'abc',
                  flags: tag.flags,
                  name: tag.name,
                  data: tag.data
                });
                break;
              case 12 /* CODE_DO_ACTION */:
                var actionBlocks = frame.actionBlocks;
                if (actionBlocks)
                  actionBlocks.push(tag.actionsData);
                else
                  frame.actionBlocks = [tag.actionsData];
                break;
              case 59 /* CODE_DO_INIT_ACTION */:
                var initActionBlocks = frame.initActionBlocks || (frame.initActionBlocks = []);
                initActionBlocks.push({ spriteId: tag.spriteId, actionsData: tag.actionsData });
                break;
              case 15 /* CODE_START_SOUND */:
                var startSounds = frame.startSounds;
                if (!startSounds)
                  frame.startSounds = startSounds = [];
                startSounds.push(tag);
                break;
              case 18 /* CODE_SOUND_STREAM_HEAD */:
                try  {
                  soundStream = createSoundStream(tag);
                  frame.soundStream = soundStream.info;
                } catch (e) {
                }
                break;
              case 19 /* CODE_SOUND_STREAM_BLOCK */:
                if (soundStream) {
                  frame.soundStreamBlock = soundStream.decode(tag.data);
                }
                break;
              case 56 /* CODE_EXPORT_ASSETS */:
                var exports = frame.exports;
                if (exports)
                  frame.exports = exports.concat(tag.exports);
                else
                  frame.exports = tag.exports.slice(0);
                break;
              case 76 /* CODE_SYMBOL_CLASS */:
                var symbolClasses = frame.symbolClasses;
                if (symbolClasses)
                  frame.symbolClasses = symbolClasses.concat(tag.exports);
                else
                  frame.symbolClasses = tag.exports.slice(0);
                break;
              case 43 /* CODE_FRAME_LABEL */:
                frame.labelName = tag.name;
                break;
              case 4 /* CODE_PLACE_OBJECT */:
              case 26 /* CODE_PLACE_OBJECT2 */:
              case 70 /* CODE_PLACE_OBJECT3 */:
                commands.push(tag);
                break;
              case 5 /* CODE_REMOVE_OBJECT */:
              case 28 /* CODE_REMOVE_OBJECT2 */:
                commands.push(tag);
                break;
              case 9 /* CODE_SET_BACKGROUND_COLOR */:
                frame.bgcolor = tag.color;
                break;
              case 1 /* CODE_SHOW_FRAME */:
                frame.repeat = tag.repeat;
                frame.commands = commands;
                frame.complete = !!tag.finalTag;
                commitData(frame);
                commands = [];
                frame = { type: 'frame' };
                break;
            }
          }

          if (result.bytesLoaded >= result.bytesTotal) {
            commitData({
              command: 'progress', result: {
                bytesLoaded: result.bytesLoaded,
                bytesTotal: result.bytesTotal
              } });
          }
        },
        oncomplete: function (result) {
          commitData(result);

          var stats;
          if (typeof result.swfVersion === 'number') {
            var bbox = result.bbox;
            stats = {
              topic: 'parseInfo',
              parseTime: result.parseTime,
              bytesTotal: result.bytesTotal,
              swfVersion: result.swfVersion,
              frameRate: result.frameRate,
              width: (bbox.xMax - bbox.xMin) / 20,
              height: (bbox.yMax - bbox.yMin) / 20,
              isAvm2: !!result.fileAttributes.doAbc
            };
          }

          commitData({ command: 'complete', stats: stats });
        },
        onexception: function (e) {
          commitData({ type: 'exception', message: e.message, stack: e.stack });
        }
      };
    }

    function parseBytes(bytes, commitData) {
      Shumway.SWF.Parser.parse(bytes, createParsingContext(commitData));
    }

    var ResourceLoader = (function () {
      function ResourceLoader(scope, isWorker) {
        this._subscription = null;

        var self = this;
        if (!isWorker) {
          this._messenger = {
            postMessage: function (data) {
              self.onmessage({ data: data });
            }
          };
        } else {
          this._messenger = scope;
          scope.onmessage = function (event) {
            self.listener(event.data);
          };
        }
      }
      ResourceLoader.prototype.terminate = function () {
        this._messenger = null;
        this.listener = null;
      };

      ResourceLoader.prototype.onmessage = function (event) {
        this.listener(event.data);
      };

      ResourceLoader.prototype.postMessage = function (data) {
        this.listener && this.listener(data);
      };

      ResourceLoader.prototype.listener = function (data) {
        if (this._subscription) {
          this._subscription.callback(data.data, data.progress);
        } else if (data === 'pipe:') {
          this._subscription = {
            subscribe: function (callback) {
              this.callback = callback;
            }
          };
          this.parseLoadedData(this._messenger, this._subscription);
        } else {
          this.parseLoadedData(this._messenger, data);
        }
      };

      ResourceLoader.prototype.parseLoadedData = function (loader, request) {
        function commitData(data, transferables) {
          try  {
            loader.postMessage(data, transferables);
          } catch (ex) {
            if (ex != 'DataCloneError') {
              throw ex;
            }
            loader.postMessage(data);
          }
        }

        if (request instanceof ArrayBuffer) {
          parseBytes(request, commitData);
        } else if ('subscribe' in request) {
          var pipe = Shumway.SWF.Parser.parseAsync(createParsingContext(commitData));
          request.subscribe(function (data, progress) {
            if (data) {
              pipe.push(data, progress);
            } else {
              pipe.close();
            }
          });
        } else if (typeof FileReaderSync !== 'undefined') {
          var readerSync = new FileReaderSync();
          var buffer = readerSync.readAsArrayBuffer(request);
          parseBytes(buffer, commitData);
        } else {
          var reader = new FileReader();
          reader.onload = function () {
            parseBytes(this.result, commitData);
          };
          reader.readAsArrayBuffer(request);
        }
      };
      return ResourceLoader;
    })();
    SWF.ResourceLoader = ResourceLoader;
  })(Shumway.SWF || (Shumway.SWF = {}));
  var SWF = Shumway.SWF;
})(Shumway || (Shumway = {}));
//# sourceMappingURL=swf.js.map

console.timeEnd("Load SWF Parser");
