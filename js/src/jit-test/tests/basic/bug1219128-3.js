// |jit-test| skip-if: !('oomTest' in this)

x = evalcx('lazy');
oomTest(function() {
    x.eval
});
