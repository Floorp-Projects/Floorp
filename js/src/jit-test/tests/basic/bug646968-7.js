load(libdir + "evalInFrame.js");

function test(s) {
    eval(s);
    {
      let y = evalInFrame(0, '3'), x = x0;
	    assertEq(x, 5);
    }
}
test('var x0= 5;');
