function testNullCallee() {
    try {
        function f() {
            var x = new Array(5);
            for (var i = 0; i < 5; i++)
                x[i] = a[i].toString();
            return x.join(',');
        }
        f([[1],[2],[3],[4],[5]]);
        f([null, null, null, null, null]);
    } catch (e) {
        return true;
    }
    return false;
}
assertEq(testNullCallee(), true);
