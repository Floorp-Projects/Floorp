// |jit-test| error: SyntaxError
f = (function(stdlib, foreign, heap) {
            "use asm";
            ({ "0"
               ()
               { eval }
