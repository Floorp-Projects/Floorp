// |reftest| skip-if(!xulRuntime.shell) -- uses shell load() function

var BUGNUMBER = 918987;
var summary = 'String.prototype.normalize - not listed in part1';

print(BUGNUMBER + ": " + summary);

function test() {
  load('ecma_6/String/normalize-generateddata-input.js');

  var part1 = new Set();
  for (var test1 of tests_part1) {
    part1.add(test1.source[0]);
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
}

if ("normalize" in String.prototype) {
  // String.prototype.normalize is not enabled in all builds.
  test();
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
