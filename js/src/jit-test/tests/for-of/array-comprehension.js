// for-of can be used in array comprehensions.

assertEq([x*x for (x of [1, 2, 3])].join(), "1,4,9");
