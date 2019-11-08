// |jit-test| --enable-weak-refs; skip-if: !('oomTest' in this)
let group = new FinalizationGroup(x => 0);
let token = {};
oomTest(() => group.register({}, 1, token));
