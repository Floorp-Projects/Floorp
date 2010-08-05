// Turn on strict mode and warnings-as-errors mode.
if (options().split().indexOf('strict') == -1)
    options('strict');
if (options().split().indexOf('werror') == -1)
    options('werror');

function expectSyntaxError(stmt) {
    print(stmt);
    var result = 'no error';
    try {
        Function(stmt);
    } catch (exc) {
        result = exc.constructor.name;
    }
    assertEq(result, 'SyntaxError');
}

function test(expr) {
    // Without extra parentheses, expect an error.
    expectSyntaxError('if (' + expr + ') ;');

    // Extra parentheses silence the warning/error.
    Function('if ((' + expr + ')) ;');
}

// Overparenthesized assignment in a condition should not be a strict error.
test('a = 0');
test('a = (f(), g)');
test('a = b || c > d');
reportCompare('passed', 'passed', 'Overparenthesized assignment in a condition should not be a strict error.');

