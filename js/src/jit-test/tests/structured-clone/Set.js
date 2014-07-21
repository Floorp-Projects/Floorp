/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var set = new Set();
set.add(set);

var magic = deserialize(serialize(set));
assertEq(magic.size, 1);
assertEq(magic.values().next().value, magic);

var values = [
    "a", "\uDEFF", undefined, null, -3.5, true, false, NaN, 155, -2
]

set = new Set();
for (var value of values) {
    set.add(value)
}

magic = deserialize(serialize(set));
var i = 0;
for (value of magic) {
    assertEq(value, values[i++]);
}

assertEq([...set.keys()].toSource(), [...magic.keys()].toSource());
assertEq([...set.values()].toSource(), [...magic.values()].toSource());

var obj = {a: 1};
obj.set = new Set();
obj.set.add(obj);

magic = deserialize(serialize(obj));

assertEq(magic.set.values().next().value, magic);
assertEq(magic.a, 1);

set = new Set();
set.add(new Number(1));
set.add(new String("aaaa"));
set.add(new Date(NaN));

magic = deserialize(serialize(set));

values = magic.values();
assertEq(values.next().value.valueOf(), 1);
assertEq(values.next().value.valueOf(), "aaaa");
assertEq(values.next().value.valueOf(), NaN);
assertEq(values.next().done, true);

// Make sure expandos aren't cloned (Bug 1041172)
set = new Set();
set.a = "aaaaa";
magic = deserialize(serialize(set));
assertEq("a" in magic, false);
assertEq(Object.keys(magic).length, 0);

// Busted [[Prototype]] shouldn't matter
set = new Set();
Object.setPrototypeOf(set, null);
Set.prototype.add.call(set, "aaa");
magic = deserialize(serialize(set));
assertEq(magic.has("aaa"), true);
assertEq(magic.size, 1);

// Can't fuzz around with Set after it is cloned
obj = {
    a: new Set(),
    get b() {
        obj.a.delete("test");
        return "invoked";
    }
}
obj.a.add("test");
assertEq(obj.a.has("test"), true);
magic = deserialize(serialize(obj));
assertEq(obj.a.has("test"), false);
assertEq(magic.a.size, 1);
assertEq([...magic.a.keys()].toString(), "test");
assertEq(magic.b, "invoked");