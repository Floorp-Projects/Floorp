// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-b7d269a291b6-linux
// Flags: -m -n -a
//

function Integer(value, exception) {
    try {} catch (e) {}
    new(value = this)(this.value);
    Math.floor(({})[value.Math, this].abstract)
}
new Integer(3, false);
