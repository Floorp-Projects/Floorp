// |jit-test| error: TypeError
try {
    i
}
catch (x if (function() {})()) {}
catch (d) {
    this.z.z
}
