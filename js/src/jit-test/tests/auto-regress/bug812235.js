// Binary: cache/js-dbg-64-a37525d304d9-linux
// Flags: --ion-eager
//
gc()
schedulegc(this)
gcslice(3)
function f() {
    this["x"] = this["x"] = {}
}
new f()
