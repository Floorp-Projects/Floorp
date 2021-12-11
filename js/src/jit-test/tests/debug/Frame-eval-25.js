// Make sure we can recover missing arguments even when it gets assigned to
// another slot.

load(libdir + "asserts.js");
load(libdir + "evalInFrame.js");

function h() {
  evalInFrame(1, "a.push(0)");
}

function f() {
  var a = arguments;
  h();
}

assertThrowsInstanceOf(f, TypeError);

function g() {
  {
    let a = arguments;
    h();
  }
}

assertThrowsInstanceOf(g, TypeError);
