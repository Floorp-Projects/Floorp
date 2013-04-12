// vim: set ts=4 sw=4 tw=99 et:

function f() {
    return [[], [], [], []];
}

function setelem(obj, key, val) {
    obj[key] = val;
}

// Generate IC
setelem([], 0, "hi");
setelem([], 0, "hi");

var arrays = f();

var evil = { };
Object.defineProperty(evil, "1", { set: function (v) { this.ham = v; } });
Array.prototype.__proto__ = evil;

var k = arrays[0];
setelem(k, 1, "yam");
assertEq(k.ham, "yam");

