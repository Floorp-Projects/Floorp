if (!('oomTest' in this))
    quit();

oomTest(function () {
    eval(`var wm = new WeakMap();
         wm.set({}, 'FOO').get(false);`);
});
