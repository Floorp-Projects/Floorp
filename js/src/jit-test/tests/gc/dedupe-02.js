// AutoStableStringChars needs to prevent the owner of its chars from being
// deduplicated, even if they are held by a different string.

gczeal(0);

function makeExtensibleStrFrom(str) {
  var left = str.substr(0, str.length/2);
  var right = str.substr(str.length/2, str.length);
  var ropeStr = left + right;
  return ensureLinearString(ropeStr);
}

// Make a string to deduplicate to.
var original = makeExtensibleStrFrom('{ "phbbbbbbbbbbbbbbttt!!!!??": [1] }\n\n');

// Construct D2 -> D1 -> base
var D2 = makeExtensibleStrFrom('{ "phbbbbbbbbbbbbbbttt!!!!??": [1] }');
var D1 = newRope(D2, '\n', {nursery: true});
ensureLinearString(D1);
var base = newRope(D1, '\n', {nursery: true});
ensureLinearString(base);

// Make an AutoStableStringChars(D2) and do a minor GC within it. (This will do
// a major GC, but it'll start out with a minor GC.) `base` would get
// deduplicated to `original`, if it weren't for AutoStableStringChars marking
// all of D2, D1, and base non-deduplicatable.

// The first time JSON.parse runs, it will create several (14 in my test) GC
// things before getting to the point where it does an allocation while holding
// the chars pointer. Get them out of the way now.
JSON.parse(D2);

// Cause a minor GC to happen during JSON.parse after AutoStableStringChars
// gives up its pointer.
schedulegc(1);
JSON.parse(D2);

// Access `D2` to verify that it is not using the deduplicated chars.
print(D2);
