// Just like newTargetDirectInvoke, except to prove it works in functions
// defined with method syntax as well. Note that methods, getters, and setters
// are not constructible.

let ol = {
    olTest(arg) { assertEq(arg, 4); assertEq(new.target, undefined); },
    get ol() { assertEq(new.target, undefined); },
    set ol(arg) { assertEq(arg, 4); assertEq(new.target, undefined); }
}

class cl {
    constructor() { assertEq(new.target, cl); }
    clTest(arg) { assertEq(arg, 4); assertEq(new.target, undefined); }
    get cl() { assertEq(new.target, undefined); }
    set cl(arg) { assertEq(arg, 4); assertEq(new.target, undefined); }

    static staticclTest(arg) { assertEq(arg, 4); assertEq(new.target, undefined); }
    static get staticcl() { assertEq(new.target, undefined); }
    static set staticcl(arg) { assertEq(arg, 4); assertEq(new.target, undefined); }
}

const TEST_ITERATIONS = 150;

for (let i = 0; i < TEST_ITERATIONS; i++)
    ol.olTest(4);
for (let i = 0; i < TEST_ITERATIONS; i++)
    ol.ol;
for (let i = 0; i < TEST_ITERATIONS; i++)
    ol.ol = 4;

for (let i = 0; i < TEST_ITERATIONS; i++)
    cl.staticclTest(4);
for (let i = 0; i < TEST_ITERATIONS; i++)
    cl.staticcl;
for (let i = 0; i < TEST_ITERATIONS; i++)
    cl.staticcl = 4;

for (let i = 0; i < TEST_ITERATIONS; i++)
    new cl();

let clInst = new cl();

for (let i = 0; i < TEST_ITERATIONS; i++)
    clInst.clTest(4);
for (let i = 0; i < TEST_ITERATIONS; i++)
    clInst.cl;
for (let i = 0; i < TEST_ITERATIONS; i++)
    clInst.cl = 4;


if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
