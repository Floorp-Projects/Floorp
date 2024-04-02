newGlobal();
const g = newGlobal({
    "newCompartment": true,
});
const p1 = g.eval(`
Promise.resolve();
`);
const p2 = p1.then();
nukeAllCCWs();
ignoreUnhandledRejections();
Promise.resolve = function() {
  return p2;
};
let caught = false;
Promise.allSettled([1]).catch(e => {
  caught = true;
  assertEq(e.message.includes("dead object"), true);
});
drainJobQueue();
assertEq(caught, true);
