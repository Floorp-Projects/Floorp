function f(v1, v2, value)
{
  var b = v1 == v2;
  assertEq(b, value,
           "failed: " + v1 + ", " + v2 + ": " + value);
}

var obj = {};
var emul = createIsHTMLDDA();

f(obj, obj, true);
f(obj, obj, true);
f(emul, obj, false);
f(emul, obj, false);
f(obj, emul, false);
f(obj, emul, false);
f(Object.prototype, obj, false);
f(Object.prototype, obj, false);
f(emul, emul, true);
f(createIsHTMLDDA(), emul, false);
f(createIsHTMLDDA(), emul, false);
f(emul, createIsHTMLDDA(), false);
f(emul, createIsHTMLDDA(), false);
