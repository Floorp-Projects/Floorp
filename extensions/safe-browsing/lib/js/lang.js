/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Boodman <aa@google.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This file has pure js helper functions. Hence you'll find metion
// of browser-specific features in here.


/**
 * lang.js - The missing JavaScript language features
 *
 * WARNING: This class adds members to the prototypes of String, Array, and
 * Function for convenience.
 *
 * The tradeoff is that the for/in statement will not work properly for those
 * objects when this library is used.
 *
 * To work around this for Arrays, you may want to use the forEach() method,
 * which is more fun and easier to read.
 */

/**
 * Returns true if the specified value is not |undefined|.
 */
function isDef(val) {
  return typeof val != "undefined";
}

/**
 * Returns true if the specified value is |null|
 */
function isNull(val) {
  return val === null;
}

/**
 * Returns true if the specified value is an array
 */
function isArray(val) {
  return isObject(val) && val.constructor == Array;
}

/**
 * Returns true if the specified value is a string
 */
function isString(val) {
  return typeof val == "string";
}

/**
 * Returns true if the specified value is a boolean
 */
function isBoolean(val) {
  return typeof val == "boolean";
}

/**
 * Returns true if the specified value is a number
 */
function isNumber(val) {
  return typeof val == "number";
}

/**
 * Returns true if the specified value is a function
 */
function isFunction(val) {
  return typeof val == "function";
}

/**
 * Returns true if the specified value is an object
 */
function isObject(val) {
  return val && typeof val == "object";
}

/**
 * Returns an array of all the properties defined on an object
 */
function getObjectProps(obj) {
  var ret = [];

  for (var p in obj) {
    ret.push(p);
  }

  return ret;
}

/**
 * Returns true if the specified value is an object which has no properties
 * defined.
 */
function isEmptyObject(val) {
  if (!isObject(val)) {
    return false;
  }

  for (var p in val) {
    return false;
  }

  return true;
}

var getHashCode;
var removeHashCode;

(function () {
  var hashCodeProperty = "lang_hashCode_";

  /**
   * Adds a lang_hashCode_ field to an object. The hash code is unique for the
   * given object.
   * @param obj {Object} The object to get the hash code for
   * @returns {Number} The hash code for the object
   */
  getHashCode = function(obj) {
    // In IE, DOM nodes do not extend Object so they do not have this method.
    // we need to check hasOwnProperty because the proto might have this set.
    if (obj.hasOwnProperty && obj.hasOwnProperty(hashCodeProperty)) {
      return obj[hashCodeProperty];
    }
    if (!obj[hashCodeProperty]) {
      obj[hashCodeProperty] = ++getHashCode.hashCodeCounter_;
    }
    return obj[hashCodeProperty];
  };

  /**
   * Removes the lang_hashCode_ field from an object.
   * @param obj {Object} The object to remove the field from. 
   */
  removeHashCode = function(obj) {
    obj.removeAttribute(hashCodeProperty);
  };

  getHashCode.hashCodeCounter_ = 0;
})();

/**
 * Fast prefix-checker.
 */
String.prototype.startsWith = function(prefix) {
  if (this.length < prefix.length) {
    return false;
  }

  if (this.substring(0, prefix.length) == prefix) {
    return true;
  }

  return false;
}

/**
 * Removes whitespace from the beginning and end of the string
 */
String.prototype.trim = function() {
  return this.replace(/^\s+|\s+$/g, "");
}

/**
 * Does simple python-style string substitution.
 * "foo%s hot%s".subs("bar", "dog") becomes "foobar hotdot".
 * For more fully-featured templating, see template.js.
 */
String.prototype.subs = function() {
  var ret = this;

  // this appears to be slow, but testing shows it compares more or less equiv.
  // to the regex.exec method.
  for (var i = 0; i < arguments.length; i++) {
    ret = ret.replace(/\%s/, arguments[i]);
  }

  return ret;
}

/**
 * Some old browsers don't have Function.apply. So sad. We emulate it for them.
 */
if (!Function.prototype.apply) {
  Function.prototype.apply = function(oScope, args) {
    var sarg = [];
    var rtrn, call;

    if (!oScope) oScope = window;
    if (!args) args = [];

    for (var i = 0; i < args.length; i++) {
      sarg[i] = "args[" + i + "]";
    }

    call = "oScope.__applyTemp__.peek().(" + sarg.join(",") + ");";

    if (!oScope.__applyTemp__) {
      oScope.__applyTemp__ = [];
    }

    oScope.__applyTemp__.push(this);
    rtrn = eval(call);
    oScope.__applyTemp__.pop();

    return rtrn;
  }
}

/**
 * Emulate Array.push for browsers which don't have it.
 */
if (!Array.prototype.push) {
  Array.prototype.push = function() {
    for (var i = 0; i < arguments.length; i++) {
      this[this.length] = arguments[i];
    }

    return this.length;
  }
}

/**
 * Emulate Array.pop for browsers which don't have it.
 */
if (!Array.prototype.pop) {
  Array.prototype.pop = function() {
    if (!this.length) {
      return;
    }

    var val = this[this.length - 1];

    this.length--;

    return val;
  }
}

/**
 * Returns the last element on an array without removing it.
 */
Array.prototype.peek = function() {
  return this[this.length];
}

/**
 * Emulate Array.shift for browsers which don't have it.
 */
if (!Array.prototype.shift) {
  Array.prototype.shift = function() {
    if (this.length == 0) {
      return; // return undefined
    }

    var val = this[0];

    for (var i = 0; i < this.length - 1; i++) {
      this[i] = this[i+1];
    }

    this.length--;

    return val;
  }
}

/**
 * Emulate Array.unshift for browsers which don't have it.
 */
if (!Array.prototype.unshift) {
  Array.prototype.unshift = function() {
    var numArgs = arguments.length;

    for (var i = this.length - 1; i >= 0; i--) {
      this[i + numArgs] = this[i];
    }

    for (var j = 0; j < numArgs; j++) {
      this[j] = arguments[j];
    }

    return this.length;
  }
}

// TODO(anyone): add splice the first time someone needs it and then implement
// push, pop, shift, unshift in terms of it where possible.

/**
 * Emulate Array.forEach for browsers which don't have it
 */
if (!Array.prototype.forEach) {
  Array.prototype.forEach = function(callback, scope) {
    for (var i = 0; i < this.length; i++) {
      callback.apply(scope, [this[i]]);
    }
  }
}

// TODO(anyone): add the other neat-o functional methods like map(), etc.

/**
 * Partially applies this function to a particular "this object" and zero or
 * more arguments. The result is a new function with some arguments and the
 * value of |this| "pre-specified".
 *
 * Remaining arguments specified at call-time are appended to the pre-
 * specified ones.
 *
 * Usage:
 * var barMethBound = fooObj.barMeth.bind(fooObj, "arg1", "arg2");
 * barMethBound("arg3", "arg4");
 *
 * @param thisObj {object} Specifies the object which |this| should point to
 * when the function is run. If the value is null or undefined, it will default
 * to the global object.
 *
 * @returns {function} A partially-applied form of the function bind() was
 * invoked as a method of.
 */
Function.prototype.bind = function(thisObj) {
  if (typeof(this) != "function") {
    throw new Error("Bind must be called as a method of a function object.");
  }

  var self = this;

  // The arguments intrinsic is not a true array so it does not have the
  // splice() method. However, we can borrow Array's splice method on it to do
  // the same thing.

  // Also, it turns out that if you remove items from the arguments object,
  // then the formal parameters which correspond to them stop working. So,
  // for example, Array.prototype.splice.call(arguemnts, 0, 1) would make the
  // thisObj be undefined. So instead, we remove all arguments *but* the named
  // ones and store them in a separate array.

  var staticArgs = Array.prototype.splice.call(arguments, 1, arguments.length);

  return function() {
    // make a copy of staticArgs (don't modify it because it gets reused for
    // every invocation).
    var args = staticArgs.concat();

    // add all the new arguments
    for (var i = 0; i < arguments.length; i++) {
      args.push(arguments[i]);
    }

    // invoke the original function with the correct thisObj and the combined
    // list of static and dynamic arguments.
    return self.apply(thisObj, args);
  };
}

/**
 * Convenience. Binds all the methods of obj to itself. Calling this in the
 * constructor before referencing any methods makes things a little more like
 * Java or Python where methods are intrinsically bound to their instance.
 */
function bindMethods(obj) {
  for (var p in obj) {
    if (isFunction(obj[p])) {
      obj[p] = obj[p].bind(obj);
    }
  }
}

/**
 * Inherit the prototype methods from one constructor into another.
 *
 * Usage:
 * <pre>
 * function ParentClass(a, b) { }
 * ParentClass.prototype.foo = function(a) { }
 *
 * function ChildClass(a, b, c) {
 *   ParentClass.call(this, a, b);
 * }
 *
 * ChildClass.inherits(ParentClass);
 *
 * var child = new ChildClass("a", "b", "see");
 * child.foo(); // works
 * </pre>
 *
 * In addition, a superclass' implementation of a method can be invoked
 * as follows:
 *
 * <pre>
 * ChildClass.prototype.foo = function(a) {
 *   ChildClass.superClass_.foo.call(this, a);
 *   // other code
 * };
 * </pre>
 */
Function.prototype.inherits = function(parentCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = parentCtor.prototype;
  this.superClass_ = parentCtor.prototype;
  this.prototype = new tempCtor();
}
