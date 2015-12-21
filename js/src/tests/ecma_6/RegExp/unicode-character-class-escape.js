var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- CharacterClassEscape.";

print(BUGNUMBER + ": " + summary);

// BMP

assertEqArray(/\d+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["0123456789"]);
assertEqArray(/\D+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["abcxyzABCXYZ"]);

assertEqArray(/\s+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["\t\r\n\v\x0c\xa0\uFEFF"]);
assertEqArray(/\S+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["abcxyzABCXYZ0123456789_"]);

assertEqArray(/\w+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["abcxyzABCXYZ0123456789_"]);
assertEqArray(/\W+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["\t\r\n\v\x0c\xa0\uFEFF*"]);

assertEqArray(/\n+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["\n"]);

assertEqArray(/[\d]+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["0123456789"]);
assertEqArray(/[\D]+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["abcxyzABCXYZ"]);

assertEqArray(/[\s]+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["\t\r\n\v\x0c\xa0\uFEFF"]);
assertEqArray(/[\S]+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["abcxyzABCXYZ0123456789_"]);

assertEqArray(/[\w]+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["abcxyzABCXYZ0123456789_"]);
assertEqArray(/[\W]+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["\t\r\n\v\x0c\xa0\uFEFF*"]);

assertEqArray(/[\n]+/u.exec("abcxyzABCXYZ0123456789_\t\r\n\v\x0c\xa0\uFEFF*"),
              ["\n"]);

// non-BMP

function testNonBMP(re) {
  assertEqArray(re.exec("\uD83D\uDBFF"),
                ["\uD83D"]);
  assertEqArray(re.exec("\uD83D\uDC00"),
                ["\uD83D\uDC00"]);
  assertEqArray(re.exec("\uD83D\uDFFF"),
                ["\uD83D\uDFFF"]);
  assertEqArray(re.exec("\uD83D\uE000"),
                ["\uD83D"]);

  assertEqArray(re.exec("\uD7FF\uDC38"),
                ["\uD7FF"]);
  assertEqArray(re.exec("\uD800\uDC38"),
                ["\uD800\uDC38"]);
  assertEqArray(re.exec("\uDBFF\uDC38"),
                ["\uDBFF\uDC38"]);
  assertEqArray(re.exec("\uDC00\uDC38"),
                ["\uDC00"]);
}

testNonBMP(/\D/u);
testNonBMP(/\S/u);
testNonBMP(/\W/u);

testNonBMP(/[\D]/u);
testNonBMP(/[\S]/u);
testNonBMP(/[\W]/u);

if (typeof reportCompare === "function")
    reportCompare(true, true);
