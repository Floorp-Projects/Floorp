// |jit-test| --no-ggc; allow-unhandlable-oom; --no-threads

load(libdir + 'oomTest.js');
oomTest(function () {
    eval(`var wm = new WeakMap();
         wm.set({}, 'FOO').get(false);`);
});
