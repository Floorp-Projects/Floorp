Object.defineProperty(Object.prototype, 'x', { 
    set: function() { evalcx('lazy'); } 
});
var obj = {};
obj.watch("x", function (id, oldval, newval) {});
for (var str in 'A') {
    obj.x = 1;
}
