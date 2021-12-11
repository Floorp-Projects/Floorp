// This should work for real... at some point.
registerPaint("sure!", () => {});
console.log(
  globalThis instanceof PaintWorkletGlobalScope ? "So far so good" : "error"
);
