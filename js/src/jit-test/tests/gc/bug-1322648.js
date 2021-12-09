// |jit-test| skip-if: helperThreadCount() === 0
gczeal(0);
print = function(s) {}
startgc(1);
offThreadCompileToStencil("");
gczeal(10, 3);
for (var count = 0; count < 20; count++) {
    print(count);
}
