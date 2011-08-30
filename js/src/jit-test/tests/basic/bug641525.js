
var o2 = Proxy.create({});
function f1() {}
function f2() {}
function f4(o) {
    var key = Object.getOwnPropertyNames(o)[18];
    o4 = o[key];
    o.prototype = {};
}
f4(f1);
f4(f1);
f4(f2);
new f2(o2);

// these will hold only if type inference is enabled.
//assertEq(shapeOf(f1) == shapeOf(f2), false);
//assertEq(shapeOf(f1) == shapeOf(f4), false);

function factory() {
  function foo() {}
  foo.x = 0;
  return foo;
}

var fobjs = [];
for (var i = 0; i < 10; i++) {
  var of = fobjs[i] = factory();
  if (i > 0) {
    assertEq(fobjs[i - 1] === of, false);
    assertEq(shapeOf(fobjs[i - 1]), shapeOf(of));
  }
}

assertEq(shapeOf(fobjs[0]) == shapeOf(f1), false);
