function testLambdaCtor() {
    var a = [];
    for (var x = 0; x < RUNLOOP; ++x) {
        var f = function(){};
        a[a.length] = new f;
    }

    // This prints false until the upvar2 bug is fixed:
    // print(a[HOTLOOP].__proto__ !== a[HOTLOOP-1].__proto__);

    // Assert that the last f was properly constructed.
    return a[RUNLOOP-1].__proto__ === f.prototype;
}
assertEq(testLambdaCtor(), true);
