var A = {x: 3};
var B = Object.create(A);
var C = Object.create(B);
var D = Object.create(C);

function f() {
    for (var i=0; i<30; i++) {
	assertEq(D.x, (i <= 20) ? 3 : 10);
	if (i === 20) {
	    C.__proto__ = {x: 10};
	}
    }
}
f();
