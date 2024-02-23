try {
function varying(mapColor, keyColor) {
  enqueueMark(`set-color-${keyColor}`);
  enqueueMark("yield");
  startgc(100000);
}
for (const mapColor of ['gray', 'black'])
    varying(mapColor, 0x7fff);
} catch (exc) {}
oomAfterAllocations(100);
