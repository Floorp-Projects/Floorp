// Binary: cache/js-dbg-32-f1764bf06b29-linux
// Flags: --ion-eager
//

gcPreserveCode();
function r() {}
gczeal(2);
evaluate("");
evaluate("\
function randomFloat () {\
    if (r < 0.25)\
        fac = 10000000;\
}\
for (var i = 0; i < 100000; i++)\
    randomFloat();\
");
