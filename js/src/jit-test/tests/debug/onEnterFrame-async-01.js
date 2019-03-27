// |jit-test| --enable-experimental-await-fix
// async functions fire onEnterFrame each time they resume, like generators

let g = newGlobal({newCompartment: true});
g.eval(`
    async function timeout(n) {
        for (let i = 0; i < n; i++) {
            await Promise.resolve(i);
        }
    }
    async function job() {
        let racer = timeout(5);
        await timeout(3);
        await racer;
    }
`);

let dbg = Debugger(g);
let log = "";
let nicknames = ["job", "t5", "t3"];
dbg.onEnterFrame = frame => {
    if (!("nickname" in frame))
        frame.nickname = nicknames.shift() || "FAIL";
    log += "(" + frame.nickname;
    frame.onPop = completion => { log += ")"; };
};

g.job();
drainJobQueue();
assertEq(log,
         "(job(t5)(t3))" +
         "(t5)(t3)".repeat(3) +
         "(t5)(job)(t5)(job)");
