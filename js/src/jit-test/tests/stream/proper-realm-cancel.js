// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")

ignoreUnhandledRejections();

var g = newGlobal({ newCompartment:  true });
var ccwCancelMethod = new g.Function("return 17;");

new ReadableStream({ cancel: ccwCancelMethod }).cancel("bye");
