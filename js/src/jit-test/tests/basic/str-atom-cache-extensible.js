// Create an extensible string.
var extensible = "foo".repeat(50);
extensible += "bar";
extensible.indexOf("X");

// Ensure it's in the StringToAtomCache.
var obj = {};
obj[extensible] = 1;

// Make it a dependent string.
var other = extensible + "baz";
other.indexOf("X");

// Must still be in the cache.
obj[extensible] = 1;
