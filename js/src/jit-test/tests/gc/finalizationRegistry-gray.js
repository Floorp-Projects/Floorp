// |jit-test| --enable-weak-refs

// Test gray finalization registry is correctly barrired.
target = {};
registry = new FinalizationRegistry(iterator => undefined);
registry.register(target, 1);
grayRoot()[0] = registry;
registry = undefined;
gc(); // Registry is now marked gray.
target = undefined;
gc(); // Target dies, registry is queued.
drainJobQueue();
