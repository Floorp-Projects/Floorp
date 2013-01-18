// Binary: cache/js-dbg-32-74a8fb1bbec5-linux
// Flags: -m -n -a
//
o0 = (3).__proto__
function f(o) {
    var prop
    [o][0][prop] = []
    try {
        ({
            x: function() {
                return o
            }
        }.x()())
    } catch (e) {}
}
f(o0)
