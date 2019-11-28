// |jit-test| --enable-weak-refs; skip-if: !('oomTest' in this)
let group = new FinalizationGroup(x => 0);
let target = {};
let token = {};
oomTest(() => group.register(target, 1, token));
