function Obj() {
    this.a = 1;
}
Obj.prototype = this;

function test() {
    return o.a;
}
var o = new Obj();
test();
