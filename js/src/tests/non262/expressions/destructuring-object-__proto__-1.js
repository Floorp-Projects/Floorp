// Test __proto__ is destructuring binding initialization.

// __proto__ shorthand, no default.
{
  let {__proto__} = {};
  assertEq(__proto__, Object.prototype);
}

// __proto__ shorthand, with default.
{
  let {__proto__ = 0} = {};
  assertEq(__proto__, Object.prototype);
}

{
  let {__proto__ = 0} = Object.create(null);
  assertEq(__proto__, 0);
}

// __proto__ keyed, no default.
{
  let {__proto__: p} = {};
  assertEq(p, Object.prototype);
}

// __proto__ keyed, with default.
{
  let {__proto__: p = 0} = {};
  assertEq(p, Object.prototype);
}

// __proto__ keyed, with default.
{
  let {__proto__: p = 0} = Object.create(null);
  assertEq(p, 0);
}

// Repeat the cases from above, but this time with a rest property.

// __proto__ shorthand, no default.
{
  let {__proto__, ...rest} = {};
  assertEq(__proto__, Object.prototype);
  assertEq(Reflect.ownKeys(rest).length, 0);
}

// __proto__ shorthand, with default.
{
  let {__proto__ = 0, ...rest} = {};
  assertEq(__proto__, Object.prototype);
  assertEq(Reflect.ownKeys(rest).length, 0);
}

{
  let {__proto__ = 0, ...rest} = Object.create(null);
  assertEq(__proto__, 0);
  assertEq(Reflect.ownKeys(rest).length, 0);
}

// __proto__ keyed, no default.
{
  let {__proto__: p, ...rest} = {};
  assertEq(p, Object.prototype);
  assertEq(Reflect.ownKeys(rest).length, 0);
}

// __proto__ keyed, with default.
{
  let {__proto__: p = 0, ...rest} = {};
  assertEq(p, Object.prototype);
  assertEq(Reflect.ownKeys(rest).length, 0);
}

// __proto__ keyed, with default.
{
  let {__proto__: p = 0, ...rest} = Object.create(null);
  assertEq(p, 0);
  assertEq(Reflect.ownKeys(rest).length, 0);
}

if (typeof reportCompare == "function")
  reportCompare(true, true);
