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
