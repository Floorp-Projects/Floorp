/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// `this` must be object coercable.

for (let badThis of [null, undefined]) {
    assertThrowsInstanceOf(() => {
        String.prototype.padStart.call(badThis, 42, "oups");
    }, TypeError);

    assertThrowsInstanceOf(() => {
        String.prototype.padEnd.call(badThis, 42, "oups");
    }, TypeError);
}

let proxy = new Proxy({}, {
get(t, name) {
  if (name === Symbol.toPrimitive || name === "toString") return;
  if (name === "valueOf") return () => 42;
  throw "This should not be reachable";
}
});

assertEq("42bloop", String.prototype.padEnd.call(proxy, 7, "bloopie"));

// maxLength must convert to an integer

assertEq("lame", "lame".padStart(0, "foo"));
assertEq("lame", "lame".padStart(0.1119, "foo"));
assertEq("lame", "lame".padStart(-0, "foo"));
assertEq("lame", "lame".padStart(NaN, "foo"));
assertEq("lame", "lame".padStart(-1, "foo"));
assertEq("lame", "lame".padStart({toString: () => 0}, "foo"));

assertEq("lame", "lame".padEnd(0, "foo"));
assertEq("lame", "lame".padEnd(0.1119, "foo"));
assertEq("lame", "lame".padEnd(-0, "foo"));
assertEq("lame", "lame".padEnd(NaN, "foo"));
assertEq("lame", "lame".padEnd(-1, "foo"));
assertEq("lame", "lame".padEnd({toString: () => 0}, "foo"));

assertThrowsInstanceOf(() => {
    "lame".padStart(Symbol("9900"), 0);
}, TypeError);

assertThrowsInstanceOf(() => {
    "lame".padEnd(Symbol("9900"), 0);
}, TypeError);

// The fill argument must be string coercable.

assertEq("nulln.", ".".padStart(6, null));
assertEq(".nulln", ".".padEnd(6, null));

assertEq("[obje.", ".".padStart(6, {}));
assertEq(".[obje", ".".padEnd(6, {}));

assertEq("1,2,3.", ".".padStart(6, [1, 2, 3]));
assertEq(".1,2,3", ".".padEnd(6, [1, 2, 3]));

assertEq("aaaaa.", ".".padStart(6, {toString: () => "a"}));
assertEq(".aaaaa", ".".padEnd(6, {toString: () => "a"}));

// undefined is converted to " "

assertEq("     .", ".".padStart(6, undefined));
assertEq(".     ", ".".padEnd(6, undefined));

assertEq("     .", ".".padStart(6));
assertEq(".     ", ".".padEnd(6));

// The empty string has no effect

assertEq("Tilda", "Tilda".padStart(100000, ""));
assertEq("Tilda", "Tilda".padEnd(100000, ""));

assertEq("Tilda", "Tilda".padStart(100000, {toString: () => ""}));
assertEq("Tilda", "Tilda".padEnd(100000, {toString: () => ""}));

// Test repetition against a bruteforce implementation

let filler = "space";
let truncatedFiller = "";
for (let i = 0; i < 2500; i++) {
    truncatedFiller += filler[i % filler.length];
    assertEq(truncatedFiller + "goto", "goto".padStart(5 + i, filler));
    assertEq("goto" + truncatedFiller, "goto".padEnd(5 + i, filler));
}

// [Argument] Length

assertEq(1, String.prototype.padStart.length)
assertEq(1, String.prototype.padEnd.length)

if (typeof reportCompare === "function")
    reportCompare(true, true);

