// file: detachArrayBuffer.js
function $DETACHBUFFER(buffer) {
  if (!$ || typeof $.detachArrayBuffer !== "function") {
    throw new Test262Error("No method available to detach an ArrayBuffer");
  }
  $.detachArrayBuffer(buffer);
}
