// This just shouldn't crash.
Promise.resolve = () => 42;
Promise.all([1]);
