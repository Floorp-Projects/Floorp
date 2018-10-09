// |jit-test| skip-if: !('oomTest' in this)

oomTest(function () {
    eval(`var wm = new WeakMap();
         wm.set({}, 'FOO').get(false);`);
});
