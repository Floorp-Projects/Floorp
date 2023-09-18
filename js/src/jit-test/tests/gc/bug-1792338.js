// |jit-test| allow-unhandlable-oom; skip-if: (getBuildConfiguration("android") && getBuildConfiguration("debug"))

gczeal(0);
if (!this.enqueueMark) {
  quit();
}

enqueueMark('set-color-gray');
enqueueMark(newGlobal({newCompartment: true}));
enqueueMark('set-color-black');
enqueueMark({});
setMarkStackLimit(1);
gc();
gc();
