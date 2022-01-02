load(libdir + "asserts.js");

assertThrowsInstanceOf(() => {
        class foo extends null {
            constructor(a = class bar extends bar {}) {}
        }
        new foo();
    },
    ReferenceError
)

class B { }
class C extends B {
    constructor(a = class D { [super()] = 5; }) {
    }
}
new C()
