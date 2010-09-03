// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

if (typeof gczeal != 'undefined' && typeof gc != 'undefined') {
    try
    {
        gczeal(2);
        var obj = {};
        for (var i = 0; i < 50; i++) {
            obj["_" + i] = 0;
            gc();
        }
    }
    catch(ex)
    {
    }
    gczeal(0);
}
reportCompare("no assertion failure", "no assertion failure", "bug 524743");
