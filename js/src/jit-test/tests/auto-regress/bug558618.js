// Binary: cache/js-dbg-64-e69034463eeb-linux
// Flags: -j
//
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
function f() {
    this.search = function(a, b, c) {
        arguments[3] = { }
        arguments.length = 4;
        for (var i = 0; i < 100; i++) {
            print(arguments[3]);
        }
    }
}
var o = new f();
o.search({x: -1, y: -1, w: 100600, h: 100600});
