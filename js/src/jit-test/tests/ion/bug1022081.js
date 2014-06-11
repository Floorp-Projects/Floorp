function f() {
    (function() {
        gc()
    })()
}
enableSPSProfiling()
f()
f()
