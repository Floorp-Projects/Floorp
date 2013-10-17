// |jit-test| error: TypeError

function $ERROR() {}
function testMultipleArgumentsObjects() {
    var testargs = arguments;
    var f = function (which) {
        var args = [ testargs ];
        return args[which][0];
    };
    var arr = [0, 0, 0, 0, 1];
    for (var i = 0; i < arr.length; i++)
        $ERROR[i] = f(arr[i]);
}
testMultipleArgumentsObjects()
