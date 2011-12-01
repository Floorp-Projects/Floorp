function test() {
  function Bug() { this.prototype }
  var actual = (new Bug instanceof Bug);
  assertEq(actual, true);
}
test();
test();

function testLambdaCtor() {
    var q;
    for (var x = 0; x < 2; ++x) {
        var f = function(){};
	if (x == 1) gc();
        q = new f;
    }
    return q.__proto__ === f.prototype;
}
assertEq(testLambdaCtor(), true);
