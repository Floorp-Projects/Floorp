// |jit-test| --ion-eager
load(libdir + "asserts.js");

[1, "", true, Symbol(), undefined].forEach(props => {
    assertEq(Object.getPrototypeOf(Object.create(null, props)), null);
});

assertThrowsInstanceOf(() => Object.create(null, null), TypeError);
