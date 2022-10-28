// |jit-test| allow-unhandlable-oom

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
