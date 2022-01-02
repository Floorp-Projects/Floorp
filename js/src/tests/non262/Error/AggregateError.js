assertEq(typeof AggregateError, "function");
assertEq(Object.getPrototypeOf(AggregateError), Error);
assertEq(AggregateError.name, "AggregateError");
assertEq(AggregateError.length, 2);

assertEq(Object.getPrototypeOf(AggregateError.prototype), Error.prototype);
assertEq(AggregateError.prototype.name, "AggregateError");
assertEq(AggregateError.prototype.message, "");

// The |errors| argument is mandatory.
assertThrowsInstanceOf(() => new AggregateError(), TypeError);
assertThrowsInstanceOf(() => AggregateError(), TypeError);

// The .errors data property is an array object.
{
  let err = new AggregateError([]);

  let {errors} = err;
  assertEq(Array.isArray(errors), true);
  assertEq(errors.length, 0);

  // The errors object is modifiable.
  errors.push(123);
  assertEq(errors.length, 1);
  assertEq(errors[0], 123);
  assertEq(err.errors[0], 123);

  // The property is writable.
  err.errors = undefined;
  assertEq(err.errors, undefined);
}

// The errors argument can be any iterable.
{
  function* g() { yield* [1, 2, 3]; }

  let {errors} = new AggregateError(g());
  assertEqArray(errors, [1, 2, 3]);
}

// The message property is populated by the second argument.
{
  let err;

  err = new AggregateError([]);
  assertEq(err.message, "");

  err = new AggregateError([], "my message");
  assertEq(err.message, "my message");
}

{
  assertEq("errors" in AggregateError.prototype, false);

  const {
    configurable,
    enumerable,
    value,
    writable
  } = Object.getOwnPropertyDescriptor(new AggregateError([]), "errors");
  assertEq(configurable, true);
  assertEq(enumerable, false);
  assertEq(writable, true);
  assertEq(value.length, 0);

  const g = newGlobal();

  let obj = {};
  let errors = new g.AggregateError([obj]).errors;

  assertEq(errors.length, 1);
  assertEq(errors[0], obj);

  // The prototype is |g.Array.prototype| in the cross-compartment case.
  let proto = Object.getPrototypeOf(errors);
  assertEq(proto === g.Array.prototype, true);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
