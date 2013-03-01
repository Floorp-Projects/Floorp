
//
// Simple getter/setter tests.
//
function F1(i) {
    this.x_ = i;
}
Object.defineProperty(F1.prototype, 'z', {
    get: function () {
        return this.x_;
    },
    set: function (x) {
        this.x_ = x;
    }
});
function check(arr) {
    for (var i = 0; i < arr.length; i++) {
        assertEq(arr[i].z, i);
        arr[i].z = 'ZING_' + i;
        assertEq(arr[i].x_, 'ZING_' + i);
    }
}
function TEST1() {
    var ARR1 = [];
    for (var i = 0; i < 30000; i++) {
        ARR1.push(new F1(i));
    }
    check(ARR1);
}
TEST1();

//
// Ensure that getters/setters with extra arguments get passed undefined as appropriate,
// and that the actual arguments.length is correct.
//
function F2(i) {
    this.x_ = i;
}
Object.defineProperty(F2.prototype, 'z', {
    get: function (a, b, c) {
        assertEq(arguments.length, 0);
        assertEq(a === undefined, true);
        assertEq(b === undefined, true);
        assertEq(c === undefined, true);
        return this.x_;
    },
    set: function (x, y, z) {
        assertEq(arguments.length, 1);
        assertEq(y === undefined, true);
        assertEq(z === undefined, true);
        this.x_ = x;
    }
});

function F3(i) {
    this.x_ = i;
}
Object.defineProperty(F3.prototype, 'z', {
    get: function (a, b, c) {
        assertEq(arguments.length, 0);
        assertEq(a === undefined, true);
        assertEq(b === undefined, true);
        assertEq(c === undefined, true);
        return this.x_;
    },
    set: function () {
        assertEq(arguments.length, 1);
        this.x_ = arguments[0];
    }
});

function check(arr) {
    for (var i = 0; i < arr.length; i++) {
        assertEq(arr[i].z, i);
        arr[i].z = 'ZING_' + i;
        assertEq(arr[i].x_, 'ZING_' + i);
    }
}
function TEST2() {
    var ARR1 = [];
    for (var i = 0; i < 30000; i++) {
        if (i % 2 == 0)
            ARR1.push(new F2(i));
        else
            ARR1.push(new F3(i));
    }
    check(ARR1);
}
TEST2();
