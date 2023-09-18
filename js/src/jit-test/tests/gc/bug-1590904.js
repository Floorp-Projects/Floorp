// |jit-test| skip-if: !getBuildConfiguration("has-gczeal")
gczeal(0);
gczeal(20);
startgc(1);
gczeal(10);
while (gcstate() == "Sweep") {}
