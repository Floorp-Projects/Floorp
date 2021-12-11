// Uncompleted scripts shouldn't be found in findScripts.

let g = newGlobal({newCompartment: true});
let dbg = new Debugger(g);

let message = "";
try {
  g.eval(`
(function nonLazyOuter() {
  (function nonLazyInner() {
    function lazy1() {
      function lazy2() {
      }
    }
  })();
})();

(function uncompletedNonLazy() {
  (function completedNonLazy() {
    function lazyInCompleted1() {
      function lazyInCompleted2() {
      }
    }
  })();
  // completedNonLazy and its inner scripts can be exposed to debugger since
  // the compilation for completedNonLazy finishes, even if the enclosing
  // uncompletedNonLazy fails to compile.
  switch (1) {
    ${"case 1:".repeat(2**16+1)}
  }
})();
`);
} catch (e) {
  message = e.message;
}

assertEq(message.includes("too many switch cases"), true,
         "Error for switch-case should be thrown," +
         "in order to test the case that uncompleted script is created");

let actualNames = [];
for (var script of dbg.findScripts()) {
  // Since all of above scripts can be GCed, we cannot check the set of
  // found scripts.
  if (script.displayName) {
    assertEq(script.displayName != "uncompletedNonLazy", true,
             "Uncompiled script shouldn't be found");
  }
}
