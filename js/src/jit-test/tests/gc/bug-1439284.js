// |jit-test| skip-if: helperThreadCount() === 0

gcparam('allocationThreshold', 1);
setGCCallback({
    action: "majorGC",
});
offThreadCompileToStencil(('Boolean.prototype.toString.call(new String())'));
for (let i = 0; i < 10; i++) {
    for (let j = 0; j < 10000; j++) Symbol.for(i + 10 * j);
}
try {
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
} catch (e) {
    assertEq(e.constructor, TypeError);
}
