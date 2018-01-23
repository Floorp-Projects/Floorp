// Test multiple concurrent off-thread parse jobs.

if (helperThreadCount() === 0)
    quit();

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
    evaluate(entry, { global: global, saveBytecode: true });
    return entry;
}

let a, b, c;

// Calling run functions without arguments assumes a single off-thread job.

// Test run functions fail when no jobs exist.

assertFails(() => runOffThreadScript());

assertFails(() => finishOffThreadModule());

assertFails(() => runOffThreadDecodedScript());

// Test run functions fail when multiple jobs exist and no ID specified.

a = offThreadCompileScript("");
b = offThreadCompileScript("");
assertFails(() => runOffThreadScript());
runOffThreadScript(a);
runOffThreadScript(b);

a = offThreadCompileModule("");
b = offThreadCompileModule("");
assertFails(() => finishOffThreadModule());
finishOffThreadModule(a);
finishOffThreadModule(b);

a = offThreadDecodeScript(encodeScript(""));
b = offThreadDecodeScript(encodeScript(""));
assertFails(() => runOffThreadScript());
runOffThreadDecodedScript(a);
runOffThreadDecodedScript(b);

// Test fun functions succeed when a single job exist and no ID specified.

offThreadCompileScript("42");
assertEq(runOffThreadScript(), 42);

offThreadCompileModule("");
assertEq(typeof finishOffThreadModule(), "object");

offThreadDecodeScript(encodeScript("23"));
assertEq(runOffThreadDecodedScript(), 23);

// Run functions take an ID argument returned from the compile function.

// Test bad ID type and unknown ID.

offThreadCompileScript("");
assertFails(() => runOffThreadScript("foo"));
assertFails(() => runOffThreadScript(42));
runOffThreadScript();

offThreadCompileModule("");
assertFails(() => finishOffThreadModule("foo"));
assertFails(() => finishOffThreadModule(42));
finishOffThreadModule();

offThreadDecodeScript(encodeScript(""));
assertFails(() => runOffThreadDecodedScript("foo"));
assertFails(() => runOffThreadDecodedScript(42));
runOffThreadDecodedScript();

// Test stale ID.

a = offThreadCompileScript("");
runOffThreadScript(a);
assertFails(() => runOffThreadScript(a));

a = offThreadCompileModule("");
finishOffThreadModule(a);
assertFails(() => finishOffThreadModule(a));

a = offThreadDecodeScript(encodeScript(""));
runOffThreadDecodedScript(a);
assertFails(() => runOffThreadDecodedScript(a));

// Test wrong job kind.

a = offThreadCompileScript("");
b = offThreadCompileModule("");
c = offThreadDecodeScript(encodeScript(""));
assertFails(() => runOffThreadScript(b));
assertFails(() => runOffThreadScript(c));
assertFails(() => finishOffThreadModule(a));
assertFails(() => finishOffThreadModule(c));
assertFails(() => runOffThreadDecodedScript(a));
assertFails(() => runOffThreadDecodedScript(b));
runOffThreadScript(a);
finishOffThreadModule(b);
runOffThreadDecodedScript(c);

// Test running multiple jobs.

a = offThreadCompileScript("1");
b = offThreadCompileScript("2");
assertEq(runOffThreadScript(a), 1);
assertEq(runOffThreadScript(b), 2);

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
    jobs[i] = offThreadCompileScript(`${i} * ${i}`);
for (let i = 0; i < jobs.length; i++)
    assertEq(runOffThreadScript(jobs[i]), i * i);

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
