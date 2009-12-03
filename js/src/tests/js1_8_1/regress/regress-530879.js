/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */
gTestfile = 'regress-530879';
function f(a, b, c, d) {
    yield arguments.length;
}
reportCompare(0, f().next(), "bug 530879");
