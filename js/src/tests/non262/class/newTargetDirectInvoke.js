// new.target is valid inside Function() invocations
var func = new Function("new.target");

// Note that this will also test new.target in ion inlines. When the toplevel
// script is compiled, assertNewTarget will be inlined.
function assertNewTarget(expected, unused) { assertEq(new.target, expected); }

// Test non-constructing invocations, with arg underflow, overflow, and correct
// numbers
for (let i = 0; i < 100; i++)
    assertNewTarget(undefined, null);

for (let i = 0; i < 100; i++)
    assertNewTarget(undefined);

for (let i = 0; i < 100; i++)
    assertNewTarget(undefined, null, 1);

// Test spread-call
for (let i = 0; i < 100; i++)
    assertNewTarget(...[undefined]);

for (let i = 0; i < 100; i++)
    assertNewTarget(...[undefined, null]);

for (let i = 0; i < 100; i++)
    assertNewTarget(...[undefined, null, 1]);

// Test constructing invocations, again with under and overflow
for (let i = 0; i < 100; i++)
    new assertNewTarget(assertNewTarget, null);

for (let i = 0; i < 100; i++)
    new assertNewTarget(assertNewTarget);

for (let i = 0; i < 100; i++)
    new assertNewTarget(assertNewTarget, null, 1);

// Test spreadnew as well.
for (let i = 0; i < 100; i++)
    new assertNewTarget(...[assertNewTarget]);

for (let i = 0; i < 100; i++)
    new assertNewTarget(...[assertNewTarget, null]);

for (let i = 0; i < 100; i++)
    new assertNewTarget(...[assertNewTarget, null, 1]);

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
