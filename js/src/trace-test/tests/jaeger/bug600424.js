// vim: set ts=4 sw=4 tw=99 et:
function f(a) {
    var x = {
        g: function () {
            return this.a;
        }
    };
    x.g.prototype.a = a;
    assertEq(x.g.prototype.a, a);
    return x;
}
f(1);
f(2);
f(3);
