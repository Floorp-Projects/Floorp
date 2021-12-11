function testBug504520Harder() {
    // test 1024 similar cases
    var vals = [1/0, -1/0, 0, 0/0];
    var ops = ["===", "!==", "==", "!=", "<", ">", "<=", ">="];
    for (var x of vals) {
        for (var y of vals) {
            for (var op of ops) {
                for (var z of vals) {
                    // Assume eval is correct. This depends on the global
                    // Infinity property not having been reassigned.
                    var xz = eval(x + op + z);
                    var yz = eval(y + op + z);

                    var arr = [x, x, x, x, x, x, x, x, x, y];
                    assertEq(arr.length > 9, true);
                    var expected = [xz, xz, xz, xz, xz, xz, xz, xz, xz, yz];

                    // ?: looks superfluous but that's what we're testing here
                    var fun = eval(
                        '(function (arr, results) {\n' +
                        '    for (let i = 0; i < arr.length; i++)\n' +
                        '        results.push(arr[i]' + op + z + ' ? "true" : "false");\n' +
                        '});\n');
                    var actual = [];
                    fun(arr, actual);
		    print(x, y, op, z);
                    assertEq("" + actual, "" + expected);
                }
            }
        }
    }
}
testBug504520Harder();
