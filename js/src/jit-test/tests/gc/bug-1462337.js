// |jit-test| skip-if: !('oomTest' in this)

oomTest(function() {
    grayRoot().x = Object.create((obj[name]++));
});
oomTest(function() {
    gczeal(9);
    gcslice(new.target);
});
