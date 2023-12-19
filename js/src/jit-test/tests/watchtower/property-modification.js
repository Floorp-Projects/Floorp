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
    let o = { a: 10 };
    addWatchtowerTarget(o);

    // modify-prop: a
    o.a = 12;
    let p = { a: 15 };

    // modify-prop: a
    Object.assign(o, p);

    // change-prop: a
    Object.defineProperty(o, "a", { value: 19 });
    let log = getLogString(o);

    assertEq(log,
        `modify-prop: a
modify-prop: a
change-prop: a`);
}

for (var i = 0; i < 20; i++) {
    console.log(`Iteration ${i}`);
    testBasic();
}
