if (this.enqueueMark) {
    gczeal(0);
    enqueueMark('set-color-gray');
    enqueueMark('set-color-black');
    enqueueMark(newGlobal());
    enqueueMark('set-color-gray');
    newGlobal();
    startgc();
}
