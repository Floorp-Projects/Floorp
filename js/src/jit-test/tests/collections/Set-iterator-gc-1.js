// A Set iterator keeps the data alive.

load(libdir + "referencesVia.js");
var key = {};
var set = Set([key]);
var iter = set.iterator();
referencesVia(iter, "**UNKNOWN SLOT 0**", set);
referencesVia(set, "key", key);
