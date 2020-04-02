// |jit-test| error: baz is null; --no-warp
// Disable WarpBuilder because the expression decompiler is not used for Ion
// frames currently. See bug 831120.

var document = {getElementById: () => null};

(function() {
    const one = 1;

    function foo() { return one; }
    function bar() { return foo(); }

    var baz = document.getElementById("baz");
    baz.value;
})();
