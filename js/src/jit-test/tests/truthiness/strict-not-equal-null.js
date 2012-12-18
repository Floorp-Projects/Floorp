function f(v, value)
{
  var b = v !== null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

f({}, true);
f({}, true);
f(null, false);
f(null, false);
f(undefined, true);
f(undefined, true);
f(objectEmulatingUndefined(), true);
f(objectEmulatingUndefined(), true);
f(Object.prototype, true);
f(Object.prototype, true);

function g(v, value)
{
  var b = v !== null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

g({}, true);
g({}, true);

function h(v, value)
{
  var b = v !== null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

h(objectEmulatingUndefined(), true);
h(objectEmulatingUndefined(), true);
