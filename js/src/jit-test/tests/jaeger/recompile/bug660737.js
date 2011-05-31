(function() {
    function f(l) {
        w++
    }
    for each(let w in ['', '', 0]) {
        try {
            f(w)
        } catch (e) {}
    }
})()
