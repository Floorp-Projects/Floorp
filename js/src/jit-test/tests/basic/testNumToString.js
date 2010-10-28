function testNumToString() {
    var r = [];
    var d = 123456789;
    for (var i = 0; i < 10; ++i) {
        r = [
             d.toString(),
             (-d).toString(),
             d.toString(10),
             (-d).toString(10),
             d.toString(16),
             (-d).toString(16),
             d.toString(36),
             (-d).toString(36)
            ];
    }
    return r.join(",");
}
assertEq(testNumToString(), "123456789,-123456789,123456789,-123456789,75bcd15,-75bcd15,21i3v9,-21i3v9");
