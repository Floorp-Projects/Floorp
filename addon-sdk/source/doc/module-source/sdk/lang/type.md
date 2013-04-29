<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `lang/type` module provides simple helper functions for working with type
detection.

<api name="isUndefined">
@function
Returns `true` if `value` is [`undefined`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/undefined), `false` otherwise.

    let { isUndefined } = require('sdk/lang/type');

    var foo;
    isUndefined(foo); // true
    isUndefined(0); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is `undefined`.
</api>

<api name="isNull">
@function
Returns `true` if `value` is [`null`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/null), `false` otherwise.

    let { isNull } = require('sdk/lang/type');
    
    isNull(null); // true
    isNull(false); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is `null`.
</api>

<api name="isString">
@function
Returns `true` if `value` is a [`String`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String),
`false` otherwise. Uses [`typeof`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Operators/typeof)
operator to check type, and will only properly detect string primitives:
for example, a string created with `new String()` will always return false.

    let { isString } = require('sdk/lang/type');
    
    isString('my string'); // true
    isString(100); // false
    isString('100'); // true

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is a `String`.
</api>

<api name="isNumber">
@function
Returns `true` if `value` is a [`Number`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number),
`false` otherwise. Uses [`typeof`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Operators/typeof)
operator to check type, and will only properly detect number primitives:
for example, a number created with `new Number()` will always return false.

    let { isNumber } = require('sdk/lang/type');
    
    isNumber(3.1415); // true
    isNumber(100); // true
    isNumber('100'); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is a `Number`.
</api>

<api name="isRegExp">
@function
Returns `true` if `value` is a [`RegExp`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp), `false` otherwise.

    let { isRegExp } = require('sdk/lang/type');
    
    isRegExp(/[^\.]*\.js$/); // true
    isRegExp(new RegExp('substring')); // true
    isRegExp(1000); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is a `RegExp`.
</api>

<api name="isDate">
@function
Returns `true` if `value` is a [`Date`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date), `false` otherwise.

    let { isDate } = require('sdk/lang/type');
    
    isDate(new Date()); // true
    isDate('3/1/2013'); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is a `Date`.
</api>

<api name="isFunction">
@function
Returns `true` if `value` is a [`Function`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Function), `false` otherwise.

    let { isFunction } = require('sdk/lang/type');
    
    let fn = function () {};
    isFunction(fn); // true;
    isFunction(otherFn); // true;
    isFunction(function () {}); // true;

    function otherFn () {}

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is a `Function`.
</api>

<api name="isObject">
@function
Returns `true` if `value` is an [`Object`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object) and not null, `false` otherwise.

    let { isObject } = require('sdk/lang/type');

    isObject({}); // true
    isObject(new Class()); // true
    isObject(null); // false
    isObject(5); // false

    function Class () {}

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is an `Object`.
</api>

<api name="isArray">
@function
Returns `true` if `value` is an [`Array`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array), `false` otherwise. Uses native
[`Array.isArray`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/isArray).

    let { isArray } = require('sdk/lang/type');
    
    isArray([]); // true
    isArray({}); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is an `Array`.
</api>

<api name="isArguments">
@function
Returns `true` if `value` is an array-like [`arguments`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Functions_and_function_scope/arguments) object,
`false` otherwise.

    let { isArguments } = require('sdk/lang/type');
    
    function run () {
      isArguments(arguments); // true
      isArguments([]); // false
      isArguments(Array.slice(arguments)); // false
    }
    run(1, 2, 3);

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is an `arguments` object.
</api>

<api name="isPrimitive">
@function
Returns `true` if `value` is a primitive value: that is, any of [`null`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/null), [`undefined`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/undefined), [`number`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/number),
[`boolean`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/boolean), or [`string`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/string). Returns `false` if `value` is not a primitive value.

    let { isPrimitive } = require('sdk/lang/type');
    
    isPrimitive(3); // true
    isPrimitive('foo'); // true
    isPrimitive({}); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is a primitive.
</api>

<api name="isFlat">
@function
Returns `true` if `value` is a direct descendant of `Object.prototype` or `null`.
Similar to jQuery's [`isPlainObject`](http://api.jquery.com/jQuery.isPlainObject/).

    let { isFlat } = require('sdk/lang/type');
    
    isFlat({}); // true
    isFlat(new Type()); // false

    function Type () {}

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is a direct descendant of `Object.prototype` or `null`.
</api>

<api name="isEmpty">
@function
Returns `true` if `value` is an [`Object`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object) with no properties and `false` otherwise.

    let { isEmpty } = require('sdk/lang/type');
    
    isEmpty({}); // true
    isEmpty({ init: false }); // false

@param value {object}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is an `Object` with no properties.
</api>

<api name="isJSON">
@function
Returns `true` if `value` is a string, number, boolean, null, array of JSON-serializable values, or an object whose property values are themselves JSON-serializable. Returns `false` otherwise.

    let { isJSON } = require('sdk/lang/type');
    
    isJSON({ value: 42 }); // true
    isJSON({ fn: function () {} ); // false

@param value {mixed}
  The variable to check.

@returns {boolean}
  Boolean indicating if `value` is an `Array`/flat `Object` containing only
  atomic values and other flat objects.
</api>

<api name="instanceOf">
@function
Returns `true` if `value` is an instance of a given `Type`. This is similar to the [`instanceof`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Operators/instanceof) operator.
The difference is that the `Type` constructor can be from a scope that has
a different top level object: for example, it could be from a different iframe,
module or sandbox.

    let { instanceOf } = require('sdk/lang/type');
    
    instanceOf(new Class(), Class); // true
    function Class() {}

@param value {object}
  The variable to check.

@param Type {object}
  The constructor to compare to `value`

@returns {boolean}
  Boolean indicating if `value` is an instance of `Type`.
</api>

<api name="source">
@function
Returns the textual representation of `value`, containing property descriptors and types
of properties contained within the object.

    let { source } = require('sdk/lang/type');
    
    var obj = {
      name: undefined,
      twitter: '@horse_js',
      tweets: [
        { id: 100, text: 'What happens to you if you break the monad laws?' },
        { id: 101, text: 'JAVASCRIPT DUBSTEP GENERATOR' }
      ]
    };

    console.log(source(obj));
    // Prints the below
    /*
    { // [object Object]
        // writable configurable enumerable
        name: undefined,
        // writable configurable enumerable
        twitter: "@horse_js",
        // writable configurable enumerable
        tweets: [
            { // [object Object]
                // writable configurable enumerable
                id: 100,
                // writable configurable enumerable
                text: "What happens to you if you break the monad laws?",
                "__proto__": { // [object Object]

                }
            },
            { // [object Object]
                // writable configurable enumerable
                id: 101,
                // writable configurable enumerable
                text: "JAVASCRIPT DUBSTEP GENERATOR",
                "__proto__": { // [object Object]

                }
            }
        ],
        "__proto__": { // [object Object]

        }
    }
    */

@param value {mixed}
  The source object to create a textual representation of.

@param indent {string}
  Optional. `String` to be used as indentation in output. 4 spaces by default.

@param limit {number}
  Optional. Number of properties to display per object.

@returns {string}
  The textual representation of `value`.
</api>
