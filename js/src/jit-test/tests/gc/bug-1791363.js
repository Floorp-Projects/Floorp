gczeal(0);
if (!this.enqueueMark) {
  quit();
}

enqueueMark('set-color-gray');
enqueueMark(newGlobal());
enqueueMark('set-color-black');
enqueueMark(newGlobal());
gcparam("markStackLimit", 1);
gc();
