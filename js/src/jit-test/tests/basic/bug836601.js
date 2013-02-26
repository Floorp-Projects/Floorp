// |jit-test| error: InternalError
let k
Proxy.createFunction(function() {
    return {
        get: Uint32Array
    }
}(), decodeURIComponent) & k

