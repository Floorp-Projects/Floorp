// |jit-test| debug; error: TypeError
function f() {
    ""(this.z)
}
trap(f, 0, '')
f()
