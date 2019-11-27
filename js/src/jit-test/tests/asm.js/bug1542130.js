// |jit-test| error:AsmJS modules do not yet support cloning; skip-if: !isAsmJSCompilationAvailable()
var g = newGlobal();
cloneAndExecuteScript(`
  function h() {
    function f() {
      'use asm';
      function g() {}
      return g
    }
    return f;
  }
`, g);
