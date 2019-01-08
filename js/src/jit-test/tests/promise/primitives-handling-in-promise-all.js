// This just shouldn't crash.
ignoreUnhandledRejections();

Promise.resolve = () => 42;
Promise.all([1]);
