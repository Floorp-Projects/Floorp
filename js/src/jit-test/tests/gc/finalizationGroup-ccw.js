// |jit-test| --enable-weak-refs

// Test combinations of arguments in different compartments.

gczeal(0);

let holdings = [];
let group = new FinalizationGroup(iterator => {
    for (const holding of iterator) {
        holdings.push(holding);
    }
});

function ccwToObject() {
    return evalcx('({})', newGlobal({newCompartment: true}));
}

function ccwToGroup() {
    let global = newGlobal({newCompartment: true});
    global.holdings = holdings;
    return global.eval(
        `new FinalizationGroup(iterator => holdings.push(...iterator))`);
}

function incrementalGC() {
    startgc(1);
    while (gcstate() !== "NotActive") {
        gcslice(1000);
    }
}

for (let w of [false, true]) {
    for (let x of [false, true]) {
        for (let y of [false, true]) {
            for (let z of [false, true]) {
                let g = w ? ccwToGroup(w) : group;
                let target = x ? ccwToObject() : {};
                let holding = y ? ccwToObject() : {};
                let token = z ? ccwToObject() : {};
                g.register(target, holding, token);
                g.unregister(token);
                g.register(target, holding, token);
                target = undefined;
                incrementalGC();
                holdings.length = 0; // Clear, don't replace.
                g.cleanupSome();
                assertEq(holdings.length, 1);
                assertEq(holdings[0], holding);
            }
        }
    }
}
