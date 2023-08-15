load(libdir + "asserts.js");

var target = {x:5};
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
    set(v) {},
    configurable: false
});

assertThrowsInstanceOf(function () { testGet(proxy) }, TypeError);
