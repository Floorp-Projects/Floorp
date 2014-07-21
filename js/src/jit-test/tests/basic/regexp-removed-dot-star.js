
// Test that removal of leading or trailing .* from RegExp test() calls
// does not affect lastMatch or other RegExpStatics info.

function first(input) {
    var re = /.*b(cd)/;
    for (var i = 0; i < 10; i++)
	re.test(input);
}

first("1234\nabcdefg\n1234");
assertEq(RegExp.lastMatch, "abcd");
assertEq(RegExp.$1, "cd");
assertEq(RegExp.input, "1234\nabcdefg\n1234");
assertEq(RegExp.leftContext, "1234\n");
assertEq(RegExp.rightContext, "efg\n1234");
assertEq(RegExp.lastParen, "cd");


// Test that removal of leading or trailing .* from RegExp test() calls
// does not affect lastMatch or other RegExpStatics info.

function second(input) {
    var re = /bcd.*/;
    for (var i = 0; i < 10; i++)
	re.test(input);
}

second("1234\nabcdefg\n1234");
assertEq(RegExp.lastMatch, "bcdefg");
assertEq(RegExp.$1, "");
assertEq(RegExp.input, "1234\nabcdefg\n1234");
assertEq(RegExp.leftContext, "1234\na");
assertEq(RegExp.rightContext, "\n1234");
assertEq(RegExp.lastParen, "");

function third(input) {
    var re = /.*bcd.*/;
    for (var i = 0; i < 10; i++)
	re.test(input);
}

third("1234\nabcdefg\n1234");
assertEq(RegExp.lastMatch, "abcdefg");
assertEq(RegExp.$1, "");
assertEq(RegExp.input, "1234\nabcdefg\n1234");
assertEq(RegExp.leftContext, "1234\n");
assertEq(RegExp.rightContext, "\n1234");
assertEq(RegExp.lastParen, "");
