<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `util/array` module provides simple helper functions for working with 
arrays.

<api name="has">
@function
Returns `true` if the given [`Array`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array) contains the element or `false` otherwise.
A simplified version of `array.indexOf(element) >= 0`.

    let { has } = require('sdk/util/array');
    let a = ['rock', 'roll', 100];

    has(a, 'rock'); // true
    has(a, 'rush'); // false
    has(a, 100); // true

@param array {array}
  The array to search.

@param element {*}
  The element to search for in the array.

@returns {boolean}
  A boolean indicating whether or not the element was found in the array.
</api>

<api name="hasAny">
@function
Returns `true` if the given [`Array`](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array) contains any of the elements in the 
`elements` array, or `false` otherwise.

    let { hasAny } = require('sdk/util/array');
    let a = ['rock', 'roll', 100];

    hasAny(a, ['rock', 'bright', 'light']); // true
    hasAny(a, ['rush', 'coil', 'jet']); // false

@param array {array}
  The array to search for elements.

@param elements {array}
  An array of elements to search for in `array`.

@returns {boolean}
  A boolean indicating whether or not any of the elements were found
  in the array.
</api>

<api name="add">
@function
If the given [array](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array) does not already contain the given element, this function
adds the element to the array and returns `true`. Otherwise, this function
does not add the element and returns `false`.

    let { add } = require('sdk/util/array');
    let a = ['alice', 'bob', 'carol'];

    add(a, 'dave'); // true
    add(a, 'dave'); // false
    add(a, 'alice'); // false

    console.log(a); // ['alice', 'bob', 'carol', 'dave']

@param array {array}
  The array to add the element to.

@param element {*}
  The element to add

@returns {boolean}
  A boolean indicating whether or not the element was added to the array.
</api>

<api name="remove">
@function
If the given [array](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array) contains the given element, this function
removes the element from the array and returns `true`. Otherwise, this function
does not alter the array and returns `false`.

    let { remove } = require('sdk/util/array');
    let a = ['alice', 'bob', 'carol'];

    remove(a, 'dave'); // false
    remove(a, 'bob'); // true 
    remove(a, 'bob'); // false

    console.log(a); // ['alice', 'carol']

@param array {array}
  The array to remove the element from.

@param element {*}
  The element to remove from the array if it contains it.

@returns {boolean}
  A boolean indicating whether or not the element was removed from the array.
</api>

<api name="unique">
@function
Produces a duplicate-free version of the given [array](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array).

    let { unique } = require('sdk/util/array');
    let a = [1, 1, 1, 1, 3, 4, 7, 7, 10, 10, 10, 10];
    let b = unique(a);

    console.log(a); // [1, 1, 1, 1, 3, 4, 7, 7, 10, 10, 10, 10];
    console.log(b); // [1, 3, 4, 7, 10];

@param array {array}
  Source array.

@returns {array}
  The new, duplicate-free array.
</api>

<api name="flatten">
@function
Flattens a nested [array](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array) of any depth.

    let { flatten } = require('sdk/util/array');
    let a = ['cut', ['ice', ['fire']], 'elec'];
    let b = flatten(a);

    console.log(a); // ['cut', ['ice', ['fire']], 'elec']
    console.log(b); // ['cut', 'ice', 'fire', 'elec'];

@param array {array}
  The array to flatten.

@returns {array}
  The new, flattened array.
</api>

<api name="fromIterator">
@function
Iterates over an [iterator](https://developer.mozilla.org/en-US/docs/JavaScript/Guide/Iterators_and_Generators#Iterators) and returns the results as an [array](https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array).

    let { fromIterator } = require('sdk/util/array');
    let i = new Set();
    i.add('otoro');
    i.add('unagi');
    i.add('keon');

    fromIterator(i) // ['otoro', 'unagi', 'keon']

@param iterator {iterator}
  The [`Iterator`](https://developer.mozilla.org/en-US/docs/JavaScript/Guide/Iterators_and_Generators#Iterators) object over which to iterate and place results into an array.

@returns {array}
  The iterator's results in an array.
</api>

