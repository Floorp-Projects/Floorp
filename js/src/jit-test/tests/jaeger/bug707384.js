// |jit-test| debug
function f() {
    do {
        return
    } while (e)
}
trap(f, 1, '')
f()
