// |jit-test| skip-if: !hasFunction.stringRepresentation

// Test handling of tenured dependent strings pointing to nursery base strings.

gczeal(0);

function makeExtensibleStrFrom(str) {
  var left = str.substr(0, str.length/2);
  var right = str.substr(str.length/2, str.length);
  var ropeStr = left + right;
  return ensureLinearString(ropeStr);
}

function repr(s) {
  return JSON.parse(stringRepresentation(s));
}

function dependsOn(s1, s2) {
  const rep1 = JSON.parse(stringRepresentation(s1));
  const rep2 = JSON.parse(stringRepresentation(s2));
  return rep1.base && rep1.base.address == rep2.address;
}

// Make a string to deduplicate to.
var original = makeExtensibleStrFrom('abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklm');

// Construct T1 -> Nbase.
var Nbase = makeExtensibleStrFrom('abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklm');
var T1 = newDependentString(Nbase, 0, 60, { tenured: true });

// Get prevented from creating T2 -> T1 -> Nbase
// (will be T2 -> Nbase instead to avoid dependency chains).
var T2 = newDependentString(T1, 30, { tenured: true });

assertEq(dependsOn(T2, Nbase), "expect: T2 -> base");

// Construct T1 -> Ndep1 (was Nbase) -> Nbase2.
var Nbase2 = newRope(Nbase, "ABC");
ensureLinearString(Nbase2);
var Ndep1 = Nbase;

assertEq(dependsOn(T1, Ndep1), "expect: T1 -> Ndep1");
assertEq(dependsOn(Ndep1, Nbase2), "expect: Ndep1 -> Nbase2");

// Fail to construct T3 -> Tbase3 -> Nbase4. It will refuse because T3 would be using
// chars from Nbase4 that can't be updated since T3 is not in the store buffer. Instead,
// it will allocate a new buffer for the rope root, leaving Tbase3 alone and keeping
// T3 -> Tbase3.
var Tbase3 = makeExtensibleStrFrom('abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklm');
minorgc();
var T3 = newDependentString(Tbase3, 0, 30, { tenured: true });
var Nbase4 = newRope(Tbase3, "DEF");
ensureLinearString(Nbase4);
assertEq(repr(Tbase3).isTenured, true, "Tbase3 is tenured");
assertEq(repr(Tbase3).flags.includes("EXTENSIBLE"), true, "Tbase3 is extensible");
assertEq(repr(Nbase4).flags.includes("DEPENDENT_BIT"), false, "expect: Nbase4 is not a dependent string")
assertEq(repr(T3).flags.includes("DEPENDENT_BIT"), true, "expect: T3 is a dependent string")
assertEq(dependsOn(T3, Tbase3), "expect: T3 -> Tbase3");

function bug1879918() {
  const s = JSON.parse('["abcdefabcdefabcdefabcdefabcdefabcdefabcdef"]')[0];
  const dep = newDependentString(s, 1, { tenured: true });
  minorgc();
  assertEq(dep, "bcdefabcdefabcdefabcdefabcdefabcdefabcdef");
}
bug1879918();
