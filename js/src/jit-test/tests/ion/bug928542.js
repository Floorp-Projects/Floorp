
called = 0;
function foo() {
    this.what();
    this.random = 0;
    this.what = 1;
}
foo.prototype.what = function() { called = 1; }
new foo();
assertEq(called, 1);
