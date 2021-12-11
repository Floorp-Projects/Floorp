// |jit-test| skip-if: !('oomTest' in this)
for (var i = 0; i < 20; i++) {}
oomTest(enableGeckoProfiling);
