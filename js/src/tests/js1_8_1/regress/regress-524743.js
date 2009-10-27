// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
if (typeof gczeal != 'undefined')
    gczeal(2);
if (typeof gc != 'undefined') {
    var obj = {};
    for (var i = 0; i < 50; i++) {
        obj["_" + i] = 0;
        gc();
    }
}
reportCompare("no assertion failure", "no assertion failure", "bug 524743");
