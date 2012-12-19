function f(v, value)
{
  var b = v !== undefined;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

f({}, true);
f({}, true);
f(null, true);
f(null, true);
f(undefined, false);
f(undefined, false);
f(objectEmulatingUndefined(), true);
f(objectEmulatingUndefined(), true);
f(Object.prototype, true);
f(Object.prototype, true);

function g(v, value)
{
  var b = v !== undefined;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

g({}, true);
g({}, true);

function h(v, value)
{
  var b = v !== undefined;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

h(objectEmulatingUndefined(), true);
h(objectEmulatingUndefined(), true);
