function f(v, value)
{
  var b = v == null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

f({}, false);
f({}, false);
f(null, true);
f(null, true);
f(undefined, true);
f(undefined, true);
f(createIsHTMLDDA(), true);
f(createIsHTMLDDA(), true);
f(Object.prototype, false);
f(Object.prototype, false);

function g(v, value)
{
  var b = v == null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

g({}, false);
g({}, false);

function h(v, value)
{
  var b = v == null;
  assertEq(b, value,
           "failed: " + v + " " + value);
}

h(createIsHTMLDDA(), true);
h(createIsHTMLDDA(), true);
