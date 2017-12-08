"use strict"

var log = "";

function f() {
  return "f0";
}

log += f();

{
  log += f();

  function f() {
    return "f1";
  }

  log += f();
}

log += f();

function g() {
  function h() {
    return "h0";
  }

  log += h();

  {
    log += h();

    function h() {
      return "h1";
    }

    log += h();
  }

  log += h();
}

g();

reportCompare(log, "f0f1f1f0h0h1h1h0");
