if (helperThreadCount() == 0)
    quit();

offThreadCompileScript("\
    (function(stdlib, foreign) {\
        \"use asm\";\
        function() {};\
    })();\
");
