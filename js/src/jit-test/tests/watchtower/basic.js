function getLogString(obj) {
    let log = getWatchtowerLog();
    return log.map(item => {
        assertEq(item.object, obj);
        if (typeof item.extra === "symbol") {
            item.extra = "<symbol>";
        }
        return item.kind + (item.extra ? ": " + item.extra : "");
    }).join("\n");
}

function testBasic() {
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
    delete o.x;
    Object.defineProperty(o, "y", {writable: false});
    Object.defineProperty(o, "x", {value: 0});
    Object.seal(o);
    Object.freeze(o);

    let log = getLogString(o);
    assertEq(log,
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
    let o = {};
    o.x = 1;
    delete o.x;
    let from = {x: 1, y: 2, z: 3, a: 4};
    addWatchtowerTarget(o);
    addWatchtowerTarget(from);
    Object.assign(o, from);
    let log = getLogString(o);
    assertEq(log, "add-prop: x\nadd-prop: y\nadd-prop: z\nadd-prop: a");
}
testAssign();

function testJit() {
    for (var i = 0; i < 20; i++) {
        let o = {};
        addWatchtowerTarget(o);
        o.x = 1;
        o.y = 2;
        let log = getLogString(o);
        assertEq(log, "add-prop: x\nadd-prop: y");
    }
}
testJit();

// array.length is a custom data property.
function testCustomDataProp() {
    let o = [];
    addWatchtowerTarget(o);

    Object.defineProperty(o, "length", {writable: false});
    let log = getLogString(o);
    assertEq(log, "change-prop: length");
}
for (var i = 0; i < 20; i++) {
    testCustomDataProp();
}
