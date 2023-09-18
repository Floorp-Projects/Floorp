// |jit-test| allow-unhandlable-oom; skip-if: (getBuildConfiguration("android") && getBuildConfiguration("debug"))

gczeal(0);
if (!this.enqueueMark) {
  quit();
}

enqueueMark('set-color-gray');
enqueueMark(newGlobal());
enqueueMark('set-color-black');
enqueueMark(newGlobal());
setMarkStackLimit(1);
gc();
