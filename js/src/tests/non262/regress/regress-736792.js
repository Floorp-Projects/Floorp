/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (typeof options == "function") {
    var opts = options();
    if (!/\bstrict_mode\b/.test(opts))
        options("strict_mode");
}

var ok = false;
try {
    eval('foo = true;');
} catch (e) {
    if (/^ReferenceError:/.test(e.toString()))
        ok = true;
}

if (ok)
    reportCompare(0, 0, "ok");
else
    reportCompare(true, false, "this should have thrown a ReferenceError");
