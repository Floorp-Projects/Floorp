// First, let's test the trivial. A chain of three works.
class base {
    constructor() { }
    testChain() {
        this.baseCalled = true;
    }
}

class middle extends base {
    constructor() { super(); }
    testChain() {
        this.middleCalled = true;
        super.testChain();
    }
}

class derived extends middle {
    constructor() { super(); }
    testChain() {
        super.testChain();
        assertEq(this.middleCalled, true);
        assertEq(this.baseCalled, true);
    }
}

new derived().testChain();

// Super even chains in a wellbehaved fashion with normal functions.
function bootlegMiddle() { }
bootlegMiddle.prototype = middle.prototype;

new class extends bootlegMiddle {
        constructor() { super(); }
        testChain() {
            super.testChain();
            assertEq(this.middleCalled, true);
            assertEq(this.baseCalled, true);
        }
    }().testChain();

// Now let's try out some "long" chains
base.prototype.x = "yeehaw";

let chain = class extends base { constructor() { super(); } }

const CHAIN_LENGTH = 100;
for (let i = 0; i < CHAIN_LENGTH; i++)
    chain = class extends chain { constructor() { super(); } }

// Now we poke the chain
let inst = new chain();
inst.testChain();
assertEq(inst.baseCalled, true);

assertEq(inst.x, "yeehaw");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
