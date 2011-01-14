function g() {}

function h() {
    for (var i = 0; i < 9; i++)
	x.f = i;
}

function j() {
    x.f();
}

var x = {f: 0.7, g: g};
x.g();  // interpreter brands x
h();
print(shapeOf(x));
x.f = function (){}; // does not change x's shape
j();
print(shapeOf(x));
h(); // should change x's shape

var thrown = 'none';
try {
    j(); // should throw since x.f === 8
} catch (exc) {
    thrown = exc.name;
}
assertEq(thrown, 'TypeError');