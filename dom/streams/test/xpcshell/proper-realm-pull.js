// This test passes if we don't have a CCW assertion.

var g = newGlobal({ newCompartment: true });
var ccwPullMethod = new g.Function("return 17;");

new ReadableStream({ pull: ccwPullMethod });
