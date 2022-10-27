// |jit-test| allow-unhandlable-oom

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
