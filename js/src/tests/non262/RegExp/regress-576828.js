var re = /(z\1){3}/;
var str = 'zzz';
var actual = re.exec(str);
var expected = makeExpectedMatch(['zzz', 'z'], 0, str);
checkRegExpMatch(actual, expected);

if (typeof reportCompare == 'function')
    reportCompare(true, true);
