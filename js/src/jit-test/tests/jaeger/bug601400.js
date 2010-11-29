// |jit-test| error: TypeError

eval("\
    function NaN() {}\
    for(w in s) {}\
")

// Don't assert.
