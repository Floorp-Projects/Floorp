if (!this.parseInt) {
    var parseInt = function () { return 5; }
}
assertEq(parseInt(10), 10);
