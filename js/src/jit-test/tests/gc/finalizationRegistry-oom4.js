let registry = new FinalizationRegistry(x => 0);
let target = {};
let token = {};
oomTest(() => registry.register(target, 1, token));
