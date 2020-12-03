load(libdir + "asserts.js");

registerModule("a", parseModule(`throw undefined`));

let b = registerModule("b", parseModule(`import "a";`));
let c = registerModule("c", parseModule(`import "a";`));

b.declarationInstantiation();
c.declarationInstantiation();

let count = 0;
try { b.evaluation() } catch (e) { count++; }
try { c.evaluation() } catch (e) { count++; }
assertEq(count, 2);
