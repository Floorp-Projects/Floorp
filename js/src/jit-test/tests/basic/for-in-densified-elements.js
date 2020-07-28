function test() {
    // An array with sparse elements...
    var arr = [];
    arr[10_000] = 1;
    arr[10_001] = 1;

    for (var prop in arr) {
        assertEq(prop, "10000");
        assertEq(arr.length, 10_002);

        // Densify the elements.
        for (var i = 0; i < arr.length; i++) {
            arr[i] = 1;
        }

        // Delete the last dense element (10001). It should not be visited by the
        // active for-in (checked above).
        arr.length = 10_001;
    }
}
test();
