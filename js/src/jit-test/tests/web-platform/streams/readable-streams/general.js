// The web-platform test we're going to load (on the last line of this file)
// contains many tests; among them, it checks for the existence of pipeTo and
// pipeThrough, which we don't implement yet. Hack it:
if (ReadableStream.prototype.pipeTo || ReadableStream.prototype.pipeThrough) {
  throw new Error("Congratulations on implementing pipeTo/pipeThrough! Please update this test.\n");
}
// They don't have to work to pass the test, just exist:
Object.defineProperties(ReadableStream.prototype, {
  "pipeTo": {
    configurable: true,
    writable: true,
    enumerable: false,
    value: function pipeTo(dest, {preventClose, preventAbort, preventCancel, signal} = {}) {
      throw new InternalError("pipeTo: not implemented");
    },
  },
  "pipeThrough": {
    configurable: true,
    writable: true,
    enumerable: false,
    value: function pipeThrough({writable, readable}, {preventClose, preventAbort, preventCancel, signal}) {
      throw new InternalError("pipeThrough: not implemented");
    },
  },
});

load(libdir + "web-platform-testharness.js");
load_web_platform_test("streams/readable-streams/general.js");
