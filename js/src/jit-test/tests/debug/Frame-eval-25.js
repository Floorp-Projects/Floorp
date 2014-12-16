// |jit-test| error: TypeError
//
// Make sure we can recover missing arguments even when it gets assigned to
// another slot.

load(libdir + "evalInFrame.js");

function h() {
  evalInFrame(1, "a.push(0)");
}

function f() {
  var a = arguments;
  h();
}

f();
