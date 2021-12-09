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

assertFails(() => runOffThreadDecodedScript());

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

a = offThreadDecodeScript(encodeScript(""));
b = offThreadDecodeScript(encodeScript(""));
assertFails(() => finishOffThreadCompileToStencil());
runOffThreadDecodedScript(a);
runOffThreadDecodedScript(b);

// Test fun functions succeed when a single job exist and no ID specified.

offThreadCompileToStencil("42");
stencil = finishOffThreadCompileToStencil();
assertEq(evalStencil(stencil), 42);

offThreadCompileModuleToStencil("");
stencil = finishOffThreadCompileModuleToStencil();
assertEq(typeof instantiateModuleStencil(stencil), "object");

offThreadDecodeScript(encodeScript("23"));
assertEq(runOffThreadDecodedScript(), 23);

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

offThreadDecodeScript(encodeScript(""));
assertFails(() => runOffThreadDecodedScript("foo"));
assertFails(() => runOffThreadDecodedScript(42));
runOffThreadDecodedScript();

// Test stale ID.

a = offThreadCompileToStencil("");
stencilA = finishOffThreadCompileToStencil(a);
evalStencil(stencilA);
assertFails(() => finishOffThreadCompileToStencil(a));

a = offThreadCompileModuleToStencil("");
stencilA = finishOffThreadCompileModuleToStencil(a);
assertFails(() => finishOffThreadCompileModuleToStencil(a));
instantiateModuleStencil(stencilA);

a = offThreadDecodeScript(encodeScript(""));
runOffThreadDecodedScript(a);
assertFails(() => runOffThreadDecodedScript(a));

// Test wrong job kind.

a = offThreadCompileToStencil("");
b = offThreadCompileModuleToStencil("");
c = offThreadDecodeScript(encodeScript(""));
assertFails(() => finishOffThreadCompileToStencil(b));
assertFails(() => finishOffThreadCompileToStencil(c));
assertFails(() => finishOffThreadCompileModuleToStencil(a));
assertFails(() => finishOffThreadCompileModuleToStencil(c));
assertFails(() => runOffThreadDecodedScript(a));
assertFails(() => runOffThreadDecodedScript(b));
stencilA = finishOffThreadCompileToStencil(a);
evalStencil(stencilA);
stencilB = finishOffThreadCompileModuleToStencil(b);
instantiateModuleStencil(stencilB);
runOffThreadDecodedScript(c);

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
    jobs[i] = offThreadCompileModuleToStencil("");
for (let i = 0; i < jobs.length; i++) {
    stencil = finishOffThreadCompileModuleToStencil(jobs[i]);
    assertEq(typeof instantiateModuleStencil(stencil), "object");
}

jobs = new Array(count);
for (let i = 0; i < jobs.length; i++)
    jobs[i] = offThreadDecodeScript(encodeScript(`${i} * ${i}`));
for (let i = 0; i < jobs.length; i++)
    assertEq(runOffThreadDecodedScript(jobs[i]), i * i);
