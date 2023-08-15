load(libdir + "asserts.js");

var target = {x: 5};
var returnValue = 42;
var handler = {
  get(t, p) {
    if (returnValue != 42) {
      gc(testGet, "shrinking");
    }
    return returnValue;
  }
};
var proxy = new Proxy(target, handler);

function testGet(p) {
  return p.x;
}

for (i = 0; i < 200; i++) {
  assertEq(testGet(proxy), returnValue);
}

Object.defineProperty(target, 'x', {
    value: 42,
    writable: false,
    configurable: false
});

assertEq(testGet(proxy), returnValue);
returnValue = 41;
assertThrowsInstanceOf(function () { testGet(proxy) }, TypeError);
