function C() {
    var k = 3;
    this.x = function () { return k; };
    for (var i = 0; i < 9; i++)
        ;
}
new C;
