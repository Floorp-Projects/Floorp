// |jit-test| error: baz is null; skip-if: getBuildConfiguration('pbl')
//
// (skip if PBL enabled: it doesn't support the decompiler so doesn't give the
// specific error message)

var document = {getElementById: () => null};

(function() {
    const one = 1;

    function foo() { return one; }
    function bar() { return foo(); }

    var baz = document.getElementById("baz");
    baz.value;
})();
