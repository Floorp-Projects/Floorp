// |jit-test| --ion-offthread-compile=off

load(libdir + "asm.js");

if (!isAsmJSCompilationAvailable())
    quit(0);

if (!('oomTest' in this))
    quit();

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
