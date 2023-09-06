// |jit-test| --fast-warmup; --no-threads

function foo() {
    let x = {};

    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 100; j++) {}
    }
    const g = this.newGlobal({sameZoneAs: this});
    const dbg = g.Debugger(this);
    dbg.getNewestFrame().eval("x = foo;");
}
foo();
