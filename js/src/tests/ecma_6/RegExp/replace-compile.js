var BUGNUMBER = 1287524;
var summary = 'RegExp.prototype[@@replace] should call replacer function after collecting all matches.';

print(BUGNUMBER + ": " + summary);

var rx = RegExp("a", "g");
var r = rx[Symbol.replace]("abba", function() {
    rx.compile("b", "g");
    return "?";
});
assertEq(r, "?bb?");

rx = RegExp("a", "g");
r = "abba".replace(rx, function() {
    rx.compile("b", "g");
    return "?";
});
assertEq(r, "?bb?");

if (typeof reportCompare === "function")
  reportCompare(true, true);
