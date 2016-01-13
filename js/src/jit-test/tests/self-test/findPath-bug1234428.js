// 1. --ion-eager causes all functions to be compiled with IonMonkey before
//    executing them.
// 2. Registering the onIonCompilation hook on the Debugger causes
//    the JSScript of the function C to be wrapped in the Debugger compartment.
// 3. The JSScript hold a pointer to its function C.
// 4. The function C, hold its environment.
// 5. The environment holds the Object o.
g = newGlobal();
g.parent = this;
g.eval(`
  dbg = Debugger(parent);
  dbg.onIonCompilation = function () {};
`);

function foo() {
  eval(`
    var o = {};
    function C() {};
    new C;
    findPath(o, o);
  `);
}
foo();
