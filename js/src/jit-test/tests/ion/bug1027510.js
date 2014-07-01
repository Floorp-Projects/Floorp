setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

function myCeil(x) {
    if(x >= 0) {
        var r = Math.abs(x % 1);
        if(r != 0)
            return (x + 1) - r;
        else
            return x;
    }
    else
        return x + Math.abs(x % 1);
}

function ceilRangeTest(x) {
    if(10 < x) {
        if(x < 100) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if(-100 < x) {
        if(x < -10) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if (-(4294967296 - 1) < x) {
        if(x < 10) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if (-10 < x) {
        if(x < 4294967296) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if (-2147483648 < x) {
        if(x < 10) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if ((-2147483648 -1) < x) {
        if(x < 10) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if (10 < x) {
        if(x < 2147483648) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if (10 < x) {
        if(x < 2147483649) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }

    if (Math.pow(2,31) < x) {
        if(x < Math.pow(2,33)) {
            assertEq(Math.ceil(x), myCeil(x));
        }
    }
}

var a = [Math.pow(2,31), Math.pow(2,33), -4294967296.4, 214748364.2, -50.4, 50.4];

for(var i = 0; i < 10; i++) {
    for (var j = 0; j < a.length; j++) {
        ceilRangeTest(a[j]);
    }
}

for (var j = 0; j < 30; j++) {
    (function() {
        Math.ceil(1.5);
    })()
}

for (var j = 0; j < 30; j++) {
    (function() {
        Math.ceil(-1.5);
    })()
}

for (var j = 0; j < 30; j++) {
    (function() {
        Math.ceil(-127.5);
    })()
}
