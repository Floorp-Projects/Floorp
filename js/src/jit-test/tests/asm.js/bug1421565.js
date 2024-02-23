// |jit-test| --ion-offthread-compile=off; skip-if: !isAsmJSCompilationAvailable()

load(libdir + "asm.js");

oomTest(
  function() {
    eval(`
      function f(stdlib, foreign, buffer) {
        "use asm";
         var i32 = new stdlib.Int32Array(buffer);
         function set(v) {
           v=v|0;
           i32[5] = v;
         }
        return set;
      }
    `);
  }
);
