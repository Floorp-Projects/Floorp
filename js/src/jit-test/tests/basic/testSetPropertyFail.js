function test(name, fn, val) {
    gc();
    var ok = {}, bad = {};
    bad.__defineSetter__(name, fn);
    var arr = [ok, ok, ok, ok, ok, bad];

    var log = '';
    try {
        for (var i = 0; i < arr.length; i++) {
            arr[i][name] = val;
            log += '.';
        }
    } catch (exc) {
        log += 'E';
    }
    assertEq(log, '.....E');
}

test("x", Function.prototype.call, null);  // TypeError: Function.prototype.call called on incompatible [object Object]
test("y", Array, 0.1);                     // RangeError: invalid array length
test(1, Function.prototype.call, null);  // TypeError: Function.prototype.call called on incompatible [object Object]
test(1, Array, 0.1);                     // RangeError: invalid array length
