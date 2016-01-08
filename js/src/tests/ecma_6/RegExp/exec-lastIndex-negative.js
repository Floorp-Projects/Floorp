var BUGNUMBER = 1207922;
var summary = "negative lastIndex should be treated as 0.";

print(BUGNUMBER + ": " + summary);

var pattern = /abc/gi;
var string = 'AbcaBcabC';

var indices = [
    -1,
    -Math.pow(2,32),
    -(Math.pow(2,32) + 1),
    -Math.pow(2,32) * 2,
    -Math.pow(2,40),
    -Number.MAX_VALUE,
];
for (var index of indices) {
  pattern.lastIndex = index;
  var result = pattern.exec(string);
  assertEq(result.index, 0);
  assertEq(result.length, 1);
  assertEq(result[0], "Abc");
  assertEq(pattern.lastIndex, 3);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
