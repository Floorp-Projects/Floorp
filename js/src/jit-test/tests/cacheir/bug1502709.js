
setJitCompilerOption("baseline.warmup.trigger", 0);
main();

function main() {
    try {
        x = [];
        Object.defineProperty(x, 1, {});
        y = [];
        Object.defineProperty(y, 1, {});
        y.__proto__ = null;
        Array.prototype.sort.apply(x, [function() {}]);
    } catch (e) {}
    try {
        Array.prototype.sort.apply(y, [function() {}]);
    } catch (e) {}
}
