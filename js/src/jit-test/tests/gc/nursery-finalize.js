assertEq(0, finalizeCount());
makeFinalizeObserver('tenured');
minorgc();
assertEq(0, finalizeCount());
makeFinalizeObserver('nursery');
minorgc();
assertEq(1, finalizeCount());
gc();
assertEq(2, finalizeCount());

var N = 10000;
for (var i = 0; i < N; ++i)
    makeFinalizeObserver('nursery');
minorgc();
assertEq(N + 2, finalizeCount());

