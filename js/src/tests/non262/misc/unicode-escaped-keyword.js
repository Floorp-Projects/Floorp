/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function throws(code) {
    var type;
    try {
        eval(code);
    } catch (ex) {
        type = ex.name;
    }
    assertEq(type, 'SyntaxError');
}

var s = '\\u0073';
throws('var thi' + s);
throws('switch (' + s + 'witch) {}')
throws('var ' + s + 'witch');

if (typeof reportCompare == 'function')
    reportCompare(true, true);
