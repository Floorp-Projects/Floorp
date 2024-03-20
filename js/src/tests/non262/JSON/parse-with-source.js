// |reftest| skip-if(!(this.hasOwnProperty('getBuildConfiguration')&&getBuildConfiguration('json-parse-with-source'))) shell-option(--enable-json-parse-with-source)

var actual;

function reviver(key, value, context) {
    assertEq(arguments.length, 3);
    if ("source" in context) {
        actual.push(context["source"]);
    } else { // objects and arrays have no "source"
        actual.push(null);
    }
}

let tests = [
    // STRINGS
    {input: '""', expected: ['""']},
    {input: '"str"', expected: ['"str"']},
    {input: '"str" ', expected: ['"str"']},
    {input: ' "str" ', expected: ['"str"']},
    {input: ' " str"', expected: ['" str"']},
    {input: '"\uD83D\uDE0A\u2764\u2FC1"', expected: ['"\uD83D\uDE0A\u2764\u2FC1"']},
    // NUMBERS
    {input: '1', expected: ['1']},
    {input: ' 1', expected: ['1']},
    {input: '4.2', expected: ['4.2']},
    {input: '4.2 ', expected: ['4.2']},
    {input: '4.2000 ', expected: ['4.2000']},
    {input: '4e2000 ', expected: ['4e2000']},
    {input: '4.4e2000 ', expected: ['4.4e2000']},
    {input: '9007199254740999', expected: ['9007199254740999']},
    {input: '-31', expected: ['-31']},
    {input: '-3.1', expected: ['-3.1']},
    {input: ' -31 ', expected: ['-31']},
    // BOOLEANS
    {input: 'true', expected: ['true']},
    {input: 'true ', expected: ['true']},
    {input: 'false', expected: ['false']},
    {input: ' false', expected: ['false']},
    // NULL
    {input: 'null', expected: ['null']},
    {input: ' null', expected: ['null']},
    {input: '\tnull ', expected: ['null']},
    {input: 'null\t', expected: ['null']},
    // OBJECTS
    {input: '{ }', expected: [null]},
    {input: '{ "4": 1 }', expected: ['1', null]},
    {input: '{ "a": 1 }', expected: ['1', null]},
    {input: '{ "b": 2, "a": 1 }', expected: ['2', '1', null]},
    {input: '{ "b": 2, "1": 1 }', expected: ['1', '2', null]},
    {input: '{ "b": 2, "b": 1, "b": 4 }', expected: ['4', null]},
    {input: '{ "b": 2, "a": "1" }', expected: ['2', '"1"', null]},
    {input: '{ "b": { "c": 3 }, "a": 1 }', expected: ['3', null, '1', null]},
];
for (const test of tests) {
    print("Testing " + JSON.stringify(test));
    actual = [];
    JSON.parse(test.input, reviver);
    assertDeepEq(actual, test.expected);
}

if (typeof reportCompare == 'function')
    reportCompare(0, 0);