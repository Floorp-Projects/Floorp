// |jit-test| --enable-symbols-as-weakmap-keys; skip-if: getBuildConfiguration("release_or_beta")
b = new WeakMap();
c = Symbol();
b.set(c);
c = gczeal(10);
for (i=0; i<1000; ++i) {
    try {
        x;
    } catch {}
}
