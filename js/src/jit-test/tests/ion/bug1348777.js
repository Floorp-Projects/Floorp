
if (typeof TypedObject === 'undefined')
    quit();

var uint8 = TypedObject.uint8;
function check(v) {
    return v.toSource();
}
function test() {
    var fake1 = {};
    var fake2 = [];
    fake2.toSource = uint8;
    var a = [fake1, fake2];
    for (var i = 0; i < 1000; i++) try {
        check(a[i % 2]);
    } catch (e) {}
}
test();
