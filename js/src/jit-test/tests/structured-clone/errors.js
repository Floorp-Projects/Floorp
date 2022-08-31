/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

load(libdir + "asserts.js");

function roundtrip(error) {
  let opts = {ErrorStackFrames: "allow"};
  return deserialize(serialize(error, [], opts), opts);
}

// Basic
{
  let error = new Error("hello world");
  let cloned = roundtrip(error);

  assertDeepEq(cloned, error);
  assertEq(cloned.name, "Error");
  assertEq(cloned.message, "hello world");
  assertEq(cloned.stack, error.stack);
}

let constructors = [Error, EvalError, RangeError, ReferenceError,
                    SyntaxError, TypeError, URIError];
for (let constructor of constructors) {
  // With message
  let error = new constructor("hello");
  let cloned = roundtrip(error);
  assertDeepEq(cloned, error);
  assertEq(cloned.hasOwnProperty('message'), true);
  assertEq(cloned instanceof constructor, true);

  // Without message
  error = new constructor();
  cloned = roundtrip(error);
  assertDeepEq(cloned, error);
  assertEq(cloned.hasOwnProperty('message'), false);
  assertEq(cloned instanceof constructor, true);

  // Custom name
  error = new constructor("hello");
  error.name = "MyError";
  cloned = roundtrip(error);
  assertEq(cloned.name, "Error");
  assertEq(cloned.message, "hello");
  assertEq(cloned.stack, error.stack);
  if (constructor !== Error) {
    assertEq(cloned instanceof constructor, false);
  }

  // |cause| property
  error = new constructor("hello", { cause: new Error("foobar") });
  cloned = roundtrip(error);
  assertDeepEq(cloned, error);
  assertEq(cloned.hasOwnProperty('message'), true);
  assertEq(cloned instanceof constructor, true);
  assertEq(cloned.stack, error.stack);
  assertEq(cloned.stack === undefined, false);

  // |cause| property, manually added after construction.
  error = new constructor("hello");
  error.cause = new Error("foobar");
  assertDeepEq(Object.getOwnPropertyDescriptor(error, "cause"), {
    value: error.cause,
    writable: true,
    enumerable: true,
    configurable: true,
  });
  cloned = roundtrip(error);
  assertDeepEq(Object.getOwnPropertyDescriptor(cloned, "cause"), {
    value: cloned.cause,
    writable: true,
    enumerable: false,  // Non-enumerable in the cloned object!
    configurable: true,
  });
  assertEq(cloned.hasOwnProperty('message'), true);
  assertEq(cloned instanceof constructor, true);
  assertEq(cloned.stack, error.stack);
  assertEq(cloned.stack === undefined, false);

  // Subclassing
  error = new (class MyError extends constructor {});
  cloned = roundtrip(error);
  assertEq(cloned.name, constructor.name);
  assertEq(cloned.hasOwnProperty('message'), false);
  assertEq(cloned.stack, error.stack);
  assertEq(cloned instanceof Error, true);

  // Cross-compartment
  error = evalcx(`new ${constructor.name}("hello")`);
  cloned = roundtrip(error);
  assertEq(cloned.name, constructor.name);
  assertEq(cloned.message, "hello");
  assertEq(cloned.stack, error.stack);
  assertEq(cloned instanceof constructor, true);
}

// Non-string message
{
  let error = new Error("hello world");
  error.message = 123;
  let cloned = roundtrip(error);
  assertEq(cloned.message, "123");
  assertEq(cloned.hasOwnProperty('message'), true);

  error = new Error();
  Object.defineProperty(error, 'message', { get: () => {} });
  cloned = roundtrip(error);
  assertEq(cloned.message, "");
  assertEq(cloned.hasOwnProperty('message'), false);
}

// AggregateError
{
  // With message
  let error = new AggregateError([{a: 1}, {b: 2}], "hello");
  let cloned = roundtrip(error);
  assertDeepEq(cloned, error);
  assertEq(cloned.hasOwnProperty('message'), true);
  assertEq(cloned instanceof AggregateError, true);

  // Without message
  error = new AggregateError([{a: 1}, {b: 2}]);
  cloned = roundtrip(error);
  assertDeepEq(cloned, error);
  assertEq(cloned.hasOwnProperty('message'), false);
  assertEq(cloned instanceof AggregateError, true);

  // Custom name breaks this!
  error = new AggregateError([{a: 1}, {b: 2}]);
  error.name = "MyError";
  cloned = roundtrip(error);
  assertEq(cloned.name, "Error");
  assertEq(cloned.message, "");
  assertEq(cloned.stack, error.stack);
  assertEq(cloned instanceof AggregateError, false);
  assertEq(cloned.errors, undefined);
  assertEq(cloned.hasOwnProperty('errors'), false);
}

{
  let error = new Error();

  // When serializing without stack-frames, deserialization is empty.
  let cloned = deserialize(serialize(error, [], {ErrorStackFrames: "deny"}),
    {ErrorStackFrames: "allow"});
  assertEq(cloned.name, "Error");
  assertEq(cloned.stack, "");

  // Defaults to disallow.
  cloned = deserialize(serialize(error));
  assertEq(cloned.name, "Error");
  assertEq(cloned.stack, "");

  // Unexpected stack frames during deserialization throw.
  assertErrorMessage(() => {
    deserialize(serialize(error, [], {ErrorStackFrames: "allow"}),
      {ErrorStackFrames: "deny"});
  }, InternalError, "bad serialized structured data (disallowed 'stack' field encountered for Error object)");

  // Sanity check
  cloned = roundtrip(error);
  assertEq(cloned.stack.length > 0, true);
}