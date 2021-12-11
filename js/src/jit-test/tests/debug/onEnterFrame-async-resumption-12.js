// If the async function's promise is already resolved, any attempt to return
// a differerent return value gets ignored.

let g = newGlobal({newCompartment: true});
g.eval(`
    async function f() {
        return "ok";
    }
`);

let dbg = new Debugger(g);

let hits = 0;
dbg.onEnterFrame = frame => {
    frame.onPop = () => {
        hits += 1;

        // Normal functions can override the return value, but async functions
        // have already resolved their promise, so this return request will get
        // ignored.
        return {return: "FAIL"};
    };
};

g.f().then(x => {
    assertEq(hits, 1);
    assertEq(x, "ok");
});
