const t = RegExp.prototype;

let properties = "toString,compile,exec,test," +
                 "flags,dotAll,global,hasIndices,ignoreCase,multiline,source,sticky,unicode," +
                 "constructor," +
                 "Symbol(Symbol.match),Symbol(Symbol.replace),Symbol(Symbol.search),Symbol(Symbol.split)";
if (Object.prototype.toSource) {
    properties = "toSource," + properties;
}
if (Symbol.matchAll) {
    properties += ",Symbol(Symbol.matchAll)";
}
assertEqArray(Reflect.ownKeys(t).map(String).sort(), properties.split(",").sort());


// Invoking getters on the prototype should not throw
function getter(name) {
    return Object.getOwnPropertyDescriptor(t, name).get.call(t);
}

assertEq(getter("flags"), "");
assertEq(getter("global"), undefined);
assertEq(getter("ignoreCase"), undefined);
assertEq(getter("multiline"), undefined);
assertEq(getter("source"), "(?:)");
assertEq(getter("sticky"), undefined);
assertEq(getter("unicode"), undefined);

assertEq(t.toString(), "/(?:)/");

// The methods don't work with the prototype
assertThrowsInstanceOf(() => t.compile("b", "i"), TypeError);
assertThrowsInstanceOf(() => t.test("x"), TypeError);
assertThrowsInstanceOf(() => t.exec("x"), TypeError);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
