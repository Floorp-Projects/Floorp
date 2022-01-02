// Binary: cache/js-dbg-64-fadb38356e0f-linux
// Flags:
//
function f() {
    this.e = function() {};
    Object.defineProperty(this, "e", ({
        get: eval
    }));
}
new f();
