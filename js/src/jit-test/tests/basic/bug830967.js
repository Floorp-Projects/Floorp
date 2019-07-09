var x = Array.prototype.concat.call(Object.freeze([{}]));
assertEq(x.length, 1);
assertEq(0 in x, true);
assertEq(JSON.stringify(x[0]), "{}");
