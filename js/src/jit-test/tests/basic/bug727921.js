(function() {
    let(d) {
        yield
    }
})()
eval("\
    (function(){\
        schedulegc(5), 'a'.replace(/a/,function(){yield})\
    })\
")()
