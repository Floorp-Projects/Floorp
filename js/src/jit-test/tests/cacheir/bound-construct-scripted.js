function testNewTargetGuard() {
  var weirdNewTarget = function() {};
  var fun = function() { return new.target; };
  var boundFun = fun.bind(null);
  for (var i = 0; i < 60; i++) {
    var newTarget = i < 40 ? boundFun : weirdNewTarget;
    var res = Reflect.construct(boundFun, [], newTarget);
    assertEq(res, i < 40 ? fun : weirdNewTarget);
  }
}
testNewTargetGuard();

function testPrototypeGuard() {
  var fun = function() {};
  var boundFun = fun.bind(null);
  var customPrototype1 = {};
  var customPrototype2 = {};
  fun.prototype = customPrototype1;

  for (var i = 0; i < 60; i++) {
    if (i === 40) {
      fun.prototype = customPrototype2;
    }
    var res = new boundFun();
    assertEq(Object.getPrototypeOf(res), i < 40 ? customPrototype1 : customPrototype2);
  }
}
testPrototypeGuard();

function testNonObjectPrototypeGuard() {
  var fun = function() {};
  var boundFun = fun.bind(null);
  fun.prototype = null;
  var customPrototype = {};

  for (var i = 0; i < 60; i++) {
    if (i === 40) {
      fun.prototype = customPrototype;
    }
    var res = new boundFun();
    assertEq(Object.getPrototypeOf(res), i < 40 ? Object.prototype : customPrototype);
  }
}
testNonObjectPrototypeGuard();

function testObjectReturnValue() {
  var fun = function() { return Math; };
  var boundFun = fun.bind(null);
  for (var i = 0; i < 60; i++) {
    var res = new boundFun();
    assertEq(res, Math);
  }
}
testObjectReturnValue();

function testManyArgs() {
  var fun = function(a, b, c, d, e, f, g, h, i, j) {
    this.values = [a, b, c, d, e, f, g, h, i, j].join(",");
  };
  var boundFun1 = fun.bind(null, 1, 2);
  var boundFun2 = fun.bind(null, 1, 2, 3, 4, 5, 6);
  for (var i = 0; i < 60; i++) {
    assertEq(new boundFun1().values, "1,2,,,,,,,,");
    assertEq(new boundFun1(10, 11, 12, 13, 14).values, "1,2,10,11,12,13,14,,,");
    assertEq(new boundFun1(10, 11, 12, 13, 14, 15, 16, 17).values, "1,2,10,11,12,13,14,15,16,17");

    assertEq(new boundFun2().values, "1,2,3,4,5,6,,,,");
    assertEq(new boundFun2(10, 11).values, "1,2,3,4,5,6,10,11,,");
    assertEq(new boundFun2(10, 11, 12, 13, 14, 15, 16, 17).values, "1,2,3,4,5,6,10,11,12,13");
  }
}
testManyArgs();
