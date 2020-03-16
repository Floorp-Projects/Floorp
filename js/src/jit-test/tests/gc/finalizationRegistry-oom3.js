// |jit-test| --enable-weak-refs; skip-if: !('oomTest' in this)
let registry = new FinalizationRegistry(x => 0);
registry.register({}, 1, {});
let token = {};
oomTest(() => registry.unregister(token));
