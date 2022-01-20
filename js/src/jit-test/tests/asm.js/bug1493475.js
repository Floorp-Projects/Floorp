// |jit-test| skip-if: helperThreadCount() === 0

offThreadCompileScript("\
    (function(stdlib, foreign) {\
        \"use asm\";\
        function() {};\
    })();\
");
