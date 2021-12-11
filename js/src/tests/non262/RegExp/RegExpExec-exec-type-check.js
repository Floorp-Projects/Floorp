// Bug 1667094.

var obj = {
  exec() {
    return function(){};
  }
};

assertEq(RegExp.prototype.test.call(obj, ""), true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
