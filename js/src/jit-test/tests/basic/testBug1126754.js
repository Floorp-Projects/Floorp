// |jit-test| error:SyntaxError

(function() {
    with ({}) {}
    function f() {
        ({ *h(){} })
    }
    function f
})()
