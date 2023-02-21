const dbg = newGlobal({ sameZoneAs: this }).Debugger(this);

async function* inspectingGenerator() {
    await undefined;

    const frame = dbg.getNewestFrame();
    const asyncPromise = frame.asyncPromise;
    assertEq(asyncPromise.getPromiseReactions().length, 0);
}

async function* emptyGenerator() {}

const gen = inspectingGenerator();
const inspectingGenPromise = gen.next();

const emptyGen = emptyGenerator();
// Close generator.
emptyGen.next();

// Creates a reaction record on the inspectingGenPromise which points to the
// closed emptyGen generator.
emptyGen.return(inspectingGenPromise);

// Execute the inspectingGenerator() code after `await`, which gets the
// promise reactions (potentially including the closed emptyGen generator)
drainJobQueue();