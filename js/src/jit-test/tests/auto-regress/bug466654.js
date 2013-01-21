// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-29add08d84ae-linux
// Flags: -j
//
this.watch('y',  /x/g );
for each (y in ['q', 'q', 'q']) continue;
gc();
