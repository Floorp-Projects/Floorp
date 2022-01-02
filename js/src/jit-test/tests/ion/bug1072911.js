
function X () {};
function Y () {};
function testCallProtoMethod() {
    var a = [new X, new X, __proto__, new Y, new Y];
}
testCallProtoMethod();

function testNot() {
    var r;
    for (var i = 0; i < 10; ++i)
        r = [];
}
testNot();
