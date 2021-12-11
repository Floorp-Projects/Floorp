var a = evalcx('');
Object.defineProperty(a, "", ({
    get: function() {},
}))
gc()

// don't crash
