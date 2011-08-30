test();
function iso(d) { new Date(d).toISOString() }
function check(s,millis) { iso(millis); }
function dd(year, month, day, hour, minute, second, millis) {
    return Date.UTC(year, 1, day, hour, minute, second, millis);
}
function test() {
    try {
        check("", 20092353211)
        check("", 2009)
        check("", 0)
        check("", dd(BUGNUMBER, 7, 23, 19, 53, 21, 1))
    } catch (e) {}
}
var BUGNUMBER = "10278";
test()
