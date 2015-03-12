// Order of Array.of operations.

load(libdir + "asserts.js");

var log;

var dstdata = [];
var dst = new Proxy(dstdata, {
    defineProperty: function (t, name, desc) {
        log.push(["def", name, desc.value]);
        return true;
    },
    set: function (t, name, value) {
        log.push(["set", name, value]);
        return true;
    }
});

function Troop() {
    return dst;
}
Troop.of = Array.of;

log = [];
assertEq(Troop.of("monkeys", "baboons", "kangaroos"), dst);
assertDeepEq(log, [
    ["def", "0", "monkeys"],
    ["def", "1", "baboons"],
    ["def", "2", "kangaroos"],
    ["set", "length", 3]
]);

