/* Back reference is actually a forwards reference. */
var re = /(\2(a)){2}/;
var str = 'aaa';
var actual = re.exec(str);
var expected = makeExpectedMatch(['aa', 'a', 'a'], 0, str);
checkRegExpMatch(actual, expected);

if (typeof reportCompare === 'function')
    reportCompare(true, true);
