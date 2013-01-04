Object.prototype[0] = "foo";
assertEq([/* hole */, 5].concat().hasOwnProperty("0"), true);
