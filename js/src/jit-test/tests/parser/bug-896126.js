// |jit-test| error: SyntaxError
({
    r: function() {
        function f() {
            w[0xe56241c6 >> 3]
        }
    },
    s: function() {
        "use asm"
        return (1 for

