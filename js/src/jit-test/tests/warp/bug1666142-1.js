// |jit-test| --fast-warmup

// This test triggers a GC in CreateThisForIC,
// without using the arguments rectifier.
var records = [];
function Record() {
    return Object.create(null);
}
function init() {
    records.push(new Record());
}
function f() {
    for (var i = 0; i < 100; i++) {
        init();
    }
}

gczeal(14,25);
f();
