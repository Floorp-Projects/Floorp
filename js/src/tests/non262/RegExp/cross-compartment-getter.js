const otherGlobal = newGlobal({newCompartment: true});

let regExp = otherGlobal.eval("/a(b|c)/iy");

function get(name) {
    const descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, name);
    return descriptor.get.call(regExp);
}

assertEq(get("flags"), "iy");
assertEq(get("global"), false);
assertEq(get("ignoreCase"), true);
assertEq(get("multiline"), false);
assertEq(get("dotAll"), false);
assertEq(get("source"), "a(b|c)");
assertEq(get("sticky"), true);
assertEq(get("unicode"), false);

regExp = otherGlobal.eval("new RegExp('', 'gu')");

assertEq(get("flags"), "gu");
assertEq(get("global"), true);
assertEq(get("ignoreCase"), false);
assertEq(get("multiline"), false);
assertEq(get("dotAll"), false);
assertEq(get("source"), "(?:)");
assertEq(get("sticky"), false);
assertEq(get("unicode"), true);

// Trigger escaping
regExp = otherGlobal.eval("new RegExp('a/b', '')");

assertEq(get("flags"), "");
assertEq(get("global"), false);
assertEq(get("ignoreCase"), false);
assertEq(get("multiline"), false);
assertEq(get("dotAll"), false);
assertEq(get("source"), "a\\/b");
assertEq(get("sticky"), false);
assertEq(get("unicode"), false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
