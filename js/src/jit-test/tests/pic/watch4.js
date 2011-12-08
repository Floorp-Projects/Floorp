// adding assignment + watchpoint vs. caching
var hits = 0;
var obj = {};
obj.watch("x", function (id, oldval, newval) { hits++; return newval; });
for (var i = 0; i < 10; i++) {
    obj.x = 1;
    delete obj.x;
}
assertEq(hits, 10);
