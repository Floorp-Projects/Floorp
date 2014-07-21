/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var map = new Map();
map.set("self", map);

var magic = deserialize(serialize(map));
assertEq(magic.get("self"), magic);
assertEq(magic.size, 1);

map = new Map();
map.set(map, "self");

magic = deserialize(serialize(map));
assertEq(magic.get(magic), "self");
assertEq(magic.size, 1);

var values = [
    "a", "\uDEFF", undefined, null, -3.5, true, false, NaN, 155, -2
]

map = new Map();
for (var value of values) {
    map.set(value, value);
}

magic = deserialize(serialize(map));
var i = 0;
for (value of magic) {
    assertEq(value[0], value[1]);
    assertEq(value[0], values[i++]);
}

assertEq([...map.keys()].toSource(), [...magic.keys()].toSource());
assertEq([...map.values()].toSource(), [...magic.values()].toSource());

var obj = {a: 1};
obj.map = new Map();
obj.map.set("obj", obj);

magic = deserialize(serialize(obj));

assertEq(magic.map.get("obj"), magic);
assertEq(magic.a, 1);

map = new Map();
map.set("a", new Number(1));
map.set("b", new String("aaaa"));
map.set("c", new Date(NaN));

magic = deserialize(serialize(map));

assertEq(magic.get("a").valueOf(), 1);
assertEq(magic.get("b").valueOf(), "aaaa");
assertEq(magic.get("c").valueOf(), NaN);

assertEq([...magic.keys()].toSource(), ["a", "b", "c"].toSource());

map = new Map();
map.set("x", new Map());
map.get("x").set("x", map);
map.get("x").set("b", null);

magic = deserialize(serialize(map));

assertEq(magic.get("x").get("x"), magic);
assertEq(magic.get("x").get("b"), null);

map = new Map()
map.set({a: 1}, "b");

magic = deserialize(serialize(map));

obj = [...magic.keys()][0];
assertEq(obj.a, 1);
assertEq(magic.get(obj), "b");

// Make sure expandos aren't cloned (Bug 1041172)
map = new Map();
map.a = "aaaaa";
magic = deserialize(serialize(map));
assertEq("a" in magic, false);
assertEq(Object.keys(magic).length, 0);

// Busted [[Prototype]] shouldn't matter
map = new Map();
Object.setPrototypeOf(map, null);
Map.prototype.set.call(map, "self", map);
magic = deserialize(serialize(map));
assertEq(magic.get("self"), magic);
assertEq(magic.size, 1);

// Can't fuzz around with Map after it is cloned
obj = {
    a: new Map(),
    get b() {
        obj.a.delete("test");
        return "invoked";
    }
}
obj.a.set("test", "hello");
assertEq(obj.a.has("test"), true);
magic = deserialize(serialize(obj));
assertEq(obj.a.has("test"), false);
assertEq(magic.a.size, 1);
assertEq(magic.a.get("test"), "hello");
assertEq([...magic.a.keys()].toString(), "test");
assertEq(magic.b, "invoked");