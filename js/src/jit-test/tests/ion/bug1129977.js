gczeal(11);
function C(a, b) {
    this.b=b;
}
evaluate('\
Object.defineProperty(Object.prototype, "b", {set: function() {}});\
var f = C.bind(0x2004, 2);\
');
for (var i=1; i<5000; ++i)
    new f;
