function testLambdaInitedVar() {
    var jQuery = function (a, b) {
        return jQuery && jQuery.length;
    }
    return jQuery();
}

assertEq(testLambdaInitedVar(), 2);
