<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `assert` module implements the `assert` interface defined in
the [CommonJS Unit Testing specification version 1.1](http://wiki.commonjs.org/wiki/Unit_Testing/1.1).

To use the `assert` module, write a set of
unit tests following the instructions in the
[unit testing tutorial](dev-guide/tutorials/unit-testing.html).
Each test will be passed an `Assert` object when you run the tests using
[`cfx test`](file:///Users/Work/mozilla/jetpack-sdk/doc/dev-guide/cfx-tool.html#cfx_test).
You can use this object to make assertions about your program's state.

For example:

    var a = 1;

    exports["test value of a"] = function(assert) {
      assert.ok(a == 1, "test that a is 1");
    }
    
    require("sdk/test").run(exports);

<api name="Assert">
@class
An object used to make assertions about a program's state in order
to implement unit tests.

The `Assert` object's interface is defined by the
[CommonJS Unit Testing specification, version 1.1](http://wiki.commonjs.org/wiki/Unit_Testing/1.1).

<api name="Assert">
@constructor
  Create a new Assert object. This function is only called by the
unit test framework, and not by unit tests themselves.

@param logger {object}
  Object used to log the results of assertions.
</api>

<api name="ok">
@method
  Tests whether an expression evaluates to true.

      assert.ok(a == 1, "test that a is equal to one"); 

  This is equivalent to:

      assert.equal(a == 1, true, "test that a is equal to one");

@param guard {expression}
  The expression to evaluate.

@param [message] {string}
  Optional message to log, providing extra information about the test.
</api>

<api name="equal">
@method
  Tests shallow, coercive equality with `==`:

      assert.equal(1, 1, "test that one is one"); 

@param actual {object}
  The actual result.

@param expected {object}
  The expected result.

@param [message] {string}
  Optional message to log, providing extra information about the test.
</api>

<api name="notEqual">
@method
  Tests that two objects are not equal, using `!=`:

      assert.notEqual(1, 2, "test that one is not two"); 

@param actual {object}
  The actual result.

@param expected {object}
  The expected result.

@param [message] {string}
  Optional message to log, providing extra information about the test.
</api>

<api name="deepEqual">
@method
  Tests that two objects have a deep equality relation.
  Deep equality is defined in the
  [CommonJS specification for Assert](http://wiki.commonjs.org/wiki/Unit_Testing/1.0#Assert),
  item 7, which is quoted here:

<div class="quote">
<ol>
<li>All identical values are equivalent, as determined by ===.</li>
<li>If the expected value is a Date object, the actual value is equivalent
if it is also a Date object that refers to the same time.</li>
<li>Other pairs that do not both pass typeof value == "object", equivalence
is determined by ==.</li>
<li>For all other Object pairs, including Array objects, equivalence
is determined by having the same number of owned properties (as verified
with Object.prototype.hasOwnProperty.call), the same set of keys (although
not necessarily the same order), equivalent values for every corresponding
key, and an identical "prototype" property. Note: this accounts for both
named and indexed properties on Arrays.</li>
</div>

    assert.deepEqual({ a: "foo" }, { a: "foo" }, "equivalent objects");

@param actual {object}
  The actual result.

@param expected {object}
  The expected result.

@param [message] {string}
  Optional message to log, providing extra information about the test.
</api>

<api name="notDeepEqual">
@method
  Tests that two objects do not have a deep equality relation, using
  the negation of the test for deep equality:

    assert.notDeepEqual({ a: "foo" }, Object.create({ a: "foo" }),
	                    "object's inherit from different prototypes");

@param actual {object}
  The actual result.

@param expected {object}
  The expected result.

@param [message] {string}
  Optional message to log, providing extra information about the test.
</api>

<api name="strictEqual">
@method
  Tests that two objects are equal, using the strict equality operator `===`:

    // This test will pass, because "==" will perform type conversion
    exports["test coercive equality"] = function(assert) {
	  assert.equal(1, "1", "test coercive equality between 1 and '1'");
	}

    // This test will fail, because the types are different
	exports["test strict equality"] = function(assert) {
	  assert.strictEqual(1, "1", "test strict equality between 1 and '1'");
	}

@param actual {object}
  The actual result.

@param expected {object}
  The expected result.

@param [message] {string}
  Optional message to log, providing extra information about the test.
</api>

<api name="notStrictEqual">
@method
  Tests that two objects are not equal, using the negation of the strict
  equality operator `===`:

    // This test will fail, because "==" will perform type conversion
    exports["test coercive equality"] = function(assert) {
	  assert.notEqual(1, "1", "test coercive equality between 1 and '1'");
	}

    // This test will pass, because the types are different
	exports["test strict equality"] = function(assert) {
	  assert.notStrictEqual(1, "1", "test strict equality between 1 and '1'");
	}

@param actual {object}
  The actual result.

@param expected {object}
  The expected result.

@param [message] {string}
  Optional message to log, providing extra information about the test.
</api>

<api name="throws">
@method
  Assert that a block of code throws the expected exception.

  This method takes an optional `Error` argument:

  * to check that the exception thrown is of the expected type, pass
  a constructor function: the exception thrown must be an
  instance of the object returned by that function.

  * to check that the exception thrown contains a specific message, pass
  a regular expression here: the `message` property of the exception
  thrown must match the regular expression

<!--terminate Markdown list-->

  For example, suppose we define two different custom exceptions:

    function MyError(message) {  
      this.name = "MyError";  
      this.message = message || "Default Message";  
    }

    MyError.prototype = new Error();  
    MyError.prototype.constructor = MyError;

    function AnotherError(message) {
      this.name = "AnotherError";  
      this.message = message || "Default Message"; 
        console.log(this.message); 
    }

    AnotherError.prototype = new Error();  
    AnotherError.prototype.constructor = AnotherError;

  We can check the type of exception by passing a function as the `Error`
  argument:

    exports["test exception type 1 expected to pass"] = function(assert) {
      assert.throws(function() {
        throw new MyError("custom message");
      },
      MyError,
      "test throwing a specific exception");
    }

    exports["test exception type 2 expected to fail"] = function(assert) {
      assert.throws(function() {
        throw new MyError("custom message");
      },
      AnotherError,
      "test throwing a specific exception");
    }

  We can check the message by passing a regular expression:

    exports["test exception message 1 expected to pass"] = function(assert) {
      assert.throws(function() {
        throw new MyError("custom message");
      },
      /custom message/,
      "test throwing a specific message");
    }

    exports["test exception message 2 expected to pass"] = function(assert) {
      assert.throws(function() {
        throw new AnotherError("custom message");
      },
      /custom message/,
      "test throwing a specific exception");
    }

    exports["test exception message 3 expected to fail"] = function(assert) {
      assert.throws(function() {
        throw new MyError("another message");
      },
      /custom message/,
      "test throwing a specific message");
    }

  @param block {block}
  The block of code to test.

  @param [error] {function|RegExp}
    Either a constructor function returning the type of exception
    expected, or a regular expression expected to match the exception's
    `message` property.	

	@param [message] {string}
	  Optional message to log, providing extra information about the test.
</api>

</api>
