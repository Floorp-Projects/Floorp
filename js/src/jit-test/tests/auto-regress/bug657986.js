// Binary: cache/js-dbg-32-5d1cbc94bc42-linux
// Flags: -m -n -a
//
function f(x) {
    this.i = x;
}
for each(let e in [0, 0, []]) {
    try {
        f(e)
    } catch (e) {}
}
print(uneval(this))
