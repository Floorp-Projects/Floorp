var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- disallow extended patterns.";

print(BUGNUMBER + ": " + summary);

// IdentityEscape

assertEqArray(/\^\$\\\.\*\+\?\(\)\[\]\{\}\|/u.exec("^$\\.*+?()[]{}|"),
              ["^$\\.*+?()[]{}|"]);
assertThrowsInstanceOf(() => eval(`/\\A/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\-/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\U{10}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\U0000/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\uD83D\\U0000/u`), SyntaxError);

assertEqArray(/[\^\$\\\.\*\+\?\(\)\[\]\{\}\|]+/u.exec("^$\\.*+?()[]{}|"),
              ["^$\\.*+?()[]{}|"]);
assertThrowsInstanceOf(() => eval(`/[\\A]/u`), SyntaxError);
assertEqArray(/[A\-Z]+/u.exec("a-zABC"),
              ["-"]);
assertThrowsInstanceOf(() => eval(`/[\\U{10}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\U0000]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\U0000]/u`), SyntaxError);

// PatternCharacter
assertThrowsInstanceOf(() => eval(`/{}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{0}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{1,}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/{1,2}/u`), SyntaxError);

// QuantifiableAssertion
assertEqArray(/.B(?=A)/u.exec("cBaCBA"),
              ["CB"]);
assertEqArray(/.B(?!A)/u.exec("CBAcBa"),
              ["cB"]);
assertEqArray(/.B(?:A)/u.exec("cBaCBA"),
              ["CBA"]);
assertEqArray(/.B(A)/u.exec("cBaCBA"),
              ["CBA", "A"]);

assertThrowsInstanceOf(() => eval(`/.B(?=A)+/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/.B(?!A)+/u`), SyntaxError);
assertEqArray(/.B(?:A)+/u.exec("cBaCBA"),
              ["CBA"]);
assertEqArray(/.B(A)+/u.exec("cBaCBA"),
              ["CBA", "A"]);

// ControlLetter
assertEqArray(/\cA/u.exec("\u0001"),
              ["\u0001"]);
assertEqArray(/\cZ/u.exec("\u001a"),
              ["\u001a"]);
assertEqArray(/\ca/u.exec("\u0001"),
              ["\u0001"]);
assertEqArray(/\cz/u.exec("\u001a"),
              ["\u001a"]);

assertEqArray(/[\cA]/u.exec("\u0001"),
              ["\u0001"]);
assertEqArray(/[\cZ]/u.exec("\u001a"),
              ["\u001a"]);
assertEqArray(/[\ca]/u.exec("\u0001"),
              ["\u0001"]);
assertEqArray(/[\cz]/u.exec("\u001a"),
              ["\u001a"]);

assertThrowsInstanceOf(() => eval(`/\\c/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\c1/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\c_/u`), SyntaxError);

assertThrowsInstanceOf(() => eval(`/[\\c]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\c1]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\c_]/u`), SyntaxError);

// HexEscapeSequence
assertThrowsInstanceOf(() => eval(`/\\x/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\x0/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\x1/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\x1G/u`), SyntaxError);

// LegacyOctalEscapeSequence
assertThrowsInstanceOf(() => eval(`/\\52/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\052/u`), SyntaxError);

// DecimalEscape
assertEqArray(/\0/u.exec("\0"),
              ["\0"]);
assertEqArray(/[\0]/u.exec("\0"),
              ["\0"]);
assertEqArray(/\0A/u.exec("\0A"),
              ["\0A"]);
assertEqArray(/\0G/u.exec("\0G"),
              ["\0G"]);
assertEqArray(/(A.)\1/u.exec("ABACABAB"),
              ["ABAB", "AB"]);
assertEqArray(/(A.)(B.)(C.)(D.)(E.)(F.)(G.)(H.)(I.)(J.)(K.)\10/u.exec("A1B2C3D4E5F6G7H8I9JaKbJa"),
              ["A1B2C3D4E5F6G7H8I9JaKbJa", "A1", "B2", "C3", "D4", "E5", "F6", "G7", "H8", "I9", "Ja", "Kb"]);

assertThrowsInstanceOf(() => eval(`/\\00/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\01/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\09/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\1/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\2/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\3/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\4/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\5/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\6/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\7/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\8/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\9/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\10/u`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
