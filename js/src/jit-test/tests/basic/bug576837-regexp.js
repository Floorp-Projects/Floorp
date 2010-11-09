/*
 * Check that builtin character classes within ranges produce syntax
 * errors.
 *
 * Note that, per the extension in bug 351463, SpiderMonkey permits hyphens
 * adjacent to character class escapes in character classes, treating them as a
 * hyphen pattern character. Therefore /[\d-\s]/ is okay
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

assertEq(isRegExpSyntaxError('[C-\\s]'), true);
assertEq(isRegExpSyntaxError('[C-\\d]'), true);
assertEq(isRegExpSyntaxError('[C-\\W]'), true);
assertEq(isRegExpSyntaxError('[C-]'), false);
assertEq(isRegExpSyntaxError('[-C]'), false);
assertEq(isRegExpSyntaxError('[C-C]'), false);
assertEq(isRegExpSyntaxError('[C-C]'), false);
assertEq(isRegExpSyntaxError('[\\b-\\b]'), false);
assertEq(isRegExpSyntaxError('[\\B-\\B]'), false);
assertEq(isRegExpSyntaxError('[\\b-\\B]'), false);
assertEq(isRegExpSyntaxError('[\\B-\\b]'), true);
assertEq(isRegExpSyntaxError('[\\b-\\w]'), true);
assertEq(isRegExpSyntaxError('[\\B-\\w]'), true);

/* Extension. */
assertEq(isRegExpSyntaxError('[\\s-\\s]'), false);
assertEq(isRegExpSyntaxError('[\\W-\\s]'), false);
assertEq(isRegExpSyntaxError('[\\s-\\W]'), false);
assertEq(isRegExpSyntaxError('[\\W-C]'), false);
assertEq(isRegExpSyntaxError('[\\d-C]'), false);
assertEq(isRegExpSyntaxError('[\\s-C]'), false);
assertEq(isRegExpSyntaxError('[\\w-\\b]'), false);
assertEq(isRegExpSyntaxError('[\\w-\\B]'), false);
