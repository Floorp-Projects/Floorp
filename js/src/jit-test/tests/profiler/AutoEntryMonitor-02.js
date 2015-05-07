// Nested uses of AutoEntryMonitor should behave with decorum.

load(libdir + 'array-compare.js');

function Cobb() {
  var twoShot = { toString: function Saito() { return Object; },
                  valueOf: function Fischer() { return "Ariadne"; } };
  assertEq(arraysEqual(entryPoints({ ToString: twoShot }),
                       [ "Saito", "Fischer" ]), true);
}

assertEq(arraysEqual(entryPoints({ function: Cobb }), ["Cobb"]), true);
