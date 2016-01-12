// JS_IS_CONSTRUCTING
var g = newGlobal();
do {
  new g.String(); // jit::CreateThis passes JS_IS_CONSTRUCTING
} while (!inIon());

// JS_UNINITIALIZED_LEXICAL
function dontAbortWholeCompilation() {
    class B {};
    class D extends B {
        constructor() { super(); }
    };

    return D;
}
var classImpl = dontAbortWholeCompilation();

do {
  new classImpl(); // jit::CreateThis passes JS_UNINITIALIZED_LEXICAL
} while (!inIon());
