// This test passes if we don't have a CCW assertion.
var g = newGlobal();
var ccwCancelMethod = new g.Function("return 17;");

new ReadableStream({ cancel: ccwCancelMethod }).cancel("bye");
