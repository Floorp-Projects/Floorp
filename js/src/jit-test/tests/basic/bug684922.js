// |jit-test| error: InternalError
var op = Object.prototype;
op.b = op;
op.__iterator__ = Iterator;
for (var c in {}) {}

