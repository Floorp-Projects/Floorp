// assignments to watched objects must not be cached
var obj = {x: 0};
var hits = 0;
obj.watch("x", function (id, oldval, newval) { hits++; return newval; });
for (var i = 0; i < HOTLOOP + 2; i++)
    obj.x = i;
assertEq(hits, HOTLOOP + 2);
