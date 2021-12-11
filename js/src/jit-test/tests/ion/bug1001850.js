function t1() {
  assertEq(thisValue, this);
}

thisValue = {};
var f1 = t1.bind(thisValue);
f1()
f1()

////////////////////////////////////////////////////////////

function t2() {
  bailout();
}

var f2 = t2.bind(thisValue);
f2()
f2()

////////////////////////////////////////////////////////////

function test3() {
  function i3(a,b,c,d) {
    bailout();
  }

  function t3(a,b,c,d) {
    i3(a,b,c,d);
  }

  var f3 = t3.bind(thisValue);
  for (var i=0;i<10; i++) {
    f3(1,2,3,4)
    f3(1,2,3,4)
  }
}
test3();
test3();

////////////////////////////////////////////////////////////

function test4() {
  this.a = 1;
  var inner = function(a,b,c,d) {
    bailout();
  }

  var t = function(a,b,c,d) {
    assertEq(this.a, undefined);
    inner(a,b,c,d);
    assertEq(this.a, undefined);
  }

  var f = t.bind(thisValue);
  for (var i=0;i<5; i++) {
    var res = f(1,2,3,4)
    var res2 = new f(1,2,3,4)
    assertEq(res, undefined);
    assertEq(res2 == undefined, false);
  }
}
test4();
test4();

////////////////////////////////////////////////////////////

function test5() {
  this.a = 1;
  var inner = function(a,b,c,d) {
    assertEq(a, 1);
    assertEq(b, 2);
    assertEq(c, 3);
    assertEq(d, 1);
    bailout();
    assertEq(a, 1);
    assertEq(b, 2);
    assertEq(c, 3);
    assertEq(d, 1);
  }

  var t = function(a,b,c,d) {
    inner(a,b,c,d);
  }

  var f = t.bind(thisValue, 1,2,3);
  for (var i=0;i<5; i++) {
    f(1,2,3,4)
  }
}
test5();
test5();

////////////////////////////////////////////////////////////

function test6() {
  function i6(a,b,c,d) {
    if (a == 1)
      bailout();
  }

  function t6(a,b,c,d) {
    i6(a,b,c,d);
  }

  var f6 = t6.bind(thisValue, 1);
  f6(1,2,3,4)
  f6(0,2,3,4)
}
test6();
test6();
