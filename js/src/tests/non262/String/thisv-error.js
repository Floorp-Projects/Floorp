function testName(thisv) {
  var failures = [
    // Not a function
    "length",
    // TODO: Different implementation
    "toString",
    "toSource",
    "valueOf",
    // Aliases
    "trimLeft",
    "trimRight",
    // Returns empty string
    "constructor"
  ]

  var keys = Object.getOwnPropertyNames(String.prototype);
  for (var key of keys) {
    var message;
    try {
      String.prototype[key].call(thisv);
    } catch (e) {
      message = e.message;
    }

    var expected = `String.prototype.${key} called on incompatible ${thisv}`;
    if (failures.includes(key)) {
      assertEq(message !== expected, true)
    } else {
      assertEq(message, expected);
    }
  }
}
testName(null);
testName(undefined);

// On-off test for Symbol.iterator
function testIterator(thisv) {
  var message;
  try {
    String.prototype[Symbol.iterator].call(thisv);
  } catch (e) {
    message = e.message;
  }

  assertEq(message, `String.prototype[Symbol.iterator] called on incompatible ${thisv}`);
}
testIterator(null);
testIterator(undefined);

if (typeof reportCompare === "function")
    reportCompare(true, true);
