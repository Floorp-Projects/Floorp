// |reftest| skip-if(!xulRuntime.shell||!("normalize"in(String.prototype))) -- uses shell load() function, String.prototype.normalize is not enabled in all builds
var BUGNUMBER = 918987;
var summary = 'String.prototype.normalize';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

load('ecma_6/String/normalize-generateddata-input.js');

function codePointsToString(points) {
  return points.map(x => String.fromCodePoint(x)).join("");
}
function stringify(points) {
  return points.map(x => x.toString(16)).join();
}

function runTest(test) {
  var source = codePointsToString(test.source);
  var NFC = codePointsToString(test.NFC);
  var NFD = codePointsToString(test.NFD);
  var NFKC = codePointsToString(test.NFKC);
  var NFKD = codePointsToString(test.NFKD);
  var sourceStr = stringify(test.source);
  var nfcStr = stringify(test.NFC);
  var nfdStr = stringify(test.NFD);
  var nfkcStr = stringify(test.NFKC);
  var nfkdStr = stringify(test.NFKD);

  /* NFC */
  assertEq(source.normalize(), NFC, "NFC of " + sourceStr);
  assertEq(NFC.normalize(), NFC, "NFC of " + nfcStr);
  assertEq(NFD.normalize(), NFC, "NFC of " + nfdStr);
  assertEq(NFKC.normalize(), NFKC, "NFC of " + nfkcStr);
  assertEq(NFKD.normalize(), NFKC, "NFC of " + nfkdStr);

  assertEq(source.normalize(undefined), NFC, "NFC of " + sourceStr);
  assertEq(NFC.normalize(undefined), NFC, "NFC of " + nfcStr);
  assertEq(NFD.normalize(undefined), NFC, "NFC of " + nfdStr);
  assertEq(NFKC.normalize(undefined), NFKC, "NFC of " + nfkcStr);
  assertEq(NFKD.normalize(undefined), NFKC, "NFC of " + nfkdStr);

  assertEq(source.normalize("NFC"), NFC, "NFC of " + sourceStr);
  assertEq(NFC.normalize("NFC"), NFC, "NFC of " + nfcStr);
  assertEq(NFD.normalize("NFC"), NFC, "NFC of " + nfdStr);
  assertEq(NFKC.normalize("NFC"), NFKC, "NFC of " + nfkcStr);
  assertEq(NFKD.normalize("NFC"), NFKC, "NFC of " + nfkdStr);

  /* NFD */
  assertEq(source.normalize("NFD"), NFD, "NFD of " + sourceStr);
  assertEq(NFC.normalize("NFD"), NFD, "NFD of " + nfcStr);
  assertEq(NFD.normalize("NFD"), NFD, "NFD of " + nfdStr);
  assertEq(NFKC.normalize("NFD"), NFKD, "NFD of " + nfkcStr);
  assertEq(NFKD.normalize("NFD"), NFKD, "NFD of " + nfkdStr);

  /* NFKC */
  assertEq(source.normalize("NFKC"), NFKC, "NFKC of " + sourceStr);
  assertEq(NFC.normalize("NFKC"), NFKC, "NFKC of " + nfcStr);
  assertEq(NFD.normalize("NFKC"), NFKC, "NFKC of " + nfdStr);
  assertEq(NFKC.normalize("NFKC"), NFKC, "NFKC of " + nfkcStr);
  assertEq(NFKD.normalize("NFKC"), NFKC, "NFKC of " + nfkdStr);

  /* NFKD */
  assertEq(source.normalize("NFKD"), NFKD, "NFKD of " + sourceStr);
  assertEq(NFC.normalize("NFKD"), NFKD, "NFKD of " + nfcStr);
  assertEq(NFD.normalize("NFKD"), NFKD, "NFKD of " + nfdStr);
  assertEq(NFKC.normalize("NFKD"), NFKD, "NFKD of " + nfkcStr);
  assertEq(NFKD.normalize("NFKD"), NFKD, "NFKD of " + nfkdStr);
}

for (var test0 of tests_part0) {
  runTest(test0);
}
var part1 = new Set();
for (var test1 of tests_part1) {
  part1.add(test1.source[0]);
  runTest(test1);
}
for (var test2 of tests_part2) {
  runTest(test2);
}
for (var test3 of tests_part3) {
  runTest(test3);
}

/* not listed in Part 1 */
for (var x = 0; x <= 0x2FFFF; x++) {
  if (part1.has(x)) {
    continue;
  }
  var xstr = x.toString(16);
  var c = String.fromCodePoint(x);
  assertEq(c.normalize(), c, "NFC of " + xstr);
  assertEq(c.normalize(undefined), c, "NFC of " + xstr);
  assertEq(c.normalize("NFC"), c, "NFC of " + xstr);
  assertEq(c.normalize("NFD"), c, "NFD of " + xstr);
  assertEq(c.normalize("NFKC"), c, "NFKC of " + xstr);
  assertEq(c.normalize("NFKD"), c, "NFKD of " + xstr);
}

var myobj = {toString : (function () "a\u0301"), normalize : String.prototype.normalize};
assertEq(myobj.normalize(), "\u00E1");

assertThrowsInstanceOf(function() {
                         "abc".normalize("NFE");
                       }, RangeError,
                       "String.prototype.normalize should raise RangeError on invalid form");

assertEq("".normalize(), "");

/* JSRope test */
var a = "";
var b = "";
for (var i = 0; i < 100; i++) {
  a += "\u0100";
  b += "\u0041\u0304";
}
assertEq(a.normalize("NFD"), b);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);
print("Tests complete");
