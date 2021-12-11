// Create Latin1 string + atom.
var latin1S = "foo".repeat(50);
var obj = {};
obj[latin1S] = 3;
assertEq(obj[latin1S], 3);

// Create a TwoByte version, ensure it's in the StringToAtomCache.
var twoByteS = newString(latin1S, {twoByte: true});
assertEq(obj[twoByteS], 3);

// Create a dependent TwoByte string.
var depTwoByteS = twoByteS.slice(1);

// Deduplication shouldn't get confused about Latin1 atom vs TwoByte strings.
minorgc();
assertEq(obj["f" + depTwoByteS], 3);
