assertEq(typeof (Array.prototype.concat).call("foo")[0], "object")
assertEq(Array.prototype.concat.call("foo")[0] instanceof String, true);
