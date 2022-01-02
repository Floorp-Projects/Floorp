function x() {
    [1];
}
Array.prototype.__proto__ = {};
x();
Array.prototype.__proto__ = null;
x();
