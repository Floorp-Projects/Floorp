gc();
gcslice(0);
function isClone(a, b) {
        var rmemory = new WeakMap();
            rmemory.set(a,b);
}
isClone([]);
