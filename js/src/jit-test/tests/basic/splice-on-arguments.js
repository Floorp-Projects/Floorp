// test whether splice works on arguments

function splice_args () {
    args = arguments;
    return Array.prototype.splice.apply(args, [0, 5]);
}

var args;
var O = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
var A = splice_args.apply(undefined, O)

// args: [5, 6, 7, 8, 9]
assertEq(args[0], 5);
assertEq(args[1], 6);
assertEq(args[2], 7);
assertEq(args[3], 8);
assertEq(args[4], 9);
assertEq(args.length, 5);

// A: [0, 1, 2, 3, 4]
assertEq(A[0], 0);
assertEq(A[1], 1);
assertEq(A[2], 2);
assertEq(A[3], 3);
assertEq(A[4], 4);
assertEq(A.length, 5);

// O: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
assertEq(O[0], 0);
assertEq(O[1], 1);
assertEq(O[2], 2);
assertEq(O[3], 3);
assertEq(O[4], 4);
assertEq(O[5], 5);
assertEq(O[6], 6);
assertEq(O[7], 7);
assertEq(O[8], 8);
assertEq(O[9], 9);
assertEq(O.length, 10);
