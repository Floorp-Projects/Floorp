// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'destructure-accessor.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 536472;
var summary =
  'ES5: { get x(v) { } } and { set x(v, v2) { } } should be syntax errors';

print(BUGNUMBER + ": " + summary);

//-----------------------------------------------------------------------------

function expectOk(s)
{
  try
  {
    eval(s);
    return;
  }
  catch (e)
  {
    assertEq(true, false,
             "expected no error parsing '" + "', got : " + e);
  }
}

function expectSyntaxError(s)
{
  try
  {
    eval(s);
    throw new Error("no error thrown");
  }
  catch (e)
  {
    assertEq(e instanceof SyntaxError, true,
             "expected syntax error parsing '" + s + "', got: " + e);
  }
}

expectSyntaxError("({ get x([]) { } })");
expectSyntaxError("({ get x({}) { } })");
expectSyntaxError("({ get x(a, []) { } })");
expectSyntaxError("({ get x(a, {}) { } })");
expectSyntaxError("({ get x([], a) { } })");
expectSyntaxError("({ get x({}, a) { } })");
expectSyntaxError("({ get x([], a, []) { } })");
expectSyntaxError("({ get x([], a, {}) { } })");
expectSyntaxError("({ get x({}, a, []) { } })");
expectSyntaxError("({ get x({}, a, {}) { } })");

expectOk("({ get x() { } })");


expectSyntaxError("({ set x() { } })");
expectSyntaxError("({ set x(a, []) { } })");
expectSyntaxError("({ set x(a, b, c) { } })");

expectOk("({ set x([]) { } })");
expectOk("({ set x({}) { } })");
expectOk("({ set x([a]) { } })");
expectOk("({ set x([a, b]) { } })");
expectOk("({ set x([a,]) { } })");
expectOk("({ set x([a, b,]) { } })");
expectOk("({ set x([, b]) { } })");
expectOk("({ set x([, b,]) { } })");
expectOk("({ set x([, b, c]) { } })");
expectOk("({ set x([, b, c,]) { } })");
expectOk("({ set x({ a: a }) { } })");
expectOk("({ set x({ a: a, b: b }) { } })");

//-----------------------------------------------------------------------------

reportCompare(true, true);
