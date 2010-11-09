// Check that builtin character classes within ranges produce syntax
// errors.

function isRegExpSyntaxError(pattern) {
    try {
        var re = new RegExp(pattern);
    } catch (e) {
        if (e instanceof SyntaxError)
            return true;
    }
    return false;
}

function testRangeSyntax(end1, end2, shouldFail) {
    var makePattern = function(e1, e2) {
        var pattern = '[' + e1 + '-' + e2 + ']';
        print(uneval(pattern));
        return pattern;
    };
    assertEq(isRegExpSyntaxError(makePattern(end1, end2)), shouldFail);
    assertEq(isRegExpSyntaxError(makePattern(end2, end1)), shouldFail);
}

function checkRangeValid(end1, end2) {
    testRangeSyntax(end1, end2, false);
}

function checkRangeInvalid(end1, end2) {
    testRangeSyntax(end1, end2, true);
}

checkRangeInvalid('C', '\\s');
checkRangeInvalid('C', '\\d');
checkRangeInvalid('C', '\\W');
checkRangeInvalid('C', '\\W');
checkRangeValid('C', '');
checkRangeValid('C', 'C');
checkRangeInvalid('\\s', '\\s');
checkRangeInvalid('\\W', '\\s');
checkRangeValid('\\b', '\\b');
checkRangeValid('\\B', '\\B');
checkRangeInvalid('\\w', '\\B');
checkRangeInvalid('\\w', '\\b');
