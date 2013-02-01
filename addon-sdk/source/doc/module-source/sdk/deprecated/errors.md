<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com] -->

<div class="warning">
<p>The <code>errors</code> module is deprecated.</p>
</div>

The `errors` module provides helpers for safely invoking user callbacks.

<api name="catchAndLog">
@function
  Wraps a callback in a function that when invoked will catch and log any
  exception thrown by the callback.
@param callback {function}
  The callback to wrap.
@param [defaultResponse] {value}
  This value will be returned by the wrapper if `callback` throws an exception.
  If not given, `undefined` is used.
@param [logException] {function}
  When `callback` throws an exception, it will be passed to this function.  If
  not given, the exception is logged using `console.exception()`.
@returns {function}
  A function that will invoke `callback` when called.  The return value of this
  function is the return value of `callback` unless `callback` throws an
  exception.  In that case, `defaultResponse` is returned or `undefined` if
  `defaultResponse` is not given.
</api>

<api name="catchAndLogProps">
@function
  Replaces methods of an object with wrapped versions of those methods returned
  by `catchAndLog()`.
@param object {object}
  The object whose methods to replace.
@param props {string,array}
  The names of the methods of `object` to replace, either a string for a single
  method or an array of strings for multiple methods.
@param [defaultResponse] {value}
  This value will be returned by any wrapper whose wrapped method throws an
  exception.  If not given, `undefined` is used.
@param [logException] {function}
  See `catchAndLog()`.
</api>
