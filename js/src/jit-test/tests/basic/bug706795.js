executed = false;
Object.defineProperty(Object.prototype, 'x', { set: function() { executed = true; } });
function A() {
    this.x = 12;
}
new A();
assertEq(executed, true);
