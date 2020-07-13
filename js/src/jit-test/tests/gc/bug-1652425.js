// |jit-test| error: Error

function varying(mapColor, keyColor) {
  enqueueMark(`set-color-${keyColor}`);
  enqueueMark("yield");
  startgc(100000);
}
for (const mapColor of ['gray', 'black']) {
  for (const keyColor of ['gray', 'black', 'unmarked'])
    varying(mapColor, keyColor);
}
