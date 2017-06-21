// Define constructor with a proxy as prototype
let hook = { get: function(target, name, receiver) { return receiver; } }
let Base = function() { }
Base.prototype = new Proxy(Base.prototype, hook);

class Derived extends Base {
    test() {
        // Check proxy receiver is |this|, rather than Base.[[Prototype]]
        assertEq(super.x, this);
    }

    test_elem() {
        // Check proxy receiver is |this|, rather than Base.[[Prototype]]
        assertEq(super[0], this);
    }
}

let d = new Derived();

for (let i = 0; i < 20; ++i) {
    d.test();
    d.test_elem();
}
