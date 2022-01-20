// |jit-test| skip-if: helperThreadCount() === 0

offThreadCompileToStencil("\
    (function(stdlib, foreign) {\
        \"use asm\";\
        function() {};\
    })();\
");
