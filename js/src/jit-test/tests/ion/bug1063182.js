// |jit-test| error: ReferenceError

eval("(function() { " + "\
var o = {};\
o.watch('p', function() { });\
for (var i = 0; i < 10; \u5ede ++)\
    o.p = 123;\
" + " })();");
