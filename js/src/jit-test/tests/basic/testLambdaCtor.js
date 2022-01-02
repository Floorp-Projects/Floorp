function testLambdaCtor() {
    var a = [];
    for (var x = 0; x < 9; ++x) {
        var f = function(){};
        a[a.length] = new f;
    }

    assertEq([8].__proto__ !== a[7].__proto__, true);

    // Assert that the last f was properly constructed.
    return a[8].__proto__ === f.prototype;
}
assertEq(testLambdaCtor(), true);
