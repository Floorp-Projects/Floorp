load(libdir + "asserts.js");

var target = {x:5};
var magic = false;
var returnValue = 42;
var handler = {
  get(t, p, a, b) {
    assertEq(a, proxy);
    assertEq(b, undefined);
    assertEq(arguments.length, 3);
    if (magic) {
      Object.defineProperty(target, 'x', {
          value: 42,
          writable: false,
          configurable: false
      });
      if (returnValue != 42) {
        gc(testGet, "shrinking");
      }
    }
    return returnValue;
  }
};
var proxy = new Proxy(target, handler);

// Use an array that sometimes has a proxy and sometimes not, to get to
// the Ion IC path.
var maybeProxies = [
  {x: returnValue},
  proxy,
];

function testGet(p) {
  return p.x;
}

for (i = 0; i < 200; i++) {
  assertEq(testGet(maybeProxies[i % maybeProxies.length]), returnValue);
}
magic = true;
assertEq(testGet(proxy), returnValue);
returnValue = 41;
assertThrowsInstanceOf(function () { testGet(proxy) }, TypeError);
