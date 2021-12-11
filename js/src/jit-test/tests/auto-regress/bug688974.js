// Binary: cache/js-dbg-64-fecae145d884-linux
// Flags: -m -n
//

gczeal(2);
Object.defineProperty(Object.prototype, "b", {set: function() {}});
function C(a, b) {
    this.a=a;
    this.b=b;
}
var f = C.bind(0x2004, 2);
for (var i=0; i<10000; ++i)
    new f;
