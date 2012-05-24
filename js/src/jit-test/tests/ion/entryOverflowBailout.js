var o;
var f1;
var counter = 0;
var bailout = Proxy.createFunction({}, Math.sin);

function f2(a) {
    bailout();
    return f2.arguments;
};

var restartCode = "counter = 0; " + f2.toSource();

// We need to reevaluate this function everytime, otherwise it is flagged as
// having an argument object and it would not be re-entered.

// This test case is checking that f.arguments reflects the overflow or the
// underflow of arguments after a bailout.  Due to the way bailouts are
// recovered we need to check for the intial frame and for any other JS frame
// below.
//
// To produce a JSFrame, we need to avoid the 'Hot' counters of f1 to be the
// same as f2, because IonMonkey will try to inline f2 in f1, and not
// compiling/calling f2 separately. This explain why we ignore the 5 first call
// of f1 by returning immediately.
//
// Bailouts are caused by calling an object function, which is expected to be a
// function by IonMonkey.  So bailout() cause a bailout in the currently
// compiled function.
//
// To avoid any preventive effect to re-enter f2, we re-evaluate it before every
// test.

// Check bailouts of the initial frame.

eval(restartCode);
while (counter++ < 50) {
    o = f2();
    assertEq(o.length, 0);
}

eval(restartCode);
while (counter++ < 50) {
    o = f2(21);
    assertEq(o.length, 1);
    assertEq(o[0], 21);
}

eval(restartCode);
while (counter++ < 50) {
    o = f2(21,42);
    assertEq(o.length, 2);
    assertEq(o[0], 21);
    assertEq(o[1], 42);
}

// 100 arguments.
eval(restartCode);
while (counter++ < 50) {
    o = f2(0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9);
    assertEq(o.length, 100);
    for (var i in o)
        assertEq(o[i], i % 10);
}

// 200 arguments.
eval(restartCode);
while (counter++ < 50) {
    o = f2(0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,

           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9);
    assertEq(o.length, 200);
    for (var i in o)
        assertEq(o[i], i % 10);
}

// 300 arguments.
eval(restartCode);
while (counter++ < 50) {
    o = f2(0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,

           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,

           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
           0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9);
    assertEq(o.length, 300);
    for (var i in o)
        assertEq(o[i], i % 10);
}

// Check bailouts of one frame which is not the initial frame.

eval(restartCode);
f1 = function() {
    if (counter < 5) return 0;
    return f2();
};
while (counter++ < 50) {
    o = f1();
    if (counter < 5) continue;
    assertEq(o.length, 0);
}

eval(restartCode);
f1 = function() {
    if (counter < 5) return 0;
    return f2(21);
};
while (counter++ < 50) {
    o = f1();
    if (counter < 5) continue;
    assertEq(o.length, 1);
    assertEq(o[0], 21);
}

eval(restartCode);
f1 = function() {
    if (counter < 5) return 0;
    return f2(21,42);
};
while (counter++ < 50) {
    o = f1();
    if (counter < 5) continue;
    assertEq(o.length, 2);
    assertEq(o[0], 21);
    assertEq(o[1], 42);
}

// 100 arguments.
eval(restartCode);
f1 = function() {
    if (counter < 5) return 0;
    return f2(0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9);
};
while (counter++ < 50) {
    o = f1();
    if (counter < 5) continue;
    assertEq(o.length, 100);
    for (var i in o)
        assertEq(o[i], i % 10);
}

// 200 arguments.
eval(restartCode);
f1 = function() {
    if (counter < 5) return 0;
    return f2(0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,

              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
              0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9);
};
while (counter++ < 50) {
    o = f1();
    if (counter < 5) continue;
    assertEq(o.length, 200);
    for (var i in o)
        assertEq(o[i], i % 10);
}

// 300 arguments.
eval(restartCode);
f1 = function() {
  if (counter < 5) return 0;
  return f2(0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,

            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,

            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
            0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9);
};
while (counter++ < 500) {
    o = f1();
    if (counter < 5) continue;
    assertEq(o.length, 300);
    for (var i in o)
        assertEq(o[i], i % 10);
}
