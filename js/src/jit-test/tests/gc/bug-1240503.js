// |jit-test| skip-if: !('oomTest' in this)

function arrayProtoOutOfRange() {
    for (let [] = () => r, get;;)
        var r = f(i % 2 ? a : b);
}
oomTest(arrayProtoOutOfRange);
