var BUGNUMBER = 887016;
var summary = "RegExpExec should throw if exec property of non-RegExp is not callable";

print(BUGNUMBER + ": " + summary);

for (var exec of [null, 0, false, undefined, ""]) {
  // RegExp with non-callable exec
  var re = /a/;
  re.exec = exec;
  RegExp.prototype[Symbol.match].call(re, "foo");

  // non-RegExp with non-callable exec
  assertThrowsInstanceOf(() => RegExp.prototype[Symbol.match].call({ exec }, "foo"),
                         TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
