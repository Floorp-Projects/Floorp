var res = undefined;
function X() {
    try {
        foobar();
    } catch (e) {
        res = this.hasOwnProperty("prop");
    }
    this.prop = 1;
}
for (var i = 0; i < 50; i++) {
    new X();
    assertEq(res, false);
}
