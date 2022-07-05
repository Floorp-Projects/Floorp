load(libdir + "asserts.js");

registerModule("a", parseModule(`throw undefined`));

let b = registerModule("b", parseModule(`import "a";`));
let c = registerModule("c", parseModule(`import "a";`));

moduleLink(b);
moduleLink(c);

(async () => {
  let count = 0;
  try { await moduleEvaluate(b) } catch (e) { count++; }
  try { await moduleEvaluate(c) } catch (e) { count++; }
  assertEq(count, 2);
})();

drainJobQueue();
