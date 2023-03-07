class Base {}
class Derived extends Base {
    constructor(a) { super(a); this.a = a; }
}
function test() {
    var boundCtor = Derived.bind();
    for (var i = 0; i < 40; i++) {
        new boundCtor();
        var ex = null;
        try {
            boundCtor();
        } catch(e) {
            ex = e;
        }
        assertEq(ex instanceof TypeError, true);
    }
}
test();
