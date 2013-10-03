// An iterator keeps its data alive.

load(libdir + "iteration.js");

load(libdir + "referencesVia.js");
var key = {};

function test(obj, edgeName) {
    var iter = obj[std_iterator]();
    referencesVia(iter, "**UNKNOWN SLOT 0**", obj);
    referencesVia(obj, edgeName, key);
}

test([key],                 "element[0]");
test(Map([[key, 'value']]), "key");
test(Set([key]),            "key");
