// |jit-test| error: ReferenceError;
gczeal(4);
try { jsTestDriverEnd(); } catch(exc1) {}
evaluate("\
schedulegc(10);\
for(var i=0; i<3; i++) {\
    var obj = { first: 'first', second: 'second' };\
    for (var elem in obj) {}\
    x.push(count);\
}\
");
