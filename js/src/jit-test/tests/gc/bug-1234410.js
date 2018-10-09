// |jit-test| skip-if: !('oomTest' in this)

enableGeckoProfiling();
oomTest(() => {
    try {
        for (var quit of oomTest.gcparam("//").ArrayBuffer(1)) {}
    } catch (e) {}
});

