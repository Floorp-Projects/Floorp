var target = {x: 5};
var returnValue = 42;
var handlerProto = {};
var handler = {};
handlerProto.get = function(t, p) {
  return returnValue;
}
handler.foo = handlerProto.get;
handler.__proto__ = handlerProto;

var proxy = new Proxy(target, handler);

function testGet(p) {
  return p.x;
}

for (i = 0; i < 500; i++) {
  assertEq(testGet(proxy), returnValue);
}

handlerProto.get = function() {
  return returnValue - 1;
}

assertEq(testGet(proxy), returnValue - 1);
