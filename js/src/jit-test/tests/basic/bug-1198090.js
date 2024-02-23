// |jit-test| allow-oom

for (let a of [
        null, function() {}, function() {}, null, function() {}, function() {},
        function() {}, null, null, null, null, null, null, null, null,
        function() {}, null, null, null, function() {}
    ]) {
    oomAtAllocation(5);
}
