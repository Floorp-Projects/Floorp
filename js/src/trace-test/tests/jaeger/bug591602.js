try {
    for (x in ['']) {
        gczeal(2)
    }
} catch(e) {}
try {
    var x, e
    p()
} catch(e) {}
try { (function() {
        let(x)((function() {
            let(y)((function() {
                try {
                    let(c) o
                } finally { (f, *::*)
                }
            }))
        }))
    })()
} catch(e) {}
try { (function() {
        if (x.w("", (function() {
            t
        })())) {}
    })()
} catch(e) {}
try {
    gczeal()
} catch(e) {}
try { (function() {
        for (let w in [0, 0]) let(b)((function() {
            let(x = w = [])((function() {
                for (let a in []);
            }))
        })())
    })()
} catch(e) {}

/* Don't assert with -m only. */

