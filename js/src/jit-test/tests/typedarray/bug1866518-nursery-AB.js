// 9373 iterations were enough to trigger the crash, which requires
// allocating an empty ArrayBuffer in the last Cell of a Chunk that
// comes just before a NurseryChunk.
//
// 15000 iterations ran in 1 second on my machine.

evalInWorker(`
  gczeal(14);
  a = [];
  for (let b = 0; b < 15000; b++)
    a.push(new ArrayBuffer);
`);
