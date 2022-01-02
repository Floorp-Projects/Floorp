function b() {}
function c() {}

function foo() {
    for (f of [b,c]) {
	(function () {
	    f.apply({}, arguments);
	})(1,2,3,4)
    }
}

with({}) {}
for (var i = 0; i < 100; i++) {
    foo();
}
