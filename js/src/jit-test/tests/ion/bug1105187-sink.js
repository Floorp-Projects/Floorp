// |jit-test| --ion-gvn=off; error:ReferenceError

(function(x) {
    x = +x
    switch (y) {
        case -1:
            x = 0
    }
})()
