/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var summary = "Recognize the function namespace anti-URI";
var expect = "1";
var actual;

function alert(s) {
    actual = String(s);
}

var a = "@mozilla.org/js/function";
try {
    a::alert("1");
} catch (e) {
    <></>.function::['x'];
    try {
        a::alert("2 " + e);
    } catch (e) {
        alert("3 " + e);
    }
}

reportCompare(expect, actual, summary);
