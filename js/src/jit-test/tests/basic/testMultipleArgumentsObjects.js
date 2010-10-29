function testMultipleArgumentsObjects() {
    var testargs = arguments;
    var f = function (name, which) {
        var args = [testargs, arguments];
        return args[which][0];
    };
    var arr = [0, 0, 0, 0, 1];
    for (var i = 0; i < arr.length; i++)
        arr[i] = f("f", arr[i]);
    return arr + '';
}
assertEq(testMultipleArgumentsObjects(), ",,,,f");
