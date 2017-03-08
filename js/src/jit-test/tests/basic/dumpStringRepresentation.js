// Try the dumpStringRepresentation shell function on various types of
// strings, and make sure it doesn't crash.

if (typeof dumpStringRepresentation !== 'function')
  quit(0);

print("Empty string:");
dumpStringRepresentation("");

print("\nResult of coercion to string:");
dumpStringRepresentation();

print("\ns = Simple short atom:");
var s = "xxxxxxxx";
dumpStringRepresentation(s);

// Simple non-atom flat.
print("\ns + s: Non-atom flat:");
var s2 = s + s;
dumpStringRepresentation(s2);

print("\nNon-Latin1 flat:");
var j = "渋谷区";
dumpStringRepresentation(j);

print("\nt = Non-inline atom:");
var t = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // 31 characters
dumpStringRepresentation(t);

print("\nr1 = t + s: Simple rope:");
var r1 = t + s;
dumpStringRepresentation(r1);

// Flatten that rope, and re-examine the representations of both the result and
// its former leaves. This should be an extensible string.
print("\nr1, former rope after flattening, now extensible:");
r1.match(/x/);
dumpStringRepresentation(r1);

print("\nt, s: Original leaves, representation unchanged:");
dumpStringRepresentation(t);
dumpStringRepresentation(s);

// Create a new rope with the extensible string as its left child.
print("\nr2 = r1 + s: Rope with extensible leftmost child:");
var r2 = r1 + s;
dumpStringRepresentation(r2);

// Flatten that; this should re-use the extensible string's buffer.
print("\nr2: flattened, stole r1's buffer:");
r2.match(/x/);
dumpStringRepresentation(r2);

print("\nr1: mutated into a dependent string:");
dumpStringRepresentation(r1);

print("\nr3 = r2 + s: a new rope with an extensible leftmost child:");
r3 = r2 + s;
r3.match(/x/);
dumpStringRepresentation(r3);

print("\nr2: now mutated into a dependent string");
dumpStringRepresentation(r2);

print("\nr1: now a doubly-dependent string, because of r2's mutation:");
dumpStringRepresentation(r1);

print("\nt, s: Original leaves, representation unchanged:");
dumpStringRepresentation(t);
dumpStringRepresentation(s);
