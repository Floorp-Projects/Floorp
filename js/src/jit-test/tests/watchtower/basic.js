setWatchtowerCallback(function(kind, object, extra) {
    assertEq(object, o);
    if (typeof extra === "symbol") {
        extra = "<symbol>";
    }
    log.push(kind + (extra ? ": " + extra : ""));
});

let log = [];
let o = {};
addWatchtowerTarget(o);

o.x = 1;
o.y = 2;
o[Symbol()] = 3;
Object.defineProperty(o, "foo", {enumerable: false, configurable: false, value: 1});
Object.defineProperty(o, "getter", {get() { return 123; }});
o.__proto__ = {};
o.__proto__ = null;
o[12345] = 1; // Sparse elements are included for now.

assertEq(log.join("\n"),
`add-prop: x
add-prop: y
add-prop: <symbol>
add-prop: foo
add-prop: getter
proto-change
proto-change
add-prop: 12345`);

// Object.assign edge case.
o = {};
o.x = 1;
delete o.x;
let from = {x: 1, y: 2, z: 3, a: 4};
log = [];
addWatchtowerTarget(o);
addWatchtowerTarget(from);
Object.assign(o, from);
assertEq(log.join("\n"), "add-prop: x\nadd-prop: y\nadd-prop: z\nadd-prop: a");

function testJit() {
    for (var i = 0; i < 20; i++) {
        o = {};
        addWatchtowerTarget(o);
        log = [];
        o.x = 1;
        o.y = 2;
        assertEq(log.join("\n"), "add-prop: x\nadd-prop: y");
    }
}
testJit();
