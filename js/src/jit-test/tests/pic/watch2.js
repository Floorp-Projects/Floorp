// assignments to watched properties via ++ must not be cached
var obj = {x: 0};
var hits = 0;
obj.watch("x", function (id, oldval, newval) { hits++; return newval; });
for (var i = 0; i < 10; i++)
    obj.x++;
assertEq(hits, 10);

