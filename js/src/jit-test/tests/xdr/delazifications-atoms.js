let s = `
function f1() { return "Atom_f1"; }
function f2() { return "Atom_f2"; }
function f3() { return "Atom_f3"; }

assertEq(f1(), "Atom_f1");
assertEq(f2(), "Atom_f2");
assertEq(f3(), "Atom_f3");
`;

let c = cacheEntry(s);
let g = newGlobal();
evaluate(c, {saveIncrementalBytecode:true});
evaluate(c, {global:g, loadBytecode:true});
