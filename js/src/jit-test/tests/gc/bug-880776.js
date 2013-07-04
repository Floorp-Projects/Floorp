
gczeal(11);
var otherGlobal = newGlobal('new-compartment');
function test(str, arg, result)
{
        var fun = new Function('x', str);
            var code = "(function (x) { " + str + " })";
                var c = clone(otherGlobal.evaluate(code, {compileAndGo: false}));
                    assertEq(c.toSource(), eval(code).toSource());
}
test('return let (y) x;');
test('return let (x) "" + x;', 'unicorns', 'undefined');
test('return let (y = x) (y++, "" + y);', 'unicorns', 'NaN');
test('return let (y = 1) (y = x, y);');
test('return let ([] = x) x;');
test('return let ([, ] = x) x;');
test('return let ([, , , , ] = x) x;');
test('return let ([[]] = x) x;');
test('return let ([[[[[[[[[[[[[]]]]]]]]]]]]] = x) x;');
test('return let ([[], []] = x) x;');
test('return let ([[[[]]], [], , [], [[]]] = x) x;');
