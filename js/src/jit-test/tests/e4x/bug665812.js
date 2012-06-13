// |jit-test| error: TypeError

(function() {
    function::d = 0
    d.(l)
    function d() {}
})()
