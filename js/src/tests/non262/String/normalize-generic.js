var BUGNUMBER = 918987;
var summary = 'String.prototype.normalize - normalize no String object';

print(BUGNUMBER + ": " + summary);

function test() {
  var myobj = {
    toString: () => "a\u0301",
    normalize: String.prototype.normalize
  };
  assertEq(myobj.normalize(), "\u00E1");
}

if ("normalize" in String.prototype) {
  // String.prototype.normalize is not enabled in all builds.
  test();
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
