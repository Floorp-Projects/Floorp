// |jit-test| error:InternalError

Object.defineProperty(this, "t2", {
    get: function() {
        for (p in h2) {
            t2
        }
    }
})
h2 = {}
mjitChunkLimit(8)
h2.a = function() {}
Object(t2)
