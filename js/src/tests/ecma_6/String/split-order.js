var BUGNUMBER = 1287521;
var summary = 'String.prototype.split should call ToUint32(limit) before ToString(separator).';

print(BUGNUMBER + ": " + summary);

var log = [];
"abba".split({
  toString() {
    log.push("separator-tostring");
    return "b";
  }
}, {
  valueOf() {
    log.push("limit-valueOf");
    return 0;
  }
});

assertEq(log.join(","), "limit-valueOf,separator-tostring");

if (typeof reportCompare === "function")
  reportCompare(true, true);
