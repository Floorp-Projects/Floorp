var typedSwitch0 = function (a) {
    switch (a) {
        case null:
        return 0;
        case 1.1:
        return 1;
        case "2":
        return 2;
        case 3:
        return 3;
    }
    return 4;
};

// reuse the same function for testing with different inputs & type.
var typedSwitch1 = eval(typedSwitch0.toSource());
var typedSwitch2 = eval(typedSwitch0.toSource());
var typedSwitch3 = eval(typedSwitch0.toSource());
var typedSwitch4 = eval(typedSwitch0.toSource());

for (var i = 0; i < 100; i++) {
    assertEq(typedSwitch0(null), 0);
    assertEq(typedSwitch1(1.1), 1);
    assertEq(typedSwitch2("2"), 2);
    assertEq(typedSwitch3(3), 3);
    assertEq(typedSwitch4(undefined), 4);
}
