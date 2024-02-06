// |jit-test| --setpref=site_based_pretenuring=false; --setpref=tests.uint32-pref=123450

let names = getAllPrefNames();
assertEq(names.length > 0, true, "Expected at least one pref!");
assertEq(new Set(names).size, names.length, "Unexpected duplicate pref name");

for (let name of names) {
    let val = getPrefValue(name);
    assertEq(typeof val === "number" || typeof val === "boolean", true);
}

// Check that --setpref worked. Note: this is just an arbitrary pref. If we ever
// remove it, change this test to check a different one.
assertEq(getPrefValue("site_based_pretenuring"), false);
assertEq(getPrefValue("tests.uint32-pref"), 123450);

// Must throw an exception for unknown pref names.
let ex;
try {
    getPrefValue("some.invalid.pref");
} catch (e) {
    ex = e;
}
assertEq(ex.toString(), "Error: invalid pref name");
