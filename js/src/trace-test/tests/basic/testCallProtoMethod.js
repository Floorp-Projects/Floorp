function testCallProtoMethod() {
    function X() { this.x = 1; }
    X.prototype.getName = function () { return "X"; }

    function Y() { this.x = 2; }
    Y.prototype.getName = function() "Y";

    var a = [new X, new X, new X, new X, new Y];
    var s = '';
    for (var i = 0; i < a.length; i++)
        s += a[i].getName();
    return s;
}
assertEq(testCallProtoMethod(), 'XXXXY');
