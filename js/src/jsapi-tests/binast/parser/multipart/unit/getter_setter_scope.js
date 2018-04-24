function foo(e) {
    var obj = {
        get x() {
            var e; // This `e` and the function argument `e` should have a distinct scope.
        },
        set x(y) {
            var e; // This `e` and the function argument `e` should have a distinct scope.
        }
    };
}