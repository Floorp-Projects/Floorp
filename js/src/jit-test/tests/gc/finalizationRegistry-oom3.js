let registry = new FinalizationRegistry(x => 0);
registry.register({}, 1, {});
let token = {};
oomTest(() => registry.unregister(token));
