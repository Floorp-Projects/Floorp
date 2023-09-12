// Bug 1851619: Shouldn't crash. |jit-test| exitstatus: 3

// This test triggers a stack overflow, then calls a finalization registry
// cleanup callback that previously used data that had already been discarded.
enableShellAllocationMetadataBuilder();
var registry = new FinalizationRegistry(()=>{
});
registry.register({}, 1, {});
function recurse(obj) {
    recurse(obj);
}
recurse();
