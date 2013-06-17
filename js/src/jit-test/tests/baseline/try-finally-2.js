var count = 0;
function f() {
    try {
	try {
	    try {
		count += 2;
	    } finally {
		count += 3;
		throw 3;
	    }
	} catch(e) {
	    count += 4;
	    throw 4;
	}
    } finally {
	count += 5;
	try {
	    count += 6;
	} catch(e) {
	    count += 7;
	    throw 123;
	} finally {
	    count += 8;
	}
	count += 9;
    }
    count += 10;
}
for (var i=0; i<3; i++) {
    try {
	f();
	assertEq(0, 1);
    } catch(e) {
	assertEq(e, 4);
    }
}
assertEq(count, 111);
