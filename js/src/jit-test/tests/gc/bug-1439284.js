// |jit-test| skip-if: helperThreadCount() === 0

gcparam('allocationThreshold', 1);
setGCCallback({
    action: "majorGC",
});
offThreadCompileScript(('Boolean.prototype.toSource.call(new String())'));
for (let i = 0; i < 10; i++) {
    for (let j = 0; j < 10000; j++) Symbol.for(i + 10 * j);
}
try {
    runOffThreadScript();
} catch (e) {
    assertEq(e.constructor, TypeError);
}
