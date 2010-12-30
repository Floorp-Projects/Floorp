/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = 'SyntaxError: duplicate argument is mixed with destructuring pattern';
var actual = 'No error';

var a = [];

// Test up to 200 to cover tunables such as js::PropertyTree::MAX_HEIGHT.
for (var i = 0; i < 200; i++) {
    a.push("b" + i);
    try {
        eval("(function ([" + a.join("],[") + "],a,a){})");
    } catch (e) {
        actual = '' + e;
    }
    assertEq(actual, expect);
}
reportCompare(0, 0, "ok");
