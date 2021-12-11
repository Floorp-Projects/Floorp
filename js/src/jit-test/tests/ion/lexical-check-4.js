var caught = false;
try {
    (function() {
        let x = (function f(y) {
            if (y == 0) {
                x
                return
            }
            return f(y - 1)
        })(3)
    })()
} catch(e) {
    caught = true;
}
assertEq(caught, true);
