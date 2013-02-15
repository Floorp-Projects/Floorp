// Labeled break tests.
function f1() {
    foo:
    if ([1]) {
	bar:
	for (var i=0; i<100; i++) {
	    if (i > 60)
		break foo;
	}
	assertEq(0, 1);
    }
    assertEq(i, 61);
    return true;
}
assertEq(f1(), true);

// Label with no breaks.
function f2() {
    foo:
    if ([1]) {
	for (var i=0; i<100; i++) {
	}
    }
    assertEq(i, 100);
    return true;
}
assertEq(f2(), true);

// No breaks and early return.
function f3() {
    foo: {
	if (true) {
	    for (var i=0; i<100; i++) {
	    }
	}
	return false;
    }
    assertEq(i, 100);
    return true;
}
assertEq(f3(), false);

// Multiple breaks.
function f4() {
    foo: {
	if (true) {
	    for (var i=0; i<100; i++)
		if (i > 70)
		    break foo;
	        if (i > 80)
		    break foo;
	}
	break foo;
    }
    assertEq(i, 71);
    return true;
}
assertEq(f4(), true);
