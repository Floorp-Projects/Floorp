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
    {input: '{ "b": 2, "c": null }', expected: ['2', 'null', null]},
    {input: '{ "b": 2, "b": 1, "b": 4 }', expected: ['4', null]},
    {input: '{ "b": 2, "a": "1" }', expected: ['2', '"1"', null]},
    {input: '{ "b": { "c": 3 }, "a": 1 }', expected: ['3', null, '1', null]},
    // ARRAYS
    {input: '[]', expected: [null]},
    {input: '[1, 5, 2]', expected: ['1', '5', '2', null]},
    {input: '[1, null, 2]', expected: ['1', 'null', '2', null]},
    {input: '[1, {"a":2}, "7"]', expected: ['1', '2', null, '"7"', null]},
    {input: '[1, [2, [3, [4, 5], [6, 7], 8], 9], 10]', expected: ['1', '2', '3', '4', '5', null, '6', '7', null, '8', null, '9', null, '10', null]},
    {input: '[1, [2, [3, [4, 5, 6, 7, 8, 9, 10], []]]]', expected: ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10', null, null, null, null, null]},
    {input: '{"a": [1, {"b":2}, "7"], "c": 8}', expected: ['1', '2', null, '"7"', null, '8', null]},
];
for (const test of tests) {
    print("Testing " + JSON.stringify(test));
    actual = [];
    JSON.parse(test.input, reviver);
    assertDeepEq(actual, test.expected);
}

// If the constructed object is modified but the result of the modification is
// the same as the original, we should still provide the source
function replace_c_with_same_val(key, value, context) {
    if (key === "a") {
        this["c"] = "ABCDEABCDE";
    }
    if (key) {
       assertEq("source" in context, true);
    }
    return value;
}
JSON.parse('{ "a": "b", "c": "ABCDEABCDE" }', replace_c_with_same_val);

// rawJSON
function assertIsRawJson(rawJson, expectedRawJsonValue) {
    assertEq(null, Object.getPrototypeOf(rawJson));
    assertEq(true, Object.hasOwn(rawJson, 'rawJSON'));
    assertDeepEq(['rawJSON'], Object.getOwnPropertyNames(rawJson));
    assertDeepEq([], Object.getOwnPropertySymbols(rawJson));
    assertEq(expectedRawJsonValue, rawJson.rawJSON);
}

assertEq(true, Object.isFrozen(JSON.rawJSON('"shouldBeFrozen"')));
assertThrowsInstanceOf(() => JSON.rawJSON(), SyntaxError);
assertIsRawJson(JSON.rawJSON(1, 2), '1');

// isRawJSON
assertEq(false, JSON.isRawJSON());
assertEq(false, JSON.isRawJSON({}, {}));
assertEq(false, JSON.isRawJSON({}, JSON.rawJSON(2)));
assertEq(true, JSON.isRawJSON(JSON.rawJSON(1), JSON.rawJSON(2)));

if (typeof reportCompare == 'function')
    reportCompare(0, 0);