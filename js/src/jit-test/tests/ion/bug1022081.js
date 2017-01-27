function f() {
    (function() {
        gc()
    })()
}
enableGeckoProfiling()
f()
f()
