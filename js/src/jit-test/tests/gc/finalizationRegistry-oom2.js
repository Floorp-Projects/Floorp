let registry = new FinalizationRegistry(x => 0);
let token = {};
oomTest(() => registry.register({}, 1, token));
