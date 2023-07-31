function test() {
    var RegExpHasCaptureGroups = getSelfHostedValue("RegExpHasCaptureGroups");
    var cases = [
        [/a.+/, false],
        [/abc/, false],
        [/\r\n?|\n/, false],
        [/(abc)/, true],
        [/a(.+)/, true],
        [/a(b)(c)(d)/, true],
        [/a(?:b)/, false],
        [/((?:a))/, true],
        [/(?<name>a)/, true],
    ];
    for (var i = 0; i < 10; i++) {
        for (var [re, expected] of cases) {
            assertEq(RegExpHasCaptureGroups(re, "abcdef"), expected);
        }
    }
}
test();
