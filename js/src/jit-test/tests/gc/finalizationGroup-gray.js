// |jit-test| --enable-weak-refs

// Test gray finalization group is correctly barrired.
target = {};
group = new FinalizationGroup(iterator => undefined);
group.register(target, 1);
grayRoot()[0] = group;
group = undefined;
gc(); // Group is now marked gray.
target = undefined;
gc(); // Target dies, group is queued.
drainJobQueue();
