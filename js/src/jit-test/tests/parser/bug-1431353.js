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

assertFails(() => finishOffThreadModule());

assertFails(() => runOffThreadDecodedScript());

// Test run functions fail when multiple jobs exist and no ID specified.

a = offThreadCompileToStencil("");
b = offThreadCompileToStencil("");
assertFails(() => finishOffThreadCompileToStencil());
stencilA = finishOffThreadCompileToStencil(a);
stencilB = finishOffThreadCompileToStencil(b);
evalStencil(stencilA);
evalStencil(stencilB);

a = offThreadCompileModule("");
b = offThreadCompileModule("");
assertFails(() => finishOffThreadModule());
finishOffThreadModule(a);
finishOffThreadModule(b);

a = offThreadDecodeScript(encodeScript(""));
b = offThreadDecodeScript(encodeScript(""));
assertFails(() => finishOffThreadCompileToStencil());
runOffThreadDecodedScript(a);
runOffThreadDecodedScript(b);

// Test fun functions succeed when a single job exist and no ID specified.

offThreadCompileToStencil("42");
stencil = finishOffThreadCompileToStencil();
assertEq(evalStencil(stencil), 42);

offThreadCompileModule("");
assertEq(typeof finishOffThreadModule(), "object");

offThreadDecodeScript(encodeScript("23"));
assertEq(runOffThreadDecodedScript(), 23);

// Run functions take an ID argument returned from the compile function.

// Test bad ID type and unknown ID.

offThreadCompileToStencil("");
assertFails(() => finishOffThreadCompileToStencil("foo"));
assertFails(() => finishOffThreadCompileToStencil(42));
stencil = finishOffThreadCompileToStencil();
evalStencil(stencil);

offThreadCompileModule("");
assertFails(() => finishOffThreadModule("foo"));
assertFails(() => finishOffThreadModule(42));
finishOffThreadModule();

offThreadDecodeScript(encodeScript(""));
assertFails(() => runOffThreadDecodedScript("foo"));
assertFails(() => runOffThreadDecodedScript(42));
runOffThreadDecodedScript();

// Test stale ID.

a = offThreadCompileToStencil("");
stencilA = finishOffThreadCompileToStencil(a);
evalStencil(stencilA);
assertFails(() => finishOffThreadCompileToStencil(a));

a = offThreadCompileModule("");
finishOffThreadModule(a);
assertFails(() => finishOffThreadModule(a));

a = offThreadDecodeScript(encodeScript(""));
runOffThreadDecodedScript(a);
assertFails(() => runOffThreadDecodedScript(a));

// Test wrong job kind.

a = offThreadCompileToStencil("");
b = offThreadCompileModule("");
c = offThreadDecodeScript(encodeScript(""));
assertFails(() => finishOffThreadCompileToStencil(b));
assertFails(() => finishOffThreadCompileToStencil(c));
assertFails(() => finishOffThreadModule(a));
assertFails(() => finishOffThreadModule(c));
assertFails(() => runOffThreadDecodedScript(a));
assertFails(() => runOffThreadDecodedScript(b));
stencilA = finishOffThreadCompileToStencil(a);
evalStencil(stencilA);
finishOffThreadModule(b);
runOffThreadDecodedScript(c);

// Test running multiple jobs.

a = offThreadCompileToStencil("1");
b = offThreadCompileToStencil("2");
stencilA = finishOffThreadCompileToStencil(a);
stencilB = finishOffThreadCompileToStencil(b);
assertEq(evalStencil(stencilA), 1);
assertEq(evalStencil(stencilB), 2);

a = offThreadCompileModule("");
b = offThreadCompileModule("");
assertEq(typeof finishOffThreadModule(a), "object");
assertEq(typeof finishOffThreadModule(b), "object");

a = offThreadDecodeScript(encodeScript("3"));
b = offThreadDecodeScript(encodeScript("4"));
assertEq(runOffThreadDecodedScript(a), 3);
assertEq(runOffThreadDecodedScript(b), 4);

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
    jobs[i] = offThreadCompileModule("");
for (let i = 0; i < jobs.length; i++)
    assertEq(typeof finishOffThreadModule(jobs[i]), "object");

jobs = new Array(count);
for (let i = 0; i < jobs.length; i++)
    jobs[i] = offThreadDecodeScript(encodeScript(`${i} * ${i}`));
for (let i = 0; i < jobs.length; i++)
    assertEq(runOffThreadDecodedScript(jobs[i]), i * i);
