var log = "";

function f() {
  log += g();
  function g() { return "outer-g"; }

  var o = { g: function () { return "with-g"; } };
  with (o) {
    // Annex B.3.3.3 says g should be set on the nearest VariableEnvironment,
    // and so should not change o.g.
    eval(`{
      function g() { return "eval-g"; }
    }`);
  }

  log += g();
  log += o.g();
}

f();

function h() {
  eval(`
    // Should return true, as var bindings introduced by eval are configurable.
    log += (delete q);
    {
      function q() { log += "q"; }
      // Should return false, as lexical bindings introduced by eval are not
      // configurable.
      log += (delete q);
    }
  `);
  return q;
}

h()();

reportCompare(log, "outer-geval-gwith-gtruefalseq");
