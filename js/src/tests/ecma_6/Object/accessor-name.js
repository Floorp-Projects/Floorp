function name(obj, property, get) {
    let desc = Object.getOwnPropertyDescriptor(obj, property);
    return (get ? desc.get : desc.set).name;
}

assertEq(name({get a() {}}, "a", true), "get a");
assertEq(name({set a(v) {}}, "a", false), "set a");

assertEq(name({get 123() {}}, "123", true), "get 123");
assertEq(name({set 123(v) {}}, "123", false), "set 123");

assertEq(name({get case() {}}, "case", true), "get case");
assertEq(name({set case(v) {}}, "case", false), "set case");

assertEq(name({get get() {}}, "get", true), "get get");
assertEq(name({set set(v) {}}, "set", false), "set set");

let o = {get a() { }, set a(v) {}};
assertEq(name(o, "a", true), "get a");
assertEq(name(o, "a", false), "set a");

o = {get 123() { }, set 123(v) {}}
assertEq(name(o, "123", true), "get 123");
assertEq(name(o, "123", false), "set 123");

o = {get case() { }, set case(v) {}}
assertEq(name(o, "case", true), "get case");
assertEq(name(o, "case", false), "set case");

// Congratulations on implementing these!
assertEq(name({get ["a"]() {}}, "a", true), "");
assertEq(name({get [123]() {}}, "123", true), "");
assertEq(name({set ["a"](v) {}}, "a", false), "");
assertEq(name({set [123](v) {}}, "123", false), "");

reportCompare(true, true);
