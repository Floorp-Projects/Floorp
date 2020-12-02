load(libdir + "asserts.js");

registerModule("a", parseModule(`throw undefined`));

let b = registerModule("b", parseModule(`import "a";`));
let c = registerModule("c", parseModule(`import "a";`));

b.declarationInstantiation();
c.declarationInstantiation();

(async () => {
  let count = 0;
  try { await b.evaluation() } catch (e) { count++; }
  try { await c.evaluation() } catch (e) { count++; }
  assertEq(count, 2);
})();

drainJobQueue();
