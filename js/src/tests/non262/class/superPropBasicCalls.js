// Super property (and calls) works in non-extending classes and object
// litterals.
class toStringTest {
    constructor() {
        // Install a property to make it plausible that it's the same this
        this.foo = "rhinoceros";
    }

    test() {
        assertEq(super.toString(), super["toString"]());
        assertEq(super.toString(), this.toString());
    }
}

new toStringTest().test();

let toStrOL = {
    test() {
        assertEq(super.toString(), super["toString"]());
        assertEq(super.toString(), this.toString());
    }
}

toStrOL.test();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
