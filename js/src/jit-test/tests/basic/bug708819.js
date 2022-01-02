/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

try {
    var e = new Error();
    e.name = e;
    "" + e;
} catch (e) {
    assertEq(e.message, 'too much recursion');
}
