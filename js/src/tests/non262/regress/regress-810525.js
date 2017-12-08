/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = '';
var actual = '';

function test(s) {
    var threw = false;
    try {
        eval(s);
    } catch (e) {
        assertEq(e.message.indexOf('(intermediate value)'), -1);
        threw = true;
    } finally {
        assertEq(threw, true);
    }
}

test("({p:1, q:2}).m()");
test("[].m()");
test("[1,2,3].m()");
test("/hi/.m()");

reportCompare(0, 0, "ok");
