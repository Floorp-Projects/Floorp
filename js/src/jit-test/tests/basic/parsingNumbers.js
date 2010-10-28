function parsingNumbers() {
    var s1 = "123";
    var s1z = "123zzz";
    var s2 = "123.456";
    var s2z = "123.456zzz";

    var e1 = 123;
    var e2 = 123.456;

    var r1, r1z, r2, r2z;

    for (var i = 0; i < 10; i++) {
        r1 = parseInt(s1);
        r1z = parseInt(s1z);
        r2 = parseFloat(s2);
        r2z = parseFloat(s2z);
    }

    if (r1 == e1 && r1z == e1 && r2 == e2 && r2z == e2)
        return "ok";
    return "fail";
}
assertEq(parsingNumbers(), "ok");
