// Repeat 1448582-{1,3,4}.js for classes.

(function(index) {
    // Does not assert.
    var c = class { constructor(){} };

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);

(function(index) {
    var c = class { constructor(){} };

    // Accessing |.name| sets the resolved-name flag, which should not be
    // copied over to the function clone.
    assertEq(c.name, "c");

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);

(function(index) {
    var c = class { constructor(a){} };

    // Accessing |.length| sets the resolved-length flag, which should not be
    // copied over to the function clone.
    assertEq(c.length, 1);

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);


// Repeat 1448582-{3,4}.js for generator functions.

(function(index) {
    function* f() {}

    // Accessing |.name| sets the resolved-name flag, which should not be
    // copied over to the function clone.
    assertEq(f.name, "f");

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);

(function(index) {
    function* f(a) {}

    // Accessing |.length| sets the resolved-length flag, which should not be
    // copied over to the function clone.
    assertEq(f.length, 1);

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);


// Repeat 1448582-{3,4}.js for async functions.

(function(index) {
    async function f() {}

    // Accessing |.name| sets the resolved-name flag, which should not be
    // copied over to the function clone.
    assertEq(f.name, "f");

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);

(function(index) {
    async function f(a) {}

    // Accessing |.length| sets the resolved-length flag, which should not be
    // copied over to the function clone.
    assertEq(f.length, 1);

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);


// Repeat 1448582-{3,4}.js for async generator functions.

(function(index) {
    async function* f() {}

    // Accessing |.name| sets the resolved-name flag, which should not be
    // copied over to the function clone.
    assertEq(f.name, "f");

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);

(function(index) {
    async function* f(a) {}

    // Accessing |.length| sets the resolved-length flag, which should not be
    // copied over to the function clone.
    assertEq(f.length, 1);

    if (index === 0) {
        (function self() {
            self.caller(1);
        })();
    }
})(0);
