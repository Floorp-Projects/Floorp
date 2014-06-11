function test(s) {
    var lat1 = toLatin1(s);

    var twoByte = "\u1200" + lat1;
    twoByte.indexOf("X"); // Flatten.
    twoByte = twoByte.substr(1);

    assertEq(isLatin1(lat1), true);
    assertEq(isLatin1(twoByte), false);

    assertEq(Date.parse(lat1), Date.parse(twoByte));
}

// ISO format
test("2014-06-06");
test("2014-06-06T08:30+01:00");
test("T11:59Z");

// Non-ISO format
test("06 Jun 2014, 17:20:36");
test("6 Jun 2014");
test("Wed Nov 05 21:49:11 GMT-0800 1997");
test("Jan 30 2014 2:30 PM");

// Invalid
test("06 Aaa 2014, 17:20:36");
test("6 Jun 10");
test("2014-13-06");
test("2014-06-06T08:30+99:00");
