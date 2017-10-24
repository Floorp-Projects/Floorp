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
f(createIsHTMLDDA(), true);
f(createIsHTMLDDA(), true);
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

h(createIsHTMLDDA(), true);
h(createIsHTMLDDA(), true);
