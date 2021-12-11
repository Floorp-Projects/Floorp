Object.defineProperty(Object.prototype, "a", {
    set: function(a) { eval(""); return 123; }
});
var obj = {};
var obj2 = {x: 1};
for (var i = 0; i < 100; i++) {
    var res = (obj.a = obj2);
    res.x = 0xbeef;
}
