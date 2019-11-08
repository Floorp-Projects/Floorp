// |jit-test| --enable-weak-refs; skip-if: !('oomTest' in this)
let group = new FinalizationGroup(x => 0);
group.register({}, 1, {});
oomTest(() => group.unregister(token));
