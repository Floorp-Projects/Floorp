// |jit-test| dump-bytecode

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
