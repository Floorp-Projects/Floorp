setWatchtowerCallback(function(kind, object, extra) {
    assertEq(object, o);
    if (typeof extra === "symbol") {
        extra = "<symbol>";
    }
    log.push(kind + (extra ? ": " + extra : ""));
});

let log;
let o;

function testBasic() {
    log = [];
    o = {};
    addWatchtowerTarget(o);

    o.x = 1;
    o.y = 2;
    o[Symbol()] = 3;
    Object.defineProperty(o, "foo", {enumerable: false, configurable: false, value: 1});
    Object.defineProperty(o, "getter", {get() { return 123; }});
    o.__proto__ = {};
    o.__proto__ = null;
    o[12345] = 1; // Sparse elements are included for now.
    delete o.x;
    Object.defineProperty(o, "y", {writable: false});
    Object.defineProperty(o, "x", {value: 0});
    Object.seal(o);
    Object.freeze(o);

    assertEq(log.join("\n"),
`add-prop: x
add-prop: y
add-prop: <symbol>
add-prop: foo
add-prop: getter
proto-change
proto-change
add-prop: 12345
remove-prop: x
change-prop: y
add-prop: x
freeze-or-seal
freeze-or-seal`);
}
for (var i = 0; i < 20; i++) {
    testBasic();
}

// Object.assign edge case.
function testAssign() {
    o = {};
    o.x = 1;
    delete o.x;
    let from = {x: 1, y: 2, z: 3, a: 4};
    log = [];
    addWatchtowerTarget(o);
    addWatchtowerTarget(from);
    Object.assign(o, from);
    assertEq(log.join("\n"), "add-prop: x\nadd-prop: y\nadd-prop: z\nadd-prop: a");
}
testAssign();

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

// array.length is a custom data property.
function testCustomDataProp() {
    o = [];
    log = [];
    addWatchtowerTarget(o);

    Object.defineProperty(o, "length", {writable: false});
    assertEq(log.join("\n"), "change-prop: length");
}
for (var i = 0; i < 20; i++) {
    testCustomDataProp();
}
