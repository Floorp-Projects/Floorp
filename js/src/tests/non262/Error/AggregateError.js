// |reftest| skip-if(release_or_beta)

assertEq(typeof AggregateError, "function");
assertEq(Object.getPrototypeOf(AggregateError), Error);
assertEq(AggregateError.name, "AggregateError");
assertEq(AggregateError.length, 2);

assertEq(Object.getPrototypeOf(AggregateError.prototype), Error.prototype);
assertEq(AggregateError.prototype.name, "AggregateError");
assertEq(AggregateError.prototype.message, "");

// AggregateError.prototype isn't an AggregateError instance.
assertThrowsInstanceOf(() => AggregateError.prototype.errors, TypeError);

// The |errors| argument is mandatory.
assertThrowsInstanceOf(() => new AggregateError(), TypeError);
assertThrowsInstanceOf(() => AggregateError(), TypeError);

// The .errors getter returns an array object.
{
  let err = new AggregateError([]);

  let {errors} = err;
  assertEq(Array.isArray(errors), true);
  assertEq(errors.length, 0);

  // A fresh object is returned each time calling the getter.
  assertEq(errors === err.errors, false);

  // The errors object is modifiable.
  errors.push(123);
  assertEq(errors.length, 1);
  assertEq(errors[0], 123);
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
  const {
    get: getErrors,
    set: setErrors,
  } = Object.getOwnPropertyDescriptor(AggregateError.prototype, "errors");
  assertEq(typeof getErrors, "function");
  assertEq(typeof setErrors, "undefined");

  // The |this| argument must be an AggregateError instance.
  assertThrowsInstanceOf(() => getErrors.call(null), TypeError);
  assertThrowsInstanceOf(() => getErrors.call({}), TypeError);
  assertThrowsInstanceOf(() => getErrors.call(new Error), TypeError);

  const g = newGlobal();

  let obj = {};
  let errors = getErrors.call(new g.AggregateError([obj]));

  assertEq(errors.length, 1);
  assertEq(errors[0], obj);

  // The prototype is (incorrectly) |g.Array.prototype| in the cross-compartment case.
  let proto = Object.getPrototypeOf(errors);
  assertEq(proto === Array.prototype || proto === g.Array.prototype, true);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
