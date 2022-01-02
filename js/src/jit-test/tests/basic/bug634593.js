this.__defineGetter__("x3", Function);
parseInt = x3;
parseInt.prototype = [];
for (var z = 0; z < 10; ++z) { new parseInt() }