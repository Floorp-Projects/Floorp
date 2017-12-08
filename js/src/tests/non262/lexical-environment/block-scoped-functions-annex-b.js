var log = "";

log += typeof f;

{
  log += f();

  function f() {
    return "f1";
  }
}

log += f();

function g() {
  log += typeof h;

  {
    log += h();

    function h() {
      return "h1";
    }
  }

  log += h();
}

g();

reportCompare(log, "undefinedf1f1undefinedh1h1");
