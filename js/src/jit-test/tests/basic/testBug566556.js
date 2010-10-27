var msg = "";
try {
    this.__defineSetter__('x', Object.create);
    this.watch('x', function() {});
    x = 3;
} catch (e) {
    msg = e.toString();
}
assertEq(msg, "TypeError: (void 0) is not an object or null");
