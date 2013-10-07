// A Set iterator keeps the data alive.

load(libdir + "referencesVia.js");
load(libdir + "iteration.js");

var key = {};
var set = Set([key]);
var iter = set[std_iterator]();
referencesVia(iter, "**UNKNOWN SLOT 0**", set);
referencesVia(set, "key", key);
