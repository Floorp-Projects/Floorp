// |jit-test| dump-bytecode;error:SyntaxError

(function() {
    const x = ((function() {
        return {
            e: function() {
                (function() {
                    for (e in x) {}
                })()
            }
        }
    }(function() {
        return {
            t: {
                c
            }
        }
    })))
})()
