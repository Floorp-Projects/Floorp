// |jit-test| error:AsmJS modules do not yet support cloning; skip-if: !isAsmJSCompilationAvailable()
var g = newGlobal();
g.evaluate(`
  function h() {
    function f() {
      'use asm';
      function g() {}
      return g
    }
    return f;
  }
`);
var h = clone(g.h);
h();
