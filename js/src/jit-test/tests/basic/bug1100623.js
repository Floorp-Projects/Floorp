// |jit-test| error: baz is null
var document = {getElementById: () => null};

(function() {
    const one = 1;

    function foo() { return one; }
    function bar() { return foo(); }

    var baz = document.getElementById("baz");
    baz.value;
})();
