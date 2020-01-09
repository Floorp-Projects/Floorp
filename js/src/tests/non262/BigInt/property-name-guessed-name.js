// |reftest| skip-if(!xulRuntime.shell)

// BigInts currently don't participate when computing guessed function names.

var p = {};
p[1] = function(){};
p[2n] = function(){};

assertEq(displayName(p[1]), "p[1]");
assertEq(displayName(p[2]), "");

var q = {
  1: [function(){}],
  2n: [function(){}],
};

assertEq(displayName(q[1][0]), "q[1]<");
assertEq(displayName(q[2][0]), "q<");

if (typeof reportCompare === "function")
  reportCompare(true, true);
