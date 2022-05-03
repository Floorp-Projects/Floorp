if (this.enqueueMark) {
    enqueueMark('set-color-gray');
    enqueueMark('set-color-black');
    enqueueMark(newGlobal());
    enqueueMark('set-color-gray');
    enqueueMark('set-color-black');
    enqueueMark(newGlobal());
    enqueueMark('set-color-gray');
    enqueueMark('set-color-black');
    enqueueMark(newGlobal());
    gc();
}
