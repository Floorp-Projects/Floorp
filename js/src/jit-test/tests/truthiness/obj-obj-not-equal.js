function f(v1, v2, value)
{
  var b = v1 != v2;
  assertEq(b, value,
           "failed: " + v1 + ", " + v2 + ": " + value);
}

var obj = {};
var emul = objectEmulatingUndefined();

f(obj, obj, false);
f(obj, obj, false);
f(emul, obj, true);
f(emul, obj, true);
f(obj, emul, true);
f(obj, emul, true);
f(Object.prototype, obj, true);
f(Object.prototype, obj, true);
f(emul, emul, false);
f(objectEmulatingUndefined(), emul, true);
f(objectEmulatingUndefined(), emul, true);
f(emul, objectEmulatingUndefined(), true);
f(emul, objectEmulatingUndefined(), true);
