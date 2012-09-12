Object.defineProperty(Object.prototype, 'x', { 
    set: function() { evalcx('lazy'); } 
});
var obj = {};
var prot = {};
obj.__proto__ = prot;
obj.watch("x", function (id, oldval, newval) {});
for (var str in 'A') {
    obj.x = 1;
}
