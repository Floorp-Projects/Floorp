/* Resetting of inner capture groups across quantified capturing parens. */
var re = /(?:(f)(o)(o)|(b)(a)(r))*/;
var str = 'foobar';
var actual = re.exec(str);
var expected = makeExpectedMatch(['foobar', undefined, undefined, undefined, 'b', 'a', 'r'], 0, str);
checkRegExpMatch(actual, expected);

if (typeof reportCompare === 'function')
    reportCompare(true, true);
