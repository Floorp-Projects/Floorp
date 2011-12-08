// assignment to watched global properties must not be cached
x = 0;
var hits = 0;
this.watch("x", function (id, oldval, newval) { hits++; return newval; });
for (var i = 0; i < 10; i++)
    x = i;
assertEq(hits, 10);
