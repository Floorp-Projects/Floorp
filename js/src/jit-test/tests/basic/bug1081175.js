var input = "webkit-search-cancel-button-aaaaaaa-bbbbb-ccccccc-dddddddd,"
var bad_regex = '([a-u-]|\\u0080|\\u0100)*[d]';

function forceUnicode(s) {
    return ('\uffff' + s).replace(/^\uffff/, '');
}
function testRegex(input) {
    for (var i = 0; i < input.length; i++) {
        var sub = input.substring(0, i + 1);
	var res = sub.match(bad_regex);
	if (i >= 50) {
	    assertEq(res.length, 2);
	    assertEq(res[1], sub.substr(-2, 1));
	} else {
	    assertEq(res, null);
	}
    }
}
testRegex(input);
testRegex(forceUnicode(input));
