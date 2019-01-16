// |jit-test| --no-ion; --no-baseline; skip-if: !('oomAfterAllocations' in this)
// Don't crash on OOM in ReadableStreamDefaultReader.prototype.read().

for (let n = 1; n < 1000; n++) {
  let stream = new ReadableStream({
    start(controller) {
      controller.enqueue(7);
    }
  });
  let reader = stream.getReader();
  oomAfterAllocations(n);
  try {
    reader.read();
    n = 1000;
  } catch {}
  resetOOMFailure();
}
