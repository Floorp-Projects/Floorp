// |jit-test| skip-if: helperThreadCount() === 0

// Test multiple concurrent off-thread parse jobs.

function assertFails(f) {
    let failed = false;
    try {
        f();
    } catch (e) {
        failed = true;
    }
    assertEq(failed, true);
}

function encodeScript(source)
{
    let entry = cacheEntry(source);
    let global = newGlobal({ cloneSingletons: true });
    evaluate(entry, { global: global, saveIncrementalBytecode: true });
    return entry;
}

let a, b, c;
let stencil, stencilA, stencilB, stencilC;

// Calling run functions without arguments assumes a single off-thread job.

// Test run functions fail when no jobs exist.

assertFails(() => finishOffThreadCompileToStencil());

assertFails(() => finishOffThreadCompileModuleToStencil());

assertFails(() => finishOffThreadDecodeStencil());

// Test run functions fail when multiple jobs exist and no ID specified.

a = offThreadCompileToStencil("");
b = offThreadCompileToStencil("");
assertFails(() => finishOffThreadCompileToStencil());
stencilA = finishOffThreadCompileToStencil(a);
stencilB = finishOffThreadCompileToStencil(b);
evalStencil(stencilA);
evalStencil(stencilB);

a = offThreadCompileModuleToStencil("");
b = offThreadCompileModuleToStencil("");
assertFails(() => finishOffThreadCompileModuleToStencil());
stencilA = finishOffThreadCompileModuleToStencil(a);
stencilB = finishOffThreadCompileModuleToStencil(b);
instantiateModuleStencil(stencilA);
instantiateModuleStencil(stencilB);

a = offThreadDecodeStencil(encodeScript(""));
b = offThreadDecodeStencil(encodeScript(""));
assertFails(() => finishOffThreadCompileToStencil());
stencilA = finishOffThreadDecodeStencil(a);
stencilB = finishOffThreadDecodeStencil(b);
evalStencil(stencilA);
evalStencil(stencilB);

// Test fun functions succeed when a single job exist and no ID specified.

offThreadCompileToStencil("42");
stencil = finishOffThreadCompileToStencil();
assertEq(evalStencil(stencil), 42);

offThreadCompileModuleToStencil("");
stencil = finishOffThreadCompileModuleToStencil();
assertEq(typeof instantiateModuleStencil(stencil), "object");

offThreadDecodeStencil(encodeScript("23"));
stencil = finishOffThreadDecodeStencil();
assertEq(evalStencil(stencil), 23);

// Run functions take an ID argument returned from the compile function.

// Test bad ID type and unknown ID.

offThreadCompileToStencil("");
assertFails(() => finishOffThreadCompileToStencil("foo"));
assertFails(() => finishOffThreadCompileToStencil(42));
stencil = finishOffThreadCompileToStencil();
evalStencil(stencil);

offThreadCompileModuleToStencil("");
assertFails(() => finishOffThreadCompileModuleToStencil("foo"));
assertFails(() => finishOffThreadCompileModuleToStencil(42));
stencil = finishOffThreadCompileModuleToStencil();
instantiateModuleStencil(stencil);

offThreadDecodeStencil(encodeScript(""));
assertFails(() => finishOffThreadDecodeStencil("foo"));
assertFails(() => finishOffThreadDecodeStencil(42));
stencil = finishOffThreadDecodeStencil();
evalStencil(stencil);

// Test stale ID.

a = offThreadCompileToStencil("");
stencilA = finishOffThreadCompileToStencil(a);
evalStencil(stencilA);
assertFails(() => finishOffThreadCompileToStencil(a));

a = offThreadCompileModuleToStencil("");
stencilA = finishOffThreadCompileModuleToStencil(a);
assertFails(() => finishOffThreadCompileModuleToStencil(a));
instantiateModuleStencil(stencilA);

a = offThreadDecodeStencil(encodeScript(""));
stencilA = finishOffThreadDecodeStencil(a);
evalStencil(stencilA);
assertFails(() => finishOffThreadDecodeStencil(a));

// Test wrong job kind.

a = offThreadCompileToStencil("");
b = offThreadCompileModuleToStencil("");
c = offThreadDecodeStencil(encodeScript(""));
assertFails(() => finishOffThreadCompileToStencil(b));
assertFails(() => finishOffThreadCompileToStencil(c));
assertFails(() => finishOffThreadCompileModuleToStencil(a));
assertFails(() => finishOffThreadCompileModuleToStencil(c));
assertFails(() => finishOffThreadDecodeStencil(a));
assertFails(() => finishOffThreadDecodeStencil(b));
stencilA = finishOffThreadCompileToStencil(a);
evalStencil(stencilA);
stencilB = finishOffThreadCompileModuleToStencil(b);
instantiateModuleStencil(stencilB);
stencilC = finishOffThreadDecodeStencil(c);
evalStencil(stencilC);

// Test running multiple jobs.

a = offThreadCompileToStencil("1");
b = offThreadCompileToStencil("2");
stencilA = finishOffThreadCompileToStencil(a);
stencilB = finishOffThreadCompileToStencil(b);
assertEq(evalStencil(stencilA), 1);
assertEq(evalStencil(stencilB), 2);

a = offThreadCompileModuleToStencil("");
b = offThreadCompileModuleToStencil("");
stencilA = finishOffThreadCompileModuleToStencil(a);
stencilB = finishOffThreadCompileModuleToStencil(b);
assertEq(typeof instantiateModuleStencil(stencilA), "object");
assertEq(typeof instantiateModuleStencil(stencilB), "object");

a = offThreadDecodeStencil(encodeScript("3"));
b = offThreadDecodeStencil(encodeScript("4"));
stencilA = finishOffThreadDecodeStencil(a);
stencilB = finishOffThreadDecodeStencil(b);
assertEq(evalStencil(stencilA), 3);
assertEq(evalStencil(stencilB), 4);

// Test many jobs.

const count = 100;
let jobs;

jobs = new Array(count);
for (let i = 0; i < jobs.length; i++)
    jobs[i] = offThreadCompileToStencil(`${i} * ${i}`);
for (let i = 0; i < jobs.length; i++) {
    stencil = finishOffThreadCompileToStencil(jobs[i]);
    assertEq(evalStencil(stencil), i * i);
}

jobs = new Array(count);
for (let i = 0; i < jobs.length; i++)
    jobs[i] = offThreadCompileModuleToStencil("");
for (let i = 0; i < jobs.length; i++) {
    stencil = finishOffThreadCompileModuleToStencil(jobs[i]);
    assertEq(typeof instantiateModuleStencil(stencil), "object");
}

jobs = new Array(count);
for (let i = 0; i < jobs.length; i++)
    jobs[i] = offThreadDecodeStencil(encodeScript(`${i} * ${i}`));
for (let i = 0; i < jobs.length; i++) {
    stencil = finishOffThreadDecodeStencil(jobs[i]);
    assertEq(evalStencil(stencil), i * i);
}
