// |reftest| skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('json-parse-with-source'))) shell-option(--enable-json-parse-with-source)

var actual;

function reviver(key, value, context) {
    assertEq(arguments.length, 3);
    assertEq("source" in context, true);
    actual = context["source"];
}

let tests = [
    // STRINGS
    {input: '""', expected: '""'},
    {input: '"str"', expected: '"str"'},
    {input: '"str" ', expected: '"str"'},
    {input: ' "str" ', expected: '"str"'},
    {input: ' " str"', expected: '" str"'},
    {input: '"\uD83D\uDE0A\u2764\u2FC1"', expected: '"\uD83D\uDE0A\u2764\u2FC1"'},
    // NUMBERS
    {input: '1', expected: '1'},
    {input: ' 1', expected: '1'},
    {input: '4.2', expected: '4.2'},
    {input: '4.2 ', expected: '4.2'},
    {input: '4.2000 ', expected: '4.2000'},
    {input: '4e2000 ', expected: '4e2000'},
    {input: '4.4e2000 ', expected: '4.4e2000'},
    {input: '9007199254740999', expected: '9007199254740999'},
    {input: '-31', expected: '-31'},
    {input: '-3.1', expected: '-3.1'},
    {input: ' -31 ', expected: '-31'},
    // BOOLEANS
    {input: 'true', expected: 'true'},
    {input: 'true ', expected: 'true'},
    {input: 'false', expected: 'false'},
    {input: ' false', expected: 'false'},
    // NULL
    {input: 'null', expected: 'null'},
    {input: ' null', expected: 'null'},
    {input: '\tnull ', expected: 'null'},
    {input: 'null\t', expected: 'null'},
];
for (const test of tests) {
    print("Testing " + JSON.stringify(test));
    JSON.parse(test.input, reviver);
    assertEq(actual, test.expected);
}

if (typeof reportCompare == 'function')
    reportCompare(0, 0);