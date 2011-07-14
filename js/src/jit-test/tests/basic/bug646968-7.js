// |jit-test| debug

function test(s) {
    eval(s);
    let (y = evalInFrame(0, '3'), x = x) {
	assertEq(x, 5);
    }
}
test('var x = 5;');
