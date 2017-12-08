class base {
    constructor(a, b, c) {
        assertEq(a, 1);
        assertEq(b, 2);
        assertEq(c, 3);
        this.calledBase = true;
    }
}

class doTest extends base {
    constructor(arr) {
        super(...arr);
    }
}

assertEq(new doTest([1,2,3]).calledBase, true);

class testRest extends base {
   constructor(...args) {
       super(...args);
   }
}

assertEq(new testRest(1,2,3).calledBase, true);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
