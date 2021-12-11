function customconstructor() {
    class X {
        constructor() {}
        a() {}
    };

    assertEq(Object.getOwnPropertyDescriptor(X, "prototype").configurable, false);
    assertEq(Object.getOwnPropertyDescriptor(X.prototype, "constructor").enumerable, false);
}

function defaultconstructor() {
    class X {
        a() {}
    };

    assertEq(Object.getOwnPropertyDescriptor(X, "prototype").configurable, false);
    assertEq(Object.getOwnPropertyDescriptor(X.prototype, "constructor").enumerable, false);
}

function run() {
    for (var i = 0; i < 100; i++) {
        customconstructor();
        defaultconstructor();
    }
}

run();
