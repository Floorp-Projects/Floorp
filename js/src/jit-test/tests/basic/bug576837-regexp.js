/*
 * Check that builtin character classes within ranges produce syntax
 * errors.
 *
 * Note: /\b/ is the backspace escape, which is a single pattern character,
 * though it looks deceptively like a character class.
 */

function isRegExpSyntaxError(pattern) {
    try {
        var re = new RegExp(pattern);
    } catch (e) {
        if (e instanceof SyntaxError)
            return true;
    }
    return false;
}

//assertEq(isRegExpSyntaxError('[C-\\s]'), false);
//assertEq(isRegExpSyntaxError('[C-\\d]'), false);
//assertEq(isRegExpSyntaxError('[C-\\W]'), false);
assertEq(isRegExpSyntaxError('[C-]'), false);
assertEq(isRegExpSyntaxError('[-C]'), false);
assertEq(isRegExpSyntaxError('[C-C]'), false);
assertEq(isRegExpSyntaxError('[C-C]'), false);
assertEq(isRegExpSyntaxError('[\\b-\\b]'), false);
assertEq(isRegExpSyntaxError('[\\B-\\B]'), false);
assertEq(isRegExpSyntaxError('[\\b-\\B]'), false);
assertEq(isRegExpSyntaxError('[\\B-\\b]'), true);
//assertEq(isRegExpSyntaxError('[\\b-\\w]'), false);
//assertEq(isRegExpSyntaxError('[\\B-\\w]'), false);

/* Extension. */
assertEq(isRegExpSyntaxError('[\\s-\\s]'), false);
assertEq(isRegExpSyntaxError('[\\W-\\s]'), false);
assertEq(isRegExpSyntaxError('[\\s-\\W]'), false);
assertEq(isRegExpSyntaxError('[\\W-C]'), false);
assertEq(isRegExpSyntaxError('[\\d-C]'), false);
assertEq(isRegExpSyntaxError('[\\s-C]'), false);
assertEq(isRegExpSyntaxError('[\\w-\\b]'), false);
assertEq(isRegExpSyntaxError('[\\w-\\B]'), false);
