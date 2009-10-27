// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
if (typeof gczeal != 'undefined')
    gczeal(2);
if (typeof gc != 'undefined') {
    for (var i = 0; i < 10; i++) {
        var obj = {};
        for (var j = 0; j < 5; j++) {
            obj[Math.floor(Math.random() * 5)] = 0;
            gc();
        }
    }
}
reportCompare("no assertion failure", "no assertion failure", "bug 524743");
