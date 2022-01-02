new class extends class { } {
    constructor() {
        let a1 = () => this;
        let a2 = (() => super());
        assertThrowsInstanceOf(a1, ReferenceError);
        assertEq(a2(), a1());
    }
}();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
