// __noSuchMethod__ is totally non-standard and evil, but in this one weird case
// below we don't actually use it.  So this test is bog-standard ES6, not
// SpiderMonkey-specific.
//
// In ES6:
//   Accessing 1[Symbol.iterator]() throws a TypeError calling |undefined|.
// In SpiderMonkey:
//   Accessing 1[Symbol.iterator]() does *not* invoke __noSuchMethod__ looked up
//   on 1 (or on an implicitly boxed 1), because 1 is a primitive value.
//   SpiderMonkey then does exactly the ES6 thing here and throws a TypeError
//   calling |undefined|.

Object.prototype.__noSuchMethod__ = {};

try
{
  var [x] = 1;
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "expected TypeError, got " + e);
}
