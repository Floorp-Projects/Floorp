function checkNameLookup() {
    return "global";
}

function assertWithMessage(got, expected, message) {
    assertEq(message + ": " + got, message + ": " + expected);
}

// Create our test func via "evaluate" so it won't be compileAndGo and
// we can clone it.
evaluate(`function testFunc() {
    assertWithMessage(checkNameLookup(), "local", "nameLookup");
    assertWithMessage(checkThisBinding(), "local", "thisBinding");
}`, { compileAndGo: false });

var obj = {
    checkNameLookup: function() {
	return "local";
    },

    checkThisBinding: function() {
	return this.checkNameLookup();
    },
};

var cloneFunc = clone(testFunc, obj);
cloneFunc();
