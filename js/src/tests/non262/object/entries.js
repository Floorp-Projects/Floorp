/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if ("entries" in Object) {
    assertEq(Object.entries.length, 1);

    var o, entries;

    o = { a: 3, b: 2 };
    entries = Object.entries(o);
    assertDeepEq(entries, [["a", 3], ["b", 2]]);

    o = { get a() { return 17; }, b: 2 };
    entries = Object.entries(o),
    assertDeepEq(entries, [["a", 17], ["b", 2]]);

    o = { __iterator__: function() { throw new Error("non-standard __iterator__ called?"); } };
    entries = Object.entries(o);
    assertDeepEq(entries, [["__iterator__", o.__iterator__]]);

    o = { a: 1, b: 2 };
    delete o.a;
    o.a = 3;
    entries = Object.entries(o);
    assertDeepEq(entries, [["b", 2], ["a", 3]]);

    o = [0, 1, 2];
    entries = Object.entries(o);
    assertDeepEq(entries, [["0", 0], ["1", 1], ["2", 2]]);

    o = /./.exec("abc");
    entries = Object.entries(o);
    assertDeepEq(entries, [["0", "a"], ["index", 0], ["input", "abc"]]);

    o = { a: 1, b: 2, c: 3 };
    delete o.b;
    o.b = 5;
    entries = Object.entries(o);
    assertDeepEq(entries, [["a", 1], ["c", 3], ["b", 5]]);

    function f() { }
    f.prototype.p = 1;
    o = new f();
    o.g = 1;
    entries = Object.entries(o);
    assertDeepEq(entries, [["g", 1]]);

    var o = {get a() {delete this.b; return 1}, b: 2, c: 3};
    entries = Object.entries(o);
    assertDeepEq(entries, [["a", 1], ["c", 3]]);

    assertThrowsInstanceOf(() => Object.entries(), TypeError);
    assertThrowsInstanceOf(() => Object.entries(undefined), TypeError);
    assertThrowsInstanceOf(() => Object.entries(null), TypeError);

    assertDeepEq(Object.entries(1), []);
    assertDeepEq(Object.entries(true), []);
    if (typeof Symbol === "function")
        assertDeepEq(Object.entries(Symbol("foo")), []);

    assertDeepEq(Object.entries("foo"), [["0", "f"], ["1", "o"], ["2", "o"]]);

    entries = Object.entries({
        get a(){
            Object.defineProperty(this, "b", {enumerable: false});
            return "A";
        },
        b: "B"
    });
    assertDeepEq(entries, [["a", "A"]]);

    let ownKeysCallCount = 0;
    let getOwnPropertyDescriptorCalls = [];
    let target = { a: 1, b: 2, c: 3 };
    o = new Proxy(target, {
        ownKeys() {
            ownKeysCallCount++;
            return ["c", "a"];
        },
        getOwnPropertyDescriptor(target, key) {
            getOwnPropertyDescriptorCalls.push(key);
            return Object.getOwnPropertyDescriptor(target, key);
        }
    });
    entries = Object.entries(o);
    assertEq(ownKeysCallCount, 1);
    assertDeepEq(entries, [["c", 3], ["a", 1]]);
    assertDeepEq(getOwnPropertyDescriptorCalls, ["c", "a"]);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
