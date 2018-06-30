// ToNumber(value) is executed for OOB writes when using a direct assignment.
function plainSet() {
    var callCount = 0;
    var value = {
        valueOf() {
            callCount++;
            return 1;
        }
    };

    var N = 100;
    var ta = new Int32Array(0);
    for (var i = 0; i < N; ++i)
        ta[0] = value

    assertEq(callCount, N);
}
for (var i = 0; i < 2; ++i) plainSet();

// ToNumber(value) is executed for OOB writes when using Reflect.set(...).
function reflectSet() {
    var callCount = 0;
    var value = {
        valueOf() {
            callCount++;
            return 1;
        }
    };

    var N = 100;
    var ta = new Int32Array(0);
    for (var i = 0; i < N; ++i)
        assertEq(Reflect.set(ta, 0, value), false);

    assertEq(callCount, N);
}
for (var i = 0; i < 2; ++i) reflectSet();

// ToNumber(value) is not executed for OOB writes when using Reflect.defineProperty(...).
function defineProp() {
    var callCount = 0;
    var value = {
        valueOf() {
            callCount++;
            return 1;
        }
    };
    var desc = {value, writable: true, enumerable: true, configurable: false};

    var N = 100;
    var ta = new Int32Array(0);
    for (var i = 0; i < N; ++i)
        assertEq(Reflect.defineProperty(ta, 0, desc), false);

    assertEq(callCount, 0);
}
for (var i = 0; i < 2; ++i) defineProp();

if (typeof reportCompare === "function")
    reportCompare(true, true);
