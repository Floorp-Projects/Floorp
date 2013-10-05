<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `test/utils` module provides additional helper methods to be used in
the CommonJS Unit Testing test suite.

## Before and After

Helper functions `before()` and `after()` are available for running a function
before or after each test in a suite. They're useful when you need to
guarantee a particular state before running a test, and to clean up
after your test.

    let { before, after } = require('sdk/test/utils');
    let { search } = require('sdk/places/bookmarks');

    exports.testCountBookmarks = function (assert, done) {
      search().on('end', function (results) {
        assert.equal(results, 0, 'should be no bookmarks');
        done();
      });
    };

    before(exports, function (name, assert) {
      removeAllBookmarks();
    });

    require('sdk/test').run(exports);

Both `before` and `after` may be asynchronous. To make them asynchronous,
pass a third argument `done`, which is a function to call when you have
finished:

    let { before, after } = require('sdk/test/utils');
    let { search } = require('sdk/places/bookmarks');

    exports.testCountBookmarks = function (assert, done) {
      search().on('end', function (results) {
        assert.equal(results, 0, 'should be no bookmarks');
        done();
      });
    };

    before(exports, function (name, assert, done) {
      removeAllBookmarksAsync(function () {
        done();
      });
    });

    require('sdk/test').run(exports);

<api name="before">
@function
  Runs `beforeFn` before each test in the file. May be asynchronous
  if `beforeFn` accepts a third argument, which is a callback.

 @param exports {Object}
    A test file's `exports` object
 @param beforeFn {Function}
    The function to be called before each test. It has two arguments,
    or three if it is asynchronous:

   * the first argument is the test's name as a `String`.
   * the second argument is the `assert` object for the test.
   * the third, optional, argument is a callback. If the callback is
    defined, then the `beforeFn` is considered asynchronous, and the
    callback must be invoked before the test runs.

</api>

<api name="after">
@function
  Runs `afterFn` after each test in the file. May be asynchronous
  if `afterFn` accepts a third argument, which is a callback.

 @param exports {Object}
    A test file's `exports` object
 @param afterFn {Function}
    The function to be called after each test. It has two arguments,
    or three if it is asynchronous:

   * the first argument is the test's name as a `String`.
   * the second argument is the `assert` object for the test.
   * the third, optional, argument is a callback. If the callback is
    defined, then the `afterFn` is considered asynchronous, and the
    callback must be invoked before the next test runs.

</api>

<api name="waitUntil">
@function
  `waitUntil` returns a promise that resolves upon the
  `predicate` returning a truthy value, which is called every
  `interval` milliseconds.

 @param predicate {Function}
    A function that gets called every `interval` milliseconds
    to determine if the promise should be resolved.

 @param [interval] {Number}
    The frequency in milliseconds to execute `predicate`.
    Defaults to `10`.

 @returns {Promise}
    `waitUntil` returns a promise that becomes resolved once
    the `predicate` returns a truthy value. The promise cannot
    be rejected.

</api>

