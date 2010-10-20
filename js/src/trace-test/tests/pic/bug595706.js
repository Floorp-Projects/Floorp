function f(useArg2, arg2, expect) {
    var args = arguments;
    if (useArg2)
	args = arg2;

    print(args)
    assertEq(args.length, expect);
}

// Generate a PIC for arguments.
f(false, 0, 3);
f(false, 0, 3);
f(false, 0, 3);

// Now call it with a slow array.
var a = [1, 2, 3];
a.x = 9;

f(true, a, 3);
